Do NOT include any other text.
```c
void avformat_close_input(AVFormatContext **ps)
{
    AVFormatContext *s;
    AVIOContext *pb;

    if (!ps || !*ps)
        return;

    s  = *ps;
    pb = s->pb;

    if ((s->iformat && strcmp(s->iformat->name, "image2") && s->iformat->flags & AVFMT_NOFILE) ||
        (s->flags & AVFMT_FLAG_CUSTOM_IO))
        pb = NULL;

    if (s->iformat)
        if (ffifmt(s->iformat)->read_close)
            ffifmt(s->iformat)->read_close(s);

    avformat_free_context(s);

    *ps = NULL;

    if (pb)
        io_close2(pb);
}
```
