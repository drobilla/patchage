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

#include <stdlib.h>
#include <pthread.h>

#include <cmath>
#include <fstream>

#include <glib.h>
#include <glib/gstdio.h>
#include <gtk/gtkwindow.h>

#include <boost/format.hpp>

#include <gtkmm/button.h>
#include <gtkmm/filechooserdialog.h>
#include <gtkmm/liststore.h>
#include <gtkmm/menuitem.h>
#include <gtkmm/messagedialog.h>
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

#ifdef HAVE_ALSA
    #include "AlsaDriver.hpp"
#endif

#ifdef PATCHAGE_GTK_OSX
    #include <gtkosxapplication.h>

static gboolean
can_activate_cb(GtkWidget* widget, guint signal_id, gpointer data)
{
  return gtk_widget_is_sensitive(widget);
}

static void
terminate_cb(GtkosxApplication* app, gpointer data)
{
	Patchage* patchage = (Patchage*)data;
	patchage->save();
	Gtk::Main::quit();
}

#endif

static bool
configure_cb(GtkWindow* parentWindow, GdkEvent* event, gpointer data)
{
	((Patchage*)data)->store_window_location();
	return FALSE;
}

static int
port_order(const GanvPort* a, const GanvPort* b, void* data)
{
	const PatchagePort* pa = dynamic_cast<const PatchagePort*>(Glib::wrap(a));
	const PatchagePort* pb = dynamic_cast<const PatchagePort*>(Glib::wrap(b));
	if (pa && pb) {
		if (pa->order() && pb->order()) {
			return *pa->order() - *pb->order();
		} else if (pa->order()) {
			return -1;
		} else if (pb->order()) {
			return 1;
		}
		return pa->name().compare(pb->name());
	}
	return 0;
}

struct ProjectList_column_record : public Gtk::TreeModel::ColumnRecord {
	Gtk::TreeModelColumn<Glib::ustring> label;
};

using std::cout;
using std::endl;
using std::string;

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
	, INIT_WIDGET(_menu_export_image)
	, INIT_WIDGET(_menu_help_about)
	, INIT_WIDGET(_menu_jack_connect)
	, INIT_WIDGET(_menu_jack_disconnect)
	, INIT_WIDGET(_menu_open_session)
	, INIT_WIDGET(_menu_save_session)
	, INIT_WIDGET(_menu_save_close_session)
	, INIT_WIDGET(_menu_view_arrange)
	, INIT_WIDGET(_menu_view_sprung_layout)
	, INIT_WIDGET(_menu_view_messages)
	, INIT_WIDGET(_menu_view_toolbar)
	, INIT_WIDGET(_menu_view_refresh)
	, INIT_WIDGET(_menu_view_human_names)
	, INIT_WIDGET(_menu_view_sort_ports)
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
	, INIT_WIDGET(_buf_size_combo)
	, INIT_WIDGET(_latency_label)
	, INIT_WIDGET(_legend_alignment)
	, INIT_WIDGET(_main_paned)
	, INIT_WIDGET(_log_scrolledwindow)
	, INIT_WIDGET(_status_text)
	, _legend(NULL)
	, _pane_initialized(false)
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

	// Create list model for buffer size selector
	Glib::RefPtr<Gtk::ListStore> buf_size_store = Gtk::ListStore::create(_buf_size_columns);
	for (size_t i = 32; i <= 4096; i *= 2) {
		Gtk::TreeModel::Row row = *(buf_size_store->append());
		row[_buf_size_columns.label] = std::to_string(i);
	}

	_buf_size_combo->set_model(buf_size_store);
	_buf_size_combo->pack_start(_buf_size_columns.label);

	_main_scrolledwin->add(_canvas->widget());

	_main_scrolledwin->property_hadjustment().get_value()->set_step_increment(10);
	_main_scrolledwin->property_vadjustment().get_value()->set_step_increment(10);

	_main_scrolledwin->signal_scroll_event().connect(
		sigc::mem_fun(this, &Patchage::on_scroll));
	_clear_load_but->signal_clicked().connect(
		sigc::mem_fun(this, &Patchage::clear_load));
	_buf_size_combo->signal_changed().connect(
		sigc::mem_fun(this, &Patchage::buffer_size_changed));
	_status_text->signal_size_allocate().connect(
		sigc::mem_fun(this, &Patchage::on_messages_resized));

#ifdef PATCHAGE_JACK_SESSION
	_menu_open_session->signal_activate().connect(
		sigc::mem_fun(this, &Patchage::show_open_session_dialog));
	_menu_save_session->signal_activate().connect(
		sigc::mem_fun(this, &Patchage::show_save_session_dialog));
	_menu_save_close_session->signal_activate().connect(
		sigc::mem_fun(this, &Patchage::show_save_close_session_dialog));
#else
	_menu_open_session->hide();
	_menu_save_session->hide();
	_menu_save_close_session->hide();
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
	_menu_export_image->signal_activate().connect(
			sigc::mem_fun(this, &Patchage::on_export_image));
	_menu_view_refresh->signal_activate().connect(
			sigc::mem_fun(this, &Patchage::refresh));
	_menu_view_human_names->signal_activate().connect(
			sigc::mem_fun(this, &Patchage::on_view_human_names));
	_menu_view_sort_ports->signal_activate().connect(
			sigc::mem_fun(this, &Patchage::on_view_sort_ports));
	_menu_view_arrange->signal_activate().connect(
			sigc::mem_fun(this, &Patchage::on_arrange));
	_menu_view_sprung_layout->signal_activate().connect(
			sigc::mem_fun(this, &Patchage::on_sprung_layout_toggled));
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

	if (_canvas->supports_sprung_layout()) {
		_menu_view_sprung_layout->set_active(true);
	} else {
		_menu_view_sprung_layout->set_active(false);
		_menu_view_sprung_layout->set_sensitive(false);
	}

	for (int s = Gtk::STATE_NORMAL; s <= Gtk::STATE_INSENSITIVE; ++s) {
		_status_text->modify_base((Gtk::StateType)s, Gdk::Color("#000000"));
		_status_text->modify_text((Gtk::StateType)s, Gdk::Color("#FFFFFF"));
	}

	_error_tag = Gtk::TextTag::create();
	_error_tag->property_foreground() = "#CC0000";
	_status_text->get_buffer()->get_tag_table()->add(_error_tag);

	_warning_tag = Gtk::TextTag::create();
	_warning_tag->property_foreground() = "#C4A000";
	_status_text->get_buffer()->get_tag_table()->add(_warning_tag);

	_canvas->widget().show();
	_main_win->present();

	_conf->set_font_size(_canvas->get_default_font_size());
	_conf->load();
	_canvas->set_zoom(_conf->get_zoom());
	_canvas->set_font_size(_conf->get_font_size());
	if (_conf->get_sort_ports()) {
		_canvas->set_port_order(port_order, NULL);
	}

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
#ifdef __APPLE__
	try {
		_about_win->set_logo(
			Gdk::Pixbuf::create_from_file(
				bundle_location() + "/Resources/Patchage.icns"));
	} catch (const Glib::Exception& e) {
		error_msg((boost::format("failed to set logo (%s)") % e.what()).str());
	}
#endif

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
	_menu_view_sprung_layout->set_active(_conf->get_sprung_layout());
	_menu_view_sort_ports->set_active(_conf->get_sort_ports());
	_status_text->set_pixels_inside_wrap(2);
	_status_text->set_left_margin(4);
	_status_text->set_right_margin(4);
	_status_text->set_pixels_below_lines(2);

	g_signal_connect(_main_win->gobj(), "configure-event",
	                 G_CALLBACK(configure_cb), this);

	_canvas->widget().grab_focus();

	// Idle callback, check if we need to refresh
	Glib::signal_timeout().connect(
			sigc::mem_fun(this, &Patchage::idle_callback), 100);

#ifdef PATCHAGE_GTK_OSX
	// Set up Mac menu bar
	GtkosxApplication* osxapp = (GtkosxApplication*)g_object_new(
		GTKOSX_TYPE_APPLICATION, NULL);
	_menubar->hide();
	_menu_file_quit->hide();
	gtkosx_application_set_menu_bar(osxapp, GTK_MENU_SHELL(_menubar->gobj()));
	gtkosx_application_insert_app_menu_item(
		osxapp, GTK_WIDGET(_menu_help_about->gobj()), 0);
	g_signal_connect(_menubar->gobj(), "can-activate-accel",
	                 G_CALLBACK(can_activate_cb), NULL);
	g_signal_connect(osxapp, "NSApplicationWillTerminate",
	                 G_CALLBACK(terminate_cb), this);
	gtkosx_application_ready(osxapp);
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
		_menu_view_messages->set_active(_conf->get_show_messages());
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
	static bool updating = false;
	if (updating) {
		return;
	} else {
		updating = true;
	}

#if defined(PATCHAGE_LIBJACK) || defined(HAVE_JACK_DBUS)
	if (_jack_driver->is_attached()) {
		const jack_nframes_t buffer_size = _jack_driver->buffer_size();
		const jack_nframes_t sample_rate = _jack_driver->sample_rate();
		if (sample_rate != 0) {
			const int latency_ms = lrintf(buffer_size * 1000 / (float)sample_rate);
			std::stringstream ss;
			ss << " frames @ " << (sample_rate / 1000)
			   << "kHz (" << latency_ms << "ms)";
			_latency_label->set_label(ss.str());
			_latency_label->set_visible(true);
			_buf_size_combo->set_active((int)log2f(_jack_driver->buffer_size()) - 5);
			updating = false;
			return;
		}
	}
#endif
	_latency_label->set_visible(false);
	updating = false;
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
	if (_canvas) {
		_canvas->arrange();
	}
}

void
Patchage::on_sprung_layout_toggled()
{
	const bool sprung = _menu_view_sprung_layout->get_active();

	_canvas->set_sprung_layout(sprung);
	_conf->set_sprung_layout(sprung);
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
Patchage::on_view_sort_ports()
{
	const bool sort_ports = this->sort_ports();
	_canvas->set_port_order(sort_ports ? port_order : NULL, NULL);
	_conf->set_sort_ports(sort_ports);
	refresh();
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
Patchage::on_messages_resized(Gtk::Allocation& alloc)
{
	const int max_pos = _main_paned->get_allocation().get_height();
	_conf->set_messages_height(max_pos - _main_paned->get_position());
}

void
Patchage::save()
{
	_conf->set_zoom(_canvas->get_zoom());  // Can be changed by ganv
	_conf->save();
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
}

void
Patchage::on_export_image()
{
	Gtk::FileChooserDialog dialog("Export Image", Gtk::FILE_CHOOSER_ACTION_SAVE);
	dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
	dialog.add_button(Gtk::Stock::SAVE, Gtk::RESPONSE_OK);
	dialog.set_default_response(Gtk::RESPONSE_OK);
	dialog.set_transient_for(*_main_win);

	typedef std::map<std::string, std::string> Types;
	Types types;
	types["*.dot"] = "Graphviz DOT";
	types["*.pdf"] = "Portable Document Format";
	types["*.ps"]  = "PostScript";
	types["*.svg"] = "Scalable Vector Graphics";
	for (Types::const_iterator t = types.begin(); t != types.end(); ++t) {
		Gtk::FileFilter filt;
		filt.add_pattern(t->first);
		filt.set_name(t->second);
		dialog.add_filter(filt);
	}

	Gtk::CheckButton* bg_but = new Gtk::CheckButton("Draw _Background", true);
	Gtk::Alignment*   extra  = new Gtk::Alignment(1.0, 0.5, 0.0, 0.0);
	bg_but->set_active(true);
	extra->add(*Gtk::manage(bg_but));
	extra->show_all();
	dialog.set_extra_widget(*Gtk::manage(extra));

	if (dialog.run() == Gtk::RESPONSE_OK) {
		const std::string filename = dialog.get_filename();
		if (Glib::file_test(filename, Glib::FILE_TEST_EXISTS)) {
			Gtk::MessageDialog confirm(
				std::string("File exists!  Overwrite ") + filename + "?",
				true, Gtk::MESSAGE_WARNING, Gtk::BUTTONS_YES_NO, true);
			confirm.set_transient_for(dialog);
			if (confirm.run() != Gtk::RESPONSE_YES) {
				return;
			}
		}
		_canvas->export_image(filename.c_str(), bg_but->get_active());
	}
}

void
Patchage::on_view_messages()
{
	if (_menu_view_messages->get_active()) {
		Glib::RefPtr<Gtk::TextBuffer> buffer = _status_text->get_buffer();
		if (!_pane_initialized) {
			int y, line_height;
			_status_text->get_line_yrange(buffer->begin(), y, line_height);
			const int pad         = _status_text->get_pixels_inside_wrap();
			const int max_pos     = _main_paned->get_allocation().get_height();
			const int min_height  = (line_height + 2 * pad);
			const int conf_height = _conf->get_messages_height();
			_main_paned->set_position(max_pos - std::max(conf_height, min_height));
			_pane_initialized = true;
		}

		_log_scrolledwindow->show();
		_status_text->scroll_to_mark(
			_status_text->get_buffer()->get_insert(), 0);
		_conf->set_show_messages(true);
	} else {
		_log_scrolledwindow->hide();
		_conf->set_show_messages(false);
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

void
Patchage::buffer_size_changed()
{
#if defined(HAVE_JACK) || defined(HAVE_JACK_DBUS)
	const int selected = _buf_size_combo->get_active_row_number();

	if (selected == -1) {
		update_toolbar();
	} else {
		const jack_nframes_t buffer_size = 1 << (selected + 5);
		_jack_driver->set_buffer_size(buffer_size);
		update_toolbar();
	}
#endif
}

