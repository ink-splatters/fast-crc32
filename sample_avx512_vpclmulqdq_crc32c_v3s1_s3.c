/* Generated by https://github.com/corsix/fast-crc32/ using: */
/* ./generate -i avx512_vpclmulqdq -p crc32c -a v3s1_s3 */
/* MIT licensed */

#include <stddef.h>
#include <stdint.h>
#include <nmmintrin.h>
#include <immintrin.h>
#include <wmmintrin.h>

#if defined(_MSC_VER)
#define CRC_AINLINE static __forceinline
#define CRC_ALIGN(n) __declspec(align(n))
#else
#define CRC_AINLINE static __inline __attribute__((always_inline))
#define CRC_ALIGN(n) __attribute__((aligned(n)))
#endif
#define CRC_EXPORT extern

#define clmul_lo(a, b) (_mm512_clmulepi64_epi128((a), (b), 0))
#define clmul_hi(a, b) (_mm512_clmulepi64_epi128((a), (b), 17))

CRC_AINLINE __m128i clmul_scalar(uint32_t a, uint32_t b) {
  return _mm_clmulepi64_si128(_mm_cvtsi32_si128(a), _mm_cvtsi32_si128(b), 0);
}

static uint32_t xnmodp(uint64_t n) /* x^n mod P, in log(n) time */ {
  uint64_t stack = ~(uint64_t)1;
  uint32_t acc, low;
  for (; n > 191; n = (n >> 1) - 16) {
    stack = (stack << 1) + (n & 1);
  }
  stack = ~stack;
  acc = ((uint32_t)0x80000000) >> (n & 31);
  for (n >>= 5; n; --n) {
    acc = _mm_crc32_u32(acc, 0);
  }
  while ((low = stack & 1), stack >>= 1) {
    __m128i x = _mm_cvtsi32_si128(acc);
    uint64_t y = _mm_cvtsi128_si64(_mm_clmulepi64_si128(x, x, 0));
    acc = _mm_crc32_u64(0, y << low);
  }
  return acc;
}

CRC_AINLINE __m128i crc_shift(uint32_t crc, size_t nbytes) {
  return clmul_scalar(crc, xnmodp(nbytes * 8 - 33));
}

CRC_EXPORT uint32_t crc32_impl(uint32_t crc0, const char* buf, size_t len) {
  crc0 = ~crc0;
  for (; len && ((uintptr_t)buf & 7); --len) {
    crc0 = _mm_crc32_u8(crc0, *buf++);
  }
  while (((uintptr_t)buf & 56) && len >= 8) {
    crc0 = _mm_crc32_u64(crc0, *(const uint64_t*)buf);
    buf += 8;
    len -= 8;
  }
  if (len >= 208) {
    size_t blk = (len - 8) / 200;
    size_t klen = blk * 8;
    const char* buf2 = buf + 0;
    uint64_t vc;
    __m128i z0;
    /* First vector chunk. */
    __m512i x0 = _mm512_loadu_si512((const void*)buf2), y0;
    __m512i x1 = _mm512_loadu_si512((const void*)(buf2 + 64)), y1;
    __m512i x2 = _mm512_loadu_si512((const void*)(buf2 + 128)), y2;
    __m512i k;
    k = _mm512_broadcast_i32x4(_mm_setr_epi32(0xa87ab8a8, 0, 0xab7aff2a, 0));
    x0 = _mm512_xor_si512(_mm512_castsi128_si512(_mm_cvtsi32_si128(crc0)), x0);
    crc0 = 0;
    buf2 += 192;
    len -= 200;
    buf += blk * 192;
    /* Main loop. */
    while (len >= 208) {
      y0 = clmul_lo(x0, k), x0 = clmul_hi(x0, k);
      y1 = clmul_lo(x1, k), x1 = clmul_hi(x1, k);
      y2 = clmul_lo(x2, k), x2 = clmul_hi(x2, k);
      x0 = _mm512_ternarylogic_epi64(x0, y0, _mm512_loadu_si512((const void*)buf2), 0x96);
      x1 = _mm512_ternarylogic_epi64(x1, y1, _mm512_loadu_si512((const void*)(buf2 + 64)), 0x96);
      x2 = _mm512_ternarylogic_epi64(x2, y2, _mm512_loadu_si512((const void*)(buf2 + 128)), 0x96);
      crc0 = _mm_crc32_u64(crc0, *(const uint64_t*)buf);
      buf += 8;
      buf2 += 192;
      len -= 200;
    }
    /* Reduce x0 ... x2 to just x0. */
    k = _mm512_broadcast_i32x4(_mm_setr_epi32(0x740eef02, 0, 0x9e4addf8, 0));
    y0 = clmul_lo(x0, k), x0 = clmul_hi(x0, k);
    x0 = _mm512_ternarylogic_epi64(x0, y0, x1, 0x96);
    x1 = x2;
    y0 = clmul_lo(x0, k), x0 = clmul_hi(x0, k);
    x0 = _mm512_ternarylogic_epi64(x0, y0, x1, 0x96);
    /* Final scalar chunk. */
    crc0 = _mm_crc32_u64(crc0, *(const uint64_t*)buf);
    buf += 8;
    vc = 0;
    /* Reduce 512 bits to 128 bits. */
    k = _mm512_setr_epi32(0x1c291d04, 0, 0xddc0152b, 0, 0x3da6d0cb, 0, 0xba4fc28e, 0, 0xf20c0dfe, 0, 0x493c7d27, 0, 0, 0, 0, 0);
    y0 = clmul_lo(x0, k), k = clmul_hi(x0, k);
    y0 = _mm512_xor_si512(y0, k);
    z0 = _mm_ternarylogic_epi64(_mm512_castsi512_si128(y0), _mm512_extracti32x4_epi32(y0, 1), _mm512_extracti32x4_epi32(y0, 2), 0x96);
    z0 = _mm_xor_si128(z0, _mm512_extracti32x4_epi32(x0, 3));
    /* Reduce 128 bits to 32 bits, and multiply by x^32. */
    vc ^= _mm_extract_epi64(crc_shift(_mm_crc32_u64(_mm_crc32_u64(0, _mm_extract_epi64(z0, 0)), _mm_extract_epi64(z0, 1)), klen * 1 + 8), 0);
    /* Final 8 bytes. */
    crc0 = _mm_crc32_u64(crc0, *(const uint64_t*)buf ^ vc), buf += 8;
    len -= 8;
  }
  if (len >= 32) {
    size_t klen = ((len - 8) / 24) * 8;
    uint32_t crc1 = 0;
    uint32_t crc2 = 0;
    __m128i vc0;
    __m128i vc1;
    uint64_t vc;
    /* Main loop. */
    do {
      crc0 = _mm_crc32_u64(crc0, *(const uint64_t*)buf);
      crc1 = _mm_crc32_u64(crc1, *(const uint64_t*)(buf + klen));
      crc2 = _mm_crc32_u64(crc2, *(const uint64_t*)(buf + klen * 2));
      buf += 8;
      len -= 24;
    } while (len >= 32);
    vc0 = crc_shift(crc0, klen * 2 + 8);
    vc1 = crc_shift(crc1, klen + 8);
    vc = _mm_extract_epi64(_mm_xor_si128(vc0, vc1), 0);
    /* Final 8 bytes. */
    buf += klen * 2;
    crc0 = crc2;
    crc0 = _mm_crc32_u64(crc0, *(const uint64_t*)buf ^ vc), buf += 8;
    len -= 8;
  }
  for (; len >= 8; buf += 8, len -= 8) {
    crc0 = _mm_crc32_u64(crc0, *(const uint64_t*)buf);
  }
  for (; len; --len) {
    crc0 = _mm_crc32_u8(crc0, *buf++);
  }
  return ~crc0;
}
