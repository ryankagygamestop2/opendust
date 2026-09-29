/**************************************************************************/
/*  opendust_editor_tools_node.cpp                                        */
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
#include "core/io/resource.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/object/script_language.h"
#include "editor/editor_interface.h"
#include "editor/editor_node.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/file_system/editor_file_system.h"
#include "editor/script/script_editor_plugin.h"
#include "scene/main/node.h"

// ---------------------------------------------------------------------------
// node.*
// ---------------------------------------------------------------------------

Dictionary OpenDustEditorTools::_node_create(const Dictionary &p_params, const Dictionary &p_context) {
	Dictionary err;
	Node *root = _scene_root(err);
	if (!root) {
		return err;
	}
	Node *parent = _resolve(p_params.get("parent", "."), err);
	if (!parent) {
		return err;
	}
	String type = p_params.get("type", "");
	if (!ClassDB::class_exists(type) || !ClassDB::is_parent_class(type, "Node") || !ClassDB::can_instantiate(type)) {
		return _err(OpenDustToolRegistry::RPC_INVALID_PARAMS, vformat("'%s' is not an instantiable Node class.", type));
	}
	Node *n = Object::cast_to<Node>(ClassDB::instantiate(type));
	if (!n) {
		return _err(OpenDustToolRegistry::RPC_INTERNAL_ERROR, "Instantiation failed.");
	}
	String name = p_params.get("name", "");
	n->set_name(name.is_empty() ? type : name);

	EditorUndoRedoManager *ur = _ur();
	ur->create_action(_action(p_context, "Create " + type));
	ur->add_do_method(parent, "add_child", n, true);
	ur->add_do_method(n, "set_owner", root);
	ur->add_do_reference(n);
	Dictionary props = p_params.get("properties", Dictionary());
	for (const KeyValue<Variant, Variant> &kv : props) {
		String pname = kv.key;
		Variant v = OpenDustNodeUtils::coerce_for_property(n, pname, kv.value);
		if (pname.contains(":")) {
			ur->add_do_method(n, "set_indexed", NodePath(pname), v);
		} else {
			ur->add_do_property(n, pname, v);
		}
	}
	ur->add_undo_method(parent, "remove_child", n);
	ur->commit_action();
	return _path_result(n);
}

Dictionary OpenDustEditorTools::_node_delete(const Dictionary &p_params, const Dictionary &p_context) {
	Dictionary err;
	Node *root = _scene_root(err);
	if (!root) {
		return err;
	}
	Node *n = _resolve(p_params.get("path", ""), err);
	if (!n) {
		return err;
	}
	if (n == root) {
		return _err(OpenDustToolRegistry::RPC_INVALID_TARGET, "Cannot delete the scene root.");
	}
	Node *parent = n->get_parent();
	int index = n->get_index();
	List<Node *> owned;
	OpenDustNodeUtils::collect_owned(n, root, owned);

	EditorUndoRedoManager *ur = _ur();
	ur->create_action(_action(p_context, "Delete " + String(n->get_name())));
	ur->add_do_method(parent, "remove_child", n);
	ur->add_undo_method(parent, "add_child", n, true);
	ur->add_undo_method(parent, "move_child", n, index);
	for (Node *o : owned) {
		ur->add_undo_method(o, "set_owner", root);
	}
	ur->add_undo_reference(n);
	ur->commit_action();
	return _ok();
}

Dictionary OpenDustEditorTools::_node_rename(const Dictionary &p_params, const Dictionary &p_context) {
	Dictionary err;
	Node *n = _resolve(p_params.get("path", ""), err);
	if (!n) {
		return err;
	}
	String new_name = p_params.get("name", "");
	if (new_name.is_empty()) {
		return _err(OpenDustToolRegistry::RPC_INVALID_PARAMS, "name is required.");
	}
	String old_name = n->get_name();
	EditorUndoRedoManager *ur = _ur();
	ur->create_action(_action(p_context, vformat("Rename %s to %s", old_name, new_name)));
	ur->add_do_method(n, "set_name", new_name);
	ur->add_undo_method(n, "set_name", old_name);
	ur->commit_action();
	return _path_result(n);
}

Dictionary OpenDustEditorTools::_node_reparent(const Dictionary &p_params, const Dictionary &p_context) {
	Dictionary err;
	Node *root = _scene_root(err);
	if (!root) {
		return err;
	}
	Node *n = _resolve(p_params.get("path", ""), err);
	if (!n) {
		return err;
	}
	Node *new_parent = _resolve(p_params.get("new_parent", "."), err);
	if (!new_parent) {
		return err;
	}
	if (n == root) {
		return _err(OpenDustToolRegistry::RPC_INVALID_TARGET, "Cannot reparent the scene root.");
	}
	if (n == new_parent || n->is_ancestor_of(new_parent)) {
		return _err(OpenDustToolRegistry::RPC_INVALID_TARGET, "Cannot reparent a node into its own subtree.");
	}
	bool keep = p_params.get("keep_global_transform", true);
	Node *old_parent = n->get_parent();
	int old_index = n->get_index();
	List<Node *> owned;
	OpenDustNodeUtils::collect_owned(n, root, owned);

	EditorUndoRedoManager *ur = _ur();
	ur->create_action(_action(p_context, vformat("Reparent %s", String(n->get_name()))));
	ur->add_do_method(n, "reparent", new_parent, keep);
	for (Node *o : owned) {
		ur->add_do_method(o, "set_owner", root);
	}
	ur->add_undo_method(n, "reparent", old_parent, keep);
	ur->add_undo_method(old_parent, "move_child", n, old_index);
	for (Node *o : owned) {
		ur->add_undo_method(o, "set_owner", root);
	}
	ur->commit_action();
	return _path_result(n);
}

Dictionary OpenDustEditorTools::_node_duplicate(const Dictionary &p_params, const Dictionary &p_context) {
	Dictionary err;
	Node *root = _scene_root(err);
	if (!root) {
		return err;
	}
	Node *n = _resolve(p_params.get("path", ""), err);
	if (!n) {
		return err;
	}
	if (n == root) {
		return _err(OpenDustToolRegistry::RPC_INVALID_TARGET, "Cannot duplicate the scene root.");
	}
	Node *dup = n->duplicate();
	if (!dup) {
		return _err(OpenDustToolRegistry::RPC_INTERNAL_ERROR, "Duplicate failed.");
	}
	String name = p_params.get("name", "");
	if (!name.is_empty()) {
		dup->set_name(name);
	}
	Node *parent = n->get_parent();
	List<Node *> owned;
	// Everything in the duplicate belongs to the scene.
	owned.push_back(dup);
	for (int i = 0; i < dup->get_child_count(); i++) {
		OpenDustNodeUtils::collect_owned(dup->get_child(i), nullptr, owned);
	}

	EditorUndoRedoManager *ur = _ur();
	ur->create_action(_action(p_context, vformat("Duplicate %s", String(n->get_name()))));
	ur->add_do_method(parent, "add_child", dup, true);
	ur->add_do_method(parent, "move_child", dup, n->get_index() + 1);
	for (Node *o : owned) {
		ur->add_do_method(o, "set_owner", root);
	}
	ur->add_do_reference(dup);
	ur->add_undo_method(parent, "remove_child", dup);
	ur->commit_action();
	return _path_result(dup);
}

Dictionary OpenDustEditorTools::_node_get(const Dictionary &p_params, const Dictionary &p_context) {
	Dictionary err;
	Node *root = _scene_root(err);
	if (!root) {
		return err;
	}
	Node *n = _resolve(p_params.get("path", "."), err);
	if (!n) {
		return err;
	}
	Array filter = p_params.get("properties", Array());
	return OpenDustNodeUtils::serialize_node(root, n, filter, filter.is_empty());
}

Dictionary OpenDustEditorTools::_node_set(const Dictionary &p_params, const Dictionary &p_context) {
	Dictionary err;
	Node *n = _resolve(p_params.get("path", "."), err);
	if (!n) {
		return err;
	}
	Dictionary props = p_params.get("properties", Dictionary());
	if (props.is_empty()) {
		return _err(OpenDustToolRegistry::RPC_INVALID_PARAMS, "properties must be a non-empty object.");
	}
	EditorUndoRedoManager *ur = _ur();
	ur->create_action(_action(p_context, vformat("Set %d propert%s on %s", props.size(), props.size() == 1 ? "y" : "ies", String(n->get_name()))));
	Array applied;
	for (const KeyValue<Variant, Variant> &kv : props) {
		String pname = kv.key;
		Variant v = OpenDustNodeUtils::coerce_for_property(n, pname, kv.value);
		bool valid = false;
		if (pname.contains(":")) {
			Variant old = n->get_indexed(NodePath(pname).get_as_property_path().get_subnames(), &valid);
			if (!valid) {
				continue;
			}
			ur->add_do_method(n, "set_indexed", NodePath(pname), v);
			ur->add_undo_method(n, "set_indexed", NodePath(pname), old);
		} else {
			Variant old = n->get(pname, &valid);
			if (!valid) {
				continue;
			}
			ur->add_do_property(n, pname, v);
			ur->add_undo_property(n, pname, old);
		}
		applied.push_back(pname);
	}
	ur->commit_action();
	Dictionary d = _ok();
	d["applied"] = applied;
	return d;
}

Dictionary OpenDustEditorTools::_node_call(const Dictionary &p_params, const Dictionary &p_context) {
	Dictionary err;
	Node *n = _resolve(p_params.get("path", "."), err);
	if (!n) {
		return err;
	}
	String method = p_params.get("method", "");
	if (method.is_empty() || !n->has_method(method)) {
		return _err(OpenDustToolRegistry::RPC_NOT_FOUND, vformat("Method '%s' not found on %s.", method, String(n->get_name())));
	}
	Array args = OpenDustJSON::from_json(p_params.get("args", Array()));
	Variant ret = n->callv(method, args);
	EditorInterface::get_singleton()->mark_scene_as_unsaved();
	Dictionary d;
	d["result"] = OpenDustJSON::to_json(ret);
	return d;
}

Dictionary OpenDustEditorTools::_node_list_properties(const Dictionary &p_params, const Dictionary &p_context) {
	Dictionary err;
	Node *n = _resolve(p_params.get("path", "."), err);
	if (!n) {
		return err;
	}
	Dictionary d;
	d["type"] = n->get_class();
	d["properties"] = OpenDustNodeUtils::list_properties(n);
	return d;
}

Dictionary OpenDustEditorTools::_node_attach_script(const Dictionary &p_params, const Dictionary &p_context) {
	Dictionary err;
	Node *n = _resolve(p_params.get("path", "."), err);
	if (!n) {
		return err;
	}
	String script_path = p_params.get("script_path", "");
	Ref<Script> script = ResourceLoader::load(script_path, "Script");
	if (script.is_null()) {
		return _err(OpenDustToolRegistry::RPC_NOT_FOUND, vformat("Cannot load script '%s'.", script_path));
	}
	Variant old = n->get_script();
	EditorUndoRedoManager *ur = _ur();
	ur->create_action(_action(p_context, vformat("Attach %s to %s", script_path.get_file(), String(n->get_name()))));
	ur->add_do_method(n, "set_script", script);
	ur->add_undo_method(n, "set_script", old);
	ur->commit_action();
	EditorInterface::get_singleton()->edit_node(n);
	return _ok();
}

Dictionary OpenDustEditorTools::_node_detach_script(const Dictionary &p_params, const Dictionary &p_context) {
	Dictionary err;
	Node *n = _resolve(p_params.get("path", "."), err);
	if (!n) {
		return err;
	}
	Variant old = n->get_script();
	if (old.get_type() == Variant::NIL) {
		return _ok();
	}
	EditorUndoRedoManager *ur = _ur();
	ur->create_action(_action(p_context, vformat("Detach script from %s", String(n->get_name()))));
	ur->add_do_method(n, "set_script", Variant());
	ur->add_undo_method(n, "set_script", old);
	ur->commit_action();
	return _ok();
}

Dictionary OpenDustEditorTools::_node_set_owner_editable(const Dictionary &p_params, const Dictionary &p_context) {
	Dictionary err;
	Node *root = _scene_root(err);
	if (!root) {
		return err;
	}
	Node *n = _resolve(p_params.get("path", ""), err);
	if (!n) {
		return err;
	}
	if (n->get_scene_file_path().is_empty() || n == root) {
		return _err(OpenDustToolRegistry::RPC_INVALID_TARGET, "Node is not an instanced scene.");
	}
	bool editable = p_params.get("editable", true);
	bool was = root->is_editable_instance(n);
	EditorUndoRedoManager *ur = _ur();
	ur->create_action(_action(p_context, vformat("%s children of %s", editable ? "Expose" : "Hide", String(n->get_name()))));
	ur->add_do_method(root, "set_editable_instance", n, editable);
	ur->add_undo_method(root, "set_editable_instance", n, was);
	ur->commit_action();
	return _ok();
}

// ---------------------------------------------------------------------------
// script.*
// ---------------------------------------------------------------------------

static ScriptLanguage *_language_for_path(const String &p_path) {
	String ext = p_path.get_extension().to_lower();
	if (ext.is_empty()) {
		return nullptr;
	}
	return ScriptServer::get_language_for_extension(ext);
}

Dictionary OpenDustEditorTools::_script_read(const Dictionary &p_params, const Dictionary &p_context) {
	String path = p_params.get("path", "");
	if (!path.begins_with("res://")) {
		return _err(OpenDustToolRegistry::RPC_INVALID_PARAMS, "path must be a res:// path.");
	}
	Error e;
	String text = FileAccess::get_file_as_string(path, &e);
	if (e != OK) {
		return _err(OpenDustToolRegistry::RPC_NOT_FOUND, vformat("Cannot read '%s'.", path));
	}
	Dictionary d;
	d["path"] = path;
	d["text"] = text;
	ScriptLanguage *lang = _language_for_path(path);
	d["language"] = lang ? lang->get_name() : String();
	return d;
}

Dictionary OpenDustEditorTools::_script_write(const Dictionary &p_params, const Dictionary &p_context) {
	String path = p_params.get("path", "");
	if (!path.begins_with("res://")) {
		return _err(OpenDustToolRegistry::RPC_INVALID_PARAMS, "path must be a res:// path.");
	}
	if (!p_params.has("text")) {
		return _err(OpenDustToolRegistry::RPC_INVALID_PARAMS, "text is required.");
	}
	String text = p_params.get("text", "");
	String dir = path.get_base_dir();
	if (!DirAccess::exists(dir)) {
		DirAccess::make_dir_recursive_absolute(ProjectSettings::get_singleton()->globalize_path(dir));
	}
	Ref<FileAccess> f = FileAccess::open(path, FileAccess::WRITE);
	if (f.is_null()) {
		return _err(OpenDustToolRegistry::RPC_INTERNAL_ERROR, vformat("Cannot write '%s'.", path));
	}
	f->store_string(text);
	f.unref();

	// Keep the in-memory script (and any open editor) in sync with disk.
	if (ResourceCache::has(path)) {
		Ref<Script> s = ResourceCache::get_ref(path);
		if (s.is_valid()) {
			s->set_source_code(text);
			s->reload(true);
		}
	}
	if (EditorFileSystem::get_singleton()) {
		EditorFileSystem::get_singleton()->update_file(path);
	}
	if (ScriptEditor::get_singleton()) {
		ScriptEditor::get_singleton()->reload_scripts(true);
	}

	Dictionary d = _ok();
	d["path"] = path;
	// Surface parse errors right away so the agent doesn't have to ask.
	ScriptLanguage *lang = _language_for_path(path);
	if (lang) {
		List<ScriptLanguage::ScriptError> errors;
		bool valid = lang->validate(text, path, nullptr, &errors, nullptr, nullptr);
		Array errs;
		for (const ScriptLanguage::ScriptError &se : errors) {
			Dictionary e;
			e["path"] = se.path.is_empty() ? path : se.path;
			e["line"] = se.line;
			e["column"] = se.column;
			e["message"] = se.message;
			errs.push_back(e);
		}
		d["valid"] = valid;
		d["errors"] = errs;
	}
	return d;
}

Dictionary OpenDustEditorTools::_script_create(const Dictionary &p_params, const Dictionary &p_context) {
	String path = p_params.get("path", "");
	if (!path.begins_with("res://")) {
		return _err(OpenDustToolRegistry::RPC_INVALID_PARAMS, "path must be a res:// path.");
	}
	if (FileAccess::exists(path)) {
		return _err(OpenDustToolRegistry::RPC_INVALID_TARGET, vformat("'%s' already exists; use script.write.", path));
	}
	String language_name = p_params.get("language", "GDScript");
	String extends = p_params.get("extends", "Node");
	String template_text = p_params.get("template", "");

	ScriptLanguage *lang = nullptr;
	for (int i = 0; i < ScriptServer::get_language_count(); i++) {
		ScriptLanguage *l = ScriptServer::get_language(i);
		if (l && l->get_name().nocasecmp_to(language_name) == 0) {
			lang = l;
			break;
		}
	}
	if (!lang) {
		lang = _language_for_path(path);
	}
	if (!lang) {
		return _err(OpenDustToolRegistry::RPC_INVALID_PARAMS, vformat("Unknown script language '%s'.", language_name));
	}
	if (path.get_extension().to_lower() != lang->get_extension().to_lower()) {
		return _err(OpenDustToolRegistry::RPC_INVALID_PARAMS, vformat("Extension for %s must be .%s.", lang->get_name(), lang->get_extension()));
	}
	if (template_text.is_empty()) {
		template_text = "extends _BASE_\n\n\nfunc _ready() -> void:\n_TS_pass\n";
	}
	String class_name = path.get_file().get_basename().to_pascal_case();
	Ref<Script> script = lang->make_template(template_text, class_name, extends);
	if (script.is_null()) {
		script = Ref<Script>(Object::cast_to<Script>(ClassDB::instantiate(lang->get_type())));
		if (script.is_null()) {
			return _err(OpenDustToolRegistry::RPC_INTERNAL_ERROR, "Could not create script object.");
		}
		script->set_source_code(template_text.replace("_BASE_", extends).replace("_CLASS_", class_name).replace("_TS_", "\t"));
	}
	String dir = path.get_base_dir();
	if (!DirAccess::exists(dir)) {
		DirAccess::make_dir_recursive_absolute(ProjectSettings::get_singleton()->globalize_path(dir));
	}
	Error e = ResourceSaver::save(script, path);
	if (e != OK) {
		return _err(OpenDustToolRegistry::RPC_INTERNAL_ERROR, vformat("Could not save '%s' (error %d).", path, (int)e));
	}
	if (EditorFileSystem::get_singleton()) {
		EditorFileSystem::get_singleton()->update_file(path);
	}
	Dictionary d = _ok();
	d["path"] = path;
	d["text"] = script->get_source_code();
	return d;
}

Dictionary OpenDustEditorTools::_script_errors(const Dictionary &p_params, const Dictionary &p_context) {
	String path = p_params.get("path", "");
	Vector<String> paths;
	if (!path.is_empty()) {
		paths.push_back(path);
	} else {
		// Every script in the edited scene.
		Node *root = EditorInterface::get_singleton()->get_edited_scene_root();
		if (root) {
			List<Node *> stack;
			stack.push_back(root);
			while (!stack.is_empty()) {
				Node *n = stack.front()->get();
				stack.pop_front();
				Ref<Script> s = n->get_script();
				if (s.is_valid() && !s->get_path().is_empty() && !paths.has(s->get_path())) {
					paths.push_back(s->get_path());
				}
				for (int i = 0; i < n->get_child_count(); i++) {
					stack.push_back(n->get_child(i));
				}
			}
		}
	}
	Array all;
	for (const String &p : paths) {
		ScriptLanguage *lang = _language_for_path(p);
		if (!lang) {
			continue;
		}
		Error e;
		String text = FileAccess::get_file_as_string(p, &e);
		if (e != OK) {
			continue;
		}
		List<ScriptLanguage::ScriptError> errors;
		lang->validate(text, p, nullptr, &errors, nullptr, nullptr);
		for (const ScriptLanguage::ScriptError &se : errors) {
			Dictionary d;
			d["path"] = se.path.is_empty() ? p : se.path;
			d["line"] = se.line;
			d["column"] = se.column;
			d["message"] = se.message;
			all.push_back(d);
		}
	}
	Dictionary d;
	d["errors"] = all;
	d["checked"] = paths.size();
	return d;
}

Dictionary OpenDustEditorTools::_script_open_in_editor(const Dictionary &p_params, const Dictionary &p_context) {
	String path = p_params.get("path", "");
	Ref<Resource> res = ResourceLoader::load(path);
	if (res.is_null()) {
		return _err(OpenDustToolRegistry::RPC_NOT_FOUND, vformat("Cannot load '%s'.", path));
	}
	int line = p_params.get("line", -1);
	ScriptEditor *se = ScriptEditor::get_singleton();
	ERR_FAIL_NULL_V(se, _err(OpenDustToolRegistry::RPC_INTERNAL_ERROR, "Script editor unavailable."));
	EditorInterface::get_singleton()->set_main_screen_editor("Script");
	se->edit(res, line > 0 ? line - 1 : -1, 0, true);
	return _ok();
}

// ---------------------------------------------------------------------------
// resource.*
// ---------------------------------------------------------------------------

static Dictionary _resource_props(const Ref<Resource> &p_res) {
	Dictionary props;
	List<PropertyInfo> plist;
	p_res->get_property_list(&plist);
	for (const PropertyInfo &pi : plist) {
		if (!(pi.usage & PROPERTY_USAGE_EDITOR) || (pi.usage & PROPERTY_USAGE_CATEGORY) || (pi.usage & PROPERTY_USAGE_GROUP) || (pi.usage & PROPERTY_USAGE_SUBGROUP)) {
			continue;
		}
		if (pi.type == Variant::NIL) {
			continue;
		}
		bool valid = false;
		Variant v = p_res->get(pi.name, &valid);
		if (valid) {
			props[pi.name] = OpenDustJSON::to_json(v);
		}
	}
	return props;
}

Dictionary OpenDustEditorTools::_resource_load(const Dictionary &p_params, const Dictionary &p_context) {
	String path = p_params.get("path", "");
	Ref<Resource> res = ResourceLoader::load(path);
	if (res.is_null()) {
		return _err(OpenDustToolRegistry::RPC_NOT_FOUND, vformat("Cannot load '%s'.", path));
	}
	Dictionary d;
	d["class"] = res->get_class();
	d["path"] = res->get_path();
	d["properties"] = _resource_props(res);
	return d;
}

Dictionary OpenDustEditorTools::_resource_create(const Dictionary &p_params, const Dictionary &p_context) {
	String cls = p_params.get("class", "");
	String path = p_params.get("path", "");
	if (!ClassDB::class_exists(cls) || !ClassDB::is_parent_class(cls, "Resource") || !ClassDB::can_instantiate(cls)) {
		return _err(OpenDustToolRegistry::RPC_INVALID_PARAMS, vformat("'%s' is not an instantiable Resource class.", cls));
	}
	if (!path.begins_with("res://")) {
		return _err(OpenDustToolRegistry::RPC_INVALID_PARAMS, "path must be a res:// path.");
	}
	if (FileAccess::exists(path)) {
		return _err(OpenDustToolRegistry::RPC_INVALID_TARGET, vformat("'%s' already exists.", path));
	}
	Ref<Resource> res = Ref<Resource>(Object::cast_to<Resource>(ClassDB::instantiate(cls)));
	if (res.is_null()) {
		return _err(OpenDustToolRegistry::RPC_INTERNAL_ERROR, "Instantiation failed.");
	}
	Dictionary props = p_params.get("properties", Dictionary());
	for (const KeyValue<Variant, Variant> &kv : props) {
		String pname = kv.key;
		res->set(pname, OpenDustNodeUtils::coerce_for_property(res.ptr(), pname, kv.value));
	}
	String dir = path.get_base_dir();
	if (!DirAccess::exists(dir)) {
		DirAccess::make_dir_recursive_absolute(ProjectSettings::get_singleton()->globalize_path(dir));
	}
	Error e = ResourceSaver::save(res, path);
	if (e != OK) {
		return _err(OpenDustToolRegistry::RPC_INTERNAL_ERROR, vformat("Could not save '%s' (error %d).", path, (int)e));
	}
	if (EditorFileSystem::get_singleton()) {
		EditorFileSystem::get_singleton()->update_file(path);
	}
	Dictionary d = _ok();
	d["path"] = path;
	d["class"] = cls;
	return d;
}

Dictionary OpenDustEditorTools::_resource_set(const Dictionary &p_params, const Dictionary &p_context) {
	String path = p_params.get("path", "");
	Ref<Resource> res = ResourceLoader::load(path);
	if (res.is_null()) {
		return _err(OpenDustToolRegistry::RPC_NOT_FOUND, vformat("Cannot load '%s'.", path));
	}
	Dictionary props = p_params.get("properties", Dictionary());
	if (props.is_empty()) {
		return _err(OpenDustToolRegistry::RPC_INVALID_PARAMS, "properties must be a non-empty object.");
	}
	EditorUndoRedoManager *ur = _ur();
	ur->create_action(_action(p_context, vformat("Edit %s", path.get_file())), UndoRedo::MERGE_DISABLE, res.ptr());
	Array applied;
	for (const KeyValue<Variant, Variant> &kv : props) {
		String pname = kv.key;
		bool valid = false;
		Variant old = res->get(pname, &valid);
		if (!valid) {
			continue;
		}
		ur->add_do_property(res.ptr(), pname, OpenDustNodeUtils::coerce_for_property(res.ptr(), pname, kv.value));
		ur->add_undo_property(res.ptr(), pname, old);
		applied.push_back(pname);
	}
	ur->commit_action();
	Dictionary d = _ok();
	d["applied"] = applied;
	d["note"] = "In memory only; call resource.save to persist.";
	return d;
}

Dictionary OpenDustEditorTools::_resource_save(const Dictionary &p_params, const Dictionary &p_context) {
	String path = p_params.get("path", "");
	Ref<Resource> res;
	if (ResourceCache::has(path)) {
		res = ResourceCache::get_ref(path);
	} else {
		res = ResourceLoader::load(path);
	}
	if (res.is_null()) {
		return _err(OpenDustToolRegistry::RPC_NOT_FOUND, vformat("Cannot load '%s'.", path));
	}
	Error e = ResourceSaver::save(res, path);
	if (e != OK) {
		return _err(OpenDustToolRegistry::RPC_INTERNAL_ERROR, vformat("Could not save '%s' (error %d).", path, (int)e));
	}
	if (EditorFileSystem::get_singleton()) {
		EditorFileSystem::get_singleton()->update_file(path);
	}
	return _ok();
}

// ---------------------------------------------------------------------------
// Registration (node/script/resource)
// ---------------------------------------------------------------------------

#define P(m_name, m_type, m_desc) props[m_name] = OpenDustToolRegistry::prop(m_type, m_desc)
#define REQ(...)                                                              \
	{                                                                         \
		const char *_r[] = { __VA_ARGS__ };                                   \
		required = Array();                                                   \
		for (size_t _i = 0; _i < sizeof(_r) / sizeof(_r[0]); _i++) {          \
			required.push_back(_r[_i]);                                       \
		}                                                                     \
	}

void OpenDustEditorTools::_register_node_script_resource() {
	Dictionary props;
	Array required;

	props = Dictionary();
	P("parent", "string", "Parent node path ('.' = scene root).");
	P("type", "string", "Node class, e.g. MeshInstance3D.");
	P("name", "string", "Node name (default: the class name).");
	P("properties", "object", "Initial property values (name -> value).");
	REQ("parent", "type");
	_reg("node.create", "Create a node under a parent.", props, required, &OpenDustEditorTools::_node_create, true);

	props = Dictionary();
	P("path", "string", "Node path.");
	REQ("path");
	_reg("node.delete", "Delete a node (undoable).", props, required, &OpenDustEditorTools::_node_delete, true);

	props = Dictionary();
	P("path", "string", "Node path.");
	P("name", "string", "New name.");
	REQ("path", "name");
	_reg("node.rename", "Rename a node.", props, required, &OpenDustEditorTools::_node_rename, true);

	props = Dictionary();
	P("path", "string", "Node path.");
	P("new_parent", "string", "New parent path.");
	P("keep_global_transform", "boolean", "Keep world transform (default true).");
	REQ("path", "new_parent");
	_reg("node.reparent", "Move a node under a different parent.", props, required, &OpenDustEditorTools::_node_reparent, true);

	props = Dictionary();
	P("path", "string", "Node path.");
	P("name", "string", "Name for the copy.");
	REQ("path");
	_reg("node.duplicate", "Duplicate a node next to itself.", props, required, &OpenDustEditorTools::_node_duplicate, true);

	props = Dictionary();
	P("path", "string", "Node path ('.' = scene root).");
	P("properties", "array", "Property names to read; omit for all editor-visible properties.");
	_reg("node.get", "Read a node's type, script, groups, signals and properties.", props, Array(), &OpenDustEditorTools::_node_get, false);

	props = Dictionary();
	P("path", "string", "Node path.");
	P("properties", "object", "name -> value. Sub-properties like position:x are allowed.");
	REQ("path", "properties");
	_reg("node.set", "Set properties on a node (one undoable action).", props, required, &OpenDustEditorTools::_node_set, true);

	props = Dictionary();
	P("path", "string", "Node path.");
	P("method", "string", "Method name.");
	P("args", "array", "Positional arguments.");
	REQ("path", "method");
	_reg("node.call", "Call a method on a node in the editor (marks the scene unsaved; not undoable).", props, required, &OpenDustEditorTools::_node_call, true);

	props = Dictionary();
	P("path", "string", "Node path.");
	REQ("path");
	_reg("node.list_properties", "List a node's properties with types and hints.", props, required, &OpenDustEditorTools::_node_list_properties, false);

	props = Dictionary();
	P("path", "string", "Node path.");
	P("script_path", "string", "res:// path of the script.");
	REQ("path", "script_path");
	_reg("node.attach_script", "Attach a script to a node.", props, required, &OpenDustEditorTools::_node_attach_script, true);

	props = Dictionary();
	P("path", "string", "Node path.");
	REQ("path");
	_reg("node.detach_script", "Remove the script from a node.", props, required, &OpenDustEditorTools::_node_detach_script, true);

	props = Dictionary();
	P("path", "string", "Instanced scene node path.");
	P("editable", "boolean", "Expose (true) or hide (false) the instance's children.");
	REQ("path");
	_reg("node.set_owner_editable", "Toggle 'editable children' on an instanced scene.", props, required, &OpenDustEditorTools::_node_set_owner_editable, true);

	props = Dictionary();
	P("path", "string", "res:// path of the script.");
	REQ("path");
	_reg("script.read", "Read a script's source.", props, required, &OpenDustEditorTools::_script_read, false);

	props = Dictionary();
	P("path", "string", "res:// path of the script.");
	P("text", "string", "Full source.");
	REQ("path", "text");
	_reg("script.write", "Write a script's source, reload it, and report parse errors.", props, required, &OpenDustEditorTools::_script_write, true);

	props = Dictionary();
	P("path", "string", "res:// path for the new script.");
	P("language", "string", "GDScript (default) or another installed language.");
	P("extends", "string", "Base class (default Node).");
	P("template", "string", "Template source; _BASE_, _CLASS_, _TS_ are substituted.");
	REQ("path");
	_reg("script.create", "Create a new script from a template.", props, required, &OpenDustEditorTools::_script_create, true);

	props = Dictionary();
	P("path", "string", "Script to check; omit to check every script in the edited scene.");
	_reg("script.errors", "Parse scripts and list errors.", props, Array(), &OpenDustEditorTools::_script_errors, false);

	props = Dictionary();
	P("path", "string", "Script path.");
	P("line", "integer", "1-based line to jump to.");
	REQ("path");
	_reg("script.open_in_editor", "Open a script in the script editor.", props, required, &OpenDustEditorTools::_script_open_in_editor, false);

	props = Dictionary();
	P("path", "string", "Resource path (res://... or res://scene.tscn::id).");
	REQ("path");
	_reg("resource.load", "Load a resource and read its editor-visible properties.", props, required, &OpenDustEditorTools::_resource_load, false);

	props = Dictionary();
	P("class", "string", "Resource class, e.g. StandardMaterial3D.");
	P("path", "string", "Where to save it (res://...).");
	P("properties", "object", "Initial property values.");
	REQ("class", "path");
	_reg("resource.create", "Create and save a new resource.", props, required, &OpenDustEditorTools::_resource_create, true);

	props = Dictionary();
	P("path", "string", "Resource path.");
	P("properties", "object", "name -> value.");
	REQ("path", "properties");
	_reg("resource.set", "Set properties on a loaded resource (undoable, in memory).", props, required, &OpenDustEditorTools::_resource_set, true);

	props = Dictionary();
	P("path", "string", "Resource path.");
	REQ("path");
	_reg("resource.save", "Save a resource to disk.", props, required, &OpenDustEditorTools::_resource_save, true);
}
