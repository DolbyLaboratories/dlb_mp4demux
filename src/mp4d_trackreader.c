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

#include "mp4d_trackreader.h"

#include "mp4d_nav.h"
#include "mp4d_internal.h"
#include "mp4d_box_read.h"

#include <stddef.h>
#include <assert.h>

struct mp4d_trackreader_t_
{
    uint32_t track_ID;                            /* 0 until initialized */
    uint32_t movie_time_scale, media_time_scale;  /* 0 until initialized */
    int is_qt;                                    /* Apple QuickTime? */

    mp4d_atom_t atom;
    uint64_t atom_offset;      /* in bytes */
    uint64_t abs_time_offset;  /* of current moof/moov atom (media time scale) */

    struct
    {
        /* Sample position */
        stsc_reader_t stsc;
        co_reader_t co;    /* stco XOR co64 */

        /* To support _next_sample() */
        uint64_t cur_sample_pos;
        uint32_t cur_sample_size;
        
        /* DTS, CTS */
        tts_reader_t stts;
        tts_reader_t ctts;

        /* Sample size */
        stsz_reader_t stz;  /* stsz XOR stz2 */
        
        /* Sample sync */
        stss_reader_t stss;
        
        /* Sample flags, also used for moof */
        sdtp_reader_t sdtp;
        stdp_reader_t stdp;
        padb_reader_t padb;
    } moov;

    /* Edit list */
    elst_reader_t elst;

    /* The elst atom has to be stored in the trackreader object because the client may not keep the
     * moov buffer in memory when the elst atom is needed (at any later time, when reading a moof) */
    struct
    {
        uint32_t version_and_flags;
        uint32_t entry_count;
        struct
        {
            uint64_t segment_duration;  /* reserve for worst case: version = 1 */
            int64_t media_time;
            int16_t media_rate_integer;
            int16_t media_rate_fraction;
        } entries[MP4D_MAX_EDITS];
    } elst_atom;  /* The members of this struct are not used,
                     except to reserve a block of memory of the right size */

    /* Sub-sample info */
    subs_reader_t subs;

    /* Sample aux info */
    saiz_reader_t saiz[MP4D_MAX_AUXDATA];
    saio_reader_t saio[MP4D_MAX_AUXDATA];
    uint8_t num_saiz, num_saio;
    uint64_t cur_aux_pos[MP4D_MAX_AUXDATA];

    /* Data required to decode piff 'senc' box */
    struct 
    {
        mp4d_buffer_t buffer;
        uint32_t version;
        uint32_t flags;
        uint32_t sampleCount;

        /* Default encryption data */
        uint32_t default_algorithmID;
        uint8_t default_iv_size;
        uint8_t default_kid[16];

        /* override data valid if flags & 0x01 */
        uint32_t override_algorithmID;
        uint8_t override_iv_size;
        uint8_t override_kid[16];
       
    } piff_senc_reader;

    int have_trex;   /* boolean */
    struct 
    {
        uint32_t default_sample_description_index;
        uint32_t default_sample_duration;
        uint32_t default_sample_size;
        uint32_t default_sample_flags;
    } trex;

    /* Data describing the current moof/trun. Does not change during sample iteration */
    struct
    {
        int num_traf;  /* Number of traf with matching track_ID,
                          Need > 0, > 1 is unsupported */
        uint32_t traf_number;  /* Number of non-matching (considering track_ID)
                                  traf boxes before the first matching
                                  traf box in moof. This is needed for the
                                  case when base_data_offset is not present. */

        int have_tfdt; /* boolean */
        uint64_t tfdt_baseMediaDecodeTime;

        struct
        {
            uint32_t tf_flags;
            uint64_t base_data_offset;          /* explicit from tfhd, or implicitly inferred */
            
            uint32_t sample_description_index;  /* global per traf */
            uint32_t default_sample_duration;
            uint32_t default_sample_size;
            uint32_t default_sample_flags;
        } tfhd;   /* Not the raw tfhd, but tfhd updated with trex (if available) */

        uint32_t num_trun; /* Number of trun s in the traf */

        struct
        {
            uint8_t version;
            uint32_t tr_flags;
            uint32_t sample_count;
            int32_t data_offset;
            uint32_t first_sample_flags;
        } trun;

        /*trick box info*/
        trik_reader_t trik;

        /*senc box info*/
        senc_reader_t senc;        
    } moof;

    /* state variables used by moof_next_sample */
    struct
    {
        uint32_t current_trun; /* Index of current trun, < num_trun */
        mp4d_buffer_t current_trun_sample;
        uint64_t cur_data_offset;
        uint32_t samples_left; /* in current trun */
    } moof_iter;

    /* State which is retained between fragments */
    uint64_t cur_dts;  /* DTS since beginning of movie */
};

/**************************************************
    Constants
**************************************************/
static const uint32_t CENC = 1667591779;

/**************************************************
    Track reader callbacks
**************************************************/

static int
tr_parse_trak
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav
    )
{
    /* Parse the track header (tkhd atom), then
       parse mdia only if track_ID is the requested one.
    */
    mp4d_trackreader_ptr_t p_tr = p_nav->p_data;
    mp4d_atom_t tkhd;

    if (mp4d_find_atom(&atom, "tkhd", 0, &tkhd) != MP4D_NO_ERROR)
    {
        debug(("Missing trak:tkhd\n"));
        return MP4D_E_UNSUPPRTED_FORMAT;
    }

    /* Parse the child boxes if the track_ID matches the requested ID */
    {
        mp4d_trak_t trak_info;
        mp4d_error_t err;
        struct mp4d_demuxer_t_ demuxer;

        demuxer.curr.moov.p_trak = &trak_info;
        p_nav->p_data = &demuxer;
        err = mp4d_parse_tkhd(tkhd, p_nav);
        p_nav->p_data = p_tr;
        if (err != MP4D_NO_ERROR)
        {
            return err;
        }

        debug(("Found trak:tkhd for track_ID = %d (need %d)\n",
               trak_info.info.track_id,
               p_tr->track_ID));
        
        if (trak_info.info.track_id == p_tr->track_ID)
        {
            return mp4d_parse_box(atom, p_nav);
        }
    }

    return MP4D_NO_ERROR;
}

static int
tr_parse_elst
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav
    )
{
    mp4d_trackreader_ptr_t p_tr = p_nav->p_data;

    if (p_tr->elst.buffer.p_data != NULL)
    {
        warning(("Multiple elst boxes found, using the last one\n"));
    }

    if (atom.size <= (uint64_t) sizeof(p_tr->elst_atom))
    {
        mp4d_atom_t tmp;

        memcpy(&p_tr->elst_atom, atom.p_data, (size_t) atom.size);

        /* The elst reader needs an atom (with only .size and .p_data) as input */
        tmp.p_data = (unsigned char *)&p_tr->elst_atom;
        tmp.size = atom.size;

        return mp4d_elst_init(&p_tr->elst,
                &tmp,
                p_tr->media_time_scale,
                p_tr->movie_time_scale
        );
    }
    else
    {
        ASSURE( 0, MP4D_E_UNSUPPRTED_FORMAT,
                ("Sorry, supports elst box of size <= %" PRIz " bytes (%d entries), found elst box of size %" PRIu64 " bytes",
                        sizeof(p_tr->elst_atom),
                        MP4D_MAX_EDITS,
                        atom.size) );
    }
}

static int
tr_parse_subs
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav
    )
{
    mp4d_trackreader_ptr_t p_tr = p_nav->p_data;

    if (p_tr->subs.buffer.p_data != NULL)
    {
        warning(("Multiple subs boxes found, using the last one\n"));
    }
    return mp4d_subs_init(&p_tr->subs, 
                          &atom);
}

static int
tr_parse_saiz
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav
    )
{
    mp4d_trackreader_ptr_t p_tr = p_nav->p_data;

    ASSURE(p_tr->num_saiz < MP4D_MAX_AUXDATA, MP4D_E_UNSUPPRTED_FORMAT, 
               ("Found saiz box number %d, up to %d is supported", 
                p_tr->num_saiz+1,
                MP4D_MAX_AUXDATA)
        );

    CHECK( mp4d_saiz_init(&p_tr->saiz[p_tr->num_saiz], 
                          &atom) );

    /* Ignore if not CENC (common encryption) */
    if (p_tr->saiz[p_tr->num_saiz].aux_info_type == CENC)
    {
        p_tr->num_saiz++;
    }

    return 0;
}

static int
tr_parse_saio
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav
    )
{
    mp4d_trackreader_ptr_t p_tr = p_nav->p_data;

    ASSURE(p_tr->num_saio < MP4D_MAX_AUXDATA, MP4D_E_UNSUPPRTED_FORMAT, 
               ("Found saio box number %d, up to %d is supported", 
                p_tr->num_saio+1,
                MP4D_MAX_AUXDATA)
        );

    return mp4d_saio_init(&p_tr->saio[p_tr->num_saio++], 
                          &atom);
}

static int
tr_parse_piff_senc
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav
    )
{
    mp4d_trackreader_ptr_t p_tr = p_nav->p_data;

    /* Read the header of this UUID piff */
    mp4d_buffer_t* pBuffer = 0;
    p_tr->piff_senc_reader.buffer = mp4d_atom_to_buffer(&atom);
    pBuffer = &p_tr->piff_senc_reader.buffer;
    {
        p_tr->piff_senc_reader.version = mp4d_read_u8(pBuffer);
        p_tr->piff_senc_reader.flags = mp4d_read_u24(pBuffer);
        ASSURE( p_tr->piff_senc_reader.version == 0 , MP4D_E_UNSUPPRTED_FORMAT, 
            ("Unknown picc 'senc' version %" PRIu32, p_tr->piff_senc_reader.version) );
    }
    
    if (p_tr->piff_senc_reader.flags & 0x01)
    {
        /* Override track encryption entry*/
        p_tr->piff_senc_reader.override_algorithmID = mp4d_read_u24(pBuffer);
        p_tr->piff_senc_reader.override_iv_size = mp4d_read_u8(pBuffer);
        mp4d_read(pBuffer, p_tr->piff_senc_reader.override_kid, 16);
    }

    /* Work out the total number of samples */
    p_tr->piff_senc_reader.sampleCount = mp4d_read_u32(pBuffer);

    /* Flag this as the beginning of the per sample data */
    p_tr->piff_senc_reader.buffer.p_begin = p_tr->piff_senc_reader.buffer.p_data;

    return mp4d_is_buffer_error(pBuffer) ? MP4D_E_INVALID_ATOM : MP4D_NO_ERROR;
}

static int
tr_parse_stsc
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav
    )
{
    mp4d_trackreader_ptr_t p_tr = p_nav->p_data;

    if (p_tr->moov.stsc.buffer.p_data != NULL)
    {
        warning(("Multiple stsc boxes found, using the last one\n"));
    }
    return mp4d_stsc_init(&p_tr->moov.stsc, &atom);
}

static int
tr_parse_co
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav
    )
{
    mp4d_trackreader_ptr_t p_tr = p_nav->p_data;
    int is_co64 = MP4D_FOURCC_EQ(atom.type, "co64");

    if (p_tr->moov.co.chunk_offsets.p_data != NULL)
    {
        warning(("Multiple stco/co64 boxes found, using the last one\n"));
    }
    return mp4d_co_init(&p_tr->moov.co, &atom, is_co64);
}

static int
tr_parse_stts
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav
    )
{
    mp4d_trackreader_ptr_t p_tr = p_nav->p_data;
    int delta_encoded = 1;

    if (p_tr->moov.stts.buffer.p_data != NULL)
    {
        warning(("Multiple stts boxes found, using the last one\n"));
    }
    
    return mp4d_tts_init(&p_tr->moov.stts, &atom, delta_encoded);
}

static int
tr_parse_ctts
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav
    )
{
    mp4d_trackreader_ptr_t p_tr = p_nav->p_data;
    int delta_encoded = 0;

    if (p_tr->moov.ctts.buffer.p_data != NULL)
    {
        warning(("Multiple ctts boxes found, using the last one\n"));
    }
    
    return mp4d_tts_init(&p_tr->moov.ctts, &atom, delta_encoded);
}

static int
tr_parse_stz
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav
    )
{
    mp4d_trackreader_ptr_t p_tr = p_nav->p_data;
    int is_stz2 = MP4D_FOURCC_EQ(atom.type, "stz2");

    if (p_tr->moov.stz.buffer.p_data != NULL)
    {
        warning(("Multiple stsz/stz2 boxes found, using the last stsz\n"));
    }

    return mp4d_stsz_init(&p_tr->moov.stz, &atom, is_stz2);
}


static int
tr_parse_stss
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav
    )
{
    mp4d_trackreader_ptr_t p_tr = p_nav->p_data;

    if (p_tr->moov.stss.buffer.p_data != NULL)
    {
        warning(("Multiple stss boxes found, using the last stss\n"));
    }

    return mp4d_stss_init(&p_tr->moov.stss, &atom);
}

static int
tr_parse_sdtp
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav
    ,int is_moov
    )
{
    mp4d_trackreader_ptr_t p_tr = p_nav->p_data;

    if (is_moov)
    {
        ASSURE( p_tr->moov.stz.buffer.p_data != NULL, MP4D_E_UNSUPPRTED_FORMAT,
            ("Cannot read sdtp without preceding stsz/stz2 box (need to know the entry count)") );
    }

    if (p_tr->moov.sdtp.buffer.p_data != NULL)
    {   
        warning(("Multiple sdtp boxes found, using the last sdtp\n"));
    }

    return mp4d_sdtp_init(&p_tr->moov.sdtp, 
                          &atom,
                          is_moov ? p_tr->moov.stz.sample_count : p_tr->moof.trun.sample_count);
}

static int
tr_parse_sdtp_moov
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav)
{
    return tr_parse_sdtp(atom, p_nav, 1);
}
static int
tr_parse_sdtp_moof
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav)
{
    return tr_parse_sdtp(atom, p_nav, 0);
}

static int
tr_parse_stdp
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav
    ,int is_moov
    )
{
    mp4d_trackreader_ptr_t p_tr = p_nav->p_data;

    ASSURE( !is_moov || p_tr->moov.stz.buffer.p_data != NULL, MP4D_E_UNSUPPRTED_FORMAT,
            ("Cannot read stdp without preceding stsz/stz2 box (need to know the entry count)") );

    if (p_tr->moov.stdp.buffer.p_data != NULL)
    {   
        warning(("Multiple stdp boxes found, using the last stdp\n"));
    }

    return mp4d_stdp_init(&p_tr->moov.stdp, 
                          &atom,
                          is_moov ? p_tr->moov.stz.sample_count : p_tr->moof.trun.sample_count);
}

static int
tr_parse_trik
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav
    )
{
    mp4d_trackreader_ptr_t p_tr = p_nav->p_data;

    if (p_tr->moof.trik.buffer.p_data != NULL)
    {
        warning(("Multiple trik boxes found, using the last trik\n"));
    }

    return mp4d_trik_init(&p_tr->moof.trik, &atom, p_tr->moof.trun.sample_count);
}

static int
tr_parse_senc
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav
    )
{
    mp4d_trackreader_ptr_t p_tr = p_nav->p_data;

    if (p_tr->moof.senc.buffer.p_data != NULL)
    {
        warning(("Multiple trik boxes found, using the last trik\n"));
    }

    return mp4d_senc_init(&p_tr->moof.senc, &atom);
}

static int
tr_parse_stdp_moov
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav
    )
{
    return tr_parse_stdp(atom, p_nav, 1);
}
static int
tr_parse_stdp_moof
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav
    )
{
    return tr_parse_stdp(atom, p_nav, 0);
}

static int
tr_parse_padb(
    mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav
    )
{
    mp4d_trackreader_ptr_t p_tr = p_nav->p_data;

    if (p_tr->moov.padb.buffer.p_data != NULL)
    {
        warning(("Multiple padb boxes found, using the last padb\n"));
    }
    return mp4d_padb_init(&p_tr->moov.padb, &atom);
}

/**
   @return MP4D_E_INFO_NOT_AVAIL if the track_ID does not match the requested track_ID
 */
static int
tr_parse_tfhd
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav
    )
{
    mp4d_trackreader_ptr_t p_tr = p_nav->p_data;
    mp4d_buffer_t p = mp4d_atom_to_buffer(&atom);
    uint8_t version = mp4d_read_u8(&p);
    uint32_t tf_flags = mp4d_read_u24(&p);
    uint32_t track_ID = mp4d_read_u32(&p);

    ASSURE( version == 0, MP4D_E_UNSUPPRTED_FORMAT,
            ("Unsupported tfhd version: %" PRIu8, version) );

    debug(("Found traf:tfhd for track_ID = %d (need %d)\n",
           track_ID,
           p_tr->track_ID));
    
    if (track_ID != p_tr->track_ID)
    {
        /* Count non-matches before first match */
        if (p_tr->moof.num_traf == 0)
        {
            p_tr->moof.traf_number++;
        }

        return MP4D_E_INFO_NOT_AVAIL;
    }
    else  /* matches track_ID */
    {
        p_tr->moof.num_traf++;
        p_tr->moof.num_trun = 0;
        p_tr->moof.tfhd.tf_flags = tf_flags;

        if (p_tr->moof.tfhd.tf_flags & 0x000001)
        {
            p_tr->moof.tfhd.base_data_offset = mp4d_read_u64(&p);
        }
        if (p_tr->moof.tfhd.tf_flags & 0x000002)
        {
            p_tr->moof.tfhd.sample_description_index = mp4d_read_u32(&p);
        }
        if (p_tr->moof.tfhd.tf_flags & 0x000008)
        {
            p_tr->moof.tfhd.default_sample_duration = mp4d_read_u32(&p);
        }
        if (p_tr->moof.tfhd.tf_flags & 0x000010)
        {
            p_tr->moof.tfhd.default_sample_size = mp4d_read_u32(&p);
        }
        if (p_tr->moof.tfhd.tf_flags & 0x000020)
        {
            p_tr->moof.tfhd.default_sample_flags = mp4d_read_u32(&p);
        }

        return MP4D_NO_ERROR;
    }      
}

static int
tr_parse_traf
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav
    )
{
    mp4d_trackreader_ptr_t p_tr = p_nav->p_data;
    mp4d_atom_t tfhd;

    /* Parse the child boxes only if the tfhd track_ID matches the requested ID */
    if (mp4d_find_atom(&atom, "tfhd", 0, &tfhd) != MP4D_NO_ERROR)
    {
        debug(("Missing traf:tfhd\n"));
        return MP4D_E_UNSUPPRTED_FORMAT;
    }

    {
        mp4d_error_t err;

        err = tr_parse_tfhd(tfhd, p_nav);

        if (err == MP4D_E_INFO_NOT_AVAIL)
        {
            /* Count non-matches before first match */
            if (p_tr->moof.num_traf == 0)
            {
                p_tr->moof.traf_number++;
            }
            return MP4D_NO_ERROR;
        }
        else if (err != MP4D_NO_ERROR)
        {
            return err;
        }
        else
        {
            /* track_ID matches */
            p_tr->moof.have_tfdt = 0;
            return mp4d_parse_box(atom, p_nav);
        }
    }
}

static int
tr_parse_tfdt
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav
    )
{
    mp4d_trackreader_ptr_t p_tr = p_nav->p_data;
    mp4d_buffer_t p = mp4d_atom_to_buffer(&atom);

    uint8_t version = mp4d_read_u8(&p);

    mp4d_read_u24(&p); /* flags */
    p_tr->moof.have_tfdt = 1;
    if (version == 1)
    {
        p_tr->moof.tfdt_baseMediaDecodeTime = mp4d_read_u64(&p);
    }
    else
    {
        p_tr->moof.tfdt_baseMediaDecodeTime = mp4d_read_u32(&p);
    }
    return MP4D_NO_ERROR;
}

static int
tr_parse_trun
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav
    )
{
    mp4d_trackreader_ptr_t p_tr = p_nav->p_data;
    mp4d_buffer_t p = mp4d_atom_to_buffer(&atom);

    p_tr->moof.trun.version = mp4d_read_u8(&p);
    ASSURE( p_tr->moof.trun.version == 0 ||
            p_tr->moof.trun.version == 1, MP4D_E_UNSUPPRTED_FORMAT,
            ("Unsupported trun version: %" PRIu8, p_tr->moof.trun.version) );

    if (p_tr->moof.num_trun == p_tr->moof_iter.current_trun)
    {
        p_tr->moof.trun.tr_flags = mp4d_read_u24(&p);
        p_tr->moof.trun.sample_count = mp4d_read_u32(&p);

        if (p_tr->moof.trun.tr_flags & 0x000001)
        {
            p_tr->moof.trun.data_offset = (int32_t) mp4d_read_u32(&p);
        }
        if (p_tr->moof.trun.tr_flags & 0x000004)
        {
            p_tr->moof.trun.first_sample_flags = mp4d_read_u32(&p);
        }
        p_tr->moof_iter.current_trun_sample = p;  /* struct assignment */
    }
    p_tr->moof.num_trun++;

    return MP4D_NO_ERROR;
}


static int
tr_parse_trex
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav
    )
{
    mp4d_trackreader_ptr_t p_tr = p_nav->p_data;
    mp4d_buffer_t p = mp4d_atom_to_buffer(&atom);
    uint8_t version = mp4d_read_u8(&p);
    uint32_t flags = mp4d_read_u24(&p);

    ASSURE( version == 0, MP4D_E_UNSUPPRTED_FORMAT,
            ("Unsupported trex version: %" PRIu8, version) );
    ASSURE( flags == 0, MP4D_E_UNSUPPRTED_FORMAT,
            ("Unsupported trex flags: %" PRIu32, flags) );

    if (mp4d_read_u32(&p) != p_tr->track_ID)
    {
        /* Not for this track */
        return MP4D_NO_ERROR;
    }

    p_tr->have_trex = 1;
    p_tr->trex.default_sample_description_index = mp4d_read_u32(&p);
    p_tr->trex.default_sample_duration = mp4d_read_u32(&p);
    p_tr->trex.default_sample_size = mp4d_read_u32(&p);
    p_tr->trex.default_sample_flags = mp4d_read_u32(&p);

    return MP4D_NO_ERROR;
}


static const mp4d_callback_t k_dispatcher_track_reader[] = 
{
    {"moov", &mp4d_parse_box},
    {"trak", &tr_parse_trak},
    {"edts", &mp4d_parse_box},
    {"elst", &tr_parse_elst},
    {"mdia", &mp4d_parse_box},
    {"minf", &mp4d_parse_box},
    {"stbl", &mp4d_parse_box},

    {"stsc", &tr_parse_stsc},
    {"stco", &tr_parse_co},
    {"co64", &tr_parse_co},

    {"stts", &tr_parse_stts},
    {"ctts", &tr_parse_ctts},

    {"stsz", &tr_parse_stz},
    {"stz2", &tr_parse_stz},

    {"stss", &tr_parse_stss},

    {"subs", &tr_parse_subs},
    {"saiz", &tr_parse_saiz},
    {"saio", &tr_parse_saio},

    /* sample flags */
    {"sdtp", &tr_parse_sdtp_moov},
    {"stdp", &tr_parse_stdp_moov},
    {"padb", &tr_parse_padb},

    /* defaults for moof */ 
    {"mvex", &mp4d_parse_box},
    {"trex", &tr_parse_trex},

    {"dumy", NULL}  /* sentinel */
};

static const mp4d_callback_t k_uuid_dispatcher_track_reader[] = 
{  
    /* sample encryption */
    {"\xA2\x39\x4F\x52\x5A\x9B\x4f\x14\xA2\x44\x6C\x42\x7C\x64\x8D\xF4", &tr_parse_piff_senc}, /* Microsoft 'senc'*/
    
    /* sentinel */
    {"dumy", NULL}
    
};

static const mp4d_callback_t k_dispatcher_moof_reader[] =
{
    {"moof", &mp4d_parse_box},
    {"traf", &tr_parse_traf},
    {"tfdt", &tr_parse_tfdt},
    {"trun", &tr_parse_trun},
    {"subs", &tr_parse_subs},
    {"saiz", &tr_parse_saiz},
    {"saio", &tr_parse_saio},

    /* sample flags */
    {"sdtp", &tr_parse_sdtp_moof},
    {"stdp", &tr_parse_stdp_moof},
    {"padb", &tr_parse_padb},
    {"trik", &tr_parse_trik},
    {"senc", &tr_parse_senc},

    {"dumy", NULL}
};

/**
   Updates tfhd data incl. tf_flags with trex info not already available in tfhd
 */
static int
update_tfhd_with_trex(mp4d_trackreader_ptr_t p_tr)
{
    assert(p_tr->have_trex);

    /* base_data_offset is not available from trex */

    if (!(p_tr->moof.tfhd.tf_flags & 0x000002))
    {
        p_tr->moof.tfhd.sample_description_index = p_tr->trex.default_sample_description_index;
        p_tr->moof.tfhd.tf_flags |= 0x000002;
    }
    if (!(p_tr->moof.tfhd.tf_flags & 0x000008))
    {
        p_tr->moof.tfhd.default_sample_duration = p_tr->trex.default_sample_duration;
        p_tr->moof.tfhd.tf_flags |= 0x000008;        
    }
    if (!(p_tr->moof.tfhd.tf_flags & 0x000010))
    {
        p_tr->moof.tfhd.default_sample_size = p_tr->trex.default_sample_size;
        p_tr->moof.tfhd.tf_flags |= 0x000010;
    }
    if (!(p_tr->moof.tfhd.tf_flags & 0x000020))
    {
        p_tr->moof.tfhd.default_sample_flags = p_tr->trex.default_sample_flags;
        p_tr->moof.tfhd.tf_flags |= 0x000020;
    }

    return MP4D_NO_ERROR;
}

/** @brief set the current sample aux offset, at the beginning of each trun
 *    
 *  This depends on the base_data_offset for the current traf.
 */
static mp4d_error_t
moof_set_aux_offset(mp4d_trackreader_ptr_t p_tr)
{
    uint8_t i;
    
    for (i = 0; i < p_tr->num_saio; i++)
    {
        /* Offsets in saio are relative to base_data_offset (for the traf) */
        uint64_t offset = p_tr->cur_aux_pos[i] - p_tr->moof.tfhd.base_data_offset;
        
        CHECK( mp4d_saio_get_next(&p_tr->saio[i],
                                  offset,
                                  &offset)
            );
        
        p_tr->cur_aux_pos[i] = offset + p_tr->moof.tfhd.base_data_offset;
    }

    return MP4D_NO_ERROR;
}



/** @brief Parses a moof

    trun_index: requested trun, counting from zero
 */
static mp4d_error_t
get_next_trun(mp4d_trackreader_ptr_t p_tr,
              uint32_t trun_index)
{
    struct mp4d_navigator_t_ nav;

    /* Parse only moof and child boxes */
    mp4d_navigator_init(&nav, 
        k_dispatcher_moof_reader, 
        k_uuid_dispatcher_track_reader, 
        p_tr);

    /* Reset memory. Set counters (num_traf, traf_number) to zero */
    mp4d_memset(&p_tr->moof, 0, sizeof(p_tr->moof));
    p_tr->moof_iter.current_trun = trun_index;
    
    CHECK( mp4d_parse_box(p_tr->atom, &nav) );
    
    ASSURE( p_tr->moof.num_traf > 0,
                MP4D_E_TRACK_NOT_FOUND,
                ("Missing moof:traf for track_ID %d", p_tr->track_ID) );
    
    ASSURE( p_tr->moof.num_traf == 1,
                MP4D_E_UNSUPPRTED_FORMAT,
                ("Too many (%" PRIu32 ") moof:traf for track_ID %" PRIu32 ", need 1",
                 p_tr->moof.num_traf, p_tr->track_ID)) ;  /* >1 legal but unsupported */
    
    if (p_tr->have_trex)
    {
        update_tfhd_with_trex(p_tr);
    }
    
    debug(("tf_flags = %" PRIu32 "\n", p_tr->moof.tfhd.tf_flags));
    debug(("base_data_offset = %" PRIu64 "\n", p_tr->moof.tfhd.base_data_offset));
    debug(("sample_description_index = %" PRIu32 "\n", p_tr->moof.tfhd.sample_description_index));
    debug(("default_sample_duration = %" PRIu32 "\n", p_tr->moof.tfhd.default_sample_duration));
    debug(("default_sample_size = %" PRIu32 "\n", p_tr->moof.tfhd.default_sample_size));
    debug(("default_sample_flags = %" PRIu32 "\n", p_tr->moof.tfhd.default_sample_flags));
    
    p_tr->moof_iter.samples_left = p_tr->moof.trun.sample_count;
    
    return MP4D_NO_ERROR;
}

/**
    @brief read sample aux info, only CENC encryption

    Shared by moof/moov reader
 */
static mp4d_error_t
get_sample_aux(mp4d_trackreader_ptr_t p_tr,
               mp4d_sampleref_t *p_sample)
{
    uint8_t i;
    int numAux = 0;
 
    for (i = 0; i < p_tr->num_saiz; i++)
    {
        p_sample->auxdata[i].datatype = p_tr->saiz[i].aux_info_type;

        CHECK( mp4d_saiz_get_next_size(&p_tr->saiz[i],
                                       &p_sample->auxdata[i].size)
            );

        if (p_sample->auxdata[i].size > 0)
        {
            uint8_t j;
            int found;

            /* Get the offset from the matching (i.e., equal aux_info_type) saio box */
            found = 0;
            for (j = 0; j < p_tr->num_saio; j++)
            {
                if (p_tr->saio[j].aux_info_type == p_tr->saiz[i].aux_info_type)
                {
                    p_sample->auxdata[i].pos = p_tr->cur_aux_pos[j];
                    p_tr->cur_aux_pos[j] += p_sample->auxdata[i].size;
                    found = 1;
                    break;
                }
            }
            ASSURE( found, MP4D_E_UNSUPPRTED_FORMAT,
                        ("Missing saio box for saiz with aux_info_type = %d", p_tr->saiz[i].aux_info_type) );
        } 
        else
        {
            /* do not write aux offset, do not move cur_aux_pos */
        }
        
    }

    numAux += p_tr->num_saiz;

    /* Now check to see if there is any information in a piff senc? */
    if (p_tr->piff_senc_reader.buffer.size > 0)
    {
        uint32_t piffAuxIndex = numAux++;
        uint8_t ivSize;
        mp4d_buffer_t buffer;
        uint8_t bufferSize;

        ASSURE(numAux < MP4D_MAX_AUXDATA, MP4D_E_UNSUPPRTED_FORMAT,
            ("Too much Aux data, unable to deliver PIFF SENC data") );

        /* Work out the size of this data and give it to the user */
        p_sample->auxdata[piffAuxIndex].datatype = 0x70696666; /* piff */
        p_sample->auxdata[piffAuxIndex].pos = (uint64_t)(intptr_t)p_tr->piff_senc_reader.buffer.p_data;
        
        /* Work out IV size: */
        ivSize = p_tr->piff_senc_reader.default_iv_size;
        if (p_tr->piff_senc_reader.flags & 0x01)
            ivSize = p_tr->piff_senc_reader.override_iv_size;

        /* Work out the size of this data:*/
        bufferSize = ivSize;
        
        /* make a temporary buffer to read ahead a little with*/
        buffer = p_tr->piff_senc_reader.buffer;
        mp4d_skip_bytes(&buffer, ivSize);

        if (p_tr->piff_senc_reader.flags & 0x02)
        {
            uint16_t numEntries = mp4d_read_u16(&buffer);
            uint32_t subSampleDataSize = numEntries * 6;
            mp4d_skip_bytes(&buffer, subSampleDataSize);

            bufferSize += 2 + subSampleDataSize;
        }

        p_sample->auxdata[piffAuxIndex].size = bufferSize;

        /* Push the buffer's data forward */
        mp4d_skip_bytes(&p_tr->piff_senc_reader.buffer, bufferSize);
    }

    for (i = numAux; i < MP4D_MAX_AUXDATA; i++)
    {
        p_sample->auxdata[i].size = 0; /* no further aux info is available */
    }

    return MP4D_NO_ERROR;
}

static int
moof_next_sample(mp4d_trackreader_ptr_t p_tr,
                 mp4d_sampleref_t * sample_ptr_out)
{
    uint32_t sample_duration;

    if (p_tr->moof_iter.current_trun == 0 && 
         p_tr->moof_iter.samples_left == p_tr->moof.trun.sample_count)
    {
        sample_ptr_out->is_first_sample_in_segment = 1;
    }
    else
    {
        sample_ptr_out->is_first_sample_in_segment = 0;
    }

    while (p_tr->moof_iter.samples_left == 0)
    {
        ASSURE( p_tr->moof_iter.current_trun + 1 < p_tr->moof.num_trun, MP4D_E_NEXT_SEGMENT,
                ("track_ID %" PRIu32 ": Out of trun(s) (after %" PRIu32 " trun(s))", 
                 p_tr->track_ID, p_tr->moof.num_trun) );

        CHECK( get_next_trun(p_tr, p_tr->moof_iter.current_trun + 1) );

        if (p_tr->moof.trun.tr_flags & 0x000001)
        {
            p_tr->moof_iter.cur_data_offset = p_tr->moof.tfhd.base_data_offset + p_tr->moof.trun.data_offset;
        }
        else
        {
            /* Data for this trun starts where the data for the previous trun ended. */
        }

        CHECK( moof_set_aux_offset(p_tr) );
    }

    /* DTS */
    sample_ptr_out->dts = p_tr->cur_dts;
    if (p_tr->moof.trun.tr_flags & 0x000100)
    {
        sample_duration = mp4d_read_u32(&p_tr->moof_iter.current_trun_sample);
    }
    else if (p_tr->moof.tfhd.tf_flags & 0x000008)
    {
        sample_duration = p_tr->moof.tfhd.default_sample_duration;
    }
    else
    {
        ASSURE( 0, MP4D_E_INFO_NOT_AVAIL, 
                    ("Sample DTS is not available from moof:traf:trun or "
                     "from moof:traf:tfhd or from moov:mvex:trex") );
    }

    /* size */
    if (p_tr->moof.trun.tr_flags & 0x000200)
    {
        sample_ptr_out->size = mp4d_read_u32(&p_tr->moof_iter.current_trun_sample);
    }
    else if (p_tr->moof.tfhd.tf_flags & 0x000010)
    {
        sample_ptr_out->size = p_tr->moof.tfhd.default_sample_size;
    }
    else
    {
        ASSURE( 0, MP4D_E_INFO_NOT_AVAIL,
                    ("Sample size is not available from moof:traf:trun or "
                     "from moof:traf:tfhd or from moov:mvex:trex") );
    }

    /* pos */
    sample_ptr_out->pos = p_tr->moof_iter.cur_data_offset;
    p_tr->moof_iter.cur_data_offset += sample_ptr_out->size;

    /* flags. boxes (sdtp, padb, stdp) take precedence of sample_flags */
    if (p_tr->moof.trun.tr_flags & 0x000400)
    {
        /* trun */
        sample_ptr_out->flags = mp4d_read_u32(&p_tr->moof_iter.current_trun_sample);
    }
    else if (p_tr->moof_iter.samples_left == p_tr->moof.trun.sample_count &&
            p_tr->moof.trun.tr_flags & 0x000004)
    {
        /* trun first sample and first_sample_flags is provided */
        sample_ptr_out->flags = p_tr->moof.trun.first_sample_flags;
    }
    else if ((p_tr->moof.tfhd.tf_flags & 0x000020)||(p_tr->have_trex == 0))
    {
        /* tfhd defaults (from trex);for PIFF streams, trex box located in moov;  */
        sample_ptr_out->flags = p_tr->moof.tfhd.default_sample_flags;
    }
    else
    {
        ASSURE( 0, MP4D_E_INFO_NOT_AVAIL,
                    ("Sample flags are not available from moof:traf:trun or "
                     "from moof:traf:tfhd or from moov:mvex:trex") );
    }

    /* flags - sdtp */
    if (p_tr->moov.sdtp.buffer.p_data != NULL)
    {
        /* Write sdtp flags from bit 20 */
        uint8_t sdtp_flags;

        CHECK( mp4d_sdtp_get_next(&p_tr->moov.sdtp, &sdtp_flags) );
        sample_ptr_out->flags &= 0xf00fffff;       /* 1111 0000 0000 1111 1111 1111 1111 1111 */
        sample_ptr_out->flags |= sdtp_flags << (3 + 1 + 16);
    }
    /* flags - padb */
    if (p_tr->moov.padb.buffer.p_data != NULL)
    {
        uint8_t sample_padding_value; /* 3 bits */
        CHECK( mp4d_padb_get_next(&p_tr->moov.padb, &sample_padding_value) );

        sample_ptr_out->flags &= 0xfff1ffff;       /* 1111 1111 1111 0001 1111 1111 1111 1111 */
        sample_ptr_out->flags |= (sample_padding_value & 0x7) << (1 + 16);
    }
    /* flags - stdp */
    if (p_tr->moov.stdp.buffer.p_data != NULL)
    {
        uint16_t sample_degradation_priority;  /* 16 bits */

        CHECK( mp4d_stdp_get_next(&p_tr->moov.stdp, &sample_degradation_priority) );
        sample_ptr_out->flags &= 0xffff0000;
        sample_ptr_out->flags |= sample_degradation_priority;
    }

    /* CTS */
    if (p_tr->moof.trun.tr_flags & 0x000800)
    {
        if (p_tr->moof.trun.version == 0)
        {
            sample_ptr_out->cts = sample_ptr_out->dts + mp4d_read_u32(&p_tr->moof_iter.current_trun_sample);
        }
        else
        {
            sample_ptr_out->cts = sample_ptr_out->dts + ((int32_t) mp4d_read_u32(&p_tr->moof_iter.current_trun_sample));
        }
    }
    else
    {
        sample_ptr_out->cts = sample_ptr_out->dts;
    }

    /* sample description index */
    if (p_tr->moof.tfhd.tf_flags & 0x000002)
    {
        sample_ptr_out->sample_description_index = p_tr->moof.tfhd.sample_description_index;
    }
    else
    {
        /* For reasons of PIFF playback, assume that the sample description index is 1, if not in tfhd (
         *  and not in moov:mvex:trex which is not available in PIFF */
        if (0)
        {
            ASSURE( 0, MP4D_E_INFO_NOT_AVAIL,
                    ("Sample description index is not available from moof:traf:tfhd "
                     "or from moov:mvex:trex") );
        }
        else
        {
            sample_ptr_out->sample_description_index = 1;
        }
    }

    /* trik box*/
    {
        if (p_tr->moof.trik.buffer.p_data != NULL)
        {
            CHECK(mp4d_trik_get_next(&p_tr->moof.trik, 
                                     &sample_ptr_out->pic_type, 
                                     &sample_ptr_out->dependency_level));
        }
        else
        {
            /* No trik box; Maybe it's an audio traf,or it's not a CFF file.*/
            sample_ptr_out->pic_type = 0;
            sample_ptr_out->dependency_level = 0;
        }
    }

    /* senc box*/
    {
        if (p_tr->moof.senc.buffer.p_data != NULL)
        {
            CHECK(mp4d_senc_get_next(&p_tr->moof.senc,                
                                     sample_ptr_out->sencdata.iv,
                                     p_tr->piff_senc_reader.default_iv_size,
                                     &sample_ptr_out->sencdata.subsample_count,
                                     (const uint8_t **)(&sample_ptr_out->sencdata.ClearEncryptBytes)
                                     ));
        }
        else
        {
            /* No senc box; Maybe it's not encrypted traf,or it's not a CFF file.*/
            sample_ptr_out->sencdata.subsample_count= 0;
            sample_ptr_out->sencdata.ClearEncryptBytes = NULL;
        }
    }

    /* edit list */
    {
        int err = mp4d_elst_get_presentation_time(&p_tr->elst,
                                                  sample_ptr_out->cts,
                                                  sample_duration,
                                                  &sample_ptr_out->pts,
                                                  &sample_ptr_out->presentation_offset,
                                                  &sample_ptr_out->presentation_duration);
        if (err == MP4D_E_INFO_NOT_AVAIL)
        {
            sample_ptr_out->pts = 0;                    /* (a valid pts) */
            sample_ptr_out->presentation_offset = 0;    /* (a valid offset) */
            sample_ptr_out->presentation_duration = 0;  /* signals that this sample is not part of the presentation */
        }
        else
        {
            CHECK( err );
        }
    }

    /* subsample */
    CHECK( mp4d_subs_get_next_count(&p_tr->subs,
                                    &sample_ptr_out->num_subsamples) );

    /* sample aux */
    CHECK( get_sample_aux(p_tr, sample_ptr_out) );

    p_tr->cur_dts += sample_duration;  /* Only update state on success */

    p_tr->moof_iter.samples_left--;

    return MP4D_NO_ERROR;
}

int
mp4d_trackreader_get_track_ID
(
    mp4d_trackreader_ptr_t p_tr,
    uint32_t *track_ID          /**< [out] */
)
{
    ASSURE( p_tr != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( track_ID != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );

    *track_ID = p_tr->track_ID;

    return MP4D_NO_ERROR;
}

int
mp4d_trackreader_get_time_scale
(
    mp4d_trackreader_ptr_t p_tr,
    uint32_t *movie_time_scale,         /**< [out] */
    uint32_t *media_time_scale          /**< [out] */
    )
{
    ASSURE( p_tr != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( movie_time_scale != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( media_time_scale != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );

    *movie_time_scale = p_tr->movie_time_scale;
    *media_time_scale = p_tr->media_time_scale;

    return MP4D_NO_ERROR;
}


int
mp4d_trackreader_next_sample 
(
    mp4d_trackreader_ptr_t p_tr,   /**<  */
    mp4d_sampleref_t *sample_ptr_out   /**<  */
)
{
    uint32_t sample_duration;

    ASSURE( p_tr != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( sample_ptr_out != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );   

    if (MP4D_FOURCC_EQ(p_tr->atom.type, "moof"))
    {
        return moof_next_sample(p_tr, sample_ptr_out);
    }

    assert( MP4D_FOURCC_EQ(p_tr->atom.type, "moov") );

    /* Sample DTS */
    CHECK( mp4d_tts_get_stts_next(&p_tr->moov.stts, 
                                  &sample_ptr_out->dts,
                                  &sample_duration) );

    /* Sample CTS */
    if (p_tr->moov.ctts.buffer.p_data != NULL)
    {
        uint32_t sample_offset;

        CHECK( mp4d_tts_get_ctts_next(&p_tr->moov.ctts,
                                      &sample_offset) );
        
        if (p_tr->is_qt)
        {
            /* offset is signed */
            sample_ptr_out->cts = sample_ptr_out->dts + (int32_t) sample_offset;
        }
        /* not quicktime stream */
        else
        {
            assert( 0 == p_tr->moov.ctts.tts_version ||
                    1 == p_tr->moov.ctts.tts_version );
            
            if (1 == p_tr->moov.ctts.tts_version)
            {
                sample_ptr_out->cts = sample_ptr_out->dts + (int32_t) sample_offset;
            }
            else
            {
                sample_ptr_out->cts = sample_ptr_out->dts + sample_offset;
            }
        }
    }
    else
    {
        sample_ptr_out->cts = sample_ptr_out->dts;
    }

    /* Sample size */
    CHECK( mp4d_stsz_get_next(&p_tr->moov.stz, &sample_ptr_out->size) );

    /* Sample flags */
    {
        uint8_t sdtp_flags;
        uint8_t sample_padding_value;
        uint16_t sample_degradation_priority;
        int is_sync;

        /* stss */
        CHECK( mp4d_stss_get_next(&p_tr->moov.stss, &is_sync) );

        /* sdtp */
        if (p_tr->moov.sdtp.buffer.p_data != NULL)
        {
            CHECK( mp4d_sdtp_get_next(&p_tr->moov.sdtp, &sdtp_flags) );
        }
        else
        {
            /* sdtp flags are unknown, except sample_depends_on */
            sdtp_flags = 0;
            if (is_sync)
            {
                /* I-sample, does not depend on other samples */
                sdtp_flags = 2 << (2 + 2);
            }
            else
            {
                sdtp_flags = 1 << (2 + 2);
            }
        }

        /* stdp */
        if (p_tr->moov.stdp.buffer.p_data != NULL)
        {
            CHECK( mp4d_stdp_get_next(&p_tr->moov.stdp, &sample_degradation_priority) );
        }
        else
        {
            sample_degradation_priority = 0;  /* priority semantics are defined in derived specs */
        }

        /* padb */
        if (p_tr->moov.padb.buffer.p_data != NULL)
        {
            CHECK( mp4d_padb_get_next(&p_tr->moov.padb, &sample_padding_value) );
        }
        else
        {
            sample_padding_value = 0;
        }

        sample_ptr_out->flags = 0;
        sample_ptr_out->flags |= sdtp_flags << (3 + 1 + 16);

        sample_ptr_out->flags |= sample_padding_value << (1 + 16);

        if (!is_sync)
        {
            sample_ptr_out->flags |= 1 << 16;
        }

        sample_ptr_out->flags |= sample_degradation_priority;
    }


    /* Sample position, and aux offset */
    {
        uint32_t chunk_index;
        uint32_t sample_index_in_chunk;
     
        CHECK( mp4d_stsc_get_next(&p_tr->moov.stsc, 
                                  &chunk_index, 
                                  &sample_ptr_out->sample_description_index,
                                  &sample_index_in_chunk)
            );

        sample_ptr_out->samples_per_chunk = p_tr->moov.stsc.cur_samples_per_chunk;
        if (sample_index_in_chunk == 0)
        {
            uint8_t i;

            /* Sample offset = chunk offset */
            CHECK( mp4d_co_get_next(&p_tr->moov.co, &sample_ptr_out->pos) );

            /* Reset sample aux info offset */
            for (i = 0; i < p_tr->num_saio; i++)
            {
                CHECK( mp4d_saio_get_next(&p_tr->saio[i],
                                          p_tr->cur_aux_pos[i],
                                          &p_tr->cur_aux_pos[i])
                    );
            }
        }
        else
        {
            /* This sample starts where the previous sample ends */
            sample_ptr_out->pos = 
                p_tr->moov.cur_sample_pos +
                p_tr->moov.cur_sample_size;
        }
        p_tr->moov.cur_sample_pos  = sample_ptr_out->pos;
        p_tr->moov.cur_sample_size = sample_ptr_out->size;
    }

    {
        int err = mp4d_elst_get_presentation_time(&p_tr->elst,
                                                  sample_ptr_out->cts,
                                                  sample_duration,
                                                  &sample_ptr_out->pts,
                                                  &sample_ptr_out->presentation_offset,
                                                  &sample_ptr_out->presentation_duration);
        if (err == MP4D_E_INFO_NOT_AVAIL)
        {
            sample_ptr_out->pts = 0;                    /* (a valid pts) */
            sample_ptr_out->presentation_offset = 0;    /* (a valid offset) */
            sample_ptr_out->presentation_duration = 0;  /* signals that this sample is not part of the presentation */
        }
        else
        {
            CHECK( err );
        }
    }

    /* subsample */
    CHECK( mp4d_subs_get_next_count(&p_tr->subs,
                                    &sample_ptr_out->num_subsamples) );

    p_tr->cur_dts = sample_ptr_out->dts;

    CHECK( get_sample_aux(p_tr, sample_ptr_out) );

    /* picture type and dependency level */
    {
            sample_ptr_out->pic_type = 0;              /* no trik box in moov, set 0 means unknown. */
            sample_ptr_out->dependency_level = 0;      /* no trik box in moov, set 0 means unknown. */
    }
    
    return MP4D_NO_ERROR;
}

int
mp4d_trackreader_next_subsample
(
    mp4d_trackreader_ptr_t p_tr,
    const mp4d_sampleref_t *p_sample,
    uint64_t *p_offset,
    uint32_t *p_size
)
{
    uint32_t offset_in_sample;

    ASSURE( p_tr != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( p_sample != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( p_offset != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( p_size != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    
    CHECK( mp4d_subs_get_next_size(&p_tr->subs,
                                   p_sample->size,
                                   p_size,
                                   &offset_in_sample) );

    *p_offset = p_sample->pos + offset_in_sample;

    return MP4D_NO_ERROR;
}

int
mp4d_trackreader_get_stss_count
(
    mp4d_trackreader_ptr_t p_tr,
    uint32_t *count,         /**< [out] */
    unsigned char **stts_content
    )
{
    *count = p_tr->moov.stss.count;
    *stts_content = p_tr->moov.stss.stts_content;

    return 0;
}


static mp4d_error_t
init_segment(mp4d_trackreader_ptr_t p_tr)
{    
    p_tr->cur_dts = p_tr->abs_time_offset;

    if (MP4D_FOURCC_EQ(p_tr->atom.type, "moov"))
    {
        /* Use a local demuxer callback object in order to be multi-thread safe. */
        struct mp4d_navigator_t_ nav;

        /* Parse the boxes which are relevant for the track reader. */
        mp4d_navigator_init(&nav, 
            k_dispatcher_track_reader, 
            k_uuid_dispatcher_track_reader,
            p_tr);

        /* Reset all data pointers to NULL */
        mp4d_memset(&p_tr->moov, 0, sizeof(p_tr->moov));
        mp4d_memset(&p_tr->elst, 0, sizeof(p_tr->elst));
        mp4d_memset(&p_tr->subs, 0, sizeof(p_tr->subs));
        mp4d_memset(&p_tr->trex, 0, sizeof(p_tr->trex));
        p_tr->have_trex = 0;
        p_tr->num_saiz = 0;
        p_tr->num_saio = 0;

        CHECK( mp4d_parse_box(p_tr->atom, &nav) );

        debug(("stts at %p\n", p_tr->moov.stts.buffer.p_data));
        debug(("ctts at %p\n", p_tr->moov.ctts.buffer.p_data));
        debug(("stsz/stz2 at %p\n", p_tr->moov.stz.buffer.p_data));
        debug(("stsc at %p\n", p_tr->moov.stsc.buffer.p_data));
        debug(("stco/co64 at %p\n", p_tr->moov.co.chunk_offsets.p_data));
        debug(("stss at %p\n", p_tr->moov.stss.buffer.p_data));
        debug(("elst at %p\n", p_tr->elst.buffer.p_data));
        debug(("subs at %p\n", p_tr->subs.buffer.p_data));
        debug(("number of saiz: %d\n", p_tr->num_saiz));
        debug(("number of saio: %d\n", p_tr->num_saio));
        debug(("sdtp at %p\n", p_tr->moov.sdtp.buffer.p_data));
        debug(("stdp at %p\n", p_tr->moov.stdp.buffer.p_data));
        debug(("padb at %p\n", p_tr->moov.padb.buffer.p_data));

        ASSURE( p_tr->moov.stts.buffer.p_data != NULL, MP4D_E_INFO_NOT_AVAIL,
                    ("Missing mandatory box 'stts'") );
        /* CTS defaults to 0 if ctts not present */

        ASSURE( p_tr->moov.stz.buffer.p_data != NULL, MP4D_E_INFO_NOT_AVAIL,
                ("track_ID %" PRIu32 ": Missing stsz/stz2", p_tr->track_ID));

        ASSURE( p_tr->moov.stsc.buffer.p_data != NULL, MP4D_E_INFO_NOT_AVAIL,
                ("track_ID %" PRIu32 ": Missing stsc", p_tr->track_ID) );

        ASSURE( p_tr->moov.co.chunk_offsets.p_data != NULL, MP4D_E_INFO_NOT_AVAIL,
                ("track_ID %" PRIu32 ": Missing stco/co64", p_tr->track_ID) );

        if (p_tr->moov.stss.buffer.p_data == NULL)
        {
            CHECK( mp4d_stss_init(&p_tr->moov.stss, NULL) );
        }

        if (p_tr->elst.buffer.p_data == NULL)
        {
            CHECK( mp4d_elst_init(&p_tr->elst,
                                  NULL,
                                  p_tr->media_time_scale,
                                  p_tr->movie_time_scale) );
        }

        if (p_tr->subs.buffer.p_data == NULL)
        {
            CHECK( mp4d_subs_init(&p_tr->subs, NULL) );
        }

        /* sdtp may be non-present, gets data from sync table */
        /* stdp may be non-present */
        /* padb may be non-present */
    }
    else if (MP4D_FOURCC_EQ(p_tr->atom.type, "moof"))
    {
        /* reset once per moof, not per trun */
        mp4d_memset(&p_tr->subs, 0, sizeof(p_tr->subs));
        mp4d_memset(&p_tr->moov, 0, sizeof(p_tr->moov)); /* sdtp, stdp, padb */

        p_tr->num_saiz = 0;
        p_tr->num_saio = 0;        

        CHECK( get_next_trun(p_tr, 0) );

        if (p_tr->subs.buffer.p_data == NULL)
        {
            CHECK( mp4d_subs_init(&p_tr->subs, NULL) );
        }
        
        /* For the first trun, set the base_data_offset */
        if (p_tr->moof.tfhd.tf_flags & 0x000001)
        {
            /* Have explicit base_data_offset */
        }
        else
        {
            if (p_tr->moof.traf_number > 0)
            {
                /* Not the first traf in moof, default-base-is-moof must be set */
                ASSURE(p_tr->moof.tfhd.tf_flags & 0x020000,
                           MP4D_E_UNSUPPRTED_FORMAT,
                           ("track_ID %d: Sorry, base-data-offset-present is zero, and traf is not "
                            "the first in moof (but number %d) and default-base-is-moof is zero. Not supported",
                            p_tr->track_ID,
                            p_tr->moof.traf_number + 1) );
                /* Without default-base-is-moof, we would need to read all samples
                   from previous traf boxes, in order to get the base_data_offset for
                   this traf (which is the end of the previous traf's samples) */
            }
            
            /* For the first traf, or when default-base-is-moof is set,
               the base_data_offset is just the beginning of moof: */
            debug(("track_ID %" PRIu32 ": Set offset to atom_offset = %" PRIu64 "\n", 
                   p_tr->track_ID,
                   p_tr->atom_offset));
            p_tr->moof.tfhd.base_data_offset = p_tr->atom_offset;
        }

        p_tr->moof_iter.cur_data_offset = p_tr->moof.tfhd.base_data_offset;

        if (p_tr->moof.trun.tr_flags & 0x000001)
        {
            p_tr->moof_iter.cur_data_offset = p_tr->moof.tfhd.base_data_offset + p_tr->moof.trun.data_offset;
        }

        if (!p_tr->moof.trun.data_offset && !p_tr->moof.traf_number)
        {
            p_tr->moof_iter.cur_data_offset = p_tr->atom_offset + p_tr->atom.size + 16;
        }
        if (p_tr->moof.have_tfdt)
        {
            if (p_tr->abs_time_offset != p_tr->moof.tfdt_baseMediaDecodeTime)
            {
                /* The offset provided by the user may be the sidx box offset, which is in PTS.
                 * tfdt.baseMediaDecodeTime is in DTS, which we need. */
                debug(("Implied moof time offset %" PRIu64 " differs from tfdt.baseMediaDecodeTime = %" PRIu64 " (due to DTS/PTS?, using the latter)\n",
                        p_tr->abs_time_offset,
                        p_tr->moof.tfdt_baseMediaDecodeTime));
            }
            p_tr->abs_time_offset = p_tr->moof.tfdt_baseMediaDecodeTime;
            p_tr->cur_dts = p_tr->abs_time_offset;
        }

        CHECK( moof_set_aux_offset(p_tr) );
    }
    else
    {
        ASSURE( 0, MP4D_E_UNSUPPRTED_FORMAT,
                    ("Cannot initialize trackreader from %c%c%c%c box",
                     p_tr->atom.type[0], 
                     p_tr->atom.type[1], 
                     p_tr->atom.type[2], 
                     p_tr->atom.type[3]));
    }

    return MP4D_NO_ERROR;
}

int
mp4d_trackreader_seek_to 
(
    mp4d_trackreader_ptr_t p_tr,
    uint64_t time_stamp_in,
    uint64_t *p_time_stamp_out
)
{
    ASSURE( p_tr != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( p_time_stamp_out != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );

    /* Convert to media time scale */
    time_stamp_in *= p_tr->media_time_scale;
    time_stamp_in /= p_tr->movie_time_scale;

    ASSURE( time_stamp_in >= p_tr->abs_time_offset, MP4D_E_PREV_SEGMENT,
            ("track_ID %" PRIu32 ": %" PRIu64 " in previous fragment (current starts at %" PRIu64 ")",
             p_tr->track_ID, time_stamp_in, p_tr->abs_time_offset) );

    /* Find last sync sample with PTS <= required PTS

        Uses brute force algorithm for now:
            - 1st linear search from the beginning until first sample which is too late.
            - 2nd linear search from the beginning, two samples less (so that next call of next_sample() returns the sync sample)

        To improve on this:
            - See description in ISO spec descriptive index.
    */
    {
        uint64_t sample_index = 0;
        uint64_t seek_sample_index = 0;  /* Of seek point */
        mp4d_sampleref_t sample;

        /* Rewind to (before) first sample */
        CHECK( init_segment(p_tr) );
        
        do
        {
            uint8_t sample_depends_on;

            CHECK( mp4d_trackreader_next_sample(p_tr, &sample) );
            /* May return NEXT_SEGMENT */

            sample_depends_on = (sample.flags >> 24) & 0x3;

            sample_index++;
            
            if (sample.pts <= (int64_t) time_stamp_in && ((sample_depends_on == 2) || (sample.pic_type == 1) || (sample.pic_type == 2)))
            {
                seek_sample_index = sample_index;
                *p_time_stamp_out = (sample.pts + sample.presentation_offset);
                /* Convert to movie time scale */
                *p_time_stamp_out *= p_tr->movie_time_scale;
                *p_time_stamp_out /= p_tr->media_time_scale;
            }
        }
        while (sample.pts + sample.presentation_offset + sample.presentation_duration <= (int64_t) time_stamp_in);
        /* while sample end point is before requested time */

        /* before we make sure the seek_sample_index, we should init it first to clean up some value. */
        CHECK( init_segment(p_tr) ); 

        ASSURE( seek_sample_index > 0, MP4D_E_PREV_SEGMENT,
                ("No early enough sync sample was found for time %" PRIu64, time_stamp_in) );

        seek_sample_index -= 1;  /* Now in range 0, ... */

        /* Move to before seek sample (using brute force) */
        {
            uint64_t i;
            for (i = 0; i < seek_sample_index; i++)
            {
                CHECK( mp4d_trackreader_next_sample(p_tr, &sample) );
            }
        }
        /* Next call to next_sample will return the seek sample */
    }

    return 0;
}

int
mp4d_trackreader_init_segment
(
    mp4d_trackreader_ptr_t p_tr,
    mp4d_demuxer_ptr_t p_demuxer,
    uint32_t track_ID,
    uint32_t movie_time_scale,
    uint32_t media_time_scale,
    const uint64_t *abs_time_offset
    )
{
    uint32_t old_track_ID;

    ASSURE( p_demuxer != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( p_tr != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( track_ID > 0, MP4D_E_WRONG_ARGUMENT, ("Illegal track_ID = 0") );
    ASSURE( movie_time_scale > 0, MP4D_E_WRONG_ARGUMENT, ("Illegal movie time scale = 0"));
    ASSURE( media_time_scale > 0, MP4D_E_WRONG_ARGUMENT, ("Illegal media time scale = 0"));

    old_track_ID = p_tr->track_ID; /* avoid changing p_tr->track_ID if this init call fails,
                                      but we have to set it temporarily, it is used by callbacks */
    if (p_tr->track_ID == 0)
    {
        p_tr->track_ID = track_ID;
    }
    else if (p_tr->track_ID > 0)
    {
        ASSURE( track_ID == p_tr->track_ID, MP4D_E_WRONG_ARGUMENT,
                ("track_ID changed from %" PRIu32 " to %" PRIu32, p_tr->track_ID, track_ID) );
    }
    p_tr->movie_time_scale = movie_time_scale;
    p_tr->media_time_scale = media_time_scale;

    if (abs_time_offset == NULL && MP4D_FOURCC_EQ(p_demuxer->atom.type, "moof"))
    {
        /* This fragment directly follows the previous fragment */
        p_tr->abs_time_offset = p_tr->cur_dts;
    }
    else
    {
        if (abs_time_offset == NULL)
        {
            p_tr->abs_time_offset = 0;
        }
        else
        {
            /* Explicitly given */
            p_tr->abs_time_offset = *abs_time_offset;
        }
    }
    p_tr->atom = p_demuxer->atom;
    p_tr->atom_offset = p_demuxer->atom_offset;

    {
        mp4d_error_t err = init_segment(p_tr);

        if (err != MP4D_NO_ERROR)
        {
            p_tr->track_ID = old_track_ID;
            return err;
        }
        else
        {
            return MP4D_NO_ERROR;
        }
    }
}

int
mp4d_trackreader_set_type
(
        mp4d_trackreader_ptr_t p_tr,
        const mp4d_ftyp_info_t *p_type
)
{
    ASSURE( p_tr != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( p_type != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );

    p_tr->is_qt = MP4D_FOURCC_EQ(p_type->major_brand, "qt  ");

    return MP4D_NO_ERROR;
}

int
mp4d_trackreader_query_mem
(
    uint64_t *static_mem_size,   /**<  */
    uint64_t *dynamic_mem_size   /**<  */
)
{
    ASSURE( dynamic_mem_size != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( static_mem_size != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );

    *dynamic_mem_size = 0;
    *static_mem_size = sizeof(struct mp4d_trackreader_t_);

    return 0;
}

int
mp4d_trackreader_init
(
    mp4d_trackreader_ptr_t *p_tr,
    void *static_mem,
    void *dynamic_mem
)
{
    ASSURE( p_tr != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( static_mem != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    (void) dynamic_mem; /* unused */

    *p_tr = static_mem;
    mp4d_memset(*p_tr, 0, sizeof(**p_tr));

    return 0;
}

int
mp4d_trackreader_set_tenc(
    mp4d_trackreader_ptr_t p_tr,
    uint32_t default_algorithmID,
    uint8_t default_iv_size,
    uint8_t* default_kid)
{
    /* Copy this data into the piff senc reader data */
    p_tr->piff_senc_reader.default_algorithmID = default_algorithmID;
    p_tr->piff_senc_reader.default_iv_size = default_iv_size;
    memcpy(p_tr->piff_senc_reader.default_kid, default_kid, 16);

    return 0;
}

int
mp4d_trackreader_get_cur_tenc(
    mp4d_trackreader_ptr_t p_tr,
    uint32_t* algorithmID,
    uint8_t* iv_size,
    uint8_t* kid)
{
    /* copy this data back out */
    
    if (p_tr->piff_senc_reader.flags & 0x01)
    {
        *algorithmID = p_tr->piff_senc_reader.override_algorithmID;
        *iv_size = p_tr->piff_senc_reader.override_iv_size;
        memcpy(kid, p_tr->piff_senc_reader.override_kid, 16);
    }
    else
    {
        *algorithmID = p_tr->piff_senc_reader.default_algorithmID;
        *iv_size = p_tr->piff_senc_reader.default_iv_size;
        memcpy(kid, p_tr->piff_senc_reader.default_kid, 16);
    }

    return 0;
}
