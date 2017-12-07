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
 * @defgroup player
 *
 * @brief Allows to play and seek multiple streams, either in sample PTS order, or
 * sample file offset order.
 *
 * This player provides functionality to play and seek in samples from a fragment_stream
 * and implements the connections to the MP4 source, and to the output ES sinks.
 * @{
 */
#ifndef PLAYER_H
#define PLAYER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stream.h"
#include "movie.h"

#include "es_sink.h"

/**
 * @brief player handle
 */
typedef struct player_t_ *player_t;

/* Decryption (id, key) pair */
typedef struct
{
    struct {
        unsigned char id[16];
        unsigned char key[16];
    } *keys;
    uint32_t num_keys;
} decrypt_info_t;

struct player_t_
{
    uint32_t movie_time_scale;

    decrypt_info_t decrypt_info;

    /* active streams */
    struct
    {
        stream_t stream;

        /* Array of decrypt info per sample entry */
        struct sample_entry_t_
        {
            uint32_t index;                /* sample description index, initialized only if encrypted */
            mp4d_sampleentry_t entry;      /* sample entry, initialized only if encrypted */
            uint8_t iv_size;               /* initial value size in bytes */
        } *sample_entries;
        uint32_t num_sample_entries;

        es_sink_t sink[3];        /* NULL-terminated array of up to two sinks
                                     subscribed to this stream */

        int end_of_track;         /* (bool) Last sample was later than stop time */

        unsigned char *data;      /* Sample payload (size = sample.size)
                                     Stored in memory, until the next sample is ready. */
        size_t data_size;         /* Allocated size >= sample.size */

    } * streams;
    uint32_t num_streams;

    /* To control playback */
    uint64_t stop_time;  /* in movie time scale */
    int single_fragment; /* (boolean) play a single fragment, or all */

    /* Function that defines the order in which driver_next_sample provides samples
       (e.g. by file position, by pts). Samples with a lesser value returned by eval_sample
       are returned before samples with a higher value.
    */
    uint64_t (*eval_sample)(const mp4d_sampleref_t *, uint32_t media_time_scale);
};

/**
 *  @brief Constructor
 *  @return error
 */
int
player_new(player_t *);

/**
 *  @brief Destructor
 *  @return error
 *
 *  Sets pointer to NULL.
 */
int
player_destroy(player_t *);

/**
 * @brief set handler for a track's samples and sample entries.
 *
 * The ownership of the sink object is transferred to
 * the driver instance. The sink will be free'd when
 * player_destroy() is called.
 *
 * Multiple sinks can be registered for the same track_ID.
 * The fragment_stream parameter is ignored if a sink for this
 * track_ID is already registered.
 */
int
player_set_track(player_t,
                 uint32_t track_ID,        /* The sink receives samples for this track_ID */
                 const char *stream_name,  /* For PIFF track_ID = 0 and the stream_name is the identifier */
                 uint32_t bit_rate,
                 movie_t p_movie,        /* movie information */
                 fragment_reader_t mp4_source,  /* fragment stream, transfer of ownership, must not be freed by caller */
                 es_sink_t sink,
                 uint32_t polarssl_flag);

/**
 * @brief provide samples in the order of presentation time
 *
 * Processes samples in a given presentation time range.
 */
int
player_play_time_range(player_t,
                       float *start_time,   /**> in seconds, NULL: beginning */
                       float *stop_time     /**> in seconds, NULL: end */
    );

/**
 * @brief process samples in order of sample position
 *
 * Processes a single fragment, or all fragments.
 */
int
player_play_fragments(player_t,
                      uint32_t fragment_number /**> counting from 1 (moov), 0: all */
    );

#ifdef __cplusplus
}
#endif

#endif
