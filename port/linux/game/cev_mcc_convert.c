/*
CEV_MCC_CONVERT.C

What the Custom Edition copy's MCC bundles (cev/mcc_tag_bundle.c) ask of the MCC bitmap and sound conversions:
a bitmap level decoded to texels, BC7 blocks made DXT5, and a permutation's samples made the samples this build
plays, each from the conversion kernels (cev/mcc_bitmap_kernels.h, cev/mcc_audio_kernels.h).
*/

#include "cseries.h"
#include "bitmaps/bitmaps.h"

#include "cev/mcc_bitmap_kernels.h"
#include "cev/mcc_audio_kernels.h"

int mcc_bitmaps_decode_texels(void const *input, uint32_t input_bytes, uint32_t *output, unsigned width,
    unsigned height, short format);
void mcc_bitmaps_bc7_transcode(unsigned char *blocks, uint32_t bytes);
unsigned char *mcc_audio_bundle_samples(unsigned char const *data, uint32_t size, int compression, int channels,
    uint32_t rate, uint32_t *bytes, int *kept);

/* (port) A level of a bitmap's pixels as 0xAARRGGBB texels, the top row
 * first, for the menus' previews (mcc_tag_preview.c): no loading pulse, no
 * channel reordering. 0 for a format not decoded, or too few bytes. */
int mcc_bitmaps_decode_texels(void const *input,uint32_t input_bytes,uint32_t *output,
    unsigned width,unsigned height,short format)
{
    unsigned char const *source=input;
    int bytes_per_pixel=mcc_bitmap_bytes(format);
    uint64_t need;
    unsigned x,y;
    if (!source || !output || !width || !height) return 0;
    if (bytes_per_pixel) need=(uint64_t)width*height*(unsigned)bytes_per_pixel;
    else if (format>=14 && format<=18 && format!=17) need=(uint64_t)((width+3)/4)*((height+3)/4)*(format==14 ? 8 : 16);
    else return 0;
    if (need>input_bytes) return 0;
    if (bytes_per_pixel) {
        for (x=0;x<width*height;x++)
            output[x]=mcc_pixel(source+x*(unsigned)bytes_per_pixel,format,(uint32_t const *)global_vector_palette);
        return 1;
    }
    for (y=0;y<height;y+=4) for (x=0;x<width;x+=4) {
        unsigned char rgba[64];
        unsigned dx,dy;
        if (format==14) bcdec_bc1(source,rgba,16);
        else if (format==15) bcdec_bc2(source,rgba,16);
        else if (format==16) bcdec_bc3(source,rgba,16);
        else bcdec_bc7(source,rgba,16);
        source+=format==14 ? 8 : 16;
        for (dy=0;dy<4 && y+dy<height;dy++) for (dx=0;dx<4 && x+dx<width;dx++) {
            unsigned char const *p=rgba+(dy*4+dx)*4;
            output[(y+dy)*width+x+dx]=p[2]|((uint32_t)p[1]<<8)|((uint32_t)p[0]<<16)|((uint32_t)p[3]<<24);
        }
    }
    return 1;
}

/* (port) BC7 blocks made DXT5 in place, as a texture drawn as it is keeps them
 * (mcc_bc7_to_dxt5): the Tag Injector's bundles of an MCC map's tags, whose
 * bitmaps are Halo PC's (mcc_tag_bundle.c). Of `bytes`, whole blocks alone */
void mcc_bitmaps_bc7_transcode(unsigned char *blocks,uint32_t bytes)
{
    uint32_t block;
    for (block=0;blocks && block<bytes/16;block++) mcc_bc7_to_dxt5(blocks+(size_t)block*16);
}

/* (port) A permutation's samples (`size` bytes of `compression`, `channels`
 * channels at `rate`) as this build plays them, as mcc_audio_prepare_serial
 * makes them: IMA ADPCM at the native rate (mono at 22,050 Hz, stereo at its
 * own), a buffer of *bytes the caller frees; or, ADPCM at that rate already,
 * NULL with *kept set (the samples are kept as they are). NULL, *kept clear,
 * when they cannot be decoded. The Tag Injector's (mcc_tag_bundle.c) */
unsigned char *mcc_audio_bundle_samples(unsigned char const *data,uint32_t size,int compression,
    int channels,uint32_t rate,uint32_t *bytes,int *kept)
{
    struct mcc_pcm pcm;
    uint32_t output_rate=channels==1 ? 22050 : rate,frames=0,encoded;
    unsigned char *output=NULL;
    *bytes=0;*kept=0;
    if (!mcc_pcm_open(data,size,compression,channels,rate,&pcm)) return NULL;
    if (compression==1 && rate==output_rate) {
        mcc_pcm_close(&pcm);
        *kept=1;
        return NULL;
    }
    encoded=mcc_ima_encoded_size(&pcm,channels,output_rate,&frames);
    output=encoded ? malloc(encoded) : NULL;
    if (output) {
        mcc_ima_encode(&pcm,channels,output_rate,frames,output,NULL);
        *bytes=encoded;
    }
    mcc_pcm_close(&pcm);
    return output;
}
