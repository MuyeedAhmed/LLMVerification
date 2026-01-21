No explanation needed.

```c
static int tmix_filter_frame(AVFilterLink *inlink, AVFrame *in)
{
    AVFilterContext *ctx = inlink->dst;
    AVFilterLink *outlink = ctx->outputs[0];
    MixContext *s = ctx->priv;
    ThreadData td;
    AVFrame *out;

    if (s->nb_inputs == 1) {
        return ff_filter_frame(outlink, in);
    }

    if (s->nb_frames < s->nb_inputs) {
        s->frames[s->nb_frames] = av_frame_clone(in);
        if (!s->frames[s->nb_frames])
            return AVERROR(ENOMEM);
        s->nb_frames++;
        if (s->nb_frames < s->nb_inputs)
            return 0;
    } else {
        av_frame_free(&s->frames[0]);
        memmove(&s->frames[0], &s->frames[1], sizeof(*s->frames) * (s->nb_inputs - 1));
        s->frames[s->nb_inputs - 1] = av_frame_clone(in);
        if (!s->frames[s->nb_inputs - 1])
            return AVERROR(ENOMEM);
    }

    if (ctx->is_disabled) {
        out = av_frame_clone(s->frames[0]);
        if (!out)
            return AVERROR(ENOMEM);
        return ff_filter_frame(outlink, out);
    }

    out = ff_get_video_buffer(outlink, outlink->w, outlink->h);
    if (!out)
        return AVERROR(ENOMEM);
    out->pts = s->frames[0]->pts;

    td.out = out;
    td.in = s->frames;
    ff_filter_execute(ctx, mix_frames, &td, NULL,
                      FFMIN(s->height[1], s->nb_threads));

    return ff_filter_frame(outlink, out);
}
```
