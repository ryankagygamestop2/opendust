/**************************************************************************/
/*  os_app_context.h                                                      */
/**************************************************************************/
/*                         This file is part of:                          */
/*                              OPENDUST                                  */
/*                         https://opendust.io                            */
/**************************************************************************/
/* OpenDust is a fork of Godot Engine. MIT licensed; see LICENSE.txt.     */
/**************************************************************************/

#pragma once

#include "core/object/ref_counted.h"
#include "world_drive.h"

class OSKernel;
class OSShell;

// Handed to every app when it is opened. Apps ask this for the session and
// the drive; they never talk to identity or storage directly.
class OSAppContext : public RefCounted {
	GDCLASS(OSAppContext, RefCounted);

	ObjectID kernel_id;
	ObjectID device_id;
	ObjectID shell_id;
	Dictionary session;
	Ref<WorldDrive> drive;
	Dictionary args;

protected:
	static void _bind_methods();

public:
	void setup(OSKernel *p_kernel, Node *p_device, OSShell *p_shell, const Dictionary &p_session, const Ref<WorldDrive> &p_drive, const Dictionary &p_args);

	OSKernel *get_kernel() const;
	Node *get_device() const;
	OSShell *get_shell() const;
	Dictionary get_session() const;
	Ref<WorldDrive> get_drive() const;
	Dictionary get_args() const;

	// Open a `$SKU` item (placeholder for now, see 05-identity.md) or a drive
	// path (text files open in os.notes).
	void open(const String &p_sku_or_path);
	void open_app(const String &p_app_id, const Dictionary &p_args = Dictionary());
	void notify(const String &p_text);

	OSAppContext() {}
};
