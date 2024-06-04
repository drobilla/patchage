// Copyright 2007-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Configuration.hpp"

#include "Coord.hpp"
#include "PortType.hpp"
#include "Setting.hpp"
#include "SignalDirection.hpp"
#include "patchage_config.h"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <utility>
#include <vector>

// IWYU pragma: no_include <algorithm>

namespace patchage {
namespace {

/// Return a vector of filenames in descending order by preference
std::vector<std::string>
get_filenames()
{
  std::vector<std::string> filenames;
  const std::string        prefix;

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
  filenames.emplace_back("patchagerc");

  return filenames;
}

} // namespace

static const char* const port_type_names[Configuration::n_port_types] =
  {"JACK_AUDIO", "JACK_MIDI", "ALSA_MIDI", "JACK_OSC", "JACK_CV"};

Configuration::Configuration(std::function<void(const Setting&)> on_change)
  : _on_change(std::move(on_change))
{
  std::get<setting::FontSize>(_settings).value       = 12.0f;
  std::get<setting::WindowLocation>(_settings).value = Coord{0.0, 0.0};
  std::get<setting::WindowSize>(_settings).value     = Coord{960.0, 540.0};
  std::get<setting::Zoom>(_settings).value           = 1.0f;

#if PATCHAGE_USE_LIGHT_THEME
  _port_colors[static_cast<unsigned>(PortType::jack_audio)] =
    _default_port_colors[static_cast<unsigned>(PortType::jack_audio)] =
      0xA4BC8CFF;

  _port_colors[static_cast<unsigned>(PortType::jack_midi)] =
    _default_port_colors[static_cast<unsigned>(PortType::jack_midi)] =
      0xC89595FF;

  _port_colors[static_cast<unsigned>(PortType::alsa_midi)] =
    _default_port_colors[static_cast<unsigned>(PortType::alsa_midi)] =
      0x8F7198FF;

  _port_colors[static_cast<unsigned>(PortType::jack_osc)] =
    _default_port_colors[static_cast<unsigned>(PortType::jack_osc)] =
      0x7E8EAAFF;

  _port_colors[static_cast<unsigned>(PortType::jack_cv)] =
    _default_port_colors[static_cast<unsigned>(PortType::jack_cv)] = 0x83AFABFF;
#else
  _port_colors[static_cast<unsigned>(PortType::jack_audio)] =
    _default_port_colors[static_cast<unsigned>(PortType::jack_audio)] =
      0x3E5E00FF;

  _port_colors[static_cast<unsigned>(PortType::jack_midi)] =
    _default_port_colors[static_cast<unsigned>(PortType::jack_midi)] =
      0x650300FF;

  _port_colors[static_cast<unsigned>(PortType::alsa_midi)] =
    _default_port_colors[static_cast<unsigned>(PortType::alsa_midi)] =
      0x2D0043FF;

  _port_colors[static_cast<unsigned>(PortType::jack_osc)] =
    _default_port_colors[static_cast<unsigned>(PortType::jack_osc)] =
      0x4100FEFF;

  _port_colors[static_cast<unsigned>(PortType::jack_cv)] =
    _default_port_colors[static_cast<unsigned>(PortType::jack_cv)] = 0x005E4EFF;
#endif
}

bool
Configuration::get_module_location(const std::string& name,
                                   SignalDirection    type,
                                   Coord&             loc) const
{
  auto i = _module_settings.find(name);
  if (i == _module_settings.end()) {
    return false;
  }

  const ModuleSettings& settings = (*i).second;
  if (type == SignalDirection::input && settings.input_location) {
    loc = *settings.input_location;
  } else if (type == SignalDirection::output && settings.output_location) {
    loc = *settings.output_location;
  } else if (type == SignalDirection::duplex && settings.inout_location) {
    loc = *settings.inout_location;
  } else {
    return false;
  }

  return true;
}

void
Configuration::set_module_location(const std::string& name,
                                   SignalDirection    type,
                                   Coord              loc)
{
  if (name.empty()) {
    return;
  }

  auto i = _module_settings.find(name);
  if (i == _module_settings.end()) {
    i = _module_settings
          .insert(std::make_pair(
            name, ModuleSettings(type != SignalDirection::duplex)))
          .first;
  }

  ModuleSettings& settings = (*i).second;
  switch (type) {
  case SignalDirection::input:
    settings.input_location = loc;
    break;
  case SignalDirection::output:
    settings.output_location = loc;
    break;
  case SignalDirection::duplex:
    settings.inout_location = loc;
    break;
  }
}

/** Returns whether or not this module should be split.
 *
 * If nothing is known about the given module, `default_val` is returned (this
 * is to allow driver's to request terminal ports get split by default).
 */
bool
Configuration::get_module_split(const std::string& name, bool default_val) const
{
  auto i = _module_settings.find(name);
  if (i == _module_settings.end()) {
    return default_val;
  }

  return (*i).second.split;
}

void
Configuration::set_module_split(const std::string& name, bool split)
{
  if (!name.empty()) {
    _module_settings[name].split = split;
  }
}

void
Configuration::load()
{
  // Try to find a readable configuration file
  const std::vector<std::string> filenames = get_filenames();
  std::ifstream                  file;
  for (const auto& filename : filenames) {
    file.open(filename.c_str(), std::ios::in);
    if (file.good()) {
      std::cout << "Loading configuration from " << filename << "\n";
      break;
    }
  }

  if (!file.good()) {
    std::cout << "No configuration file present\n";
    return;
  }

  _module_settings.clear();
  while (file.good()) {
    std::string key;
    if (file.peek() == '\"') {
      /* Old versions omitted the module_position key and listed
         positions starting with module name in quotes. */
      key = "module_position";
    } else {
      file >> key;
    }

    if (key == "window_location") {
      auto& setting = std::get<setting::WindowLocation>(_settings);
      file >> setting.value.x >> setting.value.y;
    } else if (key == "window_size") {
      auto& setting = std::get<setting::WindowSize>(_settings);
      file >> setting.value.x >> setting.value.y;
    } else if (key == "zoom_level") {
      file >> std::get<setting::Zoom>(_settings).value;
    } else if (key == "font_size") {
      file >> std::get<setting::FontSize>(_settings).value;
    } else if (key == "show_toolbar") {
      file >> std::get<setting::ToolbarVisible>(_settings).value;
    } else if (key == "sprung_layout") {
      file >> std::get<setting::SprungLayout>(_settings).value;
    } else if (key == "show_messages") {
      file >> std::get<setting::MessagesVisible>(_settings).value;
    } else if (key == "sort_ports") {
      file >> std::get<setting::SortedPorts>(_settings).value;
    } else if (key == "messages_height") {
      file >> std::get<setting::MessagesHeight>(_settings).value;
    } else if (key == "human_names") {
      file >> std::get<setting::HumanNames>(_settings).value;
    } else if (key == "port_color") {
      std::string type_name;
      uint32_t    rgba = 0u;
      file >> type_name;
      file.ignore(1, '#');
      file >> std::hex >> std::uppercase;
      file >> rgba;
      file >> std::dec >> std::nouppercase;

      bool found = false;
      for (unsigned i = 0U; i < n_port_types; ++i) {
        if (type_name == port_type_names[i]) {
          _port_colors[i] = rgba;
          found           = true;
          break;
        }
      }
      if (!found) {
        std::cerr << "error: color for unknown port type `" << type_name << "'\n";
      }
    } else if (key == "module_position") {
      Coord       loc;
      std::string name;
      file.ignore(std::numeric_limits<std::streamsize>::max(), '\"');
      std::getline(file, name, '\"');

      SignalDirection type = SignalDirection::input;
      std::string     type_str;
      file >> type_str;
      if (type_str == "input") {
        type = SignalDirection::input;
      } else if (type_str == "output") {
        type = SignalDirection::output;
      } else if (type_str == "inputoutput") {
        type = SignalDirection::duplex;
      } else {
        std::cerr << "error: bad position type `" << type_str
                  << "' for module `" << name << "'\n";
        file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        continue;
      }

      file >> loc.x;
      file >> loc.y;

      set_module_location(name, type, loc);
    } else {
      std::cerr << "warning: unknown configuration key `" << key << "'\n";
      file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }

    // Skip trailing whitespace, including newline
    while (file.good() && isspace(file.peek())) {
      file.ignore(1);
    }
  }

  file.close();
}

inline void
write_module_position(std::ofstream&     os,
                      const std::string& name,
                      const char*        type,
                      const Coord&       loc)
{
  os << "module_position \"" << name << "\""
     << " " << type << " " << loc.x << " " << loc.y << "\n";
}

void
Configuration::save()
{
  // Try to find a writable configuration file
  const std::vector<std::string> filenames = get_filenames();
  std::ofstream                  file;
  for (const std::string& filename : filenames) {
    file.open(filename.c_str(), std::ios::out);
    if (file.good()) {
      std::cout << "Writing configuration to " << filename << "\n";
      break;
    }
  }

  if (!file.good()) {
    std::cout << "Unable to open configuration file to write\n";
    return;
  }

  file << "window_location " << get<setting::WindowLocation>().x << " "
       << get<setting::WindowLocation>().y << "\n";

  file << "window_size " << get<setting::WindowSize>().x << " "
       << get<setting::WindowSize>().y << "\n";

  file << "zoom_level " << get<setting::Zoom>() << "\n";
  file << "font_size " << get<setting::FontSize>() << "\n";
  file << "show_toolbar " << get<setting::ToolbarVisible>() << "\n";
  file << "sprung_layout " << get<setting::SprungLayout>() << "\n";
  file << "show_messages " << get<setting::MessagesVisible>() << "\n";
  file << "sort_ports " << get<setting::SortedPorts>() << "\n";
  file << "messages_height " << get<setting::MessagesHeight>() << "\n";
  file << "human_names " << get<setting::HumanNames>() << "\n";

  file << std::hex << std::uppercase;
  for (unsigned i = 0U; i < n_port_types; ++i) {
    if (_port_colors[i] != _default_port_colors[i]) {
      file << "port_color " << port_type_names[i] << " " << _port_colors[i]
           << "\n";
    }
  }
  file << std::dec << std::nouppercase;

  for (const auto& s : _module_settings) {
    const std::string&    name     = s.first;
    const ModuleSettings& settings = s.second;

    if (settings.split) {
      if (settings.input_location) {
        write_module_position(file, name, "input", *settings.input_location);
      }

      if (settings.output_location) {
        write_module_position(file, name, "output", *settings.output_location);
      }
    } else if (settings.inout_location) {
      write_module_position(
        file, name, "inputoutput", *settings.inout_location);
    }
  }

  file.close();
}

} // namespace patchage
