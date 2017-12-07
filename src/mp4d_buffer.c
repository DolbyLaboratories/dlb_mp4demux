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
#include <assert.h>

uint8_t
mp4d_read_u8(mp4d_buffer_t * p)
{
    uint8_t val = (uint8_t)-1;
    if (p->size < 1 || p->size == (uint64_t)-1) {
        p->size = (uint64_t)-1;
    }
    else {
        val = *(p->p_data++);
        p->size -= 1;
    }
    return val;
}

uint16_t
mp4d_read_u16(mp4d_buffer_t * p)
{
    uint16_t val = (uint16_t)-1;
    if (p->size < 2 || p->size == (uint64_t)-1) {
        p->size = (uint64_t)-1;
        return val;
    }
    else {
        val = *(p->p_data++);
        val = (val<<8) + *(p->p_data++);
        p->size -= 2;
    }
    return val;
}


uint32_t
mp4d_read_u24(mp4d_buffer_t * p)
{
    uint32_t val = (uint32_t)-1;
    if (p->size < 3 || p->size == (uint64_t)-1) {
        p->size = (uint64_t)-1;
        return val;
    }
    else {
        val = *(p->p_data++);
        val = (val<<8) + *(p->p_data++);
        val = (val<<8) + *(p->p_data++);
        p->size -= 3;
    }
    return val;
}

uint32_t
mp4d_read_u32(mp4d_buffer_t * p)
{
    const uint64_t size = 4;
    uint32_t val = (uint32_t)-1;
    if (p->size < size || p->size == (uint64_t)-1) {
        p->size = (uint64_t)-1;
        return val;
    }
    else {
        val = *(p->p_data++);
        val = (val<<8) + *(p->p_data++);
        val = (val<<8) + *(p->p_data++);
        val = (val<<8) + *(p->p_data++);
        p->size -= size;
    }
    return val;
}

uint64_t
mp4d_read_u64(mp4d_buffer_t * p)
{
    const uint64_t size = 8;
    uint64_t val = (uint64_t)-1;
    if (p->size < size || p->size == (uint64_t)-1) {
        p->size = (uint64_t)-1;
        return val;
    }
    else {
        val = *(p->p_data++);
        val = (val<<8) + *(p->p_data++);
        val = (val<<8) + *(p->p_data++);
        val = (val<<8) + *(p->p_data++);
        val = (val<<8) + *(p->p_data++);
        val = (val<<8) + *(p->p_data++);
        val = (val<<8) + *(p->p_data++);
        val = (val<<8) + *(p->p_data++);
        p->size -= size;
    }
    return val;
}


void
mp4d_skip_bytes(mp4d_buffer_t * p, uint64_t size)
{
    if (p->size < size || p->size == (uint64_t)-1) {
        p->size = (uint64_t)-1;
    }
    else {
        p->p_data += size;
        p->size -= size;
    }
}

void
mp4d_seek(mp4d_buffer_t * p, uint64_t offset)
{
    if (p->p_begin + offset >= p->p_data + p->size)
    {
        p->size = (uint64_t)-1;
    }
    else
    {
        p->size = p->p_data + p->size - (p->p_begin + offset);
        p->p_data = p->p_begin + offset;
    }
}

void 
mp4d_read
    (mp4d_buffer_t * p 
    ,unsigned char * p_val
    ,uint32_t size
    )
{
    if (p->size < size || p->size == (uint64_t)-1) {
        p->size = (uint64_t)-1;
        mp4d_memset(p_val, 0, size);
    }
    else {
        mp4d_memcpy(p_val, p->p_data, size);
        p->p_data += size;
        p->size -= size;
    }
}


void
mp4d_read_fourcc(mp4d_buffer_t * p, mp4d_fourcc_t four_cc)
{
    mp4d_read(p, four_cc, 4);
}


mp4d_buffer_t
mp4d_atom_to_buffer(const mp4d_atom_t * p_atom)
{
    mp4d_buffer_t buf;
    buf.p_begin = p_atom->p_data;
    buf.p_data = p_atom->p_data;
    buf.size = p_atom->size;
    return buf;
}

