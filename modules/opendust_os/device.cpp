/**************************************************************************/
/*  device.cpp                                                            */
/**************************************************************************/
/*                         This file is part of:                          */
/*                              OPENDUST                                  */
/*                         https://opendust.io                            */
/**************************************************************************/
/* OpenDust is a fork of Godot Engine. MIT licensed; see LICENSE.txt.     */
/**************************************************************************/

#include "device.h"
#include "core/object/class_db.h"

#include "os_kernel.h"

#include "core/math/math_funcs.h"
#include "core/os/time.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/main/viewport.h"
#include "scene/resources/3d/primitive_meshes.h"
#include "scene/resources/material.h"

static String _generate_device_id() {
	return "dev-" + String::num_uint64(Time::get_singleton()->get_ticks_usec(), 16) + "-" + String::num_uint64(Math::rand() & 0xFFFF, 16);
}

static String _find_room_id(const Node *p_from) {
	// A Room (opendust_slate) exposes `room_id`; walk up without a hard
	// dependency on that module.
	const Node *n = p_from ? p_from->get_parent() : nullptr;
	while (n) {
		if (n->is_class("Room")) {
			return String(n->get("room_id"));
		}
		n = n->get_parent();
	}
	return String();
}

// --- Device ----------------------------------------------------------------

Device::Device() {
	viewport = memnew(SubViewport);
	viewport->set_size(screen_size_px);
	viewport->set_disable_3d(true);
	viewport->set_update_mode(SubViewport::UPDATE_ALWAYS);
	add_child(viewport, false, INTERNAL_MODE_FRONT);

	shell = memnew(OSShell);
	shell->set_host(this);
	viewport->add_child(shell);

	quad.instantiate();
	quad->set_size(screen_size);

	material.instantiate();
	material->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);

	screen = memnew(MeshInstance3D);
	screen->set_mesh(quad);
	screen->set_material_override(material);
	add_child(screen, false, INTERNAL_MODE_FRONT);
}

void Device::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			if (device_id.is_empty()) {
				device_id = _generate_device_id();
			}
			OSKernel *kernel = OSKernel::ensure(get_tree());
			if (kernel) {
				kernel->register_device(this, device_id);
			}
		} break;
		case NOTIFICATION_READY: {
			material->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, viewport->get_texture());
			shell->refresh_status();
			if (awake && !boot_app.is_empty() && shell->get_current_app_id().is_empty()) {
				shell->open_app(boot_app);
			}
			_apply_awake();
		} break;
		case NOTIFICATION_EXIT_TREE: {
			OSKernel *kernel = OSKernel::get_singleton();
			if (kernel) {
				kernel->unregister_device(device_id);
			}
		} break;
	}
}

void Device::set_device_id(const String &p_id) {
	if (is_inside_tree() && !device_id.is_empty() && device_id != p_id) {
		OSKernel *kernel = OSKernel::get_singleton();
		if (kernel) {
			kernel->unregister_device(device_id);
			device_id = p_id;
			kernel->register_device(this, device_id);
			return;
		}
	}
	device_id = p_id;
}

String Device::get_device_id() const {
	return device_id;
}

void Device::set_screen_size_px(const Vector2i &p_size) {
	screen_size_px = Vector2i(MAX(64, p_size.x), MAX(64, p_size.y));
	viewport->set_size(screen_size_px);
}

Vector2i Device::get_screen_size_px() const {
	return screen_size_px;
}

void Device::set_screen_size(const Vector2 &p_size) {
	screen_size = p_size;
	quad->set_size(screen_size);
}

Vector2 Device::get_screen_size() const {
	return screen_size;
}

void Device::set_boot_app(const String &p_app) {
	boot_app = p_app;
}

String Device::get_boot_app() const {
	return boot_app;
}

Dictionary Device::get_session() const {
	OSKernel *kernel = OSKernel::get_singleton();
	return kernel ? kernel->get_session_for(device_id) : Dictionary();
}

String Device::get_room_id() const {
	return _find_room_id(this);
}

void Device::open_app(const String &p_app_id, const Dictionary &p_args) {
	shell->open_app(p_app_id, p_args);
}

bool Device::is_awake() const {
	return awake;
}

void Device::_apply_awake() {
	viewport->set_update_mode(awake ? SubViewport::UPDATE_ALWAYS : SubViewport::UPDATE_DISABLED);
	material->set_albedo(awake ? Color(1, 1, 1) : Color(0.15, 0.15, 0.15));
}

void Device::wake() {
	awake = true;
	_apply_awake();
	emit_signal(SNAME("woke"));
}

void Device::sleep() {
	awake = false;
	_apply_awake();
	emit_signal(SNAME("slept"));
}

void Device::receive_message(const Dictionary &p_message) {
	String from = p_message.get("from", "?");
	String body = p_message.get("body", "");
	shell->notify(from + ": " + body);
	emit_signal(SNAME("message_received"), p_message);
}

void Device::push_input(const Ref<InputEvent> &p_event) {
	if (p_event.is_valid() && awake) {
		viewport->push_input(p_event, true);
	}
}

OSShell *Device::get_shell() const {
	return shell;
}

SubViewport *Device::get_screen_viewport() const {
	return viewport;
}

void Device::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_device_id", "id"), &Device::set_device_id);
	ClassDB::bind_method(D_METHOD("get_device_id"), &Device::get_device_id);
	ClassDB::bind_method(D_METHOD("set_screen_size_px", "size"), &Device::set_screen_size_px);
	ClassDB::bind_method(D_METHOD("get_screen_size_px"), &Device::get_screen_size_px);
	ClassDB::bind_method(D_METHOD("set_screen_size", "size"), &Device::set_screen_size);
	ClassDB::bind_method(D_METHOD("get_screen_size"), &Device::get_screen_size);
	ClassDB::bind_method(D_METHOD("set_boot_app", "app_id"), &Device::set_boot_app);
	ClassDB::bind_method(D_METHOD("get_boot_app"), &Device::get_boot_app);
	ClassDB::bind_method(D_METHOD("get_session"), &Device::get_session);
	ClassDB::bind_method(D_METHOD("get_room_id"), &Device::get_room_id);
	ClassDB::bind_method(D_METHOD("open_app", "app_id", "args"), &Device::open_app, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("is_awake"), &Device::is_awake);
	ClassDB::bind_method(D_METHOD("wake"), &Device::wake);
	ClassDB::bind_method(D_METHOD("sleep"), &Device::sleep);
	ClassDB::bind_method(D_METHOD("receive_message", "message"), &Device::receive_message);
	ClassDB::bind_method(D_METHOD("push_input", "event"), &Device::push_input);
	ClassDB::bind_method(D_METHOD("get_shell"), &Device::get_shell);
	ClassDB::bind_method(D_METHOD("get_screen_viewport"), &Device::get_screen_viewport);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "device_id"), "set_device_id", "get_device_id");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "screen_size_px"), "set_screen_size_px", "get_screen_size_px");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "screen_size", PROPERTY_HINT_NONE, "suffix:m"), "set_screen_size", "get_screen_size");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "boot_app"), "set_boot_app", "get_boot_app");

	ADD_SIGNAL(MethodInfo("message_received", PropertyInfo(Variant::DICTIONARY, "message")));
	ADD_SIGNAL(MethodInfo("woke"));
	ADD_SIGNAL(MethodInfo("slept"));
}

// --- Device2D --------------------------------------------------------------

Device2D::Device2D() {
	shell = memnew(OSShell);
	shell->set_host(this);
	add_child(shell, false, INTERNAL_MODE_FRONT);
}

void Device2D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			if (device_id.is_empty()) {
				device_id = _generate_device_id();
			}
			OSKernel *kernel = OSKernel::ensure(get_tree());
			if (kernel) {
				kernel->register_device(this, device_id);
			}
		} break;
		case NOTIFICATION_READY: {
			shell->refresh_status();
			if (awake && !boot_app.is_empty() && shell->get_current_app_id().is_empty()) {
				shell->open_app(boot_app);
			}
		} break;
		case NOTIFICATION_EXIT_TREE: {
			OSKernel *kernel = OSKernel::get_singleton();
			if (kernel) {
				kernel->unregister_device(device_id);
			}
		} break;
	}
}

void Device2D::set_device_id(const String &p_id) {
	device_id = p_id;
}

String Device2D::get_device_id() const {
	return device_id;
}

void Device2D::set_boot_app(const String &p_app) {
	boot_app = p_app;
}

String Device2D::get_boot_app() const {
	return boot_app;
}

Dictionary Device2D::get_session() const {
	OSKernel *kernel = OSKernel::get_singleton();
	return kernel ? kernel->get_session_for(device_id) : Dictionary();
}

String Device2D::get_room_id() const {
	return _find_room_id(this);
}

void Device2D::open_app(const String &p_app_id, const Dictionary &p_args) {
	shell->open_app(p_app_id, p_args);
}

bool Device2D::is_awake() const {
	return awake;
}

void Device2D::wake() {
	awake = true;
	shell->set_visible(true);
}

void Device2D::sleep() {
	awake = false;
	shell->set_visible(false);
}

void Device2D::receive_message(const Dictionary &p_message) {
	String from = p_message.get("from", "?");
	String body = p_message.get("body", "");
	shell->notify(from + ": " + body);
	emit_signal(SNAME("message_received"), p_message);
}

OSShell *Device2D::get_shell() const {
	return shell;
}

void Device2D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_device_id", "id"), &Device2D::set_device_id);
	ClassDB::bind_method(D_METHOD("get_device_id"), &Device2D::get_device_id);
	ClassDB::bind_method(D_METHOD("set_boot_app", "app_id"), &Device2D::set_boot_app);
	ClassDB::bind_method(D_METHOD("get_boot_app"), &Device2D::get_boot_app);
	ClassDB::bind_method(D_METHOD("get_session"), &Device2D::get_session);
	ClassDB::bind_method(D_METHOD("get_room_id"), &Device2D::get_room_id);
	ClassDB::bind_method(D_METHOD("open_app", "app_id", "args"), &Device2D::open_app, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("is_awake"), &Device2D::is_awake);
	ClassDB::bind_method(D_METHOD("wake"), &Device2D::wake);
	ClassDB::bind_method(D_METHOD("sleep"), &Device2D::sleep);
	ClassDB::bind_method(D_METHOD("receive_message", "message"), &Device2D::receive_message);
	ClassDB::bind_method(D_METHOD("get_shell"), &Device2D::get_shell);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "device_id"), "set_device_id", "get_device_id");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "boot_app"), "set_boot_app", "get_boot_app");

	ADD_SIGNAL(MethodInfo("message_received", PropertyInfo(Variant::DICTIONARY, "message")));
}
