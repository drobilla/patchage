/* This file is part of Patchage.
 * Copyright 2014 David Robillard <http://drobilla.net>
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

#ifndef PATCHAGE_LEGEND_HPP
#define PATCHAGE_LEGEND_HPP

#include "Configuration.hpp"

#include <gtkmm/box.h>
#include <gtkmm/colorbutton.h>

class Legend : public Gtk::HBox {
public:
	Legend(const Configuration& configuration) {
		add_button(JACK_AUDIO, "Audio",     configuration.get_port_color(JACK_AUDIO));
#ifdef HAVE_JACK_METADATA
		add_button(JACK_CV,    "CV",        configuration.get_port_color(JACK_CV));
		add_button(JACK_OSC,   "OSC",       configuration.get_port_color(JACK_OSC));
#endif
		add_button(JACK_MIDI,  "MIDI",      configuration.get_port_color(JACK_MIDI));
		add_button(ALSA_MIDI,  "ALSA MIDI", configuration.get_port_color(ALSA_MIDI));
		show_all_children();
	}

	void add_button(int id, const std::string& label, uint32_t rgba) {
		Gdk::Color col;
		col.set_rgb(((rgba >> 24) & 0xFF) * 0x100,
		            ((rgba>> 16)  & 0xFF) * 0x100,
		            ((rgba >> 8)  & 0xFF) * 0x100);
		Gtk::HBox*        box = new Gtk::HBox();
		Gtk::ColorButton* but = new Gtk::ColorButton(col);
		but->set_use_alpha(false);
		but->signal_color_set().connect(
			sigc::bind(sigc::mem_fun(this, &Legend::on_color_set),
			           id, label, but));

		box->pack_end(*Gtk::manage(but));
		box->pack_end(*Gtk::manage(new Gtk::Label(label)), false, false, 2);

		this->pack_start(*Gtk::manage(box), false, false, 6);
	}

	void on_color_set(const int               id,
	                  const std::string&      label,
	                  const Gtk::ColorButton* but) {
		const Gdk::Color col  = but->get_color();
		const uint32_t   rgba = (((col.get_red() / 0x100) << 24) |
		                         ((col.get_green() / 0x100) << 16) |
		                         ((col.get_blue() / 0x100) << 8) |
		                         0xFF);

		signal_color_changed.emit(id, label, rgba);
	}

	sigc::signal<void, int, std::string, uint32_t> signal_color_changed;
};

#endif // PATCHAGE_LEGEND_HPP
