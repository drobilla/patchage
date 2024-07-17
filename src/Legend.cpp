// Copyright 2014-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Legend.hpp"

#include "Configuration.hpp"
#include "PortType.hpp"
#include "i18n.hpp"
#include "patchage_config.h"

#include <gdkmm/color.h>
#include <glibmm/signalproxy.h>
#include <glibmm/ustring.h>
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
             T("Audio"),
             configuration.get_port_color(PortType::jack_audio));

#if USE_JACK_METADATA
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
  col.set_rgb(((rgba >> 24U) & 0xFFU) * 0x100U,
              ((rgba >> 16U) & 0xFFU) * 0x100U,
              ((rgba >> 8U) & 0xFFU) * 0x100U);

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
    (((col.get_red() / 0x100U) << 24U) | ((col.get_green() / 0x100U) << 16U) |
     ((col.get_blue() / 0x100U) << 8U) | 0xFFU);

  signal_color_changed.emit(id, label, rgba);
}

} // namespace patchage
