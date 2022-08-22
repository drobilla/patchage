// Copyright 2007-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PATCHAGE_SETTING_HPP
#define PATCHAGE_SETTING_HPP

#include "Coord.hpp"
#include "PortType.hpp"

#include <cstdint>
#include <variant>

namespace patchage {
namespace setting {

struct AlsaAttached {
  bool value{};
};

struct FontSize {
  float value{};
};

struct HumanNames {
  bool value{};
};

struct JackAttached {
  bool value{};
};

struct MessagesHeight {
  int value{};
};

struct MessagesVisible {
  bool value{};
};

struct PortColor {
  PortType type{};
  uint32_t color{};
};

struct SortedPorts {
  bool value{};
};

struct SprungLayout {
  bool value{};
};

struct ToolbarVisible {
  bool value{};
};

struct WindowLocation {
  Coord value{};
};

struct WindowSize {
  Coord value{};
};

struct Zoom {
  float value{};
};

} // namespace setting

/// A configuration setting
using Setting = std::variant<setting::AlsaAttached,
                             setting::FontSize,
                             setting::HumanNames,
                             setting::JackAttached,
                             setting::MessagesHeight,
                             setting::MessagesVisible,
                             setting::PortColor,
                             setting::SortedPorts,
                             setting::SprungLayout,
                             setting::ToolbarVisible,
                             setting::WindowLocation,
                             setting::WindowSize,
                             setting::Zoom>;

} // namespace patchage

#endif // PATCHAGE_SETTING_HPP
