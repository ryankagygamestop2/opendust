/**************************************************************************/
/*  os_apps.cpp                                                           */
/**************************************************************************/
/*                         This file is part of:                          */
/*                              OPENDUST                                  */
/*                         https://opendust.io                            */
/**************************************************************************/
/* OpenDust is a fork of Godot Engine. MIT licensed; see LICENSE.txt.     */
/**************************************************************************/

#include "os_apps.h"

#include "os_kernel.h"

#include "core/os/time.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/grid_container.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/rich_text_label.h"
#include "scene/gui/text_edit.h"
#include "scene/gui/tree.h"
#include "scene/main/timer.h"

// --- OSApp -----------------------------------------------------------------

void OSApp::setup(const Ref<OSAppContext> &p_ctx) {
	ctx = p_ctx;
	GDVIRTUAL_CALL(_setup, p_ctx);
}

Ref<OSAppContext> OSApp::get_context() const {
	return ctx;
}

void OSApp::_bind_methods() {
	ClassDB::bind_method(D_METHOD("setup", "context"), &OSApp::setup);
	ClassDB::bind_method(D_METHOD("get_context"), &OSApp::get_context);
	GDVIRTUAL_BIND(_setup, "context");
}

// --- os.home ---------------------------------------------------------------

Control *OSHomeApp::create() {
	return memnew(OSHomeApp);
}

OSHomeApp::OSHomeApp() {
	VBoxContainer *vb = memnew(VBoxContainer);
	vb->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	add_child(vb, false, INTERNAL_MODE_FRONT);

	Label *title = memnew(Label);
	title->set_text("OpenDust OS");
	title->add_theme_font_size_override(SNAME("font_size"), 28);
	vb->add_child(title);

	grid = memnew(GridContainer);
	grid->set_columns(3);
	grid->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	vb->add_child(grid);
}

void OSHomeApp::setup(const Ref<OSAppContext> &p_ctx) {
	OSApp::setup(p_ctx);
	_rebuild();
}

void OSHomeApp::_rebuild() {
	while (grid->get_child_count() > 0) {
		Node *c = grid->get_child(0);
		grid->remove_child(c);
		memdelete(c);
	}
	OSKernel *kernel = OSKernel::get_singleton();
	if (!kernel) {
		return;
	}
	Array apps = kernel->list_apps();
	for (int i = 0; i < apps.size(); i++) {
		Dictionary a = apps[i];
		String id = a["id"];
		if (id == "os.home") {
			continue;
		}
		Button *b = memnew(Button);
		b->set_text(String(a["title"]));
		b->set_custom_minimum_size(Size2(160, 90));
		b->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		b->connect(SNAME("pressed"), callable_mp(this, &OSHomeApp::_launch).bind(id));
		grid->add_child(b);
	}
}

void OSHomeApp::_launch(const String &p_app_id) {
	if (ctx.is_valid()) {
		ctx->open_app(p_app_id);
	}
}

// --- os.files --------------------------------------------------------------

Control *OSFilesApp::create() {
	return memnew(OSFilesApp);
}

OSFilesApp::OSFilesApp() {
	VBoxContainer *vb = memnew(VBoxContainer);
	vb->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	add_child(vb, false, INTERNAL_MODE_FRONT);

	HBoxContainer *hb = memnew(HBoxContainer);
	vb->add_child(hb);

	root_label = memnew(Label);
	root_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	hb->add_child(root_label);

	Button *refresh = memnew(Button);
	refresh->set_text("Refresh");
	refresh->connect(SNAME("pressed"), callable_mp(this, &OSFilesApp::_refresh));
	hb->add_child(refresh);

	Button *new_note = memnew(Button);
	new_note->set_text("New note");
	new_note->connect(SNAME("pressed"), callable_mp(this, &OSFilesApp::_on_new_note));
	hb->add_child(new_note);

	tree = memnew(Tree);
	tree->set_hide_root(true);
	tree->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	tree->connect(SNAME("item_activated"), callable_mp(this, &OSFilesApp::_on_activated));
	vb->add_child(tree);
}

void OSFilesApp::setup(const Ref<OSAppContext> &p_ctx) {
	OSApp::setup(p_ctx);
	OSKernel *kernel = OSKernel::get_singleton();
	if (kernel) {
		root_label->set_text("drive://" + kernel->get_world_id() + "/");
		if (!kernel->is_connected(SNAME("drive_changed"), callable_mp(this, &OSFilesApp::_on_drive_changed))) {
			kernel->connect(SNAME("drive_changed"), callable_mp(this, &OSFilesApp::_on_drive_changed));
		}
	}
	_refresh();
}

void OSFilesApp::_notification(int p_what) {
	if (p_what == NOTIFICATION_EXIT_TREE) {
		OSKernel *kernel = OSKernel::get_singleton();
		if (kernel && kernel->is_connected(SNAME("drive_changed"), callable_mp(this, &OSFilesApp::_on_drive_changed))) {
			kernel->disconnect(SNAME("drive_changed"), callable_mp(this, &OSFilesApp::_on_drive_changed));
		}
	}
}

void OSFilesApp::_populate(TreeItem *p_parent, const String &p_dir, int p_depth) {
	if (ctx.is_null() || ctx->get_drive().is_null() || p_depth > 6) {
		return;
	}
	Array entries = ctx->get_drive()->list(p_dir);
	for (int i = 0; i < entries.size(); i++) {
		Dictionary e = entries[i];
		TreeItem *it = tree->create_item(p_parent);
		bool is_dir = e["is_dir"];
		it->set_text(0, String(e["name"]) + (is_dir ? "/" : ""));
		it->set_metadata(0, e);
		if (is_dir) {
			_populate(it, String(e["path"]), p_depth + 1);
		}
	}
}

void OSFilesApp::_refresh() {
	tree->clear();
	TreeItem *root = tree->create_item();
	_populate(root, "", 0);
}

void OSFilesApp::_on_drive_changed(const String &p_path) {
	_refresh();
}

void OSFilesApp::_on_activated() {
	TreeItem *sel = tree->get_selected();
	if (!sel || ctx.is_null()) {
		return;
	}
	Dictionary e = sel->get_metadata(0);
	if (bool(e.get("is_dir", false))) {
		return;
	}
	ctx->open(String(e["path"]));
}

void OSFilesApp::_on_new_note() {
	if (ctx.is_null()) {
		return;
	}
	String stamp = Time::get_singleton()->get_datetime_string_from_system().replace(":", "-");
	Dictionary a;
	a["path"] = "notes/" + stamp + ".md";
	ctx->open_app("os.notes", a);
}

// --- os.notes --------------------------------------------------------------

Control *OSNotesApp::create() {
	return memnew(OSNotesApp);
}

OSNotesApp::OSNotesApp() {
	VBoxContainer *vb = memnew(VBoxContainer);
	vb->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	add_child(vb, false, INTERNAL_MODE_FRONT);

	path_label = memnew(Label);
	vb->add_child(path_label);

	editor = memnew(TextEdit);
	editor->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	editor->set_placeholder("Write something. It saves itself.");
	editor->connect(SNAME("text_changed"), callable_mp(this, &OSNotesApp::_on_text_changed));
	vb->add_child(editor);

	debounce = memnew(Timer);
	debounce->set_wait_time(0.8);
	debounce->set_one_shot(true);
	debounce->connect(SNAME("timeout"), callable_mp(this, &OSNotesApp::_save));
	add_child(debounce, false, INTERNAL_MODE_FRONT);
}

void OSNotesApp::setup(const Ref<OSAppContext> &p_ctx) {
	OSApp::setup(p_ctx);
	String p = String(p_ctx->get_args().get("path", "notes/untitled.md"));
	open_path(p);
}

void OSNotesApp::open_path(const String &p_path) {
	path = p_path;
	path_label->set_text(path);
	loading = true;
	if (ctx.is_valid() && ctx->get_drive().is_valid() && ctx->get_drive()->exists(path)) {
		editor->set_text(ctx->get_drive()->read_text(path));
	} else {
		editor->set_text(String());
	}
	loading = false;
}

String OSNotesApp::get_path() const {
	return path;
}

void OSNotesApp::_on_text_changed() {
	if (loading || !is_inside_tree()) {
		return;
	}
	debounce->start();
}

void OSNotesApp::_save() {
	if (ctx.is_null() || ctx->get_drive().is_null() || path.is_empty()) {
		return;
	}
	Error err = ctx->get_drive()->write_text(path, editor->get_text());
	if (err != OK) {
		ctx->notify("Could not save " + path);
	}
}

// --- os.messages -----------------------------------------------------------

Control *OSMessagesApp::create() {
	return memnew(OSMessagesApp);
}

OSMessagesApp::OSMessagesApp() {
	VBoxContainer *vb = memnew(VBoxContainer);
	vb->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	add_child(vb, false, INTERNAL_MODE_FRONT);

	log = memnew(RichTextLabel);
	log->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	log->set_scroll_follow(true);
	log->set_selection_enabled(true);
	vb->add_child(log);

	HBoxContainer *hb = memnew(HBoxContainer);
	vb->add_child(hb);

	to_edit = memnew(LineEdit);
	to_edit->set_placeholder("to (name or id)");
	to_edit->set_custom_minimum_size(Size2(180, 0));
	hb->add_child(to_edit);

	body_edit = memnew(LineEdit);
	body_edit->set_placeholder("message");
	body_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	body_edit->connect(SNAME("text_submitted"), callable_mp(this, &OSMessagesApp::_on_submitted));
	hb->add_child(body_edit);

	send_button = memnew(Button);
	send_button->set_text("Send");
	send_button->connect(SNAME("pressed"), callable_mp(this, &OSMessagesApp::_on_send));
	hb->add_child(send_button);
}

void OSMessagesApp::setup(const Ref<OSAppContext> &p_ctx) {
	OSApp::setup(p_ctx);
	log->clear();
	OSKernel *kernel = OSKernel::get_singleton();
	if (!kernel) {
		return;
	}
	Array hist = kernel->get_history(100);
	for (int i = 0; i < hist.size(); i++) {
		_append(hist[i]);
	}
	if (!kernel->is_connected(SNAME("message_received"), callable_mp(this, &OSMessagesApp::_on_message))) {
		kernel->connect(SNAME("message_received"), callable_mp(this, &OSMessagesApp::_on_message));
	}
}

void OSMessagesApp::_notification(int p_what) {
	if (p_what == NOTIFICATION_EXIT_TREE) {
		OSKernel *kernel = OSKernel::get_singleton();
		if (kernel && kernel->is_connected(SNAME("message_received"), callable_mp(this, &OSMessagesApp::_on_message))) {
			kernel->disconnect(SNAME("message_received"), callable_mp(this, &OSMessagesApp::_on_message));
		}
	}
}

void OSMessagesApp::_append(const Dictionary &p_msg) {
	String from = p_msg.get("from", "?");
	String to = p_msg.get("to", "?");
	String body = p_msg.get("body", "");
	bool delivered = p_msg.get("delivered", false);
	log->add_text(from + " -> " + to + (delivered ? "" : " (queued)") + ": " + body + "\n");
}

void OSMessagesApp::_on_message(const Dictionary &p_msg) {
	_append(p_msg);
}

void OSMessagesApp::_on_submitted(const String &p_text) {
	_on_send();
}

void OSMessagesApp::_on_send() {
	OSKernel *kernel = OSKernel::get_singleton();
	if (!kernel || ctx.is_null()) {
		return;
	}
	String to = to_edit->get_text().strip_edges();
	String body = body_edit->get_text().strip_edges();
	if (to.is_empty() || body.is_empty()) {
		ctx->notify("Need a recipient and a message.");
		return;
	}
	Dictionary msg;
	msg["from"] = String(ctx->get_session().get("display_name", "local"));
	msg["to"] = to;
	msg["body"] = body;
	Dictionary r = kernel->send(msg);
	if (!bool(r.get("delivered", false))) {
		_append(msg); // Show our own queued message; delivered ones echo via the bus.
	}
	body_edit->clear();
}
