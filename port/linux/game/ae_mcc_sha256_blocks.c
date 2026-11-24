/*
AE_MCC_SHA256_BLOCKS.C

SHA-256 blocks with x86's SHA extensions (ae_mcc_sha256_blocks.h). The CPU is asked once (CPUID: SHA, SSE4.1, SSSE3);
without them, or on any other processor, nothing is done and mcc_sha256.c's portable rounds run as before. The state
is the eight words of FIPS 180-4 in order (A to H), as mcc_sha256.c keeps it.

The SHA instructions have only the old SSE encoding. When code before them on the thread has left the upper halves of
the AVX registers in use, each of them waits on that state: the rounds ran about 90 times slower in the game. So where
the CPU and the system have AVX on, VZEROUPPER clears those halves before the rounds.
*/

#include "ae_mcc_sha256_blocks.h"

#if defined(__x86_64__) || defined(__i386__)
#define AE_MCC_SHA256_X86 1
#include <cpuid.h>
#include <immintrin.h>
#endif

#ifdef AE_MCC_SHA256_X86
static const uint32_t ae_mcc_sha256_constants[64] =
{
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

/* the schedule's next four words for the group three on: w[i..i+3] from the four groups before */
#define AE_MCC_SHA256_NEXT(a, b, c, d) \
	(a) = _mm_sha256msg2_epu32(_mm_add_epi32(_mm_sha256msg1_epu32((a), (b)), _mm_alignr_epi8((d), (c), 4)), (d))

/* (the upper halves of the AVX registers cleared: AVX's own instruction, so only called where AVX is on) */
__attribute__((target("avx"), noinline))
static void ae_mcc_sha256_upper_clear(void)
{
	_mm256_zeroupper();
}

__attribute__((target("sha,sse4.1,ssse3")))
static void ae_mcc_sha256_blocks_x86(uint32_t state[8], unsigned char const *data, int blocks)
{
	__m128i const mask = _mm_set_epi64x(0x0c0d0e0f08090a0bLL, 0x0405060700010203LL);
	__m128i lower = _mm_loadu_si128((__m128i const *)state);
	__m128i state1 = _mm_loadu_si128((__m128i const *)(state + 4));
	__m128i state0;

	/* (the instructions keep the words as ABEF and CDGH) */
	lower = _mm_shuffle_epi32(lower, 0xB1);
	state1 = _mm_shuffle_epi32(state1, 0x1B);
	state0 = _mm_alignr_epi8(lower, state1, 8);
	state1 = _mm_blend_epi16(state1, lower, 0xF0);
	while (blocks-- > 0)
	{
		__m128i const abef = state0, cdgh = state1;
		__m128i words[4], message;
		int group;

		words[0] = _mm_shuffle_epi8(_mm_loadu_si128((__m128i const *)data), mask);
		words[1] = _mm_shuffle_epi8(_mm_loadu_si128((__m128i const *)(data + 16)), mask);
		words[2] = _mm_shuffle_epi8(_mm_loadu_si128((__m128i const *)(data + 32)), mask);
		words[3] = _mm_shuffle_epi8(_mm_loadu_si128((__m128i const *)(data + 48)), mask);
		for (group = 0; group < 16; group++)
		{
			message = _mm_add_epi32(words[group & 3],
				_mm_loadu_si128((__m128i const *)(ae_mcc_sha256_constants + group * 4)));
			state1 = _mm_sha256rnds2_epu32(state1, state0, message);
			message = _mm_shuffle_epi32(message, 0x0E);
			state0 = _mm_sha256rnds2_epu32(state0, state1, message);
			if (group < 12)
			{
				switch (group & 3)
				{
				case 0: AE_MCC_SHA256_NEXT(words[0], words[1], words[2], words[3]); break;
				case 1: AE_MCC_SHA256_NEXT(words[1], words[2], words[3], words[0]); break;
				case 2: AE_MCC_SHA256_NEXT(words[2], words[3], words[0], words[1]); break;
				default: AE_MCC_SHA256_NEXT(words[3], words[0], words[1], words[2]); break;
				}
			}
		}
		state0 = _mm_add_epi32(state0, abef);
		state1 = _mm_add_epi32(state1, cdgh);
		data += 64;
	}
	/* (back to A..D, E..H) */
	lower = _mm_shuffle_epi32(state0, 0x1B);
	state1 = _mm_shuffle_epi32(state1, 0xB1);
	state0 = _mm_blend_epi16(lower, state1, 0xF0);
	state1 = _mm_alignr_epi8(state1, lower, 8);
	_mm_storeu_si128((__m128i *)state, state0);
	_mm_storeu_si128((__m128i *)(state + 4), state1);
}
#endif

/* -1 not asked yet, else whether the CPU has the instructions (asked once; a second thread asking at the same time
writes the same answer); and whether AVX is on (the CPU has it and the system saves its registers) */
static volatile int ae_mcc_sha256_cpu = -1;
#ifdef AE_MCC_SHA256_X86
static volatile int ae_mcc_sha256_avx;
#endif

int ae_mcc_sha256_hardware(void)
{
	if (ae_mcc_sha256_cpu < 0)
	{
		int found = 0;
#ifdef AE_MCC_SHA256_X86
		unsigned int a, b, c, d;

		if (__get_cpuid_count(7, 0, &a, &b, &c, &d) && (b & (1u << 29)) &&
			__get_cpuid(1, &a, &b, &c, &d) && (c & (1u << 19)) && (c & (1u << 9)))
		{
			found = 1;
			/* (AVX: CPUID's AVX and OSXSAVE bits, and XCR0's SSE and AVX state saved by the system) */
			if ((c & (1u << 28)) && (c & (1u << 27)))
			{
				unsigned int low, high;

				__asm__ volatile("xgetbv" : "=a"(low), "=d"(high) : "c"(0));
				(void)high;
				ae_mcc_sha256_avx = (low & 6u) == 6u;
			}
		}
#endif
		ae_mcc_sha256_cpu = found;
	}
	return ae_mcc_sha256_cpu;
}

static volatile unsigned long ae_mcc_sha256_done;

unsigned long ae_mcc_sha256_hardware_blocks(void)
{
	return ae_mcc_sha256_done;
}

int ae_mcc_sha256_blocks(uint32_t state[8], unsigned char const *data, int blocks)
{
	if (blocks <= 0 || !ae_mcc_sha256_hardware())
		return 0;
#ifdef AE_MCC_SHA256_X86
	if (ae_mcc_sha256_avx)
		ae_mcc_sha256_upper_clear();
	ae_mcc_sha256_blocks_x86(state, data, blocks);
	ae_mcc_sha256_done += (unsigned long)blocks;
	return blocks;
#else
	(void)state;
	(void)data;
	return 0;
#endif
}
