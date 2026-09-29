/**************************************************************************/
/*  opendust_editor_tools.cpp                                             */
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

#include "opendust_editor_tools.h"

#include "../opendust_json.h"
#include "../opendust_node_utils.h"
#include "../opendust_tool_registry.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/object/undo_redo.h"
#include "editor/editor_data.h"
#include "editor/editor_interface.h"
#include "editor/editor_main_screen.h"
#include "editor/editor_node.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/file_system/editor_file_system.h"
#include "editor/run/editor_run_bar.h"
#include "editor/settings/editor_command_palette.h"
#include "scene/main/node.h"
#include "scene/main/viewport.h"
#include "scene/main/window.h"
#include "scene/resources/packed_scene.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

Node *OpenDustEditorTools::_scene_root(Dictionary &r_error) const {
	Node *root = EditorInterface::get_singleton()->get_edited_scene_root();
	if (!root) {
		r_error = _err(OpenDustToolRegistry::RPC_NOT_FOUND, "No scene is open in the editor.");
	}
	return root;
}

Node *OpenDustEditorTools::_resolve(const String &p_path, Dictionary &r_error) const {
	Node *root = _scene_root(r_error);
	if (!root) {
		return nullptr;
	}
	Node *n = OpenDustNodeUtils::resolve(root, p_path);
	if (!n) {
		r_error = _err(OpenDustToolRegistry::RPC_NOT_FOUND, vformat("Node '%s' not found in the edited scene.", p_path));
	}
	return n;
}

String OpenDustEditorTools::_agent(const Dictionary &p_context) const {
	String a = p_context.get("agent_name", "agent");
	return a.is_empty() ? String("agent") : a;
}

String OpenDustEditorTools::_action(const Dictionary &p_context, const String &p_verb) const {
	return _agent(p_context) + ": " + p_verb;
}

EditorUndoRedoManager *OpenDustEditorTools::_ur() const {
	return EditorUndoRedoManager::get_singleton();
}

Dictionary OpenDustEditorTools::_ok() const {
	Dictionary d;
	d["ok"] = true;
	return d;
}

Dictionary OpenDustEditorTools::_err(int p_code, const String &p_message) const {
	return OpenDustToolRegistry::make_error(p_code, p_message);
}

Dictionary OpenDustEditorTools::_path_result(Node *p_node) const {
	Dictionary d;
	Node *root = EditorInterface::get_singleton()->get_edited_scene_root();
	d["path"] = OpenDustNodeUtils::relative_path(root, p_node);
	d["name"] = p_node ? String(p_node->get_name()) : String();
	return d;
}

void OpenDustEditorTools::_reg(const String &p_name, const String &p_desc, const Dictionary &p_props, const Array &p_required, Dictionary (OpenDustEditorTools::*p_method)(const Dictionary &, const Dictionary &), bool p_mutates, bool p_slow) {
	OpenDustToolRegistry *reg = OpenDustToolRegistry::get_singleton();
	ERR_FAIL_NULL(reg);
	int flags = OpenDustToolRegistry::TOOL_EDITOR;
	if (p_mutates) {
		flags |= OpenDustToolRegistry::TOOL_MUTATES;
	}
	if (p_slow) {
		flags |= OpenDustToolRegistry::TOOL_SLOW;
	}
	reg->register_tool(p_name, p_desc, OpenDustToolRegistry::object_schema(p_props, p_required), callable_mp(this, p_method), flags);
	registered_names.push_back(p_name);
}

#define P(m_name, m_type, m_desc) props[m_name] = OpenDustToolRegistry::prop(m_type, m_desc)
#define REQ(...)                                             \
	{                                                        \
		const char *_r[] = { __VA_ARGS__ };                  \
		required = Array();                                  \
		for (size_t _i = 0; _i < sizeof(_r) / sizeof(_r[0]); _i++) { \
			required.push_back(_r[_i]);                      \
		}                                                    \
	}

// ---------------------------------------------------------------------------
// project.*
// ---------------------------------------------------------------------------

Dictionary OpenDustEditorTools::_project_settings_get(const Dictionary &p_params, const Dictionary &p_context) {
	String name = p_params.get("name", "");
	if (name.is_empty()) {
		return _err(OpenDustToolRegistry::RPC_INVALID_PARAMS, "name is required.");
	}
	if (!ProjectSettings::get_singleton()->has_setting(name)) {
		return _err(OpenDustToolRegistry::RPC_NOT_FOUND, vformat("No project setting '%s'.", name));
	}
	Dictionary d;
	d["name"] = name;
	d["value"] = OpenDustJSON::to_json(ProjectSettings::get_singleton()->get_setting(name));
	return d;
}

Dictionary OpenDustEditorTools::_project_settings_set(const Dictionary &p_params, const Dictionary &p_context) {
	String name = p_params.get("name", "");
	if (name.is_empty()) {
		return _err(OpenDustToolRegistry::RPC_INVALID_PARAMS, "name is required.");
	}
	Variant value = OpenDustJSON::from_json(p_params.get("value", Variant()));
	if (ProjectSettings::get_singleton()->has_setting(name)) {
		Variant cur = ProjectSettings::get_singleton()->get_setting(name);
		value = OpenDustJSON::coerce(value, cur.get_type());
	}
	ProjectSettings::get_singleton()->set_setting(name, value);
	Error err = ProjectSettings::get_singleton()->save();
	if (err != OK) {
		return _err(OpenDustToolRegistry::RPC_INTERNAL_ERROR, vformat("Could not save project settings (error %d).", (int)err));
	}
	return _ok();
}

static void _list_dir(const String &p_dir, bool p_recursive, const String &p_glob, Array &r_out, int p_depth) {
	if (p_depth > 64) {
		return;
	}
	Ref<DirAccess> da = DirAccess::open(p_dir);
	if (da.is_null()) {
		return;
	}
	da->list_dir_begin();
	String name = da->get_next();
	while (!name.is_empty()) {
		if (name == "." || name == ".." || name.begins_with(".")) {
			name = da->get_next();
			continue;
		}
		String full = p_dir.path_join(name);
		bool is_dir = da->current_is_dir();
		if (p_glob.is_empty() || name.match(p_glob) || is_dir) {
			if (!(is_dir && !p_glob.is_empty() && !p_recursive)) {
				Dictionary e;
				e["path"] = full;
				e["is_dir"] = is_dir;
				if (!is_dir) {
					Ref<FileAccess> f = FileAccess::open(full, FileAccess::READ);
					e["size"] = f.is_valid() ? (int64_t)f->get_length() : (int64_t)0;
				}
				if (!(is_dir && !p_glob.is_empty())) {
					r_out.push_back(e);
				}
			}
		}
		if (is_dir && p_recursive) {
			_list_dir(full, true, p_glob, r_out, p_depth + 1);
		}
		name = da->get_next();
	}
	da->list_dir_end();
}

Dictionary OpenDustEditorTools::_project_files_list(const Dictionary &p_params, const Dictionary &p_context) {
	String dir = p_params.get("dir", "res://");
	bool recursive = p_params.get("recursive", false);
	String glob = p_params.get("glob", "");
	if (!dir.begins_with("res://")) {
		return _err(OpenDustToolRegistry::RPC_INVALID_PARAMS, "dir must start with res://.");
	}
	Array entries;
	_list_dir(dir, recursive, glob, entries, 0);
	Dictionary d;
	d["entries"] = entries;
	return d;
}

Dictionary OpenDustEditorTools::_project_file_read(const Dictionary &p_params, const Dictionary &p_context) {
	String path = p_params.get("path", "");
	if (!path.begins_with("res://") && !path.begins_with("user://")) {
		return _err(OpenDustToolRegistry::RPC_INVALID_PARAMS, "path must start with res:// or user://.");
	}
	int64_t max_bytes = p_params.get("max_bytes", 1 << 20);
	Ref<FileAccess> f = FileAccess::open(path, FileAccess::READ);
	if (f.is_null()) {
		return _err(OpenDustToolRegistry::RPC_NOT_FOUND, vformat("Cannot open '%s'.", path));
	}
	int64_t len = f->get_length();
	int64_t to_read = MIN(len, max_bytes);
	Vector<uint8_t> bytes = f->get_buffer(to_read);
	Dictionary d;
	d["path"] = path;
	d["size"] = len;
	d["truncated"] = to_read < len;
	String text = String::utf8((const char *)bytes.ptr(), bytes.size());
	bool binary = false;
	for (int i = 0; i < MIN(bytes.size(), 4096); i++) {
		if (bytes[i] == 0) {
			binary = true;
			break;
		}
	}
	if (binary) {
		Dictionary tagged = OpenDustJSON::to_json(bytes);
		d["bytes_base64"] = tagged["v"];
	} else {
		d["text"] = text;
	}
	return d;
}

Dictionary OpenDustEditorTools::_project_file_write(const Dictionary &p_params, const Dictionary &p_context) {
	String path = p_params.get("path", "");
	if (!path.begins_with("res://") && !path.begins_with("user://")) {
		return _err(OpenDustToolRegistry::RPC_INVALID_PARAMS, "path must start with res:// or user://.");
	}
	if (!p_params.has("text")) {
		return _err(OpenDustToolRegistry::RPC_INVALID_PARAMS, "text is required.");
	}
	String text = p_params.get("text", "");
	String dir = path.get_base_dir();
	if (!DirAccess::exists(dir)) {
		Error mk = DirAccess::make_dir_recursive_absolute(ProjectSettings::get_singleton()->globalize_path(dir));
		if (mk != OK) {
			return _err(OpenDustToolRegistry::RPC_INTERNAL_ERROR, vformat("Cannot create directory '%s'.", dir));
		}
	}
	Ref<FileAccess> f = FileAccess::open(path, FileAccess::WRITE);
	if (f.is_null()) {
		return _err(OpenDustToolRegistry::RPC_INTERNAL_ERROR, vformat("Cannot write '%s'.", path));
	}
	f->store_string(text);
	f.unref();
	if (path.begins_with("res://") && EditorFileSystem::get_singleton()) {
		EditorFileSystem::get_singleton()->update_file(path);
	}
	Dictionary d = _ok();
	d["path"] = path;
	d["bytes"] = (int64_t)text.utf8().length();
	return d;
}

Dictionary OpenDustEditorTools::_project_rescan(const Dictionary &p_params, const Dictionary &p_context) {
	if (EditorFileSystem::get_singleton()) {
		EditorFileSystem::get_singleton()->scan();
	}
	return _ok();
}

// ---------------------------------------------------------------------------
// scene.*
// ---------------------------------------------------------------------------

Dictionary OpenDustEditorTools::_scene_list_open(const Dictionary &p_params, const Dictionary &p_context) {
	Dictionary d;
	d["scenes"] = EditorInterface::get_singleton()->get_open_scenes();
	Node *root = EditorInterface::get_singleton()->get_edited_scene_root();
	d["current"] = root ? root->get_scene_file_path() : String();
	return d;
}

Dictionary OpenDustEditorTools::_scene_open(const Dictionary &p_params, const Dictionary &p_context) {
	String path = p_params.get("path", "");
	if (!FileAccess::exists(path)) {
		return _err(OpenDustToolRegistry::RPC_NOT_FOUND, vformat("Scene '%s' does not exist.", path));
	}
	EditorInterface::get_singleton()->open_scene_from_path(path);
	Dictionary d = _ok();
	d["path"] = path;
	return d;
}

Dictionary OpenDustEditorTools::_scene_new(const Dictionary &p_params, const Dictionary &p_context) {
	String root_type = p_params.get("root_type", "Node3D");
	String root_name = p_params.get("root_name", "");
	String path = p_params.get("path", "");
	if (path.is_empty()) {
		return _err(OpenDustToolRegistry::RPC_INVALID_PARAMS, "path is required (the scene is created by saving a packed scene, then opened).");
	}
	if (!ClassDB::class_exists(root_type) || !ClassDB::is_parent_class(root_type, "Node") || !ClassDB::can_instantiate(root_type)) {
		return _err(OpenDustToolRegistry::RPC_INVALID_PARAMS, vformat("'%s' is not an instantiable Node class.", root_type));
	}
	if (FileAccess::exists(path)) {
		return _err(OpenDustToolRegistry::RPC_INVALID_TARGET, vformat("'%s' already exists; open it instead.", path));
	}
	Node *root = Object::cast_to<Node>(ClassDB::instantiate(root_type));
	ERR_FAIL_NULL_V(root, _err(OpenDustToolRegistry::RPC_INTERNAL_ERROR, "Instantiation failed."));
	root->set_name(root_name.is_empty() ? path.get_file().get_basename().to_pascal_case() : root_name);
	Ref<PackedScene> ps;
	ps.instantiate();
	Error err = ps->pack(root);
	if (err == OK) {
		String dir = path.get_base_dir();
		if (!DirAccess::exists(dir)) {
			DirAccess::make_dir_recursive_absolute(ProjectSettings::get_singleton()->globalize_path(dir));
		}
		err = ResourceSaver::save(ps, path);
	}
	memdelete(root);
	if (err != OK) {
		return _err(OpenDustToolRegistry::RPC_INTERNAL_ERROR, vformat("Could not save new scene '%s' (error %d).", path, (int)err));
	}
	if (EditorFileSystem::get_singleton()) {
		EditorFileSystem::get_singleton()->update_file(path);
	}
	EditorInterface::get_singleton()->open_scene_from_path(path);
	Dictionary d = _ok();
	d["path"] = path;
	return d;
}

Dictionary OpenDustEditorTools::_scene_save(const Dictionary &p_params, const Dictionary &p_context) {
	Dictionary err;
	Node *root = _scene_root(err);
	if (!root) {
		return err;
	}
	String path = p_params.get("path", "");
	if (path.is_empty()) {
		if (root->get_scene_file_path().is_empty()) {
			return _err(OpenDustToolRegistry::RPC_INVALID_PARAMS, "The scene has never been saved; provide a path.");
		}
		Error e = EditorInterface::get_singleton()->save_scene();
		if (e != OK) {
			return _err(OpenDustToolRegistry::RPC_INTERNAL_ERROR, vformat("Save failed (error %d).", (int)e));
		}
		path = root->get_scene_file_path();
	} else {
		EditorInterface::get_singleton()->save_scene_as(path, true);
	}
	Dictionary d = _ok();
	d["path"] = path;
	return d;
}

Dictionary OpenDustEditorTools::_scene_save_all(const Dictionary &p_params, const Dictionary &p_context) {
	EditorInterface::get_singleton()->save_all_scenes();
	return _ok();
}

Dictionary OpenDustEditorTools::_scene_close(const Dictionary &p_params, const Dictionary &p_context) {
	String path = p_params.get("path", "");
	if (!path.is_empty()) {
		if (!EditorNode::get_singleton()->is_scene_open(path)) {
			return _err(OpenDustToolRegistry::RPC_NOT_FOUND, vformat("Scene '%s' is not open.", path));
		}
		EditorInterface::get_singleton()->open_scene_from_path(path); // Switch to its tab.
	}
	EditorCommandPalette *palette = EditorInterface::get_singleton()->get_command_palette();
	if (!palette) {
		return _err(OpenDustToolRegistry::RPC_INTERNAL_ERROR, "Command palette unavailable.");
	}
	palette->execute_command("editor/close_scene");
	Dictionary d = _ok();
	d["note"] = "If the scene had unsaved changes the editor shows a confirmation dialog; save first to avoid it.";
	return d;
}

Dictionary OpenDustEditorTools::_scene_reload(const Dictionary &p_params, const Dictionary &p_context) {
	String path = p_params.get("path", "");
	if (path.is_empty()) {
		Dictionary err;
		Node *root = _scene_root(err);
		if (!root) {
			return err;
		}
		path = root->get_scene_file_path();
	}
	EditorInterface::get_singleton()->reload_scene_from_path(path);
	return _ok();
}

Dictionary OpenDustEditorTools::_scene_tree(const Dictionary &p_params, const Dictionary &p_context) {
	Dictionary err;
	Node *root = _scene_root(err);
	if (!root) {
		return err;
	}
	Node *from = OpenDustNodeUtils::resolve(root, p_params.get("root", "."));
	if (!from) {
		return _err(OpenDustToolRegistry::RPC_NOT_FOUND, "root node not found.");
	}
	int depth = p_params.get("depth", -1);
	bool props = p_params.get("include_properties", false);
	Dictionary d;
	d["scene"] = root->get_scene_file_path();
	d["tree"] = OpenDustNodeUtils::serialize_tree(root, from, depth, props);
	return d;
}

Dictionary OpenDustEditorTools::_scene_select(const Dictionary &p_params, const Dictionary &p_context) {
	Dictionary err;
	Node *root = _scene_root(err);
	if (!root) {
		return err;
	}
	EditorSelection *sel = EditorInterface::get_singleton()->get_selection();
	ERR_FAIL_NULL_V(sel, _err(OpenDustToolRegistry::RPC_INTERNAL_ERROR, "No selection object."));
	Array nodes = p_params.get("nodes", Array());
	sel->clear();
	Array selected;
	for (int i = 0; i < nodes.size(); i++) {
		Node *n = OpenDustNodeUtils::resolve(root, nodes[i]);
		if (n) {
			sel->add_node(n);
			selected.push_back(OpenDustNodeUtils::relative_path(root, n));
		}
	}
	if (selected.size() == 1) {
		EditorInterface::get_singleton()->edit_node(OpenDustNodeUtils::resolve(root, selected[0]));
	}
	Dictionary d;
	d["nodes"] = selected;
	return d;
}

Dictionary OpenDustEditorTools::_scene_selection(const Dictionary &p_params, const Dictionary &p_context) {
	Array paths;
	Node *root = EditorInterface::get_singleton()->get_edited_scene_root();
	EditorSelection *sel = EditorInterface::get_singleton()->get_selection();
	if (sel && root) {
		TypedArray<Node> nodes = sel->get_top_selected_nodes();
		for (int i = 0; i < nodes.size(); i++) {
			Node *n = Object::cast_to<Node>(nodes[i]);
			if (n) {
				paths.push_back(OpenDustNodeUtils::relative_path(root, n));
			}
		}
	}
	Dictionary d;
	d["nodes"] = paths;
	return d;
}

Dictionary OpenDustEditorTools::_scene_instantiate(const Dictionary &p_params, const Dictionary &p_context) {
	Dictionary err;
	Node *root = _scene_root(err);
	if (!root) {
		return err;
	}
	String scene_path = p_params.get("scene_path", "");
	Node *parent = _resolve(p_params.get("parent", "."), err);
	if (!parent) {
		return err;
	}
	Ref<PackedScene> ps = ResourceLoader::load(scene_path, "PackedScene");
	if (ps.is_null()) {
		return _err(OpenDustToolRegistry::RPC_NOT_FOUND, vformat("Cannot load scene '%s'.", scene_path));
	}
	if (scene_path == root->get_scene_file_path()) {
		return _err(OpenDustToolRegistry::RPC_INVALID_TARGET, "A scene cannot instantiate itself.");
	}
	Node *inst = ps->instantiate(PackedScene::GEN_EDIT_STATE_INSTANCE);
	if (!inst) {
		return _err(OpenDustToolRegistry::RPC_INTERNAL_ERROR, "Instantiation failed.");
	}
	String name = p_params.get("name", "");
	if (!name.is_empty()) {
		inst->set_name(name);
	}
	EditorUndoRedoManager *ur = _ur();
	ur->create_action(_action(p_context, "Instantiate " + scene_path.get_file()));
	ur->add_do_method(parent, "add_child", inst, true);
	ur->add_do_method(inst, "set_owner", root);
	ur->add_do_reference(inst);
	ur->add_undo_method(parent, "remove_child", inst);
	ur->commit_action();
	return _path_result(inst);
}

// ---------------------------------------------------------------------------
// run.*
// ---------------------------------------------------------------------------

Dictionary OpenDustEditorTools::_run_play(const Dictionary &p_params, const Dictionary &p_context) {
	EditorRunBar *bar = EditorRunBar::get_singleton();
	ERR_FAIL_NULL_V(bar, _err(OpenDustToolRegistry::RPC_INTERNAL_ERROR, "Run bar unavailable."));
	String scene = p_params.get("scene", "main");
	if (scene == "main" || scene.is_empty()) {
		bar->play_main_scene();
	} else if (scene == "current") {
		bar->play_current_scene();
	} else {
		bar->play_custom_scene(scene);
	}
	return _ok();
}

Dictionary OpenDustEditorTools::_run_stop(const Dictionary &p_params, const Dictionary &p_context) {
	EditorRunBar *bar = EditorRunBar::get_singleton();
	ERR_FAIL_NULL_V(bar, _err(OpenDustToolRegistry::RPC_INTERNAL_ERROR, "Run bar unavailable."));
	bar->stop_playing();
	return _ok();
}

Dictionary OpenDustEditorTools::_run_state(const Dictionary &p_params, const Dictionary &p_context) {
	EditorRunBar *bar = EditorRunBar::get_singleton();
	Dictionary d;
	bool playing = bar && bar->is_playing();
	d["state"] = playing ? "playing" : "stopped";
	d["scene"] = playing ? bar->get_playing_scene() : String();
	return d;
}

Dictionary OpenDustEditorTools::_run_capture(const Dictionary &p_params, const Dictionary &p_context) {
	// The running game is a separate process; its frames are captured through the
	// runtime bridge (world.capture on bridge-runtime.json).
	return _err(OpenDustToolRegistry::RPC_WRONG_MODE, "The running game is a separate process. Connect to <project>/.opendust/bridge-runtime.json and call world.capture.");
}

// ---------------------------------------------------------------------------
// editor.*
// ---------------------------------------------------------------------------

Dictionary OpenDustEditorTools::_editor_capture(const Dictionary &p_params, const Dictionary &p_context) {
	String which = p_params.get("viewport", "3d");
	Vector2i size;
	Variant vsize = p_params.get("size", Variant());
	if (vsize.get_type() == Variant::ARRAY) {
		Array a = vsize;
		if (a.size() >= 2) {
			size = Vector2i((int)a[0], (int)a[1]);
		}
	}
	Viewport *vp = nullptr;
	if (which == "2d") {
		vp = EditorInterface::get_singleton()->get_editor_viewport_2d();
	} else if (which == "main") {
		vp = EditorNode::get_singleton()->get_window();
	} else {
		int idx = 0;
		if (which.begins_with("3d:")) {
			idx = which.substr(3).to_int();
		}
		vp = EditorInterface::get_singleton()->get_editor_viewport_3d(idx);
	}
	if (!vp) {
		return _err(OpenDustToolRegistry::RPC_NOT_FOUND, vformat("Viewport '%s' unavailable.", which));
	}
	Dictionary d = OpenDustNodeUtils::capture_viewport(vp, size);
	if (d.is_empty()) {
		return _err(OpenDustToolRegistry::RPC_INTERNAL_ERROR, "Capture failed (viewport may not have rendered yet).");
	}
	d["viewport"] = which;
	return d;
}

Dictionary OpenDustEditorTools::_editor_undo(const Dictionary &p_params, const Dictionary &p_context) {
	bool did = _ur()->undo();
	Dictionary d;
	d["ok"] = did;
	return d;
}

Dictionary OpenDustEditorTools::_editor_redo(const Dictionary &p_params, const Dictionary &p_context) {
	bool did = _ur()->redo();
	Dictionary d;
	d["ok"] = did;
	return d;
}

Dictionary OpenDustEditorTools::_editor_history(const Dictionary &p_params, const Dictionary &p_context) {
	int limit = p_params.get("limit", 50);
	Array actions;
	int history_id = EditorNode::get_editor_data().get_current_edited_scene_history_id();
	UndoRedo *ur = _ur()->get_history_undo_redo(history_id);
	if (ur) {
		int count = ur->get_history_count();
		int current = ur->get_current_action();
		int start = MAX(0, count - limit);
		for (int i = start; i < count; i++) {
			String name = ur->get_action_name(i);
			Dictionary a;
			a["index"] = i;
			a["name"] = name;
			a["applied"] = i <= current;
			int colon = name.find(": ");
			if (colon > 0) {
				a["agent"] = name.substr(0, colon);
			}
			actions.push_back(a);
		}
	}
	Dictionary d;
	d["actions"] = actions;
	d["has_undo"] = _ur()->has_undo();
	d["has_redo"] = _ur()->has_redo();
	return d;
}

Dictionary OpenDustEditorTools::_editor_inspect(const Dictionary &p_params, const Dictionary &p_context) {
	Dictionary err;
	Node *n = _resolve(p_params.get("path", "."), err);
	if (!n) {
		return err;
	}
	EditorInterface::get_singleton()->edit_node(n);
	return _ok();
}

Dictionary OpenDustEditorTools::_editor_set_main_screen(const Dictionary &p_params, const Dictionary &p_context) {
	String name = p_params.get("name", "3D");
	EditorInterface::get_singleton()->set_main_screen_editor(name);
	return _ok();
}

Dictionary OpenDustEditorTools::_editor_command(const Dictionary &p_params, const Dictionary &p_context) {
	String name = p_params.get("name", "");
	if (name.is_empty()) {
		return _err(OpenDustToolRegistry::RPC_INVALID_PARAMS, "name is required (a command palette key such as editor/close_scene).");
	}
	EditorCommandPalette *palette = EditorInterface::get_singleton()->get_command_palette();
	ERR_FAIL_NULL_V(palette, _err(OpenDustToolRegistry::RPC_INTERNAL_ERROR, "Command palette unavailable."));
	palette->execute_command(name);
	return _ok();
}

// ---------------------------------------------------------------------------
// Registration (project/scene/run/editor)
// ---------------------------------------------------------------------------

void OpenDustEditorTools::_register_project_scene_run_editor() {
	Dictionary props;
	Array required;

	props = Dictionary();
	P("name", "string", "Project setting name, e.g. application/config/name.");
	REQ("name");
	_reg("project.settings_get", "Read a project setting.", props, required, &OpenDustEditorTools::_project_settings_get, false);

	props = Dictionary();
	P("name", "string", "Project setting name.");
	P("value", "", "New value (JSON; tagged {$type} form accepted).");
	REQ("name", "value");
	_reg("project.settings_set", "Write a project setting and save project.godot.", props, required, &OpenDustEditorTools::_project_settings_set, true);

	props = Dictionary();
	P("dir", "string", "Directory (res://...). Default res://.");
	P("recursive", "boolean", "Descend into subdirectories.");
	P("glob", "string", "Filename pattern, e.g. *.tscn.");
	_reg("project.files_list", "List files in the project.", props, Array(), &OpenDustEditorTools::_project_files_list, false);

	props = Dictionary();
	P("path", "string", "res:// or user:// path.");
	P("max_bytes", "integer", "Truncate after this many bytes (default 1 MiB).");
	REQ("path");
	_reg("project.file_read", "Read a project file as text (or base64 if binary).", props, required, &OpenDustEditorTools::_project_file_read, false);

	props = Dictionary();
	P("path", "string", "res:// or user:// path.");
	P("text", "string", "Full file contents.");
	REQ("path", "text");
	_reg("project.file_write", "Write a project file and rescan it.", props, required, &OpenDustEditorTools::_project_file_write, true);

	_reg("project.rescan", "Rescan the project filesystem.", Dictionary(), Array(), &OpenDustEditorTools::_project_rescan, false, true);

	_reg("scene.list_open", "List open scenes and the current one.", Dictionary(), Array(), &OpenDustEditorTools::_scene_list_open, false);

	props = Dictionary();
	P("path", "string", "res:// path of a .tscn/.scn.");
	REQ("path");
	_reg("scene.open", "Open a scene in the editor (switches to its tab if already open).", props, required, &OpenDustEditorTools::_scene_open, false);

	props = Dictionary();
	P("root_type", "string", "Root node class (default Node3D).");
	P("root_name", "string", "Root node name (default: from file name).");
	P("path", "string", "Where to save the new scene (res://...).");
	REQ("path");
	_reg("scene.new", "Create, save and open a new scene.", props, required, &OpenDustEditorTools::_scene_new, true);

	props = Dictionary();
	P("path", "string", "Save-as path; omit to save in place.");
	_reg("scene.save", "Save the current scene.", props, Array(), &OpenDustEditorTools::_scene_save, true);

	_reg("scene.save_all", "Save every open scene.", Dictionary(), Array(), &OpenDustEditorTools::_scene_save_all, true);

	props = Dictionary();
	P("path", "string", "Scene to close; omit for the current one.");
	_reg("scene.close", "Close a scene tab.", props, Array(), &OpenDustEditorTools::_scene_close, false);

	props = Dictionary();
	P("path", "string", "Scene to reload from disk; omit for the current one.");
	_reg("scene.reload", "Reload a scene from disk, discarding unsaved edits.", props, Array(), &OpenDustEditorTools::_scene_reload, true);

	props = Dictionary();
	P("root", "string", "Node path to start from ('.' = scene root).");
	P("depth", "integer", "Levels to descend; -1 for all.");
	P("include_properties", "boolean", "Include editor-visible properties per node.");
	_reg("scene.tree", "Serialize the edited scene's tree.", props, Array(), &OpenDustEditorTools::_scene_tree, false);

	props = Dictionary();
	P("nodes", "array", "Node paths to select.");
	REQ("nodes");
	_reg("scene.select", "Set the editor selection.", props, required, &OpenDustEditorTools::_scene_select, false);

	_reg("scene.selection", "Get the editor selection.", Dictionary(), Array(), &OpenDustEditorTools::_scene_selection, false);

	props = Dictionary();
	P("scene_path", "string", "Packed scene to instantiate.");
	P("parent", "string", "Parent node path ('.' = root).");
	P("name", "string", "Name for the instance.");
	REQ("scene_path", "parent");
	_reg("scene.instantiate", "Instantiate a packed scene under a node.", props, required, &OpenDustEditorTools::_scene_instantiate, true);

	props = Dictionary();
	P("scene", "string", "'main', 'current', or a res:// path.");
	_reg("run.play", "Run the project.", props, Array(), &OpenDustEditorTools::_run_play, false);
	_reg("run.stop", "Stop the running project.", Dictionary(), Array(), &OpenDustEditorTools::_run_stop, false);
	_reg("run.state", "Is the project running, and which scene.", Dictionary(), Array(), &OpenDustEditorTools::_run_state, false);
	props = Dictionary();
	P("size", "array", "[w, h] downscale.");
	_reg("run.capture", "Capture the running game (redirects to the runtime bridge).", props, Array(), &OpenDustEditorTools::_run_capture, false);

	props = Dictionary();
	P("viewport", "string", "'3d', '3d:N', '2d' or 'main'.");
	P("size", "array", "[w, h] downscale; omit for native.");
	_reg("editor.capture", "PNG (base64) of an editor viewport.", props, Array(), &OpenDustEditorTools::_editor_capture, false, true);
	_reg("editor.undo", "Undo the last action.", Dictionary(), Array(), &OpenDustEditorTools::_editor_undo, true);
	_reg("editor.redo", "Redo the next action.", Dictionary(), Array(), &OpenDustEditorTools::_editor_redo, true);
	props = Dictionary();
	P("limit", "integer", "How many recent actions (default 50).");
	_reg("editor.history", "Undo history for the current scene, with agent attribution.", props, Array(), &OpenDustEditorTools::_editor_history, false);
	props = Dictionary();
	P("path", "string", "Node path to focus in the inspector.");
	REQ("path");
	_reg("editor.inspect", "Focus the inspector on a node.", props, required, &OpenDustEditorTools::_editor_inspect, false);
	props = Dictionary();
	P("name", "string", "Main screen name: 2D, 3D, Script, Game, AssetLib.");
	REQ("name");
	_reg("editor.set_main_screen", "Switch the main editor screen.", props, required, &OpenDustEditorTools::_editor_set_main_screen, false);
	props = Dictionary();
	P("name", "string", "Command palette key, e.g. editor/close_scene.");
	REQ("name");
	_reg("editor.command", "Execute a command palette command.", props, required, &OpenDustEditorTools::_editor_command, true);
}

void OpenDustEditorTools::register_tools() {
	_register_project_scene_run_editor();
	_register_node_script_resource();
}

void OpenDustEditorTools::unregister_tools() {
	OpenDustToolRegistry *reg = OpenDustToolRegistry::get_singleton();
	if (!reg) {
		registered_names.clear();
		return;
	}
	for (const String &n : registered_names) {
		reg->unregister_tool(n);
	}
	registered_names.clear();
}

OpenDustEditorTools::~OpenDustEditorTools() {
	unregister_tools();
}
