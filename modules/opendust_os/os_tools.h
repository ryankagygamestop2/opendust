/**************************************************************************/
/*  os_tools.h                                                            */
/**************************************************************************/
/*                         This file is part of:                          */
/*                              OPENDUST                                  */
/*                         https://opendust.io                            */
/**************************************************************************/
/* OpenDust is a fork of Godot Engine. MIT licensed; see LICENSE.txt.     */
/**************************************************************************/

#pragma once

#include "core/object/object.h"

// Registers the os.* tools on OpenDustToolRegistry (runtime mode) when the
// opendust_agent module is present. See docs/opendust/01-agent-bridge-protocol.md.
class OSTools : public Object {
	GDCLASS(OSTools, Object);

	static OSTools *singleton;

	Dictionary _err(int p_code, const String &p_message) const;
	Dictionary _no_kernel() const;

	Dictionary _devices(const Dictionary &p_params, const Dictionary &p_context);
	Dictionary _open_app(const Dictionary &p_params, const Dictionary &p_context);
	Dictionary _drive_list(const Dictionary &p_params, const Dictionary &p_context);
	Dictionary _drive_read(const Dictionary &p_params, const Dictionary &p_context);
	Dictionary _drive_write(const Dictionary &p_params, const Dictionary &p_context);
	Dictionary _message(const Dictionary &p_params, const Dictionary &p_context);

	void _register_all();
	void _unregister_all();

protected:
	static void _bind_methods() {}

public:
	static void initialize();
	static void finalize();

	OSTools() {}
};
