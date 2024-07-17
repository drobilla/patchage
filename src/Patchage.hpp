// Copyright 2007-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PATCHAGE_PATCHAGE_HPP
#define PATCHAGE_PATCHAGE_HPP

#include <gdk/gdk.h>
#include <glibmm/refptr.h>
#include <gtkmm/treemodel.h>
#include <gtkmm/treemodelcolumn.h>
#include <gtkmm/widget.h>

#include "Action.hpp"
#include "ActionSink.hpp"
#include "Configuration.hpp"
#include "Drivers.hpp"
#include "Event.hpp"
#include "Metadata.hpp"
#include "Options.hpp"
#include "Reactor.hpp"
#include "Setting.hpp"
#include "TextViewLog.hpp"
#include "Widget.hpp"

#include <cstdint>
#include <memory>
#include <mutex>
#include <queue>
#include <string>

namespace Glib {
class ustring;
} // namespace Glib

namespace Gtk {
class AboutDialog;
class Alignment;
class Builder;
class CheckMenuItem;
class ComboBox;
class ImageMenuItem;
class Label;
class MenuBar;
class MenuItem;
class Paned;
class ScrolledWindow;
class TextTag;
class TextView;
class ToolButton;
class Toolbar;
class VBox;
class Window;
} // namespace Gtk

namespace patchage {

enum class PortType;

class Canvas;
class ILog;
class Legend;

/// Main application class
class Patchage
{
public:
  explicit Patchage(Options options);
  ~Patchage();

  Patchage(const Patchage&)            = delete;
  Patchage& operator=(const Patchage&) = delete;

  Patchage(Patchage&&)            = delete;
  Patchage& operator=(Patchage&&) = delete;

  void operator()(const setting::AlsaAttached& setting);
  void operator()(const setting::FontSize& setting);
  void operator()(const setting::HumanNames& setting);
  void operator()(const setting::JackAttached& setting);
  void operator()(const setting::MessagesHeight& setting);
  void operator()(const setting::MessagesVisible& setting);
  void operator()(const setting::PortColor& setting);
  void operator()(const setting::SortedPorts& setting);
  void operator()(const setting::SprungLayout& setting);
  void operator()(const setting::ToolbarVisible& setting);
  void operator()(const setting::WindowLocation& setting);
  void operator()(const setting::WindowSize& setting);
  void operator()(const setting::Zoom& setting);

  void attach();
  void save();
  void quit();

  void store_window_location();

  Canvas&              canvas() const { return *_canvas; }
  Gtk::Window*         window() { return _main_win.get(); }
  ILog&                log() { return _log; }
  Metadata&            metadata() { return _metadata; }
  const Configuration& conf() const { return _conf; }
  Configuration&       conf() { return _conf; }

protected:
  class BufferSizeColumns : public Gtk::TreeModel::ColumnRecord
  {
  public:
    BufferSizeColumns() { add(label); }

    Gtk::TreeModelColumn<Glib::ustring> label;
  };

  void on_driver_event(const Event& event);
  void process_events();

  void on_conf_change(const Setting& setting);

  void on_arrange();
  void on_help_about();
  void on_quit();
  void on_export_image();
  void on_store_positions();

  void on_legend_color_change(PortType           id,
                              const std::string& label,
                              uint32_t           rgba);

  void on_messages_resized(Gtk::Allocation& alloc);

  static bool on_scroll(GdkEventScroll* ev);

  void on_menu_action(const Action& action);

  bool idle_callback();
  void clear_load();
  bool update_load();
  void update_toolbar();

  void buffer_size_changed();

  Glib::RefPtr<Gtk::Builder> _xml;

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
  Widget<Gtk::Label>          _dropouts_label;
  Widget<Gtk::ComboBox>       _buf_size_combo;
  Widget<Gtk::Label>          _latency_label;
  Widget<Gtk::Alignment>      _legend_alignment;
  Widget<Gtk::Paned>          _main_paned;
  Widget<Gtk::ScrolledWindow> _log_scrolledwindow;
  Widget<Gtk::TextView>       _status_text;

  Configuration           _conf;
  TextViewLog             _log;
  std::unique_ptr<Canvas> _canvas;
  std::mutex              _events_mutex;
  std::queue<Event>       _driver_events;
  BufferSizeColumns       _buf_size_columns;
  Legend*                 _legend{nullptr};
  Metadata                _metadata;
  Drivers                 _drivers;
  Reactor                 _reactor;
  ActionSink              _action_sink;

  Glib::RefPtr<Gtk::TextTag> _error_tag;
  Glib::RefPtr<Gtk::TextTag> _warning_tag;

  Options _options;
  bool    _attach{true};
};

} // namespace patchage

#endif // PATCHAGE_PATCHAGE_HPP
