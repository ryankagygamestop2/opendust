/**************************************************************************/
/*  world_drive.h                                                         */
/**************************************************************************/
/*                         This file is part of:                          */
/*                              OPENDUST                                  */
/*                         https://opendust.io                            */
/**************************************************************************/
/* OpenDust is a fork of Godot Engine. MIT licensed; see LICENSE.txt.     */
/**************************************************************************/

#pragma once

#include "core/object/ref_counted.h"

// The world drive: a filesystem namespace shared by every device in a world,
// backed by a real directory. `drive://<world>/notes/a.md` maps to
// `<root>/notes/a.md`. Escaping the root is unauthorable: every path goes
// through `_resolve()`, which returns an empty string on any violation, and
// every method fails closed on an empty resolution.
class WorldDrive : public RefCounted {
	GDCLASS(WorldDrive, RefCounted);

	String root; // Absolute, forward slashes, no trailing slash.

	// Returns the absolute path for a drive-relative path, or "" if the path
	// is absolute, contains a drive letter/scheme, or would leave the root.
	String _resolve(const String &p_path) const;

protected:
	static void _bind_methods();

public:
	void set_root(const String &p_root);
	String get_root() const;

	// Strips an optional `drive://<world>/` prefix, normalizes separators.
	static String strip_scheme(const String &p_path);

	bool is_valid_path(const String &p_path) const;
	String to_absolute(const String &p_path) const; // "" on violation.

	bool exists(const String &p_path) const;
	bool is_dir(const String &p_path) const;
	Array list(const String &p_dir) const; // [{name, path, is_dir, size}]
	PackedStringArray list_names(const String &p_dir) const;

	String read_text(const String &p_path) const;
	PackedByteArray read_bytes(const String &p_path) const;
	Error write_text(const String &p_path, const String &p_text);
	Error write_bytes(const String &p_path, const PackedByteArray &p_bytes);
	Error make_dir(const String &p_path);
	Error remove(const String &p_path);

	WorldDrive() {}
};
