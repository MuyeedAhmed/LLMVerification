/*
 * VVC Supplementary Enhancement Information messages
 *
 * copyright (c) 2024 Wu Jianhua <toqsxw@outlook.com>
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

#include "sei.h"
#include "dec.h"
#include "libavutil/refstruct.h"

int ff_vvc_sei_decode(VVCSEI *s, const H266RawSEI *sei, const struct VVCFrameContext *fc)
{
    if (!sei)
        return AVERROR_INVALIDDATA;

    for (int i = 0; i < sei->message_list.nb_messages; i++) {
        SEIRawMessage *message = &sei->message_list.messages[i];

        switch (message->payload_type) {
        case VVC_SEI_TYPE_DECODE_FILM_GRAIN_CHARACTERISTICS:
            if (message->payload_size < 1) {
                av_log(fc->log_ctx, AV_LOG_ERROR, "Invalid decode film grain characteristics SEI message size\n");
                return AVERROR_INVALIDDATA;
            }
            // Handle decode film grain characteristics SEI message
            // The payload contains film grain characteristics data
            // For now, we just log that we received it
            av_log(fc->log_ctx, AV_LOG_DEBUG, "Received decode film grain characteristics SEI message\n");
            break;
        default:
            av_log(fc->log_ctx, AV_LOG_DEBUG, "Skipped %s SEI %d\n",
                sei->nal_unit_header.nal_unit_type == VVC_PREFIX_SEI_NUT ?
                    "PREFIX" : "SUFFIX", message->payload_type);
            return FF_H2645_SEI_MESSAGE_UNHANDLED;
        }
    }

    return 0;
}

int ff_vvc_sei_replace(VVCSEI *dst, const VVCSEI *src)
{
    return ff_h2645_sei_ctx_replace(&dst->common, &src->common);
}

void ff_vvc_sei_reset(VVCSEI *s)
{
    ff_h2645_sei_reset(&s->common);
}
