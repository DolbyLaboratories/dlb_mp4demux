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
/** @defgroup mp4d_trackreader
    @brief Track reader

    A track reader object is used to get samples for a certain track_ID. The
    same track reader instance is reused for different top-level boxes (moov, moof, ...)

    @{
 */
#ifndef MP4D_TRACKREADER_H
#define MP4D_TRACKREADER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mp4d_demux.h"
#include "mp4d_types.h"

/** @brief The maximum number of edits (number of entries in moov:trak:edts:elst)
 * that the trackreader guarantees to support. Can be redefined to a higher number.
 *
 * A runtime error, MP4D_E_UNSUPPRTED_FORMAT, will happen if reading an elst box
 * which does not fit into the reserved memory bloxk.
 *
 * The memory cost is 20 bytes per edit per trackreader instance. (This figure
 * applies to an elst box of version 1; an elst box of version 0 with more than
 * MP4D_MAX_EDITS entries may fit into the reserved memory.)
 *
 */
#ifndef MP4D_MAX_EDITS          /* allow to define from outside */
#define MP4D_MAX_EDITS 2
#endif

/** @brief Track reader handle
*/
typedef struct mp4d_trackreader_t_ *mp4d_trackreader_ptr_t;



/**
   @brief Get the track_ID of this trackreader

   @return MP4D_NO_ERROR, or MP4D_E_WRONG_ARGUMENT: NULL pointers
*/

int
mp4d_trackreader_get_track_ID
(
    mp4d_trackreader_ptr_t p_tr,
    uint32_t *track_ID          /**< [out] */
);

/**
   @brief Get the movie time scale and media time scale

   @return MP4D_NO_ERROR, or MP4D_E_WRONG_ARGUMENT: NULL pointers
*/
int
mp4d_trackreader_get_time_scale
(
    mp4d_trackreader_ptr_t p_tr,
    uint32_t *movie_time_scale,         /**< [out] */
    uint32_t *media_time_scale          /**< [out] */
    );

/**
   @brief Get the sync sample count and informations

   @return MP4D_NO_ERROR, or MP4D_E_WRONG_ARGUMENT: NULL pointers
*/
int
mp4d_trackreader_get_stss_count
(
    mp4d_trackreader_ptr_t p_tr,
    uint32_t *count,                              /**< [out] */
    unsigned char **stts_content         /**< [out] */
    );

/**
   @brief return the next sample in this track

   @return error code:
        OK (0) - sample found
        MP4D_E_NEXT_SEGMENT - no more samples in this segment
            The segment buffer can be released. If more segments are expected,
            mp4d_demuxer_parse() needs to be called first until a new segment
            is found in this track.
        MP4D_E_WRONG_ARGUMENT - NULL pointers or track reader object not initialized
        
*/
int
mp4d_trackreader_next_sample 
(
    mp4d_trackreader_ptr_t trackreader_ptr,
    mp4d_sampleref_t *sample_ptr_out
);

/** @brief Get subsample information

   This function must be called after mp4d_trackreader_next_sample and provides subsample
   information for the last sample returned by mp4d_trackreader_next_sample.
   
   This function can be called up to sample.num_subsamples times.

   @return MP4D_NO_ERROR:         OK
           MP4D_E_WRONG_ARGUMENT: NULL pointers
*/
int
mp4d_trackreader_next_subsample
(
    mp4d_trackreader_ptr_t p_tr,      /**< trackreader handle */
    const mp4d_sampleref_t *p_sample, /**< sample which was provided by mp4d_trackreader_next_sample */
    uint64_t *p_offset,               /**< [out] sample position (in bytes) relative to file beginning */
    uint32_t *p_size                  /**< [out] sample size in bytes */
);

/** @brief Seek to a sample inside the current fragment.

    After calling this function, the next call to mp4d_trackreader_next_sample() will
    return the latest sample presentation time stamp less than or equal to the requested time
    stamp, which is also a sync sample.

    If no such sample exists, either MP4D_E_NEXT_SEGMENT or MP4D_E_PREV_SEGMENT is returned.
    (If there are no sync samples inside the segment MP4D_E_PREV_SEGMENT is returned.)

    @return error code:
        OK (0) - sample found
        MP4D_E_NEXT_SEGMENT - requested sample is in a later segment
        MP4D_E_PREV_SEGMENT - requested sample is in an earlier segment
        MP4D_E_WRONG_ARGUMENT - NULL pointers or track reader object not initialized

     The caller must handle the case where a seek point is in-between segments (in which case
     MP4D_E_NEXT_SEGMENT is returned when attempting to seek in the preceeding segment and
     MP4D_E_PREV_SEGMENT is returned when attempting to seek in the succeeding segment).
*/
int
mp4d_trackreader_seek_to 
(
    mp4d_trackreader_ptr_t trackreader_ptr,
    uint64_t time_stamp_in,  /**< requested time stamp in movie time scale. It is in absolute time scale
                                related to the start of the movie */
    uint64_t *time_stamp_out /**< achieved time stamp in movie time scale, includes pre-roll */
);

/** @brief Initialize the track reader with a top-level box
    @return error code:
        OK (0)
        MP4D_E_TRACK_NOT_FOUND - requested track_id not in segment
        MP4D_E_WRONG_ARGUMENT - NULL pointers or track reader object not initialized
*/
int
mp4d_trackreader_init_segment
(
    mp4d_trackreader_ptr_t trackreader_ptr, 
    mp4d_demuxer_ptr_t demuxer_ptr,
    uint32_t track_ID,          /**< must be constant for the lifetime of the object */
    uint32_t movie_time_scale,
    uint32_t media_time_scale,
    const uint64_t *abs_time_offs   /**< [in] If non-NULL, time stamp of this fragment in media time scale.
                                          Set to NULL if the first sample in the fragment directly follows the
                                          previous sample returned by mp4d_trackreader_next_sample() */
);

/** @brief Inform track reader about the input data (file) type
 *
 * This function should be called before init_segment(), next_sample() and seek_to(). If this function is
 * not called, the default assumed data type is the ISO Base Media FF.
 *
 * Specifically, this function can be used specify that the input data conforms
 * to Apple's Quick Time File Format Spec (if major type equals "qt  "), in which case
 * offsets in the 'ctts' box are interpreted as signed, where the ISO Base Media FF expects
 * the offsets in ctts to be positive.
 *
 * @return error
 */
int
mp4d_trackreader_set_type
(
        mp4d_trackreader_ptr_t,
        const mp4d_ftyp_info_t *p_type
);

/**
 * @brief Return the memory needed by an mp4d_trackreader instance.
 *
 * @return Error code:
 *     MP4D_NO_ERROR for success.
 */

int
mp4d_trackreader_query_mem
(
    uint64_t *static_mem_size,   /**< [out] */
    uint64_t *dynamic_mem_size   /**< [out] */
);

/**
 * @brief initialize an mp4d_trackreader instance.
 *
 * @return Error code:
 *     OK (0) for success.
 */
int
mp4d_trackreader_init
(
    mp4d_trackreader_ptr_t *trackreader_ptr,   
    void *static_mem,    
    void *dynamic_mem    
);

/**
 * @brief assign the default 'tenc' data to the track
 *
 * @return Error code:
 *     OK (0) for success.
 */
int
mp4d_trackreader_set_tenc
(
    mp4d_trackreader_ptr_t p_tr,
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
mp4d_trackreader_get_cur_tenc
(
    mp4d_trackreader_ptr_t p_tr,
    uint32_t* algorithmID,
    uint8_t* iv_size,
    uint8_t* kid
);

#ifdef __cplusplus
}
#endif

#endif
/** @} */
