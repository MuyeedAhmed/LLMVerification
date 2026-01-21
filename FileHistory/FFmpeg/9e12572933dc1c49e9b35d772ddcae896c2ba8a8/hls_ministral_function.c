Do not include any additional text or explanations.
```c
static int test_segment(AVFormatContext *s, const AVInputFormat *in_fmt, struct playlist *pls, struct segment *seg)
{
    HLSContext *c = s->priv_data;
    int matchA = 3;
    int matchF = 0;

    if (!c->extension_picky)
        return 0;

    if (strcmp(c->allowed_extensions, "ALL"))
        matchA =      av_match_ext(seg->url, c->allowed_extensions)
                 + 2*(ff_match_url_ext(seg->url, c->allowed_extensions) > 0);

    if (!matchA) {
        av_log(s, AV_LOG_ERROR, "URL %s is not in allowed_extensions\n", seg->url);
        return AVERROR_INVALIDDATA;
    }

    if (in_fmt) {
        if (in_fmt->extensions) {
            matchF =      av_match_ext(seg->url, in_fmt->extensions)
                     + 2*(ff_match_url_ext(seg->url, in_fmt->extensions) > 0);
        } else if (!strcmp(in_fmt->name, "mpegts")) {
            matchF = 3;
        } else if (!strcmp(in_fmt->name, "mov") || !strcmp(in_fmt->name, "mp4")) {
            matchF = 3;
        }

        if (!(matchA & matchF)) {
            av_log(s, AV_LOG_ERROR, "detected format %s extension %s mismatches allowed extensions in url %s\n", in_fmt->name, in_fmt->extensions ? in_fmt->extensions : "none", seg->url);
            return AVERROR_INVALIDDATA;
        }
    }

    return 0;
}
```
