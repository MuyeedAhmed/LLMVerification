Do NOT include any additional text or explanation.

```c
static av_always_inline int setup_classifs(vorbis_context *vc,
                                           vorbis_residue *vr,
                                           uint8_t *do_not_decode,
                                           unsigned ch_used,
                                           int partition_count)
{
    vorbis_codebook *codebook = vc->codebooks + vr->classbook;
    int p, j, i;
    unsigned c_p_c         = codebook->dimensions;
    unsigned inverse_class = ff_inverse[vr->classifications];
    unsigned temp, temp2;
    for (p = 0, j = 0; j < ch_used; ++j) {
        if (!do_not_decode[j]) {
            temp = get_vlc2(&vc->gb, codebook->vlc.table,
                                     codebook->nb_bits, 3);

            av_dlog(NULL, "Classword: %u\n", temp);

            if (temp <= 65536) {
                for (i = partition_count + c_p_c - 1; i >= partition_count; i--) {
                    temp2 = (((uint64_t)temp) * inverse_class) >> 32;

                    if (i < vr->ptns_to_read)
                        vr->classifs[p + i] = temp - temp2 * vr->classifications;
                    temp = temp2;
                }
            } else {
                for (i = partition_count + c_p_c - 1; i >= partition_count; i--) {
                    temp2 = temp / vr->classifications;

                    if (i < vr->ptns_to_read)
                        vr->classifs[p + i] = temp - temp2 * vr->classifications;
                    temp = temp2;
                }
            }
        }
        p += vr->ptns_to_read;
    }
    return 0;
}
```
