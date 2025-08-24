/*****************************************************************************
 * resource.h
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

#ifndef IDC_STATIC
#define IDC_STATIC (-1)
#endif

/* Progress dialog */
#define IDD_PROGRESS_ABORTABLE          101
#define IDD_PROGRESS_UNABORTABLE        102
#define IDC_PERCENT_TEXT               1000
#define IDC_PROGRESS                   1001

/* Reader dialog */
#define IDC_CHECK_LIBAVSMASH_INPUT     1100
#define IDC_CHECK_AVS_INPUT            1101
#define IDC_CHECK_VPY_INPUT            1102
#define IDC_CHECK_LIBAV_INPUT          1103
#define IDC_EDIT_THREADS               1110
#define IDC_SPIN_THREADS               1111
#define IDC_EDIT_FORWARD_THRESHOLD     1120
#define IDC_SPIN_FORWARD_THRESHOLD     1121
#define IDC_COMBOBOX_SEEK_MODE         1122
#define IDC_CHECK_APPLY_REPEAT_FLAG    1130
#define IDC_COMBOBOX_FIELD_DOMINANCE   1131
#define IDC_COMBOBOX_SCALER            1132
#define IDC_EDIT_AUDIO_DELAY           1140
#define IDC_EDIT_SAMPLE_RATE           1141
#define IDC_EDIT_CHANNEL_LAYOUT        1142
#define IDC_SLIDER_MIX_LEVEL_CENTER    1143
#define IDC_SLIDER_MIX_LEVEL_SURROUND  1144
#define IDC_SLIDER_MIX_LEVEL_LFE       1145
#define IDC_TEXT_MIX_LEVEL_CENTER      1146
#define IDC_TEXT_MIX_LEVEL_SURROUND    1147
#define IDC_TEXT_MIX_LEVEL_LFE         1148
#define IDC_CHECK_AV_SYNC              1149
#define IDC_CHECK_CREATE_INDEX_FILE    1150
#define IDC_CHECK_LW48_OUTPUT          1170
#define IDC_COMBOBOX_AVS_BITDEPTH      1171
#define IDC_EDIT_PREFERRED_DECODERS    1180
#define IDC_TEXT_LIBRARY_INFO          1190
#define IDC_CHECK_USE_CACHE_DIR        1200
#define IDC_EDIT_CACHE_DIR_PATH        1201
#define IDC_CHECK_DELETE_OLD_CACHE     1210
#define IDC_EDIT_DELETE_OLD_CACHE_DAYS 1211
#define IDC_CHECK_WIDE_DIALOG          1220

/* Muxer dialog */
#define IDD_MUXER_OPTIONS               103
#define IDC_BUTTON_OPTION_DEFAULT      1300
#define IDC_BUTTON_CHAPTER_BROWSE      1301
#define IDC_EDIT_CHAPTER_PATH          1302
#define IDC_CHECK_OPTIMIZE_PD          1303
