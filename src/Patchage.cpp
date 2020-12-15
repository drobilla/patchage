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

#include "Patchage.hpp"

#include "AudioDriver.hpp"
#include "Canvas.hpp"
#include "CanvasModule.hpp"
#include "CanvasPort.hpp"
#include "Configuration.hpp"
#include "Driver.hpp"
#include "Event.hpp"
#include "Legend.hpp"
#include "PortID.hpp"
#include "UIFile.hpp"
#include "event_to_string.hpp"
#include "handle_event.hpp"
#include "make_alsa_driver.hpp"
#include "make_jack_driver.hpp"
#include "patchage_config.h" // IWYU pragma: keep
#include "warnings.hpp"

PATCHAGE_DISABLE_GANV_WARNINGS
#include "ganv/Edge.hpp"
#include "ganv/Module.hpp"
#include "ganv/Node.hpp"
#include "ganv/Port.hpp"
#include "ganv/module.h"
#include "ganv/types.h"
PATCHAGE_RESTORE_WARNINGS

PATCHAGE_DISABLE_FMT_WARNINGS
#include <fmt/core.h>
PATCHAGE_RESTORE_WARNINGS

#include <boost/optional/optional.hpp>
#include <glib-object.h>
#include <glib.h>
#include <glibmm/fileutils.h>
#include <glibmm/main.h>
#include <glibmm/miscutils.h>
#include <glibmm/propertyproxy.h>
#include <glibmm/signalproxy.h>
#include <glibmm/ustring.h>
#include <gobject/gclosure.h>
#include <gtk/gtk.h>
#include <gtkmm/aboutdialog.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/alignment.h>
#include <gtkmm/box.h>
#include <gtkmm/builder.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/combobox.h>
#include <gtkmm/dialog.h>
#include <gtkmm/enums.h>
#include <gtkmm/filechooser.h>
#include <gtkmm/filechooserdialog.h>
#include <gtkmm/filefilter.h>
#include <gtkmm/imagemenuitem.h>
#include <gtkmm/label.h>
#include <gtkmm/layout.h>
#include <gtkmm/liststore.h>
#include <gtkmm/menubar.h>
#include <gtkmm/menuitem.h>
#include <gtkmm/messagedialog.h>
#include <gtkmm/object.h>
#include <gtkmm/paned.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/stock.h>
#include <gtkmm/textbuffer.h>
#include <gtkmm/texttag.h>
#include <gtkmm/textview.h>
#include <gtkmm/toolbar.h>
#include <gtkmm/toolbutton.h>
#include <gtkmm/treeiter.h>
#include <gtkmm/treemodel.h>
#include <gtkmm/window.h>
#include <sigc++/adaptors/bind.h>
#include <sigc++/functors/mem_fun.h>
#include <sigc++/signal.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <utility>

#ifdef PATCHAGE_GTK_OSX

#	include <gtkosxapplication.h>

namespace {

gboolean
can_activate_cb(GtkWidget* widget, guint signal_id, gpointer data)
{
	return gtk_widget_is_sensitive(widget);
}

void
terminate_cb(GtkosxApplication* app, gpointer data)
{
	Patchage* patchage = (Patchage*)data;
	patchage->save();
	Gtk::Main::quit();
}

} // namespace

#endif

namespace patchage {

namespace {

bool
configure_cb(GtkWindow*, GdkEvent*, gpointer data)
{
	static_cast<Patchage*>(data)->store_window_location();
	return FALSE;
}

int
port_order(const GanvPort* a, const GanvPort* b, void*)
{
	const auto* pa = dynamic_cast<const CanvasPort*>(Glib::wrap(a));
	const auto* pb = dynamic_cast<const CanvasPort*>(Glib::wrap(b));
	if (pa && pb) {
		if (pa->order() && pb->order()) {
			return *pa->order() - *pb->order();
		}

		if (pa->order()) {
			return -1;
		}

		if (pb->order()) {
			return 1;
		}

		return pa->name().compare(pb->name());
	}
	return 0;
}

void
load_module_location(GanvNode* node, void*)
{
	if (GANV_IS_MODULE(node)) {
		Ganv::Module* gmod = Glib::wrap(GANV_MODULE(node));
		auto*         pmod = dynamic_cast<CanvasModule*>(gmod);
		if (pmod) {
			pmod->load_location();
		}
	}
}

} // namespace

#define INIT_WIDGET(x) x(_xml, (#x) + 1)

Patchage::Patchage(Options options)
    : _xml(UIFile::open("patchage"))
    , _gtk_main(nullptr)
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
    , INIT_WIDGET(_dropouts_label)
    , INIT_WIDGET(_buf_size_combo)
    , INIT_WIDGET(_latency_label)
    , INIT_WIDGET(_legend_alignment)
    , INIT_WIDGET(_main_paned)
    , INIT_WIDGET(_log_scrolledwindow)
    , INIT_WIDGET(_status_text)
    , _legend(nullptr)
    , _log(_status_text)
    , _connector(_log)
    , _options{options}
    , _pane_initialized(false)
    , _attach(true)
{
	_canvas =
	    std::unique_ptr<Canvas>{new Canvas(_connector, 1600 * 2, 1200 * 2)};

	Glib::set_application_name("Patchage");
	_about_win->property_program_name()   = "Patchage";
	_about_win->property_logo_icon_name() = "patchage";
	gtk_window_set_default_icon_name("patchage");

	// Create list model for buffer size selector
	Glib::RefPtr<Gtk::ListStore> buf_size_store =
	    Gtk::ListStore::create(_buf_size_columns);
	for (size_t i = 32; i <= 4096; i *= 2) {
		Gtk::TreeModel::Row row      = *(buf_size_store->append());
		row[_buf_size_columns.label] = std::to_string(i);
	}

	_buf_size_combo->set_model(buf_size_store);
	_buf_size_combo->pack_start(_buf_size_columns.label);

	_main_scrolledwin->add(_canvas->widget());

	_main_scrolledwin->property_hadjustment().get_value()->set_step_increment(
	    10);
	_main_scrolledwin->property_vadjustment().get_value()->set_step_increment(
	    10);

	_main_scrolledwin->signal_scroll_event().connect(
	    sigc::mem_fun(this, &Patchage::on_scroll));
	_clear_load_but->signal_clicked().connect(
	    sigc::mem_fun(this, &Patchage::clear_load));
	_buf_size_combo->signal_changed().connect(
	    sigc::mem_fun(this, &Patchage::buffer_size_changed));
	_status_text->signal_size_allocate().connect(
	    sigc::mem_fun(this, &Patchage::on_messages_resized));

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

	_canvas->widget().show();
	_main_win->present();

	_conf.set_font_size(_canvas->get_default_font_size());
	_conf.load();
	_canvas->set_zoom(_conf.get_zoom());
	_canvas->set_font_size(_conf.get_font_size());
	if (_conf.get_sort_ports()) {
		_canvas->set_port_order(port_order, nullptr);
	}

	_main_win->resize(static_cast<int>(_conf.get_window_size().x),
	                  static_cast<int>(_conf.get_window_size().y));

	_main_win->move(static_cast<int>(_conf.get_window_location().x),
	                static_cast<int>(_conf.get_window_location().y));

	_legend = new Legend(_conf);
	_legend->signal_color_changed.connect(
	    sigc::mem_fun(this, &Patchage::on_legend_color_change));
	_legend_alignment->add(*Gtk::manage(_legend));
	_legend->show_all();

	_about_win->set_transient_for(*_main_win);
#ifdef __APPLE__
	try {
		_about_win->set_logo(Gdk::Pixbuf::create_from_file(
		    bundle_location() + "/Resources/Patchage.icns"));
	} catch (const Glib::Exception& e) {
		_log.error(
		    fmt::format("Failed to set logo ({})", std::string(e.what())));
	}
#endif

	// Make Jack driver if possible
	_jack_driver = make_jack_driver(
	    _log, [this](const Event& event) { on_driver_event(event); });

	if (_jack_driver) {
		_connector.add_driver(PortID::Type::jack, _jack_driver.get());

		_menu_jack_connect->signal_activate().connect(sigc::bind(
		    sigc::mem_fun(_jack_driver.get(), &AudioDriver::attach), true));
		_menu_jack_disconnect->signal_activate().connect(
		    sigc::mem_fun(_jack_driver.get(), &AudioDriver::detach));
	} else {
		_menu_jack_connect->set_sensitive(false);
		_menu_jack_disconnect->set_sensitive(false);
	}

	// Make ALSA driver if possible
	_alsa_driver = make_alsa_driver(
	    _log, [this](const Event& event) { on_driver_event(event); });

	if (_alsa_driver) {
		_connector.add_driver(PortID::Type::alsa, _alsa_driver.get());

		_menu_alsa_connect->signal_activate().connect(sigc::bind(
		    sigc::mem_fun(_alsa_driver.get(), &Driver::attach), false));
		_menu_alsa_disconnect->signal_activate().connect(
		    sigc::mem_fun(_alsa_driver.get(), &Driver::detach));

	} else {
		_menu_alsa_connect->set_sensitive(false);
		_menu_alsa_disconnect->set_sensitive(false);
	}

	_canvas->for_each_node(load_module_location, nullptr);

	_menu_view_toolbar->set_active(_conf.get_show_toolbar());
	_menu_view_sprung_layout->set_active(_conf.get_sprung_layout());
	_menu_view_sort_ports->set_active(_conf.get_sort_ports());

	g_signal_connect(
	    _main_win->gobj(), "configure-event", G_CALLBACK(configure_cb), this);

	_canvas->widget().grab_focus();

	// Idle callback, check if we need to refresh
	Glib::signal_timeout().connect(
	    sigc::mem_fun(this, &Patchage::idle_callback), 100);

#ifdef PATCHAGE_GTK_OSX
	// Set up Mac menu bar
	GtkosxApplication* osxapp =
	    (GtkosxApplication*)g_object_new(GTKOSX_TYPE_APPLICATION, nullptr);
	_menubar->hide();
	_menu_file_quit->hide();
	gtkosx_application_set_menu_bar(osxapp, GTK_MENU_SHELL(_menubar->gobj()));
	gtkosx_application_insert_app_menu_item(
	    osxapp, GTK_WIDGET(_menu_help_about->gobj()), 0);
	g_signal_connect(_menubar->gobj(),
	                 "can-activate-accel",
	                 G_CALLBACK(can_activate_cb),
	                 nullptr);
	g_signal_connect(
	    osxapp, "NSApplicationWillTerminate", G_CALLBACK(terminate_cb), this);
	gtkosx_application_ready(osxapp);
#endif
}

Patchage::~Patchage()
{
	_jack_driver.reset();
	_alsa_driver.reset();
	_about_win.destroy();
	_xml.reset();
}

void
Patchage::attach()
{
	if (_jack_driver && _options.jack_driver_autoattach) {
		_jack_driver->attach(true);
	}

	if (_alsa_driver && _options.alsa_driver_autoattach) {
		_alsa_driver->attach(false);
	}

	process_events();
	refresh();
	update_toolbar();
}

bool
Patchage::idle_callback()
{
	// Initial run, attach
	if (_attach) {
		attach();
		_menu_view_messages->set_active(_conf.get_show_messages());
		_attach = false;
	}

	// Process any events from drivers
	process_events();

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
	}

	updating = true;

	if (_jack_driver && _jack_driver->is_attached()) {
		const auto buffer_size = _jack_driver->buffer_size();
		const auto sample_rate = _jack_driver->sample_rate();
		if (sample_rate != 0) {
			const auto latency_ms =
			    lrintf(buffer_size * 1000 / float(sample_rate));

			_latency_label->set_label(fmt::format(
			    " frames @ {} kHz ({} ms)", sample_rate / 1000, latency_ms));
			_latency_label->set_visible(true);
			_buf_size_combo->set_active(
			    static_cast<int>(log2f(_jack_driver->buffer_size()) - 5));
			updating = false;
			return;
		}
	}

	_latency_label->set_visible(false);
	updating = false;
}

bool
Patchage::update_load()
{
	if (_jack_driver && _jack_driver->is_attached()) {
		const auto xruns = _jack_driver->xruns();
		if (xruns > 0u) {
			_dropouts_label->set_text(fmt::format(" Dropouts: {}", xruns));
			_dropouts_label->show();
			_clear_load_but->show();
		} else {
			_dropouts_label->set_text(" Dropouts: 0");
			_dropouts_label->hide();
			_clear_load_but->hide();
		}
	}

	return true;
}

void
Patchage::refresh()
{
	auto sink = [this](const Event& event) {
		_log.info("Refresh: " + event_to_string(event));
		handle_event(*this, event);
	};

	if (_canvas) {
		_canvas->clear();

		if (_jack_driver) {
			_jack_driver->refresh(sink);
		}

		if (_alsa_driver) {
			_alsa_driver->refresh(sink);
		}
	}
}

void
Patchage::driver_attached(const ClientType type)
{
	switch (type) {
	case ClientType::jack:
		_menu_jack_connect->set_sensitive(false);
		_menu_jack_disconnect->set_sensitive(true);

		if (_jack_driver) {
			_jack_driver->refresh(
			    [this](const Event& event) { handle_event(*this, event); });
		}

		break;
	case ClientType::alsa:
		_menu_alsa_connect->set_sensitive(false);
		_menu_alsa_disconnect->set_sensitive(true);

		if (_alsa_driver) {
			_alsa_driver->refresh(
			    [this](const Event& event) { handle_event(*this, event); });
		}

		break;
	}
}

void
Patchage::driver_detached(const ClientType type)
{
	switch (type) {
	case ClientType::jack:
		_menu_jack_connect->set_sensitive(true);
		_menu_jack_disconnect->set_sensitive(false);

		_canvas->remove_ports([](const CanvasPort* port) {
			return (port->type() == PortType::jack_audio ||
			        port->type() == PortType::jack_midi ||
			        port->type() == PortType::jack_osc ||
			        port->type() == PortType::jack_cv);
		});

		break;

	case ClientType::alsa:
		_menu_alsa_connect->set_sensitive(true);
		_menu_alsa_disconnect->set_sensitive(false);

		_canvas->remove_ports([](const CanvasPort* port) {
			return port->type() == PortType::alsa_midi;
		});

		break;
	}
}

void
Patchage::store_window_location()
{
	int loc_x = 0;
	int loc_y = 0;
	_main_win->get_position(loc_x, loc_y);

	int size_x = 0;
	int size_y = 0;
	_main_win->get_size(size_x, size_y);

	_conf.set_window_location({double(loc_x), double(loc_y)});
	_conf.set_window_size({double(size_x), double(size_y)});
}

void
Patchage::clear_load()
{
	_dropouts_label->set_text(" Dropouts: 0");
	_dropouts_label->hide();
	_clear_load_but->hide();
	if (_jack_driver) {
		_jack_driver->reset_xruns();
	}
}

void
Patchage::on_driver_event(const Event& event)
{
	std::lock_guard<std::mutex> lock{_events_mutex};

	_driver_events.emplace(event);
}

void
Patchage::process_events()
{
	std::lock_guard<std::mutex> lock{_events_mutex};

	while (!_driver_events.empty()) {
		_log.info(event_to_string(_driver_events.front()));
		handle_event(*this, _driver_events.front());
		_driver_events.pop();
	}
}

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
	_conf.set_sprung_layout(sprung);
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
	const bool human_names = *static_cast<const bool*>(data);
	if (GANV_IS_MODULE(node)) {
		Ganv::Module* gmod = Glib::wrap(GANV_MODULE(node));
		auto*         pmod = dynamic_cast<CanvasModule*>(gmod);
		if (pmod) {
			for (Ganv::Port* gport : *gmod) {
				auto* pport = dynamic_cast<CanvasPort*>(gport);
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
	_canvas->set_port_order(sort_ports ? port_order : nullptr, nullptr);
	_conf.set_sort_ports(sort_ports);
	refresh();
}

void
Patchage::on_zoom_in()
{
	const float zoom = _canvas->get_zoom() * 1.25;
	_canvas->set_zoom(zoom);
	_conf.set_zoom(zoom);
}

void
Patchage::on_zoom_out()
{
	const float zoom = _canvas->get_zoom() * 0.75;
	_canvas->set_zoom(zoom);
	_conf.set_zoom(zoom);
}

void
Patchage::on_zoom_normal()
{
	_canvas->set_zoom(1.0);
	_conf.set_zoom(1.0);
}

void
Patchage::on_zoom_full()
{
	_canvas->zoom_full();
	_conf.set_zoom(_canvas->get_zoom());
}

void
Patchage::on_increase_font_size()
{
	const float points = _canvas->get_font_size() + 1.0;
	_canvas->set_font_size(points);
	_conf.set_font_size(points);
}

void
Patchage::on_decrease_font_size()
{
	const float points = _canvas->get_font_size() - 1.0;
	_canvas->set_font_size(points);
	_conf.set_font_size(points);
}

void
Patchage::on_normal_font_size()
{
	_canvas->set_font_size(_canvas->get_default_font_size());
	_conf.set_font_size(_canvas->get_default_font_size());
}

static inline guint
highlight_color(guint c, guint delta)
{
	const guint max_char = 255;
	const guint r        = MIN((c >> 24) + delta, max_char);
	const guint g        = MIN(((c >> 16) & 0xFF) + delta, max_char);
	const guint b        = MIN(((c >> 8) & 0xFF) + delta, max_char);
	const guint a        = c & 0xFF;

	return ((r << 24u) | (g << 16u) | (b << 8u) | a);
}

static void
update_port_colors(GanvNode* node, void* data)
{
	auto* patchage = static_cast<Patchage*>(data);
	if (!GANV_IS_MODULE(node)) {
		return;
	}

	Ganv::Module* gmod = Glib::wrap(GANV_MODULE(node));
	auto*         pmod = dynamic_cast<CanvasModule*>(gmod);
	if (!pmod) {
		return;
	}

	for (Ganv::Port* p : *pmod) {
		auto* port = dynamic_cast<CanvasPort*>(p);
		if (port) {
			const uint32_t rgba = patchage->conf().get_port_color(port->type());
			port->set_fill_color(rgba);
			port->set_border_color(highlight_color(rgba, 0x20));
		}
	}
}

static void
update_edge_color(GanvEdge* edge, void* data)
{
	auto*       patchage = static_cast<Patchage*>(data);
	Ganv::Edge* edgemm   = Glib::wrap(edge);

	auto* tail = dynamic_cast<CanvasPort*>((edgemm)->get_tail());
	if (tail) {
		edgemm->set_color(patchage->conf().get_port_color(tail->type()));
	}
}

void
Patchage::on_legend_color_change(PortType id, const std::string&, uint32_t rgba)
{
	_conf.set_port_color(id, rgba);
	_canvas->for_each_node(update_port_colors, this);
	_canvas->for_each_edge(update_edge_color, this);
}

void
Patchage::on_messages_resized(Gtk::Allocation&)
{
	const int max_pos = _main_paned->get_allocation().get_height();
	_conf.set_messages_height(max_pos - _main_paned->get_position());
}

void
Patchage::save()
{
	_conf.set_zoom(_canvas->get_zoom()); // Can be changed by ganv
	_conf.save();
}

void
Patchage::quit()
{
	_main_win->hide();
}

void
Patchage::on_quit()
{
	if (_alsa_driver) {
		_alsa_driver->detach();
	}

	if (_jack_driver) {
		_jack_driver->detach();
	}

	_main_win->hide();
}

void
Patchage::on_export_image()
{
	Gtk::FileChooserDialog dialog("Export Image",
	                              Gtk::FILE_CHOOSER_ACTION_SAVE);
	dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
	dialog.add_button(Gtk::Stock::SAVE, Gtk::RESPONSE_OK);
	dialog.set_default_response(Gtk::RESPONSE_OK);
	dialog.set_transient_for(*_main_win);

	using Types = std::map<std::string, std::string>;

	Types types;
	types["*.dot"] = "Graphviz DOT";
	types["*.pdf"] = "Portable Document Format";
	types["*.ps"]  = "PostScript";
	types["*.svg"] = "Scalable Vector Graphics";
	for (const auto& t : types) {
		Gtk::FileFilter filt;
		filt.add_pattern(t.first);
		filt.set_name(t.second);
		dialog.add_filter(filt);
	}

	auto* bg_but = new Gtk::CheckButton("Draw _Background", true);
	auto* extra  = new Gtk::Alignment(1.0, 0.5, 0.0, 0.0);
	bg_but->set_active(true);
	extra->add(*Gtk::manage(bg_but));
	extra->show_all();
	dialog.set_extra_widget(*Gtk::manage(extra));

	if (dialog.run() == Gtk::RESPONSE_OK) {
		const std::string filename = dialog.get_filename();
		if (Glib::file_test(filename, Glib::FILE_TEST_EXISTS)) {
			Gtk::MessageDialog confirm(std::string("File exists!  Overwrite ") +
			                               filename + "?",
			                           true,
			                           Gtk::MESSAGE_WARNING,
			                           Gtk::BUTTONS_YES_NO,
			                           true);
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
			const int min_height  = _log.min_height();
			const int max_pos     = _main_paned->get_allocation().get_height();
			const int conf_height = _conf.get_messages_height();
			_main_paned->set_position(max_pos -
			                          std::max(conf_height, min_height));

			_pane_initialized = true;
		}

		_log_scrolledwindow->show();
		_status_text->scroll_to_mark(_status_text->get_buffer()->get_insert(),
		                             0);
		_conf.set_show_messages(true);
	} else {
		_log_scrolledwindow->hide();
		_conf.set_show_messages(false);
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
	_conf.set_show_toolbar(_menu_view_toolbar->get_active());
}

bool
Patchage::on_scroll(GdkEventScroll*)
{
	return false;
}

void
Patchage::buffer_size_changed()
{
	if (_jack_driver) {
		const int selected = _buf_size_combo->get_active_row_number();

		if (selected == -1) {
			update_toolbar();
		} else {
			const uint32_t buffer_size = 1u << (selected + 5);
			_jack_driver->set_buffer_size(buffer_size);
			update_toolbar();
		}
	}
}

} // namespace patchage
