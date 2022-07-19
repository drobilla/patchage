// Copyright 2007-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PATCHAGE_COORD_HPP
#define PATCHAGE_COORD_HPP

namespace patchage {

struct Coord {
  double x{0.0};
  double y{0.0};
};

inline bool
operator==(const Coord& lhs, const Coord& rhs)
{
  return lhs.x == rhs.x && lhs.y == rhs.y;
}

inline bool
operator!=(const Coord& lhs, const Coord& rhs)
{
  return !(lhs == rhs);
}

} // namespace patchage

#endif // PATCHAGE_COORD_HPP
