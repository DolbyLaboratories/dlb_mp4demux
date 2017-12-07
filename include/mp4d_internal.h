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

#ifndef MP4D_INTERNAL_H
#define MP4D_INTERNAL_H

#include "mp4d_types.h"
#include "mp4d_nav.h"

#include <string.h>

#ifndef NDEBUG
#include <stdio.h>
#include <stdarg.h>
#define debug(msg) printf msg
#define warning(msg) printf("WARNING: "); printf msg;
#ifdef _MSC_VER
#define PRId64 "I64d"
#define PRIu64 "I64u"
#define PRIu32 "I32u"
#define PRIu16 "hu"
#define PRId16 "hd"
#define PRIu8 "u"
#define PRIz "Iu"
#else
#include <inttypes.h>
#define PRIz "zu"
#endif   /* _MSC_VER */
#else    /* NDEBUG is defined */
#define debug(msg) (void) 0
#define warning(msg) (void) 0
#endif

#define UINT2FOURCC(a, b)   \
do {                        \
    (a)[0] = (unsigned char)(((uint32_t)b)>>24)&0xff;  \
    (a)[1] = (unsigned char)(((uint32_t)b)>>16)&0xff;  \
    (a)[2] = (unsigned char)(((uint32_t)b)>>8)&0xff;  \
    (a)[3] = (unsigned char)(((uint32_t)b))&0xff;  \
} while (0)

#define ATOM2BOXREF(a, b)   \
    do {                        \
        MP4D_FOURCC_ASSIGN((a)->type, (b)->type); \
        (a)->header = (b)->header; \
        (a)->size   = (b)->size; \
        (a)->p_data = (b)->p_data; \
    } while (0)

/* Macro for raising an error */
#ifndef NDEBUG
#define ASSURE(expr, err, msg)     \
do {                               \
    if (!(expr))                   \
    {                              \
        printf msg;                \
        printf("\n");              \
        return (err);              \
    }                              \
} while (0)
#else
#define ASSURE(expr, err, msg)   \
do {                             \
    if (!(expr))                 \
    {                            \
        return (err);            \
    }                            \
} while (0)
#endif

/* Propagate an error if non-zero */
#define CHECK(err)                                \
do {                                              \
    mp4d_error_t check_err = (err);               \
    if (check_err != 0)                           \
    {                                             \
        return check_err;                         \
    }                                             \
} while(0)

/****************************************************************************
    Internal Data Types
****************************************************************************/

typedef struct mp4d_ftyp_t_ {
    mp4d_ftyp_info_t info;
} mp4d_ftyp_t;

typedef struct mp4d_pdin_t_ {
    uint32_t         num_pdin_infos;   /**< Number of entries in pdin box. */
    uint32_t         req_rate;         /**< Requested rate. */ 
    mp4d_pdin_info_t upper;            /**< pdin info set for the closest higher rate. */
    mp4d_pdin_info_t lower;            /**< pdin info set for the closest lower rate. */
} mp4d_pdin_t;

typedef struct mp4d_bloc_t_ {
    mp4d_bloc_info_t info;
} mp4d_bloc_t;


typedef struct mp4d_hdlr_t_ {
    mp4d_fourcc_t handler_type;
    const unsigned char * p_string;
} mp4d_hdlr_t;

typedef struct mp4d_meta_t_ {
    mp4d_hdlr_t hdlr;
    mp4d_atom_t data;
} mp4d_meta_t;

typedef struct mp4d_metadata_t_ {
    mp4d_fourcc_t req_type;
    uint32_t req_idx;
    mp4d_atom_t udta;
    mp4d_atom_t atom_out;
} mp4d_metadata_t;

typedef struct mp4d_crypt_t_ {
    mp4d_fourcc_t scheme_type;
    uint32_t scheme_version;
    mp4d_crypt_info_t info;
} mp4d_crypt_t;

typedef struct mp4d_trak_t_ {
    uint32_t sampleentry_req_idx;   /* Counting from 1 */
    mp4d_stream_info_t info;
    mp4d_sampleentry_t sampleentry;
    mp4d_crypt_t crypt;
} mp4d_trak_t;

typedef struct mp4d_moov_t_ {
    mp4d_movie_info_t info;
    mp4d_trak_t * p_trak;
    mp4d_meta_t meta;
} mp4d_moov_t;

typedef union mp4d_scratch_t_ {
    mp4d_trak_t trak;
} mp4d_demuxer_scratch_t;


struct mp4d_demuxer_t_ {
    mp4d_atom_t atom;
    uint64_t atom_offset;    /**< Offset of atom relative to the beginning of the input file. */
    uint32_t movie_timescale;
    uint32_t track_cnt;
    union {
        mp4d_ftyp_t ftyp;
        mp4d_bloc_t bloc;
        mp4d_pdin_t pdin;
        mp4d_moov_t moov;
    } curr;
    mp4d_hdlr_t hdlr;
    mp4d_meta_t meta;
    mp4d_metadata_t md;
    const mp4d_callback_t *p_trak_dispatcher;
    mp4d_demuxer_scratch_t * p_scratch;
    struct mp4d_navigator_t_ navigator;
};


/****************************************************************************
    System Calls
****************************************************************************/

#define mp4d_memcpy(a,b,c)  memcpy(a,b,c)
#define mp4d_memset(a,b,c)  memset(a,b,c)
#define mp4d_memcmp(a,b,c)  memcmp((a),(b),(c))

/**************************************************
    Low Level
**************************************************/

/*
  @brief tkhd parser callback
*/
int
mp4d_parse_tkhd
    (mp4d_atom_t atom
    ,struct mp4d_navigator_t_ *p_dmux
    );


#endif  /* MP4D_INTERNAL_H */
