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

#ifndef PATCHAGE_AUDIODRIVER_HPP
#define PATCHAGE_AUDIODRIVER_HPP

#include "Driver.hpp"

#include <cstdint>

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

#endif // PATCHAGE_AUDIODRIVER_HPP
