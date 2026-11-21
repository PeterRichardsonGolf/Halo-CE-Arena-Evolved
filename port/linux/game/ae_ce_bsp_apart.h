#ifndef AE_CE_BSP_APART_H
#define AE_CE_BSP_APART_H

#include <stdint.h>

/* (included by ce_bsp.c after its private code: struct structure_lightmap, struct structure_material, ce_image_*)

ce_bsp_loaded converts every material in place: it reads a material's counts and data, writes its vertices back
normalised and its descriptors as compressed. The check reads the map's bytes before any of that, so the bytes one
material's conversion writes must not be another's counts or the lightmaps' blocks:
- lightmaps' material blocks may be the same block, or share whole materials, but not overlap at another offset
  (a material read half over another one's fields);
- no material's vertex data and no material may lie over the BSP's structure, its lightmaps or any material.
FALSE (refused) if they do. Whole blocks out of the image are left to the checks after this one. */

struct ae_ce_bsp_span
{
	uintptr_t start, end;
};

static int ae_ce_bsp_span_compare(void const *a, void const *b)
{
	uintptr_t x = ((struct ae_ce_bsp_span const *)a)->start, y = ((struct ae_ce_bsp_span const *)b)->start;

	return x < y ? -1 : x > y;
}

/* whether [start, end) overlaps a span of the sorted, disjoint list */
static boolean ae_ce_bsp_span_hits(struct ae_ce_bsp_span const *spans, long count, uintptr_t start, uintptr_t end)
{
	long low = 0, high = count;

	while (low < high)
	{
		long middle = low + (high - low) / 2;

		if (spans[middle].end <= start)
			low = middle + 1;
		else
			high = middle;
	}
	return low < count && spans[low].start < end;
}

static boolean ae_ce_bsp_materials_apart(struct ce_image const *image, byte *structure, byte *lightmaps,
	long lightmap_count)
{
	struct ae_ce_bsp_span *blocks = NULL, *regions = NULL;
	long block_count = 0, region_count = 0, index;
	boolean apart = TRUE;

	if (lightmap_count <= 0)
		return TRUE;
	blocks = malloc((size_t)lightmap_count * sizeof(*blocks));
	regions = malloc(((size_t)lightmap_count + 1) * sizeof(*regions));
	if (!blocks || !regions)
	{
		if (blocks)
			free(blocks);
		if (regions)
			free(regions);
		return ce_refuse("a structure BSP's materials cannot be checked: out of memory");
	}
	for (index = 0; index < lightmap_count; index++)
	{
		struct structure_lightmap *lightmap = (struct structure_lightmap *)(lightmaps +
			index * sizeof(struct structure_lightmap));
		long count;
		byte *materials;

		if (!ce_image_block(image, &lightmap->materials, sizeof(struct structure_material), 0x10000,
			"a structure BSP's lightmap materials", &count, &materials))
		{
			apart = FALSE;
			goto done;
		}
		if (count > 0)
		{
			blocks[block_count].start = (uintptr_t)materials;
			blocks[block_count++].end = (uintptr_t)materials + (uintptr_t)count * sizeof(struct structure_material);
		}
	}
	/* the materials' bytes as disjoint regions, every block in one on its first block's grid */
	qsort(blocks, (size_t)block_count, sizeof(*blocks), ae_ce_bsp_span_compare);
	for (index = 0; index < block_count; index++)
	{
		if (region_count && blocks[index].start < regions[region_count - 1].end)
		{
			if ((blocks[index].start - regions[region_count - 1].start) % sizeof(struct structure_material))
			{
				apart = ce_refuse("a structure BSP's lightmaps' material blocks overlap one another's materials");
				goto done;
			}
			if (blocks[index].end > regions[region_count - 1].end)
				regions[region_count - 1].end = blocks[index].end;
		}
		else
		{
			regions[region_count++] = blocks[index];
		}
	}
	if (ae_ce_bsp_span_hits(regions, region_count, (uintptr_t)lightmaps,
			(uintptr_t)lightmaps + (uintptr_t)lightmap_count * sizeof(struct structure_lightmap)) ||
		ae_ce_bsp_span_hits(regions, region_count, (uintptr_t)structure, (uintptr_t)structure + sizeof(struct structure_bsp)))
	{
		apart = ce_refuse("a structure BSP's lightmap materials lie over its structure or lightmaps");
		goto done;
	}
	/* every material once: its vertex data off the lightmaps and every material */
	for (index = 0; index < region_count && apart; index++)
	{
		uintptr_t at;

		for (at = regions[index].start; at < regions[index].end; at += sizeof(struct structure_material))
		{
			struct structure_material *material = (struct structure_material *)at;
			unsigned long size;
			byte *data;
			uintptr_t lightmaps_start = (uintptr_t)lightmaps;
			uintptr_t lightmaps_end = lightmaps_start + (uintptr_t)lightmap_count * sizeof(struct structure_lightmap);
			uintptr_t structure_start = (uintptr_t)structure, structure_end = structure_start + sizeof(struct structure_bsp);

			if (!ce_image_data(image, &material->uncompressed_vertex_data, "a structure BSP's material's vertices",
				&size, &data))
			{
				apart = FALSE;
				break;
			}
			if (!size)
				continue;
			if (ae_ce_bsp_span_hits(regions, region_count, (uintptr_t)data, (uintptr_t)data + size) ||
				((uintptr_t)data < lightmaps_end && lightmaps_start < (uintptr_t)data + size) ||
				((uintptr_t)data < structure_end && structure_start < (uintptr_t)data + size))
			{
				apart = ce_refuse("a structure BSP's material's vertices lie over its structure, lightmaps or materials");
				break;
			}
		}
	}
done:
	free(blocks);
	free(regions);
	return apart;
}

#endif
