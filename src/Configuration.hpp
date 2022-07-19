// Copyright 2007-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: GPL-3.0-or-later

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
  void set(decltype(S::value) value)
  {
    S& setting = std::get<S>(_settings);

    if (setting.value != value) {
      setting.value = std::move(value);
      _on_change(setting);
    }
  }

  // Set a global configuration setting
  template<class S>
  void set_setting(S new_setting)
  {
    set<S>(new_setting.value);
  }

  // Set a global port color setting
  void set_setting(setting::PortColor new_setting)
  {
    auto& color = _port_colors[static_cast<unsigned>(new_setting.type)];

    if (color != new_setting.color) {
      set_port_color(new_setting.type, new_setting.color);
    }
  }

  // Get a global configuration setting
  template<class S>
  const decltype(S::value) get() const
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
