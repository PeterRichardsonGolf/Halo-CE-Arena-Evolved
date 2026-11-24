/*
AE_BOTS_NAV_DUMP.INC

Arena Evolved: DEV "nav dump" (ae_bot_nav.h), the loaded map's ground for the nav tools (tools/halo_mcp/nav.py): every
collision surface of the structure (corners, normal, walkable as the bots' searches see it, piece, material), scenery and
machines with collision, vehicles, netgame flags, items, the bots' links (1.0 rc2) and spawns, as JSON. #included at the
end of ae_bots_travel.inc (bots.c's surfaces and pieces); ae_bot_nav.c writes the text to the config folder (local only:
the map's geometry).
*/

#include <stdarg.h>

struct ae_nav_text
{
	char *text;
	long length, size;
	boolean failed;
};

static void ae_nav_text_add(struct ae_nav_text *out, char const *format, ...)
{
	va_list arguments;
	int wrote;

	if (out->failed)
		return;
	for (;;)
	{
		long room = out->size - out->length;

		if (room > 256)
		{
			va_start(arguments, format);
			wrote = vsnprintf(out->text + out->length, (size_t)room, format, arguments);
			va_end(arguments);
			if (wrote >= 0 && wrote < room)
			{
				out->length += wrote;
				return;
			}
		}
		{
			long size = out->size ? out->size * 2 : 1L << 20;
			/* (malloc, copy and free, not realloc: bots.c's allocator may be the game's own, ae_mcc_address.h) */
			char *grown = size > (64L << 20) ? NULL : (char *)malloc((size_t)size);

			if (!grown)
			{
				out->failed = TRUE;
				return;
			}
			if (out->text)
			{
				csmemcpy(grown, out->text, (size_t)out->length);
				free(out->text);
			}
			out->text = grown;
			out->size = size;
		}
	}
}

/* a tag path as a JSON string's text: backslashes as slashes, quotes and control bytes left out */
static void ae_nav_text_name(struct ae_nav_text *out, char const *name)
{
	char clean[256];
	long index = 0;

	for (; name && *name && index < (long)sizeof(clean) - 1; name++)
	{
		char c = *name == '\\' ? '/' : *name;

		if (c == '"' || (unsigned char)c < 0x20)
			continue;
		clean[index++] = c;
	}
	clean[index] = 0;
	ae_nav_text_add(out, "\"%s\"", clean);
}

static char const *ae_nav_flag_type(short type)
{
	switch (type)
	{
	case _netgame_flag_ctf_flag: return "ctf_flag";
	case _netgame_flag_ctf_vehicle: return "ctf_vehicle";
	case _netgame_flag_oddball_ball_spawn: return "oddball";
	case _netgame_flag_race_track: return "race_track";
	case _netgame_flag_race_vehicle: return "race_vehicle";
	case _netgame_flag_teleporter_source: return "teleport_from";
	case _netgame_flag_teleporter_target: return "teleport_to";
	case _netgame_flag_hill: return "hill";
	default: return "other";
	}
}

/* the dump's text given back (freed by the allocator that made it) */
void ae_bots_nav_dump_free(char *text)
{
	if (text)
		free(text);
}

char *ae_bots_nav_dump_text(char const *level, char const *file, char const *hash, long *length, char *why, long why_size)
{
	struct scenario *scenario = global_scenario_get();
	struct collision_bsp const *bsp;
	struct ae_nav_text out;
	struct object_iterator iterator;
	real_point3d corners[BOTS_SURFACE_CORNERS];
	real low[3] = { 1e9f, 1e9f, 1e9f }, high[3] = { -1e9f, -1e9f, -1e9f };
	long surface_index, corner_count, corner, index, walkable = 0, first;

	csmemset(&out, 0, sizeof(out));
	*length = 0;
	if (!scenario || !bots_paths_copy() || !bots_paths.walkable)
	{
		snprintf(why, (size_t)why_size, "no map with walkable surfaces loaded");
		return NULL;
	}
	bsp = TAG_BLOCK_GET_ELEMENT(&bots_paths.structure.collision_bsp, 0, struct collision_bsp);
	/* (the pieces made, when the walkable scan is done) */
	(void)ae_travel_piece(0);
	ae_nav_text_add(&out, "{\n \"version\": 1, \"file\": \"%s\", \"level\": ", file);
	ae_nav_text_name(&out, level);
	if (hash && hash[0])
		ae_nav_text_add(&out, ", \"hash\": \"%s\",\n", hash);
	else
		ae_nav_text_add(&out, ", \"hash\": null,\n");
	ae_nav_text_add(&out, " \"surfaces\": [");
	for (surface_index = 0, first = 1; surface_index < bsp->surfaces.count; surface_index++)
	{
		struct collision_surface const *surface =
			TAG_BLOCK_GET_ELEMENT(&bsp->surfaces, surface_index, struct collision_surface);
		real normal[3] = { 0.0f, 0.0f, 0.0f }, size;
		boolean walk;
		long piece = NONE;

		corner_count = bots_surface_corners(bsp, surface_index, corners);
		if (corner_count < 3)
			continue;
		/* (the normal by Newell's sum: the corners' winding, no plane needed) */
		for (corner = 0; corner < corner_count; corner++)
		{
			real_point3d const *a = &corners[corner], *b = &corners[(corner + 1) % corner_count];

			normal[0] += (a->y - b->y) * (a->z + b->z);
			normal[1] += (a->z - b->z) * (a->x + b->x);
			normal[2] += (a->x - b->x) * (a->y + b->y);
			low[0] = MIN(low[0], a->x); low[1] = MIN(low[1], a->y); low[2] = MIN(low[2], a->z);
			high[0] = MAX(high[0], a->x); high[1] = MAX(high[1], a->y); high[2] = MAX(high[2], a->z);
		}
		size = (real)sqrt(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]);
		if (size > 0.0f)
		{
			normal[0] /= size;
			normal[1] /= size;
			normal[2] /= size;
		}
		walk = (boolean)(surface_index < bots_paths.structure.pathfinding_surfaces.count &&
			TEST_FLAG(bots_paths.walkable[surface_index], BOTS_PATHFINDING_WALKABLE_BIT));
		if (walk)
		{
			walkable++;
			piece = ae_travel_piece(surface_index);
		}
		ae_nav_text_add(&out, "%s\n  {\"i\": %ld, \"v\": [", first ? "" : ",", surface_index);
		for (corner = 0; corner < corner_count; corner++)
			ae_nav_text_add(&out, "%s[%.3f, %.3f, %.3f]", corner ? ", " : "", corners[corner].x, corners[corner].y,
				corners[corner].z);
		ae_nav_text_add(&out, "], \"n\": [%.4f, %.4f, %.4f], \"walk\": %s, \"piece\": %ld, \"mat\": %d}", normal[0],
			normal[1], normal[2], walk ? "true" : "false", piece == NONE ? -1L : piece, (int)surface->material_index);
		first = 0;
	}
	ae_nav_text_add(&out, "\n ],\n \"bounds\": [[%.3f, %.3f, %.3f], [%.3f, %.3f, %.3f]],\n", low[0], low[1], low[2],
		high[0], high[1], high[2]);
	/* (the pieces' sizes in surfaces) */
	ae_nav_text_add(&out, " \"pieces\": {");
	if (ae_travel_pieces.piece && ae_travel_pieces.size)
	{
		for (index = 0, first = 1; index < ae_travel_pieces.count; index++)
		{
			if (ae_travel_pieces.piece[index] != index || ae_travel_pieces.size[index] <= 0)
				continue;
			ae_nav_text_add(&out, "%s\"%ld\": %ld", first ? "" : ", ", index, ae_travel_pieces.size[index]);
			first = 0;
		}
	}
	ae_nav_text_add(&out, "},\n \"objects\": [");
	object_iterator_new(&iterator, _object_mask_scenery | _object_mask_machine, 0);
	first = 1;
	while (object_iterator_next(&iterator))
	{
		struct object_datum *object = object_get(iterator.index);
		struct object_definition *definition = object_definition_get(object->definition_index);

		if (!definition || definition->object.collision_model.index == NONE)
			continue;
		ae_nav_text_add(&out, "%s\n  {\"kind\": \"%s\", \"name\": ", first ? "" : ",",
			object->object.type == _object_type_machine ? "machine" : "scenery");
		ae_nav_text_name(&out, tag_get_name(object->definition_index));
		ae_nav_text_add(&out, ", \"p\": [%.3f, %.3f, %.3f], \"r\": %.3f}", object->object.bounding_sphere_center.x,
			object->object.bounding_sphere_center.y, object->object.bounding_sphere_center.z,
			ae_bots_obstacle_radius(object->object.bounding_sphere_radius));
		first = 0;
	}
	/* (the circles the vehicles' routes and dodge use, as the game made them: each scenery object's measured footprint, the
	nav file's keepouts, the circles learnt from earlier games' hits; a keepout's heights are every height) */
	/* (a map loaded with no bots has not made them yet: made now, as a game with bots makes them) */
	if (ae_path_cost_obstacle_count() == 0)
		ae_bots_obstacles_collect();
	ae_nav_text_add(&out, "\n ],\n \"obstacles\": [");
	for (index = 0, first = 1; index < ae_path_cost_obstacle_count(); index++)
	{
		float x, y, radius, zlo, zhi;
		long object_index;
		struct object_datum *object;
		boolean keepout;

		if (!ae_path_cost_obstacle_get((int)index, &x, &y, &radius) ||
			!ae_path_cost_obstacle_span((int)index, &zlo, &zhi, &object_index))
			continue;
		object = object_index != NONE ? object_try_and_get(object_index) : NULL;
		keepout = zlo < -1e20f;
		ae_nav_text_add(&out, "%s\n  {\"name\": ", first ? "" : ",");
		if (object)
			ae_nav_text_name(&out, tag_get_name(object->definition_index));
		else
			ae_nav_text_add(&out, "\"%s\"", keepout ? "keepout" : "learnt");
		if (keepout)
			ae_nav_text_add(&out, ", \"source\": \"keepout\", \"p\": [%.3f, %.3f, 0.0], \"r\": %.3f}", x, y, radius);
		else
			ae_nav_text_add(&out, ", \"source\": \"%s\", \"p\": [%.3f, %.3f, %.3f], \"r\": %.3f, \"zlo\": %.3f, "
				"\"zhi\": %.3f}", object ? "measured" : "learnt", x, y, 0.5f * (zlo + zhi), radius, zlo, zhi);
		first = 0;
	}
	ae_nav_text_add(&out, "\n ],\n \"vehicles\": [");
	object_iterator_new(&iterator, _object_mask_vehicle, 0);
	first = 1;
	while (object_iterator_next(&iterator))
	{
		struct object_datum *object = object_get(iterator.index);
		int kind = ae_bot_nav_kind_of_class(ae_bots_vehicle_class(iterator.index));

		ae_nav_text_add(&out, "%s\n  {\"name\": ", first ? "" : ",");
		ae_nav_text_name(&out, tag_get_name(object->definition_index));
		ae_nav_text_add(&out, ", \"class\": \"%s\", \"p\": [%.3f, %.3f, %.3f], \"yaw\": %.1f}",
			kind == AE_BOT_NAV_WARTHOG ? "warthog" : kind == AE_BOT_NAV_GHOST ? "ghost" :
			kind == AE_BOT_NAV_SCORPION ? "scorpion" : "other", object->object.position.x, object->object.position.y,
			object->object.position.z, atan2(object->object.forward.j, object->object.forward.i) / BOTS_DEGREES);
		first = 0;
	}
	ae_nav_text_add(&out, "\n ],\n \"flags\": [");
	for (index = 0; index < scenario->netgame_flags.count; index++)
	{
		struct scenario_netgame_flag *flag = TAG_BLOCK_GET_ELEMENT(&scenario->netgame_flags, index, struct scenario_netgame_flag);

		ae_nav_text_add(&out, "%s\n  {\"type\": \"%s\", \"team\": %d, \"p\": [%.3f, %.3f, %.3f]}", index ? "," : "",
			ae_nav_flag_type(flag->type), (int)flag->team_index, flag->position.x, flag->position.y, flag->position.z);
	}
	ae_nav_text_add(&out, "\n ],\n \"items\": [");
	for (index = 0; index < scenario->netgame_equipment.count; index++)
	{
		struct scenario_netgame_equipment *item =
			TAG_BLOCK_GET_ELEMENT(&scenario->netgame_equipment, index, struct scenario_netgame_equipment);

		ae_nav_text_add(&out, "%s\n  {\"name\": ", index ? "," : "");
		ae_nav_text_name(&out, item->item_collection.index != NONE ? tag_get_name(item->item_collection.index) : "");
		ae_nav_text_add(&out, ", \"p\": [%.3f, %.3f, %.3f]}", item->position.x, item->position.y, item->position.z);
	}
	/* (1.0 rc2: the bots' links as the game found or was given them: kind, entry and exit, the pieces they join) */
	ae_nav_text_add(&out, "\n ],\n \"links\": [");
	for (index = 0; index < ae_travel.link_count; index++)
	{
		struct ae_bots_travel_link const *link = &ae_travel.links[index];
		long from = ae_travel_link_piece(index, FALSE), to = ae_travel_link_piece(index, TRUE);
		int kind = link->kind;

		ae_nav_text_add(&out, "%s\n  {\"i\": %ld, \"kind\": \"%s\", \"entry\": [%.3f, %.3f, %.3f], \"exit\": [%.3f, %.3f, %.3f], "
			"\"from\": %ld, \"to\": %ld}", index ? "," : "", index, kind == AE_BOTS_TRAVEL_LINK_TELEPORTER ? "teleporter" :
			kind == AE_BOTS_TRAVEL_LINK_JUMP ? "jump" : kind == AE_BOTS_TRAVEL_LINK_WALK ? "walk" : "ladder", link->entry.x,
			link->entry.y, link->entry.z, link->exit.x, link->exit.y, link->exit.z, from == NONE ? -1L : from,
			to == NONE ? -1L : to);
	}
	ae_nav_text_add(&out, "\n ],\n \"spawns\": [");
	for (index = 0; index < scenario->players.count; index++)
	{
		struct player_starting_location *spawn =
			TAG_BLOCK_GET_ELEMENT(&scenario->players, index, struct player_starting_location);

		ae_nav_text_add(&out, "%s\n  {\"team\": %d, \"p\": [%.3f, %.3f, %.3f], \"yaw\": %.1f}", index ? "," : "",
			(int)spawn->team_index, spawn->position.x, spawn->position.y, spawn->position.z, spawn->facing / BOTS_DEGREES);
	}
	ae_nav_text_add(&out, "\n ]\n}\n");
	if (out.failed)
	{
		if (out.text)
			free(out.text);
		snprintf(why, (size_t)why_size, "out of memory");
		return NULL;
	}
	*length = out.length;
	snprintf(why, (size_t)why_size, "%ld surfaces (%ld walkable)", bsp->surfaces.count, walkable);
	return out.text;
}
