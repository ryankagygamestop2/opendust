/**************************************************************************/
/*  soul_session_prompt.h                                                 */
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

#include "core/object/ref_counted.h"

class AgentBody;

// Builds the short system prompt a soul-session runs with. See 04-embodiment.md,
// "What a soul-session sees". Static helpers; also exposed as a script-callable singleton-less
// class so an addon can build prompts.
class SoulSessionPrompt : public RefCounted {
	GDCLASS(SoulSessionPrompt, RefCounted);

protected:
	static void _bind_methods();

public:
	// p_body may be null (a session with no body yet). p_tool_names is the bridge tool list the
	// session will have; p_house_rules is free text from the project (may be empty).
	static String build(const Ref<Soul> &p_soul, AgentBody *p_body, const PackedStringArray &p_tool_names = PackedStringArray(), const String &p_house_rules = String());

	static String build_bind(const Ref<Soul> &p_soul, Node *p_body, const PackedStringArray &p_tool_names, const String &p_house_rules);
};
