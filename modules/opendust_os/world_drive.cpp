/**************************************************************************/
/*  world_drive.cpp                                                       */
/**************************************************************************/
/*                         This file is part of:                          */
/*                              OPENDUST                                  */
/*                         https://opendust.io                            */
/**************************************************************************/
/* OpenDust is a fork of Godot Engine. MIT licensed; see LICENSE.txt.     */
/**************************************************************************/

#include "world_drive.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"

String WorldDrive::strip_scheme(const String &p_path) {
	String p = p_path.replace("\\", "/");
	if (p.begins_with("drive://")) {
		// drive://<world>/rest → rest. The world segment is not checked here;
		// the kernel hands each device the drive for its own world.
		String rest = p.substr(8);
		int slash = rest.find("/");
		p = slash < 0 ? String() : rest.substr(slash + 1);
	}
	return p;
}

String WorldDrive::_resolve(const String &p_path) const {
	if (root.is_empty()) {
		return String();
	}
	String rel = strip_scheme(p_path);
	// Absolute paths, drive letters, schemes and user:// / res:// are all
	// rejected outright: the drive is addressed relatively, always.
	if (rel.begins_with("/") || rel.contains(":") || rel.is_absolute_path()) {
		return String();
	}
	rel = rel.simplify_path();
	if (rel == "." || rel.is_empty()) {
		return root;
	}
	if (rel == ".." || rel.begins_with("../") || rel.contains("/../") || rel.ends_with("/..")) {
		return String();
	}
	String abs = root.path_join(rel).simplify_path();
	// Belt and braces: the result must still be inside the root.
	if (abs != root && !abs.begins_with(root + "/")) {
		return String();
	}
	return abs;
}

void WorldDrive::set_root(const String &p_root) {
	String r = p_root.replace("\\", "/").simplify_path();
	while (r.length() > 1 && r.ends_with("/")) {
		r = r.substr(0, r.length() - 1);
	}
	root = r;
}

String WorldDrive::get_root() const {
	return root;
}

bool WorldDrive::is_valid_path(const String &p_path) const {
	return !_resolve(p_path).is_empty();
}

String WorldDrive::to_absolute(const String &p_path) const {
	return _resolve(p_path);
}

bool WorldDrive::exists(const String &p_path) const {
	String abs = _resolve(p_path);
	if (abs.is_empty()) {
		return false;
	}
	return FileAccess::exists(abs) || DirAccess::dir_exists_absolute(abs);
}

bool WorldDrive::is_dir(const String &p_path) const {
	String abs = _resolve(p_path);
	return !abs.is_empty() && DirAccess::dir_exists_absolute(abs);
}

Array WorldDrive::list(const String &p_dir) const {
	Array out;
	String abs = _resolve(p_dir);
	if (abs.is_empty() || !DirAccess::dir_exists_absolute(abs)) {
		return out;
	}
	String rel = strip_scheme(p_dir).simplify_path();
	if (rel == ".") {
		rel = "";
	}
	PackedStringArray dirs = DirAccess::get_directories_at(abs);
	for (const String &d : dirs) {
		Dictionary e;
		e["name"] = d;
		e["path"] = rel.is_empty() ? d : rel.path_join(d);
		e["is_dir"] = true;
		e["size"] = 0;
		out.push_back(e);
	}
	PackedStringArray files = DirAccess::get_files_at(abs);
	for (const String &f : files) {
		Dictionary e;
		e["name"] = f;
		e["path"] = rel.is_empty() ? f : rel.path_join(f);
		e["is_dir"] = false;
		uint64_t size = 0;
		Ref<FileAccess> fa = FileAccess::open(abs.path_join(f), FileAccess::READ);
		if (fa.is_valid()) {
			size = fa->get_length();
		}
		e["size"] = size;
		out.push_back(e);
	}
	return out;
}

PackedStringArray WorldDrive::list_names(const String &p_dir) const {
	PackedStringArray names;
	Array entries = list(p_dir);
	for (int i = 0; i < entries.size(); i++) {
		Dictionary e = entries[i];
		names.push_back(e["name"]);
	}
	return names;
}

String WorldDrive::read_text(const String &p_path) const {
	String abs = _resolve(p_path);
	if (abs.is_empty() || !FileAccess::exists(abs)) {
		return String();
	}
	return FileAccess::get_file_as_string(abs);
}

PackedByteArray WorldDrive::read_bytes(const String &p_path) const {
	String abs = _resolve(p_path);
	if (abs.is_empty() || !FileAccess::exists(abs)) {
		return PackedByteArray();
	}
	return FileAccess::get_file_as_bytes(abs);
}

Error WorldDrive::write_text(const String &p_path, const String &p_text) {
	String abs = _resolve(p_path);
	ERR_FAIL_COND_V_MSG(abs.is_empty() || abs == root, ERR_INVALID_PARAMETER, "WorldDrive: refused path '" + p_path + "'.");
	Error err = DirAccess::make_dir_recursive_absolute(abs.get_base_dir());
	if (err != OK && err != ERR_ALREADY_EXISTS) {
		return err;
	}
	Ref<FileAccess> f = FileAccess::open(abs, FileAccess::WRITE, &err);
	if (f.is_null()) {
		return err == OK ? ERR_CANT_OPEN : err;
	}
	f->store_string(p_text);
	f.unref();
	emit_signal(SNAME("changed"), strip_scheme(p_path).simplify_path());
	return OK;
}

Error WorldDrive::write_bytes(const String &p_path, const PackedByteArray &p_bytes) {
	String abs = _resolve(p_path);
	ERR_FAIL_COND_V_MSG(abs.is_empty() || abs == root, ERR_INVALID_PARAMETER, "WorldDrive: refused path '" + p_path + "'.");
	Error err = DirAccess::make_dir_recursive_absolute(abs.get_base_dir());
	if (err != OK && err != ERR_ALREADY_EXISTS) {
		return err;
	}
	Ref<FileAccess> f = FileAccess::open(abs, FileAccess::WRITE, &err);
	if (f.is_null()) {
		return err == OK ? ERR_CANT_OPEN : err;
	}
	f->store_buffer(p_bytes);
	f.unref();
	emit_signal(SNAME("changed"), strip_scheme(p_path).simplify_path());
	return OK;
}

Error WorldDrive::make_dir(const String &p_path) {
	String abs = _resolve(p_path);
	ERR_FAIL_COND_V_MSG(abs.is_empty(), ERR_INVALID_PARAMETER, "WorldDrive: refused path '" + p_path + "'.");
	Error err = DirAccess::make_dir_recursive_absolute(abs);
	if (err == OK) {
		emit_signal(SNAME("changed"), strip_scheme(p_path).simplify_path());
	}
	return err;
}

Error WorldDrive::remove(const String &p_path) {
	String abs = _resolve(p_path);
	ERR_FAIL_COND_V_MSG(abs.is_empty() || abs == root, ERR_INVALID_PARAMETER, "WorldDrive: refused path '" + p_path + "'.");
	Error err = DirAccess::remove_absolute(abs);
	if (err == OK) {
		emit_signal(SNAME("changed"), strip_scheme(p_path).simplify_path());
	}
	return err;
}

void WorldDrive::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_root", "root"), &WorldDrive::set_root);
	ClassDB::bind_method(D_METHOD("get_root"), &WorldDrive::get_root);
	ClassDB::bind_method(D_METHOD("is_valid_path", "path"), &WorldDrive::is_valid_path);
	ClassDB::bind_method(D_METHOD("to_absolute", "path"), &WorldDrive::to_absolute);
	ClassDB::bind_method(D_METHOD("exists", "path"), &WorldDrive::exists);
	ClassDB::bind_method(D_METHOD("is_dir", "path"), &WorldDrive::is_dir);
	ClassDB::bind_method(D_METHOD("list", "dir"), &WorldDrive::list);
	ClassDB::bind_method(D_METHOD("list_names", "dir"), &WorldDrive::list_names);
	ClassDB::bind_method(D_METHOD("read_text", "path"), &WorldDrive::read_text);
	ClassDB::bind_method(D_METHOD("read_bytes", "path"), &WorldDrive::read_bytes);
	ClassDB::bind_method(D_METHOD("write_text", "path", "text"), &WorldDrive::write_text);
	ClassDB::bind_method(D_METHOD("write_bytes", "path", "bytes"), &WorldDrive::write_bytes);
	ClassDB::bind_method(D_METHOD("make_dir", "path"), &WorldDrive::make_dir);
	ClassDB::bind_method(D_METHOD("remove", "path"), &WorldDrive::remove);
	ClassDB::bind_static_method("WorldDrive", D_METHOD("strip_scheme", "path"), &WorldDrive::strip_scheme);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "root"), "set_root", "get_root");
	ADD_SIGNAL(MethodInfo("changed", PropertyInfo(Variant::STRING, "path")));
}
