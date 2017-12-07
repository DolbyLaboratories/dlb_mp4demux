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
/*
 * - Error handling macros for functions which might allocate resources temporarily,
 * and which define a cleanup label and define an "int err = 0;" variable.
 *
 * - Logging functionality
 */

#ifndef UTIL_H
#define UTIL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#ifdef _MSC_VER
#define PRIu8 "u"
#define PRIu32 "I32u"
#define PRId64 "I64d"
#define PRIu64 "I64u"
#define PRIz "Iu"
#define snprintf sprintf_s
#else
#include <inttypes.h>
#define PRIz "zu"
#endif

#ifndef NDEBUG
#define DPRINTF(msg) printf msg
#define WARNING(msg) do { printf("WARNING: "); printf msg; } while(0)
#else
#define DPRINTF(msg) (void) 0
#define WARNING(msg) (void) 0
#endif

enum {
    LOG_VERBOSE_LVL_COMPACT = 0,
    LOG_VERBOSE_LVL_INFO,
    LOG_VERBOSE_LVL_DEBUG,
};

/**
  @brief Goto cleanup if the given error expression does not evaluate
  to zero.
*/
#define CHECK(err_expr)                                     \
do {                                                        \
    err = (err_expr);                                       \
    if (err != 0)                                           \
    {                                                       \
        goto cleanup;                                       \
    }                                                       \
} while(0)

/**
   @brief Print the given error message and goto cleanup with an
   error code of 1 if the given expression evaluates to zero (false).
 */
#define ASSURE(expr, msg)                                      \
do {                                                           \
    int assure_expr = (expr);                                  \
    if (!assure_expr)                                          \
    {                                                          \
        err = 1;                                               \
        fprintf(stderr, "FAILED: %s: %d: %s\n", __FILE__, __LINE__, #expr); \
        printf("[DEMUX]: ERROR: "); printf msg; printf("\n");               \
        goto cleanup;                                          \
    }                                                          \
} while(0)

extern const char DIRECTORY_SEPARATOR;
extern int g_verbose_level;

/**
 * @brief print to standard error
 */
void warning(const char *format, ...);

/**
   Make it possible to filter out debug messages
   by prefixing demuxer logs with "DEMUX:"
 */
void logout(int level, const char *format, ...);

/** @brief duplicate string. Result must be freed by user.
 * @return NULL on allocation failure
 */
char *string_dup(const char *);

#ifdef __cplusplus
}
#endif

#endif
