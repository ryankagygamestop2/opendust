/**************************************************************************/
/*  register_types.cpp                                                    */
/**************************************************************************/
/*                         This file is part of:                          */
/*                              OPENDUST                                  */
/*                         https://opendust.io                            */
/**************************************************************************/
/* OpenDust is a fork of Godot Engine. MIT licensed; see LICENSE.txt.     */
/**************************************************************************/

#include "register_types.h"

#include "device.h"
#include "opendust_identity.h"
#include "os_app_context.h"
#include "os_apps.h"
#include "os_kernel.h"
#include "os_shell.h"
#include "os_tools.h"
#include "world_drive.h"

#include "core/config/project_settings.h"
#include "core/object/class_db.h"

void initialize_opendust_os_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	// Project settings owned by this module. See docs/opendust/03-os-layer.md.
	GLOBAL_DEF("opendust/world_id", "default");
	GLOBAL_DEF("opendust/os/drive_root", "");

	GDREGISTER_CLASS(WorldDrive);
	GDREGISTER_CLASS(OpenDustIdentity);
	GDREGISTER_CLASS(OSAppContext);
	GDREGISTER_CLASS(OSKernel);
	GDREGISTER_CLASS(OSShell);
	GDREGISTER_CLASS(OSApp);
	GDREGISTER_CLASS(OSHomeApp);
	GDREGISTER_CLASS(OSFilesApp);
	GDREGISTER_CLASS(OSNotesApp);
	GDREGISTER_CLASS(OSMessagesApp);
	GDREGISTER_CLASS(Device);
	GDREGISTER_CLASS(Device2D);

	OSKernel::register_builtin_apps();
	OSTools::initialize();
}

void uninitialize_opendust_os_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	OSTools::finalize();
}
