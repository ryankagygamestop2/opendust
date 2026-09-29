/**************************************************************************/
/*  slate_panel.h                                                         */
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

#pragma once

#include "claude_session.h"

#include "core/templates/hash_map.h"
#include "scene/gui/panel_container.h"

class Button;
class HBoxContainer;
class Label;
class LineEdit;
class RichTextLabel;
class Room;
class ScrollContainer;
class VBoxContainer;

// The 2D surface drawn on a slate's screen: a streaming transcript, tool
// cards, an input line, and a status strip. Built procedurally so it works
// with no scene assets; a project can replace it via AgentSlate3D.panel_scene.
class SlatePanel : public PanelContainer {
	GDCLASS(SlatePanel, PanelContainer);

	struct ToolCard {
		PanelContainer *card = nullptr;
		Button *toggle = nullptr;
		Label *summary = nullptr;
		RichTextLabel *result = nullptr;
		String name;
	};

	Ref<ClaudeSession> session;
	Room *room = nullptr;

	VBoxContainer *root_vbox = nullptr;
	HBoxContainer *status_strip = nullptr;
	Label *status_room = nullptr;
	Label *status_state = nullptr;
	ScrollContainer *scroll = nullptr;
	VBoxContainer *transcript = nullptr;
	LineEdit *input = nullptr;

	RichTextLabel *current_assistant = nullptr;
	HashMap<String, ToolCard> tool_cards;
	int normal_font_size = 18;
	int small_font_size = 14;

	void _build_ui();
	RichTextLabel *_make_text_block(const Color &p_color);
	void _scroll_to_bottom();
	void _set_state(const String &p_text);
	void _refresh_room_label();
	static String _summarize_input(const Dictionary &p_input);

	void _on_text_submitted(const String &p_text);
	void _on_session_started(const String &p_session_id);
	void _on_text_delta(const String &p_text);
	void _on_message_complete(const Dictionary &p_message);
	void _on_tool_use(const String &p_id, const String &p_name, const Dictionary &p_input);
	void _on_tool_result(const String &p_id, const String &p_content, bool p_is_error);
	void _on_result(const Dictionary &p_summary);
	void _on_permission_request(const Dictionary &p_request);
	void _on_stderr_line(const String &p_text);
	void _on_exited(int p_code);
	void _on_card_toggled(bool p_pressed, Button *p_button);
	void _disconnect_session();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	void bind_session(const Ref<ClaudeSession> &p_session);
	Ref<ClaudeSession> get_session() const { return session; }
	void bind_room(Room *p_room);
	Room *get_room() const { return room; }

	// Appends the text as a user message and sends it to the session.
	void submit(const String &p_text);
	void append_user_text(const String &p_text);
	void append_system_text(const String &p_text);
	void clear_transcript();
	void focus_input();

	void set_normal_font_size(int p_size);
	int get_normal_font_size() const { return normal_font_size; }

	SlatePanel();
};
