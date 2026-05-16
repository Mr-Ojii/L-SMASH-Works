/********************************************************************************
 * vpy_input.c
 ********************************************************************************
 * Copyright (C) 2013-2015 L-SMASH Works project
 *
 * Authors: Yusuke Nakamura <muken.the.vfrmaniac@gmail.com>
 *          Mr-Ojii <contact.mr.ojii@gmail.com>
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

/* This file is available under an ISC license.
 * However, when distributing its binary file, it will be under LGPL or GPL.
 * Don't distribute it if its license is GPL. */

#include "vpy_input.h"

extern vpy_reader_t vpy_input_v3_wrapper;
extern vpy_reader_t vpy_input_v4_wrapper;

static void *open_file
(
    char            *file_name,
    reader_option_t *opt
)
{
    void* private_stuff = vpy_input_v4_wrapper.open_file( file_name, opt );
    if( private_stuff )
        return private_stuff;

    return vpy_input_v3_wrapper.open_file( file_name, opt );
}

static int find_video
(
    lsmash_handler_t *h,
    video_option_t   *opt
)
{
    vpy_handler_t *hp = (vpy_handler_t *)h->video_private;
    return hp->reader->find_video( h, opt );
}

static int get_video_track
(
    lsmash_handler_t *h,
    reader_option_t   *opt,
    int              index
)
{
    vpy_handler_t *hp = (vpy_handler_t *)h->video_private;
    return hp->reader->get_video_track( h, opt, index );
}

static int read_video
(
    lsmash_handler_t *h,
    int               sample_number,
    void             *buf
)
{
    vpy_handler_t *hp = (vpy_handler_t *)h->video_private;
    return hp->reader->read_video( h, sample_number, buf );
}

static void video_cleanup
(
    lsmash_handler_t *h
)
{
    vpy_handler_t *hp = (vpy_handler_t *)h->video_private;
    if( !hp )
        return;
    hp->reader->video_cleanup( h );
}

static void close_file
(
    void *private_stuff
)
{
    vpy_handler_t *hp = (vpy_handler_t *)private_stuff;
    if( !hp )
        return;
    if( hp->reader )
        hp->reader->close_file( hp );
}

lsmash_reader_t vpy_reader =
{
    VPY_READER,
    open_file,
    find_video,
    NULL,
    get_video_track,
    NULL,
    NULL,
    read_video,
    NULL,
    NULL,
    NULL,
    video_cleanup,
    NULL,
    close_file,
    NULL
};
