/**************************************************************************/
/*  agent_tools.cpp                                                       */
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

#include "agent_tools.h"
#include "core/object/callable_mp.h"

#include "agent_body.h"
#include "soul.h"

#include "core/config/project_settings.h"
#include "core/io/resource_loader.h"
#include "core/object/callable_mp.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"

#include "modules/modules_enabled.gen.h" // IWYU pragma: keep
#ifdef MODULE_OPENDUST_AGENT_ENABLED
#include "modules/opendust_agent/opendust_tool_registry.h"
#endif

// Error codes mirror docs/opendust/01-agent-bridge-protocol.md.
static const int ERR_NOT_FOUND = -32003;
static const int ERR_INVALID_TARGET = -32004;
static const int ERR_DENIED = -32006;
static const int ERR_INVALID_PARAMS = -32602;

/* ---------- AgentBodyRegistry ---------- */

LocalVector<ObjectID> AgentBodyRegistry::bodies;

void AgentBodyRegistry::add(AgentBody *p_body) {
	ERR_FAIL_NULL(p_body);
	ObjectID id = p_body->get_instance_id();
	for (uint32_t i = 0; i < bodies.size(); i++) {
		if (bodies[i] == id) {
			return;
		}
	}
	bodies.push_back(id);
}

void AgentBodyRegistry::remove(AgentBody *p_body) {
	ERR_FAIL_NULL(p_body);
	ObjectID id = p_body->get_instance_id();
	for (uint32_t i = 0; i < bodies.size(); i++) {
		if (bodies[i] == id) {
			bodies.remove_at(i);
			break;
		}
	}
	AgentTools::detach_body(p_body);
}

void AgentBodyRegistry::get_bodies(LocalVector<AgentBody *> &r_bodies) {
	r_bodies.clear();
	for (uint32_t i = 0; i < bodies.size(); i++) {
		AgentBody *b = Object::cast_to<AgentBody>(ObjectDB::get_instance(bodies[i]));
		if (b && b->is_inside_tree()) {
			r_bodies.push_back(b);
		}
	}
}

String AgentBodyRegistry::path_of(AgentBody *p_body) {
	if (!p_body || !p_body->is_inside_tree()) {
		return String();
	}
	SceneTree *tree = p_body->get_tree();
	Node *scene = tree ? tree->get_current_scene() : nullptr;
	if (scene && (scene == p_body || scene->is_ancestor_of(p_body))) {
		return String(scene->get_path_to(p_body));
	}
	return String(p_body->get_path());
}

AgentBody *AgentBodyRegistry::find_by_path(const String &p_path) {
	LocalVector<AgentBody *> list;
	get_bodies(list);
	for (uint32_t i = 0; i < list.size(); i++) {
		if (path_of(list[i]) == p_path || String(list[i]->get_path()) == p_path || String(list[i]->get_name()) == p_path) {
			return list[i];
		}
	}
	return nullptr;
}

/* ---------- AgentTools ---------- */

bool AgentTools::registered = false;
HashMap<String, ObjectID> AgentTools::attachments;

Dictionary AgentTools::_err(int p_code, const String &p_message) {
	Dictionary e;
	e["code"] = p_code;
	e["message"] = p_message;
	Dictionary out;
	out["$error"] = e;
	return out;
}

Dictionary AgentTools::_prop(const String &p_type, const String &p_description) {
	Dictionary d;
	d["type"] = p_type;
	d["description"] = p_description;
	return d;
}

Dictionary AgentTools::_schema(const Dictionary &p_properties, const Array &p_required) {
	Dictionary s;
	s["type"] = "object";
	s["properties"] = p_properties;
	if (p_required.size() > 0) {
		s["required"] = p_required;
	}
	return s;
}

AgentBody *AgentTools::get_attached_body(const String &p_session_id) {
	if (!attachments.has(p_session_id)) {
		return nullptr;
	}
	AgentBody *b = Object::cast_to<AgentBody>(ObjectDB::get_instance(attachments[p_session_id]));
	if (!b || !b->is_inside_tree()) {
		attachments.erase(p_session_id);
		return nullptr;
	}
	return b;
}

void AgentTools::detach_session(const String &p_session_id) {
	attachments.erase(p_session_id);
}

void AgentTools::detach_body(AgentBody *p_body) {
	if (!p_body) {
		return;
	}
	ObjectID id = p_body->get_instance_id();
	LocalVector<String> gone;
	for (const KeyValue<String, ObjectID> &kv : attachments) {
		if (kv.value == id) {
			gone.push_back(kv.key);
		}
	}
	for (uint32_t i = 0; i < gone.size(); i++) {
		attachments.erase(gone[i]);
	}
}

AgentBody *AgentTools::_attached(const Dictionary &p_context, Dictionary *r_error) {
	String sid = p_context.has("session_id") ? String(p_context["session_id"]) : String();
	AgentBody *b = get_attached_body(sid);
	if (!b && r_error) {
		*r_error = _err(ERR_INVALID_TARGET, "No body attached to this session. Call agent.bodies then agent.attach.");
	}
	return b;
}

Dictionary AgentTools::_bodies(const Dictionary &p_params, const Dictionary &p_context) {
	LocalVector<AgentBody *> list;
	AgentBodyRegistry::get_bodies(list);
	Array arr;
	for (uint32_t i = 0; i < list.size(); i++) {
		AgentBody *b = list[i];
		Dictionary d;
		d["path"] = AgentBodyRegistry::path_of(b);
		d["agent_name"] = b->get_agent_name();
		d["agent_id"] = b->get_agent_id();
		d["room_id"] = b->get_room_id();
		String attached;
		for (const KeyValue<String, ObjectID> &kv : attachments) {
			if (kv.value == b->get_instance_id()) {
				attached = kv.key;
				break;
			}
		}
		if (!attached.is_empty()) {
			d["attached_session"] = attached;
		}
		arr.push_back(d);
	}
	Dictionary out;
	out["bodies"] = arr;
	return out;
}

Dictionary AgentTools::_attach(const Dictionary &p_params, const Dictionary &p_context) {
	if (!p_params.has("body_path")) {
		return _err(ERR_INVALID_PARAMS, "body_path is required.");
	}
	AgentBody *b = AgentBodyRegistry::find_by_path(p_params["body_path"]);
	if (!b) {
		return _err(ERR_NOT_FOUND, "No AgentBody at that path.");
	}
	String sid = p_context.has("session_id") ? String(p_context["session_id"]) : String();
	String caller_id = p_context.has("agent_id") ? String(p_context["agent_id"]) : String();

	// One session per body.
	for (const KeyValue<String, ObjectID> &kv : attachments) {
		if (kv.value == b->get_instance_id() && kv.key != sid) {
			return _err(ERR_INVALID_TARGET, "That body is already attached to another session.");
		}
	}
	// Identity must match unless the body is unidentified or the project allows any.
	bool allow_any = GLOBAL_GET("opendust/agent/allow_attach_any_body");
	if (b->is_identified() && !allow_any && b->get_agent_id() != caller_id) {
		return _err(ERR_DENIED, "That body belongs to a different agent_id.");
	}
	attachments[sid] = b->get_instance_id();
	Dictionary out;
	out["attached"] = true;
	out["body_path"] = AgentBodyRegistry::path_of(b);
	out["agent_name"] = b->get_agent_name();
	return out;
}

Dictionary AgentTools::_detach(const Dictionary &p_params, const Dictionary &p_context) {
	String sid = p_context.has("session_id") ? String(p_context["session_id"]) : String();
	bool had = attachments.has(sid);
	detach_session(sid);
	Dictionary out;
	out["detached"] = had;
	return out;
}

Dictionary AgentTools::_perceive(const Dictionary &p_params, const Dictionary &p_context) {
	Dictionary err;
	AgentBody *b = _attached(p_context, &err);
	if (!b) {
		return err;
	}
	bool frame = p_params.has("include_frame") ? bool(p_params["include_frame"]) : false;
	Size2i size(256, 160);
	if (p_params.has("frame_size")) {
		Array fs = p_params["frame_size"];
		if (fs.size() >= 2) {
			size = Size2i(int(fs[0]), int(fs[1]));
		}
	}
	return b->perceive(frame, size);
}

Dictionary AgentTools::_move_to(const Dictionary &p_params, const Dictionary &p_context) {
	Dictionary err;
	AgentBody *b = _attached(p_context, &err);
	if (!b) {
		return err;
	}
	if (!p_params.has("target")) {
		return _err(ERR_INVALID_PARAMS, "target is required ([x,y,z] or a node path).");
	}
	Variant target = p_params["target"];
	if (target.get_type() == Variant::DICTIONARY) {
		Dictionary td = target;
		if (td.has("v")) {
			target = td["v"];
		}
	}
	b->move_to(target);
	Dictionary out;
	out["moving"] = b->is_moving();
	return out;
}

Dictionary AgentTools::_look_at(const Dictionary &p_params, const Dictionary &p_context) {
	Dictionary err;
	AgentBody *b = _attached(p_context, &err);
	if (!b) {
		return err;
	}
	if (!p_params.has("target")) {
		return _err(ERR_INVALID_PARAMS, "target is required.");
	}
	Variant target = p_params["target"];
	if (target.get_type() == Variant::DICTIONARY) {
		Dictionary td = target;
		if (td.has("v")) {
			target = td["v"];
		}
	}
	b->look_at_target(target);
	Dictionary out;
	out["ok"] = true;
	return out;
}

Dictionary AgentTools::_stop(const Dictionary &p_params, const Dictionary &p_context) {
	Dictionary err;
	AgentBody *b = _attached(p_context, &err);
	if (!b) {
		return err;
	}
	b->stop();
	Dictionary out;
	out["ok"] = true;
	return out;
}

Dictionary AgentTools::_interact(const Dictionary &p_params, const Dictionary &p_context) {
	Dictionary err;
	AgentBody *b = _attached(p_context, &err);
	if (!b) {
		return err;
	}
	if (!p_params.has("target")) {
		return _err(ERR_INVALID_PARAMS, "target is required.");
	}
	b->interact(p_params["target"]);
	Dictionary out;
	out["ok"] = true;
	return out;
}

Dictionary AgentTools::_hold(const Dictionary &p_params, const Dictionary &p_context) {
	Dictionary err;
	AgentBody *b = _attached(p_context, &err);
	if (!b) {
		return err;
	}
	if (!p_params.has("item")) {
		return _err(ERR_INVALID_PARAMS, "item is required.");
	}
	b->hold(p_params["item"]);
	Dictionary out;
	out["held"] = b->get_held_item() != nullptr;
	return out;
}

Dictionary AgentTools::_drop(const Dictionary &p_params, const Dictionary &p_context) {
	Dictionary err;
	AgentBody *b = _attached(p_context, &err);
	if (!b) {
		return err;
	}
	b->drop();
	Dictionary out;
	out["ok"] = true;
	return out;
}

Dictionary AgentTools::_say(const Dictionary &p_params, const Dictionary &p_context) {
	Dictionary err;
	AgentBody *b = _attached(p_context, &err);
	if (!b) {
		return err;
	}
	if (!p_params.has("text")) {
		return _err(ERR_INVALID_PARAMS, "text is required.");
	}
	double secs = p_params.has("seconds") ? double(p_params["seconds"]) : 6.0;
	b->say(p_params["text"], secs);
	Dictionary out;
	out["ok"] = true;
	return out;
}

Dictionary AgentTools::_emote(const Dictionary &p_params, const Dictionary &p_context) {
	Dictionary err;
	AgentBody *b = _attached(p_context, &err);
	if (!b) {
		return err;
	}
	if (!p_params.has("name")) {
		return _err(ERR_INVALID_PARAMS, "name is required.");
	}
	b->emote(p_params["name"]);
	Dictionary out;
	out["ok"] = true;
	return out;
}

Dictionary AgentTools::_spawn(const Dictionary &p_params, const Dictionary &p_context) {
	bool allowed = GLOBAL_GET("opendust/agent/allow_spawn");
	if (!allowed) {
		return _err(ERR_DENIED, "agent.spawn is disabled; set opendust/agent/allow_spawn in project settings.");
	}
	if (!p_params.has("soul_path")) {
		return _err(ERR_INVALID_PARAMS, "soul_path is required.");
	}
	SceneTree *tree = SceneTree::get_singleton();
	if (!tree) {
		return _err(ERR_INVALID_TARGET, "No scene tree.");
	}
	Node *root = tree->get_current_scene();
	if (!root) {
		root = tree->get_root();
	}
	if (!root) {
		return _err(ERR_INVALID_TARGET, "No scene to spawn into.");
	}

	Ref<Soul> soul = ResourceLoader::load(p_params["soul_path"], "Soul");
	if (soul.is_null()) {
		return _err(ERR_NOT_FOUND, "Could not load a Soul from soul_path.");
	}

	AgentBody *body = memnew(AgentBody);
	body->set_soul(soul);
	body->set_name(soul->get_display_name().replace(" ", "_"));
	if (p_params.has("avatar_scene")) {
		Ref<PackedScene> scene = ResourceLoader::load(p_params["avatar_scene"], "PackedScene");
		if (scene.is_valid()) {
			body->set_avatar_scene(scene);
		}
	}
	root->add_child(body, true);

	if (p_params.has("at")) {
		Variant at = p_params["at"];
		if (at.get_type() == Variant::DICTIONARY) {
			Dictionary ad = at;
			if (ad.has("v")) {
				at = ad["v"];
			}
		}
		if (at.get_type() == Variant::ARRAY) {
			Array a = at;
			if (a.size() >= 3) {
				body->set_global_position(Vector3(a[0], a[1], a[2]));
			}
		} else if (at.get_type() == Variant::STRING || at.get_type() == Variant::NODE_PATH) {
			Node *n = root->get_node_or_null(NodePath(String(at)));
			Node3D *n3 = Object::cast_to<Node3D>(n);
			if (n3) {
				body->set_global_position(n3->get_global_position());
			}
		}
	}
	body->restore_state();

	Dictionary out;
	out["body_path"] = AgentBodyRegistry::path_of(body);
	out["agent_name"] = body->get_agent_name();
	out["agent_id"] = body->get_agent_id();
	return out;
}

void AgentTools::ensure_registered() {
	if (registered) {
		return;
	}
#ifdef MODULE_OPENDUST_AGENT_ENABLED
	OpenDustToolRegistry *reg = OpenDustToolRegistry::get_singleton();
	if (!reg) {
		return; // opendust_agent hasn't created its singleton yet; AgentBody retries on enter-tree.
	}
	const int RT = OpenDustToolRegistry::TOOL_RUNTIME;
	const int RTM = OpenDustToolRegistry::TOOL_RUNTIME | OpenDustToolRegistry::TOOL_MUTATES;

	Dictionary none;
	Array req;

	reg->register_tool("agent.bodies", "List AgentBody nodes in the running world.", _schema(none), callable_mp_static(&AgentTools::_bodies), RT);

	Dictionary attach_p;
	attach_p["body_path"] = _prop("string", "Path of the AgentBody (from agent.bodies).");
	req.clear();
	req.push_back("body_path");
	reg->register_tool("agent.attach", "Bind this session to a body. One session per body; identified bodies require a matching agent_id.", _schema(attach_p, req), callable_mp_static(&AgentTools::_attach), RTM);

	reg->register_tool("agent.detach", "Release the body this session is driving.", _schema(none), callable_mp_static(&AgentTools::_detach), RTM);

	Dictionary perceive_p;
	perceive_p["include_frame"] = _prop("boolean", "Also return a PNG frame from the body's eyes (first call enables vision; frame arrives next tick).");
	Dictionary fs = _prop("array", "Frame size [width, height]; default [256,160].");
	perceive_p["frame_size"] = fs;
	reg->register_tool("agent.perceive", "What is near the body, what it is looking at, where it is, optionally what it sees.", _schema(perceive_p), callable_mp_static(&AgentTools::_perceive), RT);

	Dictionary target_p;
	target_p["target"] = _prop("string", "A node path, or an [x,y,z] array, or {\"$type\":\"Vector3\",\"v\":[x,y,z]}.");
	req.clear();
	req.push_back("target");
	reg->register_tool("agent.move_to", "Walk the body to a position or node. Emits `arrived` when there.", _schema(target_p, req), callable_mp_static(&AgentTools::_move_to), RTM);
	reg->register_tool("agent.look_at", "Turn the body to face a position or node.", _schema(target_p, req), callable_mp_static(&AgentTools::_look_at), RTM);
	reg->register_tool("agent.stop", "Stop moving.", _schema(none), callable_mp_static(&AgentTools::_stop), RTM);
	reg->register_tool("agent.interact", "Interact with a node (calls its _on_agent_interact if present).", _schema(target_p, req), callable_mp_static(&AgentTools::_interact), RTM);

	Dictionary item_p;
	item_p["item"] = _prop("string", "Node path of a Node3D to pick up.");
	req.clear();
	req.push_back("item");
	reg->register_tool("agent.hold", "Pick up an item and carry it.", _schema(item_p, req), callable_mp_static(&AgentTools::_hold), RTM);
	reg->register_tool("agent.drop", "Put down whatever is held.", _schema(none), callable_mp_static(&AgentTools::_drop), RTM);

	Dictionary say_p;
	say_p["text"] = _prop("string", "What to say. Shown above the body.");
	say_p["seconds"] = _prop("number", "How long the bubble stays (default 6).");
	req.clear();
	req.push_back("text");
	reg->register_tool("agent.say", "Say something out loud in the world.", _schema(say_p, req), callable_mp_static(&AgentTools::_say), RTM);

	Dictionary emote_p;
	emote_p["name"] = _prop("string", "Animation name on the avatar (e.g. \"wave\").");
	req.clear();
	req.push_back("name");
	reg->register_tool("agent.emote", "Play an expressive animation.", _schema(emote_p, req), callable_mp_static(&AgentTools::_emote), RTM);

	Dictionary spawn_p;
	spawn_p["soul_path"] = _prop("string", "Path to a soul.md (res:// or absolute).");
	spawn_p["at"] = _prop("string", "Optional [x,y,z] or node path to place the body.");
	spawn_p["avatar_scene"] = _prop("string", "Optional PackedScene path for the avatar.");
	req.clear();
	req.push_back("soul_path");
	reg->register_tool("agent.spawn", "Instantiate an AgentBody from a soul (project must allow it).", _schema(spawn_p, req), callable_mp_static(&AgentTools::_spawn), RTM);

	registered = true;
#endif
}

void AgentTools::unregister_all() {
	attachments.clear();
#ifdef MODULE_OPENDUST_AGENT_ENABLED
	if (!registered) {
		return;
	}
	OpenDustToolRegistry *reg = OpenDustToolRegistry::get_singleton();
	if (reg) {
		const char *names[] = { "agent.bodies", "agent.attach", "agent.detach", "agent.perceive", "agent.move_to", "agent.look_at", "agent.stop", "agent.interact", "agent.hold", "agent.drop", "agent.say", "agent.emote", "agent.spawn" };
		for (const char *n : names) {
			reg->unregister_tool(n);
		}
	}
#endif
	registered = false;
}
