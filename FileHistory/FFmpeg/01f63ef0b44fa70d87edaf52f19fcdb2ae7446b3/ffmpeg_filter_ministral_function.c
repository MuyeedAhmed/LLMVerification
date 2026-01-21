No explanation needed.

```c
static int configure_input_video_filter(FilterGraph *fg, AVFilterGraph *graph,
                                        InputFilter *ifilter, AVFilterInOut *in)
{
    InputFilterPriv *ifp = ifp_from_ifilter(ifilter);

    AVFilterContext *last_filter;
    const AVFilter *buffer_filt = avfilter_get_by_name("buffer");
    const AVPixFmtDescriptor *desc;
    char name[255];
    int ret, pad_idx = 0;
    AVBufferSrcParameters *par = av_buffersrc_parameters_alloc();
    if (!par)
        return AVERROR(ENOMEM);

    if (ifp->type_src == AVMEDIA_TYPE_SUBTITLE)
        sub2video_prepare(ifp);

    snprintf(name, sizeof(name), "graph %d input from stream %s", fg->index,
             ifp->opts.name);

    ifp->filter = avfilter_graph_alloc_filter(graph, buffer_filt, name);
    if (!ifp->filter) {
        ret = AVERROR(ENOMEM);
        goto fail;
    }

    par->format              = ifp->format;
    par->time_base           = ifp->time_base;
    par->frame_rate          = ifp->opts.framerate;
    par->width               = ifp->width;
    par->height              = ifp->height;
    par->sample_aspect_ratio = ifp->sample_aspect_ratio.den > 0 ?
                               ifp->sample_aspect_ratio : (AVRational){ 0, 1 };
    par->color_space         = ifp->color_space;
    par->color_range         = ifp->color_range;
    par->hw_frames_ctx       = ifp->hw_frames_ctx;
    par->side_data           = ifp->side_data;
    par->nb_side_data        = ifp->nb_side_data;

    ret = av_buffersrc_parameters_set(ifp->filter, par);
    if (ret < 0)
        goto fail;
    av_freep(&par);

    ret = avfilter_init_dict(ifp->filter, NULL);
    if (ret < 0)
        goto fail;

    last_filter = ifp->filter;

    desc = av_pix_fmt_desc_get(ifp->format);
    av_assert0(desc);

    if ((ifp->opts.flags & IFILTER_FLAG_CROP)) {
        char crop_buf[64];
        snprintf(crop_buf, sizeof(crop_buf), "w=iw-%u-%u:h=ih-%u-%u:x=%u:y=%u",
                 ifp->opts.crop_left, ifp->opts.crop_right,
                 ifp->opts.crop_top, ifp->opts.crop_bottom,
                 ifp->opts.crop_left, ifp->opts.crop_top);
        ret = insert_filter(&last_filter, &pad_idx, "crop", crop_buf);
        if (ret < 0)
            return ret;
    }

    // TODO: insert hwaccel enabled filters like transpose_vaapi into the graph
    ifp->displaymatrix_applied = 0;
    if ((ifp->opts.flags & IFILTER_FLAG_AUTOROTATE) &&
        !(desc->flags & AV_PIX_FMT_FLAG_HWACCEL)) {
        int32_t *displaymatrix = ifp->displaymatrix;
        double theta;

        theta = get_rotation(displaymatrix);

        if (fabs(theta - 90) < 1.0) {
            ret = insert_filter(&last_filter, &pad_idx, "transpose",
                                displaymatrix[3] > 0 ? "cclock_flip" : "clock");
        } else if (fabs(theta - 180) < 1.0) {
            if (displaymatrix[0] < 0) {
                ret = insert_filter(&last_filter, &pad_idx, "hflip", NULL);
                if (ret < 0)
                    return ret;
            }
            if (displaymatrix[4] < 0) {
                ret = insert_filter(&last_filter, &pad_idx, "vflip", NULL);
            }
        } else if (fabs(theta - 270) < 1.0) {
            ret = insert_filter(&last_filter, &pad_idx, "transpose",
                                displaymatrix[3] < 0 ? "clock_flip" : "cclock");
        } else if (fabs(theta) > 1.0) {
            char rotate_buf[64];
            snprintf(rotate_buf, sizeof(rotate_buf), "%f*PI/180", theta);
            ret = insert_filter(&last_filter, &pad_idx, "rotate", rotate_buf);
        } else if (fabs(theta) < 1.0) {
            if (displaymatrix && displaymatrix[4] < 0) {
                ret = insert_filter(&last_filter, &pad_idx, "vflip", NULL);
            }
        }
        if (ret < 0)
            return ret;

        ifp->displaymatrix_applied = 1;
    }

    snprintf(name, sizeof(name), "trim_in_%s", ifp->opts.name);
    ret = insert_trim(fg, ifp->opts.trim_start_us, ifp->opts.trim_end_us,
                      &last_filter, &pad_idx, name);
    if (ret < 0)
        return ret;

    if ((ret = avfilter_link(last_filter, 0, in->filter_ctx, in->pad_idx)) < 0)
        return ret;

    // Remove display matrix side data from buffered frames
    if (ifp->displaymatrix_present) {
        AVFrameSideData *sd = av_frame_get_side_data(ifp->filter->inputs[0]->frame, AV_FRAME_DATA_DISPLAYMATRIX);
        if (sd) {
            av_frame_remove_side_data(ifp->filter->inputs[0]->frame, AV_FRAME_DATA_DISPLAYMATRIX);
        }
    }

    return 0;
fail:
    av_freep(&par);

    return ret;
}
```
