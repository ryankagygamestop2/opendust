/**************************************************************************/
/*  register_types.cpp                                                    */
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

#include "register_types.h"

#include "opendust_agent_server.h"
#include "opendust_runtime_tools.h"
#include "opendust_tool_registry.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/object/message_queue.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"

#ifdef TOOLS_ENABLED
#include "editor/opendust_editor_plugin.h"

#include "editor/plugins/editor_plugin.h"
#endif

static OpenDustToolRegistry *_registry = nullptr;
static OpenDustRuntimeTools *_runtime_tools = nullptr;

// Runtime autostart: wait until the SceneTree exists, then attach a runtime
// server to the root. Re-queues itself if the tree isn't up yet.
static void _opendust_runtime_autostart() {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	SceneTree *tree = SceneTree::get_singleton();
	if (!tree) {
		// Main loop may not be a SceneTree at all (e.g. --script). Try once
		// more on the next flush; if there's still nothing, give up quietly.
		static int attempts = 0;
		if (attempts++ < 3) {
			MessageQueue::get_singleton()->push_callable(callable_mp_static(&_opendust_runtime_autostart));
		}
		return;
	}
	if (OpenDustAgentServer::get_singleton()) {
		return;
	}
	Window *root = tree->get_root();
	if (!root) {
		return;
	}
	OpenDustAgentServer *server = memnew(OpenDustAgentServer);
	root->add_child(server, false, Node::INTERNAL_MODE_BACK);
	if (server->start(OpenDustAgentServer::MODE_RUNTIME) == OK) {
		if (!_runtime_tools) {
			_runtime_tools = memnew(OpenDustRuntimeTools);
		}
		_runtime_tools->register_tools();
	}
}

void initialize_opendust_agent_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SERVERS) {
		GDREGISTER_CLASS(OpenDustToolRegistry);
		_registry = memnew(OpenDustToolRegistry);
		Engine::get_singleton()->add_singleton(Engine::Singleton("OpenDustToolRegistry", _registry));

		// Project settings, defined early so they appear in the settings dialog.
		GLOBAL_DEF(PropertyInfo(Variant::INT, "opendust/agent/port", PROPERTY_HINT_RANGE, "0,65535,1"), 0);
		GLOBAL_DEF(PropertyInfo(Variant::STRING, "opendust/agent/policy", PROPERTY_HINT_ENUM, "open,no_mutations,allowlist"), "");
		GLOBAL_DEF(PropertyInfo(Variant::PACKED_STRING_ARRAY, "opendust/agent/allowed_tools"), PackedStringArray());
		GLOBAL_DEF("opendust/agent/runtime_autostart", true);
		GLOBAL_DEF(PropertyInfo(Variant::STRING, "opendust/agent/mcp_bridge_script", PROPERTY_HINT_GLOBAL_FILE, "*.py"), "");
	}

	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		GDREGISTER_CLASS(OpenDustAgentServer);
		GDREGISTER_ABSTRACT_CLASS(OpenDustRuntimeTools);

		if (!Engine::get_singleton()->is_editor_hint() && (bool)GLOBAL_GET("opendust/agent/runtime_autostart")) {
			MessageQueue::get_singleton()->push_callable(callable_mp_static(&_opendust_runtime_autostart));
		}
	}

#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		EditorPlugins::add_by_type<OpenDustEditorPlugin>();
	}
#endif
}

void uninitialize_opendust_agent_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		if (_runtime_tools) {
			memdelete(_runtime_tools);
			_runtime_tools = nullptr;
		}
	}
	if (p_level == MODULE_INITIALIZATION_LEVEL_SERVERS) {
		if (_registry) {
			memdelete(_registry);
			_registry = nullptr;
		}
	}
}
