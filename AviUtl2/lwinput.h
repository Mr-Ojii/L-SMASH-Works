/*****************************************************************************
 * lwinput.h
 *****************************************************************************
 * Copyright (C) 2011-2015 L-SMASH Works project
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

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <windows.h>
#include <mmreg.h>
#include <stdbool.h>

#include "input2.h"

#include "lwcolor.h"
#include "../common/utils.h"

#define MAKE_AVIUTL_PITCH( x ) ((((x) + 31) & ~31) >> 3)
#define PREFERRED_DECODER_NAMES_BUFSIZE 512
#define CACHE_DIR_NAME_BUFSIZE (_MAX_PATH * 4)

#define MESSAGE_BOX_DESKTOP( uType, ... ) \
do \
{ \
    char temp[256]; \
    wsprintfA( temp, __VA_ARGS__ ); \
    MessageBoxA( HWND_DESKTOP, temp, "lwinput", uType ); \
} while( 0 )

/* Macros for debug */
#if defined( DEBUG_VIDEO ) || defined( DEBUG_AUDIO )
#define DEBUG_MESSAGE_BOX_DESKTOP( uType, ... ) MESSAGE_BOX_DESKTOP( uType, __VA_ARGS__ )
#else
#define DEBUG_MESSAGE_BOX_DESKTOP( uType, ... )
#endif

#ifdef DEBUG_VIDEO
#define DEBUG_VIDEO_MESSAGE_BOX_DESKTOP( uType, ... ) DEBUG_MESSAGE_BOX_DESKTOP( uType, __VA_ARGS__ )
#else
#define DEBUG_VIDEO_MESSAGE_BOX_DESKTOP( uType, ... )
#endif

#ifdef DEBUG_AUDIO
#define DEBUG_AUDIO_MESSAGE_BOX_DESKTOP( uType, ... ) DEBUG_MESSAGE_BOX_DESKTOP( uType, __VA_ARGS__ )
#else
#define DEBUG_AUDIO_MESSAGE_BOX_DESKTOP( uType, ... )
#endif

typedef enum
{
    READER_NONE       = 0,
    LIBAVSMASH_READER = 1,
    AVS_READER        = 2,
    VPY_READER        = 3,
    LIBAV_READER      = 4,
} reader_type;

typedef struct
{
    int seek_mode;
    int forward_seek_threshold;
    int scaler;
    int apply_repeat_flag;
    int field_dominance;
    output_colorspace_index colorspace;
    struct
    {
        int bit_depth;
    } avs;
} video_option_t;

enum
{
    MIX_LEVEL_INDEX_CENTER = 0,
    MIX_LEVEL_INDEX_SURROUND,
    MIX_LEVEL_INDEX_LFE,
};

typedef struct
{
    uint64_t channel_layout;
    int      sample_rate;
    int      mix_level[3];      /* { Center, Surround, LFE } */
} audio_option_t;

typedef struct
{
    int threads;
    int av_sync;
    char         preferred_decoder_names_original_buf[PREFERRED_DECODER_NAMES_BUFSIZE];
    char         preferred_decoder_names_buf[PREFERRED_DECODER_NAMES_BUFSIZE];
    const char **preferred_decoder_names;
    /* for libav reader */
    int no_create_index;
    int force_video;
    int force_video_index;
    int force_audio;
    int force_audio_index;
    int use_cache_dir;
    char  cache_dir_name_buf[CACHE_DIR_NAME_BUFSIZE];
    char *cache_dir_name;
    /* for video stream */
    video_option_t video_opt;
    /* for audio stream */
    audio_option_t audio_opt;
} reader_option_t;

typedef struct
{
    int reader_disabled[5];
    int audio_delay;
    int delete_old_cache;
    int delete_old_cache_days;
    int wide_dialog;
    uint64_t cache_last_check_date;
    reader_option_t reader_opt;
} lwinput_option_t;

typedef struct lsmash_handler_tag lsmash_handler_t;

typedef struct
{
    reader_type type;
    void *(*open_file)             ( char *file_name, reader_option_t *opt );
    int   (*find_video)            ( lsmash_handler_t *h, video_option_t *opt );
    int   (*find_audio)            ( lsmash_handler_t *h, audio_option_t *opt );
    int   (*get_video_track)       ( lsmash_handler_t *h, reader_option_t *opt, int index );
    int   (*get_audio_track)       ( lsmash_handler_t *h, reader_option_t *opt, int index );
    void  (*destroy_disposable)    ( void *private_stuff );
    int   (*read_video)            ( lsmash_handler_t *h, int sample_number, void *buf );
    int   (*read_audio)            ( lsmash_handler_t *h, int start, int wanted_length, void *buf );
    int   (*is_keyframe)           ( lsmash_handler_t *h, int sample_number );
    int   (*delay_audio)           ( lsmash_handler_t *h, int *start, int wanted_length, int audio_delay );
    void  (*video_cleanup)         ( lsmash_handler_t *h );
    void  (*audio_cleanup)         ( lsmash_handler_t *h );
    void  (*close_file)            ( void *private_stuff );
    int   (*time_to_frame)         ( lsmash_handler_t *h, double time );
} lsmash_reader_t;

struct lsmash_handler_tag
{
    void                *global_private;
    void (*destroy_disposable)( void *private_stuff );
    void (*close_file)        ( void *private_stuff );
    /* Video stuff */
    reader_type          video_reader;
    void                *video_private;
    int                  video_track_count;
    BITMAPINFOHEADER     video_format;
    int                  framerate_num;
    int                  framerate_den;
    uint32_t             video_sample_count;
    int  (*get_video_track)         ( lsmash_handler_t *h, reader_option_t *opt, int index );
    int  (*read_video)              ( lsmash_handler_t *h, int sample_number, void *buf );
    int  (*is_keyframe)             ( lsmash_handler_t *h, int sample_number );
    void (*video_cleanup)           ( lsmash_handler_t *h );
    void (*destroy_video_disposable)( void *private_stuff );
    void (*close_video_file)        ( void *private_stuff );
    int  (*time_to_frame)           ( lsmash_handler_t *h, double time );
    /* Audio stuff */
    reader_type          audio_reader;
    void                *audio_private;
    int                  audio_track_count;
    WAVEFORMATEXTENSIBLE audio_format;
    uint32_t             audio_pcm_sample_count;
    int  (*get_audio_track)         ( lsmash_handler_t *h, reader_option_t *opt, int index );
    int  (*read_audio)              ( lsmash_handler_t *h, int start, int wanted_length, void *buf );
    int  (*delay_audio)             ( lsmash_handler_t *h, int *start, int wanted_length, int audio_delay );
    void (*audio_cleanup)           ( lsmash_handler_t *h );
    void (*destroy_audio_disposable)( void *private_stuff );
    void (*close_audio_file)        ( void *private_stuff );
};

typedef void func_get_output( uint8_t *out_data, int out_linesize, uint8_t **in_data, int in_linesize, int height, int full_range );

void au_message_box_desktop
(
    lw_log_handler_t *lhp,
    lw_log_level      level,
    const char       *message
);

BOOL func_init( void );
BOOL func_exit( void );
INPUT_HANDLE func_open( LPCWSTR file );
bool func_close( INPUT_HANDLE ih );
bool func_info_get( INPUT_HANDLE ih, INPUT_INFO *iip );
int func_read_video( INPUT_HANDLE ih, int sample_number, void *buf );
int func_read_audio( INPUT_HANDLE ih, int start, int length, void *buf );
bool func_config( HWND hwnd, HINSTANCE dll_hinst );
int func_set_track( INPUT_HANDLE ih, int type, int index );
int func_time_to_frame( INPUT_HANDLE ih, double time );
