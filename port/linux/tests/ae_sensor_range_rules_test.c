#include <stdio.h>
#include "ae_sensor_range_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

/* the stock maps' range: 20 world units (CE's art: "15m") */
#define STOCK 20.0f

static int near(float a, float b)
{
	return a - b < 0.001f && b - a < 0.001f;
}

int main(void)
{
	/* SENSOR RANGE (seed revision 19): bits 6-7 of the universal variant's flags; the other bits kept and never
	read as it */
	{
		unsigned long const samples[] = { 0x0UL, 0x3FUL, 0xC0UL, 0xFFFFFFFFUL, 0x80000001UL, 0x0001FF3FUL,
			0x7FFF0000UL };
		unsigned int sample;
		int range;

		for (sample = 0; sample < sizeof(samples) / sizeof(samples[0]); sample++)
		{
			for (range = 0; range < NUMBER_OF_AE_SENSOR_RANGES; range++)
			{
				unsigned long flags = ae_sensor_range_flags_set(samples[sample], range);

				CHECK(ae_sensor_range_get(flags) == range);
				CHECK((flags & ~AE_SENSOR_RANGE_MASK) == (samples[sample] & ~AE_SENSOR_RANGE_MASK));
			}
			/* (a range out of the list is STOCK) */
			CHECK(ae_sensor_range_get(ae_sensor_range_flags_set(samples[sample], 4)) == _ae_sensor_range_stock);
			CHECK(ae_sensor_range_get(ae_sensor_range_flags_set(samples[sample], -1)) == _ae_sensor_range_stock);
		}
		/* (every gametype from before has 0 there: STOCK; the Xbox game's bits 0-5 and the port's from 16 are not it) */
		CHECK(ae_sensor_range_get(0x0UL) == _ae_sensor_range_stock);
		CHECK(ae_sensor_range_get(0x3FUL) == _ae_sensor_range_stock);
		CHECK(ae_sensor_range_get(0xFFFFFF00UL) == _ae_sensor_range_stock);
		CHECK(ae_sensor_range_get(0x40UL) == _ae_sensor_range_10m);
		CHECK(AE_SENSOR_RANGE_MASK == 0xC0UL);
	}
	/* the world units: the sensor art's 0.75 m a world unit; STOCK the map's own */
	{
		CHECK(ae_sensor_range_metres(_ae_sensor_range_stock) == 0);
		CHECK(ae_sensor_range_metres(_ae_sensor_range_10m) == 10);
		CHECK(ae_sensor_range_metres(_ae_sensor_range_15m) == 15);
		CHECK(ae_sensor_range_metres(_ae_sensor_range_25m) == 25);
		CHECK(near(ae_sensor_range_world_units(_ae_sensor_range_10m, STOCK), 13.3333f));
		CHECK(near(ae_sensor_range_world_units(_ae_sensor_range_15m, STOCK), 20.0f));
		CHECK(near(ae_sensor_range_world_units(_ae_sensor_range_25m, STOCK), 33.3333f));
		CHECK(near(ae_sensor_range_world_units(_ae_sensor_range_stock, STOCK), STOCK));
		CHECK(near(ae_sensor_range_world_units(_ae_sensor_range_stock, 31.5f), 31.5f));
		/* (whatever the map's own: a custom map's 10 m is 10 m too) */
		CHECK(near(ae_sensor_range_world_units(_ae_sensor_range_10m, 31.5f), 13.3333f));
	}
	/* the map's own range kept across games: hud_globals is the map's tag data, which a next game on the same map
	may find as the last game left it */
	{
		struct ae_sensor_range_memory memory = { 0 };
		float field = STOCK;
		float other_map = 31.5f;

		/* a 10 m game, then a stock game on the same map (not loaded again): the map's own again */
		field = ae_sensor_range_next(&memory, "map_a", &field, field, _ae_sensor_range_10m);
		CHECK(near(field, 13.3333f));
		field = ae_sensor_range_next(&memory, "map_a", &field, field, _ae_sensor_range_stock);
		CHECK(near(field, STOCK));
		/* two 10 m games in a row, then stock */
		field = ae_sensor_range_next(&memory, "map_a", &field, field, _ae_sensor_range_10m);
		field = ae_sensor_range_next(&memory, "map_a", &field, field, _ae_sensor_range_10m);
		CHECK(near(field, 13.3333f) && near(memory.stock, STOCK));
		field = ae_sensor_range_next(&memory, "map_a", &field, field, _ae_sensor_range_stock);
		CHECK(near(field, STOCK));
		/* 10 m, then 25 m: from the map's own, never from the last game's */
		field = ae_sensor_range_next(&memory, "map_a", &field, field, _ae_sensor_range_10m);
		field = ae_sensor_range_next(&memory, "map_a", &field, field, _ae_sensor_range_25m);
		CHECK(near(field, 33.3333f) && near(memory.stock, STOCK));
		/* another map's tag data (another field): its own value is its own */
		other_map = ae_sensor_range_next(&memory, "map_b", &other_map, other_map, _ae_sensor_range_stock);
		CHECK(near(other_map, 31.5f) && near(memory.stock, 31.5f));
		other_map = ae_sensor_range_next(&memory, "map_b", &other_map, other_map, _ae_sensor_range_10m);
		other_map = ae_sensor_range_next(&memory, "map_b", &other_map, other_map, _ae_sensor_range_stock);
		CHECK(near(other_map, 31.5f));
		/* the same field loaded again with the map's own (it no longer holds what was set): taken as the map's own */
		other_map = ae_sensor_range_next(&memory, "map_b", &other_map, other_map, _ae_sensor_range_10m);
		other_map = 31.5f;
		other_map = ae_sensor_range_next(&memory, "map_b", &other_map, other_map, _ae_sensor_range_stock);
		CHECK(near(other_map, 31.5f));
	}
	/* another map at the same tag address whose own range matches the last applied value */
	{
		struct ae_sensor_range_memory memory = { 0 };
		float field = 30.0f;

		field = ae_sensor_range_next(&memory, "map_a", &field, field, _ae_sensor_range_10m);
		CHECK(near(field, 13.3333f));
		field = memory.applied;
		field = ae_sensor_range_next(&memory, "map_b", &field, field, _ae_sensor_range_stock);
		CHECK(near(field, 13.3333f) && near(memory.stock, 13.3333f));
	}
	printf("ae_sensor_range_rules: %d failed\n", failures);
	return failures ? 1 : 0;
}
