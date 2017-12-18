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
* @brief frontend of mp4demuxer
*/

#include "md_sink.h"
#include "file_movie.h"
#include "player.h"
#include "util.h"

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#ifdef _MSC_VER
#define PRIu32 "I32u"
#define PRId64 "I64d"
#define PRIu64 "I64u"
#include <io.h>
#include <direct.h>
#define mkdir(x) _mkdir(x)
#define access(x, y) _access(x, y)
#else
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <inttypes.h>
#define mkdir(x) mkdir(x, S_IRWXU | S_IRWXG | S_IRWXO)
#endif

#ifdef WIN32
static const char *ProgramName = "mp4demuxer.exe";
#else
static const char *ProgramName = "mp4demuxer";
#endif

typedef struct time_range_t_
{
    float start;
    float end;
}time_range_t;

/**
* Command line options
*/
typedef struct options_t_
{
    int no_dump;      /* (boolean) write ES? */
    int raw_dump;     /* (boolean) write ES in raw format? */
    int dump_to_stdout;/* (boolean) write ES in stdout? */
    int show_samples; /* (boolean) dump samples info to stdout? */
    const char *pdin_rates;  /* download rates at which to report initial delay (pdin) */
    time_range_t time_ranges; /* time ranges to demux */
    const char *decryption_keys;
    const char *filename; /* to demux */
    const char *output_folder; /* output folder's path and name*/
    char output_path[255];  /* output folder's path and name*/
    const char *server_manifest; /* server manifest filename for mockup server */
    const char *item;  /* filename of iloc contents */
    long int fragment_number;  /* to demux, or 0 for all */
    int dv_single_ves_output_flag; /* to demux dolby vision dual track mp4 into single ves file*/
} options_t;

/**
* Application data
*/
typedef struct
{
    options_t options;         /* command line options */
    player_t player;           /* demuxer player handle */
} app_data_t;


/** @brief true if string starts with pref
*/
static
    int startswith(const char *s, const char *prefix)
{
    return (strlen(s) >= strlen(prefix) &&
        strncmp(s, prefix, strlen(prefix)) == 0);
}

/** @brief true if string ends with suffix
*/
static
    int endswith(const char *s, const char *suffix)
{
    return (strlen(s) >= strlen(suffix) &&
        strcmp(s + strlen(s) - strlen(suffix), suffix) == 0);
}

/** @brief handler for ftyp and styp
*
*/
int
    on_ftyp(app_data_t *p_data, mp4d_demuxer_ptr_t p_dmux)
{
    mp4d_ftyp_info_t ftyp_info;
    mp4d_atom_t atom;
    char atom_type[5];
    uint32_t t;
    int err;

    (void) p_data; /* unused */
    CHECK( mp4d_demuxer_get_ftyp_info(p_dmux, &ftyp_info) );
    CHECK( mp4d_demuxer_get_atom(p_dmux, &atom) );
    sprintf(atom_type, "%c%c%c%c",
        atom.type[0],
        atom.type[1],
        atom.type[2],
        atom.type[3]);
    logout(LOG_VERBOSE_LVL_INFO,
        "%s: major_brand = %c%c%c%c\n",
        atom_type,
        ftyp_info.major_brand[0],
        ftyp_info.major_brand[1],
        ftyp_info.major_brand[2],
        ftyp_info.major_brand[3]);
    logout(LOG_VERBOSE_LVL_INFO,
        "%s: minor_version = %u\n",
        atom_type,
        ftyp_info.minor_version);
    logout(LOG_VERBOSE_LVL_INFO,
        "%s: compatible_brands: ", atom_type);

    for (t=0; t<ftyp_info.num_compat_brands; t++) 
    {
        logout(LOG_VERBOSE_LVL_INFO,
            "%c%c%c%c ", 
            ftyp_info.compat_brands[4*t+0],ftyp_info.compat_brands[4*t+1],ftyp_info.compat_brands[4*t+2],ftyp_info.compat_brands[4*t+3]);                
    }
    logout(LOG_VERBOSE_LVL_INFO, "\n");

cleanup:
    return err;
}

int
    on_pdin(app_data_t *p_data, mp4d_demuxer_ptr_t p_dmux)
{
    int err = 0;
    app_data_t *data = p_data;
    mp4d_pdin_info_t pdin_upper, pdin_lower;
    const char *pdin_rates = data->options.pdin_rates;
    char *next;

    while(strlen(pdin_rates) > 0)
    {
        uint32_t rate = strtol(pdin_rates, &next, 10);

        ASSURE( next > pdin_rates, ("Could not parse list of progressive download rates: %s\n", data->options.pdin_rates));
        pdin_rates = next;
        while (pdin_rates[0] == ',')
        {
            pdin_rates++;
        }

        CHECK( mp4d_demuxer_get_pdin_pair(p_dmux, rate, &pdin_lower, &pdin_upper) );
        logout(LOG_VERBOSE_LVL_INFO, "pdin: requesting pdin pair for rate = %u\n", rate);
        if (pdin_lower.rate==0) 
        {
            logout(LOG_VERBOSE_LVL_INFO, "      no lower entry found\n");
        }
        else 
        {
            logout(LOG_VERBOSE_LVL_INFO, "      lower entry: rate = %u bytes/s, initial_delay = %u s\n", pdin_lower.rate, pdin_lower.initial_delay);
        }
        if (pdin_upper.rate==0) 
        {
            logout(LOG_VERBOSE_LVL_INFO, "      no upper entry found\n");
        }
        else 
        {
            logout(LOG_VERBOSE_LVL_INFO, "      upper entry: rate = %u bytes/s, initial_delay = %u s\n", pdin_upper.rate, pdin_upper.initial_delay);
        }

        if (pdin_lower.rate != 0 && pdin_lower.rate < pdin_upper.rate)
        {
            float delay = pdin_lower.initial_delay + ((float)rate - pdin_lower.rate) *
                ((float)pdin_upper.initial_delay - pdin_lower.initial_delay) / (pdin_upper.rate - pdin_lower.rate);
            if (delay < 0)
            {
                delay = 0;
            }

            logout(LOG_VERBOSE_LVL_INFO, 
                "      %spolated at rate = %u bytes/s, initial_delay = %.3f s\n",
                (pdin_lower.rate < rate && rate < pdin_upper.rate) ? "inter" : "extra",
                rate, delay);
        }
    }
cleanup:
    return err;
}

int
    on_bloc(app_data_t *p_data, mp4d_demuxer_ptr_t p_dmux)
{
    mp4d_bloc_info_t bloc_info;
    unsigned char str[512];

    (void) p_data; /* unused */
    mp4d_demuxer_get_bloc_info(p_dmux, &bloc_info);
    memcpy(str, bloc_info.base_location, bloc_info.base_location_size);
    str[bloc_info.base_location_size-1] = '\0';
    logout(LOG_VERBOSE_LVL_INFO, "bloc: base_location = %s\n", str);
    memcpy(str, bloc_info.purchase_location, bloc_info.purchase_location_size);
    str[bloc_info.purchase_location_size-1] = '\0';
    logout(LOG_VERBOSE_LVL_INFO, "bloc: purchase_location = %s\n", str);
    memcpy(str, bloc_info.reserved, bloc_info.reserved_size);
    str[bloc_info.reserved_size-1] = '\0';
    logout(LOG_VERBOSE_LVL_INFO, "bloc: reserved = %s\n", str);

    return 0;
}


static int
    handle_metadata(mp4d_demuxer_ptr_t p_dmux)
{
    uint32_t types[] = {MP4D_MDTYPE_CFMD, 
        MP4D_MDTYPE_AINF,
        MP4D_MDTYPE_MDIR, 
        MP4D_MDTYPE_DLBT, 
        MP4D_MDTYPE_DLBF, 
        MP4D_MDTYPE_DLBK, 
        MP4D_MDTYPE_DLBM, 
        0};
    uint32_t * p_type;

    for (p_type=&types[0]; *p_type!=0; p_type++) 
    {
        mp4d_boxref_t box;
        int err;

        err = mp4d_demuxer_get_metadata(p_dmux, *p_type, &box);
        if (!err) 
        {
            uint32_t t = *p_type;
            logout(LOG_VERBOSE_LVL_INFO, 
                "found metadata of type '%c%c%c%c'\n",
                (unsigned char)((t>>24)&0xff),
                (unsigned char)((t>>16)&0xff),
                (unsigned char)((t>>8)&0xff),
                (unsigned char)((t)&0xff)
                );
            metadata_write(stdout, &box);
        }
    }

    return 0;
}

static int
    on_meta(app_data_t *p_data, mp4d_demuxer_ptr_t p_dmux)
{
    (void)p_data;
    return handle_metadata(p_dmux);
}

static int
    player_select_movie(
    app_data_t *p_data, 
    movie_t p_movie
    )
{
    int err = 0;
    mp4d_movie_info_t movie_info;
    uint32_t stream_num;
    char *stream_name = NULL;  /* track_ID = 0 */
    uint32_t polarssl_flag = 0;

#ifdef HAVE_POLARSSL
    polarssl_flag = 1;
#endif

    CHECK(p_movie->get_movie_info(p_movie, &movie_info) );

    logout(LOG_VERBOSE_LVL_INFO, "moov: duration = (%" PRIu64 " / %u) s\n", 
        movie_info.movie_dur, 
        movie_info.time_scale);
    logout(LOG_VERBOSE_LVL_INFO, "      num streams = %u\n", movie_info.num_streams);

    for (stream_num = 0; stream_num < movie_info.num_streams; stream_num++) 
    {
        mp4d_stream_info_t stream_info;
        fragment_reader_t mp4_source = NULL;
        uint32_t track_ID;
        uint32_t bitrate = 0;  /* compiler warning */
        uint32_t s;
        uint32_t video_flag = 0;
        es_sink_t sink1 = NULL;
        es_sink_t sink2 = NULL;

        free(stream_name); 
        stream_name = NULL;
        CHECK( p_movie->get_stream_info(p_movie, stream_num, bitrate, &stream_info, &stream_name) );

        track_ID = stream_info.track_id;
        if (track_ID == 0)
        {
            logout(LOG_VERBOSE_LVL_INFO, "track_ID 0: name = '%s'\n", stream_name);
        }
        logout(LOG_VERBOSE_LVL_INFO, "track_ID %u: flags = 0x%x (%s%s%s)\n", track_ID, stream_info.flags,
            stream_info.flags & 0x1 ? "enabled " : " ",
            stream_info.flags & 0x2 ? "in_movie " : " ",
            stream_info.flags & 0x4 ? "in_preview" : "");
        logout(LOG_VERBOSE_LVL_INFO, "track_ID %u: time_scale = %u / s\n", track_ID, stream_info.time_scale);
        logout(LOG_VERBOSE_LVL_INFO, "track_ID %u: media_dur = %" PRIu64 "\n", track_ID, stream_info.media_dur);
        logout(LOG_VERBOSE_LVL_INFO, "track_ID %u: media_lang = %u\n", track_ID, stream_info.media_lang);
        logout(LOG_VERBOSE_LVL_INFO, "track_ID %u: hdlr = %c%c%c%c\n", track_ID, 
            stream_info.hdlr[0],stream_info.hdlr[1],stream_info.hdlr[2],stream_info.hdlr[3]);
        logout(LOG_VERBOSE_LVL_INFO, "track_ID %u: codec = %c%c%c%c\n", track_ID, 
            stream_info.codec[0],stream_info.codec[1],stream_info.codec[2],stream_info.codec[3]);
        logout(LOG_VERBOSE_LVL_INFO, "track_ID %u: num_dsi = %u\n", track_ID, stream_info.num_dsi);
        logout(LOG_VERBOSE_LVL_INFO, "track_ID %u: tkhd_width = %u\n", track_ID, stream_info.tkhd_width >> 16);
        logout(LOG_VERBOSE_LVL_INFO, "track_ID %u: tkhd_height = %u\n", track_ID, stream_info.tkhd_height >> 16);

        if ((stream_info.flags & 0x1) == 0)
        {
            /* just give out the warning, and continue to set track...*/
            logout(LOG_VERBOSE_LVL_INFO, "Warning: stream track is disabled!\n" );
        }
        if (MP4D_FOURCC_EQ(stream_info.codec, "H263") ||
            MP4D_FOURCC_EQ(stream_info.codec, "cvid") ||
            MP4D_FOURCC_EQ(stream_info.codec, "TTML") ||
            MP4D_FOURCC_EQ(stream_info.codec, "raw "))
        {
            /* Demuxer does not support getting these QuickTime sample entries. Ignore track. */
            continue;
        }

        for (s = 0; s < stream_info.num_dsi; s++)
        {
            mp4d_sampleentry_t sampleentry;
            mp4d_fourcc_t dsi_type;
            const unsigned char* p_dsi = NULL;
            uint64_t dsi_size;
            mp4d_crypt_info_t *p_crypt = NULL;
            uint32_t k;

            CHECK( p_movie->get_sampleentry(p_movie, stream_num, bitrate, s + 1, &sampleentry) );

            if (MP4D_FOURCC_EQ(stream_info.hdlr,"soun")) 
            {
                mp4d_sampleentry_audio_t *p_entry = &sampleentry.soun;

                logout(LOG_VERBOSE_LVL_INFO, "    Sample entry #%u: data_reference_index = %u\n", s, p_entry->data_reference_index);
                logout(LOG_VERBOSE_LVL_INFO, "    Sample entry #%u: channelcount = %u\n", s, p_entry->channelcount);
                logout(LOG_VERBOSE_LVL_INFO, "    Sample entry #%u: samplerate = %u\n", s, p_entry->samplerate);

                p_dsi = p_entry->dsi;
                memcpy(dsi_type, p_entry->dsi_type, sizeof(mp4d_fourcc_t));
                dsi_size = p_entry->dsi_size;

                p_crypt = &p_entry->crypt_info;
            }
            else if (MP4D_FOURCC_EQ(stream_info.hdlr,"vide")) 
            {
                mp4d_sampleentry_visual_t *p_entry = &sampleentry.vide;

                logout(LOG_VERBOSE_LVL_INFO, "    Sample entry #%u: data_reference_index = %u\n", s, p_entry->data_reference_index);
                logout(LOG_VERBOSE_LVL_INFO, "    Sample entry #%u: width = %u\n", s, p_entry->width);
                logout(LOG_VERBOSE_LVL_INFO, "    Sample entry #%u: height = %u\n", s, p_entry->height);
                logout(LOG_VERBOSE_LVL_INFO, "    Sample entry #%u: compressorname = %s\n", s, p_entry->compressorname);
                logout(LOG_VERBOSE_LVL_INFO, "    Sample entry #%u: par_present = %d\n", s, p_entry->par_present);
                logout(LOG_VERBOSE_LVL_INFO, "    Sample entry #%u: par_hspacing = %u\n", s, p_entry->par_hspacing);
                logout(LOG_VERBOSE_LVL_INFO, "    Sample entry #%u: par_vspacing = %u\n", s, p_entry->par_vspacing);

                p_dsi = p_entry->dsi;
                memcpy(dsi_type, p_entry->dsi_type, sizeof(mp4d_fourcc_t));
                dsi_size = p_entry->dsi_size;

                p_crypt = &p_entry->crypt_info;
            }
            else if (MP4D_FOURCC_EQ(stream_info.hdlr,"subt")) 
            {
                mp4d_sampleentry_subtdata_t  *p_entry = &sampleentry.subt;

                logout(LOG_VERBOSE_LVL_INFO, "    Sample entry #%u: data_reference_index = %u\n", s, p_entry->data_reference_index);
                logout(LOG_VERBOSE_LVL_INFO, "    subtitle namespace = %s \n", p_entry->subt_namespace);
                logout(LOG_VERBOSE_LVL_INFO, "    subtitle schema location = %s \n", p_entry->schema_location);
                logout(LOG_VERBOSE_LVL_INFO, "    subtitle image mime type = %s \n", p_entry->image_mime_type);

            }
            else
            {
                logout(LOG_VERBOSE_LVL_INFO, "    Unknown handler %c%c%c%c\n",
                    stream_info.hdlr[0],
                    stream_info.hdlr[1],
                    stream_info.hdlr[2],
                    stream_info.hdlr[3]);
            }
            if (p_dsi)
            {
                logout(LOG_VERBOSE_LVL_INFO, "    Sample entry #%u: dsi_type = %c%c%c%c\n", s, 
                    dsi_type[0],dsi_type[1],dsi_type[2],dsi_type[3]);
                logout(LOG_VERBOSE_LVL_INFO, "    Sample entry #%u: dsi = ", s);
                for (k=0; k<dsi_size; k++) {
                    logout(LOG_VERBOSE_LVL_INFO, "%02x", p_dsi[k]);
                }
                logout(LOG_VERBOSE_LVL_INFO, "\n");
            }

            if (p_crypt != NULL && p_crypt->method != 0)
            {
                logout(LOG_VERBOSE_LVL_INFO, "    Sample entry #%u: crypt.method = %u\n", s, p_crypt->method);
                logout(LOG_VERBOSE_LVL_INFO, "    Sample entry #%u: crypt.iv_size = %u\n", s, p_crypt->iv_size);
                logout(LOG_VERBOSE_LVL_INFO, "    Sample entry #%u: crypt.key_id = ", s);
                for (k=0; k<16; k++) {
                    logout(LOG_VERBOSE_LVL_INFO, "%02x", p_crypt->key_id[k]);
                }
                logout(LOG_VERBOSE_LVL_INFO, "\n");
            }
            /* for encrypted stream, codec name in stream_info is not accurate, overwrite it by dis_type_cry in sampleentry */
            if (MP4D_FOURCC_EQ(stream_info.codec, "enca") || MP4D_FOURCC_EQ(stream_info.codec, "encv"))
            {
                mp4d_sampleentry_audio_t *p_entry = &sampleentry.soun;
                MP4D_FOURCC_ASSIGN(stream_info.codec, p_entry->dsi_type_cry);
            }
        } /* for each dsi */

        /* bitrates */
        {
            int i = 0;
            int err_br = 0;
            while (err_br == 0)
            {
                err_br = p_movie->get_bitrate(p_movie, stream_num, i, &bitrate);

                if (err_br == 0)
                {
                    logout(LOG_VERBOSE_LVL_INFO, "    Bitrate #%d: %" PRIu32 " bps\n", i, bitrate);
                    i += 1;
                }
                else
                {
                    ASSURE( err_br == 2, ("Could not read bitrate") );
                }
            }
            ASSURE( i >= 1, ("No bit rate available!") );
        }

        /* Create a sink for this stream */
        if (!p_data->options.no_dump)
        {
            if ((
                (MP4D_FOURCC_EQ(stream_info.hdlr, "vide") && MP4D_FOURCC_EQ(stream_info.codec, "avc1")) ||
                (MP4D_FOURCC_EQ(stream_info.hdlr, "vide") && MP4D_FOURCC_EQ(stream_info.codec, "H264"))
                ) &&
                !p_data->options.raw_dump)
            {
                CHECK( h264_writer_new(&sink1, track_ID, stream_name, p_data->options.output_folder) );
                video_flag = 1;
            }
            else if ((
                (MP4D_FOURCC_EQ(stream_info.hdlr, "vide") && MP4D_FOURCC_EQ(stream_info.codec, "hvc1")) ||
                (MP4D_FOURCC_EQ(stream_info.hdlr, "vide") && MP4D_FOURCC_EQ(stream_info.codec, "hev1")) ||
                (MP4D_FOURCC_EQ(stream_info.hdlr, "vide") && MP4D_FOURCC_EQ(stream_info.codec, "HEVC"))
                ) &&
                !p_data->options.raw_dump)
            {
                CHECK( hevc_writer_new(&sink1, track_ID, stream_name, p_data->options.output_folder, p_data->options.dump_to_stdout) );
                video_flag = 1;
            }
            else if (
                (MP4D_FOURCC_EQ(stream_info.hdlr, "vide") && (MP4D_FOURCC_EQ(stream_info.codec, "dvav") || (MP4D_FOURCC_EQ(stream_info.codec, "dvhe"))) 
                ) &&
                !p_data->options.raw_dump)
            {
                CHECK( dv_el_writer_new(&sink1, track_ID, stream_name, (const char *)stream_info.codec, p_data->options.output_folder) );
                video_flag = 1;
            }

            if (!video_flag)
            {
                if ((MP4D_FOURCC_EQ(stream_info.codec, "mp4a") || MP4D_FOURCC_EQ(stream_info.codec, "AACL"))&&
                    (MP4D_FOURCC_EQ(stream_info.hdlr, "soun") || MP4D_FOURCC_EQ(stream_info.hdlr, "url "))
                    && !p_data->options.raw_dump)
                {
                    CHECK( adts_writer_new(&sink1, track_ID, stream_name, p_data->options.output_folder) );
                }
                else if ((MP4D_FOURCC_EQ(stream_info.codec, "ac-4") && MP4D_FOURCC_EQ(stream_info.hdlr, "soun"))
                    && !p_data->options.raw_dump)
                {
                    CHECK( ac4_writer_new(&sink1, track_ID, stream_name, p_data->options.output_folder) );
                }
                else if ((MP4D_FOURCC_EQ(stream_info.codec, "ec-3") && MP4D_FOURCC_EQ(stream_info.hdlr, "soun"))
                    && !p_data->options.raw_dump)
                {
                    CHECK( ddp_writer_new(&sink1, track_ID, stream_name, p_data->options.output_folder) );
                }
                else if(MP4D_FOURCC_EQ(stream_info.hdlr, "subt") || MP4D_FOURCC_EQ(stream_info.codec, "stpp"))
                {
                    CHECK( subt_writer_new(&sink1, track_ID, stream_name, p_data->options.output_folder) );
                }
                else
                {
                    CHECK( es_writer_new(&sink1, track_ID, stream_name, p_data->options.output_folder) );
                }
            }

            CHECK( p_movie->fragment_stream_new(p_movie, stream_num, stream_name, bitrate, &mp4_source) );
            CHECK( player_set_track(p_data->player, track_ID, stream_name, bitrate, p_movie, mp4_source, sink1, polarssl_flag) );
        }
        if (p_data->options.show_samples)
        {
            CHECK( sample_print_new(&sink2, stream_info.time_scale, track_ID, stream_name) );
            if (mp4_source == NULL)
            {
                CHECK( p_movie->fragment_stream_new(p_movie, stream_num, stream_name, bitrate, &mp4_source) );
            }
            CHECK( player_set_track(p_data->player, track_ID, stream_name, bitrate, p_movie, mp4_source, sink2, polarssl_flag) );
        }


    } /* for each stream */

cleanup:
    free(stream_name);
    return err;
}

static int
    on_moov(app_data_t *p_d, mp4d_demuxer_ptr_t p_dmux)
{
    int err = 0;
    app_data_t *p_data = p_d;
    FILE *item_file = NULL;

    if (strlen(p_data->options.item) > 0)
    {
        uint16_t item_ID = 1;
        const unsigned char *p_item;
        uint64_t item_size;

        char filename[255];
        uint32_t idx;

        CHECK( mp4d_demuxer_get_meta_item(p_dmux, item_ID, &p_item, &item_size) );

        if(NULL != p_data->options.output_folder)
        {
            strcpy(filename, p_data->options.output_folder);
            strcat(filename, p_data->options.item);
        }
        else
        {
            strcpy(filename, p_data->options.item);
        }

        for(idx = 0; idx < strlen(p_data->options.item); idx ++)
        {
            if(DIRECTORY_SEPARATOR == p_data->options.item[idx])
            {
                strcpy(filename, p_data->options.item);
                break;
            }
        }

        item_file = fopen(filename, "wb");
        ASSURE( item_file != NULL, ("Failed to open '%s' for writing", p_data->options.item) );
        ASSURE( (size_t) item_size == item_size, ("Item size (%" PRIu64 " bytes) is too large", item_size) );
        ASSURE( fwrite(p_item, (size_t) item_size, 1, item_file) == 1,
            ("Failed to write %" PRIu64 " bytes iloc item to '%s'", item_size, p_data->options.item) );
    }

    {
        mp4d_id3v2_tag_t id3v2_tag;
        uint32_t idx = 0;
        int e = mp4d_demuxer_get_id3v2_tag(p_dmux, idx++, &id3v2_tag);

        ASSURE( e == MP4D_NO_ERROR ||
            e == MP4D_E_INFO_NOT_AVAIL,
            ("Unexpected error (%d) when reading ID3v2 tags", e) );

        while (e == MP4D_NO_ERROR)
        {
            char lang_str[4] = {'\0'};

            lang_str[0] = ((id3v2_tag.lang>>10) & 0x1f) + 0x60;
            lang_str[1] = ((id3v2_tag.lang>>5) & 0x1f) + 0x60;
            lang_str[2] = ((id3v2_tag.lang) & 0x1f) + 0x60;
            logout(LOG_VERBOSE_LVL_INFO, "ID3v2 tag %d: size = %" PRIu64 "\n", idx - 1, id3v2_tag.size);
            logout(LOG_VERBOSE_LVL_INFO, "ID3v2 tag %d: language = '%s'\n", idx - 1, lang_str);

            md_write_id32(&id3v2_tag, stdout);

            e = mp4d_demuxer_get_id3v2_tag(p_dmux, idx++, &id3v2_tag);

            ASSURE( e == MP4D_NO_ERROR ||
                e == MP4D_E_IDX_OUT_OF_RANGE,
                ("Unexpected error (%d) when reading ID3v2 tags", e) );
        }

    }

    handle_metadata(p_dmux);

cleanup:
    if (item_file != NULL)
    {
        fclose(item_file);
    }

    return err;
}

/* Process top-level boxes until first moov (inclusive)
*
* @return error 1 unexpected
*               0 success (also if no moov found)
*/
static int
    movie_validation(
    app_data_t *p_data,
    movie_t p_movie       /**< handle to presentation */
    )
{
    fragment_reader_t mp4_source = NULL;
    mp4d_fourcc_t type;
    uint32_t bitrate = 0;
    int err;
    int found = 0;
    CHECK( p_movie->fragment_stream_new(p_movie, UINT32_MAX, NULL, bitrate, &mp4_source) ); /* file-based */
    do
    {
        err = fragment_reader_next_atom(mp4_source);
        if (err == 0)
        {
            CHECK( mp4d_demuxer_get_type(mp4_source->p_dmux, &type) );

            if (MP4D_FOURCC_EQ(type, "moov"))
            {
                found = 1;
            }

            if      (MP4D_FOURCC_EQ(type, "ftyp"))   CHECK( on_ftyp(p_data, mp4_source->p_dmux) );
            else if (MP4D_FOURCC_EQ(type, "styp"))   CHECK( on_ftyp(p_data, mp4_source->p_dmux) );
            else if (MP4D_FOURCC_EQ(type, "pdin"))   CHECK( on_pdin(p_data, mp4_source->p_dmux) );
            else if (MP4D_FOURCC_EQ(type, "bloc"))   CHECK( on_bloc(p_data, mp4_source->p_dmux) );
            else if (MP4D_FOURCC_EQ(type, "meta"))   CHECK( on_meta(p_data, mp4_source->p_dmux) );
            else if (MP4D_FOURCC_EQ(type, "moov"))   CHECK( on_moov(p_data, mp4_source->p_dmux) );
        }
    } while (err == 0 && !MP4D_FOURCC_EQ(type, "moov"));

    if (!found)
    {
        err = 0;
        WARNING(("No 'moov' box found (not an ISO media file?)\n"));
    }

cleanup:
    if (mp4_source != NULL)
    {
        fragment_reader_destroy(mp4_source);
    }
    return err;
}

static int 
    process(app_data_t* data, movie_t p_movie)
{
    int err = 0;
    CHECK( player_new(&data->player) );

    CHECK( player_select_movie(data, p_movie) );

    if (data->options.time_ranges.start != -1.0f || data->options.time_ranges.end != -1.0f)
    {
        /* presentation time order */
        CHECK( player_play_time_range(data->player,
            &data->options.time_ranges.start,
            (data->options.time_ranges.end >= 0) ? &data->options.time_ranges.end : NULL) );
    }
    else
    {
        /* sample position order */
        CHECK( player_play_fragments(data->player, data->options.fragment_number) );
    }
cleanup:
    player_destroy(&data->player);
    return 0;
}

static int
    print_version(void)
{    
    const mp4d_version_t *version = mp4d_get_version();

    printf("Copyright (c) 2008-2017 Dolby Laboratories, Inc. All Rights Reserved\n");
    if (version->text != NULL)
    {
        logout(LOG_VERBOSE_LVL_INFO,"mp4demuxer version %" PRIu32 ".%" PRIu32 ".%" PRIu32 " %s built: %s\n",
            version->major, version->minor, version->patch, version->text, __DATE__);
        fprintf(stdout, "mp4demuxer version %" PRIu32 ".%" PRIu32 ".%" PRIu32 " %s built: %s\n",
            version->major, version->minor, version->patch, version->text, __DATE__);
    }
    else
    {
        logout(LOG_VERBOSE_LVL_INFO,"mp4demuxer version %" PRIu32 ".%" PRIu32 ".%" PRIu32 " built: %s\n",
            version->major, version->minor, version->patch, __DATE__);
        fprintf(stdout, "mp4demuxer version %" PRIu32 ".%" PRIu32 ".%" PRIu32 " built: %s\n",
            version->major, version->minor, version->patch, __DATE__);
    }

    return 0;
}

static void
    usage(void)
{
    fprintf(stdout, "\n");
    fprintf(stdout, "This tool can demux MP4 file format to elementary streams.\n");
    fprintf(stdout, "\nUsage:\n");
    fprintf(stdout, "    %s --input-file <input_file> [--output-folder<output_folder>] [--time-ranges <ranges>]\n", ProgramName);

    fprintf(stdout, "\nOption description:\n");
    fprintf(stdout, "    --input-file            Specifies the input file (.mp4) for demultiplex.\n");
    fprintf(stdout, "    --output-folder         Specifies the output folder path and name.\n");
    fprintf(stdout, "    --time-ranges           A time range (in seconds) to demultiplex.\n");
    fprintf(stdout, "    --version               Prints version information\n");
    fprintf(stdout, "    --help                  Displays help information\n");
    fprintf(stdout, "    --verbose               Displays More information for debugging.\n");

	/*
    fprintf(stdout, "    --item     Output file to write the iloc item_ID = 1\n");
    fprintf(stdout, "    --no-dump                         Do not dump payloads to elementary stream files.\n");
    fprintf(stdout, "    --dump-to-stdout       Dump elementary stream to stdout.\n");
    fprintf(stdout, "    --raw-dump                        If dumping, dump track payload to raw format.\n");
    fprintf(stdout, "    --show-samples                         Write samples information to stdout\n");
	*/

    fprintf(stdout, "\nExamples:\n");
    fprintf(stdout, "    1. Demux a mp4 file\n");
    fprintf(stdout, "      mp4demuxer --input-file input.mp4 --output-folder tmp\n\n");
    fprintf(stdout, "    2. Demux playloads of mp4 file with an indicated time range \n");
    fprintf(stdout, "      from 0s to 5.2s: mp4demuxer --input-file input.mp4 --output-folder tmp --time-ranges 0-5.2\n");
    fprintf(stdout, "      from 4s to end: mp4demuxer --input-file input.mp4 --output-folder tmp --time-ranges 4-\n\n");
}

static void
    default_options(options_t *options)
{
    options->output_folder = NULL;
    options->item = "";
    options->pdin_rates = "0,1000,10000,1000000";
    options->time_ranges.start = -1.0f;
    options->time_ranges.end = -1.0f;
    options->no_dump = 0;  /* default is to dump */
    options->dump_to_stdout = 0;  /* default is to file */
    options->raw_dump = 0;  /* default is to dump */
    options->show_samples = 0;  /* default is to dump */
    g_verbose_level = LOG_VERBOSE_LVL_COMPACT;
    options->fragment_number = 0;
}

static int
    create_output_folder(char *output_folder)
{   
    int len = (int)strlen(output_folder);
    if (DIRECTORY_SEPARATOR != output_folder[len -1])
    {
        output_folder[len] = DIRECTORY_SEPARATOR;
        output_folder[len + 1] = '\0';
    }
    if(!access(output_folder, 0))
    {
        return 0;
    }
    return mkdir(output_folder);
}

void 
	get_extension(const char *file_name,char *extension)  
{  
	int length = strlen(file_name); 
	int i = length - 1;

	while(i)  
	{  
		if (file_name[i] == '.')
		{
			break; 
		}
		i--;  
	}  

	if( i > 0)
	{
		strcpy(extension, file_name + i);  
	}
	else
	{
		strcpy(extension,"\0");  
	}  
}

static int
    parse_options(int argc, 
    const char *argv[],
    options_t *options /**< [out] */
    )
{
    int i;
    default_options(options);
    
    if (argc == 1)
    {
        printf("Error parsing command line, using '-h' for more info.\n");
        return -1;
    }
    
    for(i=1; i < argc; i++)
    {
        const char* option = argv[i];
        if(!strcmp(option, "--input-file")) 
        {
            char ext[10];
			if (i + 1 >= argc || argv[i + 1][0] == '-'){
				printf("Error: invalid input file found.\n");
				return -1;
			}

            options->filename = argv[++i];
			get_extension(options->filename, ext);
            if (strcmp(ext, ".mp4") && strcmp(ext, ".m4a") && strcmp(ext, ".m4v"))
            {
                return -1;
            }
        }
        else if (!strcmp(option, "--output-folder"))
        {
			if (i + 1 >= argc || argv[i + 1][0] == '-'){
				printf("Error: invalid output folder found.\n");
				return -1;
			}

            options->output_folder = argv[++i];
        }
        else if (!strcmp(option, "--item"))
        {
			if (i + 1 >= argc || argv[i + 1][0] == '-'){
				printf("Error: invalid item found.\n");
				return -1;
			}

            options->item = argv[++i];
        }
        else if (!strcmp(option, "--time-ranges"))
        {
            char range[20], *tmp;
			if (i + 1 >= argc || argv[i + 1][0] == '-'){
				printf("Error: invalid time range found.\n");
				return -1;
			}
            strcpy(range, argv[++i]);
            tmp = strtok(range, "-");
            if (tmp)
            {
                sscanf(tmp, "%f", &options->time_ranges.start);
                tmp = strtok(NULL, "-");
                if (tmp)
                {
                    float end = 0.0f;
                    sscanf(tmp , "%f",  &end);
                    if (end > options->time_ranges.start)
                    {
                        options->time_ranges.end = end;
                    }
                }
            }
        }
        else if (!strcmp(option, "--no-dump)"))
        {
            options->no_dump = 1;
        }
        else if (!strcmp(option, "--dump-to-stdout"))
        {
            options->dump_to_stdout = 1;
        }
        else if (!strcmp(option, "--raw-dump"))
        {
            options->raw_dump = 1;
        }
        else if (!strcmp(option, "--show-samples"))
        {
            options->show_samples = 1;
        }
        else if (!strcmp(option, "--version"))
        {
            print_version();
        }
        else if (!strcmp(option, "--verbose"))
        {
			if (argv[i + 1] && argv[i + 1][0] != '-'){
				sscanf(argv[++i], "%d", &g_verbose_level);
			}
			else{
				g_verbose_level = LOG_VERBOSE_LVL_INFO;
			}
        }
        /*
		else if (!strcmp(option, "--fragment-number"))
        {
            sscanf(argv[++i], "%d", &options->fragment_number);
        }
		*/
        else if (!strcmp(option, "-h") || !strcmp(option, "--help"))
        {
            usage();
            return 0;
        }
        else
        {
            printf("Error: unknown option found: %s\n", option);
			return -1;
        }
    }

    if (options->filename == NULL)
    {
        return -1;
    }

    if (options->output_folder)
    {
        strcpy(options->output_path,options->output_folder);
        if (strcmp(options->output_folder, "./") && create_output_folder(options->output_path))
        {
            return -1;
        }
    }
    else
    {
        strcpy(options->output_path, "./");
    }

    options->output_folder = options->output_path;

    return 0;
}

int
    main(int argc, const char* argv[])
{
    app_data_t data;
    movie_t p_movie = NULL;
	int err = 0;

	memset(&data, 0, sizeof(app_data_t));
	CHECK( parse_options(argc, argv, &data.options) );
	if (data.options.filename){
		CHECK( movie_new(data.options.filename, &p_movie) );
		CHECK( movie_validation(&data, p_movie) );
		CHECK( process(&data, p_movie));
	}
cleanup:
    movie_destroy(p_movie);

    return err;
}
