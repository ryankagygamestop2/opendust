/**************************************************************************/
/*  claude_session.cpp                                                    */
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

#include "claude_session.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/json.h"
#include "core/os/os.h"
#include "core/os/time.h"
#include "core/string/print_string.h"

// --- static helpers ---------------------------------------------------------

String ClaudeSession::get_claude_path(const Dictionary &p_options) {
	if (p_options.has("claude_path")) {
		String p = p_options["claude_path"];
		if (!p.is_empty()) {
			return p;
		}
	}
	if (ProjectSettings::get_singleton() && ProjectSettings::get_singleton()->has_setting("opendust/slate/claude_path")) {
		String p = GLOBAL_GET("opendust/slate/claude_path");
		if (!p.is_empty()) {
			return p;
		}
	}
	return "claude";
}

Vector<String> ClaudeSession::build_arguments(const Dictionary &p_options) {
	// Confirmed against `claude --help` (Claude Code 2.1.284, 2026-09-29):
	//   -p/--print, --output-format stream-json, --input-format stream-json,
	//   --verbose, --include-partial-messages, --permission-mode <mode>,
	//   --mcp-config <configs...>, --resume <session-id>, --model <model>,
	//   -n/--name <name>, --append-system-prompt-file, --system-prompt-file,
	//   --allowedTools, --disallowedTools, --max-budget-usd, --no-session-persistence.
	Vector<String> args;
	args.push_back("-p");
	args.push_back("--output-format");
	args.push_back("stream-json");
	args.push_back("--input-format");
	args.push_back("stream-json");
	args.push_back("--verbose");
	args.push_back("--include-partial-messages");

	String permission_mode = p_options.get("permission_mode", "");
	if (permission_mode.is_empty() && ProjectSettings::get_singleton() && ProjectSettings::get_singleton()->has_setting("opendust/slate/default_permission_mode")) {
		permission_mode = GLOBAL_GET("opendust/slate/default_permission_mode");
	}
	if (!permission_mode.is_empty() && permission_mode != "default") {
		args.push_back("--permission-mode");
		args.push_back(permission_mode);
	}

	String mcp_config = p_options.get("mcp_config", "");
	if (!mcp_config.is_empty()) {
		args.push_back("--mcp-config");
		args.push_back(mcp_config);
	}

	String resume = p_options.get("resume", "");
	if (!resume.is_empty()) {
		args.push_back("--resume");
		args.push_back(resume);
	}

	String model = p_options.get("model", "");
	if (!model.is_empty()) {
		args.push_back("--model");
		args.push_back(model);
	}

	String name = p_options.get("name", "");
	if (!name.is_empty()) {
		args.push_back("--name");
		args.push_back(name);
	}

	String append_sp = p_options.get("append_system_prompt_file", "");
	if (!append_sp.is_empty()) {
		args.push_back("--append-system-prompt-file");
		args.push_back(append_sp);
	}

	String sp = p_options.get("system_prompt_file", "");
	if (!sp.is_empty()) {
		args.push_back("--system-prompt-file");
		args.push_back(sp);
	}

	if (p_options.has("allowed_tools")) {
		PackedStringArray tools = p_options["allowed_tools"];
		if (!tools.is_empty()) {
			args.push_back("--allowedTools");
			for (const String &t : tools) {
				args.push_back(t);
			}
		}
	}
	if (p_options.has("disallowed_tools")) {
		PackedStringArray tools = p_options["disallowed_tools"];
		if (!tools.is_empty()) {
			args.push_back("--disallowedTools");
			for (const String &t : tools) {
				args.push_back(t);
			}
		}
	}
	if (p_options.has("max_budget_usd")) {
		double budget = p_options["max_budget_usd"];
		if (budget > 0.0) {
			args.push_back("--max-budget-usd");
			args.push_back(String::num(budget, 2));
		}
	}
	if (p_options.has("no_session_persistence") && bool(p_options["no_session_persistence"])) {
		args.push_back("--no-session-persistence");
	}
	if (p_options.has("extra_args")) {
		PackedStringArray extra = p_options["extra_args"];
		for (const String &e : extra) {
			args.push_back(e);
		}
	}
	return args;
}

// --- launcher script --------------------------------------------------------
//
// OS::execute_with_pipe has no cwd parameter and Godot's Windows argument
// quoting does not escape embedded quotes, so a `cmd /c "cd ... && ..."`
// one-liner breaks on any path with a space. Instead we write a tiny launcher
// script whose quoting we fully control and execute that. It is deleted on stop().

static String _batch_escape(const String &p_arg) {
	// Inside double quotes in a batch file only `"` and `%` need care.
	String s = p_arg.replace("%", "%%").replace("\"", "");
	return "\"" + s + "\"";
}

static String _sh_escape(const String &p_arg) {
	return "'" + p_arg.replace("'", "'\\''") + "'";
}

String ClaudeSession::_write_launcher_script(const String &p_cwd, const Vector<String> &p_argv) {
	String dir = OS::get_singleton()->get_user_data_dir().path_join("opendust").path_join("slate");
	Error err = DirAccess::make_dir_recursive_absolute(dir);
	ERR_FAIL_COND_V_MSG(err != OK && err != ERR_ALREADY_EXISTS, String(), "ClaudeSession: can't create launcher directory " + dir);

	const bool is_windows = OS::get_singleton()->get_name() == "Windows";
	String stamp = itos(OS::get_singleton()->get_process_id()) + "-" + itos(Time::get_singleton()->get_ticks_usec());
	String path = dir.path_join("session-" + stamp + (is_windows ? ".cmd" : ".sh"));

	String exe = get_claude_path(options);
	String body;
	if (is_windows) {
		body += "@echo off\r\n";
		body += "cd /d " + _batch_escape(p_cwd.replace("/", "\\")) + "\r\n";
		body += _batch_escape(exe);
		for (const String &a : p_argv) {
			body += " " + _batch_escape(a);
		}
		body += "\r\n";
	} else {
		body += "#!/bin/sh\n";
		body += "cd " + _sh_escape(p_cwd) + " || exit 97\n";
		body += "exec " + _sh_escape(exe);
		for (const String &a : p_argv) {
			body += " " + _sh_escape(a);
		}
		body += "\n";
	}

	Ref<FileAccess> f = FileAccess::open(path, FileAccess::WRITE, &err);
	ERR_FAIL_COND_V_MSG(f.is_null(), String(), "ClaudeSession: can't write launcher script " + path);
	f->store_string(body);
	f->close();
	return path;
}

void ClaudeSession::_cleanup_launcher_script() {
	if (launcher_script_path.is_empty()) {
		return;
	}
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	if (da.is_valid() && da->file_exists(launcher_script_path)) {
		da->remove(launcher_script_path);
	}
	launcher_script_path = String();
}

// --- process ----------------------------------------------------------------

Error ClaudeSession::start(const String &p_cwd, const Dictionary &p_options) {
	ERR_FAIL_COND_V_MSG(started, ERR_ALREADY_IN_USE, "ClaudeSession already started; call stop() first.");

	cwd = p_cwd;
	options = p_options;
	session_id = String();
	exited_emitted = false;
	exit_code = -1;
	streamed_text_this_message = false;
	stop_requested.clear();
	reader_finished.clear();
	stderr_finished.clear();

	Vector<String> argv = build_arguments(options);
	launcher_script_path = _write_launcher_script(cwd, argv);
	ERR_FAIL_COND_V(launcher_script_path.is_empty(), ERR_CANT_CREATE);

	String shell;
	List<String> shell_args;
	if (OS::get_singleton()->get_name() == "Windows") {
		shell = "cmd.exe";
		shell_args.push_back("/d");
		shell_args.push_back("/c");
		shell_args.push_back(launcher_script_path);
	} else {
		shell = "/bin/sh";
		shell_args.push_back(launcher_script_path);
	}

	// Blocking pipes: the reader thread blocks on ReadFile/read(); writes from
	// the main thread only block if the child's stdin buffer is full.
	Dictionary ret = OS::get_singleton()->execute_with_pipe(shell, shell_args, true);
	if (!ret.has("stdio") || !ret.has("pid")) {
		_cleanup_launcher_script();
		ERR_FAIL_V_MSG(ERR_CANT_FORK, "ClaudeSession: failed to launch claude via " + shell);
	}
	stdio = ret["stdio"];
	stderr_pipe = ret["stderr"];
	pid = ret["pid"];
	ERR_FAIL_COND_V(stdio.is_null(), ERR_CANT_FORK);

	started = true;
	reader_thread.start(_reader_thread_func, this);
	if (stderr_pipe.is_valid()) {
		stderr_thread.start(_stderr_thread_func, this);
	} else {
		stderr_finished.set();
	}
	return OK;
}

bool ClaudeSession::is_running() const {
	if (!started || pid < 0) {
		return false;
	}
	return OS::get_singleton()->is_process_running(pid);
}

void ClaudeSession::_join_threads() {
	if (reader_thread.is_started()) {
		reader_thread.wait_to_finish();
	}
	if (stderr_thread.is_started()) {
		stderr_thread.wait_to_finish();
	}
}

void ClaudeSession::stop() {
	if (!started) {
		return;
	}
	stop_requested.set();
	if (pid >= 0 && OS::get_singleton()->is_process_running(pid)) {
		OS::get_singleton()->kill(pid);
	}
	// Killing the child closes its end of the pipes, which unblocks the readers.
	_join_threads();
	if (stdio.is_valid()) {
		stdio->close();
	}
	if (stderr_pipe.is_valid()) {
		stderr_pipe->close();
	}
	stdio.unref();
	stderr_pipe.unref();
	if (pid >= 0) {
		exit_code = OS::get_singleton()->get_process_exit_code(pid);
	}
	_cleanup_launcher_script();
	started = false;
	if (!exited_emitted) {
		exited_emitted = true;
		emit_signal(SNAME("exited"), exit_code);
	}
	pid = -1;
}

// --- reader threads ---------------------------------------------------------

bool ClaudeSession::_read_line(const Ref<FileAccess> &p_file, String &r_line) {
	// Byte-at-a-time read: FileAccess::get_line() relies on eof_reached(),
	// which pipes don't report the way regular files do. Volume here is small.
	Vector<uint8_t> buf;
	while (true) {
		uint8_t c = 0;
		uint64_t n = p_file->get_buffer(&c, 1);
		if (n != 1 || p_file->get_error() != OK) {
			if (buf.is_empty()) {
				return false;
			}
			break;
		}
		if (c == '\n') {
			break;
		}
		if (c == '\r') {
			continue;
		}
		buf.push_back(c);
	}
	r_line = String::utf8((const char *)buf.ptr(), buf.size());
	return true;
}

void ClaudeSession::_reader_thread_func(void *p_userdata) {
	ClaudeSession *self = static_cast<ClaudeSession *>(p_userdata);
	String line;
	while (!self->stop_requested.is_set() && self->stdio.is_valid() && _read_line(self->stdio, line)) {
		if (line.strip_edges().is_empty()) {
			continue;
		}
		JSON json;
		Error err = json.parse(line);
		MutexLock lock(self->queue_mutex);
		if (err == OK && json.get_data().get_type() == Variant::DICTIONARY) {
			self->event_queue.push_back(json.get_data());
		} else {
			self->parse_error_queue.push_back(line);
		}
	}
	self->reader_finished.set();
}

void ClaudeSession::_stderr_thread_func(void *p_userdata) {
	ClaudeSession *self = static_cast<ClaudeSession *>(p_userdata);
	String line;
	while (!self->stop_requested.is_set() && self->stderr_pipe.is_valid() && _read_line(self->stderr_pipe, line)) {
		MutexLock lock(self->queue_mutex);
		self->stderr_queue.push_back(line);
	}
	self->stderr_finished.set();
}

// --- sending ----------------------------------------------------------------

void ClaudeSession::send_raw(const Dictionary &p_message) {
	ERR_FAIL_COND_MSG(!started || stdio.is_null(), "ClaudeSession is not running.");
	String line = JSON::stringify(p_message, "", false);
	stdio->store_string(line + "\n");
	stdio->flush();
}

void ClaudeSession::send_user_message(const String &p_text, const Array &p_attachments) {
	Array content;
	Dictionary text_block;
	text_block["type"] = "text";
	text_block["text"] = p_text;
	content.push_back(text_block);
	for (int i = 0; i < p_attachments.size(); i++) {
		content.push_back(p_attachments[i]);
	}
	Dictionary message;
	message["role"] = "user";
	message["content"] = content;
	Dictionary envelope;
	envelope["type"] = "user";
	envelope["message"] = message;
	send_raw(envelope);
}

void ClaudeSession::respond_permission(const String &p_request_id, bool p_allow, const Dictionary &p_updated_input, const String &p_deny_message) {
	// Shape follows the Claude Code SDK control protocol (control_request /
	// control_response). Not verified against a live prompt yet; see README.
	Dictionary inner;
	if (p_allow) {
		inner["behavior"] = "allow";
		if (!p_updated_input.is_empty()) {
			inner["updatedInput"] = p_updated_input;
		}
	} else {
		inner["behavior"] = "deny";
		inner["message"] = p_deny_message.is_empty() ? String("Denied by the slate holder.") : p_deny_message;
	}
	Dictionary response;
	response["subtype"] = "success";
	response["request_id"] = p_request_id;
	response["response"] = inner;
	Dictionary envelope;
	envelope["type"] = "control_response";
	envelope["response"] = response;
	send_raw(envelope);
}

// --- polling / dispatch -----------------------------------------------------

void ClaudeSession::poll() {
	if (!started) {
		return;
	}
	List<Dictionary> events;
	List<String> errs;
	List<String> bad_lines;
	{
		MutexLock lock(queue_mutex);
		events = event_queue;
		event_queue.clear();
		errs = stderr_queue;
		stderr_queue.clear();
		bad_lines = parse_error_queue;
		parse_error_queue.clear();
	}
	for (const Dictionary &ev : events) {
		emit_signal(SNAME("raw_event"), ev);
		_dispatch_event(ev);
	}
	for (const String &e : errs) {
		emit_signal(SNAME("stderr_line"), e);
	}
	for (const String &l : bad_lines) {
		emit_signal(SNAME("stderr_line"), "[unparsed stdout] " + l);
	}

	if (reader_finished.is_set() && !exited_emitted) {
		// Child closed stdout: it has exited or is exiting. Finish cleanly.
		stop();
	}
}

void ClaudeSession::_dispatch_event(const Dictionary &p_event) {
	String type = p_event.get("type", "");
	if (type == "system") {
		String subtype = p_event.get("subtype", "");
		if (subtype == "init") {
			session_id = p_event.get("session_id", "");
			emit_signal(SNAME("started"), session_id);
		}
	} else if (type == "stream_event") {
		_handle_stream_event(p_event.get("event", Dictionary()));
	} else if (type == "assistant") {
		_handle_assistant_message(p_event.get("message", Dictionary()));
	} else if (type == "user") {
		_handle_user_message(p_event.get("message", Dictionary()));
	} else if (type == "result") {
		emit_signal(SNAME("result"), p_event);
	} else if (type == "control_request") {
		emit_signal(SNAME("permission_request"), p_event);
	}
	// Anything else was already surfaced via raw_event.
}

void ClaudeSession::_handle_stream_event(const Dictionary &p_stream_event) {
	String et = p_stream_event.get("type", "");
	if (et == "message_start") {
		streamed_text_this_message = false;
	} else if (et == "content_block_delta") {
		Dictionary delta = p_stream_event.get("delta", Dictionary());
		if (String(delta.get("type", "")) == "text_delta") {
			streamed_text_this_message = true;
			emit_signal(SNAME("text_delta"), String(delta.get("text", "")));
		}
	}
}

void ClaudeSession::_handle_assistant_message(const Dictionary &p_message) {
	Array content = p_message.get("content", Array());
	for (int i = 0; i < content.size(); i++) {
		Dictionary block = content[i];
		String btype = block.get("type", "");
		if (btype == "text") {
			if (!streamed_text_this_message) {
				// Partial messages were not streamed (flag off or old CLI):
				// deliver the whole text as one delta so listeners need one path.
				emit_signal(SNAME("text_delta"), String(block.get("text", "")));
			}
		} else if (btype == "tool_use") {
			emit_signal(SNAME("tool_use"), String(block.get("id", "")), String(block.get("name", "")), Dictionary(block.get("input", Dictionary())));
		}
	}
	streamed_text_this_message = false;
	emit_signal(SNAME("message_complete"), p_message);
}

void ClaudeSession::_handle_user_message(const Dictionary &p_message) {
	Variant content_v = p_message.get("content", Variant());
	if (content_v.get_type() != Variant::ARRAY) {
		return; // A plain-text echo of our own input (with --replay-user-messages); ignore.
	}
	Array content = content_v;
	for (int i = 0; i < content.size(); i++) {
		Dictionary block = content[i];
		if (String(block.get("type", "")) != "tool_result") {
			continue;
		}
		Variant result_content = block.get("content", Variant());
		String text;
		if (result_content.get_type() == Variant::STRING) {
			text = result_content;
		} else if (result_content.get_type() == Variant::ARRAY) {
			Array parts = result_content;
			for (int j = 0; j < parts.size(); j++) {
				if (parts[j].get_type() == Variant::DICTIONARY) {
					Dictionary part = parts[j];
					if (part.has("text")) {
						text += String(part["text"]);
					} else {
						text += JSON::stringify(part);
					}
				} else {
					text += String(parts[j]);
				}
				if (j + 1 < parts.size()) {
					text += "\n";
				}
			}
		} else {
			text = JSON::stringify(result_content);
		}
		bool is_error = block.get("is_error", false);
		emit_signal(SNAME("tool_result"), String(block.get("tool_use_id", "")), text, is_error);
	}
}

// --- bindings ---------------------------------------------------------------

void ClaudeSession::_bind_methods() {
	ClassDB::bind_static_method("ClaudeSession", D_METHOD("get_claude_path", "options"), &ClaudeSession::get_claude_path, DEFVAL(Dictionary()));

	ClassDB::bind_method(D_METHOD("start", "cwd", "options"), &ClaudeSession::start, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("stop"), &ClaudeSession::stop);
	ClassDB::bind_method(D_METHOD("is_running"), &ClaudeSession::is_running);
	ClassDB::bind_method(D_METHOD("send_user_message", "text", "attachments"), &ClaudeSession::send_user_message, DEFVAL(Array()));
	ClassDB::bind_method(D_METHOD("send_raw", "message"), &ClaudeSession::send_raw);
	ClassDB::bind_method(D_METHOD("respond_permission", "request_id", "allow", "updated_input", "deny_message"), &ClaudeSession::respond_permission, DEFVAL(Dictionary()), DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("poll"), &ClaudeSession::poll);
	ClassDB::bind_method(D_METHOD("get_session_id"), &ClaudeSession::get_session_id);
	ClassDB::bind_method(D_METHOD("get_cwd"), &ClaudeSession::get_cwd);
	ClassDB::bind_method(D_METHOD("get_pid"), &ClaudeSession::get_pid);
	ClassDB::bind_method(D_METHOD("get_exit_code"), &ClaudeSession::get_exit_code);

	ADD_SIGNAL(MethodInfo("started", PropertyInfo(Variant::STRING, "session_id")));
	ADD_SIGNAL(MethodInfo("text_delta", PropertyInfo(Variant::STRING, "text")));
	ADD_SIGNAL(MethodInfo("message_complete", PropertyInfo(Variant::DICTIONARY, "message")));
	ADD_SIGNAL(MethodInfo("tool_use", PropertyInfo(Variant::STRING, "id"), PropertyInfo(Variant::STRING, "name"), PropertyInfo(Variant::DICTIONARY, "input")));
	ADD_SIGNAL(MethodInfo("tool_result", PropertyInfo(Variant::STRING, "id"), PropertyInfo(Variant::STRING, "content"), PropertyInfo(Variant::BOOL, "is_error")));
	ADD_SIGNAL(MethodInfo("result", PropertyInfo(Variant::DICTIONARY, "summary")));
	ADD_SIGNAL(MethodInfo("permission_request", PropertyInfo(Variant::DICTIONARY, "request")));
	ADD_SIGNAL(MethodInfo("stderr_line", PropertyInfo(Variant::STRING, "text")));
	ADD_SIGNAL(MethodInfo("raw_event", PropertyInfo(Variant::DICTIONARY, "event")));
	ADD_SIGNAL(MethodInfo("exited", PropertyInfo(Variant::INT, "code")));
}

ClaudeSession::ClaudeSession() {
}

ClaudeSession::~ClaudeSession() {
	// Don't emit from a destructor; just make sure the child and threads are gone.
	if (started) {
		exited_emitted = true;
		stop();
	}
}
