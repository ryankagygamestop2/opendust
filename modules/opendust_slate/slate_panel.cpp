/**************************************************************************/
/*  slate_panel.cpp                                                       */
/**************************************************************************/
/*                         This file is part of:                          */
/*                              OPENDUST                                  */
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

#include "slate_panel.h"
#include "core/object/class_db.h"
#include "core/object/callable_mp.h"

#include "room.h"

#include "core/io/json.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/rich_text_label.h"
#include "scene/gui/scroll_container.h"
#include "scene/resources/style_box_flat.h"

// Palette. Dark, quiet, readable on a small screen.
static const Color COLOR_BG(0.063, 0.067, 0.078);
static const Color COLOR_PANEL(0.102, 0.106, 0.122);
static const Color COLOR_CARD(0.133, 0.141, 0.165);
static const Color COLOR_TEXT(0.902, 0.902, 0.902);
static const Color COLOR_USER(0.561, 0.827, 0.659);
static const Color COLOR_SYSTEM(0.62, 0.62, 0.68);
static const Color COLOR_TOOL(0.72, 0.78, 0.95);
static const Color COLOR_ERROR(0.878, 0.478, 0.478);

static Ref<StyleBoxFlat> _flat(const Color &p_color, int p_radius, int p_margin) {
	Ref<StyleBoxFlat> sb;
	sb.instantiate();
	sb->set_bg_color(p_color);
	sb->set_corner_radius_all(p_radius);
	sb->set_content_margin_all(p_margin);
	return sb;
}

void SlatePanel::_build_ui() {
	add_theme_style_override(SNAME("panel"), _flat(COLOR_BG, 12, 12));
	set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);

	root_vbox = memnew(VBoxContainer);
	root_vbox->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	root_vbox->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	add_child(root_vbox, false, Node::INTERNAL_MODE_FRONT);

	// Status strip.
	status_strip = memnew(HBoxContainer);
	root_vbox->add_child(status_strip);
	status_room = memnew(Label);
	status_room->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	status_room->add_theme_color_override(SNAME("font_color"), COLOR_SYSTEM);
	status_room->add_theme_font_size_override(SNAME("font_size"), small_font_size);
	status_room->set_text("no room");
	status_strip->add_child(status_room);
	status_state = memnew(Label);
	status_state->add_theme_color_override(SNAME("font_color"), COLOR_SYSTEM);
	status_state->add_theme_font_size_override(SNAME("font_size"), small_font_size);
	status_state->set_text("no session");
	status_strip->add_child(status_state);

	// Transcript.
	scroll = memnew(ScrollContainer);
	scroll->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	scroll->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	scroll->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
	root_vbox->add_child(scroll);
	transcript = memnew(VBoxContainer);
	transcript->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	transcript->add_theme_constant_override(SNAME("separation"), 8);
	scroll->add_child(transcript);

	// Input.
	input = memnew(LineEdit);
	input->set_placeholder("say something to the slate");
	input->add_theme_style_override(SNAME("normal"), _flat(COLOR_PANEL, 8, 10));
	input->add_theme_style_override(SNAME("focus"), _flat(COLOR_PANEL, 8, 10));
	input->add_theme_color_override(SNAME("font_color"), COLOR_TEXT);
	input->add_theme_font_size_override(SNAME("font_size"), normal_font_size);
	input->connect(SNAME("text_submitted"), callable_mp(this, &SlatePanel::_on_text_submitted));
	root_vbox->add_child(input);
}

RichTextLabel *SlatePanel::_make_text_block(const Color &p_color) {
	RichTextLabel *rtl = memnew(RichTextLabel);
	rtl->set_use_bbcode(false);
	rtl->set_fit_content(true);
	rtl->set_scroll_active(false);
	rtl->set_selection_enabled(true);
	rtl->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	rtl->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	rtl->add_theme_color_override(SNAME("default_color"), p_color);
	rtl->add_theme_font_size_override(SNAME("normal_font_size"), normal_font_size);
	transcript->add_child(rtl);
	return rtl;
}

void SlatePanel::_scroll_to_bottom() {
	if (!scroll || !scroll->is_inside_tree()) {
		return;
	}
	scroll->set_v_scroll((int)scroll->get_v_scroll_bar()->get_max());
}

void SlatePanel::_set_state(const String &p_text) {
	if (status_state) {
		status_state->set_text(p_text);
	}
}

void SlatePanel::_refresh_room_label() {
	if (!status_room) {
		return;
	}
	if (room) {
		String label = room->get_display_name().is_empty() ? room->get_room_id() : room->get_display_name();
		status_room->set_text(label + U"  ·  " + room->get_absolute_dir());
	} else if (session.is_valid() && !session->get_cwd().is_empty()) {
		status_room->set_text(session->get_cwd());
	} else {
		status_room->set_text("no room");
	}
}

String SlatePanel::_summarize_input(const Dictionary &p_input) {
	// One line: prefer the obviously-primary keys, fall back to compact JSON.
	static const char *preferred[] = { "command", "file_path", "path", "pattern", "query", "url", "description", "prompt", nullptr };
	for (int i = 0; preferred[i]; i++) {
		if (p_input.has(preferred[i])) {
			String v = String(p_input[preferred[i]]).replace("\n", " ");
			if (v.length() > 90) {
				v = v.substr(0, 87) + "...";
			}
			return v;
		}
	}
	String s = JSON::stringify(p_input).replace("\n", " ");
	if (s.length() > 90) {
		s = s.substr(0, 87) + "...";
	}
	return s;
}

// --- session wiring ---------------------------------------------------------

void SlatePanel::_disconnect_session() {
	if (session.is_null()) {
		return;
	}
	session->disconnect(SNAME("started"), callable_mp(this, &SlatePanel::_on_session_started));
	session->disconnect(SNAME("text_delta"), callable_mp(this, &SlatePanel::_on_text_delta));
	session->disconnect(SNAME("message_complete"), callable_mp(this, &SlatePanel::_on_message_complete));
	session->disconnect(SNAME("tool_use"), callable_mp(this, &SlatePanel::_on_tool_use));
	session->disconnect(SNAME("tool_result"), callable_mp(this, &SlatePanel::_on_tool_result));
	session->disconnect(SNAME("result"), callable_mp(this, &SlatePanel::_on_result));
	session->disconnect(SNAME("permission_request"), callable_mp(this, &SlatePanel::_on_permission_request));
	session->disconnect(SNAME("stderr_line"), callable_mp(this, &SlatePanel::_on_stderr_line));
	session->disconnect(SNAME("exited"), callable_mp(this, &SlatePanel::_on_exited));
}

void SlatePanel::bind_session(const Ref<ClaudeSession> &p_session) {
	if (session == p_session) {
		return;
	}
	_disconnect_session();
	session = p_session;
	current_assistant = nullptr;
	if (session.is_valid()) {
		session->connect(SNAME("started"), callable_mp(this, &SlatePanel::_on_session_started));
		session->connect(SNAME("text_delta"), callable_mp(this, &SlatePanel::_on_text_delta));
		session->connect(SNAME("message_complete"), callable_mp(this, &SlatePanel::_on_message_complete));
		session->connect(SNAME("tool_use"), callable_mp(this, &SlatePanel::_on_tool_use));
		session->connect(SNAME("tool_result"), callable_mp(this, &SlatePanel::_on_tool_result));
		session->connect(SNAME("result"), callable_mp(this, &SlatePanel::_on_result));
		session->connect(SNAME("permission_request"), callable_mp(this, &SlatePanel::_on_permission_request));
		session->connect(SNAME("stderr_line"), callable_mp(this, &SlatePanel::_on_stderr_line));
		session->connect(SNAME("exited"), callable_mp(this, &SlatePanel::_on_exited));
		_set_state(session->is_running() ? "connecting" : "no session");
	} else {
		_set_state("no session");
	}
	_refresh_room_label();
}

void SlatePanel::bind_room(Room *p_room) {
	room = p_room;
	_refresh_room_label();
}

// --- public actions ---------------------------------------------------------

void SlatePanel::append_user_text(const String &p_text) {
	RichTextLabel *rtl = _make_text_block(COLOR_USER);
	rtl->add_text("> " + p_text);
	current_assistant = nullptr;
	callable_mp(this, &SlatePanel::_scroll_to_bottom).call_deferred();
}

void SlatePanel::append_system_text(const String &p_text) {
	RichTextLabel *rtl = _make_text_block(COLOR_SYSTEM);
	rtl->add_theme_font_size_override(SNAME("normal_font_size"), small_font_size);
	rtl->add_text(p_text);
	current_assistant = nullptr;
	callable_mp(this, &SlatePanel::_scroll_to_bottom).call_deferred();
}

void SlatePanel::submit(const String &p_text) {
	String text = p_text.strip_edges();
	if (text.is_empty()) {
		return;
	}
	append_user_text(text);
	if (session.is_valid() && session->is_running()) {
		session->send_user_message(text);
		_set_state("thinking");
	} else {
		append_system_text("(no running session)");
	}
	emit_signal(SNAME("submitted"), text);
}

void SlatePanel::clear_transcript() {
	if (!transcript) {
		return;
	}
	for (int i = transcript->get_child_count() - 1; i >= 0; i--) {
		Node *c = transcript->get_child(i);
		transcript->remove_child(c);
		c->queue_free();
	}
	tool_cards.clear();
	current_assistant = nullptr;
}

void SlatePanel::focus_input() {
	if (input && input->is_inside_tree()) {
		input->grab_focus();
	}
}

void SlatePanel::set_normal_font_size(int p_size) {
	normal_font_size = MAX(8, p_size);
	if (input) {
		input->add_theme_font_size_override(SNAME("font_size"), normal_font_size);
	}
}

// --- signal handlers --------------------------------------------------------

void SlatePanel::_on_text_submitted(const String &p_text) {
	input->clear();
	submit(p_text);
}

void SlatePanel::_on_session_started(const String &p_session_id) {
	_set_state("ready");
	_refresh_room_label();
}

void SlatePanel::_on_text_delta(const String &p_text) {
	if (!current_assistant) {
		current_assistant = _make_text_block(COLOR_TEXT);
	}
	current_assistant->add_text(p_text);
	callable_mp(this, &SlatePanel::_scroll_to_bottom).call_deferred();
}

void SlatePanel::_on_message_complete(const Dictionary &p_message) {
	current_assistant = nullptr;
}

void SlatePanel::_on_tool_use(const String &p_id, const String &p_name, const Dictionary &p_input) {
	current_assistant = nullptr;

	ToolCard tc;
	tc.name = p_name;
	tc.card = memnew(PanelContainer);
	tc.card->add_theme_style_override(SNAME("panel"), _flat(COLOR_CARD, 8, 8));
	tc.card->set_h_size_flags(Control::SIZE_EXPAND_FILL);

	VBoxContainer *vb = memnew(VBoxContainer);
	tc.card->add_child(vb);

	HBoxContainer *head = memnew(HBoxContainer);
	vb->add_child(head);

	tc.toggle = memnew(Button);
	tc.toggle->set_toggle_mode(true);
	tc.toggle->set_flat(true);
	tc.toggle->set_text("+ " + p_name);
	tc.toggle->add_theme_color_override(SNAME("font_color"), COLOR_TOOL);
	tc.toggle->add_theme_font_size_override(SNAME("font_size"), small_font_size);
	tc.toggle->connect(SNAME("toggled"), callable_mp(this, &SlatePanel::_on_card_toggled).bind(tc.toggle));
	head->add_child(tc.toggle);

	tc.summary = memnew(Label);
	tc.summary->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	tc.summary->set_text(_summarize_input(p_input));
	tc.summary->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
	tc.summary->add_theme_color_override(SNAME("font_color"), COLOR_SYSTEM);
	tc.summary->add_theme_font_size_override(SNAME("font_size"), small_font_size);
	head->add_child(tc.summary);

	tc.result = memnew(RichTextLabel);
	tc.result->set_use_bbcode(false);
	tc.result->set_fit_content(true);
	tc.result->set_scroll_active(false);
	tc.result->set_selection_enabled(true);
	tc.result->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	tc.result->add_theme_color_override(SNAME("default_color"), COLOR_SYSTEM);
	tc.result->add_theme_font_size_override(SNAME("normal_font_size"), small_font_size);
	tc.result->add_text("running...");
	tc.result->set_visible(false);
	vb->add_child(tc.result);

	transcript->add_child(tc.card);
	tool_cards[p_id] = tc;
	callable_mp(this, &SlatePanel::_scroll_to_bottom).call_deferred();
}

void SlatePanel::_on_tool_result(const String &p_id, const String &p_content, bool p_is_error) {
	ToolCard *tc = tool_cards.getptr(p_id);
	if (!tc) {
		return;
	}
	tc->result->clear();
	String content = p_content;
	if (content.length() > 4000) {
		content = content.substr(0, 4000) + "\n... (truncated on the slate; full result is in the session)";
	}
	tc->result->add_text(content.is_empty() ? String("(no output)") : content);
	if (p_is_error) {
		tc->result->add_theme_color_override(SNAME("default_color"), COLOR_ERROR);
		tc->toggle->add_theme_color_override(SNAME("font_color"), COLOR_ERROR);
		tc->toggle->set_text((tc->toggle->is_pressed() ? "- " : "+ ") + tc->name + "  (error)");
	}
}

void SlatePanel::_on_card_toggled(bool p_pressed, Button *p_button) {
	for (KeyValue<String, ToolCard> &kv : tool_cards) {
		if (kv.value.toggle == p_button) {
			kv.value.result->set_visible(p_pressed);
			String base = kv.value.toggle->get_text().substr(2);
			kv.value.toggle->set_text((p_pressed ? "- " : "+ ") + base);
			break;
		}
	}
}

void SlatePanel::_on_result(const Dictionary &p_summary) {
	current_assistant = nullptr;
	bool is_error = p_summary.get("is_error", false);
	_set_state(is_error ? "error" : "ready");
	if (is_error) {
		String msg = p_summary.get("result", "");
		if (!msg.is_empty()) {
			append_system_text(msg);
		}
	}
}

void SlatePanel::_on_permission_request(const Dictionary &p_request) {
	// The panel doesn't decide; it shows the request and lets the owner
	// (AgentSlate3D or a project script) answer via ClaudeSession.respond_permission.
	Dictionary req = p_request.get("request", Dictionary());
	String tool = req.get("tool_name", "");
	append_system_text("permission requested: " + (tool.is_empty() ? String("(unknown tool)") : tool));
	_set_state("waiting for permission");
	emit_signal(SNAME("permission_requested"), p_request);
}

void SlatePanel::_on_stderr_line(const String &p_text) {
	emit_signal(SNAME("stderr_line"), p_text);
}

void SlatePanel::_on_exited(int p_code) {
	current_assistant = nullptr;
	_set_state("exited (" + itos(p_code) + ")");
	append_system_text("session ended");
}

// --- lifecycle --------------------------------------------------------------

void SlatePanel::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			_refresh_room_label();
		} break;
		case NOTIFICATION_PREDELETE: {
			_disconnect_session();
		} break;
	}
}

void SlatePanel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("bind_session", "session"), &SlatePanel::bind_session);
	ClassDB::bind_method(D_METHOD("get_session"), &SlatePanel::get_session);
	ClassDB::bind_method(D_METHOD("bind_room", "room"), &SlatePanel::bind_room);
	ClassDB::bind_method(D_METHOD("get_room"), &SlatePanel::get_room);
	ClassDB::bind_method(D_METHOD("submit", "text"), &SlatePanel::submit);
	ClassDB::bind_method(D_METHOD("append_user_text", "text"), &SlatePanel::append_user_text);
	ClassDB::bind_method(D_METHOD("append_system_text", "text"), &SlatePanel::append_system_text);
	ClassDB::bind_method(D_METHOD("clear_transcript"), &SlatePanel::clear_transcript);
	ClassDB::bind_method(D_METHOD("focus_input"), &SlatePanel::focus_input);
	ClassDB::bind_method(D_METHOD("set_normal_font_size", "size"), &SlatePanel::set_normal_font_size);
	ClassDB::bind_method(D_METHOD("get_normal_font_size"), &SlatePanel::get_normal_font_size);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "normal_font_size", PROPERTY_HINT_RANGE, "8,64,1"), "set_normal_font_size", "get_normal_font_size");

	ADD_SIGNAL(MethodInfo("submitted", PropertyInfo(Variant::STRING, "text")));
	ADD_SIGNAL(MethodInfo("permission_requested", PropertyInfo(Variant::DICTIONARY, "request")));
	ADD_SIGNAL(MethodInfo("stderr_line", PropertyInfo(Variant::STRING, "text")));
}

SlatePanel::SlatePanel() {
	_build_ui();
}
