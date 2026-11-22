/*
AE_SERVER_GAMETYPES_JSON.C

Arena Evolved's built-in gametypes (arena_gametypes.c: AE TEAM SLY, AE CTF...) for the dedicated server's GET
/api/gametypes (server_http.c): an "ae_builtin" array beside the stock "builtin" names and the saved files. Each entry
holds the word a rotation names it by ("ae_team_sly": the stored name in lower case, a '_' for a space), the name the
menus show, its style and group (as CHANGE GAMETYPE sorts it), and "source": "ae". A page that shows the stock list
as it is can show these the same way.
*/

#include "cseries.h"
#include "saved games/arena_gametypes.h"

#include "ae_rotation_shuffle.h"
#include "ae_h2_presets_rules.h"
#include "../src/server_json.h"
#include "halo_map_packs.h"

#include <string.h>

void ae_server_gametypes_json(
	struct server_json *json)
{
	short count = arena_gametype_catalog_count();
	short index;

	server_json_array_begin(json, "ae_builtin");
	for (index = 0; index < count; index++)
	{
		char const *stored = arena_gametype_catalog_stored_name(index);
		wchar_t wide[16];
		struct arena_gametype_info info;
		char word[16];
		short at;

		if (!stored || !ae_rotation_word_of(stored, word, sizeof(word)))
			continue;
		for (at = 0; stored[at] && at < (short)NUMBEROF(wide) - 1; at++)
			wide[at] = (wchar_t)(unsigned char)stored[at];
		wide[at] = 0;
		if (!arena_gametype_info(wide, &info) || ae_h2_preset_hidden_wide(wide))
			continue;
		server_json_object_begin(json, NULL);
		server_json_string(json, "name", word);
		server_json_string(json, "display", info.display_name);
		server_json_string(json, "style", info.style ? info.style : "");
		server_json_string(json, "group", info.group ? info.group : "");
		server_json_boolean(json, "team", info.team != 0);
		server_json_string(json, "source", "ae");
		server_json_object_end(json);
	}
	server_json_array_end(json);
}

/* (0.7.7) a rotation entry's map from a map pack (<file>@<pack>) for the web page: "map_title" (the pack's name for it,
"Gothra") and "pack" (the pack's, "Halo CE+"); nothing for another family's map. Additive: a reader that does not know
them reads the entry as before */
void ae_server_entry_titles(
	struct server_json *json,
	char const *map)
{
	char title[64], pack[64], tag[16];

	if (!map || !strchr(map, '@') || !map_pack_listing(map, title, (int)sizeof(title), pack, (int)sizeof(pack), tag,
		(int)sizeof(tag)) || !title[0])
	{
		return;
	}
	server_json_string(json, "map_title", title);
	server_json_string(json, "pack", pack);
}
