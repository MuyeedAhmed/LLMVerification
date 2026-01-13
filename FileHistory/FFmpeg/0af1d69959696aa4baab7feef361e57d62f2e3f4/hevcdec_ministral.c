Do not include any additional text or explanations.

```c
static int hls_slice_header(SliceHeader *sh, const HEVCLayerContext *s, GetBitContext *gb)
{
    const HEVCPPS *pps;
    const HEVCSPS *sps;
    const HEVCVPS *vps;
    unsigned pps_id, layer_idx;
    int i, ret;

    // Coded parameters
    sh->first_slice_in_pic_flag = get_bits1(gb);

    sh->no_output_of_prior_pics_flag = 0;
    if (IS_IRAP(s))
        sh->no_output_of_prior_pics_flag = get_bits1(gb);

    pps_id = get_ue_golomb_long(gb);
    if (pps_id >= HEVC_MAX_PPS_COUNT || !s->ps.pps_list[pps_id]) {
        av_log(s->avctx, AV_LOG_ERROR, "PPS id out of range: %d\n", pps_id);
        return AVERROR_INVALIDDATA;
    }
    if (!sh->first_slice_in_pic_flag && s->ps.pps_list[pps_id] != s->pps) {
        av_log(s->avctx, AV_LOG_ERROR, "PPS changed between slices.\n");
        return AVERROR_INVALIDDATA;
    }
    sh->pps_id = pps_id;

    pps = s->ps.pps_list[pps_id];
    sps = pps->sps;
    vps = sps->vps;
    layer_idx = vps->layer_idx[s->nuh_layer_id];

    if (s->nal_unit_type == HEVC_NAL_CRA_NUT && s->last_eos == 1)
        sh->no_output_of_prior_pics_flag = 1;

    sh->dependent_slice_segment_flag = 0;
    if (!sh->first_slice_in_pic_flag) {
        int slice_address_length;

        if (pps->dependent_slice_segments_enabled_flag)
            sh->dependent_slice_segment_flag = get_bits1(gb);
        if (sh->dependent_slice_segment_flag && !s->slice_initialized) {
            av_log(s->avctx, AV_LOG_ERROR, "Independent slice segment missing.\n");
            return AVERROR_INVALIDDATA;
        }

        slice_address_length = av_ceil_log2(sps->ctb_width *
                                            sps->ctb_height);
        sh->slice_segment_addr = get_bitsz(gb, slice_address_length);
        if (sh->slice_segment_addr >= sps->ctb_width * sps->ctb_height) {
            av_log(s->avctx, AV_LOG_ERROR,
                   "Invalid slice segment address: %u.\n",
                   sh->slice_segment_addr);
            return AVERROR_INVALIDDATA;
        }

        if (!sh->dependent_slice_segment_flag) {
            sh->slice_addr = sh->slice_segment_addr;
        }
    } else {
        sh->slice_segment_addr = sh->slice_addr = 0;
    }

    if (get_bits_left(gb) < 0) {
        av_log(s->avctx, AV_LOG_ERROR,
               "Overread slice header by %d bits\n", -get_bits_left(gb));
        return AVERROR_INVALIDDATA;
    }

    if (!sh->dependent_slice_segment_flag) {
        for (i = 0; i < pps->num_extra_slice_header_bits; i++)
            skip_bits(gb, 1);  // slice_reserved_undetermined_flag[]

        sh->slice_type = get_ue_golomb_long(gb);
        if (!(sh->slice_type == HEVC_SLICE_I ||
              sh->slice_type == HEVC_SLICE_P ||
              sh->slice_type == HEVC_SLICE_B)) {
            av_log(s->avctx, AV_LOG_ERROR, "Unknown slice type: %d.\n",
                   sh->slice_type);
            return AVERROR_INVALIDDATA;
        }
        if (IS_IRAP(s) && sh->slice_type != HEVC_SLICE_I &&
            !pps->pps_curr_pic_ref_enabled_flag &&
            s->nuh_layer_id == 0) {
            av_log(s->avctx, AV_LOG_ERROR, "Inter slices in an IRAP frame.\n");
            return AVERROR_INVALIDDATA;
        }

        // when flag is not present, picture is inferred to be output
        sh->pic_output_flag = 1;
        if (pps->output_flag_present_flag)
            sh->pic_output_flag = get_bits1(gb);

        if (sps->separate_colour_plane)
            sh->colour_plane_id = get_bits(gb, 2);

        if (!IS_IDR(s) ||
            (s->nuh_layer_id > 0 &&
             !(vps->poc_lsb_not_present & (1 << layer_idx)))) {
            int poc;

            sh->pic_order_cnt_lsb = get_bits(gb, sps->log2_max_poc_lsb);
            poc = ff_hevc_compute_poc(sps, s->poc_tid0, sh->pic_order_cnt_lsb, s->nal_unit_type);
            if (!sh->first_slice_in_pic_flag && poc != sh->poc) {
                av_log(s->avctx, AV_LOG_WARNING,
                       "Ignoring POC change between slices: %d -> %d\n", poc, sh->poc);
                if (s->avctx->err_recognition & AV_EF_EXPLODE)
                    return AVERROR_INVALIDDATA;
                poc = sh->poc;
            }
            sh->poc = poc;
        }

        if (!IS_IDR(s)) {
            int pos;

            sh->short_term_ref_pic_set_sps_flag = get_bits1(gb);
            pos = get_bits_left(gb);
            if (!sh->short_term_ref_pic_set_sps_flag) {
                ret = ff_hevc_decode_short_term_rps(gb, s->avctx, &sh->slice_rps, sps, 1);
                if (ret < 0)
                    return ret;

                sh->short_term_rps = &sh->slice_rps;
            } else {
                int numbits, rps_idx;

                if (!sps->nb_st_rps) {
                    av_log(s->avctx, AV_LOG_ERROR, "No ref lists in the SPS.\n");
                    return AVERROR_INVALIDDATA;
                }

                numbits = av_ceil_log2(sps->nb_st_rps);
                rps_idx = numbits > 0 ? get_bits(gb, numbits) : 0;
                sh->short_term_rps = &sps->st_rps[rps_idx];
            }
            sh->short_term_ref_pic_set_size = pos - get_bits_left(gb);

            pos = get_bits_left(gb);
            ret = decode_lt_rps(sps, &sh->long_term_rps, gb, sh->poc, sh->pic_order_cnt_lsb);
            if (ret < 0) {
                av_log(s->avctx, AV_LOG_WARNING, "Invalid long term RPS.\n");
                if (s->avctx->err_recognition & AV_EF_EXPLODE)
                    return AVERROR_INVALIDDATA;
            }
            sh->long_term_ref_pic_set_size = pos - get_bits_left(gb);

            if (sps->temporal_mvp_enabled)
                sh->slice_temporal_mvp_enabled_flag = get_bits1(gb);
            else
                sh->slice_temporal_mvp_enabled_flag = 0;
        } else {
            sh->poc                             = 0;
            sh->pic_order_cnt_lsb               = 0;
            sh->short_term_ref_pic_set_sps_flag = 0;
            sh->short_term_ref_pic_set_size     = 0;
            sh->short_term_rps                  = NULL;
            sh->long_term_ref_pic_set_size      = 0;
            sh->slice_temporal_mvp_enabled_flag = 0;
        }

        sh->inter_layer_pred = 0;
        if (s->nuh_layer_id > 0) {
            int num_direct_ref_layers = vps->num_direct_ref_layers[layer_idx];

            if (vps->default_ref_layers_active)
                sh->inter_layer_pred = !!num_direct_ref_layers;
            else if (num_direct_ref_layers) {
                sh->inter_layer_pred = get_bits1(gb);

                if (sh->inter_layer_pred && num_direct_ref_layers > 1) {
                    av_log(s->avctx, AV_LOG_ERROR,
                           "NumDirectRefLayers>1 not supported\n");
                    return AVERROR_PATCHWELCOME;
                }
            }
        }

        if (sps->sao_enabled) {
            sh->slice_sample_adaptive_offset_flag[0] = get_bits1(gb);
            if (sps->chroma_format_idc) {
                sh->slice_sample_adaptive_offset_flag[1] =
                sh->slice_sample_adaptive_offset_flag[2] = get_bits1(gb);
            }
        } else {
            sh->slice_sample_adaptive_offset_flag[0] = 0;
            sh->slice_sample_adaptive_offset_flag[1] = 0;
            sh->slice_sample_adaptive_offset_flag[2] = 0;
        }

        sh->nb_refs[L0] = sh->nb_refs[L1] = 0;
        if (sh->slice_type == HEVC_SLICE_P || sh->slice_type == HEVC_SLICE_B) {
            int nb_refs;

            sh->nb_refs[L0] = pps->num_ref_idx_l0_default_active;
            if (sh->slice_type == HEVC_SLICE_B)
                sh->nb_refs[L1] = pps->num_ref_idx_l1_default_active;

            if (get_bits1(gb)) { // num_ref_idx_active_override_flag
                sh->nb_refs[L0] = get_ue_golomb_31(gb) + 1;
                if (sh->slice_type == HEVC_SLICE_B)
                    sh->nb_refs[L1] = get_ue_golomb_31(gb) + 1;
            }
            if (sh->nb_refs[L0] >= HEVC_MAX_REFS || sh->nb_refs[L1] >= HEVC_MAX_REFS) {
                av_log(s->avctx, AV_LOG_ERROR, "Too many refs: %d/%d.\n",
                       sh->nb_refs[L0], sh->nb_refs[L1]);
                return AVERROR_INVALIDDATA;
            }

            sh->rpl_modification_flag[0] = 0;
            sh->rpl_modification_flag[1] = 0;
            nb_refs = ff_hevc_frame_nb_refs(sh, pps, layer_idx);
            if (!nb_refs) {
                av_log(s->avctx, AV_LOG_ERROR, "Zero refs for a frame with P or B slices.\n");
                return AVERROR_INVALIDDATA;
            }

            if (pps->lists_modification_present_flag && nb_refs > 1) {
                sh->rpl_modification_flag[0] = get_bits1(gb);
                if (sh->rpl_modification_flag[0]) {
                    for (i = 0; i < sh->nb_refs[L0]; i++)
                        sh->list_entry_lx[0][i] = get_bits(gb, av_ceil_log2(nb_refs));
                }

                if (sh->slice_type == HEVC_SLICE_B) {
                    sh->rpl_modification_flag[1] = get_bits1(gb);
                    if (sh->rpl_modification_flag[1] == 1)
                        for (i = 0; i < sh->nb_refs[L1]; i++)
                            sh->list_entry_lx[1][i] = get_bits(gb, av_ceil_log2(nb_refs));
                }
            }

            if (sh->slice_type == HEVC_SLICE_B)
                sh->mvd_l1_zero_flag = get_bits1(gb);

            if (pps->cabac_init_present_flag)
                sh->cabac_init_flag = get_bits1(gb);
            else
                sh->cabac_init_flag = 0;

            sh->collocated_ref_idx = 0;
            if (sh->slice_temporal_mvp_enabled_flag) {
                sh->collocated_list = L0;
                if (sh->slice_type == HEVC_SLICE_B)
                    sh->collocated_list = !get_bits1(gb);

                if (sh->nb_refs[sh->collocated_list] > 1) {
                    sh->collocated_ref_idx = get_ue_golomb_long(gb);
                    if (sh->collocated_ref_idx >= sh->nb_refs[sh->collocated_list]) {
                        av_log(s->avctx, AV_LOG_ERROR,
                               "Invalid collocated_ref_idx: %d.\n",
                               sh->collocated_ref_idx);
                        return AVERROR_INVALIDDATA;
                    }
                }
            }

            if ((pps->weighted_pred_flag   && sh->slice_type == HEVC_SLICE_P) ||
                (pps->weighted_bipred_flag && sh->slice_type == HEVC_SLICE_B)) {
                int ret = pred_weight_table(sh, s->avctx, sps, gb);
                if (ret < 0)
                    return ret;
            }

            sh->max_num_merge_cand = 5 - get_ue_golomb_long(gb);
            if (sh->max_num_merge_cand < 1 || sh->max_num_merge_cand > 5) {
                av_log(s->avctx, AV_LOG_ERROR,
                       "Invalid number of merging MVP candidates: %d.\n",
                       sh->max_num_merge_cand);
                return AVERROR_INVALIDDATA;
            }

            // Syntax in 7.3.6.1
            if (sps->motion_vector_resolution_control_idc == 2)
                sh->use_integer_mv_flag = get_bits1(gb);
            else
                // Inferred to be equal to motion_vector_resolution_control_idc if not present
                sh->use_integer_mv_flag = sps->motion_vector_resolution_control_idc;

        }

        sh->slice_qp_delta = get_se_golomb(gb);

        if (pps->pic_slice_level_chroma_qp_offsets_present_flag) {
            sh->slice_cb_qp_offset = get_se_golomb(gb);
            sh->slice_cr_qp_offset = get_se_golomb(gb);
            if (sh->slice_cb_qp_offset < -12 || sh->slice_cb_qp_offset > 12 ||
                sh->slice_cr_qp_offset < -12 || sh->slice_cr_qp_offset > 12) {
                av_log(s->avctx, AV_LOG_ERROR, "Invalid slice cx qp offset.\n");
                return AVERROR_INVALIDDATA;
            }
        } else {
            sh->slice_cb_qp_offset = 0;
            sh->slice_cr_qp_offset = 0;
        }

        if (pps->pps_slice_act_qp_offsets_present_flag) {
            sh->slice_act_y_qp_offset  = get_se_golomb(gb);
            sh->slice_act_cb_qp_offset = get_se_golomb(gb);
            sh->slice_act_cr_qp_offset = get_se_golomb(gb);
        }

        if (pps->chroma_qp_offset_list_enabled_flag)
            sh->cu_chroma_qp_offset_enabled_flag = get_bits1(gb);
        else
            sh->cu_chroma_qp_offset_enabled_flag = 0;

        if (pps->deblocking_filter_control_present_flag) {
            int deblocking_filter_override_flag = 0;

            if (pps->deblocking_filter_override_enabled_flag)
                deblocking_filter_override_flag = get_bits1(gb);

            if (deblocking_filter_override_flag) {
                sh->disable_deblocking_filter_flag = get_bits1(gb);
                if (!sh->disable_deblocking_filter_flag) {
                    int beta_offset_div2 = get_se_golomb(gb);
                    int tc_offset_div2   = get_se_golomb(gb) ;
                    if (beta_offset_div2 < -6 || beta_offset_div2 > 6 ||
                        tc_offset_div2   < -6 || tc_offset_div2   > 6) {
                        av_log(s->avctx, AV_LOG_ERROR,
                            "Invalid deblock filter offsets: %d, %d\n",
                            beta_offset_div2, tc_offset_div2);
                        return AVERROR_INVALIDDATA;
                    }
                    sh->beta_offset = beta_offset_div2 * 2;
                    sh->tc_offset   =   tc_offset_div2 * 2;
                }
            } else {
                sh->disable_deblocking_filter_flag = pps->disable_dbf;
                sh->beta_offset                    = pps->beta_offset;
                sh->tc_offset                      = pps->tc_offset;
            }
        } else {
            sh->disable_deblocking_filter_flag = 0;
            sh->beta_offset                    = 0;
            sh->tc_offset                      = 0;
        }

        if (pps->seq_loop_filter_across_slices_enabled_flag &&
            (sh->slice_sample_adaptive_offset_flag[0] ||
             sh->slice_sample_adaptive_offset_flag[1] ||
             !sh->disable_deblocking_filter_flag)) {
            sh->slice_loop_filter_across_slices_enabled_flag = get_bits1(gb);
        } else {
            sh->slice_loop_filter_across_slices_enabled_flag = pps->seq_loop_filter_across_slices_enabled_flag;
        }
    }

    sh->num_entry_point_offsets = 0;
    if (pps->tiles_enabled_flag || pps->entropy_coding_sync_enabled_flag) {
        unsigned num_entry_point_offsets = get_ue_golomb_long(gb);
        // It would be possible to bound this tighter but this here is simpler
        if (num_entry_point_offsets > get_bits_left(gb)) {
            av_log(s->avctx, AV_LOG_ERROR, "num_entry_point_offsets %d is invalid\n", num_entry_point_offsets);
            return AVERROR_INVALIDDATA;
        }

        sh->num_entry_point_offsets = num_entry_point_offsets;
        if (sh->num_entry_point_offsets > 0) {
            int offset_len = get_ue_golomb_long(gb) + 1;

            if (offset_len < 1 || offset_len > 32) {
                sh->num_entry_point_offsets = 0;
                av_log(s->avctx, AV_LOG_ERROR, "offset_len %d is invalid\n", offset_len);
                return AVERROR_INVALIDDATA;
            }

            av_freep(&sh->entry_point_offset);
            av_freep(&sh->offset);
            av_freep(&sh->size);
            sh->entry_point_offset = av_malloc_array(sh->num_entry_point_offsets, sizeof(unsigned));
            sh->offset             = av_malloc_array(sh->num_entry_point_offsets + 1, sizeof(int));
            sh->size               = av_malloc_array(sh->num_entry_point_offsets + 1, sizeof(int));
            if (!sh->entry_point_offset || !sh->offset || !sh->size) {
                sh->num_entry_point_offsets = 0;
                av_log(s->avctx, AV_LOG_ERROR, "Failed to allocate memory\n");
                return AVERROR(ENOMEM);
            }
            for (i = 0; i < sh->num_entry_point_offsets; i++) {
                unsigned val = get_bits_long(gb, offset_len);
                sh->entry_point_offset[i] = val + 1; // +1; // +1 to get the size
            }
        }
    }

    if (pps->slice_header_extension_present_flag) {
        unsigned int length = get_ue_golomb_long(gb);
        if (length*8LL > get_bits_left(gb)) {
            av_log(s->avctx, AV_LOG_ERROR, "too many slice_header_extension_data_bytes\n");
            return AVERROR_INVALIDDATA;
        }
        for (i = 0; i < length; i++)
            skip_bits(gb, 8);  // slice_header_extension_data_byte
    }

    ret = get_bits1(gb);
    if (!ret && get_bits_left(gb) >= 0) {
        av_log(s->avctx, AV_LOG_ERROR, "alignment_bit_equal_to_one=0\n");
        return AVERROR_INVALIDDATA;
    }
    sh->data_offset = align_get_bits(gb) - gb->buffer;

    // Inferred parameters
    sh->slice_qp = 26U + pps->pic_init_qp_minus26 + sh->slice_qp_delta;
    if (sh->slice_qp > 51 ||
        sh->slice_qp < -sps->qp_bd_offset) {
        av_log(s->avctx, AV_LOG_ERROR,
               "The slice_qp %d is outside the valid range "
               "[%d, 51].\n",
               sh->slice_qp,
               -sps->qp_bd_offset);
        return AVERROR_INVALIDDATA;
    }

    sh->slice_ctb_addr_rs = sh->slice_segment_addr;

    if (sh->dependent_slice_segment_flag &&
        (!sh->slice_ctb_addr_rs || !pps->ctb_addr_rs_to_ts[sh->slice_ctb_addr_rs])) {
        av_log(s->avctx, AV_LOG_ERROR, "Impossible slice segment.\n");
        return AVERROR_INVALIDDATA;
    }

    if (get_bits_left(gb) < 0) {
        av_log(s->avctx, AV_LOG_ERROR,
               "Overread slice header by %d bits\n", -get_bits_left(gb));
        return AVERROR_INVALIDDATA;
    }

    return 0;
}
```
