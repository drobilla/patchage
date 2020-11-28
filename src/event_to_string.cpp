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

#include "event_to_string.hpp"

#include "PatchageEvent.hpp"
#include "warnings.hpp"

PATCHAGE_DISABLE_FMT_WARNINGS
#include <fmt/core.h>
#include <fmt/ostream.h>
PATCHAGE_RESTORE_WARNINGS

#include <cstdint>
#include <iostream>
#include <string>

namespace {

struct EventPrinter
{
	using result_type = std::string; ///< For boost::apply_visitor

	std::string operator()(const ClientCreationEvent& event)
	{
		return fmt::format(
		    "Add client \"{}\" (\"{}\")", event.id, event.info.label);
	}

	std::string operator()(const ClientDestructionEvent& event)
	{
		return fmt::format("Remove client \"{}\"", event.id);
	}

	std::string operator()(const PortType port_type)
	{
		switch (port_type) {
		case PortType::jack_audio:
			return "JACK audio";
		case PortType::jack_midi:
			return "JACK MIDI";
		case PortType::alsa_midi:
			return "ALSA MIDI";
		case PortType::jack_osc:
			return "JACK OSC";
		case PortType::jack_cv:
			return "JACK CV";
		}
	}

	std::string operator()(const SignalDirection direction)
	{
		switch (direction) {
		case SignalDirection::input:
			return "input";
		case SignalDirection::output:
			return "output";
		case SignalDirection::duplex:
			return "duplex";
		}
	}

	std::string operator()(const PortCreationEvent& event)
	{
		auto result = fmt::format("Add{} {} {} port \"{}\" (\"{}\")",
		                          event.info.is_terminal ? " terminal" : "",
		                          (*this)(event.info.type),
		                          (*this)(event.info.direction),
		                          event.id,
		                          event.info.label);

		if (event.info.order.has_value()) {
			result += fmt::format(" order: {}", *event.info.order);
		}

		return result;
	}

	std::string operator()(const PortDestructionEvent& event)
	{
		return fmt::format("Remove port \"{}\"", event.id);
	}

	std::string operator()(const ConnectionEvent& event)
	{
		return fmt::format("Connect \"{}\" to \"{}\"", event.tail, event.head);
	}

	std::string operator()(const DisconnectionEvent& event)
	{
		return fmt::format(
		    "Disconnect \"{}\" from \"{}\"", event.tail, event.head);
	}
};

} // namespace

std::string
event_to_string(const PatchageEvent& event)
{
	EventPrinter printer;
	return boost::apply_visitor(printer, event);
}

std::ostream&
operator<<(std::ostream& os, const PatchageEvent& event)
{
	return os << event_to_string(event);
}
