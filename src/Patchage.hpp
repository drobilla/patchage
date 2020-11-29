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
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/textview.h>
#include <gtkmm/toolbar.h>
#include <gtkmm/toolbutton.h>
#include <gtkmm/viewport.h>
#include <gtkmm/window.h>

#include "ClientType.hpp"
#include "Connector.hpp"
#include "ILog.hpp"
#include "Legend.hpp"
#include "Metadata.hpp"
#include "Options.hpp"
#include "PatchageEvent.hpp"
#include "TextViewLog.hpp"
#include "Widget.hpp"
#include "patchage_config.h"

#include <memory>
#include <mutex>
#include <queue>
#include <string>

namespace patchage {

class AudioDriver;
class PatchageCanvas;
class Configuration;

/// Main application class
class Patchage
{
public:
	explicit Patchage(Options options);

	~Patchage();

	Patchage(const Patchage&) = delete;
	Patchage& operator=(const Patchage&) = delete;

	Patchage(Patchage&&) = delete;
	Patchage& operator=(Patchage&&) = delete;

	const std::unique_ptr<PatchageCanvas>& canvas() const { return _canvas; }

	void attach();
	void refresh();
	void save();
	void quit();

	void driver_attached(ClientType type);
	void driver_detached(ClientType type);

	void update_state();
	void store_window_location();

	bool show_human_names() const
	{
		return _menu_view_human_names->get_active();
	}

	bool sort_ports() const { return _menu_view_sort_ports->get_active(); }

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

	void on_driver_event(const PatchageEvent& event);
	void process_events();

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

	void on_legend_color_change(PortType           id,
	                            const std::string& label,
	                            uint32_t           rgba);

	void on_messages_resized(Gtk::Allocation& alloc);

	bool on_scroll(GdkEventScroll* ev);

	void zoom(double z);
	bool idle_callback();
	void clear_load();
	bool update_load();
	void update_toolbar();

	void buffer_size_changed();

	Glib::RefPtr<Gtk::Builder> _xml;

	std::mutex                _events_mutex;
	std::queue<PatchageEvent> _driver_events;

	std::unique_ptr<Driver> _alsa_driver;

	std::unique_ptr<PatchageCanvas> _canvas;

	std::unique_ptr<AudioDriver> _jack_driver;
	Configuration                _conf;

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
	Legend*                     _legend;
	TextViewLog                 _log;
	Connector                   _connector;
	Metadata                    _metadata;

	Glib::RefPtr<Gtk::TextTag> _error_tag;
	Glib::RefPtr<Gtk::TextTag> _warning_tag;

	Options _options;
	bool    _pane_initialized;
	bool    _attach;
};

} // namespace patchage

#endif // PATCHAGE_PATCHAGE_HPP
