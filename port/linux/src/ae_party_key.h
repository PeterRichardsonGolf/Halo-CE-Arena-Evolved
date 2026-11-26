/*
AE_PARTY_KEY.H

This machine's key for the online party (network.party_service): an Ed25519 seed in party.key beside config.toml. The friend
code comes from its public half. It is not the player key the OpenCE masters link to a Discord account, and it is never sent
to a master server. Made on first use. Thread safe.
*/

#ifndef __AE_PARTY_KEY_H
#define __AE_PARTY_KEY_H

/* the public half (32 bytes); the key is made when there is none: 1, 0 when it cannot be read or written */
int ae_party_key_public(unsigned char *public_key);
/* the text signed with the key (64 bytes): 1; 0 for any text that does not start with "ae-party-" (the key signs nothing else) */
int ae_party_key_sign(const char *text, unsigned char *signature);

#endif
