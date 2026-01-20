Do not include anything else.

----- BEGIN MODIFIED C CODE -----
/*
 * huffyuv decoder
 *
 * Copyright (c) 2002-2014 Michael Niedermayer <michaelni@gmx.at>
 *
 * see https://multimedia.cx/huffyuv.txt for a description of
 * the algorithm used
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * yuva, gray, 4:4:4, 4:1:1, 4:1:0 and >8 bit per sample support sponsored by NOA
 */

/**
 * @file
 * huffyuv decoder
 */

#define UNCHECKED_BITSTREAM_READER 1

#include "config_components.h"

#include "avcodec.h"
#include "bswapdsp.h"
#include "bytestream.h"
#include "codec_internal.h"
#include "get_bits.h"
#include "huffyuv.h"
#include "huffyuvdsp.h"
#include "lossless_videodsp.h"
#include "thread.h"
#include "libavutil/emms.h"
#include "libavutil/imgutils.h"
#include "libavutil/mem.h"
#include "libavutil/pixdesc.h"

#define VLC_BITS 12

typedef struct HYuvDecContext {
    GetBitContext gb;
    Predictor predictor;
    int interlaced;
    int decorrelate;
    int bitstream_bpp;
    int version;
    int yuy2;                               //use yuy2 instead of 422P
    int bgr32;                              //use bgr32 instead of bgr24
    int bps;
    int n;                                  // 1<<bps
    int vlc_n;                              // number of vlc codes (FFMIN(1<<bps, MAX_VLC_N))
    int alpha;
    int chroma;
    int yuv;
    int chroma_h_shift;
    int chroma_v_shift;
    int flags;
    int context;
    int last_slice_end;

    union {
        uint8_t  *temp[3];
        uint16_t *temp16[3];
    };
    uint8_t len[4][MAX_VLC_N];
    uint32_t bits[4][MAX_VLC_N];
    uint32_t pix_bgr_map[1<<VLC_BITS];
    VLC vlc[8];                             //Y,U,V,A,YY,YU,YV,AA
    uint8_t *bitstream_buffer;
    unsigned int bitstream_buffer_size;
    BswapDSPContext bdsp;
    HuffYUVDSPContext hdsp;
    LLVidDSPContext llviddsp;
} HYuvDecContext;


static const uint8_t classic_shift_luma[] = {
    34, 36, 35, 69, 135, 232,   9, 16, 10, 24,  11,  23,  12,  16, 13, 10,
    14,  8, 15,  8,  16,   8,  17, 20, 16, 10, 207, 206, 205, 236, 11,  8,
    10, 21,  9, 23,   8,   8, 199, 70, 69, 68,
};

static const uint8_t classic_shift_chroma[] = {
    66, 36,  37,  38, 39, 40,  41,  75,  76,  77, 110, 239, 144, 81, 82,  83,
    84, 85, 118, 183, 56, 57,  88,  89,  56,  89, 154,  57,  58, 57, 26, 141,
    57, 56,  58,  57, 58, 57, 184, 119, 214, 245, 116,  83,  82, 49, 80,  79,
    78, 77,  44,  75, 41, 40,  39,  38,  37,  36,  34,
};

static const unsigned char classic_add_luma[256] = {
     3,  9,  5, 12, 10, 35, 32, 29, 27, 50, 48, 45, 44, 41, 39, 37,
    73, 70, 68, 65, 64, 61, 58, 56, 53, 50, 49, 46, 44, 41, 38, 36,
    68, 65, 63, 61, 58, 55, 53, 51, 48, 46, 45, 43, 41, 39, 38, 36,
    35, 33, 32, 30, 29, 27, 26, 25, 48, 47, 46, 44, 43, 41, 40, 39,
    37, 36, 35, 34, 32, 31, 30, 28, 27, 26, 24, 23, 22, 20, 19, 37,
    35, 34, 33, 31, 30, 29, 27, 26, 24, 23, 21, 20, 18, 17, 15, 29,
    27, 26, 24, 22, 21, 19, 17, 16, 14, 26, 25, 23, 21, 19, 18, 16,
    15, 27, 25, 23, 21, 19, 17, 16, 14, 26, 25, 23, 21, 18, 17, 14,
     12, 17, 19, 13,  4,  9,  2, 11,  1,  7,  8,  0, 16,  3, 14,  6,
    12, 10,  5, 15, 18, 11, 10, 13, 15, 16, 19, 20, 22, 24, 27, 15,
    18, 20, 22, 24, 26, 14, 17, 20, 22, 24, 27, 15, 18, 20, 23, 25,
    28, 16, 19, 22, 25, 28, 32, 36, 21, 25, 29, 33, 38, 42, 45, 49,
    28, 31, 34, 37, 40, 42, 44, 47, 49, 50, 52, 54, 56, 57, 59, 60,
    62, 64, 66, 67, 69, 35, 37, 39, 40, 42, 43, 45, 47, 48, 51, 52,
    54, 55, 57, 59, 60, 62, 63, 66, 67, 69, 71, 72, 38, 40, 42, 43,
    46, 47, 49, 51, 26, 28, 30, 31, 33, 34, 18, 19, 11, 13,  7,  8,
};

static const unsigned char classic_add_chroma[256] = {
     3,    1,   2,   2,   2,   2,   3,   3,   7,   5,   7,   5,   8,   6,  11,   9,
     7,   13,  11,  10,   9,   8,   7,   5,   9,   7,   6,   4,   7,   5,   8,   7,
     11,   8,  13,  11,  19,  15,  22,  23,  20,  33,  32,  28,  27,  29,  51,  77,
     43,  45,  76,  81,  46,  82,  75,  55,  56, 144,  58,  80,  60,  74, 147,  63,
    143,  65,  66,  67,  68,  69,  70,  71,  72,  73,  74,  75,  76,  77,  78,  79,
     80,  81,  82,  83,  84,  85,  86,  87,  88,  89,  90,  91,  27,  30,  21,  22,
     17,  14,   5,   6, 100,  54,  47,  50,  51,  53, 106, 107, 108, 109, 110, 111,
    112, 113, 114, 115,   4, 117, 118,  92,  94, 121, 122,   3, 124, 103,   2,   1,
      0, 129, 130, 131, 120, 119, 126, 125, 136, 137, 138, 139, 140, 141, 142, 134,
    135, 132, 133, 104,  64, 101,  62,  57, 102,  95,  93,  59,  61,  28,  97,  96,
     52,  49,  48,  29,  32,  25,  24,  46,  23,  98,  45,  44,  43,  20,  42,  41,
     19,  18,  99,  40,  15,  39,  38,  16,  13,  12,  11,  37,  10,   9,   8,  36,
      7, 128, 127, 105, 123, 116,  35,  34,  33, 145,  31,  79,  42, 146,  78,  26,
     83,  48,  49,  50,  44,  47,  26,  31,  30,  18,  17,  19,  21,  24,  25,  13,
     14,  16,  17,  18,  20,  21,  12,  14,  15,   9,  10,   6,   9,   6,   5,   8,
      6,  12,   8,  10,   7,   9,   6,   4,   6,   2,   2,   3,   3,   3,   3,   2,
};

static int read_len_table(uint8_t *dst, GetByteContext *gb, int n)
{
    int i, val, repeat;

    for (i = 0; i < n;) {
        if (bytestream2_get_bytes_left(gb) <= 0)
            goto error;
        repeat = bytestream2_peek_byteu(gb) >> 5;
        val    = bytestream2_get_byteu(gb) & 0x1F;
        if (repeat == 0) {
            if (bytestream2_get_bytes_left(gb) <= 0)
                goto error;
            repeat = bytestream2_get_byteu(gb);
        }
        if (i + repeat > n)
            goto error;
        while (repeat--)
            dst[i++] = val;
    }
    return 0;

error:
    av_log(NULL, AV_LOG_ERROR, "Error reading huffman table\n");
    return AVERROR_INVALIDDATA;
}

static int generate_joint_tables(HYuvDecContext *s)
{
    int ret;
    uint16_t *symbols = av_mallocz(5 << VLC_BITS);
    uint16_t *bits;
    uint8_t *len;
    if (!symbols)
        return AVERROR(ENOMEM);
    bits = symbols + (1 << VLC_BITS);
    len = (uint8_t *)(bits + (1 << VLC_BITS));

    if (s->bitstream_bpp < 24 || s->version > 2) {
        int count = 1 + s->alpha + 2 * s->chroma;
        int p, i, y, u;
        for (p = 0; p < count; p++) {
            int p0 = s->version > 2 ? p : 0;
            for (i = y = 0; y < s->vlc_n; y++) {
                int len0  = s->len[p0][y];
                int limit = VLC_BITS - len0;
                if (limit <= 0 || !len0)
                    continue;
                if ((sign_extend(y, 8) & (s->vlc_n-1)) != y)
                    continue;
                for (u = 0; u < s->vlc_n; u++) {
                    int len1 = s->len[p][u];
                    if (len1 > limit || !len1)
                        continue;
                    if ((sign_extend(u, 8) & (s->vlc_n-1)) != u)
                        continue;
                    av_assert0(i < (1 << VLC_BITS));
                    len[i]     = len0 + len1;
                    bits[i]    = (s->bits[p0][y] << len1) + s->bits[p][u];
                    symbols[i] = (y << 8) + (u & 0xFF);
                        i++;
                }
            }
            ff_vlc_free(&s->vlc[4 + p]);
            if ((ret = ff_vlc_init_sparse(&s->vlc[4 + p], VLC_BITS, i, len, 1, 1,
                                          bits, 2, 2, symbols, 2, 2, 0)) < 0)
                goto out;
        }
    } else {
        uint8_t (*map)[4] = (uint8_t(*)[4]) s->pix_bgr_map;
        int i, b, g, r, code;
        int p0 = s->decorrelate;
        int p1 = !s->decorrelate;
        /* Restrict the range to +/-16 because that's pretty much guaranteed
         * to cover all the combinations that fit in 11 bits total, and it
         *  does not matter if we miss a few rare codes. */
        for (i = 0, g = -16; g < 16; g++) {
            int len0   = s->len[p0][g & 255];
            int limit0 = VLC_BITS - len0;
            if (limit0 < 2 || !len0)
                continue;
            for (b = -16; b < 16; b++) {
                int len1   = s->len[p1][b & 255];
                int limit1 = limit0 - len1;
                if (limit1 < 1 || !len1)
                    continue;
                code = (s->bits[p0][g & 255] << len1) + s->bits[p1][b & 255];
                for (r = -16; r < 16; r++) {
                    int len2 = s->len[2][r & 255];
                    if (len2 > limit1 || !len2)
                        continue;
                    av_assert0(i < (1 << VLC_BITS));
                    len[i]  = len0 + len1 + len2;
                    bits[i] = (code << len2) + s->bits[2][r & 255];
                    if (s->decorrelate) {
                        map[i][G] = g;
                        map[i][B] = g + b;
                        map[i][R] = g + r;
                    } else {
                        map[i][B] = g;
                        map[i][G] = b;
                        map[i][R] = r;
                    }
                    i++;
                }
            }
        }
        ff_vlc_free(&s->vlc[4]);
        if ((ret = vlc_init(&s->vlc[4], VLC_BITS, i, len, 1, 1,
                            bits, 2, 2, 0)) < 0)
            goto out;
    }
    ret = 0;
out:
    av_freep(&symbols);
    return ret;
}

static int read_huffman_tables(HYuvDecContext *s, const uint8_t *src, int length)
{
    GetByteContext gb;
    int i, ret;
    int count = 3;

    bytestream2_init(&gb, src, length);

    if (s->version > 2)
        count = 1 + s->alpha + 2*s->chroma;

    for (i = 0; i < count; i++) {
        if ((ret = read_len_table(s->len[i], &gb, s->vlc_n)) < 0)
            return ret;
        if ((ret = ff_huffyuv_generate_bits_table(s->bits[i], s->len[i], s->vlc_n)) < 0)
            return ret;
        ff_vlc_free(&s->vlc[i]);
        if ((ret = vlc_init(&s->vlc[i], VLC_BITS, s->vlc_n, s->len[i], 1, 1,
                           s->bits[i], 4, 4, 0)) < 0)
            return ret;
    }

    if ((ret = generate_joint_tables(s)) < 0)
        return ret;

    return bytestream2_tell(&gb);
}

static int read_old_huffman_tables(HYuvDecContext *s)
{
    GetByteContext gb;
    int i, ret;

    bytestream2_init(&gb, classic_shift_luma,
                     sizeof(classic_shift_luma));
    if ((ret = read_len_table(s->len[0], &gb, 256)) < 0)
        return ret;

    bytestream2_init(&gb, classic_shift_chroma,
                     sizeof(classic_shift_chroma));
    if ((ret = read_len_table(s->len[1], &gb, 256)) < 0)
        return ret;

    for (i = 0; i < 256; i++)
        s->bits[0][i] = classic_add_luma[i];
    for (i = 0; i < 256; i++)
        s->bits[1][i] = classic_add_chroma[i];

    if (s->bitstream_bpp >= 24) {
        memcpy(s->bits[1], s->bits[0], 256 * sizeof(uint32_t));
        memcpy(s->len[1], s->len[0], 256 * sizeof(uint8_t));
    }
    memcpy(s->bits[2], s->bits[1], 256 * sizeof(uint32_t));
    memcpy(s->len[2], s->len[1], 256 * sizeof(uint8_t));

    for (i = 0; i < 4; i++) {
        ff_vlc_free(&s->vlc[i]);
        if ((ret = vlc_init(&s->vlc[i], VLC_BITS, 256, s->len[i], 1, 1,
                            s->bits[i], 4, 4, 0)) < 0)
            return ret;
    }

    if ((ret = generate_joint_tables(s)) < 0)
        return ret;

    return 0;
}

static av_cold int decode_end(AVCodecContext *avctx)
{
    HYuvDecContext *s = avctx->priv_data;
    int i;

    for (int i = 0; i < 3; i++)
        av_freep(&s->temp[i]);

    av_freep(&s->bitstream_buffer);

    for (i = 0; i < 8; i++)
        ff_vlc_free(&s->vlc[i]);

    return 0;
}

static av_cold int decode_init(AVCodecContext *avctx)
{
    HYuvDecContext *s = avctx->priv_data;
    int ret;

    ret = av_image_check_size(avctx->width, avctx->height, 0, avctx);
    if (ret < 0)
        return ret;

    s->flags = avctx->flags;

    ff_bswapdsp_init(&s->bdsp);
    ff_huffyuvdsp_init(&s->hdsp, avctx->pix_fmt);
    ff_llviddsp_init(&s->llviddsp);

    s->interlaced = avctx->height > 288;
    s->bgr32      = 1;

    if (avctx->extradata_size) {
        if ((avctx->bits_per_coded_sample & 7) &&
            avctx->bits_per_coded_sample != 12)
            s->version = 1; // do such files exist at all?
        else if (avctx->extradata_size > 3 && avctx->extradata[3] == 0)
            s->version = 2;
        else
            s->version = 3;
    } else
        s->version = 0;

    s->bps = 8;
    s->n = 1<<s->bps;
    s->vlc_n = FFMIN(s->n, MAX_VLC_N);
    s->chroma = 1;
    if (s->version >= 2) {
        int method, interlace;

        if (avctx->extradata_size < 4)
            return AVERROR_INVALIDDATA;

        method           = avctx->extradata[0];
        s->decorrelate   = method & 64 ? 1 : 0;
        s->predictor     = method & 63;
        if (s->version == 2) {
            s->bitstream_bpp = avctx->extradata[1];
            if (s->bitstream_bpp == 0)
                s->bitstream_bpp = avctx->bits_per_coded_sample & ~7;
        } else {
            s->bps = (avctx->extradata[1] >> 4) + 1;
            s->n = 1<<s->bps;
            s->vlc_n = FFMIN(s->n, MAX_VLC_N);
            s->chroma_h_shift = avctx->extradata[1] & 3;
            s->chroma_v_shift = (avctx->extradata[1] >> 2) & 3;
            s->yuv   = !!(avctx->extradata[2] & 1);
            s->chroma= !!(avctx->extradata[2] & 3);
            s->alpha = !!(avctx->extradata[2] & 4);
        }
        interlace     = (avctx->extradata[2] & 0x30) >> 4;
        s->interlaced = (interlace == 1) ? 1 : (interlace == 2) ? 0 : s->interlaced;
        s->context    = avctx->extradata[2] & 0x40 ? 1 : 0;

        if ((ret = read_huffman_tables(s, avctx->extradata + 4,
                                       avctx->extradata_size - 4)) < 0)
            return ret;
    } else {
        switch (avctx->bits_per_coded_sample & 7) {
        case 1:
            s->predictor   = LEFT;
            s->decorrelate = 0;
            break;
        case 2:
            s->predictor   = LEFT;
            s->decorrelate = 1;
            break;
        case 3:
            s->predictor   = PLANE;
            s->decorrelate = avctx->bits_per_coded_sample >= 24;
            break;
        case 4:
            s->predictor   = MEDIAN;
            s->decorrelate = 0;
            break;
        default:
            s->predictor   = LEFT; // OLD
            s->decorrelate = 0;
            break;
        }
        s->bitstream_bpp = avctx->bits_per_coded_sample & ~7;
        s->context       = 0;

        if ((ret = read_old_huffman_tables(s)) < 0)
            return ret;
    }

    if (s->version <= 2) {
        switch (s->bitstream_bpp) {
        case 12:
            avctx->pix_fmt = AV_PIX_FMT_YUV420P;
            s->yuv = 1;
            break;
        case 16:
            if (s->yuy2)
                avctx->pix_fmt = AV_PIX_FMT_YUYV422;
            else
                avctx->pix_fmt = AV_PIX_FMT_YUV422P;
            s->yuv = 1;
            break;
        case 24:
            if (s->bgr32)
                avctx->pix_fmt = AV_PIX_FMT_0RGB32;
            else
                avctx->pix_fmt = AV_PIX_FMT_BGR24;
            break;
        case 32:
            av_assert0(s->bgr32);
            avctx->pix_fmt = AV_PIX_FMT_RGB32;
            s->alpha = 1;
            break;
        default:
            return AVERROR_INVALIDDATA;
        }
        av_pix_fmt_get_chroma_sub_sample(avctx->pix_fmt,
                                         &s->chroma_h_shift,
                                         &s->chroma_v_shift);
    } else {
        switch ( (s->chroma<<10) | (s->yuv<<9) | (s->alpha<<8) | ((s->bps-1)<<4) | s->chroma_h_shift | (s->chroma_v_shift<<2)) {
        case 0x070:
            avctx->pix_fmt = AV_PIX_FMT_GRAY8;
            break;
        case 0x0F0:
            avctx->pix_fmt = AV_PIX_FMT_GRAY16;
            break;
        case 0x470:
            avctx->pix_fmt = AV_PIX_FMT_GBRP;
            break;
        case 0x480:
            avctx->pix_fmt = AV_PIX_FMT_GBRP9;
            break;
        case 0x490:
            avctx->pix_fmt = AV_PIX_FMT_GBRP10;
            break;
        case 0x4B0:
            avctx->pix_fmt = AV_PIX_FMT_GBRP12;
            break;
        case 0x4D0:
            avctx->pix_fmt = AV_PIX_FMT_GBRP14;
            break;
        case 0x4F0:
            avctx->pix_fmt = AV_PIX_FMT_GBRP16;
            break;
        case 0x570:
            avctx->pix_fmt = AV_PIX_FMT_GBRAP;
            break;
        case 0x670:
            avctx->pix_fmt = AV_PIX_FMT_YUV444P;
            break;
        case 0x680:
            avctx->pix_fmt = AV_PIX_FMT_YUV444P9;
            break;
        case 0x690:
            avctx->pix_fmt = AV_PIX_FMT_YUV444P10;
            break;
        case 0x6B0:
            avctx->pix_fmt = AV_PIX_FMT_YUV444P12;
            break;
        case 0x6D0:
            avctx->pix_fmt = AV_PIX_FMT_YUV444P14;
            break;
        case 0x6F0:
            avctx->pix_fmt = AV_PIX_FMT_YUV444P16;
            break;
        case 0x671:
            avctx->pix_fmt = AV_PIX_FMT_YUV422P;
            break;
        case 0x681:
            avctx->pix_fmt = AV_PIX_FMT_YUV422P9;
            break;
        case 0x691:
            avctx->pix_fmt = AV_PIX_FMT_YUV422P10;
            break;
        case 0x6B1:
            avctx->pix_fmt = AV_PIX_FMT_YUV422P12;
            break;
        case 0x6D1:
            avctx->pix_fmt = AV_PIX_FMT_YUV422P14;
            break;
        case 0x6F1:
            avctx->pix_fmt = AV_PIX_FMT_YUV422P16;
            break;
        case 0x672:
            avctx->pix_fmt = AV_PIX_FMT_YUV411P;
            break;
        case 0x674:
            avctx->pix_fmt = AV_PIX_FMT_YUV440P;
            break;
        case 0x675:
            avctx->pix_fmt = AV_PIX_FMT_YUV420P;
            break;
        case 0x685:
            avctx->pix_fmt = AV_PIX_FMT_YUV420P9;
            break;
        case 0x695:
            avctx->pix_fmt = AV_PIX_FMT_YUV420P10;
            break;
        case 0x6B5:
            avctx->pix_fmt = AV_PIX_FMT_YUV420P12;
            break;
        case 0x6D5:
            avctx->pix_fmt = AV_PIX_FMT_YUV420P14;
            break;
        case 0x6F5:
            avctx->pix_fmt = AV_PIX_FMT_YUV420P16;
            break;
        case 0x67A:
            avctx->pix_fmt = AV_PIX_FMT_YUV410P;
            break;
        case 0x770:
            avctx->pix_fmt = AV_PIX_FMT_YUVA444P;
            break;
        case 0x780:
            avctx->pix_fmt = AV_PIX_FMT_YUVA444P9;
            break;
        case 0x790:
            avctx->pix_fmt = AV_PIX_FMT_YUVA444P10;
            break;
        case 0x7F0:
            avctx->pix_fmt = AV_PIX_FMT_YUVA444P16;
            break;
        case 0x771:
            avctx->pix_fmt = AV_PIX_FMT_YUVA422P;
            break;
        case 0x781:
            avctx->pix_fmt = AV_PIX_FMT_YUVA422P9;
            break;
        case 0x791:
            avctx->pix_fmt = AV_PIX_FMT_YUVA422P10;
            break;
        case 0x7F1:
            avctx->pix_fmt = AV_PIX_FMT_YUVA422P16;
            break;
        case 0x775:
            avctx->pix_fmt = AV_PIX_FMT_YUVA420P;
            break;
        case 0x785:
            avctx->pix_fmt = AV_PIX_FMT_YUVA420P9;
            break;
        case 0x795:
            avctx->pix_fmt = AV_PIX_FMT_YUVA420P10;
            break;
        case 0x7F5:
            avctx->pix_fmt = AV_PIX_FMT_YUVA420P16;
            break;
        default:
            return AVERROR_INVALIDDATA;
        }
    }

    if ((avctx->pix_fmt == AV_PIX_FMT_YUV422P || avctx->pix_fmt == AV_PIX_FMT_YUV420P) && avctx->width & 1) {
        av_log(avctx, AV_LOG_ERROR, "width must be even for this colorspace\n");
        return AVERROR_INVALIDDATA;
    }
    if (s->predictor == MEDIAN && avctx->pix_fmt == AV_PIX_FMT_YUV422P &&
        avctx->width % 4) {
        av_log(avctx, AV_LOG_ERROR,
