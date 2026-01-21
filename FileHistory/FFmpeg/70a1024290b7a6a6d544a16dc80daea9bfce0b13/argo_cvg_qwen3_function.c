Do not provide any explanation. Do not use markdown formatting in your response.

static int argo_cvg_write_trailer(AVFormatContext *s)
{
    ArgoCVGMuxContext *ctx = s->priv_data;
    int64_t ret;

    ctx->checksum +=  (ctx->size      & 255)
                   + ((ctx->size>> 8) & 255)
                   + ((ctx->size>>16) & 255)
                   +  (ctx->size>>24);

    av_log(s, AV_LOG_TRACE, "size     = %zu\n", ctx->size);
    av_log(s, AV_LOG_TRACE, "checksum = %u\n",  ctx->checksum);

    avio_wl32(s->pb, ctx->checksum);

    if ((ret = avio_seek(s->pb, 0, SEEK_SET)) < 0)
        return ret;

    avio_wl32(s->pb, (uint32_t)ctx->size);
    return 0;
}
