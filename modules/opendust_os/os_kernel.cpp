/**************************************************************************/
/*  os_kernel.cpp                                                         */
/**************************************************************************/
/*                         This file is part of:                          */
/*                              OPENDUST                                  */
/*                         https://opendust.io                            */
/**************************************************************************/
/* OpenDust is a fork of Godot Engine. MIT licensed; see LICENSE.txt.     */
/**************************************************************************/

#include "os_kernel.h"

#include "os_apps.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/os/time.h"
#include "scene/gui/control.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"

OSKernel *OSKernel::singleton = nullptr;
HashMap<String, OSKernel::AppEntry> OSKernel::builtin_apps;

OSKernel *OSKernel::get_singleton() {
	return singleton;
}

OSKernel *OSKernel::ensure(SceneTree *p_tree) {
	if (singleton) {
		return singleton;
	}
	ERR_FAIL_NULL_V(p_tree, nullptr);
	OSKernel *k = memnew(OSKernel);
	k->set_name("OpenDustOSKernel");
	Window *root = p_tree->get_root();
	ERR_FAIL_NULL_V(root, k);
	// Devices call this from their own _enter_tree; the root may be busy
	// setting up children, so the add is deferred. The singleton is already
	// usable: nothing below needs the kernel to be in the tree.
	root->call_deferred(SNAME("add_child"), k);
	return k;
}

void OSKernel::add_builtin_app(const String &p_id, const String &p_title, BuiltinAppFactory p_factory) {
	AppEntry e;
	e.id = p_id;
	e.title = p_title;
	e.factory = p_factory;
	builtin_apps[p_id] = e;
}

void OSKernel::register_builtin_apps() {
	add_builtin_app("os.home", "Home", OSHomeApp::create);
	add_builtin_app("os.files", "Files", OSFilesApp::create);
	add_builtin_app("os.notes", "Notes", OSNotesApp::create);
	add_builtin_app("os.messages", "Messages", OSMessagesApp::create);
	// os.terminal and os.soul: later, see docs/opendust/03-os-layer.md.
}

OSKernel::OSKernel() {
	singleton = this;

	world_id = GLOBAL_GET("opendust/world_id");
	if (world_id.is_empty()) {
		world_id = "default";
	}
	String configured = GLOBAL_GET("opendust/os/drive_root");
	if (configured.is_empty()) {
		configured = "user://opendust/worlds/" + world_id + "/drive";
	}
	drive_root = ProjectSettings::get_singleton()->globalize_path(configured).replace("\\", "/");
	Error err = DirAccess::make_dir_recursive_absolute(drive_root);
	if (err != OK && err != ERR_ALREADY_EXISTS) {
		ERR_PRINT("OSKernel: could not create world drive at '" + drive_root + "'.");
	}

	drive.instantiate();
	drive->set_root(drive_root);
	drive->connect(SNAME("changed"), callable_mp(this, &OSKernel::_on_drive_changed));

	identity.instantiate();

	apps = builtin_apps;
}

OSKernel::~OSKernel() {
	if (singleton == this) {
		singleton = nullptr;
	}
}

void OSKernel::_notification(int p_what) {
	if (p_what == NOTIFICATION_EXIT_TREE && singleton == this) {
		// The world is going away; a new world creates a new kernel.
		singleton = nullptr;
	}
}

void OSKernel::_on_drive_changed(const String &p_path) {
	emit_signal(SNAME("drive_changed"), p_path);
}

String OSKernel::get_world_id() const {
	return world_id;
}

String OSKernel::get_drive_root() const {
	return drive_root;
}

Ref<WorldDrive> OSKernel::get_drive() const {
	return drive;
}

Ref<OpenDustIdentity> OSKernel::get_identity() const {
	return identity;
}

void OSKernel::register_app(const String &p_id, const String &p_title, const Ref<PackedScene> &p_scene) {
	ERR_FAIL_COND_MSG(p_id.is_empty(), "OSKernel: app id must not be empty.");
	AppEntry e;
	e.id = p_id;
	e.title = p_title.is_empty() ? p_id : p_title;
	e.scene = p_scene;
	apps[p_id] = e;
	emit_signal(SNAME("apps_changed"));
}

void OSKernel::unregister_app(const String &p_id) {
	if (apps.erase(p_id)) {
		emit_signal(SNAME("apps_changed"));
	}
}

bool OSKernel::has_app(const String &p_id) const {
	return apps.has(p_id);
}

Array OSKernel::list_apps() const {
	Array out;
	for (const KeyValue<String, AppEntry> &kv : apps) {
		Dictionary d;
		d["id"] = kv.value.id;
		d["title"] = kv.value.title;
		d["builtin"] = kv.value.factory != nullptr;
		out.push_back(d);
	}
	return out;
}

Control *OSKernel::instantiate_app(const String &p_id) {
	const AppEntry *e = apps.getptr(p_id);
	ERR_FAIL_NULL_V_MSG(e, nullptr, "OSKernel: no app registered as '" + p_id + "'.");
	if (e->factory) {
		return e->factory();
	}
	ERR_FAIL_COND_V_MSG(e->scene.is_null(), nullptr, "OSKernel: app '" + p_id + "' has no scene.");
	Node *n = e->scene->instantiate();
	Control *c = Object::cast_to<Control>(n);
	if (!c) {
		ERR_PRINT("OSKernel: app '" + p_id + "' root is not a Control.");
		if (n) {
			memdelete(n);
		}
		return nullptr;
	}
	return c;
}

Dictionary OSKernel::get_session_for(const String &p_device_id) const {
	// Phase 0: every device shares the local session. When pods.global SSO
	// lands, this becomes per-device sign-in state (03-os-layer.md, Session).
	return identity->get_session();
}

bool OSKernel::_session_matches(const Dictionary &p_session, const String &p_id) const {
	if (p_id.is_empty()) {
		return false;
	}
	return String(p_session.get("user_id", "")) == p_id ||
			String(p_session.get("agent_id", "")) == p_id ||
			String(p_session.get("display_name", "")) == p_id;
}

void OSKernel::_deliver(Node *p_device, const Dictionary &p_msg) {
	if (p_device && p_device->has_method("receive_message")) {
		p_device->call("receive_message", p_msg);
	}
}

void OSKernel::_flush_pending_for(const String &p_device_id) {
	Node *dev = get_device(p_device_id);
	if (!dev) {
		return;
	}
	Dictionary session = get_session_for(p_device_id);
	List<String> keys;
	for (const KeyValue<String, List<Dictionary>> &kv : pending) {
		if (kv.key == p_device_id || _session_matches(session, kv.key)) {
			keys.push_back(kv.key);
		}
	}
	for (const String &k : keys) {
		List<Dictionary> *q = pending.getptr(k);
		for (const Dictionary &m : *q) {
			Dictionary msg = m;
			msg["delivered"] = true;
			_deliver(dev, msg);
			emit_signal(SNAME("message_received"), msg);
		}
		pending.erase(k);
	}
}

void OSKernel::register_device(Node *p_device, const String &p_device_id) {
	ERR_FAIL_NULL(p_device);
	ERR_FAIL_COND_MSG(p_device_id.is_empty(), "OSKernel: device id must not be empty.");
	if (devices.has(p_device_id) && devices[p_device_id] != p_device->get_instance_id()) {
		WARN_PRINT("OSKernel: device id '" + p_device_id + "' is already registered by another node; replacing.");
	}
	devices[p_device_id] = p_device->get_instance_id();
	emit_signal(SNAME("device_registered"), p_device_id);
	_flush_pending_for(p_device_id);
}

void OSKernel::unregister_device(const String &p_device_id) {
	if (devices.erase(p_device_id)) {
		emit_signal(SNAME("device_unregistered"), p_device_id);
	}
}

Node *OSKernel::get_device(const String &p_device_id) const {
	const ObjectID *id = devices.getptr(p_device_id);
	if (!id) {
		return nullptr;
	}
	return Object::cast_to<Node>(ObjectDB::get_instance(*id));
}

Array OSKernel::get_devices() const {
	Array out;
	for (const KeyValue<String, ObjectID> &kv : devices) {
		Node *n = Object::cast_to<Node>(ObjectDB::get_instance(kv.value));
		if (!n) {
			continue;
		}
		Dictionary d;
		d["device_id"] = kv.key;
		d["node_path"] = n->is_inside_tree() ? String(n->get_path()) : String();
		d["awake"] = n->has_method("is_awake") ? bool(n->call("is_awake")) : true;
		d["room_id"] = n->has_method("get_room_id") ? String(n->call("get_room_id")) : String();
		d["session"] = get_session_for(kv.key);
		out.push_back(d);
	}
	return out;
}

Dictionary OSKernel::send(const Dictionary &p_message) {
	Dictionary msg = p_message.duplicate();
	String to = msg.get("to", "");
	Dictionary result;
	if (to.is_empty()) {
		result["queued"] = false;
		result["delivered"] = false;
		result["error"] = "message has no 'to'";
		return result;
	}
	if (!msg.has("from")) {
		msg["from"] = "system";
	}
	if (!msg.has("attachments")) {
		msg["attachments"] = Array();
	}
	msg["ts"] = Time::get_singleton()->get_unix_time_from_system();
	String id = "m" + String::num_uint64(next_message_serial++);
	msg["id"] = id;

	// Resolve by durable id: a device id, or any device whose session matches.
	List<Node *> targets;
	Node *direct = get_device(to);
	if (direct) {
		targets.push_back(direct);
	} else {
		for (const KeyValue<String, ObjectID> &kv : devices) {
			if (_session_matches(get_session_for(kv.key), to)) {
				Node *n = Object::cast_to<Node>(ObjectDB::get_instance(kv.value));
				if (n) {
					targets.push_back(n);
				}
			}
		}
	}

	bool delivered = !targets.is_empty();
	msg["delivered"] = delivered;
	history.push_back(msg);
	while (history.size() > 500) {
		history.pop_front();
	}

	if (delivered) {
		for (Node *n : targets) {
			_deliver(n, msg);
		}
		emit_signal(SNAME("message_received"), msg);
	} else {
		// Undeliverable is a state, not an error: queue against the durable id.
		if (!pending.has(to)) {
			pending[to] = List<Dictionary>();
		}
		pending[to].push_back(msg);
	}

	result["id"] = id;
	result["queued"] = !delivered;
	result["delivered"] = delivered;
	return result;
}

Array OSKernel::get_history(int p_limit) const {
	if (p_limit <= 0 || p_limit >= history.size()) {
		return history.duplicate();
	}
	return history.slice(history.size() - p_limit);
}

void OSKernel::_bind_methods() {
	ClassDB::bind_static_method("OSKernel", D_METHOD("get_singleton"), &OSKernel::get_singleton);
	ClassDB::bind_method(D_METHOD("get_world_id"), &OSKernel::get_world_id);
	ClassDB::bind_method(D_METHOD("get_drive_root"), &OSKernel::get_drive_root);
	ClassDB::bind_method(D_METHOD("get_drive"), &OSKernel::get_drive);
	ClassDB::bind_method(D_METHOD("get_identity"), &OSKernel::get_identity);
	ClassDB::bind_method(D_METHOD("register_app", "id", "title", "scene"), &OSKernel::register_app);
	ClassDB::bind_method(D_METHOD("unregister_app", "id"), &OSKernel::unregister_app);
	ClassDB::bind_method(D_METHOD("has_app", "id"), &OSKernel::has_app);
	ClassDB::bind_method(D_METHOD("list_apps"), &OSKernel::list_apps);
	ClassDB::bind_method(D_METHOD("get_session_for", "device_id"), &OSKernel::get_session_for);
	ClassDB::bind_method(D_METHOD("get_devices"), &OSKernel::get_devices);
	ClassDB::bind_method(D_METHOD("get_device", "device_id"), &OSKernel::get_device);
	ClassDB::bind_method(D_METHOD("send", "message"), &OSKernel::send);
	ClassDB::bind_method(D_METHOD("get_history", "limit"), &OSKernel::get_history, DEFVAL(100));

	ADD_SIGNAL(MethodInfo("message_received", PropertyInfo(Variant::DICTIONARY, "message")));
	ADD_SIGNAL(MethodInfo("drive_changed", PropertyInfo(Variant::STRING, "path")));
	ADD_SIGNAL(MethodInfo("device_registered", PropertyInfo(Variant::STRING, "device_id")));
	ADD_SIGNAL(MethodInfo("device_unregistered", PropertyInfo(Variant::STRING, "device_id")));
	ADD_SIGNAL(MethodInfo("apps_changed"));
}
