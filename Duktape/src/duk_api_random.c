/*
 *  Random numbers
 */

#include "duk_internal.h"

DUK_EXTERNAL duk_double_t DUKTAPE_API duk_random(duk_hthread *thr) {
	return (duk_double_t) duk_util_get_random_double(thr);
}
