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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rfxcommon.h"

#define GetNextInput do { \
    input = *coef; \
    coef++; \
    coef_size--; \
} while (0)

#define CheckWrite do { \
    while (bit_count >= 8) \
    { \
        bit_count -= 8; \
        *cdata = bits >> bit_count; \
        cdata++; \
    } \
} while (0)

/*CodeGR(krp, lmag);*/ /* output GR code for (mag - 1) */
#define CodeGR(_krp, _lmag) do { \
    int lkr = _krp >> 3; \
    /* unary part of GR code */ \
    int lvk = _lmag >> lkr; \
    /*OutputBit(lvk, 1);*/ \
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
    /*OutputBit(1, 0);*/ \
    bits <<= 1; \
    bit_count++; \
    CheckWrite; \
    /* remainder part of GR code, if needed */ \
    if (lkr) \
    { \
        /*OutputBits(lkr, _lmag & ((1 << lkr) - 1));*/ \
        bits <<= lkr; \
        bits |= _lmag & ((1 << lkr) - 1); \
        bit_count += lkr; \
    } \
    /* update _krp, only if it is not equal to 1 */ \
    if (lvk == 0) \
    { \
        /*UpdateParam(_krp, -2, lkr);*/ \
        _krp -= 2; \
        if (_krp < 0) \
        { \
            _krp = 0; \
        } \
    } \
    else if (lvk > 1) \
    { \
        /*UpdateParam(_krp, lvk, lkr);*/ \
        _krp += lvk; \
        if (_krp > 80) \
        { \
            _krp = 80; \
        } \
    } \
} while (0)

int
rfx_encode_diff_rlgr3(sint16* coef, uint8* cdata, int cdata_size)
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
    uint8* cdata_org;

    uint32 twoMs1;
    uint32 twoMs2;
    uint32 sum2Ms;
    uint32 nIdx;

    for (k = 4095; k > 4032; k--)
    {
        coef[k] -= coef[k - 1];
    }

    //rfx_bitstream_attach(bs, cdata, cdata_size);

    /* initialize the parameters */
    k = 1;
    kp = 1 << 3;
    krp = 1 << 3;

    bit_count = 0;
    bits = 0;
    cdata_org = cdata;

    /* process all the input coefficients */
    coef_size = 4096;
    while (coef_size > 0)
    {
        if (k)
        {

            /* RUN-LENGTH MODE */

            /* collect the run of zeros in the input stream */
            numZeros = 0;

            GetNextInput;
            while (input == 0 && coef_size > 0)
            {
                numZeros++;
                GetNextInput;
            }

            /* emit output zeros */
            runmax = 1 << k;
            while (numZeros >= runmax)
            {

                //OutputBit(1, 0); /* output a zero bit */
                bits <<= 1;
                bit_count++;

                CheckWrite;

                numZeros -= runmax;

                //UpdateParam(kp, UP_GR, k); /* update kp, k */
                kp += 4;
                if (kp > 80)
                {
                    kp = 80;
                }
                k = kp >> 3;

                runmax = 1 << k;
            }

            /* output a 1 to terminate runs */

            //OutputBit(1, 1);
            bits <<= 1;
            bits |= 1;
            bit_count++;

            /* output the remaining run length using k bits */

            //OutputBits(k, numZeros);
            bits <<= k;
            //bits |= numZeros & ((1 << k) - 1);
            bits |= numZeros;
            bit_count += k;

            CheckWrite;

            /* note: when we reach here and the last byte being encoded is 0, we still
               need to output the last two bits, otherwise mstsc will crash */

            /* encode the nonzero value using GR coding */
            mag = (input < 0 ? -input : input); /* absolute value of input coefficient */
            sign = (input < 0 ? 1 : 0);  /* sign of input coefficient */

            //OutputBit(1, sign); /* output the sign bit */
            bits <<= 1;
            bits |= sign;
            bit_count++;

            lmag = mag ? mag - 1 : 0;

            CodeGR(krp, lmag); /* output GR code for (mag - 1) */

            CheckWrite;

            //UpdateParam(kp, -DN_GR, k);
            kp -= 6;
            if (kp < 0)
            {
                kp = 0;
            }
            k = kp >> 3;

        }
        else
        {

            /* GOLOMB-RICE MODE */

            /* RLGR3 variant */

            /* convert the next two input values to (2*magnitude - sign) and */
            /* encode their sum using GR code */

            GetNextInput;

            //twoMs1 = Get2MagSign(input);
            y = input >> 15;
            twoMs1 = (((input ^ y) - y) << 1) + y;

            GetNextInput;

            //twoMs2 = Get2MagSign(input);
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

            //OutputBits(nIdx, twoMs1);
            bits <<= nIdx;
            bits |= twoMs1;
            bit_count += nIdx;

            CheckWrite;

            /* update k,kp for the two input values */
            if (twoMs1 != 0)
            {
                if (twoMs2 != 0)
                {
                    //UpdateParam(kp, -2 * DQ_GR, k);
                    kp -= 6;
                    if (kp < 0)
                    {
                        kp = 0;
                    }
                    k = kp >> 3;
                }
            }
            else
            {
                if (twoMs2 == 0)
                {
                    //UpdateParam(kp, 2 * UQ_GR, k);
                    kp += 6;
                    if (kp > 80)
                    {
                        kp = 80;
                    }
                    k = kp >> 3;
                }
            }
        }
    }

    if (bit_count > 0)
    {
        bits <<= 8 - bit_count;
        *cdata = bits;
        cdata++;
        bit_count = 0;
    }

    //processed_size = rfx_bitstream_get_processed_bytes(bs);
    processed_size = cdata - cdata_org;

    return processed_size;
}
