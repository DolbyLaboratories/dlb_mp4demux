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
 * @brief mp4d_demuxer unit tests
 */

#include <stdlib.h>
#include <stdio.h>

#include "mp4d_demux.h"
#include "mp4d_internal.h"

#include "mp4d_unittest.h"

#ifdef _MSC_VER
#define PRId64 "I64d"
#define PRIu64 "I64u"    
#else
#include <inttypes.h>
#endif

static int
test_atom_header_parsing 
(
    const unsigned char *buffer, 
    uint64_t size, 
    mp4d_error_t expected_behavior,
    mp4d_atom_t expected_atom,
    const char *name
)
{
    mp4d_atom_t atom;
    mp4d_error_t res;
    int err = 0;

    res = (mp4d_error_t) mp4d_parse_atom_header(buffer, size, &atom);

    if (res != expected_behavior) {
        printf ("%s: return value (%d) not as expected (%d)\n", name, res, expected_behavior);
        err |= 1;
    }
    if (atom.size != expected_atom.size) {
        printf ("%s: atom size (%" PRIu64 ") not as expected (%" PRIu64 ")\n", name, atom.size, expected_atom.size);
        err |= 2;
    }
    if (atom.header != expected_atom.header) {
        err |= 4;
        printf ("%s: atom header (%d) not as expected (%d)\n", name, atom.header, expected_atom.header);
    }

    return err;
}

static int
test_demuxer_parse 
(
    mp4d_demuxer_ptr_t demuxer_ptr,
    const unsigned char *buffer, 
    uint64_t size, 
    int is_eof, 
    mp4d_error_t expected_behavior,
    uint64_t expected_size, 
    const char *name
)
{
    uint64_t atom_size;
    mp4d_error_t res;
    int err = 0;

    res = (mp4d_error_t) mp4d_demuxer_parse(demuxer_ptr, buffer, size, is_eof, 0, &atom_size);

    if (res != expected_behavior) {
        printf ("%s: return value (%d) not as expected (%d)\n", name, res, expected_behavior);
        err |= 1;
    }
    if (atom_size != expected_size) {
        printf ("%s: atom size (%" PRIu64 ") not as expected (%" PRIu64 ")\n", name, atom_size, expected_size);
        err |= 2;
    }

    return err;
}


static mp4d_pdin_info_t
create_pdin_info
    (uint32_t rate
    ,uint32_t delay
    )
{
    mp4d_pdin_info_t pdin_info = {rate, delay};
    return pdin_info;
}

static int
test_pdin_parse 
(
    mp4d_demuxer_ptr_t demuxer_ptr,
    mp4d_error_t expected_behavior,
    uint32_t rate, 
    mp4d_pdin_info_t expected_lower,
    mp4d_pdin_info_t expected_upper,
    const char *name
)
{
    mp4d_pdin_info_t lower, upper;
    mp4d_error_t res;
    int checkmask = 0x1;
    int err = 0;

    res = (mp4d_error_t) mp4d_demuxer_get_pdin_pair(demuxer_ptr, rate, &lower, &upper);

    if (res != expected_behavior) {
        printf ("%s: return value (%d) not as expected (%d)\n", name, res, expected_behavior);
        err |= checkmask;
    }
    checkmask <<= 1;
    
    if (lower.rate != expected_lower.rate) {
        printf ("%s: lower rate (0x%08x) not as expected (0x%08x)\n", name, lower.rate, expected_lower.rate);
        err |= checkmask;
    }
    checkmask <<= 1;

    if (upper.rate != expected_upper.rate) {
        printf ("%s: upper rate (0x%08x) not as expected (0x%08x)\n", name, upper.rate, expected_upper.rate);
        err |= checkmask;
    }
    checkmask <<= 1;
    
    if (lower.initial_delay != expected_lower.initial_delay) {
        printf ("%s: lower initial_delay (0x%08x) not as expected (0x%08x)\n", name, lower.initial_delay, expected_lower.initial_delay);
        err |= checkmask;
    }
    checkmask <<= 1;
    
    if (upper.initial_delay != expected_upper.initial_delay) {
        printf ("%s: upper initial_delay (0x%08x) not as expected (0x%08x)\n", name, upper.initial_delay, expected_upper.initial_delay);
        err |= checkmask;
    }
    checkmask <<= 1;
    
    return err;
}

static int
test_ftyp_parse 
(
    mp4d_demuxer_ptr_t demuxer_ptr,
    mp4d_error_t expected_behavior,
    const char *exp_major_brand,
    uint32_t exp_minor_version,
    uint32_t exp_num_comp_brands,
    const char *exp_comp_brand,
    const char *name
)
{
    mp4d_ftyp_info_t ftyp_info;
    mp4d_error_t res;
    int checkmask = 0x1;
    int err = 0;

    res = (mp4d_error_t) mp4d_demuxer_get_ftyp_info(demuxer_ptr, &ftyp_info);

    if (res != expected_behavior) {
        printf ("%s: return value (%d) not as expected (%d)\n", name, res, expected_behavior);
        err |= checkmask;
    }
    checkmask <<= 1;
    
    if (memcmp(ftyp_info.major_brand, exp_major_brand, 4)) {
        printf ("%s: major brand (%c%c%c%c) not as expected (%s)\n", name, 
                ftyp_info.major_brand[0],ftyp_info.major_brand[1],ftyp_info.major_brand[2],ftyp_info.major_brand[3], exp_major_brand);
        err |= checkmask;            
    }
    checkmask <<= 1;
    
    if (ftyp_info.minor_version!=exp_minor_version) {
        printf ("%s: minor version (%u) not as expected (%u)\n", name, ftyp_info.minor_version, exp_minor_version);
        err |= checkmask;            
    }
    checkmask <<= 1;
    
    if (ftyp_info.num_compat_brands!=exp_num_comp_brands) {
        printf ("%s: number of compatible brands (%u) not as expected (%u)\n", name, ftyp_info.num_compat_brands, exp_num_comp_brands);
        err |= checkmask;            
    }
    checkmask <<= 1;
    
    if (exp_comp_brand) {
        uint32_t t;
        int found = 0;
        for (t=0; t<ftyp_info.num_compat_brands; t++) {
            if (memcmp(exp_comp_brand, ftyp_info.compat_brands+4*t, 4)==0)
                found++;
        }
        if (found!=1) {
            printf ("%s: expected compatible brand (%s) found %d times, expected 1\n", name, exp_comp_brand, found);
            err |= checkmask;            
        }
    }
    checkmask <<= 1;

    return err;
}

static int
test_bloc_parse 
(
    mp4d_demuxer_ptr_t demuxer_ptr,
    mp4d_error_t expected_behavior,
    const char *exp_bloc,
    uint32_t exp_bloc_size,
    const char *exp_purl,
    uint32_t exp_purl_size,
    const char *exp_res,
    uint32_t exp_res_size,
    const char *name
)
{
    mp4d_bloc_info_t bloc_info;
    mp4d_error_t res;
    int checkmask = 0x1;
    int err = 0;

    res = (mp4d_error_t) mp4d_demuxer_get_bloc_info(demuxer_ptr, &bloc_info);

    if (res != expected_behavior) {
        printf ("%s: return value (%d) not as expected (%d)\n", name, res, expected_behavior);
        err |= checkmask;
    }
    checkmask <<= 1;
    
    if (exp_bloc && bloc_info.base_location) {
        if (memcmp(bloc_info.base_location, exp_bloc, strlen(exp_bloc)+1)) {
            printf ("%s: base_location (%s) not as expected (%s)\n", name, bloc_info.base_location, exp_bloc);
            err |= checkmask;            
        }
    }
    else {
        if (bloc_info.base_location) {
            printf ("%s: base_location (%s) not as expected (NULL)\n", name, bloc_info.base_location);
            err |= checkmask;            
        }
    }

    checkmask <<= 1;

    if (bloc_info.base_location_size!=exp_bloc_size) {
        printf ("%s: base_location size (%u) not as expected (%u)\n", name, bloc_info.base_location_size, exp_bloc_size);
        err |= checkmask;
    }
    checkmask <<= 1;
    
    if (exp_purl && bloc_info.purchase_location) {
        if (memcmp(bloc_info.purchase_location, exp_purl, strlen(exp_purl)+1)) {
            printf ("%s: purchase_location (%s) not as expected (%s)\n", name, bloc_info.purchase_location, exp_bloc);
            err |= checkmask;
        }
    }
    else {
        if (bloc_info.purchase_location) {
            printf ("%s: purchase_location (%s) not as expected (NULL)\n", name, bloc_info.purchase_location);
            err |= checkmask;
        }
    }
    checkmask <<= 1;
    
    if (bloc_info.purchase_location_size!=exp_purl_size) {
        printf ("%s: purchase_location size (%u) not as expected (%u)\n", name, bloc_info.purchase_location_size, exp_purl_size);
        err |= checkmask;
    }
    checkmask <<= 1;
    
    if (exp_res && bloc_info.reserved) {
        if (memcmp(bloc_info.reserved, exp_res, strlen(exp_res)+1)) {
            printf ("%s: reserved (%s) not as expected (%s)\n", name, bloc_info.reserved, exp_res);
            err |= checkmask;
        }
    }
    else {
        if (bloc_info.reserved) {
            printf ("%s: reserved (%s) not as expected (NULL)\n", name, bloc_info.reserved);
            err |= checkmask;
        }
    }
    checkmask <<= 1;
    
    if (bloc_info.reserved_size!=exp_res_size) {
        printf ("%s: reserved size (%u) not as expected (%u)\n", name, bloc_info.reserved_size, exp_res_size);
        err |= checkmask;
    }
    checkmask <<= 1;
    
    return err;
}

static
void update_counts(int err, int *nfailed, int *ntests)
{
    TEST_UPDATE(err, *nfailed, *ntests);
}

int
main(void)
{
    mp4d_demuxer_ptr_t p_dmux;

    uint64_t static_mem_size, dyn_mem_size;
    void *static_mem_ptr = NULL, *dyn_mem_ptr = NULL;

    char* testname;
    int ntests = 0;
    int nfailed = 0;
    int err;


    mp4d_demuxer_query_mem(&static_mem_size, &dyn_mem_size);

    if (static_mem_size) {
        static_mem_ptr = malloc((size_t) static_mem_size);
    }

    if (dyn_mem_size) {
        dyn_mem_ptr = malloc ((size_t) dyn_mem_size);
    }

    mp4d_demuxer_init(&p_dmux, static_mem_ptr, dyn_mem_ptr);

    TEST_START("Demuxer");

    {
        static const unsigned char buffer[18] = "ABCDEFGHIJKLMNOP";
        uint64_t size;

        for (size=0; size<sizeof(buffer); size++)
        {
            mp4d_buffer_t b8  = {buffer, size, buffer};
            mp4d_buffer_t b16 = {buffer, size, buffer};
            mp4d_buffer_t b24 = {buffer, size, buffer};
            mp4d_buffer_t b32 = {buffer, size, buffer};
            mp4d_buffer_t b64 = {buffer, size, buffer};
            uint8_t  u8;
            uint16_t u16;
            uint32_t u24;
            uint32_t u32;
            uint64_t u64;
            mp4d_fourcc_t c;
            unsigned char b[18];
            uint32_t t,offs;

            u8  = mp4d_read_u8(&b8);
            if (size<1) err=!(u8==((uint8_t)-1) && b8.size==((uint64_t)-1)); else err=!(u8=='A');
            update_counts(err, &nfailed, &ntests);
            if (err) printf("read_u8 (1) for size=%" PRIu64 " failed\n", size);

            u8  = mp4d_read_u8(&b8);
            if (size<2) err=!(u8==(uint8_t)-1 && b8.size==(uint64_t)-1); else err=!(u8=='B');
            update_counts(err, &nfailed, &ntests);
            if (err) printf("read_u8 (2) for size=%" PRIu64 " failed\n", size);
            
            if (size<2) 
                err = !mp4d_is_buffer_error(&b8);
            else 
                err = mp4d_is_buffer_error(&b8);
            update_counts(err, &nfailed, &ntests);
            if (err) printf("read_u8 EOB for size=%" PRIu64 " failed\n", size);

            offs=2;
            for (t=0; t<5; t++) {
                mp4d_read(&b8, b, t);
                if (size<t+offs) 
                    err = (memcmp(b, "\0\0\0\0\0", t)!=0);
                else 
                    err = (memcmp(b, buffer+offs, t)!=0);
                update_counts(err, &nfailed, &ntests);
                if (err) printf("read (1.%u) for size=%" PRIu64 " failed\n", t, size);
                offs += t;

                if (size<offs) 
                    err = !mp4d_is_buffer_error(&b8);
                else 
                    err = mp4d_is_buffer_error(&b8);
                update_counts(err, &nfailed, &ntests);
                if (err) printf("read EOB for size=%" PRIu64 " failed\n", size);
                
            }
            
            u16 = mp4d_read_u16(&b16);
            if (size<2) err=!(u16==(uint16_t)-1 && b16.size==(uint64_t)-1); else err=!(u16==0x4142);
            update_counts(err, &nfailed, &ntests);
            if (err) printf("read_u16 (1) for size=%" PRIu64 " failed\n", size);

            u16 = mp4d_read_u16(&b16);
            if (size<4) err=!(u16==(uint16_t)-1 && b16.size==(uint64_t)-1); else err=!(u16==0x4344);
            update_counts(err, &nfailed, &ntests);
            if (err) printf("read_u16 (2) for size=%" PRIu64 " failed\n", size);

            u24 = mp4d_read_u24(&b24);
            if (size<3) err=!(u24==(uint32_t)-1 && b24.size==(uint64_t)-1); else err=!(u24==0x414243);
            update_counts(err, &nfailed, &ntests);
            if (err) printf("read_u24 (1) for size=%" PRIu64 " failed\n", size);

            u24 = mp4d_read_u24(&b24);
            if (size<6) err=!(u24==(uint32_t)-1 && b24.size==(uint64_t)-1); else err=!(u24==0x444546);
            update_counts(err, &nfailed, &ntests);
            if (err) printf("read_u24 (2) for size=%" PRIu64 " failed\n", size);

            u32 = mp4d_read_u32(&b32);
            if (size<4) err=!(u32==(uint32_t)-1 && b32.size==(uint64_t)-1); else err=!(u32==0x41424344);
            update_counts(err, &nfailed, &ntests);
            if (err) printf("read_u32 (1) for size=%" PRIu64 " failed\n", size);

            u32 = mp4d_read_u32(&b32);
            if (size<8) err=!(u32==(uint32_t)-1 && b32.size==(uint64_t)-1); else err=!(u32==0x45464748);
            update_counts(err, &nfailed, &ntests);
            if (err) printf("read_u32 (2) for size=%" PRIu64 " failed\n", size);

            mp4d_read_fourcc(&b32, c);
            if (size<12) err=!(MP4D_FOURCC_EQ(c, "\0\0\0\0") && b32.size==(uint64_t)-1); else err=!(MP4D_FOURCC_EQ(c, "IJKL"));
            update_counts(err, &nfailed, &ntests);
            if (err) printf("read_fourcc (1) for size=%" PRIu64 " failed\n", size);

            mp4d_read_fourcc(&b32, c);
            if (size<16) err=!(MP4D_FOURCC_EQ(c, "\0\0\0\0") && b32.size==(uint64_t)-1); else err=!(MP4D_FOURCC_EQ(c, "MNOP"));
            update_counts(err, &nfailed, &ntests);
            if (err) printf("read_fourcc (2) for size=%" PRIu64 " failed\n", size);

            u64 = mp4d_read_u64(&b64);
            if (size<8) err=!(u64==(uint64_t)-1 && b64.size==(uint64_t)-1); else err=!(u64==0x4142434445464748ull);
            update_counts(err, &nfailed, &ntests);
            if (err) printf("read_u64 (1) for size=%" PRIu64 " failed\n", size);

            u64 = mp4d_read_u64(&b64);
            if (size<16) err=!(u64==(uint64_t)-1 && b64.size==(uint64_t)-1); else err=!(u64==0x494a4b4c4d4e4f50ull);
            update_counts(err, &nfailed, &ntests);
            if (err) printf("read_u64 (2) for size=%" PRIu64 " failed\n", size);

        }
    }

    {
        static const unsigned char buffer[] = "ABCDEF";
        static const uint64_t offsets[] = {0, 2, 3, 5, 6, 5, 4, 2, 1};
        mp4d_buffer_t b = {buffer, sizeof(buffer), buffer};
        uint8_t u8;
        size_t i;
                
        for (i = 0; i < sizeof(offsets) / sizeof(*offsets); i++)
        {
            uint64_t offset = offsets[i];

            mp4d_seek(&b, offset);
            u8 = mp4d_read_u8(&b);
            
            err = !(u8 == buffer[offset]);

            update_counts(err, &nfailed, &ntests);
            if (err) printf("seek for offset=%" PRIu64 " failed\n", offset);
        }

        mp4d_seek(&b, 7);
        err = !(b.size == (uint64_t) -1);
        update_counts(err, &nfailed, &ntests);
        if (err) printf("illegal seek u8 failed\n");
    }

    {
        mp4d_atom_t atom;
        const unsigned char *buf;
        uint64_t size = 19;

        static const unsigned char atom_with_size_0[19] = {'\0', '\0', '\0', '\0', 'm', 'd', 'a', 't', 'd', 'a', 't', 'a'};
        static const unsigned char atom_with_wrong_size_2[19] = {'\0', '\0', '\0', 2, 'm', 'd', 'a', 't', 'd', 'a', 't', 'a'};
        static const unsigned char atom_with_wrong_size_7[19] = {'\0', '\0', '\0', 7, 'm', 'd', 'a', 't', 'd', 'a', 't', 'a'};
        static const unsigned char atom_with_size_8[19] = {'\0', '\0', '\0', 8, 'm', 'd', 'a', 't', 'd', 'a', 't', 'a'};
        static const unsigned char atom_with_size_18[19] = {'\0', '\0', '\0', 18, 'm', 'd', 'a', 't', 'd', 'a', 't', 'a'};
        static const unsigned char atom_with_size_19[19] = {'\0', '\0', '\0', 19, 'm', 'd', 'a', 't', 'd', 'a', 't', 'a'};
        static const unsigned char atom_too_small_with_size_20[19] = {'\0', '\0', '\0', 20, 'm', 'd', 'a', 't', 'd', 'a', 't', 'a'};

        atom.header = 8;

        buf = atom_with_size_0;
        atom.size = size - atom.header;

        testname = "parse header of 32-bit atom with size=0";
        err = test_atom_header_parsing(buf, size, MP4D_NO_ERROR, atom, testname);
        update_counts(err, &nfailed, &ntests);

        testname = "demux of 32-bit atom with size=0, eof=1";
        err = test_demuxer_parse(p_dmux, buf, size, 1, MP4D_NO_ERROR, atom.size+atom.header, testname);
        update_counts(err, &nfailed, &ntests);

        testname = "demux of 32-bit atom with size=0, eof=0";
        err = test_demuxer_parse(p_dmux, buf, size, 0, MP4D_E_BUFFER_TOO_SMALL, atom.size+atom.header, testname);
        update_counts(err, &nfailed, &ntests);

        buf = atom_with_wrong_size_2;
        atom.size = 2;

        testname = "parse header of 32-bit atom with size=2";
        err = test_atom_header_parsing(buf, size, MP4D_E_INVALID_ATOM, atom, testname);
        update_counts(err, &nfailed, &ntests);

        testname = "demux of 32-bit atom with size=2, eof=1";
        err = test_demuxer_parse(p_dmux, buf, size, 1, MP4D_E_INVALID_ATOM, atom.size+atom.header, testname);
        update_counts(err, &nfailed, &ntests);

        testname = "demux of 32-bit atom with size=2, eof=0";
        err = test_demuxer_parse(p_dmux, buf, size, 0, MP4D_E_INVALID_ATOM, atom.size+atom.header, testname);
        update_counts(err, &nfailed, &ntests);

        buf = atom_with_wrong_size_7;
        atom.size = 7;

        testname = "parse header of 32-bit atom with size=7";
        err = test_atom_header_parsing(buf, size, MP4D_E_INVALID_ATOM, atom, testname);
        update_counts(err, &nfailed, &ntests);

        testname = "demux of 32-bit atom with size=7, eof=1";
        err = test_demuxer_parse(p_dmux, buf, size, 1, MP4D_E_INVALID_ATOM, atom.size+atom.header, testname);
        update_counts(err, &nfailed, &ntests);

        testname = "demux of 32-bit atom with size=7, eof=0";
        err = test_demuxer_parse(p_dmux, buf, size, 0, MP4D_E_INVALID_ATOM, atom.size+atom.header, testname);
        update_counts(err, &nfailed, &ntests);

        buf = atom_with_size_8;
        atom.size = 8 - atom.header;

        testname = "parse header of 32-bit atom with size=8";
        err = test_atom_header_parsing(buf, size, MP4D_NO_ERROR, atom, testname);
        update_counts(err, &nfailed, &ntests);

        testname = "demux of 32-bit atom with size=8, eof=1";
        err = test_demuxer_parse(p_dmux, buf, size, 1, MP4D_NO_ERROR, atom.size+atom.header, testname);
        update_counts(err, &nfailed, &ntests);

        testname = "demux of 32-bit atom with size=8, eof=0";
        err = test_demuxer_parse(p_dmux, buf, size, 0, MP4D_NO_ERROR, atom.size+atom.header, testname);
        update_counts(err, &nfailed, &ntests);

        buf = atom_with_size_18;
        atom.size = 18 - atom.header;

        testname = "parse header of 32-bit atom with size=18";
        err = test_atom_header_parsing(buf, size, MP4D_NO_ERROR, atom, testname);
        update_counts(err, &nfailed, &ntests);

        testname = "demux of 32-bit atom with size=18, eof=1";
        err = test_demuxer_parse(p_dmux, buf, size, 1, MP4D_NO_ERROR, atom.size+atom.header, testname);
        update_counts(err, &nfailed, &ntests);

        testname = "demux of 32-bit atom with size=18, eof=0";
        err = test_demuxer_parse(p_dmux, buf, size, 0, MP4D_NO_ERROR, atom.size+atom.header, testname);
        update_counts(err, &nfailed, &ntests);

        buf = atom_with_size_19;
        atom.size = 19 - atom.header;

        testname = "parse header of 32-bit atom with size=19";
        err = test_atom_header_parsing(buf, size, MP4D_NO_ERROR, atom, testname);
        update_counts(err, &nfailed, &ntests);

        testname = "demux of 32-bit atom with size=19, eof=1";
        err = test_demuxer_parse(p_dmux, buf, size, 1, MP4D_NO_ERROR, atom.size+atom.header, testname);
        update_counts(err, &nfailed, &ntests);

        testname = "demux of 32-bit atom with size=19, eof=0";
        err = test_demuxer_parse(p_dmux, buf, size, 0, MP4D_NO_ERROR, atom.size+atom.header, testname);
        update_counts(err, &nfailed, &ntests);

        buf = atom_too_small_with_size_20;
        atom.size = 20 - atom.header;

        testname = "parse header of 32-bit atom with size=20";
        err = test_atom_header_parsing(buf, size, MP4D_E_BUFFER_TOO_SMALL, atom, testname);
        update_counts(err, &nfailed, &ntests);

        testname = "demux of 32-bit atom with size=20, eof=1";
        err = test_demuxer_parse(p_dmux, buf, size, 1, MP4D_E_BUFFER_TOO_SMALL, atom.size+atom.header, testname);
        update_counts(err, &nfailed, &ntests);

        testname = "demux of 32-bit atom with size=20, eof=0";
        err = test_demuxer_parse(p_dmux, buf, size, 0, MP4D_E_BUFFER_TOO_SMALL, atom.size+atom.header, testname);
        update_counts(err, &nfailed, &ntests);

    }

    {
        mp4d_atom_t atom;
        const unsigned char *buf;
        const uint64_t size = 21;

        static const unsigned char atom_with_64bit_size_0[21] = {'\0', '\0', '\0', '\1', 'm', 'd', 'a', 't', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', 'd', 'a', 't', 'a'};
        static const unsigned char atom_with_64bit_size_1[21] = {'\0', '\0', '\0', '\1', 'm', 'd', 'a', 't', '\0', '\0', '\0', '\0', '\0', '\0', '\0', 1, 'd', 'a', 't', 'a'};
        static const unsigned char atom_with_64bit_size_2[21] = {'\0', '\0', '\0', '\1', 'm', 'd', 'a', 't', '\0', '\0', '\0', '\0', '\0', '\0', '\0', 2, 'd', 'a', 't', 'a'};
        static const unsigned char atom_with_64bit_size_7[21] = {'\0', '\0', '\0', '\1', 'm', 'd', 'a', 't', '\0', '\0', '\0', '\0', '\0', '\0', '\0', 7, 'd', 'a', 't', 'a'};
        static const unsigned char atom_with_64bit_size_15[21] = {'\0', '\0', '\0', '\1', 'm', 'd', 'a', 't', '\0', '\0', '\0', '\0', '\0', '\0', '\0', 15, 'd', 'a', 't', 'a'};
        static const unsigned char atom_with_64bit_size_16[21] = {'\0', '\0', '\0', '\1', 'm', 'd', 'a', 't', '\0', '\0', '\0', '\0', '\0', '\0', '\0', 16, 'd', 'a', 't', 'a'};
        static const unsigned char atom_with_64bit_size_20[21] = {'\0', '\0', '\0', '\1', 'm', 'd', 'a', 't', '\0', '\0', '\0', '\0', '\0', '\0', '\0', 20, 'd', 'a', 't', 'a'};
        static const unsigned char atom_with_64bit_size_21[21] = {'\0', '\0', '\0', '\1', 'm', 'd', 'a', 't', '\0', '\0', '\0', '\0', '\0', '\0', '\0', 21, 'd', 'a', 't', 'a'};
        static const unsigned char atom_with_64bit_size_22[21] = {'\0', '\0', '\0', '\1', 'm', 'd', 'a', 't', '\0', '\0', '\0', '\0', '\0', '\0', '\0', 22, 'd', 'a', 't', 'a'};

        atom.header = 16;

        buf = atom_with_64bit_size_0;
        atom.size = 0;

        testname = "parse header of 64-bit atom with size=0";
        err = test_atom_header_parsing(buf, size, MP4D_E_INVALID_ATOM, atom, testname);
        update_counts(err, &nfailed, &ntests);

        testname = "demux of 64-bit atom with size=0, eof=1";
        err = test_demuxer_parse(p_dmux, buf, size, 1, MP4D_E_INVALID_ATOM, atom.size+atom.header, testname);
        update_counts(err, &nfailed, &ntests);

        testname = "demux of 64-bit atom with size=0, eof=0";
        err = test_demuxer_parse(p_dmux, buf, size, 0, MP4D_E_INVALID_ATOM, atom.size+atom.header, testname);
        update_counts(err, &nfailed, &ntests);

        buf = atom_with_64bit_size_1;
        atom.size = 1;

        testname = "parse header of 64-bit atom with size=1";
        err = test_atom_header_parsing(buf, size, MP4D_E_INVALID_ATOM, atom, testname);
        update_counts(err, &nfailed, &ntests);

        testname = "demux of 64-bit atom with size=1, eof=1";
        err = test_demuxer_parse(p_dmux, buf, size, 1, MP4D_E_INVALID_ATOM, atom.size+atom.header, testname);
        update_counts(err, &nfailed, &ntests);

        testname = "demux of 64-bit atom with size=1, eof=0";
        err = test_demuxer_parse(p_dmux, buf, size, 0, MP4D_E_INVALID_ATOM, atom.size+atom.header, testname);
        update_counts(err, &nfailed, &ntests);

        buf = atom_with_64bit_size_2;
        atom.size = 2;

        testname = "parse header of 64-bit atom with size=2";
        err = test_atom_header_parsing(buf, size, MP4D_E_INVALID_ATOM, atom, testname);
        update_counts(err, &nfailed, &ntests);

        testname = "demux of 64-bit atom with size=2, eof=1";
        err = test_demuxer_parse(p_dmux, buf, size, 1, MP4D_E_INVALID_ATOM, atom.size+atom.header, testname);
        update_counts(err, &nfailed, &ntests);

        testname = "demux of 64-bit atom with size=2, eof=0";
        err = test_demuxer_parse(p_dmux, buf, size, 0, MP4D_E_INVALID_ATOM, atom.size+atom.header, testname);
        update_counts(err, &nfailed, &ntests);

        buf = atom_with_64bit_size_7;
        atom.size = 7;

        testname = "parse header of 64-bit atom with size=7";
        err = test_atom_header_parsing(buf, size, MP4D_E_INVALID_ATOM, atom, testname);
        update_counts(err, &nfailed, &ntests);

        testname = "demux of 64-bit atom with size=7, eof=1";
        err = test_demuxer_parse(p_dmux, buf, size, 1, MP4D_E_INVALID_ATOM, atom.size+atom.header, testname);
        update_counts(err, &nfailed, &ntests);

        testname = "demux of 64-bit atom with size=7, eof=0";
        err = test_demuxer_parse(p_dmux, buf, size, 0, MP4D_E_INVALID_ATOM, atom.size+atom.header, testname);
        update_counts(err, &nfailed, &ntests);

        buf = atom_with_64bit_size_15;
        atom.size = 15;

        testname = "parse header of 64-bit atom with size=15";
        err = test_atom_header_parsing(buf, size, MP4D_E_INVALID_ATOM, atom, testname);
        update_counts(err, &nfailed, &ntests);

        testname = "demux of 64-bit atom with size=15, eof=1";
        err = test_demuxer_parse(p_dmux, buf, size, 1, MP4D_E_INVALID_ATOM, atom.size+atom.header, testname);
        update_counts(err, &nfailed, &ntests);

        testname = "demux of 64-bit atom with size=15, eof=0";
        err = test_demuxer_parse(p_dmux, buf, size, 0, MP4D_E_INVALID_ATOM, atom.size+atom.header, testname);
        update_counts(err, &nfailed, &ntests);

        buf = atom_with_64bit_size_16;
        atom.size = 16 - atom.header;

        testname = "parse header of 64-bit atom with size=16";
        err = test_atom_header_parsing(buf, size, MP4D_NO_ERROR, atom, testname);
        update_counts(err, &nfailed, &ntests);

        testname = "demux of 64-bit atom with size=16, eof=1";
        err = test_demuxer_parse(p_dmux, buf, size, 1, MP4D_NO_ERROR, atom.size+atom.header, testname);
        update_counts(err, &nfailed, &ntests);

        testname = "demux of 64-bit atom with size=16, eof=0";
        err = test_demuxer_parse(p_dmux, buf, size, 0, MP4D_NO_ERROR, atom.size+atom.header, testname);
        update_counts(err, &nfailed, &ntests);

        buf = atom_with_64bit_size_20;
        atom.size = 20 - atom.header;

        testname = "parse header of 64-bit atom with size=20";
        err = test_atom_header_parsing(buf, size, MP4D_NO_ERROR, atom, testname);
        update_counts(err, &nfailed, &ntests);

        testname = "demux of 64-bit atom with size=20, eof=1";
        err = test_demuxer_parse(p_dmux, buf, size, 1, MP4D_NO_ERROR, atom.size+atom.header, testname);
        update_counts(err, &nfailed, &ntests);

        testname = "demux of 64-bit atom with size=20, eof=0";
        err = test_demuxer_parse(p_dmux, buf, size, 0, MP4D_NO_ERROR, atom.size+atom.header, testname);
        update_counts(err, &nfailed, &ntests);

        buf = atom_with_64bit_size_21;
        atom.size = 21 - atom.header;

        testname = "parse header of 64-bit atom with size=21";
        err = test_atom_header_parsing(buf, size, MP4D_NO_ERROR, atom, testname);
        update_counts(err, &nfailed, &ntests);

        testname = "demux of 64-bit atom with size=21, eof=1";
        err = test_demuxer_parse(p_dmux, buf, size, 1, MP4D_NO_ERROR, atom.size+atom.header, testname);
        update_counts(err, &nfailed, &ntests);

        testname = "demux of 64-bit atom with size=21, eof=0";
        err = test_demuxer_parse(p_dmux, buf, size, 0, MP4D_NO_ERROR, atom.size+atom.header, testname);
        update_counts(err, &nfailed, &ntests);

        buf = atom_with_64bit_size_22;
        atom.size = 22 - atom.header;

        testname = "parse header of 64-bit atom with size=22";
        err = test_atom_header_parsing(buf, size, MP4D_E_BUFFER_TOO_SMALL, atom, testname);
        update_counts(err, &nfailed, &ntests);

        testname = "demux of 64-bit atom with size=22, eof=1";
        err = test_demuxer_parse(p_dmux, buf, size, 1, MP4D_E_BUFFER_TOO_SMALL, atom.size+atom.header, testname);
        update_counts(err, &nfailed, &ntests);

        testname = "demux of 64-bit atom with size=22, eof=0";
        err = test_demuxer_parse(p_dmux, buf, size, 0, MP4D_E_BUFFER_TOO_SMALL, atom.size+atom.header, testname);
        update_counts(err, &nfailed, &ntests);
    }
    
    {
        const unsigned char *buf;
        uint64_t size;
        
        static const unsigned char pdin_0_entry[] = {
            0x00, 0x00, 0x00, 0x00, 
            'p', 'd', 'i', 'n', 
            0x00, 0x00, 0x00, 0x00, 
        };
        static const unsigned char pdin_1_entry[] = {
            0x00, 0x00, 0x00, 0x00, 
            'p', 'd', 'i', 'n', 
            0x00, 0x00, 0x00, 0x00, 
            0x01, 0x00, 0x00, 0x00, 
            0x04, 0x05, 0x06, 0x07
        };
        static const unsigned char pdin_2_entry[] = {
            0x00, 0x00, 0x00, 0x00, 
            'p', 'd', 'i', 'n', 
            0x00, 0x00, 0x00, 0x00, 
            0x02, 0x00, 0x00, 0x00, 
            0x05, 0x05, 0x06, 0x07, 
            0x01, 0x00, 0x00, 0x00, 
            0x04, 0x05, 0x06, 0x07
        };
        static const unsigned char pdin_2_entry_dup[] = {
            0x00, 0x00, 0x00, 0x00, 
            'p', 'd', 'i', 'n', 
            0x00, 0x00, 0x00, 0x00, 
            0x01, 0x00, 0x00, 0x00, 
            0x04, 0x05, 0x06, 0x07,
            0x01, 0x00, 0x00, 0x00, 
            0x04, 0x05, 0x06, 0x07
        };
        static const unsigned char pdin_3_entry[] = {
            0x00, 0x00, 0x00, 0x00, 
            'p', 'd', 'i', 'n', 
            0x00, 0x00, 0x00, 0x00, 
            0x02, 0x00, 0x00, 0x00, 
            0x05, 0x05, 0x06, 0x07, 
            0x03, 0x00, 0x00, 0x00, 
            0x06, 0x05, 0x06, 0x07, 
            0x01, 0x00, 0x00, 0x00, 
            0x04, 0x05, 0x06, 0x07
        };
        static const unsigned char pdin_3_entry_dup[] = {
            0x00, 0x00, 0x00, 0x00, 
            'p', 'd', 'i', 'n', 
            0x00, 0x00, 0x00, 0x00, 
            0x02, 0x00, 0x00, 0x00, 
            0x05, 0x05, 0x06, 0x07, 
            0x01, 0x00, 0x00, 0x00, 
            0x04, 0x05, 0x06, 0x07, 
            0x01, 0x00, 0x00, 0x00, 
            0x04, 0x05, 0x06, 0x07
        };

        /* zero entries */

        buf = pdin_0_entry;
        size = sizeof(pdin_0_entry);
        
        testname = "demux of 32-bit 'pdin' with 0 entries, eof=1";
        err = test_demuxer_parse(p_dmux, buf, size, 1, MP4D_NO_ERROR, size, testname);
        update_counts(err, &nfailed, &ntests);
        
        testname = "pdin entry for requested_rate < minrate from 'pdin' with 0 entries";
        err = test_pdin_parse(p_dmux, MP4D_NO_ERROR, 0x00010000, create_pdin_info(0x0,0xffffffff), create_pdin_info(0xffffffff,0x0), testname);
        update_counts(err, &nfailed, &ntests);
        
        testname = "pdin entry for maxrate < requested_rate from 'pdin' with 0 entries";
        err = test_pdin_parse(p_dmux, MP4D_NO_ERROR, 0x01010000, create_pdin_info(0x0,0xffffffff), create_pdin_info(0xffffffff,0x0), testname);
        update_counts(err, &nfailed, &ntests);
        
        /* one entry */
        
        buf = pdin_1_entry;
        size = sizeof(pdin_1_entry);
        
        testname = "demux of 32-bit 'pdin' with 1 entries, eof=1";
        err = test_demuxer_parse(p_dmux, buf, size, 1, MP4D_NO_ERROR, size, testname);
        update_counts(err, &nfailed, &ntests);
        
        testname = "pdin entry for requested_rate < minrate from 'pdin' with 1 entries";
        err = test_pdin_parse(p_dmux, MP4D_NO_ERROR, 0x00010000, create_pdin_info(0x01000000,0x04050607), create_pdin_info(0xffffffff,0x0), testname);
        update_counts(err, &nfailed, &ntests);
        
        testname = "pdin entry for maxrate < requested_rate from 'pdin' with 1 entries";
        err = test_pdin_parse(p_dmux, MP4D_NO_ERROR, 0x08010000, create_pdin_info(0x0,0xffffffff), create_pdin_info(0x01000000,0x04050607), testname);
        update_counts(err, &nfailed, &ntests);
        
        /* two entries with one duplicate set */
        
        buf = pdin_2_entry_dup;
        size = sizeof(pdin_2_entry_dup);
        
        testname = "demux of 32-bit 'pdin' with 2 entries, one duplicate, eof=1";
        err = test_demuxer_parse(p_dmux, buf, size, 1, MP4D_NO_ERROR, size, testname);
        update_counts(err, &nfailed, &ntests);
        
        testname = "pdin entry for requested_rate < minrate from 'pdin' with 2 entries, one duplicate";
        err = test_pdin_parse(p_dmux, MP4D_NO_ERROR, 0x00010000, create_pdin_info(0x01000000,0x04050607), create_pdin_info(0xffffffff,0x0), testname);
        update_counts(err, &nfailed, &ntests);
        
        testname = "pdin entry for maxrate < requested_rate from 'pdin' with 2 entries, one duplicate";
        err = test_pdin_parse(p_dmux, MP4D_NO_ERROR, 0x08010000, create_pdin_info(0x0,0xffffffff), create_pdin_info(0x01000000,0x04050607), testname);
        update_counts(err, &nfailed, &ntests);
        
        /* two entries */
        
        buf = pdin_2_entry;
        size = sizeof(pdin_2_entry);
        
        testname = "demux of 32-bit 'pdin' with 2 entries, eof=1";
        err = test_demuxer_parse(p_dmux, buf, size, 1, MP4D_NO_ERROR, size, testname);
        update_counts(err, &nfailed, &ntests);
        
        testname = "pdin entry for requested_rate < minrate from 'pdin' with 2 entries";
        err = test_pdin_parse(p_dmux, MP4D_NO_ERROR, 0x00010000, create_pdin_info(0x01000000,0x04050607), create_pdin_info(0x02000000,0x05050607), testname);
        update_counts(err, &nfailed, &ntests);
        
        testname = "pdin entry for maxrate < requested_rate from 'pdin' with 2 entries";
        err = test_pdin_parse(p_dmux, MP4D_NO_ERROR, 0x08010000, create_pdin_info(0x01000000,0x04050607), create_pdin_info(0x02000000,0x05050607), testname);
        update_counts(err, &nfailed, &ntests);
        
        testname = "pdin entry for minrate < requested_rate < maxrate from 'pdin' with 2 entries";
        err = test_pdin_parse(p_dmux, MP4D_NO_ERROR, 0x01010000, create_pdin_info(0x01000000,0x04050607), create_pdin_info(0x02000000,0x05050607), testname);
        update_counts(err, &nfailed, &ntests);
        
        /* two entries, box 7 bytes too long */
        
        buf = pdin_3_entry;
        size = sizeof(pdin_3_entry)-1;
        
        testname = "demux of 32-bit 'pdin' with 2 entries, box 7 bytes too long, eof=1";
        err = test_demuxer_parse(p_dmux, buf, size, 1, MP4D_NO_ERROR, size, testname);
        update_counts(err, &nfailed, &ntests);
        
        testname = "pdin entry for requested_rate < minrate from 'pdin' with 2 entries, box 7 bytes too long";
        err = test_pdin_parse(p_dmux, MP4D_NO_ERROR, 0x00010000, create_pdin_info(0x02000000,0x05050607), create_pdin_info(0x03000000,0x06050607), testname);
        update_counts(err, &nfailed, &ntests);
        
        testname = "pdin entry for maxrate < requested_rate from 'pdin' with 2 entries, box 7 bytes too long";
        err = test_pdin_parse(p_dmux, MP4D_NO_ERROR, 0x08010000, create_pdin_info(0x02000000,0x05050607), create_pdin_info(0x03000000,0x06050607), testname);
        update_counts(err, &nfailed, &ntests);
        
        testname = "pdin entry for minrate < requested_rate < maxrate from 'pdin' with 2 entries, box 7 bytes too long";
        err = test_pdin_parse(p_dmux, MP4D_NO_ERROR, 0x01010000, create_pdin_info(0x02000000,0x05050607), create_pdin_info(0x03000000,0x06050607), testname);
        update_counts(err, &nfailed, &ntests);
        
        /* three entries with one duplicate set */
        
        buf = pdin_3_entry_dup;
        size = sizeof(pdin_3_entry_dup);
        
        testname = "demux of 32-bit 'pdin' with 3 entries, one duplicate, eof=1";
        err = test_demuxer_parse(p_dmux, buf, size, 1, MP4D_NO_ERROR, size, testname);
        update_counts(err, &nfailed, &ntests);
        
        testname = "pdin entry for requested_rate < minrate from 'pdin' with 3 entries, one duplicate";
        err = test_pdin_parse(p_dmux, MP4D_NO_ERROR, 0x00010000, create_pdin_info(0x01000000,0x04050607), create_pdin_info(0x02000000,0x05050607), testname);
        update_counts(err, &nfailed, &ntests);
        
        testname = "pdin entry for maxrate < requested_rate from 'pdin' with 3 entries, one duplicate";
        err = test_pdin_parse(p_dmux, MP4D_NO_ERROR, 0x08010000, create_pdin_info(0x01000000,0x04050607), create_pdin_info(0x02000000,0x05050607), testname);
        update_counts(err, &nfailed, &ntests);
        
        testname = "pdin entry for minrate < requested_rate < maxrate from 'pdin' with 3 entries, one duplicate";
        err = test_pdin_parse(p_dmux, MP4D_NO_ERROR, 0x01010000, create_pdin_info(0x01000000,0x04050607), create_pdin_info(0x02000000,0x05050607), testname);
        update_counts(err, &nfailed, &ntests);
        
        /* three entries */
        
        buf = pdin_3_entry;
        size = sizeof(pdin_3_entry);
        
        testname = "demux of 32-bit 'pdin' with 3 entries, eof=1";
        err = test_demuxer_parse(p_dmux, buf, size, 1, MP4D_NO_ERROR, size, testname);
        update_counts(err, &nfailed, &ntests);
        
        testname = "pdin entry for requested_rate < minrate from 'pdin' with 3 entries";
        err = test_pdin_parse(p_dmux, MP4D_NO_ERROR, 0x00010000, create_pdin_info(0x01000000,0x04050607), create_pdin_info(0x02000000,0x05050607), testname);
        update_counts(err, &nfailed, &ntests);
        
        testname = "pdin entry for maxrate < requested_rate from 'pdin' with 3 entries";
        err = test_pdin_parse(p_dmux, MP4D_NO_ERROR, 0x08010000, create_pdin_info(0x02000000,0x05050607), create_pdin_info(0x03000000,0x06050607), testname);
        update_counts(err, &nfailed, &ntests);
        
        testname = "pdin entry for minrate < requested_rate < middle_rate from 'pdin' with 3 entries";
        err = test_pdin_parse(p_dmux, MP4D_NO_ERROR, 0x01010000, create_pdin_info(0x01000000,0x04050607), create_pdin_info(0x02000000,0x05050607), testname);
        update_counts(err, &nfailed, &ntests);
        
        testname = "pdin entry for middle_rate < requested_rate < maxrate from 'pdin' with 3 entries";
        err = test_pdin_parse(p_dmux, MP4D_NO_ERROR, 0x02ff0000, create_pdin_info(0x02000000,0x05050607), create_pdin_info(0x03000000,0x06050607), testname);
        update_counts(err, &nfailed, &ntests);
        
        
    }

    {
        const unsigned char *buf;
        uint64_t size;
        
        static const unsigned char ftyp_0_entry[] = {
            0x00, 0x00, 0x00, 0x10, 
            'f', 't', 'y', 'p', 
            'a', 'b', 'c', 'd', 
            0x01, 0x02, 0x03, 0x04, 
        };
        static const unsigned char ftyp_2_entry[] = {
            0x00, 0x00, 0x00, 0x00, 
            'f', 't', 'y', 'p', 
            'a', 'b', 'c', 'd', 
            0x01, 0x02, 0x03, 0x04, 
            'e', 'f', 'g', 'h', 
            'm', 'n', 'o', 'p', 
        };
        
        /* no compatible brands */
        
        buf = ftyp_0_entry;
        size = sizeof(ftyp_0_entry);
        
        testname = "demux of 32-bit 'ftyp' with no compatible brands, eof=0";
        err = test_demuxer_parse(p_dmux, buf, size, 0, MP4D_NO_ERROR, size, testname);
        update_counts(err, &nfailed, &ntests);
        
        testname = "'ftyp' with no compatible brands";
        err = test_ftyp_parse(p_dmux, MP4D_NO_ERROR, "abcd", 0x01020304, 0, NULL, testname);
        update_counts(err, &nfailed, &ntests);
        
        /* one compatible brands, three bytes extra */
        
        buf = ftyp_2_entry;
        size = sizeof(ftyp_2_entry)-1;
        
        testname = "demux of 32-bit 'ftyp' with one compatible brand, eof=1";
        err = test_demuxer_parse(p_dmux, buf, size, 1, MP4D_NO_ERROR, size, testname);
        update_counts(err, &nfailed, &ntests);
        
        testname = "brand 'efgh' in 'ftyp' with one compatible brand";
        err = test_ftyp_parse(p_dmux, MP4D_NO_ERROR, "abcd", 0x01020304, 1, "efgh", testname);
        update_counts(err, &nfailed, &ntests);
        
        /* two compatible brands */
        
        buf = ftyp_2_entry;
        size = sizeof(ftyp_2_entry);
        
        testname = "demux of 32-bit 'ftyp' with two compatible brands, eof=1";
        err = test_demuxer_parse(p_dmux, buf, size, 1, MP4D_NO_ERROR, size, testname);
        update_counts(err, &nfailed, &ntests);
        
        testname = "brand 'efgh' in 'ftyp' with two compatible brands";
        err = test_ftyp_parse(p_dmux, MP4D_NO_ERROR, "abcd", 0x01020304, 2, "efgh", testname);
        update_counts(err, &nfailed, &ntests);
        
        testname = "brand 'mnop' in 'ftyp' with two compatible brands";
        err = test_ftyp_parse(p_dmux, MP4D_NO_ERROR, "abcd", 0x01020304, 2, "mnop", testname);
        update_counts(err, &nfailed, &ntests);
        
        
    }
    
    {
        const unsigned char *buf;
        uint64_t size;
        
        char bloc_entry[12+1024] = {
            0x00, 0x00, 0x00, 0x00, 
            'b', 'l', 'o', 'c', 
            0x00, 0x00, 0x00, 0x00
        };
        
        /* empty 'bloc' */
        
        buf = (unsigned char*) bloc_entry;
        size = sizeof(bloc_entry);
        
        testname = "demux of 32-bit empty 'bloc', eof=1";
        err = test_demuxer_parse(p_dmux, buf, size, 1, MP4D_NO_ERROR, size, testname);
        update_counts(err, &nfailed, &ntests);
        
        testname = "empty 'bloc'";
        err = test_bloc_parse(p_dmux, MP4D_NO_ERROR, "", 256, "", 256, "", 512, testname);
        update_counts(err, &nfailed, &ntests);
        
        /* 'bloc', all entries set */
        
        strcpy(bloc_entry+12, "bloc");
        strcpy(bloc_entry+12+256, "purl");
        strcpy(bloc_entry+12+256+256, "reserved");
        
        testname = "demux of 32-bit 'bloc' with all entries set, eof=1";
        err = test_demuxer_parse(p_dmux, buf, size, 1, MP4D_NO_ERROR, size, testname);
        update_counts(err, &nfailed, &ntests);
        
        testname = "'bloc' with all entries set";
        err = test_bloc_parse(p_dmux, MP4D_NO_ERROR, "bloc", 256, "purl", 256, "reserved", 512, testname);
        update_counts(err, &nfailed, &ntests);
        
        /* 'bloc', wrong version */
        
        bloc_entry[8] = 0x01; /* set version to 1 */
        
        testname = "demux of 32-bit 'bloc' with wrong version, eof=1";
        err = test_demuxer_parse(p_dmux, buf, size, 1, MP4D_NO_ERROR, size, testname);
        update_counts(err, &nfailed, &ntests);
        
        testname = "'bloc' with wrong version";
        err = test_bloc_parse(p_dmux, MP4D_NO_ERROR, NULL, 0, NULL, 0, NULL, 0, testname);
        update_counts(err, &nfailed, &ntests);
        
        /* 'bloc', wrong size */
        
        bloc_entry[8] = 0x00;  /* reset version */
        bloc_entry[2] = 0x02;  /* set size of box to 512 */
        
        testname = "demux of 32-bit 'bloc' with wrong size, eof=1";
        err = test_demuxer_parse(p_dmux, buf, size, 1, MP4D_NO_ERROR, 512, testname);
        update_counts(err, &nfailed, &ntests);
        
        testname = "'bloc' with wrong size";
        err = test_bloc_parse(p_dmux, MP4D_NO_ERROR, "bloc", 256, "purl", 256-12, NULL, 0, testname);
        update_counts(err, &nfailed, &ntests);
        
        
        
        
    }    
    
    if (dyn_mem_ptr)
        free(dyn_mem_ptr);
    if (static_mem_ptr)
        free(static_mem_ptr);

    TEST_END(nfailed, ntests);
}


