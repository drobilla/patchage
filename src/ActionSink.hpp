// Copyright 2007-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PATCHAGE_ACTION_SINK_HPP
#define PATCHAGE_ACTION_SINK_HPP

#include "Action.hpp"

#include <functional>

namespace patchage {

/// Sink function for user actions
using ActionSink = std::function<void(Action)>;

} // namespace patchage

#endif // PATCHAGE_ACTION_SINK_HPP
