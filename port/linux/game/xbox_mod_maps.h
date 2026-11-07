/*
XBOX_MOD_MAPS.H

The library of known Xbox-format map packs and their maps (map_packs.c):
each pack by its folder's name (maps_<id>) and the other names the folder
can have, with its name, tag, team, credits and a public page about it; each
map by its file's fingerprint, with its pack, file, name, the pack's version,
its author and a public page about it, where known. Written by
tools/xbox_mod_maps.py from port/assets/maps/xbox_mod_maps.json; not edited by
hand.
*/

static struct xbox_mod_pack const xbox_mod_packs[] =
{
	{ "cex", "ceplus,ce+", "Halo CE+", "CE+", "VKMT", "Halo CE+ X (Original Xbox) by VKMT: Sean T (project lead, tagging, Xbox porting, design and balancing), Ludus (project lead, 3D art), Elote (Canyon, the singleplayer level), and the others the release's ModDB page credits.", "" },
	{ "pe", "patchedition", "Halo 1: Patch Edition", "PE", "Patch Edition team", "Halo 1: Patch Edition (Original Xbox, 2020) by Patch (creator, producer) and hirsute (lead developer), with Bonesaw (concepts); built on Neutral Host Edition (NHE) by dds, insidi0us and the NHE team.", "" },
};

static struct xbox_mod_map const xbox_mod_maps[] =
{
	{ 0x01918a42UL, "pe", "ratrace", "Pat Race", "2020-01-29", "Patch Edition team", "" },
	{ 0x0766b450UL, "pe", "chillout", "Chill Out", "2020-01-29", "Patch Edition team", "" },
	{ 0x2c9311f3UL, "pe", "prisoner", "Prisoner", "2020-01-29", "Patch Edition team", "" },
	{ 0x2cd590faUL, "cex", "putput", "Chiron TL35", "1.01", "VKMT", "" },
	{ 0x307de412UL, "cex", "cex_museum", "CE+ Museum", "1.01", "VKMT", "" },
	{ 0x30974033UL, "cex", "hangemhigh", "Fallout", "1.01", "VKMT", "" },
	{ 0x35d05b03UL, "cex", "carousel", "Magnitude", "1.01", "VKMT", "" },
	{ 0x50f6e863UL, "pe", "dammy", "Clonky", "2020-01-29", "Patch Edition team", "" },
	{ 0x586e6e5eUL, "pe", "downrush", "Downrush", "2020-01-29", "Patch Edition team", "" },
	{ 0x5d4ca835UL, "cex", "ratrace", "Underground", "1.01", "VKMT", "" },
	{ 0x657a401fUL, "pe", "hangemhigh", "Hang 'Em High", "2020-01-29", "Patch Edition team", "" },
	{ 0x65f52524UL, "cex", "longest", "Intensity", "1.01", "VKMT", "" },
	{ 0x77188ba8UL, "cex", "beavercreek", "Delta Ruined", "1.01", "VKMT", "" },
	{ 0x80042d23UL, "pe", "badcreek", "Better Creek", "2020-01-29", "Patch Edition team", "" },
	{ 0x8058870eUL, "cex", "ui", "Menus", "1.01", "VKMT", "" },
	{ 0x821b99c4UL, "pe", "ui", "Menus", "2020-01-29", "Patch Edition team", "" },
	{ 0x8c03bb53UL, "pe", "imminent", "Imminent", "2020-01-29", "Patch Edition team", "" },
	{ 0x92f96180UL, "cex", "bloodgulch", "Abyss", "1.01", "VKMT", "" },
	{ 0x94652288UL, "cex", "prisoner", "Hektik", "1.01", "VKMT", "" },
	{ 0xabc9226bUL, "cex", "boardingaction", "Nitra", "1.01", "VKMT", "" },
	{ 0xc759da54UL, "cex", "chillout", "CO2", "1.01", "VKMT", "" },
	{ 0xcd2996c4UL, "pe", "overflow", "Overflow", "2020-01-29", "Patch Edition team", "" },
	{ 0xcfc37153UL, "cex", "sidewinder", "Retribution", "1.01", "VKMT", "" },
	{ 0xd1289cacUL, "pe", "derelict", "Dreadnought", "2020-01-29", "Patch Edition team", "" },
	{ 0xd984d506UL, "cex", "wizard", "Gothra", "1.01", "VKMT", "" },
	{ 0xda1229a7UL, "cex", "damnation", "Fissure Fall", "1.01", "VKMT", "" },
};
