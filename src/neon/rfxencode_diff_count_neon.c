/**
 * RFX codec encoder
 *
 * Copyright 2024 Jay Sorg <jay.sorg@gmail.com>
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

#include <arm_neon.h>

#include "rfxcommon.h"

#include "rfxencode_diff_count_neon.h"

static const int16x8_t g_vec_zerov = { 0, 0, 0, 0, 0, 0, 0, 0 };

/******************************************************************************/
int
rfx_encode_diff_count_neon(sint16 *diff_buffer,
                           const sint16 *dwt_buffer,
                           const sint16 *hist_buffer,
                           int *diff_zeros, int *dwt_zeros)
{
    int index;
    int ldiff_zeros = 0;
    int ldwt_zeros = 0;
    int16x8_t dwt_vec;
    int16x8_t hist_vec;
    int16x8_t diff_vec;
    int16x8_t cmp_vec;
    int16x8_t dwt_sum_vec = { 0, 0, 0, 0, 0, 0, 0, 0 };
    int16x8_t diff_sum_vec = { 0, 0, 0, 0, 0, 0, 0, 0 };

    /* diff and count for most of tile */
    for (index = 0; index < 4096 - 88; index += 8)
    {
        /* diff */
        dwt_vec = vld1q_s16(dwt_buffer + index);
        hist_vec = vld1q_s16(hist_buffer + index);
        diff_vec = vsubq_s16(dwt_vec, hist_vec);
        vst1q_s16(diff_buffer + index, diff_vec);
        /* count */
        cmp_vec = (int16x8_t) vceqq_s16(dwt_vec, g_vec_zerov);
        dwt_sum_vec = vsubq_s16(dwt_sum_vec, cmp_vec); /* sub -1 or 0 */
        cmp_vec = (int16x8_t) vceqq_s16(diff_vec, g_vec_zerov);
        diff_sum_vec = vsubq_s16(diff_sum_vec, cmp_vec); /* sub -1 or 0 */
    }
    /* diff for the rest of tile */
    while (index < 4096)
    {
        dwt_vec = vld1q_s16(dwt_buffer + index);
        hist_vec = vld1q_s16(hist_buffer + index);
        diff_vec = vsubq_s16(dwt_vec, hist_vec);
        vst1q_s16(diff_buffer + index, diff_vec);
        index += 8;
    }
    /* count for the missing part */
    for (index = 4096 - 88; index < 4096 - 81; index++)
    {
        if (diff_buffer[index] == 0)
        {
            ldiff_zeros++;
        }
        if (dwt_buffer[index] == 0)
        {
            ldwt_zeros++;
        }
    }
    ldwt_zeros += vgetq_lane_s16(dwt_sum_vec, 0);
    ldiff_zeros += vgetq_lane_s16(diff_sum_vec, 0);
    ldwt_zeros += vgetq_lane_s16(dwt_sum_vec, 1);
    ldiff_zeros += vgetq_lane_s16(diff_sum_vec, 1);
    ldwt_zeros += vgetq_lane_s16(dwt_sum_vec, 2);
    ldiff_zeros += vgetq_lane_s16(diff_sum_vec, 2);
    ldwt_zeros += vgetq_lane_s16(dwt_sum_vec, 3);
    ldiff_zeros += vgetq_lane_s16(diff_sum_vec, 3);
    ldwt_zeros += vgetq_lane_s16(dwt_sum_vec, 4);
    ldiff_zeros += vgetq_lane_s16(diff_sum_vec, 4);
    ldwt_zeros += vgetq_lane_s16(dwt_sum_vec, 5);
    ldiff_zeros += vgetq_lane_s16(diff_sum_vec, 5);
    ldwt_zeros += vgetq_lane_s16(dwt_sum_vec, 6);
    ldiff_zeros += vgetq_lane_s16(diff_sum_vec, 6);
    ldwt_zeros += vgetq_lane_s16(dwt_sum_vec, 7);
    ldiff_zeros += vgetq_lane_s16(diff_sum_vec, 7);
    *diff_zeros = ldiff_zeros;
    *dwt_zeros = ldwt_zeros;
    return 0;
}