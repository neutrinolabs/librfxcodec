/**
 * RemoteFX Codec Library
 *
 * Copyright 2024 Jay Sorg <jay.sorg@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * DWT Reduce-Extrapolate Method MS-RDPEGFX 3.2.8.1.2.2
 * also does Quantization and Linearization 3.2.8.1.3
 * SSE2 accel
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <emmintrin.h>

#include "rfxcommon.h"
#include "rfxencode_dwt_shift_rem.h"
#include "rfxencode_dwt_shift_rem_sse2.h"
#include "rfxencode_dwt_shift_rem_common.h"

/* level 1      LL0 -> L0, H0 */
#define IC_LL0_U8V(_val, _offset) \
    _val = _mm_slli_epi16(_mm_sub_epi16(_mm_unpacklo_epi8( \
        _mm_loadl_epi64((__m128i_u const *)(ic + (_offset) * 64)), \
        g_vec_zerov), g_vec_128v), DWT_FACTOR)
#define OC_L0V(_offset, _val) \
    _mm_storeu_si128((__m128i *)(lo + (_offset) * 64), _val)
#define OC_H0V(_offset, _val) \
    _mm_storeu_si128((__m128i *)(hi + (_offset) * 64), _val)
/*              L0 -> LL1, HL1 */
#define IC_L0V(_x2nv, _x2n1v, _x2n2v, _offset) do { \
    v1 = _mm_loadu_si128((const __m128i *)(ic + 2 * (_offset) + 0)); \
    v2 = _mm_loadu_si128((const __m128i *)(ic + 2 * (_offset) + 8)); \
    _x2nv = _mm_packs_epi32(_mm_srai_epi32(_mm_slli_epi32(v1, 16), 16), \
                            _mm_srai_epi32(_mm_slli_epi32(v2, 16), 16)); \
    _x2n1v = _mm_packs_epi32(_mm_srai_epi32( \
                             _mm_slli_epi32(_mm_srli_si128(v1, 2), 16), 16), \
                             _mm_srai_epi32( \
                             _mm_slli_epi32(_mm_srli_si128(v2, 2), 16), 16)); \
    _x2n2v = _mm_insert_epi16(_mm_srli_si128(_x2nv, 2), \
                              ic[(_offset) * 2 + 16], 7);  \
} while (0)
#define IC_L0V_POST(_x2nv, _x2n1v, _x2n2v, _offset) do { \
    v1 = _mm_loadu_si128((const __m128i *)(ic + 2 * (_offset))); \
    v2 = _mm_loadu_si128((const __m128i *)(ic + 2 * (_offset) + 8)); \
    _x2nv = _mm_packs_epi32(_mm_srai_epi32(_mm_slli_epi32(v1, 16), 16), \
                            _mm_srai_epi32(_mm_slli_epi32(v2, 16), 16)); \
    _x2n1v = _mm_packs_epi32(_mm_srai_epi32( \
                             _mm_slli_epi32(_mm_srli_si128(v1, 2), 16), 16), \
                             _mm_srai_epi32( \
                             _mm_slli_epi32(_mm_srli_si128(v2, 2), 16), 16)); \
    /* ic[64] = 2 * ic[63] - ic[62] */ \
    ic64 = 2 * ic[(_offset) * 2 + 15] - ic[(_offset) * 2 + 14]; \
    _x2n2v = _mm_insert_epi16(_mm_srli_si128(_x2nv, 2), ic64, 7); \
} while (0)
#define OC_LL1V(_offset, _val) \
    _mm_storeu_si128((__m128i *)(lo + (_offset)), _val)
#define OC_HL1V(_offset, _val) \
    _mm_storeu_si128((__m128i *)(hi + (_offset)), _val)
/*              H0 -> LH1, HH1 */
#define IC_H0V_PRE IC_L0V_PRE
#define IC_H0V IC_L0V
#define IC_H0V_LOOP IC_L0V_LOOP
#define IC_H0V_POST IC_L0V_POST
#define OC_LH1V OC_LL1V
#define OC_HH1V OC_HL1V

/* level 2      LL1 -> L1, H1 */
#define IC_LL1V(_val, _offset) \
    _val = _mm_loadu_si128((const __m128i *)(ic + (_offset) * 33))
#define OC_L1V(_offset, _val) \
    _mm_storeu_si128((__m128i *)(lo + (_offset) * 33), _val)
#define OC_H1V(_offset, _val) \
    _mm_storeu_si128((__m128i *)(hi + (_offset) * 33), _val)
/*              L1 -> LL2, HL2 */
#define IC_L1V IC_L0V
#define OC_LL2V OC_LL1V
#define OC_HL2V OC_HL1V
/*              H1 -> LH2, HH2 */
#define IC_H1V IC_L0V
#define OC_LH2V OC_LL1V
#define OC_HH2V OC_HL1V

/* level 3      LL2 -> L2, H2 */
#define IC_LL2V(_val, _offset) \
    _val = _mm_loadu_si128((const __m128i *)(ic + (_offset) * 17))
#define OC_L2V(_offset, _val) \
    _mm_storeu_si128((__m128i *)(lo + (_offset) * 17), _val)
#define OC_H2V(_offset, _val) \
    _mm_storeu_si128((__m128i *)(hi + (_offset) * 17), _val)
/*              L2 -> LL3, HL3 */
#define IC_L2V IC_L0V
#define OC_LL3V OC_LL1V
#define OC_HL3V OC_HL1V
/*              H2 -> LH3, HH3 */
#define IC_H2V IC_L0V
#define OC_LH3V OC_LL1V
#define OC_HH3V OC_HL1V

#define SETUPLOQV lo_halfv = _mm_set1_epi16(lo_half)
#define SETUPHIQV hi_halfv = _mm_set1_epi16(hi_half)
#define LOQV(_val) _mm_srai_epi16(_mm_add_epi16(_val, lo_halfv), lo_fact)
#define HIQV(_val) _mm_srai_epi16(_mm_add_epi16(_val, hi_halfv), hi_fact)

#define LO_MATHV(_hn, _hn1, _x2n) \
    _mm_add_epi16(_x2n, _mm_srai_epi16(_mm_add_epi16(_hn1, _hn), 1))
#define HI_MATHV(_x2n, _x2n1, _x2n2) \
    _mm_srai_epi16(_mm_sub_epi16(_x2n1, _mm_srai_epi16( \
        _mm_add_epi16(_x2n, _x2n2), 1)), 1)

static const __m128i g_vec_zerov = { 0, 0 };
static const __m128i g_vec_128v = { 0x0080008000800080, 0x0080008000800080 };
static const __m128i g_i16_0 = { 0xFFFF, 0x0 };

/******************************************************************************/
static void
rfx_encode_dwt_shift_rem_vert_lv1_u8(const uint8 *in_buffer,
                                     sint16 *out_buffer)
{
    const uint8 *ic; /* input coefficients */
    sint16 *lo;
    sint16 *hi;
    __m128i x2nv;   /* n[2n]     */
    __m128i x2n1v;  /* n[2n + 1] */
    __m128i x2n2v;  /* n[2n + 2] */
    __m128i hn1v;   /* H[n - 1]  */
    __m128i hnv;    /* H[n]      */
    __m128i ic62v;
    int n;
    int y;

    for (y = 0; y < 64; y += 8)
    {
        /* setup */
        ic = SETUP_IC_LL0(y);
        lo = SETUP_OC_L0(y);
        hi = SETUP_OC_H0(y);
        /* pre */
        IC_LL0_U8V(x2nv, 0);
        IC_LL0_U8V(x2n1v, 1);
        IC_LL0_U8V(x2n2v, 2);
        OC_H0V(0, NOQ(hnv = HI_MATHV(x2nv, x2n1v, x2n2v)));
        OC_L0V(0, NOQ(_mm_add_epi16(x2nv, hnv))); /* mirror */
        /* loop */
        for (n = 1; n < 31; n++)
        {
            hn1v = hnv;
            x2nv = x2n2v;
            IC_LL0_U8V(x2n1v, 2 * n + 1);
            IC_LL0_U8V(x2n2v, 2 * n + 2);
            OC_H0V(n, NOQ(hnv = HI_MATHV(x2nv, x2n1v, x2n2v)));
            OC_L0V(n, NOQ(LO_MATHV(hnv, hn1v, x2nv)));
        }
        /* post */
        hn1v = hnv;
        ic62v = x2nv = x2n2v;
        IC_LL0_U8V(x2n1v, 63);
        /* ic[64] = 2 * ic[63] - ic[62] */
        x2n2v = _mm_sub_epi16(_mm_slli_epi16(x2n1v, 1), x2nv);
        OC_L0V(31, NOQ(_mm_add_epi16(x2nv, _mm_srai_epi16(hn1v, 1))));
        /* post ex */
        x2nv = x2n2v;
        /* x2n1 already set, mirror 65 -> 63 */
        x2n2v = ic62v;    /* mirror 66 -> 62 */
        hnv = HI_MATHV(x2nv, x2n1v, x2n2v);
        OC_L0V(32, NOQ(_mm_add_epi16(x2nv, _mm_srai_epi16(hnv, 1))));
    }
}

/******************************************************************************/
static void
rfx_encode_dwt_shift_rem_horz_lv1(const sint16 *in_buffer, sint16 *out_buffer,
                                  const char *quants)
{
    const sint16 *ic; /* input coefficients */
    sint16 *lo;
    sint16 *hi;
    __m128i x2nv;   /* n[2n]     */
    __m128i x2n1v;  /* n[2n + 1] */
    __m128i x2n2v;  /* n[2n + 2] */
    __m128i hn1v;   /* H[n - 1]  */
    __m128i hnv;    /* H[n]      */
    __m128i lo_halfv;
    __m128i hi_halfv;
    __m128i v1;
    __m128i v2;
    __m128i hn_savev;
    sint16 x2n;     /* n[2n]     */
    sint16 hn;      /* H[n]      */
    sint16 hi31;
    sint16 ic64;
    int n;
    int y;
    int lo_fact;
    int hi_fact;
    int lo_half;
    int hi_half;

    /* LL1 no Q */
    SETUPHIQ(4, 0); /* HL1 */
    SETUPHIQV;
    for (y = 0; y < 33; y++) /* lo */
    {
        /* setup */
        ic = SETUP_IC_L0(y);
        lo = SETUP_OC_LL1(y);
        hi = SETUP_OC_HL1(y);
        /* pre */
        IC_L0V(x2nv, x2n1v, x2n2v, 0);
        OC_HL1V(0, HIQV(hnv = HI_MATHV(x2nv, x2n1v, x2n2v)));
        hn_savev = _mm_and_si128(hnv, g_i16_0); /* mirror */
        hn1v = _mm_or_si128(_mm_slli_si128(hnv, 2), hn_savev);
        hn_savev = _mm_srli_si128(hnv, 14);
        OC_LL1V(0, NOQ(LO_MATHV(hnv, hn1v, x2nv)));
        /* loop */
        for (n = 8; n < 24; n += 8)
        {
            IC_L0V(x2nv, x2n1v, x2n2v, n);
            OC_HL1V(n, HIQV(hnv = HI_MATHV(x2nv, x2n1v, x2n2v)));
            hn1v = _mm_or_si128(_mm_slli_si128(hnv, 2), hn_savev);
            hn_savev = _mm_srli_si128(hnv, 14);
            OC_LL1V(n, NOQ(LO_MATHV(hnv, hn1v, x2nv)));
        }
        /* post */
        IC_L0V_POST(x2nv, x2n1v, x2n2v, 24);
        /* this will write 2 bytes past HL1 buffer, but still in tile,
           next buffer is LH1(not done yet) so no save and restore */
        OC_HL1V(24, HIQV(hnv = HI_MATHV(x2nv, x2n1v, x2n2v)));
        hn1v = _mm_or_si128(_mm_slli_si128(hnv, 2), hn_savev);
        hn = _mm_extract_epi16(hnv, 7);
        OC_LL1V(n, NOQ(LO_MATHV(hnv, hn1v, x2nv)));
        /* hn already set */
        x2n = ic64;
        OC_LL1(32, NOQ(x2n + (hn >> 1)));
    }
    SETUPLOQ(3, 4); /* LH1 */
    SETUPHIQ(4, 4); /* HH1 */
    SETUPLOQV;
    SETUPHIQV;
    for (y = 0; y < 31; y++) /* hi */
    {
        /* setup */
        ic = SETUP_IC_H0(y);
        lo = SETUP_OC_LH1(y);
        hi = SETUP_OC_HH1(y);
        /* pre */
        IC_H0V(x2nv, x2n1v, x2n2v, 0);
        OC_HH1V(0, HIQV(hnv = HI_MATHV(x2nv, x2n1v, x2n2v)));
        hn_savev = _mm_and_si128(hnv, g_i16_0); /* mirror */
        hn1v = _mm_or_si128(_mm_slli_si128(hnv, 2), hn_savev);
        hn_savev = _mm_srli_si128(hnv, 14);
        OC_LH1V(0, LOQV(LO_MATHV(hnv, hn1v, x2nv)));
        /* loop */
        for (n = 8; n < 24; n += 8)
        {
            IC_H0V(x2nv, x2n1v, x2n2v, n);
            OC_HH1V(n, HIQV(hnv = HI_MATHV(x2nv, x2n1v, x2n2v)));
            hn1v = _mm_or_si128(_mm_slli_si128(hnv, 2), hn_savev);
            hn_savev = _mm_srli_si128(hnv, 14);
            OC_LH1V(n, LOQV(LO_MATHV(hnv, hn1v, x2nv)));
        }
        /* post */
        IC_H0V_POST(x2nv, x2n1v, x2n2v, 24);
        /* this will write 2 bytes past HH1 buffer, but still in tile,
           next buffer is LL1(done already) so save and restore */
        hi31 = hi[31];
        OC_HH1V(24, HIQV(hnv = HI_MATHV(x2nv, x2n1v, x2n2v)));
        hi[31] = hi31;
        hn1v = _mm_or_si128(_mm_slli_si128(hnv, 2), hn_savev);
        hn = _mm_extract_epi16(hnv, 7);
        OC_LH1V(24, LOQV(LO_MATHV(hnv, hn1v, x2nv)));
        /* hn already set */
        x2n = ic64;
        OC_LH1(32, LOQ(x2n + (hn >> 1)));
    }
}

/******************************************************************************/
static void
rfx_encode_dwt_shift_rem_vert_lv2(const sint16 *in_buffer, sint16 *out_buffer)
{
    const sint16 *ic; /* input coefficients */
    sint16 *lo;
    sint16 *hi;
    __m128i x2nv;   /* n[2n]     */
    __m128i x2n1v;  /* n[2n + 1] */
    __m128i x2n2v;  /* n[2n + 2] */
    __m128i hn1v;   /* H[n - 1]  */
    __m128i hnv;    /* H[n]      */
    __m128i ic30v;
    sint16 x2n;     /* n[2n]     */
    sint16 x2n1;    /* n[2n + 1] */
    sint16 x2n2;    /* n[2n + 2] */
    sint16 hn1;     /* H[n - 1]  */
    sint16 hn;      /* H[n]      */
    sint16 ic30;
    int n;
    int y;

    for (y = 0; y < 32; y += 8)
    {
        /* setup */
        ic = SETUP_IC_LL1(y);
        lo = SETUP_OC_L1(y);
        hi = SETUP_OC_H1(y);
        /* pre */
        IC_LL1V(x2nv, 0);
        IC_LL1V(x2n1v, 1);
        IC_LL1V(x2n2v, 2);
        OC_H1V(0, NOQ(hnv = HI_MATHV(x2nv, x2n1v, x2n2v)));
        OC_L1V(0, NOQ(_mm_add_epi16(x2nv, hnv))); /* mirror */
        /* loop */
        for (n = 1; n < 15; n++)
        {
            hn1v = hnv;
            x2nv = x2n2v;
            IC_LL1V(x2n1v, 2 * n + 1);
            IC_LL1V(x2n2v, 2 * n + 2);
            OC_H1V(n, NOQ(hnv = HI_MATHV(x2nv, x2n1v, x2n2v)));
            OC_L1V(n, NOQ(LO_MATHV(hnv, hn1v, x2nv)));
        }
        /* post */
        hn1v = hnv;
        ic30v = x2nv = x2n2v;
        IC_LL1V(x2n1v, 31);
        IC_LL1V(x2n2v, 32);
        OC_H1V(15, NOQ(hnv = HI_MATHV(x2nv, x2n1v, x2n2v)));
        OC_L1V(15, NOQ(LO_MATHV(hnv, hn1v, x2nv)));
        /* post ex */
        hn1v = hnv;
        x2nv = x2n2v;
        /* x2n1 already set, mirror 33 -> 31 */
        x2n2v = ic30v;    /* mirror 34 -> 30 */
        hnv = HI_MATHV(x2nv, x2n1v, x2n2v);
        OC_L1V(16, NOQ(LO_MATHV(hnv, hn1v, x2nv)));
    }
    /* 33rd column */
    /* setup */
    ic = SETUP_IC_LL1(32);
    lo = SETUP_OC_L1(32);
    hi = SETUP_OC_H1(32);
    /* pre */
    IC_LL1(x2n, 0);
    IC_LL1(x2n1, 1);
    IC_LL1(x2n2, 2);
    OC_H1(0, NOQ(hn = (x2n1 - ((x2n + x2n2) >> 1)) >> 1));
    OC_L1(0, NOQ(x2n + hn)); /* mirror */
    /* loop */
    for (n = 1; n < 15; n++)
    {
        hn1 = hn;
        x2n = x2n2;
        IC_LL1(x2n1, 2 * n + 1);
        IC_LL1(x2n2, 2 * n + 2);
        OC_H1(n, NOQ(hn = (x2n1 - ((x2n + x2n2) >> 1)) >> 1));
        OC_L1(n, NOQ(x2n + ((hn1 + hn) >> 1)));
    }
    /* post */
    hn1 = hn;
    ic30 = x2n = x2n2;
    IC_LL1(x2n1, 31);
    IC_LL1(x2n2, 32);
    OC_H1(15, NOQ(hn = (x2n1 - ((x2n + x2n2) >> 1)) >> 1));
    OC_L1(15, NOQ(x2n + ((hn1 + hn) >> 1)));
    /* post ex */
    hn1 = hn;
    x2n = x2n2;
    /* x2n1 already set, mirror 33 -> 31 */
    x2n2 = ic30;      /* mirror 34 -> 30 */
    hn = (x2n1 - ((x2n + x2n2) >> 1)) >> 1;
    OC_L1(16, NOQ(x2n + ((hn1 + hn) >> 1)));
}

/******************************************************************************/
static void
rfx_encode_dwt_shift_rem_horz_lv2(const sint16 *in_buffer, sint16 *out_buffer,
                                  const char *quants)
{
    const sint16 *ic; /* input coefficients */
    sint16 *lo;
    sint16 *hi;
    __m128i x2nv;   /* n[2n]     */
    __m128i x2n1v;  /* n[2n + 1] */
    __m128i x2n2v;  /* n[2n + 2] */
    __m128i hn1v;   /* H[n - 1]  */
    __m128i hnv;    /* H[n]      */
    __m128i lo_halfv;
    __m128i hi_halfv;
    __m128i v1;
    __m128i v2;
    __m128i hn_savev;
    sint16 x2n;     /* n[2n]     */
    sint16 x2n1;    /* n[2n + 1] */
    sint16 x2n2;    /* n[2n + 2] */
    sint16 hn1;     /* H[n - 1]  */
    sint16 hn;      /* H[n]      */
    sint16 ic30;
    int y;
    int lo_fact;
    int hi_fact;
    int lo_half;
    int hi_half;

    /* LL2 no Q */
    SETUPHIQ(2, 4); /* HL2 */
    SETUPHIQV;
    for (y = 0; y < 17; y++) /* lo */
    {
        /* setup */
        ic = SETUP_IC_L1(y);
        lo = SETUP_OC_LL2(y);
        hi = SETUP_OC_HL2(y);
        /* pre */
        IC_L1V(x2nv, x2n1v, x2n2v, 0);
        OC_HL2V(0, HIQV(hnv = HI_MATHV(x2nv, x2n1v, x2n2v)));
        hn_savev = _mm_and_si128(hnv, g_i16_0); /* mirror */
        hn1v = _mm_or_si128(_mm_slli_si128(hnv, 2), hn_savev);
        hn_savev = _mm_srli_si128(hnv, 14);
        OC_LL2V(0, NOQ(LO_MATHV(hnv, hn1v, x2nv)));
        /* loop */
        IC_L1V(x2nv, x2n1v, x2n2v, 8);
        OC_HL2V(8, HIQV(hnv = HI_MATHV(x2nv, x2n1v, x2n2v)));
        hn1v = _mm_or_si128(_mm_slli_si128(hnv, 2), hn_savev);
        hn = _mm_extract_epi16(hnv, 7);
        OC_LL2V(8, NOQ(LO_MATHV(hnv, hn1v, x2nv)));
        /* hn already set */
        IC_L1(ic30, 30);
        IC_L1(x2n1, 31);
        IC_L1(x2n2, 32);
        /* post ex */
        hn1 = hn;
        x2n = x2n2;
        /* x2n1 already set, mirror 33 -> 31 */
        x2n2 = ic30;      /* mirror 34 -> 30 */
        hn = (x2n1 - ((x2n + x2n2) >> 1)) >> 1;
        OC_LL2(16, NOQ(x2n + ((hn1 + hn) >> 1)));
    }
    SETUPLOQ(2, 0); /* LH2 */
    SETUPHIQ(3, 0); /* HH2 */
    SETUPLOQV;
    SETUPHIQV;
    for (y = 0; y < 16; y++) /* hi */
    {
        /* setup */
        ic = SETUP_IC_H1(y);
        lo = SETUP_OC_LH2(y);
        hi = SETUP_OC_HH2(y);
        /* pre */
        IC_H1V(x2nv, x2n1v, x2n2v, 0);
        OC_HH2V(0, HIQV(hnv = HI_MATHV(x2nv, x2n1v, x2n2v)));
        hn_savev = _mm_and_si128(hnv, g_i16_0); /* mirror */
        hn1v = _mm_or_si128(_mm_slli_si128(hnv, 2), hn_savev);
        hn_savev = _mm_srli_si128(hnv, 14);
        OC_LH2V(0, LOQV(LO_MATHV(hnv, hn1v, x2nv)));
        /* loop */
        IC_H1V(x2nv, x2n1v, x2n2v, 8);
        OC_HH2V(8, HIQV(hnv = HI_MATHV(x2nv, x2n1v, x2n2v)));
        hn1v = _mm_or_si128(_mm_slli_si128(hnv, 2), hn_savev);
        hn = _mm_extract_epi16(hnv, 7);
        OC_LH2V(8, LOQV(LO_MATHV(hnv, hn1v, x2nv)));
        /* hn already set */
        IC_H1(ic30, 30);
        IC_H1(x2n1, 31);
        IC_H1(x2n2, 32);
        /* post ex */
        hn1 = hn;
        x2n = x2n2;
        /* x2n1 already set, mirror 33 -> 31 */
        x2n2 = ic30;      /* mirror 34 -> 30 */
        hn = (x2n1 - ((x2n + x2n2) >> 1)) >> 1;
        OC_LH2(16, LOQ(x2n + ((hn1 + hn) >> 1)));
    }
}

/******************************************************************************/
static void
rfx_encode_dwt_shift_rem_vert_lv3(const sint16 *in_buffer, sint16 *out_buffer)
{
    const sint16 *ic; /* input coefficients */
    sint16 *lo;
    sint16 *hi;
    __m128i x2nv;   /* n[2n]     */
    __m128i x2n1v;  /* n[2n + 1] */
    __m128i x2n2v;  /* n[2n + 2] */
    __m128i hn1v;   /* H[n - 1]  */
    __m128i hnv;    /* H[n]      */
    __m128i ic14v;
    sint16 x2n;     /* n[2n]     */
    sint16 x2n1;    /* n[2n + 1] */
    sint16 x2n2;    /* n[2n + 2] */
    sint16 hn1;     /* H[n - 1]  */
    sint16 hn;      /* H[n]      */
    sint16 ic14;
    int n;
    int y;

    for (y = 0; y < 16; y += 8)
    {
        /* setup */
        ic = SETUP_IC_LL2(y);
        lo = SETUP_OC_L2(y);
        hi = SETUP_OC_H2(y);
        /* pre */
        IC_LL2V(x2nv, 0);
        IC_LL2V(x2n1v, 1);
        IC_LL2V(x2n2v, 2);
        OC_H2V(0, NOQ(hnv = HI_MATHV(x2nv, x2n1v, x2n2v)));
        OC_L2V(0, NOQ(_mm_add_epi16(x2nv, hnv))); /* mirror */
        /* loop */
        for (n = 1; n < 7; n++)
        {
            hn1v = hnv;
            x2nv = x2n2v;
            IC_LL2V(x2n1v, 2 * n + 1);
            IC_LL2V(x2n2v, 2 * n + 2);
            OC_H2V(n, NOQ(hnv = HI_MATHV(x2nv, x2n1v, x2n2v)));
            OC_L2V(n, NOQ(LO_MATHV(hnv, hn1v, x2nv)));
        }
        /* post */
        hn1v = hnv;
        ic14v = x2nv = x2n2v;
        IC_LL2V(x2n1v, 15);
        IC_LL2V(x2n2v, 16);
        OC_H2V(7, NOQ(hnv = HI_MATHV(x2nv, x2n1v, x2n2v)));
        OC_L2V(7, NOQ(LO_MATHV(hnv, hn1v, x2nv)));
        /* post ex */
        hn1v = hnv;
        x2nv = x2n2v;
        /* x2n1 already set, mirror 17 -> 15 */
        x2n2v = ic14v;      /* mirror 18 -> 14 */
        hnv = HI_MATHV(x2nv, x2n1v, x2n2v);
        OC_L2V(8, NOQ(LO_MATHV(hnv, hn1v, x2nv)));
    }
    /* 17th column */
    /* setup */
    ic = SETUP_IC_LL2(16);
    lo = SETUP_OC_L2(16);
    hi = SETUP_OC_H2(16);
    /* pre */
    IC_LL2(x2n, 0);
    IC_LL2(x2n1, 1);
    IC_LL2(x2n2, 2);
    OC_H2(0, NOQ(hn = (x2n1 - ((x2n + x2n2) >> 1)) >> 1));
    OC_L2(0, NOQ(x2n + hn)); /* mirror */
    /* loop */
    for (n = 1; n < 7; n++)
    {
        hn1 = hn;
        x2n = x2n2;
        IC_LL2(x2n1, 2 * n + 1);
        IC_LL2(x2n2, 2 * n + 2);
        OC_H2(n, NOQ(hn = (x2n1 - ((x2n + x2n2) >> 1)) >> 1));
        OC_L2(n, NOQ(x2n + ((hn1 + hn) >> 1)));
    }
    /* post */
    hn1 = hn;
    ic14 = x2n = x2n2;
    IC_LL2(x2n1, 15);
    IC_LL2(x2n2, 16);
    OC_H2(7, NOQ(hn = (x2n1 - ((x2n + x2n2) >> 1)) >> 1));
    OC_L2(7, NOQ(x2n + ((hn1 + hn) >> 1)));
    /* post ex */
    hn1 = hn;
    x2n = x2n2;
    /* x2n1 already set, mirror 17 -> 15 */
    x2n2 = ic14;      /* mirror 18 -> 14 */
    hn = (x2n1 - ((x2n + x2n2) >> 1)) >> 1;
    OC_L2(8, NOQ(x2n + ((hn1 + hn) >> 1)));
}

/******************************************************************************/
static void
rfx_encode_dwt_shift_rem_horz_lv3(const sint16 *in_buffer, sint16 *out_buffer,
                                  const char *quants)
{
    const sint16 *ic; /* input coefficients */
    sint16 *lo;
    sint16 *hi;
    __m128i x2nv;   /* n[2n]     */
    __m128i x2n1v;  /* n[2n + 1] */
    __m128i x2n2v;  /* n[2n + 2] */
    __m128i hn1v;   /* H[n - 1]  */
    __m128i hnv;    /* H[n]      */
    __m128i lo_halfv;
    __m128i hi_halfv;
    __m128i v1;
    __m128i v2;
    __m128i hn_savev;
    sint16 x2n;     /* n[2n]     */
    sint16 x2n1;    /* n[2n + 1] */
    sint16 x2n2;    /* n[2n + 2] */
    sint16 hn1;     /* H[n - 1]  */
    sint16 hn;      /* H[n]      */
    sint16 ic14;
    int y;
    int lo_fact;
    int hi_fact;
    int lo_half;
    int hi_half;

    SETUPLOQ(0, 0); /* LL3 */
    SETUPHIQ(1, 0); /* HL3 */
    SETUPLOQV;
    SETUPHIQV;
    for (y = 0; y < 9; y++) /* lo */
    {
        /* setup */
        ic = SETUP_IC_L2(y);
        lo = SETUP_OC_LL3(y);
        hi = SETUP_OC_HL3(y);
        /* pre */
        IC_L2V(x2nv, x2n1v, x2n2v, 0);
        OC_HL3V(0, HIQV(hnv = HI_MATHV(x2nv, x2n1v, x2n2v)));
        hn_savev = _mm_and_si128(hnv, g_i16_0); /* mirror */
        hn1v = _mm_or_si128(_mm_slli_si128(hnv, 2), hn_savev);
        hn = _mm_extract_epi16(hnv, 7);
        OC_LL3V(0, LOQV(LO_MATHV(hnv, hn1v, x2nv)));
        /* no loop */
        /* hn already set */
        IC_L2(ic14, 14);
        IC_L2(x2n1, 15);
        IC_L2(x2n2, 16);
        /* post ex */
        hn1 = hn;
        x2n = x2n2;
        /* x2n1 already set, mirror 17 -> 15 */
        x2n2 = ic14;      /* mirror 18 -> 14 */
        hn = (x2n1 - ((x2n + x2n2) >> 1)) >> 1;
        OC_LL3(8, LOQ(x2n + ((hn1 + hn) >> 1)));
    }
    SETUPLOQ(0, 4); /* LH3 */
    SETUPHIQ(1, 4); /* HH3 */
    SETUPLOQV;
    SETUPHIQV;
    for (y = 0; y < 8; y++) /* hi */
    {
        /* setup */
        ic = SETUP_IC_H2(y);
        lo = SETUP_OC_LH3(y);
        hi = SETUP_OC_HH3(y);
        /* pre */
        IC_H2V(x2nv, x2n1v, x2n2v, 0);
        OC_HH3V(0, HIQV(hnv = HI_MATHV(x2nv, x2n1v, x2n2v)));
        hn_savev = _mm_and_si128(hnv, g_i16_0); /* mirror */
        hn1v = _mm_or_si128(_mm_slli_si128(hnv, 2), hn_savev);
        hn = _mm_extract_epi16(hnv, 7);
        OC_LH3V(0, LOQV(LO_MATHV(hnv, hn1v, x2nv)));
        /* no loop */
        /* hn already set */
        IC_H2(ic14, 14);
        IC_H2(x2n1, 15);
        IC_H2(x2n2, 16);
        /* post ex */
        hn1 = hn;
        x2n = x2n2;
        /* x2n1 already set, mirror 17 -> 15 */
        x2n2 = ic14;      /* mirror 18 -> 14 */
        hn = (x2n1 - ((x2n + x2n2) >> 1)) >> 1;
        OC_LH3(8, LOQ(x2n + ((hn1 + hn) >> 1)));
    }
}

/******************************************************************************/
int
rfx_encode_dwt_shift_rem_sse2(const uint8 *in_buffer,
                              sint16 *out_buffer, sint16 *tmp_buffer,
                              const char *quants)
{
    rfx_encode_dwt_shift_rem_vert_lv1_u8(in_buffer, tmp_buffer);
    rfx_encode_dwt_shift_rem_horz_lv1(tmp_buffer, out_buffer, quants);
    rfx_encode_dwt_shift_rem_vert_lv2(out_buffer + 3007, tmp_buffer);
    rfx_encode_dwt_shift_rem_horz_lv2(tmp_buffer, out_buffer + 3007, quants);
    rfx_encode_dwt_shift_rem_vert_lv3(out_buffer + 3807, tmp_buffer);
    rfx_encode_dwt_shift_rem_horz_lv3(tmp_buffer, out_buffer + 3807, quants);
    return 0;
}
