/**
 * RFX codec encoder
 *
 * Copyright 2014-2015 Jay Sorg <jay.sorg@gmail.com>
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

#ifndef __RFXENCODE_H
#define __RFXENCODE_H

struct rfxencode;

typedef int (*rfx_encode_rgb_to_yuv_proc)(struct rfxencode *enc,
                                          const char *rgb_data,
                                          int width, int height,
                                          int stride_bytes);
typedef int (*rfx_encode_argb_to_yuva_proc)(struct rfxencode *enc,
                                            const char *argb_data,
                                            int width, int height,
                                       int stride_bytes);
typedef int (*rfx_encode_proc)(struct rfxencode *enc, const char *qtable,
                               const uint8 *data,
                               uint8 *buffer, int buffer_size, int *size);

struct rfx_rb
{
    sint16 y[4096];
    sint16 u[4096];
    sint16 v[4096];
};


#define RFX_MAX_RB_X 64
#define RFX_MAX_RB_Y 64

struct rfxencode
{
    int width;
    int height;
    int frame_idx;
    int header_processed;
    int mode;
    int properties;
    int flags;
    int bits_per_pixel;
    int format;
    int pro_ver;
    int pad0[6];

    uint8 a_buffer[4096];
    uint8 y_r_buffer[4096];
    uint8 u_g_buffer[4096];
    uint8 v_b_buffer[4096];
    uint8 pad1[16];
    sint16 dwt_buffer_a[4096];
    sint16 dwt_buffer1_a[4096];
    sint16 dwt_buffer2_a[4096];
    sint16 dwt_buffer3_a[4096];
    sint16 dwt_buffer4_a[4096];
    sint16 dwt_buffer5_a[4096];
    sint16 dwt_buffer6_a[4096];
    uint8 pad2[16];
    sint16 *dwt_buffer;
    sint16 *dwt_buffer1;
    sint16 *dwt_buffer2;
    sint16 *dwt_buffer3;
    sint16 *dwt_buffer4;
    sint16 *dwt_buffer5;
    sint16 *dwt_buffer6;
    rfx_encode_proc rfx_encode;
    rfx_encode_rgb_to_yuv_proc rfx_encode_rgb_to_yuv;
    rfx_encode_argb_to_yuva_proc rfx_encode_argb_to_yuva;
    rfx_encode_proc rfx_rem_encode;

    struct rfx_rb * rbs[RFX_MAX_RB_X][RFX_MAX_RB_Y];

    int got_sse2;
    int got_sse3;
    int got_sse41;
    int got_sse42;
    int got_sse4a;
    int got_popcnt;
    int got_lzcnt;
    int got_neon;
};

void
rfxcodec_hexdump(const void *p, int len);

#endif
