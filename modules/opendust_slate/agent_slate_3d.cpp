/**************************************************************************/
/*  agent_slate_3d.cpp                                                    */
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

#include "agent_slate_3d.h"

#include "room.h"
#include "slate_panel.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/input/input_event.h"
#include "core/input/input_map.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/animation/tween.h"
#include "scene/main/viewport.h"
#include "scene/resources/3d/primitive_meshes.h"
#include "scene/resources/material.h"

// --- construction -----------------------------------------------------------

void AgentSlate3D::_build_children() {
	viewport = memnew(SubViewport);
	viewport->set_name("SlateViewport");
	viewport->set_size(screen_size_px);
	viewport->set_disable_3d(true);
	viewport->set_transparent_background(false);
	viewport->set_update_mode(SubViewport::UPDATE_ALWAYS);
	viewport->set_clear_mode(SubViewport::CLEAR_MODE_ALWAYS);
	add_child(viewport, false, Node::INTERNAL_MODE_FRONT);

	Ref<QuadMesh> quad;
	quad.instantiate();
	screen = memnew(MeshInstance3D);
	screen->set_name("Screen");
	screen->set_mesh(quad);
	add_child(screen, false, Node::INTERNAL_MODE_FRONT);

	screen_material.instantiate();
	screen_material->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	screen_material->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
	screen_material->set_flag(BaseMaterial3D::FLAG_ALBEDO_TEXTURE_FORCE_SRGB, true);
	screen->set_material_override(screen_material);

	_apply_screen_size();
}

void AgentSlate3D::_ensure_panel() {
	// Deferred to ready so that panel_scene set from a scene file is honoured.
	if (panel || !viewport) {
		return;
	}
	if (panel_scene.is_valid()) {
		Node *inst = panel_scene->instantiate();
		panel = Object::cast_to<Control>(inst);
		if (!panel) {
			WARN_PRINT("AgentSlate3D: panel_scene root is not a Control; using the built-in SlatePanel.");
			if (inst) {
				memdelete(inst);
			}
		}
	}
	if (!panel) {
		panel = memnew(SlatePanel);
	}
	panel->set_name("Panel");
	panel->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	viewport->add_child(panel);
}

void AgentSlate3D::_apply_screen_size() {
	if (viewport) {
		viewport->set_size(screen_size_px);
	}
	if (screen) {
		Ref<QuadMesh> quad = screen->get_mesh();
		if (quad.is_valid()) {
			float aspect = screen_size_px.x > 0 ? (float)screen_size_px.y / (float)screen_size_px.x : 0.75f;
			quad->set_size(Size2(screen_width_m, screen_width_m * aspect));
		}
	}
}

// --- room / cwd -------------------------------------------------------------

Room *AgentSlate3D::get_room() const {
	return Room::find_room_for(const_cast<AgentSlate3D *>(this));
}

String AgentSlate3D::get_cwd() const {
	Room *room = get_room();
	if (room) {
		return room->get_absolute_dir();
	}
	return ProjectSettings::get_singleton()->globalize_path("res://").simplify_path();
}

// --- session persistence ----------------------------------------------------

String AgentSlate3D::_session_state_path(const String &p_cwd) const {
	return p_cwd.path_join(".opendust-slate.json");
}

String AgentSlate3D::_load_last_session_id(const String &p_cwd) const {
	String path = _session_state_path(p_cwd);
	if (!FileAccess::exists(path)) {
		return String();
	}
	Ref<FileAccess> f = FileAccess::open(path, FileAccess::READ);
	if (f.is_null()) {
		return String();
	}
	Variant v = JSON::parse_string(f->get_as_text());
	if (v.get_type() != Variant::DICTIONARY) {
		return String();
	}
	Dictionary d = v;
	Dictionary slates = d.get("slates", Dictionary());
	Dictionary mine = slates.get(agent_name, Dictionary());
	return mine.get("session_id", "");
}

void AgentSlate3D::_save_last_session_id(const String &p_cwd, const String &p_session_id) const {
	String path = _session_state_path(p_cwd);
	Dictionary d;
	if (FileAccess::exists(path)) {
		Ref<FileAccess> f = FileAccess::open(path, FileAccess::READ);
		if (f.is_valid()) {
			Variant v = JSON::parse_string(f->get_as_text());
			if (v.get_type() == Variant::DICTIONARY) {
				d = v;
			}
		}
	}
	d["schema"] = "opendust.slate-state/1";
	Dictionary slates = d.get("slates", Dictionary());
	Dictionary mine;
	mine["session_id"] = p_session_id; // an address, not an identity: safe to lose
	mine["agent_name"] = agent_name;
	slates[agent_name] = mine;
	d["slates"] = slates;
	Ref<FileAccess> f = FileAccess::open(path, FileAccess::WRITE);
	if (f.is_valid()) {
		f->store_string(JSON::stringify(d, "  ") + "\n");
		f->close();
	}
}

// --- session ----------------------------------------------------------------

String AgentSlate3D::_bridge_mcp_config_path() const {
	return ProjectSettings::get_singleton()->globalize_path("res://.opendust/mcp-runtime.json").simplify_path();
}

String AgentSlate3D::_permission_mode_to_flag(PermissionMode p_mode) {
	switch (p_mode) {
		case PERMISSION_ACCEPT_EDITS:
			return "acceptEdits";
		case PERMISSION_PLAN:
			return "plan";
		case PERMISSION_BYPASS:
			return "bypassPermissions";
		case PERMISSION_DONT_ASK:
			return "dontAsk";
		case PERMISSION_AUTO:
			return "auto";
		case PERMISSION_DEFAULT:
		default:
			return "default";
	}
}

Dictionary AgentSlate3D::_build_session_options(const String &p_cwd) {
	Dictionary opts;
	opts["permission_mode"] = _permission_mode_to_flag(permission_mode);
	opts["name"] = agent_name;
	if (!model.is_empty()) {
		opts["model"] = model;
	}
	if (!allowed_tools.is_empty()) {
		opts["allowed_tools"] = allowed_tools;
	}
	if (!soul_path.is_empty()) {
		String sp = soul_path;
		if (sp.begins_with("res://") || sp.begins_with("user://")) {
			sp = ProjectSettings::get_singleton()->globalize_path(sp);
		}
		if (FileAccess::exists(sp)) {
			opts["append_system_prompt_file"] = sp;
		} else {
			WARN_PRINT("AgentSlate3D: soul_path does not exist: " + sp);
		}
	}
	if (attach_bridge) {
		String mcp = _bridge_mcp_config_path();
		if (FileAccess::exists(mcp)) {
			opts["mcp_config"] = mcp;
		} else if (!warned_no_bridge) {
			warned_no_bridge = true;
			WARN_PRINT("AgentSlate3D: attach_bridge is on but " + mcp + " doesn't exist. Is the opendust_agent runtime server running? The session starts without world tools.");
		}
	}
	if (resume_last_session) {
		String last = _load_last_session_id(p_cwd);
		if (!last.is_empty()) {
			opts["resume"] = last;
		}
	}
	return opts;
}

Error AgentSlate3D::start_session() {
	if (Engine::get_singleton()->is_editor_hint()) {
		return ERR_UNAVAILABLE;
	}
	if (session.is_valid() && session->is_running()) {
		return ERR_ALREADY_IN_USE;
	}
	_ensure_panel();
	String cwd = get_cwd();
	if (session.is_null()) {
		session.instantiate();
		session->connect(SNAME("started"), callable_mp(this, &AgentSlate3D::_on_session_started));
		session->connect(SNAME("exited"), callable_mp(this, &AgentSlate3D::_on_session_exited));
	}
	SlatePanel *sp = Object::cast_to<SlatePanel>(panel);
	if (sp) {
		sp->bind_room(get_room());
		sp->bind_session(session);
		if (!sp->is_connected(SNAME("permission_requested"), callable_mp(this, &AgentSlate3D::_on_panel_permission_requested))) {
			sp->connect(SNAME("permission_requested"), callable_mp(this, &AgentSlate3D::_on_panel_permission_requested));
		}
	} else if (panel && panel->has_method("bind_session")) {
		panel->call("bind_session", session);
	}

	Dictionary opts = _build_session_options(cwd);
	Error err = session->start(cwd, opts);
	if (err != OK) {
		if (sp) {
			sp->append_system_text("couldn't start claude (" + itos(err) + "). Check opendust/slate/claude_path.");
		}
		return err;
	}
	if (sp) {
		sp->append_system_text("starting in " + cwd + (opts.has("resume") ? " (resuming)" : ""));
	}
	emit_signal(SNAME("session_started"));
	return OK;
}

void AgentSlate3D::stop_session() {
	if (session.is_valid()) {
		session->stop();
	}
}

bool AgentSlate3D::is_session_running() const {
	return session.is_valid() && session->is_running();
}

void AgentSlate3D::_on_session_started(const String &p_session_id) {
	if (!p_session_id.is_empty()) {
		_save_last_session_id(get_cwd(), p_session_id);
	}
}

void AgentSlate3D::_on_session_exited(int p_code) {
	emit_signal(SNAME("session_exited"), p_code);
}

void AgentSlate3D::_on_panel_permission_requested(const Dictionary &p_request) {
	// Default policy for a held slate: the holder decides. We surface it and
	// let a project script answer; if nobody is listening, deny so the agent
	// keeps moving rather than hanging.
	if (has_connections(SNAME("permission_requested"))) {
		emit_signal(SNAME("permission_requested"), p_request);
		return;
	}
	if (session.is_valid()) {
		String request_id = p_request.get("request_id", "");
		if (!request_id.is_empty()) {
			session->respond_permission(request_id, false, Dictionary(), "No one is holding the slate to approve this.");
		}
	}
}

// --- raise / lower ----------------------------------------------------------

void AgentSlate3D::_animate(bool p_raised, bool p_instant) {
	Vector3 target = p_raised ? raised_position : lowered_position;
	if (raise_tween.is_valid() && raise_tween->is_valid()) {
		raise_tween->kill();
	}
	if (p_instant || !is_inside_tree() || raise_time <= 0.0f) {
		set_position(target);
		return;
	}
	raise_tween = create_tween();
	raise_tween->set_trans(Tween::TRANS_CUBIC);
	raise_tween->set_ease(Tween::EASE_OUT);
	raise_tween->tween_property(this, NodePath("position"), target, raise_time);
}

void AgentSlate3D::set_raised(bool p_raised) {
	if (raised == p_raised) {
		return;
	}
	raised = p_raised;
	bool in_game = is_inside_tree() && !Engine::get_singleton()->is_editor_hint();
	_animate(raised, !in_game);
	if (in_game) {
		if (raised) {
			SlatePanel *sp = Object::cast_to<SlatePanel>(panel);
			if (sp) {
				sp->focus_input();
			}
			if (auto_start && !is_session_running()) {
				start_session();
			}
		}
		emit_signal(SNAME("raised_changed"), raised);
	}
}

// --- input ------------------------------------------------------------------

void AgentSlate3D::input(const Ref<InputEvent> &p_event) {
	if (Engine::get_singleton()->is_editor_hint() || p_event.is_null()) {
		return;
	}
	bool has_toggle = toggle_action != StringName() && InputMap::get_singleton()->has_action(toggle_action);
	if (!raised) {
		if (has_toggle && p_event->is_action_pressed(toggle_action)) {
			set_raised(true);
			get_viewport()->set_input_as_handled();
		}
		return;
	}
	if (p_event->is_action_pressed(SNAME("ui_cancel"))) {
		set_raised(false);
		get_viewport()->set_input_as_handled();
		return;
	}
	// Keyboard goes to the slate while it's raised; mouse keeps going to the world.
	Ref<InputEventKey> key = p_event;
	if (key.is_valid() && viewport) {
		viewport->push_input(p_event, false);
		get_viewport()->set_input_as_handled();
	}
}

// --- lifecycle --------------------------------------------------------------

void AgentSlate3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			_ensure_panel();
			// The viewport texture is only valid once the viewport is in the tree.
			if (viewport && screen_material.is_valid()) {
				screen_material->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, viewport->get_texture());
			}
			set_position(raised ? raised_position : lowered_position);
			if (!Engine::get_singleton()->is_editor_hint()) {
				set_process_input(true);
				set_process_internal(true);
				SlatePanel *sp = Object::cast_to<SlatePanel>(panel);
				if (sp) {
					sp->bind_room(get_room());
				}
				if (auto_start && raised) {
					start_session();
				}
			}
		} break;
		case NOTIFICATION_INTERNAL_PROCESS: {
			if (session.is_valid()) {
				session->poll();
			}
		} break;
		case NOTIFICATION_EXIT_TREE: {
			if (session.is_valid() && session->is_running()) {
				session->stop();
			}
		} break;
	}
}

// --- properties -------------------------------------------------------------

void AgentSlate3D::set_panel_scene(const Ref<PackedScene> &p_scene) {
	panel_scene = p_scene;
	// Takes effect on the next instantiation; swapping a live panel is not supported yet.
}

void AgentSlate3D::set_screen_size_px(const Vector2i &p_size) {
	screen_size_px = Vector2i(MAX(64, p_size.x), MAX(64, p_size.y));
	_apply_screen_size();
}

void AgentSlate3D::set_screen_width_m(float p_width) {
	screen_width_m = MAX(0.01f, p_width);
	_apply_screen_size();
}

void AgentSlate3D::set_permission_mode(PermissionMode p_mode) {
	permission_mode = p_mode;
}

void AgentSlate3D::set_attach_bridge(bool p_attach) {
	attach_bridge = p_attach;
}

void AgentSlate3D::set_agent_name(const String &p_name) {
	agent_name = p_name.is_empty() ? String("slate") : p_name;
}

void AgentSlate3D::set_soul_path(const String &p_path) {
	soul_path = p_path;
}

void AgentSlate3D::set_auto_start(bool p_auto) {
	auto_start = p_auto;
}

void AgentSlate3D::set_resume_last_session(bool p_resume) {
	resume_last_session = p_resume;
}

void AgentSlate3D::set_toggle_action(const StringName &p_action) {
	toggle_action = p_action;
}

void AgentSlate3D::set_lowered_position(const Vector3 &p_pos) {
	lowered_position = p_pos;
	if (!raised && is_inside_tree()) {
		set_position(lowered_position);
	}
}

void AgentSlate3D::set_raised_position(const Vector3 &p_pos) {
	raised_position = p_pos;
	if (raised && is_inside_tree()) {
		set_position(raised_position);
	}
}

void AgentSlate3D::set_raise_time(float p_time) {
	raise_time = MAX(0.0f, p_time);
}

void AgentSlate3D::set_allowed_tools(const PackedStringArray &p_tools) {
	allowed_tools = p_tools;
}

void AgentSlate3D::set_model(const String &p_model) {
	model = p_model;
}

void AgentSlate3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("start_session"), &AgentSlate3D::start_session);
	ClassDB::bind_method(D_METHOD("stop_session"), &AgentSlate3D::stop_session);
	ClassDB::bind_method(D_METHOD("is_session_running"), &AgentSlate3D::is_session_running);
	ClassDB::bind_method(D_METHOD("get_session"), &AgentSlate3D::get_session);
	ClassDB::bind_method(D_METHOD("get_panel"), &AgentSlate3D::get_panel);
	ClassDB::bind_method(D_METHOD("get_cwd"), &AgentSlate3D::get_cwd);
	ClassDB::bind_method(D_METHOD("get_room"), &AgentSlate3D::get_room);

	ClassDB::bind_method(D_METHOD("set_panel_scene", "scene"), &AgentSlate3D::set_panel_scene);
	ClassDB::bind_method(D_METHOD("get_panel_scene"), &AgentSlate3D::get_panel_scene);
	ClassDB::bind_method(D_METHOD("set_screen_size_px", "size"), &AgentSlate3D::set_screen_size_px);
	ClassDB::bind_method(D_METHOD("get_screen_size_px"), &AgentSlate3D::get_screen_size_px);
	ClassDB::bind_method(D_METHOD("set_screen_width_m", "width"), &AgentSlate3D::set_screen_width_m);
	ClassDB::bind_method(D_METHOD("get_screen_width_m"), &AgentSlate3D::get_screen_width_m);
	ClassDB::bind_method(D_METHOD("set_raised", "raised"), &AgentSlate3D::set_raised);
	ClassDB::bind_method(D_METHOD("is_raised"), &AgentSlate3D::is_raised);
	ClassDB::bind_method(D_METHOD("set_permission_mode", "mode"), &AgentSlate3D::set_permission_mode);
	ClassDB::bind_method(D_METHOD("get_permission_mode"), &AgentSlate3D::get_permission_mode);
	ClassDB::bind_method(D_METHOD("set_attach_bridge", "attach"), &AgentSlate3D::set_attach_bridge);
	ClassDB::bind_method(D_METHOD("get_attach_bridge"), &AgentSlate3D::get_attach_bridge);
	ClassDB::bind_method(D_METHOD("set_agent_name", "name"), &AgentSlate3D::set_agent_name);
	ClassDB::bind_method(D_METHOD("get_agent_name"), &AgentSlate3D::get_agent_name);
	ClassDB::bind_method(D_METHOD("set_soul_path", "path"), &AgentSlate3D::set_soul_path);
	ClassDB::bind_method(D_METHOD("get_soul_path"), &AgentSlate3D::get_soul_path);
	ClassDB::bind_method(D_METHOD("set_auto_start", "auto_start"), &AgentSlate3D::set_auto_start);
	ClassDB::bind_method(D_METHOD("get_auto_start"), &AgentSlate3D::get_auto_start);
	ClassDB::bind_method(D_METHOD("set_resume_last_session", "resume"), &AgentSlate3D::set_resume_last_session);
	ClassDB::bind_method(D_METHOD("get_resume_last_session"), &AgentSlate3D::get_resume_last_session);
	ClassDB::bind_method(D_METHOD("set_toggle_action", "action"), &AgentSlate3D::set_toggle_action);
	ClassDB::bind_method(D_METHOD("get_toggle_action"), &AgentSlate3D::get_toggle_action);
	ClassDB::bind_method(D_METHOD("set_lowered_position", "position"), &AgentSlate3D::set_lowered_position);
	ClassDB::bind_method(D_METHOD("get_lowered_position"), &AgentSlate3D::get_lowered_position);
	ClassDB::bind_method(D_METHOD("set_raised_position", "position"), &AgentSlate3D::set_raised_position);
	ClassDB::bind_method(D_METHOD("get_raised_position"), &AgentSlate3D::get_raised_position);
	ClassDB::bind_method(D_METHOD("set_raise_time", "seconds"), &AgentSlate3D::set_raise_time);
	ClassDB::bind_method(D_METHOD("get_raise_time"), &AgentSlate3D::get_raise_time);
	ClassDB::bind_method(D_METHOD("set_allowed_tools", "tools"), &AgentSlate3D::set_allowed_tools);
	ClassDB::bind_method(D_METHOD("get_allowed_tools"), &AgentSlate3D::get_allowed_tools);
	ClassDB::bind_method(D_METHOD("set_model", "model"), &AgentSlate3D::set_model);
	ClassDB::bind_method(D_METHOD("get_model"), &AgentSlate3D::get_model);

	ADD_GROUP("Screen", "screen_");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "screen_size_px"), "set_screen_size_px", "get_screen_size_px");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "screen_width_m", PROPERTY_HINT_RANGE, "0.05,2,0.01,suffix:m"), "set_screen_width_m", "get_screen_width_m");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "panel_scene", PROPERTY_HINT_RESOURCE_TYPE, "PackedScene"), "set_panel_scene", "get_panel_scene");

	ADD_GROUP("Hold", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "raised"), "set_raised", "is_raised");
	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "toggle_action"), "set_toggle_action", "get_toggle_action");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "lowered_position"), "set_lowered_position", "get_lowered_position");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "raised_position"), "set_raised_position", "get_raised_position");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "raise_time", PROPERTY_HINT_RANGE, "0,2,0.01,suffix:s"), "set_raise_time", "get_raise_time");

	ADD_GROUP("Session", "");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "agent_name"), "set_agent_name", "get_agent_name");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "soul_path", PROPERTY_HINT_FILE, "*.md"), "set_soul_path", "get_soul_path");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "permission_mode", PROPERTY_HINT_ENUM, "Default,Accept Edits,Plan,Bypass Permissions,Don't Ask,Auto"), "set_permission_mode", "get_permission_mode");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "attach_bridge"), "set_attach_bridge", "get_attach_bridge");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_start"), "set_auto_start", "get_auto_start");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "resume_last_session"), "set_resume_last_session", "get_resume_last_session");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "allowed_tools"), "set_allowed_tools", "get_allowed_tools");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "model"), "set_model", "get_model");

	BIND_ENUM_CONSTANT(PERMISSION_DEFAULT);
	BIND_ENUM_CONSTANT(PERMISSION_ACCEPT_EDITS);
	BIND_ENUM_CONSTANT(PERMISSION_PLAN);
	BIND_ENUM_CONSTANT(PERMISSION_BYPASS);
	BIND_ENUM_CONSTANT(PERMISSION_DONT_ASK);
	BIND_ENUM_CONSTANT(PERMISSION_AUTO);

	ADD_SIGNAL(MethodInfo("raised_changed", PropertyInfo(Variant::BOOL, "raised")));
	ADD_SIGNAL(MethodInfo("session_started"));
	ADD_SIGNAL(MethodInfo("session_exited", PropertyInfo(Variant::INT, "code")));
	ADD_SIGNAL(MethodInfo("permission_requested", PropertyInfo(Variant::DICTIONARY, "request")));
}

AgentSlate3D::AgentSlate3D() {
	_build_children();
}
