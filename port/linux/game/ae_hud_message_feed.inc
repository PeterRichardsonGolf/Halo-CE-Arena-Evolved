/*
AE_HUD_MESSAGE_FEED.INC

The FEED's lines in the game's message list (hud_messaging.c includes this where the message structures are in range: the
slots are its own). The marks' calls print here with hud_print_feed_message; they take the slots kill lines and the game's
other lines do not need (ae_message_slot_choice, ae_marker_rules.h): a kill replaces the oldest feed line first, a feed line
takes a free slot or the oldest feed line and gets none when every slot holds a kill, and with no feed line in the list the
game's lines choose as they always did.
*/

#include "ae_hud_style.h"
#include "ae_marker_rules.h"

/* set while a feed line looks for its slot (find_free_hud_message reads it) */
static boolean ae_hud_message_new_is_feed;

/* the slot the game's choice leaves for a line when none is free and none holds its item: NULL for no line */
static struct hud_message_definition *ae_hud_message_slot(
	struct hud_messaging_datum_definition *datum,
	boolean new_is_feed)
{
	struct ae_message_slot slots[NUMBER_OF_HUD_MESSAGES_PER_DATUM];
	short index, slot;

	for (index = 0; index < NUMBER_OF_HUD_MESSAGES_PER_DATUM; index++)
	{
		slots[index].valid = datum->messages[index].valid;
		slots[index].is_feed = datum->messages[index].feed;
		slots[index].time = datum->messages[index].time;
	}
	slot = ae_message_slot_choice(slots, NUMBER_OF_HUD_MESSAGES_PER_DATUM, new_is_feed);

	return slot == NONE ? NULL : &datum->messages[slot];
}

/* TRUE when the line has a slot (a feed line has none where every slot holds a kill) */
boolean hud_print_feed_message(
	short local_player_index,
	wchar_t const *text,
	int merge_count)
{
	struct hud_messaging_datum_definition *datum;
	struct hud_message_definition *message = NULL;
	short index;

	if (!hud_messaging_globals || local_player_index < 0 || local_player_index >= NUMBER_OF_HUD_MESSAGING_DATUMS || !text)
		return FALSE;
	datum = &hud_messaging_globals->message_data[local_player_index];
	if (merge_count > 0)
	{
		/* (the same call again: the newest feed line says it once more, its time starting over) */
		for (index = 0; index < NUMBER_OF_HUD_MESSAGES_PER_DATUM; index++)
		{
			struct hud_message_definition *line = &datum->messages[index];

			if (line->valid && line->feed && (!message || line->time > message->time))
				message = line;
		}
	}
	if (!message)
	{
		ae_hud_message_new_is_feed = TRUE;
		message = find_free_hud_message(datum, NONE, 0);
		ae_hud_message_new_is_feed = FALSE;
	}
	if (!message)
		return FALSE;
	ustrncpy_terminated(message->text, text, NUMBEROF(message->text));
	/* (a HUD that draws no colour runs shows the plain words) */
	if (!ae_hud_style_is_ae())
		ae_feed_strip_runs(message->text);
	message->item_definition_index = NONE;
	message->time = game_time_get();
	message->valid = TRUE;
	message->feed = TRUE;
	message->magic_number = hud_messaging_globals->magic_number++;
	datum->leave_first_line_blank = FALSE;

	return TRUE;
}
