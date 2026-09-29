/**************************************************************************/
/*  os_apps.h                                                             */
/**************************************************************************/
/*                         This file is part of:                          */
/*                              OPENDUST                                  */
/*                         https://opendust.io                            */
/**************************************************************************/
/* OpenDust is a fork of Godot Engine. MIT licensed; see LICENSE.txt.     */
/**************************************************************************/

#pragma once

#include "os_app_context.h"
#include "scene/gui/control.h"

class Button;
class GridContainer;
class LineEdit;
class RichTextLabel;
class TextEdit;
class Timer;
class Tree;
class Label;

// Base for apps. Scenes may extend this in GDScript and override `_setup`;
// C++ apps override `setup()`.
class OSApp : public Control {
	GDCLASS(OSApp, Control);

protected:
	Ref<OSAppContext> ctx;

	static void _bind_methods();
	GDVIRTUAL1(_setup, Ref<OSAppContext>)

public:
	virtual void setup(const Ref<OSAppContext> &p_ctx);
	Ref<OSAppContext> get_context() const;

	OSApp() {}
};

// os.home — app grid.
class OSHomeApp : public OSApp {
	GDCLASS(OSHomeApp, OSApp);

	GridContainer *grid = nullptr;

	void _rebuild();
	void _launch(const String &p_app_id);

protected:
	static void _bind_methods() {}

public:
	static Control *create();
	virtual void setup(const Ref<OSAppContext> &p_ctx) override;

	OSHomeApp();
};

// os.files — the world drive as a tree; text files open in os.notes.
class OSFilesApp : public OSApp {
	GDCLASS(OSFilesApp, OSApp);

	Tree *tree = nullptr;
	Label *root_label = nullptr;

	void _populate(class TreeItem *p_parent, const String &p_dir, int p_depth);
	void _refresh();
	void _on_drive_changed(const String &p_path);
	void _on_activated();
	void _on_new_note();

protected:
	void _notification(int p_what);
	static void _bind_methods() {}

public:
	static Control *create();
	virtual void setup(const Ref<OSAppContext> &p_ctx) override;

	OSFilesApp();
};

// os.notes — a text editor with autosave to the drive.
class OSNotesApp : public OSApp {
	GDCLASS(OSNotesApp, OSApp);

	Label *path_label = nullptr;
	TextEdit *editor = nullptr;
	Timer *debounce = nullptr;
	String path;
	bool loading = false;

	void _on_text_changed();
	void _save();

protected:
	static void _bind_methods() {}

public:
	static Control *create();
	virtual void setup(const Ref<OSAppContext> &p_ctx) override;
	void open_path(const String &p_path);
	String get_path() const;

	OSNotesApp();
};

// os.messages — the kernel bus as a chat.
class OSMessagesApp : public OSApp {
	GDCLASS(OSMessagesApp, OSApp);

	RichTextLabel *log = nullptr;
	LineEdit *to_edit = nullptr;
	LineEdit *body_edit = nullptr;
	Button *send_button = nullptr;

	void _append(const Dictionary &p_msg);
	void _on_message(const Dictionary &p_msg);
	void _on_send();
	void _on_submitted(const String &p_text);

protected:
	void _notification(int p_what);
	static void _bind_methods() {}

public:
	static Control *create();
	virtual void setup(const Ref<OSAppContext> &p_ctx) override;

	OSMessagesApp();
};
