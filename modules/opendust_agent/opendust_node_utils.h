/**************************************************************************/
/*  opendust_node_utils.h                                                 */
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

#include "core/variant/dictionary.h"
#include "core/variant/variant.h"

class Node;
class Viewport;

// Serialization helpers shared by the editor and runtime tool sets.
class OpenDustNodeUtils {
public:
	// Path of p_node relative to p_root ("." for the root itself).
	static String relative_path(Node *p_root, Node *p_node);
	// Resolve "." / relative / absolute paths against p_root.
	static Node *resolve(Node *p_root, const String &p_path);

	// {name, type, path, script?, children[], properties?}
	static Dictionary serialize_tree(Node *p_root, Node *p_node, int p_depth, bool p_include_properties);
	// {type, path, script?, groups[], signals[], properties{}}
	static Dictionary serialize_node(Node *p_root, Node *p_node, const Array &p_property_filter, bool p_all_properties);
	// [{name, type, hint, hint_string, usage}]
	static Array list_properties(Object *p_object);
	// Coerce a wire value to the declared type of p_object's property.
	static Variant coerce_for_property(Object *p_object, const String &p_property, const Variant &p_value);
	// Collect p_node and all descendants whose owner is p_owner.
	static void collect_owned(Node *p_node, Node *p_owner, List<Node *> &r_out);

	// {png_base64, width, height}
	static Dictionary capture_viewport(Viewport *p_viewport, const Vector2i &p_size);
};
