No explanation needed.
```c
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
        link->dst->inputs[0]->sample_aspect_ratio.num = in->sample_aspect_ratio.num;

        if ((ret = config_props(outlink)) < 0)
            goto err;
    }

scale:
    scale->hsub = desc->log2_chroma_w;
    scale->vsub = desc->log2_chroma_h;

    out = ff_get_video_buffer(outlink, outlink->w, outlink->h);
    if (!out) {
        ret = AVERROR(ENOMEM);
        goto err;
    }

    if (out->width != in->width || out->height != in->height) {
        av_frame_side_data_remove_by_props(&out->side_data, &out->nb_side_data,
                                           AV_SIDE_DATA_PROP_SIZE_DEPENDENT);
    }

    if (in->color_primaries != out->color_primaries || in->color_trc != out->color_trc) {
        av_frame_side_data_remove_by_props(&out->side_data, &out->nb_side_data,
                                           AV_SIDE_DATA_PROP_COLOR_DEPENDENT);
    }

    av_reduce(&out->sample_aspect_ratio.num, &out->sample_aspect_ratio.den,
              (int64_t)in->sample_aspect_ratio.num * outlink->h * link->w,
              (int64_t)in->sample_aspect_ratio.den * outlink->w * link->h,
              INT_MAX);

    if (sws_is_noop(out, in)) {
        av_frame_free(&out);
        in->flags = flags_orig;
        *frame_out = in;
        return 0;
    }

    if (out->format == AV_PIX_FMT_PAL8) {
        out->format = AV_PIX_FMT_BGR8;
        avpriv_set_systematic_pal2((uint32_t*) out->data[1], out->format);
    }

    ret = sws_scale_frame(scale->sws, out, in);
    av_frame_free(&in);
    out->flags = flags_orig;
    out->format = outlink->format; /* undo PAL8 handling */
    if (ret < 0)
        av_frame_free(&out);
    *frame_out = out;
    return ret;

err:
    av_frame_free(&in);
    return ret;
}
```
