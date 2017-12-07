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
/** @defgroup mp4d_box_read
 *
 * @brief API for reading MP4 boxs
 *
 */

#ifndef MP4D_BOX_READ_H
#define MP4D_BOX_READ_H

#include "mp4d_internal.h"

/**
  @brief Module for reading the time-to-sample (stts/ctts) boxes.
  Allows access to the next sample, or random access.
  Run-length decodes the boxes on the fly.
*/
typedef struct tts_reader_t_
{
    mp4d_buffer_t buffer;    /* Points to user's buffer.
                                buffer.p_data is non-NULL after the 
                                object has been initialized */
    
    int delta_encoded;  /* True for stts, false for ctts */
    uint8_t  tts_version;    /* box version */
    uint32_t entry_count;

    /* Cache the values from a previous read of DTS, in order
       to optimize the case where samples are read in order */
    uint64_t next_sample_index;   /* index of next sample (current sample index + 1).  Zero means: no current sample */
    uint64_t cur_dts;

    uint32_t cur_entry_index;
    uint32_t cur_entry_sample_count;
    uint32_t cur_entry_sample_value;

    uint32_t cur_entry_consumed;   /* current sample offset into current entry */
} tts_reader_t;

mp4d_error_t
mp4d_tts_init(tts_reader_t *, 
              mp4d_atom_t *p_tts, 
              int delta_encoded    /**< non-zero: Sample values are delta encoded (stts)
                                            zero: Sample values are the actual values (ctts)
                                   */
    );

/**
  @brief Get the time stamp of any sample.
  
  @return MP4D_NO_ERROR, or
          MP4D_E_NEXT_SEGMENT: sample number is too high
*/
mp4d_error_t
mp4d_tts_get_ts(tts_reader_t *, 
                uint64_t sample_index,  /**< Sample number, counting from zero */
                uint64_t *p_ts,       /**< [out] */
                uint32_t *p_duration  /**< [out] */
    );


/* @brief Get the next sample's time stamp

   Same as mp4d_tts_get_ts() but optimized version for getting
   the next sample. As an optimization, p_r->delta_encoded is
   ignored and assumed true.
*/
mp4d_error_t
mp4d_tts_get_stts_next(tts_reader_t *p_r, uint64_t *p_ts, uint32_t *p_duration);

/* @brief Get the next sample's time stamp (stts)

   Same as mp4d_tts_get_stts_next()
   p_r->delta_encoded is ignored and assumed false.
 */
mp4d_error_t
mp4d_tts_get_ctts_next(tts_reader_t *p_r, uint32_t *p_ts);

/** 
  @brief Reader of the sample size atoms (stsz/stz2)
*/
typedef struct stsz_reader_t_
{
    mp4d_buffer_t buffer;    /* Points to user's buffer.
                                buffer.p_data is non-NULL after the 
                                object has been initialized */
    uint32_t sample_size;    /* default sample size */
    uint32_t sample_count;

    uint8_t field_size;      /* 4, 8, 16, 32 */
    uint8_t size_4;          /* only used when field_size == 4: contains two samples sizes */

    uint32_t next_sample_index; /* Index of the next sample to get */
} stsz_reader_t;

mp4d_error_t
mp4d_stsz_init(stsz_reader_t *, 
               mp4d_atom_t *, 
               int is_stz2   /** boolean */);

mp4d_error_t
mp4d_stsz_get_next(stsz_reader_t *, uint32_t *);

mp4d_error_t
mp4d_stsz_get(stsz_reader_t *, 
              uint64_t sample_index, /* counting from zero */
              uint32_t *);

/**
   @brief Reader of the sample to chunk index (stsc) atoms
*/
typedef struct stsc_reader_t_
{
    mp4d_buffer_t buffer;
    uint32_t entry_count;

    uint32_t cur_entry_index;   /* counting from one */

    uint32_t cur_chunk;   /* Current chunk number */
    uint32_t cur_samples_per_chunk;
    uint32_t cur_sample_description_index;
    uint32_t next_first_chunk;

    uint32_t samples_consumed;

} stsc_reader_t;

mp4d_error_t
mp4d_stsc_init(stsc_reader_t *, mp4d_atom_t *);

mp4d_error_t
mp4d_stsc_get_next(stsc_reader_t *, 
                   uint32_t *chunk_index,              /** [out] */
                   uint32_t *sample_description_index, /** [out] */
                   uint32_t *sample_index_in_chunk     /** [out] sample number in this chunk, counting from zero */
                   );

/**
   @brief Reader of the chunk offsets boxes (stco/co64)
*/
typedef struct co_reader_t_
{
    mp4d_buffer_t chunk_offsets;
    uint32_t entry_count;
    uint32_t cur_entry_index;   /* counting from one */

    int is_co64;   /* zero: stco, non-zero: co64 */
} co_reader_t;

mp4d_error_t
mp4d_co_init(co_reader_t *, 
             mp4d_atom_t *, 
             int is_co64    /* boolean */
    );

mp4d_error_t
mp4d_co_get_next(co_reader_t *, 
                 uint64_t *  /** [out] 64-bit integer for both stco and co64 */
    );



/**
   @brief Reader of sync sample info (stss)
*/
typedef struct stss_reader_t_
{
    mp4d_buffer_t buffer;
    uint32_t entries_left;
    uint32_t cur_sample_number;  /* Counting from one */
    uint32_t next_sync_sample;
    uint32_t count;
    unsigned char *stts_content;
} stss_reader_t;

mp4d_error_t
mp4d_stss_init(stss_reader_t *,
               mp4d_atom_t * /** NULL means: do not have stss (all samples are sync samples) */
    );

mp4d_error_t
mp4d_stss_get_next(stss_reader_t *,
                   int * is_sync  /** [out] */
    );


/**
   @brief Reader of edit list box (elst)
*/
typedef struct elst_reader_t_
{
    mp4d_buffer_t buffer;
    uint8_t version;
    uint32_t entries_left;
    uint32_t movie_ts, media_ts; /* movie, media time scale */

    /* current entry: */
    int64_t media_time;           /* media time scale */
    uint64_t segment_start;       /* movie time scale */
    uint64_t segment_duration;    /* movie time scale */
    int16_t media_rate;
} elst_reader_t;

mp4d_error_t
mp4d_elst_init(elst_reader_t *,
               mp4d_atom_t *,        /**<  NULL means: no elst box, trivial mapping */
               uint32_t media_time_scale,
               uint32_t movie_time_scale
    );


/*
   @return: MP4D_E_INFO_NOT_AVAIL if no part of the sample corresponds
            to a presentation time
*/
mp4d_error_t
mp4d_elst_get_presentation_time(elst_reader_t *p_r,
                                uint64_t media_time,  /** sample cts (media time scale) */
                                uint32_t duration,    /** sample duration (media time scale) */
                                int64_t *p_time,      /**< [out] PTS (media time scale) of sample begin, whether sample begin is included
                                                         in presentation or not. May be negative */
                                uint32_t *p_offset,   /**< [out] Offset (media time scale) from sample begin where presentation starts */
                                uint32_t *p_duration  /**< [out] Duration (media time scale) of the part of the sample which is
                                                         included in the presentation */
    );


/**
   @brief Reader of independent and disposable samples (sdtp)
*/
typedef struct sdtp_reader_t_
{
    mp4d_buffer_t buffer;
    uint32_t sample_count;
    uint32_t next_sample_index;
} sdtp_reader_t;

mp4d_error_t
mp4d_sdtp_init(sdtp_reader_t *,
               mp4d_atom_t *,
               uint32_t sample_count   /* Must be provided from stsz/stz2 */
    );

mp4d_error_t
mp4d_sdtp_get_next(sdtp_reader_t *,
                   uint8_t *entry     /**< [out] is_leading + sample_depends_on + sample_is_depended_on + sample_has_redundancy */
    );


/**
   @brief Reader of sample degradation priority (stdp)
*/
typedef struct stdp_reader_t_
{
    mp4d_buffer_t buffer;
    uint32_t sample_count;
    uint32_t next_sample_index;
} stdp_reader_t;

mp4d_error_t
mp4d_stdp_init(stdp_reader_t *,
               mp4d_atom_t *,
               uint32_t sample_count   /* Must be provided from stsz/stz2 */
    );

mp4d_error_t
mp4d_stdp_get_next(stdp_reader_t *,
                   uint16_t *p_priority /**< [out] is_leading + sample_depends_on + sample_is_depended_on + sample_has_redundancy */
    );

/**
   @brief Reader of trick play box (trik)
*/
typedef struct trik_reader_t_
{
    mp4d_buffer_t buffer;
    uint32_t sample_count;
    uint32_t next_sample_index;
} trik_reader_t;

mp4d_error_t
mp4d_trik_init(trik_reader_t *,
               mp4d_atom_t *,
               uint32_t sample_count   /* Must be provided from trun */
    );

mp4d_error_t
mp4d_trik_get_next(trik_reader_t *,
                   uint8_t *p_pic_type,        /**< [out] picture type of the sample */
                   uint8_t *p_dependency_level /**< [out]  picture dependency level of the sample */
    );

/**
   @brief Reader of senc box (CFF senc)
*/
typedef struct senc_reader_t_
{
    mp4d_buffer_t buffer;
    uint32_t sample_count;
    uint32_t flags;
    uint32_t next_sample_index;
} senc_reader_t;

mp4d_error_t
mp4d_senc_init(senc_reader_t *,
               mp4d_atom_t *
    );

mp4d_error_t
mp4d_senc_get_next(senc_reader_t *p_r,
                          uint8_t *init_vector,      /**< [out] initilization vector */
                          uint8_t iv_size,           /**< [in]  initilization vector size: 8 or 16 */
                          uint16_t *subsample_count, /**< [out] subsample count  */
                          const uint8_t **p_encryp_info     /**< [out] pointer to begin of clear and encrypted bytes */
    );


/**
   @brief Reader of padding bits (padb)
*/
typedef struct padb_reader_t_
{
    mp4d_buffer_t buffer;
    uint32_t sample_count;
    uint32_t next_sample_index;
    uint8_t current_entry;
} padb_reader_t;


mp4d_error_t
mp4d_padb_init(padb_reader_t *,
               mp4d_atom_t *
    );

mp4d_error_t
mp4d_padb_get_next(padb_reader_t *,
                   uint8_t *padding     /**< [out]  */
    );

/**
   @brief Reader of subsample info (subs)

   This module must be used as follows

   * mp4d_subs_init(...)
   * for each sample
     mp4d_subs_get_next_count(..., &count)
     * Call mp4d_subs_get_next_size() up to 'count' times, or less
*/
typedef struct subs_reader_t_
{
    mp4d_buffer_t buffer;

    uint8_t version;
    uint32_t next_sample_index;

    uint32_t entries_left;
    uint32_t next_entry_sample_number;
    uint16_t next_entry_subsample_count;

    uint32_t current_offset;  /* running subsample offset */
    uint32_t subsamples_left; /* In current subsample table */
} subs_reader_t;


mp4d_error_t
mp4d_subs_init(subs_reader_t *,
               mp4d_atom_t *    /* NULL: do not have subs box =>
                                   Each sample has 1 subsample, identical to the sample
                                 */
    );

mp4d_error_t
mp4d_subs_get_next_count(subs_reader_t *,
                         uint16_t *count     /**< [out] always >= 1  */
    );

mp4d_error_t
mp4d_subs_get_next_size(subs_reader_t *,
                        uint32_t sample_size, /* needed when the subsample info is implicitly given */
                        uint32_t *p_size,     /**< [out] */
                        uint32_t *p_offset    /** relative to sample beginning */
    );

/**
   @brief Reader of sample aux size (saiz)
*/
typedef struct saiz_reader_t_
{
    mp4d_buffer_t buffer;

    uint32_t aux_info_type;
    uint8_t default_sample_info_size;
    uint32_t samples_left;
} saiz_reader_t;


mp4d_error_t
mp4d_saiz_init(saiz_reader_t *,
               mp4d_atom_t *
    );

mp4d_error_t
mp4d_saiz_get_next_size(saiz_reader_t *,
                        uint8_t *p_size     /**< [out] zero: no aux information */
    );

/**
   @brief Reader of sample aux offset (saio)
*/
typedef struct saio_reader_t_
{
    mp4d_buffer_t buffer;

    uint8_t version;
    uint32_t aux_info_type;
    uint32_t entries_left;
} saio_reader_t;


mp4d_error_t
mp4d_saio_init(saio_reader_t *,
               mp4d_atom_t *
    );

/**
   @brief Get the aux info offset for the first sample in the next chunk/trun.
 */
mp4d_error_t
mp4d_saio_get_next(saio_reader_t *,
                   uint64_t current_offset,
                   uint64_t *p_offset       /**< [out] Either explicitly given by saio,
                                                       or inherited from the current_offset
                                                       (for later than the first chunk/trun) */
    );

#endif
