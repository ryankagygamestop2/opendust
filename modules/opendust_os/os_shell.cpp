/**************************************************************************/
/*  os_shell.cpp                                                          */
/**************************************************************************/
/*                         This file is part of:                          */
/*                              OPENDUST                                  */
/*                         https://opendust.io                            */
/**************************************************************************/
/* OpenDust is a fork of Godot Engine. MIT licensed; see LICENSE.txt.     */
/**************************************************************************/

#include "os_shell.h"

#include "os_apps.h"
#include "os_kernel.h"

#include "core/os/time.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/label.h"
#include "scene/gui/panel_container.h"
#include "scene/main/timer.h"
#include "scene/resources/style_box_flat.h"

OSShell::OSShell() {
	set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);

	Ref<StyleBoxFlat> bg;
	bg.instantiate();
	bg->set_bg_color(Color(0.09, 0.10, 0.12));

	vbox = memnew(VBoxContainer);
	vbox->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	add_child(vbox, false, INTERNAL_MODE_FRONT);

	PanelContainer *status_panel = memnew(PanelContainer);
	Ref<StyleBoxFlat> bar;
	bar.instantiate();
	bar->set_bg_color(Color(0.14, 0.16, 0.19));
	status_panel->add_theme_stylebox_override(SNAME("panel"), bar);
	vbox->add_child(status_panel);

	status_bar = memnew(HBoxContainer);
	status_panel->add_child(status_bar);

	back_button = memnew(Button);
	back_button->set_text(U"←");
	back_button->set_visible(false);
	back_button->connect(SNAME("pressed"), callable_mp(this, &OSShell::_on_back_pressed));
	status_bar->add_child(back_button);

	clock_label = memnew(Label);
	status_bar->add_child(clock_label);

	notify_label = memnew(Label);
	notify_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	notify_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	notify_label->add_theme_color_override(SNAME("font_color"), Color(0.95, 0.85, 0.5));
	status_bar->add_child(notify_label);

	session_label = memnew(Label);
	status_bar->add_child(session_label);

	world_label = memnew(Label);
	world_label->add_theme_color_override(SNAME("font_color"), Color(0.6, 0.65, 0.7));
	status_bar->add_child(world_label);

	content = memnew(PanelContainer);
	content->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	content->add_theme_stylebox_override(SNAME("panel"), bg);
	vbox->add_child(content);

	clock_timer = memnew(Timer);
	clock_timer->set_wait_time(1.0);
	clock_timer->set_autostart(true);
	clock_timer->connect(SNAME("timeout"), callable_mp(this, &OSShell::_tick));
	add_child(clock_timer, false, INTERNAL_MODE_FRONT);

	notify_timer = memnew(Timer);
	notify_timer->set_wait_time(4.0);
	notify_timer->set_one_shot(true);
	notify_timer->connect(SNAME("timeout"), callable_mp(this, &OSShell::_clear_notify));
	add_child(notify_timer, false, INTERNAL_MODE_FRONT);
}

void OSShell::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY) {
		refresh_status();
	}
}

void OSShell::set_host(Node *p_host) {
	host_id = p_host ? p_host->get_instance_id() : ObjectID();
}

Node *OSShell::get_host() const {
	return Object::cast_to<Node>(ObjectDB::get_instance(host_id));
}

Ref<OSAppContext> OSShell::make_context(const Dictionary &p_args) const {
	Ref<OSAppContext> ctx;
	ctx.instantiate();
	OSKernel *kernel = OSKernel::get_singleton();
	Node *host = get_host();
	Dictionary session;
	Ref<WorldDrive> drive;
	if (kernel) {
		String device_id = (host && host->has_method("get_device_id")) ? String(host->call("get_device_id")) : String();
		session = kernel->get_session_for(device_id);
		drive = kernel->get_drive();
	}
	ctx->setup(kernel, host, const_cast<OSShell *>(this), session, drive, p_args);
	return ctx;
}

void OSShell::_close_current() {
	if (current_app) {
		content->remove_child(current_app);
		current_app->queue_free();
		current_app = nullptr;
		current_app_id = String();
	}
}

void OSShell::open_app(const String &p_app_id, const Dictionary &p_args) {
	OSKernel *kernel = OSKernel::get_singleton();
	ERR_FAIL_NULL_MSG(kernel, "OSShell: no OSKernel; is a Device in the tree?");
	Control *app = kernel->instantiate_app(p_app_id);
	if (!app) {
		notify("No app: " + p_app_id);
		return;
	}
	_close_current();
	current_app = app;
	current_app_id = p_app_id;
	app->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	content->add_child(app);

	Ref<OSAppContext> ctx = make_context(p_args);
	OSApp *typed = Object::cast_to<OSApp>(app);
	if (typed) {
		typed->setup(ctx);
	} else if (app->has_method("setup")) {
		app->call("setup", ctx);
	}
	back_button->set_visible(p_app_id != "os.home");
	emit_signal(SNAME("app_opened"), p_app_id);
}

void OSShell::go_home() {
	open_app("os.home");
}

void OSShell::_on_back_pressed() {
	go_home();
}

String OSShell::get_current_app_id() const {
	return current_app_id;
}

Control *OSShell::get_current_app() const {
	return current_app;
}

void OSShell::notify(const String &p_text) {
	notify_label->set_text(p_text);
	if (is_inside_tree()) {
		notify_timer->start();
	}
}

void OSShell::_clear_notify() {
	notify_label->set_text(String());
}

void OSShell::_tick() {
	clock_label->set_text(Time::get_singleton()->get_time_string_from_system().substr(0, 5));
}

void OSShell::refresh_status() {
	_tick();
	OSKernel *kernel = OSKernel::get_singleton();
	if (kernel) {
		Node *host = get_host();
		String device_id = (host && host->has_method("get_device_id")) ? String(host->call("get_device_id")) : String();
		Dictionary session = kernel->get_session_for(device_id);
		session_label->set_text(String(session.get("display_name", "")));
		world_label->set_text(" " + kernel->get_world_id());
	}
}

void OSShell::_bind_methods() {
	ClassDB::bind_method(D_METHOD("open_app", "app_id", "args"), &OSShell::open_app, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("go_home"), &OSShell::go_home);
	ClassDB::bind_method(D_METHOD("get_current_app_id"), &OSShell::get_current_app_id);
	ClassDB::bind_method(D_METHOD("get_current_app"), &OSShell::get_current_app);
	ClassDB::bind_method(D_METHOD("notify", "text"), &OSShell::notify);
	ClassDB::bind_method(D_METHOD("refresh_status"), &OSShell::refresh_status);
	ClassDB::bind_method(D_METHOD("get_host"), &OSShell::get_host);

	ADD_SIGNAL(MethodInfo("app_opened", PropertyInfo(Variant::STRING, "app_id")));
}
