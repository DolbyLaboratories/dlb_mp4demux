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
 * @defgroup mp4d_demux
 *
 * @brief Get information from top-level MP4 boxes
 *
 * @{
 */

#ifndef MP4D_DEMUX_H
#define MP4D_DEMUX_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include "mp4d_types.h"
#include "mp4d_nav.h"

/** @brief Compile time library version
 */
#define MP4D_VERSION_MAJOR 1
#define MP4D_VERSION_MINOR 0
#define MP4D_VERSION_PATCH 1

/**
 * @brief Runtime version info
 */
typedef struct
{
    int         major;
    int         minor;
    int         patch;
    const char *text;  /* Additional version string, or NULL */
} mp4d_version_t;

/**
 * @brief Opaque declaration of pointer to mp4 demuxer object.
 */
typedef struct mp4d_demuxer_t_ *mp4d_demuxer_ptr_t;

/** @brief Get the library runtime version
 * @return (always non-NULL) pointer to version struct
 */
const mp4d_version_t *
mp4d_get_version(void);

/**
 * @brief Parse the segment in this buffer.
 *
 * The buffer is expected to hold a complete file level box. This is parsed and
 * only the basic parameters are extracted. Detailed parameters are only extracted
 * when requested.
 *
 * @return Error code:
 *     OK (0) for success.
 *     MP4D_E_BUFFER_TOO_SMALL - Buffer does not contain a complete atom
 */
int
mp4d_demuxer_parse
    (mp4d_demuxer_ptr_t   p_demuxer   /**< [in]  Pointer to the mp4 demuxer */
    ,const unsigned char *p_buffer    /**< [in]  Input buffer */
    ,uint64_t             size        /**< [in]  Input buffer size */
    ,int                  is_eof      /**< [in]  Indicates this buffer holds all data until the end of the file.
                                                 This information is needed to parse boxes indicating size=0. */
    ,uint64_t             ref_offs    /**< [in]  Set the reference offset for this buffer relative to the input file. */
    ,uint64_t            *p_box_size  /**< [out] Size of the top level box. In case of a failure this helps refilling the
                                                 input buffer with the required number of data. p_box_size==0 indicates
                                                 that the is_eof flag needs to be set for successful parsing. */
    );


/**
 * @brief Get top level box type.
 *
 * @return Error code:
 *     OK (0) for success.
 */
int
mp4d_demuxer_get_type
    (mp4d_demuxer_ptr_t  p_demuxer   /**< [in]  Pointer to the mp4 demuxer */
    ,mp4d_fourcc_t      *p_type      /**< [out] Top level box type */
    );

/**
 * @brief Get movie info.
 *
 * @return Error code:
 *     OK (0) for success.
 */
int
mp4d_demuxer_get_movie_info
    (mp4d_demuxer_ptr_t     p_demuxer    /**< [in]  Pointer to the mp4 demuxer */
    ,mp4d_movie_info_t *p_movie_info     /**< [out] Movie info */
    );

/**
 * @brief Get stream info.
 *
 * @return Error code:
 *     OK (0) for success.
 */
int
mp4d_demuxer_get_stream_info
    (mp4d_demuxer_ptr_t  p_demuxer      /**< [in]  Pointer to the mp4 demuxer */
    ,uint32_t            stream_num     /**< [in]  The stream_num is not the 'track_id'! It is the xth track found in the 'moov'. Counting starts with 0! */
    ,mp4d_stream_info_t *p_stream_info  /**< [out] Stream info */
    );

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
    (mp4d_demuxer_ptr_t  p_demuxer                 /**< [in]  Pointer to the mp4 demuxer */
    ,uint32_t            stream_num                /**< [in]  The stream_num is not the 'track_id'! It is the xth track found in the 'moov'. Counting starts with 0! */
    ,uint32_t            sample_description_index  /**< [in]  Index of the DSI in the sample description table. Counting starts with 1! */
    ,mp4d_sampleentry_t *p_sampleentry             /**< [out] Sample entry including the DSI */
    );



/**
 * @brief Return the ftyp/styp information.
 *
 * @return Error code:
 *     OK (0) for success.
 *     MP4D_E_INFO_NOT_AVAIL - Information is not in this segment.
 */
int
mp4d_demuxer_get_ftyp_info
    (mp4d_demuxer_ptr_t  p_demuxer     /**< [in]  Pointer to the mp4 demuxer */
    ,mp4d_ftyp_info_t   *p_ftyp_info   /**< [out] 'ftyp' or 'styp' information struct */
    );


/**
 * @brief Return UltraViolet bloc information.
 *
 * @return Error code:
 *     OK (0) for success.
 *     MP4D_E_INFO_NOT_AVAIL - Information is not in this segment.
 */
int
mp4d_demuxer_get_bloc_info
    (mp4d_demuxer_ptr_t  p_demuxer     /**< [in]  Pointer to the mp4 demuxer */
    ,mp4d_bloc_info_t   *p_bloc_info   /**< [out] 'bloc' information struct */
    );
    
/**
 * @brief Return a lower and an upper progressive download information entry.
 *
 * The progressive download information box provides initial delays for the playback when a progressive download
 * is running at a specific download rate. Use this function to determine the closest lower and an upper entries
 * for a given rate. If the given rate is larger than all the entries in the file, the function provides the two
 * closest lower rates that can be used for extrapolation. Similar if the given rate is lower than all entries,
 * the two closest entries with a higher rate are provided. If there are less than two entries in the 'pdin' box,
 * than the closest entry is provided. A missing upper entry is initialized as rate=UINT32_MAX and initial_delay=0.
 * A missing lower entry is initialized as rate=0 and initial_delay=UINT32_MAX.
 *
 * @return Error code:
 *     OK (0) for success.
 *     MP4D_E_INFO_NOT_AVAIL - Information is not in this segment.
 */
int
mp4d_demuxer_get_pdin_pair
    (mp4d_demuxer_ptr_t  p_demuxer     /**< [in]  Pointer to the mp4 demuxer */
    ,uint32_t            req_rate      /**< [in]  The requested download rate. */
    ,mp4d_pdin_info_t   *p_lower       /**< [out] The closest lower entry. */
    ,mp4d_pdin_info_t   *p_upper       /**< [out] The closest upper entry. */
    );


/** @brief Read metadata, from the top-level meta box, or from moov:meta
 */
int
mp4d_demuxer_get_metadata
    (mp4d_demuxer_ptr_t    p_dmux
    ,uint32_t              md_type
    ,mp4d_boxref_t        *p_box
    );

/** @brief Get an item referenced by an Item Location (iloc) Box
 *
 * @note Only non-fragmented items (extent_count = 1) are supported. Only items store
 *       in the idat box of the same meta box are supported.
 *
 * @return Error code:
 *        MP4D_NO_ERROR for succes,
 *        MP4D_E_INFO_NOT_AVAIL - Current atom is not moov nor meta, or no iloc
 *                                box was found, or no idat box was found, or the
 *                                item with the requested item_ID was not found
 *        MP4D_E_UNSUPPRTED_FORMAT - Item is fragmented, or data reference is 
 *                                   not 'this file', or iloc construction_method
 *                                   is not 1.
 */
int
mp4d_demuxer_get_meta_item
    (mp4d_demuxer_ptr_t p_dmux     /** Demuxer handle */
     ,uint16_t item_ID             /** Requested item ID (as defined in the iloc box) */
     ,const unsigned char **p_item /**< [out] Pointer to the beginning of the item (in the current meta box) */
     ,uint64_t *p_size             /**< [out] Item size in bytes */
    );

/** @brief Get the ID3v2 tag from a ID32 box
 *
 *  A file can contain multiple ID32 boxes. Each of the boxes contains one ID3v2 tag in one language.
 *  Use this function to get access to the tag of the ID32 box specified by its index.
 *
 *  @return Error code
 *     OK (0) - Box found and tag successfully extracted. Output is valid.
 *     MP4D_E_IDX_OUT_OF_RANGE - The index given is larger than the index of the last ID32 box in the file.
 *     MP4D_E_INFO_NOT_AVAIL - Information is not in this top-level atom, or the 'meta' box
 *                             did not contain ID32 boxes.
 */
int
mp4d_demuxer_get_id3v2_tag
    (mp4d_demuxer_ptr_t    p_dmux    /**< [in]  Pointer to the mp4 demuxer */
    ,uint32_t              idx       /**< [in]  Index of the requested ID32 box. Set to zero for the first one. */
    ,mp4d_id3v2_tag_t     *p_tag     /**< [out] Structure describing the ID3v2 tag found. */
    );


/** @brief Read movie random acces info (mfro box)
 *
 * See also mp4d_demuxer_fragment_for_time() 
 *
 * @return error code
 */
int
mp4d_demuxer_read_mfro 
    (const unsigned char *buffer  /**< input buffer which ends at the end of file */
    ,uint64_t size                /**< input buffer size, must be >= 16 bytes */
    ,uint64_t *p_mfra_size        /**< [out] The distance from beginning of the 'mfra' box to the end of the file,
                                             or zero to indicate that no mfro box was found */
    );

/** @brief Get the moof offset for a given seek time
 *
 *  The information is taken from the mfra box. To locate the mfra box,
 *  see also mp4d_demuxer_read_mfro().
 *
 *  @return Error code
 */
int
mp4d_demuxer_fragment_for_time
    (const unsigned char *mfra_buffer /**< buffer which contains the full 'mfra' box (including the initial box header) */
    ,uint64_t mfra_size               /**< buffer size (size of mfra header + payload) */
    ,uint32_t track_ID
    ,uint64_t media_time
    ,uint64_t *p_pos                  /**< [out] Offset the latest moof before media_time,
                                           or zero if no moof box is before the given media_time */
    ,uint64_t *p_time                 /**< [out] start time (media time scale) of the fragment
                                            that starts at p_pos */
    );

/** @brief Get sidx entry
 *
 * @return error code     MP4D_E_IDX_OUT_OF_RANGE:  entry_index exceeds the number of available entries
 */
int
mp4d_demuxer_get_sidx_entry
    (mp4d_demuxer_ptr_t              /**< [in] demuxer handle, initialized with sidx box */
    ,uint32_t entry_index            /**< [in] requested entry index, from zero */
    ,uint64_t *p_offset              /**< [out] referenced box offset. The offset
                                                is relative to the anchor point for the sidx box */
    ,uint32_t *p_size                /**< [out] referenced box size */
    ,uint64_t *p_time                /**< [out] referenced box start time (media time scale) */
);

/** @brief Get the moof/sidx box offset for a given seek time
 *
 *  The demuxer handle must have been initialized with a sidx box.
 *
 *  Note: The entries of the sidx box point to moof boxes, or to other sidx boxes.
 *
 *  @return Error code
 */
int
mp4d_demuxer_get_sidx_offset
    (mp4d_demuxer_ptr_t           /**< [in] demuxer handle, initialized with sidx box */
    ,uint64_t media_time          /**< [in] requested seek presentation time (media time scale)
                                            since the beginning of the presentation */
    ,uint64_t *p_time             /**< [out] start presentation time (media time scale, since
                                             the beginning of the presentation) of the moof/sidx box
                                             pointed to by p_pos */
    ,uint64_t *p_offset           /**< [out] offset of the latest moof box (or sidx box in the case
                                             that reference_type=1) before media_time. The offset
                                             is relative to the anchor point for the sidx box */
    ,uint64_t *p_size             /**< [out] size in bytes of the moof/sidx box (including headers and payload) */
    ,uint32_t *p_index            /**< [out] sidx entry number, from zero */
    );

/** @brief Get the current top-level atom 
 *
 *  @return Error code
 */
int
mp4d_demuxer_get_atom
    (const mp4d_demuxer_ptr_t p_dmux
     ,mp4d_atom_t *p_atom              /**< [out] */
    );
    
/**************************************************
    Instantiate
**************************************************/

/**
 * @brief Return the memory needed by the mp4 demuxer instance.
 *
 * @return Error code:
 *     OK (0) for success.
 */
int
mp4d_demuxer_query_mem
    (uint64_t *p_static_mem_size   /**< [out] Static memory size */
    ,uint64_t *p_dynamic_mem_size  /**< [out] Dynamic memory size */
    );

/**
 * @brief Initialize the mp4 demuxer instance.
 *
 * @return Error code:
 *     OK (0) for success.
 */
int
mp4d_demuxer_init
    (mp4d_demuxer_ptr_t *p_demuxer_ptr,  /**< [out] Pointer to mp4 demuxer pointer */
    void                *p_static_mem,   /**< [in]  Static memory */
    void                *p_dynamic_mem   /**< [in]  Dynamic memory */
    );

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
}
#endif

#endif  /* MP4D_DEMUX_H */

/** @} */
