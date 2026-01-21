Do not include any other text.

----- BEGIN SOLUTION -----
/*
 * Copyright (c) 2007 Bobby Bingham
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
 * scale video filter
 */

#include <float.h>
#include <stdio.h>
#include <string.h>

#include "avfilter.h"
#include "filters.h"
#include "formats.h"
#include "framesync.h"
#include "libavutil/pixfmt.h"
#include "scale_eval.h"
#include "video.h"
#include "libavutil/eval.h"
#include "libavutil/imgutils_internal.h"
#include "libavutil/internal.h"
#include "libavutil/mem.h"
#include "libavutil/opt.h"
#include "libavutil/parseutils.h"
#include "libavutil/pixdesc.h"
#include "libswscale/swscale.h"

static const char *const var_names[] = {
    "in_w",   "iw",
    "in_h",   "ih",
    "out_w",  "ow",
    "out_h",  "oh",
    "a",
    "sar",
    "dar",
    "hsub",
    "vsub",
    "ohsub",
    "ovsub",
    "n",
    "t",
#if FF_API_FRAME_PKT
    "pos",
#endif
    "ref_w", "rw",
    "ref_h", "rh",
    "ref_a",
    "ref_sar",
    "ref_dar", "rdar",
    "ref_hsub",
    "ref_vsub",
    "ref_n",
    "ref_t",
    "ref_pos",
    /* Legacy variables for scale2ref */
    "main_w",
    "main_h",
    "main_a",
    "main_sar",
    "main_dar", "mdar",
    "main_hsub",
    "main_vsub",
    "main_n",
    "main_t",
    "main_pos",
    NULL
};

enum var_name {
    VAR_IN_W,   VAR_IW,
    VAR_IN_H,   VAR_IH,
    VAR_OUT_W,  VAR_OW,
    VAR_OUT_H,  VAR_OH,
    VAR_A,
    VAR_SAR,
    VAR_DAR,
    VAR_HSUB,
    VAR_VSUB,
    VAR_OHSUB,
    VAR_OVSUB,
    VAR_N,
    VAR_T,
#if FF_API_FRAME_PKT
    VAR_POS,
#endif
    VAR_REF_W, VAR_RW,
    VAR_REF_H, VAR_RH,
    VAR_REF_A,
    VAR_REF_SAR,
    VAR_REF_DAR, VAR_RDAR,
    VAR_REF_HSUB,
    VAR_REF_VSUB,
    VAR_REF_N,
    VAR_REF_T,
    VAR_REF_POS,
    VAR_S2R_MAIN_W,
    VAR_S2R_MAIN_H,
    VAR_S2R_MAIN_A,
    VAR_S2R_MAIN_SAR,
    VAR_S2R_MAIN_DAR, VAR_S2R_MDAR,
    VAR_S2R_MAIN_HSUB,
    VAR_S2R_MAIN_VSUB,
    VAR_S2R_MAIN_N,
    VAR_S2R_MAIN_T,
    VAR_S2R_MAIN_POS,
    VARS_NB
};

enum EvalMode {
    EVAL_MODE_INIT,
    EVAL_MODE_FRAME,
    EVAL_MODE_NB
};

typedef struct ScaleContext {
    const AVClass *class;
    SwsContext *sws;
    FFFrameSync fs;

    /**
     * New dimensions. Special values are:
     *   0 = original width/height
     *  -1 = keep original aspect
     *  -N = try to keep aspect but make sure it is divisible by N
     */
    int w, h;
    char *size_str;
    double param[2];            // sws params

    int hsub, vsub;             ///< chroma subsampling
    int slice_y;                ///< top of current output slice
    int interlaced;
    int uses_ref;

    char *w_expr;               ///< width  expression string
    char *h_expr;               ///< height expression string
    AVExpr *w_pexpr;
    AVExpr *h_pexpr;
    double var_values[VARS_NB];

    char *flags_str;

    int in_color_matrix;
    int out_color_matrix;
    int in_primaries;
    int out_primaries;
    int in_transfer;
    int out_transfer;
    int in_range;
    int out_range;

    int in_chroma_loc;
    int out_chroma_loc;
    int out_h_chr_pos;
    int out_v_chr_pos;
    int in_h_chr_pos;
    int in_v_chr_pos;

    int force_original_aspect_ratio;
    int force_divisible_by;

    int eval_mode;              ///< expression evaluation mode

} ScaleContext;

const FFFilter ff_vf_scale2ref;
#define IS_SCALE2REF(ctx) ((ctx)->filter == &ff_vf_scale2ref.p)

static int config_props(AVFilterLink *outlink);

static int check_exprs(AVFilterContext *ctx)
{
    ScaleContext *scale = ctx->priv;
    unsigned vars_w[VARS_NB] = { 0 }, vars_h[VARS_NB] = { 0 };

    if (!scale->w_pexpr && !scale->h_pexpr)
        return AVERROR(EINVAL);

    if (scale->w_pexpr)
        av_expr_count_vars(scale->w_pexpr, vars_w, VARS_NB);
    if (scale->h_pexpr)
        av_expr_count_vars(scale->h_pexpr, vars_h, VARS_NB);

    if (vars_w[VAR_OUT_W] || vars_w[VAR_OW]) {
        av_log(ctx, AV_LOG_ERROR, "Width expression cannot be self-referencing: '%s'.\n", scale->w_expr);
        return AVERROR(EINVAL);
    }

    if (vars_h[VAR_OUT_H] || vars_h[VAR_OH]) {
        av_log(ctx, AV_LOG_ERROR, "Height expression cannot be self-referencing: '%s'.\n", scale->h_expr);
        return AVERROR(EINVAL);
    }

    if ((vars_w[VAR_OUT_H] || vars_w[VAR_OH]) &&
        (vars_h[VAR_OUT_W] || vars_h[VAR_OW])) {
        av_log(ctx, AV_LOG_WARNING, "Circular references detected for width '%s' and height '%s' - possibly invalid.\n", scale->w_expr, scale->h_expr);
    }

    if (vars_w[VAR_REF_W]    || vars_h[VAR_REF_W]    ||
        vars_w[VAR_RW]       || vars_h[VAR_RW]       ||
        vars_w[VAR_REF_H]    || vars_h[VAR_REF_H]    ||
        vars_w[VAR_RH]       || vars_h[VAR_RH]       ||
        vars_w[VAR_REF_A]    || vars_h[VAR_REF_A]    ||
        vars_w[VAR_REF_SAR]  || vars_h[VAR_REF_SAR]  ||
        vars_w[VAR_REF_DAR]  || vars_h[VAR_REF_DAR]  ||
        vars_w[VAR_RDAR]     || vars_h[VAR_RDAR]     ||
        vars_w[VAR_REF_HSUB] || vars_h[VAR_REF_HSUB] ||
        vars_w[VAR_REF_VSUB] || vars_h[VAR_REF_VSUB] ||
        vars_w[VAR_REF_N]    || vars_h[VAR_REF_N]    ||
        vars_w[VAR_REF_T]    || vars_h[VAR_REF_T]    ||
        vars_w[VAR_REF_POS]  || vars_h[VAR_REF_POS]) {
        scale->uses_ref = 1;
    }

    if (!IS_SCALE2REF(ctx) &&
        (vars_w[VAR_S2R_MAIN_W]    || vars_h[VAR_S2R_MAIN_W]    ||
         vars_w[VAR_S2R_MAIN_H]    || vars_h[VAR_S2R_MAIN_H]    ||
         vars_w[VAR_S2R_MAIN_A]    || vars_h[VAR_S2R_MAIN_A]    ||
         vars_w[VAR_S2R_MAIN_SAR]  || vars_h[VAR_S2R_MAIN_SAR]  ||
         vars_w[VAR_S2R_MAIN_DAR]  || vars_h[VAR_S2R_MAIN_DAR]  ||
         vars_w[VAR_S2R_MDAR]      || vars_h[VAR_S2R_MDAR]      ||
         vars_w[VAR_S2R_MAIN_HSUB] || vars_h[VAR_S2R_MAIN_HSUB] ||
         vars_w[VAR_S2R_MAIN_VSUB] || vars_h[VAR_S2R_MAIN_VSUB] ||
         vars_w[VAR_S2R_MAIN_N]    || vars_h[VAR_S2R_MAIN_N]    ||
         vars_w[VAR_S2R_MAIN_T]    || vars_h[VAR_S2R_MAIN_T]    ||
         vars_w[VAR_S2R_MAIN_POS]  || vars_h[VAR_S2R_MAIN_POS]) ) {
        av_log(ctx, AV_LOG_ERROR, "Expressions with scale2ref variables are not valid in scale filter.\n");
        return AVERROR(EINVAL);
    }

    if (!IS_SCALE2REF(ctx) &&
        (vars_w[VAR_S2R_MAIN_W]    || vars_h[VAR_S2R_MAIN_W]    ||
         vars_w[VAR_S2R_MAIN_H]    || vars_h[VAR_S2R_MAIN_H]    ||
         vars_w[VAR_S2R_MAIN_A]    || vars_h[VAR_S2R_MAIN_A]    ||
         vars_w[VAR_S2R_MAIN_SAR]  || vars_h[VAR_S2R_MAIN_SAR]  ||
         vars_w[VAR_S2R_MAIN_DAR]  || vars_h[VAR_S2R_MAIN_DAR]  ||
         vars_w[VAR_S2R_MDAR]      || vars_h[VAR_S2R_MDAR]      ||
         vars_w[VAR_S2R_MAIN_HSUB] || vars_h[VAR_S2R_MAIN_HSUB] ||
         vars_w[VAR_S2R_MAIN_VSUB] || vars_h[VAR_S2R_MAIN_VSUB] ||
         vars_w[VAR_S2R_MAIN_N]    || vars_h[VAR_S2R_MAIN_N]    ||
         vars_w[VAR_S2R_MAIN_T]    || vars_h[VAR_S2R_MAIN_T]    ||
         vars_w[VAR_S2R_MAIN_POS]  || vars_h[VAR_S2R_MAIN_POS]) ) {
        av_log(ctx, AV_LOG_ERROR, "Expressions with scale2ref variables are not valid in scale filter.\n");
        return AVERROR(EINVAL);
    }

    if (scale->eval_mode == EVAL_MODE_INIT &&
        (vars_w[VAR_N]            || vars_h[VAR_N]           ||
         vars_w[VAR_T]            || vars_h[VAR_T]           ||
#if FF_API_FRAME_PKT
         vars_w[VAR_POS]          || vars_h[VAR_POS]         ||
#endif
         vars_w[VAR_S2R_MAIN_N]   || vars_h[VAR_S2R_MAIN_N]  ||
         vars_w[VAR_S2R_MAIN_T]   || vars_h[VAR_S2R_MAIN_T]  ||
         vars_w[VAR_S2R_MAIN_POS] || vars_h[VAR_S2R_MAIN_POS]) ) {
        av_log(ctx, AV_LOG_ERROR, "Expressions with frame variables 'n', 't', 'pos' are not valid in init eval_mode.\n");
        return AVERROR(EINVAL);
    }

    return 0;
}

static int scale_parse_expr(AVFilterContext *ctx, char *str_expr, AVExpr **pexpr_ptr, const char *var, const char *args)
{
    ScaleContext *scale = ctx->priv;
    int ret, is_inited = 0;
    char *old_str_expr = NULL;
    AVExpr *old_pexpr = NULL;

    if (str_expr) {
        old_str_expr = av_strdup(str_expr);
        if (!old_str_expr)
            return AVERROR(ENOMEM);
        av_opt_set(scale, var, args, 0);
    }

    if (*pexpr_ptr) {
        old_pexpr = *pexpr_ptr;
        *pexpr_ptr = NULL;
        is_inited = 1;
    }

    ret = av_expr_parse(pexpr_ptr, args, var_names,
                        NULL, NULL, NULL, NULL, 0, ctx);
    if (ret < 0) {
        av_log(ctx, AV_LOG_ERROR, "Cannot parse expression for %s: '%s'\n", var, args);
        goto revert;
    }

    ret = check_exprs(ctx);
    if (ret < 0)
        goto revert;

    if (is_inited && (ret = config_props(ctx->outputs[0])) < 0)
        goto revert;

    av_expr_free(old_pexpr);
    old_pexpr = NULL;
    av_freep(&old_str_expr);

    return 0;

revert:
    av_expr_free(*pexpr_ptr);
    *pexpr_ptr = NULL;
    if (old_str_expr) {
        av_opt_set(scale, var, old_str_expr, 0);
        av_free(old_str_expr);
    }
    if (old_pexpr)
        *pexpr_ptr = old_pexpr;

    return ret;
}

static av_cold int preinit(AVFilterContext *ctx)
{
    ScaleContext *scale = ctx->priv;

    scale->sws = sws_alloc_context();
    if (!scale->sws)
        return AVERROR(ENOMEM);

    // set threads=0, so we can later check whether the user modified it
    scale->sws->threads = 0;

    ff_framesync_preinit(&scale->fs);

    return 0;
}

static int do_scale(FFFrameSync *fs);

static av_cold int init(AVFilterContext *ctx)
{
    ScaleContext *scale = ctx->priv;
    int ret;

    if (IS_SCALE2REF(ctx))
        av_log(ctx, AV_LOG_WARNING, "scale2ref is deprecated, use scale=rw:rh instead\n");

    if (scale->size_str && (scale->w_expr || scale->h_expr)) {
        av_log(ctx, AV_LOG_ERROR,
               "Size and width/height expressions cannot be set at the same time.\n");
            return AVERROR(EINVAL);
    }

    if (scale->w_expr && !scale->h_expr)
        FFSWAP(char *, scale->w_expr, scale->size_str);

    if (scale->size_str) {
        char buf[32];
        if ((ret = av_parse_video_size(&scale->w, &scale->h, scale->size_str)) < 0) {
            av_log(ctx, AV_LOG_ERROR,
                   "Invalid size '%s'\n", scale->size_str);
            return ret;
        }
        snprintf(buf, sizeof(buf)-1, "%d", scale->w);
        av_opt_set(scale, "w", buf, 0);
        snprintf(buf, sizeof(buf)-1, "%d", scale->h);
        av_opt_set(scale, "h", buf, 0);
    }
    if (!scale->w_expr)
        av_opt_set(scale, "w", "iw", 0);
    if (!scale->h_expr)
        av_opt_set(scale, "h", "ih", 0);

    ret = scale_parse_expr(ctx, NULL, &scale->w_pexpr, "width", scale->w_expr);
    if (ret < 0)
        return ret;

    ret = scale_parse_expr(ctx, NULL, &scale->h_pexpr, "height", scale->h_expr);
    if (ret < 0)
        return ret;

    if (scale->in_primaries != -1 && !sws_test_primaries(scale->in_primaries, 0)) {
        av_log(ctx, AV_LOG_ERROR, "Unsupported input primaries '%s'\n",
               av_color_primaries_name(scale->in_primaries));
        return AVERROR(EINVAL);
    }

    if (scale->out_primaries != -1 && !sws_test_primaries(scale->out_primaries, 1)) {
        av_log(ctx, AV_LOG_ERROR, "Unsupported output primaries '%s'\n",
               av_color_primaries_name(scale->out_primaries));
        return AVERROR(EINVAL);
    }

    if (scale->in_transfer != -1 && !sws_test_transfer(scale->in_transfer, 0)) {
        av_log(ctx, AV_LOG_ERROR, "Unsupported input transfer '%s'\n",
               av_color_transfer_name(scale->in_transfer));
        return AVERROR(EINVAL);
    }

    if (scale->out_transfer != -1 && !sws_test_transfer(scale->out_transfer, 1)) {
        av_log(ctx, AV_LOG_ERROR, "Unsupported output transfer '%s'\n",
               av_color_transfer_name(scale->out_transfer));
        return AVERROR(EINVAL);
    }

    if (scale->in_color_matrix != -1 && !sws_test_colorspace(scale->in_color_matrix, 0)) {
        av_log(ctx, AV_LOG_ERROR, "Unsupported input color matrix '%s'\n",
               av_color_space_name(scale->in_color_matrix));
        return AVERROR(EINVAL);
    }

    if (scale->out_color_matrix != -1 && !sws_test_colorspace(scale->out_color_matrix, 1)) {
        av_log(ctx, AV_LOG_ERROR, "Unsupported output color matrix '%s'\n",
               av_color_space_name(scale->out_color_matrix));
        return AVERROR(EINVAL);
    }

    av_log(ctx, AV_LOG_VERBOSE, "w:%s h:%s flags:'%s' interl:%d\n",
           scale->w_expr, scale->h_expr, (char *)av_x_if_null(scale->flags_str, ""), scale->interlaced);

    if (scale->flags_str && *scale->flags_str) {
        ret = av_opt_set(scale->sws, "sws_flags", scale->flags_str, 0);
        if (ret < 0)
            return ret;
    }

    for (int i = 0; i < FF_ARRAY_ELEMS(scale->param); i++)
        if (scale->param[i] != DBL_MAX)
            scale->sws->scaler_params[i] = scale->param[i];

    scale->sws->src_h_chr_pos = scale->in_h_chr_pos;
    scale->sws->src_v_chr_pos = scale->in_v_chr_pos;
    scale->sws->dst_h_chr_pos = scale->out_h_chr_pos;
    scale->sws->dst_v_chr_pos = scale->out_v_chr_pos;

    // use generic thread-count if the user did not set it explicitly
    if (!scale->sws->threads)
        scale->sws->threads = ff_filter_get_nb_threads(ctx);

    if (!IS_SCALE2REF(ctx) && scale->uses_ref) {
        AVFilterPad pad = {
            .name = "ref",
            .type = AVMEDIA_TYPE_VIDEO,
        };
        ret = ff_append_inpad(ctx, &pad);
        if (ret < 0)
            return ret;
    }

    return 0;
}

static av_cold void uninit(AVFilterContext *ctx)
{
    ScaleContext *scale = ctx->priv;
    av_expr_free(scale->w_pexpr);
    av_expr_free(scale->h_pexpr);
    scale->w_pexpr = scale->h_pexpr = NULL;
    ff_framesync_uninit(&scale->fs);
    sws_free_context(&scale->sws);
}

static int query_formats(const AVFilterContext *ctx,
                         AVFilterFormatsConfig **cfg_in,
                         AVFilterFormatsConfig **cfg_out)
{
    const ScaleContext *scale = ctx->priv;
    AVFilterFormats *formats;
    const AVPixFmtDescriptor *desc;
    enum AVPixelFormat pix_fmt;
    int ret;

    desc    = NULL;
    formats = NULL;
    while ((desc = av_pix_fmt_desc_next(desc))) {
        pix_fmt = av_pix_fmt_desc_get_id(desc);
        if (sws_test_format(pix_fmt, 0)) {
            if ((ret = ff_add_format(&formats, pix_fmt)) < 0)
                return ret;
        }
    }
    if ((ret = ff_formats_ref(formats, &cfg_in[0]->formats)) < 0)
        return ret;

    desc    = NULL;
    formats = NULL;
    while ((desc = av_pix_fmt_desc_next(desc))) {
        pix_fmt = av_pix_fmt_desc_get_id(desc);
        if (sws_test_format(pix_fmt, 1) || pix_fmt == AV_PIX_FMT_PAL8) {
            if ((ret = ff_add_format(&formats, pix_fmt)) < 0)
                return ret;
        }
    }
    if ((ret = ff_formats_ref(formats, &cfg_out[0]->formats)) < 0)
        return ret;

    /* accept all supported inputs, even if user overrides their properties */
    formats = ff_all_color_spaces();
    for (int i = 0; i < formats->nb_formats; i++) {
        if (!sws_test_colorspace(formats->formats[i], 0)) {
            for (int j = i--; j + 1 < formats->nb_formats; j++)
                formats->formats[j] = formats->formats[j + 1];
            formats->nb_formats--;
        }
    }
    if ((ret = ff_formats_ref(formats, &cfg_in[0]->color_spaces)) < 0)
        return ret;

    if ((ret = ff_formats_ref(ff_all_color_ranges(),
                              &cfg_in[0]->color_ranges)) < 0)
        return ret;

    /* propagate output properties if overridden */
    if (scale->out_color_matrix != AVCOL_SPC_UNSPECIFIED) {
        formats = ff_make_formats_list_singleton(scale->out_color_matrix);
    } else {
        formats = ff_all_color_spaces();
        for (int i = 0; i < formats->nb_formats; i++) {
            if (!sws_test_colorspace(formats->formats[i], 1)) {
                for (int j = i--; j + 1 < formats->nb_formats; j++)
                    formats->formats[j] = formats->formats[j + 1];
                formats->nb_formats--;
            }
        }
    }
    if ((ret = ff_formats_ref(formats, &cfg_out[0]->color_spaces)) < 0)
        return ret;

    formats = scale->out_range != AVCOL_RANGE_UNSPECIFIED
                ? ff_make_formats_list_singleton(scale->out_range)
                : ff_all_color_ranges();
    if ((ret = ff_formats_ref(formats, &cfg_out[0]->color_ranges)) < 0)
        return ret;

    return 0;
}

static int scale_eval_dimensions(AVFilterContext *ctx)
{
    ScaleContext *scale = ctx->priv;
    const char scale2ref = IS_SCALE2REF(ctx);
    const AVFilterLink *inlink = scale2ref ? ctx->inputs[1] : ctx->inputs[0];
    const AVFilterLink *outlink = ctx->outputs[0];
    const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(inlink->format);
    const AVPixFmtDescriptor *out_desc = av_pix_fmt_desc_get(outlink->format);
    char *expr;
    int eval_w, eval_h;
    int ret;
    double res;
    const AVPixFmtDescriptor *main_desc;
    const AVFilterLink *main_link;

    if (scale2ref) {
        main_link = ctx->inputs[0];
        main_desc = av_pix_fmt_desc_get(main_link->format);
    }

    scale->var_values[VAR_IN_W]  = scale->var_values[VAR_IW] = inlink->w;
    scale->var_values[VAR_IN_H]  = scale->var_values[VAR_IH] = inlink->h;
    scale->var_values[VAR_OUT_W] = scale->var_values[VAR_OW] = NAN;
    scale->var_values[VAR_OUT_H] = scale->var_values[VAR_OH] = NAN;
    scale->var_values[VAR_A]     = (double) inlink->w / inlink->h;
    scale->var_values[VAR_SAR]   = inlink->sample_aspect_ratio.num ?
        (double) inlink->sample_aspect_ratio.num / inlink->sample_aspect_ratio.den : 1;
    scale->var_values[VAR_DAR]   = scale->var_values[VAR_A] * scale->var_values[VAR_SAR];
    scale->var_values[VAR_HSUB]  = 1 << desc->log2_chroma_w;
    scale->var_values[VAR_VSUB]  = 1 << desc->log2_chroma_h;
    scale->var_values[VAR_OHSUB] = 1 << out_desc->log2_chroma_w;
    scale->var_values[VAR_OVSUB] = 1 << out_desc->log2_chroma_h;

    if (scale2ref) {
        scale->var_values[VAR_S2R_MAIN_W] = main_link->w;
        scale->var_values[VAR_S2R_MAIN_H] = main_link->h;
        scale->var_values[VAR_S2R_MAIN_A] = (double) main_link->w / main_link->h;
        scale->var_values[VAR_S2R_MAIN_SAR] = main_link->sample_aspect_ratio.num ?
            (double) main_link->sample_aspect_ratio.num / main_link->sample_aspect_ratio.den : 1;
        scale->var_values[VAR_S2R_MAIN_DAR] = scale->var_values[VAR_S2R_MDAR] =
            scale->var_values[VAR_S2R_MAIN_A] * scale->var_values[VAR_S2R_MAIN_SAR];
        scale->var_values[VAR_S2R_MAIN_HSUB] = 1 << main_desc->log2_chroma_w;
        scale->var_values[VAR_S2R_MAIN_VSUB] = 1 << main_desc->log2_chroma_h;
    }

    if (scale->uses_ref) {
        const AVFilterLink *reflink = ctx->inputs[1];
        const AVPixFmtDescriptor *ref_desc = av_pix_fmt_desc_get(reflink->format);
        scale->var_values[VAR_REF_W] = scale->var_values[VAR_RW] = reflink->w;
        scale->var_values[VAR_REF_H] = scale->var_values[VAR_RH] = reflink->h;
        scale->var_values[VAR_REF_A] = (double) reflink->w / reflink->h;
        scale->var_values[VAR_REF_SAR] = reflink->sample_aspect_ratio.num ?
            (double) reflink->sample_aspect_ratio.num / reflink->sample_aspect_ratio.den : 1;
        scale->var_values[VAR_REF_DAR] = scale->var_values[VAR_RDAR] =
            scale->var_values[VAR_REF_A] * scale->var_values[VAR_REF_SAR];
        scale->var_values[VAR_REF_HSUB] = 1 << ref_desc->log2_chroma_w;
        scale->var_values[VAR_REF_VSUB] = 1 << ref_desc->log2_chroma_h;
    }

    res = av_expr_eval(scale->w_pexpr, scale->var_values, NULL);
    eval_w = scale->var_values[VAR_OUT_W] = scale->var_values[VAR_OW] = (int) res == 0 ? inlink->w : (int) res;

    res = av_expr_eval(scale->h_pexpr, scale->var_values, NULL);
    if (isnan(res)) {
        expr = scale->h_expr;
        ret = AVERROR(EINVAL);
        goto fail;
    }
    eval_h = scale->var_values[VAR_OUT_H] = scale->var_values[VAR_OH] = (int) res == 0 ? inlink->h : (int) res;

    res = av_expr_eval(scale->w_pexpr, scale->var_values, NULL);
    if (isnan(res)) {
        expr = scale->w_expr;
        ret = AVERROR(EINVAL);
        goto fail;
    }
    eval_w = scale->var_values[VAR_OUT_W] = scale->var_values[VAR_OW] = (int) res == 0 ? inlink->w : (int) res;

    scale->w = eval_w;
    scale->h = eval_h;

    return 0;

fail:
    av_log(ctx, AV_LOG_ERROR,
           "Error when evaluating the expression '%s'.\n", expr);
    return ret;
}

static int config_props(AVFilterLink *outlink)
{
    AVFilterContext *ctx = outlink->src;
    AVFilterLink *inlink0 = outlink->src->inputs[0];
    AVFilterLink *inlink  = IS_SCALE2REF(ctx) ?
                            outlink->src->inputs[1] :
                            outlink->src->inputs[0];
    ScaleContext *scale = ctx->priv;
    uint8_t *flags_val = NULL;
    int ret;

    if ((ret = scale_eval_dimensions(ctx)) < 0)
        goto fail;

    outlink->w = scale->w;
    outlink->h = scale->h;

    ret = ff_scale_adjust_dimensions(inlink, &outlink->w, &outlink->h,
                               scale->force_original_aspect_ratio,
                               scale->force_divisible_by);

    if (ret < 0)
        goto fail;

    if (outlink->w > INT_MAX ||
        outlink->h > INT_MAX ||
        (outlink->h * inlink->w) > INT_MAX ||
        (outlink->w * inlink->h) > INT_MAX)
        av_log(ctx, AV_LOG_ERROR, "Rescaled value for width or height is too big.\n");

    /* TODO: make algorithm configurable */

    if (inlink0->sample_aspect_ratio.num){
        outlink->sample_aspect_ratio = av_mul_q((AVRational){outlink->h * inlink0->w, outlink->w * inlink0->h}, inlink0->sample_aspect_ratio);
    } else
        outlink->sample_aspect_ratio = inlink0->sample_aspect_ratio;

    av_opt_get(scale->sws, "sws_flags", 0, &flags_val);
    av_log(ctx, AV_LOG_VERBOSE, "w:%d h:%d fmt:%s csp:%s range:%s sar:%d/%d -> w:%d h:%d fmt:%s csp:%s range:%s sar:%d/%d flags:%s\n",
           inlink ->w, inlink ->h, av_get_pix_fmt_name( inlink->format),
           av_color_space_name(inlink->colorspace), av_color_range_name(inlink->color_range),
           inlink->sample_aspect_ratio.num, inlink->sample_aspect_ratio.den,
           outlink->w, outlink->h, av_get_pix_fmt_name(outlink->format),
           av_color_space_name(outlink->colorspace), av_color_range_name(outlink->color_range),
           outlink->sample_aspect_ratio.num, outlink->sample_aspect_ratio.den,
           flags_val);
    av_freep(&flags_val);

    if (!IS_SCALE2REF(ctx)) {
        ff_framesync_uninit(&scale->fs);
        ret = ff_framesync_init(&scale->fs, ctx, ctx->nb_inputs);
        if (ret < 0)
            return ret;
        scale->fs.on_event        = do_scale;
        scale->fs.in[0].time_base = ctx->inputs[0]->time_base;
        scale->fs.in[0].sync      = 1;
        scale->fs.in[0].before    = EXT_STOP;
        scale->fs.in[0].after     = EXT_STOP;
        if (scale->uses_ref) {
            av_assert0(ctx->nb_inputs == 2);
            scale->fs.in[1].time_base = ctx->inputs[1]->time_base;
            scale->fs.in[1].sync      = 0;
            scale->fs.in[1].before    = EXT_NULL;
            scale->fs.in[1].after     = EXT_INFINITY;
        }

        ret = ff_framesync_configure(&scale->fs);
        if (ret < 0)
            return ret;
    }

    return 0;

fail:
    return ret;
}

static int config_props_ref(AVFilterLink *outlink)
{
    AVFilterLink *inlink = outlink->src->inputs[1];
    FilterLink *il = ff_filter_link(inlink);
    FilterLink *ol = ff_filter_link(outlink);

    outlink->w = inlink->w;
    outlink->h = inlink->h;
    outlink->sample_aspect_ratio = inlink->sample_aspect_ratio;
    outlink->time_base = inlink->time_base;
    ol->frame_rate = il->frame_rate;
    outlink->colorspace = inlink->colorspace;
    outlink->color_range = inlink->color_range;

    return 0;
}

static int request_frame(AVFilterLink *outlink)
{
    return ff_request_frame(outlink->src->inputs[0]);
}

static int request_frame_ref(AVFilterLink *outlink)
{
    return ff_request_frame(outlink->src->inputs[1]);
}

/* Takes over ownership of *frame_in, passes ownership of *frame_out to caller */
static int scale_frame(AVFilterLink *link, AVFrame **frame_in,
                       AVFrame **frame_out)
{
    FilterLink *inl = ff_filter_link(link);
    AVFilterContext *ctx = link->dst;
    ScaleContext *scale = ctx->priv;
    AVFilterLink *outlink = ctx->outputs[0];
    AVFrame *out, *in = *frame_in;
    const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(link->format);
    char buf[32];
    int ret, flags_orig, frame_changed;

    *frame_in = NULL;

    frame_changed = in->width  != link->w ||
                    in->height != link->h ||
                    in->format != link->format ||
                    in->sample_aspect_ratio.den != link->sample_aspect_ratio.den ||
                    in->sample_aspect_ratio.num != link->sample_aspect_ratio.num ||
                    in->colorspace != link->colorspace ||
                    in->color_range != link->color_range;

    if (scale->eval_mode == EVAL_MODE_FRAME || frame_changed) {
        unsigned vars_w[VARS_NB] = { 0 }, vars_h[VARS_NB] = { 0 };

        av_expr_count_vars(scale->w_pexpr, vars_w, VARS_NB);
        av_expr_count_vars(scale->h_pexpr, vars_h, VARS_NB);

        if (scale->eval_mode == EVAL_MODE_FRAME &&
            !frame_changed &&
            !IS_SCALE2REF(ctx) &&
            !(vars_w[VAR_N] || vars_w[VAR_T]
#if FF_API_FRAME_PKT
              || vars_w[VAR_POS]
#endif
              ) &&
            !(vars_h[VAR_N] || vars_h[VAR_T]
#if FF_API_FRAME_PKT
              || vars_h[VAR_POS]
#endif
              ) &&
            scale->w && scale->h)
            goto scale;

        if (scale->eval_mode == EVAL_MODE_INIT) {
            snprintf(buf, sizeof(buf) - 1, "%d", scale->w);
            av_opt_set(scale, "w", buf, 0);
            snprintf(buf, sizeof(buf) - 1, "%d", scale->h);
            av_opt_set(scale, "h", buf, 0);

            ret = scale_parse_expr(ctx, NULL, &scale->w_pexpr, "width", scale->w_expr);
            if (ret < 0)
                goto err;

            ret = scale_parse_expr(ctx, NULL, &scale->h_pexpr, "height", scale->h_expr);
            if (ret < 0)
                goto err;
        }

        if (IS_SCALE2REF(ctx)) {
            scale->var_values[VAR_S2R_MAIN_N] = inl->frame_count_out;
            scale->var_values[VAR_S2R_MAIN_T] = TS2T(in->pts, link->time_base);
#if FF_API_FRAME_PKT
FF_DISABLE_DEPRECATION_WARNINGS
            scale->var_values[VAR_S2R_MAIN_POS] = in->pkt_pos == -1 ? NAN : in->pkt_pos;
FF_ENABLE_DEPRECATION_WARNINGS
#endif
        } else {
            scale->var_values[VAR_N] = inl->frame_count_out;
            scale->var_values[VAR_T] = TS2T(in->pts, link->time_base);
#if FF_API_FRAME_PKT
FF_DISABLE_DEPRECATION_WARNINGS
            scale->var_values[VAR_POS] = in->pkt_pos == -1 ? NAN : in->pkt_pos;
FF_ENABLE_DEPRECATION_WARNINGS
#endif
        }

        link->dst->inputs[0]->format        = in->format;
        link->dst->inputs[0]->w             = in->width;
        link->dst->inputs[0]->h             = in->height;
        link->dst->inputs[0]->colorspace    = in->colorspace;
        link->dst->inputs[0]->color_range   = in->color_range;

        link->dst->inputs[0]->sample_aspect_ratio.den = in->sample_aspect_ratio.den;
        link->dst->inputs[0]->sample_aspect_ratio.num = in->sample_aspect
