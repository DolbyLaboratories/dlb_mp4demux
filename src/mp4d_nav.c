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

#include "mp4d_nav.h"
#include "mp4d_internal.h"

#include "mp4d_box_read.h"  /* used by track reader */

#include <assert.h>

static const
mp4d_callback_t k_dummy_list[] = {
    {"0123456789abcdef", NULL}
};

void
mp4d_navigator_init
    (mp4d_navigator_ptr_t p_nav
    ,const mp4d_callback_t * p_atom_hdlr_list
    ,const mp4d_callback_t * p_uuid_hdlr_list
    ,void * obj
    )
{
    p_nav->atom_hdlr_list = (p_atom_hdlr_list) ? p_atom_hdlr_list : k_dummy_list;
    p_nav->uuid_hdlr_list = (p_uuid_hdlr_list) ? p_uuid_hdlr_list : k_dummy_list;
    p_nav->p_data = obj;
}

int
mp4d_parse_atom_header
    (const unsigned char *buffer  /**< payload in */
    ,uint64_t size          /**< payload size, either from input buffer or parent atom */
    ,mp4d_atom_t *atom_out
    )
{
    mp4d_buffer_t p = {buffer, size, buffer};

    if (!atom_out || !buffer)
        return MP4D_E_WRONG_ARGUMENT;
        
    mp4d_memset(atom_out, 0, sizeof(mp4d_atom_t));
    
    atom_out->header = 8;
    atom_out->size = size;
    if (size < atom_out->header)
        return MP4D_E_BUFFER_TOO_SMALL;
        
    atom_out->size = mp4d_read_u32(&p);
    if (1 == atom_out->size) {
        /* 64-bit size */
        atom_out->flags |= MP4D_ATOMFLAGS_IS_64BIT_BOX;
        atom_out->header = 16;
        if (size < atom_out->header)
        {
            atom_out->size = size;
            return MP4D_E_BUFFER_TOO_SMALL;
        }
    }
    else if (0 == atom_out->size) {
        /* size equals file size */
        atom_out->flags |= MP4D_ATOMFLAGS_IS_FINAL_BOX;
        atom_out->size = size;
    }

    mp4d_read_fourcc(&p, atom_out->type);

    if (1 == atom_out->size)
    {
        atom_out->size = mp4d_read_u64(&p);
    }
    
    if (MP4D_FOURCC_EQ(atom_out->type,"uuid")) {
        atom_out->p_uuid = p.p_data;
        mp4d_skip_bytes(&p, 16);
        atom_out->header += 16;
    }
    else {
        atom_out->p_uuid = NULL;
    }
    
    if (atom_out->size < atom_out->header)
        return MP4D_E_INVALID_ATOM;
    atom_out->size -= atom_out->header;
    atom_out->p_data = p.p_data;

    if (size < atom_out->size + atom_out->header)
        return MP4D_E_BUFFER_TOO_SMALL;

    return 0;
}

int
mp4d_dispatch
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav
    )
{
    const mp4d_callback_t *t = p_nav->atom_hdlr_list;
    mp4d_error_t err = MP4D_E_ATOM_UNKNOWN;

    if (atom.p_uuid) {
        t = p_nav->uuid_hdlr_list;
    }

    while(t->parser != NULL)
    {
        int (*parser) (mp4d_atom_t atom, struct mp4d_navigator_t_ * p_nav) = NULL;
        
        if (atom.p_uuid) {
            if (mp4d_memcmp(atom.p_uuid, t->type, 16)==0) {
                parser = t->parser;
            }
        }
        else {
            if (MP4D_FOURCC_EQ(atom.type, t->type)) {
                parser = t->parser;
            }
        }
                
        if (parser) {
            err = (*parser)(atom, p_nav);
            if (err != 0)
            {
                debug(("Warning: ignoring error %d when parsing %c%c%c%c\n",
                       err, atom.type[0], atom.type[1], atom.type[2], atom.type[3]));
            }
            err = 0;
            break;
        }
        t++;
    }
    
    return err;
}



int
mp4d_next_atom
    (mp4d_buffer_t     * p_buf
    ,mp4d_atom_t       * p_parent
    ,mp4d_atom_t       * p_next
    )
{
    mp4d_error_t err = 0;
    
    err = mp4d_parse_atom_header(p_buf->p_data, p_buf->size, p_next);
    if (err) return err;
    p_next->p_parent = p_parent;
    if (p_next->header > p_buf->size)
        return MP4D_E_BUFFER_TOO_SMALL;
        
    mp4d_skip_bytes(p_buf, p_next->header);
    if (p_next->size > p_buf->size)
        return MP4D_E_BUFFER_TOO_SMALL;
    
    mp4d_skip_bytes(p_buf, p_next->size);

    return err;
}



int
mp4d_parse_box
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav
    )
{
    mp4d_buffer_t p = mp4d_atom_to_buffer(&atom);
    mp4d_error_t err = 0;
    mp4d_atom_t child;
    
    while (mp4d_bytes_left(&p)) 
    {
        err = mp4d_next_atom(&p, &atom, &child);
        if (err) return err;
        mp4d_dispatch (child, p_nav);
    }
    return err;
}

mp4d_error_t
mp4d_find_atom
    (mp4d_atom_t *atom
    ,const char *type
    ,uint32_t occurence
    ,mp4d_atom_t *child
    )
{
    mp4d_buffer_t p = mp4d_atom_to_buffer(atom);
    
    while (p.size > 0) 
    {
        if (mp4d_parse_atom_header(p.p_data, p.size, child))
            return MP4D_E_INVALID_ATOM;   /* parse error */
        child->p_parent = atom;
        if (child->header > p.size)
            return MP4D_E_INVALID_ATOM;  /* parse error */

        mp4d_skip_bytes(&p, child->header);
        if (child->size > p.size)
            return MP4D_E_INVALID_ATOM;  /* parse error */

        if (MP4D_FOURCC_EQ(child->type, type) && 0 == occurence--)
            return MP4D_NO_ERROR;
        
        mp4d_skip_bytes(&p, child->size);
    }
    return MP4D_E_ATOM_UNKNOWN; /* not found */
}


