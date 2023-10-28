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
#include "rfxencode_quantization.h"
#include "rfxencode_differential.h"
#include "rfxencode_rlgr1.h"
#include "rfxencode_rlgr3.h"
#include "rfxencode_alpha.h"
#include "rfxencode_rgb_to_yuv.h"

#define LLOG_LEVEL 1
#define LLOGLN(_level, _args) \
    do { if (_level < LLOG_LEVEL) { printf _args ; printf("\n"); } } while (0)

/******************************************************************************/
static int
rfx_encode_format_rgb(const char *rgb_data, int width, int height,
                      int stride_bytes, int pixel_format,
                      uint8 *r_buf, uint8 *g_buf, uint8 *b_buf)
{
    int x;
    int y;
    const uint8 *src;
    uint8 r;
    uint8 g;
    uint8 b;
    uint8 *lr_buf;
    uint8 *lg_buf;
    uint8 *lb_buf;

    LLOGLN(10, ("rfx_encode_format_rgb: pixel_format %d", pixel_format));
    b = 0;
    g = 0;
    r = 0;
    switch (pixel_format)
    {
        case RFX_FORMAT_BGRA:
            for (y = 0; y < height; y++)
            {
                src = (uint8 *) (rgb_data + y * stride_bytes);
                lr_buf = r_buf + y * 64;
                lg_buf = g_buf + y * 64;
                lb_buf = b_buf + y * 64;
                for (x = 0; x < width; x++)
                {
                    b = *src++;
                    *lb_buf++ = b;
                    g = *src++;
                    *lg_buf++ = g;
                    r = *src++;
                    *lr_buf++ = r;
                    src++;
                }
                while (x < 64)
                {
                    *lb_buf++ = b;
                    *lg_buf++ = g;
                    *lr_buf++ = r;
                    x++;
                }
            }
            while (y < 64)
            {
                lr_buf = r_buf + y * 64;
                lg_buf = g_buf + y * 64;
                lb_buf = b_buf + y * 64;
                memcpy(lr_buf, lr_buf - 64, 64);
                memcpy(lg_buf, lg_buf - 64, 64);
                memcpy(lb_buf, lb_buf - 64, 64);
                y++;
            }
            break;
        case RFX_FORMAT_RGBA:
            for (y = 0; y < height; y++)
            {
                src = (uint8 *) (rgb_data + y * stride_bytes);
                lr_buf = r_buf + y * 64;
                lg_buf = g_buf + y * 64;
                lb_buf = b_buf + y * 64;
                for (x = 0; x < width; x++)
                {
                    r = *src++;
                    *lr_buf++ = r;
                    g = *src++;
                    *lg_buf++ = g;
                    b = *src++;
                    *lb_buf++ = b;
                    src++;
                }
                while (x < 64)
                {
                    *lr_buf++ = r;
                    *lg_buf++ = g;
                    *lb_buf++ = b;
                    x++;
                }
            }
            while (y < 64)
            {
                lr_buf = r_buf + y * 64;
                lg_buf = g_buf + y * 64;
                lb_buf = b_buf + y * 64;
                memcpy(lr_buf, lr_buf - 64, 64);
                memcpy(lg_buf, lg_buf - 64, 64);
                memcpy(lb_buf, lb_buf - 64, 64);
                y++;
            }
            break;
        case RFX_FORMAT_BGR:
            for (y = 0; y < height; y++)
            {
                src = (uint8 *) (rgb_data + y * stride_bytes);
                lr_buf = r_buf + y * 64;
                lg_buf = g_buf + y * 64;
                lb_buf = b_buf + y * 64;
                for (x = 0; x < width; x++)
                {
                    b = *src++;
                    *lb_buf++ = b;
                    g = *src++;
                    *lg_buf++ = g;
                    r = *src++;
                    *lr_buf++ = r;
                }
                while (x < 64)
                {
                    *lb_buf++ = b;
                    *lg_buf++ = g;
                    *lr_buf++ = r;
                    x++;
                }
            }
            while (y < 64)
            {
                lr_buf = r_buf + y * 64;
                lg_buf = g_buf + y * 64;
                lb_buf = b_buf + y * 64;
                memcpy(lr_buf, lr_buf - 64, 64);
                memcpy(lg_buf, lg_buf - 64, 64);
                memcpy(lb_buf, lb_buf - 64, 64);
                y++;
            }
            break;
        case RFX_FORMAT_RGB:
            for (y = 0; y < height; y++)
            {
                src = (uint8 *) (rgb_data + y * stride_bytes);
                lr_buf = r_buf + y * 64;
                lg_buf = g_buf + y * 64;
                lb_buf = b_buf + y * 64;
                for (x = 0; x < width; x++)
                {
                    r = *src++;
                    *lr_buf++ = r;
                    g = *src++;
                    *lg_buf++ = g;
                    b = *src++;
                    *lb_buf++ = b;
                }
                while (x < 64)
                {
                    *lr_buf++ = r;
                    *lg_buf++ = g;
                    *lb_buf++ = b;
                    x++;
                }
            }
            while (y < 64)
            {
                lr_buf = r_buf + y * 64;
                lg_buf = g_buf + y * 64;
                lb_buf = b_buf + y * 64;
                memcpy(lr_buf, lr_buf - 64, 64);
                memcpy(lg_buf, lg_buf - 64, 64);
                memcpy(lb_buf, lb_buf - 64, 64);
                y++;
            }
            break;
    }
    return 0;
}

/******************************************************************************/
static int
rfx_encode_format_argb(const char *argb_data, int width, int height,
                       int stride_bytes, int pixel_format,
                       uint8 *a_buf, uint8 *r_buf, uint8 *g_buf, uint8 *b_buf)
{
    int x;
    int y;
    const uint8 *src;
    uint8 a;
    uint8 r;
    uint8 g;
    uint8 b;
    uint8 *la_buf;
    uint8 *lr_buf;
    uint8 *lg_buf;
    uint8 *lb_buf;

    LLOGLN(10, ("rfx_encode_format_argb: pixel_format %d", pixel_format));
    b = 0;
    g = 0;
    r = 0;
    a = 0;
    switch (pixel_format)
    {
        case RFX_FORMAT_BGRA:
            for (y = 0; y < height; y++)
            {
                src = (uint8 *) (argb_data + y * stride_bytes);
                la_buf = a_buf + y * 64;
                lr_buf = r_buf + y * 64;
                lg_buf = g_buf + y * 64;
                lb_buf = b_buf + y * 64;
                for (x = 0; x < width; x++)
                {
                    b = *src++;
                    *lb_buf++ = b;
                    g = *src++;
                    *lg_buf++ = g;
                    r = *src++;
                    *lr_buf++ = r;
                    a = *src++;
                    *la_buf++ = a;
                }
                while (x < 64)
                {
                    *lb_buf++ = b;
                    *lg_buf++ = g;
                    *lr_buf++ = r;
                    *la_buf++ = a;
                    x++;
                }
            }
            while (y < 64)
            {
                la_buf = a_buf + y * 64;
                lr_buf = r_buf + y * 64;
                lg_buf = g_buf + y * 64;
                lb_buf = b_buf + y * 64;
                memcpy(la_buf, la_buf - 64, 64);
                memcpy(lr_buf, lr_buf - 64, 64);
                memcpy(lg_buf, lg_buf - 64, 64);
                memcpy(lb_buf, lb_buf - 64, 64);
                y++;
            }
            break;
        case RFX_FORMAT_RGBA:
            for (y = 0; y < height; y++)
            {
                src = (uint8 *) (argb_data + y * stride_bytes);
                la_buf = a_buf + y * 64;
                lr_buf = r_buf + y * 64;
                lg_buf = g_buf + y * 64;
                lb_buf = b_buf + y * 64;
                for (x = 0; x < width; x++)
                {
                    r = *src++;
                    *lr_buf++ = r;
                    g = *src++;
                    *lg_buf++ = g;
                    b = *src++;
                    *lb_buf++ = b;
                    a = *src++;
                    *la_buf++ = a;
                }
                while (x < 64)
                {
                    *lr_buf++ = r;
                    *lg_buf++ = g;
                    *lb_buf++ = b;
                    *la_buf++ = a;
                    x++;
                }
            }
            while (y < 64)
            {
                la_buf = a_buf + y * 64;
                lr_buf = r_buf + y * 64;
                lg_buf = g_buf + y * 64;
                lb_buf = b_buf + y * 64;
                memcpy(la_buf, la_buf - 64, 64);
                memcpy(lr_buf, lr_buf - 64, 64);
                memcpy(lg_buf, lg_buf - 64, 64);
                memcpy(lb_buf, lb_buf - 64, 64);
                y++;
            }
            break;
        case RFX_FORMAT_BGR:
            for (y = 0; y < height; y++)
            {
                src = (uint8 *) (argb_data + y * stride_bytes);
                lr_buf = r_buf + y * 64;
                lg_buf = g_buf + y * 64;
                lb_buf = b_buf + y * 64;
                for (x = 0; x < width; x++)
                {
                    b = *src++;
                    *lb_buf++ = b;
                    g = *src++;
                    *lg_buf++ = g;
                    r = *src++;
                    *lr_buf++ = r;
                }
                while (x < 64)
                {
                    *lb_buf++ = b;
                    *lg_buf++ = g;
                    *lr_buf++ = r;
                    x++;
                }
            }
            while (y < 64)
            {
                lr_buf = r_buf + y * 64;
                lg_buf = g_buf + y * 64;
                lb_buf = b_buf + y * 64;
                memcpy(lr_buf, lr_buf - 64, 64);
                memcpy(lg_buf, lg_buf - 64, 64);
                memcpy(lb_buf, lb_buf - 64, 64);
                y++;
            }
            break;
        case RFX_FORMAT_RGB:
            for (y = 0; y < height; y++)
            {
                src = (uint8 *) (argb_data + y * stride_bytes);
                lr_buf = r_buf + y * 64;
                lg_buf = g_buf + y * 64;
                lb_buf = b_buf + y * 64;
                for (x = 0; x < width; x++)
                {
                    r = *src++;
                    *lr_buf++ = r;
                    g = *src++;
                    *lg_buf++ = g;
                    b = *src++;
                    *lb_buf++ = b;
                }
                while (x < 64)
                {
                    *lr_buf++ = r;
                    *lg_buf++ = g;
                    *lb_buf++ = b;
                    x++;
                }
            }
            while (y < 64)
            {
                lr_buf = r_buf + y * 64;
                lg_buf = g_buf + y * 64;
                lb_buf = b_buf + y * 64;
                memcpy(lr_buf, lr_buf - 64, 64);
                memcpy(lg_buf, lg_buf - 64, 64);
                memcpy(lb_buf, lb_buf - 64, 64);
                y++;
            }
            break;
    }
    return 0;
}

/******************************************************************************/
/* http://msdn.microsoft.com/en-us/library/ff635643.aspx
 * 0.299   -0.168935    0.499813
 * 0.587   -0.331665   -0.418531
 * 0.114    0.50059    -0.081282
   y = r *  0.299000 + g *  0.587000 + b *  0.114000;
   u = r * -0.168935 + g * -0.331665 + b *  0.500590;
   v = r *  0.499813 + g * -0.418531 + b * -0.081282; */
/* 19595  38470   7471
  -11071 -21736  32807
   32756 -27429  -5327 */
static int
rfx_encode_rgb_to_yuv_tile(uint8 *y_r_buf, uint8 *u_g_buf, uint8 *v_b_buf)
{
    int i;
    sint32 r, g, b;
    sint32 y, u, v;

    for (i = 0; i < 4096; i++)
    {
        r = y_r_buf[i];
        g = u_g_buf[i];
        b = v_b_buf[i];

        y = (r *  19595 + g *  38470 + b *   7471) >> 16;
        u = (r * -11071 + g * -21736 + b *  32807) >> 16;
        v = (r *  32756 + g * -27429 + b *  -5327) >> 16;

        y_r_buf[i] = MINMAX(y, 0, 255);
        u_g_buf[i] = MINMAX(u + 128, 0, 255);
        v_b_buf[i] = MINMAX(v + 128, 0, 255);

    }
    return 0;
}

/******************************************************************************/
int
rfx_encode_rgb_to_yuv(struct rfxencode *enc, const char *rgb_data,
                      int width, int height, int stride_bytes)
{
    uint8 *y_r_buffer;
    uint8 *u_g_buffer;
    uint8 *v_b_buffer;

    y_r_buffer = enc->y_r_buffer;
    u_g_buffer = enc->u_g_buffer;
    v_b_buffer = enc->v_b_buffer;

    if (rfx_encode_format_rgb(rgb_data, width, height, stride_bytes,
                              enc->format,
                              y_r_buffer, u_g_buffer, v_b_buffer) != 0)
    {
        return 1;
    }
    if (rfx_encode_rgb_to_yuv_tile(y_r_buffer, u_g_buffer, v_b_buffer) != 0)
    {
        return 1;
    }
    return 0;
}

/******************************************************************************/
int
rfx_encode_argb_to_yuva(struct rfxencode *enc, const char *argb_data,
                        int width, int height, int stride_bytes)
{
    uint8 *a_buffer;
    uint8 *y_r_buffer;
    uint8 *u_g_buffer;
    uint8 *v_b_buffer;

    a_buffer = enc->a_buffer;
    y_r_buffer = enc->y_r_buffer;
    u_g_buffer = enc->u_g_buffer;
    v_b_buffer = enc->v_b_buffer;

    if (rfx_encode_format_argb(argb_data, width, height, stride_bytes,
                               enc->format, a_buffer,
                               y_r_buffer, u_g_buffer, v_b_buffer) != 0)
    {
        return 1;
    }
    if (rfx_encode_rgb_to_yuv_tile(y_r_buffer, u_g_buffer, v_b_buffer) != 0)
    {
        return 1;
    }
    return 0;
}
