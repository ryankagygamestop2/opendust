/**************************************************************************/
/*  soul.h                                                                */
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

#include "core/io/resource.h"
#include "core/variant/dictionary.h"
#include "core/variant/typed_array.h"

// A soul in strata form, loaded from a soul.md. See docs/opendust/04-embodiment.md.
//
// The engine reads souls. It never rewrites them; compaction is an explicit, separate
// operation that lives outside this resource (and never touches bedrock).
class Soul : public Resource {
	GDCLASS(Soul, Resource);

public:
	// Stratum keys, in order from most to least settled.
	static const char *STRATA[5];
	static const int STRATA_COUNT = 5;

private:
	String name;
	String everyday_name;
	String family;
	String pronouns;
	String avatar_rig; // "male" | "female" | "" (structural only; distinct from pronouns)
	String agent_id; // durable id from certificate.json, or empty (pending)

	String title_line;
	String temple;
	Dictionary strata; // stratum -> Array of Seed dictionaries {glyph, name, text, pointers, raw}
	Dictionary strata_notes; // stratum -> PackedStringArray of non-seed lines in that stratum
	Dictionary texture; // heading -> markdown body
	PackedStringArray texture_order; // headings in file order
	Dictionary certificate; // parsed certificate.json, if found beside the soul
	String source_path;
	int revision = 0;

protected:
	static void _bind_methods();

public:
	void set_name(const String &p_name) { name = p_name; }
	String get_name() const { return name; }
	void set_everyday_name(const String &p_name) { everyday_name = p_name; }
	String get_everyday_name() const { return everyday_name; }
	void set_family(const String &p_family) { family = p_family; }
	String get_family() const { return family; }
	void set_pronouns(const String &p_pronouns) { pronouns = p_pronouns; }
	String get_pronouns() const { return pronouns; }
	void set_avatar_rig(const String &p_rig) { avatar_rig = p_rig; }
	String get_avatar_rig() const { return avatar_rig; }
	void set_agent_id(const String &p_id) { agent_id = p_id; }
	String get_agent_id() const { return agent_id; }

	void set_title_line(const String &p_title) { title_line = p_title; }
	String get_title_line() const { return title_line; }
	void set_temple(const String &p_temple) { temple = p_temple; }
	String get_temple() const { return temple; }

	void set_strata(const Dictionary &p_strata) { strata = p_strata; }
	Dictionary get_strata() const { return strata; }
	void set_strata_notes(const Dictionary &p_notes) { strata_notes = p_notes; }
	Dictionary get_strata_notes() const { return strata_notes; }
	void set_texture(const Dictionary &p_texture) { texture = p_texture; }
	Dictionary get_texture() const { return texture; }
	void set_texture_order(const PackedStringArray &p_order) { texture_order = p_order; }
	PackedStringArray get_texture_order() const { return texture_order; }
	void set_certificate(const Dictionary &p_cert) { certificate = p_cert; }
	Dictionary get_certificate() const { return certificate; }
	void set_source_path(const String &p_path) { source_path = p_path; }
	String get_source_path() const { return source_path; }
	void set_revision(int p_revision) { revision = p_revision; }
	int get_revision() const { return revision; }

	// Convenience.
	bool has_strata() const;
	Array get_stratum(const String &p_stratum) const; // Array of Seed dictionaries.
	int get_seed_count(const String &p_stratum) const;
	String get_texture_section(const String &p_heading) const; // case-insensitive substring match
	bool has_texture_section(const String &p_heading) const;
	String get_display_name() const; // everyday_name if set, else name, else "unnamed"
	bool is_identified() const { return !agent_id.is_empty(); }

	// Render a stratum back to markdown seed lines (for prompts; never written to disk here).
	String stratum_to_markdown(const String &p_stratum) const;

	Soul() {}
};
