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

#include "movie.h"

#include "file_stream.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>

typedef struct
{
    struct movie_t_ base;

    char *path;     /* Of mp4 file */
    fragment_reader_t file_source;

} *file_movie_t;

/* Find the first moov box, fail if it does not exist */
static int
init_file_source(file_movie_t p_fi)
{
    int err = 0;
    mp4d_fourcc_t type;

    if (p_fi->file_source != NULL)
    {
        return 0;
    }

    CHECK( file_stream_new(&p_fi->file_source, p_fi->path) );
    do
    {
        int err_next = fragment_reader_next_atom(p_fi->file_source);

        ASSURE( err_next != 2, ("%s: Missing 'moov' box", p_fi->path) );
        CHECK( err_next );
        CHECK( mp4d_demuxer_get_type(p_fi->file_source->p_dmux, &type) );
    } while (!MP4D_FOURCC_EQ(type, "moov"));

cleanup:
    return err;
}

void file_movie_destroy(movie_t p_movie)
{
    file_movie_t p_fi = (file_movie_t) p_movie;

    if (p_fi != NULL)
    {
        if (p_fi->file_source != NULL)
        {
            fragment_reader_destroy(p_fi->file_source);
        }
        free(p_fi->path);
        free(p_fi);
    }
}

int
file_movie_get_movie_info(movie_t p_movie,
                         mp4d_movie_info_t *p_movie_info
                         )
{
    int err = 0;
    file_movie_t p_fi = (file_movie_t) p_movie;

    CHECK( init_file_source(p_fi) );
    CHECK( mp4d_demuxer_get_movie_info(p_fi->file_source->p_dmux, p_movie_info) );

cleanup:
    return err;
}

int
file_movie_get_stream_info(movie_t p_movie,
                          uint32_t stream_num,
                          uint32_t bit_rate,   /* no used in file format */
                          mp4d_stream_info_t *p_stream_info,
                          char **stream_name
    )
{
    int err = 0;
    file_movie_t p_fi = (file_movie_t) p_movie;

    (void)bit_rate;

    CHECK( init_file_source(p_fi) );
    *stream_name = NULL;
    CHECK( mp4d_demuxer_get_stream_info(p_fi->file_source->p_dmux, stream_num, p_stream_info) );

cleanup:
    return err;
}

int
file_movie_get_sampleentry(movie_t p_movie,
                          uint32_t stream_num,
                          uint32_t bit_rate,   /* no used in file format */
                          uint32_t sample_description_index,
                          mp4d_sampleentry_t *p_sampleentry
                         )
{
    int err = 0;
    file_movie_t p_fi = (file_movie_t) p_movie;

    (void)bit_rate;

    CHECK( init_file_source(p_fi) );
    CHECK( mp4d_demuxer_get_sampleentry(p_fi->file_source->p_dmux, stream_num, sample_description_index, p_sampleentry) );

cleanup:
    return err;
}

int
file_movie_get_bitrate(movie_t p_movie,
                      uint32_t stream_num,
                      uint32_t index,
                      uint32_t *p_bitrate
                     )
{
    (void)p_movie;
    (void)stream_num;

    if (index == 0)
    {
        *p_bitrate = 0;
        return 0;
    }
    else
    {
        /* out of bit rates */
        return 2;
    }
}

int
file_movie_fragment_stream_new(movie_t p_movie,
                              uint32_t stream_num,
                              const char *stream_name,
                              uint32_t bitrate,
                              fragment_reader_t *p_source
                              )
{
    int err = 0;
    file_movie_t p_fi = (file_movie_t) p_movie;

    (void) bitrate;
    (void) stream_num;
    (void) stream_name;
    CHECK( file_stream_new(p_source, p_fi->path) );

cleanup:
    return err;
}

int movie_new(const char *path,   /**< path to MP4 file */
                           movie_t *p_movie        /**< [out] */
                           )
{
    int err = 0;
    file_movie_t p_fi = malloc(sizeof(*p_fi));

    ASSURE( p_fi != NULL, ("malloc failure") );

    p_fi->file_source = NULL;

    p_fi->base.destroy = file_movie_destroy;
    p_fi->base.get_movie_info = file_movie_get_movie_info;
    p_fi->base.get_stream_info = file_movie_get_stream_info;
    p_fi->base.get_sampleentry = file_movie_get_sampleentry;
    p_fi->base.get_bitrate = file_movie_get_bitrate;
    p_fi->base.fragment_stream_new = file_movie_fragment_stream_new;

    p_fi->path = malloc(strlen(path) + 1);
    ASSURE( p_fi->path != NULL, ("malloc failure") );
    strcpy(p_fi->path, path);

    *p_movie = &p_fi->base;
cleanup:
    if (err)
    {
        *p_movie = NULL;
        free(p_fi);
    }
    return err;
}

void movie_destroy(movie_t p_movie){
    if (p_movie != NULL){
        p_movie->destroy(p_movie);
    }
}