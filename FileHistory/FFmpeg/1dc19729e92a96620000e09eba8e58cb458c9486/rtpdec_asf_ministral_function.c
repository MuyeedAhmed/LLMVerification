No explanation needed.
```c
static void asfrtp_close_context(PayloadContext *asf)
{
    ffio_free_dyn_buf(&asf->pktbuf);
    // Don't free the payload context's buf here
    av_free(asf);
}
```
