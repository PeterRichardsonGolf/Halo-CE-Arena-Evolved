/*
AE_HIT_FEEDBACK.C

HIT FEEDBACK (ae_hit_feedback.h): the gametype's option and its ARENA OPTIONS
row. The effects themselves are drawn from here as they are built
(notes: the pop, the flicker, the recharge band, the bloodshot).
*/

#include <string.h>

#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "game/game_engine.h"
#include "interface/ui_widget.h"
#include "interface/ui_widget_instance.h"
#include "../src/port_config.h"

#include "ae_hit_feedback.h"

void platform_log(char const *format, ...);

#define SPINNER_HIT_FEEDBACK "hit_feedback_spinner"

boolean ae_hit_feedback_active(void)
{
	struct game_variant const *variant = game_engine_running() ? game_engine_get_variant() : NULL;

	/* (debug.hit_feedback: on in every multiplayer game, for tests) */
	return variant && variant->game_engine_index &&
		(ae_hit_feedback_value((unsigned char)AE_HIT_FEEDBACK_VARIANT_BYTE(variant)) == AE_HIT_FEEDBACK_ARENA ||
			config_boolean("debug.hit_feedback") != 0);
}

void ae_hit_feedback_log_rules(void)
{
	struct game_variant const *variant = game_engine_get_variant();
	unsigned char byte = variant && variant->game_engine_index ? (unsigned char)AE_HIT_FEEDBACK_VARIANT_BYTE(variant) : 0;

	platform_log("hit feedback: %s", ae_hit_feedback_name(ae_hit_feedback_value(byte)));
}

static struct widget_instance *spinner_named(struct widget_instance *list, char const *name)
{
	struct widget_instance *row;

	for (row = list ? list->child : NULL; row; row = row->next)
	{
		struct widget_instance *child;

		for (child = row->child; child; child = child->next)
		{
			if (child->type == 2 /* spinner */ && child->name && !strncmp(child->name, name, 31))
				return child;
		}
	}
	return NULL;
}

void ae_hit_feedback_menu_rows(struct widget_instance *list, struct game_variant *variant, boolean save)
{
	struct widget_instance *spinner = spinner_named(list, SPINNER_HIT_FEEDBACK);

	if (!variant || !spinner)
		return;
	if (save)
	{
		short value = spinner->parameters.list.selected_index;
		unsigned char byte = ae_hit_feedback_byte_set((unsigned char)AE_HIT_FEEDBACK_VARIANT_BYTE(variant),
			value == AE_HIT_FEEDBACK_ARENA ? AE_HIT_FEEDBACK_ARENA : AE_HIT_FEEDBACK_CLASSIC);

		AE_HIT_FEEDBACK_VARIANT_BYTE(variant) = byte;
		platform_log("menus: the gametype's hit feedback %s", ae_hit_feedback_name(ae_hit_feedback_value(byte)));
		return;
	}
	spinner->parameters.list.selected_index =
		(short)ae_hit_feedback_value((unsigned char)AE_HIT_FEEDBACK_VARIANT_BYTE(variant));
}
