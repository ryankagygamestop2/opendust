/**************************************************************************/
/*  opendust_node_utils.cpp                                               */
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

#include "opendust_node_utils.h"

#include "opendust_json.h"

#include "core/crypto/crypto_core.h"
#include "core/io/image.h"
#include "core/object/script_language.h"
#include "scene/main/node.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "scene/main/viewport.h"
#include "scene/resources/texture.h"

String OpenDustNodeUtils::relative_path(Node *p_root, Node *p_node) {
	if (!p_root || !p_node) {
		return String();
	}
	if (p_root == p_node) {
		return ".";
	}
	if (!p_root->is_ancestor_of(p_node)) {
		return String(p_node->get_path());
	}
	return String(p_root->get_path_to(p_node));
}

Node *OpenDustNodeUtils::resolve(Node *p_root, const String &p_path) {
	if (!p_root) {
		return nullptr;
	}
	String path = p_path.strip_edges();
	if (path.is_empty() || path == ".") {
		return p_root;
	}
	if (path.begins_with("/")) {
		// Absolute path: resolve from the tree root if we're in one.
		if (p_root->is_inside_tree()) {
			Node *n = p_root->get_tree()->get_root()->get_node_or_null(NodePath(path));
			if (n) {
				return n;
			}
		}
		// Absolute paths may also be spelled from the scene root ("/Player/Camera3D").
		path = path.substr(1);
		if (path.is_empty()) {
			return p_root;
		}
	}
	if (path.begins_with("./")) {
		path = path.substr(2);
	}
	return p_root->get_node_or_null(NodePath(path));
}

static String _script_path(Object *p_object) {
	Ref<Script> s = p_object->get_script();
	if (s.is_valid()) {
		return s->get_path();
	}
	return String();
}

Dictionary OpenDustNodeUtils::serialize_tree(Node *p_root, Node *p_node, int p_depth, bool p_include_properties) {
	Dictionary d;
	if (!p_node) {
		return d;
	}
	d["name"] = String(p_node->get_name());
	d["type"] = p_node->get_class();
	d["path"] = relative_path(p_root, p_node);
	String script = _script_path(p_node);
	if (!script.is_empty()) {
		d["script"] = script;
	}
	String scene = p_node->get_scene_file_path();
	if (!scene.is_empty() && p_node != p_root) {
		d["instance"] = scene;
	}
	if (p_include_properties) {
		d["properties"] = serialize_node(p_root, p_node, Array(), true)["properties"];
	}
	Array children;
	if (p_depth != 0) {
		for (int i = 0; i < p_node->get_child_count(); i++) {
			children.push_back(serialize_tree(p_root, p_node->get_child(i), p_depth - 1, p_include_properties));
		}
	}
	d["children"] = children;
	d["child_count"] = p_node->get_child_count();
	return d;
}

Dictionary OpenDustNodeUtils::serialize_node(Node *p_root, Node *p_node, const Array &p_property_filter, bool p_all_properties) {
	Dictionary d;
	if (!p_node) {
		return d;
	}
	d["name"] = String(p_node->get_name());
	d["type"] = p_node->get_class();
	d["path"] = relative_path(p_root, p_node);
	String script = _script_path(p_node);
	if (!script.is_empty()) {
		d["script"] = script;
	}
	String scene = p_node->get_scene_file_path();
	if (!scene.is_empty()) {
		d["instance"] = scene;
	}

	Array groups;
	List<Node::GroupInfo> ginfo;
	p_node->get_groups(&ginfo);
	for (const Node::GroupInfo &g : ginfo) {
		if (g.persistent) {
			groups.push_back(String(g.name));
		}
	}
	d["groups"] = groups;

	Array signals;
	List<MethodInfo> sigs;
	p_node->get_signal_list(&sigs);
	for (const MethodInfo &mi : sigs) {
		signals.push_back(mi.name);
	}
	d["signals"] = signals;

	Dictionary props;
	if (!p_property_filter.is_empty()) {
		for (int i = 0; i < p_property_filter.size(); i++) {
			String name = p_property_filter[i];
			bool valid = false;
			Variant v;
			if (name.contains(":")) {
				v = p_node->get_indexed(NodePath(name).get_as_property_path().get_subnames(), &valid);
			} else {
				v = p_node->get(name, &valid);
			}
			if (valid) {
				props[name] = OpenDustJSON::to_json(v);
			}
		}
	} else if (p_all_properties) {
		List<PropertyInfo> plist;
		p_node->get_property_list(&plist);
		for (const PropertyInfo &pi : plist) {
			if (!(pi.usage & PROPERTY_USAGE_EDITOR) || (pi.usage & PROPERTY_USAGE_CATEGORY) || (pi.usage & PROPERTY_USAGE_GROUP) || (pi.usage & PROPERTY_USAGE_SUBGROUP)) {
				continue;
			}
			if (pi.type == Variant::NIL) {
				continue;
			}
			bool valid = false;
			Variant v = p_node->get(pi.name, &valid);
			if (valid) {
				props[pi.name] = OpenDustJSON::to_json(v);
			}
		}
	}
	d["properties"] = props;
	return d;
}

Array OpenDustNodeUtils::list_properties(Object *p_object) {
	Array out;
	if (!p_object) {
		return out;
	}
	List<PropertyInfo> plist;
	p_object->get_property_list(&plist);
	for (const PropertyInfo &pi : plist) {
		if ((pi.usage & PROPERTY_USAGE_CATEGORY) || (pi.usage & PROPERTY_USAGE_GROUP) || (pi.usage & PROPERTY_USAGE_SUBGROUP)) {
			continue;
		}
		Dictionary d;
		d["name"] = pi.name;
		d["type"] = Variant::get_type_name(pi.type);
		d["class_name"] = String(pi.class_name);
		d["hint"] = pi.hint;
		d["hint_string"] = pi.hint_string;
		d["usage"] = pi.usage;
		d["editor"] = (pi.usage & PROPERTY_USAGE_EDITOR) != 0;
		d["storage"] = (pi.usage & PROPERTY_USAGE_STORAGE) != 0;
		out.push_back(d);
	}
	return out;
}

Variant OpenDustNodeUtils::coerce_for_property(Object *p_object, const String &p_property, const Variant &p_value) {
	if (!p_object) {
		return OpenDustJSON::from_json(p_value);
	}
	String name = p_property;
	if (name.contains(":")) {
		// Sub-property: use the current value's type as the target.
		bool valid = false;
		Variant cur = p_object->get_indexed(NodePath(name).get_as_property_path().get_subnames(), &valid);
		if (valid) {
			return OpenDustJSON::coerce(p_value, cur.get_type());
		}
		return OpenDustJSON::from_json(p_value);
	}
	List<PropertyInfo> plist;
	p_object->get_property_list(&plist);
	for (const PropertyInfo &pi : plist) {
		if (pi.name == name) {
			return OpenDustJSON::coerce(p_value, pi.type, pi.hint_string);
		}
	}
	bool valid = false;
	Variant cur = p_object->get(name, &valid);
	if (valid) {
		return OpenDustJSON::coerce(p_value, cur.get_type());
	}
	return OpenDustJSON::from_json(p_value);
}

void OpenDustNodeUtils::collect_owned(Node *p_node, Node *p_owner, List<Node *> &r_out) {
	if (!p_node) {
		return;
	}
	if (p_node->get_owner() == p_owner) {
		r_out.push_back(p_node);
	}
	for (int i = 0; i < p_node->get_child_count(); i++) {
		collect_owned(p_node->get_child(i), p_owner, r_out);
	}
}

Dictionary OpenDustNodeUtils::capture_viewport(Viewport *p_viewport, const Vector2i &p_size) {
	Dictionary d;
	ERR_FAIL_NULL_V(p_viewport, d);
	Ref<ViewportTexture> tex = p_viewport->get_texture();
	ERR_FAIL_COND_V(tex.is_null(), d);
	Ref<Image> img = tex->get_image();
	ERR_FAIL_COND_V(img.is_null() || img->is_empty(), d);
	if (p_size.x > 0 && p_size.y > 0 && (img->get_width() != p_size.x || img->get_height() != p_size.y)) {
		img->resize(p_size.x, p_size.y, Image::INTERPOLATE_BILINEAR);
	}
	if (img->get_format() != Image::FORMAT_RGBA8 && img->get_format() != Image::FORMAT_RGB8) {
		img->convert(Image::FORMAT_RGBA8);
	}
	Vector<uint8_t> png = img->save_png_to_buffer();
	ERR_FAIL_COND_V(png.is_empty(), d);

	size_t dst_len = 4 * ((size_t)png.size() / 3 + 1) + 1;
	Vector<uint8_t> buf;
	buf.resize(dst_len);
	size_t out_len = 0;
	Error err = CryptoCore::b64_encode(buf.ptrw(), dst_len, &out_len, png.ptr(), png.size());
	ERR_FAIL_COND_V(err != OK, d);
	d["png_base64"] = String::utf8((const char *)buf.ptr(), out_len);
	d["width"] = img->get_width();
	d["height"] = img->get_height();
	return d;
}
