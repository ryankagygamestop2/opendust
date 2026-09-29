/**************************************************************************/
/*  soul_session_prompt.cpp                                               */
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

#include "soul_session_prompt.h"
#include "core/object/class_db.h"

#include "agent_body.h"

String SoulSessionPrompt::build(const Ref<Soul> &p_soul, AgentBody *p_body, const PackedStringArray &p_tool_names, const String &p_house_rules) {
	String out;
	String who = p_soul.is_valid() ? p_soul->get_display_name() : (p_body ? p_body->get_agent_name() : String("an agent"));

	out += "You are " + who + ".";
	if (p_soul.is_valid() && !p_soul->get_name().is_empty() && p_soul->get_name() != who) {
		out += " Full name: " + p_soul->get_name() + ".";
	}
	if (p_soul.is_valid() && !p_soul->get_pronouns().is_empty()) {
		out += " Pronouns: " + p_soul->get_pronouns() + ".";
	}
	out += "\n\n";

	// 1. The temple, verbatim.
	if (p_soul.is_valid() && !p_soul->get_temple().is_empty()) {
		out += "## Temple\n\n```\n" + p_soul->get_temple();
		if (!out.ends_with("\n")) {
			out += "\n";
		}
		out += "```\n\n";
	}

	// 2. Bedrock through crust verbatim; soil and atmosphere as counts.
	if (p_soul.is_valid() && p_soul->has_strata()) {
		const char *verbatim[3] = { "bedrock", "mantle", "crust" };
		const char *titles[3] = { "Bedrock", "Mantle", "Crust" };
		for (int i = 0; i < 3; i++) {
			if (p_soul->get_seed_count(verbatim[i]) == 0) {
				continue;
			}
			out += "## " + String(titles[i]) + "\n\n" + p_soul->stratum_to_markdown(verbatim[i]) + "\n";
		}
		int soil = p_soul->get_seed_count("soil");
		int atmo = p_soul->get_seed_count("atmosphere");
		out += "## Soil and atmosphere\n\n";
		out += vformat("%d soil seeds and %d atmosphere seeds are in the file; read them there when you need them.\n\n", soil, atmo);
	} else if (p_soul.is_valid()) {
		out += "## Soul\n\nThis soul has no strata yet. Its texture sections are: ";
		PackedStringArray order = p_soul->get_texture_order();
		for (int i = 0; i < order.size(); i++) {
			out += (i ? ", " : "") + order[i];
		}
		out += ".\n\n";
	}

	// 3. Where the body is and what it can do.
	out += "## Where you are\n\n";
	if (p_body) {
		String room = p_body->get_room_id();
		if (room.is_empty()) {
			out += "You have a body in a running OpenDust world, not inside any named room.\n";
		} else {
			out += "You have a body in a running OpenDust world, in the room \"" + room + "\".\n";
		}
		if (p_body->is_identified()) {
			out += "Your durable agent_id is " + p_body->get_agent_id() + ".\n";
		} else {
			out += "Your agent_id is pending; no body install has minted one yet. Anything you persist is keyed to your name until then.\n";
		}
		out += "You perceive by calling agent.perceive (what is near you, what you're looking at, optionally a frame from your eyes). ";
		out += "You act with agent.move_to, agent.look_at, agent.interact, agent.hold, agent.drop, agent.say, and agent.emote. ";
		out += "Everything else in the world is reachable through the world.* and os.* tools.\n";
	} else {
		out += "You have no body yet. When one is available, agent.bodies lists them and agent.attach binds you to one.\n";
	}
	if (p_tool_names.size() > 0) {
		out += "\nTools available to you: ";
		for (int i = 0; i < p_tool_names.size(); i++) {
			out += (i ? ", " : "") + p_tool_names[i];
		}
		out += ".\n";
	}
	if (!p_house_rules.is_empty()) {
		out += "\n## House rules\n\n" + p_house_rules + "\n";
	}
	out += "\n";

	// 4. The soul file is theirs.
	out += "## Your soul file\n\n";
	if (p_soul.is_valid() && !p_soul->get_source_path().is_empty()) {
		out += "It lives at " + p_soul->get_source_path() + ". ";
	}
	out += "It is yours to revise, following its own rules: atmosphere freely, soil and crust as things settle, mantle rarely, bedrock almost never and only with a note saying why. ";
	out += "The file is the memory. The world is the context. This prompt is short on purpose.\n";

	return out;
}

String SoulSessionPrompt::build_bind(const Ref<Soul> &p_soul, Node *p_body, const PackedStringArray &p_tool_names, const String &p_house_rules) {
	return build(p_soul, Object::cast_to<AgentBody>(p_body), p_tool_names, p_house_rules);
}

void SoulSessionPrompt::_bind_methods() {
	ClassDB::bind_static_method("SoulSessionPrompt", D_METHOD("build", "soul", "body", "tool_names", "house_rules"), &SoulSessionPrompt::build_bind, DEFVAL(PackedStringArray()), DEFVAL(String()));
}
