/*
AE_HOST_IDENTITY.C

The Android host's own build_identity_user_agent: posix_update.c, built into the host (libmain.so: its HTTPS for the
updater and the master servers), names the game in its requests' User-Agent with the game's build_identity.c, which is
a unit of the game library, not of the host. Here the host's, the same name without the build's version.
*/

const char *build_identity_user_agent(void)
{
	return "ArenaEvolved (Android)";
}
