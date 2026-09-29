/**************************************************************************/
/*  opendust_agents_dock.cpp                                              */
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

#include "opendust_agents_dock.h"
#include "core/object/callable_mp.h"

#include "../opendust_agent_server.h"

#include "core/io/json.h"
#include "core/os/os.h"
#include "scene/gui/button.h"
#include "scene/gui/item_list.h"
#include "scene/gui/label.h"
#include "scene/gui/rich_text_label.h"
#include "scene/gui/separator.h"
#include "servers/display/display_server.h"
#include "servers/text/text_server.h"

void OpenDustAgentsDock::_append_log(const String &p_bbcode) {
	if (!activity_log) {
		return;
	}
	activity_log->append_text(p_bbcode + "\n");
	log_lines++;
	if (log_lines > MAX_LOG_LINES) {
		activity_log->clear();
		log_lines = 0;
		activity_log->append_text("[i](log trimmed)[/i]\n");
	}
}

void OpenDustAgentsDock::log_line(const String &p_text) {
	_append_log(p_text);
}

void OpenDustAgentsDock::_refresh_status() {
	if (!status_label) {
		return;
	}
	if (!server) {
		status_label->set_text("Bridge: no server");
		discovery_label->set_text("");
		return;
	}
	if (server->is_running()) {
		status_label->set_text(vformat(U"Bridge: ws://127.0.0.1:%d  ·  %d agent(s)", server->get_port(), server->get_session_count()));
		discovery_label->set_text(server->get_discovery_path());
	} else {
		status_label->set_text("Bridge: stopped");
		discovery_label->set_text("");
	}
}

void OpenDustAgentsDock::_refresh_agents() {
	if (!agents_list) {
		return;
	}
	agents_list->clear();
	if (!server) {
		return;
	}
	Array sessions = server->get_sessions();
	for (int i = 0; i < sessions.size(); i++) {
		Dictionary s = sessions[i];
		String agent_id = s.get("agent_id", "");
		String label = vformat(U"%s  ·  %s  ·  %s  ·  %d calls", String(s.get("agent_name", "agent")), agent_id.is_empty() ? String("pending") : agent_id, String(s.get("session_id", "")), (int)s.get("tool_calls", 0));
		String last = s.get("last_tool", "");
		if (!last.is_empty()) {
			label += String(U"  ·  last: ") + last;
		}
		int idx = agents_list->add_item(label);
		agents_list->set_item_metadata(idx, s.get("session_id", ""));
	}
	_refresh_status();
}

void OpenDustAgentsDock::_on_started(int p_port) {
	_append_log(vformat("[color=#8f8]bridge started[/color] on port %d", p_port));
	_refresh_status();
}

void OpenDustAgentsDock::_on_stopped() {
	_append_log("[color=#f88]bridge stopped[/color]");
	_refresh_agents();
}

void OpenDustAgentsDock::_on_session_connected(const Dictionary &p_session) {
	_append_log(vformat("[b]%s[/b] connected (%s)", String(p_session.get("agent_name", "agent")), String(p_session.get("session_id", ""))));
	_refresh_agents();
}

void OpenDustAgentsDock::_on_session_disconnected(const String &p_session_id) {
	_append_log(vformat("session %s disconnected", p_session_id));
	_refresh_agents();
}

void OpenDustAgentsDock::_on_tool_called(const String &p_session_id, const String &p_tool, const Dictionary &p_params, bool p_ok) {
	String params = JSON::stringify(p_params);
	if (params.length() > 160) {
		params = params.substr(0, 157) + "...";
	}
	String who = p_session_id;
	if (server) {
		Array sessions = server->get_sessions();
		for (int i = 0; i < sessions.size(); i++) {
			Dictionary s = sessions[i];
			if (String(s.get("session_id", "")) == p_session_id) {
				who = s.get("agent_name", p_session_id);
				break;
			}
		}
	}
	_append_log(vformat("%s[b]%s[/b] %s [code]%s[/code]%s", p_ok ? "" : "[color=#f88]", who, p_tool, params, p_ok ? "" : " (failed)[/color]"));
	_refresh_agents();
}

void OpenDustAgentsDock::_on_spawn_pressed() {
	emit_signal(SNAME("spawn_from_soul_requested"));
	_append_log("[i]spawn from soul requested (needs the opendust_soul module)[/i]");
}

void OpenDustAgentsDock::_on_copy_discovery_pressed() {
	if (server && server->is_running()) {
		DisplayServer::get_singleton()->clipboard_set(server->get_discovery_path());
		_append_log("[i]discovery path copied to clipboard[/i]");
	}
}

void OpenDustAgentsDock::_on_restart_pressed() {
	if (!server) {
		return;
	}
	OpenDustAgentServer::Mode mode = server->get_mode();
	server->stop();
	server->start(mode);
	_refresh_agents();
}

void OpenDustAgentsDock::_on_clear_log_pressed() {
	if (activity_log) {
		activity_log->clear();
		log_lines = 0;
	}
}

void OpenDustAgentsDock::set_server(OpenDustAgentServer *p_server) {
	if (server) {
		server->disconnect("started", callable_mp(this, &OpenDustAgentsDock::_on_started));
		server->disconnect("stopped", callable_mp(this, &OpenDustAgentsDock::_on_stopped));
		server->disconnect("session_connected", callable_mp(this, &OpenDustAgentsDock::_on_session_connected));
		server->disconnect("session_disconnected", callable_mp(this, &OpenDustAgentsDock::_on_session_disconnected));
		server->disconnect("tool_called", callable_mp(this, &OpenDustAgentsDock::_on_tool_called));
	}
	server = p_server;
	if (server) {
		server->connect("started", callable_mp(this, &OpenDustAgentsDock::_on_started));
		server->connect("stopped", callable_mp(this, &OpenDustAgentsDock::_on_stopped));
		server->connect("session_connected", callable_mp(this, &OpenDustAgentsDock::_on_session_connected));
		server->connect("session_disconnected", callable_mp(this, &OpenDustAgentsDock::_on_session_disconnected));
		server->connect("tool_called", callable_mp(this, &OpenDustAgentsDock::_on_tool_called));
	}
	_refresh_agents();
}

void OpenDustAgentsDock::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			_refresh_agents();
		} break;
	}
}

void OpenDustAgentsDock::_bind_methods() {
	ADD_SIGNAL(MethodInfo("spawn_from_soul_requested"));
}

OpenDustAgentsDock::OpenDustAgentsDock() {
	set_v_size_flags(SIZE_EXPAND_FILL);

	status_label = memnew(Label);
	status_label->set_text(U"Bridge: starting…");
	add_child(status_label);

	discovery_label = memnew(Label);
	discovery_label->set_text("");
	discovery_label->set_autowrap_mode(TextServer::AUTOWRAP_ARBITRARY);
	discovery_label->add_theme_font_size_override("font_size", 11);
	add_child(discovery_label);

	HBoxContainer *buttons = memnew(HBoxContainer);
	add_child(buttons);

	spawn_button = memnew(Button);
	spawn_button->set_text(U"Spawn from soul…");
	spawn_button->set_tooltip_text("Instantiate an agent body from a soul.md (requires the opendust_soul module).");
	spawn_button->connect("pressed", callable_mp(this, &OpenDustAgentsDock::_on_spawn_pressed));
	buttons->add_child(spawn_button);

	copy_discovery_button = memnew(Button);
	copy_discovery_button->set_text("Copy bridge path");
	copy_discovery_button->set_tooltip_text("Copy the discovery file path for tools/opendust-mcp.");
	copy_discovery_button->connect("pressed", callable_mp(this, &OpenDustAgentsDock::_on_copy_discovery_pressed));
	buttons->add_child(copy_discovery_button);

	restart_button = memnew(Button);
	restart_button->set_text("Restart");
	restart_button->set_tooltip_text("Restart the bridge (disconnects all agents, rotates the token).");
	restart_button->connect("pressed", callable_mp(this, &OpenDustAgentsDock::_on_restart_pressed));
	buttons->add_child(restart_button);

	add_child(memnew(HSeparator));

	Label *agents_title = memnew(Label);
	agents_title->set_text("Connected agents");
	add_child(agents_title);

	agents_list = memnew(ItemList);
	agents_list->set_custom_minimum_size(Size2(0, 80));
	agents_list->set_v_size_flags(SIZE_EXPAND_FILL);
	agents_list->set_stretch_ratio(1.0);
	add_child(agents_list);

	HBoxContainer *log_header = memnew(HBoxContainer);
	add_child(log_header);
	Label *log_title = memnew(Label);
	log_title->set_text("Activity");
	log_title->set_h_size_flags(SIZE_EXPAND_FILL);
	log_header->add_child(log_title);
	clear_log_button = memnew(Button);
	clear_log_button->set_text("Clear");
	clear_log_button->connect("pressed", callable_mp(this, &OpenDustAgentsDock::_on_clear_log_pressed));
	log_header->add_child(clear_log_button);

	activity_log = memnew(RichTextLabel);
	activity_log->set_use_bbcode(true);
	activity_log->set_scroll_follow(true);
	activity_log->set_selection_enabled(true);
	activity_log->set_v_size_flags(SIZE_EXPAND_FILL);
	activity_log->set_stretch_ratio(2.0);
	add_child(activity_log);
}
