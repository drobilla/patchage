/* This file is part of Patchage.
 * Copyright 2007-2011 David Robillard <http://drobilla.net>
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

#include <set>
#include <string>

#include <boost/shared_ptr.hpp>

#include <gtkmm/aboutdialog.h>
#include <gtkmm/builder.h>
#include <gtkmm/button.h>
#include <gtkmm/checkmenuitem.h>
#include <gtkmm/dialog.h>
#include <gtkmm/imagemenuitem.h>
#include <gtkmm/label.h>
#include <gtkmm/main.h>
#include <gtkmm/menuitem.h>
#include <gtkmm/progressbar.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/statusbar.h>
#include <gtkmm/textview.h>
#include <gtkmm/viewport.h>
#include <gtkmm/window.h>

#include "patchage-config.h"
#include "Widget.hpp"

class AlsaDriver;
class JackDriver;
class PatchageCanvas;
class StateManager;

namespace FlowCanvas { class Module; }

class Patchage {
public:
	Patchage(int argc, char** argv);
	~Patchage();

	boost::shared_ptr<PatchageCanvas> canvas() const { return _canvas; }

	Gtk::Window* window() { return _main_win.get(); }

	StateManager* state_manager() const { return _state_manager; }
	JackDriver*   jack_driver()   const { return _jack_driver; }
#ifdef HAVE_ALSA
	AlsaDriver*   alsa_driver()   const { return _alsa_driver; }
#endif
#ifdef PATCHAGE_JACK_SESSION
	void show_open_session_dialog();
	void show_save_session_dialog();
	void show_save_close_session_dialog();
#endif

	Glib::RefPtr<Gtk::Builder> xml() { return _xml; }

	void attach();
	void quit() { _main_win->hide(); }

	void        refresh();
	inline void queue_refresh()   { _refresh = true; }
	inline void driver_detached() { _driver_detached = true; }

	void clear_load();
	void info_msg(const std::string& msg);
	void error_msg(const std::string& msg);
	void status_msg(const std::string& msg);
	void update_state();
	void store_window_location();

protected:
	void connect_widgets();

	void on_arrange();
	void on_help_about();
	void on_messages_clear();
	void on_messages_close();
	bool on_messages_delete(GdkEventAny*);
	void on_quit();
	void on_show_messages();
	void on_store_positions();
	void on_view_statusbar();
	void on_zoom_in();
	void on_zoom_out();
	void on_zoom_normal();

	bool on_scroll(GdkEventScroll* ev);

	void zoom(double z);
	bool idle_callback();
	bool update_load();
	void update_statusbar();

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

	boost::shared_ptr<PatchageCanvas> _canvas;

	JackDriver*   _jack_driver;
	StateManager* _state_manager;

	Gtk::Main* _gtk_main;

	std::string _settings_filename;

	Widget<Gtk::AboutDialog>    _about_win;
	Widget<Gtk::ScrolledWindow> _main_scrolledwin;
	Widget<Gtk::Window>         _main_win;
	Widget<Gtk::ProgressBar>    _main_xrun_progress;
	Widget<Gtk::MenuItem>       _menu_alsa_connect;
	Widget<Gtk::MenuItem>       _menu_alsa_disconnect;
	Widget<Gtk::MenuItem>       _menu_file_quit;
	Widget<Gtk::MenuItem>       _menu_help_about;
	Widget<Gtk::MenuItem>       _menu_jack_connect;
	Widget<Gtk::MenuItem>       _menu_jack_disconnect;
	Widget<Gtk::MenuItem>       _menu_open_session;
	Widget<Gtk::MenuItem>       _menu_save_session;
	Widget<Gtk::MenuItem>       _menu_save_close_session;
	Widget<Gtk::MenuItem>       _menu_store_positions;
	Widget<Gtk::MenuItem>       _menu_view_arrange;
	Widget<Gtk::CheckMenuItem>  _menu_view_messages;
	Widget<Gtk::MenuItem>       _menu_view_refresh;
	Widget<Gtk::CheckMenuItem>  _menu_view_statusbar;
	Widget<Gtk::ImageMenuItem>  _menu_zoom_in;
	Widget<Gtk::ImageMenuItem>  _menu_zoom_out;
	Widget<Gtk::ImageMenuItem>  _menu_zoom_normal;
	Widget<Gtk::Button>         _messages_clear_but;
	Widget<Gtk::Button>         _messages_close_but;
	Widget<Gtk::Dialog>         _messages_win;
	Widget<Gtk::Label>          _latency_frames_label;
	Widget<Gtk::Label>          _latency_ms_label;
	Widget<Gtk::Label>          _sample_rate_label;
	Widget<Gtk::TextView>       _status_text;
	Widget<Gtk::Statusbar>      _statusbar;

	bool        _attach;
	bool        _driver_detached;
	bool        _refresh;
	bool        _enable_refresh;
	bool        _jack_driver_autoattach;
#ifdef HAVE_ALSA
	bool        _alsa_driver_autoattach;
#endif

};

#endif // PATCHAGE_PATCHAGE_HPP
