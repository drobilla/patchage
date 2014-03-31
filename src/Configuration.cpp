/* This file is part of Patchage.
 * Copyright 2007-2013 David Robillard <http://drobilla.net>
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

#include <ctype.h>
#include <stdlib.h>

#include <fstream>
#include <ios>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

#include "Configuration.hpp"
#include "Patchage.hpp"

static const char* port_type_names[N_PORT_TYPES] = {
	"JACK_AUDIO",
	"JACK_MIDI",
	"ALSA_MIDI"
};

Configuration::Configuration()
	: _window_location(0, 0)
	, _window_size(640, 480)
	, _zoom(1.0)
{
	_port_colors[JACK_AUDIO] = _default_port_colors[JACK_AUDIO] = 0x244678FF;
	_port_colors[JACK_MIDI]  = _default_port_colors[JACK_MIDI] = 0x960909FF;
	_port_colors[ALSA_MIDI]  = _default_port_colors[ALSA_MIDI] = 0x4A8A0EFF;
}

bool
Configuration::get_module_location(const std::string& name, ModuleType type, Coord& loc)
{
	std::map<std::string, ModuleSettings>::const_iterator i = _module_settings.find(name);
	if (i == _module_settings.end()) {
		return false;
	}

	const ModuleSettings& settings = (*i).second;
	if (type == Input && settings.input_location) {
		loc = *settings.input_location;
	} else if (type == Output && settings.output_location) {
		loc = *settings.output_location;
	} else if (type == InputOutput && settings.inout_location) {
		loc = *settings.inout_location;
	} else {
		return false;
	}

	return true;
}

void
Configuration::set_module_location(const std::string& name, ModuleType type, Coord loc)
{
	std::map<std::string, ModuleSettings>::iterator i = _module_settings.find(name);
	if (i == _module_settings.end()) {
		i = _module_settings.insert(
			std::make_pair(name, ModuleSettings(type != InputOutput))).first;
	}

	ModuleSettings& settings = (*i).second;
	switch (type) {
	case Input:
		settings.input_location = loc;
		break;
	case Output:
		settings.output_location = loc;
		break;
	case InputOutput:
		settings.inout_location = loc;
		break;
	default:
		break;  // shouldn't reach here
	}
}

/** Returns whether or not this module should be split.
 *
 * If nothing is known about the given module, @a default_val is returned (this is
 * to allow driver's to request terminal ports get split by default).
 */
bool
Configuration::get_module_split(const std::string& name, bool default_val) const
{
	std::map<std::string, ModuleSettings>::const_iterator i = _module_settings.find(name);
	if (i == _module_settings.end()) {
		return default_val;
	}

	return (*i).second.split;
}

void
Configuration::set_module_split(const std::string& name, bool split)
{
	_module_settings[name].split = split;
}

/** Return a vector of filenames in descending order by preference. */
static std::vector<std::string>
get_filenames()
{
	std::vector<std::string> filenames;
	std::string prefix;

	const char* xdg_config_home = getenv("XDG_CONFIG_HOME");
	const char* home            = getenv("HOME");

	// XDG spec
	if (xdg_config_home) {
		filenames.push_back(std::string(xdg_config_home) + "/patchagerc");
	} else if (home) {
		filenames.push_back(std::string(home) + "/.config/patchagerc");
	}

	// Old location
	if (home) {
		filenames.push_back(std::string(home) + "/.patchagerc");
	}

	// Current directory (bundle or last-ditch effort)
	filenames.push_back("patchagerc");

	return filenames;
}

void
Configuration::load()
{
	// Try to find a readable configuration file
	const std::vector<std::string> filenames = get_filenames();
	std::ifstream                  file;
	for (size_t i = 0; i < filenames.size(); ++i) {
		file.open(filenames[i].c_str(), std::ios::in);
		if (file.good()) {
			std::cout << "Loading configuration from " << filenames[i] << std::endl;
			break;
		}
	}

	if (!file.good()) {
		std::cout << "No configuration file present" << std::endl;
		return;
	}

	_module_settings.clear();
	while (file.good()) {
		std::string key;
		if (file.peek() == '\"') {
			/* Old versions ommitted the module_position key and listed
			   positions starting with module name in quotes. */
			key = "module_position";
		} else {
			file >> key;
		}

		if (key == "window_location") {
			file >> _window_location.x >> _window_location.y;
		} else if (key == "window_size") {
			file >> _window_size.x >> _window_size.y;
		} else if (key == "zoom_level") {
			file >> _zoom;
		} else if (key == "port_color") {
			std::string type_name;
			uint32_t    rgba;
			file >> type_name;
			file.ignore(1, '#');
			file >> std::hex >> std::uppercase;
			file >> rgba;
			file >> std::dec >> std::nouppercase;

			bool found = false;
			for (int i = 0; i < N_PORT_TYPES; ++i) {
				if (type_name == port_type_names[i]) {
					_port_colors[i] = rgba;
					found           = true;
					break;
				}
			}
			if (!found) {
				std::cerr << "error: color for unknown port type `"
				          << type_name << "'" << std::endl;
			}
		} else if (key == "module_position" || key[0] == '\"') {

			Coord       loc;
			std::string name;
			file.ignore(1, '\"');
			std::getline(file, name, '\"');

			ModuleType  type;
			std::string type_str;
			file >> type_str;
			if (type_str == "input") {
				type = Input;
			} else if (type_str == "output") {
				type = Output;
			} else if (type_str == "inputoutput") {
				type = InputOutput;
			} else {
				std::cerr << "error: bad position type `" << type_str
				          << "' for module `" << name << "'" << std::endl;
				file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
				continue;
			}

			file >> loc.x;
			file >> loc.y;

			set_module_location(name, type, loc);
		} else {
			std::cerr << "warning: unknown configuration key `" << key << "'"
			          << std::endl;
			file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
		}

		// Skip trailing whitespace, including newline
		while (file.good() && isspace(file.peek())) {
			file.ignore(1);
		}
	}

	file.close();
}

static inline void
write_module_position(std::ofstream&     os,
                      const std::string& name,
                      const char*        type,
                      const Coord&       loc)
{
	os << "module_position \"" << name << "\""
	   << " " << type << " " << loc.x << " " << loc.y << std::endl;
}

void
Configuration::save()
{
	// Try to find a writable configuration file
	const std::vector<std::string> filenames = get_filenames();
	std::ofstream                  file;
	for (size_t i = 0; i < filenames.size(); ++i) {
		file.open(filenames[i].c_str(), std::ios::out);
		if (file.good()) {
			std::cout << "Writing configuration to " << filenames[i] << std::endl;
			break;
		}
	}

	if (!file.good()) {
		std::cout << "Unable to open configuration file to write" << std::endl;
		return;
	}

	file << "window_location " << _window_location.x << " " << _window_location.y << std::endl;
	file << "window_size " << _window_size.x << " " << _window_size.y << std::endl;
	file << "zoom_level " << _zoom << std::endl;

	file << std::hex << std::uppercase;
	for (int i = 0; i < N_PORT_TYPES; ++i) {
		if (_port_colors[i] != _default_port_colors[i]) {
			file << "port_color " << port_type_names[i] << " " << _port_colors[i] << std::endl;
		}
	}
	file << std::dec << std::nouppercase;
		
	for (std::map<std::string, ModuleSettings>::iterator i = _module_settings.begin();
	     i != _module_settings.end(); ++i) {
		const ModuleSettings& settings = (*i).second;
		const std::string&    name     = (*i).first;

		if (settings.split) {
			if (settings.input_location && settings.output_location) {
				write_module_position(file, name, "input", *settings.input_location);
				write_module_position(file, name, "output", *settings.output_location);
			}
		} else if (settings.inout_location) {
			write_module_position(file, name, "inputoutput", *settings.inout_location);
		}
	}

	file.close();
}
