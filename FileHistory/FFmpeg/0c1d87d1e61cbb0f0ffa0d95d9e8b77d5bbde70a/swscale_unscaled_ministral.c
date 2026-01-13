Do NOT include any additional text or explanation.
```c
static void packed30togbra10(const uint8_t *src, int srcStride,
                             uint16_t *dst[], const int dstStride[], int srcSliceH,
                             int swap, int bpc, int width)
{
    int x, h, i;
    int dst_alpha = dst[3] != NULL;
    int scale_high = bpc - 10, scale_low = 10 - scale_high;
    for (h = 0; h < srcSliceH; h++) {
        uint32_t *src_line = (uint32_t *)(src + srcStride * h);
        unsigned component;

        switch (swap) {
        case 3:
        case 2:
            if (dst_alpha) {
                for (x = 0; x < width; x++) {
                    unsigned p = AV_RL32(src_line);
                    component = (p >> 20) & 0x3FF;
                    dst[0][x] = av_bswap16(component << scale_high | component >> scale_low);
                    component = (p >> 10) & 0x3FF;
                    dst[1][x] = av_bswap16(component << scale_high | component >> scale_low);
                    component =  p        & 0x3FF;
                    dst[2][x] = av_bswap16(component << scale_high | component >> scale_low);
                    dst[3][x] = 0xFFFF;
                    src_line++;
                }
            } else {
                for (x = 0; x < width; x++) {
                    unsigned p = AV_RL32(src_line);
                    component = (p >> 20) & 0x3FF;
                    dst[0][x] = av_bswap16(component << scale_high | component >> scale_low);
                    component = (p >> 10) & 0x3FF;
                    dst[1][x] = av_bswap16(component << scale_high | component >> scale_low);
                    component =  p        & 0x3FF;
                    dst[2][x] = av_bswap16(component << scale_high | component >> scale_low);
                    src_line++;
                }
            }
            break;
        default:
            if (dst_alpha) {
                for (x = 0; x < width; x++) {
                    unsigned p = AV_RL32(src_line);
                    component = (p >> 20) & 0x3FF;
                    dst[0][x] = component << scale_high | component >> scale_low;
                    component = (p >> 10) & 0x3FF;
                    dst[1][x] = component << scale_high | component >> scale_low;
                    component =  p        & 0x3FF;
                    dst[2][x] = component << scale_high | component >> scale_low;
                    dst[3][x] = 0xFFFF;
                    src_line++;
                }
            } else {
                for (x = 0; x < width; x++) {
                    unsigned p = AV_RL32(src_line);
                    component = (p >> 20) & 0x3FF;
                    dst[0][x] = component << scale_high | component >> scale_low;
                    component = (p >> 10) & 0x3FF;
                    dst[1][x] = component << scale_high | component >> scale_low;
                    component =  p        & 0x3FF;
                    dst[2][x] = component << scale_high | component >> scale_low;
                    src_line++;
                }
            }
            break;
        }
        for (i = 0; i < 4; i++)
            dst[i] += dstStride[i] >> 1;
    }
}
```
