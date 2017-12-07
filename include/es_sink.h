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
/** @defgroup es_sink
 *
 * @brief API for sinking samples to an elementary stream
 *
 * @{
 */
#ifndef ES_SINK_H
#define ES_SINK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mp4d_types.h"   /* mp4d_sampleref_t */

#include <stdio.h>

typedef struct es_sink_t_ *es_sink_t;

struct es_sink_t_
{
    /** @brief Inform the sink about a sample description entry

        The sample entry includes a data reference index, which is
        referred to by sample references provided to sample_ready.

        @return error code
     */
    int (*sample_entry) (
        es_sink_t,
        const mp4d_sampleentry_t *);

    /** @brief Notify that a sample is ready
     *
     *  @return error code
     */
    int (*sample_ready) (
        es_sink_t,
        const mp4d_sampleref_t *p_sample,
        const unsigned char *payload   /**< Sample buffer whose size is p_sample->size */
        );

    /** @brief Notify that a subsample is ready
     *
     *  Subsamples are provided after the corresponding sample, before
     *  the next sample, in the order of subsample offset.
     *
     *  The payload data provided to sample_ready() is valid (i.e. is not
     *  deallocated or overwritten) until the return of the last call of
     *  subsample_ready(), for a given sample.
     *
     *  For samples with no non-trivial subsample structure (i.e. contain
     *  one subsample which is the whole sample), this function is called
     *  once.
     *
     *  Implementations of the API may choose to not implement this
     *  method, in which case the function pointer is NULL.
     *
     *  @return error code
     */
    int (*subsample_ready) (
        uint32_t subsample_index,
        es_sink_t,
        const mp4d_sampleref_t *sample,  /**< parent sample */
        const unsigned char *payload,
        uint64_t offset,                 /**< relative to beginning of file */
        uint32_t size                    /**< in bytes */
        );

    void (*destroy) (es_sink_t);
};

int 
sink_sample_entry (
    es_sink_t,
    const mp4d_sampleentry_t *);


int 
sink_sample_ready (
    es_sink_t,
    const mp4d_sampleref_t *p_sample,
    const unsigned char *payload   /**< Sample buffer whose size is p_sample->size */
    );

int 
sink_subsample_ready (
    uint32_t subsample_index,
    es_sink_t,
    const mp4d_sampleref_t *sample,  /**< parent sample */
    const unsigned char *payload,
    uint64_t offset,                 /**< relative to beginning of file */
    uint32_t size                     /**< in bytes */
    );

void 
sink_destroy (
    es_sink_t
    );


/* Constructor for es_writer: dumps raw samples to file */
int
es_writer_new(
    es_sink_t *,    /**< [out] */
    uint32_t track_ID,
    const char *stream_name,
    const char *output_folder
    );

int
ac4_writer_new(
    es_sink_t *,     /**< [out] */
    uint32_t track_ID,
    const char *stream_name,
    const char *output_folder
    );

int
adts_writer_new(
    es_sink_t *,     /**< [out] */
    uint32_t track_ID,
    const char *stream_name,
    const char *output_folder
    );

int
h264_writer_new(
    es_sink_t *,     /**< [out] */
    uint32_t track_ID,
    const char *stream_name,
    const char *output_folder
    );

int
h264_validator_new(
    es_sink_t *,     /**< [out] */
    uint32_t track_ID,
    const char *stream_name,
    const char *output_folder
    );

int 
dv_el_writer_new(
    es_sink_t *,     /**< [out] */
    uint32_t track_ID, 
    const char *stream_name, 
    const char *codec_type, 
    const char *output_folder
    );

int 
dv_el_validator_new(
    es_sink_t *,     /**< [out] */
    uint32_t track_ID, 
    const char *stream_name, 
    const char *codec_type, 
    const char *output_folder
    );


int
hevc_writer_new(
    es_sink_t *,     /**< [out] */
    uint32_t track_ID,
    const char *stream_name,
    const char *output_folder,
    uint32_t stdout_flag
    );

int
hevc_validator_new(
    es_sink_t *,     /**< [out] */
    uint32_t track_ID,
    const char *stream_name,
    const char *output_folder,
    uint32_t stdout_flag
    );


/* Prints sample information to standard output */
int
sample_print_new(
    es_sink_t *,
    uint32_t media_time_scale,
    uint32_t track_ID,
    const char *stream_name);

int
subt_writer_new(
    es_sink_t *,
    uint32_t track_ID,
    const char *stream_name,
    const char *output_folder);

int
ddp_writer_new(
    es_sink_t *p_es_sink, 
    uint32_t track_ID, 
    const char *stream_name, 
    const char *output_folder);

/* DD+ ES Validator */
int 
ddp_validator_new(
    es_sink_t *, 
    uint32_t track_ID, 
    const char *stream_name, 
    const char *output_folder,
    int b_check_joc_flag);

/* AC4 ES Validator */
int 
ac4_validator_new(
    es_sink_t *, 
    uint32_t track_ID, 
    const char *stream_name, 
    const char *output_folder);

#ifdef __cplusplus
}
#endif

#endif
/* @} */
