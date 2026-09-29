/**************************************************************************/
/*  agent_body.cpp                                                        */
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

#include "agent_body.h"
#include "core/object/class_db.h"

#include "agent_tools.h"

#include "core/config/project_settings.h"
#include "core/crypto/crypto_core.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "scene/3d/camera_3d.h"
#include "scene/3d/label_3d.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/navigation/navigation_agent_3d.h"
#include "scene/3d/physics/collision_shape_3d.h"
#include "scene/animation/animation_player.h"
#include "scene/main/scene_tree.h"
#include "scene/main/viewport.h"
#include "scene/main/window.h"
#include "scene/resources/3d/capsule_shape_3d.h"
#include "scene/resources/3d/primitive_meshes.h"
#include "scene/resources/animation.h"
#include "scene/resources/animation_library.h"
#include "scene/resources/material.h"

AgentBody::AgentBody() {
	eyes = memnew(Camera3D);
	eyes->set_name("Eyes");
	eyes->set_current(false);
	eyes->set_position(Vector3(0, 1.6, 0));
	add_child(eyes, false, INTERNAL_MODE_FRONT);

	bubble = memnew(Label3D);
	bubble->set_name("Bubble");
	bubble->set_position(Vector3(0, 2.1, 0));
	bubble->set_billboard_mode(StandardMaterial3D::BILLBOARD_ENABLED);
	bubble->set_pixel_size(0.004);
	bubble->set_font_size(40);
	bubble->set_outline_size(8);
	bubble->set_visible(false);
	add_child(bubble, false, INTERNAL_MODE_FRONT);

	nav = memnew(NavigationAgent3D);
	nav->set_name("Nav");
	nav->set_target_desired_distance(arrive_distance);
	nav->set_path_desired_distance(arrive_distance);
	add_child(nav, false, INTERNAL_MODE_FRONT);

	set_physics_process(true);
}

AgentBody::~AgentBody() {
}

AgentBody::Rig AgentBody::_effective_rig() const {
	if (rig != RIG_AUTO) {
		return rig;
	}
	if (soul.is_valid()) {
		String r = soul->get_avatar_rig().to_lower();
		if (r == "male") {
			return RIG_MALE;
		}
		if (r == "female") {
			return RIG_FEMALE;
		}
	}
	return RIG_NEUTRAL;
}

void AgentBody::_clear_avatar() {
	if (avatar_root) {
		remove_child(avatar_root);
		memdelete(avatar_root);
		avatar_root = nullptr;
	}
	anim = nullptr;
}

void AgentBody::_build_avatar() {
	_clear_avatar();

	if (avatar_scene.is_valid()) {
		Node *inst = avatar_scene->instantiate();
		Node3D *root3d = Object::cast_to<Node3D>(inst);
		if (!root3d) {
			// A non-3D avatar scene is a mistake; wrap it so it still has a place in the tree.
			root3d = memnew(Node3D);
			root3d->add_child(inst);
		}
		root3d->set_name("Avatar");
		avatar_root = root3d;
		add_child(avatar_root, false, INTERNAL_MODE_BACK);
		anim = Object::cast_to<AnimationPlayer>(avatar_root->find_child("AnimationPlayer", true, false));
		return;
	}

	// Procedural placeholder: capsule body + sphere head + collision + two tiny animations.
	Rig r = _effective_rig();
	float height = r == RIG_MALE ? 1.8f : (r == RIG_FEMALE ? 1.65f : 1.72f);
	float radius = r == RIG_MALE ? 0.32f : (r == RIG_FEMALE ? 0.28f : 0.30f);

	avatar_root = memnew(Node3D);
	avatar_root->set_name("Avatar");
	add_child(avatar_root, false, INTERNAL_MODE_BACK);

	Ref<CapsuleMesh> capsule;
	capsule.instantiate();
	capsule->set_radius(radius);
	capsule->set_height(height - 0.3f);
	MeshInstance3D *body = memnew(MeshInstance3D);
	body->set_name("Body");
	body->set_mesh(capsule);
	body->set_position(Vector3(0, (height - 0.3f) * 0.5f, 0));
	avatar_root->add_child(body);

	Ref<SphereMesh> sphere;
	sphere.instantiate();
	sphere->set_radius(0.16f);
	sphere->set_height(0.32f);
	MeshInstance3D *head = memnew(MeshInstance3D);
	head->set_name("Head");
	head->set_mesh(sphere);
	head->set_position(Vector3(0, height - 0.1f, 0));
	avatar_root->add_child(head);

	Ref<CapsuleShape3D> shape;
	shape.instantiate();
	shape->set_radius(radius);
	shape->set_height(height);
	CollisionShape3D *col = memnew(CollisionShape3D);
	col->set_name("Collision");
	col->set_shape(shape);
	col->set_position(Vector3(0, height * 0.5f, 0));
	avatar_root->add_child(col);

	// Eyes sit at head height for whatever rig this is.
	eyes->set_position(Vector3(0, height - 0.1f, 0));
	bubble->set_position(Vector3(0, height + 0.35f, 0));

	anim = memnew(AnimationPlayer);
	anim->set_name("AnimationPlayer");
	avatar_root->add_child(anim);

	Ref<AnimationLibrary> lib;
	lib.instantiate();

	{
		Ref<Animation> idle;
		idle.instantiate();
		idle->set_length(2.0);
		idle->set_loop_mode(Animation::LOOP_LINEAR);
		int t = idle->add_track(Animation::TYPE_POSITION_3D);
		idle->track_set_path(t, NodePath("Head"));
		idle->position_track_insert_key(t, 0.0, Vector3(0, height - 0.1f, 0));
		idle->position_track_insert_key(t, 1.0, Vector3(0, height - 0.08f, 0));
		idle->position_track_insert_key(t, 2.0, Vector3(0, height - 0.1f, 0));
		lib->add_animation("idle", idle);
	}
	{
		Ref<Animation> wave;
		wave.instantiate();
		wave->set_length(1.2);
		int t = wave->add_track(Animation::TYPE_ROTATION_3D);
		wave->track_set_path(t, NodePath("Body"));
		wave->rotation_track_insert_key(t, 0.0, Quaternion());
		wave->rotation_track_insert_key(t, 0.3, Quaternion(Vector3(0, 0, 1), 0.25));
		wave->rotation_track_insert_key(t, 0.6, Quaternion(Vector3(0, 0, 1), -0.25));
		wave->rotation_track_insert_key(t, 0.9, Quaternion(Vector3(0, 0, 1), 0.25));
		wave->rotation_track_insert_key(t, 1.2, Quaternion());
		lib->add_animation("wave", wave);
	}
	anim->add_animation_library("", lib);
	anim->set_autoplay("idle");
}

void AgentBody::set_soul(const Ref<Soul> &p_soul) {
	soul = p_soul;
	if (is_inside_tree() && rig == RIG_AUTO && avatar_scene.is_null()) {
		_build_avatar();
	}
}

String AgentBody::get_agent_name() const {
	if (!agent_name.is_empty()) {
		return agent_name;
	}
	if (soul.is_valid()) {
		return soul->get_display_name();
	}
	return String(get_name());
}

String AgentBody::get_agent_id() const {
	if (!agent_id.is_empty()) {
		return agent_id;
	}
	if (soul.is_valid()) {
		return soul->get_agent_id();
	}
	return String();
}

void AgentBody::set_avatar_scene(const Ref<PackedScene> &p_scene) {
	avatar_scene = p_scene;
	if (is_inside_tree()) {
		_build_avatar();
	}
}

void AgentBody::set_rig(Rig p_rig) {
	rig = p_rig;
	if (is_inside_tree() && avatar_scene.is_null()) {
		_build_avatar();
	}
}

void AgentBody::set_vision_enabled(bool p_enabled) {
	vision_enabled = p_enabled;
	if (p_enabled && !vision_viewport) {
		vision_viewport = memnew(SubViewport);
		vision_viewport->set_name("Vision");
		vision_viewport->set_size(vision_size);
		vision_viewport->set_update_mode(SubViewport::UPDATE_ALWAYS);
		add_child(vision_viewport, false, INTERNAL_MODE_FRONT);
		vision_camera = memnew(Camera3D);
		vision_camera->set_name("VisionCamera");
		vision_viewport->add_child(vision_camera);
		vision_camera->set_current(true);
	}
	if (vision_viewport) {
		vision_viewport->set_update_mode(p_enabled ? SubViewport::UPDATE_ALWAYS : SubViewport::UPDATE_DISABLED);
	}
}

void AgentBody::set_vision_size(const Size2i &p_size) {
	vision_size = p_size;
	if (vision_viewport) {
		vision_viewport->set_size(vision_size);
	}
}

Node *AgentBody::_scene_root() const {
	if (!is_inside_tree()) {
		return nullptr;
	}
	Node *cur = get_tree()->get_current_scene();
	if (cur) {
		return cur;
	}
	return get_tree()->get_root();
}

Node *AgentBody::_find_room() const {
	Node *n = get_parent();
	while (n) {
		if (n->is_class("Room") || n->has_meta("room_id")) {
			return n;
		}
		n = n->get_parent();
	}
	return nullptr;
}

String AgentBody::get_room_id() const {
	Node *room = _find_room();
	if (!room) {
		return String();
	}
	if (room->has_meta("room_id")) {
		return room->get_meta("room_id");
	}
	bool valid = false;
	Variant v = room->get("room_id", &valid);
	return valid ? String(v) : String(room->get_name());
}

Vector3 AgentBody::_resolve_target(const Variant &p_target, bool *r_ok) const {
	if (r_ok) {
		*r_ok = true;
	}
	switch (p_target.get_type()) {
		case Variant::VECTOR3:
			return p_target;
		case Variant::ARRAY:
		case Variant::PACKED_FLOAT32_ARRAY:
		case Variant::PACKED_FLOAT64_ARRAY: {
			Array a = p_target;
			if (a.size() >= 3) {
				return Vector3(a[0], a[1], a[2]);
			}
		} break;
		case Variant::NODE_PATH:
		case Variant::STRING: {
			NodePath np = p_target.get_type() == Variant::NODE_PATH ? NodePath(p_target) : NodePath(String(p_target));
			Node *n = nullptr;
			if (is_inside_tree()) {
				n = get_node_or_null(np);
				if (!n) {
					n = get_tree()->get_root()->get_node_or_null(np);
				}
				if (!n && _scene_root()) {
					n = _scene_root()->get_node_or_null(np);
				}
			}
			Node3D *n3 = Object::cast_to<Node3D>(n);
			if (n3) {
				return n3->get_global_position();
			}
		} break;
		case Variant::OBJECT: {
			Node3D *n3 = Object::cast_to<Node3D>(p_target);
			if (n3) {
				return n3->get_global_position();
			}
		} break;
		default:
			break;
	}
	if (r_ok) {
		*r_ok = false;
	}
	return get_global_position();
}

void AgentBody::move_to(const Variant &p_target) {
	bool ok = false;
	Vector3 t = _resolve_target(p_target, &ok);
	ERR_FAIL_COND_MSG(!ok, "AgentBody.move_to: target must be a Vector3, [x,y,z], NodePath, or Node3D.");
	move_target = t;
	moving = true;
	if (nav && use_navigation) {
		nav->set_target_position(t);
	}
}

void AgentBody::look_at_target(const Variant &p_target) {
	bool ok = false;
	Vector3 t = _resolve_target(p_target, &ok);
	ERR_FAIL_COND_MSG(!ok, "AgentBody.look_at_target: bad target.");
	look_target = t;
	has_look_target = true;
}

void AgentBody::stop() {
	moving = false;
	set_velocity(Vector3(0, get_velocity().y, 0));
}

void AgentBody::_physics_step(double p_delta) {
	Vector3 vel = get_velocity();
	if (!is_on_floor()) {
		vel.y -= gravity * p_delta;
	} else if (vel.y < 0) {
		vel.y = 0;
	}

	Vector3 pos = get_global_position();
	Vector3 horizontal_goal;
	bool have_goal = false;

	if (moving) {
		Vector3 flat_target = move_target;
		flat_target.y = pos.y;
		if (pos.distance_to(flat_target) <= arrive_distance) {
			moving = false;
			emit_signal(SNAME("arrived"));
		} else {
			Vector3 next = move_target;
			if (nav && use_navigation && nav->is_target_reachable()) {
				next = nav->get_next_path_position();
			}
			next.y = pos.y;
			horizontal_goal = next;
			have_goal = true;
		}
	}

	if (have_goal) {
		Vector3 dir = (horizontal_goal - pos);
		dir.y = 0;
		if (dir.length_squared() > 0.0001) {
			dir.normalize();
			vel.x = dir.x * move_speed;
			vel.z = dir.z * move_speed;
			if (!has_look_target) {
				look_target = pos + dir;
			}
		}
	} else {
		vel.x = 0;
		vel.z = 0;
	}

	// Turn toward look target (yaw only).
	if (has_look_target || have_goal) {
		Vector3 to = look_target - pos;
		to.y = 0;
		if (to.length_squared() > 0.0001) {
			real_t want_yaw = Math::atan2(-to.x, -to.z);
			Vector3 rot = get_rotation();
			real_t diff = (real_t)Math::wrapf((double)(want_yaw - rot.y), -Math::PI, Math::PI);
			real_t step = turn_speed * p_delta;
			rot.y += CLAMP(diff, -step, step);
			set_rotation(rot);
		}
		if (!have_goal && has_look_target) {
			// Keep looking until told otherwise; clear once aligned.
			Vector3 fwd = -get_global_basis().get_column(2);
			fwd.y = 0;
			if (fwd.normalized().dot(to.normalized()) > 0.999) {
				has_look_target = false;
			}
		}
	}

	set_velocity(vel);
	move_and_slide();

	if (vision_viewport && vision_camera && vision_enabled) {
		vision_camera->set_global_transform(eyes->get_global_transform());
	}

	if (bubble_time_left > 0.0) {
		bubble_time_left -= p_delta;
		if (bubble_time_left <= 0.0) {
			bubble->set_visible(false);
		}
	}
}

void AgentBody::interact(const Variant &p_target) {
	Node *target = nullptr;
	if (p_target.get_type() == Variant::OBJECT) {
		target = Object::cast_to<Node>(p_target);
	} else if (is_inside_tree()) {
		NodePath np = p_target.get_type() == Variant::NODE_PATH ? NodePath(p_target) : NodePath(String(p_target));
		target = get_node_or_null(np);
		if (!target) {
			target = get_tree()->get_root()->get_node_or_null(np);
		}
		if (!target && _scene_root()) {
			target = _scene_root()->get_node_or_null(np);
		}
	}
	ERR_FAIL_NULL_MSG(target, "AgentBody.interact: target not found.");
	if (target->has_method("_on_agent_interact")) {
		target->call("_on_agent_interact", this);
	}
	emit_signal(SNAME("interacted"), target->get_path());
}

void AgentBody::hold(const Variant &p_item) {
	Node3D *item = nullptr;
	if (p_item.get_type() == Variant::OBJECT) {
		item = Object::cast_to<Node3D>(p_item);
	} else if (is_inside_tree()) {
		NodePath np = p_item.get_type() == Variant::NODE_PATH ? NodePath(p_item) : NodePath(String(p_item));
		Node *n = get_node_or_null(np);
		if (!n) {
			n = get_tree()->get_root()->get_node_or_null(np);
		}
		if (!n && _scene_root()) {
			n = _scene_root()->get_node_or_null(np);
		}
		item = Object::cast_to<Node3D>(n);
	}
	ERR_FAIL_NULL_MSG(item, "AgentBody.hold: item not found or not a Node3D.");
	drop();
	Transform3D keep = item->get_global_transform();
	Node *old_parent = item->get_parent();
	if (old_parent) {
		old_parent->remove_child(item);
	}
	add_child(item);
	item->set_global_transform(keep);
	// Snap to a hand-ish spot in front of the body.
	item->set_position(Vector3(0.35, 1.1, -0.4));
	held_item = item->get_instance_id();
	emit_signal(SNAME("held"), item->get_path());
}

void AgentBody::drop() {
	Node *item = get_held_item();
	if (!item) {
		held_item = ObjectID();
		return;
	}
	Node3D *item3d = Object::cast_to<Node3D>(item);
	Transform3D keep = item3d ? item3d->get_global_transform() : Transform3D();
	remove_child(item);
	Node *root = _scene_root();
	if (root) {
		root->add_child(item);
		if (item3d) {
			item3d->set_global_transform(keep);
		}
	} else {
		memdelete(item);
	}
	held_item = ObjectID();
	emit_signal(SNAME("dropped"));
}

Node *AgentBody::get_held_item() const {
	if (held_item.is_null()) {
		return nullptr;
	}
	return Object::cast_to<Node>(ObjectDB::get_instance(held_item));
}

void AgentBody::say(const String &p_text, double p_seconds) {
	bubble->set_text(p_text);
	bubble->set_visible(true);
	bubble_time_left = p_seconds;
	emit_signal(SNAME("said"), p_text);
}

void AgentBody::emote(const String &p_name) {
	if (anim && anim->has_animation(p_name)) {
		anim->play(p_name);
	}
	emit_signal(SNAME("emoted"), p_name);
}

Dictionary AgentBody::perceive(bool p_include_frame, const Size2i &p_frame_size) {
	Dictionary out;
	out["agent_name"] = get_agent_name();
	out["agent_id"] = get_agent_id();
	out["room"] = get_room_id();
	if (!is_inside_tree()) {
		out["nearby"] = Array();
		return out;
	}

	Vector3 pos = get_global_position();
	Vector3 facing = -get_global_basis().get_column(2);
	Array pv;
	pv.push_back(pos.x);
	pv.push_back(pos.y);
	pv.push_back(pos.z);
	out["position"] = pv;
	Array fv;
	fv.push_back(facing.x);
	fv.push_back(facing.y);
	fv.push_back(facing.z);
	out["facing"] = fv;

	Array nearby;
	String looking_at;
	real_t best_dot = 0.94;
	Node *root = _scene_root();
	Vector3 eye_pos = eyes->get_global_position();
	Vector3 eye_fwd = -eyes->get_global_basis().get_column(2);

	if (root) {
		List<Node *> stack;
		stack.push_back(root);
		while (!stack.is_empty()) {
			Node *n = stack.front()->get();
			stack.pop_front();
			for (int i = 0; i < n->get_child_count(false); i++) {
				stack.push_back(n->get_child(i, false));
			}
			// Skip ourselves and our own internals (avatar, eyes, held item).
			if (n == this || is_ancestor_of(n)) {
				continue;
			}
			Node3D *n3 = Object::cast_to<Node3D>(n);
			if (!n3 || n3 == avatar_root) {
				continue;
			}
			Vector3 p = n3->get_global_position();
			real_t d = p.distance_to(pos);
			if (d > perception_radius) {
				continue;
			}
			bool in_view = eyes->is_position_in_frustum(p);
			Dictionary e;
			e["path"] = String(root->get_path_to(n3));
			e["type"] = n3->get_class();
			e["name"] = String(n3->get_name());
			e["distance"] = d;
			e["in_view"] = in_view;
			if (n3->is_class("AgentBody")) {
				e["agent_name"] = n3->get("agent_name");
			}
			nearby.push_back(e);
			if (in_view && d > 0.05) {
				Vector3 to = (p - eye_pos).normalized();
				real_t dt = to.dot(eye_fwd);
				if (dt > best_dot) {
					best_dot = dt;
					looking_at = String(root->get_path_to(n3));
				}
			}
		}
	}
	out["nearby"] = nearby;
	if (!looking_at.is_empty()) {
		out["looking_at"] = looking_at;
	}
	Node *held = get_held_item();
	if (held && root) {
		out["held"] = String(root->get_path_to(held));
	}

	if (p_include_frame) {
		if (!vision_enabled || vision_size != p_frame_size) {
			set_vision_size(p_frame_size);
			set_vision_enabled(true);
			out["frame_pending"] = true; // first frame renders next tick
		} else if (vision_viewport) {
			Ref<Image> img = vision_viewport->get_texture()->get_image();
			if (img.is_valid() && !img->is_empty()) {
				Vector<uint8_t> png = img->save_png_to_buffer();
				out["frame_png_base64"] = CryptoCore::b64_encode_str(png.ptr(), png.size());
				out["frame_width"] = img->get_width();
				out["frame_height"] = img->get_height();
			} else {
				out["frame_pending"] = true;
			}
		}
	}

	emit_signal(SNAME("perception"), out);
	return out;
}

String AgentBody::_state_path() const {
	String world_id = GLOBAL_GET("opendust/world_id");
	if (world_id.is_empty()) {
		world_id = "default";
	}
	String id = get_agent_id();
	if (id.is_empty()) {
		id = "unidentified-" + get_agent_name().to_lower().replace(" ", "-");
	}
	return "user://opendust/worlds/" + world_id + "/agents/" + id + ".json";
}

Error AgentBody::save_state() {
	String path = _state_path();
	Ref<DirAccess> da = DirAccess::open("user://");
	ERR_FAIL_COND_V(da.is_null(), ERR_CANT_OPEN);
	Error err = da->make_dir_recursive(path.get_base_dir().trim_prefix("user://"));
	ERR_FAIL_COND_V(err != OK && err != ERR_ALREADY_EXISTS, err);

	Dictionary st;
	st["schema"] = "opendust.agent_state/1";
	st["agent_id"] = get_agent_id();
	st["agent_name"] = get_agent_name();
	st["room_id"] = get_room_id();
	Transform3D t = get_global_transform();
	Array origin;
	origin.push_back(t.origin.x);
	origin.push_back(t.origin.y);
	origin.push_back(t.origin.z);
	st["origin"] = origin;
	Vector3 rot = get_rotation();
	Array rota;
	rota.push_back(rot.x);
	rota.push_back(rot.y);
	rota.push_back(rot.z);
	st["rotation"] = rota;
	Node *held = get_held_item();
	Node *root = _scene_root();
	st["held"] = (held && root) ? String(root->get_path_to(held)) : String();
	if (soul.is_valid()) {
		st["soul_ref"] = soul->get_source_path();
	}

	Ref<FileAccess> f = FileAccess::open(path, FileAccess::WRITE, &err);
	ERR_FAIL_COND_V(f.is_null(), err);
	f->store_string(JSON::stringify(st, "  "));
	return OK;
}

Error AgentBody::restore_state() {
	String path = _state_path();
	if (!FileAccess::exists(path)) {
		return ERR_DOES_NOT_EXIST;
	}
	Error err = OK;
	String text = FileAccess::get_file_as_string(path, &err);
	ERR_FAIL_COND_V(err != OK, err);
	Variant parsed = JSON::parse_string(text);
	ERR_FAIL_COND_V(parsed.get_type() != Variant::DICTIONARY, ERR_FILE_CORRUPT);
	Dictionary st = parsed;
	if (st.has("origin")) {
		Array o = st["origin"];
		if (o.size() >= 3) {
			set_global_position(Vector3(o[0], o[1], o[2]));
		}
	}
	if (st.has("rotation")) {
		Array r = st["rotation"];
		if (r.size() >= 3) {
			set_rotation(Vector3(r[0], r[1], r[2]));
		}
	}
	if (st.has("held") && !String(st["held"]).is_empty()) {
		hold(String(st["held"]));
	}
	return OK;
}

void AgentBody::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			AgentBodyRegistry::add(this);
			AgentTools::ensure_registered();
		} break;
		case NOTIFICATION_READY: {
			if (!avatar_root) {
				_build_avatar();
			}
		} break;
		case NOTIFICATION_EXIT_TREE: {
			AgentBodyRegistry::remove(this);
		} break;
		case NOTIFICATION_PHYSICS_PROCESS: {
			_physics_step(get_physics_process_delta_time());
		} break;
	}
}

void AgentBody::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_soul", "soul"), &AgentBody::set_soul);
	ClassDB::bind_method(D_METHOD("get_soul"), &AgentBody::get_soul);
	ClassDB::bind_method(D_METHOD("set_agent_name", "name"), &AgentBody::set_agent_name);
	ClassDB::bind_method(D_METHOD("get_agent_name"), &AgentBody::get_agent_name);
	ClassDB::bind_method(D_METHOD("set_agent_id", "agent_id"), &AgentBody::set_agent_id);
	ClassDB::bind_method(D_METHOD("get_agent_id"), &AgentBody::get_agent_id);
	ClassDB::bind_method(D_METHOD("is_identified"), &AgentBody::is_identified);
	ClassDB::bind_method(D_METHOD("set_avatar_scene", "scene"), &AgentBody::set_avatar_scene);
	ClassDB::bind_method(D_METHOD("get_avatar_scene"), &AgentBody::get_avatar_scene);
	ClassDB::bind_method(D_METHOD("set_rig", "rig"), &AgentBody::set_rig);
	ClassDB::bind_method(D_METHOD("get_rig"), &AgentBody::get_rig);
	ClassDB::bind_method(D_METHOD("set_move_speed", "speed"), &AgentBody::set_move_speed);
	ClassDB::bind_method(D_METHOD("get_move_speed"), &AgentBody::get_move_speed);
	ClassDB::bind_method(D_METHOD("set_turn_speed", "speed"), &AgentBody::set_turn_speed);
	ClassDB::bind_method(D_METHOD("get_turn_speed"), &AgentBody::get_turn_speed);
	ClassDB::bind_method(D_METHOD("set_arrive_distance", "distance"), &AgentBody::set_arrive_distance);
	ClassDB::bind_method(D_METHOD("get_arrive_distance"), &AgentBody::get_arrive_distance);
	ClassDB::bind_method(D_METHOD("set_perception_radius", "radius"), &AgentBody::set_perception_radius);
	ClassDB::bind_method(D_METHOD("get_perception_radius"), &AgentBody::get_perception_radius);
	ClassDB::bind_method(D_METHOD("set_use_navigation", "use"), &AgentBody::set_use_navigation);
	ClassDB::bind_method(D_METHOD("get_use_navigation"), &AgentBody::get_use_navigation);
	ClassDB::bind_method(D_METHOD("set_vision_enabled", "enabled"), &AgentBody::set_vision_enabled);
	ClassDB::bind_method(D_METHOD("is_vision_enabled"), &AgentBody::is_vision_enabled);
	ClassDB::bind_method(D_METHOD("set_vision_size", "size"), &AgentBody::set_vision_size);
	ClassDB::bind_method(D_METHOD("get_vision_size"), &AgentBody::get_vision_size);
	ClassDB::bind_method(D_METHOD("get_eyes"), &AgentBody::get_eyes);

	ClassDB::bind_method(D_METHOD("move_to", "target"), &AgentBody::move_to);
	ClassDB::bind_method(D_METHOD("look_at_target", "target"), &AgentBody::look_at_target);
	ClassDB::bind_method(D_METHOD("stop"), &AgentBody::stop);
	ClassDB::bind_method(D_METHOD("is_moving"), &AgentBody::is_moving);
	ClassDB::bind_method(D_METHOD("interact", "target"), &AgentBody::interact);
	ClassDB::bind_method(D_METHOD("hold", "item"), &AgentBody::hold);
	ClassDB::bind_method(D_METHOD("drop"), &AgentBody::drop);
	ClassDB::bind_method(D_METHOD("get_held_item"), &AgentBody::get_held_item);
	ClassDB::bind_method(D_METHOD("say", "text", "seconds"), &AgentBody::say, DEFVAL(6.0));
	ClassDB::bind_method(D_METHOD("emote", "name"), &AgentBody::emote);
	ClassDB::bind_method(D_METHOD("perceive", "include_frame", "frame_size"), &AgentBody::perceive, DEFVAL(false), DEFVAL(Size2i(256, 160)));
	ClassDB::bind_method(D_METHOD("get_room_id"), &AgentBody::get_room_id);
	ClassDB::bind_method(D_METHOD("save_state"), &AgentBody::save_state);
	ClassDB::bind_method(D_METHOD("restore_state"), &AgentBody::restore_state);

	ADD_GROUP("Identity", "");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "soul", PROPERTY_HINT_RESOURCE_TYPE, "Soul"), "set_soul", "get_soul");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "agent_name"), "set_agent_name", "get_agent_name");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "agent_id"), "set_agent_id", "get_agent_id");
	ADD_GROUP("Avatar", "");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "avatar_scene", PROPERTY_HINT_RESOURCE_TYPE, "PackedScene"), "set_avatar_scene", "get_avatar_scene");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "rig", PROPERTY_HINT_ENUM, "Auto,Neutral,Male,Female"), "set_rig", "get_rig");
	ADD_GROUP("Motion", "");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "move_speed", PROPERTY_HINT_RANGE, "0,20,0.1"), "set_move_speed", "get_move_speed");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "turn_speed", PROPERTY_HINT_RANGE, "0,20,0.1"), "set_turn_speed", "get_turn_speed");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "arrive_distance", PROPERTY_HINT_RANGE, "0.05,5,0.05"), "set_arrive_distance", "get_arrive_distance");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_navigation"), "set_use_navigation", "get_use_navigation");
	ADD_GROUP("Perception", "");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "perception_radius", PROPERTY_HINT_RANGE, "1,100,0.5"), "set_perception_radius", "get_perception_radius");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "vision_enabled"), "set_vision_enabled", "is_vision_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "vision_size"), "set_vision_size", "get_vision_size");

	ADD_SIGNAL(MethodInfo("said", PropertyInfo(Variant::STRING, "text")));
	ADD_SIGNAL(MethodInfo("emoted", PropertyInfo(Variant::STRING, "name")));
	ADD_SIGNAL(MethodInfo("interacted", PropertyInfo(Variant::NODE_PATH, "target")));
	ADD_SIGNAL(MethodInfo("held", PropertyInfo(Variant::NODE_PATH, "item")));
	ADD_SIGNAL(MethodInfo("dropped"));
	ADD_SIGNAL(MethodInfo("arrived"));
	ADD_SIGNAL(MethodInfo("perception", PropertyInfo(Variant::DICTIONARY, "data")));

	BIND_ENUM_CONSTANT(RIG_AUTO);
	BIND_ENUM_CONSTANT(RIG_NEUTRAL);
	BIND_ENUM_CONSTANT(RIG_MALE);
	BIND_ENUM_CONSTANT(RIG_FEMALE);
}
