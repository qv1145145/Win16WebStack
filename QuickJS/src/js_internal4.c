#include "js_internal.h"



JSValue JS_ToNumberHintFree(JSContext *ctx, JSValue val,
                                   JSToNumberHintEnum flag)
{
    uint32_t tag;
    JSValue ret;

 redo:
    tag = JS_VALUE_GET_NORM_TAG(val);
    switch(tag) {
    case JS_TAG_BIG_INT:
    case JS_TAG_SHORT_BIG_INT:
        if (flag != TON_FLAG_NUMERIC) {
            JS_FreeValue(ctx, val);
            return JS_ThrowTypeError(ctx, "cannot convert bigint to number");
        }
        ret = val;
        break;
    case JS_TAG_FLOAT64:
    case JS_TAG_INT:
    case JS_TAG_EXCEPTION:
        ret = val;
        break;
    case JS_TAG_BOOL:
    case JS_TAG_NULL:
        ret = JS_NewInt32(ctx, JS_VALUE_GET_INT(val));
        break;
    case JS_TAG_UNDEFINED:
        ret = JS_NAN;
        break;
    case JS_TAG_OBJECT:
        val = JS_ToPrimitiveFree(ctx, val, HINT_NUMBER);
        if (JS_IsException(val))
            return JS_EXCEPTION;
        goto redo;
    case JS_TAG_STRING:
    case JS_TAG_STRING_ROPE:
        {
            const char *str;
            const char *p;
            size_t len;

            str = JS_ToCStringLen(ctx, &len, val);
            JS_FreeValue(ctx, val);
            if (!str)
                return JS_EXCEPTION;
            p = str;
            p += skip_spaces(p);
            if ((p - str) == len) {
                ret = JS_NewInt32(ctx, 0);
            } else {
                int flags = ATOD_ACCEPT_BIN_OCT;
                ret = js_atof(ctx, p, &p, 0, flags);
                if (!JS_IsException(ret)) {
                    p += skip_spaces(p);
                    if ((p - str) != len) {
                        JS_FreeValue(ctx, ret);
                        ret = JS_NAN;
                    }
                }
            }
            JS_FreeCString(ctx, str);
        }
        break;
    case JS_TAG_SYMBOL:
        JS_FreeValue(ctx, val);
        return JS_ThrowTypeError(ctx, "cannot convert symbol to number");
    default:
        JS_FreeValue(ctx, val);
        ret = JS_NAN;
        break;
    }
    return ret;
}

JSValue JS_ToNumberFree(JSContext *ctx, JSValue val)
{
    return JS_ToNumberHintFree(ctx, val, TON_FLAG_NUMBER);
}

JSValue JS_ToNumericFree(JSContext *ctx, JSValue val)
{
    return JS_ToNumberHintFree(ctx, val, TON_FLAG_NUMERIC);
}

JSValue JS_ToNumeric(JSContext *ctx, JSValueConst val)
{
    return JS_ToNumericFree(ctx, JS_DupValue(ctx, val));
}

__exception int __JS_ToFloat64Free(JSContext *ctx, double *pres,
                                          JSValue val)
{
    double d;
    uint32_t tag;

    val = JS_ToNumberFree(ctx, val);
    if (JS_IsException(val))
        goto fail;
    tag = JS_VALUE_GET_NORM_TAG(val);
    switch(tag) {
    case JS_TAG_INT:
        d = JS_VALUE_GET_INT(val);
        break;
    case JS_TAG_FLOAT64:
        d = JS_VALUE_GET_FLOAT64(val);
        break;
    default:
        abort();
    }
    *pres = d;
    return 0;
 fail:
    *pres = JS_FLOAT64_NAN;
    return -1;
}

JSValue JS_ToNumber(JSContext *ctx, JSValueConst val)
{
    return JS_ToNumberFree(ctx, JS_DupValue(ctx, val));
}

/* same as JS_ToNumber() but return 0 in case of NaN/Undefined */
__maybe_unused JSValue JS_ToIntegerFree(JSContext *ctx, JSValue val)
{
    uint32_t tag;
    JSValue ret;

 redo:
    tag = JS_VALUE_GET_NORM_TAG(val);
    switch(tag) {
    case JS_TAG_INT:
    case JS_TAG_BOOL:
    case JS_TAG_NULL:
    case JS_TAG_UNDEFINED:
        ret = JS_NewInt32(ctx, JS_VALUE_GET_INT(val));
        break;
    case JS_TAG_FLOAT64:
        {
            double d = JS_VALUE_GET_FLOAT64(val);
            if (isnan(d)) {
                ret = JS_NewInt32(ctx, 0);
            } else {
                /* convert -0 to +0 */
                d = trunc(d) + 0.0;
                ret = JS_NewFloat64(ctx, d);
            }
        }
        break;
    default:
        val = JS_ToNumberFree(ctx, val);
        if (JS_IsException(val))
            return val;
        goto redo;
    }
    return ret;
}

/* Note: the integer value is satured to 32 bits */
int JS_ToInt32SatFree(JSContext *ctx, int *pres, JSValue val)
{
    uint32_t tag;
    int ret;

 redo:
    tag = JS_VALUE_GET_NORM_TAG(val);
    switch(tag) {
    case JS_TAG_INT:
    case JS_TAG_BOOL:
    case JS_TAG_NULL:
    case JS_TAG_UNDEFINED:
        ret = JS_VALUE_GET_INT(val);
        break;
    case JS_TAG_EXCEPTION:
        *pres = 0;
        return -1;
    case JS_TAG_FLOAT64:
        {
            double d = JS_VALUE_GET_FLOAT64(val);
            if (isnan(d)) {
                ret = 0;
            } else {
                if (d < INT32_MIN)
                    ret = INT32_MIN;
                else if (d > INT32_MAX)
                    ret = INT32_MAX;
                else
                    ret = (int)d;
            }
        }
        break;
    default:
        val = JS_ToNumberFree(ctx, val);
        if (JS_IsException(val)) {
            *pres = 0;
            return -1;
        }
        goto redo;
    }
    *pres = ret;
    return 0;
}

int JS_ToInt32Sat(JSContext *ctx, int *pres, JSValueConst val)
{
    return JS_ToInt32SatFree(ctx, pres, JS_DupValue(ctx, val));
}

int JS_ToInt32Clamp(JSContext *ctx, int *pres, JSValueConst val,
                    int min, int max, int min_offset)
{
    int res = JS_ToInt32SatFree(ctx, pres, JS_DupValue(ctx, val));
    if (res == 0) {
        if (*pres < min) {
            *pres += min_offset;
            if (*pres < min)
                *pres = min;
        } else {
            if (*pres > max)
                *pres = max;
        }
    }
    return res;
}

int JS_ToInt64SatFree(JSContext *ctx, int64_t *pres, JSValue val)
{
    uint32_t tag;

 redo:
    tag = JS_VALUE_GET_NORM_TAG(val);
    switch(tag) {
    case JS_TAG_INT:
    case JS_TAG_BOOL:
    case JS_TAG_NULL:
    case JS_TAG_UNDEFINED:
        *pres = JS_VALUE_GET_INT(val);
        return 0;
    case JS_TAG_EXCEPTION:
        *pres = 0;
        return -1;
    case JS_TAG_FLOAT64:
        {
            double d = JS_VALUE_GET_FLOAT64(val);
            if (isnan(d)) {
                *pres = 0;
            } else {
                if (d < INT64_MIN)
                    *pres = INT64_MIN;
                else if (d >= 0x1p63) /* must use INT64_MAX + 1 because INT64_MAX cannot be exactly represented as a double */
                    *pres = INT64_MAX;
                else
                    *pres = (int64_t)d;
            }
        }
        return 0;
    default:
        val = JS_ToNumberFree(ctx, val);
        if (JS_IsException(val)) {
            *pres = 0;
            return -1;
        }
        goto redo;
    }
}

int JS_ToInt64Sat(JSContext *ctx, int64_t *pres, JSValueConst val)
{
    return JS_ToInt64SatFree(ctx, pres, JS_DupValue(ctx, val));
}

int JS_ToInt64Clamp(JSContext *ctx, int64_t *pres, JSValueConst val,
                    int64_t min, int64_t max, int64_t neg_offset)
{
    int res = JS_ToInt64SatFree(ctx, pres, JS_DupValue(ctx, val));
    if (res == 0) {
        if (*pres < 0)
            *pres += neg_offset;
        if (*pres < min)
            *pres = min;
        else if (*pres > max)
            *pres = max;
    }
    return res;
}

/* Same as JS_ToInt32Free() but with a 64 bit result. Return (<0, 0)
   in case of exception */
int JS_ToInt64Free(JSContext *ctx, int64_t *pres, JSValue val)
{
    uint32_t tag;
    int64_t ret;

 redo:
    tag = JS_VALUE_GET_NORM_TAG(val);
    switch(tag) {
    case JS_TAG_INT:
    case JS_TAG_BOOL:
    case JS_TAG_NULL:
    case JS_TAG_UNDEFINED:
        ret = JS_VALUE_GET_INT(val);
        break;
    case JS_TAG_FLOAT64:
        {
            JSFloat64Union u;
            double d;
            int e;
            d = JS_VALUE_GET_FLOAT64(val);
            u.d = d;
            /* we avoid doing fmod(x, 2^64) */
            e = (u.u64 >> 52) & 0x7ff;
            if (likely(e <= (1023 + 62))) {
                /* fast case */
                ret = (int64_t)d;
            } else if (e <= (1023 + 62 + 53)) {
                uint64_t v;
                /* remainder modulo 2^64 */
                v = (u.u64 & (((uint64_t)1 << 52) - 1)) | ((uint64_t)1 << 52);
                ret = v << ((e - 1023) - 52);
                /* take the sign into account */
                if (u.u64 >> 63)
                    ret = -ret;
            } else {
                ret = 0; /* also handles NaN and +inf */
            }
        }
        break;
    default:
        val = JS_ToNumberFree(ctx, val);
        if (JS_IsException(val)) {
            *pres = 0;
            return -1;
        }
        goto redo;
    }
    *pres = ret;
    return 0;
}

/* return (<0, 0) in case of exception */
int JS_ToInt32Free(JSContext *ctx, int32_t *pres, JSValue val)
{
    uint32_t tag;
    int32_t ret;

 redo:
    tag = JS_VALUE_GET_NORM_TAG(val);
    switch(tag) {
    case JS_TAG_INT:
    case JS_TAG_BOOL:
    case JS_TAG_NULL:
    case JS_TAG_UNDEFINED:
        ret = JS_VALUE_GET_INT(val);
        break;
    case JS_TAG_FLOAT64:
        {
            JSFloat64Union u;
            double d;
            int e;
            d = JS_VALUE_GET_FLOAT64(val);
            u.d = d;
            /* we avoid doing fmod(x, 2^32) */
            e = (u.u64 >> 52) & 0x7ff;
            if (likely(e <= (1023 + 30))) {
                /* fast case */
                ret = (int32_t)d;
            } else if (e <= (1023 + 30 + 53)) {
                uint64_t v;
                /* remainder modulo 2^32 */
                v = (u.u64 & (((uint64_t)1 << 52) - 1)) | ((uint64_t)1 << 52);
                v = v << ((e - 1023) - 52 + 32);
                ret = v >> 32;
                /* take the sign into account */
                if (u.u64 >> 63)
                    ret = -ret;
            } else {
                ret = 0; /* also handles NaN and +inf */
            }
        }
        break;
    default:
        val = JS_ToNumberFree(ctx, val);
        if (JS_IsException(val)) {
            *pres = 0;
            return -1;
        }
        goto redo;
    }
    *pres = ret;
    return 0;
}

int JS_ToUint8ClampFree(JSContext *ctx, int32_t *pres, JSValue val)
{
    uint32_t tag;
    int res;

 redo:
    tag = JS_VALUE_GET_NORM_TAG(val);
    switch(tag) {
    case JS_TAG_INT:
    case JS_TAG_BOOL:
    case JS_TAG_NULL:
    case JS_TAG_UNDEFINED:
        res = JS_VALUE_GET_INT(val);
        res = max_int(0, min_int(255, res));
        break;
    case JS_TAG_FLOAT64:
        {
            double d = JS_VALUE_GET_FLOAT64(val);
            if (isnan(d)) {
                res = 0;
            } else {
                if (d < 0)
                    res = 0;
                else if (d > 255)
                    res = 255;
                else
                    res = lrint(d);
            }
        }
        break;
    default:
        val = JS_ToNumberFree(ctx, val);
        if (JS_IsException(val)) {
            *pres = 0;
            return -1;
        }
        goto redo;
    }
    *pres = res;
    return 0;
}

__exception int JS_ToArrayLengthFree(JSContext *ctx, uint32_t *plen,
                                            JSValue val, BOOL is_array_ctor)
{
    uint32_t tag, len;

    tag = JS_VALUE_GET_TAG(val);
    switch(tag) {
    case JS_TAG_INT:
    case JS_TAG_BOOL:
    case JS_TAG_NULL:
        {
            int v;
            v = JS_VALUE_GET_INT(val);
            if (v < 0)
                goto fail;
            len = v;
        }
        break;
    default:
        if (JS_TAG_IS_FLOAT64(tag)) {
            double d;
            d = JS_VALUE_GET_FLOAT64(val);
            if (!(d >= 0 && d <= UINT32_MAX))
                goto fail;
            len = (uint32_t)d;
            if (len != d)
                goto fail;
        } else {
            uint32_t len1;

            if (is_array_ctor) {
                val = JS_ToNumberFree(ctx, val);
                if (JS_IsException(val))
                    return -1;
                /* cannot recurse because val is a number */
                if (JS_ToArrayLengthFree(ctx, &len, val, TRUE))
                    return -1;
            } else {
                /* legacy behavior: must do the conversion twice and compare */
                if (JS_ToUint32(ctx, &len, val)) {
                    JS_FreeValue(ctx, val);
                    return -1;
                }
                val = JS_ToNumberFree(ctx, val);
                if (JS_IsException(val))
                    return -1;
                /* cannot recurse because val is a number */
                if (JS_ToArrayLengthFree(ctx, &len1, val, FALSE))
                    return -1;
                if (len1 != len) {
                fail:
                    JS_ThrowRangeError(ctx, "invalid array length");
                    return -1;
                }
            }
        }
        break;
    }
    *plen = len;
    return 0;
}

#define MAX_SAFE_INTEGER (((int64_t)1 << 53) - 1)

BOOL is_safe_integer(double d)
{
    return isfinite(d) && floor(d) == d &&
        fabs(d) <= (double)MAX_SAFE_INTEGER;
}

/* convert a value to a length between 0 and MAX_SAFE_INTEGER.
   return -1 for exception */
__exception int JS_ToLengthFree(JSContext *ctx, int64_t *plen,
                                       JSValue val)
{
    int res = JS_ToInt64Clamp(ctx, plen, val, 0, MAX_SAFE_INTEGER, 0);
    JS_FreeValue(ctx, val);
    return res;
}

/* Note: can return an exception */
int JS_NumberIsInteger(JSContext *ctx, JSValueConst val)
{
    double d;
    if (!JS_IsNumber(val))
        return FALSE;
    if (unlikely(JS_ToFloat64(ctx, &d, val)))
        return -1;
    return isfinite(d) && floor(d) == d;
}

BOOL JS_NumberIsNegativeOrMinusZero(JSContext *ctx, JSValueConst val)
{
    uint32_t tag;

    tag = JS_VALUE_GET_NORM_TAG(val);
    switch(tag) {
    case JS_TAG_INT:
        {
            int v;
            v = JS_VALUE_GET_INT(val);
            return (v < 0);
        }
    case JS_TAG_FLOAT64:
        {
            JSFloat64Union u;
            u.d = JS_VALUE_GET_FLOAT64(val);
            return (u.u64 >> 63);
        }
    case JS_TAG_SHORT_BIG_INT:
        return (JS_VALUE_GET_SHORT_BIG_INT(val) < 0);
    case JS_TAG_BIG_INT:
        {
            JSBigInt *p = JS_VALUE_GET_PTR(val);
            return js_bigint_sign(p);
        }
    default:
        return FALSE;
    }
}

JSValue js_bigint_to_string(JSContext *ctx, JSValueConst val)
{
    return js_bigint_to_string1(ctx, val, 10);
}

JSValue js_dtoa2(JSContext *ctx,
                        double d, int radix, int n_digits, int flags)
{
    char static_buf[128], *buf, *tmp_buf;
    int len, len_max;
    JSValue res;
    JSDTOATempMem dtoa_mem;
    len_max = js_dtoa_max_len(d, radix, n_digits, flags);

    /* longer buffer may be used if radix != 10 */
    if (len_max > sizeof(static_buf) - 1) {
        tmp_buf = js_malloc(ctx, len_max + 1);
        if (!tmp_buf)
            return JS_EXCEPTION;
        buf = tmp_buf;
    } else {
        tmp_buf = NULL;
        buf = static_buf;
    }
    len = js_dtoa(buf, d, radix, n_digits, flags, &dtoa_mem);
    res = js_new_string8_len(ctx, buf, len);
    js_free(ctx, tmp_buf);
    return res;
}

JSValue JS_ToStringInternal(JSContext *ctx, JSValueConst val, BOOL is_ToPropertyKey)
{
    uint32_t tag;
    char buf[32];

    tag = JS_VALUE_GET_NORM_TAG(val);
    switch(tag) {
    case JS_TAG_STRING:
        return JS_DupValue(ctx, val);
    case JS_TAG_STRING_ROPE:
        return js_linearize_string_rope(ctx, JS_DupValue(ctx, val));
    case JS_TAG_INT:
        {
            size_t len;
            len = i32toa(buf, JS_VALUE_GET_INT(val));
            return js_new_string8_len(ctx, buf, len);
        }
        break;
    case JS_TAG_BOOL:
        return JS_AtomToString(ctx, JS_VALUE_GET_BOOL(val) ?
                          JS_ATOM_true : JS_ATOM_false);
    case JS_TAG_NULL:
        return JS_AtomToString(ctx, JS_ATOM_null);
    case JS_TAG_UNDEFINED:
        return JS_AtomToString(ctx, JS_ATOM_undefined);
    case JS_TAG_EXCEPTION:
        return JS_EXCEPTION;
    case JS_TAG_OBJECT:
        {
            JSValue val1, ret;
            val1 = JS_ToPrimitive(ctx, val, HINT_STRING);
            if (JS_IsException(val1))
                return val1;
            ret = JS_ToStringInternal(ctx, val1, is_ToPropertyKey);
            JS_FreeValue(ctx, val1);
            return ret;
        }
        break;
    case JS_TAG_FUNCTION_BYTECODE:
        return js_new_string8(ctx, "[function bytecode]");
    case JS_TAG_SYMBOL:
        if (is_ToPropertyKey) {
            return JS_DupValue(ctx, val);
        } else {
            return JS_ThrowTypeError(ctx, "cannot convert symbol to string");
        }
    case JS_TAG_FLOAT64:
        return js_dtoa2(ctx, JS_VALUE_GET_FLOAT64(val), 10, 0,
                        JS_DTOA_FORMAT_FREE);
    case JS_TAG_SHORT_BIG_INT:
    case JS_TAG_BIG_INT:
        return js_bigint_to_string(ctx, val);
    default:
        return js_new_string8(ctx, "[unsupported type]");
    }
}

JSValue JS_ToStringFree(JSContext *ctx, JSValue val)
{
    JSValue ret;
    ret = JS_ToString(ctx, val);
    JS_FreeValue(ctx, val);
    return ret;
}

JSValue JS_ToLocaleStringFree(JSContext *ctx, JSValue val)
{
    if (JS_IsUndefined(val) || JS_IsNull(val))
        return JS_ToStringFree(ctx, val);
    return JS_InvokeFree(ctx, val, JS_ATOM_toLocaleString, 0, NULL);
}

JSValue JS_ToStringCheckObject(JSContext *ctx, JSValueConst val)
{
    uint32_t tag = JS_VALUE_GET_TAG(val);
    if (tag == JS_TAG_NULL || tag == JS_TAG_UNDEFINED)
        return JS_ThrowTypeError(ctx, "null or undefined are forbidden");
    return JS_ToString(ctx, val);
}

void js_print_value(JSPrintValueState *s, JSValueConst val);

void js_putc(JSPrintValueState *s, char c)
{
    s->write_func(s->write_opaque, &c, 1);
}

void js_puts(JSPrintValueState *s, const char *str)
{
    s->write_func(s->write_opaque, str, strlen(str));
}

void __attribute__((format(printf, 2, 3))) js_printf(JSPrintValueState *s, const char *fmt, ...)
{
    va_list ap;
    char buf[256];

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    s->write_func(s->write_opaque, buf, strlen(buf));
}

void js_print_float64(JSPrintValueState *s, double d)
{
    JSDTOATempMem dtoa_mem;
    char buf[32];
    int len;
    len = js_dtoa(buf, d, 10, 0, JS_DTOA_FORMAT_FREE | JS_DTOA_MINUS_ZERO, &dtoa_mem);
    s->write_func(s->write_opaque, buf, len);
}

uint32_t js_string_get_length(JSValueConst val)
{
    if (JS_VALUE_GET_TAG(val) == JS_TAG_STRING) {
        JSString *p = JS_VALUE_GET_STRING(val);
        return p->len;
    } else if (JS_VALUE_GET_TAG(val) == JS_TAG_STRING_ROPE) {
        JSStringRope *r = JS_VALUE_GET_PTR(val);
        return r->len;
    } else {
        return 0;
    }
}

/* pretty print the first 'len' characters of 'p' */
void js_print_string1(JSPrintValueState *s, JSString *p, int len, int sep)
{
    uint8_t buf[UTF8_CHAR_LEN_MAX];
    int l, i, c, c1;

    for(i = 0; i < len; i++) {
        c = string_get(p, i);
        switch(c) {
        case '\t':
            c = 't';
            goto quote;
        case '\r':
            c = 'r';
            goto quote;
        case '\n':
            c = 'n';
            goto quote;
        case '\b':
            c = 'b';
            goto quote;
        case '\f':
            c = 'f';
            goto quote;
        case '\\':
        quote:
            js_putc(s, '\\');
            js_putc(s, c);
            break;
        default:
            if (c == sep)
                goto quote;
            if (c >= 32 && c <= 126) {
                js_putc(s, c);
            } else if (c < 32 ||
                       (c >= 0x7f && c <= 0x9f)) {
            escape:
                js_printf(s, "\\u%04x", c);
            } else {
                if (is_hi_surrogate(c)) {
                    if ((i + 1) >= len)
                        goto escape;
                    c1 = string_get(p, i + 1);
                    if (!is_lo_surrogate(c1))
                        goto escape;
                    i++;
                    c = from_surrogate(c, c1);
                } else if (is_lo_surrogate(c)) {
                    goto escape;
                }
                l = unicode_to_utf8(buf, c);
                s->write_func(s->write_opaque, (char *)buf, l);
            }
            break;
        }
    }
}

void js_print_string_rec(JSPrintValueState *s, JSValueConst val,
                                int sep, uint32_t pos)
{
    if (JS_VALUE_GET_TAG(val) == JS_TAG_STRING) {
        JSString *p = JS_VALUE_GET_STRING(val);
        uint32_t len;
        if (pos < s->options.max_string_length) {
            len = min_uint32(p->len, s->options.max_string_length - pos);
            js_print_string1(s, p, len, sep);
        }
    } else if (JS_VALUE_GET_TAG(val) == JS_TAG_STRING_ROPE) {
        JSStringRope *r = JS_VALUE_GET_PTR(val);
        js_print_string_rec(s, r->left, sep, pos);
        js_print_string_rec(s, r->right, sep, pos + js_string_get_length(r->left));
    } else {
        js_printf(s, "<invalid string tag %d>", (int)JS_VALUE_GET_TAG(val));
    }
}

void js_print_string(JSPrintValueState *s, JSValueConst val)
{
    int sep;
    if (s->options.raw_dump && JS_VALUE_GET_TAG(val) == JS_TAG_STRING) {
        JSString *p = JS_VALUE_GET_STRING(val);
        js_printf(s, "%d", js_rc(p)->ref_count);
        sep = (js_rc(p)->ref_count == 1) ? '\"' : '\'';
    } else {
        sep = '\"';
    }
    js_putc(s, sep);
    js_print_string_rec(s, val, sep, 0);
    js_putc(s, sep);
    if (js_string_get_length(val) > s->options.max_string_length) {
        uint32_t n = js_string_get_length(val) - s->options.max_string_length;
        js_printf(s, "... %u more character%s", n, n > 1 ? "s" : "");
    }
}

void js_print_raw_string(JSPrintValueState *s, JSValueConst val)
{
    const char *cstr;
    size_t len;
    cstr = JS_ToCStringLen(s->ctx, &len, val);
    if (cstr) {
        s->write_func(s->write_opaque, cstr, len);
        JS_FreeCString(s->ctx, cstr);
    }
}

BOOL is_ascii_ident(const JSString *p)
{
    int i, c;

    if (p->len == 0)
        return FALSE;
    for(i = 0; i < p->len; i++) {
        c = string_get(p, i);
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c == '_' || c == '$') || (c >= '0' && c <= '9' && i > 0)))
            return FALSE;
    }
    return TRUE;
}

void js_print_atom(JSPrintValueState *s, JSAtom atom)
{
    int i;
    if (__JS_AtomIsTaggedInt(atom)) {
        js_printf(s, "%u", __JS_AtomToUInt32(atom));
    } else if (atom == JS_ATOM_NULL) {
        js_puts(s, "<null>");
    } else {
        assert(atom < s->rt->atom_size);
        JSString *p;
        p = s->rt->atom_array[atom];
        if (is_ascii_ident(p)) {
            for(i = 0; i < p->len; i++) {
                js_putc(s, string_get(p, i));
            }
        } else {
            js_putc(s, '"');
            js_print_string1(s, p, p->len, '\"');
            js_putc(s, '"');
        }
    }
}

/* return 0 if invalid length */
uint32_t js_print_array_get_length(JSObject *p)
{
    JSProperty *pr;
    JSShapeProperty *prs;
    JSValueConst val;

    prs = find_own_property(&pr, p, JS_ATOM_length);
    if (!prs)
        return 0;
    if ((prs->flags & JS_PROP_TMASK) != JS_PROP_NORMAL)
        return 0;
    val = pr->u.value;
    switch(JS_VALUE_GET_NORM_TAG(val)) {
    case JS_TAG_INT:
        return JS_VALUE_GET_INT(val);
    case JS_TAG_FLOAT64:
        return (uint32_t)JS_VALUE_GET_FLOAT64(val);
    default:
        return 0;
    }
}

void js_print_comma(JSPrintValueState *s, int *pcomma_state)
{
    switch(*pcomma_state) {
    case 0:
        break;
    case 1:
        js_printf(s, ", ");
        break;
    case 2:
        js_printf(s, " { ");
        break;
    }
    *pcomma_state = 1;
}

void js_print_more_items(JSPrintValueState *s, int *pcomma_state,
                                uint32_t n)
{
    js_print_comma(s, pcomma_state);
    js_printf(s, "... %u more item%s", n, n > 1 ? "s" : "");
}

/* similar to js_regexp_toString() but without side effect */
void js_print_regexp(JSPrintValueState *s, JSObject *p1)
{
    JSRegExp *re = &p1->u.regexp;
    JSString *p;
    int i, n, c, c2, bra, flags;
    const char regexp_flags[] = { 'g', 'i', 'm', 's', 'u', 'y', 'd', 'v' };

    if (!re->pattern || !re->bytecode) {
        /* the regexp fields are zeroed at init */
        js_puts(s, "[uninitialized_regexp]");
        return;
    }
    p = re->pattern;
    js_putc(s, '/');
    if (p->len == 0) {
        js_puts(s, "(?:)");
    } else {
        bra = 0;
        for (i = 0, n = p->len; i < n;) {
            c2 = -1;
            switch (c = string_get(p, i++)) {
            case '\\':
                if (i < n)
                    c2 = string_get(p, i++);
                break;
            case ']':
                bra = 0;
                break;
            case '[':
                if (!bra) {
                    if (i < n && string_get(p, i) == ']')
                        c2 = string_get(p, i++);
                    bra = 1;
                }
                break;
            case '\n':
                c = '\\';
                c2 = 'n';
                break;
            case '\r':
                c = '\\';
                c2 = 'r';
                break;
            case '/':
                if (!bra) {
                    c = '\\';
                    c2 = '/';
                }
                break;
            }
            js_putc(s, c);
            if (c2 >= 0)
                js_putc(s, c2);
        }
    }
    js_putc(s, '/');

    flags = lre_get_flags(re->bytecode->u.str8);
    for(i = 0; i < countof(regexp_flags); i++) {
        if ((flags >> i) & 1) {
            js_putc(s, regexp_flags[i]);
        }
    }
}

/* similar to js_error_toString() but without side effect */
void js_print_error(JSPrintValueState *s, JSObject *p)
{
    const char *str;
    size_t len;

    str = get_prop_string(s->ctx, JS_MKPTR(JS_TAG_OBJECT, p), JS_ATOM_name);
    if (!str) {
        js_puts(s, "Error");
    } else {
        js_puts(s, str);
        JS_FreeCString(s->ctx, str);
    }

    str = get_prop_string(s->ctx, JS_MKPTR(JS_TAG_OBJECT, p), JS_ATOM_message);
    if (str && str[0] != '\0') {
        js_puts(s, ": ");
        js_puts(s, str);
    }
    JS_FreeCString(s->ctx, str);

    /* dump the stack if present */
    str = get_prop_string(s->ctx, JS_MKPTR(JS_TAG_OBJECT, p), JS_ATOM_stack);
    if (str) {
        js_putc(s, '\n');

        /* XXX: should remove the last '\n' in stack as
           v8. SpiderMonkey does not do it */
        len = strlen(str);
        if (len > 0 && str[len - 1] == '\n')
            len--;
        s->write_func(s->write_opaque, str, len);

        JS_FreeCString(s->ctx, str);
    }
}

void js_print_object(JSPrintValueState *s, JSObject *p)
{
    JSRuntime *rt = s->rt;
    JSShape *sh;
    JSShapeProperty *prs;
    JSProperty *pr;
    int comma_state;
    BOOL is_array;
    uint32_t i;

    comma_state = 0;
    is_array = FALSE;
    if (p->class_id == JS_CLASS_ARRAY) {
        is_array = TRUE;
        js_printf(s, "[ ");
        /* XXX: print array like properties even if not fast array */
        if (p->fast_array) {
            uint32_t len, n, len1;
            len = js_print_array_get_length(p);

            len1 = min_uint32(p->u.array.count, s->options.max_item_count);
            for(i = 0; i < len1; i++) {
                js_print_comma(s, &comma_state);
                js_print_value(s, p->u.array.u.values[i]);
            }
            if (len1 < p->u.array.count)
                js_print_more_items(s, &comma_state, p->u.array.count - len1);
            if (p->u.array.count < len) {
                n = len - p->u.array.count;
                js_print_comma(s, &comma_state);
                js_printf(s, "<%u empty item%s>", n, n > 1 ? "s" : "");
            }
        }
    } else if (p->class_id >= JS_CLASS_UINT8C_ARRAY && p->class_id <= JS_CLASS_FLOAT64_ARRAY) {
        uint32_t size = 1 << typed_array_size_log2(p->class_id);
        uint32_t len1;
        int64_t v;

        js_print_atom(s, rt->class_array[p->class_id].class_name);
        js_printf(s, "(%u) [ ", p->u.array.count);

        is_array = TRUE;
        len1 = min_uint32(p->u.array.count, s->options.max_item_count);
        for(i = 0; i < len1; i++) {
            const uint8_t *ptr = p->u.array.u.uint8_ptr + i * size;
            js_print_comma(s, &comma_state);
            switch(p->class_id) {
            case JS_CLASS_UINT8C_ARRAY:
            case JS_CLASS_UINT8_ARRAY:
                v = *ptr;
                goto ta_int64;
            case JS_CLASS_INT8_ARRAY:
                v = *(int8_t *)ptr;
                goto ta_int64;
            case JS_CLASS_INT16_ARRAY:
                v = *(int16_t *)ptr;
                goto ta_int64;
            case JS_CLASS_UINT16_ARRAY:
                v = *(uint16_t *)ptr;
                goto ta_int64;
            case JS_CLASS_INT32_ARRAY:
                v = *(int32_t *)ptr;
                goto ta_int64;
            case JS_CLASS_UINT32_ARRAY:
                v = *(uint32_t *)ptr;
                goto ta_int64;
            case JS_CLASS_BIG_INT64_ARRAY:
                v = *(int64_t *)ptr;
            ta_int64:
                js_printf(s, "%" PRId64, v);
                break;
            case JS_CLASS_BIG_UINT64_ARRAY:
                js_printf(s, "%" PRIu64, *(uint64_t *)ptr);
                break;
            case JS_CLASS_FLOAT16_ARRAY:
                js_print_float64(s, fromfp16(*(uint16_t *)ptr));
                break;
            case JS_CLASS_FLOAT32_ARRAY:
                js_print_float64(s, *(float *)ptr);
                break;
            case JS_CLASS_FLOAT64_ARRAY:
                js_print_float64(s, *(double *)ptr);
                break;
            }
        }
        if (len1 < p->u.array.count)
            js_print_more_items(s, &comma_state, p->u.array.count - len1);
    } else if (p->class_id == JS_CLASS_BYTECODE_FUNCTION ||
               (rt->class_array[p->class_id].call != NULL &&
                p->class_id != JS_CLASS_PROXY)) {
        js_printf(s, "[Function");
        /* XXX: allow dump without ctx */
        if (!s->options.raw_dump && s->ctx) {
            const char *func_name_str;
            js_putc(s, ' ');
            func_name_str = get_prop_string(s->ctx, JS_MKPTR(JS_TAG_OBJECT, p), JS_ATOM_name);
            if (!func_name_str || func_name_str[0] == '\0')
                js_puts(s, "(anonymous)");
            else
                js_puts(s, func_name_str);
            JS_FreeCString(s->ctx, func_name_str);
        }
        js_printf(s, "]");
        comma_state = 2;
    } else if (p->class_id == JS_CLASS_MAP || p->class_id == JS_CLASS_SET) {
        JSMapState *ms = p->u.opaque;
        struct list_head *el;

        if (!ms)
            goto default_obj;
        js_print_atom(s, rt->class_array[p->class_id].class_name);
        js_printf(s, "(%u) { ", ms->record_count);
        i = 0;
        list_for_each(el, &ms->records) {
            JSMapRecord *mr = list_entry(el, JSMapRecord, link);
            js_print_comma(s, &comma_state);
            if (mr->empty)
                continue;
            js_print_value(s, mr->key);
            if (p->class_id == JS_CLASS_MAP) {
                js_printf(s, " => ");
                js_print_value(s, mr->value);
            }
            i++;
            if (i >= s->options.max_item_count)
                break;
        }
        if (i < ms->record_count)
            js_print_more_items(s, &comma_state, ms->record_count - i);
    } else if (p->class_id == JS_CLASS_REGEXP && s->ctx) {
        js_print_regexp(s, p);
        comma_state = 2;
    } else if (p->class_id == JS_CLASS_DATE && s->ctx) {
        /* get_date_string() has no side effect */
        JSValue str = get_date_string(s->ctx, JS_MKPTR(JS_TAG_OBJECT, p), 0, NULL, 0x23); /* toISOString() */
        if (JS_IsException(str))
            goto default_obj;
        js_print_raw_string(s, str);
        JS_FreeValueRT(s->rt, str);
        comma_state = 2;
    } else if (p->class_id == JS_CLASS_ERROR && s->ctx) {
        js_print_error(s, p);
        comma_state = 2;
    } else {
        default_obj:
        if (p->class_id != JS_CLASS_OBJECT) {
            js_print_atom(s, rt->class_array[p->class_id].class_name);
            js_printf(s, " ");
        }
        js_printf(s, "{ ");
    }

    sh = p->shape; /* the shape can be NULL while freeing an object */
    if (sh) {
        uint32_t j;

        j = 0;
        for(i = 0, prs = get_shape_prop(sh); i < sh->prop_count; i++, prs++) {
            if (prs->atom != JS_ATOM_NULL) {
                if (!(prs->flags & JS_PROP_ENUMERABLE) &&
                    !s->options.show_hidden) {
                    continue;
                }
                if (j < s->options.max_item_count) {
                    pr = &p->prop[i];
                    js_print_comma(s, &comma_state);
                    js_print_atom(s, prs->atom);
                    js_printf(s, ": ");

                    /* XXX: autoinit property */
                    if ((prs->flags & JS_PROP_TMASK) == JS_PROP_GETSET) {
                        if (s->options.raw_dump) {
                            js_printf(s, "[Getter %p Setter %p]",
                                    pr->u.getset.getter, pr->u.getset.setter);
                        } else {
                            if (pr->u.getset.getter && pr->u.getset.setter) {
                                js_printf(s, "[Getter/Setter]");
                            } else if (pr->u.getset.setter) {
                                js_printf(s, "[Setter]");
                            } else {
                                js_printf(s, "[Getter]");
                            }
                        }
                    } else if ((prs->flags & JS_PROP_TMASK) == JS_PROP_VARREF) {
                        if (s->options.raw_dump) {
                            js_printf(s, "[varref %p]", (void *)pr->u.var_ref);
                        } else {
                            js_print_value(s, *pr->u.var_ref->pvalue);
                        }
                    } else if ((prs->flags & JS_PROP_TMASK) == JS_PROP_AUTOINIT) {
                        if (s->options.raw_dump) {
                            js_printf(s, "[autoinit %p %d %p]",
                                    (void *)js_autoinit_get_realm(pr),
                                    js_autoinit_get_id(pr),
                                    (void *)pr->u.init.opaque);
                        } else {
                            /* XXX: could autoinit but need to restart
                               the iteration */
                            js_printf(s, "[autoinit]");
                        }
                    } else {
                        js_print_value(s, pr->u.value);
                    }
                }
                j++;
            }
        }
        if (j > s->options.max_item_count)
            js_print_more_items(s, &comma_state, j - s->options.max_item_count);
    }
    if (s->options.raw_dump && js_class_has_bytecode(p->class_id)) {
        JSFunctionBytecode *b = p->u.func.function_bytecode;
        if (b->closure_var_count) {
            JSVarRef **var_refs;
            var_refs = p->u.func.var_refs;

            js_print_comma(s, &comma_state);
            js_printf(s, "[[Closure]]: [");
            for(i = 0; i < b->closure_var_count; i++) {
                if (i != 0)
                    js_printf(s, ", ");
                js_print_value(s, var_refs[i]->value);
            }
            js_printf(s, " ]");
        }
        if (p->u.func.home_object) {
            js_print_comma(s, &comma_state);
            js_printf(s, "[[HomeObject]]: ");
            js_print_value(s, JS_MKPTR(JS_TAG_OBJECT, p->u.func.home_object));
        }
    }

    if (!is_array) {
        if (comma_state != 2) {
            js_printf(s, " }");
        }
    } else {
        js_printf(s, " ]");
    }
}

int js_print_stack_index(JSPrintValueState *s, JSObject *p)
{
    int i;
    for(i = 0; i < s->level; i++)
        if (s->print_stack[i] == p)
            return i;
    return -1;
}

void js_print_value(JSPrintValueState *s, JSValueConst val)
{
    uint32_t tag = JS_VALUE_GET_NORM_TAG(val);
    const char *str;

    switch(tag) {
    case JS_TAG_INT:
        js_printf(s, "%d", JS_VALUE_GET_INT(val));
        break;
    case JS_TAG_BOOL:
        if (JS_VALUE_GET_BOOL(val))
            str = "true";
        else
            str = "false";
        goto print_str;
    case JS_TAG_NULL:
        str = "null";
        goto print_str;
    case JS_TAG_EXCEPTION:
        str = "exception";
        goto print_str;
    case JS_TAG_UNINITIALIZED:
        str = "uninitialized";
        goto print_str;
    case JS_TAG_UNDEFINED:
        str = "undefined";
    print_str:
        js_puts(s, str);
        break;
    case JS_TAG_FLOAT64:
        js_print_float64(s, JS_VALUE_GET_FLOAT64(val));
        break;
    case JS_TAG_SHORT_BIG_INT:
        js_printf(s, "%" PRId64 "n", (int64_t)JS_VALUE_GET_SHORT_BIG_INT(val));
        break;
    case JS_TAG_BIG_INT:
        if (!s->options.raw_dump && s->ctx) {
            JSValue str = js_bigint_to_string(s->ctx, val);
            if (JS_IsException(str))
                goto raw_bigint;
            js_print_raw_string(s, str);
            js_putc(s, 'n');
            JS_FreeValueRT(s->rt, str);
        } else {
            JSBigInt *p;
            int sgn, i;
        raw_bigint:
            p = JS_VALUE_GET_PTR(val);
            /* In order to avoid allocations we just dump the limbs */
            sgn = js_bigint_sign(p);
            if (sgn)
                js_printf(s, "BigInt.asIntN(%d,", p->len * JS_LIMB_BITS);
            js_printf(s, "0x");
            for(i = p->len - 1; i >= 0; i--) {
                if (i != p->len - 1)
                    js_putc(s, '_');
#if JS_LIMB_BITS == 32
                js_printf(s, "%08x", p->tab[i]);
#else
                js_printf(s, "%016" PRIx64, p->tab[i]);
#endif
            }
            js_putc(s, 'n');
            if (sgn)
                js_putc(s, ')');
        }
        break;
    case JS_TAG_STRING:
    case JS_TAG_STRING_ROPE:
        if (s->options.raw_dump && tag == JS_TAG_STRING_ROPE) {
            JSStringRope *r = JS_VALUE_GET_STRING_ROPE(val);
            js_printf(s, "[rope len=%d depth=%d]", r->len, r->depth);
        } else {
            js_print_string(s, val);
        }
        break;
    case JS_TAG_FUNCTION_BYTECODE:
        {
            JSFunctionBytecode *b = JS_VALUE_GET_PTR(val);
            js_puts(s, "[bytecode ");
            js_print_atom(s, b->func_name);
            js_putc(s, ']');
        }
        break;
    case JS_TAG_OBJECT:
        {
            JSObject *p = JS_VALUE_GET_OBJ(val);
            int idx;
            idx = js_print_stack_index(s, p);
            if (idx >= 0) {
                js_printf(s, "[circular %d]", idx);
            } else if (s->level < s->options.max_depth) {
                s->print_stack[s->level++] = p;
                js_print_object(s, JS_VALUE_GET_OBJ(val));
                s->level--;
            } else {
                JSAtom atom = s->rt->class_array[p->class_id].class_name;
                js_putc(s, '[');
                js_print_atom(s, atom);
                if (s->options.raw_dump) {
                    js_printf(s, " %p", (void *)p);
                }
                js_putc(s, ']');
            }
        }
        break;
    case JS_TAG_SYMBOL:
        {
            JSAtomStruct *p = JS_VALUE_GET_PTR(val);
            js_puts(s, "Symbol(");
            js_print_atom(s, js_get_atom_index(s->rt, p));
            js_putc(s, ')');
        }
        break;
    case JS_TAG_MODULE:
        js_puts(s, "[module]");
        break;
    default:
        js_printf(s, "[unknown tag %d]", tag);
        break;
    }
}

void JS_PrintValueInternal(JSRuntime *rt, JSContext *ctx,
                                  JSPrintValueWrite *write_func, void *write_opaque,
                                  JSValueConst val, const JSPrintValueOptions *options)
{
    JSPrintValueState ss, *s = &ss;
    if (options)
        s->options = *options;
    else
        JS_PrintValueSetDefaultOptions(&s->options);
    if (s->options.max_depth <= 0)
        s->options.max_depth = JS_PRINT_MAX_DEPTH;
    else
        s->options.max_depth = min_int(s->options.max_depth, JS_PRINT_MAX_DEPTH);
    if (s->options.max_string_length == 0)
        s->options.max_string_length = UINT32_MAX;
    if (s->options.max_item_count == 0)
        s->options.max_item_count = UINT32_MAX;
    s->rt = rt;
    s->ctx = ctx;
    s->write_func = write_func;
    s->write_opaque = write_opaque;
    s->level = 0;
    js_print_value(s, val);
}

void js_dump_value_write(void *opaque, const char *buf, size_t len)
{
    FILE *fo = opaque;
    fwrite(buf, 1, len, fo);
}

__maybe_unused void print_atom(JSContext *ctx, JSAtom atom)
{
    JSPrintValueState ss, *s = &ss;
    memset(s, 0, sizeof(*s));
    s->rt = ctx->rt;
    s->ctx = ctx;
    s->write_func = js_dump_value_write;
    s->write_opaque = stdout;
    js_print_atom(s, atom);
}

__maybe_unused void JS_DumpAtom(JSContext *ctx, const char *str, JSAtom atom)
{
    printf("%s=", str);
    print_atom(ctx, atom);
    printf("\n");
}

__maybe_unused void JS_DumpValue(JSContext *ctx, const char *str, JSValueConst val)
{
    printf("%s=", str);
    JS_PrintValue(ctx, js_dump_value_write, stdout, val, NULL);
    printf("\n");
}

__maybe_unused void JS_DumpValueRT(JSRuntime *rt, const char *str, JSValueConst val)
{
    printf("%s=", str);
    JS_PrintValueRT(rt, js_dump_value_write, stdout, val, NULL);
    printf("\n");
}

__maybe_unused void JS_DumpObjectHeader(JSRuntime *rt)
{
    printf("%14s %4s %4s %14s %s\n",
           "ADDRESS", "REFS", "SHRF", "PROTO", "CONTENT");
}

/* for debug only: dump an object without side effect */
__maybe_unused void JS_DumpObject(JSRuntime *rt, JSObject *p)
{
    JSShape *sh;
    JSPrintValueOptions options;

    /* XXX: should encode atoms with special characters */
    sh = p->shape; /* the shape can be NULL while freeing an object */
    printf("%14p %4d ",
           (void *)p,
           js_rc(p)->ref_count);
    if (sh) {
        printf("%3d%c %14p ",
               js_rc(sh)->ref_count,
               " *"[sh->is_hashed],
               (void *)sh->proto);
    } else {
        printf("%3s  %14s ", "-", "-");
    }

    JS_PrintValueSetDefaultOptions(&options);
    options.max_depth = 1;
    options.show_hidden = TRUE;
    options.raw_dump = TRUE;
    JS_PrintValueRT(rt, js_dump_value_write, stdout, JS_MKPTR(JS_TAG_OBJECT, p), &options);

    printf("\n");
}

__maybe_unused void JS_DumpGCObject(JSRuntime *rt, JSGCObjectHeader *p)
{
    if (js_rc(p)->gc_obj_type == JS_GC_OBJ_TYPE_JS_OBJECT) {
        JS_DumpObject(rt, (JSObject *)p);
    } else {
        printf("%14p %4d ",
               (void *)p,
               js_rc(p)->ref_count);
        switch(js_rc(p)->gc_obj_type) {
        case JS_GC_OBJ_TYPE_FUNCTION_BYTECODE:
            printf("[function bytecode]");
            break;
        case JS_GC_OBJ_TYPE_SHAPE:
            printf("[shape]");
            break;
        case JS_GC_OBJ_TYPE_VAR_REF:
            printf("[var_ref]");
            break;
        case JS_GC_OBJ_TYPE_ASYNC_FUNCTION:
            printf("[async_function]");
            break;
        case JS_GC_OBJ_TYPE_JS_CONTEXT:
            printf("[js_context]");
            break;
        case JS_GC_OBJ_TYPE_MODULE:
            printf("[module]");
            break;
        default:
            printf("[unknown %d]", js_rc(p)->gc_obj_type);
            break;
        }
        printf("\n");
    }
}

double js_pow(double a, double b)
{
    if (unlikely(!isfinite(b)) && fabs(a) == 1) {
        /* not compatible with IEEE 754 */
        return JS_FLOAT64_NAN;
    } else {
        return pow(a, b);
    }
}

/* return NaN if bad bigint literal */
JSValue JS_StringToBigInt(JSContext *ctx, JSValue val)
{
    const char *str, *p;
    size_t len;
    int flags;

    str = JS_ToCStringLen(ctx, &len, val);
    JS_FreeValue(ctx, val);
    if (!str)
        return JS_EXCEPTION;
    p = str;
    p += skip_spaces(p);
    if ((p - str) == len) {
        val = JS_NewBigInt64(ctx, 0);
    } else {
        flags = ATOD_INT_ONLY | ATOD_ACCEPT_BIN_OCT | ATOD_TYPE_BIG_INT;
        val = js_atof(ctx, p, &p, 0, flags);
        p += skip_spaces(p);
        if (!JS_IsException(val)) {
            if ((p - str) != len) {
                JS_FreeValue(ctx, val);
                val = JS_NAN;
            }
        }
    }
    JS_FreeCString(ctx, str);
    return val;
}

JSValue JS_StringToBigIntErr(JSContext *ctx, JSValue val)
{
    val = JS_StringToBigInt(ctx, val);
    if (JS_VALUE_IS_NAN(val))
        return JS_ThrowSyntaxError(ctx, "invalid bigint literal");
    return val;
}

/* JS Numbers are not allowed */
JSValue JS_ToBigIntFree(JSContext *ctx, JSValue val)
{
    uint32_t tag;

 redo:
    tag = JS_VALUE_GET_NORM_TAG(val);
    switch(tag) {
    case JS_TAG_SHORT_BIG_INT:
    case JS_TAG_BIG_INT:
        break;
    case JS_TAG_INT:
    case JS_TAG_NULL:
    case JS_TAG_UNDEFINED:
    case JS_TAG_FLOAT64:
        goto fail;
    case JS_TAG_BOOL:
        val = __JS_NewShortBigInt(ctx, JS_VALUE_GET_INT(val));
        break;
    case JS_TAG_STRING:
    case JS_TAG_STRING_ROPE:
        val = JS_StringToBigIntErr(ctx, val);
        if (JS_IsException(val))
            return val;
        goto redo;
    case JS_TAG_OBJECT:
        val = JS_ToPrimitiveFree(ctx, val, HINT_NUMBER);
        if (JS_IsException(val))
            return val;
        goto redo;
    default:
    fail:
        JS_FreeValue(ctx, val);
        return JS_ThrowTypeError(ctx, "cannot convert to bigint");
    }
    return val;
}

JSValue JS_ToBigInt(JSContext *ctx, JSValueConst val)
{
    return JS_ToBigIntFree(ctx, JS_DupValue(ctx, val));
}

/* XXX: merge with JS_ToInt64Free with a specific flag ? */
int JS_ToBigInt64Free(JSContext *ctx, int64_t *pres, JSValue val)
{
    uint64_t res;

    val = JS_ToBigIntFree(ctx, val);
    if (JS_IsException(val)) {
        *pres = 0;
        return -1;
    }
    if (JS_VALUE_GET_TAG(val) == JS_TAG_SHORT_BIG_INT) {
        res = JS_VALUE_GET_SHORT_BIG_INT(val);
    } else {
        JSBigInt *p = JS_VALUE_GET_PTR(val);
        /* return the value mod 2^64 */
        res = p->tab[0];
#if JS_LIMB_BITS == 32
        if (p->len >= 2)
            res |= (uint64_t)p->tab[1] << 32;
#endif
        JS_FreeValue(ctx, val);
    }
    *pres = res;
    return 0;
}

no_inline __exception int js_unary_arith_slow(JSContext *ctx,
                                                     JSValue *sp,
                                                     OPCodeEnum op)
{
    JSValue op1;
    int v;
    uint32_t tag;
    JSBigIntBuf buf1;
    JSBigInt *p1;

    op1 = sp[-1];
    /* fast path for float64 */
    if (JS_TAG_IS_FLOAT64(JS_VALUE_GET_TAG(op1)))
        goto handle_float64;
    op1 = JS_ToNumericFree(ctx, op1);
    if (JS_IsException(op1))
        goto exception;
    tag = JS_VALUE_GET_TAG(op1);
    switch(tag) {
    case JS_TAG_INT:
        {
            int64_t v64;
            v64 = JS_VALUE_GET_INT(op1);
            switch(op) {
            case OP_inc:
            case OP_dec:
                v = 2 * (op - OP_dec) - 1;
                v64 += v;
                break;
            case OP_plus:
                break;
            case OP_neg:
                if (v64 == 0) {
                    sp[-1] = __JS_NewFloat64(ctx, -0.0);
                    return 0;
                } else {
                    v64 = -v64;
                }
                break;
            default:
                abort();
            }
            sp[-1] = JS_NewInt64(ctx, v64);
        }
        break;
    case JS_TAG_SHORT_BIG_INT:
        {
            int64_t v;
            v = JS_VALUE_GET_SHORT_BIG_INT(op1);
            switch(op) {
            case OP_plus:
                JS_ThrowTypeError(ctx, "bigint argument with unary +");
                goto exception;
            case OP_inc:
                if (v == JS_SHORT_BIG_INT_MAX)
                    goto bigint_slow_case;
                sp[-1] = __JS_NewShortBigInt(ctx, v + 1);
                break;
            case OP_dec:
                if (v == JS_SHORT_BIG_INT_MIN)
                    goto bigint_slow_case;
                sp[-1] = __JS_NewShortBigInt(ctx, v - 1);
                break;
            case OP_neg:
                v = JS_VALUE_GET_SHORT_BIG_INT(op1);
                if (v == JS_SHORT_BIG_INT_MIN) {
                bigint_slow_case:
                    p1 = js_bigint_set_short(&buf1, op1);
                    goto bigint_slow_case1;
                }
                sp[-1] = __JS_NewShortBigInt(ctx, -v);
                break;
            default:
                abort();
            }
        }
        break;
    case JS_TAG_BIG_INT:
        {
            JSBigInt *r;
            p1 = JS_VALUE_GET_PTR(op1);
        bigint_slow_case1:
            switch(op) {
            case OP_plus:
                JS_ThrowTypeError(ctx, "bigint argument with unary +");
                JS_FreeValue(ctx, op1);
                goto exception;
            case OP_inc:
            case OP_dec:
                {
                    JSBigIntBuf buf2;
                    JSBigInt *p2;
                    p2 = js_bigint_set_si(&buf2, 2 * (op - OP_dec) - 1);
                    r = js_bigint_add(ctx, p1, p2, 0);
                }
                break;
            case OP_neg:
                r = js_bigint_neg(ctx, p1);
                break;
            case OP_not:
                r = js_bigint_not(ctx, p1);
                break;
            default:
                abort();
            }
            JS_FreeValue(ctx, op1);
            if (!r)
                goto exception;
            sp[-1] = JS_CompactBigInt(ctx, r);
        }
        break;
    default:
    handle_float64:
        {
            double d;
            d = JS_VALUE_GET_FLOAT64(op1);
            switch(op) {
            case OP_inc:
            case OP_dec:
                v = 2 * (op - OP_dec) - 1;
                d += v;
                break;
            case OP_plus:
                break;
            case OP_neg:
                d = -d;
                break;
            default:
                abort();
            }
            sp[-1] = __JS_NewFloat64(ctx, d);
        }
        break;
    }
    return 0;
 exception:
    sp[-1] = JS_UNDEFINED;
    return -1;
}

__exception int js_post_inc_slow(JSContext *ctx,
                                        JSValue *sp, OPCodeEnum op)
{
    JSValue op1;

    /* XXX: allow custom operators */
    op1 = sp[-1];
    op1 = JS_ToNumericFree(ctx, op1);
    if (JS_IsException(op1)) {
        sp[-1] = JS_UNDEFINED;
        return -1;
    }
    sp[-1] = op1;
    sp[0] = JS_DupValue(ctx, op1);
    return js_unary_arith_slow(ctx, sp + 1, op - OP_post_dec + OP_dec);
}

no_inline int js_not_slow(JSContext *ctx, JSValue *sp)
{
    JSValue op1;

    op1 = sp[-1];
    op1 = JS_ToNumericFree(ctx, op1);
    if (JS_IsException(op1))
        goto exception;
    if (JS_VALUE_GET_TAG(op1) == JS_TAG_SHORT_BIG_INT) {
        sp[-1] = __JS_NewShortBigInt(ctx, ~JS_VALUE_GET_SHORT_BIG_INT(op1));
    } else if (JS_VALUE_GET_TAG(op1) == JS_TAG_BIG_INT) {
        JSBigInt *r;
        r = js_bigint_not(ctx, JS_VALUE_GET_PTR(op1));
        JS_FreeValue(ctx, op1);
        if (!r)
            goto exception;
        sp[-1] = JS_CompactBigInt(ctx, r);
    } else {
        int32_t v1;
        if (unlikely(JS_ToInt32Free(ctx, &v1, op1)))
            goto exception;
        sp[-1] = JS_NewInt32(ctx, ~v1);
    }
    return 0;
 exception:
    sp[-1] = JS_UNDEFINED;
    return -1;
}

no_inline __exception int js_binary_arith_slow(JSContext *ctx, JSValue *sp,
                                                      OPCodeEnum op)
{
    JSValue op1, op2;
    uint32_t tag1, tag2;
    double d1, d2;

    op1 = sp[-2];
    op2 = sp[-1];
    tag1 = JS_VALUE_GET_NORM_TAG(op1);
    tag2 = JS_VALUE_GET_NORM_TAG(op2);
    /* fast path for float operations */
    if (tag1 == JS_TAG_FLOAT64 && tag2 == JS_TAG_FLOAT64) {
        d1 = JS_VALUE_GET_FLOAT64(op1);
        d2 = JS_VALUE_GET_FLOAT64(op2);
        goto handle_float64;
    }
    /* fast path for short big int operations */
    if (tag1 == JS_TAG_SHORT_BIG_INT && tag2 == JS_TAG_SHORT_BIG_INT) {
        js_slimb_t v1, v2;
        js_sdlimb_t v;
        v1 = JS_VALUE_GET_SHORT_BIG_INT(op1);
        v2 = JS_VALUE_GET_SHORT_BIG_INT(op2);
        switch(op) {
        case OP_sub:
            v = (js_sdlimb_t)v1 - (js_sdlimb_t)v2;
            break;
        case OP_mul:
            v = (js_sdlimb_t)v1 * (js_sdlimb_t)v2;
            break;
        case OP_div:
            if (v2 == 0 ||
                ((js_limb_t)v1 == (js_limb_t)1 << (JS_LIMB_BITS - 1) &&
                 v2 == -1)) {
                goto slow_big_int;
            }
            sp[-2] = __JS_NewShortBigInt(ctx, v1 / v2);
            return 0;
        case OP_mod:
            if (v2 == 0 ||
                ((js_limb_t)v1 == (js_limb_t)1 << (JS_LIMB_BITS - 1) &&
                 v2 == -1)) {
                goto slow_big_int;
            }
            sp[-2] = __JS_NewShortBigInt(ctx, v1 % v2);
            return 0;
        case OP_pow:
            goto slow_big_int;
        default:
            abort();
        }
        if (likely(v >= JS_SHORT_BIG_INT_MIN && v <= JS_SHORT_BIG_INT_MAX)) {
            sp[-2] = __JS_NewShortBigInt(ctx, v);
        } else {
            JSBigInt *r = js_bigint_new_di(ctx, v);
            if (!r)
                goto exception;
            sp[-2] = JS_MKPTR(JS_TAG_BIG_INT, r);
        }
        return 0;
    }
    op1 = JS_ToNumericFree(ctx, op1);
    if (JS_IsException(op1)) {
        JS_FreeValue(ctx, op2);
        goto exception;
    }
    op2 = JS_ToNumericFree(ctx, op2);
    if (JS_IsException(op2)) {
        JS_FreeValue(ctx, op1);
        goto exception;
    }
    tag1 = JS_VALUE_GET_NORM_TAG(op1);
    tag2 = JS_VALUE_GET_NORM_TAG(op2);

    if (tag1 == JS_TAG_INT && tag2 == JS_TAG_INT) {
        int32_t v1, v2;
        int64_t v;
        v1 = JS_VALUE_GET_INT(op1);
        v2 = JS_VALUE_GET_INT(op2);
        switch(op) {
        case OP_sub:
            v = (int64_t)v1 - (int64_t)v2;
            break;
        case OP_mul:
            v = (int64_t)v1 * (int64_t)v2;
            if (v == 0 && (v1 | v2) < 0) {
                sp[-2] = __JS_NewFloat64(ctx, -0.0);
                return 0;
            }
            break;
        case OP_div:
            sp[-2] = JS_NewFloat64(ctx, (double)v1 / (double)v2);
            return 0;
        case OP_mod:
            if (v1 < 0 || v2 <= 0) {
                sp[-2] = JS_NewFloat64(ctx, fmod(v1, v2));
                return 0;
            } else {
                v = (int64_t)v1 % (int64_t)v2;
            }
            break;
        case OP_pow:
            sp[-2] = JS_NewFloat64(ctx, js_pow(v1, v2));
            return 0;
        default:
            abort();
        }
        sp[-2] = JS_NewInt64(ctx, v);
    } else if ((tag1 == JS_TAG_SHORT_BIG_INT || tag1 == JS_TAG_BIG_INT) &&
               (tag2 == JS_TAG_SHORT_BIG_INT || tag2 == JS_TAG_BIG_INT)) {
        JSBigInt *p1, *p2, *r;
        JSBigIntBuf buf1, buf2;
    slow_big_int:
        /* bigint result */
        if (JS_VALUE_GET_TAG(op1) == JS_TAG_SHORT_BIG_INT)
            p1 = js_bigint_set_short(&buf1, op1);
        else
            p1 = JS_VALUE_GET_PTR(op1);
        if (JS_VALUE_GET_TAG(op2) == JS_TAG_SHORT_BIG_INT)
            p2 = js_bigint_set_short(&buf2, op2);
        else
            p2 = JS_VALUE_GET_PTR(op2);
        switch(op) {
        case OP_add:
            r = js_bigint_add(ctx, p1, p2, 0);
            break;
        case OP_sub:
            r = js_bigint_add(ctx, p1, p2, 1);
            break;
        case OP_mul:
            r = js_bigint_mul(ctx, p1, p2);
            break;
        case OP_div:
            r = js_bigint_divrem(ctx, p1, p2, FALSE);
            break;
        case OP_mod:
            r = js_bigint_divrem(ctx, p1, p2, TRUE);
            break;
        case OP_pow:
            r = js_bigint_pow(ctx, p1, p2);
            break;
        default:
            abort();
        }
        JS_FreeValue(ctx, op1);
        JS_FreeValue(ctx, op2);
        if (!r)
            goto exception;
        sp[-2] = JS_CompactBigInt(ctx, r);
    } else {
        double dr;
        /* float64 result */
        if (JS_ToFloat64Free(ctx, &d1, op1)) {
            JS_FreeValue(ctx, op2);
            goto exception;
        }
        if (JS_ToFloat64Free(ctx, &d2, op2))
            goto exception;
    handle_float64:
        switch(op) {
        case OP_sub:
            dr = d1 - d2;
            break;
        case OP_mul:
            dr = d1 * d2;
            break;
        case OP_div:
            dr = d1 / d2;
            break;
        case OP_mod:
            dr = fmod(d1, d2);
            break;
        case OP_pow:
            dr = js_pow(d1, d2);
            break;
        default:
            abort();
        }
        sp[-2] = __JS_NewFloat64(ctx, dr);
    }
    return 0;
 exception:
    sp[-2] = JS_UNDEFINED;
    sp[-1] = JS_UNDEFINED;
    return -1;
}

no_inline __exception int js_add_slow(JSContext *ctx, JSValue *sp)
{
    JSValue op1, op2;
    uint32_t tag1, tag2;

    op1 = sp[-2];
    op2 = sp[-1];

    tag1 = JS_VALUE_GET_NORM_TAG(op1);
    tag2 = JS_VALUE_GET_NORM_TAG(op2);
    /* fast path for float64 */
    if (tag1 == JS_TAG_FLOAT64 && tag2 == JS_TAG_FLOAT64) {
        double d1, d2;
        d1 = JS_VALUE_GET_FLOAT64(op1);
        d2 = JS_VALUE_GET_FLOAT64(op2);
        sp[-2] = __JS_NewFloat64(ctx, d1 + d2);
        return 0;
    }
    /* fast path for short bigint */
    if (tag1 == JS_TAG_SHORT_BIG_INT && tag2 == JS_TAG_SHORT_BIG_INT) {
        js_slimb_t v1, v2;
        js_sdlimb_t v;
        v1 = JS_VALUE_GET_SHORT_BIG_INT(op1);
        v2 = JS_VALUE_GET_SHORT_BIG_INT(op2);
        v = (js_sdlimb_t)v1 + (js_sdlimb_t)v2;
        if (likely(v >= JS_SHORT_BIG_INT_MIN && v <= JS_SHORT_BIG_INT_MAX)) {
            sp[-2] = __JS_NewShortBigInt(ctx, v);
        } else {
            JSBigInt *r = js_bigint_new_di(ctx, v);
            if (!r)
                goto exception;
            sp[-2] = JS_MKPTR(JS_TAG_BIG_INT, r);
        }
        return 0;
    }

    if (tag1 == JS_TAG_OBJECT || tag2 == JS_TAG_OBJECT) {
        op1 = JS_ToPrimitiveFree(ctx, op1, HINT_NONE);
        if (JS_IsException(op1)) {
            JS_FreeValue(ctx, op2);
            goto exception;
        }

        op2 = JS_ToPrimitiveFree(ctx, op2, HINT_NONE);
        if (JS_IsException(op2)) {
            JS_FreeValue(ctx, op1);
            goto exception;
        }
        tag1 = JS_VALUE_GET_NORM_TAG(op1);
        tag2 = JS_VALUE_GET_NORM_TAG(op2);
    }

    if (tag_is_string(tag1) || tag_is_string(tag2)) {
        sp[-2] = JS_ConcatString(ctx, op1, op2);
        if (JS_IsException(sp[-2]))
            goto exception;
        return 0;
    }

    op1 = JS_ToNumericFree(ctx, op1);
    if (JS_IsException(op1)) {
        JS_FreeValue(ctx, op2);
        goto exception;
    }
    op2 = JS_ToNumericFree(ctx, op2);
    if (JS_IsException(op2)) {
        JS_FreeValue(ctx, op1);
        goto exception;
    }
    tag1 = JS_VALUE_GET_NORM_TAG(op1);
    tag2 = JS_VALUE_GET_NORM_TAG(op2);

    if (tag1 == JS_TAG_INT && tag2 == JS_TAG_INT) {
        int32_t v1, v2;
        int64_t v;
        v1 = JS_VALUE_GET_INT(op1);
        v2 = JS_VALUE_GET_INT(op2);
        v = (int64_t)v1 + (int64_t)v2;
        sp[-2] = JS_NewInt64(ctx, v);
    } else if ((tag1 == JS_TAG_BIG_INT || tag1 == JS_TAG_SHORT_BIG_INT) &&
               (tag2 == JS_TAG_BIG_INT || tag2 == JS_TAG_SHORT_BIG_INT)) {
        JSBigInt *p1, *p2, *r;
        JSBigIntBuf buf1, buf2;
        /* bigint result */
        if (JS_VALUE_GET_TAG(op1) == JS_TAG_SHORT_BIG_INT)
            p1 = js_bigint_set_short(&buf1, op1);
        else
            p1 = JS_VALUE_GET_PTR(op1);
        if (JS_VALUE_GET_TAG(op2) == JS_TAG_SHORT_BIG_INT)
            p2 = js_bigint_set_short(&buf2, op2);
        else
            p2 = JS_VALUE_GET_PTR(op2);
        r = js_bigint_add(ctx, p1, p2, 0);
        JS_FreeValue(ctx, op1);
        JS_FreeValue(ctx, op2);
        if (!r)
            goto exception;
        sp[-2] = JS_CompactBigInt(ctx, r);
    } else {
        double d1, d2;
        /* float64 result */
        if (JS_ToFloat64Free(ctx, &d1, op1)) {
            JS_FreeValue(ctx, op2);
            goto exception;
        }
        if (JS_ToFloat64Free(ctx, &d2, op2))
            goto exception;
        sp[-2] = __JS_NewFloat64(ctx, d1 + d2);
    }
    return 0;
 exception:
    sp[-2] = JS_UNDEFINED;
    sp[-1] = JS_UNDEFINED;
    return -1;
}

no_inline __exception int js_binary_logic_slow(JSContext *ctx,
                                                      JSValue *sp,
                                                      OPCodeEnum op)
{
    JSValue op1, op2;
    uint32_t tag1, tag2;
    uint32_t v1, v2, r;

    op1 = sp[-2];
    op2 = sp[-1];
    tag1 = JS_VALUE_GET_NORM_TAG(op1);
    tag2 = JS_VALUE_GET_NORM_TAG(op2);

    if (tag1 == JS_TAG_SHORT_BIG_INT && tag2 == JS_TAG_SHORT_BIG_INT) {
        js_slimb_t v1, v2, v;
        js_sdlimb_t vd;
        v1 = JS_VALUE_GET_SHORT_BIG_INT(op1);
        v2 = JS_VALUE_GET_SHORT_BIG_INT(op2);
        /* bigint fast path */
        switch(op) {
        case OP_and:
            v = v1 & v2;
            break;
        case OP_or:
            v = v1 | v2;
            break;
        case OP_xor:
            v = v1 ^ v2;
            break;
        case OP_sar:
            if (v2 > (JS_LIMB_BITS - 1)) {
                goto slow_big_int;
            } else if (v2 < 0) {
                if (v2 < -(JS_LIMB_BITS - 1))
                    goto slow_big_int;
                v2 = -v2;
                goto bigint_shl;
            }
        bigint_sar:
            v = v1 >> v2;
            break;
        case OP_shl:
            if (v2 > (JS_LIMB_BITS - 1)) {
                goto slow_big_int;
            } else if (v2 < 0) {
                if (v2 < -(JS_LIMB_BITS - 1))
                    goto slow_big_int;
                v2 = -v2;
                goto bigint_sar;
            }
        bigint_shl:
            vd = (js_dlimb_t)v1 << v2;
            if (likely(vd >= JS_SHORT_BIG_INT_MIN &&
                       vd <= JS_SHORT_BIG_INT_MAX)) {
                v = vd;
            } else {
                JSBigInt *r = js_bigint_new_di(ctx, vd);
                if (!r)
                    goto exception;
                sp[-2] = JS_MKPTR(JS_TAG_BIG_INT, r);
                return 0;
            }
            break;
        default:
            abort();
        }
        sp[-2] = __JS_NewShortBigInt(ctx, v);
        return 0;
    }
    op1 = JS_ToNumericFree(ctx, op1);
    if (JS_IsException(op1)) {
        JS_FreeValue(ctx, op2);
        goto exception;
    }
    op2 = JS_ToNumericFree(ctx, op2);
    if (JS_IsException(op2)) {
        JS_FreeValue(ctx, op1);
        goto exception;
    }

    tag1 = JS_VALUE_GET_TAG(op1);
    tag2 = JS_VALUE_GET_TAG(op2);
    if ((tag1 == JS_TAG_BIG_INT || tag1 == JS_TAG_SHORT_BIG_INT) &&
        (tag2 == JS_TAG_BIG_INT || tag2 == JS_TAG_SHORT_BIG_INT)) {
        JSBigInt *p1, *p2, *r;
        JSBigIntBuf buf1, buf2;
    slow_big_int:
        if (JS_VALUE_GET_TAG(op1) == JS_TAG_SHORT_BIG_INT)
            p1 = js_bigint_set_short(&buf1, op1);
        else
            p1 = JS_VALUE_GET_PTR(op1);
        if (JS_VALUE_GET_TAG(op2) == JS_TAG_SHORT_BIG_INT)
            p2 = js_bigint_set_short(&buf2, op2);
        else
            p2 = JS_VALUE_GET_PTR(op2);
        switch(op) {
        case OP_and:
        case OP_or:
        case OP_xor:
            r = js_bigint_logic(ctx, p1, p2, op);
            break;
        case OP_shl:
        case OP_sar:
            {
                js_slimb_t shift;
                shift = js_bigint_get_si_sat(p2);
                if (shift > INT32_MAX)
                    shift = INT32_MAX;
                else if (shift < -INT32_MAX)
                    shift = -INT32_MAX;
                if (op == OP_sar)
                    shift = -shift;
                if (shift >= 0)
                    r = js_bigint_shl(ctx, p1, shift);
                else
                    r = js_bigint_shr(ctx, p1, -shift);
            }
            break;
        default:
            abort();
        }
        JS_FreeValue(ctx, op1);
        JS_FreeValue(ctx, op2);
        if (!r)
            goto exception;
        sp[-2] = JS_CompactBigInt(ctx, r);
    } else {
        if (unlikely(JS_ToInt32Free(ctx, (int32_t *)&v1, op1))) {
            JS_FreeValue(ctx, op2);
            goto exception;
        }
        if (unlikely(JS_ToInt32Free(ctx, (int32_t *)&v2, op2)))
            goto exception;
        switch(op) {
        case OP_shl:
            r = v1 << (v2 & 0x1f);
            break;
        case OP_sar:
            r = (int)v1 >> (v2 & 0x1f);
            break;
        case OP_and:
            r = v1 & v2;
            break;
        case OP_or:
            r = v1 | v2;
            break;
        case OP_xor:
            r = v1 ^ v2;
            break;
        default:
            abort();
        }
        sp[-2] = JS_NewInt32(ctx, r);
    }
    return 0;
 exception:
    sp[-2] = JS_UNDEFINED;
    sp[-1] = JS_UNDEFINED;
    return -1;
}
