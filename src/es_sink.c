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

#include "es_sink.h"

#include "util.h"

#ifdef _MSC_VER
#define PRId64 "I64d"
#define PRIu64 "I64u"
#define PRIu32 "I32u"
#define PRId32 "ld"
#define PRIu16 "hu"
#define PRIu8 "u"
#define PRIz "Iu"
#define snprintf sprintf_s  /* snprintf is C99 */
#else
#include <inttypes.h>
#define PRIz "zu"     /* not specified by C99 */
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifdef _MSC_VER
#include <io.h>
#include <fcntl.h>
#else
#include <stdbool.h>
#include <unistd.h>
#define _unlink unlink
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif


typedef struct es_writer_t_
{
    struct es_sink_t_ base;

    FILE *out_file;
} *es_writer_t;

static void
es_writer_destroy(es_sink_t p_es_sink)
{
    es_writer_t p_es_writer = (es_writer_t) p_es_sink;

    if (p_es_writer->out_file != NULL)
    {
        fclose(p_es_writer->out_file);
    }
    free(p_es_sink);
}

/** @brief write bits to buffer

    @return new position
*/
static uint32_t
write_bits(uint32_t pos,   /* buffer offset (in bits) */
           unsigned char *buffer, 
           uint32_t num_bits,   /* must be <= 16 */
           uint16_t data)
{
    uint32_t byte = pos / 8;
    uint8_t bit = pos % 8;

    while (num_bits > 0)
    {
        if ((data >> (num_bits - 1)) & 1)
        {
            buffer[byte] |= 1 << (7 - bit);
        }
        else
        {
            buffer[byte] &= ~(1 << (7 - bit));
        }

        bit++;
        if (bit == 8)
        {
            bit = 0;
            byte++;
        }

        num_bits--;
        pos++;
    }

    return pos;
}

/** Reads up to 32 bits, updates pos
 */
static uint32_t
read_bits(uint32_t *pos,   /* buffer offset in bits */
          const unsigned char *buffer, 
          uint32_t num_bits)
{
    uint32_t data = 0;
    uint32_t byte = *pos / 8;
    uint32_t bit = *pos % 8;

    while (num_bits > 0)
    { 
        data = data << 1;
        data |= ((buffer[byte] >> (7 - bit)) & 1);
        
        num_bits--;
        (*pos)++;
        bit++;
        if (bit == 8)
        {
            bit = 0;
            byte++;
        }
    }

    return data;
}

static int
es_writer_sample_entry(es_sink_t p_es_sink,
                       const mp4d_sampleentry_t *sample_entry)
{
    (void) p_es_sink; /* unused */
    (void) sample_entry;
    return 0;
}

static int
es_writer_sample_ready(es_sink_t p_es_sink,
                       const mp4d_sampleref_t *sample,
                       const unsigned char *payload)
{
    es_writer_t p_es_writer = (es_writer_t) p_es_sink;
    int err = 0;

    ASSURE( fwrite(payload, sample->size, 1, p_es_writer->out_file) == 1,
            ("Failed to write %" PRIu32 " bytes to output", sample->size) );

cleanup:
    return err;
}

int
ddp_writer_new(es_sink_t *p_es_sink, uint32_t track_ID, const char *stream_name, const char *output_folder)
{
    es_writer_t p_es_writer;
    int err = 0;

    *p_es_sink = malloc(sizeof(struct es_writer_t_));
    (*p_es_sink)->sample_ready = es_writer_sample_ready;
    (*p_es_sink)->subsample_ready = NULL;
    (*p_es_sink)->sample_entry = es_writer_sample_entry;
    (*p_es_sink)->destroy = es_writer_destroy;

    p_es_writer = (es_writer_t) *p_es_sink;

    {
        char filename[255];
        int n = sizeof filename;
        int folder_len = 0;

        if (NULL != output_folder)
        {
            snprintf(filename, n, "%s", output_folder);
            folder_len = (int)strlen(output_folder);
            n -=  folder_len;
        }
        
        if (track_ID > 0)
        {
            ASSURE( snprintf(filename + folder_len, n, "out_%" PRIu32 ".ec3", track_ID) < n,
                    (" "));
        }
        else
        {
            ASSURE( snprintf(filename + folder_len, n, "%s.ec3", stream_name) < n,
                    (" "));
        }
        p_es_writer->out_file = fopen(filename, "wb");
        ASSURE( p_es_writer->out_file != NULL, ("Failed to open %s for writing", filename) );
        logout(LOG_VERBOSE_LVL_INFO,"Writing track_ID = %" PRIu32 " to %s\n", track_ID, filename);
    }

cleanup:
    return err;
}

int
es_writer_new(es_sink_t *p_es_sink, uint32_t track_ID, const char *stream_name, const char *output_folder)
{
    es_writer_t p_es_writer;
    int err = 0;

    *p_es_sink = malloc(sizeof(struct es_writer_t_));
    (*p_es_sink)->sample_ready = es_writer_sample_ready;
    (*p_es_sink)->subsample_ready = NULL;
    (*p_es_sink)->sample_entry = es_writer_sample_entry;
    (*p_es_sink)->destroy = es_writer_destroy;

    p_es_writer = (es_writer_t) *p_es_sink;

    {
        char filename[255];
        int n = sizeof filename;
        int folder_len = 0;

        if (NULL != output_folder)
        {
            snprintf(filename, n, "%s", output_folder);
            folder_len = (int)strlen(output_folder);
            n -=  folder_len;
        }
        
        if (track_ID > 0)
        {
            ASSURE( snprintf(filename + folder_len, n, "out_%" PRIu32 ".dat", track_ID) < n,
                    (" "));
        }
        else
        {
            ASSURE( snprintf(filename + folder_len, n, "%s.dat", stream_name) < n,
                    (" "));
        }
        p_es_writer->out_file = fopen(filename, "wb");
        ASSURE( p_es_writer->out_file != NULL, ("Failed to open %s for writing", filename) );
        logout(LOG_VERBOSE_LVL_INFO,"Writing track_ID = %" PRIu32 " to %s\n", track_ID, filename);
    }

cleanup:
    return err;
}

typedef struct ac4_writer_t_
{
    struct es_sink_t_ base;

    uint16_t sync_word;
    uint16_t frame_len_24bit_flag;
    uint16_t crc;

    FILE *out_file;
} *ac4_writer_t;

static void
ac4_writer_destroy(es_sink_t p_es_sink)
{
    ac4_writer_t p_ac4_writer = (ac4_writer_t) p_es_sink;

    if (p_ac4_writer->out_file != NULL)
    {
        fclose(p_ac4_writer->out_file);
    }
    free(p_es_sink);
}

static int
ac4_writer_sample_entry(es_sink_t p_es_sink,
                       const mp4d_sampleentry_t *sample_entry)
{
    (void) p_es_sink; /* unused */
    (void) sample_entry;
    return 0;
}

static int
ac4_writer_sample_ready(es_sink_t p_es_sink,
                       const mp4d_sampleref_t *sample,
                       const unsigned char *payload)
{
    ac4_writer_t p_ac4_writer = (ac4_writer_t) p_es_sink;
    int err = 0;
    uint8_t temp;

    if (p_ac4_writer->sync_word == 0xac40)
    {
        /* write sync words as big endian*/
        temp = (uint8_t)(p_ac4_writer->sync_word >> 8);
        fwrite(&temp, 1, 1, p_ac4_writer->out_file);
        temp = (uint8_t)(p_ac4_writer->sync_word & 0xff);
        fwrite(&temp, 1, 1, p_ac4_writer->out_file);

        /* write 0xffff*/
        fwrite(&(p_ac4_writer->frame_len_24bit_flag), sizeof(uint16_t), 1, p_ac4_writer->out_file);

        /* write frame length as big endian*/
        temp = (uint8_t)(sample->size >> 16);
        fwrite(&temp, 1, 1, p_ac4_writer->out_file);
        temp = (uint8_t)((sample->size >> 8) & 0xff);
        fwrite(&temp, 1, 1, p_ac4_writer->out_file);
        temp = (uint8_t)(sample->size & 0xff);
        fwrite(&temp, 1, 1, p_ac4_writer->out_file);

        ASSURE( fwrite(payload, sample->size, 1, p_ac4_writer->out_file) == 1,
                ("Failed to write %" PRIu32 " bytes to output", sample->size) );
    }
    else if (p_ac4_writer->sync_word == 0xac41)
    {
        ASSURE(0,("Currently mp4demuxer don't support output ac-4 frame with CRC!" ));
    }
cleanup:
    return err;
}

int
ac4_writer_new(es_sink_t *p_es_sink, uint32_t track_ID, const char *stream_name, const char *output_folder)
{
    ac4_writer_t p_ac4_writer;
    int err = 0;

    *p_es_sink = malloc(sizeof(struct ac4_writer_t_));
    (*p_es_sink)->sample_ready = ac4_writer_sample_ready;
    (*p_es_sink)->subsample_ready = NULL;
    (*p_es_sink)->sample_entry = ac4_writer_sample_entry;
    (*p_es_sink)->destroy = ac4_writer_destroy;

    p_ac4_writer = (ac4_writer_t) *p_es_sink;

    {
        char filename[255];
        int n = sizeof filename;
        int folder_len = 0;

        if (NULL != output_folder)
        {
            snprintf(filename, n, "%s", output_folder);
            folder_len = (int)strlen(output_folder);
            n -=  folder_len;
        }
        
        if (track_ID > 0)
        {
            ASSURE( snprintf(filename + folder_len, n, "out_%" PRIu32 ".ac4", track_ID) < n,
                    (" "));
        }
        else
        {
            ASSURE( snprintf(filename + folder_len, n, "%s.ac4", stream_name) < n,
                    (" "));
        }
        p_ac4_writer->out_file = fopen(filename, "wb");
        ASSURE( p_ac4_writer->out_file != NULL, ("Failed to open %s for writing", filename) );
        logout(LOG_VERBOSE_LVL_INFO,"Writing track_ID = %" PRIu32 " to %s\n", track_ID, filename);
    }

    /* currently we only support output ac4 without crc words */
    p_ac4_writer->sync_word = 0xac40;
    p_ac4_writer->frame_len_24bit_flag = 0xffff;
    p_ac4_writer->crc = 0;
cleanup:
    return err;
}

typedef struct sample_print_t_
{
    struct es_sink_t_ base;

    uint32_t track_ID;
    char *stream_name;
    uint32_t media_time_scale;

} * sample_print_t;

static void
sample_print_destroy(es_sink_t p_es_sink)
{
    sample_print_t p_sample_print = (sample_print_t) p_es_sink;

    if (p_es_sink != NULL)
    {
        free(p_sample_print->stream_name);
    }
    free(p_es_sink);
}

static int
sample_print_sample_entry(es_sink_t p_es_sink,
                       const mp4d_sampleentry_t *sample_entry)
{
    (void) p_es_sink;
    (void) sample_entry;
    return 0;
}

static int
sample_print_sample_ready(es_sink_t p_es_sink,
                          const mp4d_sampleref_t *sample,
                          const unsigned char *payload)
{
    sample_print_t p_sample_print = (sample_print_t) p_es_sink;

    (void) payload;
    logout(LOG_VERBOSE_LVL_INFO,"DEMUX: Sample: ");
    if (p_sample_print->stream_name != NULL)
    {
        logout(LOG_VERBOSE_LVL_INFO,"track = '%s'", p_sample_print->stream_name);
    }
    else
    {
        logout(LOG_VERBOSE_LVL_INFO,"track_ID = %" PRIu32, p_sample_print->track_ID);
    }
    logout(LOG_VERBOSE_LVL_INFO,": dts = %" PRIu64
           ", cts = %" PRIu64
           ", pts = %.3fs"
           ", flags = 0x%x"
           ", size = %" PRIu32
           ", SDI = %" PRIu32
           ", position = %" PRIu64 " - %" PRIu64
           ", subs = %" PRIu32
           ", pic_type = %" PRIu32
           ", dependency_level = %" PRIu32
           "\n",
           sample->dts,
           sample->cts,
           sample->pts / (double) p_sample_print->media_time_scale,
           sample->flags,
           sample->size,
           sample->sample_description_index,
           sample->pos, sample->pos + sample->size - 1,
           sample->num_subsamples,
           sample->pic_type,
           sample->dependency_level);
    
    return 0;
}

static int
sample_print_subsample_ready(uint32_t subsample_index,
                             es_sink_t p_es_sink,
                             const mp4d_sampleref_t *p_sample,
                             const unsigned char *payload,
                             uint64_t offset,
                             uint32_t size)
{
    sample_print_t p_sample_print = (sample_print_t) p_es_sink;

    (void) payload;
    if (p_sample->num_subsamples > 1)
    {
        logout(LOG_VERBOSE_LVL_INFO,"DEMUX: Subsample: track_ID = %" PRIu32
               ": sample_pos = %" PRIu64 
               ", position = %" PRIu64
               ", size = %" PRIu32
               ", subsample index = %" PRIu32
               "\n",
               p_sample_print->track_ID,
               p_sample->pos,
               offset,
               size);
    }

    return 0;
}

int
sample_print_new(es_sink_t *p_es_sink, uint32_t media_time_scale, uint32_t track_ID, const char *stream_name)
{
    int err = 0;
    sample_print_t p_sample_print;

    *p_es_sink = malloc(sizeof(struct sample_print_t_));
    (*p_es_sink)->sample_ready = sample_print_sample_ready;
    (*p_es_sink)->subsample_ready = sample_print_subsample_ready;
    (*p_es_sink)->sample_entry = sample_print_sample_entry;
    (*p_es_sink)->destroy = sample_print_destroy;

    p_sample_print = (sample_print_t) *p_es_sink;

    p_sample_print->track_ID = track_ID;
    if (stream_name != NULL)
    {
        p_sample_print->stream_name = string_dup(stream_name);
        ASSURE( p_sample_print->stream_name != NULL, ("Allocation failure") );
    }
    else
    {
        p_sample_print->stream_name = NULL;
    }
    p_sample_print->media_time_scale = media_time_scale;

cleanup:
    return err;
}
    

typedef struct adts_writer_t_
{
    struct es_sink_t_ base;

    FILE *out_file;
    uint32_t track_ID;  /* For messaging */

    struct
    {
        uint16_t sample_description_index;
        uint8_t AOT;
        uint8_t frequency_index;
        uint8_t channel_config;
    } *sample_entries;              /* array of sample descriptions */
    uint32_t num_sample_entries;    /* array size */

} *adts_writer_t;


static void
adts_writer_destroy(es_sink_t p_es_sink)
{
    adts_writer_t p_adts_writer = (adts_writer_t) p_es_sink;

    if (p_adts_writer->out_file != NULL)
    {
        fclose(p_adts_writer->out_file);
    }
    free(p_adts_writer->sample_entries);
    free(p_es_sink);
}

static uint32_t adts_writer_get_freqidx(uint32_t freq)
{
    uint32_t freq_idx = 0;
    uint32_t i;
    static const uint32_t FrequencyTable[] = { 96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
                                               16000, 12000, 11025, 8000, 7350, 0, 0, 0 };

    for (i = 0; i < sizeof(FrequencyTable)/sizeof(FrequencyTable[0]); i++)
    {
        if (freq == FrequencyTable[i])
        {
            freq_idx = i;
            break;
        }
    }

    return freq_idx;
}

static int
adts_writer_sample_entry(es_sink_t p_es_sink,
                       const mp4d_sampleentry_t *sample_entry)
{
    const mp4d_sampleentry_audio_t *p_entry = &sample_entry->soun;
    adts_writer_t p_adts_writer = (adts_writer_t) p_es_sink;
    uint32_t pos = 0;
    int err = 0;
    uint32_t i, size, next_byte;
    int url_flag;

    p_adts_writer->num_sample_entries++;
    p_adts_writer->sample_entries = realloc(p_adts_writer->sample_entries,
                                            p_adts_writer->num_sample_entries *
                                            sizeof(*p_adts_writer->sample_entries));

    i = p_adts_writer->num_sample_entries - 1;

    /* SDI counts from 1 */
    p_adts_writer->sample_entries[i].sample_description_index = p_adts_writer->num_sample_entries;

    if (MP4D_FOURCC_EQ(p_entry->dsi_type, "PIFF"))
    {
        /* No CodecPrivateData for PIFF(AACL) format */
        p_adts_writer->sample_entries[i].AOT = 2;
        p_adts_writer->sample_entries[i].frequency_index = adts_writer_get_freqidx(p_entry->samplerate);
        p_adts_writer->sample_entries[i].channel_config = (uint8_t)p_entry->channelcount;
    }
    else
    {
        /* p_entry->dsi is the esds box. Parse through, in order to read
           the Audio Specific Config (should this parsing move to the demuxer (excl ASC))?
        */
    
        ASSURE( p_entry->dsi != NULL, ("Missing decoder specific info") );
        logout(LOG_VERBOSE_LVL_INFO,"version = %d\n", read_bits(&pos, p_entry->dsi, 24));
        logout(LOG_VERBOSE_LVL_INFO,"flags = %d\n",   read_bits(&pos, p_entry->dsi, 8));  

        /* ES_Descriptor */
        logout(LOG_VERBOSE_LVL_INFO,"desc tag = %d\n", read_bits(&pos, p_entry->dsi, 8));
        size = 0;
        do
        {
            next_byte = read_bits(&pos, p_entry->dsi, 1);
            size = (size << 7) | read_bits(&pos, p_entry->dsi, 7);
        } while (next_byte);
        logout(LOG_VERBOSE_LVL_INFO,"desc size = %d\n", size);
    
        logout(LOG_VERBOSE_LVL_INFO,"ES_ID = %d\n", read_bits(&pos, p_entry->dsi, 16));
        ASSURE( read_bits(&pos, p_entry->dsi, 1) == 0, ("streamDependenceFlag is not 0") );
        url_flag = read_bits(&pos, p_entry->dsi, 1);
        logout(LOG_VERBOSE_LVL_INFO,"URL_Flag = %d\n", url_flag);
        ASSURE( read_bits(&pos, p_entry->dsi, 1) == 0, ("OCRstreamFlag is not 0") );
        logout(LOG_VERBOSE_LVL_INFO,"streamPriority = %d\n", read_bits(&pos, p_entry->dsi, 5));
        if (url_flag)
        {
            uint32_t URLlength = read_bits(&pos, p_entry->dsi, 8);
            logout(LOG_VERBOSE_LVL_INFO,"URLlength = %d\n", URLlength);
            read_bits(&pos, p_entry->dsi, 8 * URLlength);
        }

        /* DecoderConfigDescriptor */
        logout(LOG_VERBOSE_LVL_INFO,"DecoderConfigDescriptorTag = %d\n", read_bits(&pos, p_entry->dsi, 8));
        size = 0;
        do
        {
            next_byte = read_bits(&pos, p_entry->dsi, 1);
            size = (size << 7) | read_bits(&pos, p_entry->dsi, 7);
        } while (next_byte);
        logout(LOG_VERBOSE_LVL_INFO,"DecoderConfigDescriptor size = %d\n", size);

        logout(LOG_VERBOSE_LVL_INFO,"objectTypeIndication = 0x%x\n", read_bits(&pos, p_entry->dsi, 8));
        logout(LOG_VERBOSE_LVL_INFO,"streamType = 0x%x\n", read_bits(&pos, p_entry->dsi, 6));
        logout(LOG_VERBOSE_LVL_INFO,"upStream = %d\n", read_bits(&pos, p_entry->dsi, 1));
        logout(LOG_VERBOSE_LVL_INFO,"reserved = %d\n", read_bits(&pos, p_entry->dsi, 1));
        logout(LOG_VERBOSE_LVL_INFO,"bufferSizeDB = %d\n", read_bits(&pos, p_entry->dsi, 24));
        logout(LOG_VERBOSE_LVL_INFO,"maxBitRate = %d\n", read_bits(&pos, p_entry->dsi, 32));
        logout(LOG_VERBOSE_LVL_INFO,"avgBitRate = %d\n", read_bits(&pos, p_entry->dsi, 32));

        /* DecoderSpecificInfo (header + audio specific config) */
        logout(LOG_VERBOSE_LVL_INFO,"DecSpecificInfoTag = %d\n", read_bits(&pos, p_entry->dsi, 8));
        size = 0;
        do
        {
            next_byte = read_bits(&pos, p_entry->dsi, 1);
            size = (size << 7) | read_bits(&pos, p_entry->dsi, 7);
        } while (next_byte);
        logout(LOG_VERBOSE_LVL_INFO,"DecSpecificInfo size = %d\n", size);

        p_adts_writer->sample_entries[i].AOT = read_bits(&pos, p_entry->dsi, 5);
        logout(LOG_VERBOSE_LVL_INFO,"track_ID %d: esds: AOT = %d\n", 
               p_adts_writer->track_ID,
               p_adts_writer->sample_entries[i].AOT);
        ASSURE( p_adts_writer->sample_entries[i].AOT < 31,
                ("multi-byte AOT (%" PRIu8 ") is unsupported", p_adts_writer->sample_entries[i].AOT) );

        p_adts_writer->sample_entries[i].frequency_index = read_bits(&pos, p_entry->dsi, 4);
        logout(LOG_VERBOSE_LVL_INFO,"Frequency index = %d\n", p_adts_writer->sample_entries[i].frequency_index);
        ASSURE( p_adts_writer->sample_entries[i].frequency_index < 15, 
                ("explicit frequency (%" PRIu8 " is unsupported", p_adts_writer->sample_entries[i].frequency_index) );

        p_adts_writer->sample_entries[i].channel_config = read_bits(&pos, p_entry->dsi, 4);
        logout(LOG_VERBOSE_LVL_INFO,"Channel config = %d\n", p_adts_writer->sample_entries[i].channel_config);
    }

cleanup:
    return err;
}

static int
adts_writer_sample_ready(es_sink_t p_es_sink,
                         const mp4d_sampleref_t *sample,
                         const unsigned char *payload)
{
    adts_writer_t p_adts_writer = (adts_writer_t) p_es_sink;
    unsigned char header[7] = {0, 0, 0, 0, 0, 0, 0};
    uint32_t pos = 0;
    int err = 0;
    uint32_t i;

    /* Verify sample description index */
    for (i = 0; i < p_adts_writer->num_sample_entries; i++)
    {
        if (p_adts_writer->sample_entries[i].sample_description_index ==
            sample->sample_description_index)
        {
            break;
        }
    }
    ASSURE( i < p_adts_writer->num_sample_entries, 
            ("Sample description index %" PRIu16 " is unknown", p_adts_writer->sample_entries[i].sample_description_index) );

    pos = write_bits(pos, header, 12, 0xFFF);            /* sync */
    pos = write_bits(pos, header, 1, 0x1);               /* 0: MPEG-4, 1: MPEG-2 */
    pos = write_bits(pos, header, 2, 0x0);               /* layer */
    pos = write_bits(pos, header, 1, 0x1);               /* protection absent */
    pos = write_bits(pos, header, 2, p_adts_writer->sample_entries[i].AOT - 1);
    pos = write_bits(pos, header, 4, p_adts_writer->sample_entries[i].frequency_index);
    pos = write_bits(pos, header, 1, 0x0);               /* private stream */
    pos = write_bits(pos, header, 3, p_adts_writer->sample_entries[i].channel_config);
    pos = write_bits(pos, header, 1, 0x0);               /* originality */
    pos = write_bits(pos, header, 1, 0x0);               /* home */
    pos = write_bits(pos, header, 1, 0x0);               /* copyrighted stream */
    pos = write_bits(pos, header, 1, 0x0);               /* copyright start */
    pos = write_bits(pos, header, 13, 7 + sample->size); /* 7 + sample size */
    pos = write_bits(pos, header, 11, 0x7FF);            /* buffer fullness */
    pos = write_bits(pos, header, 2, 0x0);               /* number of AAC frames in ADTS frame minus 1 */
    assert( pos == 8 * (sizeof header) );

    ASSURE( fwrite(header, sizeof header, 1, p_adts_writer->out_file) == 1, ("fwrite %" PRIz " bytes failed", sizeof header) );
    ASSURE( fwrite(payload, sample->size, 1, p_adts_writer->out_file) == 1, ("Writing %" PRIu32 " bytes to output failed", sample->size) );

cleanup:
    return err;
}

int adts_writer_new(es_sink_t *p_es_sink, uint32_t track_ID, const char *stream_name, const char *output_folder)
{
    adts_writer_t p_adts_writer;
    int err = 0;

    *p_es_sink = malloc(sizeof(struct adts_writer_t_));
    (*p_es_sink)->sample_ready = adts_writer_sample_ready;
    (*p_es_sink)->subsample_ready = NULL;
    (*p_es_sink)->sample_entry = adts_writer_sample_entry;
    (*p_es_sink)->destroy = adts_writer_destroy;
    

    p_adts_writer = (adts_writer_t) *p_es_sink; 

    p_adts_writer->track_ID = track_ID;

    {
        char filename[255];
        int n = sizeof filename;
        int folder_len = 0;

        if (NULL != output_folder)
        {
            snprintf(filename, n, "%s", output_folder);
            folder_len = (int)strlen(output_folder);
            n -=  folder_len;
        }
        
        if (track_ID > 0)
        {
            ASSURE( snprintf(filename + folder_len, n, "out_%" PRIu32 ".adts", track_ID) < n,
                    (" "));
        }
        else
        {
            ASSURE( snprintf(filename + folder_len, n, "%s.adts", stream_name) < n,
                    (" "));
        }
        p_adts_writer->out_file = fopen(filename, "wb");
        ASSURE( p_adts_writer->out_file != NULL, ("Failed to open '%s' for writing", filename) );
        logout(LOG_VERBOSE_LVL_INFO,"Writing track_ID = %" PRIu32 " to %s\n", track_ID, filename);
    }

    p_adts_writer->sample_entries = NULL;
    p_adts_writer->num_sample_entries = 0;

cleanup:
    return err;
}


typedef struct h264_parameter_set_t_
{
    unsigned char * buf;
    uint32_t size;

} h264_parameter_set_t;

typedef struct h264_nal_unit_
{
    uint8_t nal_unit_type;
    uint64_t size;
} h264_nal_unit;

typedef struct h264_sample_entry_t_
{
    uint16_t sample_description_index;
    uint8_t size_field;
    uint8_t num_sps;
    h264_parameter_set_t sps[32];
    uint8_t num_pps;
    h264_parameter_set_t pps[256];
} h264_sample_entry_t;              /* array of sample descriptions */

typedef struct h264_writer_t_
{
    struct es_sink_t_ base;

    FILE *out_file;
    uint32_t track_ID;  /* For messaging */

    h264_sample_entry_t *sample_entries;              /* array of sample descriptions */
    uint32_t num_sample_entries;    /* array size */

    int wrote_sps_pps;  /* bool, used only if 1 sps and 1 pps */
} *h264_writer_t;


static void
h264_writer_destroy(es_sink_t p_es_sink)
{
    h264_writer_t p_h264_writer = (h264_writer_t) p_es_sink;

    if (p_h264_writer->out_file != NULL)
    {
        fclose(p_h264_writer->out_file);
        p_h264_writer->out_file = NULL;
    }

    {
        uint32_t i;
        for (i = 0; i < p_h264_writer->num_sample_entries; i++)
        {
            uint32_t k;
            for (k = 0; k < p_h264_writer->sample_entries[i].num_sps; k++) 
            {
                free( p_h264_writer->sample_entries[i].sps[k].buf );
            }
            for (k = 0; k < p_h264_writer->sample_entries[i].num_pps; k++) 
            {
                free( p_h264_writer->sample_entries[i].pps[k].buf );
            }
        }
    }
    free(p_h264_writer->sample_entries);
    free(p_es_sink);
}

static int
h264_writer_sample_entry(es_sink_t p_es_sink,
                       const mp4d_sampleentry_t *sample_entry)
{
    const mp4d_sampleentry_visual_t *p_entry = &sample_entry->vide;
    h264_writer_t p_h264_writer = (h264_writer_t) p_es_sink;
    uint32_t pos = 0;
    int err = 0;

    uint32_t i, k;

    p_h264_writer->num_sample_entries++;
    p_h264_writer->sample_entries = realloc(p_h264_writer->sample_entries,
                                            p_h264_writer->num_sample_entries *
                                            sizeof(*p_h264_writer->sample_entries));

    i = p_h264_writer->num_sample_entries - 1;

    /* SDI counts from 1 */
    p_h264_writer->sample_entries[i].sample_description_index = p_h264_writer->num_sample_entries;

    if (MP4D_FOURCC_EQ(p_entry->dsi_type, "PIFF"))
    {
        /* It would be cleaner to implement the logic in the PIFF reader,
         *  which converts the DSI to a dsi_type=avcC
         */

		/* (default). Real value is given by <QualityLevl NALUnitLengthField=...>*/
        p_h264_writer->sample_entries[i].size_field = 3; 
        
		p_h264_writer->sample_entries[i].num_sps = 1;
        logout(LOG_VERBOSE_LVL_INFO,"syncword = %d\n", read_bits(&pos, p_entry->dsi, 32));
    }
    else
    {
        logout(LOG_VERBOSE_LVL_INFO,"version = %d\n", read_bits(&pos, p_entry->dsi, 8));
        logout(LOG_VERBOSE_LVL_INFO,"profile = %d\n",   read_bits(&pos, p_entry->dsi, 8));
        logout(LOG_VERBOSE_LVL_INFO,"profile_compat = %d\n",   read_bits(&pos, p_entry->dsi, 8));
        logout(LOG_VERBOSE_LVL_INFO,"level = %d\n",   read_bits(&pos, p_entry->dsi, 8));

        p_h264_writer->sample_entries[i].size_field = 0x03 & read_bits(&pos, p_entry->dsi, 8);
        logout(LOG_VERBOSE_LVL_INFO,"length_minus_one = %d\n", p_h264_writer->sample_entries[i].size_field);

        p_h264_writer->sample_entries[i].num_sps    = 0x1f & read_bits(&pos, p_entry->dsi, 8);
    }
    logout(LOG_VERBOSE_LVL_INFO,"num_sps = %d\n", p_h264_writer->sample_entries[i].num_sps);

    for (k = 0; k < p_h264_writer->sample_entries[i].num_sps; k++) 
    {
        uint32_t length;
        uint32_t offset;
        if (MP4D_FOURCC_EQ(p_entry->dsi_type, "PIFF"))
        {
            offset = (pos >> 3);
            /* length is given by... searching for the next sync word */
            length = 0;
            while (offset + length + 4 < p_entry->dsi_size &&
                    !(p_entry->dsi[offset + length] == 0 &&
                      p_entry->dsi[offset + length + 1] == 0 &&
                      p_entry->dsi[offset + length + 2] == 0 &&
                      p_entry->dsi[offset + length + 3] == 1))
            {
                length += 1;
            }

            ASSURE( offset + length + 4 < p_entry->dsi_size, ("Missing syncword 0x0001 in CodecPrivateData") );
        }
        else
        {
            length = read_bits(&pos, p_entry->dsi, 16);
            offset = (pos >> 3);

            ASSURE(offset + length <= p_entry->dsi_size,
                    ("SPS offset + length (%" PRIu32 " + %" PRIu32") exceeds DSI size (%" PRIu64 ")",
                            offset, length, p_entry->dsi_size) );
        }

        p_h264_writer->sample_entries[i].sps[k].size = length;
        p_h264_writer->sample_entries[i].sps[k].buf = malloc(length);
        ASSURE(p_h264_writer->sample_entries[i].sps[k].buf != NULL, ("Allocation failure"));

        memcpy(p_h264_writer->sample_entries[i].sps[k].buf, p_entry->dsi + offset, length);

        pos += (length << 3);
    }

    if (MP4D_FOURCC_EQ(p_entry->dsi_type, "PIFF"))
    {
        p_h264_writer->sample_entries[i].num_pps = 1;
        logout(LOG_VERBOSE_LVL_INFO,"syncword = %d\n", read_bits(&pos, p_entry->dsi, 32));
    }
    else
    {
        p_h264_writer->sample_entries[i].num_pps = read_bits(&pos, p_entry->dsi, 8);
    }
    logout(LOG_VERBOSE_LVL_INFO,"num_pps = %d\n", p_h264_writer->sample_entries[i].num_pps);  

    for (k = 0; k < p_h264_writer->sample_entries[i].num_pps; k++) 
    {
        uint32_t length;
        uint32_t offset;
        if (MP4D_FOURCC_EQ(p_entry->dsi_type, "PIFF"))
        {
            offset = (pos >> 3);
            length = (uint32_t)p_entry->dsi_size - offset;
        }
        else
        {
            length = read_bits(&pos, p_entry->dsi, 16);
            offset = (pos >> 3);

            ASSURE(offset + length <= p_entry->dsi_size,
                    ("SPS offset + length (%" PRIu32 " + %" PRIu32") exceeds DSI size (%" PRIu64 ")",
                            offset, length, p_entry->dsi_size) );
        }

        ASSURE(offset + length <= p_entry->dsi_size,
               ("PPS offset + length (%" PRIu32 " + %" PRIu32") exceeds DSI size (%" PRIu64 ")",
                offset, length, p_entry->dsi_size) );
        p_h264_writer->sample_entries[i].pps[k].size = length;
        p_h264_writer->sample_entries[i].pps[k].buf = malloc(length);
        ASSURE(p_h264_writer->sample_entries[i].pps[k].buf != NULL, ("Allocation failure") );

        memcpy(p_h264_writer->sample_entries[i].pps[k].buf, p_entry->dsi+offset, length);
        pos += (length << 3);
    }

cleanup:
    return err;
}


static int
h264_write_sps_pps_nal(h264_sample_entry_t * p_entry, FILE* out_file)
{
    uint32_t i;
    unsigned char nal_header[5] = "\0\0\0\1";
    int err = 0;

    for (i = 0; i < p_entry->num_sps; i++) {
        ASSURE( fwrite(nal_header, 4, 1, out_file) == 1,
                ("Failed to fwrite NAL header to out file") );
        ASSURE( fwrite(p_entry->sps[i].buf, p_entry->sps[i].size, 1, out_file) == 1,
                ("Failed to fwrite %" PRIu32 " bytes SPS to out file", p_entry->sps[i].size) );
    }

    for (i = 0; i < p_entry->num_pps; i++) {
        ASSURE( fwrite(nal_header, 4, 1, out_file) == 1,
                ("Failed to fwrite NAL header to out file") );
        ASSURE( fwrite(p_entry->pps[i].buf, p_entry->pps[i].size, 1, out_file) == 1,
                ("Failed to fwrite %" PRIu32 " bytes PPS to out file", p_entry->pps[i].size) );
    }

cleanup:
    return err;
}


static int
h264_writer_sample_ready(es_sink_t p_es_sink,
                         const mp4d_sampleref_t *sample,
                         const unsigned char *payload)
{
    h264_writer_t p_h264_writer = (h264_writer_t) p_es_sink;
    uint32_t i;
    uint32_t in_pos; /* position in payload buffer */
    int err = 0;
    uint32_t size_field; /* in bytes */

    for (i = 0; i < p_h264_writer->num_sample_entries; i++)
    {
        if (p_h264_writer->sample_entries[i].sample_description_index ==
            sample->sample_description_index)
        {
            break;
        }
    }
    ASSURE( i < p_h264_writer->num_sample_entries, 
            ("Sample description index %" PRIu16 " is unknown", sample->sample_description_index) );

    size_field = p_h264_writer->sample_entries[i].size_field + 1;
    in_pos = 0;


    while (in_pos < sample->size)
    {
        uint32_t nal_size;
        uint8_t nal_unit_type;
        uint32_t pos = 0;

        nal_size = read_bits(&pos, payload + in_pos, size_field * 8);
        in_pos += size_field;
        pos = 0;

        ASSURE( in_pos + nal_size <= sample->size, 
                ("NAL size (%" PRIu32 ") exceeds remaining data (%" PRIu32 " bytes)",
                 nal_size, sample->size - in_pos) );

        nal_unit_type = read_bits(&pos, payload + in_pos, 8) & 0x1f;

        if (nal_unit_type == 5 && (
               p_h264_writer->sample_entries[i].num_pps != 1 ||
               p_h264_writer->sample_entries[i].num_sps != 1))
        {
            h264_write_sps_pps_nal(&p_h264_writer->sample_entries[i], p_h264_writer->out_file);
        }

        if ((nal_unit_type != 9) && (!p_h264_writer->wrote_sps_pps))
        {
            h264_write_sps_pps_nal(&p_h264_writer->sample_entries[i], p_h264_writer->out_file);
            p_h264_writer->wrote_sps_pps = 1;
        }
        ASSURE( fwrite("\0\0\0\1", 4, 1, p_h264_writer->out_file) == 1,
                ("Failed to write 4 bytes to output file") );
        ASSURE( fwrite(payload + in_pos, nal_size, 1, p_h264_writer->out_file) == 1,
                ("Failed to fwrite %" PRId32 " bytes to file", nal_size - size_field) );
        if (nal_unit_type == 9)
        {
            if (p_h264_writer->sample_entries[i].num_pps == 1 &&
                p_h264_writer->sample_entries[i].num_sps == 1 && !p_h264_writer->wrote_sps_pps)
            {
                h264_write_sps_pps_nal(&p_h264_writer->sample_entries[i], p_h264_writer->out_file);
                p_h264_writer->wrote_sps_pps = 1;
            }
        }

        in_pos += nal_size;
    }
cleanup:
    return err;
}

int h264_writer_new(es_sink_t *p_es_sink, uint32_t track_ID, const char *stream_name, const char *output_folder)
{
    h264_writer_t p_h264_writer;
    int err = 0;

    *p_es_sink = malloc(sizeof(struct h264_writer_t_));
    (*p_es_sink)->sample_ready = h264_writer_sample_ready;
    (*p_es_sink)->subsample_ready = NULL;
    (*p_es_sink)->sample_entry = h264_writer_sample_entry;
    (*p_es_sink)->destroy = h264_writer_destroy;
    

    p_h264_writer = (h264_writer_t) *p_es_sink; 

    p_h264_writer->track_ID = track_ID;
    p_h264_writer->wrote_sps_pps = 0;
    {
        char filename[255];
        int n = sizeof filename;
        int folder_len = 0;

        if (NULL != output_folder)
        {
            snprintf(filename, n, "%s", output_folder);
            folder_len = (int)strlen(output_folder);
            n -=  folder_len;
        }
        
        if (track_ID > 0)
        {
            ASSURE( snprintf(filename + folder_len, n, "out_%" PRIu32 ".h264", track_ID) < n,
                    (" "));
        }
        else
        {
            ASSURE( snprintf(filename + folder_len, n, "%s.h264", stream_name) < n,
                    (" "));
        }
        p_h264_writer->out_file = fopen(filename, "wb");
        ASSURE( p_h264_writer->out_file != NULL, ("Could not open '%s' for writing", filename) );
        logout(LOG_VERBOSE_LVL_INFO,"Writing track_ID = %" PRIu32 " to %s\n", track_ID, filename);
    }

    p_h264_writer->sample_entries = NULL;
    p_h264_writer->num_sample_entries = 0;

cleanup:
    return err;
}



typedef struct hevc_parameter_set_t_
{
    unsigned char * buf;
    uint32_t size;

} hevc_parameter_set_t;

typedef struct hevc_nal_unit_
{
    uint8_t nal_unit_type;
    uint64_t size;
} hevc_nal_unit;

typedef struct hevc_sample_entry_t_
{
    uint16_t sample_description_index;
    uint8_t size_field;
    uint8_t numOfArrays;
    uint16_t num_vps;
    hevc_parameter_set_t vps[32];
    uint16_t num_sps;
    hevc_parameter_set_t sps[32];
    uint16_t num_pps;
    hevc_parameter_set_t pps[32];
} hevc_sample_entry_t;              /* array of sample descriptions */

typedef struct hevc_writer_t_
{
    struct es_sink_t_ base;

    FILE *out_file;
    uint32_t track_ID;  /* For messaging */

    hevc_sample_entry_t *sample_entries;              /* array of sample descriptions */
    uint32_t num_sample_entries;    /* array size */

    int wrote_vps_sps_pps;  /* bool, used only if 1 sps and 1 pps */
} *hevc_writer_t;


static void
hevc_writer_destroy(es_sink_t p_es_sink)
{
    hevc_writer_t p_hevc_writer = (hevc_writer_t) p_es_sink;

    if (p_hevc_writer->out_file != NULL)
    {
        fclose(p_hevc_writer->out_file);
        p_hevc_writer->out_file = NULL;
    }

    {
        uint32_t i;
        for (i = 0; i < p_hevc_writer->num_sample_entries; i++)
        {
            uint32_t k;
            for (k = 0; k < p_hevc_writer->sample_entries[i].num_vps; k++) 
            {
                free( p_hevc_writer->sample_entries[i].vps[k].buf );
            }
            for (k = 0; k < p_hevc_writer->sample_entries[i].num_sps; k++) 
            {
                free( p_hevc_writer->sample_entries[i].sps[k].buf );
            }
            for (k = 0; k < p_hevc_writer->sample_entries[i].num_pps; k++) 
            {
                free( p_hevc_writer->sample_entries[i].pps[k].buf );
            }
        }
    }
    free(p_hevc_writer->sample_entries);
    free(p_es_sink);
}

static int
hevc_writer_sample_entry(es_sink_t p_es_sink,
                       const mp4d_sampleentry_t *sample_entry)
{
    const mp4d_sampleentry_visual_t *p_entry = &sample_entry->vide;
    hevc_writer_t p_hevc_writer = (hevc_writer_t) p_es_sink;
    uint32_t pos = 0;
    int err = 0;

    uint32_t i, k, j;

    p_hevc_writer->num_sample_entries++;
    p_hevc_writer->sample_entries = realloc(p_hevc_writer->sample_entries,
                                            p_hevc_writer->num_sample_entries *
                                            sizeof(*p_hevc_writer->sample_entries));

    i = p_hevc_writer->num_sample_entries - 1;

    /* SDI counts from 1 */
    p_hevc_writer->sample_entries[i].sample_description_index = p_hevc_writer->num_sample_entries;

    logout(LOG_VERBOSE_LVL_INFO,"configurationVersion = %d\n", read_bits(&pos, p_entry->dsi, 8));

    logout(LOG_VERBOSE_LVL_INFO,"profile_space = %d\n",   read_bits(&pos, p_entry->dsi, 2));
    logout(LOG_VERBOSE_LVL_INFO,"tier_flag = %d\n", read_bits(&pos, p_entry->dsi, 1));
    logout(LOG_VERBOSE_LVL_INFO,"profile_idc = %d\n",   read_bits(&pos, p_entry->dsi, 5));

    logout(LOG_VERBOSE_LVL_INFO,"profile_compatibility_indications = %d\n",     read_bits(&pos, p_entry->dsi, 32));

    logout(LOG_VERBOSE_LVL_INFO,"progressive_source_flag = %d \n",  read_bits(&pos, p_entry->dsi,1));
    logout(LOG_VERBOSE_LVL_INFO,"interlaced_source_flag = %d \n",  read_bits(&pos, p_entry->dsi,1));
    logout(LOG_VERBOSE_LVL_INFO,"non_packed_constraint_flag = %d \n",  read_bits(&pos, p_entry->dsi,1));
    logout(LOG_VERBOSE_LVL_INFO,"frame_only_constraint_flag = %d \n",  read_bits(&pos, p_entry->dsi,1));

    logout(LOG_VERBOSE_LVL_INFO,"constraint_indicator_flags = %d \n",  read_bits(&pos, p_entry->dsi,44));

    logout(LOG_VERBOSE_LVL_INFO,"level_idc = %d\n",   read_bits(&pos, p_entry->dsi, 8));

    logout(LOG_VERBOSE_LVL_INFO,"reserved = %d\n",   read_bits(&pos, p_entry->dsi, 4));

    logout(LOG_VERBOSE_LVL_INFO,"min_spatial_segmentation_idc = %d\n",   read_bits(&pos, p_entry->dsi, 12));

    logout(LOG_VERBOSE_LVL_INFO,"reserved = %d\n",   read_bits(&pos, p_entry->dsi, 6));
    logout(LOG_VERBOSE_LVL_INFO,"parallelismType = %d\n",   read_bits(&pos, p_entry->dsi, 2));

    logout(LOG_VERBOSE_LVL_INFO,"reserved = %d\n",   read_bits(&pos, p_entry->dsi, 6));
    logout(LOG_VERBOSE_LVL_INFO,"chromaFormat = %d\n",   read_bits(&pos, p_entry->dsi, 2));

    logout(LOG_VERBOSE_LVL_INFO,"reserved = %d\n",   read_bits(&pos, p_entry->dsi, 5));
    logout(LOG_VERBOSE_LVL_INFO,"bitDepthLumaMinus8 = %d\n",   read_bits(&pos, p_entry->dsi, 3));

    logout(LOG_VERBOSE_LVL_INFO,"reserved = %d\n",   read_bits(&pos, p_entry->dsi, 5));
    logout(LOG_VERBOSE_LVL_INFO,"bitDepthChromaMinus8 = %d\n",   read_bits(&pos, p_entry->dsi, 3));

    logout(LOG_VERBOSE_LVL_INFO,"avgFrameRate = %d\n",   read_bits(&pos, p_entry->dsi, 16));

    logout(LOG_VERBOSE_LVL_INFO,"constantFrameRate = %d\n",   read_bits(&pos, p_entry->dsi, 2));
    logout(LOG_VERBOSE_LVL_INFO,"numTemporalLayers = %d\n",   read_bits(&pos, p_entry->dsi, 3));
    logout(LOG_VERBOSE_LVL_INFO,"temporalIdNested = %d\n",   read_bits(&pos, p_entry->dsi, 1));

    p_hevc_writer->sample_entries[i].size_field = read_bits(&pos, p_entry->dsi, 2);
    p_hevc_writer->sample_entries[i].numOfArrays = read_bits(&pos, p_entry->dsi, 8);
    if (!p_hevc_writer->sample_entries[i].numOfArrays)
    {
        p_hevc_writer->sample_entries[i].num_pps = p_hevc_writer->sample_entries[i].num_sps = p_hevc_writer->sample_entries[i].num_vps = 0;
    }
    for (k = 0; k < p_hevc_writer->sample_entries[i].numOfArrays; k++) 
    {
        uint8_t NAL_unit_type;
        uint16_t numNalus;
        uint32_t length;
        uint32_t offset;

        logout(LOG_VERBOSE_LVL_INFO,"array_completeness = %d\n",     read_bits(&pos, p_entry->dsi, 1));
        logout(LOG_VERBOSE_LVL_INFO,"reserved = %d\n",     read_bits(&pos, p_entry->dsi, 1));
        NAL_unit_type = read_bits(&pos, p_entry->dsi, 6);
        
		if (NAL_unit_type == 32) /*VPS*/
        {
            numNalus = read_bits(&pos, p_entry->dsi, 16);
            p_hevc_writer->sample_entries[i].num_vps = numNalus;
            for(j = 0; j < numNalus; j++)
            {
                length = read_bits(&pos, p_entry->dsi, 16);
                offset = (pos >> 3);

                ASSURE(offset + length <= p_entry->dsi_size,
                        ("VPS offset + length (%" PRIu32 " + %" PRIu32") exceeds DSI size (%" PRIu64 ")",
                            offset, length, p_entry->dsi_size) );

                p_hevc_writer->sample_entries[i].vps[j].size = length;
                p_hevc_writer->sample_entries[i].vps[j].buf = malloc(length);
                ASSURE(p_hevc_writer->sample_entries[i].vps[j].buf != NULL, ("Allocation failure") );

                memcpy(p_hevc_writer->sample_entries[i].vps[j].buf, p_entry->dsi+offset, length);
                pos += (length << 3);
            }
        }
		/*SPS*/
        else if (NAL_unit_type == 33)
        {
            numNalus = read_bits(&pos, p_entry->dsi, 16);
            p_hevc_writer->sample_entries[i].num_sps = numNalus;
            for(j = 0; j < numNalus; j++)
            {
                length = read_bits(&pos, p_entry->dsi, 16);
                offset = (pos >> 3);

                ASSURE(offset + length <= p_entry->dsi_size,
                        ("SPS offset + length (%" PRIu32 " + %" PRIu32") exceeds DSI size (%" PRIu64 ")",
                            offset, length, p_entry->dsi_size) );

                p_hevc_writer->sample_entries[i].sps[j].size = length;
                p_hevc_writer->sample_entries[i].sps[j].buf = malloc(length);
                ASSURE(p_hevc_writer->sample_entries[i].sps[j].buf != NULL, ("Allocation failure") );

                memcpy(p_hevc_writer->sample_entries[i].sps[j].buf, p_entry->dsi+offset, length);
                pos += (length << 3);
            }
        }
		/*PPS*/
        else if (NAL_unit_type == 34)
        {
            numNalus = read_bits(&pos, p_entry->dsi, 16);
            p_hevc_writer->sample_entries[i].num_pps = numNalus;
            for(j = 0; j < numNalus; j++)
            {
                length = read_bits(&pos, p_entry->dsi, 16);
                offset = (pos >> 3);

                ASSURE(offset + length <= p_entry->dsi_size,
                        ("PPS offset + length (%" PRIu32 " + %" PRIu32") exceeds DSI size (%" PRIu64 ")",
                            offset, length, p_entry->dsi_size) );

                p_hevc_writer->sample_entries[i].pps[j].size = length;
                p_hevc_writer->sample_entries[i].pps[j].buf = malloc(length);
                ASSURE(p_hevc_writer->sample_entries[i].pps[j].buf != NULL, ("Allocation failure") );

                memcpy(p_hevc_writer->sample_entries[i].pps[j].buf, p_entry->dsi+offset, length);
                pos += (length << 3);
            }
        }
        else
        {
            ASSURE(1,("Not supporting NAL type!"));
        }
        
    }


cleanup:
    return err;
}


static int
hevc_write_ps_nal(hevc_sample_entry_t * p_entry, FILE* out_file)
{
    uint32_t i;
    unsigned char nal_header[5] = "\0\0\0\1";
    int err = 0;

    for (i = 0; i < p_entry->num_vps; i++) {
        ASSURE( fwrite(nal_header, 4, 1, out_file) == 1,
                ("Failed to fwrite NAL header to out file") );
        ASSURE( fwrite(p_entry->vps[i].buf, p_entry->vps[i].size, 1, out_file) == 1,
                ("Failed to fwrite %" PRIu32 " bytes VPS to out file", p_entry->vps[i].size) );
    }

    for (i = 0; i < p_entry->num_sps; i++) {
        ASSURE( fwrite(nal_header, 4, 1, out_file) == 1,
                ("Failed to fwrite NAL header to out file") );
        ASSURE( fwrite(p_entry->sps[i].buf, p_entry->sps[i].size, 1, out_file) == 1,
                ("Failed to fwrite %" PRIu32 " bytes SPS to out file", p_entry->sps[i].size) );
    }

    for (i = 0; i < p_entry->num_pps; i++) {
        ASSURE( fwrite(nal_header, 4, 1, out_file) == 1,
                ("Failed to fwrite NAL header to out file") );
        ASSURE( fwrite(p_entry->pps[i].buf, p_entry->pps[i].size, 1, out_file) == 1,
                ("Failed to fwrite %" PRIu32 " bytes PPS to out file", p_entry->pps[i].size) );
    }

cleanup:
    return err;
}

static int
hevc_writer_sample_ready(es_sink_t p_es_sink,
                         const mp4d_sampleref_t *sample,
                         const unsigned char *payload)
{
    hevc_writer_t p_hevc_writer = (hevc_writer_t) p_es_sink;
    uint32_t i;
    uint32_t in_pos; /* position in payload buffer */
    int err = 0;
    uint32_t size_field; /* in bytes */

    for (i = 0; i < p_hevc_writer->num_sample_entries; i++)
    {
        if (p_hevc_writer->sample_entries[i].sample_description_index ==
            sample->sample_description_index)
        {
            break;
        }
    }
    ASSURE( i < p_hevc_writer->num_sample_entries, 
            ("Sample description index %" PRIu16 " is unknown", sample->sample_description_index) );

    size_field = p_hevc_writer->sample_entries[i].size_field + 1;
    in_pos = 0;



    while (in_pos < sample->size)
    {
        uint32_t nal_size;
        uint32_t nal_size_tmp;
        uint8_t nal_unit_type;
        uint32_t pos = 0;
        uint32_t nal_tmp_pos;

        nal_size = read_bits(&pos, payload + in_pos, size_field * 8);
        in_pos += size_field;
        pos = 0;

        ASSURE( in_pos + nal_size <= sample->size, 
              ("NAL size (%" PRIu32 ") exceeds remaining data (%" PRIu32 " bytes)",
               nal_size, sample->size - in_pos) );

        nal_unit_type = (read_bits(&pos, payload + in_pos, 8) & 0x7e) >> 1;

        if (nal_unit_type == 19)
        {
            hevc_write_ps_nal(&p_hevc_writer->sample_entries[i], p_hevc_writer->out_file);
        }

        if ((nal_unit_type != 35) && (!p_hevc_writer->wrote_vps_sps_pps))
        {
            hevc_write_ps_nal(&p_hevc_writer->sample_entries[i], p_hevc_writer->out_file);
            p_hevc_writer->wrote_vps_sps_pps = 1;
        }
        ASSURE( fwrite("\0\0\0\1", 4, 1, p_hevc_writer->out_file) == 1,
                ("Failed to write 4 bytes to output file") );
        nal_size_tmp = nal_size;
        nal_tmp_pos = in_pos;
        while(nal_size_tmp)
        {
            if (nal_size_tmp > 1024)
            {
                fwrite(payload + nal_tmp_pos, 1024, 1, p_hevc_writer->out_file);
                nal_tmp_pos += 1024;
                nal_size_tmp -= 1024;
            }
            else
            {
                fwrite(payload + nal_tmp_pos, nal_size_tmp, 1, p_hevc_writer->out_file);
                nal_size_tmp = 0;
            }
        }

        if (nal_unit_type == 35)
        {
            if (p_hevc_writer->sample_entries[i].num_vps == 1 && p_hevc_writer->sample_entries[i].num_sps == 1 &&
                p_hevc_writer->sample_entries[i].num_pps == 1 && !p_hevc_writer->wrote_vps_sps_pps)
            {
                hevc_write_ps_nal(&p_hevc_writer->sample_entries[i], p_hevc_writer->out_file);
                p_hevc_writer->wrote_vps_sps_pps = 1;
            }
        }

        in_pos += nal_size;
    }
cleanup:
    return err;
}

int 
hevc_writer_new(es_sink_t *p_es_sink, uint32_t track_ID, const char *stream_name, const char *output_folder, uint32_t stdout_flag)
{
    hevc_writer_t p_hevc_writer;
    int err = 0;

    *p_es_sink = malloc(sizeof(struct hevc_writer_t_));
    (*p_es_sink)->sample_ready = hevc_writer_sample_ready;
    (*p_es_sink)->subsample_ready = NULL;
    (*p_es_sink)->sample_entry = hevc_writer_sample_entry;
    (*p_es_sink)->destroy = hevc_writer_destroy;
    

    p_hevc_writer = (hevc_writer_t) *p_es_sink; 

    p_hevc_writer->track_ID = track_ID;
    p_hevc_writer->wrote_vps_sps_pps = 0;
    if (!stdout_flag)
    {
        char filename[255];
        int n = sizeof filename;
        int folder_len = 0;

        if (NULL != output_folder)
        {
            snprintf(filename, n, "%s", output_folder);
            folder_len = (int)strlen(output_folder);
            n -=  folder_len;
        }
        
        if (track_ID > 0)
        {
            ASSURE( snprintf(filename + folder_len, n, "out_%" PRIu32 ".h265", track_ID) < n,
                    (" "));
        }
        else
        {
            ASSURE( snprintf(filename + folder_len, n, "%s.h265", stream_name) < n,
                    (" "));
        }
        p_hevc_writer->out_file = fopen(filename, "wb");
        ASSURE( p_hevc_writer->out_file != NULL, ("Could not open '%s' for writing", filename) );
        logout(LOG_VERBOSE_LVL_INFO,"Writing track_ID = %" PRIu32 " to %s\n", track_ID, filename);
    }
    else
    {
        #if WIN32
        int32_t res;
        fflush( stdout );
        res = _setmode( _fileno( stdout ), _O_BINARY );
        if( res == -1 )
        {
            logout(LOG_VERBOSE_LVL_INFO,"Failed to switch binary mode - output may be invalid!\n" );
        }
        #endif
        p_hevc_writer->out_file = stdout;
    }

    p_hevc_writer->sample_entries = NULL;
    p_hevc_writer->num_sample_entries = 0;

cleanup:
    return err;
}


typedef struct subt_writer_t_
{
    struct es_sink_t_ base;

    FILE *out_file;
    FILE *subsample_out_file;
    uint32_t track_ID;  /* For messaging */

    const char *output_folder;  /* specified output folder name */
} *subt_writer_t;

static void
subt_writer_destroy(es_sink_t p_es_sink)
{
    subt_writer_t p_es_writer = (subt_writer_t) p_es_sink;

    if (p_es_writer->out_file != NULL)
    {
        fclose(p_es_writer->out_file);
    }
    free(p_es_sink);
}


static int
subt_writer_sample_entry(es_sink_t p_es_sink,
                       const mp4d_sampleentry_t *sample_entry)
{
    (void) p_es_sink; /* unused */
    (void) sample_entry;
    return 0;
}

static int
subt_writer_sample_ready(es_sink_t p_es_sink,
                       const mp4d_sampleref_t *sample,
                       const unsigned char *payload)
{
    subt_writer_t p_es_writer = (subt_writer_t) p_es_sink;
    int err = 0;

    ASSURE( fwrite(payload, sample->size, 1, p_es_writer->out_file) == 1,
            ("Failed to write %" PRIu32 " bytes to output", sample->size) );

cleanup:
    return err;
}

static int
subt_writer_subsample_ready(uint32_t subsample_index,
                             es_sink_t p_es_sink,
                             const mp4d_sampleref_t *p_sample,
                             const unsigned char *payload,
                             uint64_t offset,
                             uint32_t size)
{
    int err = 0;
    subt_writer_t p_es_writer = (subt_writer_t) p_es_sink;
    char filename[255];
    int n = sizeof(filename);

    if (subsample_index != 0)
    {
        int folder_len = 0;
        if (NULL != p_es_writer->output_folder)
        {
            snprintf(filename, n, "%s", p_es_writer->output_folder);
            folder_len = (int)strlen(p_es_writer->output_folder);
            n -=  folder_len;
        }

        ASSURE( snprintf(filename + folder_len, n, "out_%" PRIu32 "_%" PRIu64 "_%" PRIu32 ".png", p_es_writer->track_ID, offset, subsample_index) < n,
                (" "));
        p_es_writer->subsample_out_file = fopen(filename, "wb");
        ASSURE( p_es_writer->subsample_out_file != NULL, ("Failed to open %s for writing", filename) );
        ASSURE( fwrite(payload, size, 1, p_es_writer->subsample_out_file) == 1,
              ("Failed to write %" PRIu32 " bytes to output", size) );
        fclose(p_es_writer->subsample_out_file);    
        p_es_writer->subsample_out_file = NULL;
    }
    else
    {
        ASSURE( fwrite(payload, size, 1, p_es_writer->out_file) == 1,
                ("Failed to write %" PRIu32 " bytes to output", size) );
    }

cleanup:
    return err;
}


int
subt_writer_new(es_sink_t *p_es_sink, uint32_t track_ID, const char *stream_name, const char *output_folder)
{
    subt_writer_t p_subt_writer;
    int err = 0;

    *p_es_sink = malloc(sizeof(struct subt_writer_t_));
    (*p_es_sink)->sample_ready = subt_writer_sample_ready;
    (*p_es_sink)->subsample_ready = subt_writer_subsample_ready;
    (*p_es_sink)->sample_entry = subt_writer_sample_entry;
    (*p_es_sink)->destroy = subt_writer_destroy;

    p_subt_writer = (subt_writer_t) *p_es_sink;
    p_subt_writer->track_ID = track_ID;
    p_subt_writer->output_folder = output_folder;
    p_subt_writer->subsample_out_file = NULL;

    {
        char filename[255];
        int n = sizeof filename;
        int folder_len = 0;

        if (NULL != output_folder)
        {
            snprintf(filename, n, "%s", output_folder);
            folder_len = (int)strlen(output_folder);
            n -=  folder_len;
        }
        
        if (track_ID > 0)
        {
            ASSURE( snprintf(filename + folder_len, n, "out_%" PRIu32 ".xml", track_ID) < n,
                    (" "));
        }
        else
        {
            ASSURE( snprintf(filename + folder_len, n, "%s.dat", stream_name) < n,
                    (" "));
        }
        p_subt_writer->out_file = fopen(filename, "wb");
        ASSURE( p_subt_writer->out_file != NULL, ("Failed to open %s for writing", filename) );
        logout(LOG_VERBOSE_LVL_INFO,"Writing track_ID = %" PRIu32 " to %s\n", track_ID, filename);
    }

cleanup:
    return err;
}

int 
sink_sample_entry (es_sink_t sink, const mp4d_sampleentry_t * p_entry)
{
    if (sink != NULL && sink->sample_entry!= NULL)
    {
         return (sink->sample_entry(sink, p_entry));
    }

    return -1;

}


int 
sink_sample_ready (
    es_sink_t sink,
    const mp4d_sampleref_t *p_sample,
    const unsigned char *payload   /**< Sample buffer whose size is p_sample->size */
    )
{
    if (sink != NULL && sink->sample_ready != NULL)
    {
         return (sink->sample_ready(sink, p_sample, payload));
    }

    return -1;

}

int 
sink_subsample_ready (
    uint32_t subsample_index,
    es_sink_t sink,
    const mp4d_sampleref_t *p_sample,  /**< parent sample */
    const unsigned char *payload,
    uint64_t offset,                 /**< relative to beginning of file */
    uint32_t size                     /**< in bytes */
    )
{
    if (sink != NULL && sink->subsample_ready != NULL)
    {
         return (sink->subsample_ready(subsample_index, sink, p_sample, payload, offset, size));
    }

    return -1;

}

void 
sink_destroy (es_sink_t sink)
{
    if (sink != NULL && sink->destroy!= NULL)
    {
         sink->destroy(sink);
    }

}


typedef struct dv_parameter_set_t_
{
    unsigned char * buf;
    uint32_t size;

} dv_parameter_set_t;

typedef struct dv_nal_unit_
{
    uint8_t nal_unit_type;
    uint64_t size;
} dv_nal_unit;

typedef struct dv_sample_entry_t_
{
    uint16_t sample_description_index;
    uint8_t size_field;
    uint8_t num_sps;
    uint8_t num_pps;
} dv_sample_entry_t;              /* array of sample descriptions */

typedef struct dv_writer_t_
{
    struct es_sink_t_ base;

    FILE *out_file;
    uint32_t track_ID;  /* For messaging */

} *dv_writer_t;

static void
dv_el_writer_destroy(es_sink_t p_es_sink)
{
    dv_writer_t p_dv_writer = (dv_writer_t) p_es_sink;

    if (p_dv_writer->out_file != NULL)
    {
        fclose(p_dv_writer->out_file);
        p_dv_writer->out_file = NULL;
    }

    free(p_es_sink);
}

static int
dv_el_writer_sample_entry(es_sink_t p_es_sink,
                       const mp4d_sampleentry_t *sample_entry)
{
    return 0;
}

static int
dv_el_writer_sample_ready(es_sink_t p_es_sink,
                         const mp4d_sampleref_t *sample,
                         const unsigned char *payload)
{
    dv_writer_t p_dv_writer = (dv_writer_t) p_es_sink;

    uint32_t in_pos = 0; /* position in payload buffer */
    int err = 0;
    uint32_t size_field = 4; /* in bytes */

    while (in_pos < sample->size)
    {
        uint32_t nal_size;
        uint32_t pos = 0;

        nal_size = read_bits(&pos, payload + in_pos, size_field * 8);
        in_pos += size_field;
        pos = 0;

        ASSURE( in_pos + nal_size <= sample->size, 
                ("NAL size (%" PRIu32 ") exceeds remaining data (%" PRIu32 " bytes)",
                 nal_size, sample->size - in_pos) );
        ASSURE( fwrite("\0\0\0\1", 4, 1, p_dv_writer->out_file) == 1,
                ("Failed to write 4 bytes to output file") );
        ASSURE( fwrite(payload + in_pos, nal_size, 1, p_dv_writer->out_file) == 1,
                ("Failed to fwrite %" PRId32 " bytes to file", nal_size - size_field) );
        

        in_pos += nal_size;
    }

cleanup:
    return err;
}

int dv_el_writer_new(es_sink_t *p_es_sink, uint32_t track_ID, const char *stream_name, const char *codec_type, const char *output_folder)
{
    dv_writer_t p_dv_writer;

    int err = 0;

    if (MP4D_FOURCC_EQ(codec_type, "dvav"))
    {
        *p_es_sink = malloc(sizeof(struct h264_writer_t_));
        (*p_es_sink)->sample_ready = h264_writer_sample_ready;
        (*p_es_sink)->subsample_ready = NULL;
        (*p_es_sink)->sample_entry = h264_writer_sample_entry;
        (*p_es_sink)->destroy = h264_writer_destroy;
        ((h264_writer_t)*p_es_sink)->num_sample_entries = 0;
        ((h264_writer_t)*p_es_sink)->sample_entries = NULL;
         ((h264_writer_t)*p_es_sink)->wrote_sps_pps = 0;
    }
    else
    {
        *p_es_sink = malloc(sizeof(struct dv_writer_t_));
        (*p_es_sink)->sample_ready = dv_el_writer_sample_ready;
        (*p_es_sink)->subsample_ready = NULL;
        (*p_es_sink)->sample_entry = dv_el_writer_sample_entry;
        (*p_es_sink)->destroy = dv_el_writer_destroy;
    }

    p_dv_writer = (dv_writer_t) *p_es_sink; 

    p_dv_writer->track_ID = track_ID;
    {
        char filename[255];
        int n = sizeof filename;
        int folder_len = 0;

        if (NULL != output_folder)
        {
            snprintf(filename, n, "%s", output_folder);
            folder_len = (int)strlen(output_folder);
            n -=  folder_len;
        }
        if (MP4D_FOURCC_EQ(codec_type, "dvav"))
        {
            if (track_ID > 0)
            {
                ASSURE( snprintf(filename + folder_len, n, "dv_el_out_%" PRIu32 ".h264", track_ID) < n,
                        (" "));
            }
            else
            {
                ASSURE( snprintf(filename + folder_len, n, "%s.h264", stream_name) < n,
                        (" "));
            }
        }
        else if (MP4D_FOURCC_EQ(codec_type, "dvhe"))
        {
            if (track_ID > 0)
            {
                ASSURE( snprintf(filename + folder_len, n, "dv_el_out_%" PRIu32 ".h265", track_ID) < n,
                        (" "));
            }
            else
            {
                ASSURE( snprintf(filename + folder_len, n, "%s.h265", stream_name) < n,
                        (" "));
            }
        }

        p_dv_writer->out_file = fopen(filename, "wb");
        ASSURE( p_dv_writer->out_file != NULL, ("Could not open '%s' for writing", filename) );
        logout(LOG_VERBOSE_LVL_INFO,"Writing track_ID = %" PRIu32 " to %s\n", track_ID, filename);
    }

cleanup:
    return err;
}


int dv_bl_el_writer_new(es_sink_t *p_bl_es_sink, es_sink_t *p_el_es_sink, uint32_t track_ID, const char *stream_name, const char *codec_type, const char *output_folder)
{
    dv_writer_t p_dv_writer;
    h264_writer_t p_h264_writer;
    hevc_writer_t p_hevc_writer;

    int err = 0;

    if (MP4D_FOURCC_EQ(codec_type, "avc1") || (MP4D_FOURCC_EQ(codec_type, "hvc1")) || (MP4D_FOURCC_EQ(codec_type, "hev1")))
    {
        char filename[255];
        int n = sizeof filename;
        int folder_len = 0;

        if (NULL != output_folder)
        {
            snprintf(filename, n, "%s", output_folder);
            folder_len = (int)strlen(output_folder);
            n -=  folder_len;
        }

           *p_el_es_sink = malloc(sizeof(struct dv_writer_t_));
        p_dv_writer = (dv_writer_t) *p_el_es_sink; 

        (*p_el_es_sink)->sample_ready = dv_el_writer_sample_ready;
        (*p_el_es_sink)->subsample_ready = NULL;
        (*p_el_es_sink)->sample_entry = dv_el_writer_sample_entry;
        (*p_el_es_sink)->destroy = dv_el_writer_destroy;

        p_dv_writer = (dv_writer_t) *p_el_es_sink; 
        if (MP4D_FOURCC_EQ(codec_type, "avc1"))
        {
            *p_bl_es_sink = malloc(sizeof(struct h264_writer_t_));
            p_h264_writer = (h264_writer_t) *p_bl_es_sink; 
            (*p_bl_es_sink)->sample_ready = h264_writer_sample_ready;
            (*p_bl_es_sink)->subsample_ready = NULL;
            (*p_bl_es_sink)->sample_entry = h264_writer_sample_entry;
            (*p_bl_es_sink)->destroy = h264_writer_destroy;
            p_h264_writer->sample_entries = NULL;
            p_h264_writer->num_sample_entries = 0;
            p_h264_writer->wrote_sps_pps = 0;
            ASSURE( snprintf(filename + folder_len, n, "dv_bl_el_out_%" PRIu32 ".h264", track_ID) < n,(" "));
            p_h264_writer->out_file = fopen(filename, "wb");
            p_dv_writer->out_file = p_h264_writer->out_file;
        }
        else
        {
            *p_bl_es_sink = malloc(sizeof(struct hevc_writer_t_));
            p_hevc_writer = (hevc_writer_t) *p_bl_es_sink; 
            (*p_bl_es_sink)->sample_ready = hevc_writer_sample_ready;
            (*p_bl_es_sink)->subsample_ready = NULL;
            (*p_bl_es_sink)->sample_entry = hevc_writer_sample_entry;
            (*p_bl_es_sink)->destroy = hevc_writer_destroy;
            p_hevc_writer->sample_entries = NULL;
            p_hevc_writer->num_sample_entries = 0;
            p_hevc_writer->wrote_vps_sps_pps = 0;
            ASSURE( snprintf(filename + folder_len, n, "dv_bl_el_out_%" PRIu32 ".h265", track_ID) < n,(" "));
            p_hevc_writer->out_file = fopen(filename, "wb");
            p_dv_writer->out_file = p_hevc_writer->out_file;
        }
    }

cleanup:
    return err;
}



