/**************************************************************************/
/*  opendust_tool_registry.h                                              */
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
#include "core/templates/hash_map.h"
#include "core/templates/vector.h"
#include "core/variant/callable.h"
#include "core/variant/dictionary.h"

class Node;

// The single source of truth for what an agent can do in this engine.
// Modules (and GDScript addons) register tools here; OpenDustAgentServer
// lists and dispatches them. See docs/opendust/01-agent-bridge-protocol.md.
class OpenDustToolRegistry : public Object {
	GDCLASS(OpenDustToolRegistry, Object);

public:
	enum ToolFlags {
		TOOL_EDITOR = 1,
		TOOL_RUNTIME = 2,
		TOOL_MUTATES = 4,
		TOOL_SLOW = 8,
	};

	// JSON-RPC error codes shared by every tool provider.
	enum RpcError {
		RPC_PARSE_ERROR = -32700,
		RPC_INVALID_REQUEST = -32600,
		RPC_METHOD_NOT_FOUND = -32601,
		RPC_INVALID_PARAMS = -32602,
		RPC_INTERNAL_ERROR = -32603,
		RPC_NOT_AUTHENTICATED = -32001,
		RPC_WRONG_MODE = -32002,
		RPC_NOT_FOUND = -32003,
		RPC_INVALID_TARGET = -32004,
		RPC_ENGINE_BUSY = -32005,
		RPC_DENIED = -32006,
	};

	struct Tool {
		String name;
		String description;
		Dictionary input_schema;
		Callable handler;
		int flags = 0;
	};

private:
	static OpenDustToolRegistry *singleton;

	HashMap<String, Tool> tools;
	Vector<String> tool_order; // Stable listing order (registration order).
	HashMap<String, ObjectID> rooms;
	Vector<String> room_order;
	int capabilities_version = 0;

	void _bump_version();

	Dictionary _call_tool_bind(const String &p_name, const Dictionary &p_params, const Dictionary &p_context);

protected:
	static void _bind_methods();

public:
	static OpenDustToolRegistry *get_singleton();

	void register_tool(const String &p_name, const String &p_description, const Dictionary &p_input_schema, const Callable &p_handler, int p_flags);
	void unregister_tool(const String &p_name);
	bool has_tool(const String &p_name) const;
	int get_tool_flags(const String &p_name) const;
	Array list_tools(int p_mode_mask) const;
	Variant call_tool(const String &p_name, const Dictionary &p_params, const Dictionary &p_context, Dictionary &r_error);
	int get_capabilities_version() const;

	// Helpers for tool providers.
	static Dictionary make_error(int p_code, const String &p_message);
	static bool is_error(const Variant &p_result);
	static Array flags_to_array(int p_flags);
	static Dictionary object_schema(const Dictionary &p_properties, const Array &p_required = Array());
	static Dictionary prop(const String &p_type, const String &p_description = String());

	// Rooms: populated by opendust_slate's Room node, read by world.rooms.
	void register_room(const String &p_room_id, Node *p_node);
	void unregister_room(const String &p_room_id);
	Array get_rooms() const;
	Node *get_room_node(const String &p_room_id) const;
	Node *find_room_for_node(Node *p_node) const;

	OpenDustToolRegistry();
	~OpenDustToolRegistry();
};

VARIANT_ENUM_CAST(OpenDustToolRegistry::ToolFlags);
