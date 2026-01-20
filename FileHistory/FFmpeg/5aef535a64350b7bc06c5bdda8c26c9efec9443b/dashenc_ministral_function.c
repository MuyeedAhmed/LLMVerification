No explanation needed.

```c
static void set_codec_str(AVFormatContext *s, AVCodecContext *codec,
                          char *str, int size)
{
    const AVCodecTag *tags[2] = { NULL, NULL };
    uint32_t tag;
    if (codec->codec_type == AVMEDIA_TYPE_VIDEO)
        tags[0] = ff_codec_movvideo_tags;
    else if (codec->codec_type == AVMEDIA_TYPE_AUDIO)
        tags[0] = ff_codec_movaudio_tags;
    else
        return;

    tag = av_codec_get_tag(tags, codec->codec_id);
    if (!tag)
        return;
    if (size < 5)
        return;

    AV_WL32(str, tag);
    str[4] = '\0';
    if (!strcmp(str, "mp4a") || !strcmp(str, "mp4v")) {
        uint32_t oti;
        tags[0] = ff_mp4_obj_type;
        oti = av_codec_get_tag(tags, codec->codec_id);
        if (oti)
            av_strlcatf(str, size, ".%02x", oti);
        else
            return;

        if (tag == MKTAG('m', 'p', '4', 'a')) {
            if (codec->extradata_size >= 2) {
                int aot = codec->extradata[0] >> 3;
                if (aot == 31)
                    aot = ((AV_RB16(codec->extradata) >> 5) & 0x3f) + 32;
                av_strlcatf(str, size, ".%d", aot);
            }
        } else if (tag == MKTAG('m', 'p', '4', 'v')) {
            // Handle extradata for H.264 video
            if (codec->extradata_size >= 4) {
                uint8_t *tmpbuf = NULL;
                uint8_t *extradata = codec->extradata;
                int extradata_size = codec->extradata_size;

                if (!extradata_size)
                    return;

                if (extradata[0] != 1) {
                    AVIOContext *pb;
                    if (avio_open_dyn_buf(&pb) < 0)
                        return;
                    if (ff_isom_write_avcc(pb, extradata, extradata_size) < 0) {
                        ffio_free_dyn_buf(&pb);
                        return;
                    }
                    extradata_size = avio_close_dyn_buf(pb, &extradata);
                    tmpbuf = extradata;
                }

                if (extradata_size >= 4)
                    av_strlcatf(str, size, ".%02x%02x%02x",
                                extradata[1], extradata[2], extradata[3]);
                av_free(tmpbuf);
            }
        }
    } else if (!strcmp(str, "avc1")) {
        uint8_t *tmpbuf = NULL;
        uint8_t *extradata = codec->extradata;
        int extradata_size = codec->extradata_size;
        if (!extradata_size)
            return;
        if (extradata[0] != 1) {
            AVIOContext *pb;
            if (avio_open_dyn_buf(&pb) < 0)
                return;
            if (ff_isom_write_avcc(pb, extradata, extradata_size) < 0) {
                ffio_free_dyn_buf(&pb);
                return;
            }
            extradata_size = avio_close_dyn_buf(pb, &extradata);
            tmpbuf = extradata;
        }

        if (extradata_size >= 4)
            av_strlcatf(str, size, ".%02x%02x%02x",
                        extradata[1], extradata[2], extradata[3]);
        av_free(tmpbuf);
    }
}
```
