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

#ifndef PATCHAGE_TEXTVIEWLOG_HPP
#define PATCHAGE_TEXTVIEWLOG_HPP

#include "ILog.hpp"
#include "Widget.hpp"

#include <glibmm/refptr.h>
#include <gtkmm/texttag.h>
#include <gtkmm/textview.h>

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

	void info_msg(const std::string& msg) override;
	void error_msg(const std::string& msg) override;
	void warning_msg(const std::string& msg) override;

	int min_height() const;

	const Widget<Gtk::TextView>& text_view() const { return _text_view; }
	Widget<Gtk::TextView>&       text_view() { return _text_view; }

private:
	Glib::RefPtr<Gtk::TextTag> _error_tag;
	Glib::RefPtr<Gtk::TextTag> _warning_tag;
	Widget<Gtk::TextView>&     _text_view;
};

#endif // PATCHAGE_TEXTVIEWLOG_HPP
