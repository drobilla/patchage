// Copyright 2007-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PATCHAGE_TEXTVIEWLOG_HPP
#define PATCHAGE_TEXTVIEWLOG_HPP

#include "ILog.hpp"

#include <glibmm/refptr.h>
#include <gtkmm/texttag.h>

#include <string>

namespace Gtk {
class TextView;
} // namespace Gtk

namespace patchage {

template<typename W>
class Widget;

/// Log that writes colored messages to a Gtk TextView
class TextViewLog : public ILog
{
public:
  explicit TextViewLog(Widget<Gtk::TextView>& text_view);

  TextViewLog(const TextViewLog&) = delete;
  TextViewLog& operator=(const TextViewLog&) = delete;

  TextViewLog(TextViewLog&&) = delete;
  TextViewLog& operator=(TextViewLog&&) = delete;

  ~TextViewLog() override = default;

  void info(const std::string& msg) override;
  void error(const std::string& msg) override;
  void warning(const std::string& msg) override;

  int min_height() const;

  const Widget<Gtk::TextView>& text_view() const { return _text_view; }
  Widget<Gtk::TextView>&       text_view() { return _text_view; }

private:
  Glib::RefPtr<Gtk::TextTag> _error_tag;
  Glib::RefPtr<Gtk::TextTag> _warning_tag;
  Widget<Gtk::TextView>&     _text_view;
};

} // namespace patchage

#endif // PATCHAGE_TEXTVIEWLOG_HPP
