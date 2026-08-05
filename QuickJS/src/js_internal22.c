#include "js_internal.h"



/* SharedArrayBuffer */

JSObject *get_typed_array(JSContext *ctx, JSValueConst this_val)
{
    JSObject *p;
    if (JS_VALUE_GET_TAG(this_val) != JS_TAG_OBJECT)
        goto fail;
    p = JS_VALUE_GET_OBJ(this_val);
    if (!(p->class_id >= JS_CLASS_UINT8C_ARRAY &&
          p->class_id <= JS_CLASS_FLOAT64_ARRAY)) {
    fail:
        JS_ThrowTypeError(ctx, "not a TypedArray");
        return NULL;
    }
    return p;
}

// is the typed array detached or out of bounds relative to its RAB?
// |p| must be a typed array, *not* a DataView
BOOL typed_array_is_oob(JSObject *p)
{
    JSArrayBuffer *abuf;
    JSTypedArray *ta;
    int len, size_elem;
    int64_t end;

    assert(p->class_id >= JS_CLASS_UINT8C_ARRAY);
    assert(p->class_id <= JS_CLASS_FLOAT64_ARRAY);

    ta = p->u.typed_array;
    abuf = ta->buffer->u.array_buffer;
    if (abuf->detached)
        return TRUE;
    len = abuf->byte_length;
    if (ta->offset > len)
        return TRUE;
    if (ta->track_rab)
        return FALSE;
    if (len < (int64_t)ta->offset + ta->length)
        return TRUE;
    size_elem = 1 << typed_array_size_log2(p->class_id);
    end = (int64_t)ta->offset + (int64_t)p->u.array.count * size_elem;
    return end > len;
}

// Be *very* careful if you touch the typed array's memory directly:
// the length is only valid until the next call into JS land because
// JS code can detach or resize the backing array buffer. Functions
// like JS_GetProperty and JS_ToIndex call JS code.
//
// Exclusively reading or writing elements with JS_GetProperty,
// JS_GetPropertyInt64, JS_SetProperty, etc. is safe because they
// perform bounds checks, as does js_get_fast_array_element.
int js_typed_array_get_length_unsafe(JSContext *ctx, JSValueConst obj)
{
    JSObject *p;
    p = get_typed_array(ctx, obj);
    if (!p)
        return -1;
    if (typed_array_is_oob(p)) {
        JS_ThrowTypeErrorArrayBufferOOB(ctx);
        return -1;
    }
    return p->u.array.count;
}

int validate_typed_array(JSContext *ctx, JSValueConst this_val)
{
    JSObject *p;
    p = get_typed_array(ctx, this_val);
    if (!p)
        return -1;
    if (typed_array_is_oob(p)) {
        JS_ThrowTypeErrorArrayBufferOOB(ctx);
        return -1;
    }
    return 0;
}

JSValue js_typed_array_get_length(JSContext *ctx,
                                         JSValueConst this_val)
{
    JSObject *p;
    p = get_typed_array(ctx, this_val);
    if (!p)
        return JS_EXCEPTION;
    return JS_NewInt32(ctx, p->u.array.count);
}

JSValue js_typed_array_get_buffer(JSContext *ctx,
                                         JSValueConst this_val)
{
    JSObject *p;
    JSTypedArray *ta;
    p = get_typed_array(ctx, this_val);
    if (!p)
        return JS_EXCEPTION;
    ta = p->u.typed_array;
    return JS_DupValue(ctx, JS_MKPTR(JS_TAG_OBJECT, ta->buffer));
}

JSValue js_typed_array_get_byteLength(JSContext *ctx,
                                             JSValueConst this_val)
{
    JSObject *p;
    JSTypedArray *ta;
    int size_log2;

    p = get_typed_array(ctx, this_val);
    if (!p)
        return JS_EXCEPTION;
    if (typed_array_is_oob(p))
        return JS_NewInt32(ctx, 0);
    ta = p->u.typed_array;
    if (!ta->track_rab)
        return JS_NewUint32(ctx, ta->length);
    size_log2 = typed_array_size_log2(p->class_id);
    return JS_NewInt64(ctx, (int64_t)p->u.array.count << size_log2);
}

JSValue js_typed_array_get_byteOffset(JSContext *ctx,
                                             JSValueConst this_val)
{
    JSObject *p;
    JSTypedArray *ta;
    p = get_typed_array(ctx, this_val);
    if (!p)
        return JS_EXCEPTION;
    if (typed_array_is_oob(p))
        return JS_NewInt32(ctx, 0);
    ta = p->u.typed_array;
    return JS_NewUint32(ctx, ta->offset);
}

JSValue js_typed_array_get_toStringTag(JSContext *ctx,
                                              JSValueConst this_val)
{
    JSObject *p;
    if (JS_VALUE_GET_TAG(this_val) != JS_TAG_OBJECT)
        return JS_UNDEFINED;
    p = JS_VALUE_GET_OBJ(this_val);
    if (!(p->class_id >= JS_CLASS_UINT8C_ARRAY &&
          p->class_id <= JS_CLASS_FLOAT64_ARRAY))
        return JS_UNDEFINED;
    return JS_AtomToString(ctx, ctx->rt->class_array[p->class_id].class_name);
}

JSValue js_typed_array_set_internal(JSContext *ctx,
                                           JSValueConst dst,
                                           JSValueConst src,
                                           JSValueConst off)
{
    JSObject *p;
    JSObject *src_p;
    uint32_t i;
    int64_t dst_len, src_len, offset;
    JSValue val, src_obj = JS_UNDEFINED;

    p = get_typed_array(ctx, dst);
    if (!p)
        goto fail;
    if (JS_ToInt64Sat(ctx, &offset, off))
        goto fail;
    if (offset < 0)
        goto range_error;
    if (typed_array_is_oob(p)) {
    detached:
        JS_ThrowTypeErrorArrayBufferOOB(ctx);
        goto fail;
    }
    dst_len = p->u.array.count;
    src_obj = JS_ToObject(ctx, src);
    if (JS_IsException(src_obj))
        goto fail;
    src_p = JS_VALUE_GET_OBJ(src_obj);
    if (src_p->class_id >= JS_CLASS_UINT8C_ARRAY &&
        src_p->class_id <= JS_CLASS_FLOAT64_ARRAY) {
        JSTypedArray *dest_ta = p->u.typed_array;
        JSArrayBuffer *dest_abuf = dest_ta->buffer->u.array_buffer;
        JSTypedArray *src_ta = src_p->u.typed_array;
        JSArrayBuffer *src_abuf = src_ta->buffer->u.array_buffer;
        int shift = typed_array_size_log2(p->class_id);

        if (typed_array_is_oob(src_p))
            goto detached;

        src_len = src_p->u.array.count;
        if (offset > dst_len - src_len)
            goto range_error;

        /* copying between typed objects */
        if (src_p->class_id == p->class_id) {
            /* same type, use memmove */
            memmove(dest_abuf->data + dest_ta->offset + (offset << shift),
                    src_abuf->data + src_ta->offset, src_len << shift);
            goto done;
        }
        if (dest_abuf->data == src_abuf->data) {
            /* copying between the same buffer using different types of mappings
               would require a temporary buffer */
        }
        /* otherwise, default behavior is slow but correct */
    } else {
        // can change |dst| as a side effect; per spec,
        // perform the range check against its old length
        if (js_get_length64(ctx, &src_len, src_obj))
            goto fail;
        if (offset > dst_len - src_len) {
        range_error:
            JS_ThrowRangeError(ctx, "invalid array length");
            goto fail;
        }
    }
    for(i = 0; i < src_len; i++) {
        val = JS_GetPropertyUint32(ctx, src_obj, i);
        if (JS_IsException(val))
            goto fail;
        if (JS_SetPropertyUint32(ctx, dst, offset + i, val) < 0)
            goto fail;
    }
done:
    JS_FreeValue(ctx, src_obj);
    return JS_UNDEFINED;
fail:
    JS_FreeValue(ctx, src_obj);
    return JS_EXCEPTION;
}

JSValue js_typed_array_at(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv)
{
    JSObject *p;
    int64_t idx, len;

    p = get_typed_array(ctx, this_val);
    if (!p)
        return JS_EXCEPTION;

    if (typed_array_is_oob(p))
        return JS_ThrowTypeErrorDetachedArrayBuffer(ctx);
    len = p->u.array.count;

    // note: can change p->u.array.count
    if (JS_ToInt64Sat(ctx, &idx, argv[0]))
        return JS_EXCEPTION;

    if (idx < 0)
        idx = len + idx;

    len = p->u.array.count;
    if (idx < 0 || idx >= len)
        return JS_UNDEFINED;
    return JS_GetPropertyInt64(ctx, this_val, idx);
}

JSValue js_typed_array_with(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv)
{
    JSValue arr, val;
    JSObject *p;
    int64_t idx, len;

    p = get_typed_array(ctx, this_val);
    if (!p)
        return JS_EXCEPTION;
    if (typed_array_is_oob(p))
        return JS_ThrowTypeErrorDetachedArrayBuffer(ctx);

    len = p->u.array.count;
    if (JS_ToInt64Sat(ctx, &idx, argv[0]))
        return JS_EXCEPTION;

    if (idx < 0)
        idx = len + idx;

    val = JS_ToPrimitive(ctx, argv[1], HINT_NUMBER);
    if (JS_IsException(val))
        return JS_EXCEPTION;

    if (typed_array_is_oob(p) || idx < 0 || idx >= p->u.array.count)
        return JS_ThrowRangeError(ctx, "invalid array index");

    /* warning: 'this_val' may have been resized, so 'len' may be
       larger than its length */
    arr = js_typed_array_constructor_ta(ctx, JS_UNDEFINED, this_val,
                                        p->class_id, len);
    if (JS_IsException(arr)) {
        JS_FreeValue(ctx, val);
        return JS_EXCEPTION;
    }
    if (JS_SetPropertyInt64(ctx, arr, idx, val) < 0) {
        JS_FreeValue(ctx, arr);
        return JS_EXCEPTION;
    }
    return arr;
}

JSValue js_typed_array_set(JSContext *ctx,
                                  JSValueConst this_val,
                                  int argc, JSValueConst *argv)
{
    JSValueConst offset = JS_UNDEFINED;
    if (argc > 1) {
        offset = argv[1];
    }
    return js_typed_array_set_internal(ctx, this_val, argv[0], offset);
}

JSValue js_create_typed_array_iterator(JSContext *ctx, JSValueConst this_val,
                                              int argc, JSValueConst *argv, int magic)
{
    if (validate_typed_array(ctx, this_val))
        return JS_EXCEPTION;
    return js_create_array_iterator(ctx, this_val, argc, argv, magic);
}

JSValue js_typed_array_create(JSContext *ctx, JSValueConst ctor,
                                     int argc, JSValueConst *argv)
{
    JSValue ret;
    int new_len;
    int64_t len;

    ret = JS_CallConstructor(ctx, ctor, argc, argv);
    if (JS_IsException(ret))
        return ret;
    /* validate the typed array */
    new_len = js_typed_array_get_length_unsafe(ctx, ret);
    if (new_len < 0)
        goto fail;
    if (argc == 1) {
        /* ensure that it is large enough */
        if (JS_ToLengthFree(ctx, &len, JS_DupValue(ctx, argv[0])))
            goto fail;
        if (new_len < len) {
            JS_ThrowTypeError(ctx, "TypedArray length is too small");
        fail:
            JS_FreeValue(ctx, ret);
            return JS_EXCEPTION;
        }
    }
    return ret;
}

#if 0
JSValue js_typed_array___create(JSContext *ctx,
                                       JSValueConst this_val,
                                       int argc, JSValueConst *argv)
{
    return js_typed_array_create(ctx, argv[0], max_int(argc - 1, 0), argv + 1);
}
#endif

JSValue js_typed_array___speciesCreate(JSContext *ctx,
                                              JSValueConst this_val,
                                              int argc, JSValueConst *argv)
{
    JSValueConst obj;
    JSObject *p;
    JSValue ctor, ret;
    int argc1;

    obj = argv[0];
    p = get_typed_array(ctx, obj);
    if (!p)
        return JS_EXCEPTION;
    ctor = JS_SpeciesConstructor(ctx, obj, JS_UNDEFINED);
    if (JS_IsException(ctor))
        return ctor;
    argc1 = max_int(argc - 1, 0);
    if (JS_IsUndefined(ctor)) {
        ret = js_typed_array_constructor(ctx, JS_UNDEFINED, argc1, argv + 1,
                                         p->class_id);
    } else {
        ret = js_typed_array_create(ctx, ctor, argc1, argv + 1);
        JS_FreeValue(ctx, ctor);
    }
    return ret;
}

JSValue js_typed_array_from(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv)
{
    // from(items, mapfn = void 0, this_arg = void 0)
    JSValueConst items = argv[0], mapfn, this_arg;
    JSValueConst args[2];
    JSValue iter, arr, r, v, v2;
    int64_t k, len;
    int mapping;

    mapping = FALSE;
    mapfn = JS_UNDEFINED;
    this_arg = JS_UNDEFINED;
    r = JS_UNDEFINED;
    arr = JS_UNDEFINED;
    iter = JS_UNDEFINED;

    if (argc > 1) {
        mapfn = argv[1];
        if (!JS_IsUndefined(mapfn)) {
            if (check_function(ctx, mapfn))
                goto exception;
            mapping = 1;
            if (argc > 2)
                this_arg = argv[2];
        }
    }
    iter = JS_GetProperty(ctx, items, JS_ATOM_Symbol_iterator);
    if (JS_IsException(iter))
        goto exception;
    if (!JS_IsUndefined(iter) && !JS_IsNull(iter)) {
        uint32_t len1;
        if (! JS_IsFunction(ctx, iter)) {
            JS_ThrowTypeError(ctx, "value is not iterable");
            goto exception;
        }
        arr = js_array_from_iterator(ctx, &len1, items, iter);
        if (JS_IsException(arr))
            goto exception;
        len = len1;
    } else {
        arr = JS_ToObject(ctx, items);
        if (JS_IsException(arr))
            goto exception;
        if (js_get_length64(ctx, &len, arr) < 0)
            goto exception;
    }
    v = JS_NewInt64(ctx, len);
    args[0] = v;
    r = js_typed_array_create(ctx, this_val, 1, args);
    JS_FreeValue(ctx, v);
    if (JS_IsException(r))
        goto exception;
    for(k = 0; k < len; k++) {
        v = JS_GetPropertyInt64(ctx, arr, k);
        if (JS_IsException(v))
            goto exception;
        if (mapping) {
            args[0] = v;
            args[1] = JS_NewInt32(ctx, k);
            v2 = JS_Call(ctx, mapfn, this_arg, 2, args);
            JS_FreeValue(ctx, v);
            v = v2;
            if (JS_IsException(v))
                goto exception;
        }
        if (JS_SetPropertyInt64(ctx, r, k, v) < 0)
            goto exception;
    }
    goto done;
 exception:
    JS_FreeValue(ctx, r);
    r = JS_EXCEPTION;
 done:
    JS_FreeValue(ctx, arr);
    JS_FreeValue(ctx, iter);
    return r;
}

JSValue js_typed_array_of(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv)
{
    JSValue obj;
    JSValueConst args[1];
    int i;

    args[0] = JS_NewInt32(ctx, argc);
    obj = js_typed_array_create(ctx, this_val, 1, args);
    if (JS_IsException(obj))
        return obj;

    for(i = 0; i < argc; i++) {
        if (JS_SetPropertyUint32(ctx, obj, i, JS_DupValue(ctx, argv[i])) < 0) {
            JS_FreeValue(ctx, obj);
            return JS_EXCEPTION;
        }
    }
    return obj;
}

JSValue js_typed_array_copyWithin(JSContext *ctx, JSValueConst this_val,
                                         int argc, JSValueConst *argv)
{
    JSObject *p;
    int len, to, from, final, count, shift, space;

    p = get_typed_array(ctx, this_val);
    if (!p)
        return JS_EXCEPTION;
    if (typed_array_is_oob(p))
        return JS_ThrowTypeErrorArrayBufferOOB(ctx);
    len = p->u.array.count;

    if (JS_ToInt32Clamp(ctx, &to, argv[0], 0, len, len))
        return JS_EXCEPTION;

    if (JS_ToInt32Clamp(ctx, &from, argv[1], 0, len, len))
        return JS_EXCEPTION;

    final = len;
    if (argc > 2 && !JS_IsUndefined(argv[2])) {
        if (JS_ToInt32Clamp(ctx, &final, argv[2], 0, len, len))
            return JS_EXCEPTION;
    }

    if (typed_array_is_oob(p))
        return JS_ThrowTypeErrorArrayBufferOOB(ctx);

    // RAB may have been resized by evil .valueOf method
    space = p->u.array.count - max_int(to, from);
    count = min_int(final - from, len - to);
    count = min_int(count, space);
    if (count > 0) {
        shift = typed_array_size_log2(p->class_id);
        memmove(p->u.array.u.uint8_ptr + (to << shift),
                p->u.array.u.uint8_ptr + (from << shift),
                count << shift);
    }
    return JS_DupValue(ctx, this_val);
}

JSValue js_typed_array_fill(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv)
{
    JSObject *p;
    int len, k, final, shift;
    uint64_t v64;

    p = get_typed_array(ctx, this_val);
    if (!p)
        return JS_EXCEPTION;
    if (typed_array_is_oob(p))
        return JS_ThrowTypeErrorArrayBufferOOB(ctx);
    len = p->u.array.count;

    if (p->class_id == JS_CLASS_UINT8C_ARRAY) {
        int32_t v;
        if (JS_ToUint8ClampFree(ctx, &v, JS_DupValue(ctx, argv[0])))
            return JS_EXCEPTION;
        v64 = v;
    } else if (p->class_id <= JS_CLASS_UINT32_ARRAY) {
        uint32_t v;
        if (JS_ToUint32(ctx, &v, argv[0]))
            return JS_EXCEPTION;
        v64 = v;
    } else if (p->class_id <= JS_CLASS_BIG_UINT64_ARRAY) {
        if (JS_ToBigInt64(ctx, (int64_t *)&v64, argv[0]))
            return JS_EXCEPTION;
    } else {
        double d;
        if (JS_ToFloat64(ctx, &d, argv[0]))
            return JS_EXCEPTION;
        if (p->class_id == JS_CLASS_FLOAT16_ARRAY) {
            v64 = tofp16(d);
        } else if (p->class_id == JS_CLASS_FLOAT32_ARRAY) {
            union {
                float f;
                uint32_t u32;
            } u;
            u.f = d;
            v64 = u.u32;
        } else {
            JSFloat64Union u;
            u.d = d;
            v64 = u.u64;
        }
    }

    k = 0;
    if (argc > 1) {
        if (JS_ToInt32Clamp(ctx, &k, argv[1], 0, len, len))
            return JS_EXCEPTION;
    }

    final = len;
    if (argc > 2 && !JS_IsUndefined(argv[2])) {
        if (JS_ToInt32Clamp(ctx, &final, argv[2], 0, len, len))
            return JS_EXCEPTION;
    }

    if (typed_array_is_oob(p))
        return JS_ThrowTypeErrorArrayBufferOOB(ctx);

    // RAB may have been resized by evil .valueOf method
    final = min_int(final, p->u.array.count);
    shift = typed_array_size_log2(p->class_id);
    switch(shift) {
    case 0:
        if (k < final) {
            memset(p->u.array.u.uint8_ptr + k, v64, final - k);
        }
        break;
    case 1:
        for(; k < final; k++) {
            p->u.array.u.uint16_ptr[k] = v64;
        }
        break;
    case 2:
        for(; k < final; k++) {
            p->u.array.u.uint32_ptr[k] = v64;
        }
        break;
    case 3:
        for(; k < final; k++) {
            p->u.array.u.uint64_ptr[k] = v64;
        }
        break;
    default:
        abort();
    }
    return JS_DupValue(ctx, this_val);
}

JSValue js_typed_array_find(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv, int mode)
{
    JSValueConst func, this_arg;
    JSValueConst args[3];
    JSValue val, index_val, res;
    int len, k, end;
    int dir;

    val = JS_UNDEFINED;
    len = js_typed_array_get_length_unsafe(ctx, this_val);
    if (len < 0)
        goto exception;

    func = argv[0];
    if (check_function(ctx, func))
        goto exception;

    this_arg = JS_UNDEFINED;
    if (argc > 1)
        this_arg = argv[1];

    k = 0;
    dir = 1;
    end = len;
    if (mode == ArrayFindLast || mode == ArrayFindLastIndex) {
        k = len - 1;
        dir = -1;
        end = -1;
    }

    for(; k != end; k += dir) {
        index_val = JS_NewInt32(ctx, k);
        val = JS_GetPropertyValue(ctx, this_val, index_val);
        if (JS_IsException(val))
            goto exception;
        args[0] = val;
        args[1] = index_val;
        args[2] = this_val;
        res = JS_Call(ctx, func, this_arg, 3, args);
        if (JS_IsException(res))
            goto exception;
        if (JS_ToBoolFree(ctx, res)) {
            if (mode == ArrayFindIndex || mode == ArrayFindLastIndex) {
                JS_FreeValue(ctx, val);
                return index_val;
            } else {
                return val;
            }
        }
        JS_FreeValue(ctx, val);
    }
    if (mode == ArrayFindIndex || mode == ArrayFindLastIndex)
        return JS_NewInt32(ctx, -1);
    else
        return JS_UNDEFINED;

exception:
    JS_FreeValue(ctx, val);
    return JS_EXCEPTION;
}

#define special_indexOf 0
#define special_lastIndexOf 1
#define special_includes -1

JSValue js_typed_array_indexOf(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv, int special)
{
    JSObject *p;
    int len, tag, is_int, is_bigint, k, stop, inc, res = -1;
    int64_t v64;
    double d;
    float f;
    uint16_t hf;

    p = get_typed_array(ctx, this_val);
    if (!p)
        return JS_EXCEPTION;
    if (typed_array_is_oob(p))
        return JS_ThrowTypeErrorArrayBufferOOB(ctx);
    len = p->u.array.count;

    if (len == 0)
        goto done;

    if (special == special_lastIndexOf) {
        k = len - 1;
        if (argc > 1) {
            int64_t k1;
            if (JS_ToInt64Clamp(ctx, &k1, argv[1], -1, len - 1, len))
                goto exception;
            k = k1;
            if (k < 0)
                goto done;
        }
        stop = -1;
        inc = -1;
    } else {
        k = 0;
        if (argc > 1) {
            if (JS_ToInt32Clamp(ctx, &k, argv[1], 0, len, len))
                goto exception;
        }
        stop = len;
        inc = 1;
    }

    /* includes function: 'undefined' can be found if searching out of bounds */
    if (len > p->u.array.count && special == special_includes &&
        JS_IsUndefined(argv[0]) && k < len) {
        res = 0;
        goto done;
    }

    // RAB may have been resized by evil .valueOf method
    len = min_int(len, p->u.array.count);
    if (len == 0)
        goto done;
    if (special == special_lastIndexOf)
        k = min_int(k, len - 1);
    else
        k = min_int(k, len);
    stop = min_int(stop, len);

    is_bigint = 0;
    is_int = 0; /* avoid warning */
    v64 = 0; /* avoid warning */
    tag = JS_VALUE_GET_NORM_TAG(argv[0]);
    if (tag == JS_TAG_INT) {
        is_int = 1;
        v64 = JS_VALUE_GET_INT(argv[0]);
        d = v64;
    } else
    if (tag == JS_TAG_FLOAT64) {
        d = JS_VALUE_GET_FLOAT64(argv[0]);
        if (d >= INT64_MIN && d < 0x1p63) {
            v64 = d;
            is_int = (v64 == d);
        }
    } else if (tag == JS_TAG_BIG_INT || tag == JS_TAG_SHORT_BIG_INT) {
        JSBigIntBuf buf1;
        JSBigInt *p1;
        int sz = (64 / JS_LIMB_BITS);
        if (tag == JS_TAG_SHORT_BIG_INT)
            p1 = js_bigint_set_short(&buf1, argv[0]);
        else
            p1 = JS_VALUE_GET_PTR(argv[0]);

        if (p->class_id == JS_CLASS_BIG_INT64_ARRAY) {
            if (p1->len > sz)
                goto done; /* does not fit an int64 : cannot be found */
        } else if (p->class_id == JS_CLASS_BIG_UINT64_ARRAY) {
            if (js_bigint_sign(p1))
                goto done; /* v < 0 */
            if (p1->len <= sz) {
                /* OK */
            } else if (p1->len == sz + 1 && p1->tab[sz] == 0) {
                /* 2^63 <= v <= 2^64-1 */
            } else {
                goto done;
            }
        } else {
            goto done;
        }
        if (JS_ToBigInt64(ctx, &v64, argv[0]))
            goto exception;
        d = 0;
        is_bigint = 1;
    } else {
        goto done;
    }

    switch (p->class_id) {
    case JS_CLASS_INT8_ARRAY:
        if (is_int && (int8_t)v64 == v64)
            goto scan8;
        break;
    case JS_CLASS_UINT8C_ARRAY:
    case JS_CLASS_UINT8_ARRAY:
        if (is_int && (uint8_t)v64 == v64) {
            const uint8_t *pv, *pp;
            uint16_t v;
        scan8:
            pv = p->u.array.u.uint8_ptr;
            v = v64;
            if (inc > 0) {
                pp = NULL;
                if (pv)
                    pp = memchr(pv + k, v, len - k);
                if (pp)
                    res = pp - pv;
            } else {
                for (; k != stop; k += inc) {
                    if (pv[k] == v) {
                        res = k;
                        break;
                    }
                }
            }
        }
        break;
    case JS_CLASS_INT16_ARRAY:
        if (is_int && (int16_t)v64 == v64)
            goto scan16;
        break;
    case JS_CLASS_UINT16_ARRAY:
        if (is_int && (uint16_t)v64 == v64) {
            const uint16_t *pv;
            uint16_t v;
        scan16:
            pv = p->u.array.u.uint16_ptr;
            v = v64;
            for (; k != stop; k += inc) {
                if (pv[k] == v) {
                    res = k;
                    break;
                }
            }
        }
        break;
    case JS_CLASS_INT32_ARRAY:
        if (is_int && (int32_t)v64 == v64)
            goto scan32;
        break;
    case JS_CLASS_UINT32_ARRAY:
        if (is_int && (uint32_t)v64 == v64) {
            const uint32_t *pv;
            uint32_t v;
        scan32:
            pv = p->u.array.u.uint32_ptr;
            v = v64;
            for (; k != stop; k += inc) {
                if (pv[k] == v) {
                    res = k;
                    break;
                }
            }
        }
        break;
    case JS_CLASS_FLOAT16_ARRAY:
        if (is_bigint)
            break;
        if (isnan(d)) {
            const uint16_t *pv = p->u.array.u.fp16_ptr;
            /* special case: indexOf returns -1, includes finds NaN */
            if (special != special_includes)
                goto done;
            for (; k != stop; k += inc) {
                if (isfp16nan(pv[k])) {
                    res = k;
                    break;
                }
            }
        } else if (d == 0) {
            // special case: includes also finds negative zero
            const uint16_t *pv = p->u.array.u.fp16_ptr;
            for (; k != stop; k += inc) {
                if (isfp16zero(pv[k])) {
                    res = k;
                    break;
                }
            }
        } else if (hf = tofp16(d), d == fromfp16(hf)) {
            const uint16_t *pv = p->u.array.u.fp16_ptr;
            for (; k != stop; k += inc) {
                if (pv[k] == hf) {
                    res = k;
                    break;
                }
            }
        }
        break;
    case JS_CLASS_FLOAT32_ARRAY:
        if (is_bigint)
            break;
        if (isnan(d)) {
            const float *pv = p->u.array.u.float_ptr;
            /* special case: indexOf returns -1, includes finds NaN */
            if (special != special_includes)
                goto done;
            for (; k != stop; k += inc) {
                if (isnan(pv[k])) {
                    res = k;
                    break;
                }
            }
        } else if ((f = (float)d) == d) {
            const float *pv = p->u.array.u.float_ptr;
            for (; k != stop; k += inc) {
                if (pv[k] == f) {
                    res = k;
                    break;
                }
            }
        }
        break;
    case JS_CLASS_FLOAT64_ARRAY:
        if (is_bigint)
            break;
        if (isnan(d)) {
            const double *pv = p->u.array.u.double_ptr;
            /* special case: indexOf returns -1, includes finds NaN */
            if (special != special_includes)
                goto done;
            for (; k != stop; k += inc) {
                if (isnan(pv[k])) {
                    res = k;
                    break;
                }
            }
        } else {
            const double *pv = p->u.array.u.double_ptr;
            for (; k != stop; k += inc) {
                if (pv[k] == d) {
                    res = k;
                    break;
                }
            }
        }
        break;
    case JS_CLASS_BIG_INT64_ARRAY:
        if (is_bigint) {
            goto scan64;
        }
        break;
    case JS_CLASS_BIG_UINT64_ARRAY:
        if (is_bigint) {
            const uint64_t *pv;
            uint64_t v;
        scan64:
            pv = p->u.array.u.uint64_ptr;
            v = v64;
            for (; k != stop; k += inc) {
                if (pv[k] == v) {
                    res = k;
                    break;
                }
            }
        }
        break;
    }

done:
    if (special == special_includes)
        return JS_NewBool(ctx, res >= 0);
    else
        return JS_NewInt32(ctx, res);

exception:
    return JS_EXCEPTION;
}

JSValue js_typed_array_join(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv, int toLocaleString)
{
    JSValue sep = JS_UNDEFINED, el;
    StringBuffer b_s, *b = &b_s;
    JSString *s = NULL;
    JSObject *p;
    int i, len, oldlen, newlen;
    int c;

    p = get_typed_array(ctx, this_val);
    if (!p)
        return JS_EXCEPTION;
    if (typed_array_is_oob(p))
        return JS_ThrowTypeErrorArrayBufferOOB(ctx);
    len = oldlen = newlen = p->u.array.count;

    c = ',';    /* default separator */
    if (!toLocaleString && argc > 0 && !JS_IsUndefined(argv[0])) {
        sep = JS_ToString(ctx, argv[0]);
        if (JS_IsException(sep))
            goto exception;
        s = JS_VALUE_GET_STRING(sep);
        if (s->len == 1 && !s->is_wide_char)
            c = s->u.str8[0];
        else
            c = -1;
        // ToString(sep) can detach or resize the arraybuffer as a side effect
        newlen = p->u.array.count;
        len = min_int(len, newlen);
    }
    string_buffer_init(ctx, b, 0);

    /* XXX: optimize with direct access */
    for(i = 0; i < len; i++) {
        if (i > 0) {
            if (c >= 0) {
                if (string_buffer_putc8(b, c))
                    goto fail;
            } else {
                if (string_buffer_concat(b, s, 0, s->len))
                    goto fail;
            }
        }
        el = JS_GetPropertyUint32(ctx, this_val, i);
        /* Can return undefined for example if the typed array is detached */
        if (!JS_IsNull(el) && !JS_IsUndefined(el)) {
            if (JS_IsException(el))
                goto fail;
            if (toLocaleString) {
                el = JS_ToLocaleStringFree(ctx, el);
            }
            if (string_buffer_concat_value_free(b, el))
                goto fail;
        }
    }

    // add extra separators in case RAB was resized by evil .valueOf method
    i = max_int(1, newlen);
    for(/*empty*/; i < oldlen; i++) {
        if (c >= 0) {
            if (string_buffer_putc8(b, c))
                goto fail;
        } else {
            if (string_buffer_concat(b, s, 0, s->len))
                goto fail;
        }
    }

    JS_FreeValue(ctx, sep);
    return string_buffer_end(b);

fail:
    string_buffer_free(b);
    JS_FreeValue(ctx, sep);
exception:
    return JS_EXCEPTION;
}

JSValue js_typed_array_reverse(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv)
{
    JSObject *p;
    int len;

    len = js_typed_array_get_length_unsafe(ctx, this_val);
    if (len < 0)
        return JS_EXCEPTION;
    if (len > 0) {
        p = JS_VALUE_GET_OBJ(this_val);
        switch (typed_array_size_log2(p->class_id)) {
        case 0:
            {
                uint8_t *p1 = p->u.array.u.uint8_ptr;
                uint8_t *p2 = p1 + len - 1;
                while (p1 < p2) {
                    uint8_t v = *p1;
                    *p1++ = *p2;
                    *p2-- = v;
                }
            }
            break;
        case 1:
            {
                uint16_t *p1 = p->u.array.u.uint16_ptr;
                uint16_t *p2 = p1 + len - 1;
                while (p1 < p2) {
                    uint16_t v = *p1;
                    *p1++ = *p2;
                    *p2-- = v;
                }
            }
            break;
        case 2:
            {
                uint32_t *p1 = p->u.array.u.uint32_ptr;
                uint32_t *p2 = p1 + len - 1;
                while (p1 < p2) {
                    uint32_t v = *p1;
                    *p1++ = *p2;
                    *p2-- = v;
                }
            }
            break;
        case 3:
            {
                uint64_t *p1 = p->u.array.u.uint64_ptr;
                uint64_t *p2 = p1 + len - 1;
                while (p1 < p2) {
                    uint64_t v = *p1;
                    *p1++ = *p2;
                    *p2-- = v;
                }
            }
            break;
        default:
            abort();
        }
    }
    return JS_DupValue(ctx, this_val);
}

JSValue js_typed_array_toReversed(JSContext *ctx, JSValueConst this_val,
                                         int argc, JSValueConst *argv)
{
    JSValue arr, ret;
    JSObject *p;

    p = get_typed_array(ctx, this_val);
    if (!p)
        return JS_EXCEPTION;
    arr = js_typed_array_constructor_ta(ctx, JS_UNDEFINED, this_val,
                                        p->class_id, p->u.array.count);
    if (JS_IsException(arr))
        return JS_EXCEPTION;
    ret = js_typed_array_reverse(ctx, arr, argc, argv);
    JS_FreeValue(ctx, arr);
    return ret;
}

void slice_memcpy(uint8_t *dst, const uint8_t *src, size_t len)
{
    if (dst + len <= src || dst >= src + len) {
        /* no overlap: can use memcpy */
        memcpy(dst, src, len);
    } else {
        /* otherwise the spec mandates byte copy */
        while (len-- != 0)
            *dst++ = *src++;
    }
}

JSValue js_typed_array_slice(JSContext *ctx, JSValueConst this_val,
                                    int argc, JSValueConst *argv)
{
    JSValueConst args[2];
    JSValue arr, val;
    JSObject *p, *p1;
    int n, len, start, final, count, shift, space;

    arr = JS_UNDEFINED;
    p = get_typed_array(ctx, this_val);
    if (!p)
        goto exception;
    if (typed_array_is_oob(p))
        return JS_ThrowTypeErrorArrayBufferOOB(ctx);
    len = p->u.array.count;

    if (JS_ToInt32Clamp(ctx, &start, argv[0], 0, len, len))
        goto exception;
    final = len;
    if (!JS_IsUndefined(argv[1])) {
        if (JS_ToInt32Clamp(ctx, &final, argv[1], 0, len, len))
            goto exception;
    }
    count = max_int(final - start, 0);

    shift = typed_array_size_log2(p->class_id);

    args[0] = this_val;
    args[1] = JS_NewInt32(ctx, count);
    arr = js_typed_array___speciesCreate(ctx, JS_UNDEFINED, 2, args);
    if (JS_IsException(arr))
        goto exception;

    if (count > 0) {
        if (validate_typed_array(ctx, this_val)
        ||  validate_typed_array(ctx, arr))
            goto exception;

        p1 = get_typed_array(ctx, arr);
        space = max_int(0, p->u.array.count - start);
        count = min_int(count, space);
        if (p1 != NULL && p->class_id == p1->class_id) {
            slice_memcpy(p1->u.array.u.uint8_ptr,
                         p->u.array.u.uint8_ptr + (start << shift),
                         count << shift);
        } else {
            for (n = 0; n < count; n++) {
                val = JS_GetPropertyValue(ctx, this_val, JS_NewInt32(ctx, start + n));
                if (JS_IsException(val))
                    goto exception;
                if (JS_SetPropertyValue(ctx, arr, JS_NewInt32(ctx, n), val,
                                        JS_PROP_THROW) < 0)
                    goto exception;
            }
        }
    }
    return arr;

 exception:
    JS_FreeValue(ctx, arr);
    return JS_EXCEPTION;
}

JSValue js_typed_array_subarray(JSContext *ctx, JSValueConst this_val,
                                       int argc, JSValueConst *argv)
{
    JSValueConst args[4];
    JSValue arr, ta_buffer;
    JSTypedArray *ta;
    JSObject *p;
    int len, start, final, count, shift, offset;
    BOOL is_auto;

    p = get_typed_array(ctx, this_val);
    if (!p)
        goto exception;
    len = p->u.array.count;
    if (JS_ToInt32Clamp(ctx, &start, argv[0], 0, len, len))
        goto exception;

    shift = typed_array_size_log2(p->class_id);
    ta = p->u.typed_array;
    /* Read byteOffset (ta->offset) even if detached */
    offset = ta->offset + (start << shift);

    final = len;
    if (JS_IsUndefined(argv[1])) {
        is_auto = ta->track_rab;
    } else {
        is_auto = FALSE;
        if (JS_ToInt32Clamp(ctx, &final, argv[1], 0, len, len))
            goto exception;
    }
    count = max_int(final - start, 0);
    ta_buffer = js_typed_array_get_buffer(ctx, this_val);
    if (JS_IsException(ta_buffer))
        goto exception;
    args[0] = this_val;
    args[1] = ta_buffer;
    args[2] = JS_NewInt32(ctx, offset);
    args[3] = JS_NewInt32(ctx, count);
    arr = js_typed_array___speciesCreate(ctx, JS_UNDEFINED, is_auto ? 3 : 4, args);
    JS_FreeValue(ctx, ta_buffer);
    return arr;

 exception:
    return JS_EXCEPTION;
}

/* TypedArray.prototype.sort */

int js_cmp_doubles(double x, double y)
{
    if (isnan(x))    return isnan(y) ? 0 : +1;
    if (isnan(y))    return -1;
    if (x < y)       return -1;
    if (x > y)       return 1;
    if (x != 0)      return 0;
    if (signbit(x))  return signbit(y) ? 0 : -1;
    else             return signbit(y) ? 1 : 0;
}

int js_TA_cmp_int8(const void *a, const void *b, void *opaque) {
    return *(const int8_t *)a - *(const int8_t *)b;
}

int js_TA_cmp_uint8(const void *a, const void *b, void *opaque) {
    return *(const uint8_t *)a - *(const uint8_t *)b;
}

int js_TA_cmp_int16(const void *a, const void *b, void *opaque) {
    return *(const int16_t *)a - *(const int16_t *)b;
}

int js_TA_cmp_uint16(const void *a, const void *b, void *opaque) {
    return *(const uint16_t *)a - *(const uint16_t *)b;
}

int js_TA_cmp_int32(const void *a, const void *b, void *opaque) {
    int32_t x = *(const int32_t *)a;
    int32_t y = *(const int32_t *)b;
    return (y < x) - (y > x);
}

int js_TA_cmp_uint32(const void *a, const void *b, void *opaque) {
    uint32_t x = *(const uint32_t *)a;
    uint32_t y = *(const uint32_t *)b;
    return (y < x) - (y > x);
}

int js_TA_cmp_int64(const void *a, const void *b, void *opaque) {
    int64_t x = *(const int64_t *)a;
    int64_t y = *(const int64_t *)b;
    return (y < x) - (y > x);
}

int js_TA_cmp_uint64(const void *a, const void *b, void *opaque) {
    uint64_t x = *(const uint64_t *)a;
    uint64_t y = *(const uint64_t *)b;
    return (y < x) - (y > x);
}

int js_TA_cmp_float16(const void *a, const void *b, void *opaque) {
    return js_cmp_doubles(fromfp16(*(const uint16_t *)a),
                          fromfp16(*(const uint16_t *)b));
}

int js_TA_cmp_float32(const void *a, const void *b, void *opaque) {
    return js_cmp_doubles(*(const float *)a, *(const float *)b);
}

int js_TA_cmp_float64(const void *a, const void *b, void *opaque) {
    return js_cmp_doubles(*(const double *)a, *(const double *)b);
}

JSValue js_TA_get_int8(JSContext *ctx, const void *a) {
    return JS_NewInt32(ctx, *(const int8_t *)a);
}

JSValue js_TA_get_uint8(JSContext *ctx, const void *a) {
    return JS_NewInt32(ctx, *(const uint8_t *)a);
}

JSValue js_TA_get_int16(JSContext *ctx, const void *a) {
    return JS_NewInt32(ctx, *(const int16_t *)a);
}

JSValue js_TA_get_uint16(JSContext *ctx, const void *a) {
    return JS_NewInt32(ctx, *(const uint16_t *)a);
}

JSValue js_TA_get_int32(JSContext *ctx, const void *a) {
    return JS_NewInt32(ctx, *(const int32_t *)a);
}

JSValue js_TA_get_uint32(JSContext *ctx, const void *a) {
    return JS_NewUint32(ctx, *(const uint32_t *)a);
}

JSValue js_TA_get_int64(JSContext *ctx, const void *a) {
    return JS_NewBigInt64(ctx, *(int64_t *)a);
}

JSValue js_TA_get_uint64(JSContext *ctx, const void *a) {
    return JS_NewBigUint64(ctx, *(uint64_t *)a);
}

JSValue js_TA_get_float16(JSContext *ctx, const void *a) {
    return __JS_NewFloat64(ctx, fromfp16(*(const uint16_t *)a));
}

JSValue js_TA_get_float32(JSContext *ctx, const void *a) {
    return __JS_NewFloat64(ctx, *(const float *)a);
}

JSValue js_TA_get_float64(JSContext *ctx, const void *a) {
    return __JS_NewFloat64(ctx, *(const double *)a);
}

int js_TA_cmp_generic(const void *a, const void *b, void *opaque) {
    struct TA_sort_context *psc = opaque;
    JSContext *ctx = psc->ctx;
    uint32_t a_idx, b_idx;
    JSValueConst argv[2];
    JSValue res;
    int cmp;

    cmp = 0;
    if (!psc->exception) {
        /* Note: the typed array can be detached without causing an
           error */
        a_idx = *(uint32_t *)a;
        b_idx = *(uint32_t *)b;
        argv[0] = psc->getfun(ctx, psc->array +
                              a_idx * (size_t)psc->elt_size);
        argv[1] = psc->getfun(ctx, psc->array +
                              b_idx * (size_t)(psc->elt_size));
        res = JS_Call(ctx, psc->cmp, JS_UNDEFINED, 2, argv);
        if (JS_IsException(res)) {
            psc->exception = 1;
            goto done;
        }
        if (JS_VALUE_GET_TAG(res) == JS_TAG_INT) {
            int val = JS_VALUE_GET_INT(res);
            cmp = (val > 0) - (val < 0);
        } else {
            double val;
            if (JS_ToFloat64Free(ctx, &val, res) < 0) {
                psc->exception = 1;
                goto done;
            } else {
                cmp = (val > 0) - (val < 0);
            }
        }
        if (cmp == 0) {
            /* make sort stable: compare array offsets */
            cmp = (a_idx > b_idx) - (a_idx < b_idx);
        }
    done:
        JS_FreeValue(ctx, (JSValue)argv[0]);
        JS_FreeValue(ctx, (JSValue)argv[1]);
    }
    return cmp;
}

JSValue js_typed_array_sort(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv)
{
    JSObject *p;
    int len;
    size_t elt_size;
    struct TA_sort_context tsc;
    int (*cmpfun)(const void *a, const void *b, void *opaque);

    tsc.ctx = ctx;
    tsc.exception = 0;
    tsc.cmp = argv[0];

    if (!JS_IsUndefined(tsc.cmp) && check_function(ctx, tsc.cmp))
        return JS_EXCEPTION;
    len = js_typed_array_get_length_unsafe(ctx, this_val);
    if (len < 0)
        return JS_EXCEPTION;

    if (len > 1) {
        p = JS_VALUE_GET_OBJ(this_val);
        switch (p->class_id) {
        case JS_CLASS_INT8_ARRAY:
            tsc.getfun = js_TA_get_int8;
            cmpfun = js_TA_cmp_int8;
            break;
        case JS_CLASS_UINT8C_ARRAY:
        case JS_CLASS_UINT8_ARRAY:
            tsc.getfun = js_TA_get_uint8;
            cmpfun = js_TA_cmp_uint8;
            break;
        case JS_CLASS_INT16_ARRAY:
            tsc.getfun = js_TA_get_int16;
            cmpfun = js_TA_cmp_int16;
            break;
        case JS_CLASS_UINT16_ARRAY:
            tsc.getfun = js_TA_get_uint16;
            cmpfun = js_TA_cmp_uint16;
            break;
        case JS_CLASS_INT32_ARRAY:
            tsc.getfun = js_TA_get_int32;
            cmpfun = js_TA_cmp_int32;
            break;
        case JS_CLASS_UINT32_ARRAY:
            tsc.getfun = js_TA_get_uint32;
            cmpfun = js_TA_cmp_uint32;
            break;
        case JS_CLASS_BIG_INT64_ARRAY:
            tsc.getfun = js_TA_get_int64;
            cmpfun = js_TA_cmp_int64;
            break;
        case JS_CLASS_BIG_UINT64_ARRAY:
            tsc.getfun = js_TA_get_uint64;
            cmpfun = js_TA_cmp_uint64;
            break;
        case JS_CLASS_FLOAT16_ARRAY:
            tsc.getfun = js_TA_get_float16;
            cmpfun = js_TA_cmp_float16;
            break;
        case JS_CLASS_FLOAT32_ARRAY:
            tsc.getfun = js_TA_get_float32;
            cmpfun = js_TA_cmp_float32;
            break;
        case JS_CLASS_FLOAT64_ARRAY:
            tsc.getfun = js_TA_get_float64;
            cmpfun = js_TA_cmp_float64;
            break;
        default:
            abort();
        }
        elt_size = 1 << typed_array_size_log2(p->class_id);
        if (!JS_IsUndefined(tsc.cmp)) {
            uint32_t *array_idx;
            void *array;
            size_t i, j;

            /* the array must be copied because the comparison
               function may modify it */
            array = js_malloc(ctx, len * elt_size);
            if (!array)
                return JS_EXCEPTION;
            memcpy(array, p->u.array.u.ptr, len * elt_size);

            /* array_idx is needed to have a stable sort */
            array_idx = js_malloc(ctx, len * sizeof(array_idx[0]));
            if (!array_idx) {
                js_free(ctx, array);
                return JS_EXCEPTION;
            }
            for(i = 0; i < len; i++)
                array_idx[i] = i;
            tsc.elt_size = elt_size;
            tsc.array = array;
            rqsort(array_idx, len, sizeof(array_idx[0]),
                   js_TA_cmp_generic, &tsc);
            if (tsc.exception) {
                if (tsc.exception == 1) {
                    js_free(ctx, array_idx);
                    js_free(ctx, array);
                    return JS_EXCEPTION;
                }
                /* detached typed array during the sort: no error */
            } else {
                void *array_ptr = p->u.array.u.ptr;
                len = min_int(len, p->u.array.count);
                switch(elt_size) {
                case 1:
                    for(i = 0; i < len; i++) {
                        j = array_idx[i];
                        ((uint8_t *)array_ptr)[i] = ((uint8_t *)array)[j];
                    }
                    break;
                case 2:
                    for(i = 0; i < len; i++) {
                        j = array_idx[i];
                        ((uint16_t *)array_ptr)[i] = ((uint16_t *)array)[j];
                    }
                    break;
                case 4:
                    for(i = 0; i < len; i++) {
                        j = array_idx[i];
                        ((uint32_t *)array_ptr)[i] = ((uint32_t *)array)[j];
                    }
                    break;
                case 8:
                    for(i = 0; i < len; i++) {
                        j = array_idx[i];
                        ((uint64_t *)array_ptr)[i] = ((uint64_t *)array)[j];
                    }
                    break;
                default:
                    abort();
                }
            }
            js_free(ctx, array_idx);
            js_free(ctx, array);
        } else {
            rqsort(p->u.array.u.ptr, len, elt_size, cmpfun, &tsc);
            if (tsc.exception)
                return JS_EXCEPTION;
        }
    }
    return JS_DupValue(ctx, this_val);
}

JSValue js_typed_array_toSorted(JSContext *ctx, JSValueConst this_val,
                                       int argc, JSValueConst *argv)
{
    JSValue arr, ret;
    JSObject *p;

    p = get_typed_array(ctx, this_val);
    if (!p)
        return JS_EXCEPTION;
    arr = js_typed_array_constructor_ta(ctx, JS_UNDEFINED, this_val,
                                        p->class_id, p->u.array.count);
    if (JS_IsException(arr))
        return JS_EXCEPTION;
    ret = js_typed_array_sort(ctx, arr, argc, argv);
    JS_FreeValue(ctx, arr);
    return ret;
}

/* Uint8Array base64/hex (tc39 proposal-arraybuffer-base64) */

size_t b64_encode(const uint8_t *src, size_t len, char *dst,
                         const unsigned char *alpha)
{
    size_t i, j;

    for (i = 0, j = 0; i + 3 <= len; i += 3, j += 4) {
        uint32_t v = 65536*src[i] + 256*src[i + 1] + src[i + 2];
        dst[j + 0] = alpha[(v >> 18) & 63];
        dst[j + 1] = alpha[(v >> 12) & 63];
        dst[j + 2] = alpha[(v >> 6) & 63];
        dst[j + 3] = alpha[v & 63];
    }

    size_t rem = len - i;
    if (rem == 1) {
        uint32_t v = 65536*src[i];
        dst[j++] = alpha[(v >> 18) & 63];
        dst[j++] = alpha[(v >> 12) & 63];
        dst[j++] = '=';
        dst[j++] = '=';
    } else if (rem == 2) {
        uint32_t v = 65536*src[i] + 256*src[i + 1];
        dst[j++] = alpha[(v >> 18) & 63];
        dst[j++] = alpha[(v >> 12) & 63];
        dst[j++] = alpha[(v >> 6) & 63];
        dst[j++] = '=';
    }
    return j;
}

size_t b64_skip_ws(const char *src, size_t len, size_t index,
                          const uint8_t *dec_table)
{
    while (index < len && dec_table[(unsigned char)src[index]] == K_WS)
        index++;
    return index;
}

/* Implements the FromBase64 abstract operation.
   src/src_len: the input string (must be ASCII/latin1)
   dst/max_len: output buffer
   flags: b64_flags or b64_flags_url (selects valid characters)
   last_chunk: B64_LAST_LOOSE, B64_LAST_STRICT, or B64_LAST_STOP_BEFORE_PARTIAL
   *p_read: set to number of input characters consumed
   *p_err: set to 1 on error, 0 on success
   Returns: number of bytes written to dst */
size_t from_base64(const char *src, size_t src_len,
                          uint8_t *dst, size_t max_len,
                          const uint8_t *dec_table, int last_chunk,
                          size_t *p_read, int *p_err)
{
    size_t read = 0, written = 0;
    uint32_t v, acc = 0;
    int seen = 0;
    size_t index = 0;
    uint8_t ch;

    *p_err = 0;

    if (max_len == 0) {
        *p_read = 0;
        return 0;
    }

    for (;;) {
        if (seen == 0) {
            /* Fast path: decode complete groups of 4 valid characters.
               Breaks out on whitespace, padding, invalid chars, or capacity. */
            while (index + 4 <= src_len && written + 3 <= max_len) {
                uint32_t v0, v1, v2, v3;
                v0 = dec_table[(unsigned char)src[index]];
                v1 = dec_table[(unsigned char)src[index + 1]];
                v2 = dec_table[(unsigned char)src[index + 2]];
                v3 = dec_table[(unsigned char)src[index + 3]];
                if ((v0 | v1 | v2 | v3) >= 64)
                    break;
                v = (v0 << 18) | (v1 << 12) | (v2 << 6) | v3;
                dst[written]     = (uint8_t)(v >> 16);
                dst[written + 1] = (uint8_t)(v >> 8);
                dst[written + 2] = (uint8_t)(v);
                written += 3;
                index += 4;
            }
            read = index;

            if (written >= max_len) {
                *p_read = read;
                return written;
            }
        }

        /* Slow path: handle whitespace, padding, partial groups, capacity. */
        index = b64_skip_ws(src, src_len, index, dec_table);

        if (index == src_len) {
            if (seen > 0) {
                if (last_chunk == B64_LAST_STOP_BEFORE_PARTIAL) {
                    *p_read = read;
                    return written;
                }
                if (last_chunk == B64_LAST_STRICT) {
                    *p_err = 1;
                    return 0;
                }
                /* loose */
                if (seen == 1) {
                    *p_err = 1;
                    return 0;
                }
                break;
            }
            *p_read = src_len;
            return written;
        }

        ch = src[index++];

        if (ch == '=') {
            if (seen < 2) {
                *p_err = 1;
                return 0;
            }
            index = b64_skip_ws(src, src_len, index, dec_table);
            if (seen == 2) {
                if (index == src_len) {
                    if (last_chunk == B64_LAST_STOP_BEFORE_PARTIAL) {
                        *p_read = read;
                        return written;
                    }
                    *p_err = 1;
                    return 0;
                }
                if (src[index] == '=') {
                    index++;
                    index = b64_skip_ws(src, src_len, index, dec_table);
                } else {
                    *p_err = 1;
                    return 0;
                }
            }
            /* After padding, only whitespace is allowed */
            if (index != src_len) {
                *p_err = 1;
                return 0;
            }
            if (last_chunk == B64_LAST_STRICT) {
                uint32_t mask = (seen == 2) ? 0xF : 0x3;
                if (acc & mask) {
                    *p_err = 1;
                    return 0;
                }
            }
            break;
        }

        v = dec_table[ch];
        if (v >= 64) {
            *p_err = 1;
            return 0;
        }

        /* Check remaining capacity before committing to this group */
        {
            size_t remaining = max_len - written;
            if ((remaining == 1 && seen == 2) ||
                    (remaining == 2 && seen == 3)) {
                *p_read = read;
                return written;
            }
        }

        acc = (acc << 6) | v;
        seen++;

        if (seen == 4) {
            dst[written]     = (uint8_t)(acc >> 16);
            dst[written + 1] = (uint8_t)(acc >> 8);
            dst[written + 2] = (uint8_t)(acc);
            written += 3;
            acc = 0;
            seen = 0;
            read = index;
            if (written >= max_len) {
                *p_read = read;
                return written;
            }
        }
    }

    if (seen == 2) {
        dst[written++] = (uint8_t)(acc >> 4);
    } else if (seen == 3) {
        dst[written]     = (uint8_t)(acc >> 10);
        dst[written + 1] = (uint8_t)(acc >> 2);
        written += 2;
    }
    *p_read = src_len;
    return written;
}

/* Hex helpers */
size_t u8a_hex_encode(const uint8_t *src, size_t len, char *dst)
{
    for (size_t i = 0; i < len; i++) {
        dst[i * 2]     = u8a_hex_digits[src[i] >> 4];
        dst[i * 2 + 1] = u8a_hex_digits[src[i] & 0xF];
    }
    return len * 2;
}

/* Decode hex string to bytes.
   Returns bytes written. Sets *p_read to chars consumed, *p_err on error. */
size_t u8a_hex_decode(const char *src, size_t src_len,
                             uint8_t *dst, size_t max_len,
                             size_t *p_read, int *p_err)
{
    size_t written = 0, i = 0;
    *p_err = 0;

    if (src_len & 1) {
        *p_err = 1;
        return 0;
    }

    while (i < src_len && written < max_len) {
        int hi = from_hex(src[i]);
        int lo = from_hex(src[i + 1]);
        if (hi < 0 || lo < 0) {
            *p_err = 1;
            return 0;
        }
        dst[written++] = (uint8_t)((hi << 4) | lo);
        i += 2;
    }

    *p_read = i;
    return written;
}

JSValue JS_NewUint8ArrayCopy(JSContext *ctx, const uint8_t *buf, size_t len)
{
    JSValue buffer, obj;
    JSArrayBuffer *abuf;

    buffer = js_array_buffer_constructor3(ctx, JS_UNDEFINED, len, NULL,
                                          JS_CLASS_ARRAY_BUFFER,
                                          (uint8_t *)buf,
                                          js_array_buffer_free, NULL,
                                          TRUE);
    if (JS_IsException(buffer))
        return JS_EXCEPTION;
    obj = js_create_from_ctor(ctx, JS_UNDEFINED, JS_CLASS_UINT8_ARRAY);
    if (JS_IsException(obj)) {
        JS_FreeValue(ctx, buffer);
        return JS_EXCEPTION;
    }
    abuf = js_get_array_buffer(ctx, buffer);
    assert(abuf != NULL);
    if (typed_array_init(ctx, obj, buffer, 0, abuf->byte_length, /*track_rab*/FALSE)) {
        // 'buffer' is freed on error above.
        JS_FreeValue(ctx, obj);
        return JS_EXCEPTION;
    }
    return obj;
}

/* Validate that this_val is a Uint8Array (type check only, no detach check).
   Returns the JSObject pointer or NULL on error (throws). */
JSObject *check_uint8array(JSContext *ctx, JSValueConst this_val)
{
    JSObject *p;

    if (JS_VALUE_GET_TAG(this_val) != JS_TAG_OBJECT)
        goto fail;
    p = JS_VALUE_GET_OBJ(this_val);
    if (p->class_id != JS_CLASS_UINT8_ARRAY)
        goto fail;
    return p;
fail:
    JS_ThrowTypeError(ctx, "not a Uint8Array");
    return NULL;
}

/* Get the data pointer and length of a Uint8Array, checking for detached
   buffers. Must be called after options are read (per spec ordering).
   Returns 0 on success, -1 on error (throws). */
int get_uint8array_bytes(JSContext *ctx, JSObject *p,
                                uint8_t **pdata, size_t *plen)
{
    if (typed_array_is_oob(p)) {
        JS_ThrowTypeErrorArrayBufferOOB(ctx);
        *pdata = NULL; /* fail safe */
        *plen = 0;
        return -1;
    }
    *pdata = p->u.array.u.uint8_ptr;
    *plen = p->u.array.count;
    return 0;
}

/* Validate options is undefined or an object (GetOptionsObject).
   Returns 0 on success, -1 on error (throws). */
int check_options_object(JSContext *ctx, JSValueConst options)
{
    if (JS_IsUndefined(options))
        return 0;
    if (!JS_IsObject(options)) {
        JS_ThrowTypeError(ctx, "options must be an object");
        return -1;
    }
    return 0;
}

/* Parse the 'alphabet' option from an options object.
   Returns B64_ALPHABET_BASE64 or B64_ALPHABET_BASE64URL, or -1 on error. */
int parse_alphabet_option(JSContext *ctx, JSValueConst options)
{
    JSValue val;
    const char *str;
    int ret;

    if (JS_IsUndefined(options))
        return B64_ALPHABET_BASE64;

    val = JS_GetProperty(ctx, options, JS_ATOM_alphabet);
    if (JS_IsException(val))
        return -1;
    if (JS_IsUndefined(val))
        return B64_ALPHABET_BASE64;
    if (!JS_IsString(val)) {
        JS_FreeValue(ctx, val);
        JS_ThrowTypeError(ctx, "expected string for alphabet");
        return -1;
    }

    str = JS_ToCString(ctx, val);
    JS_FreeValue(ctx, val);
    if (!str)
        return -1;

    if (!strcmp(str, "base64"))
        ret = B64_ALPHABET_BASE64;
    else if (!strcmp(str, "base64url"))
        ret = B64_ALPHABET_BASE64URL;
    else {
        JS_ThrowTypeError(ctx, "invalid alphabet");
        ret = -1;
    }
    JS_FreeCString(ctx, str);
    return ret;
}

/* Parse the 'lastChunkHandling' option. Returns mode or -1 on error. */
int parse_last_chunk_option(JSContext *ctx, JSValueConst options)
{
    JSValue val;
    const char *str;
    int ret;

    if (JS_IsUndefined(options))
        return B64_LAST_LOOSE;

    val = JS_GetProperty(ctx, options, JS_ATOM_lastChunkHandling);
    if (JS_IsException(val))
        return -1;
    if (JS_IsUndefined(val))
        return B64_LAST_LOOSE;
    if (!JS_IsString(val)) {
        JS_FreeValue(ctx, val);
        JS_ThrowTypeError(ctx, "expected string for lastChunkHandling");
        return -1;
    }

    str = JS_ToCString(ctx, val);
    JS_FreeValue(ctx, val);
    if (!str)
        return -1;

    if (!strcmp(str, "loose"))
        ret = B64_LAST_LOOSE;
    else if (!strcmp(str, "strict"))
        ret = B64_LAST_STRICT;
    else if (!strcmp(str, "stop-before-partial"))
        ret = B64_LAST_STOP_BEFORE_PARTIAL;
    else {
        JS_ThrowTypeError(ctx, "invalid lastChunkHandling option");
        ret = -1;
    }
    JS_FreeCString(ctx, str);
    return ret;
}

/* Uint8Array.prototype.toBase64([options]) */
JSValue js_uint8array_to_base64(JSContext *ctx, JSValueConst this_val,
                                       int argc, JSValueConst *argv)
{
    uint8_t *data;
    size_t len;
    JSValueConst options;
    JSObject *p;
    int alphabet, omit_padding;
    size_t out_len, written;
    JSString *ostr;
    char *dst;

    p = check_uint8array(ctx, this_val);
    if (!p)
        return JS_EXCEPTION;

    options = argc > 0 ? argv[0] : JS_UNDEFINED;
    if (check_options_object(ctx, options))
        return JS_EXCEPTION;
    alphabet = parse_alphabet_option(ctx, options);
    if (alphabet < 0)
        return JS_EXCEPTION;

    omit_padding = 0;
    if (!JS_IsUndefined(options)) {
        JSValue op_val = JS_GetProperty(ctx, options, JS_ATOM_omitPadding);
        if (JS_IsException(op_val))
            return JS_EXCEPTION;
        omit_padding = JS_ToBool(ctx, op_val);
        JS_FreeValue(ctx, op_val);
    }

    if (get_uint8array_bytes(ctx, p, &data, &len))
        return JS_EXCEPTION;

    out_len = 4 * ((len + 2) / 3);

    if (unlikely(out_len > JS_STRING_LEN_MAX))
        return JS_ThrowRangeError(ctx, "output too large");

    ostr = js_alloc_string(ctx, out_len, 0);
    if (!ostr)
        return JS_EXCEPTION;

    dst = (char *)ostr->u.str8;
    written = b64_encode(data, len, dst,
                         alphabet == B64_ALPHABET_BASE64URL ? b64url_enc : b64_enc);
    if (omit_padding) {
        while (written > 0 && dst[written - 1] == '=')
            written--;
    }
    dst[written] = '\0';

    ostr->len = written;
    return JS_MKPTR(JS_TAG_STRING, ostr);
}
