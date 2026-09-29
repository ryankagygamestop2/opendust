/**************************************************************************/
/*  os_tools.cpp                                                          */
/**************************************************************************/
/*                         This file is part of:                          */
/*                              OPENDUST                                  */
/*                         https://opendust.io                            */
/**************************************************************************/
/* OpenDust is a fork of Godot Engine. MIT licensed; see LICENSE.txt.     */
/**************************************************************************/

#include "os_tools.h"
#include "core/object/callable_mp.h"

#include "os_kernel.h"

#include "core/io/json.h"
#include "modules/modules_enabled.gen.h" // For MODULE_OPENDUST_AGENT_ENABLED.

#ifdef MODULE_OPENDUST_AGENT_ENABLED
#include "modules/opendust_agent/opendust_tool_registry.h"
#endif

OSTools *OSTools::singleton = nullptr;

void OSTools::initialize() {
	if (!singleton) {
		singleton = memnew(OSTools);
		singleton->_register_all();
	}
}

void OSTools::finalize() {
	if (singleton) {
		singleton->_unregister_all();
		memdelete(singleton);
		singleton = nullptr;
	}
}

Dictionary OSTools::_err(int p_code, const String &p_message) const {
	Dictionary e;
	e["code"] = p_code;
	e["message"] = p_message;
	Dictionary out;
	out["$error"] = e;
	return out;
}

Dictionary OSTools::_no_kernel() const {
	return _err(-32003, "No OpenDust OS kernel in this world (no Device has entered the tree).");
}

Dictionary OSTools::_devices(const Dictionary &p_params, const Dictionary &p_context) {
	OSKernel *k = OSKernel::get_singleton();
	if (!k) {
		return _no_kernel();
	}
	Dictionary out;
	out["world_id"] = k->get_world_id();
	out["devices"] = k->get_devices();
	return out;
}

Dictionary OSTools::_open_app(const Dictionary &p_params, const Dictionary &p_context) {
	OSKernel *k = OSKernel::get_singleton();
	if (!k) {
		return _no_kernel();
	}
	String device_id = p_params.get("device_id", "");
	String app_id = p_params.get("app_id", "");
	Node *dev = k->get_device(device_id);
	if (!dev) {
		return _err(-32003, "No device '" + device_id + "'.");
	}
	if (!k->has_app(app_id)) {
		return _err(-32003, "No app '" + app_id + "'.");
	}
	Dictionary args = p_params.get("args", Dictionary());
	dev->call("open_app", app_id, args);
	Dictionary out;
	out["ok"] = true;
	return out;
}

Dictionary OSTools::_drive_list(const Dictionary &p_params, const Dictionary &p_context) {
	OSKernel *k = OSKernel::get_singleton();
	if (!k) {
		return _no_kernel();
	}
	String path = p_params.get("path", "");
	if (!k->get_drive()->is_valid_path(path)) {
		return _err(-32602, "Invalid drive path '" + path + "'.");
	}
	Dictionary out;
	out["path"] = path;
	out["entries"] = k->get_drive()->list(path);
	return out;
}

Dictionary OSTools::_drive_read(const Dictionary &p_params, const Dictionary &p_context) {
	OSKernel *k = OSKernel::get_singleton();
	if (!k) {
		return _no_kernel();
	}
	String path = p_params.get("path", "");
	if (!k->get_drive()->is_valid_path(path) || !k->get_drive()->exists(path) || k->get_drive()->is_dir(path)) {
		return _err(-32003, "No file at drive path '" + path + "'.");
	}
	int max_bytes = p_params.get("max_bytes", 262144);
	String text = k->get_drive()->read_text(path);
	Dictionary out;
	out["path"] = path;
	out["truncated"] = text.length() > max_bytes;
	out["text"] = text.length() > max_bytes ? text.substr(0, max_bytes) : text;
	return out;
}

Dictionary OSTools::_drive_write(const Dictionary &p_params, const Dictionary &p_context) {
	OSKernel *k = OSKernel::get_singleton();
	if (!k) {
		return _no_kernel();
	}
	String path = p_params.get("path", "");
	String text = p_params.get("text", "");
	Error err = k->get_drive()->write_text(path, text);
	if (err != OK) {
		return _err(-32004, "Could not write drive path '" + path + "' (error " + itos(err) + ").");
	}
	Dictionary out;
	out["path"] = path;
	out["bytes"] = text.utf8().length();
	return out;
}

Dictionary OSTools::_message(const Dictionary &p_params, const Dictionary &p_context) {
	OSKernel *k = OSKernel::get_singleton();
	if (!k) {
		return _no_kernel();
	}
	Dictionary msg;
	msg["to"] = String(p_params.get("to", ""));
	msg["body"] = String(p_params.get("body", ""));
	String from = p_context.get("agent_name", "");
	msg["from"] = from.is_empty() ? "agent" : from;
	if (p_context.has("agent_id") && !String(p_context["agent_id"]).is_empty()) {
		msg["from_agent_id"] = p_context["agent_id"];
	}
	return k->send(msg);
}

void OSTools::_register_all() {
#ifdef MODULE_OPENDUST_AGENT_ENABLED
	OpenDustToolRegistry *reg = OpenDustToolRegistry::get_singleton();
	if (!reg) {
		WARN_PRINT("opendust_os: OpenDustToolRegistry not available; os.* tools not registered.");
		return;
	}
	const int RT = OpenDustToolRegistry::TOOL_RUNTIME;
	const int RTM = OpenDustToolRegistry::TOOL_RUNTIME | OpenDustToolRegistry::TOOL_MUTATES;

	reg->register_tool("os.devices", "List OpenDust OS devices in the running world.",
			JSON::parse_string(R"({"type":"object","properties":{}})"),
			callable_mp(this, &OSTools::_devices), RT);
	reg->register_tool("os.open_app", "Open an app on a device.",
			JSON::parse_string(R"({"type":"object","properties":{"device_id":{"type":"string"},"app_id":{"type":"string"},"args":{"type":"object"}},"required":["device_id","app_id"]})"),
			callable_mp(this, &OSTools::_open_app), RTM);
	reg->register_tool("os.drive_list", "List a directory on the world drive (drive-relative path; \"\" for the root).",
			JSON::parse_string(R"({"type":"object","properties":{"path":{"type":"string","default":""}}})"),
			callable_mp(this, &OSTools::_drive_list), RT);
	reg->register_tool("os.drive_read", "Read a text file from the world drive.",
			JSON::parse_string(R"({"type":"object","properties":{"path":{"type":"string"},"max_bytes":{"type":"integer","default":262144}},"required":["path"]})"),
			callable_mp(this, &OSTools::_drive_read), RT);
	reg->register_tool("os.drive_write", "Write a text file to the world drive. Policy-gated at runtime.",
			JSON::parse_string(R"({"type":"object","properties":{"path":{"type":"string"},"text":{"type":"string"}},"required":["path","text"]})"),
			callable_mp(this, &OSTools::_drive_write), RTM);
	reg->register_tool("os.message", "Send a message on the world bus to a durable id (device id, user, or agent).",
			JSON::parse_string(R"({"type":"object","properties":{"to":{"type":"string"},"body":{"type":"string"}},"required":["to","body"]})"),
			callable_mp(this, &OSTools::_message), RTM);
#endif
}

void OSTools::_unregister_all() {
#ifdef MODULE_OPENDUST_AGENT_ENABLED
	OpenDustToolRegistry *reg = OpenDustToolRegistry::get_singleton();
	if (!reg) {
		return;
	}
	const char *names[] = { "os.devices", "os.open_app", "os.drive_list", "os.drive_read", "os.drive_write", "os.message" };
	for (const char *n : names) {
		if (reg->has_tool(n)) {
			reg->unregister_tool(n);
		}
	}
#endif
}
