Do not include anything else.

----- BEGIN modified.c -----
/*
 * RTMP network protocol
 * Copyright (c) 2009 Konstantin Shishkov
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

/**
 * @file
 * RTMP protocol
 */

#include "config_components.h"

#include "libavcodec/bytestream.h"
#include "libavutil/avstring.h"
#include "libavutil/base64.h"
#include "libavutil/intfloat.h"
#include "libavutil/lfg.h"
#include "libavutil/md5.h"
#include "libavutil/mem.h"
#include "libavutil/opt.h"
#include "libavutil/random_seed.h"
#include "avformat.h"
#include "internal.h"

#include "network.h"

#include "flv.h"
#include "rtmp.h"
#include "rtmpcrypt.h"
#include "rtmppkt.h"
#include "url.h"
#include "version.h"

#if CONFIG_ZLIB
#include <zlib.h>
#endif

#define APP_MAX_LENGTH 1024
#define TCURL_MAX_LENGTH 1024
#define FLASHVER_MAX_LENGTH 64
#define RTMP_PKTDATA_DEFAULT_SIZE 4096
#define RTMP_HEADER 11

/** RTMP protocol handler state */
typedef enum {
    STATE_START,      ///< client has not done anything yet
    STATE_HANDSHAKED, ///< client has performed handshake
    STATE_FCPUBLISH,  ///< client FCPublishing stream (for output)
    STATE_PLAYING,    ///< client has started receiving multimedia data from server
    STATE_SEEKING,    ///< client has started the seek operation. Back on STATE_PLAYING when the time comes
    STATE_PUBLISHING, ///< client has started sending multimedia data to server (for output)
    STATE_RECEIVING,  ///< received a publish command (for input)
    STATE_SENDING,    ///< received a play command (for output)
    STATE_STOPPED,    ///< the broadcast has been stopped
} ClientState;

typedef struct TrackedMethod {
    char *name;
    int id;
} TrackedMethod;

/** protocol handler context */
typedef struct RTMPContext {
    const AVClass *class;
    URLContext*   stream;                     ///< TCP stream used in interactions with RTMP server
    RTMPPacket    *prev_pkt[2];               ///< packet history used when reading and sending packets ([0] for reading, [1] for writing)
    int           nb_prev_pkt[2];             ///< number of elements in prev_pkt
    int           in_chunk_size;              ///< size of the chunks incoming RTMP packets are divided into
    int           out_chunk_size;             ///< size of the chunks outgoing RTMP packets are divided into
    int           is_input;                   ///< input/output flag
    char          *playpath;                  ///< stream identifier to play (with possible "mp4:" prefix)
    int           live;                       ///< 0: recorded, -1: live, -2: both
    char          *app;                       ///< name of application
    char          *conn;                      ///< append arbitrary AMF data to the Connect message
    ClientState   state;                      ///< current state
    int           stream_id;                  ///< ID assigned by the server for the stream
    uint8_t*      flv_data;                   ///< buffer with data for demuxer
    int           flv_size;                   ///< current buffer size
    int           flv_off;                    ///< number of bytes read from current buffer
    int           flv_nb_packets;             ///< number of flv packets published
    RTMPPacket    out_pkt;                    ///< rtmp packet, created from flv a/v or metadata (for output)
    uint32_t      receive_report_size;        ///< number of bytes after which we should report the number of received bytes to the peer
    uint64_t      bytes_read;                 ///< number of bytes read from server
    uint64_t      last_bytes_read;            ///< number of bytes read last reported to server
    uint32_t      last_timestamp;             ///< last timestamp received in a packet
    int           skip_bytes;                 ///< number of bytes to skip from the input FLV stream in the next write call
    int           has_audio;                  ///< presence of audio data
    int           has_video;                  ///< presence of video data
    int           received_metadata;          ///< Indicates if we have received metadata about the streams
    uint8_t       flv_header[RTMP_HEADER];    ///< partial incoming flv packet header
    int           flv_header_bytes;           ///< number of initialized bytes in flv_header
    int           nb_invokes;                 ///< keeps track of invoke messages
    char*         tcurl;                      ///< url of the target stream
    char*         flashver;                   ///< version of the flash plugin
    char*         swfhash;                    ///< SHA256 hash of the decompressed SWF file (32 bytes)
    int           swfhash_len;                ///< length of the SHA256 hash
    int           swfsize;                    ///< size of the decompressed SWF file
    char*         swfurl;                     ///< url of the swf player
    char*         swfverify;                  ///< URL to player swf file, compute hash/size automatically
    char          swfverification[42];        ///< hash of the SWF verification
    char*         pageurl;                    ///< url of the web page
    char*         subscribe;                  ///< name of live stream to subscribe
    int           max_sent_unacked;           ///< max unacked sent bytes
    int           client_buffer_time;         ///< client buffer time in ms
    int           flush_interval;             ///< number of packets flushed in the same request (RTMPT only)
    int           encrypted;                  ///< use an encrypted connection (RTMPE only)
    TrackedMethod*tracked_methods;            ///< tracked methods buffer
    int           nb_tracked_methods;         ///< number of tracked methods
    int           tracked_methods_size;       ///< size of the tracked methods buffer
    int           listen;                     ///< listen mode flag
    int           listen_timeout;             ///< listen timeout to wait for new connections
    int           nb_streamid;                ///< The next stream id to return on createStream calls
    double        duration;                   ///< Duration of the stream in seconds as returned by the server (only valid if non-zero)
    int           tcp_nodelay;                ///< Use TCP_NODELAY to disable Nagle's algorithm if set to 1
    char          *enhanced_codecs;           ///< codec list in enhanced rtmp
    char          username[50];
    char          password[50];
    char          auth_params[500];
    int           do_reconnect;
    int           auth_tried;
} RTMPContext;

#define PLAYER_KEY_OPEN_PART_LEN 30   ///< length of partial key used for first client digest signing
/** Client key used for digest signing */
static const uint8_t rtmp_player_key[] = {
    'G', 'e', 'n', 'u', 'i', 'n', 'e', ' ', 'A', 'd', 'o', 'b', 'e', ' ',
    'F', 'l', 'a', 's', 'h', ' ', 'P', 'l', 'a', 'y', 'e', 'r', ' ', '0', '0', '1',

    0xF0, 0xEE, 0xC2, 0x4A, 0x80, 0x68, 0xBE, 0xE8, 0x2E, 0x00, 0xD0, 0xD1, 0x02,
    0x9E, 0x7E, 0x57, 0x6E, 0xEC, 0x5D, 0x2D, 0x29, 0x80, 0x6F, 0xAB, 0x93, 0xB8,
    0xE6, 0x36, 0xCF, 0xEB, 0x31, 0xAE
};

#define SERVER_KEY_OPEN_PART_LEN 36   ///< length of partial key used for first server digest signing
/** Key used for RTMP server digest signing */
static const uint8_t rtmp_server_key[] = {
    'G', 'e', 'n', 'u', 'i', 'n', 'e', ' ', 'A', 'd', 'o', 'b', 'e', ' ',
    'F', 'l', 'a', 's', 'h', ' ', 'M', 'e', 'd', 'i', 'a', ' ',
    'S', 'e', 'r', 'v', 'e', 'r', ' ', '0', '0', '1',

    0xF0, 0xEE, 0xC2, 0x4A, 0x80, 0x68, 0xBE, 0xE8, 0x2E, 0x00, 0xD0, 0xD1, 0x02,
    0x9E, 0x7E, 0x57, 0x6E, 0xEC, 0x5D, 0x2D, 0x29, 0x80, 0x6F, 0xAB, 0x93, 0xB8,
    0xE6, 0x36, 0xCF, 0xEB, 0x31, 0xAE
};

static int handle_chunk_size(URLContext *s, RTMPPacket *pkt);
static int handle_window_ack_size(URLContext *s, RTMPPacket *pkt);
static int handle_set_peer_bw(URLContext *s, RTMPPacket *pkt);

static int add_tracked_method(RTMPContext *rt, const char *name, int id)
{
    int err;

    if (rt->nb_tracked_methods + 1 > rt->tracked_methods_size) {
        rt->tracked_methods_size = (rt->nb_tracked_methods + 1) * 2;
        if ((err = av_reallocp_array(&rt->tracked_methods, rt->tracked_methods_size,
                               sizeof(*rt->tracked_methods))) < 0) {
            rt->nb_tracked_methods = 0;
            rt->tracked_methods_size = 0;
            return err;
        }
    }

    rt->tracked_methods[rt->nb_tracked_methods].name = av_strdup(name);
    if (!rt->tracked_methods[rt->nb_tracked_methods].name)
        return AVERROR(ENOMEM);
    rt->tracked_methods[rt->nb_tracked_methods].id = id;
    rt->nb_tracked_methods++;

    return 0;
}

static void del_tracked_method(RTMPContext *rt, int index)
{
    memmove(&rt->tracked_methods[index], &rt->tracked_methods[index + 1],
            sizeof(*rt->tracked_methods) * (rt->nb_tracked_methods - index - 1));
    rt->nb_tracked_methods--;
}

static int find_tracked_method(URLContext *s, RTMPPacket *pkt, int offset,
                               char **tracked_method)
{
    RTMPContext *rt = s->priv_data;
    GetByteContext gbc;
    double pkt_id;
    int ret;
    int i;

    bytestream2_init(&gbc, pkt->data + offset, pkt->size - offset);
    if ((ret = ff_amf_read_number(&gbc, &pkt_id)) < 0)
        return ret;

    for (i = 0; i < rt->nb_tracked_methods; i++) {
        if (rt->tracked_methods[i].id != pkt_id)
            continue;

        *tracked_method = rt->tracked_methods[i].name;
        del_tracked_method(rt, i);
        break;
    }

    return 0;
}

static void free_tracked_methods(RTMPContext *rt)
{
    int i;

    for (i = 0; i < rt->nb_tracked_methods; i ++)
        av_freep(&rt->tracked_methods[i].name);
    av_freep(&rt->tracked_methods);
    rt->tracked_methods_size = 0;
    rt->nb_tracked_methods   = 0;
}

static int rtmp_send_packet(RTMPContext *rt, RTMPPacket *pkt, int track)
{
    int ret;

    if (pkt->type == RTMP_PT_INVOKE && track) {
        GetByteContext gbc;
        char name[128];
        double pkt_id;
        int len;

        bytestream2_init(&gbc, pkt->data, pkt->size);
        if ((ret = ff_amf_read_string(&gbc, name, sizeof(name), &len)) < 0)
            goto fail;

        if ((ret = ff_amf_read_number(&gbc, &pkt_id)) < 0)
            goto fail;

        if ((ret = add_tracked_method(rt, name, pkt_id)) < 0)
            goto fail;
    }

    ret = ff_rtmp_packet_write(rt->stream, pkt, rt->out_chunk_size,
                               &rt->prev_pkt[1], &rt->nb_prev_pkt[1]);
fail:
    ff_rtmp_packet_destroy(pkt);
    return ret;
}

static int rtmp_write_amf_data(URLContext *s, char *param, uint8_t **p)
{
    char *field, *value;
    char type;

    /* The type must be B for Boolean, N for number, S for string, O for
     * object, or Z for null. For Booleans the data must be either 0 or 1 for
     * FALSE or TRUE, respectively. Likewise for Objects the data must be
     * 0 or 1 to end or begin an object, respectively. Data items in subobjects
     * may be named, by prefixing the type with 'N' and specifying the name
     * before the value (ie. NB:myFlag:1). This option may be used multiple times
     * to construct arbitrary AMF sequences. */
    if (param[0] && param[1] == ':') {
        type = param[0];
        value = param + 2;
    } else if (param[0] == 'N' && param[1] && param[2] == ':') {
        type = param[1];
        field = param + 3;
        value = strchr(field, ':');
        if (!value)
            goto fail;
        *value = '\0';
        value++;

        ff_amf_write_field_name(p, field);
    } else {
        goto fail;
    }

    switch (type) {
    case 'B':
        ff_amf_write_bool(p, value[0] != '0');
        break;
    case 'S':
        ff_amf_write_string(p, value);
        break;
    case 'N':
        ff_amf_write_number(p, strtod(value, NULL));
        break;
    case 'Z':
        ff_amf_write_null(p);
        break;
    case 'O':
        if (value[0] != '0')
            ff_amf_write_object_start(p);
        else
            ff_amf_write_object_end(p);
        break;
    default:
        goto fail;
        break;
    }

    return 0;

fail:
    av_log(s, AV_LOG_ERROR, "Invalid AMF parameter: %s\n", param);
    return AVERROR(EINVAL);
}

/**
 * Generate 'connect' call and send it to the server.
 */
static int gen_connect(URLContext *s, RTMPContext *rt)
{
    RTMPPacket pkt;
    uint8_t *p;
    int ret;

    if ((ret = ff_rtmp_packet_create(&pkt, RTMP_SYSTEM_CHANNEL, RTMP_PT_INVOKE,
                                     0, 4096 + APP_MAX_LENGTH)) < 0)
        return ret;

    p = pkt.data;

    ff_amf_write_string(&p, "connect");
    ff_amf_write_number(&p, ++rt->nb_invokes);
    ff_amf_write_object_start(&p);
    ff_amf_write_field_name(&p, "app");
    ff_amf_write_string2(&p, rt->app, rt->auth_params);

    if (rt->enhanced_codecs) {
        uint32_t list_len = 0;
        char *fourcc_data = rt->enhanced_codecs;
        int fourcc_str_len = strlen(fourcc_data);

        // check the string, fourcc + ',' + ...  + end fourcc correct length should be (4+1)*n+4
        if ((fourcc_str_len + 1) % 5 != 0) {
            av_log(s, AV_LOG_ERROR, "Malformed rtmp_enhanched_codecs, "
                   "should be of the form hvc1[,av01][,vp09][,...]\n");
            return AVERROR(EINVAL);
        }

        list_len = (fourcc_str_len + 1) / 5;
        ff_amf_write_field_name(&p, "fourCcList");
        ff_amf_write_array_start(&p, list_len);

        while(fourcc_data - rt->enhanced_codecs < fourcc_str_len) {
            unsigned char fourcc[5];
            if (!strncmp(fourcc_data, "ac-3", 4) ||
                !strncmp(fourcc_data, "av01", 4) ||
                !strncmp(fourcc_data, "avc1", 4) ||
                !strncmp(fourcc_data, "ec-3", 4) ||
                !strncmp(fourcc_data, "fLaC", 4) ||
                !strncmp(fourcc_data, "hvc1", 4) ||
                !strncmp(fourcc_data, ".mp3", 4) ||
                !strncmp(fourcc_data, "mp4a", 4) ||
                !strncmp(fourcc_data, "Opus", 4) ||
                !strncmp(fourcc_data, "vp09", 4)) {
                    av_strlcpy(fourcc, fourcc_data, sizeof(fourcc));
                    ff_amf_write_string(&p, fourcc);
            } else {
                    av_log(s, AV_LOG_ERROR, "Unsupported codec fourcc, %.*s\n", 4, fourcc_data);
                    return AVERROR_PATCHWELCOME;
            }

            fourcc_data += 5;
        }
    }

    if (!rt->is_input) {
        ff_amf_write_field_name(&p, "type");
        ff_amf_write_string(&p, "nonprivate");
    }
    ff_amf_write_field_name(&p, "flashVer");
    ff_amf_write_string(&p, rt->flashver);

    if (rt->swfurl || rt->swfverify) {
        ff_amf_write_field_name(&p, "swfUrl");
        if (rt->swfurl)
            ff_amf_write_string(&p, rt->swfurl);
        else
            ff_amf_write_string(&p, rt->swfverify);
    }

    ff_amf_write_field_name(&p, "tcUrl");
    ff_amf_write_string2(&p, rt->tcurl, rt->auth_params);
    if (rt->is_input) {
        ff_amf_write_field_name(&p, "fpad");
        ff_amf_write_bool(&p, 0);
        ff_amf_write_field_name(&p, "capabilities");
        ff_amf_write_number(&p, 15.0);

        /* Tell the server we support all the audio codecs except
         * SUPPORT_SND_INTEL (0x0008) and SUPPORT_SND_UNUSED (0x0010)
         * which are unused in the RTMP protocol implementation. */
        ff_amf_write_field_name(&p, "audioCodecs");
        ff_amf_write_number(&p, 4071.0);
        ff_amf_write_field_name(&p, "videoCodecs");
        ff_amf_write_number(&p, 252.0);
        ff_amf_write_field_name(&p, "videoFunction");
        ff_amf_write_number(&p, 1.0);

        if (rt->pageurl) {
            ff_amf_write_field_name(&p, "pageUrl");
            ff_amf_write_string(&p, rt->pageurl);
        }
    }
    ff_amf_write_object_end(&p);

    if (rt->conn) {
        char *param = rt->conn;

        // Write arbitrary AMF data to the Connect message.
        while (param) {
            char *sep;
            param += strspn(param, " ");
            if (!*param)
                break;
            sep = strchr(param, ' ');
            if (sep)
                *sep = '\0';
            if ((ret = rtmp_write_amf_data(s, param, &p)) < 0) {
                // Invalid AMF parameter.
                ff_rtmp_packet_destroy(&pkt);
                return ret;
            }

            if (sep)
                param = sep + 1;
            else
                break;
        }
    }

    pkt.size = p - pkt.data;

    return rtmp_send_packet(rt, &pkt, 1);
}


#define RTMP_CTRL_ABORT_MESSAGE  (2)

static int read_connect(URLContext *s, RTMPContext *rt)
{
    RTMPPacket pkt = { 0 };
    uint8_t *p;
    const uint8_t *cp;
    int ret;
    char command[64];
    int stringlen;
    double seqnum;
    uint8_t tmpstr[256];
    GetByteContext gbc;

    // handle RTMP Protocol Control Messages
    for (;;) {
        if ((ret = ff_rtmp_packet_read(rt->stream, &pkt, rt->in_chunk_size,
                                       &rt->prev_pkt[0], &rt->nb_prev_pkt[0])) < 0)
            return ret;
#ifdef DEBUG
        ff_rtmp_packet_dump(s, &pkt);
#endif
        if (pkt.type == RTMP_PT_CHUNK_SIZE) {
            if ((ret = handle_chunk_size(s, &pkt)) < 0) {
                ff_rtmp_packet_destroy(&pkt);
                return ret;
            }
        } else if (pkt.type == RTMP_CTRL_ABORT_MESSAGE) {
            av_log(s, AV_LOG_ERROR, "received abort message\n");
            ff_rtmp_packet_destroy(&pkt);
            return AVERROR_UNKNOWN;
        } else if (pkt.type == RTMP_PT_BYTES_READ) {
            av_log(s, AV_LOG_TRACE, "received acknowledgement\n");
        } else if (pkt.type == RTMP_PT_WINDOW_ACK_SIZE) {
            if ((ret = handle_window_ack_size(s, &pkt)) < 0) {
                ff_rtmp_packet_destroy(&pkt);
                return ret;
            }
        } else if (pkt.type == RTMP_PT_SET_PEER_BW) {
            if ((ret = handle_set_peer_bw(s, &pkt)) < 0) {
                ff_rtmp_packet_destroy(&pkt);
                return ret;
            }
        } else if (pkt.type == RTMP_PT_INVOKE) {
            // received RTMP Command Message
            break;
        } else {
            av_log(s, AV_LOG_ERROR, "Unknown control message type (%d)\n", pkt.type);
        }
        ff_rtmp_packet_destroy(&pkt);
    }

    cp = pkt.data;
    bytestream2_init(&gbc, cp, pkt.size);
    if (ff_amf_read_string(&gbc, command, sizeof(command), &stringlen)) {
        av_log(s, AV_LOG_ERROR, "Unable to read command string\n");
        ff_rtmp_packet_destroy(&pkt);
        return AVERROR_INVALIDDATA;
    }
    if (strcmp(command, "connect")) {
        av_log(s, AV_LOG_ERROR, "Expecting connect, got %s\n", command);
        ff_rtmp_packet_destroy(&pkt);
        return AVERROR_INVALIDDATA;
    }
    ret = ff_amf_read_number(&gbc, &seqnum);
    if (ret)
        av_log(s, AV_LOG_WARNING, "SeqNum not found\n");
    /* Here one could parse an AMF Object with data as flashVers and others. */
    ret = ff_amf_get_field_value(gbc.buffer,
                                 gbc.buffer + bytestream2_get_bytes_left(&gbc),
                                 "app", tmpstr, sizeof(tmpstr));
    if (ret)
        av_log(s, AV_LOG_WARNING, "App field not found in connect\n");
    if (!ret && strcmp(tmpstr, rt->app))
        av_log(s, AV_LOG_WARNING, "App field don't match up: %s <-> %s\n",
               tmpstr, rt->app);
    ff_rtmp_packet_destroy(&pkt);

    // Send Window Acknowledgement Size (as defined in specification)
    if ((ret = ff_rtmp_packet_create(&pkt, RTMP_NETWORK_CHANNEL,
                                     RTMP_PT_WINDOW_ACK_SIZE, 0, 4)) < 0)
        return ret;
    p = pkt.data;
    // Inform the peer about how often we want acknowledgements about what
    // we send. (We don't check for the acknowledgements currently.)
    bytestream_put_be32(&p, rt->max_sent_unacked);
    pkt.size = p - pkt.data;
    ret = ff_rtmp_packet_write(rt->stream, &pkt, rt->out_chunk_size,
                               &rt->prev_pkt[1], &rt->nb_prev_pkt[1]);
    ff_rtmp_packet_destroy(&pkt);
    if (ret < 0)
        return ret;
    // Set Peer Bandwidth
    if ((ret = ff_rtmp_packet_create(&pkt, RTMP_NETWORK_CHANNEL,
                                     RTMP_PT_SET_PEER_BW, 0, 5)) < 0)
        return ret;
    p = pkt.data;
    // Tell the peer to only send this many bytes unless it gets acknowledgements.
    // This could be any arbitrary value we want here.
    bytestream_put_be32(&p, rt->max_sent_unacked);
    bytestream_put_byte(&p, 2); // dynamic
    pkt.size = p - pkt.data;
    ret = ff_rtmp_packet_write(rt->stream, &pkt, rt->out_chunk_size,
                               &rt->prev_pkt[1], &rt->nb_prev_pkt[1]);
    ff_rtmp_packet_destroy(&pkt);
    if (ret < 0)
        return ret;

    // User control
    if ((ret = ff_rtmp_packet_create(&pkt, RTMP_NETWORK_CHANNEL,
                                     RTMP_PT_USER_CONTROL, 0, 6)) < 0)
        return ret;

    p = pkt.data;
    bytestream_put_be16(&p, 0); // 0 -> Stream Begin
    bytestream_put_be32(&p, 0); // Stream 0
    ret = ff_rtmp_packet_write(rt->stream, &pkt, rt->out_chunk_size,
                               &rt->prev_pkt[1], &rt->nb_prev_pkt[1]);
    ff_rtmp_packet_destroy(&pkt);
    if (ret < 0)
        return ret;

    // Chunk size
    if ((ret = ff_rtmp_packet_create(&pkt, RTMP_NETWORK_CHANNEL,
                                     RTMP_PT_CHUNK_SIZE, 0, 4)) < 0)
        return ret;

    p = pkt.data;
    bytestream_put_be32(&p, rt->out_chunk_size);
    ret = ff_rtmp_packet_write(rt->stream, &pkt, rt->out_chunk_size,
                               &rt->prev_pkt[1], &rt->nb_prev_pkt[1]);
    ff_rtmp_packet_destroy(&pkt);
    if (ret < 0)
        return ret;

    // Send _result NetConnection.Connect.Success to connect
    if ((ret = ff_rtmp_packet_create(&pkt, RTMP_SYSTEM_CHANNEL,
                                     RTMP_PT_INVOKE, 0,
                                     RTMP_PKTDATA_DEFAULT_SIZE)) < 0)
        return ret;

    p = pkt.data;
    ff_amf_write_string(&p, "_result");
    ff_amf_write_number(&p, seqnum);

    ff_amf_write_object_start(&p);
    ff_amf_write_field_name(&p, "fmsVer");
    ff_amf_write_string(&p, "FMS/3,0,1,123");
    ff_amf_write_field_name(&p, "capabilities");
    ff_amf_write_number(&p, 31);
    ff_amf_write_object_end(&p);

    ff_amf_write_object_start(&p);
    ff_amf_write_field_name(&p, "level");
    ff_amf_write_string(&p, "status");
    ff_amf_write_field_name(&p, "code");
    ff_amf_write_string(&p, "NetConnection.Connect.Success");
    ff_amf_write_field_name(&p, "description");
    ff_amf_write_string(&p, "Connection succeeded.");
    ff_amf_write_field_name(&p, "objectEncoding");
    ff_amf_write_number(&p, 0);
    ff_amf_write_object_end(&p);

    pkt.size = p - pkt.data;
    ret = ff_rtmp_packet_write(rt->stream, &pkt, rt->out_chunk_size,
                               &rt->prev_pkt[1], &rt->nb_prev_pkt[1]);
    ff_rtmp_packet_destroy(&pkt);
    if (ret < 0)
        return ret;

    if ((ret = ff_rtmp_packet_create(&pkt, RTMP_SYSTEM_CHANNEL,
                                     RTMP_PT_INVOKE, 0, 30)) < 0)
        return ret;
    p = pkt.data;
    ff_amf_write_string(&p, "onBWDone");
    ff_amf_write_number(&p, 0);
    ff_amf_write_null(&p);
    ff_amf_write_number(&p, 8192);
    pkt.size = p - pkt.data;
    ret = ff_rtmp_packet_write(rt->stream, &pkt, rt->out_chunk_size,
                               &rt->prev_pkt[1], &rt->nb_prev_pkt[1]);
    ff_rtmp_packet_destroy(&pkt);

    return ret;
}

/**
 * Generate 'releaseStream' call and send it to the server. It should make
 * the server release some channel for media streams.
 */
static int gen_release_stream(URLContext *s, RTMPContext *rt)
{
    RTMPPacket pkt;
    uint8_t *p;
    int ret;

    if ((ret = ff_rtmp_packet_create(&pkt, RTMP_SYSTEM_CHANNEL, RTMP_PT_INVOKE,
                                     0, 29 + strlen(rt->playpath))) < 0)
        return ret;

    av_log(s, AV_LOG_DEBUG, "Releasing stream...\n");
    p = pkt.data;
    ff_amf_write_string(&p, "releaseStream");
    ff_amf_write_number(&p, ++rt->nb_invokes);
    ff_amf_write_null(&p);
    ff_amf_write_string(&p, rt->playpath);

    return rtmp_send_packet(rt, &pkt, 1);
}

/**
 * Generate 'FCPublish' call and send it to the server. It should make
 * the server prepare for receiving media streams.
 */
static int gen_fcpublish_stream(URLContext *s, RTMPContext *rt)
{
    RTMPPacket pkt;
    uint8_t *p;
    int ret;

    if ((ret = ff_rtmp_packet_create(&pkt, RTMP_SYSTEM_CHANNEL, RTMP_PT_INVOKE,
                                     0, 25 + strlen(rt->playpath))) < 0)
        return ret;

    av_log(s, AV_LOG_DEBUG, "FCPublish stream...\n");
    p = pkt.data;
    ff_amf_write_string(&p, "FCPublish");
    ff_amf_write_number(&p, ++rt->nb_invokes);
    ff_amf_write_null(&p);
    ff_amf_write_string(&p, rt->playpath);

    return rtmp_send_packet(rt, &pkt, 1);
}

/**
 * Generate 'FCUnpublish' call and send it to the server. It should make
 * the server destroy stream.
 */
static int gen_fcunpublish_stream(URLContext *s, RTMPContext *rt)
{
    RTMPPacket pkt;
    uint8_t *p;
    int ret;

    if ((ret = ff_rtmp_packet_create(&pkt, RTMP_SYSTEM_CHANNEL, RTMP_PT_INVOKE,
                                     0, 27 + strlen(rt->playpath))) < 0)
        return ret;

    av_log(s, AV_LOG_DEBUG, "UnPublishing stream...\n");
    p = pkt.data;
    ff_amf_write_string(&p, "FCUnpublish");
    ff_amf_write_number(&p, ++rt->nb_invokes);
    ff_amf_write_null(&p);
    ff_amf_write_string(&p, rt->playpath);

    return rtmp_send_packet(rt, &pkt, 0);
}

/**
 * Generate 'createStream' call and send it to the server. It should make
 * the server allocate some channel for media streams.
 */
static int gen_create_stream(URLContext *s, RTMPContext *rt)
{
    RTMPPacket pkt;
    uint8_t *p;
    int ret;

    av_log(s, AV_LOG_DEBUG, "Creating stream...\n");

    if ((ret = ff_rtmp_packet_create(&pkt, RTMP_SYSTEM_CHANNEL, RTMP_PT_INVOKE,
                                     0, 25)) < 0)
        return ret;

    p = pkt.data;
    ff_amf_write_string(&p, "createStream");
    ff_amf_write_number(&p, ++rt->nb_invokes);
    ff_amf_write_null(&p);

    return rtmp_send_packet(rt, &pkt, 1);
}


/**
 * Generate 'deleteStream' call and send it to the server. It should make
 * the server remove some channel for media streams.
 */
static int gen_delete_stream(URLContext *s, RTMPContext *rt)
{
    RTMPPacket pkt;
    uint8_t *p;
    int ret;

    av_log(s, AV_LOG_DEBUG, "Deleting stream...\n");

    if ((ret = ff_rtmp_packet_create(&pkt, RTMP_SYSTEM_CHANNEL, RTMP_PT_INVOKE,
                                     0, 34)) < 0)
        return ret;

    p = pkt.data;
    ff_amf_write_string(&p, "deleteStream");
    ff_amf_write_number(&p, ++rt->nb_invokes);
    ff_amf_write_null(&p);
    ff_amf_write_number(&p, rt->stream_id);

    return rtmp_send_packet(rt, &pkt, 0);
}

/**
 * Generate 'getStreamLength' call and send it to the server. If the server
 * knows the duration of the selected stream, it will reply with the duration
 * in seconds.
 */
static int gen_get_stream_length(URLContext *s, RTMPContext *rt)
{
    RTMPPacket pkt;
    uint8_t *p;
    int ret;

    if ((ret = ff_rtmp_packet_create(&pkt, RTMP_SOURCE_CHANNEL, RTMP_PT_INVOKE,
                                     0, 31 + strlen(rt->playpath))) < 0)
        return ret;

    p = pkt.data;
    ff_amf_write_string(&p, "getStreamLength");
    ff_amf_write_number(&p, ++rt->nb_invokes);
    ff_amf_write_null(&p);
    ff_amf_write_string(&p, rt->playpath);

    return rtmp_send_packet(rt, &pkt, 1);
}

/**
 * Generate client buffer time and send it to the server.
 */
static int gen_buffer_time(URLContext *s, RTMPContext *rt)
{
    RTMPPacket pkt;
    uint8_t *p;
    int ret;

    if ((ret = ff_rtmp_packet_create(&pkt, RTMP_NETWORK_CHANNEL, RTMP_PT_USER_CONTROL,
                                     1, 10)) < 0)
        return ret;

    p = pkt.data;
    bytestream_put_be16(&p, 3); // SetBuffer Length
    bytestream_put_be32(&p, rt->stream_id);
    bytestream_put_be32(&p, rt->client_buffer_time);

    return rtmp_send_packet(rt, &pkt, 0);
}

/**
 * Generate 'play' call and send it to the server, then ping the server
 * to start actual playing.
 */
static int gen_play(URLContext *s, RTMPContext *rt)
{
    RTMPPacket pkt;
    uint8_t *p;
    int ret;

    av_log(s, AV_LOG_DEBUG, "Sending play command for '%s'\n", rt->playpath);

    if ((ret = ff_rtmp_packet_create(&pkt, RTMP_SOURCE_CHANNEL, RTMP_PT_INVOKE,
                                     0, 29 + strlen(rt->playpath))) < 0)
        return ret;

    pkt.extra = rt->stream_id;

    p = pkt.data;
    ff_amf_write_string(&p, "play");
    ff_amf_write_number(&p,
