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

#include "md_sink.h"
#include "mp4d_nav.h"

#ifdef _MSC_VER
#define PRId32 "ld"
#define PRIu32 "I32u"
#define PRId64 "I64d"
#define PRIu64 "I64u"
#define PRIu8 "u"
#define snprintf sprintf_s  /* snprintf is C99 */
#else
#include <inttypes.h>
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

typedef struct metadata_writer_t_
{
    FILE *out_file;
    
} metadata_writer_t;


static int
md_dump_string
    (FILE * fp
    ,const unsigned char * buf
    ,uint64_t size
    )
{
    const char *str = (const char *) buf;
    size_t len = strlen(str);

    if (len > size)
    {
        len = (size_t) size;
    }
    return (int)(fwrite(str, len, 1, fp));
}

static int
md_dump_hex
    (FILE * fp
    ,const unsigned char * buf
    ,uint64_t size
    )
{
    const char *c = (const char *) buf;
    while (size) {
        fprintf(fp, "%02x", *c);
        c++;
        size--;
    }
    return 0;
}


int
md_write_id32
    (mp4d_id3v2_tag_t *p_data
    ,FILE *p_out
    )
{
    mp4d_buffer_t p = {p_data->p_data, p_data->size, p_data->p_data};
    char string[17];
    char lang_str[4] = {'\0'};

    lang_str[0] = ((p_data->lang>>10) & 0x1f) + 0x60;
    lang_str[1] = ((p_data->lang>>5) & 0x1f) + 0x60;
    lang_str[2] = ((p_data->lang) & 0x1f) + 0x60;

    fprintf(p_out, "<mp4d_id3v2_metadata_tag language=\"%s\">\n", lang_str);

    while (mp4d_bytes_left(&p)) {
        int i;
        memset(string, 0, sizeof(string));
        for (i=0; i<16; i++) {
            uint8_t c = mp4d_read_u8(&p);
            if (mp4d_is_buffer_error(&p))
                break;
            if (c>=32 && c<128)
                string[i] = (char)c;
            else
                string[i] = '.';

            fprintf(p_out, "%02x ", c);
        }
        for (; i<16; i++) {
            fprintf(p_out, "   ");
        }
        fprintf(p_out, "  %s\n", string);
    }

    fprintf(p_out, "</mp4d_id3v2_metadata_tag>\n");

    return 0;
}

/**
    @brief Parse the iTunes DataAtom
*/
static int
md_write_itunes_data
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav
    )
{
    metadata_writer_t * p_md = (metadata_writer_t*) p_nav->p_data;
    mp4d_buffer_t p = mp4d_atom_to_buffer(&atom);
    uint32_t data_type;
    
    data_type = mp4d_read_u32(&p);
    mp4d_read_u32(&p);
    
    switch (data_type) {
        case 1:  /* UTF-8 */
            fprintf(p_md->out_file, "<string><![CDATA[");
            md_dump_string (p_md->out_file, p.p_data, p.size);
            fprintf(p_md->out_file, "]]></string>\n");
            break;
        case 21: /* signed integer */
            switch (p.size) {
                case 1:
                    fprintf(p_md->out_file, "<integer size=\"%u\">%d</integer>\n", 8, (int8_t)mp4d_read_u8(&p));
                    break;
                case 2:
                    fprintf(p_md->out_file, "<integer size=\"%u\">%d</integer>\n", 16, (int16_t)mp4d_read_u16(&p));
                    break;
                case 4:
                    fprintf(p_md->out_file, "<integer size=\"%u\">%d</integer>\n", 32, (int32_t)mp4d_read_u32(&p));
                    break;
                case 8:
                    fprintf(p_md->out_file, "<integer size=\"%u\">%" PRId64 "</integer>\n", 64, (int64_t)mp4d_read_u64(&p));
                    break;
                default:
                    break;
            }
            break;
        default:
            fprintf(p_md->out_file, "<data format=\"%u\">", data_type);
            md_dump_hex(p_md->out_file, p.p_data, p.size);
            fprintf(p_md->out_file, "</data format>\n");
            break;
    }
    
    return 0;
}


static const char * k_itunes_tags[] = {
    "\251alb", "Album Name",
    "\251ART", "Artist",
    "\251cmt", "User Comment",
    "covr", "Cover Art",
    "cprt", "Copyright",
    "\251day", "Release Date",
    "\251enc", "Encoded By",
    "gnre", "Pre-defined Genre",
    "\251gen", "User Genre",
    "\251nam", "Song Name",
    "\251st3", "Track Sub-Title",
    "\251too", "Encoding Tool",
    "\251wrt", "Composer",
    "aART", "Album Artist",
    "cpil", "Disc Compilation",
    "disk", "Disc Number",
    "grup", "Grouping",
    "rtng", "Content Rating",
    "tmpo", "Beats Per Minute",
    "trkn", "Track Number",
    NULL, NULL
};

/**
    @brief Parse the iTunes MetaItemAtom
*/
static int
md_write_itunes_atom
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav
    )
{
    metadata_writer_t * p_md = (metadata_writer_t*) p_nav->p_data;
    const char** id = k_itunes_tags;
    const char* type_str = NULL;
    char* name = NULL;
    char type_4cc[6] = {'\0'};

    if (MP4D_FOURCC_EQ(atom.type, "----")) {
        mp4d_atom_t meaning_atom;
        mp4d_atom_t name_atom;
        uint64_t size;
        mp4d_error_t err;

        /* find 'mean' atom */
        err = mp4d_parse_atom_header(atom.p_data, atom.size, &meaning_atom);
        if (err) return err;
        if (!(MP4D_FOURCC_EQ(atom.type, "mean") && meaning_atom.size > 4))
            return MP4D_E_INVALID_ATOM;
        size = meaning_atom.size - 4 + 1;
        atom.p_data += (meaning_atom.size + meaning_atom.header);
        atom.size   -= (meaning_atom.size + meaning_atom.header);

        /* find 'name' atom */
        err = mp4d_parse_atom_header(atom.p_data, atom.size, &name_atom);
        if (err) return err;
        if (MP4D_FOURCC_EQ(atom.type, "name") && name_atom.size > 4) {
            size += name_atom.size - 4;
            atom.p_data += (name_atom.size + name_atom.header);
            atom.size   -= (name_atom.size + name_atom.header);
        }
        else {
            name_atom.size = 0;
        }

        /* get full name */
        name = calloc((size_t) size, 1);
        if (name == NULL)
        {
            return 1;
        }
        memcpy(name, meaning_atom.p_data + 4, (size_t) (meaning_atom.size - 4));
        if (name_atom.size) {
            strncat(name, (char *) name_atom.p_data+4, (size_t) (name_atom.size - 4));
        }
        type_str = name;
    }
    
    while (id) {
        if (MP4D_FOURCC_EQ(atom.type, *id)) {
            type_str = *(id + 1);
            break;
        }
        id += 2;
    }

    if (!type_str) {
        type_str = type_4cc;
        memcpy(type_4cc, atom.type, 4);
    }

    if (type_str) {
        fprintf(p_md->out_file, "<mp4d_itunes_metadata_item type=\"%s\">\n", type_str);
        mp4d_parse_box(atom, p_nav);
        fprintf(p_md->out_file, "</mp4d_itunes_metadata_item>\n");
    }
    
    return 0;
}


static int
md_write_ilst
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav
    )
{
    metadata_writer_t * p_md = (metadata_writer_t*) p_nav->p_data;
    fprintf(p_md->out_file, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n");
    fprintf(p_md->out_file, "<mp4d_itunes_metadata>\n");
    mp4d_parse_box(atom, p_nav);
    fprintf(p_md->out_file, "</mp4d_itunes_metadata>\n");
    return 0;
}
static int
md_write_ainf
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav
    )
{
    metadata_writer_t * p_md = (metadata_writer_t*) p_nav->p_data;
    mp4d_buffer_t p = mp4d_atom_to_buffer(&atom);
    int32_t profile_version;
    const unsigned char *APID;
    uint8_t version = mp4d_read_u8(&p);
    uint32_t flags = mp4d_read_u24(&p);

    fprintf(p_md->out_file, "ainf: version = %" PRIu8 "\n", version);
    fprintf(p_md->out_file, "ainf: flags = %" PRIu32 "\n", flags);
    if (version == 0)
    {
        profile_version = mp4d_read_u32(&p);
        APID = p.p_data;
        
        fprintf(p_md->out_file, "ainf: profile_version = %" PRId32 "\n", profile_version);
        fprintf(p_md->out_file, "ainf: APID = '%s'\n", APID);
    }

    return 0;
}

static int
md_write_xml
    (mp4d_atom_t atom
    ,mp4d_navigator_ptr_t p_nav
    )
{
    metadata_writer_t * p_md = (metadata_writer_t*) p_nav->p_data;
    mp4d_buffer_t p = mp4d_atom_to_buffer(&atom);
    
    mp4d_skip_bytes(&p, 4);
    
    if (mp4d_is_buffer_error(&p)) {
        return MP4D_E_INVALID_ATOM;
    }
    md_dump_string(p_md->out_file, p.p_data, p.size);
    return 0;
}


static const mp4d_callback_t k_dispatcher_metadata[] = 
{
    {"xml ", &md_write_xml},
    {"ainf", &md_write_ainf},
    {"ilst", &md_write_ilst},
    {"\251alb", &md_write_itunes_atom},
    {"\251alb", &md_write_itunes_atom},
    {"\251ART", &md_write_itunes_atom},
    {"\251cmt", &md_write_itunes_atom},
    {"covr", &md_write_itunes_atom},
    {"cprt", &md_write_itunes_atom},
    {"\251day", &md_write_itunes_atom},
    {"\251enc", &md_write_itunes_atom},
    {"gnre", &md_write_itunes_atom},
    {"\251gen", &md_write_itunes_atom},
    {"\251nam", &md_write_itunes_atom},
    {"\251st3", &md_write_itunes_atom},
    {"\251too", &md_write_itunes_atom},
    {"\251wrt", &md_write_itunes_atom},
    {"aART", &md_write_itunes_atom},
    {"cpil", &md_write_itunes_atom},
    {"disk", &md_write_itunes_atom},
    {"grup", &md_write_itunes_atom},
    {"rtng", &md_write_itunes_atom},
    {"tmpo", &md_write_itunes_atom},
    {"trkn", &md_write_itunes_atom},
    {"----", &md_write_itunes_atom},
    {"data", &md_write_itunes_data},

    {"dumy", NULL}  /* sentinel */
};    

int
metadata_write (FILE* fp, const mp4d_boxref_t * p_box)
{
    metadata_writer_t md = {0};
    struct mp4d_navigator_t_ navigator;
    mp4d_atom_t atom;
    mp4d_error_t err = 0;
    
    if (!fp)
        return -1;

    memset(&atom, 0, sizeof(atom));
    md.out_file = fp;
    
    mp4d_navigator_init(&navigator, k_dispatcher_metadata, NULL, &md);
    
    MP4D_FOURCC_ASSIGN(atom.type, p_box->type);
    atom.header = p_box->header;
    atom.size = p_box->size;
    atom.p_data = p_box->p_data;
    
    err = mp4d_dispatch(atom, &navigator);

    return 0;
}
