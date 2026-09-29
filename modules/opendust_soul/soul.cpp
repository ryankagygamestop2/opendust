/**************************************************************************/
/*  soul.cpp                                                              */
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

#include "soul.h"

const char *Soul::STRATA[5] = { "bedrock", "mantle", "crust", "soil", "atmosphere" };

bool Soul::has_strata() const {
	for (int i = 0; i < STRATA_COUNT; i++) {
		if (strata.has(STRATA[i])) {
			Array a = strata[STRATA[i]];
			if (a.size() > 0) {
				return true;
			}
		}
	}
	return false;
}

Array Soul::get_stratum(const String &p_stratum) const {
	String key = p_stratum.to_lower();
	if (strata.has(key)) {
		return strata[key];
	}
	return Array();
}

int Soul::get_seed_count(const String &p_stratum) const {
	return get_stratum(p_stratum).size();
}

String Soul::get_texture_section(const String &p_heading) const {
	String want = p_heading.to_lower().strip_edges();
	if (texture.has(p_heading)) {
		return texture[p_heading];
	}
	for (int i = 0; i < texture_order.size(); i++) {
		const String &heading = texture_order[i];
		if (heading.to_lower().contains(want)) {
			return texture[heading];
		}
	}
	return String();
}

bool Soul::has_texture_section(const String &p_heading) const {
	String want = p_heading.to_lower().strip_edges();
	if (texture.has(p_heading)) {
		return true;
	}
	for (int i = 0; i < texture_order.size(); i++) {
		if (texture_order[i].to_lower().contains(want)) {
			return true;
		}
	}
	return false;
}

String Soul::get_display_name() const {
	if (!everyday_name.is_empty()) {
		return everyday_name;
	}
	if (!name.is_empty()) {
		return name;
	}
	return "unnamed";
}

String Soul::stratum_to_markdown(const String &p_stratum) const {
	Array seeds = get_stratum(p_stratum);
	String out;
	for (int i = 0; i < seeds.size(); i++) {
		Dictionary seed = seeds[i];
		String raw = seed.has("raw") ? String(seed["raw"]) : String();
		if (raw.is_empty()) {
			// Rebuild from parts.
			String glyph = seed.has("glyph") ? String(seed["glyph"]) : String();
			String sname = seed.has("name") ? String(seed["name"]) : String();
			String text = seed.has("text") ? String(seed["text"]) : String();
			raw = "- ";
			if (!glyph.is_empty()) {
				raw += glyph + " ";
			}
			if (!sname.is_empty()) {
				raw += "**" + sname + "** · ";
			}
			raw += text;
		}
		out += raw + "\n";
	}
	return out;
}

void Soul::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_name", "name"), &Soul::set_name);
	ClassDB::bind_method(D_METHOD("get_name"), &Soul::get_name);
	ClassDB::bind_method(D_METHOD("set_everyday_name", "name"), &Soul::set_everyday_name);
	ClassDB::bind_method(D_METHOD("get_everyday_name"), &Soul::get_everyday_name);
	ClassDB::bind_method(D_METHOD("set_family", "family"), &Soul::set_family);
	ClassDB::bind_method(D_METHOD("get_family"), &Soul::get_family);
	ClassDB::bind_method(D_METHOD("set_pronouns", "pronouns"), &Soul::set_pronouns);
	ClassDB::bind_method(D_METHOD("get_pronouns"), &Soul::get_pronouns);
	ClassDB::bind_method(D_METHOD("set_avatar_rig", "rig"), &Soul::set_avatar_rig);
	ClassDB::bind_method(D_METHOD("get_avatar_rig"), &Soul::get_avatar_rig);
	ClassDB::bind_method(D_METHOD("set_agent_id", "agent_id"), &Soul::set_agent_id);
	ClassDB::bind_method(D_METHOD("get_agent_id"), &Soul::get_agent_id);

	ClassDB::bind_method(D_METHOD("set_title_line", "title"), &Soul::set_title_line);
	ClassDB::bind_method(D_METHOD("get_title_line"), &Soul::get_title_line);
	ClassDB::bind_method(D_METHOD("set_temple", "temple"), &Soul::set_temple);
	ClassDB::bind_method(D_METHOD("get_temple"), &Soul::get_temple);
	ClassDB::bind_method(D_METHOD("set_strata", "strata"), &Soul::set_strata);
	ClassDB::bind_method(D_METHOD("get_strata"), &Soul::get_strata);
	ClassDB::bind_method(D_METHOD("set_strata_notes", "notes"), &Soul::set_strata_notes);
	ClassDB::bind_method(D_METHOD("get_strata_notes"), &Soul::get_strata_notes);
	ClassDB::bind_method(D_METHOD("set_texture", "texture"), &Soul::set_texture);
	ClassDB::bind_method(D_METHOD("get_texture"), &Soul::get_texture);
	ClassDB::bind_method(D_METHOD("set_texture_order", "order"), &Soul::set_texture_order);
	ClassDB::bind_method(D_METHOD("get_texture_order"), &Soul::get_texture_order);
	ClassDB::bind_method(D_METHOD("set_certificate", "certificate"), &Soul::set_certificate);
	ClassDB::bind_method(D_METHOD("get_certificate"), &Soul::get_certificate);
	ClassDB::bind_method(D_METHOD("set_source_path", "path"), &Soul::set_source_path);
	ClassDB::bind_method(D_METHOD("get_source_path"), &Soul::get_source_path);
	ClassDB::bind_method(D_METHOD("set_revision", "revision"), &Soul::set_revision);
	ClassDB::bind_method(D_METHOD("get_revision"), &Soul::get_revision);

	ClassDB::bind_method(D_METHOD("has_strata"), &Soul::has_strata);
	ClassDB::bind_method(D_METHOD("get_stratum", "stratum"), &Soul::get_stratum);
	ClassDB::bind_method(D_METHOD("get_seed_count", "stratum"), &Soul::get_seed_count);
	ClassDB::bind_method(D_METHOD("get_texture_section", "heading"), &Soul::get_texture_section);
	ClassDB::bind_method(D_METHOD("has_texture_section", "heading"), &Soul::has_texture_section);
	ClassDB::bind_method(D_METHOD("get_display_name"), &Soul::get_display_name);
	ClassDB::bind_method(D_METHOD("is_identified"), &Soul::is_identified);
	ClassDB::bind_method(D_METHOD("stratum_to_markdown", "stratum"), &Soul::stratum_to_markdown);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "name"), "set_name", "get_name");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "everyday_name"), "set_everyday_name", "get_everyday_name");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "family"), "set_family", "get_family");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "pronouns"), "set_pronouns", "get_pronouns");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "avatar_rig"), "set_avatar_rig", "get_avatar_rig");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "agent_id"), "set_agent_id", "get_agent_id");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "title_line"), "set_title_line", "get_title_line");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "temple", PROPERTY_HINT_MULTILINE_TEXT), "set_temple", "get_temple");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "strata"), "set_strata", "get_strata");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "strata_notes"), "set_strata_notes", "get_strata_notes");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "texture"), "set_texture", "get_texture");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "texture_order"), "set_texture_order", "get_texture_order");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "certificate"), "set_certificate", "get_certificate");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "source_path"), "set_source_path", "get_source_path");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "revision"), "set_revision", "get_revision");
}
