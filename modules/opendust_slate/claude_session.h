/**************************************************************************/
/*  claude_session.h                                                      */
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

#include "core/io/file_access.h"
#include "core/object/ref_counted.h"
#include "core/os/mutex.h"
#include "core/os/thread.h"
#include "core/templates/list.h"
#include "core/templates/safe_refcount.h"

// One Claude Code child process speaking stream-json on stdin/stdout.
//
// Threading model: a reader thread blocks on the child's stdout and pushes
// parsed JSON events onto a mutex-guarded queue. Nothing but poll() touches
// signals or the scene tree; poll() must be called from the main thread
// (AgentSlate3D does this from its internal process notification).
//
// See docs/opendust/02-slate.md.
class ClaudeSession : public RefCounted {
	GDCLASS(ClaudeSession, RefCounted);

	Ref<FileAccess> stdio;
	Ref<FileAccess> stderr_pipe;
	int64_t pid = -1;
	String cwd;
	Dictionary options;
	String session_id;
	String launcher_script_path;

	Thread reader_thread;
	Thread stderr_thread;
	Mutex queue_mutex;
	List<Dictionary> event_queue;
	List<String> stderr_queue;
	List<String> parse_error_queue;
	SafeFlag reader_finished;
	SafeFlag stderr_finished;
	SafeFlag stop_requested;

	bool started = false;
	bool exited_emitted = false;
	int exit_code = -1;
	bool streamed_text_this_message = false;

	static void _reader_thread_func(void *p_userdata);
	static void _stderr_thread_func(void *p_userdata);
	static bool _read_line(const Ref<FileAccess> &p_file, String &r_line);

	String _write_launcher_script(const String &p_cwd, const Vector<String> &p_argv);
	void _dispatch_event(const Dictionary &p_event);
	void _handle_assistant_message(const Dictionary &p_message);
	void _handle_user_message(const Dictionary &p_message);
	void _handle_stream_event(const Dictionary &p_stream_event);
	void _join_threads();
	void _cleanup_launcher_script();

protected:
	static void _bind_methods();

public:
	// Resolves the claude executable: options["claude_path"], then the
	// project setting opendust/slate/claude_path, then "claude".
	static String get_claude_path(const Dictionary &p_options = Dictionary());

	// The exact argv (without the executable) for the given options. Kept in
	// one place on purpose so the flags are easy to audit against `claude --help`.
	// Recognised option keys:
	//   permission_mode (String), mcp_config (String path), resume (String session id),
	//   model (String), name (String), append_system_prompt_file (String path),
	//   system_prompt_file (String path), allowed_tools (PackedStringArray),
	//   disallowed_tools (PackedStringArray), max_budget_usd (float),
	//   no_session_persistence (bool), extra_args (PackedStringArray).
	static Vector<String> build_arguments(const Dictionary &p_options);

	Error start(const String &p_cwd, const Dictionary &p_options = Dictionary());
	void stop();
	bool is_running() const;

	// Writes one stream-json user message. Attachments are appended as extra
	// content blocks verbatim (e.g. {"type":"image","source":{...}}).
	void send_user_message(const String &p_text, const Array &p_attachments = Array());
	// Writes any JSON object as one line. For callers that know the protocol.
	void send_raw(const Dictionary &p_message);
	void respond_permission(const String &p_request_id, bool p_allow, const Dictionary &p_updated_input = Dictionary(), const String &p_deny_message = String());

	// Main-thread only. Drains the event queue and emits signals.
	void poll();

	String get_session_id() const { return session_id; }
	String get_cwd() const { return cwd; }
	int get_pid() const { return (int)pid; }
	int get_exit_code() const { return exit_code; }

	ClaudeSession();
	~ClaudeSession();
};
