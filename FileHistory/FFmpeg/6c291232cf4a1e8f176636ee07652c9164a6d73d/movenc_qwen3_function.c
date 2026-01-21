Do not include anything else. Do not include the original or intermediate versions. Do not include any explanations. Do not include any markdown formatting. Do not include the "-----" line separators. Do not include the "Return ONLY the final C source code as your answer." line. Do not include any other text.

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
