Do not include any additional text or explanations.

```c
static void encode_histogram_remap(FFV1Context *f, FFV1SliceContext *sc)
{
    int len = 1 << f->bits_per_raw_sample;
    int flip = sc->remap == 2 ? 0x7FFF : 0;

    for (int p= 0; p < 1 + 2*f->chroma_planes + f->transparency; p++) {
        int j = 0;
        int lu = 0;
        uint8_t state[2][32];
        int run = 0;

        memset(state, 128, sizeof(state));
        put_symbol(&sc->c, state[0], 0, 0);
        memset(state, 128, sizeof(state));
        for (int i= 0; i<len; i++) {
            int ri = i ^ ((i&0x8000) ? 0 : flip);
            int u = sc->fltmap[p][ri];
            sc->fltmap[p][ri] = j;
            j+= u;

            if (lu == u) {
                run ++;
            } else {
                put_symbol_inline(&sc->c, state[lu], run, 0, NULL, NULL);
                if (run == 0)
                    lu = u;
                run = 0;
            }
        }
        if (run)
            put_symbol(&sc->c, state[lu], run, 0);
        sc->remap_count[p] = j;
    }
}
```
