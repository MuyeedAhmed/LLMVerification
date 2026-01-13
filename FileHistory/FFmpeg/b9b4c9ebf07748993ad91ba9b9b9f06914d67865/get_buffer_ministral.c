No explanation needed.
```c
static int audio_get_buffer(AVCodecContext *avctx, AVFrame *frame)
{
    FramePool *pool = avctx->internal->pool;
    int planes = pool->planes;
    int i;

    frame->linesize[0] = pool->linesize[0];

    if (planes > AV_NUM_DATA_POINTERS) {
        frame->extended_data = av_calloc(planes, sizeof(*frame->extended_data));
        frame->nb_extended_buf = planes - AV_NUM_DATA_POINTERS;
        frame->extended_buf  = av_calloc(frame->nb_extended_buf,
                                          sizeof(*frame->extended_buf));
        if (!frame->extended_data || !frame->extended_buf) {
            av_freep(&frame->extended_data);
            av_freep(&frame->extended_buf);
            return AVERROR(ENOMEM);
        }
    } else {
        frame->extended_data = frame->data;
        av_assert0(frame->nb_extended_buf == 0);
    }

    for (i = 0; i < FFMIN(planes, AV_NUM_DATA_POINTERS); i++) {
        frame->buf[i] = av_buffer_pool_get(pool->pools[0]);
        if (!frame->buf[i])
            goto fail;
        frame->extended_data[i] = frame->data[i] = frame->buf[i]->data;
    }
    for (i = 0; i < frame->nb_extended_buf; i++) {
        frame->extended_buf[i] = av_buffer_pool_get(pool->pools[0]);
        if (!frame->extended_buf[i])
            goto fail;
        frame->extended_data[i + AV_NUM_DATA_POINTERS] = frame->extended_buf[i]->data;
    }

    if (avctx->debug & FF_DEBUG_BUFFERS)
        av_log(avctx, AV_LOG_DEBUG, "default_get_buffer called on frame %p", frame);

    return 0;
fail:
    av_frame_unref(frame);
    return AVERROR(ENOMEM);
}

static int update_frame_pool(AVCodecContext *avctx, AVFrame *frame)
{
    FramePool *pool = avctx->internal->pool;
    int i, ret;

    if (pool && pool->format == frame->format) {
        if (avctx->codec_type == AVMEDIA_TYPE_VIDEO &&
            pool->width == frame->width && pool->height == frame->height)
            return 0;
        if (avctx->codec_type == AVMEDIA_TYPE_AUDIO &&
            pool->channels == frame->ch_layout.nb_channels &&
            frame->nb_samples == pool->samples)
            return 0;
    }

    pool = av_refstruct_alloc_ext(sizeof(*pool), 0, NULL, frame_pool_free);
    if (!pool)
        return AVERROR(ENOMEM);

    switch (avctx->codec_type) {
    case AVMEDIA_TYPE_VIDEO: {
        int linesize[4];
        int w = frame->width;
        int h = frame->height;
        int unaligned;
        ptrdiff_t linesize1[4];
        size_t size[4];

        avcodec_align_dimensions2(avctx, &w, &h, pool->stride_align);

        do {
            ret = av_image_fill_linesizes(linesize, avctx->pix_fmt, w);
            if (ret < 0)
                goto fail;
            w += w & ~(w - 1);

            unaligned = 0;
            for (i = 0; i < 4; i++)
                unaligned |= linesize[i] % pool->stride_align[i];
        } while (unaligned);

        for (i = 0; i < 4; i++)
            linesize1[i] = linesize[i];
        ret = av_image_fill_plane_sizes(size, avctx->pix_fmt, h, linesize1);
        if (ret < 0)
            goto fail;

        for (i = 0; i < 4; i++) {
            pool->linesize[i] = linesize[i];
            if (size[i]) {
                if (size[i] > INT_MAX - (16 + STRIDE_ALIGN - 1)) {
                    ret = AVERROR(EINVAL);
                    goto fail;
                }
                pool->pools[i] = av_buffer_pool_init(size[i] + 16 + STRIDE_ALIGN - 1,
                                                     CONFIG_MEMORY_POISONING ?
                                                        NULL :
                                                        av_buffer_mallocz);
                if (!pool->pools[i]) {
                    ret = AVERROR(ENOMEM);
                    goto fail;
                }
            }
        }
        pool->format = frame->format;
        pool->width  = frame->width;
        pool->height = frame->height;

        break;
    }
    case AVMEDIA_TYPE_AUDIO: {
        ret = av_samples_get_buffer_size(&pool->linesize[0],
                                         frame->ch_layout.nb_channels,
                                         frame->nb_samples, frame->format, 0);
        if (ret < 0)
            goto fail;

        pool->pools[0] = av_buffer_pool_init(pool->linesize[0],
                                             CONFIG_MEMORY_POISONING ?
                                                NULL :
                                                av_buffer_mallocz);
        if (!pool->pools[0]) {
            ret = AVERROR(ENOMEM);
            goto fail;
        }

        pool->format     = frame->format;
        pool->channels   = frame->ch_layout.nb_channels;
        pool->samples = frame->nb_samples;
        pool->planes     = av_sample_fmt_is_planar(pool->format) ? pool->channels : 1;
        break;
    }
    default: av_assert0(0);
    }

    av_refstruct_unref(&avctx->internal->pool);
    avctx->internal->pool = pool;

    return 0;
fail:
    av_refstruct_unref(&pool);
    return ret;
}
```
