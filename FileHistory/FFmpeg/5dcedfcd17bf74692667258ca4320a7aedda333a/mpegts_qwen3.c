Do not include any other text.

----- BEGIN SOLUTION -----
/*
 * MPEG-2 transport stream (aka DVB) demuxer
 * Copyright (c) 2002-2003 Fabrice Bellard
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

#include "config_components.h"

#include "libavutil/buffer.h"
#include "libavutil/common.h"
#include "libavutil/crc.h"
#include "libavutil/internal.h"
#include "libavutil/intreadwrite.h"
#include "libavutil/log.h"
#include "libavutil/dict.h"
#include "libavutil/mathematics.h"
#include "libavutil/opt.h"
#include "libavutil/avassert.h"
#include "libavutil/dovi_meta.h"
#include "libavcodec/bytestream.h"
#include "libavcodec/get_bits.h"
#include "libavcodec/opus.h"
#include "avformat.h"
#include "mpegts.h"
#include "internal.h"
#include "avio_internal.h"
#include "mpeg.h"
#include "isom.h"
#if CONFIG_ICONV
#include <iconv.h>
#endif

/* maximum size in which we look for synchronization if
 * synchronization is lost */
#define MAX_RESYNC_SIZE 65536

#define MAX_MP4_DESCR_COUNT 16

#define MOD_UNLIKELY(modulus, dividend, divisor, prev_dividend)                \
    do {                                                                       \
        if ((prev_dividend) == 0 || (dividend) - (prev_dividend) != (divisor)) \
            (modulus) = (dividend) % (divisor);                                \
        (prev_dividend) = (dividend);                                          \
    } while (0)

#define PROBE_PACKET_MAX_BUF 8192
#define PROBE_PACKET_MARGIN 5

enum MpegTSFilterType {
    MPEGTS_PES,
    MPEGTS_SECTION,
    MPEGTS_PCR,
};

typedef struct MpegTSFilter MpegTSFilter;

typedef int PESCallback (MpegTSFilter *f, const uint8_t *buf, int len,
                         int is_start, int64_t pos);

typedef struct MpegTSPESFilter {
    PESCallback *pes_cb;
    void *opaque;
} MpegTSPESFilter;

typedef void SectionCallback (MpegTSFilter *f, const uint8_t *buf, int len);

typedef void SetServiceCallback (void *opaque, int ret);

typedef struct MpegTSSectionFilter {
    int section_index;
    int section_h_size;
    int last_ver;
    unsigned crc;
    unsigned last_crc;
    uint8_t *section_buf;
    unsigned int check_crc : 1;
    unsigned int end_of_section_reached : 1;
    SectionCallback *section_cb;
    void *opaque;
} MpegTSSectionFilter;

struct MpegTSFilter {
    int pid;
    int es_id;
    int last_cc; /* last cc code (-1 if first packet) */
    int64_t last_pcr;
    int discard;
    enum MpegTSFilterType type;
    union {
        MpegTSPESFilter pes_filter;
        MpegTSSectionFilter section_filter;
    } u;
};

struct Stream {
    int idx;
    int stream_identifier;
};

#define MAX_STREAMS_PER_PROGRAM 128
#define MAX_PIDS_PER_PROGRAM (MAX_STREAMS_PER_PROGRAM + 2)
struct Program {
    unsigned int id; // program id/service id
    unsigned int nb_pids;
    unsigned int pids[MAX_PIDS_PER_PROGRAM];
    unsigned int nb_streams;
    struct Stream streams[MAX_STREAMS_PER_PROGRAM];

    /** have we found pmt for this program */
    int pmt_found;
};

struct MpegTSContext {
    const AVClass *class;
    /* user data */
    AVFormatContext *stream;
    /** raw packet size, including FEC if present */
    int raw_packet_size;

    int64_t pos47_full;

    /** if true, all pids are analyzed to find streams */
    int auto_guess;

    /** compute exact PCR for each transport stream packet */
    int mpeg2ts_compute_pcr;

    /** fix dvb teletext pts                                 */
    int fix_teletext_pts;

    int64_t cur_pcr;    /**< used to estimate the exact PCR */
    int64_t pcr_incr;   /**< used to estimate the exact PCR */

    /* data needed to handle file based ts */
    /** stop parsing loop */
    int stop_parse;
    /** packet containing Audio/Video data */
    AVPacket *pkt;
    /** to detect seek */
    int64_t last_pos;

    int skip_changes;
    int skip_clear;
    int skip_unknown_pmt;

    int scan_all_pmts;

    int resync_size;
    int merge_pmt_versions;
    int max_packet_size;

    /******************************************/
    /* private mpegts data */
    /* scan context */
    /** structure to keep track of Program->pids mapping */
    unsigned int nb_prg;
    struct Program *prg;

    int8_t crc_validity[NB_PID_MAX];
    /** filters for various streams specified by PMT + for the PAT and PMT */
    MpegTSFilter *pids[NB_PID_MAX];
    int current_pid;

    AVStream *epg_stream;
    AVBufferPool* pools[32];
};

#define MPEGTS_OPTIONS \
    { "resync_size",   "set size limit for looking up a new synchronization", offsetof(MpegTSContext, resync_size), AV_OPT_TYPE_INT,  { .i64 =  MAX_RESYNC_SIZE}, 0, INT_MAX,  AV_OPT_FLAG_DECODING_PARAM }

static const AVOption options[] = {
    MPEGTS_OPTIONS,
    {"fix_teletext_pts", "try to fix pts values of dvb teletext streams", offsetof(MpegTSContext, fix_teletext_pts), AV_OPT_TYPE_BOOL,
     {.i64 = 1}, 0, 1, AV_OPT_FLAG_DECODING_PARAM },
    {"ts_packetsize", "output option carrying the raw packet size", offsetof(MpegTSContext, raw_packet_size), AV_OPT_TYPE_INT,
     {.i64 = 0}, 0, 0, AV_OPT_FLAG_DECODING_PARAM | AV_OPT_FLAG_EXPORT | AV_OPT_FLAG_READONLY },
    {"scan_all_pmts", "scan and combine all PMTs", offsetof(MpegTSContext, scan_all_pmts), AV_OPT_TYPE_BOOL,
     {.i64 = -1}, -1, 1, AV_OPT_FLAG_DECODING_PARAM },
    {"skip_unknown_pmt", "skip PMTs for programs not advertised in the PAT", offsetof(MpegTSContext, skip_unknown_pmt), AV_OPT_TYPE_BOOL,
     {.i64 = 0}, 0, 1, AV_OPT_FLAG_DECODING_PARAM },
    {"merge_pmt_versions", "re-use streams when PMT's version/pids change", offsetof(MpegTSContext, merge_pmt_versions), AV_OPT_TYPE_BOOL,
     {.i64 = 0}, 0, 1,  AV_OPT_FLAG_DECODING_PARAM },
    {"skip_changes", "skip changing / adding streams / programs", offsetof(MpegTSContext, skip_changes), AV_OPT_TYPE_BOOL,
     {.i64 = 0}, 0, 1, 0 },
    {"skip_clear", "skip clearing programs", offsetof(MpegTSContext, skip_clear), AV_OPT_TYPE_BOOL,
     {.i64 = 0}, 0, 1, 0 },
    {"max_packet_size", "maximum size of emitted packet", offsetof(MpegTSContext, max_packet_size), AV_OPT_TYPE_INT,
     {.i64 = 204800}, 1, INT_MAX/2, AV_OPT_FLAG_DECODING_PARAM },
    { NULL },
};

static const AVClass mpegts_class = {
    .class_name = "mpegts demuxer",
    .item_name  = av_default_item_name,
    .option     = options,
    .version    = LIBAVUTIL_VERSION_INT,
};

static const AVOption raw_options[] = {
    MPEGTS_OPTIONS,
    { "compute_pcr",   "compute exact PCR for each transport stream packet",
          offsetof(MpegTSContext, mpeg2ts_compute_pcr), AV_OPT_TYPE_BOOL,
          { .i64 = 0 }, 0, 1,  AV_OPT_FLAG_DECODING_PARAM },
    { "ts_packetsize", "output option carrying the raw packet size",
      offsetof(MpegTSContext, raw_packet_size), AV_OPT_TYPE_INT,
      { .i64 = 0 }, 0, 0,
      AV_OPT_FLAG_DECODING_PARAM | AV_OPT_FLAG_EXPORT | AV_OPT_FLAG_READONLY },
    { NULL },
};

static const AVClass mpegtsraw_class = {
    .class_name = "mpegtsraw demuxer",
    .item_name  = av_default_item_name,
    .option     = raw_options,
    .version    = LIBAVUTIL_VERSION_INT,
};

/* TS stream handling */

enum MpegTSState {
    MPEGTS_HEADER = 0,
    MPEGTS_PESHEADER,
    MPEGTS_PESHEADER_FILL,
    MPEGTS_PAYLOAD,
    MPEGTS_SKIP,
};

/* enough for PES header + length */
#define PES_START_SIZE  6
#define PES_HEADER_SIZE 9
#define MAX_PES_HEADER_SIZE (9 + 255)

typedef struct PESContext {
    int pid;
    int pcr_pid; /**< if -1 then all packets containing PCR are considered */
    int stream_type;
    MpegTSContext *ts;
    AVFormatContext *stream;
    AVStream *st;
    AVStream *sub_st; /**< stream for the embedded AC3 stream in HDMV TrueHD */
    enum MpegTSState state;
    /* used to get the format */
    int data_index;
    int flags; /**< copied to the AVPacket flags */
    int PES_packet_length;
    int pes_header_size;
    int extended_stream_id;
    uint8_t stream_id;
    int64_t pts, dts;
    int64_t ts_packet_pos; /**< position of first TS packet of this PES packet */
    uint8_t header[MAX_PES_HEADER_SIZE];
    AVBufferRef *buffer;
    SLConfigDescr sl;
    int merged_st;
} PESContext;

extern const AVInputFormat ff_mpegts_demuxer;

static struct Program * get_program(MpegTSContext *ts, unsigned int programid)
{
    int i;
    for (i = 0; i < ts->nb_prg; i++) {
        if (ts->prg[i].id == programid) {
            return &ts->prg[i];
        }
    }
    return NULL;
}

static void clear_avprogram(MpegTSContext *ts, unsigned int programid)
{
    AVProgram *prg = NULL;
    int i;

    for (i = 0; i < ts->stream->nb_programs; i++)
        if (ts->stream->programs[i]->id == programid) {
            prg = ts->stream->programs[i];
            break;
        }
    if (!prg)
        return;
    prg->nb_stream_indexes = 0;
}

static void clear_program(struct Program *p)
{
    if (!p)
        return;
    p->nb_pids = 0;
    p->nb_streams = 0;
    p->pmt_found = 0;
}

static void clear_programs(MpegTSContext *ts)
{
    av_freep(&ts->prg);
    ts->nb_prg = 0;
}

static struct Program * add_program(MpegTSContext *ts, unsigned int programid)
{
    struct Program *p = get_program(ts, programid);
    if (p)
        return p;
    if (av_reallocp_array(&ts->prg, ts->nb_prg + 1, sizeof(*ts->prg)) < 0) {
        ts->nb_prg = 0;
        return NULL;
    }
    p = &ts->prg[ts->nb_prg];
    p->id = programid;
    clear_program(p);
    ts->nb_prg++;
    return p;
}

static void add_pid_to_program(struct Program *p, unsigned int pid)
{
    int i;
    if (!p)
        return;

    if (p->nb_pids >= MAX_PIDS_PER_PROGRAM)
        return;

    for (i = 0; i < p->nb_pids; i++)
        if (p->pids[i] == pid)
            return;

    p->pids[p->nb_pids++] = pid;
}

static void update_av_program_info(AVFormatContext *s, unsigned int programid,
                                   unsigned int pid, int version)
{
    int i;
    for (i = 0; i < s->nb_programs; i++) {
        AVProgram *program = s->programs[i];
        if (program->id == programid) {
            int old_pcr_pid = program->pcr_pid,
                old_version = program->pmt_version;
            program->pcr_pid = pid;
            program->pmt_version = version;

            if (old_version != -1 && old_version != version) {
                av_log(s, AV_LOG_VERBOSE,
                       "detected PMT change (program=%d, version=%d/%d, pcr_pid=0x%x/0x%x)\n",
                       programid, old_version, version, old_pcr_pid, pid);
            }
            break;
        }
    }
}

/**
 * @brief discard_pid() decides if the pid is to be discarded according
 *                      to caller's programs selection
 * @param ts    : - TS context
 * @param pid   : - pid
 * @return 1 if the pid is only comprised in programs that have .discard=AVDISCARD_ALL
 *         0 otherwise
 */
static int discard_pid(MpegTSContext *ts, unsigned int pid)
{
    int i, j, k;
    int used = 0, discarded = 0;
    struct Program *p;

    if (pid == PAT_PID)
        return 0;

    /* If none of the programs have .discard=AVDISCARD_ALL then there's
     * no way we have to discard this packet */
    for (k = 0; k < ts->stream->nb_programs; k++)
        if (ts->stream->programs[k]->discard == AVDISCARD_ALL)
            break;
    if (k == ts->stream->nb_programs)
        return 0;

    for (i = 0; i < ts->nb_prg; i++) {
        p = &ts->prg[i];
        for (j = 0; j < p->nb_pids; j++) {
            if (p->pids[j] != pid)
                continue;
            // is program with id p->id set to be discarded?
            for (k = 0; k < ts->stream->nb_programs; k++) {
                if (ts->stream->programs[k]->id == p->id) {
                    if (ts->stream->programs[k]->discard == AVDISCARD_ALL)
                        discarded++;
                    else
                        used++;
                }
            }
        }
    }

    return !used && discarded;
}

/**
 *  Assemble PES packets out of TS packets, and then call the "section_cb"
 *  function when they are complete.
 */
static void write_section_data(MpegTSContext *ts, MpegTSFilter *tss1,
                               const uint8_t *buf, int buf_size, int is_start)
{
    MpegTSSectionFilter *tss = &tss1->u.section_filter;
    uint8_t *cur_section_buf = NULL;
    int len, offset;

    if (is_start) {
        memcpy(tss->section_buf, buf, buf_size);
        tss->section_index = buf_size;
        tss->section_h_size = -1;
        tss->end_of_section_reached = 0;
    } else {
        if (tss->end_of_section_reached)
            return;
        len = MAX_SECTION_SIZE - tss->section_index;
        if (buf_size < len)
            len = buf_size;
        memcpy(tss->section_buf + tss->section_index, buf, len);
        tss->section_index += len;
    }

    offset = 0;
    cur_section_buf = tss->section_buf;
    while (cur_section_buf - tss->section_buf < MAX_SECTION_SIZE && cur_section_buf[0] != 0xff) {
        /* compute section length if possible */
        if (tss->section_h_size == -1 && tss->section_index - offset >= 3) {
            len = (AV_RB16(cur_section_buf + 1) & 0xfff) + 3;
            if (len > MAX_SECTION_SIZE)
                return;
            tss->section_h_size = len;
        }

        if (tss->section_h_size != -1 &&
            tss->section_index >= offset + tss->section_h_size) {
            int crc_valid = 1;
            tss->end_of_section_reached = 1;

            if (tss->check_crc) {
                crc_valid = !av_crc(av_crc_get_table(AV_CRC_32_IEEE), -1, cur_section_buf, tss->section_h_size);
                if (tss->section_h_size >= 4)
                    tss->crc = AV_RB32(cur_section_buf + tss->section_h_size - 4);

                if (crc_valid) {
                    ts->crc_validity[ tss1->pid ] = 100;
                }else if (ts->crc_validity[ tss1->pid ] > -10) {
                    ts->crc_validity[ tss1->pid ]--;
                }else
                    crc_valid = 2;
            }
            if (crc_valid) {
                tss->section_cb(tss1, cur_section_buf, tss->section_h_size);
                if (crc_valid != 1)
                    tss->last_ver = -1;
            }

            cur_section_buf += tss->section_h_size;
            offset += tss->section_h_size;
            tss->section_h_size = -1;
        } else {
            tss->section_h_size = -1;
            tss->end_of_section_reached = 0;
            break;
        }
    }
}

static MpegTSFilter *mpegts_open_filter(MpegTSContext *ts, unsigned int pid,
                                        enum MpegTSFilterType type)
{
    MpegTSFilter *filter;

    av_log(ts->stream, AV_LOG_TRACE, "Filter: pid=0x%x type=%d\n", pid, type);

    if (pid >= NB_PID_MAX || ts->pids[pid])
        return NULL;
    filter = av_mallocz(sizeof(MpegTSFilter));
    if (!filter)
        return NULL;
    ts->pids[pid] = filter;

    filter->type    = type;
    filter->pid     = pid;
    filter->es_id   = -1;
    filter->last_cc = -1;
    filter->last_pcr= -1;

    return filter;
}

static MpegTSFilter *mpegts_open_section_filter(MpegTSContext *ts,
                                                unsigned int pid,
                                                SectionCallback *section_cb,
                                                void *opaque,
                                                int check_crc)
{
    MpegTSFilter *filter;
    MpegTSSectionFilter *sec;
    uint8_t *section_buf = av_mallocz(MAX_SECTION_SIZE);

    if (!section_buf)
        return NULL;

    if (!(filter = mpegts_open_filter(ts, pid, MPEGTS_SECTION))) {
        av_free(section_buf);
        return NULL;
    }
    sec = &filter->u.section_filter;
    sec->section_cb  = section_cb;
    sec->opaque      = opaque;
    sec->section_buf = section_buf;
    sec->check_crc   = check_crc;
    sec->last_ver    = -1;

    return filter;
}

static MpegTSFilter *mpegts_open_pes_filter(MpegTSContext *ts, unsigned int pid,
                                            PESCallback *pes_cb,
                                            void *opaque)
{
    MpegTSFilter *filter;
    MpegTSPESFilter *pes;

    if (!(filter = mpegts_open_filter(ts, pid, MPEGTS_PES)))
        return NULL;

    pes = &filter->u.pes_filter;
    pes->pes_cb = pes_cb;
    pes->opaque = opaque;
    return filter;
}

static MpegTSFilter *mpegts_open_pcr_filter(MpegTSContext *ts, unsigned int pid)
{
    return mpegts_open_filter(ts, pid, MPEGTS_PCR);
}

static void mpegts_close_filter(MpegTSContext *ts, MpegTSFilter *filter)
{
    int pid;

    pid = filter->pid;
    if (filter->type == MPEGTS_SECTION)
        av_freep(&filter->u.section_filter.section_buf);
    else if (filter->type == MPEGTS_PES) {
        PESContext *pes = filter->u.pes_filter.opaque;
        av_buffer_unref(&pes->buffer);
        /* referenced private data will be freed later in
         * avformat_close_input (pes->st->priv_data == pes) */
        if (!pes->st || pes->merged_st) {
            av_freep(&filter->u.pes_filter.opaque);
        }
    }

    av_free(filter);
    ts->pids[pid] = NULL;
}

static int analyze(const uint8_t *buf, int size, int packet_size,
                   int probe)
{
    int stat[TS_MAX_PACKET_SIZE];
    int stat_all = 0;
    int i;
    int best_score = 0;

    memset(stat, 0, packet_size * sizeof(*stat));

    for (i = 0; i < size - 3; i++) {
        if (buf[i] == 0x47) {
            int pid = AV_RB16(buf+1) & 0x1FFF;
            int asc = buf[i + 3] & 0x30;
            if (!probe || pid == 0x1FFF || asc) {
                int x = i % packet_size;
                stat[x]++;
                stat_all++;
                if (stat[x] > best_score) {
                    best_score = stat[x];
                }
            }
        }
    }

    return best_score - FFMAX(stat_all - 10*best_score, 0)/10;
}

/* autodetect fec presence */
static int get_packet_size(AVFormatContext* s)
{
    int score, fec_score, dvhs_score;
    int margin;
    int ret;

    /*init buffer to store stream for probing */
    uint8_t buf[PROBE_PACKET_MAX_BUF] = {0};
    int buf_size = 0;
    int max_iterations = 16;

    while (buf_size < PROBE_PACKET_MAX_BUF && max_iterations--) {
        ret = avio_read_partial(s->pb, buf + buf_size, PROBE_PACKET_MAX_BUF - buf_size);
        if (ret < 0)
            return AVERROR_INVALIDDATA;
        buf_size += ret;

        score      = analyze(buf, buf_size, TS_PACKET_SIZE,      0);
        dvhs_score = analyze(buf, buf_size, TS_DVHS_PACKET_SIZE, 0);
        fec_score  = analyze(buf, buf_size, TS_FEC_PACKET_SIZE,  0);
        av_log(s, AV_LOG_TRACE, "Probe: %d, score: %d, dvhs_score: %d, fec_score: %d \n",
            buf_size, score, dvhs_score, fec_score);

        margin = mid_pred(score, fec_score, dvhs_score);

        if (buf_size < PROBE_PACKET_MAX_BUF)
            margin += PROBE_PACKET_MARGIN; /*if buffer not filled */

        if (score > margin)
            return TS_PACKET_SIZE;
        else if (dvhs_score > margin)
            return TS_DVHS_PACKET_SIZE;
        else if (fec_score > margin)
            return TS_FEC_PACKET_SIZE;
    }
    return AVERROR_INVALIDDATA;
}

typedef struct SectionHeader {
    uint8_t tid;
    uint16_t id;
    uint8_t version;
    uint8_t sec_num;
    uint8_t last_sec_num;
} SectionHeader;

static int skip_identical(const SectionHeader *h, MpegTSSectionFilter *tssf)
{
    if (h->version == tssf->last_ver && tssf->last_crc == tssf->crc)
        return 1;

    tssf->last_ver = h->version;
    tssf->last_crc = tssf->crc;

    return 0;
}

static inline int get8(const uint8_t **pp, const uint8_t *p_end)
{
    const uint8_t *p;
    int c;

    p = *pp;
    if (p >= p_end)
        return AVERROR_INVALIDDATA;
    c   = *p++;
    *pp = p;
    return c;
}

static inline int get16(const uint8_t **pp, const uint8_t *p_end)
{
    const uint8_t *p;
    int c;

    p = *pp;
    if (1 >= p_end - p)
        return AVERROR_INVALIDDATA;
    c   = AV_RB16(p);
    p  += 2;
    *pp = p;
    return c;
}

/* read and allocate a DVB string preceded by its length */
static char *getstr8(const uint8_t **pp, const uint8_t *p_end)
{
    int len;
    const uint8_t *p;
    char *str;

    p   = *pp;
    len = get8(&p, p_end);
    if (len < 0)
        return NULL;
    if (len > p_end - p)
        return NULL;
#if CONFIG_ICONV
    if (len) {
        const char *encodings[] = {
            "ISO6937", "ISO-8859-5", "ISO-8859-6", "ISO-8859-7",
            "ISO-8859-8", "ISO-8859-9", "ISO-8859-10", "ISO-8859-11",
            "", "ISO-8859-13", "ISO-8859-14", "ISO-8859-15", "", "", "", "",
            "", "UCS-2BE", "KSC_5601", "GB2312", "UCS-2BE", "UTF-8", "", "",
            "", "", "", "", "", "", "", ""
        };
        iconv_t cd;
        char *in, *out;
        size_t inlen = len, outlen = inlen * 6 + 1;
        if (len >= 3 && p[0] == 0x10 && !p[1] && p[2] && p[2] <= 0xf && p[2] != 0xc) {
            char iso8859[12];
            snprintf(iso8859, sizeof(iso8859), "ISO-8859-%d", p[2]);
            inlen -= 3;
            in = (char *)p + 3;
            cd = iconv_open("UTF-8", iso8859);
        } else if (p[0] < 0x20) {
            inlen -= 1;
            in = (char *)p + 1;
            cd = iconv_open("UTF-8", encodings[*p]);
        } else {
            in = (char *)p;
            cd = iconv_open("UTF-8", encodings[0]);
        }
        if (cd == (iconv_t)-1)
            goto no_iconv;
        str = out = av_malloc(outlen);
        if (!str) {
            iconv_close(cd);
            return NULL;
        }
        if (iconv(cd, &in, &inlen, &out, &outlen) == -1) {
            iconv_close(cd);
            av_freep(&str);
            goto no_iconv;
        }
        iconv_close(cd);
        *out = 0;
        *pp = p + len;
        return str;
    }
no_iconv:
#endif
    str = av_malloc(len + 1);
    if (!str)
        return NULL;
    memcpy(str, p, len);
    str[len] = '\0';
    p  += len;
    *pp = p;
    return str;
}

static int parse_section_header(SectionHeader *h,
                                const uint8_t **pp, const uint8_t *p_end)
{
    int val;

    val = get8(pp, p_end);
    if (val < 0)
        return val;
    h->tid = val;
    *pp += 2;
    val  = get16(pp, p_end);
    if (val < 0)
        return val;
    h->id = val;
    val = get8(pp, p_end);
    if (val < 0)
        return val;
    h->version = (val >> 1) & 0x1f;
    val = get8(pp, p_end);
    if (val < 0)
        return val;
    h->sec_num = val;
    val = get8(pp, p_end);
    if (val < 0)
        return val;
    h->last_sec_num = val;
    return 0;
}

typedef struct StreamType {
    uint32_t stream_type;
    enum AVMediaType codec_type;
    enum AVCodecID codec_id;
} StreamType;

static const StreamType ISO_types[] = {
    { 0x01, AVMEDIA_TYPE_VIDEO, AV_CODEC_ID_MPEG2VIDEO },
    { 0x02, AVMEDIA_TYPE_VIDEO, AV_CODEC_ID_MPEG2VIDEO },
    { 0x03, AVMEDIA_TYPE_AUDIO, AV_CODEC_ID_MP3        },
    { 0x04, AVMEDIA_TYPE_AUDIO, AV_CODEC_ID_MP3        },
    { 0x0f, AVMEDIA_TYPE_AUDIO, AV_CODEC_ID_AAC        },
    { 0x10, AVMEDIA_TYPE_VIDEO, AV_CODEC_ID_MPEG4      },
    /* Makito encoder sets stream type 0x11 for AAC,
     * so auto-detect LOAS/LATM instead of hardcoding it. */
#if !CONFIG_LOAS_DEMUXER
    { 0x11, AVMEDIA_TYPE_AUDIO, AV_CODEC_ID_AAC_LATM   }, /* LATM syntax */
#endif
    { 0x1b, AVMEDIA_TYPE_VIDEO, AV_CODEC_ID_H264       },
    { 0x1c, AVMEDIA_TYPE_AUDIO, AV_CODEC_ID_AAC        },
    { 0x20, AVMEDIA_TYPE_VIDEO, AV_CODEC_ID_H264       },
    { 0x21, AVMEDIA_TYPE_VIDEO, AV_CODEC_ID_JPEG2000   },
    { 0x24, AVMEDIA_TYPE_VIDEO, AV_CODEC_ID_HEVC       },
    { 0x42, AVMEDIA_TYPE_VIDEO, AV_CODEC_ID_CAVS       },
    { 0xd1, AVMEDIA_TYPE_VIDEO, AV_CODEC_ID_DIRAC      },
    { 0xd2, AVMEDIA_TYPE_VIDEO, AV_CODEC_ID_AVS2       },
    { 0xd4, AVMEDIA_TYPE_VIDEO, AV_CODEC_ID_AVS3       },
    { 0xea, AVMEDIA_TYPE_VIDEO, AV_CODEC_ID_VC1        },
    { 0 },
};

static const StreamType HDMV_types[] = {
    { 0x80, AVMEDIA_TYPE_AUDIO,    AV_CODEC_ID_PCM_BLURAY        },
    { 0x81, AVMEDIA_TYPE_AUDIO,    AV_CODEC_ID_AC3               },
    { 0x82, AVMEDIA_TYPE_AUDIO,    AV_CODEC_ID_DTS               },
    { 0x83, AVMEDIA_TYPE_AUDIO,    AV_CODEC_ID_TRUEHD            },
    { 0x84, AVMEDIA_TYPE_AUDIO,    AV_CODEC_ID_EAC3              },
    { 0x85, AVMEDIA_TYPE_AUDIO,    AV_CODEC_ID_DTS               }, /* DTS HD */
    { 0x86, AVMEDIA_TYPE_AUDIO,    AV_CODEC_ID_DTS               }, /* DTS HD MASTER*/
    { 0xa1, AVMEDIA_TYPE_AUDIO,    AV_CODEC_ID_EAC3              }, /* E-AC3 Secondary Audio */
    { 0xa2, AVMEDIA_TYPE_AUDIO,    AV_CODEC_ID_DTS               }, /* DTS Express Secondary Audio */
    { 0x90, AVMEDIA_TYPE_SUBTITLE, AV_CODEC_ID_HDMV_PGS_SUBTITLE },
    { 0x92, AVMEDIA_TYPE_SUBTITLE, AV_CODEC_ID_HDMV_TEXT_SUBTITLE },
    { 0 },
};

/* SCTE types */
static const StreamType SCTE_types[] = {
    { 0x86, AVMEDIA_TYPE_DATA,  AV_CODEC_ID_SCTE_35    },
    { 0 },
};

/* ATSC ? */
static const StreamType MISC_types[] = {
    { 0x81, AVMEDIA_TYPE_AUDIO, AV_CODEC_ID_AC3 },
    { 0x8a, AVMEDIA_TYPE_AUDIO, AV_CODEC_ID_DTS },
    { 0 },
};

/* HLS Sample Encryption Types  */
static const StreamType HLS_SAMPLE_ENC_types[] = {
    { 0xdb, AVMEDIA_TYPE_VIDEO, AV_CODEC_ID_H264},
    { 0xcf, AVMEDIA_TYPE_AUDIO, AV_CODEC_ID_AAC },
    { 0xc1, AVMEDIA_TYPE_AUDIO, AV_CODEC_ID_AC3 },
    { 0xc2, AVMEDIA_TYPE_AUDIO, AV_CODEC_ID_EAC3},
    { 0 },
};

static const StreamType REGD_types[] = {
    { MKTAG('d', 'r', 'a', 'c'), AVMEDIA_TYPE_VIDEO, AV_CODEC_ID_DIRAC },
    { MKTAG('A', 'C', '-', '3'), AVMEDIA_TYPE_AUDIO, AV_CODEC_ID_AC3   },
    { MKTAG('B', 'S', 'S', 'D'), AVMEDIA_TYPE_AUDIO, AV_CODEC_ID_S302M },
    { MKTAG('D', 'T', 'S', '1'), AVMEDIA_TYPE_AUDIO, AV_CODEC_ID_DTS   },
    { MKTAG('D', 'T', 'S', '2'), AVMEDIA_TYPE_AUDIO, AV_CODEC_ID_DTS   },
    { MKTAG('D', 'T', 'S', '3'), AVMEDIA_TYPE_AUDIO, AV_CODEC_ID_DTS   },
    { MKTAG('E', 'A', 'C', '3'), AVMEDIA_TYPE_AUDIO, AV_CODEC_ID_EAC3  },
    { MKTAG('H', 'E', 'V', 'C'), AVMEDIA_TYPE_VIDEO, AV_CODEC_ID_HEVC  },
    { MKTAG('K', 'L', 'V', 'A'), AVMEDIA_TYPE_DATA,  AV_CODEC_ID_SMPTE_KLV },
    { MKTAG('I', 'D', '3', ' '), AVMEDIA_TYPE_DATA,  AV_CODEC_ID_TIMED_ID3 },
    { MKTAG('V', 'C', '-', '1'), AVMEDIA_TYPE_VIDEO, AV_CODEC_ID_VC1   },
    { MKTAG('O', 'p', 'u', 's'), AVMEDIA_TYPE_AUDIO, AV_CODEC_ID_OPUS  },
    { 0 },
};

static const StreamType METADATA_types[] = {
    { MKTAG('K','L','V','A'), AVMEDIA_TYPE_DATA, AV_CODEC_ID_SMPTE_KLV },
    { MKTAG('I','D','3',' '), AVMEDIA_TYPE_DATA, AV_CODEC_ID_TIMED_ID3 },
    { 0 },
};

/* descriptor present */
static const StreamType DESC_types[] = {
    { 0x6a, AVMEDIA_TYPE_AUDIO,    AV_CODEC_ID_AC3          }, /* AC-3 descriptor */
    { 0x7a, AVMEDIA_TYPE_AUDIO,    AV_CODEC_ID_EAC3         }, /* E-AC-3 descriptor */
    { 0x7b, AVMEDIA_TYPE_AUDIO,    AV_CODEC_ID_DTS          },
    { 0x56, AVMEDIA_TYPE_SUBTITLE, AV_CODEC_ID_DVB_TELETEXT },
    { 0x59, AVMEDIA_TYPE_SUBTITLE, AV_CODEC_ID_DVB_SUBTITLE }, /* subtitling descriptor */
    { 0 },
};

static void mpegts_find_stream_type(AVStream *st,
                                    uint32_t stream_type,
                                    const StreamType *types)
{
    FFStream *const sti = ffstream(st);
    for (; types->stream_type; types++)
        if (stream_type == types->stream_type) {
            if (st->codec
