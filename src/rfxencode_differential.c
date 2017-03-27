/**
 * FreeRDP: A Remote Desktop Protocol client.
 * RemoteFX Codec Library - Differential Encoding
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

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rfxcodec_encode.h>

#include "rfxcommon.h"
#include "rfxencode.h"
#include "rfxconstants.h"

/******************************************************************************/
int
rfx_differential_encode(sint16 *buffer, int buffer_size)
{
    sint16 n1, n2;
    sint16 *dst;

    for (n1 = *buffer, dst = buffer + 1; buffer_size > 1; dst++, buffer_size--)
    {
        n2 = *dst;
        *dst -= n1;
        n1 = n2;
    }
    return 0;
}
