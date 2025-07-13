
        Modify the function in the provided C file such that it fixes the following issue: avformat/mov: don't read key_size bytes twice in the keys atom in mov_read_keys.
        Please return only the updated definition of the function 'mov_read_keys' as a single C function. Do not include the full source file or any extra commentary.
        
```c
static int mov_read_keys(MOVContext *c, AVIOContext *pb, MOVAtom atom)
{
    unsigned int count, i;
    unsigned int key_size;

    avio_rb32(pb);
    count = avio_rb32(pb);
    if (count >= UINT_MAX / 12 || count * 12LL >= atom.size)
        return AVERROR_INVALIDDATA;
    for (i = 0; i < count; i++) {
        avio_rb32(pb); /* key_namespace */
        avio_rb32(pb); /* key_value */
        key_size = avio_rb32(pb);
        if (key_size >= UINT_MAX - 8)
            return AVERROR_INVALIDDATA;
        if (key_size > atom.size - avio_tell(pb)) // Check boundaries before seeking
            return AVERROR_INVALIDDATA;
        avio_skip(pb, key_size); /* key_string */
    }
    return 0;
}
```
