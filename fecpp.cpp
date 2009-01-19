/*
 * Forward error correction based on Vandermonde matrices
 *
 * (C) 1997-98 Luigi Rizzo (luigi@iet.unipi.it)
 * (C) 2009 Jack Lloyd (lloyd@randombit.net)
 *
 * Distributed under the terms given in license.txt
 */

#include "fecpp.h"
#include <stdexcept>
#include <vector>
#include <cstring>

#if defined(__SSE2__)
  #include <emmintrin.h>
#endif

namespace fecpp {

namespace {

/* Tables for arithetic in GF(2^8) using 1+x^2+x^3+x^4+x^8
 *
 * See Lin & Costello, Appendix A, and Lee & Messerschmitt, p. 453.
 *
 * Generate GF(2**m) from the irreducible polynomial p(X) in p[0]..p[m]
 * Lookup tables:
 *     index->polynomial form           gf_exp[] contains j= \alpha^i;
 *     polynomial form -> index form    gf_log[ j = \alpha^i ] = i
 * \alpha=x is the primitive element of GF(2^m)
 *
 * For efficiency, gf_exp[] has size 2*GF_SIZE, so that a simple
 * multiplication of two numbers can be resolved without calling mod
 */
const byte GF_EXP[510] = {
0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1D, 0x3A, 0x74,
0xE8, 0xCD, 0x87, 0x13, 0x26, 0x4C, 0x98, 0x2D, 0x5A, 0xB4, 0x75,
0xEA, 0xC9, 0x8F, 0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0xC0, 0x9D,
0x27, 0x4E, 0x9C, 0x25, 0x4A, 0x94, 0x35, 0x6A, 0xD4, 0xB5, 0x77,
0xEE, 0xC1, 0x9F, 0x23, 0x46, 0x8C, 0x05, 0x0A, 0x14, 0x28, 0x50,
0xA0, 0x5D, 0xBA, 0x69, 0xD2, 0xB9, 0x6F, 0xDE, 0xA1, 0x5F, 0xBE,
0x61, 0xC2, 0x99, 0x2F, 0x5E, 0xBC, 0x65, 0xCA, 0x89, 0x0F, 0x1E,
0x3C, 0x78, 0xF0, 0xFD, 0xE7, 0xD3, 0xBB, 0x6B, 0xD6, 0xB1, 0x7F,
0xFE, 0xE1, 0xDF, 0xA3, 0x5B, 0xB6, 0x71, 0xE2, 0xD9, 0xAF, 0x43,
0x86, 0x11, 0x22, 0x44, 0x88, 0x0D, 0x1A, 0x34, 0x68, 0xD0, 0xBD,
0x67, 0xCE, 0x81, 0x1F, 0x3E, 0x7C, 0xF8, 0xED, 0xC7, 0x93, 0x3B,
0x76, 0xEC, 0xC5, 0x97, 0x33, 0x66, 0xCC, 0x85, 0x17, 0x2E, 0x5C,
0xB8, 0x6D, 0xDA, 0xA9, 0x4F, 0x9E, 0x21, 0x42, 0x84, 0x15, 0x2A,
0x54, 0xA8, 0x4D, 0x9A, 0x29, 0x52, 0xA4, 0x55, 0xAA, 0x49, 0x92,
0x39, 0x72, 0xE4, 0xD5, 0xB7, 0x73, 0xE6, 0xD1, 0xBF, 0x63, 0xC6,
0x91, 0x3F, 0x7E, 0xFC, 0xE5, 0xD7, 0xB3, 0x7B, 0xF6, 0xF1, 0xFF,
0xE3, 0xDB, 0xAB, 0x4B, 0x96, 0x31, 0x62, 0xC4, 0x95, 0x37, 0x6E,
0xDC, 0xA5, 0x57, 0xAE, 0x41, 0x82, 0x19, 0x32, 0x64, 0xC8, 0x8D,
0x07, 0x0E, 0x1C, 0x38, 0x70, 0xE0, 0xDD, 0xA7, 0x53, 0xA6, 0x51,
0xA2, 0x59, 0xB2, 0x79, 0xF2, 0xF9, 0xEF, 0xC3, 0x9B, 0x2B, 0x56,
0xAC, 0x45, 0x8A, 0x09, 0x12, 0x24, 0x48, 0x90, 0x3D, 0x7A, 0xF4,
0xF5, 0xF7, 0xF3, 0xFB, 0xEB, 0xCB, 0x8B, 0x0B, 0x16, 0x2C, 0x58,
0xB0, 0x7D, 0xFA, 0xE9, 0xCF, 0x83, 0x1B, 0x36, 0x6C, 0xD8, 0xAD,
0x47, 0x8E,

0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1D, 0x3A, 0x74,
0xE8, 0xCD, 0x87, 0x13, 0x26, 0x4C, 0x98, 0x2D, 0x5A, 0xB4, 0x75,
0xEA, 0xC9, 0x8F, 0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0xC0, 0x9D,
0x27, 0x4E, 0x9C, 0x25, 0x4A, 0x94, 0x35, 0x6A, 0xD4, 0xB5, 0x77,
0xEE, 0xC1, 0x9F, 0x23, 0x46, 0x8C, 0x05, 0x0A, 0x14, 0x28, 0x50,
0xA0, 0x5D, 0xBA, 0x69, 0xD2, 0xB9, 0x6F, 0xDE, 0xA1, 0x5F, 0xBE,
0x61, 0xC2, 0x99, 0x2F, 0x5E, 0xBC, 0x65, 0xCA, 0x89, 0x0F, 0x1E,
0x3C, 0x78, 0xF0, 0xFD, 0xE7, 0xD3, 0xBB, 0x6B, 0xD6, 0xB1, 0x7F,
0xFE, 0xE1, 0xDF, 0xA3, 0x5B, 0xB6, 0x71, 0xE2, 0xD9, 0xAF, 0x43,
0x86, 0x11, 0x22, 0x44, 0x88, 0x0D, 0x1A, 0x34, 0x68, 0xD0, 0xBD,
0x67, 0xCE, 0x81, 0x1F, 0x3E, 0x7C, 0xF8, 0xED, 0xC7, 0x93, 0x3B,
0x76, 0xEC, 0xC5, 0x97, 0x33, 0x66, 0xCC, 0x85, 0x17, 0x2E, 0x5C,
0xB8, 0x6D, 0xDA, 0xA9, 0x4F, 0x9E, 0x21, 0x42, 0x84, 0x15, 0x2A,
0x54, 0xA8, 0x4D, 0x9A, 0x29, 0x52, 0xA4, 0x55, 0xAA, 0x49, 0x92,
0x39, 0x72, 0xE4, 0xD5, 0xB7, 0x73, 0xE6, 0xD1, 0xBF, 0x63, 0xC6,
0x91, 0x3F, 0x7E, 0xFC, 0xE5, 0xD7, 0xB3, 0x7B, 0xF6, 0xF1, 0xFF,
0xE3, 0xDB, 0xAB, 0x4B, 0x96, 0x31, 0x62, 0xC4, 0x95, 0x37, 0x6E,
0xDC, 0xA5, 0x57, 0xAE, 0x41, 0x82, 0x19, 0x32, 0x64, 0xC8, 0x8D,
0x07, 0x0E, 0x1C, 0x38, 0x70, 0xE0, 0xDD, 0xA7, 0x53, 0xA6, 0x51,
0xA2, 0x59, 0xB2, 0x79, 0xF2, 0xF9, 0xEF, 0xC3, 0x9B, 0x2B, 0x56,
0xAC, 0x45, 0x8A, 0x09, 0x12, 0x24, 0x48, 0x90, 0x3D, 0x7A, 0xF4,
0xF5, 0xF7, 0xF3, 0xFB, 0xEB, 0xCB, 0x8B, 0x0B, 0x16, 0x2C, 0x58,
0xB0, 0x7D, 0xFA, 0xE9, 0xCF, 0x83, 0x1B, 0x36, 0x6C, 0xD8, 0xAD,
0x47, 0x8E };

const byte GF_LOG[256] = {
0xFF, 0x00, 0x01, 0x19, 0x02, 0x32, 0x1A, 0xC6, 0x03, 0xDF, 0x33,
0xEE, 0x1B, 0x68, 0xC7, 0x4B, 0x04, 0x64, 0xE0, 0x0E, 0x34, 0x8D,
0xEF, 0x81, 0x1C, 0xC1, 0x69, 0xF8, 0xC8, 0x08, 0x4C, 0x71, 0x05,
0x8A, 0x65, 0x2F, 0xE1, 0x24, 0x0F, 0x21, 0x35, 0x93, 0x8E, 0xDA,
0xF0, 0x12, 0x82, 0x45, 0x1D, 0xB5, 0xC2, 0x7D, 0x6A, 0x27, 0xF9,
0xB9, 0xC9, 0x9A, 0x09, 0x78, 0x4D, 0xE4, 0x72, 0xA6, 0x06, 0xBF,
0x8B, 0x62, 0x66, 0xDD, 0x30, 0xFD, 0xE2, 0x98, 0x25, 0xB3, 0x10,
0x91, 0x22, 0x88, 0x36, 0xD0, 0x94, 0xCE, 0x8F, 0x96, 0xDB, 0xBD,
0xF1, 0xD2, 0x13, 0x5C, 0x83, 0x38, 0x46, 0x40, 0x1E, 0x42, 0xB6,
0xA3, 0xC3, 0x48, 0x7E, 0x6E, 0x6B, 0x3A, 0x28, 0x54, 0xFA, 0x85,
0xBA, 0x3D, 0xCA, 0x5E, 0x9B, 0x9F, 0x0A, 0x15, 0x79, 0x2B, 0x4E,
0xD4, 0xE5, 0xAC, 0x73, 0xF3, 0xA7, 0x57, 0x07, 0x70, 0xC0, 0xF7,
0x8C, 0x80, 0x63, 0x0D, 0x67, 0x4A, 0xDE, 0xED, 0x31, 0xC5, 0xFE,
0x18, 0xE3, 0xA5, 0x99, 0x77, 0x26, 0xB8, 0xB4, 0x7C, 0x11, 0x44,
0x92, 0xD9, 0x23, 0x20, 0x89, 0x2E, 0x37, 0x3F, 0xD1, 0x5B, 0x95,
0xBC, 0xCF, 0xCD, 0x90, 0x87, 0x97, 0xB2, 0xDC, 0xFC, 0xBE, 0x61,
0xF2, 0x56, 0xD3, 0xAB, 0x14, 0x2A, 0x5D, 0x9E, 0x84, 0x3C, 0x39,
0x53, 0x47, 0x6D, 0x41, 0xA2, 0x1F, 0x2D, 0x43, 0xD8, 0xB7, 0x7B,
0xA4, 0x76, 0xC4, 0x17, 0x49, 0xEC, 0x7F, 0x0C, 0x6F, 0xF6, 0x6C,
0xA1, 0x3B, 0x52, 0x29, 0x9D, 0x55, 0xAA, 0xFB, 0x60, 0x86, 0xB1,
0xBB, 0xCC, 0x3E, 0x5A, 0xCB, 0x59, 0x5F, 0xB0, 0x9C, 0xA9, 0xA0,
0x51, 0x0B, 0xF5, 0x16, 0xEB, 0x7A, 0x75, 0x2C, 0xD7, 0x4F, 0xAE,
0xD5, 0xE9, 0xE6, 0xE7, 0xAD, 0xE8, 0x74, 0xD6, 0xF4, 0xEA, 0xA8,
0x50, 0x58, 0xAF };

const byte GF_INVERSE[256] = {
0x00, 0x01, 0x8E, 0xF4, 0x47, 0xA7, 0x7A, 0xBA, 0xAD, 0x9D, 0xDD,
0x98, 0x3D, 0xAA, 0x5D, 0x96, 0xD8, 0x72, 0xC0, 0x58, 0xE0, 0x3E,
0x4C, 0x66, 0x90, 0xDE, 0x55, 0x80, 0xA0, 0x83, 0x4B, 0x2A, 0x6C,
0xED, 0x39, 0x51, 0x60, 0x56, 0x2C, 0x8A, 0x70, 0xD0, 0x1F, 0x4A,
0x26, 0x8B, 0x33, 0x6E, 0x48, 0x89, 0x6F, 0x2E, 0xA4, 0xC3, 0x40,
0x5E, 0x50, 0x22, 0xCF, 0xA9, 0xAB, 0x0C, 0x15, 0xE1, 0x36, 0x5F,
0xF8, 0xD5, 0x92, 0x4E, 0xA6, 0x04, 0x30, 0x88, 0x2B, 0x1E, 0x16,
0x67, 0x45, 0x93, 0x38, 0x23, 0x68, 0x8C, 0x81, 0x1A, 0x25, 0x61,
0x13, 0xC1, 0xCB, 0x63, 0x97, 0x0E, 0x37, 0x41, 0x24, 0x57, 0xCA,
0x5B, 0xB9, 0xC4, 0x17, 0x4D, 0x52, 0x8D, 0xEF, 0xB3, 0x20, 0xEC,
0x2F, 0x32, 0x28, 0xD1, 0x11, 0xD9, 0xE9, 0xFB, 0xDA, 0x79, 0xDB,
0x77, 0x06, 0xBB, 0x84, 0xCD, 0xFE, 0xFC, 0x1B, 0x54, 0xA1, 0x1D,
0x7C, 0xCC, 0xE4, 0xB0, 0x49, 0x31, 0x27, 0x2D, 0x53, 0x69, 0x02,
0xF5, 0x18, 0xDF, 0x44, 0x4F, 0x9B, 0xBC, 0x0F, 0x5C, 0x0B, 0xDC,
0xBD, 0x94, 0xAC, 0x09, 0xC7, 0xA2, 0x1C, 0x82, 0x9F, 0xC6, 0x34,
0xC2, 0x46, 0x05, 0xCE, 0x3B, 0x0D, 0x3C, 0x9C, 0x08, 0xBE, 0xB7,
0x87, 0xE5, 0xEE, 0x6B, 0xEB, 0xF2, 0xBF, 0xAF, 0xC5, 0x64, 0x07,
0x7B, 0x95, 0x9A, 0xAE, 0xB6, 0x12, 0x59, 0xA5, 0x35, 0x65, 0xB8,
0xA3, 0x9E, 0xD2, 0xF7, 0x62, 0x5A, 0x85, 0x7D, 0xA8, 0x3A, 0x29,
0x71, 0xC8, 0xF6, 0xF9, 0x43, 0xD7, 0xD6, 0x10, 0x73, 0x76, 0x78,
0x99, 0x0A, 0x19, 0x91, 0x14, 0x3F, 0xE6, 0xF0, 0x86, 0xB1, 0xE2,
0xF1, 0xFA, 0x74, 0xF3, 0xB4, 0x6D, 0x21, 0xB2, 0x6A, 0xE3, 0xE7,
0xB5, 0xEA, 0x03, 0x8F, 0xD3, 0xC9, 0x42, 0xD4, 0xE8, 0x75, 0x7F,
0xFF, 0x7E, 0xFD };

static byte GF_MUL_TABLE[256][256];

void init_fec()
   {
   static bool fec_initialized = false;

   if(!fec_initialized)
      {
      fec_initialized = true;

      for(size_t i = 0; i != 256; ++i)
         for(size_t j = 0; j != 256; ++j)
            GF_MUL_TABLE[i][j] = GF_EXP[(GF_LOG[i] + GF_LOG[j]) % 255];

      for(size_t i = 0; i != 256; ++i)
         GF_MUL_TABLE[0][i] = GF_MUL_TABLE[i][0] = 0;
      }
   }

/*
* addmul() computes z[] = z[] + x * y[]
*/
void addmul(byte z[], const byte x[], byte y, size_t size)
   {
   if(y == 0)
      return;

   const byte* GF_MUL_Y = GF_MUL_TABLE[y];

#if 0
   byte *lim = &z[size - 16 + 1];

   for(; z < lim; z += 16, x += 16)
      {
      z[0] ^= GF_MUL_Y[x[0]];
      z[1] ^= GF_MUL_Y[x[1]];
      z[2] ^= GF_MUL_Y[x[2]];
      z[3] ^= GF_MUL_Y[x[3]];
      z[4] ^= GF_MUL_Y[x[4]];
      z[5] ^= GF_MUL_Y[x[5]];
      z[6] ^= GF_MUL_Y[x[6]];
      z[7] ^= GF_MUL_Y[x[7]];
      z[8] ^= GF_MUL_Y[x[8]];
      z[9] ^= GF_MUL_Y[x[9]];
      z[10] ^= GF_MUL_Y[x[10]];
      z[11] ^= GF_MUL_Y[x[11]];
      z[12] ^= GF_MUL_Y[x[12]];
      z[13] ^= GF_MUL_Y[x[13]];
      z[14] ^= GF_MUL_Y[x[14]];
      z[15] ^= GF_MUL_Y[x[15]];
      }

   lim += 16 - 1;
   for(; z < lim; z++, x++)
      *z ^= GF_MUL_Y[*x];
#else

   while(size && (uintptr_t)z % 16) // align z to 16 bytes
      {
      z[0] ^= GF_MUL_Y[x[0]];
      ++z;
      ++x;
      size--;
      }

   const size_t blocks_64 = size - (size % 64);

   const __m128i polynomial = _mm_set1_epi8(0x1D);
   const __m128i all_zeros = _mm_setzero_si128();

   const size_t y_bits = 32 - __builtin_clz(y);

   // unrolled out to cache line size
   for(size_t i = 0; i != blocks_64; i += 64)
      {
      __m128i x_1 = _mm_loadu_si128((const __m128i*)(x));
      __m128i x_2 = _mm_loadu_si128((const __m128i*)(x + 16));
      __m128i x_3 = _mm_loadu_si128((const __m128i*)(x + 32));
      __m128i x_4 = _mm_loadu_si128((const __m128i*)(x + 48));

      __m128i z_1 = _mm_load_si128((const __m128i*)(z));
      __m128i z_2 = _mm_load_si128((const __m128i*)(z + 16));
      __m128i z_3 = _mm_load_si128((const __m128i*)(z + 32));
      __m128i z_4 = _mm_load_si128((const __m128i*)(z + 48));

      // prefetch next two x and z blocks
      _mm_prefetch(x + 64, _MM_HINT_T0);
      _mm_prefetch(z + 64, _MM_HINT_T0);
      _mm_prefetch(x + 128, _MM_HINT_T1);
      _mm_prefetch(z + 128, _MM_HINT_T1);

      if(y & 0x01)
         {
         z_1 = _mm_xor_si128(z_1, x_1);
         z_2 = _mm_xor_si128(z_2, x_2);
         z_3 = _mm_xor_si128(z_3, x_3);
         z_4 = _mm_xor_si128(z_4, x_4);
         }

      for(size_t j = 1; j != y_bits; ++j)
         {
         /*
         * Each byte of each mask is either 0 or the polynomial 0x1D,
         * depending on if the high bit of x_i is set or not.
         */

         __m128i mask_1 = _mm_cmpgt_epi8(all_zeros, x_1);
         __m128i mask_2 = _mm_cmpgt_epi8(all_zeros, x_2);
         __m128i mask_3 = _mm_cmpgt_epi8(all_zeros, x_3);
         __m128i mask_4 = _mm_cmpgt_epi8(all_zeros, x_4);

         x_1 = _mm_add_epi8(x_1, x_1);
         x_2 = _mm_add_epi8(x_2, x_2);
         x_3 = _mm_add_epi8(x_3, x_3);
         x_4 = _mm_add_epi8(x_4, x_4);

         mask_1 = _mm_and_si128(mask_1, polynomial);
         mask_2 = _mm_and_si128(mask_2, polynomial);
         mask_3 = _mm_and_si128(mask_3, polynomial);
         mask_4 = _mm_and_si128(mask_4, polynomial);

         x_1 = _mm_xor_si128(x_1, mask_1);
         x_2 = _mm_xor_si128(x_2, mask_2);
         x_3 = _mm_xor_si128(x_3, mask_3);
         x_4 = _mm_xor_si128(x_4, mask_4);

         if((y >> j) & 1)
            {
            z_1 = _mm_xor_si128(z_1, x_1);
            z_2 = _mm_xor_si128(z_2, x_2);
            z_3 = _mm_xor_si128(z_3, x_3);
            z_4 = _mm_xor_si128(z_4, x_4);
            }
         }

      _mm_store_si128((__m128i*)(z     ), z_1);
      _mm_store_si128((__m128i*)(z + 16), z_2);
      _mm_store_si128((__m128i*)(z + 32), z_3);
      _mm_store_si128((__m128i*)(z + 48), z_4);

      x += 64;
      z += 64;
      }

   for(size_t i = 0; i != size - blocks_64; ++i)
      {
      z[i] ^= GF_MUL_Y[x[i]];
      }

#endif
   }

/*
* invert_matrix() takes a K*K matrix and produces its inverse
* (Gauss-Jordan algorithm, adapted from Numerical Recipes in C)
*/
void invert_matrix(byte matrix[], size_t K)
   {
   class pivot_searcher
      {
      public:
         pivot_searcher(size_t K) : ipiv(K) {}

         std::pair<size_t, size_t> operator()(size_t col, const byte* matrix)
            {
            const size_t K = ipiv.size();

            if(ipiv[col] == false && matrix[col*K + col] != 0)
               {
               ipiv[col] = true;
               return std::make_pair(col, col);
               }

            for(size_t row = 0; row != K; ++row)
               {
               if(ipiv[row])
                  continue;

               for(size_t i = 0; i != K; ++i)
                  {
                  if(ipiv[i] == false && matrix[row*K + i] != 0)
                     {
                     ipiv[i] = true;
                     return std::make_pair(row, i);
                     }
                  }
               }

            throw std::invalid_argument("pivot not found in invert_matrix");
            }

      private:
         // Marks elements already used as pivots
         std::vector<bool> ipiv;
      };

   pivot_searcher pivot_search(K);
   std::vector<size_t> indxc(K);
   std::vector<size_t> indxr(K);
   std::vector<byte> id_row(K);

   for(size_t col = 0; col != K; ++col)
      {
      /*
      * Zeroing column 'col', look for a non-zero element.
      * First try on the diagonal, if it fails, look elsewhere.
      */

      std::pair<size_t, size_t> icolrow = pivot_search(col, matrix);

      size_t icol = icolrow.first;
      size_t irow = icolrow.second;

      /*
      * swap rows irow and icol, so afterwards the diagonal
      * element will be correct. Rarely done, not worth
      * optimizing.
      */
      if(irow != icol)
         {
         for(size_t i = 0; i != K; ++i)
            std::swap(matrix[irow*K + i], matrix[icol*K + i]);
         }

      indxr[col] = irow;
      indxc[col] = icol;
      byte* pivot_row = &matrix[icol*K];
      byte c = pivot_row[icol];

      if(c == 0)
         throw std::invalid_argument("singlar matrix");

      if(c != 1)
         { /* otherwhise this is a NOP */
         /*
         * this is done often, but optimizing is not so
         * fruitful, at least in the obvious ways (unrolling)
         */
         c = GF_INVERSE[c];
         pivot_row[icol] = 1;

         const byte* mul_c = GF_MUL_TABLE[c];

         for(size_t i = 0; i != K; ++i)
            pivot_row[i] = mul_c[pivot_row[i]];
         }

      /*
      * from all rows, remove multiples of the selected row
      * to zero the relevant entry (in fact, the entry is not zero
      * because we know it must be zero).
      * (Here, if we know that the pivot_row is the identity,
      * we can optimize the addmul).
      */
      id_row[icol] = 1;
      if(memcmp(pivot_row, &id_row[0], K) != 0)
         {
         byte* p = matrix;

         for(size_t i = 0; i != K; ++i)
            {
            if(i != icol)
               {
               c = p[icol];
               p[icol] = 0;
               addmul(p, pivot_row, c, K);
               }
            p += K;
            }
         }
      id_row[icol] = 0;
      } /* done all columns */

   for(size_t i = 0; i != K; ++i)
      {
      if(indxr[i] != indxc[i])
         {
         for(size_t row = 0; row != K; ++row)
            std::swap(matrix[row*K + indxr[i]], matrix[row*K + indxc[i]]);
         }
      }
   }

/*
* Generate and invert a Vandermonde matrix.
*
* Only uses the second column of the matrix, containing the p_i's
* (contents - 0, GF_EXP[0...n])
*
* Algorithm borrowed from "Numerical recipes in C", section 2.8, but
* largely revised for my purposes.
*
* p = coefficients of the matrix (p_i)
* q = values of the polynomial (known)
*/
void create_inverted_vdm(byte vdm[], size_t K)
   {
   if(K == 1) /* degenerate case, matrix must be p^0 = 1 */
      {
      vdm[0] = 1;
      return;
      }

   /*
   * c holds the coefficient of P(x) = Prod (x - p_i), i=0..K-1
   * b holds the coefficient for the matrix inversion
   */
   std::vector<byte> b(K), c(K);

   /*
   * construct coeffs. recursively. We know c[K] = 1 (implicit)
   * and start P_0 = x - p_0, then at each stage multiply by
   * x - p_i generating P_i = x P_{i-1} - p_i P_{i-1}
   * After K steps we are done.
   */
   c[K-1] = 0; /* really -p(0), but x = -x in GF(2^m) */
   for(size_t i = 1; i < K; ++i)
      {
      const byte* mul_p_i = GF_MUL_TABLE[GF_EXP[i]];

      for(size_t j = K-1  - (i - 1); j < K-1; ++j)
         c[j] ^= mul_p_i[c[j+1]];
      c[K-1] ^= GF_EXP[i];
      }

   for(size_t row = 0; row < K; ++row)
      {
      // synthetic division etc.
      const byte* mul_p_row = GF_MUL_TABLE[row == 0 ? 0 : GF_EXP[row]];

      byte t = 1;
      b[K-1] = 1; /* this is in fact c[K] */
      for(int i = K-2; i >= 0; i--)
         {
         b[i] = c[i+1] ^ mul_p_row[b[i+1]];
         t = b[i] ^ mul_p_row[t];
         }

      const byte* mul_t_inv = GF_MUL_TABLE[GF_INVERSE[t]];
      for(size_t col = 0; col != K; ++col)
         vdm[col*K + row] = mul_t_inv[b[col]];
      }
   }

}

/*
 * This section contains the proper FEC encoding/decoding routines.
 * The encoding matrix is computed starting with a Vandermonde matrix,
 * and then transforming it into a systematic matrix.
 */

/*
 * fec_code constructor
 */
fec_code::fec_code(size_t K_arg, size_t N_arg) :
   K(K_arg), N(N_arg), enc_matrix(N * K)
   {
   init_fec();

   if(K > 256 || N > 256)
      throw std::invalid_argument("fec_code: K and N must be < 256");

   if(K > N)
      throw std::invalid_argument("fec_code: K must be <= N");

   std::vector<byte> temp_matrix(N * K);

   /*
   * quick code to build systematic matrix: invert the top
   * K*K vandermonde matrix, multiply right the bottom n-K rows
   * by the inverse, and construct the identity matrix at the top.
   */
   create_inverted_vdm(&temp_matrix[0], K);

   for(size_t i = K*K; i != temp_matrix.size(); ++i)
      temp_matrix[i] = GF_EXP[((i / K) * (i % K)) % 255];

   /*
   * the upper part of the encoding matrix is I
   */
   for(size_t i = 0; i != K; ++i)
      enc_matrix[i*(K+1)] = 1;

   /*
   * computes C = AB where A is n*K, B is K*m, C is n*m
   */
   for(size_t row = K*K; row != N*K; row += K)
      {
      for(size_t col = 0; col != K; ++col)
         {
         const byte* pa = &temp_matrix[row];
         const byte* pb = &temp_matrix[col];
         byte acc = 0;
         for(size_t i = 0; i < K; i++, pa++, pb += K)
            acc ^= GF_MUL_TABLE[*pa][*pb];
         enc_matrix[row + col] = acc;
         }
      }
   }

/*
* FEC encoding routine
*/
void fec_code::encode(
   const byte input[], size_t size,
   std::tr1::function<void (size_t, size_t, const byte[], size_t)> output)
   const
   {
   if(size % K != 0)
      throw std::invalid_argument("encode: input must be multiple of K bytes");

   size_t block_size = size / K;

   for(size_t i = 0; i != K; ++i)
      output(i, N, input + i*block_size, block_size);

#if 1
   for(size_t i = 0; i != K; ++i)
      output(i, N, input + i*block_size, block_size);

   for(size_t i = K; i != N; ++i)
      {
      std::vector<byte> fec_buf(block_size);

      for(size_t j = 0; j != K; ++j)
         addmul(&fec_buf[0], input + j*block_size,
                enc_matrix[i*K+j], block_size);

      output(i, N, &fec_buf[0], fec_buf.size());
      }
#else

   // align??
   std::vector<std::vector<byte> > fec_buf(N - K);

   for(size_t i = 0; i != fec_buf.size(); ++i)
      fec_buf[i].resize(block_size);

   /*
   for(size_t i = 0; i != K; ++i)
      {
      for(size_t j = K; j != N; ++j)
         addmul(&fec_buf[j-K][0], input + i*block_size,
                enc_matrix[j*K+i], block_size);
      }
   */

   size_t stride = block_size;

   while(stride > 16*1024)
      stride >>= 1;

   for(size_t i = 0; i != size; i += stride)
      {
      for(size_t j = K; j != N; ++j)
         addmul(&fec_buf[j-K][0], input + i,
                enc_matrix[j*K+i/block_size], stride);
      }

   for(size_t i = 0; i != fec_buf.size(); ++i)
      output(i+K, N, &fec_buf[i][0], fec_buf[i].size());
#endif
   }

/*
* FEC decoding routine
*/
void fec_code::decode(
   const std::map<size_t, const byte*>& shares,
   size_t share_size,
   std::tr1::function<void (size_t, size_t, const byte[], size_t)> output) const
   {
   /*
   Todo:
    If shares.size() < K:
          signal decoding error for missing shares < K
          emit existing shares < K
        (ie, partial recovery if possible)
    Assert share_size % K == 0
   */

   if(shares.size() < K)
      throw std::logic_error("Could not decode, less than K surviving shares");

   std::vector<byte> m_dec(K * K);
   std::vector<size_t> indexes(K);
   std::vector<const byte*> sharesv(K);

   std::map<size_t, const byte*>::const_iterator shares_b_iter =
      shares.begin();
   std::map<size_t, const byte*>::const_reverse_iterator shares_e_iter =
      shares.rbegin();

   for(size_t i = 0; i != K; ++i)
      {
      size_t share_id = 0;
      const byte* share_data = 0;

      if(shares_b_iter->first == i)
         {
         share_id = shares_b_iter->first;
         share_data = shares_b_iter->second;
         ++shares_b_iter;
         }
      else
         {
         // if share i not found, use the unused one closest to n
         share_id = shares_e_iter->first;
         share_data = shares_e_iter->second;
         ++shares_e_iter;
         }

      if(share_id >= N)
         throw std::logic_error("Invalid share id detected during decode");

      /*
      This is a systematic code (encoding matrix includes K*K identity
      matrix), so shares less than K are copies of the input data,
      can output directly. Also we know the encoding matrix in those rows
      contains I, so we can set the single bit directly without copying
      */
      if(share_id < K)
         {
         m_dec[i*(K+1)] = 1;
         output(share_id, K, share_data, share_size);
         }
      else // will decode after inverting matrix
         std::memcpy(&m_dec[i*K], &(enc_matrix[share_id*K]), K);

      sharesv[i] = share_data;
      indexes[i] = share_id;
      }

   /*
   TODO: if all primary shares were recovered, don't invert the matrix
   and return immediately
   */
   invert_matrix(&m_dec[0], K);

   for(size_t i = 0; i != indexes.size(); ++i)
      {
      if(indexes[i] >= K)
         {
         std::vector<byte> buf(share_size);
         for(size_t col = 0; col != K; ++col)
            addmul(&buf[0], sharesv[col], m_dec[i*K + col], share_size);
         output(i, K, &buf[0], share_size);
         }
      }
   }

}
