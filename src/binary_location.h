// Copyright 2008-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <dlfcn.h>

#include <climits>
#include <cstdlib>
#include <string>

namespace patchage {

/** Return the absolute path of the binary. */
inline std::string
binary_location()
{
  Dl_info   dli = {};
  const int ret = dladdr(reinterpret_cast<void*>(&binary_location), &dli);
  if (!ret) {
    return "";
  }

  char* const bin_loc = realpath(dli.dli_fname, nullptr);
  if (!bin_loc) {
    return "";
  }

  std::string loc{bin_loc};
  free(bin_loc);
  return loc;
}

/** Return the absolute path of the bundle (binary parent directory). */
inline std::string
bundle_location()
{
  const std::string binary = binary_location();
  if (binary.empty()) {
    return "";
  }
  return binary.substr(0, binary.find_last_of('/'));
}

} // namespace patchage
