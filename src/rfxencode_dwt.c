/**
 * FreeRDP: A Remote Desktop Protocol client.
 * RemoteFX Codec Library - DWT
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

#include "rfxcommon.h"
#include "rfxencode_dwt.h"

/******************************************************************************/
static int
rfx_dwt_2d_encode_horz(const sint16 *in_buffer, sint16 *out_buffer,
                       int subband_width)
{
    const sint16 *l_src, *h_src;
    sint16 *hl, *lh, *hh, *ll;
    int x, y;
    int n;

    /* DWT in horizontal direction, results in 4 sub-bands in
     * HL(0), LH(1), HH(2), LL(3) order, stored in original buffer. */
    /* The lower part L generates LL(3) and HL(0). */
    /* The higher part H generates LH(1) and HH(2). */

    ll = out_buffer + subband_width * subband_width * 3;
    hl = out_buffer;
    l_src = in_buffer;

    lh = out_buffer + subband_width * subband_width;
    hh = out_buffer + subband_width * subband_width * 2;
    h_src = in_buffer + subband_width * subband_width * 2;

    for (y = 0; y < subband_width; y++)
    {

        /* pre */
        hl[0] = (l_src[1] - ((l_src[0] + l_src[2]) >> 1)) >> 1;
        ll[0] = l_src[0] + hl[0];

        /* loop */
        for (n = 1; n < subband_width - 1; n++)
        {
            x = n << 1;
            hl[n] = (l_src[x + 1] - ((l_src[x] + l_src[x + 2]) >> 1)) >> 1;
            ll[n] = l_src[x] + ((hl[n - 1] + hl[n]) >> 1);
        }

        /* post */
        n = subband_width - 1;
        x = n << 1;
        hl[n] = (l_src[x + 1] - ((l_src[x] + l_src[x]) >> 1)) >> 1;
        ll[n] = l_src[x] + ((hl[n - 1] + hl[n]) >> 1);

        /* pre */
        hh[0] = (h_src[1] - ((h_src[0] + h_src[2]) >> 1)) >> 1;
        lh[0] = h_src[0] + hh[0];

        /* loop */
        for (n = 1; n < subband_width - 1; n++)
        {
            x = n << 1;
            hh[n] = (h_src[x + 1] - ((h_src[x] + h_src[x + 2]) >> 1)) >> 1;
            lh[n] = h_src[x] + ((hh[n - 1] + hh[n]) >> 1);
        }

        /* post */
        n = subband_width - 1;
        x = n << 1;
        hh[n] = (h_src[x + 1] - ((h_src[x] + h_src[x]) >> 1)) >> 1;
        lh[n] = h_src[x] + ((hh[n - 1] + hh[n]) >> 1);

        ll += subband_width;
        hl += subband_width;
        l_src += subband_width << 1;

        lh += subband_width;
        hh += subband_width;
        h_src += subband_width << 1;
    }
    return 0;
}

/******************************************************************************/
static int
rfx_dwt_2d_encode_block(sint16 *in_out_buffer, sint16 *tmp_buffer,
                        int subband_width)
{
    sint16 *src, *l, *h;
    int total_width;
    int x, y;
    int n;

    total_width = subband_width << 1;

    /* DWT in vertical direction, results in 2 sub-bands in L, H order in
     * tmp buffer. */
    for (x = 0; x < total_width; x++)
    {

        /* pre */
        l = tmp_buffer + x;
        h = l + subband_width * total_width;
        src = in_out_buffer + x;
        *h = (src[total_width] - ((src[0] + src[2 * total_width]) >> 1)) >> 1;
        *l = src[0] + (*h);

        /* loop */
        for (n = 1; n < subband_width - 1; n++)
        {
            y = n << 1;
            l = tmp_buffer + n * total_width + x;
            h = l + subband_width * total_width;
            src = in_out_buffer + y * total_width + x;
            *h = (src[total_width] - ((src[0] + src[2 * total_width]) >> 1)) >> 1;
            *l = src[0] + ((*(h - total_width) + *h) >> 1);
        }

        /* post */
        n = subband_width - 1;
        y = n << 1;
        l = tmp_buffer + n * total_width + x;
        h = l + subband_width * total_width;
        src = in_out_buffer + y * total_width + x;
        *h = (src[total_width] - ((src[0] + src[0]) >> 1)) >> 1;
        *l = src[0] + ((*(h - total_width) + *h) >> 1);

    }

    return rfx_dwt_2d_encode_horz(tmp_buffer, in_out_buffer, subband_width);
}

/******************************************************************************/
static int
rfx_dwt_2d_encode_block8(const uint8 *in_buffer,
                         sint16 *out_buffer, sint16 *tmp_buffer,
                         int subband_width)
{
    const uint8 *src;
    sint16 *l, *h;
    sint16 s1, s2, s3;
    int total_width;
    int x, y;
    int n;

    total_width = subband_width << 1;

    /* DWT in vertical direction, results in 2 sub-bands in L, H order in
     * tmp buffer. */
    for (x = 0; x < total_width; x++)
    {

        /* pre */
        l = tmp_buffer + x;
        h = l + subband_width * total_width;
        src = in_buffer + x;
        s1 = (src[total_width] - 128) << DWT_FACTOR;
        s2 = (src[0] - 128) << DWT_FACTOR;
        s3 = (src[2 * total_width] - 128) << DWT_FACTOR;
        *h = (s1 - ((s2 + s3) >> 1)) >> 1;
        s1 = (src[0] - 128) << DWT_FACTOR;
        *l = s1 + *h;

        /* loop */
        for (n = 1; n < subband_width - 1; n++)
        {
            y = n << 1;
            l = tmp_buffer + n * total_width + x;
            h = l + subband_width * total_width;
            src = in_buffer + y * total_width + x;
            s1 = (src[total_width] - 128) << DWT_FACTOR;
            s2 = (src[0] - 128) << DWT_FACTOR;
            s3 = (src[2 * total_width] - 128) << DWT_FACTOR;
            *h = (s1 - ((s2 + s3) >> 1)) >> 1;
            s1 = (src[0] - 128) << DWT_FACTOR;
            *l = s1 + ((*(h - total_width) + *h) >> 1);
        }

        /* post */
        n = subband_width - 1;
        y = n << 1;
        l = tmp_buffer + n * total_width + x;
        h = l + subband_width * total_width;
        src = in_buffer + y * total_width + x;
        s1 = (src[total_width] - 128) << DWT_FACTOR;
        s2 = (src[0] - 128) << DWT_FACTOR;
        s3 = (src[0] - 128) << DWT_FACTOR;
        *h = (s1 - ((s2 + s3) >> 1)) >> 1;
        s1 = (src[0] - 128) << DWT_FACTOR;
        *l = s1 + ((*(h - total_width) + *h) >> 1);

    }

    return rfx_dwt_2d_encode_horz(tmp_buffer, out_buffer, subband_width);
}

/******************************************************************************/
int
rfx_dwt_2d_encode(const uint8 *in_buffer, sint16 *out_buffer,
                  sint16 *tmp_buffer)
{
    rfx_dwt_2d_encode_block8(in_buffer, out_buffer, tmp_buffer, 32);
    rfx_dwt_2d_encode_block(out_buffer + 3072, tmp_buffer, 16);
    rfx_dwt_2d_encode_block(out_buffer + 3840, tmp_buffer, 8);
    return 0;
}
