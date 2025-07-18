
/*
 * Microsoft RTP/ASF support.
 * Copyright (c) 2008 Ronald S. Bultje
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

/**
 * @file
 * @brief Microsoft RTP/ASF support
 * @author Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 */

#include "libavutil/base64.h"
#include "libavutil/avstring.h"
#include "libavutil/intreadwrite.h"
#include "rtp.h"
#include "rtpdec_formats.h"
#include "rtsp.h"
#include "asf.h"
#include "avio_internal.h"
#include "internal.h"

static int rtp_asf_fix_header(uint8_t *buf, int len)
{
    uint8_t *p = buf, *end = buf + len;

    if (len < sizeof(ff_asf_guid) * 2 + 22 ||
        memcmp(p, ff_asf_header, sizeof(ff_asf_guid))) {
        return -1;
    }
    p += sizeof(ff_asf_guid) + 14;
    do {
        uint64_t chunksize = AV_RL64(p + sizeof(ff_asf_guid));
        if (memcmp(p, ff_asf_file_header, sizeof(ff_asf_guid))) {
            if (chunksize > end - p)
                return -1;
            p += chunksize;
            continue;
        }

        p += 6 * 8 + 3 * 4 + sizeof(ff_asf_guid) * 2;
        if (p + 8 <= end && AV_RL32(p) == AV_RL32(p + 4)) {
            AV_WL32(p, 0);
            return 0;
        }
        break;
    } while (end - p >= sizeof(ff_asf_guid) + 8);

    return -1;
}

static int packetizer_read(void *opaque, uint8_t *buf, int buf_size)
{
    return AVERROR(EAGAIN);
}

static void init_packetizer(AVIOContext *pb, uint8_t *buf, int len)
{
    ffio_init_context(pb, buf, len, 0, NULL, packetizer_read, NULL, NULL);
    pb->pos     = len;
    pb->buf_end = buf + len;
}

int ff_wms_parse_sdp_a_line(AVFormatContext *s, const char *p)
{
    int ret = 0;
    if (av_strstart(p, "pgmpu:data:application/vnd.ms.wms-hdr.asfv1;base64,", &p)) {
        AVIOContext pb;
        RTSPState *rt = s->priv_data;
        AVDictionary *opts = NULL;
        int len = strlen(p) * 6 / 8;
        char *buf = av_mallocz(len);
        av_base64_decode(buf, p, len);

        if (rtp_asf_fix_header(buf, len) < 0)
            av_log(s, AV_LOG_ERROR,
                   "Failed to fix invalid RTSP-MS/ASF min_pktsize\n");
        init_packetizer(&pb, buf, len);
        if (rt->asf_ctx) {
            avformat_close_input(&rt->asf_ctx);
        }
        if (!(rt->asf_ctx = avformat_alloc_context()))
            return AVERROR(ENOMEM);
        rt->asf_ctx->pb      = &pb;
        av_dict_set(&opts, "no_resync_search", "1", 0);
        ret = avformat_open_input(&rt->asf_ctx, "", &ff_asf_demuxer, &opts);
        av_dict_free(&opts);
        if (ret < 0)
            return ret;
        av_dict_copy(&s->metadata, rt->asf_ctx->metadata, 0);
        rt->asf_pb_pos = avio_tell(&pb);
        av_free(buf);
        rt->asf_ctx->pb = NULL;
    }
    return ret;
}

static int asfrtp_parse_sdp_line(AVFormatContext *s, int stream_index,
                                 PayloadContext *asf, const char *line)
{
    if (stream_index < 0)
        return 0;
    if (av_strstart(line, "stream:", &line)) {
        RTSPState *rt = s->priv_data;

        s->streams[stream_index]->id = strtol(line, NULL, 10);

        if (rt->asf_ctx) {
            int i;

            for (i = 0; i < rt->asf_ctx->nb_streams; i++) {
                if (s->streams[stream_index]->id == rt->asf_ctx->streams[i]->id) {
                    *s->streams[stream_index]->codec =
                        *rt->asf_ctx->streams[i]->codec;
                    s->streams[stream_index]->need_parsing =
                        rt->asf_ctx->streams[i]->need_parsing;
                    rt->asf_ctx->streams[i]->codec->extradata_size = 0;
                    rt->asf_ctx->streams[i]->codec->extradata = NULL;
                    avpriv_set_pts_info(s->streams[stream_index], 32, 1, 1000);
                }
           }
        }
    }

    return 0;
}

struct PayloadContext {
    AVIOContext *pktbuf, pb;
    uint8_t *buf;
};

static int asfrtp_parse_packet(AVFormatContext *s, PayloadContext *asf,
                               AVStream *st, AVPacket *pkt,
                               uint32_t *timestamp,
                               const uint8_t *buf, int len, uint16_t seq,
                               int flags)
{
    AVIOContext *pb = &asf->pb;
    int res, mflags, len_off;
    RTSPState *rt = s->priv_data;

    if (!rt->asf_ctx)
        return -1;

    if (len > 0) {
        int off, out_len = 0;

        if (len < 4)
            return -1;

        av_freep(&asf->buf);

        ffio_init_context(pb, buf, len, 0, NULL, NULL, NULL, NULL);

        while (avio_tell(pb) + 4 < len) {
            int start_off = avio_tell(pb);

            mflags = avio_r8(pb);
            len_off = avio_rb24(pb);
            if (mflags & 0x20)
                avio_skip(pb, 4);
            if (mflags & 0x10)
                avio_skip(pb, 4);
            if (mflags & 0x8)
                avio_skip(pb, 4);
            off = avio_tell(pb);

            if (!(mflags & 0x40)) {
                if (asf->pktbuf && len_off != avio_tell(asf->pktbuf)) {
                    ffio_free_dyn_buf(&asf->pktbuf);
                }
                if (!len_off && !asf->pktbuf &&
                    (res = avio_open_dyn_buf(&asf->pktbuf)) < 0)
                    return res;
                if (!asf->pktbuf)
                    return AVERROR(EIO);

                avio_write(asf->pktbuf, buf + off, len - off);
                avio_skip(pb, len - off);
                if (!(flags & RTP_FLAG_MARKER))
                    return -1;
                out_len     = avio_close_dyn_buf(asf->pktbuf, &asf->buf);
                asf->pktbuf = NULL;
            } else {
                int cur_len = start_off + len_off - off;
                int prev_len = out_len;
                out_len += cur_len;
                if (FFMIN(cur_len, len - off) < 0)
                    return -1;
                if ((res = av_reallocp(&asf->buf, out_len)) < 0)
                    return res;
                memcpy(asf->buf + prev_len, buf + off,
                       FFMIN(cur_len, len - off));
                avio_skip(pb, cur_len);
            }
        }

        init_packetizer(pb, asf->buf, out_len);
        pb->pos += rt->asf_pb_pos;
        pb->eof_reached = 0;
        rt->asf_ctx->pb = pb;
    }

    for (;;) {
        int i;

        res = ff_read_packet(rt->asf_ctx, pkt);
        rt->asf_pb_pos = avio_tell(pb);
        if (res != 0)
            break;
        for (i = 0; i < s->nb_streams; i++) {
            if (s->streams[i]->id == rt->asf_ctx->streams[pkt->stream_index]->id) {
                pkt->stream_index = i;
                pkt->flags &= ~AV_PKT_FLAG_KEY;
                return 1;
            }
        }
        av_free_packet(pkt);
    }

    return res == 1 ? -1 : res;
}

static void asfrtp_close_context(PayloadContext *asf)
{
    ffio_free_dyn_buf(&asf->pktbuf);
    av_freep(&asf->buf);
}

#define RTP_ASF_HANDLER(n, s, t) \
RTPDynamicProtocolHandler ff_ms_rtp_ ## n ## _handler = { \
    .enc_name         = s, \
    .codec_type       = t, \
    .codec_id         = AV_CODEC_ID_NONE, \
    .priv_data_size   = sizeof(PayloadContext), \
    .parse_sdp_a_line = asfrtp_parse_sdp_line, \
    .close            = asfrtp_close_context, \
    .parse_packet     = asfrtp_parse_packet,   \
}

RTP_ASF_HANDLER(asf_pfv, "x-asf-pf",  AVMEDIA_TYPE_VIDEO);
RTP_ASF_HANDLER(asf_pfa, "x-asf-pf",  AVMEDIA_TYPE_AUDIO);
