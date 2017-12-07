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

#include "util.h"

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifdef _MSC_VER
const char DIRECTORY_SEPARATOR = '\\';
#else
const char DIRECTORY_SEPARATOR = '/';
#endif

int g_verbose_level = LOG_VERBOSE_LVL_COMPACT;

void warning(const char *format, ...)
{
    va_list vl;

    fprintf(stderr, "WARNING: ");
    va_start(vl, format);
    vfprintf(stderr, format, vl);
    va_end(vl);
}

void logout(int level, const char *format, ...)
{
    static int is_newline = 1;
    va_list vl;

    if (level > g_verbose_level)
    {
        return;
    }

    if (is_newline)
    {
        printf("[DEMUX]: ");
    }
    is_newline = format[strlen(format) - 1] == '\n';

    va_start(vl, format);
    vprintf(format, vl);
    va_end(vl);
}

char *string_dup(const char *s)
{
    size_t n = strlen(s);
    char *t = malloc(n + 1);
    if (t != NULL)
    {
        strcpy(t, s);
    }
        return t;
}
