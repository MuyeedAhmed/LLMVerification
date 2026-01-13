Do NOT include any other text.
```c
static int mov_read_sbgp(MOVContext *c, AVIOContext *pb, MOVAtom atom)
{
    AVStream *st;
    MOVStreamContext *sc;
    unsigned int i, entries;
    uint8_t version;
    uint32_t grouping_type;
    MOVSbgp *table, **tablep;
    int *table_count;

    if (c->fc->nb_streams < 1)
        return 0;
    st = c->fc->streams[c->fc->nb_streams-1];
    sc = st->priv_data;

    version = avio_r8(pb); /* version */
    avio_rb24(pb); /* flags */
    grouping_type = avio_rl32(pb);

    if (grouping_type == MKTAG('r','a','p',' ')) {
        tablep = &sc->rap_group;
        table_count = &sc->rap_group_count;
    } else if (grouping_type == MKTAG('s','y','n','c')) {
        tablep = &sc->sync_group;
        table_count = &sc->sync_group_count;
    } else {
        return 0;
    }

    if (version == 1)
        avio_rb32(pb); /* grouping_type_parameter */

    entries = avio_rb32(pb);
    if (!entries)
        return 0;
    if (*tablep)
        av_log(c->fc, AV_LOG_WARNING, "Duplicated SBGP %s atom\n", av_fourcc2str(grouping_type));
    av_freep(tablep);
    table = av_malloc_array(entries, sizeof(*table));
    if (!table)
        return AVERROR(ENOMEM);
    *tablep = table;

    for (i = 0; i < entries && !pb->eof_reached; i++) {
        table[i].count = avio_rb32(pb); /* sample_count */
        table[i].index = avio_rb32(pb); /* group_description_index */
    }

    *table_count = i;

    if (pb->eof_reached) {
        av_log(c->fc, AV_LOG_WARNING, "reached eof, corrupted SBGP atom\n");
        return AVERROR_EOF;
    }

    return 0;
}
```
