// Copyright 2007-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: GPL-3.0-or-later

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
#include <gtkmm/menu.h>

#include <memory>
#include <string>

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
