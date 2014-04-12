/**
 * FreeRDP: A Remote Desktop Protocol client.
 * RemoteFX Codec Library - API Header
 *
 * Copyright 2011 Vic Lee
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rfxcodec_encode.h>

#include "rfxcommon.h"
#include "rfxencode.h"
#include "rfxconstants.h"

/******************************************************************************/
static int
rfx_compose_message_sync(struct rfxencode* enc, STREAM* s)
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
rfx_compose_message_context(struct rfxencode* enc, STREAM* s)
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
    stream_write_uint8(s, 0); /* CodecChannelT.channelId */
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
rfx_compose_message_codec_versions(struct rfxencode* enc, STREAM* s)
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
rfx_compose_message_channels(struct rfxencode* enc, STREAM* s)
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
rfx_compose_message_header(struct rfxencode* enc, STREAM* s)
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
rfx_compose_message_frame_begin(struct rfxencode* enc, STREAM* s)
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
rfx_compose_message_region(struct rfxencode* enc, STREAM* s,
                           struct rfx_rect *regions, int num_regions)
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
rfx_compose_message_frame_end(struct rfxencode* enc, STREAM* s)
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
rfx_compose_message_data(struct rfxencode* enc, STREAM* s,
                         struct rfx_rect *regions, int num_regions,
                         char *buf, int width, int height, int stride_bytes,
                         struct rfx_rect *tiles, int num_tiles, int *quant)
{
    if (rfx_compose_message_frame_begin(enc, s) != 0)
    {
        return 1;
    }
    if (rfx_compose_message_region(enc, s, regions, num_regions) != 0)
    {
        return 1;
    }



    if (rfx_compose_message_frame_end(enc, s) != 0)
    {
        return 1;
    }
    return 0;
}