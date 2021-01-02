/* This file is part of Patchage.
 * Copyright 2014-2020 David Robillard <d@drobilla.net>
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

#include "Legend.hpp"

#include "Configuration.hpp"
#include "PortType.hpp"
#include "patchage_config.h"

#include <gdkmm/color.h>
#include <glibmm/signalproxy.h>
#include <gtkmm/box.h>
#include <gtkmm/colorbutton.h>
#include <gtkmm/label.h>
#include <gtkmm/object.h>
#include <sigc++/adaptors/bind.h>
#include <sigc++/functors/mem_fun.h>

#include <string>

namespace patchage {

Legend::Legend(const Configuration& configuration)
{
  add_button(PortType::jack_audio,
             "Audio",
             configuration.get_port_color(PortType::jack_audio));

#ifdef HAVE_JACK_METADATA
  add_button(
    PortType::jack_cv, "CV", configuration.get_port_color(PortType::jack_cv));
  add_button(PortType::jack_osc,
             "OSC",
             configuration.get_port_color(PortType::jack_osc));
#endif

  add_button(PortType::jack_midi,
             "MIDI",
             configuration.get_port_color(PortType::jack_midi));

  add_button(PortType::alsa_midi,
             "ALSA MIDI",
             configuration.get_port_color(PortType::alsa_midi));

  show_all_children();
}

void
Legend::add_button(const PortType id, const std::string& label, uint32_t rgba)
{
  Gdk::Color col;
  col.set_rgb(((rgba >> 24) & 0xFF) * 0x100,
              ((rgba >> 16) & 0xFF) * 0x100,
              ((rgba >> 8) & 0xFF) * 0x100);

  auto* box = new Gtk::HBox();
  auto* but = new Gtk::ColorButton(col);
  but->set_use_alpha(false);
  but->signal_color_set().connect(
    sigc::bind(sigc::mem_fun(this, &Legend::on_color_set), id, label, but));

  box->pack_end(*Gtk::manage(but));
  box->pack_end(*Gtk::manage(new Gtk::Label(label)), false, false, 2);

  this->pack_start(*Gtk::manage(box), false, false, 6);
}

void
Legend::on_color_set(const PortType          id,
                     const std::string&      label,
                     const Gtk::ColorButton* but)
{
  const Gdk::Color col = but->get_color();
  const uint32_t   rgba =
    (((col.get_red() / 0x100) << 24) | ((col.get_green() / 0x100) << 16) |
     ((col.get_blue() / 0x100) << 8) | 0xFF);

  signal_color_changed.emit(id, label, rgba);
}

} // namespace patchage
