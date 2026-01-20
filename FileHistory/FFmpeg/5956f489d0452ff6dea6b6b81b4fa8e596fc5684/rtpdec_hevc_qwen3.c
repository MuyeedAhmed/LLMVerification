/*
 * RTP parser for HEVC/H.265 payload format (draft version 6)
 * Copyright (c) 2014 Thomas Volkert <thomas@homer-conferencing.com>
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
 *
 */

#include "libavutil/avstring.h"
#include "libavutil/base64.h"

#include "avformat.h"
#include "rtpdec.h"
#include "rtpdec_formats.h"

#define RTP_HEVC_PAYLOAD_HEADER_SIZE  2
#define RTP_HEVC_FU_HEADER_SIZE       1
#define RTP_HEVC_DONL_FIELD_SIZE      2
#define RTP_HEVC_DOND_FIELD_SIZE      1
#define HEVC_SPECIFIED_NAL_UNIT_TYPES 48

/* SDP out-of-band signaling data */
struct PayloadContext {
    int using_donl_field;
    int profile_id;
    uint8_t *sps, *pps, *vps, *sei;
    int sps_size, pps_size, vps_size, sei_size;
};

static const uint8_t start_sequence[] = { 0x00, 0x00, 0x00, 0x01 };

static av_cold int hevc_sdp_parse_fmtp_config(AVFormatContext *s,
                                              AVStream *stream,
                                              PayloadContext *hevc_data,
                                              char *attr, char *value)
{
    /* profile-space: 0-3 */
    /* profile-id: 0-31 */
    if (!strcmp(attr, "profile-id")) {
        hevc_data->profile_id = atoi(value);
        av_dlog(s, "SDP: found profile-id: %d\n", hevc_data->profile_id);
    }

    /* tier-flag: 0-1 */
    /* level-id: 0-255 */
    /* interop-constraints: [base16] */
    /* profile-compatibility-indicator: [base16] */
    /* sprop-sub-layer-id: 0-6, defines highest possible value for TID, default: 6 */
    /* recv-sub-layer-id: 0-6 */
    /* max-recv-level-id: 0-255 */
    /* tx-mode: MSM,SSM */
    /* sprop-vps: [base64] */
    /* sprop-sps: [base64] */
    /* sprop-pps: [base64] */
    /* sprop-sei: [base64] */
    if (!strcmp(attr, "sprop-vps") || !strcmp(attr, "sprop-sps") ||
        !strcmp(attr, "sprop-pps") || !strcmp(attr, "sprop-sei")) {
        uint8_t **data_ptr = NULL;
        int *size_ptr = NULL;
        if (!strcmp(attr, "sprop-vps")) {
            data_ptr = &hevc_data->vps;
            size_ptr = &hevc_data->vps_size;
        } else if (!strcmp(attr, "sprop-sps")) {
            data_ptr = &hevc_data->sps;
            size_ptr = &hevc_data->sps_size;
        } else if (!strcmp(attr, "sprop-pps")) {
            data_ptr = &hevc_data->pps;
            size_ptr = &hevc_data->pps_size;
        } else if (!strcmp(attr, "sprop-sei")) {
            data_ptr = &hevc_data->sei;
            size_ptr = &hevc_data->sei_size;
        }

        ff_h264_parse_sprop_parameter_sets(s, data_ptr,
                                           size_ptr, value);
    }

    /* max-lsr, max-lps, max-cpb, max-dpb, max-br, max-tr, max-tc */
    /* max-fps */

    /* sprop-max-don-diff: 0-32767

         When the RTP stream depends on one or more other RTP
         streams (in this case tx-mode MUST be equal to "MSM" and
         MSM is in use), this parameter MUST be present and the
         value MUST be greater than 0.
    */
    if (!strcmp(attr, "sprop-max-don-diff")) {
        if (atoi(value) > 0)
            hevc_data->using_donl_field = 1;
        av_dlog(s, "Found sprop-max-don-diff in SDP, DON field usage is: %d\n",
                hevc_data->using_donl_field);
    }

    /* sprop-depack-buf-nalus: 0-32767 */
    if (!strcmp(attr, "sprop-depack-buf-nalus")) {
        if (atoi(value) > 0)
            hevc_data->using_donl_field = 1;
        av_dlog(s, "Found sprop-depack-buf-nalus in SDP, DON field usage is: %d\n",
                hevc_data->using_donl_field);
    }

    /* sprop-depack-buf-bytes: 0-4294967295 */
    /* depack-buf-cap */
    /* sprop-segmentation-id: 0-3 */
    /* sprop-spatial-segmentation-idc: [base16] */
    /* dec-parallel-ca: */
    /* include-dph */

    return 0;
}

static av_cold int hevc_parse_sdp_line(AVFormatContext *ctx, int st_index,
                                       PayloadContext *hevc_data, const char *line)
{
    AVStream *current_stream;
    AVCodecContext *codec;
    const char *sdp_line_ptr = line;

    if (st_index < 0)
        return 0;

    current_stream = ctx->streams[st_index];
    codec  = current_stream->codec;

    if (av_strstart(sdp_line_ptr, "framesize:", &sdp_line_ptr)) {
        char str_video_width[50];
        char *str_video_width_ptr = str_video_width;

        /*
         * parse "a=framesize:96 320-240"
         */

        /* ignore spaces */
        while (*sdp_line_ptr && *sdp_line_ptr == ' ')
            sdp_line_ptr++;
        /* ignore RTP payload ID */
        while (*sdp_line_ptr && *sdp_line_ptr != ' ')
            sdp_line_ptr++;
        /* ignore spaces */
        while (*sdp_line_ptr && *sdp_line_ptr == ' ')
            sdp_line_ptr++;
        /* extract the actual video resolution description */
        while (*sdp_line_ptr && *sdp_line_ptr != '-' &&
               (str_video_width_ptr - str_video_width) < sizeof(str_video_width) - 1)
            *str_video_width_ptr++ = *sdp_line_ptr++;
        /* add trailing zero byte */
        *str_video_width_ptr = '\0';

        /* determine the width value */
        codec->width   = atoi(str_video_width);
        /* jump beyond the "-" and determine the height value */
        codec->height  = atoi(sdp_line_ptr + 1);
    } else if (av_strstart(sdp_line_ptr, "fmtp:", &sdp_line_ptr)) {
        int ret = ff_parse_fmtp(ctx, current_stream, hevc_data, sdp_line_ptr,
                                hevc_sdp_parse_fmtp_config);
        if (hevc_data->vps_size || hevc_data->sps_size ||
            hevc_data->pps_size || hevc_data->sei_size) {
            av_freep(&codec->extradata);
            codec->extradata_size = hevc_data->vps_size + hevc_data->sps_size +
                                    hevc_data->pps_size + hevc_data->sei_size;
            codec->extradata = av_malloc(codec->extradata_size +
                                         FF_INPUT_BUFFER_PADDING_SIZE);
            if (!codec->extradata) {
                ret = AVERROR(ENOMEM);
                codec->extradata_size = 0;
            } else {
                int pos = 0;
                memcpy(codec->extradata + pos, hevc_data->vps, hevc_data->vps_size);
                pos += hevc_data->vps_size;
                memcpy(codec->extradata + pos, hevc_data->sps, hevc_data->sps_size);
                pos += hevc_data->sps_size;
                memcpy(codec->extradata + pos, hevc_data->pps, hevc_data->pps_size);
                pos += hevc_data->pps_size;
                memcpy(codec->extradata + pos, hevc_data->sei, hevc_data->sei_size);
                pos += hevc_data->sei_size;
                memset(codec->extradata + pos, 0, FF_INPUT_BUFFER_PADDING_SIZE);
            }

            av_freep(&hevc_data->vps);
            av_freep(&hevc_data->sps);
            av_freep(&hevc_data->pps);
            av_freep(&hevc_data->sei);
            hevc_data->vps_size = 0;
            hevc_data->sps_size = 0;
            hevc_data->pps_size = 0;
            hevc_data->sei_size = 0;
        }
        return ret;
    }

    return 0;
}


RTPDynamicProtocolHandler ff_hevc_dynamic_handler = {
    .enc_name         = "H265",
    .codec_type       = AVMEDIA_TYPE_VIDEO,
    .codec_id         = AV_CODEC_ID_HEVC,
    .need_parsing     = AVSTREAM_PARSE_FULL,
    .priv_data_size   = sizeof(PayloadContext),
    .parse_sdp_a_line = hevc_parse_sdp_line,
    .parse_packet     = hevc_handle_packet,
};
