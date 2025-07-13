
// Modified section only - rest of the file remains unchanged

static int sync(AVFormatContext *s, int64_t *timestamp, int *flags, int *stream_index, int64_t *pos){
    RMDemuxContext *rm = s->priv_data;
    AVIOContext *pb = s->pb;
    AVStream *st;
    uint32_t state=0xFFFFFFFF;

    while(!pb->eof_reached){
        int len, num, i;
        *pos= avio_tell(pb) - 3;
        if(rm->remaining_len > 0){
            num= rm->current_stream;
            len= rm->remaining_len;
            *timestamp = AV_NOPTS_VALUE;
            *flags= 0;
        }else{
            state= (state<<8) + avio_r8(pb);

            if(state == MKBETAG('I', 'N', 'D', 'X')){
                int n_pkts, expected_len;
                len = avio_rb32(pb);
                avio_skip(pb, 2);
                n_pkts = avio_rb32(pb);
                expected_len = 20 + n_pkts * 14;
                if (len == 20)
                    len = expected_len;
                else if (len != expected_len)
                    av_log(s, AV_LOG_WARNING,
                           "Index size %d (%d pkts) is wrong, should be %d.
",
                           len, n_pkts, expected_len);
                len -= 14;
                if(len<0)
                    continue;
                goto skip;
            } else if (state == MKBETAG('D','A','T','A')) {
                av_log(s, AV_LOG_ERROR,
                       "Demux error: Unexpected DATA tag encountered mid-chunk. Stream may be malformed or corrupted.
");
            }

            if(state > (unsigned)0xFFFF || state <= 12)
                continue;
            len=state - 12;
            state= 0xFFFFFFFF;

            num = avio_rb16(pb);
            *timestamp = avio_rb32(pb);
            avio_r8(pb);
            *flags = avio_r8(pb);
        }
        for(i=0;i<s->nb_streams;i++) {
            st = s->streams[i];
            if (num == st->id)
                break;
        }
        if (i == s->nb_streams) {
skip:
            avio_skip(pb, len);
            rm->remaining_len = 0;
            continue;
        }
        *stream_index= i;

        return len;
    }
    return -1;
}
