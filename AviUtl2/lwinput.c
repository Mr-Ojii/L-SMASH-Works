/*****************************************************************************
 * lwinput.c
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

#include "lwinput.h"
#include "resource.h"
#include "../common/osdep.h"

#include "config.h"

#include <commctrl.h>

#include <libavutil/channel_layout.h>
/* Version */
#include <libavutil/version.h>
#include <libavcodec/version.h>
#include <libavformat/version.h>
#include <libswscale/version.h>
#include <libswresample/version.h>
/* License */
#include <libavutil/avutil.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>

#define MAX_AUTO_NUM_THREADS 16

#define MPEG4_FILE_EXT       "*.mp4;*.m4v;*.m4a;*.mov;*.qt;*.3gp;*.3g2;*.f4v;*.ismv;*.isma"
#define INDEX_FILE_EXT       "*.lwi"
#define ANY_FILE_EXT         "*.*"

/* It looks like "AviUtl ExEdit2 beta1" has a parsing bug about filefilter? */
/* Note: It is not always possible to load the following */
#define VIDEO_FILE_EXT       "*.avi;*.mpg;*.mpeg;*.m1v;*.mts;*.m2t;*.m2ts;*.ts;*.vob;*.dv;*.flv;*.mkv;*.webm;*.wmv;*.asf;*.apng;*.gif"
#define AUDIO_FILE_EXT       "*.wav;*.aiff;*.flac;*.wv;*.alac;*.aac;*.mp3;*.ogg;*.opus;*.ac3;*.wma"
#define AVISYNTH_FILE_EXT    "*.avs"
#define VAPOURSYNTH_FILE_EXT "*.vpy"
#define SUPPORTED_FILE_EXT  MPEG4_FILE_EXT ";" VIDEO_FILE_EXT ";" AUDIO_FILE_EXT ";" INDEX_FILE_EXT ";" AVISYNTH_FILE_EXT ";" VAPOURSYNTH_FILE_EXT


static char plugin_information[512] = { 0 };
static char* plugin_dir = NULL;
static char* default_index_dir = NULL;
static HMODULE hModuleDLL = NULL;

static void get_plugin_information( void )
{
    sprintf( plugin_information,
             "L-SMASH Works File Reader for AviUtl2 r%s\n"
             "    libavutil %s : %s\n"
             "    libavcodec %s : %s\n"
             "    libavformat %s : %s\n"
             "    libswscale %s : %s\n"
             "    libswresample %s : %s",
             LSMASHWORKS_REV,
             AV_STRINGIFY( LIBAVUTIL_VERSION     ), avutil_license    (),
             AV_STRINGIFY( LIBAVCODEC_VERSION    ), avcodec_license   (),
             AV_STRINGIFY( LIBAVFORMAT_VERSION   ), avformat_license  (),
             AV_STRINGIFY( LIBSWSCALE_VERSION    ), swscale_license   (),
             AV_STRINGIFY( LIBSWRESAMPLE_VERSION ), swresample_license() );
}

INPUT_PLUGIN_TABLE input_plugin_table =
{
    INPUT_PLUGIN_FLAG_VIDEO | INPUT_PLUGIN_FLAG_AUDIO | INPUT_PLUGIN_FLAG_MULTI_TRACK,
    L"L-SMASH Works File Reader for AviUtl2",
    L"Supported File (" SUPPORTED_FILE_EXT ")\0" SUPPORTED_FILE_EXT "\0"
    L"Any File (" ANY_FILE_EXT ")\0" ANY_FILE_EXT "\0",
    L"L-SMASH Works File Reader for AviUtl2 r" LSMASHWORKS_REV L" by Mr-Ojii\0",
    func_open,
    func_close,
    func_info_get,
    func_read_video,
    func_read_audio,
    func_config,
    func_set_track,
    func_time_to_frame,
};

EXTERN_C INPUT_PLUGIN_TABLE __declspec(dllexport) * __stdcall GetInputPluginTable( void )
{
    // In AviUtl ExEdit2, func_init is obsolete.
    // In the sample code, it is called in DLL_PROCESS_ATTACH, but since the program freezes, it is called here.
    func_init();

    return &input_plugin_table;
}

static lwinput_option_t lwinput_opt = { 0 };
static reader_option_t *reader_opt = &lwinput_opt.reader_opt;
static video_option_t *video_opt = &lwinput_opt.reader_opt.video_opt;
static audio_option_t *audio_opt = &lwinput_opt.reader_opt.audio_opt;

static lwinput_option_t lwinput_opt_config = { 0 };
static reader_option_t *reader_opt_config = &lwinput_opt_config.reader_opt;
static video_option_t *video_opt_config = &lwinput_opt_config.reader_opt.video_opt;
static audio_option_t *audio_opt_config = &lwinput_opt_config.reader_opt.audio_opt;

static const char *settings_path = NULL;
static const char *settings_path_list[] = { "lsmash.ini", "plugins/lsmash.ini" };
static const char *seek_mode_list[] = { "Normal", "Unsafe", "Aggressive" };
static const char *scaler_list[] = { "Fast bilinear", "Bilinear", "Bicubic", "Experimental", "Nearest neighbor", "Area averaging",
                                     "L-bicubic/C-bilinear", "Gaussian", "Sinc", "Lanczos", "Bicubic spline" };
static const char *field_dominance_list[] = { "Obey source flags", "Top -> Bottom", "Bottom -> Top" };
static const char *avs_bit_depth_list[] = { "8", "9", "10", "16" };

void au_message_box_desktop
(
    lw_log_handler_t *lhp,
    lw_log_level      level,
    const char       *message
)
{
    UINT uType = *(UINT *)lhp->priv;
    MessageBoxA( HWND_DESKTOP, message, "lwinput", uType );
}

static FILE *open_settings( const char* mode )
{
    FILE *ini = NULL;

    if( settings_path ) {
        size_t size = strlen( plugin_dir ) + strlen( settings_path ) + 1;
        char* ini_file_path = lw_malloc_zero( size );
        if( ini_file_path ) {
            strcpy_s( ini_file_path, size, plugin_dir );
            strcat_s( ini_file_path, size, settings_path );
            ini = lw_fopen( ini_file_path, mode );
            lw_free( ini_file_path );
            if( ini )
                return ini;
        }
    }

    for( int i = 0; i < 2; i++ )
    {
        const char* tmp_settings_path = settings_path_list[i];
        size_t size = strlen( plugin_dir ) + strlen( tmp_settings_path ) + 1;
        char* ini_file_path = lw_malloc_zero( size );
        if( ini_file_path ) {
            strcpy_s( ini_file_path, size, plugin_dir );
            strcat_s( ini_file_path, size, tmp_settings_path );
            ini = lw_fopen( ini_file_path, mode );
            lw_free( ini_file_path );
            if( ini )
            {
                settings_path = tmp_settings_path;
                return ini;
            }
        }
    }
    return NULL;
}

static int get_auto_threads( void )
{
    int n = atoi( getenv( "NUMBER_OF_PROCESSORS" ) );
    if( n > MAX_AUTO_NUM_THREADS )
        n = MAX_AUTO_NUM_THREADS;
    return n;
}

static inline void clean_preferred_decoder_names( reader_option_t *_reader_opt )
{
    lw_freep( &_reader_opt->preferred_decoder_names );
    memset( _reader_opt->preferred_decoder_names_original_buf, 0, PREFERRED_DECODER_NAMES_BUFSIZE );
    memset( _reader_opt->preferred_decoder_names_buf, 0, PREFERRED_DECODER_NAMES_BUFSIZE );
}

static inline void set_preferred_decoder_names_on_buf
(
    reader_option_t *_reader_opt,
    const char *preferred_decoder_names
)
{
    clean_preferred_decoder_names( _reader_opt );
    memcpy( _reader_opt->preferred_decoder_names_original_buf, preferred_decoder_names,
            MIN( PREFERRED_DECODER_NAMES_BUFSIZE - 1, strlen(preferred_decoder_names) ) );
    memcpy(_reader_opt->preferred_decoder_names_buf, _reader_opt->preferred_decoder_names_original_buf, PREFERRED_DECODER_NAMES_BUFSIZE);
    _reader_opt->preferred_decoder_names = lw_tokenize_string( _reader_opt->preferred_decoder_names_buf, ',', NULL );
}

static inline void set_cache_dir( reader_option_t *_reader_opt, const char *user_index_dir )
{
    _reader_opt->cache_dir_name = NULL;
    memset( _reader_opt->cache_dir_name_buf, 0, CACHE_DIR_NAME_BUFSIZE);
    memcpy( _reader_opt->cache_dir_name_buf, user_index_dir,
            MIN( CACHE_DIR_NAME_BUFSIZE - 1, strlen(user_index_dir) ) );
    if( _reader_opt->cache_dir_name_buf[0] != '\0' ) {
        size_t edit_len = MIN( CACHE_DIR_NAME_BUFSIZE - 2, strlen(_reader_opt->cache_dir_name_buf) );
        if( _reader_opt->cache_dir_name_buf[edit_len - 1] != '\\' ) {
            _reader_opt->cache_dir_name_buf[edit_len] = '\\';
            _reader_opt->cache_dir_name_buf[edit_len + 1] = '\0';
        }
    }
    if( _reader_opt->use_cache_dir ) {
        DWORD dwAttrib = INVALID_FILE_ATTRIBUTES;
        wchar_t *cache_dir_name_w = NULL;
        if ( lw_string_to_wchar( CP_UTF8, _reader_opt->cache_dir_name_buf, &cache_dir_name_w ) ) {
            dwAttrib = GetFileAttributesW( cache_dir_name_w );
            lw_freep( &cache_dir_name_w );
        }

        if((dwAttrib != INVALID_FILE_ATTRIBUTES) && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY)) {
            _reader_opt->cache_dir_name = _reader_opt->cache_dir_name_buf;
        }
        else {
            wchar_t* default_index_dir_w = NULL;
            if( lw_string_to_wchar( CP_UTF8, default_index_dir, &default_index_dir_w ) ) {
                DWORD dwAttrib_default_index_dir = GetFileAttributesW( default_index_dir_w );
                if( !( ( dwAttrib_default_index_dir != INVALID_FILE_ATTRIBUTES ) && ( dwAttrib_default_index_dir & FILE_ATTRIBUTE_DIRECTORY ) ) )
                    if( !CreateDirectoryW( default_index_dir_w, NULL ) ) {
                        MESSAGE_BOX_DESKTOP( MB_ICONERROR | MB_OK, "Failed to create cache dir." );
                        return;
                    }
                lw_free( default_index_dir_w );
            } else {
                MESSAGE_BOX_DESKTOP( MB_ICONERROR | MB_OK, "Failed to allocate memory for create cache dir." );
            }
            _reader_opt->cache_dir_name = default_index_dir;
        }
    }
}

static void get_settings( lwinput_option_t *_lwinput_opt )
{
    reader_option_t *_reader_opt = &_lwinput_opt->reader_opt;
    video_option_t *_video_opt = &_reader_opt->video_opt;
    audio_option_t *_audio_opt = &_reader_opt->audio_opt;

    FILE *ini = open_settings( "rb" );
    char buf[1024];
    if( ini )
    {
        /* threads */
        if( !fgets( buf, sizeof(buf), ini ) || sscanf( buf, "threads=%d", &_reader_opt->threads ) != 1 )
            _reader_opt->threads = 0;
        /* av_sync */
        if( !fgets( buf, sizeof(buf), ini ) || sscanf( buf, "av_sync=%d", &_reader_opt->av_sync ) != 1 )
            _reader_opt->av_sync = 1;
        /* no_create_index */
        if( !fgets( buf, sizeof(buf), ini ) || sscanf( buf, "no_create_index=%d", &_reader_opt->no_create_index ) != 1 )
            _reader_opt->no_create_index = 0;
        /* force stream index */
        fgets( buf, sizeof(buf), ini ); /* force_video_index is deleted, for compatibility */
        fgets( buf, sizeof(buf), ini ); /* force_audio_index is deleted, for compatibility */
        /* seek_mode */
        if( !fgets( buf, sizeof(buf), ini ) || sscanf( buf, "seek_mode=%d", &_video_opt->seek_mode ) != 1 )
            _video_opt->seek_mode = 0;
        else
            _video_opt->seek_mode = CLIP_VALUE( _video_opt->seek_mode, 0, 2 );
        /* forward_seek_threshold */
        if( !fgets( buf, sizeof(buf), ini ) || sscanf( buf, "forward_threshold=%d", &_video_opt->forward_seek_threshold ) != 1 )
            _video_opt->forward_seek_threshold = 10;
        else
            _video_opt->forward_seek_threshold = CLIP_VALUE( _video_opt->forward_seek_threshold, 1, 999 );
        /* scaler */
        if( !fgets( buf, sizeof(buf), ini ) || sscanf( buf, "scaler=%d", &_video_opt->scaler ) != 1 )
            _video_opt->scaler = 0;
        else
            _video_opt->scaler = CLIP_VALUE( _video_opt->scaler, 0, 10 );
        /* apply_repeat_flag */
        if( !fgets( buf, sizeof(buf), ini ) || sscanf( buf, "apply_repeat_flag=%d", &_video_opt->apply_repeat_flag ) != 1 )
            _video_opt->apply_repeat_flag = 0;
        /* field_dominance */
        if( !fgets( buf, sizeof(buf), ini ) || sscanf( buf, "field_dominance=%d", &_video_opt->field_dominance ) != 1 )
            _video_opt->field_dominance = 0;
        else
            _video_opt->field_dominance = CLIP_VALUE( _video_opt->field_dominance, 0, 2 );
        /* VFR->CFR (deleted, for compatibility ) */
        fgets( buf, sizeof(buf), ini );
        /* LW48 output */
        if( !fgets( buf, sizeof(buf), ini ) || sscanf( buf, "colorspace=%d", (int *)&_video_opt->colorspace ) != 1 )
            _video_opt->colorspace = 0;
        else
            _video_opt->colorspace = _video_opt->colorspace ? OUTPUT_LW48 : 0;
        /* AVS bit-depth */
        if( !fgets( buf, sizeof(buf), ini ) || sscanf( buf, "avs_bit_depth=%d", &_video_opt->avs.bit_depth ) != 1 )
            _video_opt->avs.bit_depth = 8;
        else
        {
            _video_opt->avs.bit_depth = CLIP_VALUE( _video_opt->avs.bit_depth, 8, 16 );
            if( _video_opt->avs.bit_depth > 10 )
                _video_opt->avs.bit_depth = 16;
        }
        /* audio_delay */
        if( !fgets( buf, sizeof(buf), ini ) || sscanf( buf, "audio_delay=%d", &_lwinput_opt->audio_delay ) != 1 )
            _lwinput_opt->audio_delay = 0;
        /* channel_layout */
        if( !fgets( buf, sizeof(buf), ini ) || sscanf( buf, "channel_layout=0x%"SCNx64, &_audio_opt->channel_layout ) != 1 )
            _audio_opt->channel_layout = 0;
        /* sample_rate */
        if( !fgets( buf, sizeof(buf), ini ) || sscanf( buf, "sample_rate=%d", &_audio_opt->sample_rate ) != 1 )
            _audio_opt->sample_rate = 0;
        /* mix_level */
        if( !fgets( buf, sizeof(buf), ini )
         || sscanf( buf, "mix_level=%d:%d:%d",
                    &_audio_opt->mix_level[MIX_LEVEL_INDEX_CENTER  ],
                    &_audio_opt->mix_level[MIX_LEVEL_INDEX_SURROUND],
                    &_audio_opt->mix_level[MIX_LEVEL_INDEX_LFE     ] ) != 3 )
        {
            _audio_opt->mix_level[MIX_LEVEL_INDEX_CENTER  ] = 71;
            _audio_opt->mix_level[MIX_LEVEL_INDEX_SURROUND] = 71;
            _audio_opt->mix_level[MIX_LEVEL_INDEX_LFE     ] = 0;
        }
        else
        {
            _audio_opt->mix_level[MIX_LEVEL_INDEX_CENTER  ] = CLIP_VALUE( _audio_opt->mix_level[MIX_LEVEL_INDEX_CENTER  ], 0, 10000 );
            _audio_opt->mix_level[MIX_LEVEL_INDEX_SURROUND] = CLIP_VALUE( _audio_opt->mix_level[MIX_LEVEL_INDEX_SURROUND], 0, 10000 );
            _audio_opt->mix_level[MIX_LEVEL_INDEX_LFE     ] = CLIP_VALUE( _audio_opt->mix_level[MIX_LEVEL_INDEX_LFE     ], 0, 30000 );
        }
        /* readers */
        if( !fgets( buf, sizeof(buf), ini ) || sscanf( buf, "libavsmash_disabled=%d", &_lwinput_opt->reader_disabled[0] ) != 1 )
            _lwinput_opt->reader_disabled[0] = 0;
        if( !fgets( buf, sizeof(buf), ini ) || sscanf( buf, "avs_disabled=%d",        &_lwinput_opt->reader_disabled[1] ) != 1 )
            _lwinput_opt->reader_disabled[1] = 0;
        if( !fgets( buf, sizeof(buf), ini ) || sscanf( buf, "vpy_disabled=%d",        &_lwinput_opt->reader_disabled[2] ) != 1 )
            _lwinput_opt->reader_disabled[2] = 0;
        if( !fgets( buf, sizeof(buf), ini ) || sscanf( buf, "libav_disabled=%d",      &_lwinput_opt->reader_disabled[3] ) != 1 )
            _lwinput_opt->reader_disabled[3] = 0;
        /* dummy reader */
        fgets( buf, sizeof(buf), ini ); // dummy_resolution is deleted, for compatibility
        fgets( buf, sizeof(buf), ini ); // dummy_framerate is deleted, for compatibility
        fgets( buf, sizeof(buf), ini ); // dummy_colorspace is deleted, for compatibility
        /* preferred decoders settings */
        char preferred_decoder_names[512] = { 0 };
        if( !fgets( buf, sizeof(buf), ini ) || sscanf( buf, "preferred_decoders=%s", preferred_decoder_names ) != 1 )
            clean_preferred_decoder_names( _reader_opt );
        else
            set_preferred_decoder_names_on_buf( _reader_opt, preferred_decoder_names );
        /* handle cache (deleted, for compatibility) */
        fgets( buf, sizeof(buf), ini );
        /* use cache dir */
        if( !fgets( buf, sizeof(buf), ini ) || sscanf( buf, "use_cache_dir=%d", &_reader_opt->use_cache_dir ) != 1 )
            _reader_opt->use_cache_dir = 1;
            
        char user_index_dir[_MAX_PATH * 4] = { 0 };
        if( !fgets( buf, sizeof(buf), ini ) || sscanf( buf, "cache_dir_path=%s", user_index_dir ) != 1 )
            set_cache_dir( _reader_opt, "" );
        else {
            // "sscanf" abort loading with a space character.
            buf[strlen(buf) - 1] = '\0';
            set_cache_dir( _reader_opt, buf + 15 );
        }
        /* delete old cache */
        if ( !fgets( buf, sizeof(buf), ini ) || sscanf( buf, "delete_old_cache=%d", &_lwinput_opt->delete_old_cache ) != 1 )
            _lwinput_opt->delete_old_cache = 1;
        /* delete old cache days */
        if ( !fgets( buf, sizeof(buf), ini ) || sscanf( buf, "delete_old_cache_days=%d", &_lwinput_opt->delete_old_cache_days ) != 1 )
            _lwinput_opt->delete_old_cache_days = 30;
        else
            _lwinput_opt->delete_old_cache_days = MAX(_lwinput_opt->delete_old_cache_days, 2);
        /* cache last check date */
        if ( !fgets( buf, sizeof(buf), ini ) || sscanf( buf, "cache_last_check_date=%"SCNu64, &_lwinput_opt->cache_last_check_date ) != 1 )
            _lwinput_opt->cache_last_check_date = 0;
        /* wide dialog */
        if ( !fgets( buf, sizeof(buf), ini ) || sscanf( buf, "wide_dialog=%d", &_lwinput_opt->wide_dialog ) != 1 )
            _lwinput_opt->wide_dialog = 0;

        fclose( ini );
    }
    else
    {
        /* Set up defalut values. */
        clean_preferred_decoder_names( _reader_opt );
        _reader_opt->threads                = 0;
        _reader_opt->av_sync                = 1;
        _reader_opt->no_create_index        = 0;
        _reader_opt->force_video            = 0;
        _reader_opt->force_video_index      = -1;
        _reader_opt->force_audio            = 0;
        _reader_opt->force_audio_index      = -1;
        _reader_opt->use_cache_dir          = 1;
        _reader_opt->cache_dir_name         = NULL;
        _lwinput_opt->reader_disabled[0]    = 0;
        _lwinput_opt->reader_disabled[1]    = 0;
        _lwinput_opt->reader_disabled[2]    = 0;
        _lwinput_opt->reader_disabled[3]    = 0;
        _video_opt->seek_mode               = 0;
        _video_opt->forward_seek_threshold  = 10;
        _video_opt->scaler                  = 0;
        _video_opt->apply_repeat_flag       = 1;
        _video_opt->field_dominance         = 0;
        _video_opt->colorspace              = 0;
        _video_opt->avs.bit_depth           = 8;
        _lwinput_opt->audio_delay           = 0;
        _lwinput_opt->delete_old_cache      = 1;
        _lwinput_opt->delete_old_cache_days = 30;
        _lwinput_opt->cache_last_check_date = 0;
        _lwinput_opt->wide_dialog           = 0;
        _audio_opt->mix_level[MIX_LEVEL_INDEX_CENTER  ] = 71;
        _audio_opt->mix_level[MIX_LEVEL_INDEX_SURROUND] = 71;
        _audio_opt->mix_level[MIX_LEVEL_INDEX_LFE     ] = 0;
        set_cache_dir( _reader_opt, "" );
    }
}

static void save_settings( lwinput_option_t *_lwinput_opt ) {
    reader_option_t *_reader_opt = &_lwinput_opt->reader_opt;
    video_option_t *_video_opt = &_reader_opt->video_opt;
    audio_option_t *_audio_opt = &_reader_opt->audio_opt;

    /* Open */
    FILE *ini = open_settings( "wb" );
    if( !ini )
    {
        MESSAGE_BOX_DESKTOP( MB_ICONERROR | MB_OK, "Failed to update configuration file" );
        return;
    }
    /* threads */
    if( _reader_opt->threads > 0 )
        fprintf( ini, "threads=%d\n", _reader_opt->threads );
    else
        fprintf( ini, "threads=0 (auto)\n" );
    /* av_sync */
    fprintf( ini, "av_sync=%d\n", _reader_opt->av_sync );
    /* no_create_index */
    fprintf( ini, "no_create_index=%d\n", _reader_opt->no_create_index );
    /* force stream index (deleted, for compatibility) */
    fprintf( ini, "force_video_index=%d:%d\n", 0, -1 );
    fprintf( ini, "force_audio_index=%d:%d\n", 0, -1 );
    /* seek_mode */
    fprintf( ini, "seek_mode=%d\n", _video_opt->seek_mode );
    /* forward_seek_threshold */
    fprintf( ini, "forward_threshold=%d\n", _video_opt->forward_seek_threshold );
    /* scaler */
    fprintf( ini, "scaler=%d\n", _video_opt->scaler );
    /* apply_repeat_flag */
    fprintf( ini, "apply_repeat_flag=%d\n", _video_opt->apply_repeat_flag );
    /* field_dominance */
    fprintf( ini, "field_dominance=%d\n", _video_opt->field_dominance );
    /* VFR->CFR (deleted, for compatibility) */
    fprintf( ini, "vfr2cfr=%d:%d:%d\n", 0, 60000, 1001 );
    /* LW48 output */
    fprintf( ini, "colorspace=%d\n", _video_opt->colorspace );
    /* AVS bit-depth */
    fprintf( ini, "avs_bit_depth=%d\n", _video_opt->avs.bit_depth );
    /* audio_delay */
    fprintf( ini, "audio_delay=%d\n", _lwinput_opt->audio_delay );
    /* channel_layout */
    fprintf( ini, "channel_layout=0x%"PRIx64"\n", _audio_opt->channel_layout );
    /* sample_rate */
    fprintf( ini, "sample_rate=%d\n", _audio_opt->sample_rate );
    /* mix_level */
    fprintf( ini, "mix_level=%d:%d:%d\n",
                             _audio_opt->mix_level[MIX_LEVEL_INDEX_CENTER  ],
                             _audio_opt->mix_level[MIX_LEVEL_INDEX_SURROUND],
                             _audio_opt->mix_level[MIX_LEVEL_INDEX_LFE     ] );
    /* readers */
    fprintf( ini, "libavsmash_disabled=%d\n", _lwinput_opt->reader_disabled[0] );
    fprintf( ini, "avs_disabled=%d\n",        _lwinput_opt->reader_disabled[1] );
    fprintf( ini, "vpy_disabled=%d\n",        _lwinput_opt->reader_disabled[2] );
    fprintf( ini, "libav_disabled=%d\n",      _lwinput_opt->reader_disabled[3] );
    /* dummy reader (deleted, for compatibility) */
    fprintf( ini, "dummy_resolution=%dx%d\n", 720, 480 );
    fprintf( ini, "dummy_framerate=%d/%d\n",  24, 1 );
    fprintf( ini, "dummy_colorspace=%d\n",    0 );
    /* preferred decoders */
    fprintf( ini, "preferred_decoders=%s\n", _reader_opt->preferred_decoder_names_original_buf );
    /* handle cache (deleted, for compatibility) */
    fprintf( ini, "handle_cache=%d\n", 0 );
    /* use cache dir */
    fprintf( ini, "use_cache_dir=%d\n", _reader_opt->use_cache_dir );
    /* cache dir path */
    fprintf( ini, "cache_dir_path=%s\n", _reader_opt->cache_dir_name_buf );
    /* delete old cache */
    fprintf( ini, "delete_old_cache=%d\n", _lwinput_opt->delete_old_cache );
    /* delete old cache days */
    fprintf( ini, "delete_old_cache_days=%d\n", _lwinput_opt->delete_old_cache_days );
    /* cache last check date */
    fprintf( ini, "cache_last_check_date=%"PRIu64"\n", _lwinput_opt->cache_last_check_date );
    /* wide dialog */
    fprintf( ini, "wide_dialog=%d\n", _lwinput_opt->wide_dialog );

    /* Close */
    fclose( ini );
}

static void delete_old_cache( void )
{
    lwinput_option_t _lwinput_opt = { 0 };
    reader_option_t *_reader_opt = &_lwinput_opt.reader_opt;

    get_settings( &_lwinput_opt );

    if ( !_reader_opt->use_cache_dir || !_lwinput_opt.delete_old_cache )
        return;

    wchar_t *cache_dir_name_w, *search_path, *file_path_buf;
    lw_string_to_wchar( CP_UTF8, _reader_opt->cache_dir_name, &cache_dir_name_w );
    size_t cache_dir_name_w_size = wcslen( cache_dir_name_w ) + 1, search_path_size = cache_dir_name_w_size + 1, file_path_buf_size = cache_dir_name_w_size + 260;
    search_path = (wchar_t*)lw_malloc_zero( search_path_size * sizeof( wchar_t ) );
    file_path_buf = (wchar_t*)lw_malloc_zero( file_path_buf_size * sizeof( wchar_t ) );

    wcscpy_s( search_path, search_path_size, cache_dir_name_w );
    wcscat_s( search_path, search_path_size, L"*" );

    HWND hFind = INVALID_HANDLE_VALUE;
    WIN32_FIND_DATAW win32fd;
    if ( ( hFind = FindFirstFileW( search_path, &win32fd ) ) == INVALID_HANDLE_VALUE )
        goto fail_delete_old_cache;

    SYSTEMTIME s_st;
    FILETIME f_st;
    ULARGE_INTEGER u_st, u_ft;
    uint64_t diff;
    const uint64_t to_day_ratio = 864000000000;
    GetSystemTime(&s_st);
    if ( !SystemTimeToFileTime(&s_st, &f_st) )
        goto fail_delete_old_cache;
    u_st.HighPart = f_st.dwHighDateTime;
    u_st.LowPart = f_st.dwLowDateTime;
    u_st.QuadPart /= to_day_ratio;

    if ( u_st.QuadPart == _lwinput_opt.cache_last_check_date )
        goto fail_delete_old_cache;

    _lwinput_opt.cache_last_check_date = u_st.QuadPart;

    do
    {
        if ( win32fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
            continue;

        wchar_t* p = win32fd.cFileName;
        while( *p != L'\0' )
                p++;
        while( *p != L'.' )
                p--;
        if ( wcscmp( p, L".lwi" ) )
            continue;

        u_ft.HighPart = win32fd.ftLastAccessTime.dwHighDateTime;
        u_ft.LowPart = win32fd.ftLastAccessTime.dwLowDateTime;
        u_ft.QuadPart /= to_day_ratio;
        diff = u_st.QuadPart - u_ft.QuadPart;

        if ( diff >= _lwinput_opt.delete_old_cache_days )
        {
            wcscpy_s( file_path_buf, file_path_buf_size, cache_dir_name_w );
            wcscat_s( file_path_buf, file_path_buf_size, win32fd.cFileName );
            if ( !DeleteFileW( file_path_buf ) )
                break;
        }
    } while ( FindNextFileW( hFind, &win32fd ) );

    FindClose( hFind );

    save_settings( &_lwinput_opt );
fail_delete_old_cache:
    lw_free( file_path_buf );
    lw_free( search_path );
    lw_free( cache_dir_name_w );
}

BOOL func_init( void ) {
    if( lw_GetModuleFileNameUTF8( hModuleDLL, &plugin_dir ) ) {
        char* p = plugin_dir;
        while(*p != '\0')
                p++;
        while(*p != '\\')
                p--;
        p++;
        *p = '\0';

        size_t size = strlen( plugin_dir ) + 5;
        default_index_dir = lw_malloc_zero( size );
        if ( default_index_dir ) {
            strcpy_s(default_index_dir, size, plugin_dir );
            strcat_s(default_index_dir, size, "lwi\\");
        }
    }

    get_settings( &lwinput_opt );
    delete_old_cache();
    return TRUE;
}

BOOL func_exit( void ) {
    delete_old_cache();
    clean_preferred_decoder_names( reader_opt );
    clean_preferred_decoder_names( reader_opt_config );
    lw_freep( &default_index_dir );
    lw_freep( &plugin_dir );
    return TRUE;
}

INPUT_HANDLE func_open( LPCWSTR filew )
{
    char* file = NULL;
    lw_string_from_wchar( CP_UTF8, filew, &file );
    if( !file )
        return NULL;

    lsmash_handler_t *hp = (lsmash_handler_t *)lw_malloc_zero( sizeof(lsmash_handler_t) );
    if( !hp ) {
        lw_free( file );
        return NULL;
    }
    hp->video_reader = READER_NONE;
    hp->audio_reader = READER_NONE;
    if( reader_opt->threads <= 0 )
        reader_opt->threads = get_auto_threads();
    extern lsmash_reader_t libavsmash_reader;
    extern lsmash_reader_t avs_reader;
    extern lsmash_reader_t vpy_reader;
    extern lsmash_reader_t libav_reader;
    enum
    {
        AU_VIDEO_READER  = 1,
        AU_SCRIPT_READER = 2
    };
    static const struct
    {
        lsmash_reader_t *reader;
        int              attribute;
    } lsmash_reader_table[] =
        {
            { &libavsmash_reader, AU_VIDEO_READER  },
            { &avs_reader       , AU_SCRIPT_READER },
            { &vpy_reader       , AU_SCRIPT_READER },
            { &libav_reader     , AU_VIDEO_READER  },
            { NULL              , 0                }
        };
    for( int i = 0; lsmash_reader_table[i].reader; i++ )
    {
        if( lwinput_opt.reader_disabled[lsmash_reader_table[i].reader->type - 1] )
            continue;
        int video_none = 1;
        int audio_none = 1;
        lsmash_reader_t reader      = *lsmash_reader_table[i].reader;
        int             reader_attr =  lsmash_reader_table[i].attribute;
        void *private_stuff = reader.open_file( file, reader_opt );
        if( private_stuff )
        {
            if( !hp->video_private )
            {
                hp->video_private = private_stuff;
                if( reader.find_video
                 && reader.find_video( hp, video_opt ) == 0 )
                {
                    hp->video_reader             = reader.type;
                    hp->get_video_track          = reader.get_video_track;
                    hp->read_video               = reader.read_video;
                    hp->is_keyframe              = reader.is_keyframe;
                    hp->video_cleanup            = reader.video_cleanup;
                    hp->destroy_video_disposable = reader.destroy_disposable;
                    hp->close_video_file         = reader.close_file;
                    hp->time_to_frame            = reader.time_to_frame;
                    video_none = 0;
                }
                else
                    hp->video_private = NULL;
            }
            if( !hp->audio_private )
            {
                hp->audio_private = private_stuff;
                if( reader.find_audio
                 && reader.find_audio( hp, audio_opt ) == 0 )
                {
                    hp->audio_reader             = reader.type;
                    hp->get_audio_track          = reader.get_audio_track;
                    hp->read_audio               = reader.read_audio;
                    hp->delay_audio              = reader.delay_audio;
                    hp->audio_cleanup            = reader.audio_cleanup;
                    hp->destroy_audio_disposable = reader.destroy_disposable;
                    hp->close_audio_file         = reader.close_file;
                    audio_none = 0;
                }
                else
                    hp->audio_private = NULL;
            }
        }
        if( video_none && audio_none )
        {
            if( reader.close_file )
                reader.close_file( private_stuff );
        }
        /* Found both video and audio reader. */
        if( hp->video_reader != READER_NONE && hp->audio_reader != READER_NONE )
            break;
        if( reader_attr == AU_SCRIPT_READER )
        {
            if( hp->video_reader == reader.type )
                break;
            if( hp->audio_reader == reader.type )
                i = LIBAV_READER - 1;
        }
    }
    if( hp->video_reader == hp->audio_reader )
    {
        hp->global_private           = hp->video_private;
        hp->destroy_disposable       = hp->destroy_video_disposable;
        hp->destroy_video_disposable = NULL;
        hp->destroy_audio_disposable = NULL;
        hp->close_file               = hp->close_video_file;
        hp->close_video_file         = NULL;
        hp->close_audio_file         = NULL;
    }
    if( hp->video_reader == READER_NONE && hp->audio_reader == READER_NONE )
    {
        DEBUG_MESSAGE_BOX_DESKTOP( MB_OK, "No readable video and/or audio stream" );
        func_close( hp );
        lw_free( file );
        return NULL;
    }

    lw_free( file );
    return hp;
}

int func_set_track( INPUT_HANDLE ih, int type, int index )
{
    lsmash_handler_t *hp = (lsmash_handler_t *)ih;
    if( index == -1 )
    {
        if( type == TRACK_TYPE_VIDEO )
            return hp->video_track_count;
        else if( type == TRACK_TYPE_AUDIO )
            return hp->audio_track_count;

        return 0;
    }

    if( type == TRACK_TYPE_VIDEO )
    {
        if( hp->get_video_track ) {
            int ret = hp->get_video_track( hp, reader_opt, index );
            
            if( hp->destroy_video_disposable )
                hp->destroy_video_disposable( hp->video_private );
            
            return ret;
        }
        return -1;
    }
    else if( type == TRACK_TYPE_AUDIO )
    {
        if( hp->get_audio_track ) {
            int ret = hp->get_audio_track( hp, reader_opt, index );

            if( hp->destroy_audio_disposable )
                hp->destroy_audio_disposable( hp->audio_private );

            /* Assuming that called in the order of video -> audio */
            if( hp->destroy_disposable )
                hp->destroy_disposable( hp->global_private );

            return ret;
        }
        return -1;
    }

    return -1;
}

bool func_close( INPUT_HANDLE ih )
{
    lsmash_handler_t *hp = (lsmash_handler_t *)ih;
    if( !hp )
        return TRUE;
    if( hp->video_cleanup )
        hp->video_cleanup( hp );
    if( hp->audio_cleanup )
        hp->audio_cleanup( hp );
    if( hp->close_file )
        hp->close_file( hp->global_private );
    else
    {
        if( hp->close_video_file )
            hp->close_video_file( hp->video_private );
        if( hp->close_audio_file )
            hp->close_audio_file( hp->audio_private );
    }
    lw_free( hp );
    return TRUE;
}

bool func_info_get( INPUT_HANDLE ih, INPUT_INFO *iip )
{
    if( !ih || !iip )
        return FALSE;

    lsmash_handler_t *hp = (lsmash_handler_t *)ih;
    memset( iip, 0, sizeof(INPUT_INFO) );
    if( hp->video_reader != READER_NONE )
    {
        iip->flag             |= INPUT_INFO_FLAG_VIDEO;
        if( hp->time_to_frame )
            iip->flag         |= INPUT_INFO_TIME_TO_FRAME;
        iip->rate              = hp->framerate_num;
        iip->scale             = hp->framerate_den;
        iip->n                 = hp->video_sample_count;
        iip->format            = &hp->video_format;
        iip->format_size       = hp->video_format.biSize;
    }
    if( hp->audio_reader != READER_NONE )
    {
        iip->flag             |= INPUT_INFO_FLAG_AUDIO;
        iip->audio_n           = hp->audio_pcm_sample_count + lwinput_opt.audio_delay;
        iip->audio_format      = &hp->audio_format.Format;
        iip->audio_format_size = sizeof( WAVEFORMATEX ) + hp->audio_format.Format.cbSize;
    }
    return TRUE;
}

int func_read_video( INPUT_HANDLE ih, int sample_number, void *buf )
{
    if( !ih || !buf )
        return 0;

    lsmash_handler_t *hp = (lsmash_handler_t *)ih;
    return hp->read_video ? hp->read_video( hp, sample_number, buf ) : 0;
}

int func_read_audio( INPUT_HANDLE ih, int start, int length, void *buf )
{
    if( !ih || !buf )
        return 0;

    lsmash_handler_t *hp = (lsmash_handler_t *)ih;
    if( hp->read_audio && hp->delay_audio( hp, &start, length, lwinput_opt.audio_delay ) )
        return hp->read_audio( hp, start, length, buf );
    uint8_t silence = hp->audio_format.Format.wBitsPerSample == 8 ? 128 : 0;
    memset( buf, silence, length * hp->audio_format.Format.nBlockAlign );
    return length;
}

int func_time_to_frame( INPUT_HANDLE ih, double time )
{
    if( !ih )
        return 0;

    lsmash_handler_t *hp = (lsmash_handler_t *)ih;
    return hp->time_to_frame ? hp->time_to_frame( hp, time ) : time * hp->framerate_num / hp->framerate_den;
}

static inline void set_buddy_window_for_updown_control
(
    HWND hwnd,
    int  spin_idc,
    int  buddy_idc
)
{
    SendMessage( GetDlgItem( hwnd, spin_idc ), UDM_SETBUDDY, (WPARAM)GetDlgItem( hwnd, buddy_idc ), 0 );
}

static inline void set_check_state
(
    HWND hwnd,
    int  idc,   /* identifier for control */
    int  value
)
{
    SendMessage( GetDlgItem( hwnd, idc ), BM_SETCHECK, (WPARAM) value ? BST_CHECKED : BST_UNCHECKED, 0 );
}

static inline int get_check_state
(
    HWND hwnd,
    int  idc    /* identifier for control */
)
{
    return (BST_CHECKED == SendMessage( GetDlgItem( hwnd, idc ), BM_GETCHECK, 0, 0 ));
}

static void set_int_to_dlg
(
    HWND hwnd,
    int  idc,   /* identifier for control */
    int  value  /* message value */
)
{
    char edit_buf[512];
    sprintf( edit_buf, "%d", value );
    SetDlgItemTextA( hwnd, idc, edit_buf );
}

static int get_int_from_dlg
(
    HWND hwnd,
    int  idc    /* identifier for control */
)
{
    char edit_buf[512];
    GetDlgItemTextA( hwnd, idc, edit_buf, sizeof(edit_buf) );
    return atoi( edit_buf );
}

static int get_int_from_dlg_with_min
(
    HWND hwnd,
    int  idc,   /* identifier for control */
    int  min
)
{
    int value = get_int_from_dlg( hwnd, idc );
    return MAX( value, min );
}

static inline void set_string_to_dlg
(
    HWND  hwnd,
    int   idc,  /* identifier for control */
    char *value /* message value */
)
{
    wchar_t *value_w = NULL;
    if( lw_string_to_wchar( CP_UTF8, value, &value_w) ) {
        SetDlgItemTextW( hwnd, idc, value_w );
        lw_freep( &value_w );
    } else {
        SetDlgItemTextA( hwnd, idc, value );
    }
}

static void send_mix_level
(
    HWND  hwnd,
    int   slider_idc,
    int   text_idc,
    int   range_min,
    int   range_max,
    int   mix_level
)
{
    char edit_buf[512];
    HWND hslider = GetDlgItem( hwnd, slider_idc );
    SendMessage( hslider, TBM_SETRANGE,    TRUE, MAKELPARAM( range_min, range_max ) );
    SendMessage( hslider, TBM_SETTICFREQ,  1,    0 );
    SendMessage( hslider, TBM_SETPOS,      TRUE, mix_level );
    SendMessage( hslider, TBM_SETLINESIZE, 0,    1 );
    SendMessage( hslider, TBM_SETPAGESIZE, 0,    1 );
    sprintf( edit_buf, "%.2f", mix_level / 100.0 );
    SetWindowTextA( GetDlgItem( hwnd, text_idc ), edit_buf );
}

static void get_mix_level
(
    HWND  hwnd,
    int   slider_idc,
    int   text_idc,
    int  *mix_level
)
{
    char edit_buf[512];
    HWND hslider = GetDlgItem( hwnd, slider_idc );
    *mix_level = SendMessage( hslider, TBM_GETPOS, 0, 0 );
    sprintf( edit_buf, "%.2f", *mix_level / 100.0 );
    SetWindowTextA( GetDlgItem( hwnd, text_idc ), edit_buf );
}

static INT_PTR CALLBACK dialog_proc
(
    HWND   hwnd,
    UINT   message,
    WPARAM wparam,
    LPARAM lparam
)
{
    switch( message )
    {
        case WM_INITDIALOG :
            InitCommonControls();
            get_settings( &lwinput_opt_config );
            /* threads */
            set_int_to_dlg( hwnd, IDC_EDIT_THREADS, reader_opt_config->threads );
            set_buddy_window_for_updown_control( hwnd, IDC_SPIN_THREADS, IDC_EDIT_THREADS );
            /* av_sync */
            set_check_state( hwnd, IDC_CHECK_AV_SYNC, reader_opt_config->av_sync );
            /* no_create_index */
            set_check_state( hwnd, IDC_CHECK_CREATE_INDEX_FILE, !reader_opt_config->no_create_index );
            /* forward_seek_threshold */
            set_int_to_dlg( hwnd, IDC_EDIT_FORWARD_THRESHOLD, video_opt_config->forward_seek_threshold );
            set_buddy_window_for_updown_control( hwnd, IDC_SPIN_FORWARD_THRESHOLD, IDC_EDIT_FORWARD_THRESHOLD );
            /* seek mode */
            HWND hcombo = GetDlgItem( hwnd, IDC_COMBOBOX_SEEK_MODE );
            for( int i = 0; i < 3; i++ )
                SendMessageA( hcombo, CB_ADDSTRING, 0, (LPARAM)seek_mode_list[i] );
            SendMessageA( hcombo, CB_SETCURSEL, video_opt_config->seek_mode, 0 );
            /* scaler */
            hcombo = GetDlgItem( hwnd, IDC_COMBOBOX_SCALER );
            for( int i = 0; i < 11; i++ )
                SendMessageA( hcombo, CB_ADDSTRING, 0, (LPARAM)scaler_list[i] );
            SendMessageA( hcombo, CB_SETCURSEL, video_opt_config->scaler, 0 );
            /* apply_repeat_flag */
            set_check_state( hwnd, IDC_CHECK_APPLY_REPEAT_FLAG, video_opt_config->apply_repeat_flag );
            /* field_dominance */
            hcombo = GetDlgItem( hwnd, IDC_COMBOBOX_FIELD_DOMINANCE );
            for( int i = 0; i < 3; i++ )
                SendMessageA( hcombo, CB_ADDSTRING, 0, (LPARAM)field_dominance_list[i] );
            SendMessageA( hcombo, CB_SETCURSEL, video_opt_config->field_dominance, 0 );
            /* LW48 output */
            set_check_state( hwnd, IDC_CHECK_LW48_OUTPUT, video_opt_config->colorspace != 0 );
            /* AVS bit-depth */
            hcombo = GetDlgItem( hwnd, IDC_COMBOBOX_AVS_BITDEPTH );
            for( int i = 0; i < 4; i++ )
            {
                SendMessageA( hcombo, CB_ADDSTRING, 0, (LPARAM)avs_bit_depth_list[i] );
                if( video_opt_config->avs.bit_depth == atoi( avs_bit_depth_list[i] ) )
                    SendMessageA( hcombo, CB_SETCURSEL, i, 0 );
            }
            /* audio_delay */
            set_int_to_dlg( hwnd, IDC_EDIT_AUDIO_DELAY, lwinput_opt_config.audio_delay );
            /* channel_layout */
            if( audio_opt_config->channel_layout )
            {
                char edit_buf[512] = { 0 };
                char *buf = edit_buf;
                AVChannelLayout ch_layout;
                av_channel_layout_from_mask( &ch_layout, audio_opt_config->channel_layout );
                for( int i = 0; i < 64; i++ )
                {
                    enum AVChannel ch = av_channel_layout_channel_from_index( &ch_layout, i );
                    if( ch == AV_CHAN_NONE )
                        continue;
                    int name_length = av_channel_name( buf, sizeof(edit_buf) - (buf - edit_buf), ch ) - 1;
                    if ( name_length > 0 )
                    {
                        buf += name_length;
                        *(buf++) = '+';
                    }
                }
                av_channel_layout_uninit( &ch_layout );
                if( buf > edit_buf )
                {
                    *(buf - 1) = '\0';  /* Replace the last '+' with NULL terminator. */
                    SetDlgItemTextA( hwnd, IDC_EDIT_CHANNEL_LAYOUT, edit_buf );
                }
                else
                    set_string_to_dlg( hwnd, IDC_EDIT_CHANNEL_LAYOUT, "Unspecified" );
            }
            else
                set_string_to_dlg( hwnd, IDC_EDIT_CHANNEL_LAYOUT, "Unspecified" );
            /* sample_rate */
            if( audio_opt_config->sample_rate > 0 )
                set_int_to_dlg( hwnd, IDC_EDIT_SAMPLE_RATE, audio_opt_config->sample_rate );
            else
            {
                audio_opt_config->sample_rate = 0;
                set_string_to_dlg( hwnd, IDC_EDIT_SAMPLE_RATE, "0 (Auto)" );
            }
            /* mix_level */
            send_mix_level( hwnd, IDC_SLIDER_MIX_LEVEL_CENTER,   IDC_TEXT_MIX_LEVEL_CENTER,   0, 500, audio_opt_config->mix_level[MIX_LEVEL_INDEX_CENTER  ] );
            send_mix_level( hwnd, IDC_SLIDER_MIX_LEVEL_SURROUND, IDC_TEXT_MIX_LEVEL_SURROUND, 0, 500, audio_opt_config->mix_level[MIX_LEVEL_INDEX_SURROUND] );
            send_mix_level( hwnd, IDC_SLIDER_MIX_LEVEL_LFE,      IDC_TEXT_MIX_LEVEL_LFE,      0, 500, audio_opt_config->mix_level[MIX_LEVEL_INDEX_LFE     ] );
            /* readers */
            set_check_state( hwnd, IDC_CHECK_LIBAVSMASH_INPUT, !lwinput_opt_config.reader_disabled[0] );
            set_check_state( hwnd, IDC_CHECK_AVS_INPUT,        !lwinput_opt_config.reader_disabled[1] );
            set_check_state( hwnd, IDC_CHECK_VPY_INPUT,        !lwinput_opt_config.reader_disabled[2] );
            set_check_state( hwnd, IDC_CHECK_LIBAV_INPUT,      !lwinput_opt_config.reader_disabled[3] );
            /* preferred decoders */
            if( reader_opt_config->preferred_decoder_names )
            {
                char edit_buf[512] = { 0 };
                char *buf = edit_buf;
                for( const char **decoder = reader_opt_config->preferred_decoder_names; *decoder != NULL; decoder++ )
                {
                    if( *decoder != *reader_opt_config->preferred_decoder_names )
                        *(buf++) = ',';
                    int length = strlen( *decoder );
                    memcpy( buf, *decoder, length );
                    buf += length;
                }
                set_string_to_dlg( hwnd, IDC_EDIT_PREFERRED_DECODERS, edit_buf );
            }
            else
                set_string_to_dlg( hwnd, IDC_EDIT_PREFERRED_DECODERS, "" );
            /* cache dir path */
            set_string_to_dlg( hwnd, IDC_EDIT_CACHE_DIR_PATH, reader_opt_config->cache_dir_name_buf );
            /* Library informations */
            if( plugin_information[0] == 0 )
                get_plugin_information();
            SetDlgItemTextA( hwnd, IDC_TEXT_LIBRARY_INFO, plugin_information );
            HFONT hfont = (HFONT)GetStockObject( DEFAULT_GUI_FONT );
            LOGFONT lf = { 0 };
            GetObject( hfont, sizeof(lf), &lf );
            lf.lfWidth  *= 0.90;
            lf.lfHeight *= 0.90;
            lf.lfQuality = ANTIALIASED_QUALITY;
            SendMessage( GetDlgItem( hwnd, IDC_TEXT_LIBRARY_INFO ), WM_SETFONT, (WPARAM)CreateFontIndirect( &lf ), 1 );
            /* use cache dir */
            set_check_state( hwnd, IDC_CHECK_USE_CACHE_DIR, reader_opt_config->use_cache_dir );
            /* delete old cache */
            set_check_state( hwnd, IDC_CHECK_DELETE_OLD_CACHE, lwinput_opt_config.delete_old_cache );
            /* delete old cache days */
            set_int_to_dlg( hwnd, IDC_EDIT_DELETE_OLD_CACHE_DAYS, lwinput_opt_config.delete_old_cache_days );
            /* wide dialog */
            set_check_state( hwnd, IDC_CHECK_WIDE_DIALOG, lwinput_opt_config.wide_dialog );
            return TRUE;
        case WM_NOTIFY :
            if( wparam == IDC_SPIN_THREADS )
            {
                LPNMUPDOWN lpnmud = (LPNMUPDOWN)lparam;
                if( lpnmud->hdr.code == UDN_DELTAPOS )
                {
                    reader_opt_config->threads = get_int_from_dlg( hwnd, IDC_EDIT_THREADS );
                    if( lpnmud->iDelta )
                        reader_opt_config->threads += lpnmud->iDelta > 0 ? -1 : 1;
                    if( reader_opt_config->threads < 0 )
                        reader_opt_config->threads = 0;
                    set_int_to_dlg( hwnd, IDC_EDIT_THREADS, reader_opt_config->threads );
                }
            }
            else if( wparam == IDC_SPIN_FORWARD_THRESHOLD )
            {
                LPNMUPDOWN lpnmud = (LPNMUPDOWN)lparam;
                if( lpnmud->hdr.code == UDN_DELTAPOS )
                {
                    video_opt_config->forward_seek_threshold = get_int_from_dlg( hwnd, IDC_EDIT_FORWARD_THRESHOLD );
                    if( lpnmud->iDelta )
                        video_opt_config->forward_seek_threshold += lpnmud->iDelta > 0 ? -1 : 1;
                    video_opt_config->forward_seek_threshold = CLIP_VALUE( video_opt_config->forward_seek_threshold, 1, 999 );
                    set_int_to_dlg( hwnd, IDC_EDIT_FORWARD_THRESHOLD, video_opt_config->forward_seek_threshold );
                }
            }
            return TRUE;
        case WM_HSCROLL :
            if( GetDlgItem( hwnd, IDC_SLIDER_MIX_LEVEL_CENTER ) == (HWND)lparam )
                get_mix_level( hwnd, IDC_SLIDER_MIX_LEVEL_CENTER,   IDC_TEXT_MIX_LEVEL_CENTER,   &audio_opt_config->mix_level[MIX_LEVEL_INDEX_CENTER  ] );
            else if( GetDlgItem( hwnd, IDC_SLIDER_MIX_LEVEL_SURROUND ) == (HWND)lparam )
                get_mix_level( hwnd, IDC_SLIDER_MIX_LEVEL_SURROUND, IDC_TEXT_MIX_LEVEL_SURROUND, &audio_opt_config->mix_level[MIX_LEVEL_INDEX_SURROUND] );
            else if( GetDlgItem( hwnd, IDC_SLIDER_MIX_LEVEL_LFE ) == (HWND)lparam )
                get_mix_level( hwnd, IDC_SLIDER_MIX_LEVEL_LFE,      IDC_TEXT_MIX_LEVEL_LFE,      &audio_opt_config->mix_level[MIX_LEVEL_INDEX_LFE     ] );
            return FALSE;
        case WM_COMMAND :
            switch( wparam )
            {
                case IDCANCEL :
                    EndDialog( hwnd, IDCANCEL );
                    return TRUE;
                case IDOK :
                {
                    /* threads */
                    reader_opt_config->threads = get_int_from_dlg_with_min( hwnd, IDC_EDIT_THREADS, 0 );
                    /* av_sync */
                    reader_opt_config->av_sync = get_check_state( hwnd, IDC_CHECK_AV_SYNC );
                    /* no_create_index */
                    reader_opt_config->no_create_index = !get_check_state( hwnd, IDC_CHECK_CREATE_INDEX_FILE );
                    /* seek_mode */
                    video_opt_config->seek_mode = SendMessageA( GetDlgItem( hwnd, IDC_COMBOBOX_SEEK_MODE ), CB_GETCURSEL, 0, 0 );
                    /* forward_seek_threshold */
                    video_opt_config->forward_seek_threshold = get_int_from_dlg( hwnd, IDC_EDIT_FORWARD_THRESHOLD );
                    video_opt_config->forward_seek_threshold = CLIP_VALUE( video_opt_config->forward_seek_threshold, 1, 999 );
                    /* scaler */
                    video_opt_config->scaler = SendMessageA( GetDlgItem( hwnd, IDC_COMBOBOX_SCALER ), CB_GETCURSEL, 0, 0 );
                    /* apply_repeat_flag */
                    video_opt_config->apply_repeat_flag = get_check_state( hwnd, IDC_CHECK_APPLY_REPEAT_FLAG );
                    /* field_dominance */
                    video_opt_config->field_dominance = SendMessageA( GetDlgItem( hwnd, IDC_COMBOBOX_FIELD_DOMINANCE ), CB_GETCURSEL, 0, 0 );
                    /* LW48 output */
                    video_opt_config->colorspace = (get_check_state( hwnd, IDC_CHECK_LW48_OUTPUT ) ? OUTPUT_LW48 : 0);
                    /* AVS bit-depth */
                    video_opt_config->avs.bit_depth = SendMessageA( GetDlgItem( hwnd, IDC_COMBOBOX_AVS_BITDEPTH ), CB_GETCURSEL, 0, 0 );
                    video_opt_config->avs.bit_depth = atoi( avs_bit_depth_list[ video_opt_config->avs.bit_depth ] );
                    /* audio_delay */
                    lwinput_opt_config.audio_delay = get_int_from_dlg( hwnd, IDC_EDIT_AUDIO_DELAY );
                    /* channel_layout */
                    {
                        const int buf_size = 512;
                        char *utf8_buf = NULL;
                        wchar_t edit_buf[buf_size];
                        GetDlgItemTextW( hwnd, IDC_EDIT_CHANNEL_LAYOUT, edit_buf, buf_size );
                        audio_opt_config->channel_layout = 0;
                        if ( lw_string_from_wchar( CP_UTF8, edit_buf, &utf8_buf ) ) {
                            AVChannelLayout ch_layout;
                            if( !av_channel_layout_from_string( &ch_layout, utf8_buf ) )
                            {
                                audio_opt_config->channel_layout = ch_layout.u.mask;
                                av_channel_layout_uninit( &ch_layout );
                            }
                            lw_freep( &utf8_buf );
                        }
                    }
                    /* sample_rate */
                    audio_opt_config->sample_rate = get_int_from_dlg_with_min( hwnd, IDC_EDIT_SAMPLE_RATE, 0 );
                    /* mix_level */
                    // changed on WM_HSCROLL
                    /* readers */
                    lwinput_opt_config.reader_disabled[0] = !get_check_state( hwnd, IDC_CHECK_LIBAVSMASH_INPUT );
                    lwinput_opt_config.reader_disabled[1] = !get_check_state( hwnd, IDC_CHECK_AVS_INPUT        );
                    lwinput_opt_config.reader_disabled[2] = !get_check_state( hwnd, IDC_CHECK_VPY_INPUT        );
                    lwinput_opt_config.reader_disabled[3] = !get_check_state( hwnd, IDC_CHECK_LIBAV_INPUT      );
                    /* preferred decoders */
                    {
                        const int buf_size = 512;
                        char *utf8_buf = NULL;
                        wchar_t edit_buf[buf_size];
                        GetDlgItemTextW( hwnd, IDC_EDIT_PREFERRED_DECODERS, edit_buf, buf_size );
                        if ( lw_string_from_wchar( CP_UTF8, edit_buf, &utf8_buf ) ) {
                            set_preferred_decoder_names_on_buf( reader_opt_config, utf8_buf );
                            lw_freep( &utf8_buf );
                        } else {
                            clean_preferred_decoder_names( reader_opt_config );
                        }
                    }
                    /* use cache dir */
                    reader_opt_config->use_cache_dir = get_check_state( hwnd, IDC_CHECK_USE_CACHE_DIR );
                    /* cache dir path */
                    {
                        const int buf_size = _MAX_PATH * 2;
                        char *utf8_buf = NULL;
                        wchar_t edit_buf[buf_size];
                        GetDlgItemTextW( hwnd, IDC_EDIT_CACHE_DIR_PATH, edit_buf, buf_size );
                        if ( lw_string_from_wchar( CP_UTF8, edit_buf, &utf8_buf ) ) {
                            set_cache_dir( reader_opt_config, utf8_buf );
                            lw_freep( &utf8_buf );
                        } else {
                            set_cache_dir( reader_opt_config, "" );
                        }
                    }
                    /* delete old cache */
                    lwinput_opt_config.delete_old_cache = get_check_state( hwnd, IDC_CHECK_DELETE_OLD_CACHE );
                    /* delete old cache days */
                    lwinput_opt_config.delete_old_cache_days = get_int_from_dlg_with_min( hwnd, IDC_EDIT_DELETE_OLD_CACHE_DAYS, 2 );
                    /* wide dialog */
                    lwinput_opt_config.wide_dialog = get_check_state( hwnd, IDC_CHECK_WIDE_DIALOG );

                    save_settings( &lwinput_opt_config );

                    EndDialog( hwnd, IDOK );
                    
                    MESSAGE_BOX_DESKTOP( MB_OK, "Please relaunch AviUtl ExEdit2 for updating settings!" );
                    
                    return TRUE;
                }
                default :
                    return FALSE;
            }
        case WM_CLOSE :
            EndDialog( hwnd, IDOK );
            return TRUE;
        default :
            return FALSE;
    }
}

bool func_config( HWND hwnd, HINSTANCE dll_hinst )
{
    const wchar_t* template = L"LWINPUT_CONFIG";
    /* Get Display Height ( Scaled ) */
    int height = GetSystemMetrics( SM_CYSCREEN );
    /* Get Dialog Height */
    HRSRC hresource = FindResourceW( dll_hinst, template, RT_DIALOG );
    HGLOBAL htemplate = LoadResource( dll_hinst, hresource );
    DLGTEMPLATE* ptemplate = (DLGTEMPLATE*)LockResource( htemplate );
    LONG dlg_baseunits = GetDialogBaseUnits();
    int baseunit_y = HIWORD( dlg_baseunits );
    int dlg_height = MulDiv( ptemplate->cy, baseunit_y, 8 );
    UnlockResource(htemplate);
    FreeResource(htemplate);

    if ( dlg_height > height || lwinput_opt.wide_dialog )
        template = L"LWINPUT_CONFIG_WIDE";

    DialogBoxW( dll_hinst, template, hwnd, dialog_proc );
    return TRUE;
}

BOOL WINAPI DllMain( HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved )
{
    switch( fdwReason ) 
    { 
        case DLL_PROCESS_ATTACH:
            hModuleDLL = hinstDLL;
            break;
        case DLL_PROCESS_DETACH:
            if (lpReserved != NULL) break;
            func_exit();
            break;
    }
    return TRUE;
}
