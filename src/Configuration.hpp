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

#ifndef PATCHAGE_CONFIGURATION_HPP
#define PATCHAGE_CONFIGURATION_HPP

#include "Coord.hpp"
#include "PortType.hpp"
#include "Setting.hpp"
#include "SignalDirection.hpp"

#include <boost/optional/optional.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <tuple>

#define N_PORT_TYPES 5

namespace patchage {

class Configuration
{
public:
  explicit Configuration(std::function<void(const Setting&)> on_change);

  void load();
  void save();

  bool get_module_location(const std::string& name,
                           SignalDirection    type,
                           Coord&             loc) const;

  void set_module_location(const std::string& name,
                           SignalDirection    type,
                           Coord              loc);

  void set_module_split(const std::string& name, bool split);
  bool get_module_split(const std::string& name, bool default_val) const;

  uint32_t get_port_color(PortType type) const
  {
    return _port_colors[static_cast<unsigned>(type)];
  }

  void set_port_color(PortType type, uint32_t rgba)
  {
    _port_colors[static_cast<unsigned>(type)] = rgba;
    _on_change(setting::PortColor{type, rgba});
  }

  // Set a global configuration setting
  template<class S>
  void set(typename S::Value value)
  {
    S& setting = std::get<S>(_settings);

    if (setting.value != value) {
      setting.value = std::move(value);
      _on_change(setting);
    }
  }

  // Get a global configuration setting
  template<class S>
  typename S::Value get() const
  {
    return std::get<S>(_settings).value;
  }

  /// Call `visitor` once with each configuration setting
  template<class Visitor>
  void each(Visitor visitor)
  {
    visitor(std::get<setting::FontSize>(_settings));
    visitor(std::get<setting::HumanNames>(_settings));
    visitor(std::get<setting::MessagesHeight>(_settings));
    visitor(std::get<setting::MessagesVisible>(_settings));
    visitor(std::get<setting::SortedPorts>(_settings));
    visitor(std::get<setting::SprungLayout>(_settings));
    visitor(std::get<setting::ToolbarVisible>(_settings));
    visitor(std::get<setting::WindowLocation>(_settings));
    visitor(std::get<setting::WindowSize>(_settings));
    visitor(std::get<setting::Zoom>(_settings));

    for (auto i = 0u; i < N_PORT_TYPES; ++i) {
      visitor(setting::PortColor{static_cast<PortType>(i), _port_colors[i]});
    }
  }

private:
  struct ModuleSettings {
    explicit ModuleSettings(bool s = false)
      : split(s)
    {}

    boost::optional<Coord> input_location;
    boost::optional<Coord> output_location;
    boost::optional<Coord> inout_location;
    bool                   split;
  };

  std::map<std::string, ModuleSettings> _module_settings;

  uint32_t _default_port_colors[N_PORT_TYPES] = {};
  uint32_t _port_colors[N_PORT_TYPES]         = {};

  using Settings = std::tuple<setting::AlsaAttached,
                              setting::FontSize,
                              setting::HumanNames,
                              setting::JackAttached,
                              setting::MessagesHeight,
                              setting::MessagesVisible,
                              setting::SortedPorts,
                              setting::SprungLayout,
                              setting::ToolbarVisible,
                              setting::WindowLocation,
                              setting::WindowSize,
                              setting::Zoom>;

  Settings _settings;

  std::function<void(const Setting&)> _on_change;
};

} // namespace patchage

#endif // PATCHAGE_CONFIGURATION_HPP
