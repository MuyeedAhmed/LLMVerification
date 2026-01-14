Do not include anything else.

----- BEGIN modified.c -----
/*
 * RTSP/SDP client
 * Copyright (c) 2002 Fabrice Bellard
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

#include "libavutil/base64.h"
#include "libavutil/avstring.h"
#include "libavutil/intreadwrite.h"
#include "libavutil/mathematics.h"
#include "libavutil/parseutils.h"
#include "libavutil/random_seed.h"
#include "libavutil/dict.h"
#include "libavutil/opt.h"
#include "libavutil/time.h"
#include "avformat.h"
#include "avio_internal.h"

#if HAVE_POLL_H
#include <poll.h>
#endif
#include "internal.h"
#include "network.h"
#include "os_support.h"
#include "http.h"
#include "rtsp.h"

#include "rtpdec.h"
#include "rtpproto.h"
#include "rdt.h"
#include "rtpdec_formats.h"
#include "rtpenc_chain.h"
#include "url.h"
#include "rtpenc.h"
#include "mpegts.h"

/* Timeout values for socket poll, in ms,
 * and read_packet(), in seconds  */
#define POLL_TIMEOUT_MS 100
#define READ_PACKET_TIMEOUT_S 10
#define MAX_TIMEOUTS READ_PACKET_TIMEOUT_S * 1000 / POLL_TIMEOUT_MS
#define SDP_MAX_SIZE 16384
#define RECVBUF_SIZE 10 * RTP_MAX_PACKET_LENGTH
#define DEFAULT_REORDERING_DELAY 100000

#define OFFSET(x) offsetof(RTSPState, x)
#define DEC AV_OPT_FLAG_DECODING_PARAM
#define ENC AV_OPT_FLAG_ENCODING_PARAM

#define RTSP_FLAG_OPTS(name, longname) \
    { name, longname, OFFSET(rtsp_flags), AV_OPT_TYPE_FLAGS, {.i64 = 0}, INT_MIN, INT_MAX, DEC, "rtsp_flags" }, \
    { "filter_src", "Only receive packets from the negotiated peer IP", 0, AV_OPT_TYPE_CONST, {.i64 = RTSP_FLAG_FILTER_SRC}, 0, 0, DEC, "rtsp_flags" }

#define RTSP_MEDIATYPE_OPTS(name, longname) \
    { name, longname, OFFSET(media_type_mask), AV_OPT_TYPE_FLAGS, { .i64 = (1 << (AVMEDIA_TYPE_DATA+1)) - 1 }, INT_MIN, INT_MAX, DEC, "allowed_media_types" }, \
    { "video", "Video", 0, AV_OPT_TYPE_CONST, {.i64 = 1 << AVMEDIA_TYPE_VIDEO}, 0, 0, DEC, "allowed_media_types" }, \
    { "audio", "Audio", 0, AV_OPT_TYPE_CONST, {.i64 = 1 << AVMEDIA_TYPE_AUDIO}, 0, 0, DEC, "allowed_media_types" }, \
    { "data", "Data", 0, AV_OPT_TYPE_CONST, {.i64 = 1 << AVMEDIA_TYPE_DATA}, 0, 0, DEC, "allowed_media_types" }

#define RTSP_REORDERING_OPTS() \
    { "reorder_queue_size", "Number of packets to buffer for handling of reordered packets", OFFSET(reordering_queue_size), AV_OPT_TYPE_INT, { .i64 = -1 }, -1, INT_MAX, DEC }

const AVOption ff_rtsp_options[] = {
    { "initial_pause",  "Don't start playing the stream immediately", OFFSET(initial_pause), AV_OPT_TYPE_INT, {.i64 = 0}, 0, 1, DEC },
    FF_RTP_FLAG_OPTS(RTSPState, rtp_muxer_flags),
    { "rtsp_transport", "RTSP transport protocols", OFFSET(lower_transport_mask), AV_OPT_TYPE_FLAGS, {.i64 = 0}, INT_MIN, INT_MAX, DEC|ENC, "rtsp_transport" }, \
    { "udp", "UDP", 0, AV_OPT_TYPE_CONST, {.i64 = 1 << RTSP_LOWER_TRANSPORT_UDP}, 0, 0, DEC|ENC, "rtsp_transport" }, \
    { "tcp", "TCP", 0, AV_OPT_TYPE_CONST, {.i64 = 1 << RTSP_LOWER_TRANSPORT_TCP}, 0, 0, DEC|ENC, "rtsp_transport" }, \
    { "udp_multicast", "UDP multicast", 0, AV_OPT_TYPE_CONST, {.i64 = 1 << RTSP_LOWER_TRANSPORT_UDP_MULTICAST}, 0, 0, DEC, "rtsp_transport" },
    { "http", "HTTP tunneling", 0, AV_OPT_TYPE_CONST, {.i64 = (1 << RTSP_LOWER_TRANSPORT_HTTP)}, 0, 0, DEC, "rtsp_transport" },
    RTSP_FLAG_OPTS("rtsp_flags", "RTSP flags"),
    { "listen", "Wait for incoming connections", 0, AV_OPT_TYPE_CONST, {.i64 = RTSP_FLAG_LISTEN}, 0, 0, DEC, "rtsp_flags" },
    RTSP_MEDIATYPE_OPTS("allowed_media_types", "Media types to accept from the server"),
    { "min_port", "Minimum local UDP port", OFFSET(rtp_port_min), AV_OPT_TYPE_INT, {.i64 = RTSP_RTP_PORT_MIN}, 0, 65535, DEC|ENC },
    { "max_port", "Maximum local UDP port", OFFSET(rtp_port_max), AV_OPT_TYPE_INT, {.i64 = RTSP_RTP_PORT_MAX}, 0, 65535, DEC|ENC },
    { "timeout", "Maximum timeout (in seconds) to wait for incoming connections. -1 is infinite. Implies flag listen", OFFSET(initial_timeout), AV_OPT_TYPE_INT, {.i64 = -1}, INT_MIN, INT_MAX, DEC },
    RTSP_REORDERING_OPTS(),
    { NULL },
};

static const AVOption sdp_options[] = {
    RTSP_FLAG_OPTS("sdp_flags", "SDP flags"),
    { "custom_io", "Use custom IO", 0, AV_OPT_TYPE_CONST, {.i64 = RTSP_FLAG_CUSTOM_IO}, 0, 0, DEC, "rtsp_flags" },
    { "rtcp_to_source", "Send RTCP packets to the source address of received packets", 0, AV_OPT_TYPE_CONST, {.i64 = RTSP_FLAG_RTCP_TO_SOURCE}, 0, 0, DEC, "rtsp_flags" },
    RTSP_MEDIATYPE_OPTS("allowed_media_types", "Media types to accept from the server"),
    RTSP_REORDERING_OPTS(),
    { NULL },
};

static const AVOption rtp_options[] = {
    RTSP_FLAG_OPTS("rtp_flags", "RTP flags"),
    RTSP_REORDERING_OPTS(),
    { NULL },
};

static void get_word_until_chars(char *buf, int buf_size,
                                 const char *sep, const char **pp)
{
    const char *p;
    char *q;

    p = *pp;
    p += strspn(p, SPACE_CHARS);
    q = buf;
    while (!strchr(sep, *p) && *p != '\0') {
        if ((q - buf) < buf_size - 1)
            *q++ = *p;
        p++;
    }
    if (buf_size > 0)
        *q = '\0';
    *pp = p;
}

static void get_word_sep(char *buf, int buf_size, const char *sep,
                         const char **pp)
{
    if (**pp == '/') (*pp)++;
    get_word_until_chars(buf, buf_size, sep, pp);
}

static void get_word(char *buf, int buf_size, const char **pp)
{
    get_word_until_chars(buf, buf_size, SPACE_CHARS, pp);
}

/** Parse a string p in the form of Range:npt=xx-xx, and determine the start
 *  and end time.
 *  Used for seeking in the rtp stream.
 */
static void rtsp_parse_range_npt(const char *p, int64_t *start, int64_t *end)
{
    char buf[256];

    p += strspn(p, SPACE_CHARS);
    if (!av_stristart(p, "npt=", &p))
        return;

    *start = AV_NOPTS_VALUE;
    *end = AV_NOPTS_VALUE;

    get_word_sep(buf, sizeof(buf), "-", &p);
    av_parse_time(start, buf, 1);
    if (*p == '-') {
        p++;
        get_word_sep(buf, sizeof(buf), "-", &p);
        av_parse_time(end, buf, 1);
    }
}

static int get_sockaddr(const char *buf, struct sockaddr_storage *sock)
{
    struct addrinfo hints = { 0 }, *ai = NULL;
    hints.ai_flags = AI_NUMERICHOST;
    if (getaddrinfo(buf, NULL, &hints, &ai))
        return -1;
    memcpy(sock, ai->ai_addr, FFMIN(sizeof(*sock), ai->ai_addrlen));
    freeaddrinfo(ai);
    return 0;
}

#if CONFIG_RTPDEC
static void init_rtp_handler(RTPDynamicProtocolHandler *handler,
                             RTSPStream *rtsp_st, AVCodecContext *codec)
{
    if (!handler)
        return;
    if (codec)
        codec->codec_id          = handler->codec_id;
    rtsp_st->dynamic_handler = handler;
    if (handler->alloc) {
        rtsp_st->dynamic_protocol_context = handler->alloc();
        if (!rtsp_st->dynamic_protocol_context)
            rtsp_st->dynamic_handler = NULL;
    }
}

/* parse the rtpmap description: <codec_name>/<clock_rate>[/<other params>] */
static int sdp_parse_rtpmap(AVFormatContext *s,
                            AVStream *st, RTSPStream *rtsp_st,
                            int payload_type, const char *p)
{
    AVCodecContext *codec = st->codec;
    char buf[256];
    int i;
    AVCodec *c;
    const char *c_name;

    /* See if we can handle this kind of payload.
     * The space should normally not be there but some Real streams or
     * particular servers ("RealServer Version 6.1.3.970", see issue 1658)
     * have a trailing space. */
    get_word_sep(buf, sizeof(buf), "/ ", &p);
    if (payload_type < RTP_PT_PRIVATE) {
        /* We are in a standard case
         * (from http://www.iana.org/assignments/rtp-parameters). */
        codec->codec_id = ff_rtp_codec_id(buf, codec->codec_type);
    }

    if (codec->codec_id == AV_CODEC_ID_NONE) {
        RTPDynamicProtocolHandler *handler =
            ff_rtp_handler_find_by_name(buf, codec->codec_type);
        init_rtp_handler(handler, rtsp_st, codec);
        /* If no dynamic handler was found, check with the list of standard
         * allocated types, if such a stream for some reason happens to
         * use a private payload type. This isn't handled in rtpdec.c, since
         * the format name from the rtpmap line never is passed into rtpdec. */
        if (!rtsp_st->dynamic_handler)
            codec->codec_id = ff_rtp_codec_id(buf, codec->codec_type);
    }

    c = avcodec_find_decoder(codec->codec_id);
    if (c && c->name)
        c_name = c->name;
    else
        c_name = "(null)";

    get_word_sep(buf, sizeof(buf), "/", &p);
    i = atoi(buf);
    switch (codec->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
        av_log(s, AV_LOG_DEBUG, "audio codec set to: %s\n", c_name);
        codec->sample_rate = RTSP_DEFAULT_AUDIO_SAMPLERATE;
        codec->channels = RTSP_DEFAULT_NB_AUDIO_CHANNELS;
        if (i > 0) {
            codec->sample_rate = i;
            avpriv_set_pts_info(st, 32, 1, codec->sample_rate);
            get_word_sep(buf, sizeof(buf), "/", &p);
            i = atoi(buf);
            if (i > 0)
                codec->channels = i;
        }
        av_log(s, AV_LOG_DEBUG, "audio samplerate set to: %i\n",
               codec->sample_rate);
        av_log(s, AV_LOG_DEBUG, "audio channels set to: %i\n",
               codec->channels);
        break;
    case AVMEDIA_TYPE_VIDEO:
        av_log(s, AV_LOG_DEBUG, "video codec set to: %s\n", c_name);
        if (i > 0)
            avpriv_set_pts_info(st, 32, 1, i);
        break;
    default:
        break;
    }
    if (rtsp_st->dynamic_handler && rtsp_st->dynamic_handler->init)
        rtsp_st->dynamic_handler->init(s, st->index,
                                       rtsp_st->dynamic_protocol_context);
    return 0;
}

/* parse the attribute line from the fmtp a line of an sdp response. This
 * is broken out as a function because it is used in rtp_h264.c, which is
 * forthcoming. */
int ff_rtsp_next_attr_and_value(const char **p, char *attr, int attr_size,
                                char *value, int value_size)
{
    *p += strspn(*p, SPACE_CHARS);
    if (**p) {
        get_word_sep(attr, attr_size, "=", p);
        if (**p == '=')
            (*p)++;
        get_word_sep(value, value_size, ";", p);
        if (**p == ';')
            (*p)++;
        return 1;
    }
    return 0;
}

typedef struct SDPParseState {
    /* SDP only */
    struct sockaddr_storage default_ip;
    int            default_ttl;
    int            skip_media;  ///< set if an unknown m= line occurs
    int nb_default_include_source_addrs; /**< Number of source-specific multicast include source IP address (from SDP content) */
    struct RTSPSource **default_include_source_addrs; /**< Source-specific multicast include source IP address (from SDP content) */
    int nb_default_exclude_source_addrs; /**< Number of source-specific multicast exclude source IP address (from SDP content) */
    struct RTSPSource **default_exclude_source_addrs; /**< Source-specific multicast exclude source IP address (from SDP content) */
    int seen_rtpmap;
    int seen_fmtp;
    char delayed_fmtp[2048];
} SDPParseState;

static void copy_default_source_addrs(struct RTSPSource **addrs, int count,
                                      struct RTSPSource ***dest, int *dest_count)
{
    RTSPSource *rtsp_src, *rtsp_src2;
    int i;
    for (i = 0; i < count; i++) {
        rtsp_src = addrs[i];
        rtsp_src2 = av_malloc(sizeof(*rtsp_src2));
        if (!rtsp_src2)
            continue;
        memcpy(rtsp_src2, rtsp_src, sizeof(*rtsp_src));
        dynarray_add(dest, dest_count, rtsp_src2);
    }
}

static void parse_fmtp(AVFormatContext *s, RTSPState *rt,
                       int payload_type, const char *line)
{
    int i;

    for (i = 0; i < rt->nb_rtsp_streams; i++) {
        RTSPStream *rtsp_st = rt->rtsp_streams[i];
        if (rtsp_st->sdp_payload_type == payload_type &&
            rtsp_st->dynamic_handler &&
            rtsp_st->dynamic_handler->parse_sdp_a_line) {
            rtsp_st->dynamic_handler->parse_sdp_a_line(s, i,
            rtsp_st->dynamic_protocol_context, line);
        }
    }
}

static void sdp_parse_line(AVFormatContext *s, SDPParseState *s1,
                           int letter, const char *buf)
{
    RTSPState *rt = s->priv_data;
    char buf1[64], st_type[64];
    const char *p;
    enum AVMediaType codec_type;
    int payload_type;
    AVStream *st;
    RTSPStream *rtsp_st;
    RTSPSource *rtsp_src;
    struct sockaddr_storage sdp_ip;
    int ttl;

    av_dlog(s, "sdp: %c='%s'\n", letter, buf);

    p = buf;
    if (s1->skip_media && letter != 'm')
        return;
    switch (letter) {
    case 'c':
        get_word(buf1, sizeof(buf1), &p);
        if (strcmp(buf1, "IN") != 0)
            return;
        get_word(buf1, sizeof(buf1), &p);
        if (strcmp(buf1, "IP4") && strcmp(buf1, "IP6"))
            return;
        get_word_sep(buf1, sizeof(buf1), "/", &p);
        if (get_sockaddr(buf1, &sdp_ip))
            return;
        ttl = 16;
        if (*p == '/') {
            p++;
            get_word_sep(buf1, sizeof(buf1), "/", &p);
            ttl = atoi(buf1);
        }
        if (s->nb_streams == 0) {
            s1->default_ip = sdp_ip;
            s1->default_ttl = ttl;
        } else {
            rtsp_st = rt->rtsp_streams[rt->nb_rtsp_streams - 1];
            rtsp_st->sdp_ip = sdp_ip;
            rtsp_st->sdp_ttl = ttl;
        }
        break;
    case 's':
        av_dict_set(&s->metadata, "title", p, 0);
        break;
    case 'i':
        if (s->nb_streams == 0) {
            av_dict_set(&s->metadata, "comment", p, 0);
            break;
        }
        break;
    case 'm':
        /* new stream */
        s1->skip_media  = 0;
        s1->seen_fmtp   = 0;
        s1->seen_rtpmap = 0;
        codec_type = AVMEDIA_TYPE_UNKNOWN;
        get_word(st_type, sizeof(st_type), &p);
        if (!strcmp(st_type, "audio")) {
            codec_type = AVMEDIA_TYPE_AUDIO;
        } else if (!strcmp(st_type, "video")) {
            codec_type = AVMEDIA_TYPE_VIDEO;
        } else if (!strcmp(st_type, "application") || !strcmp(st_type, "text")) {
            codec_type = AVMEDIA_TYPE_DATA;
        }
        if (codec_type == AVMEDIA_TYPE_UNKNOWN || !(rt->media_type_mask & (1 << codec_type))) {
            s1->skip_media = 1;
            return;
        }
        rtsp_st = av_mallocz(sizeof(RTSPStream));
        if (!rtsp_st)
            return;
        rtsp_st->stream_index = -1;
        dynarray_add(&rt->rtsp_streams, &rt->nb_rtsp_streams, rtsp_st);

        rtsp_st->sdp_ip = s1->default_ip;
        rtsp_st->sdp_ttl = s1->default_ttl;

        copy_default_source_addrs(s1->default_include_source_addrs,
                                  s1->nb_default_include_source_addrs,
                                  &rtsp_st->include_source_addrs,
                                  &rtsp_st->nb_include_source_addrs);
        copy_default_source_addrs(s1->default_exclude_source_addrs,
                                  s1->nb_default_exclude_source_addrs,
                                  &rtsp_st->exclude_source_addrs,
                                  &rtsp_st->nb_exclude_source_addrs);

        get_word(buf1, sizeof(buf1), &p); /* port */
        rtsp_st->sdp_port = atoi(buf1);

        get_word(buf1, sizeof(buf1), &p); /* protocol */
        if (!strcmp(buf1, "udp"))
            rt->transport = RTSP_TRANSPORT_RAW;
        else if (strstr(buf1, "/AVPF") || strstr(buf1, "/SAVPF"))
            rtsp_st->feedback = 1;

        /* XXX: handle list of formats */
        get_word(buf1, sizeof(buf1), &p); /* format list */
        rtsp_st->sdp_payload_type = atoi(buf1);

        if (!strcmp(ff_rtp_enc_name(rtsp_st->sdp_payload_type), "MP2T")) {
            /* no corresponding stream */
            if (rt->transport == RTSP_TRANSPORT_RAW) {
                if (CONFIG_RTPDEC && !rt->ts)
                    rt->ts = ff_mpegts_parse_open(s);
            } else {
                RTPDynamicProtocolHandler *handler;
                handler = ff_rtp_handler_find_by_id(
                              rtsp_st->sdp_payload_type, AVMEDIA_TYPE_DATA);
                init_rtp_handler(handler, rtsp_st, NULL);
                if (handler && handler->init)
                    handler->init(s, -1, rtsp_st->dynamic_protocol_context);
            }
        } else if (rt->server_type == RTSP_SERVER_WMS &&
                   codec_type == AVMEDIA_TYPE_DATA) {
            /* RTX stream, a stream that carries all the other actual
             * audio/video streams. Don't expose this to the callers. */
        } else {
            st = avformat_new_stream(s, NULL);
            if (!st)
                return;
            st->id = rt->nb_rtsp_streams - 1;
            rtsp_st->stream_index = st->index;
            st->codec->codec_type = codec_type;
            if (rtsp_st->sdp_payload_type < RTP_PT_PRIVATE) {
                RTPDynamicProtocolHandler *handler;
                /* if standard payload type, we can find the codec right now */
                ff_rtp_get_codec_info(st->codec, rtsp_st->sdp_payload_type);
                if (st->codec->codec_type == AVMEDIA_TYPE_AUDIO &&
                    st->codec->sample_rate > 0)
                    avpriv_set_pts_info(st, 32, 1, st->codec->sample_rate);
                /* Even static payload types may need a custom depacketizer */
                handler = ff_rtp_handler_find_by_id(
                              rtsp_st->sdp_payload_type, st->codec->codec_type);
                init_rtp_handler(handler, rtsp_st, st->codec);
                if (handler && handler->init)
                    handler->init(s, st->index,
                                  rtsp_st->dynamic_protocol_context);
            }
            if (rt->default_lang[0])
                av_dict_set(&st->metadata, "language", rt->default_lang, 0);
        }
        /* put a default control url */
        av_strlcpy(rtsp_st->control_url, rt->control_uri,
                   sizeof(rtsp_st->control_url));
        break;
    case 'a':
        if (av_strstart(p, "control:", &p)) {
            if (s->nb_streams == 0) {
                if (!strncmp(p, "rtsp://", 7))
                    av_strlcpy(rt->control_uri, p,
                               sizeof(rt->control_uri));
            } else {
                char proto[32];
                /* get the control url */
                rtsp_st = rt->rtsp_streams[rt->nb_rtsp_streams - 1];

                /* XXX: may need to add full url resolution */
                av_url_split(proto, sizeof(proto), NULL, 0, NULL, 0,
                             NULL, NULL, 0, p);
                if (proto[0] == '\0') {
                    /* relative control URL */
                    if (rtsp_st->control_url[strlen(rtsp_st->control_url)-1]!='/')
                        av_strlcat(rtsp_st->control_url, "/",
                                   sizeof(rtsp_st->control_url));
                    av_strlcat(rtsp_st->control_url, p,
                               sizeof(rtsp_st->control_url));
                } else
                    av_strlcpy(rtsp_st->control_url, p,
                               sizeof(rtsp_st->control_url));
            }
        } else if (av_strstart(p, "rtpmap:", &p) && s->nb_streams > 0) {
            /* NOTE: rtpmap is only supported AFTER the 'm=' tag */
            get_word(buf1, sizeof(buf1), &p);
            payload_type = atoi(buf1);
            rtsp_st = rt->rtsp_streams[rt->nb_rtsp_streams - 1];
            if (rtsp_st->stream_index >= 0) {
                st = s->streams[rtsp_st->stream_index];
                sdp_parse_rtpmap(s, st, rtsp_st, payload_type, p);
            }
            s1->seen_rtpmap = 1;
            if (s1->seen_fmtp) {
                parse_fmtp(s, rt, payload_type, s1->delayed_fmtp);
            }
        } else if (av_strstart(p, "fmtp:", &p) ||
                   av_strstart(p, "framesize:", &p)) {
            // let dynamic protocol handlers have a stab at the line.
            get_word(buf1, sizeof(buf1), &p);
            payload_type = atoi(buf1);
            if (s1->seen_rtpmap) {
                parse_fmtp(s, rt, payload_type, buf);
            } else {
                s1->seen_fmtp = 1;
                av_strlcpy(s1->delayed_fmtp, buf, sizeof(s1->delayed_fmtp));
            }
        } else if (av_strstart(p, "range:", &p)) {
            int64_t start, end;

            // this is so that seeking on a streamed file can work.
            rtsp_parse_range_npt(p, &start, &end);
            s->start_time = start;
            /* AV_NOPTS_VALUE means live broadcast (and can't seek) */
            s->duration   = (end == AV_NOPTS_VALUE) ?
                            AV_NOPTS_VALUE : end - start;
        } else if (av_strstart(p, "lang:", &p)) {
            if (s->nb_streams > 0) {
                get_word(buf1, sizeof(buf1), &p);
                rtsp_st = rt->rtsp_streams[rt->nb_rtsp_streams - 1];
                if (rtsp_st->stream_index >= 0) {
                    st = s->streams[rtsp_st->stream_index];
                    av_dict_set(&st->metadata, "language", buf1, 0);
                }
            } else
                get_word(rt->default_lang, sizeof(rt->default_lang), &p);
        } else if (av_strstart(p, "IsRealDataType:integer;",&p)) {
            if (atoi(p) == 1)
                rt->transport = RTSP_TRANSPORT_RDT;
        } else if (av_strstart(p, "SampleRate:integer;", &p) &&
                   s->nb_streams > 0) {
            st = s->streams[s->nb_streams - 1];
            st->codec->sample_rate = atoi(p);
        } else if (av_strstart(p, "crypto:", &p) && s->nb_streams > 0) {
            // RFC 4568
            rtsp_st = rt->rtsp_streams[rt->nb_rtsp_streams - 1];
            get_word(buf1, sizeof(buf1), &p); // ignore tag
            get_word(rtsp_st->crypto_suite, sizeof(rtsp_st->crypto_suite), &p);
            p += strspn(p, SPACE_CHARS);
            if (av_strstart(p, "inline:", &p))
                get_word(rtsp_st->crypto_params, sizeof(rtsp_st->crypto_params), &p);
        } else if (av_strstart(p, "source-filter:", &p)) {
            int exclude = 0;
            get_word(buf1, sizeof(buf1), &p);
            if (strcmp(buf1, "incl") && strcmp(buf1, "excl"))
                return;
            exclude = !strcmp(buf1, "excl");

            get_word(buf1, sizeof(buf1), &p);
            if (strcmp(buf1, "IN") != 0)
                return;
            get_word(buf1, sizeof(buf1), &p);
            if (strcmp(buf1, "IP4") && strcmp(buf1, "IP6") && strcmp(buf1, "*"))
                return;
            // not checking that the destination address actually matches or is wildcard
            get_word(buf1, sizeof(buf1), &p);

            while (*p != '\0') {
                rtsp_src = av_mallocz(sizeof(*rtsp_src));
                if (!rtsp_src)
                    return;
                get_word(rtsp_src->addr, sizeof(rtsp_src->addr), &p);
                if (exclude) {
                    if (s->nb_streams == 0) {
                        dynarray_add(&s1->default_exclude_source_addrs, &s1->nb_default_exclude_source_addrs, rtsp_src);
                    } else {
                        rtsp_st = rt->rtsp_streams[rt->nb_rtsp_streams - 1];
                        dynarray_add(&rtsp_st->exclude_source_addrs, &rtsp_st->nb_exclude_source_addrs, rtsp_src);
                    }
                } else {
                    if (s->nb_streams == 0) {
                        dynarray_add(&s1->default_include_source_addrs, &s1->nb_default_include_source_addrs, rtsp_src);
                    } else {
                        rtsp_st = rt->rtsp_streams[rt->nb_rtsp_streams - 1];
                        dynarray_add(&rtsp_st->include_source_addrs, &rtsp_st->nb_include_source_addrs, rtsp_src);
                    }
                }
            }
        } else {
            if (rt->server_type == RTSP_SERVER_WMS)
                ff_wms_parse_sdp_a_line(s, p);
            if (s->nb_streams > 0) {
                rtsp_st = rt->rtsp_streams[rt->nb_rtsp_streams - 1];

                if (rt->server_type == RTSP_SERVER_REAL)
                    ff_real_parse_sdp_a_line(s, rtsp_st->stream_index, p);

                if (rtsp_st->dynamic_handler &&
                    rtsp_st->dynamic_handler->parse_sdp_a_line)
                    rtsp_st->dynamic_handler->parse_sdp_a_line(s,
                        rtsp_st->stream_index,
                        rtsp_st->dynamic_protocol_context, buf);
            }
        }
        break;
    }
}

int ff_sdp_parse(AVFormatContext *s, const char *content)
{
    RTSPState *rt = s->priv_data;
    const char *p;
    int letter, i;
    /* Some SDP lines, particularly for Realmedia or ASF RTSP streams,
     * contain long SDP lines containing complete ASF Headers (several
     * kB) or arrays of MDPR (RM stream descriptor) headers plus
     * "rulebooks" describing their properties. Therefore, the SDP line
     * buffer is large.
     *
     * The Vorbis FMTP line can be up to 16KB - see xiph_parse_sdp_line
     * in rtpdec_xiph.c. */
    char buf[16384], *q;
    SDPParseState sdp_parse_state = { { 0 } }, *s1 = &sdp_parse_state;

    p = content;
    for (;;) {
        p += strspn(p, SPACE_CHARS);
        letter = *p;
        if (letter == '\0')
            break;
        p++;
        if (*p != '=')
            goto next_line;
        p++;
        /* get the content */
        q = buf;
        while (*p != '\n' && *p != '\r' && *p != '\0') {
            if ((q - buf) < sizeof(buf) - 1)
                *q++ = *p;
            p++;
        }
        *q = '\0';
        sdp_parse_line(s, s1, letter, buf);
    next_line:
        while (*p != '\n' && *p != '\0')
            p++;
        if (*p == '\n')
            p++;
    }

    for (i = 0; i < s1->nb_default_include_source_addrs; i++)
        av_free(s1->default_include_source_addrs[i]);
    av_freep(&s1->default_include_source_addrs);
    for (i = 0; i < s1->nb_default_exclude_source_addrs; i++)
        av_free(s1->default_exclude_source_addrs[i]);
    av_freep(&s1->default_exclude_source_addrs);

    rt->p = av_malloc(sizeof(struct pollfd)*2*(rt->nb_rtsp_streams+1));
    if (!rt->p) return AVERROR(ENOMEM);
    return 0;
}
#endif /* CONFIG_RTPDEC */

void ff_rtsp_undo_setup(AVFormatContext *s, int send_packets)
{
    RTSPState *rt = s->priv_data;
    int i;

    for (i = 0; i < rt->nb_rtsp_streams; i++) {
        RTSPStream *rtsp_st = rt->rtsp_streams[i];
        if (!rtsp_st)
            continue;
        if (rtsp_st->transport_priv) {
            if (s->oformat) {
                AVFormatContext *rtpctx = rtsp_st->transport_priv;
                av_write_trailer(rtpctx);
                if (rt->lower_transport == RTSP_LOWER_TRANSPORT_TCP) {
                    uint8_t *ptr;
                    if (CONFIG_RTSP_MUXER && rtpctx->pb && send_packets)
                        ff_rtsp_tcp_write_packet(s, rtsp_st);
                    avio_close_dyn_buf(rtpctx->pb, &ptr);
                    av_free(ptr);
                } else {
                    avio_close(rtpctx->pb);
                }
                avformat_free_context(rtpctx);
            } else if (CONFIG_RTPDEC && rt->transport == RTSP_TRANSPORT_RDT)
                ff_rdt_parse_close(rtsp_st->transport_priv);
            else if (CONFIG_RTPDEC && rt->transport == RTSP_TRANSPORT_RTP)
                ff_rtp_parse_close(rtsp_st->transport_priv);
        }
        rtsp_st->transport_priv = NULL;
        if (rtsp_st->rtp_handle)
            ffurl_close(rtsp_st->rtp_handle);
        rtsp_st->rtp_handle = NULL;
    }
}

/* close and free RTSP streams */
void ff_rtsp_close_streams(AVFormatContext *s)
{
    RTSPState *rt = s->priv_data;
    int i, j;
    RTSPStream *rtsp_st;

    ff_rtsp_undo_setup(s, 0);
    for (i = 0; i < rt->nb_rtsp_streams; i++) {
        rtsp_st = rt->rtsp_streams[i];
        if (rtsp_st) {
            if (rtsp_st->dynamic_handler && rtsp_st->dynamic_protocol_context)
                rtsp_st->dynamic_handler->free(
                    rtsp_st->dynamic_protocol_context);
            for (j = 0; j < rtsp_st->nb_include_source_addrs; j++)
                av_free(rtsp_st->include_source_addrs[j]);
            av_freep(&rtsp_st->include_source_addrs);
            for (j = 0; j < rtsp_st->nb_exclude_source_addrs; j++)
                av_free(rtsp_st->exclude_source_addrs[j]);
            av_freep(&rtsp_st->exclude_source_addrs);

            av_free(rtsp_st);
        }
    }
    av_free(rt->rtsp_streams);
    if (rt->asf_ctx) {
        avformat_close_input(&rt->asf_ctx);
    }
    if (CONFIG_RTPDEC && rt->ts)
        ff_mpegts_parse_close(rt->ts);
    av_free(rt->p);
    av_free(rt->recvbuf);
}

int ff_rtsp_open_transport_ctx(AVFormatContext *s, RTSPStream *rtsp_st)
{
    RTSPState *rt = s->priv_data;
    AVStream *st = NULL;
    int reordering_queue_size = rt->reordering_queue_size;
    if (reordering_queue_size < 0) {
        if (rt->lower_transport == RTSP_LOWER_TRANSPORT_TCP || !s->max_delay)
            reordering_queue_size = 0;
        else
            reordering_queue_size = RTP_REORDER_QUEUE_DEFAULT_SIZE;
    }

    /* open the RTP context */
    if (rtsp_st->stream_index >= 0)
        st = s->streams[rtsp_st->stream_index];
    if (!st)
        s->ctx_flags |= AVFMTCTX_NOHEADER;

    if (CONFIG_RTSP_MUXER && s->oformat) {
        int ret = ff_rtp_chain_mux_open((AVFormatContext **)&rtsp_st->transport_priv,
                                        s, st, rtsp_st->rtp_handle,
                                        RTSP_TCP_MAX_PACKET_SIZE,
                                        rtsp_st->stream_index);
        /* Ownership of rtp_handle is passed to the rtp mux context */
        rtsp_st->rtp_handle = NULL;
        if (ret < 0)
            return ret
