/*
SERVICE_RANK.C

Compartmentalised MCC Service Rank / Career Leveling System (11 Tours x 42 Ranks = 462 Levels).
Kept in its own standalone file so it can be toggled or removed independently.
*/

#include "cseries/cseries.h"
#include "game/game.h"
#include "game/players.h"
#include "interface/hud_messaging.h"

#include "service_rank.h"

#include <stdio.h>
#include <string.h>

#if OPENCE_ENABLE_SERVICE_RANK

static char const *const s_mcc_tour_names[FIREFIGHT_SERVICE_TOURS] =
{
	"Tour 1 (Tier 11)",
	"Tour 2 (Tier 10)",
	"Tour 3 (Tier 9)",
	"Tour 4 (Tier 8)",
	"Tour 5 (Tier 7)",
	"Tour 6 (Tier 6)",
	"Tour 7 (Tier 5)",
	"Tour 8 (Tier 4)",
	"Tour 9 (Tier 3)",
	"Tour 10 (Tier 2)",
	"Tour 11 (Tier 1)"
};

static char const *const s_mcc_rank_names[FIREFIGHT_SERVICE_RANKS] =
{
	"Recruit",
	"Apprentice", "Apprentice (Grade 2)",
	"Private", "Private (Grade 2)",
	"Corporal", "Corporal (Grade 2)",
	"Sergeant", "Sergeant (Grade 2)", "Sergeant (Grade 3)",
	"Gunnery Sergeant", "Gunnery Sergeant (Grade 2)", "Gunnery Sergeant (Grade 3)", "Master Gunnery Sergeant",
	"Lieutenant", "Lieutenant (Grade 2)", "Lieutenant (Grade 3)", "First Lieutenant",
	"Captain", "Captain (Grade 2)", "Captain (Grade 3)", "Staff Captain",
	"Major", "Major (Grade 2)", "Major (Grade 3)", "Field Major",
	"Commander", "Commander (Grade 2)", "Commander (Grade 3)", "Strike Commander",
	"Colonel", "Colonel (Grade 2)", "Colonel (Grade 3)", "Force Colonel",
	"Brigadier", "Brigadier (Grade 2)", "Brigadier (Grade 3)", "Brigadier General",
	"General", "General (Grade 2)", "General (Grade 3)", "5 Star General"
};

/* each rank's XP. Career XP is what is saved and a rank is worked out from it,
so a change here moves players' ranks: a tour's total is kept when it changes,
for players to stay in their tours */
static const long s_mcc_tour_rank_xp[FIREFIGHT_SERVICE_TOURS][FIREFIGHT_SERVICE_RANKS] =
{
	/* Tour 1 (Ranks 1-42): 2,100 - 114,300 | Total: 2,039,000 */
	{ 2100L, 2600L, 3200L, 4200L, 5500L, 6500L, 7900L, 10000L, 11300L, 12700L, 15000L, 17900L, 19700L, 21400L, 25000L, 27100L, 29500L, 32800L, 36300L, 38900L, 41400L, 45700L, 48700L, 51700L, 55800L, 59700L, 62700L, 65700L, 71400L, 74000L, 76500L, 79200L, 82800L, 85200L, 87900L, 92100L, 95200L, 98300L, 102800L, 107500L, 110800L, 114300L },
	/* Tour 2 (Ranks 43-84): 14,300 - 157,100 | Total: 3,070,000 */
	{ 14300L, 14700L, 15300L, 16400L, 18200L, 19400L, 20700L, 23600L, 25300L, 27100L, 30000L, 32900L, 35200L, 37900L, 42100L, 45200L, 48100L, 52200L, 56100L, 59400L, 62900L, 68600L, 72000L, 75500L, 80800L, 85800L, 90000L, 94300L, 100700L, 105000L, 109200L, 114200L, 118900L, 122300L, 125700L, 131400L, 135300L, 139200L, 144200L, 149500L, 153300L, 157100L },
	/* Tour 3 (Ranks 85-126): 28,600 - 285,700 | Total: 5,362,000 */
	{ 28600L, 29000L, 29500L, 30800L, 32400L, 34000L, 35700L, 39300L, 42300L, 45300L, 49200L, 53900L, 57600L, 61400L, 68600L, 73300L, 78000L, 84200L, 91800L, 97200L, 102900L, 112900L, 119200L, 125900L, 135000L, 144700L, 152000L, 159300L, 172100L, 180300L, 188600L, 200000L, 211400L, 220300L, 229300L, 242100L, 248600L, 255000L, 263600L, 272100L, 278900L, 285700L },
	/* Tour 4 (Ranks 127-168): 47,100 - 500,000 | Total: 9,109,000 */
	{ 47100L, 47600L, 48300L, 50000L, 52300L, 54600L, 57100L, 62900L, 67100L, 71600L, 77800L, 85300L, 91400L, 97900L, 109300L, 117000L, 125000L, 136400L, 148400L, 158000L, 167900L, 185700L, 197300L, 209000L, 225000L, 242100L, 255600L, 269300L, 292900L, 307800L, 323200L, 344200L, 366600L, 383600L, 400700L, 424300L, 435400L, 446600L, 461400L, 476900L, 488400L, 500000L },
	/* Tour 5 (Ranks 169-210): 78,600 - 928,600 | Total: 16,584,000 */
	{ 78600L, 79000L, 79800L, 82200L, 86100L, 90000L, 94300L, 104300L, 111100L, 118500L, 129200L, 142500L, 153100L, 164300L, 185700L, 199900L, 214500L, 235800L, 258500L, 277200L, 296400L, 330700L, 353000L, 375900L, 407800L, 442200L, 469000L, 496400L, 545000L, 576300L, 608000L, 651400L, 697700L, 723900L, 745700L, 781400L, 802900L, 824400L, 853600L, 883300L, 905800L, 928600L },
	/* Tour 6 (Ranks 211-252): 128,600 - 1,500,000 | Total: 26,813,000 */
	{ 128600L, 129400L, 130900L, 134200L, 140600L, 147000L, 154300L, 170000L, 181100L, 193200L, 211400L, 232600L, 250100L, 268600L, 302900L, 325500L, 349300L, 383600L, 420700L, 450600L, 481400L, 536400L, 572500L, 609100L, 660000L, 714900L, 758000L, 802100L, 880000L, 929300L, 979600L, 1049200L, 1123000L, 1165200L, 1200700L, 1258600L, 1293700L, 1329100L, 1377200L, 1425700L, 1462700L, 1500000L },
	/* Tour 7 (Ranks 253-294): 207,100 - 2,428,600 | Total: 42,877,000 */
	{ 207100L, 208500L, 210500L, 215800L, 225400L, 235000L, 245700L, 269300L, 287300L, 306300L, 334200L, 367500L, 394800L, 423600L, 477900L, 513800L, 551300L, 605000L, 664400L, 711600L, 760000L, 847900L, 904800L, 963300L, 1045000L, 1133000L, 1201900L, 1272100L, 1397100L, 1476900L, 1558100L, 1670800L, 1789500L, 1865300L, 1934300L, 2031400L, 2089700L, 2148500L, 2227800L, 2307900L, 2368100L, 2428600L },
	/* Tour 8 (Ranks 295-336): 328,600 - 3,928,600 | Total: 68,781,000 */
	{ 328600L, 330300L, 333500L, 342200L, 357000L, 372100L, 389300L, 427100L, 455500L, 485400L, 530000L, 582600L, 626300L, 672100L, 757900L, 815700L, 875800L, 962200L, 1056400L, 1132000L, 1210000L, 1350700L, 1442400L, 1536500L, 1667800L, 1809600L, 1920700L, 2034300L, 2235700L, 2364300L, 2495600L, 2677800L, 2870500L, 2999400L, 3120700L, 3278600L, 3374500L, 3470900L, 3600000L, 3730900L, 3829500L, 3928600L },
	/* Tour 9 (Ranks 337-378): 528,600 - 6,285,700 | Total: 110,423,000 */
	{ 528600L, 531500L, 536900L, 550000L, 574600L, 599000L, 626400L, 687100L, 732600L, 780900L, 852800L, 938000L, 1008200L, 1081400L, 1220000L, 1313000L, 1409600L, 1547800L, 1699900L, 1821300L, 1946400L, 2172900L, 2320200L, 2471500L, 2682800L, 2910300L, 3088600L, 3270700L, 3595000L, 3801600L, 4012400L, 4305000L, 4614100L, 4816000L, 5002900L, 5254300L, 5406800L, 5559900L, 5765000L, 5972400L, 6128900L, 6285700L },
	/* Tour 10 (Ranks 379-420): 842,900 - 10,000,000 | Total: 176,465,000 */
	{ 842900L, 847500L, 856200L, 877800L, 917900L, 957600L, 1002100L, 1100700L, 1174000L, 1252300L, 1370000L, 1507100L, 1620300L, 1738600L, 1962100L, 2112200L, 2267600L, 2489200L, 2733300L, 2928000L, 3128600L, 3491400L, 3726700L, 3968200L, 4306400L, 4668700L, 4953000L, 5243600L, 5759300L, 6088000L, 6423100L, 6887200L, 7378000L, 7684700L, 7960700L, 8361400L, 8604000L, 8847300L, 9173600L, 9502700L, 9751000L, 10000000L },
	/* Tour 11 (Ranks 421-462): 2,142,900 - 71,428,600 | Total: 821,100,000 */
	{ 2142900L, 2152200L, 2172500L, 2227800L, 2341000L, 2459300L, 2594300L, 2912900L, 3166100L, 3441700L, 3868600L, 4392600L, 4837000L, 5307100L, 6227900L, 6867700L, 7538100L, 8513600L, 9617600L, 10512700L, 11441400L, 13164300L, 14308100L, 15490000L, 17167200L, 19002000L, 20459800L, 21958600L, 24667900L, 26424500L, 28226500L, 30749200L, 33457900L, 35585000L, 37760000L, 41632100L, 44107200L, 46634300L, 50142800L, 58828000L, 65171000L, 71428600L }
};

static void print_rank_notice(char const *text)
{
	wchar_t msg[96];
	short c, lp;

	if (!text || !game_in_progress())
		return;
	for (c = 0; text[c] && c < (short)NUMBEROF(msg) - 1; c++)
		msg[c] = (wchar_t)(unsigned char)text[c];
	msg[c] = 0;
	for (lp = 0; lp < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS; lp++)
	{
		if (local_player_get_player_index(lp) != NONE)
			hud_print_message(lp, msg);
	}
}

void service_rank_compute_from_xp(
	long total_xp,
	struct firefight_mcc_rank_info *out_rank)
{
	long rem = MAX(0L, total_xp);
	short tour = 0;
	short r = 0;
	long step_xp = 3000L;

	if (!out_rank)
		return;

	for (tour = 0; tour < FIREFIGHT_SERVICE_TOURS; tour++)
	{
		for (r = 0; r < FIREFIGHT_SERVICE_RANKS; r++)
		{
			step_xp = s_mcc_tour_rank_xp[tour][r];
			if (tour == FIREFIGHT_SERVICE_TOURS - 1 && r == FIREFIGHT_SERVICE_RANKS - 1)
			{
				out_rank->tour_number = FIREFIGHT_SERVICE_TOURS;
				out_rank->tour_name = s_mcc_tour_names[tour];
				out_rank->rank_in_tour = FIREFIGHT_SERVICE_RANKS;
				out_rank->rank_name = s_mcc_rank_names[r];
				out_rank->total_xp = MAX(0L, total_xp);
				out_rank->xp_in_rank = MIN(rem, step_xp);
				out_rank->xp_for_next_rank = step_xp;
				return;
			}
			if (rem < step_xp)
			{
				out_rank->tour_number = (short)(tour + 1);
				out_rank->tour_name = s_mcc_tour_names[tour];
				out_rank->rank_in_tour = (short)(r + 1);
				out_rank->rank_name = s_mcc_rank_names[r];
				out_rank->total_xp = MAX(0L, total_xp);
				out_rank->xp_in_rank = rem;
				out_rank->xp_for_next_rank = step_xp;
				return;
			}
			rem -= step_xp;
		}
	}
}

long service_rank_apply_xp_gain(
	long current_xp,
	long xp_amount,
	char const *reason,
	boolean announce_hud)
{
	struct firefight_mcc_rank_info before_rk, after_rk;
	long new_xp;

	(void)reason;
	if (xp_amount <= 0)
		return current_xp;

	service_rank_compute_from_xp(current_xp, &before_rk);
	new_xp = current_xp + xp_amount;
	service_rank_compute_from_xp(new_xp, &after_rk);

	if (announce_hud)
	{
		if (after_rk.tour_number > before_rk.tour_number)
		{
			char msg[96];
			snprintf(
				msg,
				sizeof(msg),
				"TOUR PROMOTION! %s - %s!",
				after_rk.tour_name,
				after_rk.rank_name);
			print_rank_notice(msg);
		}
		else if (after_rk.rank_in_tour > before_rk.rank_in_tour)
		{
			char msg[96];
			snprintf(
				msg,
				sizeof(msg),
				"RANK UP: %s (Rank %d - %s)",
				after_rk.rank_name,
				(int)after_rk.rank_in_tour,
				after_rk.tour_name);
			print_rank_notice(msg);
		}
	}
	return new_xp;
}

void service_rank_on_daily_challenge_completed(
	long score)
{
	long bonus_xp = 5000L + MAX(0L, score / 10L);
	char msg[96];

	snprintf(msg, sizeof(msg), "DAILY CHALLENGE COMPLETE (+%ld XP)", bonus_xp);
	print_rank_notice(msg);
	firefight_stats_add_xp(bonus_xp, "Daily Challenge Complete");
}

#endif /* OPENCE_ENABLE_SERVICE_RANK */
