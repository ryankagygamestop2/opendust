/**************************************************************************/
/*  opendust_agent_server.h                                               */
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

#include "core/io/tcp_server.h"
#include "core/os/mutex.h"
#include "core/string/print_string.h"
#include "core/templates/list.h"
#include "scene/main/node.h"

#include "modules/modules_enabled.gen.h" // For websocket.

#ifdef MODULE_WEBSOCKET_ENABLED
#include "modules/websocket/websocket_peer.h"
#endif

class OpenDustToolRegistry;

// JSON-RPC 2.0 over WebSocket, loopback only. One instance per process:
// the editor runs one in MODE_EDITOR, a running game runs one in MODE_RUNTIME.
// See docs/opendust/01-agent-bridge-protocol.md.
class OpenDustAgentServer : public Node {
	GDCLASS(OpenDustAgentServer, Node);

public:
	enum Mode {
		MODE_EDITOR,
		MODE_RUNTIME,
	};

	static const int DEFAULT_EDITOR_PORT = 6120;
	static const int DEFAULT_RUNTIME_PORT = 6121;
	static const int PORT_SEARCH_RANGE = 16;
	static const int LOG_RING_SIZE = 500;

	struct Session {
		String session_id;
		String agent_name;
		String agent_id;
		String soul_ref;
		String client;
		bool authenticated = false;
		uint64_t connected_at_ms = 0;
		String last_tool;
		int tool_calls = 0;
#ifdef MODULE_WEBSOCKET_ENABLED
		Ref<WebSocketPeer> peer;
#endif
	};

private:
	static OpenDustAgentServer *singleton;

	Mode mode = MODE_EDITOR;
	bool running = false;
	int port = 0;
	String token;
	String discovery_path;
	String mcp_config_path;
	Ref<TCPServer> tcp_server;
	List<Session> sessions;

	// Log capture (print + error handlers can fire from any thread).
	Mutex log_mutex;
	List<Dictionary> log_pending;
	List<Dictionary> log_ring;
	PrintHandlerList print_handler;
	ErrorHandlerList error_handler;
	static void _print_handler_func(void *p_this, const String &p_string, bool p_error, bool p_rich);
	static void _error_handler_func(void *p_this, const char *p_func, const char *p_file, int p_line, const char *p_error, const char *p_errorexp, bool p_editor_notify, ErrorHandlerType p_type);
	void _queue_log(const String &p_level, const String &p_text, const String &p_source);
	void _flush_logs();

	String _mode_name() const;
	String _generate_hex(int p_bytes) const;
	String _project_data_dir() const;
	void _write_discovery();
	void _remove_discovery();
	void _write_mcp_config();

	void _poll();
	void _accept_pending();
	void _process_session(List<Session>::Element *p_e);
	void _handle_message(Session &p_session, const String &p_text);
	void _handle_hello(Session &p_session, const Variant &p_id, const Dictionary &p_params);
	bool _is_tool_allowed(const Session &p_session, const String &p_tool, int p_flags, int &r_code, String &r_message) const;

	void _send(Session &p_session, const Variant &p_message);
	void _send_result(Session &p_session, const Variant &p_id, const Variant &p_result);
	void _send_error(Session &p_session, const Variant &p_id, int p_code, const String &p_message, const String &p_tool = String(), const String &p_detail = String());

	void _on_capabilities_changed(int p_version);

	Dictionary _session_info(const Session &p_session) const;

	// Built-in engine.* tools.
	Dictionary _tool_engine_info(const Dictionary &p_params, const Dictionary &p_context);
	Dictionary _tool_engine_capabilities(const Dictionary &p_params, const Dictionary &p_context);
	Dictionary _tool_engine_ping(const Dictionary &p_params, const Dictionary &p_context);
	Dictionary _tool_engine_log_tail(const Dictionary &p_params, const Dictionary &p_context);
	void _register_builtin_tools();
	void _unregister_builtin_tools();

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	static OpenDustAgentServer *get_singleton();

	Error start(Mode p_mode);
	void stop();
	bool is_running() const;
	Mode get_mode() const;
	int get_port() const;
	String get_token() const;
	String get_discovery_path() const;
	String get_mcp_config_path() const;
	String get_engine_version_string() const;

	void broadcast(const String &p_method, const Dictionary &p_params);
	void send_to(const String &p_session_id, const String &p_method, const Dictionary &p_params);
	Array get_sessions() const;
	int get_session_count() const;
	Array get_log_tail(int p_lines) const;

	OpenDustAgentServer();
	~OpenDustAgentServer();
};

VARIANT_ENUM_CAST(OpenDustAgentServer::Mode);
