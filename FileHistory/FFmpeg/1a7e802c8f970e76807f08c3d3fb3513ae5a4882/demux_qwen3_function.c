Do NOT return any extra information.

----- BEGIN SOLUTION -----
/*
 * Core demuxing component
 * Copyright (c) 2000, 2001, 2002 Fabrice Bellard
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
 */

#include <stdint.h>

#include "config_components.h"

#include "libavutil/avassert.h"
#include "libavutil/avstring.h"
#include "libavutil/dict.h"
#include "libavutil/internal.h"
#include "libavutil/intreadwrite.h"
#include "libavutil/mathematics.h"
#include "libavutil/mem.h"
#include "libavutil/opt.h"
#include "libavutil/pixfmt.h"
#include "libavutil/time.h"
#include "libavutil/timestamp.h"

#include "libavcodec/avcodec.h"
#include "libavcodec/bsf.h"
#include "libavcodec/codec_desc.h"
#include "libavcodec/internal.h"
#include "libavcodec/packet_internal.h"
#include "libavcodec/raw.h"

#include "avformat.h"
#include "avformat_internal.h"
#include "avio_internal.h"
#include "demux.h"
#include "id3v2.h"
#include "internal.h"
#include "url.h"

static int64_t wrap_timestamp(const AVStream *st, int64_t timestamp)
{
    const FFStream *const sti = cffstream(st);
    if (sti->pts_wrap_behavior != AV_PTS_WRAP_IGNORE && st->pts_wrap_bits < 64 &&
        sti->pts_wrap_reference != AV_NOPTS_VALUE && timestamp != AV_NOPTS_VALUE) {
        if (sti->pts_wrap_behavior == AV_PTS_WRAP_ADD_OFFSET &&
            timestamp < sti->pts_wrap_reference)
            return timestamp + (1ULL << st->pts_wrap_bits);
        else if (sti->pts_wrap_behavior == AV_PTS_WRAP_SUB_OFFSET &&
            timestamp >= sti->pts_wrap_reference)
            return timestamp - (1ULL << st->pts_wrap_bits);
    }
    return timestamp;
}

int64_t ff_wrap_timestamp(const AVStream *st, int64_t timestamp)
{
    return wrap_timestamp(st, timestamp);
}

static const AVCodec *find_probe_decoder(AVFormatContext *s, const AVStream *st, enum AVCodecID codec_id)
{
    const AVCodec *codec;

#if CONFIG_H264_DECODER
    /* Other parts of the code assume this decoder to be used for h264,
     * so force it if possible. */
    if (codec_id == AV_CODEC_ID_H264)
        return avcodec_find_decoder_by_name("h264");
#endif

    codec = ff_find_decoder(s, st, codec_id);
    if (!codec)
        return NULL;

    if (codec->capabilities & AV_CODEC_CAP_AVOID_PROBING) {
        const AVCodec *probe_codec = NULL;
        void *iter = NULL;
        while ((probe_codec = av_codec_iterate(&iter))) {
            if (probe_codec->id == codec->id &&
                    av_codec_is_decoder(probe_codec) &&
                    !(probe_codec->capabilities & (AV_CODEC_CAP_AVOID_PROBING | AV_CODEC_CAP_EXPERIMENTAL))) {
                return probe_codec;
            }
        }
    }

    return codec;
}

static int set_codec_from_probe_data(AVFormatContext *s, AVStream *st,
                                     AVProbeData *pd)
{
    static const struct {
        const char *name;
        enum AVCodecID id;
        enum AVMediaType type;
    } fmt_id_type[] = {
        { "aac",        AV_CODEC_ID_AAC,          AVMEDIA_TYPE_AUDIO    },
        { "ac3",        AV_CODEC_ID_AC3,          AVMEDIA_TYPE_AUDIO    },
        { "aptx",       AV_CODEC_ID_APTX,         AVMEDIA_TYPE_AUDIO    },
        { "av1",        AV_CODEC_ID_AV1,          AVMEDIA_TYPE_VIDEO    },
        { "dts",        AV_CODEC_ID_DTS,          AVMEDIA_TYPE_AUDIO    },
        { "dvbsub",     AV_CODEC_ID_DVB_SUBTITLE, AVMEDIA_TYPE_SUBTITLE },
        { "dvbtxt",     AV_CODEC_ID_DVB_TELETEXT, AVMEDIA_TYPE_SUBTITLE },
        { "eac3",       AV_CODEC_ID_EAC3,         AVMEDIA_TYPE_AUDIO    },
        { "h264",       AV_CODEC_ID_H264,         AVMEDIA_TYPE_VIDEO    },
        { "hevc",       AV_CODEC_ID_HEVC,         AVMEDIA_TYPE_VIDEO    },
        { "loas",       AV_CODEC_ID_AAC_LATM,     AVMEDIA_TYPE_AUDIO    },
        { "m4v",        AV_CODEC_ID_MPEG4,        AVMEDIA_TYPE_VIDEO    },
        { "mjpeg_2000", AV_CODEC_ID_JPEG2000,     AVMEDIA_TYPE_VIDEO    },
        { "mp3",        AV_CODEC_ID_MP3,          AVMEDIA_TYPE_AUDIO    },
        { "mpegvideo",  AV_CODEC_ID_MPEG2VIDEO,   AVMEDIA_TYPE_VIDEO    },
        { "truehd",     AV_CODEC_ID_TRUEHD,       AVMEDIA_TYPE_AUDIO    },
        { "evc",        AV_CODEC_ID_EVC,          AVMEDIA_TYPE_VIDEO    },
        { "vvc",        AV_CODEC_ID_VVC,          AVMEDIA_TYPE_VIDEO    },
        { 0 }
    };
    int score;
    const AVInputFormat *fmt = av_probe_input_format3(pd, 1, &score);
    FFStream *const sti = ffstream(st);

    if (fmt) {
        av_log(s, AV_LOG_DEBUG,
               "Probe with size=%d, packets=%d detected %s with score=%d\n",
               pd->buf_size, s->max_probe_packets - sti->probe_packets,
               fmt->name, score);
        for (int i = 0; fmt_id_type[i].name; i++) {
            if (!strcmp(fmt->name, fmt_id_type[i].name)) {
                if (fmt_id_type[i].type != AVMEDIA_TYPE_AUDIO &&
                    st->codecpar->sample_rate)
                    continue;
                if (sti->request_probe > score &&
                    st->codecpar->codec_id != fmt_id_type[i].id)
                    continue;
                st->codecpar->codec_id   = fmt_id_type[i].id;
                st->codecpar->codec_type = fmt_id_type[i].type;
                sti->need_context_update = 1;
                return score;
            }
        }
    }
    return 0;
}

static int init_input(AVFormatContext *s, const char *filename,
                      AVDictionary **options)
{
    int ret;
    AVProbeData pd = { filename, NULL, 0 };
    int score = AVPROBE_SCORE_RETRY;

    if (s->pb) {
        s->flags |= AVFMT_FLAG_CUSTOM_IO;
        if (!s->iformat)
            return av_probe_input_buffer2(s->pb, &s->iformat, filename,
                                          s, 0, s->format_probesize);
        else if (s->iformat->flags & AVFMT_NOFILE)
            av_log(s, AV_LOG_WARNING, "Custom AVIOContext makes no sense and "
                                      "will be ignored with AVFMT_NOFILE format.\n");
        return 0;
    }

    if ((s->iformat && s->iformat->flags & AVFMT_NOFILE) ||
        (!s->iformat && (s->iformat = av_probe_input_format2(&pd, 0, &score))))
        return score;

    if ((ret = s->io_open(s, &s->pb, filename, AVIO_FLAG_READ | s->avio_flags, options)) < 0)
        return ret;

    if (s->iformat)
        return 0;
    return av_probe_input_buffer2(s->pb, &s->iformat, filename,
                                  s, 0, s->format_probesize);
}

static int update_stream_avctx(AVFormatContext *s)
{
    int ret;
    for (unsigned i = 0; i < s->nb_streams; i++) {
        AVStream *const st  = s->streams[i];
        FFStream *const sti = ffstream(st);

        if (!sti->need_context_update)
            continue;

        /* close parser, because it depends on the codec */
        if (sti->parser && sti->avctx->codec_id != st->codecpar->codec_id) {
            av_parser_close(sti->parser);
            sti->parser = NULL;
        }

        /* update internal codec context, for the parser */
        ret = avcodec_parameters_to_context(sti->avctx, st->codecpar);
        if (ret < 0)
            return ret;

        sti->codec_desc = avcodec_descriptor_get(sti->avctx->codec_id);

        sti->need_context_update = 0;
    }
    return 0;
}

int avformat_open_input(AVFormatContext **ps, const char *filename,
                        const AVInputFormat *fmt, AVDictionary **options)
{
    FormatContextInternal *fci;
    AVFormatContext *s = *ps;
    FFFormatContext *si;
    AVDictionary *tmp = NULL;
    ID3v2ExtraMeta *id3v2_extra_meta = NULL;
    int ret = 0;

    if (!s && !(s = avformat_alloc_context()))
        return AVERROR(ENOMEM);
    fci = ff_fc_internal(s);
    si = &fci->fc;
    if (!s->av_class) {
        av_log(NULL, AV_LOG_ERROR, "Input context has not been properly allocated by avformat_alloc_context() and is not NULL either\n");
        return AVERROR(EINVAL);
    }
    if (fmt)
        s->iformat = fmt;

    if (options)
        av_dict_copy(&tmp, *options, 0);

    if (s->pb) // must be before any goto fail
        s->flags |= AVFMT_FLAG_CUSTOM_IO;

    if ((ret = av_opt_set_dict(s, &tmp)) < 0)
        goto fail;

    if (!(s->url = av_strdup(filename ? filename : ""))) {
        ret = AVERROR(ENOMEM);
        goto fail;
    }

    if ((ret = init_input(s, filename, &tmp)) < 0)
        goto fail;
    s->probe_score = ret;

    if (!s->protocol_whitelist && s->pb && s->pb->protocol_whitelist) {
        s->protocol_whitelist = av_strdup(s->pb->protocol_whitelist);
        if (!s->protocol_whitelist) {
            ret = AVERROR(ENOMEM);
            goto fail;
        }
    }

    if (!s->protocol_blacklist && s->pb && s->pb->protocol_blacklist) {
        s->protocol_blacklist = av_strdup(s->pb->protocol_blacklist);
        if (!s->protocol_blacklist) {
            ret = AVERROR(ENOMEM);
            goto fail;
        }
    }

    if (s->format_whitelist && av_match_list(s->iformat->name, s->format_whitelist, ',') <= 0) {
        av_log(s, AV_LOG_ERROR, "Format not on whitelist \'%s\'\n", s->format_whitelist);
        ret = AVERROR(EINVAL);
        goto fail;
    }

    avio_skip(s->pb, s->skip_initial_bytes);

    /* Check filename in case an image number is expected. */
    if (s->iformat->flags & AVFMT_NEEDNUMBER) {
        if (!av_filename_number_test(filename)) {
            ret = AVERROR(EINVAL);
            goto fail;
        }
    }

    s->duration = s->start_time = AV_NOPTS_VALUE;

    /* Allocate private data. */
    if (ffifmt(s->iformat)->priv_data_size > 0) {
        if (!(s->priv_data = av_mallocz(ffifmt(s->iformat)->priv_data_size))) {
            ret = AVERROR(ENOMEM);
            goto fail;
        }
        if (s->iformat->priv_class) {
            *(const AVClass **) s->priv_data = s->iformat->priv_class;
            av_opt_set_defaults(s->priv_data);
            if ((ret = av_opt_set_dict(s->priv_data, &tmp)) < 0)
                goto fail;
        }
    }

    /* e.g. AVFMT_NOFILE formats will not have an AVIOContext */
    if (s->pb)
        ff_id3v2_read_dict(s->pb, &si->id3v2_meta, ID3v2_DEFAULT_MAGIC, &id3v2_extra_meta);

    if (ffifmt(s->iformat)->read_header)
        if ((ret = ffifmt(s->iformat)->read_header(s)) < 0) {
            if (ffifmt(s->iformat)->flags_internal & FF_INFMT_FLAG_INIT_CLEANUP)
                goto close;
            goto fail;
        }

    if (!s->metadata) {
        s->metadata    = si->id3v2_meta;
        si->id3v2_meta = NULL;
    } else if (si->id3v2_meta) {
        av_log(s, AV_LOG_WARNING, "Discarding ID3 tags because more suitable tags were found.\n");
        av_dict_free(&si->id3v2_meta);
    }

    if (id3v2_extra_meta) {
        if (!strcmp(s->iformat->name, "mp3") || !strcmp(s->iformat->name, "aac") ||
            !strcmp(s->iformat->name, "tta") || !strcmp(s->iformat->name, "wav")) {
            if ((ret = ff_id3v2_parse_apic(s, id3v2_extra_meta)) < 0)
                goto close;
            if ((ret = ff_id3v2_parse_chapters(s, id3v2_extra_meta)) < 0)
                goto close;
            if ((ret = ff_id3v2_parse_priv(s, id3v2_extra_meta)) < 0)
                goto close;
        } else
            av_log(s, AV_LOG_DEBUG, "demuxer does not support additional id3 data, skipping\n");
        ff_id3v2_free_extra_meta(&id3v2_extra_meta);
    }

    if ((ret = avformat_queue_attached_pictures(s)) < 0)
        goto close;

    if (s->pb && !si->data_offset)
        si->data_offset = avio_tell(s->pb);

    fci->raw_packet_buffer_size = 0;

    update_stream_avctx(s);

    if (options) {
        av_dict_free(options);
        *options = tmp;
    }
    *ps = s;
    return 0;

close:
    if (ffifmt(s->iformat)->read_close)
        ffifmt(s->iformat)->read_close(s);
fail:
    ff_id3v2_free_extra_meta(&id3v2_extra_meta);
    av_dict_free(&tmp);
    if (s->pb && !(s->flags & AVFMT_FLAG_CUSTOM_IO))
        avio_closep(&s->pb);
    avformat_free_context(s);
    *ps = NULL;
    return ret;
}

void avformat_close_input(AVFormatContext **ps)
{
    AVFormatContext *s;
    AVIOContext *pb;

    if (!ps || !*ps)
        return;

    s  = *ps;
    pb = s->pb;

    if ((s->iformat && strcmp(s->iformat->name, "image2") && s->iformat->flags & AVFMT_NOFILE) ||
        (s->flags & AVFMT_FLAG_CUSTOM_IO))
        pb = NULL;

    if (s->iformat)
        if (ffifmt(s->iformat)->read_close)
            ffifmt(s->iformat)->read_close(s);

    avformat_free_context(s);

    *ps = NULL;

    avio_close2(pb);
}

static void force_codec_ids(AVFormatContext *s, AVStream *st)
{
    switch (st->codecpar->codec_type) {
    case AVMEDIA_TYPE_VIDEO:
        if (s->video_codec_id)
            st->codecpar->codec_id = s->video_codec_id;
        break;
    case AVMEDIA_TYPE_AUDIO:
        if (s->audio_codec_id)
            st->codecpar->codec_id = s->audio_codec_id;
        break;
    case AVMEDIA_TYPE_SUBTITLE:
        if (s->subtitle_codec_id)
            st->codecpar->codec_id = s->subtitle_codec_id;
        break;
    case AVMEDIA_TYPE_DATA:
        if (s->data_codec_id)
            st->codecpar->codec_id = s->data_codec_id;
        break;
    }
}

static int probe_codec(AVFormatContext *s, AVStream *st, const AVPacket *pkt)
{
    FormatContextInternal *const fci = ff_fc_internal(s);
    FFStream *const sti = ffstream(st);

    if (sti->request_probe > 0) {
        AVProbeData *const pd = &sti->probe_data;
        int end;
        av_log(s, AV_LOG_DEBUG, "probing stream %d pp:%d\n", st->index, sti->probe_packets);
        --sti->probe_packets;

        if (pkt) {
            uint8_t *new_buf = av_realloc(pd->buf, pd->buf_size+pkt->size+AVPROBE_PADDING_SIZE);
            if (!new_buf) {
                av_log(s, AV_LOG_WARNING,
                       "Failed to reallocate probe buffer for stream %d\n",
                       st->index);
                goto no_packet;
            }
            pd->buf = new_buf;
            memcpy(pd->buf + pd->buf_size, pkt->data, pkt->size);
            pd->buf_size += pkt->size;
            memset(pd->buf + pd->buf_size, 0, AVPROBE_PADDING_SIZE);
        } else {
no_packet:
            sti->probe_packets = 0;
            if (!pd->buf_size) {
                av_log(s, AV_LOG_WARNING,
                       "nothing to probe for stream %d\n", st->index);
            }
        }

        end = fci->raw_packet_buffer_size >= s->probesize ||
              sti->probe_packets <= 0;

        if (end || av_log2(pd->buf_size) != av_log2(pd->buf_size - pkt->size)) {
            int score = set_codec_from_probe_data(s, st, pd);
            if (    (st->codecpar->codec_id != AV_CODEC_ID_NONE && score > AVPROBE_SCORE_STREAM_RETRY)
                || end) {
                pd->buf_size = 0;
                av_freep(&pd->buf);
                sti->request_probe = -1;
                if (st->codecpar->codec_id != AV_CODEC_ID_NONE) {
                    av_log(s, AV_LOG_DEBUG, "probed stream %d\n", st->index);
                } else
                    av_log(s, AV_LOG_WARNING, "probed stream %d failed\n", st->index);
            }
            force_codec_ids(s, st);
        }
    }
    return 0;
}

static int update_wrap_reference(AVFormatContext *s, AVStream *st, int stream_index, AVPacket *pkt)
{
    FFStream *const sti = ffstream(st);
    int64_t ref = pkt->dts;
    int pts_wrap_behavior;
    int64_t pts_wrap_reference;
    AVProgram *first_program;

    if (ref == AV_NOPTS_VALUE)
        ref = pkt->pts;
    if (sti->pts_wrap_reference != AV_NOPTS_VALUE || st->pts_wrap_bits >= 63 || ref == AV_NOPTS_VALUE || !s->correct_ts_overflow)
        return 0;
    ref &= (1LL << st->pts_wrap_bits)-1;

    // reference time stamp should be 60 s before first time stamp
    pts_wrap_reference = ref - av_rescale(60, st->time_base.den, st->time_base.num);
    // if first time stamp is not more than 1/8 and 60s before the wrap point, subtract rather than add wrap offset
    pts_wrap_behavior = (ref < (1LL << st->pts_wrap_bits) - (1LL << st->pts_wrap_bits-3)) ||
        (ref < (1LL << st->pts_wrap_bits) - av_rescale(60, st->time_base.den, st->time_base.num)) ?
        AV_PTS_WRAP_ADD_OFFSET : AV_PTS_WRAP_SUB_OFFSET;

    first_program = av_find_program_from_stream(s, NULL, stream_index);

    if (!first_program) {
        int default_stream_index = av_find_default_stream_index(s);
        FFStream *const default_sti = ffstream(s->streams[default_stream_index]);
        if (default_sti->pts_wrap_reference == AV_NOPTS_VALUE) {
            for (unsigned i = 0; i < s->nb_streams; i++) {
                FFStream *const sti = ffstream(s->streams[i]);
                if (av_find_program_from_stream(s, NULL, i))
                    continue;
                sti->pts_wrap_reference = pts_wrap_reference;
                sti->pts_wrap_behavior  = pts_wrap_behavior;
            }
        } else {
            sti->pts_wrap_reference = default_sti->pts_wrap_reference;
            sti->pts_wrap_behavior  = default_sti->pts_wrap_behavior;
        }
    } else {
        AVProgram *program = first_program;
        while (program) {
            if (program->pts_wrap_reference != AV_NOPTS_VALUE) {
                pts_wrap_reference = program->pts_wrap_reference;
                pts_wrap_behavior = program->pts_wrap_behavior;
                break;
            }
            program = av_find_program_from_stream(s, program, stream_index);
        }

        // update every program with differing pts_wrap_reference
        program = first_program;
        while (program) {
            if (program->pts_wrap_reference != pts_wrap_reference) {
                for (unsigned i = 0; i < program->nb_stream_indexes; i++) {
                    FFStream *const sti = ffstream(s->streams[program->stream_index[i]]);
                    sti->pts_wrap_reference = pts_wrap_reference;
                    sti->pts_wrap_behavior  = pts_wrap_behavior;
                }

                program->pts_wrap_reference = pts_wrap_reference;
                program->pts_wrap_behavior = pts_wrap_behavior;
            }
            program = av_find_program_from_stream(s, program, stream_index);
        }
    }
    return 1;
}

static void update_timestamps(AVFormatContext *s, AVStream *st, AVPacket *pkt)
{
    FFStream *const sti = ffstream(st);

    if (update_wrap_reference(s, st, pkt->stream_index, pkt) && sti->pts_wrap_behavior == AV_PTS_WRAP_SUB_OFFSET) {
        // correct first time stamps to negative values
        if (!is_relative(sti->first_dts))
            sti->first_dts = wrap_timestamp(st, sti->first_dts);
        if (!is_relative(st->start_time))
            st->start_time = wrap_timestamp(st, st->start_time);
        if (!is_relative(sti->cur_dts))
            sti->cur_dts = wrap_timestamp(st, sti->cur_dts);
    }

    pkt->dts = wrap_timestamp(st, pkt->dts);
    pkt->pts = wrap_timestamp(st, pkt->pts);

    force_codec_ids(s, st);

    /* TODO: audio: time filter; video: frame reordering (pts != dts) */
    if (s->use_wallclock_as_timestamps)
        pkt->dts = pkt->pts = av_rescale_q(av_gettime(), AV_TIME_BASE_Q, st->time_base);
}

/**
 * Handle a new packet and either return it directly if possible and
 * allow_passthrough is true or queue the packet (or drop the packet
 * if corrupt).
 *
 * @return < 0 on error, 0 if the packet was passed through,
 *         1 if it was queued or dropped
 */
static int handle_new_packet(AVFormatContext *s, AVPacket *pkt, int allow_passthrough)
{
    FormatContextInternal *const fci = ff_fc_internal(s);
    AVStream *st;
    FFStream *sti;
    int err;

    av_assert0(pkt->stream_index < (unsigned)s->nb_streams &&
               "Invalid stream index.\n");

    if (pkt->flags & AV_PKT_FLAG_CORRUPT) {
        av_log(s, AV_LOG_WARNING,
               "Packet corrupt (stream = %d, dts = %s)%s.\n",
               pkt->stream_index, av_ts2str(pkt->dts),
               s->flags & AVFMT_FLAG_DISCARD_CORRUPT ? ", dropping it" : "");
        if (s->flags & AVFMT_FLAG_DISCARD_CORRUPT) {
            av_packet_unref(pkt);
            return 1;
        }
    }

    st  = s->streams[pkt->stream_index];
    sti = ffstream(st);

    update_timestamps(s, st, pkt);

    if (sti->request_probe <= 0 && allow_passthrough && !fci->raw_packet_buffer.head)
        return 0;

    err = avpriv_packet_list_put(&fci->raw_packet_buffer, pkt, NULL, 0);
    if (err < 0) {
        av_packet_unref(pkt);
        return err;
    }

    pkt = &fci->raw_packet_buffer.tail->pkt;
    fci->raw_packet_buffer_size += pkt->size;

    err = probe_codec(s, st, pkt);
    if (err < 0)
        return err;

    return 1;
}

int ff_buffer_packet(AVFormatContext *s, AVPacket *pkt)
{
    int err = handle_new_packet(s, pkt, 0);

    return err < 0 ? err : 0;
}

int ff_read_packet(AVFormatContext *s, AVPacket *pkt)
{
    FormatContextInternal *const fci = ff_fc_internal(s);
    int err;

#if FF_API_INIT_PACKET
FF_DISABLE_DEPRECATION_WARNINGS
    pkt->data = NULL;
    pkt->size = 0;
    av_init_packet(pkt);
FF_ENABLE_DEPRECATION_WARNINGS
#else
    av_packet_unref(pkt);
#endif

    for (;;) {
        PacketListEntry *pktl = fci->raw_packet_buffer.head;

        if (pktl) {
            AVStream *const st = s->streams[pktl->pkt.stream_index];
            if (fci->raw_packet_buffer_size >= s->probesize)
                if ((err = probe_codec(s, st, NULL)) < 0)
                    return err;
            if (ffstream(st)->request_probe <= 0) {
                avpriv_packet_list_get(&fci->raw_packet_buffer, pkt);
                fci->raw_packet_buffer_size -= pkt->size;
                return 0;
            }
        }

        err = ffifmt(s->iformat)->read_packet(s, pkt);
        if (err < 0) {
            av_packet_unref(pkt);

            /* Some demuxers return FFERROR_REDO when they consume
               data and discard it (ignored streams, junk, extradata).
               We must re-call the demuxer to get the real packet. */
            if (err == FFERROR_REDO)
                continue;
            if (!pktl || err == AVERROR(EAGAIN))
                return err;
            for (unsigned i = 0; i < s->nb_streams; i++) {
                AVStream *const st  = s->streams[i];
                FFStream *const sti = ffstream(st);
                if (sti->probe_packets || sti->request_probe > 0)
                    if ((err = probe_codec(s, st, NULL)) < 0)
                        return err;
                av_assert0(sti->request_probe <= 0);
            }
            continue;
        }

        err = av_packet_make_refcounted(pkt);
        if (err < 0) {
            av_packet_unref(pkt);
            return err;
        }

        err = handle_new_packet(s, pkt, 1);
        if (err <= 0) /* Error or passthrough */
            return err;
    }
}

/**
 * Return the frame duration in seconds. Return 0 if not available.
 */
static void compute_frame_duration(AVFormatContext *s, int *pnum, int *pden,
                                   AVStream *st, AVCodecParserContext *pc,
                                   AVPacket *pkt)
{
    FFStream *const sti = ffstream(st);
    AVRational codec_framerate = sti->avctx->framerate;
    int frame_size, sample_rate;

    *pnum = 0;
    *pden = 0;
    switch (st->codecpar->codec_type) {
    case AVMEDIA_TYPE_VIDEO:
        if (st->r_frame_rate.num && (!pc || !codec_framerate.num)) {
            *pnum = st->r_frame_rate.den;
            *pden = st->r_frame_rate.num;
        } else if ((s->iformat->flags & AVFMT_NOTIMESTAMPS) &&
                   !codec_framerate.num &&
                   st->avg_frame_rate.num && st->avg_frame_rate.den) {
            *pnum = st->avg_frame_rate.den;
            *pden = st->avg_frame_rate.num;
        } else if (st->time_base.num * 1000LL > st->time_base.den) {
            *pnum = st->time_base.num;
            *pden = st->time_base.den;
        } else if (codec_framerate.den * 1000LL > codec_framerate.num) {
            int ticks_per_frame = (sti->codec_desc &&
                                   (sti->codec_desc->props & AV_CODEC_PROP_FIELDS)) ? 2 : 1;
            av_reduce(pnum, pden,
                      codec_framerate.den,
                      codec_framerate.num * (int64_t)ticks_per_frame,
                      INT_MAX);

            if (pc && pc->repeat_pict) {
                av_reduce(pnum, pden,
                          (*pnum) * (1LL + pc->repeat_pict),
                          (*pden),
                          INT_MAX);
            }
            /* If this codec can be interlaced or progressive then we need
             * a parser to compute duration of a packet. Thus if we have
             * no parser in such case leave duration undefined. */
            if (sti->codec_desc &&
                (sti->codec_desc->props & AV_CODEC_PROP_FIELDS) && !pc)
                *pnum = *pden = 0;
        }
        break;
    case AVMEDIA_TYPE_AUDIO:
        if (sti->avctx_inited) {
            frame_size  = av_get_audio_frame_duration(sti->avctx, pkt->size);
            sample_rate = sti->avctx->sample_rate;
        } else {
            frame_size  = av_get_audio_frame_duration2(st->codecpar, pkt->size);
            sample_rate = st->codecpar->sample_rate;
        }
        if (frame_size <= 0 || sample_rate <= 0)
            break;
        *pnum = frame_size;
        *pden = sample_rate;
        break;
    default:
        break;
    }
}

static int has_decode_delay_been_guessed(AVStream *st)
{
    FFStream *const sti = ffstream(st);
    if (st->codecpar->codec_id != AV_CODEC_ID_H264) return 1;
    if (!sti->info) // if we have left find_stream_info then nb_decoded_frames won't increase anymore for stream copy
        return 1;
#if CONFIG_H264_DECODER
    if (sti->avctx->has_b_frames &&
        avpriv_h264_has_num_reorder_frames(sti->avctx) == sti->avctx->has_b_frames)
        return 1;
#endif
    if (sti->avctx->has_b_frames < 3)
        return sti->nb_decoded_frames >= 7;
    else if (sti->avctx->has_b_frames < 4)
        return sti->nb_decoded_frames >= 18;
    else
        return sti->nb_decoded_frames >= 20;
}

static PacketListEntry *get_next_pkt(AVFormatContext *s, AVStream *st,
                                     PacketListEntry *pktl)
{
    FormatContextInternal *const fci = ff_fc_internal(s);
    FFFormatContext *const si = &fci->fc;
    if (pktl->next)
        return pktl->next;
    if (pktl == si->packet_buffer.tail)
        return fci->parse_queue.head;
    return NULL;
}

static int64_t select_from_pts_buffer(AVStream *st, int64_t *pts_buffer, int64_t dts)
{
    FFStream *const sti = ffstream(st);
    int onein_oneout = st->codecpar->codec_id != AV_CODEC_ID_H264 &&
                       st->codecpar->codec_id != AV_CODEC_ID_HEVC &&
                       st->codecpar->codec_id != AV_CODEC_ID_VVC;

    if (!onein_oneout) {
        int delay = sti->avctx->has_b_frames;

        if (dts == AV_NOPTS_VALUE) {
            int64_t best_score = INT64_MAX;
            for (int i = 0; i < delay; i++) {
                if (sti->pts_reorder_error_count[i]) {
                    int64_t score = sti->pts_reorder_error[i] / sti->pts_reorder_error_count[i];
                    if (score < best_score) {
                        best_score = score;
                        dts = pts_buffer[i];
                    }
                }
            }
        } else {
            for (int i = 0; i < delay; i++) {
                if (pts_buffer[i] != AV_NOPTS_VALUE) {
                    int64_t diff = FFABS(pts_buffer[i] - dts)
                                   + (uint64_t)sti->pts_reorder_error[i];
                    diff = FFMAX(diff, sti->pts_reorder_error[i]);
                    sti->pts_reorder_error[i] = diff;
                    sti->pts_reorder_error_count[i]++;
                    if (sti->pts_reorder_error_count[i] > 250) {
                        sti->pts_reorder_error[i] >>= 1;
                        sti->pts_reorder_error_count[i] >>= 1;
                    }
                }
            }
        }
    }

    if (dts == AV_NOPTS_VALUE)
        dts = pts_buffer[0];

    return dts;
}

/**
 * Updates the dts of packets of a stream in pkt_buffer, by re-ordering the pts
 * of the packets in a window.
 */
static void update_dts_from_pts(AVFormatContext *s, int stream_index,
                                PacketListEntry *pkt_buffer)
{
    AVStream *const st = s->streams[stream_index];
    int delay = ffstream(st)->avctx->has_b_frames;

    int64_t pts_buffer[MAX_REORDER_DELAY+1];

    for (int i = 0; i < MAX_REORDER_DELAY + 1; i++)
        pts_buffer[i] = AV_NOPTS_VALUE;

    for (; pkt_buffer; pkt_buffer = get_next_pkt(s, st, pkt_buffer)) {
        if (pkt_buffer->pkt.stream_index != stream_index)
            continue;

        if (pkt_buffer->pkt.pts != AV_NOPTS_VALUE && delay <= MAX_REORDER_DELAY) {
            pts_buffer[0] = pkt_buffer->pkt.pts;
            for (int i = 0; i < delay && pts_buffer[i] > pts_buffer[i + 1]; i++)
                FFSWAP(int64_t, pts_buffer[i], pts_buffer[i + 1]);

            pkt_buffer->pkt.dts = select_from_pts_buffer(st, pts_buffer, pkt_buffer->pkt.dts);
        }
    }
}

static void update_initial_timestamps(AVFormatContext *s, int stream_index,
                                      int64_t dts, int64_t pts, AVPacket *pkt)
{
    FormatContextInternal *const fci = ff_fc_internal(s);
    FFFormatContext *const si = &fci->fc;
    AVStream *const st  = s->streams[stream_index];
    FFStream *const sti = ffstream(st);
    PacketListEntry *pktl = si->packet_buffer.head ? si->packet_buffer.head : fci->parse_queue.head;

    uint64_t shift;

    if (sti->first_dts != AV_NOPTS_VALUE ||
        dts           == AV_NOPTS_VALUE ||
        sti->cur_dts   == AV_NOPTS_VALUE ||
        sti->cur_dts < INT_MIN + RELATIVE_TS_BASE ||
        dts  < INT_MIN + (sti->cur_dts - RELATIVE_TS_BASE) ||
        is_relative(dts))
        return;

    sti->first_dts = dts - (sti->cur_dts - RELATIVE_TS_BASE);
    sti->cur_dts
