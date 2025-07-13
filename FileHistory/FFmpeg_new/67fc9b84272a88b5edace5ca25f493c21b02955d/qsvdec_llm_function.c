
        Modify the function in the provided C file such that it fixes the following issue: lavc/qsvdec: Use FFmpeg default 1/25 framerate if can't derive it from bitstream in qsv_decode_header.
        Please return only the updated definition of the function 'qsv_decode_header' as a single C function. Do not include the full source file or any extra commentary.
        
```c
static int qsv_decode_header(AVCodecContext *avctx, QSVContext *q,
                             const AVPacket *avpkt, enum AVPixelFormat pix_fmt,
                             mfxVideoParam *param)
{
    int ret;
    mfxExtVideoSignalInfo video_signal_info = { 0 };
    mfxExtBuffer *header_ext_params[1] = { (mfxExtBuffer *)&video_signal_info };
    mfxBitstream bs = { 0 };

    if (avpkt->size) {
        bs.Data       = avpkt->data;
        bs.DataLength = avpkt->size;
        bs.MaxLength  = bs.DataLength;
        bs.TimeStamp  = PTS_TO_MFX_PTS(avpkt->pts, avctx->pkt_timebase);
        if (avctx->field_order == AV_FIELD_PROGRESSIVE)
            bs.DataFlag   |= MFX_BITSTREAM_COMPLETE_FRAME;
    } else
        return AVERROR_INVALIDDATA;

    if(!q->session) {
        ret = qsv_decode_preinit(avctx, q, pix_fmt, param);
        if (ret < 0)
            return ret;
    }

    ret = ff_qsv_codec_id_to_mfx(avctx->codec_id);
    if (ret < 0)
        return ret;

    param->mfx.CodecId = ret;
    video_signal_info.Header.BufferId = MFX_EXTBUFF_VIDEO_SIGNAL_INFO;
    video_signal_info.Header.BufferSz = sizeof(video_signal_info);
    // The SDK doesn't support other ext buffers when calling MFXVideoDECODE_DecodeHeader,
    // so do not append this buffer to the existent buffer array
    param->ExtParam    = header_ext_params;
    param->NumExtParam = 1;
    ret = MFXVideoDECODE_DecodeHeader(q->session, &bs, param);
    if (MFX_ERR_MORE_DATA == ret) {
       return AVERROR(EAGAIN);
    }
    if (ret < 0)
        return ff_qsv_print_error(avctx, ret,
                "Error decoding stream header");

    avctx->color_range = video_signal_info.VideoFullRange ? AVCOL_RANGE_JPEG : AVCOL_RANGE_MPEG;

    if (video_signal_info.ColourDescriptionPresent) {
        avctx->color_primaries = video_signal_info.ColourPrimaries;
        avctx->color_trc = video_signal_info.TransferCharacteristics;
        avctx->colorspace = video_signal_info.MatrixCoefficients;
    }

    param->ExtParam    = q->ext_buffers;
    param->NumExtParam = q->nb_ext_buffers;

#if QSV_VERSION_ATLEAST(1, 34)
    if (QSV_RUNTIME_VERSION_ATLEAST(q->ver, 1, 34) && avctx->codec_id == AV_CODEC_ID_AV1)
        param->mfx.FilmGrain = (avctx->export_side_data & AV_CODEC_EXPORT_DATA_FILM_GRAIN) ? 0 : param->mfx.FilmGrain;
#endif

    // Use FFmpeg default framerate 1/25 if it cannot be derived from stream
    if (avctx->framerate.num == 0 && avctx->framerate.den == 0) {
        avctx->framerate.num = 1;
        avctx->framerate.den = 25;
    }

    return 0;
}
```
