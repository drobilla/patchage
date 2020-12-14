/* This file is part of Patchage
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

#ifndef PATCHAGE_WIDGET_HPP
#define PATCHAGE_WIDGET_HPP

#include <glibmm/refptr.h>
#include <gtkmm/builder.h> // IWYU pragma: keep

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

	Widget(const Widget&) = delete;
	Widget& operator=(const Widget&) = delete;

	Widget(Widget&&) = delete;
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
