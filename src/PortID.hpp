/* This file is part of Patchage.
 * Copyright 2008-2020 David Robillard <d@drobilla.net>
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

#ifndef PATCHAGE_PORTID_HPP
#define PATCHAGE_PORTID_HPP

#include <cassert>
#include <iostream>
#include <string>
#include <tuple>
#include <utility>

/// An ID for some port on a client (program)
struct PortID
{
	enum class Type
	{
		nothing,
		jack,
		alsa,
	};

	PortID(const PortID& copy) = default;
	PortID& operator=(const PortID& copy) = default;

	PortID(PortID&& id) = default;
	PortID& operator=(PortID&& id) = default;

	~PortID() = default;

	/// Return a null ID that refers to nothing
	static PortID nothing() { return PortID{}; }

	/// Return an ID for a JACK port by full name (like "client:port")
	static PortID jack(std::string name)
	{
		return PortID{Type::jack, std::move(name)};
	}

	/// Return an ID for an ALSA Sequencer port by ID
	static PortID
	alsa(const uint8_t client_id, const uint8_t port, const bool is_input)
	{
		return PortID{Type::alsa, client_id, port, is_input};
	}

	Type               type() const { return _type; }
	const std::string& jack_name() const { return _jack_name; }
	uint8_t            alsa_client() const { return _alsa_client; }
	uint8_t            alsa_port() const { return _alsa_port; }
	bool               alsa_is_input() const { return _alsa_is_input; }

private:
	PortID() = default;

	PortID(const Type type, std::string jack_name)
	    : _type{type}
	    , _jack_name{std::move(jack_name)}
	{
		assert(_type == Type::jack);
		assert(_jack_name.find(':') != std::string::npos);
	}

	PortID(const Type    type,
	       const uint8_t alsa_client,
	       const uint8_t alsa_port,
	       const bool    is_input)
	    : _type{type}
	    , _alsa_client{alsa_client}
	    , _alsa_port{alsa_port}
	    , _alsa_is_input{is_input}
	{
		assert(_type == Type::alsa);
	}

	Type        _type{Type::nothing}; ///< Determines which field is active
	std::string _jack_name{};         ///< Full port name for Type::jack
	uint8_t     _alsa_client{};       ///< Client ID for Type::alsa
	uint8_t     _alsa_port{};         ///< Port ID for Type::alsa
	bool        _alsa_is_input{};     ///< Input flag for Type::alsa
};

static inline std::ostream&
operator<<(std::ostream& os, const PortID& id)
{
	switch (id.type()) {
	case PortID::Type::nothing:
		return os << "(null)";
	case PortID::Type::jack:
		return os << "jack:" << id.jack_name();
	case PortID::Type::alsa:
		return os << "alsa:" << int(id.alsa_client()) << ":"
		          << int(id.alsa_port()) << ":"
		          << (id.alsa_is_input() ? "in" : "out");
	}

	assert(false);
	return os;
}

static inline bool
operator<(const PortID& lhs, const PortID& rhs)
{
	if (lhs.type() != rhs.type()) {
		return lhs.type() < rhs.type();
	}

	switch (lhs.type()) {
	case PortID::Type::nothing:
		return true;
	case PortID::Type::jack:
		return lhs.jack_name() < rhs.jack_name();
	case PortID::Type::alsa:
		return std::make_tuple(
		           lhs.alsa_client(), lhs.alsa_port(), lhs.alsa_is_input()) <
		       std::make_tuple(
		           rhs.alsa_client(), rhs.alsa_port(), rhs.alsa_is_input());
	}

	assert(false);
	return false;
}

namespace std {

template<>
struct hash<PortID::Type>
{
	size_t operator()(const PortID::Type& v) const noexcept
	{
		return hash<unsigned>()(static_cast<unsigned>(v));
	}
};

} // namespace std

#endif // PATCHAGE_PORTID_HPP
