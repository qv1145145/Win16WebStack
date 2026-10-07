/*
 * This file is part of LibParserUtils.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef parserutils_charset_mibenum_h_
#define parserutils_charset_mibenum_h_

#ifdef __cplusplus
extern "C"
{
#endif

#include <inttypes.h>
#include <stdbool.h>

#include <parserutils/errors.h>
#include <parserutils/functypes.h>

/* Win16 DLL export macro */
#ifndef PARSERUTILS_API
#if defined(_WIN16)
#define PARSERUTILS_API __far __pascal __export
#else
#define PARSERUTILS_API
#endif
#endif

/* Convert an encoding alias to a MIB enum value */
uint16_t PARSERUTILS_API parserutils_charset_mibenum_from_name(const char *alias, size_t len);
/* Convert a MIB enum value into an encoding alias */
const char * PARSERUTILS_API parserutils_charset_mibenum_to_name(uint16_t mibenum);
/* Determine if a MIB enum value represents a Unicode variant */
bool PARSERUTILS_API parserutils_charset_mibenum_is_unicode(uint16_t mibenum);

#ifdef __cplusplus
}
#endif

#endif
