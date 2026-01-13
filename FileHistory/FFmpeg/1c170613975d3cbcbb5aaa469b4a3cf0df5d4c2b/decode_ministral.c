Do not include any additional text or explanation.

```c
static int decode_get_packet(AVCodecContext *avctx, AVPacket *pkt)
{
    AVCodecInternal *avci = avctx->internal;
    int ret;

    ret = av_bsf_receive_packet(avci->bsf, pkt);
    if (ret < 0)
        return ret;

    if (!(ffcodec(avctx->codec)->caps_internal & FF_CODEC_CAP_SETS_FRAME_PROPS)) {
        ret = extract_packet_props(avctx->internal, pkt);
        if (ret < 0)
            goto finish;
    }

    ret = apply_param_change(avctx, pkt);
    if (ret < 0)
        goto finish;

    if (avci->bsf) {
        ret = avcodec_parameters_from_context(avci->bsf->par_out, avctx);
        if (ret < 0)
            goto finish;
    }

    return 0;
finish:
    av_packet_unref(pkt);
    return ret;
}
```
