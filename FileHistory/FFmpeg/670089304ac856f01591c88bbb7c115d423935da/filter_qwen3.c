Do not provide any explanations. Do not repeat the original code unless required.

----- BEGIN SOLUTION -----
#include "libavutil/frame.h"
#include "libavutil/imgutils.h"

#include "ctu.h"
#include "data.h"
#include "filter.h"
#include "refs.h"

#define LEFT        0
#define TOP         1
#define RIGHT       2
#define BOTTOM      3
#define MAX_EDGES   4

#define DEFAULT_INTRA_TC_OFFSET 2

#define POS(c_idx, x, y)                                                                        \
    &fc->frame->data[c_idx][((y) >> fc->ps.sps->vshift[c_idx]) * fc->frame->linesize[c_idx] +   \
        (((x) >> fc->ps.sps->hshift[c_idx]) << fc->ps.sps->pixel_shift)]

//Table 43 Derivation of threshold variables beta' and tc' from input Q
static const uint16_t tctable[66] = {
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   3,   4,   4,   4,   4,   5,   5,   5,   5,   7,   7,   8,   9,  10,
     10,  11,  13,  14,  15,  17,  19,  21,  24,  25,  29,  33,  36,  41,  45,  51,
     57,  64,  71,  80,  89, 100, 112, 125, 141, 157, 177, 198, 222, 250, 280, 314,
    352, 395,
};

//Table 43 Derivation of threshold variables beta' and tc' from input Q
static const uint8_t betatable[64] = {
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      6,   7,   8,   9,  10,  11,  12,  13,  14,  15,  16,  17,  18,  20,  22,  24,
     26,  28,  30,  32,  34,  36,  38,  40,  42,  44,  46,  48,  50,  52,  54,  56,
     58,  60,  62,  64,  66,  68,  70,  72,  74,  76,  78,  80,  82,  84,  86,  88,
};

// One vertical and one horizontal virtual boundary in a CTU at most. The CTU will be divided into 4 subblocks.
#define MAX_VBBS 4

static int get_virtual_boundary(const VVCFrameContext *fc, const int ctu_pos, const int vertical)
{
    const VVCSPS *sps    = fc->ps.sps;
    const VVCPH *ph      = &fc->ps.ph;
    const uint16_t *vbs  = vertical ? ph->vb_pos_x    : ph->vb_pos_y;
    const uint8_t nb_vbs = vertical ? ph->num_ver_vbs : ph->num_hor_vbs;
    const int pos        = ctu_pos << sps->ctb_log2_size_y;

    if (sps->r->sps_virtual_boundaries_enabled_flag) {
        for (int i = 0; i < nb_vbs; i++) {
            const int o = vbs[i] - pos;
            if (o >= 0 && o < sps->ctb_size_y)
                return vbs[i];
        }
    }
    return 0;
}

static int is_virtual_boundary(const VVCFrameContext *fc, const int pos, const int vertical)
{
    return get_virtual_boundary(fc, pos >> fc->ps.sps->ctb_log2_size_y, vertical) == pos;
}

static int get_qPc(const VVCFrameContext *fc, const int x0, const int y0, const int chroma)
{
    const int x            = x0 >> MIN_TU_LOG2;
    const int y            = y0 >> MIN_TU_LOG2;
    const int min_tu_width = fc->ps.pps->min_tu_width;
    return fc->tab.qp[chroma][x + y * min_tu_width];
}

static void copy_ctb(uint8_t *dst, const uint8_t *src, const int width, const int height,
    const ptrdiff_t dst_stride, const ptrdiff_t src_stride)
{
    for (int y = 0; y < height; y++) {
        memcpy(dst, src, width);

        dst += dst_stride;
        src += src_stride;
    }
}

static void copy_pixel(uint8_t *dst, const uint8_t *src, const int pixel_shift)
{
    if (pixel_shift)
        *(uint16_t *)dst = *(uint16_t *)src;
    else
        *dst = *src;
}

static void copy_vert(uint8_t *dst, const uint8_t *src, const int pixel_shift, const int height,
    const ptrdiff_t dst_stride, const ptrdiff_t src_stride)
{
    int i;
    if (pixel_shift == 0) {
        for (i = 0; i < height; i++) {
            *dst = *src;
            dst += dst_stride;
            src += src_stride;
        }
    } else {
        for (i = 0; i < height; i++) {
            *(uint16_t *)dst = *(uint16_t *)src;
            dst += dst_stride;
            src += src_stride;
        }
    }
}

static void copy_ctb_to_hv(VVCFrameContext *fc, const uint8_t *src,
    const ptrdiff_t src_stride, const int x, const int y, const int width, const int height,
    const int c_idx, const int rx, const int ry, const int top)
{
    const int ps = fc->ps.sps->pixel_shift;
    const int w  = fc->ps.pps->width >> fc->ps.sps->hshift[c_idx];
    const int h  = fc->ps.pps->height >> fc->ps.sps->vshift[c_idx];

    if (top) {
        /* top */
        memcpy(fc->tab.sao_pixel_buffer_h[c_idx] + (((2 * ry) * w + x) << ps),
            src, width << ps);
    } else {
        /* bottom */
        memcpy(fc->tab.sao_pixel_buffer_h[c_idx] + (((2 * ry + 1) * w + x) << ps),
            src + src_stride * (height - 1), width << ps);

        /* copy vertical edges */
        copy_vert(fc->tab.sao_pixel_buffer_v[c_idx] + (((2 * rx) * h + y) << ps), src, ps, height, 1 << ps, src_stride);
        copy_vert(fc->tab.sao_pixel_buffer_v[c_idx] + (((2 * rx + 1) * h + y) << ps), src + ((width - 1) << ps), ps, height, 1 << ps, src_stride);
    }
}

static void sao_copy_ctb_to_hv(VVCLocalContext *lc, const int rx, const int ry, const int top)
{
    VVCFrameContext *fc  = lc->fc;
    const int ctb_size_y = fc->ps.sps->ctb_size_y;
    const int x0         = rx << fc->ps.sps->ctb_log2_size_y;
    const int y0         = ry << fc->ps.sps->ctb_log2_size_y;

    for (int c_idx = 0; c_idx < (fc->ps.sps->r->sps_chroma_format_idc ? 3 : 1); c_idx++) {
        const int x                = x0 >> fc->ps.sps->hshift[c_idx];
        const int y                = y0 >> fc->ps.sps->vshift[c_idx];
        const ptrdiff_t src_stride = fc->frame->linesize[c_idx];
        const int ctb_size_h       = ctb_size_y >> fc->ps.sps->hshift[c_idx];
        const int ctb_size_v       = ctb_size_y >> fc->ps.sps->vshift[c_idx];
        const int width            = FFMIN(ctb_size_h, (fc->ps.pps->width  >> fc->ps.sps->hshift[c_idx]) - x);
        const int height           = FFMIN(ctb_size_v, (fc->ps.pps->height >> fc->ps.sps->vshift[c_idx]) - y);
        const uint8_t *src         = POS(c_idx, x0, y0);
        copy_ctb_to_hv(fc, src, src_stride, x, y, width, height, c_idx, rx, ry, top);
    }
}

void ff_vvc_sao_copy_ctb_to_hv(VVCLocalContext *lc, const int rx, const int ry, const int last_row)
{
    if (ry)
        sao_copy_ctb_to_hv(lc, rx, ry - 1, 0);

    sao_copy_ctb_to_hv(lc, rx, ry, 1);

    if (last_row)
        sao_copy_ctb_to_hv(lc, rx, ry, 0);
}

static int sao_can_cross_slices(const VVCFrameContext *fc, const int rx, const int ry, const int dx, const int dy)
{
    const uint8_t lfase = fc->ps.pps->r->pps_loop_filter_across_slices_enabled_flag;

    return lfase || CTB(fc->tab.slice_idx, rx, ry) == CTB(fc->tab.slice_idx, rx + dx, ry + dy);
}

static void sao_get_edges(uint8_t vert_edge[2], uint8_t horiz_edge[2], uint8_t diag_edge[4], int *restore,
    const VVCLocalContext *lc, const int edges[4], const int rx, const int ry)
{
    const VVCFrameContext *fc      = lc->fc;
    const VVCSPS *sps              = fc->ps.sps;
    const H266RawSPS *rsps         = sps->r;
    const VVCPPS *pps              = fc->ps.pps;
    const int subpic_idx           = lc->sc->sh.r->curr_subpic_idx;
    const uint8_t lfase            = fc->ps.pps->r->pps_loop_filter_across_slices_enabled_flag;
    const uint8_t no_tile_filter   = pps->r->num_tiles_in_pic > 1 && !pps->r->pps_loop_filter_across_tiles_enabled_flag;
    const uint8_t no_subpic_filter = rsps->sps_num_subpics_minus1 && !rsps->sps_loop_filter_across_subpic_enabled_flag[subpic_idx];
    uint8_t lf_edge[] = { 0, 0, 0, 0 };

    *restore = no_subpic_filter || no_tile_filter || !lfase || rsps->sps_virtual_boundaries_enabled_flag;

    if (!*restore)
        return;

    if (!edges[LEFT]) {
        lf_edge[LEFT]  = no_tile_filter && pps->ctb_to_col_bd[rx] == rx;
        lf_edge[LEFT] |= no_subpic_filter && rsps->sps_subpic_ctu_top_left_x[subpic_idx] == rx;
        lf_edge[LEFT] |= is_virtual_boundary(fc, rx << sps->ctb_log2_size_y, 1);
        vert_edge[0]   = !sao_can_cross_slices(fc, rx, ry, -1, 0) || lf_edge[LEFT];
    }
    if (!edges[RIGHT]) {
        lf_edge[RIGHT]  = no_tile_filter && pps->ctb_to_col_bd[rx] != pps->ctb_to_col_bd[rx + 1];
        lf_edge[RIGHT] |= no_subpic_filter && rsps->sps_subpic_ctu_top_left_x[subpic_idx] + rsps->sps_subpic_width_minus1[subpic_idx] == rx;
        lf_edge[RIGHT] |= is_virtual_boundary(fc, (rx + 1) << sps->ctb_log2_size_y, 1);
        vert_edge[1]    = !sao_can_cross_slices(fc, rx, ry, 1, 0) || lf_edge[RIGHT];
    }
    if (!edges[TOP]) {
        lf_edge[TOP]   = no_tile_filter && pps->ctb_to_row_bd[ry] == ry;
        lf_edge[TOP]  |= no_subpic_filter && rsps->sps_subpic_ctu_top_left_y[subpic_idx] == ry;
        lf_edge[TOP]  |= is_virtual_boundary(fc, ry << sps->ctb_log2_size_y, 0);
        horiz_edge[0]  = !sao_can_cross_slices(fc, rx, ry, 0, -1) || lf_edge[TOP];
    }
    if (!edges[BOTTOM]) {
        lf_edge[BOTTOM]  = no_tile_filter && pps->ctb_to_row_bd[ry] != pps->ctb_to_row_bd[ry + 1];
        lf_edge[BOTTOM] |= no_subpic_filter && rsps->sps_subpic_ctu_top_left_y[subpic_idx] + rsps->sps_subpic_height_minus1[subpic_idx] == ry;
        lf_edge[BOTTOM] |= is_virtual_boundary(fc, (ry + 1) << sps->ctb_log2_size_y, 0);
        horiz_edge[1]    = !sao_can_cross_slices(fc, rx, ry, 0, 1) || lf_edge[BOTTOM];
    }

    if (!edges[LEFT] && !edges[TOP])
        diag_edge[0] = !sao_can_cross_slices(fc, rx, ry, -1, -1) || lf_edge[LEFT] || lf_edge[TOP];

    if (!edges[TOP] && !edges[RIGHT])
        diag_edge[1] = !sao_can_cross_slices(fc, rx, ry,  1, -1) || lf_edge[RIGHT] || lf_edge[TOP];

    if (!edges[RIGHT] && !edges[BOTTOM])
        diag_edge[2] = !sao_can_cross_slices(fc, rx, ry,  1,  1) || lf_edge[RIGHT] || lf_edge[BOTTOM];

    if (!edges[LEFT] && !edges[BOTTOM])
        diag_edge[3] = !sao_can_cross_slices(fc, rx, ry, -1,  1) || lf_edge[LEFT] || lf_edge[BOTTOM];
}

static void sao_copy_hor(uint8_t *dst, const ptrdiff_t dst_stride,
    const uint8_t *src, const ptrdiff_t src_stride, const int width, const int edges[4], const int ps)
{
    const int left      = 1 - edges[LEFT];
    const int right     = 1 - edges[RIGHT];
    int pos             = 0;

    src -= left << ps;
    dst -= left << ps;

    if (left) {
        copy_pixel(dst, src, ps);
        pos += (1 << ps);
    }
    memcpy(dst + pos, src + pos, width << ps);
    if (right) {
        pos += width << ps;
        copy_pixel(dst + pos, src + pos, ps);
    }
}

static void sao_extends_edges(uint8_t *dst, const ptrdiff_t dst_stride,
    const uint8_t *src, const ptrdiff_t src_stride, const int width, const int height,
    const VVCFrameContext *fc, const int x0, const int y0, const int rx, const int ry, const int edges[4], const int c_idx)
{
    const uint8_t *sao_h = fc->tab.sao_pixel_buffer_h[c_idx];
    const uint8_t *sao_v = fc->tab.sao_pixel_buffer_v[c_idx];
    const int x          = x0 >> fc->ps.sps->hshift[c_idx];
    const int y          = y0 >> fc->ps.sps->vshift[c_idx];
    const int w          = fc->ps.pps->width >> fc->ps.sps->hshift[c_idx];
    const int h          = fc->ps.pps->height >> fc->ps.sps->vshift[c_idx];
    const int ps         = fc->ps.sps->pixel_shift;

    if (!edges[TOP])
        sao_copy_hor(dst - dst_stride, dst_stride, sao_h + (((2 * ry - 1) * w + x) << ps), src_stride, width, edges, ps);

    if (!edges[BOTTOM])
        sao_copy_hor(dst + height * dst_stride, dst_stride, sao_h + (((2 * ry + 2) * w + x) << ps), src_stride, width, edges, ps);

    if (!edges[LEFT])
        copy_vert(dst - (1 << ps), sao_v + (((2 * rx - 1) * h + y) << ps), ps, height, dst_stride, 1 << ps);

    if (!edges[RIGHT])
        copy_vert(dst + (width << ps), sao_v + (((2 * rx + 2) * h + y) << ps),  ps, height, dst_stride, 1 << ps);

    copy_ctb(dst, src, width << ps, height, dst_stride, src_stride);
}

static void sao_restore_vb(uint8_t *dst, ptrdiff_t dst_stride, const uint8_t *src, ptrdiff_t src_stride,
    const int width, const int height, const int vb_pos, const int ps, const int vertical)
{
    int w = 2;
    int h = (vertical ? height : width);
    int dx = vb_pos - 1;
    int dy = 0;

    if (!vertical) {
        FFSWAP(int, w, h);
        FFSWAP(int, dx, dy);
    }
    dst += dy * dst_stride +(dx << ps);
    src += dy * src_stride +(dx << ps);

    av_image_copy_plane(dst, dst_stride, src, src_stride, w << ps, h);
}

void ff_vvc_sao_filter(VVCLocalContext *lc, int x0, int y0)
{
    VVCFrameContext *fc  = lc->fc;
    const VVCSPS *sps    = fc->ps.sps;
    const int rx         = x0 >> sps->ctb_log2_size_y;
    const int ry         = y0 >> sps->ctb_log2_size_y;
    const int edges[4]   = { !rx, !ry, rx == fc->ps.pps->ctb_width - 1, ry == fc->ps.pps->ctb_height - 1 };
    const SAOParams *sao = &CTB(fc->tab.sao, rx, ry);
    // flags indicating unfilterable edges
    uint8_t vert_edge[]  = { 0, 0 };
    uint8_t horiz_edge[] = { 0, 0 };
    uint8_t diag_edge[]  = { 0, 0, 0, 0 };
    int restore, vb_x = 0, vb_y = 0;;

    if (sps->r->sps_virtual_boundaries_enabled_flag) {
        vb_x = get_virtual_boundary(fc, rx, 1);
        vb_y = get_virtual_boundary(fc, ry, 0);
    }

    sao_get_edges(vert_edge, horiz_edge, diag_edge, &restore, lc, edges, rx, ry);

    for (int c_idx = 0; c_idx < (sps->r->sps_chroma_format_idc ? 3 : 1); c_idx++) {
        static const uint8_t sao_tab[16] = { 0, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8 };
        const ptrdiff_t src_stride       = fc->frame->linesize[c_idx];
        uint8_t *src                     = POS(c_idx, x0, y0);
        const int hs                     = sps->hshift[c_idx];
        const int vs                     = sps->vshift[c_idx];
        const int ps                     = sps->pixel_shift;
        const int width                  = FFMIN(sps->ctb_size_y, fc->ps.pps->width  - x0) >> hs;
        const int height                 = FFMIN(sps->ctb_size_y, fc->ps.pps->height - y0) >> vs;
        const int tab                    = sao_tab[(FFALIGN(width, 8) >> 3) - 1];
        const int sao_eo_class           = sao->eo_class[c_idx];

        switch (sao->type_idx[c_idx]) {
            case SAO_BAND:
                fc->vvcdsp.sao.band_filter[tab](src, src, src_stride, src_stride,
                    sao->offset_val[c_idx], sao->band_position[c_idx], width, height);
                break;
            case SAO_EDGE:
            {
                const ptrdiff_t dst_stride = 2 * MAX_PB_SIZE + AV_INPUT_BUFFER_PADDING_SIZE;
                uint8_t *dst               = lc->sao_buffer + dst_stride + AV_INPUT_BUFFER_PADDING_SIZE;

                sao_extends_edges(dst, dst_stride, src, src_stride, width, height, fc, x0, y0, rx, ry, edges, c_idx);

                fc->vvcdsp.sao.edge_filter[tab](src, dst, src_stride, sao->offset_val[c_idx],
                    sao->eo_class[c_idx], width, height);
                fc->vvcdsp.sao.edge_restore[restore](src, dst, src_stride, dst_stride,
                    sao, edges, width, height, c_idx, vert_edge, horiz_edge, diag_edge);

                if (vb_x > x0 &&  sao_eo_class != SAO_EO_VERT)
                    sao_restore_vb(src, src_stride, dst, dst_stride, width, height, (vb_x - x0) >> hs, ps, 1);
                if (vb_y > y0 &&  sao_eo_class != SAO_EO_HORIZ)
                    sao_restore_vb(src, src_stride, dst, dst_stride, width, height, (vb_y - y0) >> vs, ps, 0);

                break;
            }
        }
    }
}

#define TAB_BS(t, x, y)       (t)[((y) >> MIN_TU_LOG2) * (fc->ps.pps->min_tu_width) + ((x) >> MIN_TU_LOG2)]
#define TAB_MAX_LEN(t, x, y)  (t)[((y) >> MIN_TU_LOG2) * (fc->ps.pps->min_tu_width) + ((x) >> MIN_TU_LOG2)]

//8 samples a time
#define DEBLOCK_STEP            8
#define LUMA_GRID               4
#define CHROMA_GRID             8

static int boundary_strength(const VVCLocalContext *lc, const MvField *curr, const MvField *neigh,
    const RefPicList *neigh_rpl)
{
    RefPicList *rpl = lc->sc->rpl;

    if (curr->pred_flag == PF_IBC)
        return FFABS(neigh->mv[0].x - curr->mv[0].x) >= 8 || FFABS(neigh->mv[0].y - curr->mv[0].y) >= 8;

    if (curr->pred_flag == PF_BI &&  neigh->pred_flag == PF_BI) {
        // same L0 and L1
        if (rpl[L0].refs[curr->ref_idx[L0]].poc == neigh_rpl[L0].refs[neigh->ref_idx[L0]].poc  &&
            rpl[L0].refs[curr->ref_idx[L0]].poc == rpl[L1].refs[curr->ref_idx[L1]].poc &&
            neigh_rpl[L0].refs[neigh->ref_idx[L0]].poc == neigh_rpl[L1].refs[neigh->ref_idx[L1]].poc) {
            if ((FFABS(neigh->mv[0].x - curr->mv[0].x) >= 8 || FFABS(neigh->mv[0].y - curr->mv[0].y) >= 8 ||
                 FFABS(neigh->mv[1].x - curr->mv[1].x) >= 8 || FFABS(neigh->mv[1].y - curr->mv[1].y) >= 8) &&
                (FFABS(neigh->mv[1].x - curr->mv[0].x) >= 8 || FFABS(neigh->mv[1].y - curr->mv[0].y) >= 8 ||
                 FFABS(neigh->mv[0].x - curr->mv[1].x) >= 8 || FFABS(neigh->mv[0].y - curr->mv[1].y) >= 8))
                return 1;
            else
                return 0;
        } else if (neigh_rpl[L0].refs[neigh->ref_idx[L0]].poc == rpl[L0].refs[curr->ref_idx[L0]].poc &&
                   neigh_rpl[L1].refs[neigh->ref_idx[L1]].poc == rpl[L1].refs[curr->ref_idx[L1]].poc) {
            if (FFABS(neigh->mv[0].x - curr->mv[0].x) >= 8 || FFABS(neigh->mv[0].y - curr->mv[0].y) >= 8 ||
                FFABS(neigh->mv[1].x - curr->mv[1].x) >= 8 || FFABS(neigh->mv[1].y - curr->mv[1].y) >= 8)
                return 1;
            else
                return 0;
        } else if (neigh_rpl[L1].refs[neigh->ref_idx[L1]].poc == rpl[L0].refs[curr->ref_idx[L0]].poc &&
                   neigh_rpl[L0].refs[neigh->ref_idx[L0]].poc == rpl[L1].refs[curr->ref_idx[L1]].poc) {
            if (FFABS(neigh->mv[1].x - curr->mv[0].x) >= 8 || FFABS(neigh->mv[1].y - curr->mv[0].y) >= 8 ||
                FFABS(neigh->mv[0].x - curr->mv[1].x) >= 8 || FFABS(neigh->mv[0].y - curr->mv[1].y) >= 8)
                return 1;
            else
                return 0;
        } else {
            return 1;
        }
    } else if ((curr->pred_flag != PF_BI) && (neigh->pred_flag != PF_BI)){ // 1 MV
        Mv A, B;
        int ref_A, ref_B;

        if (curr->pred_flag & 1) {
            A     = curr->mv[0];
            ref_A = rpl[L0].refs[curr->ref_idx[L0]].poc;
        } else {
            A     = curr->mv[1];
            ref_A = rpl[L1].refs[curr->ref_idx[L1]].poc;
        }

        if (neigh->pred_flag & 1) {
            B     = neigh->mv[0];
            ref_B = neigh_rpl[L0].refs[neigh->ref_idx[L0]].poc;
        } else {
            B     = neigh->mv[1];
            ref_B = neigh_rpl[L1].refs[neigh->ref_idx[L1]].poc;
        }

        if (ref_A == ref_B) {
            if (FFABS(A.x - B.x) >= 8 || FFABS(A.y - B.y) >= 8)
                return 1;
            else
                return 0;
        } else
            return 1;
    }

    return 1;
}

//part of 8.8.3.3 Derivation process of transform block boundary
static void derive_max_filter_length_luma(const VVCFrameContext *fc, const int qx, const int qy,
    const int size_q, const int has_subblock, const int vertical, uint8_t *max_len_p, uint8_t *max_len_q)
{
    const int px =  vertical ? qx - 1 : qx;
    const int py = !vertical ? qy - 1 : qy;
    const uint8_t *tb_size = vertical ? fc->tab.tb_width[LUMA] : fc->tab.tb_height[LUMA];
    const int size_p = tb_size[(py >> MIN_TU_LOG2) * fc->ps.pps->min_tu_width + (px >> MIN_TU_LOG2)];
    const int min_cb_log2 = fc->ps.sps->min_cb_log2_size_y;
    const int off_p = (py >> min_cb_log2) * fc->ps.pps->min_cb_width + (px >> min_cb_log2);

    if (size_p <= 4 || size_q <= 4) {
        *max_len_p = *max_len_q = 1;
    } else {
        *max_len_p = *max_len_q = 3;
        if (size_p >= 32)
            *max_len_p = 7;
        if (size_q >= 32)
            *max_len_q = 7;
    }
    if (has_subblock)
        *max_len_q = FFMIN(5, *max_len_q);
    if (fc->tab.msf[off_p] || fc->tab.iaf[off_p])
        *max_len_p = FFMIN(5, *max_len_p);
}

static void vvc_deblock_subblock_bs(const VVCLocalContext *lc,
    const int cb, int x0, int y0, int width, int height, const int vertical)
{
    const VVCFrameContext  *fc = lc->fc;
    const MvField *tab_mvf     = fc->tab.mvf;
    const RefPicList *rpl      = lc->sc->rpl;
    int stridea                = fc->ps.pps->min_pu_width;
    int strideb                = 1;
    const int log2_min_pu_size = MIN_PU_LOG2;

    if (!vertical) {
        FFSWAP(int, x0, y0);
        FFSWAP(int, width, height);
        FFSWAP(int, stridea, strideb);
    }

    // bs for TU internal vertical PU boundaries
    for (int i = 8 - ((x0 - cb) % 8); i < width; i += 8) {
        const int is_vb = is_virtual_boundary(fc, x0 + i, vertical);
        const int xp_pu = (x0 + i - 1) >> log2_min_pu_size;
        const int xq_pu = (x0 + i)     >> log2_min_pu_size;

        for (int j = 0; j < height; j += 4) {
            const int y_pu       = (y0 + j) >> log2_min_pu_size;
            const MvField *mvf_p = &tab_mvf[y_pu * stridea + xp_pu * strideb];
            const MvField *mvf_q = &tab_mvf[y_pu * stridea + xq_pu * strideb];
            const int bs         = is_vb ? 0 : boundary_strength(lc, mvf_q, mvf_p, rpl);
            int x                = x0 + i;
            int y                = y0 + j;
            uint8_t max_len_p = 0, max_len_q = 0;

            if (!vertical)
                FFSWAP(int, x, y);

            TAB_BS(fc->tab.bs[vertical][LUMA], x, y) = bs;

            if (i == 4 || i == width - 4)
                max_len_p = max_len_q = 1;
            else if (i == 8 || i == width - 8)
                max_len_p = max_len_q = 2;
            else
                max_len_p = max_len_q = 3;

            TAB_MAX_LEN(fc->tab.max_len_p[vertical], x, y) = max_len_p;
            TAB_MAX_LEN(fc->tab.max_len_q[vertical], x, y) = max_len_q;
        }
    }
}

static av_always_inline int deblock_bs(const VVCLocalContext *lc,
    const int x_p, const int y_p, const int x_q, const int y_q, const CodingUnit *cu, const TransformUnit *tu,
    const RefPicList *rpl_p, const int c_idx, const int off_to_cb, const uint8_t has_sub_block)
{
    const VVCFrameContext *fc  = lc->fc;
    const MvField *tab_mvf     = fc->tab.mvf;
    const int log2_min_pu_size = MIN_PU_LOG2;
    const int log2_min_tu_size = MIN_TU_LOG2;
    const int log2_min_cb_size = fc->ps.sps->min_cb_log2_size_y;
    const int min_pu_width     = fc->ps.pps->min_pu_width;
    const int min_tu_width     = fc->ps.pps->min_tu_width;
    const int min_cb_width     = fc->ps.pps->min_cb_width;
    const int pu_p             = (y_p >> log2_min_pu_size) * min_pu_width  + (x_p >>  log2_min_pu_size);
    const int pu_q             = (y_q >> log2_min_pu_size) * min_pu_width  + (x_q >>  log2_min_pu_size);
    const MvField *mvf_p       = &tab_mvf[pu_p];
    const MvField *mvf_q       = &tab_mvf[pu_q];
    const uint8_t chroma       = !!c_idx;
    const int tu_p             = (y_p >> log2_min_tu_size) * min_tu_width  + (x_p >>  log2_min_tu_size);
    const int cb_p             = (y_p >> log2_min_cb_size) * min_cb_width  + (x_p >>  log2_min_cb_size);
    const uint8_t pcmf         = fc->tab.pcmf[chroma][cb_p] && cu->bdpcm_flag[chroma];
    const uint8_t intra        = fc->tab.cpm[chroma][cb_p] == MODE_INTRA || cu->pred_mode == MODE_INTRA;
    const uint8_t same_mode    = fc->tab.cpm[chroma][cb_p] == cu->pred_mode;

    if (pcmf)
        return 0;

    if (intra || mvf_p->ciip_flag || mvf_q->ciip_flag)
        return 2;

    if (chroma) {
        return fc->tab.tu_coded_flag[c_idx][tu_p] ||
               fc->tab.tu_joint_cbcr_residual_flag[tu_p] ||
               tu->coded_flag[c_idx] ||
               tu->joint_cbcr_residual_flag;
    }

    if (fc->tab.tu_coded_flag[LUMA][tu_p] || tu->coded_flag[LUMA])
        return 1;

    if ((off_to_cb && ((off_to_cb % 8) || !has_sub_block)))
        return 0;                                     // inside a cu, not aligned to 8 or with no subblocks

    if (!same_mode)
        return 1;

    return boundary_strength(lc,
