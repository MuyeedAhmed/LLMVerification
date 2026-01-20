No explanation needed.
```c
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
        ret = read_hybrid_uint_conf(gb, &bundle->dists[i].config, bundle->log_alphabet_size);
        if (ret < 0)
            return ret;
        if (get_bits_left(gb) < 0)
            return AVERROR_BUFFER_TOO_SMALL;
    }

    if (bundle->use_prefix_code) {
        for (int i = 0; i < bundle->num_clusters; i++) {
            JXLSymbolDistribution *dist = &bundle->dists[i];
            if (get_bits1(gb)) {
                int n = get_bits(gb, 4);
                dist->alphabet_size = 1 + (1 << n) + get_bitsz(gb, n);
                if (dist->alphabet_size > (1 << 15))
                    return AVERROR_INVALIDDATA;
            } else {
                dist->alphabet_size = 1;
            }
            dist->log_alphabet_size = clog1p(dist->alphabet_size - 1);
        }
        for (int i = 0; i < bundle->num_clusters; i++) {
            ret = read_vlc_prefix(gb, dec, &bundle->dists[i]);
            if (ret < 0)
                return ret;
            if (get_bits_left(gb) < 0)
                return AVERROR_BUFFER_TOO_SMALL;
        }
    } else {
        for (int i = 0; i < bundle->num_clusters; i++) {
            ret = populate_distribution(gb, &bundle->dists[i], bundle->log_alphabet_size);
            if (ret < 0)
                return ret;
            if (get_bits_left(gb) < 0)
                return AVERROR_BUFFER_TOO_SMALL;
        }
        for (int i = 0; i < bundle->num_clusters; i++) {
            ret = gen_alias_map(dec, &bundle->dists[i], bundle->log_alphabet_size);
            if (ret < 0)
                return ret;
        }
    }

    return 0;
}
```
