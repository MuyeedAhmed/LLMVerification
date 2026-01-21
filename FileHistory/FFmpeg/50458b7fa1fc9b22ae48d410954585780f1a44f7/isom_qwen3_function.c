Do not include any other text.

int ff_mp4_read_dec_config_descr(AVFormatContext *fc, AVStream *st, AVIOContext *pb)
{
    enum AVCodecID codec_id;
    int len, tag;
    int ret;
    int object_type_id = avio_r8(pb);
    avio_r8(pb); /* stream type */
    avio_rb24(pb); /* buffer size db */
    avio_rb32(pb); /* rc_max_rate */

    st->codecpar->bit_rate = avio_rb32(pb); /* avg bitrate */

    codec_id= ff_codec_get_id(ff_mp4_obj_type, object_type_id);
    if (codec_id)
        st->codecpar->codec_id = codec_id;
    av_log(fc, AV_LOG_TRACE, "esds object type id 0x%02x\n", object_type_id);
    len = ff_mp4_read_descr(fc, pb, &tag);
    if (tag == MP4DecSpecificDescrTag) {
        av_log(fc, AV_LOG_TRACE, "Specific MPEG-4 header len=%d\n", len);
        /* As per 14496-3:2009 9.D.2.2, No decSpecificInfo is defined
           for MPEG-1 Audio or MPEG-2 Audio; MPEG-2 AAC excluded. */
        if (object_type_id == 0x69 || object_type_id == 0x6b)
            return 0;
        if (!len || (uint64_t)len > (1<<30))
            return AVERROR_INVALIDDATA;
        if ((ret = ff_get_extradata(fc, st->codecpar, pb, len)) < 0)
            return ret;
        if (st->codecpar->codec_id == AV_CODEC_ID_AAC) {
            MPEG4AudioConfig cfg = {0};
            ret = avpriv_mpeg4audio_get_config2(&cfg, st->codecpar->extradata,
                                                st->codecpar->extradata_size, 1, fc);
            if (ret < 0)
                return ret;
            av_channel_layout_uninit(&st->codecpar->ch_layout);
            st->codecpar->ch_layout.order = AV_CHANNEL_ORDER_UNSPEC;
            st->codecpar->ch_layout.nb_channels = cfg.channels;
            if (cfg.object_type == 29 && cfg.sampling_index < 3) // old mp3on4
                st->codecpar->sample_rate = ff_mpa_freq_tab[cfg.sampling_index];
            else if (cfg.ext_sample_rate)
                st->codecpar->sample_rate = cfg.ext_sample_rate;
            else
                st->codecpar->sample_rate = cfg.sample_rate;
            av_log(fc, AV_LOG_TRACE, "mp4a config channels %d obj %d ext obj %d "
                    "sample rate %d ext sample rate %d\n", cfg.channels,
                    cfg.object_type, cfg.ext_object_type,
                    cfg.sample_rate, cfg.ext_sample_rate);
            if (!(st->codecpar->codec_id = ff_codec_get_id(mp4_audio_types,
                                                        cfg.object_type)))
                st->codecpar->codec_id = AV_CODEC_ID_AAC;
        }
    }
    return 0;
}
