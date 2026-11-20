/*
AE_CAMPAIGN_WORKSHOP_RULES.H

Which custom campaigns CAMPAIGN lists (ae_screen_campaign.c): each MCC package of mcc_maps (a Steam Workshop item or a
local folder) whose CampaignInfo.json was read, with its missions in the manifest's order. The glue
(ae_glue_campaign.c) reads the manifests and the map catalog (mcc_maps.c) into ae_ws_package entries; these rules make
the list: a package without a readable manifest, naming no mission, or with a mission whose map is not installed is left
out (the engine never skips a mission to get to a later one); a mission that is a multiplayer map is left out of its
campaign. Each is one fault line, in the words CAMPAIGN shows and logs. The texts are cut to printable ASCII in
capitals, as CAMPAIGN's level names are. Pure (port/linux/tests/ae_campaign_workshop_rules_test.c).
*/

#ifndef __AE_CAMPAIGN_WORKSHOP_RULES_H
#define __AE_CAMPAIGN_WORKSHOP_RULES_H

enum
{
	/* a package's missions as its manifest lists them (mcc_manifest.h's MCC_MANIFEST_MAXIMUM_MISSIONS) */
	AE_WS_PACKAGE_MISSIONS = 64,
	/* the missions CAMPAIGN shows of one campaign: its rows hold 20 (ae_setup_flow.h), three are CAMPAIGN, SPLIT SCREEN
	and the header */
	AE_WS_MISSIONS = 16,
	AE_WS_CAMPAIGNS = 16,
	AE_WS_TEXT = 64,
	/* a manifest as the glue read it (mcc_manifest_for_level's answer) */
	AE_WS_MANIFEST_NONE = 0, AE_WS_MANIFEST_OK = 1, AE_WS_MANIFEST_DAMAGED = -1,
	/* a mission's map in the catalog: not there, a campaign map, a multiplayer map */
	AE_WS_MAP_MISSING = 0, AE_WS_MAP_CAMPAIGN = 1, AE_WS_MAP_MULTIPLAYER = 2,
	AE_WS_FAULT_TEXT = 192
};

struct ae_ws_package_mission
{
	char name[AE_WS_TEXT];      /* the manifest's ScenarioFile */
	char title[AE_WS_TEXT];     /* its Title ("" none) */
	char level[AE_WS_TEXT];     /* the level name the game loads: mcc_maps\<package>\<name> */
	int map;                    /* AE_WS_MAP_*: the map in the catalog */
};

struct ae_ws_package
{
	char package[AE_WS_TEXT];   /* the folder under mcc_maps */
	int manifest;               /* AE_WS_MANIFEST_* */
	char reason[128];           /* a damaged manifest's reason */
	char title[AE_WS_TEXT];     /* the manifest's Title ("" none) */
	int count;
	struct ae_ws_package_mission missions[AE_WS_PACKAGE_MISSIONS];
};

struct ae_ws_mission
{
	char title[AE_WS_TEXT];     /* shown: the mission's title, else its map's name */
	char level[AE_WS_TEXT];
};

struct ae_ws_campaign
{
	char title[AE_WS_TEXT];     /* shown: the manifest's title, else the first mission's, else the package's name */
	char package[AE_WS_TEXT];
	int count;
	struct ae_ws_mission missions[AE_WS_MISSIONS];
};

/* a fault: the package's folder and the sentence CAMPAIGN shows ("HALO X is not listed: mission 2's map (m20) is not
installed."); the log's prefix is the caller's */
typedef void (*ae_ws_fault_function)(void *context, const char *package, const char *fault);

/* the campaigns of count packages, in their order, at most maximum into campaigns; how many. Each package left out, a
mission left out, a campaign cut to AE_WS_MISSIONS missions, and campaigns past maximum, each give one fault (NULL:
none) */
int ae_ws_campaigns_make(const struct ae_ws_package *packages, int count, struct ae_ws_campaign *campaigns, int maximum,
	ae_ws_fault_function fault, void *context);

/* text as CAMPAIGN shows it: printable ASCII ('?' for any other byte, a UTF-8 character's first byte only), letters in
capitals, '_' and runs of spaces one space, no space at either end; size includes the end */
void ae_ws_shown_text(const char *text, char *shown, int size);

#endif
