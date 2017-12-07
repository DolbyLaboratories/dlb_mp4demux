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
#include "file_stream.h"

#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _MSC_VER0
#include <Windows.h>
#endif

static
int file_seek(FILE *stream,
              uint64_t offset   /* offset relative to beginning of file (i.e. SEEK_SET is implied) */
    )
{
#ifdef _MSC_VER
        return _fseeki64(stream, offset, SEEK_SET);
#else  /* posix */
        return fseeko(stream, offset, SEEK_SET);
#endif
}

/** @brief file_stream implementation of the mp4_source API */
typedef struct file_stream_
{
    struct fragment_reader_t_ base;

    FILE* infile;               /* Current MP4 source (owned, not a reference) */
    const char *path;           /* Of infile */

    unsigned char *inbuf;
    size_t inbuf_size;   /* bytes allocated */
    uint64_t inbuf_fill;   /* bytes used */
    size_t inbuf_rpos;   /* first byte used */
    size_t buffer_granularity;
    uint64_t file_offs;  /* file position corresponding to inbuf_rpos */
    uint64_t atom_file_offs;
    int is_eof;
    mp4d_ftyp_info_t ftyp;
    unsigned char *compat_brands;

} *file_stream_t;

static const size_t SOURCE_BUFFER_SIZE = 2*1024*200;
static const size_t SOURCE_BUFFER_GRANULARITY = 1024;

/** @brief Parses mfra box and returns seek point
 *
 *  If the input file does not end with an mfra box,
 *  zero is returned as the box_offset.
 */

static int
get_mfra_seek_point(FILE *infile,
                    uint32_t track_ID,      /**< of track to consider */
                    uint64_t seek_time,     /**< in media time scale */
                    uint64_t *box_offset,   /**< [out] offset of latest moof, not after the seek_time according to mfra */
                    uint64_t *box_time      /**< [out] box start time (media time scale) */
    )
{
    int err = 0;
    uint64_t file_size;
    uint64_t mfra_size;
    unsigned char mfro_buffer[16];
    unsigned char *mfra_buffer = NULL;

    ASSURE( fseek(infile, 0, SEEK_END) == 0, ("Failed to seek to input file end") );

    {
        int64_t offset;
#ifdef _MSC_VER
        offset = _ftelli64(infile);
#else  /* posix */
        offset = ftello(infile);
#endif
        ASSURE( offset >= 0, ("Failed to get input file size") );

        file_size = (uint64_t) offset;
    }

    if (file_size < 16)
    {
        /* no mfro */
        *box_offset = 0;
        *box_time = 0;
        return 0;
    }

    ASSURE( file_seek(infile, file_size - 16) == 0, ("Failed to seek to end of input minus 16 bytes") );
    ASSURE( fread(mfro_buffer, 16, 1, infile) == 1, ("Failed to read last 16 bytes of input file") );

    if (mp4d_demuxer_read_mfro(mfro_buffer,
                               16,
                               &mfra_size) != MP4D_NO_ERROR || mfra_size == 0)
    {
        *box_offset = 0;
        *box_time = 0;
        return 0;
    }

    /* mfro was found.
       If the last box is an mfra, then expect the box header to start at file_size - mfra_size
    */
    ASSURE( file_seek(infile, file_size - mfra_size) == 0,
            ("Failed to seek to end of input minus %" PRIu64 " bytes (potential mfra box size)",
             mfra_size) );

    ASSURE( (size_t) mfra_size == mfra_size, ("mfra atom is too big (size = %" PRIu64")", mfra_size) );
    mfra_buffer = malloc((size_t) mfra_size * sizeof(*mfra_buffer));

    ASSURE( mfra_buffer != NULL, ("Allocation failure") );

    ASSURE( fread(mfra_buffer, (size_t) mfra_size, 1, infile) == 1, ("Failed to read mfra atom of size %" PRIu64, mfra_size) );

    CHECK( mp4d_demuxer_fragment_for_time(mfra_buffer,
                                          mfra_size,
                                          track_ID,
                                          seek_time,
                                          box_offset,
                                          box_time) );

cleanup:
    free(mfra_buffer);
    return err;
}

int
file_stream_seek_sidx(fragment_reader_t s,
                      uint64_t seek_time,
                      uint64_t segment_start,
                      uint64_t *out_time
                      )
{
    int err = 0;
    mp4d_fourcc_t type;
    uint64_t offset;
    file_stream_t fs = (file_stream_t) s;

    /* Rewind */
    fs->inbuf_fill = 0;
    fs->inbuf_rpos = 0;
    fs->file_offs = 0;
    fs->atom_file_offs = 0;

    /* Find sidx, must be before moof */
    do
    {
        int err_next = fragment_reader_next_atom(&fs->base);

        ASSURE( err_next == 0 || err_next == 2, ("Unexpected error %d when getting next atom", err_next) );

        /* If EOF and no sidx nor moof found */
        if (err_next == 2)
        {
            /* ... then rewind and move to moov (if any) */
            fs->inbuf_fill = 0;
            fs->inbuf_rpos = 0;
            fs->file_offs = 0;
            fs->atom_file_offs = 0;

            do
            {
                CHECK( fragment_reader_next_atom(&fs->base) );
                CHECK( mp4d_demuxer_get_type(fs->base.p_dmux, &type) );
            } while (!(MP4D_FOURCC_EQ(type, "moov")));
            /* Done */
            *out_time = segment_start;
            goto cleanup;
        }
        CHECK( mp4d_demuxer_get_type(fs->base.p_dmux, &type) );
    } while (!(MP4D_FOURCC_EQ(type, "moof") || MP4D_FOURCC_EQ(type, "sidx")));

    if (MP4D_FOURCC_EQ(type, "moof"))
    {
        /* no sidx */
        *out_time = segment_start;
        goto cleanup;
    }

    {
        uint64_t size;
        uint32_t indx;

        CHECK( mp4d_demuxer_get_sidx_offset(fs->base.p_dmux,
                                            seek_time,
                                            out_time,
                                            &offset,
                                            &size,
                                            &indx) );
    }

    fs->inbuf_fill = 0;
    fs->inbuf_rpos = 0;
    fs->is_eof = 0;
    fs->atom_file_offs += offset;
	
	/* offset is relative to the first byte after the sidx box, at which file_offs already points */
    fs->file_offs += offset;
    do
    {
        CHECK( fragment_reader_next_atom(&fs->base) );
        CHECK( mp4d_demuxer_get_type(fs->base.p_dmux, &type) );
     } while (!MP4D_FOURCC_EQ(type, "moov") &&
              !MP4D_FOURCC_EQ(type, "moof"));

cleanup:
    return err;
}

static int
file_stream_seek(fragment_reader_t s,
                 uint32_t track_ID,
                 uint64_t seek_time,
                 uint64_t *out_time)
{
    int err = 0;
    file_stream_t fs = (file_stream_t) s;
    uint64_t offset;
    mp4d_fourcc_t type;

    CHECK( get_mfra_seek_point(fs->infile,
                               track_ID,
                               seek_time,
                               &offset,
                               out_time) );

    fs->inbuf_fill = 0;
    fs->inbuf_rpos = 0;
    fs->is_eof = 0;
    fs->file_offs = offset;
    fs->atom_file_offs = offset;

    do
    {
        CHECK( fragment_reader_next_atom(s) );
        CHECK( mp4d_demuxer_get_type(s->p_dmux, &type) );
    } while (!MP4D_FOURCC_EQ(type, "moov") &&
             !MP4D_FOURCC_EQ(type, "moof"));
cleanup:
    return err;
}
static int
file_stream_load(fragment_reader_t s, uint64_t position, uint32_t size, unsigned char *p_buffer)
{
    file_stream_t fs = (file_stream_t) s;
    int err = 0;

    ASSURE( file_seek(fs->infile, position) == 0, ("fseek to %" PRIu64 " on input file failed", position) );
    ASSURE( fread(p_buffer, size, 1, fs->infile) == 1, ("Reading %" PRIu32 " bytes from input @%" PRIu64 " failed", size, position) );

cleanup:
    return err;
}

static void
file_stream_destroy(fragment_reader_t s)
{
    file_stream_t fs = (file_stream_t) s;

    if (fs != NULL)
    {
        if (fs->infile != NULL)
        {
            fclose(fs->infile);
        }
        free(fs->inbuf);
        free(fs->compat_brands);

        fragment_reader_deinit(s);
        free(s);
    }
}

#ifdef MEASURE_PERFORMANCE
#include <windows.h>
#define DebugString(...) { TCHAR temp[256]; _stprintf_s(temp,256,__VA_ARGS__); OutputDebugString(temp); };
#include <tchar.h>
#endif

/* just keep the old version of this function for several following check-in */
static int
file_stream_next_atom(fragment_reader_t s)
{
    file_stream_t fs = (file_stream_t) s;
    size_t bytes_read;
    int is_eof;
    uint64_t atom_size = 8;
    mp4d_error_t rv;
    int err = 0;

    if (fs->inbuf_rpos) {
        if (fs->inbuf_fill > fs->inbuf_rpos) {
            fs->inbuf_fill -= (size_t) fs->inbuf_rpos;
            memmove(fs->inbuf, fs->inbuf + fs->inbuf_rpos, (size_t)fs->inbuf_fill);
        }
        else {
            fs->inbuf_fill = 0;
        }
        fs->inbuf_rpos = 0;
    }

    ASSURE( file_seek(fs->infile, fs->file_offs + fs->inbuf_fill) == 0, ("file seek to %" PRIu64 " failed!", fs->file_offs + fs->inbuf_fill) );

    do {
        mp4d_fourcc_t type;

        if (atom_size > fs->inbuf_size) {
            /* Safe to cast uint64_t -> size_t because the atom cannot be larger than the input buffer size */
            fs->inbuf_size = ((size_t)atom_size + fs->buffer_granularity - 1);
            fs->inbuf_size -= (fs->inbuf_size % fs->buffer_granularity);
            fs->inbuf = realloc (fs->inbuf, fs->inbuf_size);
            ASSURE( fs->inbuf != NULL, ("Failed to allocate %" PRIz " bytes", fs->inbuf_size) );
        }

        bytes_read = fread(fs->inbuf + fs->inbuf_fill, 1, fs->inbuf_size - fs->inbuf_fill, fs->infile);

        fs->inbuf_fill += bytes_read;
        is_eof = feof(fs->infile);
        ASSURE( is_eof || bytes_read > 0, ("Failed to read input file") );

        rv = mp4d_demuxer_parse(s->p_dmux, fs->inbuf, fs->inbuf_fill, is_eof, fs->file_offs, &atom_size);

        if (rv == MP4D_E_BUFFER_TOO_SMALL) {
            CHECK( mp4d_demuxer_get_type(s->p_dmux, &type) );
            if (MP4D_FOURCC_EQ(type, "mdat") ||
                MP4D_FOURCC_EQ(type, "free") ||
                MP4D_FOURCC_EQ(type, "skip"))
            {
                fs->inbuf_rpos = atom_size;
                fs->file_offs += atom_size;

                return 0;
            }
        }

    } while (rv == MP4D_E_BUFFER_TOO_SMALL && !is_eof);

    if (rv)
    {
        return is_eof ? 2 : 1;
    }

    fs->inbuf_rpos = (size_t) atom_size;
    fs->file_offs += atom_size;

cleanup:
    return err;
}

static int
file_stream_get_offset(fragment_reader_t s, uint64_t *offset)
{
    int err = 0;
    file_stream_t fs = (file_stream_t) s;
    mp4d_atom_t atom;

    ASSURE( offset != NULL, ("Null input") );
    /* file_offs points to next atom */
    CHECK( mp4d_demuxer_get_atom(s->p_dmux, &atom) );
    *offset = fs->file_offs - atom.header - atom.size;

cleanup:
    return err;
}

static int
file_stream_get_type(fragment_reader_t s, mp4d_ftyp_info_t *p_type)
{
    int err = 0;
    file_stream_t fs = (file_stream_t) s;

    ASSURE( fs->compat_brands != NULL, ("No ftyp atom found, cannot get file type") );
    *p_type = fs->ftyp;

cleanup:
    return err;
}

int file_stream_new(fragment_reader_t *p_s,  /**< [out] */
                            const char *path)
{
    int err = 0;
    file_stream_t fs = malloc(sizeof *fs);
    fragment_reader_t s;

    ASSURE( fs != NULL, ("Allocation error") );
    s = &fs->base;
    CHECK( fragment_reader_init(s) );

    fs->buffer_granularity = SOURCE_BUFFER_GRANULARITY;
    fs->inbuf_size = SOURCE_BUFFER_SIZE;
    fs->inbuf = malloc(fs->inbuf_size);
    ASSURE( fs->inbuf != NULL, ("Allocation error") );
    fs->inbuf_fill = 0;
    fs->inbuf_rpos = 0;

#ifdef _MSC_VER0
    {
        wchar_t *wfilename;
        errno_t ret;
        int wlen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, NULL, 0);
        if (wlen == 0)
        {
            free(fs->inbuf);
            return 1;
        }
        wfilename = malloc(sizeof(wchar_t) * wlen);
        if (wfilename == NULL)
        {
            free(fs->inbuf);
            return 1;
        }
        MultiByteToWideChar(CP_UTF8, 0, path, -1, wfilename, wlen);
        ret = _wfopen_s(&fs->infile, wfilename, L"rb");
        free(wfilename);
    }
#else
    fs->infile = fopen(path, "rb");
#endif
    ASSURE( fs->infile != NULL, ("Failed to open input file '%s'", path) );
    fs->path = path;
    fs->file_offs = 0;
    fs->atom_file_offs = 0;
    fs->is_eof = 0;
    fs->ftyp.num_compat_brands = 0;
    fs->compat_brands = NULL;

    s->next_atom = file_stream_next_atom;
    s->seek = file_stream_seek;
    s->destroy = file_stream_destroy;
    s->load = file_stream_load;
    s->get_offset = file_stream_get_offset;
    s->get_type = file_stream_get_type;

    {
        mp4d_atom_t atom;

        int err_next = fragment_reader_next_atom(s);

        ASSURE( err_next != 2, ("Found no boxes in %s", path) );
        ASSURE( err_next == 0, ("Unexpected error %d when reading first box from %s", err_next, path) );
        CHECK( mp4d_demuxer_get_atom(s->p_dmux, &atom) );
        if (MP4D_FOURCC_EQ(atom.type, "ftyp") )
        {
            CHECK( mp4d_demuxer_get_ftyp_info(s->p_dmux, &fs->ftyp) );

            /* Copy the compatible_brands memory */
            fs->compat_brands = (unsigned char*)malloc(4 * fs->ftyp.num_compat_brands);
            ASSURE( fs->compat_brands != NULL, ("malloc failure") );
            memcpy(fs->compat_brands, fs->ftyp.compat_brands, 4 * fs->ftyp.num_compat_brands);

            fs->ftyp.compat_brands = fs->compat_brands;
        }
        /* support for Quicktime files 
           Note: this is unfortunate that we cannot be more specific (than 'else'), so we just hope it's Quicktime and
           the demuxer doesn't fail later on. */
        else
        {
            /* Copy the compatible_brands memory */
            fs->ftyp.num_compat_brands = 1;
            fs->compat_brands = (unsigned char*)malloc(4);
            
            ASSURE( fs->compat_brands != NULL, ("malloc failure") );
            memcpy(fs->compat_brands, "qt  ", 4);

            fs->ftyp.compat_brands = fs->compat_brands;
            memcpy(fs->ftyp.major_brand, fs->ftyp.compat_brands, 4);
            fs->ftyp.minor_version = 0; 
            
            logout(LOG_VERBOSE_LVL_INFO,
            "major_brand = %c%c%c%c\n",
            fs->ftyp.major_brand[0],
            fs->ftyp.major_brand[1],
            fs->ftyp.major_brand[2],
            fs->ftyp.major_brand[3]);
        }

        /* rewind */
        fs->inbuf_fill = 0;
        fs->inbuf_rpos = 0;
        fs->is_eof = 0;
        fs->file_offs = 0;
        fs->atom_file_offs = 0;
    }

    *p_s = s;
cleanup:
    return err;
}
