
        Modify the function in the provided C file such that it fixes the following issue: avformat/mov: fix the check for the heif item parsing loop in mov_parse_tiles.
        Please return only the updated definition of the function 'mov_parse_tiles' as a single C function. Do not include the full source file or any extra commentary.
        
```c
int mov_parse_tiles(AVFormatContext *s)
{
    MOVContext *mov = s->priv_data;

    for (int i = 0; i < mov->nb_heif_grid; i++) {
        AVStreamGroup *stg = avformat_stream_group_create(s, AV_STREAM_GROUP_PARAMS_TILE_GRID, NULL);
        AVStreamGroupTileGrid *tile_grid;
        const HEIFGrid *grid = &mov->heif_grid[i];
        int err;

        if (!stg)
            return AVERROR(ENOMEM);

        stg->id = grid->item->item_id;
        tile_grid = stg->params.tile_grid;

        for (int j = 0; j < grid->nb_tiles; j++) {
            int tile_id = grid->tile_id_list[j];
            int k;

            for (k = 0; k < mov->nb_heif_item; k++) {
                HEIFItem *item = &mov->heif_item[k];
                AVStream *st = item->st;

                if (item->item_id != tile_id)
                    continue;
                if (!st) {
                    av_log(s, AV_LOG_WARNING, "HEIF item id %d from grid id %d doesn\'t "
                                              "reference a stream\n",
                           tile_id, grid->item->item_id);
                    ff_remove_stream_group(s, stg);
                    return AVERROR_INVALIDDATA;
                }

                avformat_stream_group_add_stream(s, stg, st);
                tile_grid->tile[k].x_offset = item->x_offset;
                tile_grid->tile[k].y_offset = item->y_offset;
                tile_grid->tile[k].width = st->codecpar->width;
                tile_grid->tile[k].height = st->codecpar->height;
                break;
            }
        }
        tile_grid->columns = grid->columns;
        tile_grid->rows    = grid->rows;
    }
    return 0;
}
```
