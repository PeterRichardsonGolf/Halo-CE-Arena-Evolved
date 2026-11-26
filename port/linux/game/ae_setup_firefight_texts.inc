/* Firefight's rules' texts: Firefight is SutlabSol's, Halo Content Evolved's (Watanoge) carries it, and these are the words of its rules
   screens (generated once, the product names taken out; a few lists hold only the values the engine has). ae_setup_options.c includes this above its rows. */

/* STARTING SKULLS and ITEM SET: one value each, the text of the glue's summary (ae_setup_firefight_summary_set) */
static char ff_summary_text[2][48] = { "NONE", "NONE" };
static const char *const ff_set_limit_values[] = { "UNLIMITED", "1 SET", "2 SETS", "3 SETS", "5 SETS", "10 SETS" };
static const char *const ff_set_limit_helps[] = {
	"Play endless sets until all defenders fall.",
	"Complete 1 Set (3 Rounds + Bonus Round) to win.",
	"Complete 2 Sets for victory.",
	"Complete 3 Sets for victory.",
	"Complete 5 Sets for victory.",
	"Complete 10 Sets for an endurance victory." };
static const char *const ff_start_lives_values[] = { "UNLIMITED", "0 LIVES", "3 LIVES", "5 LIVES", "7 LIVES", "10 LIVES", "15 LIVES", "20 LIVES" };
static const char *const ff_start_lives_helps[] = {
	"Unlimited shared team lives.",
	"Start with 0 reserve lives (Last Man Standing).",
	"Start with 3 shared team lives.",
	"Start with 5 shared team lives.",
	"Start with 7 shared team lives.",
	"Start with 10 shared team lives.",
	"Start with 15 shared team lives.",
	"Start with 20 shared team lives." };
static const char *const ff_round_lives_values[] = { "NONE", "+1 LIFE", "+2 LIVES", "+3 LIVES", "+5 LIVES" };
static const char *const ff_round_lives_helps[] = {
	"No bonus lives awarded upon completing a Round.",
	"Award +1 shared team life after each Round.",
	"Award +2 shared team lives after each Round.",
	"Award +3 shared team lives after each Round.",
	"Award +5 shared team lives after each Round." };
static const char *const ff_bonus_lives_values[] = { "NONE", "+1 LIFE", "+2 LIVES", "+3 LIVES", "+5 LIVES", "+10 LIVES" };
static const char *const ff_bonus_lives_helps[] = {
	"No bonus lives awarded upon completing a Set.",
	"Award +1 shared team life after each Set.",
	"Award +2 shared team lives after each Set.",
	"Award +3 shared team lives after each Set.",
	"Award +5 shared team lives after each Set.",
	"Award +10 shared team lives after each Set." };
static const char *const ff_max_lives_values[] = { "NO CAP", "5 LIVES", "10 LIVES", "15 LIVES", "20 LIVES", "30 LIVES", "50 LIVES" };
static const char *const ff_max_lives_helps[] = {
	"No upper cap on earned shared team reserve lives.",
	"Cap shared team reserve lives at 5.",
	"Cap shared team reserve lives at 10.",
	"Cap shared team reserve lives at 15.",
	"Cap shared team reserve lives at 20.",
	"Cap shared team reserve lives at 30.",
	"Cap shared team reserve lives at 50." };
static const char *const ff_prep_time_values[] = { "5 SECONDS", "15 SECONDS", "AUTO", "45 SECONDS", "60 SECONDS" };
static const char *const ff_prep_time_helps[] = {
	"5-second prep countdown before each Round begins.",
	"15-second prep countdown to collect ammo and gear.",
	"Short prep countdown, 5 to 20 seconds, longer when vehicles are deployed.",
	"45-second prep countdown to fortify positions.",
	"60-second prep countdown to fortify positions." };
static const char *const ff_wave_limit_values[] = { "2 ENEMIES LEFT", "60 SECONDS", "90 SECONDS", "120 SECONDS", "CLEAR ALL" };
static const char *const ff_wave_limit_helps[] = {
	"Next wave spawns when 2 or fewer enemies remain.",
	"Force-spawn the next wave after 60 seconds.",
	"Force-spawn the next wave after 90 seconds.",
	"Force-spawn the next wave after 120 seconds.",
	"Next wave spawns only when every enemy is dead." };
static const char *const ff_bonus_round_values[] = { "YES", "NO" };
static const char *const ff_bonus_round_helps[] = {
	"Enable the high-value Bonus Round after Round 3 of each Set.",
	"Skip the Bonus Round between Sets." };
static const char *const ff_bonus_dur_values[] = { "30 SECONDS", "45 SECONDS", "60 SECONDS", "90 SECONDS", "120 SECONDS" };
static const char *const ff_bonus_dur_helps[] = {
	"30-second Bonus Round timer.",
	"45-second Bonus Round timer.",
	"60-second Bonus Round timer.",
	"90-second Bonus Round timer.",
	"120-second Bonus Round timer." };
static const char *const ff_faction_values[] = { "COVENANT", "GRUNTS ONLY", "ELITES & HEAVIES", "ALL FACTIONS", "FLOOD", "SENTINELS", "COV + FLOOD", "COV + SENTINELS", "FLOOD + SENTINELS", "HUMANS (UNSC)" };
static const char *const ff_faction_helps[] = {
	"Spawn Covenant waves (Grunts, Jackals, Elites, and Hunters).",
	"Spawn nothing but Covenant Grunts.",
	"Spawn Covenant Elites and Hunters, with no Grunts or Jackals.",
	"Rotate Covenant, Flood, and Sentinels across Rounds.",
	"Spawn Flood waves (Infection, Combat, and Carrier Forms).",
	"Spawn Forerunner Sentinel and Enforcer waves.",
	"Spawn mixed Covenant and Flood waves together.",
	"Spawn mixed Covenant and Sentinel waves together.",
	"Spawn mixed Flood and Sentinel waves together.",
	"Fight UNSC Marines, ODSTs, Snipers, and Rocket teams." };
static const char *const ff_allegiance_values[] = { "ALLIED VS TEAM", "FREE-FOR-ALL" };
static const char *const ff_allegiance_helps[] = {
	"Covenant, Flood, and Sentinels ally together against the players.",
	"Enemy factions fight both the players and each other (Multi-Team Chaos)." };
static const char *const ff_difficulty_values[] = { "EASY", "NORMAL", "HEROIC", "LEGENDARY" };
static const char *const ff_difficulty_helps[] = {
	"Easy AI combat difficulty.",
	"Normal AI combat difficulty.",
	"Heroic AI combat difficulty.",
	"Legendary AI combat difficulty." };
static const char *const ff_escalation_values[] = { "+25% / SET", "OFF", "+15% / SET", "+40% / SET", "+60% / SET" };
static const char *const ff_escalation_helps[] = {
	"Enemy health and shields increase by +25% after each completed Set.",
	"Disable per-Set enemy health and shield escalation.",
	"Enemy health and shields increase by +15% after each completed Set.",
	"Enemy health and shields increase by +40% after each completed Set.",
	"Enemy health and shields increase by +60% after each completed Set." };
static const char *const ff_enemy_hp_values[] = { "100%", "50%", "150%", "200%", "300%" };
static const char *const ff_enemy_hp_helps[] = {
	"100% base enemy body health.",
	"50% base enemy body health.",
	"150% base enemy body health.",
	"200% base enemy body health.",
	"300% base enemy body health." };
static const char *const ff_enemy_shd_values[] = { "100%", "50%", "150%", "200%", "300%" };
static const char *const ff_enemy_shd_helps[] = {
	"100% base enemy energy shield strength.",
	"50% base enemy energy shield strength.",
	"150% base enemy energy shield strength.",
	"200% base enemy energy shield strength.",
	"300% base enemy energy shield strength." };
static const char *const ff_wave_size_values[] = { "SMALL (50%)", "NORMAL (100%)", "LARGE (150%)", "HORDE (200%)", "SWARM (250%)", "LEGION (300%)" };
static const char *const ff_wave_size_helps[] = {
	"Small squad size (50% enemy count for solo play).",
	"Normal squad size (100% enemy count per wave).",
	"Large squad size (150% enemy count per wave).",
	"Massive Horde squad size (200% enemy count).",
	"Swarm squad size (250% enemy count).",
	"Legion squad size (300% enemy count)." };
static const char *const ff_boss_freq_values[] = { "WAVE 5 ONLY", "EVERY 3RD WAVE", "BOSS RUSH" };
static const char *const ff_boss_freq_helps[] = {
	"Boss squads spawn on Wave 5 of each Round.",
	"Boss squads spawn on Waves 3 and 5 of each Round.",
	"Boss Rush: Every wave spawns heavy commanders, Hunters, and vehicles!" };
static const char *const ff_plasma_flinch_values[] = { "OFF", "25%", "50%", "75%", "100%" };
static const char *const ff_plasma_flinch_helps[] = {
	"Plasma hits never slow or stun you, as in the campaign.",
	"Plasma hits stun you a quarter as much as in multiplayer.",
	"Plasma hits stun you half as much as in multiplayer.",
	"Plasma hits stun you three quarters as much as in multiplayer.",
	"Plasma hits slow and stun you as in multiplayer." };
static const char *const ff_enemy_loadout_values[] = { "DEFAULT", "HUMAN", "COVENANT", "RANDOMIZED" };
static const char *const ff_enemy_loadout_helps[] = {
	"Enemies wield their standard faction weapons.",
	"Enemies wield any human weapon they can hold.",
	"Enemies wield any Covenant weapon they can hold.",
	"Fiesta: Every enemy spawns with a random UNSC or Covenant weapon!" };
static const char *const ff_u_grunts_values[] = { "ON", "OFF" };
static const char *const ff_u_grunts_helps[] = {
	"Allow Covenant Grunts (Minor, Major, Heavy) to spawn.",
	"Disable Covenant Grunts from spawning." };
static const char *const ff_u_jackals_values[] = { "ON", "OFF" };
static const char *const ff_u_jackals_helps[] = {
	"Allow Covenant Shield Jackals to spawn.",
	"Disable Covenant Shield Jackals from spawning." };
static const char *const ff_u_elites_values[] = { "ON", "OFF" };
static const char *const ff_u_elites_helps[] = {
	"Allow Covenant Elites (Minor, Major) to spawn.",
	"Disable standard Covenant Elites from spawning." };
static const char *const ff_u_specops_values[] = { "ON", "OFF" };
static const char *const ff_u_specops_helps[] = {
	"Allow Stealth and SpecOps Elites to spawn.",
	"Disable Stealth and SpecOps Elites from spawning." };
static const char *const ff_u_zealots_values[] = { "ON", "OFF" };
static const char *const ff_u_zealots_helps[] = {
	"Allow Zealot Field Marshals and Sword Commanders to spawn.",
	"Disable Zealot Field Marshals from spawning." };
static const char *const ff_u_hunters_values[] = { "ON", "OFF" };
static const char *const ff_u_hunters_helps[] = {
	"Allow Covenant Hunters to spawn in heavy and boss waves.",
	"Disable Covenant Hunters from spawning." };
static const char *const ff_u_flood_c_values[] = { "ON", "OFF" };
static const char *const ff_u_flood_c_helps[] = {
	"Allow Flood Elite and Human Combat Forms to spawn.",
	"Disable Flood Combat Forms from spawning." };
static const char *const ff_u_flood_k_values[] = { "ON", "OFF" };
static const char *const ff_u_flood_k_helps[] = {
	"Allow Explosive Flood Carrier and Infection Forms to spawn.",
	"Disable Flood Carrier and Infection Forms." };
static const char *const ff_u_sents_values[] = { "ON", "OFF" };
static const char *const ff_u_sents_helps[] = {
	"Allow Forerunner Sentinels and UNSC Marine enemy units to spawn.",
	"Disable Forerunner Sentinels and UNSC Marine enemy units." };
static const char *const ff_dropships_values[] = { "AUTO TELEPORT", "GROUND ONLY", "PLASMA BEACON", "FLOOD BURST" };
static const char *const ff_dropships_helps[] = {
	"Insert enemy squads with Covenant Teleportation beacons at each active wave zone.",
	"Spawn enemy squads directly at ground zones with no insertion beacon effect.",
	"Highlight wave insertion zones with Covenant Plasma Orbital Beacons.",
	"Highlight wave insertion zones with Flood Spore Burst effects." };
static const char *const ff_boss_veh_values[] = { "WRAITH/BANSHEE", "OFF", "WRAITH ONLY", "BANSHEE ONLY", "SCORPION ONLY" };
static const char *const ff_boss_veh_helps[] = {
	"Deploy enemy Wraiths and Banshees during Boss Waves.",
	"Disable enemy Boss Wave combat vehicles.",
	"Deploy enemy Covenant Wraith mortar tanks on Boss Waves.",
	"Deploy enemy Covenant Banshees on Boss Waves.",
	"Deploy enemy Scorpion tanks on Boss Waves." };
static const char *const ff_ordnance_values[] = { "OFF", "PER SET", "PER ROUND", "HEAVY CRATES" };
static const char *const ff_ordnance_helps[] = {
	"Disable UNSC weapon and ammo resupply drops.",
	"Drop a resupply pod at the start of each Set.",
	"Drop a pod of precision and heavy ordnance at the start of each Round.",
	"Drop heavy anti-armor pods every Round and Boss Wave." };
static const char *const ff_allies_values[] = { "OFF", "2 ALLIES / ROUND", "4 ALLIES / SET" };
static const char *const ff_allies_helps[] = {
	"No friendly AI squadmates.",
	"Deploy 2 friendly AI Marines (or Elites) at the start of each Round.",
	"Deploy 4 friendly AI Marines (or Elites) at the start of each Set." };
static const char *const ff_generators_values[] = { "OFF", "1 GENERATOR", "2 GENERATORS", "3 GENERATORS" };
static const char *const ff_generators_helps[] = {
	"Standard survival Firefight with no generators to defend.",
	"Defend 1 UNSC Generator (ALPHA) from enemy assault.",
	"Defend 2 UNSC Generators (ALPHA, BETA) from enemy assault.",
	"Defend 3 UNSC Generators (ALPHA, BETA, GAMMA) with 30s Lockdown switches." };
static const char *const ff_gen_fail_values[] = { "LOSE ALL", "LOSE ANY" };
static const char *const ff_gen_fail_helps[] = {
	"Defeat only when all generators are destroyed.",
	"Defeat immediately if any single generator is destroyed." };
static const char *const ff_gen_attackers_values[] = { "NONE", "25% OF UNITS", "50% OF UNITS", "75% OF UNITS", "ALL UNITS" };
static const char *const ff_gen_attackers_helps[] = {
	"Every enemy fights the players; generators are only hit by those that reach them.",
	"A quarter of each wave goes for the generators instead of the players.",
	"Half of each wave goes for the generators instead of the players.",
	"Three quarters of each wave goes for the generators instead of the players.",
	"Every enemy goes for the generators while one is unlocked." };
static const char *const ff_prog_skulls_values[] = { "OFF (LOCKED)", "STANDARD", "RANDOM / ROUND" };
static const char *const ff_prog_skulls_helps[] = {
	"Only your chosen Starting Skulls are active; no automatic Round skulls.",
	"Activate Tough Luck (R2), Catch (R3), Black Eye (Bonus), and Set Skulls.",
	"Activate a random Firefight Skull every Round, never one already on." };
static const char *const ff_start_skulls_values[] = { ff_summary_text[0] };
static const char *const ff_start_skulls_helps[] = { "The skulls the game starts with. {A} opens the grid; a skull the variant carries shows locked on." };
static const char *const ff_primary_values[] = { "NONE", "RANDOM", "ASSAULT RIFLE", "PISTOL", "SHOTGUN", "SNIPER RIFLE", "ROCKET LAUNCHER", "PLASMA PISTOL", "PLASMA RIFLE", "NEEDLER" };
static const char *const ff_primary_helps[] = {
	"No primary weapon.",
	"A random primary weapon at each spawn.",
	"Spawn with an Assault Rifle.",
	"Spawn with a Pistol.",
	"Spawn with a Shotgun.",
	"Spawn with a Sniper Rifle.",
	"Spawn with a Rocket Launcher.",
	"Spawn with a Plasma Pistol.",
	"Spawn with a Plasma Rifle.",
	"Spawn with a Needler." };
static const char *const ff_secondary_values[] = { "NONE", "RANDOM", "ASSAULT RIFLE", "PISTOL", "SHOTGUN", "SNIPER RIFLE", "ROCKET LAUNCHER", "PLASMA PISTOL", "PLASMA RIFLE", "NEEDLER" };
static const char *const ff_secondary_helps[] = {
	"No secondary weapon.",
	"A random secondary weapon at each spawn.",
	"An Assault Rifle as the second weapon.",
	"A Pistol as the second weapon.",
	"A Shotgun as the second weapon.",
	"A Sniper Rifle as the second weapon.",
	"A Rocket Launcher as the second weapon.",
	"A Plasma Pistol as the second weapon.",
	"A Plasma Rifle as the second weapon.",
	"A Needler as the second weapon." };
static const char *const ff_shields_values[] = { "NORMAL (100%)", "NO SHIELDS", "2X OVERSHIELD", "4X JUGGERNAUT" };
static const char *const ff_shields_helps[] = {
	"Standard 100% Spartan energy shields.",
	"Disable player energy shields (0% Shields).",
	"200% Overshield capacity on spawn.",
	"400% Juggernaut energy shield capacity on spawn." };
static const char *const ff_speed_values[] = { "100%", "75%", "125%", "150%" };
static const char *const ff_speed_helps[] = {
	"100% standard player movement speed.",
	"75% reduced player movement speed.",
	"125% fast player movement speed.",
	"150% sprint-tier player movement speed." };
static const char *const ff_gravity_values[] = { "100%", "50% (LOW GRAV)", "75%", "150% (HEAVY)" };
static const char *const ff_gravity_helps[] = {
	"100% standard gravity.",
	"50% low-gravity moon jumps.",
	"75% reduced gravity.",
	"150% heavy high-gravity combat." };
static const char *const ff_ammo_values[] = { "OFF", "INF. RESERVE", "BOTTOMLESS" };
static const char *const ff_ammo_helps[] = {
	"Standard ammunition consumption.",
	"Infinite reserve ammunition and grenades (reloading still required).",
	"Bottomless Clip: Fire continuously without reloading or running out of ammo!" };
static const char *const ff_gen_hp_values[] = { "NORMAL (2000)", "FRAGILE (1000)", "HEAVY (4000)", "FORTRESS (8000)" };
static const char *const ff_gen_hp_helps[] = {
	"Standard Generator durability (2000 HP + rechargeable energy shield).",
	"Fragile Generator durability (1000 HP).",
	"Reinforced Generator durability (4000 HP).",
	"Fortress Generator durability (8000 HP)." };
static const char *const ff_tag_profile_values[] = { ff_summary_text[1] };
static const char *const ff_tag_profile_helps[] = { "The item set whose actors and weapons join the waves and drops. {A} chooses a set." };
