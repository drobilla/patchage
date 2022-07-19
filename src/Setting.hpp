// Copyright 2007-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PATCHAGE_SETTING_HPP
#define PATCHAGE_SETTING_HPP

#include "Coord.hpp"
#include "PortType.hpp"

#include <boost/variant/variant.hpp>

#include <cstdint>

namespace patchage {
namespace setting {

template<class T>
struct Setting {
  using Value = T;

  Value value{};
};

struct PortColor {
  PortType type{};
  uint32_t value{};
};

struct AlsaAttached : Setting<bool> {};
struct FontSize : Setting<float> {};
struct HumanNames : Setting<bool> {};
struct JackAttached : Setting<bool> {};
struct MessagesHeight : Setting<int> {};
struct MessagesVisible : Setting<bool> {};
struct SortedPorts : Setting<bool> {};
struct SprungLayout : Setting<bool> {};
struct ToolbarVisible : Setting<bool> {};
struct WindowLocation : Setting<Coord> {};
struct WindowSize : Setting<Coord> {};
struct Zoom : Setting<float> {};

} // namespace setting

/// A configuration setting
using Setting = boost::variant<setting::AlsaAttached,
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
