Do not include any other text or explanation.
```c
static void sdp_parse_line(AVFormatContext *s, SDPParseState *s1,
                           int letter, const char *buf)
{
    RTSPState *rt = s->priv_data;
    char buf1[64], st_type[64];
    const char *p;
    enum AVMediaType codec_type;
    int payload_type;
    AVStream *st;
    RTSPStream *rtsp_st;
    RTSPSource *rtsp_src;
    struct sockaddr_storage sdp_ip;
    int ttl;

    av_dlog(s, "sdp: %c='%s'\n", letter, buf);

    p = buf;
    if (s1->skip_media && letter != 'm')
        return;
    switch (letter) {
    case 'c':
        get_word(buf1, sizeof(buf1), &p);
        if (strcmp(buf1, "IN") != 0)
            return;
        get_word(buf1, sizeof(buf1), &p);
        if (strcmp(buf1, "IP4") && strcmp(buf1, "IP6"))
            return;
        get_word_sep(buf1, sizeof(buf1), "/", &p);
        if (get_sockaddr(buf1, &sdp_ip))
            return;
        ttl = 16;
        if (*p == '/') {
            p++;
            get_word_sep(buf1, sizeof(buf1), "/", &p);
            ttl = atoi(buf1);
        }
        if (s->nb_streams == 0) {
            s1->default_ip = sdp_ip;
            s1->default_ttl = ttl;
        } else {
            rtsp_st = rt->rtsp_streams[rt->nb_rtsp_streams - 1];
            rtsp_st->sdp_ip = sdp_ip;
            rtsp_st->sdp_ttl = ttl;
        }
        break;
    case 's':
        av_dict_set(&s->metadata, "title", p, 0);
        break;
    case 'i':
        if (s->nb_streams == 0) {
            av_dict_set(&s->metadata, "comment", p, 0);
            break;
        }
        break;
    case 'm':
        /* new stream */
        s1->skip_media  = 0;
        s1->seen_fmtp   = 0;
        s1->seen_rtpmap = 0;
        codec_type = AVMEDIA_TYPE_UNKNOWN;
        get_word(st_type, sizeof(st_type), &p);
        if (!strcmp(st_type, "audio")) {
            codec_type = AVMEDIA_TYPE_AUDIO;
        } else if (!strcmp(st_type, "video")) {
            codec_type = AVMEDIA_TYPE_VIDEO;
        } else if (!strcmp(st_type, "application")) {
            codec_type = AVMEDIA_TYPE_DATA;
        }
        if (codec_type == AVMEDIA_TYPE_UNKNOWN || !(rt->media_type_mask & (1 << codec_type))) {
            s1->skip_media = 1;
            return;
        }
        rtsp_st = av_mallocz(sizeof(RTSPStream));
        if (!rtsp_st)
            return;
        rtsp_st->stream_index = -1;
        dynarray_add(&rt->rtsp_streams, &rt->nb_rtsp_streams, rtsp_st);

        rtsp_st->sdp_ip = s1->default_ip;
        rtsp_st->sdp_ttl = s1->default_ttl;

        copy_default_source_addrs(s1->default_include_source_addrs,
                                  s1->nb_default_include_source_addrs,
                                  &rtsp_st->include_source_addrs,
                                  &rtsp_st->nb_include_source_addrs);
        copy_default_source_addrs(s1->default_exclude_source_addrs,
                                  s1->nb_default_exclude_source_addrs,
                                  &rtsp_st->exclude_source_addrs,
                                  &rtsp_st->nb_exclude_source_addrs);

        get_word(buf1, sizeof(buf1), &p); /* port */
        rtsp_st->sdp_port = atoi(buf1);

        get_word(buf1, sizeof(buf1), &p); /* protocol */
        if (!strcmp(buf1, "udp"))
            rt->transport = RTSP_TRANSPORT_RAW;
        else if (strstr(buf1, "/AVPF") || strstr(buf1, "/SAVPF"))
            rtsp_st->feedback = 1;

        /* XXX: handle list of formats */
        get_word(buf1, sizeof(buf1), &p); /* format list */
        rtsp_st->sdp_payload_type = atoi(buf1);

        if (!strcmp(ff_rtp_enc_name(rtsp_st->sdp_payload_type), "MP2T")) {
            /* no corresponding stream */
            if (rt->transport == RTSP_TRANSPORT_RAW) {
                if (CONFIG_RTPDEC && !rt->ts)
                    rt->ts = ff_mpegts_parse_open(s);
            } else {
                RTPDynamicProtocolHandler *handler;
                handler = ff_rtp_handler_find_by_id(
                              rtsp_st->sdp_payload_type, AVMEDIA_TYPE_DATA);
                init_rtp_handler(handler, rtsp_st, NULL);
                if (handler && handler->init)
                    handler->init(s, -1, rtsp_st->dynamic_protocol_context);
            }
        } else if (rt->server_type == RTSP_SERVER_WMS &&
                   codec_type == AVMEDIA_TYPE_DATA) {
            /* RTX stream, a stream that carries all the other actual
             * audio/video streams. Don't expose this to the callers. */
        } else {
            st = avformat_new_stream(s, NULL);
            if (!st)
                return;
            st->id = rt->nb_rtsp_streams - 1;
            rtsp_st->stream_index = st->index;
            st->codec->codec_type = codec_type;
            if (rtsp_st->sdp_payload_type < RTP_PT_PRIVATE) {
                RTPDynamicProtocolHandler *handler;
                /* if standard payload type, we can find the codec right now */
                ff_rtp_get_codec_info(st->codec, rtsp_st->sdp_payload_type);
                if (st->codec->codec_type == AVMEDIA_TYPE_AUDIO &&
                    st->codec->sample_rate > 0)
                    avpriv_set_pts_info(st, 32, 1, st->codec->sample_rate);
                /* Even static payload types may need a custom depacketizer */
                handler = ff_rtp_handler_find_by_id(
                              rtsp_st->sdp_payload_type, st->codec->codec_type);
                init_rtp_handler(handler, rtsp_st, st->codec);
                if (handler && handler->init)
                    handler->init(s, st->index,
                                  rtsp_st->dynamic_protocol_context);
            }
            if (rt->default_lang[0])
                av_dict_set(&st->metadata, "language", rt->default_lang, 0);
        }
        /* put a default control url */
        av_strlcpy(rtsp_st->control_url, rt->control_uri,
                   sizeof(rtsp_st->control_url));
        break;
    case 'a':
        if (av_strstart(p, "control:", &p)) {
            if (s->nb_streams == 0) {
                if (!strncmp(p, "rtsp://", 7))
                    av_strlcpy(rt->control_uri, p,
                               sizeof(rt->control_uri));
            } else {
                char proto[32];
                /* get the control url */
                rtsp_st = rt->rtsp_streams[rt->nb_rtsp_streams - 1];

                /* XXX: may need to add full url resolution */
                av_url_split(proto, sizeof(proto), NULL, 0, NULL, 0,
                             NULL, NULL, 0, p);
                if (proto[0] == '\0') {
                    /* relative control URL */
                    if (rtsp_st->control_url[strlen(rtsp_st->control_url)-1]!='/')
                    av_strlcat(rtsp_st->control_url, "/",
                               sizeof(rtsp_st->control_url));
                    av_strlcat(rtsp_st->control_url, p,
                               sizeof(rtsp_st->control_url));
                } else
                    av_strlcpy(rtsp_st->control_url, p,
                               sizeof(rtsp_st->control_url));
            }
        } else if (av_strstart(p, "rtpmap:", &p) && s->nb_streams > 0) {
            /* NOTE: rtpmap is only supported AFTER the 'm=' tag */
            get_word(buf1, sizeof(buf1), &p);
            payload_type = atoi(buf1);
            rtsp_st = rt->rtsp_streams[rt->nb_rtsp_streams - 1];
            if (rtsp_st->stream_index >= 0) {
                st = s->streams[rtsp_st->stream_index];
                sdp_parse_rtpmap(s, st, rtsp_st, payload_type, p);
            }
            s1->seen_rtpmap = 1;
            if (s1->seen_fmtp) {
                parse_fmtp(s, rt, payload_type, s1->delayed_fmtp);
            }
        } else if (av_strstart(p, "fmtp:", &p) ||
                   av_strstart(p, "framesize:", &p)) {
            // let dynamic protocol handlers have a stab at the line.
            get_word(buf1, sizeof(buf1), &p);
            payload_type = atoi(buf1);
            if (s1->seen_rtpmap) {
                parse_fmtp(s, rt, payload_type, buf);
            } else {
                s1->seen_fmtp = 1;
                av_strlcpy(s1->delayed_fmtp, buf, sizeof(s1->delayed_fmtp));
            }
        } else if (av_strstart(p, "range:", &p)) {
            int64_t start, end;

            // this is so that seeking on a streamed file can work.
            rtsp_parse_range_npt(p, &start, &end);
            s->start_time = start;
            /* AV_NOPTS_VALUE means live broadcast (and can't seek) */
            s->duration   = (end == AV_NOPTS_VALUE) ?
                            AV_NOPTS_VALUE : end - start;
        } else if (av_strstart(p, "lang:", &p)) {
            if (s->nb_streams > 0) {
                get_word(buf1, sizeof(buf1), &p);
                rtsp_st = rt->rtsp_streams[rt->nb_rtsp_streams - 1];
                if (rtsp_st->stream_index >= 0) {
                    st = s->streams[rtsp_st->stream_index];
                    av_dict_set(&st->metadata, "language", buf1, 0);
                }
            } else
                get_word(rt->default_lang, sizeof(rt->default_lang), &p);
        } else if (av_strstart(p, "IsRealDataType:integer;",&p)) {
            if (atoi(p) == 1)
                rt->transport = RTSP_TRANSPORT_RDT;
        } else if (av_strstart(p, "SampleRate:integer;", &p) &&
                   s->nb_streams > 0) {
            st = s->streams[s->nb_streams - 1];
            st->codec->sample_rate = atoi(p);
        } else if (av_strstart(p, "crypto:", &p) && s->nb_streams > 0) {
            // RFC 4568
            rtsp_st = rt->rtsp_streams[rt->nb_rtsp_streams - 1];
            get_word(buf1, sizeof(buf1), &p); // ignore tag
            get_word(rtsp_st->crypto_suite, sizeof(rtsp_st->crypto_suite), &p);
            p += strspn(p, SPACE_CHARS);
            if (av_strstart(p, "inline:", &p))
                get_word(rtsp_st->crypto_params, sizeof(rtsp_st->crypto_params), &p);
        } else if (av_strstart(p, "source-filter:", &p)) {
            int exclude = 0;
            get_word(buf1, sizeof(buf1), &p);
            if (strcmp(buf1, "incl") && strcmp(buf1, "excl"))
                return;
            exclude = !strcmp(buf1, "excl");

            get_word(buf1, sizeof(buf1), &p);
            if (strcmp(buf1, "IN") != 0)
                return;
            get_word(buf1, sizeof(buf1), &p);
            if (strcmp(buf1, "IP4") && strcmp(buf1, "IP6") && strcmp(buf1, "*"))
                return;
            // not checking that the destination address actually matches or is wildcard
            get_word(buf1, sizeof(buf1), &p);

            while (*p != '\0') {
                rtsp_src = av_mallocz(sizeof(*rtsp_src));
                if (!rtsp_src)
                    return;
                get_word(rtsp_src->addr, sizeof(rtsp_src->addr), &p);
                if (exclude) {
                    if (s->nb_streams == 0) {
                        dynarray_add(&s1->default_exclude_source_addrs, &s1->nb_default_exclude_source_addrs, rtsp_src);
                    } else {
                        rtsp_st = rt->rtsp_streams[rt->nb_rtsp_streams - 1];
                        dynarray_add(&rtsp_st->exclude_source_addrs, &rtsp_st->nb_exclude_source_addrs, rtsp_src);
                    }
                } else {
                    if (s->nb_streams == 0) {
                        dynarray_add(&s1->default_include_source_addrs, &s1->nb_default_include_source_addrs, rtsp_src);
                    } else {
                        rtsp_st = rt->rtsp_streams[rt->nb_rtsp_streams - 1];
                        dynarray_add(&rtsp_st->include_source_addrs, &rtsp_st->nb_include_source_addrs, rtsp_src);
                    }
                }
            }
        } else {
            if (rt->server_type == RTSP_SERVER_WMS)
                ff_wms_parse_sdp_a_line(s, p);
            if (s->nb_streams > 0) {
                rtsp_st = rt->rtsp_streams[rt->nb_rtsp_streams - 1];

                if (rt->server_type == RTSP_SERVER_REAL)
                    ff_real_parse_sdp_a_line(s, rtsp_st->stream_index, p);

                if (rtsp_st->dynamic_handler &&
                    rtsp_st->dynamic_handler->parse_sdp_a_line)
                    rtsp_st->dynamic_handler->parse_sdp_a_line(s,
                        rtsp_st->stream_index,
                        rtsp_st->dynamic_protocol_context, buf);
            }
        }
        break;
    }
}
```
