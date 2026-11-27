/*
AE_FORGE_COMPAT.H

Included before every source of Content Evolved's Forge (port/forge, tools/forge_build.py): what Forge's sources
expect of the game that this build declares elsewhere, or under other names. Forge reads Custom Edition maps and tag
bundles through the Custom Edition copy (cev/, its names prefixed by cev_names.h); the map list and the loaded map's
path are this build's own (ae_map_share.h).
*/

#ifndef __AE_FORGE_COMPAT_H
#define __AE_FORGE_COMPAT_H

#include "cseries.h"
#include "cache/cache_files.h"

#include "cev/cev_names.h"
#include "cev/custom_edition_cache.h"
#include "cev/cache_file_formats.h"
#include "cev/tag_bundle.h"
#include "cev/tag_schema.h"

/* (the names this build's own map sharing (ae_map_share.c) and sound encoder (loose_sounds_encode.c) answer to,
not the copy's) */
#undef custom_edition_map_file_path
#undef custom_edition_cache_loaded_path
#undef custom_edition_sounds_encode
#undef halo_custom_edition_tag_cache
boolean custom_edition_map_file_path(char const *level_name, char *path);
char const *custom_edition_cache_loaded_path(void);
boolean halo_custom_edition_tag_cache(void);

/* Forge's buffers that the game's data points into are in the game's heap (an Xbox address, in a 64-bit build, is
a place in it; malloc and free are the game's already) */
#undef calloc
#define calloc(count, size) ae_forge_calloc(__FILE__, __LINE__, (count), (size))
static __inline__ void *ae_forge_calloc(char const *file, int line, size_t count, size_t size)
{
	void *memory = 0;

	if (count == 0 || size <= ((size_t)-1) / count)
	{
		memory = match_malloc(file, line, count * size);
		if (memory)
			memset(memory, 0, count * size);
	}
	return memory;
}

/* the rasterizer's buffers (as mod_models.c declares them) */
struct vertex_buffer;
struct triangle_buffer;
boolean rasterizer_vertex_buffer_new(struct vertex_buffer *vertex_buffer, long vertex_type, long count,
	void const *vertices, long buffer_size);
void rasterizer_vertex_buffer_delete(struct vertex_buffer *vertex_buffer);
boolean rasterizer_triangle_buffer_new(struct triangle_buffer *triangle_buffer, short triangle_type, long count,
	void const *triangles);
void rasterizer_triangle_buffer_delete(struct triangle_buffer *triangle_buffer);

#endif /* __AE_FORGE_COMPAT_H */
