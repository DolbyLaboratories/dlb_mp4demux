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

#include "mp4d_demux.h"
#include "mp4d_internal.h"

#include "mp4d_box_read.h"  /* used by track reader */

#include <assert.h>

#ifndef _MSC_VER
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif

typedef struct
{
    uint32_t track_id;        /* requested track_ID */
    uint64_t timestamp;       /* requested time stamp (media time scale) */
    uint64_t pos;             /* moof_offset */
    uint64_t pos_time;        /* corresponding to moof_offset (media time scale) */
} mfra_t;


static int
mp4d_parse_ftyp
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav
    )
{
    uint64_t n;
    mp4d_demuxer_ptr_t p_dmux = p_nav->p_data;
    mp4d_buffer_t p = mp4d_atom_to_buffer(&atom);    
    mp4d_read_fourcc(&p, p_dmux->curr.ftyp.info.major_brand);
    p_dmux->curr.ftyp.info.minor_version = mp4d_read_u32(&p);
    n = (atom.size - 8) / 4;
    ASSURE( (uint32_t) n == n, MP4D_E_UNSUPPRTED_FORMAT, ("ftyp/styp is too big (%" PRIu64 " entries)", n) );
    p_dmux->curr.ftyp.info.num_compat_brands = (uint32_t) n;
    p_dmux->curr.ftyp.info.compat_brands = p.p_data;
    return 0;
}


static int
mp4d_parse_pdin
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav
    )
{
    mp4d_demuxer_ptr_t p_dmux = p_nav->p_data;
    mp4d_buffer_t p = mp4d_atom_to_buffer(&atom);
    mp4d_pdin_t *p_pdin = &p_dmux->curr.pdin;
    mp4d_buffer_t startbuf;    
    uint8_t version;
    uint32_t flags;
    
    version = mp4d_read_u8(&p);
    flags = mp4d_read_u24(&p);
    
    p_pdin->lower.initial_delay = (uint32_t)-1; /* "infinite" delay at ... */
    p_pdin->lower.rate = 0;                     /* ... at zero rate */
    p_pdin->upper.initial_delay = 0;            /* zero delay at ... */
    p_pdin->upper.rate = (uint32_t)-1;          /* ... at "infinite" rate */

    if (version==0) {
        uint32_t rate,delay;
        int found_upper_rate = 0;
        int found_lower_rate = 0;
        uint32_t t;
        
        ASSURE( (uint32_t)(p.size / 8) == p.size / 8, MP4D_E_UNSUPPRTED_FORMAT, ("Too many (%" PRIu64 ") pdin entries", p.size / 8 ));
        p_pdin->num_pdin_infos = (uint32_t) (p.size / 8);

        startbuf = p;
        for (t=0; t<p_pdin->num_pdin_infos; t++) {
            rate = mp4d_read_u32(&p);
            delay = mp4d_read_u32(&p);
            if (rate <= p_pdin->req_rate) {
                if (rate > p_pdin->lower.rate) {
                    p_pdin->lower.rate = rate;
                    p_pdin->lower.initial_delay = delay;
                    found_lower_rate = 1;
                }
            }
            else {
                if (rate < p_pdin->upper.rate) {
                    p_pdin->upper.rate = rate;
                    p_pdin->upper.initial_delay = delay;
                    found_upper_rate = 1;
                }
            }
        }

        /* there was no upper entry for interpolation (rate > req_rate),
         search for a 2nd lower entry for extrapolation */
        if (t>0 && found_upper_rate==0) {
            p_pdin->upper = p_pdin->lower;
            p_pdin->lower.initial_delay = (uint32_t)-1; /* "infinite" delay at ... */
            p_pdin->lower.rate = 0;                     /* ... at zero rate */
            
            p = startbuf;
            for (t=0; t<p_pdin->num_pdin_infos; t++) {
                rate = mp4d_read_u32(&p);
                delay = mp4d_read_u32(&p);
                if (rate < p_pdin->upper.rate) {
                    if (rate > p_pdin->lower.rate) {
                        p_pdin->lower.rate = rate;
                        p_pdin->lower.initial_delay = delay;
                    }
                }
            }
        }
        
        /* there was no lower entry for interpolation (rate < req_rate),
         search for a 2nd upper entry for extrapolation */
        if (t>0 && found_lower_rate==0) {
            p_pdin->lower = p_pdin->upper;
            p_pdin->upper.initial_delay = 0;   /* zero delay at ... */
            p_pdin->upper.rate = (uint32_t)-1; /* ... at "infinite" rate */
            
            p = startbuf;
            for (t=0; t<p_pdin->num_pdin_infos; t++) {
                rate = mp4d_read_u32(&p);
                delay = mp4d_read_u32(&p);
                if (rate > p_pdin->lower.rate) {
                    if (rate < p_pdin->upper.rate) {
                        p_pdin->upper.rate = rate;
                        p_pdin->upper.initial_delay = delay;
                    }
                }
            }
        }
    }
    else {
        return MP4D_E_UNSUPPRTED_FORMAT;
    }
    return 0;
}


static int
mp4d_parse_bloc
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav
    )
{
    mp4d_demuxer_ptr_t p_dmux = p_nav->p_data;
    mp4d_buffer_t p = mp4d_atom_to_buffer(&atom);    
    uint8_t version;
    uint32_t flags;
    
    version = mp4d_read_u8(&p);
    flags = mp4d_read_u24(&p);
    
    p_dmux->curr.bloc.info.base_location = NULL;
    p_dmux->curr.bloc.info.base_location_size = 0;
    p_dmux->curr.bloc.info.purchase_location = NULL;
    p_dmux->curr.bloc.info.purchase_location_size = 0;
    p_dmux->curr.bloc.info.reserved = NULL;
    p_dmux->curr.bloc.info.reserved_size = 0;    
    
    if (version==0) {
        
        if (!mp4d_is_buffer_error(&p)) {
            const uint32_t entry_size = 256;
            p_dmux->curr.bloc.info.base_location = p.p_data;
            p_dmux->curr.bloc.info.base_location_size = (p.size < entry_size) ? (uint32_t) p.size : entry_size;
            mp4d_skip_bytes(&p, entry_size);
        }
        
        if (!mp4d_is_buffer_error(&p)) {
            const uint32_t entry_size = 256;
            p_dmux->curr.bloc.info.purchase_location = p.p_data;
            p_dmux->curr.bloc.info.purchase_location_size = (p.size < entry_size) ? (uint32_t) p.size : entry_size;
            mp4d_skip_bytes(&p, entry_size);
        }
        
        if (!mp4d_is_buffer_error(&p)) {
            const uint32_t entry_size = 512;
            p_dmux->curr.bloc.info.reserved = p.p_data;
            p_dmux->curr.bloc.info.reserved_size = (p.size < entry_size) ? (uint32_t) p.size : entry_size;
            mp4d_skip_bytes(&p, entry_size);
        }
    }
    else {
        return MP4D_E_UNSUPPRTED_FORMAT;
    }
    return 0;
}



static int
mp4d_parse_moov
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav
    )
{
    mp4d_demuxer_ptr_t p_dmux = (mp4d_demuxer_ptr_t)(p_nav->p_data);
    mp4d_buffer_t p = mp4d_atom_to_buffer(&atom);
    mp4d_atom_t child;
    mp4d_error_t err = MP4D_NO_ERROR;
    p_dmux->curr.moov.info.movie_dur = 0;

    while (mp4d_bytes_left(&p)) 
    {
        err = (mp4d_error_t)mp4d_next_atom(&p, &atom, &child);
        if (err) return err;
        err = (mp4d_error_t)mp4d_dispatch(child, p_nav);

        if (MP4D_FOURCC_EQ(child.type, "meta")) {
            p_dmux->curr.moov.meta = p_dmux->meta;
        }
    }

    p_dmux->curr.moov.info.num_streams = p_dmux->track_cnt;

    return (int) err;
}

static int
mp4d_parse_mvhd
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav
    )
{
    mp4d_demuxer_ptr_t p_dmux = p_nav->p_data;
    mp4d_buffer_t p = mp4d_atom_to_buffer(&atom);
    uint8_t version;
    uint32_t flags;

    version = mp4d_read_u8(&p);
    flags = mp4d_read_u24(&p);

    if (version==1) {
        mp4d_read_u64(&p); /*creation time*/
        mp4d_read_u64(&p); /*modification time*/
        p_dmux->curr.moov.info.time_scale = mp4d_read_u32(&p);
        p_dmux->curr.moov.info.movie_dur = mp4d_read_u64(&p);
    }
    else if (version==0) {
        mp4d_read_u32(&p); /*creation time*/
        mp4d_read_u32(&p); /*modification time*/
        p_dmux->curr.moov.info.time_scale = mp4d_read_u32(&p);
        p_dmux->curr.moov.info.movie_dur = mp4d_read_u32(&p);
    }
    else {
        return MP4D_E_UNSUPPRTED_FORMAT;
    }
    p_dmux->movie_timescale = p_dmux->curr.moov.info.time_scale;

    return 0;
}

static int
mp4d_parse_mehd
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav
    )
{
    mp4d_demuxer_ptr_t p_dmux = p_nav->p_data;
    mp4d_buffer_t p = mp4d_atom_to_buffer(&atom);
    uint8_t version;
    uint32_t flags;

    version = mp4d_read_u8(&p);
    flags = mp4d_read_u24(&p);

    if (version==0) {
        p_dmux->curr.moov.info.movie_dur = mp4d_read_u32(&p);
    }
    else if (version==1) {
        p_dmux->curr.moov.info.movie_dur = mp4d_read_u64(&p);
    }
    else {
        return MP4D_E_UNSUPPRTED_FORMAT;
    }

    return 0;
}

int 
mp4d_parse_tref
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav
    )
{
    mp4d_demuxer_ptr_t p_dmux = p_nav->p_data;
    mp4d_buffer_t p = mp4d_atom_to_buffer(&atom);
    uint8_t version;
    uint32_t flags;
    
    version = mp4d_read_u8(&p);
    flags = mp4d_read_u24(&p);

    if (MP4D_FOURCC_EQ(p.p_data, "vdep"))
    {
        p_dmux->curr.moov.p_trak->sampleentry.vide.tref_vide_flag = 1;
    }
    return 0;
}

int
mp4d_parse_tkhd
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav
    )
{
    mp4d_demuxer_ptr_t p_dmux = p_nav->p_data;
    mp4d_buffer_t p = mp4d_atom_to_buffer(&atom);
    uint8_t version;
    uint32_t flags;
    uint32_t track_id, tkhd_width, tkhd_height;

    version = mp4d_read_u8(&p);
    flags = mp4d_read_u24(&p);

    if (version==1) {
        mp4d_read_u64(&p); /*creation time*/
        mp4d_read_u64(&p); /*modification time*/
        track_id = mp4d_read_u32(&p);
        mp4d_read_u32(&p); /*reserved*/
        mp4d_read_u64(&p); /*duration*/
    }
    else if (version==0) {
        mp4d_read_u32(&p); /*creation time*/
        mp4d_read_u32(&p); /*modification time*/
        track_id = mp4d_read_u32(&p);
        mp4d_read_u32(&p); /*reserved*/
        mp4d_read_u32(&p); /*duration*/
    }
    else {
        return MP4D_E_UNSUPPRTED_FORMAT;
    }
    mp4d_skip_bytes(&p, 52); /* 2*4(res) + 2(lay) + 2(agrp) + 2(vol) + 2(res) + 9*4(mtx) */
    tkhd_width = mp4d_read_u32(&p);
    tkhd_height = mp4d_read_u32(&p);

    if (p_dmux->curr.moov.p_trak) {
        p_dmux->curr.moov.p_trak->info.track_id = track_id;
        p_dmux->curr.moov.p_trak->info.flags = flags;
        p_dmux->curr.moov.p_trak->info.tkhd_width = tkhd_width;
        p_dmux->curr.moov.p_trak->info.tkhd_height = tkhd_height;
    }

    ASSURE( track_id > 0, MP4D_E_INVALID_ATOM, ("tkhd:track_ID is zero") );

    return 0;
}

static int
mp4d_parse_trak
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav
    )
{
    mp4d_demuxer_ptr_t p_dmux = p_nav->p_data;
    mp4d_error_t err = 0;

    p_dmux->track_cnt++;
    if (p_dmux->curr.moov.p_trak) {
        const mp4d_callback_t *dispatcher = p_nav->atom_hdlr_list;

        p_nav->atom_hdlr_list = p_dmux->p_trak_dispatcher;
        err =  mp4d_parse_box(atom, p_nav);
        p_nav->atom_hdlr_list = dispatcher;

        MP4D_FOURCC_ASSIGN( p_dmux->curr.moov.p_trak->info.hdlr, p_dmux->hdlr.handler_type );
    }

    return err;
}

static int
mp4d_parse_vmhd
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav
    )
{
    mp4d_demuxer_ptr_t p_dmux = p_nav->p_data;
    
	if (p_dmux->curr.moov.p_trak) 
	{
            p_dmux->curr.moov.p_trak->info.vmhd_flag = 1;
    }

    return 0;
}


static int
mp4d_parse_hdlr
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav
    )
{
    mp4d_demuxer_ptr_t p_dmux = p_nav->p_data;
    mp4d_buffer_t p = mp4d_atom_to_buffer(&atom);
    uint8_t version;
    uint32_t flags;

    version = mp4d_read_u8(&p);
    flags = mp4d_read_u24(&p);

    if (MP4D_FOURCC_EQ(atom.p_parent->type, "minf"))
    {
        /* Already got the handler type from mdia:hdlr,
           do not overwrite with minf:hdlr (relevant for QuickTime) */
        return 0;
    }

    if (version==0) {
        mp4d_read_u32(&p);
        mp4d_read_fourcc(&p, p_dmux->hdlr.handler_type);
        mp4d_skip_bytes(&p, 12);
        p_dmux->hdlr.p_string = p.p_data;
    }
    else {
        return MP4D_E_UNSUPPRTED_FORMAT;
    }
    return 0;
}

static int
mp4d_find_hdlr
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav
    )
{
    mp4d_atom_t hdlr;
    mp4d_error_t err = MP4D_NO_ERROR;

    err = mp4d_find_atom(&atom, "hdlr", 0, &hdlr);
    if (err) {
        debug(("No 'hdlr' in '%c%c%c%c'\n", atom.type[0],atom.type[1],atom.type[2],atom.type[3]));
        return err;
    }

    err = mp4d_parse_hdlr(hdlr, p_nav);
    if (err) {
        debug(("'hdlr' in '%c%c%c%c' parsing error\n", atom.type[0],atom.type[1],atom.type[2],atom.type[3]));
        return err;
    }

    return 0;
}

static int
mp4d_parse_mdia
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav
    )
{
    mp4d_demuxer_ptr_t p_dmux = p_nav->p_data;
    mp4d_error_t err = MP4D_NO_ERROR;

    if (p_dmux->curr.moov.p_trak) {
        MP4D_FOURCC_ASSIGN(p_dmux->curr.moov.p_trak->info.hdlr, "\0\0\0\0");
        err = mp4d_find_hdlr(atom, p_nav);
        if (err) return err;
        MP4D_FOURCC_ASSIGN(p_dmux->curr.moov.p_trak->info.hdlr, p_dmux->hdlr.handler_type);
    }

    return mp4d_parse_box(atom, p_nav);
}

static int
mp4d_parse_mdhd
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav
    )
{
    mp4d_demuxer_ptr_t p_dmux = p_nav->p_data;
    mp4d_buffer_t p = mp4d_atom_to_buffer(&atom);
    uint8_t version;
    uint32_t flags;
    uint32_t time_scale;
    uint64_t media_dur;
    uint16_t media_lang;

    version = mp4d_read_u8(&p);
    flags = mp4d_read_u24(&p);

    if (version==1) {
        mp4d_read_u64(&p); /*creation time*/
        mp4d_read_u64(&p); /*modification time*/
        time_scale = mp4d_read_u32(&p);
        media_dur = mp4d_read_u64(&p);
    }
    else if (version==0) {
        mp4d_read_u32(&p); /*creation time*/
        mp4d_read_u32(&p); /*modification time*/
        time_scale = mp4d_read_u32(&p);
        media_dur = mp4d_read_u32(&p);
    }
    else {
        return MP4D_E_UNSUPPRTED_FORMAT;
    }
    media_lang = mp4d_read_u16(&p);
    mp4d_read_u16(&p);

    if (p_dmux->curr.moov.p_trak) {
        p_dmux->curr.moov.p_trak->info.media_lang = media_lang;
        p_dmux->curr.moov.p_trak->info.time_scale = time_scale;
        p_dmux->curr.moov.p_trak->info.media_dur = media_dur;
    }

    return 0;
}

static int
mp4d_parse_schm
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav
    )
{
    mp4d_demuxer_ptr_t p_dmux = p_nav->p_data;
    mp4d_buffer_t p = mp4d_atom_to_buffer(&atom);
    mp4d_fourcc_t scheme_type;
    uint32_t scheme_version;
    uint8_t version;
    uint32_t flags;
    
    version = mp4d_read_u8(&p);
    flags = mp4d_read_u24(&p);
    
    if (version==0) {
        mp4d_read_fourcc(&p, scheme_type);
        scheme_version = mp4d_read_u32(&p);
    }
    else {
        return MP4D_E_UNSUPPRTED_FORMAT;
    }
    
    if (p_dmux->curr.moov.p_trak) {
        MP4D_FOURCC_ASSIGN(p_dmux->curr.moov.p_trak->crypt.scheme_type, scheme_type);
        p_dmux->curr.moov.p_trak->crypt.scheme_version = scheme_version;
    }
    
    return 0;
}


static int
mp4d_parse_encryption_entry
    (mp4d_buffer_t * p
    ,mp4d_navigator_ptr_t p_nav
    )
{
    mp4d_demuxer_ptr_t p_dmux = p_nav->p_data;
    if (p_dmux->curr.moov.p_trak) {
        p_dmux->curr.moov.p_trak->crypt.info.method = mp4d_read_u24(p);
        p_dmux->curr.moov.p_trak->crypt.info.iv_size = mp4d_read_u8(p);
        mp4d_read(p, p_dmux->curr.moov.p_trak->crypt.info.key_id, 16);
    }
    
    return mp4d_is_buffer_error(p)?MP4D_E_INVALID_ATOM:MP4D_NO_ERROR;
}

static int
mp4d_parse_tenc
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav
    )
{
    mp4d_buffer_t p = mp4d_atom_to_buffer(&atom);
    uint8_t version;
    uint32_t flags;
    
    version = mp4d_read_u8(&p);
    flags = mp4d_read_u24(&p);
    
    if (version==0) {
        return mp4d_parse_encryption_entry(&p, p_nav);
    }
    else {
        return MP4D_E_UNSUPPRTED_FORMAT;
    }
}

static int
mp4d_parse_frma
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav
    )
{
    mp4d_demuxer_ptr_t p_dmux = p_nav->p_data;
    mp4d_buffer_t p = mp4d_atom_to_buffer(&atom);
    mp4d_fourcc_t data_format;

    mp4d_read_fourcc(&p, data_format);

    if (p_dmux->curr.moov.p_trak) {
        MP4D_FOURCC_ASSIGN(p_dmux->curr.moov.p_trak->info.codec, data_format);
    }

    return 0;
}


static int
mp4d_read_crypt_data
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav
    ,mp4d_crypt_info_t * p_crypt_info
    )
{
    mp4d_demuxer_ptr_t p_dmux = p_nav->p_data;
    mp4d_trak_t * p_trak = p_dmux->curr.moov.p_trak;
    mp4d_memset(&p_trak->crypt.info, 0, sizeof(mp4d_crypt_info_t));
    mp4d_parse_box(atom, p_nav);
    p_crypt_info->method = 0xff;
    if ((MP4D_FOURCC_EQ(p_trak->crypt.scheme_type, "cenc") && p_trak->crypt.scheme_version==0x00010000ul) ||
        (MP4D_FOURCC_EQ(p_trak->crypt.scheme_type, "piff") && p_trak->crypt.scheme_version==0x00010000ul) ||
        (MP4D_FOURCC_EQ(p_trak->crypt.scheme_type, "piff") && p_trak->crypt.scheme_version==0x00010001ul))
    {
        *p_crypt_info = p_trak->crypt.info;
    }
    
    return 0;
}

static int
mp4d_parse_visual
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav
    )
{
    mp4d_demuxer_ptr_t p_dmux = p_nav->p_data;
    mp4d_buffer_t p = mp4d_atom_to_buffer(&atom);
    uint16_t data_reference_index;
    uint16_t width, height, depth;
    uint8_t compressorname_size;
    const unsigned char *p_compressorname;
    int no_more_children = 0;
    mp4d_error_t err;

    mp4d_skip_bytes(&p, 6); /* reserved */
    data_reference_index = mp4d_read_u16(&p);

    /* Visual Sample Entry */
    mp4d_skip_bytes(&p, 2+2+3*4); /* reserved */
    width = mp4d_read_u16(&p);
    height = mp4d_read_u16(&p);
    mp4d_skip_bytes(&p, 2*4+4+2); /* unused + reserved */
    compressorname_size = mp4d_read_u8(&p);
    p_compressorname = p.p_data;
    mp4d_skip_bytes(&p, 31); /* compressorname */
    depth = mp4d_read_u16(&p);
    mp4d_skip_bytes(&p, 2); /* pre-defined */

    if (p_dmux->curr.moov.p_trak)
    {
        p_dmux->curr.moov.p_trak->sampleentry.vide.data_reference_index = data_reference_index;
        p_dmux->curr.moov.p_trak->sampleentry.vide.width = width;
        p_dmux->curr.moov.p_trak->sampleentry.vide.height = height;
        p_dmux->curr.moov.p_trak->sampleentry.vide.depth = depth;
        p_dmux->curr.moov.p_trak->sampleentry.vide.dsi_size = 0;   /* If no DSI is found */
        p_dmux->curr.moov.p_trak->sampleentry.vide.dsi = p.p_data; /* will not be read (because dsi_size = 0) but signals that a sample entry was found */
        MP4D_FOURCC_ASSIGN(p_dmux->curr.moov.p_trak->sampleentry.vide.dsi_type, "\0\0\0\0");

        p_dmux->curr.moov.p_trak->sampleentry.vide.avcC_flag = 0;
        p_dmux->curr.moov.p_trak->sampleentry.vide.hvcC_flag = 0;
        p_dmux->curr.moov.p_trak->sampleentry.vide.dvcC_flag = 0;
        p_dmux->curr.moov.p_trak->sampleentry.vide.avcE_flag = 0;
        p_dmux->curr.moov.p_trak->sampleentry.vide.hvcE_flag = 0;
        p_dmux->curr.moov.p_trak->sampleentry.vide.dv_dsi = NULL;
        p_dmux->curr.moov.p_trak->sampleentry.vide.dv_dsi_size = 0;
        p_dmux->curr.moov.p_trak->sampleentry.vide.dv_el_dsi = NULL;
        p_dmux->curr.moov.p_trak->sampleentry.vide.dv_el_dsi_size = 0;

        if (compressorname_size > 31)
        {
            debug(("Invalid length of compressorname: %u (max: 32)\n", compressorname_size));
            compressorname_size=31;
        }
        mp4d_memcpy(p_dmux->curr.moov.p_trak->sampleentry.vide.compressorname, p_compressorname, compressorname_size);
        p_dmux->curr.moov.p_trak->sampleentry.vide.compressorname[compressorname_size] = '\0';
        p_dmux->curr.moov.p_trak->sampleentry.vide.par_present = 0;

        /* This is the beginning of child data. Record it's location */
        p_dmux->curr.moov.p_trak->sampleentry.vide.child_data = p.p_data;
        p_dmux->curr.moov.p_trak->sampleentry.vide.child_data_size = p.size;

        do {
            mp4d_atom_t child;

            err = mp4d_parse_atom_header(p.p_data, p.size, &child);
            if (err) break;

            if (MP4D_FOURCC_EQ(child.type, "sinf")) {
                mp4d_read_crypt_data(child, p_nav, &p_dmux->curr.moov.p_trak->sampleentry.vide.crypt_info);
                MP4D_FOURCC_ASSIGN(p_dmux->curr.moov.p_trak->sampleentry.vide.dsi_type_cry, p_dmux->curr.moov.p_trak->info.codec);
            } 
            else if (MP4D_FOURCC_EQ(child.type, "clap")) {
                debug(("Ignoring 'clap'\n"));
            } 
            else if (MP4D_FOURCC_EQ(child.type, "pasp")) {
                mp4d_buffer_t p_buf = mp4d_atom_to_buffer(&child);
                p_dmux->curr.moov.p_trak->sampleentry.vide.par_present = 1;
                p_dmux->curr.moov.p_trak->sampleentry.vide.par_hspacing = mp4d_read_u32(&p_buf);
                p_dmux->curr.moov.p_trak->sampleentry.vide.par_vspacing = mp4d_read_u32(&p_buf);
            } 
            else if (MP4D_FOURCC_EQ(child.type, "dvcC")) {
                /* dolby vision stream */
                p_dmux->curr.moov.p_trak->sampleentry.vide.dv_dsi_size = child.size;
                p_dmux->curr.moov.p_trak->sampleentry.vide.dv_dsi = child.p_data;
                p_dmux->curr.moov.p_trak->sampleentry.vide.dvcC_flag = 1;
                MP4D_FOURCC_ASSIGN(p_dmux->curr.moov.p_trak->sampleentry.vide.sampleentry_name, atom.type);
            } 
            else if (MP4D_FOURCC_EQ(child.type, "avcE") || MP4D_FOURCC_EQ(child.type, "hvcE")) {
                /* dolby vision stream */
                if (MP4D_FOURCC_EQ(child.type, "avcE") )
                {
                    p_dmux->curr.moov.p_trak->sampleentry.vide.avcE_flag = 1;
                }
                else
                {
                    p_dmux->curr.moov.p_trak->sampleentry.vide.hvcE_flag = 1;
                }
                p_dmux->curr.moov.p_trak->sampleentry.vide.dv_el_dsi_size = child.size;
                p_dmux->curr.moov.p_trak->sampleentry.vide.dv_el_dsi = child.p_data;
            } 
            else if (no_more_children==0) {
                MP4D_FOURCC_ASSIGN(p_dmux->curr.moov.p_trak->sampleentry.vide.dsi_type, child.type);
                p_dmux->curr.moov.p_trak->sampleentry.vide.dsi_size = child.size;
                p_dmux->curr.moov.p_trak->sampleentry.vide.dsi = child.p_data;
                no_more_children = 1;
                if (MP4D_FOURCC_EQ(child.type, "avcC")) {
                p_dmux->curr.moov.p_trak->sampleentry.vide.avcC_flag = 1;
                } else if (MP4D_FOURCC_EQ(child.type, "hvcC")) {
                    p_dmux->curr.moov.p_trak->sampleentry.vide.hvcC_flag = 1;
                } 
            }
            mp4d_skip_bytes(&p, child.header + child.size);

        } while (p.size && !mp4d_is_buffer_error(&p));

    }
    return 0;  /* Always return success seems wrong (but ignored by the navigator anyway) */
}

static int
mp4d_parse_audio
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav
    )
{
    mp4d_demuxer_ptr_t p_dmux = p_nav->p_data;
    mp4d_buffer_t p = mp4d_atom_to_buffer(&atom);
    uint16_t data_reference_index;
    uint16_t channelcount, samplesize, reserved16;
    uint32_t samplerate, reserved32, qtflags;
    int more_children = 1;
    mp4d_error_t err;
    uint16_t sound_version;

    mp4d_skip_bytes(&p, 6); /* reserved */
    data_reference_index = mp4d_read_u16(&p);

    /* Audio Sample Entry */
    sound_version = mp4d_read_u16(&p);
    reserved16 = mp4d_read_u16(&p); /* revision level */
    reserved32 = mp4d_read_u32(&p); /* vendor */
    channelcount = mp4d_read_u16(&p);
    samplesize = mp4d_read_u16(&p);
    reserved16 = mp4d_read_u16(&p); /* compression ID */
    reserved16 = mp4d_read_u16(&p); /* packet size */
    samplerate = mp4d_read_u32(&p);
    samplerate >>= 16;
    qtflags = 0;

    if (sound_version == 1)
    {
        int spp, bpp, bpf, bps;
        /* QuickTime */
        spp = mp4d_read_u32(&p); /* samples per packet */
        bpp = mp4d_read_u32(&p); /* bytes per packet */
        bpf = mp4d_read_u32(&p); /* bytes per frame */
        bps = mp4d_read_u32(&p); /* bytes per sample */
        samplesize = bpf / channelcount * 8;
        qtflags |= 0x02; /* we set default as big endian */
    }
    else if (sound_version == 2)
    {
        union { uint64_t i; double f; } u;

        /* QuickTime */
        mp4d_skip_bytes(&p, 4); /* sizeOfStructOnly */
        u.i = mp4d_read_u64(&p); /* audioSampleRate */
        samplerate = (uint32_t)u.f;
        channelcount = mp4d_read_u32(&p); /* numAudioChannels */
        mp4d_skip_bytes(&p, 4); /* always7F000000 */
        samplesize = mp4d_read_u32(&p); /* constBitsPerChannel */ 
        qtflags = mp4d_read_u32(&p); /* formatSpecificFlags */
        mp4d_skip_bytes(&p, 4); /* constBytesPerAudioPacket */
        mp4d_skip_bytes(&p, 4); /* constLPCMFramesPerAudioPacket */
    }

    if (p_dmux->curr.moov.p_trak)
    {
        p_dmux->curr.moov.p_trak->sampleentry.soun.data_reference_index = data_reference_index;
        p_dmux->curr.moov.p_trak->sampleentry.soun.channelcount = channelcount;
        p_dmux->curr.moov.p_trak->sampleentry.soun.samplerate = samplerate;
        p_dmux->curr.moov.p_trak->sampleentry.soun.dsi_size = 0;   /* If no DSI is found */
        p_dmux->curr.moov.p_trak->sampleentry.soun.dsi = p.p_data; /* will not be read (because dsi_size = 0)
                                                                      but signals that a sample entry was found.
                                                                      Any non-NULL pointer works. */
        MP4D_FOURCC_ASSIGN(p_dmux->curr.moov.p_trak->sampleentry.soun.dsi_type, "\0\0\0\0");
        p_dmux->curr.moov.p_trak->sampleentry.soun.sound_version = sound_version;
        p_dmux->curr.moov.p_trak->sampleentry.soun.qtflags = qtflags;
        p_dmux->curr.moov.p_trak->sampleentry.soun.samplesize = samplesize;

        /* This is the beginning of child data. Record it's location */
        p_dmux->curr.moov.p_trak->sampleentry.soun.child_data = p.p_data;
        p_dmux->curr.moov.p_trak->sampleentry.soun.child_data_size = p.size;

        do {
            mp4d_atom_t child;

            err = mp4d_parse_atom_header(p.p_data, p.size, &child);
            if (err) break;

            if (MP4D_FOURCC_EQ(child.type, "wave")) 
            {
                /* QuickTime */
                mp4d_atom_t esds;
                mp4d_atom_t enda;
                
                /* Endiannes of sound component */
                err = mp4d_find_atom(&child, "enda", 0, &enda);
                if (!err)
                {
                    if (enda.size == 1 && enda.p_data[0])
                        p_dmux->curr.moov.p_trak->sampleentry.soun.qtflags &= 0xFFFD;
                    else if (enda.size == 2 && enda.p_data[1])
                        p_dmux->curr.moov.p_trak->sampleentry.soun.qtflags &= 0xFFFD;
                }

                err = mp4d_find_atom(&child, "esds", 0, &esds);
                if (err) break;

                MP4D_FOURCC_ASSIGN(p_dmux->curr.moov.p_trak->sampleentry.soun.dsi_type, "esds");
                p_dmux->curr.moov.p_trak->sampleentry.soun.dsi_size = esds.size;
                p_dmux->curr.moov.p_trak->sampleentry.soun.dsi = esds.p_data;
                more_children = 0;
            }
            else if (MP4D_FOURCC_EQ(child.type, "sinf")) 
            {
                mp4d_read_crypt_data(child, p_nav, &p_dmux->curr.moov.p_trak->sampleentry.soun.crypt_info);
                MP4D_FOURCC_ASSIGN(p_dmux->curr.moov.p_trak->sampleentry.vide.dsi_type_cry, p_dmux->curr.moov.p_trak->info.codec);
            } 
            else if (more_children) 
            {
                MP4D_FOURCC_ASSIGN(p_dmux->curr.moov.p_trak->sampleentry.soun.dsi_type, child.type);
                p_dmux->curr.moov.p_trak->sampleentry.soun.dsi_size = child.size;
                p_dmux->curr.moov.p_trak->sampleentry.soun.dsi = child.p_data;
                more_children = 0;
            }
            mp4d_skip_bytes(&p, child.header + child.size);

        } while (p.size && !mp4d_is_buffer_error(&p));
    }

    return 0;
}

static int
mp4d_parse_subtitle
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav
    )
{
    mp4d_demuxer_ptr_t p_dmux = p_nav->p_data;
    mp4d_buffer_t p = mp4d_atom_to_buffer(&atom);
    uint16_t data_reference_index;
    char temp;

    mp4d_skip_bytes(&p, 6); /* reserved */
    data_reference_index = mp4d_read_u16(&p);
    /* Subtitle Sample Entry */
    
    if (p_dmux->curr.moov.p_trak) {
        p_dmux->curr.moov.p_trak->sampleentry.subt.data_reference_index = data_reference_index;
    }

    p_dmux->curr.moov.p_trak->sampleentry.subt.subt_namespace = p.p_data;
    do
    {
        temp = mp4d_read_u8(&p);
    }while(temp!='\0');

    p_dmux->curr.moov.p_trak->sampleentry.subt.schema_location = p.p_data;
    do
    {
        temp = mp4d_read_u8(&p);
    }while(temp!='\0');

    p_dmux->curr.moov.p_trak->sampleentry.subt.image_mime_type = p.p_data;
    do
    {
        temp = mp4d_read_u8(&p);
    }while(temp!='\0');

    return 0;
}

static int
mp4d_parse_xmlmeta
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav
    )
{
    mp4d_demuxer_ptr_t p_dmux = p_nav->p_data;
    mp4d_buffer_t p = mp4d_atom_to_buffer(&atom);
    uint16_t data_reference_index;
    
    mp4d_skip_bytes(&p, 6); /* reserved */
    data_reference_index = mp4d_read_u16(&p);
    
    /* Metadata Sample Entry */

    if (p_dmux->curr.moov.p_trak) {
        p_dmux->curr.moov.p_trak->sampleentry.meta.data_reference_index = data_reference_index;
    }
    
    return 0;
}


static int
mp4d_parse_stsd
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav
    )
{
    mp4d_demuxer_ptr_t p_dmux = p_nav->p_data;
    mp4d_buffer_t p = mp4d_atom_to_buffer(&atom);
    uint32_t entry_count;
    mp4d_error_t err = 0;
    uint8_t version;
    uint32_t flags;
    uint32_t n;

    version = mp4d_read_u8(&p);
    flags = mp4d_read_u24(&p);

    if (version==0) {
        entry_count = mp4d_read_u32(&p);
        if (p.size==(uint64_t)-1)
            return MP4D_E_INVALID_ATOM;
        atom.p_data += 8;
        atom.size -= 8;
    }
    else {
        return MP4D_E_UNSUPPRTED_FORMAT;
    }

    if (p_dmux->curr.moov.p_trak) {
        p_dmux->curr.moov.p_trak->info.num_dsi = entry_count;
        MP4D_FOURCC_ASSIGN(p_dmux->curr.moov.p_trak->info.codec, "\0\0\0\0");

        for (n = 0; n < entry_count; n++) {
            mp4d_atom_t sampleentry;
            if (p.size && !mp4d_is_buffer_error(&p)) {
                err = mp4d_parse_atom_header(p.p_data, p.size, &sampleentry);
                if (err) break;

                MP4D_FOURCC_ASSIGN(p_dmux->curr.moov.p_trak->info.codec, sampleentry.type);
                if (p_dmux->curr.moov.p_trak->sampleentry_req_idx == 0 ||
                    p_dmux->curr.moov.p_trak->sampleentry_req_idx == n + 1) {
                    if (MP4D_FOURCC_EQ(p_dmux->curr.moov.p_trak->info.hdlr, "vide")) {
                        mp4d_parse_visual(sampleentry, p_nav);
                    }
                    else if (MP4D_FOURCC_EQ(p_dmux->curr.moov.p_trak->info.hdlr, "soun")) {
                        mp4d_parse_audio(sampleentry, p_nav);
                    }
                    else if (MP4D_FOURCC_EQ(p_dmux->curr.moov.p_trak->info.hdlr, "subt")) {
                        mp4d_parse_subtitle(sampleentry, p_nav);
                    }
                    else if (MP4D_FOURCC_EQ(p_dmux->curr.moov.p_trak->info.hdlr, "meta")) {
                        mp4d_parse_xmlmeta(sampleentry, p_nav);
                    }
                }
                mp4d_skip_bytes(&p, sampleentry.header+sampleentry.size);
            }

        }
    }
    if (err) return err;

    return 0;
}

static int
mp4d_parse_mfhd
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav
    )
{
    mp4d_buffer_t p = mp4d_atom_to_buffer(&atom);
    uint32_t sequence_number;
    uint8_t version;
    uint32_t flags;

    (void) p_nav; /* unused */
    version = mp4d_read_u8(&p);
    flags = mp4d_read_u24(&p);

    if (version==0) {
        sequence_number = mp4d_read_u32(&p);
    }
    else {
        return MP4D_E_UNSUPPRTED_FORMAT;
    }

    return 0;
}

static int
mp4d_parse_tfra
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav
    )
{
    mfra_t *p_mfra = p_nav->p_data;
    mp4d_buffer_t p = mp4d_atom_to_buffer(&atom);
    uint32_t track_id;
    uint8_t version;
    uint32_t flags;

    version = mp4d_read_u8(&p);
    flags = mp4d_read_u24(&p);
    
    if (version!=1 && version!=0) {
        return MP4D_E_UNSUPPRTED_FORMAT;
    }
    
    track_id = mp4d_read_u32(&p);
    
    if (track_id == p_mfra->track_id) {
        uint32_t sizes;
        uint32_t num_entries;
        uint32_t t;
        
        sizes = mp4d_read_u32(&p);
        num_entries = mp4d_read_u32(&p);
        
        for (t=0; t<num_entries; t++) 
        {
            uint64_t time;
            uint64_t offs;

            time = (version==1) ? mp4d_read_u64(&p) : mp4d_read_u32(&p);
            offs = (version==1) ? mp4d_read_u64(&p) : mp4d_read_u32(&p);
            if (time <= p_mfra->timestamp && offs > p_mfra->pos) 
            {
                p_mfra->pos = offs;
                p_mfra->pos_time = time;
            }
            mp4d_skip_bytes(&p, ((sizes>>4)&0x3)+1);  /*traf_number*/
            mp4d_skip_bytes(&p, ((sizes>>2)&0x3)+1);  /*trun_number*/
            mp4d_skip_bytes(&p, ((sizes)&0x3)+1);  /*sample_number*/
        }
    }

    return 0;
}

static int
mp4d_parse_udta
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav
    )
{
    mp4d_demuxer_ptr_t p_dmux = p_nav->p_data;
    mp4d_buffer_t p = mp4d_atom_to_buffer(&atom);
    mp4d_atom_t child;
    mp4d_error_t err = MP4D_NO_ERROR;

    if (!MP4D_FOURCC_EQ(p_dmux->md.udta.type, "udta")) {
        p_dmux->md.udta = atom;
    }

    while (mp4d_bytes_left(&p)) 
    {
        err = mp4d_next_atom(&p, &atom, &child);
        if (err) return err;

        if (MP4D_FOURCC_EQ(child.type, "uuid") ||
            MP4D_FOURCC_EQ(child.type, "meta")) 
        {
            /* look for meta in udta (iTunes) or in uuid (Dolby) */
            err = mp4d_dispatch(child, p_nav);
            if (!err && MP4D_FOURCC_EQ(p_dmux->meta.hdlr.handler_type, p_dmux->md.req_type)) {
                p_dmux->md.atom_out = p_dmux->meta.data;
                return 0;
            }
        }
        else if (MP4D_FOURCC_EQ(child.type, p_dmux->md.req_type)) 
        {
            /* look for child type (3GP Asset Information) */
            p_dmux->md.atom_out = child;
            return 0;
        }
    }

    return (int) err;
}

static int
mp4d_parse_meta
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav
    )
{
    mp4d_demuxer_ptr_t p_dmux = p_nav->p_data;
    mp4d_buffer_t p = mp4d_atom_to_buffer(&atom);
    mp4d_error_t err = MP4D_NO_ERROR;
    
    uint8_t version;
    uint32_t flags;
    
    version = mp4d_read_u8(&p);
    flags = mp4d_read_u24(&p);
    
    if (version==0) {
        atom.p_data = p.p_data;
        atom.size   = p.size;
        
        MP4D_FOURCC_ASSIGN(p_dmux->meta.hdlr.handler_type, "\0\0\0\0");
        err = mp4d_find_hdlr(atom, p_nav);
        if (err) return err;
        MP4D_FOURCC_ASSIGN(p_dmux->meta.hdlr.handler_type, p_dmux->hdlr.handler_type);

        if (MP4D_FOURCC_EQ(p_dmux->meta.hdlr.handler_type, "cfmd") ||
            MP4D_FOURCC_EQ(p_dmux->meta.hdlr.handler_type, "dlbt") ||
            MP4D_FOURCC_EQ(p_dmux->meta.hdlr.handler_type, "dlbf") ||
            MP4D_FOURCC_EQ(p_dmux->meta.hdlr.handler_type, "dlbk") ||
            MP4D_FOURCC_EQ(p_dmux->meta.hdlr.handler_type, "dlbm")) 
        {
            err = mp4d_find_atom(&atom, "xml ", 0, &p_dmux->meta.data);
        }
        else if (MP4D_FOURCC_EQ(p_dmux->meta.hdlr.handler_type, "mdir"))
        {
            err = mp4d_find_atom(&atom, "ilst", 0, &p_dmux->meta.data);
        }
        else if (MP4D_FOURCC_EQ(p_dmux->meta.hdlr.handler_type, "ID32"))
        {
            err = mp4d_find_atom(&atom, "ID32", p_dmux->md.req_idx, &p_dmux->meta.data);
        }

        if (err) {
            mp4d_memset(&p_dmux->meta.data, 0, sizeof(p_dmux->meta.data));
        }
                
        return 0;
    }

    return MP4D_E_UNSUPPRTED_FORMAT;

}



/**************************************************
    Low Level 
**************************************************/



static const mp4d_callback_t k_main_dispatcher_list[] = 
{
    {"ftyp", &mp4d_parse_ftyp},
    {"styp", &mp4d_parse_ftyp},
    {"pdin", &mp4d_parse_pdin},
    {"bloc", &mp4d_parse_bloc},
    {"moov", &mp4d_parse_moov},
    {"mvhd", &mp4d_parse_mvhd},
    {"trak", &mp4d_parse_trak},

    /*fragments*/
    {"mvex", &mp4d_parse_box},
    {"mehd", &mp4d_parse_mehd},

    {"moof", &mp4d_parse_box},
    {"mfhd", &mp4d_parse_mfhd},
    {"traf", &mp4d_parse_box},

    /* metadata */
    {"udta", &mp4d_parse_udta},
    {"meta", &mp4d_parse_meta},
    {"hdlr", &mp4d_parse_hdlr},

    /* sentinel */
    {"dumy", NULL},

};

static const mp4d_callback_t k_trak_dispatcher_list[] = 
{
    {"tkhd", &mp4d_parse_tkhd},
    {"tref", &mp4d_parse_tref},
    {"mdia", &mp4d_parse_mdia},
    {"mdhd", &mp4d_parse_mdhd},
    {"minf", &mp4d_parse_box},
    {"hdlr", &mp4d_parse_hdlr},
    {"vmhd", &mp4d_parse_vmhd},
    {"stbl", &mp4d_parse_box},
    {"stsd", &mp4d_parse_stsd},
    
    /*encryption*/
    {"sinf", &mp4d_parse_box},
    {"frma", &mp4d_parse_frma},
    {"schm", &mp4d_parse_schm},
    {"schi", &mp4d_parse_box},
    {"tenc", &mp4d_parse_tenc},
    
    /* sentinel */
    {"dumy", NULL}
};

static const mp4d_callback_t k_uuid_dispatcher_list[] = 
{
    {"DLBY-METADATA-00", &mp4d_parse_box},
    
    /* encryption */
    {"\x89\x74\xdb\xce\x7b\xe7\x4c\x51\x84\xf9\x71\x48\xf9\x88\x25\x54", &mp4d_parse_tenc}, /* Microsoft 'tenc'*/
    
    /* sentinel */
    {"dumy", NULL}
    
};



/**************************************************
    Demuxer API
**************************************************/

const mp4d_version_t *
mp4d_get_version(void)
{
    static const mp4d_version_t v =
    {
        MP4D_VERSION_MAJOR,
        MP4D_VERSION_MINOR,
        MP4D_VERSION_PATCH,
        ""
    };

    return &v;
}

int
mp4d_demuxer_parse 
    (mp4d_demuxer_ptr_t p_dmux
    ,const unsigned char *buffer       /**< input buffer */
    ,uint64_t size         /**< input buffer size */
    ,int is_eof            /**< Indicate this buffer holds all data until the end of the file. 
        This information is needed to parse boxes indicating size=0. */
    ,uint64_t ref_offs      /**< Set the reference offset for this buffer relative to the input file. */
    ,uint64_t *box_size_out   /**< Returns the size of the top level box. In case of a failure 
        this helps refilling the input buffer with the required number of data. box_size_out==0 
        indicates that the is_eof flag needs to be set for successful parsing. */
    )
{
    mp4d_error_t err = 0;

    if (!p_dmux || !buffer || !box_size_out)
        return MP4D_E_WRONG_ARGUMENT;

    /* Clear memory of earlier parses */
    mp4d_memset(&p_dmux->curr, 0, sizeof(p_dmux->curr));

    p_dmux->track_cnt = 0;
    
    err = mp4d_parse_atom_header(buffer, size, &p_dmux->atom);

    if (size < p_dmux->atom.header) {
        *box_size_out = p_dmux->atom.header;
    }
    else {
        *box_size_out = p_dmux->atom.header + p_dmux->atom.size;
    }
    p_dmux->atom_offset = ref_offs;

    if (err)
        return err;

    if ((p_dmux->atom.flags & MP4D_ATOMFLAGS_IS_FINAL_BOX) && !is_eof)
        return MP4D_E_BUFFER_TOO_SMALL;

    err = mp4d_dispatch(p_dmux->atom, &p_dmux->navigator);

    return 0;
}


int
mp4d_demuxer_get_type 
    (mp4d_demuxer_ptr_t p_dmux
    ,mp4d_fourcc_t *type_out
    )
{
    if (!p_dmux || !type_out)
        return MP4D_E_WRONG_ARGUMENT;
    MP4D_FOURCC_ASSIGN(*type_out, p_dmux->atom.type);
    return 0;
}

int
mp4d_demuxer_get_atom(
    const mp4d_demuxer_ptr_t p_dmux,
    mp4d_atom_t *p_atom
    )
{
    ASSURE( p_dmux != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( p_atom != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );

    *p_atom = p_dmux->atom;

    return MP4D_NO_ERROR;
}

int
mp4d_demuxer_get_movie_info 
    (mp4d_demuxer_ptr_t p_dmux
    ,mp4d_movie_info_t *movie_info_out
    )
{
    if (!p_dmux || !movie_info_out)
        return MP4D_E_WRONG_ARGUMENT;
    if (MP4D_FOURCC_EQ(p_dmux->atom.type, "moov")) {
        *movie_info_out = p_dmux->curr.moov.info;
    }
    else {
        return MP4D_E_INVALID_ATOM;
    }

    return 0;
}


static int
mp4d_demuxer_read_track_info
    (mp4d_demuxer_ptr_t  p_dmux         /**< [in]  Pointer to the mp4 demuxer */
    ,uint32_t            stream_num     /**< [in]  The stream_num is not the 'track_id'! It is the xth track found in the 'moov'. Counting starts with 0! */
    )
{
    mp4d_atom_t atom;

    if (MP4D_FOURCC_EQ(p_dmux->atom.type, "moov")) {
        if (mp4d_find_atom(&p_dmux->atom, "trak", stream_num, &atom) != 0)
        {
            return MP4D_E_TRACK_NOT_FOUND;
        }
        return mp4d_parse_trak(atom, &p_dmux->navigator);
    }
    else {
        return MP4D_E_INFO_NOT_AVAIL;
    }
}

int
mp4d_demuxer_get_stream_info
    (mp4d_demuxer_ptr_t  p_dmux         /**< [in]  Pointer to the mp4 demuxer */
    ,uint32_t            stream_num     /**< [in]  The stream_num is not the 'track_id'! It is the xth track found in the 'moov'. Counting starts with 0! */
    ,mp4d_stream_info_t *p_stream_info  /**< [out] Stream info */
    )
{
    mp4d_error_t err;

    if (!p_dmux || !p_stream_info)
    {
        return MP4D_E_WRONG_ARGUMENT;
    }

    /* Do not reset p_dmux->curr because only moov.p_trak will be
       written, other memory has to be retained */
    /* mp4d_memset(&p_dmux->curr, 0, sizeof(p_dmux->curr));  */

    p_dmux->curr.moov.p_trak = &p_dmux->p_scratch->trak;
    mp4d_memset(p_dmux->curr.moov.p_trak, 0, sizeof(mp4d_trak_t));

    err = mp4d_demuxer_read_track_info(p_dmux, stream_num);
    
    ASSURE( p_dmux->curr.moov.p_trak->info.track_id > 0, MP4D_E_INVALID_ATOM, ("Illegal track_ID = 0") );

    if (!err)
    {
        *p_stream_info = p_dmux->curr.moov.p_trak->info;
    }
    
    return err;
}


/**
 * @brief Return the location of the DSI.
 *
 * @return Error code:
 *     OK (0) for success.
 *     MP4D_E_TRACK_NOT_FOUND  - Requested track not in file.
 *     MP4D_E_IDX_OUT_OF_RANGE - Requested DSI index not in track.
 *     MP4D_E_INFO_NOT_AVAIL   - Information is not in this segment.
 */
int
mp4d_demuxer_get_sampleentry
    (mp4d_demuxer_ptr_t  p_dmux                    /**< [in]  Pointer to the mp4 demuxer */
    ,uint32_t            stream_num                /**< [in]  The stream_num is not the 'track_id'! It is the xth track found in the 'moov'. Counting starts with 0! */
    ,uint32_t            sample_description_index  /**< [in]  Index of the DSI in the sample description table. Counting starts with 1! */
    ,mp4d_sampleentry_t *p_sampleentry             /**< [out] Sample entry including the DSI */
    )
{
    mp4d_error_t err;

    if (!p_dmux || !p_sampleentry || !sample_description_index)
    {
        return MP4D_E_WRONG_ARGUMENT;
    }

    /* Do not clear memory of earlier parses */

    p_dmux->curr.moov.p_trak = &p_dmux->p_scratch->trak;
    mp4d_memset(p_dmux->curr.moov.p_trak, 0, sizeof(mp4d_trak_t));
    p_dmux->curr.moov.p_trak->sampleentry_req_idx = sample_description_index;
    
    err = mp4d_demuxer_read_track_info(p_dmux, stream_num);

    if (err != MP4D_NO_ERROR)
    {
        return err;
    }

    {
        /* An error from parsing the stsd was swalled, hence a post-check if the information
         * now looks correct. It would be more robust to propagate the error from the initial parsing. */
        mp4d_trak_t *p_trak = p_dmux->curr.moov.p_trak;

        if ((MP4D_FOURCC_EQ(p_trak->info.hdlr, "vide") && p_trak->sampleentry.vide.dsi == NULL) ||
            (MP4D_FOURCC_EQ(p_trak->info.hdlr, "soun") && p_trak->sampleentry.soun.dsi == NULL) ||
            (MP4D_FOURCC_EQ(p_trak->info.hdlr, "subt") && p_trak->sampleentry.subt.subt_namespace == NULL) ||
            (MP4D_FOURCC_EQ(p_trak->info.hdlr, "meta") && p_trak->sampleentry.meta.content_encoding == NULL))
        {
            err = MP4D_E_IDX_OUT_OF_RANGE;
        }
        else
        {
            *p_sampleentry = p_dmux->curr.moov.p_trak->sampleentry;
        }
    }
    
    return err;
}


int
mp4d_demuxer_fragment_for_time
    (const unsigned char *mfra_buffer
    ,uint64_t mfra_size
    ,uint32_t track_id
    ,uint64_t media_time
    ,uint64_t * p_pos
    ,uint64_t * p_time
    )
{
    static const mp4d_callback_t cb[] = {
        {"mfra", &mp4d_parse_box},
        {"tfra", &mp4d_parse_tfra},
        {"dumy", NULL}
    };

    mp4d_atom_t atom;
    mfra_t data = {track_id, media_time, 0, 0};

    if (!mfra_buffer || !p_pos || !p_time)
        return MP4D_E_WRONG_ARGUMENT;

    CHECK( mp4d_parse_atom_header(mfra_buffer,
                                  mfra_size,
                                  &atom) );

    if (!MP4D_FOURCC_EQ(atom.type, "mfra") )
    {
        *p_pos = 0;
        *p_time = 0;
        return MP4D_NO_ERROR;
    }
    
    {
        struct mp4d_navigator_t_ nav;
        mp4d_navigator_init(&nav, cb, NULL, &data);
        CHECK( mp4d_parse_box(atom, &nav) );
    }
    
    *p_pos  = data.pos;
    *p_time = data.pos_time;

    return MP4D_NO_ERROR;
}

int
mp4d_demuxer_get_sidx_entry
    (mp4d_demuxer_ptr_t p_dmux
    ,uint32_t entry_index
    ,uint64_t *p_offset
    ,uint32_t *p_size
    ,uint64_t *p_time
)
{
    ASSURE( p_dmux != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( p_offset != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( p_size != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( p_time != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );

    ASSURE( MP4D_FOURCC_EQ(p_dmux->atom.type, "sidx"), MP4D_E_INFO_NOT_AVAIL, ("Wrong box, expected sidx"));

    /* parse the sidx */
    {
        mp4d_buffer_t p = mp4d_atom_to_buffer(&p_dmux->atom);
        uint8_t version = mp4d_read_u8(&p);
        uint16_t i, reference_count;

        ASSURE( version == 0 || version == 1, MP4D_E_UNSUPPRTED_FORMAT, ("Unsupported sidx box version = %" PRIu8, version));

        mp4d_read_u24(&p); /* flags */

        mp4d_read_u32(&p); /* reference_ID */
        mp4d_read_u32(&p); /* timescale */

        if (version == 0)
        {
            *p_time = mp4d_read_u32(&p);
            *p_offset = mp4d_read_u32(&p);
        }
        else
        {
            *p_time = mp4d_read_u64(&p);
            *p_offset = mp4d_read_u64(&p);
        }
        mp4d_read_u16(&p); /* reserved */
        reference_count = mp4d_read_u16(&p);

        ASSURE( entry_index < reference_count, MP4D_E_IDX_OUT_OF_RANGE,
                ("Have %" PRIu16 " sidx entries, requested index %" PRIu32, reference_count, entry_index) );

        for (i = 0; i < reference_count; i++)
        {
            uint32_t referenced_size = mp4d_read_u32(&p) & 0x7fffffff;
            uint32_t subsegment_duration = mp4d_read_u32(&p);
            mp4d_read_u32(&p);  /* SAP */

            *p_size = referenced_size;

            if (i < entry_index)
            {
                *p_time += subsegment_duration;
                *p_offset += referenced_size;
            }
            else
            {
                break;
            }
        }
    }

    return MP4D_NO_ERROR;
}

int
mp4d_demuxer_get_sidx_offset
    (mp4d_demuxer_ptr_t p_dmux
    ,uint64_t media_time
    ,uint64_t *p_time
    ,uint64_t *p_pos
    ,uint64_t *p_size
    ,uint32_t *p_index)
{
    ASSURE( p_dmux != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( p_time != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( p_pos != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( p_size != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );
    ASSURE( p_index != NULL, MP4D_E_WRONG_ARGUMENT, ("Null input") );

    ASSURE( MP4D_FOURCC_EQ(p_dmux->atom.type, "sidx"), MP4D_E_INFO_NOT_AVAIL, ("Wrong box, expected sidx"));

    /* parse the sidx */
    {
        mp4d_buffer_t p = mp4d_atom_to_buffer(&p_dmux->atom);
        uint8_t version = mp4d_read_u8(&p);
        uint64_t pts, offset;
        uint16_t reference_count;

        ASSURE( version == 0 || version == 1, MP4D_E_UNSUPPRTED_FORMAT, ("Unsupported sidx box version = %" PRIu8, version));

        mp4d_read_u24(&p); /* flags */

        mp4d_read_u32(&p); /* reference_ID */
        mp4d_read_u32(&p); /* timescale */

        if (version == 0)
        {
            pts = mp4d_read_u32(&p);
            offset = mp4d_read_u32(&p);
        }
        else
        {
            pts = mp4d_read_u64(&p);
            offset = mp4d_read_u64(&p);
        }
        mp4d_read_u16(&p); /* reserved */
        reference_count = mp4d_read_u16(&p);

        *p_pos = offset;
        *p_time = pts;

        for (*p_index = 0; *p_index < reference_count; (*p_index)++)
        {
            uint32_t referenced_size = mp4d_read_u32(&p) & 0x7fffffff;
            uint32_t subsegment_duration = mp4d_read_u32(&p);
            mp4d_read_u32(&p);  /* SAP */

            pts += subsegment_duration;
            offset += referenced_size;

            if (pts <= media_time)
            {
                *p_pos = offset;
                *p_time = pts;
                *p_size = referenced_size;
            }
            else
            {
                break;
            }
        }
    }

    return MP4D_NO_ERROR;
}


int
mp4d_demuxer_read_mfro 
    (const unsigned char *buffer
    ,uint64_t size
    ,uint64_t *p_mfra_size
    )
{
    mp4d_atom_t mfro;
    mp4d_error_t err = 0;

    if (!buffer || !p_mfra_size)
        return MP4D_E_WRONG_ARGUMENT;
    
    *p_mfra_size = 0;
    
    if (size < 16) {
        return MP4D_E_BUFFER_TOO_SMALL;
    }

    err = mp4d_parse_atom_header(buffer+size-16, 16, &mfro);
    if (err) return err;
    
    if (MP4D_FOURCC_EQ(mfro.type,"mfro")) {
        mp4d_buffer_t p = mp4d_atom_to_buffer(&mfro);
        uint8_t version;
        uint32_t flags;
        
        version = mp4d_read_u8(&p);
        flags = mp4d_read_u24(&p);
        
        if (version==0) {
            *p_mfra_size = mp4d_read_u32(&p);
        }
        else {
            return MP4D_E_UNSUPPRTED_FORMAT;
        }
    }
    return 0;
}

int
mp4d_demuxer_get_ftyp_info
    (mp4d_demuxer_ptr_t  p_dmux     /**< [in]  Pointer to the mp4 demuxer */
    ,mp4d_ftyp_info_t   *p_ftyp_info   /**< [out] 'ftyp' information struct */
    )
{
    if (!p_dmux || !p_ftyp_info)
        return MP4D_E_WRONG_ARGUMENT;

    ASSURE( MP4D_FOURCC_EQ(p_dmux->atom.type, "styp") ||
            MP4D_FOURCC_EQ(p_dmux->atom.type, "ftyp"), MP4D_E_INFO_NOT_AVAIL,
            ("Wrong atom type '%c%c%c%c', expected 'ftyp'",
                    p_dmux->atom.type[0],
                    p_dmux->atom.type[1],
                    p_dmux->atom.type[2],
                    p_dmux->atom.type[3]) );

    *p_ftyp_info = p_dmux->curr.ftyp.info;
    
    return 0;
}

int
mp4d_demuxer_get_bloc_info
    (mp4d_demuxer_ptr_t  p_dmux
    ,mp4d_bloc_info_t   *p_bloc_info
    )
{
    if (!p_dmux || !p_bloc_info)
        return MP4D_E_WRONG_ARGUMENT;

    if (MP4D_FOURCC_EQ(p_dmux->atom.type, "bloc")) {
        *p_bloc_info = p_dmux->curr.bloc.info;
    }
    else {
        return MP4D_E_INFO_NOT_AVAIL;
    }
    
    return 0;
}

static int
mp4d_parse_ainf(mp4d_atom_t atom,
        mp4d_navigator_ptr_t p_nav)
{
    mp4d_boxref_t *p_box = p_nav->p_data;
    ATOM2BOXREF(p_box, &atom);
    return MP4D_NO_ERROR;
}


/**
 * @brief Get UltraViolet profile and asset location
 *
 * From moov:ainf
 *
 * @return Error code:
 *      MP4D_NO_ERROR for succes,
 *      MP4D_E_INFO_NOT_AVAIL - ainf box is not present
 */
static int
get_ainf_info(mp4d_demuxer_ptr_t p_demuxer,
              mp4d_boxref_t *p_box             /**< [out] ainf box if found, or NULL */
    )
{
    static const mp4d_callback_t cb[] = {
        {"ainf", mp4d_parse_ainf},
        {"dumy", NULL}
    };
    struct mp4d_navigator_t_ nav;

    nav.atom_hdlr_list = cb;
    nav.uuid_hdlr_list = k_uuid_dispatcher_list;
    nav.p_data = p_box;
    p_box->p_data = NULL;

    CHECK( mp4d_parse_box(p_demuxer->atom, &nav) );

    ASSURE( p_box->p_data != NULL, MP4D_E_INFO_NOT_AVAIL, ("No ainf found") );

    return MP4D_NO_ERROR;
}

struct iloc_parse_t_
{
    mp4d_atom_t idat;     /* The idat atom, or p_data pointing to NULL if idat was not found */
    uint16_t item_ID;     /* Requested item ID */
    int found;            /* (boolean) Found requested item */
    uint64_t item_offset; /* Item offset relative to beginning of idat */
    uint64_t item_size;   /* Item size, or zero to indicate full idat */
};

static int
mp4d_parse_meta_iloc(mp4d_atom_t atom,
                     mp4d_navigator_ptr_t p_nav)
{
    mp4d_buffer_t p = mp4d_atom_to_buffer(&atom);
    uint8_t version = mp4d_read_u8(&p);

    ASSURE( version == 0, MP4D_E_UNSUPPRTED_FORMAT, ("Unsupported meta version %" PRIu8, version) );

    mp4d_read_u24(&p); /* flags */

    while (mp4d_bytes_left(&p)) 
    {
        mp4d_atom_t child;
        CHECK( mp4d_next_atom(&p, NULL, &child) );
        mp4d_dispatch(child, p_nav);
    }

    return 0;
}

static int
mp4d_parse_idat(mp4d_atom_t atom,
                mp4d_navigator_ptr_t p_nav)
{
    struct iloc_parse_t_ *p_data = p_nav->p_data;

    p_data->idat = atom;

    return 0;
}

static int
mp4d_parse_iloc(mp4d_atom_t atom,
                mp4d_navigator_ptr_t p_nav)
{
    struct iloc_parse_t_ *p_data = p_nav->p_data;
    mp4d_buffer_t p = mp4d_atom_to_buffer(&atom);
    uint8_t offset_size, length_size, base_offset_size, index_size;
    uint16_t item_count, i;
    uint8_t version = mp4d_read_u8(&p);

    if (p_data->found)
    {
        warning(("Multiple iloc boxes found")); /* zero or one is required */
        return MP4D_NO_ERROR;
    }

    ASSURE( version == 1, MP4D_E_UNSUPPRTED_FORMAT,
            ("Unsupported iloc box version %" PRIu8 " (because construction_method == 1 is required)",
             version) );

    mp4d_read_u24(&p); /* flags */
    {
        uint8_t u = mp4d_read_u8(&p);
        offset_size = u >> 4;
        length_size = u & 0xf;
        u = mp4d_read_u8(&p);
        base_offset_size = u >> 4;
        index_size = u & 0xf;
    }
    ASSURE( offset_size == 0 || offset_size == 4 || offset_size == 8, MP4D_E_INVALID_ATOM,
            ("offset_size = %" PRIu8 ", expected 0, 4 or 8", offset_size) );
    ASSURE( length_size == 0 || length_size == 4 || length_size == 8, MP4D_E_INVALID_ATOM,
            ("length_size = %" PRIu8 ", expected 0, 4 or 8", length_size) );
    ASSURE( base_offset_size == 0 || base_offset_size == 4 || base_offset_size == 8, MP4D_E_INVALID_ATOM,
            ("base_offset_size = %" PRIu8 ", expected 0, 4 or 8", base_offset_size) );
    item_count = mp4d_read_u16(&p);

    for (i = 0; i < item_count; i++)
    {
        uint16_t extent_count, j;
        uint16_t item_ID = mp4d_read_u16(&p);
        uint8_t construction_method = mp4d_read_u16(&p) & 0xf;
        uint16_t data_reference_index = mp4d_read_u16(&p);
        uint64_t base_offset = 0;   /* suppress compiler warning */

        switch (base_offset_size)
        {
        case 0: base_offset = 0; break;
        case 4: base_offset = mp4d_read_u32(&p); break;
        case 8: base_offset = mp4d_read_u64(&p); break;
        default: assert( 0 ); break;
        }
        extent_count = mp4d_read_u16(&p);

        for (j = 0; j < extent_count; j++)
        {
            /* extent_index */
            switch (index_size)
            {
            case 0: break;
            case 4: mp4d_read_u32(&p); break;
            case 8: mp4d_read_u64(&p); break;
            default: assert( 0 ); break;
            }
            
            /* extent_offset */
            switch (offset_size)
            {
            case 0: p_data->item_offset = base_offset; break;
            case 4: p_data->item_offset = mp4d_read_u32(&p) + base_offset; break;
            case 8: p_data->item_offset = mp4d_read_u64(&p) + base_offset; break;
            default: assert( 0 ); break;
            }

            /* extent_length */
            switch (length_size)
            {
            case 0: p_data->item_size = 0; break; /* entire idat */
            case 4: p_data->item_size = mp4d_read_u32(&p); break;
            case 8: p_data->item_size = mp4d_read_u64(&p); break;
            default: assert( 0 ); break;
            }
        }

        if (item_ID == p_data->item_ID &&   /* requested item */
            construction_method == 1 &&     /* in idat */
            data_reference_index == 0 &&    /* in this file */
            extent_count == 1)              /* non-fragmented */
        {
            p_data->found = 1;
            /* p_data->item_offset and p_data->item_size are already set */
            return MP4D_NO_ERROR;
        }
    }

    /* Item was not found */
    return MP4D_NO_ERROR;
}

int
mp4d_demuxer_get_meta_item
    (mp4d_demuxer_ptr_t p_dmux
     ,uint16_t item_ID
     ,const unsigned char **p_item
     ,uint64_t *p_size
    )
{
    static const mp4d_callback_t cb[] = {
        {"moov", mp4d_parse_box},
        {"meta", mp4d_parse_meta_iloc},
        {"idat", mp4d_parse_idat},
        {"iloc", mp4d_parse_iloc},
        {"dumy", NULL}
    };
    struct mp4d_navigator_t_ nav;
    struct iloc_parse_t_ data;
    
    ASSURE( p_dmux != NULL, MP4D_E_WRONG_ARGUMENT, ("Null pointer") );
    ASSURE( p_item != NULL, MP4D_E_WRONG_ARGUMENT, ("Null pointer") );
    ASSURE( p_size != NULL, MP4D_E_WRONG_ARGUMENT, ("Null pointer") );
    ASSURE( MP4D_FOURCC_EQ(p_dmux->atom.type, "moov") ||
            MP4D_FOURCC_EQ(p_dmux->atom.type, "meta"), MP4D_E_INFO_NOT_AVAIL,
            ("Wrong atom, moov or meta expected") );

    nav.atom_hdlr_list = cb;
    nav.uuid_hdlr_list = NULL;
    nav.p_data = &data;
    data.idat.p_data = NULL;
    data.item_ID = item_ID;
    data.found = 0;

    mp4d_dispatch(p_dmux->atom, &nav);
    ASSURE( data.idat.p_data != NULL, MP4D_E_INFO_NOT_AVAIL, ("Could not find idat box") );
    ASSURE( data.found, MP4D_E_INFO_NOT_AVAIL, ("Could not find iloc box, or item_ID %" PRIu32, item_ID) );

    *p_item = data.idat.p_data + data.item_offset;
    if (data.item_size == 0)
    {
        *p_size = data.idat.size;
    }
    else
    {
        *p_size = data.item_size;
    }

    return MP4D_NO_ERROR;
}


int
mp4d_demuxer_get_pdin_pair
    (mp4d_demuxer_ptr_t  p_dmux
    ,uint32_t            req_rate
    ,mp4d_pdin_info_t   *p_lower
    ,mp4d_pdin_info_t   *p_upper
    )
{
    mp4d_error_t err = 0;
    
    if (!p_dmux || !p_lower || !p_upper)
        return MP4D_E_WRONG_ARGUMENT;

    if (MP4D_FOURCC_EQ(p_dmux->atom.type, "pdin")) {
        p_dmux->curr.pdin.req_rate = req_rate;

        err = mp4d_parse_pdin(p_dmux->atom, &p_dmux->navigator);
        *p_lower = p_dmux->curr.pdin.lower;
        *p_upper = p_dmux->curr.pdin.upper;
    }
    else {
        return MP4D_E_INFO_NOT_AVAIL;
    }
    
    return err;
}

static int
mp4d_select_metadata
    (mp4d_demuxer_ptr_t    p_dmux
    ,mp4d_meta_t    *p_meta
    ,mp4d_fourcc_t   md4cc
    ,mp4d_boxref_t  *p_box
    )
{
    mp4d_error_t err = 0;
    
    p_dmux->md.req_idx = 0;
    if (MP4D_FOURCC_EQ(md4cc, p_meta->hdlr.handler_type)) {
        ATOM2BOXREF(p_box, &p_meta->data);
        return 0;
    }
    else if (MP4D_FOURCC_EQ(p_dmux->md.udta.type, "udta")) {
        MP4D_FOURCC_ASSIGN(p_dmux->md.atom_out.type, "\0\0\0\0");
        MP4D_FOURCC_ASSIGN(p_dmux->md.req_type, md4cc);
        err = mp4d_parse_udta(p_dmux->md.udta, &p_dmux->navigator);
        if (err) return err;

        if (!MP4D_FOURCC_EQ(p_dmux->md.atom_out.type, "\0\0\0\0")) {
            ATOM2BOXREF(p_box, &p_dmux->md.atom_out);
            return 0;
        }
    }
    
    return MP4D_E_INFO_NOT_AVAIL;
}

int
mp4d_demuxer_get_metadata
    (mp4d_demuxer_ptr_t    p_dmux
    ,uint32_t              md_type
    ,mp4d_boxref_t        *p_box
    )
{
    mp4d_fourcc_t md4cc;
    
    if (!p_dmux || !p_box)
        return MP4D_E_WRONG_ARGUMENT;
    
    UINT2FOURCC(md4cc, md_type);
    
    if (MP4D_FOURCC_EQ(md4cc, "ainf"))
    {
        return get_ainf_info(p_dmux, p_box);
    }
    else if (MP4D_FOURCC_EQ(p_dmux->atom.type, "moov")) {
        return mp4d_select_metadata(p_dmux, &p_dmux->curr.moov.meta, md4cc, p_box);
    }
    else if (MP4D_FOURCC_EQ(p_dmux->atom.type, "meta")) {
        return mp4d_select_metadata(p_dmux, &p_dmux->meta, md4cc, p_box);
    }
    else {
        return MP4D_E_INFO_NOT_AVAIL;
    }
}

int
mp4d_demuxer_get_id3v2_tag
    (mp4d_demuxer_ptr_t    p_dmux
    ,uint32_t              idx
    ,mp4d_id3v2_tag_t     *p_tag
    )
{
    static const mp4d_callback_t cb[] = {
        {"moov", mp4d_parse_box},
        {"meta", mp4d_parse_meta},
        {"dumy", NULL}
    };
    struct mp4d_navigator_t_ nav;

    if (!p_dmux || !p_tag)
    {
        return MP4D_E_WRONG_ARGUMENT;
    }

    /* Cannot rely on the previous call to mp4d_parse_meta. Need to set md.reg_idx correctly before
       calling mp4d_parse_meta */

    nav.atom_hdlr_list = cb;
    nav.uuid_hdlr_list = k_uuid_dispatcher_list;
    nav.p_data = p_dmux;

    p_dmux->md.req_idx = idx;

    mp4d_dispatch(p_dmux->atom, &nav);

    ASSURE( MP4D_FOURCC_EQ(p_dmux->meta.hdlr.handler_type, "ID32"), MP4D_E_INFO_NOT_AVAIL,
            ("Meta handler type is %c%c%c%c, not 'ID32'",
             p_dmux->meta.hdlr.handler_type[0],
             p_dmux->meta.hdlr.handler_type[1],
             p_dmux->meta.hdlr.handler_type[2],
             p_dmux->meta.hdlr.handler_type[3]) );

    ASSURE( !MP4D_FOURCC_EQ(p_dmux->meta.data.type, "\0\0\0\0"), MP4D_E_IDX_OUT_OF_RANGE,
            ("Found meta box with handler type 'ID32' but an error happened while getting the ID32 box with index %" PRIu32, 
             idx) );

    ASSURE( MP4D_FOURCC_EQ(p_dmux->meta.data.type, "ID32"), MP4D_E_INFO_NOT_AVAIL,
            ("Found meta box with handler type 'ID32' but the ID32 atom type is '%c%c%c%c",
             p_dmux->meta.data.type[0],
             p_dmux->meta.data.type[1],
             p_dmux->meta.data.type[2],
             p_dmux->meta.data.type[3]) );
    
    {
        mp4d_buffer_t p = mp4d_atom_to_buffer(&p_dmux->meta.data);
        uint32_t flags;
        uint8_t version;
        version = mp4d_read_u8(&p);
        flags = mp4d_read_u24(&p);
        ASSURE( version == 0, MP4D_E_UNSUPPRTED_FORMAT, ("Unsupported ID32 box version: %" PRIu8, version) );

        p_tag->lang = mp4d_read_u16(&p);
        p_tag->p_data = p.p_data;
        p_tag->size = p.size;
    }

    return MP4D_NO_ERROR;
}


/**************************************************
    Demuxer Initialization
**************************************************/

int
mp4d_demuxer_query_mem
    (uint64_t *p_static_mem_size    /**< [out] Static memory size */
    ,uint64_t *p_dynamic_mem_size   /**< [out] Dynamic memory size */
    )
{
    uint64_t static_mem_size = sizeof(struct mp4d_demuxer_t_);
    uint64_t dynamic_mem_size = sizeof(mp4d_demuxer_scratch_t);

    *p_static_mem_size = static_mem_size;
    *p_dynamic_mem_size = dynamic_mem_size;

    return 0;
}


int
mp4d_demuxer_init
    (mp4d_demuxer_ptr_t *p_demuxer_ptr   /**<  */
    ,void *static_mem   /**<  */
    ,void *dynamic_mem   /**<  */

)
{
    mp4d_demuxer_ptr_t p_dmux;

    p_dmux = (mp4d_demuxer_ptr_t) static_mem;
    mp4d_memset (p_dmux, 0, sizeof(struct mp4d_demuxer_t_));

    p_dmux->p_scratch = (mp4d_demuxer_scratch_t *) dynamic_mem;
    p_dmux->p_trak_dispatcher = k_trak_dispatcher_list;
    mp4d_navigator_init(&p_dmux->navigator, k_main_dispatcher_list, k_uuid_dispatcher_list, p_dmux);

    *p_demuxer_ptr = p_dmux;
    return 0;
}
