/*****************************************************************************
 * ffmpeg.c / ffmpeg.cpp
 *****************************************************************************
 * Copyright (C) 2012-2021 L-SMASH Works project
 *
 * Authors: Yusuke Nakamura <muken.the.vfrmaniac@gmail.com>
 *          Akarin <i@akarin.info>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *****************************************************************************/

/* This file is available under an ISC license. */

#include "libavformat/avformat.h"

#include "ffmpeg.h"

// https://github.com/FFmpeg/FFmpeg/commit/557953a397dfdd9c7a3d8c2f60d1204599e3d3ac
#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(58, 78, 100)
#define FFMPEG_45
#endif

int avstream_get_index_entries_count(const AVStream *st) {
#ifdef FFMPEG_45
    return avformat_index_get_entries_count(st);
#else
    return st->nb_index_entries;
#endif
}

const AVIndexEntry *avstream_index_get_entry(const AVStream *st, int idx) {
#ifdef FFMPEG_45
    return avformat_index_get_entry(st, idx);
#else
    if (idx < 0 || idx >= avstream_get_index_entries_count(st)) return NULL;
    return &st->index_entries[idx];
#endif
}

int avstream_add_index_entry(AVStream *st, int64_t pos, int64_t timestamp, int size, int distance, int flags) {
    return av_add_index_entry( st, pos, timestamp, size, distance, flags );
}

void avstream_update_index_entries(AVStream *st, AVIndexEntry *entries, int n, unsigned int alloc_size) {
#ifdef FFMPEG_45
    // There is no way but to insert them one by one.
    if ( !entries ) {
        // TODO: There is no API to remove entries....
        return;
    }
    for ( int i = 0; i < n; i++ )
        avstream_add_index_entry( st, entries[i].pos, entries[i].timestamp,
                entries[i].size, entries[i].min_distance, entries[i].flags );
#else
    av_free( st->index_entries );
    st->index_entries                = entries;
    st->nb_index_entries             = n;
    st->index_entries_allocated_size = alloc_size;
#endif
}
