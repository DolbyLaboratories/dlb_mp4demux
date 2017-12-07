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
/** @defgroup file_stream
 *
 * @brief Provide fragments from a file source.
 *
 * Implements the fragment_reader_t API.

 * @{
 */
#ifndef FILE_STREAM_H
#define FILE_STREAM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "fragment_stream.h"

/** @brief Create a fragment stream from a local file
 * @return error
 */
int file_stream_new(fragment_reader_t *,     /**< [out] */
                             const char *path);



/** @brief Seek according to a sidx box in the file
 *
 * If the file does not contain a sidx box before the first moof,
 * seeks to the first moof
 *
 * This function is used by the DASH stream.
 *
 * @return error
 */
int
file_stream_seek_sidx(fragment_reader_t,      /* File stream object */
                      uint64_t seek_time,     /* requested (global) time */
                      uint64_t segment_start, /* segment start time */
                      uint64_t *out_time      /* output seek time */
                      );

#ifdef __cplusplus
}
#endif

#endif
/* @} */
