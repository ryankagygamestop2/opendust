/**************************************************************************/
/*  agent_tools.h                                                         */
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

#include "core/variant/array.h"

#include "core/object/object_id.h"
#include "core/string/ustring.h"
#include "core/templates/hash_map.h"
#include "core/templates/local_vector.h"
#include "core/variant/dictionary.h"

class AgentBody;

// Every AgentBody in the tree, by object id. Populated from AgentBody's enter/exit tree.
class AgentBodyRegistry {
	static LocalVector<ObjectID> bodies;

public:
	static void add(AgentBody *p_body);
	static void remove(AgentBody *p_body);
	static void get_bodies(LocalVector<AgentBody *> &r_bodies);
	static AgentBody *find_by_path(const String &p_path);
	static String path_of(AgentBody *p_body); // path from the current scene root, else absolute
};

// The agent.* tools on the bridge. Registration is a no-op without opendust_agent.
class AgentTools {
	static bool registered;
	static HashMap<String, ObjectID> attachments; // session_id -> body

	static Dictionary _err(int p_code, const String &p_message);
	static AgentBody *_attached(const Dictionary &p_context, Dictionary *r_error);
	static Dictionary _schema(const Dictionary &p_properties, const Array &p_required = Array());
	static Dictionary _prop(const String &p_type, const String &p_description);

	// Handlers: (params, context) -> result or {"$error": …}.
	static Dictionary _bodies(const Dictionary &p_params, const Dictionary &p_context);
	static Dictionary _attach(const Dictionary &p_params, const Dictionary &p_context);
	static Dictionary _detach(const Dictionary &p_params, const Dictionary &p_context);
	static Dictionary _perceive(const Dictionary &p_params, const Dictionary &p_context);
	static Dictionary _move_to(const Dictionary &p_params, const Dictionary &p_context);
	static Dictionary _look_at(const Dictionary &p_params, const Dictionary &p_context);
	static Dictionary _stop(const Dictionary &p_params, const Dictionary &p_context);
	static Dictionary _interact(const Dictionary &p_params, const Dictionary &p_context);
	static Dictionary _hold(const Dictionary &p_params, const Dictionary &p_context);
	static Dictionary _drop(const Dictionary &p_params, const Dictionary &p_context);
	static Dictionary _say(const Dictionary &p_params, const Dictionary &p_context);
	static Dictionary _emote(const Dictionary &p_params, const Dictionary &p_context);
	static Dictionary _spawn(const Dictionary &p_params, const Dictionary &p_context);

public:
	// Safe to call repeatedly; registers once, when the registry singleton exists.
	static void ensure_registered();
	static void unregister_all();
	static bool is_registered() { return registered; }

	// Which body (if any) a bridge session is driving.
	static AgentBody *get_attached_body(const String &p_session_id);
	static void detach_session(const String &p_session_id);
	static void detach_body(AgentBody *p_body);
};
