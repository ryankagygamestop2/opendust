/**************************************************************************/
/*  opendust_runtime_tools.h                                              */
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

class Node;

// world.* tools available while a game is running. Created by the runtime
// autostart in register_types.cpp; other modules (soul, os) add their own.
class OpenDustRuntimeTools : public Object {
	GDCLASS(OpenDustRuntimeTools, Object);

	static OpenDustRuntimeTools *singleton;
	bool registered = false;

	Node *_root() const;
	Node *_resolve(const String &p_path, Dictionary &r_error) const;

	Dictionary _world_tree(const Dictionary &p_params, const Dictionary &p_context);
	Dictionary _world_rooms(const Dictionary &p_params, const Dictionary &p_context);
	Dictionary _world_room_of(const Dictionary &p_params, const Dictionary &p_context);
	Dictionary _world_capture(const Dictionary &p_params, const Dictionary &p_context);
	Dictionary _world_time(const Dictionary &p_params, const Dictionary &p_context);
	Dictionary _world_node_get(const Dictionary &p_params, const Dictionary &p_context);
	Dictionary _world_node_call(const Dictionary &p_params, const Dictionary &p_context);

protected:
	static void _bind_methods() {}

public:
	static OpenDustRuntimeTools *get_singleton();

	void register_tools();
	void unregister_tools();

	OpenDustRuntimeTools();
	~OpenDustRuntimeTools();
};
