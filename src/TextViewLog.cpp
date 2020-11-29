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

#include "TextViewLog.hpp"

#include <glibmm/refptr.h>
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
	buffer->insert_with_tag(
	    buffer->end(), std::string("\n") + msg, _warning_tag);
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
