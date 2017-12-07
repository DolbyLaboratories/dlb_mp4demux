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

#include "stream.h"

#include "util.h"

#include "assert.h"

#ifndef _MSC_VER
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

int
stream_init(stream_t *p_s,
            fragment_reader_t source,
            uint32_t track_ID,
            const char *stream_name,
            uint32_t movie_time_scale,
            uint32_t media_time_scale)
{
    int err = 0;

    p_s->track_ID = track_ID;
    p_s->name = stream_name != NULL ? string_dup(stream_name) : NULL;

    p_s->movie_time_scale = movie_time_scale;
    p_s->media_time_scale = media_time_scale;

    p_s->p_static_mem = NULL;
    p_s->p_dynamic_mem = NULL;

    p_s->have_sample = 0;

    p_s->subsample_pos = NULL;
    p_s->subsample_size = NULL;
    p_s->size_subsample = 0;
    p_s->subtitle_track_flag = 0;

    p_s->fragments = source;
    p_s->have_fragment = 0; /* Before getting first fragment */

    p_s->sample.pts = 0;
    p_s->sample.presentation_duration = 0;
    p_s->sample.is_first_sample_in_segment = 0;
    /* Create track reader */
    {
        uint64_t static_mem_size, dyn_mem_size;

        CHECK( mp4d_trackreader_query_mem(&static_mem_size,
                                          &dyn_mem_size) );

        ASSURE( (uint64_t) (size_t) static_mem_size == static_mem_size, ("Cannot allocate %" PRIu64 " bytes of memory", static_mem_size) );
        ASSURE( (uint64_t) (size_t) dyn_mem_size == dyn_mem_size, ("Cannot allocate %" PRIu64 " bytes of memory", dyn_mem_size) );

        p_s->p_static_mem = malloc((size_t) static_mem_size);
        p_s->p_dynamic_mem = malloc((size_t) dyn_mem_size);

        CHECK( mp4d_trackreader_init(&(p_s->p_tr),
                                     p_s->p_static_mem,
                                     p_s->p_dynamic_mem) );

        if (p_s->fragments->get_type != NULL)
        {
            mp4d_ftyp_info_t ftyp;  /* Probably ftyp */
            int err_ft = fragment_reader_get_type(p_s->fragments, &ftyp);

            if (err_ft != 0)
            {
                WARNING(("Could not get file type (err: %d)", err_ft));
            }
            else
            {
                CHECK( mp4d_trackreader_set_type(p_s->p_tr,
                        &ftyp) );
            }
        }

        p_s->have_sample = 0;  /* No sample in the queue */
    }

cleanup:
    return err;
}

void
stream_deinit(stream_t *p_s)
{
    if (p_s != NULL)
    {
        free(p_s->name);
        free(p_s->p_static_mem);
        free(p_s->p_dynamic_mem);

        free(p_s->subsample_pos);
        free(p_s->subsample_size);

        if (p_s->fragments != NULL)
        {
            fragment_reader_destroy(p_s->fragments);
        }
    }
}

int
stream_seek(stream_t *p_s,
            uint64_t seek_time,  /**< movie time scale */
            uint64_t *out_time   /**< [out] result of seek (movie time scale) */
    )
{
    int err = 0;
    int err_seek;
    mp4d_fourcc_t type;
    uint32_t media_time_scale, movie_time_scale;
    uint64_t offset_time;

    if (!p_s->have_fragment)
    {
        do
        {
            CHECK( fragment_reader_next_atom(p_s->fragments) );
            CHECK( mp4d_demuxer_get_type(p_s->fragments->p_dmux, &type) );
        } while (!MP4D_FOURCC_EQ(type, "moov") &&
                 !MP4D_FOURCC_EQ(type, "moof"));

        if (p_s->track_ID == 0)
        {
            /* track_ID not known before looking in fragment */
            int err_tr = MP4D_E_TRACK_NOT_FOUND;

            while (err_tr == MP4D_E_TRACK_NOT_FOUND)
            {
                p_s->track_ID++;
                err_tr = mp4d_trackreader_init_segment(p_s->p_tr,
                        p_s->fragments->p_dmux,
                        p_s->track_ID,
                        p_s->movie_time_scale,
                        p_s->media_time_scale,
                        NULL);
            }
        }
        else
        {
            CHECK( mp4d_trackreader_init_segment(p_s->p_tr,
                    p_s->fragments->p_dmux,
                    p_s->track_ID,
                    p_s->movie_time_scale,
                    p_s->media_time_scale,
                    NULL) );
        }
        p_s->have_fragment = 1;
    }

    p_s->sample.presentation_duration = 0;
    p_s->sample.pts = 0;
    p_s->have_sample = 0;

    /* Try to seek in the current moov/moof */
    CHECK( mp4d_demuxer_get_type(p_s->fragments->p_dmux, &type));

    if (MP4D_FOURCC_EQ(type, "moov") || MP4D_FOURCC_EQ(type, "moof"))
    {
        err_seek = mp4d_trackreader_seek_to(p_s->p_tr, seek_time, out_time);

        if (err_seek == MP4D_NO_ERROR)
        {
            p_s->have_sample = 0;
            return 0;
        }

        ASSURE( err_seek == MP4D_E_PREV_SEGMENT ||
                err_seek == MP4D_E_NEXT_SEGMENT,
                ("Seek to %" PRIu64 " (movie time scale) failed (error %d)\n", seek_time, err_seek) );
    }

    /* Not in the current fragment, call source->seek */

    CHECK( mp4d_trackreader_get_time_scale(p_s->p_tr, &movie_time_scale, &media_time_scale) );

    /* Load a fragment before/at the requested time */
    CHECK( fragment_reader_seek(p_s->fragments, p_s->track_ID,
             (seek_time * media_time_scale) / movie_time_scale,
             &offset_time) );

    if(p_s->subtitle_track_flag == 1)
    {
        p_s->have_sample = 0;
        return err;
    }

    do
    {
        err_seek = mp4d_trackreader_init_segment(p_s->p_tr,
                                                 p_s->fragments->p_dmux,
                                                 p_s->track_ID,
                                                 p_s->movie_time_scale,
                                                 p_s->media_time_scale,
                                                 &offset_time);

        ASSURE( err_seek == MP4D_NO_ERROR || err_seek == MP4D_E_TRACK_NOT_FOUND,
                ("Error %d while initializing trackreader", err_seek) );

        offset_time = 0;  /* On a next call, the moof box time is inherited from the preceeding moof box */

        if (err_seek == MP4D_NO_ERROR)
        {
            err_seek = mp4d_trackreader_seek_to(p_s->p_tr,
                                                seek_time, out_time);

            ASSURE( err_seek == MP4D_NO_ERROR ||
                    err_seek == MP4D_E_PREV_SEGMENT ||
                    err_seek == MP4D_E_NEXT_SEGMENT,
                    ("Failed (%d) to seek to %" PRIu64 " (movie time scale)", err_seek, seek_time) );

            if (err_seek == MP4D_E_PREV_SEGMENT)
            {
                /* Seek point is before the fragment indicated by mfra,
                   or is after the previous fragment which returned MP4D_E_NEXT_SEGMENT */

                /* Leave the state at the beginning of this fragment.
                   The samples in this fragment may have the desired time stamps */
                err_seek = MP4D_NO_ERROR;
            }
        }
        else
        {
            assert( err_seek == MP4D_E_TRACK_NOT_FOUND );
            /* Try a later fragment */
            err_seek = MP4D_E_NEXT_SEGMENT;
        }

        assert( err_seek == MP4D_NO_ERROR ||
                err_seek == MP4D_E_NEXT_SEGMENT );

        if (err_seek == MP4D_E_NEXT_SEGMENT)
        {
            int err_next;

            logout(LOG_VERBOSE_LVL_INFO,
                   "track_ID %d: Seek request to %" PRIu64 " (movie time scale), in a later fragment\n",
                   p_s->track_ID,
                   seek_time);

            do
            {
                err_next = fragment_reader_next_atom(p_s->fragments);

                ASSURE( err_next != 2, ("Could not seek to %" PRIu64 " (movie time scale) for track_ID %d. End of presentation",
                                        seek_time, p_s->track_ID) );

                ASSURE( err_next == 0,
                        ("Unexpected error %d when getting next moof", err_next) );

                CHECK( mp4d_demuxer_get_type(p_s->fragments->p_dmux, &type) );

            } while (!MP4D_FOURCC_EQ(type, "moof"));
        }
    } while (err_seek == MP4D_E_NEXT_SEGMENT);

    assert( err_seek == MP4D_NO_ERROR );

    p_s->have_sample = 0;

cleanup:
    return err;
}

int
stream_next_sample(stream_t *p_s,
                   int single_fragment   /* Stay within one fragment? */
    )
{
    int err = 0;
    mp4d_error_t err_tr;

    if (!p_s->have_fragment)
    {
        err_tr = MP4D_E_NEXT_SEGMENT;
    }
    else
    {
        err_tr = mp4d_trackreader_next_sample(p_s->p_tr,
                &p_s->sample);

        ASSURE( err_tr == MP4D_NO_ERROR || err_tr == MP4D_E_NEXT_SEGMENT,
                ("Failed (%d) to get the next sample", err_tr) );
    }

    while (err_tr != MP4D_NO_ERROR)
    {
        mp4d_fourcc_t type;

        /* Need to read next fragment */
        if (single_fragment)
        {
            return 0;
        }

        do
        {
            err_tr = fragment_reader_next_atom(p_s->fragments);

            ASSURE( err_tr == 0 ||  err_tr == MP4D_E_SKIP_BIG_BOX || err_tr == 2,
                    ("Unexpected error %d when getting next moof", err_tr));

            if (err_tr == 0 )
            {
                CHECK( mp4d_demuxer_get_type(p_s->fragments->p_dmux, &type) );
            }

            /* atom is mdat/free/skip box, just set to mdat */
            if (err_tr == MP4D_E_SKIP_BIG_BOX)
            {
                MP4D_FOURCC_ASSIGN(type, "mdat");
            }

        } while ((err_tr == 0 || err_tr == MP4D_E_SKIP_BIG_BOX) &&
                    (!MP4D_FOURCC_EQ(type, "moof") &&
                     !MP4D_FOURCC_EQ(type, "moov")
                    )
                );

        if (err_tr == 2)
        {
            /* end of track */
            return 2;
        }

        p_s->have_fragment = 1;

        if (p_s->track_ID == 0)
        {
            /* track_ID not known before looking in fragment */
            err_tr = MP4D_E_TRACK_NOT_FOUND;
            while (err_tr == MP4D_E_TRACK_NOT_FOUND)
            {
                p_s->track_ID++;
                err_tr = mp4d_trackreader_init_segment(p_s->p_tr,
                        p_s->fragments->p_dmux,
                        p_s->track_ID,
                        p_s->movie_time_scale,
                        p_s->media_time_scale,
                        NULL);
                /* danger of infinite loop here e.g. if the fragment contains no tracks.
                 * Could be avoided by having a get_available_track_IDs() library method
                 */
            }
        }
        else
        {
            err_tr = mp4d_trackreader_init_segment(p_s->p_tr,
                    p_s->fragments->p_dmux,
                    p_s->track_ID,
                    p_s->movie_time_scale,
                    p_s->media_time_scale,
                    NULL);
        }

        ASSURE( err_tr == MP4D_NO_ERROR || err_tr == MP4D_E_TRACK_NOT_FOUND,
                ("Failed to initialize track reader") );

        if (err_tr == MP4D_NO_ERROR)
        {
            err_tr = mp4d_trackreader_next_sample(p_s->p_tr,
                                                  &p_s->sample);

            ASSURE( err_tr == MP4D_NO_ERROR || err_tr == MP4D_E_NEXT_SEGMENT,
                    ("Failed (%d) to get the next sample", err_tr));

        }
        assert( err_tr == MP4D_NO_ERROR ||
                err_tr == MP4D_E_NEXT_SEGMENT ||
                err_tr == MP4D_E_TRACK_NOT_FOUND );
    }

    p_s->have_sample = 1;

cleanup:
    return err;
}

int
subtitle_next_sample(stream_t *p_s,
                   int single_fragment   /* Stay within one fragment? */
    )
{
    int err = 0;
    uint64_t offset_time;
    mp4d_error_t err_tr;

   CHECK( fragment_reader_seek(p_s->fragments, p_s->track_ID,
        (p_s->sample.pts + p_s->sample.presentation_duration),
         &offset_time) );
   if (offset_time ==  (uint64_t)(p_s->sample.pts + p_s->sample.presentation_duration))
   {
        err_tr = mp4d_trackreader_init_segment(p_s->p_tr,
                                                p_s->fragments->p_dmux,
                                                p_s->track_ID,
                                                p_s->movie_time_scale,
                                                p_s->media_time_scale,
                                                &offset_time);
        ASSURE( err_tr == MP4D_NO_ERROR, ("should no error"));

        err_tr = mp4d_trackreader_next_sample(p_s->p_tr,
                                              &p_s->sample);

        if (err_tr == MP4D_NO_ERROR)
        {
            p_s->have_sample = 1;
        }
   }
cleanup:
    return 0;
}

int
stream_set_tenc(
    stream_t *p_s,
    uint32_t default_algorithmID,
    uint8_t default_iv_size,
    uint8_t* default_kid)
{
    return mp4d_trackreader_set_tenc(p_s->p_tr, 
        default_algorithmID, 
        default_iv_size, 
        default_kid);
}

int
stream_get_cur_tenc(
    stream_t *p_s,
    uint32_t* algorithmID,
    uint8_t* iv_size,
    uint8_t* kid)
{
    return mp4d_trackreader_get_cur_tenc(p_s->p_tr, 
        algorithmID, 
        iv_size, 
        kid);
}
