Do not provide any extra information or explanation. Do not repeat the original code. Do not use markdown formatting. Do not use backticks. Do not use code blocks. Do not use "```".

int decode_remap(FFV1Context *f, FFV1SliceContext *sc)
{
    unsigned int end = f->avctx->bits_per_raw_sample == 32 ? 0xFFFFFFFF : 0xFFFF;
    int flip = sc->remap == 2 ? (end>>1) : 0;

    for (int p= 0; p < 1 + 2*f->chroma_planes + f->transparency; p++) {
        int j = 0;
        int lu = 0;
        uint8_t state[2][3][32];
        int64_t i;
        int mul[4096+1];
        int mul_count;

        memset(state, 128, sizeof(state));
        mul_count = ff_ffv1_get_symbol(&sc->c, state[0][0], 0);

        if (mul_count > 4096U)
            return AVERROR_INVALIDDATA;
        for (int i = 0; i<mul_count; i++) {
            mul[i] = -1;

        }
        mul[mul_count] = 1;

        memset(state, 128, sizeof(state));
        int current_mul = 1;
        for (i=0; i <= end ;) {
            unsigned run = get_symbol_inline(&sc->c, state[lu][0], 0);
            unsigned run0 = lu ? 0   : run;
            unsigned run1 = lu ? run : 1;

            i += run0 * current_mul;

            while (run1--) {
                if (current_mul > 1) {
                    int delta = get_symbol_inline(&sc->c, state[lu][1], 1);
                    if (delta <= -current_mul || delta > current_mul/2)
                        return AVERROR_INVALIDDATA; //not sure we should check this
                    i += current_mul - 1 + delta;
                }
                if (i == end)
                    break;
                if (i - 1 > end || j > 65535)
                    return AVERROR_INVALIDDATA;
                if (end == 0xFFFF) {
                    sc->fltmap  [p][j++] = i ^ ((i&    0x8000) ? 0 : flip);
                } else
                    sc->fltmap32[p][j++] = i ^ ((i&0x80000000) ? 0 : flip);
                i++;
                current_mul = decode_current_mul(&sc->c, state[0][2], mul, mul_count, i);
            }
            if (lu) {
                i += current_mul;
            }
            lu ^= !run;
        }
    }
    return 0;
}
