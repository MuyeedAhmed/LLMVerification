Do not provide any extra information.

static int rm_read_audio_stream_info(AVFormatContext *s, AVIOContext *pb,
                                     AVStream *st, RMStream *ast, int read_all)
{
    char buf[256];
    uint32_t version;
    int ret;

    /* ra type header */
    version = avio_rb16(pb); /* version */
    if (version == 3) {
        int header_size = avio_rb16(pb);
        int64_t startpos = avio_tell(pb);
        avio_skip(pb, 14);
        rm_read_metadata(s, pb, 0);
        if ((startpos + header_size) >= avio_tell(pb) + 2) {
            // fourcc (should always be "lpcJ")
            avio_r8(pb);
            get_str8(pb, buf, sizeof(buf));
        }
        // Skip extra header crap (this should never happen)
        if ((startpos + header_size) > avio_tell(pb))
            avio_skip(pb, header_size + startpos - avio_tell(pb));
        st->codec->sample_rate = 8000;
        st->codec->channels = 1;
        st->codec->channel_layout = AV_CH_LAYOUT_MONO;
        st->codec->codec_type = AVMEDIA_TYPE_AUDIO;
        st->codec->codec_id = AV_CODEC_ID_RA_144;
        ast->deint_id = DEINT_ID_INT0;
    } else {
        int flavor, sub_packet_h, coded_framesize, sub_packet_size;
        int codecdata_length;
        /* old version (4) */
        avio_skip(pb, 2); /* unused */
        avio_rb32(pb); /* .ra4 */
        avio_rb32(pb); /* data size */
        avio_rb16(pb); /* version2 */
        avio_rb32(pb); /* header size */
        flavor= avio_rb16(pb); /* add codec info / flavor */
        ast->coded_framesize = coded_framesize = avio_rb32(pb); /* coded frame size */
        avio_rb32(pb); /* ??? */
        avio_rb32(pb); /* ??? */
        avio_rb32(pb); /* ??? */
        ast->sub_packet_h = sub_packet_h = avio_rb16(pb); /* 1 */
        st->codec->block_align= avio_rb16(pb); /* frame size */
        ast->sub_packet_size = sub_packet_size = avio_rb16(pb); /* sub packet size */
        avio_rb16(pb); /* ??? */
        if (version == 5) {
            avio_rb16(pb); avio_rb16(pb); avio_rb16(pb);
        }
        st->codec->sample_rate = avio_rb16(pb);
        avio_rb32(pb);
        st->codec->channels = avio_rb16(pb);
        if (version == 5) {
            ast->deint_id = avio_rl32(pb);
            avio_read(pb, buf, 4);
            buf[4] = 0;
        } else {
            get_str8(pb, buf, sizeof(buf)); /* desc */
            ast->deint_id = AV_RL32(buf);
            get_str8(pb, buf, sizeof(buf)); /* desc */
        }
        st->codec->codec_type = AVMEDIA_TYPE_AUDIO;
        st->codec->codec_tag  = AV_RL32(buf);
        st->codec->codec_id   = ff_codec_get_id(ff_rm_codec_tags,
                                                st->codec->codec_tag);

        switch (st->codec->codec_id) {
        case AV_CODEC_ID_AC3:
            st->need_parsing = AVSTREAM_PARSE_FULL;
            break;
        case AV_CODEC_ID_RA_288:
            st->codec->extradata_size= 0;
            ast->audio_framesize = st->codec->block_align;
            st->codec->block_align = coded_framesize;
            break;
        case AV_CODEC_ID_COOK:
            st->need_parsing = AVSTREAM_PARSE_HEADERS;
        case AV_CODEC_ID_ATRAC3:
        case AV_CODEC_ID_SIPR:
            avio_rb16(pb); avio_r8(pb);
            if (version == 5)
                avio_r8(pb);
            codecdata_length = avio_rb32(pb);
            if(codecdata_length + FF_INPUT_BUFFER_PADDING_SIZE <= (unsigned)codecdata_length){
                av_log(s, AV_LOG_ERROR, "codecdata_length too large\n");
                return -1;
            }

            ast->audio_framesize = st->codec->block_align;
            if (st->codec->codec_id == AV_CODEC_ID_SIPR) {
                if (flavor > 3) {
                    av_log(s, AV_LOG_ERROR, "bad SIPR file flavor %d\n",
                           flavor);
                    return -1;
                }
                st->codec->block_align = ff_sipr_subpk_size[flavor];
            } else {
                if(sub_packet_size <= 0){
                    av_log(s, AV_LOG_ERROR, "sub_packet_size is invalid\n");
                    return -1;
                }
                st->codec->block_align = ast->sub_packet_size;
            }
            if ((ret = rm_read_extradata(pb, st->codec, codecdata_length)) < 0)
                return ret;
            break;
        case AV_CODEC_ID_AAC:
            avio_rb16(pb); avio_r8(pb);
            if (version == 5)
                avio_r8(pb);
            codecdata_length = avio_rb32(pb);
            if(codecdata_length + FF_INPUT_BUFFER_PADDING_SIZE <= (unsigned)codecdata_length){
                av_log(s, AV_LOG_ERROR, "codecdata_length too large\n");
                return -1;
            }
            if (codecdata_length >= 1) {
                avio_r8(pb);
                if ((ret = rm_read_extradata(pb, st->codec, codecdata_length - 1)) < 0)
                    return ret;
            }
            break;
        }
        if (ast->deint_id == DEINT_ID_INT4 ||
            ast->deint_id == DEINT_ID_GENR ||
            ast->deint_id == DEINT_ID_SIPR) {
            if (st->codec->block_align <= 0 ||
                ast->audio_framesize * sub_packet_h > (unsigned)INT_MAX ||
                ast->audio_framesize * sub_packet_h < st->codec->block_align)
                return AVERROR_INVALIDDATA;
            if (av_new_packet(&ast->pkt, ast->audio_framesize * sub_packet_h) < 0)
                return AVERROR(ENOMEM);
        }
        switch (ast->deint_id) {
        case DEINT_ID_INT4:
            if (ast->coded_framesize > ast->audio_framesize ||
                sub_packet_h <= 1 ||
                ast->coded_framesize * sub_packet_h > (2 + (sub_packet_h & 1)) * ast->audio_framesize)
                return AVERROR_INVALIDDATA;
            break;
        case DEINT_ID_GENR:
            if (ast->sub_packet_size <= 0 ||
                ast->sub_packet_size > ast->audio_framesize)
                return AVERROR_INVALIDDATA;
            break;
        case DEINT_ID_SIPR:
        case DEINT_ID_INT0:
        case DEINT_ID_VBRS:
        case DEINT_ID_VBRF:
            break;
        default:
            av_log(s, AV_LOG_ERROR, "Unknown interleaver %"PRIX32"\n", ast->deint_id);
            return AVERROR_INVALIDDATA;
        }

        if (read_all) {
            avio_r8(pb);
            avio_r8(pb);
            avio_r8(pb);
            rm_read_metadata(s, pb, 0);
        }
    }
    return 0;
}
