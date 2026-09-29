/**************************************************************************/
/*  opendust_json.h                                                       */
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

#include "core/object/object.h"
#include "core/variant/variant.h"

class Node;

// Value encoding rules from docs/opendust/01-agent-bridge-protocol.md:
//  - plain JSON for null/bool/int/float/string/array/dict
//  - {"$type": "Vector3", "v": [x, y, z]} for math types, NodePath, Color, RID, packed arrays
//  - {"$type": "Object", "class": ..., "path": ..., "id": ...} for objects, never inlined
// from_json() accepts both the tagged form and a plain array where the target
// type is known (coerce()).
class OpenDustJSON {
public:
	static const int MAX_DEPTH = 32;

	// Variant -> JSON-safe Variant.
	static Variant to_json(const Variant &p_value, int p_depth = 0);
	// JSON-safe Variant -> Variant (decodes tagged values, recurses).
	static Variant from_json(const Variant &p_json, int p_depth = 0);
	// Coerce a decoded value to the Variant type a property expects
	// (e.g. [1,2,3] -> Vector3, "#ff0000" -> Color, "Player" -> NodePath).
	static Variant coerce(const Variant &p_value, Variant::Type p_type, const String &p_hint_string = String());

	static String encode(const Variant &p_value, const String &p_indent = String());
	static Variant decode(const String &p_text, String *r_error = nullptr);

	static Dictionary object_ref(Object *p_object);
	static Dictionary tagged(const String &p_type, const Variant &p_v);
	static bool is_tagged(const Variant &p_value, String *r_type = nullptr);
};
