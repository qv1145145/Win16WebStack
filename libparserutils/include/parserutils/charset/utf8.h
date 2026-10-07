/*
 * This file is part of LibParserUtils.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

/** \file
 * UTF-8 manipulation functions (interface).
 */

#ifndef parserutils_charset_utf8_h_
#define parserutils_charset_utf8_h_

#ifdef __cplusplus
extern "C"
{
#endif

#include <inttypes.h>

#include <parserutils/errors.h>

/* Win16 DLL export macro */
#ifndef PARSERUTILS_API
#if defined(_WIN16)
#define PARSERUTILS_API __far __pascal __export
#else
#define PARSERUTILS_API
#endif
#endif

parserutils_error PARSERUTILS_API parserutils_charset_utf8_to_ucs4(const uint8_t *s, size_t len,
                uint32_t *ucs4, size_t *clen);
parserutils_error PARSERUTILS_API parserutils_charset_utf8_from_ucs4(uint32_t ucs4, uint8_t **s,
                size_t *len);

parserutils_error PARSERUTILS_API parserutils_charset_utf8_length(const uint8_t *s, size_t max,
                size_t *len);
parserutils_error PARSERUTILS_API parserutils_charset_utf8_char_byte_length(const uint8_t *s,
                size_t *len);

parserutils_error PARSERUTILS_API parserutils_charset_utf8_prev(const uint8_t *s, uint32_t off,
                uint32_t *prevoff);
parserutils_error PARSERUTILS_API parserutils_charset_utf8_next(const uint8_t *s, uint32_t len,
                uint32_t off, uint32_t *nextoff);

parserutils_error PARSERUTILS_API parserutils_charset_utf8_next_paranoid(const uint8_t *s, 
                uint32_t len, uint32_t off, uint32_t *nextoff);

#ifdef __cplusplus
}
#endif

#endif
