Do not include any additional text or explanations.
```c
static int append_flv_data(RTMPContext *rt, RTMPPacket *pkt, int skip)
{
    int old_flv_size, ret;
    PutByteContext pbc;
    const uint8_t *data = pkt->data + skip;
    const int size      = pkt->size - skip;
    uint32_t ts         = pkt->timestamp;

    if (pkt->type == RTMP_PT_AUDIO) {
        rt->has_audio = 1;
    } else if (pkt->type == RTMP_PT_VIDEO) {
        rt->has_video = 1;
    }

    old_flv_size = update_offset(rt, size + 15);

    if ((ret = av_reallocp(&rt->flv_data, rt->flv_size)) < 0) {
        rt->flv_size = rt->flv_off = 0;
        return ret;
    }
    bytestream2_init_writer(&pbc, rt->flv_data, rt->flv_size);
    bytestream2_skip_p(&pbc, old_flv_size);
    bytestream2_put_byte(&pbc, pkt->type);
    bytestream2_put_be24(&pbc, size);
    bytestream2_put_be24(&pbc, ts);
    bytestream2_put_byte(&pbc, ts >> 24);
    bytestream2_put_be24(&pbc, 0);
    bytestream2_put_buffer(&pbc, data, size);
    bytestream2_put_be32(&pbc, size + RTMP_HEADER);

    return 0;
}
```
