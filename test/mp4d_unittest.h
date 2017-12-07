/******************************************************************************
 * This program is protected under international and U.S. copyright laws as
 * an unpublished work. This program is confidential and proprietary to the
 * copyright owners. Reproduction or disclosure, in whole or in part, or the
 * production of derivative works therefrom without the express permission of
 * the copyright owners is prohibited.
 *
 *                Copyright (C) 2012 by Dolby International AB.
 *                            All rights reserved.
 ******************************************************************************/

/**
 * @file 
 * @brief Light-weight unit test infrastructure
 */
#ifndef MP4D_UNITTEST_H
#define MP4D_UNITTEST_H

#define TEST_START(name) printf("%s unit tests\n", (name))
#define TEST_UPDATE(err, nfailed, ntests) do { (ntests)++; if (err) (nfailed)++; } while (0)
#define TEST_END(nfailed, ntests)                               \
do {                                                            \
    printf("%d out of %d tests failed\n", (nfailed), (ntests)); \
    return (nfailed) ? 1 : 0;                                   \
} while (0)

#endif
