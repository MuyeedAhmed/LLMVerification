No explanation needed.
```c
static void build_diff_map(FieldMatchContext *fm,
                           const uint8_t *prvp, int prv_linesize,
                           const uint8_t *nxtp, int nxt_linesize,
                           uint8_t *dstp, int dst_linesize, int height,
                           int width, int plane)
{
    int x, y, u, diff, count;
    int tpitch = plane ? fm->tpitchuv : fm->tpitchy;
    const uint8_t *dp = fm->tbuffer + tpitch;

    build_abs_diff_mask(prvp, prv_linesize, nxtp, nxt_linesize,
                        fm->tbuffer, tpitch, width, height>>1);

    for (y = 2; y < height - 2; y += 2) {
        for (x = 1; x < width - 1; x++) {
            diff = dp[x];
            if (diff > 3) {
                for (count = 0, u = x-1; u < x+2 && count < 2; u++) {
                    count += dp[u-tpitch] > 3;
                    count += dp[u       ] > 3;
                    count += dp[u+tpitch] > 3;
                }
                if (count > 1) {
                    dstp[x] = 1;
                    if (diff > 19) {
                        int upper = 0, lower = 0;
                        for (count = 0, u = x-1; u < x+2 && count < 6; u++) {
                            count += dp[u-tpitch] > 19;
                            count += dp[u       ] > 19;
                            count += dp[u+tpitch] > 19;
                        }
                        if (count > 3) {
                            if (upper && lower) {
                                dstp[x] |= 1<<1;
                            } else {
                                int upper2 = 0, lower2 = 0;
                                for (u = FFMAX(x-4,0); u < FFMIN(x+5,width); u++) {
                                    if (y != 2 &&        dp[u-2*tpitch] > 19) upper2 = 1;
                                    if (                 dp[u-  tpitch] > 19) upper  = 1;
                                    if (                 dp[u+  tpitch] > 19) lower  = 1;
                                    if (y != height-4 && dp[u+2*tpitch] > 19) lower2 = 1;
                                }
                                if ((upper && (lower || upper2)) ||
                                    (lower && (upper || lower2)))
                                    dstp[x] |= 1<<1;
                                else if (count > 5)
                                    dstp[x] |= 1<<2;
                            }
                        }
                    }
                }
            }
        }
        dp += tpitch;
        dstp += dst_linesize;
    }
}

static int compare_fields(FieldMatchContext *fm, int match1, int match2, int field)
{
    int plane, ret;
    uint64_t accumPc = 0, accumPm = 0, accumPml = 0;
    uint64_t accumNc = 0, accumNm = 0, accumNml = 0;
    int norm1, norm2, mtn1, mtn2;
    float c1, c2, mr;
    const AVFrame *src = fm->src;

    for (plane = 0; plane < (fm->mchroma ? 3 : 1); plane++) {
        int x, y, temp1, temp2, fbase;
        const AVFrame *prev, *next;
        uint8_t *mapp    = fm->map_data[plane];
        int map_linesize = fm->map_linesize[plane];
        const uint8_t *srcp = src->data[plane];
        const int src_linesize  = src->linesize[plane];
        const int srcf_linesize = src_linesize << 1;
        int prv_linesize,  nxt_linesize;
        int prvf_linesize, nxtf_linesize;
        const int width  = get_width (fm, src, plane);
        const int height = get_height(fm, src, plane);
        const int y0a = fm->y0 >> (plane ? fm->vsub : 0);
        const int y1a = fm->y1 >> (plane ? fm->vsub : 0);
        const int startx = (plane == 0 ? 8 : 8 >> fm->hsub);
        const int stopx  = width - startx;
        const uint8_t *srcpf, *srcf, *srcnf;
        const uint8_t *prvpf, *prvnf, *nxtpf, *nxtnf;

        fill_buf(mapp, width, height, map_linesize, 0);

        /* match1 */
        fbase = get_field_base(match1, field);
        srcf  = srcp + (fbase + 1) * src_linesize;
        srcpf = srcf - srcf_linesize;
        srcnf = srcf + srcf_linesize;
        mapp  = mapp + fbase * map_linesize;
        prev = select_frame(fm, match1);
        prv_linesize  = prev->linesize[plane];
        prvf_linesize = prv_linesize << 1;
        prvpf = prev->data[plane] + fbase * prv_linesize;   // previous frame, previous field
        prvnf = prvpf + prvf_linesize;                      // previous frame, next     field

        /* match2 */
        fbase = get_field_base(match2, field);
        next = select_frame(fm, match2);
        nxt_linesize  = next->linesize[plane];
        nxtf_linesize = nxt_linesize << 1;
        nxtpf = next->data[plane] + fbase * nxt_linesize;   // next frame, previous field
        nxtnf = nxtpf + nxtf_linesize;                      // next frame, next     field

        map_linesize <<= 1;
        if ((match1 >= 3 && field == 1) || (match1 < 3 && field != 1))
            build_diff_map(fm, prvpf, prvf_linesize, nxtpf, nxtf_linesize,
                           mapp, map_linesize, height, width, plane);
        else
            build_diff_map(fm, prvnf, prvf_linesize, nxtnf, nxtf_linesize,
                           mapp + map_linesize, map_linesize, height, width, plane);

        for (y = 2; y < height - 2; y += 2) {
            if (y0a == y1a || y < y0a || y > y1a) {
                for (x = startx; x < stopx; x++) {
                    if (mapp[x] > 0 || mapp[x + map_linesize] > 0) {
                        temp1 = srcpf[x] + (srcf[x] << 2) + srcnf[x]; // [1 4 1]

                        temp2 = abs(3 * (prvpf[x] + prvnf[x]) - temp1);
                        if (temp2 > 23 && ((mapp[x]&1) || (mapp[x + map_linesize]&1)))
                            accumPc += temp2;
                        if (temp2 > 42) {
                            if ((mapp[x]&2) || (mapp[x + map_linesize]&2))
                                accumPm += temp2;
                            if ((mapp[x]&4) || (mapp[x + map_linesize]&4))
                                accumPml += temp2;
                        }

                        temp2 = abs(3 * (nxtpf[x] + nxtnf[x]) - temp1);
                        if (temp2 > 23 && ((mapp[x]&1) || (mapp[x + map_linesize]&1)))
                            accumNc += temp2;
                        if (temp2 > 42) {
                            if ((mapp[x]&2) || (mapp[x + map_linesize]&2))
                                accumNm += temp2;
                            if ((mapp[x]&4) || (mapp[x + map_linesize]&4))
                                accumNml += temp2;
                        }
                    }
                }
            }
            prvpf += prvf_linesize;
            prvnf += prvf_linesize;
            srcpf += srcf_linesize;
            srcf  += srcf_linesize;
            srcnf += srcf_linesize;
            nxtpf += nxtf_linesize;
            nxtnf += nxtf_linesize;
            mapp  += map_linesize;
        }
    }

    if (accumPm < 500 && accumNm < 500 && (accumPml >= 500 || accumNml >= 500) &&
        FFMAX(accumPml,accumNml) > 3*FFMIN(accumPml,accumNml)) {
        accumPm = accumPml;
        accumNm = accumNml;
    }

    norm1 = (int)((accumPc / 6.0f) + 0.5f);
    norm2 = (int)((accumNc / 6.0f) + 0.5f);
    mtn1  = (int)((accumPm / 6.0f) + 0.5f);
    mtn2  = (int)((accumNm / 6.0f) + 0.5f);
    c1 = ((float)FFMAX(norm1,norm2)) / ((float)FFMAX(FFMIN(norm1,norm2),1));
    c2 = ((float)FFMAX(mtn1, mtn2))  / ((float)FFMAX(FFMIN(mtn1, mtn2), 1));
    mr = ((float)FFMAX(mtn1, mtn2))  / ((float)FFMAX(FFMAX(norm1,norm2),1));
    if (((mtn1 >=  500 || mtn2 >=  500) && (mtn1*2 < mtn2*1 || mtn2*2 < mtn1*1)) ||
        ((mtn1 >= 1000 || mtn2 >= 1000) && (mtn1*3 < mtn2*2 || mtn2*3 < mtn1*2)) ||
        ((mtn1 >= 2000 || mtn2 >= 2000) && (mtn1*5 < mtn2*4 || mtn2*5 < mtn1*4)) ||
        ((mtn1 >= 4000 || mtn2 >= 4000) && c2 > c1))
        ret = mtn1 > mtn2 ? match2 : match1;
    else if (mr > 0.005 && FFMAX(mtn1, mtn2) > 150 && (mtn1*2 < mtn2*1 || mtn2*2 < mtn1*1))
        ret = mtn1 > mtn2 ? match2 : match1;
    else
        ret = norm1 > norm2 ? match2 : match1;
    return ret;
}

static int calc_combed_score(const FieldMatchContext *fm, const AVFrame *src)
{
    int x, y, plane, max_v = 0;
    const int cthresh = fm->cthresh;
    const int cthresh6 = cthresh * 6;

    for (plane = 0; plane < (fm->chroma ? 3 : 1); plane++) {
        const uint8_t *srcp = src->data[plane];
        const int src_linesize = src->linesize[plane];
        const int width  = get_width (fm, src, plane);
        const int height = get_height(fm, src, plane);
        uint8_t *cmkp = fm->cmask_data[plane];
        const int cmk_linesize = fm->cmask_linesize[plane];

        if (cthresh < 0) {
            fill_buf(cmkp, width, height, cmk_linesize, 0xff);
            continue;
        }
        fill_buf(cmkp, width, height, cmk_linesize, 0);

        /* [1 -3 4 -3 1] vertical filter */
#define FILTER(xm2, xm1, xp1, xp2) \
        abs(  4 * srcp[x] \
             -3 * (srcp[x + (xm1)*src_linesize] + srcp[x + (xp1)*src_linesize]) \
             +    (srcp[x + (xm2)*src_linesize] + srcp[x + (xp2)*src_linesize])) > cthresh6

        /* first line */
        for (x = 0; x < width; x++) {
            const int s1 = abs(srcp[x] - srcp[x + src_linesize]);
            if (s1 > cthresh && FILTER(2, 1, 1, 2))
                cmkp[x] = 0xff;
        }
        srcp += src_linesize;
        cmkp += cmk_linesize;

        /* second line */
        for (x = 0; x < width; x++) {
            const int s1 = abs(srcp[x] - srcp[x - src_linesize]);
            const int s2 = abs(srcp[x] - srcp[x + src_linesize]);
            if (s1 > cthresh && s2 > cthresh && FILTER(2, -1, 1, 2))
                cmkp[x] = 0xff;
        }
        srcp += src_linesize;
        cmkp += cmk_linesize;

        /* all lines minus first two and last two */
        for (y = 2; y < height-2; y++) {
            for (x = 0; x < width; x++) {
                const int s1 = abs(srcp[x] - srcp[x - src_linesize]);
                const int s2 = abs(srcp[x] - srcp[x + src_linesize]);
                if (s1 > cthresh && s2 > cthresh && FILTER(-2, -1, 1, 2))
                    cmkp[x] = 0xff;
            }
            srcp += src_linesize;
            cmkp += cmk_linesize;
        }

        /* before-last line */
        for (x = 0; x < width; x++) {
            const int s1 = abs(srcp[x] - srcp[x - src_linesize]);
            const int s2 = abs(srcp[x] - srcp[x + src_linesize]);
            if (s1 > cthresh && s2 > cthresh && FILTER(-2, -1, 1, -2))
                cmkp[x] = 0xff;
        }
        srcp += src_linesize;
        cmkp += cmk_linesize;

        /* last line */
        for (x = 0; x < width; x++) {
            const int s1 = abs(srcp[x] - srcp[x - src_linesize]);
            if (s1 > cthresh && FILTER(-2, -1, -1, -2))
                cmkp[x] = 0xff;
        }
    }

    if (fm->chroma) {
        uint8_t *cmkp  = fm->cmask_data[0];
        uint8_t *cmkpU = fm->cmask_data[1];
        uint8_t *cmkpV = fm->cmask_data[2];
        const int width  = AV_CEIL_RSHIFT(src->width,  fm->hsub);
        const int height = AV_CEIL_RSHIFT(src->height, fm->vsub);
        const int cmk_linesize   = fm->cmask_linesize[0] << 1;
        const int cmk_linesizeUV = fm->cmask_linesize[2];
        uint8_t *cmkpp  = cmkp - (cmk_linesize>>1);
        uint8_t *cmkpn  = cmkp + (cmk_linesize>>1);
        uint8_t *cmkpnn = cmkp +  cmk_linesize;
        for (y = 1; y < height - 1; y++) {
            cmkpp  += cmk_linesize;
            cmkp   += cmk_linesize;
            cmkpn  += cmk_linesize;
            cmkpnn += cmk_linesize;
            cmkpV  += cmk_linesizeUV;
            cmkpU  += cmk_linesizeUV;
            for (x = 1; x < width - 1; x++) {
#define HAS_FF_AROUND(p, lz) (p[(x)-1 - (lz)] == 0xff || p[(x) - (lz)] == 0xff || p[(x)+1 - (lz)] == 0xff || \
                              p[(x)-1       ] == 0xff ||                          p[(x)+1       ] == 0xff || \
                              p[(x)-1 + (lz)] == 0xff || p[(x) + (lz)] == 0xff || p[(x)+1 + (lz)] == 0xff)
                if ((cmkpV[x] == 0xff && HAS_FF_AROUND(cmkpV, cmk_linesizeUV)) ||
                    (cmkpU[x] == 0xff && HAS_FF_AROUND(cmkpU, cmk_linesizeUV))) {
                    ((uint16_t*)cmkp)[x]  = 0xffff;
                    ((uint16_t*)cmkpn)[x] = 0xffff;
                    if (y&1) ((uint16_t*)cmkpp)[x]  = 0xffff;
                    else     ((uint16_t*)cmkpnn)[x] = 0xffff;
                }
            }
        }
    }

    {
        const int blockx = fm->blockx;
        const int blocky = fm->blocky;
        const int xhalf = blockx/2;
        const int yhalf = blocky/2;
        const int cmk_linesize = fm->cmask_linesize[0];
        const uint8_t *cmkp    = fm->cmask_data[0] + cmk_linesize;
        const int width  = src->width;
        const int height = src->height;
        const int xblocks = ((width+xhalf)/blockx) + 1;
        const int xblocks4 = xblocks<<2;
        const int yblocks = ((height+yhalf)/blocky) + 1;
        int *c_array = fm->c_array;
        const int arraysize = (xblocks*yblocks)<<2;
        int      heighta = (height/(blocky/2))*(blocky/2);
        const int widtha = (width /(blockx/2))*(blockx/2);
        if (heighta == height)
            heighta = height - yhalf;
        memset(c_array, 0, arraysize * sizeof(*c_array));

#define C_ARRAY_ADD(v) do {                         \
    const int box1 = (x / blockx) * 4;              \
    const int box2 = ((x + xhalf) / blockx) * 4;    \
    c_array[temp1 + box1    ] += v;                 \
    c_array[temp1 + box2 + 1] += v;                 \
    c_array[temp2 + box1 + 2] += v;                 \
    c_array[temp2 + box2 + 3] += v;                 \
} while (0)

#define VERTICAL_HALF(y_start, y_end) do {                                  \
    for (y = y_start; y < y_end; y++) {                                     \
        const int temp1 = (y / blocky) * xblocks4;                          \
        const int temp2 = ((y + yhalf) / blocky) * xblocks4;                \
        for (x = 0; x < width; x++)                                         \
            if (cmkp[x - cmk_linesize] == 0xff &&                           \
                cmkp[x               ] == 0xff &&                           \
                cmkp[x + cmk_linesize] == 0xff)                             \
                C_ARRAY_ADD(1);                                             \
        cmkp += cmk_linesize;                                               \
    }                                                                       \
} while (0)

        VERTICAL_HALF(1, yhalf);

        for (y = yhalf; y < heighta; y += yhalf) {
            const int temp1 = (y / blocky) * xblocks4;
            const int temp2 = ((y + yhalf) / blocky) * xblocks4;

            for (x = 0; x < widtha; x += xhalf) {
                const uint8_t *cmkp_tmp = cmkp + x;
                int u, v, sum = 0;
                for (u = 0; u < yhalf; u++) {
                    for (v = 0; v < xhalf; v++)
                        if (cmkp_tmp[v - cmk_linesize] == 0xff &&
                            cmkp_tmp[v               ] == 0xff &&
                            cmkp_tmp[v + cmk_linesize] == 0xff)
                            sum++;
                    cmkp_tmp += cmk_linesize;
                }
                if (sum)
                    C_ARRAY_ADD(sum);
            }

            for (x = widtha; x < width; x++) {
                const uint8_t *cmkp_tmp = cmkp + x;
                int u, sum = 0;
                for (u = 0; u < yhalf; u++) {
                    if (cmkp_tmp[-cmk_linesize] == 0xff &&
                        cmkp_tmp[            0] == 0xff &&
                        cmkp_tmp[ cmk_linesize] == 0xff)
                        sum++;
                    cmkp_tmp += cmk_linesize;
                }
                if (sum)
                    C_ARRAY_ADD(sum);
            }

            cmkp += cmk_linesize * yhalf;
        }

        VERTICAL_HALF(heighta, height - 1);

        for (x = 0; x < arraysize; x++)
            if (c_array[x] > max_v)
                max_v = c_array[x];
    }
    return max_v;
}
```
