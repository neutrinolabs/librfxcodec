/**
 * FreeRDP: A Remote Desktop Protocol client.
 * RemoteFX Codec Library - Encode
 *
 * Copyright 2011 Vic Lee
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

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rfxcommon.h"
#include "rfxencode.h"
#include "rfxencode_differential.h"
#include "rfxencode_rlgr1.h"
#include "rfxencode_rlgr3.h"
#include "rfxencode_alpha.h"
#include "rfxencode_diff_rlgr1.h"
#include "rfxencode_diff_rlgr3.h"

#include "x86/funcs_x86.h"

#define LLOG_LEVEL 1
#define LLOGLN(_level, _args) \
    do { if (_level < LLOG_LEVEL) { printf _args ; printf("\n"); } } while (0)

/******************************************************************************/
int
rfx_encode_component_rlgr1_x86_sse2(struct rfxencode *enc, const char *qtable,
                                    const uint8 *data,
                                    uint8 *buffer, int buffer_size, int *size)
{
    LLOGLN(10, ("rfx_encode_component_rlgr1_x86_sse2:"));
    if (rfxcodec_encode_dwt_shift_x86_sse2(qtable, data, enc->dwt_buffer1,
                                           enc->dwt_buffer) != 0)
    {
        return 1;
    }
    *size = rfx_encode_diff_rlgr1(enc->dwt_buffer1, buffer, buffer_size, 64);
    return 0;
}

/******************************************************************************/
int
rfx_encode_component_rlgr3_x86_sse2(struct rfxencode *enc, const char *qtable,
                                    const uint8 *data,
                                    uint8 *buffer, int buffer_size, int *size)
{
    LLOGLN(10, ("rfx_encode_component_rlgr3_x86_sse2:"));
    if (rfxcodec_encode_dwt_shift_x86_sse2(qtable, data, enc->dwt_buffer1,
                                           enc->dwt_buffer) != 0)
    {
        return 1;
    }
    *size = rfx_encode_diff_rlgr3(enc->dwt_buffer1, buffer, buffer_size, 64);
    return 0;
}

/******************************************************************************/
int
rfx_encode_component_rlgr1_x86_sse41(struct rfxencode *enc, const char *qtable,
                                     const uint8 *data,
                                     uint8 *buffer, int buffer_size, int *size)
{
    LLOGLN(10, ("rfx_encode_component_rlgr1_x86_sse41:"));
    if (rfxcodec_encode_dwt_shift_x86_sse41(qtable, data, enc->dwt_buffer1,
                                            enc->dwt_buffer) != 0)
    {
        return 1;
    }
    *size = rfx_encode_diff_rlgr1(enc->dwt_buffer1, buffer, buffer_size, 64);
    return 0;
}

/******************************************************************************/
int
rfx_encode_component_rlgr3_x86_sse41(struct rfxencode *enc, const char *qtable,
                                     const uint8 *data,
                                     uint8 *buffer, int buffer_size, int *size)
{
    LLOGLN(10, ("rfx_encode_component_rlgr3_x86_sse41:"));
    if (rfxcodec_encode_dwt_shift_x86_sse41(qtable, data, enc->dwt_buffer1,
                                            enc->dwt_buffer) != 0)
    {
        return 1;
    }
    *size = rfx_encode_diff_rlgr3(enc->dwt_buffer1, buffer, buffer_size, 64);
    return 0;
}
