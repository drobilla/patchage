/* This file is part of Patchage.
 * Copyright 2007-2011 David Robillard <http://drobilla.net>
 *
 * Patchage is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * Patchage is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include <pthread.h>

#include <cmath>
#include <fstream>
#include <sstream>

#include <gtk/gtkwindow.h>
#include <gtkmm.h>
#include <libgnomecanvasmm.h>

#include "flowcanvas/Module.hpp"
#include "raul/SharedPtr.hpp"

#include "patchage-config.h"
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

#ifdef HAVE_ALSA
  #include "AlsaDriver.hpp"
#endif
#if defined(HAVE_LASH) || defined(HAVE_JACK_DBUS)
  #include "DBus.hpp"
#endif
#ifdef HAVE_LASH
  #include "LashProxy.hpp"
  #include "LoadProjectDialog.hpp"
  #include "ProjectList.hpp"
  #include "Session.hpp"
#endif

#define LOG_TO_STATUS 1

using std::cout;
using std::endl;
using std::string;

struct ProjectList_column_record : public Gtk::TreeModel::ColumnRecord {
	Gtk::TreeModelColumn<Glib::ustring> label;
};

#define INIT_WIDGET(x) x(_xml, ((const char*)#x) + 1)

Patchage::Patchage(int argc, char** argv)
	: _xml(UIFile::open("patchage"))
#ifdef HAVE_LASH
	, _dbus(NULL)
	, _lash_proxy(NULL)
	, _project_list(NULL)
	, _session(NULL)
#endif
#ifdef HAVE_ALSA
	, _alsa_driver(NULL)
	, _alsa_driver_autoattach(true)
#endif
	, _jack_driver(NULL)
	, _jack_driver_autoattach(true)
	, _state_manager(NULL)
	, _attach(true)
	, _driver_detached(false)
	, _refresh(false)
	, _enable_refresh(true)
	, INIT_WIDGET(_about_win)
	, INIT_WIDGET(_main_scrolledwin)
	, INIT_WIDGET(_main_win)
	, INIT_WIDGET(_main_xrun_progress)
	, INIT_WIDGET(_menu_alsa_connect)
	, INIT_WIDGET(_menu_alsa_disconnect)
	, INIT_WIDGET(_menu_file_quit)
	, INIT_WIDGET(_menu_help_about)
	, INIT_WIDGET(_menu_jack_connect)
	, INIT_WIDGET(_menu_jack_disconnect)
	, INIT_WIDGET(_menu_open_session)
	, INIT_WIDGET(_menu_store_positions)
	, INIT_WIDGET(_menu_view_arrange)
	, INIT_WIDGET(_menu_view_messages)
	, INIT_WIDGET(_menu_view_projects)
	, INIT_WIDGET(_menu_view_refresh)
	, INIT_WIDGET(_menu_view_statusbar)
	, INIT_WIDGET(_menu_zoom_in)
	, INIT_WIDGET(_menu_zoom_out)
	, INIT_WIDGET(_menu_zoom_full)
	, INIT_WIDGET(_menu_zoom_normal)
	, INIT_WIDGET(_messages_clear_but)
	, INIT_WIDGET(_messages_close_but)
	, INIT_WIDGET(_messages_win)
	, INIT_WIDGET(_project_list_viewport)
	, INIT_WIDGET(_latency_frames_label)
	, INIT_WIDGET(_latency_ms_label)
	, INIT_WIDGET(_sample_rate_label)
	, INIT_WIDGET(_status_text)
	, INIT_WIDGET(_statusbar)
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

	_main_scrolledwin->add(*_canvas);

	_main_scrolledwin->property_hadjustment().get_value()->set_step_increment(10);
	_main_scrolledwin->property_vadjustment().get_value()->set_step_increment(10);

	_main_scrolledwin->signal_scroll_event().connect(
			sigc::mem_fun(this, &Patchage::on_scroll));

#ifdef HAVE_LASH
	_menu_open_session->signal_activate().connect(
			sigc::mem_fun(this, &Patchage::show_load_project_dialog));
	_menu_view_projects->set_active(true);
#else
	_menu_open_session->set_sensitive(false);
	_menu_view_projects->set_active(false);
	_menu_view_projects->set_sensitive(false);
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
	_menu_view_statusbar->signal_activate().connect(
			sigc::mem_fun(this, &Patchage::on_view_statusbar));
	_menu_view_messages->signal_toggled().connect(
			sigc::mem_fun(this, &Patchage::on_show_messages));
	_menu_view_projects->signal_toggled().connect(
			sigc::mem_fun(this, &Patchage::on_show_projects));
	_menu_help_about->signal_activate().connect(
			sigc::mem_fun(this, &Patchage::on_help_about));
	_menu_zoom_in->signal_activate().connect(
			sigc::mem_fun(this, &Patchage::on_zoom_in));
	_menu_zoom_out->signal_activate().connect(
			sigc::mem_fun(this, &Patchage::on_zoom_out));
	_menu_zoom_full->signal_activate().connect(
			sigc::mem_fun(this, &Patchage::on_zoom_full));
	_menu_zoom_normal->signal_activate().connect(
			sigc::mem_fun(this, &Patchage::on_zoom_normal));

	_messages_clear_but->signal_clicked().connect(
			sigc::mem_fun(this, &Patchage::on_messages_clear));
	_messages_close_but->signal_clicked().connect(
			sigc::mem_fun(this, &Patchage::on_messages_close));
	_messages_win->signal_delete_event().connect(
			sigc::mem_fun(this, &Patchage::on_messages_delete));

	_canvas->show();
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

#ifdef HAVE_LASH
	_dbus = new DBus(this);
	_session = new Session();
	_project_list = new ProjectList(this, _session);
	_lash_proxy = new LashProxy(this, _session);
#else
	_project_list_viewport->hide();
#endif

	connect_widgets();
	update_state();

	_canvas->grab_focus();

	// Idle callback, check if we need to refresh
	Glib::signal_timeout().connect(
			sigc::mem_fun(this, &Patchage::idle_callback), 100);
}

Patchage::~Patchage()
{
#if defined(PATCHAGE_LIBJACK) || defined(HAVE_JACK_DBUS)
	delete _jack_driver;
#endif
#ifdef HAVE_ALSA
	delete _alsa_driver;
#endif
#ifdef HAVE_LASH
	delete _lash_proxy;
	delete _project_list;
	delete _session;
	delete _dbus;
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

	update_statusbar();
}

bool
Patchage::idle_callback()
{
	// Initial run, attach
	if (_attach) {
		attach();
		_canvas->scroll_to_center();
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

	// Do a full refresh (ie user clicked refresh)
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

	flush_resize();

	// Update load every 10 idle callbacks
	static int count = 0;
	if (++count == 10) {
		update_load();
		count = 0;
	}

	return true;
}

void
Patchage::update_statusbar()
{
#if defined(PATCHAGE_LIBJACK) || defined(HAVE_JACK_DBUS)
	if (_enable_refresh && _jack_driver->is_attached()) {
		_enable_refresh = false;

		const jack_nframes_t buffer_size = _jack_driver->buffer_size();
		const jack_nframes_t sample_rate = _jack_driver->sample_rate();

		std::stringstream ss;
		ss << buffer_size;
		_latency_frames_label->set_text(ss.str());

		ss.str("");
		ss << (sample_rate / 1000);
		_sample_rate_label->set_text(ss.str());

		ss.str("");
		ss << buffer_size * 1000 / sample_rate;
		_latency_ms_label->set_text(ss.str());
		
		_enable_refresh = true;
	}
#endif
}

bool
Patchage::update_load()
{
#if defined(PATCHAGE_LIBJACK) || defined(HAVE_JACK_DBUS)
	if (!_jack_driver->is_attached())
		return true;

	char tmp_buf[8];
	snprintf(tmp_buf, sizeof(tmp_buf), "%zd", _jack_driver->get_xruns());

	_main_xrun_progress->set_text(string(tmp_buf) + " Dropouts");

	static float last_max_dsp_load = 0;

	const float max_dsp_load = _jack_driver->get_max_dsp_load();

	if (max_dsp_load != last_max_dsp_load) {
		_main_xrun_progress->set_fraction(max_dsp_load);
		last_max_dsp_load = max_dsp_load;
	}
#endif

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

		flush_resize();
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
#if defined(LOG_TO_STATUS)
	status_msg(msg);
#endif
#if defined(LOG_TO_STD)
	cerr << msg << endl;
#endif
}

void
Patchage::info_msg(const std::string& msg)
{
#if defined(LOG_TO_STATUS)
	status_msg(msg);
#endif
#if defined(LOG_TO_STD)
	cerr << msg << endl;
#endif
}

void
Patchage::status_msg(const string& msg)
{
	if (_status_text->get_buffer()->size() > 0)
		_status_text->get_buffer()->insert(_status_text->get_buffer()->end(), "\n");

	_status_text->get_buffer()->insert(_status_text->get_buffer()->end(), msg);
	_status_text->scroll_to_mark(_status_text->get_buffer()->get_insert(), 0);
}

void
Patchage::update_state()
{
	for (FlowCanvas::ItemList::iterator i = _canvas->items().begin(); i != _canvas->items().end(); ++i) {
		SharedPtr<FlowCanvas::Module> module = PtrCast<FlowCanvas::Module>(*i);
		if (module)
			module->load_location();
	}
}

/** Update the sensitivity status of menus to reflect the present.
 *
 * (eg. disable "Connect to Jack" when Patchage is already connected to Jack)
 */
void
Patchage::connect_widgets()
{
#if defined(PATCHAGE_LIBJACK) || defined(HAVE_JACK_DBUS)
	_jack_driver->signal_attached.connect(
			sigc::mem_fun(this, &Patchage::update_statusbar));
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

#ifdef HAVE_LASH
void
Patchage::show_load_project_dialog()
{
	std::list<ProjectInfo> projects;
	_lash_proxy->get_available_projects(projects);

	LoadProjectDialog dialog(this);
	dialog.run(projects);
}
#endif

#ifdef HAVE_LASH
void
Patchage::set_lash_available(bool available)
{
	_project_list->set_lash_available(available);
	if (!available) {
		_menu_view_projects->set_active(false);
		_session->clear();
	}
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
Patchage::on_zoom_full()
{
	_canvas->zoom_full();
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
	_menu_view_messages->set_active(false);
}

bool
Patchage::on_messages_delete(GdkEventAny*)
{
	_menu_view_messages->set_active(false);
	return true;
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
	if (_menu_view_messages->get_active())
		_messages_win->present();
	else
		_messages_win->hide();
}

void
Patchage::on_show_projects()
{
	if (_menu_view_projects->get_active())
		_project_list_viewport->show();
	else
		_project_list_viewport->hide();
}

void
Patchage::on_store_positions()
{
	store_window_location();
	_state_manager->save(_settings_filename);
}

void
Patchage::on_view_statusbar()
{
	if (_menu_view_statusbar->get_active())
		_statusbar->show();
	else
		_statusbar->hide();
}

bool
Patchage::on_scroll(GdkEventScroll* ev)
{
	return false;
}

void
Patchage::enqueue_resize(boost::shared_ptr<FlowCanvas::Module> module)
{
	if (module)
		_pending_resize.insert(module);
}

void
Patchage::flush_resize()
{
	for (std::set< boost::shared_ptr<FlowCanvas::Module> >::iterator i = _pending_resize.begin();
			i != _pending_resize.end(); ++i) {
		(*i)->resize();
	}

	_pending_resize.clear();
}

