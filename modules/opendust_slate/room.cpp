/**************************************************************************/
/*  room.cpp                                                              */
/**************************************************************************/
/*                         This file is part of:                          */
/*                              OPENDUST                                  */
/*                        https://opendust.io                             */
/**************************************************************************/
/* Copyright (c) 2026-present OpenDust contributors.                      */
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "room.h"
#include "core/object/class_db.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/os/time.h"

#include "modules/modules_enabled.gen.h" // For opendust_agent.
#ifdef MODULE_OPENDUST_AGENT_ENABLED
#include "modules/opendust_agent/opendust_tool_registry.h"
#endif

void Room::_resolve_dir() {
	String project_dir = ProjectSettings::get_singleton()->globalize_path("res://");
	if (room_dir.is_empty()) {
		resolved_dir = project_dir;
	} else if (room_dir.begins_with("res://") || room_dir.begins_with("user://")) {
		resolved_dir = ProjectSettings::get_singleton()->globalize_path(room_dir);
	} else if (room_dir.is_absolute_path()) {
		resolved_dir = room_dir;
	} else {
		resolved_dir = project_dir.path_join(room_dir);
	}
	resolved_dir = resolved_dir.simplify_path();
	if (resolved_dir.ends_with("/") && resolved_dir.length() > 1) {
		resolved_dir = resolved_dir.substr(0, resolved_dir.length() - 1);
	}
}

String Room::get_absolute_dir() const {
	if (resolved_dir.is_empty()) {
		const_cast<Room *>(this)->_resolve_dir();
	}
	return resolved_dir;
}

void Room::_write_marker() {
	if (resolved_dir.is_empty()) {
		return;
	}
	Error err = DirAccess::make_dir_recursive_absolute(resolved_dir);
	if (err != OK && err != ERR_ALREADY_EXISTS) {
		WARN_PRINT("Room '" + room_id + "': can't create room directory " + resolved_dir);
		return;
	}
	Dictionary marker;
	marker["schema"] = "opendust.room/1";
	marker["room_id"] = room_id;
	marker["display_name"] = display_name;
	marker["scene"] = get_scene_file_path().is_empty() && get_owner() ? get_owner()->get_scene_file_path() : get_scene_file_path();
	marker["node_path"] = String(get_path());
	marker["project_path"] = ProjectSettings::get_singleton()->globalize_path("res://");
	marker["written_at"] = Time::get_singleton()->get_datetime_string_from_system(true);

	String marker_path = resolved_dir.path_join(".opendust-room.json");
	Ref<FileAccess> f = FileAccess::open(marker_path, FileAccess::WRITE, &err);
	if (f.is_null()) {
		WARN_PRINT("Room '" + room_id + "': can't write " + marker_path);
		return;
	}
	f->store_string(JSON::stringify(marker, "  ") + "\n");
	f->close();
}

void Room::_register() {
#ifdef MODULE_OPENDUST_AGENT_ENABLED
	if (registered || room_id.is_empty()) {
		return;
	}
	OpenDustToolRegistry *reg = OpenDustToolRegistry::get_singleton();
	if (reg) {
		reg->register_room(room_id, this);
		registered = true;
	}
#endif
}

void Room::_unregister() {
#ifdef MODULE_OPENDUST_AGENT_ENABLED
	if (!registered) {
		return;
	}
	OpenDustToolRegistry *reg = OpenDustToolRegistry::get_singleton();
	if (reg) {
		reg->unregister_room(room_id);
	}
	registered = false;
#endif
}

Room *Room::find_room_for(Node *p_node) {
	Node *n = p_node;
	while (n) {
		Room *r = Object::cast_to<Room>(n);
		if (r) {
			return r;
		}
		n = n->get_parent();
	}
	return nullptr;
}

void Room::set_room_id(const String &p_id) {
	if (room_id == p_id) {
		return;
	}
	bool was_registered = registered;
	_unregister();
	room_id = p_id;
	if (was_registered) {
		_register();
	}
}

void Room::set_room_dir(const String &p_dir) {
	room_dir = p_dir;
	resolved_dir = String();
	if (is_inside_tree() && !Engine::get_singleton()->is_editor_hint()) {
		_resolve_dir();
		_write_marker();
	}
}

void Room::set_display_name(const String &p_name) {
	display_name = p_name;
}

void Room::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			_resolve_dir();
			if (!Engine::get_singleton()->is_editor_hint()) {
				if (room_id.is_empty()) {
					room_id = String(get_name()).to_snake_case();
				}
				_write_marker();
				_register();
			}
		} break;
		case NOTIFICATION_EXIT_TREE: {
			_unregister();
		} break;
	}
}

void Room::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_room_id", "id"), &Room::set_room_id);
	ClassDB::bind_method(D_METHOD("get_room_id"), &Room::get_room_id);
	ClassDB::bind_method(D_METHOD("set_room_dir", "dir"), &Room::set_room_dir);
	ClassDB::bind_method(D_METHOD("get_room_dir"), &Room::get_room_dir);
	ClassDB::bind_method(D_METHOD("set_display_name", "name"), &Room::set_display_name);
	ClassDB::bind_method(D_METHOD("get_display_name"), &Room::get_display_name);
	ClassDB::bind_method(D_METHOD("get_absolute_dir"), &Room::get_absolute_dir);
	ClassDB::bind_static_method("Room", D_METHOD("find_room_for", "node"), &Room::find_room_for);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "room_id"), "set_room_id", "get_room_id");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "room_dir", PROPERTY_HINT_DIR), "set_room_dir", "get_room_dir");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "display_name"), "set_display_name", "get_display_name");
}

Room::Room() {
}
