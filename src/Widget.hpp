// Copyright 2007-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PATCHAGE_WIDGET_HPP
#define PATCHAGE_WIDGET_HPP

#include <glibmm/refptr.h>
#include <gtkmm/builder.h>

#include <string>

namespace patchage {

template<typename W>
class Widget
{
public:
  Widget(const Glib::RefPtr<Gtk::Builder>& xml, const std::string& name)
  {
    xml->get_widget(name, _me);
  }

  Widget(const Widget&)            = delete;
  Widget& operator=(const Widget&) = delete;

  Widget(Widget&&)            = delete;
  Widget& operator=(Widget&&) = delete;

  ~Widget() = default;

  void destroy() { delete _me; }

  W*       get() { return _me; }
  const W* get() const { return _me; }
  W*       operator->() { return _me; }
  const W* operator->() const { return _me; }
  W&       operator*() { return *_me; }
  const W& operator*() const { return *_me; }

private:
  W* _me;
};

} // namespace patchage

#endif // PATCHAGE_WIDGET_HPP
