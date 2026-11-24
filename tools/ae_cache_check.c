/*
AE_CACHE_CHECK.C

The AE weapon cache (ae_weapon_cache_format.h) checked as the game checks it (port/linux/game/ae_pack_format.c,
ae_cache_open): opened, every resource read, then its header, parts, sections, tags, directory, configs and externs
printed, and a line:
	<cache>: ok
	<cache>: refused: <reason>
Exits 1 if any cache was refused. tools/weapon_pack/aepack.py's cache functions print the same lines for a cache
they accept (tools/test_weapon_cache_format.py holds the two to that).

	ae_cache_check [--no-resources] cache...
	ae_cache_check --schema-hash            AE's compiled tag schema hash (what a cache is stamped with; the build
	                                        ninja linux makes links the schema in)

Built by ninja linux as build/linux/ae_cache_check, with the platform layer's POSIX flags, against the game's object of
ae_pack_format.c (as ae_pack_check). --validate (the tag validator over the placed cache) and --schema-hash arrive
with the schema linked: --schema-hash. The tag validation of a placed cache is the game's own at attach (ae_weapon_pack.c), so
there is no --validate here: tools/weapon_cache/link.py's validate step runs a headless game on the cache instead.
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
#include "ae_weapon_cache_format.h"

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

#ifdef AE_CACHE_CHECK_SCHEMA
/* ---------- what the schema's objects call (as tools/map_validate.c supplies it) */

#ifdef HALO_WINDOW_REBASE
unsigned long platform_contiguous_base = 0x80000000UL;
#endif

void *csmemset(void *buffer, long c, unsigned long size)
{
	return memset(buffer, (int)c, size);
}

void *csmemcpy(void *destination, void const *source, unsigned long size)
{
	return memcpy(destination, source, size);
}

unsigned long csstrlen(char const *s)
{
	return strlen(s);
}

char *csstrcat(char *s1, char const *s2)
{
	return strcat(s1, s2);
}

long csstrcmp(char const *s1, char const *s2)
{
	return strcmp(s1, s2);
}

void tag_validate_report(char const *message)
{
	fprintf(stderr, "%s\n", message);
}
#endif

/* ---------- printing */

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

static char const *kind_text(unsigned int kind)
{
	switch (kind)
	{
	case AE_CACHE_KIND_WEAPON:
		return "weapon";
	case AE_CACHE_KIND_TAG_SET:
		return "tag_set";
	case AE_CACHE_KIND_GRENADE:
		return "grenade";
	case AE_CACHE_KIND_BASE:
		return "base";
	default:
		return "?";
	}
}

static void hex(unsigned char const *bytes, unsigned int count)
{
	unsigned int index;

	for (index = 0; index < count; index++)
		printf("%02x", bytes[index]);
}

static void describe(struct ae_pack const *pack)
{
	struct ae_cache_info const *cache = pack->cache;
	struct ae_cache_header const *header = &cache->header;
	unsigned int index;
	char group[16], parent0[16], parent1[16], items[16], icon[32];

	printf("format %u header %u flags %u layout %u size %u\n", header->format_version, header->header_size,
		header->flags, header->tag_layout, pack->file_size);
	printf("serial %u registry %u version \"%s\" schema %08x%08x parts %u\n", header->cache_serial,
		header->registry_revision, header->ae_version, header->schema_hash[1], header->schema_hash[0], header->part_count);
	printf("sha256 ");
	hex(header->sha256, AE_PACK_SHA256_SIZE);
	printf(" id ");
	hex(header->sha256, AE_CACHE_ID_SIZE);
	printf("\n");
	for (index = 0; index < header->part_count; index++)
	{
		printf("part %u name %s flags %u size %u sha256 ", index, header->parts[index].name, header->parts[index].flags,
			header->parts[index].size[0]);
		hex(header->parts[index].sha256, AE_PACK_SHA256_SIZE);
		printf("\n");
	}
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
	for (index = 0; index < cache->directory_count; index++)
	{
		struct ae_cache_directory_record const *entry = &cache->directory[index];

		printf("entry %u id 0x%08x %s part %u flags %u key %s display \"%s\" tags %u from %u", index, entry->id,
			kind_text(entry->kind), (unsigned int)entry->part, (unsigned int)entry->flags, text(pack, entry->key),
			text(pack, entry->display), entry->tag_count, entry->first_tag);
		if (entry->kind == AE_CACHE_KIND_WEAPON)
		{
			if (entry->item_collection_tag == AE_PACK_NONE)
				strcpy(items, "NONE");
			else
				sprintf(items, "%u", entry->item_collection_tag);
			if (entry->icon_bitmap_tag == AE_PACK_NONE)
				strcpy(icon, "NONE");
			else
				sprintf(icon, "%u:%u", entry->icon_bitmap_tag, entry->icon_sequence);
			printf(" root %u items %s behaviour %u label %s fallbacks %s,%s menu \"%s\" pickup \"%s\" ammo \"%s\" icon %s "
				"stock %u replaces_map %u legacy %u", entry->root_tag, items, (unsigned int)entry->behaviour,
				text(pack, entry->label), text(pack, entry->fallback_labels[0]), text(pack, entry->fallback_labels[1]),
				text(pack, entry->menu_name), text(pack, entry->weapon_pickup_text), text(pack, entry->ammo_pickup_text),
				icon, (unsigned int)entry->stock_fallback, (unsigned int)entry->replaces_map_own,
				(unsigned int)entry->legacy_slot);
		}
		else if (entry->kind == AE_CACHE_KIND_GRENADE)
		{
			printf(" catalogue %u projectile %d equipment %d collection %d hud %d field %d",
				(unsigned int)entry->grenade_catalogue_id, (int)entry->projectile_tag, (int)entry->equipment_tag,
				(int)entry->collection_tag, (int)entry->hud_tag, (int)entry->field_effect_tag);
		}
		else if (entry->kind == AE_CACHE_KIND_TAG_SET)
		{
			printf(" roots %u from %u", (unsigned int)entry->root_list_count, (unsigned int)entry->root_list_first);
		}
		printf("\n");
	}
	for (index = 0; index < cache->config_count; index++)
	{
		struct ae_cache_config_record const *config = &cache->configs[index];

		printf("config %u id 0x%08x weapon 0x%08x key %s mask 0x%x rounds %u,%u,%u rate %g,%g error %g,%g zoom %u,%g "
			"autoaim %g,%g magnetism %g,%g behaviour %u hud %d menu \"%s\"\n", index, config->config_id,
			config->weapon_id, text(pack, config->key), config->override_mask, config->rounds_loaded_maximum,
			config->rounds_total_initial, config->rounds_total_maximum, (double)config->rate_of_fire_minimum,
			(double)config->rate_of_fire_maximum, (double)config->error_angle_minimum,
			(double)config->error_angle_maximum, config->zoom_level_count, (double)config->magnification_maximum,
			(double)config->autoaim_angle, (double)config->autoaim_range, (double)config->magnetism_angle,
			(double)config->magnetism_range, config->behaviour, (int)config->hud_interface_tag,
			text(pack, config->menu_name));
	}
	for (index = 0; index < cache->extern_count; index++)
	{
		struct ae_cache_extern_record const *external = &cache->externs[index];

		printf("extern %u %s required %u entries %u from %u name \"%s\"\n", index, group_text(external->group, group),
			external->required, external->entry_count, external->entry_first, text(pack, external->name));
	}
	(void)parent0;
	printf("credits %u bytes\n", pack->credits_size);
	printf("resources %u of %u bytes\n", pack->resource_count, pack->resources_size);
}

static int check(char const *path, int resources)
{
	struct ae_pack *pack;
	struct ae_pack_problem problem;

	printf("cache %s\n", path);
	if (!ae_cache_open(path, &pack, &problem) || (resources && !ae_pack_verify_resources(pack, &problem)))
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

#ifdef AE_CACHE_CHECK_SCHEMA
/* port/linux/game/tag_schema_hash.c, in the game's ABI */
void tag_schema_hash(unsigned int *low, unsigned int *high);
void tag_schema_group_hashes(
	void (*report)(unsigned int group_tag, char const *definition_name, unsigned int low, unsigned int high));

static void group_line(unsigned int group_tag, char const *name, unsigned int low, unsigned int high)
{
	printf("schema group %c%c%c%c %s %08x%08x\n", (int)(group_tag >> 24) & 0xFF, (int)(group_tag >> 16) & 0xFF,
		(int)(group_tag >> 8) & 0xFF, (int)group_tag & 0xFF, name, high, low);
}
#endif

int main(int argc, char **argv)
{
	int resources = 1;
	int status = 0;
	int argument = 1;

#ifdef AE_CACHE_CHECK_SCHEMA
	if (argc == 2 && !strcmp(argv[1], "--schema-groups"))
	{
		tag_schema_group_hashes(group_line);
		return 0;
	}
	if (argc == 2 && !strcmp(argv[1], "--schema-hash"))
	{
		unsigned int low, high;

		tag_schema_hash(&low, &high);
		printf("schema %08x%08x\n", high, low);
		return 0;
	}
#endif
	if (argument < argc && !strcmp(argv[argument], "--no-resources"))
	{
		resources = 0;
		argument++;
	}
	if (argument >= argc)
	{
		fprintf(stderr, "usage: ae_cache_check [--no-resources] cache...\n");
		return 2;
	}
	for (; argument < argc; argument++)
	{
		if (!check(argv[argument], resources))
			status = 1;
	}
	return status;
}
