/**************************************************************************/
/*  device.h                                                              */
/**************************************************************************/
/*                         This file is part of:                          */
/*                              OPENDUST                                  */
/*                         https://opendust.io                            */
/**************************************************************************/
/* OpenDust is a fork of Godot Engine. MIT licensed; see LICENSE.txt.     */
/**************************************************************************/

#pragma once

#include "os_shell.h"
#include "scene/3d/node_3d.h"
#include "scene/gui/control.h"

class MeshInstance3D;
class QuadMesh;
class StandardMaterial3D;
class SubViewport;

// A device in a 3D world: a screen that boots OpenDust OS. Owns a SubViewport
// with an OSShell and a quad that displays it. Registers with the OSKernel
// (creating it if this is the first device in the world).
class Device : public Node3D {
	GDCLASS(Device, Node3D);

	String device_id;
	Vector2i screen_size_px = Vector2i(1024, 768);
	Vector2 screen_size = Vector2(0.32, 0.24);
	String boot_app = "os.home";
	bool awake = true;

	SubViewport *viewport = nullptr;
	OSShell *shell = nullptr;
	MeshInstance3D *screen = nullptr;
	Ref<QuadMesh> quad;
	Ref<StandardMaterial3D> material;

	void _apply_awake();

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	void set_device_id(const String &p_id);
	String get_device_id() const;
	void set_screen_size_px(const Vector2i &p_size);
	Vector2i get_screen_size_px() const;
	void set_screen_size(const Vector2 &p_size);
	Vector2 get_screen_size() const;
	void set_boot_app(const String &p_app);
	String get_boot_app() const;

	Dictionary get_session() const;
	String get_room_id() const;
	void open_app(const String &p_app_id, const Dictionary &p_args = Dictionary());
	bool is_awake() const;
	void wake();
	void sleep();
	void receive_message(const Dictionary &p_message);
	void push_input(const Ref<InputEvent> &p_event);

	OSShell *get_shell() const;
	SubViewport *get_screen_viewport() const;

	Device();
};

// A device drawn directly in a 2D world or a HUD.
class Device2D : public Control {
	GDCLASS(Device2D, Control);

	String device_id;
	String boot_app = "os.home";
	bool awake = true;
	OSShell *shell = nullptr;

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	void set_device_id(const String &p_id);
	String get_device_id() const;
	void set_boot_app(const String &p_app);
	String get_boot_app() const;

	Dictionary get_session() const;
	String get_room_id() const;
	void open_app(const String &p_app_id, const Dictionary &p_args = Dictionary());
	bool is_awake() const;
	void wake();
	void sleep();
	void receive_message(const Dictionary &p_message);
	OSShell *get_shell() const;

	Device2D();
};
