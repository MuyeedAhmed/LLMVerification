Do not include any other text.

----- BEGIN MODIFIED C SOURCE CODE -----
/*
 * copyright (c) 2015 Rick Kern <kernrj@gmail.com>
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

#include <VideoToolbox/VideoToolbox.h>
#include <CoreVideo/CoreVideo.h>
#include <CoreMedia/CoreMedia.h>
#include <TargetConditionals.h>
#include <Availability.h>
#include "avcodec.h"
#include "libavutil/mem.h"
#include "libavutil/opt.h"
#include "libavutil/avassert.h"
#include "libavutil/avstring.h"
#include "libavcodec/avcodec.h"
#include "libavutil/pixdesc.h"
#include "libavutil/hwcontext_videotoolbox.h"
#include "codec_internal.h"
#include "internal.h"
#include <pthread.h>
#include "atsc_a53.h"
#include "encode.h"
#include "h264.h"
#include "h264_sei.h"
#include "hwconfig.h"
#include <dlfcn.h>

#if !HAVE_KCMVIDEOCODECTYPE_HEVC
enum { kCMVideoCodecType_HEVC = 'hvc1' };
#endif

#if !HAVE_KCMVIDEOCODECTYPE_HEVCWITHALPHA
enum { kCMVideoCodecType_HEVCWithAlpha = 'muxa' };
#endif

#if !HAVE_KCVPIXELFORMATTYPE_420YPCBCR10BIPLANARVIDEORANGE
enum { kCVPixelFormatType_420YpCbCr10BiPlanarFullRange = 'xf20' };
enum { kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange = 'x420' };
#endif

#if !HAVE_KVTQPMODULATIONLEVEL_DEFAULT
enum { kVTQPModulationLevel_Default = -1 };
enum { kVTQPModulationLevel_Disable = 0 };
#endif

#ifndef TARGET_CPU_ARM64
#   define TARGET_CPU_ARM64 0
#endif

typedef OSStatus (*getParameterSetAtIndex)(CMFormatDescriptionRef videoDesc,
                                           size_t parameterSetIndex,
                                           const uint8_t **parameterSetPointerOut,
                                           size_t *parameterSetSizeOut,
                                           size_t *parameterSetCountOut,
                                           int *NALUnitHeaderLengthOut);

/*
 * Symbols that aren't available in MacOS 10.8 and iOS 8.0 need to be accessed
 * from compat_keys, or it will cause compiler errors when compiling for older
 * OS versions.
 *
 * For example, kVTCompressionPropertyKey_H264EntropyMode was added in
 * MacOS 10.9. If this constant were used directly, a compiler would generate
 * an error when it has access to the MacOS 10.8 headers, but does not have
 * 10.9 headers.
 *
 * Runtime errors will still occur when unknown keys are set. A warning is
 * logged and encoding continues where possible.
 *
 * When adding new symbols, they should be loaded/set in loadVTEncSymbols().
 */
static struct{
    CFStringRef kCVImageBufferColorPrimaries_ITU_R_2020;
    CFStringRef kCVImageBufferTransferFunction_ITU_R_2020;
    CFStringRef kCVImageBufferYCbCrMatrix_ITU_R_2020;

    CFStringRef kVTCompressionPropertyKey_H264EntropyMode;
    CFStringRef kVTH264EntropyMode_CAVLC;
    CFStringRef kVTH264EntropyMode_CABAC;

    CFStringRef kVTProfileLevel_H264_Baseline_4_0;
    CFStringRef kVTProfileLevel_H264_Baseline_4_2;
    CFStringRef kVTProfileLevel_H264_Baseline_5_0;
    CFStringRef kVTProfileLevel_H264_Baseline_5_1;
    CFStringRef kVTProfileLevel_H264_Baseline_5_2;
    CFStringRef kVTProfileLevel_H264_Baseline_AutoLevel;
    CFStringRef kVTProfileLevel_H264_Main_4_2;
    CFStringRef kVTProfileLevel_H264_Main_5_1;
    CFStringRef kVTProfileLevel_H264_Main_5_2;
    CFStringRef kVTProfileLevel_H264_Main_AutoLevel;
    CFStringRef kVTProfileLevel_H264_High_3_0;
    CFStringRef kVTProfileLevel_H264_High_3_1;
    CFStringRef kVTProfileLevel_H264_High_3_2;
    CFStringRef kVTProfileLevel_H264_High_4_0;
    CFStringRef kVTProfileLevel_H264_High_4_1;
    CFStringRef kVTProfileLevel_H264_High_4_2;
    CFStringRef kVTProfileLevel_H264_High_5_1;
    CFStringRef kVTProfileLevel_H264_High_5_2;
    CFStringRef kVTProfileLevel_H264_High_AutoLevel;
    CFStringRef kVTProfileLevel_H264_Extended_5_0;
    CFStringRef kVTProfileLevel_H264_Extended_AutoLevel;
    CFStringRef kVTProfileLevel_H264_ConstrainedBaseline_AutoLevel;
    CFStringRef kVTProfileLevel_H264_ConstrainedHigh_AutoLevel;

    CFStringRef kVTProfileLevel_HEVC_Main_AutoLevel;
    CFStringRef kVTProfileLevel_HEVC_Main10_AutoLevel;
    CFStringRef kVTProfileLevel_HEVC_Main42210_AutoLevel;

    CFStringRef kVTCompressionPropertyKey_RealTime;
    CFStringRef kVTCompressionPropertyKey_TargetQualityForAlpha;
    CFStringRef kVTCompressionPropertyKey_PrioritizeEncodingSpeedOverQuality;
    CFStringRef kVTCompressionPropertyKey_ConstantBitRate;
    CFStringRef kVTCompressionPropertyKey_EncoderID;
    CFStringRef kVTCompressionPropertyKey_SpatialAdaptiveQPLevel;

    CFStringRef kVTVideoEncoderSpecification_EnableHardwareAcceleratedVideoEncoder;
    CFStringRef kVTVideoEncoderSpecification_RequireHardwareAcceleratedVideoEncoder;
    CFStringRef kVTVideoEncoderSpecification_EnableLowLatencyRateControl;
    CFStringRef kVTCompressionPropertyKey_AllowOpenGOP;
    CFStringRef kVTCompressionPropertyKey_MaximizePowerEfficiency;
    CFStringRef kVTCompressionPropertyKey_ReferenceBufferCount;
    CFStringRef kVTCompressionPropertyKey_MaxAllowedFrameQP;
    CFStringRef kVTCompressionPropertyKey_MinAllowedFrameQP;

    getParameterSetAtIndex CMVideoFormatDescriptionGetHEVCParameterSetAtIndex;
} compat_keys;

#define GET_SYM(symbol, defaultVal)                                     \
do{                                                                     \
    CFStringRef* handle = (CFStringRef*)dlsym(RTLD_DEFAULT, #symbol);   \
    if(!handle)                                                         \
        compat_keys.symbol = CFSTR(defaultVal);                         \
    else                                                                \
        compat_keys.symbol = *handle;                                   \
}while(0)

static pthread_once_t once_ctrl = PTHREAD_ONCE_INIT;

static void loadVTEncSymbols(void){
    compat_keys.CMVideoFormatDescriptionGetHEVCParameterSetAtIndex =
        (getParameterSetAtIndex)dlsym(
            RTLD_DEFAULT,
            "CMVideoFormatDescriptionGetHEVCParameterSetAtIndex"
        );

    GET_SYM(kCVImageBufferColorPrimaries_ITU_R_2020,   "ITU_R_2020");
    GET_SYM(kCVImageBufferTransferFunction_ITU_R_2020, "ITU_R_2020");
    GET_SYM(kCVImageBufferYCbCrMatrix_ITU_R_2020,      "ITU_R_2020");

    GET_SYM(kVTCompressionPropertyKey_H264EntropyMode, "H264EntropyMode");
    GET_SYM(kVTH264EntropyMode_CAVLC, "CAVLC");
    GET_SYM(kVTH264EntropyMode_CABAC, "CABAC");

    GET_SYM(kVTProfileLevel_H264_Baseline_4_0,       "H264_Baseline_4_0");
    GET_SYM(kVTProfileLevel_H264_Baseline_4_2,       "H264_Baseline_4_2");
    GET_SYM(kVTProfileLevel_H264_Baseline_5_0,       "H264_Baseline_5_0");
    GET_SYM(kVTProfileLevel_H264_Baseline_5_1,       "H264_Baseline_5_1");
    GET_SYM(kVTProfileLevel_H264_Baseline_5_2,       "H264_Baseline_5_2");
    GET_SYM(kVTProfileLevel_H264_Baseline_AutoLevel, "H264_Baseline_AutoLevel");
    GET_SYM(kVTProfileLevel_H264_Main_4_2,           "H264_Main_4_2");
    GET_SYM(kVTProfileLevel_H264_Main_5_1,           "H264_Main_5_1");
    GET_SYM(kVTProfileLevel_H264_Main_5_2,           "H264_Main_5_2");
    GET_SYM(kVTProfileLevel_H264_Main_AutoLevel,     "H264_Main_AutoLevel");
    GET_SYM(kVTProfileLevel_H264_High_3_0,           "H264_High_3_0");
    GET_SYM(kVTProfileLevel_H264_High_3_1,           "H264_High_3_1");
    GET_SYM(kVTProfileLevel_H264_High_3_2,           "H264_High_3_2");
    GET_SYM(kVTProfileLevel_H264_High_4_0,           "H264_High_4_0");
    GET_SYM(kVTProfileLevel_H264_High_4_1,           "H264_High_4_1");
    GET_SYM(kVTProfileLevel_H264_High_4_2,           "H264_High_4_2");
    GET_SYM(kVTProfileLevel_H264_High_5_1,           "H264_High_5_1");
    GET_SYM(kVTProfileLevel_H264_High_5_2,           "H264_High_5_2");
    GET_SYM(kVTProfileLevel_H264_High_AutoLevel,     "H264_High_AutoLevel");
    GET_SYM(kVTProfileLevel_H264_Extended_5_0,       "H264_Extended_5_0");
    GET_SYM(kVTProfileLevel_H264_Extended_AutoLevel, "H264_Extended_AutoLevel");
    GET_SYM(kVTProfileLevel_H264_ConstrainedBaseline_AutoLevel, "H264_ConstrainedBaseline_AutoLevel");
    GET_SYM(kVTProfileLevel_H264_ConstrainedHigh_AutoLevel,     "H264_ConstrainedHigh_AutoLevel");

    GET_SYM(kVTProfileLevel_HEVC_Main_AutoLevel,     "HEVC_Main_AutoLevel");
    GET_SYM(kVTProfileLevel_HEVC_Main10_AutoLevel,   "HEVC_Main10_AutoLevel");
    GET_SYM(kVTProfileLevel_HEVC_Main42210_AutoLevel,   "HEVC_Main42210_AutoLevel");

    GET_SYM(kVTCompressionPropertyKey_RealTime, "RealTime");
    GET_SYM(kVTCompressionPropertyKey_TargetQualityForAlpha,
            "TargetQualityForAlpha");
    GET_SYM(kVTCompressionPropertyKey_PrioritizeEncodingSpeedOverQuality,
            "PrioritizeEncodingSpeedOverQuality");
    GET_SYM(kVTCompressionPropertyKey_ConstantBitRate, "ConstantBitRate");
    GET_SYM(kVTCompressionPropertyKey_EncoderID, "EncoderID");

    GET_SYM(kVTVideoEncoderSpecification_EnableHardwareAcceleratedVideoEncoder,
            "EnableHardwareAcceleratedVideoEncoder");
    GET_SYM(kVTVideoEncoderSpecification_RequireHardwareAcceleratedVideoEncoder,
            "RequireHardwareAcceleratedVideoEncoder");
    GET_SYM(kVTVideoEncoderSpecification_EnableLowLatencyRateControl,
                "EnableLowLatencyRateControl");
    GET_SYM(kVTCompressionPropertyKey_AllowOpenGOP, "AllowOpenGOP");
    GET_SYM(kVTCompressionPropertyKey_MaximizePowerEfficiency,
            "MaximizePowerEfficiency");
    GET_SYM(kVTCompressionPropertyKey_ReferenceBufferCount,
            "ReferenceBufferCount");
    GET_SYM(kVTCompressionPropertyKey_MaxAllowedFrameQP, "MaxAllowedFrameQP");
    GET_SYM(kVTCompressionPropertyKey_MinAllowedFrameQP, "MinAllowedFrameQP");
    GET_SYM(kVTCompressionPropertyKey_SpatialAdaptiveQPLevel, "SpatialAdaptiveQPLevel");
}

#define H264_PROFILE_CONSTRAINED_HIGH (AV_PROFILE_H264_HIGH | AV_PROFILE_H264_CONSTRAINED)

typedef enum VTH264Entropy{
    VT_ENTROPY_NOT_SET,
    VT_CAVLC,
    VT_CABAC
} VTH264Entropy;

static const uint8_t start_code[] = { 0, 0, 0, 1 };

typedef struct ExtraSEI {
  void *data;
  size_t size;
} ExtraSEI;

typedef struct BufNode {
    CMSampleBufferRef cm_buffer;
    ExtraSEI sei;
    AVBufferRef *frame_buf;
    struct BufNode* next;
} BufNode;

typedef struct VTEncContext {
    AVClass *class;
    enum AVCodecID codec_id;
    VTCompressionSessionRef session;
    CFDictionaryRef supported_props;
    CFStringRef ycbcr_matrix;
    CFStringRef color_primaries;
    CFStringRef transfer_function;
    getParameterSetAtIndex get_param_set_func;

    pthread_mutex_t lock;
    pthread_cond_t  cv_sample_sent;

    int async_error;

    BufNode *q_head;
    BufNode *q_tail;

    int64_t frame_ct_out;
    int64_t frame_ct_in;

    int64_t first_pts;
    int64_t dts_delta;

    int profile;
    int level;
    int entropy;
    int realtime;
    int frames_before;
    int frames_after;
    int constant_bit_rate;

    int allow_sw;
    int require_sw;
    double alpha_quality;
    int prio_speed;

    bool flushing;
    int has_b_frames;
    bool warned_color_range;

    /* can't be bool type since AVOption will access it as int */
    int a53_cc;

    int max_slice_bytes;
    int power_efficient;
    int max_ref_frames;
    int spatialaq;
} VTEncContext;

static void vtenc_free_buf_node(BufNode *info)
{
    if (!info)
        return;

    av_free(info->sei.data);
    if (info->cm_buffer)
        CFRelease(info->cm_buffer);
    av_buffer_unref(&info->frame_buf);
    av_free(info);
}

static int vt_dump_encoder(AVCodecContext *avctx)
{
    VTEncContext *vtctx = avctx->priv_data;
    CFStringRef encoder_id = NULL;
    int status;
    CFIndex length, max_size;
    char *name;

    status = VTSessionCopyProperty(vtctx->session,
                                   compat_keys.kVTCompressionPropertyKey_EncoderID,
                                   kCFAllocatorDefault,
                                   &encoder_id);
    // OK if not supported
    if (status != noErr)
        return 0;

    length = CFStringGetLength(encoder_id);
    max_size = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8);
    name = av_malloc(max_size);
    if (!name) {
        CFRelease(encoder_id);
        return AVERROR(ENOMEM);
    }

    CFStringGetCString(encoder_id,
                       name,
                       max_size,
                       kCFStringEncodingUTF8);
    av_log(avctx, AV_LOG_DEBUG, "Init the encoder: %s\n", name);
    av_freep(&name);
    CFRelease(encoder_id);

    return 0;
}

static int vtenc_populate_extradata(AVCodecContext   *avctx,
                                    CMVideoCodecType codec_type,
                                    CFStringRef      profile_level,
                                    CFNumberRef      gamma_level,
                                    CFDictionaryRef  enc_info,
                                    CFDictionaryRef  pixel_buffer_info);

/**
 * NULL-safe release of *refPtr, and sets value to NULL.
 */
static void vt_release_num(CFNumberRef* refPtr){
    if (!*refPtr) {
        return;
    }

    CFRelease(*refPtr);
    *refPtr = NULL;
}

static void set_async_error(VTEncContext *vtctx, int err)
{
    BufNode *info;

    pthread_mutex_lock(&vtctx->lock);

    vtctx->async_error = err;

    info = vtctx->q_head;
    vtctx->q_head = vtctx->q_tail = NULL;

    while (info) {
        BufNode *next = info->next;
        vtenc_free_buf_node(info);
        info = next;
    }

    pthread_mutex_unlock(&vtctx->lock);
}

static void clear_frame_queue(VTEncContext *vtctx)
{
    set_async_error(vtctx, 0);
}

static void vtenc_reset(VTEncContext *vtctx)
{
    if (vtctx->session) {
        CFRelease(vtctx->session);
        vtctx->session = NULL;
    }

    if (vtctx->supported_props) {
        CFRelease(vtctx->supported_props);
        vtctx->supported_props = NULL;
    }

    if (vtctx->color_primaries) {
        CFRelease(vtctx->color_primaries);
        vtctx->color_primaries = NULL;
    }

    if (vtctx->transfer_function) {
        CFRelease(vtctx->transfer_function);
        vtctx->transfer_function = NULL;
    }

    if (vtctx->ycbcr_matrix) {
        CFRelease(vtctx->ycbcr_matrix);
        vtctx->ycbcr_matrix = NULL;
    }
}

static int vtenc_q_pop(VTEncContext *vtctx, bool wait, CMSampleBufferRef *buf, ExtraSEI *sei)
{
    BufNode *info;

    pthread_mutex_lock(&vtctx->lock);

    if (vtctx->async_error) {
        pthread_mutex_unlock(&vtctx->lock);
        return vtctx->async_error;
    }

    if (vtctx->flushing && vtctx->frame_ct_in == vtctx->frame_ct_out) {
        *buf = NULL;

        pthread_mutex_unlock(&vtctx->lock);
        return 0;
    }

    while (!vtctx->q_head && !vtctx->async_error && wait && !vtctx->flushing) {
        pthread_cond_wait(&vtctx->cv_sample_sent, &vtctx->lock);
    }

    if (!vtctx->q_head) {
        pthread_mutex_unlock(&vtctx->lock);
        *buf = NULL;
        return 0;
    }

    info = vtctx->q_head;
    vtctx->q_head = vtctx->q_head->next;
    if (!vtctx->q_head) {
        vtctx->q_tail = NULL;
    }

    vtctx->frame_ct_out++;
    pthread_mutex_unlock(&vtctx->lock);

    *buf = info->cm_buffer;
    info->cm_buffer = NULL;
    if (sei && *buf) {
        *sei = info->sei;
        info->sei = (ExtraSEI) {0};
    }
    vtenc_free_buf_node(info);

    return 0;
}

static void vtenc_q_push(VTEncContext *vtctx, BufNode *info)
{
    pthread_mutex_lock(&vtctx->lock);

    if (!vtctx->q_head) {
        vtctx->q_head = info;
    } else {
        vtctx->q_tail->next = info;
    }

    vtctx->q_tail = info;

    pthread_cond_signal(&vtctx->cv_sample_sent);
    pthread_mutex_unlock(&vtctx->lock);
}

static int count_nalus(size_t length_code_size,
                       CMSampleBufferRef sample_buffer,
                       int *count)
{
    size_t offset = 0;
    int status;
    int nalu_ct = 0;
    uint8_t size_buf[4];
    size_t src_size = CMSampleBufferGetTotalSampleSize(sample_buffer);
    CMBlockBufferRef block = CMSampleBufferGetDataBuffer(sample_buffer);

    if (length_code_size > 4)
        return AVERROR_INVALIDDATA;

    while (offset < src_size) {
        size_t curr_src_len;
        size_t box_len = 0;
        size_t i;

        status = CMBlockBufferCopyDataBytes(block,
                                            offset,
                                            length_code_size,
                                            size_buf);

        if (status != kCMBlockBufferNoErr) {
            return AVERROR_EXTERNAL;
        }

        for (i = 0; i < length_code_size; i++) {
            box_len <<= 8;
            box_len |= size_buf[i];
        }

        curr_src_len = box_len + length_code_size;
        offset += curr_src_len;

        nalu_ct++;
    }

    *count = nalu_ct;
    return 0;
}

static CMVideoCodecType get_cm_codec_type(AVCodecContext *avctx,
                                          int profile,
                                          double alpha_quality)
{
    const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(avctx->pix_fmt == AV_PIX_FMT_VIDEOTOOLBOX ? avctx->sw_pix_fmt : avctx->pix_fmt);
    switch (avctx->codec_id) {
    case AV_CODEC_ID_H264: return kCMVideoCodecType_H264;
    case AV_CODEC_ID_HEVC:
        if (desc && (desc->flags & AV_PIX_FMT_FLAG_ALPHA) && alpha_quality > 0.0) {
            return kCMVideoCodecType_HEVCWithAlpha;
        }
        return kCMVideoCodecType_HEVC;
    case AV_CODEC_ID_PRORES:
        if (desc && (desc->flags & AV_PIX_FMT_FLAG_ALPHA))
            avctx->bits_per_coded_sample = 32;
        switch (profile) {
        case AV_PROFILE_PRORES_PROXY:
            return MKBETAG('a','p','c','o'); // kCMVideoCodecType_AppleProRes422Proxy
        case AV_PROFILE_PRORES_LT:
            return MKBETAG('a','p','c','s'); // kCMVideoCodecType_AppleProRes422LT
        case AV_PROFILE_PRORES_STANDARD:
            return MKBETAG('a','p','c','n'); // kCMVideoCodecType_AppleProRes422
        case AV_PROFILE_PRORES_HQ:
            return MKBETAG('a','p','c','h'); // kCMVideoCodecType_AppleProRes422HQ
        case AV_PROFILE_PRORES_4444:
            return MKBETAG('a','p','4','h'); // kCMVideoCodecType_AppleProRes4444
        case AV_PROFILE_PRORES_XQ:
            return MKBETAG('a','p','4','x'); // kCMVideoCodecType_AppleProRes4444XQ

        default:
            av_log(avctx, AV_LOG_ERROR, "Unknown profile ID: %d, using auto\n", profile);
        case AV_PROFILE_UNKNOWN:
            if (desc &&
                ((desc->flags & AV_PIX_FMT_FLAG_ALPHA) ||
                  desc->log2_chroma_w == 0))
                return MKBETAG('a','p','4','h'); // kCMVideoCodecType_AppleProRes4444
            else
                return MKBETAG('a','p','c','n'); // kCMVideoCodecType_AppleProRes422
        }
    default:               return 0;
    }
}

/**
 * Get the parameter sets from a CMSampleBufferRef.
 * @param dst If *dst isn't NULL, the parameters are copied into existing
 *            memory. *dst_size must be set accordingly when *dst != NULL.
 *            If *dst is NULL, it will be allocated.
 *            In all cases, *dst_size is set to the number of bytes used starting
 *            at *dst.
 */
static int get_params_size(
    AVCodecContext              *avctx,
    CMVideoFormatDescriptionRef vid_fmt,
    size_t                      *size)
{
    VTEncContext *vtctx = avctx->priv_data;
    size_t total_size = 0;
    size_t ps_count;
    int is_count_bad = 0;
    size_t i;
    int status;
    status = vtctx->get_param_set_func(vid_fmt,
                                       0,
                                       NULL,
                                       NULL,
                                       &ps_count,
                                       NULL);
    if (status) {
        is_count_bad = 1;
        ps_count     = 0;
        status       = 0;
    }

    for (i = 0; i < ps_count || is_count_bad; i++) {
        const uint8_t *ps;
        size_t ps_size;
        status = vtctx->get_param_set_func(vid_fmt,
                                           i,
                                           &ps,
                                           &ps_size,
                                           NULL,
                                           NULL);
        if (status) {
            /*
             * When ps_count is invalid, status != 0 ends the loop normally
             * unless we didn't get any parameter sets.
             */
            if (i > 0 && is_count_bad) status = 0;

            break;
        }

        total_size += ps_size + sizeof(start_code);
    }

    if (status) {
        av_log(avctx, AV_LOG_ERROR, "Error getting parameter set sizes: %d\n", status);
        return AVERROR_EXTERNAL;
    }

    *size = total_size;
    return 0;
}

static int copy_param_sets(
    AVCodecContext              *avctx,
    CMVideoFormatDescriptionRef vid_fmt,
    uint8_t                     *dst,
    size_t                      dst_size)
{
    VTEncContext *vtctx = avctx->priv_data;
    size_t ps_count;
    int is_count_bad = 0;
    int status;
    size_t offset = 0;
    size_t i;

    status = vtctx->get_param_set_func(vid_fmt,
                                       0,
                                       NULL,
                                       NULL,
                                       &ps_count,
                                       NULL);
    if (status) {
        is_count_bad = 1;
        ps_count     = 0;
        status       = 0;
    }


    for (i = 0; i < ps_count || is_count_bad; i++) {
        const uint8_t *ps;
        size_t ps_size;
        size_t next_offset;

        status = vtctx->get_param_set_func(vid_fmt,
                                           i,
                                           &ps,
                                           &ps_size,
                                           NULL,
                                           NULL);
        if (status) {
            if (i > 0 && is_count_bad) status = 0;

            break;
        }

        next_offset = offset + sizeof(start_code) + ps_size;
        if (dst_size < next_offset) {
            av_log(avctx, AV_LOG_ERROR, "Error: buffer too small for parameter sets.\n");
            return AVERROR_BUFFER_TOO_SMALL;
        }

        memcpy(dst + offset, start_code, sizeof(start_code));
        offset += sizeof(start_code);

        memcpy(dst + offset, ps, ps_size);
        offset = next_offset;
    }

    if (status) {
        av_log(avctx, AV_LOG_ERROR, "Error getting parameter set data: %d\n", status);
        return AVERROR_EXTERNAL;
    }

    return 0;
}

static int set_extradata(AVCodecContext *avctx, CMSampleBufferRef sample_buffer)
{
    VTEncContext *vtctx = avctx->priv_data;
    CMVideoFormatDescriptionRef vid_fmt;
    size_t total_size;
    int status;

    vid_fmt = CMSampleBufferGetFormatDescription(sample_buffer);
    if (!vid_fmt) {
        av_log(avctx, AV_LOG_ERROR, "No video format.\n");
        return AVERROR_EXTERNAL;
    }

    if (vtctx->get_param_set_func) {
        status = get_params_size(avctx, vid_fmt, &total_size);
        if (status) {
            av_log(avctx, AV_LOG_ERROR, "Could not get parameter sets.\n");
            return status;
        }

        avctx->extradata = av_mallocz(total_size + AV_INPUT_BUFFER_PADDING_SIZE);
        if (!avctx->extradata) {
            return AVERROR(ENOMEM);
        }
        avctx->extradata_size = total_size;

        status = copy_param_sets(avctx, vid_fmt, avctx->extradata, total_size);

        if (status) {
            av_log(avctx, AV_LOG_ERROR, "Could not copy param sets.\n");
            return status;
        }
    } else {
        CFDataRef data = CMFormatDescriptionGetExtension(vid_fmt, kCMFormatDescriptionExtension_VerbatimSampleDescription);
        if (data && CFGetTypeID(data) == CFDataGetTypeID()) {
            CFIndex size = CFDataGetLength(data);

            avctx->extradata = av_mallocz(size + AV_INPUT_BUFFER_PADDING_SIZE);
            if (!avctx->extradata)
                return AVERROR(ENOMEM);
            avctx->extradata_size = size;

            CFDataGetBytes(data, CFRangeMake(0, size), avctx->extradata);
        }
    }

    return 0;
}

static void vtenc_output_callback(
    void *ctx,
    void *sourceFrameCtx,
    OSStatus status,
    VTEncodeInfoFlags flags,
    CMSampleBufferRef sample_buffer)
{
    AVCodecContext *avctx = ctx;
    VTEncContext   *vtctx = avctx->priv_data;
    BufNode *info = sourceFrameCtx;

    av_buffer_unref(&info->frame_buf);
    if (vtctx->async_error) {
        vtenc_free_buf_node(info);
        return;
    }

    if (status) {
        vtenc_free_buf_node(info);
        av_log(avctx, AV_LOG_ERROR, "Error encoding frame: %d\n", (int)status);
        set_async_error(vtctx, AVERROR_EXTERNAL);
        return;
    }

    if (!sample_buffer) {
        return;
    }

    CFRetain(sample_buffer);
    info->cm_buffer = sample_buffer;

    if (!avctx->extradata && (avctx->flags & AV_CODEC_FLAG_GLOBAL_HEADER)) {
        int set_status = set_extradata(avctx, sample_buffer);
        if (set_status) {
            vtenc_free_buf_node(info);
            set_async_error(vtctx, set_status);
            return;
        }
    }

    vtenc_q_push(vtctx, info);
}

static int get_length_code_size(
    AVCodecContext    *avctx,
    CMSampleBufferRef sample_buffer,
    size_t            *size)
{
    VTEncContext *vtctx = avctx->priv_data;
    CMVideoFormatDescriptionRef vid_fmt;
    int isize;
    int status;

    vid_fmt = CMSampleBufferGetFormatDescription(sample_buffer);
    if (!vid_fmt) {
        av_log(avctx, AV_LOG_ERROR, "Error getting buffer format description.\n");
        return AVERROR_EXTERNAL;
    }

    status = vtctx->get_param_set_func(vid_fmt,
                                       0,
                                       NULL,
                                       NULL,
                                       NULL,
                                       &isize);
    if (status) {
        av_log(avctx, AV_LOG_ERROR, "Error getting length code size: %d\n", status);
        return AVERROR_EXTERNAL;
    }

    *size = isize;
    return 0;
}

/*
 * Returns true on success.
 *
 * If profile_level_val is NULL and this method returns true, don't specify the
 * profile/level to the encoder.
 */
static bool get_vt_h264_profile_level(AVCodecContext *avctx,
                                      CFStringRef    *profile_level_val)
{
    VTEncContext *vtctx = avctx->priv_data;
    int profile = vtctx->profile;

    if (profile == AV_PROFILE_UNKNOWN && vtctx->level) {
        //Need to pick a profile if level is not auto-selected.
        profile = vtctx->has_b_frames ? AV_PROFILE_H264_MAIN : AV_PROFILE_H264_BASELINE;
    }

    *profile_level_val = NULL;

    switch (profile) {
        case AV_PROFILE_UNKNOWN:
            return true;

        case AV_PROFILE_H264_BASELINE:
            switch (vtctx->level) {
                case  0: *profile_level_val =
                                  compat_keys.kVTProfileLevel_H264_Baseline_AutoLevel; break;
                case 13: *profile_level_val = kVTProfileLevel_H264_Baseline_1_3;       break;
                case 30: *profile_level_val = kVTProfileLevel_H264_Baseline_3_0;       break;
                case 31: *profile_level_val = kVTProfileLevel_H264_Baseline_3_1;       break;
                case 32: *profile_level_val = kVTProfileLevel_H264_Baseline_3_2;       break;
                case 40: *profile_level_val =
                                  compat_keys.kVTProfileLevel_H264_Baseline_4_0;       break;
                case 41: *profile_level_val = kVTProfileLevel_H264_Baseline_4_1;       break;
                case 42: *profile_level_val =
                                  compat_keys.kVTProfileLevel_H264_Baseline_4_2;       break;
                case 50: *profile_level_val =
                                  compat_keys.kVTProfileLevel_H264_Baseline_5_0;       break;
                case 51: *profile_level_val =
                                  compat_keys.kVTProfileLevel_H264_Baseline_5_1;       break;
                case 52: *profile_level_val =
                                  compat_keys.kVTProfileLevel_H264_Baseline_5_2;       break;
            }
            break;

        case AV_PROFILE_H264_CONSTRAINED_BASELINE:
            *profile_level_val =  compat_keys.kVTProfileLevel_H264_ConstrainedBaseline_AutoLevel;

            if (vtctx->level != 0) {
                av_log(avctx,
                       AV_LOG_WARNING,
                       "Level is auto-selected when constrained-baseline "
                       "profile is used. The output may be encoded with a "
                       "different level.\n");
            }
            break;

        case AV_PROFILE_H264_MAIN:
            switch (vtctx->level) {
                case  0: *profile_level_val =
                                  compat_keys.kVTProfileLevel_H264_Main_AutoLevel; break;
                case 30: *profile_level_val = kVTProfileLevel_H264_Main_3_0;       break;
                case 31: *profile_level_val = kVTProfileLevel_H264_Main_3_1;       break;
                case 32: *profile_level_val = kVTProfileLevel_H264_Main_3_2;       break;
                case 40: *profile_level_val = kVTProfileLevel_H264_Main_4_0;       break;
                case 41: *profile_level_val = kVTProfileLevel_H264_Main_4_1;       break;
                case 42: *profile_level_val =
                                  compat_keys.kVTProfileLevel_H264_Main_4_2;       break;
                case 50: *profile_level_val = kVTProfileLevel_H264_Main_5_0;       break;
                case 51: *profile_level_val =
                                  compat_keys.kVTProfileLevel_H264_Main_5_1;       break;
                case 52: *profile_level_val =
                                  compat_keys.kVTProfileLevel_H264_Main_5_2;       break;
            }
            break;

        case H264_PROFILE_CONSTRAINED_HIGH:
            *profile_level_val =  compat_keys.kVTProfileLevel_H264_ConstrainedHigh_AutoLevel;

            if (vtctx->level != 0) {
                av_log(avctx,
                      AV_LOG_WARNING,
                       "Level is auto-selected when constrained-high profile "
                       "is used. The output may be encoded with a different "
                       "level.\n");
            }
            break;

        case AV_PROFILE_H264_HIGH:
            switch (vtctx->level) {
                case  0: *profile_level_val =
                                  compat_keys.kVTProfileLevel_H264_High_AutoLevel; break;
                case 30: *profile_level_val =
                                  compat_keys.kVTProfileLevel_H264_High_3_0;       break;
                case 31: *profile_level_val =
                                  compat_keys.kVTProfileLevel_H264_High_3_1;       break;
                case 32: *profile_level_val =
                                  compat_keys.kVTProfileLevel_H264_High_3_2;       break;
                case 40: *profile_level_val =
                                  compat_keys.kVTProfile
