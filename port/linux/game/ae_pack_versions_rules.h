/*
AE_PACK_VERSIONS_RULES.H

A pack map of another version (rc6): the pure rules of ae_pack_versions.c (unit test:
port/linux/tests/ae_pack_versions_rules_test.c).

A guest that joins a game on a pack's map (<file>@<pack>) whose own maps_<pack>\<file>.map is another version than the
host's (another fingerprint, map_pack_fingerprint) gets the host's copy from map sharing beside its own, in
maps_shared\<pack>\<version>\<file>.map (the version the fingerprint's 8 hex digits), and plays that network game on
it. The player's own file is never touched, and every other load of the map (local play, the menus' lists) is the
player's own. At most AE_PACK_VERSIONS_KEPT copies of one map are kept: the one used longest ago goes when another
arrives, never the one the game plays.
*/

#ifndef AE_PACK_VERSIONS_RULES_H
#define AE_PACK_VERSIONS_RULES_H

enum
{
	/* the host's copies of one pack map kept at most */
	AE_PACK_VERSIONS_KEPT = 3,
	/* the copies of one map looked at at most */
	AE_PACK_VERSIONS_LISTED = 32
};

/* the folder of the host's copies (maps_shared under the data root, "d:\\") */
#define AE_PACK_VERSIONS_ROOT "d:\\maps_shared\\"

/* the folder of one version's copy, "d:\\maps_shared\\<pack>\\<version>\\": 0 when a name is not a plain one (empty, a
path's characters or a dot first) or the text does not fit */
int ae_pack_versions_folder(char *out, int size, char const *pack, unsigned long version);

/* the copy's path, the folder's then "<file>.map": 0 as for the folder, or for a file name that is not plain */
int ae_pack_versions_path(char *out, int size, char const *pack, char const *file, unsigned long version);

/* a version folder's name: exactly 8 hex digits (either case), not 0: 1 and its version, else 0 */
int ae_pack_versions_parse_folder(char const *name, unsigned long *version);

/* which copies of one map go so that `keep` stay: the ones used longest ago (last_used, larger is later), never the
one in_use (its version; 0 for none). remove gets their indexes, oldest first; the count is returned */
int ae_pack_versions_evict(int count, unsigned long const *versions, unsigned long long const *last_used,
	unsigned long in_use, int keep, int *remove);

#endif
