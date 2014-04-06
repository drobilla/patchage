/* This file is part of Patchage.
 * Copyright 2007-2013 David Robillard <http://drobilla.net>
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

#include <glib.h>
#include <glib/gstdio.h>
#include <gtk/gtkwindow.h>

#include <gtkmm/button.h>
#include <gtkmm/filechooserdialog.h>
#include <gtkmm/messagedialog.h>
#include <gtkmm/menuitem.h>
#include <gtkmm/stock.h>
#include <gtkmm/treemodel.h>

#include "ganv/Module.hpp"
#include "ganv/Edge.hpp"

#include "Configuration.hpp"
#include "Legend.hpp"
#include "Patchage.hpp"
#include "PatchageCanvas.hpp"
#include "PatchageEvent.hpp"
#include "UIFile.hpp"
#include "patchage_config.h"

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
	, _conf(NULL)
	, INIT_WIDGET(_about_win)
	, INIT_WIDGET(_main_scrolledwin)
	, INIT_WIDGET(_main_win)
	, INIT_WIDGET(_main_vbox)
	, INIT_WIDGET(_menubar)
	, INIT_WIDGET(_menu_alsa_connect)
	, INIT_WIDGET(_menu_alsa_disconnect)
	, INIT_WIDGET(_menu_file_quit)
	, INIT_WIDGET(_menu_export_dot)
	, INIT_WIDGET(_menu_help_about)
	, INIT_WIDGET(_menu_jack_connect)
	, INIT_WIDGET(_menu_jack_disconnect)
	, INIT_WIDGET(_menu_open_session)
	, INIT_WIDGET(_menu_save_session)
	, INIT_WIDGET(_menu_save_close_session)
	, INIT_WIDGET(_menu_view_arrange)
	, INIT_WIDGET(_menu_view_messages)
	, INIT_WIDGET(_menu_view_toolbar)
	, INIT_WIDGET(_menu_view_refresh)
	, INIT_WIDGET(_menu_view_human_names)
	, INIT_WIDGET(_menu_zoom_in)
	, INIT_WIDGET(_menu_zoom_out)
	, INIT_WIDGET(_menu_zoom_normal)
	, INIT_WIDGET(_menu_zoom_full)
	, INIT_WIDGET(_menu_increase_font_size)
	, INIT_WIDGET(_menu_decrease_font_size)
	, INIT_WIDGET(_menu_normal_font_size)
	, INIT_WIDGET(_toolbar)
	, INIT_WIDGET(_clear_load_but)
	, INIT_WIDGET(_xrun_progress)
	, INIT_WIDGET(_latency_label)
	, INIT_WIDGET(_legend_alignment)
	, INIT_WIDGET(_main_paned)
	, INIT_WIDGET(_log_scrolledwindow)
	, INIT_WIDGET(_status_text)
	, _legend(NULL)
	, _attach(true)
	, _driver_detached(false)
	, _refresh(false)
	, _enable_refresh(true)
	, _jack_driver_autoattach(true)
#ifdef HAVE_ALSA
	, _alsa_driver_autoattach(true)
#endif
{
	_conf   = new Configuration();
	_canvas = boost::shared_ptr<PatchageCanvas>(new PatchageCanvas(this, 1600*2, 1200*2));

	while (argc > 0) {
		if (!strcmp(*argv, "-h") || !strcmp(*argv, "--help")) {
			cout << "Usage: patchage [OPTION]..." << endl;
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

	_clear_load_but->signal_clicked().connect(
		sigc::mem_fun(this, &Patchage::clear_load));

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

	_menu_file_quit->signal_activate().connect(
			sigc::mem_fun(this, &Patchage::on_quit));
	_menu_export_dot->signal_activate().connect(
			sigc::mem_fun(this, &Patchage::on_export_dot));
	_menu_view_refresh->signal_activate().connect(
			sigc::mem_fun(this, &Patchage::refresh));
	_menu_view_human_names->signal_activate().connect(
			sigc::mem_fun(this, &Patchage::on_view_human_names));
	_menu_view_arrange->signal_activate().connect(
			sigc::mem_fun(this, &Patchage::on_arrange));
	_menu_view_messages->signal_activate().connect(
			sigc::mem_fun(this, &Patchage::on_view_messages));
	_menu_view_toolbar->signal_activate().connect(
			sigc::mem_fun(this, &Patchage::on_view_toolbar));
	_menu_help_about->signal_activate().connect(
			sigc::mem_fun(this, &Patchage::on_help_about));
	_menu_zoom_in->signal_activate().connect(
			sigc::mem_fun(this, &Patchage::on_zoom_in));
	_menu_zoom_out->signal_activate().connect(
			sigc::mem_fun(this, &Patchage::on_zoom_out));
	_menu_zoom_normal->signal_activate().connect(
			sigc::mem_fun(this, &Patchage::on_zoom_normal));
	_menu_zoom_full->signal_activate().connect(
			sigc::mem_fun(this, &Patchage::on_zoom_full));
	_menu_increase_font_size->signal_activate().connect(
			sigc::mem_fun(this, &Patchage::on_increase_font_size));
	_menu_decrease_font_size->signal_activate().connect(
			sigc::mem_fun(this, &Patchage::on_decrease_font_size));
	_menu_normal_font_size->signal_activate().connect(
			sigc::mem_fun(this, &Patchage::on_normal_font_size));

	_error_tag = Gtk::TextTag::create();
	_error_tag->property_foreground() = "#CC0000";
	_status_text->get_buffer()->get_tag_table()->add(_error_tag);

	_warning_tag = Gtk::TextTag::create();
	_warning_tag->property_foreground() = "#E6B200";
	_status_text->get_buffer()->get_tag_table()->add(_warning_tag);

	_canvas->widget().show();
	_main_win->present();

	_conf->set_font_size(_canvas->get_default_font_size());
	_conf->load();
	_canvas->set_zoom(_conf->get_zoom());
	_canvas->set_font_size(_conf->get_font_size());

	_main_win->resize(
		static_cast<int>(_conf->get_window_size().x),
		static_cast<int>(_conf->get_window_size().y));

	_main_win->move(
		static_cast<int>(_conf->get_window_location().x),
		static_cast<int>(_conf->get_window_location().y));

	_legend = new Legend(*_conf);
	_legend->signal_color_changed.connect(
		sigc::mem_fun(this, &Patchage::on_legend_color_change));
	_legend_alignment->add(*Gtk::manage(_legend));
	_legend->show_all();

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
	_menu_view_toolbar->set_active(_conf->get_show_toolbar());
	_main_paned->set_position(42);

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
	store_window_location();
	_conf->set_zoom(_canvas->get_zoom());  // Can be changed by ganv
	_conf->save();

#if defined(PATCHAGE_LIBJACK) || defined(HAVE_JACK_DBUS)
	delete _jack_driver;
#endif
#ifdef HAVE_ALSA
	delete _alsa_driver;
#endif

	delete _conf;

	_about_win.destroy();
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
	update_toolbar();
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

	// Update load every 5 idle callbacks
	static int count = 0;
	if (++count == 5) {
		update_load();
		count = 0;
	}

	return true;
}

void
Patchage::update_toolbar()
{
#if defined(PATCHAGE_LIBJACK) || defined(HAVE_JACK_DBUS)
	if (_jack_driver->is_attached()) {
		const jack_nframes_t buffer_size = _jack_driver->buffer_size();
		const jack_nframes_t sample_rate = _jack_driver->sample_rate();
		if (sample_rate != 0) {
			const int latency_ms = lrintf(buffer_size * 1000 / (float)sample_rate);
			std::stringstream ss;
			ss << buffer_size << " frames @ "
			   << (sample_rate / 1000) << "kHz (" << latency_ms << "ms)";
			_latency_label->set_label(ss.str());
			_latency_label->set_visible(true);
			return;
		}
	}
#endif
	_latency_label->set_visible(false);
}

bool
Patchage::update_load()
{
#if defined(PATCHAGE_LIBJACK) || defined(HAVE_JACK_DBUS)
	if (_jack_driver->is_attached()) {
		char buf[8];
		snprintf(buf, sizeof(buf), "%u", _jack_driver->get_xruns());
		_xrun_progress->set_text(std::string(buf) + " Dropouts");
		_xrun_progress->set_fraction(_jack_driver->get_max_dsp_load());
	}
#endif

	return true;
}

void
Patchage::zoom(double z)
{
	_conf->set_zoom(z);
	_canvas->set_zoom(z);
}

void
Patchage::refresh()
{
	if (_canvas && _enable_refresh) {
		_canvas->clear();

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
	_conf->set_window_location(window_location);
	_conf->set_window_size(window_size);
}

void
Patchage::clear_load()
{
#if defined(PATCHAGE_LIBJACK) || defined(HAVE_JACK_DBUS)
	_xrun_progress->set_fraction(0.0);
	_jack_driver->reset_xruns();
	_jack_driver->reset_max_dsp_load();
#endif
}

void
Patchage::error_msg(const std::string& msg)
{
	Glib::RefPtr<Gtk::TextBuffer> buffer = _status_text->get_buffer();
	buffer->insert_with_tag(buffer->end(), std::string("\n") + msg, _error_tag);
	_status_text->scroll_to_mark(buffer->get_insert(), 0);
	_menu_view_messages->set_active(true);
}

void
Patchage::info_msg(const std::string& msg)
{
	Glib::RefPtr<Gtk::TextBuffer> buffer = _status_text->get_buffer();
	buffer->insert(buffer->end(), std::string("\n") + msg);
	_status_text->scroll_to_mark(buffer->get_insert(), 0);
}

void
Patchage::warning_msg(const std::string& msg)
{
	Glib::RefPtr<Gtk::TextBuffer> buffer = _status_text->get_buffer();
	buffer->insert_with_tag(buffer->end(), std::string("\n") + msg, _warning_tag);
	_status_text->scroll_to_mark(buffer->get_insert(), 0);
}

static void
load_module_location(GanvNode* node, void* data)
{
	if (GANV_IS_MODULE(node)) {
		Ganv::Module*   gmod = Glib::wrap(GANV_MODULE(node));
		PatchageModule* pmod = dynamic_cast<PatchageModule*>(gmod);
		if (pmod) {
			pmod->load_location();
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

static void
update_labels(GanvNode* node, void* data)
{
	const bool human_names = *(const bool*)data;
	if (GANV_IS_MODULE(node)) {
		Ganv::Module*   gmod = Glib::wrap(GANV_MODULE(node));
		PatchageModule* pmod = dynamic_cast<PatchageModule*>(gmod);
		if (pmod) {
			for (Ganv::Port* gport : *gmod) {
				PatchagePort* pport = dynamic_cast<PatchagePort*>(gport);
				if (pport) {
					pport->show_human_name(human_names);
				}
			}
		}
	}
}

void
Patchage::on_view_human_names()
{
	bool human_names = show_human_names();
	_canvas->for_each_node(update_labels, &human_names);
}

void
Patchage::on_zoom_in()
{
	const float zoom = _canvas->get_zoom() * 1.25;
	_canvas->set_zoom(zoom);
	_conf->set_zoom(zoom);
}

void
Patchage::on_zoom_out()
{
	const float zoom = _canvas->get_zoom() * 0.75;
	_canvas->set_zoom(zoom);
	_conf->set_zoom(zoom);
}

void
Patchage::on_zoom_normal()
{
	_canvas->set_zoom(1.0);
	_conf->set_zoom(1.0);
}

void
Patchage::on_zoom_full()
{
	_canvas->zoom_full();
	_conf->set_zoom(_canvas->get_zoom());
}

void
Patchage::on_increase_font_size()
{
	const float points = _canvas->get_font_size() + 1.0;
	_canvas->set_font_size(points);
	_conf->set_font_size(points);
}

void
Patchage::on_decrease_font_size()
{
	const float points = _canvas->get_font_size() - 1.0;
	_canvas->set_font_size(points);
	_conf->set_font_size(points);
}

void
Patchage::on_normal_font_size()
{
	_canvas->set_font_size(_canvas->get_default_font_size());
	_conf->set_font_size(_canvas->get_default_font_size());
}

static inline guint
highlight_color(guint c, guint delta)
{
	const guint max_char = 255;
	const guint r        = MIN((c >> 24) + delta, max_char);
	const guint g        = MIN(((c >> 16) & 0xFF) + delta, max_char);
	const guint b        = MIN(((c >> 8) & 0xFF) + delta, max_char);
	const guint a        = c & 0xFF;

	return ((((guint)(r)) << 24) |
	        (((guint)(g)) << 16) |
	        (((guint)(b)) << 8) |
	        (((guint)(a))));
}

static void
update_port_colors(GanvNode* node, void* data)
{
	Patchage* patchage = (Patchage*)data;
	if (!GANV_IS_MODULE(node)) {
		return;
	}

	Ganv::Module*   gmod = Glib::wrap(GANV_MODULE(node));
	PatchageModule* pmod = dynamic_cast<PatchageModule*>(gmod);
	if (!pmod) {
		return;
	}

	for (PatchageModule::iterator i = pmod->begin(); i != pmod->end(); ++i) {
		PatchagePort* port = dynamic_cast<PatchagePort*>(*i);
		if (port) {
			const uint32_t rgba = patchage->conf()->get_port_color(port->type());
			port->set_fill_color(rgba);
			port->set_border_color(highlight_color(rgba, 0x20));
		}
	}
}

static void
update_edge_color(GanvEdge* edge, void* data)
{
	Patchage*   patchage = (Patchage*)data;
	Ganv::Edge* edgemm   = Glib::wrap(edge);

	PatchagePort* tail = dynamic_cast<PatchagePort*>((edgemm)->get_tail());
	if (tail) {
		edgemm->set_color(patchage->conf()->get_port_color(tail->type()));
	}
}

void
Patchage::on_legend_color_change(int id, const std::string& label, uint32_t rgba)
{
	_conf->set_port_color((PortType)id, rgba);
	_canvas->for_each_node(update_port_colors, this);
	_canvas->for_each_edge(update_edge_color, this);
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
Patchage::on_export_dot()
{
	Gtk::FileChooserDialog dialog("Export to DOT", Gtk::FILE_CHOOSER_ACTION_SAVE);
	dialog.set_transient_for(*_main_win);
	dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);

	Gtk::Button* save_button = dialog.add_button(Gtk::Stock::SAVE, Gtk::RESPONSE_OK);
	save_button->property_has_default() = true;

	if (dialog.run() == Gtk::RESPONSE_OK) {
		std::string filename = dialog.get_filename();
		if (filename.find(".") == std::string::npos)
			filename += ".dot";

		if (Glib::file_test(filename, Glib::FILE_TEST_EXISTS)) {
			Gtk::MessageDialog dialog(
				std::string("File exists!  Overwrite ") + filename + "?",
				true, Gtk::MESSAGE_WARNING, Gtk::BUTTONS_YES_NO, true);
			dialog.set_transient_for(*_main_win);
			if (dialog.run() != Gtk::RESPONSE_YES) {
				return;
			}
		}

		_canvas->export_dot(filename.c_str());
	}
}

void
Patchage::on_view_messages()
{
	if (_menu_view_messages->get_active()) {
		_log_scrolledwindow->show();
	} else {
		_log_scrolledwindow->hide();
	}
}

void
Patchage::on_view_toolbar()
{
	if (_menu_view_toolbar->get_active()) {
		_toolbar->show();
	} else {
		_toolbar->hide();
	}
	_conf->set_show_toolbar(_menu_view_toolbar->get_active());
}

bool
Patchage::on_scroll(GdkEventScroll* ev)
{
	return false;
}
