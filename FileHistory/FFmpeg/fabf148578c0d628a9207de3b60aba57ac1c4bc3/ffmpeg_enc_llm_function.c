
        Modify the function in the provided C file such that it fixes the following issue: fftools/ffmpeg_enc: only promote first frame side data to global when meaningful in enc_open.
        Please return only the updated definition of the function 'enc_open' as a single C function. Do not include the full source file or any extra commentary.
        
```c
int enc_open(void *opaque, const AVFrame *frame)
{
    OutputStream *ost = opaque;
    InputStream *ist = ost->ist;
    Encoder              *e = ost->enc;
    AVCodecContext *enc_ctx = ost->enc_ctx;
    Decoder            *dec;
    const AVCodec      *enc = enc_ctx->codec;
    OutputFile          *of = ost->file;
    FrameData *fd;
    int frame_samples = 0;
    int ret;

    if (e->opened)
        return 0;

    // frame is always non-NULL for audio and video
    av_assert0(frame || (enc->type != AVMEDIA_TYPE_VIDEO && enc->type != AVMEDIA_TYPE_AUDIO));

    if (frame) {
        av_assert0(frame->opaque_ref);
        fd = (FrameData*)frame->opaque_ref->data;
    }

    ret = set_encoder_id(of, ost);
    if (ret < 0)
        return ret;

    if (ist)
        dec = ist->decoder;

    // the timebase is chosen by filtering code
    if (ost->type == AVMEDIA_TYPE_AUDIO || ost->type == AVMEDIA_TYPE_VIDEO) {
        enc_ctx->time_base      = frame->time_base;
        enc_ctx->framerate      = fd->frame_rate_filter;
        ost->st->avg_frame_rate = fd->frame_rate_filter;
    }

    switch (enc_ctx->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
        av_assert0(frame->format != AV_SAMPLE_FMT_NONE &&
                   frame->sample_rate > 0 &&
                   frame->ch_layout.nb_channels > 0);
        enc_ctx->sample_fmt     = frame->format;
        enc_ctx->sample_rate    = frame->sample_rate;
        ret = av_channel_layout_copy(&enc_ctx->ch_layout, &frame->ch_layout);
        if (ret < 0)
            return ret;

        if (ost->bits_per_raw_sample)
            enc_ctx->bits_per_raw_sample = ost->bits_per_raw_sample;
        else
            enc_ctx->bits_per_raw_sample = FFMIN(fd->bits_per_raw_sample,
                                                 av_get_bytes_per_sample(enc_ctx->sample_fmt) << 3);
        break;

    case AVMEDIA_TYPE_VIDEO: {
        av_assert0(frame->format != AV_PIX_FMT_NONE &&
                   frame->width > 0 &&
                   frame->height > 0);
        enc_ctx->width  = frame->width;
        enc_ctx->height = frame->height;
        enc_ctx->sample_aspect_ratio = ost->st->sample_aspect_ratio =
            ost->frame_aspect_ratio.num ? // overridden by the -aspect cli option
            av_mul_q(ost->frame_aspect_ratio, (AVRational){ enc_ctx->height, enc_ctx->width }) :
            frame->sample_aspect_ratio;

        enc_ctx->pix_fmt = frame->format;

        if (ost->bits_per_raw_sample)
            enc_ctx->bits_per_raw_sample = ost->bits_per_raw_sample;
        else
            enc_ctx->bits_per_raw_sample = FFMIN(fd->bits_per_raw_sample,
                                                 av_pix_fmt_desc_get(enc_ctx->pix_fmt)->comp[0].depth);

        enc_ctx->color_range            = frame->color_range;
        enc_ctx->color_primaries        = frame->color_primaries;
        enc_ctx->color_trc              = frame->color_trc;
        enc_ctx->colorspace             = frame->colorspace;
        enc_ctx->chroma_sample_location = frame->chroma_location;

        for (int i = 0; i < frame->nb_side_data; i++) {
            ret = av_frame_side_data_clone(
                &enc_ctx->decoded_side_data, &enc_ctx->nb_decoded_side_data,
                frame->side_data[i], AV_FRAME_SIDE_DATA_FLAG_UNIQUE);
            if (ret < 0) {
                av_frame_side_data_free(
                    &enc_ctx->decoded_side_data,
                    &enc_ctx->nb_decoded_side_data);
                av_log(NULL, AV_LOG_ERROR,
                        "failed to configure video encoder: %s!\n",
                        av_err2str(ret));
                return ret;
            }
        }

        if (enc_ctx->flags & (AV_CODEC_FLAG_INTERLACED_DCT | AV_CODEC_FLAG_INTERLACED_ME) ||
            (frame->flags & AV_FRAME_FLAG_INTERLACED)
#if FFMPEG_OPT_TOP
            || ost->top_field_first >= 0
#endif
            ) {
            int top_field_first =
#if FFMPEG_OPT_TOP
                ost->top_field_first >= 0 ?
                ost->top_field_first :
#endif
                !!(frame->flags & AV_FRAME_FLAG_TOP_FIELD_FIRST);

            if (enc->id == AV_CODEC_ID_MJPEG)
                enc_ctx->field_order = top_field_first ? AV_FIELD_TT : AV_FIELD_BB;
            else
                enc_ctx->field_order = top_field_first ? AV_FIELD_TB : AV_FIELD_BT;
        } else
            enc_ctx->field_order = AV_FIELD_PROGRESSIVE;

        break;
        }
    case AVMEDIA_TYPE_SUBTITLE:
        if (ost->enc_timebase.num)
            av_log(ost, AV_LOG_WARNING,
                   "-enc_time_base not supported for subtitles, ignoring\n");
        enc_ctx->time_base = AV_TIME_BASE_Q;

        if (!enc_ctx->width) {
            enc_ctx->width     = ost->ist->par->width;
            enc_ctx->height    = ost->ist->par->height;
        }

        av_assert0(dec);
        if (dec->subtitle_header) {
            /* ASS code assumes this buffer is null terminated so add extra byte. */
            enc_ctx->subtitle_header = av_mallocz(dec->subtitle_header_size + 1);
            if (!enc_ctx->subtitle_header)
                return AVERROR(ENOMEM);
            memcpy(enc_ctx->subtitle_header, dec->subtitle_header,
                   dec->subtitle_header_size);
            enc_ctx->subtitle_header_size = dec->subtitle_header_size;
        }

        break;
    default:
        av_assert0(0);
        break;
    }

    if (ost->bitexact)
        enc_ctx->flags |= AV_CODEC_FLAG_BITEXACT;

    if (!av_dict_get(ost->encoder_opts, "threads", NULL, 0))
        av_dict_set(&ost->encoder_opts, "threads", "auto", 0);

    if (enc->capabilities & AV_CODEC_CAP_ENCODER_REORDERED_OPAQUE) {
        ret = av_dict_set(&ost->encoder_opts, "flags", "+copy_opaque", AV_DICT_MULTIKEY);
        if (ret < 0)
            return ret;
    }

    av_dict_set(&ost->encoder_opts, "flags", "+frame_duration", AV_DICT_MULTIKEY);

    ret = hw_device_setup_for_encode(ost, frame ? frame->hw_frames_ctx : NULL);
    if (ret < 0) {
        av_log(ost, AV_LOG_ERROR,
               "Encoding hardware device setup failed: %s\n", av_err2str(ret));
        return ret;
    }

    if ((ret = avcodec_open2(ost->enc_ctx, enc, &ost->encoder_opts)) < 0) {
        if (ret != AVERROR_EXPERIMENTAL)
            av_log(ost, AV_LOG_ERROR, "Error while opening encoder - maybe "
                   "incorrect parameters such as bit_rate, rate, width or height.\n");
        return ret;
    }

    e->opened = 1;

    if (ost->enc_ctx->frame_size)
        frame_samples = ost->enc_ctx->frame_size;

    ret = check_avoptions(ost->encoder_opts);
    if (ret < 0)
        return ret;

    if (ost->enc_ctx->bit_rate && ost->enc_ctx->bit_rate < 1000 &&
        ost->enc_ctx->codec_id != AV_CODEC_ID_CODEC2 /* don't complain about 700 bit/s modes */)
        av_log(ost, AV_LOG_WARNING, "The bitrate parameter is set too low."
                                    " It takes bits/s as argument, not kbits/s\n");

    ret = avcodec_parameters_from_context(ost->par_in, ost->enc_ctx);
    if (ret < 0) {
        av_log(ost, AV_LOG_FATAL,
               "Error initializing the output stream codec context.\n");
        return ret;
    }

    /*
     * Add global input side data. For now this is naive, and copies it
     * from the input stream's global side data. All side data should
     * really be funneled over AVFrame and libavfilter, then added back to
     * packet side data, and then potentially using the first packet for
     * global side data.
     */
    if (ist) {
        for (int i = 0; i < ist->st->codecpar->nb_coded_side_data; i++) {
            AVPacketSideData *sd_src = &ist->st->codecpar->coded_side_data[i];
            if (sd_src->type != AV_PKT_DATA_CPB_PROPERTIES &&
                strncmp((char *)sd_src->data, "meaningful", 10) == 0) {
                AVPacketSideData *sd_dst = av_packet_side_data_new(&ost->par_in->coded_side_data,
                                                                   &ost->par_in->nb_coded_side_data,
                                                                   sd_src->type, sd_src->size, 0);
                if (!sd_dst)
                    return AVERROR(ENOMEM);
                memcpy(sd_dst->data, sd_src->data, sd_src->size);
                if (ist->autorotate && sd_src->type == AV_PKT_DATA_DISPLAYMATRIX)
                    av_display_rotation_set((int32_t *)sd_dst->data, 0);
            }
        }
    }

    // copy timebase while removing common factors
    if (ost->st->time_base.num <= 0 || ost->st->time_base.den <= 0)
        ost->st->time_base = enc_ctx->time_base;

    return 0;
}
```
