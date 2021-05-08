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

#ifndef PATCHAGE_CANVASMODULE_HPP
#define PATCHAGE_CANVASMODULE_HPP

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

class CanvasPort;
class Patchage;

class CanvasModule : public Ganv::Module
{
public:
  CanvasModule(Patchage*          app,
               const std::string& name,
               SignalDirection    type,
               ClientID           id,
               double             x = 0.0,
               double             y = 0.0);

  CanvasModule(const CanvasModule&) = delete;
  CanvasModule& operator=(const CanvasModule&) = delete;

  CanvasModule(CanvasModule&&) = delete;
  CanvasModule& operator=(CanvasModule&&) = delete;

  ~CanvasModule() override;

  bool show_menu(GdkEventButton* ev);
  void update_menu();

  void split();
  void join();
  void disconnect_all();

  CanvasPort* get_port(const PortID& id);

  void load_location();
  void store_location(double x, double y);

  SignalDirection    type() const { return _type; }
  ClientID           id() const { return _id; }
  const std::string& name() const { return _name; }

protected:
  bool on_event(GdkEvent* ev) override;

  Patchage*                  _app;
  std::unique_ptr<Gtk::Menu> _menu;
  std::string                _name;
  SignalDirection            _type;
  ClientID                   _id;
};

} // namespace patchage

#endif // PATCHAGE_CANVASMODULE_HPP
