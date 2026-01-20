No explanation needed.

```c
static int audio_element_obu(void *s, IAMFContext *c, AVIOContext *pb, int len)
{
    const IAMFCodecConfig *codec_config;
    AVIAMFAudioElement *element;
    IAMFAudioElement **tmp, *audio_element = NULL;
    FFIOContext b;
    AVIOContext *pbc;
    uint8_t *buf;
    unsigned audio_element_id, nb_substreams, codec_config_id, num_parameters;
    int audio_element_type, ret;

    buf = av_malloc(len);
    if (!buf)
        return AVERROR(ENOMEM);

    ret = ffio_read_size(pb, buf, len);
    if (ret < 0)
        goto fail;

    ffio_init_context(&b, buf, len, 0, NULL, NULL, NULL, NULL);
    pbc = &b.pub;

    audio_element_id = ffio_read_leb(pbc);

    for (int i = 0; i < c->nb_audio_elements; i++)
        if (c->audio_elements[i]->audio_element_id == audio_element_id) {
            av_log(s, AV_LOG_ERROR, "Duplicate audio_element_id %d\n", audio_element_id);
            ret = AVERROR_INVALIDDATA;
            goto fail;
        }

    audio_element_type = avio_r8(pbc) >> 5;
    if (audio_element_type > AV_IAMF_AUDIO_ELEMENT_TYPE_SCENE) {
        av_log(s, AV_LOG_DEBUG, "Unknown audio_element_type referenced in an audio element. Ignoring\n");
        ret = 0;
        goto fail;
    }

    codec_config_id = ffio_read_leb(pbc);

    codec_config = ff_iamf_get_codec_config(c, codec_config_id);
    if (!codec_config) {
        av_log(s, AV_LOG_ERROR, "Non existant codec config id %d referenced in an audio element\n", codec_config_id);
        ret = AVERROR_INVALIDDATA;
        goto fail;
    }

    if (codec_config->codec_id == AV_CODEC_ID_NONE) {
        av_log(s, AV_LOG_DEBUG, "Unknown codec id referenced in an audio element. Ignoring\n");
        ret = 0;
        goto fail;
    }

    tmp = av_realloc_array(c->audio_elements, c->nb_audio_elements + 1, sizeof(*c->audio_elements));
    if (!tmp) {
        ret = AVERROR(ENOMEM);
        goto fail;
    }
    c->audio_elements = tmp;

    audio_element = av_mallocz(sizeof(*audio_element));
    if (!audio_element) {
        ret = AVERROR(ENOMEM);
        goto fail;
    }

    nb_substreams = ffio_read_leb(pbc);
    audio_element->codec_config_id = codec_config_id;
    audio_element->audio_element_id = audio_element_id;
    audio_element->substreams = av_calloc(nb_substreams, sizeof(*audio_element->substreams));
    if (!audio_element->substreams) {
        ret = AVERROR(ENOMEM);
        goto fail;
    }
    audio_element->nb_substreams = nb_substreams;

    element = audio_element->element = av_iamf_audio_element_alloc();
    if (!element) {
        ret = AVERROR(ENOMEM);
        goto fail;
    }
    audio_element->celement = element;

    element->audio_element_type = audio_element_type;

    for (int i = 0; i < audio_element->nb_substreams; i++) {
        IAMFSubStream *substream = &audio_element->substreams[i];

        substream->codecpar = avcodec_parameters_alloc();
        if (!substream->codecpar) {
            ret = AVERROR(ENOMEM);
            goto fail;
        }

        substream->audio_substream_id = ffio_read_leb(pbc);

        substream->codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
        substream->codecpar->codec_id   = codec_config->codec_id;
        substream->codecpar->frame_size = codec_config->nb_samples;
        substream->codecpar->sample_rate = codec_config->sample_rate;
        substream->codecpar->seek_preroll = -codec_config->audio_roll_distance * codec_config->nb_samples;

        switch(substream->codecpar->codec_id) {
        case AV_CODEC_ID_AAC:
        case AV_CODEC_ID_FLAC:
        case AV_CODEC_ID_OPUS:
            substream->codecpar->extradata = av_malloc(codec_config->extradata_size + AV_INPUT_BUFFER_PADDING_SIZE);
            if (!substream->codecpar->extradata) {
                ret = AVERROR(ENOMEM);
                goto fail;
            }
            memcpy(substream->codecpar->extradata, codec_config->extradata, codec_config->extradata_size);
            memset(substream->codecpar->extradata + codec_config->extradata_size, 0, AV_INPUT_BUFFER_PADDING_SIZE);
            substream->codecpar->extradata_size = codec_config->extradata_size;
            break;
        }
    }

    // Check for duplicate parameter types
    int has_mix_gain = 0, has_demixing = 0, has_recon_gain = 0;
    num_parameters = ffio_read_leb(pbc);
    if (num_parameters > 2 && audio_element_type == 0) {
        av_log(s, AV_LOG_ERROR, "Audio Element parameter count %u is invalid"
                                " for Channel representations\n", num_parameters);
        ret = AVERROR_INVALIDDATA;
        goto fail;
    }
    if (num_parameters && audio_element_type != 0) {
        av_log(s, AV_LOG_ERROR, "Audio Element parameter count %u is invalid"
                                " for Scene representations\n", num_parameters);
        ret = AVERROR_INVALIDDATA;
        goto fail;
    }

    for (int i = 0; i < num_parameters; i++) {
        unsigned type;

        type = ffio_read_leb(pbc);
        if (type == AV_IAMF_PARAMETER_DEFINITION_MIX_GAIN) {
            if (has_mix_gain) {
                av_log(s, AV_LOG_ERROR, "Duplicate parameter type %u in audio element\n", type);
                ret = AVERROR_INVALIDDATA;
                goto fail;
            }
            has_mix_gain = 1;
        } else if (type == AV_IAMF_PARAMETER_DEFINITION_DEMIXING) {
            if (has_demixing) {
                av_log(s, AV_LOG_ERROR, "Duplicate parameter type %u in audio element\n", type);
                ret = AVERROR_INVALIDDATA;
                goto fail;
            }
            has_demixing = 1;
            ret = param_parse(s, c, pbc, type, audio_element, &element->demixing_info);
        } else if (type == AV_IAMF_PARAMETER_DEFINITION_RECON_GAIN) {
            if (has_recon_gain) {
                av_log(s, AV_LOG_ERROR, "Duplicate parameter type %u in audio element\n", type);
                ret = AVERROR_INVALIDDATA;
                goto fail;
            }
            has_recon_gain = 1;
            ret = param_parse(s, c, pbc, type, audio_element, &element->recon_gain_info);
        } else {
            unsigned param_definition_size = ffio_read_leb(pbc);
            avio_skip(pbc, param_definition_size);
        }
        if (ret < 0)
            goto fail;
    }

    if (audio_element_type == AV_IAMF_AUDIO_ELEMENT_TYPE_CHANNEL) {
        ret = scalable_channel_layout_config(s, pbc, audio_element, codec_config);
        if (ret < 0)
            goto fail;
    } else if (audio_element_type == AV_IAMF_AUDIO_ELEMENT_TYPE_SCENE) {
        ret = ambisonics_config(s, pbc, audio_element, codec_config);
        if (ret < 0)
            goto fail;
    } else {
        av_assert0(0);
    }

    c->audio_elements[c->nb_audio_elements++] = audio_element;

    len -= avio_tell(pbc);
    if (len)
       av_log(s, AV_LOG_WARNING, "Underread in audio_element_obu. %d bytes left at the end\n", len);

    ret = 0;
fail:
    av_free(buf);
    if (ret < 0)
        ff_iamf_free_audio_element(&audio_element);
    return ret;
}
```
