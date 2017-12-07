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
 * @file
 * @brief Track reader unit tests
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "mp4d_trackreader.h"
#include "mp4d_box_read.h"

#include "mp4d_unittest.h"

#define expect(expr)                                                 \
do {                                                                 \
    int expect_expr = (expr);                                        \
                                                                     \
    TEST_UPDATE(!expect_expr, nfailed, ntests);                      \
    if (!expect_expr)                                                \
    {                                                                \
        fprintf(stderr, "Expect failed: %d: %s\n", __LINE__, #expr); \
    }                                                                \
} while(0)

/* API to write an an atom */
typedef struct
{
    unsigned char *p_data;
    size_t size;
    size_t alloc;
} buffer_t;

static
void buffer_init(buffer_t *buffer)
{
    buffer->p_data = NULL;
    buffer->size = 0;
    buffer->alloc = 0;
}

static
void buffer_grow(buffer_t *buffer, size_t size)
{
    buffer->size += size;
    while (buffer->size > buffer->alloc)
    {
        buffer->alloc *= 2;
        buffer->alloc += 1;
        buffer->p_data = realloc(buffer->p_data, buffer->alloc);
    }
}
static
void write_u64(buffer_t *buffer, uint64_t u)
{
    buffer_grow(buffer, sizeof(u));

    buffer->p_data[buffer->size - 1] = (u>>0) & 0xFF;
    buffer->p_data[buffer->size - 2] = (u>>8) & 0xFF;
    buffer->p_data[buffer->size - 3] = (u>>16) & 0xFF;
    buffer->p_data[buffer->size - 4] = (u>>24) & 0xFF;
    buffer->p_data[buffer->size - 5] = (u>>32) & 0xFF;
    buffer->p_data[buffer->size - 6] = (u>>40) & 0xFF;
    buffer->p_data[buffer->size - 7] = (u>>48) & 0xFF;
    buffer->p_data[buffer->size - 8] = (u>>56) & 0xFF;
}

static
void write_u32(buffer_t *buffer, uint32_t u)
{
    buffer_grow(buffer, sizeof(u));

    buffer->p_data[buffer->size - 1] = (u>>0) & 0xFF;
    buffer->p_data[buffer->size - 2] = (u>>8) & 0xFF;
    buffer->p_data[buffer->size - 3] = (u>>16) & 0xFF;
    buffer->p_data[buffer->size - 4] = (u>>24) & 0xFF;
}

static
void write_u24(buffer_t *buffer, uint32_t u)
{
    buffer_grow(buffer, 3);

    buffer->p_data[buffer->size - 1] = (u>>0) & 0xFF;
    buffer->p_data[buffer->size - 2] = (u>>8) & 0xFF;
    buffer->p_data[buffer->size - 3] = (u>>16) & 0xFF;
}

static
void write_u16(buffer_t *buffer, uint16_t u)
{
    buffer_grow(buffer, sizeof(u));

    buffer->p_data[buffer->size - 1] = (u>>0) & 0xFF;
    buffer->p_data[buffer->size - 2] = (u>>8) & 0xFF;
}

static
void write_u8(buffer_t *buffer, uint8_t u)
{
    buffer_grow(buffer, sizeof(u));

    buffer->p_data[buffer->size - 1] = u;
}

static
void write_32(buffer_t *buffer, int32_t i)
{
    write_u32(buffer, i);
}

static
void write_16(buffer_t *buffer, int16_t i)
{
    write_u16(buffer, i);
}

mp4d_atom_t
wrap_buffer(const buffer_t *buffer)
{
    mp4d_atom_t atom;
    memset(&atom, 0, sizeof(atom));

    atom.p_data = buffer->p_data;
    atom.size = buffer->size;
    
    return atom;
}

static int nfailed = 0;
static int ntests = 0;

static void
test_tts_not_init(void)
{
    tts_reader_t r;
    uint64_t dts;
    uint32_t dur;

    memset(&r, 0, sizeof(r));
    expect( mp4d_tts_get_ts(&r, 0, &dts, &dur) == MP4D_E_WRONG_ARGUMENT );
}

static void
test_tts_bad_version(void)
{
    buffer_t tts;
    buffer_init(&tts);

    write_u8(&tts, 1);  /* version */
    write_u24(&tts, 0);  /* flags */
    write_u32(&tts, 0);  /* entry count */

    {
        tts_reader_t r;
        mp4d_atom_t atom = wrap_buffer(&tts);

        expect( mp4d_tts_init(&r, &atom, 1) == MP4D_E_UNSUPPRTED_FORMAT );
    }

    free(tts.p_data);
}

static void
test_tts_empty(void)
{
    buffer_t tts;
    buffer_init(&tts);

    write_u8(&tts, 0);  /* version */
    write_u24(&tts, 0);  /* flags */
    write_u32(&tts, 0);  /* entry count */

    {
        tts_reader_t r;
        uint64_t dts; 
        uint32_t dur;
        mp4d_atom_t atom = wrap_buffer(&tts);
        expect( mp4d_tts_init(&r, &atom, 1) == MP4D_NO_ERROR );    
        expect( mp4d_tts_get_ts(&r, 0, &dts, &dur) == MP4D_E_NEXT_SEGMENT );
        expect( mp4d_tts_get_ts(&r, 1, &dts, &dur) == MP4D_E_NEXT_SEGMENT );
        expect( mp4d_tts_get_ts(&r, 2, &dts, &dur) == MP4D_E_NEXT_SEGMENT );
    }

    free(tts.p_data);
}

static void
test_tts_one_sample(void)
{
    buffer_t tts;
    buffer_init(&tts);

    write_u8(&tts, 0);  /* version */
    write_u24(&tts, 0);  /* flags */
    write_u32(&tts, 1);  /* entry count */
    write_u32(&tts, 1);  /* sample count */
    write_u32(&tts, 10); /* sample delta */

    {
        tts_reader_t r;
        uint64_t dts; 
        uint32_t dur;
        mp4d_atom_t atom = wrap_buffer(&tts);
        expect( mp4d_tts_init(&r, &atom, 1) == MP4D_NO_ERROR );    
        expect( mp4d_tts_get_ts(&r, 0, &dts, &dur) == MP4D_NO_ERROR );
        expect( dts == 0 );
        expect( mp4d_tts_get_ts(&r, 1, &dts, &dur) == MP4D_E_NEXT_SEGMENT );
        expect( mp4d_tts_get_ts(&r, 2, &dts, &dur) == MP4D_E_NEXT_SEGMENT );
        expect( mp4d_tts_get_ts(&r, 93, &dts, &dur) == MP4D_E_NEXT_SEGMENT );
    }

    free(tts.p_data);
}

static void
test_tts_one_entry(void)
{
    buffer_t tts;
    buffer_init(&tts);

    write_u8(&tts, 0);  /* version */
    write_u24(&tts, 0);  /* flags */
    write_u32(&tts, 1);  /* entry count */
    write_u32(&tts, 3);  /* sample count */
    write_u32(&tts, 10); /* sample delta */

    {
        tts_reader_t r;
        uint64_t dts; 
        uint32_t dur;
        mp4d_atom_t atom = wrap_buffer(&tts);
        expect( mp4d_tts_init(&r, &atom, 1) == MP4D_NO_ERROR );    
        expect( mp4d_tts_get_ts(&r, 0, &dts, &dur) == MP4D_NO_ERROR ); expect( dts == 0 );
        expect( mp4d_tts_get_ts(&r, 1, &dts, &dur) == MP4D_NO_ERROR ); expect( dts == 10 );
        expect( mp4d_tts_get_ts(&r, 2, &dts, &dur) == MP4D_NO_ERROR ); expect( dts == 20 );
        expect( mp4d_tts_get_ts(&r, 3, &dts, &dur) == MP4D_E_NEXT_SEGMENT );
        expect( mp4d_tts_get_ts(&r, 93, &dts, &dur) == MP4D_E_NEXT_SEGMENT );
    }

    free(tts.p_data);
}

static void
test_tts_multiple_entries(void)
{
    buffer_t tts;
    buffer_init(&tts);

    write_u8(&tts, 0);  /* version */
    write_u24(&tts, 0);  /* flags */
    write_u32(&tts, 7);  /* entry count */

    write_u32(&tts, 3);  /* sample count */
    write_u32(&tts, 10); /* sample delta */

    write_u32(&tts, 2);  /* sample count */
    write_u32(&tts, 3);  /* sample delta */

    write_u32(&tts, 1);  /* sample count */
    write_u32(&tts, 300);/* sample delta */

    write_u32(&tts, 0);  /* sample count (no samples) */
    write_u32(&tts, 1);  /* sample delta */

    write_u32(&tts, 0);  /* sample count */
    write_u32(&tts, 12); /* sample delta */

    write_u32(&tts, 2);  /* sample count */
    write_u32(&tts, 1);  /* sample delta */

    write_u32(&tts, 1);  /* sample count */
    write_u32(&tts, 0);  /* sample delta */

    {
        tts_reader_t r;
        uint64_t dts; 
        uint32_t dur;
        mp4d_atom_t atom = wrap_buffer(&tts);
        expect( mp4d_tts_init(&r, &atom, 1) == MP4D_NO_ERROR );    
        expect( mp4d_tts_get_ts(&r, 0, &dts, &dur) == MP4D_NO_ERROR ); expect( dts == 0 );
        expect( mp4d_tts_get_ts(&r, 1, &dts, &dur) == MP4D_NO_ERROR ); expect( dts == 10 );
        expect( mp4d_tts_get_ts(&r, 2, &dts, &dur) == MP4D_NO_ERROR ); expect( dts == 20 );

        expect( mp4d_tts_get_ts(&r, 3, &dts, &dur) == MP4D_NO_ERROR ); expect( dts == 30 );
        expect( mp4d_tts_get_ts(&r, 4, &dts, &dur) == MP4D_NO_ERROR ); expect( dts == 33 );

        expect( mp4d_tts_get_ts(&r, 5, &dts, &dur) == MP4D_NO_ERROR ); expect( dts == 36 );

        expect( mp4d_tts_get_ts(&r, 6, &dts, &dur) == MP4D_NO_ERROR ); expect( dts == 336 );
        expect( mp4d_tts_get_ts(&r, 7, &dts, &dur) == MP4D_NO_ERROR ); expect( dts == 337 );

        expect( mp4d_tts_get_ts(&r, 8, &dts, &dur) == MP4D_NO_ERROR ); expect( dts == 338 );

        expect( mp4d_tts_get_ts(&r, 9, &dts, &dur) == MP4D_E_NEXT_SEGMENT );
        expect( mp4d_tts_get_ts(&r, 93, &dts, &dur) == MP4D_E_NEXT_SEGMENT );
    }

    free(tts.p_data);
}

/** @brief exercise mp4d_tts_get_stts_next()
 */
static void
test_tts_multiple_entries_stts_next(void)
{
    buffer_t tts;
    buffer_init(&tts);

    write_u8(&tts, 0);  /* version */
    write_u24(&tts, 0);  /* flags */
    write_u32(&tts, 7);  /* entry count */

    write_u32(&tts, 3);  /* sample count */
    write_u32(&tts, 10); /* sample delta */

    write_u32(&tts, 2);  /* sample count */
    write_u32(&tts, 3);  /* sample delta */

    write_u32(&tts, 1);  /* sample count */
    write_u32(&tts, 300);/* sample delta */

    write_u32(&tts, 0);  /* sample count (no samples) */
    write_u32(&tts, 1);  /* sample delta */

    write_u32(&tts, 0);  /* sample count */
    write_u32(&tts, 12); /* sample delta */

    write_u32(&tts, 2);  /* sample count */
    write_u32(&tts, 1);  /* sample delta */

    write_u32(&tts, 1);  /* sample count */
    write_u32(&tts, 0);  /* sample delta */

    {
        tts_reader_t r;
        uint64_t dts; 
        uint32_t dur;
        mp4d_atom_t atom = wrap_buffer(&tts);
        expect( mp4d_tts_init(&r, &atom, 1) == MP4D_NO_ERROR );    
        expect( mp4d_tts_get_stts_next(&r, &dts, &dur) == MP4D_NO_ERROR ); expect( dts == 0 );
        expect( mp4d_tts_get_stts_next(&r, &dts, &dur) == MP4D_NO_ERROR ); expect( dts == 10 );
        expect( mp4d_tts_get_stts_next(&r, &dts, &dur) == MP4D_NO_ERROR ); expect( dts == 20 );

        expect( mp4d_tts_get_stts_next(&r, &dts, &dur) == MP4D_NO_ERROR ); expect( dts == 30 );
        expect( mp4d_tts_get_stts_next(&r, &dts, &dur) == MP4D_NO_ERROR ); expect( dts == 33 );

        expect( mp4d_tts_get_stts_next(&r, &dts, &dur) == MP4D_NO_ERROR ); expect( dts == 36 );

        expect( mp4d_tts_get_stts_next(&r, &dts, &dur) == MP4D_NO_ERROR ); expect( dts == 336 );
        expect( mp4d_tts_get_stts_next(&r, &dts, &dur) == MP4D_NO_ERROR ); expect( dts == 337 );

        expect( mp4d_tts_get_stts_next(&r, &dts, &dur) == MP4D_NO_ERROR ); expect( dts == 338 );

        expect( mp4d_tts_get_stts_next(&r, &dts, &dur) == MP4D_E_NEXT_SEGMENT );
        expect( mp4d_tts_get_stts_next(&r, &dts, &dur) == MP4D_E_NEXT_SEGMENT );
        expect( mp4d_tts_get_stts_next(&r, &dts, &dur) == MP4D_E_NEXT_SEGMENT );
        expect( mp4d_tts_get_stts_next(&r, &dts, &dur) == MP4D_E_NEXT_SEGMENT );
    }

    free(tts.p_data);
}

/** @brief exercise mp4d_tts_get_ctts_next()
 */
static void
test_tts_multiple_entries_ctts_next(void)
{
    buffer_t tts;
    buffer_init(&tts);

    write_u8(&tts, 0);  /* version */
    write_u24(&tts, 0);  /* flags */
    write_u32(&tts, 7);  /* entry count */

    write_u32(&tts, 3);  /* sample count */
    write_u32(&tts, 10); /* sample value */

    write_u32(&tts, 2);  /* sample count */
    write_u32(&tts, 3);  /* sample value */

    write_u32(&tts, 1);  /* sample count */
    write_u32(&tts, 300);/* sample value */

    write_u32(&tts, 0);  /* sample count (no samples) */
    write_u32(&tts, 1);  /* sample value */

    write_u32(&tts, 0);  /* sample count */
    write_u32(&tts, 12); /* sample value */

    write_u32(&tts, 2);  /* sample count */
    write_u32(&tts, 1);  /* sample value */

    write_u32(&tts, 1);  /* sample count */
    write_u32(&tts, 0);  /* sample value */

    {
        tts_reader_t r;
        uint32_t cts;
        mp4d_atom_t atom = wrap_buffer(&tts);

        expect( mp4d_tts_init(&r, &atom, 0) == MP4D_NO_ERROR );    
        expect( mp4d_tts_get_ctts_next(&r, &cts) == MP4D_NO_ERROR ); expect( cts == 10 );
        expect( mp4d_tts_get_ctts_next(&r, &cts) == MP4D_NO_ERROR ); expect( cts == 10 );
        expect( mp4d_tts_get_ctts_next(&r, &cts) == MP4D_NO_ERROR ); expect( cts == 10 );

        expect( mp4d_tts_get_ctts_next(&r, &cts) == MP4D_NO_ERROR ); expect( cts == 3 );
        expect( mp4d_tts_get_ctts_next(&r, &cts) == MP4D_NO_ERROR ); expect( cts == 3 );

        expect( mp4d_tts_get_ctts_next(&r, &cts) == MP4D_NO_ERROR ); expect( cts == 300 );

        expect( mp4d_tts_get_ctts_next(&r, &cts) == MP4D_NO_ERROR ); expect( cts == 1 );
        expect( mp4d_tts_get_ctts_next(&r, &cts) == MP4D_NO_ERROR ); expect( cts == 1 );

        expect( mp4d_tts_get_ctts_next(&r, &cts) == MP4D_NO_ERROR ); expect( cts == 0 );

        expect( mp4d_tts_get_ctts_next(&r, &cts) == MP4D_E_NEXT_SEGMENT );
        expect( mp4d_tts_get_ctts_next(&r, &cts) == MP4D_E_NEXT_SEGMENT );
        expect( mp4d_tts_get_ctts_next(&r, &cts) == MP4D_E_NEXT_SEGMENT );
        expect( mp4d_tts_get_ctts_next(&r, &cts) == MP4D_E_NEXT_SEGMENT );
    }

    free(tts.p_data);
}

/** @brief exercise _get_ts in combination with _next()
 */
static void
test_tts_seek_next(void)
{
    struct
    {
        uint64_t ts;
        uint32_t duration;
    } dts[] = {{0, 10},
               {10, 10},
               {20, 10},
               {30, 3},
               {33, 3},
               {36, 300},
               {336, 1},
               {337, 1},
               {338, 0}};
    
    buffer_t tts;

    buffer_init(&tts);

    write_u8(&tts, 0);  /* version */
    write_u24(&tts, 0);  /* flags */
    write_u32(&tts, 7);  /* entry count */

    write_u32(&tts, 3);  /* sample count */
    write_u32(&tts, 10); /* sample value */

    write_u32(&tts, 2);  /* sample count */
    write_u32(&tts, 3);  /* sample value */

    write_u32(&tts, 1);  /* sample count */
    write_u32(&tts, 300);/* sample value */

    write_u32(&tts, 0);  /* sample count (no samples) */
    write_u32(&tts, 1);  /* sample value */

    write_u32(&tts, 0);  /* sample count */
    write_u32(&tts, 12); /* sample value */

    write_u32(&tts, 2);  /* sample count */
    write_u32(&tts, 1);  /* sample value */

    write_u32(&tts, 1);  /* sample count */
    write_u32(&tts, 0);  /* sample value */

    {
        tts_reader_t r;
        uint64_t ts;
        uint32_t duration;
        mp4d_atom_t atom = wrap_buffer(&tts);
        uint32_t i, from, to;
        uint32_t n = sizeof(dts) / sizeof(*dts);
        
        expect( mp4d_tts_init(&r, &atom, 1) == MP4D_NO_ERROR );    

        for (from = 0; from < n; from++)
        {
            for (to = 0; to <= n; to++)
            {
                expect( mp4d_tts_get_ts(&r, from, &ts, &duration) == MP4D_NO_ERROR );
                expect( ts == dts[from].ts );
                expect( duration == dts[from].duration );

                if (to == n)
                {
                    expect( mp4d_tts_get_ts(&r, to, &ts, &duration) == MP4D_E_NEXT_SEGMENT);
                    continue;
                }
                expect( mp4d_tts_get_ts(&r, to, &ts, &duration) == MP4D_NO_ERROR );
                expect( ts == dts[to].ts );
                expect( duration == dts[to].duration );

                for (i = to + 1; i < n; i++)
                {
                    expect( mp4d_tts_get_stts_next(&r, &ts, &duration) == MP4D_NO_ERROR ); 
                    expect( ts == dts[i].ts );
                    expect( duration == dts[i].duration );
                }
                expect( mp4d_tts_get_stts_next(&r, &ts, &duration) == MP4D_E_NEXT_SEGMENT );
            }
        }
    }

    free(tts.p_data);
}

static void
test_tts_first_empty(void)
{
    buffer_t tts;
    buffer_init(&tts);

    write_u8(&tts, 0);  /* version */
    write_u24(&tts, 0);  /* flags */
    write_u32(&tts, 3);  /* entry count */

    write_u32(&tts, 0);  /* sample count */
    write_u32(&tts, 10); /* sample delta */

    write_u32(&tts, 0);  /* sample count */
    write_u32(&tts, 3);  /* sample delta */

    write_u32(&tts, 2);  /* sample count */
    write_u32(&tts, 300);/* sample delta */

    {
        tts_reader_t r;
        uint64_t dts; 
        uint32_t dur;
        mp4d_atom_t atom = wrap_buffer(&tts);
        expect( mp4d_tts_init(&r, &atom, 1) == MP4D_NO_ERROR );    
        expect( mp4d_tts_get_ts(&r, 0, &dts, &dur) == MP4D_NO_ERROR ); expect( dts == 0 );
        expect( mp4d_tts_get_ts(&r, 1, &dts, &dur) == MP4D_NO_ERROR ); expect( dts == 300 );
        expect( mp4d_tts_get_ts(&r, 2, &dts, &dur) == MP4D_E_NEXT_SEGMENT );
    }

    free(tts.p_data);
}


static void
test_tts_seek(void)
{
    buffer_t tts;
    buffer_init(&tts);

    write_u8(&tts, 0);  /* version */
    write_u24(&tts, 0);  /* flags */
    write_u32(&tts, 2);  /* entry count */

    write_u32(&tts, 3);  /* sample count */
    write_u32(&tts, 10); /* sample delta */

    write_u32(&tts, 4);  /* sample count */
    write_u32(&tts, 1);  /* sample delta */

    {
        tts_reader_t r;
        uint64_t dts; 
        uint32_t dur;
        mp4d_atom_t atom = wrap_buffer(&tts);
        expect( mp4d_tts_init(&r, &atom, 1) == MP4D_NO_ERROR );    
        expect( mp4d_tts_get_ts(&r, 6, &dts, &dur) == MP4D_NO_ERROR ); expect( dts == 33 && dur == 1);
        expect( mp4d_tts_get_ts(&r, 1, &dts, &dur) == MP4D_NO_ERROR ); expect( dts == 10 && dur == 10);
        expect( mp4d_tts_get_ts(&r, 2, &dts, &dur) == MP4D_NO_ERROR ); expect( dts == 20 && dur == 10);
        expect( mp4d_tts_get_ts(&r, 2, &dts, &dur) == MP4D_NO_ERROR ); expect( dts == 20 && dur == 10);
        expect( mp4d_tts_get_ts(&r, 2, &dts, &dur) == MP4D_NO_ERROR ); expect( dts == 20 && dur == 10);
        expect( mp4d_tts_get_ts(&r, 3, &dts, &dur) == MP4D_NO_ERROR ); expect( dts == 30 && dur == 1);
        expect( mp4d_tts_get_ts(&r, 7, &dts, &dur) == MP4D_E_NEXT_SEGMENT );
        expect( mp4d_tts_get_ts(&r, 5, &dts, &dur) == MP4D_NO_ERROR ); expect( dts == 32 && dur == 1);
        expect( mp4d_tts_get_ts(&r, 7, &dts, &dur) == MP4D_E_NEXT_SEGMENT );
        expect( mp4d_tts_get_ts(&r, 3, &dts, &dur) == MP4D_NO_ERROR ); expect( dts == 30 && dur == 1);
        expect( mp4d_tts_get_ts(&r, 4, &dts, &dur) == MP4D_NO_ERROR ); expect( dts == 31 && dur == 1);
        expect( mp4d_tts_get_ts(&r, 7, &dts, &dur) == MP4D_E_NEXT_SEGMENT );
        expect( mp4d_tts_get_ts(&r, 0, &dts, &dur) == MP4D_NO_ERROR ); expect( dts == 0 && dur == 10);
        expect( mp4d_tts_get_ts(&r, 6, &dts, &dur) == MP4D_NO_ERROR ); expect( dts == 33 && dur == 1);

        expect( mp4d_tts_get_ts(&r, 7, &dts, &dur) == MP4D_E_NEXT_SEGMENT );
    }

    free(tts.p_data);
}

/**
   @brief ctts box parsing
*/
static void
test_tts_seek_nodelta(void)
{
    buffer_t tts;
    buffer_init(&tts);

    write_u8(&tts, 0);  /* version */
    write_u24(&tts, 0);  /* flags */
    write_u32(&tts, 2);  /* entry count */

    write_u32(&tts, 3);  /* sample count */
    write_u32(&tts, 10); /* sample value */

    write_u32(&tts, 4);  /* sample count */
    write_u32(&tts, 1);  /* sample value */

    {
        tts_reader_t r;
        uint64_t cts;
        mp4d_atom_t atom = wrap_buffer(&tts);
        expect( mp4d_tts_init(&r, &atom, 0) == MP4D_NO_ERROR );
        expect( mp4d_tts_get_ts(&r, 6, &cts, NULL) == MP4D_NO_ERROR ); expect( cts == 1 );
        expect( mp4d_tts_get_ts(&r, 1, &cts, NULL) == MP4D_NO_ERROR ); expect( cts == 10 );
        expect( mp4d_tts_get_ts(&r, 2, &cts, NULL) == MP4D_NO_ERROR ); expect( cts == 10 );
        expect( mp4d_tts_get_ts(&r, 2, &cts, NULL) == MP4D_NO_ERROR ); expect( cts == 10 );
        expect( mp4d_tts_get_ts(&r, 2, &cts, NULL) == MP4D_NO_ERROR ); expect( cts == 10 );
        expect( mp4d_tts_get_ts(&r, 3, &cts, NULL) == MP4D_NO_ERROR ); expect( cts == 1 );
        expect( mp4d_tts_get_ts(&r, 7, &cts, NULL) == MP4D_E_NEXT_SEGMENT );
        expect( mp4d_tts_get_ts(&r, 5, &cts, NULL) == MP4D_NO_ERROR ); expect( cts == 1 );
        expect( mp4d_tts_get_ts(&r, 7, &cts, NULL) == MP4D_E_NEXT_SEGMENT );
        expect( mp4d_tts_get_ts(&r, 3, &cts, NULL) == MP4D_NO_ERROR ); expect( cts == 1 );
        expect( mp4d_tts_get_ts(&r, 4, &cts, NULL) == MP4D_NO_ERROR ); expect( cts == 1 );
        expect( mp4d_tts_get_ts(&r, 7, &cts, NULL) == MP4D_E_NEXT_SEGMENT );
        expect( mp4d_tts_get_ts(&r, 0, &cts, NULL) == MP4D_NO_ERROR ); expect( cts == 10 );
        expect( mp4d_tts_get_ts(&r, 6, &cts, NULL) == MP4D_NO_ERROR ); expect( cts == 1 );

        expect( mp4d_tts_get_ts(&r, 7, &cts, NULL) == MP4D_E_NEXT_SEGMENT );
    }

    free(tts.p_data);
}

static void
test_stsz_not_init(void)
{
    stsz_reader_t r;
    uint32_t size;

    memset(&r, 0, sizeof(r));
    expect( mp4d_stsz_get_next(&r, &size) == MP4D_E_WRONG_ARGUMENT );
}

/**
   Test already initialized box reader against array of expected values.
 */
static void
stsz_test(stsz_reader_t *p_r, uint32_t *sizes, uint32_t n)
{
    uint64_t from, to, i;
    uint32_t size;

    for (from = 0; from < n; from++)
    {
        expect( mp4d_stsz_get_next(p_r, &size) == MP4D_NO_ERROR );
        expect( size = sizes[from] );
    }
    expect( mp4d_stsz_get_next(p_r, &size) == MP4D_E_NEXT_SEGMENT );
    expect( mp4d_stsz_get_next(p_r, &size) == MP4D_E_NEXT_SEGMENT );

    for (from = 0; from <= n; from++)
    {
        for (to = 0; to <= n; to++)
        {
            if (from < n)
            {
                expect( mp4d_stsz_get(p_r, from, &size) == MP4D_NO_ERROR );
                expect( size = sizes[from] );
            }
            else
            {
                expect( mp4d_stsz_get(p_r, from, &size) == MP4D_E_NEXT_SEGMENT );
            }

            if (to < n)
            {
                expect( mp4d_stsz_get(p_r, to, &size) == MP4D_NO_ERROR );
                expect( size = sizes[to] );
            }
            else
            {
                expect( mp4d_stsz_get(p_r, to, &size) == MP4D_E_NEXT_SEGMENT );
            }
        
            for (i = to + 1; i < n; i++)
            {
                expect( mp4d_stsz_get_next(p_r, &size) == MP4D_NO_ERROR );
                expect( size = sizes[i] );
            }
            expect( mp4d_stsz_get_next(p_r, &size) == MP4D_E_NEXT_SEGMENT );
        }
    }
}

static void
test_stsz_s0_empty(void)
{
    buffer_t stsz;
    buffer_init(&stsz);

    write_u8(&stsz, 0);  /* version */
    write_u24(&stsz, 0);  /* flags */
    write_u32(&stsz, 1234);  /* sample_size */

    write_u32(&stsz, 0);  /* sample count */

    {
        stsz_reader_t r;
        mp4d_atom_t atom = wrap_buffer(&stsz);

        expect( mp4d_stsz_init(&r, &atom, 0) == MP4D_NO_ERROR );

        stsz_test(&r, NULL, 0);
    }

    free(stsz.p_data);
}

static void
test_stsz_s0_single(void)
{
    buffer_t stsz;
    buffer_init(&stsz);

    write_u8(&stsz, 0);  /* version */
    write_u24(&stsz, 0);  /* flags */
    write_u32(&stsz, 1234);  /* sample_size */

    write_u32(&stsz, 1);  /* sample count */

    {
        stsz_reader_t r;
        uint32_t sizes [] = {1234};
        mp4d_atom_t atom = wrap_buffer(&stsz);

        expect( mp4d_stsz_init(&r, &atom, 0) == MP4D_NO_ERROR );

        stsz_test(&r, sizes, 1);
    }
    free(stsz.p_data);
}

static void
test_stsz_s0_multiple(void)
{
    buffer_t stsz;
    buffer_init(&stsz);

    write_u8(&stsz, 0);  /* version */
    write_u24(&stsz, 0);  /* flags */
    write_u32(&stsz, 1234);  /* sample_size */

    write_u32(&stsz, 3);  /* sample count */

    {
        stsz_reader_t r;
        uint32_t sizes [] = {1234, 1234, 1234};
        mp4d_atom_t atom = wrap_buffer(&stsz);

        expect( mp4d_stsz_init(&r, &atom, 0) == MP4D_NO_ERROR );

        stsz_test(&r, sizes, sizeof(sizes) / sizeof(*sizes));
    }

    free(stsz.p_data);
}

static void
test_stsz_s1_empty(void)
{
    buffer_t stsz;
    buffer_init(&stsz);

    write_u8(&stsz, 0);  /* version */
    write_u24(&stsz, 0);  /* flags */
    write_u32(&stsz, 0);  /* sample_size */

    write_u32(&stsz, 0);  /* sample count */

    {
        stsz_reader_t r;
        mp4d_atom_t atom = wrap_buffer(&stsz);

        expect( mp4d_stsz_init(&r, &atom, 0) == MP4D_NO_ERROR );

        stsz_test(&r, NULL, 0);
    }
    free(stsz.p_data);
}

static void
test_stsz_s1_single(void)
{
    buffer_t stsz;
    buffer_init(&stsz);

    write_u8(&stsz, 0);  /* version */
    write_u24(&stsz, 0);  /* flags */
    write_u32(&stsz, 0);  /* sample_size */

    write_u32(&stsz, 1);  /* sample count */

    write_u32(&stsz, 31);  /* size */

    {
        stsz_reader_t r;
        mp4d_atom_t atom = wrap_buffer(&stsz);
        uint32_t sizes [] = {31};

        expect( mp4d_stsz_init(&r, &atom, 0) == MP4D_NO_ERROR );

        stsz_test(&r, sizes, sizeof(sizes) / sizeof(*sizes));
    }
    free(stsz.p_data);
}

static void
test_stsz_s1_multiple(void)
{
    buffer_t stsz;
    buffer_init(&stsz);

    write_u8(&stsz, 0);  /* version */
    write_u24(&stsz, 0);  /* flags */
    write_u32(&stsz, 0);  /* sample_size */

    write_u32(&stsz, 3);  /* sample count */

    write_u32(&stsz, 31);  /* size */
    write_u32(&stsz, 30);  /* size */
    write_u32(&stsz, 240);  /* size */

    {
        stsz_reader_t r;
        mp4d_atom_t atom = wrap_buffer(&stsz);
        uint32_t sizes [] = {31, 30, 240};

        expect( mp4d_stsz_init(&r, &atom, 0) == MP4D_NO_ERROR );

        stsz_test(&r, sizes, sizeof(sizes) / sizeof(*sizes));
    }
    free(stsz.p_data);
}

static void
test_stz2_invalid(void)
{
    buffer_t stsz;
    buffer_init(&stsz);

    write_u8(&stsz, 0);  /* version */
    write_u24(&stsz, 0);  /* flags */
    write_u24(&stsz, 0);  /* reserved */
    write_u8(&stsz, 1);   /* field size */
    write_u32(&stsz, 5);  /* sample count */

    write_u8(&stsz, 34);

    {
        stsz_reader_t r;
        mp4d_atom_t atom = wrap_buffer(&stsz);

        expect( mp4d_stsz_init(&r, &atom, 1) == MP4D_E_UNSUPPRTED_FORMAT );
    }
    free(stsz.p_data);
}

static void
test_stz2_4(void)
{
    buffer_t stsz;
    buffer_init(&stsz);

    write_u8(&stsz, 0);  /* version */
    write_u24(&stsz, 0);  /* flags */
    write_u24(&stsz, 0);  /* reserved */
    write_u8(&stsz, 4);   /* field size */

    write_u32(&stsz, 5);  /* sample count */

    write_u8(&stsz, (11 << 4) + 12);  /* size */
    write_u8(&stsz, (13 << 4) + 4);  /* size */
    write_u8(&stsz, 6 << 4);       /* size */
    {
        stsz_reader_t r;
        mp4d_atom_t atom = wrap_buffer(&stsz);
        uint32_t sizes [] = {11, 12, 13, 4, 6};

        expect( mp4d_stsz_init(&r, &atom, 1) == MP4D_NO_ERROR );

        stsz_test(&r, sizes, sizeof(sizes) / sizeof(*sizes));
    }
    free(stsz.p_data);
}

static void
test_stz2_8(void)
{
    buffer_t stsz;
    buffer_init(&stsz);

    write_u8(&stsz, 0);  /* version */
    write_u24(&stsz, 0);  /* flags */
    write_u24(&stsz, 0);  /* reserved */
    write_u8(&stsz, 8);   /* field size */

    write_u32(&stsz, 3);  /* sample count */

    write_u8(&stsz, 12);  /* size */
    write_u8(&stsz, 14);  /* size */
    write_u8(&stsz, 250); /* size */
    {
        stsz_reader_t r;
        mp4d_atom_t atom = wrap_buffer(&stsz);
        uint32_t sizes [] = {12, 14, 250};

        expect( mp4d_stsz_init(&r, &atom, 1) == MP4D_NO_ERROR );

        stsz_test(&r, sizes, sizeof(sizes) / sizeof(*sizes));
    }
    free(stsz.p_data);
}

static void
test_stz2_16(void)
{
    buffer_t stsz;
    buffer_init(&stsz);

    write_u8(&stsz, 0);  /* version */
    write_u24(&stsz, 0);  /* flags */
    write_u24(&stsz, 0);  /* reserved */
    write_u8(&stsz, 16);   /* field size */

    write_u32(&stsz, 3);  /* sample count */

    write_u16(&stsz, 12);  /* size */
    write_u16(&stsz, 14);  /* size */
    write_u16(&stsz, 25053);       /* size */
    {
        stsz_reader_t r;
        mp4d_atom_t atom = wrap_buffer(&stsz);
        uint32_t sizes [] = {12, 14, 25053};

        expect( mp4d_stsz_init(&r, &atom, 1) == MP4D_NO_ERROR );

        stsz_test(&r, sizes, sizeof(sizes) / sizeof(*sizes));
    }
    free(stsz.p_data);
}

static void
test_stsc_not_init(void)
{
    stsc_reader_t r;
    uint32_t chunk_index;
    uint32_t sdi;
    uint32_t sample_index;

    memset(&r, 0, sizeof(r));
    expect( mp4d_stsc_get_next(&r, &chunk_index, &sdi, &sample_index) == MP4D_E_WRONG_ARGUMENT );
}

static void
test_stsc_empty(void)
{
    buffer_t stsc;
    buffer_init(&stsc);

    write_u8(&stsc, 0);  /* version */
    write_u24(&stsc, 0);  /* flags */
    write_u32(&stsc, 0);  /* entry count */

    {
        stsc_reader_t r;
        uint32_t ci, sdi, si;
        mp4d_atom_t atom = wrap_buffer(&stsc);

        expect( mp4d_stsc_init(&r, &atom) == MP4D_NO_ERROR );
        expect( mp4d_stsc_get_next(&r, &ci, &sdi, &si) == MP4D_E_NEXT_SEGMENT );
    }
    free(stsc.p_data);
}

static void
test_stsc_empty_entries(void)
{
    buffer_t stsc;
    buffer_init(&stsc);

    write_u8(&stsc, 0);  /* version */
    write_u24(&stsc, 0);  /* flags */
    write_u32(&stsc, 3);  /* entry count */

    write_u32(&stsc, 1);  /* first chunk */
    write_u32(&stsc, 0);  /* samples per chunk */
    write_u32(&stsc, 3);  /* sample description index */

    write_u32(&stsc, 12); /* first chunk */
    write_u32(&stsc, 99); /* samples per chunk */
    write_u32(&stsc, 3);  /* sample description index */

    write_u32(&stsc, 12); /* first chunk */
    write_u32(&stsc, 0);  /* samples per chunk */
    write_u32(&stsc, 3);  /* sample description index */

    write_u32(&stsc, 20);  /* first chunk */
    write_u32(&stsc, 0);  /* samples per chunk */
    write_u32(&stsc, 3);  /* sample description index */

    {
        stsc_reader_t r;
        uint32_t ci, sdi, si;
        mp4d_atom_t atom = wrap_buffer(&stsc);

        expect( mp4d_stsc_init(&r, &atom) == MP4D_NO_ERROR );
        expect( mp4d_stsc_get_next(&r, &ci, &sdi, &si) == MP4D_E_NEXT_SEGMENT );
    }
    free(stsc.p_data);
}

static void
test_stsc_one_entry_1(void)
{
    buffer_t stsc;
    buffer_init(&stsc);

    write_u8(&stsc, 0);  /* version */
    write_u24(&stsc, 0);  /* flags */
    write_u32(&stsc, 1);  /* entry count */

    write_u32(&stsc, 1);  /* first chunk */
    write_u32(&stsc, 1);  /* samples per chunk */
    write_u32(&stsc, 3);  /* sample description index */

    {
        stsc_reader_t r;
        uint32_t ci, sdi, si;
        mp4d_atom_t atom = wrap_buffer(&stsc);

        expect( mp4d_stsc_init(&r, &atom) == MP4D_NO_ERROR );
        expect( mp4d_stsc_get_next(&r, &ci, &sdi, &si) == MP4D_NO_ERROR ); expect( ci == 1 && sdi == 3 && si == 0 );
        expect( mp4d_stsc_get_next(&r, &ci, &sdi, &si) == MP4D_NO_ERROR ); expect( ci == 2 && sdi == 3 && si == 0 );
        expect( mp4d_stsc_get_next(&r, &ci, &sdi, &si) == MP4D_NO_ERROR ); expect( ci == 3 && sdi == 3 && si == 0 );
        expect( mp4d_stsc_get_next(&r, &ci, &sdi, &si) == MP4D_NO_ERROR ); expect( ci == 4 && sdi == 3 && si == 0 );
    }
    free(stsc.p_data);
}

static void
test_stsc_one_entry_2(void)
{
    buffer_t stsc;
    buffer_init(&stsc);

    write_u8(&stsc, 0);  /* version */
    write_u24(&stsc, 0);  /* flags */
    write_u32(&stsc, 1);  /* entry count */

    write_u32(&stsc, 1);  /* first chunk */
    write_u32(&stsc, 3);  /* samples per chunk */
    write_u32(&stsc, 7);  /* sample description index */

    {
        stsc_reader_t r;
        uint32_t ci, sdi, si;
        mp4d_atom_t atom = wrap_buffer(&stsc);

        expect( mp4d_stsc_init(&r, &atom) == MP4D_NO_ERROR );
        expect( mp4d_stsc_get_next(&r, &ci, &sdi, &si) == MP4D_NO_ERROR ); expect( ci == 1 && sdi == 7 && si == 0 );
        expect( mp4d_stsc_get_next(&r, &ci, &sdi, &si) == MP4D_NO_ERROR ); expect( ci == 1 && sdi == 7 && si == 1 );
        expect( mp4d_stsc_get_next(&r, &ci, &sdi, &si) == MP4D_NO_ERROR ); expect( ci == 1 && sdi == 7 && si == 2 );
        expect( mp4d_stsc_get_next(&r, &ci, &sdi, &si) == MP4D_NO_ERROR ); expect( ci == 2 && sdi == 7 && si == 0 );
        expect( mp4d_stsc_get_next(&r, &ci, &sdi, &si) == MP4D_NO_ERROR ); expect( ci == 2 && sdi == 7 && si == 1 );
        expect( mp4d_stsc_get_next(&r, &ci, &sdi, &si) == MP4D_NO_ERROR ); expect( ci == 2 && sdi == 7 && si == 2 );
        expect( mp4d_stsc_get_next(&r, &ci, &sdi, &si) == MP4D_NO_ERROR ); expect( ci == 3 && sdi == 7 && si == 0 );
    }
    free(stsc.p_data);
}


static void
test_stsc_multiple_entries(void)
{
    buffer_t stsc;
    buffer_init(&stsc);

    write_u8(&stsc, 0);  /* version */
    write_u24(&stsc, 0);  /* flags */
    write_u32(&stsc, 4);  /* entry count */

    write_u32(&stsc, 1);  /* first chunk */
    write_u32(&stsc, 2);  /* samples per chunk */
    write_u32(&stsc, 10);  /* sample description index */

    write_u32(&stsc, 2);  /* first chunk */
    write_u32(&stsc, 1);  /* samples per chunk */
    write_u32(&stsc, 12);  /* sample description index */

    write_u32(&stsc, 4);  /* first chunk */
    write_u32(&stsc, 2);  /* samples per chunk */
    write_u32(&stsc, 15);  /* sample description index */

    write_u32(&stsc, 6);  /* first chunk */
    write_u32(&stsc, 3);  /* samples per chunk */
    write_u32(&stsc, 2);  /* sample description index */

    {
        stsc_reader_t r;
        uint32_t ci, sdi, si;
        mp4d_atom_t atom = wrap_buffer(&stsc);

        expect( mp4d_stsc_init(&r, &atom) == MP4D_NO_ERROR );
        expect( mp4d_stsc_get_next(&r, &ci, &sdi, &si) == MP4D_NO_ERROR ); expect( ci == 1 && sdi == 10 && si == 0 );
        expect( mp4d_stsc_get_next(&r, &ci, &sdi, &si) == MP4D_NO_ERROR ); expect( ci == 1 && sdi == 10 && si == 1 );

        expect( mp4d_stsc_get_next(&r, &ci, &sdi, &si) == MP4D_NO_ERROR ); expect( ci == 2 && sdi == 12 && si == 0 );
        expect( mp4d_stsc_get_next(&r, &ci, &sdi, &si) == MP4D_NO_ERROR ); expect( ci == 3 && sdi == 12 && si == 0 );

        expect( mp4d_stsc_get_next(&r, &ci, &sdi, &si) == MP4D_NO_ERROR ); expect( ci == 4 && sdi == 15 && si == 0 );
        expect( mp4d_stsc_get_next(&r, &ci, &sdi, &si) == MP4D_NO_ERROR ); expect( ci == 4 && sdi == 15 && si == 1 );
        expect( mp4d_stsc_get_next(&r, &ci, &sdi, &si) == MP4D_NO_ERROR ); expect( ci == 5 && sdi == 15 && si == 0 );
        expect( mp4d_stsc_get_next(&r, &ci, &sdi, &si) == MP4D_NO_ERROR ); expect( ci == 5 && sdi == 15 && si == 1 );

        expect( mp4d_stsc_get_next(&r, &ci, &sdi, &si) == MP4D_NO_ERROR ); expect( ci == 6 && sdi == 2 && si == 0 );
        expect( mp4d_stsc_get_next(&r, &ci, &sdi, &si) == MP4D_NO_ERROR ); expect( ci == 6 && sdi == 2 && si == 1 );
        expect( mp4d_stsc_get_next(&r, &ci, &sdi, &si) == MP4D_NO_ERROR ); expect( ci == 6 && sdi == 2 && si == 2 );
        expect( mp4d_stsc_get_next(&r, &ci, &sdi, &si) == MP4D_NO_ERROR ); expect( ci == 7 && sdi == 2 && si == 0 );
    }
    free(stsc.p_data);
}

static void
test_stsc_multiple_with_empty(void)
{
    buffer_t stsc;
    buffer_init(&stsc);

    write_u8(&stsc, 0);  /* version */
    write_u24(&stsc, 0);  /* flags */
    write_u32(&stsc, 4);  /* entry count */

    write_u32(&stsc, 1);  /* first chunk */
    write_u32(&stsc, 1);  /* samples per chunk */
    write_u32(&stsc, 10);  /* sample description index */

    write_u32(&stsc, 1);  /* first chunk */
    write_u32(&stsc, 2);  /* samples per chunk */
    write_u32(&stsc, 12);  /* sample description index */

    write_u32(&stsc, 3);  /* first chunk */
    write_u32(&stsc, 0);  /* samples per chunk */
    write_u32(&stsc, 15);  /* sample description index */

    write_u32(&stsc, 6);  /* first chunk */
    write_u32(&stsc, 1);  /* samples per chunk */
    write_u32(&stsc, 2);  /* sample description index */

    {
        stsc_reader_t r;
        uint32_t ci, sdi, si;
        mp4d_atom_t atom = wrap_buffer(&stsc);

        expect( mp4d_stsc_init(&r, &atom) == MP4D_NO_ERROR );
        expect( mp4d_stsc_get_next(&r, &ci, &sdi, &si) == MP4D_NO_ERROR ); expect( ci == 1 && sdi == 12 && si == 0 );
        expect( mp4d_stsc_get_next(&r, &ci, &sdi, &si) == MP4D_NO_ERROR ); expect( ci == 1 && sdi == 12 && si == 1 );
        expect( mp4d_stsc_get_next(&r, &ci, &sdi, &si) == MP4D_NO_ERROR ); expect( ci == 2 && sdi == 12 && si == 0 );
        expect( mp4d_stsc_get_next(&r, &ci, &sdi, &si) == MP4D_NO_ERROR ); expect( ci == 2 && sdi == 12 && si == 1 );

        expect( mp4d_stsc_get_next(&r, &ci, &sdi, &si) == MP4D_NO_ERROR ); expect( ci == 6 && sdi == 2 && si == 0 );
        expect( mp4d_stsc_get_next(&r, &ci, &sdi, &si) == MP4D_NO_ERROR ); expect( ci == 7 && sdi == 2 && si == 0 );
        expect( mp4d_stsc_get_next(&r, &ci, &sdi, &si) == MP4D_NO_ERROR ); expect( ci == 8 && sdi == 2 && si == 0 );
    }
    free(stsc.p_data);
}

static void
test_stsc_first_chunk_not_ascending(void)
{
    buffer_t stsc;
    buffer_init(&stsc);

    write_u8(&stsc, 0);  /* version */
    write_u24(&stsc, 0);  /* flags */
    write_u32(&stsc, 3);  /* entry count */

    write_u32(&stsc, 1);  /* first chunk */
    write_u32(&stsc, 1);  /* samples per chunk */
    write_u32(&stsc, 10);  /* sample description index */

    write_u32(&stsc, 3);  /* first chunk */
    write_u32(&stsc, 2);  /* samples per chunk */
    write_u32(&stsc, 12);  /* sample description index */

    write_u32(&stsc, 2);  /* first chunk */
    write_u32(&stsc, 0);  /* samples per chunk */
    write_u32(&stsc, 15);  /* sample description index */

    {
        stsc_reader_t r;
        uint32_t ci, sdi, si;
        mp4d_atom_t atom = wrap_buffer(&stsc);

        expect( mp4d_stsc_init(&r, &atom) == MP4D_NO_ERROR );
        expect( mp4d_stsc_get_next(&r, &ci, &sdi, &si) == MP4D_NO_ERROR ); expect( ci == 1 && sdi == 10 && si == 0 );
        expect( mp4d_stsc_get_next(&r, &ci, &sdi, &si) == MP4D_NO_ERROR ); expect( ci == 2 && sdi == 10 && si == 0 );
        expect( mp4d_stsc_get_next(&r, &ci, &sdi, &si) == MP4D_E_UNSUPPRTED_FORMAT );
    }
    free(stsc.p_data);
}

static void
test_co_not_init(void)
{
    co_reader_t r;
    uint64_t chunk_offset;

    memset(&r, 0, sizeof(r));
    expect( mp4d_co_get_next(&r, &chunk_offset) == MP4D_E_WRONG_ARGUMENT );
}



static void
test_stco_empty(void)
{
    buffer_t stco;
    buffer_init(&stco);

    write_u8(&stco, 0);  /* version */
    write_u24(&stco, 0);  /* flags */
    write_u32(&stco, 0);  /* entry count */

    {
        co_reader_t r;
        uint64_t chunk_offset;
        mp4d_atom_t atom = wrap_buffer(&stco);

        expect( mp4d_co_init(&r, &atom, 0) == MP4D_NO_ERROR );
        expect( mp4d_co_get_next(&r, &chunk_offset) == MP4D_E_NEXT_SEGMENT );
    }
    free(stco.p_data);
}

static void
test_stco_single(void)
{
    buffer_t stco;
    buffer_init(&stco);

    write_u8(&stco, 0);  /* version */
    write_u24(&stco, 0);  /* flags */
    write_u32(&stco, 1);  /* entry count */

    write_u32(&stco, 35);

    {
        co_reader_t r;
        uint64_t co;
        mp4d_atom_t atom = wrap_buffer(&stco);

        expect( mp4d_co_init(&r, &atom, 0) == MP4D_NO_ERROR );
        expect( mp4d_co_get_next(&r, &co) == MP4D_NO_ERROR ); expect( co == 35 );
        expect( mp4d_co_get_next(&r, &co) == MP4D_E_NEXT_SEGMENT );
    }
    free(stco.p_data);
}

static void
test_stco_multiple(void)
{
    buffer_t stco;
    buffer_init(&stco);

    write_u8(&stco, 0);  /* version */
    write_u24(&stco, 0);  /* flags */
    write_u32(&stco, 3);  /* entry count */

    write_u32(&stco, 35);
    write_u32(&stco, 39);
    write_u32(&stco, 38);

    {
        co_reader_t r;
        uint64_t co;
        mp4d_atom_t atom = wrap_buffer(&stco);

        expect( mp4d_co_init(&r, &atom, 0) == MP4D_NO_ERROR );
        expect( mp4d_co_get_next(&r, &co) == MP4D_NO_ERROR ); expect( co == 35 );
        expect( mp4d_co_get_next(&r, &co) == MP4D_NO_ERROR ); expect( co == 39 );
        expect( mp4d_co_get_next(&r, &co) == MP4D_NO_ERROR ); expect( co == 38 );
        expect( mp4d_co_get_next(&r, &co) == MP4D_E_NEXT_SEGMENT );
        expect( mp4d_co_get_next(&r, &co) == MP4D_E_NEXT_SEGMENT );
    }
    free(stco.p_data);
}

static void
test_co64_multiple(void)
{
    buffer_t co64;
    buffer_init(&co64);

    write_u8(&co64, 0);  /* version */
    write_u24(&co64, 0);  /* flags */
    write_u32(&co64, 3);  /* entry count */

    write_u64(&co64, 9123123123);
    write_u64(&co64, 9123123124);
    write_u64(&co64, 38);

    {
        co_reader_t r;
        uint64_t co;
        mp4d_atom_t atom = wrap_buffer(&co64);

        expect( mp4d_co_init(&r, &atom, 1) == MP4D_NO_ERROR );
        expect( mp4d_co_get_next(&r, &co) == MP4D_NO_ERROR ); expect( co == 9123123123 );
        expect( mp4d_co_get_next(&r, &co) == MP4D_NO_ERROR ); expect( co == 9123123124 );
        expect( mp4d_co_get_next(&r, &co) == MP4D_NO_ERROR ); expect( co == 38 );
        expect( mp4d_co_get_next(&r, &co) == MP4D_E_NEXT_SEGMENT );
    }
    free(co64.p_data);
}

static void
test_stss_no_box(void)
{
    {
        stss_reader_t r;
        int is_sync;

        expect( mp4d_stss_init(&r, NULL) == MP4D_NO_ERROR );
        expect( mp4d_stss_get_next(&r, &is_sync) == MP4D_NO_ERROR ); expect( is_sync );
        expect( mp4d_stss_get_next(&r, &is_sync) == MP4D_NO_ERROR ); expect( is_sync );
        expect( mp4d_stss_get_next(&r, &is_sync) == MP4D_NO_ERROR ); expect( is_sync );
        expect( mp4d_stss_get_next(&r, &is_sync) == MP4D_NO_ERROR ); expect( is_sync );
    }
}

static void
test_stss_illegal_order(void)
{
    buffer_t stss;
    buffer_init(&stss);

    write_u8(&stss, 0); /* version */
    write_u24(&stss, 0); /* flags */
    write_u32(&stss, 3); /* entry count */

    write_u32(&stss, 2); 
    write_u32(&stss, 3); 
    write_u32(&stss, 3); 

    {
        stss_reader_t r;
        int is_sync;
        mp4d_atom_t atom = wrap_buffer(&stss);

        expect( mp4d_stss_init(&r, &atom) == MP4D_NO_ERROR );
        expect( mp4d_stss_get_next(&r, &is_sync) == MP4D_NO_ERROR ); expect( !is_sync );
        expect( mp4d_stss_get_next(&r, &is_sync) == MP4D_NO_ERROR ); expect( is_sync );
        expect( mp4d_stss_get_next(&r, &is_sync) == MP4D_NO_ERROR ); expect( is_sync );
        expect( mp4d_stss_get_next(&r, &is_sync) == MP4D_E_UNSUPPRTED_FORMAT );
    }

    free(stss.p_data);
}

static void
test_stss_empty(void)
{
    buffer_t stss;
    buffer_init(&stss);

    write_u8(&stss, 0); /* version */
    write_u24(&stss, 0); /* flags */
    write_u32(&stss, 0); /* entry count */

    {
        stss_reader_t r;
        int is_sync;
        mp4d_atom_t atom = wrap_buffer(&stss);

        expect( mp4d_stss_init(&r, &atom) == MP4D_NO_ERROR );
        expect( mp4d_stss_get_next(&r, &is_sync) == MP4D_NO_ERROR ); expect( !is_sync );
        expect( mp4d_stss_get_next(&r, &is_sync) == MP4D_NO_ERROR ); expect( !is_sync );
        expect( mp4d_stss_get_next(&r, &is_sync) == MP4D_NO_ERROR ); expect( !is_sync );
        expect( mp4d_stss_get_next(&r, &is_sync) == MP4D_NO_ERROR ); expect( !is_sync );
    }

    free(stss.p_data);
}

static void
test_stss_single_first(void)
{
    buffer_t stss;
    buffer_init(&stss);

    write_u8(&stss, 0); /* version */
    write_u24(&stss, 0); /* flags */
    write_u32(&stss, 1); /* entry count */

    write_u32(&stss, 1); /* first sample is sync */

    {
        stss_reader_t r;
        int is_sync;
        mp4d_atom_t atom = wrap_buffer(&stss);

        expect( mp4d_stss_init(&r, &atom) == MP4D_NO_ERROR );
        expect( mp4d_stss_get_next(&r, &is_sync) == MP4D_NO_ERROR ); expect( is_sync );
        expect( mp4d_stss_get_next(&r, &is_sync) == MP4D_NO_ERROR ); expect( !is_sync );
        expect( mp4d_stss_get_next(&r, &is_sync) == MP4D_NO_ERROR ); expect( !is_sync );
        expect( mp4d_stss_get_next(&r, &is_sync) == MP4D_NO_ERROR ); expect( !is_sync );
    }

    free(stss.p_data);
}

static void
test_stss_single_not_first(void)
{
    buffer_t stss;
    buffer_init(&stss);

    write_u8(&stss, 0); /* version */
    write_u24(&stss, 0); /* flags */
    write_u32(&stss, 1); /* entry count */

    write_u32(&stss, 3); /* third sample is sync */

    {
        stss_reader_t r;
        int is_sync;
        mp4d_atom_t atom = wrap_buffer(&stss);

        expect( mp4d_stss_init(&r, &atom) == MP4D_NO_ERROR );
        expect( mp4d_stss_get_next(&r, &is_sync) == MP4D_NO_ERROR ); expect( !is_sync );
        expect( mp4d_stss_get_next(&r, &is_sync) == MP4D_NO_ERROR ); expect( !is_sync );
        expect( mp4d_stss_get_next(&r, &is_sync) == MP4D_NO_ERROR ); expect( is_sync );
        expect( mp4d_stss_get_next(&r, &is_sync) == MP4D_NO_ERROR ); expect( !is_sync );
    }

    free(stss.p_data);
}


static void
test_stss_multiple(void)
{
    buffer_t stss;
    buffer_init(&stss);

    write_u8(&stss, 0); /* version */
    write_u24(&stss, 0); /* flags */
    write_u32(&stss, 3); /* entry count */

    write_u32(&stss, 2); 
    write_u32(&stss, 3); 
    write_u32(&stss, 5); 

    {
        stss_reader_t r;
        int is_sync;
        mp4d_atom_t atom = wrap_buffer(&stss);

        expect( mp4d_stss_init(&r, &atom) == MP4D_NO_ERROR );
        expect( mp4d_stss_get_next(&r, &is_sync) == MP4D_NO_ERROR ); expect( !is_sync );
        expect( mp4d_stss_get_next(&r, &is_sync) == MP4D_NO_ERROR ); expect( is_sync );
        expect( mp4d_stss_get_next(&r, &is_sync) == MP4D_NO_ERROR ); expect( is_sync );
        expect( mp4d_stss_get_next(&r, &is_sync) == MP4D_NO_ERROR ); expect( !is_sync );
        expect( mp4d_stss_get_next(&r, &is_sync) == MP4D_NO_ERROR ); expect( is_sync );
        expect( mp4d_stss_get_next(&r, &is_sync) == MP4D_NO_ERROR ); expect( !is_sync );
        expect( mp4d_stss_get_next(&r, &is_sync) == MP4D_NO_ERROR ); expect( !is_sync );
    }

    free(stss.p_data);
}

static void
test_stss_all(void)
{
    buffer_t stss;
    buffer_init(&stss);

    write_u8(&stss, 0); /* version */
    write_u24(&stss, 0); /* flags */
    write_u32(&stss, 4); /* entry count */

    write_u32(&stss, 1); 
    write_u32(&stss, 2); 
    write_u32(&stss, 3); 
    write_u32(&stss, 4); 

    {
        stss_reader_t r;
        int is_sync;
        mp4d_atom_t atom = wrap_buffer(&stss);

        expect( mp4d_stss_init(&r, &atom) == MP4D_NO_ERROR );
        expect( mp4d_stss_get_next(&r, &is_sync) == MP4D_NO_ERROR ); expect( is_sync );
        expect( mp4d_stss_get_next(&r, &is_sync) == MP4D_NO_ERROR ); expect( is_sync );
        expect( mp4d_stss_get_next(&r, &is_sync) == MP4D_NO_ERROR ); expect( is_sync );
        expect( mp4d_stss_get_next(&r, &is_sync) == MP4D_NO_ERROR ); expect( is_sync );
        expect( mp4d_stss_get_next(&r, &is_sync) == MP4D_NO_ERROR ); expect( !is_sync );
        expect( mp4d_stss_get_next(&r, &is_sync) == MP4D_NO_ERROR ); expect( !is_sync );
    }

    free(stss.p_data);
}

static void
test_elst_empty(void)
{
    buffer_t elst;
    buffer_init(&elst);

    write_u8(&elst, 0); /* version */
    write_u24(&elst, 0); /* flags */
    write_u32(&elst, 0); /* entry count */
    {
        elst_reader_t r;
        mp4d_atom_t atom = wrap_buffer(&elst);
        int64_t pt;
        uint32_t off, dur;

        expect( mp4d_elst_init(&r, &atom, 1, 1) == MP4D_NO_ERROR );
        expect( mp4d_elst_get_presentation_time(&r, 0, 1, &pt, &off, &dur) == MP4D_E_INFO_NOT_AVAIL );
    }

    free(elst.p_data);
}

static void
test_elst_one_entry_1(void)
{
    buffer_t elst;
    buffer_init(&elst);

    write_u8(&elst, 0); /* version */
    write_u24(&elst, 0); /* flags */
    write_u32(&elst, 1); /* entry count */

    write_u32(&elst, 10); /* segment duration */
    write_32(&elst, 0);  /* media_time */
    write_16(&elst, 1);   /* media_rate */
    write_16(&elst, 0);   /* 0 */
    {
        elst_reader_t r;
        mp4d_atom_t atom = wrap_buffer(&elst);
        int64_t pt;
        uint32_t off, dur;

        expect( mp4d_elst_init(&r, &atom, 1, 1) == MP4D_NO_ERROR );
        expect( mp4d_elst_get_presentation_time(&r, 0, 1, &pt, &off, &dur) == MP4D_NO_ERROR ); expect( pt == 0 && off == 0 && dur == 1 );
        expect( mp4d_elst_get_presentation_time(&r, 0, 10, &pt, &off, &dur) == MP4D_NO_ERROR ); expect( pt == 0 && off == 0 && dur == 10 );
        expect( mp4d_elst_get_presentation_time(&r, 0, 11, &pt, &off, &dur) == MP4D_NO_ERROR ); expect( pt == 0 && off == 0 && dur == 10 );
        expect( mp4d_elst_get_presentation_time(&r, 2, 1, &pt, &off, &dur) == MP4D_NO_ERROR ); expect( pt == 2 && off == 0 && dur == 1 );
        expect( mp4d_elst_get_presentation_time(&r, 2, 7, &pt, &off, &dur) == MP4D_NO_ERROR ); expect( pt == 2 && off == 0 && dur == 7 );
        expect( mp4d_elst_get_presentation_time(&r, 2, 10, &pt, &off, &dur) == MP4D_NO_ERROR ); expect( pt == 2 && off == 0 && dur == 8 );
        expect( mp4d_elst_get_presentation_time(&r, 10, 1, &pt, &off, &dur) == MP4D_E_INFO_NOT_AVAIL );
    }

    free(elst.p_data);
}
static void
test_elst_one_entry_2(void)
{
    buffer_t elst;
    buffer_init(&elst);

    write_u8(&elst, 0); /* version */
    write_u24(&elst, 0); /* flags */
    write_u32(&elst, 1); /* entry count */

    write_u32(&elst, 18000); /* segment duration */
    write_32(&elst, 1000);  /* media_time */
    write_16(&elst, 1);   /* media_rate */
    write_16(&elst, 0);   /* 0 */
    {
        elst_reader_t r;
        mp4d_atom_t atom = wrap_buffer(&elst);
        int64_t pt;
        uint32_t off, dur;

        expect( mp4d_elst_init(&r, &atom, 1, 1) == MP4D_NO_ERROR );
        expect( mp4d_elst_get_presentation_time(&r, 1000, 1000, &pt, &off, &dur) == MP4D_NO_ERROR );
        expect( mp4d_elst_get_presentation_time(&r, 9000, 1000, &pt, &off, &dur) == MP4D_NO_ERROR );
        expect( mp4d_elst_get_presentation_time(&r, 17000, 1000, &pt, &off, &dur) == MP4D_NO_ERROR );
        expect( mp4d_elst_get_presentation_time(&r, 17998, 1000, &pt, &off, &dur) == MP4D_NO_ERROR );
        expect( mp4d_elst_get_presentation_time(&r, 17999, 1000, &pt, &off, &dur) == MP4D_NO_ERROR );
    }

    free(elst.p_data);
}

static void
test_elst_one_entry_offset(void)
{
    buffer_t elst;
    buffer_init(&elst);

    write_u8(&elst, 0); /* version */
    write_u24(&elst, 0); /* flags */
    write_u32(&elst, 1); /* entry count */

    write_u32(&elst, 10); /* segment duration */
    write_32(&elst, 320);  /* media_time */
    write_16(&elst, 1);   /* media_rate */
    write_16(&elst, 0);   /* 0 */
    {
        elst_reader_t r;
        mp4d_atom_t atom = wrap_buffer(&elst);
        int64_t pt;
        uint32_t off, dur;

        expect( mp4d_elst_init(&r, &atom, 1, 1) == MP4D_NO_ERROR );
        expect( mp4d_elst_get_presentation_time(&r, 0, 1, &pt, &off, &dur) == MP4D_E_INFO_NOT_AVAIL );
        expect( mp4d_elst_get_presentation_time(&r, 300, 20, &pt, &off, &dur) == MP4D_E_INFO_NOT_AVAIL );
        expect( mp4d_elst_get_presentation_time(&r, 300, 21, &pt, &off, &dur) == MP4D_NO_ERROR ); expect( pt == -20 && off == 20 && dur == 1 );
        expect( mp4d_elst_get_presentation_time(&r, 300, 25, &pt, &off, &dur) == MP4D_NO_ERROR ); expect( pt == -20 && off == 20 && dur == 5 );
        expect( mp4d_elst_get_presentation_time(&r, 300, 35, &pt, &off, &dur) == MP4D_NO_ERROR ); expect( pt == -20 && off == 20 && dur == 10 );
        expect( mp4d_elst_get_presentation_time(&r, 322, 1, &pt, &off, &dur) == MP4D_NO_ERROR ); expect( pt == 2 && off == 0 && dur == 1 );
        expect( mp4d_elst_get_presentation_time(&r, 322, 10, &pt, &off, &dur) == MP4D_NO_ERROR ); expect( pt == 2 && off == 0 && dur == 8 );
        expect( mp4d_elst_get_presentation_time(&r, 330, 1, &pt, &off, &dur) == MP4D_E_INFO_NOT_AVAIL );
    }

    free(elst.p_data);
}

static void
test_elst_multiple(void)
{
    buffer_t elst;
    buffer_init(&elst);

    write_u8(&elst, 0); /* version */
    write_u24(&elst, 0); /* flags */
    write_u32(&elst, 2); /* entry count */

    write_u32(&elst, 10); /* segment duration */
    write_32(&elst, 321); /* media_time */
    write_16(&elst, 1);   /* media_rate */
    write_16(&elst, 0);   /* 0 */

    write_u32(&elst, 40); /* segment duration */
    write_32(&elst, 500); /* media_time */
    write_16(&elst, 1);   /* media_rate */
    write_16(&elst, 0);   /* 0 */
    {
        elst_reader_t r;
        mp4d_atom_t atom = wrap_buffer(&elst);
        int64_t pt;
        uint32_t off, dur;

        expect( mp4d_elst_init(&r, &atom, 1, 1) == MP4D_NO_ERROR );

        expect( mp4d_elst_get_presentation_time(&r, 0, 15, &pt, &off, &dur) == MP4D_E_INFO_NOT_AVAIL );
        expect( mp4d_elst_get_presentation_time(&r, 320, 15, &pt, &off, &dur) == MP4D_NO_ERROR ); expect( pt == -1 && off == 1 && dur == 10 );
        expect( mp4d_elst_get_presentation_time(&r, 321, 15, &pt, &off, &dur) == MP4D_NO_ERROR ); expect( pt == 0 && off == 0 && dur == 10 );
        expect( mp4d_elst_get_presentation_time(&r, 330, 15, &pt, &off, &dur) == MP4D_NO_ERROR ); expect( pt == 9 && off == 0 && dur == 1 );
        expect( mp4d_elst_get_presentation_time(&r, 331, 15, &pt, &off, &dur) == MP4D_E_INFO_NOT_AVAIL );
        expect( mp4d_elst_get_presentation_time(&r, 499, 15, &pt, &off, &dur) == MP4D_NO_ERROR ); expect( pt == 9 && off == 1 && dur == 14 );
        expect( mp4d_elst_get_presentation_time(&r, 500, 15, &pt, &off, &dur) == MP4D_NO_ERROR ); expect( pt == 10 && off == 0 && dur == 15 );
        expect( mp4d_elst_get_presentation_time(&r, 539, 15, &pt, &off, &dur) == MP4D_NO_ERROR ); expect( pt == 49 && off == 0 && dur == 1 );
        expect( mp4d_elst_get_presentation_time(&r, 500, 15, &pt, &off, &dur) == MP4D_NO_ERROR ); expect( pt == 10 && off == 0 && dur == 15 );
        expect( mp4d_elst_get_presentation_time(&r, 540, 15, &pt, &off, &dur) == MP4D_E_INFO_NOT_AVAIL );
        /* seek */
        expect( mp4d_elst_get_presentation_time(&r, 321, 15, &pt, &off, &dur) == MP4D_NO_ERROR ); expect( pt == 0 && off == 0 && dur == 10 );
        expect( mp4d_elst_get_presentation_time(&r, 320, 15, &pt, &off, &dur) == MP4D_NO_ERROR ); expect( pt == -1 && off == 1 && dur == 10 );
    }

    free(elst.p_data);
}

static void
test_elst_empty_dwell(void)
{
    buffer_t elst;
    buffer_init(&elst);

    write_u8(&elst, 0); /* version */
    write_u24(&elst, 0); /* flags */
    write_u32(&elst, 4); /* entry count */

    write_u32(&elst, 10); /* segment duration (0 - 10) */
    write_32(&elst, 300); /* media_time */
    write_16(&elst, 1);   /* media_rate */
    write_16(&elst, 0);   /* 0 */

    write_u32(&elst, 40); /* segment duration (10 - 50) */
    write_32(&elst, -1);  /* media_time */           /* empty */
    write_16(&elst, 0);   /* media_rate */
    write_16(&elst, 0);   /* 0 */

    write_u32(&elst, 20); /* segment duration (50 - 70) */
    write_32(&elst, 500); /* media_time */
    write_16(&elst, 0);   /* media_rate */           /* dwell */
    write_16(&elst, 0);   /* 0 */

    write_u32(&elst, 10); /* segment duration (70 - 80) */
    write_32(&elst, 510); /* media_time */
    write_16(&elst, 1);   /* media_rate */
    write_16(&elst, 0);   /* 0 */
    {
        elst_reader_t r;
        mp4d_atom_t atom = wrap_buffer(&elst);
        int64_t pt;
        uint32_t off, dur;

        expect( mp4d_elst_init(&r, &atom, 1, 1) == MP4D_NO_ERROR );

        expect( mp4d_elst_get_presentation_time(&r, 300, 5, &pt, &off, &dur) == MP4D_NO_ERROR ); expect( pt == 0 && off == 0 && dur == 5 );
        expect( mp4d_elst_get_presentation_time(&r, 309, 5, &pt, &off, &dur) == MP4D_NO_ERROR ); expect( pt == 9 && off == 0 && dur == 1 );
        expect( mp4d_elst_get_presentation_time(&r, 309, 5, &pt, &off, &dur) == MP4D_NO_ERROR ); expect( pt == 9 && off == 0 && dur == 1 );
        expect( mp4d_elst_get_presentation_time(&r, 309, 5, &pt, &off, &dur) == MP4D_NO_ERROR ); expect( pt == 9 && off == 0 && dur == 1 );
        expect( mp4d_elst_get_presentation_time(&r, 309, 5, &pt, &off, &dur) == MP4D_NO_ERROR ); expect( pt == 9 && off == 0 && dur == 1 );
        expect( mp4d_elst_get_presentation_time(&r, 309, 5, &pt, &off, &dur) == MP4D_NO_ERROR ); expect( pt == 9 && off == 0 && dur == 1 );
        expect( mp4d_elst_get_presentation_time(&r, 309, 5, &pt, &off, &dur) == MP4D_NO_ERROR ); expect( pt == 9 && off == 0 && dur == 1 );
        expect( mp4d_elst_get_presentation_time(&r, 310, 5, &pt, &off, &dur) == MP4D_E_INFO_NOT_AVAIL );
        expect( mp4d_elst_get_presentation_time(&r, 495, 5, &pt, &off, &dur) == MP4D_E_INFO_NOT_AVAIL );
        expect( mp4d_elst_get_presentation_time(&r, 496, 5, &pt, &off, &dur) == MP4D_E_UNSUPPRTED_FORMAT );   /* contains dwell */
        expect( mp4d_elst_get_presentation_time(&r, 500, 5, &pt, &off, &dur) == MP4D_E_UNSUPPRTED_FORMAT );   /* contains dwell */
        expect( mp4d_elst_get_presentation_time(&r, 501, 5, &pt, &off, &dur) == MP4D_E_INFO_NOT_AVAIL );
        expect( mp4d_elst_get_presentation_time(&r, 510, 5, &pt, &off, &dur) == MP4D_NO_ERROR ); expect( pt == 70 && off == 0 && dur == 5 );
        expect( mp4d_elst_get_presentation_time(&r, 515, 5, &pt, &off, &dur) == MP4D_NO_ERROR ); expect( pt == 75 && off == 0 && dur == 5 );
        expect( mp4d_elst_get_presentation_time(&r, 516, 5, &pt, &off, &dur) == MP4D_NO_ERROR ); expect( pt == 76 && off == 0 && dur == 4 );
        expect( mp4d_elst_get_presentation_time(&r, 520, 5, &pt, &off, &dur) == MP4D_E_INFO_NOT_AVAIL );
    }

    free(elst.p_data);
}

static void
test_elst_time_scale(void)
{
    uint32_t movie_time_scale;
    buffer_t elst;
    
    buffer_init(&elst);
    
    write_u8(&elst, 0); /* version */
    write_u24(&elst, 0); /* flags */
    write_u32(&elst, 1); /* entry count */
    
    write_u32(&elst, 10); /* segment duration (movie time scale) */
    write_32(&elst, 320);  /* media_time */
    write_16(&elst, 1);   /* media_rate */
    write_16(&elst, 0);   /* 0 */

    for (movie_time_scale = 1000; movie_time_scale <= 2000; movie_time_scale += 1000)
    {
        int32_t n;
        for (n = 1; n <= 1; n++)
        {
            elst_reader_t r;
            mp4d_atom_t atom = wrap_buffer(&elst);
            int64_t pt;
            uint32_t off, dur;
            uint32_t media_time_scale = n * movie_time_scale;
            
            expect( mp4d_elst_init(&r, &atom, media_time_scale, movie_time_scale) == MP4D_NO_ERROR );
            
            expect( mp4d_elst_get_presentation_time(&r, 0, 1*n, &pt, &off, &dur) == MP4D_E_INFO_NOT_AVAIL );
            expect( mp4d_elst_get_presentation_time(&r, 320 - 20*n, 20*n, &pt, &off, &dur) == MP4D_E_INFO_NOT_AVAIL );
            expect( mp4d_elst_get_presentation_time(&r, 320 - 20*n, 21*n, &pt, &off, &dur) == MP4D_NO_ERROR ); expect( pt == -20*n && (int32_t) off == 20*n && (int32_t) dur == 1*n );
            expect( mp4d_elst_get_presentation_time(&r, 320 - 20*n, 25*n, &pt, &off, &dur) == MP4D_NO_ERROR ); expect( pt == -20*n && (int32_t) off == 20*n && (int32_t) dur == 5*n );
            expect( mp4d_elst_get_presentation_time(&r, 320 - 20*n, 35*n, &pt, &off, &dur) == MP4D_NO_ERROR ); expect( pt == -20*n && (int32_t) off == 20*n && (int32_t) dur == 10*n );
            expect( mp4d_elst_get_presentation_time(&r, 320 + 2*n, 1*n, &pt, &off, &dur) == MP4D_NO_ERROR ); expect( pt == 2*n && off == 0*n && (int32_t) dur == 1*n );
            expect( mp4d_elst_get_presentation_time(&r, 320 + 2*n, 10*n, &pt, &off, &dur) == MP4D_NO_ERROR ); expect( pt == 2*n && off == 0*n && (int32_t) dur == 8*n );
            expect( mp4d_elst_get_presentation_time(&r, 320 + 10*n, 1*n, &pt, &off, &dur) == MP4D_E_INFO_NOT_AVAIL );
        }
    }

    free(elst.p_data);
}

static void
test_subs_no_box(void)
{
    subs_reader_t r;
    uint16_t count;
    uint32_t size, offset;

    expect( mp4d_subs_init(&r, NULL) == MP4D_NO_ERROR );
    expect( mp4d_subs_get_next_count(&r, &count) == MP4D_NO_ERROR ); expect( count == 1 );
    expect( mp4d_subs_get_next_size(&r, 3, &size, &offset) == MP4D_NO_ERROR ); expect( size == 3 && offset == 0 );
    expect( mp4d_subs_get_next_count(&r, &count) == MP4D_NO_ERROR ); expect( count == 1 );
    expect( mp4d_subs_get_next_size(&r, 1, &size, &offset) == MP4D_NO_ERROR ); expect( size == 1 && offset == 0 );
    expect( mp4d_subs_get_next_count(&r, &count) == MP4D_NO_ERROR ); expect( count == 1 );
    expect( mp4d_subs_get_next_size(&r, 20, &size, &offset) == MP4D_NO_ERROR ); expect( size == 20 && offset == 0 );
}

static void
test_subs_empty(void)
{
    buffer_t subs;
    
    buffer_init(&subs);
    
    write_u8(&subs, 0); /* version */
    write_u24(&subs, 0); /* flags */
    write_u32(&subs, 0); /* entry count */
    
    {
        subs_reader_t r;
        uint16_t count;
        uint32_t size, offset;
        mp4d_atom_t atom = wrap_buffer(&subs);
        
        expect( mp4d_subs_init(&r, &atom) == MP4D_NO_ERROR );
        expect( mp4d_subs_get_next_count(&r, &count) == MP4D_NO_ERROR ); expect( count == 1 );
        expect( mp4d_subs_get_next_size(&r, 3, &size, &offset) == MP4D_NO_ERROR ); expect( size == 3 && offset == 0 );
        expect( mp4d_subs_get_next_count(&r, &count) == MP4D_NO_ERROR ); expect( count == 1 );
        expect( mp4d_subs_get_next_size(&r, 1, &size, &offset) == MP4D_NO_ERROR ); expect( size == 1 && offset == 0 );
        expect( mp4d_subs_get_next_count(&r, &count) == MP4D_NO_ERROR ); expect( count == 1 );
        expect( mp4d_subs_get_next_size(&r, 20, &size, &offset) == MP4D_NO_ERROR ); expect( size == 20 && offset == 0 );
    }

    free(subs.p_data);
}

static void
test_subs_one_entry(void)
{
    buffer_t subs;
    
    buffer_init(&subs);
    
    write_u8(&subs, 0); /* version */
    write_u24(&subs, 0); /* flags */
    write_u32(&subs, 1); /* entry count */
    
    write_u32(&subs, 2); /* sample_delta */
    write_u16(&subs, 2); /* subsample_count */

    write_u16(&subs, 300); /* subsample_size */
    write_u16(&subs, 0); write_u32(&subs, 0);   /* not used */
    
    write_u16(&subs, 400); /* subsample_size */
    write_u16(&subs, 0); write_u32(&subs, 0);   /* not used */
    
    {
        subs_reader_t r;
        uint16_t count;
        uint32_t size, offset;
        mp4d_atom_t atom = wrap_buffer(&subs);
        
        expect( mp4d_subs_init(&r, &atom) == MP4D_NO_ERROR );
        expect( mp4d_subs_get_next_count(&r, &count) == MP4D_NO_ERROR ); expect( count == 1 );
        expect( mp4d_subs_get_next_size(&r, 3, &size, &offset) == MP4D_NO_ERROR ); expect( size == 3 && offset == 0 );

        expect( mp4d_subs_get_next_count(&r, &count) == MP4D_NO_ERROR ); expect( count == 2 );
        expect( mp4d_subs_get_next_size(&r, 700, &size, &offset) == MP4D_NO_ERROR ); expect( size == 300 && offset == 0 );
        expect( mp4d_subs_get_next_size(&r, 700, &size, &offset) == MP4D_NO_ERROR ); expect( size == 400 && offset == 300 );

        expect( mp4d_subs_get_next_count(&r, &count) == MP4D_NO_ERROR ); expect( count == 1 );
        expect( mp4d_subs_get_next_size(&r, 900, &size, &offset) == MP4D_NO_ERROR ); expect( size == 900 && offset == 0 );

        expect( mp4d_subs_get_next_count(&r, &count) == MP4D_NO_ERROR ); expect( count == 1 );
        expect( mp4d_subs_get_next_size(&r, 1000, &size, &offset) == MP4D_NO_ERROR ); expect( size == 1000 && offset == 0 );
    }

    free(subs.p_data);
}

static void
test_subs_first_sample(void)
{
    buffer_t subs;
    
    buffer_init(&subs);
    
    write_u8(&subs, 0); /* version */
    write_u24(&subs, 0); /* flags */
    write_u32(&subs, 1); /* entry count */
    
    write_u32(&subs, 1); /* sample_delta */
    write_u16(&subs, 2); /* subsample_count */

    write_u16(&subs, 300); /* subsample_size */
    write_u16(&subs, 0); write_u32(&subs, 0);   /* not used */
    
    write_u16(&subs, 400); /* subsample_size */
    write_u16(&subs, 0); write_u32(&subs, 0);   /* not used */
    
    {
        subs_reader_t r;
        uint16_t count;
        uint32_t size, offset;
        mp4d_atom_t atom = wrap_buffer(&subs);
        
        expect( mp4d_subs_init(&r, &atom) == MP4D_NO_ERROR );
        expect( mp4d_subs_get_next_count(&r, &count) == MP4D_NO_ERROR ); expect( count == 2 );
        expect( mp4d_subs_get_next_size(&r, 700, &size, &offset) == MP4D_NO_ERROR ); expect( size == 300 && offset == 0 );
        expect( mp4d_subs_get_next_size(&r, 700, &size, &offset) == MP4D_NO_ERROR ); expect( size == 400 && offset == 300 );

        expect( mp4d_subs_get_next_count(&r, &count) == MP4D_NO_ERROR ); expect( count == 1 );
        expect( mp4d_subs_get_next_size(&r, 900, &size, &offset) == MP4D_NO_ERROR ); expect( size == 900 && offset == 0 );

        expect( mp4d_subs_get_next_count(&r, &count) == MP4D_NO_ERROR ); expect( count == 1 );
        expect( mp4d_subs_get_next_size(&r, 1000, &size, &offset) == MP4D_NO_ERROR ); expect( size == 1000 && offset == 0 );
    }

    free(subs.p_data);
}

static void
test_subs_version_1(void)
{
    buffer_t subs;

    buffer_init(&subs);

    write_u8(&subs, 1); /* version */
    write_u24(&subs, 0); /* flags */
    write_u32(&subs, 1); /* entry count */

    write_u32(&subs, 1); /* sample_delta */
    write_u16(&subs, 2); /* subsample_count */

    write_u32(&subs, 300); /* subsample_size */
    write_u16(&subs, 0); write_u32(&subs, 0);   /* not used */

    write_u32(&subs, 400); /* subsample_size */
    write_u16(&subs, 0); write_u32(&subs, 0);   /* not used */

    {
        subs_reader_t r;
        uint16_t count;
        uint32_t size, offset;
        mp4d_atom_t atom = wrap_buffer(&subs);

        expect( mp4d_subs_init(&r, &atom) == MP4D_NO_ERROR );
        expect( mp4d_subs_get_next_count(&r, &count) == MP4D_NO_ERROR ); expect( count == 2 );
        expect( mp4d_subs_get_next_size(&r, 700, &size, &offset) == MP4D_NO_ERROR ); expect( size == 300 && offset == 0 );
        expect( mp4d_subs_get_next_size(&r, 700, &size, &offset) == MP4D_NO_ERROR ); expect( size == 400 && offset == 300 );

        expect( mp4d_subs_get_next_count(&r, &count) == MP4D_NO_ERROR ); expect( count == 1 );
        expect( mp4d_subs_get_next_size(&r, 900, &size, &offset) == MP4D_NO_ERROR ); expect( size == 900 && offset == 0 );

        expect( mp4d_subs_get_next_count(&r, &count) == MP4D_NO_ERROR ); expect( count == 1 );
        expect( mp4d_subs_get_next_size(&r, 1000, &size, &offset) == MP4D_NO_ERROR ); expect( size == 1000 && offset == 0 );
    }

    free(subs.p_data);
}


static void
test_subs_multiple_entries(void)
{
    buffer_t subs;
    
    buffer_init(&subs);
    
    write_u8(&subs, 0);  /* version */
    write_u24(&subs, 0); /* flags */
    write_u32(&subs, 4); /* entry count */
    
    write_u32(&subs, 2); /* sample_delta */
    write_u16(&subs, 0); /* subsample_count */

    write_u32(&subs, 1); /* sample_delta */
    write_u16(&subs, 2); /* subsample_count */
    write_u16(&subs, 300); /* subsample_size */
    write_u16(&subs, 0); write_u32(&subs, 0);   /* not used */
    write_u16(&subs, 400); /* subsample_size */
    write_u16(&subs, 0); write_u32(&subs, 0);   /* not used */
    
    write_u32(&subs, 2); /* sample_delta */
    write_u16(&subs, 1); /* subsample_count */
    write_u16(&subs, 300); /* subsample_size */
    write_u16(&subs, 0); write_u32(&subs, 0);   /* not used */

    write_u32(&subs, 1); /* sample_delta */
    write_u16(&subs, 2); /* subsample_count */
    write_u16(&subs, 301); /* subsample_size */
    write_u16(&subs, 0); write_u32(&subs, 0);   /* not used */
    write_u16(&subs, 401); /* subsample_size */
    write_u16(&subs, 0); write_u32(&subs, 0);   /* not used */

    {
        subs_reader_t r;
        uint16_t count;
        uint32_t size, offset;
        mp4d_atom_t atom = wrap_buffer(&subs);

        expect( mp4d_subs_init(&r, &atom) == MP4D_NO_ERROR );
        expect( mp4d_subs_get_next_count(&r, &count) == MP4D_NO_ERROR ); expect( count == 1 );
        expect( mp4d_subs_get_next_size(&r, 1000, &size, &offset) == MP4D_NO_ERROR ); expect( size == 1000 && offset == 0 );

        expect( mp4d_subs_get_next_count(&r, &count) == MP4D_NO_ERROR ); expect( count == 1 ); /* returns actual count */
        expect( mp4d_subs_get_next_size(&r, 800, &size, &offset) == MP4D_NO_ERROR ); expect( size == 800 && offset == 0 );

        expect( mp4d_subs_get_next_count(&r, &count) == MP4D_NO_ERROR ); expect( count == 2 );
        expect( mp4d_subs_get_next_size(&r, 700, &size, &offset) == MP4D_NO_ERROR ); expect( size == 300 && offset == 0 );
        expect( mp4d_subs_get_next_size(&r, 700, &size, &offset) == MP4D_NO_ERROR ); expect( size == 400 && offset == 300 );

        expect( mp4d_subs_get_next_count(&r, &count) == MP4D_NO_ERROR ); expect( count == 1 );
        expect( mp4d_subs_get_next_size(&r, 1001, &size, &offset) == MP4D_NO_ERROR ); expect( size == 1001 && offset == 0 );

        expect( mp4d_subs_get_next_count(&r, &count) == MP4D_NO_ERROR ); expect( count == 1 );
        expect( mp4d_subs_get_next_size(&r, 300, &size, &offset) == MP4D_NO_ERROR ); expect( size == 300 && offset == 0 );

        expect( mp4d_subs_get_next_count(&r, &count) == MP4D_NO_ERROR ); expect( count == 2 );
        expect( mp4d_subs_get_next_size(&r, 702, &size, &offset) == MP4D_NO_ERROR ); expect( size == 301 && offset == 0 );
        expect( mp4d_subs_get_next_size(&r, 702, &size, &offset) == MP4D_NO_ERROR ); expect( size == 401 && offset == 301 );

        /* Repeat, but do not read all subsample info */
        expect( mp4d_subs_init(&r, &atom) == MP4D_NO_ERROR );
        expect( mp4d_subs_get_next_count(&r, &count) == MP4D_NO_ERROR ); expect( count == 1 );

        expect( mp4d_subs_get_next_count(&r, &count) == MP4D_NO_ERROR ); expect( count == 1 );

        expect( mp4d_subs_get_next_count(&r, &count) == MP4D_NO_ERROR ); expect( count == 2 );
        expect( mp4d_subs_get_next_size(&r, 700, &size, &offset) == MP4D_NO_ERROR ); expect( size == 300 && offset == 0 );

        expect( mp4d_subs_get_next_count(&r, &count) == MP4D_NO_ERROR ); expect( count == 1 );

        expect( mp4d_subs_get_next_count(&r, &count) == MP4D_NO_ERROR ); expect( count == 1 );

        expect( mp4d_subs_get_next_count(&r, &count) == MP4D_NO_ERROR ); expect( count == 2 );
        expect( mp4d_subs_get_next_size(&r, 702, &size, &offset) == MP4D_NO_ERROR ); expect( size == 301 && offset == 0 );

    }

    free(subs.p_data);
}

static void
test_trik_not_init(void)
{
    trik_reader_t r;
    uint8_t pic_type;
    uint8_t denpendency_level;
    memset(&r, 0, sizeof(r));
    expect( mp4d_trik_get_next(&r, &pic_type, &denpendency_level) == MP4D_E_WRONG_ARGUMENT );
}


static void
test_saiz_flag_0(void)
{
    buffer_t saiz;
    
    buffer_init(&saiz);
    
    write_u8(&saiz, 0); /* version */
    write_u24(&saiz, 0); /* flags */

    write_u8(&saiz, 241); /* default_sample_info_size */
    write_u32(&saiz, 0);  /* sample_count */

    {
        saiz_reader_t r;
        mp4d_atom_t atom = wrap_buffer(&saiz);
        
        expect( mp4d_saiz_init(&r, &atom) == MP4D_NO_ERROR );
    }

    free(saiz.p_data);
}

static void
test_saiz_default_empty(void)
{
    buffer_t saiz;
    
    buffer_init(&saiz);
    
    write_u8(&saiz, 0); /* version */
    write_u24(&saiz, 1); /* flags */

    write_u32(&saiz, 0); /* aux_info_type */
    write_u32(&saiz, 0); /* aux_info_type_parameter */
    
    write_u8(&saiz, 241); /* default_sample_info_size */
    write_u32(&saiz, 0);  /* sample_count */

    {
        saiz_reader_t r;
        uint8_t size;
        mp4d_atom_t atom = wrap_buffer(&saiz);
        
        expect( mp4d_saiz_init(&r, &atom) == MP4D_NO_ERROR );
        expect( mp4d_saiz_get_next_size(&r, &size) == MP4D_NO_ERROR ); expect( size == 0 );
        expect( mp4d_saiz_get_next_size(&r, &size) == MP4D_NO_ERROR ); expect( size == 0 );
        expect( mp4d_saiz_get_next_size(&r, &size) == MP4D_NO_ERROR ); expect( size == 0 );
    }

    free(saiz.p_data);
}
static void
test_saiz_default_multiple(void)
{
    buffer_t saiz;
    
    buffer_init(&saiz);
    
    write_u8(&saiz, 0); /* version */
    write_u24(&saiz, 1); /* flags */

    write_u32(&saiz, 0); /* aux_info_type */
    write_u32(&saiz, 0); /* aux_info_type_parameter */
    
    write_u8(&saiz, 241); /* default_sample_info_size */
    write_u32(&saiz, 2);  /* sample_count */

    {
        saiz_reader_t r;
        uint8_t size;
        mp4d_atom_t atom = wrap_buffer(&saiz);
        
        expect( mp4d_saiz_init(&r, &atom) == MP4D_NO_ERROR );
        expect( mp4d_saiz_get_next_size(&r, &size) == MP4D_NO_ERROR ); expect( size == 241 );
        expect( mp4d_saiz_get_next_size(&r, &size) == MP4D_NO_ERROR ); expect( size == 241 );
        expect( mp4d_saiz_get_next_size(&r, &size) == MP4D_NO_ERROR ); expect( size == 0 );
        expect( mp4d_saiz_get_next_size(&r, &size) == MP4D_NO_ERROR ); expect( size == 0 );
    }

    free(saiz.p_data);
}

static void
test_saiz_empty(void)
{
    buffer_t saiz;
    
    buffer_init(&saiz);
    
    write_u8(&saiz, 0); /* version */
    write_u24(&saiz, 1); /* flags */

    write_u32(&saiz, 0); /* aux_info_type */
    write_u32(&saiz, 0); /* aux_info_type_parameter */
    
    write_u8(&saiz, 0); /* default_sample_info_size */
    write_u32(&saiz, 0);  /* sample_count */

    {
        saiz_reader_t r;
        uint8_t size;
        mp4d_atom_t atom = wrap_buffer(&saiz);
        
        expect( mp4d_saiz_init(&r, &atom) == MP4D_NO_ERROR );
        expect( mp4d_saiz_get_next_size(&r, &size) == MP4D_NO_ERROR ); expect( size == 0 );
        expect( mp4d_saiz_get_next_size(&r, &size) == MP4D_NO_ERROR ); expect( size == 0 );
        expect( mp4d_saiz_get_next_size(&r, &size) == MP4D_NO_ERROR ); expect( size == 0 );
    }

    free(saiz.p_data);
}
static void
test_saiz_multiple(void)
{
    buffer_t saiz;
    
    buffer_init(&saiz);
    
    write_u8(&saiz, 0); /* version */
    write_u24(&saiz, 1); /* flags */

    write_u32(&saiz, 5); /* aux_info_type */
    write_u32(&saiz, 0); /* aux_info_type_parameter */
    
    write_u8(&saiz, 0);  /* default_sample_info_size */
    write_u32(&saiz, 2); /* sample_count */

    write_u8(&saiz, 133); /* sample_info_size */
    write_u8(&saiz, 120); /* sample_info_size */

    {
        saiz_reader_t r;
        uint8_t size;
        mp4d_atom_t atom = wrap_buffer(&saiz);
        
        expect( mp4d_saiz_init(&r, &atom) == MP4D_NO_ERROR );
        expect( r.aux_info_type == 5 );
        expect( mp4d_saiz_get_next_size(&r, &size) == MP4D_NO_ERROR ); expect( size == 133 );
        expect( mp4d_saiz_get_next_size(&r, &size) == MP4D_NO_ERROR ); expect( size == 120 );
        expect( mp4d_saiz_get_next_size(&r, &size) == MP4D_NO_ERROR ); expect( size == 0 );
        expect( mp4d_saiz_get_next_size(&r, &size) == MP4D_NO_ERROR ); expect( size == 0 );
    }

    free(saiz.p_data);
}

static void
test_saio_flag_0(void)
{
    buffer_t saio;
    
    buffer_init(&saio);
    
    write_u8(&saio, 0);  /* version */
    write_u24(&saio, 0); /* flags */

    write_u32(&saio, 1);    /* entry_count */
    write_u32(&saio, 3453); /* offset */

    {
        saio_reader_t r;
        mp4d_atom_t atom = wrap_buffer(&saio);
        
        expect( mp4d_saio_init(&r, &atom) == MP4D_NO_ERROR );
    }

    free(saio.p_data);
}

static void
test_saio_one_entry(void)
{
    buffer_t saio;
    
    buffer_init(&saio);
    
    write_u8(&saio, 0);  /* version */
    write_u24(&saio, 1281); /* flags */

    write_u32(&saio, 7); /* aux_info_type */
    write_u32(&saio, 0); /* aux_info_type_parameter */   

    write_u32(&saio, 1);    /* entry_count */
    write_u32(&saio, 3453); /* offset */

    {
        saio_reader_t r;
        uint64_t offset;
        mp4d_atom_t atom = wrap_buffer(&saio);
        
        expect( mp4d_saio_init(&r, &atom) == MP4D_NO_ERROR );
        expect( r.aux_info_type == 7 );
        expect( mp4d_saio_get_next(&r, 99, &offset) == MP4D_NO_ERROR); expect( offset == 3453 );
        expect( mp4d_saio_get_next(&r, 99, &offset) == MP4D_NO_ERROR); expect( offset == 99 );
    }

    free(saio.p_data);
}

static void
test_saio_multiple_entries(void)
{
    buffer_t saio;
    
    buffer_init(&saio);
    
    write_u8(&saio, 0);  /* version */
    write_u24(&saio, 1281); /* flags */

    write_u32(&saio, 7); /* aux_info_type */
    write_u32(&saio, 0); /* aux_info_type_parameter */   

    write_u32(&saio, 3);    /* entry_count */
    write_u32(&saio, 101); /* offset */
    write_u32(&saio, 106); /* offset */
    write_u32(&saio, 102); /* offset */

    {
        saio_reader_t r;
        uint64_t offset;
        mp4d_atom_t atom = wrap_buffer(&saio);
        
        expect( mp4d_saio_init(&r, &atom) == MP4D_NO_ERROR );
        expect( mp4d_saio_get_next(&r, 99, &offset) == MP4D_NO_ERROR); expect( offset == 101 );
        expect( mp4d_saio_get_next(&r, 98, &offset) == MP4D_NO_ERROR); expect( offset == 106 );
        expect( mp4d_saio_get_next(&r, 97, &offset) == MP4D_NO_ERROR); expect( offset == 102 );
        expect( mp4d_saio_get_next(&r, 96, &offset) == MP4D_NO_ERROR); expect( offset == 96 );
        expect( mp4d_saio_get_next(&r, 95, &offset) == MP4D_NO_ERROR); expect( offset == 95 );
    }

    free(saio.p_data);
}

static void
test_saio_multiple_entries_version_1(void)
{
    buffer_t saio;
    
    buffer_init(&saio);
    
    write_u8(&saio, 1);  /* version */
    write_u24(&saio, 1281); /* flags */

    write_u32(&saio, 7); /* aux_info_type */
    write_u32(&saio, 0); /* aux_info_type_parameter */   

    write_u32(&saio, 3);    /* entry_count */
    write_u64(&saio, 101); /* offset */
    write_u64(&saio, 106); /* offset */
    write_u64(&saio, 102); /* offset */

    {
        saio_reader_t r;
        uint64_t offset;
        mp4d_atom_t atom = wrap_buffer(&saio);
        
        expect( mp4d_saio_init(&r, &atom) == MP4D_NO_ERROR );
        expect( mp4d_saio_get_next(&r, 99, &offset) == MP4D_NO_ERROR); expect( offset == 101 );
        expect( mp4d_saio_get_next(&r, 98, &offset) == MP4D_NO_ERROR); expect( offset == 106 );
        expect( mp4d_saio_get_next(&r, 97, &offset) == MP4D_NO_ERROR); expect( offset == 102 );
        expect( mp4d_saio_get_next(&r, 96, &offset) == MP4D_NO_ERROR); expect( offset == 96 );
        expect( mp4d_saio_get_next(&r, 95, &offset) == MP4D_NO_ERROR); expect( offset == 95 );
    }

    free(saio.p_data);
}

int main(void)
{
    TEST_START("Trackreader");

    /* stts, ctts */
    test_tts_not_init();
    test_tts_bad_version();
    test_tts_empty();
    test_tts_one_sample();
    test_tts_one_entry();
    test_tts_multiple_entries();
    test_tts_multiple_entries_stts_next();
    test_tts_multiple_entries_ctts_next();
    test_tts_first_empty();
    test_tts_seek();
    test_tts_seek_nodelta();
    test_tts_seek_next();

    /* stsz, stz2 */
    test_stsz_not_init();
    test_stsz_s0_empty();
    test_stsz_s0_single();
    test_stsz_s0_multiple();
    test_stsz_s1_empty();
    test_stsz_s1_single();
    test_stsz_s1_multiple();
    test_stz2_invalid();
    test_stz2_4();
    test_stz2_8();
    test_stz2_16();

    /* stsc */
    test_stsc_not_init();
    test_stsc_empty();
    test_stsc_empty_entries();
    test_stsc_one_entry_1();
    test_stsc_one_entry_2();
    test_stsc_multiple_entries();
    test_stsc_multiple_with_empty();
    test_stsc_first_chunk_not_ascending();

    /* stco, co64 */
    test_co_not_init();
    test_stco_empty();
    test_stco_single();
    test_stco_multiple();
    test_co64_multiple();

    /* stss */
    test_stss_no_box();
    test_stss_illegal_order();
    test_stss_empty();
    test_stss_single_first();
    test_stss_single_not_first();
    test_stss_multiple();
    test_stss_all();

    /* edit list */
    test_elst_empty();
    test_elst_one_entry_1();
    test_elst_one_entry_2();
    test_elst_one_entry_offset();
    test_elst_multiple();
    test_elst_empty_dwell();
    test_elst_time_scale();

    /* sub-sample */
    test_subs_no_box();
    test_subs_empty();
    test_subs_one_entry();
    test_subs_first_sample();
    test_subs_multiple_entries();
    test_subs_version_1();

    /* sample aux size */
    test_saiz_flag_0();
    test_saiz_default_empty();
    test_saiz_default_multiple();
    test_saiz_empty();
    test_saiz_multiple();

    /* trik box*/
    test_trik_not_init();

    /* sample aux offset */
    test_saio_flag_0();
    test_saio_one_entry();
    test_saio_multiple_entries();
    test_saio_multiple_entries_version_1();
    TEST_END(nfailed, ntests);
}
