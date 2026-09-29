/**************************************************************************/
/*  opendust_editor_plugin.cpp                                            */
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

#include "opendust_editor_plugin.h"

#include "opendust_agents_dock.h"
#include "opendust_editor_tools.h"

#include "../opendust_agent_server.h"
#include "../opendust_node_utils.h"

#include "editor/editor_data.h"
#include "editor/editor_interface.h"
#include "editor/editor_node.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/run/editor_run_bar.h"

void OpenDustEditorPlugin::_on_scene_changed() {
	if (!server) {
		return;
	}
	Dictionary d;
	Node *root = EditorInterface::get_singleton()->get_edited_scene_root();
	d["scene"] = root ? root->get_scene_file_path() : String();
	d["reason"] = "opened";
	server->broadcast("event.scene_changed", d);
}

void OpenDustEditorPlugin::_on_scene_saved(const String &p_path) {
	if (!server) {
		return;
	}
	Dictionary d;
	d["scene"] = p_path;
	d["reason"] = "saved";
	server->broadcast("event.scene_changed", d);
}

void OpenDustEditorPlugin::_on_scene_closed(const String &p_path) {
	if (!server) {
		return;
	}
	Dictionary d;
	d["scene"] = p_path;
	d["reason"] = "closed";
	server->broadcast("event.scene_changed", d);
}

void OpenDustEditorPlugin::_on_undo_redo_version_changed() {
	if (!server) {
		return;
	}
	Dictionary d;
	Node *root = EditorInterface::get_singleton()->get_edited_scene_root();
	d["scene"] = root ? root->get_scene_file_path() : String();
	d["reason"] = "edited";
	server->broadcast("event.scene_changed", d);
}

void OpenDustEditorPlugin::_on_selection_changed() {
	if (!server) {
		return;
	}
	Array paths;
	Node *root = EditorInterface::get_singleton()->get_edited_scene_root();
	EditorSelection *sel = EditorInterface::get_singleton()->get_selection();
	if (sel) {
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
	server->broadcast("event.selection_changed", d);
}

void OpenDustEditorPlugin::_broadcast_run_state() {
	if (!server) {
		return;
	}
	Dictionary d;
	bool playing = EditorRunBar::get_singleton() && EditorRunBar::get_singleton()->is_playing();
	d["state"] = playing ? "playing" : "stopped";
	if (playing) {
		d["scene"] = EditorRunBar::get_singleton()->get_playing_scene();
	}
	server->broadcast("event.run_state", d);
}

void OpenDustEditorPlugin::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			EditorNode *en = EditorNode::get_singleton();
			ERR_FAIL_NULL(en);

			server = memnew(OpenDustAgentServer);
			en->add_child(server, false, Node::INTERNAL_MODE_BACK);
			Error err = server->start(OpenDustAgentServer::MODE_EDITOR);
			if (err != OK) {
				WARN_PRINT("OpenDust: editor agent bridge failed to start; agents cannot connect.");
			}

			tools = memnew(OpenDustEditorTools);
			tools->register_tools();

			dock = memnew(OpenDustAgentsDock);
			dock->set_name("Agents");
			dock->set_server(server);
			add_control_to_dock(DOCK_SLOT_RIGHT_BL, dock);

			en->connect("scene_changed", callable_mp(this, &OpenDustEditorPlugin::_on_scene_changed));
			en->connect("scene_saved", callable_mp(this, &OpenDustEditorPlugin::_on_scene_saved));
			en->connect("scene_closed", callable_mp(this, &OpenDustEditorPlugin::_on_scene_closed));
			EditorSelection *sel = EditorInterface::get_singleton()->get_selection();
			if (sel) {
				sel->connect("selection_changed", callable_mp(this, &OpenDustEditorPlugin::_on_selection_changed));
			}
			if (EditorUndoRedoManager::get_singleton()) {
				EditorUndoRedoManager::get_singleton()->connect("version_changed", callable_mp(this, &OpenDustEditorPlugin::_on_undo_redo_version_changed));
			}
			set_process(true);
		} break;

		case NOTIFICATION_PROCESS: {
			bool playing = EditorRunBar::get_singleton() && EditorRunBar::get_singleton()->is_playing();
			if (playing != was_playing) {
				was_playing = playing;
				_broadcast_run_state();
			}
		} break;

		case NOTIFICATION_EXIT_TREE: {
			set_process(false);
			EditorNode *en = EditorNode::get_singleton();
			if (en) {
				en->disconnect("scene_changed", callable_mp(this, &OpenDustEditorPlugin::_on_scene_changed));
				en->disconnect("scene_saved", callable_mp(this, &OpenDustEditorPlugin::_on_scene_saved));
				en->disconnect("scene_closed", callable_mp(this, &OpenDustEditorPlugin::_on_scene_closed));
			}
			EditorSelection *sel = EditorInterface::get_singleton() ? EditorInterface::get_singleton()->get_selection() : nullptr;
			if (sel && sel->is_connected("selection_changed", callable_mp(this, &OpenDustEditorPlugin::_on_selection_changed))) {
				sel->disconnect("selection_changed", callable_mp(this, &OpenDustEditorPlugin::_on_selection_changed));
			}
			if (EditorUndoRedoManager::get_singleton() && EditorUndoRedoManager::get_singleton()->is_connected("version_changed", callable_mp(this, &OpenDustEditorPlugin::_on_undo_redo_version_changed))) {
				EditorUndoRedoManager::get_singleton()->disconnect("version_changed", callable_mp(this, &OpenDustEditorPlugin::_on_undo_redo_version_changed));
			}
			if (dock) {
				remove_control_from_docks(dock);
				dock->queue_free();
				dock = nullptr;
			}
			if (tools) {
				tools->unregister_tools();
				memdelete(tools);
				tools = nullptr;
			}
			if (server) {
				server->stop();
				server->queue_free();
				server = nullptr;
			}
		} break;
	}
}

OpenDustEditorPlugin::OpenDustEditorPlugin() {
}

OpenDustEditorPlugin::~OpenDustEditorPlugin() {
	if (tools) {
		memdelete(tools);
		tools = nullptr;
	}
}
