/**************************************************************************/
/*  opendust_runtime_tools.cpp                                            */
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

#include "opendust_runtime_tools.h"

#include "opendust_json.h"
#include "opendust_node_utils.h"
#include "opendust_tool_registry.h"

#include "core/config/engine.h"
#include "core/os/os.h"
#include "scene/main/node.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"

OpenDustRuntimeTools *OpenDustRuntimeTools::singleton = nullptr;

OpenDustRuntimeTools *OpenDustRuntimeTools::get_singleton() {
	return singleton;
}

Node *OpenDustRuntimeTools::_root() const {
	SceneTree *tree = SceneTree::get_singleton();
	if (!tree) {
		return nullptr;
	}
	return tree->get_root();
}

Node *OpenDustRuntimeTools::_resolve(const String &p_path, Dictionary &r_error) const {
	Node *root = _root();
	if (!root) {
		r_error = OpenDustToolRegistry::make_error(OpenDustToolRegistry::RPC_ENGINE_BUSY, "No scene tree.");
		return nullptr;
	}
	Node *n = OpenDustNodeUtils::resolve(root, p_path);
	if (!n) {
		r_error = OpenDustToolRegistry::make_error(OpenDustToolRegistry::RPC_NOT_FOUND, vformat("Node '%s' not found.", p_path));
	}
	return n;
}

Dictionary OpenDustRuntimeTools::_world_tree(const Dictionary &p_params, const Dictionary &p_context) {
	Dictionary err;
	String root_path = p_params.get("root", "/root");
	Node *n = _resolve(root_path, err);
	if (!n) {
		return err;
	}
	int depth = p_params.get("depth", -1);
	bool props = p_params.get("include_properties", false);
	Dictionary out;
	out["tree"] = OpenDustNodeUtils::serialize_tree(_root(), n, depth, props);
	return out;
}

Dictionary OpenDustRuntimeTools::_world_rooms(const Dictionary &p_params, const Dictionary &p_context) {
	Dictionary out;
	OpenDustToolRegistry *reg = OpenDustToolRegistry::get_singleton();
	out["rooms"] = reg ? reg->get_rooms() : Array();
	return out;
}

Dictionary OpenDustRuntimeTools::_world_room_of(const Dictionary &p_params, const Dictionary &p_context) {
	Dictionary err;
	Node *n = _resolve(p_params.get("node_path", "/root"), err);
	if (!n) {
		return err;
	}
	OpenDustToolRegistry *reg = OpenDustToolRegistry::get_singleton();
	Node *room = reg ? reg->find_room_for_node(n) : nullptr;
	Dictionary out;
	if (!room) {
		out["room"] = Variant();
		return out;
	}
	Dictionary r;
	bool valid = false;
	Variant id = room->get("room_id", &valid);
	r["room_id"] = valid ? id : Variant(String(room->get_name()));
	Variant dir = room->get("room_dir", &valid);
	r["room_dir"] = valid ? dir : Variant("");
	r["node_path"] = String(room->get_path());
	out["room"] = r;
	return out;
}

Dictionary OpenDustRuntimeTools::_world_capture(const Dictionary &p_params, const Dictionary &p_context) {
	SceneTree *tree = SceneTree::get_singleton();
	if (!tree) {
		return OpenDustToolRegistry::make_error(OpenDustToolRegistry::RPC_ENGINE_BUSY, "No scene tree.");
	}
	Vector2i size;
	Variant vsize = p_params.get("size", Variant());
	if (vsize.get_type() == Variant::ARRAY) {
		Array a = vsize;
		if (a.size() >= 2) {
			size = Vector2i((int)a[0], (int)a[1]);
		}
	} else if (vsize.get_type() == Variant::VECTOR2I || vsize.get_type() == Variant::VECTOR2) {
		size = vsize.operator Vector2i();
	}
	Window *root = tree->get_root();
	Dictionary d = OpenDustNodeUtils::capture_viewport(root, size);
	if (d.is_empty()) {
		return OpenDustToolRegistry::make_error(OpenDustToolRegistry::RPC_INTERNAL_ERROR, "Viewport capture failed.");
	}
	return d;
}

Dictionary OpenDustRuntimeTools::_world_time(const Dictionary &p_params, const Dictionary &p_context) {
	Dictionary d;
	d["ticks_ms"] = (int64_t)OS::get_singleton()->get_ticks_msec();
	d["frame"] = (int64_t)Engine::get_singleton()->get_process_frames();
	d["physics_frame"] = (int64_t)Engine::get_singleton()->get_physics_frames();
	d["time_scale"] = Engine::get_singleton()->get_time_scale();
	SceneTree *tree = SceneTree::get_singleton();
	d["paused"] = tree ? tree->is_paused() : false;
	return d;
}

Dictionary OpenDustRuntimeTools::_world_node_get(const Dictionary &p_params, const Dictionary &p_context) {
	Dictionary err;
	Node *n = _resolve(p_params.get("path", "/root"), err);
	if (!n) {
		return err;
	}
	Array filter = p_params.get("properties", Array());
	return OpenDustNodeUtils::serialize_node(_root(), n, filter, filter.is_empty());
}

Dictionary OpenDustRuntimeTools::_world_node_call(const Dictionary &p_params, const Dictionary &p_context) {
	Dictionary err;
	Node *n = _resolve(p_params.get("path", ""), err);
	if (!n) {
		return err;
	}
	String method = p_params.get("method", "");
	if (method.is_empty() || !n->has_method(method)) {
		return OpenDustToolRegistry::make_error(OpenDustToolRegistry::RPC_NOT_FOUND, vformat("Method '%s' not found on %s.", method, String(n->get_path())));
	}
	Array args = OpenDustJSON::from_json(p_params.get("args", Array()));
	Variant ret = n->callv(method, args);
	Dictionary out;
	out["result"] = OpenDustJSON::to_json(ret);
	return out;
}

void OpenDustRuntimeTools::register_tools() {
	OpenDustToolRegistry *reg = OpenDustToolRegistry::get_singleton();
	ERR_FAIL_NULL(reg);
	if (registered) {
		return;
	}
	registered = true;
	const int R = OpenDustToolRegistry::TOOL_RUNTIME;
	const int RM = OpenDustToolRegistry::TOOL_RUNTIME | OpenDustToolRegistry::TOOL_MUTATES;

	Dictionary p;
	p["root"] = OpenDustToolRegistry::prop("string", "Node path to start from (default /root).");
	p["depth"] = OpenDustToolRegistry::prop("integer", "Levels to descend; -1 for all.");
	p["include_properties"] = OpenDustToolRegistry::prop("boolean", "Include editor-visible properties per node.");
	reg->register_tool("world.tree", "Serialize the live scene tree.", OpenDustToolRegistry::object_schema(p), callable_mp(this, &OpenDustRuntimeTools::_world_tree), R);

	reg->register_tool("world.rooms", "List registered Room nodes and their working directories.", OpenDustToolRegistry::object_schema(Dictionary()), callable_mp(this, &OpenDustRuntimeTools::_world_rooms), R);

	p = Dictionary();
	p["node_path"] = OpenDustToolRegistry::prop("string", "Node whose enclosing Room to find.");
	Array req;
	req.push_back("node_path");
	reg->register_tool("world.room_of", "Find the Room that contains a node.", OpenDustToolRegistry::object_schema(p, req), callable_mp(this, &OpenDustRuntimeTools::_world_room_of), R);

	p = Dictionary();
	p["size"] = OpenDustToolRegistry::prop("array", "[width, height] to downscale to; omit for native size.");
	reg->register_tool("world.capture", "PNG (base64) of the game's main viewport.", OpenDustToolRegistry::object_schema(p), callable_mp(this, &OpenDustRuntimeTools::_world_capture), R | OpenDustToolRegistry::TOOL_SLOW);

	reg->register_tool("world.time", "Engine ticks, frame counters, time scale, pause state.", OpenDustToolRegistry::object_schema(Dictionary()), callable_mp(this, &OpenDustRuntimeTools::_world_time), R);

	p = Dictionary();
	p["path"] = OpenDustToolRegistry::prop("string", "Node path.");
	p["properties"] = OpenDustToolRegistry::prop("array", "Property names to read; omit for all editor-visible properties.");
	req = Array();
	req.push_back("path");
	reg->register_tool("world.node_get", "Read a node's type, groups, signals and properties.", OpenDustToolRegistry::object_schema(p, req), callable_mp(this, &OpenDustRuntimeTools::_world_node_get), R);

	p = Dictionary();
	p["path"] = OpenDustToolRegistry::prop("string", "Node path.");
	p["method"] = OpenDustToolRegistry::prop("string", "Method name.");
	p["args"] = OpenDustToolRegistry::prop("array", "Positional arguments.");
	req = Array();
	req.push_back("path");
	req.push_back("method");
	reg->register_tool("world.node_call", "Call a method on a live node (policy-gated).", OpenDustToolRegistry::object_schema(p, req), callable_mp(this, &OpenDustRuntimeTools::_world_node_call), RM);
}

void OpenDustRuntimeTools::unregister_tools() {
	OpenDustToolRegistry *reg = OpenDustToolRegistry::get_singleton();
	if (!reg || !registered) {
		return;
	}
	registered = false;
	const char *names[] = { "world.tree", "world.rooms", "world.room_of", "world.capture", "world.time", "world.node_get", "world.node_call" };
	for (const char *n : names) {
		reg->unregister_tool(n);
	}
}

OpenDustRuntimeTools::OpenDustRuntimeTools() {
	singleton = this;
}

OpenDustRuntimeTools::~OpenDustRuntimeTools() {
	unregister_tools();
	if (singleton == this) {
		singleton = nullptr;
	}
}
