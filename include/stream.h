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
/**
 * @defgroup stream
 *
 * @brief Provides an API for pulling samples from a stream of fragments,
 * by wrapping a fragment_stream and an mp4d_trackreader_t object
 *
 * @{
 */
#ifndef STREAM_H
#define STREAM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mp4d_trackreader.h"
#include "fragment_stream.h"

#include <stdlib.h>

typedef struct
{
    uint32_t track_ID;  /* may be initialized to zero, in which case the real track ID is available
                           after the first call to next_sample
                           */
    char *name;   /* stream name (used in DASH/PIFF when track_ID is zero) */

    uint32_t movie_time_scale;
    uint32_t media_time_scale;

    mp4d_trackreader_ptr_t p_tr;
    void *p_static_mem;
    void *p_dynamic_mem;

    fragment_reader_t fragments;  /* handle to the current moov/moof used by the trackreader */
    int have_fragment;   /* bool, false until loading the first fragment */

    /* sample queue (containing zero or one sample) */
    int have_sample;     /* true iff sample in queue */

    mp4d_sampleref_t sample;
    uint64_t *subsample_pos;  /* Arrays of length sample.num_subsamples */
    uint32_t *subsample_size;
    size_t size_subsample;    /* allocated size */
    uint32_t subtitle_track_flag; /* a flag for subtitle track: it will be 1; audio/video track should not be 1*/
    uint32_t stss_count;
    unsigned char *stss_buf;
} stream_t;

/** @brief initialize a stream from the given source
 *
 * Fills a pre-allocated struct.
 *
 * @return error
 */
int
stream_init(stream_t *p_s,
            fragment_reader_t,        /**< Transfer of ownership, must not be freed by caller */
            uint32_t track_ID,
            const char *stream_name,
            uint32_t movie_time_scale,
            uint32_t media_time_scale);

/** @brief desctructor
 *
 * Does not deallocate struct
 *
 */
void
stream_deinit(stream_t *p_s);

/** @brief Seek to the given presentation_time in a stream.
 */
int
stream_seek(stream_t *p_s,
            uint64_t seek_time,
            uint64_t *out_time
    );

/** @brief attempts to fill the sample queue for one track
 *
 *  Sets have_sample to true if a next sample could be read.
 *  Otherwise have_sample is false and the function succeeds.
 *
 *  @return error
 */
int
stream_next_sample(stream_t *p_s,
                   int single_fragment   /**< If true, do not load the next fragment if out of samples */
                   );


/** @brief attempts to fill the sample queue for subtitle track
 *
 *  Sets have_sample to true if a next sample could be read.
 *  Otherwise have_sample is false and the function succeeds.
 *
 *  @return error
 */
int
subtitle_next_sample(stream_t *p_s,
                   int single_fragment   /**< If true, do not load the next fragment if out of samples */
                   );

/**
 * @brief assign the default 'tenc' data to the stream
 *
 * @return Error code:
 *     OK (0) for success.
 */
int
stream_set_tenc
(
    stream_t *p_s,
    uint32_t default_algorithmID,
    uint8_t default_iv_size,
    uint8_t* default_kid
);

/**
 * @brief fetch the latest 'tenc' data to be used during decryption
 *
 * @return Error code:
 *     OK (0) for success.
 */
int
stream_get_cur_tenc
(
    stream_t *p_s,
    uint32_t* algorithmID,
    uint8_t* iv_size,
    uint8_t* kid
);

#ifdef __cplusplus
}
#endif

#endif
/** @} */
