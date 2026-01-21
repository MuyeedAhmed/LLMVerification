Do NOT repeat the original code.
```c
static int mov_flush_fragment_interleaving(AVFormatContext *s, MOVTrack *track)
{
    MOVMuxContext *mov = s->priv_data;
    int ret, buf_size;
    uint8_t *buf;
    int i, offset;

    if (!track->mdat_buf)
        return 0;
    if (!mov->mdat_buf) {
        if ((ret = avio_open_dyn_buf(&mov->mdat_buf)) < 0)
            return ret;
    }
    buf_size = avio_get_dyn_buf(track->mdat_buf, &buf);

    offset = avio_tell(mov->mdat_buf);
    avio_write(mov->mdat_buf, buf, buf_size);
    ffio_free_dyn_buf(&track->mdat_buf);

    for (i = track->entries_flushed; i < track->entry; i++)
        track->cluster[i].pos += offset;
    track->entries_flushed = track->entry;
    return 0;
}
```
