Do NOT include any other text.
```c
static int mov_read_heif_item(MOVContext *c, AVIOContext *pb, MOVAtom atom)
{
    int64_t end = av_sat_add64(avio_tell(pb), atom.size);
    int version, flags;
    int ret;
    AVStream *st;
    MOVStreamContext *sc;

    if (c->fc->nb_streams < 1)
        return 0;
    st = c->fc->streams[c->fc->nb_streams-1];
    sc = st->priv_data;

    version = avio_r8(pb);
    flags   = avio_rb24(pb);
    if (version != 0 || flags != 0) {
        av_log(c->fc, AV_LOG_ERROR,
               "Unsupported 'heif' box with version %d, flags: %#x",
               version, flags);
        return AVERROR_INVALIDDATA;
    }

    ret = ff_mov_read_heif_item(c->fc, pb, st);
    if (ret < 0)
        return ret;

    if (avio_tell(pb) != end) {
        av_log(c->fc, AV_LOG_WARNING, "skip %" PRId64 " bytes of unknown data inside heif\n",
                end - avio_tell(pb));
        avio_seek(pb, end, SEEK_SET);
    }
    return ret;
}
```
