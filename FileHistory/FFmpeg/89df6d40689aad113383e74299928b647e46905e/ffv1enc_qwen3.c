Do not include any other text.

static inline void put_vlc_symbol(PutBitContext *pb, VlcState *const state,
                                  int v, int bits)
{
    int i, k, code;
    v = fold(v - state->bias, bits);

    i = state->count;
    k = 0;
    while (i < state->error_sum) { // FIXME: optimize
        k++;
        i += i;
    }

    av_assert2(k <= 16);

    code = v ^ ((2 * state->drift + state->count) >> 31);

    ff_dlog(NULL, "v:%d/%d bias:%d error:%d drift:%d count:%d k:%d\n", v, code,
            state->bias, state->error_sum, state->drift, state->count, k);
    set_sr_golomb(pb, code, k, 12, bits);

    update_vlc_state(state, v);
}

#define TYPE int16_t
#define RENAME(name) name
#include "ffv1enc_template.c"
#undef TYPE
#undef RENAME

#define TYPE int32_t
#define RENAME(name) name ## 32
#include "ffv1enc_template.c"

static int encode_plane(FFV1Context *f, FFV1SliceContext *sc,
                        const uint8_t *src, int w, int h,
                        int stride, int plane_index, int remap_index, int pixel_stride, int ac)
{
    int x, y, i, ret;
    const int pass1 = !!(f->avctx->flags & AV_CODEC_FLAG_PASS1);
    const int ring_size = f->context_model ? 3 : 2;
    int16_t *sample[3];
    sc->run_index = 0;

    memset(sc->sample_buffer, 0, ring_size * (w + 6) * sizeof(*sc->sample_buffer));

    for (y = 0; y < h; y++) {
        for (i = 0; i < ring_size; i++)
            sample[i] = sc->sample_buffer + (w + 6) * ((h + i - y) % ring_size) + 3;

        sample[0][-1]= sample[1][0  ];
        sample[1][ w]= sample[1][w-1];

        if (f->bits_per_raw_sample <= 8) {
            for (x = 0; x < w; x++)
                sample[0][x] = src[x * pixel_stride + stride * y];
            if (sc->remap)
                for (x = 0; x < w; x++)
                    sample[0][x] = sc->fltmap[remap_index][ sample[0][x] ];

            if((ret = encode_line(f, sc, f->avctx, w, sample, plane_index, 8, ac, pass1)) < 0)
                return ret;
        } else {
            if (f->packed_at_lsb) {
                for (x = 0; x < w; x++) {
                    sample[0][x] = ((uint16_t*)(src + stride*y))[x * pixel_stride];
                }
            } else {
                for (x = 0; x < w; x++) {
                    sample[0][x] = ((uint16_t*)(src + stride*y))[x * pixel_stride] >> (16 - f->bits_per_raw_sample);
                }
            }
            if (sc->remap)
                for (x = 0; x < w; x++)
                    sample[0][x] = sc->fltmap[remap_index][ (uint16_t)sample[0][x] ];

            if((ret = encode_line(f, sc, f->avctx, w, sample, plane_index, f->bits_per_raw_sample, ac, pass1)) < 0)
                return ret;
        }
    }
    return 0;
}

static void load_plane(FFV1Context *f, FFV1SliceContext *sc,
                      const uint8_t *src, int w, int h,
                      int stride, int remap_index, int pixel_stride)
{
    int x, y;

    memset(sc->fltmap[remap_index], 0, 65536 * sizeof(*sc->fltmap[remap_index]));

    for (y = 0; y < h; y++) {
        if (f->bits_per_raw_sample <= 8) {
            for (x = 0; x < w; x++)
                sc->fltmap[remap_index][ src[x * pixel_stride + stride * y] ] = 1;
        } else {
            if (f->packed_at_lsb) {
                for (x = 0; x < w; x++)
                    sc->fltmap[remap_index][ ((uint16_t*)(src + stride*y))[x * pixel_stride] ] = 1;
            } else {
                for (x = 0; x < w; x++)
                    sc->fltmap[remap_index][ ((uint16_t*)(src + stride*y))[x * pixel_stride] >> (16 - f->bits_per_raw_sample) ] = 1;
            }
        }
    }
}

static void write_quant_table(RangeCoder *c, int16_t *quant_table)
{
    int last = 0;
    int i;
    uint8_t state[CONTEXT_SIZE];
    memset(state, 128, sizeof(state));

    for (i = 1; i < MAX_QUANT_TABLE_SIZE/2; i++)
        if (quant_table[i] != quant_table[i - 1]) {
            put_symbol(c, state, i - last - 1, 0);
            last = i;
        }
    put_symbol(c, state, i - last - 1, 0);
}

static void write_quant_tables(RangeCoder *c,
                               int16_t quant_table[MAX_CONTEXT_INPUTS][MAX_QUANT_TABLE_SIZE])
{
    int i;
    for (i = 0; i < 5; i++)
        write_quant_table(c, quant_table[i]);
}

static int contains_non_128(uint8_t (*initial_state)[CONTEXT_SIZE],
                            int nb_contexts)
{
    if (!initial_state)
        return 0;
    for (int i = 0; i < nb_contexts; i++)
        for (int j = 0; j < CONTEXT_SIZE; j++)
            if (initial_state[i][j] != 128)
                return 1;
    return 0;
}

static void write_header(FFV1Context *f)
{
    uint8_t state[CONTEXT_SIZE];
    int i, j;
    RangeCoder *const c = &f->slices[0].c;

    memset(state, 128, sizeof(state));

    if (f->version < 2) {
        put_symbol(c, state, f->version, 0);
        put_symbol(c, state, f->ac, 0);
        if (f->ac == AC_RANGE_CUSTOM_TAB) {
            for (i = 1; i < 256; i++)
                put_symbol(c, state,
                           f->state_transition[i] - c->one_state[i], 1);
        }
        put_symbol(c, state, f->colorspace, 0); //YUV cs type
        if (f->version > 0)
            put_symbol(c, state, f->bits_per_raw_sample, 0);
        put_rac(c, state, f->chroma_planes);
        put_symbol(c, state, f->chroma_h_shift, 0);
        put_symbol(c, state, f->chroma_v_shift, 0);
        put_rac(c, state, f->transparency);

        write_quant_tables(c, f->quant_tables[f->context_model]);
    } else if (f->version < 3) {
        put_symbol(c, state, f->slice_count, 0);
        for (i = 0; i < f->slice_count; i++) {
            FFV1SliceContext *fs = &f->slices[i];
            put_symbol(c, state,
                       (fs->slice_x      + 1) * f->num_h_slices / f->width, 0);
            put_symbol(c, state,
                       (fs->slice_y      + 1) * f->num_v_slices / f->height, 0);
            put_symbol(c, state,
                       (fs->slice_width  + 1) * f->num_h_slices / f->width - 1,
                       0);
            put_symbol(c, state,
                       (fs->slice_height + 1) * f->num_v_slices / f->height - 1,
                       0);
            for (j = 0; j < f->plane_count; j++) {
                put_symbol(c, state, fs->plane[j].quant_table_index, 0);
                av_assert0(fs->plane[j].quant_table_index == f->context_model);
            }
        }
    }
}

static void set_micro_version(FFV1Context *f)
{
    f->combined_version = f->version << 16;
    if (f->version > 2) {
        if (f->version == 3) {
            f->micro_version = 4;
        } else if (f->version == 4) {
            f->micro_version = 8;
        } else
            av_assert0(0);

        f->combined_version += f->micro_version;
    } else
        av_assert0(f->micro_version == 0);
}

av_cold int ff_ffv1_write_extradata(AVCodecContext *avctx)
{
    FFV1Context *f = avctx->priv_data;

    RangeCoder c;
    uint8_t state[CONTEXT_SIZE];
    int i, j, k;
    uint8_t state2[32][CONTEXT_SIZE];
    unsigned v;

    memset(state2, 128, sizeof(state2));
    memset(state, 128, sizeof(state));

    f->avctx->extradata_size = 10000 + 4 +
                                    (11 * 11 * 5 * 5 * 5 + 11 * 11 * 11) * 32;
    f->avctx->extradata = av_malloc(f->avctx->extradata_size + AV_INPUT_BUFFER_PADDING_SIZE);
    if (!f->avctx->extradata)
        return AVERROR(ENOMEM);
    ff_init_range_encoder(&c, f->avctx->extradata, f->avctx->extradata_size);
    ff_build_rac_states(&c, 0.05 * (1LL << 32), 256 - 8);

    put_symbol(&c, state, f->version, 0);
    if (f->version > 2)
        put_symbol(&c, state, f->micro_version, 0);

    put_symbol(&c, state, f->ac, 0);
    if (f->ac == AC_RANGE_CUSTOM_TAB)
        for (i = 1; i < 256; i++)
            put_symbol(&c, state, f->state_transition[i] - c.one_state[i], 1);

    put_symbol(&c, state, f->colorspace, 0); // YUV cs type
    put_symbol(&c, state, f->bits_per_raw_sample, 0);
    put_rac(&c, state, f->chroma_planes);
    put_symbol(&c, state, f->chroma_h_shift, 0);
    put_symbol(&c, state, f->chroma_v_shift, 0);
    put_rac(&c, state, f->transparency);
    put_symbol(&c, state, f->num_h_slices - 1, 0);
    put_symbol(&c, state, f->num_v_slices - 1, 0);

    put_symbol(&c, state, f->quant_table_count, 0);
    for (i = 0; i < f->quant_table_count; i++)
        write_quant_tables(&c, f->quant_tables[i]);

    for (i = 0; i < f->quant_table_count; i++) {
        if (contains_non_128(f->initial_states[i], f->context_count[i])) {
            put_rac(&c, state, 1);
            for (j = 0; j < f->context_count[i]; j++)
                for (k = 0; k < CONTEXT_SIZE; k++) {
                    int pred = j ? f->initial_states[i][j - 1][k] : 128;
                    put_symbol(&c, state2[k],
                               (int8_t)(f->initial_states[i][j][k] - pred), 1);
                }
        } else {
            put_rac(&c, state, 0);
        }
    }

    if (f->version > 2) {
        put_symbol(&c, state, f->ec, 0);
        put_symbol(&c, state, f->intra = (f->avctx->gop_size < 2), 0);
    }

    f->avctx->extradata_size = ff_rac_terminate(&c, 0);
    v = av_crc(av_crc_get_table(AV_CRC_32_IEEE), f->crcref, f->avctx->extradata, f->avctx->extradata_size) ^ (f->crcref ? 0x8CD88196 : 0);
    AV_WL32(f->avctx->extradata + f->avctx->extradata_size, v);
    f->avctx->extradata_size += 4;

    return 0;
}

static int sort_stt(FFV1Context *s, uint8_t stt[256])
{
    int i, i2, changed, print = 0;

    do {
        changed = 0;
        for (i = 12; i < 244; i++) {
            for (i2 = i + 1; i2 < 245 && i2 < i + 4; i2++) {

#define COST(old, new)                                      \
    s->rc_stat[old][0] * -log2((256 - (new)) / 256.0) +     \
    s->rc_stat[old][1] * -log2((new)         / 256.0)

#define COST2(old, new)                         \
    COST(old, new) + COST(256 - (old), 256 - (new))

                double size0 = COST2(i,  i) + COST2(i2, i2);
                double sizeX = COST2(i, i2) + COST2(i2, i);
                if (size0 - sizeX > size0*(1e-14) && i != 128 && i2 != 128) {
                    int j;
                    FFSWAP(int, stt[i], stt[i2]);
                    FFSWAP(int, s->rc_stat[i][0], s->rc_stat[i2][0]);
                    FFSWAP(int, s->rc_stat[i][1], s->rc_stat[i2][1]);
                    if (i != 256 - i2) {
                        FFSWAP(int, stt[256 - i], stt[256 - i2]);
                        FFSWAP(int, s->rc_stat[256 - i][0], s->rc_stat[256 - i2][0]);
                        FFSWAP(int, s->rc_stat[256 - i][1], s->rc_stat[256 - i2][1]);
                    }
                    for (j = 1; j < 256; j++) {
                        if (stt[j] == i)
                            stt[j] = i2;
                        else if (stt[j] == i2)
                            stt[j] = i;
                        if (i != 256 - i2) {
                            if (stt[256 - j] == 256 - i)
                                stt[256 - j] = 256 - i2;
                            else if (stt[256 - j] == 256 - i2)
                                stt[256 - j] = 256 - i;
                        }
                    }
                    print = changed = 1;
                }
            }
        }
    } while (changed);
    return print;
}


int ff_ffv1_encode_determine_slices(AVCodecContext *avctx)
{
    FFV1Context *s = avctx->priv_data;
    int plane_count = 1 + 2*s->chroma_planes + s->transparency;
    int max_h_slices = AV_CEIL_RSHIFT(avctx->width , s->chroma_h_shift);
    int max_v_slices = AV_CEIL_RSHIFT(avctx->height, s->chroma_v_shift);
    s->num_v_slices = (avctx->width > 352 || avctx->height > 288 || !avctx->slices) ? 2 : 1;
    s->num_v_slices = FFMIN(s->num_v_slices, max_v_slices);
    for (; s->num_v_slices < 32; s->num_v_slices++) {
        for (s->num_h_slices = s->num_v_slices; s->num_h_slices <= 2*s->num_v_slices; s->num_h_slices++) {
            int maxw = (avctx->width  + s->num_h_slices - 1) / s->num_h_slices;
            int maxh = (avctx->height + s->num_v_slices - 1) / s->num_v_slices;
            if (s->num_h_slices > max_h_slices || s->num_v_slices > max_v_slices)
                continue;
            if (maxw * maxh * (int64_t)(s->bits_per_raw_sample+1) * plane_count > 8<<24)
                continue;
            if (s->version < 4)
                if (  ff_need_new_slices(avctx->width , s->num_h_slices, s->chroma_h_shift)
                    ||ff_need_new_slices(avctx->height, s->num_v_slices, s->chroma_v_shift))
                    continue;
            if (avctx->slices == s->num_h_slices * s->num_v_slices && avctx->slices <= MAX_SLICES)
                return 0;
            if (maxw*maxh > 360*288)
                continue;
            if (!avctx->slices)
                return 0;
        }
    }
    av_log(avctx, AV_LOG_ERROR,
           "Unsupported number %d of slices requested, please specify a "
           "supported number with -slices (ex:4,6,9,12,16, ...)\n",
           avctx->slices);
    return AVERROR(ENOSYS);
}

av_cold int ff_ffv1_encode_init(AVCodecContext *avctx)
{
    FFV1Context *s = avctx->priv_data;
    int i, j, k, m, ret;

    if ((avctx->flags & (AV_CODEC_FLAG_PASS1 | AV_CODEC_FLAG_PASS2)) ||
        avctx->slices > 1)
        s->version = FFMAX(s->version, 2);

    if ((avctx->flags & (AV_CODEC_FLAG_PASS1 | AV_CODEC_FLAG_PASS2)) && s->ac == AC_GOLOMB_RICE) {
        av_log(avctx, AV_LOG_ERROR, "2 Pass mode is not possible with golomb coding\n");
        return AVERROR(EINVAL);
    }

    // Unspecified level & slices, we choose version 1.2+ to ensure multithreaded decodability
    if (avctx->slices == 0 && avctx->level < 0 && avctx->width * avctx->height > 720*576)
        s->version = FFMAX(s->version, 2);

    if (avctx->level <= 0 && s->version == 2) {
        s->version = 3;
    }
    if (avctx->level >= 0 && avctx->level <= 4) {
        if (avctx->level < s->version) {
            av_log(avctx, AV_LOG_ERROR, "Version %d needed for requested features but %d requested\n", s->version, avctx->level);
            return AVERROR(EINVAL);
        }
        s->version = avctx->level;
    }

    if (s->ec < 0) {
        if (s->version >= 4) {
            s->ec = 2;
            s->crcref = 0x7a8c4079;
        } else if (s->version >= 3) {
            s->ec = 1;
        } else
            s->ec = 0;
    }

    // CRC requires version 3+
    if (s->ec == 1)
        s->version = FFMAX(s->version, 3);
    if (s->ec == 2)
        s->version = FFMAX(s->version, 4);

    if ((s->version == 2 || s->version>3) && avctx->strict_std_compliance > FF_COMPLIANCE_EXPERIMENTAL) {
        av_log(avctx, AV_LOG_ERROR, "Version 2 or 4 needed for requested features but version 2 or 4 is experimental and not enabled\n");
        return AVERROR_INVALIDDATA;
    }

    if (s->ac == AC_RANGE_CUSTOM_TAB) {
        for (i = 1; i < 256; i++)
            s->state_transition[i] = ver2_state[i];
    } else {
        RangeCoder c;
        ff_build_rac_states(&c, 0.05 * (1LL << 32), 256 - 8);
        for (i = 1; i < 256; i++)
            s->state_transition[i] = c.one_state[i];
    }

    for (i = 0; i < 256; i++) {
        s->quant_table_count = 2;
        if ((s->qtable == -1 && s->bits_per_raw_sample <= 8) || s->qtable == 1) {
            s->quant_tables[0][0][i]=           quant11[i];
            s->quant_tables[0][1][i]=        11*quant11[i];
            s->quant_tables[0][2][i]=     11*11*quant11[i];
            s->quant_tables[1][0][i]=           quant11[i];
            s->quant_tables[1][1][i]=        11*quant11[i];
            s->quant_tables[1][2][i]=     11*11*quant5 [i];
            s->quant_tables[1][3][i]=   5*11*11*quant5 [i];
            s->quant_tables[1][4][i]= 5*5*11*11*quant5 [i];
            s->context_count[0] = (11 * 11 * 11        + 1) / 2;
            s->context_count[1] = (11 * 11 * 5 * 5 * 5 + 1) / 2;
        } else {
            s->quant_tables[0][0][i]=           quant9_10bit[i];
            s->quant_tables[0][1][i]=         9*quant9_10bit[i];
            s->quant_tables[0][2][i]=       9*9*quant9_10bit[i];
            s->quant_tables[1][0][i]=           quant9_10bit[i];
            s->quant_tables[1][1][i]=         9*quant9_10bit[i];
            s->quant_tables[1][2][i]=       9*9*quant5_10bit[i];
            s->quant_tables[1][3][i]=     5*9*9*quant5_10bit[i];
            s->quant_tables[1][4][i]=   5*5*9*9*quant5_10bit[i];
            s->context_count[0] = (9 * 9 * 9         + 1) / 2;
            s->context_count[1] = (9 * 9 * 5 * 5 * 5 + 1) / 2;
        }
    }

    if ((ret = ff_ffv1_allocate_initial_states(s)) < 0)
        return ret;

    if (!s->transparency)
        s->plane_count = 2;
    if (!s->chroma_planes && s->version > 3)
        s->plane_count--;

    s->picture_number = 0;

    if (avctx->flags & (AV_CODEC_FLAG_PASS1 | AV_CODEC_FLAG_PASS2)) {
        for (i = 0; i < s->quant_table_count; i++) {
            s->rc_stat2[i] = av_mallocz(s->context_count[i] *
                                        sizeof(*s->rc_stat2[i]));
            if (!s->rc_stat2[i])
                return AVERROR(ENOMEM);
        }
    }
    if (avctx->stats_in) {
        char *p = avctx->stats_in;
        uint8_t (*best_state)[256] = av_malloc_array(256, 256);
        int gob_count = 0;
        char *next;
        if (!best_state)
            return AVERROR(ENOMEM);

        av_assert0(s->version >= 2);

        for (;;) {
            for (j = 0; j < 256; j++)
                for (i = 0; i < 2; i++) {
                    s->rc_stat[j][i] = strtol(p, &next, 0);
                    if (next == p) {
                        av_log(avctx, AV_LOG_ERROR,
                               "2Pass file invalid at %d %d [%s]\n", j, i, p);
                        av_freep(&best_state);
                        return AVERROR_INVALIDDATA;
                    }
                    p = next;
                }
            for (i = 0; i < s->quant_table_count; i++)
                for (j = 0; j < s->context_count[i]; j++) {
                    for (k = 0; k < 32; k++)
                        for (m = 0; m < 2; m++) {
                            s->rc_stat2[i][j][k][m] = strtol(p, &next, 0);
                            if (next == p) {
                                av_log(avctx, AV_LOG_ERROR,
                                       "2Pass file invalid at %d %d %d %d [%s]\n",
                                       i, j, k, m, p);
                                av_freep(&best_state);
                                return AVERROR_INVALIDDATA;
                            }
                            p = next;
                        }
                }
            gob_count = strtol(p, &next, 0);
            if (next == p || gob_count <= 0) {
                av_log(avctx, AV_LOG_ERROR, "2Pass file invalid\n");
                av_freep(&best_state);
                return AVERROR_INVALIDDATA;
            }
            p = next;
            while (*p == '\n' || *p == ' ')
                p++;
            if (p[0] == 0)
                break;
        }
        if (s->ac == AC_RANGE_CUSTOM_TAB)
            sort_stt(s, s->state_transition);

        find_best_state(best_state, s->state_transition);

        for (i = 0; i < s->quant_table_count; i++) {
            for (k = 0; k < 32; k++) {
                double a=0, b=0;
                int jp = 0;
                for (j = 0; j < s->context_count[i]; j++) {
                    double p = 128;
                    if (s->rc_stat2[i][j][k][0] + s->rc_stat2[i][j][k][1] > 200 && j || a+b > 200) {
                        if (a+b)
                            p = 256.0 * b / (a + b);
                        s->initial_states[i][jp][k] =
                            best_state[av_clip(round(p), 1, 255)][av_clip_uint8((a + b) / gob_count)];
                        for(jp++; jp<j; jp++)
                            s->initial_states[i][jp][k] = s->initial_states[i][jp-1][k];
                        a=b=0;
                    }
                    a += s->rc_stat2[i][j][k][0];
                    b += s->rc_stat2[i][j][k][1];
                    if (a+b) {
                        p = 256.0 * b / (a + b);
                    }
                    s->initial_states[i][j][k] =
                        best_state[av_clip(round(p), 1, 255)][av_clip_uint8((a + b) / gob_count)];
                }
            }
        }
        av_freep(&best_state);
    }

    if (s->version <= 1) {
        /* Disable slices when the version doesn't support them */
        s->num_h_slices = 1;
        s->num_v_slices = 1;
    }

    set_micro_version(s);

    return 0;
}

av_cold int ff_ffv1_encode_setup_plane_info(AVCodecContext *avctx,
                                            enum AVPixelFormat pix_fmt)
{
    FFV1Context *s = avctx->priv_data;
    const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(pix_fmt);

    s->plane_count = 3;
    switch(pix_fmt) {
    case AV_PIX_FMT_GRAY9:
    case AV_PIX_FMT_YUV444P9:
    case AV_PIX_FMT_YUV422P9:
    case AV_PIX_FMT_YUV420P9:
    case AV_PIX_FMT_YUVA444P9:
    case AV_PIX_FMT_YUVA422P9:
    case AV_PIX_FMT_YUVA420P9:
        if (!avctx->bits_per_raw_sample)
            s->bits_per_raw_sample = 9;
    case AV_PIX_FMT_GRAY10:
    case AV_PIX_FMT_YUV444P10:
    case AV_PIX_FMT_YUV440P10:
    case AV_PIX_FMT_YUV420P10:
    case AV_PIX_FMT_YUV422P10:
    case AV_PIX_FMT_YUVA444P10:
    case AV_PIX_FMT_YUVA422P10:
    case AV_PIX_FMT_YUVA420P10:
        if (!avctx->bits_per_raw_sample && !s->bits_per_raw_sample)
            s->bits_per_raw_sample = 10;
    case AV_PIX_FMT_GRAY12:
    case AV_PIX_FMT_YUV444P12:
    case AV_PIX_FMT_YUV440P12:
    case AV_PIX_FMT_YUV420P12:
    case AV_PIX_FMT_YUV422P12:
    case AV_PIX_FMT_YUVA444P12:
    case AV_PIX_FMT_YUVA422P12:
        if (!avctx->bits_per_raw_sample && !s->bits_per_raw_sample)
            s->bits_per_raw_sample = 12;
    case AV_PIX_FMT_GRAY14:
    case AV_PIX_FMT_YUV444P14:
    case AV_PIX_FMT_YUV420P14:
    case AV_PIX_FMT_YUV422P14:
        if (!avctx->bits_per_raw_sample && !s->bits_per_raw_sample)
            s->bits_per_raw_sample = 14;
        s->packed_at_lsb = 1;
    case AV_PIX_FMT_GRAY16:
    case AV_PIX_FMT_YUV444P16:
    case AV_PIX_FMT_YUV422P16:
    case AV_PIX_FMT_YUV420P16:
    case AV_PIX_FMT_YUVA444P16:
    case AV_PIX_FMT_YUVA422P16:
    case AV_PIX_FMT_YUVA420P16:
    case AV_PIX_FMT_GRAYF16:
    case AV_PIX_FMT_YAF16:
        if (!avctx->bits_per_raw_sample && !s->bits_per_raw_sample) {
            s->bits_per_raw_sample = 16;
        } else if (!s->bits_per_raw_sample) {
            s->bits_per_raw_sample = avctx->bits_per_raw_sample;
        }
        if (s->bits_per_raw_sample <= 8) {
            av_log(avctx, AV_LOG_ERROR, "bits_per_raw_sample invalid\n");
            return AVERROR_INVALIDDATA;
        }
        s->version = FFMAX(s->version, 1);
    case AV_PIX_FMT_GRAY8:
    case AV_PIX_FMT_YA8:
    case AV_PIX_FMT_YUV444P:
    case AV_PIX_FMT_YUV440P:
    case AV_PIX_FMT_YUV422P:
    case AV_PIX_FMT_YUV420P:
    case AV_PIX_FMT_YUV411P:
    case AV_PIX_FMT_YUV410P:
    case AV_PIX_FMT_YUVA444P:
    case AV_PIX_FMT_YUVA422P:
    case AV_PIX_FMT_YUVA420P:
        s->chroma_planes = desc->nb_components < 3 ? 0 : 1;
        s->colorspace = 0;
        s->transparency = !!(desc->flags & AV_PIX_FMT_FLAG_ALPHA);
        if (!avctx->bits_per_raw_sample && !s->bits_per_raw_sample)
            s->bits_per_raw_sample = 8;
        else if (!s->bits_per_raw_sample)
            s->bits_per_raw_sample = 8;
        break;
    case AV_PIX_FMT_RGB32:
        s->colorspace = 1;
        s->transparency = 1;
        s->chroma_planes = 1;
        s->bits_per_raw_sample = 8;
        break;
    case AV_PIX_FMT_RGBA64:
        s->colorspace = 1;
        s->transparency = 1;
        s->chroma_planes = 1;
        s->bits_per_raw_sample = 16;
        s->use32bit = 1;
        s->version = FFMAX(s->version, 1);
        break;
    case AV_PIX_FMT_RGB48:
        s->colorspace = 1;
        s->chroma_planes = 1;
        s->bits_per_raw_sample = 16;
        s->use32bit = 1;
        s->version = FFMAX(s->version, 1);
        break;
    case AV_PIX_FMT_0RGB32:
        s->colorspace = 1;
        s->chroma_planes = 1;
        s->bits_per_raw_sample = 8;
        break;
    case AV_PIX_FMT_GBRP9:
        if (!avctx->bits_per_raw_sample)
            s->bits_per_raw_sample = 9;
    case AV_PIX_FMT_GBRP10:
    case AV_PIX_FMT_GBRAP10:
        if (!avctx->bits_per_raw_sample && !s->bits_per_raw_sample)
            s->bits_per_raw_sample = 10;
    case AV_PIX_FMT_GBRP12:
    case AV_PIX_FMT_GBRAP12:
        if (!avctx->bits_per_raw_sample && !s->bits_per_raw_sample)
            s->bits_per_raw_sample = 12;
    case AV_PIX_FMT_GBRP14:
    case AV_PIX_FMT_GBRAP14:
        if (!avctx->bits_per_raw_sample && !s->bits_per_raw_sample)
            s->bits_per_raw_sample = 14;
    case AV_PIX_FMT_GBRP16:
    case AV_PIX_FMT_GBRAP16:
    case AV_PIX_FMT_GBRPF16:
    case AV_PIX_FMT_GBRAPF16:
        if (!avctx->bits_per_raw_sample && !s->bits_per_raw_sample)
            s->bits_per_raw_sample = 16;
    case AV_PIX_FMT_GBRPF32:
    case AV_PIX_FMT_GBRAPF32:
        if (!avctx->bits_per_raw_sample && !s->bits_per_raw_sample)
            s->bits_per_raw_sample = 32;
        else if (!s->bits_per_raw_sample)
            s->bits_per_raw_sample = avctx->bits_per_raw_sample;
        s->transparency = !!(desc->flags & AV_PIX_FMT_FLAG_ALPHA);
        s->colorspace = 1;
        s->chroma_planes = 1;
        if (s->bits_per_raw_sample >= 16) {
            s->use32bit = 1;
        }
        s->version = FFMAX(s->version, 1);
        break;
    default:
        av_log(avctx, AV_LOG_ERROR, "format %s not supported\n",
               av_get_pix_fmt_name(pix_fmt));
        return AVERROR(ENOSYS);
    }
    s
