Do not include any other text.

----- BEGIN modified.c -----
/*
 * ffmpeg filter configuration
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdint.h>

#include "ffmpeg.h"

#include "libavfilter/avfilter.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"

#include "libavutil/avassert.h"
#include "libavutil/avstring.h"
#include "libavutil/bprint.h"
#include "libavutil/channel_layout.h"
#include "libavutil/downmix_info.h"
#include "libavutil/mem.h"
#include "libavutil/opt.h"
#include "libavutil/pixdesc.h"
#include "libavutil/pixfmt.h"
#include "libavutil/samplefmt.h"
#include "libavutil/time.h"
#include "libavutil/timestamp.h"

// FIXME private header, used for mid_pred()
#include "libavcodec/mathops.h"

typedef struct FilterGraphPriv {
    FilterGraph      fg;

    // name used for logging
    char             log_name[32];

    int              is_simple;
    // true when the filtergraph contains only meta filters
    // that do not modify the frame data
    int              is_meta;
    // source filters are present in the graph
    int              have_sources;
    int              disable_conversions;

    unsigned         nb_outputs_done;

    const char      *graph_desc;

    int              nb_threads;

    // frame for temporarily holding output from the filtergraph
    AVFrame         *frame;
    // frame for sending output to the encoder
    AVFrame         *frame_enc;

    Scheduler       *sch;
    unsigned         sch_idx;
} FilterGraphPriv;

static FilterGraphPriv *fgp_from_fg(FilterGraph *fg)
{
    return (FilterGraphPriv*)fg;
}

static const FilterGraphPriv *cfgp_from_cfg(const FilterGraph *fg)
{
    return (const FilterGraphPriv*)fg;
}

// data that is local to the filter thread and not visible outside of it
typedef struct FilterGraphThread {
    AVFilterGraph   *graph;

    AVFrame         *frame;

    // Temporary buffer for output frames, since on filtergraph reset
    // we cannot send them to encoders immediately.
    // The output index is stored in frame opaque.
    AVFifo          *frame_queue_out;

    // index of the next input to request from the scheduler
    unsigned         next_in;
    // set to 1 after at least one frame passed through this output
    int              got_frame;

    // EOF status of each input/output, as received by the thread
    uint8_t         *eof_in;
    uint8_t         *eof_out;
} FilterGraphThread;

typedef struct InputFilterPriv {
    InputFilter         ifilter;

    InputFilterOptions  opts;

    int                 index;

    AVFilterContext    *filter;

    // used to hold submitted input
    AVFrame            *frame;

    /* for filters that are not yet bound to an input stream,
     * this stores the input linklabel, if any */
    uint8_t            *linklabel;

    // filter data type
    enum AVMediaType    type;
    // source data type: AVMEDIA_TYPE_SUBTITLE for sub2video,
    // same as type otherwise
    enum AVMediaType    type_src;

    int                 eof;
    int                 bound;

    // parameters configured for this input
    int                 format;

    int                 width, height;
    AVRational          sample_aspect_ratio;
    enum AVColorSpace   color_space;
    enum AVColorRange   color_range;

    int                 sample_rate;
    AVChannelLayout     ch_layout;

    AVRational          time_base;

    AVFrameSideData   **side_data;
    int                 nb_side_data;

    AVFifo             *frame_queue;

    AVBufferRef        *hw_frames_ctx;

    int                 displaymatrix_present;
    int                 displaymatrix_applied;
    int32_t             displaymatrix[9];

    int                 downmixinfo_present;
    AVDownmixInfo       downmixinfo;

    struct {
        AVFrame *frame;

        int64_t last_pts;
        int64_t end_pts;

        ///< marks if sub2video_update should force an initialization
        unsigned int initialize;
    } sub2video;
} InputFilterPriv;

static InputFilterPriv *ifp_from_ifilter(InputFilter *ifilter)
{
    return (InputFilterPriv*)ifilter;
}

typedef struct FPSConvContext {
    AVFrame          *last_frame;
    /* number of frames emitted by the video-encoding sync code */
    int64_t           frame_number;
    /* history of nb_frames_prev, i.e. the number of times the
     * previous frame was duplicated by vsync code in recent
     * do_video_out() calls */
    int64_t           frames_prev_hist[3];

    uint64_t          dup_warning;

    int               last_dropped;
    int               dropped_keyframe;

    enum VideoSyncMethod vsync_method;

    AVRational        framerate;
    AVRational        framerate_max;
    const AVRational *framerate_supported;
    int               framerate_clip;
} FPSConvContext;

typedef struct OutputFilterPriv {
    OutputFilter            ofilter;

    int                     index;

    void                   *log_parent;
    char                    log_name[32];

    char                   *name;

    AVFilterContext        *filter;

    /* desired output stream properties */
    int                     format;
    int                     width, height;
    int                     sample_rate;
    AVChannelLayout         ch_layout;
    enum AVColorSpace       color_space;
    enum AVColorRange       color_range;

    AVFrameSideData       **side_data;
    int                     nb_side_data;

    // time base in which the output is sent to our downstream
    // does not need to match the filtersink's timebase
    AVRational              tb_out;
    // at least one frame with the above timebase was sent
    // to our downstream, so it cannot change anymore
    int                     tb_out_locked;

    AVRational              sample_aspect_ratio;

    AVDictionary           *sws_opts;
    AVDictionary           *swr_opts;

    // those are only set if no format is specified and the encoder gives us multiple options
    // They point directly to the relevant lists of the encoder.
    const int              *formats;
    const AVChannelLayout  *ch_layouts;
    const int              *sample_rates;
    const enum AVColorSpace *color_spaces;
    const enum AVColorRange *color_ranges;

    AVRational              enc_timebase;
    int64_t                 trim_start_us;
    int64_t                 trim_duration_us;
    // offset for output timestamps, in AV_TIME_BASE_Q
    int64_t                 ts_offset;
    int64_t                 next_pts;
    FPSConvContext          fps;

    unsigned                flags;
} OutputFilterPriv;

static OutputFilterPriv *ofp_from_ofilter(OutputFilter *ofilter)
{
    return (OutputFilterPriv*)ofilter;
}

typedef struct FilterCommand {
    char *target;
    char *command;
    char *arg;

    double time;
    int    all_filters;
} FilterCommand;

static void filter_command_free(void *opaque, uint8_t *data)
{
    FilterCommand *fc = (FilterCommand*)data;

    av_freep(&fc->target);
    av_freep(&fc->command);
    av_freep(&fc->arg);

    av_free(data);
}

static int sub2video_get_blank_frame(InputFilterPriv *ifp)
{
    AVFrame *frame = ifp->sub2video.frame;
    int ret;

    av_frame_unref(frame);

    frame->width  = ifp->width;
    frame->height = ifp->height;
    frame->format = ifp->format;
    frame->colorspace = ifp->color_space;
    frame->color_range = ifp->color_range;

    ret = av_frame_get_buffer(frame, 0);
    if (ret < 0)
        return ret;

    memset(frame->data[0], 0, frame->height * frame->linesize[0]);

    return 0;
}

static void sub2video_copy_rect(uint8_t *dst, int dst_linesize, int w, int h,
                                AVSubtitleRect *r)
{
    uint32_t *pal, *dst2;
    uint8_t *src, *src2;
    int x, y;

    if (r->type != SUBTITLE_BITMAP) {
        av_log(NULL, AV_LOG_WARNING, "sub2video: non-bitmap subtitle\n");
        return;
    }
    if (r->x < 0 || r->x + r->w > w || r->y < 0 || r->y + r->h > h) {
        av_log(NULL, AV_LOG_WARNING, "sub2video: rectangle (%d %d %d %d) overflowing %d %d\n",
            r->x, r->y, r->w, r->h, w, h
        );
        return;
    }

    dst += r->y * dst_linesize + r->x * 4;
    src = r->data[0];
    pal = (uint32_t *)r->data[1];
    for (y = 0; y < r->h; y++) {
        dst2 = (uint32_t *)dst;
        src2 = src;
        for (x = 0; x < r->w; x++)
            *(dst2++) = pal[*(src2++)];
        dst += dst_linesize;
        src += r->linesize[0];
    }
}

static void sub2video_push_ref(InputFilterPriv *ifp, int64_t pts)
{
    AVFrame *frame = ifp->sub2video.frame;
    int ret;

    av_assert1(frame->data[0]);
    ifp->sub2video.last_pts = frame->pts = pts;
    ret = av_buffersrc_add_frame_flags(ifp->filter, frame,
                                       AV_BUFFERSRC_FLAG_KEEP_REF |
                                       AV_BUFFERSRC_FLAG_PUSH);
    if (ret != AVERROR_EOF && ret < 0)
        av_log(ifp->ifilter.graph, AV_LOG_WARNING,
               "Error while add the frame to buffer source(%s).\n",
               av_err2str(ret));
}

static void sub2video_update(InputFilterPriv *ifp, int64_t heartbeat_pts,
                             const AVSubtitle *sub)
{
    AVFrame *frame = ifp->sub2video.frame;
    int8_t *dst;
    int     dst_linesize;
    int num_rects;
    int64_t pts, end_pts;

    if (sub) {
        pts       = av_rescale_q(sub->pts + sub->start_display_time * 1000LL,
                                 AV_TIME_BASE_Q, ifp->time_base);
        end_pts   = av_rescale_q(sub->pts + sub->end_display_time   * 1000LL,
                                 AV_TIME_BASE_Q, ifp->time_base);
        num_rects = sub->num_rects;
    } else {
        /* If we are initializing the system, utilize current heartbeat
           PTS as the start time, and show until the following subpicture
           is received. Otherwise, utilize the previous subpicture's end time
           as the fall-back value. */
        pts       = ifp->sub2video.initialize ?
                    heartbeat_pts : ifp->sub2video.end_pts;
        end_pts   = INT64_MAX;
        num_rects = 0;
    }
    if (sub2video_get_blank_frame(ifp) < 0) {
        av_log(ifp->ifilter.graph, AV_LOG_ERROR,
               "Impossible to get a blank canvas.\n");
        return;
    }
    dst          = frame->data    [0];
    dst_linesize = frame->linesize[0];
    for (int i = 0; i < num_rects; i++)
        sub2video_copy_rect(dst, dst_linesize, frame->width, frame->height, sub->rects[i]);
    sub2video_push_ref(ifp, pts);
    ifp->sub2video.end_pts = end_pts;
    ifp->sub2video.initialize = 0;
}

/* Define a function for appending a list of allowed formats
 * to an AVBPrint. If nonempty, the list will have a header. */
#define DEF_CHOOSE_FORMAT(name, type, var, supported_list, none, printf_format, get_name) \
static void choose_ ## name (OutputFilterPriv *ofp, AVBPrint *bprint)          \
{                                                                              \
    if (ofp->var == none && !ofp->supported_list)                              \
        return;                                                                \
    av_bprintf(bprint, #name "=");                                             \
    if (ofp->var != none) {                                                    \
        av_bprintf(bprint, printf_format, get_name(ofp->var));                 \
    } else {                                                                   \
        const type *p;                                                         \
                                                                               \
        for (p = ofp->supported_list; *p != none; p++) {                       \
            av_bprintf(bprint, printf_format "|", get_name(*p));               \
        }                                                                      \
        if (bprint->len > 0)                                                   \
            bprint->str[--bprint->len] = '\0';                                 \
    }                                                                          \
    av_bprint_chars(bprint, ':', 1);                                           \
}

DEF_CHOOSE_FORMAT(pix_fmts, enum AVPixelFormat, format, formats,
                  AV_PIX_FMT_NONE, "%s", av_get_pix_fmt_name)

DEF_CHOOSE_FORMAT(sample_fmts, enum AVSampleFormat, format, formats,
                  AV_SAMPLE_FMT_NONE, "%s", av_get_sample_fmt_name)

DEF_CHOOSE_FORMAT(sample_rates, int, sample_rate, sample_rates, 0,
                  "%d", )

DEF_CHOOSE_FORMAT(color_spaces, enum AVColorSpace, color_space, color_spaces,
                  AVCOL_SPC_UNSPECIFIED, "%s", av_color_space_name);

DEF_CHOOSE_FORMAT(color_ranges, enum AVColorRange, color_range, color_ranges,
                  AVCOL_RANGE_UNSPECIFIED, "%s", av_color_range_name);

static void choose_channel_layouts(OutputFilterPriv *ofp, AVBPrint *bprint)
{
    if (av_channel_layout_check(&ofp->ch_layout)) {
        av_bprintf(bprint, "channel_layouts=");
        av_channel_layout_describe_bprint(&ofp->ch_layout, bprint);
    } else if (ofp->ch_layouts) {
        const AVChannelLayout *p;

        av_bprintf(bprint, "channel_layouts=");
        for (p = ofp->ch_layouts; p->nb_channels; p++) {
            av_channel_layout_describe_bprint(p, bprint);
            av_bprintf(bprint, "|");
        }
        if (bprint->len > 0)
            bprint->str[--bprint->len] = '\0';
    } else
        return;
    av_bprint_chars(bprint, ':', 1);
}

static int read_binary(void *logctx, const char *path,
                       uint8_t **data, int *len)
{
    AVIOContext *io = NULL;
    int64_t fsize;
    int ret;

    *data = NULL;
    *len  = 0;

    ret = avio_open2(&io, path, AVIO_FLAG_READ, &int_cb, NULL);
    if (ret < 0) {
        av_log(logctx, AV_LOG_ERROR, "Cannot open file '%s': %s\n",
               path, av_err2str(ret));
        return ret;
    }

    fsize = avio_size(io);
    if (fsize < 0 || fsize > INT_MAX) {
        av_log(logctx, AV_LOG_ERROR, "Cannot obtain size of file %s\n", path);
        ret = AVERROR(EIO);
        goto fail;
    }

    *data = av_malloc(fsize);
    if (!*data) {
        ret = AVERROR(ENOMEM);
        goto fail;
    }

    ret = avio_read(io, *data, fsize);
    if (ret != fsize) {
        av_log(logctx, AV_LOG_ERROR, "Error reading file %s\n", path);
        ret = ret < 0 ? ret : AVERROR(EIO);
        goto fail;
    }

    *len = fsize;

    ret = 0;
fail:
    avio_close(io);
    if (ret < 0) {
        av_freep(data);
        *len = 0;
    }
    return ret;
}

static int filter_opt_apply(void *logctx, AVFilterContext *f,
                            const char *key, const char *val)
{
    const AVOption *o = NULL;
    int ret;

    ret = av_opt_set(f, key, val, AV_OPT_SEARCH_CHILDREN);
    if (ret >= 0)
        return 0;

    if (ret == AVERROR_OPTION_NOT_FOUND && key[0] == '/')
        o = av_opt_find(f, key + 1, NULL, 0, AV_OPT_SEARCH_CHILDREN);
    if (!o)
        goto err_apply;

    // key is a valid option name prefixed with '/'
    // interpret value as a path from which to load the actual option value
    key++;

    if (o->type == AV_OPT_TYPE_BINARY) {
        uint8_t *data;
        int      len;

        ret = read_binary(logctx, val, &data, &len);
        if (ret < 0)
            goto err_load;

        ret = av_opt_set_bin(f, key, data, len, AV_OPT_SEARCH_CHILDREN);
        av_freep(&data);
    } else {
        char *data = file_read(val);
        if (!data) {
            ret = AVERROR(EIO);
            goto err_load;
        }

        ret = av_opt_set(f, key, data, AV_OPT_SEARCH_CHILDREN);
        av_freep(&data);
    }
    if (ret < 0)
        goto err_apply;

    return 0;

err_apply:
    av_log(logctx, AV_LOG_ERROR,
           "Error applying option '%s' to filter '%s': %s\n",
           key, f->filter->name, av_err2str(ret));
    return ret;
err_load:
    av_log(logctx, AV_LOG_ERROR,
           "Error loading value for option '%s' from file '%s'\n",
           key, val);
    return ret;
}

static int graph_opts_apply(void *logctx, AVFilterGraphSegment *seg)
{
    for (size_t i = 0; i < seg->nb_chains; i++) {
        AVFilterChain *ch = seg->chains[i];

        for (size_t j = 0; j < ch->nb_filters; j++) {
            AVFilterParams *p = ch->filters[j];
            const AVDictionaryEntry *e = NULL;

            av_assert0(p->filter);

            while ((e = av_dict_iterate(p->opts, e))) {
                int ret = filter_opt_apply(logctx, p->filter, e->key, e->value);
                if (ret < 0)
                    return ret;
            }

            av_dict_free(&p->opts);
        }
    }

    return 0;
}

static int graph_parse(void *logctx,
                       AVFilterGraph *graph, const char *desc,
                       AVFilterInOut **inputs, AVFilterInOut **outputs,
                       AVBufferRef *hw_device)
{
    AVFilterGraphSegment *seg;
    int ret;

    *inputs  = NULL;
    *outputs = NULL;

    ret = avfilter_graph_segment_parse(graph, desc, 0, &seg);
    if (ret < 0)
        return ret;

    ret = avfilter_graph_segment_create_filters(seg, 0);
    if (ret < 0)
        goto fail;

    if (hw_device) {
        for (int i = 0; i < graph->nb_filters; i++) {
            AVFilterContext *f = graph->filters[i];

            if (!(f->filter->flags & AVFILTER_FLAG_HWDEVICE))
                continue;
            f->hw_device_ctx = av_buffer_ref(hw_device);
            if (!f->hw_device_ctx) {
                ret = AVERROR(ENOMEM);
                goto fail;
            }
        }
    }

    ret = graph_opts_apply(logctx, seg);
    if (ret < 0)
        goto fail;

    ret = avfilter_graph_segment_apply(seg, 0, inputs, outputs);

fail:
    avfilter_graph_segment_free(&seg);
    return ret;
}

// Filters can be configured only if the formats of all inputs are known.
static int ifilter_has_all_input_formats(FilterGraph *fg)
{
    for (int i = 0; i < fg->nb_inputs; i++) {
        InputFilterPriv *ifp = ifp_from_ifilter(fg->inputs[i]);
        if (ifp->format < 0)
            return 0;
    }
    return 1;
}

static int filter_thread(void *arg);

static char *describe_filter_link(FilterGraph *fg, AVFilterInOut *inout, int in)
{
    AVFilterContext *ctx = inout->filter_ctx;
    AVFilterPad *pads = in ? ctx->input_pads  : ctx->output_pads;
    int       nb_pads = in ? ctx->nb_inputs   : ctx->nb_outputs;

    if (nb_pads > 1)
        return av_strdup(ctx->filter->name);
    return av_asprintf("%s:%s", ctx->filter->name,
                       avfilter_pad_get_name(pads, inout->pad_idx));
}

static const char *ofilter_item_name(void *obj)
{
    OutputFilterPriv *ofp = obj;
    return ofp->log_name;
}

static const AVClass ofilter_class = {
    .class_name                = "OutputFilter",
    .version                   = LIBAVUTIL_VERSION_INT,
    .item_name                 = ofilter_item_name,
    .parent_log_context_offset = offsetof(OutputFilterPriv, log_parent),
    .category                  = AV_CLASS_CATEGORY_FILTER,
};

static OutputFilter *ofilter_alloc(FilterGraph *fg, enum AVMediaType type)
{
    OutputFilterPriv *ofp;
    OutputFilter *ofilter;

    ofp = allocate_array_elem(&fg->outputs, sizeof(*ofp), &fg->nb_outputs);
    if (!ofp)
        return NULL;

    ofilter           = &ofp->ofilter;
    ofilter->class    = &ofilter_class;
    ofp->log_parent   = fg;
    ofilter->graph    = fg;
    ofilter->type     = type;
    ofp->format       = -1;
    ofp->color_space  = AVCOL_SPC_UNSPECIFIED;
    ofp->color_range  = AVCOL_RANGE_UNSPECIFIED;
    ofp->index        = fg->nb_outputs - 1;

    snprintf(ofp->log_name, sizeof(ofp->log_name), "%co%d",
             av_get_media_type_string(type)[0], ofp->index);

    return ofilter;
}

static int ifilter_bind_ist(InputFilter *ifilter, InputStream *ist,
                            const ViewSpecifier *vs)
{
    InputFilterPriv *ifp = ifp_from_ifilter(ifilter);
    FilterGraphPriv *fgp = fgp_from_fg(ifilter->graph);
    SchedulerNode src;
    int ret;

    av_assert0(!ifp->bound);
    ifp->bound = 1;

    if (ifp->type != ist->par->codec_type &&
        !(ifp->type == AVMEDIA_TYPE_VIDEO && ist->par->codec_type == AVMEDIA_TYPE_SUBTITLE)) {
        av_log(fgp, AV_LOG_ERROR, "Tried to connect %s stream to %s filtergraph input\n",
               av_get_media_type_string(ist->par->codec_type), av_get_media_type_string(ifp->type));
        return AVERROR(EINVAL);
    }

    ifp->type_src        = ist->st->codecpar->codec_type;

    ifp->opts.fallback = av_frame_alloc();
    if (!ifp->opts.fallback)
        return AVERROR(ENOMEM);

    ret = ist_filter_add(ist, ifilter, filtergraph_is_simple(ifilter->graph),
                         vs, &ifp->opts, &src);
    if (ret < 0)
        return ret;

    ret = sch_connect(fgp->sch,
                      src, SCH_FILTER_IN(fgp->sch_idx, ifp->index));
    if (ret < 0)
        return ret;

    if (ifp->type_src == AVMEDIA_TYPE_SUBTITLE) {
        ifp->sub2video.frame = av_frame_alloc();
        if (!ifp->sub2video.frame)
            return AVERROR(ENOMEM);

        ifp->width  = ifp->opts.sub2video_width;
        ifp->height = ifp->opts.sub2video_height;

        /* rectangles are AV_PIX_FMT_PAL8, but we have no guarantee that the
           palettes for all rectangles are identical or compatible */
        ifp->format = AV_PIX_FMT_RGB32;

        ifp->time_base = AV_TIME_BASE_Q;

        av_log(fgp, AV_LOG_VERBOSE, "sub2video: using %dx%d canvas\n",
               ifp->width, ifp->height);
    }

    return 0;
}

static int ifilter_bind_dec(InputFilterPriv *ifp, Decoder *dec,
                            const ViewSpecifier *vs)
{
    FilterGraphPriv *fgp = fgp_from_fg(ifp->ifilter.graph);
    SchedulerNode src;
    int ret;

    av_assert0(!ifp->bound);
    ifp->bound = 1;

    if (ifp->type != dec->type) {
        av_log(fgp, AV_LOG_ERROR, "Tried to connect %s decoder to %s filtergraph input\n",
               av_get_media_type_string(dec->type), av_get_media_type_string(ifp->type));
        return AVERROR(EINVAL);
    }

    ifp->type_src = ifp->type;

    ret = dec_filter_add(dec, &ifp->ifilter, &ifp->opts, vs, &src);
    if (ret < 0)
        return ret;

    ret = sch_connect(fgp->sch, src, SCH_FILTER_IN(fgp->sch_idx, ifp->index));
    if (ret < 0)
        return ret;

    return 0;
}

static int set_channel_layout(OutputFilterPriv *f, const AVChannelLayout *layouts_allowed,
                              const AVChannelLayout *layout_requested)
{
    int i, err;

    if (layout_requested->order != AV_CHANNEL_ORDER_UNSPEC) {
        /* Pass the layout through for all orders but UNSPEC */
        err = av_channel_layout_copy(&f->ch_layout, layout_requested);
        if (err < 0)
            return err;
        return 0;
    }

    /* Requested layout is of order UNSPEC */
    if (!layouts_allowed) {
        /* Use the default native layout for the requested amount of channels when the
           encoder doesn't have a list of supported layouts */
        av_channel_layout_default(&f->ch_layout, layout_requested->nb_channels);
        return 0;
    }
    /* Encoder has a list of supported layouts. Pick the first layout in it with the
       same amount of channels as the requested layout */
    for (i = 0; layouts_allowed[i].nb_channels; i++) {
        if (layouts_allowed[i].nb_channels == layout_requested->nb_channels)
            break;
    }
    if (layouts_allowed[i].nb_channels) {
        /* Use it if one is found */
        err = av_channel_layout_copy(&f->ch_layout, &layouts_allowed[i]);
        if (err < 0)
            return err;
        return 0;
    }
    /* If no layout for the amount of channels requested was found, use the default
       native layout for it. */
    av_channel_layout_default(&f->ch_layout, layout_requested->nb_channels);

    return 0;
}

int ofilter_bind_enc(OutputFilter *ofilter, unsigned sched_idx_enc,
                     const OutputFilterOptions *opts)
{
    OutputFilterPriv *ofp = ofp_from_ofilter(ofilter);
    FilterGraph  *fg = ofilter->graph;
    FilterGraphPriv *fgp = fgp_from_fg(fg);
    int ret;

    av_assert0(!ofilter->bound);
    av_assert0(!opts->enc ||
               ofilter->type == opts->enc->type);

    ofilter->bound = 1;
    av_freep(&ofilter->linklabel);

    ofp->flags        = opts->flags;
    ofp->ts_offset    = opts->ts_offset;
    ofp->enc_timebase = opts->output_tb;

    ofp->trim_start_us    = opts->trim_start_us;
    ofp->trim_duration_us = opts->trim_duration_us;

    ofp->name         = av_strdup(opts->name);
    if (!ofp->name)
        return AVERROR(EINVAL);

    ret = av_dict_copy(&ofp->sws_opts, opts->sws_opts, 0);
    if (ret < 0)
        return ret;

    ret = av_dict_copy(&ofp->swr_opts, opts->swr_opts, 0);
    if (ret < 0)
        return ret;

    if (opts->flags & OFILTER_FLAG_AUDIO_24BIT)
        av_dict_set(&ofp->swr_opts, "output_sample_bits", "24", 0);

    if (fgp->is_simple) {
        // for simple filtergraph there is just one output,
        // so use only graph-level information for logging
        ofp->log_parent = NULL;
        av_strlcpy(ofp->log_name, fgp->log_name, sizeof(ofp->log_name));
    } else
        av_strlcatf(ofp->log_name, sizeof(ofp->log_name), "->%s", ofp->name);

    switch (ofilter->type) {
    case AVMEDIA_TYPE_VIDEO:
        ofp->width      = opts->width;
        ofp->height     = opts->height;
        if (opts->format != AV_PIX_FMT_NONE) {
            ofp->format = opts->format;
        } else
            ofp->formats = opts->formats;

        if (opts->color_space != AVCOL_SPC_UNSPECIFIED)
            ofp->color_space = opts->color_space;
        else
            ofp->color_spaces = opts->color_spaces;

        if (opts->color_range != AVCOL_RANGE_UNSPECIFIED)
            ofp->color_range = opts->color_range;
        else
            ofp->color_ranges = opts->color_ranges;

        fgp->disable_conversions |= !!(ofp->flags & OFILTER_FLAG_DISABLE_CONVERT);

        ofp->fps.last_frame = av_frame_alloc();
        if (!ofp->fps.last_frame)
            return AVERROR(ENOMEM);

        ofp->fps.vsync_method        = opts->vsync_method;
        ofp->fps.framerate           = opts->frame_rate;
        ofp->fps.framerate_max       = opts->max_frame_rate;
        ofp->fps.framerate_supported = opts->frame_rates;

        // reduce frame rate for mpeg4 to be within the spec limits
        if (opts->enc && opts->enc->id == AV_CODEC_ID_MPEG4)
            ofp->fps.framerate_clip = 65535;

        ofp->fps.dup_warning         = 1000;

        break;
    case AVMEDIA_TYPE_AUDIO:
        if (opts->format != AV_SAMPLE_FMT_NONE) {
            ofp->format = opts->format;
        } else {
            ofp->formats = opts->formats;
        }
        if (opts->sample_rate) {
            ofp->sample_rate = opts->sample_rate;
        } else
            ofp->sample_rates = opts->sample_rates;
        if (opts->ch_layout.nb_channels) {
            int ret = set_channel_layout(ofp, opts->ch_layouts, &opts->ch_layout);
            if (ret < 0)
                return ret;
        } else {
            ofp->ch_layouts = opts->ch_layouts;
        }
        break;
    }

    ret = sch_connect(fgp->sch, SCH_FILTER_OUT(fgp->sch_idx, ofp->index),
                                SCH_ENC(sched_idx_enc));
    if (ret < 0)
        return ret;

    return 0;
}

static int ofilter_bind_ifilter(OutputFilter *ofilter, InputFilterPriv *ifp,
                                const OutputFilterOptions *opts)
{
    OutputFilterPriv *ofp = ofp_from_ofilter(ofilter);

    av_assert0(!ofilter->bound);
    av_assert0(ofilter->type == ifp->type);

    ofilter->bound = 1;
    av_freep(&ofilter->linklabel);

    ofp->name = av_strdup(opts->name);
    if (!ofp->name)
        return AVERROR(EINVAL);

    av_strlcatf(ofp->log_name, sizeof(ofp->log_name), "->%s", ofp->name);

    return 0;
}

static int ifilter_bind_fg(InputFilterPriv *ifp, FilterGraph *fg_src, int out_idx)
{
    FilterGraphPriv      *fgp = fgp_from_fg(ifp->ifilter.graph);
    OutputFilter *ofilter_src = fg_src->outputs[out_idx];
    OutputFilterOptions opts;
    char name[32];
    int ret;

    av_assert0(!ifp->bound);
    ifp->bound = 1;

    if (ifp->type != ofilter_src->type) {
        av_log(fgp, AV_LOG_ERROR, "Tried to connect %s output to %s input\n",
               av_get_media_type_string(ofilter_src->type),
               av_get_media_type_string(ifp->type));
        return AVERROR(EINVAL);
    }

    ifp->type_src = ifp->type;

    memset(&opts, 0, sizeof(opts));

    snprintf(name, sizeof(name), "fg:%d:%d", fgp->fg.index, ifp->index);
    opts.name = name;

    ret = ofilter_bind_ifilter(ofilter_src, ifp, &opts);
    if (ret < 0)
        return ret;

    ret = sch_connect(fgp->sch, SCH_FILTER_OUT(fg_src->index, out_idx),
                                SCH_FILTER_IN(fgp->sch_idx, ifp->index));
    if (ret < 0)
        return ret;

    return 0;
}

static InputFilter *ifilter_alloc(FilterGraph *fg)
{
    InputFilterPriv *ifp;
    InputFilter *ifilter;

    ifp = allocate_array_elem(&fg->inputs, sizeof(*ifp), &fg->nb_inputs);
    if (!ifp)
        return NULL;

    ifilter         = &ifp->ifilter;
    ifilter->graph  = fg;

    ifp->frame = av_frame_alloc();
    if (!ifp->frame)
        return NULL;

    ifp->index           = fg->nb_inputs - 1;
    ifp->format          = -1;
    ifp->color_space     = AVCOL_SPC_UNSPECIFIED;
    ifp->color_range     = AVCOL_RANGE_UNSPECIFIED;

    ifp->frame_queue = av_fifo_alloc2(8, sizeof(AVFrame*), AV_FIFO_FLAG_AUTO_GROW);
    if (!ifp->frame_queue)
        return NULL;

    return ifilter;
}

void fg_free(FilterGraph **pfg)
{
    FilterGraph *fg = *pfg;
    FilterGraphPriv *fgp;

    if (!fg)
        return;
    fgp = fgp_from_fg(fg);

    for (int j = 0; j < fg->nb_inputs; j++) {
        InputFilter *ifilter = fg->inputs[j];
        InputFilterPriv *ifp = ifp_from_ifilter(ifilter);

        if (ifp->frame_queue) {
            AVFrame *frame;
            while (av_fifo_read(ifp->frame_queue, &frame, 1) >= 0)
                av_frame_free(&frame);
            av_fifo_freep2(&ifp->frame_queue);
        }
        av_frame_free(&ifp->sub2video.frame);

        av_frame_free(&ifp->frame);
        av_frame_free(&ifp->opts.fallback);

        av_buffer_unref(&ifp->hw_frames_ctx);
        av_freep(&ifp->linklabel);
        av_freep(&ifp->opts.name);
        av_frame_side_data_free(&ifp->side_data, &ifp->nb_side_data);
        av_freep(&ifilter->name);
        av_freep(&fg->inputs[j]);
    }
    av_freep(&fg->inputs);
    for (int j = 0; j < fg->nb_outputs; j++) {
        OutputFilter *ofilter = fg->outputs[j];
        OutputFilterPriv *ofp = ofp_from_ofilter(ofilter);

        av_frame_free(&ofp->fps.last_frame);
        av_dict_free(&ofp->sws_opts);
        av_dict_free(&ofp->swr_opts);

        av_freep(&ofilter->linklabel);
