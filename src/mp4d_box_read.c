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

#include <stdlib.h>
#include "mp4d_box_read.h"
#include "mp4d_internal.h"

static const uint32_t CENC = 1667591779;  /** common encryption */

static mp4d_atom_t
buffer_to_atom(const mp4d_buffer_t * buffer)
{
    mp4d_atom_t atom;

    memset(&atom, 0, sizeof(atom));

    atom.size = buffer->size + (buffer->p_data - buffer->p_begin);
    atom.p_data = buffer->p_begin;

    return atom;
}

mp4d_error_t
mp4d_tts_init(tts_reader_t *p_r, mp4d_atom_t *p_tts, int delta_encoded)
{
    ASSURE( p_r != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( p_tts != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( p_tts->p_data != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );

    p_r->buffer = mp4d_atom_to_buffer(p_tts);
    p_r->delta_encoded = delta_encoded;
    
    {
        uint32_t flags;
        
        p_r->tts_version = mp4d_read_u8(&p_r->buffer);
        flags            = mp4d_read_u24(&p_r->buffer);

        /* stts */
        if( 0 != delta_encoded )
        {
            ASSURE( 0 == p_r->tts_version, MP4D_E_UNSUPPRTED_FORMAT, ("Unknown stts version %" PRIu32, p_r->tts_version) );
        }
        /* ctts */
        else
        {
            ASSURE( 0 == p_r->tts_version || 1 == p_r->tts_version, MP4D_E_UNSUPPRTED_FORMAT, ("Unknown ctts version %" PRIu32, p_r->tts_version) );
        }
        
        ASSURE( flags == 0, MP4D_E_UNSUPPRTED_FORMAT, ("Unknown *tts flags %" PRIu32, flags) );
    }
    
    p_r->entry_count = mp4d_read_u32(&p_r->buffer);
    
    /* Force rewind in the first call to get_ts() / get_ts_next() */
    p_r->next_sample_index = 0;
    p_r->cur_entry_sample_count = 0;
    p_r->cur_entry_consumed = 0;
    
    return MP4D_NO_ERROR;
}

mp4d_error_t
mp4d_tts_get_ctts_next(tts_reader_t *p_r, uint32_t *p_ts)
{
    ASSURE( p_r != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( p_ts != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( p_r->buffer.p_data != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );  /* not initialized */

    if (p_r->cur_entry_consumed < p_r->cur_entry_sample_count)
    {
        p_r->next_sample_index++;
        p_r->cur_entry_consumed++;
        *p_ts = p_r->cur_entry_sample_value;
    
        return MP4D_NO_ERROR;
    }
    else
    {
        /* If current entry is consumed, use the generic function */
        uint64_t ts;
        mp4d_error_t err = mp4d_tts_get_ts(p_r, p_r->next_sample_index, &ts, NULL);

        *p_ts = (uint32_t) ts;  /* safe: is uint32_t for ctts */

        return err;
    }
}

mp4d_error_t
mp4d_tts_get_stts_next(tts_reader_t *p_r, uint64_t *p_ts, uint32_t *p_duration)
{
    ASSURE( p_r != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( p_ts != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( p_duration != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( p_r->buffer.p_data != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );  /* not initialized */

    if (p_r->cur_entry_consumed < p_r->cur_entry_sample_count)
    {
        p_r->next_sample_index++;
        p_r->cur_entry_consumed++;
        p_r->cur_dts += p_r->cur_entry_sample_value;
        *p_ts = p_r->cur_dts;
        *p_duration = p_r->cur_entry_sample_value;
    
        return MP4D_NO_ERROR;
    }
    else
    {
        /* If current entry is consumed, use the generic function */
        return mp4d_tts_get_ts(p_r, p_r->next_sample_index, p_ts, p_duration);
    }
}

mp4d_error_t
mp4d_tts_get_ts(tts_reader_t *p_r, uint64_t sample_index, uint64_t *p_ts, uint32_t *p_duration)
{
    ASSURE( p_r != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( p_ts != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( p_r->buffer.p_data != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );  /* not initialized */
    ASSURE( (p_duration == NULL && !p_r->delta_encoded) ||
            (p_duration != NULL && p_r->delta_encoded), MP4D_E_WRONG_ARGUMENT, ("Null input") ); /* Can only get DTS duration */

    if (sample_index < p_r->next_sample_index - 1)
    {
        /* Requested sample is before the current position */

        /* Rewind */
        ASSURE( p_r->entry_count > 0, MP4D_E_NEXT_SEGMENT, ("empty *tts table") );

        p_r->cur_entry_index = 0;
        mp4d_seek(&p_r->buffer, 1 + 3 + 4);  /* version + flags + entry_count */

        p_r->cur_entry_sample_count = mp4d_read_u32(&p_r->buffer);
        p_r->cur_entry_sample_value = mp4d_read_u32(&p_r->buffer);
        
        /* Skip to the first non-empty entry */
        while (p_r->cur_entry_sample_count == 0)
        {
            ASSURE( p_r->entry_count > p_r->cur_entry_index + 1, MP4D_E_NEXT_SEGMENT,
                    ("out of *tts entries (count = %" PRIu32 ")", p_r->entry_count) );

            p_r->cur_entry_sample_count = mp4d_read_u32(&p_r->buffer);
            p_r->cur_entry_sample_value = mp4d_read_u32(&p_r->buffer);
            p_r->cur_entry_index++;
        }
        p_r->cur_entry_consumed = 0;

        /* Read the first sample */
        p_r->next_sample_index = 1;
        p_r->cur_dts = 0;
        p_r->cur_entry_consumed++;
    }

    while (sample_index > p_r->next_sample_index - 1)
    {
        p_r->cur_dts += p_r->cur_entry_sample_value;

        while (p_r->cur_entry_consumed == p_r->cur_entry_sample_count)
        {
            /* Consumed this entry, move to next entry */
            ASSURE( p_r->entry_count > p_r->cur_entry_index + 1, MP4D_E_NEXT_SEGMENT,
                    ("out of *tts entries (count = %" PRIu32 ")", p_r->entry_count) );

            p_r->cur_entry_sample_count = mp4d_read_u32(&p_r->buffer);
            p_r->cur_entry_sample_value = mp4d_read_u32(&p_r->buffer);

            p_r->cur_entry_index++;
            p_r->cur_entry_consumed = 0;
        }            

        {
            /* Jump forward to the requested sample, but stay inside this entry */
            uint32_t step = p_r->cur_entry_sample_count - p_r->cur_entry_consumed;
            if (sample_index - p_r->next_sample_index + 1 < step)
            {
                step = (uint32_t) (sample_index - p_r->next_sample_index + 1);
            }
            p_r->next_sample_index += step;
            p_r->cur_entry_consumed += step;
            p_r->cur_dts += (step - 1) * p_r->cur_entry_sample_value;
        }
    }

    if (p_r->delta_encoded)
    {
        *p_ts = p_r->cur_dts;
        *p_duration = p_r->cur_entry_sample_value;
    }
    else
    {
        *p_ts = p_r->cur_entry_sample_value;
    }
    
    return MP4D_NO_ERROR;
}

/* end stts */

/* begin stsz */
mp4d_error_t
mp4d_stsz_init(stsz_reader_t *p_r, mp4d_atom_t *p_stsz, int is_stz2)
{
    ASSURE( p_r != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( p_stsz != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );

    p_r->buffer = mp4d_atom_to_buffer(p_stsz);

    {
        uint8_t version = mp4d_read_u8(&p_r->buffer);
        uint32_t flags = mp4d_read_u24(&p_r->buffer);
    
        ASSURE( version == 0, MP4D_E_UNSUPPRTED_FORMAT, ("Unknown stsz version %" PRIu32, version) );
        ASSURE( flags == 0, MP4D_E_UNSUPPRTED_FORMAT, ("Unknown stsz flags %" PRIu32, flags) );
    }

    /* There are three formats: 
       - stsz with sample_size == 0,
       - stsz with sample_size != 0,
       - stz2
    */
       
    if (is_stz2)
    {
        mp4d_skip_bytes(&p_r->buffer, 3);             /* reserved */
        p_r->field_size = mp4d_read_u8(&p_r->buffer);
        
        ASSURE( p_r->field_size == 4 ||
                p_r->field_size == 8 ||
                p_r->field_size == 16, MP4D_E_UNSUPPRTED_FORMAT,
                ("stz2 field size must be 4, 8 or 16, got %" PRIu8, p_r->field_size) );
        
        p_r->sample_size = 0;
    }
    else
    {
        p_r->field_size = 32;
        p_r->sample_size = mp4d_read_u32(&p_r->buffer);
    }

    p_r->sample_count = mp4d_read_u32(&p_r->buffer);
    p_r->next_sample_index = 0;

    return MP4D_NO_ERROR;
}

mp4d_error_t
mp4d_stsz_get(stsz_reader_t *p_r, 
              uint64_t sample_index, /* counting from zero */
              uint32_t *p_size)
{
    ASSURE( p_r != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( p_size != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );

    {
        mp4d_atom_t atom = buffer_to_atom(&p_r->buffer);
        int is_stz2 = (p_r->field_size < 32);

        CHECK( mp4d_stsz_init(p_r, &atom, is_stz2) );
    }

    while (1)
    {
        CHECK( mp4d_stsz_get_next(p_r, p_size) );
        if (sample_index == 0)
        {
            break;
        }
        sample_index--;
    }

    return MP4D_NO_ERROR;
}

mp4d_error_t
mp4d_stsz_get_next(stsz_reader_t *p_r, uint32_t *p_size)
{
    ASSURE( p_r != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( p_size != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( p_r->buffer.p_data != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") ); /* not initialized */

    ASSURE( p_r->next_sample_index < p_r->sample_count, MP4D_E_NEXT_SEGMENT,
            ("Out of stsz samples (count is %" PRIu32 ")", p_r->sample_count) );

    if (p_r->sample_size != 0)
    {
        /* stsz with constant samples */
        *p_size = p_r->sample_size;
        p_r->next_sample_index++;
        
        return MP4D_NO_ERROR;
    }
    else
    {
        switch(p_r->field_size)
        {
        case 4:
            if ((p_r->next_sample_index & 1) == 0)
            {
                p_r->size_4 = mp4d_read_u8(&p_r->buffer);
                *p_size = (p_r->size_4 >> 4) & 15;
            }
            else
            {
                *p_size = (p_r->size_4) & 15;
            }
            break;
        case 8:
            *p_size = mp4d_read_u8(&p_r->buffer);
            break;
        case 16:
            *p_size = mp4d_read_u16(&p_r->buffer);
            break;
        case 32:
            *p_size = mp4d_read_u32(&p_r->buffer);
            break;
        default:
            /* impossible */
            break;
        }

        p_r->next_sample_index++;

        return MP4D_NO_ERROR;
    }
}

/* end stsz */

/* begin stsc */

mp4d_error_t
mp4d_stsc_init(stsc_reader_t *p_r, mp4d_atom_t *p_stsc)
{
    ASSURE( p_r != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( p_stsc != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );

    p_r->buffer = mp4d_atom_to_buffer(p_stsc);

    {
        uint8_t version = mp4d_read_u8(&p_r->buffer);
        uint32_t flags = mp4d_read_u24(&p_r->buffer);
    
        ASSURE( version == 0, MP4D_E_UNSUPPRTED_FORMAT, ("Unknown stsc version %" PRIu32, version) );
        ASSURE( flags == 0, MP4D_E_UNSUPPRTED_FORMAT, ("Unknown stsc flags %" PRIu32, flags) );
    }
    
    p_r->entry_count = mp4d_read_u32(&p_r->buffer);

    if (p_r->entry_count >= 1)
    {
        /* Peek the first entry */
        p_r->next_first_chunk = mp4d_read_u32(&p_r->buffer);
    }
    else
    {
        /* Make sure that first call to get_next() fails */
        p_r->next_first_chunk = 1;
    }
    p_r->cur_chunk = p_r->next_first_chunk - 1;
    p_r->samples_consumed = 0;
    p_r->cur_samples_per_chunk = 0;
    p_r->cur_entry_index = 0;

    return MP4D_NO_ERROR;
}

mp4d_error_t
mp4d_stsc_get_next(stsc_reader_t *p_r, uint32_t *chunk_index, uint32_t *sample_description_index,
                   uint32_t *sample_index_in_chunk)
{
    ASSURE( p_r != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( chunk_index != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( sample_description_index != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( sample_index_in_chunk != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( p_r->buffer.p_data != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") ); /* not initialized */

    while (p_r->samples_consumed == p_r->cur_samples_per_chunk)
    {
        p_r->cur_chunk++;
        p_r->samples_consumed = 0;

        /* While no more samples in the current entry */
        while (p_r->cur_chunk == p_r->next_first_chunk || p_r->cur_samples_per_chunk == 0)
        {
            /* Read the next entry */
            ASSURE( p_r->cur_entry_index < p_r->entry_count, MP4D_E_NEXT_SEGMENT,
                    ("Out of stsc entries (count is %" PRIu32 ")", p_r->entry_count) );

            p_r->cur_chunk = p_r->next_first_chunk;
            p_r->cur_samples_per_chunk = mp4d_read_u32(&p_r->buffer);
            p_r->cur_sample_description_index = mp4d_read_u32(&p_r->buffer);
            p_r->cur_entry_index++;

            if (p_r->entry_count > p_r->cur_entry_index)
            {
                /* Peek the next entry, in order to determine the number of chunks in the current entry */
                p_r->next_first_chunk = mp4d_read_u32(&p_r->buffer);

                /* first_chunk must be ascending */
                ASSURE( p_r->next_first_chunk >= p_r->cur_chunk, MP4D_E_UNSUPPRTED_FORMAT,
                        ("stsc: First chunk must be ascending, current = %" PRIu32", next = %" PRIu32,
                         p_r->cur_chunk, p_r->next_first_chunk) );
            }
            else
            {
                /* Current entry is the last entry */
                p_r->next_first_chunk = (uint32_t) -1;
            }
        }
    }

    *chunk_index = p_r->cur_chunk;
    *sample_description_index = p_r->cur_sample_description_index;
    *sample_index_in_chunk = p_r->samples_consumed;
    p_r->samples_consumed++;

    return MP4D_NO_ERROR;
}

/* end stsc */

/* begin stco, co64 */

mp4d_error_t
mp4d_co_init(co_reader_t *p_r, mp4d_atom_t *p_atom, int is_co64)
{
    ASSURE( p_r != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( p_atom != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );

    p_r->chunk_offsets = mp4d_atom_to_buffer(p_atom);

    {
        uint8_t version = mp4d_read_u8(&p_r->chunk_offsets);
        uint32_t flags = mp4d_read_u24(&p_r->chunk_offsets);
    
        ASSURE( version == 0, MP4D_E_UNSUPPRTED_FORMAT, ("Unknown stco/co64 version %" PRIu32, version) );
        ASSURE( flags == 0, MP4D_E_UNSUPPRTED_FORMAT, ("Unknown stco/co64 flags %" PRIu32, flags) );
    }

    p_r->entry_count = mp4d_read_u32(&p_r->chunk_offsets);
    p_r->cur_entry_index = 0;
    p_r->is_co64 = is_co64;

    return MP4D_NO_ERROR;
}

mp4d_error_t
mp4d_co_get_next(co_reader_t *p_r, uint64_t *chunk_offset)
{
    ASSURE( p_r != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( chunk_offset != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( p_r->chunk_offsets.p_data != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );

    ASSURE( p_r->cur_entry_index < p_r->entry_count, MP4D_E_NEXT_SEGMENT,
            ("stco/co64: out of entries (count = %" PRIu32 ")", p_r->entry_count) );

    p_r->cur_entry_index++;

    if (p_r->is_co64)
    {
        *chunk_offset = mp4d_read_u64(&p_r->chunk_offsets);
    }
    else
    {
        *chunk_offset = mp4d_read_u32(&p_r->chunk_offsets);
    }

    return MP4D_NO_ERROR;
}

/* end stco, co64 */

/* begin stss */
mp4d_error_t
mp4d_stss_init(stss_reader_t *p_r,
               mp4d_atom_t *p_atom)
{
    ASSURE( p_r != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );

    if (p_atom == NULL)
    {
        p_r->buffer.p_data = NULL;
        return MP4D_NO_ERROR;
    }

    p_r->buffer = mp4d_atom_to_buffer(p_atom);

    {
        uint8_t version = mp4d_read_u8(&p_r->buffer);
        uint32_t flags = mp4d_read_u24(&p_r->buffer);
    
        ASSURE( version == 0, MP4D_E_UNSUPPRTED_FORMAT, ("Unknown stss version %" PRIu32, version) );
        ASSURE( flags == 0, MP4D_E_UNSUPPRTED_FORMAT, ("Unknown stss flags %" PRIu32, flags) );
    }

    p_r->entries_left = mp4d_read_u32(&p_r->buffer);
    p_r->count = p_r->entries_left; 
    if (p_r->count != 0)
    {
        p_r->stts_content = (unsigned char*)malloc(4*p_r->count);
        memcpy(p_r->stts_content, p_r->buffer.p_data, 4*p_r->count);
    }
    p_r->next_sync_sample = 0;
    p_r->cur_sample_number = 0;

    return MP4D_NO_ERROR;
}


mp4d_error_t
mp4d_stss_get_next(stss_reader_t *p_r,
                   int * is_sync  /** [out] */
    )
{
    ASSURE( is_sync != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );

    if (p_r->buffer.p_data == NULL)
    {
        *is_sync = 1;
        return MP4D_NO_ERROR;
    }

    p_r->cur_sample_number++;

    while (p_r->cur_sample_number > p_r->next_sync_sample)
    {
        if (p_r->entries_left > 0)
        {
            uint32_t old_sync_sample = p_r->next_sync_sample;
            p_r->next_sync_sample = mp4d_read_u32(&p_r->buffer);
            p_r->entries_left--;
            
            /* A conforming stss table is arranged in strictly increasing order
               of sample number */
            ASSURE( old_sync_sample < p_r->next_sync_sample, MP4D_E_UNSUPPRTED_FORMAT,
                    ("Non-conforming stss sample number order: sync sample = %" PRIu32 ", next sync sample = %" PRIu32,
                     old_sync_sample, p_r->next_sync_sample) );
        }
        else
        {
            p_r->next_sync_sample = (uint32_t) -1;  /* infinity */
        }
    }

    *is_sync = (p_r->cur_sample_number == p_r->next_sync_sample);
    
    return MP4D_NO_ERROR;
}

/* end stss */

/* begin elst */
mp4d_error_t
mp4d_elst_init(elst_reader_t *p_r,
               mp4d_atom_t *p_atom,
               uint32_t media_time_scale,
               uint32_t movie_time_scale
    )
{
    ASSURE( p_r != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( media_time_scale > 0, MP4D_E_WRONG_ARGUMENT, ("media_time_scale is 0") );
    ASSURE( movie_time_scale > 0, MP4D_E_WRONG_ARGUMENT, ("movie_time_scale_is 0") );

    if (p_atom == NULL)
    {
        p_r->buffer.p_data = NULL;
        return MP4D_NO_ERROR;
    }

    p_r->media_ts = media_time_scale;
    p_r->movie_ts = movie_time_scale;
    p_r->buffer = mp4d_atom_to_buffer(p_atom);
    p_r->version = mp4d_read_u8(&p_r->buffer);
    ASSURE( p_r->version == 0 || p_r->version == 1, MP4D_E_UNSUPPRTED_FORMAT, 
            ("Unknown elst version %" PRIu32, p_r->version) );
    {
        uint32_t flags = mp4d_read_u24(&p_r->buffer);
        ASSURE( flags == 0, MP4D_E_UNSUPPRTED_FORMAT, ("Unknown elst flags %" PRIu32, flags) );
    }

    p_r->entries_left = mp4d_read_u32(&p_r->buffer);
    p_r->media_time = -1;
    p_r->segment_start = 0;
    p_r->segment_duration = 0;

    return MP4D_NO_ERROR;
}

mp4d_error_t
mp4d_elst_get_presentation_time(elst_reader_t *p_r,
                                uint64_t media_time,
                                uint32_t duration,
                                int64_t *p_time,
                                uint32_t *p_offset,
                                uint32_t *p_duration
    )
{
    int64_t prev_media_time = -1;  /* Latest positive media time, or -1 for the initial entries */

    ASSURE( p_r != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( p_time != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( p_offset != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( p_duration != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );

    if (p_r->buffer.p_data == NULL)
    {
        /* No elst */
        *p_time = media_time;
        *p_offset = 0;
        *p_duration = duration;

        return MP4D_NO_ERROR;
    }

    if (media_time < p_r->media_time + (p_r->segment_duration * p_r->media_ts) / p_r->movie_ts)
    {
        /* Rewind to beginning of elst */
        mp4d_seek(&p_r->buffer, 1 + 3);  /* version + flags */

        p_r->entries_left = mp4d_read_u32(&p_r->buffer);

        p_r->media_time = -1;
        p_r->segment_start = 0;
        p_r->segment_duration = 0;
    }

    /* Read next entry while entry is empty or end of entry is before the requested media time */
    while (p_r->media_time == -1 || 
           (p_r->media_rate == 1 && p_r->media_time + (p_r->segment_duration * p_r->media_ts) / p_r->movie_ts <= media_time) ||
           (p_r->media_rate == 0 && p_r->media_time < (int64_t) media_time))
    {
        if (p_r->media_time > -1)
        {
            prev_media_time = p_r->media_time;
        }

        ASSURE( p_r->entries_left > 0, MP4D_E_INFO_NOT_AVAIL, 
                ("No more elst entries for time=%" PRIu64 ", duration=%" PRIu32, media_time, duration) );

        p_r->segment_start += p_r->segment_duration;

        if (p_r->version == 1)
        {
            p_r->segment_duration = mp4d_read_u64(&p_r->buffer);
            p_r->media_time = (int64_t) mp4d_read_u64(&p_r->buffer);
        }
        else
        {
            p_r->segment_duration = mp4d_read_u32(&p_r->buffer);
            p_r->media_time = (int32_t) mp4d_read_u32(&p_r->buffer);
        }
        p_r->media_rate = (int16_t) mp4d_read_u16(&p_r->buffer);
        ASSURE( p_r->media_rate == 0 || p_r->media_rate == 1, MP4D_E_UNSUPPRTED_FORMAT,
                ("Unsupported media_rate = %" PRIu32 " (expected 0 or 1)", p_r->media_rate) );
        {
            uint16_t media_rate_fraction = mp4d_read_u16(&p_r->buffer);
            
            ASSURE( media_rate_fraction == 0, MP4D_E_UNSUPPRTED_FORMAT,
                    ("media_rate_fraction must be zero, got %" PRIu16, media_rate_fraction) );

        }
        
        ASSURE( p_r->media_time >= -1, MP4D_E_UNSUPPRTED_FORMAT,
                ("media_time must be >= -1, got %" PRId64, p_r->media_time) );

        if (p_r->media_time >= 0)
        {
            ASSURE( p_r->media_time >= prev_media_time, MP4D_E_UNSUPPRTED_FORMAT,
                    ("Unsupported media time = %" PRId64 ", previous media time = %" PRId64
                     ", must be increasing", p_r->media_time, prev_media_time) );
        }
        
        p_r->entries_left -= 1;
    }

    /* Current segment ends at media_time or later */
 
    /* Case 1:
       sample:          |---|
       segment:     |-----|         --> time

       Case 2 (typical):
       sample:        |--| 
       segment:     |-----|

       Case 3:
       sample:     |---| 
       segment:      |-----|
    
       Case 4:
       sample:     |----| 
       segment:      |-|
    
       Case 5:
       sample:  |--| 
       segment:     |---|
    */
        
    *p_time = ((p_r->segment_start * p_r->media_ts) / p_r->movie_ts + (media_time - p_r->media_time));
    if (media_time + duration > p_r->media_time + (p_r->segment_duration * p_r->media_ts) / p_r->movie_ts)
    {
        if ((int64_t) media_time >= p_r->media_time)
        {
            /* 1 */
            *p_offset = 0;
            *p_duration = (uint32_t) ((p_r->segment_duration * p_r->media_ts) / p_r->movie_ts - (media_time - p_r->media_time));
        }
        else
        {
            /* 4 */
            *p_offset = (uint32_t) (p_r->media_time - media_time);
            *p_duration = (uint32_t) ((p_r->segment_duration * p_r->media_ts) / p_r->movie_ts);
        }
    }
    else
    {
        /* 5 */
        ASSURE( (int64_t) (media_time + duration) > p_r->media_time, MP4D_E_INFO_NOT_AVAIL,
                ("Sample CTS + duration (%" PRId64 ") must not be after current media_time (%" PRIu64 ")",
                 media_time + duration, p_r->media_time) );

        if ((int64_t) media_time >= p_r->media_time)
        {
            /* 2 */
            *p_offset = 0;
            *p_duration = duration;
        }
        else
        {
            /* 3 */
            *p_offset = (uint32_t) (p_r->media_time - media_time);
            *p_duration = duration - *p_offset;
        }
    }

    if (p_r->media_rate == 0)
    {
        ASSURE( 0, MP4D_E_UNSUPPRTED_FORMAT,
                ("media_rate = %" PRId16 ", dwells are not supported",
                 p_r->media_rate) );
        /* Is valid but we do not support dwells.
           The values just computed do not make sense
           if current entry is a dwell. */
    }
    
    return MP4D_NO_ERROR;
}
    
/* end elst */
    
/* begin sdtp */
mp4d_error_t
mp4d_sdtp_init(sdtp_reader_t *p_r, mp4d_atom_t *p_sdtp, uint32_t sample_count)
{
    ASSURE( p_r != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( p_sdtp != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );

    p_r->buffer = mp4d_atom_to_buffer(p_sdtp);

    {
        uint8_t version = mp4d_read_u8(&p_r->buffer);
        uint32_t flags = mp4d_read_u24(&p_r->buffer);
    
        ASSURE( version == 0, MP4D_E_UNSUPPRTED_FORMAT, ("Unknown sdtp version %" PRIu32, version) );
        ASSURE( flags == 0, MP4D_E_UNSUPPRTED_FORMAT, ("Unknown sdtp flags %" PRIu32, flags) );
    }

    p_r->sample_count = sample_count;
    p_r->next_sample_index = 0;

    return MP4D_NO_ERROR;
}

mp4d_error_t
mp4d_sdtp_get_next(sdtp_reader_t *p_r, uint8_t *p_entry)
{
    ASSURE( p_r != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( p_entry != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( p_r->buffer.p_data != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") ); /* not initialized */

    ASSURE( p_r->next_sample_index < p_r->sample_count, MP4D_E_NEXT_SEGMENT,
            ("Out of sdtp samples (count = %" PRIu32 ")", p_r->sample_count) );

    *p_entry = mp4d_read_u8(&p_r->buffer);

    p_r->next_sample_index++;

    return MP4D_NO_ERROR;
}

/* end sdtp */

/* begin stdp */
mp4d_error_t
mp4d_stdp_init(stdp_reader_t *p_r, mp4d_atom_t *p_stdp, uint32_t sample_count)
{
    ASSURE( p_r != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( p_stdp != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );

    p_r->buffer = mp4d_atom_to_buffer(p_stdp);
    {
        uint8_t version = mp4d_read_u8(&p_r->buffer);
        uint32_t flags = mp4d_read_u24(&p_r->buffer);
    
        ASSURE( version == 0, MP4D_E_UNSUPPRTED_FORMAT, ("Unknown stdp version %" PRIu32, version) );
        ASSURE( flags == 0, MP4D_E_UNSUPPRTED_FORMAT, ("Unknown stdp flags %" PRIu32, flags) );
    }

    p_r->sample_count = sample_count;
    p_r->next_sample_index = 0;

    return MP4D_NO_ERROR;
}

mp4d_error_t
mp4d_stdp_get_next(stdp_reader_t *p_r, uint16_t *p_priority)
{
    ASSURE( p_r != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( p_priority != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( p_r->buffer.p_data != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") ); /* not initialized */

    ASSURE( p_r->next_sample_index < p_r->sample_count, MP4D_E_NEXT_SEGMENT,
            ("Out of stdp samples (count = %" PRIu32 ")", p_r->sample_count) );

    *p_priority = mp4d_read_u16(&p_r->buffer);

    p_r->next_sample_index++;

    return MP4D_NO_ERROR;
}

/* end stdp */

/* begin trik */
mp4d_error_t
mp4d_trik_init(trik_reader_t *p_r, mp4d_atom_t *p_trik, uint32_t sample_count)
{
    ASSURE( p_r != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( p_trik != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );

    p_r->buffer = mp4d_atom_to_buffer(p_trik);

    {
        uint8_t version = mp4d_read_u8(&p_r->buffer);
        uint32_t flags = mp4d_read_u24(&p_r->buffer);
    
        ASSURE( version == 0, MP4D_E_UNSUPPRTED_FORMAT, ("Unknown trik version %" PRIu32, version) );
        ASSURE( flags == 0, MP4D_E_UNSUPPRTED_FORMAT, ("Unknown trik flags %" PRIu32, flags) );
    }
    if (sample_count != 0) /* if sample counts available from trun */
    {
        p_r->sample_count = sample_count;
    }
    else /* In CFF spec(Figure 2.3) box layout, the trik box placed before the trun; So we just infer the sample count from trik 
         box size. Also we think this is a defect for CFF spec. Maybe this will be rectified in the future version of CFF spec. */
    {
        p_r->sample_count = (uint32_t)p_r->buffer.size;
    }

    p_r->next_sample_index = 0;

    return MP4D_NO_ERROR;
}

mp4d_error_t
mp4d_trik_get_next(trik_reader_t *p_r, uint8_t *p_pic_type, uint8_t *p_dependency_level)
{
    ASSURE( p_r != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( p_pic_type != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( p_dependency_level != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( p_r->sample_count > 0, MP4D_E_WRONG_ARGUMENT,
          ("Have not got sample count yet (count = %" PRIu32 ")", p_r->sample_count) );
    ASSURE( p_r->next_sample_index < p_r->sample_count, MP4D_E_NEXT_SEGMENT,
          ("Out of trick samples (count = %" PRIu32 ")", p_r->sample_count) );

    {
        uint8_t temp = mp4d_read_u8(&p_r->buffer);
        *p_pic_type = temp >> 6;
        *p_dependency_level = temp & 0x3f;
    }

    p_r->next_sample_index++;

    return MP4D_NO_ERROR;
}

/* end trik */

/* begin senc */
mp4d_error_t
mp4d_senc_init(senc_reader_t *p_r, mp4d_atom_t *p_senc)
{
    ASSURE( p_r != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( p_senc != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );

    p_r->buffer = mp4d_atom_to_buffer(p_senc);

    {
        uint8_t version = mp4d_read_u8(&p_r->buffer);
        ASSURE( version == 0, MP4D_E_UNSUPPRTED_FORMAT, ("Unknown senc version %" PRIu32, version) );
    }

    p_r->flags = mp4d_read_u24(&p_r->buffer);
    p_r->sample_count = mp4d_read_u32(&p_r->buffer);
    p_r->next_sample_index = 0;

    return MP4D_NO_ERROR;

}
mp4d_error_t
mp4d_senc_get_next(senc_reader_t *p_r,
                          uint8_t *init_vector,
                          uint8_t iv_size,
                          uint16_t *subsample_count,
                          const uint8_t **p_encryp_info)
{
    ASSURE( p_r != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( ((iv_size == 8)||(iv_size == 16)),    MP4D_E_WRONG_ARGUMENT, ("illegal iv size") );
    ASSURE( init_vector != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( subsample_count != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( p_encryp_info != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );

    while(iv_size --) 
    {
        *init_vector++ = mp4d_read_u8(&p_r->buffer);
    }
    if (p_r->flags == 2)
    {
        uint16_t temp;
        temp = *subsample_count = mp4d_read_u16(&p_r->buffer);
        *p_encryp_info = p_r->buffer.p_data;
        while( temp-- )
        {
            mp4d_read_u16(&p_r->buffer);
            mp4d_read_u32(&p_r->buffer);
        }
    }
    else
    {
        *subsample_count = 0;
        *p_encryp_info = 0;
    }
    p_r->next_sample_index++;

    return MP4D_NO_ERROR;
}

/* end senc */

/* begin padb */
mp4d_error_t
mp4d_padb_init(padb_reader_t *p_r, mp4d_atom_t *p_padb)
{
    ASSURE( p_r != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( p_padb != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );

    p_r->buffer = mp4d_atom_to_buffer(p_padb);

    {
        uint8_t version = mp4d_read_u8(&p_r->buffer);
        uint32_t flags = mp4d_read_u24(&p_r->buffer);
    
        ASSURE( version == 0, MP4D_E_UNSUPPRTED_FORMAT, ("Unknown padb version %" PRIu32, version) );
        ASSURE( flags == 0, MP4D_E_UNSUPPRTED_FORMAT, ("Unknown padb flags %" PRIu32, flags) );
    }

    p_r->sample_count = mp4d_read_u32(&p_r->buffer);
    p_r->next_sample_index = 0;

    return MP4D_NO_ERROR;
}

mp4d_error_t
mp4d_padb_get_next(padb_reader_t *p_r, uint8_t *p_entry)
{
    ASSURE( p_r != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( p_entry != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( p_r->buffer.p_data != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") ); /* not initialized */

    ASSURE( p_r->next_sample_index < p_r->sample_count, MP4D_E_NEXT_SEGMENT,
            ("Out of padb samples (count = %" PRIu32 ")", p_r->sample_count) );

    if ((p_r->next_sample_index & 1) == 0)
    {
        p_r->current_entry = mp4d_read_u8(&p_r->buffer);
        *p_entry = (p_r->current_entry >> 4) & 0x7;
    }
    else
    {
        *p_entry = p_r->current_entry & 0x7;
    }
    p_r->next_sample_index++;

    return MP4D_NO_ERROR;
}

/* end padb */

/* begin subs */
mp4d_error_t
mp4d_subs_init(subs_reader_t * p_r,
               mp4d_atom_t *p_atom
    )
{
    ASSURE( p_r != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );

    if (p_atom == NULL)
    {
        p_r->buffer.p_data = NULL;
        return MP4D_NO_ERROR;
    }

    p_r->buffer = mp4d_atom_to_buffer(p_atom);
    p_r->version = mp4d_read_u8(&p_r->buffer);
    ASSURE( p_r->version == 0 || p_r->version == 1, MP4D_E_UNSUPPRTED_FORMAT, ("Unknown subs version %" PRIu32, p_r->version) );
    {
        uint32_t flags = mp4d_read_u24(&p_r->buffer);
        ASSURE( flags == 0, MP4D_E_UNSUPPRTED_FORMAT, ("Unknown subs flags %" PRIu32, flags) );
    }

    p_r->entries_left = mp4d_read_u32(&p_r->buffer);
    p_r->subsamples_left = 0;
    p_r->next_sample_index = 0;
    p_r->next_entry_sample_number = 0;
    p_r->next_entry_subsample_count = 0;

    return MP4D_NO_ERROR;
}

mp4d_error_t
mp4d_subs_get_next_count(subs_reader_t *p_r,
                         uint16_t *p_count
    )
{
    ASSURE( p_r != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( p_count != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );

    p_r->next_sample_index++;  /* now equals current requested sample number */
    p_r->current_offset = 0;

    if (p_r->buffer.p_data == NULL)
    {
        p_r->subsamples_left = 0;
        *p_count = 1;
        return MP4D_NO_ERROR;
    }

    if (p_r->next_sample_index > p_r->next_entry_sample_number && p_r->entries_left > 0)
    {
        uint32_t sample_delta;

        /* Move past any subsamples which were not consumed by get_next_size() */
        while (p_r->subsamples_left > 0)
        {
            if (p_r->version == 1)
            {
                mp4d_read_u32(&p_r->buffer);
            }
            else
            {
                mp4d_read_u16(&p_r->buffer);
            }
            mp4d_read_u8(&p_r->buffer);
            mp4d_read_u8(&p_r->buffer);
            mp4d_read_u32(&p_r->buffer);
            p_r->subsamples_left--;
        }

        sample_delta = mp4d_read_u32(&p_r->buffer);       
        ASSURE( sample_delta > 0, MP4D_E_INVALID_ATOM,
                ("sample_delta is zero" ) );

        p_r->next_entry_sample_number += sample_delta;
        p_r->next_entry_subsample_count = mp4d_read_u16(&p_r->buffer);
        p_r->entries_left--;
    }

    if (p_r->next_sample_index < p_r->next_entry_sample_number)
    {
        p_r->subsamples_left = 0;
        *p_count = 1;
        return MP4D_NO_ERROR;
    }
    else if (p_r->next_sample_index == p_r->next_entry_sample_number)
    {
        if (p_r->next_entry_subsample_count == 0)
        {
            *p_count = 1;
        }
        else
        {
            *p_count = p_r->next_entry_subsample_count;
        }
        p_r->subsamples_left = p_r->next_entry_subsample_count;
        return MP4D_NO_ERROR;
    }
    else
    {
        p_r->subsamples_left = 0;
        *p_count = 1;
        return MP4D_NO_ERROR;
    }
}

mp4d_error_t
mp4d_subs_get_next_size(subs_reader_t *p_r,
                        uint32_t sample_size,
                        uint32_t *p_size,
                        uint32_t *p_offset
    )
{
    ASSURE( p_r != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( p_size != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( p_offset != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( p_r->current_offset < sample_size, MP4D_E_WRONG_ARGUMENT,
            ("No more subsamples (offset = %" PRIu32 ", sample_size = %" PRIu32 ")",
             p_r->current_offset, sample_size) );

    if (p_r->buffer.p_data == NULL || p_r->subsamples_left == 0)
    {
        *p_size = sample_size;
        *p_offset = p_r->current_offset;
        p_r->current_offset += *p_size;
        return MP4D_NO_ERROR;
    }

    if (p_r->version == 1)
    {
        *p_size = mp4d_read_u32(&p_r->buffer);
    }
    else
    {
        *p_size = mp4d_read_u16(&p_r->buffer);
    }
    mp4d_read_u8(&p_r->buffer);  /* subsample_priority */
    mp4d_read_u8(&p_r->buffer);  /* discardable */
    mp4d_read_u32(&p_r->buffer); /* reserved */

    *p_offset = p_r->current_offset;
    p_r->current_offset += *p_size;
    p_r->subsamples_left--;

    return MP4D_NO_ERROR;
}
/* end subs */

/* begin saiz */
mp4d_error_t
mp4d_saiz_init(saiz_reader_t *p_r,
               mp4d_atom_t *p_atom)
{
    uint32_t flags;

    ASSURE( p_r != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( p_atom != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );

    p_r->buffer = mp4d_atom_to_buffer(p_atom);
    {
        uint8_t version = mp4d_read_u8(&p_r->buffer);
        flags = mp4d_read_u24(&p_r->buffer);
        ASSURE( version == 0, MP4D_E_UNSUPPRTED_FORMAT, ("Unknown saiz version %" PRIu32, version) );
    }
    if (flags & 1)
    {
        p_r->aux_info_type = mp4d_read_u32(&p_r->buffer);
        mp4d_read_u32(&p_r->buffer); /* aux_info_type_parameter */
    }
    else
    {
        warning(("saiz.flags & 0 = 0, assuming common encryption"));
        /* See comment in saio_init() */
        p_r->aux_info_type = CENC;
    }

    p_r->default_sample_info_size = mp4d_read_u8(&p_r->buffer);
    p_r->samples_left = mp4d_read_u32(&p_r->buffer);

    return MP4D_NO_ERROR;
}

mp4d_error_t
mp4d_saiz_get_next_size(saiz_reader_t *p_r,
                        uint8_t *p_size
    )
{
    ASSURE( p_r != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( p_size != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );

    if (p_r->samples_left == 0)
    {
        *p_size = 0;
        return MP4D_NO_ERROR;
        
    }

    p_r->samples_left --;

    if (p_r->default_sample_info_size > 0)
    {
        *p_size = p_r->default_sample_info_size; 
    }
    else
    {
        *p_size = mp4d_read_u8(&p_r->buffer);
    }

    return MP4D_NO_ERROR;
}
/* end saiz */

/* begin saio */
mp4d_error_t
mp4d_saio_init(saio_reader_t *p_r,
               mp4d_atom_t *p_atom
    )
{
    uint32_t flags;

    ASSURE( p_r != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( p_atom != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );

    p_r->buffer = mp4d_atom_to_buffer(p_atom);
    {
        p_r->version = mp4d_read_u8(&p_r->buffer);
        flags = mp4d_read_u24(&p_r->buffer);
        ASSURE( p_r->version == 0 || p_r->version == 1, MP4D_E_UNSUPPRTED_FORMAT, ("Unknown saio version %" PRIu32, p_r->version) );
    }
    if (flags & 1)
    {
        p_r->aux_info_type = mp4d_read_u32(&p_r->buffer);
        mp4d_read_u32(&p_r->buffer); /* aux_info_type_parameter */
    }
    else
    {
        warning(("saio.flags & 0 = 0, assuming common encryption"));
        /* In this case, the scheme should be taken from the 'schm' box.
         * However commen enryption is the only scheme, this demuxer supports.
         */
        p_r->aux_info_type = CENC;
    }

    p_r->entries_left = mp4d_read_u32(&p_r->buffer);

    return MP4D_NO_ERROR;
}

mp4d_error_t
mp4d_saio_get_next(saio_reader_t *p_r,
                   uint64_t current_offset,
                   uint64_t *p_offset)
{
    ASSURE( p_r != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( p_offset != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );

    if (p_r->entries_left > 0)
    {
        if (p_r->version == 0)
        {
            *p_offset = mp4d_read_u32(&p_r->buffer);
        }
        else
        {
            *p_offset = mp4d_read_u64(&p_r->buffer);
        }
        p_r->entries_left--;
    }
    else
    {
        *p_offset = current_offset;
    }

    return MP4D_NO_ERROR;
}
/* end saio */
