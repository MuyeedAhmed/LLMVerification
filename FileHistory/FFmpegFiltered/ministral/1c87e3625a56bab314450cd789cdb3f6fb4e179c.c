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
        case VVC_SEI_TYPE_FILM_GRAIN_CHARACTERISTICS:
            if (ff_vvc_sei_decode_film_grain_characteristics(s, message, fc) < 0)
                return AVERROR_INVALIDDATA;
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