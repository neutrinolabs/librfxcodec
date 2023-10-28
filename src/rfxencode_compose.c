/**
 * FreeRDP: A Remote Desktop Protocol client.
 * RemoteFX Codec Library
 *
 * Copyright 2011 Vic Lee
 * Copyright 2015-2017 Jay Sorg <jay.sorg@gmail.com>
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

#include "rfxencode_quantization.h"
#include "rfxencode_dwt_rem.h"
#include "rfxencode_dwt_shift_rem.h"
#include "rfxencode_diff_rlgr1.h"
#include "rfxencode_rlgr1.h"
#include "rfxencode_differential.h"
#include "rfxencode_compose.h"

#define LLOG_LEVEL 1
#define LLOGLN(_level, _args) \
    do { if (_level < LLOG_LEVEL) { printf _args ; printf("\n"); } } while (0)

/*
 * LL3, LH3, HL3, HH3, LH2, HL2, HH2, LH1, HL1, HH1
 */
static const unsigned char g_rfx_default_quantization_values[] =
{
    0x66, 0x66, 0x77, 0x88, 0x98
};

/******************************************************************************/
static int
rfx_compose_message_sync(struct rfxencode *enc, STREAM *s)
{
    if (stream_get_left(s) < 12)
    {
        return 1;
    }
    stream_write_uint16(s, WBT_SYNC); /* BlockT.blockType */
    stream_write_uint32(s, 12); /* BlockT.blockLen */
    stream_write_uint32(s, WF_MAGIC); /* magic */
    stream_write_uint16(s, WF_VERSION_1_0); /* version */
    return 0;
}

/******************************************************************************/
static int
rfx_compose_message_context(struct rfxencode *enc, STREAM *s)
{
    uint16 properties;
    int rlgr;

    if (stream_get_left(s) < 13)
    {
        return 1;
    }
    stream_write_uint16(s, WBT_CONTEXT); /* CodecChannelT.blockType */
    stream_write_uint32(s, 13); /* CodecChannelT.blockLen */
    stream_write_uint8(s, 1); /* CodecChannelT.codecId */
    stream_write_uint8(s, 255); /* CodecChannelT.channelId */
    stream_write_uint8(s, 0); /* ctxId */
    stream_write_uint16(s, CT_TILE_64x64); /* tileSize */

    /* properties */
    properties = enc->flags; /* flags */
    properties |= (COL_CONV_ICT << 3); /* cct */
    properties |= (CLW_XFORM_DWT_53_A << 5); /* xft */
    rlgr = enc->mode == RLGR1 ? CLW_ENTROPY_RLGR1 : CLW_ENTROPY_RLGR3;
    properties |= rlgr << 9; /* et */
    properties |= (SCALAR_QUANTIZATION << 13); /* qt */
    stream_write_uint16(s, properties);

    /* properties in tilesets: note that this has different format from
     * the one in TS_RFX_CONTEXT */
    properties = 1; /* lt */
    properties |= (enc->flags << 1); /* flags */
    properties |= (COL_CONV_ICT << 4); /* cct */
    properties |= (CLW_XFORM_DWT_53_A << 6); /* xft */
    properties |= rlgr << 10; /* et */
    properties |= (SCALAR_QUANTIZATION << 14); /* qt */
    enc->properties = properties;
    return 0;
}

/******************************************************************************/
static int
rfx_compose_message_codec_versions(struct rfxencode *enc, STREAM *s)
{
    if (stream_get_left(s) < 10)
    {
        return 1;
    }
    stream_write_uint16(s, WBT_CODEC_VERSIONS); /* BlockT.blockType */
    stream_write_uint32(s, 10); /* BlockT.blockLen */
    stream_write_uint8(s, 1); /* numCodecs */
    stream_write_uint8(s, 1); /* codecs.codecId */
    stream_write_uint16(s, WF_VERSION_1_0); /* codecs.version */
    return 0;
}

/******************************************************************************/
static int
rfx_compose_message_channels(struct rfxencode *enc, STREAM *s)
{
    if (stream_get_left(s) < 12)
    {
        return 1;
    }
    stream_write_uint16(s, WBT_CHANNELS); /* BlockT.blockType */
    stream_write_uint32(s, 12); /* BlockT.blockLen */
    stream_write_uint8(s, 1); /* numChannels */
    stream_write_uint8(s, 0); /* Channel.channelId */
    stream_write_uint16(s, enc->width); /* Channel.width */
    stream_write_uint16(s, enc->height); /* Channel.height */
    return 0;
}

/******************************************************************************/
int
rfx_compose_message_header(struct rfxencode *enc, STREAM *s)
{
    if (rfx_compose_message_sync(enc, s) != 0)
    {
        return 1;
    }
    if (rfx_compose_message_context(enc, s) != 0)
    {
        return 1;
    }
    if (rfx_compose_message_codec_versions(enc, s) != 0)
    {
        return 1;
    }
    if (rfx_compose_message_channels(enc, s) != 0)
    {
        return 1;
    }
    enc->header_processed = 1;
    return 0;
}

/******************************************************************************/
static int
rfx_compose_message_frame_begin(struct rfxencode *enc, STREAM *s)
{
    if (stream_get_left(s) < 14)
    {
        return 1;
    }
    stream_write_uint16(s, WBT_FRAME_BEGIN); /* CodecChannelT.blockType */
    stream_write_uint32(s, 14); /* CodecChannelT.blockLen */
    stream_write_uint8(s, 1); /* CodecChannelT.codecId */
    stream_write_uint8(s, 0); /* CodecChannelT.channelId */
    stream_write_uint32(s, enc->frame_idx); /* frameIdx */
    stream_write_uint16(s, 1); /* numRegions */
    enc->frame_idx++;
    return 0;
}

/******************************************************************************/
static int
rfx_compose_message_region(struct rfxencode *enc, STREAM *s,
                           const struct rfx_rect *regions, int num_regions)
{
    int size;
    int i;

    size = 15 + num_regions * 8;
    if (stream_get_left(s) < size)
    {
        return 1;
    }
    stream_write_uint16(s, WBT_REGION); /* CodecChannelT.blockType */
    stream_write_uint32(s, size); /* set CodecChannelT.blockLen later */
    stream_write_uint8(s, 1); /* CodecChannelT.codecId */
    stream_write_uint8(s, 0); /* CodecChannelT.channelId */
    stream_write_uint8(s, 1); /* regionFlags */
    stream_write_uint16(s, num_regions); /* numRects */
    for (i = 0; i < num_regions; i++)
    {
        stream_write_uint16(s, regions[i].x);
        stream_write_uint16(s, regions[i].y);
        stream_write_uint16(s, regions[i].cx);
        stream_write_uint16(s, regions[i].cy);
    }
    stream_write_uint16(s, CBT_REGION); /* regionType */
    stream_write_uint16(s, 1); /* numTilesets */
    return 0;
}

/******************************************************************************/
static int
rfx_compose_message_tile_yuv(struct rfxencode *enc, STREAM *s,
                             const char *tile_data,
                             int tile_width, int tile_height,
                             int stride_bytes, const char *quantVals,
                             int quantIdxY, int quantIdxCb, int quantIdxCr,
                             int xIdx, int yIdx)
{
    int YLen = 0;
    int CbLen = 0;
    int CrLen = 0;
    int start_pos;
    int end_pos;

    start_pos = stream_get_pos(s);
    stream_write_uint16(s, CBT_TILE); /* BlockT.blockType */
    stream_seek_uint32(s); /* set BlockT.blockLen later */
    stream_write_uint8(s, quantIdxY);
    stream_write_uint8(s, quantIdxCb);
    stream_write_uint8(s, quantIdxCr);
    stream_write_uint16(s, xIdx);
    stream_write_uint16(s, yIdx);
    stream_seek(s, 6); /* YLen, CbLen, CrLen */
    if (rfx_encode_yuv(enc, tile_data, tile_width, tile_height,
                       stride_bytes,
                       quantVals + quantIdxY * 5,
                       quantVals + quantIdxCb * 5,
                       quantVals + quantIdxCr * 5,
                       s, &YLen, &CbLen, &CrLen) != 0)
    {
        return 1;
    }
    LLOGLN(10, ("rfx_compose_message_tile_yuv: YLen %d CbLen %d CrLen %d",
           YLen, CbLen, CrLen));
    end_pos = stream_get_pos(s);
    stream_set_pos(s, start_pos + 2);
    stream_write_uint32(s, 19 + YLen + CbLen + CrLen); /* BlockT.blockLen */
    stream_set_pos(s, start_pos + 13);
    stream_write_uint16(s, YLen);
    stream_write_uint16(s, CbLen);
    stream_write_uint16(s, CrLen);
    stream_set_pos(s, end_pos);
    return 0;
}

/******************************************************************************/
static int
rfx_compose_message_tile_yuva(struct rfxencode *enc, STREAM *s,
                              const char *tile_data,
                              int tile_width, int tile_height,
                              int stride_bytes, const char *quantVals,
                              int quantIdxY, int quantIdxCb, int quantIdxCr,
                              int xIdx, int yIdx)
{
    int YLen = 0;
    int CbLen = 0;
    int CrLen = 0;
    int ALen = 0;
    int start_pos;
    int end_pos;

    start_pos = stream_get_pos(s);
    stream_write_uint16(s, CBT_TILE); /* BlockT.blockType */
    stream_seek_uint32(s); /* set BlockT.blockLen later */
    stream_write_uint8(s, quantIdxY);
    stream_write_uint8(s, quantIdxCb);
    stream_write_uint8(s, quantIdxCr);
    stream_write_uint16(s, xIdx);
    stream_write_uint16(s, yIdx);
    stream_seek(s, 8); /* YLen, CbLen, CrLen, ALen */
    if (rfx_encode_yuva(enc, tile_data, tile_width, tile_height,
                        stride_bytes,
                        quantVals + quantIdxY * 5,
                        quantVals + quantIdxCb * 5,
                        quantVals + quantIdxCr * 5,
                        s, &YLen, &CbLen, &CrLen, &ALen) != 0)
    {
        return 1;
    }
    end_pos = stream_get_pos(s);
    stream_set_pos(s, start_pos + 2);
    stream_write_uint32(s, 19 + YLen + CbLen + CrLen + ALen); /* BlockT.blockLen */
    stream_set_pos(s, start_pos + 13);
    stream_write_uint16(s, YLen);
    stream_write_uint16(s, CbLen);
    stream_write_uint16(s, CrLen);
    stream_write_uint16(s, ALen);
    stream_set_pos(s, end_pos);
    return 0;
}

/******************************************************************************/
static int
rfx_compose_message_tile_rgb(struct rfxencode *enc, STREAM *s,
                             const char *tile_data,
                             int tile_width, int tile_height,
                             int stride_bytes, const char *quantVals,
                             int quantIdxY, int quantIdxCb, int quantIdxCr,
                             int xIdx, int yIdx)
{
    int YLen = 0;
    int CbLen = 0;
    int CrLen = 0;
    int start_pos;
    int end_pos;

    start_pos = stream_get_pos(s);
    stream_write_uint16(s, CBT_TILE); /* BlockT.blockType */
    stream_seek_uint32(s); /* set BlockT.blockLen later */
    stream_write_uint8(s, quantIdxY);
    stream_write_uint8(s, quantIdxCb);
    stream_write_uint8(s, quantIdxCr);
    stream_write_uint16(s, xIdx);
    stream_write_uint16(s, yIdx);
    stream_seek(s, 6); /* YLen, CbLen, CrLen */
    if (rfx_encode_rgb(enc, tile_data, tile_width, tile_height,
                       stride_bytes,
                       quantVals + quantIdxY * 5,
                       quantVals + quantIdxCb * 5,
                       quantVals + quantIdxCr * 5,
                       s, &YLen, &CbLen, &CrLen) != 0)
    {
        return 1;
    }
    end_pos = stream_get_pos(s);
    stream_set_pos(s, start_pos + 2);
    stream_write_uint32(s, 19 + YLen + CbLen + CrLen); /* BlockT.blockLen */
    stream_set_pos(s, start_pos + 13);
    stream_write_uint16(s, YLen);
    stream_write_uint16(s, CbLen);
    stream_write_uint16(s, CrLen);
    stream_set_pos(s, end_pos);
    return 0;
}

/******************************************************************************/
static int
rfx_compose_message_tile_argb(struct rfxencode *enc, STREAM *s,
                              const char *tile_data,
                              int tile_width, int tile_height,
                              int stride_bytes, const char *quantVals,
                              int quantIdxY, int quantIdxCb, int quantIdxCr,
                              int xIdx, int yIdx)
{
    int YLen = 0;
    int CbLen = 0;
    int CrLen = 0;
    int ALen = 0;
    int start_pos;
    int end_pos;

    LLOGLN(10, ("rfx_compose_message_tile_argb:"));
    start_pos = stream_get_pos(s);
    stream_write_uint16(s, CBT_TILE); /* BlockT.blockType */
    stream_seek_uint32(s); /* set BlockT.blockLen later */
    stream_write_uint8(s, quantIdxY);
    stream_write_uint8(s, quantIdxCb);
    stream_write_uint8(s, quantIdxCr);
    stream_write_uint16(s, xIdx);
    stream_write_uint16(s, yIdx);
    stream_seek(s, 8); /* YLen, CbLen, CrLen, ALen */
    if (rfx_encode_argb(enc, tile_data, tile_width, tile_height,
                        stride_bytes,
                        quantVals + quantIdxY * 5,
                        quantVals + quantIdxCb * 5,
                        quantVals + quantIdxCr * 5,
                        s, &YLen, &CbLen, &CrLen, &ALen) != 0)
    {
        LLOGLN(10, ("rfx_compose_message_tile_argb: rfx_encode_argb failed"));
        return 1;
    }
    end_pos = stream_get_pos(s);
    stream_set_pos(s, start_pos + 2);
    stream_write_uint32(s, 19 + YLen + CbLen + CrLen + ALen); /* BlockT.blockLen */
    stream_set_pos(s, start_pos + 13);
    stream_write_uint16(s, YLen);
    stream_write_uint16(s, CbLen);
    stream_write_uint16(s, CrLen);
    stream_write_uint16(s, ALen);
    stream_set_pos(s, end_pos);
    return 0;
}

/******************************************************************************/
static int
rfx_compose_message_tileset(struct rfxencode *enc, STREAM *s,
                            const char *buf, int width, int height,
                            int stride_bytes,
                            const struct rfx_tile *tiles, int num_tiles,
                            const char *quants, int num_quants,
                            int flags)
{
    int size;
    int start_pos;
    int end_pos;
    int tiles_end_checkpoint;
    int tiles_written;
    int index;
    int numQuants;
    const char *quantVals;
    int quantIdxY;
    int quantIdxCb;
    int quantIdxCr;
    int numTiles;
    int tilesDataSize;
    int x;
    int y;
    int cx;
    int cy;
    const char *tile_data;

    LLOGLN(10, ("rfx_compose_message_tileset:"));
    if (quants == 0)
    {
        numQuants = 1;
        quantVals = (const char *) g_rfx_default_quantization_values;
    }
    else
    {
        numQuants = num_quants;
        quantVals = quants;
    }
    numTiles = num_tiles;
    size = 22 + numQuants * 5;
    start_pos = stream_get_pos(s);
    if (flags & RFX_FLAGS_ALPHAV1)
    {
        LLOGLN(10, ("rfx_compose_message_tileset: RFX_FLAGS_ALPHAV1 set"));
        stream_write_uint16(s, WBT_EXTENSION_PLUS); /* CodecChannelT.blockType */
    }
    else
    {
        stream_write_uint16(s, WBT_EXTENSION); /* CodecChannelT.blockType */
    }
    stream_seek_uint32(s); /* set CodecChannelT.blockLen later */
    stream_write_uint8(s, 1); /* CodecChannelT.codecId */
    stream_write_uint8(s, 0); /* CodecChannelT.channelId */
    stream_write_uint16(s, CBT_TILESET); /* subtype */
    stream_write_uint16(s, 0); /* idx */
    stream_write_uint16(s, enc->properties); /* properties */
    stream_write_uint8(s, numQuants); /* numQuants */
    stream_write_uint8(s, 0x40); /* tileSize */
    stream_write_uint16(s, numTiles); /* numTiles */
    stream_seek_uint32(s); /* set tilesDataSize later */
    memcpy(s->p, quantVals, numQuants * 5);
    s->p += numQuants * 5;
    end_pos = stream_get_pos(s);
    tiles_written = 0;
    tiles_end_checkpoint = stream_get_pos(s);

    if (enc->format == RFX_FORMAT_YUV)
    {
        if (flags & RFX_FLAGS_ALPHAV1)
        {
            for (index = 0; index < numTiles; index++)
            {
                x = tiles[index].x;
                y = tiles[index].y;
                cx = tiles[index].cx;
                cy = tiles[index].cy;
                quantIdxY = tiles[index].quant_y;
                quantIdxCb = tiles[index].quant_cb;
                quantIdxCr = tiles[index].quant_cr;
                tile_data = buf + (y << 8) * (stride_bytes >> 8) + (x << 8);
                if (rfx_compose_message_tile_yuva(enc, s,
                                                  tile_data, cx, cy, stride_bytes,
                                                  quantVals,
                                                  quantIdxY, quantIdxCb, quantIdxCr,
                                                  x / 64, y / 64) != 0)
                {
                    break;
                }
                tiles_end_checkpoint = stream_get_pos(s);
                tiles_written += 1;
            }
        }
        else
        {
            for (index = 0; index < numTiles; index++)
            {
                x = tiles[index].x;
                y = tiles[index].y;
                cx = tiles[index].cx;
                cy = tiles[index].cy;
                quantIdxY = tiles[index].quant_y;
                quantIdxCb = tiles[index].quant_cb;
                quantIdxCr = tiles[index].quant_cr;
                tile_data = buf + (y << 8) * (stride_bytes >> 8) + (x << 8);
                if (rfx_compose_message_tile_yuv(enc, s,
                                                 tile_data, cx, cy, stride_bytes,
                                                 quantVals,
                                                 quantIdxY, quantIdxCb, quantIdxCr,
                                                 x / 64, y / 64) != 0)
                {
                    break;
                }
                tiles_end_checkpoint = stream_get_pos(s);
                tiles_written += 1;
            }
        }
    }
    else
    {
        if (flags & RFX_FLAGS_ALPHAV1)
        {
            for (index = 0; index < numTiles; index++)
            {
                x = tiles[index].x;
                y = tiles[index].y;
                cx = tiles[index].cx;
                cy = tiles[index].cy;
                quantIdxY = tiles[index].quant_y;
                quantIdxCb = tiles[index].quant_cb;
                quantIdxCr = tiles[index].quant_cr;
                tile_data = buf + y * stride_bytes + x * (enc->bits_per_pixel / 8);
                if (rfx_compose_message_tile_argb(enc, s,
                                                  tile_data, cx, cy, stride_bytes,
                                                  quantVals,
                                                  quantIdxY, quantIdxCb, quantIdxCr,
                                                  x / 64, y / 64) != 0)
                {
                    break;
                }
                tiles_end_checkpoint = stream_get_pos(s);
                tiles_written += 1;
            }
        }
        else
        {
            for (index = 0; index < numTiles; index++)
            {
                x = tiles[index].x;
                y = tiles[index].y;
                cx = tiles[index].cx;
                cy = tiles[index].cy;
                quantIdxY = tiles[index].quant_y;
                quantIdxCb = tiles[index].quant_cb;
                quantIdxCr = tiles[index].quant_cr;
                tile_data = buf + y * stride_bytes + x * (enc->bits_per_pixel / 8);
                if (rfx_compose_message_tile_rgb(enc, s,
                                                 tile_data, cx, cy, stride_bytes,
                                                 quantVals,
                                                 quantIdxY, quantIdxCb, quantIdxCr,
                                                 x / 64, y / 64) != 0)
                {
                    break;
                }
                tiles_end_checkpoint = stream_get_pos(s);
                tiles_written += 1;
            }
        }
    }
    tilesDataSize = tiles_end_checkpoint - end_pos;
    size += tilesDataSize;
    end_pos = tiles_end_checkpoint;
    stream_set_pos(s, start_pos + 2);
    stream_write_uint32(s, size); /* CodecChannelT.blockLen */
    stream_set_pos(s, start_pos + 16);
    stream_write_uint16(s, tiles_written);
    stream_set_pos(s, start_pos + 18);
    stream_write_uint32(s, tilesDataSize);
    stream_set_pos(s, end_pos);
    return tiles_written;
}

/******************************************************************************/
static int
rfx_compose_message_frame_end(struct rfxencode *enc, STREAM *s)
{
    if (stream_get_left(s) < 8)
    {
        return 1;
    }
    stream_write_uint16(s, WBT_FRAME_END); /* CodecChannelT.blockType */
    stream_write_uint32(s, 8); /* CodecChannelT.blockLen */
    stream_write_uint8(s, 1); /* CodecChannelT.codecId */
    stream_write_uint8(s, 0); /* CodecChannelT.channelId */
    return 0;
}

/******************************************************************************/
int
rfx_compose_message_data(struct rfxencode *enc, STREAM *s,
                         const struct rfx_rect *regions, int num_regions,
                         const char *buf, int width, int height,
                         int stride_bytes,
                         const struct rfx_tile *tiles, int num_tiles,
                         const char *quants, int num_quants, int flags)
{
    int tiles_written;
    if (rfx_compose_message_frame_begin(enc, s) != 0)
    {
        return -1;
    }
    if (rfx_compose_message_region(enc, s, regions, num_regions) != 0)
    {
        return -1;
    }
    tiles_written = rfx_compose_message_tileset(enc, s, buf, width, height, stride_bytes,
                   tiles, num_tiles, quants, num_quants, flags);
    if (rfx_compose_message_frame_end(enc, s) != 0)
    {
        return -1;
    }
    return tiles_written;
}

/******************************************************************************/
static int
rfx_pro_compose_message_context(struct rfxencode *enc, STREAM *s)
{
    if (stream_get_left(s) < 10)
    {
        return 1;
    }
    stream_write_uint16(s, PRO_WBT_CONTEXT);
    stream_write_uint32(s, 10);
    stream_write_uint8(s, 0); /* ctxId */
    stream_write_uint16(s, CT_TILE_64x64); /* tileSize */
    stream_write_uint8(s, RFX_SUBBAND_DIFFING); /* flags */
    return 0;
}

/******************************************************************************/
int
rfx_pro_compose_message_header(struct rfxencode *enc, STREAM *s)
{
    if (rfx_compose_message_sync(enc, s) != 0)
    {
        return 1;
    }
    if (rfx_pro_compose_message_context(enc, s) != 0)
    {
        return 1;
    }
    enc->header_processed = 1;
    return 0;
}

/******************************************************************************/
static int
rfx_pro_compose_message_frame_begin(struct rfxencode *enc, STREAM *s)
{
    if (stream_get_left(s) < 12)
    {
        return 1;
    }
    stream_write_uint16(s, PRO_WBT_FRAME_BEGIN);
    stream_write_uint32(s, 12);
    stream_write_uint32(s, enc->frame_idx);
    stream_write_uint16(s, 1);
    enc->frame_idx++;
    return 0;
}

/******************************************************************************/
/* coef1 = coef2 - coef3 (QCdt = QCot - QCrb)
   count zeros in coef1, coef2
   coef3 = coef2 */
#define COEF_DIFF_COUNT_COPY(_coef1, _coef2, _coef3, _loop, _count1, _count2) \
do { _count1 = 0; _count2 = 0; \
    for (_loop = 0; _loop < 4096 - 81; _loop++) { \
        _coef1[_loop] = _coef2[_loop] - _coef3[_loop]; \
        if (_coef1[_loop] == 0) { _count1++; } \
        if (_coef2[_loop] == 0) { _count2++; } \
        _coef3[_loop] = _coef2[_loop]; } \
    while (_loop < 4096) { \
        _coef1[_loop] = _coef2[_loop] - _coef3[_loop]; \
        _coef3[_loop] = _coef2[_loop]; _loop++; } \
} while (0)

/******************************************************************************/
/* coef1 = coef2 - coef3 (QCdt = QCot - QCrb)
   count zeros in coef1, coef2 */
#define COEF_DIFF_COUNT(_coef1, _coef2, _coef3, _loop, _count1, _count2) \
do { _count1 = 0; _count2 = 0; \
    for (_loop = 0; _loop < 4096 - 81; _loop++) { \
        _coef1[_loop] = _coef2[_loop] - _coef3[_loop]; \
        if (_coef1[_loop] == 0) { _count1++; } \
        if (_coef2[_loop] == 0) { _count2++; } } \
    while (_loop < 4096) { \
        _coef1[_loop] = _coef2[_loop] - _coef3[_loop]; _loop++; } \
} while (0)

/******************************************************************************/
/* coef1 = coef2 - coef3 (QCdt = QCot - QCrb)
   coef3 = coef2 */
#define COEF_DIFF_COPY(_coef1, _coef2, _coef3, _loop) \
do { \
    for (_loop = 0; _loop < 4096; _loop++) { \
        _coef1[_loop] = _coef2[_loop] - _coef3[_loop]; \
        _coef3[_loop] = _coef2[_loop]; } \
} while (0)

/******************************************************************************/
/* coef1 = coef2 - coef3 (QCdt = QCot - QCrb) */
#define COEF_DIFF(_coef1, _coef2, _coef3, _loop) \
do { \
    for (_loop = 0; _loop < 4096; _loop++) { \
        _coef1[_loop] = _coef2[_loop] - _coef3[_loop]; } \
} while (0)

/******************************************************************************/
static int
rfx_pro_compose_message_region(struct rfxencode *enc, STREAM *s,
                               const struct rfx_rect *regions, int num_regions,
                               const char *buf, int width, int height,
                               int stride_bytes,
                               const struct rfx_tile *tiles, int num_tiles,
                               const char *quants, int num_quants,
                               int flags)
{
    int index;
    int jndex;
    int start_pos;
    int tiles_start_pos;
    int end_pos;
    int tiles_written;
    int x;
    int y;
    uint8 quantIdxY;
    uint8 quantIdxCb;
    uint8 quantIdxCr;
    const char *tile_data;

    int y_bytes;
    int u_bytes;
    int v_bytes;
    int tile_start_pos;
    int tile_end_pos;
    uint16 xIdx;
    uint16 yIdx;

    const uint8 *y_buffer;
    const uint8 *u_buffer;
    const uint8 *v_buffer;
    const char *y_quants;
    const char *u_quants;
    const char *v_quants;

    struct rfx_rb *rb;
    int dt_y_zeros;
    int dt_u_zeros;
    int dt_v_zeros;
    int ot_y_zeros;
    int ot_u_zeros;
    int ot_v_zeros;
    int tile_flags;
    sint16 *dwt_buffer_y;
    sint16 *dwt_buffer_u;
    sint16 *dwt_buffer_v;

    if (stream_get_left(s) < 18 + num_regions * 8 + num_quants * 5)
    {
        return 1;
    }
    if (quants == NULL)
    {
        num_quants = 1;
        quants = (const char *) g_rfx_default_quantization_values;
    }
    start_pos = stream_get_pos(s);
    stream_write_uint16(s, PRO_WBT_REGION);
    stream_seek_uint32(s); /* blockLen, set later */
    stream_write_uint8(s, CT_TILE_64x64);
    stream_write_uint16(s, num_regions);
    stream_write_uint8(s, num_quants);
    stream_write_uint8(s, 0); /* numProgQuant */
    stream_write_uint8(s, RFX_DWT_REDUCE_EXTRAPOLATE); /* flags */
    stream_write_uint16(s, num_tiles);
    stream_seek_uint32(s); /* tileDataSize, set later */
    for (index = 0; index < num_regions; index++)
    {
        stream_write_uint16(s, regions[index].x);
        stream_write_uint16(s, regions[index].y);
        stream_write_uint16(s, regions[index].cx);
        stream_write_uint16(s, regions[index].cy);
    }
    stream_write(s, quants, num_quants * 5);
    tiles_start_pos = stream_get_pos(s);
    tiles_written = 0;
    for (index = 0; index < num_tiles; index++)
    {
        if (stream_get_left(s) < 22)
        {
            return 1;
        }
        x = tiles[index].x;
        y = tiles[index].y;
        quantIdxY = tiles[index].quant_y;
        quantIdxCb = tiles[index].quant_cb;
        quantIdxCr = tiles[index].quant_cr;
        if ((quantIdxY >= num_quants) || (quantIdxCb >= num_quants) ||
            (quantIdxCr >= num_quants))
        {
            return 1;
        }
        tile_data = buf + (y << 8) * (stride_bytes >> 8) + (x << 8);
        xIdx = x / 64;
        yIdx = y / 64;
        if ((xIdx >= RFX_MAX_RB_X) || (yIdx >= RFX_MAX_RB_Y))
        {
            return 1;
        }
        tile_start_pos = stream_get_pos(s);
        stream_write_uint16(s, PRO_WBT_TILE_SIMPLE);
        stream_seek_uint32(s); /* set later */
        stream_write_uint8(s, quantIdxY);
        stream_write_uint8(s, quantIdxCb);
        stream_write_uint8(s, quantIdxCr);
        stream_write_uint16(s, xIdx);
        stream_write_uint16(s, yIdx);
        stream_seek(s, 1); /* flags, set later */
        stream_seek(s, 8); /* yLen, cbLen, crLen, tailLen, set later */
        y_buffer = (const uint8 *) tile_data;
        u_buffer = (const uint8 *) (tile_data + RFX_YUV_BTES);
        v_buffer = (const uint8 *) (tile_data + RFX_YUV_BTES * 2);
        y_quants = quants + quantIdxY * 5;
        u_quants = quants + quantIdxCb * 5;
        v_quants = quants + quantIdxCr * 5;
        rb = enc->rbs[xIdx][yIdx];
        if (rb == NULL)
        {
            rb = xnew(struct rfx_rb);
            if (rb == NULL)
            {
                return 1;
            }
            enc->rbs[xIdx][yIdx] = rb;
        }
        rfx_rem_dwt_shift_encode(y_buffer, enc->dwt_buffer1,
                                 enc->dwt_buffer, y_quants);
        rfx_rem_dwt_shift_encode(u_buffer, enc->dwt_buffer2,
                                 enc->dwt_buffer, u_quants);
        rfx_rem_dwt_shift_encode(v_buffer, enc->dwt_buffer3,
                                 enc->dwt_buffer, v_quants);
        COEF_DIFF_COUNT_COPY(enc->dwt_buffer4, enc->dwt_buffer1, rb->y,
                             jndex, dt_y_zeros, ot_y_zeros);
        COEF_DIFF_COUNT_COPY(enc->dwt_buffer5, enc->dwt_buffer2, rb->u,
                             jndex, dt_u_zeros, ot_u_zeros);
        COEF_DIFF_COUNT_COPY(enc->dwt_buffer6, enc->dwt_buffer3, rb->v,
                             jndex, dt_v_zeros, ot_v_zeros);
        if (ot_y_zeros + ot_u_zeros + ot_v_zeros <
            dt_y_zeros + dt_u_zeros + dt_v_zeros)
        {
            LLOGLN(10, ("rfx_pro_compose_message_region: diff"));
            tile_flags = RFX_TILE_DIFFERENCE;
            dwt_buffer_y = enc->dwt_buffer4;
            dwt_buffer_u = enc->dwt_buffer5;
            dwt_buffer_v = enc->dwt_buffer6;
        }
        else
        {
            LLOGLN(10, ("rfx_pro_compose_message_region: orig"));
            tile_flags = 0;
            dwt_buffer_y = enc->dwt_buffer1;
            dwt_buffer_u = enc->dwt_buffer2;
            dwt_buffer_v = enc->dwt_buffer3;
        }
        y_bytes = rfx_encode_diff_rlgr1(dwt_buffer_y,
                                        stream_get_tail(s),
                                        stream_get_left(s), 81);
        if (y_bytes < 0)
        {
            return 1;
        }
        stream_seek(s, y_bytes);
        u_bytes = rfx_encode_diff_rlgr1(dwt_buffer_u,
                                        stream_get_tail(s),
                                        stream_get_left(s), 81);
        if (u_bytes < 0)
        {
            return 1;
        }
        stream_seek(s, u_bytes);
        v_bytes = rfx_encode_diff_rlgr1(dwt_buffer_v,
                                        stream_get_tail(s),
                                        stream_get_left(s), 81);
        if (v_bytes < 0)
        {
            return 1;
        }
        stream_seek(s, v_bytes);
        LLOGLN(10, ("rfx_pro_compose_message_region: y_bytes %d "
               "u_bytes %d v_bytes %d", y_bytes, u_bytes, v_bytes));
        tile_end_pos = stream_get_pos(s);
        stream_set_pos(s, tile_start_pos + 2);
        stream_write_uint32(s, tile_end_pos - tile_start_pos); /* blockLen */
        stream_set_pos(s, tile_start_pos + 13);
        stream_write_uint8(s, tile_flags); /* flags */
        stream_write_uint16(s, y_bytes); /* yLen */
        stream_write_uint16(s, u_bytes); /* cbLen */
        stream_write_uint16(s, v_bytes); /* crLen */
        stream_write_uint16(s, 0); /* tailLen */
        stream_set_pos(s, tile_end_pos);
        ++tiles_written;
    }
    end_pos = stream_get_pos(s);
    stream_set_pos(s, start_pos + 2);
    stream_write_uint32(s, end_pos - start_pos); /* blockLen */
    stream_set_pos(s, start_pos + 14);
    stream_write_uint32(s, end_pos - tiles_start_pos); /* tileDataSize */
    stream_set_pos(s, end_pos);
    return tiles_written;
}

/******************************************************************************/
static int
rfx_pro_compose_message_frame_end(struct rfxencode *enc, STREAM *s)
{
    if (stream_get_left(s) < 6)
    {
        return 1;
    }
    stream_write_uint16(s, PRO_WBT_FRAME_END);
    stream_write_uint32(s, 6);
    return 0;
}

/******************************************************************************/
int
rfx_pro_compose_message_data(struct rfxencode *enc, STREAM *s,
                             const struct rfx_rect *regions, int num_regions,
                             const char *buf, int width, int height,
                             int stride_bytes,
                             const struct rfx_tile *tiles, int num_tiles,
                             const char *quants, int num_quants,
                             int flags)
{
    int tiles_written;
    LLOGLN(10, ("rfx_pro_compose_message_data:"));
    if (rfx_pro_compose_message_frame_begin(enc, s) != 0)
    {
        return -1;
    }
    tiles_written = rfx_pro_compose_message_region(enc, s, regions, num_regions,
                                   buf, width, height, stride_bytes,
                                   tiles, num_tiles, quants, num_quants,
                                   flags);
    if (tiles_written <= 0)
    {
        return -1;
    }
    if (rfx_pro_compose_message_frame_end(enc, s) != 0)
    {
        return -1;
    }
    return tiles_written;
}
