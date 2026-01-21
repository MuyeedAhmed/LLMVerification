No explanation needed.

```c
void ff_vvc_unref_frame(VVCFrameContext *fc, VVCFrame *frame, int flags)
{
    /* frame->frame can be NULL if context init failed */
    if (!frame->frame || !frame->frame->buf[0])
        return;

    frame->flags &= ~flags;
    if (!(frame->flags & ~VVC_FRAME_FLAG_CORRUPT))
        frame->flags = 0;
    if (!frame->flags) {
        av_frame_unref(frame->frame);

        if (frame->needs_fg) {
            av_frame_unref(frame->frame_grain);
            frame->needs_fg = 0;
        }

        av_refstruct_unref(&frame->sps);
        av_refstruct_unref(&frame->pps);
        av_refstruct_unref(&frame->progress);

        av_refstruct_unref(&frame->tab_dmvr_mvf);

        av_refstruct_unref(&frame->rpl);
        frame->nb_rpl_elems = 0;
        av_refstruct_unref(&frame->rpl_tab);

        frame->collocated_ref = NULL;
        av_refstruct_unref(&frame->hwaccel_picture_private);

        // Remove film grain parameters from side data
        if (frame->frame->side_data) {
            for (int i = 0; i < frame->frame->nb_side_data; i++) {
                if (frame->frame->side_data[i]->type == AV_PKT_DATA_FILM_GRAIN) {
                    av_freep(&frame->frame->side_data[i]);
                    frame->frame->nb_side_data--;
                    i--; // Adjust index after removal
                }
            }
        }
    }
}
```
