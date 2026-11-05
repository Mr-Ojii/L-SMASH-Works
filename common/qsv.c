/*****************************************************************************
 * qsv.c / qsv.cpp
 *****************************************************************************
 * Copyright (C) 2015 L-SMASH Works project
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

#include "cpp_compat.h"

#include <string.h>

#ifdef __cplusplus
extern "C"
{
#endif  /* __cplusplus */
#include <libavcodec/avcodec.h>
#include <libavutil/mem.h>
#ifdef __cplusplus
}
#endif  /* __cplusplus */

#include "decode.h"

int is_qsv_decoder
(
    const AVCodec *codec
)
{
    if( codec )
    {
        const enum AVPixelFormat *pix_fmts;
        int ret = avcodec_get_supported_config( NULL, codec, AV_CODEC_CONFIG_PIX_FORMAT, 0, ( const void ** )&pix_fmts, NULL );
        if ( ret >= 0 && pix_fmts )
            for( const enum AVPixelFormat *pix_fmt = pix_fmts; *pix_fmt != AV_PIX_FMT_NONE; pix_fmt++ )
                if( *pix_fmt == AV_PIX_FMT_QSV )
                    return 1;
    }
    return 0;
}

/* Workarounds for Intel QuickSync Video Decoder through libavcodec */
int do_qsv_decoder_workaround
(
    AVCodecContext *ctx
)
{
    ctx->has_b_frames = 16; /* the maximum decoder latency for AVC and HEVC frame */
    if( ctx->codec_id != AV_CODEC_ID_H264 )
        return 0;
    if( !ctx->extradata || ctx->extradata_size <= 0 )
        return 0;
    /* Feed an initializer packet to the decoder since libavcodec does not append parameter sets to access units
     * containing no IDR NAL units. Here we append parameter sets before the fake IDR NAL unit. Without this,
     * MFXVideoDECODE_DecodeHeader will return MFX_ERR_MORE_DATA.
     *
     * The h264_qsv decoder applies the h264_mp4toannexb bitstream filter to input packets by itself.
     * Therefore, feed the packet as-is if extradata is in AVCC format; the internal bitstream filter converts
     * it into Annex B and prepends the parameter sets to this IDR NAL unit. Converting the packet manually
     * here would make the internal bitstream filter parse Annex B data as AVCC and fail. */
    static const uint8_t fake_idr[] = { 0x00, 0x00, 0x00, 0x01, 0x65 }; /* start code + IDR NAL unit header */
    int ret = -1;
    AVPacket initializer = { 0 };
    if( ctx->extradata[0] == 1 )
    {
        /* AVCC format. Construct a packet containing only the fake IDR NAL unit.
         * The internal h264_mp4toannexb bitstream filter prepends SPS/PPS from extradata to it. */
        if( ctx->extradata_size < 5 )
            return 0;                                       /* Invalid AVCDecoderConfigurationRecord. */
        int length_size = (ctx->extradata[4] & 0x03) + 1;   /* Length in bytes of NAL unit length fields */
        int idr_size    = (int)sizeof(fake_idr) - 4;        /* Skip the start code */
        if( (ret = av_new_packet( &initializer, length_size + idr_size )) < 0 )
            return ret;
        memset( initializer.data, 0, length_size );
        initializer.data[length_size - 1] = (uint8_t)idr_size;
        memcpy( initializer.data + length_size, fake_idr + 4, idr_size );
    }
    else
    {
        /* Annex B format. The internal bitstream filter passes packets through,
         * so prepend the parameter sets in extradata to the fake IDR NAL unit ourselves. */
        if( (ret = av_new_packet( &initializer, ctx->extradata_size + sizeof(fake_idr) )) < 0 )
            return ret;
        memcpy( initializer.data, ctx->extradata, ctx->extradata_size );
        memcpy( initializer.data + ctx->extradata_size, fake_idr, sizeof(fake_idr) );
    }
    /* Initialize the decoder. */
    AVFrame *picture = av_frame_alloc();
    if( !picture )
    {
        ret = -1;
        goto fail;
    }
    int got_picture;    /* unused */
    ret = decode_video_packet( ctx, picture, &got_picture, &initializer );
    av_frame_free( &picture );
fail:
    av_packet_unref( &initializer );
    return ret;
}
