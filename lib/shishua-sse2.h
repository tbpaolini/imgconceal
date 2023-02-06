/* SHISHUA pseudo-random number generator
 * Origin: https://github.com/espadrine/shishua/blob/ed0d60a8b35fb6aba711dcbc24aaeda78c623b45/shishua-sse2.h
 * More info: https://espadrine.github.io/blog/posts/shishua-the-fastest-prng-in-the-world.html
 * License: Creative Commons Zero v1.0 Universal - https://github.com/espadrine/shishua/blob/master/LICENSE
 */

// An SSE2/SSSE3 version of shishua. Slower than AVX2, but more compatible.
// Also compatible with 32-bit x86.
//
// SSSE3 is recommended, as it has the useful _mm_alignr_epi8 intrinsic.
// We can still emulate it on SSE2, but it is slower.
// Consider the half version if AVX2 is not available.
#ifndef SHISHUA_SSE2_H
#define SHISHUA_SSE2_H
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
// Note: cl.exe doesn't define __SSSE3__
#if defined(__SSSE3__) || defined(__AVX__)
#  include <tmmintrin.h> // SSSE3
#  define SHISHUA_ALIGNR_EPI8(hi, lo, amt) \
   _mm_alignr_epi8(hi, lo, amt)
#else
#  include <emmintrin.h> // SSE2
// Emulate _mm_alignr_epi8 for SSE2. It's a little slow.
// The compiler may convert it to a sequence of shufps instructions, which is
// perfectly fine.
#  define SHISHUA_ALIGNR_EPI8(hi, lo, amt) \
   _mm_or_si128( \
     _mm_slli_si128(hi, 16 - (amt)), \
     _mm_srli_si128(lo, amt) \
   )
#endif

typedef struct prng_state {
  __m128i state[8];
  __m128i output[8];
  __m128i counter[2];
} prng_state;

// Wrappers for x86 targets which usually lack these intrinsics.
// Don't call these with side effects.
#if defined(__x86_64__) || defined(_M_X64)
#   define SHISHUA_SET_EPI64X(b, a) _mm_set_epi64x(b, a)
#   define SHISHUA_CVTSI64_SI128(x) _mm_cvtsi64_si128(x)
#else
#   define SHISHUA_SET_EPI64X(b, a) \
      _mm_set_epi32( \
        (int)(((uint64_t)(b)) >> 32), \
        (int)(b), \
        (int)(((uint64_t)(a)) >> 32), \
        (int)(a) \
      )
#   define SHISHUA_CVTSI64_SI128(x) SHISHUA_SET_EPI64X(0, x)
#endif

// buf could technically alias with prng_state, according to the compiler.
#if defined(__GNUC__) || defined(_MSC_VER)
#  define SHISHUA_RESTRICT __restrict
#else
#  define SHISHUA_RESTRICT
#endif

// buf's size must be a multiple of 128 bytes.
void prng_gen(prng_state *SHISHUA_RESTRICT s, uint8_t *SHISHUA_RESTRICT buf, size_t size);

// Nothing up my sleeve: those are the hex digits of Φ,
// the least approximable irrational number.
// $ echo 'scale=310;obase=16;(sqrt(5)-1)/2' | bc
static uint64_t phi[16];

void prng_init(prng_state *s, uint64_t seed[4]);

#undef SHISHUA_CVTSI64_SI128
#undef SHISHUA_ALIGNR_EPI8
#undef SHISHUA_SET_EPI64X
#undef SHISHUA_RESTRICT
#endif
