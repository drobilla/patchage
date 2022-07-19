// Copyright 2007-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PATCHAGE_HANDLE_EVENT_HPP
#define PATCHAGE_HANDLE_EVENT_HPP

#include "Event.hpp"

namespace patchage {

class Configuration;
class Metadata;
class Canvas;
class ILog;

/// Handle an event from the system by updating the GUI as necessary
void
handle_event(Configuration& conf,
             Metadata&      metadata,
             Canvas&        canvas,
             ILog&          log,
             const Event&   event);

} // namespace patchage

#endif // PATCHAGE_HANDLE_EVENT_HPP
