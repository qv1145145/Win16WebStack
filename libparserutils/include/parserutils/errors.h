/*
 * This file is part of LibParserUtils.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef parserutils_errors_h_
#define parserutils_errors_h_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>

/* Win16 DLL export macro */
#ifndef PARSERUTILS_API
#if defined(_WIN16)
#define PARSERUTILS_API __far __pascal __export
#else
#define PARSERUTILS_API
#endif
#endif

typedef enum parserutils_error {
        PARSERUTILS_OK               = 0,

        PARSERUTILS_NOMEM            = 1,
        PARSERUTILS_BADPARM          = 2,
        PARSERUTILS_INVALID          = 3,
        PARSERUTILS_FILENOTFOUND     = 4,
        PARSERUTILS_NEEDDATA         = 5,
        PARSERUTILS_BADENCODING      = 6,
        PARSERUTILS_EOF              = 7
} parserutils_error;

/* Convert a parserutils error value to a string */
const char * PARSERUTILS_API parserutils_error_to_string(parserutils_error error);
/* Convert a string to a parserutils error value */
parserutils_error PARSERUTILS_API parserutils_error_from_string(const char *str, size_t len);

#ifdef __cplusplus
}
#endif

#endif
