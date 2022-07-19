// Copyright 2007-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PATCHAGE_AUDIODRIVER_HPP
#define PATCHAGE_AUDIODRIVER_HPP

#include "Driver.hpp"

#include <cstdint>
#include <utility>

namespace patchage {

/// Base class for drivers that work with an audio system
class AudioDriver : public Driver
{
public:
  explicit AudioDriver(EventSink emit_event)
    : Driver{std::move(emit_event)}
  {}

  /// Return the number of xruns (dropouts) since the last reset
  virtual uint32_t xruns() = 0;

  /// Reset the xrun count
  virtual void reset_xruns() = 0;

  /// Return the current buffer size in frames
  virtual uint32_t buffer_size() = 0;

  /// Try to set the current buffer size in frames, return true on success
  virtual bool set_buffer_size(uint32_t frames) = 0;

  /// Return the current sample rate in Hz
  virtual uint32_t sample_rate() = 0;
};

} // namespace patchage

#endif // PATCHAGE_AUDIODRIVER_HPP
