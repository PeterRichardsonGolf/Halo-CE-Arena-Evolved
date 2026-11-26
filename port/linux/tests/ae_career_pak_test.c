#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ae_pak_rules.h"
#include "ae_mcc_career_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define EQ(a, b) do { long x_ = (long)(a), y_ = (long)(b); if (x_ != y_) { printf("FAIL %s:%d %s = %ld, want %ld\n", __FILE__, __LINE__, #a, x_, y_); failures++; } } while (0)

static void p32(unsigned char *at, unsigned int v) { at[0] = (unsigned char)v; at[1] = (unsigned char)(v >> 8); at[2] = (unsigned char)(v >> 16); at[3] = (unsigned char)(v >> 24); }
static void p64(unsigned char *at, unsigned long long v) { p32(at, (unsigned int)v); p32(at + 4, (unsigned int)(v >> 32)); }

/* ---- a made-up pak: two plain entries, a compressed one, an encrypted one, one outside the file ---- */

static unsigned char pak[8192];
static size_t pak_size;

static size_t put_string(unsigned char *at, char const *s)
{
	p32(at, (unsigned int)strlen(s) + 1);
	memcpy(at + 4, s, strlen(s) + 1);
	return 4 + strlen(s) + 1;
}

static size_t put_entry(unsigned char *at, char const *name, unsigned long long offset, unsigned long long size, unsigned long long usize,
	unsigned int compression, unsigned int flags)
{
	size_t n = put_string(at, name);

	p64(at + n, offset); p64(at + n + 8, size); p64(at + n + 16, usize); p32(at + n + 24, compression);
	memset(at + n + 28, 0xAB, 20);
	n += 48;
	if (compression)
	{
		p32(at + n, 1);
		memset(at + n + 4, 0, 16);
		n += 20;
	}
	at[n] = (unsigned char)flags;
	p32(at + n + 1, 0);
	return n + 5;
}

static void record_put(unsigned char *at, unsigned long long size)
{
	memset(at, 0, AE_PAK_RECORD);
	p64(at + 8, size); p64(at + 16, size);
}

static void pak_build(void)
{
	unsigned char *index = pak + 2048;
	size_t n = 0, count_at;
	static char const one[] = "hello pak data", two[] = "second";

	memset(pak, 0, sizeof(pak));
	/* entry data: a record then the bytes, at 100 and 400 */
	record_put(pak + 100, sizeof(one)); memcpy(pak + 100 + AE_PAK_RECORD, one, sizeof(one));
	record_put(pak + 400, sizeof(two)); memcpy(pak + 400 + AE_PAK_RECORD, two, sizeof(two));
	n += put_string(index + n, "../../../");
	count_at = n; p32(index + n, 5); n += 4;
	n += put_entry(index + n, "MCC/Content/a/one.uexp", 100, sizeof(one), sizeof(one), 0, 0);
	n += put_entry(index + n, "MCC/Content/a/two.uexp", 400, sizeof(two), sizeof(two), 0, 0);
	n += put_entry(index + n, "MCC/Content/a/packed.uexp", 100, 8, 20, 1, 0);
	n += put_entry(index + n, "MCC/Content/a/secret.uexp", 100, 8, 8, 0, 1);
	n += put_entry(index + n, "MCC/Content/a/far.uexp", 8000, 500, 500, 0, 0);
	(void)count_at;
	{
		unsigned char *footer = pak + 2048 + n;

		memset(footer, 0, AE_PAK_FOOTER);
		p32(footer + 17, 0x5A6F12E1u); p32(footer + 21, 7);
		p64(footer + 25, 2048); p64(footer + 33, n);
		pak_size = 2048 + n + AE_PAK_FOOTER;
	}
}

static void test_pak(void)
{
	unsigned long long io, is;
	struct ae_pak_entry e;
	unsigned char *footer;

	pak_build();
	footer = pak + pak_size - AE_PAK_FOOTER;
	CHECK(ae_pak_footer(footer, AE_PAK_FOOTER, pak_size, &io, &is));
	EQ(io, 2048);
	CHECK(is > 100);
	/* a footer that is not a version 7 pak's, an encrypted index, an index outside the file, a short read */
	footer[17] ^= 1;
	CHECK(!ae_pak_footer(footer, AE_PAK_FOOTER, pak_size, &io, &is));
	footer[17] ^= 1;
	footer[21] = 8;
	CHECK(!ae_pak_footer(footer, AE_PAK_FOOTER, pak_size, &io, &is));
	footer[21] = 7;
	footer[16] = 1;
	CHECK(!ae_pak_footer(footer, AE_PAK_FOOTER, pak_size, &io, &is));
	footer[16] = 0;
	CHECK(!ae_pak_footer(footer, AE_PAK_FOOTER, 2048, &io, &is));
	CHECK(!ae_pak_footer(footer, AE_PAK_FOOTER - 1, pak_size, &io, &is));
	CHECK(!ae_pak_footer(NULL, AE_PAK_FOOTER, pak_size, &io, &is));

	CHECK(ae_pak_index_find(pak + io, (size_t)is, pak_size, "MCC/Content/a/one.uexp", &e));
	EQ(e.data_offset, 100 + AE_PAK_RECORD); EQ(e.size, 15);
	CHECK(ae_pak_record_ok(pak + 100, AE_PAK_RECORD, e.size));
	CHECK(!ae_pak_record_ok(pak + 100, AE_PAK_RECORD, e.size + 1));
	CHECK(!ae_pak_record_ok(pak + 100, AE_PAK_RECORD - 1, e.size));
	CHECK(!memcmp(pak + e.data_offset, "hello pak data", 15));
	CHECK(ae_pak_index_find(pak + io, (size_t)is, pak_size, "MCC/Content/a/two.uexp", &e));
	EQ(e.data_offset, 400 + AE_PAK_RECORD); EQ(e.size, 7);
	/* none, a prefix or a longer name, a compressed entry, an encrypted one, one outside the file */
	CHECK(!ae_pak_index_find(pak + io, (size_t)is, pak_size, "MCC/Content/a/none.uexp", &e));
	CHECK(!ae_pak_index_find(pak + io, (size_t)is, pak_size, "MCC/Content/a/one", &e));
	CHECK(!ae_pak_index_find(pak + io, (size_t)is, pak_size, "MCC/Content/a/one.uexpx", &e));
	CHECK(!ae_pak_index_find(pak + io, (size_t)is, pak_size, "MCC/Content/a/packed.uexp", &e));
	CHECK(!ae_pak_index_find(pak + io, (size_t)is, pak_size, "MCC/Content/a/secret.uexp", &e));
	CHECK(!ae_pak_index_find(pak + io, (size_t)is, pak_size, "MCC/Content/a/far.uexp", &e));
	CHECK(!ae_pak_index_find(pak + io, (size_t)is, pak_size, "", &e));
	CHECK(!ae_pak_index_find(NULL, 10, pak_size, "x", &e));
	/* an index cut short never reads past it: every cut length answers cleanly */
	{
		size_t cut;

		for (cut = 0; cut < (size_t)is; cut += 7)
			(void)ae_pak_index_find(pak + io, cut, pak_size, "MCC/Content/a/far.uexp", &e);
	}
	/* a count past the limit, and a name that is not ended in the index */
	{
		unsigned char copy[512];

		memcpy(copy, pak + io, (size_t)is < sizeof(copy) ? (size_t)is : sizeof(copy));
		p32(copy + 4 + 10, 2000000);
		CHECK(!ae_pak_index_find(copy, sizeof(copy), pak_size, "MCC/Content/a/one.uexp", &e));
		p32(copy, 100000);
		CHECK(!ae_pak_index_find(copy, sizeof(copy), pak_size, "MCC/Content/a/one.uexp", &e));
	}
}

/* ---- a made-up data asset: 30 rank pictures and 11 tour pictures named in its name table ---- */

static unsigned char ua[8192], ux[8192];
static size_t ua_size, ux_size;

static void asset_build(int ranks, int tours, int noise)
{
	size_t at = 0, names_at, n;
	int each, total_names;
	char name[128];

	memset(ua, 0, sizeof(ua));
	memset(ux, 0, sizeof(ux));
	p32(ua, 0x9E2A83C1u); p32(ua + 4, (unsigned int)-7); p32(ua + 8, 864); p32(ua + 12, 517); p32(ua + 16, 0);
	p32(ua + 20, 1);			/* one custom version */
	at = 24 + 20;
	p32(ua + at, 7771); at += 4;			/* total size */
	at += put_string(ua + at, "None");
	p32(ua + at, 0); at += 4;			/* flags */
	total_names = 2 + 2 * 41 + 3;
	names_at = at + 8 + 64;
	p32(ua + at, (unsigned int)total_names); p32(ua + at + 4, (unsigned int)names_at);
	n = names_at;
	/* names: a package, a class, then each picture twice (package path and object path), then others */
	n += put_string(ua + n, "/Game/Fake/DA_Test") + 4;
	n += put_string(ua + n, "Default__Thing") + 4;
	for (each = 0; each < 41; each++)
	{
		snprintf(name, sizeof(name), "/Game/Fake/Art/T_Im_%c%d", each < 30 ? 'R' : 'T', each < 30 ? each : each - 30);
		n += put_string(ua + n, name) + 4;
		snprintf(name, sizeof(name), "/Game/Fake/Art/T_Im_%c%d.T_Im_%c%d", each < 30 ? 'R' : 'T', each < 30 ? each : each - 30,
			each < 30 ? 'R' : 'T', each < 30 ? each : each - 30);
		n += put_string(ua + n, name) + 4;
	}
	n += put_string(ua + n, "/Script/Core") + 4;
	n += put_string(ua + n, "Level") + 4;
	n += put_string(ua + n, "None") + 4;
	ua_size = n;
	/* the export data: ranks 50 bytes apart from 300, tours 40 apart from 2400; the object path of picture k is name 3 + 2k */
	for (each = 0; each < ranks; each++)
		{ p32(ux + 300 + each * 50, (unsigned int)(3 + 2 * each)); p32(ux + 304 + each * 50, 0); }
	for (each = 0; each < tours; each++)
		{ p32(ux + 2400 + each * 40, (unsigned int)(3 + 2 * (30 + each))); p32(ux + 2404 + each * 40, 0); }
	if (noise)
		for (each = 0; each < 20; each++)
			{ p32(ux + 1900 + each * 20, 3 + 2 * 5); p32(ux + 1904 + each * 20, 0); }
	ux_size = 4000;
}

static void test_order(void)
{
	static char ranks[AE_MCC_CAREER_RANKS][AE_MCC_CAREER_PATH], tours[AE_MCC_CAREER_TOURS][AE_MCC_CAREER_PATH];

	asset_build(30, 11, 0);
	CHECK(ae_mcc_career_order(ua, ua_size, ux, ux_size, ranks, tours));
	CHECK(!strcmp(ranks[0], "MCC/Content/Fake/Art/T_Im_R0.uexp"));
	CHECK(!strcmp(ranks[29], "MCC/Content/Fake/Art/T_Im_R29.uexp"));
	CHECK(!strcmp(tours[0], "MCC/Content/Fake/Art/T_Im_T0.uexp"));
	CHECK(!strcmp(tours[10], "MCC/Content/Fake/Art/T_Im_T10.uexp"));
	/* a repeated default value among the real ones changes nothing */
	asset_build(30, 11, 1);
	CHECK(ae_mcc_career_order(ua, ua_size, ux, ux_size, ranks, tours));
	CHECK(!strcmp(ranks[7], "MCC/Content/Fake/Art/T_Im_R7.uexp"));
	/* a list that is not whole is no order at all */
	asset_build(29, 11, 0);
	CHECK(!ae_mcc_career_order(ua, ua_size, ux, ux_size, ranks, tours));
	asset_build(30, 10, 0);
	CHECK(!ae_mcc_career_order(ua, ua_size, ux, ux_size, ranks, tours));
	/* a package that is not one, cut short, a null */
	asset_build(30, 11, 0);
	ua[0] ^= 1;
	CHECK(!ae_mcc_career_order(ua, ua_size, ux, ux_size, ranks, tours));
	ua[0] ^= 1;
	CHECK(!ae_mcc_career_order(ua, 30, ux, ux_size, ranks, tours));
	CHECK(!ae_mcc_career_order(ua, ua_size, ux, 8, ranks, tours));
	CHECK(!ae_mcc_career_order(NULL, 0, ux, ux_size, ranks, tours));
	{
		size_t cut;

		for (cut = 0; cut < ua_size; cut += 11)
			(void)ae_mcc_career_order(ua, cut, ux, ux_size, ranks, tours);
	}
}

static void test_texture(void)
{
	unsigned char t[600], rgba[16 * 4];
	unsigned int side;
	size_t pixels, at = 40, i;

	memset(t, 0, sizeof(t));
	p32(t + at, 12); memcpy(t + at + 4, "PF_B8G8R8A8", 12);
	at += 16;
	p32(t + at, 0); p32(t + at + 4, 1); p32(t + at + 8, 1);
	p32(t + at + 12, 72); p32(t + at + 16, 64); p32(t + at + 20, 64);
	at += 12 + 20;
	for (i = 0; i < 16; i++)
	{
		t[at + i * 4] = 10;			/* b */
		t[at + i * 4 + 1] = 20;		/* g */
		t[at + i * 4 + 2] = 30;		/* r */
		t[at + i * 4 + 3] = (unsigned char)(i < 8 ? 255 : 0);
	}
	CHECK(ae_mcc_career_texture(t, at + 64 + 4, &side, &pixels));
	EQ(side, 4); EQ(pixels, at);
	/* cut short, another size than a square, the mip count wrong */
	CHECK(!ae_mcc_career_texture(t, at + 60, &side, &pixels));
	p32(t + 40 + 16 + 4, 2);
	CHECK(!ae_mcc_career_texture(t, at + 68, &side, &pixels));
	p32(t + 40 + 16 + 4, 1);
	p32(t + 40 + 16 + 12 + 8, 60); p32(t + 40 + 16 + 12 + 4, 60);
	CHECK(!ae_mcc_career_texture(t, at + 68, &side, &pixels));
	CHECK(!ae_mcc_career_texture(NULL, 100, &side, &pixels));
	/* scaling: BGRA to RGBA, the colour weighed by its alpha, the alpha averaged */
	CHECK(ae_mcc_career_scale(t + at, 4, rgba, 2));
	EQ(rgba[0], 30); EQ(rgba[1], 20); EQ(rgba[2], 10); EQ(rgba[3], 255);
	EQ(rgba[3 + 4 * 1], 255);
	EQ(rgba[3 + 8], 0); EQ(rgba[0 + 8], 0);
	CHECK(ae_mcc_career_scale(t + at, 4, rgba, 4));
	EQ(rgba[0], 30);
	CHECK(!ae_mcc_career_scale(t + at, 4, rgba, 3));
	CHECK(!ae_mcc_career_scale(t + at, 4, rgba, 8));
	CHECK(!ae_mcc_career_scale(NULL, 4, rgba, 2));
}

/* ---- the player's own pak, when the test is pointed at it (AE_MCC_PAK_TEST: the file, or a sparse copy with the same places) ---- */

static int file_read(FILE *f, unsigned long long at, void *buffer, size_t size)
{
	return fseeko(f, (off_t)at, SEEK_SET) == 0 && fread(buffer, 1, size, f) == size;
}

static unsigned char *entry_load(FILE *f, struct ae_pak_entry const *e)
{
	unsigned char record[AE_PAK_RECORD], *data;

	if (!file_read(f, e->data_offset - AE_PAK_RECORD, record, sizeof(record)) || !ae_pak_record_ok(record, sizeof(record), e->size))
		return NULL;
	data = malloc((size_t)e->size);
	if (data && !file_read(f, e->data_offset, data, (size_t)e->size))
	{
		free(data);
		return NULL;
	}
	return data;
}

static void test_real(char const *path)
{
	static char ranks[AE_MCC_CAREER_RANKS][AE_MCC_CAREER_PATH], tours[AE_MCC_CAREER_TOURS][AE_MCC_CAREER_PATH];
	static unsigned char rgba[128 * 128 * 4];
	FILE *f = fopen(path, "rb");
	unsigned char footer[AE_PAK_FOOTER], *index, *a, *x;
	unsigned long long size, io, is;
	struct ae_pak_entry ea, ex, e;
	int each, read = 0;

	CHECK(f != NULL);
	if (!f)
		return;
	fseeko(f, 0, SEEK_END);
	size = (unsigned long long)ftello(f);
	CHECK(file_read(f, size - AE_PAK_FOOTER, footer, sizeof(footer)));
	CHECK(ae_pak_footer(footer, sizeof(footer), size, &io, &is));
	index = malloc((size_t)is);
	CHECK(index && file_read(f, io, index, (size_t)is));
	CHECK(ae_pak_index_find(index, (size_t)is, size, "MCC/Content/UI/Features/Nameplate/DA_AccountProgression.uasset", &ea));
	CHECK(ae_pak_index_find(index, (size_t)is, size, "MCC/Content/UI/Features/Nameplate/DA_AccountProgression.uexp", &ex));
	a = entry_load(f, &ea);
	x = entry_load(f, &ex);
	CHECK(a && x);
	CHECK(ae_mcc_career_order(a, (size_t)ea.size, x, (size_t)ex.size, ranks, tours));
	for (each = 0; each < AE_MCC_CAREER_RANKS + AE_MCC_CAREER_TOURS; each++)
	{
		char const *p = each < AE_MCC_CAREER_RANKS ? ranks[each] : tours[each - AE_MCC_CAREER_RANKS];
		unsigned char *d;
		unsigned int side;
		size_t pixels, i, solid = 0;

		CHECK(ae_pak_index_find(index, (size_t)is, size, p, &e));
		d = entry_load(f, &e);
		CHECK(d != NULL);
		if (!d)
			continue;
		CHECK(ae_mcc_career_texture(d, (size_t)e.size, &side, &pixels));
		EQ(side, 512);
		CHECK(ae_mcc_career_scale(d + pixels, side, rgba, 128));
		for (i = 0; i < 128 * 128; i++)
			solid += rgba[i * 4 + 3] > 200;
		CHECK(solid > 500);
		read++;
		if (each == 0 || each == 29 || each == 30 || each == 40)
			printf("%s\n", p);
		free(d);
	}
	printf("mcc career insignia read: %d\n", read);
	free(a); free(x); free(index);
	fclose(f);
}

int main(void)
{
	char const *real = getenv("AE_MCC_PAK_TEST");

	test_pak();
	test_order();
	test_texture();
	if (real)
		test_real(real);
	if (failures)
		return 1;
	printf("ok\n");
	return 0;
}
