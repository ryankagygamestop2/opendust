/**************************************************************************/
/*  soul_loader.cpp                                                       */
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

#include "soul_loader.h"

#include "soul.h"
#include "soul_parser.h"

bool ResourceFormatLoaderSoul::is_soul_path(const String &p_path) {
	String file = p_path.get_file().to_lower();
	return file == "soul.md" || file.ends_with(".soul.md");
}

Ref<Resource> ResourceFormatLoaderSoul::load(const String &p_path, const String &p_original_path, Error *r_error, bool p_use_sub_threads, float *r_progress, CacheMode p_cache_mode) {
	if (r_error) {
		*r_error = ERR_FILE_CANT_OPEN;
	}
	Error err = OK;
	Ref<Soul> soul = SoulParser::parse_file(p_path, &err);
	if (soul.is_null()) {
		if (r_error) {
			*r_error = err == OK ? ERR_FILE_CORRUPT : err;
		}
		return Ref<Resource>();
	}
	// Keep resource identity tied to the original (res://) path when loaded through a remap.
	soul->set_path(p_original_path.is_empty() ? p_path : p_original_path, p_cache_mode == CACHE_MODE_REPLACE);
	if (r_error) {
		*r_error = OK;
	}
	if (r_progress) {
		*r_progress = 1.0f;
	}
	return soul;
}

void ResourceFormatLoaderSoul::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("md");
}

bool ResourceFormatLoaderSoul::recognize_path(const String &p_path, const String &p_for_type) const {
	if (!p_for_type.is_empty() && p_for_type != "Soul" && p_for_type != "Resource") {
		return false;
	}
	return is_soul_path(p_path);
}

bool ResourceFormatLoaderSoul::handles_type(const String &p_type) const {
	return p_type == "Soul";
}

String ResourceFormatLoaderSoul::get_resource_type(const String &p_path) const {
	return is_soul_path(p_path) ? "Soul" : String();
}
