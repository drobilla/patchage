// Copyright 2007-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: GPL-3.0-or-later

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
