/*
AE_SCREEN_VOTE.C

A dedicated server's map vote in AE's menus (0.7.7; ae_screen_vote.h). The server's vote (network_map_vote.c: its
choices and their votes, its seconds, the machine's own choice, the winner) as a popover over whatever AE screen the
player is on in the server's lobby (the lobby, the post-game report): an ice top edge, the title and the seconds left on
its right, a row a choice (the map, its gametype, the votes; YOUR VOTE on the player's), the help line and the prompts.
A votes for the focused choice (again for another, while the vote runs); B hides it until the next vote. Once the vote
is over the winner shows a moment (NEXT GAME, its row lit, the others dimmed), and it closes. The look is the attention
dialog's (ae_screen_attention.c): 880 u wide, 40 u in, the title 40 u.
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "game/game.h"
#include "../src/ae_draw.h"
#include "../src/ae_layout.h"
#include "ae_hooks.h"
#include "ae_motion.h"
#include "ae_screen_vote.h"
#include "ae_server_ingame.h"
#include "ae_server_lobby.h"
#include "ae_sound.h"
#include "ae_strings.h"
#include "ae_ui.h"
#include "ae_widgets.h"
#include "network_map_vote.h"

void platform_log(char const *format, ...);
unsigned long system_milliseconds(void);

enum { VOTE_HIT = 0x7E40, HIT_ROWS = VOTE_HIT + 1, HIT_PROMPTS, HIT_OUTSIDE };

#define BOX_WIDTH_U 880.0f
#define PAD_U 40.0f
#define STRIPE_U 4.0f
#define TITLE_U 40.0f
#define BODY_U 19.0f
#define COUNT_U 22.0f
/* the winner shown this long before the popover closes */
#define WINNER_MS 3000

static struct
{
	int open;
	short opened_round;      /* the vote it last opened for (or was hidden in): not again for it */
	short winner_round;
	unsigned long winner_at;
	short focus;
	struct network_map_vote_status status;
	struct ae_density density;
	char captions[NETWORK_MAP_VOTE_MAXIMUM_CHOICES][48];
	char gametypes[NETWORK_MAP_VOTE_MAXIMUM_CHOICES][32];
} vote = { 0, -1, -1 };

static struct ae_screen_class const vote_class;

static float un(float spec_units)
{
	return spec_units * vote.density.unit;
}

static float cap_top(int font, float size, float center)
{
	return center - ae_draw_cap_height(font, size) * 0.5f;
}

static void close_it(char const *why)
{
	if (!vote.open)
		return;
	vote.open = 0;
	if (ae_ui_holds(&vote))
		ae_ui_remove(&vote);
	platform_log("ae vote: closed (%s)", why);
}

static void choose(short choice)
{
	if (vote.status.winner != NONE || choice < 0 || choice >= vote.status.count)
		return;
	if (!network_map_vote_choose(choice))
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	ae_sound_request(AE_SOUND_FORWARD, 0);
	platform_log("ae vote: voted for %s:%s", vote.status.map_names[choice], vote.status.gametypes[choice]);
}

static int vote_handle(struct ae_screen *screen, struct ae_event const *event)
{
	(void)screen;
	switch (event->action)
	{
	case AE_ACTION_ACCEPT:
		choose(vote.focus);
		return 1;
	case AE_ACTION_BACK:
		ae_sound_request(AE_SOUND_BACK, 0);
		close_it("hidden");
		return 1;
	case AE_ACTION_UP:
	case AE_ACTION_DOWN:
	{
		short focus = ae_choice_step(vote.status.count, vote.focus, event->action == AE_ACTION_UP ? -1 : 1);

		if (focus != vote.focus && vote.status.winner == NONE)
		{
			vote.focus = focus;
			ae_sound_request(AE_SOUND_CURSOR, event->repeat);
		}
		return 1;
	}
	default:
		return 1;
	}
}

static void prompts_fill(struct ae_prompt *prompts)
{
	prompts[0] = (struct ae_prompt){ AE_BUTTON_A, "Vote", NULL, AE_ACTION_ACCEPT, 0 };
	prompts[1] = (struct ae_prompt){ AE_BUTTON_B, "Hide", NULL, AE_ACTION_BACK, 0 };
}

static void vote_pointer(struct ae_screen *screen, struct ae_pointer const *pointer)
{
	struct ae_event event;
	struct ae_hit hit;
	struct ae_prompt prompts[2];

	prompts_fill(prompts);
	if (ae_prompts_pointer(prompts, 2, pointer, HIT_PROMPTS, &event))
	{
		vote_handle(screen, &event);
		return;
	}
	if (!ae_hit_at(pointer->x, pointer->y, 0, &hit) || hit.id != HIT_ROWS || hit.index < 0 ||
		hit.index >= vote.status.count || vote.status.winner != NONE)
	{
		return;
	}
	if (pointer->moved)
		vote.focus = hit.index;
	if (pointer->left_clicks)
	{
		vote.focus = hit.index;
		choose(hit.index);
	}
}

static void vote_draw(struct ae_screen *screen)
{
	struct ae_layout layout;
	struct network_map_vote_status const *status = &vote.status;
	struct ae_prompt prompts[2];
	float view_width = ae_draw_view_width(), width, pad, row, gap, x0, y0, ix, iw, y, height, center;
	char text[64], values[NETWORK_MAP_VOTE_MAXIMUM_CHOICES][64];
	short index;
	int over = status->winner != NONE;

	(void)screen;
	ae_draw_current_layout(&layout);
	ae_density_full(layout.height * layout.scale, ae_settings_ui_scale(), &vote.density);
	width = un(BOX_WIDTH_U);
	if (width > view_width - un(32.0f))
		width = view_width - un(32.0f);
	pad = un(PAD_U);
	row = ae_size_row(&vote.density);
	gap = vote.density.metrics->gap * vote.density.unit;
	iw = width - 2.0f * pad;
	height = un(STRIPE_U) + pad + un(TITLE_U) * 1.4f + un(14.0f) + un(BODY_U) * 1.5f + un(20.0f) +
		status->count * (row + gap) + un(10.0f) + un(BODY_U) * 1.5f + pad + un(40.0f);
	x0 = (view_width - width) * 0.5f;
	y0 = ((float)AE_LAYOUT_HEIGHT - height) * 0.5f;
	ix = x0 + pad;
	ae_draw_rect(-(float)AE_LAYOUT_HEIGHT, -(float)AE_LAYOUT_HEIGHT, view_width + 2.0f * AE_LAYOUT_HEIGHT,
		3.0f * AE_LAYOUT_HEIGHT, 0.0f, AE_COLOR_SCRIM);
	ae_hit_add(0.0f, 0.0f, view_width, (float)AE_LAYOUT_HEIGHT, HIT_OUTSIDE, AE_PART_OUTSIDE, -1);
	ae_draw_rect(x0, y0, width, height, un(2.0f), AE_COLOR_POPOVER);
	ae_hit_add(x0, y0, width, height, HIT_OUTSIDE, AE_PART_CARD, -1);
	ae_draw_rect(x0, y0, width, un(STRIPE_U), 0.0f, AE_COLOR_ACCENT);
	/* the title, the seconds left on its right (a vote over: NEXT GAME) */
	center = y0 + un(STRIPE_U) + pad + un(TITLE_U) * 0.7f;
	ae_draw_text(AE_FONT_TITLE, un(TITLE_U), ix, cap_top(AE_FONT_TITLE, un(TITLE_U), center), AE_ALIGN_LEFT,
		AE_COLOR_TITLE, over ? "NEXT GAME" : "VOTE FOR THE NEXT GAME");
	if (!over)
	{
		snprintf(text, sizeof(text), "%d s", status->seconds_left > 0 ? status->seconds_left : 0);
		ae_draw_text(AE_FONT_ROW, un(COUNT_U), x0 + width - pad, cap_top(AE_FONT_ROW, un(COUNT_U), center),
			AE_ALIGN_RIGHT, status->seconds_left <= 5 ? AE_COLOR_WARNING : AE_COLOR_ACCENT, text);
	}
	y = center + un(TITLE_U) * 0.7f + un(14.0f);
	ae_draw_text(AE_FONT_BODY, un(BODY_U), ix, cap_top(AE_FONT_BODY, un(BODY_U), y + un(BODY_U) * 0.75f),
		AE_ALIGN_LEFT, AE_COLOR_MUTED, over ? "The vote is over. The next game starts in a moment." :
		"Everyone in the lobby votes. The most votes wins; a tie is drawn at random.");
	y += un(BODY_U) * 1.5f + un(20.0f);
	/* a row a choice: the map, the gametype and its votes; the player's vote badged */
	for (index = 0; index < status->count; index++)
	{
		struct ae_row widget;

		memset(&widget, 0, sizeof(widget));
		widget.label = vote.captions[index];
		snprintf(values[index], sizeof(values[index]), "%s  \xE2\x80\xA2  %d VOTE%s", vote.gametypes[index],
			status->votes[index], status->votes[index] == 1 ? "" : "S");
		widget.value = values[index];
		widget.flags = AE_ROW_STATUS;
		if (over)
		{
			if (index == status->winner)
				widget.flags |= AE_ROW_FOCUSED;
			else
				widget.flags |= AE_ROW_DISABLED;
		}
		else if (index == vote.focus)
			widget.flags |= AE_ROW_FOCUSED;
		if (index == status->chosen)
			widget.badges[0] = "YOUR VOTE";
		y += ae_widget_row(&vote.density, ix, y, iw, &widget, HIT_ROWS, index) + gap;
	}
	y += un(10.0f);
	if (!over)
		ae_draw_text(AE_FONT_BODY, un(BODY_U), ix, cap_top(AE_FONT_BODY, un(BODY_U), y + un(BODY_U) * 0.75f),
			AE_ALIGN_LEFT, AE_COLOR_MUTED, status->chosen == NONE ? "A votes for a game. You can change your vote "
			"until the time is up." : "You can change your vote until the time is up.");
	prompts_fill(prompts);
	ae_widget_prompts(&vote.density, ix, y0 + height - pad * 0.5f - un(10.0f), prompts, over ? 0 : 2, NULL, ix + iw, -1,
		HIT_PROMPTS);
}

static struct ae_screen_class const vote_class =
{
	.name = "vote", .handle = vote_handle, .draw = vote_draw, .pointer = vote_pointer, .popover = 1,
};

/* the words of a choice: its map's caption (the scoreboard's: a pack's or Workshop map by its name), its gametype's */
static void captions_read(void)
{
	short index;

	for (index = 0; index < vote.status.count && index < NETWORK_MAP_VOTE_MAXIMUM_CHOICES; index++)
	{
		ae_server_ingame_map_caption(vote.status.map_names[index], vote.captions[index], (int)sizeof(vote.captions[index]));
		ae_ingame_gametype_word(vote.status.gametypes[index], vote.gametypes[index], (int)sizeof(vote.gametypes[index]));
	}
}

void ae_screen_vote_tick(unsigned long now)
{
	struct network_map_vote_status status;
	int heard;

	if (game_connection() != _game_connection_network_client)
	{
		close_it("not a client");
		return;
	}
	/* (0.7.7: a dedicated server's lobby page holds the vote itself: no popover there) */
	if (ae_server_lobby_dedicated())
	{
		close_it("the server lobby page has it");
		return;
	}
	heard = network_map_vote_status_get(&status);
	if (!heard)
	{
		close_it("no vote");
		return;
	}
	/* (a vote begun since: open over whatever is up, unless the player hid this one) */
	if (!vote.open && status.winner == NONE && status.round != vote.opened_round)
	{
		vote.opened_round = status.round;
		vote.status = status;
		vote.focus = status.chosen != NONE ? status.chosen : 0;
		captions_read();
		if (ae_ui_push(&vote_class, AE_OWNER_ANY, &vote))
		{
			vote.open = 1;
			ae_sound_request(AE_SOUND_FORWARD, 0);
			platform_log("ae vote: opened, %d choices for %d s", status.count, status.seconds_left);
		}
		return;
	}
	if (!vote.open)
		return;
	if (status.round != vote.status.round || status.count != vote.status.count)
	{
		vote.status = status;
		captions_read();
	}
	vote.status = status;
	if (vote.focus >= status.count)
		vote.focus = 0;
	if (status.winner != NONE)
	{
		if (vote.winner_round != status.round)
		{
			vote.winner_round = status.round;
			vote.winner_at = now;
			platform_log("ae vote: over, the next game is %s:%s (%d votes)", status.map_names[status.winner],
				status.gametypes[status.winner], status.votes[status.winner]);
		}
		else if (now - vote.winner_at >= WINNER_MS)
			close_it("the winner shown");
	}
}

int ae_screen_vote_open(void)
{
	return vote.open;
}
