/* This file is part of Patchage.
 * Copyright 2007-2014 David Robillard <http://drobilla.net>
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

#ifndef PATCHAGE_CONFIGURATION_HPP
#define PATCHAGE_CONFIGURATION_HPP

#include <stdint.h>

#include <string>
#include <list>
#include <map>

#include <boost/optional.hpp>

enum ModuleType { Input, Output, InputOutput };

enum PortType { JACK_AUDIO, JACK_MIDI, ALSA_MIDI, JACK_OSC, JACK_CV };

#define N_PORT_TYPES 5

struct Coord {
	Coord(double x_=0, double y_=0) : x(x_), y(y_) {}
	double x;
	double y;
};

class Configuration
{
public:
	Configuration();

	void load();
	void save();

	bool get_module_location(const std::string& name, ModuleType type, Coord& loc);
	void set_module_location(const std::string& name, ModuleType type, Coord loc);

	void set_module_split(const std::string& name, bool split);
	bool get_module_split(const std::string& name, bool default_val) const;

	float get_zoom() const               { return _zoom; }
	void  set_zoom(float zoom)           { _zoom = zoom; }
	float get_font_size() const          { return _font_size; }
	void  set_font_size(float font_size) { _font_size = font_size; }

	float get_show_toolbar() const             { return _show_toolbar; }
	void  set_show_toolbar(float show_toolbar) { _show_toolbar = show_toolbar; }

	float get_sprung_layout() const              { return _sprung_layout; }
	void  set_sprung_layout(float sprung_layout) { _sprung_layout = sprung_layout; }

	bool  get_show_messages() const             { return _show_messages; }
	void  set_show_messages(bool show_messages) { _show_messages = show_messages; }

	int  get_messages_height() const          { return _messages_height; }
	void set_messages_height(int height) { _messages_height = height; }

	uint32_t get_port_color(PortType type) const { return _port_colors[type]; }
	void     set_port_color(PortType type, uint32_t rgba) {
		_port_colors[type] = rgba;
	}

	Coord get_window_location()          { return _window_location; }
	void  set_window_location(Coord loc) { _window_location = loc; }
	Coord get_window_size()              { return _window_size; }
	void  set_window_size(Coord size)    { _window_size = size; }

private:
	struct ModuleSettings {
		ModuleSettings(bool s=false) : split(s) {}
		boost::optional<Coord> input_location;
		boost::optional<Coord> output_location;
		boost::optional<Coord> inout_location;
		bool                   split;
	};

	std::map<std::string, ModuleSettings> _module_settings;

	uint32_t _default_port_colors[N_PORT_TYPES];
	uint32_t _port_colors[N_PORT_TYPES];

	Coord _window_location;
	Coord _window_size;
	float _zoom;
	float _font_size;
	bool  _show_toolbar;
	bool  _sprung_layout;
	bool  _show_messages;
	int   _messages_height;
};

#endif // PATCHAGE_CONFIGURATION_HPP
