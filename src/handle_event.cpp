/* This file is part of Patchage.
 * Copyright 2007-2020 David Robillard <d@drobilla.net>
 *
 * Patchage is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Patchage is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Patchage.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "handle_event.hpp"

#include "Canvas.hpp"
#include "CanvasPort.hpp"
#include "ClientType.hpp"
#include "Configuration.hpp"
#include "Event.hpp"
#include "ILog.hpp"
#include "Metadata.hpp"
#include "PortID.hpp"
#include "Setting.hpp"
#include "warnings.hpp"

PATCHAGE_DISABLE_FMT_WARNINGS
#include <fmt/core.h>
#include <fmt/ostream.h>
PATCHAGE_RESTORE_WARNINGS

#include <boost/variant/apply_visitor.hpp>

#include <iosfwd>

namespace patchage {

namespace {

class EventHandler
{
public:
  using result_type = void; ///< For boost::apply_visitor

  explicit EventHandler(Configuration& conf,
                        Metadata&      metadata,
                        Canvas&        canvas,
                        ILog&          log)
    : _conf{conf}
    , _metadata{metadata}
    , _canvas{canvas}
    , _log{log}
  {}

  void operator()(const event::Cleared&) { _canvas.clear(); }

  void operator()(const event::DriverAttached& event)
  {
    switch (event.type) {
    case ClientType::alsa:
      _conf.set<setting::AlsaAttached>(true);
      break;
    case ClientType::jack:
      _conf.set<setting::JackAttached>(true);
      break;
    }
  }

  void operator()(const event::DriverDetached& event)
  {
    switch (event.type) {
    case ClientType::alsa:
      _conf.set<setting::AlsaAttached>(false);
      break;
    case ClientType::jack:
      _conf.set<setting::JackAttached>(false);
      break;
    }
  }

  void operator()(const event::ClientCreated& event)
  {
    // Don't create empty modules, they will be created when ports are added
    _metadata.set_client(event.id, event.info);
  }

  void operator()(const event::ClientDestroyed& event)
  {
    _canvas.remove_module(event.id);
    _metadata.erase_client(event.id);
  }

  void operator()(const event::PortCreated& event)
  {
    _metadata.set_port(event.id, event.info);

    auto* const port =
      _canvas.create_port(_conf, _metadata, event.id, event.info);

    if (!port) {
      _log.error(
        fmt::format("Unable to create view for port \"{}\"", event.id));
    }
  }

  void operator()(const event::PortDestroyed& event)
  {
    _canvas.remove_port(event.id);
    _metadata.erase_port(event.id);
  }

  void operator()(const event::PortsConnected& event)
  {
    CanvasPort* port_1 = _canvas.find_port(event.tail);
    CanvasPort* port_2 = _canvas.find_port(event.head);

    if (!port_1) {
      _log.error(
        fmt::format("Unable to find port \"{}\" to connect", event.tail));
    } else if (!port_2) {
      _log.error(
        fmt::format("Unable to find port \"{}\" to connect", event.head));
    } else {
      _canvas.make_connection(port_1, port_2);
    }
  }

  void operator()(const event::PortsDisconnected& event)
  {
    CanvasPort* port_1 = _canvas.find_port(event.tail);
    CanvasPort* port_2 = _canvas.find_port(event.head);

    if (!port_1) {
      _log.error(
        fmt::format("Unable to find port \"{}\" to disconnect", event.tail));
    } else if (!port_2) {
      _log.error(
        fmt::format("Unable to find port \"{}\" to disconnect", event.head));
    } else {
      _canvas.remove_edge_between(port_1, port_2);
    }
  }

private:
  Configuration& _conf;
  Metadata&      _metadata;
  Canvas&        _canvas;
  ILog&          _log;
};

} // namespace

void
handle_event(Configuration& conf,
             Metadata&      metadata,
             Canvas&        canvas,
             ILog&          log,
             const Event&   event)
{
  EventHandler handler{conf, metadata, canvas, log};
  boost::apply_visitor(handler, event);
}

} // namespace patchage
