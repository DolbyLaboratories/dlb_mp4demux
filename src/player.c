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

#include "player.h"
#include "util.h"

#include <string.h>
#include <assert.h>

#ifndef _MSC_VER
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#endif

/**
 * @brief load sample into memory, decrypt if encrypted
 */
static int
load_sample(stream_t *p_s,
            const mp4d_sampleref_t *p_sample,
            struct sample_entry_t_ *sample_entries,
            uint32_t num_sample_entries,
            unsigned char *p_data                  /**< [out] */
    )
{
    return fragment_reader_load(p_s->fragments, p_sample->pos, p_sample->size, p_data);
}

/** Considers all active tracks. Returns the sample with minimum evaluation,
 *  which is before the stop_time.
 *
 *  @return 0: ok
 *          1: unknown error
 *          2: no more samples (less than stop_time, or in this fragment when playing a single fragment)
*/
int
next_sample(player_t p_d,
            uint32_t *track_ID,       /**< [out] track_ID of output sample */
            const char **stream_name, /**< [out] stream_name of output sample */
            mp4d_sampleref_t **sample, /**< [out] on success, pointer to the next sample */
            uint32_t *active_track
    )
{
    int err = 0;
    int have_any = 0;   /* (boolean) have sample for any track */
    uint32_t min_i = 0;
    uint32_t i;
    uint64_t min_val = 0;  /* suppress compiler warning */
    have_any = 0;

    /* Attempt to fill sample queue for video/audio and subtitle track */
    for (i = 0; i < p_d->num_streams; i++)
    {
        if (active_track[i] == 1)
        {
            if (!p_d->streams[i].stream.have_sample &&
                !p_d->streams[i].end_of_track)
            {
                p_d->streams[i].stream.have_sample = 0;
                if (p_d->streams[i].stream.subtitle_track_flag == 1)
                {
                    CHECK( subtitle_next_sample(&p_d->streams[i].stream,
                          p_d->single_fragment) );
                }
                else
                {
                    err = stream_next_sample(&p_d->streams[i].stream,
                          p_d->single_fragment);
                    /* no more sample in this track */
                    if (err == 2) 
                    {
                        p_d->streams[i].end_of_track = 1;
                    }
                    else 
                    {
                        CHECK( err );
                    }
                }

                if (p_d->streams[i].stream.have_sample)
                {
                    uint64_t stop_time_mts;

                    stop_time_mts = (p_d->stop_time * p_d->streams[i].stream.media_time_scale) / p_d->movie_time_scale;
                    if (p_d->streams[i].stream.sample.pts + p_d->streams[i].stream.sample.presentation_offset > (int64_t) stop_time_mts)
                    {
                        p_d->streams[i].stream.have_sample = 0;
                        p_d->streams[i].end_of_track = 1;
                    }
                }
            }

            if (p_d->streams[i].stream.have_sample &&
                (!have_any || p_d->eval_sample(&p_d->streams[i].stream.sample, p_d->streams[i].stream.media_time_scale) < min_val)
                )
            {
                min_i = i;
                min_val = p_d->eval_sample(&p_d->streams[i].stream.sample, p_d->streams[i].stream.media_time_scale);
                have_any = 1;
            }
        }
    }

    if (!have_any)
    {
        err = 2;
        goto cleanup;
    }

    *sample = &p_d->streams[min_i].stream.sample;
    *track_ID = p_d->streams[min_i].stream.track_ID;
    *stream_name = p_d->streams[min_i].stream.name;

    /* Fill subsample info */
    if (p_d->streams[min_i].stream.size_subsample < (*sample)->num_subsamples)
    {
        p_d->streams[min_i].stream.size_subsample = (*sample)->num_subsamples;
        p_d->streams[min_i].stream.subsample_pos = realloc(p_d->streams[min_i].stream.subsample_pos, (*sample)->num_subsamples * sizeof(uint64_t));
        p_d->streams[min_i].stream.subsample_size = realloc(p_d->streams[min_i].stream.subsample_size, (*sample)->num_subsamples * sizeof(uint32_t));

        ASSURE( p_d->streams[min_i].stream.subsample_pos != NULL, ("Allocation error") );
        ASSURE( p_d->streams[min_i].stream.subsample_size != NULL, ("Allocation error") );
    }

    {
        uint32_t i;
        for (i = 0; i < (*sample)->num_subsamples; i++)
        {
            CHECK( mp4d_trackreader_next_subsample(p_d->streams[min_i].stream.p_tr,
                                                   *sample,
                                                   &p_d->streams[min_i].stream.subsample_pos[i],
                                                   &p_d->streams[min_i].stream.subsample_size[i]));
        }
    }

    p_d->streams[min_i].stream.have_sample = 0;

cleanup:
    return err;
}

/**
 * @brief play until stop time reached, or end of fragment if single fragment
 */
static int
play(player_t p_d,
            int single_fragment   /* boolean */
    )
{
    int err = 0;
    int ns_err;
    unsigned int active_track[255+32+1];
    unsigned int index;
    p_d->single_fragment = single_fragment;
    assert(p_d->eval_sample != NULL);

    /* for our driver application, we set all track to active */
    for (index = 0; index < p_d->num_streams; index++)
    {
        active_track[index] = 1;
    }

    do
    {
        mp4d_sampleref_t * sample;
        uint32_t track_ID;
        const char *name;
        uint32_t i;


        ns_err = next_sample(p_d, &track_ID, &name, &sample, active_track);
        if (ns_err == 0)
        {
            if (!sample->dts) 
            {
                mp4d_trackreader_get_stss_count(p_d->streams->stream.p_tr, &(p_d->streams->stream.stss_count), &(p_d->streams->stream.stss_buf));
            }
            logout(LOG_VERBOSE_LVL_INFO,"Track_ID %d sample's DTS: %lld CTS: %lld PTS: %lld \n", track_ID, sample->dts, sample->cts, sample->pts);
            /* Push sample to subscribers of this track_ID */
            for (i = 0; i < p_d->num_streams; i++)
            {
                /* Match with name before track_ID, if name is non-NULL
                 * (essentail for MPEG-DASH where track_IDs are not unique) */
                if ((name != NULL && strcmp(name, p_d->streams[i].stream.name) == 0) ||
                    (name == NULL && p_d->streams[i].stream.track_ID == track_ID))
                {
                    uint32_t j;

                    if (p_d->streams[i].data_size < sample->size)
                    {
                        p_d->streams[i].data = realloc(p_d->streams[i].data, sample->size);
                    }

                    CHECK( load_sample(&p_d->streams[i].stream,
                                       sample,
                                       p_d->streams[i].sample_entries,
                                       p_d->streams[i].num_sample_entries,
                                       p_d->streams[i].data) );

                    for (j = 0; p_d->streams[i].sink[j] != NULL; j++)
                    {
                        es_sink_t sink = p_d->streams[i].sink[j];
                        uint32_t k;
                        unsigned char *subsample_payload = p_d->streams[i].data;

                        if (sample->num_subsamples == 1)
                        {
                            CHECK( sink_sample_ready(sink, sample, p_d->streams[i].data) );
                        }
                        else if (sample->num_subsamples > 1)
                        {
                            for (k = 0; k < sample->num_subsamples; k++)
                            {
                                if (sink->subsample_ready != NULL)
                                {
                                    CHECK( sink_subsample_ready(
                                               k,
                                               sink,
                                               sample,
                                               subsample_payload,
                                               p_d->streams[i].stream.subsample_pos[k],
                                               p_d->streams[i].stream.subsample_size[k]) );
                                }
                                subsample_payload += p_d->streams[i].stream.subsample_size[k];
                            }
                        }
                        else
                        {
                             ASSURE(sample->num_subsamples < 1 , ("Not valid subsample number!") );
                        }
                    }
                    break;
                }
            }
        }

    } while (ns_err == 0);

    ASSURE( ns_err == 2, ("Unexpected error (%d) when getting next sample", ns_err));

cleanup:
    return err;
}

/** @brief call-back function to define samples order
 */
static uint64_t
get_sample_offset(const mp4d_sampleref_t *sample, uint32_t media_time_scale)
{
    media_time_scale = media_time_scale;
    return sample->pos;
}

/** @brief call-back function to define samples order
 */
static uint64_t
get_sample_pts(const mp4d_sampleref_t *sample, uint32_t media_time_scale)
{
    /* Return presentation time stamp in miliseconds */
    return (sample->pts * 1000 / media_time_scale);
}

/** @brief call-back function to define samples order
 */
static uint64_t
get_sample_dts(const mp4d_sampleref_t *sample, uint32_t media_time_scale)
{
    /* Return presentation time stamp in miliseconds */
    return (sample->dts * 1000 / media_time_scale);
}

int
player_new(player_t *p_d)
{
    int err = 0;

    ASSURE( p_d != NULL, ("Null pointer") );

    *p_d = malloc(sizeof(**p_d));

    ASSURE( p_d != NULL, ("Allocation failure") );

    (*p_d)->streams = NULL;
    (*p_d)->num_streams = 0;

    (*p_d)->decrypt_info.num_keys = 0;
    (*p_d)->decrypt_info.keys = NULL;
cleanup:
    return err;
}

int
player_destroy(player_t *p_d)
{
    if (p_d != NULL && *p_d != NULL)
    {
        player_t d = *p_d;
        uint32_t i;

        free(d->decrypt_info.keys);

        for (i = 0; i < d->num_streams; i++)
        {
            uint32_t j;

            free(d->streams[i].sample_entries);

            stream_deinit(&d->streams[i].stream);

            for (j = 0; d->streams[i].sink[j] != NULL; j++)
            {
                sink_destroy(d->streams[i].sink[j]);
            }
            free(d->streams[i].data);
        }
        free(d->streams);

        free(d);
    }

    return 0;
}

int
player_set_track(player_t p_d,
                 uint32_t track_ID,
                 const char *stream_name,
                 uint32_t bit_rate,
                 movie_t p_movie,
                 fragment_reader_t mp4_source,
                 es_sink_t sink,
                 uint32_t polarssl_flag)
{
    int err = 0;
    uint32_t i, j;
    int have_track = 0;
    char *name = NULL;  /* For track_ID = 0 */

    /* If track is not already enabled, create trackreader and sample entries for it */
    for (i = 0; i < p_d->num_streams; i++)
    {
        if ((track_ID == 0 && strcmp(stream_name, p_d->streams[i].stream.name) == 0) ||
             (track_ID > 0 && p_d->streams[i].stream.track_ID == track_ID))
        {
            have_track = 1;
            break;
        }
    }

    if (!have_track)
    {
        mp4d_movie_info_t movie_info;
        mp4d_stream_info_t stream_info;
        uint32_t stream_index;
        int found = 0;

        i = p_d->num_streams;
        p_d->num_streams++;
        p_d->streams = realloc(p_d->streams, p_d->num_streams * sizeof(*(p_d->streams)));

        ASSURE( p_d->streams != NULL, ("Allocation failure") );

        memset(p_d->streams[i].sink, 0, sizeof(p_d->streams[i].sink));

        p_d->streams[i].data = NULL;
        p_d->streams[i].data_size = 0;

        CHECK( p_movie->get_movie_info(p_movie, &movie_info) );
        p_d->movie_time_scale = movie_info.time_scale;

        for (stream_index = 0; stream_index < movie_info.num_streams; stream_index++)
        {
            free(name); name = NULL;
            if (p_movie->get_stream_info(p_movie, stream_index, bit_rate, &stream_info, &name) == 0)
            {
                assert( (track_ID == 0 && stream_name != NULL) ||
                        (track_ID > 0 && stream_name == NULL) );

                if ((track_ID > 0 && stream_info.track_id == track_ID) ||
                    (track_ID == 0 && strcmp(name, stream_name) == 0))
                {
                    found = 1;
                    break;
                }
            }
        }

        ASSURE( found, ("Could not find stream information for track_ID = %" PRIu32, track_ID) );

        CHECK( stream_init(&p_d->streams[i].stream,
                                mp4_source,
                                track_ID,
                                stream_name,
                                movie_info.time_scale,
                                stream_info.time_scale)
            );

        p_d->streams[i].end_of_track = 0;

        /* Create decryptors for each encrypted sample description index */
        p_d->streams[i].sample_entries = NULL;
        p_d->streams[i].num_sample_entries = stream_info.num_dsi;

        p_d->streams[i].sample_entries = malloc(stream_info.num_dsi * sizeof(*p_d->streams[i].sample_entries));
        ASSURE( p_d->streams[i].sample_entries != NULL, ("Allocation failure") );

        for (j = 0; j < stream_info.num_dsi; j++)
        {
            mp4d_crypt_info_t *p_crypt = NULL;

            CHECK( p_movie->get_sampleentry(p_movie, stream_index, bit_rate, j + 1, &p_d->streams[i].sample_entries[j].entry) );

            if (MP4D_FOURCC_EQ(stream_info.hdlr, "soun"))
            {
                p_d->streams[i].sample_entries[j].index = j + 1;
                p_crypt = &p_d->streams[i].sample_entries[j].entry.soun.crypt_info;
                p_d->streams[i].sample_entries[j].entry.soun.timescale = p_d->streams[i].stream.media_time_scale;
            }
            else if (MP4D_FOURCC_EQ(stream_info.hdlr, "vide"))
            {
                p_d->streams[i].sample_entries[j].index = j + 1;
                p_crypt = &p_d->streams[i].sample_entries[j].entry.vide.crypt_info;
            }
            else if (MP4D_FOURCC_EQ(stream_info.hdlr, "subt"))
            {
                p_d->streams[i].sample_entries[j].index = j + 1;
                p_crypt = NULL;
                p_d->streams[i].stream.subtitle_track_flag = 1;
            }
            else if (MP4D_FOURCC_EQ(stream_info.hdlr, "meta"))
            {
                p_d->streams[i].sample_entries[j].index = j + 1;
                p_crypt = NULL;
            }
        } /* for each sample description */
    } /* if !have_track */

    assert( p_d->streams[i].stream.track_ID == track_ID );

    /* Add ES subscriber, submit all sample entries for this track */
    j = 0;
    while (p_d->streams[i].sink[j] != NULL)
    {
        j++;
    }
    p_d->streams[i].sink[j] = sink;
    assert( p_d->streams[i].sink[j + 1] == NULL );

    for (j = 0; j < p_d->streams[i].num_sample_entries; j++)
    {
        CHECK( sink_sample_entry(sink, &p_d->streams[i].sample_entries[j].entry) );
    }

cleanup:
    free(name);
    return err;
}

/** @brief Seek all active tracks to the given presentation_time
 */
int
player_seek(player_t p_d,
            float presentation_time  /**< in seconds */
    )
{
    const uint64_t m = (uint64_t) -1;
    int err = 0;
    uint32_t i;

    ASSURE( presentation_time * p_d->movie_time_scale < (float) m, ("Requested presentation time (%f) is too big", presentation_time) );

    for (i = 0; i < p_d->num_streams; i++)
    {
        uint64_t seek_time = (uint64_t) (presentation_time * p_d->movie_time_scale);

        p_d->streams[i].end_of_track = 0;

        CHECK( stream_seek(&p_d->streams[i].stream, seek_time, &seek_time) );

        logout(LOG_VERBOSE_LVL_INFO,
               "track_ID %d: Seek request to %.3f s, got %.3f s\n",
               p_d->streams[i].stream.track_ID,
               presentation_time,
               seek_time / ((double)p_d->movie_time_scale));
    }

cleanup:
    return err;
}

int
player_play_fragments(player_t p_d,
                      uint32_t fragment_number
    )
{
    int err = 0;
    int single_fragment;
    uint32_t f;

    for (f = 1; f < fragment_number; f++)
    {
        /* Move each stream to next moof */
        uint32_t i;
        for (i = 0; i < p_d->num_streams; i++)
        {
            mp4d_fourcc_t type;
            stream_t *p_s = &p_d->streams[i].stream;
            do
            {
                CHECK( fragment_reader_next_atom(p_s->fragments) );
                CHECK( mp4d_demuxer_get_type(p_s->fragments->p_dmux, &type) );
            } while (!MP4D_FOURCC_EQ(type, "moof"));
        }
    }

    p_d->stop_time = (uint64_t) -1; /* infinity */
    p_d->eval_sample = get_sample_dts;

    if (fragment_number == 0)
    {
        single_fragment = 0;
    }
    else
    {
        single_fragment = 1;
    }
    CHECK( play(p_d, single_fragment) );
cleanup:
    return err;
}

int
player_play_time_range(player_t p_d,
                       float *start_time,
                       float *stop_time)
{
    int err = 0;
    int single_fragment = 0;

    ASSURE( start_time != NULL, ("Missing start time") );

    if (stop_time != NULL)
    {
        uint64_t m = (uint64_t) -1;

        ASSURE( (*stop_time) * p_d->movie_time_scale < (float) m, ("Stop time (%f s) is too big", *stop_time) );
        p_d->stop_time = (uint64_t) ((*stop_time) * p_d->movie_time_scale);
    }
    else
    {
        p_d->stop_time = (uint64_t) -1; /* infinity */
    }
    p_d->eval_sample = get_sample_pts;

    CHECK( player_seek(p_d, *start_time) );
    CHECK( play(p_d, single_fragment) );

cleanup:
    return err;
}
