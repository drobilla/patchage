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

#ifndef PATCHAGE_ILOG_HPP
#define PATCHAGE_ILOG_HPP

#include <string>

namespace patchage {

/// Interface for writing log messages
class ILog
{
public:
  ILog() = default;

  ILog(const ILog&) = default;
  ILog& operator=(const ILog&) = default;

  ILog(ILog&&)  = default;
  ILog& operator=(ILog&&) = default;

  virtual ~ILog() = default;

  virtual void info(const std::string& msg)    = 0;
  virtual void warning(const std::string& msg) = 0;
  virtual void error(const std::string& msg)   = 0;
};

} // namespace patchage

#endif // PATCHAGE_ILOG_HPP
