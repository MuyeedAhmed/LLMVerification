
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

static void decode_film_grain_characteristics(VVCSEI *s, const SEIRawMessage *message, const VVCFrameContext *fc)
{
    // Placeholder implementation
    av_log(fc->log_ctx, AV_LOG_VERBOSE, "Parsed film grain characteristics payload (size=%d)\n",
           message->payload_size);
    // Actual parsing logic would go here
}

int ff_vvc_sei_decode(VVCSEI *s, const H266RawSEI *sei, const struct VVCFrameContext *fc)
{
    if (!sei)
        return AVERROR_INVALIDDATA;

    for (int i = 0; i < sei->message_list.nb_messages; i++) {
        SEIRawMessage *message = &sei->message_list.messages[i];

        switch (message->payload_type) {
        case VVC_SEI_FILM_GRAIN_CHARACTERISTICS:
            av_log(fc->log_ctx, AV_LOG_DEBUG, "Decoding film grain characteristics SEI\n");
            decode_film_grain_characteristics(s, message, fc);
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
