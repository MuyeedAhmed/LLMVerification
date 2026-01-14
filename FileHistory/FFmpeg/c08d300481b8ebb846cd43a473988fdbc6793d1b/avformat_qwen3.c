Do not include anything else.

void ff_flush_packet_queue(AVFormatContext *s)
{
    FormatContextInternal *const fci = ff_fc_internal(s);
    FFFormatContext *const si = &fci->fc;
    avpriv_packet_list_free(&fci->parse_queue);
    avpriv_packet_list_free(&si->packet_buffer);
    avpriv_packet_list_free(&fci->raw_packet_buffer);

    fci->raw_packet_buffer_size = 0;
}

void avformat_free_context(AVFormatContext *s)
{
    FormatContextInternal *fci;
    FFFormatContext *si;

    if (!s)
        return;
    fci = ff_fc_internal(s);
    si  = &fci->fc;

    if (s->oformat && ffofmt(s->oformat)->deinit && fci->initialized)
        ffofmt(s->oformat)->deinit(s);

    av_opt_free(s);
    if (s->iformat && s->iformat->priv_class && s->priv_data)
        av_opt_free(s->priv_data);
    if (s->oformat && s->oformat->priv_class && s->priv_data)
        av_opt_free(s->priv_data);

    for (unsigned i = 0; i < s->nb_streams; i++)
        ff_free_stream(&s->streams[i]);
    for (unsigned i = 0; i < s->nb_stream_groups; i++)
        ff_free_stream_group(&s->stream_groups[i]);
    s->nb_stream_groups = 0;
    s->nb_streams = 0;

    for (unsigned i = 0; i < s->nb_programs; i++) {
        av_dict_free(&s->programs[i]->metadata);
        av_freep(&s->programs[i]->stream_index);
        av_freep(&s->programs[i]);
    }
    s->nb_programs = 0;

    av_freep(&s->programs);
    av_freep(&s->priv_data);
    while (s->nb_chapters--) {
        av_dict_free(&s->chapters[s->nb_chapters]->metadata);
        av_freep(&s->chapters[s->nb_chapters]);
    }
    av_freep(&s->chapters);
    av_dict_free(&s->metadata);
    av_dict_free(&si->id3v2_meta);
    av_packet_free(&si->pkt);
    av_packet_free(&si->parse_pkt);
    av_freep(&s->streams);
    av_freep(&s->stream_groups);
    if (s->iformat)
        ff_flush_packet_queue(s);
    if (s->oformat)
        ff_flush_packet_queue(s);
    av_freep(&s->url);
    av_free(s);
}
