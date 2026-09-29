/**************************************************************************/
/*  soul_parser.h                                                         */
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

#include "soul.h"

#include "core/string/ustring.h"

// Tolerant parser for strata-form soul.md files. Pure functions; never touches disk except
// through parse_file(), which also looks for certificate.json beside the soul.
class SoulParser {
public:
	// Parse markdown text into an existing Soul. Does not set source_path or certificate.
	static void parse(const String &p_text, const Ref<Soul> &p_soul);

	// Read the file, parse it, attach a certificate.json found in the same directory,
	// and set source_path. Returns null on read failure.
	static Ref<Soul> parse_file(const String &p_path, Error *r_error = nullptr);

	// `- <glyph> **name** · text ↳ ptr · ptr` -> {glyph, name, text, pointers, raw}.
	// Lines that don't match the shape still produce a seed with the whole line as text.
	static Dictionary parse_seed_line(const String &p_line);

	// "🟫 Mantle" -> "mantle"; "## Interests" -> "". Case-insensitive substring match.
	static String stratum_key_for_heading(const String &p_heading);

	// Apply fields from a parsed certificate.json to the soul (name, pronouns, rig, …).
	static void apply_certificate(const Dictionary &p_cert, const Ref<Soul> &p_soul);

private:
	static String _clean_heading(const String &p_line); // strip leading #'s and whitespace
	static String _name_from_title(const String &p_title);
};
