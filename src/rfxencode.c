/**
 * RFX codec encoder
 *
 * Copyright 2014-2017 Jay Sorg <jay.sorg@gmail.com>
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
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rfxcodec_encode.h>

#include "rfxcommon.h"
#include "rfxencode.h"
#include "rfxencode_compose.h"
#include "rfxconstants.h"
#include "rfxencode_tile.h"
#include "rfxencode_rlgr1.h"
#include "rfxencode_rlgr3.h"
#include "rfxencode_differential.h"
#include "rfxencode_quantization.h"
#include "rfxencode_dwt.h"
#include "rfxencode_diff_rlgr1.h"
#include "rfxencode_diff_rlgr3.h"
#include "rfxencode_rgb_to_yuv.h"

#ifdef RFX_USE_ACCEL_X86
#include "x86/funcs_x86.h"
#endif

#ifdef RFX_USE_ACCEL_AMD64
#include "amd64/funcs_amd64.h"
#endif

static void
clear_encoder_rbs(struct rfxencode *enc)
{
    int index;
    int jndex;
    for (index = 0; index < RFX_MAX_RB_Y; ++index)
    {
        for (jndex = 0; jndex < RFX_MAX_RB_X; ++jndex)
        {
            free(enc->rbs[index][jndex]);
            enc->rbs[index][jndex] = NULL;
        }
    }
}

static int
rfxencode_reset_encoder(void *handle)
{
    struct rfxencode *enc;
    enc = (struct rfxencode *) handle;

    enc->frame_idx = 0;
    enc->header_processed = 0;

    memset(enc->a_buffer, 0, sizeof(enc->a_buffer));
    memset(enc->y_r_buffer, 0, sizeof(enc->y_r_buffer));
    memset(enc->u_g_buffer, 0, sizeof(enc->u_g_buffer));
    memset(enc->v_b_buffer, 0, sizeof(enc->v_b_buffer));

    memset(enc->dwt_buffer_a, 0, sizeof(enc->dwt_buffer_a));
    enc->dwt_buffer = (sint16 *) (((size_t) (enc->dwt_buffer_a)) & ~15);

    memset(enc->dwt_buffer1_a, 0, sizeof(enc->dwt_buffer1_a));
    enc->dwt_buffer1 = (sint16 *) (((size_t) (enc->dwt_buffer1_a)) & ~15);

    memset(enc->dwt_buffer2_a, 0, sizeof(enc->dwt_buffer2_a));
    enc->dwt_buffer2 = (sint16 *) (((size_t) (enc->dwt_buffer2_a)) & ~15);

    memset(enc->dwt_buffer3_a, 0, sizeof(enc->dwt_buffer3_a));
    enc->dwt_buffer3 = (sint16 *) (((size_t) (enc->dwt_buffer3_a)) & ~15);

    memset(enc->dwt_buffer4_a, 0, sizeof(enc->dwt_buffer4_a));
    enc->dwt_buffer4 = (sint16 *) (((size_t) (enc->dwt_buffer4_a)) & ~15);

    memset(enc->dwt_buffer5_a, 0, sizeof(enc->dwt_buffer5_a));
    enc->dwt_buffer5 = (sint16 *) (((size_t) (enc->dwt_buffer5_a)) & ~15);

    memset(enc->dwt_buffer6_a, 0, sizeof(enc->dwt_buffer6_a));
    enc->dwt_buffer6 = (sint16 *) (((size_t) (enc->dwt_buffer6_a)) & ~15);

    clear_encoder_rbs(enc);
    return 0;
}

/******************************************************************************/
int
rfxcodec_encode_create_ex(int width, int height, int format, int flags,
                          void **handle)
{
    struct rfxencode *enc;
    int ax;
    int bx;
    int cx;
    int dx;

    enc = (struct rfxencode *) calloc(1, sizeof(struct rfxencode));
    if (enc == NULL)
    {
        return 1;
    }

    enc->dwt_buffer = (sint16 *) (((size_t) (enc->dwt_buffer_a)) & ~15);
    enc->dwt_buffer1 = (sint16 *) (((size_t) (enc->dwt_buffer1_a)) & ~15);
    enc->dwt_buffer2 = (sint16 *) (((size_t) (enc->dwt_buffer2_a)) & ~15);
    enc->dwt_buffer3 = (sint16 *) (((size_t) (enc->dwt_buffer3_a)) & ~15);
    enc->dwt_buffer4 = (sint16 *) (((size_t) (enc->dwt_buffer4_a)) & ~15);
    enc->dwt_buffer5 = (sint16 *) (((size_t) (enc->dwt_buffer5_a)) & ~15);
    enc->dwt_buffer6 = (sint16 *) (((size_t) (enc->dwt_buffer6_a)) & ~15);

#if defined(RFX_USE_ACCEL_X86)
    cpuid_x86(1, 0, &ax, &bx, &cx, &dx);
#elif defined(RFX_USE_ACCEL_AMD64)
    cpuid_amd64(1, 0, &ax, &bx, &cx, &dx);
#else
    ax = 0;
    bx = 0;
    cx = 0;
    dx = 0;
#endif
    if (dx & (1 << 26)) /* SSE 2 */
    {
        printf("rfxcodec_encode_create: got sse2\n");
        enc->got_sse2 = 1;
    }
    if (cx & (1 << 0)) /* SSE 3 */
    {
        printf("rfxcodec_encode_create: got sse3\n");
        enc->got_sse3 = 1;
    }
    if (cx & (1 << 19)) /* SSE 4.1 */
    {
        printf("rfxcodec_encode_create: got sse4.1\n");
        enc->got_sse41 = 1;
    }
    if (cx & (1 << 20)) /* SSE 4.2 */
    {
        printf("rfxcodec_encode_create: got sse4.2\n");
        enc->got_sse42 = 1;
    }
    if (cx & (1 << 23)) /* popcnt */
    {
        printf("rfxcodec_encode_create: got popcnt\n");
        enc->got_popcnt = 1;
    }
#if defined(RFX_USE_ACCEL_X86)
    cpuid_x86(0x80000001, 0, &ax, &bx, &cx, &dx);
#elif defined(RFX_USE_ACCEL_AMD64)
    cpuid_amd64(0x80000001, 0, &ax, &bx, &cx, &dx);
#else
    ax = 0;
    bx = 0;
    cx = 0;
    dx = 0;
#endif
    if (cx & (1 << 5)) /* lzcnt */
    {
        printf("rfxcodec_encode_create: got lzcnt\n");
        enc->got_lzcnt = 1;
    }
    if (cx & (1 << 6)) /* SSE 4.a */
    {
        printf("rfxcodec_encode_create: got sse4.a\n");
        enc->got_sse4a = 1;
    }

    enc->width = width;
    enc->height = height;
    enc->mode = RLGR3;
    if (flags & RFX_FLAGS_RLGR1)
    {
        enc->mode = RLGR1;
    }
    switch (format)
    {
        case RFX_FORMAT_BGRA:
            enc->bits_per_pixel = 32;
            break;
        case RFX_FORMAT_RGBA:
            enc->bits_per_pixel = 32;
            break;
        case RFX_FORMAT_BGR:
            enc->bits_per_pixel = 24;
            break;
        case RFX_FORMAT_RGB:
            enc->bits_per_pixel = 24;
            break;
        case RFX_FORMAT_YUV:
            enc->bits_per_pixel = 32;
            break;
        default:
            free(enc);
            return 2;
    }
    enc->format = format;
    enc->rfx_encode_rgb_to_yuv = rfx_encode_rgb_to_yuv;
    enc->rfx_encode_argb_to_yuva = rfx_encode_argb_to_yuva;
    /* assign encoding functions */
    if (flags & RFX_FLAGS_PRO1)
    {
        enc->pro_ver = 1;
    }
    else if (flags & RFX_FLAGS_NOACCEL)
    {
        if (enc->mode == RLGR3)
        {
            printf("rfxcodec_encode_create: rfx_encode set to rfx_encode_component_rlgr3\n");
            enc->rfx_encode = rfx_encode_component_rlgr3; /* rfxencode_tile.c */
        }
        else
        {
            printf("rfxcodec_encode_create: rfx_encode set to rfx_encode_component_rlgr1\n");
            enc->rfx_encode = rfx_encode_component_rlgr1; /* rfxencode_tile.c */
        }
    }
    else
    {
#if defined(RFX_USE_ACCEL_X86)
        if (enc->got_sse41)
        {
            if (enc->mode == RLGR3)
            {
                printf("rfxcodec_encode_create: rfx_encode set to rfx_encode_component_rlgr3_x86_sse41\n");
                enc->rfx_encode = rfx_encode_component_rlgr3_x86_sse41; /* rfxencode_tile.c */
            }
            else
            {
                printf("rfxcodec_encode_create: rfx_encode set to rfx_encode_component_rlgr1_x86_sse41\n");
                enc->rfx_encode = rfx_encode_component_rlgr1_x86_sse41; /* rfxencode_tile.c */
            }
        }
        else if (enc->got_sse2)
        {
            if (enc->mode == RLGR3)
            {
                printf("rfxcodec_encode_create: rfx_encode set to rfx_encode_component_rlgr3_x86_sse2\n");
                enc->rfx_encode = rfx_encode_component_rlgr3_x86_sse2; /* rfxencode_tile.c */
            }
            else
            {
                printf("rfxcodec_encode_create: rfx_encode set to rfx_encode_component_rlgr1_x86_sse2\n");
                enc->rfx_encode = rfx_encode_component_rlgr1_x86_sse2; /* rfxencode_tile.c */
            }
        }
        else
        {
            if (enc->mode == RLGR3)
            {
                printf("rfxcodec_encode_create: rfx_encode set to rfx_encode_component_rlgr3\n");
                enc->rfx_encode = rfx_encode_component_rlgr3; /* rfxencode_tile.c */
            }
            else
            {
                printf("rfxcodec_encode_create: rfx_encode set to rfx_encode_component_rlgr1\n");
                enc->rfx_encode = rfx_encode_component_rlgr1; /* rfxencode_tile.c */
            }
        }
#elif defined(RFX_USE_ACCEL_AMD64)
        if (enc->got_sse41)
        {
            if (enc->mode == RLGR3)
            {
                printf("rfxcodec_encode_create: rfx_encode set to rfx_encode_component_rlgr3_amd64_sse41\n");
                enc->rfx_encode = rfx_encode_component_rlgr3_amd64_sse41; /* rfxencode_tile.c */
            }
            else
            {
                printf("rfxcodec_encode_create: rfx_encode set to rfx_encode_component_rlgr1_amd64_sse41\n");
                enc->rfx_encode = rfx_encode_component_rlgr1_amd64_sse41; /* rfxencode_tile.c */
            }
        }
        else if (enc->got_sse2)
        {
            if (enc->mode == RLGR3)
            {
                printf("rfxcodec_encode_create: rfx_encode set to rfx_encode_component_rlgr3_amd64_sse2\n");
                enc->rfx_encode = rfx_encode_component_rlgr3_amd64_sse2; /* rfxencode_tile.c */
            }
            else
            {
                printf("rfxcodec_encode_create: rfx_encode set to rfx_encode_component_rlgr1_amd64_sse2\n");
                enc->rfx_encode = rfx_encode_component_rlgr1_amd64_sse2; /* rfxencode_tile.c */
            }
        }
        else
        {
            if (enc->mode == RLGR3)
            {
                printf("rfxcodec_encode_create: rfx_encode set to rfx_encode_component_rlgr3\n");
                enc->rfx_encode = rfx_encode_component_rlgr3; /* rfxencode_tile.c */
            }
            else
            {
                printf("rfxcodec_encode_create: rfx_encode set to rfx_encode_component_rlgr1\n");
                enc->rfx_encode = rfx_encode_component_rlgr1; /* rfxencode_tile.c */
            }
        }
#else
        if (enc->mode == RLGR3)
        {
            printf("rfxcodec_encode_create: rfx_encode set to rfx_encode_component_rlgr3\n");
            enc->rfx_encode = rfx_encode_component_rlgr3; /* rfxencode_tile.c */
        }
        else
        {
            printf("rfxcodec_encode_create: rfx_encode set to rfx_encode_component_rlgr1\n");
            enc->rfx_encode = rfx_encode_component_rlgr1; /* rfxencode_tile.c */
        }
#endif
    }
    if (ax == 0)
    {
    }
    if (bx == 0)
    {
    }
    *handle = enc;
    return 0;
}

/******************************************************************************/
void *
rfxcodec_encode_create(int width, int height, int format, int flags)
{
    int error;
    void *handle;

    error = rfxcodec_encode_create_ex(width, height, format, flags, &handle);
    if (error == 0)
    {
        return handle;
    }
    return 0;
}

/******************************************************************************/
int
rfxcodec_encode_destroy(void *handle)
{
    struct rfxencode *enc;

    enc = (struct rfxencode *) handle;
    if (enc == NULL)
    {
        return 0;
    }
    clear_encoder_rbs(enc);
    free(enc);
    return 0;
}

/******************************************************************************/
int
rfxcodec_encode_ex(void *handle, char *cdata, int *cdata_bytes,
                   const char *buf, int width, int height, int stride_bytes,
                   const struct rfx_rect *regions, int num_regions,
                   const struct rfx_tile *tiles, int num_tiles,
                   const char *quants, int num_quants, int flags)
{
    struct rfxencode *enc;
    int tiles_written;
    STREAM s;

    enc = (struct rfxencode *) handle;

    s.data = (uint8 *) cdata;
    s.p = s.data;
    s.size = *cdata_bytes;

    if (enc->pro_ver > 0)
    {
        if (flags & RFX_FLAGS_PRO_KEY)
        {
            rfxencode_reset_encoder(handle);
        }
        /* Only the first frame should send the RemoteFX header */
        if ((enc->frame_idx == 0) && (enc->header_processed == 0))
        {
            if (rfx_pro_compose_message_header(enc, &s) != 0)
            {
                return -1;
            }
        }
        tiles_written = rfx_pro_compose_message_data(enc, &s, regions, num_regions,
                                         buf, width, height, stride_bytes,
                                         tiles, num_tiles, quants, num_quants,
                                         flags);
        if (tiles_written <= 0)
        {
            return -1;
        }
        *cdata_bytes = (int) (s.p - s.data);
        return tiles_written;
    }

    /* Only the first frame should send the RemoteFX header */
    if ((enc->frame_idx == 0) && (enc->header_processed == 0))
    {
        if (rfx_compose_message_header(enc, &s) != 0)
        {
            return -1;
        }
    }
    tiles_written = rfx_compose_message_data(enc, &s, regions, num_regions,
                                            buf, width, height, stride_bytes,
                                            tiles, num_tiles,
                                            quants, num_quants, flags);
    *cdata_bytes = (int) (s.p - s.data);
    return tiles_written;
}

/******************************************************************************/
int
rfxcodec_encode(void *handle, char *cdata, int *cdata_bytes,
                const char *buf, int width, int height, int stride_bytes,
                const struct rfx_rect *regions, int num_regions,
                const struct rfx_tile *tiles, int num_tiles,
                const char *quants, int num_quants)
{
    return rfxcodec_encode_ex(handle, cdata, cdata_bytes, buf, width, height,
                              stride_bytes, regions, num_regions, tiles,
                              num_tiles, quants, num_quants, 0);
}

/******************************************************************************/
int
rfxcodec_encode_get_internals(struct rfxcodec_encode_internals *internals)
{
    memset(internals, 0, sizeof(struct rfxcodec_encode_internals));
    internals->rfxencode_rlgr1 = rfx_rlgr1_encode;
    internals->rfxencode_rlgr3 = rfx_rlgr3_encode;
    internals->rfxencode_differential = rfx_differential_encode;
    internals->rfxencode_quantization = rfx_quantization_encode;
    internals->rfxencode_dwt_2d = rfx_dwt_2d_encode;
    internals->rfxencode_diff_rlgr1 = rfx_encode_diff_rlgr1;
    internals->rfxencode_diff_rlgr3 = rfx_encode_diff_rlgr3;
#if defined(RFX_USE_ACCEL_X86)
    internals->rfxencode_dwt_shift_x86_sse2 = rfxcodec_encode_dwt_shift_x86_sse2;
    internals->rfxencode_dwt_shift_x86_sse41 = rfxcodec_encode_dwt_shift_x86_sse41;
#endif
#if defined(RFX_USE_ACCEL_AMD64)
    internals->rfxencode_dwt_shift_amd64_sse2 = rfxcodec_encode_dwt_shift_amd64_sse2;
    internals->rfxencode_dwt_shift_amd64_sse41 = rfxcodec_encode_dwt_shift_amd64_sse41;
#endif
    return 0;
}

/*****************************************************************************/
/* produce a hex dump */
void
rfxcodec_hexdump(const void *p, int len)
{
    unsigned char *line;
    int i;
    int thisline;
    int offset;

    line = (unsigned char *)p;
    offset = 0;

    while (offset < len)
    {
        printf("%04x ", offset);
        thisline = len - offset;

        if (thisline > 16)
        {
            thisline = 16;
        }

        for (i = 0; i < thisline; i++)
        {
            printf("%02x ", line[i]);
        }

        for (; i < 16; i++)
        {
            printf("   ");
        }

        for (i = 0; i < thisline; i++)
        {
            printf("%c", (line[i] >= 0x20 && line[i] < 0x7f) ? line[i] : '.');
        }

        printf("%s", "\n");
        offset += thisline;
        line += thisline;
    }
}

