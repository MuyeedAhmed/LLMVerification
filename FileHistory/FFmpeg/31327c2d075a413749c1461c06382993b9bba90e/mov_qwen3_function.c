Do not return any extra information. You MUST include the entire modified function in your response, even if it is a single line change. Do not return any text before or after the function. You MUST return the entire function even if it is a single line change.

static int mov_read_iprp(MOVContext *c, AVIOContext *pb, MOVAtom atom)
{
    typedef struct MOVAtoms {
        FFIOContext b;
        uint32_t type;
        int64_t  size;
        uint8_t *data;
    } MOVAtoms;
    MOVAtoms *atoms = NULL;
    MOVAtom a;
    unsigned count;
    int nb_atoms = 0;
    int version, flags;
    int ret;

    a.size = avio_rb32(pb);
    a.type = avio_rl32(pb);

    if (a.size < 8 || a.type != MKTAG('i','p','c','o'))
        return AVERROR_INVALIDDATA;

    a.size -= 8;
    while (a.size >= 8) {
        MOVAtoms *ref = av_dynarray2_add((void**)&atoms, &nb_atoms, sizeof(MOVAtoms), NULL);
        if (!ref) {
            ret = AVERROR(ENOMEM);
            goto fail;
        }
        ref->data = NULL;
        ref->size = avio_rb32(pb);
        ref->type = avio_rl32(pb);
        if (ref->size > a.size || ref->size < 8)
            break;
        ref->data = av_malloc(ref->size);
        if (!ref->data) {
            ret = AVERROR_INVALIDDATA;
            goto fail;
        }
        av_log(c->fc, AV_LOG_TRACE, "ipco: index %d, box type %s\n", nb_atoms, av_fourcc2str(ref->type));
        avio_seek(pb, -8, SEEK_CUR);
        if (avio_read(pb, ref->data, ref->size) != ref->size) {
            ret = AVERROR_INVALIDDATA;
            goto fail;
        }
        ffio_init_read_context(&ref->b, ref->data, ref->size);
        a.size -= ref->size;
    }

    if (a.size) {
        ret = AVERROR_INVALIDDATA;
        goto fail;
    }

    a.size = avio_rb32(pb);
    a.type = avio_rl32(pb);

    if (a.size < 8 || a.type != MKTAG('i','p','m','a')) {
        ret = AVERROR_INVALIDDATA;
        goto fail;
    }

    version = avio_r8(pb);
    flags   = avio_rb24(pb);
    count   = avio_rb32(pb);

    for (int i = 0; i < count; i++) {
        int item_id = version ? avio_rb32(pb) : avio_rb16(pb);
        int assoc_count = avio_r8(pb);

        for (int j = 0; j < assoc_count; j++) {
            MOVAtoms *ref;
            int index = avio_r8(pb) & 0x7f;
            if (flags & 1) {
                index <<= 8;
                index |= avio_r8(pb);
            }
            if (index > nb_atoms || index <= 0) {
                ret = AVERROR_INVALIDDATA;
                goto fail;
            }
            ref = &atoms[--index];

            av_log(c->fc, AV_LOG_TRACE, "ipma: property_index %d, item_id %d, item_type %s\n",
                   index + 1, item_id, av_fourcc2str(ref->type));

            c->cur_item_id = item_id;

            ret = mov_read_default(c, &ref->b.pub,
                                   (MOVAtom) { .size = ref->size,
                                               .type = MKTAG('i','p','c','o') });
            if (ret < 0)
                goto fail;
            ffio_init_read_context(&ref->b, ref->data, ref->size);
        }
    }

    ret = 0;
fail:
    c->cur_item_id = -1;
    for (int i = 0; i < nb_atoms; i++)
        av_free(atoms[i].data);
    av_free(atoms);

    return ret;
}
