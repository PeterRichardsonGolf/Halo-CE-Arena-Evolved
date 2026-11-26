#include <stdio.h>
#include <string.h>
#include "ae_campaign_workshop_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static char logged[16][512];
static int log_count;

static void log_line(void *context, const char *package, const char *line)
{
	(void)context;
	if (log_count < 16)
		snprintf(logged[log_count], sizeof(logged[0]), "%s|%s", package, line);
	log_count++;
}

static void mission(struct ae_ws_package *package, const char *name, const char *title, int map)
{
	struct ae_ws_package_mission *m = &package->missions[package->count++];
	char folder[24];

	snprintf(m->name, sizeof(m->name), "%s", name);
	snprintf(m->title, sizeof(m->title), "%s", title);
	snprintf(folder, sizeof(folder), "%.20s", package->package);
	snprintf(m->level, sizeof(m->level), "mcc_maps\\%.20s\\%.30s", folder, name);
	m->map = map;
}

static void package_make(struct ae_ws_package *package, const char *folder, int manifest, const char *title)
{
	memset(package, 0, sizeof(*package));
	snprintf(package->package, sizeof(package->package), "%s", folder);
	snprintf(package->title, sizeof(package->title), "%s", title);
	package->manifest = manifest;
}

static struct ae_ws_package packages[24];
static struct ae_ws_campaign campaigns[AE_WS_CAMPAIGNS];

int main(void)
{
	char shown[AE_WS_TEXT];
	int made, index;

	/* no packages: no campaign, nothing logged */
	log_count = 0;
	CHECK(ae_ws_campaigns_make(packages, 0, campaigns, AE_WS_CAMPAIGNS, log_line, NULL) == 0);
	CHECK(log_count == 0);
	CHECK(ae_ws_campaigns_make(NULL, 3, campaigns, AE_WS_CAMPAIGNS, log_line, NULL) == 0);

	/* one package, one mission: one campaign of one mission, its texts in capitals, the level kept as it is */
	package_make(&packages[0], "100", AE_WS_MANIFEST_OK, "Sample Campaign");
	mission(&packages[0], "a10_sample", "Sample Campaign", 1);
	log_count = 0;
	made = ae_ws_campaigns_make(packages, 1, campaigns, AE_WS_CAMPAIGNS, log_line, NULL);
	CHECK(made == 1);
	CHECK(log_count == 0);
	CHECK(!strcmp(campaigns[0].title, "SAMPLE CAMPAIGN"));
	CHECK(!strcmp(campaigns[0].package, "100"));
	CHECK(campaigns[0].count == 1);
	CHECK(!strcmp(campaigns[0].missions[0].level, "mcc_maps\\100\\a10_sample"));

	/* several missions, in the manifest's order; a mission with no title shows its map's name */
	package_make(&packages[1], "my_campaign", AE_WS_MANIFEST_OK, "");
	mission(&packages[1], "c20_first", "The First", 1);
	mission(&packages[1], "c10_second", "", 1);
	mission(&packages[1], "b40_third", "Third\xC3\xA9  one", 1);
	made = ae_ws_campaigns_make(packages, 2, campaigns, AE_WS_CAMPAIGNS, log_line, NULL);
	CHECK(made == 2);
	CHECK(campaigns[1].count == 3);
	CHECK(!strcmp(campaigns[1].missions[0].title, "THE FIRST"));
	CHECK(!strcmp(campaigns[1].missions[1].title, "C10 SECOND"));
	CHECK(!strcmp(campaigns[1].missions[2].title, "THIRD? ONE"));
	CHECK(!strcmp(campaigns[1].missions[2].level, "mcc_maps\\my_campaign\\b40_third"));
	/* (no manifest title: the first mission's) */
	CHECK(!strcmp(campaigns[1].title, "THE FIRST"));

	/* a damaged manifest, no manifest, no missions, a missing map, only multiplayer maps: each left out with one line;
	a multiplayer mission left out of its campaign with one line; the others listed */
	package_make(&packages[2], "200", AE_WS_MANIFEST_DAMAGED, "Broken");
	snprintf(packages[2].reason, sizeof(packages[2].reason), "CampaignMaps is not an array");
	package_make(&packages[3], "300", AE_WS_MANIFEST_NONE, "");
	mission(&packages[3], "m10", "", 1);
	package_make(&packages[4], "400", AE_WS_MANIFEST_OK, "Empty");
	package_make(&packages[5], "500", AE_WS_MANIFEST_OK, "Half There");
	mission(&packages[5], "m10", "One", 1);
	mission(&packages[5], "m20", "Two", 0);
	package_make(&packages[6], "600", AE_WS_MANIFEST_OK, "Last");
	mission(&packages[6], "z05", "", AE_WS_MAP_MULTIPLAYER);
	mission(&packages[6], "z10", "", 1);
	package_make(&packages[7], "700", AE_WS_MANIFEST_OK, "");
	mission(&packages[7], "y10", "", AE_WS_MAP_MULTIPLAYER);
	log_count = 0;
	made = ae_ws_campaigns_make(packages, 8, campaigns, AE_WS_CAMPAIGNS, log_line, NULL);
	CHECK(made == 3);
	CHECK(log_count == 6);
	CHECK(!strcmp(logged[0], "200|BROKEN is not listed: its CampaignInfo.json could not be read (CampaignMaps is not an array)."));
	CHECK(!strcmp(logged[1], "300|300 is not listed: it has no CampaignInfo.json."));
	CHECK(!strcmp(logged[2], "400|EMPTY is not listed: its CampaignInfo.json names no mission."));
	CHECK(!strcmp(logged[3], "500|HALF THERE is not listed: mission 2's map (m20) is not installed."));
	CHECK(!strcmp(logged[4], "600|LAST: mission 1 (z05) is a multiplayer map, left out."));
	CHECK(!strcmp(logged[5], "700|700 is not listed: it has no campaign map."));
	CHECK(!strcmp(campaigns[2].title, "LAST"));
	CHECK(campaigns[2].count == 1);
	CHECK(!strcmp(campaigns[2].missions[0].title, "Z10"));
	CHECK(!strcmp(campaigns[2].missions[0].level, "mcc_maps\\600\\z10"));
	/* (no fault function is fine) */
	CHECK(ae_ws_campaigns_make(packages, 8, campaigns, AE_WS_CAMPAIGNS, NULL, NULL) == 3);

	/* a package of more missions than CAMPAIGN shows: the first ones, one line */
	package_make(&packages[0], "big", AE_WS_MANIFEST_OK, "Big");
	for (index = 0; index < AE_WS_PACKAGE_MISSIONS; index++)
	{
		char name[16];

		snprintf(name, sizeof(name), "m%02d", index);
		mission(&packages[0], name, "", 1);
	}
	log_count = 0;
	made = ae_ws_campaigns_make(packages, 1, campaigns, AE_WS_CAMPAIGNS, log_line, NULL);
	CHECK(made == 1 && campaigns[0].count == AE_WS_MISSIONS && log_count == 1);
	CHECK(!strcmp(campaigns[0].missions[AE_WS_MISSIONS - 1].title, "M15"));
	/* (a count past the array is held to it) */
	packages[0].count = 1000;
	CHECK(ae_ws_campaigns_make(packages, 1, campaigns, AE_WS_CAMPAIGNS, NULL, NULL) == 1);
	CHECK(campaigns[0].count == AE_WS_MISSIONS);

	/* more campaigns than the list holds: the first ones, a line for each left */
	for (index = 0; index < 20; index++)
	{
		char folder[16];

		snprintf(folder, sizeof(folder), "p%d", index);
		package_make(&packages[index], folder, AE_WS_MANIFEST_OK, folder);
		mission(&packages[index], "m10", "", 1);
	}
	log_count = 0;
	made = ae_ws_campaigns_make(packages, 20, campaigns, AE_WS_CAMPAIGNS, log_line, NULL);
	CHECK(made == AE_WS_CAMPAIGNS && log_count == 20 - AE_WS_CAMPAIGNS);
	CHECK(ae_ws_campaigns_make(packages, 20, campaigns, 2, NULL, NULL) == 2);

	/* the shown text */
	ae_ws_shown_text("  a__b\tc\r\n ", shown, sizeof(shown));
	CHECK(!strcmp(shown, "A B C"));
	ae_ws_shown_text(NULL, shown, sizeof(shown));
	CHECK(!strcmp(shown, ""));
	ae_ws_shown_text("abcdef", shown, 4);
	CHECK(!strcmp(shown, "ABC"));
	ae_ws_shown_text("\x01x\x7F", shown, sizeof(shown));
	CHECK(!strcmp(shown, "?X?"));

	if (failures)
		printf("%d failures\n", failures);
	else
		printf("ae_campaign_workshop_rules_test: ok\n");
	return failures ? 1 : 0;
}
