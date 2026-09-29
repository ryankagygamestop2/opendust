/**************************************************************************/
/*  os_shell.h                                                            */
/**************************************************************************/
/*                         This file is part of:                          */
/*                              OPENDUST                                  */
/*                         https://opendust.io                            */
/**************************************************************************/
/* OpenDust is a fork of Godot Engine. MIT licensed; see LICENSE.txt.     */
/**************************************************************************/

#pragma once

#include "os_app_context.h"
#include "scene/gui/control.h"

class Button;
class HBoxContainer;
class Label;
class PanelContainer;
class Timer;
class VBoxContainer;

// The window manager. Status bar on top, one fullscreen app below it.
// Built procedurally so a device works with zero project assets.
class OSShell : public Control {
	GDCLASS(OSShell, Control);

	ObjectID host_id; // Device or Device2D.

	VBoxContainer *vbox = nullptr;
	HBoxContainer *status_bar = nullptr;
	Button *back_button = nullptr;
	Label *clock_label = nullptr;
	Label *notify_label = nullptr;
	Label *session_label = nullptr;
	Label *world_label = nullptr;
	PanelContainer *content = nullptr;
	Timer *clock_timer = nullptr;
	Timer *notify_timer = nullptr;

	Control *current_app = nullptr;
	String current_app_id;

	void _tick();
	void _clear_notify();
	void _close_current();
	void _on_back_pressed();

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	void set_host(Node *p_host);
	Node *get_host() const;

	Ref<OSAppContext> make_context(const Dictionary &p_args) const;

	void open_app(const String &p_app_id, const Dictionary &p_args = Dictionary());
	void go_home();
	String get_current_app_id() const;
	Control *get_current_app() const;
	void notify(const String &p_text);
	void refresh_status();

	OSShell();
};
