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

#ifndef __RFXENCODE_DWT_SHIFT_REM_COMMON_H
#define __RFXENCODE_DWT_SHIFT_REM_COMMON_H

/*
    LL0 -> L0, H0       - level 1 vertical

    L0 -> LL1, HL1      - level 1 horizontal
    H0 -> LH1, HH1

    LL1 -> L1, H1       - level 2 vertical

    L1 -> LL2, HL2      - level 2 horizontal
    H1 -> LH2, HH2

    LL2 -> L2, H2       - level 3 vertical

    L2 -> LL3, HL3      - level 3 horizontal
    H2 -> LH3, HH3
*/

/* level 1      LL0 -> L0, H0 */
#define SETUP_IC_LL0(_y) in_buffer + (_y)
#define SETUP_OC_L0(_y) out_buffer + (_y)
#define SETUP_OC_H0(_y) out_buffer + 64 * 33 + (_y)
#define IC_LL0_U8(_val, _offset) _val = (ic[(_offset) * 64] - 128) << DWT_REM_FACTOR
#define IC_LL0_S16(_val, _offset) _val = ic[(_offset) * 64]
#define OC_L0(_offset, _val) lo[(_offset) * 64] = _val
#define OC_H0(_offset, _val) hi[(_offset) * 64] = _val
/*              L0 -> LL1, HL1 */
#define SETUP_IC_L0(_y) in_buffer + 64 * (_y)
#define SETUP_OC_LL1(_y) out_buffer + 31 * 33 + 33 * 31 + 31 * 31 + 33 * (_y)
#define SETUP_OC_HL1(_y) out_buffer + 31 * (_y);
#define IC_L0(_val, _offset) _val = ic[(_offset)]
#define OC_LL1(_offset, _val) lo[(_offset)] = _val
#define OC_HL1(_offset, _val) hi[(_offset)] = _val
/*              H0 -> LH1, HH1 */
#define SETUP_IC_H0(_y) in_buffer + 64 * (33 + (_y))
#define SETUP_OC_LH1(_y) out_buffer + 31 * 33 + 33 * (_y)
#define SETUP_OC_HH1(_y) out_buffer + 31 * 33 + 33 * 31 + 31 * (_y)
#define IC_H0(_val, _offset) _val = ic[(_offset)]
#define OC_LH1(_offset, _val) lo[(_offset)] = _val
#define OC_HH1(_offset, _val) hi[(_offset)] = _val

/* level 2      LL1 -> L1, H1 */
#define SETUP_IC_LL1(_y) in_buffer + (_y)
#define SETUP_OC_L1(_y) out_buffer + (_y)
#define SETUP_OC_H1(_y) out_buffer + 33 * 17 + (_y)
#define IC_LL1(_val, _offset) _val = ic[(_offset) * 33]
#define OC_L1(_offset, _val) lo[(_offset) * 33] = _val
#define OC_H1(_offset, _val) hi[(_offset) * 33] = _val
/*              L1 -> LL2, HL2 */
#define SETUP_IC_L1(_y) in_buffer + 33 * (_y)
#define SETUP_OC_LL2(_y) out_buffer + 16 * 17 + 17 * 16 + 16 * 16 + 17 * (_y)
#define SETUP_OC_HL2(_y) out_buffer + 16 * (_y)
#define IC_L1(_val, _offset) _val = ic[(_offset)]
#define OC_LL2(_offset, _val) lo[(_offset)] = _val
#define OC_HL2(_offset, _val) hi[(_offset)] = _val
/*              H1 -> LH2, HH2 */
#define SETUP_IC_H1(_y) in_buffer + 33 * (17 + (_y))
#define SETUP_OC_LH2(_y) out_buffer + 16 * 17 + 17 * (_y)
#define SETUP_OC_HH2(_y) out_buffer + 16 * 17 + 17 * 16 + 16 * (_y)
#define IC_H1(_val, _offset) _val = ic[(_offset)]
#define OC_LH2(_offset, _val) lo[(_offset)] = _val
#define OC_HH2(_offset, _val) hi[(_offset)] = _val

/* level 3      LL2 -> L2, H2 */
#define SETUP_IC_LL2(_y) in_buffer + (_y)
#define SETUP_OC_L2(_y) out_buffer + (_y)
#define SETUP_OC_H2(_y) out_buffer + 17 * 9 + (_y)
#define IC_LL2(_val, _offset) _val = ic[(_offset) * 17]
#define OC_L2(_offset, _val) lo[(_offset) * 17] = _val
#define OC_H2(_offset, _val) hi[(_offset) * 17] = _val
/*              L2 -> LL3, HL3 */
#define SETUP_IC_L2(_y) in_buffer + 17 * (_y)
#define SETUP_OC_LL3(_y) out_buffer + 8 * 9 + 9 * 8 + 8 * 8 + 9 * (_y)
#define SETUP_OC_HL3(_y) out_buffer + 8 * (_y)
#define IC_L2(_val, _offset) _val = ic[(_offset)]
#define OC_LL3(_offset, _val) lo[(_offset)] = _val
#define OC_HL3(_offset, _val) hi[(_offset)] = _val
/*              H2 -> LH3, HH3 */
#define SETUP_IC_H2(_y) in_buffer + 17 * (9 + (_y))
#define SETUP_OC_LH3(_y) out_buffer + 8 * 9 + 9 * (_y)
#define SETUP_OC_HH3(_y) out_buffer + 8 * 9 + 9 * 8 + 8 * (_y)
#define IC_H2(_val, _offset) _val = ic[(_offset)]
#define OC_LH3(_offset, _val) lo[(_offset)] = _val
#define OC_HH3(_offset, _val) hi[(_offset)] = _val

#define SETUPLOQ(_index, _shift) do { \
    lo_fact = (((quants[_index] >> (_shift)) & 0xf) - 6) + DWT_REM_FACTOR; \
    lo_half = 1 << (lo_fact - 1); } while (0)
#define SETUPHIQ(_index, _shift) do { \
    hi_fact = (((quants[_index] >> (_shift)) & 0xf) - 6) + DWT_REM_FACTOR; \
    hi_half = 1 << (hi_fact - 1); } while (0)
#define LOQ(_val) ((_val) + lo_half) >> lo_fact
#define HIQ(_val) ((_val) + hi_half) >> hi_fact
#define NOQ(_val) _val

#endif
