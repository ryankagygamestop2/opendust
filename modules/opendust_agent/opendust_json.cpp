/**************************************************************************/
/*  opendust_json.cpp                                                     */
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

#include "opendust_json.h"

#include "core/crypto/crypto_core.h"
#include "core/io/json.h"
#include "core/io/resource.h"
#include "core/math/color.h"
#include "core/string/node_path.h"
#include "scene/main/node.h"

static String _b64(const PackedByteArray &p_bytes) {
	if (p_bytes.is_empty()) {
		return String();
	}
	size_t dst_len = 4 * ((size_t)p_bytes.size() / 3 + 1) + 1;
	Vector<uint8_t> buf;
	buf.resize(dst_len);
	size_t out_len = 0;
	Error err = CryptoCore::b64_encode(buf.ptrw(), dst_len, &out_len, p_bytes.ptr(), p_bytes.size());
	ERR_FAIL_COND_V(err != OK, String());
	buf.write[out_len] = 0;
	return String::utf8((const char *)buf.ptr(), out_len);
}

static PackedByteArray _unb64(const String &p_text) {
	CharString cs = p_text.utf8();
	if (cs.length() == 0) {
		return PackedByteArray();
	}
	Vector<uint8_t> buf;
	buf.resize(cs.length());
	size_t out_len = 0;
	Error err = CryptoCore::b64_decode(buf.ptrw(), buf.size(), &out_len, (const uint8_t *)cs.get_data(), cs.length());
	ERR_FAIL_COND_V(err != OK, PackedByteArray());
	buf.resize(out_len);
	return buf;
}

Dictionary OpenDustJSON::tagged(const String &p_type, const Variant &p_v) {
	Dictionary d;
	d["$type"] = p_type;
	d["v"] = p_v;
	return d;
}

bool OpenDustJSON::is_tagged(const Variant &p_value, String *r_type) {
	if (p_value.get_type() != Variant::DICTIONARY) {
		return false;
	}
	Dictionary d = p_value;
	if (!d.has("$type")) {
		return false;
	}
	if (r_type) {
		*r_type = String(d["$type"]);
	}
	return true;
}

Dictionary OpenDustJSON::object_ref(Object *p_object) {
	Dictionary d;
	d["$type"] = "Object";
	if (!p_object) {
		d["class"] = "";
		d["path"] = "";
		d["id"] = 0;
		return d;
	}
	d["class"] = p_object->get_class();
	d["id"] = (int64_t)p_object->get_instance_id();
	Node *n = Object::cast_to<Node>(p_object);
	if (n && n->is_inside_tree()) {
		d["path"] = String(n->get_path());
	} else {
		Resource *r = Object::cast_to<Resource>(p_object);
		d["path"] = r ? r->get_path() : String();
	}
	return d;
}

static Array _v2(const Vector2 &v) {
	Array a;
	a.push_back(v.x);
	a.push_back(v.y);
	return a;
}
static Array _v2i(const Vector2i &v) {
	Array a;
	a.push_back(v.x);
	a.push_back(v.y);
	return a;
}
static Array _v3(const Vector3 &v) {
	Array a;
	a.push_back(v.x);
	a.push_back(v.y);
	a.push_back(v.z);
	return a;
}
static Array _v3i(const Vector3i &v) {
	Array a;
	a.push_back(v.x);
	a.push_back(v.y);
	a.push_back(v.z);
	return a;
}
static Array _v4(const Vector4 &v) {
	Array a;
	a.push_back(v.x);
	a.push_back(v.y);
	a.push_back(v.z);
	a.push_back(v.w);
	return a;
}
static Array _v4i(const Vector4i &v) {
	Array a;
	a.push_back(v.x);
	a.push_back(v.y);
	a.push_back(v.z);
	a.push_back(v.w);
	return a;
}

Variant OpenDustJSON::to_json(const Variant &p_value, int p_depth) {
	if (p_depth > MAX_DEPTH) {
		return Variant("<max depth>");
	}
	switch (p_value.get_type()) {
		case Variant::NIL:
		case Variant::BOOL:
		case Variant::INT:
		case Variant::FLOAT:
		case Variant::STRING:
			return p_value;
		case Variant::STRING_NAME:
			return String(p_value);
		case Variant::VECTOR2:
			return tagged("Vector2", _v2(p_value));
		case Variant::VECTOR2I:
			return tagged("Vector2i", _v2i(p_value));
		case Variant::VECTOR3:
			return tagged("Vector3", _v3(p_value));
		case Variant::VECTOR3I:
			return tagged("Vector3i", _v3i(p_value));
		case Variant::VECTOR4:
			return tagged("Vector4", _v4(p_value));
		case Variant::VECTOR4I:
			return tagged("Vector4i", _v4i(p_value));
		case Variant::RECT2: {
			Rect2 r = p_value;
			Array a;
			a.push_back(r.position.x);
			a.push_back(r.position.y);
			a.push_back(r.size.x);
			a.push_back(r.size.y);
			return tagged("Rect2", a);
		}
		case Variant::RECT2I: {
			Rect2i r = p_value;
			Array a;
			a.push_back(r.position.x);
			a.push_back(r.position.y);
			a.push_back(r.size.x);
			a.push_back(r.size.y);
			return tagged("Rect2i", a);
		}
		case Variant::TRANSFORM2D: {
			Transform2D t = p_value;
			Array a;
			a.push_back(_v2(t.columns[0]));
			a.push_back(_v2(t.columns[1]));
			a.push_back(_v2(t.columns[2]));
			return tagged("Transform2D", a);
		}
		case Variant::PLANE: {
			Plane p = p_value;
			Array a;
			a.push_back(p.normal.x);
			a.push_back(p.normal.y);
			a.push_back(p.normal.z);
			a.push_back(p.d);
			return tagged("Plane", a);
		}
		case Variant::QUATERNION: {
			Quaternion q = p_value;
			Array a;
			a.push_back(q.x);
			a.push_back(q.y);
			a.push_back(q.z);
			a.push_back(q.w);
			return tagged("Quaternion", a);
		}
		case Variant::AABB: {
			AABB b = p_value;
			Array a;
			a.push_back(_v3(b.position));
			a.push_back(_v3(b.size));
			return tagged("AABB", a);
		}
		case Variant::BASIS: {
			Basis b = p_value;
			Array a;
			a.push_back(_v3(b.get_column(0)));
			a.push_back(_v3(b.get_column(1)));
			a.push_back(_v3(b.get_column(2)));
			return tagged("Basis", a);
		}
		case Variant::TRANSFORM3D: {
			Transform3D t = p_value;
			Array a;
			a.push_back(_v3(t.basis.get_column(0)));
			a.push_back(_v3(t.basis.get_column(1)));
			a.push_back(_v3(t.basis.get_column(2)));
			a.push_back(_v3(t.origin));
			return tagged("Transform3D", a);
		}
		case Variant::PROJECTION: {
			Projection p = p_value;
			Array a;
			for (int i = 0; i < 4; i++) {
				a.push_back(_v4(p[i]));
			}
			return tagged("Projection", a);
		}
		case Variant::COLOR: {
			Color c = p_value;
			Array a;
			a.push_back(c.r);
			a.push_back(c.g);
			a.push_back(c.b);
			a.push_back(c.a);
			return tagged("Color", a);
		}
		case Variant::NODE_PATH:
			return tagged("NodePath", String(p_value));
		case Variant::RID:
			return tagged("RID", (int64_t)RID(p_value).get_id());
		case Variant::OBJECT:
			return object_ref(p_value.get_validated_object());
		case Variant::CALLABLE:
			return tagged("Callable", p_value.operator String());
		case Variant::SIGNAL:
			return tagged("Signal", p_value.operator String());
		case Variant::DICTIONARY: {
			Dictionary in = p_value;
			Dictionary out;
			for (const KeyValue<Variant, Variant> &kv : in) {
				out[String(kv.key)] = to_json(kv.value, p_depth + 1);
			}
			return out;
		}
		case Variant::ARRAY: {
			Array in = p_value;
			Array out;
			for (int i = 0; i < in.size(); i++) {
				out.push_back(to_json(in[i], p_depth + 1));
			}
			return out;
		}
		case Variant::PACKED_BYTE_ARRAY:
			return tagged("PackedByteArray", _b64(p_value));
		case Variant::PACKED_INT32_ARRAY:
		case Variant::PACKED_INT64_ARRAY:
		case Variant::PACKED_FLOAT32_ARRAY:
		case Variant::PACKED_FLOAT64_ARRAY:
		case Variant::PACKED_STRING_ARRAY: {
			Array plain = Array(p_value);
			return tagged(Variant::get_type_name(p_value.get_type()), plain);
		}
		case Variant::PACKED_VECTOR2_ARRAY:
		case Variant::PACKED_VECTOR3_ARRAY:
		case Variant::PACKED_VECTOR4_ARRAY:
		case Variant::PACKED_COLOR_ARRAY: {
			Array plain = Array(p_value);
			Array out;
			for (int i = 0; i < plain.size(); i++) {
				Variant t = to_json(plain[i], p_depth + 1);
				out.push_back(Dictionary(t)["v"]);
			}
			return tagged(Variant::get_type_name(p_value.get_type()), out);
		}
		default:
			return p_value.operator String();
	}
}

static real_t _num(const Array &a, int i) {
	return i < a.size() ? (real_t)(double)a[i] : 0;
}
static Vector2 _av2(const Array &a) {
	return Vector2(_num(a, 0), _num(a, 1));
}
static Vector3 _av3(const Array &a) {
	return Vector3(_num(a, 0), _num(a, 1), _num(a, 2));
}
static Vector4 _av4(const Array &a) {
	return Vector4(_num(a, 0), _num(a, 1), _num(a, 2), _num(a, 3));
}
static Vector2i _av2i(const Array &a) {
	return Vector2i((int32_t)_num(a, 0), (int32_t)_num(a, 1));
}
static Vector3i _av3i(const Array &a) {
	return Vector3i((int32_t)_num(a, 0), (int32_t)_num(a, 1), (int32_t)_num(a, 2));
}
static Vector4i _av4i(const Array &a) {
	return Vector4i((int32_t)_num(a, 0), (int32_t)_num(a, 1), (int32_t)_num(a, 2), (int32_t)_num(a, 3));
}

Variant OpenDustJSON::from_json(const Variant &p_json, int p_depth) {
	if (p_depth > MAX_DEPTH) {
		return Variant();
	}
	if (p_json.get_type() == Variant::ARRAY) {
		Array in = p_json;
		Array out;
		for (int i = 0; i < in.size(); i++) {
			out.push_back(from_json(in[i], p_depth + 1));
		}
		return out;
	}
	if (p_json.get_type() != Variant::DICTIONARY) {
		return p_json;
	}
	Dictionary d = p_json;
	if (!d.has("$type")) {
		Dictionary out;
		for (const KeyValue<Variant, Variant> &kv : d) {
			out[kv.key] = from_json(kv.value, p_depth + 1);
		}
		return out;
	}

	String type = d["$type"];
	Variant v = d.get("v", Variant());
	Array a = v.get_type() == Variant::ARRAY ? Array(v) : Array();

	if (type == "Vector2") {
		return _av2(a);
	} else if (type == "Vector2i") {
		return _av2i(a);
	} else if (type == "Vector3") {
		return _av3(a);
	} else if (type == "Vector3i") {
		return _av3i(a);
	} else if (type == "Vector4") {
		return _av4(a);
	} else if (type == "Vector4i") {
		return _av4i(a);
	} else if (type == "Rect2") {
		return Rect2(_num(a, 0), _num(a, 1), _num(a, 2), _num(a, 3));
	} else if (type == "Rect2i") {
		return Rect2i(_num(a, 0), _num(a, 1), _num(a, 2), _num(a, 3));
	} else if (type == "Transform2D") {
		if (a.size() >= 3) {
			return Transform2D(_av2(a[0]), _av2(a[1]), _av2(a[2]));
		}
		return Transform2D();
	} else if (type == "Plane") {
		return Plane(_num(a, 0), _num(a, 1), _num(a, 2), _num(a, 3));
	} else if (type == "Quaternion") {
		return Quaternion(_num(a, 0), _num(a, 1), _num(a, 2), _num(a, 3));
	} else if (type == "AABB") {
		if (a.size() >= 2) {
			return AABB(_av3(a[0]), _av3(a[1]));
		}
		return AABB();
	} else if (type == "Basis") {
		if (a.size() >= 3) {
			return Basis(_av3(a[0]), _av3(a[1]), _av3(a[2]));
		}
		return Basis();
	} else if (type == "Transform3D") {
		if (a.size() >= 4) {
			return Transform3D(Basis(_av3(a[0]), _av3(a[1]), _av3(a[2])), _av3(a[3]));
		}
		return Transform3D();
	} else if (type == "Projection") {
		if (a.size() >= 4) {
			return Projection(_av4(a[0]), _av4(a[1]), _av4(a[2]), _av4(a[3]));
		}
		return Projection();
	} else if (type == "Color") {
		if (v.get_type() == Variant::STRING) {
			return coerce(v, Variant::COLOR);
		}
		return Color(_num(a, 0), _num(a, 1), _num(a, 2), a.size() > 3 ? _num(a, 3) : 1.0);
	} else if (type == "NodePath") {
		return NodePath(String(v));
	} else if (type == "StringName") {
		return StringName(String(v));
	} else if (type == "RID") {
		return RID(); // RIDs cannot be reconstructed from the wire; callers must use paths.
	} else if (type == "Object") {
		int64_t id = d.get("id", 0);
		if (id != 0) {
			Object *o = ObjectDB::get_instance(ObjectID((uint64_t)id));
			if (o) {
				return o;
			}
		}
		return Variant();
	} else if (type == "PackedByteArray") {
		return _unb64(String(v));
	} else if (type == "PackedInt32Array") {
		return Variant(a).operator PackedInt32Array();
	} else if (type == "PackedInt64Array") {
		return Variant(a).operator PackedInt64Array();
	} else if (type == "PackedFloat32Array") {
		return Variant(a).operator PackedFloat32Array();
	} else if (type == "PackedFloat64Array") {
		return Variant(a).operator PackedFloat64Array();
	} else if (type == "PackedStringArray") {
		return Variant(a).operator PackedStringArray();
	} else if (type == "PackedVector2Array") {
		PackedVector2Array out;
		for (int i = 0; i < a.size(); i++) {
			out.push_back(_av2(a[i]));
		}
		return out;
	} else if (type == "PackedVector3Array") {
		PackedVector3Array out;
		for (int i = 0; i < a.size(); i++) {
			out.push_back(_av3(a[i]));
		}
		return out;
	} else if (type == "PackedVector4Array") {
		PackedVector4Array out;
		for (int i = 0; i < a.size(); i++) {
			out.push_back(_av4(a[i]));
		}
		return out;
	} else if (type == "PackedColorArray") {
		PackedColorArray out;
		for (int i = 0; i < a.size(); i++) {
			Array c = a[i];
			out.push_back(Color(_num(c, 0), _num(c, 1), _num(c, 2), c.size() > 3 ? _num(c, 3) : 1.0));
		}
		return out;
	}
	// Unknown tag: hand back the payload untouched.
	return v;
}

Variant OpenDustJSON::coerce(const Variant &p_value, Variant::Type p_type, const String &p_hint_string) {
	Variant value = from_json(p_value);
	if (value.get_type() == p_type || p_type == Variant::NIL) {
		return value;
	}
	bool is_array = value.get_type() == Variant::ARRAY;
	Array a = is_array ? Array(value) : Array();
	bool is_string = value.get_type() == Variant::STRING;
	String s = is_string ? String(value) : String();

	switch (p_type) {
		case Variant::VECTOR2:
			return is_array ? Variant(_av2(a)) : value;
		case Variant::VECTOR2I:
			return is_array ? Variant(_av2i(a)) : value;
		case Variant::VECTOR3:
			return is_array ? Variant(_av3(a)) : value;
		case Variant::VECTOR3I:
			return is_array ? Variant(_av3i(a)) : value;
		case Variant::VECTOR4:
			return is_array ? Variant(_av4(a)) : value;
		case Variant::VECTOR4I:
			return is_array ? Variant(_av4i(a)) : value;
		case Variant::QUATERNION:
			return is_array ? Variant(Quaternion(_num(a, 0), _num(a, 1), _num(a, 2), _num(a, 3))) : value;
		case Variant::RECT2:
			return is_array ? Variant(Rect2(_num(a, 0), _num(a, 1), _num(a, 2), _num(a, 3))) : value;
		case Variant::COLOR: {
			if (is_array) {
				return Color(_num(a, 0), _num(a, 1), _num(a, 2), a.size() > 3 ? _num(a, 3) : 1.0);
			}
			if (is_string) {
				if (Color::html_is_valid(s)) {
					return Color::html(s);
				}
				return Color::named(s, Color());
			}
			return value;
		}
		case Variant::NODE_PATH:
			return is_string ? Variant(NodePath(s)) : value;
		case Variant::STRING_NAME:
			return is_string ? Variant(StringName(s)) : value;
		case Variant::STRING:
			return value.operator String();
		case Variant::INT:
			if (value.get_type() == Variant::FLOAT) {
				return (int64_t)(double)value;
			}
			if (is_string && s.is_valid_int()) {
				return s.to_int();
			}
			return value;
		case Variant::FLOAT:
			if (value.get_type() == Variant::INT) {
				return (double)(int64_t)value;
			}
			if (is_string && s.is_valid_float()) {
				return s.to_float();
			}
			return value;
		case Variant::BOOL:
			if (value.get_type() == Variant::INT) {
				return (int64_t)value != 0;
			}
			return value;
		case Variant::PACKED_STRING_ARRAY:
			return is_array ? Variant(a).operator PackedStringArray() : value;
		case Variant::PACKED_INT32_ARRAY:
			return is_array ? Variant(a).operator PackedInt32Array() : value;
		case Variant::PACKED_FLOAT32_ARRAY:
			return is_array ? Variant(a).operator PackedFloat32Array() : value;
		default:
			return value;
	}
}

String OpenDustJSON::encode(const Variant &p_value, const String &p_indent) {
	return JSON::stringify(to_json(p_value), p_indent, false);
}

Variant OpenDustJSON::decode(const String &p_text, String *r_error) {
	JSON json;
	Error err = json.parse(p_text);
	if (err != OK) {
		if (r_error) {
			*r_error = vformat("%s (line %d)", json.get_error_message(), json.get_error_line());
		}
		return Variant();
	}
	return json.get_data();
}
