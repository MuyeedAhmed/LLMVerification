Do not include anything else.

static av_cold int dovi_configure_ext(DOVIContext *s, enum AVCodecID codec_id,
                                      const AVDOVIMetadata *metadata,
                                      enum AVDOVICompression compression,
                                      int strict_std_compliance,
                                      int width, int height,
                                      AVRational framerate,
                                      enum AVPixelFormat pix_format,
                                      enum AVColorSpace color_space,
                                      enum AVColorPrimaries color_primaries,
                                      enum AVColorTransferCharacteristic color_trc,
                                      AVPacketSideData **coded_side_data,
                                      int *nb_coded_side_data)
{
    AVDOVIDecoderConfigurationRecord *cfg;
    const AVDOVIRpuDataHeader *hdr = NULL;
    int dv_profile, dv_level, bl_compat_id = -1;
    size_t cfg_size;
    uint64_t pps;

    if (!s->enable)
        goto skip;

    if (metadata)
        hdr = av_dovi_get_header(metadata);

    if (s->enable == FF_DOVI_AUTOMATIC && !hdr)
        goto skip;

    if (compression == AV_DOVI_COMPRESSION_RESERVED ||
        compression > AV_DOVI_COMPRESSION_EXTENDED)
        return AVERROR(EINVAL);

    switch (codec_id) {
    case AV_CODEC_ID_AV1:  dv_profile = 10; break;
    case AV_CODEC_ID_H264: dv_profile = 9;  break;
    case AV_CODEC_ID_HEVC:
        if (hdr) {
            dv_profile = ff_dovi_guess_profile_hevc(hdr);
            break;
        }

        /* This is likely to be proprietary IPTPQc2 */
        if (color_space == AVCOL_SPC_IPT_C2 ||
            (color_space == AVCOL_SPC_UNSPECIFIED &&
             color_trc == AVCOL_TRC_UNSPECIFIED))
            dv_profile = 5;
        else
            dv_profile = 8;
        break;
    default:
        /* No other encoder should be calling this! */
        av_unreachable();
        return AVERROR_BUG;
    }

    if (strict_std_compliance > FF_COMPLIANCE_UNOFFICIAL) {
        if (dv_profile == 9) {
            if (pix_format != AV_PIX_FMT_YUV420P)
                dv_profile = 0;
        } else {
            if (pix_format != AV_PIX_FMT_YUV420P10)
                dv_profile = 0;
        }
    }

    switch (dv_profile) {
    case 4: /* HEVC with enhancement layer */
    case 7:
        if (s->enable > 0) {
            av_log(s->logctx, AV_LOG_ERROR, "Coding of Dolby Vision enhancement "
                   "layers is currently unsupported.");
            return AVERROR_PATCHWELCOME;
        } else {
            goto skip;
        }
    case 5: /* HEVC with proprietary IPTPQc2 */
        bl_compat_id = 0;
        break;
    case 10:
        /* FIXME: check for proper H.273 tags once those are added */
        if (hdr && hdr->bl_video_full_range_flag) {
            /* AV1 with proprietary IPTPQc2 */
            bl_compat_id = 0;
            break;
        }
        /* fall through */
    case 8: /* HEVC (or AV1) with BL compatibility */
        if (color_space == AVCOL_SPC_BT2020_NCL &&
            color_primaries == AVCOL_PRI_BT2020 &&
            color_trc == AVCOL_TRC_SMPTE2084) {
            bl_compat_id = 1;
        } else if (color_space == AVCOL_SPC_BT2020_NCL &&
                   color_primaries == AVCOL_PRI_BT2020 &&
                   color_trc == AVCOL_TRC_ARIB_STD_B67) {
            bl_compat_id = 4;
        } else if (color_space == AVCOL_SPC_BT709 &&
                   color_primaries == AVCOL_PRI_BT709 &&
                   color_trc == AVCOL_TRC_BT709) {
            bl_compat_id = 2;
        }
    }

    if (!dv_profile || bl_compat_id < 0) {
        if (s->enable > 0) {
            av_log(s->logctx, AV_LOG_ERROR, "Dolby Vision enabled, but could "
                   "not determine profile and compatibility mode. Double-check "
                   "colorspace and format settings for compatibility?\n");
            return AVERROR(EINVAL);
        }
        goto skip;
    }

    if (compression != AV_DOVI_COMPRESSION_NONE) {
        if (dv_profile < 8 && strict_std_compliance > FF_COMPLIANCE_UNOFFICIAL) {
            av_log(s->logctx, AV_LOG_ERROR, "Dolby Vision metadata compression "
                   "is not permitted for profiles 7 and earlier. (dv_profile: %d, "
                   "compression: %d)\n", dv_profile, compression);
            return AVERROR(EINVAL);
        } else if (compression == AV_DOVI_COMPRESSION_EXTENDED &&
                   strict_std_compliance > FF_COMPLIANCE_EXPERIMENTAL) {
            av_log(s->logctx, AV_LOG_ERROR, "Dolby Vision extended metadata "
                   "compression is experimental and not supported by "
                   "devices.");
            return AVERROR(EINVAL);
        } else if (dv_profile == 8) {
            av_log(s->logctx, AV_LOG_WARNING, "Dolby Vision metadata compression "
                   "for profile 8 is known to be unsupported by many devices, "
                   "use with caution.\n");
        }
    }

    pps = width * height;
    if (framerate.num) {
        pps = pps * framerate.num / framerate.den;
    } else {
        pps *= 25; /* sanity fallback */
    }

    dv_level = 0;
    for (int i = 1; i < FF_ARRAY_ELEMS(dv_levels); i++) {
        if (pps > dv_levels[i].pps)
            continue;
        if (width > dv_levels[i].width)
            continue;
        /* In theory, we should also test the bitrate when known, and
         * distinguish between main and high tier. In practice, just ignore
         * the bitrate constraints and hope they work out. This would ideally
         * be handled by either the encoder or muxer directly. */
        dv_level = i;
        break;
    }

    if (!dv_level) {
        if (strict_std_compliance >= FF_COMPLIANCE_STRICT) {
            av_log(s->logctx, AV_LOG_ERROR, "Coded PPS (%"PRIu64") and width (%d) "
                   "exceed Dolby Vision limitations\n", pps, width);
            return AVERROR(EINVAL);
        } else {
            av_log(s->logctx, AV_LOG_WARNING, "Coded PPS (%"PRIu64") and width (%d) "
                   "exceed Dolby Vision limitations. Ignoring, resulting file "
                   "may be non-conforming.\n", pps, width);
            dv_level = FF_ARRAY_ELEMS(dv_levels) - 1;
        }
    }

    cfg = av_dovi_alloc(&cfg_size);
    if (!cfg)
        return AVERROR(ENOMEM);

    if (!av_packet_side_data_add(coded_side_data,
                                 nb_coded_side_data,
                                 AV_PKT_DATA_DOVI_CONF, cfg, cfg_size, 0)) {
        av_free(cfg);
        return AVERROR(ENOMEM);
    }

    cfg->dv_version_major = 1;
    cfg->dv_version_minor = 0;
    cfg->dv_profile = dv_profile;
    cfg->dv_level = dv_level;
    cfg->rpu_present_flag = 1;
    cfg->el_present_flag = 0;
    cfg->bl_present_flag = 1;
    cfg->dv_bl_signal_compatibility_id = bl_compat_id;
    cfg->dv_md_compression = compression;

    s->cfg = *cfg;
    return 0;

skip:
    s->cfg = (AVDOVIDecoderConfigurationRecord) {0};
    return 0;
}
