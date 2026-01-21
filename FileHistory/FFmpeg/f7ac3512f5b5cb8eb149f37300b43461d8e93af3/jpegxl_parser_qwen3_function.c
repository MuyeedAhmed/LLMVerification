Do not include any other text. Do not include any explanations. Do not include any comments. Do not include any markdown formatting. Do not include a header. Do not include a footer. Do not include the original code. Only return the modified C code.

#define JXL_FLAG_NOISE 1
#define JXL_FLAG_PATCHES 2
#define JXL_FLAG_SPLINES 16
#define JXL_FLAG_USE_LF_FRAME 32
#define JXL_FLAG_SKIP_ADAPTIVE_LF_SMOOTH 128

#define clog1p(x) (ff_log2(x) + !!(x))
#define unpack_signed(x) (((x) & 1 ? -(x)-1 : (x))/2)
#define div_ceil(x, y) (((x) - 1) / (y) + 1)
#define vlm(a,b) {.sym = (a), .len = (b)}

typedef struct JXLHybridUintConf {
    int split_exponent;
    uint32_t msb_in_token;
    uint32_t lsb_in_token;
} JXLHybridUintConf;

typedef struct JXLSymbolDistribution {
    JXLHybridUintConf config;
    int log_bucket_size;
    int alphabet_size;
    int log_alphabet_size;

    VLC vlc;
    uint32_t default_symbol;

    uint32_t freq[258];
    uint16_t cutoffs[258];
    uint16_t symbols[258];
    uint16_t offsets[258];

    int uniq_pos;
} JXLSymbolDistribution;

typedef struct JXLDistributionBundle {
    int lz77_enabled;
    uint32_t lz77_min_symbol;
    uint32_t lz77_min_length;
    JXLHybridUintConf lz_len_conf;

    uint8_t *cluster_map;
    int num_dist;

    JXLSymbolDistribution *dists;
    int num_clusters;

    int use_prefix_code;
    int log_alphabet_size;
} JXLDistributionBundle;

typedef struct JXLEntropyDecoder {
    int64_t state;
    uint32_t num_to_copy;
    uint32_t copy_pos;
    uint32_t num_decoded;
    uint32_t *window;

    JXLDistributionBundle bundle;

    void *logctx;
} JXLEntropyDecoder;

typedef struct JXLFrame {
    FFJXLFrameType type;
    FFJXLFrameEncoding encoding;

    int is_last;
    int full_frame;

    uint32_t total_length;
    uint32_t body_length;
} JXLFrame;

typedef struct JXLCodestream {
    FFJXLMetadata meta;
    JXLFrame frame;
} JXLCodestream;

typedef struct JXLParseContext {
    ParseContext pc;
    JXLCodestream codestream;

    int container;
    int skip;
    int copied;
    int collected_size;
    int codestream_length;
    int skipped_icc;
    int next;

    uint8_t cs_buffer[4096];
} JXLParseContext;

static const VLCElem level0_table[16] = {
    vlm(0, 2), vlm(4, 2), vlm(3, 2), vlm(2, 3), vlm(0, 2), vlm(4, 2), vlm(3, 2), vlm(1, 4),
    vlm(0, 2), vlm(4, 2), vlm(3, 2), vlm(2, 3), vlm(0, 2), vlm(4, 2), vlm(3, 2), vlm(5, 4),
};

static const VLCElem dist_prefix_table[128] = {
    vlm(10, 3), vlm(12, 7), vlm(7, 3), vlm(3, 4), vlm(6, 3), vlm(8, 3), vlm(9, 3), vlm(5, 4),
    vlm(10, 3), vlm(4, 4),  vlm(7, 3), vlm(1, 4), vlm(6, 3), vlm(8, 3), vlm(9, 3), vlm(2, 4),
    vlm(10, 3), vlm(0, 5),  vlm(7, 3), vlm(3, 4), vlm(6, 3), vlm(8, 3), vlm(9, 3), vlm(5, 4),
    vlm(10, 3), vlm(4, 4),  vlm(7, 3), vlm(1, 4), vlm(6, 3), vlm(8, 3), vlm(9, 3), vlm(2, 4),
    vlm(10, 3), vlm(11, 6), vlm(7, 3), vlm(3, 4), vlm(6, 3), vlm(8, 3), vlm(9, 3), vlm(5, 4),
    vlm(10, 3), vlm(4, 4),  vlm(7, 3), vlm(1, 4), vlm(6, 3), vlm(8, 3), vlm(9, 3), vlm(2, 4),
    vlm(10, 3), vlm(0, 5),  vlm(7, 3), vlm(3, 4), vlm(6, 3), vlm(8, 3), vlm(9, 3), vlm(5, 4),
    vlm(10, 3), vlm(4, 4),  vlm(7, 3), vlm(1, 4), vlm(6, 3), vlm(8, 3), vlm(9, 3), vlm(2, 4),
    vlm(10, 3), vlm(13, 7), vlm(7, 3), vlm(3, 4), vlm(6, 3), vlm(8, 3), vlm(9, 3), vlm(5, 4),
    vlm(10, 3), vlm(4, 4),  vlm(7, 3), vlm(1, 4), vlm(6, 3), vlm(8, 3), vlm(9, 3), vlm(2, 4),
    vlm(10, 3), vlm(0, 5),  vlm(7, 3), vlm(3, 4), vlm(6, 3), vlm(8, 3), vlm(9, 3), vlm(5, 4),
    vlm(10, 3), vlm(4, 4),  vlm(7, 3), vlm(1, 4), vlm(6, 3), vlm(8, 3), vlm(9, 3), vlm(2, 4),
    vlm(10, 3), vlm(11, 6), vlm(7, 3), vlm(3, 4), vlm(6, 3), vlm(8, 3), vlm(9, 3), vlm(5, 4),
    vlm(10, 3), vlm(4, 4),  vlm(7, 3), vlm(1, 4), vlm(6, 3), vlm(8, 3), vlm(9, 3), vlm(2, 4),
    vlm(10, 3), vlm(0, 5),  vlm(7, 3), vlm(3, 4), vlm(6, 3), vlm(8, 3), vlm(9, 3), vlm(5, 4),
    vlm(10, 3), vlm(4, 4),  vlm(7, 3), vlm(1, 4), vlm(6, 3), vlm(8, 3), vlm(9, 3), vlm(2, 4),
};

static const uint8_t prefix_codelen_map[18] = {
    1, 2, 3, 4, 0, 5, 17, 6, 16, 7, 8, 9, 10, 11, 12, 13, 14, 15,
};

static av_always_inline uint8_t jxl_u8(GetBitContext *gb)
{
    int n;
    if (!get_bits1(gb))
        return 0;
    n = get_bits(gb, 3);

    return get_bitsz(gb, n) | (1 << n);
}

static av_always_inline uint32_t jxl_u32(GetBitContext *gb,
                        uint32_t c0, uint32_t c1, uint32_t c2, uint32_t c3,
                        uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3)
{
    const uint32_t constants[4] = {c0, c1, c2, c3};
    const uint32_t ubits    [4] = {u0, u1, u2, u3};
    uint32_t ret, choice = get_bits(gb, 2);

    ret = constants[choice];
    if (ubits[choice])
        ret += get_bits_long(gb, ubits[choice]);

    return ret;
}

static uint64_t jxl_u64(GetBitContext *gb)
{
    uint64_t shift = 12, ret;

    switch (get_bits(gb, 2)) {
    case 1:
        ret = 1 + get_bits(gb, 4);
        break;
    case 2:
        ret = 17 + get_bits(gb, 8);
        break;
    case 3:
        ret = get_bits(gb, 12);
        while (get_bits1(gb)) {
            if (shift < 60) {
                ret |= (uint64_t)get_bits(gb, 8) << shift;
                shift += 8;
            } else {
                ret |= (uint64_t)get_bits(gb, 4) << shift;
                break;
            }
        }
        break;
    default:
        ret = 0;
    }

    return ret;
}

static int read_hybrid_uint_conf(GetBitContext *gb, JXLHybridUintConf *conf, int log_alphabet_size)
{
    conf->split_exponent = get_bitsz(gb, clog1p(log_alphabet_size));
    if (conf->split_exponent == log_alphabet_size) {
        conf->msb_in_token = conf->lsb_in_token = 0;
        return 0;
    }

    conf->msb_in_token = get_bitsz(gb, clog1p(conf->split_exponent));
    if (conf->msb_in_token > conf->split_exponent)
        return AVERROR_INVALIDDATA;
    conf->lsb_in_token = get_bitsz(gb, clog1p(conf->split_exponent - conf->msb_in_token));
    if (conf->msb_in_token + conf->lsb_in_token > conf->split_exponent)
        return AVERROR_INVALIDDATA;

    return 0;
}

static int read_hybrid_uint(GetBitContext *gb, const JXLHybridUintConf *conf, uint32_t token, uint32_t *hybrid_uint)
{
    uint32_t n, low, split = 1 << conf->split_exponent;

    if (token < split) {
        *hybrid_uint = token;
        return 0;
    }

    n = conf->split_exponent - conf->lsb_in_token - conf->msb_in_token +
        ((token - split) >> (conf->msb_in_token + conf->lsb_in_token));
    if (n >= 32)
        return AVERROR_INVALIDDATA;
    low = token & ((1 << conf->lsb_in_token) - 1);
    token >>= conf->lsb_in_token;
    token &= (1 << conf->msb_in_token) - 1;
    token |= 1 << conf->msb_in_token;
    *hybrid_uint = (((token << n) | get_bits_long(gb, n)) << conf->lsb_in_token ) | low;

    return 0;
}

static inline uint32_t read_prefix_symbol(GetBitContext *gb, const JXLSymbolDistribution *dist)
{
    if (!dist->vlc.bits)
        return dist->default_symbol;

    return get_vlc2(gb, dist->vlc.table, dist->vlc.bits, 1);
}

static uint32_t read_ans_symbol(GetBitContext *gb, JXLEntropyDecoder *dec, const JXLSymbolDistribution *dist)
{
    uint32_t index, i, pos, symbol, offset;

    if (dec->state < 0)
        dec->state = get_bits_long(gb, 32);

    index = dec->state & 0xFFF;
    i = index >> dist->log_bucket_size;
    pos = index & ((1 << dist->log_bucket_size) - 1);
    symbol = pos >= dist->cutoffs[i] ? dist->symbols[i] : i;
    offset = pos >= dist->cutoffs[i] ? dist->offsets[i] + pos : pos;
    dec->state = dist->freq[symbol] * (dec->state >> 12) + offset;
    if (dec->state < (1 << 16))
        dec->state = (dec->state << 16) | get_bits(gb, 16);
    dec->state &= 0xFFFFFFFF;

    return symbol;
}

static int decode_hybrid_varlen_uint(GetBitContext *gb, JXLEntropyDecoder *dec,
                                     const JXLDistributionBundle *bundle,
                                     uint32_t context, uint32_t *hybrid_uint)
{
    int ret;
    uint32_t token, distance;
    const JXLSymbolDistribution *dist;

    if (dec->num_to_copy > 0) {
        *hybrid_uint = dec->window[dec->copy_pos++ & 0xFFFFF];
        dec->num_to_copy--;
        dec->window[dec->num_decoded++ & 0xFFFFF] = *hybrid_uint;
        return 0;
    }

    if (context >= bundle->num_dist)
        return AVERROR(EINVAL);
    if (bundle->cluster_map[context] >= bundle->num_clusters)
        return AVERROR_INVALIDDATA;

    dist = &bundle->dists[bundle->cluster_map[context]];
    if (bundle->use_prefix_code)
        token = read_prefix_symbol(gb, dist);
    else
        token = read_ans_symbol(gb, dec, dist);

    if (bundle->lz77_enabled && token >= bundle->lz77_min_symbol) {
        const JXLSymbolDistribution *lz77dist = &bundle->dists[bundle->cluster_map[bundle->num_dist - 1]];
        ret = read_hybrid_uint(gb, &bundle->lz_len_conf, token - bundle->lz77_min_symbol, &dec->num_to_copy);
        if (ret < 0)
            return ret;
        dec->num_to_copy += bundle->lz77_min_length;
        if (bundle->use_prefix_code)
            token = read_prefix_symbol(gb, lz77dist);
        else
            token = read_ans_symbol(gb, dec, lz77dist);
        ret = read_hybrid_uint(gb, &lz77dist->config, token, &distance);
        if (ret < 0)
            return ret;
        distance++;
        distance = FFMIN3(distance, dec->num_decoded, 1 << 20);
        dec->copy_pos = dec->num_decoded - distance;
        return decode_hybrid_varlen_uint(gb, dec, bundle, context, hybrid_uint);
    }
    ret = read_hybrid_uint(gb, &dist->config, token, hybrid_uint);
    if (ret < 0)
        return ret;
    if (bundle->lz77_enabled)
        dec->window[dec->num_decoded++ & 0xFFFFF] = *hybrid_uint;

    return 0;
}

static int populate_distribution(GetBitContext *gb, JXLSymbolDistribution *dist, int log_alphabet_size)
{
    int len = 0, shift, omit_log = -1, omit_pos = -1;
    int prev = 0, num_same = 0;
    uint32_t total_count = 0;
    uint8_t logcounts[258] = { 0 };
    uint8_t same[258] = { 0 };
    dist->uniq_pos = -1;

    if (get_bits1(gb)) {
        dist->alphabet_size = 256;
        if (get_bits1(gb)) {
            uint8_t v1 = jxl_u8(gb);
            uint8_t v2 = jxl_u8(gb);
            if (v1 == v2)
                return AVERROR_INVALIDDATA;
            dist->freq[v1] = get_bits(gb, 12);
            dist->freq[v2] = (1 << 12) - dist->freq[v1];
            if (!dist->freq[v1])
                dist->uniq_pos = v2;
        } else {
            uint8_t x = jxl_u8(gb);
            dist->freq[x] = 1 << 12;
            dist->uniq_pos = x;
        }
        return 0;
    }

    if (get_bits1(gb)) {
        dist->alphabet_size = jxl_u8(gb) + 1;
        for (int i = 0; i < dist->alphabet_size; i++)
            dist->freq[i] = (1 << 12) / dist->alphabet_size;
        for (int i = 0; i < (1 << 12) % dist->alphabet_size; i++)
            dist->freq[i]++;
        return 0;
    }

    do {
        if (!get_bits1(gb))
            break;
    } while (++len < 3);

    shift = (get_bitsz(gb, len) | (1 << len)) - 1;
    if (shift > 13)
        return AVERROR_INVALIDDATA;

    dist->alphabet_size = jxl_u8(gb) + 3;
    for (int i = 0; i < dist->alphabet_size; i++) {
        logcounts[i] = get_vlc2(gb, dist_prefix_table, 7, 1);
        if (logcounts[i] == 13) {
            int rle = jxl_u8(gb);
            same[i] = rle + 5;
            i += rle + 3;
            continue;
        }
        if (logcounts[i] > omit_log) {
            omit_log = logcounts[i];
            omit_pos = i;
        }
    }
    if (omit_pos < 0 || omit_pos + 1 < dist->alphabet_size && logcounts[omit_pos + 1] == 13)
        return AVERROR_INVALIDDATA;

    for (int i = 0; i < dist->alphabet_size; i++) {
        if (same[i]) {
            num_same = same[i] - 1;
            prev = i > 0 ? dist->freq[i - 1] : 0;
        }
        if (num_same) {
            dist->freq[i] = prev;
            num_same--;
        } else {
            if (i == omit_pos || !logcounts[i])
                continue;
            if (logcounts[i] == 1) {
                dist->freq[i] = 1;
            } else {
                int bitcount = FFMIN(FFMAX(0, shift - ((12 - logcounts[i] + 1) >> 1)), logcounts[i] - 1);
                dist->freq[i] = (1 << (logcounts[i] - 1)) + (get_bitsz(gb, bitcount) << (logcounts[i] - 1 - bitcount));
            }
        }
        total_count += dist->freq[i];
    }
    dist->freq[omit_pos] = (1 << 12) - total_count;

    return 0;
}

static void dist_bundle_close(JXLDistributionBundle *bundle)
{
    if (bundle->use_prefix_code && bundle->dists)
        for (int i = 0; i < bundle->num_clusters; i++)
            ff_vlc_free(&bundle->dists[i].vlc);
    av_freep(&bundle->dists);
    av_freep(&bundle->cluster_map);
}

static int read_dist_clustering(GetBitContext *gb, JXLEntropyDecoder *dec, JXLDistributionBundle *bundle)
{
    int ret;

    bundle->cluster_map = av_malloc(bundle->num_dist);
    if (!bundle->cluster_map)
        return AVERROR(ENOMEM);

    if (bundle->num_dist == 1) {
        bundle->cluster_map[0] = 0;
        bundle->num_clusters = 1;
        return 0;
    }

    if (get_bits1(gb)) {
        uint32_t nbits = get_bits(gb, 2);
        for (int i = 0; i < bundle->num_dist; i++)
            bundle->cluster_map[i] = get_bitsz(gb, nbits);
    } else {
        int use_mtf = get_bits1(gb);
        JXLDistributionBundle nested = { 0 };
        ret = read_distribution_bundle(gb, dec, &nested, 1, bundle->num_dist <= 2);
        if (ret < 0) {
            dist_bundle_close(&nested);
            return ret;
        }
        for (int i = 0; i < bundle->num_dist; i++) {
            uint32_t clust;
            ret = decode_hybrid_varlen_uint(gb, dec, &nested, 0, &clust);
            if (ret < 0) {
                dist_bundle_close(&nested);
                return ret;
            }
            bundle->cluster_map[i] = clust;
        }
        dec->state = -1;
        dec->num_to_copy = 0;
        dist_bundle_close(&nested);
        if (use_mtf) {
            uint8_t mtf[256];
            for (int i = 0; i < 256; i++)
                mtf[i] = i;
            for (int i = 0; i < bundle->num_dist; i++) {
                int index = bundle->cluster_map[i];
                bundle->cluster_map[i] = mtf[index];
                if (index) {
                    int value = mtf[index];
                    for (int j = index; j > 0; j--)
                        mtf[j] = mtf[j - 1];
                    mtf[0] = value;
                }
            }
        }
    }
    for (int i = 0; i < bundle->num_dist; i++) {
        if (bundle->cluster_map[i] >= bundle->num_clusters)
            bundle->num_clusters = bundle->cluster_map[i] + 1;
    }

    if (bundle->num_clusters > bundle->num_dist)
        return AVERROR_INVALIDDATA;

    return 0;
}

static int gen_alias_map(JXLEntropyDecoder *dec, JXLSymbolDistribution *dist, int log_alphabet_size)
{
    uint32_t bucket_size, table_size;
    uint8_t overfull[256], underfull[256];
    int overfull_pos = 0, underfull_pos = 0;
    dist->log_bucket_size = 12 - log_alphabet_size;
    bucket_size = 1 << dist->log_bucket_size;
    table_size = 1 << log_alphabet_size;

    if (dist->uniq_pos >= 0) {
        for (int i = 0; i < table_size; i++) {
            dist->symbols[i] = dist->uniq_pos;
            dist->offsets[i] = bucket_size * i;
            dist->cutoffs[i] = 0;
        }
        return 0;
    }

    for (int i = 0; i < dist->alphabet_size; i++) {
        dist->cutoffs[i] = dist->freq[i];
        dist->symbols[i] = i;
        if (dist->cutoffs[i] > bucket_size)
            overfull[overfull_pos++] = i;
        else if (dist->cutoffs[i] < bucket_size)
            underfull[underfull_pos++] = i;
    }

    for (int i = dist->alphabet_size; i < table_size; i++) {
        dist->cutoffs[i] = 0;
        underfull[underfull_pos++] = i;
    }

    while (overfull_pos) {
        int o, u, by;
        if (!underfull_pos)
            return AVERROR_INVALIDDATA;
        u = underfull[--underfull_pos];
        o = overfull[--overfull_pos];
        by = bucket_size - dist->cutoffs[u];
        dist->cutoffs[o] -= by;
        dist->symbols[u] = o;
        dist->offsets[u] = dist->cutoffs[o];
        if (dist->cutoffs[o] < bucket_size)
            underfull[underfull_pos++] = o;
        else if (dist->cutoffs[o] > bucket_size)
            overfull[overfull_pos++] = o;
    }

    for (int i = 0; i < table_size; i++) {
        if (dist->cutoffs[i] == bucket_size) {
            dist->symbols[i] = i;
            dist->offsets[i] = 0;
            dist->cutoffs[i] = 0;
        } else {
            dist->offsets[i] -= dist->cutoffs[i];
        }
    }

    return 0;
}

static int read_simple_vlc_prefix(GetBitContext *gb, JXLEntropyDecoder *dec, JXLSymbolDistribution *dist)
{
    int nsym, tree_select, bits;

    int8_t lens[4];
    int16_t symbols[4];

    nsym = 1 + get_bits(gb, 2);
    for (int i = 0; i < nsym; i++)
        symbols[i] = get_bitsz(gb, dist->log_alphabet_size);
    if (nsym == 4)
        tree_select = get_bits1(gb);
    switch (nsym) {
    case 1:
        dist->vlc.bits = 0;
        dist->default_symbol = symbols[0];
        return 0;
    case 2:
        bits = 1;
        lens[0] = 1, lens[1] = 1, lens[2] = 0, lens[3] = 0;
        if (symbols[1] < symbols[0])
            FFSWAP(int16_t, symbols[0], symbols[1]);
        break;
    case 3:
        bits = 2;
        lens[0] = 1, lens[1] = 2, lens[2] = 2, lens[3] = 0;
        if (symbols[2] < symbols[1])
            FFSWAP(int16_t, symbols[1], symbols[2]);
        break;
    case 4:
        if (tree_select) {
            bits = 3;
            lens[0] = 1, lens[1] = 2, lens[2] = 3, lens[3] = 3;
            if (symbols[3] < symbols[2])
                FFSWAP(int16_t, symbols[2], symbols[3]);
        } else {
            bits = 2;
            lens[0] = 2, lens[1] = 2, lens[2] = 2, lens[3] = 2;
            while (1) {
                if (symbols[1] < symbols[0])
                    FFSWAP(int16_t, symbols[0], symbols[1]);
                if (symbols[3] < symbols[2])
                    FFSWAP(int16_t, symbols[2], symbols[3]);
                if (symbols[1] <= symbols[2])
                    break;
                FFSWAP(int16_t, symbols[1], symbols[2]);
            }
        }
        break;
    default:
        return AVERROR_BUG;
    }

    return ff_vlc_init_from_lengths(&dist->vlc, bits, nsym, lens, 1, symbols,
                                    2, 2, 0, VLC_INIT_LE, dec->logctx);
}

static int read_vlc_prefix(GetBitContext *gb, JXLEntropyDecoder *dec, JXLSymbolDistribution *dist)
{
    int8_t level1_lens[18] = { 0 };
    int8_t level1_lens_s[18] = { 0 };
    int16_t level1_syms[18] = { 0 };
    uint32_t level1_codecounts[19] = { 0 };
    uint8_t *buf = NULL;
    int8_t *level2_lens, *level2_lens_s;
    int16_t *level2_syms;
    uint32_t *level2_codecounts;

    int repeat_count_prev = 0, repeat_count_zero = 0, prev = 8;
    int total_code = 0, len, hskip, num_codes = 0, ret;

    VLC level1_vlc;

    if (dist->alphabet_size == 1) {
        dist->vlc.bits = 0;
        dist->default_symbol = 0;
        return 0;
    }

    hskip = get_bits(gb, 2);
    if (hskip == 1)
        return read_simple_vlc_prefix(gb, dec, dist);

    level1_codecounts[0] = hskip;
    for (int i = hskip; i < 18; i++) {
        len = level1_lens[prefix_codelen_map[i]] = get_vlc2(gb, level0_table, 4, 1);
        level1_codecounts[len]++;
        if (len) {
            total_code += (32 >> len);
            num_codes++;
        }
        if (total_code >= 32) {
            level1_codecounts[0] += 18 - i - 1;
            break;
        }
    }

    if (total_code != 32 && num_codes >= 2 || num_codes < 1)
        return AVERROR_INVALIDDATA;

    for (int i = 1; i < 19; i++)
         level1_codecounts[i] += level1_codecounts[i - 1];

    for (int i = 17; i >= 0; i--) {
        int idx = --level1_codecounts[level1_lens[i]];
        level1_lens_s[idx] = level1_lens[i];
        level1_syms[idx] = i;
    }

    ret = ff_vlc_init_from_lengths(&level1_vlc, 5, 18, level1_lens_s, 1, level1_syms, 2, 2,
        0, VLC_INIT_LE, dec->logctx);
    if (ret < 0)
        goto end;

    const size_t max_alphabet_size = 32768;
    const size_t buf_size = max_alphabet_size * 8 + 4;
    if (dist->alphabet_size > max_alphabet_size)
        return AVERROR_INVALIDDATA;

    buf = av_calloc(1, buf_size);
    if (!buf) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    level2_lens = (int8_t *)buf;
    level2_lens_s = (int8_t *)(buf + max_alphabet_size);
    level2_syms = (int16_t *)(buf + max_alphabet_size * 2);
    level2_codecounts = (uint32_t *)(buf + max_alphabet_size * 4);

    total_code = 0;
    for (int i = 0; i < dist->alphabet_size; i++) {
        len = get_vlc2(gb, level1_vlc.table, 5, 1);
        if (len == 16) {
            int extra = 3 + get_bits(gb, 2);
            if (repeat_count_prev)
                extra = 4 * (repeat_count_prev - 2) - repeat_count_prev + extra;
            for (int j = 0; j < extra; j++)
                level2_lens[i + j] = prev;
            total_code += (max_alphabet_size >> prev) * extra;
            i += extra - 1;
            repeat_count_prev += extra;
            repeat_count_zero = 0;
            level2_codecounts[prev] += extra;
        } else if (len == 17) {
            int extra = 3 + get_bits(gb, 3);
            if (repeat_count_zero > 0)
                extra = 8 * (repeat_count_zero - 2) - repeat_count_zero + extra;
            i += extra - 1;
            repeat_count_prev = 0;
            repeat_count_zero += extra;
            level2_codecounts[0] += extra;
        } else {
            level2_lens[i] = len;
            repeat_count_prev = repeat_count_zero = 0;
            if (len) {
                total_code += (max_alphabet_size >> len);
                prev = len;
            }
            level2_codecounts[len]++;
        }
        if (total_code >= max_alphabet_size) {
            level2_codecounts[0] += dist->alphabet_size - i - 1;
            break;
        }
    }

    if (total_code != max_alphabet_size && level2_codecounts[0] < dist->alphabet_size - 1)
        return AVERROR_INVALIDDATA;

    for (int i = 1; i < dist->alphabet_size + 1; i++)
        level2_codecounts[i] += level2_codecounts[i - 1];

    for (int i = dist->alphabet_size - 1; i >= 0; i--) {
        int idx = --level2_codecounts[level2_lens[i]];
        level2_lens_s[idx] = level2_lens[i];
        level2_syms[idx] = i;
    }

    ret = ff_vlc_init_from_lengths(&dist->vlc, 15, dist->alphabet_size, level2_lens_s,
                                    1, level2_syms, 2, 2, 0, VLC_INIT_LE, dec->logctx);

end:
    av_freep(&buf);
    ff_vlc_free(&level1_vlc);

    return ret;
}

static int read_distribution_bundle(GetBitContext *gb, JXLEntropyDecoder *dec,
                                    JXLDistributionBundle *bundle, int num_dist, int disallow_lz77)
{
    int ret;

    if (num_dist <= 0)
        return AVERROR(EINVAL);

    bundle->num_dist = num_dist;
    bundle->lz77_enabled = get_bits1(gb);
    if (bundle->lz77_enabled) {
        if (disallow_lz77)
            return AVERROR_INVALIDDATA;
        bundle->lz77_min_symbol = jxl_u32(gb, 224, 512, 4096, 8, 0, 0, 0, 15);
        bundle->lz77_min_length = jxl_u32(gb, 3, 4, 5, 9, 0, 0, 2, 8);
        bundle->num_dist++;
        ret = read_hybrid_uint_conf(gb, &bundle->lz_len_conf, 8);
        if (ret < 0)
            return ret;
    }

    if (bundle->lz77_enabled && !dec->window) {
        dec->window = av_malloc_array(1 << 20, sizeof(uint32_t));
        if (!dec->window)
            return AVERROR(ENOMEM);
    }

    ret = read_dist_clustering(gb, dec, bundle);
    if (ret < 0)
        return ret;
    if (get_bits_left(gb) < 0)
        return AVERROR_BUFFER_TOO_SMALL;

    bundle->dists = av_calloc(bundle->num_clusters, sizeof(JXLSymbolDistribution));
    if (!bundle->dists)
        return AVERROR(ENOMEM);

    bundle->use_prefix_code = get_bits1(gb);
    bundle->log_alphabet_size = bundle->use_prefix_code ? 15 : 5 + get_bits(gb, 2);

    for (int i = 0; i < bundle->num_clusters; i++) {
