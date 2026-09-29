/**************************************************************************/
/*  room.h                                                                */
/**************************************************************************/
/*                         This file is part of:                          */
/*                              OPENDUST                                  */
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

#include "scene/3d/node_3d.h"

// A room is a subtree with a working directory. Slates held inside it run
// their Claude session there; agent bodies standing in it report it as their
// location. The directory *is* the room's memory. See docs/opendust/00-architecture.md.
class Room : public Node3D {
	GDCLASS(Room, Node3D);

	String room_id;
	String room_dir; // absolute, res://, user://, or project-relative
	String display_name;
	String resolved_dir;
	bool registered = false;

	void _resolve_dir();
	void _write_marker();
	void _register();
	void _unregister();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	void set_room_id(const String &p_id);
	String get_room_id() const { return room_id; }
	void set_room_dir(const String &p_dir);
	String get_room_dir() const { return room_dir; }
	void set_display_name(const String &p_name);
	String get_display_name() const { return display_name; }

	// The room directory as an absolute filesystem path (created if missing at runtime).
	String get_absolute_dir() const;

	// Walks up from p_node (inclusive) and returns the nearest enclosing Room, or null.
	static Room *find_room_for(Node *p_node);

	Room();
};
