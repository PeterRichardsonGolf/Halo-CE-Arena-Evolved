#include "harness.h"
#include <stdint.h>
#include <stdlib.h>
/* port/linux/game/ae_ce_bsp_apart.h: a Custom Edition BSP's lightmaps' material blocks, and the materials' vertex
data, as ce_bsp_loaded converts them in place: the same block or whole shared materials pass; a block over
another's materials at another offset, or vertex data over the structure, the lightmaps or a material, is refused */

struct block_field { long count; byte *address; };
struct data_field { unsigned long size; byte *address; };
struct structure_bsp { long pad[4]; };
struct structure_lightmap { long pad[3]; struct block_field materials; };
struct structure_material { long pad[6]; struct data_field uncompressed_vertex_data; long tail[2]; };
struct ce_image { byte *data; unsigned long base, size; };
static int refusals;
static boolean ce_refuse(char const *format, ...) { refusals++; return FALSE; }
static boolean ce_image_block(struct ce_image const *image, void const *field, unsigned long element_size,
	unsigned long maximum_count, char const *what, long *count, byte **elements)
{
	struct block_field const *block = field;

	*count = block->count;
	*elements = block->address;
	return TRUE;
}
static boolean ce_image_data(struct ce_image const *image, void const *field, char const *what, unsigned long *size,
	byte **data)
{
	struct data_field const *data_field = field;

	*size = data_field->size;
	*data = data_field->address;
	return TRUE;
}
#include "under_test.inc"

static byte image_bytes[0x10000] __attribute__((aligned(16)));
static struct ce_image image = { image_bytes, 0, sizeof(image_bytes) };
#define STRUCTURE (image_bytes)
#define LIGHTMAPS (image_bytes + 0x100)
#define MATERIALS (image_bytes + 0x1000)
#define VERTICES (image_bytes + 0x8000)

static struct structure_lightmap *lightmap(int index) { return (struct structure_lightmap *)LIGHTMAPS + index; }
static struct structure_material *material(byte *block, int index) { return (struct structure_material *)block + index; }

/* two lightmaps, 4 materials each at first and second, each material's data its own */
static void layout(byte *first, byte *second)
{
	int index;

	memset(image_bytes, 0, sizeof(image_bytes));
	lightmap(0)->materials.count = 4;
	lightmap(0)->materials.address = first;
	lightmap(1)->materials.count = 4;
	lightmap(1)->materials.address = second;
	for (index = 0; index < 4; index++)
	{
		material(first, index)->uncompressed_vertex_data.size = 64;
		material(first, index)->uncompressed_vertex_data.address = VERTICES + (first - MATERIALS) * 4 + index * 64;
		material(second, index)->uncompressed_vertex_data.size = 64;
		material(second, index)->uncompressed_vertex_data.address = VERTICES + (second - MATERIALS) * 4 + index * 64;
	}
}

static boolean apart(void) { return ae_ce_bsp_materials_apart(&image, STRUCTURE, LIGHTMAPS, 2); }

int main(int argc, char **argv)
{
	char const *case_name = argv[1];

	CASE("kept")
	{
		layout(MATERIALS, MATERIALS + 0x800);
		CHECK(apart(), "two blocks apart refused");
		layout(MATERIALS, MATERIALS);
		CHECK(apart(), "one block for both lightmaps refused");
		layout(MATERIALS, MATERIALS + 2 * sizeof(struct structure_material));
		CHECK(apart(), "blocks sharing whole materials refused");
		CHECK(!refusals, "%d refusals", refusals);
		return 0;
	}
	CASE("refused")
	{
		layout(MATERIALS, MATERIALS + sizeof(struct structure_material) + 8);
		CHECK(!apart(), "a block over another's materials at another offset kept");
		layout(MATERIALS, MATERIALS + 0x800);
		material(MATERIALS, 1)->uncompressed_vertex_data.address = MATERIALS + 0x800 + 16;
		CHECK(!apart(), "vertex data over a material kept");
		layout(MATERIALS, MATERIALS + 0x800);
		material(MATERIALS, 2)->uncompressed_vertex_data.address = LIGHTMAPS;
		CHECK(!apart(), "vertex data over the lightmaps kept");
		layout(MATERIALS, MATERIALS + 0x800);
		material(MATERIALS + 0x800, 3)->uncompressed_vertex_data.address = STRUCTURE + 4;
		CHECK(!apart(), "vertex data over the structure kept");
		layout(MATERIALS, LIGHTMAPS + sizeof(struct structure_lightmap));
		CHECK(!apart(), "a material block over the lightmaps kept");
		return 0;
	}
	fprintf(stderr, "unknown case %s\n", case_name);
	return 2;
}
