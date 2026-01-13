No explanation needed.
```c
int ff_dovi_configure(DOVIContext *s, AVCodecContext *avctx)
{
    av_cold int ff_dovi_configure_init(DOVIContext *s, AVCodecContext *avctx)
    {
        const AVDOVIMetadata *metadata = NULL;
        const AVFrameSideData *sd;
        sd = av_frame_side_data_get(avctx->decoded_side_data,
                                    avctx->nb_decoded_side_data,
                                    AV_FRAME_DATA_DOVI_METADATA);
        if (sd)
            metadata = (const AVDOVIMetadata *) sd->data;

        /* Current encoders cannot handle metadata compression during encoding */
        return dovi_configure_ext(s, avctx->codec_id, metadata, AV_DOVI_COMPRESSION_NONE,
                                  avctx->strict_std_compliance, avctx->width,
                                  avctx->height, avctx->framerate, avctx->pix_fmt,
                                  avctx->colorspace, avctx->color_primaries, avctx->color_trc,
                                  &avctx->coded_side_data, &avctx->nb_coded_side_data);
    }

    if (!s->initialized) {
        int ret = ff_dovi_configure_init(s, avctx);
        if (ret < 0)
            return ret;
        s->initialized = 1;
    }
    return 0;
}
```
