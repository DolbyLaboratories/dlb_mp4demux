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
/** @defgroup fragment_stream
 *
 * @brief API for reading MP4 boxes into a buffer
 *
 * This API provides access to top-level boxes from an MP4 source.
 *
 * Concrete implementations of this API (derived classes) may read
 * boxes from a local MP4 file.
 *
 * @{
 */
#ifndef FRAGMENT_STREAM_H
#define FRAGMENT_STREAM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mp4d_demux.h"

typedef struct fragment_reader_t_ * fragment_reader_t;

struct fragment_reader_t_
{
    mp4d_demuxer_ptr_t p_dmux;     /* Initialized with the current box */
    void *p_static_mem;
    void *p_dynamic_mem;

    /** @brief Destructor
     * @return
     */
    void (*destroy)(fragment_reader_t);

    /** @brief Get the next atom, initializes demuxer object with this atom
     *  @return 0: no error
                1: unexpected error
                2: end of file and no more segments. Atom is not ready.
     */
    int (*next_atom)(fragment_reader_t);

    /* @brief Seek to a fragment, given a presentation time
     *
     * The source must move to a fragment (moov or moof) which starts before
     * the requested time and may or may not include the given time.
     * Sources which have no random-access information may move to the
     * first fragment. The client code has to call source->next_atom()
     * until the requested presentation time is reached.
     *
     * @return error
     */
    int (*seek)(fragment_reader_t,
                uint32_t track_ID,
                uint64_t seek_time, /**< presentation time, in media time scale */
                uint64_t *out_time  /**< [out] on success, actual seek time */
                );

    /* @brief Load data (sample, aux data) into buffer
     * @return error
     */
    int (*load)(fragment_reader_t,
                uint64_t position,
                uint32_t size,
                unsigned char *p_buffer  /* Output */
                );

    /* @brief Get file offset of current atom
     *
     * Only implemented for a file-based source (otherwise NULL).
     *
     * @return error
     */
    int (*get_offset)(fragment_reader_t,
                      uint64_t *offset  /**< [out] */
                      );

    /* @brief Get file type box
     *
     * May not be implemented for all sources, in which case the function pointer is NULL
     *
     * @return error
     */
    int (*get_type)(fragment_reader_t,
                    mp4d_ftyp_info_t *p_type  /**< [out] pointer to memory owned by the mp4_source object. */
                    );
};

/** @brief Destructor
 * @return
 */
void fragment_reader_destroy(fragment_reader_t);


/** @brief Initialize the base class. Memory must have been already allocated as prerequiste.
 */
int fragment_reader_init(fragment_reader_t);

/** @brief Deinitialize the base class, which does not free the memory.
 */
void fragment_reader_deinit(fragment_reader_t);

/** @brief Get the next atom, with which initializes the demuxer object with this atom.
 *  @return 0: no error
            1: unexpected error
            2: end of file and no more segments. Atom is not ready.
 */
int fragment_reader_next_atom(fragment_reader_t s);


int fragment_reader_seek(fragment_reader_t,
            uint32_t track_ID,
            uint64_t seek_time, /**< presentation time, in media time scale */
            uint64_t *out_time    /**< [out] on success, actual seek time */
            );

int fragment_reader_load(fragment_reader_t,
            uint64_t position,
            uint32_t size,
            unsigned char *p_buffer  /* Output */
            );

int fragment_reader_get_offset(fragment_reader_t,
                  uint64_t *offset    /**< [out] */
                  );

int fragment_reader_get_type(fragment_reader_t,
                mp4d_ftyp_info_t *p_type  /**< [out] pointer to memory owned by the mp4_source object. */
                );




#ifdef __cplusplus
}
#endif

#endif
/* @} */

