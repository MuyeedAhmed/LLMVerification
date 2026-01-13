Do NOT include any explanation or comments.

```c
static int add_param_definition(IAMFContext *iamf, AVIAMFParamDefinition *param,
                                const IAMFAudioElement *audio_element, void *log_ctx)
{
    IAMFParamDefinition **tmp, *param_definition;
    IAMFCodecConfig *codec_config = NULL;

    tmp = av_realloc_array(iamf->param_definitions, iamf->nb_param_definitions + 1,
                           sizeof(*iamf->param_definitions));
    if (!tmp)
        return AVERROR(ENOMEM);

    iamf->param_definitions = tmp;

    if (audio_element)
        codec_config = iamf->codec_configs[audio_element->codec_config_id];

    if (!param->parameter_rate) {
        if (!codec_config) {
            av_log(log_ctx, AV_LOG_ERROR, "parameter_rate needed but not set for parameter_id %u\n",
                   param->parameter_id);
            return AVERROR(EINVAL);
        }
        param->parameter_rate = codec_config->sample_rate;
    }
    if (codec_config) {
        if (!param->duration)
            param->duration = av_rescale(codec_config->nb_samples, param->parameter_rate, codec_config->sample_rate);
        if (!param->constant_subblock_duration)
            param->constant_subblock_duration = av_rescale(codec_config->nb_samples, param->parameter_rate, codec_config->sample_rate);
    }

    param_definition = av_mallocz(sizeof(*param_definition));
    if (!param_definition)
        return AVERROR(ENOMEM);

    param_definition->mode = !!param->duration;
    param_definition->param = param;
    param_definition->audio_element = audio_element;
    iamf->param_definitions[iamf->nb_param_definitions++] = param_definition;

    return 0;
}

int ff_iamf_add_audio_element(IAMFContext *iamf, const AVStreamGroup *stg, void *log_ctx)
{
    const AVIAMFAudioElement *iamf_audio_element;
    IAMFAudioElement **tmp, *audio_element;
    IAMFCodecConfig *codec_config;
    int ret;

    if (stg->type != AV_STREAM_GROUP_PARAMS_IAMF_AUDIO_ELEMENT)
        return AVERROR(EINVAL);
    if (!stg->nb_streams) {
        av_log(log_ctx, AV_LOG_ERROR, "Audio Element id %"PRId64" has no streams\n", stg->id);
        return AVERROR(EINVAL);
    }

    iamf_audio_element = stg->params.iamf_audio_element;
    if (iamf_audio_element->audio_element_type == AV_IAMF_AUDIO_ELEMENT_TYPE_SCENE) {
        const AVIAMFLayer *layer = iamf_audio_element->layers[0];
        if (iamf_audio_element->nb_layers != 1) {
            av_log(log_ctx, AV_LOG_ERROR, "Invalid amount of layers for SCENE_BASED audio element. Must be 1\n");
            return AVERROR(EINVAL);
        }
        if (layer->ch_layout.order != AV_CHANNEL_ORDER_CUSTOM &&
            layer->ch_layout.order != AV_CHANNEL_ORDER_AMBISONIC) {
            av_log(log_ctx, AV_LOG_ERROR, "Invalid channel layout for SCENE_BASED audio element\n");
            return AVERROR(EINVAL);
        }
        if (layer->ambisonics_mode >= AV_IAMF_AMBISONICS_MODE_PROJECTION) {
            av_log(log_ctx, AV_LOG_ERROR, "Unsuported ambisonics mode %d\n", layer->ambisonics_mode);
            return AVERROR_PATCHWELCOME;
        }
        for (int i = 0; i < stg->nb_streams; i++) {
            if (stg->streams[i]->codecpar->ch_layout.nb_channels > 1) {
                av_log(log_ctx, AV_LOG_ERROR, "Invalid amount of channels in a stream for MONO mode ambisonics\n");
                return AVERROR(EINVAL);
            }
        }
    } else
        for (int j, i = 0; i < iamf_audio_element->nb_layers; i++) {
            const AVIAMFLayer *layer = iamf_audio_element->layers[i];
            for (j = 0; j < FF_ARRAY_ELEMS(ff_iamf_scalable_ch_layouts); j++)
                if (!av_channel_layout_compare(&layer->ch_layout, &ff_iamf_scalable_ch_layouts[j]))
                    break;

            if (j >= FF_ARRAY_ELEMS(ff_iamf_scalable_ch_layouts)) {
                for (j = 0; j < FF_ARRAY_ELEMS(ff_iamf_expanded_scalable_ch_layouts); j++)
                    if (!av_channel_layout_compare(&layer->ch_layout, &ff_iamf_expanded_scalable_ch_layouts[j]))
                        break;
                if (j >= FF_ARRAY_ELEMS(ff_iamf_expanded_scalable_ch_layouts)) {
                    av_log(log_ctx, AV_LOG_ERROR, "Unsupported channel layout in stream group #%d\n", i);
                    av_log(log_ctx, AV_LOG_ERROR, "Expected one of: %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s\n",
                           av_get_channel_layout_name(&ff_iamf_scalable_ch_layouts[0]),
                           av_get_channel_layout_name(&ff_iamf_scalable_ch_layouts[1]),
                           av_get_channel_layout_name(&ff_iamf_scalable_ch_layouts[2]),
                           av_get_channel_layout_name(&ff_iamf_scalable_ch_layouts[3]),
                           av_get_channel_layout_name(&ff_iamf_scalable_ch_layouts[4]),
                           av_get_channel_layout_name(&ff_iamf_scalable_ch_layouts[5]),
                           av_get_channel_layout_name(&ff_iamf_scalable_ch_layouts[6]),
                           av_get_channel_layout_name(&ff_iamf_scalable_ch_layouts[7]),
                           av_get_channel_layout_name(&ff_iamf_scalable_ch_layouts[8]),
                           av_get_channel_layout_name(&ff_iamf_scalable_ch_layouts[9]),
                           av_get_channel_layout_name(&ff_iamf_scalable_ch_layouts[10]),
                           av_get_channel_layout_name(&ff_iamf_scalable_ch_layouts[11]),
                           av_get_channel_layout_name(&ff_iamf_scalable_ch_layouts[12]),
                           av_get_channel_layout_name(&ff_iamf_scalable_ch_layouts[13]),
                           av_get_channel_layout_name(&ff_iamf_scalable_ch_layouts[14]),
                           av_get_channel_layout_name(&ff_iamf_scalable_ch_layouts[15]));
                    return AVERROR(EINVAL);
                }
            }
        }

    for (int i = 0; i < iamf->nb_audio_elements; i++) {
        if (stg->id == iamf->audio_elements[i]->audio_element_id) {
            av_log(log_ctx, AV_LOG_ERROR, "Duplicated Audio Element id %"PRId64"\n", stg->id);
            return AVERROR(EINVAL);
        }
    }

    codec_config = av_mallocz(sizeof(*codec_config));
    if (!codec_config)
        return AVERROR(ENOMEM);

    ret = fill_codec_config(iamf, stg, codec_config);
    if (ret < 0) {
        av_free(codec_config);
        return ret;
    }

    audio_element = av_mallocz(sizeof(*audio_element));
    if (!audio_element)
        return AVERROR(ENOMEM);

    audio_element->celement = stg->params.iamf_audio_element;
    audio_element->audio_element_id = stg->id;
    audio_element->codec_config_id = ret;

    audio_element->substreams = av_calloc(stg->nb_streams, sizeof(*audio_element->substreams));
    if (!audio_element->substreams) {
        ret = AVERROR(ENOMEM);
        goto fail;
    }
    audio_element->nb_substreams = stg->nb_streams;

    audio_element->layers = av_calloc(iamf_audio_element->nb_layers, sizeof(*audio_element->layers));
    if (!audio_element->layers) {
        ret = AVERROR(ENOMEM);
        goto fail;
    }

    for (int i = 0, j = 0; i < iamf_audio_element->nb_layers; i++) {
        int nb_channels = iamf_audio_element->layers[i]->ch_layout.nb_channels;

        IAMFLayer *layer = &audio_element->layers[i];

        if (i)
            nb_channels -= iamf_audio_element->layers[i - 1]->ch_layout.nb_channels;
        for (; nb_channels > 0 && j < stg->nb_streams; j++) {
            const AVStream *st = stg->streams[j];
            IAMFSubStream *substream = &audio_element->substreams[j];

            substream->audio_substream_id = st->id;
            layer->substream_count++;
            layer->coupled_substream_count += st->codecpar->ch_layout.nb_channels == 2;
            nb_channels -= st->codecpar->ch_layout.nb_channels;
        }
        if (nb_channels) {
            av_log(log_ctx, AV_LOG_ERROR, "Invalid channel count across substreams in layer %u from stream group %u\n",
                   i, stg->index);
            ret = AVERROR(EINVAL);
            goto fail;
        }
    }

    for (int i = 0; i < audio_element->nb_substreams; i++) {
        for (int j = i + 1; j < audio_element->nb_substreams; j++)
            if (audio_element->substreams[i].audio_substream_id ==
                audio_element->substreams[j].audio_substream_id) {
                av_log(log_ctx, AV_LOG_ERROR, "Duplicate id %u in streams %u and %u from stream group %u\n",
                       audio_element->substreams[i].audio_substream_id, i, j, stg->index);
                ret = AVERROR(EINVAL);
                goto fail;
            }
    }

    if (iamf_audio_element->demixing_info) {
        AVIAMFParamDefinition *param = iamf_audio_element->demixing_info;
        const IAMFParamDefinition *param_definition = ff_iamf_get_param_definition(iamf, param->parameter_id);

        if (param->nb_subblocks != 1) {
            av_log(log_ctx, AV_LOG_ERROR, "nb_subblocks in demixing_info for stream group %u is not 1\n", stg->index);
            ret = AVERROR(EINVAL);
            goto fail;
        }

        if (!param_definition) {
            ret = add_param_definition(iamf, param, audio_element, log_ctx);
            if (ret < 0)
                goto fail;
        }
    }
    if (iamf_audio_element->recon_gain_info) {
        AVIAMFParamDefinition *param = iamf_audio_element->recon_gain_info;
        const IAMFParamDefinition *param_definition = ff_iamf_get_param_definition(iamf, param->parameter_id);

        if (param->nb_subblocks != 1) {
            av_log(log_ctx, AV_LOG_ERROR, "nb_subblocks in recon_gain_info for stream group %u is not 1\n", stg->index);
            ret = AVERROR(EINVAL);
            goto fail;
        }

        if (!param_definition) {
            ret = add_param_definition(iamf, param, audio_element, log_ctx);
            if (ret < 0)
                goto fail;
        }
    }

    tmp = av_realloc_array(iamf->audio_elements, iamf->nb_audio_elements + 1, sizeof(*iamf->audio_elements));
    if (!tmp) {
        ret = AVERROR(ENOMEM);
        goto fail;
    }

    iamf->audio_elements = tmp;
    iamf->audio_elements[iamf->nb_audio_elements++] = audio_element;

    return 0;
fail:
    ff_iamf_free_audio_element(&audio_element);
    return ret;
}
```
