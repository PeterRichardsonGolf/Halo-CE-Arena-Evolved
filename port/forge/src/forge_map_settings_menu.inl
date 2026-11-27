/*
FORGE_MAP_SETTINGS_MENU.INL

The MAP SETTINGS menu (forge_map_settings.c holds what it sets), included
into forge_3d_mode.c for its menus' drawing and input: the palette's button
held opens it (forge_3d_update). Its pages:

- MAP SETTINGS: DISPLAY NAME, SKYBOX, FOG SETTINGS, PLAYER BIPEDS, RENDER
  DISTANCE (how far a big map is drawn), MONITOR SPEED (how fast Forge's
  Monitor flies over it), and UNDO and REDO (the undo history's, any player's:
  forge_3d_history_step);
- FOG SETTINGS: the atmosphere and its fog's values;
- PLAYER BIPEDS: RED TEAM, BLUE TEAM, FREE FOR ALL, each a page of its own
  (its CONFIG: CUSTOM, or another team's settings played; with CUSTOM its
  BIPED, its HANDS, which follow the biped chosen, its MOVEMENT, a Spartan's
  or the biped's own; and TEST SPAWN);
- a list of the tags a setting can be (a sky, a biped, hands);
- the display name, typed on an on-screen keyboard (or the keyboard's own
  keys).

B goes back a page, and from the first closes it.
*/

/* ---------- constants */

#define FORGE_MAP_SETTINGS_HOLD_MS 450u
#define FORGE_MAP_SETTINGS_VISIBLE_ROWS 10
#define FORGE_MAP_SETTINGS_PICK_ROWS 10
#define FORGE_MAP_SETTINGS_MAX_CANDIDATES 1024
#define FORGE_MAP_SETTINGS_MAX_PAGE_ROWS 16

enum
{
	_map_settings_page_main = 0,
	_map_settings_page_fog,
	_map_settings_page_bipeds,
	_map_settings_page_team,
	_map_settings_page_pick,
	_map_settings_page_name,
	NUMBER_OF_MAP_SETTINGS_PAGES
};

/* the rows the pages of rows have */
enum
{
	_map_settings_row_name = 0,
	_map_settings_row_sky,
	_map_settings_row_fog_menu,
	_map_settings_row_bipeds_menu,
	_map_settings_row_render_distance,
	_map_settings_row_monitor_speed,
	/* (the undo history's, forge_3d_history_step: anyone's) */
	_map_settings_row_undo,
	_map_settings_row_redo,

	_map_settings_row_atmosphere,
	_map_settings_row_fog_red,
	_map_settings_row_fog_green,
	_map_settings_row_fog_blue,
	_map_settings_row_fog_density,
	_map_settings_row_fog_start,
	_map_settings_row_fog_end,
	_map_settings_row_fog_indoors,
	_map_settings_row_fog_sky,

	_map_settings_row_team_red,
	_map_settings_row_team_blue,
	_map_settings_row_team_ffa,

	_map_settings_row_team_config,
	_map_settings_row_team_biped,
	_map_settings_row_team_hands,
	_map_settings_row_team_movement,
	_map_settings_row_test_spawn,
	NUMBER_OF_MAP_SETTINGS_ROWS
};

/* the on-screen keyboard's keys: four rows of characters, then CAPS,
SPACE, DEL and DONE */
static char const *const s_map_name_keys[4] = { "1234567890", "QWERTYUIOP", "ASDFGHJKL-", "ZXCVBNM_'." };
#define FORGE_MAP_NAME_KEY_COLUMNS 10
#define FORGE_MAP_NAME_KEY_ROWS 5

/* ---------- globals */

static struct
{
	int page;
	/* each page of rows' row picked, and its first shown */
	int cursors[NUMBER_OF_MAP_SETTINGS_PAGES];
	int scrolls[NUMBER_OF_MAP_SETTINGS_PAGES];
	/* the team page's slot */
	int team_slot;
	/* the list (pick page): of what, the page it goes back to, its tags
	(NONE first: the map's own) */
	int pick_kind;
	int pick_slot;
	int pick_return;
	long pick_tags[FORGE_MAP_SETTINGS_MAX_CANDIDATES + 1];
	int pick_count;
	int pick_cursor;
	int pick_scroll;
	/* the name typed (name page), the key picked, small letters */
	char name[FORGE_MAP_DISPLAY_NAME_MAX + 1];
	int key_row;
	int key_column;
	int lower;
	int keys_down[256];
	int keys_read;
	/* a TEST SPAWN wanted: forge_3d_update leaves Forge mode for it */
	int test_requested;
} s_map_settings_menu;

/* ---------- private code */

/* a page of rows' rows: how many */
static int map_settings_page_rows(int page, int *rows)
{
	int count = 0;
	int row;

	switch (page)
	{
	case _map_settings_page_main:
		for (row = _map_settings_row_name; row <= _map_settings_row_redo; row++)
			rows[count++] = row;
		break;
	case _map_settings_page_fog:
		for (row = _map_settings_row_atmosphere; row <= _map_settings_row_fog_sky; row++)
			rows[count++] = row;
		break;
	case _map_settings_page_bipeds:
		for (row = _map_settings_row_team_red; row <= _map_settings_row_team_ffa; row++)
			rows[count++] = row;
		break;
	case _map_settings_page_team:
		/* (a team playing another's: its CONFIG alone, and the test) */
		rows[count++] = _map_settings_row_team_config;
		if (!forge_map_settings_get()->link[s_map_settings_menu.team_slot])
		{
			for (row = _map_settings_row_team_biped; row <= _map_settings_row_team_movement; row++)
				rows[count++] = row;
		}
		rows[count++] = _map_settings_row_test_spawn;
		break;
	}
	return count;
}

static int map_settings_row_at(int page, int place)
{
	int rows[FORGE_MAP_SETTINGS_MAX_PAGE_ROWS];
	int count = map_settings_page_rows(page, rows);

	return place >= 0 && place < count ? rows[place] : NONE;
}

static void map_settings_page_open(int page)
{
	s_map_settings_menu.page = page;
	s_map_settings_menu.cursors[page] = 0;
	s_map_settings_menu.scrolls[page] = 0;
}

/* back a page: a team's to the teams', the fog's and the teams' to the
first, the first's closes the menu */
static void map_settings_page_back(void)
{
	switch (s_map_settings_menu.page)
	{
	case _map_settings_page_team:
		s_map_settings_menu.page = _map_settings_page_bipeds;
		break;
	case _map_settings_page_fog:
	case _map_settings_page_bipeds:
		s_map_settings_menu.page = _map_settings_page_main;
		break;
	case _map_settings_page_pick:
		s_map_settings_menu.page = s_map_settings_menu.pick_return;
		break;
	case _map_settings_page_name:
		s_map_settings_menu.page = _map_settings_page_main;
		break;
	default:
		g_forge.active_modal = _forge_modal_none;
		break;
	}
}

/* a row's kind of tag and slot (FALSE for a row of no tag) */
static int map_settings_row_tag(int row, int *kind, int *slot)
{
	if (row == _map_settings_row_sky)
	{
		*kind = _forge_map_kind_sky;
		*slot = 0;
		return 1;
	}
	/* (a team playing another's has no biped of its own to step) */
	if (row >= _map_settings_row_team_red && row <= _map_settings_row_team_ffa &&
		!forge_map_settings_get()->link[row - _map_settings_row_team_red])
	{
		*kind = _forge_map_kind_biped;
		*slot = row - _map_settings_row_team_red;
		return 1;
	}
	if (row == _map_settings_row_team_biped || row == _map_settings_row_team_hands)
	{
		*kind = row == _map_settings_row_team_biped ? _forge_map_kind_biped : _forge_map_kind_hands;
		*slot = s_map_settings_menu.team_slot;
		return 1;
	}
	return 0;
}

static void map_settings_candidates_take(int kind, int slot)
{
	s_map_settings_menu.pick_kind = kind;
	s_map_settings_menu.pick_slot = slot;
	s_map_settings_menu.pick_tags[0] = NONE;
	s_map_settings_menu.pick_count = 1 + forge_map_settings_candidates(kind, s_map_settings_menu.pick_tags + 1,
		FORGE_MAP_SETTINGS_MAX_CANDIDATES);
}

/* the place of the tag set now among the candidates (0: the map's own) */
static int map_settings_candidate_place(void)
{
	long now = forge_map_settings_tag(s_map_settings_menu.pick_kind, s_map_settings_menu.pick_slot);
	int place;

	for (place = 1; now != NONE && place < s_map_settings_menu.pick_count; place++)
	{
		if (s_map_settings_menu.pick_tags[place] == now)
			return place;
	}
	return 0;
}

static int map_settings_refuse(void)
{
	if (forge_map_settings_can_edit())
		return 0;
	forge_set_status("Only the host changes the map settings");
	return 1;
}

static char const *map_settings_slot_name(int slot)
{
	return slot == _forge_map_slot_red ? "RED TEAM" : slot == _forge_map_slot_blue ? "BLUE TEAM" : "FREE FOR ALL";
}

/* a team's CONFIG choices, as the menu steps them: the other two teams',
then CUSTOM (its own) */
static int map_settings_link_step(int slot, int link, int dir)
{
	int choices[NUMBER_OF_FORGE_MAP_SLOTS];
	int count = 0;
	int other;
	int place;

	for (other = 0; other < NUMBER_OF_FORGE_MAP_SLOTS; other++)
	{
		if (other != slot)
			choices[count++] = other + 1;
	}
	choices[count++] = _forge_link_custom;
	for (place = 0; place < count && choices[place] != link; place++)
	{
	}
	if (place == count)
		place = count - 1;
	return choices[(place + count + dir) % count];
}

static char const *map_settings_link_name(int link)
{
	return link > _forge_link_custom && link <= NUMBER_OF_FORGE_MAP_SLOTS ? map_settings_slot_name(link - 1) : "CUSTOM";
}

/* a tag chosen: what the status bar says (a biped's hands chosen with it) */
static void map_settings_tag_set(int kind, int slot, long tag)
{
	char label[64];
	char hands[64];

	forge_map_settings_set_tag(kind, slot, tag);
	forge_map_settings_tag_label(tag, label, sizeof(label));
	if (kind == _forge_map_kind_biped)
	{
		forge_map_settings_tag_label(forge_map_settings_tag(_forge_map_kind_hands, slot), hands, sizeof(hands));
		forge_set_status("%s: %s, hands %s", map_settings_slot_name(slot), label, hands);
	}
	else
	{
		forge_set_status("%s", label);
	}
}

/* a fog row's step: its value moved `dir` steps */
static void map_settings_fog_step(int row, int dir)
{
	struct forge_map_settings const *now = forge_map_settings_get();
	float color[3];
	float density = now->fog_density;
	float start = now->fog_start;
	float opaque = now->fog_opaque;
	int indoors = now->fog_indoors;
	int sky = now->sky_visibility;

	color[0] = now->fog_color[0];
	color[1] = now->fog_color[1];
	color[2] = now->fog_color[2];
	/* (the map's own fog, or none, changed: the player's own) */
	if (now->atmosphere == _forge_atmosphere_map_default || now->atmosphere == _forge_atmosphere_clear)
	{
		forge_map_settings_set_atmosphere(_forge_atmosphere_custom);
		if (now->fog_density <= 0.0f)
			density = 0.5f;
	}
	switch (row)
	{
	case _map_settings_row_fog_red:
	case _map_settings_row_fog_green:
	case _map_settings_row_fog_blue:
	{
		int channel = row - _map_settings_row_fog_red;
		int value = (int)(color[channel] * 255.0f + 0.5f) + dir * 5;

		color[channel] = (float)PIN(value, 0, 255) / 255.0f;
		break;
	}
	case _map_settings_row_fog_density:
		density = (float)PIN((int)(density * 100.0f + 0.5f) + dir * 5, 0, 100) / 100.0f;
		break;
	case _map_settings_row_fog_start:
		start += (float)dir * (start < 10.0f ? 1.0f : start < 100.0f ? 5.0f : 25.0f);
		start = PIN(start, 0.0f, 2000.0f);
		if (opaque <= start + 1.0f)
			opaque = start + 1.0f;
		break;
	case _map_settings_row_fog_end:
		opaque += (float)dir * (opaque < 20.0f ? 2.0f : opaque < 200.0f ? 10.0f : 50.0f);
		opaque = PIN(opaque, start + 1.0f, 4000.0f);
		break;
	case _map_settings_row_fog_indoors:
		indoors = !indoors;
		break;
	case _map_settings_row_fog_sky:
		sky = (sky + NUMBER_OF_FORGE_SKY_VISIBILITIES + dir) % NUMBER_OF_FORGE_SKY_VISIBILITIES;
		break;
	}
	forge_map_settings_set_fog(color, density, start, opaque, indoors, sky);
}

static void map_settings_name_type(char character)
{
	size_t length = strlen(s_map_settings_menu.name);

	if (length >= FORGE_MAP_DISPLAY_NAME_MAX)
		return;
	if (s_map_settings_menu.lower && character >= 'A' && character <= 'Z')
		character = (char)(character - 'A' + 'a');
	s_map_settings_menu.name[length] = character;
	s_map_settings_menu.name[length + 1] = 0;
}

static void map_settings_name_delete(void)
{
	size_t length = strlen(s_map_settings_menu.name);

	if (length)
		s_map_settings_menu.name[length - 1] = 0;
}

static void map_settings_name_done(void)
{
	forge_map_settings_set_display_name(s_map_settings_menu.name);
	forge_set_status(s_map_settings_menu.name[0] ? "Map name: %s" : "Map name: the file's", s_map_settings_menu.name);
	s_map_settings_menu.page = _map_settings_page_main;
}

/* the key picked on the on-screen keyboard pressed */
static void map_settings_name_key_press(void)
{
	int row = s_map_settings_menu.key_row;
	int column = s_map_settings_menu.key_column;

	if (row < 4)
	{
		map_settings_name_type(s_map_name_keys[row][column]);
		return;
	}
	/* (the last row's four wide keys, each over its columns) */
	switch (column * 4 / FORGE_MAP_NAME_KEY_COLUMNS)
	{
	case 0: s_map_settings_menu.lower = !s_map_settings_menu.lower; break;
	case 1: map_settings_name_type(' '); break;
	case 2: map_settings_name_delete(); break;
	default: map_settings_name_done(); break;
	}
}

/* the keyboard's own keys (PC and web), as the save dialog reads them */
static int map_settings_name_keys_typed(void)
{
	int typed = 0;
	int vk;
	int shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;

	for (vk = 0; vk < 256; vk++)
	{
		int down = (GetAsyncKeyState(vk) & 0x8000) != 0;
		int pressed = down && !s_map_settings_menu.keys_down[vk] && s_map_settings_menu.keys_read;

		s_map_settings_menu.keys_down[vk] = down;
		if (!pressed)
			continue;
		if (vk >= 'A' && vk <= 'Z')
		{
			int lower = s_map_settings_menu.lower;

			s_map_settings_menu.lower = !shift;
			map_settings_name_type((char)vk);
			s_map_settings_menu.lower = lower;
			typed = 1;
		}
		else if (vk >= '0' && vk <= '9')
		{
			map_settings_name_type((char)vk);
			typed = 1;
		}
		else if (vk == VK_SPACE)
		{
			map_settings_name_type(' ');
			typed = 1;
		}
		else if (vk == VK_OEM_MINUS)
		{
			map_settings_name_type(shift ? '_' : '-');
			typed = 1;
		}
		else if (vk == VK_BACK)
		{
			map_settings_name_delete();
			typed = 1;
		}
		else if (vk == VK_RETURN)
		{
			map_settings_name_done();
			typed = 1;
		}
	}
	s_map_settings_menu.keys_read = 1;
	return typed;
}

/* a row of a page of rows pressed (A: `enter`) or stepped (`dir`) */
static void map_settings_row_act(int row, int dir, int enter)
{
	int kind, slot;

	/* (the pages it opens, any player's to see) */
	if (row == _map_settings_row_fog_menu || row == _map_settings_row_bipeds_menu)
	{
		if (enter)
			map_settings_page_open(row == _map_settings_row_fog_menu ? _map_settings_page_fog : _map_settings_page_bipeds);
		return;
	}
	if (enter && row >= _map_settings_row_team_red && row <= _map_settings_row_team_ffa)
	{
		s_map_settings_menu.team_slot = row - _map_settings_row_team_red;
		map_settings_page_open(_map_settings_page_team);
		return;
	}
	if (row == _map_settings_row_test_spawn)
	{
		if (!enter)
			return;
		if (!forge_map_settings_can_edit())
		{
			forge_set_status("Only the host can test spawn");
			return;
		}
		if (forge_map_settings_test_start(s_map_settings_menu.team_slot))
		{
			s_map_settings_menu.test_requested = 1;
			g_forge.active_modal = _forge_modal_none;
			forge_set_status("Test spawn: %s. %s FORGE ends it", map_settings_slot_name(s_map_settings_menu.team_slot),
				forge_action_prompt(FORGE_BIND_TOGGLE_MODE));
		}
		return;
	}
	if (row == _map_settings_row_undo || row == _map_settings_row_redo)
	{
		/* (the menu stays open, for another step) */
		if (enter)
			forge_3d_history_step(row == _map_settings_row_redo);
		return;
	}
	if (map_settings_refuse())
		return;
	if (row == _map_settings_row_name)
	{
		if (enter)
		{
			snprintf(s_map_settings_menu.name, sizeof(s_map_settings_menu.name), "%s",
				forge_map_settings_get()->display_name);
			s_map_settings_menu.key_row = 1;
			s_map_settings_menu.key_column = 0;
			s_map_settings_menu.keys_read = 0;
			s_map_settings_menu.page = _map_settings_page_name;
			/* (the press that opened it is not a key typed) */
			map_settings_name_keys_typed();
		}
	}
	else if (map_settings_row_tag(row, &kind, &slot))
	{
		map_settings_candidates_take(kind, slot);
		if (enter)
		{
			s_map_settings_menu.pick_cursor = map_settings_candidate_place();
			s_map_settings_menu.pick_scroll = 0;
			s_map_settings_menu.pick_return = s_map_settings_menu.page;
			s_map_settings_menu.page = _map_settings_page_pick;
		}
		else
		{
			int place = (map_settings_candidate_place() + s_map_settings_menu.pick_count + dir) %
				s_map_settings_menu.pick_count;

			map_settings_tag_set(kind, slot, s_map_settings_menu.pick_tags[place]);
		}
	}
	else if (row == _map_settings_row_team_config)
	{
		int slot_now = s_map_settings_menu.team_slot;
		int link = map_settings_link_step(slot_now, forge_map_settings_get()->link[slot_now], dir ? dir : 1);

		forge_map_settings_set_link(slot_now, link);
		if (link == _forge_link_custom)
			forge_set_status("%s: its own biped, hands and movement", map_settings_slot_name(slot_now));
		else
			forge_set_status("%s plays %s's settings", map_settings_slot_name(slot_now), map_settings_link_name(link));
	}
	else if (row == _map_settings_row_team_movement)
	{
		/* (SPARTAN, the default, or the biped's own) */
		int own = forge_map_settings_get()->movement[s_map_settings_menu.team_slot] != _forge_movement_own;

		forge_map_settings_set_movement(s_map_settings_menu.team_slot, own ? _forge_movement_own : _forge_movement_spartan);
		forge_set_status("%s movement: %s", map_settings_slot_name(s_map_settings_menu.team_slot),
			own ? "the biped's own" : "Spartan");
	}
	else if (row == _map_settings_row_render_distance)
	{
		/* (the game's, then further out: a valley, a whole world) */
		static float const distances[] = { 0.0f, 1536.0f, 2048.0f, 3072.0f, 4096.0f, 6144.0f, 8192.0f };
		int count = (int)(sizeof(distances) / sizeof(distances[0]));
		int place = 0, k;

		for (k = 0; k < count; k++)
		{
			if (forge_map_settings_get()->render_distance >= distances[k] - 0.5f)
				place = k;
		}
		place = (place + count + (dir ? dir : 1)) % count;
		forge_map_settings_set_render_distance(distances[place]);
		if (distances[place] > 0.0f)
			forge_set_status("Render distance: %.0f (the map is drawn further out)", distances[place]);
		else
			forge_set_status("Render distance: the game's (1024)");
	}
	else if (row == _map_settings_row_monitor_speed)
	{
		static float const speeds[] = { 1.0f, 1.5f, 2.0f, 3.0f, 4.0f, 6.0f, 8.0f, 12.0f };
		int count = (int)(sizeof(speeds) / sizeof(speeds[0]));
		int place = 0, k;

		for (k = 0; k < count; k++)
		{
			if (forge_map_settings_monitor_speed() >= speeds[k] - 0.01f)
				place = k;
		}
		place = (place + count + (dir ? dir : 1)) % count;
		forge_map_settings_set_monitor_speed(speeds[place]);
		forge_set_status("Monitor speed: %gx", (double)speeds[place]);
	}
	else if (row == _map_settings_row_atmosphere)
	{
		int atmosphere = forge_map_settings_get()->atmosphere;

		atmosphere = (atmosphere + NUMBER_OF_FORGE_ATMOSPHERES + dir) % NUMBER_OF_FORGE_ATMOSPHERES;
		forge_map_settings_set_atmosphere(atmosphere);
		forge_set_status("Atmosphere: %s", forge_map_settings_atmosphere_name(atmosphere));
	}
	else if (row >= _map_settings_row_fog_red && row <= _map_settings_row_fog_sky)
	{
		map_settings_fog_step(row, dir);
	}
}

/* ---------- public code (forge_3d_mode.c's) */

static void forge_map_settings_menu_show(void)
{
	if (g_forge.grabbed_live_index >= 0)
		forge_cancel_or_release_held_item();
	memset(&s_map_settings_menu, 0, sizeof(s_map_settings_menu));
	s_map_settings_menu.page = _map_settings_page_main;
	g_forge.active_modal = _forge_modal_map_settings;
	if (!forge_map_settings_can_edit())
		forge_set_status("Map settings: only the host changes them");
}

/* a TEST SPAWN wanted this frame (forge_3d_update leaves Forge mode) */
static int forge_map_settings_menu_test_requested(void)
{
	int requested = s_map_settings_menu.test_requested;

	s_map_settings_menu.test_requested = 0;
	return requested;
}

/* whether a row of a page of rows has a value a click on its left steps
back; and whether the page shown scrolls with the wheel */
static int forge_map_settings_menu_row_has_value(int index)
{
	int row = map_settings_row_at(s_map_settings_menu.page, index);
	int kind, slot;

	return row != NONE && (map_settings_row_tag(row, &kind, &slot) || row == _map_settings_row_team_movement ||
		row == _map_settings_row_team_config || row == _map_settings_row_render_distance ||
		row == _map_settings_row_monitor_speed ||
		(row >= _map_settings_row_atmosphere && row <= _map_settings_row_fog_sky));
}

static int forge_map_settings_menu_paging(void)
{
	return s_map_settings_menu.page != _map_settings_page_name;
}

/* the pointer over a row (or a key, or a list's row) */
static void forge_map_settings_menu_hover(int index)
{
	int page = s_map_settings_menu.page;

	if (page == _map_settings_page_pick)
		s_map_settings_menu.pick_cursor = PIN(index, 0, s_map_settings_menu.pick_count - 1);
	else if (page == _map_settings_page_name)
	{
		s_map_settings_menu.key_row = PIN(index / FORGE_MAP_NAME_KEY_COLUMNS, 0, FORGE_MAP_NAME_KEY_ROWS - 1);
		s_map_settings_menu.key_column = PIN(index % FORGE_MAP_NAME_KEY_COLUMNS, 0, FORGE_MAP_NAME_KEY_COLUMNS - 1);
	}
	else
	{
		int rows[FORGE_MAP_SETTINGS_MAX_PAGE_ROWS];
		int count = map_settings_page_rows(page, rows);

		s_map_settings_menu.cursors[page] = PIN(index, 0, count - 1);
	}
}

/* a frame's presses (each TRUE on the frame it was pressed) */
static void forge_map_settings_menu_input(int up, int down, int left, int right, int enter, int back)
{
	int page = s_map_settings_menu.page;

	if (page == _map_settings_page_name)
	{
		int typed = map_settings_name_keys_typed();

		/* (Backspace, Enter and Escape are the keyboard's: the on-screen
		keyboard's are the pad's A and B) */
		if (typed)
			return;
		if (back)
		{
			map_settings_page_back();
			return;
		}
		if (up)
			s_map_settings_menu.key_row = (s_map_settings_menu.key_row + FORGE_MAP_NAME_KEY_ROWS - 1) % FORGE_MAP_NAME_KEY_ROWS;
		if (down)
			s_map_settings_menu.key_row = (s_map_settings_menu.key_row + 1) % FORGE_MAP_NAME_KEY_ROWS;
		if (s_map_settings_menu.key_row == 4 && (left || right))
		{
			/* (a wide key a step) */
			int wide = s_map_settings_menu.key_column * 4 / FORGE_MAP_NAME_KEY_COLUMNS;

			wide = (wide + 4 + (right ? 1 : -1)) % 4;
			s_map_settings_menu.key_column = (wide * FORGE_MAP_NAME_KEY_COLUMNS + 3) / 4;
		}
		else if (left)
			s_map_settings_menu.key_column = (s_map_settings_menu.key_column + FORGE_MAP_NAME_KEY_COLUMNS - 1) %
				FORGE_MAP_NAME_KEY_COLUMNS;
		else if (right)
			s_map_settings_menu.key_column = (s_map_settings_menu.key_column + 1) % FORGE_MAP_NAME_KEY_COLUMNS;
		if (enter)
			map_settings_name_key_press();
		return;
	}
	if (page == _map_settings_page_pick)
	{
		int count = s_map_settings_menu.pick_count;

		if (back)
		{
			map_settings_page_back();
			return;
		}
		if (up)
			s_map_settings_menu.pick_cursor = (s_map_settings_menu.pick_cursor + count - 1) % count;
		if (down)
			s_map_settings_menu.pick_cursor = (s_map_settings_menu.pick_cursor + 1) % count;
		if (left)
			s_map_settings_menu.pick_cursor = MAX(0, s_map_settings_menu.pick_cursor - FORGE_MAP_SETTINGS_PICK_ROWS);
		if (right)
			s_map_settings_menu.pick_cursor = MIN(count - 1, s_map_settings_menu.pick_cursor + FORGE_MAP_SETTINGS_PICK_ROWS);
		if (enter && !map_settings_refuse())
		{
			map_settings_tag_set(s_map_settings_menu.pick_kind, s_map_settings_menu.pick_slot,
				s_map_settings_menu.pick_tags[s_map_settings_menu.pick_cursor]);
			map_settings_page_back();
		}
		return;
	}

	/* (a page of rows) */
	{
		int rows[FORGE_MAP_SETTINGS_MAX_PAGE_ROWS];
		int count = map_settings_page_rows(page, rows);
		int *cursor = &s_map_settings_menu.cursors[page];

		if (back)
		{
			map_settings_page_back();
			return;
		}
		if (up)
			*cursor = (*cursor + count - 1) % count;
		if (down)
			*cursor = (*cursor + 1) % count;
		if ((left || right || enter) && *cursor >= 0 && *cursor < count)
			map_settings_row_act(rows[*cursor], left ? -1 : 1, enter);
	}
}

static char const *map_settings_fog_summary(void)
{
	return forge_map_settings_atmosphere_name(forge_map_settings_get()->atmosphere);
}

/* a row's label and value */
static void map_settings_row_text(int row, char *label, size_t label_size, char *value, size_t value_size)
{
	struct forge_map_settings const *now = forge_map_settings_get();
	int kind, slot;
	int custom = now->atmosphere != _forge_atmosphere_map_default && now->atmosphere != _forge_atmosphere_clear;

	value[0] = 0;
	if (map_settings_row_tag(row, &kind, &slot))
	{
		if (row == _map_settings_row_sky)
			snprintf(label, label_size, "SKYBOX:");
		else if (row == _map_settings_row_team_biped)
			snprintf(label, label_size, "BIPED:");
		else if (row == _map_settings_row_team_hands)
			snprintf(label, label_size, "HANDS:");
		else
			snprintf(label, label_size, "%s:", map_settings_slot_name(slot));
		forge_map_settings_tag_label(forge_map_settings_tag(kind, slot), value, value_size);
		/* (named, but not in this level: a profile not played) */
		if (forge_map_settings_tag(kind, slot) == NONE &&
			((kind == _forge_map_kind_sky && now->sky[0]) ||
			(kind == _forge_map_kind_biped && now->biped[slot][0]) ||
			(kind == _forge_map_kind_hands && now->hands[slot][0])))
		{
			snprintf(value, value_size, "MISSING");
		}
		return;
	}
	switch (row)
	{
	case _map_settings_row_name:
		snprintf(label, label_size, "DISPLAY NAME:");
		snprintf(value, value_size, "%s", now->display_name[0] ? now->display_name : "THE FILE'S");
		break;
	case _map_settings_row_fog_menu:
		snprintf(label, label_size, "FOG SETTINGS");
		snprintf(value, value_size, "%s", map_settings_fog_summary());
		break;
	case _map_settings_row_bipeds_menu:
		snprintf(label, label_size, "PLAYER BIPEDS");
		break;
	case _map_settings_row_atmosphere:
		snprintf(label, label_size, "ATMOSPHERE:");
		snprintf(value, value_size, "%s", forge_map_settings_atmosphere_name(now->atmosphere));
		break;
	case _map_settings_row_fog_red:
	case _map_settings_row_fog_green:
	case _map_settings_row_fog_blue:
		snprintf(label, label_size, "FOG %s:", row == _map_settings_row_fog_red ? "RED" :
			row == _map_settings_row_fog_green ? "GREEN" : "BLUE");
		if (custom)
			snprintf(value, value_size, "%d", (int)(now->fog_color[row - _map_settings_row_fog_red] * 255.0f + 0.5f));
		else
			snprintf(value, value_size, "-");
		break;
	case _map_settings_row_fog_density:
		snprintf(label, label_size, "FOG DENSITY:");
		if (custom)
			snprintf(value, value_size, "%d%%", (int)(now->fog_density * 100.0f + 0.5f));
		else
			snprintf(value, value_size, "-");
		break;
	case _map_settings_row_fog_start:
		snprintf(label, label_size, "FOG START:");
		if (custom)
			snprintf(value, value_size, "%.0f", now->fog_start);
		else
			snprintf(value, value_size, "-");
		break;
	case _map_settings_row_fog_end:
		snprintf(label, label_size, "FOG FULL AT:");
		if (custom)
			snprintf(value, value_size, "%.0f", now->fog_opaque);
		else
			snprintf(value, value_size, "-");
		break;
	case _map_settings_row_fog_indoors:
		snprintf(label, label_size, "FOG INDOORS:");
		snprintf(value, value_size, "%s", now->fog_indoors ? "ON" : "OFF");
		break;
	case _map_settings_row_fog_sky:
		snprintf(label, label_size, "SKY IN FOG:");
		snprintf(value, value_size, "%s", now->sky_visibility == _forge_sky_shown ? "SHOWN" :
			now->sky_visibility == _forge_sky_hidden ? "HIDDEN" : "AUTO");
		break;
	case _map_settings_row_team_config:
		snprintf(label, label_size, "CONFIG:");
		snprintf(value, value_size, "%s", map_settings_link_name(now->link[s_map_settings_menu.team_slot]));
		break;
	case _map_settings_row_team_red:
	case _map_settings_row_team_blue:
	case _map_settings_row_team_ffa:
		/* (a team playing another's: map_settings_row_tag says no tag) */
		snprintf(label, label_size, "%s:", map_settings_slot_name(row - _map_settings_row_team_red));
		snprintf(value, value_size, "AS %s", map_settings_link_name(now->link[row - _map_settings_row_team_red]));
		break;
	case _map_settings_row_render_distance:
		snprintf(label, label_size, "RENDER DISTANCE:");
		if (now->render_distance > 0.0f)
			snprintf(value, value_size, "%.0f", now->render_distance);
		else
			snprintf(value, value_size, "DEFAULT (1024)");
		break;
	case _map_settings_row_monitor_speed:
		snprintf(label, label_size, "MONITOR SPEED:");
		snprintf(value, value_size, "%gX", (double)forge_map_settings_monitor_speed());
		break;
	case _map_settings_row_team_movement:
		snprintf(label, label_size, "MOVEMENT:");
		snprintf(value, value_size, "%s", now->movement[s_map_settings_menu.team_slot] == _forge_movement_own ?
			"BIPED'S OWN" : "SPARTAN");
		break;
	case _map_settings_row_test_spawn:
		snprintf(label, label_size, "TEST SPAWN");
		break;
	case _map_settings_row_undo:
	case _map_settings_row_redo:
	{
		int steps = forge_3d_history_steps(row == _map_settings_row_redo);

		snprintf(label, label_size, "%s", row == _map_settings_row_redo ? "REDO" : "UNDO");
		if (steps > 0)
			snprintf(value, value_size, "%d STEP%s", steps, steps == 1 ? "" : "S");
		else
			snprintf(value, value_size, "NONE");
		break;
	}
	default:
		label[0] = 0;
		break;
	}
}

/* what the row picked does, under the rows */
static char const *map_settings_row_help(int row)
{
	switch (row)
	{
	case _map_settings_row_name: return "The name map lists show. A types it.";
	case _map_settings_row_sky: return "A lists the skies: the map's, and a profile's.";
	case _map_settings_row_fog_menu: return "The map's fog: presets, or your own.";
	case _map_settings_row_bipeds_menu: return "What players spawn as, team by team.";
	case _map_settings_row_render_distance: return "How far the map is drawn: further for big maps.";
	case _map_settings_row_monitor_speed: return "How fast the Monitor flies in Forge.";
	case _map_settings_row_atmosphere: return "Fog presets; any value changed is CUSTOM.";
	case _map_settings_row_fog_indoors: return "Fog in rooms without a sky too (closer in).";
	case _map_settings_row_fog_sky: return "AUTO hides the sky behind full fog.";
	case _map_settings_row_team_red:
	case _map_settings_row_team_blue:
	case _map_settings_row_team_ffa: return "Left/right: its biped. A: its biped, hands, test.";
	case _map_settings_row_team_biped: return "Players spawn as it. Its hands are chosen with it.";
	case _map_settings_row_team_hands: return "First-person arms: the biped's, or any other.";
	case _map_settings_row_team_config: return "CUSTOM: its own. A team: plays that one's.";
	case _map_settings_row_team_movement: return "SPARTAN: strafing, jumps, slopes as the map's player.";
	case _map_settings_row_test_spawn: return "Play as it here, with a mirrored copy to watch.";
	case _map_settings_row_undo: return "Puts back your last move, turn, settings or placing.";
	case _map_settings_row_redo: return "Does again what UNDO put back.";
	default: return "Left/right changes it.";
	}
}

static char const *map_settings_page_title(int page, char *text, size_t size)
{
	switch (page)
	{
	case _map_settings_page_fog: return "FOG SETTINGS";
	case _map_settings_page_bipeds: return "PLAYER BIPEDS";
	case _map_settings_page_team:
		snprintf(text, size, "%s", map_settings_slot_name(s_map_settings_menu.team_slot));
		return text;
	default: return "MAP SETTINGS";
	}
}

static void forge_map_settings_menu_draw(short window_x0, short window_y0, long font_index)
{
	short box_w = 392;
	short px0 = (short)(window_x0 + 16);
	short py0 = (short)(window_y0 + 34);
	short px1 = (short)(px0 + box_w);
	char label[64];
	char value[96];
	int r;

	set_forge_hud_ui_scale(FORGE_IN_MATCH_HUD_SCALE, (float)px0, (float)py0);

	if (s_map_settings_menu.page == _map_settings_page_name)
	{
		short key_w = 34, key_h = 26;
		short box_h = (short)(34 + 40 + FORGE_MAP_NAME_KEY_ROWS * (key_h + 4) + 52);
		short kx0 = (short)(px0 + (box_w - FORGE_MAP_NAME_KEY_COLUMNS * (key_w + 2)) / 2);
		short ky0 = (short)(py0 + 80);
		char shown[FORGE_MAP_DISPLAY_NAME_MAX + 2];
		int row, column;

		draw_ce_pause_box(px0, py0, box_w, box_h, 1);
		draw_ce_menu_str(1, font_index, (short)(px0 + 14), (short)(py0 + 4), "DISPLAY NAME", 0xFFFFFFFFu);
		draw_ce_rounded_row_bar(1, (short)(px0 + 14), (short)(py0 + 38), (short)(px1 - 14), (short)(py0 + 64));
		snprintf(shown, sizeof(shown), "%s_", s_map_settings_menu.name);
		forge_text_fit(1, font_index, shown, (short)(box_w - 52));
		draw_ce_menu_str_box(1, font_index, (short)(px0 + 26), (short)(py0 + 41), (short)(px1 - 26), (short)(py0 + 64), 0,
			shown, 0xFFFFFFFFu);
		for (row = 0; row < FORGE_MAP_NAME_KEY_ROWS; row++)
		{
			for (column = 0; column < (row < 4 ? FORGE_MAP_NAME_KEY_COLUMNS : 4); column++)
			{
				int wide = row == 4;
				short x0 = wide ? (short)(kx0 + column * (FORGE_MAP_NAME_KEY_COLUMNS * (key_w + 2)) / 4) :
					(short)(kx0 + column * (key_w + 2));
				short x1 = wide ? (short)(kx0 + (column + 1) * (FORGE_MAP_NAME_KEY_COLUMNS * (key_w + 2)) / 4 - 2) :
					(short)(x0 + key_w);
				short y0 = (short)(ky0 + row * (key_h + 4));
				int picked = s_map_settings_menu.key_row == row && (wide ?
					s_map_settings_menu.key_column * 4 / FORGE_MAP_NAME_KEY_COLUMNS == column :
					s_map_settings_menu.key_column == column);
				char key[8];

				if (wide)
					snprintf(key, sizeof(key), "%s", column == 0 ? (s_map_settings_menu.lower ? "ABC" : "abc") :
						column == 1 ? "SPACE" : column == 2 ? "DEL" : "DONE");
				else
				{
					key[0] = s_map_name_keys[row][column];
					if (s_map_settings_menu.lower && key[0] >= 'A' && key[0] <= 'Z')
						key[0] = (char)(key[0] - 'A' + 'a');
					key[1] = 0;
				}
				draw_ce_rounded_row_bar((short)(picked ? 1 : 0), x0, y0, x1, (short)(y0 + key_h));
				forge_hit_add(_forge_hit_settings_row,
					row * FORGE_MAP_NAME_KEY_COLUMNS + (wide ? (column * FORGE_MAP_NAME_KEY_COLUMNS + 3) / 4 : column),
					x0, y0, x1, (short)(y0 + key_h));
				draw_ce_menu_str_box(1, font_index, x0, (short)(y0 + 2), x1, (short)(y0 + key_h), 2, key,
					picked ? 0xFFFFFFFFu : 0xFF2896FFu);
			}
		}
		draw_ce_menu_str_box(0, font_index, (short)(px0 + 14), (short)(py0 + box_h - 40), (short)(px1 - 14),
			(short)(py0 + box_h - 8), 0,
			g_forge.using_controller ? "{A} TYPE   {B} BACK   (DONE KEEPS IT)" : "TYPE IT. [ENTER] KEEPS IT, [ESC] BACK",
			0xFF2896FFu);
		return;
	}

	if (s_map_settings_menu.page == _map_settings_page_pick)
	{
		int count = s_map_settings_menu.pick_count;
		short box_h = (short)(34 + FORGE_MAP_SETTINGS_PICK_ROWS * 28 + 58);
		long picked_tag = s_map_settings_menu.pick_tags[s_map_settings_menu.pick_cursor];
		char title[64];

		if (s_map_settings_menu.pick_cursor < s_map_settings_menu.pick_scroll)
			s_map_settings_menu.pick_scroll = s_map_settings_menu.pick_cursor;
		if (s_map_settings_menu.pick_cursor >= s_map_settings_menu.pick_scroll + FORGE_MAP_SETTINGS_PICK_ROWS)
			s_map_settings_menu.pick_scroll = s_map_settings_menu.pick_cursor - FORGE_MAP_SETTINGS_PICK_ROWS + 1;
		if (s_map_settings_menu.pick_kind == _forge_map_kind_sky)
			snprintf(title, sizeof(title), "SKYBOX");
		else
			snprintf(title, sizeof(title), "%s %s", map_settings_slot_name(s_map_settings_menu.pick_slot),
				s_map_settings_menu.pick_kind == _forge_map_kind_biped ? "BIPED" : "HANDS");
		draw_ce_pause_box(px0, py0, box_w, box_h, 1);
		draw_ce_menu_str(1, font_index, (short)(px0 + 14), (short)(py0 + 4), title, 0xFFFFFFFFu);
		snprintf(value, sizeof(value), "%d / %d", s_map_settings_menu.pick_cursor + 1, count);
		draw_ce_menu_str_box(0, font_index, (short)(px1 - 110), (short)(py0 + 6), (short)(px1 - 14), (short)(py0 + 28), 1,
			value, 0xFF2896FFu);
		for (r = 0; r < FORGE_MAP_SETTINGS_PICK_ROWS; r++)
		{
			int place = s_map_settings_menu.pick_scroll + r;
			short rx0 = (short)(px0 + 14);
			short ry0 = (short)(py0 + 34 + r * 28);
			short rx1 = (short)(px1 - 14);
			short ry1 = (short)(ry0 + 25);
			int sel = place == s_map_settings_menu.pick_cursor;

			if (place >= count)
				break;
			draw_ce_rounded_row_bar((short)(sel ? 1 : 0), rx0, ry0, rx1, ry1);
			forge_hit_add(_forge_hit_settings_row, place, rx0, ry0, rx1, ry1);
			forge_map_settings_tag_label(s_map_settings_menu.pick_tags[place], label, sizeof(label));
			forge_text_fit(1, font_index, label, (short)(rx1 - rx0 - 20));
			draw_ce_menu_str_box(1, font_index, (short)(rx0 + 10), (short)(ry0 + 2), (short)(rx1 - 10), ry1, 0, label,
				sel ? 0xFFFFFFFFu : 0xFF2896FFu);
		}
		/* (the tag's whole path) */
		if (picked_tag != NONE && tag_get_name(picked_tag))
			snprintf(value, sizeof(value), "%s", tag_get_name(picked_tag));
		else
			snprintf(value, sizeof(value), "The map's own");
		forge_text_fit(0, font_index, value, (short)(box_w - 28));
		draw_ce_menu_str_box(0, font_index, (short)(px0 + 14), (short)(py0 + box_h - 50), (short)(px1 - 14),
			(short)(py0 + box_h - 30), 0, value, 0xFF2896FFu);
		draw_ce_menu_str_box(0, font_index, (short)(px0 + 14), (short)(py0 + box_h - 28), (short)(px1 - 14),
			(short)(py0 + box_h - 8), 0, g_forge.using_controller ? "{A} CHOOSE   {B} BACK" : "[ENTER] CHOOSE   [ESC] BACK",
			0xFF2896FFu);
		return;
	}

	/* (a page of rows) */
	{
		int page = s_map_settings_menu.page;
		int rows[FORGE_MAP_SETTINGS_MAX_PAGE_ROWS];
		int count = map_settings_page_rows(page, rows);
		int visible = MIN(count, FORGE_MAP_SETTINGS_VISIBLE_ROWS);
		short box_h = (short)(34 + visible * 28 + 34);
		int *cursor = &s_map_settings_menu.cursors[page];
		int *scroll = &s_map_settings_menu.scrolls[page];
		char title[64];

		if (*cursor < *scroll)
			*scroll = *cursor;
		if (*cursor >= *scroll + FORGE_MAP_SETTINGS_VISIBLE_ROWS)
			*scroll = *cursor - FORGE_MAP_SETTINGS_VISIBLE_ROWS + 1;
		draw_ce_pause_box(px0, py0, box_w, box_h, 1);
		draw_ce_menu_str(1, font_index, (short)(px0 + 14), (short)(py0 + 4), map_settings_page_title(page, title,
			sizeof(title)), 0xFFFFFFFFu);
		if (!forge_map_settings_can_edit())
		{
			draw_ce_menu_str_box(0, font_index, (short)(px1 - 160), (short)(py0 + 6), (short)(px1 - 14), (short)(py0 + 28), 1,
				"HOST'S", 0xFF2896FFu);
		}
		for (r = 0; r < visible; r++)
		{
			int place = *scroll + r;
			int row;
			short rx0 = (short)(px0 + 14);
			short ry0 = (short)(py0 + 34 + r * 28);
			short rx1 = (short)(px1 - 14);
			short ry1 = (short)(ry0 + 25);
			int sel = place == *cursor;
			uint32_t col = sel ? 0xFFFFFFFFu : 0xFF2896FFu;

			if (place >= count)
				break;
			row = rows[place];
			map_settings_row_text(row, label, sizeof(label), value, sizeof(value));
			draw_ce_rounded_row_bar((short)(sel ? 1 : 0), rx0, ry0, rx1, ry1);
			forge_hit_add(_forge_hit_settings_row, place, rx0, ry0, rx1, ry1);
			draw_ce_menu_str_box(1, font_index, (short)(rx0 + 10), (short)(ry0 + 2), (short)(rx1 - 10), ry1, 0, label, col);
			if (value[0])
			{
				short vx0 = (short)(rx0 + 150);
				short vx1 = (short)(rx1 - 8);
				int steps = forge_map_settings_menu_row_has_value(place);

				if (steps)
				{
					draw_ce_menu_bitmap_cropped("pc\\bitmaps\\arrow_sm_left", 0, vx0, (short)(ry0 + 6), (short)(vx0 + 6),
						(short)(ry0 + 18), 6, 12, 0xFFFFFFFFu);
					draw_ce_menu_bitmap_cropped("pc\\bitmaps\\arrow_sm_right", 0, (short)(vx1 - 6), (short)(ry0 + 6), vx1,
						(short)(ry0 + 18), 6, 12, 0xFFFFFFFFu);
				}
				forge_text_fit(0, font_index, value, (short)(vx1 - 10 - (vx0 + 10)));
				draw_ce_menu_str_box(0, font_index, (short)(vx0 + 10), (short)(ry0 + 3), (short)(vx1 - 10), ry1, 2, value,
					col);
			}
		}
		snprintf(value, sizeof(value), "%s", map_settings_row_help(map_settings_row_at(page, *cursor)));
		forge_text_fit(0, font_index, value, (short)(box_w - 28));
		draw_ce_menu_str_box(0, font_index, (short)(px0 + 14), (short)(py0 + box_h - 30), (short)(px1 - 14),
			(short)(py0 + box_h - 8), 0, value, 0xFF2896FFu);
	}
}
