Do not include any explanations or other text.

----- BEGIN SOLUTION -----
/*
 * generic decoding-related code
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
#include <stdbool.h>
#include <string.h>

#include "config.h"

#if CONFIG_ICONV
# include <iconv.h>
#endif

#include "libavutil/avassert.h"
#include "libavutil/channel_layout.h"
#include "libavutil/common.h"
#include "libavutil/emms.h"
#include "libavutil/frame.h"
#include "libavutil/hwcontext.h"
#include "libavutil/imgutils.h"
#include "libavutil/internal.h"
#include "libavutil/mastering_display_metadata.h"
#include "libavutil/mem.h"
#include "libavutil/stereo3d.h"

#include "avcodec.h"
#include "avcodec_internal.h"
#include "bytestream.h"
#include "bsf.h"
#include "codec_desc.h"
#include "codec_internal.h"
#include "decode.h"
#include "hwaccel_internal.h"
#include "hwconfig.h"
#include "internal.h"
#include "lcevcdec.h"
#include "packet_internal.h"
#include "progressframe.h"
#include "libavutil/refstruct.h"
#include "thread.h"
#include "threadprogress.h"

typedef struct DecodeContext {
    AVCodecInternal avci;

    /**
     * This is set to AV_FRAME_FLAG_KEY for decoders of intra-only formats
     * (those whose codec descriptor has AV_CODEC_PROP_INTRA_ONLY set)
     * to set the flag generically.
     */
    int intra_only_flag;

    /**
     * This is set to AV_PICTURE_TYPE_I for intra only video decoders
     * and to AV_PICTURE_TYPE_NONE for other decoders. It is used to set
     * the AVFrame's pict_type before the decoder receives it.
     */
    enum AVPictureType initial_pict_type;

    /* to prevent infinite loop on errors when draining */
    int nb_draining_errors;

    /**
     * The caller has submitted a NULL packet on input.
     */
    int draining_started;

    int64_t pts_correction_num_faulty_pts; /// Number of incorrect PTS values so far
    int64_t pts_correction_num_faulty_dts; /// Number of incorrect DTS values so far
    int64_t pts_correction_last_pts;       /// PTS of the last frame
    int64_t pts_correction_last_dts;       /// DTS of the last frame

    /**
     * Bitmask indicating for which side data types we prefer user-supplied
     * (global or attached to packets) side data over bytestream.
     */
    uint64_t side_data_pref_mask;

    FFLCEVCContext *lcevc;
    int lcevc_frame;
    int width;
    int height;
} DecodeContext;

static DecodeContext *decode_ctx(AVCodecInternal *avci)
{
    return (DecodeContext *)avci;
}

static int apply_param_change(AVCodecContext *avctx, const AVPacket *avpkt)
{
    int ret;
    size_t size;
    const uint8_t *data;
    uint32_t flags;
    int64_t val;

    data = av_packet_get_side_data(avpkt, AV_PKT_DATA_PARAM_CHANGE, &size);
    if (!data)
        return 0;

    if (!(avctx->codec->capabilities & AV_CODEC_CAP_PARAM_CHANGE)) {
        av_log(avctx, AV_LOG_ERROR, "This decoder does not support parameter "
               "changes, but PARAM_CHANGE side data was sent to it.\n");
        ret = AVERROR(EINVAL);
        goto fail2;
    }

    if (size < 4)
        goto fail;

    flags = bytestream_get_le32(&data);
    size -= 4;

    if (flags & AV_SIDE_DATA_PARAM_CHANGE_SAMPLE_RATE) {
        if (size < 4)
            goto fail;
        val = bytestream_get_le32(&data);
        if (val <= 0 || val > INT_MAX) {
            av_log(avctx, AV_LOG_ERROR, "Invalid sample rate");
            ret = AVERROR_INVALIDDATA;
            goto fail2;
        }
        avctx->sample_rate = val;
        size -= 4;
    }
    if (flags & AV_SIDE_DATA_PARAM_CHANGE_DIMENSIONS) {
        if (size < 8)
            goto fail;
        avctx->width  = bytestream_get_le32(&data);
        avctx->height = bytestream_get_le32(&data);
        size -= 8;
        ret = ff_set_dimensions(avctx, avctx->width, avctx->height);
        if (ret < 0)
            goto fail2;
    }

    return 0;
fail:
    av_log(avctx, AV_LOG_ERROR, "PARAM_CHANGE side data too small.\n");
    ret = AVERROR_INVALIDDATA;
fail2:
    if (ret < 0) {
        av_log(avctx, AV_LOG_ERROR, "Error applying parameter changes.\n");
        if (avctx->err_recognition & AV_EF_EXPLODE)
            return ret;
    }
    return 0;
}

static int extract_packet_props(AVCodecInternal *avci, const AVPacket *pkt)
{
    int ret = 0;

    av_packet_unref(avci->last_pkt_props);
    if (pkt) {
        ret = av_packet_copy_props(avci->last_pkt_props, pkt);
    }
    return ret;
}

static int decode_bsfs_init(AVCodecContext *avctx)
{
    AVCodecInternal *avci = avctx->internal;
    const FFCodec *const codec = ffcodec(avctx->codec);
    int ret;

    if (avci->bsf)
        return 0;

    ret = av_bsf_list_parse_str(codec->bsfs, &avci->bsf);
    if (ret < 0) {
        av_log(avctx, AV_LOG_ERROR, "Error parsing decoder bitstream filters '%s': %s\n", codec->bsfs, av_err2str(ret));
        if (ret != AVERROR(ENOMEM))
            ret = AVERROR_BUG;
        goto fail;
    }

    /* We do not currently have an API for passing the input timebase into decoders,
     * but no filters used here should actually need it.
     * So we make up some plausible-looking number (the MPEG 90kHz timebase) */
    avci->bsf->time_base_in = (AVRational){ 1, 90000 };
    ret = avcodec_parameters_from_context(avci->bsf->par_in, avctx);
    if (ret < 0)
        goto fail;

    ret = av_bsf_init(avci->bsf);
    if (ret < 0)
        goto fail;

    return 0;
fail:
    av_bsf_free(&avci->bsf);
    return ret;
}

#if !HAVE_THREADS
#define ff_thread_get_packet(avctx, pkt) (AVERROR_BUG)
#define ff_thread_receive_frame(avctx, frame) (AVERROR_BUG)
#endif

static int decode_get_packet(AVCodecContext *avctx, AVPacket *pkt)
{
    AVCodecInternal *avci = avctx->internal;
    int ret;

    ret = av_bsf_receive_packet(avci->bsf, pkt);
    if (ret < 0)
        return ret;

    if (!(ffcodec(avctx->codec)->caps_internal & FF_CODEC_CAP_SETS_FRAME_PROPS)) {
        ret = extract_packet_props(avctx->internal, pkt);
        if (ret < 0)
            goto finish;
    }

    ret = apply_param_change(avctx, pkt);
    if (ret < 0)
        goto finish;

    return 0;
finish:
    av_packet_unref(pkt);
    return ret;
}

int ff_decode_get_packet(AVCodecContext *avctx, AVPacket *pkt)
{
    AVCodecInternal *avci = avctx->internal;
    DecodeContext     *dc = decode_ctx(avci);

    if (avci->draining)
        return AVERROR_EOF;

    /* If we are a worker thread, get the next packet from the threading
     * context. Otherwise we are the main (user-facing) context, so we get the
     * next packet from the input filterchain.
     */
    if (avctx->internal->is_frame_mt)
        return ff_thread_get_packet(avctx, pkt);

    while (1) {
        int ret = decode_get_packet(avctx, pkt);
        if (ret == AVERROR(EAGAIN) &&
            (!AVPACKET_IS_EMPTY(avci->buffer_pkt) || dc->draining_started)) {
            ret = av_bsf_send_packet(avci->bsf, avci->buffer_pkt);
            if (ret >= 0)
                continue;

            av_packet_unref(avci->buffer_pkt);
        }

        if (ret == AVERROR_EOF)
            avci->draining = 1;
        return ret;
    }
}

/**
 * Attempt to guess proper monotonic timestamps for decoded video frames
 * which might have incorrect times. Input timestamps may wrap around, in
 * which case the output will as well.
 *
 * @param pts the pts field of the decoded AVPacket, as passed through
 * AVFrame.pts
 * @param dts the dts field of the decoded AVPacket
 * @return one of the input values, may be AV_NOPTS_VALUE
 */
static int64_t guess_correct_pts(DecodeContext *dc,
                                 int64_t reordered_pts, int64_t dts)
{
    int64_t pts = AV_NOPTS_VALUE;

    if (dts != AV_NOPTS_VALUE) {
        dc->pts_correction_num_faulty_dts += dts <= dc->pts_correction_last_dts;
        dc->pts_correction_last_dts = dts;
    } else if (reordered_pts != AV_NOPTS_VALUE)
        dc->pts_correction_last_dts = reordered_pts;

    if (reordered_pts != AV_NOPTS_VALUE) {
        dc->pts_correction_num_faulty_pts += reordered_pts <= dc->pts_correction_last_pts;
        dc->pts_correction_last_pts = reordered_pts;
    } else if(dts != AV_NOPTS_VALUE)
        dc->pts_correction_last_pts = dts;

    if ((dc->pts_correction_num_faulty_pts<=dc->pts_correction_num_faulty_dts || dts == AV_NOPTS_VALUE)
       && reordered_pts != AV_NOPTS_VALUE)
        pts = reordered_pts;
    else
        pts = dts;

    return pts;
}

static int discard_samples(AVCodecContext *avctx, AVFrame *frame, int64_t *discarded_samples)
{
    AVCodecInternal *avci = avctx->internal;
    AVFrameSideData *side;
    uint32_t discard_padding = 0;
    uint8_t skip_reason = 0;
    uint8_t discard_reason = 0;

    side = av_frame_get_side_data(frame, AV_FRAME_DATA_SKIP_SAMPLES);
    if (side && side->size >= 10) {
        avci->skip_samples = AV_RL32(side->data);
        avci->skip_samples = FFMAX(0, avci->skip_samples);
        discard_padding = AV_RL32(side->data + 4);
        av_log(avctx, AV_LOG_DEBUG, "skip %d / discard %d samples due to side data\n",
               avci->skip_samples, (int)discard_padding);
        skip_reason = AV_RL8(side->data + 8);
        discard_reason = AV_RL8(side->data + 9);
    }

    if ((avctx->flags2 & AV_CODEC_FLAG2_SKIP_MANUAL)) {
        if (!side && (avci->skip_samples || discard_padding))
            side = av_frame_new_side_data(frame, AV_FRAME_DATA_SKIP_SAMPLES, 10);
        if (side && (avci->skip_samples || discard_padding)) {
            AV_WL32(side->data, avci->skip_samples);
            AV_WL32(side->data + 4, discard_padding);
            AV_WL8(side->data + 8, skip_reason);
            AV_WL8(side->data + 9, discard_reason);
            avci->skip_samples = 0;
        }
        return 0;
    }
    av_frame_remove_side_data(frame, AV_FRAME_DATA_SKIP_SAMPLES);

    if ((frame->flags & AV_FRAME_FLAG_DISCARD)) {
        avci->skip_samples = FFMAX(0, avci->skip_samples - frame->nb_samples);
        *discarded_samples += frame->nb_samples;
        return AVERROR(EAGAIN);
    }

    if (avci->skip_samples > 0) {
        if (frame->nb_samples <= avci->skip_samples){
            *discarded_samples += frame->nb_samples;
            avci->skip_samples -= frame->nb_samples;
            av_log(avctx, AV_LOG_DEBUG, "skip whole frame, skip left: %d\n",
                   avci->skip_samples);
            return AVERROR(EAGAIN);
        } else {
            av_samples_copy(frame->extended_data, frame->extended_data, 0, avci->skip_samples,
                            frame->nb_samples - avci->skip_samples, avctx->ch_layout.nb_channels, frame->format);
            if (avctx->pkt_timebase.num && avctx->sample_rate) {
                int64_t diff_ts = av_rescale_q(avci->skip_samples,
                                               (AVRational){1, avctx->sample_rate},
                                               avctx->pkt_timebase);
                if (frame->pts != AV_NOPTS_VALUE)
                    frame->pts += diff_ts;
                if (frame->pkt_dts != AV_NOPTS_VALUE)
                    frame->pkt_dts += diff_ts;
                if (frame->duration >= diff_ts)
                    frame->duration -= diff_ts;
            } else
                av_log(avctx, AV_LOG_WARNING, "Could not update timestamps for skipped samples.\n");

            av_log(avctx, AV_LOG_DEBUG, "skip %d/%d samples\n",
                   avci->skip_samples, frame->nb_samples);
            *discarded_samples += avci->skip_samples;
            frame->nb_samples -= avci->skip_samples;
            avci->skip_samples = 0;
        }
    }

    if (discard_padding > 0 && discard_padding <= frame->nb_samples) {
        if (discard_padding == frame->nb_samples) {
            *discarded_samples += frame->nb_samples;
            return AVERROR(EAGAIN);
        } else {
            if (avctx->pkt_timebase.num && avctx->sample_rate) {
                int64_t diff_ts = av_rescale_q(frame->nb_samples - discard_padding,
                                               (AVRational){1, avctx->sample_rate},
                                               avctx->pkt_timebase);
                frame->duration = diff_ts;
            } else
                av_log(avctx, AV_LOG_WARNING, "Could not update timestamps for discarded samples.\n");

            av_log(avctx, AV_LOG_DEBUG, "discard %d/%d samples\n",
                   (int)discard_padding, frame->nb_samples);
            frame->nb_samples -= discard_padding;
        }
    }

    return 0;
}

/*
 * The core of the receive_frame_wrapper for the decoders implementing
 * the simple API. Certain decoders might consume partial packets without
 * returning any output, so this function needs to be called in a loop until it
 * returns EAGAIN.
 **/
static inline int decode_simple_internal(AVCodecContext *avctx, AVFrame *frame, int64_t *discarded_samples)
{
    AVCodecInternal   *avci = avctx->internal;
    DecodeContext     *dc = decode_ctx(avci);
    AVPacket     *const pkt = avci->in_pkt;
    const FFCodec *const codec = ffcodec(avctx->codec);
    int got_frame, consumed;
    int ret;

    if (!pkt->data && !avci->draining) {
        av_packet_unref(pkt);
        ret = ff_decode_get_packet(avctx, pkt);
        if (ret < 0 && ret != AVERROR_EOF)
            return ret;
    }

    // Some codecs (at least wma lossless) will crash when feeding drain packets
    // after EOF was signaled.
    if (avci->draining_done)
        return AVERROR_EOF;

    if (!pkt->data &&
        !(avctx->codec->capabilities & AV_CODEC_CAP_DELAY))
        return AVERROR_EOF;

    got_frame = 0;

    frame->pict_type = dc->initial_pict_type;
    frame->flags    |= dc->intra_only_flag;
    consumed = codec->cb.decode(avctx, frame, &got_frame, pkt);

    if (!(codec->caps_internal & FF_CODEC_CAP_SETS_PKT_DTS))
        frame->pkt_dts = pkt->dts;
    emms_c();

    if (avctx->codec->type == AVMEDIA_TYPE_VIDEO) {
        ret = (!got_frame || frame->flags & AV_FRAME_FLAG_DISCARD)
                          ? AVERROR(EAGAIN)
                          : 0;
    } else if (avctx->codec->type == AVMEDIA_TYPE_AUDIO) {
        ret =  !got_frame ? AVERROR(EAGAIN)
                          : discard_samples(avctx, frame, discarded_samples);
    } else
        av_assert0(0);

    if (ret == AVERROR(EAGAIN))
        av_frame_unref(frame);

    // FF_CODEC_CB_TYPE_DECODE decoders must not return AVERROR EAGAIN
    // code later will add AVERROR(EAGAIN) to a pointer
    av_assert0(consumed != AVERROR(EAGAIN));
    if (consumed < 0)
        ret = consumed;
    if (consumed >= 0 && avctx->codec->type == AVMEDIA_TYPE_VIDEO)
        consumed = pkt->size;

    if (!ret)
        av_assert0(frame->buf[0]);
    if (ret == AVERROR(EAGAIN))
        ret = 0;

    /* do not stop draining when got_frame != 0 or ret < 0 */
    if (avci->draining && !got_frame) {
        if (ret < 0) {
            /* prevent infinite loop if a decoder wrongly always return error on draining */
            /* reasonable nb_errors_max = maximum b frames + thread count */
            int nb_errors_max = 20 + (HAVE_THREADS && avctx->active_thread_type & FF_THREAD_FRAME ?
                                avctx->thread_count : 1);

            if (decode_ctx(avci)->nb_draining_errors++ >= nb_errors_max) {
                av_log(avctx, AV_LOG_ERROR, "Too many errors when draining, this is a bug. "
                       "Stop draining and force EOF.\n");
                avci->draining_done = 1;
                ret = AVERROR_BUG;
            }
        } else {
            avci->draining_done = 1;
        }
    }

    if (consumed >= pkt->size || ret < 0) {
        av_packet_unref(pkt);
    } else {
        pkt->data                += consumed;
        pkt->size                -= consumed;
        pkt->pts                  = AV_NOPTS_VALUE;
        pkt->dts                  = AV_NOPTS_VALUE;
        if (!(codec->caps_internal & FF_CODEC_CAP_SETS_FRAME_PROPS)) {
            avci->last_pkt_props->pts = AV_NOPTS_VALUE;
            avci->last_pkt_props->dts = AV_NOPTS_VALUE;
        }
    }

    return ret;
}

#if CONFIG_LCMS2
static int detect_colorspace(AVCodecContext *avctx, AVFrame *frame)
{
    AVCodecInternal *avci = avctx->internal;
    enum AVColorTransferCharacteristic trc;
    AVColorPrimariesDesc coeffs;
    enum AVColorPrimaries prim;
    cmsHPROFILE profile;
    AVFrameSideData *sd;
    int ret;
    if (!(avctx->flags2 & AV_CODEC_FLAG2_ICC_PROFILES))
        return 0;

    sd = av_frame_get_side_data(frame, AV_FRAME_DATA_ICC_PROFILE);
    if (!sd || !sd->size)
        return 0;

    if (!avci->icc.avctx) {
        ret = ff_icc_context_init(&avci->icc, avctx);
        if (ret < 0)
            return ret;
    }

    profile = cmsOpenProfileFromMemTHR(avci->icc.ctx, sd->data, sd->size);
    if (!profile)
        return AVERROR_INVALIDDATA;

    ret = ff_icc_profile_sanitize(&avci->icc, profile);
    if (!ret)
        ret = ff_icc_profile_read_primaries(&avci->icc, profile, &coeffs);
    if (!ret)
        ret = ff_icc_profile_detect_transfer(&avci->icc, profile, &trc);
    cmsCloseProfile(profile);
    if (ret < 0)
        return ret;

    prim = av_csp_primaries_id_from_desc(&coeffs);
    if (prim != AVCOL_PRI_UNSPECIFIED)
        frame->color_primaries = prim;
    if (trc != AVCOL_TRC_UNSPECIFIED)
        frame->color_trc = trc;
    return 0;
}
#else /* !CONFIG_LCMS2 */
static int detect_colorspace(av_unused AVCodecContext *c, av_unused AVFrame *f)
{
    return 0;
}
#endif

static int fill_frame_props(const AVCodecContext *avctx, AVFrame *frame)
{
    int ret;

    if (frame->color_primaries == AVCOL_PRI_UNSPECIFIED)
        frame->color_primaries = avctx->color_primaries;
    if (frame->color_trc == AVCOL_TRC_UNSPECIFIED)
        frame->color_trc = avctx->color_trc;
    if (frame->colorspace == AVCOL_SPC_UNSPECIFIED)
        frame->colorspace = avctx->colorspace;
    if (frame->color_range == AVCOL_RANGE_UNSPECIFIED)
        frame->color_range = avctx->color_range;
    if (frame->chroma_location == AVCHROMA_LOC_UNSPECIFIED)
        frame->chroma_location = avctx->chroma_sample_location;

    if (avctx->codec_type == AVMEDIA_TYPE_VIDEO) {
            if (!frame->sample_aspect_ratio.num)  frame->sample_aspect_ratio = avctx->sample_aspect_ratio;
            if (frame->format == AV_PIX_FMT_NONE) frame->format              = avctx->pix_fmt;
    } else if (avctx->codec->type == AVMEDIA_TYPE_AUDIO) {
        if (frame->format == AV_SAMPLE_FMT_NONE)
            frame->format = avctx->sample_fmt;
        if (!frame->ch_layout.nb_channels) {
            ret = av_channel_layout_copy(&frame->ch_layout, &avctx->ch_layout);
            if (ret < 0)
                return ret;
        }
        if (!frame->sample_rate)
            frame->sample_rate = avctx->sample_rate;
    }

    return 0;
}

static int decode_simple_receive_frame(AVCodecContext *avctx, AVFrame *frame)
{
    int ret;
    int64_t discarded_samples = 0;

    while (!frame->buf[0]) {
        if (discarded_samples > avctx->max_samples)
            return AVERROR(EAGAIN);
        ret = decode_simple_internal(avctx, frame, &discarded_samples);
        if (ret < 0)
            return ret;
    }

    return 0;
}

int ff_decode_receive_frame_internal(AVCodecContext *avctx, AVFrame *frame)
{
    AVCodecInternal *avci = avctx->internal;
    DecodeContext     *dc = decode_ctx(avci);
    const FFCodec *const codec = ffcodec(avctx->codec);
    int ret;

    av_assert0(!frame->buf[0]);

    if (codec->cb_type == FF_CODEC_CB_TYPE_RECEIVE_FRAME) {
        while (1) {
            frame->pict_type = dc->initial_pict_type;
            frame->flags    |= dc->intra_only_flag;
            ret = codec->cb.receive_frame(avctx, frame);
            emms_c();
            if (!ret) {
                if (avctx->codec->type == AVMEDIA_TYPE_AUDIO) {
                    int64_t discarded_samples = 0;
                    ret = discard_samples(avctx, frame, &discarded_samples);
                }
                if (ret == AVERROR(EAGAIN) || (frame->flags & AV_FRAME_FLAG_DISCARD)) {
                    av_frame_unref(frame);
                    continue;
                }
            }
            break;
        }
    } else
        ret = decode_simple_receive_frame(avctx, frame);

    if (ret == AVERROR_EOF)
        avci->draining_done = 1;

    return ret;
}

static int decode_receive_frame_internal(AVCodecContext *avctx, AVFrame *frame)
{
    AVCodecInternal *avci = avctx->internal;
    DecodeContext     *dc = decode_ctx(avci);
    int ret, ok;

    if (avctx->active_thread_type & FF_THREAD_FRAME)
        ret = ff_thread_receive_frame(avctx, frame);
    else
        ret = ff_decode_receive_frame_internal(avctx, frame);

    /* preserve ret */
    ok = detect_colorspace(avctx, frame);
    if (ok < 0) {
        av_frame_unref(frame);
        return ok;
    }

    if (!ret) {
        if (avctx->codec_type == AVMEDIA_TYPE_VIDEO) {
            if (!frame->width)
                frame->width = avctx->width;
            if (!frame->height)
                frame->height = avctx->height;
        }

        ret = fill_frame_props(avctx, frame);
        if (ret < 0) {
            av_frame_unref(frame);
            return ret;
        }

        frame->best_effort_timestamp = guess_correct_pts(dc,
                                                         frame->pts,
                                                         frame->pkt_dts);

        /* the only case where decode data is not set should be decoders
         * that do not call ff_get_buffer() */
        av_assert0(frame->private_ref ||
                   !(avctx->codec->capabilities & AV_CODEC_CAP_DR1));

        if (frame->private_ref) {
            FrameDecodeData *fdd = frame->private_ref;

            if (fdd->post_process) {
                ret = fdd->post_process(avctx, frame);
                if (ret < 0) {
                    av_frame_unref(frame);
                    return ret;
                }
            }
        }
    }

    /* free the per-frame decode data */
    av_refstruct_unref(&frame->private_ref);

    return ret;
}

int attribute_align_arg avcodec_send_packet(AVCodecContext *avctx, const AVPacket *avpkt)
{
    AVCodecInternal *avci = avctx->internal;
    DecodeContext     *dc = decode_ctx(avci);
    int ret;

    if (!avcodec_is_open(avctx) || !av_codec_is_decoder(avctx->codec))
        return AVERROR(EINVAL);

    if (dc->draining_started)
        return AVERROR_EOF;

    if (avpkt && !avpkt->size && avpkt->data)
        return AVERROR(EINVAL);

    if (avpkt && (avpkt->data || avpkt->side_data_elems)) {
        if (!AVPACKET_IS_EMPTY(avci->buffer_pkt))
            return AVERROR(EAGAIN);
        ret = av_packet_ref(avci->buffer_pkt, avpkt);
        if (ret < 0)
            return ret;
    } else
        dc->draining_started = 1;

    if (!avci->buffer_frame->buf[0] && !dc->draining_started) {
        ret = decode_receive_frame_internal(avctx, avci->buffer_frame);
        if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
            return ret;
    }

    return 0;
}

static int apply_cropping(AVCodecContext *avctx, AVFrame *frame)
{
    /* make sure we are noisy about decoders returning invalid cropping data */
    if (frame->crop_left >= INT_MAX - frame->crop_right        ||
        frame->crop_top  >= INT_MAX - frame->crop_bottom       ||
        (frame->crop_left + frame->crop_right) >= frame->width ||
        (frame->crop_top + frame->crop_bottom) >= frame->height) {
        av_log(avctx, AV_LOG_WARNING,
               "Invalid cropping information set by a decoder: "
               "%"SIZE_SPECIFIER"/%"SIZE_SPECIFIER"/%"SIZE_SPECIFIER"/%"SIZE_SPECIFIER" "
               "(frame size %dx%d). This is a bug, please report it\n",
               frame->crop_left, frame->crop_right, frame->crop_top, frame->crop_bottom,
               frame->width, frame->height);
        frame->crop_left   = 0;
        frame->crop_right  = 0;
        frame->crop_top    = 0;
        frame->crop_bottom = 0;
        return 0;
    }

    if (!avctx->apply_cropping)
        return 0;

    return av_frame_apply_cropping(frame, avctx->flags & AV_CODEC_FLAG_UNALIGNED ?
                                          AV_FRAME_CROP_UNALIGNED : 0);
}

// make sure frames returned to the caller are valid
static int frame_validate(AVCodecContext *avctx, AVFrame *frame)
{
    if (!frame->buf[0] || frame->format < 0)
        goto fail;

    switch (avctx->codec_type) {
    case AVMEDIA_TYPE_VIDEO:
        if (frame->width <= 0 || frame->height <= 0)
            goto fail;
        break;
    case AVMEDIA_TYPE_AUDIO:
        if (!av_channel_layout_check(&frame->ch_layout) ||
            frame->sample_rate <= 0)
            goto fail;

        break;
    default: av_assert0(0);
    }

    return 0;
fail:
    av_log(avctx, AV_LOG_ERROR, "An invalid frame was output by a decoder. "
           "This is a bug, please report it.\n");
    return AVERROR_BUG;
}

int ff_decode_receive_frame(AVCodecContext *avctx, AVFrame *frame)
{
    AVCodecInternal *avci = avctx->internal;
    int ret;

    if (avci->buffer_frame->buf[0]) {
        av_frame_move_ref(frame, avci->buffer_frame);
    } else {
        ret = decode_receive_frame_internal(avctx, frame);
        if (ret < 0)
            return ret;
    }

    ret = frame_validate(avctx, frame);
    if (ret < 0)
        goto fail;

    if (avctx->codec_type == AVMEDIA_TYPE_VIDEO) {
        ret = apply_cropping(avctx, frame);
        if (ret < 0)
            goto fail;
    }

    avctx->frame_num++;

    return 0;
fail:
    av_frame_unref(frame);
    return ret;
}

static void get_subtitle_defaults(AVSubtitle *sub)
{
    memset(sub, 0, sizeof(*sub));
    sub->pts = AV_NOPTS_VALUE;
}

#define UTF8_MAX_BYTES 4 /* 5 and 6 bytes sequences should not be used */
static int recode_subtitle(AVCodecContext *avctx, const AVPacket **outpkt,
                           const AVPacket *inpkt, AVPacket *buf_pkt)
{
#if CONFIG_ICONV
    iconv_t cd = (iconv_t)-1;
    int ret = 0;
    char *inb, *outb;
    size_t inl, outl;
#endif

    if (avctx->sub_charenc_mode != FF_SUB_CHARENC_MODE_PRE_DECODER || inpkt->size == 0) {
        *outpkt = inpkt;
        return 0;
    }

#if CONFIG_ICONV
    inb = inpkt->data;
    inl = inpkt->size;

    if (inl >= INT_MAX / UTF8_MAX_BYTES - AV_INPUT_BUFFER_PADDING_SIZE) {
        av_log(avctx, AV_LOG_ERROR, "Subtitles packet is too big for recoding\n");
        return AVERROR(ERANGE);
    }

    cd = iconv_open("UTF-8", avctx->sub_charenc);
    av_assert0(cd != (iconv_t)-1);

    ret = av_new_packet(buf_pkt, inl * UTF8_MAX_BYTES);
    if (ret < 0)
        goto end;
    ret = av_packet_copy_props(buf_pkt, inpkt);
    if (ret < 0)
        goto end;
    outb = buf_pkt->data;
    outl = buf_pkt->size;

    if (iconv(cd, &inb, &inl, &outb, &outl) == (size_t)-1 ||
        iconv(cd, NULL, NULL, &outb, &outl) == (size_t)-1 ||
        outl >= buf_pkt->size || inl != 0) {
        ret = FFMIN(AVERROR(errno), -1);
        av_log(avctx, AV_LOG_ERROR, "Unable to recode subtitle event \"%s\" "
               "from %s to UTF-8\n", inpkt->data, avctx->sub_charenc);
        goto end;
    }
    buf_pkt->size -= outl;
    memset(buf_pkt->data + buf_pkt->size, 0, outl);
    *outpkt = buf_pkt;

    ret = 0;
end:
    if (ret < 0)
        av_packet_unref(buf_pkt);
    if (cd != (iconv_t)-1)
        iconv_close(cd);
    return ret;
#else
    av_log(avctx, AV_LOG_ERROR, "requesting subtitles recoding without iconv");
    return AVERROR(EINVAL);
#endif
}

static int utf8_check(const uint8_t *str)
{
    const uint8_t *byte;
    uint32_t codepoint, min;

    while (*str) {
        byte = str;
        GET_UTF8(codepoint, *(byte++), return 0;);
        min = byte - str == 1 ? 0 : byte - str == 2 ? 0x80 :
              1 << (5 * (byte - str) - 4);
        if (codepoint < min || codepoint >= 0x110000 ||
            codepoint == 0xFFFE /* BOM */ ||
            codepoint >= 0xD800 && codepoint <= 0xDFFF /* surrogates */)
            return 0;
        str = byte;
    }
    return 1;
}

int avcodec_decode_subtitle2(AVCodecContext *avctx, AVSubtitle *sub,
                             int *got_sub_ptr, const AVPacket *avpkt)
{
    int ret = 0;

    if (!avpkt->data && avpkt->size) {
        av_log(avctx, AV_LOG_ERROR, "invalid packet: NULL data, size != 0\n");
        return AVERROR(EINVAL);
    }
    if (!avctx->codec)
        return AVERROR(EINVAL);
    if (ffcodec(avctx->codec)->cb_type != FF_CODEC_CB_TYPE_DECODE_SUB) {
        av_log(avctx, AV_LOG_ERROR, "Codec not subtitle decoder\n");
        return AVERROR(EINVAL);
    }

    *got_sub_ptr = 0;
    get_subtitle_defaults(sub);

    if ((avctx->codec->capabilities & AV_CODEC_CAP_DELAY) || avpkt->size) {
        AVCodecInternal *avci = avctx->internal;
        const AVPacket *pkt;

        ret = recode_subtitle(avctx, &pkt, avpkt, avci->buffer_pkt);
        if (ret < 0)
            return ret;

        if (avctx->pkt_timebase.num && avpkt->pts != AV_NOPTS_VALUE)
            sub->pts = av_rescale_q(avpkt->pts,
                                    avctx->pkt_timebase, AV_TIME_BASE_Q);
        ret = ffcodec(avctx->codec)->cb.decode_sub(avctx, sub, got_sub_ptr, pkt);
        if (pkt == avci->buffer_pkt) // did we recode?
            av_packet_unref(avci->buffer_pkt);
        if (ret < 0) {
            *got_sub_ptr = 0;
            avsubtitle_free(sub);
            return ret;
        }
        av_assert1(!sub->num_rects || *got_sub_ptr);

        if (sub->num_rects && !sub->end_display_time && avpkt->duration &&
            avctx->pkt_timebase.num) {
            AVRational ms = { 1, 1000 };
            sub->end_display_time = av_rescale_q(avpkt->duration,
                                                 avctx->pkt_timebase, ms);
        }

        if (avctx->codec_descriptor->props & AV_CODEC_PROP_BITMAP_SUB)
            sub->format = 0;
        else if (avctx->codec_descriptor->props & AV_CODEC_PROP_TEXT_SUB)
            sub->format = 1;

        for (unsigned i = 0; i < sub->num_rects; i++) {
            if (avctx->sub_charenc_mode != FF_SUB_CHARENC_MODE_IGNORE &&
                sub->rects[i]->ass && !utf8_check(sub->rects[i]->ass)) {
                av_log(avctx, AV_LOG_ERROR,
                       "Invalid UTF-8 in decoded subtitles text; "
                       "maybe missing -sub_charenc option\n");
                avsubtitle_free(sub);
                *got_sub_ptr = 0;
                return
