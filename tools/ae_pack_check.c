/*
AE_PACK_CHECK.C

Weapon packs (docs/weapon-packs.md) checked as the game checks them
(port/linux/game/ae_pack_format.c): each pack opened and every resource
read, then its header, sections, tags (with their flags) and weapons
printed, and a line for each pack:
	<pack>: ok
	<pack>: refused: <reason>
Exits 1 if any pack was refused. tools/weapon_pack/aepack.py prints the
same lines for a pack it accepts (tools/test_weapon_pack_format.py holds
the two to that).

	ae_pack_check [--no-resources] pack...
	ae_pack_check --sha256 file...          the files' SHA-256 (fed to
	                                        ae_pack_sha256 in uneven pieces)
	ae_pack_check --rewrite in out          in read and written again by
	                                        ae_pack_write (stored, R2)
	ae_pack_check --writer-check out        ae_pack_write's own checks

Built by ninja linux as build/linux/ae_pack_check, with the platform
layer's POSIX flags, against the game's object of ae_pack_format.c (whose
functions take and return the same types in both ABIs; as map_validate).
*/

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "ae_pack_format.h"

/* ---------- the game's C library names (port/linux/include/stdio.h), which its object calls */

FILE *halo_linux_fopen(char const *path, char const *mode);
int halo_linux_remove(char const *path);
int halo_linux_rename(char const *old_path, char const *new_path);
int halo_linux_vsnprintf(char *buffer, size_t count, char const *format, va_list arguments);
int halo_linux_snprintf(char *buffer, size_t count, char const *format, ...);

FILE *halo_linux_fopen(char const *path, char const *mode)
{
	return fopen(path, mode);
}

int halo_linux_remove(char const *path)
{
	return remove(path);
}

int halo_linux_rename(char const *old_path, char const *new_path)
{
	return rename(old_path, new_path);
}

int halo_linux_vsnprintf(char *buffer, size_t count, char const *format, va_list arguments)
{
	return vsnprintf(buffer, count, format, arguments);
}

int halo_linux_snprintf(char *buffer, size_t count, char const *format, ...)
{
	va_list arguments;
	int result;

	va_start(arguments, format);
	result = vsnprintf(buffer, count, format, arguments);
	va_end(arguments);
	return result;
}

/* ---------- printing (as tools/weapon_pack/aepack.py's describe) */

static char const *group_text(unsigned int group, char *buffer)
{
	int index;

	if (group == AE_PACK_NONE)
		return "-";
	for (index = 0; index < 4; index++)
	{
		unsigned int c = (group >> (24 - index * 8)) & 0xFF;

		if (c < 0x20 || c > 0x7E)
		{
			sprintf(buffer, "0x%08x", group);
			return buffer;
		}
		buffer[index] = (char)c;
	}
	buffer[4] = 0;
	return buffer;
}

static char const *flags_text(unsigned int flags)
{
	switch (flags & AE_PACK_TAG_KNOWN_FLAGS)
	{
	case AE_PACK_TAG_FLAG_SMOOTH:
		return "smooth";
	case AE_PACK_TAG_FLAG_METER_COVERAGE:
		return "coverage";
	case AE_PACK_TAG_FLAG_SMOOTH | AE_PACK_TAG_FLAG_METER_COVERAGE:
		return "smooth,coverage";
	default:
		return "-";
	}
}

static char const *text(struct ae_pack const *pack, unsigned int offset)
{
	char const *string = ae_pack_string(pack, offset);

	return string ? string : "-";
}

static void describe(struct ae_pack const *pack)
{
	struct ae_pack_header const *header = &pack->header;
	unsigned int index;
	char group[16], parent0[16], parent1[16], items[16], icon[32];

	printf("format %u header %u flags %u layout %u size %u\n", header->format_version, header->header_size,
		header->flags, header->tag_layout, pack->file_size);
	printf("id %s revision %u name \"%s\"\n", header->pack_id, header->pack_revision, header->display_name);
	printf("sha256 ");
	for (index = 0; index < AE_PACK_SHA256_SIZE; index++)
		printf("%02x", header->sha256[index]);
	printf("\n");
	for (index = 0; index < header->section_count; index++)
	{
		struct ae_pack_section const *section = &pack->sections[index];

		printf("section %s offset %u stored %u inflated %u compression %u\n", ae_pack_section_name(section->kind),
			section->offset, section->stored_size, section->inflated_size, section->compression);
	}
	for (index = 0; index < pack->tag_count; index++)
	{
		struct ae_pack_tag_record const *tag = &pack->tags[index];

		printf("tag %u %s parents %s,%s %s required %u flags %s bytes %u at %u fixups %u from %u parts %u from %u "
			"resources %u from %u name \"%s\" source \"%s\"\n", index, group_text(tag->group, group),
			group_text(tag->parent_groups[0], parent0), group_text(tag->parent_groups[1], parent1),
			tag->kind == AE_PACK_TAG_OWN ? "own" : "external", (unsigned int)tag->required, flags_text(tag->flags),
			tag->bytes_size, tag->bytes_offset, tag->fixups_count, tag->fixups_first, tag->parts_count,
			tag->parts_first, tag->resources_count, tag->resources_first, text(pack, tag->name),
			text(pack, tag->source_path));
	}
	for (index = 0; index < pack->weapon_count; index++)
	{
		struct ae_pack_weapon_record const *weapon = &pack->weapons[index];

		if (weapon->item_collection_tag == AE_PACK_NONE)
			strcpy(items, "NONE");
		else
			sprintf(items, "%u", weapon->item_collection_tag);
		if (weapon->icon_bitmap_tag == AE_PACK_NONE)
			strcpy(icon, "NONE");
		else
			sprintf(icon, "%u:%u", weapon->icon_bitmap_tag, weapon->icon_sequence);
		printf("weapon %u key %s root %u items %s behaviour %u label %s fallbacks %s,%s menu \"%s\" pickup \"%s\" "
			"ammo \"%s\" icon %s replaces %u power %u\n", index, text(pack, weapon->key), weapon->root_tag, items,
			weapon->behaviour, text(pack, weapon->label), text(pack, weapon->fallback_labels[0]),
			text(pack, weapon->fallback_labels[1]), text(pack, weapon->menu_name),
			text(pack, weapon->weapon_pickup_text), text(pack, weapon->ammo_pickup_text), icon, weapon->replaces, weapon->power);
	}
	printf("credits %u bytes\n", pack->credits_size);
	printf("resources %u of %u bytes\n", pack->resource_count, pack->resources_size);
}

static int check(char const *path, int resources)
{
	struct ae_pack *pack;
	struct ae_pack_problem problem;

	printf("pack %s\n", path);
	if (!ae_pack_open(path, &pack, &problem) || (resources && !ae_pack_verify_resources(pack, &problem)))
	{
		ae_pack_close(pack);
		printf("%s: refused: %s\n", path, problem.text);
		return 0;
	}
	describe(pack);
	printf("%s: ok\n", path);
	ae_pack_close(pack);
	return 1;
}

/* ---------- SHA-256 of files */

static unsigned char *file_read(char const *path, unsigned int *size)
{
	FILE *file = fopen(path, "rb");
	unsigned char *data = NULL;
	unsigned int capacity = 0;

	*size = 0;
	if (!file)
		return NULL;
	for (;;)
	{
		size_t read;

		if (*size == capacity)
		{
			unsigned char *grown;

			capacity = capacity ? capacity * 2 : 65536;
			grown = (unsigned char *)realloc(data, capacity);
			if (!grown)
			{
				free(data);
				fclose(file);
				return NULL;
			}
			data = grown;
		}
		read = fread(data + *size, 1, capacity - *size, file);
		if (!read)
			break;
		*size += (unsigned int)read;
	}
	fclose(file);
	return data;
}

static int sha256_files(int count, char **paths)
{
	static unsigned int const pieces[] = { 1, 3, 64, 65, 127, 55, 56, 1000, 4096, 63 };
	int index;

	for (index = 0; index < count; index++)
	{
		unsigned int size, position = 0, piece = 0, k;
		unsigned char *data = file_read(paths[index], &size);
		struct ae_pack_sha256 context;
		unsigned char digest[AE_PACK_SHA256_SIZE];

		if (!data)
		{
			fprintf(stderr, "cannot read %s\n", paths[index]);
			return 1;
		}
		ae_pack_sha256_begin(&context);
		while (position < size)
		{
			unsigned int take = pieces[piece++ % (sizeof(pieces) / sizeof(pieces[0]))];

			if (take > size - position)
				take = size - position;
			ae_pack_sha256_add(&context, data + position, take);
			position += take;
		}
		ae_pack_sha256_end(&context, digest);
		free(data);
		for (k = 0; k < AE_PACK_SHA256_SIZE; k++)
			printf("%02x", digest[k]);
		printf("  %s\n", paths[index]);
	}
	return 0;
}

/* ---------- the writer */

/* the open pack written again through ae_pack_write */
static int rewrite(char const *in, char const *out)
{
	struct ae_pack *pack;
	struct ae_pack_problem problem;
	struct ae_pack_build_data data;
	struct ae_pack_build_tag *tags = NULL;
	struct ae_pack_build_part *parts = NULL;
	struct ae_pack_build_resource *resources = NULL;
	struct ae_pack_build_weapon *weapons = NULL;
	unsigned char **resource_data = NULL;
	unsigned int index, k;
	int result = 0;

	if (!ae_pack_open(in, &pack, &problem) || !ae_pack_verify_resources(pack, &problem))
	{
		ae_pack_close(pack);
		printf("%s: refused: %s\n", in, problem.text);
		return 1;
	}
	tags = (struct ae_pack_build_tag *)calloc(pack->tag_count + 1, sizeof(*tags));
	parts = (struct ae_pack_build_part *)calloc(pack->part_count + 1, sizeof(*parts));
	resources = (struct ae_pack_build_resource *)calloc(pack->resource_count + 1, sizeof(*resources));
	resource_data = (unsigned char **)calloc(pack->resource_count + 1, sizeof(*resource_data));
	weapons = (struct ae_pack_build_weapon *)calloc(pack->weapon_count + 1, sizeof(*weapons));
	if (!tags || !parts || !resources || !resource_data || !weapons)
		goto done;
	for (index = 0; index < pack->part_count; index++)
	{
		struct ae_pack_part_record const *part = &pack->parts[index];

		parts[index].offset = part->offset;
		parts[index].vertex_type = part->vertex_type;
		parts[index].triangle_type = part->triangle_type;
		parts[index].vertex_count = part->vertex_count;
		parts[index].triangle_count = part->triangle_count;
		parts[index].vertices = pack->geometry + part->vertices_offset;
		parts[index].vertices_size = part->vertices_size;
		parts[index].indices = pack->geometry + part->indices_offset;
		parts[index].indices_size = part->indices_size;
	}
	for (index = 0; index < pack->resource_count; index++)
	{
		struct ae_pack_resource_record const *resource = &pack->resources[index];

		resource_data[index] = (unsigned char *)malloc(resource->size);
		if (!resource_data[index] ||
			!ae_pack_read_resource(pack, resource->tag, resource->map_offset, resource->size, resource_data[index]))
		{
			goto done;
		}
		resources[index].map_offset = resource->map_offset;
		resources[index].data = resource_data[index];
		resources[index].size = resource->size;
	}
	for (index = 0; index < pack->tag_count; index++)
	{
		struct ae_pack_tag_record const *tag = &pack->tags[index];

		tags[index].group = tag->group;
		tags[index].parent_groups[0] = tag->parent_groups[0];
		tags[index].parent_groups[1] = tag->parent_groups[1];
		tags[index].name = ae_pack_string(pack, tag->name);
		tags[index].source_path = ae_pack_string(pack, tag->source_path);
		tags[index].kind = tag->kind;
		tags[index].required = tag->required;
		tags[index].flags = tag->flags;
		tags[index].bytes = pack->tag_bytes + tag->bytes_offset;
		tags[index].bytes_size = tag->bytes_size;
		tags[index].fixups = pack->fixups + tag->fixups_first;
		tags[index].fixup_count = tag->fixups_count;
		tags[index].parts = parts + tag->parts_first;
		tags[index].part_count = tag->parts_count;
		tags[index].resources = resources + tag->resources_first;
		tags[index].resource_count = tag->resources_count;
	}
	for (index = 0; index < pack->weapon_count; index++)
	{
		struct ae_pack_weapon_record const *weapon = &pack->weapons[index];

		weapons[index].key = ae_pack_string(pack, weapon->key);
		weapons[index].root_tag = weapon->root_tag;
		weapons[index].item_collection_tag = weapon->item_collection_tag;
		weapons[index].behaviour = weapon->behaviour;
		weapons[index].label = ae_pack_string(pack, weapon->label);
		for (k = 0; k < 2; k++)
		{
			weapons[index].fallback_labels[k] = ae_pack_string(pack, weapon->fallback_labels[k]);
		}
		weapons[index].weapon_pickup_text = ae_pack_string(pack, weapon->weapon_pickup_text);
		weapons[index].ammo_pickup_text = ae_pack_string(pack, weapon->ammo_pickup_text);
		weapons[index].menu_name = ae_pack_string(pack, weapon->menu_name);
		weapons[index].icon_bitmap_tag = weapon->icon_bitmap_tag;
		weapons[index].icon_sequence = weapon->icon_sequence;
		weapons[index].replaces = weapon->replaces;
		weapons[index].power = weapon->power;
	}
	memset(&data, 0, sizeof(data));
	data.pack_id = pack->header.pack_id;
	data.pack_revision = pack->header.pack_revision;
	data.display_name = pack->header.display_name;
	data.tags = tags;
	data.tag_count = pack->tag_count;
	data.weapons = weapons;
	data.weapon_count = pack->weapon_count;
	data.credits = pack->credits;
	result = ae_pack_write(out, &data, &problem);
	if (!result)
		printf("%s: not written: %s\n", out, problem.text);

done:
	if (resource_data)
	{
		for (index = 0; index < pack->resource_count; index++)
			free(resource_data[index]);
	}
	free(resource_data);
	free(resources);
	free(parts);
	free(tags);
	free(weapons);
	ae_pack_close(pack);
	return result ? 0 : 1;
}

/* (ae_pack_replace_file's moves, failing on the calls whose bits are set: 1 the first) */
static unsigned int move_calls, move_failing;

static int scripted_move(char const *from, char const *to)
{
	move_calls++;
	if (move_calls < 32 && (move_failing & (1u << move_calls)))
		return -1;
	return rename(from, to);
}

static void file_write(char const *path, char const *text)
{
	FILE *file = fopen(path, "wb");

	if (file)
	{
		fputs(text, file);
		fclose(file);
	}
}

static int file_exists(char const *path)
{
	FILE *file = fopen(path, "rb");

	if (!file)
		return 0;
	fclose(file);
	return 1;
}

/* a small pack made here: written and read back, and the writer refusing what a reader would refuse */
static int writer_check(char const *out)
{
	static unsigned char weapon_bytes[64], bitmap_bytes[32], pixels[100];
	struct ae_pack_fixup_record fixups[2];
	struct ae_pack_build_resource resource;
	struct ae_pack_build_tag tags[2];
	struct ae_pack_build_weapon weapon;
	struct ae_pack_build_data data;
	struct ae_pack_problem problem;
	struct ae_pack *pack;
	unsigned char read_back[40];
	char temporary[4096];
	int failures = 0;
	unsigned int index;

	for (index = 0; index < sizeof(pixels); index++)
		pixels[index] = (unsigned char)(index * 3);
	memset(fixups, 0, sizeof(fixups));
	/* (out of offset order: the writer sorts them) */
	fixups[0].offset = 16;
	fixups[0].kind = AE_PACK_FIXUP_REFERENCE;
	fixups[0].value = 1;
	fixups[1].offset = 0;
	fixups[1].kind = AE_PACK_FIXUP_POINTER;
	fixups[1].value = 8;
	resource.map_offset = 0x4000;
	resource.data = pixels;
	resource.size = sizeof(pixels);
	memset(tags, 0, sizeof(tags));
	tags[0].group = AE_PACK_GROUP_WEAPON;
	tags[0].parent_groups[0] = 0x6974656D; /* 'item' */
	tags[0].parent_groups[1] = 0x6F626A65; /* 'obje' */
	tags[0].name = "ae\\packs\\check\\rifle";
	tags[0].source_path = "check\\rifle";
	tags[0].kind = AE_PACK_TAG_OWN;
	tags[0].bytes = weapon_bytes;
	tags[0].bytes_size = sizeof(weapon_bytes);
	tags[0].fixups = fixups;
	tags[0].fixup_count = 2;
	tags[1].group = AE_PACK_GROUP_BITMAP;
	tags[1].parent_groups[0] = AE_PACK_NONE;
	tags[1].parent_groups[1] = AE_PACK_NONE;
	tags[1].name = "ae\\packs\\check\\meter";
	tags[1].kind = AE_PACK_TAG_OWN;
	tags[1].flags = AE_PACK_TAG_FLAG_SMOOTH | AE_PACK_TAG_FLAG_METER_COVERAGE;
	tags[1].bytes = bitmap_bytes;
	tags[1].bytes_size = sizeof(bitmap_bytes);
	tags[1].resources = &resource;
	tags[1].resource_count = 1;
	memset(&weapon, 0, sizeof(weapon));
	weapon.key = "battle_rifle";
	weapon.root_tag = 0;
	weapon.item_collection_tag = AE_PACK_NONE;
	weapon.behaviour = AE_PACK_BEHAVIOUR_AE_BATTLE_RIFLE;
	weapon.label = "h_br";
	weapon.fallback_labels[0] = "ar";
	weapon.menu_name = "BATTLE RIFLE";
	weapon.weapon_pickup_text = "Picked up a battle rifle";
	weapon.ammo_pickup_text = "Picked up %d rounds for battle rifle";
	weapon.icon_bitmap_tag = 1;
	weapon.replaces = AE_PACK_REPLACES_ASSAULT_RIFLE;
	memset(&data, 0, sizeof(data));
	data.pack_id = "ae_check";
	data.pack_revision = 3;
	data.display_name = "CHECK";
	data.tags = tags;
	data.tag_count = 2;
	data.weapons = &weapon;
	data.weapon_count = 1;
	data.credits = "made by the check";

	/* written, read back */
	if (!ae_pack_write(out, &data, &problem))
	{
		printf("FAIL: the valid pack was not written: %s\n", problem.text);
		return 1;
	}
	if (!ae_pack_open(out, &pack, &problem))
	{
		printf("FAIL: the written pack was refused: %s\n", problem.text);
		return 1;
	}
	if (pack->tag_count != 2 || pack->own_tag_count != 2 || pack->weapon_count != 1 || pack->fixup_count != 2 ||
		pack->resource_count != 1 || pack->tags[1].flags != 3 || pack->header.pack_revision != 3 ||
		strcmp(ae_pack_string(pack, pack->weapons[0].key), "battle_rifle") ||
		strcmp(pack->credits, "made by the check") ||
		strcmp(ae_pack_string(pack, pack->weapons[0].ammo_pickup_text), "Picked up %d rounds for battle rifle") ||
		pack->fixups[0].offset != 0 || pack->fixups[1].offset != 16)
	{
		printf("FAIL: the written pack does not hold what was written\n");
		failures++;
	}
	/* (a part of a resource, and requests no resource holds) */
	if (!ae_pack_read_resource(pack, 1, 0x4000 + 50, 40, read_back) || memcmp(read_back, pixels + 50, 40) ||
		ae_pack_read_resource(pack, 1, 0x4000 + 61, 40, read_back) ||
		ae_pack_read_resource(pack, 1, 0x3FFF, 2, read_back) || ae_pack_read_resource(pack, 0, 0x4000, 1, read_back) ||
		ae_pack_read_resource(pack, 2, 0x4000, 1, read_back))
	{
		printf("FAIL: resource reads\n");
		failures++;
	}
	ae_pack_close(pack);

	/* refused, nothing written (nor its temporary file) */
	snprintf(temporary, sizeof(temporary), "%s.tmp", out);
#define REFUSED(what, change, undo) \
	do { \
		remove(out); \
		change; \
		if (ae_pack_write(out, &data, &problem)) \
		{ \
			printf("FAIL: %s was written\n", what); \
			failures++; \
		} \
		else if (file_exists(out) || file_exists(temporary)) \
		{ \
			printf("FAIL: %s left a file\n", what); \
			failures++; \
		} \
		else \
		{ \
			printf("refused (%s): %s\n", what, problem.text); \
		} \
		undo; \
	} while (0)

	REFUSED("an unknown flag bit", tags[1].flags = 4, tags[1].flags = 3);
	REFUSED("a flag on the weapon", tags[0].flags = 1, tags[0].flags = 0);
	REFUSED("a fixup past its tag", fixups[1].offset = 61, fixups[1].offset = 0);
	REFUSED("a pointer target past its tag", fixups[1].value = 64, fixups[1].value = 8);
	REFUSED("a reference past the tags", fixups[0].value = 2, fixups[0].value = 1);
	REFUSED("overlapping fixups", fixups[1].offset = 14, fixups[1].offset = 0);
	REFUSED("two fixups at one offset", fixups[1].offset = 16, fixups[1].offset = 0);
	REFUSED("an external tag with bytes", tags[1].kind = AE_PACK_TAG_EXTERNAL; tags[1].flags = 0;
		tags[1].resource_count = 0, tags[1].kind = AE_PACK_TAG_OWN; tags[1].flags = 3; tags[1].resource_count = 1);
	REFUSED("a weapon pickup text with %", weapon.weapon_pickup_text = "Picked up 100% rifle",
		weapon.weapon_pickup_text = "Picked up a battle rifle");
	REFUSED("an ammo pickup text with %s", weapon.ammo_pickup_text = "Picked up %s rounds",
		weapon.ammo_pickup_text = "Picked up %d rounds for battle rifle");
	REFUSED("an ammo pickup text with two %d", weapon.ammo_pickup_text = "%d of %d rounds",
		weapon.ammo_pickup_text = "Picked up %d rounds for battle rifle");
	REFUSED("a second tag of the same name", tags[1].group = AE_PACK_GROUP_WEAPON;
		tags[1].name = tags[0].name; tags[1].flags = 0,
		tags[1].group = AE_PACK_GROUP_BITMAP; tags[1].name = "ae\\packs\\check\\meter"; tags[1].flags = 3);
	REFUSED("a root that is not a weapon", weapon.root_tag = 1, weapon.root_tag = 0);
	REFUSED("no menu name", weapon.menu_name = NULL, weapon.menu_name = "BATTLE RIFLE");
	REFUSED("a bad key", weapon.key = "Battle Rifle", weapon.key = "battle_rifle");
	REFUSED("replaces random", weapon.replaces = 1, weapon.replaces = 2);
	REFUSED("a resource past 8 MiB", resource.size = AE_PACK_MAXIMUM_RESOURCE_SIZE + 1, resource.size = 100);
	REFUSED("1025 tags", data.tag_count = AE_PACK_MAXIMUM_TAGS + 1, data.tag_count = 2);
	REFUSED("a bad pack id", data.pack_id = "AE CHECK", data.pack_id = "ae_check");
	REFUSED("revision 0", data.pack_revision = 0, data.pack_revision = 3);
	REFUSED("bad UTF-8 credits", data.credits = "\xC0\xAF", data.credits = "made by the check");
#undef REFUSED

	/* a pack of tags only (no weapon entries): written with AE_PACK_FLAG_NO_WEAPONS, read back with no weapons */
	{
		struct ae_pack *tags_only = NULL;

		remove(out);
		data.weapon_count = 0;
		if (!ae_pack_write(out, &data, &problem))
		{
			printf("FAIL: a pack of tags only was not written: %s\n", problem.text);
			failures++;
		}
		else if (!ae_pack_open(out, &tags_only, &problem))
		{
			printf("FAIL: a pack of tags only was not read: %s\n", problem.text);
			failures++;
		}
		else if (!(tags_only->header.flags & AE_PACK_FLAG_NO_WEAPONS) || tags_only->weapon_count)
		{
			printf("FAIL: a pack of tags only: flags 0x%x, %u weapons\n", tags_only->header.flags,
				tags_only->weapon_count);
			failures++;
		}
		if (tags_only)
			ae_pack_close(tags_only);
		data.weapon_count = 1;
		remove(out);
	}

	/* a write that fails (its temporary file cannot be made) leaves the pack already there as it was */
	{
		unsigned int before_size, after_size;
		unsigned char *before, *after;

		if (!ae_pack_write(out, &data, &problem))
		{
			printf("FAIL: not written: %s\n", problem.text);
			return 1;
		}
		before = file_read(out, &before_size);
		mkdir(temporary, 0700);
		weapon.menu_name = "OTHER RIFLE";
		if (ae_pack_write(out, &data, &problem))
		{
			printf("FAIL: written through a directory\n");
			failures++;
		}
		weapon.menu_name = "BATTLE RIFLE";
		rmdir(temporary);
		after = file_read(out, &after_size);
		if (!before || !after || before_size != after_size || memcmp(before, after, before_size))
		{
			printf("FAIL: a failed write changed the pack already there\n");
			failures++;
		}
		free(before);
		free(after);
	}

	/* ae_pack_replace_file where a rename does not replace a file (the first move fails), and its later moves
	failing too: the new file in place, or the old one left */
	{
		static struct { char const *what; unsigned int failing; int placed; } const cases[] =
		{
			{ "a rename that does not replace", 1u << 1, 1 },
			{ "the old pack not moved aside", (1u << 1) | (1u << 2), 0 },
			{ "the new pack not put in place", (1u << 1) | (1u << 3), 0 },
		};
		char path[4096], old[4096];
		unsigned int case_index;

		snprintf(path, sizeof(path), "%s.replace", out);
		snprintf(old, sizeof(old), "%s.replace.old", out);
		for (case_index = 0; case_index < sizeof(cases) / sizeof(cases[0]); case_index++)
		{
			unsigned int size;
			unsigned char *now;
			int placed;

			file_write(path, "old pack");
			file_write(temporary, "new pack");
			move_calls = 0;
			move_failing = cases[case_index].failing;
			placed = ae_pack_replace_file(temporary, path, scripted_move);
			now = file_read(path, &size);
			if (placed != cases[case_index].placed || !now ||
				size != 8 || memcmp(now, placed ? "new pack" : "old pack", 8) || file_exists(old))
			{
				printf("FAIL: replace (%s)\n", cases[case_index].what);
				failures++;
			}
			free(now);
			remove(temporary);
			remove(path);
		}
	}

	/* the same data, the same bytes */
	{
		unsigned int first_size, second_size;
		unsigned char *first, *second;

		if (!ae_pack_write(out, &data, &problem))
		{
			printf("FAIL: not written again: %s\n", problem.text);
			return 1;
		}
		first = file_read(out, &first_size);
		if (!ae_pack_write(out, &data, &problem))
		{
			printf("FAIL: not written a third time: %s\n", problem.text);
			free(first);
			return 1;
		}
		second = file_read(out, &second_size);
		if (!first || !second || first_size != second_size || memcmp(first, second, first_size))
		{
			printf("FAIL: two writes differ\n");
			failures++;
		}
		free(first);
		free(second);
	}
	printf(failures ? "FAIL\n" : "PASS\n");
	return failures ? 1 : 0;
}

int main(int argc, char **argv)
{
	int resources = 1;
	int status = 0;
	int argument = 1;

	if (argc >= 2 && !strcmp(argv[1], "--sha256"))
		return sha256_files(argc - 2, argv + 2);
	if (argc == 4 && !strcmp(argv[1], "--rewrite"))
		return rewrite(argv[2], argv[3]);
	if (argc == 3 && !strcmp(argv[1], "--writer-check"))
		return writer_check(argv[2]);
	if (argument < argc && !strcmp(argv[argument], "--no-resources"))
	{
		resources = 0;
		argument++;
	}
	if (argument >= argc)
	{
		fprintf(stderr, "usage: ae_pack_check [--no-resources] pack...\n"
			"       ae_pack_check --sha256 file...\n"
			"       ae_pack_check --rewrite in out\n"
			"       ae_pack_check --writer-check out\n");
		return 2;
	}
	for (; argument < argc; argument++)
	{
		if (!check(argv[argument], resources))
			status = 1;
	}
	return status;
}
