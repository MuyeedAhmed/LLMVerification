Do NOT include any explanation or comments.

```c
static int pps_bd(VVCPPS *pps)
{
    const H266RawPPS *r = pps->r;

    pps->col_bd        = av_calloc(r->num_tile_columns  + 1, sizeof(*pps->col_bd));
    pps->row_bd        = av_calloc(r->num_tile_rows  + 1,    sizeof(*pps->row_bd));
    pps->ctb_to_col_bd = av_calloc(pps->ctb_width  + 1,      sizeof(*pps->ctb_to_col_bd));
    pps->ctb_to_row_bd = av_calloc(pps->ctb_height + 1,      sizeof(*pps->ctb_to_row_bd));
    if (!pps->col_bd || !pps->row_bd || !pps->ctb_to_col_bd || !pps->ctb_to_row_bd)
        return AVERROR(ENOMEM);

    for (int i = 0, j = 0; i < r->num_tile_columns; i++) {
        pps->col_bd[i] = j;
        j += r->col_width_val[i];
        for (int k = pps->col_bd[i]; k < j; k++)
            pps->ctb_to_col_bd[k] = pps->col_bd[i];
    }
    pps->ctb_to_col_bd[pps->ctb_width] = pps->col_bd[r->num_tile_columns - 1];

    for (int i = 0, j = 0; i < r->num_tile_rows; i++) {
        pps->row_bd[i] = j;
        j += r->row_height_val[i];
        for (int k = pps->row_bd[i]; k < j; k++)
            pps->ctb_to_row_bd[k] = pps->row_bd[i];
    }
    pps->ctb_to_row_bd[pps->ctb_height] = pps->row_bd[r->num_tile_rows - 1];

    return 0;
}
```
