/*
AE_MASTER_KEY.H

The AE master server this build asks and publishes to, and its pinned list-signing key. THE ONE PLACE the real key
goes (the master's public list key, key id 78ddec6e9a22665b; the private half stays offline). A placeholder key
(AE_MASTER_LIST_KEY_IS_PLACEHOLDER set to 1, or any key of one repeated digit) is NOT asked and NOT published to (one log
line says so): no placeholder key is ever accepted. Rules: ae_server_listing_rules.c (ae_listing_key_usable).
*/

#ifndef __AE_MASTER_KEY_H
#define __AE_MASTER_KEY_H

#define AE_MASTER_LIST_URL "https://master.arenaevolved.com"
/* the real key, made offline by the owner (public half) */
#define AE_MASTER_LIST_KEY "ed25519:7dd4055858d8e37c60cd3f3c4a944deb6f515d063727e93087fa3ef37b316993"
#define AE_MASTER_LIST_KEY_IS_PLACEHOLDER 0

#endif
