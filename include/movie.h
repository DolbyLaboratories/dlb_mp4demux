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
/** @defgroup mp4_info
 *
 * @brief Abstract API for getting information from presentation
 *
 * @{
 */
#ifndef MOVIE_H
#define MOVIE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "fragment_stream.h"

typedef struct movie_t_ * movie_t;

struct movie_t_
{
    /** @brief destructor */
    void (*destroy)(movie_t);

    /** @brief get movie information
     *
     * @return error
     */
    int (*get_movie_info)(movie_t,
                          mp4d_movie_info_t *   /**< [out] */
                          );

    /** @brief get stream information
     *
     * @return error
     */
    int (*get_stream_info)(movie_t,
                           uint32_t stream_num,               /**< counting from zero */
                           uint32_t bit_rate,                 /**< selected bitrate */
                           mp4d_stream_info_t *p_stream_info, /**< [out] */
                           char **stream_name                 /**< [out] PIFF/DASH: instead of track_ID, must be freed by caller */
        );

    /** @brief get sample entry
     *
     * @return error
     */
    int (*get_sampleentry)(movie_t,
                           uint32_t stream_num,               /**< counting from zero */
                           uint32_t bit_rate,                 /**< selected bitrate */
                           uint32_t sample_description_index, /**< counting from one */
                           mp4d_sampleentry_t *p_sampleentry  /**< [out] */
                           );

    /** @brief get available bit rates
     *
     * @return error 1: unexpected
     *               2: no more bitrates
     */
    int (*get_bitrate)(movie_t,
                       uint32_t stream_num,         /** counting from zero */
                       uint32_t indx,               /** counting from zero. If index is >= number of available bitrates, an error is returned */
                       uint32_t *p_bitrate          /**< [out] in bits per second */
                       );

    /** @brief Fragment_stream factory
     *
     * Creates a handle which allows to access the fragments of the given stream
     *
     * @return error
     */
    int (*fragment_stream_new)(movie_t,
                               uint32_t stream_num,       /**< counting from zero. PIFF: ignored. DASH: AdaptationSet index */
                               const char *stream_name,   /**< PIFF only: Alternative to stream_num */
                               uint32_t bitrate,          /**< One of the bitrates returned from get_bitrate() */
                               fragment_reader_t *        /**< [out] must be deallocated by the caller */
                               );
};

#ifdef __cplusplus
}
#endif

#endif
/* @} */
