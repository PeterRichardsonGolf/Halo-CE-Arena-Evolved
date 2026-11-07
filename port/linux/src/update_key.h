/*
UPDATE_KEY.H

The public keys the self-updater (updater.c, update_signature.c) accepts a
release's signature from: Ed25519, 32 bytes each. The release workflow signs
every update zip with the matching private key (tools/update_sign.py), which
lives only in the release workflow's secrets, never in this repository.

A build whose keys are all zero has no key: it installs updates unsigned, as
builds did before signatures (and says so in its log). Once a key is set
here, the builds from then on install only signed updates, so the releases
from then on must be signed with it.

To rotate: add the new key beside the old one, release, sign with the new key
from the next release on, and drop the old key a few releases later.

The key below is Arena Evolved's release key's public half (made
2026-10-10; tools/update_sign.py public <key.pem> prints it). Its private
half stays with the project owner, outside every repository and CI.
*/

#ifndef UPDATE_KEY_H
#define UPDATE_KEY_H

#ifndef HALO_UPDATE_TEST_KEY
static const unsigned char update_public_keys[][32] =
{
	/* Arena Evolved's release key */
	{
		0xce, 0x75, 0xc7, 0x07, 0x3e, 0x39, 0xc5, 0xb1, 0x03, 0x9f, 0x01, 0x7b, 0x67, 0x60, 0xf5, 0x71,
		0x1c, 0x7e, 0x9b, 0xd0, 0xe7, 0x74, 0x48, 0xc1, 0x3a, 0x1f, 0x63, 0x32, 0xd2, 0x9d, 0xbc, 0xf6,
	},
};
#else
/* (a test's key, given by the test's build: HALO_UPDATE_TEST_KEY is its bytes) */
static const unsigned char update_public_keys[][32] = { { HALO_UPDATE_TEST_KEY } };
#endif

#endif
