/*****************************************************************************
 * lwlibav_input.c
 *****************************************************************************
 * Copyright (C) 2012-2015 L-SMASH Works project
 *
 * Authors: Yusuke Nakamura <muken.the.vfrmaniac@gmail.com>
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

/* Libav
 * The binary file will be LGPLed or GPLed. */
#include <libavformat/avformat.h>       /* Demuxer */
#include <libavcodec/avcodec.h>         /* Decoder */
#include <libswscale/swscale.h>         /* Colorspace converter */
#include <libswresample/swresample.h>   /* Audio resampler */

#include "lwinput.h"
#include "resource.h"
#include "progress_dlg.h"
#include "video_output.h"
#include "audio_output.h"

#include "../common/progress.h"
#include "../common/lwlibav_dec.h"
#include "../common/lwlibav_video.h"
#include "../common/lwlibav_audio.h"
#include "../common/lwindex.h"

static const char reader_name[] = "LW-Libav";

typedef struct libav_handler_tag
{
    UINT                           uType;
    lwlibav_file_handler_t         lwh;
    /* Video stuff */
    lwlibav_video_decode_handler_t *vdhp;
    lwlibav_video_output_handler_t *vohp;
    /* Audio stuff */
    lwlibav_audio_decode_handler_t *adhp;
    lwlibav_audio_output_handler_t *aohp;
} libav_handler_t;

struct progress_handler_tag
{
    progress_dlg_t dlg;
    const char    *module_name;
    int            template_id;
};

static void open_indicator( progress_handler_t *php )
{
    init_progress_dlg( &php->dlg, php->module_name, php->template_id );
}

static int update_indicator( progress_handler_t *php, const char *message, int percent )
{
    return update_progress_dlg( &php->dlg, message, percent );
}

static void close_indicator( progress_handler_t *php )
{
    close_progress_dlg( &php->dlg );
}

/* Deallocate the handler of this plugin. */
static void free_handler
(
    libav_handler_t **hpp
)
{
    if( !hpp || !*hpp )
        return;
    libav_handler_t *hp = *hpp;
    lwlibav_video_free_decode_handler( hp->vdhp );
    lwlibav_video_free_output_handler( hp->vohp );
    lwlibav_audio_free_decode_handler( hp->adhp );
    lwlibav_audio_free_output_handler( hp->aohp );
    lw_freep( hpp );
}

/* Allocate the handler of this plugin. */
static libav_handler_t *alloc_handler
(
    void
)
{
    libav_handler_t *hp = lw_malloc_zero( sizeof(libav_handler_t) );
    if( !hp )
        return NULL;
    if( !(hp->vdhp = lwlibav_video_alloc_decode_handler())
     || !(hp->vohp = lwlibav_video_alloc_output_handler())
     || !(hp->adhp = lwlibav_audio_alloc_decode_handler())
     || !(hp->aohp = lwlibav_audio_alloc_output_handler()) )
    {
        free_handler( &hp );
        return NULL;
    }
    return hp;
}

static int prepare_video_decoding( lsmash_handler_t *h, video_option_t *opt )
{
    libav_handler_t *hp = (libav_handler_t *)h->video_private;
    lwlibav_video_decode_handler_t *vdhp = hp->vdhp;
    AVCodecContext *ctx = lwlibav_video_get_codec_context( vdhp );
    if( !ctx )
        return 0;
    lwlibav_video_set_seek_mode             ( vdhp, opt->seek_mode );
    lwlibav_video_set_forward_seek_threshold( vdhp, opt->forward_seek_threshold );
    lwlibav_video_output_handler_t *vohp = hp->vohp;
    /* Import AVIndexEntrys. */
    if( lwlibav_import_av_index_entry( (lwlibav_decode_handler_t *)vdhp ) < 0 )
        return -1;
    /* Set up timestamp info. */
    hp->uType = MB_OK;
    int64_t fps_num = 25;
    int64_t fps_den = 1;
    lwlibav_video_setup_timestamp_info( &hp->lwh, vdhp, vohp, &fps_num, &fps_den, opt->apply_repeat_flag );
    h->framerate_num      = (int)fps_num;
    h->framerate_den      = (int)fps_den;
    h->video_sample_count = vohp->frame_count;
    hp->uType = MB_ICONERROR | MB_OK;
    /* Set up the initial input format. */
    lwlibav_video_set_initial_input_format( vdhp );
    /* Set up video rendering. */
    int max_width  = lwlibav_video_get_max_width ( vdhp );
    int max_height = lwlibav_video_get_max_height( vdhp );
    if( au_setup_video_rendering( vohp, opt, &h->video_format, max_width, max_height, ctx->pix_fmt ) < 0 )
        return -1;
#ifndef DEBUG_VIDEO
    lw_log_handler_t *lhp = lwlibav_video_get_log_handler( vdhp );
    lhp->level = LW_LOG_FATAL;
#endif
    /* Find the first valid video frame. */
    if( lwlibav_video_find_first_valid_frame( vdhp ) < 0 )
        return -1;
    /* Force seeking at the first reading. */
    lwlibav_video_force_seek( vdhp );
    return 0;
}

static int prepare_audio_decoding( lsmash_handler_t *h, audio_option_t *opt )
{
    libav_handler_t *hp = (libav_handler_t *)h->audio_private;
    lwlibav_audio_decode_handler_t *adhp = hp->adhp;
    AVCodecContext *ctx = lwlibav_audio_get_codec_context( adhp );
    if( !ctx )
        return 0;
    /* Import AVIndexEntrys. */
    if( lwlibav_import_av_index_entry( (lwlibav_decode_handler_t *)adhp ) < 0 )
        return -1;
#ifndef DEBUG_AUDIO
    lw_log_handler_t *lhp = lwlibav_audio_get_log_handler( adhp );
    lhp->level = LW_LOG_FATAL;
#endif
    lwlibav_audio_output_handler_t *aohp = hp->aohp;
    if( au_setup_audio_rendering( aohp, ctx, opt, &h->audio_format.Format ) < 0 )
        return -1;
    /* Count the number of PCM audio samples. */
    h->audio_pcm_sample_count = lwlibav_audio_count_overall_pcm_samples( adhp, aohp->output_sample_rate );
    if( h->audio_pcm_sample_count == 0 )
    {
        DEBUG_AUDIO_MESSAGE_BOX_DESKTOP( MB_ICONERROR | MB_OK, "No valid audio frame." );
        return -1;
    }
    if( hp->lwh.av_gap && aohp->output_sample_rate != ctx->sample_rate )
        hp->lwh.av_gap = ((int64_t)hp->lwh.av_gap * aohp->output_sample_rate - 1) / ctx->sample_rate + 1;
    h->audio_pcm_sample_count += hp->lwh.av_gap;
    /* Force seeking at the first reading. */
    lwlibav_audio_force_seek( adhp );
    return 0;
}

static void *open_file( char *file_path, reader_option_t *opt )
{
    libav_handler_t *hp = alloc_handler();
    if( !hp )
        return NULL;
    /* Set up error handler. */
    lw_log_handler_t *vlhp = lwlibav_video_get_log_handler( hp->vdhp );
    lw_log_handler_t *alhp = lwlibav_audio_get_log_handler( hp->adhp );
    vlhp->name     = reader_name;
    vlhp->level    = LW_LOG_FATAL;
    vlhp->priv     = &hp->uType;
    vlhp->show_log = NULL;
    *alhp = *vlhp;
    hp->uType = MB_ICONERROR | MB_OK;
    /* Set options. */
    lwlibav_option_t lwlibav_opt;
    lwlibav_opt.file_path                      = file_path;
    lwlibav_opt.cache_dir                      = opt->cache_dir_name;
    lwlibav_opt.threads                        = opt->threads;
    lwlibav_opt.no_create_index                = opt->no_create_index;
    lwlibav_opt.index_file_path                = NULL;
    lwlibav_opt.force_video                    = opt->force_video;
    lwlibav_opt.force_video_index              = opt->force_video_index;
    lwlibav_opt.force_audio                    = opt->force_audio;
    lwlibav_opt.force_audio_index              = opt->force_audio_index;
    lwlibav_opt.post_process.av_sync           = opt->av_sync;
    lwlibav_opt.post_process.apply_repeat_flag = opt->video_opt.apply_repeat_flag;
    lwlibav_opt.post_process.field_dominance   = opt->video_opt.field_dominance;
    lwlibav_opt.post_process.vfr2cfr.active    = 0;
    lwlibav_opt.post_process.vfr2cfr.fps_num   = 60000;
    lwlibav_opt.post_process.vfr2cfr.fps_den   = 1001;
    lwlibav_video_set_preferred_decoder_names( hp->vdhp, opt->preferred_decoder_names );
    lwlibav_audio_set_preferred_decoder_names( hp->adhp, opt->preferred_decoder_names );
    /* Set up progress indicator. */
    progress_indicator_t indicator;
    indicator.open   = open_indicator;
    indicator.update = update_indicator;
    indicator.close  = close_indicator;
    progress_handler_t ph = { { 0 } };
    ph.module_name = "lwinput.aui2";
    ph.template_id = IDD_PROGRESS_ABORTABLE;
    /* Construct index. */
    if( lwlibav_construct_index( &hp->lwh, hp->vdhp, hp->vohp, hp->adhp, hp->aohp, vlhp, &lwlibav_opt, &indicator, &ph ) < 0 )
    {
        free_handler( &hp );
        return NULL;
    }
    return hp;
}

static int find_video( lsmash_handler_t *h, video_option_t *opt )
{
    libav_handler_t *hp = (libav_handler_t *)h->video_private;
    h->video_track_count = lwlibav_video_get_track_count( hp->vdhp );
    if( h->video_track_count <= 0 )
    {
        h->video_track_count = 0;
        return -1;
    }
    return 0;
}

static int find_audio( lsmash_handler_t *h, audio_option_t *opt )
{
    libav_handler_t *hp = (libav_handler_t *)h->audio_private;
    h->audio_track_count = lwlibav_audio_get_track_count( hp->adhp );
    if( h->audio_track_count <= 0 )
    {
        h->audio_track_count = 0;
        return -1;
    }
    return 0;
}

static int get_video_track( lsmash_handler_t *h, reader_option_t *opt, int index )
{
    index++; /* 1-origin */
    libav_handler_t *hp = (libav_handler_t *)h->video_private;

    /* Set stream index */
    int stream_index = lwlibav_video_get_stream_index_from_index( hp->vdhp, index );
    lwlibav_video_set_stream_index( hp->vdhp, stream_index );
    lwlibav_post_process_option_t post_opt;
    post_opt.av_sync           = opt->av_sync;
    post_opt.apply_repeat_flag = opt->video_opt.apply_repeat_flag;
    post_opt.field_dominance   = opt->video_opt.field_dominance;
    post_opt.vfr2cfr.active    = 0;
    post_opt.vfr2cfr.fps_num   = 60000;
    post_opt.vfr2cfr.fps_den   = 1001;
    lwlibav_post_process( &hp->lwh, hp->vdhp, hp->vohp, hp->adhp, hp->aohp, &post_opt );

    if( lwlibav_video_get_desired_track( hp->lwh.file_path, hp->vdhp, hp->lwh.threads ) < 0 )
        return -1;
    lw_log_handler_t *lhp = lwlibav_video_get_log_handler( hp->vdhp );
    lhp->name     = reader_name;
    lhp->level    = LW_LOG_WARNING;
    lhp->priv     = &hp->uType;
    lhp->show_log = au_message_box_desktop;
    if( prepare_video_decoding( h, &opt->video_opt ) < 0 )
        return -1;
    return index - 1;
}

static int get_audio_track( lsmash_handler_t *h, reader_option_t *opt, int index )
{
    index++; /* 1-origin */
    libav_handler_t *hp = (libav_handler_t *)h->audio_private;

    /* Set stream index */
    int stream_index = lwlibav_audio_get_stream_index_from_index( hp->adhp, index );
    lwlibav_audio_set_stream_index( hp->adhp, stream_index );
    lwlibav_post_process_option_t post_opt;
    post_opt.av_sync           = opt->av_sync;
    post_opt.apply_repeat_flag = opt->video_opt.apply_repeat_flag;
    post_opt.field_dominance   = opt->video_opt.field_dominance;
    post_opt.vfr2cfr.active    = 0;
    post_opt.vfr2cfr.fps_num   = 60000;
    post_opt.vfr2cfr.fps_den   = 1001;
    lwlibav_post_process( &hp->lwh, hp->vdhp, hp->vohp, hp->adhp, hp->aohp, &post_opt );

    if( lwlibav_audio_get_desired_track( hp->lwh.file_path, hp->adhp, hp->lwh.threads ) < 0 )
        return -1;
    lw_log_handler_t *lhp = lwlibav_audio_get_log_handler( hp->adhp );
    lhp->name     = reader_name;
    lhp->level    = LW_LOG_WARNING;
    lhp->priv     = &hp->uType;
    lhp->show_log = au_message_box_desktop;
    if( prepare_audio_decoding( h, &opt->audio_opt ) < 0 )
        return -1;
    return index - 1;
}

static int read_video( lsmash_handler_t *h, int frame_number, void *buf )
{
    libav_handler_t *hp = (libav_handler_t *)h->video_private;
    lwlibav_video_decode_handler_t *vdhp = hp->vdhp;
    if( lwlibav_video_get_error( vdhp ) )
        return 0;
    lwlibav_video_output_handler_t *vohp = hp->vohp;
    ++frame_number;            /* frame_number is 1-origin. */
    if( frame_number == 1 )
    {
        au_video_output_handler_t *au_vohp = (au_video_output_handler_t *)vohp->private_handler;
        memcpy( buf, au_vohp->back_ground, au_vohp->output_frame_size );
    }
    int ret = lwlibav_video_get_frame( vdhp, vohp, frame_number );
    if( ret != 0 && !(ret == 1 && frame_number == 1) )
        /* Skip writing frame data into AviUtl's frame buffer.
         * Apparently, AviUtl clears the frame buffer at the first frame.
         * Therefore, don't skip in that case. */
        return 0;
    AVFrame *av_frame = lwlibav_video_get_frame_buffer( vdhp );
    return convert_colorspace( vohp, av_frame, buf );
}

static int read_audio( lsmash_handler_t *h, int start, int wanted_length, void *buf )
{
    libav_handler_t *hp = (libav_handler_t *)h->audio_private;
    return lwlibav_audio_get_pcm_samples( hp->adhp, hp->aohp, buf, start, wanted_length );
}

static int is_keyframe( lsmash_handler_t *h, int frame_number )
{
    libav_handler_t *hp = (libav_handler_t *)h->video_private;
    return lwlibav_video_is_keyframe( hp->vdhp, hp->vohp, frame_number + 1 );
}

static int delay_audio( lsmash_handler_t *h, int *start, int wanted_length, int audio_delay )
{
    /* Even if start become negative, its absolute value shall be equal to wanted_length or smaller. */
    libav_handler_t *hp = (libav_handler_t *)h->audio_private;
    int end = *start + wanted_length;
    audio_delay += hp->lwh.av_gap;
    if( *start < audio_delay && end <= audio_delay )
    {
        lwlibav_audio_force_seek( hp->adhp );   /* Force seeking at the next access for valid audio frame. */
        return 0;
    }
    *start -= audio_delay;
    return 1;
}

static void video_cleanup( lsmash_handler_t *h )
{
    libav_handler_t *hp = (libav_handler_t *)h->video_private;
    if( !hp )
        return;
    /* Free and then set nullptr since other functions might reference the pointer later. */
    lwlibav_video_free_decode_handler_ptr( &hp->vdhp );
    lwlibav_video_free_output_handler_ptr( &hp->vohp );
}

static void audio_cleanup( lsmash_handler_t *h )
{
    libav_handler_t *hp = (libav_handler_t *)h->audio_private;
    if( !hp )
        return;
    /* Free and then set nullptr since other functions might reference the pointer later. */
    lwlibav_audio_free_decode_handler_ptr( &hp->adhp );
    lwlibav_audio_free_output_handler_ptr( &hp->aohp );
}

static void close_file( void *private_stuff )
{
    libav_handler_t *hp = (libav_handler_t *)private_stuff;
    if( !hp )
        return;
    lw_free( hp->lwh.file_path );
    lw_free( hp );
}

static int time_to_frame( lsmash_handler_t *h, double time )
{
    libav_handler_t *hp = (libav_handler_t *)h->video_private;
    if( !hp )
        return 0;
    return lwlibav_ts_to_frame_number( hp->vdhp, hp->vohp, time ) - 1; /* frame_number is 1-origin. */
}


lsmash_reader_t libav_reader =
{
    LIBAV_READER,
    open_file,
    find_video,
    find_audio,
    get_video_track,
    get_audio_track,
    NULL,
    read_video,
    read_audio,
    is_keyframe,
    delay_audio,
    video_cleanup,
    audio_cleanup,
    close_file,
    time_to_frame
};
