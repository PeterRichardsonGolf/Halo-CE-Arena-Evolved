/*
AE_SENSOR_RANGE_RULES.H

SENSOR RANGE, a gametype rule (seed revision 19: 10 m in every AE gametype with the motion sensor on), pure
(port/linux/tests/ae_sensor_range_rules_test.c): ae_sensor.c applies it to the map's HUD globals as each game's
HUD loads (hud.c's AE hook).

Where it lives: bits 6 and 7 of the universal variant's flags (game_engine.h: the Xbox game's bits are 0-5, the
port's own from 16; nothing reads 6-15). The flags travel with the variant (the saved file's and the network
game's sizes are unchanged), so every machine in a game draws the host's range; every gametype from before has 0
there (STOCK), and older builds ignore them (they draw the map's own range).

The range is hud_globals' motion_sensor_range, in world units: the sensor's reach (motion_sensor.c: which units
take a blip, how far out a blip is drawn) and how far away enemies' names are shown (hud.c). Metres are the
sensor art's: CE's own art reads "15m" for the stock maps' 20 world units, 0.75 m a world unit
(ae_sensor_geometry.c's label says the same), so 10 m is 13.33 world units, two thirds of stock.
*/

#ifndef __AE_SENSOR_RANGE_RULES_H
#define __AE_SENSOR_RANGE_RULES_H

#define AE_SENSOR_RANGE_SHIFT 6
#define AE_SENSOR_RANGE_MASK (3UL << AE_SENSOR_RANGE_SHIFT)
#define AE_SENSOR_RANGE_MAP_NAME_LENGTH 128
/* the sensor art's metres in a world unit (ae_sensor_geometry.c's METRES_PER_WORLD_UNIT) */
#define AE_SENSOR_RANGE_METRES_PER_WORLD_UNIT 0.75f

enum ae_sensor_range
{
	/* the map's own (hud_globals as the map has it): every gametype from before */
	_ae_sensor_range_stock = 0,
	_ae_sensor_range_10m,
	_ae_sensor_range_15m,
	_ae_sensor_range_25m,
	NUMBER_OF_AE_SENSOR_RANGES
};

static inline int ae_sensor_range_get(unsigned long flags)
{
	return (int)((flags & AE_SENSOR_RANGE_MASK) >> AE_SENSOR_RANGE_SHIFT);
}

/* the flags with the range set (the other bits kept; a range out of the list is STOCK) */
static inline unsigned long ae_sensor_range_flags_set(unsigned long flags, int range)
{
	if (range < 0 || range >= NUMBER_OF_AE_SENSOR_RANGES)
		range = _ae_sensor_range_stock;
	return (flags & ~AE_SENSOR_RANGE_MASK) | ((unsigned long)range << AE_SENSOR_RANGE_SHIFT);
}

/* the range's metres; 0 for STOCK */
static inline int ae_sensor_range_metres(int range)
{
	switch (range)
	{
	case _ae_sensor_range_10m: return 10;
	case _ae_sensor_range_15m: return 15;
	case _ae_sensor_range_25m: return 25;
	}
	return 0;
}

/* the world units the HUD globals get: the range's metres, or the map's own (stock_world_units) for STOCK */
static inline float ae_sensor_range_world_units(int range, float stock_world_units)
{
	int metres = ae_sensor_range_metres(range);

	return metres > 0 ? (float)metres / AE_SENSOR_RANGE_METRES_PER_WORLD_UNIT : stock_world_units;
}

/* the map's own range, kept across games: hud_globals is the map's tag data, which a next game on the same map
may find as the last game left it (not loaded again). The value found is the map's own unless it is the same map's
field the last game set and still holds what it set */
struct ae_sensor_range_memory
{
	void const *field;
	float stock;
	float applied;
	int valid;
	char map[AE_SENSOR_RANGE_MAP_NAME_LENGTH];
};

static inline int ae_sensor_range_same_map(struct ae_sensor_range_memory const *memory, char const *map)
{
	int i;

	if (!map)
		map = "";
	for (i = 0; i < AE_SENSOR_RANGE_MAP_NAME_LENGTH - 1; i++)
	{
		if (memory->map[i] != map[i])
			return 0;
		if (!map[i])
			return 1;
	}
	return 1;
}

static inline void ae_sensor_range_remember_map(struct ae_sensor_range_memory *memory, char const *map)
{
	int i;

	if (!map)
		map = "";
	for (i = 0; i < AE_SENSOR_RANGE_MAP_NAME_LENGTH - 1 && map[i]; i++)
		memory->map[i] = map[i];
	memory->map[i] = 0;
}

/* the value to write to field (which holds current) for a game with range; memory updated */
static inline float ae_sensor_range_next(struct ae_sensor_range_memory *memory, char const *map, void const *field,
	float current, int range)
{
	float wanted;

	if (!(memory->valid && ae_sensor_range_same_map(memory, map) && memory->field == field &&
		memory->applied == current))
		memory->stock = current;
	wanted = ae_sensor_range_world_units(range, memory->stock);
	memory->field = field;
	memory->applied = wanted;
	ae_sensor_range_remember_map(memory, map);
	memory->valid = 1;
	return wanted;
}

#endif
