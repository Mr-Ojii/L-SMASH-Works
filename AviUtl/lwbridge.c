/*****************************************************************************
 * lwbridge.c
 *****************************************************************************
 * Copyright (C) 2013-2022 L-SMASH Works project
 *
 * Authors: Mr-Ojii <okaschan@gmail.com>
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

#include <time.h>
#include <string.h>
#include <stdint.h>

#include "config.h"
#include "pipe.h"
#include "lwbridge.h"

#define MPEG4_FILE_EXT      "*.mp4;*.m4v;*.m4a;*.mov;*.qt;*.3gp;*.3g2;*.f4v;*.ismv;*.isma"
#define INDEX_FILE_EXT      "*.lwi"
#define ANY_FILE_EXT        "*.*"

static HANDLE pipe_handle = NULL;
static char pipe_name[56];
static char random_string[33];
static char plugin_dir[_MAX_PATH * 2];
static char exe_path[_MAX_PATH * 2];
static const char *exe_path_list[] = { "lwinput.exe", "plugins\\lwinput.exe" };
static HMODULE hModuleDLL = NULL;
static HANDLE mutex = NULL;


INPUT_PLUGIN_TABLE input_plugin_table =
{
    INPUT_PLUGIN_FLAG_VIDEO | INPUT_PLUGIN_FLAG_AUDIO,              /* INPUT_PLUGIN_FLAG_VIDEO : support images
                                                                     * INPUT_PLUGIN_FLAG_AUDIO : support audio */
    "L-SMASH Works File Reader (Bridge)",                           /* Name of plugin */
    "MPEG-4 File (" MPEG4_FILE_EXT ")\0" MPEG4_FILE_EXT "\0"        /* Filter for Input file */
    "LW-Libav Index File (" INDEX_FILE_EXT ")\0" INDEX_FILE_EXT "\0"
    "Any File (" ANY_FILE_EXT ")\0" ANY_FILE_EXT "\0",
    "L-SMASH Works File Reader r" LSMASHWORKS_REV "\0",             /* Information of plugin */
    func_init,                                                      /* Pointer to function called when opening DLL (If NULL, won't be called.) */
    func_exit,                                                      /* Pointer to function called when closing DLL (If NULL, won't be called.) */
    func_open,                                                      /* Pointer to function to open input file */
    func_close,                                                     /* Pointer to function to close input file */
    func_info_get,                                                  /* Pointer to function to get information of input file */
    func_read_video,                                                /* Pointer to function to read image data */
    func_read_audio,                                                /* Pointer to function to read audio data */
    func_is_keyframe,                                               /* Pointer to function to check if it is a keyframe or not (If NULL, all is keyframe.) */
    func_config,                                                    /* Pointer to function called when configuration dialog is required */
};

EXTERN_C INPUT_PLUGIN_TABLE __declspec(dllexport) * __stdcall GetInputPluginTable( void )
{
    char path[_MAX_PATH * 2], drive[_MAX_DRIVE], dir[_MAX_DIR * 2], fname[_MAX_FNAME * 2], ext[_MAX_EXT * 2];
    if( GetModuleFileName( hModuleDLL, path, MAX_PATH * 2) ) {
        _splitpath(path, drive, dir, fname, ext);
        strcpy(plugin_dir, drive);
        strcat(plugin_dir, dir);
    }
    return &input_plugin_table;
}

BOOL func_init( void )
{
    BOOL exe_search_success = FALSE;
    for( int i = 0; i < 2; i++ )
    {
        strcpy(exe_path, plugin_dir);
        strcat(exe_path, exe_path_list[i]);
        FILE* target = fopen( exe_path, "rb" );
        if( target )
        {
            fclose(target);
            exe_search_success = TRUE;
            break;
        }
    }

    if(!exe_search_success) {
        MessageBox( HWND_DESKTOP, "'lwinput.exe' not found.\n'L-SMASH Works File Reader' will be disabled.", "lwbridge", MB_ICONERROR | MB_OK );
        return FALSE;
    }
    
    mutex = CreateMutex( NULL, FALSE, NULL );
    if( !mutex ) {
        MessageBox( HWND_DESKTOP, "Failed to create a mutex object.\n'L-SMASH Works File Reader' will be disabled.", "lwbridge", MB_ICONERROR | MB_OK );
        return FALSE;
    }
    
    srand(time(NULL));
    memset(pipe_name, 0, sizeof(pipe_name));
    memset(random_string, 0, sizeof(random_string));
    strcpy(pipe_name, "\\\\.\\pipe\\L-SMASH-Works\\");
    const char* tmpstr = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    for( int i = 0; i < 32; i++) {
        random_string[i] = tmpstr[rand() % 62];
    }
    strcat(pipe_name, random_string);
    pipe_handle = CreateNamedPipe(pipe_name, 
        PIPE_ACCESS_DUPLEX,       // read/write access 
        PIPE_TYPE_BYTE |       // byte type pipe 
        PIPE_READMODE_BYTE |   // byte-read mode 
        PIPE_WAIT,                // blocking mode
        1, 4096, 4096, 0, NULL);
    if ( pipe_handle == INVALID_HANDLE_VALUE || pipe_handle == NULL ) {
        pipe_handle = NULL;
        MessageBox( HWND_DESKTOP, "Failed to create a named pipe.\n'L-SMASH Works File Reader' will be disabled.", "lwbridge", MB_ICONERROR | MB_OK );
        return FALSE;
    }
    ShellExecute(NULL, NULL, exe_path, random_string, NULL, SW_HIDE);
    if ( !ConnectNamedPipe(pipe_handle, NULL) ) {
        return FALSE;
    }
    return TRUE;
}

BOOL func_exit( void )
{
    if( pipe_handle ) {
        pipe_header header;
        header.call_func = CALL_EXIT;
        header.data_size = 0;
        pipe_write(pipe_handle, (BYTE*)(&header), sizeof(header));
        DisconnectNamedPipe(pipe_handle);
        CloseHandle(pipe_handle);
        pipe_handle = NULL;
    }
    if( mutex ) {
        ReleaseMutex(mutex);
        CloseHandle( mutex );
        mutex = NULL;
    }
    return TRUE;
}

INPUT_HANDLE func_open( LPSTR file )
{
    if( !pipe_handle || !mutex )
        return NULL;
        
    WaitForSingleObject(mutex, INFINITE);

    pipe_header header;
    header.call_func = CALL_OPEN;
    header.data_size = (strlen(file) + 1) * sizeof(char);

    pipe_write(pipe_handle, (BYTE*)(&header), sizeof(header));
    pipe_write(pipe_handle, (BYTE*)file, header.data_size);
    
    pipe_header received_header;
    pipe_read(pipe_handle, (BYTE*)(&received_header), sizeof(received_header));
    input_handle_cache* hp = lw_malloc_zero(sizeof(input_handle_cache));
    if(!hp)
        return NULL;
    hp->format = NULL;
    hp->audio_format = NULL;
    hp->handle = lw_malloc_zero(received_header.data_size);
    if(!hp->handle) {
        lw_free(hp);
        return NULL;
    }
    hp->length = received_header.data_size;
    pipe_read(pipe_handle, hp->handle, received_header.data_size);

    BOOL is_NULL = TRUE;
    for(int i = 0; i < received_header.data_size; i++) {
        if( hp->handle[i] != 0 ) {
            is_NULL = FALSE;
            break;
        }
    }
    if( is_NULL ) {
        lw_free(hp->handle);
        hp->handle = NULL;
        lw_free(hp);
        hp = NULL;
    }

    ReleaseMutex(mutex);

    return (INPUT_HANDLE)hp;
}

BOOL func_close( INPUT_HANDLE ih )
{
    if( !pipe_handle || !mutex )
        return FALSE;

    WaitForSingleObject(mutex, INFINITE);
    input_handle_cache* hp = ih;

    pipe_header header;
    header.call_func = CALL_CLOSE;
    header.data_size = hp->length;
    pipe_write(pipe_handle, (BYTE*)(&header), sizeof(header));
    pipe_write(pipe_handle,  hp->handle, hp->length);

    pipe_header received_header;
    pipe_read(pipe_handle, (BYTE*)(&received_header), sizeof(received_header));
    BOOL ret;
    pipe_read(pipe_handle, (BYTE*)(&ret), received_header.data_size);

    lw_free(hp->audio_format);
    lw_free(hp->format);
    lw_free(hp->handle);
    lw_free(hp);

    ReleaseMutex(mutex);

    return ret;
}

BOOL func_info_get( INPUT_HANDLE ih, INPUT_INFO *iip )
{
    if( !pipe_handle || !mutex ) 
        return FALSE;

    WaitForSingleObject(mutex, INFINITE);
    input_handle_cache* hp = ih;

    pipe_header header;
    header.call_func = CALL_INFO_GET;
    header.data_size = hp->length;

    pipe_write(pipe_handle, (BYTE*)(&header), sizeof(header));
    pipe_write(pipe_handle, hp->handle, hp->length);

    pipe_header received_header;
    pipe_read(pipe_handle, (BYTE*)(&received_header), sizeof(received_header));
    if (received_header.data_size == 0)
        return FALSE;
    
    BYTE* data = lw_malloc_zero(received_header.data_size);
    if(!data)
        return FALSE;
    pipe_read(pipe_handle, data, received_header.data_size);

    BOOL ret;
    memcpy(&ret, data, sizeof(BOOL));
    if(ret) {
        pipe_input_info pipe_info;
        memcpy(&pipe_info, data + sizeof(BOOL), sizeof(pipe_input_info));
        hp->audio_format = lw_malloc_zero(pipe_info.audio_format_size);
        if(!hp->audio_format) {
            lw_free(data);
            return FALSE;
        }
        hp->format = lw_malloc_zero(pipe_info.format_size);
        if(!hp->format) {
            lw_free(hp->audio_format);
            lw_free(data);
            return FALSE;
        }
        memcpy(hp->audio_format, data + sizeof(BOOL) + sizeof(pipe_input_info), pipe_info.audio_format_size);
        memcpy(hp->format, data + sizeof(BOOL) + sizeof(pipe_input_info) + pipe_info.audio_format_size, pipe_info.format_size);
        iip->format = hp->format;
        iip->audio_format = hp->audio_format;
        iip->audio_format_size = pipe_info.audio_format_size;
        iip->audio_n = pipe_info.audio_n;
        iip->flag = pipe_info.flag;
        iip->format_size = pipe_info.format_size;
        iip->handler = pipe_info.handler;
        iip->n = pipe_info.n;
        iip->rate = pipe_info.rate;
        for(int i = 0; i < 7; i++)
            iip->reserve[i] = pipe_info.reserve[i];
        iip->scale = pipe_info.scale;
    }
    lw_free(data);

    ReleaseMutex(mutex);

    return ret;
}

int func_read_video( INPUT_HANDLE ih, int sample_number, void *buf )
{
    if( !pipe_handle || !mutex )
        return 0;

    WaitForSingleObject(mutex, INFINITE);
    input_handle_cache* hp = ih;
    
    pipe_header header;
    header.call_func = CALL_READ_VIDEO;
    header.data_size = sizeof(int) + hp->length;

    pipe_write(pipe_handle, (BYTE*)(&header), sizeof(header));
    pipe_write(pipe_handle, (BYTE*)(&sample_number), sizeof(sample_number));
    pipe_write(pipe_handle, hp->handle, hp->length);

    pipe_header received_header;
    pipe_read(pipe_handle, (BYTE*)(&received_header), sizeof(received_header));
    pipe_read(pipe_handle, buf, received_header.data_size);

    ReleaseMutex(mutex);

    return received_header.data_size;
}

int func_read_audio( INPUT_HANDLE ih, int start, int length, void *buf )
{
    if( !pipe_handle || !mutex )
        return 0;

    WaitForSingleObject(mutex, INFINITE);
    input_handle_cache* hp = ih;
    
    pipe_header header;
    header.call_func = CALL_READ_AUDIO;
    header.data_size = sizeof(int) * 2 + hp->length;
    pipe_write(pipe_handle, (BYTE*)(&header), sizeof(header));
    pipe_write(pipe_handle, (BYTE*)(&start), sizeof(start));
    pipe_write(pipe_handle, (BYTE*)(&length), sizeof(length));
    pipe_write(pipe_handle, hp->handle, hp->length);

    pipe_header received_header;
    pipe_read(pipe_handle, (BYTE*)(&received_header), sizeof(received_header));
    int read_size;
    pipe_read(pipe_handle, (BYTE*)(&read_size), sizeof(int));
    pipe_read(pipe_handle, (BYTE*)buf, received_header.data_size - sizeof(int));
    
    ReleaseMutex(mutex);

    return read_size;
}

BOOL func_is_keyframe( INPUT_HANDLE ih, int sample_number )
{
    if( !pipe_handle || !mutex ) 
        return TRUE;

    WaitForSingleObject(mutex, INFINITE);
    input_handle_cache* hp = ih;

    pipe_header header;
    header.call_func = CALL_IS_KEY_FRAME;
    header.data_size = hp->length + sizeof(sample_number);

    pipe_write(pipe_handle, (BYTE*)(&header), sizeof(header));
    pipe_write(pipe_handle, (BYTE*)(&sample_number), sizeof(sample_number));
    pipe_write(pipe_handle,  hp->handle, hp->length);

    pipe_header received_header;
    pipe_read(pipe_handle, (BYTE*)(&received_header), sizeof(received_header));
    BOOL ret;
    pipe_read(pipe_handle, (BYTE*)(&ret), received_header.data_size);

    ReleaseMutex(mutex);

    return ret;
}

BOOL func_config( HWND hwnd, HINSTANCE dll_hinst )
{
    ShellExecute( NULL, NULL, exe_path, "-config", NULL, SW_HIDE );
    return TRUE;
}

BOOL WINAPI DllMain( HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved )
{
    switch( fdwReason ) 
    { 
        case DLL_PROCESS_ATTACH:
            hModuleDLL = hinstDLL;
            break;
    }
    return TRUE;
}
