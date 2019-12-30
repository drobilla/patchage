/* This file is part of Patchage.
 * Copyright 2007-2014 David Robillard <http://drobilla.net>
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

#ifndef PATCHAGE_PATCHAGE_HPP
#define PATCHAGE_PATCHAGE_HPP

#include <gtkmm/aboutdialog.h>
#include <gtkmm/alignment.h>
#include <gtkmm/builder.h>
#include <gtkmm/button.h>
#include <gtkmm/checkmenuitem.h>
#include <gtkmm/combobox.h>
#include <gtkmm/dialog.h>
#include <gtkmm/imagemenuitem.h>
#include <gtkmm/label.h>
#include <gtkmm/main.h>
#include <gtkmm/menubar.h>
#include <gtkmm/menuitem.h>
#include <gtkmm/paned.h>
#include <gtkmm/progressbar.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/textview.h>
#include <gtkmm/toolbar.h>
#include <gtkmm/toolbutton.h>
#include <gtkmm/viewport.h>
#include <gtkmm/window.h>

#include "Legend.hpp"
#include "Widget.hpp"
#include "patchage_config.h"

#include <memory>
#include <set>
#include <string>

class AlsaDriver;
class JackDriver;
class PatchageCanvas;
class Configuration;

namespace Ganv { class Module; }

class Patchage {
public:
	Patchage(int argc, char** argv);
	~Patchage();

	const std::shared_ptr<PatchageCanvas>& canvas() const { return _canvas; }

	Gtk::Window* window() { return _main_win.get(); }

	Configuration* conf()        const { return _conf; }
	JackDriver*    jack_driver() const { return _jack_driver; }
#ifdef HAVE_ALSA
	AlsaDriver*    alsa_driver() const { return _alsa_driver; }
#endif
#ifdef PATCHAGE_JACK_SESSION
	void show_open_session_dialog();
	void show_save_session_dialog();
	void show_save_close_session_dialog();
#endif

	Glib::RefPtr<Gtk::Builder> xml() { return _xml; }

	void attach();
	void save();
	void quit() { _main_win->hide(); }

	void        refresh();
	inline void queue_refresh()   { _refresh = true; }
	inline void driver_detached() { _driver_detached = true; }

	void info_msg(const std::string& msg);
	void error_msg(const std::string& msg);
	void warning_msg(const std::string& msg);

	void update_state();
	void store_window_location();

	bool show_human_names() const { return _menu_view_human_names->get_active(); }
	bool sort_ports()       const { return _menu_view_sort_ports->get_active(); }

protected:
	class BufferSizeColumns : public Gtk::TreeModel::ColumnRecord {
	public:
		BufferSizeColumns() { add(label); }

		Gtk::TreeModelColumn<Glib::ustring> label;
	};

	void connect_widgets();

	void on_arrange();
	void on_sprung_layout_toggled();
	void on_help_about();
	void on_quit();
	void on_export_image();
	void on_view_messages();
	void on_view_toolbar();
	void on_store_positions();
	void on_view_human_names();
	void on_view_sort_ports();
	void on_zoom_in();
	void on_zoom_out();
	void on_zoom_normal();
	void on_zoom_full();
	void on_increase_font_size();
	void on_decrease_font_size();
	void on_normal_font_size();
	void on_legend_color_change(int id, const std::string& label, uint32_t rgba);
	void on_messages_resized(Gtk::Allocation& alloc);

	bool on_scroll(GdkEventScroll* ev);

	void zoom(double z);
	bool idle_callback();
	void clear_load();
	bool update_load();
	void update_toolbar();

	void buffer_size_changed();

	Glib::RefPtr<Gtk::Builder> _xml;

#ifdef HAVE_ALSA
	AlsaDriver* _alsa_driver;
	void menu_alsa_connect();
	void menu_alsa_disconnect();
#endif

#ifdef PATCHAGE_JACK_SESSION
	void save_session(bool close);
#endif

	std::shared_ptr<PatchageCanvas> _canvas;

	JackDriver*    _jack_driver;
	Configuration* _conf;

	Gtk::Main* _gtk_main;

	BufferSizeColumns _buf_size_columns;

	Widget<Gtk::AboutDialog>    _about_win;
	Widget<Gtk::ScrolledWindow> _main_scrolledwin;
	Widget<Gtk::Window>         _main_win;
	Widget<Gtk::VBox>           _main_vbox;
	Widget<Gtk::MenuBar>        _menubar;
	Widget<Gtk::MenuItem>       _menu_alsa_connect;
	Widget<Gtk::MenuItem>       _menu_alsa_disconnect;
	Widget<Gtk::MenuItem>       _menu_file_quit;
	Widget<Gtk::MenuItem>       _menu_export_image;
	Widget<Gtk::MenuItem>       _menu_help_about;
	Widget<Gtk::MenuItem>       _menu_jack_connect;
	Widget<Gtk::MenuItem>       _menu_jack_disconnect;
	Widget<Gtk::MenuItem>       _menu_open_session;
	Widget<Gtk::MenuItem>       _menu_save_session;
	Widget<Gtk::MenuItem>       _menu_save_close_session;
	Widget<Gtk::MenuItem>       _menu_view_arrange;
	Widget<Gtk::CheckMenuItem>  _menu_view_sprung_layout;
	Widget<Gtk::CheckMenuItem>  _menu_view_messages;
	Widget<Gtk::CheckMenuItem>  _menu_view_toolbar;
	Widget<Gtk::MenuItem>       _menu_view_refresh;
	Widget<Gtk::CheckMenuItem>  _menu_view_human_names;
	Widget<Gtk::CheckMenuItem>  _menu_view_sort_ports;
	Widget<Gtk::ImageMenuItem>  _menu_zoom_in;
	Widget<Gtk::ImageMenuItem>  _menu_zoom_out;
	Widget<Gtk::ImageMenuItem>  _menu_zoom_normal;
	Widget<Gtk::ImageMenuItem>  _menu_zoom_full;
	Widget<Gtk::MenuItem>       _menu_increase_font_size;
	Widget<Gtk::MenuItem>       _menu_decrease_font_size;
	Widget<Gtk::MenuItem>       _menu_normal_font_size;
	Widget<Gtk::Toolbar>        _toolbar;
	Widget<Gtk::ToolButton>     _clear_load_but;
	Widget<Gtk::ProgressBar>    _xrun_progress;
	Widget<Gtk::ComboBox>       _buf_size_combo;
	Widget<Gtk::Label>          _latency_label;
	Widget<Gtk::Alignment>      _legend_alignment;
	Widget<Gtk::Paned>          _main_paned;
	Widget<Gtk::ScrolledWindow> _log_scrolledwindow;
	Widget<Gtk::TextView>       _status_text;
	Legend*                     _legend;

	Glib::RefPtr<Gtk::TextTag> _error_tag;
	Glib::RefPtr<Gtk::TextTag> _warning_tag;

	bool _pane_initialized;
	bool _attach;
	bool _driver_detached;
	bool _refresh;
	bool _enable_refresh;
	bool _jack_driver_autoattach;
#ifdef HAVE_ALSA
	bool _alsa_driver_autoattach;
#endif
};

#endif // PATCHAGE_PATCHAGE_HPP
