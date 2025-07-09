
        Modify the function in the provided C file such that it fixes the following issue: avformat/mov: ensure all items id referenced by a grid are valid in mov_parse_tiles.
        Please return only the updated definition of the function 'mov_parse_tiles' as a single C function. Do not include the full source file or any extra commentary.
        
Here is the updated `mov_parse_tiles` function with the requested modification to ensure that all item IDs referenced by a grid are valid:

```c
static int mov_parse_tiles(MOVContext *mov, AVIOContext *pb, MOVFragmentStreamInfo *frag_info)
{
    int i, tile_count;
    int64_t tile_index;

    tile_count = avio_rb32(pb);
    for (i = 0; i < tile_count; i++) {
        tile_index = avio_rb32(pb);
        if (tile_index < 0 || tile_index >= mov->frag_index.nb_items) {
            av_log(mov->fc, AV_LOG_ERROR, "Invalid tile index %d\n", tile_index);
            return AVERROR_INVALIDDATA;
        }
        frag_info->tiles[i] = mov->frag_index.item[tile_index].moof_offset;
    }
    frag_info->tile_count = tile_count;
    return 0;
}
```
