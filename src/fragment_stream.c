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

#include "fragment_stream.h"

#include "util.h"

#include <stdlib.h>

int fragment_reader_init(fragment_reader_t s)
{
    int err = 0;
    uint64_t static_mem_size, dyn_mem_size;

    CHECK( mp4d_demuxer_query_mem(&static_mem_size, &dyn_mem_size) );

    ASSURE( (uint64_t) (size_t) static_mem_size == static_mem_size, ("Cannot allocate %" PRIu64 " bytes of memory", static_mem_size) );
    ASSURE( (uint64_t) (size_t) dyn_mem_size == dyn_mem_size, ("Cannot allocate %" PRIu64 " bytes of memory", dyn_mem_size) );

    s->p_static_mem = malloc((size_t) static_mem_size);
    s->p_dynamic_mem = malloc((size_t) dyn_mem_size);

    CHECK( mp4d_demuxer_init(&s->p_dmux,
                             s->p_static_mem,
                             s->p_dynamic_mem) );
cleanup:
    return err;
}

void fragment_reader_deinit(fragment_reader_t s)
{
    if (s != NULL && s->p_dmux != NULL)
    {
        free(s->p_static_mem);
        free(s->p_dynamic_mem);
    }
}

void fragment_reader_destroy(fragment_reader_t s)
{
    if (s != NULL && s->destroy!= NULL)
    {
         s->destroy(s);
    }

}

int fragment_reader_next_atom(fragment_reader_t s)
{
    if (s != NULL && s->next_atom != NULL)
    {
         return (s->next_atom(s));
    }

    return -1;
}

int fragment_reader_seek(fragment_reader_t s,
            uint32_t track_ID,
            uint64_t seek_time, 
            uint64_t *out_time)
{
    if (s != NULL && s->seek != NULL)
    {
         return (s->seek(s, track_ID, seek_time, out_time));
    }

    return -1;
}

int fragment_reader_load(fragment_reader_t s,
            uint64_t position,
            uint32_t size,
            unsigned char *p_buffer  /* Output */
            )
{
    if (s != NULL && s->load != NULL)
    {
         return (s->load(s, position, size, p_buffer));
    }

    return -1;

}
int fragment_reader_get_offset(fragment_reader_t s,
                  uint64_t *offset  /**< [out] */
                  )
{
    if (s != NULL && s->get_offset != NULL)
    {
         return (s->get_offset(s, offset));
    }

    return -1;
}
int fragment_reader_get_type(fragment_reader_t s,
                mp4d_ftyp_info_t *p_type  /**< [out] pointer to memory owned by the mp4_source object. */
                )
{
    if (s != NULL && s->get_type != NULL)
    {
         return (s->get_type(s, p_type));
    }

    return -1;
}