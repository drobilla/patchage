// Copyright 2007-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PATCHAGE_UIFILE_HPP
#define PATCHAGE_UIFILE_HPP

#include "patchage_config.h"

#ifdef PATCHAGE_BINLOC
#  include "binary_location.h"
#endif

#include <glibmm/refptr.h>
#include <gtkmm/builder.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace patchage {

class UIFile
{
public:
  inline static bool is_readable(const std::string& filename)
  {
    std::ifstream fs(filename.c_str());
    const bool    fail = fs.fail();
    fs.close();
    return !fail;
  }

  static Glib::RefPtr<Gtk::Builder> open(const std::string& base_name)
  {
    std::cout << "Base name: " << base_name << std::endl;
    std::string ui_filename = base_name + ".ui";

#ifdef PATCHAGE_BINLOC
    const std::string bundle = bundle_location();
    if (!bundle.empty()) {
      const std::string bundle_ui_filename = bundle + "/" + ui_filename;
      if (is_readable(bundle_ui_filename)) {
        std::cout << "Loading UI file " << bundle_ui_filename << std::endl;
        return Gtk::Builder::create_from_file(bundle_ui_filename);
      }
    }
#endif

    ui_filename = std::string(PATCHAGE_DATA_DIR) + "/" + ui_filename;
    if (is_readable(ui_filename)) {
      std::cout << "Loading UI file " << ui_filename << std::endl;
      return Gtk::Builder::create_from_file(ui_filename);
    }

    std::stringstream ss;
    ss << "Unable to find " << ui_filename << std::endl;
    throw std::runtime_error(ss.str());
    return {};
  }
};

} // namespace patchage

#endif // PATCHAGE_UIFILE_HPP
