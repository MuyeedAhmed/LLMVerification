Do not include any extra text.

static void show_stream(AVFormatContext *fmt_ctx, int stream_idx)
{
    AVStream *stream = fmt_ctx->streams[stream_idx];
    AVCodecContext *dec_ctx;
    const AVCodec *dec;
    const char *profile;
    char val_str[128];
    AVRational display_aspect_ratio, *sar = NULL;
    const AVPixFmtDescriptor *desc;

    probe_object_header("stream");

    probe_int("index", stream->index);

    if ((dec_ctx = stream->codec)) {
        if ((dec = dec_ctx->codec)) {
            probe_str("codec_name", dec->name);
            probe_str("codec_long_name", dec->long_name);
        } else {
            probe_str("codec_name", "unknown");
        }

        probe_str("codec_type", media_type_string(dec_ctx->codec_type));
        probe_str("codec_time_base",
                  rational_string(val_str, sizeof(val_str),
                                  "/", &dec_ctx->time_base));

        /* print AVI/FourCC tag */
        av_get_codec_tag_string(val_str, sizeof(val_str), dec_ctx->codec_tag);
        probe_str("codec_tag_string", val_str);
        probe_str("codec_tag", tag_string(val_str, sizeof(val_str),
                                          dec_ctx->codec_tag));

        /* print profile, if there is one */
        if (dec && (profile = av_get_profile_name(dec, dec_ctx->profile)))
            probe_str("profile", profile);

        switch (dec_ctx->codec_type) {
        case AVMEDIA_TYPE_VIDEO:
            probe_int("width", dec_ctx->width);
            probe_int("height", dec_ctx->height);
            probe_int("coded_width", dec_ctx->coded_width);
            probe_int("coded_height", dec_ctx->coded_height);
            probe_int("has_b_frames", dec_ctx->has_b_frames);
            if (dec_ctx->sample_aspect_ratio.num)
                sar = &dec_ctx->sample_aspect_ratio;
            else if (stream->sample_aspect_ratio.num)
                sar = &stream->sample_aspect_ratio;

            if (sar) {
                probe_str("sample_aspect_ratio",
                          rational_string(val_str, sizeof(val_str), ":", sar));
                av_reduce(&display_aspect_ratio.num, &display_aspect_ratio.den,
                          dec_ctx->width  * sar->num, dec_ctx->height * sar->den,
                          1024*1024);
                probe_str("display_aspect_ratio",
                          rational_string(val_str, sizeof(val_str), ":",
                          &display_aspect_ratio));
            }
            desc = av_pix_fmt_desc_get(dec_ctx->pix_fmt);
            probe_str("pix_fmt", desc ? desc->name : "unknown");
            probe_int("level", dec_ctx->level);

            probe_str("color_range", av_color_range_name(dec_ctx->color_range));
            probe_str("color_space", av_color_space_name(dec_ctx->colorspace));
            probe_str("color_trc", av_color_transfer_name(dec_ctx->color_trc));
            probe_str("color_pri", av_color_primaries_name(dec_ctx->color_primaries));
            probe_str("chroma_loc", av_chroma_location_name(dec_ctx->chroma_sample_location));
            break;

        case AVMEDIA_TYPE_AUDIO:
            probe_str("sample_rate",
                      value_string(val_str, sizeof(val_str),
                                   dec_ctx->sample_rate,
                                   unit_hertz_str));
            probe_int("channels", dec_ctx->channels);
            probe_int("bits_per_sample",
                      av_get_bits_per_sample(dec_ctx->codec_id));
            break;
        }
    } else {
        probe_str("codec_type", "unknown");
    }

    if (fmt_ctx->iformat->flags & AVFMT_SHOW_IDS)
        probe_int("id", stream->id);
    probe_str("avg_frame_rate",
              rational_string(val_str, sizeof(val_str), "/",
              &stream->avg_frame_rate));
    if (dec_ctx->bit_rate)
        probe_str("bit_rate",
                  value_string(val_str, sizeof(val_str),
                               dec_ctx->bit_rate, unit_bit_per_second_str));
    probe_str("time_base",
              rational_string(val_str, sizeof(val_str), "/",
              &stream->time_base));
    probe_str("start_time",
              time_value_string(val_str, sizeof(val_str),
                                stream->start_time, &stream->time_base));
    probe_str("duration",
              time_value_string(val_str, sizeof(val_str),
                                stream->duration, &stream->time_base));
    if (stream->nb_frames)
        probe_int("nb_frames", stream->nb_frames);

    probe_dict(stream->metadata, "tags");

    probe_object_footer("stream");
}
