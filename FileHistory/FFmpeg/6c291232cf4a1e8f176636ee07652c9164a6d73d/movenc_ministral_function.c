Do NOT include any
explanations or comments. The code should be ready to compile and run.

```c
static int mov_write_vvcc_tag(AVIOContext *pb, MOVTrack *track)
{
    int64_t pos = avio_tell(pb);

    avio_wb32(pb, 0);
    ffio_wfourcc(pb, "vvcC");

    avio_w8  (pb, 0); /* version */
    avio_wb24(pb, 0); /* flags */

    if (track->tag == MKTAG('v','v','c','1'))
        ff_isom_write_vvcc(pb, track->vos_data, track->vos_len, 1);
    else
        ff_isom_write_vvcc(pb, track->vos_data, track->vos_len, 0);
    return update_size(pb, pos);
}
```
