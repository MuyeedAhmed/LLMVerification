Do NOT include any explanation or comments.

```c
static void max_filter_length_chroma(const VVCFrameContext *fc, const int qx, const int qy,
                                     const int vertical, const int horizontal_ctu_edge, const int bs, uint8_t *max_len_p, uint8_t *max_len_q)
{
    const int px =  vertical ? qx - 1 : qx;
    const int py = !vertical ? qy - 1 : qy;
    const uint8_t *tb_size = vertical ? fc->tab.tb_width[CHROMA] : fc->tab.tb_height[CHROMA];

    const int size_p = tb_size[(py >> MIN_TU_LOG2) * fc->ps.pps->min_tu_width + (px >> MIN_TU_LOG2)];
    const int size_q = tb_size[(qy >> MIN_TU_LOG2) * fc->ps.pps->min_tu_width + (qx >> MIN_TU_LOG2)];
    if (size_p >= 8 && size_q >= 8) {
        *max_len_p = *max_len_q = 3;
        if (horizontal_ctu_edge)
            *max_len_p = 1;
    } else {
        //part of 8.8.3.6.4 Decision process for chroma block edges
        *max_len_p = *max_len_q = (bs == 2);
    }
}
```
