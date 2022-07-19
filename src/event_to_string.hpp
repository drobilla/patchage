// Copyright 2007-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PATCHAGE_EVENT_TO_STRING_HPP
#define PATCHAGE_EVENT_TO_STRING_HPP

#include "Event.hpp"

#include <iosfwd>
#include <string>

namespace patchage {

std::string
event_to_string(const Event& event);

std::ostream&
operator<<(std::ostream& os, const Event& event);

} // namespace patchage

#endif // PATCHAGE_EVENT_TO_STRING_HPP
