/* This file is part of Patchage.
 * Copyright 2007-2021 David Robillard <d@drobilla.net>
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

#ifndef PATCHAGE_CANVASMODULE_HPP
#define PATCHAGE_CANVASMODULE_HPP

#include "ActionSink.hpp"
#include "ClientID.hpp"
#include "SignalDirection.hpp"
#include "warnings.hpp"

PATCHAGE_DISABLE_GANV_WARNINGS
#include "ganv/Module.hpp"
PATCHAGE_RESTORE_WARNINGS

#include <gdk/gdk.h>

#include <memory>
#include <string>

namespace Gtk {
class Menu;
} // namespace Gtk

namespace patchage {

struct PortID;

class Canvas;
class CanvasPort;

class CanvasModule : public Ganv::Module
{
public:
  CanvasModule(Canvas&            canvas,
               ActionSink&        action_sink,
               const std::string& name,
               SignalDirection    type,
               ClientID           id,
               double             x,
               double             y);

  CanvasModule(const CanvasModule&) = delete;
  CanvasModule& operator=(const CanvasModule&) = delete;

  CanvasModule(CanvasModule&&) = delete;
  CanvasModule& operator=(CanvasModule&&) = delete;

  ~CanvasModule() override = default;

  bool show_menu(GdkEventButton* ev);
  void update_menu();

  CanvasPort* get_port(const PortID& id);

  SignalDirection    type() const { return _type; }
  ClientID           id() const { return _id; }
  const std::string& name() const { return _name; }

protected:
  bool on_event(GdkEvent* ev) override;
  void on_moved(double x, double y);
  void on_split();
  void on_join();
  void on_disconnect();

  ActionSink&                _action_sink;
  std::unique_ptr<Gtk::Menu> _menu;
  std::string                _name;
  SignalDirection            _type;
  ClientID                   _id;
};

} // namespace patchage

#endif // PATCHAGE_CANVASMODULE_HPP
