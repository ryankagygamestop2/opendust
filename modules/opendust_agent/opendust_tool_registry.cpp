/**************************************************************************/
/*  opendust_tool_registry.cpp                                            */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             OPENDUST ENGINE                            */
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

#include "opendust_tool_registry.h"
#include "core/object/class_db.h"

#include "core/object/object.h"
#include "core/variant/variant.h"
#include "scene/main/node.h"

OpenDustToolRegistry *OpenDustToolRegistry::singleton = nullptr;

OpenDustToolRegistry *OpenDustToolRegistry::get_singleton() {
	return singleton;
}

void OpenDustToolRegistry::_bump_version() {
	capabilities_version++;
	emit_signal(SNAME("capabilities_changed"), capabilities_version);
}

void OpenDustToolRegistry::register_tool(const String &p_name, const String &p_description, const Dictionary &p_input_schema, const Callable &p_handler, int p_flags) {
	ERR_FAIL_COND_MSG(p_name.is_empty(), "Tool name cannot be empty.");
	ERR_FAIL_COND_MSG(!p_handler.is_valid(), vformat("Tool '%s' handler is not a valid Callable.", p_name));
	ERR_FAIL_COND_MSG((p_flags & (TOOL_EDITOR | TOOL_RUNTIME)) == 0, vformat("Tool '%s' must declare TOOL_EDITOR and/or TOOL_RUNTIME.", p_name));

	Tool t;
	t.name = p_name;
	t.description = p_description;
	t.input_schema = p_input_schema;
	t.handler = p_handler;
	t.flags = p_flags;

	if (!tools.has(p_name)) {
		tool_order.push_back(p_name);
	}
	tools[p_name] = t;
	_bump_version();
}

void OpenDustToolRegistry::unregister_tool(const String &p_name) {
	if (!tools.has(p_name)) {
		return;
	}
	tools.erase(p_name);
	int idx = tool_order.find(p_name);
	if (idx >= 0) {
		tool_order.remove_at(idx);
	}
	_bump_version();
}

bool OpenDustToolRegistry::has_tool(const String &p_name) const {
	return tools.has(p_name);
}

int OpenDustToolRegistry::get_tool_flags(const String &p_name) const {
	const Tool *t = tools.getptr(p_name);
	return t ? t->flags : 0;
}

Array OpenDustToolRegistry::flags_to_array(int p_flags) {
	Array out;
	if (p_flags & TOOL_EDITOR) {
		out.push_back("editor");
	}
	if (p_flags & TOOL_RUNTIME) {
		out.push_back("runtime");
	}
	if (p_flags & TOOL_MUTATES) {
		out.push_back("mutates");
	}
	if (p_flags & TOOL_SLOW) {
		out.push_back("slow");
	}
	return out;
}

Array OpenDustToolRegistry::list_tools(int p_mode_mask) const {
	Array out;
	for (const String &name : tool_order) {
		const Tool *t = tools.getptr(name);
		if (!t) {
			continue;
		}
		if (p_mode_mask != 0 && (t->flags & p_mode_mask) == 0) {
			continue;
		}
		Dictionary spec;
		spec["name"] = t->name;
		spec["description"] = t->description;
		spec["inputSchema"] = t->input_schema;
		spec["flags"] = flags_to_array(t->flags);
		out.push_back(spec);
	}
	return out;
}

Dictionary OpenDustToolRegistry::make_error(int p_code, const String &p_message) {
	Dictionary err;
	err["code"] = p_code;
	err["message"] = p_message;
	Dictionary out;
	out["$error"] = err;
	return out;
}

bool OpenDustToolRegistry::is_error(const Variant &p_result) {
	if (p_result.get_type() != Variant::DICTIONARY) {
		return false;
	}
	return Dictionary(p_result).has("$error");
}

Dictionary OpenDustToolRegistry::object_schema(const Dictionary &p_properties, const Array &p_required) {
	Dictionary schema;
	schema["type"] = "object";
	schema["properties"] = p_properties;
	if (!p_required.is_empty()) {
		schema["required"] = p_required;
	}
	return schema;
}

Dictionary OpenDustToolRegistry::prop(const String &p_type, const String &p_description) {
	Dictionary d;
	if (!p_type.is_empty()) {
		d["type"] = p_type;
	}
	if (!p_description.is_empty()) {
		d["description"] = p_description;
	}
	return d;
}

Variant OpenDustToolRegistry::call_tool(const String &p_name, const Dictionary &p_params, const Dictionary &p_context, Dictionary &r_error) {
	r_error = Dictionary();
	const Tool *t = tools.getptr(p_name);
	if (!t) {
		r_error["code"] = RPC_METHOD_NOT_FOUND;
		r_error["message"] = vformat("Unknown tool '%s'.", p_name);
		return Variant();
	}
	if (!t->handler.is_valid()) {
		r_error["code"] = RPC_INTERNAL_ERROR;
		r_error["message"] = vformat("Tool '%s' handler is no longer valid (its owner was freed).", p_name);
		return Variant();
	}

	Variant vparams = p_params;
	Variant vcontext = p_context;
	const Variant *args[2] = { &vparams, &vcontext };
	Variant ret;
	Callable::CallError ce;
	t->handler.callp(args, 2, ret, ce);
	if (ce.error != Callable::CallError::CALL_OK) {
		r_error["code"] = RPC_INTERNAL_ERROR;
		r_error["message"] = vformat("Tool '%s' handler call failed: %s", p_name, Variant::get_callable_error_text(t->handler, args, 2, ce));
		return Variant();
	}
	if (is_error(ret)) {
		Dictionary e = Dictionary(ret)["$error"];
		r_error["code"] = e.get("code", (int)RPC_INTERNAL_ERROR);
		r_error["message"] = e.get("message", "Tool failed.");
		if (e.has("detail")) {
			r_error["detail"] = e["detail"];
		}
		return Variant();
	}
	return ret;
}

Dictionary OpenDustToolRegistry::_call_tool_bind(const String &p_name, const Dictionary &p_params, const Dictionary &p_context) {
	Dictionary err;
	Variant res = call_tool(p_name, p_params, p_context, err);
	Dictionary out;
	if (!err.is_empty()) {
		out["error"] = err;
	} else {
		out["result"] = res;
	}
	return out;
}

int OpenDustToolRegistry::get_capabilities_version() const {
	return capabilities_version;
}

void OpenDustToolRegistry::register_room(const String &p_room_id, Node *p_node) {
	ERR_FAIL_COND(p_room_id.is_empty());
	ERR_FAIL_NULL(p_node);
	if (!rooms.has(p_room_id)) {
		room_order.push_back(p_room_id);
	}
	rooms[p_room_id] = p_node->get_instance_id();
}

void OpenDustToolRegistry::unregister_room(const String &p_room_id) {
	if (!rooms.has(p_room_id)) {
		return;
	}
	rooms.erase(p_room_id);
	int idx = room_order.find(p_room_id);
	if (idx >= 0) {
		room_order.remove_at(idx);
	}
}

Node *OpenDustToolRegistry::get_room_node(const String &p_room_id) const {
	const ObjectID *id = rooms.getptr(p_room_id);
	if (!id) {
		return nullptr;
	}
	return Object::cast_to<Node>(ObjectDB::get_instance(*id));
}

Array OpenDustToolRegistry::get_rooms() const {
	Array out;
	for (const String &id : room_order) {
		Node *n = get_room_node(id);
		if (!n || !n->is_inside_tree()) {
			continue;
		}
		Dictionary d;
		d["room_id"] = id;
		d["node_path"] = String(n->get_path());
		bool valid = false;
		Variant dir = n->get("room_dir", &valid);
		d["room_dir"] = valid ? dir : Variant("");
		Variant name = n->get("display_name", &valid);
		d["display_name"] = valid ? name : Variant(id);
		out.push_back(d);
	}
	return out;
}

Node *OpenDustToolRegistry::find_room_for_node(Node *p_node) const {
	if (!p_node) {
		return nullptr;
	}
	// Walk up to the nearest registered room.
	Node *cur = p_node;
	while (cur) {
		for (const String &id : room_order) {
			Node *room = get_room_node(id);
			if (room == cur) {
				return room;
			}
		}
		cur = cur->get_parent();
	}
	return nullptr;
}

void OpenDustToolRegistry::_bind_methods() {
	ClassDB::bind_method(D_METHOD("register_tool", "name", "description", "input_schema", "handler", "flags"), &OpenDustToolRegistry::register_tool);
	ClassDB::bind_method(D_METHOD("unregister_tool", "name"), &OpenDustToolRegistry::unregister_tool);
	ClassDB::bind_method(D_METHOD("has_tool", "name"), &OpenDustToolRegistry::has_tool);
	ClassDB::bind_method(D_METHOD("get_tool_flags", "name"), &OpenDustToolRegistry::get_tool_flags);
	ClassDB::bind_method(D_METHOD("list_tools", "mode_mask"), &OpenDustToolRegistry::list_tools, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("call_tool", "name", "params", "context"), &OpenDustToolRegistry::_call_tool_bind, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("get_capabilities_version"), &OpenDustToolRegistry::get_capabilities_version);
	ClassDB::bind_method(D_METHOD("register_room", "room_id", "node"), &OpenDustToolRegistry::register_room);
	ClassDB::bind_method(D_METHOD("unregister_room", "room_id"), &OpenDustToolRegistry::unregister_room);
	ClassDB::bind_method(D_METHOD("get_rooms"), &OpenDustToolRegistry::get_rooms);
	ClassDB::bind_method(D_METHOD("get_room_node", "room_id"), &OpenDustToolRegistry::get_room_node);
	ClassDB::bind_method(D_METHOD("find_room_for_node", "node"), &OpenDustToolRegistry::find_room_for_node);
	ClassDB::bind_static_method("OpenDustToolRegistry", D_METHOD("make_error", "code", "message"), &OpenDustToolRegistry::make_error);

	ADD_SIGNAL(MethodInfo("capabilities_changed", PropertyInfo(Variant::INT, "version")));

	BIND_ENUM_CONSTANT(TOOL_EDITOR);
	BIND_ENUM_CONSTANT(TOOL_RUNTIME);
	BIND_ENUM_CONSTANT(TOOL_MUTATES);
	BIND_ENUM_CONSTANT(TOOL_SLOW);
}

OpenDustToolRegistry::OpenDustToolRegistry() {
	singleton = this;
}

OpenDustToolRegistry::~OpenDustToolRegistry() {
	if (singleton == this) {
		singleton = nullptr;
	}
}
