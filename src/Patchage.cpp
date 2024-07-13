// Copyright 2007-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Patchage.hpp"

#include "Action.hpp"
#include "AudioDriver.hpp"
#include "Canvas.hpp"
#include "CanvasModule.hpp"
#include "CanvasPort.hpp"
#include "Configuration.hpp"
#include "Coord.hpp"
#include "Driver.hpp"
#include "Drivers.hpp"
#include "Event.hpp"
#include "Legend.hpp"
#include "Options.hpp"
#include "PortType.hpp"
#include "Reactor.hpp"
#include "Setting.hpp"
#include "TextViewLog.hpp"
#include "UIFile.hpp"
#include "Widget.hpp"
#include "event_to_string.hpp"
#include "handle_event.hpp"
#include "i18n.hpp"
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
#include <gtkmm/checkmenuitem.h>
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
#include <sigc++/functors/ptr_fun.h>
#include <sigc++/functors/slot.h>
#include <sigc++/signal.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <map>
#include <optional>
#include <utility>
#include <variant>

#ifdef PATCHAGE_GTK_OSX

#  include <gtkmm/main.h>
#  include <gtkosxapplication.h>

namespace {

gboolean
can_activate_cb(GtkWidget* widget, guint, gpointer)
{
  return gtk_widget_is_sensitive(widget);
}

void
terminate_cb(GtkosxApplication*, gpointer data)
{
  auto* patchage = static_cast<patchage::Patchage*>(data);
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
    const auto oa = pa->order();
    const auto ob = pb->order();
    if (oa && ob) {
      return *oa - *ob;
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

template<class S>
void
on_setting_toggled(Reactor* const reactor, const Gtk::CheckMenuItem* const item)
{
  (*reactor)(action::ChangeSetting{{S{item->get_active()}}});
}

void
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

inline guint
highlight_color(guint c, guint delta)
{
  const guint max_char = 255;
  const guint r        = MIN((c >> 24) + delta, max_char);
  const guint g        = MIN(((c >> 16) & 0xFF) + delta, max_char);
  const guint b        = MIN(((c >> 8) & 0xFF) + delta, max_char);
  const guint a        = c & 0xFF;

  return ((r << 24u) | (g << 16u) | (b << 8u) | a);
}

void
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

void
update_edge_color(GanvEdge* edge, void* data)
{
  auto*       patchage = static_cast<Patchage*>(data);
  Ganv::Edge* edgemm   = Glib::wrap(edge);

  if (edgemm) {
    auto* tail = dynamic_cast<CanvasPort*>((edgemm)->get_tail());
    if (tail) {
      edgemm->set_color(patchage->conf().get_port_color(tail->type()));
    }
  }
}

} // namespace

#define INIT_WIDGET(x) x(_xml, (#x) + 1)

Patchage::Patchage(Options options)
  : _xml(UIFile::open("patchage"))
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
  , _conf([this](const Setting& setting) { on_conf_change(setting); })
  , _log(_status_text)
  , _canvas(new Canvas{_log, _action_sink, 1600 * 2, 1200 * 2})
  , _drivers(_log, [this](const Event& event) { on_driver_event(event); })
  , _reactor(_conf, _drivers, *_canvas, _log)
  , _action_sink([this](const Action& action) { _reactor(action); })
  , _options{options}
{
  Glib::set_application_name("Patchage");
  _about_win->property_program_name()   = "Patchage";
  _about_win->property_logo_icon_name() = "patchage";
  gtk_window_set_default_icon_name("patchage");

  // Create list model for buffer size selector
  const Glib::RefPtr<Gtk::ListStore> buf_size_store =
    Gtk::ListStore::create(_buf_size_columns);
  for (size_t i = 32; i <= 4096; i *= 2) {
    const Gtk::TreeModel::Row row = *(buf_size_store->append());
    row[_buf_size_columns.label]  = std::to_string(i);
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

  _menu_file_quit->signal_activate().connect(
    sigc::mem_fun(this, &Patchage::on_quit));
  _menu_export_image->signal_activate().connect(
    sigc::mem_fun(this, &Patchage::on_export_image));
  _menu_view_refresh->signal_activate().connect(sigc::bind(
    sigc::mem_fun(this, &Patchage::on_menu_action), Action{action::Refresh{}}));

  _menu_view_human_names->signal_activate().connect(
    sigc::bind(sigc::ptr_fun(&on_setting_toggled<setting::HumanNames>),
               &_reactor,
               _menu_view_human_names.get()));

  _menu_view_sort_ports->signal_activate().connect(
    sigc::bind(sigc::ptr_fun(&on_setting_toggled<setting::SortedPorts>),
               &_reactor,
               _menu_view_sort_ports.get()));

  _menu_view_arrange->signal_activate().connect(
    sigc::mem_fun(this, &Patchage::on_arrange));

  _menu_view_sprung_layout->signal_activate().connect(
    sigc::bind(sigc::ptr_fun(&on_setting_toggled<setting::SprungLayout>),
               &_reactor,
               _menu_view_sprung_layout.get()));

  _menu_view_messages->signal_activate().connect(
    sigc::bind(sigc::ptr_fun(&on_setting_toggled<setting::MessagesVisible>),
               &_reactor,
               _menu_view_messages.get()));

  _menu_view_toolbar->signal_activate().connect(
    sigc::bind(sigc::ptr_fun(&on_setting_toggled<setting::ToolbarVisible>),
               &_reactor,
               _menu_view_toolbar.get()));

  _menu_help_about->signal_activate().connect(
    sigc::mem_fun(this, &Patchage::on_help_about));

  _menu_zoom_in->signal_activate().connect(sigc::bind(
    sigc::mem_fun(this, &Patchage::on_menu_action), Action{action::ZoomIn{}}));
  _menu_zoom_out->signal_activate().connect(sigc::bind(
    sigc::mem_fun(this, &Patchage::on_menu_action), Action{action::ZoomOut{}}));
  _menu_zoom_normal->signal_activate().connect(
    sigc::bind(sigc::mem_fun(this, &Patchage::on_menu_action),
               Action{action::ZoomNormal{}}));
  _menu_zoom_full->signal_activate().connect(
    sigc::bind(sigc::mem_fun(this, &Patchage::on_menu_action),
               Action{action::ZoomFull{}}));
  _menu_increase_font_size->signal_activate().connect(
    sigc::bind(sigc::mem_fun(this, &Patchage::on_menu_action),
               Action{action::IncreaseFontSize{}}));
  _menu_decrease_font_size->signal_activate().connect(
    sigc::bind(sigc::mem_fun(this, &Patchage::on_menu_action),
               Action{action::DecreaseFontSize{}}));
  _menu_normal_font_size->signal_activate().connect(
    sigc::bind(sigc::mem_fun(this, &Patchage::on_menu_action),
               Action{action::ResetFontSize{}}));

  if (_canvas->supports_sprung_layout()) {
    _menu_view_sprung_layout->set_active(true);
  } else {
    _menu_view_sprung_layout->set_active(false);
    _menu_view_sprung_layout->set_sensitive(false);
  }

  // Present window so that display attributes like font size are available
  _canvas->widget().show();
  _main_win->present();

  // Set the default font size based on the current GUI environment
  _conf.set<setting::FontSize>(_canvas->get_default_font_size());

  // Load configuration file (but do not apply it yet, see below)
  _conf.load();

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
    _log.error(fmt::format("Failed to set logo ({})", std::string(e.what())));
  }
#endif

  // Enable JACK menu items if driver is present
  if (_drivers.jack()) {
    _menu_jack_connect->signal_activate().connect(sigc::bind(
      sigc::mem_fun(_drivers.jack().get(), &AudioDriver::attach), true));
    _menu_jack_disconnect->signal_activate().connect(
      sigc::mem_fun(_drivers.jack().get(), &AudioDriver::detach));
  } else {
    _menu_jack_connect->set_sensitive(false);
    _menu_jack_disconnect->set_sensitive(false);
  }

  // Enable ALSA menu items if driver is present
  if (_drivers.alsa()) {
    _menu_alsa_connect->signal_activate().connect(
      sigc::bind(sigc::mem_fun(_drivers.alsa().get(), &Driver::attach), false));
    _menu_alsa_disconnect->signal_activate().connect(
      sigc::mem_fun(_drivers.alsa().get(), &Driver::detach));
  } else {
    _menu_alsa_connect->set_sensitive(false);
    _menu_alsa_disconnect->set_sensitive(false);
  }

  g_signal_connect(
    _main_win->gobj(), "configure-event", G_CALLBACK(configure_cb), this);

  _canvas->widget().grab_focus();

#ifdef PATCHAGE_GTK_OSX
  // Set up Mac menu bar
  GtkosxApplication* osxapp = static_cast<GtkosxApplication*>(
    g_object_new(GTKOSX_TYPE_APPLICATION, nullptr));

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

  // Apply all configuration settings to ensure the GUI is synced
  _conf.each([this](const Setting& setting) { on_conf_change(setting); });

  // Set up an idle callback to process events and update the GUI if necessary
  Glib::signal_timeout().connect(sigc::mem_fun(this, &Patchage::idle_callback),
                                 100);
}

Patchage::~Patchage()
{
  _about_win.destroy();
  _xml.reset();
}

void
Patchage::attach()
{
  if (_drivers.jack() && _options.jack_driver_autoattach) {
    _drivers.jack()->attach(true);
  }

  if (_drivers.alsa() && _options.alsa_driver_autoattach) {
    _drivers.alsa()->attach(false);
  }

  process_events();
  update_toolbar();
}

bool
Patchage::idle_callback()
{
  // Initial run, attach
  if (_attach) {
    attach();
    _menu_view_messages->set_active(_conf.get<setting::MessagesVisible>());
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

  if (_drivers.jack() && _drivers.jack()->is_attached()) {
    const auto buffer_size = _drivers.jack()->buffer_size();
    const auto sample_rate = _drivers.jack()->sample_rate();
    if (sample_rate != 0) {
      const auto sample_rate_khz = sample_rate / 1000.0;
      const auto latency_ms      = buffer_size / sample_rate_khz;

      _latency_label->set_label(" " +
                                fmt::format(T("frames at {} kHz ({:0.2f} ms)"),
                                            sample_rate_khz,
                                            latency_ms));

      _latency_label->set_visible(true);
      _buf_size_combo->set_active(
        static_cast<int>(log2f(_drivers.jack()->buffer_size()) - 5));
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
  if (_drivers.jack() && _drivers.jack()->is_attached()) {
    const auto xruns = _drivers.jack()->xruns();

    _dropouts_label->set_text(" " + fmt::format(T("Dropouts: {}"), xruns));

    if (xruns > 0u) {
      _dropouts_label->show();
      _clear_load_but->show();
    } else {
      _dropouts_label->hide();
      _clear_load_but->hide();
    }
  }

  return true;
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

  _conf.set<setting::WindowLocation>(
    {static_cast<double>(loc_x), static_cast<double>(loc_y)});

  _conf.set<setting::WindowSize>(
    {static_cast<double>(size_x), static_cast<double>(size_y)});
}

void
Patchage::clear_load()
{
  _dropouts_label->set_text(" " + fmt::format(T("Dropouts: {}"), 0U));
  _dropouts_label->hide();
  _clear_load_but->hide();
  if (_drivers.jack()) {
    _drivers.jack()->reset_xruns();
  }
}

void
Patchage::operator()(const setting::AlsaAttached& setting)
{
  if (setting.value) {
    _menu_alsa_connect->set_sensitive(false);
    _menu_alsa_disconnect->set_sensitive(true);

    if (_drivers.alsa()) {
      _drivers.alsa()->refresh([this](const Event& event) {
        handle_event(_conf, _metadata, *_canvas, _log, event);
      });
    }
  } else {
    _menu_alsa_connect->set_sensitive(true);
    _menu_alsa_disconnect->set_sensitive(false);

    _canvas->remove_ports([](const CanvasPort* port) {
      return port->type() == PortType::alsa_midi;
    });
  }
}

void
Patchage::operator()(const setting::JackAttached& setting)
{
  if (setting.value) {
    _menu_jack_connect->set_sensitive(false);
    _menu_jack_disconnect->set_sensitive(true);

    if (_drivers.jack()) {
      _drivers.jack()->refresh([this](const Event& event) {
        handle_event(_conf, _metadata, *_canvas, _log, event);
      });
    }
  } else {
    _menu_jack_connect->set_sensitive(true);
    _menu_jack_disconnect->set_sensitive(false);

    _canvas->remove_ports([](const CanvasPort* port) {
      return (port->type() == PortType::jack_audio ||
              port->type() == PortType::jack_midi ||
              port->type() == PortType::jack_osc ||
              port->type() == PortType::jack_cv);
    });
  }
}

void
Patchage::operator()(const setting::FontSize& setting)
{
  if (static_cast<float>(_canvas->get_font_size()) != setting.value) {
    _canvas->set_font_size(setting.value);
  }
}

void
Patchage::operator()(const setting::HumanNames& setting)
{
  bool human_names = setting.value;

  _menu_view_human_names->set_active(human_names);
  _canvas->for_each_node(update_labels, &human_names);
}

void
Patchage::operator()(const setting::MessagesHeight& setting)
{
  if (_log_scrolledwindow->is_visible()) {
    const int min_height  = _log.min_height();
    const int max_pos     = _main_paned->get_allocation().get_height();
    const int conf_height = setting.value;

    _main_paned->set_position(max_pos - std::max(conf_height, min_height));
  }
}

void
Patchage::operator()(const setting::MessagesVisible& setting)
{
  if (setting.value) {
    _log_scrolledwindow->show();
    _status_text->scroll_to_mark(_status_text->get_buffer()->get_insert(), 0);
  } else {
    _log_scrolledwindow->hide();
  }

  _menu_view_messages->set_active(setting.value);
}

void
Patchage::operator()(const setting::PortColor&)
{
  _canvas->for_each_node(update_port_colors, this);
  _canvas->for_each_edge(update_edge_color, this);
}

void
Patchage::operator()(const setting::SortedPorts& setting)
{
  _menu_view_sort_ports->set_active(setting.value);
  if (setting.value) {
    _canvas->set_port_order(port_order, nullptr);
  } else {
    _canvas->set_port_order(nullptr, nullptr);
  }
}

void
Patchage::operator()(const setting::SprungLayout& setting)
{
  _canvas->set_sprung_layout(setting.value);
  _menu_view_sprung_layout->set_active(setting.value);
}

void
Patchage::operator()(const setting::ToolbarVisible& setting)
{
  if (setting.value) {
    _toolbar->show();
    _menu_view_toolbar->set_active(true);
  } else {
    _toolbar->hide();
    _menu_view_toolbar->set_active(false);
  }
}

void
Patchage::operator()(const setting::WindowLocation& setting)
{
  const int new_x = static_cast<int>(setting.value.x);
  const int new_y = static_cast<int>(setting.value.y);

  int current_x = 0;
  int current_y = 0;
  _main_win->get_position(current_x, current_y);

  if (new_x != current_x || new_y != current_y) {
    _main_win->move(new_x, new_y);
  }
}

void
Patchage::operator()(const setting::WindowSize& setting)
{
  const int new_w = static_cast<int>(setting.value.x);
  const int new_h = static_cast<int>(setting.value.y);

  int current_w = 0;
  int current_h = 0;
  _main_win->get_size(current_w, current_h);

  if (new_w != current_w || new_h != current_h) {
    _main_win->resize(new_w, new_h);
  }
}

void
Patchage::operator()(const setting::Zoom& setting)
{
  if (static_cast<float>(_canvas->get_zoom()) != setting.value) {
    _canvas->set_zoom(setting.value);
  }
}

void
Patchage::on_driver_event(const Event& event)
{
  const std::lock_guard<std::mutex> lock{_events_mutex};

  _driver_events.emplace(event);
}

void
Patchage::process_events()
{
  const std::lock_guard<std::mutex> lock{_events_mutex};

  while (!_driver_events.empty()) {
    const Event& event = _driver_events.front();

    _log.info(event_to_string(event));
    handle_event(_conf, _metadata, *_canvas, _log, event);

    _driver_events.pop();
  }
}

void
Patchage::on_conf_change(const Setting& setting)
{
  std::visit(*this, setting);
}

void
Patchage::on_arrange()
{
  if (_canvas) {
    _canvas->arrange();
  }
}

void
Patchage::on_help_about()
{
  _about_win->run();
  _about_win->hide();
}

void
Patchage::on_legend_color_change(PortType id, const std::string&, uint32_t rgba)
{
  _reactor(action::ChangeSetting{{setting::PortColor{id, rgba}}});
}

void
Patchage::on_messages_resized(Gtk::Allocation&)
{
  const int max_pos = _main_paned->get_allocation().get_height();

  _conf.set<setting::MessagesHeight>(max_pos - _main_paned->get_position());
}

void
Patchage::save()
{
  _conf.set<setting::Zoom>(_canvas->get_zoom()); // Can be changed by ganv
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
  if (_drivers.alsa()) {
    _drivers.alsa()->detach();
  }

  if (_drivers.jack()) {
    _drivers.jack()->detach();
  }

  _main_win->hide();
}

void
Patchage::on_export_image()
{
  Gtk::FileChooserDialog dialog(T("Export Image"),
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

  auto* bg_but = new Gtk::CheckButton(T("Draw _Background"), true);
  auto* extra  = new Gtk::Alignment(1.0, 0.5, 0.0, 0.0);
  bg_but->set_active(true);
  extra->add(*Gtk::manage(bg_but));
  extra->show_all();
  dialog.set_extra_widget(*Gtk::manage(extra));

  if (dialog.run() == Gtk::RESPONSE_OK) {
    const std::string filename = dialog.get_filename();
    if (Glib::file_test(filename, Glib::FILE_TEST_EXISTS)) {
      Gtk::MessageDialog confirm(
        fmt::format(T("File exists!  Overwrite {}?"), filename),
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

bool
Patchage::on_scroll(GdkEventScroll*)
{
  return false;
}

void
Patchage::on_menu_action(const Action& action)
{
  _reactor(action);
}

void
Patchage::buffer_size_changed()
{
  if (_drivers.jack()) {
    const int selected = _buf_size_combo->get_active_row_number();

    if (selected == -1) {
      update_toolbar();
    } else {
      const uint32_t buffer_size = 1u << (selected + 5);
      _drivers.jack()->set_buffer_size(buffer_size);
      update_toolbar();
    }
  }
}

} // namespace patchage
