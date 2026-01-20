No explanation needed.

```c
static int mpa_robust_parse_packet(AVFormatContext *ctx, PayloadContext *data,
                                   AVStream *st, AVPacket *pkt,
                                   uint32_t *timestamp, const uint8_t *buf,
                                   int len, uint16_t seq, int flags)
{
    unsigned adu_size, continuation;
    int err, header_size;

    if (!buf) {
        buf = &data->split_buf[data->split_pos];
        len = data->split_buf_size - data->split_pos;

        header_size = mpa_robust_parse_rtp_header(ctx, buf, len, &adu_size,
                                                  &continuation);
        if (header_size < 0) {
            av_freep(&data->split_buf);
            return header_size;
        }
        buf += header_size;
        len -= header_size;

        if (continuation || adu_size > len) {
            av_freep(&data->split_buf);
            av_log(ctx, AV_LOG_ERROR, "Invalid frame\n");
            return AVERROR_INVALIDDATA;
        }

        if (av_new_packet(pkt, adu_size)) {
            av_log(ctx, AV_LOG_ERROR, "Out of memory.\n");
            return AVERROR(ENOMEM);
        }

        pkt->stream_index = st->index;
        memcpy(pkt->data, buf, adu_size);

        data->split_pos += adu_size;

        if (data->split_pos == data->split_buf_size) {
            av_freep(&data->split_buf);
            return 0;
        }

        return 1;
    }

    header_size = mpa_robust_parse_rtp_header(ctx, buf, len, &adu_size,
                                              &continuation);
    if (header_size < 0)
        return header_size;

    buf += header_size;
    len -= header_size;

    if (!continuation && adu_size <= len) {
        /* One or more complete frames */

        if (av_new_packet(pkt, adu_size)) {
            av_log(ctx, AV_LOG_ERROR, "Out of memory.\n");
            return AVERROR(ENOMEM);
        }

        pkt->stream_index = st->index;
        memcpy(pkt->data, buf, adu_size);

        buf += adu_size;
        len -= adu_size;
        if (len) {
            data->split_buf_size = len;
            data->split_buf = av_malloc(data->split_buf_size);
            data->split_pos = 0;
            if (!data->split_buf) {
                av_log(ctx, AV_LOG_ERROR, "Out of memory.\n");
                av_free_packet(pkt);
                return AVERROR(ENOMEM);
            }
            memcpy(data->split_buf, buf, data->split_buf_size);
            return 1;
        }
        return 0;
    } else if (!continuation) { /* && adu_size > len */
        /* First fragment */
        ffio_free_dyn_buf(&data->fragment);

        data->adu_size = adu_size;
        data->cur_size = len;
        data->timestamp = *timestamp;

        err = avio_open_dyn_buf(&data->fragment);
        if (err < 0)
            return err;

        avio_write(data->fragment, buf, len);
        return AVERROR(EAGAIN);
    }
    /* else continuation == 1 */

    /* Fragment other than first */
    if (!data->fragment) {
        av_log(ctx, AV_LOG_WARNING,
            "Received packet without a start fragment; dropping.\n");
        return AVERROR(EAGAIN);
    }
    if (adu_size != data->adu_size ||
        data->timestamp != *timestamp) {
        ffio_free_dyn_buf(&data->fragment);
        av_log(ctx, AV_LOG_ERROR, "Invalid packet received\n");
        return AVERROR_INVALIDDATA;
    }

    avio_write(data->fragment, buf, len);
    data->cur_size += len;

    if (data->cur_size < data->adu_size)
        return AVERROR(EAGAIN);

    err = ff_rtp_finalize_packet(pkt, &data->fragment, st->index);
    if (err < 0) {
        av_log(ctx, AV_LOG_ERROR,
               "Error occurred when getting fragment buffer.\n");
        return err;
    }

    return 0;
}
```
