/************************************************************************************************************
 * Copyright (c) 2017, Dolby Laboratories Inc.
 * All rights reserved.

 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:

 * 1. Redistributions of source code must retain the above copyright notice, this list of conditions
 *    and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions
 *    and the following disclaimer in the documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or
 *    promote products derived from this software without specific prior written permission.

 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 ************************************************************************************************************/
/** @defgroup mp4d_buffer
 *  @brief Buffer read access
 * @{
 */
#ifndef MP4D_BUFFER_H
#define MP4D_BUFFER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mp4d_types.h"

#define mp4d_is_buffer_error(p) ((p)->size==(uint64_t)-1)
#define mp4d_bytes_left(p)      ((p)->size!=(uint64_t)-1 && (p)->size>0)

/**
 * @brief buffer type
 */
typedef struct mp4d_buffer_t_ {
    const unsigned char *p_data;      /**< Pointer to current position */
    uint64_t             size;        /**< Remaining size in buffer */
    const unsigned char *p_begin;     /**< Pointer to beginning of buffer */
} mp4d_buffer_t;

/** @brief read an unsigned 8-bit integer */
uint8_t  mp4d_read_u8(mp4d_buffer_t * p);

/** @brief read an unsigned 8-bit integer */
uint16_t mp4d_read_u16(mp4d_buffer_t * p);

/** @brief read an unsigned 8-bit integer */
uint32_t mp4d_read_u24(mp4d_buffer_t * p);

/** @brief read an unsigned 8-bit integer */
uint32_t mp4d_read_u32(mp4d_buffer_t * p);

/** @brief read an unsigned 8-bit integer */
uint64_t mp4d_read_u64(mp4d_buffer_t * p);

/** @brief seek forward from the current position */
void mp4d_skip_bytes(mp4d_buffer_t * p, uint64_t size);

/** @brief seek 
 */
void mp4d_seek(mp4d_buffer_t *p, 
               uint64_t offset    /**< relative to beginning of buffer */
    );

/** @brief read a 4cc */
void mp4d_read_fourcc(mp4d_buffer_t * p, 
                      mp4d_fourcc_t four_cc   /**< [out] */
    );

/** @brief read a number of bytes */
void 
mp4d_read
    (mp4d_buffer_t * p
     ,unsigned char * p_val    /**< output buffer */
     ,uint32_t size            /**< number of bytes to read */
        );

#ifdef __cplusplus
}
#endif

#endif
/** @} */
