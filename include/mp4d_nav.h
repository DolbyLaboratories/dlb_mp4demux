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
/** @defgroup mp4d_nav
 * @brief Navigating and parsing MP4 boxes
 * @{
 */
#ifndef MP4D_NAV_H
#define MP4D_NAV_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mp4d_types.h"
#include "mp4d_buffer.h"

/****************************************************************************
    Internal Data Types
****************************************************************************/



#define MP4D_ATOMFLAGS_IS_FINAL_BOX  (1<<0)  /**< Indicates box continues to end of file. This is true for
                                                  a 32-bit box that signals zero length */
#define MP4D_ATOMFLAGS_IS_64BIT_BOX  (1<<1)  /**< Indicates box uses 64-bit length field */


/**
 * @brief MP4 atom.
 */
typedef struct mp4d_atom_t_
{
    mp4d_fourcc_t        type;        /**< Atom type */
    uint32_t             header;      /**< Atom header size */
    uint64_t             size;        /**< Atom payload size */
    uint32_t             flags;       /**< Atom flags */
    const unsigned char *p_uuid;      /**< If this atom is a UUID atom, then this points to the uuid. Otherwise it is NULL. */
    const unsigned char *p_data;      /**< Atom payload without header, contains version and flags of full atoms */
    struct mp4d_atom_t_ *p_parent;    /**< Pointer to parent atom on stack */
} mp4d_atom_t;


typedef struct mp4d_navigator_t_ * mp4d_navigator_ptr_t;

/**
 * @brief MP4 Demuxer Callback Entry
 */
typedef struct mp4d_callback_t_ {
    const char * type;               /**< Atom type as 4-char string for regular boxes or 
                                            as 16 char array for UUID boxes. */
    int (*parser) (mp4d_atom_t atom, struct mp4d_navigator_t_ * p_nav); /**< Handler function for this atom. */
} mp4d_callback_t;


/**
 * @brief MP4 Navigator Object
 * 
 */
struct mp4d_navigator_t_ {
    const mp4d_callback_t *atom_hdlr_list;  /**< NULL-terminated table of callbacks for box parser */
    const mp4d_callback_t *uuid_hdlr_list;  /**< NULL-terminated table of callbacks for uuid box parser */
    void * p_data;                          /**< user data */
};



/**************************************************
    Functions
**************************************************/


/**
 * @brief Parse an atom header.
 * The atom header is parsed and the atom structure initialized. The atom size is
 * validated.
 * @return Error code:
 *   OK (0) for success.
 *   MP4D_E_BUFFER_TOO_SMALL - Input buffer too small to parse the atom header
 *      or smaller than the size of the atom. 
 *   MP4D_E_INVALID_ATOM - Size signaled in the atom header smaller than the 
 *      atom header itself.
 *   MP4D_E_WRONG_ARGUMENT - Function arguments obviously wrong.
 */
int
mp4d_parse_atom_header
    (const unsigned char *p_buffer  /**< [in]  Input buffer */
    ,uint64_t             size      /**< [in]  Input buffer size */
    ,mp4d_atom_t         *p_atom    /**< [out] MP4 atom */
    );

/**
 * @brief Parse a container box.
 * The atom given with the first argument is considered a container box. The function
 * goes through this box and dispatches each child atom found to its handlers.
 * @return Error code:
 *   OK (0) for success.
 *   MP4D_E_INVALID_ATOM - Parsing the atom caused an error.
 */
int
mp4d_parse_box
    (mp4d_atom_t  atom         /**< [in] MP4 atom */
    ,struct mp4d_navigator_t_ *p_nav /**< [in] Demuxer handle */
    );

/**
 * @brief Read the next atom from the buffer.
 * Consumes an atom from the given buffer. The buffer is updated and points
 * to the end of the atom if one has been found.
 * @return error code:
 *   MP4D_NO_ERROR (0) - An atom was found and the buffer updated.
 *   MP4D_E_BUFFER_TOO_SMALL - The atom found indicates a size larger than the
 *      given buffer.
 */
int
mp4d_next_atom
    (struct mp4d_buffer_t_ * p_buf      /**< [in] Buffer object of current box. If getting the next
            box was successful, the buffer is progressed by the size of that box. */
    ,mp4d_atom_t       * p_parent       /**< [in]  Parent atom as reference. */
    ,mp4d_atom_t       * p_next         /**< [out] Next atom. */
    );

/**
 * @brief Find and return child atom (one level deep)
 * Searches the given atom for a child atom of provided type.
 * @return error code
 *   MP4D_NO_ERROR (0) - The child atom was found and stored.
 *   MP4D_E_ATOM_UNKNOWN - The child atom was not found.
 *   MP4D_E_INVALID_ATOM - Parsing the parent atom caused an error.
 */
mp4d_error_t
mp4d_find_atom
    (mp4d_atom_t *atom   /**< [in] The parent atom (the containing box). Non-const because the
                                   out child contains a non-const pointer to the parent atom. */
    ,const char *type    /**< [in] The type of the atom to be searched for as string of min. 4 characters. */
    ,uint32_t occurence  /**< [in] The occurence of the atom to be found. Counting starts from 0.
                            In order to find the first occurence this must be set to 0. */
    ,mp4d_atom_t *child  /**< [out] Pointer to the child atom. */
    );


/**
 * @brief Dispatcher
 * Finds and calls the handler function from the atom handler or UUID handler
 * list of this navigator object that corresponds to the atom type.
 * For a robust operation errors from the handler functions are not propagated
 * back to the calling layer.
 * @return error code:
 *   MP4D_NO_ERROR (0) - handler found and called
 *   MP4D_E_ATOM_UNKNOWN - no handler found
 */
int
mp4d_dispatch
    (mp4d_atom_t atom            /**< [in] Atom to be dispatched */
    ,mp4d_navigator_ptr_t p_nav  /**< [in] Navigator object */
    );


/**
 * @brief Initialize the Navigator Object
 * Initializes the given navigator object with the atom handler
 * lists and the data provided. If an atom handler list is NULL
 * it will be replaced with an empty list.
 */
void
mp4d_navigator_init
    (mp4d_navigator_ptr_t p_nav
    ,const mp4d_callback_t * p_atom_hdlr_list
    ,const mp4d_callback_t * p_uuid_hdlr_list
    ,void * obj
    );

/** @brief construct a buffer from an atom
 */
mp4d_buffer_t mp4d_atom_to_buffer(const mp4d_atom_t * p_atom);

#ifdef __cplusplus
}
#endif

#endif  /* MP4D_NAV_H */
/** @} */
