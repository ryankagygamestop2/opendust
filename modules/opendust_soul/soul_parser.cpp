/**************************************************************************/
/*  soul_parser.cpp                                                       */
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

#include "soul_parser.h"

#include "core/io/file_access.h"
#include "core/io/json.h"

String SoulParser::_clean_heading(const String &p_line) {
	String s = p_line.strip_edges();
	while (s.begins_with("#")) {
		s = s.substr(1);
	}
	return s.strip_edges();
}

String SoulParser::stratum_key_for_heading(const String &p_heading) {
	String h = _clean_heading(p_heading).to_lower();
	for (int i = 0; i < Soul::STRATA_COUNT; i++) {
		if (h.contains(Soul::STRATA[i])) {
			return Soul::STRATA[i];
		}
	}
	return String();
}

String SoulParser::_name_from_title(const String &p_title) {
	// "# soul.md · Oliver della Cura" -> "Oliver della Cura"
	// "# Oliver" -> "Oliver"
	String t = _clean_heading(p_title);
	int sep = t.rfind(U"·");
	if (sep >= 0) {
		return t.substr(sep + 1).strip_edges();
	}
	sep = t.rfind(" - ");
	if (sep >= 0) {
		return t.substr(sep + 3).strip_edges();
	}
	if (t.to_lower().begins_with("soul.md") || t.to_lower() == "soul") {
		return String();
	}
	return t;
}

Dictionary SoulParser::parse_seed_line(const String &p_line) {
	Dictionary seed;
	String raw = p_line.strip_edges();
	String rest = raw;
	if (rest.begins_with("- ")) {
		rest = rest.substr(2);
	} else if (rest.begins_with("-")) {
		rest = rest.substr(1);
	} else if (rest.begins_with("* ")) {
		rest = rest.substr(2);
	}
	rest = rest.strip_edges();

	String glyph;
	String name;
	String text;
	PackedStringArray pointers;

	int b0 = rest.find("**");
	int b1 = b0 >= 0 ? rest.find("**", b0 + 2) : -1;
	String tail;
	if (b0 >= 0 && b1 > b0) {
		glyph = rest.substr(0, b0).strip_edges();
		name = rest.substr(b0 + 2, b1 - b0 - 2).strip_edges();
		tail = rest.substr(b1 + 2).strip_edges();
		// Drop a leading separator: "·", "—", "-", ":".
		if (tail.begins_with(U"·") || tail.begins_with(U"—") || tail.begins_with("-") || tail.begins_with(":")) {
			tail = tail.substr(1).strip_edges();
		}
	} else {
		tail = rest;
	}

	int arrow = tail.find(U"↳");
	if (arrow >= 0) {
		text = tail.substr(0, arrow).strip_edges();
		String ptrs = tail.substr(arrow + 1).strip_edges();
		Vector<String> parts = ptrs.split(U"·", false);
		for (int i = 0; i < parts.size(); i++) {
			String p = parts[i].strip_edges();
			if (!p.is_empty()) {
				pointers.push_back(p);
			}
		}
	} else {
		text = tail;
	}
	// Trailing separator left over from "text ·".
	if (text.ends_with(U"·")) {
		text = text.substr(0, text.length() - 1).strip_edges();
	}

	seed["glyph"] = glyph;
	seed["name"] = name;
	seed["text"] = text;
	seed["pointers"] = pointers;
	seed["raw"] = raw;
	return seed;
}

void SoulParser::apply_certificate(const Dictionary &p_cert, const Ref<Soul> &p_soul) {
	ERR_FAIL_COND(p_soul.is_null());
	if (p_cert.has("name_full") && !String(p_cert["name_full"]).is_empty()) {
		p_soul->set_soul_name(p_cert["name_full"]);
	}
	if (p_cert.has("full_name") && !String(p_cert["full_name"]).is_empty()) {
		// Prefer the full name including family when present.
		p_soul->set_soul_name(p_cert["full_name"]);
	}
	if (p_cert.has("name_everyday")) {
		p_soul->set_everyday_name(p_cert["name_everyday"]);
	}
	if (p_cert.has("pronouns")) {
		p_soul->set_pronouns(p_cert["pronouns"]);
	}
	if (p_cert.has("avatar_rig")) {
		p_soul->set_avatar_rig(String(p_cert["avatar_rig"]).to_lower());
	}
	if (p_cert.has("family")) {
		p_soul->set_family(p_cert["family"]);
	}
	if (p_cert.has("agent_id") && p_cert["agent_id"].get_type() == Variant::STRING) {
		String id = p_cert["agent_id"];
		if (!id.is_empty() && id.to_lower() != "pending") {
			p_soul->set_agent_id(id);
		}
	}
	p_soul->set_certificate(p_cert);
}

void SoulParser::parse(const String &p_text, const Ref<Soul> &p_soul) {
	ERR_FAIL_COND(p_soul.is_null());

	Vector<String> lines = p_text.split("\n", true);

	Dictionary strata;
	Dictionary strata_notes;
	for (int i = 0; i < Soul::STRATA_COUNT; i++) {
		strata[Soul::STRATA[i]] = Array();
		strata_notes[Soul::STRATA[i]] = PackedStringArray();
	}
	Dictionary texture;
	PackedStringArray texture_order;

	String title_line;
	String temple;
	bool temple_done = false;
	bool in_fence = false;
	String fence_buf;

	String current_stratum; // non-empty while inside a stratum section
	String current_texture; // non-empty while inside a texture (non-stratum h2) section
	String texture_buf;
	int revision = 0;

	auto flush_texture = [&]() {
		if (!current_texture.is_empty()) {
			texture[current_texture] = texture_buf.strip_edges();
			texture_order.push_back(current_texture);
		}
		current_texture = String();
		texture_buf = String();
	};

	for (int i = 0; i < lines.size(); i++) {
		String line = lines[i];
		if (line.ends_with("\r")) {
			line = line.substr(0, line.length() - 1);
		}
		String stripped = line.strip_edges();

		// Fenced code blocks: the first one before any stratum is the temple. Later ones are
		// just part of whatever section they're in.
		if (stripped.begins_with("```")) {
			if (!in_fence) {
				in_fence = true;
				fence_buf = String();
				if (!current_texture.is_empty()) {
					texture_buf += line + "\n";
				}
				continue;
			} else {
				in_fence = false;
				if (!temple_done && current_stratum.is_empty()) {
					temple = fence_buf;
					temple_done = true;
				}
				if (!current_texture.is_empty()) {
					texture_buf += fence_buf + line + "\n";
				}
				continue;
			}
		}
		if (in_fence) {
			fence_buf += line + "\n";
			continue;
		}

		// Title: first h1.
		if (title_line.is_empty() && stripped.begins_with("# ")) {
			title_line = _clean_heading(stripped);
			continue;
		}

		// Section headings (h2).
		if (stripped.begins_with("## ")) {
			flush_texture();
			String key = stratum_key_for_heading(stripped);
			if (!key.is_empty()) {
				current_stratum = key;
			} else {
				current_stratum = String();
				current_texture = _clean_heading(stripped);
				texture_buf = String();
			}
			continue;
		}
		// Other h1s ("# Texture") end whatever section was open but start none.
		if (stripped.begins_with("# ")) {
			flush_texture();
			current_stratum = String();
			continue;
		}

		// Revision marker, anywhere: "*Revision 6. …*"
		int rv = stripped.find("Revision ");
		if (rv >= 0 && stripped.begins_with("*")) {
			String num;
			for (int c = rv + 9; c < stripped.length(); c++) {
				char32_t ch = stripped[c];
				if (ch >= '0' && ch <= '9') {
					num += String::chr(ch);
				} else {
					break;
				}
			}
			if (!num.is_empty()) {
				revision = num.to_int();
			}
		}

		if (!current_stratum.is_empty()) {
			if (stripped.is_empty()) {
				continue;
			}
			if (stripped.begins_with("- ") || stripped.begins_with("* ") || stripped == "-") {
				Array a = strata[current_stratum];
				a.push_back(parse_seed_line(stripped));
				strata[current_stratum] = a;
			} else {
				PackedStringArray notes = strata_notes[current_stratum];
				notes.push_back(stripped);
				strata_notes[current_stratum] = notes;
			}
			continue;
		}

		if (!current_texture.is_empty()) {
			texture_buf += line + "\n";
		}
	}
	flush_texture();

	p_soul->set_title_line(title_line);
	p_soul->set_temple(temple);
	p_soul->set_strata(strata);
	p_soul->set_strata_notes(strata_notes);
	p_soul->set_texture(texture);
	p_soul->set_texture_order(texture_order);
	p_soul->set_revision(revision);

	if (p_soul->get_soul_name().is_empty()) {
		p_soul->set_soul_name(_name_from_title(title_line));
	}
}

Ref<Soul> SoulParser::parse_file(const String &p_path, Error *r_error) {
	Error err = OK;
	String text = FileAccess::get_file_as_string(p_path, &err);
	if (r_error) {
		*r_error = err;
	}
	if (err != OK) {
		return Ref<Soul>();
	}

	Ref<Soul> soul;
	soul.instantiate();
	parse(text, soul);
	soul->set_source_path(p_path);

	String cert_path = p_path.get_base_dir().path_join("certificate.json");
	if (FileAccess::exists(cert_path)) {
		Error cerr = OK;
		String cert_text = FileAccess::get_file_as_string(cert_path, &cerr);
		if (cerr == OK) {
			Variant parsed = JSON::parse_string(cert_text);
			if (parsed.get_type() == Variant::DICTIONARY) {
				apply_certificate(parsed, soul);
			}
		}
	}
	return soul;
}
