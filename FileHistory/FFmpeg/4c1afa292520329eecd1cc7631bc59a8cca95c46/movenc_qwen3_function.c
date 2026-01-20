Do not include anything else.

static int mov_write_video_tag(AVFormatContext *s, AVIOContext *pb, MOVMuxContext *mov, MOVTrack *track)
{
    int ret = AVERROR_BUG;
    int64_t pos = avio_tell(pb);
    char compressor_name[32] = { 0 };
    int avid = 0;

    int uncompressed_ycbcr = ((track->par->codec_id == AV_CODEC_ID_RAWVIDEO && track->par->format == AV_PIX_FMT_UYVY422)
                           || (track->par->codec_id == AV_CODEC_ID_RAWVIDEO && track->par->format == AV_PIX_FMT_YUYV422)
                           ||  track->par->codec_id == AV_CODEC_ID_V308
                           ||  track->par->codec_id == AV_CODEC_ID_V408
                           ||  track->par->codec_id == AV_CODEC_ID_V410
                           ||  track->par->codec_id == AV_CODEC_ID_V210);

    avio_wb32(pb, 0); /* size */
    if (mov->encryption_scheme != MOV_ENC_NONE) {
        ffio_wfourcc(pb, "encv");
    } else {
        avio_wl32(pb, track->tag); // store it byteswapped
    }
    avio_wb32(pb, 0); /* Reserved */
    avio_wb16(pb, 0); /* Reserved */
    avio_wb16(pb, 1); /* Data-reference index */

    if (uncompressed_ycbcr) {
        avio_wb16(pb, 2); /* Codec stream version */
    } else {
        avio_wb16(pb, 0); /* Codec stream version */
    }
    avio_wb16(pb, 0); /* Codec stream revision (=0) */
    if (track->mode == MODE_MOV) {
        ffio_wfourcc(pb, "FFMP"); /* Vendor */
        if (track->par->codec_id == AV_CODEC_ID_RAWVIDEO || uncompressed_ycbcr) {
            avio_wb32(pb, 0); /* Temporal Quality */
            avio_wb32(pb, 0x400); /* Spatial Quality = lossless*/
        } else {
            avio_wb32(pb, 0x200); /* Temporal Quality = normal */
            avio_wb32(pb, 0x200); /* Spatial Quality = normal */
        }
    } else {
        avio_wb32(pb, 0); /* Reserved */
        avio_wb32(pb, 0); /* Reserved */
        avio_wb32(pb, 0); /* Reserved */
    }
    avio_wb16(pb, track->par->width); /* Video width */
    avio_wb16(pb, track->height); /* Video height */
    avio_wb32(pb, 0x00480000); /* Horizontal resolution 72dpi */
    avio_wb32(pb, 0x00480000); /* Vertical resolution 72dpi */
    avio_wb32(pb, 0); /* Data size (= 0) */
    avio_wb16(pb, 1); /* Frame count (= 1) */

    /* FIXME not sure, ISO 14496-1 draft where it shall be set to 0 */
    find_compressor(compressor_name, 32, track);
    avio_w8(pb, strlen(compressor_name));
    avio_write(pb, compressor_name, 31);

    if (track->mode == MODE_MOV &&
       (track->par->codec_id == AV_CODEC_ID_V410 || track->par->codec_id == AV_CODEC_ID_V210))
        avio_wb16(pb, 0x18);
    else if (track->mode == MODE_MOV && track->par->bits_per_coded_sample)
        avio_wb16(pb, track->par->bits_per_coded_sample |
                  (track->par->format == AV_PIX_FMT_GRAY8 ? 0x20 : 0));
    else
        avio_wb16(pb, 0x18); /* Reserved */

    if (track->mode == MODE_MOV && track->par->format == AV_PIX_FMT_PAL8) {
        int pal_size = 1 << track->par->bits_per_coded_sample;
        int i;
        avio_wb16(pb, 0);             /* Color table ID */
        avio_wb32(pb, 0);             /* Color table seed */
        avio_wb16(pb, 0x8000);        /* Color table flags */
        avio_wb16(pb, pal_size - 1);  /* Color table size (zero-relative) */
        for (i = 0; i < pal_size; i++) {
            uint32_t rgb = track->palette[i];
            uint16_t r = (rgb >> 16) & 0xff;
            uint16_t g = (rgb >> 8)  & 0xff;
            uint16_t b = rgb         & 0xff;
            avio_wb16(pb, 0);
            avio_wb16(pb, (r << 8) | r);
            avio_wb16(pb, (g << 8) | g);
            avio_wb16(pb, (b << 8) | b);
        }
    } else
        avio_wb16(pb, 0xffff); /* Reserved */

    if (track->tag == MKTAG('m','p','4','v'))
        mov_write_esds_tag(pb, track);
    else if (track->par->codec_id == AV_CODEC_ID_H263)
        mov_write_d263_tag(pb);
    else if (track->par->codec_id == AV_CODEC_ID_AVUI ||
            track->par->codec_id == AV_CODEC_ID_SVQ3) {
        mov_write_extradata_tag(pb, track);
        avio_wb32(pb, 0);
    } else if (track->par->codec_id == AV_CODEC_ID_DNXHD) {
        mov_write_avid_tag(pb, track);
        avid = 1;
    } else if (track->par->codec_id == AV_CODEC_ID_HEVC)
        mov_write_hvcc_tag(pb, track);
    else if (track->par->codec_id == AV_CODEC_ID_H264 && !TAG_IS_AVCI(track->tag)) {
        mov_write_avcc_tag(pb, track);
        if (track->mode == MODE_IPOD)
            mov_write_uuid_tag_ipod(pb);
    } else if (track->par->codec_id == AV_CODEC_ID_VP9) {
        mov_write_vpcc_tag(mov->fc, pb, track);
    } else if (track->par->codec_id == AV_CODEC_ID_AV1) {
        mov_write_av1c_tag(pb, track);
    } else if (track->par->codec_id == AV_CODEC_ID_VC1 && track->vos_len > 0)
        mov_write_dvc1_tag(pb, track);
    else if (track->par->codec_id == AV_CODEC_ID_VP6F ||
             track->par->codec_id == AV_CODEC_ID_VP6A) {
        /* Don't write any potential extradata here - the cropping
         * is signalled via the normal width/height fields. */
    } else if (track->par->codec_id == AV_CODEC_ID_R10K) {
        if (track->par->codec_tag == MKTAG('R','1','0','k'))
            mov_write_dpxe_tag(pb, track);
    } else if (track->vos_len > 0)
        mov_write_glbl_tag(pb, track);

    if (track->par->codec_id != AV_CODEC_ID_H264 &&
        track->par->codec_id != AV_CODEC_ID_MPEG4 &&
        track->par->codec_id != AV_CODEC_ID_DNXHD) {
        int field_order = track->par->field_order;

        if (field_order != AV_FIELD_UNKNOWN)
            mov_write_fiel_tag(pb, track, field_order);
    }

    if (mov->flags & FF_MOV_FLAG_WRITE_GAMA) {
        if (track->mode == MODE_MOV)
            mov_write_gama_tag(s, pb, track, mov->gamma);
        else
            av_log(mov->fc, AV_LOG_WARNING, "Not writing 'gama' atom. Format is not MOV.\n");
    }
    if (track->mode == MODE_MOV || track->mode == MODE_MP4) {
        int has_color_info = track->par->color_primaries != AVCOL_PRI_UNSPECIFIED &&
                             track->par->color_trc != AVCOL_TRC_UNSPECIFIED &&
                             track->par->color_space != AVCOL_SPC_UNSPECIFIED;
        if (has_color_info || mov->flags & FF_MOV_FLAG_WRITE_COLR ||
            av_stream_get_side_data(track->st, AV_PKT_DATA_ICC_PROFILE, NULL)) {
            int prefer_icc = mov->flags & FF_MOV_FLAG_PREFER_ICC || !has_color_info;
            mov_write_colr_tag(pb, track, prefer_icc);
        } else if (mov->flags & FF_MOV_FLAG_WRITE_COLR) {
             av_log(mov->fc, AV_LOG_WARNING, "Not writing 'colr' atom. Format is not MOV or MP4.\n");
        }
    }
    if (track->mode == MODE_MOV || track->mode == MODE_MP4) {
        mov_write_clli_tag(pb, track);
        mov_write_mdcv_tag(pb, track);
    }

    if (track->mode == MODE_MP4 && mov->fc->strict_std_compliance <= FF_COMPLIANCE_UNOFFICIAL) {
        AVStereo3D* stereo_3d = (AVStereo3D*) av_stream_get_side_data(track->st, AV_PKT_DATA_STEREO3D, NULL);
        AVSphericalMapping* spherical_mapping = (AVSphericalMapping*)av_stream_get_side_data(track->st, AV_PKT_DATA_SPHERICAL, NULL);
        AVDOVIDecoderConfigurationRecord *dovi = (AVDOVIDecoderConfigurationRecord *)
                                                 av_stream_get_side_data(track->st, AV_PKT_DATA_DOVI_CONF, NULL);

        if (stereo_3d)
            mov_write_st3d_tag(s, pb, stereo_3d);
        if (spherical_mapping)
            mov_write_sv3d_tag(mov->fc, pb, spherical_mapping);
        if (dovi)
            mov_write_dvcc_dvvc_tag(s, pb, dovi);
    }

    if (track->par->sample_aspect_ratio.den && track->par->sample_aspect_ratio.num) {
        mov_write_pasp_tag(pb, track);
    }

    if (uncompressed_ycbcr){
        mov_write_clap_tag(pb, track);
    }

    if (mov->encryption_scheme != MOV_ENC_NONE) {
        ff_mov_cenc_write_sinf_tag(track, pb, mov->encryption_kid);
    }

    if (track->mode == MODE_MP4 &&
            ((ret = mov_write_btrt_tag(pb, track)) < 0))
        return ret;

    /* extra padding for avid stsd */
    /* https://developer.apple.com/library/mac/documentation/QuickTime/QTFF/QTFFChap2/qtff2.html#//apple_ref/doc/uid/TP40000939-CH204-61112 */
    if (avid)
        avio_wb32(pb, 0);

    return update_size(pb, pos);
}
