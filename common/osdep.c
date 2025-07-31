/*****************************************************************************
 * osdep.c / osdep.cpp
 *****************************************************************************
 * Copyright (C) 2014 L-SMASH project
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

/* This file is available under an ISC license. */

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS

#include "osdep.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

int lw_string_to_wchar( int cp, const char *from, wchar_t **to )
{
    int nc = MultiByteToWideChar( cp, MB_ERR_INVALID_CHARS, from, -1, 0, 0 );
    if( nc == 0 )
        return 0;
    *to = (wchar_t *)lw_malloc_zero( nc * sizeof(wchar_t) );
    if( !*to )
        return 0;
    MultiByteToWideChar( cp, 0, from, -1, *to, nc );
    return nc;
}

int lw_string_from_wchar( int cp, const wchar_t *from, char **to )
{
    int nc = WideCharToMultiByte( cp, 0, from, -1, 0, 0, 0, 0 );
    if( nc == 0 )
        return 0;
    *to = (char *)lw_malloc_zero( nc * sizeof(char) );
    if( !*to )
        return 0;
    WideCharToMultiByte( cp, 0, from, -1, *to, nc, 0, 0 );
    return nc;
}

int lw_convert_mb_string( int cp_from, int cp_to, const char* from, char** to )
{
    wchar_t* w;
    int ncw = lw_string_to_wchar( cp_from, from, &w );
    if( ncw == 0 )
        return 0;
    int nc = lw_string_from_wchar( cp_to, w, to );
    lw_free( w );
    if( nc == 0 )
        return 0;
    return nc;
}

FILE *lw_win32_fopen( const char *name, const char *mode )
{
    wchar_t *wname = 0, *wmode = 0;
    FILE *fp = 0;
    if( lw_string_to_wchar( CP_UTF8, name, &wname ) &&
        lw_string_to_wchar( CP_UTF8, mode, &wmode ) )
        fp = _wfopen( wname, wmode );
    if( !fp )
        fp = fopen( name, mode );
    lw_freep( &wname );
    lw_freep( &wmode );
    return fp;
}

char *lw_realpath( const char *path, char *resolved )
{
    wchar_t *wpath = 0, *wresolved = 0;
    char *ret = 0;
    if( lw_string_to_wchar( CP_UTF8, path, &wpath ) ) {
        wresolved = _wfullpath(0, wpath, _MAX_PATH);
        lw_string_from_wchar( CP_UTF8, wresolved, &ret);
    } else {
        ret = _fullpath(0, path, _MAX_PATH);
    }
    lw_freep( &wpath );
    lw_freep( &wresolved );
    if (resolved) {
        strcpy(resolved, ret);
        free(ret);
        return resolved;
    }
    return ret;
}

int lw_GetModuleFileNameW( void* module, wchar_t **data )
{
    *data = NULL;

    DWORD w_size = _MAX_PATH, result;
    wchar_t* w_data = NULL;
    while( 1 )
    {
        w_data = lw_malloc_zero( w_size * sizeof(wchar_t) );
        if( !w_data )
            return 0;

        if( !( result = GetModuleFileNameW( (HMODULE)module, w_data, w_size ) ) ) {
            goto failed_W;
        }

        DWORD err = GetLastError();
        if ( err == ERROR_SUCCESS ) {
            break;
        } else if( err == ERROR_INSUFFICIENT_BUFFER ) {
            lw_free( w_data );
            w_size *= 2;
            continue;
        } else {
            goto failed_W;
        }
    }
    *data = w_data;
    return result;

failed_W:
    lw_free( w_data );
    return 0;
}

int lw_GetModuleFileNameUTF8( void* module, char **data )
{
    *data = NULL;

    wchar_t* w_data = NULL;
    if( !lw_GetModuleFileNameW( module, &w_data ) )
        return 0;
    
    int size = lw_string_from_wchar( CP_UTF8, w_data, data );
    lw_free( w_data );
    return size;
}

#endif
