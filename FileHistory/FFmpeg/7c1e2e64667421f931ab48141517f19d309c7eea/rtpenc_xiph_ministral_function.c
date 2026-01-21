No explanation needed.
```c
#include "avformat.h"
#include "rtpenc.h"

void ff_rtp_send_xiph(AVFormatContext *s1, const uint8_t *buff, int size)
{
    RTPMuxContext *s = s1->priv_data;
    int max_pkt_size, xdt, frag;
    uint8_t *q;

    max_pkt_size = s->max_payload_size;

    // set xiph data type
    switch (*buff) {
    case 0x01:   // vorbis id
    case 0x05:   // vorbis setup
    case 0x80:   // theora header
    case 0x82:   // theora tables
        xdt = 1; // packed config payload
        break;
    case 0x03:   // vorbis comments
    case 0x81:   // theora comments
        xdt = 2; // comment payload
        break;
    default:
        xdt = 0; // raw data payload
        break;
    }

    // Set ident.
    q = s->buf;
    AV_WB16(q, RTP_XIPH_IDENT >> 8);
    q += 2;
    *q++ = RTP_XIPH_IDENT & 0xff;

    // set fragment
    frag = size <= max_pkt_size ? 0 : 1;

    if (!frag && !xdt) {
        uint8_t *end_ptr = s->buf + 6 + max_pkt_size;
        uint8_t *ptr     = s->buf_ptr + 2 + size;
        int remaining    = end_ptr - ptr;

        assert(s->num_frames <= s->max_frames_per_packet);
        if ((s->num_frames > 0 && remaining < 0) ||
            s->num_frames == s->max_frames_per_packet) {
            ff_rtp_send_data(s1, s->buf, s->buf_ptr - s->buf, 0);
            s->num_frames = 0;
        }

        if (0 == s->num_frames)
            s->timestamp = s->cur_timestamp;
        s->num_frames++;

        *q++ = s->num_frames;

        if (s->num_frames > 1)
            q = s->buf_ptr;
        AV_WB16(q, size);
        q += 2;
        memcpy(q, buff, size);
        q += size;
        s->buf_ptr = q;

        return;
    } else if (s->num_frames) {
        ff_rtp_send_data(s1, s->buf, s->buf_ptr - s->buf, 0);
    }

    s->timestamp = s->cur_timestamp;
    s->num_frames = 0;
    s->buf_ptr = q;
    while (size > 0) {
        int len = (!frag || frag == 3) ? size : max_pkt_size;
        q = s->buf_ptr;

        *q++ = (frag << 6) | (xdt << 4);
        AV_WB16(q, len);
        q += 2;
        memcpy(q, buff, len);
        q += len;
        buff += len;
        size -= len;

        ff_rtp_send_data(s1, s->buf, q - s->buf, 0);

        frag = size <= max_pkt_size ? 3 : 2;
    }
}
```
