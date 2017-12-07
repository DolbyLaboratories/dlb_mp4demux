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
/** @addtogroup mp4d_demux
 * @{
 */

#ifndef MP4D_TYPES_H
#define MP4D_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum mp4d_error_t_ {
    MP4D_NO_ERROR = 0
    , MP4D_E_WRONG_ARGUMENT
    , MP4D_E_BUFFER_TOO_SMALL
    , MP4D_E_INVALID_ATOM
    , MP4D_E_ATOM_UNKNOWN
    , MP4D_E_NEXT_SEGMENT
    , MP4D_E_PREV_SEGMENT
    , MP4D_E_INFO_NOT_AVAIL
    , MP4D_E_TRACK_NOT_FOUND
    , MP4D_E_IDX_OUT_OF_RANGE
    , MP4D_E_UNSUPPRTED_FORMAT
    , MP4D_E_SKIP_BIG_BOX
} mp4d_error_t;

/**
   The maximum number of auxillary data associated with a sample
 */
#define MP4D_MAX_AUXDATA 4

/** @brief compare 4cc types */
#define MP4D_FOURCC_EQ(a, b) ((unsigned char)((a)[0]) == (unsigned char)((b)[0]) && \
                              (unsigned char)((a)[1]) == (unsigned char)((b)[1]) && \
                              (unsigned char)((a)[2]) == (unsigned char)((b)[2]) && \
                              (unsigned char)((a)[3]) == (unsigned char)((b)[3]))

/** @brief Assign to a variable of type mp4d_fourcc_t */
#define MP4D_FOURCC_ASSIGN(a, b) \
do {                             \
    (a)[0] = (b)[0];             \
    (a)[1] = (b)[1];             \
    (a)[2] = (b)[2];             \
    (a)[3] = (b)[3];             \
} while (0)

/** @brief The 4cc type */
typedef unsigned char mp4d_fourcc_t[4];

/**
    @brief Box Reference
    Provides access to a particular box in the current buffer. This is used e.g. for accessing metadata.
*/
typedef struct mp4d_boxref_t_ {
    mp4d_fourcc_t type;             /**< Type of the box. */
    uint32_t header;                /**< Size of the box header without version and flags for full boxes. */
    uint64_t size;                  /**< Size of the box payload incl. version and flags for full boxes. */
    const unsigned char * p_data;   /**< Pointer to data in current input buffer. */
} mp4d_boxref_t;

/**
    @brief Auxillary Data Reference

    Any sample can have one or more auxillary data sets. Those data sets
    are used e.g. to store sample dependent decryption information like
    IVs and the ranges in H.264 samples that remain unencrypted. The 
    corresponding boxes are 'saiz' and 'saio'.
*/
typedef struct mp4d_auxref_t_ {
    uint32_t datatype;/**< aux type as defined in 'saiz' */
    uint64_t pos;     /**< position in input buffer */
    uint8_t size;    /**< size of sample in octets, or zero 
                          if no sample aux info is available */
} mp4d_auxref_t;

/**
    @brief Senc Data Reference

    Any sample can have one senc data sets if it's encrypted in UV file. Those data sets
    are used e.g. to store sample necryption information like
    IV and clear data encrypted data in H.264 subsample. 
*/
typedef struct mp4d_sencref_t_ {
    unsigned char iv[16];
    uint16_t subsample_count;
    uint8_t * ClearEncryptBytes;
}mp4d_sencref_t;

/**
    @brief MP4 Sample Reference

    The demuxer will not return the samples directly but such sample references.
    The reference provides information how to locate the sample in the input
    buffer plus additional sample specific data.
*/
typedef struct mp4d_sampleref_t_ {
    uint64_t dts;     /**< decoding time stamp in media time scale,
                         relative to the beginning of the presentation */
    uint64_t cts;     /**< composition time stamp in media time scale, 
                         relative to the beginning of the presentation */
    uint32_t flags;   /**< sample flags */
    uint64_t pos;     /**< position in input buffer */
    uint32_t size;    /**< size of sample in octets */
    uint32_t sample_description_index;    /**< index of the sample entry
                                             in the Sample Description Box */
    uint16_t num_subsamples; /**< number (>= 1) of subsamples in this sample */
    mp4d_auxref_t auxdata[MP4D_MAX_AUXDATA];   /**< auxillary sample information
                                                  references (needed for decryption) */
    mp4d_sencref_t sencdata;
    int64_t pts;                    /**< presentation time stamp of sample
                                         beginning (media time scale), relative
                                         to the beginning of the presentation */
    uint32_t presentation_offset;   /**< offset into sample where presentation
                                         starts (media time scale) */
    uint32_t presentation_duration; /**< length of sample used in presentation
                                         (media time scale). If zero, the sample
                                         is not part of the presentation. */
    uint8_t pic_type;                /**< picture type info from trik box(refer CFF-1.0.5 2.2.7) for details,
                                             default value is 0, if the file don't have trik box. */
    uint8_t dependency_level;        /**< the level of dependency of this sample from trik box (refer CFF-1.0.5 2.2.7) for details,
                                             default value is 0, if the file don't have trik box. */
    uint32_t samples_per_chunk;      /**< read from stsc box and is useful for QT files */

    uint32_t is_first_sample_in_segment;
} mp4d_sampleref_t;


/**
    @brief MP4 Stream Info
*/
typedef struct mp4d_stream_info_t_ {
    uint32_t track_id;    /**< track_id used to identify tracks in a mux. This may be different from the track number! */
    uint32_t flags;       /**< The 24-bit flags of the Track Header Box (tkhd) of this track. 8 most significant bits are unused */
    uint32_t time_scale;  /**< media time scale */
    uint64_t media_dur;   /**< track duration in this segment */
    uint16_t media_lang;  /**< media language */
    mp4d_fourcc_t hdlr;   /**< media handler type 4CC */
    mp4d_fourcc_t codec;  /**< codec 4CC */
    uint32_t num_dsi;     /**< Number of DSI. Each DSI can be obtained with get_dsi(). */
    uint32_t tkhd_width;  /**< width parameter in track header */
    uint32_t tkhd_height; /**< height parameter in track header */
    uint32_t vmhd_flag;

} mp4d_stream_info_t;


/**
    @brief MP4 Movie Info
*/
typedef struct mp4d_movie_info_t_ {
    uint32_t num_streams; /**< Number of tracks */
    uint32_t time_scale;  /**< Movie time scale */
    uint64_t movie_dur;   /**< Movie duration in movie time scale or zero.
                             Fragmented files report the overall duration. */
} mp4d_movie_info_t;

/**
    @brief Encryption information
*/
typedef struct mp4d_crypt_info_t_ {
    uint32_t method;            /**< Signals the encryption method according to ISO/IEC 23001-7:2011, section 9.2, 'IsEncrypted' */
    uint8_t iv_size;            /**< IV size in bytes */
    unsigned char key_id[16];
} mp4d_crypt_info_t;


/**
    @brief Visual Sample Entry
*/
typedef struct mp4d_sampleentry_visual_t_ {
    uint16_t data_reference_index;
    mp4d_fourcc_t dsi_type;
    uint64_t dsi_size;
    const unsigned char * dsi;
    mp4d_fourcc_t dsi_type_cry;
    mp4d_crypt_info_t crypt_info;
    uint64_t child_data_size;
    const unsigned char * child_data;
    uint16_t width;
    uint16_t height;
    uint16_t depth;
    int par_present;
    uint32_t par_hspacing;
    uint32_t par_vspacing;
    unsigned char compressorname[32];
    uint32_t avcC_flag;
    uint32_t hvcC_flag;
    uint32_t dvcC_flag;
    uint32_t avcE_flag;
    uint32_t hvcE_flag;
    uint32_t tref_vide_flag;
    uint64_t dv_dsi_size;
    const unsigned char * dv_dsi;
    uint64_t dv_el_dsi_size;
    const unsigned char * dv_el_dsi;
    mp4d_fourcc_t sampleentry_name;
} mp4d_sampleentry_visual_t;


/**
    @brief Audio Sample Entry
*/
typedef struct mp4d_sampleentry_audio_t_ {
    uint16_t data_reference_index;
    mp4d_fourcc_t dsi_type;
    uint64_t dsi_size;
    const unsigned char * dsi;
    mp4d_fourcc_t dsi_type_cry;
    mp4d_crypt_info_t crypt_info;
    uint64_t child_data_size;
    const unsigned char * child_data;
    uint16_t channelcount;
    uint16_t samplesize;
    uint32_t samplerate;
    uint32_t qtflags;
    uint16_t sound_version;    
    uint32_t packetsize;     /**< Only available for streaming case, derive from manifest */
    uint32_t bitrate;        /**< Only available for streaming case, derive from manifest */
    uint32_t timescale;
} mp4d_sampleentry_audio_t;

/**
    @brief XML Metadata Sample Entry
*/
typedef struct mp4d_sampleentry_xmlmetadata_t_ {
    uint16_t data_reference_index;
    const unsigned char * content_encoding;
    const unsigned char * xml_namespace;
    const unsigned char * schema_location;
} mp4d_sampleentry_xmlmetadata_t;

/**
    @brief Subtitle data Sample Entry
*/
typedef struct mp4d_sampleentry_subtdata_t_ {
    uint16_t data_reference_index;
    const unsigned char * subt_namespace;
    const unsigned char * schema_location;
    const unsigned char * image_mime_type;
} mp4d_sampleentry_subtdata_t;

/**
    @brief Sample Entry
*/
typedef union mp4d_sampleentry_t_ {
    mp4d_sampleentry_visual_t       vide;
    mp4d_sampleentry_audio_t        soun;
    mp4d_sampleentry_xmlmetadata_t  meta;
    mp4d_sampleentry_subtdata_t  subt;
} mp4d_sampleentry_t;


/**
    @brief File/Segment Type Information
*/
typedef struct mp4d_ftyp_info_t_ {
    mp4d_fourcc_t         major_brand;
    uint32_t              minor_version;
    uint32_t              num_compat_brands;
    const unsigned char * compat_brands;
} mp4d_ftyp_info_t;


/**
    @brief Progressive Download Information
*/
typedef struct mp4d_pdin_info_t_ {
    uint32_t rate;
    uint32_t initial_delay;
} mp4d_pdin_info_t;


/**
    @brief Ultraviolet Base Location Information
*/
typedef struct mp4d_bloc_info_t_ {
    const unsigned char * base_location;
    uint32_t base_location_size;
    const unsigned char * purchase_location;
    uint32_t purchase_location_size;
    const unsigned char * reserved;
    uint32_t reserved_size;
} mp4d_bloc_info_t;


/**
 * @brief ID3v2 tag
 */
typedef struct mp4d_id3v2_tag_t_ {
    const unsigned char * p_data;  /**< pointer to ID3v2 tag in buffer */
    uint64_t size;                 /**< size of the ID3v2 tag */
    uint16_t lang;                 /**< language code from the ID32 box */
} mp4d_id3v2_tag_t;


#define MP4D_MDTYPE_CFMD 0x63666D64         /**< UltraViolet metadata */
#define MP4D_MDTYPE_AINF 0x61696E66         /**< UltraViolet asset information */

#define MP4D_MDTYPE_MDIR 0x6D646972         /**< iTunes metadata */

#define MP4D_MDTYPE_DLBT 0x646C6274         /**< Dolby metadata. (Dolby Tags) */
#define MP4D_MDTYPE_DLBF 0x646C6266         /**< Dolby metadata. (Dolby Media File Metrics) */
#define MP4D_MDTYPE_DLBK 0x646C626B         /**< Dolby metadata. (Dolby Audio Kernel Parameters) */
#define MP4D_MDTYPE_DLBM 0x646C626D         /**< Dolby metadata. (Dolby Media Metadata) */

#define MP4D_MDTYPE_3GP_TITL 0x7469746C     /**< 3GP asset information. ('titl') */
#define MP4D_MDTYPE_3GP_DSCP 0x64736370     /**< 3GP asset information. ('dscp') */
#define MP4D_MDTYPE_3GP_CPRT 0x64736370     /**< 3GP asset information. ('cprt') */
#define MP4D_MDTYPE_3GP_PERF 0x70657266     /**< 3GP asset information. ('perf') */
#define MP4D_MDTYPE_3GP_AUTH 0x61757468     /**< 3GP asset information. ('auth') */
#define MP4D_MDTYPE_3GP_GNRE 0x676E7265     /**< 3GP asset information. ('gnre') */
#define MP4D_MDTYPE_3GP_RTNG 0x72746E67     /**< 3GP asset information. ('rtng') */
#define MP4D_MDTYPE_3GP_CLSF 0x636C7366     /**< 3GP asset information. ('clsf') */
#define MP4D_MDTYPE_3GP_KYWD 0x6B797764     /**< 3GP asset information. ('kywd') */
#define MP4D_MDTYPE_3GP_LOCI 0x6C6F6369     /**< 3GP asset information. ('loci') */
#define MP4D_MDTYPE_3GP_ALBM 0x616C626D     /**< 3GP asset information. ('albm') */
#define MP4D_MDTYPE_3GP_YRRC 0x79727263     /**< 3GP asset information. ('yrrc') */
#define MP4D_MDTYPE_3GP_COLL 0x636F6C6C     /**< 3GP asset information. ('coll') */
#define MP4D_MDTYPE_3GP_URAT 0x75726174     /**< 3GP asset information. ('urat') */
#define MP4D_MDTYPE_3GP_THMB 0x74686D62     /**< 3GP asset information. ('thmb') */

#ifdef __cplusplus
}
#endif

#endif  /* MP4D_TYPES_H */
/** @} */
