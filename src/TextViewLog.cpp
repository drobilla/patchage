// Copyright 2007-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TextViewLog.hpp"

#include "Widget.hpp"

#include <gdkmm/color.h>
#include <glibmm/propertyproxy.h>
#include <glibmm/refptr.h>
#include <gtkmm/enums.h>
#include <gtkmm/textbuffer.h>
#include <gtkmm/texttag.h>
#include <gtkmm/texttagtable.h>
#include <gtkmm/textview.h>

#include <string>

namespace patchage {

TextViewLog::TextViewLog(Widget<Gtk::TextView>& text_view)
  : _error_tag{Gtk::TextTag::create()}
  , _warning_tag{Gtk::TextTag::create()}
  , _text_view{text_view}
{
  for (int s = Gtk::STATE_NORMAL; s <= Gtk::STATE_INSENSITIVE; ++s) {
    _text_view->modify_base(static_cast<Gtk::StateType>(s),
                            Gdk::Color("#000000"));
    _text_view->modify_text(static_cast<Gtk::StateType>(s),
                            Gdk::Color("#FFFFFF"));
  }

  _error_tag->property_foreground() = "#CC0000";
  _text_view->get_buffer()->get_tag_table()->add(_error_tag);

  _warning_tag->property_foreground() = "#C4A000";
  _text_view->get_buffer()->get_tag_table()->add(_warning_tag);

  _text_view->set_pixels_inside_wrap(2);
  _text_view->set_left_margin(4);
  _text_view->set_right_margin(4);
  _text_view->set_pixels_below_lines(2);
}

void
TextViewLog::info(const std::string& msg)
{
  Glib::RefPtr<Gtk::TextBuffer> buffer = _text_view->get_buffer();
  buffer->insert(buffer->end(), std::string("\n") + msg);
  _text_view->scroll_to_mark(buffer->get_insert(), 0);
}

void
TextViewLog::warning(const std::string& msg)
{
  Glib::RefPtr<Gtk::TextBuffer> buffer = _text_view->get_buffer();
  buffer->insert_with_tag(buffer->end(), std::string("\n") + msg, _warning_tag);
  _text_view->scroll_to_mark(buffer->get_insert(), 0);
}

void
TextViewLog::error(const std::string& msg)
{
  Glib::RefPtr<Gtk::TextBuffer> buffer = _text_view->get_buffer();
  buffer->insert_with_tag(buffer->end(), std::string("\n") + msg, _error_tag);
  _text_view->scroll_to_mark(buffer->get_insert(), 0);
}

int
TextViewLog::min_height() const
{
  Glib::RefPtr<Gtk::TextBuffer> buffer = _text_view->get_buffer();

  int y           = 0;
  int line_height = 0;
  _text_view->get_line_yrange(buffer->begin(), y, line_height);

  return line_height + 2 * _text_view->get_pixels_inside_wrap();
}

} // namespace patchage
