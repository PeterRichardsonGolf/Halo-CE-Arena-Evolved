/*
AE_FRIEND_CODE.H

A player's friend code: twelve characters, written XXXX-XXXX-XXXX, made from the install's player key (player.key, Ed25519
public half). The code is the first 60 bits of SHA-256(public key) in Crockford base32 (0-9 and A-Z without I, L, O, U). Typed
codes are read forgivingly: any case, dashes and spaces ignored, I and L read as 1, O as 0. Pure (no engine includes;
port/linux/tests/ae_friend_code_test.c). The party service computes the same code (tools/ae_party_service).
*/

#ifndef __AE_FRIEND_CODE_H
#define __AE_FRIEND_CODE_H

enum { AE_FRIEND_CODE_DIGITS = 12, AE_FRIEND_CODE_SIZE = 15 };

/* the code of a 32-byte public key into out (14 characters and the end): 1 */
int ae_friend_code_from_key(const unsigned char pub[32], char out[AE_FRIEND_CODE_SIZE]);

/* a typed code read into its written form (out as above): 1 when text holds exactly 12 code characters (others besides
dashes and spaces refuse it), else 0 and out is left empty */
int ae_friend_code_parse(const char *text, char out[AE_FRIEND_CODE_SIZE]);

/* how many code characters text holds so far (for the field's "n / 12"); characters that cannot be in a code are not counted */
int ae_friend_code_count(const char *text);

#endif
