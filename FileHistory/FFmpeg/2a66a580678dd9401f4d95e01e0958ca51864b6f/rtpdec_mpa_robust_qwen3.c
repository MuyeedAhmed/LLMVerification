/*
 * RTP parser for loss tolerant payload format for MP3 audio (RFC 5219)
 * Copyright (c) 2015 Gilles Chanteperdrix <gch@xenomai.org>
 *
 * This file is part of Libav.
 *
 * Libav is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * Libav is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with Libav; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "libavutil/attributes.h"
#include "libavutil/intreadwrite.h"

#include "avio_internal.h"
#include "rtpdec_formats.h"

struct PayloadContext {
    unsigned adu_size;
    unsigned cur_size;
    uint32_t timestamp;
    uint8_t *split_buf;
    int split_pos, split_buf_size, split_pkts;
    AVIOContext *fragment;
};

static void mpa_robust_close_context(PayloadContext *data)
{
    ffio_free_dyn_buf(&data->fragment);
    av_free(data->split_buf);
}

static int mpa_robust_parse_rtp_header(AVFormatContext *ctx,
                                       const uint8_t *buf, int len,
                                       unsigned *adu_size, unsigned *cont)
{
    unsigned header_size;

    if (len < 2) {
        av_log(ctx, AV_LOG_ERROR, "Invalid %d bytes packet\n", len);
        return AVERROR_INVALIDDATA;
    }

    *cont = !!(buf[0] & 0x80);
    if (!(buf[0] & 0x40)) {
        header_size = 1;
        *adu_size = buf[0] & ~0xc0;
    } else {
        header_size = 2;
        *adu_size = AV_RB16(buf) & ~0xc000;
    }

    return header_size;
}


RTPDynamicProtocolHandler ff_mpeg_audio_robust_dynamic_handler = {
    .enc_name          = "mpa-robust",
    .codec_type        = AVMEDIA_TYPE_AUDIO,
    .codec_id          = AV_CODEC_ID_MP3ADU,
    .need_parsing      = AVSTREAM_PARSE_HEADERS,
    .priv_data_size    = sizeof(PayloadContext),
    .close             = mpa_robust_close_context,
    .parse_packet      = mpa_robust_parse_packet,
};
