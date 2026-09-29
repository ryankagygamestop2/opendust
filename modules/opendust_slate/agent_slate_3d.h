/**************************************************************************/
/*  agent_slate_3d.h                                                      */
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

#pragma once

#include "claude_session.h"

#include "scene/3d/node_3d.h"
#include "scene/animation/tween.h"
#include "scene/resources/material.h"
#include "scene/resources/packed_scene.h"

class MeshInstance3D;
class Room;
class SlatePanel;
class SubViewport;

// The held terminal object. A quad showing a SlatePanel rendered through a
// SubViewport, backed by one ClaudeSession whose cwd is the enclosing Room.
// See docs/opendust/02-slate.md.
class AgentSlate3D : public Node3D {
	GDCLASS(AgentSlate3D, Node3D);

public:
	enum PermissionMode {
		PERMISSION_DEFAULT,
		PERMISSION_ACCEPT_EDITS,
		PERMISSION_PLAN,
		PERMISSION_BYPASS,
		PERMISSION_DONT_ASK,
		PERMISSION_AUTO,
	};

private:
	// Configuration.
	Ref<PackedScene> panel_scene;
	Vector2i screen_size_px = Vector2i(1024, 768);
	float screen_width_m = 0.32f;
	bool raised = false;
	PermissionMode permission_mode = PERMISSION_DEFAULT;
	bool attach_bridge = true;
	String agent_name = "slate";
	String soul_path;
	bool auto_start = true;
	bool resume_last_session = true;
	StringName toggle_action = "slate_toggle";
	Vector3 lowered_position = Vector3(0.28, -0.42, -0.55);
	Vector3 raised_position = Vector3(0.0, -0.06, -0.36);
	float raise_time = 0.25f;
	PackedStringArray allowed_tools;
	String model;

	// Runtime.
	SubViewport *viewport = nullptr;
	Control *panel = nullptr; // SlatePanel or a custom panel_scene root
	MeshInstance3D *screen = nullptr;
	Ref<StandardMaterial3D> screen_material;
	Ref<ClaudeSession> session;
	Ref<Tween> raise_tween;
	bool warned_no_bridge = false;

	void _build_children();
	void _ensure_panel();
	void _apply_screen_size();
	void _animate(bool p_raised, bool p_instant);
	String _session_state_path(const String &p_cwd) const;
	String _load_last_session_id(const String &p_cwd) const;
	void _save_last_session_id(const String &p_cwd, const String &p_session_id) const;
	String _bridge_mcp_config_path() const;
	Dictionary _build_session_options(const String &p_cwd);
	static String _permission_mode_to_flag(PermissionMode p_mode);

	void _on_session_started(const String &p_session_id);
	void _on_session_exited(int p_code);
	void _on_panel_permission_requested(const Dictionary &p_request);

protected:
	static void _bind_methods();
	void _notification(int p_what);
	virtual void input(const Ref<InputEvent> &p_event) override;

public:
	// Session control.
	Error start_session();
	void stop_session();
	bool is_session_running() const;
	Ref<ClaudeSession> get_session() const { return session; }
	Control *get_panel() const { return panel; }
	String get_cwd() const;
	Room *get_room() const;

	// Properties.
	void set_panel_scene(const Ref<PackedScene> &p_scene);
	Ref<PackedScene> get_panel_scene() const { return panel_scene; }
	void set_screen_size_px(const Vector2i &p_size);
	Vector2i get_screen_size_px() const { return screen_size_px; }
	void set_screen_width_m(float p_width);
	float get_screen_width_m() const { return screen_width_m; }
	void set_raised(bool p_raised);
	bool is_raised() const { return raised; }
	void set_permission_mode(PermissionMode p_mode);
	PermissionMode get_permission_mode() const { return permission_mode; }
	void set_attach_bridge(bool p_attach);
	bool get_attach_bridge() const { return attach_bridge; }
	void set_agent_name(const String &p_name);
	String get_agent_name() const { return agent_name; }
	void set_soul_path(const String &p_path);
	String get_soul_path() const { return soul_path; }
	void set_auto_start(bool p_auto);
	bool get_auto_start() const { return auto_start; }
	void set_resume_last_session(bool p_resume);
	bool get_resume_last_session() const { return resume_last_session; }
	void set_toggle_action(const StringName &p_action);
	StringName get_toggle_action() const { return toggle_action; }
	void set_lowered_position(const Vector3 &p_pos);
	Vector3 get_lowered_position() const { return lowered_position; }
	void set_raised_position(const Vector3 &p_pos);
	Vector3 get_raised_position() const { return raised_position; }
	void set_raise_time(float p_time);
	float get_raise_time() const { return raise_time; }
	void set_allowed_tools(const PackedStringArray &p_tools);
	PackedStringArray get_allowed_tools() const { return allowed_tools; }
	void set_model(const String &p_model);
	String get_model() const { return model; }

	AgentSlate3D();
};

VARIANT_ENUM_CAST(AgentSlate3D::PermissionMode);
