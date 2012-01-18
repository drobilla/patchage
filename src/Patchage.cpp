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

#include <stdlib.h>
#include <pthread.h>

#include <cmath>
#include <fstream>
#include <sstream>

#include <glib.h>
#include <glib/gstdio.h>
#include <gtk/gtkwindow.h>

#include <gtkmm/button.h>
#include <gtkmm/filechooserdialog.h>
#include <gtkmm/menuitem.h>
#include <gtkmm/stock.h>
#include <gtkmm/treemodel.h>

#include "ganv/Module.hpp"
#include "ganv/Edge.hpp"

#include "patchage_config.h"
#include "UIFile.hpp"
#include "Patchage.hpp"
#include "PatchageCanvas.hpp"
#include "PatchageEvent.hpp"
#include "StateManager.hpp"

#if defined(HAVE_JACK_DBUS)
    #include "JackDbusDriver.hpp"
#elif defined(PATCHAGE_LIBJACK)
    #include "JackDriver.hpp"
    #include <jack/statistics.h>
#endif

#ifdef PATCHAGE_JACK_SESSION
    #include <jack/session.h>
#endif

#ifdef PATCHAGE_GTK_OSX
    #include <gtkosxapplication.h>

static gboolean
can_activate_cb(GtkWidget* widget, guint signal_id, gpointer data)
{
  return gtk_widget_is_sensitive(widget);
}
#endif

#ifdef HAVE_ALSA
    #include "AlsaDriver.hpp"
#endif

using std::cout;
using std::endl;
using std::string;

struct ProjectList_column_record : public Gtk::TreeModel::ColumnRecord {
	Gtk::TreeModelColumn<Glib::ustring> label;
};

#define INIT_WIDGET(x) x(_xml, ((const char*)#x) + 1)

Patchage::Patchage(int argc, char** argv)
	: _xml(UIFile::open("patchage"))
#ifdef HAVE_ALSA
	, _alsa_driver(NULL)
#endif
	, _jack_driver(NULL)
	, _state_manager(NULL)
	, INIT_WIDGET(_about_win)
	, INIT_WIDGET(_main_scrolledwin)
	, INIT_WIDGET(_main_win)
	, INIT_WIDGET(_menubar)
	, INIT_WIDGET(_menu_alsa_connect)
	, INIT_WIDGET(_menu_alsa_disconnect)
	, INIT_WIDGET(_menu_file_quit)
	, INIT_WIDGET(_menu_help_about)
	, INIT_WIDGET(_menu_jack_connect)
	, INIT_WIDGET(_menu_jack_disconnect)
	, INIT_WIDGET(_menu_open_session)
	, INIT_WIDGET(_menu_save_session)
	, INIT_WIDGET(_menu_save_close_session)
	, INIT_WIDGET(_menu_store_positions)
	, INIT_WIDGET(_menu_view_arrange)
	, INIT_WIDGET(_menu_view_messages)
	, INIT_WIDGET(_menu_view_refresh)
	, INIT_WIDGET(_menu_zoom_in)
	, INIT_WIDGET(_menu_zoom_out)
	, INIT_WIDGET(_menu_zoom_normal)
	, INIT_WIDGET(_messages_clear_but)
	, INIT_WIDGET(_messages_close_but)
	, INIT_WIDGET(_messages_win)
	, INIT_WIDGET(_status_text)
	, _attach(true)
	, _driver_detached(false)
	, _refresh(false)
	, _enable_refresh(true)
	, _jack_driver_autoattach(true)
#ifdef HAVE_ALSA
	, _alsa_driver_autoattach(true)
#endif
{
	_settings_filename = getenv("HOME");
	_settings_filename += "/.patchagerc";
	_state_manager = new StateManager();
	_canvas = boost::shared_ptr<PatchageCanvas>(new PatchageCanvas(this, 1600*2, 1200*2));

	while (argc > 0) {
		if (!strcmp(*argv, "-h") || !strcmp(*argv, "--help")) {
			cout << "Usage: patchage [OPTIONS]" << endl;
			cout << "Visually connect JACK and ALSA Audio/MIDI ports." << endl << endl;
			cout << "Options:" << endl;
			cout << "\t-h  --help     Show this help" << endl;
			cout << "\t-A  --no-alsa  Do not automatically attach to ALSA" << endl;
			cout << "\t-J  --no-jack  Do not automatically attack to JACK" << endl;
			exit(0);
#ifdef HAVE_ALSA
		} else if (!strcmp(*argv, "-A") || !strcmp(*argv, "--no-alsa")) {
			_alsa_driver_autoattach = false;
#endif
#if defined(PATCHAGE_LIBJACK) || defined(HAVE_JACK_DBUS)
		} else if (!strcmp(*argv, "-J") || !strcmp(*argv, "--no-jack")) {
			_jack_driver_autoattach = false;
#endif
		}

		argv++;
		argc--;
	}

	Glib::set_application_name("Patchage");
	_about_win->property_program_name() = "Patchage";
	_about_win->property_logo_icon_name() = "patchage";
	gtk_window_set_default_icon_name("patchage");

	_main_scrolledwin->add(_canvas->widget());

	_main_scrolledwin->property_hadjustment().get_value()->set_step_increment(10);
	_main_scrolledwin->property_vadjustment().get_value()->set_step_increment(10);

	_main_scrolledwin->signal_scroll_event().connect(
		sigc::mem_fun(this, &Patchage::on_scroll));

#ifdef PATCHAGE_JACK_SESSION
	_menu_open_session->signal_activate().connect(
		sigc::mem_fun(this, &Patchage::show_open_session_dialog));
	_menu_save_session->signal_activate().connect(
		sigc::mem_fun(this, &Patchage::show_save_session_dialog));
	_menu_save_close_session->signal_activate().connect(
		sigc::mem_fun(this, &Patchage::show_save_close_session_dialog));
#else
	_menu_open_session->set_sensitive(false);
#endif

#ifdef HAVE_ALSA
	_menu_alsa_connect->signal_activate().connect(
			sigc::mem_fun(this, &Patchage::menu_alsa_connect));
	_menu_alsa_disconnect->signal_activate().connect(
			sigc::mem_fun(this, &Patchage::menu_alsa_disconnect));
#else
	_menu_alsa_connect->set_sensitive(false);
	_menu_alsa_disconnect->set_sensitive(false);
#endif

	_menu_store_positions->signal_activate().connect(
			sigc::mem_fun(this, &Patchage::on_store_positions));
	_menu_file_quit->signal_activate().connect(
			sigc::mem_fun(this, &Patchage::on_quit));
	_menu_view_refresh->signal_activate().connect(
			sigc::mem_fun(this, &Patchage::refresh));
	_menu_view_arrange->signal_activate().connect(
			sigc::mem_fun(this, &Patchage::on_arrange));
	_menu_view_messages->signal_activate().connect(
			sigc::mem_fun(this, &Patchage::on_show_messages));
	_menu_help_about->signal_activate().connect(
			sigc::mem_fun(this, &Patchage::on_help_about));
	_menu_zoom_in->signal_activate().connect(
			sigc::mem_fun(this, &Patchage::on_zoom_in));
	_menu_zoom_out->signal_activate().connect(
			sigc::mem_fun(this, &Patchage::on_zoom_out));
	_menu_zoom_normal->signal_activate().connect(
			sigc::mem_fun(this, &Patchage::on_zoom_normal));

	_messages_clear_but->signal_clicked().connect(
			sigc::mem_fun(this, &Patchage::on_messages_clear));
	_messages_close_but->signal_clicked().connect(
			sigc::mem_fun(this, &Patchage::on_messages_close));

	_error_tag = Gtk::TextTag::create();
	_error_tag->property_foreground() = "#FF0000";
	_status_text->get_buffer()->get_tag_table()->add(_error_tag);

	_warning_tag = Gtk::TextTag::create();
	_warning_tag->property_foreground() = "#FFFF00";
	_status_text->get_buffer()->get_tag_table()->add(_warning_tag);

	_canvas->widget().show();
	_main_win->present();

	_state_manager->load(_settings_filename);

	_main_win->resize(
		static_cast<int>(_state_manager->get_window_size().x),
		static_cast<int>(_state_manager->get_window_size().y));

	_main_win->move(
		static_cast<int>(_state_manager->get_window_location().x),
		static_cast<int>(_state_manager->get_window_location().y));

	_about_win->set_transient_for(*_main_win);

#if defined(PATCHAGE_LIBJACK) || defined(HAVE_JACK_DBUS)
	_jack_driver = new JackDriver(this);
	_jack_driver->signal_detached.connect(sigc::mem_fun(this, &Patchage::driver_detached));

	_menu_jack_connect->signal_activate().connect(sigc::bind(
			sigc::mem_fun(_jack_driver, &JackDriver::attach), true));
	_menu_jack_disconnect->signal_activate().connect(
			sigc::mem_fun(_jack_driver, &JackDriver::detach));
#endif

#ifdef HAVE_ALSA
	_alsa_driver = new AlsaDriver(this);
#endif

	connect_widgets();
	update_state();

	_canvas->widget().grab_focus();

	// Idle callback, check if we need to refresh
	Glib::signal_timeout().connect(
			sigc::mem_fun(this, &Patchage::idle_callback), 100);

#ifdef PATCHAGE_GTK_OSX
	// Set up Mac menu bar
	GtkOSXApplication* osxapp;
	gtk_osxapplication_ready(osxapp);

	_menubar->hide();
	gtk_osxapplication_set_menu_bar(osxapp, GTK_MENU_SHELL(_menubar->gobj()));
	gtk_osxapplication_insert_app_menu_item(
		osxapp, GTK_WIDGET(_menu_help_about->gobj()), 0);
	g_signal_connect(_menubar->gobj(), "can-activate-accel", 
	                 G_CALLBACK(can_activate_cb), NULL);
#endif
}

Patchage::~Patchage()
{
#if defined(PATCHAGE_LIBJACK) || defined(HAVE_JACK_DBUS)
	delete _jack_driver;
#endif
#ifdef HAVE_ALSA
	delete _alsa_driver;
#endif

	delete _state_manager;

	_about_win.destroy();
	_messages_win.destroy();

	_xml.reset();
}

void
Patchage::attach()
{
	_enable_refresh = false;

#if defined(PATCHAGE_LIBJACK) || defined(HAVE_JACK_DBUS)
	if (_jack_driver_autoattach)
		_jack_driver->attach(true);
#endif

#ifdef HAVE_ALSA
	if (_alsa_driver_autoattach)
		_alsa_driver->attach();
#endif

	_enable_refresh = true;

	refresh();
}

bool
Patchage::idle_callback()
{
	// Initial run, attach
	if (_attach) {
		attach();
		_attach = false;
	}

	// Process any JACK events
#if defined(PATCHAGE_LIBJACK) || defined(HAVE_JACK_DBUS)
	if (_jack_driver) {
		_jack_driver->process_events(this);
	}
#endif

	// Process any ALSA events
#ifdef HAVE_ALSA
	if (_alsa_driver) {
		_alsa_driver->process_events(this);
	}
#endif

	// Do a full refresh
	if (_refresh) {
		refresh();
	} else if (_driver_detached) {
#if defined(PATCHAGE_LIBJACK) || defined(HAVE_JACK_DBUS)
		if (_jack_driver && !_jack_driver->is_attached())
			_jack_driver->destroy_all();
#endif
#ifdef HAVE_ALSA
		if (_alsa_driver && !_alsa_driver->is_attached())
			_alsa_driver->destroy_all();
#endif
	}

	_refresh         = false;
	_driver_detached = false;

	return true;
}

void
Patchage::zoom(double z)
{
	_state_manager->set_zoom(z);
	_canvas->set_zoom(z);
}

void
Patchage::refresh()
{
	assert(_canvas);

	if (_enable_refresh) {

		_canvas->destroy();

#if defined(PATCHAGE_LIBJACK) || defined(HAVE_JACK_DBUS)
		if (_jack_driver)
			_jack_driver->refresh();
#endif

#ifdef HAVE_ALSA
		if (_alsa_driver)
			_alsa_driver->refresh();
#endif
	}
}

/** Update the stored window location and size in the StateManager (in memory).
 */
void
Patchage::store_window_location()
{
	int loc_x, loc_y, size_x, size_y;
	_main_win->get_position(loc_x, loc_y);
	_main_win->get_size(size_x, size_y);
	Coord window_location;
	window_location.x = loc_x;
	window_location.y = loc_y;
	Coord window_size;
	window_size.x = size_x;
	window_size.y = size_y;
	_state_manager->set_window_location(window_location);
	_state_manager->set_window_size(window_size);
}

void
Patchage::error_msg(const std::string& msg)
{
	Glib::RefPtr<Gtk::TextBuffer> buffer = _status_text->get_buffer();
	buffer->insert_with_tag(buffer->end(), msg + "\n", _error_tag);
	_status_text->scroll_to_mark(buffer->get_insert(), 0);
	_messages_win->present();
}

void
Patchage::info_msg(const std::string& msg)
{
	Glib::RefPtr<Gtk::TextBuffer> buffer = _status_text->get_buffer();
	buffer->insert(buffer->end(), msg + "\n");
	_status_text->scroll_to_mark(buffer->get_insert(), 0);
}

void
Patchage::warning_msg(const std::string& msg)
{
	Glib::RefPtr<Gtk::TextBuffer> buffer = _status_text->get_buffer();
	buffer->insert_with_tag(buffer->end(), msg + "\n", _warning_tag);
	_status_text->scroll_to_mark(buffer->get_insert(), 0);
}

static void
load_module_location(GanvNode* node, void* data)
{
	if (GANV_IS_MODULE(node)) {
		Ganv::Module* cmodule = Glib::wrap(GANV_MODULE(node));
		PatchageModule* pmodule = dynamic_cast<PatchageModule*>(cmodule);
		if (pmodule) {
			pmodule->load_location();
		}
	}
}
	
void
Patchage::update_state()
{
	_canvas->for_each_node(load_module_location, NULL);
}

/** Update the sensitivity status of menus to reflect the present.
 *
 * (eg. disable "Connect to Jack" when Patchage is already connected to Jack)
 */
void
Patchage::connect_widgets()
{
#if defined(PATCHAGE_LIBJACK) || defined(HAVE_JACK_DBUS)
	_jack_driver->signal_attached.connect(sigc::bind(
			sigc::mem_fun(*_menu_jack_connect, &Gtk::MenuItem::set_sensitive), false));
	_jack_driver->signal_attached.connect(
			sigc::mem_fun(this, &Patchage::refresh));
	_jack_driver->signal_attached.connect(sigc::bind(
			sigc::mem_fun(*_menu_jack_disconnect, &Gtk::MenuItem::set_sensitive), true));

	_jack_driver->signal_detached.connect(sigc::bind(
			sigc::mem_fun(*_menu_jack_connect, &Gtk::MenuItem::set_sensitive), true));
	_jack_driver->signal_detached.connect(sigc::bind(
			sigc::mem_fun(*_menu_jack_disconnect, &Gtk::MenuItem::set_sensitive), false));
#endif

#ifdef HAVE_ALSA
	_alsa_driver->signal_attached.connect(sigc::bind(
			sigc::mem_fun(*_menu_alsa_connect, &Gtk::MenuItem::set_sensitive), false));
	_alsa_driver->signal_attached.connect(sigc::bind(
			sigc::mem_fun(*_menu_alsa_disconnect, &Gtk::MenuItem::set_sensitive), true));

	_alsa_driver->signal_detached.connect(sigc::bind(
			sigc::mem_fun(*_menu_alsa_connect, &Gtk::MenuItem::set_sensitive), true));
	_alsa_driver->signal_detached.connect(sigc::bind(
			sigc::mem_fun(*_menu_alsa_disconnect, &Gtk::MenuItem::set_sensitive), false));
#endif
}

#ifdef PATCHAGE_JACK_SESSION
void
Patchage::show_open_session_dialog()
{
	Gtk::FileChooserDialog dialog(*_main_win, "Open Session",
	                              Gtk::FILE_CHOOSER_ACTION_SELECT_FOLDER);
	
	dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
	Gtk::Button* open_but = dialog.add_button(Gtk::Stock::OPEN, Gtk::RESPONSE_OK);
	open_but->property_has_default() = true;

	if (dialog.run() != Gtk::RESPONSE_OK) {
		return;
	}

	const std::string dir = dialog.get_filename();
	if (g_chdir(dir.c_str())) {
		error_msg("Failed to switch to session directory " + dir);
		return;
	}

	if (system("./jack-session") < 0) {
		error_msg("Error executing `./jack-session' in " + dir);
	} else {
		info_msg("Loaded session " + dir);
	}
}

static void
print_edge(GanvEdge* edge, void* data)
{
	std::ofstream* script = (std::ofstream*)data;
	Ganv::Edge*    edgemm = Glib::wrap(edge);

	PatchagePort* src = dynamic_cast<PatchagePort*>((edgemm)->get_tail());
	PatchagePort* dst = dynamic_cast<PatchagePort*>((edgemm)->get_head());

	if (!src || !dst || src->type() == ALSA_MIDI || dst->type() == ALSA_MIDI) {
		return;
	}

	(*script) << "jack_connect '" << src->full_name()
	          << "' '" << dst->full_name() << "' &" << endl;
}

void
Patchage::save_session(bool close)
{
	Gtk::FileChooserDialog dialog(*_main_win, "Save Session",
	                              Gtk::FILE_CHOOSER_ACTION_SAVE);

	dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
	Gtk::Button* save_but = dialog.add_button(Gtk::Stock::SAVE, Gtk::RESPONSE_OK);
	save_but->property_has_default() = true;

	if (dialog.run() != Gtk::RESPONSE_OK) {
		return;
	}

	std::string path = dialog.get_filename();
	if (g_mkdir_with_parents(path.c_str(), 0740)) {
		error_msg("Failed to create session directory " + path);
		return;
	}

	path += '/';
	jack_session_command_t* cmd = jack_session_notify(
		_jack_driver->client(),
		NULL,
		close ? JackSessionSaveAndQuit : JackSessionSave,
		path.c_str());

	const std::string script_path = path + "jack-session";
	std::ofstream script(script_path.c_str());
	script << "#!/bin/sh" << endl << endl;

	const std::string var("${SESSION_DIR}");
	for (int c = 0; cmd[c].uuid; ++c) {
		std::string  command = cmd[c].command;
		const size_t index   = command.find(var);
		if (index != string::npos) {
			command.replace(index, var.length(), cmd[c].client_name);
		}

		script << command << " &" << endl;
	}

	script << endl;
	script << "sleep 3" << endl;
	script << endl;

	_canvas->for_each_edge(print_edge, &script);

	script.close();
	g_chmod(script_path.c_str(), 0740);
}

void
Patchage::show_save_session_dialog()
{
	save_session(false);
}

void
Patchage::show_save_close_session_dialog()
{
	save_session(true);
}

#endif

#ifdef HAVE_ALSA
void
Patchage::menu_alsa_connect()
{
	_alsa_driver->attach(false);
	_alsa_driver->refresh();
}

void
Patchage::menu_alsa_disconnect()
{
	_alsa_driver->detach();
	refresh();
}
#endif

void
Patchage::on_arrange()
{
	assert(_canvas);

	_canvas->arrange();
}

void
Patchage::on_help_about()
{
	_about_win->run();
	_about_win->hide();
}

void
Patchage::on_zoom_in()
{
	_canvas->set_font_size(_canvas->get_font_size() + 1.0);
}

void
Patchage::on_zoom_out()
{
	_canvas->set_font_size(_canvas->get_font_size() - 1.0);
}

void
Patchage::on_zoom_normal()
{
	_canvas->set_zoom_and_font_size(1.0, _canvas->get_default_font_size());
}

void
Patchage::on_messages_clear()
{
	_status_text->get_buffer()->erase(
			_status_text->get_buffer()->begin(),
			_status_text->get_buffer()->end());
}

void
Patchage::on_messages_close()
{
	_messages_win->hide();
}

void
Patchage::on_quit()
{
#ifdef HAVE_ALSA
	_alsa_driver->detach();
#endif
#if defined(PATCHAGE_LIBJACK) || defined(HAVE_JACK_DBUS)
	_jack_driver->detach();
#endif
	_main_win->hide();
	_canvas.reset();
}

void
Patchage::on_show_messages()
{
	_messages_win->present();
}

void
Patchage::on_store_positions()
{
	store_window_location();
	_state_manager->save(_settings_filename);
}

bool
Patchage::on_scroll(GdkEventScroll* ev)
{
	return false;
}
