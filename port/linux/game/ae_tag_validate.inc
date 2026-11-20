/*
AE_TAG_VALIDATE.INC

port: weapon pack. Tags ADDED to a loaded map's (a weapon pack's own tags:
ae_weapon_pack.c) checked exactly as the map's own were, by the same walk,
before the game reads them.

This file is included by tag_validate.c at its end, ONCE, and is part of
it (hence .inc, not .h: it defines functions): it uses that file's walk,
claims and messages as they are, and changes none of them. Other files
declare the two functions they call.

The map's tags have been checked (tag_validate_tags) and are in use. The
added tags lie in a REGION of their own, outside the tag cache, laid out as
a map's tags are:
- at its first byte a tag header (signature 'tags', no vertex or index
  buffers) whose table is the whole table the game will use: the map's
  tags' entries as they are, then the added tags';
- the added tags' names, roots, blocks and data, and the names their
  references carry, all inside the region.
Every added tag goes through its group's schema as a map's tag does: each
block and data must lie in the region and overlap no other's, each tag
reference must name a tag of the table (the map's or an added one) of the
right group, counts, indices and enums are cut or corrected, runtime values
reset, then the checks run. What an added tag may point at is the region
only: a pointer into the map's own tags is refused, as one outside the tags
is for a map.

The validator's state is the map's while this runs elsewhere: it is put
aside here and put back before returning (its header, sizes and name, which
a structure bsp's check reads later; the claims of the region's size, kept
in the caller's scratch; the extents, for which the structure bsps' table
is borrowed: the next bsp to load makes it new).
*/

/* the added tags (the table's entries from first_index on) of the tags at region (region_size bytes, a tag header
at its first byte): TRUE if the game may use them; corrections are made in place and counted. name is what the
messages call them; file_length what data in the "map file" must lie within; claims_scratch is
ae_tag_validate_claim_words(region_size) words the caller owns */
long ae_tag_validate_claim_words(
	long region_size)
{
	return region_size > 0 ? (region_size + CLAIM_BITS - 1) / CLAIM_BITS : 0;
}

boolean ae_tag_validate_added_tags(
	void *region,
	long region_size,
	long first_index,
	long file_length,
	char const *name,
	unsigned long *claims_scratch,
	long *corrections)
{
	struct tag_validate_header *header = region;
	struct tag_validation validation;
	long claim_words = ae_tag_validate_claim_words(region_size);
	long absolute_index;
	boolean shared_bytes_allowed = tag_validate_shared_bytes_allowed;
	/* (the map's state put aside; static: it is large for a stack, and one thread loads maps) */
	static __typeof__(tag_validate_globals) map_globals;
	__typeof__(extent_table) map_extent_table;

	if (corrections)
		*corrections = 0;
	/* (only beside a map whose tags were checked, and within what the claims hold) */
	if (!tag_validate_globals.header || !region || !claims_scratch || !name ||
		region_size < (long)sizeof(*header) || region_size > TAG_CACHE_SIZE)
	{
		return FALSE;
	}
#ifdef HALO_64BIT
	if (!tag_validate_empty_name)
		return FALSE;
#endif

	memcpy(&map_globals, &tag_validate_globals, sizeof(map_globals));
	memcpy(&map_extent_table, &extent_table, sizeof(map_extent_table));
	memcpy(claims_scratch, tag_validate_claims, (size_t)claim_words * sizeof(tag_validate_claims[0]));
	memset(tag_validate_claims, 0, (size_t)claim_words * sizeof(tag_validate_claims[0]));

	memset(&tag_validate_globals, 0, sizeof(tag_validate_globals));
	tag_validate_globals.header = header;
	tag_validate_globals.tag_data_size = region_size;
	tag_validate_globals.file_length = file_length;
	snprintf(tag_validate_globals.map_name, sizeof(tag_validate_globals.map_name), "%s", name);
	/* (added tags share no bytes, whatever the map may) */
	tag_validate_shared_bytes_allowed = FALSE;
	extent_table_new(tag_validate_structure_bsp_extents, MAXIMUM_STRUCTURE_BSP_EXTENTS);
	validation_new(&validation, region, (unsigned long)region_size);

	if (!schemas_fit())
	{
		tag_validate_refuse(&validation, "cannot be checked: a tag schema is wrong");
	}
	else if (header->signature != TAG_HEADER_SIGNATURE || header->vertex_buffer_count || header->index_buffer_count ||
		header->tag_count <= 0 || header->tag_count > UNSIGNED_SHORT_MAX ||
		first_index <= 0 || first_index >= header->tag_count ||
		!region_contains(&validation, instances_get(header),
			(unsigned long)header->tag_count * sizeof(struct tag_validate_instance)) ||
		!claim(header, sizeof(*header)) ||
		!claim(instances_get(header), (unsigned long)header->tag_count * sizeof(struct tag_validate_instance)))
	{
		tag_validate_refuse(&validation, "has a damaged tag header");
	}

	/* the added tags' entries: each one's index, name, groups and root (as tag_validate_tags has a map's, but
	an added tag is of a group with a schema, and never a structure bsp) */
	for (absolute_index = first_index; !validation.refused && absolute_index < header->tag_count; absolute_index++)
	{
		struct tag_validate_instance *instance = &instances_get(header)[absolute_index];
		struct tag_schema_group const *group = schema_group_get(instance->group_tag);
		boolean shared;

		validation.tag_index = instance->tag_index;
		if ((long)((unsigned long)instance->tag_index & 0xffff) != absolute_index || instance->tag_index == NONE)
		{
			tag_validate_refuse(&validation, "has tag %08lx in the table's place %ld", instance->tag_index,
				absolute_index);
			break;
		}
		if (!region_contains(&validation, xbox_pointer(instance->name), 1) ||
			!string_valid(&validation, xbox_pointer(instance->name)))
		{
			tag_validate_refuse(&validation, "has no name");
			break;
		}
		if (!group || !group->definition || instance->group_tag == STRUCTURE_BSP_GROUP_TAG)
		{
			tag_validate_refuse(&validation, "is of a group no tag is added of");
			break;
		}
		instance->parent_group_tags[0] = group->parent_group_tags[0];
		instance->parent_group_tags[1] = group->parent_group_tags[1];
		if (!instance->base_address ||
			!region_contains(&validation, xbox_pointer(instance->base_address), group->definition->size) ||
			!extent_claim(xbox_pointer(instance->base_address), group->definition->size, group, _extent_root,
				instance, &shared))
		{
			tag_validate_refuse(&validation, "has its data at %08lx, outside the tags or overlapping another's",
				(unsigned long)instance->base_address);
			break;
		}
	}

	/* every added tag through its schema, then every added tag's checks */
	for (absolute_index = first_index; !validation.refused && absolute_index < header->tag_count; absolute_index++)
		validate_instance(&validation, &instances_get(header)[absolute_index], _pass_values);
	for (absolute_index = first_index; !validation.refused && absolute_index < header->tag_count; absolute_index++)
		validate_instance(&validation, &instances_get(header)[absolute_index], _pass_checks);
	if (!shared_bytes_unchanged() && !validation.refused)
	{
		validation.tag_index = NONE;
		tag_validate_refuse(&validation, "has bytes that tags of different kinds share, which its check changed");
	}
	if (corrections)
		*corrections = validation.corrections;

	/* the map's state back */
	memcpy(tag_validate_claims, claims_scratch, (size_t)claim_words * sizeof(tag_validate_claims[0]));
	memcpy(&extent_table, &map_extent_table, sizeof(extent_table));
	memcpy(&tag_validate_globals, &map_globals, sizeof(tag_validate_globals));
	tag_validate_shared_bytes_allowed = shared_bytes_allowed;

	return !validation.refused;
}

/* a Custom Edition map's tags (or Halo PC's, HaloMD's), which tag_validate_tags never checks (they are Halo PC's
layout, checked as they load: ce_map_checks.c), made the tags last checked: the validator's state is this map's,
not the map checked before it (the menus' ui.map, or none at all on a dedicated server that plays only such maps),
so the tags a pack adds to it are checked beside it as beside an Xbox map's. Its structure bsps are never checked
here (scenario_load_structure_bsp skips them on such a map), and the next map checked starts anew */
void ae_tag_validate_custom_edition_map(
	void *tag_header,
	long tag_data_size,
	long file_length,
	char const *map_name)
{
	memset(&tag_validate_globals, 0, sizeof(tag_validate_globals));
	if (!tag_header || tag_data_size <= 0 || !map_name)
		return;
#ifdef HALO_64BIT
	if (!tag_validate_empty_name)
		tag_validate_empty_name = malloc(1);
	if (!tag_validate_empty_name)
		return;
	tag_validate_empty_name[0] = 0;
#endif
	tag_validate_globals.header = tag_header;
	tag_validate_globals.tag_data_size = tag_data_size;
	tag_validate_globals.file_length = file_length;
	snprintf(tag_validate_globals.map_name, sizeof(tag_validate_globals.map_name), "%s", map_name);
}
