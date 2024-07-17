// Copyright 2008-2020 David Robillard <d@drobilla.net>
// Copyright 2008 Nedko Arnaudov <nedko@arnaudov.name>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "AudioDriver.hpp"
#include "ClientID.hpp"
#include "ClientType.hpp"
#include "Driver.hpp"
#include "Event.hpp"
#include "ILog.hpp"
#include "PortID.hpp"
#include "PortInfo.hpp"
#include "PortNames.hpp"
#include "PortType.hpp"
#include "SignalDirection.hpp"
#include "make_jack_driver.hpp"
#include "warnings.hpp"

PATCHAGE_DISABLE_FMT_WARNINGS
#include <fmt/core.h>
PATCHAGE_RESTORE_WARNINGS

#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus-protocol.h>
#include <dbus/dbus-shared.h>
#include <dbus/dbus.h>

#include <cassert>
#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>

#define JACKDBUS_SERVICE "org.jackaudio.service"
#define JACKDBUS_OBJECT "/org/jackaudio/Controller"
#define JACKDBUS_IFACE_CONTROL "org.jackaudio.JackControl"
#define JACKDBUS_IFACE_PATCHBAY "org.jackaudio.JackPatchbay"

#define JACKDBUS_CALL_DEFAULT_TIMEOUT 1000 // in milliseconds

#define JACKDBUS_PORT_FLAG_INPUT 0x00000001
#define JACKDBUS_PORT_FLAG_TERMINAL 0x00000010

#define JACKDBUS_PORT_TYPE_AUDIO 0
#define JACKDBUS_PORT_TYPE_MIDI 1

namespace patchage {
namespace {

/// Driver for JACK audio and midi ports that uses D-Bus
class JackDriver : public AudioDriver
{
public:
  explicit JackDriver(ILog& log, EventSink emit_event);

  JackDriver(const JackDriver&)            = delete;
  JackDriver& operator=(const JackDriver&) = delete;

  JackDriver(JackDriver&&)            = delete;
  JackDriver& operator=(JackDriver&&) = delete;

  ~JackDriver() override;

  // Driver interface
  void attach(bool launch_daemon) override;
  void detach() override;
  bool is_attached() const override;
  void refresh(const EventSink& sink) override;
  bool connect(const PortID& tail_id, const PortID& head_id) override;
  bool disconnect(const PortID& tail_id, const PortID& head_id) override;

  // AudioDriver interface
  uint32_t xruns() override;
  void     reset_xruns() override;
  uint32_t buffer_size() override;
  bool     set_buffer_size(uint32_t frames) override;
  uint32_t sample_rate() override;

private:
  PortType patchage_port_type(dbus_uint32_t dbus_port_type) const;

  PortInfo port_info(const std::string& port_name,
                     dbus_uint32_t      port_type,
                     dbus_uint32_t      port_flags) const;

  void error_msg(const std::string& msg) const;
  void info_msg(const std::string& msg) const;

  bool call(bool          response_expected,
            const char*   iface,
            const char*   method,
            DBusMessage** reply_ptr_ptr,
            int           in_type,
            ...);

  void update_attached();

  bool is_started();

  void start_server();

  void stop_server();

  static DBusHandlerResult dbus_message_hook(DBusConnection* connection,
                                             DBusMessage*    message,
                                             void*           jack_driver);

  void on_jack_appeared();

  void on_jack_disappeared();

  ILog&           _log;
  DBusError       _dbus_error;
  DBusConnection* _dbus_connection;

  mutable bool _server_responding;
  bool         _server_started;

  dbus_uint64_t _graph_version;
};

JackDriver::JackDriver(ILog& log, EventSink emit_event)
  : AudioDriver{std::move(emit_event)}
  , _log(log)
  , _dbus_error()
  , _dbus_connection(nullptr)
  , _server_responding(false)
  , _server_started(false)
  , _graph_version(0)
{
  dbus_error_init(&_dbus_error);
}

JackDriver::~JackDriver()
{
  if (_dbus_connection) {
    dbus_connection_flush(_dbus_connection);
  }

  if (dbus_error_is_set(&_dbus_error)) {
    dbus_error_free(&_dbus_error);
  }
}

void
JackDriver::update_attached()
{
  const bool was_attached = _server_started;
  _server_started         = is_started();

  if (!_server_responding) {
    if (was_attached) {
      _emit_event(event::DriverDetached{ClientType::jack});
    }
    return;
  }

  if (_server_started && !was_attached) {
    _emit_event(event::DriverAttached{ClientType::jack});
    return;
  }

  if (!_server_started && was_attached) {
    _emit_event(event::DriverDetached{ClientType::jack});
    return;
  }
}

void
JackDriver::on_jack_appeared()
{
  info_msg("Server appeared");
  update_attached();
}

void
JackDriver::on_jack_disappeared()
{
  info_msg("Server disappeared");

  // we are not calling update_attached() here, because it will activate
  // jackdbus

  _server_responding = false;

  if (_server_started) {
    _emit_event(event::DriverDetached{ClientType::jack});
  }

  _server_started = false;
}

DBusHandlerResult
JackDriver::dbus_message_hook(DBusConnection* /*connection*/,
                              DBusMessage* message,
                              void*        jack_driver)
{
  const char*   client2_name      = nullptr;
  const char*   client_name       = nullptr;
  const char*   new_owner         = nullptr;
  const char*   object_name       = nullptr;
  const char*   old_owner         = nullptr;
  const char*   port2_name        = nullptr;
  const char*   port_name         = nullptr;
  dbus_uint32_t port_flags        = 0u;
  dbus_uint32_t port_type         = 0u;
  dbus_uint64_t client2_id        = 0u;
  dbus_uint64_t client_id         = 0u;
  dbus_uint64_t connection_id     = 0u;
  dbus_uint64_t new_graph_version = 0u;
  dbus_uint64_t port2_id          = 0u;
  dbus_uint64_t port_id           = 0u;

  assert(jack_driver);
  auto* me = static_cast<JackDriver*>(jack_driver);
  assert(me->_dbus_connection);

  if (dbus_message_is_signal(
        message, DBUS_INTERFACE_DBUS, "NameOwnerChanged")) {
    if (!dbus_message_get_args(message,
                               &me->_dbus_error,
                               DBUS_TYPE_STRING,
                               &object_name,
                               DBUS_TYPE_STRING,
                               &old_owner,
                               DBUS_TYPE_STRING,
                               &new_owner,
                               DBUS_TYPE_INVALID)) {
      me->error_msg(fmt::format("dbus_message_get_args() failed to extract "
                                "NameOwnerChanged signal arguments ({})",
                                me->_dbus_error.message));

      dbus_error_free(&me->_dbus_error);
      return DBUS_HANDLER_RESULT_HANDLED;
    }

    if (old_owner[0] == '\0') {
      me->on_jack_appeared();
    } else if (new_owner[0] == '\0') {
      me->on_jack_disappeared();
    }
  }

  if (dbus_message_is_signal(
        message, JACKDBUS_IFACE_PATCHBAY, "PortAppeared")) {
    if (!dbus_message_get_args(message,
                               &me->_dbus_error,
                               DBUS_TYPE_UINT64,
                               &new_graph_version,
                               DBUS_TYPE_UINT64,
                               &client_id,
                               DBUS_TYPE_STRING,
                               &client_name,
                               DBUS_TYPE_UINT64,
                               &port_id,
                               DBUS_TYPE_STRING,
                               &port_name,
                               DBUS_TYPE_UINT32,
                               &port_flags,
                               DBUS_TYPE_UINT32,
                               &port_type,
                               DBUS_TYPE_INVALID)) {
      me->error_msg(fmt::format("dbus_message_get_args() failed to extract "
                                "PortAppeared signal arguments ({})",
                                me->_dbus_error.message));
      dbus_error_free(&me->_dbus_error);
      return DBUS_HANDLER_RESULT_HANDLED;
    }

    if (!me->_server_started) {
      me->_server_started = true;
      me->_emit_event(event::DriverAttached{ClientType::jack});
    }

    me->_emit_event(
      event::PortCreated{PortID::jack(client_name, port_name),
                         me->port_info(port_name, port_type, port_flags)});

    return DBUS_HANDLER_RESULT_HANDLED;
  }

  if (dbus_message_is_signal(
        message, JACKDBUS_IFACE_PATCHBAY, "PortDisappeared")) {
    if (!dbus_message_get_args(message,
                               &me->_dbus_error,
                               DBUS_TYPE_UINT64,
                               &new_graph_version,
                               DBUS_TYPE_UINT64,
                               &client_id,
                               DBUS_TYPE_STRING,
                               &client_name,
                               DBUS_TYPE_UINT64,
                               &port_id,
                               DBUS_TYPE_STRING,
                               &port_name,
                               DBUS_TYPE_INVALID)) {
      me->error_msg(fmt::format("dbus_message_get_args() failed to extract "
                                "PortDisappeared signal arguments ({})",
                                me->_dbus_error.message));
      dbus_error_free(&me->_dbus_error);
      return DBUS_HANDLER_RESULT_HANDLED;
    }

    if (!me->_server_started) {
      me->_server_started = true;
      me->_emit_event(event::DriverAttached{ClientType::jack});
    }

    me->_emit_event(event::PortDestroyed{PortID::jack(client_name, port_name)});

    return DBUS_HANDLER_RESULT_HANDLED;
  }

  if (dbus_message_is_signal(
        message, JACKDBUS_IFACE_PATCHBAY, "PortsConnected")) {
    if (!dbus_message_get_args(message,
                               &me->_dbus_error,
                               DBUS_TYPE_UINT64,
                               &new_graph_version,
                               DBUS_TYPE_UINT64,
                               &client_id,
                               DBUS_TYPE_STRING,
                               &client_name,
                               DBUS_TYPE_UINT64,
                               &port_id,
                               DBUS_TYPE_STRING,
                               &port_name,
                               DBUS_TYPE_UINT64,
                               &client2_id,
                               DBUS_TYPE_STRING,
                               &client2_name,
                               DBUS_TYPE_UINT64,
                               &port2_id,
                               DBUS_TYPE_STRING,
                               &port2_name,
                               DBUS_TYPE_UINT64,
                               &connection_id,
                               DBUS_TYPE_INVALID)) {
      me->error_msg(fmt::format("dbus_message_get_args() failed to extract "
                                "PortsConnected signal arguments ({})",
                                me->_dbus_error.message));
      dbus_error_free(&me->_dbus_error);
      return DBUS_HANDLER_RESULT_HANDLED;
    }

    if (!me->_server_started) {
      me->_server_started = true;
      me->_emit_event(event::DriverAttached{ClientType::jack});
    }

    me->_emit_event(
      event::PortsConnected{PortID::jack(client_name, port_name),
                            PortID::jack(client2_name, port2_name)});

    return DBUS_HANDLER_RESULT_HANDLED;
  }

  if (dbus_message_is_signal(
        message, JACKDBUS_IFACE_PATCHBAY, "PortsDisconnected")) {
    if (!dbus_message_get_args(message,
                               &me->_dbus_error,
                               DBUS_TYPE_UINT64,
                               &new_graph_version,
                               DBUS_TYPE_UINT64,
                               &client_id,
                               DBUS_TYPE_STRING,
                               &client_name,
                               DBUS_TYPE_UINT64,
                               &port_id,
                               DBUS_TYPE_STRING,
                               &port_name,
                               DBUS_TYPE_UINT64,
                               &client2_id,
                               DBUS_TYPE_STRING,
                               &client2_name,
                               DBUS_TYPE_UINT64,
                               &port2_id,
                               DBUS_TYPE_STRING,
                               &port2_name,
                               DBUS_TYPE_UINT64,
                               &connection_id,
                               DBUS_TYPE_INVALID)) {
      me->error_msg(fmt::format("dbus_message_get_args() failed to extract "
                                "PortsDisconnected signal arguments ({})",
                                me->_dbus_error.message));
      dbus_error_free(&me->_dbus_error);
      return DBUS_HANDLER_RESULT_HANDLED;
    }

    if (!me->_server_started) {
      me->_server_started = true;
      me->_emit_event(event::DriverAttached{ClientType::jack});
    }

    me->_emit_event(
      event::PortsDisconnected{PortID::jack(client_name, port_name),
                               PortID::jack(client2_name, port2_name)});

    return DBUS_HANDLER_RESULT_HANDLED;
  }

  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

bool
JackDriver::call(bool          response_expected,
                 const char*   iface,
                 const char*   method,
                 DBusMessage** reply_ptr_ptr,
                 int           in_type,
                 ...)
{
  DBusMessage* request_ptr = nullptr;
  DBusMessage* reply_ptr   = nullptr;
  va_list      ap;

  request_ptr = dbus_message_new_method_call(
    JACKDBUS_SERVICE, JACKDBUS_OBJECT, iface, method);
  if (!request_ptr) {
    throw std::runtime_error("dbus_message_new_method_call() returned 0");
  }

  va_start(ap, in_type);

  dbus_message_append_args_valist(request_ptr, in_type, ap);

  va_end(ap);

  // send message and get a handle for a reply
  reply_ptr = dbus_connection_send_with_reply_and_block(
    _dbus_connection, request_ptr, JACKDBUS_CALL_DEFAULT_TIMEOUT, &_dbus_error);

  dbus_message_unref(request_ptr);

  if (!reply_ptr) {
    if (response_expected) {
      error_msg(fmt::format("No reply from server when calling method {} ({})",
                            method,
                            _dbus_error.message));
    }
    _server_responding = false;
    dbus_error_free(&_dbus_error);
  } else {
    _server_responding = true;
    *reply_ptr_ptr     = reply_ptr;
  }

  return reply_ptr;
}

bool
JackDriver::is_started()
{
  DBusMessage* reply_ptr = nullptr;
  dbus_bool_t  started   = false;

  if (!call(false,
            JACKDBUS_IFACE_CONTROL,
            "IsStarted",
            &reply_ptr,
            DBUS_TYPE_INVALID)) {
    return false;
  }

  if (!dbus_message_get_args(reply_ptr,
                             &_dbus_error,
                             DBUS_TYPE_BOOLEAN,
                             &started,
                             DBUS_TYPE_INVALID)) {
    dbus_message_unref(reply_ptr);
    dbus_error_free(&_dbus_error);
    error_msg("Decoding reply of IsStarted failed");
    return false;
  }

  dbus_message_unref(reply_ptr);

  return started;
}

void
JackDriver::start_server()
{
  DBusMessage* reply_ptr = nullptr;

  if (!call(false,
            JACKDBUS_IFACE_CONTROL,
            "StartServer",
            &reply_ptr,
            DBUS_TYPE_INVALID)) {
    return;
  }

  dbus_message_unref(reply_ptr);

  update_attached();
}

void
JackDriver::stop_server()
{
  DBusMessage* reply_ptr = nullptr;

  if (!call(false,
            JACKDBUS_IFACE_CONTROL,
            "StopServer",
            &reply_ptr,
            DBUS_TYPE_INVALID)) {
    error_msg("Error stopping JACK server");
  }

  dbus_message_unref(reply_ptr);
  _emit_event(event::DriverDetached{ClientType::jack});
}

void
JackDriver::attach(bool launch_daemon)
{
  // Connect to the bus
  _dbus_connection = dbus_bus_get(DBUS_BUS_SESSION, &_dbus_error);
  if (dbus_error_is_set(&_dbus_error)) {
    error_msg(fmt::format("dbus_bus_get() failed ({})", _dbus_error.message));
    dbus_error_free(&_dbus_error);
    return;
  }

  dbus_connection_setup_with_g_main(_dbus_connection, nullptr);

  dbus_bus_add_match(_dbus_connection,
                     "type='signal',interface='" DBUS_INTERFACE_DBUS
                     "',member=NameOwnerChanged,arg0='org.jackaudio.service'",
                     nullptr);
  dbus_bus_add_match(_dbus_connection,
                     "type='signal',interface='" JACKDBUS_IFACE_PATCHBAY
                     "',member=PortAppeared",
                     nullptr);
  dbus_bus_add_match(_dbus_connection,
                     "type='signal',interface='" JACKDBUS_IFACE_PATCHBAY
                     "',member=PortDisappeared",
                     nullptr);
  dbus_bus_add_match(_dbus_connection,
                     "type='signal',interface='" JACKDBUS_IFACE_PATCHBAY
                     "',member=PortsConnected",
                     nullptr);
  dbus_bus_add_match(_dbus_connection,
                     "type='signal',interface='" JACKDBUS_IFACE_PATCHBAY
                     "',member=PortsDisconnected",
                     nullptr);

  dbus_connection_add_filter(
    _dbus_connection, dbus_message_hook, this, nullptr);

  update_attached();

  if (!_server_responding) {
    return;
  }

  if (launch_daemon) {
    start_server();
  }

  _log.info("[JACK] Attached to bus");
}

void
JackDriver::detach()
{
  stop_server();
}

bool
JackDriver::is_attached() const
{
  return _dbus_connection && _server_responding;
}

void
JackDriver::refresh(const EventSink& sink)
{
  DBusMessage*    reply_ptr              = nullptr;
  DBusMessageIter iter                   = {};
  dbus_uint64_t   version                = 0u;
  const char*     reply_signature        = nullptr;
  DBusMessageIter clients_array_iter     = {};
  DBusMessageIter client_struct_iter     = {};
  DBusMessageIter ports_array_iter       = {};
  DBusMessageIter port_struct_iter       = {};
  DBusMessageIter connections_array_iter = {};
  DBusMessageIter connection_struct_iter = {};
  dbus_uint64_t   client_id              = 0u;
  const char*     client_name            = nullptr;
  dbus_uint64_t   port_id                = 0u;
  const char*     port_name              = nullptr;
  dbus_uint32_t   port_flags             = 0u;
  dbus_uint32_t   port_type              = 0u;
  dbus_uint64_t   client2_id             = 0u;
  const char*     client2_name           = nullptr;
  dbus_uint64_t   port2_id               = 0u;
  const char*     port2_name             = nullptr;
  dbus_uint64_t   connection_id          = 0u;

  if (!call(true,
            JACKDBUS_IFACE_PATCHBAY,
            "GetGraph",
            &reply_ptr,
            DBUS_TYPE_UINT64,
            &version,
            DBUS_TYPE_INVALID)) {
    error_msg("GetGraph() failed");
    return;
  }

  reply_signature = dbus_message_get_signature(reply_ptr);

  if (strcmp(reply_signature, "ta(tsa(tsuu))a(tstststst)") != 0) {
    error_msg(std::string{"GetGraph() reply signature mismatch. "} +
              reply_signature);
    dbus_message_unref(reply_ptr);
    return;
  }

  dbus_message_iter_init(reply_ptr, &iter);

  dbus_message_iter_get_basic(&iter, &version);
  dbus_message_iter_next(&iter);

  _graph_version = version;

  // Emit all clients and ports
  for (dbus_message_iter_recurse(&iter, &clients_array_iter);
       dbus_message_iter_get_arg_type(&clients_array_iter) != DBUS_TYPE_INVALID;
       dbus_message_iter_next(&clients_array_iter)) {
    dbus_message_iter_recurse(&clients_array_iter, &client_struct_iter);

    dbus_message_iter_get_basic(&client_struct_iter, &client_id);
    dbus_message_iter_next(&client_struct_iter);

    dbus_message_iter_get_basic(&client_struct_iter, &client_name);
    dbus_message_iter_next(&client_struct_iter);

    // TODO: Pretty name?
    sink({event::ClientCreated{ClientID::jack(client_name), {client_name}}});

    for (dbus_message_iter_recurse(&client_struct_iter, &ports_array_iter);
         dbus_message_iter_get_arg_type(&ports_array_iter) != DBUS_TYPE_INVALID;
         dbus_message_iter_next(&ports_array_iter)) {
      dbus_message_iter_recurse(&ports_array_iter, &port_struct_iter);

      dbus_message_iter_get_basic(&port_struct_iter, &port_id);
      dbus_message_iter_next(&port_struct_iter);

      dbus_message_iter_get_basic(&port_struct_iter, &port_name);
      dbus_message_iter_next(&port_struct_iter);

      dbus_message_iter_get_basic(&port_struct_iter, &port_flags);
      dbus_message_iter_next(&port_struct_iter);

      dbus_message_iter_get_basic(&port_struct_iter, &port_type);
      dbus_message_iter_next(&port_struct_iter);

      sink({event::PortCreated{PortID::jack(client_name, port_name),
                               port_info(port_name, port_type, port_flags)}});
    }

    dbus_message_iter_next(&client_struct_iter);
  }

  dbus_message_iter_next(&iter);

  // Emit all connections
  for (dbus_message_iter_recurse(&iter, &connections_array_iter);
       dbus_message_iter_get_arg_type(&connections_array_iter) !=
       DBUS_TYPE_INVALID;
       dbus_message_iter_next(&connections_array_iter)) {
    dbus_message_iter_recurse(&connections_array_iter, &connection_struct_iter);

    dbus_message_iter_get_basic(&connection_struct_iter, &client_id);
    dbus_message_iter_next(&connection_struct_iter);

    dbus_message_iter_get_basic(&connection_struct_iter, &client_name);
    dbus_message_iter_next(&connection_struct_iter);

    dbus_message_iter_get_basic(&connection_struct_iter, &port_id);
    dbus_message_iter_next(&connection_struct_iter);

    dbus_message_iter_get_basic(&connection_struct_iter, &port_name);
    dbus_message_iter_next(&connection_struct_iter);

    dbus_message_iter_get_basic(&connection_struct_iter, &client2_id);
    dbus_message_iter_next(&connection_struct_iter);

    dbus_message_iter_get_basic(&connection_struct_iter, &client2_name);
    dbus_message_iter_next(&connection_struct_iter);

    dbus_message_iter_get_basic(&connection_struct_iter, &port2_id);
    dbus_message_iter_next(&connection_struct_iter);

    dbus_message_iter_get_basic(&connection_struct_iter, &port2_name);
    dbus_message_iter_next(&connection_struct_iter);

    dbus_message_iter_get_basic(&connection_struct_iter, &connection_id);
    dbus_message_iter_next(&connection_struct_iter);

    sink({event::PortsConnected{PortID::jack(client_name, port_name),
                                PortID::jack(client2_name, port2_name)}});
  }
}

bool
JackDriver::connect(const PortID& tail_id, const PortID& head_id)
{
  const auto        tail_names       = PortNames(tail_id);
  const auto        head_names       = PortNames(head_id);
  const char* const tail_client_name = tail_names.client().c_str();
  const char* const tail_port_name   = tail_names.port().c_str();
  const char* const head_client_name = head_names.client().c_str();
  const char* const head_port_name   = head_names.port().c_str();

  DBusMessage* reply_ptr = nullptr;

  if (!call(true,
            JACKDBUS_IFACE_PATCHBAY,
            "ConnectPortsByName",
            &reply_ptr,
            DBUS_TYPE_STRING,
            &tail_client_name,
            DBUS_TYPE_STRING,
            &tail_port_name,
            DBUS_TYPE_STRING,
            &head_client_name,
            DBUS_TYPE_STRING,
            &head_port_name,
            DBUS_TYPE_INVALID)) {
    error_msg("ConnectPortsByName() failed");
    return false;
  }

  return true;
}

bool
JackDriver::disconnect(const PortID& tail_id, const PortID& head_id)
{
  const auto        tail_names       = PortNames(tail_id);
  const auto        head_names       = PortNames(head_id);
  const char* const tail_client_name = tail_names.client().c_str();
  const char* const tail_port_name   = tail_names.port().c_str();
  const char* const head_client_name = head_names.client().c_str();
  const char* const head_port_name   = head_names.port().c_str();

  DBusMessage* reply_ptr = nullptr;

  if (!call(true,
            JACKDBUS_IFACE_PATCHBAY,
            "DisconnectPortsByName",
            &reply_ptr,
            DBUS_TYPE_STRING,
            &tail_client_name,
            DBUS_TYPE_STRING,
            &tail_port_name,
            DBUS_TYPE_STRING,
            &head_client_name,
            DBUS_TYPE_STRING,
            &head_port_name,
            DBUS_TYPE_INVALID)) {
    error_msg("DisconnectPortsByName() failed");
    return false;
  }

  return true;
}

uint32_t
JackDriver::xruns()
{
  DBusMessage*  reply_ptr = nullptr;
  dbus_uint32_t xruns     = 0u;

  if (_server_responding && !_server_started) {
    return 0;
  }

  if (!call(true,
            JACKDBUS_IFACE_CONTROL,
            "GetXruns",
            &reply_ptr,
            DBUS_TYPE_INVALID)) {
    return 0;
  }

  if (!dbus_message_get_args(
        reply_ptr, &_dbus_error, DBUS_TYPE_UINT32, &xruns, DBUS_TYPE_INVALID)) {
    dbus_message_unref(reply_ptr);
    dbus_error_free(&_dbus_error);
    error_msg("Decoding reply of GetXruns failed");
    return 0;
  }

  dbus_message_unref(reply_ptr);

  return xruns;
}

void
JackDriver::reset_xruns()
{
  DBusMessage* reply_ptr = nullptr;

  if (!call(true,
            JACKDBUS_IFACE_CONTROL,
            "ResetXruns",
            &reply_ptr,
            DBUS_TYPE_INVALID)) {
    return;
  }

  dbus_message_unref(reply_ptr);
}

uint32_t
JackDriver::buffer_size()
{
  DBusMessage*  reply_ptr   = nullptr;
  dbus_uint32_t buffer_size = 0u;

  if (_server_responding && !_server_started) {
    return 4096;
  }

  if (!call(true,
            JACKDBUS_IFACE_CONTROL,
            "GetBufferSize",
            &reply_ptr,
            DBUS_TYPE_INVALID)) {
    return 4096;
  }

  if (!dbus_message_get_args(reply_ptr,
                             &_dbus_error,
                             DBUS_TYPE_UINT32,
                             &buffer_size,
                             DBUS_TYPE_INVALID)) {
    dbus_message_unref(reply_ptr);
    dbus_error_free(&_dbus_error);
    error_msg("Decoding reply of GetBufferSize failed");
    return 4096;
  }

  dbus_message_unref(reply_ptr);

  return buffer_size;
}

bool
JackDriver::set_buffer_size(const uint32_t frames)
{
  DBusMessage*  reply_ptr   = nullptr;
  dbus_uint32_t buffer_size = frames;

  if (!call(true,
            JACKDBUS_IFACE_CONTROL,
            "SetBufferSize",
            &reply_ptr,
            DBUS_TYPE_UINT32,
            &buffer_size,
            DBUS_TYPE_INVALID)) {
    return false;
  }

  dbus_message_unref(reply_ptr);

  return true;
}

uint32_t
JackDriver::sample_rate()
{
  DBusMessage*  reply_ptr   = nullptr;
  dbus_uint32_t sample_rate = 0u;

  if (!call(true,
            JACKDBUS_IFACE_CONTROL,
            "GetSampleRate",
            &reply_ptr,
            DBUS_TYPE_INVALID)) {
    return false;
  }

  if (!dbus_message_get_args(reply_ptr,
                             &_dbus_error,
                             DBUS_TYPE_UINT32,
                             &sample_rate,
                             DBUS_TYPE_INVALID)) {
    dbus_message_unref(reply_ptr);
    dbus_error_free(&_dbus_error);
    error_msg("Decoding reply of GetSampleRate failed");
    return false;
  }

  dbus_message_unref(reply_ptr);

  return sample_rate;
}

PortType
JackDriver::patchage_port_type(const dbus_uint32_t dbus_port_type) const
{
  switch (dbus_port_type) {
  case JACKDBUS_PORT_TYPE_AUDIO:
    return PortType::jack_audio;
  case JACKDBUS_PORT_TYPE_MIDI:
    return PortType::jack_midi;
  default:
    break;
  }

  error_msg(fmt::format("Unknown JACK D-Bus port type {}", dbus_port_type));
  return PortType::jack_audio;
}

PortInfo
JackDriver::port_info(const std::string&  port_name,
                      const dbus_uint32_t port_type,
                      const dbus_uint32_t port_flags) const
{
  const SignalDirection direction =
    ((port_flags & JACKDBUS_PORT_FLAG_INPUT) ? SignalDirection::input
                                             : SignalDirection::output);

  // TODO: Metadata?
  return {port_name,
          patchage_port_type(port_type),
          direction,
          {},
          bool(port_flags & JACKDBUS_PORT_FLAG_TERMINAL)};
}

void
JackDriver::error_msg(const std::string& msg) const
{
  _log.error(std::string{"[JACK] "} + msg);
}

void
JackDriver::info_msg(const std::string& msg) const
{
  _log.info(std::string{"[JACK] "} + msg);
}

} // namespace

std::unique_ptr<AudioDriver>
make_jack_driver(ILog& log, Driver::EventSink emit_event)
{
  return std::unique_ptr<AudioDriver>{
    new JackDriver{log, std::move(emit_event)}};
}

} // namespace patchage
