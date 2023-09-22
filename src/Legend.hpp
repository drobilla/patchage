// Copyright 2014-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PATCHAGE_LEGEND_HPP
#define PATCHAGE_LEGEND_HPP

#include <gtkmm/box.h>
#include <sigc++/signal.h>

#include <cstdint>
#include <string>

namespace Gtk {
class ColorButton;
} // namespace Gtk

namespace patchage {

enum class PortType;

class Configuration;

class Legend : public Gtk::HBox
{
public:
  explicit Legend(const Configuration& configuration);

  sigc::signal<void, PortType, std::string, uint32_t> signal_color_changed;

private:
  void add_button(PortType id, const std::string& label, uint32_t rgba);

  void on_color_set(PortType                id,
                    const std::string&      label,
                    const Gtk::ColorButton* but);
};

} // namespace patchage

#endif // PATCHAGE_LEGEND_HPP
