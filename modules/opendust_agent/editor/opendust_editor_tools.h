/**************************************************************************/
/*  opendust_editor_tools.h                                               */
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

#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/templates/vector.h"

class EditorUndoRedoManager;
class Node;

// The editor tool set from docs/opendust/01-agent-bridge-protocol.md.
// Every mutating tool is an undo/redo action named "<agent>: <verb>".
// Split across opendust_editor_tools.cpp (project/scene/run/editor) and
// opendust_editor_tools_node.cpp (node/script/resource).
class OpenDustEditorTools : public Object {
	GDCLASS(OpenDustEditorTools, Object);

	Vector<String> registered_names;

	// Shared helpers.
	Node *_scene_root(Dictionary &r_error) const;
	Node *_resolve(const String &p_path, Dictionary &r_error) const;
	String _agent(const Dictionary &p_context) const;
	String _action(const Dictionary &p_context, const String &p_verb) const;
	EditorUndoRedoManager *_ur() const;
	Dictionary _ok() const;
	Dictionary _err(int p_code, const String &p_message) const;
	Dictionary _path_result(Node *p_node) const;
	void _reg(const String &p_name, const String &p_desc, const Dictionary &p_props, const Array &p_required, Dictionary (OpenDustEditorTools::*p_method)(const Dictionary &, const Dictionary &), bool p_mutates, bool p_slow = false);

	// project.*
	Dictionary _project_settings_get(const Dictionary &p_params, const Dictionary &p_context);
	Dictionary _project_settings_set(const Dictionary &p_params, const Dictionary &p_context);
	Dictionary _project_files_list(const Dictionary &p_params, const Dictionary &p_context);
	Dictionary _project_file_read(const Dictionary &p_params, const Dictionary &p_context);
	Dictionary _project_file_write(const Dictionary &p_params, const Dictionary &p_context);
	Dictionary _project_rescan(const Dictionary &p_params, const Dictionary &p_context);

	// scene.*
	Dictionary _scene_list_open(const Dictionary &p_params, const Dictionary &p_context);
	Dictionary _scene_open(const Dictionary &p_params, const Dictionary &p_context);
	Dictionary _scene_new(const Dictionary &p_params, const Dictionary &p_context);
	Dictionary _scene_save(const Dictionary &p_params, const Dictionary &p_context);
	Dictionary _scene_save_all(const Dictionary &p_params, const Dictionary &p_context);
	Dictionary _scene_close(const Dictionary &p_params, const Dictionary &p_context);
	Dictionary _scene_reload(const Dictionary &p_params, const Dictionary &p_context);
	Dictionary _scene_tree(const Dictionary &p_params, const Dictionary &p_context);
	Dictionary _scene_select(const Dictionary &p_params, const Dictionary &p_context);
	Dictionary _scene_selection(const Dictionary &p_params, const Dictionary &p_context);
	Dictionary _scene_instantiate(const Dictionary &p_params, const Dictionary &p_context);

	// run.*
	Dictionary _run_play(const Dictionary &p_params, const Dictionary &p_context);
	Dictionary _run_stop(const Dictionary &p_params, const Dictionary &p_context);
	Dictionary _run_state(const Dictionary &p_params, const Dictionary &p_context);
	Dictionary _run_capture(const Dictionary &p_params, const Dictionary &p_context);

	// editor.*
	Dictionary _editor_capture(const Dictionary &p_params, const Dictionary &p_context);
	Dictionary _editor_undo(const Dictionary &p_params, const Dictionary &p_context);
	Dictionary _editor_redo(const Dictionary &p_params, const Dictionary &p_context);
	Dictionary _editor_history(const Dictionary &p_params, const Dictionary &p_context);
	Dictionary _editor_inspect(const Dictionary &p_params, const Dictionary &p_context);
	Dictionary _editor_set_main_screen(const Dictionary &p_params, const Dictionary &p_context);
	Dictionary _editor_command(const Dictionary &p_params, const Dictionary &p_context);

	// node.*
	Dictionary _node_create(const Dictionary &p_params, const Dictionary &p_context);
	Dictionary _node_delete(const Dictionary &p_params, const Dictionary &p_context);
	Dictionary _node_rename(const Dictionary &p_params, const Dictionary &p_context);
	Dictionary _node_reparent(const Dictionary &p_params, const Dictionary &p_context);
	Dictionary _node_duplicate(const Dictionary &p_params, const Dictionary &p_context);
	Dictionary _node_get(const Dictionary &p_params, const Dictionary &p_context);
	Dictionary _node_set(const Dictionary &p_params, const Dictionary &p_context);
	Dictionary _node_call(const Dictionary &p_params, const Dictionary &p_context);
	Dictionary _node_list_properties(const Dictionary &p_params, const Dictionary &p_context);
	Dictionary _node_attach_script(const Dictionary &p_params, const Dictionary &p_context);
	Dictionary _node_detach_script(const Dictionary &p_params, const Dictionary &p_context);
	Dictionary _node_set_owner_editable(const Dictionary &p_params, const Dictionary &p_context);

	// script.*
	Dictionary _script_read(const Dictionary &p_params, const Dictionary &p_context);
	Dictionary _script_write(const Dictionary &p_params, const Dictionary &p_context);
	Dictionary _script_create(const Dictionary &p_params, const Dictionary &p_context);
	Dictionary _script_errors(const Dictionary &p_params, const Dictionary &p_context);
	Dictionary _script_open_in_editor(const Dictionary &p_params, const Dictionary &p_context);

	// resource.*
	Dictionary _resource_load(const Dictionary &p_params, const Dictionary &p_context);
	Dictionary _resource_create(const Dictionary &p_params, const Dictionary &p_context);
	Dictionary _resource_set(const Dictionary &p_params, const Dictionary &p_context);
	Dictionary _resource_save(const Dictionary &p_params, const Dictionary &p_context);

	void _register_project_scene_run_editor();
	void _register_node_script_resource();

protected:
	static void _bind_methods() {}

public:
	void register_tools();
	void unregister_tools();

	OpenDustEditorTools() {}
	~OpenDustEditorTools();
};
