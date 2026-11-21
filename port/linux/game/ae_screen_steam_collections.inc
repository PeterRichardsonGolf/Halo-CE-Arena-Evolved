/*
AE_SCREEN_STEAM_COLLECTIONS.INC

(AE) MAPS & MODS > WORKSHOP > SHOW: COLLECTIONS, written inside ae_screen_steam.c's own unit (it is included before the
screen's drawing) because it draws with the screen's rows, card and state. Two levels: the list of collections (the two the
offline playlists play, then the others a Workshop map of this PC is also in), and one collection opened (its action rows,
then its maps). The rules are ae_collections_rules.c, the model ae_collections_data.c. Getting a collection hands its maps to
the Steam client's queue, the same queue the one-item install uses; nothing here reads a Steam answer directly.
*/

#include "ae_collections_data.h"
#include "ae_decimal.h"

enum { COLL_ROWS_MOST = 12, COLL_ACTIONS_MOST = 2 };
/* the picture tile's cells: a map this PC has, a map it has not */
#define AE_COLL_PRESENT_FILL 0x2E6A80C8u
#define AE_COLL_ABSENT_FILL AE_COLOR_ROW
/* an update question comes from 1 GB, a first get from 500 MB */
#define COLL_UPDATE_ASK_BYTES (1000ull * 1000ull * 1000ull)

static struct
{
	int on;                          /* SHOW is COLLECTIONS */
	int sort;
	int level;                       /* 1 the list, 2 a collection opened */
	char open_id[AE_COLL_ID];
	int focus;                       /* level 1: the row; level 2: the action rows, then the maps */
	struct ae_list_view maps;
	struct ae_coll_model model;
	int model_valid;
	int scene;                       /* staged: AE_COLL_SCENE_* */
	int pending_dialog;              /* staged: 1 the get question, 2 the remove question, opened on the first layout */
	int rows[COLL_ROWS_MOST];        /* level 1: the model's collection of each row */
	int row_count;
	char dialog_map[AE_COLL_ID];     /* the map a remove question is about ("": the whole collection) */
	int logged_scene;
	/* staged: the focus the scene opens on, set once the page's first layout has chosen its own */
	int want_focus;
	/* a sentence the help line shows for a few seconds: what a press did or could not do */
	char note[200];
	unsigned long note_until;
	/* a remove handed to the client is under way (its result is told when it ends) */
	int removing;
	/* SEARCH's text: the collections whose name holds it (any case) are listed */
	char filter[48];
} C;

struct coll_action
{
	int kind;                        /* AE_COLL_ACT_*, or COLL_ACT_REMOVE */
	char label[72], value[40];
	int ink;
	int enabled;
};
enum { COLL_ACT_REMOVE = 100 };

static void coll_dialog_closed(void);

static void coll_note(char const *text)
{
	snprintf(C.note, sizeof(C.note), "%s", text);
	C.note_until = ae_motion_now() + 6000ul;
}

/* whether a case-blind copy of needle is in text */
static int coll_has_text(char const *text, char const *needle)
{
	size_t length = strlen(needle), index;

	if (!length)
		return 1;
	for (index = 0; text[index]; index++)
	{
		size_t each;

		for (each = 0; each < length && text[index + each]; each++)
		{
			char a = text[index + each], b = needle[each];

			if (a >= 'A' && a <= 'Z')
				a = (char)(a + 32);
			if (b >= 'A' && b <= 'Z')
				b = (char)(b + 32);
			if (a != b)
				break;
		}
		if (each == length)
			return 1;
	}
	return 0;
}

/* ---------- the model */

static void coll_model_refresh(int force)
{
	if (S.stage >= 0)
	{
		if (!C.model_valid || force)
		{
			ae_coll_data_stage(&C.model, C.scene, C.sort);
			C.model_valid = 1;
		}
		return;
	}
	ae_coll_data_read(&C.model, C.sort, force || !C.model_valid);
	C.model_valid = 1;
}

static int coll_active(void)
{
	return S.workshop && S.hub && C.on;
}

static int coll_level(void)
{
	return C.level;
}

static void coll_reset(void)
{
	memset(&C, 0, sizeof(C));
	C.level = 1;
	C.logged_scene = -1;
}

static char const *coll_sort_text(void)
{
	return ae_coll_sort_name(C.sort);
}

/* the collections the list shows: the playlists' two, then the others that hold a map this PC has or is getting */
static void coll_rows_build(void)
{
	int index, pass;

	C.row_count = 0;
	for (pass = 0; pass < 2; pass++)
		for (index = 0; index < C.model.count && C.row_count < COLL_ROWS_MOST; index++)
		{
			struct ae_coll const *coll = &C.model.colls[index];
			struct ae_coll_summary summary;

			if ((pass == 0) != (coll->playlist != 0))
				continue;
			ae_coll_summarize(coll, &summary);
			if (pass == 1 && summary.present + summary.waiting + summary.getting == 0)
				continue;
			if (!coll_has_text(coll->name, C.filter))
				continue;
			C.rows[C.row_count++] = index;
		}
}

static struct ae_coll const *coll_open(void)
{
	int index;

	for (index = 0; index < C.model.count; index++)
		if (!strcmp(C.model.colls[index].id, C.open_id))
			return &C.model.colls[index];
	return NULL;
}

/* the level 2 action rows: the first action (when there is one), and REMOVE FROM THIS PC */
static int coll_actions(struct ae_coll const *coll, struct ae_coll_summary *summary, struct coll_action *out)
{
	int count = 0, kind, room_ok;
	char const *remove_label = "REMOVE FROM THIS PC";

	ae_coll_summarize(coll, summary);
	kind = ae_coll_first_action(summary);
	room_ok = ae_coll_room_for(summary, C.model.free_bytes);
	if (kind != AE_COLL_ACT_NONE)
	{
		memset(&out[count], 0, sizeof(out[count]));
		out[count].kind = kind;
		out[count].enabled = 1;
		ae_coll_action_text(kind, summary, room_ok, out[count].label, (int)sizeof(out[count].label), out[count].value,
			(int)sizeof(out[count].value), &out[count].ink);
		count++;
	}
	memset(&out[count], 0, sizeof(out[count]));
	out[count].kind = COLL_ACT_REMOVE;
	snprintf(out[count].label, sizeof(out[count].label), "%s", remove_label);
	snprintf(out[count].value, sizeof(out[count].value), "%d MAP%s", summary->removable, summary->removable == 1 ? "" : "S");
	out[count].enabled = summary->removable > 0;
	count++;
	return count;
}

static int coll_action_count(struct ae_coll const *coll)
{
	struct ae_coll_summary summary;
	struct coll_action actions[COLL_ACTIONS_MOST];

	return coll ? coll_actions(coll, &summary, actions) : 0;
}

static char const *coll_name_upper(char const *name, char *out, int size)
{
	int index;

	for (index = 0; name[index] && index + 1 < size; index++)
		out[index] = name[index] >= 'a' && name[index] <= 'z' ? (char)(name[index] - 32) : name[index];
	out[index] = 0;
	return out;
}

/* SEARCH on the collections: their names must hold the text (any case); an empty text lists them all */
static void coll_search(char const *text)
{
	snprintf(C.filter, sizeof(C.filter), "%s", text ? text : "");
	C.focus = 0;
	platform_log("ae steam: collections search");
}

/* ---------- entering and leaving the view */

static void coll_enter(void)
{
	C.level = 1;
	C.focus = 0;
	C.open_id[0] = 0;
	C.model_valid = 0;
	coll_model_refresh(1);
	coll_rows_build();
	platform_log("ae steam: collections view%s", S.stage >= 0 ? " (staged)" : "");
}

static unsigned long long coll_id_number(char const *id)
{
	return ae_decimal_u64(id);
}

static void coll_log_scene(void)
{
	if (S.stage >= 0 && C.scene != C.logged_scene)
	{
		static char const *const words[AE_COLL_SCENES] = { "none", "show", "list", "open", "map", "getting", "installed",
			"get question", "remove question", "no room" };

		C.logged_scene = C.scene;
		platform_log("ae steam: collections scene %s", words[C.scene]);
	}
}

/* ---------- actions */

static void coll_get(struct ae_coll const *coll)
{
	struct ae_coll_summary summary;
	char ids[AE_COLL_ITEMS][AE_COLL_ID];
	unsigned long long numbers[AE_COLL_ITEMS];
	int count, index;

	ae_coll_summarize(coll, &summary);
	count = ae_coll_get_ids(coll, &summary, ids, AE_COLL_ITEMS);
	if (!count)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	for (index = 0; index < count; index++)
		numbers[index] = coll_id_number(ids[index]);
	platform_log("ae steam: collection get asked: %d maps%s", count, S.stage >= 0 ? " (staged)" : "");
	if (client_live())
	{
		int taken = ae_steam_client_collection_get(numbers, count);

		if (!taken)
		{
			coll_note("Nothing was started: those maps are already here, waiting, or in another get. Steam may also be busy.");
			ae_sound_request(AE_SOUND_FAILURE, 0);
			return;
		}
		if (taken < count)
			coll_note("Some of the maps were already here, waiting or in another get, and were left out.");
	}
	else
	{
		C.scene = AE_COLL_SCENE_GETTING;
		coll_model_refresh(1);
	}
	ae_sound_request(AE_SOUND_FORWARD, 0);
}

static void coll_stop(struct ae_coll const *coll)
{
	char ids[AE_COLL_ITEMS][AE_COLL_ID];
	unsigned long long numbers[AE_COLL_ITEMS];
	int count, index;

	count = ae_coll_stop_ids(coll, ids, AE_COLL_ITEMS);
	for (index = 0; index < count; index++)
		numbers[index] = coll_id_number(ids[index]);
	platform_log("ae steam: collection stop asked: %d maps%s", count, S.stage >= 0 ? " (staged)" : "");
	if (client_live())
		ae_steam_client_collection_stop(numbers, count);
	else
	{
		C.scene = AE_COLL_SCENE_OPEN;
		coll_model_refresh(1);
	}
	ae_sound_request(AE_SOUND_BACK, 0);
}

static void coll_remove(struct ae_coll const *coll)
{
	char ids[AE_COLL_ITEMS][AE_COLL_ID];
	int count = ae_coll_remove_ids(coll, ids, AE_COLL_ITEMS);

	platform_log("ae steam: collection remove asked: %d maps%s", count, S.stage >= 0 ? " (staged)" : "");
	if (!count)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	if (client_live())
	{
		int in_use, taken = ae_coll_data_remove_start(ids, count, &in_use);
		char words[200];

		snprintf(words, sizeof(words), "%d map%s being removed.%s%s", taken, taken == 1 ? "" : "s",
			in_use ? " A map a lobby or game is using now was left." : "", taken < count - in_use ?
			" Some were not taken: they are being downloaded or not installed by this game." : "");
		coll_note(taken ? words : in_use ? "That map is in use now, so it was left." : "None of those maps can be removed now.");
		C.removing = taken > 0;
		if (!taken)
		{
			ae_sound_request(AE_SOUND_FAILURE, 0);
			return;
		}
	}
	else
	{
		/* (the staged copy: the removed maps are not on it any more) */
		C.scene = AE_COLL_SCENE_NONE;
		coll_model_refresh(1);
	}
	ae_sound_request(AE_SOUND_FORWARD, 0);
}

/* one map of the collection: get it, take it out of the queue, or remove it */
static void coll_map_remove(struct ae_coll_item const *item)
{
	char ids[1][AE_COLL_ID];

	snprintf(ids[0], AE_COLL_ID, "%s", item->id);
	platform_log("ae steam: collection map remove asked%s", S.stage >= 0 ? " (staged)" : "");
	if (client_live())
	{
		int in_use, taken = ae_coll_data_remove_start(ids, 1, &in_use);

		coll_note(taken ? "Removing the map." : in_use ? "That map is in use now, so it was left." :
			"That map can't be removed now.");
		C.removing = taken > 0;
		if (!taken)
		{
			ae_sound_request(AE_SOUND_FAILURE, 0);
			return;
		}
	}
	ae_sound_request(AE_SOUND_FORWARD, 0);
}

static void coll_map_get(struct ae_coll_item const *item)
{
	unsigned long long number = coll_id_number(item->id);

	platform_log("ae steam: collection map get asked%s", S.stage >= 0 ? " (staged)" : "");
	if (client_live() && !ae_steam_client_collection_get(&number, 1))
	{
		coll_note("Nothing was started: the map is already here, waiting, or in another get.");
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	ae_sound_request(AE_SOUND_FORWARD, 0);
}

static void coll_map_stop(struct ae_coll_item const *item)
{
	unsigned long long number = coll_id_number(item->id);

	platform_log("ae steam: collection map stop asked%s", S.stage >= 0 ? " (staged)" : "");
	if (client_live())
		ae_steam_client_collection_stop(&number, 1);
	ae_sound_request(AE_SOUND_BACK, 0);
}

/* ---------- the questions */

static void coll_get_picked(short choice, void *context)
{
	struct ae_coll const *coll = coll_open();

	(void)context;
	coll_dialog_closed();
	if (choice == 0 && coll)
		coll_get(coll);
}

static void coll_remove_picked(short choice, void *context)
{
	struct ae_coll const *coll = coll_open();

	(void)context;
	coll_dialog_closed();
	if (choice != 1 || !coll)
		return;
	if (C.dialog_map[0])
	{
		int index;

		for (index = 0; index < coll->count; index++)
			if (!strcmp(coll->items[index].id, C.dialog_map))
				coll_map_remove(&coll->items[index]);
	}
	else
		coll_remove(coll);
}

static void coll_dialog_closed(void)
{
	S.asking = 0;
}

static void coll_get_ask(struct ae_coll const *coll)
{
	static char title[AE_COLL_NAME + 16], body[400], get[64];
	struct ae_coll_summary summary;
	struct ae_dialog_spec spec;
	char bytes[24];
	int maps;

	ae_coll_summarize(coll, &summary);
	maps = summary.absent > 0 ? summary.absent : summary.updates;
	ae_coll_size_text(ae_coll_get_bytes(&summary), bytes, (int)sizeof(bytes));
	snprintf(title, sizeof(title), "Get %.*s?", AE_COLL_NAME, coll->name);
	ae_coll_get_body(coll->name, &summary, C.model.free_bytes, body, (int)sizeof(body));
	snprintf(get, sizeof(get), "GET %d MAP%s%s%s", maps, maps == 1 ? "" : "S", bytes[0] ? " \xE2\x80\xA2 " : "", bytes);
	memset(&spec, 0, sizeof(spec));
	spec.kind = AE_DIALOG_CONFIRM;
	spec.title = title;
	spec.body = body;
	spec.choices[0] = get;
	spec.choices[1] = ae_string(AE_STR_CANCEL_CAPS);
	spec.choice_count = 2;
	spec.safe_choice = 0;
	spec.cancel_choice = 1;
	spec.has_cancel = 1;
	spec.density = S.flow.density;
	spec.bounds = S.flow.frame.rect;
	spec.picked = coll_get_picked;
	if (ae_dialog_open(&spec, AE_OWNER_ANY))
	{
		S.asking = 1;
		ae_sound_request(AE_SOUND_FORWARD, 0);
	}
}

static void coll_remove_ask(struct ae_coll const *coll, struct ae_coll_item const *map)
{
	static char title[AE_COLL_NAME + 24], body[460], remove[48];
	struct ae_coll_summary summary;
	struct ae_dialog_spec spec;
	char name[AE_COLL_TITLE];
	char bytes[24];

	ae_coll_summarize(coll, &summary);
	C.dialog_map[0] = 0;
	if (map)
	{
		snprintf(C.dialog_map, sizeof(C.dialog_map), "%s", map->id);
		ae_coll_map_name(map->title, name, (int)sizeof(name));
		snprintf(title, sizeof(title), "Remove %.*s?", AE_COLL_TITLE - 1, name);
		snprintf(body, sizeof(body), "The map is deleted from this PC. The offline playlists skip maps this PC doesn't have.");
		snprintf(remove, sizeof(remove), "REMOVE 1 MAP");
	}
	else
	{
		ae_coll_size_text(summary.removable_bytes, bytes, (int)sizeof(bytes));
		snprintf(title, sizeof(title), "Remove %.*s?", AE_COLL_NAME, coll->name);
		ae_coll_remove_body(coll->name, coll, &summary, body, (int)sizeof(body));
		snprintf(remove, sizeof(remove), "REMOVE %d MAP%s%s%s", summary.removable, summary.removable == 1 ? "" : "S",
			bytes[0] ? " \xE2\x80\xA2 " : "", bytes);
	}
	memset(&spec, 0, sizeof(spec));
	spec.kind = AE_DIALOG_CONFIRM;
	spec.title = title;
	spec.body = body;
	spec.choices[0] = ae_string(AE_STR_CANCEL_CAPS);
	spec.choices[1] = remove;
	spec.choice_count = 2;
	spec.safe_choice = 0;
	spec.cancel_choice = 0;
	spec.has_cancel = 1;
	spec.destructive_choices = 1u << 1;
	spec.density = S.flow.density;
	spec.bounds = S.flow.frame.rect;
	spec.picked = coll_remove_picked;
	if (ae_dialog_open(&spec, AE_OWNER_ANY))
	{
		S.asking = 1;
		ae_sound_request(AE_SOUND_FORWARD, 0);
	}
}

/* ---------- opening and leaving a collection */

static void coll_focus_first_action(struct ae_coll const *coll)
{
	C.focus = 0;
	C.maps.list.focus = -1;
	(void)coll;
}

static void coll_open_row(int row)
{
	struct ae_coll const *coll;

	if (row < 0 || row >= C.row_count)
		return;
	coll = &C.model.colls[C.rows[row]];
	if (coll->cut)
	{
		coll_note("Too many maps for this view: it holds 64 at most. Y opens the collection's Steam page.");
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	snprintf(C.open_id, sizeof(C.open_id), "%s", coll->id);
	C.level = 2;
	ae_list_view_init(&C.maps, (short)coll->count, LIST_ROWS + 1);
	coll_focus_first_action(coll);
	platform_log("ae steam: collection opened: %s", coll->name);
	ae_sound_request(AE_SOUND_FORWARD, 0);
}

static void coll_back(void)
{
	int row;

	C.level = 1;
	for (row = 0; row < C.row_count; row++)
		if (!strcmp(C.model.colls[C.rows[row]].id, C.open_id))
			C.focus = row;
	platform_log("ae steam: collection closed");
	ae_sound_request(AE_SOUND_BACK, 0);
}

static void coll_page_open(void)
{
	char url[96];
	struct ae_coll const *coll = coll_open();
	char const *id = NULL;

	if (C.level == 1 && C.focus >= 0 && C.focus < C.row_count)
		id = C.model.colls[C.rows[C.focus]].id;
	else if (coll)
	{
		int actions = coll_action_count(coll);

		id = C.focus >= actions && C.focus - actions < coll->count ? coll->items[C.focus - actions].id : coll->id;
	}
	if (!id || !id[0])
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	snprintf(url, sizeof(url), "https://steamcommunity.com/sharedfiles/filedetails/?id=%.20s", id);
	if (!client_live())
	{
		platform_log("ae steam: collection Steam page would open (staged)");
		staged_note();
		return;
	}
	platform_log("ae steam: collection Steam page opened");
	platform_open_url(url);
	ae_sound_request(AE_SOUND_FORWARD, 0);
}

/* A on the focused row */
static void coll_accept(void)
{
	struct ae_coll const *coll;
	struct ae_coll_summary summary;
	struct coll_action actions[COLL_ACTIONS_MOST];
	int count;

	if (C.level == 1)
	{
		coll_open_row(C.focus);
		return;
	}
	coll = coll_open();
	if (!coll)
		return;
	count = coll_actions(coll, &summary, actions);
	if (C.focus >= count)
	{
		struct ae_coll_item const *map = &coll->items[C.focus - count];

		switch (map->state)
		{
		case AE_COLL_ITEM_ABSENT:
		case AE_COLL_ITEM_UPDATE:
			if (!C.model.signed_in)
				ae_sound_request(AE_SOUND_FAILURE, 0);
			else
				coll_map_get(map);
			break;
		case AE_COLL_ITEM_WAITING:
		case AE_COLL_ITEM_GETTING:
			coll_map_stop(map);
			break;
		default:
			if (map->steam_owned)
				ae_sound_request(AE_SOUND_FAILURE, 0);
			else
				coll_remove_ask(coll, map);
			break;
		}
		return;
	}
	switch (actions[C.focus].kind)
	{
	case AE_COLL_ACT_STOP:
	case AE_COLL_ACT_STOP_WAITING:
		coll_stop(coll);
		break;
	case COLL_ACT_REMOVE:
		if (actions[C.focus].enabled)
			coll_remove_ask(coll, NULL);
		else
			ae_sound_request(AE_SOUND_FAILURE, 0);
		break;
	default:
	{
		int room_ok = ae_coll_room_for(&summary, C.model.free_bytes);

		if (C.model.signed_in && ae_coll_get_unknown(&summary) > 0)
		{
			coll_note("Steam has not told the sizes yet: wait a moment, then A again.");
			ae_sound_request(AE_SOUND_FAILURE, 0);
			break;
		}
		if (C.model.signed_in && summary.failed_get > 0)
		{
			coll_note("Steam did not answer for some maps. A tries again in a moment.");
			ae_sound_request(AE_SOUND_FAILURE, 0);
			break;
		}
		if (!C.model.signed_in || !room_ok)
		{
			/* (no room: nothing was started; the list is read again so a freed drive is seen) */
			if (!room_ok)
				coll_model_refresh(1);
			ae_sound_request(AE_SOUND_FAILURE, 0);
			break;
		}
		/* (a get under 500 MB, and updates under 1 GB, are queued at once) */
		if (actions[C.focus].kind == AE_COLL_ACT_UPDATES ? ae_coll_get_bytes(&summary) >= COLL_UPDATE_ASK_BYTES :
			ae_coll_get_asks(ae_coll_get_bytes(&summary)))
			coll_get_ask(coll);
		else
			coll_get(coll);
		break;
	}
	}
}

/* ---------- moving and typing */

/* up and down: 1 when handled here; leaving the list upward at its first row puts the focus on SORT */
static void coll_move(struct ae_event const *event)
{
	int up = event->action == AE_ACTION_UP;

	if (C.level == 1)
	{
		if (up && C.focus <= 0)
			S.focus = IT_SORT;
		else if (!up && C.focus + 1 < C.row_count)
			C.focus++;
		else if (up)
			C.focus--;
		else
			return;
		ae_sound_request(AE_SOUND_CURSOR, event->repeat);
		return;
	}
	{
		struct ae_coll const *coll = coll_open();
		int actions = coll_action_count(coll);

		if (!coll)
			return;
		if (C.focus >= actions)
		{
			if (up && C.maps.list.focus <= 0)
				C.focus = actions - 1;
			else
			{
				ae_list_view_event(&C.maps, event);
				C.focus = actions + C.maps.list.focus;
				return;
			}
		}
		else if (up && C.focus > 0)
			C.focus--;
		else if (!up && C.focus + 1 < actions)
			C.focus++;
		else if (!up && coll->count > 0)
		{
			C.focus = actions;
			ae_list_set_focus(&C.maps.list, 0);
		}
		else
			return;
		ae_sound_request(AE_SOUND_CURSOR, event->repeat);
	}
}

static void coll_page(int delta)
{
	struct ae_coll const *coll = coll_open();
	int actions = coll_action_count(coll);

	if (C.level != 2 || !coll || coll->count <= 0)
		return;
	{
		struct ae_event event;

		memset(&event, 0, sizeof(event));
		event.action = delta > 0 ? AE_ACTION_PAGE_DOWN : AE_ACTION_PAGE_UP;
		ae_list_view_event(&C.maps, &event);
		C.focus = actions + C.maps.list.focus;
	}
	ae_sound_request(AE_SOUND_CURSOR, 0);
}

/* the SHOW row's step (any of its four values): the client's TYPE follows the first three */
static void coll_show_step(int direction)
{
	int current = C.on ? 3 : S.status.type % TYPES, target = (current + (direction > 0 ? 1 : 3)) % 4, steps;

	if (target == 3)
		C.on = 1;
	else
	{
		steps = (target - S.status.type % TYPES + TYPES) % TYPES;
		if (client_live())
			for (current = 0; current < steps; current++)
				ae_steam_client_filter_step(1);
		else
			S.status.type = target;
		C.on = 0;
		coll_pictures_release();
	}
	ae_list_set_focus(&S.list.list, 0);
	ae_sound_request(AE_SOUND_CURSOR, 0);
	if (C.on)
		coll_enter();
	else
		platform_log("ae steam: collections view left");
}

static void coll_sort_step(void)
{
	C.sort = (C.sort + 1) % AE_COLL_SORTS;
	coll_model_refresh(1);
	C.focus = 0;
	ae_sound_request(AE_SOUND_FORWARD, 0);
}

/* ---------- drawing */

static void coll_group_label(float x, float y, float width, char const *left, char const *right)
{
	struct ae_density const *d = &S.flow.density;
	float size = ae_size_minor(d) * 0.9f;

	ae_draw_text_tracked(AE_FONT_BODY, size, 0.09f, x + u(10.0f), y, AE_ALIGN_LEFT, AE_COLOR_MUTED, left);
	if (right && right[0])
		ae_draw_text_tracked(AE_FONT_BODY, size, 0.09f, x + width - u(10.0f), y, AE_ALIGN_RIGHT, AE_COLOR_MUTED, right);
}

/* a row of this view: its hit is HIT_COLL, index; ink picks the value's look */
static float coll_row(float x, float y, float width, int index, char const *label, char const *value, int ink, int focused,
	int enabled, int chevron)
{
	struct ae_row row;
	char fitted[100];

	memset(&row, 0, sizeof(row));
	ae_fit_text(AE_FONT_ROW, ae_size_text(&S.flow.density), label, width * 0.62f, fitted, sizeof(fitted));
	row.label = fitted;
	row.value = value && value[0] ? value : NULL;
	row.flags = focused ? AE_ROW_FOCUSED : 0;
	if (ink == AE_COLL_INK_WARNING)
	{
		row.flags |= AE_ROW_DISABLED | AE_ROW_NO_ARROWS;
		row.reason = row.value;
		row.value = NULL;
	}
	else if (ink == AE_COLL_INK_ACCENT)
		row.flags |= chevron ? AE_ROW_LINK : AE_ROW_NO_ARROWS;
	else
		row.flags |= chevron ? AE_ROW_SUBSCREEN : AE_ROW_STATUS;
	if (!enabled && ink != AE_COLL_INK_WARNING)
		row.flags |= AE_ROW_STATUS;
	return ae_widget_row(&S.flow.density, x, y, width, &row, HIT_COLL, (short)index);
}

/* level 1: the collections under the count line */
static void coll_draw_list(float x, float y, float width)
{
	struct ae_density const *d = &S.flow.density;
	float minor = ae_size_minor(d) * 0.9f;
	char count[48], value[48], size[24];
	int row, other_label = 0, playlist_label = 0;

	coll_model_refresh(0);
	coll_rows_build();
	if (C.focus >= C.row_count)
		C.focus = C.row_count - 1;
	if (C.focus < 0)
		C.focus = 0;
	snprintf(count, sizeof(count), "%d COLLECTION%s", C.row_count, C.row_count == 1 ? "" : "S");
	ae_draw_text_tracked(AE_FONT_BODY, minor, 0.09f, x + u(10.0f), y, AE_ALIGN_LEFT, AE_COLOR_MUTED, "PAGE 1 OF 1");
	ae_draw_text_tracked(AE_FONT_BODY, minor, 0.09f, x + width - u(10.0f), y, AE_ALIGN_RIGHT, AE_COLOR_MUTED,
		C.model.loading ? "ASKING STEAM..." : C.model.empty ? "NONE" : count);
	y += u(38.0f);
	(void)size;
	if (C.row_count == 0)
	{
		ae_draw_text(AE_FONT_BODY, ae_size_text(d) * 0.9f, x + u(10.0f), y + u(8.0f), AE_ALIGN_LEFT, AE_COLOR_MUTED,
			C.model.loading ? "Asking Steam..." : C.model.empty ? "Steam gave no collections. Try again later." :
			C.filter[0] ? "No collection has that name." : "No collections yet.");
		return;
	}
	for (row = 0; row < C.row_count; row++)
	{
		struct ae_coll const *coll = &C.model.colls[C.rows[row]];
		struct ae_coll_summary summary;
		int ink, room_ok;

		if (coll->playlist && !playlist_label)
		{
			coll_group_label(x, y, width, "FOR THE OFFLINE PLAYLISTS", NULL);
			y += u(36.0f);
			playlist_label = 1;
		}
		if (!coll->playlist && !other_label)
		{
			coll_group_label(x, y + u(6.0f), width, "YOUR WORKSHOP MAPS ARE ALSO IN", NULL);
			y += u(42.0f);
			other_label = 1;
		}
		ae_coll_summarize(coll, &summary);
		room_ok = ae_coll_room_for(&summary, C.model.free_bytes);
		if (!C.model.signed_in && summary.present < summary.count)
		{
			snprintf(value, sizeof(value), "%d OF %d ON THIS PC", summary.present, summary.count);
			ink = AE_COLL_INK_MUTED;
		}
		else
			ae_coll_row_value(&summary, room_ok, value, (int)sizeof(value), &ink);
		y += coll_row(x, y, width - u(20.0f), row, coll->name, value, ink,
			S.focus == IT_COLL && C.focus == row, 1, 1) + u(6.0f);
	}
}

static void coll_map_item(void *context, short item, float x, float y, float width, float height, unsigned int flags)
{
	struct ae_coll const *coll = context;
	struct ae_row row;
	char name[AE_COLL_TITLE], value[24], fitted[100];
	int ink;

	(void)height;
	if (!coll || item < 0 || item >= coll->count)
		return;
	ae_coll_map_name(coll->items[item].title, name, (int)sizeof(name));
	ae_coll_item_value(&coll->items[item], value, (int)sizeof(value), &ink);
	ae_fit_text(AE_FONT_ROW, ae_size_text(&S.flow.density), name, width * 0.62f, fitted, sizeof(fitted));
	memset(&row, 0, sizeof(row));
	row.label = fitted;
	row.value = value[0] ? value : NULL;
	row.flags = flags | (ink == AE_COLL_INK_ACCENT ? AE_ROW_NO_ARROWS : AE_ROW_STATUS);
	ae_widget_row(&S.flow.density, x, y, width, &row, HIT_COLL_LIST, item);
}

/* level 2: the crumb, the action rows, the count line and the maps */
static void coll_draw_open(float x, float y, float width)
{
	struct ae_density const *d = &S.flow.density;
	struct ae_coll const *coll;
	struct ae_coll_summary summary;
	struct coll_action actions[COLL_ACTIONS_MOST];
	char crumb[160], upper[AE_COLL_NAME], left[40], right[80];
	int count, index;
	float bottom = ae_flow_content_bottom(&S.flow) - u(96.0f);

	coll_model_refresh(0);
	coll = coll_open();
	if (!coll)
	{
		C.level = 1;
		return;
	}
	count = coll_actions(coll, &summary, actions);
	snprintf(crumb, sizeof(crumb), "WORKSHOP \xE2\x80\xBA COLLECTIONS \xE2\x80\xBA %s", coll_name_upper(coll->name, upper, (int)sizeof(upper)));
	ae_draw_text_tracked(AE_FONT_BODY, ae_size_minor(d) * 0.9f, 0.09f, x, y, AE_ALIGN_LEFT, AE_COLOR_MUTED, crumb);
	y += u(32.0f);
	if (C.focus >= count + coll->count)
		C.focus = count + coll->count - 1;
	for (index = 0; index < count; index++)
		y += coll_row(x, y, width, index, actions[index].label, actions[index].value, actions[index].ink,
			S.focus == IT_COLL && C.focus == index, actions[index].enabled, 0) + u(6.0f);
	y += u(10.0f);
	snprintf(left, sizeof(left), "ITS MAPS");
	if (summary.waiting + summary.getting > 0)
		snprintf(right, sizeof(right), "%d OF %d ON THIS PC \xE2\x80\xA2 %d WAITING", summary.present, summary.count,
			summary.waiting + summary.getting);
	else
		snprintf(right, sizeof(right), "%d OF %d ON THIS PC", summary.present, summary.count);
	coll_group_label(x, y, width, left, right);
	y += u(38.0f);
	if (coll->count <= 0)
	{
		ae_draw_text(AE_FONT_BODY, ae_size_text(d) * 0.9f, x + u(10.0f), y + u(8.0f), AE_ALIGN_LEFT, AE_COLOR_MUTED,
			"Steam named no maps for it.");
		return;
	}
	ae_list_set_count(&C.maps.list, (short)coll->count);
	if (C.focus >= count)
		ae_list_set_focus(&C.maps.list, (short)(C.focus - count));
	else
		C.maps.list.focus = -1;
	ae_widget_list(d, &C.maps, x, y, width, bottom - y, coll_map_item, (void *)coll, HIT_COLL_LIST);
}

/* ---------- the card */

static void coll_card_frame(float x, float y, float width, float height, unsigned int stripe)
{
	ae_draw_rect(x, y, width, height, 0.0f, AE_COLOR_PANEL);
	ae_draw_outline(x, y, width, height, 0.0f, u(1.0f), AE_COLOR_RULE);
	ae_draw_rect(x, y, width, u(4.0f), 0.0f, stripe);
}

/* ---------- the maps' pictures

A map this PC has shows the picture the game already holds for it (ae_setup_map_picture_named: its Workshop item's own,
kept by name). A map it has not shows its Workshop preview from the client's kept previews (ae_steam_client_collection_picture:
one is fetched at a time, so a tile fills one picture after another); each is made an image here once and kept in a few
slots, the one used longest ago let go for a new one, all let go when the screen goes. The client is never opened from here:
signed out, or with no picture yet, the cell stays plain. A staged page makes its pictures up and asks nothing. */

enum { COLL_PICS = 12 };
static struct
{
	unsigned long long key;       /* the item (0: free) */
	int image;                    /* 0: none could be made for key */
	int width, height;
	unsigned int used;
} coll_pics[COLL_PICS];
static unsigned int coll_pixels[AE_STEAM_PICTURE_SIDE * AE_STEAM_PICTURE_SIDE];
static unsigned char coll_rgba[AE_STEAM_PICTURE_SIDE * AE_STEAM_PICTURE_SIDE * 4];

static void coll_pictures_release(void)
{
	int slot;

	for (slot = 0; slot < COLL_PICS; slot++)
		if (coll_pics[slot].image)
			ae_draw_image_release(coll_pics[slot].image);
	memset(coll_pics, 0, sizeof(coll_pics));
}

/* (a staged page's picture: the WORKSHOP page's made-up one, its colours turned a step by the item) */
static void coll_staged_pixels(unsigned long long key, unsigned int *pixels)
{
	int index, turn = (int)(key % 3ull);

	picture_staged(pixels);
	for (index = 0; index < AE_STEAM_PICTURE_SIDE * AE_STEAM_PICTURE_SIDE && turn; index++)
	{
		unsigned int pixel = pixels[index], red = pixel >> 16 & 255u, green = pixel >> 8 & 255u, blue = pixel & 255u;

		pixels[index] = turn == 1 ? (pixel & 0xFF000000u) | green << 16 | blue << 8 | red :
			(pixel & 0xFF000000u) | blue << 16 | red << 8 | green;
	}
}

static int coll_pixel_black(unsigned int pixel)
{
	return (pixel >> 16 & 255u) <= 10u && (pixel >> 8 & 255u) <= 10u && (pixel & 255u) <= 10u;
}

/* whether every pixel of a row (columns left up to right) or of a column (rows top up to bottom) is black */
static int coll_row_black(int row, int left, int right)
{
	int x;

	for (x = left; x < right; x++)
		if (!coll_pixel_black(coll_pixels[row * AE_STEAM_PICTURE_SIDE + x]))
			return 0;
	return 1;
}

static int coll_column_black(int column, int top, int bottom)
{
	int y;

	for (y = top; y < bottom; y++)
		if (!coll_pixel_black(coll_pixels[y * AE_STEAM_PICTURE_SIDE + column]))
			return 0;
	return 1;
}

/* the client's square with its black bars (a wide picture is fitted whole on black) cut off, as RGBA in coll_rgba; the
picture's width and height. A sliver or an all black square is kept whole */
static void coll_crop(int *width, int *height)
{
	int side = AE_STEAM_PICTURE_SIDE, top = 0, bottom = side, left = 0, right = side, x, y;

	while (top < side && coll_row_black(top, 0, side))
		top++;
	while (bottom > top && coll_row_black(bottom - 1, 0, side))
		bottom--;
	while (left < side && coll_column_black(left, top, bottom))
		left++;
	while (right > left && coll_column_black(right - 1, top, bottom))
		right--;
	if (bottom - top < 16 || right - left < 16)
	{
		top = left = 0;
		bottom = right = side;
	}
	for (y = top; y < bottom; y++)
		for (x = left; x < right; x++)
		{
			unsigned int pixel = coll_pixels[y * side + x];
			unsigned char *out = coll_rgba + 4 * ((y - top) * (right - left) + (x - left));

			out[0] = (unsigned char)(pixel >> 16);
			out[1] = (unsigned char)(pixel >> 8);
			out[2] = (unsigned char)pixel;
			out[3] = 255;
		}
	*width = right - left;
	*height = bottom - top;
}

/* the image of a map's picture and its size, or 0 (the plain cell) */
static int coll_item_image(struct ae_coll_item const *item, int *width, int *height)
{
	unsigned long long key;
	int slot, pick = 0, image;

	*width = *height = 0;
	if (!item)
		return 0;
	if (item->picture_name[0])
	{
		image = ae_setup_map_picture_named(item->picture_name, width, height);
		if (image && *width > 0 && *height > 0 && ae_draw_image_live(image))
			return image;
		*width = *height = 0;
	}
	key = coll_id_number(item->id);
	if (!key)
		return 0;
	for (slot = 0; slot < COLL_PICS; slot++)
		if (coll_pics[slot].key == key)
		{
			if (!coll_pics[slot].image || ae_draw_image_live(coll_pics[slot].image))
			{
				coll_pics[slot].used = ae_draw_frame();
				*width = coll_pics[slot].width;
				*height = coll_pics[slot].height;
				return coll_pics[slot].image;
			}
			memset(&coll_pics[slot], 0, sizeof(coll_pics[slot]));
		}
	if (S.stage >= 0)
		coll_staged_pixels(key, coll_pixels);
	else if (ae_steam_client_collection_picture(key, NULL) != AE_STEAM_PICTURE_SIDE ||
		ae_steam_client_collection_picture(key, coll_pixels) != AE_STEAM_PICTURE_SIDE)
		return 0;
	for (slot = 0; slot < COLL_PICS; slot++)
	{
		if (!coll_pics[slot].key)
		{
			pick = slot;
			break;
		}
		if (ae_draw_frame() - coll_pics[slot].used > ae_draw_frame() - coll_pics[pick].used)
			pick = slot;
	}
	if (coll_pics[pick].image)
		ae_draw_image_release(coll_pics[pick].image);
	coll_crop(width, height);
	coll_pics[pick].key = key;
	coll_pics[pick].width = *width;
	coll_pics[pick].height = *height;
	coll_pics[pick].used = ae_draw_frame();
	coll_pics[pick].image = ae_draw_image_load(coll_rgba, *width, *height);
	if (!coll_pics[pick].image)
		*width = *height = 0;
	return coll_pics[pick].image;
}

/* an image filling a box, its middle shown and the rest cut off by the box */
static void coll_image_cover(int image, int width, int height, float x, float y, float box_w, float box_h)
{
	float scale = box_w / (float)width > box_h / (float)height ? box_w / (float)width : box_h / (float)height;
	float draw_w = (float)width * scale, draw_h = (float)height * scale;

	ae_draw_clip_push(x, y, box_w, box_h);
	ae_draw_image(image, x + (box_w - draw_w) * 0.5f, y + (box_h - draw_h) * 0.5f, draw_w, draw_h, 0xFFFFFFFFu);
	ae_draw_clip_pop();
}

/* an image whole in a box, centred */
static void coll_image_fit(int image, int width, int height, float x, float y, float box_w, float box_h)
{
	float scale = box_w / (float)width < box_h / (float)height ? box_w / (float)width : box_h / (float)height;
	float draw_w = (float)width * scale, draw_h = (float)height * scale;

	ae_draw_image(image, x + (box_w - draw_w) * 0.5f, y + (box_h - draw_h) * 0.5f, draw_w, draw_h, 0xFFFFFFFFu);
}

/* the picture box: a 4 x 2 tile of the maps' pictures (a map with none: its plain cell and name), the last cell "+n" over
its picture when there are more than eight. coll NULL: the offline playlists' maps, one of each collection in turn */
static float coll_picture(float x, float y, float width, struct ae_coll const *coll)
{
	struct ae_density const *d = &S.flow.density;
	float inset = u(23.0f), gap = u(6.0f), box_w = width - 2.0f * inset, box_h = u(260.0f);
	float cell_w = (box_w - 3.0f * gap) / 4.0f, cell_h = (box_h - gap) / 2.0f;
	struct ae_coll_item const *cells[8];
	int index, shown = coll ? coll->count : 0, turn, member;

	for (index = 0; index < 8; index++)
		cells[index] = coll && index < shown ? &coll->items[index] : NULL;
	if (!coll)
		for (turn = 0, index = 0; turn < AE_COLL_ITEMS && index < 8; turn++)
			for (member = 0; member < C.model.count && index < 8; member++)
				if (C.model.colls[member].playlist && turn < C.model.colls[member].count)
					cells[index++] = &C.model.colls[member].items[turn];
	for (index = 0; index < 8; index++)
	{
		float cx = x + inset + (float)(index % 4) * (cell_w + gap), cy = y + u(24.0f) + (float)(index / 4) * (cell_h + gap);
		int more = coll && coll->count > 8 && index == 7, width_in = 0, height_in = 0, image;
		unsigned int fill = cells[index] && cells[index]->state != AE_COLL_ITEM_ABSENT ? AE_COLL_PRESENT_FILL :
			AE_COLL_ABSENT_FILL;

		ae_draw_rect(cx, cy, cell_w, cell_h, 0.0f, fill);
		image = coll_item_image(cells[index], &width_in, &height_in);
		if (image && width_in > 0 && height_in > 0)
			coll_image_cover(image, width_in, height_in, cx, cy, cell_w, cell_h);
		if (more)
		{
			char text[16];

			if (image)
				ae_draw_rect(cx, cy, cell_w, cell_h, 0.0f, 0x000000A8u);
			snprintf(text, sizeof(text), "+%d", coll->count - 7);
			ae_draw_text(AE_FONT_TITLE, u(30.0f), cx + cell_w * 0.5f, ae_flow_cap_top(AE_FONT_TITLE, u(30.0f), cy + cell_h * 0.5f),
				AE_ALIGN_CENTER, AE_COLOR_TITLE, text);
		}
		else if (cells[index] && !image)
		{
			char name[AE_COLL_TITLE], fitted[AE_COLL_TITLE];

			ae_coll_map_name(cells[index]->title, name, (int)sizeof(name));
			ae_fit_text(AE_FONT_BODY, ae_size_minor(d) * 0.84f, name, cell_w - u(16.0f), fitted, sizeof(fitted));
			ae_draw_text(AE_FONT_BODY, ae_size_minor(d) * 0.84f, cx + u(8.0f), cy + cell_h - u(34.0f), AE_ALIGN_LEFT,
				AE_COLOR_TEXT, fitted);
		}
	}
	return u(24.0f) + box_h + u(20.0f);
}

static float coll_title(float x, float y, float width, char const *text)
{
	char upper[AE_COLL_NAME + 8], fitted[AE_COLL_NAME + 8];

	coll_name_upper(text, upper, (int)sizeof(upper));
	ae_fit_text(AE_FONT_TITLE, u(40.0f), upper, width - u(46.0f), fitted, sizeof(fitted));
	ae_draw_text(AE_FONT_TITLE, u(40.0f), x + u(23.0f), y, AE_ALIGN_LEFT, AE_COLOR_TITLE, fitted);
	return u(62.0f);
}

static void coll_card_text(float x, float y, float width, float bottom, char const *text, unsigned int color)
{
	struct ae_density const *d = &S.flow.density;
	float body = ae_size_text(d) * 0.88f, line = u(31.0f);
	char lines[4][160];
	int count = ae_wrap_text(AE_FONT_BODY, body, text, width - u(46.0f), lines, 3), each;

	for (each = 0; each < count && y + line * (float)each < bottom - u(20.0f); each++)
		ae_draw_text(AE_FONT_BODY, body, x + u(23.0f), y + line * (float)each, AE_ALIGN_LEFT, color, lines[each]);
}

/* a bar with its label line over it: "THE COLLECTION" left in muted, the words right, the filled part in the accent */
static float coll_bar(float x, float y, float width, char const *label, char const *words, float fraction)
{
	struct ae_density const *d = &S.flow.density;
	float inset = u(23.0f), inner = width - 2.0f * inset, size = ae_size_minor(d) * 0.84f;

	ae_draw_text_tracked(AE_FONT_BODY, size, 0.09f, x + inset, y, AE_ALIGN_LEFT, AE_COLOR_MUTED, label);
	ae_draw_text(AE_FONT_ROW, ae_size_text(d) * 0.83f, x + width - inset, y - u(3.0f), AE_ALIGN_RIGHT, AE_COLOR_TITLE, words);
	if (fraction > 1.0f)
		fraction = 1.0f;
	if (fraction < 0.0f)
		fraction = 0.0f;
	ae_draw_rect(x + inset, y + u(34.0f), inner, u(6.0f), 0.0f, AE_COLOR_RULE);
	ae_draw_rect(x + inset, y + u(34.0f), inner * fraction, u(6.0f), 0.0f, AE_COLOR_ACCENT);
	return u(62.0f);
}

/* card_fact with the value's colour of its own */
static float coll_fact(float x, float y, float width, char const *label, char const *value, unsigned int color)
{
	struct ae_density const *d = &S.flow.density;
	float inset = u(23.0f), label_size = ae_size_minor(d) * 0.84f, value_size = ae_size_text(d) * 0.83f;
	float label_width = ae_draw_text_tracked_width(AE_FONT_BODY, label_size, 0.09f, label);
	char fitted[160];

	ae_draw_text_tracked(AE_FONT_BODY, label_size, 0.09f, x + inset, y + u(4.0f), AE_ALIGN_LEFT, AE_COLOR_MUTED, label);
	ae_fit_text(AE_FONT_ROW, value_size, value, width - 2.0f * inset - label_width - u(20.0f), fitted, sizeof(fitted));
	ae_draw_text(AE_FONT_ROW, value_size, x + width - inset, y, AE_ALIGN_RIGHT, color, fitted);
	ae_draw_rect(x + inset, y + u(31.0f), width - 2.0f * inset, u(1.0f), 0.0f, AE_COLOR_RULE);
	return u(40.0f);
}

static void coll_card_show(float x, float y, float width, float height)
{
	struct ae_coll_summary summary;
	char value[48], bytes[24];
	int index, maps = 0, here = 0, playlists = 0;
	unsigned long long get = 0;
	float py;

	coll_card_frame(x, y, width, height, AE_COLOR_ACCENT);
	py = y + coll_picture(x, y, width, NULL);
	py += coll_title(x, py, width, "COLLECTIONS");
	for (index = 0; index < C.model.count; index++)
	{
		if (!C.model.colls[index].playlist)
			continue;
		ae_coll_summarize(&C.model.colls[index], &summary);
		playlists++;
		maps += summary.count;
		here += summary.present;
		get += summary.absent_bytes;
	}
	snprintf(value, sizeof(value), "%d COLLECTION%s", playlists, playlists == 1 ? "" : "S");
	py += card_fact(x, py, width, "FOR THE OFFLINE PLAYLISTS", value);
	snprintf(value, sizeof(value), "%d OF %d", here, maps);
	py += card_fact(x, py, width, "THEIR MAPS ON THIS PC", value);
	ae_coll_size_text(get, bytes, (int)sizeof(bytes));
	if (bytes[0])
		py += card_fact(x, py, width, "TO GET THEM ALL", bytes);
	ae_coll_size_text(C.model.free_bytes, bytes, (int)sizeof(bytes));
	py += card_fact(x, py, width, "FREE ON THE MAPS DRIVE", bytes[0] ? bytes : "NOT KNOWN");
	py += u(8.0f);
	coll_card_text(x, py, width, y + height, "A collection is a set of Workshop maps kept together by its publisher. Get one, and "
		"all its maps install one after another.", AE_COLOR_TEXT);
}

/* the opened collection's card, for the focused action row */
static void coll_card_collection(float x, float y, float width, float height, struct ae_coll const *coll, int action_kind)
{
	struct ae_coll_summary summary;
	char value[64], bytes[24];
	int room_ok;
	float py;
	unsigned int stripe = AE_COLOR_ACCENT;

	ae_coll_summarize(coll, &summary);
	room_ok = ae_coll_room_for(&summary, C.model.free_bytes);
	if (action_kind == AE_COLL_ACT_GET || action_kind == AE_COLL_ACT_GET_ALL)
		if (!room_ok)
			stripe = AE_COLOR_WARNING;
	coll_card_frame(x, y, width, height, stripe);
	py = y;
	if (stripe == AE_COLOR_WARNING)
	{
		py += u(24.0f);
		py += coll_title(x, py, width, "Not enough room");
		ae_coll_size_text(ae_coll_get_bytes(&summary), bytes, (int)sizeof(bytes));
		snprintf(value, sizeof(value), "%d MAP%s \xE2\x80\xA2 %s", summary.absent, summary.absent == 1 ? "" : "S", bytes);
		py += card_fact(x, py, width, "TO GET", value);
		ae_coll_size_text(C.model.free_bytes, bytes, (int)sizeof(bytes));
		py += coll_fact(x, py, width, "FREE ON THE MAPS DRIVE", bytes, AE_COLOR_WARNING);
		snprintf(value, sizeof(value), "%d OF %d MAPS", summary.present, summary.count);
		py += card_fact(x, py, width, "ON THIS PC", value);
		py += u(8.0f);
		{
			char words[260], need[24];

			ae_coll_size_text(ae_coll_get_bytes(&summary), need, (int)sizeof(need));
			snprintf(words, sizeof(words), "%s needs %s; the maps drive has %s free. Remove maps you don't play on INSTALLED, or "
				"free space on that drive, then A tries again.", coll->name, need, bytes);
			coll_card_text(x, py, width, y + height, words, AE_COLOR_TEXT);
		}
		return;
	}
	py += coll_picture(x, y, width, coll);
	py += coll_title(x, py, width, coll->name);
	if (summary.getting + summary.waiting > 0)
	{
		unsigned long long done_bytes = summary.total_bytes - summary.absent_bytes - summary.left_bytes;
		char words[80], total[24], done[24], now_label[96], now_words[64];
		float fraction = summary.total_bytes ? (float)((double)done_bytes / (double)summary.total_bytes) : 0.0f;
		int index;

		py += coll_fact(x, py, width, "STATE", "GETTING IT", AE_COLOR_ACCENT);
		ae_coll_size_text(summary.left_bytes, bytes, (int)sizeof(bytes));
		snprintf(value, sizeof(value), "%d MAP%s%s%s", summary.waiting + summary.getting,
			summary.waiting + summary.getting == 1 ? "" : "S", bytes[0] ? " \xE2\x80\xA2 " : "", bytes);
		py += card_fact(x, py, width, "TO GO", value);
		py += u(8.0f);
		ae_coll_size_text(summary.total_bytes, total, (int)sizeof(total));
		ae_coll_size_text(done_bytes, done, (int)sizeof(done));
		snprintf(words, sizeof(words), "%d OF %d \xE2\x80\xA2 %s OF %s", summary.present, summary.count, done, total);
		py += coll_bar(x, py, width, "THE COLLECTION", words, fraction);
		for (index = 0; index < coll->count; index++)
		{
			struct ae_coll_item const *item = &coll->items[index];
			char name[AE_COLL_TITLE], upper[AE_COLL_TITLE], got[24], all[24];

			if (item->state != AE_COLL_ITEM_GETTING)
				continue;
			ae_coll_map_name(item->title, name, (int)sizeof(name));
			coll_name_upper(name, upper, (int)sizeof(upper));
			snprintf(now_label, sizeof(now_label), "NOW: %s", upper);
			ae_coll_size_text(item->size * (unsigned long long)item->percent / 100ull, got, (int)sizeof(got));
			ae_coll_size_text(item->size, all, (int)sizeof(all));
			snprintf(now_words, sizeof(now_words), "%s OF %s", got[0] ? got : "0 MB", all);
			py += coll_bar(x, py, width, now_label, now_words, (float)item->percent / 100.0f);
			break;
		}
		py += u(6.0f);
		coll_card_text(x, py, width, y + height, "It keeps going while you play or use other pages.", AE_COLOR_MUTED);
		return;
	}
	if (action_kind == AE_COLL_ACT_NONE || action_kind == AE_COLL_ACT_UPDATES || summary.absent == 0)
	{
		snprintf(value, sizeof(value), "%d OF %d MAPS", summary.present, summary.count);
		py += card_fact(x, py, width, "ON THIS PC", value);
		if (summary.updates)
		{
			ae_coll_size_text(summary.update_bytes, bytes, (int)sizeof(bytes));
			snprintf(value, sizeof(value), "%d \xE2\x80\xA2 %s", summary.updates, bytes);
			py += coll_fact(x, py, width, "UPDATES", value, AE_COLOR_ACCENT);
		}
		ae_coll_size_text(summary.removable_bytes, bytes, (int)sizeof(bytes));
		if (bytes[0])
			py += card_fact(x, py, width, "REMOVE FREES", bytes);
		if (summary.steam_kept)
		{
			snprintf(value, sizeof(value), "%d MAP%s", summary.steam_kept, summary.steam_kept == 1 ? "" : "S");
			py += card_fact(x, py, width, "KEPT BY STEAM", value);
		}
		if (coll->playlist)
			py += card_fact(x, py, width, "USED BY", "OFFLINE PLAYLISTS");
		py += u(8.0f);
		if (summary.steam_kept)
			coll_card_text(x, py, width, y + height, "Maps this game did not install stay: Steam's own, or put there by hand.", AE_COLOR_TEXT);
		return;
	}
	snprintf(value, sizeof(value), "%d OF %d MAPS", summary.present, summary.count);
	py += card_fact(x, py, width, "ON THIS PC", value);
	ae_coll_size_text(summary.absent_bytes, bytes, (int)sizeof(bytes));
	snprintf(value, sizeof(value), "%d MAP%s%s%s", summary.absent, summary.absent == 1 ? "" : "S", bytes[0] ? " \xE2\x80\xA2 " : "",
		bytes);
	py += card_fact(x, py, width, "TO GET", value);
	ae_coll_size_text(C.model.free_bytes, bytes, (int)sizeof(bytes));
	py += card_fact(x, py, width, "FREE ON THE MAPS DRIVE", bytes[0] ? bytes : "NOT KNOWN");
	if (coll->playlist)
		py += card_fact(x, py, width, "USED BY", "OFFLINE PLAYLISTS");
	py += u(8.0f);
	coll_card_text(x, py, width, y + height, coll->playlist ?
		"The maps the offline playlists play. Get it and every map installs one after another." :
		"A set of Workshop maps kept together by its publisher.", AE_COLOR_TEXT);
}

/* a map of the opened collection */
static void coll_card_map(float x, float y, float width, float height, struct ae_coll const *coll,
	struct ae_coll_item const *map)
{
	char name[AE_COLL_TITLE], value[64], upper[AE_COLL_NAME], size[24];
	int ink;
	float py;

	coll_card_frame(x, y, width, height, AE_COLOR_ACCENT);
	py = y + u(24.0f);
	ae_draw_rect(x + u(23.0f), py, width - u(46.0f), u(260.0f), 0.0f, AE_COLL_ABSENT_FILL);
	{
		int picture_w = 0, picture_h = 0, image = coll_item_image(map, &picture_w, &picture_h);

		if (image && picture_w > 0 && picture_h > 0)
			coll_image_fit(image, picture_w, picture_h, x + u(23.0f), py, width - u(46.0f), u(260.0f));
	}
	py += u(260.0f) + u(20.0f);
	ae_coll_map_name(map->title, name, (int)sizeof(name));
	py += coll_title(x, py, width, name);
	ae_coll_item_value(map, value, (int)sizeof(value), &ink);
	if (map->state == AE_COLL_ITEM_ABSENT)
		snprintf(value, sizeof(value), "NOT ON THIS PC");
	py += card_fact(x, py, width, "STATE", value);
	ae_coll_size_text(map->size, size, (int)sizeof(size));
	if (size[0])
		py += card_fact(x, py, width, "SIZE", size);
	py += card_fact(x, py, width, "IN", coll_name_upper(coll->name, upper, (int)sizeof(upper)));
	py += u(8.0f);
	coll_card_text(x, py, width, y + height, "A gets this one map only. A map of the collection can be removed on its own too.",
		AE_COLOR_TEXT);
}

static void coll_draw_card(float x, float y, float width, float height)
{
	struct ae_coll_summary summary;
	struct coll_action actions[COLL_ACTIONS_MOST];
	struct ae_coll const *coll;
	int count;

	if (S.focus != IT_COLL)
	{
		coll_card_show(x, y, width, height);
		return;
	}
	if (C.level == 1)
	{
		if (C.row_count <= 0)
		{
			coll_card_show(x, y, width, height);
			return;
		}
		coll = &C.model.colls[C.rows[C.focus < C.row_count ? C.focus : 0]];
		ae_coll_summarize(coll, &summary);
		coll_card_collection(x, y, width, height, coll, ae_coll_first_action(&summary));
		return;
	}
	coll = coll_open();
	if (!coll)
		return;
	count = coll_actions(coll, &summary, actions);
	if (C.focus >= count && C.focus - count < coll->count)
		coll_card_map(x, y, width, height, coll, &coll->items[C.focus - count]);
	else
		coll_card_collection(x, y, width, height, coll, actions[C.focus < count ? C.focus : 0].kind);
}

/* ---------- words and prompts */

static char const *coll_help(void)
{
	struct ae_coll const *coll;
	struct ae_coll_summary summary;
	struct coll_action actions[COLL_ACTIONS_MOST];
	int count;

	if (S.stage >= 0 && ae_motion_now() < S.staged_until)
		return "TEST PAGE: nothing is sent to Steam. This press did nothing.";
	if (C.note[0] && ae_motion_now() < C.note_until)
		return C.note;
	switch (S.focus)
	{
	case IT_SEARCH:
		return "A name or word of a collection: the list keeps the collections whose name has it. An empty search clears it.";
	case IT_SHOW:
		return "SHOW: COLLECTIONS lists sets of Workshop maps you can get in one go. Left and right change what the page lists.";
	case IT_SORT:
		return "A changes the order: the offline playlists' two first, then by name or by size.";
	default:
		break;
	}
	if (C.level == 1)
		return "A opens the collection: get all its maps at once, or look at them one by one.";
	coll = coll_open();
	if (!coll)
		return "";
	count = coll_actions(coll, &summary, actions);
	if (C.focus >= count)
		return "A gets this one map only. A map of the collection can be removed on its own too.";
	switch (actions[C.focus].kind)
	{
	case AE_COLL_ACT_STOP:
		return "A stops it: the map being fetched is dropped, the ones already here stay. The MAPS & MODS row on the main menu "
			"shows DOWNLOADING while it runs.";
	case AE_COLL_ACT_STOP_WAITING:
		return "A takes the waiting maps out of the download queue.";
	case AE_COLL_ACT_UPDATES:
		return "Gets the maps an author has updated. The ones that are current stay as they are.";
	case COLL_ACT_REMOVE:
		if (!actions[C.focus].enabled)
			return summary.steam_kept ? "This game did not install these, so it can't remove them. Steam's own: unsubscribe on their Steam pages (Y)." :
				"None of its maps are on this PC.";
		return "Removes the maps this game installed. The offline playlists skip maps this PC doesn't have.";
	default:
		if (!ae_coll_room_for(&summary, C.model.free_bytes))
			return "Not enough room for it: nothing was started. A tries again; a map can still be got on its own.";
		if (!C.model.signed_in)
			return "Getting a collection needs STEAM ACCOUNT signed in.";
		return "Gets every map of the collection not on this PC, one after another. They keep coming while you play or use other pages.";
	}
}

static void coll_prompts(void)
{
	struct ae_flow_screen *flow = &S.flow;
	char const *a = "Open";

	flow->prompt_count = 0;
	if (S.focus == IT_SEARCH)
		a = "Search";
	else if (S.focus == IT_SHOW)
		a = "Change";
	else if (S.focus == IT_SORT)
		a = "Change";
	else if (C.level == 2)
	{
		struct ae_coll const *coll = coll_open();
		struct ae_coll_summary summary;
		struct coll_action actions[COLL_ACTIONS_MOST];
		int count = coll ? coll_actions(coll, &summary, actions) : 0;

		if (coll && C.focus >= count && C.focus - count < coll->count)
		{
			struct ae_coll_item const *map = &coll->items[C.focus - count];

			a = map->state == AE_COLL_ITEM_ABSENT || map->state == AE_COLL_ITEM_UPDATE ? "Install" :
				map->state == AE_COLL_ITEM_PRESENT ? (map->steam_owned ? "Kept" : "Remove") : "Stop";
		}
		else if (coll && C.focus < count)
		{
			switch (actions[C.focus].kind)
			{
			case AE_COLL_ACT_STOP:
			case AE_COLL_ACT_STOP_WAITING:
				a = "Stop";
				break;
			case COLL_ACT_REMOVE:
				a = "Remove";
				break;
			case AE_COLL_ACT_UPDATES:
				a = "Get updates";
				break;
			default:
				a = ae_coll_room_for(&summary, C.model.free_bytes) ? "Get them" : "Try again";
				break;
			}
		}
	}
	flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, a, AE_ACTION_ACCEPT);
	if (C.level == 1)
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_X, "Search", AE_ACTION_X);
	if (S.focus == IT_COLL)
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_Y, "Steam page", AE_ACTION_Y);
	if (S.focus == IT_COLL && C.level == 2)
	{
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_LEFT_TRIGGER, "Page", AE_ACTION_PAGE_UP);
		flow->prompts[flow->prompt_count - 1].pair = 1;
	}
	if (C.level == 1)
	{
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_LEFT_SHOULDER, "Pages", AE_ACTION_TAB_PREVIOUS);
		flow->prompts[flow->prompt_count - 1].pair = 1;
	}
	flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, C.level == 2 ? "Collections" : ae_string(AE_STR_BACK),
		AE_ACTION_BACK);
}

/* ---------- the frame's part: staged dialogs and the removals */

static void coll_update(void)
{
	if (!coll_active())
		return;
	coll_log_scene();
	if (C.want_focus)
	{
		S.focus = (short)C.want_focus;
		C.want_focus = 0;
	}
	if (client_live() && C.removing)
	{
		int pending, done, failed;

		ae_coll_data_remove_status(&pending, &done, &failed);
		if (!pending)
		{
			char words[160];

			C.removing = 0;
			snprintf(words, sizeof(words), failed ? "Removed %d map%s; %d could not be removed (see the log)." :
				"Removed %d map%s.", done, done == 1 ? "" : "s", failed);
			coll_note(words);
			platform_log("ae steam: collection remove finished: %d removed, %d failed", done, failed);
			coll_model_refresh(1);
		}
	}
	if (C.pending_dialog && S.flow.density.metrics)
	{
		struct ae_coll const *coll = coll_open();
		int which = C.pending_dialog;

		C.pending_dialog = 0;
		if (coll && which == 1)
			coll_get_ask(coll);
		else if (coll && which == 2)
			coll_remove_ask(coll, NULL);
	}
}

/* the pointer on this view's rows: 1 when it took the pointer */
static int coll_pointer(struct ae_pointer const *pointer)
{
	struct ae_hit hit;
	struct ae_coll const *coll = C.level == 2 ? coll_open() : NULL;
	int actions = coll_action_count(coll);

	if (coll && coll->count > 0)
	{
		short item = ae_list_view_pointer(&C.maps, pointer, HIT_COLL_LIST);

		if (pointer->moved && C.maps.hover >= 0 && C.maps.hover < coll->count)
		{
			S.focus = IT_COLL;
			C.focus = actions + C.maps.hover;
		}
		if (item >= 0 && item < coll->count)
		{
			int was = S.focus == IT_COLL && C.focus == actions + item;

			S.focus = IT_COLL;
			C.focus = actions + item;
			if (was && pointer->left_clicks)
				coll_accept();
			return 1;
		}
	}
	if ((pointer->moved || pointer->left_clicks) && ae_hit_at(pointer->x, pointer->y, 0, &hit) && hit.id == HIT_COLL &&
		hit.index >= 0 && hit.index < (C.level == 1 ? C.row_count : actions))
	{
		int was = S.focus == IT_COLL && C.focus == hit.index;

		S.focus = IT_COLL;
		C.focus = hit.index;
		if (pointer->left_clicks && was)
			coll_accept();
		return 1;
	}
	return 0;
}
