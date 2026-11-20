/*
AE_BLOOD_DECALS.INC

(Arena Evolved) dynamic blood's decals of one quad (ae_blood.h), #included at the end of source/effects/decals.c (one
AE hook) as they reach its private decal list and vertices.

Halo's decals are clipped to the level's surfaces where they land, and stay. These are single quads blood places (the
streak of a drip running down a wall, the bead at its head: ae_blood.c) and moves after (as it runs); otherwise
ordinary decals, drawn in their cluster's list, fading and going as their definition says.
*/

/* a decal blood lays and deletes itself is never one of the few the game keeps locked against recycling, and one
deleted is no longer counted locked: else the locked count runs up past what is there, until no decal can be made at
all ("failed to unlock decals during insert") */
static void ae_blood_decal_unlock(
	long decal_index)
{
	struct decal_datum *decal;

	if (decal_index == NONE || !global_decal_data->valid)
		return;
	decal = (struct decal_datum *)datum_try_and_get(global_decal_data, decal_index);
	if (decal && TEST_FLAG(decal->flags, _decal_locked_bit))
	{
		SET_FLAG(decal->flags, _decal_locked_bit, FALSE);
		if (decal_globals->locked_count > 0)
			decal_globals->locked_count--;
	}
}

real ae_blood_texture_core_along = 1.f;
real ae_blood_texture_core_across = 1.f;
int ae_blood_full_intensity = FALSE;

long ae_blood_decal_new_quad(
	long decal_definition_index,
	real_point3d const *point,
	real_vector3d const *normal,
	real_vector3d const *axis,
	real half_length,
	real half_width,
	short cluster_index,
	real_point3d corners[4])
{
	struct decal_definition *definition;
	struct bitmap_group *bitmap_group;
	real_rectangle2d sprite_bounds = { 0.f, 1.f, 0.f, 1.f };
	real_rectangle2d extent;
	real_vector3d forward;
	real_vector3d left;
	short sequence_index;
	short bitmap_index = 0;
	long cache_index;
	long decal_index;
	struct decal_quad *quad;
	short corner;

	if (!decals_enabled || decal_definition_index == NONE || !global_decal_data->valid ||
		cluster_index < 0 || cluster_index >= MAXIMUM_CLUSTERS_PER_STRUCTURE)
	{
		return NONE;
	}
	definition = decal_definition_get(decal_definition_index);
	if (!definition || definition->shader.map.index == NONE)
		return NONE;
	bitmap_group = bitmap_group_get(definition->shader.map.index);
	if (!bitmap_group || bitmap_group->bitmaps.count <= 0 ||
		definition->layer < 0 || definition->layer >= NUMBER_OF_DECAL_LAYERS)
	{
		return NONE;
	}

	/* its frame: up the normal, forward along axis (or any way) */
	if (axis)
	{
		real along = dot_product3d(axis, normal);

		forward.i = axis->i - normal->i * along;
		forward.j = axis->j - normal->j * along;
		forward.k = axis->k - normal->k * along;
	}
	if (!axis || normalize3d(&forward) == 0.f)
	{
		real angle = real_local_random_range(0.f, 2.f * _pi);
		real_vector3d tangent;
		real_vector3d bitangent;

		perpendicular3d(normal, &tangent);
		normalize3d(&tangent);
		cross_product3d(normal, &tangent, &bitangent);
		forward.i = tangent.i * (real)cos(angle) + bitangent.i * (real)sin(angle);
		forward.j = tangent.j * (real)cos(angle) + bitangent.j * (real)sin(angle);
		forward.k = tangent.k * (real)cos(angle) + bitangent.k * (real)sin(angle);
	}
	cross_product3d(normal, &forward, &left);
	normalize3d(&left);

	/* its picture: a sequence at random, a sprite's bounds */
	sequence_index = bitmap_group->sequences.count > 0 ?
		(short)PIN(local_random_range(0, (short)bitmap_group->sequences.count), 0, bitmap_group->sequences.count - 1) : 0;
	extent.x0 = -half_length;
	extent.x1 = half_length;
	extent.y0 = -half_width;
	extent.y1 = half_width;
	if (bitmap_group->type == _bitmap_group_type_sprites && bitmap_group->sequences.count > 0)
	{
		struct bitmap_group_sequence *sequence = TAG_BLOCK_GET_ELEMENT(
			&bitmap_group->sequences, sequence_index, struct bitmap_group_sequence);

		if (sequence->sprites.count <= 0)
			return NONE;
		bitmap_index = TAG_BLOCK_GET_ELEMENT(&sequence->sprites, 0, struct bitmap_group_sprite)->bitmap_index;
		sprite_bounds = TAG_BLOCK_GET_ELEMENT(&sequence->sprites, 0, struct bitmap_group_sprite)->bounds;
	}

	cache_index = rasterizer_decal_vertices_new(sizeof(struct decal_quad));
	if (cache_index == NONE)
		return NONE;
	decal_index = decal_insert(cache_index, cluster_index, definition->layer, NONE, FALSE);
	if (decal_index == NONE)
	{
		rasterizer_decal_vertices_delete(cache_index);
		return NONE;
	}
	ae_blood_decal_unlock(decal_index);
	quad = rasterizer_decal_vertices_lock((short)cache_index, sizeof(struct decal_quad));
	if (!quad)
	{
		decal_delete(decal_index);
		rasterizer_decal_vertices_delete(cache_index);
		return NONE;
	}

	for (corner = 0; corner < 4; corner++)
	{
		/* (round the quad clockwise seen from in front, 0 0, 0 1, 1 1, 1 0: decals are drawn with counter-clockwise
		triangles culled) */
		real u = (corner >= 2) ? 1.f : 0.f;
		real v = (corner == 1 || corner == 2) ? 1.f : 0.f;
		real x = extent.x0 + (extent.x1 - extent.x0) * u;
		real y = extent.y0 + (extent.y1 - extent.y0) * v;
		real core_u = PIN(ae_blood_texture_core_along, 0.01f, 1.f);
		real core_v = PIN(ae_blood_texture_core_across, 0.01f, 1.f);
		real tu = PIN(sprite_bounds.x0 + (sprite_bounds.x1 - sprite_bounds.x0) * (0.5f + (u - 0.5f) * core_u), 0.f, 1.f);
		real tv = PIN(sprite_bounds.y0 + (sprite_bounds.y1 - sprite_bounds.y0) * (0.5f + (v - 0.5f) * core_v), 0.f, 1.f);

		corners[corner].x = point->x + forward.i * x + left.i * y;
		corners[corner].y = point->y + forward.j * x + left.j * y;
		corners[corner].z = point->z + forward.k * x + left.k * y;
		quad->vertices[corner].position = corners[corner];
		quad->vertices[corner].texcoord[0] = (short)PIN((long)(tu * 32768.f + 0.5f), 0, 32767);
		quad->vertices[corner].texcoord[1] = (short)PIN((long)(tv * 32768.f + 0.5f), 0, 32767);
	}
	rasterizer_decal_vertices_unlock();

	{
		struct decal_datum *decal = DECAL_GET(decal_index);
		real intensity = ae_blood_full_intensity ?
			MAX(definition->intensity_lower_bound, definition->intensity_upper_bound) :
			real_local_random_range(definition->intensity_lower_bound, definition->intensity_upper_bound);
		real_rgb_color color;

		rgb_colors_interpolate(
			&color,
			(definition->flags >> _decal_definition_color_interpolate_in_hsv_bit) & 3,
			&definition->color_lower_bound,
			&definition->color_upper_bound,
			real_local_random_range(0.f, 1.f));
		decal->position = *point;
		decal->creation_time = game_time_get();
		decal->sequence_index = (byte)sequence_index;
		decal->sprite_index = 0;
		decal->bitmap_index = (byte)bitmap_index;
		decal->lifetime = real_local_random_range(definition->lifetime_lower_bound, definition->lifetime_upper_bound);
		decal->decay_time = real_local_random_range(definition->decay_time_lower_bound, definition->decay_time_upper_bound);
		decal->definition_index = decal_definition_index;
		decal->quad_count = 1;
		decal->color = real_a_rgb_color_to_pixel32(PIN(intensity, 0.f, 1.f), &color);
		decal->intensity = 255;
	}
	return decal_index;
}

int ae_blood_decal_move_quad(
	long decal_index,
	long decal_definition_index,
	real_point3d const corners[4],
	short cluster_index)
{
	struct decal_datum *decal;
	struct decal_quad *quad;
	short corner;

	if (decal_index == NONE || !global_decal_data->valid)
		return FALSE;
	decal = (struct decal_datum *)datum_try_and_get(global_decal_data, decal_index);
	/* (gone: faded, or its vertices taken for newer decals) */
	if (!decal || decal->definition_index != decal_definition_index || decal->quad_count != 1)
		return FALSE;

	/* (the decal's index is its vertices' block's, as decal_new_from_collision locks them) */
	quad = rasterizer_decal_vertices_lock((short)decal_index, sizeof(struct decal_quad));
	if (!quad)
		return FALSE;
	for (corner = 0; corner < 4; corner++)
		quad->vertices[corner].position = corners[corner];
	rasterizer_decal_vertices_unlock();
	decal->position.x = (corners[0].x + corners[2].x) * 0.5f;
	decal->position.y = (corners[0].y + corners[2].y) * 0.5f;
	decal->position.z = (corners[0].z + corners[2].z) * 0.5f;

	/* (into the list of the cluster it is in now) */
	if (cluster_index >= 0 && cluster_index < MAXIMUM_CLUSTERS_PER_STRUCTURE &&
		decal->cluster_index != NONE && decal->cluster_index != cluster_index)
	{
		if (decal->next_decal_index != NONE)
			DECAL_GET(decal->next_decal_index)->previous_decal_index = decal->previous_decal_index;
		if (decal->previous_decal_index != NONE)
			DECAL_GET(decal->previous_decal_index)->next_decal_index = decal->next_decal_index;
		else
			decal_set_first_decal_index(decal->cluster_index, decal->layer, decal->next_decal_index);
		decal_reinsert(decal_index, cluster_index, decal->layer);
	}
	return TRUE;
}

void ae_blood_decal_color(
	long decal_definition_index,
	real_rgb_color *color)
{
	struct decal_definition *definition = decal_definition_index != NONE && decal_definition_index != 0 ?
		decal_definition_get(decal_definition_index) : NULL;

	if (!definition)
	{
		color->red = color->green = color->blue = 1.f;
		return;
	}
	color->red = (definition->color_lower_bound.red + definition->color_upper_bound.red) * 0.5f;
	color->green = (definition->color_lower_bound.green + definition->color_upper_bound.green) * 0.5f;
	color->blue = (definition->color_lower_bound.blue + definition->color_upper_bound.blue) * 0.5f;
}

real ae_blood_decal_radius(
	long decal_definition_index)
{
	struct decal_definition *definition = decal_definition_index != NONE ? decal_definition_get(decal_definition_index) :
		NULL;

	return definition ? MAX(definition->radius_upper_bound, definition->radius_lower_bound) : 0.f;
}
