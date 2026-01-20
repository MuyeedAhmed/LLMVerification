Do NOT repeat the original code verbatim.
```c
static int mov_read_covr(MOVContext *c, AVIOContext *pb, int type, int len)
{
    AVStream *st;
    MOVStreamContext *sc;
    enum AVCodecID id;
    int ret;

    switch (type) {
    case 0xd:  id = AV_CODEC_ID_MJPEG; break;
    case 0xe:  id = AV_CODEC_ID_PNG;   break;
    case 0x1b: id = AV_CODEC_ID_BMP;   break;
    default:
        av_log(c->fc, AV_LOG_WARNING, "Unknown cover type: 0x%x.\n", type);
        avio_skip(pb, len);
        return 0;
    }

    sc = av_mallocz(sizeof(*sc));
    if (!sc)
        return AVERROR(ENOMEM);
    ret = ff_add_attached_pic(c->fc, NULL, pb, NULL, len);
    if (ret < 0) {
        av_free(sc);
        return ret;
    }
    st = c->fc->streams[c->fc->nb_streams - 1];
    st->priv_data = sc;
    sc->id = st->id;
    sc->refcount = 1;

    if (st->attached_pic.size >= 8 && id != AV_CODEC_ID_BMP) {
        if (AV_RB64(st->attached_pic.data) == 0x89504e470d0a1a0a) {
            id = AV_CODEC_ID_PNG;
        } else {
            id = AV_CODEC_ID_MJPEG;
        }
    }
    st->codecpar->codec_id   = id;

    // Check if there's already an attached picture
    if (st->attached_pic.data) {
        av_log(c->fc, AV_LOG_WARNING, "An attached picture is already present. Skipping this one.\n");
        av_free(sc);
        return 0;
    }

    return 0;
}
```
