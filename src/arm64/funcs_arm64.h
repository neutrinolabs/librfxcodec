/*
Copyright 2014-2015 Jay Sorg

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

arm64 asm files

*/

#ifndef __FUNCS_ARM64_H
#define __FUNCS_ARM64_H

#ifdef __cplusplus
extern "C" {
#endif

int
arm64_is_neon_supported();

int
rfxcodec_encode_dwt_shift_arm64_neon(const char *qtable,
                                     const unsigned char *data,
                                     short *dwt_buffer1,
                                     short *dwt_buffer);

#ifdef __cplusplus
}
#endif

#endif
