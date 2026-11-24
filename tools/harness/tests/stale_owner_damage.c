/*
STALE_OWNER_DAMAGE.C

Damage whose owner object has been deleted: a projectile still in flight
when the vehicle that fired it (or whose explosion threw it) is deleted, or
is deleted and made again in the same slot by a script's object_create_anew.
The damage code runs here on the real data arrays (memory/data.c), so a
deleted object's index is refused as it is in the game: datum_get's report
in a release build, after which the game reads through a NULL and crashes,
is a failed CHECK here.
*/

/* the real data arrays, not the harness's */
#define data_array harness_data_array
#define datum_header harness_datum_header
#define game_state_data_new harness_game_state_data_new
#define datum_new harness_datum_new
#define datum_get harness_datum_get
#define datum_delete harness_datum_delete
#include "harness.h"
#undef data_array
#undef datum_header
#undef game_state_data_new
#undef datum_new
#undef datum_get
#undef datum_delete

#define HALO_RELEASE
typedef unsigned short word;
#define match_assert(file, line, condition) ((void)0)
#define match_vassert(file, line, condition, message) ((void)0)
#define csprintf(buffer, ...) snprintf(buffer, sizeof(buffer), "an index is unused or changed")

static void release_assert_failed(char const *message, char const *file, int line, boolean halt)
{
	CHECK(FALSE, "%s", message);
}

#define XPTR(type) type *
#define XBOX_ADDRESS(pointer) ((void *)(pointer))
#define xbox_pointer(address) ((void *)(address))
#define DATA_ARRAY_OWN_ELEMENTS(array) ((void *)((array) + 1))
#include "types.inc"

/* the fake world: objects with what the code under test reads */
struct object_datum
{
	struct
	{
		short type;
		long parent_object_index;
	} object;
};
#define unit_datum object_datum
struct player_datum { short identifier; };
enum { MAXIMUM_FAKE_OBJECTS = 16, MAXIMUM_FAKE_PLAYERS = 4 };

struct data_array *object_header_data;
static struct object_datum fake_objects[MAXIMUM_FAKE_OBJECTS];
static long fake_unit_players[MAXIMUM_FAKE_OBJECTS];
static boolean fake_players_live[MAXIMUM_FAKE_PLAYERS];
static int player_lookups;

static struct player_datum *player_try_and_get(long player_index)
{
	static struct player_datum player;
	return player_index>=0 && player_index<MAXIMUM_FAKE_PLAYERS && fake_players_live[player_index] ? &player : NULL;
}

static long player_index_from_unit_index(long unit_index)
{
	player_lookups++;
	return fake_unit_players[unit_index&0xFFFF];
}

/* the real data arrays' other half: a slot's next identifier always differs */
static long datum_new(struct data_array *data)
{
	short index;
	for (index = 0; index<data->maximum_count; index++)
	{
		struct datum_header *header = (struct datum_header *)((byte *)data->data+data->size*index);
		if (!header->identifier)
		{
			memset(header, 0, (size_t)data->size);
			header->identifier = (short)(data->next_identifier++|0x8000);
			if (index>=data->count)
				data->count = index+1;
			data->actual_count++;
			return ((long)(unsigned short)header->identifier<<16)|index;
		}
	}
	return NONE;
}
void *datum_try_and_get(struct data_array *data, long index);
void *datum_get(struct data_array *data, long index);
void *object_try_and_get_and_verify_type(long object_index, unsigned long valid_type_flags);
void *object_get_and_verify_type(long object_index, unsigned long valid_type_flags);
#include "under_test.inc"

static long object_new(short type, long parent_object_index)
{
	long object_index = datum_new(object_header_data);
	struct object_header_datum *header = object_header_get(object_index);
	struct object_datum *object = &fake_objects[object_index&0xFFFF];

	memset(object, 0, sizeof(*object));
	object->object.type = type;
	object->object.parent_object_index = parent_object_index;
	fake_unit_players[object_index&0xFFFF] = NONE;
	header->type = (byte)type;
	header->datum = object;
	return object_index;
}

static void object_delete(long object_index)
{
	datum_delete(object_header_data, object_index);
}

static void world_new(void)
{
	struct data_array *data = calloc(1, sizeof(*data));
	memcpy(data->name, "object", sizeof("object"));
	data->maximum_count = MAXIMUM_FAKE_OBJECTS;
	data->size = sizeof(struct object_header_datum);
	data->valid = TRUE;
	data->next_identifier = 0x1000;
	data->data = calloc(MAXIMUM_FAKE_OBJECTS, sizeof(struct object_header_datum));
	object_header_data = data;
	memset(fake_players_live, 0, sizeof(fake_players_live));
	fake_players_live[2] = TRUE;
	player_lookups = 0;
}

/* a projectile's damage with that owner, through the checks object_cause_damage makes before reading its owner */
static long owner_player(long owner_object_index, struct damage_data *damage)
{
	memset(damage, 0, sizeof(*damage));
	damage->owner_player_index = 2;
	damage->owner_object_index = owner_object_index;
	damage_data_validate_owner(damage);
	return get_player_index_from_object_or_parents(damage->owner_object_index);
}

int main(int argc, char **argv)
{
	char const *case_name = argv[1];
	struct damage_data damage;

	world_new();

	CASE("live-vehicle")
	{
		long vehicle = object_new(_object_type_vehicle, NONE);

		fake_unit_players[vehicle&0xFFFF] = 2;
		CHECK(owner_player(vehicle, &damage)==2, "the living vehicle's player");
		CHECK(damage.owner_object_index==vehicle, "the living owner kept");
		CHECK(damage.owner_player_index==2, "the player kept");
		return 0;
	}
	CASE("live-parent")
	{
		long biped = object_new(_object_type_biped, NONE);
		long weapon = object_new(_object_type_weapon, biped);

		fake_unit_players[biped&0xFFFF] = 2;
		CHECK(owner_player(weapon, &damage)==2, "the parent unit's player");
		CHECK(damage.owner_object_index==weapon, "the living owner kept");
		return 0;
	}
	CASE("deleted")
	{
		long vehicle = object_new(_object_type_vehicle, NONE);

		object_delete(vehicle);
		CHECK(owner_player(vehicle, &damage)==NONE, "no player from a deleted owner");
		CHECK(damage.owner_object_index==NONE, "the deleted owner dropped");
		CHECK(damage.owner_player_index==2, "the player still credited");
		CHECK(player_lookups==0, "a player looked up from a deleted owner");
		return 0;
	}
	CASE("recreated")
	{
		/* object_create_anew: the old vehicle deleted, the new one in its slot with another identifier */
		long vehicle = object_new(_object_type_vehicle, NONE);
		long again;

		object_delete(vehicle);
		again = object_new(_object_type_vehicle, NONE);
		CHECK((again&0xFFFF)==(vehicle&0xFFFF) && again!=vehicle, "the same slot, another identifier");
		fake_unit_players[again&0xFFFF] = 2;
		CHECK(owner_player(vehicle, &damage)==NONE, "no player from the old vehicle");
		CHECK(damage.owner_object_index==NONE, "the old owner dropped");
		CHECK(owner_player(again, &damage)==2, "the new vehicle's player");
		return 0;
	}
	CASE("gone-player")
	{
		long vehicle = object_new(_object_type_vehicle, NONE);

		fake_players_live[2] = FALSE;
		owner_player(vehicle, &damage);
		CHECK(damage.owner_player_index==NONE, "a gone player credited");
		CHECK(damage.owner_object_index==vehicle, "the living owner kept");
		return 0;
	}
	CASE("unowned")
	{
		CHECK(owner_player(NONE, &damage)==NONE, "no owner, no player");
		CHECK(damage.owner_object_index==NONE, "no owner");
		return 0;
	}

	fprintf(stderr, "unknown case %s\n", case_name);
	return 2;
}
