/**************************************************************************/
/*  agent_body.h                                                          */
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

#pragma once

#include "soul.h"

#include "scene/3d/physics/character_body_3d.h"
#include "scene/resources/packed_scene.h"

class AnimationPlayer;
class Camera3D;
class CollisionShape3D;
class Label3D;
class NavigationAgent3D;
class SubViewport;

// A persistent body for an agent. Perception, motion, expression. Driven over the agent bridge
// (agent.* tools) or directly from script. See docs/opendust/04-embodiment.md.
class AgentBody : public CharacterBody3D {
	GDCLASS(AgentBody, CharacterBody3D);

public:
	enum Rig {
		RIG_AUTO, // from the soul's avatar_rig, else neutral
		RIG_NEUTRAL,
		RIG_MALE,
		RIG_FEMALE,
	};

private:
	Ref<Soul> soul;
	String agent_name;
	String agent_id;
	Ref<PackedScene> avatar_scene;
	Rig rig = RIG_AUTO;

	real_t move_speed = 3.0;
	real_t turn_speed = 6.0;
	real_t arrive_distance = 0.5;
	real_t perception_radius = 12.0;
	real_t gravity = 9.8;
	bool use_navigation = true;
	bool vision_enabled = false;
	Size2i vision_size = Size2i(256, 160);

	// Internal children (created in the constructor, INTERNAL_MODE_FRONT).
	Camera3D *eyes = nullptr;
	Label3D *bubble = nullptr;
	NavigationAgent3D *nav = nullptr;
	SubViewport *vision_viewport = nullptr;
	Camera3D *vision_camera = nullptr;
	Node3D *avatar_root = nullptr;
	CollisionShape3D *collision = nullptr;
	AnimationPlayer *anim = nullptr;

	// Motion state.
	bool moving = false;
	Vector3 move_target;
	bool has_look_target = false;
	Vector3 look_target;

	// Speech bubble timeout.
	double bubble_time_left = 0.0;

	// Held item (an object id so we never keep a dangling pointer).
	ObjectID held_item;

	void _build_avatar();
	void _ensure_collision(Rig p_rig);
	void _clear_avatar();
	Rig _effective_rig() const;
	void _physics_step(double p_delta);
	Node *_scene_root() const;
	Node *_find_room() const;
	Vector3 _resolve_target(const Variant &p_target, bool *r_ok) const;
	String _state_path() const;

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	// Identity.
	void set_soul(const Ref<Soul> &p_soul);
	Ref<Soul> get_soul() const { return soul; }
	void set_agent_name(const String &p_name) { agent_name = p_name; }
	String get_agent_name() const;
	void set_agent_id(const String &p_id) { agent_id = p_id; }
	String get_agent_id() const;
	bool is_identified() const { return !get_agent_id().is_empty(); }

	// Appearance.
	void set_avatar_scene(const Ref<PackedScene> &p_scene);
	Ref<PackedScene> get_avatar_scene() const { return avatar_scene; }
	void set_rig(Rig p_rig);
	Rig get_rig() const { return rig; }

	// Tunables.
	void set_move_speed(real_t p_speed) { move_speed = p_speed; }
	real_t get_move_speed() const { return move_speed; }
	void set_turn_speed(real_t p_speed) { turn_speed = p_speed; }
	real_t get_turn_speed() const { return turn_speed; }
	void set_arrive_distance(real_t p_d) { arrive_distance = p_d; }
	real_t get_arrive_distance() const { return arrive_distance; }
	void set_perception_radius(real_t p_r) { perception_radius = p_r; }
	real_t get_perception_radius() const { return perception_radius; }
	void set_use_navigation(bool p_use) { use_navigation = p_use; }
	bool get_use_navigation() const { return use_navigation; }
	void set_vision_enabled(bool p_enabled);
	bool is_vision_enabled() const { return vision_enabled; }
	void set_vision_size(const Size2i &p_size);
	Size2i get_vision_size() const { return vision_size; }

	Camera3D *get_eyes() const { return eyes; }

	// Motion. Targets may be a Vector3 or a NodePath/String naming a node.
	void move_to(const Variant &p_target);
	void look_at_target(const Variant &p_target);
	void stop();
	bool is_moving() const { return moving; }

	// Interaction.
	void interact(const Variant &p_target);
	void hold(const Variant &p_item);
	void drop();
	Node *get_held_item() const;

	// Expression.
	void say(const String &p_text, double p_seconds = 6.0);
	void emote(const String &p_name);

	// Perception.
	Dictionary perceive(bool p_include_frame = false, const Size2i &p_frame_size = Size2i(256, 160));
	String get_room_id() const;

	// Persistence: user://opendust/worlds/<world_id>/agents/<agent_id>.json
	Error save_state();
	Error restore_state();

	AgentBody();
	~AgentBody();
};

VARIANT_ENUM_CAST(AgentBody::Rig);
