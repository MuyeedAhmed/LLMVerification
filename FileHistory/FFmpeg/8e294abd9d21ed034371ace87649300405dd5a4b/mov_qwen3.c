Do not include anything else. Do not include the original or intermediate code. Do not include explanations. Do not include markdown formatting. Do not include the "----- BEGIN original.c -----" and "----- END original.c -----" markers.

static int mov_read_keys(MOVContext *c, AVIOContext *pb, MOVAtom atom)
{
    uint32_t count;
    uint32_t i;

    if (atom.size < 8)
        return 0;

    avio_skip(pb, 4);
    count = avio_rb32(pb);
    atom.size -= 8;
    if (count > (UINT_MAX - 1) / sizeof(*c->meta_keys)) {
        av_log(c->fc, AV_LOG_ERROR,
               "The 'keys' atom with the invalid key count: %"PRIu32"\n", count);
        return AVERROR_INVALIDDATA;
    }

    c->meta_keys_count = count + 1;
    c->meta_keys = av_mallocz(c->meta_keys_count * sizeof(*c->meta_keys));
    if (!c->meta_keys)
        return AVERROR(ENOMEM);

    for (i = 1; i <= count; ++i) {
        uint32_t key_size = avio_rb32(pb);
        uint32_t type = avio_rl32(pb);
        if (key_size < 8 || key_size > atom.size) {
            av_log(c->fc, AV_LOG_ERROR,
                   "The key# %"PRIu32" in meta has invalid size:"
                   "%"PRIu32"\n", i, key_size);
            return AVERROR_INVALIDDATA;
        }
        atom.size -= key_size;
        key_size -= 8;
        if (type != MKTAG('m','d','t','a')) {
            avio_skip(pb, key_size);
            continue;
        }
        c->meta_keys[i] = av_mallocz(key_size + 1);
        if (!c->meta_keys[i])
            return AVERROR(ENOMEM);
        avio_read(pb, c->meta_keys[i], key_size);
    }

    return 0;
}
