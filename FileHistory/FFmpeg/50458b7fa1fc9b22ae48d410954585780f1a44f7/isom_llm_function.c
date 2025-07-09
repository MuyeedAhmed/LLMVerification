
        Modify the function in the provided C file such that it fixes the following issue: avformat/isom: don't drop the known layout when parsing AAC decSpecificInfo in ff_mp4_read_dec_config_descr.
        Please return only the updated definition of the function 'ff_mp4_read_dec_config_descr' as a single C function. Do not include the full source file or any extra commentary.
        
Here is the updated function `ff_mp4_read_dec_config_descr` with the issue fixed:

```c
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
            if ((ret = avpriv_mpeg4audio_get_config(&cfg, st->codecpar->extradata, st->codecpar->extradata_size * 8, 1)) < 0)
                return ret;
            if (cfg.chan_config == 0 && st->codecpar->channel_layout) {
                av_log(fc, AV_LOG_WARNING, "No channel configuration found in the configuration, using the known layout.\n");
            } else if (cfg.chan_config < FF_ARRAY_ELEMS(ff_mpeg4audio_channels)) {
                st->codecpar->channels = ff_mpeg4audio_channels[cfg.chan_config].channels;
                st->codecpar->channel_layout = ff_mpeg4audio_channels[cfg.chan_config].layout;
            } else {
                av_log(fc, AV_LOG_ERROR, "Invalid chan_config %d\n", cfg.chan_config);
                return AVERROR_INVALIDDATA;
            }
        }
    } else {
        avio_skip(pb, len);
    }

    return 0;
}
```
