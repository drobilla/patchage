// Copyright 2010-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "CanvasModule.hpp"

#include "Action.hpp"
#include "ActionSink.hpp"
#include "Canvas.hpp"
#include "CanvasPort.hpp"
#include "ClientID.hpp"
#include "PortID.hpp"
#include "SignalDirection.hpp"
#include "i18n.hpp"
#include "warnings.hpp"

PATCHAGE_DISABLE_GANV_WARNINGS
#include "ganv/Module.hpp"
#include "ganv/Port.hpp"
PATCHAGE_RESTORE_WARNINGS

#include <glibmm/helperlist.h>
#include <gtkmm/menu.h>
#include <gtkmm/menu_elems.h>
#include <gtkmm/menuitem.h>
#include <gtkmm/menushell.h>
#include <sigc++/functors/mem_fun.h>
#include <sigc++/signal.h>

#include <cassert>
#include <functional>
#include <memory>
#include <utility>

namespace patchage {

CanvasModule::CanvasModule(Canvas&            canvas,
                           ActionSink&        action_sink,
                           const std::string& name,
                           SignalDirection    type,
                           ClientID           id,
                           double             x,
                           double             y)
  : Module(canvas, name, x, y)
  , _action_sink(action_sink)
  , _name(name)
  , _type(type)
  , _id(std::move(id))
{
  signal_event().connect(sigc::mem_fun(this, &CanvasModule::on_event));
  signal_moved().connect(sigc::mem_fun(this, &CanvasModule::on_moved));
}

void
CanvasModule::update_menu()
{
  if (!_menu) {
    return;
  }

  if (_type == SignalDirection::duplex) {
    bool has_in  = false;
    bool has_out = false;
    for (const auto* p : *this) {
      if (p) {
        if (p->is_input()) {
          has_in = true;
        } else {
          has_out = true;
        }

        if (has_in && has_out) {
          break;
        }
      }
    }

    if (has_in && has_out) {
      _menu->items()[0].show(); // Show "Split" menu item
    } else {
      _menu->items()[0].hide(); // Hide "Split" menu item
    }
  }
}

bool
CanvasModule::show_menu(GdkEventButton* ev)
{
  _menu = std::make_unique<Gtk::Menu>();

  Gtk::Menu::MenuList& items = _menu->items();

  if (_type == SignalDirection::duplex) {
    items.push_back(Gtk::Menu_Helpers::MenuElem(
      T("_Split"), sigc::mem_fun(this, &CanvasModule::on_split)));
    update_menu();
  } else {
    items.push_back(Gtk::Menu_Helpers::MenuElem(
      T("_Join"), sigc::mem_fun(this, &CanvasModule::on_join)));
  }

  items.push_back(Gtk::Menu_Helpers::MenuElem(
    T("_Disconnect"), sigc::mem_fun(this, &CanvasModule::on_disconnect)));

  _menu->popup(ev->button, ev->time);
  return true;
}

bool
CanvasModule::on_event(GdkEvent* ev)
{
  if (ev->type == GDK_BUTTON_PRESS && ev->button.button == 3) {
    return show_menu(&ev->button);
  }
  return false;
}

void
CanvasModule::on_moved(double x, double y)
{
  _action_sink(action::MoveModule{_id, _type, x, y});
}

void
CanvasModule::on_split()
{
  assert(_type == SignalDirection::duplex);
  _action_sink(action::SplitModule{_id});
}

void
CanvasModule::on_join()
{
  assert(_type != SignalDirection::duplex);
  _action_sink(action::UnsplitModule{_id});
}

void
CanvasModule::on_disconnect()
{
  _action_sink(action::DisconnectClient{_id, _type});
}

CanvasPort*
CanvasModule::get_port(const PortID& id)
{
  for (Ganv::Port* p : *this) {
    auto* pport = dynamic_cast<CanvasPort*>(p);
    if (pport && pport->id() == id) {
      return pport;
    }
  }

  return nullptr;
}

} // namespace patchage
