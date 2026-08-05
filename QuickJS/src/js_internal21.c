#include "js_internal.h"
#include "debuglog.h"
#undef countof
#define countof(arr) JS_ARRAY_LEN(arr)


JSValue js_async_from_sync_iterator_unwrap(JSContext *ctx,
                                                  JSValueConst this_val,
                                                  int argc, JSValueConst *argv,
                                                  int magic, JSValue *func_data)
{
    return js_create_iterator_result(ctx, JS_DupValue(ctx, argv[0]),
                                     JS_ToBool(ctx, func_data[0]));
}

JSValue js_async_from_sync_iterator_unwrap_func_create(JSContext *ctx,
                                                              BOOL done)
{
    JSValueConst func_data[1];

    func_data[0] = (JSValueConst)JS_NewBool(ctx, done);
    return JS_NewCFunctionData(ctx, js_async_from_sync_iterator_unwrap,
                               1, 0, 1, func_data);
}

JSValue js_async_from_sync_iterator_close_wrap(JSContext *ctx,
                                                      JSValueConst this_val,
                                                      int argc, JSValueConst *argv,
                                                      int magic, JSValue *func_data)
{
    JS_Throw(ctx, JS_DupValue(ctx, argv[0]));
    JS_IteratorClose(ctx, func_data[0], TRUE);
    return JS_EXCEPTION;
}

JSValue js_async_from_sync_iterator_close_wrap_func_create(JSContext *ctx, JSValueConst sync_iter)
{
    return JS_NewCFunctionData(ctx, js_async_from_sync_iterator_close_wrap,
                               1, 0, 1, &sync_iter);
}

JSValue js_async_from_sync_iterator_next(JSContext *ctx, JSValueConst this_val,
                                                int argc, JSValueConst *argv,
                                                int magic)
{
    JSValue promise, resolving_funcs[2], value, err, method;
    JSAsyncFromSyncIteratorData *s;
    int done;
    int is_reject;

    promise = JS_NewPromiseCapability(ctx, resolving_funcs);
    if (JS_IsException(promise))
        return JS_EXCEPTION;
    s = JS_GetOpaque(this_val, JS_CLASS_ASYNC_FROM_SYNC_ITERATOR);
    if (!s) {
        JS_ThrowTypeError(ctx, "not an Async-from-Sync Iterator");
        goto reject;
    }

    if (magic == GEN_MAGIC_NEXT) {
        method = JS_DupValue(ctx, s->next_method);
    } else {
        method = JS_GetProperty(ctx, s->sync_iter,
                                magic == GEN_MAGIC_RETURN ? JS_ATOM_return :
                                JS_ATOM_throw);
        if (JS_IsException(method))
            goto reject;
        if (JS_IsUndefined(method) || JS_IsNull(method)) {
            if (magic == GEN_MAGIC_RETURN) {
                err = js_create_iterator_result(ctx, JS_DupValue(ctx, argv[0]), TRUE);
                is_reject = 0;
                goto done_resolve;
            } else {
                if (JS_IteratorClose(ctx, s->sync_iter, FALSE))
                    goto reject;
                JS_ThrowTypeError(ctx, "throw is not a method");
                goto reject;
            }
        }
    }
    value = JS_IteratorNext2(ctx, s->sync_iter, method,
                             argc >= 1 ? 1 : 0, argv, &done);
    JS_FreeValue(ctx, method);
    if (JS_IsException(value))
        goto reject;
    if (done == 2) {
        JSValue obj = value;
        value = JS_IteratorGetCompleteValue(ctx, obj, &done);
        JS_FreeValue(ctx, obj);
        if (JS_IsException(value))
            goto reject;
    }

    if (JS_IsException(value))
        goto reject;
    {
        JSValue value_wrapper_promise, resolve_reject[2];
        int res;

        value_wrapper_promise = js_promise_resolve(ctx, ctx->promise_ctor,
                                                   1, (JSValueConst *)&value, 0);
        if (JS_IsException(value_wrapper_promise)) {
            JSValue res2;
            JS_FreeValue(ctx, value);
            if (magic != GEN_MAGIC_RETURN && !done) {
                JS_IteratorClose(ctx, s->sync_iter, TRUE);
            }
        reject:
            err = JS_GetException(ctx);
            is_reject = 1;
        done_resolve:
            res2 = JS_Call(ctx, resolving_funcs[is_reject], JS_UNDEFINED,
                           1, (JSValueConst *)&err);
            JS_FreeValue(ctx, err);
            JS_FreeValue(ctx, res2);
            JS_FreeValue(ctx, resolving_funcs[0]);
            JS_FreeValue(ctx, resolving_funcs[1]);
            return promise;
        }

        resolve_reject[0] =
            js_async_from_sync_iterator_unwrap_func_create(ctx, done);
        if (JS_IsException(resolve_reject[0])) {
            JS_FreeValue(ctx, value_wrapper_promise);
            goto fail;
        }
        if (done || magic == GEN_MAGIC_RETURN) {
            resolve_reject[1] = JS_UNDEFINED;
        } else {
            resolve_reject[1] =
                js_async_from_sync_iterator_close_wrap_func_create(ctx, s->sync_iter);
            if (JS_IsException(resolve_reject[1])) {
                JS_FreeValue(ctx, value_wrapper_promise);
                JS_FreeValue(ctx, resolve_reject[0]);
                goto fail;
            }
        }
        JS_FreeValue(ctx, value);
        res = perform_promise_then(ctx, value_wrapper_promise,
                                   (JSValueConst *)resolve_reject,
                                   (JSValueConst *)resolving_funcs);
        JS_FreeValue(ctx, resolve_reject[0]);
        JS_FreeValue(ctx, resolve_reject[1]);
        JS_FreeValue(ctx, value_wrapper_promise);
        JS_FreeValue(ctx, resolving_funcs[0]);
        JS_FreeValue(ctx, resolving_funcs[1]);
        if (res) {
            JS_FreeValue(ctx, promise);
            return JS_EXCEPTION;
        }
    }
    return promise;
 fail:
    JS_FreeValue(ctx, value);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, promise);
    return JS_EXCEPTION;
}

/* URI handling */

int string_get_hex(JSString *p, int k, int n) {
    int c = 0, h;
    while (n-- > 0) {
        if ((h = from_hex(string_get(p, k++))) < 0)
            return -1;
        c = (c << 4) | h;
    }
    return c;
}

int isURIReserved(int c) {
    return c < 0x100 && memchr(";/?:@&=+$,#", c, sizeof(";/?:@&=+$,#") - 1) != NULL;
}

int __attribute__((format(printf, 2, 3))) js_throw_URIError(JSContext *ctx, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    JS_ThrowError(ctx, JS_URI_ERROR, fmt, ap);
    va_end(ap);
    return -1;
}

int hex_decode(JSContext *ctx, JSString *p, int k) {
    int c;

    if (k >= p->len || string_get(p, k) != '%')
        return js_throw_URIError(ctx, "expecting %%");
    if (k + 2 >= p->len || (c = string_get_hex(p, k + 1, 2)) < 0)
        return js_throw_URIError(ctx, "expecting hex digit");

    return c;
}

JSValue js_global_decodeURI(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv, int isComponent)
{
    JSValue str;
    StringBuffer b_s, *b = &b_s;
    JSString *p;
    int k, c, c1, n, c_min;

    str = JS_ToString(ctx, argv[0]);
    if (JS_IsException(str))
        return str;

    string_buffer_init(ctx, b, 0);

    p = JS_VALUE_GET_STRING(str);
    for (k = 0; k < p->len;) {
        c = string_get(p, k);
        if (c == '%') {
            c = hex_decode(ctx, p, k);
            if (c < 0)
                goto fail;
            k += 3;
            if (c < 0x80) {
                if (!isComponent && isURIReserved(c)) {
                    c = '%';
                    k -= 2;
                }
            } else {
                /* Decode URI-encoded UTF-8 sequence */
                if (c >= 0xc0 && c <= 0xdf) {
                    n = 1;
                    c_min = 0x80;
                    c &= 0x1f;
                } else if (c >= 0xe0 && c <= 0xef) {
                    n = 2;
                    c_min = 0x800;
                    c &= 0xf;
                } else if (c >= 0xf0 && c <= 0xf7) {
                    n = 3;
                    c_min = 0x10000;
                    c &= 0x7;
                } else {
                    n = 0;
                    c_min = 1;
                    c = 0;
                }
                while (n-- > 0) {
                    c1 = hex_decode(ctx, p, k);
                    if (c1 < 0)
                        goto fail;
                    k += 3;
                    if ((c1 & 0xc0) != 0x80) {
                        c = 0;
                        break;
                    }
                    c = (c << 6) | (c1 & 0x3f);
                }
                if (c < c_min || c > 0x10FFFF || is_surrogate(c)) {
                    js_throw_URIError(ctx, "malformed UTF-8");
                    goto fail;
                }
            }
        } else {
            k++;
        }
        string_buffer_putc(b, c);
    }
    JS_FreeValue(ctx, str);
    return string_buffer_end(b);

fail:
    JS_FreeValue(ctx, str);
    string_buffer_free(b);
    return JS_EXCEPTION;
}

int isUnescaped(int c) {
    char const unescaped_chars[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789"
        "@*_+-./";
    return c < 0x100 &&
        memchr(unescaped_chars, c, sizeof(unescaped_chars) - 1);
}

int isURIUnescaped(int c, int isComponent) {
    return c < 0x100 &&
        ((c >= 0x61 && c <= 0x7a) ||
         (c >= 0x41 && c <= 0x5a) ||
         (c >= 0x30 && c <= 0x39) ||
         memchr("-_.!~*'()", c, sizeof("-_.!~*'()") - 1) != NULL ||
         (!isComponent && isURIReserved(c)));
}

int encodeURI_hex(StringBuffer *b, int c) {
    uint8_t buf[6];
    int n = 0;
    const char *hex = "0123456789ABCDEF";

    buf[n++] = '%';
    if (c >= 256) {
        buf[n++] = 'u';
        buf[n++] = hex[(c >> 12) & 15];
        buf[n++] = hex[(c >>  8) & 15];
    }
    buf[n++] = hex[(c >> 4) & 15];
    buf[n++] = hex[(c >> 0) & 15];
    return string_buffer_write8(b, buf, n);
}

JSValue js_global_encodeURI(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv,
                                   int isComponent)
{
    JSValue str;
    StringBuffer b_s, *b = &b_s;
    JSString *p;
    int k, c, c1;

    str = JS_ToString(ctx, argv[0]);
    if (JS_IsException(str))
        return str;

    p = JS_VALUE_GET_STRING(str);
    string_buffer_init(ctx, b, p->len);
    for (k = 0; k < p->len;) {
        c = string_get(p, k);
        k++;
        if (isURIUnescaped(c, isComponent)) {
            string_buffer_putc16(b, c);
        } else {
            if (is_lo_surrogate(c)) {
                js_throw_URIError(ctx, "invalid character");
                goto fail;
            } else if (is_hi_surrogate(c)) {
                if (k >= p->len) {
                    js_throw_URIError(ctx, "expecting surrogate pair");
                    goto fail;
                }
                c1 = string_get(p, k);
                k++;
                if (!is_lo_surrogate(c1)) {
                    js_throw_URIError(ctx, "expecting surrogate pair");
                    goto fail;
                }
                c = from_surrogate(c, c1);
            }
            if (c < 0x80) {
                encodeURI_hex(b, c);
            } else {
                /* XXX: use C UTF-8 conversion ? */
                if (c < 0x800) {
                    encodeURI_hex(b, (c >> 6) | 0xc0);
                } else {
                    if (c < 0x10000) {
                        encodeURI_hex(b, (c >> 12) | 0xe0);
                    } else {
                        encodeURI_hex(b, (c >> 18) | 0xf0);
                        encodeURI_hex(b, ((c >> 12) & 0x3f) | 0x80);
                    }
                    encodeURI_hex(b, ((c >> 6) & 0x3f) | 0x80);
                }
                encodeURI_hex(b, (c & 0x3f) | 0x80);
            }
        }
    }
    JS_FreeValue(ctx, str);
    return string_buffer_end(b);

fail:
    JS_FreeValue(ctx, str);
    string_buffer_free(b);
    return JS_EXCEPTION;
}

JSValue js_global_escape(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv)
{
    JSValue str;
    StringBuffer b_s, *b = &b_s;
    JSString *p;
    int i, len, c;

    str = JS_ToString(ctx, argv[0]);
    if (JS_IsException(str))
        return str;

    p = JS_VALUE_GET_STRING(str);
    string_buffer_init(ctx, b, p->len);
    for (i = 0, len = p->len; i < len; i++) {
        c = string_get(p, i);
        if (isUnescaped(c)) {
            string_buffer_putc16(b, c);
        } else {
            encodeURI_hex(b, c);
        }
    }
    JS_FreeValue(ctx, str);
    return string_buffer_end(b);
}

JSValue js_global_unescape(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv)
{
    JSValue str;
    StringBuffer b_s, *b = &b_s;
    JSString *p;
    int i, len, c, n;

    str = JS_ToString(ctx, argv[0]);
    if (JS_IsException(str))
        return str;

    string_buffer_init(ctx, b, 0);
    p = JS_VALUE_GET_STRING(str);
    for (i = 0, len = p->len; i < len; i++) {
        c = string_get(p, i);
        if (c == '%') {
            if (i + 6 <= len
            &&  string_get(p, i + 1) == 'u'
            &&  (n = string_get_hex(p, i + 2, 4)) >= 0) {
                c = n;
                i += 6 - 1;
            } else
            if (i + 3 <= len
            &&  (n = string_get_hex(p, i + 1, 2)) >= 0) {
                c = n;
                i += 3 - 1;
            }
        }
        string_buffer_putc16(b, c);
    }
    JS_FreeValue(ctx, str);
    return string_buffer_end(b);
}

/* Date */

int64_t math_mod(int64_t a, int64_t b) {
    /* return positive modulo */
    int64_t m = a % b;
    return m + (m < 0) * b;
}

int64_t floor_div(int64_t a, int64_t b) {
    /* integer division rounding toward -Infinity */
    int64_t m = a % b;
    return (a - (m + (m < 0) * b)) / b;
}

JSValue js_Date_parse(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv);

__exception int JS_ThisTimeValue(JSContext *ctx, double *valp, JSValueConst this_val)
{
    if (JS_VALUE_GET_TAG(this_val) == JS_TAG_OBJECT) {
        JSObject *p = JS_VALUE_GET_OBJ(this_val);
        if (p->class_id == JS_CLASS_DATE && JS_IsNumber(p->u.object_data))
            return JS_ToFloat64(ctx, valp, p->u.object_data);
    }
    JS_ThrowTypeError(ctx, "not a Date object");
    return -1;
}

JSValue JS_SetThisTimeValue(JSContext *ctx, JSValueConst this_val, double v)
{
    if (JS_VALUE_GET_TAG(this_val) == JS_TAG_OBJECT) {
        JSObject *p = JS_VALUE_GET_OBJ(this_val);
        if (p->class_id == JS_CLASS_DATE) {
            JS_FreeValue(ctx, p->u.object_data);
            p->u.object_data = JS_NewFloat64(ctx, v);
            return JS_DupValue(ctx, p->u.object_data);
        }
    }
    return JS_ThrowTypeError(ctx, "not a Date object");
}

int64_t days_from_year(int64_t y) {
    return 365 * (y - 1970) + floor_div(y - 1969, 4) -
        floor_div(y - 1901, 100) + floor_div(y - 1601, 400);
}

int64_t days_in_year(int64_t y) {
    return 365 + !(y % 4) - !(y % 100) + !(y % 400);
}

/* return the year, update days */
int64_t year_from_days(int64_t *days) {
    int64_t y, d1, nd, d = *days;
    y = floor_div(d * 10000, 3652425) + 1970;
    /* the initial approximation is very good, so only a few
       iterations are necessary */
    for(;;) {
        d1 = d - days_from_year(y);
        if (d1 < 0) {
            y--;
            d1 += days_in_year(y);
        } else {
            nd = days_in_year(y);
            if (d1 < nd)
                break;
            d1 -= nd;
            y++;
        }
    }
    *days = d1;
    return y;
}

__exception int get_date_fields(JSContext *ctx, JSValueConst obj,
                                       double fields[minimum_length(9)], int is_local, int force)
{
    double dval;
    int64_t d, days, wd, y, i, md, h, m, s, ms, tz = 0;

    if (JS_ThisTimeValue(ctx, &dval, obj))
        return -1;

    if (isnan(dval)) {
        if (!force)
            return FALSE; /* NaN */
        d = 0;        /* initialize all fields to 0 */
    } else {
        d = dval;     /* assuming -8.64e15 <= dval <= -8.64e15 */
        if (is_local) {
            tz = -getTimezoneOffset(d);
            d += tz * 60000;
        }
    }

    /* result is >= 0, we can use % */
    h = math_mod(d, 86400000);
    days = (d - h) / 86400000;
    ms = h % 1000;
    h = (h - ms) / 1000;
    s = h % 60;
    h = (h - s) / 60;
    m = h % 60;
    h = (h - m) / 60;
    wd = math_mod(days + 4, 7); /* week day */
    y = year_from_days(&days);

    for(i = 0; i < 11; i++) {
        md = month_days[i];
        if (i == 1)
            md += days_in_year(y) - 365;
        if (days < md)
            break;
        days -= md;
    }
    fields[0] = y;
    fields[1] = i;
    fields[2] = days + 1;
    fields[3] = h;
    fields[4] = m;
    fields[5] = s;
    fields[6] = ms;
    fields[7] = wd;
    fields[8] = tz;
    return TRUE;
}

double time_clip(double t) {
    if (t >= -8.64e15 && t <= 8.64e15)
        return trunc(t) + 0.0;  /* convert -0 to +0 */
    else
        return NAN;
}

/* The spec mandates the use of 'double' and it specifies the order
   of the operations */
double set_date_fields(double fields[minimum_length(7)], int is_local) {
    double y, m, dt, ym, mn, day, h, s, milli, time, tv;
    int yi, mi, i;
    int64_t days;
    volatile double temp;  /* enforce evaluation order */

    /* emulate 21.4.1.15 MakeDay ( year, month, date ) */
    y = fields[0];
    m = fields[1];
    dt = fields[2];
    ym = y + floor(m / 12);
    mn = fmod(m, 12);
    if (mn < 0)
        mn += 12;
    if (ym < -271821 || ym > 275760)
        return NAN;

    yi = ym;
    mi = mn;
    days = days_from_year(yi);
    for(i = 0; i < mi; i++) {
        days += month_days[i];
        if (i == 1)
            days += days_in_year(yi) - 365;
    }
    day = days + dt - 1;

    /* emulate 21.4.1.14 MakeTime ( hour, min, sec, ms ) */
    h = fields[3];
    m = fields[4];
    s = fields[5];
    milli = fields[6];
    /* Use a volatile intermediary variable to ensure order of evaluation
     * as specified in ECMA. This fixes a test262 error on
     * test262/test/built-ins/Date/UTC/fp-evaluation-order.js.
     * Without the volatile qualifier, the compile can generate code
     * that performs the computation in a different order or with instructions
     * that produce a different result such as FMA (float multiply and add).
     */
    time = h * 3600000;
    time += (temp = m * 60000);
    time += (temp = s * 1000);
    time += milli;

    /* emulate 21.4.1.16 MakeDate ( day, time ) */
    tv = (temp = day * 86400000) + time;   /* prevent generation of FMA */
    if (!isfinite(tv))
        return NAN;

    /* adjust for local time and clip */
    if (is_local) {
        int64_t ti = tv < INT64_MIN ? INT64_MIN : tv >= 0x1p63 ? INT64_MAX : (int64_t)tv;
        tv += getTimezoneOffset(ti) * 60000;
    }
    return time_clip(tv);
}

double set_date_fields_checked(double fields[minimum_length(7)], int is_local)
{
    int i;
    double a;
    for(i = 0; i < 7; i++) {
        a = fields[i];
        if (!isfinite(a))
            return NAN;
        fields[i] = trunc(a);
        if (i == 0 && fields[0] >= 0 && fields[0] < 100)
            fields[0] += 1900;
    }
    return set_date_fields(fields, is_local);
}

JSValue get_date_field(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv, int magic)
{
    // get_date_field(obj, n, is_local)
    double fields[9];
    int res, n, is_local;

    is_local = magic & 0x0F;
    n = (magic >> 4) & 0x0F;
    res = get_date_fields(ctx, this_val, fields, is_local, 0);
    if (res < 0)
        return JS_EXCEPTION;
    if (!res)
        return JS_NAN;

    if (magic & 0x100) {    // getYear
        fields[0] -= 1900;
    }
    return JS_NewFloat64(ctx, fields[n]);
}

JSValue set_date_field(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv, int magic)
{
    // _field(obj, first_field, end_field, args, is_local)
    double fields[9];
    int res, first_field, end_field, is_local, i, n, res1;
    double d, a;

    d = NAN;
    first_field = (magic >> 8) & 0x0F;
    end_field = (magic >> 4) & 0x0F;
    is_local = magic & 0x0F;

    res = get_date_fields(ctx, this_val, fields, is_local, first_field == 0);
    if (res < 0)
        return JS_EXCEPTION;
    res1 = res;

    // Argument coercion is observable and must be done unconditionally.
    n = min_int(argc, end_field - first_field);
    for(i = 0; i < n; i++) {
        if (JS_ToFloat64(ctx, &a, argv[i]))
            return JS_EXCEPTION;
        if (!isfinite(a))
            res = FALSE;
        fields[first_field + i] = trunc(a);
    }

    if (!res1)
        return JS_NAN; /* thisTimeValue is NaN */

    if (res && argc > 0)
        d = set_date_fields(fields, is_local);

    return JS_SetThisTimeValue(ctx, this_val, d);
}

/* fmt:
   0: toUTCString: "Tue, 02 Jan 2018 23:04:46 GMT"
   1: toString: "Wed Jan 03 2018 00:05:22 GMT+0100 (CET)"
   2: toISOString: "2018-01-02T23:02:56.927Z"
   3: toLocaleString: "1/2/2018, 11:40:40 PM"
   part: 1=date, 2=time 3=all
   XXX: should use a variant of strftime().
 */
JSValue get_date_string(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv, int magic)
{
    // _string(obj, fmt, part)
    char buf[64];
    double fields[9];
    int res, fmt, part, pos;
    int y, mon, d, h, m, s, ms, wd, tz;

    fmt = (magic >> 4) & 0x0F;
    part = magic & 0x0F;

    res = get_date_fields(ctx, this_val, fields, fmt & 1, 0);
    if (res < 0)
        return JS_EXCEPTION;
    if (!res) {
        if (fmt == 2)
            return JS_ThrowRangeError(ctx, "Date value is NaN");
        else
            return js_new_string8(ctx, "Invalid Date");
    }

    y = fields[0];
    mon = fields[1];
    d = fields[2];
    h = fields[3];
    m = fields[4];
    s = fields[5];
    ms = fields[6];
    wd = fields[7];
    tz = fields[8];

    pos = 0;

    if (part & 1) { /* date part */
        switch(fmt) {
        case 0:
            pos += snprintf(buf + pos, sizeof(buf) - pos,
                            "%.3s, %02d %.3s %0*d ",
                            day_names + wd * 3, d,
                            month_names + mon * 3, 4 + (y < 0), y);
            break;
        case 1:
            pos += snprintf(buf + pos, sizeof(buf) - pos,
                            "%.3s %.3s %02d %0*d",
                            day_names + wd * 3,
                            month_names + mon * 3, d, 4 + (y < 0), y);
            if (part == 3) {
                buf[pos++] = ' ';
            }
            break;
        case 2:
            if (y >= 0 && y <= 9999) {
                pos += snprintf(buf + pos, sizeof(buf) - pos,
                                "%04d", y);
            } else {
                pos += snprintf(buf + pos, sizeof(buf) - pos,
                                "%+07d", y);
            }
            pos += snprintf(buf + pos, sizeof(buf) - pos,
                            "-%02d-%02dT", mon + 1, d);
            break;
        case 3:
            pos += snprintf(buf + pos, sizeof(buf) - pos,
                            "%02d/%02d/%0*d", mon + 1, d, 4 + (y < 0), y);
            if (part == 3) {
                buf[pos++] = ',';
                buf[pos++] = ' ';
            }
            break;
        }
    }
    if (part & 2) { /* time part */
        switch(fmt) {
        case 0:
            pos += snprintf(buf + pos, sizeof(buf) - pos,
                            "%02d:%02d:%02d GMT", h, m, s);
            break;
        case 1:
            pos += snprintf(buf + pos, sizeof(buf) - pos,
                            "%02d:%02d:%02d GMT", h, m, s);
            if (tz < 0) {
                buf[pos++] = '-';
                tz = -tz;
            } else {
                buf[pos++] = '+';
            }
            /* tz is >= 0, can use % */
            pos += snprintf(buf + pos, sizeof(buf) - pos,
                            "%02d%02d", tz / 60, tz % 60);
            /* XXX: tack the time zone code? */
            break;
        case 2:
            pos += snprintf(buf + pos, sizeof(buf) - pos,
                            "%02d:%02d:%02d.%03dZ", h, m, s, ms);
            break;
        case 3:
            pos += snprintf(buf + pos, sizeof(buf) - pos,
                            "%02d:%02d:%02d %cM", (h + 11) % 12 + 1, m, s,
                            (h < 12) ? 'A' : 'P');
            break;
        }
    }
    return  JS_NewStringLen(ctx, buf, pos);
}

/* OS dependent: return the UTC time in ms since 1970. */
int64_t date_now(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000 + (tv.tv_usec / 1000);
}

JSValue js_date_constructor(JSContext *ctx, JSValueConst new_target,
                                   int argc, JSValueConst *argv)
{
    // Date(y, mon, d, h, m, s, ms)
    JSValue rv;
    int i, n;
    double val;

    if (JS_IsUndefined(new_target)) {
        /* invoked as function */
        argc = 0;
    }
    n = argc;
    if (n == 0) {
        val = date_now();
    } else if (n == 1) {
        JSValue v, dv;
        if (JS_VALUE_GET_TAG(argv[0]) == JS_TAG_OBJECT) {
            JSObject *p = JS_VALUE_GET_OBJ(argv[0]);
            if (p->class_id == JS_CLASS_DATE && JS_IsNumber(p->u.object_data)) {
                if (JS_ToFloat64(ctx, &val, p->u.object_data))
                    return JS_EXCEPTION;
                val = time_clip(val);
                goto has_val;
            }
        }
        v = JS_ToPrimitive(ctx, argv[0], HINT_NONE);
        if (JS_IsString(v)) {
            dv = js_Date_parse(ctx, JS_UNDEFINED, 1, (JSValueConst *)&v);
            JS_FreeValue(ctx, v);
            if (JS_IsException(dv))
                return JS_EXCEPTION;
            if (JS_ToFloat64Free(ctx, &val, dv))
                return JS_EXCEPTION;
        } else {
            if (JS_ToFloat64Free(ctx, &val, v))
                return JS_EXCEPTION;
        }
        val = time_clip(val);
    } else {
        double fields[] = { 0, 0, 1, 0, 0, 0, 0 };
        if (n > 7)
            n = 7;
        for(i = 0; i < n; i++) {
            if (JS_ToFloat64(ctx, &fields[i], argv[i]))
                return JS_EXCEPTION;
        }
        val = set_date_fields_checked(fields, 1);
    }
has_val:
#if 0
    JSValueConst args[3];
    args[0] = new_target;
    args[1] = ctx->class_proto[JS_CLASS_DATE];
    args[2] = JS_NewFloat64(ctx, val);
    rv = js___date_create(ctx, JS_UNDEFINED, 3, args);
#else
    rv = js_create_from_ctor(ctx, new_target, JS_CLASS_DATE);
    if (!JS_IsException(rv))
        JS_SetObjectData(ctx, rv, JS_NewFloat64(ctx, val));
#endif
    if (!JS_IsException(rv) && JS_IsUndefined(new_target)) {
        /* invoked as a function, return (new Date()).toString(); */
        JSValue s;
        s = get_date_string(ctx, rv, 0, NULL, 0x13);
        JS_FreeValue(ctx, rv);
        rv = s;
    }
    return rv;
}

JSValue js_Date_UTC(JSContext *ctx, JSValueConst this_val,
                           int argc, JSValueConst *argv)
{
    // UTC(y, mon, d, h, m, s, ms)
    double fields[] = { 0, 0, 1, 0, 0, 0, 0 };
    int i, n;

    n = argc;
    if (n == 0)
        return JS_NAN;
    if (n > 7)
        n = 7;
    for(i = 0; i < n; i++) {
        if (JS_ToFloat64(ctx, &fields[i], argv[i]))
            return JS_EXCEPTION;
    }
    return JS_NewFloat64(ctx, set_date_fields_checked(fields, 0));
}

/* Date string parsing */

BOOL string_skip_char(const uint8_t *sp, int *pp, int c) {
    if (sp[*pp] == c) {
        *pp += 1;
        return TRUE;
    } else {
        return FALSE;
    }
}

/* skip spaces, update offset, return next char */
int string_skip_spaces(const uint8_t *sp, int *pp) {
    int c;
    while ((c = sp[*pp]) == ' ')
        *pp += 1;
    return c;
}

/* skip dashes dots and commas */
int string_skip_separators(const uint8_t *sp, int *pp) {
    int c;
    while ((c = sp[*pp]) == '-' || c == '/' || c == '.' || c == ',')
        *pp += 1;
    return c;
}

/* skip a word, stop on spaces, digits and separators, update offset */
int string_skip_until(const uint8_t *sp, int *pp, const char *stoplist) {
    int c;
    while (!strchr(stoplist, c = sp[*pp]))
        *pp += 1;
    return c;
}

/* parse a numeric field (max_digits = 0 -> no maximum) */
BOOL string_get_digits(const uint8_t *sp, int *pp, int *pval,
                              int min_digits, int max_digits)
{
    int v = 0;
    int c, p = *pp, p_start;

    p_start = p;
    while ((c = sp[p]) >= '0' && c <= '9') {
        /* arbitrary limit to 9 digits */
        if (v >= 100000000)
            return FALSE;
        v = v * 10 + c - '0';
        p++;
        if (p - p_start == max_digits)
            break;
    }
    if (p - p_start < min_digits)
        return FALSE;
    *pval = v;
    *pp = p;
    return TRUE;
}

BOOL string_get_milliseconds(const uint8_t *sp, int *pp, int *pval) {
    /* parse optional fractional part as milliseconds and truncate. */
    /* spec does not indicate which rounding should be used */
    int mul = 100, ms = 0, c, p_start, p = *pp;

    c = sp[p];
    if (c == '.' || c == ',') {
        p++;
        p_start = p;
        while ((c = sp[p]) >= '0' && c <= '9') {
            ms += (c - '0') * mul;
            mul /= 10;
            p++;
            if (p - p_start == 9)
                break;
        }
        if (p > p_start) {
            /* only consume the separator if digits are present */
            *pval = ms;
            *pp = p;
        }
    }
    return TRUE;
}

uint8_t upper_ascii(uint8_t c) {
    return c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c;
}

BOOL string_get_tzoffset(const uint8_t *sp, int *pp, int *tzp, BOOL strict) {
    int tz = 0, sgn, hh, mm, p = *pp;

    sgn = sp[p++];
    if (sgn == '+' || sgn == '-') {
        int n = p;
        if (!string_get_digits(sp, &p, &hh, 1, 0))
            return FALSE;
        n = p - n;
        if (strict && n != 2 && n != 4)
            return FALSE;
        while (n > 4) {
            n -= 2;
            hh /= 100;
        }
        if (n > 2) {
            mm = hh % 100;
            hh = hh / 100;
        } else {
            mm = 0;
            if (string_skip_char(sp, &p, ':')) {
                /* optional separator */
                if (!string_get_digits(sp, &p, &mm, 2, 2))
                    return FALSE;
            } else {
                if (strict)
                    return FALSE; /* [+-]HH is not accepted in strict mode */
            }
        }
        if (hh > 23 || mm > 59)
            return FALSE;
        tz = hh * 60 + mm;
        if (sgn != '+')
            tz = -tz;
    } else
    if (sgn != 'Z') {
        return FALSE;
    }
    *pp = p;
    *tzp = tz;
    return TRUE;
}

BOOL string_match(const uint8_t *sp, int *pp, const char *s) {
    int p = *pp;
    while (*s != '\0') {
        if (upper_ascii(sp[p]) != upper_ascii(*s++))
            return FALSE;
        p++;
    }
    *pp = p;
    return TRUE;
}

int find_abbrev(const uint8_t *sp, int p, const char *list, int count) {
    int n, i;

    for (n = 0; n < count; n++) {
        for (i = 0;; i++) {
            if (upper_ascii(sp[p + i]) != upper_ascii(list[n * 3 + i]))
                break;
            if (i == 2)
                return n;
        }
    }
    return -1;
}

BOOL string_get_month(const uint8_t *sp, int *pp, int *pval) {
    int n;

    n = find_abbrev(sp, *pp, month_names, 12);
    if (n < 0)
        return FALSE;

    *pval = n + 1;
    *pp += 3;
    return TRUE;
}

/* parse toISOString format */
BOOL js_date_parse_isostring(const uint8_t *sp, int fields[9], BOOL *is_local) {
    int sgn, i, p = 0;

    /* initialize fields to the beginning of the Epoch */
    for (i = 0; i < 9; i++) {
        fields[i] = (i == 2);
    }
    *is_local = FALSE;

    /* year is either yyyy digits or [+-]yyyyyy */
    sgn = sp[p];
    if (sgn == '-' || sgn == '+') {
        p++;
        if (!string_get_digits(sp, &p, &fields[0], 6, 6))
            return FALSE;
        if (sgn == '-') {
            if (fields[0] == 0)
                return FALSE; // reject -000000
            fields[0] = -fields[0];
        }
    } else {
        if (!string_get_digits(sp, &p, &fields[0], 4, 4))
            return FALSE;
    }
    if (string_skip_char(sp, &p, '-')) {
        if (!string_get_digits(sp, &p, &fields[1], 2, 2))  /* month */
            return FALSE;
        if (fields[1] < 1)
            return FALSE;
        fields[1] -= 1;
        if (string_skip_char(sp, &p, '-')) {
            if (!string_get_digits(sp, &p, &fields[2], 2, 2))  /* day */
                return FALSE;
            if (fields[2] < 1)
                return FALSE;
        }
    }
    if (string_skip_char(sp, &p, 'T')) {
        *is_local = TRUE;
        if (!string_get_digits(sp, &p, &fields[3], 2, 2)  /* hour */
        ||  !string_skip_char(sp, &p, ':')
        ||  !string_get_digits(sp, &p, &fields[4], 2, 2)) {  /* minute */
            fields[3] = 100;  // reject unconditionally
            return TRUE;
        }
        if (string_skip_char(sp, &p, ':')) {
            if (!string_get_digits(sp, &p, &fields[5], 2, 2))  /* second */
                return FALSE;
            string_get_milliseconds(sp, &p, &fields[6]);
        }
    }
    /* parse the time zone offset if present: [+-]HH:mm or [+-]HHmm */
    if (sp[p]) {
        *is_local = FALSE;
        if (!string_get_tzoffset(sp, &p, &fields[8], TRUE))
            return FALSE;
    }
    /* error if extraneous characters */
    return sp[p] == '\0';
}

BOOL string_get_tzabbr(const uint8_t *sp, int *pp, int *offset) {
    for (size_t i = 0; i < countof(js_tzabbr); i++) {
        if (string_match(sp, pp, js_tzabbr[i].name)) {
            *offset = js_tzabbr[i].offset;
            return TRUE;
        }
    }
    return FALSE;
}

/* parse toString, toUTCString and other formats */
BOOL js_date_parse_otherstring(const uint8_t *sp,
                                      int fields[minimum_length(9)],
                                      BOOL *is_local) {
    int c, i, val, p = 0, p_start;
    int num[3];
    BOOL has_year = FALSE;
    BOOL has_mon = FALSE;
    BOOL has_time = FALSE;
    int num_index = 0;

    /* initialize fields to the beginning of 2001-01-01 */
    fields[0] = 2001;
    fields[1] = 1;
    fields[2] = 1;
    for (i = 3; i < 9; i++) {
        fields[i] = 0;
    }
    *is_local = TRUE;

    while (string_skip_spaces(sp, &p)) {
        p_start = p;
        if ((c = sp[p]) == '+' || c == '-') {
            if (has_time && string_get_tzoffset(sp, &p, &fields[8], FALSE)) {
                *is_local = FALSE;
            } else {
                p++;
                if (string_get_digits(sp, &p, &val, 1, 0)) {
                    if (c == '-') {
                        if (val == 0)
                            return FALSE;
                        val = -val;
                    }
                    fields[0] = val;
                    has_year = TRUE;
                }
            }
        } else
        if (string_get_digits(sp, &p, &val, 1, 0)) {
            if (string_skip_char(sp, &p, ':')) {
                /* time part */
                fields[3] = val;
                if (!string_get_digits(sp, &p, &fields[4], 1, 2))
                    return FALSE;
                if (string_skip_char(sp, &p, ':')) {
                    if (!string_get_digits(sp, &p, &fields[5], 1, 2))
                        return FALSE;
                    string_get_milliseconds(sp, &p, &fields[6]);
                }
                has_time = TRUE;
                if ((sp[p] == '+' || sp[p] == '-') &&
                    string_get_tzoffset(sp, &p, &fields[8], FALSE)) {
                    *is_local = FALSE;
                }
            } else {
                if (p - p_start > 2 && !has_year) {
                    fields[0] = val;
                    has_year = TRUE;
                } else
                if ((val < 1 || val > 31) && !has_year) {
                    fields[0] = val + (val < 100) * 1900 + (val < 50) * 100;
                    has_year = TRUE;
                } else {
                    if (num_index == 3)
                        return FALSE;
                    num[num_index++] = val;
                }
            }
        } else
        if (string_get_month(sp, &p, &fields[1])) {
            has_mon = TRUE;
            string_skip_until(sp, &p, "0123456789 -/(");
        } else
        if (has_time && string_match(sp, &p, "PM")) {
            if (fields[3] < 12)
                fields[3] += 12;
            continue;
        } else
        if (has_time && string_match(sp, &p, "AM")) {
            if (fields[3] == 12)
                fields[3] -= 12;
            continue;
        } else
        if (string_get_tzabbr(sp, &p, &fields[8])) {
            *is_local = FALSE;
            continue;
        } else
        if (c == '(') {  /* skip parenthesized phrase */
            int level = 0;
            while ((c = sp[p]) != '\0') {
                p++;
                level += (c == '(');
                level -= (c == ')');
                if (!level)
                    break;
            }
            if (level > 0)
                return FALSE;
        } else
        if (c == ')') {
            return FALSE;
        } else {
            if (has_year + has_mon + has_time + num_index)
                return FALSE;
            /* skip a word */
            string_skip_until(sp, &p, " -/(");
        }
        string_skip_separators(sp, &p);
    }
    if (num_index + has_year + has_mon > 3)
        return FALSE;

    switch (num_index) {
    case 0:
        if (!has_year)
            return FALSE;
        break;
    case 1:
        if (has_mon)
            fields[2] = num[0];
        else
            fields[1] = num[0];
        break;
    case 2:
        if (has_year) {
            fields[1] = num[0];
            fields[2] = num[1];
        } else
        if (has_mon) {
            fields[0] = num[1] + (num[1] < 100) * 1900 + (num[1] < 50) * 100;
            fields[2] = num[0];
        } else {
            fields[1] = num[0];
            fields[2] = num[1];
        }
        break;
    case 3:
        fields[0] = num[2] + (num[2] < 100) * 1900 + (num[2] < 50) * 100;
        fields[1] = num[0];
        fields[2] = num[1];
        break;
    default:
        return FALSE;
    }
    if (fields[1] < 1 || fields[2] < 1)
        return FALSE;
    fields[1] -= 1;
    return TRUE;
}

JSValue js_Date_parse(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv)
{
    JSValue s, rv;
    int fields[9];
    double fields1[9];
    double d;
    int i, c;
    JSString *sp;
    uint8_t buf[128];
    BOOL is_local;

#undef countof
#define countof(x) (sizeof(x) / sizeof((x)[0]))
    rv = JS_NAN;

    s = JS_ToString(ctx, argv[0]);
    if (JS_IsException(s))
        return JS_EXCEPTION;

    sp = JS_VALUE_GET_STRING(s);
    /* convert the string as a byte array */
    for (i = 0; i < sp->len && i < (int)countof(buf) - 1; i++) {
        c = string_get(sp, i);
        if (c > 255)
            c = (c == 0x2212) ? '-' : 'x';
        buf[i] = c;
    }
    buf[i] = '\0';
    if (js_date_parse_isostring(buf, fields, &is_local)
    ||  js_date_parse_otherstring(buf, fields, &is_local)) {
        int const field_max[6] = { 0, 11, 31, 24, 59, 59 };
        BOOL valid = TRUE;
        /* check field maximum values */
        for (i = 1; i < 6; i++) {
            if (fields[i] > field_max[i])
                valid = FALSE;
        }
        /* special case 24:00:00.000 */
        if (fields[3] == 24 && (fields[4] | fields[5] | fields[6]))
            valid = FALSE;
        if (valid) {
            for(i = 0; i < 7; i++)
                fields1[i] = fields[i];
            d = set_date_fields(fields1, is_local) - fields[8] * 60000;
            rv = JS_NewFloat64(ctx, d);
        }
    }
    JS_FreeValue(ctx, s);
#undef countof
#define countof(arr) JS_ARRAY_LEN(arr)
    return rv;
}

JSValue js_Date_now(JSContext *ctx, JSValueConst this_val,
                           int argc, JSValueConst *argv)
{
    // now()
    return JS_NewInt64(ctx, date_now());
}

JSValue js_date_Symbol_toPrimitive(JSContext *ctx, JSValueConst this_val,
                                          int argc, JSValueConst *argv)
{
    // Symbol_toPrimitive(hint)
    JSValueConst obj = this_val;
    JSAtom hint = JS_ATOM_NULL;
    int hint_num;

    if (!JS_IsObject(obj))
        return JS_ThrowTypeErrorNotAnObject(ctx);

    if (JS_IsString(argv[0])) {
        hint = JS_ValueToAtom(ctx, argv[0]);
        if (hint == JS_ATOM_NULL)
            return JS_EXCEPTION;
        JS_FreeAtom(ctx, hint);
    }
    switch (hint) {
    case JS_ATOM_number:
    case JS_ATOM_integer:
        hint_num = HINT_NUMBER;
        break;
    case JS_ATOM_string:
    case JS_ATOM_default:
        hint_num = HINT_STRING;
        break;
    default:
        return JS_ThrowTypeError(ctx, "invalid hint");
    }
    return JS_ToPrimitive(ctx, obj, hint_num | HINT_FORCE_ORDINARY);
}

JSValue js_date_getTimezoneOffset(JSContext *ctx, JSValueConst this_val,
                                         int argc, JSValueConst *argv)
{
    // getTimezoneOffset()
    double v;

    if (JS_ThisTimeValue(ctx, &v, this_val))
        return JS_EXCEPTION;
    if (isnan(v))
        return JS_NAN;
    else
        /* assuming -8.64e15 <= v <= -8.64e15 */
        return JS_NewInt64(ctx, getTimezoneOffset((int64_t)trunc(v)));
}

JSValue js_date_getTime(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv)
{
    // getTime()
    double v;

    if (JS_ThisTimeValue(ctx, &v, this_val))
        return JS_EXCEPTION;
    return JS_NewFloat64(ctx, v);
}

JSValue js_date_setTime(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv)
{
    // setTime(v)
    double v;

    if (JS_ThisTimeValue(ctx, &v, this_val) || JS_ToFloat64(ctx, &v, argv[0]))
        return JS_EXCEPTION;
    return JS_SetThisTimeValue(ctx, this_val, time_clip(v));
}

JSValue js_date_setYear(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv)
{
    // setYear(y)
    double y;
    JSValueConst args[1];

    if (JS_ThisTimeValue(ctx, &y, this_val) || JS_ToFloat64(ctx, &y, argv[0]))
        return JS_EXCEPTION;
    y = +y;
    if (isfinite(y)) {
        y = trunc(y);
        if (y >= 0 && y < 100)
            y += 1900;
    }
    args[0] = JS_NewFloat64(ctx, y);
    return set_date_field(ctx, this_val, 1, args, 0x011);
}

JSValue js_date_toJSON(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv)
{
    // toJSON(key)
    JSValue obj, tv, method, rv;
    double d;

    rv = JS_EXCEPTION;
    tv = JS_UNDEFINED;

    obj = JS_ToObject(ctx, this_val);
    tv = JS_ToPrimitive(ctx, obj, HINT_NUMBER);
    if (JS_IsException(tv))
        goto exception;
    if (JS_IsNumber(tv)) {
        if (JS_ToFloat64(ctx, &d, tv) < 0)
            goto exception;
        if (!isfinite(d)) {
            rv = JS_NULL;
            goto done;
        }
    }
    method = JS_GetProperty(ctx, obj, JS_ATOM_toISOString);
    if (JS_IsException(method))
        goto exception;
    if (! JS_IsFunction(ctx, method)) {
        JS_ThrowTypeError(ctx, "object needs toISOString method");
        JS_FreeValue(ctx, method);
        goto exception;
    }
    rv = JS_CallFree(ctx, method, obj, 0, NULL);
exception:
done:
    JS_FreeValue(ctx, obj);
    JS_FreeValue(ctx, tv);
    return rv;
}

/* BigInt */

JSValue JS_ToBigIntCtorFree(JSContext *ctx, JSValue val)
{
    uint32_t tag;

 redo:
    tag = JS_VALUE_GET_NORM_TAG(val);
    switch(tag) {
    case JS_TAG_INT:
    case JS_TAG_BOOL:
        val = JS_NewBigInt64(ctx, JS_VALUE_GET_INT(val));
        break;
    case JS_TAG_SHORT_BIG_INT:
    case JS_TAG_BIG_INT:
        break;
    case JS_TAG_FLOAT64:
        {
            double d = JS_VALUE_GET_FLOAT64(val);
            JSBigInt *r;
            int res;
            r = js_bigint_from_float64(ctx, &res, d);
            if (!r) {
                if (res == 0) {
                    val = JS_EXCEPTION;
                } else if (res == 1) {
                    val = JS_ThrowRangeError(ctx, "cannot convert to BigInt: not an integer");
                } else {
                    val = JS_ThrowRangeError(ctx, "cannot convert NaN or Infinity to BigInt");                }
            } else {
                val = JS_CompactBigInt(ctx, r);
            }
        }
        break;
    case JS_TAG_STRING:
    case JS_TAG_STRING_ROPE:
        val = JS_StringToBigIntErr(ctx, val);
        break;
    case JS_TAG_OBJECT:
        val = JS_ToPrimitiveFree(ctx, val, HINT_NUMBER);
        if (JS_IsException(val))
            break;
        goto redo;
    case JS_TAG_NULL:
    case JS_TAG_UNDEFINED:
    default:
        JS_FreeValue(ctx, val);
        return JS_ThrowTypeError(ctx, "cannot convert to BigInt");
    }
    return val;
}

JSValue js_bigint_constructor(JSContext *ctx,
                                     JSValueConst new_target,
                                     int argc, JSValueConst *argv)
{
    if (!JS_IsUndefined(new_target))
        return JS_ThrowTypeErrorNotAConstructor(ctx, new_target);
    return JS_ToBigIntCtorFree(ctx, JS_DupValue(ctx, argv[0]));
}

JSValue js_thisBigIntValue(JSContext *ctx, JSValueConst this_val)
{
    if (JS_IsBigInt(ctx, this_val))
        return JS_DupValue(ctx, this_val);

    if (JS_VALUE_GET_TAG(this_val) == JS_TAG_OBJECT) {
        JSObject *p = JS_VALUE_GET_OBJ(this_val);
        if (p->class_id == JS_CLASS_BIG_INT) {
            if (JS_IsBigInt(ctx, p->u.object_data))
                return JS_DupValue(ctx, p->u.object_data);
        }
    }
    return JS_ThrowTypeError(ctx, "not a BigInt");
}

JSValue js_bigint_toString(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv)
{
    JSValue val;
    int base;
    JSValue ret;

    val = js_thisBigIntValue(ctx, this_val);
    if (JS_IsException(val))
        return val;
    if (argc == 0 || JS_IsUndefined(argv[0])) {
        base = 10;
    } else {
        base = js_get_radix(ctx, argv[0]);
        if (base < 0)
            goto fail;
    }
    ret = js_bigint_to_string1(ctx, val, base);
    JS_FreeValue(ctx, val);
    return ret;
 fail:
    JS_FreeValue(ctx, val);
    return JS_EXCEPTION;
}

JSValue js_bigint_valueOf(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv)
{
    return js_thisBigIntValue(ctx, this_val);
}

JSValue js_bigint_asUintN(JSContext *ctx,
                                  JSValueConst this_val,
                                  int argc, JSValueConst *argv, int asIntN)
{
    uint64_t bits;
    JSValue res, a;

    if (JS_ToIndex(ctx, &bits, argv[0]))
        return JS_EXCEPTION;
    a = JS_ToBigInt(ctx, argv[1]);
    if (JS_IsException(a))
        return JS_EXCEPTION;
    if (bits == 0) {
        JS_FreeValue(ctx, a);
        res = __JS_NewShortBigInt(ctx, 0);
    } else if (JS_VALUE_GET_TAG(a) == JS_TAG_SHORT_BIG_INT) {
        /* fast case */
        if (bits >= JS_SHORT_BIG_INT_BITS) {
            res = a;
        } else {
            uint64_t v;
            int shift;
            shift = 64 - bits;
            v = JS_VALUE_GET_SHORT_BIG_INT(a);
            v = v << shift;
            if (asIntN)
                v = (int64_t)v >> shift;
            else
                v = v >> shift;
            res = __JS_NewShortBigInt(ctx, v);
        }
    } else {
        JSBigInt *r, *p = JS_VALUE_GET_PTR(a);
        if (bits >= p->len * JS_LIMB_BITS) {
            res = a;
        } else {
            int len, shift, i;
            js_limb_t v;
            len = (bits + JS_LIMB_BITS - 1) / JS_LIMB_BITS;
            r = js_bigint_new(ctx, len);
            if (!r) {
                JS_FreeValue(ctx, a);
                return JS_EXCEPTION;
            }
            r->len = len;
            for(i = 0; i < len - 1; i++)
                r->tab[i] = p->tab[i];
            shift = (-bits) & (JS_LIMB_BITS - 1);
            /* 0 <= shift <= JS_LIMB_BITS - 1 */
            v = p->tab[len - 1] << shift;
            if (asIntN)
                v = (js_slimb_t)v >> shift;
            else
                v = v >> shift;
            r->tab[len - 1] = v;
            r = js_bigint_normalize(ctx, r);
            JS_FreeValue(ctx, a);
            res = JS_CompactBigInt(ctx, r);
        }
    }
    return res;
}

int JS_AddIntrinsicBigInt(JSContext *ctx)
{
    JSValue obj1;

    obj1 = JS_NewCConstructor(ctx, JS_CLASS_BIG_INT, "BigInt",
                                     js_bigint_constructor, 1, JS_CFUNC_constructor_or_func, 0,
                                     JS_UNDEFINED,
                                     js_bigint_funcs, countof(js_bigint_funcs),
                                     js_bigint_proto_funcs, countof(js_bigint_proto_funcs),
                                     0);
    if (JS_IsException(obj1))
        return -1;
    JS_FreeValue(ctx, obj1);
    return 0;
}

/* Minimum amount of objects to be able to compile code and display
   error messages. */
int JS_AddIntrinsicBasicObjects(JSContext *ctx)
{
    JSValue obj;
    JSCFunctionType ft;
    int i;

    JS_LOG("JS_AddIntrinsicBasicObjects", "Entered");

    /* 创建 Object 原型 */
    JS_LOG("JS_AddIntrinsicBasicObjects", "Creating class_proto[JS_CLASS_OBJECT]...");
    ctx->class_proto[JS_CLASS_OBJECT] =
        JS_NewObjectProtoClassAlloc(ctx, JS_NULL, JS_CLASS_OBJECT,
                                    countof(js_object_proto_funcs) + 1);
    if (JS_IsException(ctx->class_proto[JS_CLASS_OBJECT])) {
        JS_LOG("JS_AddIntrinsicBasicObjects", "class_proto[JS_CLASS_OBJECT] failed");
        return -1;
    }
    JS_LOG("JS_AddIntrinsicBasicObjects", "class_proto[JS_CLASS_OBJECT] ok");
    JS_SetImmutablePrototype(ctx, ctx->class_proto[JS_CLASS_OBJECT]);

    /* 创建 Function 原型 */
    {
		JSObject *obj = JS_VALUE_GET_OBJ(ctx->class_proto[JS_CLASS_OBJECT]);
		int ref_before = js_rc(obj)->ref_count;
		JS_LOG("TRACK", "class_proto ref_count before JS_NewCFunction3 = %d", ref_before);
	}
    ctx->function_proto = JS_NewCFunction3(ctx, js_function_proto, "", 0,
                                           JS_CFUNC_generic, 0,
                                           ctx->class_proto[JS_CLASS_OBJECT],
                                           countof(js_function_proto_funcs) + 3 + 2);
    {
		JSObject *obj = JS_VALUE_GET_OBJ(ctx->class_proto[JS_CLASS_OBJECT]);
		int ref_after = js_rc(obj)->ref_count;
		JS_LOG("TRACK", "class_proto ref_count after JS_NewCFunction3 = %d", ref_after);
	}
	
	if (JS_IsException(ctx->function_proto)) {
        JS_LOG("JS_AddIntrinsicBasicObjects", "function_proto failed");
        return -1;
    }
    JS_LOG("JS_AddIntrinsicBasicObjects", "function_proto ok");
    ctx->class_proto[JS_CLASS_BYTECODE_FUNCTION] = JS_DupValue(ctx, ctx->function_proto);

    /* 创建 Global 对象 */
    JS_LOG("JS_AddIntrinsicBasicObjects", "Creating global_obj...");
    ctx->global_obj = JS_NewObjectProtoClassAlloc(ctx, ctx->class_proto[JS_CLASS_OBJECT],
                                                  JS_CLASS_GLOBAL_OBJECT, 64);
    if (JS_IsException(ctx->global_obj)) {
        JS_LOG("JS_AddIntrinsicBasicObjects", "global_obj failed");
        return -1;
    }
    JS_LOG("JS_AddIntrinsicBasicObjects", "global_obj ok");

    {
        JSObject *p;
        obj = JS_NewObjectProtoClassAlloc(ctx, JS_NULL, JS_CLASS_OBJECT, 4);
        p = JS_VALUE_GET_OBJ(ctx->global_obj);
        p->u.global_object.uninitialized_vars = obj;
    }

    /* 创建 Global Var 对象 */
    JS_LOG("JS_AddIntrinsicBasicObjects", "Creating global_var_obj...");
    ctx->global_var_obj = JS_NewObjectProtoClassAlloc(ctx, JS_NULL,
                                                      JS_CLASS_OBJECT, 16);
    if (JS_IsException(ctx->global_var_obj)) {
        JS_LOG("JS_AddIntrinsicBasicObjects", "global_var_obj failed");
        return -1;
    }
    JS_LOG("JS_AddIntrinsicBasicObjects", "global_var_obj ok");

    /* 创建 Error 对象 */
    JS_LOG("JS_AddIntrinsicBasicObjects", "Creating Error constructor...");
    ft.generic_magic = js_error_constructor;
    obj = JS_NewCConstructor(ctx, JS_CLASS_ERROR, "Error",
                                    ft.generic, 1, JS_CFUNC_constructor_or_func_magic, -1,
                                    JS_UNDEFINED,
                                    js_error_funcs, countof(js_error_funcs),
                                    js_error_proto_funcs, countof(js_error_proto_funcs),
                                    0);
    if (JS_IsException(obj)) {
        JS_LOG("JS_AddIntrinsicBasicObjects", "Error constructor failed");
        return -1;
    }
    JS_LOG("JS_AddIntrinsicBasicObjects", "Error constructor ok");

    /* 创建 Native Error 子类 */
    JS_LOG("JS_AddIntrinsicBasicObjects", "Creating native error subclasses...");
    for(i = 0; i < JS_NATIVE_ERROR_COUNT; i++) {
        JSValue func_obj;
        const JSCFunctionListEntry *funcs;
        int n_args;
        char buf[ATOM_GET_STR_BUF_SIZE];

        const char *name = JS_AtomGetStr(ctx, buf, sizeof(buf),
                                         JS_ATOM_EvalError + i);
        n_args = 1 + (i == JS_AGGREGATE_ERROR);
        funcs = js_native_error_proto_funcs + 2 * i;
        func_obj = JS_NewCConstructor(ctx, -1, name,
                                      ft.generic, n_args, JS_CFUNC_constructor_or_func_magic, i,
                                      obj,
                                      NULL, 0,
                                      funcs, 2,
                                      0);
        if (JS_IsException(func_obj)) {
            JS_LOG("JS_AddIntrinsicBasicObjects", "Native error %d ('%s') failed", i, name);
            JS_FreeValue(ctx, obj);
            return -1;
        }
        ctx->native_error_proto[i] = JS_GetProperty(ctx, func_obj, JS_ATOM_prototype);
        JS_FreeValue(ctx, func_obj);
        if (JS_IsException(ctx->native_error_proto[i])) {
            JS_LOG("JS_AddIntrinsicBasicObjects", "Native error %d prototype failed", i);
            JS_FreeValue(ctx, obj);
            return -1;
        }
    }
    JS_LOG("JS_AddIntrinsicBasicObjects", "Native error subclasses ok");
    JS_FreeValue(ctx, obj);

    /* 创建 Array 构造函数 */
    JS_LOG("JS_AddIntrinsicBasicObjects", "Creating Array constructor...");
    obj = JS_NewCConstructor(ctx, JS_CLASS_ARRAY, "Array",
                                    js_array_constructor, 1, JS_CFUNC_constructor_or_func, 0,
                                    JS_UNDEFINED,
                                    js_array_funcs, countof(js_array_funcs),
                                    js_array_proto_funcs, countof(js_array_proto_funcs),
                                    JS_NEW_CTOR_PROTO_CLASS);
    if (JS_IsException(obj)) {
        JS_LOG("JS_AddIntrinsicBasicObjects", "Array constructor failed");
        return -1;
    }
    JS_LOG("JS_AddIntrinsicBasicObjects", "Array constructor ok");
    ctx->array_ctor = obj;

    {
        JSObject *p = JS_VALUE_GET_OBJ(ctx->class_proto[JS_CLASS_ARRAY]);
        p->is_std_array_prototype = TRUE;
    }

    /* 创建 Array shape */
    JS_LOG("JS_AddIntrinsicBasicObjects", "Creating array_shape...");
    ctx->array_shape = js_new_shape2(ctx, get_proto_obj(ctx->class_proto[JS_CLASS_ARRAY]),
                                     JS_PROP_INITIAL_HASH_SIZE, 1);
    if (!ctx->array_shape) {
        JS_LOG("JS_AddIntrinsicBasicObjects", "array_shape failed");
        return -1;
    }
    if (add_shape_property(ctx, &ctx->array_shape, NULL,
                           JS_ATOM_length, JS_PROP_WRITABLE | JS_PROP_LENGTH)) {
        JS_LOG("JS_AddIntrinsicBasicObjects", "add_shape_property length failed");
        return -1;
    }
    JS_LOG("JS_AddIntrinsicBasicObjects", "array_shape ok");

    /* 创建 Arguments shape */
    JS_LOG("JS_AddIntrinsicBasicObjects", "Creating arguments_shape...");
    ctx->arguments_shape = js_new_shape2(ctx, get_proto_obj(ctx->class_proto[JS_CLASS_OBJECT]),
                                         JS_PROP_INITIAL_HASH_SIZE, 3);
    if (!ctx->arguments_shape) {
        JS_LOG("JS_AddIntrinsicBasicObjects", "arguments_shape failed");
        return -1;
    }
    if (add_shape_property(ctx, &ctx->arguments_shape, NULL,
                           JS_ATOM_length, JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE)) {
        JS_LOG("JS_AddIntrinsicBasicObjects", "arguments_shape add length failed");
        return -1;
    }
    if (add_shape_property(ctx, &ctx->arguments_shape, NULL,
                           JS_ATOM_Symbol_iterator, JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE)) {
        JS_LOG("JS_AddIntrinsicBasicObjects", "arguments_shape add iterator failed");
        return -1;
    }
    if (add_shape_property(ctx, &ctx->arguments_shape, NULL,
                           JS_ATOM_callee, JS_PROP_GETSET)) {
        JS_LOG("JS_AddIntrinsicBasicObjects", "arguments_shape add callee failed");
        return -1;
    }
    JS_LOG("JS_AddIntrinsicBasicObjects", "arguments_shape ok");

    /* 创建 Mapped Arguments shape */
    JS_LOG("JS_AddIntrinsicBasicObjects", "Creating mapped_arguments_shape...");
    ctx->mapped_arguments_shape = js_new_shape2(ctx, get_proto_obj(ctx->class_proto[JS_CLASS_OBJECT]),
                                         JS_PROP_INITIAL_HASH_SIZE, 3);
    if (!ctx->mapped_arguments_shape) {
        JS_LOG("JS_AddIntrinsicBasicObjects", "mapped_arguments_shape failed");
        return -1;
    }
    if (add_shape_property(ctx, &ctx->mapped_arguments_shape, NULL,
                           JS_ATOM_length, JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE)) {
        JS_LOG("JS_AddIntrinsicBasicObjects", "mapped_arguments_shape add length failed");
        return -1;
    }
    if (add_shape_property(ctx, &ctx->mapped_arguments_shape, NULL,
                           JS_ATOM_Symbol_iterator, JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE)) {
        JS_LOG("JS_AddIntrinsicBasicObjects", "mapped_arguments_shape add iterator failed");
        return -1;
    }
    if (add_shape_property(ctx, &ctx->mapped_arguments_shape, NULL,
                           JS_ATOM_callee, JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE)) {
        JS_LOG("JS_AddIntrinsicBasicObjects", "mapped_arguments_shape add callee failed");
        return -1;
    }
    JS_LOG("JS_AddIntrinsicBasicObjects", "mapped_arguments_shape ok");

    JS_LOG("JS_AddIntrinsicBasicObjects", "All basic objects created successfully");
    return 0;
}

/* Typed Arrays */

JSValue js_array_buffer_constructor3(JSContext *ctx,
                                            JSValueConst new_target,
                                            uint64_t len, uint64_t *max_len,
                                            JSClassID class_id,
                                            uint8_t *buf,
                                            JSFreeArrayBufferDataFunc *free_func,
                                            void *opaque, BOOL alloc_flag)
{
    JSRuntime *rt = ctx->rt;
    JSValue obj;
    JSArrayBuffer *abuf = NULL;
    uint64_t sab_alloc_len;

    if (!alloc_flag && buf && max_len && free_func != js_array_buffer_free) {
        // not observable from JS land, only through C API misuse;
        // JS code cannot create externally managed buffers directly
        return JS_ThrowInternalError(ctx,
                                     "resizable ArrayBuffers not supported "
                                     "for externally managed buffers");
    }
    obj = js_create_from_ctor(ctx, new_target, class_id);
    if (JS_IsException(obj))
        return obj;
    /* XXX: we are currently limited to 2 GB */
    if (len > INT32_MAX) {
        JS_ThrowRangeError(ctx, "invalid array buffer length");
        goto fail;
    }
    if (max_len && *max_len > INT32_MAX) {
        JS_ThrowRangeError(ctx, "invalid max array buffer length");
        goto fail;
    }
    abuf = js_malloc(ctx, sizeof(*abuf));
    if (!abuf)
        goto fail;
    abuf->byte_length = len;
    abuf->max_byte_length = max_len ? *max_len : -1;
    if (alloc_flag) {
        if (class_id == JS_CLASS_SHARED_ARRAY_BUFFER &&
            rt->sab_funcs.sab_alloc) {
            // TOOD(bnoordhuis) resizing backing memory for SABs atomically
            // is hard so we cheat and allocate |maxByteLength| bytes upfront
            sab_alloc_len = max_len ? *max_len : len;
            abuf->data = rt->sab_funcs.sab_alloc(rt->sab_funcs.sab_opaque,
                                                 max_int(sab_alloc_len, 1));
            if (!abuf->data)
                goto fail;
            memset(abuf->data, 0, sab_alloc_len);
        } else {
            /* the allocation must be done after the object creation */
            abuf->data = js_mallocz(ctx, max_int(len, 1));
            if (!abuf->data)
                goto fail;
        }
    } else {
        if (class_id == JS_CLASS_SHARED_ARRAY_BUFFER &&
            rt->sab_funcs.sab_dup) {
            rt->sab_funcs.sab_dup(rt->sab_funcs.sab_opaque, buf);
        }
        abuf->data = buf;
    }
    init_list_head(&abuf->array_list);
    abuf->detached = FALSE;
    abuf->shared = (class_id == JS_CLASS_SHARED_ARRAY_BUFFER);
    abuf->opaque = opaque;
    abuf->free_func = free_func;
    if (alloc_flag && buf)
        memcpy(abuf->data, buf, len);
    JS_SetOpaque(obj, abuf);
    return obj;
 fail:
    JS_FreeValue(ctx, obj);
    js_free(ctx, abuf);
    return JS_EXCEPTION;
}

void js_array_buffer_free(JSRuntime *rt, void *opaque, void *ptr)
{
    js_free_rt(rt, ptr);
}

JSValue js_array_buffer_constructor2(JSContext *ctx,
                                            JSValueConst new_target,
                                            uint64_t len, uint64_t *max_len,
                                            JSClassID class_id)
{
    return js_array_buffer_constructor3(ctx, new_target, len, max_len, class_id,
                                        NULL, js_array_buffer_free, NULL,
                                        TRUE);
}

JSValue js_array_buffer_constructor1(JSContext *ctx,
                                            JSValueConst new_target,
                                            uint64_t len, uint64_t *max_len)
{
    return js_array_buffer_constructor2(ctx, new_target, len, max_len,
                                        JS_CLASS_ARRAY_BUFFER);
}

JSValue js_array_buffer_constructor0(JSContext *ctx, JSValueConst new_target,
                                            int argc, JSValueConst *argv,
                                            JSClassID class_id)
 {
    uint64_t len, max_len, *pmax_len = NULL;
    JSValue obj, val;
    int64_t i;

     if (JS_ToIndex(ctx, &len, argv[0]))
         return JS_EXCEPTION;
    if (argc < 2)
        goto next;
    if (!JS_IsObject(argv[1]))
        goto next;
    obj = JS_ToObject(ctx, argv[1]);
    if (JS_IsException(obj))
        return JS_EXCEPTION;
    val = JS_GetProperty(ctx, obj, JS_ATOM_maxByteLength);
    JS_FreeValue(ctx, obj);
    if (JS_IsException(val))
        return JS_EXCEPTION;
    if (JS_IsUndefined(val))
        goto next;
    if (JS_ToInt64Free(ctx, &i, val))
        return JS_EXCEPTION;
    // don't have to check i < 0 because len >= 0
    if (len > i || i > MAX_SAFE_INTEGER)
        return JS_ThrowRangeError(ctx, "invalid array buffer max length");
    max_len = i;
    pmax_len = &max_len;
next:
    return js_array_buffer_constructor2(ctx, new_target, len, pmax_len,
                                        class_id);
}

JSValue js_array_buffer_constructor(JSContext *ctx,
                                           JSValueConst new_target,
                                           int argc, JSValueConst *argv)
{
    return js_array_buffer_constructor0(ctx, new_target, argc, argv,
                                        JS_CLASS_ARRAY_BUFFER);
}

JSValue js_shared_array_buffer_constructor(JSContext *ctx,
                                                  JSValueConst new_target,
                                                  int argc, JSValueConst *argv)
{
    return js_array_buffer_constructor0(ctx, new_target, argc, argv,
                                        JS_CLASS_SHARED_ARRAY_BUFFER);
}

/* also used for SharedArrayBuffer */
void js_array_buffer_finalizer(JSRuntime *rt, JSValue val)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JSArrayBuffer *abuf = p->u.array_buffer;
    struct list_head *el, *el1;

    if (abuf) {
        /* The ArrayBuffer finalizer may be called before the typed
           array finalizers using it, so abuf->array_list is not
           necessarily empty. */
        list_for_each_safe(el, el1, &abuf->array_list) {
            JSTypedArray *ta;
            JSObject *p1;

            ta = list_entry(el, JSTypedArray, link);
            ta->link.prev = NULL;
            ta->link.next = NULL;
            p1 = ta->obj;
            /* Note: the typed array length and offset fields are not modified */
            if (p1->class_id != JS_CLASS_DATAVIEW) {
                p1->u.array.count = 0;
                p1->u.array.u.ptr = NULL;
            }
        }
        if (abuf->shared && rt->sab_funcs.sab_free) {
            rt->sab_funcs.sab_free(rt->sab_funcs.sab_opaque, abuf->data);
        } else {
            if (abuf->free_func)
                abuf->free_func(rt, abuf->opaque, abuf->data);
        }
        js_free_rt(rt, abuf);
    }
}

JSValue js_array_buffer_isView(JSContext *ctx,
                                      JSValueConst this_val,
                                      int argc, JSValueConst *argv)
{
    JSObject *p;
    BOOL res;
    res = FALSE;
    if (JS_VALUE_GET_TAG(argv[0]) == JS_TAG_OBJECT) {
        p = JS_VALUE_GET_OBJ(argv[0]);
        if (p->class_id >= JS_CLASS_UINT8C_ARRAY &&
            p->class_id <= JS_CLASS_DATAVIEW) {
            res = TRUE;
        }
    }
    return JS_NewBool(ctx, res);
}

JSValue JS_ThrowTypeErrorDetachedArrayBuffer(JSContext *ctx)
{
    return JS_ThrowTypeError(ctx, "ArrayBuffer is detached");
}

JSValue JS_ThrowTypeErrorArrayBufferOOB(JSContext *ctx)
{
    return JS_ThrowTypeError(ctx, "ArrayBuffer is detached or resized");
}

// #sec-get-arraybuffer.prototype.detached
JSValue js_array_buffer_get_detached(JSContext *ctx,
                                                 JSValueConst this_val)
{
    JSArrayBuffer *abuf = JS_GetOpaque2(ctx, this_val, JS_CLASS_ARRAY_BUFFER);
    if (!abuf)
        return JS_EXCEPTION;
    if (abuf->shared)
        return JS_ThrowTypeError(ctx, "detached called on SharedArrayBuffer");
    return JS_NewBool(ctx, abuf->detached);
}

JSValue js_array_buffer_get_byteLength(JSContext *ctx,
                                              JSValueConst this_val,
                                              int class_id)
{
    JSArrayBuffer *abuf = JS_GetOpaque2(ctx, this_val, class_id);
    if (!abuf)
        return JS_EXCEPTION;
    /* return 0 if detached */
    return JS_NewUint32(ctx, abuf->byte_length);
}

JSValue js_array_buffer_get_maxByteLength(JSContext *ctx,
                                                 JSValueConst this_val,
                                                 int class_id)
{
    JSArrayBuffer *abuf = JS_GetOpaque2(ctx, this_val, class_id);
    if (!abuf)
        return JS_EXCEPTION;
    if (array_buffer_is_resizable(abuf))
        return JS_NewUint32(ctx, abuf->max_byte_length);
    return JS_NewUint32(ctx, abuf->byte_length);
}

JSValue js_array_buffer_get_resizable(JSContext *ctx,
                                             JSValueConst this_val,
                                             int class_id)
{
    JSArrayBuffer *abuf = JS_GetOpaque2(ctx, this_val, class_id);
    if (!abuf)
        return JS_EXCEPTION;
    return JS_NewBool(ctx, array_buffer_is_resizable(abuf));
}

void js_array_buffer_update_typed_arrays(JSArrayBuffer *abuf)
{
    uint32_t size_log2, size_elem;
    struct list_head *el;
    JSTypedArray *ta;
    JSObject *p;
    uint8_t *data;
    int64_t len;

    len = abuf->byte_length;
    data = abuf->data;
    // update lengths of all typed arrays backed by this array buffer
    list_for_each(el, &abuf->array_list) {
        ta = list_entry(el, JSTypedArray, link);
        p = ta->obj;
        if (p->class_id == JS_CLASS_DATAVIEW) {
            if (ta->track_rab) {
                if (ta->offset < len)
                    ta->length = len - ta->offset;
                else
                    ta->length = 0;
            }
        } else {
            p->u.array.count = 0;
            p->u.array.u.ptr = NULL;
            size_log2 = typed_array_size_log2(p->class_id);
            size_elem = 1 << size_log2;
            if (ta->track_rab) {
                if (len >= (int64_t)ta->offset + size_elem) {
                    p->u.array.count = (len - ta->offset) >> size_log2;
                    p->u.array.u.ptr = &data[ta->offset];
                }
            } else {
                if (len >= (int64_t)ta->offset + ta->length) {
                    p->u.array.count = ta->length >> size_log2;
                    p->u.array.u.ptr = &data[ta->offset];
                }
            }
        }
    }

}

/* get an ArrayBuffer or SharedArrayBuffer */
JSArrayBuffer *js_get_array_buffer(JSContext *ctx, JSValueConst obj)
{
    JSObject *p;
    if (JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT)
        goto fail;
    p = JS_VALUE_GET_OBJ(obj);
    if (p->class_id != JS_CLASS_ARRAY_BUFFER &&
        p->class_id != JS_CLASS_SHARED_ARRAY_BUFFER) {
    fail:
        JS_ThrowTypeErrorInvalidClass(ctx, JS_CLASS_ARRAY_BUFFER);
        return NULL;
    }
    return p->u.array_buffer;
}

BOOL array_buffer_is_resizable(const JSArrayBuffer *abuf)
{
    return abuf->max_byte_length >= 0;
}

// ES #sec-arraybuffer.prototype.transfer
JSValue js_array_buffer_transfer(JSContext *ctx,
                                        JSValueConst this_val,
                                        int argc, JSValueConst *argv,
                                        int transfer_to_fixed_length)
{
    JSArrayBuffer *abuf;
    uint64_t new_len, *pmax_len, max_len;
    JSValue res;

    abuf = JS_GetOpaque2(ctx, this_val, JS_CLASS_ARRAY_BUFFER);
    if (!abuf)
        return JS_EXCEPTION;
    if (abuf->shared)
        return JS_ThrowTypeError(ctx, "cannot transfer a SharedArrayBuffer");
    if (argc < 1 || JS_IsUndefined(argv[0]))
        new_len = abuf->byte_length;
    else if (JS_ToIndex(ctx, &new_len, argv[0]))
        return JS_EXCEPTION;
    if (abuf->detached)
        return JS_ThrowTypeErrorDetachedArrayBuffer(ctx);
    pmax_len = NULL;
    if (!transfer_to_fixed_length) {
        if (array_buffer_is_resizable(abuf)) { // carry over maxByteLength
            max_len = abuf->max_byte_length;
            if (new_len > max_len)
                return JS_ThrowTypeError(ctx, "invalid array buffer length");
            // TODO(bnoordhuis) support externally managed RABs
            if (abuf->free_func == js_array_buffer_free)
                pmax_len = &max_len;
        }
    }

    /* create an empty AB */
    if (new_len == 0) {
        res = js_array_buffer_constructor2(ctx, JS_UNDEFINED, 0, pmax_len, JS_CLASS_ARRAY_BUFFER);
        if (JS_IsException(res))
            return res;
        JS_DetachArrayBuffer(ctx, this_val);
    } else {
        uint64_t old_len;

        old_len = abuf->byte_length;

        /* if length mismatch, realloc. Otherwise, use the same backing buffer. */
        if (new_len != old_len) {
            /* XXX: we are currently limited to 2 GB */
            if (new_len > INT32_MAX)
                return JS_ThrowRangeError(ctx, "invalid array buffer length");

            if (abuf->free_func != js_array_buffer_free) {
                JSArrayBuffer *new_abuf;
                /* cannot use js_realloc() because the buffer was
                   allocated with a custom allocator */
                res = js_array_buffer_constructor2(ctx, JS_UNDEFINED, new_len, pmax_len, JS_CLASS_ARRAY_BUFFER);
                if (JS_IsException(res))
                    return res;
                new_abuf = JS_GetOpaque2(ctx, res, JS_CLASS_ARRAY_BUFFER);
                memcpy(new_abuf->data, abuf->data, min_int(old_len, new_len));
                abuf->free_func(ctx->rt, abuf->opaque, abuf->data);
            } else {
                JSArrayBuffer *new_abuf;
                uint8_t *new_bs;
                /* reallocate the buffer after the new array buffer is
                   created in case the new array buffer creation
                   fails. */
                res = js_array_buffer_constructor2(ctx, JS_UNDEFINED, 0, pmax_len, JS_CLASS_ARRAY_BUFFER);
                if (JS_IsException(res))
                    return res;
                new_bs = js_realloc(ctx, abuf->data, new_len);
                if (!new_bs) {
                    JS_FreeValue(ctx, res);
                    return JS_EXCEPTION;
                }
                if (new_len > old_len)
                    memset(new_bs + old_len, 0, new_len - old_len);
                new_abuf = JS_GetOpaque2(ctx, res, JS_CLASS_ARRAY_BUFFER);
                js_free(ctx, new_abuf->data);
                new_abuf->data = new_bs;
                new_abuf->byte_length = new_len;
            }
        } else {
            /* can keep the custom free function */
            res = js_array_buffer_constructor3(ctx, JS_UNDEFINED, new_len, pmax_len,
                                               JS_CLASS_ARRAY_BUFFER,
                                               abuf->data, abuf->free_func,
                                               abuf->opaque, FALSE);
            if (JS_IsException(res))
                return res;
        }
        /* neuter the backing buffer */
        abuf->data = NULL;
        abuf->byte_length = 0;
        abuf->detached = TRUE;
        js_array_buffer_update_typed_arrays(abuf);
    }
    return res;
}
