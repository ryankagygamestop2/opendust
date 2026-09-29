/**************************************************************************/
/*  opendust_agents_dock.h                                                */
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

#include "scene/gui/box_container.h"

class Button;
class ItemList;
class Label;
class OpenDustAgentServer;
class RichTextLabel;

// The Agents dock: who is connected, what they've done, and the bridge status.
// Emits `spawn_from_soul_requested` for the soul module to pick up.
class OpenDustAgentsDock : public VBoxContainer {
	GDCLASS(OpenDustAgentsDock, VBoxContainer);

	OpenDustAgentServer *server = nullptr;

	Label *status_label = nullptr;
	Label *discovery_label = nullptr;
	ItemList *agents_list = nullptr;
	RichTextLabel *activity_log = nullptr;
	Button *spawn_button = nullptr;
	Button *copy_discovery_button = nullptr;
	Button *restart_button = nullptr;
	Button *clear_log_button = nullptr;

	int log_lines = 0;
	static const int MAX_LOG_LINES = 400;

	void _refresh_status();
	void _refresh_agents();
	void _append_log(const String &p_bbcode);

	void _on_started(int p_port);
	void _on_stopped();
	void _on_session_connected(const Dictionary &p_session);
	void _on_session_disconnected(const String &p_session_id);
	void _on_tool_called(const String &p_session_id, const String &p_tool, const Dictionary &p_params, bool p_ok);

	void _on_spawn_pressed();
	void _on_copy_discovery_pressed();
	void _on_restart_pressed();
	void _on_clear_log_pressed();

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	void set_server(OpenDustAgentServer *p_server);
	void log_line(const String &p_text);

	OpenDustAgentsDock();
};
