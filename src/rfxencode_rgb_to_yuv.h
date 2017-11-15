/**
 * FreeRDP: A Remote Desktop Protocol client.
 * RemoteFX Codec Library - Encode
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

#ifndef __RFXRBGTOYUV_H
#define __RFXRBGTOYUV_H

#include "rfxcommon.h"

int
rfx_encode_rgb_to_yuv(struct rfxencode *enc, const char *rgb_data,
                      int width, int height, int stride_bytes);
int
rfx_encode_argb_to_yuva(struct rfxencode *enc, const char *argb_data,
                        int width, int height, int stride_bytes);

#endif
