/* This file is part of Patchage.
 * Copyright 2007-2021 David Robillard <d@drobilla.net>
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

#ifndef PATCHAGE_SETTING_HPP
#define PATCHAGE_SETTING_HPP

#include "ClientID.hpp"
#include "Coord.hpp"
#include "PortID.hpp"
#include "PortType.hpp"
#include "SignalDirection.hpp"

#include <boost/variant/variant.hpp>

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
