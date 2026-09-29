/**************************************************************************/
/*  opendust_identity.h                                                   */
/**************************************************************************/
/*                         This file is part of:                          */
/*                              OPENDUST                                  */
/*                         https://opendust.io                            */
/**************************************************************************/
/* OpenDust is a fork of Godot Engine. MIT licensed; see LICENSE.txt.     */
/**************************************************************************/

#pragma once

#include "core/object/ref_counted.h"

// pods.global SSO consumer. STUB for phase 0: only the local (offline)
// session exists. The OIDC/PKCE loopback flow described in
// docs/opendust/05-identity.md is TODO and must not be hand-rolled here
// without the platform's IdP decision (Zitadel pending ratification).
class OpenDustIdentity : public RefCounted {
	GDCLASS(OpenDustIdentity, RefCounted);

	Dictionary local_session;

protected:
	static void _bind_methods();

public:
	// Returns ERR_UNAVAILABLE until the pods.global flow lands.
	Error sign_in();
	void sign_out();
	bool is_signed_in() const;

	// The session every device gets while offline / unconfigured:
	// {family_id:"local", user_id:"local", display_name:<os user>, scopes:["local"], source:"local"}
	Dictionary get_local_session() const;
	Dictionary get_session() const;

	OpenDustIdentity();
};
