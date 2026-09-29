/**************************************************************************/
/*  opendust_identity.cpp                                                 */
/**************************************************************************/
/*                         This file is part of:                          */
/*                              OPENDUST                                  */
/*                         https://opendust.io                            */
/**************************************************************************/
/* OpenDust is a fork of Godot Engine. MIT licensed; see LICENSE.txt.     */
/**************************************************************************/

#include "opendust_identity.h"

#include "core/os/os.h"

OpenDustIdentity::OpenDustIdentity() {
	String user;
	if (OS::get_singleton()->has_environment("USERNAME")) {
		user = OS::get_singleton()->get_environment("USERNAME");
	} else if (OS::get_singleton()->has_environment("USER")) {
		user = OS::get_singleton()->get_environment("USER");
	}
	if (user.is_empty()) {
		user = "local";
	}
	PackedStringArray scopes;
	scopes.push_back("local");
	local_session["family_id"] = "local";
	local_session["user_id"] = "local";
	local_session["display_name"] = user;
	local_session["scopes"] = scopes;
	local_session["source"] = "local";
}

Error OpenDustIdentity::sign_in() {
	// TODO(05-identity.md): OIDC authorization-code + PKCE against pods.global,
	// loopback redirect, tokens in memory, refresh token via OS keychain.
	WARN_PRINT_ONCE("OpenDustIdentity::sign_in(): pods.global SSO is not implemented yet (docs/opendust/05-identity.md). Using the local session.");
	return ERR_UNAVAILABLE;
}

void OpenDustIdentity::sign_out() {
	// Nothing to clear while only the local session exists.
}

bool OpenDustIdentity::is_signed_in() const {
	return false;
}

Dictionary OpenDustIdentity::get_local_session() const {
	return local_session.duplicate();
}

Dictionary OpenDustIdentity::get_session() const {
	return get_local_session();
}

void OpenDustIdentity::_bind_methods() {
	ClassDB::bind_method(D_METHOD("sign_in"), &OpenDustIdentity::sign_in);
	ClassDB::bind_method(D_METHOD("sign_out"), &OpenDustIdentity::sign_out);
	ClassDB::bind_method(D_METHOD("is_signed_in"), &OpenDustIdentity::is_signed_in);
	ClassDB::bind_method(D_METHOD("get_local_session"), &OpenDustIdentity::get_local_session);
	ClassDB::bind_method(D_METHOD("get_session"), &OpenDustIdentity::get_session);
}
