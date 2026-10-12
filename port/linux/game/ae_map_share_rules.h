/*
AE_MAP_SHARE_RULES.H

Arena Evolved: map sharing's safety rules (from DamnationCE v0.3.41, CC0), as
pure functions with no engine calls (port/linux/tests/ae_map_share_rules_test.c
runs them):

- A source the host names as a LAN player is reached at its address only when
  that address is a local network's (private 10/8, 172.16/12, 192.168/16,
  link-local 169.254/16, loopback 127/8). A host can so never have its players
  open connections to anywhere on the internet. Internet play's sources are
  reached by their keys, never at an address the host gave, and are not
  affected.
- A download that finished but failed its check (the host's version, a broken
  map, a resource map it needs) is removed from the downloads' folder, never
  left there under the map's name, where it would stand in for the real one.
  Only a file directly in that folder, named <something>.map, is removed: a
  path with another folder in it (a separator, "..") is refused.
*/

#ifndef __AE_MAP_SHARE_RULES_H
#define __AE_MAP_SHARE_RULES_H

/* whether an IPv4 address (host byte order) is a local network's: private,
link-local or loopback. 0.0.0.0, broadcast and anything above 32 bits are not */
int ae_map_share_address_local(
	unsigned long address);

/* whether path is a file directly in folder (folder ends in its separator, as
"d:\\maps_ce\\"), named <name>.map: nothing after the folder but a plain name */
int ae_map_share_path_in_folder(
	char const *path,
	char const *folder);

/* the failed download's files removed: each of the count paths that is in
folder (ae_map_share_path_in_folder) handed to delete_file; the others left
alone. Returns how many delete_file removed (it returns nonzero for one it did) */
int ae_map_share_discard(
	char const *const *paths,
	int count,
	char const *folder,
	int (*delete_file)(char const *path, void *context),
	void *context);

#endif
