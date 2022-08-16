// Copyright 2007-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Reactor.hpp"

#include "Action.hpp"
#include "Canvas.hpp"
#include "CanvasModule.hpp"
#include "CanvasPort.hpp"
#include "ClientType.hpp"
#include "Configuration.hpp"
#include "Driver.hpp"
#include "Drivers.hpp"
#include "ILog.hpp"
#include "PortID.hpp"
#include "Setting.hpp"
#include "SignalDirection.hpp"
#include "warnings.hpp"

#include "ganv/Module.hpp"
#include "ganv/Port.hpp"

PATCHAGE_DISABLE_FMT_WARNINGS
#include <fmt/core.h>
PATCHAGE_RESTORE_WARNINGS

#include <boost/variant/apply_visitor.hpp>

namespace patchage {

class SettingVisitor
{
public:
  using result_type = void; ///< For boost::apply_visitor

  explicit SettingVisitor(Configuration& conf)
    : _conf{conf}
  {}

  template<class S>
  void operator()(const S& setting) const
  {
    _conf.set_setting(setting);
  }

private:
  Configuration& _conf;
};

Reactor::Reactor(Configuration& conf,
                 Drivers&       drivers,
                 Canvas&        canvas,
                 ILog&          log)
  : _conf{conf}
  , _drivers{drivers}
  , _canvas{canvas}
  , _log{log}
{}

void
Reactor::operator()(const action::ChangeSetting& action)
{
  SettingVisitor visitor{_conf};
  boost::apply_visitor(visitor, action.setting);
}

void
Reactor::operator()(const action::ConnectPorts& action)
{
  if (action.tail.type() == action.head.type()) {
    if (auto* d = _drivers.driver(action.tail.type())) {
      d->connect(action.tail, action.head);
    } else {
      _log.error(fmt::format("No driver for {}", action.tail.type()));
    }
  } else {
    _log.warning("Unable to connect incompatible port");
  }
}

void
Reactor::operator()(const action::DecreaseFontSize&)
{
  _conf.set<setting::FontSize>(_conf.get<setting::FontSize>() - 1.0f);
}

void
Reactor::operator()(const action::DisconnectClient& action)
{
  if (CanvasModule* mod = find_module(action.client, action.direction)) {
    for (Ganv::Port* p : *mod) {
      if (p) {
        p->disconnect();
      }
    }
  }
}

void
Reactor::operator()(const action::DisconnectPort& action)
{
  if (CanvasPort* port = find_port(action.port)) {
    port->disconnect();
  }
}

void
Reactor::operator()(const action::DisconnectPorts& action)
{
  if (action.tail.type() == action.head.type()) {
    if (auto* d = _drivers.driver(action.tail.type())) {
      d->disconnect(action.tail, action.head);
    } else {
      _log.error(fmt::format("No driver available to disconnect ports"));
    }
  } else {
    _log.error("Unable to disconnect incompatible ports");
  }
}

void
Reactor::operator()(const action::IncreaseFontSize&)
{
  _conf.set<setting::FontSize>(_conf.get<setting::FontSize>() + 1.0f);
}

void
Reactor::operator()(const action::MoveModule& action)
{
  _conf.set_module_location(
    module_name(action.client), action.direction, {action.x, action.y});
}

void
Reactor::operator()(const action::Refresh&)
{
  _drivers.refresh();
}

void
Reactor::operator()(const action::ResetFontSize&)
{
  _conf.set<setting::FontSize>(_canvas.get_default_font_size());
}

void
Reactor::operator()(const action::SplitModule& action)
{
  _conf.set_module_split(module_name(action.client), true);
  _drivers.refresh();
}

void
Reactor::operator()(const action::UnsplitModule& action)
{
  _conf.set_module_split(module_name(action.client), false);
  _drivers.refresh();
}

void
Reactor::operator()(const action::ZoomFull&)
{
  _canvas.zoom_full();
  _conf.set<setting::Zoom>(_canvas.get_zoom());
}

void
Reactor::operator()(const action::ZoomIn&)
{
  _conf.set<setting::Zoom>(_conf.get<setting::Zoom>() * 1.25f);
}

void
Reactor::operator()(const action::ZoomNormal&)
{
  _conf.set<setting::Zoom>(1.0);
}

void
Reactor::operator()(const action::ZoomOut&)
{
  _conf.set<setting::Zoom>(_conf.get<setting::Zoom>() * 0.75f);
}

void
Reactor::operator()(const Action& action)
{
  boost::apply_visitor(*this, action);
}

std::string
Reactor::module_name(const ClientID& client)
{
  // Note that split modules always have the same name

  if (CanvasModule* mod = find_module(client, SignalDirection::input)) {
    return mod->name();
  }

  if (CanvasModule* mod = find_module(client, SignalDirection::output)) {
    return mod->name();
  }

  return std::string{};
}

CanvasModule*
Reactor::find_module(const ClientID& client, const SignalDirection type)
{
  return _canvas.find_module(client, type);
}

CanvasPort*
Reactor::find_port(const PortID& port)
{
  return _canvas.find_port(port);
}

} // namespace patchage
