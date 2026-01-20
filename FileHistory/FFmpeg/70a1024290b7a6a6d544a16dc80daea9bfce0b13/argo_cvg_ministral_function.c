No explanation needed.
```c
static int argo_cvg_write_trailer(AVFormatContext *s)
{
    ArgoCVGMuxContext *ctx = s->priv_data;
    int64_t ret;

    avio_wl32(s->pb, ctx->checksum);

    if ((ret = avio_seek(s->pb, 0, SEEK_SET)) < 0)
        return ret;

    avio_wl32(s->pb, (uint32_t)ctx->size);

    ctx->checksum +=  (ctx->size      & 255)
                   + ((ctx->size>> 8) & 255)
                   + ((ctx->size>>16) & 255)
                   +  (ctx->size>>24);

    return 0;
}
```
