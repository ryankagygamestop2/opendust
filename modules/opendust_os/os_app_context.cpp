/**************************************************************************/
/*  os_app_context.cpp                                                    */
/**************************************************************************/
/*                         This file is part of:                          */
/*                              OPENDUST                                  */
/*                         https://opendust.io                            */
/**************************************************************************/
/* OpenDust is a fork of Godot Engine. MIT licensed; see LICENSE.txt.     */
/**************************************************************************/

#include "os_app_context.h"
#include "core/object/class_db.h"

#include "os_kernel.h"
#include "os_shell.h"

void OSAppContext::setup(OSKernel *p_kernel, Node *p_device, OSShell *p_shell, const Dictionary &p_session, const Ref<WorldDrive> &p_drive, const Dictionary &p_args) {
	kernel_id = p_kernel ? p_kernel->get_instance_id() : ObjectID();
	device_id = p_device ? p_device->get_instance_id() : ObjectID();
	shell_id = p_shell ? p_shell->get_instance_id() : ObjectID();
	session = p_session;
	drive = p_drive;
	args = p_args;
}

OSKernel *OSAppContext::get_kernel() const {
	return Object::cast_to<OSKernel>(ObjectDB::get_instance(kernel_id));
}

Node *OSAppContext::get_device() const {
	return Object::cast_to<Node>(ObjectDB::get_instance(device_id));
}

OSShell *OSAppContext::get_shell() const {
	return Object::cast_to<OSShell>(ObjectDB::get_instance(shell_id));
}

Dictionary OSAppContext::get_session() const {
	return session;
}

Ref<WorldDrive> OSAppContext::get_drive() const {
	return drive;
}

Dictionary OSAppContext::get_args() const {
	return args;
}

void OSAppContext::open(const String &p_sku_or_path) {
	if (p_sku_or_path.begins_with("$SKU")) {
		// TODO(05-identity.md): InventoryRegistry resolve → app_id. Until then
		// an unknown item is shown, never dropped.
		notify("Unknown item " + p_sku_or_path + " (inventory registry not available yet)");
		return;
	}
	Dictionary a;
	a["path"] = p_sku_or_path;
	open_app("os.notes", a);
}

void OSAppContext::open_app(const String &p_app_id, const Dictionary &p_args) {
	OSShell *shell = get_shell();
	if (shell) {
		shell->open_app(p_app_id, p_args);
	}
}

void OSAppContext::notify(const String &p_text) {
	OSShell *shell = get_shell();
	if (shell) {
		shell->notify(p_text);
	}
}

void OSAppContext::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_kernel"), &OSAppContext::get_kernel);
	ClassDB::bind_method(D_METHOD("get_device"), &OSAppContext::get_device);
	ClassDB::bind_method(D_METHOD("get_shell"), &OSAppContext::get_shell);
	ClassDB::bind_method(D_METHOD("get_session"), &OSAppContext::get_session);
	ClassDB::bind_method(D_METHOD("get_drive"), &OSAppContext::get_drive);
	ClassDB::bind_method(D_METHOD("get_args"), &OSAppContext::get_args);
	ClassDB::bind_method(D_METHOD("open", "sku_or_path"), &OSAppContext::open);
	ClassDB::bind_method(D_METHOD("open_app", "app_id", "args"), &OSAppContext::open_app, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("notify", "text"), &OSAppContext::notify);
}
