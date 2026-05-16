/********************************************************************************
 * vpy_input.h
 ********************************************************************************
 * Copyright (C) 2013-2015 L-SMASH Works project
 *
 * Authors: Mr-Ojii <contact.mr.ojii@gmail.com>
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

#include "lwinput.h"

typedef struct vpy_handler_tag vpy_handler_t;

typedef struct
{
    vpy_handler_t *(*open_file)             ( char *file_name, reader_option_t *opt );
    int            (*find_video)            ( lsmash_handler_t *h, video_option_t *opt );
    int            (*get_video_track)       ( lsmash_handler_t *h, reader_option_t *opt, int index );
    int            (*read_video)            ( lsmash_handler_t *h, int sample_number, void *buf );
    void           (*video_cleanup)         ( lsmash_handler_t *h );
    void           (*close_file)            ( vpy_handler_t *hp );
} vpy_reader_t;

struct vpy_handler_tag
{
    vpy_reader_t *reader;
    void         *private_stuff;
};
