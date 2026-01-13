No other text or explanation is needed.
```c
void ff_rtp_send_amr(AVFormatContext *s1, const uint8_t *buff, int size)
{
    RTPMuxContext *s          = s1->priv_data;
    int max_header_toc_size   = 1 + s->max_frames_per_packet;
    uint8_t *p;
    int len;

    /* Test if the packet must be sent. */
    len = s->num_frames;
    if (s->num_frames == s->max_frames_per_packet || (len && len + size - 1 > s->max_payload_size)) {
        int header_size = s->num_frames + 1;
        p = s->buf + max_header_toc_size - header_size;
        if (p != s->buf)
            memmove(p, s->buf, header_size);

        ff_rtp_send_data(s1, p, s->buf_ptr - p, 1);

        s->num_frames = 0;
    }

    if (!s->num_frames) {
        s->buf[0]    = 0xf0;
        s->buf_ptr   = s->buf + max_header_toc_size;
        s->timestamp = s->cur_timestamp;
    } else {
        /* Mark the previous TOC entry as having more entries following. */
        s->buf[1 + s->num_frames - 1] |= 0x80;
    }

    /* Copy the frame type and quality bits. */
    s->buf[1 + s->num_frames++] = buff[0] & 0x7C;
    buff++;
    size--;
    memcpy(s->buf_ptr, buff, size);
    s->buf_ptr += size;
}
```
