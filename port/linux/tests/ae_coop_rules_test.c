#include <stdio.h>
#include <string.h>
#include "ae_coop_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	struct ae_coop_seats seats;
	char const *title, *body;

	ae_coop_seats_reset(&seats);
	CHECK(ae_coop_players(&seats) == 1 && ae_coop_second(&seats) == -1 && ae_coop_pad_mask(&seats) == 0);
	CHECK(ae_coop_continue_allowed(&seats));
	CHECK(!strcmp(ae_coop_players_text(1), "1 PLAYER") && !strcmp(ae_coop_players_text(2), "2 PLAYERS") &&
		!strcmp(ae_coop_players_text(3), "3 PLAYERS") && !strcmp(ae_coop_players_text(4), "4 PLAYERS"));
	CHECK(AE_COOP_PLAYERS_MAX == 4 && AE_COOP_ONLINE_PLAYERS_MAX == 16);

	/* START joins from pads 2-4 only; never player 1's controller */
	CHECK(!ae_coop_start_joins(&seats, 0) && ae_coop_start_joins(&seats, 1) && ae_coop_start_joins(&seats, 3));
	CHECK(!ae_coop_start_joins(&seats, 4) && !ae_coop_start_joins(&seats, -1));
	/* cards open count against the four seats: three cards fill the table */
	seats.picking = 2;
	CHECK(ae_coop_start_joins(&seats, 2));
	seats.picking = 3;
	CHECK(!ae_coop_start_joins(&seats, 2));
	seats.picking = 0;

	/* pads sit in any order; the fourth is refused; the seat's B is the pad's */
	CHECK(ae_coop_join(&seats, 2) == AE_COOP_JOIN_OK);
	CHECK(ae_coop_players(&seats) == 2 && ae_coop_second(&seats) == 2 && ae_coop_pad_mask(&seats) == (1u << 2));
	CHECK(ae_coop_join(&seats, 2) == AE_COOP_JOIN_SEATED);
	CHECK(ae_coop_join(&seats, 0) == AE_COOP_JOIN_BAD_CONTROLLER && ae_coop_join(&seats, 4) == AE_COOP_JOIN_BAD_CONTROLLER);
	CHECK(ae_coop_start_joins(&seats, 3) && !ae_coop_start_joins(&seats, 2));
	CHECK(ae_coop_back_is_seat(&seats, 2) && !ae_coop_back_is_seat(&seats, 0) && !ae_coop_back_is_seat(&seats, 3));
	CHECK(!ae_coop_continue_allowed(&seats));
	CHECK(ae_coop_join(&seats, 3) == AE_COOP_JOIN_OK && ae_coop_join(&seats, 1) == AE_COOP_JOIN_OK);
	CHECK(ae_coop_players(&seats) == 4 && ae_coop_pad_mask(&seats) == (2u | 4u | 8u));
	CHECK(!ae_coop_start_joins(&seats, 1) && ae_coop_second(&seats) == 1);
	/* players in the order of their controllers: player 1, then controllers 2, 3, 4 */
	CHECK(ae_coop_controller_at(&seats, 0) == 0 && ae_coop_controller_at(&seats, 1) == 1 &&
		ae_coop_controller_at(&seats, 2) == 2 && ae_coop_controller_at(&seats, 3) == 3 && ae_coop_controller_at(&seats, 4) == -1);
	CHECK(!ae_coop_continue_allowed(&seats));

	/* leaving: another player's only; player 1 never leaves by B here; the ones left close ranks */
	CHECK(!ae_coop_leave(&seats, 0) && ae_coop_leave(&seats, 1) && ae_coop_leave(&seats, 2));
	CHECK(ae_coop_players(&seats) == 2 && ae_coop_controller_at(&seats, 1) == 3 && ae_coop_pad_mask(&seats) == 8u);
	CHECK(ae_coop_leave(&seats, 3) && !ae_coop_leave(&seats, 3) && ae_coop_players(&seats) == 1 && ae_coop_continue_allowed(&seats));

	/* a lone gamepad on the keyboard's controller */
	CHECK(ae_coop_lone_pad_joins(&seats, 0, 1, 1));
	CHECK(!ae_coop_lone_pad_joins(&seats, 0, 0, 1) && !ae_coop_lone_pad_joins(&seats, 0, 1, 0) &&
		!ae_coop_lone_pad_joins(&seats, 1, 1, 1));
	seats.picking = 1;
	CHECK(!ae_coop_lone_pad_joins(&seats, 0, 1, 1));
	seats.picking = 0;
	seats.adding = 1;
	ae_coop_join(&seats, 1);
	CHECK(seats.adding == 0 && !ae_coop_lone_pad_joins(&seats, 0, 1, 1));

	/* the start's check: every seated pad needs a profile (the mask names the ones without) */
	ae_coop_seats_reset(&seats);
	CHECK(ae_coop_start_check(&seats, 1, 0) == AE_COOP_ERR_NONE);
	CHECK(ae_coop_start_check(&seats, 0, 0) == AE_COOP_ERR_NO_PROFILE);
	ae_coop_join(&seats, 3);
	ae_coop_join(&seats, 1);
	CHECK(ae_coop_start_check(&seats, 1, 0) == AE_COOP_ERR_NONE);
	CHECK(ae_coop_start_check(&seats, 1, 1u << 3) == AE_COOP_ERR_SECOND_PROFILE);
	CHECK(ae_coop_start_check(&seats, 1, 1u << 2) == AE_COOP_ERR_NONE);   /* (no pad on controller 3: nothing missing) */
	CHECK(ae_coop_second_controller_ok(1) && ae_coop_second_controller_ok(3));
	CHECK(!ae_coop_second_controller_ok(0) && !ae_coop_second_controller_ok(4) && !ae_coop_second_controller_ok(-1));

	/* the error words: every error says something; none says a pad's "Press B" for another's mistake without naming it */
	ae_coop_error_text(AE_COOP_ERR_SECOND_CONTROLLER, &title, &body);
	CHECK(title[0] && strstr(body, "own controller"));
	ae_coop_error_text(AE_COOP_ERR_NO_PROFILE, &title, &body);
	CHECK(title[0] && strstr(body, "player 1"));
	ae_coop_error_text(AE_COOP_ERR_SECOND_PROFILE, &title, &body);
	CHECK(title[0] && body[0]);
	ae_coop_error_text(AE_COOP_ERR_NONE, &title, &body);
	CHECK(!title[0] && !body[0]);

	/* online co-op: the network, the limit, the browser row's words */
	CHECK(ae_coop_host_network(1) == AE_COOP_NET_ONLINE && ae_coop_host_network(0) == AE_COOP_NET_LAN);
	CHECK(ae_coop_host_maximum(0) == 16 && ae_coop_host_maximum(16) == 16 && ae_coop_host_maximum(32) == 16 &&
		ae_coop_host_maximum(8) == 8);
	{
		char text[48];

		ae_coop_listing_type(-1, text, sizeof(text));
		CHECK(!strcmp(text, "CO-OP"));
		ae_coop_listing_type(2, text, sizeof(text));
		CHECK(strstr(text, "CO-OP") == text && strstr(text, "HEROIC") && strlen(text) > 10);
		ae_coop_listing_type(4, text, sizeof(text));
		CHECK(!strcmp(text, "CO-OP"));
		ae_coop_listing_type(3, text, sizeof(text));
		CHECK(strstr(text, "LEGENDARY"));
		ae_coop_listing_type(0, text, sizeof(text));
		CHECK(strstr(text, "EASY"));
		ae_coop_listing_type(2, text, 8);   /* (cut, still ended) */
		CHECK(strlen(text) < 8);
	}
	CHECK(ae_coop_level_of_map("a10") == 0 && ae_coop_level_of_map("levels\\a30\\a30") == 1 && ae_coop_level_of_map("D40") == 9 &&
		ae_coop_level_of_map("levels/c20/c20") == 6 && ae_coop_level_of_map("b40") == 4);
	CHECK(ae_coop_level_of_map("pat_a10_remix") == -1 && ae_coop_level_of_map("a10@ce") == -1 &&
		ae_coop_level_of_map("levels\\test\\bloodgulch\\bloodgulch") == -1 && ae_coop_level_of_map("") == -1 &&
		ae_coop_level_of_map(NULL) == -1 && ae_coop_level_of_map("a100") == -1 && ae_coop_level_of_map("a1") == -1 &&
		ae_coop_level_of_map("levels\\a10\\") == -1);
	CHECK(ae_coop_gametype_is_coop("Co-op") && ae_coop_gametype_is_coop("co-op legend") && ae_coop_gametype_is_coop("CO-OP Easy"));
	CHECK(!ae_coop_gametype_is_coop("Co-opera") && !ae_coop_gametype_is_coop("Slayer") && !ae_coop_gametype_is_coop("") &&
		!ae_coop_gametype_is_coop(NULL));
	CHECK(ae_coop_difficulty_from_gametype("Co-op Easy") == 0 && ae_coop_difficulty_from_gametype("Co-op normal") == 1 &&
		ae_coop_difficulty_from_gametype("co-op Heroic") == 2 && ae_coop_difficulty_from_gametype("Co-op Legend") == 3 &&
		ae_coop_difficulty_from_gametype("Co-op Legendary") == 3);
	CHECK(ae_coop_difficulty_from_gametype("Co-op") == -1 && ae_coop_difficulty_from_gametype("Co-op ") == -1 &&
		ae_coop_difficulty_from_gametype("Slayer Easy") == -1 && ae_coop_difficulty_from_gametype(NULL) == -1);
	/* only PUBLIC online co-op is listed; LAN co-op never is; ordinary multiplayer keeps its own listing rule */
	CHECK(ae_coop_listing_kept(1, 1, 0));
	CHECK(!ae_coop_listing_kept(1, 0, 0));
	CHECK(!ae_coop_listing_kept(1, 1, 1) && !ae_coop_listing_kept(1, 0, 1));
	CHECK(ae_coop_listing_kept(0, 0, 0) && ae_coop_listing_kept(0, 0, 1));
	{
		int error;

		for (error = AE_COOP_HOST_ERR_NO_PROFILE; error <= AE_COOP_HOST_ERR_IN_GAME; error++)
		{
			ae_coop_host_error_text(error, &title, &body);
			CHECK(title[0] && body[0]);
		}
		ae_coop_host_error_text(AE_COOP_HOST_ERR_NONE, &title, &body);
		CHECK(!title[0] && !body[0]);
	}
	ae_coop_host_left_text(&title, &body);
	CHECK(!strcmp(title, "The host left the game.") && !strcmp(body, "You will return to the menus."));

	if (failures)
		return 1;
	printf("ae_coop_rules_test: ok\n");
	return 0;
}
