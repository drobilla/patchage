// Copyright 2007-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "make_alsa_driver.hpp"

#include "ClientID.hpp"
#include "ClientInfo.hpp"
#include "ClientType.hpp"
#include "Driver.hpp"
#include "Event.hpp"
#include "ILog.hpp"
#include "PortID.hpp"
#include "PortInfo.hpp"
#include "PortType.hpp"
#include "SignalDirection.hpp"
#include "warnings.hpp"

PATCHAGE_DISABLE_FMT_WARNINGS
#include <fmt/core.h>
#include <fmt/ostream.h>
PATCHAGE_RESTORE_WARNINGS

#include <alsa/asoundlib.h>
#include <pthread.h>

#include <boost/optional/optional.hpp>

#include <cassert>
#include <cstdint>
#include <functional>
#include <iosfwd>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <utility>

namespace patchage {
namespace {

/// Driver for ALSA Sequencer ports
class AlsaDriver : public Driver
{
public:
  explicit AlsaDriver(ILog& log, EventSink emit_event);

  AlsaDriver(const AlsaDriver&) = delete;
  AlsaDriver& operator=(const AlsaDriver&) = delete;

  AlsaDriver(AlsaDriver&&) = delete;
  AlsaDriver& operator=(AlsaDriver&&) = delete;

  ~AlsaDriver() override;

  void attach(bool launch_daemon) override;
  void detach() override;

  bool is_attached() const override { return (_seq != nullptr); }

  void refresh(const EventSink& sink) override;

  bool connect(const PortID& tail_id, const PortID& head_id) override;

  bool disconnect(const PortID& tail_id, const PortID& head_id) override;

private:
  bool         create_refresh_port();
  static void* refresh_main(void* me);
  void         _refresh_main();

  ILog&      _log;
  snd_seq_t* _seq;
  pthread_t  _refresh_thread;

  struct SeqAddrComparator {
    bool operator()(const snd_seq_addr_t& a, const snd_seq_addr_t& b) const
    {
      return ((a.client < b.client) ||
              ((a.client == b.client) && a.port < b.port));
    }
  };

  using Ignored = std::set<snd_seq_addr_t, SeqAddrComparator>;

  Ignored _ignored;

  bool ignore(const snd_seq_addr_t& addr, bool add = true);
};

PortID
addr_to_id(const snd_seq_addr_t& addr, const bool is_input)
{
  return PortID::alsa(addr.client, addr.port, is_input);
}

SignalDirection
port_direction(const snd_seq_port_info_t* const pinfo)
{
  const int caps = snd_seq_port_info_get_capability(pinfo);

  if ((caps & SND_SEQ_PORT_CAP_READ) && (caps & SND_SEQ_PORT_CAP_WRITE)) {
    return SignalDirection::duplex;
  }

  if (caps & SND_SEQ_PORT_CAP_READ) {
    return SignalDirection::output;
  }

  if (caps & SND_SEQ_PORT_CAP_WRITE) {
    return SignalDirection::input;
  }

  return SignalDirection::duplex;
}

ClientInfo
client_info(snd_seq_client_info_t* const cinfo)
{
  return {snd_seq_client_info_get_name(cinfo)};
}

PortInfo
port_info(const snd_seq_port_info_t* const pinfo)
{
  const int type = snd_seq_port_info_get_type(pinfo);

  return {snd_seq_port_info_get_name(pinfo),
          PortType::alsa_midi,
          port_direction(pinfo),
          snd_seq_port_info_get_port(pinfo),
          (type & SND_SEQ_PORT_TYPE_APPLICATION) == 0};
}

AlsaDriver::AlsaDriver(ILog& log, EventSink emit_event)
  : Driver{std::move(emit_event)}
  , _log(log)
  , _seq(nullptr)
  , _refresh_thread{}
{}

AlsaDriver::~AlsaDriver()
{
  detach();
}

void
AlsaDriver::attach(bool /*launch_daemon*/)
{
  int ret = snd_seq_open(&_seq, "default", SND_SEQ_OPEN_DUPLEX, 0);
  if (ret) {
    _log.error("[ALSA] Unable to attach");
    _seq = nullptr;
  } else {
    _emit_event(event::DriverAttached{ClientType::alsa});

    snd_seq_set_client_name(_seq, "Patchage");

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 50000);

    ret =
      pthread_create(&_refresh_thread, &attr, &AlsaDriver::refresh_main, this);
    if (ret) {
      _log.error("[ALSA] Failed to start refresh thread");
    }
  }
}

void
AlsaDriver::detach()
{
  if (_seq) {
    pthread_cancel(_refresh_thread);
    pthread_join(_refresh_thread, nullptr);
    snd_seq_close(_seq);
    _seq = nullptr;
    _emit_event(event::DriverDetached{ClientType::alsa});
  }
}

void
AlsaDriver::refresh(const EventSink& sink)
{
  if (!is_attached() || !_seq) {
    return;
  }

  _ignored.clear();

  snd_seq_client_info_t* cinfo = nullptr;
  snd_seq_client_info_alloca(&cinfo);
  snd_seq_client_info_set_client(cinfo, -1);

  snd_seq_port_info_t* pinfo = nullptr;
  snd_seq_port_info_alloca(&pinfo);

  // Emit all clients
  snd_seq_client_info_set_client(cinfo, -1);
  while (snd_seq_query_next_client(_seq, cinfo) >= 0) {
    const auto client_id = snd_seq_client_info_get_client(cinfo);

    assert(client_id < std::numeric_limits<uint8_t>::max());
    sink({event::ClientCreated{ClientID::alsa(static_cast<uint8_t>(client_id)),
                               client_info(cinfo)}});
  }

  // Emit all ports
  snd_seq_client_info_set_client(cinfo, -1);
  while (snd_seq_query_next_client(_seq, cinfo) >= 0) {
    const auto client_id = snd_seq_client_info_get_client(cinfo);

    snd_seq_port_info_set_client(pinfo, client_id);
    snd_seq_port_info_set_port(pinfo, -1);
    while (snd_seq_query_next_port(_seq, pinfo) >= 0) {
      const auto addr = *snd_seq_port_info_get_addr(pinfo);
      if (!ignore(addr)) {
        const auto caps = snd_seq_port_info_get_capability(pinfo);
        auto       info = port_info(pinfo);

        if (caps & SND_SEQ_PORT_CAP_READ) {
          info.direction = SignalDirection::output;
          sink({event::PortCreated{addr_to_id(addr, false), info}});
        }

        if (caps & SND_SEQ_PORT_CAP_WRITE) {
          info.direction = SignalDirection::input;
          sink({event::PortCreated{addr_to_id(addr, true), info}});
        }
      }
    }
  }

  // Emit all connections
  snd_seq_client_info_set_client(cinfo, -1);
  while (snd_seq_query_next_client(_seq, cinfo) >= 0) {
    const auto client_id = snd_seq_client_info_get_client(cinfo);

    snd_seq_port_info_set_client(pinfo, client_id);
    snd_seq_port_info_set_port(pinfo, -1);
    while (snd_seq_query_next_port(_seq, pinfo) >= 0) {
      const auto port_addr = *snd_seq_port_info_get_addr(pinfo);
      const auto caps      = snd_seq_port_info_get_capability(pinfo);

      if (!ignore(port_addr) && (caps & SND_SEQ_PORT_CAP_READ)) {
        const auto tail_id = addr_to_id(port_addr, false);

        snd_seq_query_subscribe_t* sinfo = nullptr;
        snd_seq_query_subscribe_alloca(&sinfo);
        snd_seq_query_subscribe_set_type(sinfo, SND_SEQ_QUERY_SUBS_READ);
        snd_seq_query_subscribe_set_root(sinfo, &port_addr);
        snd_seq_query_subscribe_set_index(sinfo, 0);
        while (!snd_seq_query_port_subscribers(_seq, sinfo)) {
          const auto head_addr = *snd_seq_query_subscribe_get_addr(sinfo);
          const auto head_id   = addr_to_id(head_addr, true);

          sink({event::PortsConnected{tail_id, head_id}});

          snd_seq_query_subscribe_set_index(
            sinfo, snd_seq_query_subscribe_get_index(sinfo) + 1);
        }
      }
    }
  }
}

bool
AlsaDriver::ignore(const snd_seq_addr_t& addr, bool add)
{
  if (_ignored.find(addr) != _ignored.end()) {
    return true;
  }

  if (!add) {
    return false;
  }

  snd_seq_client_info_t* cinfo = nullptr;
  snd_seq_client_info_alloca(&cinfo);
  snd_seq_client_info_set_client(cinfo, addr.client);
  snd_seq_get_any_client_info(_seq, addr.client, cinfo);

  snd_seq_port_info_t* pinfo = nullptr;
  snd_seq_port_info_alloca(&pinfo);
  snd_seq_port_info_set_client(pinfo, addr.client);
  snd_seq_port_info_set_port(pinfo, addr.port);
  snd_seq_get_any_port_info(_seq, addr.client, addr.port, pinfo);

  const int type = snd_seq_port_info_get_type(pinfo);
  const int caps = snd_seq_port_info_get_capability(pinfo);

  if (caps & SND_SEQ_PORT_CAP_NO_EXPORT) {
    _ignored.insert(addr);
    return true;
  }

  if (!((caps & SND_SEQ_PORT_CAP_READ) || (caps & SND_SEQ_PORT_CAP_WRITE) ||
        (caps & SND_SEQ_PORT_CAP_DUPLEX))) {
    _ignored.insert(addr);
    return true;
  }

  if ((snd_seq_client_info_get_type(cinfo) != SND_SEQ_USER_CLIENT) &&
      ((type == SND_SEQ_PORT_SYSTEM_TIMER ||
        type == SND_SEQ_PORT_SYSTEM_ANNOUNCE))) {
    _ignored.insert(addr);
    return true;
  }

  return false;
}

bool
AlsaDriver::connect(const PortID& tail_id, const PortID& head_id)
{
  if (tail_id.type() != PortID::Type::alsa ||
      head_id.type() != PortID::Type::alsa) {
    _log.error("[ALSA] Attempt to connect non-ALSA ports");
    return false;
  }

  const snd_seq_addr_t tail_addr = {tail_id.alsa_client(), tail_id.alsa_port()};
  const snd_seq_addr_t head_addr = {head_id.alsa_client(), head_id.alsa_port()};

  if (tail_addr.client == head_addr.client &&
      tail_addr.port == head_addr.port) {
    _log.warning("[ALSA] Refusing to connect port to itself");
    return false;
  }

  bool result = true;

  snd_seq_port_subscribe_t* subs = nullptr;
  snd_seq_port_subscribe_malloc(&subs);
  snd_seq_port_subscribe_set_sender(subs, &tail_addr);
  snd_seq_port_subscribe_set_dest(subs, &head_addr);
  snd_seq_port_subscribe_set_exclusive(subs, 0);
  snd_seq_port_subscribe_set_time_update(subs, 0);
  snd_seq_port_subscribe_set_time_real(subs, 0);

  // Already connected (shouldn't happen)
  if (!snd_seq_get_port_subscription(_seq, subs)) {
    _log.error("[ALSA] Attempt to double subscribe ports");
    result = false;
  }

  int ret = snd_seq_subscribe_port(_seq, subs);
  if (ret < 0) {
    _log.error(
      fmt::format("[ALSA] Subscription failed ({})", snd_strerror(ret)));
    result = false;
  }

  if (!result) {
    _log.error(
      fmt::format("[ALSA] Failed to connect {} => {}", tail_id, head_id));
  }

  return (!result);
}

bool
AlsaDriver::disconnect(const PortID& tail_id, const PortID& head_id)
{
  if (tail_id.type() != PortID::Type::alsa ||
      head_id.type() != PortID::Type::alsa) {
    _log.error("[ALSA] Attempt to disconnect non-ALSA ports");
    return false;
  }

  const snd_seq_addr_t tail_addr = {tail_id.alsa_client(), tail_id.alsa_port()};
  const snd_seq_addr_t head_addr = {head_id.alsa_client(), head_id.alsa_port()};

  snd_seq_port_subscribe_t* subs = nullptr;
  snd_seq_port_subscribe_malloc(&subs);
  snd_seq_port_subscribe_set_sender(subs, &tail_addr);
  snd_seq_port_subscribe_set_dest(subs, &head_addr);
  snd_seq_port_subscribe_set_exclusive(subs, 0);
  snd_seq_port_subscribe_set_time_update(subs, 0);
  snd_seq_port_subscribe_set_time_real(subs, 0);

  // Not connected (shouldn't happen)
  if (snd_seq_get_port_subscription(_seq, subs) != 0) {
    _log.error("[ALSA] Attempt to unsubscribe ports that are not subscribed");
    return false;
  }

  int ret = snd_seq_unsubscribe_port(_seq, subs);
  if (ret < 0) {
    _log.error(fmt::format("[ALSA] Failed to disconnect {} => {} ({})",
                           tail_id,
                           head_id,
                           snd_strerror(ret)));
    return false;
  }

  return true;
}

bool
AlsaDriver::create_refresh_port()
{
  snd_seq_port_info_t* port_info = nullptr;
  snd_seq_port_info_alloca(&port_info);
  snd_seq_port_info_set_name(port_info, "System Announcement Receiver");
  snd_seq_port_info_set_type(port_info, SND_SEQ_PORT_TYPE_APPLICATION);
  snd_seq_port_info_set_capability(port_info,
                                   SND_SEQ_PORT_CAP_WRITE |
                                     SND_SEQ_PORT_CAP_SUBS_WRITE |
                                     SND_SEQ_PORT_CAP_NO_EXPORT);

  int ret = snd_seq_create_port(_seq, port_info);
  if (ret) {
    _log.error(
      fmt::format("[ALSA] Error creating port ({})", snd_strerror(ret)));
    return false;
  }

  // Subscribe the port to the system announcer
  ret = snd_seq_connect_from(_seq,
                             snd_seq_port_info_get_port(port_info),
                             SND_SEQ_CLIENT_SYSTEM,
                             SND_SEQ_PORT_SYSTEM_ANNOUNCE);
  if (ret) {
    _log.error(
      fmt::format("[ALSA] Failed to connect to system announce port ({})",
                  snd_strerror(ret)));
    return false;
  }

  return true;
}

void*
AlsaDriver::refresh_main(void* me)
{
  auto* ad = static_cast<AlsaDriver*>(me);
  ad->_refresh_main();
  return nullptr;
}

void
AlsaDriver::_refresh_main()
{
  if (!create_refresh_port()) {
    _log.error("[ALSA] Could not create listen port, auto-refresh disabled");
    return;
  }

  int caps = 0;

  snd_seq_client_info_t* cinfo = nullptr;
  snd_seq_client_info_alloca(&cinfo);

  snd_seq_port_info_t* pinfo = nullptr;
  snd_seq_port_info_alloca(&pinfo);

  snd_seq_event_t* ev = nullptr;
  while (snd_seq_event_input(_seq, &ev) > 0) {
    assert(ev);

    switch (ev->type) {
    case SND_SEQ_EVENT_CLIENT_START:
      snd_seq_get_any_client_info(_seq, ev->data.addr.client, cinfo);
      _emit_event(event::ClientCreated{
        ClientID::alsa(ev->data.addr.client),
        client_info(cinfo),
      });
      break;

    case SND_SEQ_EVENT_CLIENT_EXIT:
      _emit_event(event::ClientDestroyed{
        ClientID::alsa(ev->data.addr.client),
      });
      break;

    case SND_SEQ_EVENT_CLIENT_CHANGE:
      break;

    case SND_SEQ_EVENT_PORT_START:
      snd_seq_get_any_client_info(_seq, ev->data.addr.client, cinfo);
      snd_seq_get_any_port_info(
        _seq, ev->data.addr.client, ev->data.addr.port, pinfo);
      caps = snd_seq_port_info_get_capability(pinfo);

      if (!ignore(ev->data.addr)) {
        _emit_event(event::PortCreated{
          addr_to_id(ev->data.addr, (caps & SND_SEQ_PORT_CAP_WRITE)),
          port_info(pinfo),
        });
      }
      break;

    case SND_SEQ_EVENT_PORT_EXIT:
      if (!ignore(ev->data.addr, false)) {
        // Note: getting caps at this point does not work
        // Delete both inputs and outputs (to handle duplex ports)
        _emit_event(event::PortDestroyed{addr_to_id(ev->data.addr, true)});
        _emit_event(event::PortDestroyed{addr_to_id(ev->data.addr, false)});
      }
      break;

    case SND_SEQ_EVENT_PORT_CHANGE:
      break;

    case SND_SEQ_EVENT_PORT_SUBSCRIBED:
      if (!ignore(ev->data.connect.sender) && !ignore(ev->data.connect.dest)) {
        _emit_event(
          event::PortsConnected{addr_to_id(ev->data.connect.sender, false),
                                addr_to_id(ev->data.connect.dest, true)});
      }
      break;

    case SND_SEQ_EVENT_PORT_UNSUBSCRIBED:
      if (!ignore(ev->data.connect.sender) && !ignore(ev->data.connect.dest)) {
        _emit_event(
          event::PortsDisconnected{addr_to_id(ev->data.connect.sender, false),
                                   addr_to_id(ev->data.connect.dest, true)});
      }
      break;

    case SND_SEQ_EVENT_RESET:
    default:
      break;
    }
  }
}

} // namespace

std::unique_ptr<Driver>
make_alsa_driver(ILog& log, Driver::EventSink emit_event)
{
  return std::unique_ptr<Driver>{new AlsaDriver{log, std::move(emit_event)}};
}

} // namespace patchage
