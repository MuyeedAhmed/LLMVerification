No other text or explanation is needed.

```c
static int asfrtp_parse_packet(AVFormatContext *s, PayloadContext *asf,
                               AVStream *st, AVPacket *pkt,
                               uint32_t *timestamp,
                               const uint8_t *buf, int len, uint16_t seq,
                               int flags)
{
    AVIOContext *pb = &asf->pb;
    int res, mflags, len_off;
    RTSPState *rt = s->priv_data;

    if (!rt->asf_ctx)
        return -1;

    if (len > 0) {
        int off, out_len = 0;

        if (len < 4)
            return -1;

        av_freep(&asf->buf);

        ffio_init_context(pb, buf, len, 0, NULL, NULL, NULL, NULL);

        while (avio_tell(pb) + 4 < len) {
            int start_off = avio_tell(pb);

            mflags = avio_r8(pb);
            if (mflags & 0x20)   /**< relative timestamp */
                avio_skip(pb, 4);
            if (mflags & 0x10)   /**< has duration */
                avio_skip(pb, 4);
            if (mflags & 0x8)    /**< has location ID */
                avio_skip(pb, 4);
            off = avio_tell(pb);

            if (!(mflags & 0x40)) {
                if (asf->pktbuf && len_off != avio_tell(asf->pktbuf)) {
                    ffio_free_dyn_buf(&asf->pktbuf);
                }
                if (!len_off && !asf->pktbuf &&
                    (res = avio_open_dyn_buf(&asf->pktbuf)) < 0)
                    return res;
                if (!asf->pktbuf)
                    return AVERROR(EIO);

                avio_write(asf->pktbuf, buf + off, len - off);
                avio_skip(pb, len - off);
                if (!(flags & RTP_FLAG_MARKER))
                    return -1;
                out_len     = avio_close_dyn_buf(asf->pktbuf, &asf->buf);
                asf->pktbuf = NULL;
            } else {
                int cur_len = start_off + len_off - off;
                int prev_len = out_len;
                out_len += cur_len;
                if (FFMIN(cur_len, len - off) < 0)
                    return -1;
                if ((res = av_reallocp(&asf->buf, out_len)) < 0)
                    return res;
                memcpy(asf->buf + prev_len, buf + off,
                       FFMIN(cur_len, len - off));
                avio_skip(pb, cur_len);
            }
        }

        init_packetizer(pb, asf->buf, out_len);
        pb->pos += rt->asf_pb_pos;
        pb->eof_reached = 0;
        rt->asf_ctx->pb = pb;
    }

    for (;;) {
        int i;

        res = ff_read_packet(rt->asf_ctx, pkt);
        rt->asf_pb_pos = avio_tell(pb);
        if (res != 0)
            break;
        for (i = 0; i < s->nb_streams; i++) {
            if (s->streams[i]->id == rt->asf_ctx->streams[pkt->stream_index]->id) {
                pkt->stream_index = i;
                return 1;
            }
        }
        av_free_packet(pkt);
    }

    return res == 1 ? -1 : res;
}
```
