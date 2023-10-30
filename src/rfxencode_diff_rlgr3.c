/**
 * FreeRDP: A Remote Desktop Protocol client.
 * RemoteFX Codec Library - RLGR
 *
 * Copyright 2011 Vic Lee
 * Copyright 2016-2017 Jay Sorg <jay.sorg@gmail.com>
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

/**
 * This implementation of RLGR refers to
 * [MS-RDPRFX] 3.1.8.1.7.3 RLGR1/RLGR3 Pseudocode
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rfxcommon.h"
#include "rfxencode_diff_rlgr3.h"

#define PIXELS_IN_TILE 4096

/* Constants used within the RLGR1/RLGR3 algorithm */
#define KPMAX   (80)  /* max value for kp or krp */
#define LSGR    (3)   /* shift count to convert kp to k */
#define UP_GR   (4)   /* increase in kp after a zero run in RL mode */
#define DN_GR   (6)   /* decrease in kp after a nonzero symbol in RL mode */
#define UQ_GR   (3)   /* increase in kp after nonzero symbol in GR mode */
#define DQ_GR   (3)   /* decrease in kp after zero symbol in GR mode */

#define GetNextInput do { \
    input = *coef; \
    coef++; \
    coef_size--; \
} while (0)

#define CheckWrite do { \
    while (bit_count >= 8) \
    { \
        if (cdata_size < 1) \
        { \
            return -1; \
        } \
        cdata_size--; \
        bit_count -= 8; \
        *cdata = bits >> bit_count; \
        cdata++; \
    } \
} while (0)

/* output GR code for (mag - 1) */
#define CodeGR(_krp, _lmag) do { \
    int lkr = _krp >> LSGR; \
    /* unary part of GR code */ \
    int lvk = _lmag >> lkr; \
    int llvk = lvk; \
    while (llvk >= 8) \
    { \
        bits <<= 8; \
        bits |= 0xFF; \
        llvk -= 8; \
        *cdata = bits >> bit_count; \
        cdata++; \
    } \
    bits <<= llvk; \
    bits |= (1 << llvk) - 1; \
    bit_count += llvk; \
    bits <<= 1; \
    bit_count++; \
    CheckWrite; \
    /* remainder part of GR code, if needed */ \
    if (lkr) \
    { \
        bits <<= lkr; \
        bits |= _lmag & ((1 << lkr) - 1); \
        bit_count += lkr; \
    } \
    /* update _krp, only if it is not equal to 1 */ \
    if (lvk == 0) \
    { \
        _krp = MAX(0, _krp - 2); \
    } \
    else if (lvk > 1) \
    { \
        _krp = MIN(KPMAX, _krp + lvk); \
    } \
} while (0)

int
rfx_encode_diff_rlgr3(sint16 *coef, uint8 *cdata, int cdata_size,
                      int diff_bytes)
{
    int k;
    int kp;
    int krp;

    int input;
    int numZeros;
    int runmax;
    int mag;
    int sign;
    int processed_size;
    int lmag;
    int coef_size;
    int y;

    int bit_count;
    unsigned int bits;
    uint8 *cdata_org;

    uint32 twoMs1;
    uint32 twoMs2;
    uint32 sum2Ms;
    uint32 nIdx;

    /* the last x bytes are diff */
    for (k = PIXELS_IN_TILE - 1; k > PIXELS_IN_TILE - diff_bytes; k--)
    {
        coef[k] -= coef[k - 1];
    }

    /* initialize the parameters */
    k = 1;
    kp = 1 << LSGR;
    krp = 1 << LSGR;

    bit_count = 0;
    bits = 0;
    cdata_org = cdata;

    /* process all the input coefficients */
    coef_size = PIXELS_IN_TILE;
    while (coef_size > 0)
    {
        if (k)
        {

            /* RUN-LENGTH MODE */

            /* collect the run of zeros in the input stream */
            numZeros = 0;

            GetNextInput;
            while (input == 0)
            {
                numZeros++;
                if (coef_size < 1)
                {
                    break;
                }
                GetNextInput;
            }

            /* emit output zeros */
            runmax = 1 << k;
            while (numZeros >= runmax)
            {

                bits <<= 1;
                bit_count++;

                CheckWrite;

                numZeros -= runmax;

                kp = MIN(KPMAX, kp + UP_GR);
                k = kp >> LSGR;

                runmax = 1 << k;
            }

            /* output a 1 to terminate runs */
            bits <<= 1;
            bits |= 1;
            bit_count++;

            /* output the remaining run length using k bits */
            bits <<= k;
            bits |= numZeros;
            bit_count += k;

            CheckWrite;

            if (input == 0)
            {
                continue;
            }

            /* encode the nonzero value using GR coding */
            if (input < 0)
            {
                mag = -input;
                sign = 1;
            }
            else
            {
                mag = input;
                sign = 0;
            }

            bits <<= 1;
            bits |= sign;
            bit_count++;

            lmag = mag - 1;

            CodeGR(krp, lmag); /* output GR code for (mag - 1) */
            CheckWrite;

            kp = MAX(0, kp - DN_GR);
            k = kp >> LSGR;

        }
        else
        {

            /* GOLOMB-RICE MODE */

            /* RLGR3 variant */

            /* convert the next two input values to (2*magnitude - sign) and */
            /* encode their sum using GR code */

            GetNextInput;

            y = input >> 15;
            twoMs1 = (((input ^ y) - y) << 1) + y;

            GetNextInput;

            y = input >> 15;
            twoMs2 = (((input ^ y) - y) << 1) + y;

            sum2Ms = twoMs1 + twoMs2;

            CodeGR(krp, sum2Ms);

            CheckWrite;

            /* encode binary representation of the first input (twoMs1). */
            if (sum2Ms != 0)
            {
                GBSR(sum2Ms, nIdx);
                nIdx++;
            }
            else
            {
                nIdx = 0;
            }

            bits <<= nIdx;
            bits |= twoMs1;
            bit_count += nIdx;

            CheckWrite;

            /* update k,kp for the two input values */
            if (twoMs1 != 0)
            {
                if (twoMs2 != 0)
                {
                    kp = MAX(0, kp - (2 * DQ_GR));
                    k = kp >> LSGR;
                }
            }
            else
            {
                if (twoMs2 == 0)
                {
                    kp = MIN(KPMAX, kp + (2 * UQ_GR));
                    k = kp >> LSGR;
                }
            }
        }
    }

    if (bit_count > 0)
    {
        if (cdata_size < 1)
        {
            return -1;
        }
        bits <<= 8 - bit_count;
        *cdata = bits;
        cdata++;
        bit_count = 0;
    }

    processed_size = cdata - cdata_org;

    return processed_size;
}
