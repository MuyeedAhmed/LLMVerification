Do not include anything else.

----- BEGIN MODIFIED C SOURCE CODE -----
/*
 * MOV, 3GP, MP4 muxer
 * Copyright (c) 2003 Thomas Raivio
 * Copyright (c) 2004 Gildas Bazin <gbazin at videolan dot org>
 * Copyright (c) 2009 Baptiste Coudurier <baptiste dot coudurier at gmail dot com>
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

#include <stdint.h>
#include <inttypes.h>

#include "movenc.h"
#include "avformat.h"
#include "avio_internal.h"
#include "dovi_isom.h"
#include "riff.h"
#include "avio.h"
#include "isom.h"
#include "av1.h"
#include "avc.h"
#include "libavcodec/ac3_parser_internal.h"
#include "libavcodec/dnxhddata.h"
#include "libavcodec/flac.h"
#include "libavcodec/get_bits.h"

#include "libavcodec/internal.h"
#include "libavcodec/put_bits.h"
#include "libavcodec/vc1_common.h"
#include "libavcodec/raw.h"
#include "internal.h"
#include "libavutil/avstring.h"
#include "libavutil/channel_layout.h"
#include "libavutil/intfloat.h"
#include "libavutil/mathematics.h"
#include "libavutil/libm.h"
#include "libavutil/opt.h"
#include "libavutil/dict.h"
#include "libavutil/pixdesc.h"
#include "libavutil/stereo3d.h"
#include "libavutil/timecode.h"
#include "libavutil/dovi_meta.h"
#include "libavutil/color_utils.h"
#include "hevc.h"
#include "rtpenc.h"
#include "mov_chan.h"
#include "movenc_ttml.h"
#include "ttmlenc.h"
#include "version.h"
#include "vpcc.h"

static const AVOption options[] = {
    { "movflags", "MOV muxer flags", offsetof(MOVMuxContext, flags), AV_OPT_TYPE_FLAGS, {.i64 = 0}, INT_MIN, INT_MAX, AV_OPT_FLAG_ENCODING_PARAM, "movflags" },
    { "rtphint", "Add RTP hint tracks", 0, AV_OPT_TYPE_CONST, {.i64 = FF_MOV_FLAG_RTP_HINT}, INT_MIN, INT_MAX, AV_OPT_FLAG_ENCODING_PARAM, "movflags" },
    { "moov_size", "maximum moov size so it can be placed at the begin", offsetof(MOVMuxContext, reserved_moov_size), AV_OPT_TYPE_INT, {.i64 = 0}, 0, INT_MAX, AV_OPT_FLAG_ENCODING_PARAM, 0 },
    { "empty_moov", "Make the initial moov atom empty", 0, AV_OPT_TYPE_CONST, {.i64 = FF_MOV_FLAG_EMPTY_MOOV}, INT_MIN, INT_MAX, AV_OPT_FLAG_ENCODING_PARAM, "movflags" },
    { "frag_keyframe", "Fragment at video keyframes", 0, AV_OPT_TYPE_CONST, {.i64 = FF_MOV_FLAG_FRAG_KEYFRAME}, INT_MIN, INT_MAX, AV_OPT_FLAG_ENCODING_PARAM, "movflags" },
    { "frag_every_frame", "Fragment at every frame", 0, AV_OPT_TYPE_CONST, {.i64 = FF_MOV_FLAG_FRAG_EVERY_FRAME}, INT_MIN, INT_MAX, AV_OPT_FLAG_ENCODING_PARAM, "movflags" },
    { "separate_moof", "Write separate moof/mdat atoms for each track", 0, AV_OPT_TYPE_CONST, {.i64 = FF_MOV_FLAG_SEPARATE_MOOF}, INT_MIN, INT_MAX, AV_OPT_FLAG_ENCODING_PARAM, "movflags" },
    { "frag_custom", "Flush fragments on caller requests", 0, AV_OPT_TYPE_CONST, {.i64 = FF_MOV_FLAG_FRAG_CUSTOM}, INT_MIN, INT_MAX, AV_OPT_FLAG_ENCODING_PARAM, "movflags" },
    { "isml", "Create a live smooth streaming feed (for pushing to a publishing point)", 0, AV_OPT_TYPE_CONST, {.i64 = FF_MOV_FLAG_ISML}, INT_MIN, INT_MAX, AV_OPT_FLAG_ENCODING_PARAM, "movflags" },
    { "faststart", "Run a second pass to put the index (moov atom) at the beginning of the file", 0, AV_OPT_TYPE_CONST, {.i64 = FF_MOV_FLAG_FASTSTART}, INT_MIN, INT_MAX, AV_OPT_FLAG_ENCODING_PARAM, "movflags" },
    { "omit_tfhd_offset", "Omit the base data offset in tfhd atoms", 0, AV_OPT_TYPE_CONST, {.i64 = FF_MOV_FLAG_OMIT_TFHD_OFFSET}, INT_MIN, INT_MAX, AV_OPT_FLAG_ENCODING_PARAM, "movflags" },
    { "disable_chpl", "Disable Nero chapter atom", 0, AV_OPT_TYPE_CONST, {.i64 = FF_MOV_FLAG_DISABLE_CHPL}, INT_MIN, INT_MAX, AV_OPT_FLAG_ENCODING_PARAM, "movflags" },
    { "default_base_moof", "Set the default-base-is-moof flag in tfhd atoms", 0, AV_OPT_TYPE_CONST, {.i64 = FF_MOV_FLAG_DEFAULT_BASE_MOOF}, INT_MIN, INT_MAX, AV_OPT_FLAG_ENCODING_PARAM, "movflags" },
    { "dash", "Write DASH compatible fragmented MP4", 0, AV_OPT_TYPE_CONST, {.i64 = FF_MOV_FLAG_DASH}, INT_MIN, INT_MAX, AV_OPT_FLAG_ENCODING_PARAM, "movflags" },
    { "cmaf", "Write CMAF compatible fragmented MP4", 0, AV_OPT_TYPE_CONST, {.i64 = FF_MOV_FLAG_CMAF}, INT_MIN, INT_MAX, AV_OPT_FLAG_ENCODING_PARAM, "movflags" },
    { "frag_discont", "Signal that the next fragment is discontinuous from earlier ones", 0, AV_OPT_TYPE_CONST, {.i64 = FF_MOV_FLAG_FRAG_DISCONT}, INT_MIN, INT_MAX, AV_OPT_FLAG_ENCODING_PARAM, "movflags" },
    { "delay_moov", "Delay writing the initial moov until the first fragment is cut, or until the first fragment flush", 0, AV_OPT_TYPE_CONST, {.i64 = FF_MOV_FLAG_DELAY_MOOV}, INT_MIN, INT_MAX, AV_OPT_FLAG_ENCODING_PARAM, "movflags" },
    { "global_sidx", "Write a global sidx index at the start of the file", 0, AV_OPT_TYPE_CONST, {.i64 = FF_MOV_FLAG_GLOBAL_SIDX}, INT_MIN, INT_MAX, AV_OPT_FLAG_ENCODING_PARAM, "movflags" },
    { "skip_sidx", "Skip writing of sidx atom", 0, AV_OPT_TYPE_CONST, {.i64 = FF_MOV_FLAG_SKIP_SIDX}, INT_MIN, INT_MAX, AV_OPT_FLAG_ENCODING_PARAM, "movflags" },
    { "write_colr", "Write colr atom even if the color info is unspecified (Experimental, may be renamed or changed, do not use from scripts)", 0, AV_OPT_TYPE_CONST, {.i64 = FF_MOV_FLAG_WRITE_COLR}, INT_MIN, INT_MAX, AV_OPT_FLAG_ENCODING_PARAM, "movflags" },
    { "prefer_icc", "If writing colr atom prioritise usage of ICC profile if it exists in stream packet side data", 0, AV_OPT_TYPE_CONST, {.i64 = FF_MOV_FLAG_PREFER_ICC}, INT_MIN, INT_MAX, AV_OPT_FLAG_ENCODING_PARAM, "movflags" },
    { "write_gama", "Write deprecated gama atom", 0, AV_OPT_TYPE_CONST, {.i64 = FF_MOV_FLAG_WRITE_GAMA}, INT_MIN, INT_MAX, AV_OPT_FLAG_ENCODING_PARAM, "movflags" },
    { "use_metadata_tags", "Use mdta atom for metadata.", 0, AV_OPT_TYPE_CONST, {.i64 = FF_MOV_FLAG_USE_MDTA}, INT_MIN, INT_MAX, AV_OPT_FLAG_ENCODING_PARAM, "movflags" },
    { "skip_trailer", "Skip writing the mfra/tfra/mfro trailer for fragmented files", 0, AV_OPT_TYPE_CONST, {.i64 = FF_MOV_FLAG_SKIP_TRAILER}, INT_MIN, INT_MAX, AV_OPT_FLAG_ENCODING_PARAM, "movflags" },
    { "negative_cts_offsets", "Use negative CTS offsets (reducing the need for edit lists)", 0, AV_OPT_TYPE_CONST, {.i64 = FF_MOV_FLAG_NEGATIVE_CTS_OFFSETS}, INT_MIN, INT_MAX, AV_OPT_FLAG_ENCODING_PARAM, "movflags" },
    FF_RTP_FLAG_OPTS(MOVMuxContext, rtp_flags),
    { "skip_iods", "Skip writing iods atom.", offsetof(MOVMuxContext, iods_skip), AV_OPT_TYPE_BOOL, {.i64 = 1}, 0, 1, AV_OPT_FLAG_ENCODING_PARAM},
    { "iods_audio_profile", "iods audio profile atom.", offsetof(MOVMuxContext, iods_audio_profile), AV_OPT_TYPE_INT, {.i64 = -1}, -1, 255, AV_OPT_FLAG_ENCODING_PARAM},
    { "iods_video_profile", "iods video profile atom.", offsetof(MOVMuxContext, iods_video_profile), AV_OPT_TYPE_INT, {.i64 = -1}, -1, 255, AV_OPT_FLAG_ENCODING_PARAM},
    { "frag_duration", "Maximum fragment duration", offsetof(MOVMuxContext, max_fragment_duration), AV_OPT_TYPE_INT, {.i64 = 0}, 0, INT_MAX, AV_OPT_FLAG_ENCODING_PARAM},
    { "min_frag_duration", "Minimum fragment duration", offsetof(MOVMuxContext, min_fragment_duration), AV_OPT_TYPE_INT, {.i64 = 0}, 0, INT_MAX, AV_OPT_FLAG_ENCODING_PARAM},
    { "frag_size", "Maximum fragment size", offsetof(MOVMuxContext, max_fragment_size), AV_OPT_TYPE_INT, {.i64 = 0}, 0, INT_MAX, AV_OPT_FLAG_ENCODING_PARAM},
    { "ism_lookahead", "Number of lookahead entries for ISM files", offsetof(MOVMuxContext, ism_lookahead), AV_OPT_TYPE_INT, {.i64 = 0}, 0, 255, AV_OPT_FLAG_ENCODING_PARAM},
    { "video_track_timescale", "set timescale of all video tracks", offsetof(MOVMuxContext, video_track_timescale), AV_OPT_TYPE_INT, {.i64 = 0}, 0, INT_MAX, AV_OPT_FLAG_ENCODING_PARAM},
    { "brand",    "Override major brand", offsetof(MOVMuxContext, major_brand),   AV_OPT_TYPE_STRING, {.str = NULL}, .flags = AV_OPT_FLAG_ENCODING_PARAM },
    { "use_editlist", "use edit list", offsetof(MOVMuxContext, use_editlist), AV_OPT_TYPE_BOOL, {.i64 = -1}, -1, 1, AV_OPT_FLAG_ENCODING_PARAM},
    { "fragment_index", "Fragment number of the next fragment", offsetof(MOVMuxContext, fragments), AV_OPT_TYPE_INT, {.i64 = 1}, 1, INT_MAX, AV_OPT_FLAG_ENCODING_PARAM},
    { "mov_gamma", "gamma value for gama atom", offsetof(MOVMuxContext, gamma), AV_OPT_TYPE_FLOAT, {.dbl = 0.0 }, 0.0, 10, AV_OPT_FLAG_ENCODING_PARAM},
    { "frag_interleave", "Interleave samples within fragments (max number of consecutive samples, lower is tighter interleaving, but with more overhead)", offsetof(MOVMuxContext, frag_interleave), AV_OPT_TYPE_INT, {.i64 = 0}, 0, INT_MAX, AV_OPT_FLAG_ENCODING_PARAM },
    { "encryption_scheme",    "Configures the encryption scheme, allowed values are none, cenc-aes-ctr", offsetof(MOVMuxContext, encryption_scheme_str),   AV_OPT_TYPE_STRING, {.str = NULL}, .flags = AV_OPT_FLAG_ENCODING_PARAM },
    { "encryption_key", "The media encryption key (hex)", offsetof(MOVMuxContext, encryption_key), AV_OPT_TYPE_BINARY, .flags = AV_OPT_FLAG_ENCODING_PARAM },
    { "encryption_kid", "The media encryption key identifier (hex)", offsetof(MOVMuxContext, encryption_kid), AV_OPT_TYPE_BINARY, .flags = AV_OPT_FLAG_ENCODING_PARAM },
    { "use_stream_ids_as_track_ids", "use stream ids as track ids", offsetof(MOVMuxContext, use_stream_ids_as_track_ids), AV_OPT_TYPE_BOOL, {.i64 = 0}, 0, 1, AV_OPT_FLAG_ENCODING_PARAM},
    { "write_tmcd", "force or disable writing tmcd", offsetof(MOVMuxContext, write_tmcd), AV_OPT_TYPE_BOOL, {.i64 = -1}, -1, 1, AV_OPT_FLAG_ENCODING_PARAM},
    { "write_prft", "Write producer reference time box with specified time source", offsetof(MOVMuxContext, write_prft), AV_OPT_TYPE_INT, {.i64 = MOV_PRFT_NONE}, 0, MOV_PRFT_NB-1, AV_OPT_FLAG_ENCODING_PARAM, "prft"},
    { "wallclock", NULL, 0, AV_OPT_TYPE_CONST, {.i64 = MOV_PRFT_SRC_WALLCLOCK}, 0, 0, AV_OPT_FLAG_ENCODING_PARAM, "prft"},
    { "pts", NULL, 0, AV_OPT_TYPE_CONST, {.i64 = MOV_PRFT_SRC_PTS}, 0, 0, AV_OPT_FLAG_ENCODING_PARAM, "prft"},
    { "empty_hdlr_name", "write zero-length name string in hdlr atoms within mdia and minf atoms", offsetof(MOVMuxContext, empty_hdlr_name), AV_OPT_TYPE_BOOL, {.i64 = 0}, 0, 1, AV_OPT_FLAG_ENCODING_PARAM},
    { "movie_timescale", "set movie timescale", offsetof(MOVMuxContext, movie_timescale), AV_OPT_TYPE_INT, {.i64 = MOV_TIMESCALE}, 1, INT_MAX, AV_OPT_FLAG_ENCODING_PARAM},
    { NULL },
};

static const AVClass mov_isobmff_muxer_class = {
    .class_name = "mov/mp4/tgp/psp/tg2/ipod/ismv/f4v muxer",
    .item_name  = av_default_item_name,
    .option     = options,
    .version    = LIBAVUTIL_VERSION_INT,
};

static int get_moov_size(AVFormatContext *s);
static int mov_write_single_packet(AVFormatContext *s, AVPacket *pkt);

static int utf8len(const uint8_t *b)
{
    int len = 0;
    int val;
    while (*b) {
        GET_UTF8(val, *b++, return -1;)
        len++;
    }
    return len;
}

//FIXME support 64 bit variant with wide placeholders
static int64_t update_size(AVIOContext *pb, int64_t pos)
{
    int64_t curpos = avio_tell(pb);
    avio_seek(pb, pos, SEEK_SET);
    avio_wb32(pb, curpos - pos); /* rewrite size */
    avio_seek(pb, curpos, SEEK_SET);

    return curpos - pos;
}

static int co64_required(const MOVTrack *track)
{
    if (track->entry > 0 && track->cluster[track->entry - 1].pos + track->data_offset > UINT32_MAX)
        return 1;
    return 0;
}

static int is_cover_image(const AVStream *st)
{
    /* Eg. AV_DISPOSITION_ATTACHED_PIC | AV_DISPOSITION_TIMED_THUMBNAILS
     * is encoded as sparse video track */
    return st && st->disposition == AV_DISPOSITION_ATTACHED_PIC;
}

static int rtp_hinting_needed(const AVStream *st)
{
    /* Add hint tracks for each real audio and video stream */
    if (is_cover_image(st))
        return 0;
    return st->codecpar->codec_type == AVMEDIA_TYPE_VIDEO ||
           st->codecpar->codec_type == AVMEDIA_TYPE_AUDIO;
}

/* Chunk offset atom */
static int mov_write_stco_tag(AVIOContext *pb, MOVTrack *track)
{
    int i;
    int mode64 = co64_required(track); // use 32 bit size variant if possible
    int64_t pos = avio_tell(pb);
    avio_wb32(pb, 0); /* size */
    if (mode64)
        ffio_wfourcc(pb, "co64");
    else
        ffio_wfourcc(pb, "stco");
    avio_wb32(pb, 0); /* version & flags */
    avio_wb32(pb, track->chunkCount); /* entry count */
    for (i = 0; i < track->entry; i++) {
        if (!track->cluster[i].chunkNum)
            continue;
        if (mode64 == 1)
            avio_wb64(pb, track->cluster[i].pos + track->data_offset);
        else
            avio_wb32(pb, track->cluster[i].pos + track->data_offset);
    }
    return update_size(pb, pos);
}

/* Sample size atom */
static int mov_write_stsz_tag(AVIOContext *pb, MOVTrack *track)
{
    int equalChunks = 1;
    int i, j, entries = 0, tst = -1, oldtst = -1;

    int64_t pos = avio_tell(pb);
    avio_wb32(pb, 0); /* size */
    ffio_wfourcc(pb, "stsz");
    avio_wb32(pb, 0); /* version & flags */

    for (i = 0; i < track->entry; i++) {
        tst = track->cluster[i].size / track->cluster[i].entries;
        if (oldtst != -1 && tst != oldtst)
            equalChunks = 0;
        oldtst = tst;
        entries += track->cluster[i].entries;
    }
    if (equalChunks && track->entry) {
        int sSize = track->entry ? track->cluster[0].size / track->cluster[0].entries : 0;
        sSize = FFMAX(1, sSize); // adpcm mono case could make sSize == 0
        avio_wb32(pb, sSize); // sample size
        avio_wb32(pb, entries); // sample count
    } else {
        avio_wb32(pb, 0); // sample size
        avio_wb32(pb, entries); // sample count
        for (i = 0; i < track->entry; i++) {
            for (j = 0; j < track->cluster[i].entries; j++) {
                avio_wb32(pb, track->cluster[i].size /
                          track->cluster[i].entries);
            }
        }
    }
    return update_size(pb, pos);
}

/* Sample to chunk atom */
static int mov_write_stsc_tag(AVIOContext *pb, MOVTrack *track)
{
    int index = 0, oldval = -1, i;
    int64_t entryPos, curpos;

    int64_t pos = avio_tell(pb);
    avio_wb32(pb, 0); /* size */
    ffio_wfourcc(pb, "stsc");
    avio_wb32(pb, 0); // version & flags
    entryPos = avio_tell(pb);
    avio_wb32(pb, track->chunkCount); // entry count
    for (i = 0; i < track->entry; i++) {
        if (oldval != track->cluster[i].samples_in_chunk && track->cluster[i].chunkNum) {
            avio_wb32(pb, track->cluster[i].chunkNum); // first chunk
            avio_wb32(pb, track->cluster[i].samples_in_chunk); // samples per chunk
            avio_wb32(pb, 0x1); // sample description index
            oldval = track->cluster[i].samples_in_chunk;
            index++;
        }
    }
    curpos = avio_tell(pb);
    avio_seek(pb, entryPos, SEEK_SET);
    avio_wb32(pb, index); // rewrite size
    avio_seek(pb, curpos, SEEK_SET);

    return update_size(pb, pos);
}

/* Sync sample atom */
static int mov_write_stss_tag(AVIOContext *pb, MOVTrack *track, uint32_t flag)
{
    int64_t curpos, entryPos;
    int i, index = 0;
    int64_t pos = avio_tell(pb);
    avio_wb32(pb, 0); // size
    ffio_wfourcc(pb, flag == MOV_SYNC_SAMPLE ? "stss" : "stps");
    avio_wb32(pb, 0); // version & flags
    entryPos = avio_tell(pb);
    avio_wb32(pb, track->entry); // entry count
    for (i = 0; i < track->entry; i++) {
        if (track->cluster[i].flags & flag) {
            avio_wb32(pb, i + 1);
            index++;
        }
    }
    curpos = avio_tell(pb);
    avio_seek(pb, entryPos, SEEK_SET);
    avio_wb32(pb, index); // rewrite size
    avio_seek(pb, curpos, SEEK_SET);
    return update_size(pb, pos);
}

/* Sample dependency atom */
static int mov_write_sdtp_tag(AVIOContext *pb, MOVTrack *track)
{
    int i;
    uint8_t leading, dependent, reference, redundancy;
    int64_t pos = avio_tell(pb);
    avio_wb32(pb, 0); // size
    ffio_wfourcc(pb, "sdtp");
    avio_wb32(pb, 0); // version & flags
    for (i = 0; i < track->entry; i++) {
        dependent = MOV_SAMPLE_DEPENDENCY_YES;
        leading = reference = redundancy = MOV_SAMPLE_DEPENDENCY_UNKNOWN;
        if (track->cluster[i].flags & MOV_DISPOSABLE_SAMPLE) {
            reference = MOV_SAMPLE_DEPENDENCY_NO;
        }
        if (track->cluster[i].flags & MOV_SYNC_SAMPLE) {
            dependent = MOV_SAMPLE_DEPENDENCY_NO;
        }
        avio_w8(pb, (leading << 6)   | (dependent << 4) |
                    (reference << 2) | redundancy);
    }
    return update_size(pb, pos);
}

static int mov_write_amr_tag(AVIOContext *pb, MOVTrack *track)
{
    avio_wb32(pb, 0x11); /* size */
    if (track->mode == MODE_MOV) ffio_wfourcc(pb, "samr");
    else                         ffio_wfourcc(pb, "damr");
    ffio_wfourcc(pb, "FFMP");
    avio_w8(pb, 0); /* decoder version */

    avio_wb16(pb, 0x81FF); /* Mode set (all modes for AMR_NB) */
    avio_w8(pb, 0x00); /* Mode change period (no restriction) */
    avio_w8(pb, 0x01); /* Frames per sample */
    return 0x11;
}

static int mov_write_ac3_tag(AVFormatContext *s, AVIOContext *pb, MOVTrack *track)
{
    GetBitContext gbc;
    PutBitContext pbc;
    uint8_t buf[3];
    int fscod, bsid, bsmod, acmod, lfeon, frmsizecod;

    if (track->vos_len < 7) {
        av_log(s, AV_LOG_WARNING,
               "Cannot write moov atom before AC3 packets."
               " Set the delay_moov flag to fix this.\n");
        return AVERROR(EINVAL);
    }

    avio_wb32(pb, 11);
    ffio_wfourcc(pb, "dac3");

    init_get_bits(&gbc, track->vos_data + 4, (track->vos_len - 4) * 8);
    fscod      = get_bits(&gbc, 2);
    frmsizecod = get_bits(&gbc, 6);
    bsid       = get_bits(&gbc, 5);
    bsmod      = get_bits(&gbc, 3);
    acmod      = get_bits(&gbc, 3);
    if (acmod == 2) {
        skip_bits(&gbc, 2); // dsurmod
    } else {
        if ((acmod & 1) && acmod != 1)
            skip_bits(&gbc, 2); // cmixlev
        if (acmod & 4)
            skip_bits(&gbc, 2); // surmixlev
    }
    lfeon = get_bits1(&gbc);

    init_put_bits(&pbc, buf, sizeof(buf));
    put_bits(&pbc, 2, fscod);
    put_bits(&pbc, 5, bsid);
    put_bits(&pbc, 3, bsmod);
    put_bits(&pbc, 3, acmod);
    put_bits(&pbc, 1, lfeon);
    put_bits(&pbc, 5, frmsizecod >> 1); // bit_rate_code
    put_bits(&pbc, 5, 0); // reserved

    flush_put_bits(&pbc);
    avio_write(pb, buf, sizeof(buf));

    return 11;
}

struct eac3_info {
    AVPacket *pkt;
    uint8_t ec3_done;
    uint8_t num_blocks;

    /* Layout of the EC3SpecificBox */
    /* maximum bitrate */
    uint16_t data_rate;
    /* number of independent substreams */
    uint8_t  num_ind_sub;
    struct {
        /* sample rate code (see ff_ac3_sample_rate_tab) 2 bits */
        uint8_t fscod;
        /* bit stream identification 5 bits */
        uint8_t bsid;
        /* one bit reserved */
        /* audio service mixing (not supported yet) 1 bit */
        /* bit stream mode 3 bits */
        uint8_t bsmod;
        /* audio coding mode 3 bits */
        uint8_t acmod;
        /* sub woofer on 1 bit */
        uint8_t lfeon;
        /* 3 bits reserved */
        /* number of dependent substreams associated with this substream 4 bits */
        uint8_t num_dep_sub;
        /* channel locations of the dependent substream(s), if any, 9 bits */
        uint16_t chan_loc;
        /* if there is no dependent substream, then one bit reserved instead */
    } substream[1]; /* TODO: support 8 independent substreams */
};

#if CONFIG_AC3_PARSER
static int handle_eac3(MOVMuxContext *mov, AVPacket *pkt, MOVTrack *track)
{
    AC3HeaderInfo *hdr = NULL;
    struct eac3_info *info;
    int num_blocks, ret;

    if (!track->eac3_priv && !(track->eac3_priv = av_mallocz(sizeof(*info))))
        return AVERROR(ENOMEM);
    info = track->eac3_priv;

    if (!info->pkt && !(info->pkt = av_packet_alloc()))
        return AVERROR(ENOMEM);

    if (avpriv_ac3_parse_header(&hdr, pkt->data, pkt->size) < 0) {
        /* drop the packets until we see a good one */
        if (!track->entry) {
            av_log(mov->fc, AV_LOG_WARNING, "Dropping invalid packet from start of the stream\n");
            ret = 0;
        } else
            ret = AVERROR_INVALIDDATA;
        goto end;
    }

    info->data_rate = FFMAX(info->data_rate, hdr->bit_rate / 1000);
    num_blocks = hdr->num_blocks;

    if (!info->ec3_done) {
        /* AC-3 substream must be the first one */
        if (hdr->bitstream_id <= 10 && hdr->substreamid != 0) {
            ret = AVERROR(EINVAL);
            goto end;
        }

        /* this should always be the case, given that our AC-3 parser
         * concatenates dependent frames to their independent parent */
        if (hdr->frame_type == EAC3_FRAME_TYPE_INDEPENDENT) {
            /* substream ids must be incremental */
            if (hdr->substreamid > info->num_ind_sub + 1) {
                ret = AVERROR(EINVAL);
                goto end;
            }

            if (hdr->substreamid == info->num_ind_sub + 1) {
                //info->num_ind_sub++;
                avpriv_request_sample(mov->fc, "Multiple independent substreams");
                ret = AVERROR_PATCHWELCOME;
                goto end;
            } else if (hdr->substreamid < info->num_ind_sub ||
                       hdr->substreamid == 0 && info->substream[0].bsid) {
                info->ec3_done = 1;
                goto concatenate;
            }
        } else {
            if (hdr->substreamid != 0) {
                avpriv_request_sample(mov->fc, "Multiple non EAC3 independent substreams");
                ret = AVERROR_PATCHWELCOME;
                goto end;
            }
        }

        /* fill the info needed for the "dec3" atom */
        info->substream[hdr->substreamid].fscod = hdr->sr_code;
        info->substream[hdr->substreamid].bsid  = hdr->bitstream_id;
        info->substream[hdr->substreamid].bsmod = hdr->bitstream_mode;
        info->substream[hdr->substreamid].acmod = hdr->channel_mode;
        info->substream[hdr->substreamid].lfeon = hdr->lfe_on;

        /* Parse dependent substream(s), if any */
        if (pkt->size != hdr->frame_size) {
            int cumul_size = hdr->frame_size;
            int parent = hdr->substreamid;

            while (cumul_size != pkt->size) {
                GetBitContext gbc;
                int i;
                ret = avpriv_ac3_parse_header(&hdr, pkt->data + cumul_size, pkt->size - cumul_size);
                if (ret < 0)
                    goto end;
                if (hdr->frame_type != EAC3_FRAME_TYPE_DEPENDENT) {
                    ret = AVERROR(EINVAL);
                    goto end;
                }
                info->substream[parent].num_dep_sub++;
                ret /= 8;

                /* header is parsed up to lfeon, but custom channel map may be needed */
                init_get_bits8(&gbc, pkt->data + cumul_size + ret, pkt->size - cumul_size - ret);
                /* skip bsid */
                skip_bits(&gbc, 5);
                /* skip volume control params */
                for (i = 0; i < (hdr->channel_mode ? 1 : 2); i++) {
                    skip_bits(&gbc, 5); // skip dialog normalization
                    if (get_bits1(&gbc)) {
                        skip_bits(&gbc, 8); // skip compression gain word
                    }
                }
                /* get the dependent stream channel map, if exists */
                if (get_bits1(&gbc))
                    info->substream[parent].chan_loc |= (get_bits(&gbc, 16) >> 5) & 0x1f;
                else
                    info->substream[parent].chan_loc |= hdr->channel_mode;
                cumul_size += hdr->frame_size;
            }
        }
    }

concatenate:
    if (!info->num_blocks && num_blocks == 6) {
        ret = pkt->size;
        goto end;
    }
    else if (info->num_blocks + num_blocks > 6) {
        ret = AVERROR_INVALIDDATA;
        goto end;
    }

    if (!info->num_blocks) {
        ret = av_packet_ref(info->pkt, pkt);
        if (!ret)
            info->num_blocks = num_blocks;
        goto end;
    } else {
        if ((ret = av_grow_packet(info->pkt, pkt->size)) < 0)
            goto end;
        memcpy(info->pkt->data + info->pkt->size - pkt->size, pkt->data, pkt->size);
        info->num_blocks += num_blocks;
        info->pkt->duration += pkt->duration;
        if (info->num_blocks != 6)
            goto end;
        av_packet_unref(pkt);
        av_packet_move_ref(pkt, info->pkt);
        info->num_blocks = 0;
    }
    ret = pkt->size;

end:
    av_free(hdr);

    return ret;
}
#endif

static int mov_write_eac3_tag(AVFormatContext *s, AVIOContext *pb, MOVTrack *track)
{
    PutBitContext pbc;
    uint8_t *buf;
    struct eac3_info *info;
    int size, i;

    if (!track->eac3_priv) {
        av_log(s, AV_LOG_WARNING,
               "Cannot write moov atom before EAC3 packets parsed.\n");
        return AVERROR(EINVAL);
    }

    info = track->eac3_priv;
    size = 2 + ((34 * (info->num_ind_sub + 1) + 7) >> 3);
    buf = av_malloc(size);
    if (!buf) {
        return AVERROR(ENOMEM);
    }

    init_put_bits(&pbc, buf, size);
    put_bits(&pbc, 13, info->data_rate);
    put_bits(&pbc,  3, info->num_ind_sub);
    for (i = 0; i <= info->num_ind_sub; i++) {
        put_bits(&pbc, 2, info->substream[i].fscod);
        put_bits(&pbc, 5, info->substream[i].bsid);
        put_bits(&pbc, 1, 0); /* reserved */
        put_bits(&pbc, 1, 0); /* asvc */
        put_bits(&pbc, 3, info->substream[i].bsmod);
        put_bits(&pbc, 3, info->substream[i].acmod);
        put_bits(&pbc, 1, info->substream[i].lfeon);
        put_bits(&pbc, 5, 0); /* reserved */
        put_bits(&pbc, 4, info->substream[i].num_dep_sub);
        if (!info->substream[i].num_dep_sub) {
            put_bits(&pbc, 1, 0); /* reserved */
        } else {
            put_bits(&pbc, 9, info->substream[i].chan_loc);
        }
    }
    flush_put_bits(&pbc);
    size = put_bytes_output(&pbc);

    avio_wb32(pb, size + 8);
    ffio_wfourcc(pb, "dec3");
    avio_write(pb, buf, size);

    av_free(buf);

    return size;
}

/**
 * This function writes extradata "as is".
 * Extradata must be formatted like a valid atom (with size and tag).
 */
static int mov_write_extradata_tag(AVIOContext *pb, MOVTrack *track)
{
    avio_write(pb, track->par->extradata, track->par->extradata_size);
    return track->par->extradata_size;
}

static int mov_write_enda_tag(AVIOContext *pb)
{
    avio_wb32(pb, 10);
    ffio_wfourcc(pb, "enda");
    avio_wb16(pb, 1); /* little endian */
    return 10;
}

static int mov_write_enda_tag_be(AVIOContext *pb)
{
  avio_wb32(pb, 10);
  ffio_wfourcc(pb, "enda");
  avio_wb16(pb, 0); /* big endian */
  return 10;
}

static void put_descr(AVIOContext *pb, int tag, unsigned int size)
{
    int i = 3;
    avio_w8(pb, tag);
    for (; i > 0; i--)
        avio_w8(pb, (size >> (7 * i)) | 0x80);
    avio_w8(pb, size & 0x7F);
}

static unsigned compute_avg_bitrate(MOVTrack *track)
{
    uint64_t size = 0;
    int i;
    if (!track
