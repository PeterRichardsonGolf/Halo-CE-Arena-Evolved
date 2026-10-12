/*
HUD_HIRES.C

The high-res HUD's textures (hud_hires.h): which one stands for a bitmap being
uploaded, and each one's GL texture.

Which bitmap is at an address the game knows (from the loaded map's tags:
port/linux/game/hud_hires_tags.c). Each texture is uploaded from its PNG when
first drawn and kept: up to 71 of the HUD's, about 227 MB with their mip
levels, though a game draws only some (the scopes' only when zoomed), and
the titles of the menus shown, about 3 MB each (11 MB for the carnage
report's, a whole panel), and their button icons, about 0.3 MB each (5.3
MB for the message icons' sheet). The HUD's PNGs in a map are decoded as it
loads, on a thread of their own, so that the HUD's first frame only uploads
them (decoding them there held that frame up); what is decoded and not drawn
is let go when the map is unloaded.
They are drawn with linear filtering and their mip levels (d3d8_gl.c,
configure_sampler), as they are larger than they appear.

A button icon may have a second texture, of a PlayStation controller's
buttons, drawn instead while one is the controller last used
(platform_input_scheme), whatever display.high_res_text says; one that has
only that is drawn only then (DamnationCE v0.3.41's).

The embedded PNGs are the ones tools/hud_assets.py, title_assets.py and
button_assets.py write. A menus folder's PNGs can be anyone's. Only 8-bit
RGBA, non-interlaced PNGs are read, with their data inflated by the port's
zlib (port/third_party/zlib).
*/

#include "hud_hires.h"
#include "platform.h"
#include "port_config.h"
#include "xgpu.h"
#include "ae_ce_hud.h" /* AE hook */

#include "zlib_prefixed.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

/* xinput_sdl.c's: the controller last used (2: a PlayStation one), which it
keeps with the game browser (every build but --no-game-browser's) */
#ifdef HALO_GAME_BROWSER
int platform_input_scheme(void);
#else
#define platform_input_scheme() 1
#endif

/* the game's (port/linux/game/hud_hires_tags.c) */
long hud_hires_asset_at(unsigned long address, long width, long height);

#define MAXIMUM_TEXTURES 128
/* a PNG's inflated rows and its texels, which are held at once: 128 MB for
a 4096 by 4096 sheet (the largest shipped, 2048 by 2048, takes 32 MB), and
no more for a small file that names a large size (a menus folder's) */
#define MAXIMUM_DECODED_SIZE (192UL << 20)

/* a texture's PNG decoded ahead of its first draw (decode_state) */
enum
{
	_decode_none,
	_decode_queued,
	_decode_decoding,
	_decode_done,
};

static struct
{
	unsigned int texture;
	unsigned long levels;
	int failed;
	int other_pixels_logged;
	/* (decoded ahead: these, the decoding thread's until it is done, under
	decode_lock) */
	int decode_state;
	unsigned char *pixels;
	unsigned long width, height;
	/* its PlayStation texture's, decoded on first use (DamnationCE v0.3.41) */
	unsigned int playstation_texture;
	unsigned long playstation_levels;
	int playstation_failed;
} textures[MAXIMUM_TEXTURES];

/* (the thread, detached, is waited for by its decode_running) */
static pthread_mutex_t decode_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t decode_finished = PTHREAD_COND_INITIALIZER;
static int decode_running, decode_stopping;

static unsigned char *png_decode(const unsigned char *data, unsigned long size, unsigned long *png_width,
	unsigned long *png_height);
static unsigned int own_texture(long asset, unsigned long *levels);

long hud_hires_asset_count(void)
{
	return hud_hires_embedded_count < MAXIMUM_TEXTURES ? (long)hud_hires_embedded_count : MAXIMUM_TEXTURES;
}

char const *hud_hires_asset_tag(long asset)
{
	return hud_hires_embedded[asset].tag;
}

long hud_hires_asset_bitmap(long asset)
{
	return hud_hires_embedded[asset].bitmap;
}

/* whether the texture can stand for a bitmap of this size: a whole multiple
of it, the same both ways */
long hud_hires_asset_fits(long asset, long width, long height)
{
	const struct hud_hires_embedded *embedded = &hud_hires_embedded[asset];

	return width > 0 && height > 0 && embedded->width % width == 0 && embedded->height % height == 0 &&
		embedded->width / width == embedded->height / height && embedded->width / width > 1;
}

/* whether the asset's own texture is drawn: its setting (display.high_res_hud,
or display.high_res_text for the menus') on */
static int setting_on(const struct hud_hires_embedded *embedded)
{
	static int hud_enabled, titles_enabled;
	static unsigned long read_at = (unsigned long)-1;

	if (read_at != config_changes())
	{
		read_at = config_changes();
		hud_enabled = config_boolean("display.high_res_hud");
		titles_enabled = config_boolean("display.high_res_text");
	}
	return embedded->title ? titles_enabled : hud_enabled;
}

/* whether the asset is drawn now: its PlayStation texture (a PlayStation
controller the last used), else its own (its setting on); DamnationCE
v0.3.41 */
static int drawn_now(const struct hud_hires_embedded *embedded, int *playstation)
{
	*playstation = embedded->png_playstation && platform_input_scheme() == 2;
	return *playstation || (embedded->png && setting_on(embedded));
}

/* its PlayStation texture, decoded on first use */
static unsigned int playstation_texture(long asset, unsigned long *levels)
{
	const struct hud_hires_embedded *embedded = &hud_hires_embedded[asset];

	if (!embedded->png_playstation || textures[asset].playstation_failed)
		return 0;
	if (!textures[asset].playstation_texture)
	{
		textures[asset].playstation_texture = hud_hires_png_texture(embedded->png_playstation,
			embedded->png_playstation_size, &textures[asset].playstation_levels);
		if (!textures[asset].playstation_texture)
		{
			platform_log("high-res hud: could not decode the PlayStation texture for %s bitmap %d", embedded->tag,
				embedded->bitmap);
			textures[asset].playstation_failed = 1;
			return 0;
		}
	}
	*levels = textures[asset].playstation_levels;
	return textures[asset].playstation_texture;
}

long hud_hires_override_find(unsigned long address, unsigned long width, unsigned long height,
	unsigned long level0_size)
{
	long asset;

	asset = hud_hires_asset_at(address, (long)width, (long)height);
	if (asset < 0 || asset >= hud_hires_asset_count())
		return -1;
	/* (one with a PlayStation texture whatever its setting: which is drawn
	is chosen as it is drawn, hud_hires_override_texture) */
	if (!setting_on(&hud_hires_embedded[asset]) && !hud_hires_embedded[asset].png_playstation)
		return -1;
	if (ae_ce_hud_pixels(hud_hires_embedded[asset].tag, hud_hires_embedded[asset].bitmap, address, level0_size)) /* AE hook */
		return asset;
#ifdef HALO_64BIT
	/* (an Xbox address: the texture's pixels in the contiguous memory) */
	if (crc32(0L, (const Bytef *)xbox_pointer(address), (uInt)level0_size) != hud_hires_embedded[asset].crc)
#else
	if (crc32(0L, (const Bytef *)address, (uInt)level0_size) != hud_hires_embedded[asset].crc)
#endif
	{
		if (!textures[asset].other_pixels_logged)
		{
			platform_log("high-res hud: %s bitmap %d is not the one its texture was drawn for here "
				"(another language's or a modified map): drawn as it is",
				hud_hires_embedded[asset].tag, hud_hires_embedded[asset].bitmap);
			textures[asset].other_pixels_logged = 1;
		}
		return -1;
	}
	return asset;
}

int hud_hires_override_coverage(long asset)
{
	return asset >= 0 && asset < hud_hires_asset_count() && hud_hires_embedded[asset].coverage;
}

int hud_hires_override_switches(long asset)
{
	return asset >= 0 && asset < hud_hires_asset_count() && hud_hires_embedded[asset].png_playstation;
}

/* ---------- sprites */

/* the placeholders of the textures drawn for some of a bitmap's sprites
(port/linux/game/hud_hires_tags.c): their D3D textures' Data */
#define MAXIMUM_PLACEHOLDERS 8

static struct
{
	unsigned long data;
	long asset;
} placeholders[MAXIMUM_PLACEHOLDERS];
static long placeholder_count = 0;

/* the sequences whose sprites the texture is drawn for (hud_hires.h), or 0 */
unsigned long hud_hires_asset_sprites(long asset)
{
	return hud_hires_embedded[asset].sprites;
}

/* whether the texture drawn for some of a bitmap's sprites can be drawn for
the bitmap whose pixels are at address (guest virtual; its first mip level
level0_size bytes): its setting on (or a PlayStation texture, whatever it
says), the pixels those it was drawn for, and a texture of it decoded.
Which is drawn, if either, is chosen as the sprites are
(hud_hires_sprites_drawn) */
int hud_hires_sprites_drawable(long asset, unsigned long address, unsigned long level0_size)
{
	const struct hud_hires_embedded *embedded;
	unsigned long levels;

	if (asset < 0 || asset >= hud_hires_asset_count() || !hud_hires_embedded[asset].sprites)
		return 0;
	embedded = &hud_hires_embedded[asset];
	if (!(embedded->png && setting_on(embedded)) && !embedded->png_playstation)
		return 0;
#ifdef HALO_64BIT
	/* (an Xbox address, as hud_hires_override_find's) */
	if (crc32(0L, (const Bytef *)xbox_pointer(address), (uInt)level0_size) != hud_hires_embedded[asset].crc)
#else
	if (crc32(0L, (const Bytef *)address, (uInt)level0_size) != hud_hires_embedded[asset].crc)
#endif
	{
		if (!textures[asset].other_pixels_logged)
		{
			platform_log("high-res hud: %s bitmap %d is not the one its sprites' texture was drawn for "
				"here (another language's or a modified map): drawn as it is",
				hud_hires_embedded[asset].tag, hud_hires_embedded[asset].bitmap);
			textures[asset].other_pixels_logged = 1;
		}
		return 0;
	}
	return embedded->png && setting_on(embedded) ? own_texture(asset, &levels) != 0 :
		playstation_texture(asset, &levels) != 0;
}

/* whether the texture's sprites are drawn now (a texture of it for the
controller last used), not the bitmap's own */
int hud_hires_sprites_drawn(long asset)
{
	unsigned long levels;

	return hud_hires_override_texture(asset, &levels) != 0;
}

/* the placeholder the game draws the texture's sprites from: its D3D
texture (NULL forgets it) */
void hud_hires_register_placeholder(long asset, const unsigned long *texture)
{
	long index;

	for (index = 0; index < placeholder_count && placeholders[index].asset != asset; index++)
		;
	if (texture && index == placeholder_count && placeholder_count < MAXIMUM_PLACEHOLDERS)
		placeholder_count++;
	if (index < placeholder_count)
	{
		placeholders[index].data = texture ? texture[1] : 0;
		placeholders[index].asset = asset;
	}
}

unsigned int hud_hires_placeholder_texture(unsigned long data, unsigned long *levels)
{
	long index;

	for (index = 0; data && index < placeholder_count; index++)
	{
		if (placeholders[index].data == data)
			return hud_hires_override_texture(placeholders[index].asset, levels);
	}
	return 0;
}

int hud_hires_override_point_threshold(long asset)
{
	return asset >= 0 && asset < hud_hires_asset_count() && hud_hires_embedded[asset].point_threshold;
}

/* ---------- decoding */

static unsigned long big_endian_long(const unsigned char *bytes)
{
	return ((unsigned long)bytes[0] << 24) | ((unsigned long)bytes[1] << 16) |
		((unsigned long)bytes[2] << 8) | bytes[3];
}

static unsigned char paeth(unsigned char left, unsigned char up, unsigned char up_left)
{
	int estimate = (int)left + up - up_left;
	int to_left = abs(estimate - left), to_up = abs(estimate - up), to_up_left = abs(estimate - up_left);

	if (to_left <= to_up && to_left <= to_up_left)
		return left;
	return to_up <= to_up_left ? up : up_left;
}

/* the PNG's texels, RGBA in rows top first, and its size; NULL if it is not
one that tools/hud_assets.py writes */
static unsigned char *png_decode(const unsigned char *data, unsigned long size, unsigned long *png_width,
	unsigned long *png_height)
{
	unsigned long position = 8;
	unsigned long width = size >= 33 ? big_endian_long(data + 16) : 0;
	unsigned long height = size >= 33 ? big_endian_long(data + 20) : 0;
	unsigned long stride = width * 4, filtered_size = height * (stride + 1);
	unsigned char *compressed = NULL, *filtered = NULL, *pixels = NULL;
	unsigned long compressed_size = 0, row, column;
	uLongf inflated_size = filtered_size;
	int result;

	if (size < 33 || memcmp(data, "\x89PNG\r\n\x1a\n", 8) || memcmp(data + 12, "IHDR", 4) ||
		!width || !height || width > 8192 || height > 8192 ||
		data[24] != 8 || data[25] != 6 || data[28] != 0)
		return NULL;
	if (filtered_size + stride * height > MAXIMUM_DECODED_SIZE)
	{
		platform_log("png: %lux%lu is too large to decode (more than %lu MB)", width, height,
			(unsigned long)(MAXIMUM_DECODED_SIZE >> 20));
		return NULL;
	}
	*png_width = width;
	*png_height = height;
	compressed = malloc(size);
	while (compressed && position + 12 <= size)
	{
		unsigned long length = big_endian_long(data + position);

		if (length > size - position - 12)
			break;
		if (!memcmp(data + position + 4, "IDAT", 4))
		{
			memcpy(compressed + compressed_size, data + position + 8, length);
			compressed_size += length;
		}
		else if (!memcmp(data + position + 4, "IEND", 4))
		{
			break;
		}
		position += 12 + length;
	}
	filtered = compressed ? malloc(filtered_size) : NULL;
	pixels = filtered ? malloc(stride * height) : NULL;
	if (!pixels)
		goto failed;
	result = uncompress(filtered, &inflated_size, compressed, compressed_size);
	/* (the game's zlib is 1.1, which can stop short of saying the stream has
	ended when the output is exactly full: all of it is enough) */
	if ((result != Z_OK && result != Z_BUF_ERROR) || inflated_size != filtered_size)
		goto failed;
	/* (each row by its filter, the first pixel's 4 bytes, which have none to
	their left, apart; the first row has none above: zeroes) */
	for (row = 0; row < height; row++)
	{
		const unsigned char *line = filtered + row * (stride + 1) + 1;
		unsigned char filter = line[-1];
		unsigned char *out = pixels + row * stride;
		const unsigned char *above = row ? out - stride : NULL;

		if (filter > 4)
			goto failed;
		if (!above && filter == 2)
			filter = 0; /* (up: zero) */
		else if (!above && filter == 4)
			filter = 1; /* (Paeth of left, zero and zero: left) */
		switch (filter)
		{
		case 0:
			memcpy(out, line, stride);
			break;
		case 1:
			memcpy(out, line, 4);
			for (column = 4; column < stride; column++)
				out[column] = (unsigned char)(line[column] + out[column - 4]);
			break;
		case 2:
			for (column = 0; column < stride; column++)
				out[column] = (unsigned char)(line[column] + above[column]);
			break;
		case 3:
			for (column = 0; column < 4; column++)
				out[column] = (unsigned char)(line[column] + (above ? above[column] : 0) / 2);
			for (column = 4; column < stride; column++)
				out[column] = (unsigned char)(line[column] +
					((unsigned)out[column - 4] + (above ? above[column] : 0)) / 2);
			break;
		default:
			/* (Paeth of zero, up and zero: up) */
			for (column = 0; column < 4; column++)
				out[column] = (unsigned char)(line[column] + above[column]);
			for (column = 4; column < stride; column++)
				out[column] = (unsigned char)(line[column] + paeth(out[column - 4], above[column], above[column - 4]));
			break;
		}
	}
	free(compressed);
	free(filtered);
	return pixels;

failed:
	free(compressed);
	free(filtered);
	free(pixels);
	return NULL;
}

/* a GL texture of a decoded PNG's texels (which it frees), as below */
static unsigned int png_pixels_texture(unsigned char *pixels, unsigned long width, unsigned long height,
	unsigned long *levels)
{
	unsigned long largest;
	GLuint texture;

	if (!pixels)
		return 0;
	*levels = 1;
	for (largest = width > height ? width : height; largest > 1; largest >>= 1)
		(*levels)++;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	xgpu_gl_state_invalidate();
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, (GLint)*levels - 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, (GLsizei)width, (GLsizei)height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	glGenerateMipmap(GL_TEXTURE_2D);
	xgpu_gl_state_invalidate();
	free(pixels);
	return texture;
}

unsigned int hud_hires_png_texture(const void *png, unsigned long size, unsigned long *levels)
{
	unsigned long width = 0, height = 0;
	unsigned char *pixels = png_decode(png, size, &width, &height);

	return png_pixels_texture(pixels, width, height, levels);
}

unsigned int hud_hires_override_texture(long asset, unsigned long *levels)
{
	int playstation;

	if (asset < 0 || asset >= hud_hires_asset_count() || !drawn_now(&hud_hires_embedded[asset], &playstation))
		return 0;
	return playstation ? playstation_texture(asset, levels) : own_texture(asset, levels);
}

/* its own texture (its PNG decoded as the map loaded, or now) */
static unsigned int own_texture(long asset, unsigned long *levels)
{
	const struct hud_hires_embedded *embedded;

	if (asset < 0 || asset >= hud_hires_asset_count() || textures[asset].failed || !hud_hires_embedded[asset].png)
		return 0;
	if (textures[asset].texture)
	{
		*levels = textures[asset].levels;
		return textures[asset].texture;
	}
	embedded = &hud_hires_embedded[asset];
	{
		unsigned char *pixels = NULL;
		unsigned long width = 0, height = 0;
		int decoded = 0;

		/* (its PNG decoded as the map loaded, or being decoded: waited for) */
		pthread_mutex_lock(&decode_lock);
		while (textures[asset].decode_state == _decode_decoding)
			pthread_cond_wait(&decode_finished, &decode_lock);
		if (textures[asset].decode_state == _decode_done)
		{
			pixels = textures[asset].pixels;
			width = textures[asset].width;
			height = textures[asset].height;
			textures[asset].pixels = NULL;
			decoded = 1;
		}
		textures[asset].decode_state = _decode_none;
		pthread_mutex_unlock(&decode_lock);
		if (!decoded)
			pixels = png_decode((const unsigned char *)embedded->png, embedded->png_size, &width, &height);
		textures[asset].texture = png_pixels_texture(pixels, width, height, &textures[asset].levels);
	}
	if (!textures[asset].texture)
	{
		platform_log("high-res hud: could not decode the texture for %s bitmap %d", embedded->tag, embedded->bitmap);
		textures[asset].failed = 1;
		return 0;
	}
	*levels = textures[asset].levels;
	return textures[asset].texture;
}

/* ---------- decoding ahead */

static void *decode_thread_main(void *unused)
{
	long asset, count = hud_hires_asset_count();

	(void)unused;
	for (asset = 0; asset < count; asset++)
	{
		const struct hud_hires_embedded *embedded = &hud_hires_embedded[asset];
		unsigned long width = 0, height = 0;
		unsigned char *pixels;

		pthread_mutex_lock(&decode_lock);
		if (decode_stopping)
		{
			pthread_mutex_unlock(&decode_lock);
			break;
		}
		if (textures[asset].decode_state != _decode_queued || !embedded->png)
		{
			pthread_mutex_unlock(&decode_lock);
			continue;
		}
		textures[asset].decode_state = _decode_decoding;
		pthread_mutex_unlock(&decode_lock);
		pixels = png_decode((const unsigned char *)embedded->png, embedded->png_size, &width, &height);
		pthread_mutex_lock(&decode_lock);
		textures[asset].pixels = pixels;
		textures[asset].width = width;
		textures[asset].height = height;
		textures[asset].decode_state = _decode_done;
		pthread_cond_broadcast(&decode_finished);
		pthread_mutex_unlock(&decode_lock);
	}
	pthread_mutex_lock(&decode_lock);
	decode_running = 0;
	pthread_cond_broadcast(&decode_finished);
	pthread_mutex_unlock(&decode_lock);
	return NULL;
}

void hud_hires_map_unloaded(void)
{
	long asset;

	pthread_mutex_lock(&decode_lock);
	decode_stopping = 1;
	while (decode_running)
		pthread_cond_wait(&decode_finished, &decode_lock);
	pthread_mutex_unlock(&decode_lock);
	for (asset = 0; asset < MAXIMUM_TEXTURES; asset++)
	{
		free(textures[asset].pixels);
		textures[asset].pixels = NULL;
		textures[asset].decode_state = _decode_none;
	}
}

void hud_hires_map_loaded(const long *assets, long count)
{
	long index, queued = 0;
	pthread_t thread;

	hud_hires_map_unloaded();
#if defined(HALO_SERVER) || defined(HALO_ANDROID)
	/* (the dedicated server draws nothing; Android, whose memory is the
	tightest, decodes each when first drawn: holding a map's decoded HUD
	pictures takes about 130 MB more at once) */
	(void)assets;
	(void)count;
	return;
#endif
	/* (none for a test's run that draws nothing, or with the high-res HUD
	off: then each is decoded when first drawn, as titles are) */
	if (!config_boolean("display.high_res_hud") || config_boolean("debug.null_renderer"))
		return;
	for (index = 0; index < count; index++)
	{
		long asset = assets[index];

		if (asset >= 0 && asset < hud_hires_asset_count() && !hud_hires_embedded[asset].title &&
			hud_hires_embedded[asset].png &&
			!textures[asset].texture && !textures[asset].failed)
		{
			textures[asset].decode_state = _decode_queued;
			queued++;
		}
	}
	if (!queued)
		return;
	decode_stopping = 0;
	decode_running = 1;
	if (pthread_create(&thread, NULL, decode_thread_main, NULL) == 0)
	{
		pthread_detach(thread);
	}
	else
	{
		decode_running = 0;
		for (index = 0; index < MAXIMUM_TEXTURES; index++)
			textures[index].decode_state = _decode_none;
	}
}
