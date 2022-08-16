// Copyright 2007-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PATCHAGE_CANVASPORT_HPP
#define PATCHAGE_CANVASPORT_HPP

#include "PortID.hpp"
#include "PortType.hpp"
#include "warnings.hpp"

PATCHAGE_DISABLE_GANV_WARNINGS
#include "ganv/Port.hpp"
PATCHAGE_RESTORE_WARNINGS

#include <boost/optional/optional.hpp>
#include <gdk/gdk.h>
#include <gtkmm/menu.h>
#include <gtkmm/menu_elems.h>
#include <gtkmm/menushell.h>
#include <gtkmm/object.h>
#include <sigc++/functors/mem_fun.h>
#include <sigc++/signal.h>

#include <cstdint>
#include <string>
#include <utility>

namespace Ganv {
class Module;
} // namespace Ganv

namespace patchage {

/// A port on a CanvasModule
class CanvasPort : public Ganv::Port
{
public:
  CanvasPort(Ganv::Module&        module,
             PortType             type,
             PortID               id,
             const std::string&   name,
             const std::string&   human_name,
             bool                 is_input,
             uint32_t             color,
             bool                 show_human_name,
             boost::optional<int> order = boost::optional<int>())
    : Port(module,
           (show_human_name && !human_name.empty()) ? human_name : name,
           is_input,
           color)
    , _type(type)
    , _id(std::move(id))
    , _name(name)
    , _human_name(human_name)
    , _order(order)
  {
    signal_event().connect(sigc::mem_fun(this, &CanvasPort::on_event));
  }

  CanvasPort(const CanvasPort&)            = delete;
  CanvasPort& operator=(const CanvasPort&) = delete;

  CanvasPort(CanvasPort&&)            = delete;
  CanvasPort& operator=(CanvasPort&&) = delete;

  ~CanvasPort() override = default;

  void show_human_name(bool human)
  {
    if (human && !_human_name.empty()) {
      set_label(_human_name.c_str());
    } else {
      set_label(_name.c_str());
    }
  }

  bool on_event(GdkEvent* ev) override
  {
    if (ev->type != GDK_BUTTON_PRESS || ev->button.button != 3) {
      return false;
    }

    Gtk::Menu* menu = Gtk::manage(new Gtk::Menu());
    menu->items().push_back(Gtk::Menu_Helpers::MenuElem(
      "Disconnect", sigc::mem_fun(this, &Port::disconnect)));

    menu->popup(ev->button.button, ev->button.time);
    return true;
  }

  PortType                    type() const { return _type; }
  PortID                      id() const { return _id; }
  const std::string&          name() const { return _name; }
  const std::string&          human_name() const { return _human_name; }
  const boost::optional<int>& order() const { return _order; }

private:
  PortType             _type;
  PortID               _id;
  std::string          _name;
  std::string          _human_name;
  boost::optional<int> _order;
};

} // namespace patchage

#endif // PATCHAGE_CANVASPORT_HPP
