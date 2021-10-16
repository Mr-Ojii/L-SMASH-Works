/*****************************************************************************
 * pipe.h
 *****************************************************************************
 * Copyright (C) 2013-2021 L-SMASH Works project
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

#include <stdint.h>
#include <windows.h>

typedef enum
{
    CALL_OPEN = 1,
    CALL_CLOSE,
    CALL_INFO_GET,
    CALL_READ_VIDEO,
    CALL_READ_AUDIO,
    CALL_IS_KEY_FRAME,

    CALL_EXIT,
} call_func;

typedef struct
{
    call_func call_func;
    uint32_t data_size;
} pipe_header;

typedef struct
{
	int flag;
	int	rate,scale;
	int	n;
	int format_size;
	int	audio_n;
	int audio_format_size;
	DWORD handler;
	int reserve[7];
} pipe_input_info;

static uint32_t pipe_write(HANDLE pipe_handle, const BYTE* data, uint32_t length) {
    if( !pipe_handle )
        return 0;
    DWORD writen_size = 0;
    WriteFile(pipe_handle, data, length, &writen_size, NULL);
    FlushFileBuffers(pipe_handle);
    return writen_size;
}

static uint32_t pipe_read(HANDLE pipe_handle, BYTE* data, uint32_t length) {
    if( !pipe_handle )
        return 0;

    BYTE* write_data_pos = data;
    DWORD rest_read_bytes = length;
    uint32_t total_read_bytes = 0;

    while (rest_read_bytes > 0) {
        DWORD ready_bytes = 0;
        BOOL success = ReadFile(pipe_handle, write_data_pos, rest_read_bytes, &ready_bytes, NULL);
        if ((!success || ready_bytes == 0)) {
            if (GetLastError() == 0) {
                Sleep(10);
                continue;
            }
            return total_read_bytes;
        } else {
            rest_read_bytes -= ready_bytes;
            total_read_bytes = length - rest_read_bytes;
            write_data_pos += ready_bytes;
        }
    }

    return total_read_bytes;
}
