Do not provide any explanation or any other text.

```c
[...]
        for (int i = 0; i < frame->nb_side_data; i++) {
            AVFrameSideData *sd = frame->side_data[i];
            if (sd->type == AV_FRAME_DATA_DISPLAYMATRIX) {
                // Only promote display matrix side data to global if it's meaningful
                // (i.e., not identity matrix)
                int32_t *data = (int32_t *)sd->data;
                if (data[0] != 1 || data[1] != 0 || data[2] != 0 ||
                    data[3] != 0 || data[4] != 1 || data[5] != 0 ||
                    data[6] != 0 || data[7] != 0 || data[8] != 1) {
                    ret = av_frame_side_data_clone(
                        &enc_ctx->decoded_side_data, &enc_ctx->nb_decoded_side_data,
                        sd, AV_FRAME_SIDE_DATA_FLAG_UNIQUE);
                    if (ret < 0) {
                        av_frame_side_data_free(
                            &enc_ctx->decoded_side_data,
                            &enc_ctx->nb_decoded_side_data);
                        av_log(NULL, AV_LOG_ERROR,
                                "failed to configure video encoder: %s!\n",
                                av_err2str(ret));
                        return ret;
                    }
                }
            } else {
                ret = av_frame_side_data_clone(
                    &enc_ctx->decoded_side_data, &enc_ctx->nb_decoded_side_data,
                    sd, AV_FRAME_SIDE_DATA_FLAG_UNIQUE);
                if (ret < 0) {
                    av_frame_side_data_free(
                        &enc_ctx->decoded_side_data,
                        &enc_ctx->nb_decoded_side_data);
                    av_log(NULL, AV_LOG_ERROR,
                            "failed to configure video encoder: %s!\n",
                            av_err2str(ret));
                    return ret;
                }
            }
        }
[...]
```
