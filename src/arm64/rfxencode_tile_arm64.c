/**
 * FreeRDP: A Remote Desktop Protocol client.
 * RemoteFX Codec Library - Encode
 *
 * Copyright 2011 Vic Lee
 * Copyright 2014-2015 Jay Sorg <jay.sorg@gmail.com>
 * Copyright 2022 Gyuhwan Park <unstabler@unstabler.pl>
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
#include "rfxencode.h"
#include "rfxencode_differential.h"
#include "rfxencode_rlgr1.h"
#include "rfxencode_rlgr3.h"
#include "rfxencode_alpha.h"
#include "rfxencode_diff_rlgr1.h"
#include "rfxencode_diff_rlgr3.h"

#include "arm64/funcs_arm64.h"
#include "fakesse.h"


#define LLOG_LEVEL 1
#define LLOGLN(_level, _args) \
    do { if (_level < LLOG_LEVEL) { printf _args ; printf("\n"); } } while (0)

union virt_register {
    uint8_t al;
    uint16_t ax;
    uint32_t eax;
    uint64_t rax;
};

struct cpu_context {
    int16x8_t xmm0;
    int16x8_t xmm1;
    int16x8_t xmm2;
    int16x8_t xmm3;
    int16x8_t xmm4;
    int16x8_t xmm5;
    int16x8_t xmm6;
    int16x8_t xmm7;
    int16x8_t xmm8;
    int16x8_t xmm9;
    int16x8_t xmm10;
    int16x8_t xmm11;
    int16x8_t xmm12;
    int16x8_t xmm13;
    int16x8_t xmm14;
    int16x8_t xmm15;
    
    union virt_register rax;
    union virt_register rbx;
    int64_t rcx;
    int64_t rdx;
};

// PREPARE_RODATA
const static int16x8_t cw128 = { 128, 128, 128, 128, 128, 128, 128, 128 };
const static int32x4_t cdFFFF = { 65535, 65535, 65535, 65535 };

const static int16x8_t cwa0 = { 0, 0, 0, 0, 0, 0, 0, 0 };
const static int16x8_t cwa1 = { 1, 1, 1, 1, 1, 1, 1, 1 };
const static int16x8_t cwa2 = { 2, 2, 2, 2, 2, 2, 2, 2 };
const static int16x8_t cwa4 = { 4, 4, 4, 4, 4, 4, 4, 4 };
const static int16x8_t cwa8 = { 8, 8, 8, 8, 8, 8, 8, 8 };
const static int16x8_t cwa16 = { 16, 16, 16, 16, 16, 16, 16, 16 };
const static int16x8_t cwa32 = { 32, 32, 32, 32, 32, 32, 32, 32 };
const static int16x8_t cwa64 = { 64, 64, 64, 64, 64, 64, 64, 64 };
const static int16x8_t cwa128 = { 128, 128, 128, 128, 128, 128, 128, 128 };
const static int16x8_t cwa256 = { 256, 256, 256, 256, 256, 256, 256, 256 };
const static int16x8_t cwa512 = { 512, 512, 512, 512, 512, 512, 512, 512 };
const static int16x8_t cwa1024 = { 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024 };
const static int16x8_t cwa2048 = { 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048 };
const static int16x8_t cwa4096 = { 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096 };
const static int16x8_t cwa8192 = { 8192, 8192, 8192, 8192, 8192, 8192, 8192, 8192 };
const static int16x8_t cwa16384 = { 16384, 16384, 16384, 16384, 16384, 16384, 16384, 16384 };


const static int16x8_t RODATA[] = {
    cwa0,
    cwa1,
    cwa2,
    cwa4,
    cwa8,
    cwa16,
    cwa32,
    cwa64,
    cwa128,
    cwa256,
    cwa512,
    cwa1024,
    cwa2048,
    cwa4096,
    cwa8192,
    cwa16384
};

void
rfx_dwt_2d_encode_block_horiz_16_16(struct cpu_context *context,
                                    const uint8_t *in_buffer,
                                    uint8_t *out_hi_buffer,
                                    uint8_t *out_lo_buffer)
{
    int32_t ecx = 8;
    // loop1a
    while (ecx != 0) {
        // pre / post
        load_dqword(context->xmm1, in_buffer);                  // src[2n]
        load_dqword(context->xmm2, in_buffer + 16);
        movdqa(&context->xmm6, context->xmm1);
        movdqa(&context->xmm7, context->xmm2);
        pand(context->xmm1, cdFFFF);
        pand(context->xmm2, cdFFFF);
        packusdw(context->xmm1, context->xmm2);
        movdqa(&context->xmm2, context->xmm6);                   // src[2n + 1]
        movdqa(&context->xmm3, context->xmm7);
        psrldq(context->xmm2, 2);
        psrldq(context->xmm3, 2);
        pand(context->xmm2, cdFFFF);
        pand(context->xmm3, cdFFFF);
        packusdw(context->xmm2, context->xmm3);
        movdqa(&context->xmm3, context->xmm6);                   // src[2n + 2]
        movdqa(&context->xmm4, context->xmm7);
        psrldq(context->xmm3, 4);
        psrldq(context->xmm4, 4);
        store_dword(context->rax.eax, context->xmm7);
        load_dword(context->xmm5, context->rax.eax);
        pslldq(context->xmm5, 12);
        por(context->xmm3, context->xmm5);
        movdqa(&context->xmm5, context->xmm7);
        psrldq(context->xmm5, 12);
        pslldq(context->xmm5, 12);
        por(context->xmm4, context->xmm5);
        pand(context->xmm3, cdFFFF);
        pand(context->xmm4, cdFFFF);
        packusdw(context->xmm3, context->xmm4);
        movdqa(&context->xmm4, context->xmm1);
        movdqa(&context->xmm5, context->xmm2);
        movdqa(&context->xmm6, context->xmm3);
        // h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
        paddw(context->xmm4, context->xmm6);
        psraw(context->xmm4, 1);
        psubw(context->xmm5, context->xmm4);
        psraw(context->xmm5, 1);
        movdqa(&context->xmm6, context->xmm5);                   // out hi
        paddw(context->xmm6, context->xmm8);
        psraw_vec(context->xmm6, context->xmm9);
        movdqa(out_hi_buffer, context->xmm6);
        // l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
        movdqa(&context->xmm7, context->xmm5);
        store_dword(context->rax.eax, context->xmm7);
        pslldq(context->xmm7, 2);
        context->rax.eax = context->rax.eax & 0xFFFF;
        load_dword(context->xmm6, context->rax.eax);
        por(context->xmm7, context->xmm6);
        paddw(context->xmm5, context->xmm7);
        psraw(context->xmm5, 1);
        paddw(context->xmm5, context->xmm1);
    
        movdqa(&context->xmm6, context->xmm5);                   // out lo
        paddw(context->xmm6, context->xmm10);
        psraw_vec(context->xmm6, context->xmm11);
        movdqa(out_lo_buffer, context->xmm6);
    
        // move right
        in_buffer = in_buffer + 16 * 2;
        out_hi_buffer = out_hi_buffer + 8 * 2;
        out_lo_buffer = out_lo_buffer + 8 * 2;
    
        // move left
        in_buffer = in_buffer - (16 * 2);
        out_hi_buffer = out_hi_buffer - (8 * 2);
        out_lo_buffer = out_lo_buffer - (8 * 2);
    
        // move down
        in_buffer = in_buffer + 16 * 2;
        out_hi_buffer = out_hi_buffer + 8 * 2;
        out_lo_buffer = out_lo_buffer + 8 * 2;
    
        ecx--;
    }
}

void
rfx_dwt_2d_encode_block_verti_16_16(struct cpu_context *context,
                                    const uint8_t *in_buffer,
                                    uint8_t *out_hi_buffer,
                                    uint8_t *out_lo_buffer)
{
    int16_t cx = 0;
    int32_t ecx = 2;
    
    // loop1b
    while (ecx != 0) {
        // pre
        load_dqword(context->xmm1, in_buffer);                  // src[2n]
        load_dqword(context->xmm2, in_buffer + 16 * 2);         // src[2n + 1]
        load_dqword(context->xmm3, in_buffer + 16 * 2 * 2);     // src[2n + 2]
        movdqa(&context->xmm4, context->xmm1);
        movdqa(&context->xmm5, context->xmm2);
        movdqa(&context->xmm6, context->xmm3);
        // h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
        paddw(context->xmm4, context->xmm6);
        psraw(context->xmm4, 1);
        psubw(context->xmm5, context->xmm4);
        psraw(context->xmm5, 1);
        movdqa(out_hi_buffer, context->xmm5);                  // out hi
        movdqa(&context->xmm6, context->xmm5);                   // save hi
        // l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
        paddw(context->xmm5, context->xmm1);
        movdqa(out_lo_buffer, context->xmm5);                  // out lo
        movdqa(&context->xmm7, context->xmm6);                   // save hi
        // move down
        in_buffer = in_buffer + 16 * 2 * 2;         // 2 rows
        out_hi_buffer = out_hi_buffer + 16 * 2;             // 1 row
        out_lo_buffer = out_lo_buffer + 16 * 2;             // 1 row
    
        // loop
        cx = 6;
        // loop2b
        while (cx != 0) {
            movdqa(&context->xmm1, context->xmm3);                   // src[2n]
            load_dqword(context->xmm2, in_buffer + 16 * 2);         // src[2n + 1]
            load_dqword(context->xmm3, in_buffer + 16 * 2 * 2);     // src[2n + 2]
            movdqa(&context->xmm4, context->xmm1);
            movdqa(&context->xmm5, context->xmm2);
            movdqa(&context->xmm6, context->xmm3);
            // h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
            paddw(context->xmm4, context->xmm6);
            psraw(context->xmm4, 1);
            psubw(context->xmm5, context->xmm4);
            psraw(context->xmm5, 1);
            movdqa(out_hi_buffer, context->xmm5);                  // out hi
            movdqa(&context->xmm6, context->xmm5);                   // save hi
            // l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
            paddw(context->xmm5, context->xmm7);
            psraw(context->xmm5, 1);
            paddw(context->xmm5, context->xmm1);
            movdqa(out_lo_buffer, context->xmm5);                  // out lo
            movdqa(&context->xmm7, context->xmm6);                   // save hi
            // move down
            in_buffer = in_buffer + 16 * 2 * 2;         // 2 rows
            out_hi_buffer = out_hi_buffer + 16 * 2;             // 1 row
            out_lo_buffer = out_lo_buffer + 16 * 2;             // 1 row
    
            cx--;
        }
    
        // post
        movdqa(&context->xmm1, context->xmm3);                   // src[2n]
        load_dqword(context->xmm2, in_buffer + 16 * 2);         // src[2n + 1]
        movdqa(&context->xmm4, context->xmm1);
        movdqa(&context->xmm5, context->xmm2);
        movdqa(&context->xmm6, context->xmm3);
        // h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
        paddw(context->xmm4, context->xmm6);
        psraw(context->xmm4, 1);
        psubw(context->xmm5, context->xmm4);
        psraw(context->xmm5, 1);
        movdqa(out_hi_buffer, context->xmm5);                  // out hi
        // l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
        paddw(context->xmm5, context->xmm7);
        psraw(context->xmm5, 1);
        paddw(context->xmm5, context->xmm1);
        movdqa(out_lo_buffer, context->xmm5);                  // out lo
        // move down
        in_buffer = in_buffer + 16 * 2 * 2;         // 2 row
        out_hi_buffer = out_hi_buffer + 16 * 2;             // 1 row
        out_lo_buffer = out_lo_buffer + 16 * 2;             // 1 row
    
        // move up
        in_buffer = in_buffer - (16 * 16 * 2);
        out_hi_buffer = out_hi_buffer - (8 * 16 * 2);
        out_lo_buffer = out_lo_buffer - (8 * 16 * 2);
    
        // move right
        in_buffer = in_buffer + 16;
        out_hi_buffer = out_hi_buffer + 16;
        out_lo_buffer = out_lo_buffer + 16;
    
        ecx--;
    }
}

void
rfx_dwt_2d_encode_block_horiz_16_32(struct cpu_context *context,
                                    const uint8_t *in_buffer,
                                    uint8_t *out_hi_buffer,
                                    uint8_t *out_lo_buffer)
{
    int32_t ecx = 16;
    // loop1c:
    while (ecx != 0) {
        // pre
        load_dqword(context->xmm1, in_buffer);                  // src[2n]
        load_dqword(context->xmm2, in_buffer + 16);
        movdqa(&context->xmm6, context->xmm1);
        movdqa(&context->xmm7, context->xmm2);
        pand(context->xmm1, cdFFFF);
        pand(context->xmm2, cdFFFF);
        packusdw(context->xmm1, context->xmm2);
        movdqa(&context->xmm2, context->xmm6);                   // src[2n + 1]
        movdqa(&context->xmm3, context->xmm7);
        psrldq(context->xmm2, 2);
        psrldq(context->xmm3, 2);
        pand(context->xmm2, cdFFFF);
        pand(context->xmm3, cdFFFF);
        packusdw(context->xmm2, context->xmm3);
        movdqa(&context->xmm3, context->xmm6);                   // src[2n + 2]
        movdqa(&context->xmm4, context->xmm7);
        psrldq(context->xmm3, 4);
        psrldq(context->xmm4, 4);
        store_dword(context->rax.eax, context->xmm7);
        load_dword(context->xmm5, context->rax.eax);
        pslldq(context->xmm5, 12);
        por(context->xmm3, context->xmm5);
        // context->rax = *((int32_t *) (void *) in_buffer + 32); // FIXME: mov(eax, (void *) in_buffer + 32)
        context->rax.eax = *((int32_t *) (in_buffer + 32));
        load_dword(context->xmm5, context->rax.eax);
        pslldq(context->xmm5, 12);
        por(context->xmm4, context->xmm5);
        pand(context->xmm3, cdFFFF);
        pand(context->xmm4, cdFFFF);
        packusdw(context->xmm3, context->xmm4);
        movdqa(&context->xmm4, context->xmm1);
        movdqa(&context->xmm5, context->xmm2);
        movdqa(&context->xmm6, context->xmm3);
        // h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
        paddw(context->xmm4, context->xmm6);
        psraw(context->xmm4, 1);
        psubw(context->xmm5, context->xmm4);
        psraw(context->xmm5, 1);
    
        movdqa(&context->xmm6, context->xmm5);                   // out hi
        paddw(context->xmm6, context->xmm8);
        psraw_vec(context->xmm6, context->xmm9);
        movdqa(out_hi_buffer, context->xmm6);
        movdqa(&context->xmm2, context->xmm5);                   // save hi
    
        // l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
        movdqa(&context->xmm7, context->xmm5);
        store_dword(context->rax.eax, context->xmm7);
        pslldq(context->xmm7, 2);
        context->rax.eax = context->rax.eax & 0xFFFF;
        load_dword(context->xmm6, context->rax.eax);
        por(context->xmm7, context->xmm6);
        paddw(context->xmm5, context->xmm7);
        psraw(context->xmm5, 1);
        paddw(context->xmm5, context->xmm1);
    
        psrldq(context->xmm2, 14);
        store_dword(context->rbx.eax, context->xmm2);                      // save hi
    
        movdqa(&context->xmm6, context->xmm5);                   // out lo
        paddw(context->xmm6, context->xmm10);
        psraw_vec(context->xmm6, context->xmm11);
        movdqa(out_lo_buffer, context->xmm6);
    
        // move right
        in_buffer = in_buffer + 16 * 2;
        out_hi_buffer = out_hi_buffer + 8 * 2;
        out_lo_buffer = out_lo_buffer + 8 * 2;
    
        // post
        load_dqword(context->xmm1, in_buffer);                  // src[2n]
        load_dqword(context->xmm2, in_buffer + 16);
        movdqa(&context->xmm6, context->xmm1);
        movdqa(&context->xmm7, context->xmm2);
        pand(context->xmm1, cdFFFF);
        pand(context->xmm2, cdFFFF);
        packusdw(context->xmm1, context->xmm2);
        movdqa(&context->xmm2, context->xmm6);                   // src[2n + 1]
        movdqa(&context->xmm3, context->xmm7);
        psrldq(context->xmm2, 2);
        psrldq(context->xmm3, 2);
        pand(context->xmm2, cdFFFF);
        pand(context->xmm3, cdFFFF);
        packusdw(context->xmm2, context->xmm3);
        movdqa(&context->xmm3, context->xmm6);                   // src[2n + 2]
        movdqa(&context->xmm4, context->xmm7);
        psrldq(context->xmm3, 4);
        psrldq(context->xmm4, 4);
        store_dword(context->rax.eax, context->xmm7);
        load_dword(context->xmm5, context->rax.eax);
        pslldq(context->xmm5, 12);
        por(context->xmm3, context->xmm5);
        movdqa(&context->xmm5, context->xmm7);
        psrldq(context->xmm5, 12);
        pslldq(context->xmm5, 12);
        por(context->xmm4, context->xmm5);
        pand(context->xmm3, cdFFFF);
        pand(context->xmm4, cdFFFF);
        packusdw(context->xmm3, context->xmm4);
        movdqa(&context->xmm4, context->xmm1);
        movdqa(&context->xmm5, context->xmm2);
        movdqa(&context->xmm6, context->xmm3);
        // h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
        paddw(context->xmm4, context->xmm6);
        psraw(context->xmm4, 1);
        psubw(context->xmm5, context->xmm4);
        psraw(context->xmm5, 1);
    
        movdqa(&context->xmm6, context->xmm5);                   // out hi
        paddw(context->xmm6, context->xmm8);
        psraw_vec(context->xmm6, context->xmm9);
        movdqa(out_hi_buffer, context->xmm6);
    
        // l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
        movdqa(&context->xmm7, context->xmm5);
        pslldq(context->xmm7, 2);
        load_dword(context->xmm6, context->rbx.eax);
        por(context->xmm7, context->xmm6);
        paddw(context->xmm5, context->xmm7);
        psraw(context->xmm5, 1);
        paddw(context->xmm5, context->xmm1);
    
        movdqa(&context->xmm6, context->xmm5);                   // out lo
        paddw(context->xmm6, context->xmm10);
        psraw_vec(context->xmm6, context->xmm11);
        movdqa(out_lo_buffer, context->xmm6);
    
        // move right
        in_buffer = in_buffer + 16 * 2;
        out_hi_buffer = out_hi_buffer + 8 * 2;
        out_lo_buffer = out_lo_buffer + 8 * 2;
    
        // move left
        in_buffer = in_buffer - (32 * 2);
        out_hi_buffer = out_hi_buffer - (16 * 2);
        out_lo_buffer = out_lo_buffer - (16 * 2);
    
        // move down
        in_buffer = in_buffer + 32 * 2;
        out_hi_buffer = out_hi_buffer + 16 * 2;
        out_lo_buffer = out_lo_buffer + 16 * 2;
    
        ecx--;
    }
}

void
rfx_dwt_2d_encode_block_horiz_16_32_no_lo(struct cpu_context *context,
                                          const uint8_t *in_buffer, // rsi
                                          uint8_t *out_hi_buffer,  // rdi
                                          uint8_t *out_lo_buffer)  // rdx
{
    int32_t ecx = 16;
    
    // loop1c1:
    while (ecx != 0) {
        // pre
        load_dqword(context->xmm1, in_buffer);                  // src[2n]
        load_dqword(context->xmm2, in_buffer + 16);
        movdqa(&context->xmm6, context->xmm1);
        movdqa(&context->xmm7, context->xmm2);
        pand(context->xmm1, cdFFFF);
        pand(context->xmm2, cdFFFF);
        packusdw(context->xmm1, context->xmm2);
        movdqa(&context->xmm2, context->xmm6);                   // src[2n + 1]
        movdqa(&context->xmm3, context->xmm7);
        psrldq(context->xmm2, 2);
        psrldq(context->xmm3, 2);
        pand(context->xmm2, cdFFFF);
        pand(context->xmm3, cdFFFF);
        packusdw(context->xmm2, context->xmm3);
        movdqa(&context->xmm3, context->xmm6);                   // src[2n + 2]
        movdqa(&context->xmm4, context->xmm7);
        psrldq(context->xmm3, 4);
        psrldq(context->xmm4, 4);
        store_dword(context->rax.eax, context->xmm7);
        load_dword(context->xmm5, context->rax.eax);
        pslldq(context->xmm5, 12);
        por(context->xmm3, context->xmm5);
        context->rax.eax = *((int32_t *) (in_buffer + 32));
        load_dword(context->xmm5, context->rax.eax);
        pslldq(context->xmm5, 12);
        por(context->xmm4, context->xmm5);
        pand(context->xmm3, cdFFFF);
        pand(context->xmm4, cdFFFF);
        packusdw(context->xmm3, context->xmm4);
        movdqa(&context->xmm4, context->xmm1);
        movdqa(&context->xmm5, context->xmm2);
        movdqa(&context->xmm6, context->xmm3);
        // h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
        paddw(context->xmm4, context->xmm6);
        psraw(context->xmm4, 1);
        psubw(context->xmm5, context->xmm4);
        psraw(context->xmm5, 1);
    
        movdqa(&context->xmm6, context->xmm5);                   // out hi
        paddw(context->xmm6, context->xmm8);
        psraw_vec(context->xmm6, context->xmm9);
        movdqa(out_hi_buffer, context->xmm6);
        movdqa(&context->xmm2, context->xmm5);                   // save hi
    
        // l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
        movdqa(&context->xmm7, context->xmm5);
        store_dword(context->rax.eax, context->xmm7);
        pslldq(context->xmm7, 2);
        context->rax.eax = context->rax.eax & 0xFFFF;
        load_dword(context->xmm6, context->rax.eax);
        por(context->xmm7, context->xmm6);
        paddw(context->xmm5, context->xmm7);
        psraw(context->xmm5, 1);
        paddw(context->xmm5, context->xmm1);
    
        psrldq(context->xmm2, 14);
        store_dword(context->rbx.eax, context->xmm2);                      // save hi
    
        movdqa(out_lo_buffer, context->xmm5);                  // out lo
    
        // move right
        in_buffer = in_buffer + 16 * 2;
        out_hi_buffer = out_hi_buffer + 8 * 2;
        out_lo_buffer = out_lo_buffer + 8 * 2;
    
        // post
        load_dqword(context->xmm1, in_buffer);                  // src[2n]
        load_dqword(context->xmm2, in_buffer + 16);
        movdqa(&context->xmm6, context->xmm1);
        movdqa(&context->xmm7, context->xmm2);
        pand(context->xmm1, cdFFFF);
        pand(context->xmm2, cdFFFF);
        packusdw(context->xmm1, context->xmm2);
        movdqa(&context->xmm2, context->xmm6);                   // src[2n + 1]
        movdqa(&context->xmm3, context->xmm7);
        psrldq(context->xmm2, 2);
        psrldq(context->xmm3, 2);
        pand(context->xmm2, cdFFFF);
        pand(context->xmm3, cdFFFF);
        packusdw(context->xmm2, context->xmm3);
        movdqa(&context->xmm3, context->xmm6);                   // src[2n + 2]
        movdqa(&context->xmm4, context->xmm7);
        psrldq(context->xmm3, 4);
        psrldq(context->xmm4, 4);
        store_dword(context->rax.eax, context->xmm7);
        load_dword(context->xmm5, context->rax.eax);
        pslldq(context->xmm5, 12);
        por(context->xmm3, context->xmm5);
        movdqa(&context->xmm5, context->xmm7);
        psrldq(context->xmm5, 12);
        pslldq(context->xmm5, 12);
        por(context->xmm4, context->xmm5);
        pand(context->xmm3, cdFFFF);
        pand(context->xmm4, cdFFFF);
        packusdw(context->xmm3, context->xmm4);
        movdqa(&context->xmm4, context->xmm1);
        movdqa(&context->xmm5, context->xmm2);
        movdqa(&context->xmm6, context->xmm3);
        // h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
        paddw(context->xmm4, context->xmm6);
        psraw(context->xmm4, 1);
        psubw(context->xmm5, context->xmm4);
        psraw(context->xmm5, 1);
    
        movdqa(&context->xmm6, context->xmm5);                   // out hi
        paddw(context->xmm6, context->xmm8);
        psraw_vec(context->xmm6, context->xmm9);
        movdqa(out_hi_buffer, context->xmm6);
    
        // l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
        movdqa(&context->xmm7, context->xmm5);
        pslldq(context->xmm7, 2);
        load_dword(context->xmm6, context->rbx.eax);
        por(context->xmm7, context->xmm6);
        paddw(context->xmm5, context->xmm7);
        psraw(context->xmm5, 1);
        paddw(context->xmm5, context->xmm1);
    
        movdqa(out_lo_buffer, context->xmm5);                  // out lo
    
        // move right
        in_buffer = in_buffer + 16 * 2;
        out_hi_buffer = out_hi_buffer + 8 * 2;
        out_lo_buffer = out_lo_buffer + 8 * 2;
    
        // move left
        in_buffer = in_buffer - (32 * 2);
        out_hi_buffer = out_hi_buffer - (16 * 2);
        out_lo_buffer = out_lo_buffer - (16 * 2);
    
        // move down
        in_buffer = in_buffer + 32 * 2;
        out_hi_buffer = out_hi_buffer + 16 * 2;
        out_lo_buffer = out_lo_buffer + 16 * 2;
    
        ecx--;
    }
}

void
rfx_dwt_2d_encode_block_verti_16_32(struct cpu_context *context,
                                    const uint8_t *in_buffer,  // rsi
                                    uint8_t *out_hi_buffer,   // rdi
                                    uint8_t *out_lo_buffer)   // rdx
{
    int16_t cx = 0;
    int32_t ecx = 4;
    
    // loop1d:
    while (ecx != 0) {
        // pre
        load_dqword(context->xmm1, in_buffer);                  // src[2n]
        load_dqword(context->xmm2, in_buffer + 32 * 2);         // src[2n + 1]
        load_dqword(context->xmm3, in_buffer + 32 * 2 * 2);     // src[2n + 2]
        movdqa(&context->xmm4, context->xmm1);
        movdqa(&context->xmm5, context->xmm2);
        movdqa(&context->xmm6, context->xmm3);
        // h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
        paddw(context->xmm4, context->xmm6);
        psraw(context->xmm4, 1);
        psubw(context->xmm5, context->xmm4);
        psraw(context->xmm5, 1);
        movdqa(out_hi_buffer, context->xmm5);                  // out hi
        movdqa(&context->xmm6, context->xmm5);                   // save hi
        // l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
        paddw(context->xmm5, context->xmm1);
        movdqa(out_lo_buffer, context->xmm5);                  // out lo
        movdqa(&context->xmm7, context->xmm6);                   // save hi
        // move down
        in_buffer = in_buffer + 32 * 2 * 2;         // 2 rows
        out_hi_buffer = out_hi_buffer + 32 * 2;             // 1 row
        out_lo_buffer = out_lo_buffer + 32 * 2;             // 1 row
    
        // loop
        cx = 14;
        
        // loop2d:
        while (cx != 0) {
            movdqa(&context->xmm1, context->xmm3);                   // src[2n]
            load_dqword(context->xmm2, in_buffer + 32 * 2);         // src[2n + 1]
            load_dqword(context->xmm3, in_buffer + 32 * 2 * 2);     // src[2n + 2]
            movdqa(&context->xmm4, context->xmm1);
            movdqa(&context->xmm5, context->xmm2);
            movdqa(&context->xmm6, context->xmm3);
            // h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
            paddw(context->xmm4, context->xmm6);
            psraw(context->xmm4, 1);
            psubw(context->xmm5, context->xmm4);
            psraw(context->xmm5, 1);
            movdqa(out_hi_buffer, context->xmm5);                  // out hi
            movdqa(&context->xmm6, context->xmm5);                   // save hi
            // l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
            paddw(context->xmm5, context->xmm7);
            psraw(context->xmm5, 1);
            paddw(context->xmm5, context->xmm1);
            movdqa(out_lo_buffer, context->xmm5);                  // out lo
            movdqa(&context->xmm7, context->xmm6);                   // save hi
            // move down
            in_buffer = in_buffer + 32 * 2 * 2;         // 2 rows
            out_hi_buffer = out_hi_buffer + 32 * 2;             // 1 row
            out_lo_buffer = out_lo_buffer + 32 * 2;             // 1 row
    
            cx--;
        }
    
        // post
        movdqa(&context->xmm1, context->xmm3);                   // src[2n]
        load_dqword(context->xmm2, in_buffer + 32 * 2);         // src[2n + 1]
        movdqa(&context->xmm4, context->xmm1);
        movdqa(&context->xmm5, context->xmm2);
        movdqa(&context->xmm6, context->xmm3);
        // h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
        paddw(context->xmm4, context->xmm6);
        psraw(context->xmm4, 1);
        psubw(context->xmm5, context->xmm4);
        psraw(context->xmm5, 1);
        movdqa(out_hi_buffer, context->xmm5);                  // out hi
        // l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
        paddw(context->xmm5, context->xmm7);
        psraw(context->xmm5, 1);
        paddw(context->xmm5, context->xmm1);
        movdqa(out_lo_buffer, context->xmm5);                  // out lo
        
        // move down
        in_buffer = in_buffer + 32 * 2 * 2;         // 2 row
        out_hi_buffer = out_hi_buffer + 32 * 2;             // 1 row
        out_lo_buffer = out_lo_buffer + 32 * 2;             // 1 row
    
        // move up
        in_buffer = in_buffer - (32 * 32 * 2);
        out_hi_buffer = out_hi_buffer - (16 * 32 * 2);
        out_lo_buffer = out_lo_buffer - (16 * 32 * 2);
    
        // move right
        in_buffer = in_buffer + 16;
        out_hi_buffer = out_hi_buffer + 16;
        out_lo_buffer = out_lo_buffer + 16;
    
        ecx--;
    }
}

void
rfx_dwt_2d_encode_block_horiz_16_64(struct cpu_context *context,
                                    const uint8_t *in_buffer, // rsi
                                    uint8_t *out_hi_buffer,  // rdi
                                    uint8_t *out_lo_buffer)  // rdx
{
    int16_t cx = 0;
    int32_t ecx = 32;
    
    // loop1e:
    while (ecx != 0) {
        // pre
        load_dqword(context->xmm1, in_buffer);                  // src[2n]
        load_dqword(context->xmm2, in_buffer + 16);
        movdqa(&context->xmm6, context->xmm1);
        movdqa(&context->xmm7, context->xmm2);
        pand(context->xmm1, cdFFFF);
        pand(context->xmm2, cdFFFF);
        packusdw(context->xmm1, context->xmm2);
        movdqa(&context->xmm2, context->xmm6);                   // src[2n + 1]
        movdqa(&context->xmm3, context->xmm7);
        psrldq(context->xmm2, 2);
        psrldq(context->xmm3, 2);
        pand(context->xmm2, cdFFFF);
        pand(context->xmm3, cdFFFF);
        packusdw(context->xmm2, context->xmm3);
        movdqa(&context->xmm3, context->xmm6);                   // src[2n + 2]
        movdqa(&context->xmm4, context->xmm7);
        psrldq(context->xmm3, 4);
        psrldq(context->xmm4, 4);
        store_dword(context->rax.eax, context->xmm7);
        load_dword(context->xmm5, context->rax.eax);
        pslldq(context->xmm5, 12);
        por(context->xmm3, context->xmm5);
        context->rax.eax = *((int32_t *) (in_buffer + 32));
        load_dword(context->xmm5, context->rax.eax);
        pslldq(context->xmm5, 12);
        por(context->xmm4, context->xmm5);
        pand(context->xmm3, cdFFFF);
        pand(context->xmm4, cdFFFF);
        packusdw(context->xmm3, context->xmm4);
        movdqa(&context->xmm4, context->xmm1);
        movdqa(&context->xmm5, context->xmm2);
        movdqa(&context->xmm6, context->xmm3);
        // h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
        paddw(context->xmm4, context->xmm6);
        psraw(context->xmm4, 1);
        psubw(context->xmm5, context->xmm4);
        psraw(context->xmm5, 1);
    
        movdqa(&context->xmm6, context->xmm5);                   // out hi
        paddw(context->xmm6, context->xmm8);
        psraw_vec(context->xmm6, context->xmm9);
        movdqa(out_hi_buffer, context->xmm6);
        movdqa(&context->xmm2, context->xmm5);                   // save hi
    
        // l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
        movdqa(&context->xmm7, context->xmm5);
        store_dword(context->rax.eax, context->xmm7);
        pslldq(context->xmm7, 2);
        context->rax.eax = context->rax.eax & 0xFFFF;
        load_dword(context->xmm6, context->rax.eax);
        por(context->xmm7, context->xmm6);
        paddw(context->xmm5, context->xmm7);
        psraw(context->xmm5, 1);
        paddw(context->xmm5, context->xmm1);
    
        psrldq(context->xmm2, 14);
        store_dword(context->rbx.eax, context->xmm2);                      // save hi
    
        movdqa(&context->xmm6, context->xmm5);                   // out lo
        paddw(context->xmm6, context->xmm10);
        psraw_vec(context->xmm6, context->xmm11);
        movdqa(out_lo_buffer, context->xmm6);
    
        // move right
        in_buffer = in_buffer + 16 * 2;
        out_hi_buffer = out_hi_buffer + 8 * 2;
        out_lo_buffer = out_lo_buffer + 8 * 2;
    
        // loop
        cx = 2;
        
        // loop2e:
        while (cx != 0) {
            load_dqword(context->xmm1, in_buffer);                  // src[2n]
            load_dqword(context->xmm2, in_buffer + 16);
            movdqa(&context->xmm6, context->xmm1);
            movdqa(&context->xmm7, context->xmm2);
            pand(context->xmm1, cdFFFF);
            pand(context->xmm2, cdFFFF);
            packusdw(context->xmm1, context->xmm2);
            movdqa(&context->xmm2, context->xmm6);                   // src[2n + 1]
            movdqa(&context->xmm3, context->xmm7);
            psrldq(context->xmm2, 2);
            psrldq(context->xmm3, 2);
            pand(context->xmm2, cdFFFF);
            pand(context->xmm3, cdFFFF);
            packusdw(context->xmm2, context->xmm3);
            movdqa(&context->xmm3, context->xmm6);                   // src[2n + 2]
            movdqa(&context->xmm4, context->xmm7);
            psrldq(context->xmm3, 4);
            psrldq(context->xmm4, 4);
            store_dword(context->rax.eax, context->xmm7);
            load_dword(context->xmm5, context->rax.eax);
            pslldq(context->xmm5, 12);
            por(context->xmm3, context->xmm5);
            context->rax.eax = *((int32_t *) (in_buffer + 32));
            load_dword(context->xmm5, context->rax.eax);
            pslldq(context->xmm5, 12);
            por(context->xmm4, context->xmm5);
            pand(context->xmm3, cdFFFF);
            pand(context->xmm4, cdFFFF);
            packusdw(context->xmm3, context->xmm4);
            movdqa(&context->xmm4, context->xmm1);
            movdqa(&context->xmm5, context->xmm2);
            movdqa(&context->xmm6, context->xmm3);
            // h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
            paddw(context->xmm4, context->xmm6);
            psraw(context->xmm4, 1);
            psubw(context->xmm5, context->xmm4);
            psraw(context->xmm5, 1);
    
            movdqa(&context->xmm6, context->xmm5);                   // out hi
            paddw(context->xmm6, context->xmm8);
            psraw_vec(context->xmm6, context->xmm9);
            movdqa(out_hi_buffer, context->xmm6);
            movdqa(&context->xmm2, context->xmm5);                   // save hi
    
            // l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
            movdqa(&context->xmm7, context->xmm5);
            pslldq(context->xmm7, 2);
            load_dword(context->xmm6, context->rbx.eax);
            por(context->xmm7, context->xmm6);
            paddw(context->xmm5, context->xmm7);
            psraw(context->xmm5, 1);
            paddw(context->xmm5, context->xmm1);
    
            psrldq(context->xmm2, 14);
            store_dword(context->rbx.eax, context->xmm2);                      // save hi
    
            movdqa(&context->xmm6, context->xmm5);                   // out lo
            paddw(context->xmm6, context->xmm10);
            psraw_vec(context->xmm6, context->xmm11);
            movdqa(out_lo_buffer, context->xmm6);
    
            // move right
            in_buffer = in_buffer + 16 * 2;
            out_hi_buffer = out_hi_buffer + 8 * 2;
            out_lo_buffer = out_lo_buffer + 8 * 2;
    
            cx--;
        }
    
        // post
        load_dqword(context->xmm1, in_buffer);                  // src[2n]
        load_dqword(context->xmm2, in_buffer + 16);
        movdqa(&context->xmm6, context->xmm1);
        movdqa(&context->xmm7, context->xmm2);
        pand(context->xmm1, cdFFFF);
        pand(context->xmm2, cdFFFF);
        packusdw(context->xmm1, context->xmm2);
        movdqa(&context->xmm2, context->xmm6);                   // src[2n + 1]
        movdqa(&context->xmm3, context->xmm7);
        psrldq(context->xmm2, 2);
        psrldq(context->xmm3, 2);
        pand(context->xmm2, cdFFFF);
        pand(context->xmm3, cdFFFF);
        packusdw(context->xmm2, context->xmm3);
        movdqa(&context->xmm3, context->xmm6);                   // src[2n + 2]
        movdqa(&context->xmm4, context->xmm7);
        psrldq(context->xmm3, 4);
        psrldq(context->xmm4, 4);
        store_dword(context->rax.eax, context->xmm7);
        load_dword(context->xmm5, context->rax.eax);
        pslldq(context->xmm5, 12);
        por(context->xmm3, context->xmm5);
        movdqa(&context->xmm5, context->xmm7);
        psrldq(context->xmm5, 12);
        pslldq(context->xmm5, 12);
        por(context->xmm4, context->xmm5);
        pand(context->xmm3, cdFFFF);
        pand(context->xmm4, cdFFFF);
        packusdw(context->xmm3, context->xmm4);
        movdqa(&context->xmm4, context->xmm1);
        movdqa(&context->xmm5, context->xmm2);
        movdqa(&context->xmm6, context->xmm3);
        // h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
        paddw(context->xmm4, context->xmm6);
        psraw(context->xmm4, 1);
        psubw(context->xmm5, context->xmm4);
        psraw(context->xmm5, 1);
    
        movdqa(&context->xmm6, context->xmm5);                   // out hi
        paddw(context->xmm6, context->xmm8);
        psraw_vec(context->xmm6, context->xmm9);
        movdqa(out_hi_buffer, context->xmm6);
    
        // l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
        movdqa(&context->xmm7, context->xmm5);
        pslldq(context->xmm7, 2);
        load_dword(context->xmm6, context->rbx.eax);
        por(context->xmm7, context->xmm6);
        paddw(context->xmm5, context->xmm7);
        psraw(context->xmm5, 1);
        paddw(context->xmm5, context->xmm1);
    
        movdqa(&context->xmm6, context->xmm5);                   // out lo
        paddw(context->xmm6, context->xmm10);
        psraw_vec(context->xmm6, context->xmm11);
        movdqa(out_lo_buffer, context->xmm6);
    
        // move right
        in_buffer = in_buffer + 16 * 2;
        out_hi_buffer = out_hi_buffer + 8 * 2;
        out_lo_buffer = out_lo_buffer + 8 * 2;
    
        // move left
        in_buffer = in_buffer - (64 * 2);
        out_hi_buffer = out_hi_buffer - (32 * 2);
        out_lo_buffer = out_lo_buffer - (32 * 2);
    
        // move down
        in_buffer = in_buffer + 64 * 2;
        out_hi_buffer = out_hi_buffer + 32 * 2;
        out_lo_buffer = out_lo_buffer + 32 * 2;
    
        ecx--;
    }
}

void
rfx_dwt_2d_encode_block_horiz_16_64_no_lo(struct cpu_context *context,
                                          const uint8_t *in_buffer, // rsi
                                          uint8_t *out_hi_buffer,  // rdi
                                          uint8_t *out_lo_buffer)  // rdx
{
    int32_t ecx = 32;
    int16_t cx = 0;
    
    
    // loop1e1
    while (ecx != 0) {
        // pre
        load_dqword(context->xmm1, in_buffer);                  // src[2n]
        load_dqword(context->xmm2, in_buffer + 16);
        movdqa(&context->xmm6, context->xmm1);
        movdqa(&context->xmm7, context->xmm2);
        pand(context->xmm1,cdFFFF);
        pand(context->xmm2,cdFFFF);
        packusdw(context->xmm1, context->xmm2);
        movdqa(&context->xmm2, context->xmm6);                   // src[2n + 1]
        movdqa(&context->xmm3, context->xmm7);
        psrldq(context->xmm2, 2);
        psrldq(context->xmm3, 2);
        pand(context->xmm2,cdFFFF);
        pand(context->xmm3,cdFFFF);
        packusdw(context->xmm2, context->xmm3);
        movdqa(&context->xmm3, context->xmm6);                   // src[2n + 2]
        movdqa(&context->xmm4, context->xmm7);
        psrldq(context->xmm3, 4);
        psrldq(context->xmm4, 4);
        store_dword(context->rax.eax, context->xmm7);
        load_dword(context->xmm5, context->rax.eax);
        pslldq(context->xmm5, 12);
        por(context->xmm3, context->xmm5);
        context->rax.eax = *((int32_t *) (in_buffer + 32));
        load_dword(context->xmm5, context->rax.eax);
        pslldq(context->xmm5, 12);
        por(context->xmm4, context->xmm5);
        pand(context->xmm3,cdFFFF);
        pand(context->xmm4,cdFFFF);
        packusdw(context->xmm3, context->xmm4);
        movdqa(&context->xmm4, context->xmm1);
        movdqa(&context->xmm5, context->xmm2);
        movdqa(&context->xmm6, context->xmm3);
        // h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
        paddw(context->xmm4, context->xmm6);
        psraw(context->xmm4, 1);
        psubw(context->xmm5, context->xmm4);
        psraw(context->xmm5, 1);
    
        movdqa(&context->xmm6, context->xmm5);                   // out hi
        paddw(context->xmm6, context->xmm8);
        psraw_vec(context->xmm6, context->xmm9);
        movdqa(out_hi_buffer, context->xmm6);
        movdqa(&context->xmm2, context->xmm5);                   // save hi
    
        // l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
        movdqa(&context->xmm7, context->xmm5);
        store_dword(context->rax.eax, context->xmm7);
        pslldq(context->xmm7, 2);
        context->rax.eax = context->rax.eax & 0xFFFF;
        load_dword(context->xmm6, context->rax.eax);
        por(context->xmm7, context->xmm6);
        paddw(context->xmm5, context->xmm7);
        psraw(context->xmm5, 1);
        paddw(context->xmm5, context->xmm1);
    
        psrldq(context->xmm2, 14);
        store_dword(context->rbx.eax, context->xmm2);           // save hi
    
        movdqa(out_lo_buffer, context->xmm5);      // out lo
    
        // move right
        in_buffer = in_buffer + 16 * 2;
        out_hi_buffer = out_hi_buffer + 8 * 2;
        out_lo_buffer = out_lo_buffer + 8 * 2;
    
        // loop
        cx = 2;
        
        // loop2e1
        while (cx != 0) {
            load_dqword(context->xmm1, in_buffer);                  // src[2n]
            load_dqword(context->xmm2, in_buffer + 16);
            movdqa(&context->xmm6, context->xmm1);
            movdqa(&context->xmm7, context->xmm2);
            pand(context->xmm1,cdFFFF);
            pand(context->xmm2,cdFFFF);
            packusdw(context->xmm1, context->xmm2);
            movdqa(&context->xmm2, context->xmm6);                   // src[2n + 1]
            movdqa(&context->xmm3, context->xmm7);
            psrldq(context->xmm2, 2);
            psrldq(context->xmm3, 2);
            pand(context->xmm2,cdFFFF);
            pand(context->xmm3,cdFFFF);
            packusdw(context->xmm2, context->xmm3);
            movdqa(&context->xmm3, context->xmm6);                   // src[2n + 2]
            movdqa(&context->xmm4, context->xmm7);
            psrldq(context->xmm3, 4);
            psrldq(context->xmm4, 4);
            store_dword(context->rax.eax, context->xmm7);
            load_dword(context->xmm5, context->rax.eax);
            pslldq(context->xmm5, 12);
            por(context->xmm3, context->xmm5);
            context->rax.eax = *((int32_t *) (in_buffer + 32));
            load_dword(context->xmm5, context->rax.eax);
            pslldq(context->xmm5, 12);
            por(context->xmm4, context->xmm5);
            pand(context->xmm3,cdFFFF);
            pand(context->xmm4,cdFFFF);
            packusdw(context->xmm3, context->xmm4);
            movdqa(&context->xmm4, context->xmm1);
            movdqa(&context->xmm5, context->xmm2);
            movdqa(&context->xmm6, context->xmm3);
            // h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
            paddw(context->xmm4, context->xmm6);
            psraw(context->xmm4, 1);
            psubw(context->xmm5, context->xmm4);
            psraw(context->xmm5, 1);
    
            movdqa(&context->xmm6, context->xmm5);                   // out hi
            paddw(context->xmm6, context->xmm8);
            psraw_vec(context->xmm6, context->xmm9);
            movdqa(out_hi_buffer, context->xmm6);
            movdqa(&context->xmm2, context->xmm5);                   // save hi
    
            // l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
            movdqa(&context->xmm7, context->xmm5);
            pslldq(context->xmm7, 2);
            load_dword(context->xmm6, context->rbx.eax);
            por(context->xmm7, context->xmm6);
            paddw(context->xmm5, context->xmm7);
            psraw(context->xmm5, 1);
            paddw(context->xmm5, context->xmm1);
    
            psrldq(context->xmm2, 14);
            store_dword(context->rbx.eax, context->xmm2);   // save hi
                                                        //
            movdqa(out_lo_buffer, context->xmm5);                  // out lo
    
            // move right
            in_buffer = in_buffer + 16 * 2;
            out_hi_buffer = out_hi_buffer + 8 * 2;
            out_lo_buffer = out_lo_buffer + 8 * 2;
    
            cx--;
        }
    
        // post
        load_dqword(context->xmm1, in_buffer);                  // src[2n]
        load_dqword(context->xmm2, in_buffer + 16);
        movdqa(&context->xmm6, context->xmm1);
        movdqa(&context->xmm7, context->xmm2);
        pand(context->xmm1,cdFFFF);
        pand(context->xmm2,cdFFFF);
        packusdw(context->xmm1, context->xmm2);
        movdqa(&context->xmm2, context->xmm6);                   // src[2n + 1]
        movdqa(&context->xmm3, context->xmm7);
        psrldq(context->xmm2, 2);
        psrldq(context->xmm3, 2);
        pand(context->xmm2,cdFFFF);
        pand(context->xmm3,cdFFFF);
        packusdw(context->xmm2, context->xmm3);
        movdqa(&context->xmm3, context->xmm6);                   // src[2n + 2]
        movdqa(&context->xmm4, context->xmm7);
        psrldq(context->xmm3, 4);
        psrldq(context->xmm4, 4);
        store_dword(context->rax.eax, context->xmm7);
        load_dword(context->xmm5, context->rax.eax);
        pslldq(context->xmm5, 12);
        por(context->xmm3, context->xmm5);
        movdqa(&context->xmm5, context->xmm7);
        psrldq(context->xmm5, 12);
        pslldq(context->xmm5, 12);
        por(context->xmm4, context->xmm5);
        pand(context->xmm3,cdFFFF);
        pand(context->xmm4,cdFFFF);
        packusdw(context->xmm3, context->xmm4);
        movdqa(&context->xmm4, context->xmm1);
        movdqa(&context->xmm5, context->xmm2);
        movdqa(&context->xmm6, context->xmm3);
        // h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
        paddw(context->xmm4, context->xmm6);
        psraw(context->xmm4, 1);
        psubw(context->xmm5, context->xmm4);
        psraw(context->xmm5, 1);
    
        movdqa(&context->xmm6, context->xmm5);                   // out hi
        paddw(context->xmm6, context->xmm8);
        psraw_vec(context->xmm6, context->xmm9);
        movdqa(out_hi_buffer, context->xmm6);
    
        // l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
        movdqa(&context->xmm7, context->xmm5);
        pslldq(context->xmm7, 2);
        load_dword(context->xmm6, context->rbx.eax);
        por(context->xmm7, context->xmm6);
        paddw(context->xmm5, context->xmm7);
        psraw(context->xmm5, 1);
        paddw(context->xmm5, context->xmm1);
    
        movdqa(out_lo_buffer, context->xmm5);                  // out lo
    
        // move right
        in_buffer = in_buffer + 16 * 2;
        out_hi_buffer = out_hi_buffer + 8 * 2;
        out_lo_buffer = out_lo_buffer + 8 * 2;
    
        // move left
        in_buffer = in_buffer - (64 * 2);
        out_hi_buffer = out_hi_buffer - (32 * 2);
        out_lo_buffer = out_lo_buffer - (32 * 2);
    
        // move down
        in_buffer = in_buffer + 64 * 2;
        out_hi_buffer = out_hi_buffer + 32 * 2;
        out_lo_buffer = out_lo_buffer + 32 * 2;
    
        ecx--;
    }
}

/**
 * source 8 bit unsigned, 64 pixel width
 */
void
rfx_dwt_2d_encode_block_verti_8_64(struct cpu_context *context,
                                   const uint8_t *in_buffer, // rsi
                                   uint8_t *out_hi_buffer,  // rdi
                                   uint8_t *out_lo_buffer)  // rdx
{
    int16_t cx = 0;
    int32_t ecx = 8;
    
    // loop1f
    while (ecx != 0) {
        // pre
        movq(context->xmm1, *((uint64_t *) in_buffer));                    // src[2n]
        movq(context->xmm2, *((uint64_t *) (in_buffer + 64 * 1)));           // src[2n + 1]
        movq(context->xmm3, *((uint64_t *) (in_buffer + 64 * 1 * 2)));       // src[2n + 2]
        punpcklbw(context->xmm1, context->xmm0);
        punpcklbw(context->xmm2, context->xmm0);
        punpcklbw(context->xmm3, context->xmm0);
        psubw(context->xmm1, cw128);
        psubw(context->xmm2, cw128);
        psubw(context->xmm3, cw128);
        psllw(context->xmm1, 5);
        psllw(context->xmm2, 5);
        psllw(context->xmm3, 5);
        movdqa(&context->xmm4, context->xmm1);
        movdqa(&context->xmm5, context->xmm2);
        movdqa(&context->xmm6, context->xmm3);

        // h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
        paddw(context->xmm4, context->xmm6);
        psraw(context->xmm4, 1);
        psubw(context->xmm5, context->xmm4);
        psraw(context->xmm5, 1);
        movdqa(out_hi_buffer, context->xmm5);        // out hi
        movdqa(&context->xmm6, context->xmm5);       // save hi
        
        // l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
        paddw(context->xmm5, context->xmm1);
        movdqa(out_lo_buffer, context->xmm5);        // out lo
        movdqa(&context->xmm7, context->xmm6);       // save hi
        
        // move down
        in_buffer = in_buffer + 64 * 1 * 2;         // 2 rows
        out_hi_buffer = out_hi_buffer + 64 * 2;     // 1 row
        out_lo_buffer = out_lo_buffer + 64 * 2;     // 1 row
        
        // loop
        cx = 30;
        
        // loop2f
        while (cx != 0) {
            movdqa(&context->xmm1, context->xmm3);                  // src[2n]
            movq(context->xmm2, *((uint64_t *) (in_buffer + 64 * 1)));       // src[2n + 1]
            movq(context->xmm3, *((uint64_t *) (in_buffer + 64 * 1 * 2)));   // src[2n + 2]
            punpcklbw(context->xmm2, context->xmm0);
            punpcklbw(context->xmm3, context->xmm0);
            psubw(context->xmm2, cw128);
            psubw(context->xmm3, cw128);
            psllw(context->xmm2, 5);
            psllw(context->xmm3, 5);
            movdqa(&context->xmm4, context->xmm1);
            movdqa(&context->xmm5, context->xmm2);
            movdqa(&context->xmm6, context->xmm3);
            
            // h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
            paddw(context->xmm4, context->xmm6);
            psraw(context->xmm4, 1);
            psubw(context->xmm5, context->xmm4);
            psraw(context->xmm5, 1);
            movdqa(out_hi_buffer, context->xmm5);           // out hi
            movdqa(&context->xmm6, context->xmm5);          // save hi
            
            // l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
            paddw(context->xmm5, context->xmm7);
            psraw(context->xmm5, 1);
            paddw(context->xmm5, context->xmm1);
            movdqa(out_lo_buffer, context->xmm5);           // out lo
            movdqa(&context->xmm7, context->xmm6);          // save hi
            
            // move down
            in_buffer = in_buffer + 64 * 1 * 2;         // 2 rows
            out_hi_buffer = out_hi_buffer + 64 * 2;     // 1 row
            out_lo_buffer = out_lo_buffer + 64 * 2;     // 1 row
            
            cx--;
        }
    
        // post
        movdqa(&context->xmm1, context->xmm3);                      // src[2n]
        movq(context->xmm2, *((uint64_t *) (in_buffer + 64 * 1)));  // src[2n + 1]
        punpcklbw(context->xmm2, context->xmm0);
        psubw(context->xmm2, cw128);
        psllw(context->xmm2, 5);
        movdqa(&context->xmm4, context->xmm1);
        movdqa(&context->xmm5, context->xmm2);
        movdqa(&context->xmm6, context->xmm3);
        
        // h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
        paddw(context->xmm4, context->xmm6);
        psraw(context->xmm4, 1);
        psubw(context->xmm5, context->xmm4);
        psraw(context->xmm5, 1);
        movdqa(out_hi_buffer, context->xmm5);                  // out hi
        
        // l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
        paddw(context->xmm5, context->xmm7);
        psraw(context->xmm5, 1);
        paddw(context->xmm5, context->xmm1);
        movdqa(out_lo_buffer, context->xmm5);                  // out lo
         
        // move down
        in_buffer = in_buffer + 64 * 1 * 2;         // 2 rows
        out_hi_buffer = out_hi_buffer + 64 * 2;     // 1 row
        out_lo_buffer = out_lo_buffer + 64 * 2;     // 1 row
        
        // move up
        in_buffer = in_buffer - 64 * 1 * 64;
        out_hi_buffer = out_hi_buffer - 32 * 64 * 2;
        out_lo_buffer = out_lo_buffer - 32 * 64 * 2;
    
        // move right
        in_buffer = in_buffer + 8;
        out_hi_buffer = out_hi_buffer + 16;
        out_lo_buffer = out_lo_buffer + 16;
        
        ecx--;
    }
}

void set_quants_hi(struct cpu_context *context) {
    context->rax.rax = context->rax.rax - (6 - 5);
    load_word_dup(context->xmm9, context->rax.ax);

    context->rax.rax = context->rax.rax * 16;
    context->rdx = (void *) &RODATA;
    context->rdx = context->rdx + context->rax.eax;
    
    load_dqword(context->xmm8, (void *) context->rdx);
}

void set_quants_lo(struct cpu_context *context) {
    context->rax.rax = context->rax.rax - (6 - 5);
    load_word_dup(context->xmm11, context->rax.ax);
     
    context->rax.rax = context->rax.rax * 16;
    context->rdx = (void *) &RODATA;
    context->rdx = context->rdx + context->rax.rax;
    
    load_dqword(context->xmm10, (void *) context->rdx);
}

int
rfxcodec_encode_dwt_shift_arm64_neon(const char *qtable,
                                     const uint8_t *in_buffer,
                                     uint8_t *_work_buffer,
                                     uint8_t *_out_buffer)
{
    struct cpu_context context = { 0 };

    
    // vertical DWT to work buffer, level 1
    rfx_dwt_2d_encode_block_verti_8_64(
        &context,
        in_buffer,                  // src
        work_buffer + 64 * 32 * 2,  // dst hi
        work_buffer                 // dst lo
    );
    printf("rfx_dwt_2d_encode_block_verti_8_64 end\n");
    
    // horizontal DWT to out buffer, level 1, part 1
    
    // xor(rax, rax)
    // mov(rdx, [rsp])
    // mov(al, [rdx + 4])
    // and(al, 0xF)
    context.rax.rax = 0;
    context.rax.al = (uint8_t) qtable[4] & 0xF;
    set_quants_hi(&context);
    
    rfx_dwt_2d_encode_block_horiz_16_64_no_lo(
        &context,
        work_buffer,                // src
        out_buffer,                 // dst hi - HL1
        out_buffer + 32 * 32 * 6    // dst lo - LL1
    );
    printf("rfx_dwt_2d_encode_block_horiz_16_64_no_lo end\n");
    
    // horizontal DWT to out buffer, level 1, part 2
    context.rax.rax = 0;
    context.rax.al = ((uint8_t) qtable[4]) >> 4;
    set_quants_hi(&context);

    context.rax.rax = 0;
    context.rax.al = ((uint8_t) qtable[3]) >> 4;
    set_quants_lo(&context);
    
    
    rfx_dwt_2d_encode_block_horiz_16_64(
        &context,
        work_buffer + 64 * 32 * 2,
        out_buffer + 32 * 32 * 4,
        out_buffer + 32 * 32 * 2
    );
    
    // vertical DWT to work buffer, level 2
    rfx_dwt_2d_encode_block_verti_16_32(
        &context,
        out_buffer + 32 * 32 * 6,   // src
        work_buffer + 32 * 16 * 2,  // dst hi
        work_buffer                 // dst lo
    );
    
    // horizontal DWT to out buffer, level 2, part 1
    context.rax.rax = 0;
    context.rax.al = ((uint8_t) qtable[2]) >> 4;
    set_quants_hi(&context);
    
    rfx_dwt_2d_encode_block_horiz_16_32_no_lo(
        &context,
        work_buffer,        // src
        // 32 * 32 * 6 + 16 * 16 * 0 = 6144
        out_buffer + 6144,  // dst hi - HL2
        // 32 * 32 * 6 + 16 * 16 * 6 = 7680
        out_buffer + 7680   // dst lo - LL2
    );
    
    // horizontal DWT to out buffer, level 2, part 2
    context.rax.rax = 0;
    context.rax.al = ((uint8_t) qtable[3]) & 0xF;
    set_quants_hi(&context);
    
    context.rax.rax = 0;
    context.rax.al = ((uint8_t) qtable[2]) & 0xF;
    set_quants_lo(&context);
    
    rfx_dwt_2d_encode_block_horiz_16_32(
        &context,
        work_buffer + 32 * 16 * 2,      // src
        // 32 * 32 * 6 + 16 * 16 * 4 = 7168
        out_buffer + 7168,              // dst hi = HH2
        // 32 * 32 * 6 + 16 * 16 * 2 = 6656
        out_buffer + 6656               // dst lo - LH2
    );
    
    // vertical DWT to work buffer, level 3
    // 32 * 32 * 6 + 16 * 16 * 6 = 7680
    rfx_dwt_2d_encode_block_verti_16_16(
        &context,
        out_buffer + 7680,          // src
        work_buffer + 16 * 8 * 2,   // dst hi,
        work_buffer                 // dst lo
    );
    
    // horizontal DWT to out buffer, level 3, part 1
    context.rax.rax = 0;
    context.rax.al = ((uint8_t) qtable[1]) & 0xF;
    set_quants_hi(&context);
    
    context.rax.rax = 0;
    context.rax.al = ((uint8_t) qtable[0]) & 0xF;
    set_quants_lo(&context);
    
    rfx_dwt_2d_encode_block_horiz_16_16(
        &context,
        work_buffer,                // src
        // 32 * 32 * 6 + 16 * 16 * 6 + 8 * 8 * 0 = 7680
        out_buffer + 7680,          // dst hi - HL3
        // 32 * 32 * 6 + 16 * 16 * 6 + 8 * 8 * 6 = 8064
        out_buffer + 8064           // dst lo - LL3
    );
    
    // horizontal DWT to out buffer, level 3, part 2
    context.rax.rax = 0;
    context.rax.al = ((uint8_t) qtable[1]) >> 4;
    set_quants_hi(&context);
    
    context.rax.rax = 0;
    context.rax.al = ((uint8_t) qtable[0]) >> 4;
    set_quants_lo(&context);
    
    rfx_dwt_2d_encode_block_horiz_16_16(
        &context,
        work_buffer + 16 * 8 * 2,   // src
        // 32 * 32 * 6 + 16 * 16 * 6 + 8 * 8 * 4 = 7936
        out_buffer + 7936,          // dst hi - HH3
        // 32 * 32 * 6 + 16 * 16 * 6 + 8 * 8 * 2 = 7808
        out_buffer + 7808           // dst lo - LH3
    );
    
    return 0;
}

/******************************************************************************/
int
rfx_encode_component_rlgr1_arm64_neon(struct rfxencode *enc, const char *qtable,
                                      const uint8 *data,
                                      uint8 *buffer, int buffer_size, int *size)
{
    LLOGLN(10, ("rfx_encode_component_rlgr1_amm64_neon:"));
    if (rfxcodec_encode_dwt_shift_arm64_neon(qtable, data, 
                                             (uint8_t *) enc->dwt_buffer1,
                                             (uint8_t *) enc->dwt_buffer) != 0)
    {
        return 1;
    }
    // FIXME: if i pass enc->dwt_buffer1 instead of enc->dwt_buffer, the test fails
    *size = rfx_encode_diff_rlgr1(enc->dwt_buffer, buffer, buffer_size, 64);
    return 0;
}

/******************************************************************************/
int
rfx_encode_component_rlgr3_arm64_neon(struct rfxencode *enc, const char *qtable,
                                      const uint8 *data,
                                      uint8 *buffer, int buffer_size, int *size)
{
    LLOGLN(10, ("rfx_encode_component_rlgr3_arm64_neon:"));
    if (rfxcodec_encode_dwt_shift_arm64_neon(qtable, data, 
                                             (uint8_t *) enc->dwt_buffer1,
                                             (uint8_t *) enc->dwt_buffer) != 0)
    {
        return 1;
    }
    // FIXME: if i pass enc->dwt_buffer1 instead of enc->dwt_buffer, the test fails
    *size = rfx_encode_diff_rlgr3(enc->dwt_buffer, buffer, buffer_size, 64);
    return 0;
}
