/* AE's grenade catalogue's rows (ae_grenade_catalogue.h), in id order: row n is id n. Included inside
ae_grenade_catalogue.c's table. Ids are permanent: a row is never removed or moved, only marked not available. */

/* 0: the EMP. Stalls vehicles, scrambles the hit player's motion tracker and HUD, shorts powerup items on the ground.
No damage. Tags in the grenade pack under ae\grenades\emp\ (the stock plasma throwing effect) */
{
	"emp", "EMP GRENADE",
	"emp\\emp_grenade", "emp\\emp_grenade", "emp\\emp_grenades", "emp\\emp_grenade", 0, "emp\\emp_field",
	_ae_grenade_glyph_bolt, 2, 0, _ae_grenade_map_near_vehicles, 30,
	AE_GRENADE_FX_STALL | AE_GRENADE_FX_SCRAMBLE | AE_GRENADE_FX_POWERUP_SHORT,
	0, 1, 1,
	{ 1.2f, 0.2f, 2.0f, 3.5f, 0.40f, 4.0f, 1.6f, 2.0f, 5.0f, 2.0f, 0.75f }
},
/* 1: the SMOKE. Reserved: today the smoke grenade is the plasma slot's, edited while an AE SND game asks for it
(ae_smoke_grenade.c, ae_smoke_volume.c). Available once it has its own projectile and equipment in the grenade pack;
its volume rules stay ae_smoke_rules.h's (radius 2.8 wu, hold 300 ticks, fade 90) */
{
	"smoke", "SMOKE GRENADE",
	"smoke\\smoke_grenade", "smoke\\smoke_grenade", "smoke\\smoke_grenades", "smoke\\smoke_grenade", 0, 0,
	_ae_grenade_glyph_smoke, 2, 0, _ae_grenade_map_none, 30,
	AE_GRENADE_FX_SMOKE_VOLUME,
	0, 0, 2,
	{ 1.2f, 0.2f, 2.8f, 2.8f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }
},
