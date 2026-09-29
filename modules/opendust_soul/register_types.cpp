/**************************************************************************/
/*  register_types.cpp                                                    */
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

#include "register_types.h"

#include "agent_body.h"
#include "agent_tools.h"
#include "soul.h"
#include "soul_loader.h"
#include "soul_session_prompt.h"

#include "core/config/project_settings.h"
#include "core/io/resource_loader.h"
#include "core/object/class_db.h"

static Ref<ResourceFormatLoaderSoul> soul_loader;

void initialize_opendust_soul_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	GDREGISTER_CLASS(Soul);
	GDREGISTER_CLASS(AgentBody);
	GDREGISTER_CLASS(SoulSessionPrompt);

	soul_loader.instantiate();
	ResourceLoader::add_resource_format_loader(soul_loader);

	// Project settings this module reads. Defaults are safe: no spawning over the bridge
	// unless a project opts in.
	GLOBAL_DEF("opendust/world_id", "default");
	GLOBAL_DEF("opendust/agent/allow_spawn", false);
	GLOBAL_DEF("opendust/agent/allow_attach_any_body", false);

	// Bridge tools (agent.*). No-op when opendust_agent is not compiled in or its registry
	// singleton hasn't been created yet; AgentBody retries on enter-tree.
	AgentTools::ensure_registered();
}

void uninitialize_opendust_soul_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	AgentTools::unregister_all();

	ResourceLoader::remove_resource_format_loader(soul_loader);
	soul_loader.unref();
}
