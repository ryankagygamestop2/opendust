/**************************************************************************/
/*  opendust_agent_server.cpp                                             */
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

#include "opendust_agent_server.h"
#include "core/object/class_db.h"
#include "core/object/callable_mp.h"

#include "opendust_json.h"
#include "opendust_tool_registry.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/crypto/crypto_core.h"
#include "core/error/error_macros.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/math/math_funcs.h"
#include "core/os/os.h"
#include "core/os/time.h"
#include "core/version.h"

OpenDustAgentServer *OpenDustAgentServer::singleton = nullptr;

OpenDustAgentServer *OpenDustAgentServer::get_singleton() {
	return singleton;
}

// ---------------------------------------------------------------------------
// Log capture
// ---------------------------------------------------------------------------

void OpenDustAgentServer::_print_handler_func(void *p_this, const String &p_string, bool p_error, bool p_rich) {
	OpenDustAgentServer *self = static_cast<OpenDustAgentServer *>(p_this);
	if (!self) {
		return;
	}
	self->_queue_log(p_error ? "error" : "info", p_string, "print");
}

void OpenDustAgentServer::_error_handler_func(void *p_this, const char *p_func, const char *p_file, int p_line, const char *p_error, const char *p_errorexp, bool p_editor_notify, ErrorHandlerType p_type) {
	OpenDustAgentServer *self = static_cast<OpenDustAgentServer *>(p_this);
	if (!self) {
		return;
	}
	String level = "error";
	if (p_type == ERR_HANDLER_WARNING) {
		level = "warning";
	}
	String text = String::utf8(p_error);
	if (p_errorexp && *p_errorexp) {
		text += ": " + String::utf8(p_errorexp);
	}
	String source = vformat("%s:%d %s", String::utf8(p_file), p_line, String::utf8(p_func));
	self->_queue_log(level, text, source);
}

void OpenDustAgentServer::_queue_log(const String &p_level, const String &p_text, const String &p_source) {
	Dictionary d;
	d["level"] = p_level;
	d["text"] = p_text;
	d["source"] = p_source;
	d["time_ms"] = (int64_t)OS::get_singleton()->get_ticks_msec();
	MutexLock lock(log_mutex);
	log_pending.push_back(d);
	log_ring.push_back(d);
	while (log_ring.size() > LOG_RING_SIZE) {
		log_ring.pop_front();
	}
}

void OpenDustAgentServer::_flush_logs() {
	List<Dictionary> batch;
	{
		MutexLock lock(log_mutex);
		if (log_pending.is_empty()) {
			return;
		}
		batch = log_pending;
		log_pending.clear();
	}
	if (sessions.is_empty()) {
		return;
	}
	for (const Dictionary &d : batch) {
		broadcast("event.log", d);
	}
}

Array OpenDustAgentServer::get_log_tail(int p_lines) const {
	Array out;
	MutexLock lock(log_mutex);
	int skip = MAX(0, log_ring.size() - p_lines);
	int i = 0;
	for (const Dictionary &d : log_ring) {
		if (i++ < skip) {
			continue;
		}
		out.push_back(d);
	}
	return out;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

String OpenDustAgentServer::_mode_name() const {
	return mode == MODE_EDITOR ? "editor" : "runtime";
}

String OpenDustAgentServer::_generate_hex(int p_bytes) const {
	Vector<uint8_t> buf;
	buf.resize(p_bytes);
	CryptoCore::RandomGenerator rng;
	if (rng.init() == OK && rng.get_random_bytes(buf.ptrw(), p_bytes) == OK) {
		return String::hex_encode_buffer(buf.ptr(), p_bytes);
	}
	// Fallback: not cryptographic, but never leaves the token empty.
	for (int i = 0; i < p_bytes; i++) {
		buf.write[i] = (uint8_t)(Math::rand() & 0xFF);
	}
	return String::hex_encode_buffer(buf.ptr(), p_bytes);
}

String OpenDustAgentServer::get_engine_version_string() const {
	return String(GODOT_VERSION_FULL_CONFIG) + ".opendust";
}

String OpenDustAgentServer::_project_data_dir() const {
	// Prefer <project>/.opendust; fall back to user://opendust for exported games
	// where res:// is a read-only pack.
	String dir = ProjectSettings::get_singleton()->globalize_path("res://.opendust");
	if (ProjectSettings::get_singleton()->get_resource_path().is_empty() || DirAccess::make_dir_recursive_absolute(dir) != OK) {
		dir = ProjectSettings::get_singleton()->globalize_path("user://opendust");
		DirAccess::make_dir_recursive_absolute(dir);
	}
	return dir;
}

void OpenDustAgentServer::_write_discovery() {
	Dictionary d;
	d["schema"] = "opendust.bridge/1";
	d["mode"] = _mode_name();
	d["port"] = port;
	d["token"] = token;
	d["pid"] = OS::get_singleton()->get_process_id();
	d["project_path"] = ProjectSettings::get_singleton()->get_resource_path();
	d["project_name"] = GLOBAL_GET("application/config/name");
	d["engine_version"] = get_engine_version_string();
	d["started_at"] = Time::get_singleton()->get_datetime_string_from_system(true);

	Ref<FileAccess> f = FileAccess::open(discovery_path, FileAccess::WRITE);
	ERR_FAIL_COND_MSG(f.is_null(), vformat("OpenDust: cannot write discovery file '%s'.", discovery_path));
	f->store_string(JSON::stringify(d, "  "));
}

void OpenDustAgentServer::_remove_discovery() {
	if (!discovery_path.is_empty() && FileAccess::exists(discovery_path)) {
		DirAccess::remove_absolute(discovery_path);
	}
}

void OpenDustAgentServer::_write_mcp_config() {
	String script = GLOBAL_GET("opendust/agent/mcp_bridge_script");
	if (script.is_empty()) {
		script = OS::get_singleton()->get_executable_path().get_base_dir().path_join("../tools/opendust-mcp/opendust_mcp.py").simplify_path();
	}
	Dictionary server;
	server["command"] = "python";
	Array args;
	args.push_back(script);
	args.push_back("--discovery");
	args.push_back(discovery_path);
	server["args"] = args;
	Dictionary servers;
	servers["opendust"] = server;
	Dictionary root;
	root["mcpServers"] = servers;

	Ref<FileAccess> f = FileAccess::open(mcp_config_path, FileAccess::WRITE);
	if (f.is_null()) {
		WARN_PRINT(vformat("OpenDust: cannot write MCP config '%s'.", mcp_config_path));
		return;
	}
	f->store_string(JSON::stringify(root, "  "));
}

Error OpenDustAgentServer::start(Mode p_mode) {
	ERR_FAIL_COND_V_MSG(running, ERR_ALREADY_IN_USE, "OpenDust agent server is already running.");
#ifndef MODULE_WEBSOCKET_ENABLED
	ERR_FAIL_V_MSG(ERR_UNAVAILABLE, "OpenDust agent server requires the websocket module.");
#else
	mode = p_mode;

	int base_port = mode == MODE_EDITOR ? DEFAULT_EDITOR_PORT : DEFAULT_RUNTIME_PORT;
	int configured = GLOBAL_GET("opendust/agent/port");
	if (configured > 0) {
		base_port = configured;
	}

	tcp_server.instantiate();
	Error err = ERR_CANT_CREATE;
	for (int i = 0; i < PORT_SEARCH_RANGE; i++) {
		err = tcp_server->listen(base_port + i, IPAddress("127.0.0.1"));
		if (err == OK) {
			port = base_port + i;
			break;
		}
	}
	if (err != OK) {
		tcp_server.unref();
		ERR_FAIL_V_MSG(err, vformat("OpenDust: could not bind a loopback port in %d..%d.", base_port, base_port + PORT_SEARCH_RANGE - 1));
	}

	token = _generate_hex(32);
	String dir = _project_data_dir();
	discovery_path = dir.path_join("bridge-" + _mode_name() + ".json");
	mcp_config_path = dir.path_join("mcp-" + _mode_name() + ".json");
	_write_discovery();
	_write_mcp_config();

	print_handler.printfunc = _print_handler_func;
	print_handler.userdata = this;
	add_print_handler(&print_handler);
	error_handler.errfunc = _error_handler_func;
	error_handler.userdata = this;
	add_error_handler(&error_handler);

	_register_builtin_tools();
	if (OpenDustToolRegistry::get_singleton()) {
		OpenDustToolRegistry::get_singleton()->connect("capabilities_changed", callable_mp(this, &OpenDustAgentServer::_on_capabilities_changed));
	}

	running = true;
	set_process_internal(true);
	print_line(vformat(U"OpenDust agent bridge (%s) listening on ws://127.0.0.1:%d — discovery: %s", _mode_name(), port, discovery_path));
	emit_signal(SNAME("started"), port);
	return OK;
#endif
}

void OpenDustAgentServer::stop() {
	if (!running) {
		return;
	}
	running = false;
	set_process_internal(false);

#ifdef MODULE_WEBSOCKET_ENABLED
	for (Session &s : sessions) {
		if (s.peer.is_valid()) {
			s.peer->close(1001, "server shutting down");
		}
	}
#endif
	sessions.clear();
	if (tcp_server.is_valid()) {
		tcp_server->stop();
		tcp_server.unref();
	}

	remove_print_handler(&print_handler);
	remove_error_handler(&error_handler);

	if (OpenDustToolRegistry::get_singleton()) {
		if (OpenDustToolRegistry::get_singleton()->is_connected("capabilities_changed", callable_mp(this, &OpenDustAgentServer::_on_capabilities_changed))) {
			OpenDustToolRegistry::get_singleton()->disconnect("capabilities_changed", callable_mp(this, &OpenDustAgentServer::_on_capabilities_changed));
		}
	}
	_unregister_builtin_tools();
	_remove_discovery();
	emit_signal(SNAME("stopped"));
}

bool OpenDustAgentServer::is_running() const {
	return running;
}

OpenDustAgentServer::Mode OpenDustAgentServer::get_mode() const {
	return mode;
}

int OpenDustAgentServer::get_port() const {
	return port;
}

String OpenDustAgentServer::get_token() const {
	return token;
}

String OpenDustAgentServer::get_discovery_path() const {
	return discovery_path;
}

String OpenDustAgentServer::get_mcp_config_path() const {
	return mcp_config_path;
}

void OpenDustAgentServer::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_INTERNAL_PROCESS: {
			if (running) {
				_poll();
			}
		} break;
		case NOTIFICATION_EXIT_TREE: {
			stop();
		} break;
	}
}

// ---------------------------------------------------------------------------
// Networking
// ---------------------------------------------------------------------------

void OpenDustAgentServer::_poll() {
#ifdef MODULE_WEBSOCKET_ENABLED
	_accept_pending();
	List<Session>::Element *e = sessions.front();
	while (e) {
		List<Session>::Element *next = e->next();
		_process_session(e);
		e = next;
	}
	_flush_logs();
#endif
}

void OpenDustAgentServer::_accept_pending() {
#ifdef MODULE_WEBSOCKET_ENABLED
	if (tcp_server.is_null()) {
		return;
	}
	while (tcp_server->is_connection_available()) {
		Ref<StreamPeerTCP> tcp = tcp_server->take_connection();
		if (tcp.is_null()) {
			break;
		}
		Ref<WebSocketPeer> ws = Ref<WebSocketPeer>(WebSocketPeer::create());
		if (ws.is_null()) {
			WARN_PRINT("OpenDust: WebSocketPeer implementation unavailable.");
			return;
		}
		ws->set_inbound_buffer_size(4 * 1024 * 1024);
		ws->set_outbound_buffer_size(16 * 1024 * 1024);
		ws->set_max_queued_packets(4096);
		if (ws->accept_stream(tcp) != OK) {
			continue;
		}
		Session s;
		s.peer = ws;
		s.connected_at_ms = OS::get_singleton()->get_ticks_msec();
		sessions.push_back(s);
	}
#endif
}

void OpenDustAgentServer::_process_session(List<Session>::Element *p_e) {
#ifdef MODULE_WEBSOCKET_ENABLED
	Session &s = p_e->get();
	if (s.peer.is_null()) {
		sessions.erase(p_e);
		return;
	}
	s.peer->poll();
	WebSocketPeer::State state = s.peer->get_ready_state();
	if (state == WebSocketPeer::STATE_CLOSED) {
		if (s.authenticated) {
			emit_signal(SNAME("session_disconnected"), s.session_id);
		}
		sessions.erase(p_e);
		return;
	}
	if (state != WebSocketPeer::STATE_OPEN) {
		return;
	}
	// Bound the work per frame so a chatty client can't stall the editor.
	int budget = 64;
	while (s.peer->get_available_packet_count() > 0 && budget-- > 0) {
		const uint8_t *buf = nullptr;
		int len = 0;
		Error err = s.peer->get_packet(&buf, len);
		if (err != OK) {
			break;
		}
		if (!s.peer->was_string_packet()) {
			_send_error(s, Variant(), OpenDustToolRegistry::RPC_INVALID_REQUEST, "Binary frames are not supported; send JSON text frames.");
			continue;
		}
		String text = String::utf8((const char *)buf, len);
		_handle_message(s, text);
		if (s.peer.is_null() || s.peer->get_ready_state() != WebSocketPeer::STATE_OPEN) {
			break;
		}
	}
#endif
}

void OpenDustAgentServer::_send(Session &p_session, const Variant &p_message) {
#ifdef MODULE_WEBSOCKET_ENABLED
	if (p_session.peer.is_null() || p_session.peer->get_ready_state() != WebSocketPeer::STATE_OPEN) {
		return;
	}
	p_session.peer->send_text(JSON::stringify(p_message, "", false));
#endif
}

void OpenDustAgentServer::_send_result(Session &p_session, const Variant &p_id, const Variant &p_result) {
	Dictionary msg;
	msg["jsonrpc"] = "2.0";
	msg["id"] = p_id;
	msg["result"] = OpenDustJSON::to_json(p_result);
	_send(p_session, msg);
}

void OpenDustAgentServer::_send_error(Session &p_session, const Variant &p_id, int p_code, const String &p_message, const String &p_tool, const String &p_detail) {
	Dictionary err;
	err["code"] = p_code;
	err["message"] = p_message;
	Dictionary data;
	data["tool"] = p_tool;
	data["detail"] = p_detail;
	err["data"] = data;
	Dictionary msg;
	msg["jsonrpc"] = "2.0";
	msg["id"] = p_id;
	msg["error"] = err;
	_send(p_session, msg);
}

void OpenDustAgentServer::broadcast(const String &p_method, const Dictionary &p_params) {
	if (sessions.is_empty()) {
		return;
	}
	Dictionary msg;
	msg["jsonrpc"] = "2.0";
	msg["method"] = p_method;
	msg["params"] = OpenDustJSON::to_json(p_params);
	for (Session &s : sessions) {
		if (s.authenticated) {
			_send(s, msg);
		}
	}
}

void OpenDustAgentServer::send_to(const String &p_session_id, const String &p_method, const Dictionary &p_params) {
	Dictionary msg;
	msg["jsonrpc"] = "2.0";
	msg["method"] = p_method;
	msg["params"] = OpenDustJSON::to_json(p_params);
	for (Session &s : sessions) {
		if (s.authenticated && s.session_id == p_session_id) {
			_send(s, msg);
			return;
		}
	}
}

// ---------------------------------------------------------------------------
// JSON-RPC
// ---------------------------------------------------------------------------

void OpenDustAgentServer::_handle_hello(Session &p_session, const Variant &p_id, const Dictionary &p_params) {
	String provided = p_params.get("token", "");
	if (provided.is_empty() || provided != token) {
		_send_error(p_session, p_id, OpenDustToolRegistry::RPC_NOT_AUTHENTICATED, "Bad token.", "session.hello");
#ifdef MODULE_WEBSOCKET_ENABLED
		if (p_session.peer.is_valid()) {
			p_session.peer->close(4001, "bad token");
		}
#endif
		return;
	}
	Dictionary agent = p_params.get("agent", Dictionary());
	p_session.agent_name = String(agent.get("name", "agent")).strip_edges();
	if (p_session.agent_name.is_empty()) {
		p_session.agent_name = "agent";
	}
	p_session.agent_id = agent.get("agent_id", "");
	p_session.soul_ref = agent.get("soul_ref", "");
	p_session.client = agent.get("client", "");
	p_session.session_id = "s_" + _generate_hex(8);
	p_session.authenticated = true;

	Dictionary result;
	result["session_id"] = p_session.session_id;
	result["mode"] = _mode_name();
	result["engine_version"] = get_engine_version_string();
	result["project_name"] = GLOBAL_GET("application/config/name");
	result["capabilities_version"] = OpenDustToolRegistry::get_singleton() ? OpenDustToolRegistry::get_singleton()->get_capabilities_version() : 0;
	_send_result(p_session, p_id, result);
	emit_signal(SNAME("session_connected"), _session_info(p_session));
}

bool OpenDustAgentServer::_is_tool_allowed(const Session &p_session, const String &p_tool, int p_flags, int &r_code, String &r_message) const {
	int mode_flag = mode == MODE_EDITOR ? OpenDustToolRegistry::TOOL_EDITOR : OpenDustToolRegistry::TOOL_RUNTIME;
	if ((p_flags & mode_flag) == 0) {
		r_code = OpenDustToolRegistry::RPC_WRONG_MODE;
		r_message = vformat("Tool '%s' is not available in %s mode.", p_tool, _mode_name());
		return false;
	}

	String policy = GLOBAL_GET("opendust/agent/policy");
	if (policy.is_empty()) {
		policy = mode == MODE_EDITOR ? "open" : "no_mutations";
	}
	bool mutates = p_flags & OpenDustToolRegistry::TOOL_MUTATES;
	PackedStringArray allowed = GLOBAL_GET("opendust/agent/allowed_tools");
	bool in_allowlist = allowed.has(p_tool);

	// Always-gated runtime tools regardless of policy.
	if (mode == MODE_RUNTIME && (p_tool == "world.node_call" || p_tool == "os.drive_write") && !in_allowlist) {
		r_code = OpenDustToolRegistry::RPC_DENIED;
		r_message = vformat("Tool '%s' requires an entry in opendust/agent/allowed_tools at runtime.", p_tool);
		return false;
	}

	if (policy == "open") {
		return true;
	}
	if (policy == "no_mutations") {
		if (mutates && !in_allowlist) {
			r_code = OpenDustToolRegistry::RPC_DENIED;
			r_message = vformat("Tool '%s' mutates state and the policy is no_mutations.", p_tool);
			return false;
		}
		return true;
	}
	if (policy == "allowlist") {
		if (!in_allowlist) {
			r_code = OpenDustToolRegistry::RPC_DENIED;
			r_message = vformat("Tool '%s' is not in opendust/agent/allowed_tools.", p_tool);
			return false;
		}
		return true;
	}
	r_code = OpenDustToolRegistry::RPC_DENIED;
	r_message = vformat("Unknown policy '%s'.", policy);
	return false;
}

void OpenDustAgentServer::_handle_message(Session &p_session, const String &p_text) {
	String parse_err;
	Variant parsed = OpenDustJSON::decode(p_text, &parse_err);
	if (parsed.get_type() != Variant::DICTIONARY) {
		_send_error(p_session, Variant(), OpenDustToolRegistry::RPC_PARSE_ERROR, parse_err.is_empty() ? "Expected a JSON object." : parse_err);
		return;
	}
	Dictionary msg = parsed;
	Variant id = msg.get("id", Variant());
	// Godot's JSON parser yields doubles for every number; JSON-RPC ids must be echoed as sent,
	// so integral floats go back as integers.
	if (id.get_type() == Variant::FLOAT) {
		double f = id;
		if (f == Math::floor(f) && Math::abs(f) < 9007199254740992.0) {
			id = (int64_t)f;
		}
	}
	bool is_notification = !msg.has("id");
	String method = msg.get("method", "");
	if (method.is_empty()) {
		if (!is_notification) {
			_send_error(p_session, id, OpenDustToolRegistry::RPC_INVALID_REQUEST, "Missing method.");
		}
		return;
	}
	Variant vparams = msg.get("params", Dictionary());
	if (vparams.get_type() != Variant::DICTIONARY) {
		_send_error(p_session, id, OpenDustToolRegistry::RPC_INVALID_PARAMS, "params must be an object.", method);
		return;
	}
	Dictionary params = vparams;

	if (method == "session.hello") {
		_handle_hello(p_session, id, params);
		return;
	}
	if (!p_session.authenticated) {
		_send_error(p_session, id, OpenDustToolRegistry::RPC_NOT_AUTHENTICATED, "Call session.hello first.", method);
		return;
	}

	OpenDustToolRegistry *reg = OpenDustToolRegistry::get_singleton();
	if (!reg || !reg->has_tool(method)) {
		_send_error(p_session, id, OpenDustToolRegistry::RPC_METHOD_NOT_FOUND, vformat("Unknown tool '%s'.", method), method);
		return;
	}
	int flags = reg->get_tool_flags(method);
	int code = 0;
	String why;
	if (!_is_tool_allowed(p_session, method, flags, code, why)) {
		_send_error(p_session, id, code, why, method);
		emit_signal(SNAME("tool_called"), p_session.session_id, method, params, false);
		return;
	}

	Dictionary context;
	context["session_id"] = p_session.session_id;
	context["agent_name"] = p_session.agent_name;
	context["agent_id"] = p_session.agent_id;
	context["mode"] = _mode_name();

	p_session.last_tool = method;
	p_session.tool_calls++;

	Dictionary err;
	Variant result = reg->call_tool(method, OpenDustJSON::from_json(params), context, err);
	if (!err.is_empty()) {
		_send_error(p_session, id, err.get("code", (int)OpenDustToolRegistry::RPC_INTERNAL_ERROR), err.get("message", "Tool failed."), method, err.get("detail", ""));
		emit_signal(SNAME("tool_called"), p_session.session_id, method, params, false);
		return;
	}
	if (!is_notification) {
		_send_result(p_session, id, result);
	}
	emit_signal(SNAME("tool_called"), p_session.session_id, method, params, true);
}

void OpenDustAgentServer::_on_capabilities_changed(int p_version) {
	Dictionary d;
	d["version"] = p_version;
	broadcast("event.capabilities_changed", d);
}

Dictionary OpenDustAgentServer::_session_info(const Session &p_session) const {
	Dictionary d;
	d["session_id"] = p_session.session_id;
	d["agent_name"] = p_session.agent_name;
	d["agent_id"] = p_session.agent_id;
	d["soul_ref"] = p_session.soul_ref;
	d["client"] = p_session.client;
	d["authenticated"] = p_session.authenticated;
	d["connected_at_ms"] = (int64_t)p_session.connected_at_ms;
	d["last_tool"] = p_session.last_tool;
	d["tool_calls"] = p_session.tool_calls;
	return d;
}

Array OpenDustAgentServer::get_sessions() const {
	Array out;
	for (const Session &s : sessions) {
		if (s.authenticated) {
			out.push_back(_session_info(s));
		}
	}
	return out;
}

int OpenDustAgentServer::get_session_count() const {
	int n = 0;
	for (const Session &s : sessions) {
		if (s.authenticated) {
			n++;
		}
	}
	return n;
}

// ---------------------------------------------------------------------------
// Built-in tools
// ---------------------------------------------------------------------------

Dictionary OpenDustAgentServer::_tool_engine_info(const Dictionary &p_params, const Dictionary &p_context) {
	Dictionary d;
	d["version"] = get_engine_version_string();
	d["godot_version"] = Engine::get_singleton()->get_version_info();
	d["mode"] = _mode_name();
	d["project_name"] = GLOBAL_GET("application/config/name");
	d["project_path"] = ProjectSettings::get_singleton()->get_resource_path();
	d["platform"] = OS::get_singleton()->get_name();
	d["architecture"] = Engine::get_singleton()->get_architecture_name();
	Array features;
#ifdef TOOLS_ENABLED
	features.push_back("tools");
#endif
	features.push_back("opendust_agent");
	d["features"] = features;
	d["port"] = port;
	return d;
}

Dictionary OpenDustAgentServer::_tool_engine_capabilities(const Dictionary &p_params, const Dictionary &p_context) {
	Dictionary d;
	OpenDustToolRegistry *reg = OpenDustToolRegistry::get_singleton();
	d["version"] = reg ? reg->get_capabilities_version() : 0;
	d["mode"] = _mode_name();
	int mask = mode == MODE_EDITOR ? OpenDustToolRegistry::TOOL_EDITOR : OpenDustToolRegistry::TOOL_RUNTIME;
	d["tools"] = reg ? reg->list_tools(mask) : Array();
	return d;
}

Dictionary OpenDustAgentServer::_tool_engine_ping(const Dictionary &p_params, const Dictionary &p_context) {
	Dictionary d;
	d["time_ms"] = (int64_t)OS::get_singleton()->get_ticks_msec();
	return d;
}

Dictionary OpenDustAgentServer::_tool_engine_log_tail(const Dictionary &p_params, const Dictionary &p_context) {
	int lines = p_params.get("lines", 100);
	Dictionary d;
	d["lines"] = get_log_tail(lines);
	return d;
}

void OpenDustAgentServer::_register_builtin_tools() {
	OpenDustToolRegistry *reg = OpenDustToolRegistry::get_singleton();
	ERR_FAIL_NULL(reg);
	const int both = OpenDustToolRegistry::TOOL_EDITOR | OpenDustToolRegistry::TOOL_RUNTIME;

	reg->register_tool("engine.info", "Engine, project and platform information.",
			OpenDustToolRegistry::object_schema(Dictionary()),
			callable_mp(this, &OpenDustAgentServer::_tool_engine_info), both);
	reg->register_tool("engine.capabilities", "List every tool available in the current mode with its JSON schema.",
			OpenDustToolRegistry::object_schema(Dictionary()),
			callable_mp(this, &OpenDustAgentServer::_tool_engine_capabilities), both);
	reg->register_tool("engine.ping", "Round-trip check; returns engine time in milliseconds.",
			OpenDustToolRegistry::object_schema(Dictionary()),
			callable_mp(this, &OpenDustAgentServer::_tool_engine_ping), both);
	Dictionary log_props;
	log_props["lines"] = OpenDustToolRegistry::prop("integer", "How many recent lines to return (default 100).");
	reg->register_tool("editor.log_tail", "Recent print/warning/error output from this process.",
			OpenDustToolRegistry::object_schema(log_props),
			callable_mp(this, &OpenDustAgentServer::_tool_engine_log_tail), both);
}

void OpenDustAgentServer::_unregister_builtin_tools() {
	OpenDustToolRegistry *reg = OpenDustToolRegistry::get_singleton();
	if (!reg) {
		return;
	}
	reg->unregister_tool("engine.info");
	reg->unregister_tool("engine.capabilities");
	reg->unregister_tool("engine.ping");
	reg->unregister_tool("editor.log_tail");
}

// ---------------------------------------------------------------------------

void OpenDustAgentServer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("start", "mode"), &OpenDustAgentServer::start);
	ClassDB::bind_method(D_METHOD("stop"), &OpenDustAgentServer::stop);
	ClassDB::bind_method(D_METHOD("is_running"), &OpenDustAgentServer::is_running);
	ClassDB::bind_method(D_METHOD("get_mode"), &OpenDustAgentServer::get_mode);
	ClassDB::bind_method(D_METHOD("get_port"), &OpenDustAgentServer::get_port);
	ClassDB::bind_method(D_METHOD("get_discovery_path"), &OpenDustAgentServer::get_discovery_path);
	ClassDB::bind_method(D_METHOD("get_mcp_config_path"), &OpenDustAgentServer::get_mcp_config_path);
	ClassDB::bind_method(D_METHOD("get_engine_version_string"), &OpenDustAgentServer::get_engine_version_string);
	ClassDB::bind_method(D_METHOD("broadcast", "method", "params"), &OpenDustAgentServer::broadcast);
	ClassDB::bind_method(D_METHOD("send_to", "session_id", "method", "params"), &OpenDustAgentServer::send_to);
	ClassDB::bind_method(D_METHOD("get_sessions"), &OpenDustAgentServer::get_sessions);
	ClassDB::bind_method(D_METHOD("get_session_count"), &OpenDustAgentServer::get_session_count);
	ClassDB::bind_method(D_METHOD("get_log_tail", "lines"), &OpenDustAgentServer::get_log_tail, DEFVAL(100));

	ADD_SIGNAL(MethodInfo("started", PropertyInfo(Variant::INT, "port")));
	ADD_SIGNAL(MethodInfo("stopped"));
	ADD_SIGNAL(MethodInfo("session_connected", PropertyInfo(Variant::DICTIONARY, "session")));
	ADD_SIGNAL(MethodInfo("session_disconnected", PropertyInfo(Variant::STRING, "session_id")));
	ADD_SIGNAL(MethodInfo("tool_called", PropertyInfo(Variant::STRING, "session_id"), PropertyInfo(Variant::STRING, "tool"), PropertyInfo(Variant::DICTIONARY, "params"), PropertyInfo(Variant::BOOL, "ok")));

	BIND_ENUM_CONSTANT(MODE_EDITOR);
	BIND_ENUM_CONSTANT(MODE_RUNTIME);
}

OpenDustAgentServer::OpenDustAgentServer() {
	singleton = this;
	set_process_mode(PROCESS_MODE_ALWAYS);
	set_name("OpenDustAgentServer");
}

OpenDustAgentServer::~OpenDustAgentServer() {
	if (running) {
		// Tear down without emitting signals; the object is going away.
		running = false;
		sessions.clear();
		if (tcp_server.is_valid()) {
			tcp_server->stop();
			tcp_server.unref();
		}
		remove_print_handler(&print_handler);
		remove_error_handler(&error_handler);
		_unregister_builtin_tools();
		_remove_discovery();
	}
	if (singleton == this) {
		singleton = nullptr;
	}
}
