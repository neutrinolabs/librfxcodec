/**
 * RFX codec encoder test
 *
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
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <rfxcodec_encode.h>

#define MAX_OUT_DATA_BYTES (1024 * 1024)
#define MAX_BMP_DATA_BYTES (10 * 1024 * 1024)

static char g_in_filename[256] = "";
static char g_out_filename[256] = "";
static int g_count = 1;
static int g_no_accel = 0;
static int g_use_rlgr1 = 0;

struct bmp_magic
{
    char magic[2];
};

struct bmp_hdr
{
    unsigned int   size;
    unsigned short reserved1;
    unsigned short reserved2;
    unsigned int offset;
};

struct dib_hdr
{
    unsigned int   hdr_size;
    int            width;
    int            height;
    unsigned short nplanes;
    unsigned short bpp;
    unsigned int   compress_type;
    unsigned int   image_size;
    int            hres;
    int            vres;
    unsigned int   ncolors;
    unsigned int   nimpcolors;
};

static int
read_bitmap(char *file_name, int *width, int *height, int *bpp, char *bmp_data)
{
    int i;
    int j;
    int e;
    int fd;
    int file_stride_bytes;
    int pixel;
    struct bmp_magic bm;
    struct bmp_hdr bh;
    struct dib_hdr dh;
    unsigned char *src8;
    int *src32;
    int *dst32;

    fd = open(file_name, O_RDONLY);
    if (fd == -1)
    {
        return 1;
    }
    if (read(fd, &bm, sizeof(bm)) != sizeof(bm))
    {
        close(fd);
        return 1;
    }
    if (read(fd, &bh, sizeof(bh)) != sizeof(bh))
    {
        close(fd);
        return 1;
    }
    if (read(fd, &dh, sizeof(dh)) != sizeof(dh))
    {
        close(fd);
        return 1;
    }

    if (dh.bpp != 24 && dh.bpp != 32)
    {
        printf("error, only support 24 and 32 bpp\n");
        close(fd);
        return 1;
    }

    *width = dh.width;
    *height = dh.height;
    *bpp = dh.bpp;

    file_stride_bytes = dh.width * ((dh.bpp + 7) / 8);
    e = (4 - file_stride_bytes) & 3;
    src8 = (unsigned char *) malloc(file_stride_bytes * 4);
    for (j = 0; j < dh.height; j++)
    {
        dst32 = (int *) (bmp_data + (dh.width * dh.height * 4) - ((j + 1) * dh.width * 4));
        if (read(fd, src8, file_stride_bytes + e) != file_stride_bytes + e)
        {
            free(src8);
            close(fd);
            return 1;
        }
        if (dh.bpp == 32)
        {
            src32 = (int *) src8;
            for (i = 0; i < dh.width; i++)
            {
                pixel = src32[i];
                dst32[i] = pixel;
            }
        }
        else if (dh.bpp == 24)
        {
            for (i = 0; i < dh.width; i++)
            {
                pixel =  src8[i * 3 + 0] << 0;
                pixel |= src8[i * 3 + 1] << 8;
                pixel |= src8[i * 3 + 2] << 16;
                dst32[i] = pixel;
            }
        }
    }

    free(src8);
    close(fd);
    return 0;
}

static int
out_params(void)
{
    printf("rfxencode: a RemoteFX encoder testing program.\n");
    printf("  -i <in file name> bmp file\n");
    printf("  -o <out file name> rfx file\n");
    printf("  -c <number> times to loop\n");
    printf("  -n no accel\n");
    printf("  -1 use rlgr1\n");
    return 0;
}

static int
process(void)
{
    char *out_data;
    char *bmp_data;
    int out_fd;
    int out_bytes = 0;
    int error;
    int index;
    int index_x;
    int index_y;
    void *han;
    int num_tiles;
    int num_tiles_x;
    int num_tiles_y;
    int flags;
    int width;
    int height;
    int bpp;
    struct rfx_rect region;
    struct rfx_tile *tiles;
    struct rfx_tile *tile;

    out_data = (char *) malloc(MAX_OUT_DATA_BYTES);
    bmp_data = (char *) malloc(MAX_BMP_DATA_BYTES);
    memset(bmp_data, 0xff, MAX_BMP_DATA_BYTES);

    if (read_bitmap(g_in_filename, &width, &height, &bpp, bmp_data) != 0)
    {
        printf("read bitmap failed\n");
        free(bmp_data);
        free(out_data);
        return 1;
    }

    printf("process: got bitmap width %d height %d bpp %d\n", width, height, bpp);

    flags = 0;
    if (g_no_accel)
    {
        flags |= RFX_FLAGS_NOACCEL;
    }
    if (g_use_rlgr1)
    {
        flags |= RFX_FLAGS_RLGR1;
    }
    han = rfxcodec_encode_create(1920, 1080, RFX_FORMAT_BGRA, flags);

    region.x = 0;
    region.y = 0;
    region.cx = width;
    region.cy = height;

    num_tiles_x = (width + 63) / 64;
    num_tiles_y = (height + 63) / 64;

    num_tiles = num_tiles_x * num_tiles_y;
    tiles = (struct rfx_tile *) calloc(num_tiles, sizeof(struct rfx_tile));
    if (tiles == NULL)
    {
        free(bmp_data);
        free(out_data);
        return 1;
    }
    for (index_y = 0; index_y < num_tiles_y; index_y++)
    {
        for (index_x = 0; index_x < num_tiles_x; index_x++)
        {
            tile = tiles + (index_y * num_tiles_x + index_x);
            tile->x = index_x * 64;
            tile->y = index_y * 64;
            tile->cx = 64;
            tile->cy = 64;
        }
    }
    if (han != NULL)
    {
        error = 0;
        for (index = 0; index < g_count; index++)
        {
            out_bytes = 1024 * 1024;
            error = rfxcodec_encode(han, out_data, &out_bytes, bmp_data,
                                    width, height, width * 4,
                                    &region, 1, tiles, num_tiles, NULL, 0);
            if (error < num_tiles)
            {
                break;
            }
        }
        printf("error %d out_bytes %d num_tiles %d\n", error,
               out_bytes, num_tiles);
        if (g_out_filename[0] != 0)
        {
            out_fd = open(g_out_filename, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
            if (out_fd == -1)
            {
                printf("failed to open %s\n", g_out_filename);
                free(bmp_data);
                free(out_data);
                free(tiles);
                return 1;
            }
            if (write(out_fd, out_data, out_bytes) != out_bytes)
            {
                printf("write failed\n");
                free(bmp_data);
                free(out_data);
                free(tiles);
                close(out_fd);
                return 1;
            }
            close(out_fd);
        }
    }
    rfxcodec_encode_destroy(han);

    free(bmp_data);
    free(out_data);
    free(tiles);
    return 0;
}

int main(int argc, char **argv)
{
    int index;

    if (argc < 2)
    {
        out_params();
        return 0;
    }
    for (index = 1; index < argc; index++)
    {
        if (strcmp(argv[index], "-i") == 0)
        {
            index++;
            strcpy(g_in_filename, argv[index]);
        }
        else if (strcmp(argv[index], "-o") == 0)
        {
            index++;
            strcpy(g_out_filename, argv[index]);
        }
        else if (strcmp(argv[index], "-c") == 0)
        {
            index++;
            g_count = atoi(argv[index]);
        }
        else if (strcmp(argv[index], "-n") == 0)
        {
            g_no_accel = 1;
        }
        else if (strcmp(argv[index], "-1") == 0)
        {
            g_use_rlgr1 = 1;
        }
        else
        {
            out_params();
            return 0;
        }
    }
    process();
    return 0;
}

