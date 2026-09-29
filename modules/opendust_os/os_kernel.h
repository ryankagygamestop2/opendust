/**************************************************************************/
/*  os_kernel.h                                                           */
/**************************************************************************/
/*                         This file is part of:                          */
/*                              OPENDUST                                  */
/*                         https://opendust.io                            */
/**************************************************************************/
/* OpenDust is a fork of Godot Engine. MIT licensed; see LICENSE.txt.     */
/**************************************************************************/

#pragma once

#include "core/templates/hash_map.h"
#include "core/templates/list.h"
#include "opendust_identity.h"
#include "scene/main/node.h"
#include "scene/resources/packed_scene.h"
#include "world_drive.h"

class Control;

// One per running world. Owns the world drive, the app registry, sessions and
// the message bus. Created lazily by the first Device that enters the tree.
class OSKernel : public Node {
	GDCLASS(OSKernel, Node);

public:
	typedef Control *(*BuiltinAppFactory)();

private:
	struct AppEntry {
		String id;
		String title;
		Ref<PackedScene> scene;
		BuiltinAppFactory factory = nullptr;
	};

	static OSKernel *singleton;
	static HashMap<String, AppEntry> builtin_apps;

	String world_id;
	String drive_root;
	Ref<WorldDrive> drive;
	Ref<OpenDustIdentity> identity;

	HashMap<String, AppEntry> apps;
	HashMap<String, ObjectID> devices; // device_id → node
	HashMap<String, List<Dictionary>> pending; // durable id → queued messages
	Array history;
	uint64_t next_message_serial = 1;

	void _on_drive_changed(const String &p_path);
	bool _session_matches(const Dictionary &p_session, const String &p_id) const;
	void _deliver(Node *p_device, const Dictionary &p_msg);
	void _flush_pending_for(const String &p_device_id);

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	static OSKernel *get_singleton();
	// Find-or-create. Adds the kernel under the tree root (deferred) and
	// returns it immediately; get_singleton() is valid from the constructor.
	static OSKernel *ensure(SceneTree *p_tree);

	static void add_builtin_app(const String &p_id, const String &p_title, BuiltinAppFactory p_factory);
	static void register_builtin_apps();

	String get_world_id() const;
	String get_drive_root() const;
	Ref<WorldDrive> get_drive() const;
	Ref<OpenDustIdentity> get_identity() const;

	void register_app(const String &p_id, const String &p_title, const Ref<PackedScene> &p_scene);
	void unregister_app(const String &p_id);
	bool has_app(const String &p_id) const;
	Array list_apps() const; // [{id, title, builtin}]
	Control *instantiate_app(const String &p_id);

	Dictionary get_session_for(const String &p_device_id) const;

	void register_device(Node *p_device, const String &p_device_id);
	void unregister_device(const String &p_device_id);
	Array get_devices() const; // [{device_id, node_path, awake, session, room_id}]
	Node *get_device(const String &p_device_id) const;

	// {from?, to, body, attachments?} → {id, queued, delivered}
	Dictionary send(const Dictionary &p_message);
	Array get_history(int p_limit = 100) const;

	OSKernel();
	~OSKernel();
};
