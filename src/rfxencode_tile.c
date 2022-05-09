/**
 * FreeRDP: A Remote Desktop Protocol client.
 * RemoteFX Codec Library - Encode
 *
 * Copyright 2011 Vic Lee
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
#include "rfxconstants.h"
#include "rfxencode_tile.h"
#include "rfxencode_dwt.h"
#include "rfxencode_dwt_rem.h"
#include "rfxencode_quantization.h"
#include "rfxencode_differential.h"
#include "rfxencode_rlgr1.h"
#include "rfxencode_rlgr3.h"
#include "rfxencode_diff_rlgr1.h"
#include "rfxencode_diff_rlgr3.h"
#include "rfxencode_alpha.h"

#ifdef RFX_USE_ACCEL_X86
#include "x86/funcs_x86.h"
#endif

#ifdef RFX_USE_ACCEL_AMD64
#include "amd64/funcs_amd64.h"
#endif

#define LLOG_LEVEL 1
#define LLOGLN(_level, _args) \
    do { if (_level < LLOG_LEVEL) { printf _args ; printf("\n"); } } while (0)

/**
 * In order not to overflow the output buffer, we need to have an
 * upper limit on the size of a tile which could possibly be written to
 * the buffer.
 *
 * The tile data structure (TS_RFX_TILE) is defined in [MS-RDPRFX]
 * 2.2.2.3.4.1
 *
 * We make the conservative assumption that the RLGR1/RLGL3 algorithm
 * worst case results in a doubling of the YCbCr data for each pixel.
 * This is likely to be far higher than necessary.
 */
#define RLGR_WORST_CASE_SIZE_FACTOR 2
#define TILE_SIZE_UPPER_LIMIT (6 + 1 + 1 + 1 + 2 + 2 + 2 + 2 + 2 + \
                                (64 * 64) * 3 * RLGR_WORST_CASE_SIZE_FACTOR)


/******************************************************************************/
int
rfx_encode_component_rlgr1(struct rfxencode *enc, const char *qtable,
                           const uint8 *data,
                           uint8 *buffer, int buffer_size, int *size)
{
    LLOGLN(10, ("rfx_encode_component_rlgr1:"));
    if (rfx_dwt_2d_encode(data, enc->dwt_buffer1, enc->dwt_buffer) != 0)
    {
        return 1;
    }
    if (rfx_quantization_encode(enc->dwt_buffer1, qtable) != 0)
    {
        return 1;
    }
    if (rfx_differential_encode(enc->dwt_buffer1 + 4032, 64) != 0)
    {
        return 1;
    }
    *size = rfx_rlgr1_encode(enc->dwt_buffer1, buffer, buffer_size);
    return 0;
}

/******************************************************************************/
int
rfx_encode_component_rlgr3(struct rfxencode *enc, const char *qtable,
                           const uint8 *data,
                           uint8 *buffer, int buffer_size, int *size)
{
    LLOGLN(10, ("rfx_encode_component_rlgr3:"));
    if (rfx_dwt_2d_encode(data, enc->dwt_buffer1, enc->dwt_buffer) != 0)
    {
        return 1;
    }
    if (rfx_quantization_encode(enc->dwt_buffer1, qtable) != 0)
    {
        return 1;
    }
    if (rfx_differential_encode(enc->dwt_buffer1 + 4032, 64) != 0)
    {
        return 1;
    }
    *size = rfx_rlgr3_encode(enc->dwt_buffer1, buffer, buffer_size);
    return 0;
}

/******************************************************************************/
static int
check_and_rfx_encode(struct rfxencode *enc, const char *qtable,
                     const uint8 *data,
                     uint8 *buffer, int buffer_size, int *size)
{
    if (buffer_size < TILE_SIZE_UPPER_LIMIT)
    {
        return 1;
    }
    return enc->rfx_encode(enc, qtable, data, buffer, buffer_size, size);
}

/******************************************************************************/
int
rfx_encode_rgb(struct rfxencode *enc, const char *rgb_data,
               int width, int height, int stride_bytes,
               const char *y_quants, const char *u_quants,
               const char *v_quants,
               STREAM *data_out, int *y_size, int *u_size, int *v_size)
{
    uint8 *y_r_buffer;
    uint8 *u_g_buffer;
    uint8 *v_b_buffer;

    LLOGLN(10, ("rfx_encode_rgb:"));
    if (enc->rfx_encode_rgb_to_yuv(enc, rgb_data, width, height,
                                   stride_bytes) != 0)
    {
        return 1;
    }
    y_r_buffer = enc->y_r_buffer;
    u_g_buffer = enc->u_g_buffer;
    v_b_buffer = enc->v_b_buffer;
    if (check_and_rfx_encode(enc, y_quants, y_r_buffer,
                             stream_get_tail(data_out),
                             stream_get_left(data_out),
                             y_size) != 0)
    {
        return 1;
    }
    LLOGLN(10, ("rfx_encode_rgb: y_size %d", *y_size));
    stream_seek(data_out, *y_size);
    if (check_and_rfx_encode(enc, u_quants, u_g_buffer,
                             stream_get_tail(data_out),
                             stream_get_left(data_out),
                             u_size) != 0)
    {
        return 1;
    }
    LLOGLN(10, ("rfx_encode_rgb: u_size %d", *u_size));
    stream_seek(data_out, *u_size);
    if (check_and_rfx_encode(enc, v_quants, v_b_buffer,
                             stream_get_tail(data_out),
                             stream_get_left(data_out),
                             v_size) != 0)
    {
        return 1;
    }
    LLOGLN(10, ("rfx_encode_rgb: v_size %d", *v_size));
    stream_seek(data_out, *v_size);
    return 0;
}

/******************************************************************************/
int
rfx_encode_argb(struct rfxencode *enc, const char *argb_data,
                int width, int height, int stride_bytes,
                const char *y_quants, const char *u_quants,
                const char *v_quants,
                STREAM *data_out, int *y_size, int *u_size,
                int *v_size, int *a_size)
{
    uint8 *a_buffer;
    uint8 *y_r_buffer;
    uint8 *u_g_buffer;
    uint8 *v_b_buffer;

    LLOGLN(10, ("rfx_encode_argb:"));
    if (enc->rfx_encode_argb_to_yuva(enc, argb_data, width, height,
                                     stride_bytes) != 0)
    {
        return 1;
    }
    a_buffer = enc->a_buffer;
    y_r_buffer = enc->y_r_buffer;
    u_g_buffer = enc->u_g_buffer;
    v_b_buffer = enc->v_b_buffer;
    if (check_and_rfx_encode(enc, y_quants, y_r_buffer,
                             stream_get_tail(data_out),
                             stream_get_left(data_out),
                             y_size) != 0)
    {
        return 1;
    }
    LLOGLN(10, ("rfx_encode_rgb: y_size %d", *y_size));
    stream_seek(data_out, *y_size);
    if (check_and_rfx_encode(enc, u_quants, u_g_buffer,
                             stream_get_tail(data_out),
                             stream_get_left(data_out),
                             u_size) != 0)
    {
        return 1;
    }
    LLOGLN(10, ("rfx_encode_rgb: u_size %d", *u_size));
    stream_seek(data_out, *u_size);
    if (check_and_rfx_encode(enc, v_quants, v_b_buffer,
                             stream_get_tail(data_out),
                             stream_get_left(data_out),
                             v_size) != 0)
    {
        return 1;
    }
    LLOGLN(10, ("rfx_encode_rgb: v_size %d", *v_size));
    stream_seek(data_out, *v_size);
    *a_size = rfx_encode_plane(enc, a_buffer, 64, 64, data_out);
    return 0;
}

/******************************************************************************/
int
rfx_encode_yuv(struct rfxencode *enc, const char *yuv_data,
               int width, int height, int stride_bytes,
               const char *y_quants, const char *u_quants,
               const char *v_quants,
               STREAM *data_out, int *y_size, int *u_size, int *v_size)
{
    const uint8 *y_buffer;
    const uint8 *u_buffer;
    const uint8 *v_buffer;

    y_buffer = (const uint8 *) yuv_data;
    u_buffer = (const uint8 *) (yuv_data + RFX_YUV_BTES);
    v_buffer = (const uint8 *) (yuv_data + RFX_YUV_BTES * 2);
    if (check_and_rfx_encode(enc, y_quants, y_buffer,
                             stream_get_tail(data_out),
                             stream_get_left(data_out),
                             y_size) != 0)
    {
        return 1;
    }
    stream_seek(data_out, *y_size);
    if (check_and_rfx_encode(enc, u_quants, u_buffer,
                             stream_get_tail(data_out),
                             stream_get_left(data_out),
                             u_size) != 0)
    {
        return 1;
    }
    stream_seek(data_out, *u_size);
    if (check_and_rfx_encode(enc, v_quants, v_buffer,
                             stream_get_tail(data_out),
                             stream_get_left(data_out),
                             v_size) != 0)
    {
        return 1;
    }
    stream_seek(data_out, *v_size);
    return 0;
}

/******************************************************************************/
int
rfx_encode_yuva(struct rfxencode *enc, const char *yuva_data,
                int width, int height, int stride_bytes,
                const char *y_quants, const char *u_quants,
                const char *v_quants,
                STREAM *data_out, int *y_size, int *u_size,
                int *v_size, int *a_size)
{
    const uint8 *y_buffer;
    const uint8 *u_buffer;
    const uint8 *v_buffer;
    const uint8 *a_buffer;

    y_buffer = (const uint8 *) yuva_data;
    u_buffer = (const uint8 *) (yuva_data + RFX_YUV_BTES);
    v_buffer = (const uint8 *) (yuva_data + RFX_YUV_BTES * 2);
    a_buffer = (const uint8 *) (yuva_data + RFX_YUV_BTES * 3);
    if (check_and_rfx_encode(enc, y_quants, y_buffer,
                             stream_get_tail(data_out),
                             stream_get_left(data_out),
                             y_size) != 0)
    {
        return 1;
    }
    stream_seek(data_out, *y_size);
    if (check_and_rfx_encode(enc, u_quants, u_buffer,
                             stream_get_tail(data_out),
                             stream_get_left(data_out),
                             u_size) != 0)
    {
        return 1;
    }
    stream_seek(data_out, *u_size);
    if (check_and_rfx_encode(enc, v_quants, v_buffer,
                             stream_get_tail(data_out),
                             stream_get_left(data_out),
                             v_size) != 0)
    {
        return 1;
    }
    stream_seek(data_out, *v_size);
    *a_size = rfx_encode_plane(enc, a_buffer, 64, 64, data_out);
    return 0;
}

