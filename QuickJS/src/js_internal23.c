#include "js_internal.h"



/* Uint8Array.prototype.toHex() */
JSValue js_uint8array_to_hex(JSContext *ctx, JSValueConst this_val,
                                    int argc, JSValueConst *argv)
{
    uint8_t *data;
    size_t len, out_len;
    JSObject *p;
    JSString *ostr;

    p = check_uint8array(ctx, this_val);
    if (!p)
        return JS_EXCEPTION;
    if (get_uint8array_bytes(ctx, p, &data, &len))
        return JS_EXCEPTION;

    out_len = len * 2;
    if (unlikely(out_len > JS_STRING_LEN_MAX))
        return JS_ThrowRangeError(ctx, "output too large");

    ostr = js_alloc_string(ctx, out_len, 0);
    if (!ostr)
        return JS_EXCEPTION;

    u8a_hex_encode(data, len, (char *)ostr->u.str8);
    ostr->u.str8[out_len] = '\0';
    return JS_MKPTR(JS_TAG_STRING, ostr);
}

/* Uint8Array.fromBase64(string[, options]) */
JSValue js_uint8array_from_base64(JSContext *ctx, JSValueConst this_val,
                                         int argc, JSValueConst *argv)
{
    const char *str;
    size_t str_len, read_pos, decoded_len, out_cap;
    int alphabet, last_chunk, err;
    uint8_t *buf;
    JSValue result;
    JSValueConst options;

    if (!JS_IsString(argv[0]))
        return JS_ThrowTypeError(ctx, "expected string");

    str = JS_ToCStringLen(ctx, &str_len, argv[0]);
    if (!str)
        return JS_EXCEPTION;

    options = argc > 1 ? argv[1] : JS_UNDEFINED;
    if (check_options_object(ctx, options)) {
        JS_FreeCString(ctx, str);
        return JS_EXCEPTION;
    }
    alphabet = parse_alphabet_option(ctx, options);
    if (alphabet < 0) {
        JS_FreeCString(ctx, str);
        return JS_EXCEPTION;
    }
    last_chunk = parse_last_chunk_option(ctx, options);
    if (last_chunk < 0) {
        JS_FreeCString(ctx, str);
        return JS_EXCEPTION;
    }

    out_cap = (str_len / 4) * 3 + 3;
    buf = js_malloc(ctx, out_cap);
    if (!buf) {
        JS_FreeCString(ctx, str);
        return JS_EXCEPTION;
    }

    decoded_len = from_base64(str, str_len, buf, out_cap,
                              alphabet == B64_ALPHABET_BASE64URL
                                  ? b64url_dec : b64_dec,
                              last_chunk, &read_pos, &err);
    JS_FreeCString(ctx, str);

    if (err) {
        js_free(ctx, buf);
        return JS_ThrowSyntaxError(ctx, "invalid base64 string");
    }

    result = JS_NewUint8ArrayCopy(ctx, buf, decoded_len);
    js_free(ctx, buf);
    return result;
}

/* Uint8Array.fromHex(string) */
JSValue js_uint8array_from_hex(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv)
{
    const char *str;
    size_t str_len, read_pos, decoded_len, out_cap;
    int err;
    uint8_t *buf;
    JSValue result;

    if (!JS_IsString(argv[0]))
        return JS_ThrowTypeError(ctx, "expected string");

    str = JS_ToCStringLen(ctx, &str_len, argv[0]);
    if (!str)
        return JS_EXCEPTION;

    out_cap = str_len / 2 + 1;
    buf = js_malloc(ctx, out_cap);
    if (!buf) {
        JS_FreeCString(ctx, str);
        return JS_EXCEPTION;
    }

    decoded_len = u8a_hex_decode(str, str_len, buf, out_cap, &read_pos, &err);
    JS_FreeCString(ctx, str);

    if (err) {
        js_free(ctx, buf);
        return JS_ThrowSyntaxError(ctx, "invalid hex string");
    }

    /* XXX: could avoid the copy */
    result = JS_NewUint8ArrayCopy(ctx, buf, decoded_len);
    js_free(ctx, buf);
    return result;
}

/* Return a { read, written } result object */
JSValue js_make_read_written(JSContext *ctx, size_t read, size_t written)
{
    JSValue obj = JS_NewObject(ctx);
    if (JS_IsException(obj))
        return JS_EXCEPTION;
    if (JS_DefinePropertyValueStr(ctx, obj, "read",
                                  JS_NewUint32(ctx, read), JS_PROP_C_W_E) < 0)
        goto fail;
    if (JS_DefinePropertyValueStr(ctx, obj, "written",
                                  JS_NewUint32(ctx, written), JS_PROP_C_W_E) < 0)
        goto fail;
    return obj;
fail:
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

/* Uint8Array.prototype.setFromBase64(string[, options]) */
JSValue js_uint8array_set_from_base64(JSContext *ctx,
                                             JSValueConst this_val,
                                             int argc, JSValueConst *argv)
{
    uint8_t *data;
    size_t len;
    const char *str;
    size_t str_len, read_pos, decoded_len;
    JSObject *p;
    int alphabet, last_chunk, err;
    JSValueConst options;

    p = check_uint8array(ctx, this_val);
    if (!p)
        return JS_EXCEPTION;

    if (!JS_IsString(argv[0]))
        return JS_ThrowTypeError(ctx, "expected string");

    str = JS_ToCStringLen(ctx, &str_len, argv[0]);
    if (!str)
        return JS_EXCEPTION;

    options = argc > 1 ? argv[1] : JS_UNDEFINED;
    if (check_options_object(ctx, options)) {
        JS_FreeCString(ctx, str);
        return JS_EXCEPTION;
    }
    alphabet = parse_alphabet_option(ctx, options);
    if (alphabet < 0) {
        JS_FreeCString(ctx, str);
        return JS_EXCEPTION;
    }
    last_chunk = parse_last_chunk_option(ctx, options);
    if (last_chunk < 0) {
        JS_FreeCString(ctx, str);
        return JS_EXCEPTION;
    }

    if (get_uint8array_bytes(ctx, p, &data, &len)) {
        JS_FreeCString(ctx, str);
        return JS_EXCEPTION;
    }

    decoded_len = from_base64(str, str_len, data, len,
                              alphabet == B64_ALPHABET_BASE64URL
                                  ? b64url_dec : b64_dec,
                              last_chunk, &read_pos, &err);
    JS_FreeCString(ctx, str);

    if (err)
        return JS_ThrowSyntaxError(ctx, "invalid base64 string");

    return js_make_read_written(ctx, read_pos, decoded_len);
}

/* Uint8Array.prototype.setFromHex(string) */
JSValue js_uint8array_set_from_hex(JSContext *ctx,
                                          JSValueConst this_val,
                                          int argc, JSValueConst *argv)
{
    uint8_t *data;
    size_t len;
    const char *str;
    size_t str_len, read_pos, decoded_len;
    JSObject *p;
    int err;

    p = check_uint8array(ctx, this_val);
    if (!p)
        return JS_EXCEPTION;

    if (!JS_IsString(argv[0]))
        return JS_ThrowTypeError(ctx, "expected string");

    str = JS_ToCStringLen(ctx, &str_len, argv[0]);
    if (!str)
        return JS_EXCEPTION;

    if (get_uint8array_bytes(ctx, p, &data, &len)) {
        JS_FreeCString(ctx, str);
        return JS_EXCEPTION;
    }

    decoded_len = u8a_hex_decode(str, str_len, data, len, &read_pos, &err);
    JS_FreeCString(ctx, str);

    if (err)
        return JS_ThrowSyntaxError(ctx, "invalid hex string");

    return js_make_read_written(ctx, read_pos, decoded_len);
}

JSValue js_typed_array_base_constructor(JSContext *ctx,
                                               JSValueConst this_val,
                                               int argc, JSValueConst *argv)
{
    return JS_ThrowTypeError(ctx, "cannot be called");
}

/* 'obj' must be an allocated typed array object */
int typed_array_init(JSContext *ctx, JSValueConst obj,
                            JSValue buffer, uint64_t offset, uint64_t len,
                            BOOL track_rab)
{
    JSTypedArray *ta;
    JSObject *p, *pbuffer;
    JSArrayBuffer *abuf;
    int size_log2;

    p = JS_VALUE_GET_OBJ(obj);
    size_log2 = typed_array_size_log2(p->class_id);
    ta = js_malloc(ctx, sizeof(*ta));
    if (!ta) {
        JS_FreeValue(ctx, buffer);
        return -1;
    }
    pbuffer = JS_VALUE_GET_OBJ(buffer);
    abuf = pbuffer->u.array_buffer;
    ta->obj = p;
    ta->buffer = pbuffer;
    ta->offset = offset;
    ta->length = len << size_log2;
    ta->track_rab = track_rab;
    list_add_tail(&ta->link, &abuf->array_list);
    p->u.typed_array = ta;
    p->u.array.count = len;
    p->u.array.u.ptr = abuf->data + offset;
    return 0;
}


JSValue js_array_from_iterator(JSContext *ctx, uint32_t *plen,
                                      JSValueConst obj, JSValueConst method)
{
    JSValue arr, iter, next_method = JS_UNDEFINED, val;
    BOOL done;
    uint32_t k;

    *plen = 0;
    arr = JS_NewArray(ctx);
    if (JS_IsException(arr))
        return arr;
    iter = JS_GetIterator2(ctx, obj, method);
    if (JS_IsException(iter))
        goto fail;
    next_method = JS_GetProperty(ctx, iter, JS_ATOM_next);
    if (JS_IsException(next_method))
        goto fail;
    k = 0;
    for(;;) {
        val = JS_IteratorNext(ctx, iter, next_method, 0, NULL, &done);
        if (JS_IsException(val))
            goto fail;
        if (done)
            break;
        if (JS_CreateDataPropertyUint32(ctx, arr, k, val, JS_PROP_THROW) < 0)
            goto fail;
        k++;
    }
    JS_FreeValue(ctx, next_method);
    JS_FreeValue(ctx, iter);
    *plen = k;
    return arr;
 fail:
    JS_FreeValue(ctx, next_method);
    JS_FreeValue(ctx, iter);
    JS_FreeValue(ctx, arr);
    return JS_EXCEPTION;
}

JSValue js_typed_array_constructor_obj(JSContext *ctx,
                                              JSValueConst new_target,
                                              JSValueConst obj,
                                              int classid)
{
    JSValue iter, ret, arr = JS_UNDEFINED, val, buffer;
    uint32_t i;
    int size_log2;
    int64_t len;

    size_log2 = typed_array_size_log2(classid);
    ret = js_create_from_ctor(ctx, new_target, classid);
    if (JS_IsException(ret))
        return JS_EXCEPTION;

    iter = JS_GetProperty(ctx, obj, JS_ATOM_Symbol_iterator);
    if (JS_IsException(iter))
        goto fail;
    if (!JS_IsUndefined(iter) && !JS_IsNull(iter)) {
        uint32_t len1;
        arr = js_array_from_iterator(ctx, &len1, obj, iter);
        JS_FreeValue(ctx, iter);
        if (JS_IsException(arr))
            goto fail;
        len = len1;
    } else {
        if (js_get_length64(ctx, &len, obj))
            goto fail;
        arr = JS_DupValue(ctx, obj);
    }

    buffer = js_array_buffer_constructor1(ctx, JS_UNDEFINED,
                                          len << size_log2,
                                          NULL);
    if (JS_IsException(buffer))
        goto fail;
    if (typed_array_init(ctx, ret, buffer, 0, len, /*track_rab*/FALSE))
        goto fail;

    for(i = 0; i < len; i++) {
        val = JS_GetPropertyUint32(ctx, arr, i);
        if (JS_IsException(val))
            goto fail;
        if (JS_SetPropertyUint32(ctx, ret, i, val) < 0)
            goto fail;
    }
    JS_FreeValue(ctx, arr);
    return ret;
 fail:
    JS_FreeValue(ctx, arr);
    JS_FreeValue(ctx, ret);
    return JS_EXCEPTION;
}

JSValue js_typed_array_constructor_ta(JSContext *ctx,
                                             JSValueConst new_target,
                                             JSValueConst src_obj,
                                             int classid, uint32_t len)
{
    JSObject *p, *src_buffer;
    JSTypedArray *ta;
    JSValue obj, buffer;
    uint32_t i;
    int size_log2;
    JSArrayBuffer *src_abuf, *abuf;

    obj = js_create_from_ctor(ctx, new_target, classid);
    if (JS_IsException(obj))
        return obj;
    p = JS_VALUE_GET_OBJ(src_obj);
    if (typed_array_is_oob(p)) {
        JS_ThrowTypeErrorArrayBufferOOB(ctx);
        goto fail;
    }
    size_log2 = typed_array_size_log2(classid);
    buffer = js_array_buffer_constructor1(ctx, JS_UNDEFINED,
                                          (uint64_t)len << size_log2,
                                          NULL);
    if (JS_IsException(buffer))
        goto fail;
    /* necessary because it could have been detached */
    if (typed_array_is_oob(p)) {
        JS_FreeValue(ctx, buffer);
        JS_ThrowTypeErrorArrayBufferOOB(ctx);
        goto fail;
    }
    abuf = JS_GetOpaque(buffer, JS_CLASS_ARRAY_BUFFER);
    if (typed_array_init(ctx, obj, buffer, 0, len, /*track_rab*/FALSE))
        goto fail;
    ta = p->u.typed_array;
    src_buffer = ta->buffer;
    src_abuf = src_buffer->u.array_buffer;
    if (p->class_id == classid &&
        (int64_t)ta->offset + (int64_t)abuf->byte_length <= src_abuf->byte_length) {
        /* same type and no overflow: copy the content */
        memcpy(abuf->data, src_abuf->data + ta->offset, abuf->byte_length);
    } else {
        for(i = 0; i < len; i++) {
            JSValue val;
            val = JS_GetPropertyUint32(ctx, src_obj, i);
            if (JS_IsException(val))
                goto fail;
            if (JS_SetPropertyUint32(ctx, obj, i, val) < 0)
                goto fail;
        }
    }
    return obj;
 fail:
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

JSValue js_typed_array_constructor(JSContext *ctx,
                                          JSValueConst new_target,
                                          int argc, JSValueConst *argv,
                                          int classid)
{
    BOOL track_rab = FALSE;
    JSValue buffer, obj;
    JSArrayBuffer *abuf;
    int size_log2;
    uint64_t len, offset;

    size_log2 = typed_array_size_log2(classid);
    if (JS_VALUE_GET_TAG(argv[0]) != JS_TAG_OBJECT) {
        if (JS_ToIndex(ctx, &len, argv[0]))
            return JS_EXCEPTION;
        obj = js_create_from_ctor(ctx, new_target, classid);
        if (JS_IsException(obj))
            return JS_EXCEPTION;
        buffer = js_array_buffer_constructor1(ctx, JS_UNDEFINED,
                                              len << size_log2,
                                              NULL);
        if (JS_IsException(buffer))
            goto fail;
        offset = 0;
    } else {
        JSObject *p = JS_VALUE_GET_OBJ(argv[0]);
        if (p->class_id == JS_CLASS_ARRAY_BUFFER ||
            p->class_id == JS_CLASS_SHARED_ARRAY_BUFFER) {
            obj = js_create_from_ctor(ctx, new_target, classid);
            if (JS_IsException(obj))
                return JS_EXCEPTION;
            if (JS_ToIndex(ctx, &offset, argv[1]))
                goto fail;
            if ((offset & ((1 << size_log2) - 1)) != 0)
                goto invalid_offset;
            abuf = p->u.array_buffer;
            if (JS_IsUndefined(argv[2])) {
                if (abuf->detached) {
                    JS_ThrowTypeErrorDetachedArrayBuffer(ctx);
                    goto fail;
                }
                if (offset > abuf->byte_length) {
                invalid_offset:
                    JS_ThrowRangeError(ctx, "invalid offset");
                    goto fail;
                }
                track_rab = array_buffer_is_resizable(abuf);
                if (!track_rab) {
                    if ((abuf->byte_length & ((1 << size_log2) - 1)) != 0)
                        goto invalid_length;
                }
                len = (abuf->byte_length - offset) >> size_log2;
            } else {
                if (JS_ToIndex(ctx, &len, argv[2]))
                    goto fail;
                if (abuf->detached) {
                    JS_ThrowTypeErrorDetachedArrayBuffer(ctx);
                    goto fail;
                }
                if ((offset + (len << size_log2)) > abuf->byte_length) {
                invalid_length:
                    JS_ThrowRangeError(ctx, "invalid length");
                    goto fail;
                }
            }
            buffer = JS_DupValue(ctx, argv[0]);
        } else {
            if (p->class_id >= JS_CLASS_UINT8C_ARRAY &&
                p->class_id <= JS_CLASS_FLOAT64_ARRAY) {
                return js_typed_array_constructor_ta(ctx, new_target, argv[0],
                                                     classid, p->u.array.count);
            } else {
                return js_typed_array_constructor_obj(ctx, new_target, argv[0], classid);
            }
        }
    }
    if (typed_array_init(ctx, obj, buffer, offset, len, track_rab))
        goto fail;
    return obj;
 fail:
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

void js_typed_array_finalizer(JSRuntime *rt, JSValue val)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JSTypedArray *ta = p->u.typed_array;
    if (ta) {
        /* during the GC the finalizers are called in an arbitrary
           order so the ArrayBuffer finalizer may have been called */
        if (ta->link.next) {
            list_del(&ta->link);
        }
        JS_FreeValueRT(rt, JS_MKPTR(JS_TAG_OBJECT, ta->buffer));
        js_free_rt(rt, ta);
    }
}

void js_typed_array_mark(JSRuntime *rt, JSValueConst val,
                                JS_MarkFunc *mark_func)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JSTypedArray *ta = p->u.typed_array;
    if (ta) {
        JS_MarkValue(rt, JS_MKPTR(JS_TAG_OBJECT, ta->buffer), mark_func);
    }
}

JSValue js_dataview_constructor(JSContext *ctx,
                                       JSValueConst new_target,
                                       int argc, JSValueConst *argv)
{
    BOOL recompute_len = FALSE;
    BOOL track_rab = FALSE;
    JSArrayBuffer *abuf;
    uint64_t offset;
    uint32_t len;
    JSValueConst buffer;
    JSValue obj;
    JSTypedArray *ta;
    JSObject *p;

    buffer = argv[0];
    abuf = js_get_array_buffer(ctx, buffer);
    if (!abuf)
        return JS_EXCEPTION;
    offset = 0;
    if (argc > 1) {
        if (JS_ToIndex(ctx, &offset, argv[1]))
            return JS_EXCEPTION;
    }
    if (abuf->detached)
        return JS_ThrowTypeErrorDetachedArrayBuffer(ctx);
    if (offset > abuf->byte_length)
        return JS_ThrowRangeError(ctx, "invalid byteOffset");
    len = abuf->byte_length - offset;
    if (argc > 2 && !JS_IsUndefined(argv[2])) {
        uint64_t l;
        if (JS_ToIndex(ctx, &l, argv[2]))
            return JS_EXCEPTION;
        if (l > len)
            return JS_ThrowRangeError(ctx, "invalid byteLength");
        len = l;
    } else {
        recompute_len = TRUE;
        track_rab = array_buffer_is_resizable(abuf);
    }

    obj = js_create_from_ctor(ctx, new_target, JS_CLASS_DATAVIEW);
    if (JS_IsException(obj))
        return JS_EXCEPTION;
    if (abuf->detached) {
        /* could have been detached in js_create_from_ctor() */
        JS_ThrowTypeErrorDetachedArrayBuffer(ctx);
        goto fail;
    }
    // RAB could have been resized in js_create_from_ctor()
    if (offset > abuf->byte_length) {
        goto out_of_bound;
    } else if (recompute_len) {
        len = abuf->byte_length - offset;
    } else if (offset + len > abuf->byte_length) {
    out_of_bound:
        JS_ThrowRangeError(ctx, "invalid byteOffset or byteLength");
        goto fail;
    }
    ta = js_malloc(ctx, sizeof(*ta));
    if (!ta) {
    fail:
        JS_FreeValue(ctx, obj);
        return JS_EXCEPTION;
    }
    p = JS_VALUE_GET_OBJ(obj);
    ta->obj = p;
    ta->buffer = JS_VALUE_GET_OBJ(JS_DupValue(ctx, buffer));
    ta->offset = offset;
    ta->length = len;
    ta->track_rab = track_rab;
    list_add_tail(&ta->link, &abuf->array_list);
    p->u.typed_array = ta;
    return obj;
}

// is the DataView out of bounds relative to its parent arraybuffer?
BOOL dataview_is_oob(JSObject *p)
{
    JSArrayBuffer *abuf;
    JSTypedArray *ta;

    assert(p->class_id == JS_CLASS_DATAVIEW);
    ta = p->u.typed_array;
    abuf = ta->buffer->u.array_buffer;
    if (abuf->detached)
        return TRUE;
    if (ta->offset > abuf->byte_length)
        return TRUE;
    if (ta->track_rab)
        return FALSE;
    return (int64_t)ta->offset + ta->length > abuf->byte_length;
}

JSObject *get_dataview(JSContext *ctx, JSValueConst this_val)
{
    JSObject *p;
    if (JS_VALUE_GET_TAG(this_val) != JS_TAG_OBJECT)
        goto fail;
    p = JS_VALUE_GET_OBJ(this_val);
    if (p->class_id != JS_CLASS_DATAVIEW) {
    fail:
        JS_ThrowTypeError(ctx, "not a DataView");
        return NULL;
    }
    return p;
}

JSValue js_dataview_get_buffer(JSContext *ctx, JSValueConst this_val)
{
    JSObject *p;
    JSTypedArray *ta;
    p = get_dataview(ctx, this_val);
    if (!p)
        return JS_EXCEPTION;
    ta = p->u.typed_array;
    return JS_DupValue(ctx, JS_MKPTR(JS_TAG_OBJECT, ta->buffer));
}

JSValue js_dataview_get_byteLength(JSContext *ctx, JSValueConst this_val)
{
    JSArrayBuffer *abuf;
    JSTypedArray *ta;
    JSObject *p;

    p = get_dataview(ctx, this_val);
    if (!p)
        return JS_EXCEPTION;
    if (dataview_is_oob(p))
        return JS_ThrowTypeErrorArrayBufferOOB(ctx);
    ta = p->u.typed_array;
    if (ta->track_rab) {
        abuf = ta->buffer->u.array_buffer;
        return JS_NewUint32(ctx, abuf->byte_length - ta->offset);
    }
    return JS_NewUint32(ctx, ta->length);
}

JSValue js_dataview_get_byteOffset(JSContext *ctx, JSValueConst this_val)
{
    JSTypedArray *ta;
    JSObject *p;

    p = get_dataview(ctx, this_val);
    if (!p)
        return JS_EXCEPTION;
    if (dataview_is_oob(p))
        return JS_ThrowTypeErrorArrayBufferOOB(ctx);
    ta = p->u.typed_array;
    return JS_NewUint32(ctx, ta->offset);
}

JSValue js_dataview_getValue(JSContext *ctx,
                                    JSValueConst this_obj,
                                    int argc, JSValueConst *argv, int class_id)
{
    JSTypedArray *ta;
    JSArrayBuffer *abuf;
    BOOL littleEndian, is_swap;
    int size;
    uint8_t *ptr;
    uint32_t v;
    uint64_t pos;

    ta = JS_GetOpaque2(ctx, this_obj, JS_CLASS_DATAVIEW);
    if (!ta)
        return JS_EXCEPTION;
    size = 1 << typed_array_size_log2(class_id);
    if (JS_ToIndex(ctx, &pos, argv[0]))
        return JS_EXCEPTION;
    littleEndian = argc > 1 && JS_ToBool(ctx, argv[1]);
    is_swap = littleEndian ^ !is_be();
    abuf = ta->buffer->u.array_buffer;
    if (abuf->detached)
        return JS_ThrowTypeErrorDetachedArrayBuffer(ctx);
    // order matters: this check should come before the next one
    if ((pos + size) > ta->length)
        return JS_ThrowRangeError(ctx, "out of bound");
    // test262 expects a TypeError for this and V8, in its infinite wisdom,
    // throws a "detached array buffer" exception, but IMO that doesn't make
    // sense because the buffer is not in fact detached, it's still there
    if ((int64_t)ta->offset + ta->length > abuf->byte_length)
        return JS_ThrowTypeError(ctx, "out of bound");
    ptr = abuf->data + ta->offset + pos;

    switch(class_id) {
    case JS_CLASS_INT8_ARRAY:
        return JS_NewInt32(ctx, *(int8_t *)ptr);
    case JS_CLASS_UINT8_ARRAY:
        return JS_NewInt32(ctx, *(uint8_t *)ptr);
    case JS_CLASS_INT16_ARRAY:
        v = get_u16(ptr);
        if (is_swap)
            v = bswap16(v);
        return JS_NewInt32(ctx, (int16_t)v);
    case JS_CLASS_UINT16_ARRAY:
        v = get_u16(ptr);
        if (is_swap)
            v = bswap16(v);
        return JS_NewInt32(ctx, v);
    case JS_CLASS_INT32_ARRAY:
        v = get_u32(ptr);
        if (is_swap)
            v = bswap32(v);
        return JS_NewInt32(ctx, v);
    case JS_CLASS_UINT32_ARRAY:
        v = get_u32(ptr);
        if (is_swap)
            v = bswap32(v);
        return JS_NewUint32(ctx, v);
    case JS_CLASS_BIG_INT64_ARRAY:
        {
            uint64_t v;
            v = get_u64(ptr);
            if (is_swap)
                v = bswap64(v);
            return JS_NewBigInt64(ctx, v);
        }
        break;
    case JS_CLASS_BIG_UINT64_ARRAY:
        {
            uint64_t v;
            v = get_u64(ptr);
            if (is_swap)
                v = bswap64(v);
            return JS_NewBigUint64(ctx, v);
        }
        break;
    case JS_CLASS_FLOAT16_ARRAY:
        {
            uint16_t v;
            v = get_u16(ptr);
            if (is_swap)
                v = bswap16(v);
            return __JS_NewFloat64(ctx, fromfp16(v));
        }
    case JS_CLASS_FLOAT32_ARRAY:
        {
            union {
                float f;
                uint32_t i;
            } u;
            v = get_u32(ptr);
            if (is_swap)
                v = bswap32(v);
            u.i = v;
            return __JS_NewFloat64(ctx, u.f);
        }
    case JS_CLASS_FLOAT64_ARRAY:
        {
            union {
                double f;
                uint64_t i;
            } u;
            u.i = get_u64(ptr);
            if (is_swap)
                u.i = bswap64(u.i);
            return __JS_NewFloat64(ctx, u.f);
        }
    default:
        abort();
    }
}

JSValue js_dataview_setValue(JSContext *ctx,
                                    JSValueConst this_obj,
                                    int argc, JSValueConst *argv, int class_id)
{
    JSTypedArray *ta;
    JSArrayBuffer *abuf;
    BOOL littleEndian, is_swap;
    int size;
    uint8_t *ptr;
    uint64_t v64;
    uint32_t v;
    uint64_t pos;
    JSValueConst val;

    ta = JS_GetOpaque2(ctx, this_obj, JS_CLASS_DATAVIEW);
    if (!ta)
        return JS_EXCEPTION;
    size = 1 << typed_array_size_log2(class_id);
    if (JS_ToIndex(ctx, &pos, argv[0]))
        return JS_EXCEPTION;
    val = argv[1];
    v = 0; /* avoid warning */
    v64 = 0; /* avoid warning */
    if (class_id <= JS_CLASS_UINT32_ARRAY) {
        if (JS_ToUint32(ctx, &v, val))
            return JS_EXCEPTION;
    } else if (class_id <= JS_CLASS_BIG_UINT64_ARRAY) {
        if (JS_ToBigInt64(ctx, (int64_t *)&v64, val))
            return JS_EXCEPTION;
    } else {
        double d;
        if (JS_ToFloat64(ctx, &d, val))
            return JS_EXCEPTION;
        if (class_id == JS_CLASS_FLOAT16_ARRAY) {
            v = tofp16(d);
        } else if (class_id == JS_CLASS_FLOAT32_ARRAY) {
            union {
                float f;
                uint32_t i;
            } u;
            u.f = d;
            v = u.i;
        } else {
            JSFloat64Union u;
            u.d = d;
            v64 = u.u64;
        }
    }
    littleEndian = argc > 2 && JS_ToBool(ctx, argv[2]);
    is_swap = littleEndian ^ !is_be();
    abuf = ta->buffer->u.array_buffer;
    if (abuf->detached)
        return JS_ThrowTypeErrorDetachedArrayBuffer(ctx);
    // order matters: this check should come before the next one
    if ((pos + size) > ta->length)
        return JS_ThrowRangeError(ctx, "out of bound");
    // test262 expects a TypeError for this and V8, in its infinite wisdom,
    // throws a "detached array buffer" exception, but IMO that doesn't make
    // sense because the buffer is not in fact detached, it's still there
    if ((int64_t)ta->offset + ta->length > abuf->byte_length)
        return JS_ThrowTypeError(ctx, "out of bound");
    ptr = abuf->data + ta->offset + pos;

    switch(class_id) {
    case JS_CLASS_INT8_ARRAY:
    case JS_CLASS_UINT8_ARRAY:
        *ptr = v;
        break;
    case JS_CLASS_INT16_ARRAY:
    case JS_CLASS_UINT16_ARRAY:
    case JS_CLASS_FLOAT16_ARRAY:
        if (is_swap)
            v = bswap16(v);
        put_u16(ptr, v);
        break;
    case JS_CLASS_INT32_ARRAY:
    case JS_CLASS_UINT32_ARRAY:
    case JS_CLASS_FLOAT32_ARRAY:
        if (is_swap)
            v = bswap32(v);
        put_u32(ptr, v);
        break;
    case JS_CLASS_BIG_INT64_ARRAY:
    case JS_CLASS_BIG_UINT64_ARRAY:
    case JS_CLASS_FLOAT64_ARRAY:
        if (is_swap)
            v64 = bswap64(v64);
        put_u64(ptr, v64);
        break;
    default:
        abort();
    }
    return JS_UNDEFINED;
}

/* Atomics */
#ifdef CONFIG_ATOMICS

JSObject *js_atomics_get_buf(JSContext *ctx,
                                    JSValueConst obj, JSValueConst idx_val,
                                    uint64_t *pidx, int is_waitable)
{
    JSObject *p;
    JSTypedArray *ta;
    JSArrayBuffer *abuf;
    uint64_t idx;
    BOOL err;
    int old_len;

    if (JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT)
        goto fail;
    p = JS_VALUE_GET_OBJ(obj);
    if (is_waitable)
        err = (p->class_id != JS_CLASS_INT32_ARRAY &&
               p->class_id != JS_CLASS_BIG_INT64_ARRAY);
    else
        err = !(p->class_id >= JS_CLASS_INT8_ARRAY &&
                p->class_id <= JS_CLASS_BIG_UINT64_ARRAY);
    if (err) {
    fail:
        JS_ThrowTypeError(ctx, "integer TypedArray expected");
        return NULL;
    }
    ta = p->u.typed_array;
    abuf = ta->buffer->u.array_buffer;
    if (!abuf->shared) {
        if (is_waitable == 2) {
            JS_ThrowTypeError(ctx, "not a SharedArrayBuffer TypedArray");
            return NULL;
        }
        if (abuf->detached) {
            JS_ThrowTypeErrorDetachedArrayBuffer(ctx);
            return NULL;
        }
    }
    old_len = p->u.array.count;

    if (JS_ToIndex(ctx, &idx, idx_val)) {
        return NULL;
    }

    if (idx >= old_len)
        goto oob;

    if (is_waitable != 1) {
        /* RevalidateAtomicAccess() */
        if (typed_array_is_oob(p)) {
            JS_ThrowTypeErrorArrayBufferOOB(ctx);
            return NULL;
        }
        if (idx >= p->u.array.count) {
        oob:
            JS_ThrowRangeError(ctx, "out-of-bound access");
            return NULL;
        }
    }

    *pidx = idx;
    return p;
}

JSValue js_atomics_op(JSContext *ctx,
                             JSValueConst this_obj,
                             int argc, JSValueConst *argv, int op)
{
    int size_log2;
    uint64_t v, a, rep_val, idx;
    void *ptr;
    JSValue ret;
    JSObject *p;

    p = js_atomics_get_buf(ctx, argv[0], argv[1], &idx, 0);
    if (!p)
        return JS_EXCEPTION;
    size_log2 = typed_array_size_log2(p->class_id);
    rep_val = 0;
    if (op == ATOMICS_OP_LOAD) {
        v = 0;
    } else {
        if (size_log2 == 3) {
            int64_t v64;
            if (JS_ToBigInt64(ctx, &v64, argv[2]))
                return JS_EXCEPTION;
            v = v64;
            if (op == ATOMICS_OP_COMPARE_EXCHANGE) {
                if (JS_ToBigInt64(ctx, &v64, argv[3]))
                    return JS_EXCEPTION;
                rep_val = v64;
            }
        } else {
                uint32_t v32;
                if (JS_ToUint32(ctx, &v32, argv[2]))
                    return JS_EXCEPTION;
                v = v32;
                if (op == ATOMICS_OP_COMPARE_EXCHANGE) {
                    if (JS_ToUint32(ctx, &v32, argv[3]))
                        return JS_EXCEPTION;
                    rep_val = v32;
                }
        }
        if (typed_array_is_oob(p))
            return JS_ThrowTypeErrorDetachedArrayBuffer(ctx);
        if (idx >= p->u.array.count)
            return JS_ThrowRangeError(ctx, "out-of-bound access");
    }
    ptr = p->u.array.u.uint8_ptr + ((uintptr_t)idx << size_log2);

    switch(op | (size_log2 << 3)) {

#define OP(op_name, func_name)                          \
    case ATOMICS_OP_ ## op_name | (0 << 3):             \
       a = func_name((_Atomic(uint8_t) *)ptr, v);       \
       break;                                           \
    case ATOMICS_OP_ ## op_name | (1 << 3):             \
        a = func_name((_Atomic(uint16_t) *)ptr, v);     \
        break;                                          \
    case ATOMICS_OP_ ## op_name | (2 << 3):             \
        a = func_name((_Atomic(uint32_t) *)ptr, v);     \
        break;                                          \
    case ATOMICS_OP_ ## op_name | (3 << 3):             \
        a = func_name((_Atomic(uint64_t) *)ptr, v);     \
        break;

        OP(ADD, atomic_fetch_add)
        OP(AND, atomic_fetch_and)
        OP(OR, atomic_fetch_or)
        OP(SUB, atomic_fetch_sub)
        OP(XOR, atomic_fetch_xor)
        OP(EXCHANGE, atomic_exchange)
#undef OP

    case ATOMICS_OP_LOAD | (0 << 3):
        a = atomic_load((_Atomic(uint8_t) *)ptr);
        break;
    case ATOMICS_OP_LOAD | (1 << 3):
        a = atomic_load((_Atomic(uint16_t) *)ptr);
        break;
    case ATOMICS_OP_LOAD | (2 << 3):
        a = atomic_load((_Atomic(uint32_t) *)ptr);
        break;
    case ATOMICS_OP_LOAD | (3 << 3):
        a = atomic_load((_Atomic(uint64_t) *)ptr);
        break;

    case ATOMICS_OP_COMPARE_EXCHANGE | (0 << 3):
        {
            uint8_t v1 = v;
            atomic_compare_exchange_strong((_Atomic(uint8_t) *)ptr, &v1, rep_val);
            a = v1;
        }
        break;
    case ATOMICS_OP_COMPARE_EXCHANGE | (1 << 3):
        {
            uint16_t v1 = v;
            atomic_compare_exchange_strong((_Atomic(uint16_t) *)ptr, &v1, rep_val);
            a = v1;
        }
        break;
    case ATOMICS_OP_COMPARE_EXCHANGE | (2 << 3):
        {
            uint32_t v1 = v;
            atomic_compare_exchange_strong((_Atomic(uint32_t) *)ptr, &v1, rep_val);
            a = v1;
        }
        break;
    case ATOMICS_OP_COMPARE_EXCHANGE | (3 << 3):
        {
            uint64_t v1 = v;
            atomic_compare_exchange_strong((_Atomic(uint64_t) *)ptr, &v1, rep_val);
            a = v1;
        }
        break;
    default:
        abort();
    }

    switch(p->class_id) {
    case JS_CLASS_INT8_ARRAY:
        a = (int8_t)a;
        goto done;
    case JS_CLASS_UINT8_ARRAY:
        a = (uint8_t)a;
        goto done;
    case JS_CLASS_INT16_ARRAY:
        a = (int16_t)a;
        goto done;
    case JS_CLASS_UINT16_ARRAY:
        a = (uint16_t)a;
        goto done;
    case JS_CLASS_INT32_ARRAY:
    done:
        ret = JS_NewInt32(ctx, a);
        break;
    case JS_CLASS_UINT32_ARRAY:
        ret = JS_NewUint32(ctx, a);
        break;
    case JS_CLASS_BIG_INT64_ARRAY:
        ret = JS_NewBigInt64(ctx, a);
        break;
    case JS_CLASS_BIG_UINT64_ARRAY:
        ret = JS_NewBigUint64(ctx, a);
        break;
    default:
        abort();
    }
    return ret;
}

JSValue js_atomics_store(JSContext *ctx,
                                JSValueConst this_obj,
                                int argc, JSValueConst *argv)
{
    int size_log2;
    void *ptr;
    JSValue ret;
    JSObject *p;
    uint64_t idx;
    int64_t v;

    p = js_atomics_get_buf(ctx, argv[0], argv[1], &idx, 0);
    if (!p)
        return JS_EXCEPTION;
    size_log2 = typed_array_size_log2(p->class_id);
    if (size_log2 == 3) {
        ret = JS_ToBigIntFree(ctx, JS_DupValue(ctx, argv[2]));
        if (JS_IsException(ret))
            return ret;
        if (JS_ToBigInt64(ctx, &v, ret)) {
            JS_FreeValue(ctx, ret);
            return JS_EXCEPTION;
        }
    } else {
        uint32_t v32;
        /* XXX: spec, would be simpler to return the written value */
        ret = JS_ToIntegerFree(ctx, JS_DupValue(ctx, argv[2]));
        if (JS_IsException(ret))
            return ret;
        if (JS_ToUint32(ctx, &v32, ret)) {
            JS_FreeValue(ctx, ret);
            return JS_EXCEPTION;
        }
        v = v32;
    }
    if (typed_array_is_oob(p))
        return JS_ThrowTypeErrorDetachedArrayBuffer(ctx);
    if (idx >= p->u.array.count)
        return JS_ThrowRangeError(ctx, "out-of-bound access");

    ptr = p->u.array.u.uint8_ptr + ((uintptr_t)idx << size_log2);

    switch(size_log2) {
    case 0:
        atomic_store((_Atomic(uint8_t) *)ptr, v);
        break;
    case 1:
        atomic_store((_Atomic(uint16_t) *)ptr, v);
        break;
    case 2:
        atomic_store((_Atomic(uint32_t) *)ptr, v);
        break;
    case 3:
        atomic_store((_Atomic(uint64_t) *)ptr, v);
        break;
    default:
        abort();
    }
    return ret;
}

JSValue js_atomics_isLockFree(JSContext *ctx,
                                     JSValueConst this_obj,
                                     int argc, JSValueConst *argv)
{
    int v, ret;
    if (JS_ToInt32Sat(ctx, &v, argv[0]))
        return JS_EXCEPTION;
    ret = (v == 1 || v == 2 || v == 4 || v == 8);
    return JS_NewBool(ctx, ret);
}

// no-op: Atomics.pause() is not allowed to block or yield to another
// thread, only to hint the CPU that it should back off for a bit;
// the amount of work we do here is a good enough substitute
JSValue js_atomics_pause(JSContext *ctx, JSValueConst this_obj,
                                int argc, JSValueConst *argv)
{
    double d;

    if (argc > 0) {
        switch (JS_VALUE_GET_NORM_TAG(argv[0])) {
        case JS_TAG_FLOAT64: // accepted if and only if fraction == 0.0
            d = JS_VALUE_GET_FLOAT64(argv[0]);
            if (isfinite(d))
                if (0 == modf(d, &d))
                    break;
            // fallthru
        default:
            return JS_ThrowTypeError(ctx, "not an integral number");
        case JS_TAG_UNDEFINED:
        case JS_TAG_INT:
            break;
        }
    }
    cpu_pause();
    return JS_UNDEFINED;
}

JSValue js_atomics_wait(JSContext *ctx,
                               JSValueConst this_obj,
                               int argc, JSValueConst *argv)
{
    JSObject *p;
    int64_t v;
    int32_t v32;
    uint64_t idx;
    void *ptr;
    int64_t timeout;
    struct timespec ts;
    JSAtomicsWaiter waiter_s, *waiter;
    int ret, size_log2, res;
    double d;

    p = js_atomics_get_buf(ctx, argv[0], argv[1], &idx, 2);
    if (!p)
        return JS_EXCEPTION;
    size_log2 = typed_array_size_log2(p->class_id);
    ptr = p->u.array.u.uint8_ptr + ((uintptr_t)idx << size_log2);

    /* 'argv[0]' is a SharedArrayBuffer so it cannot be detached nor reduced */
    if (size_log2 == 3) {
        if (JS_ToBigInt64(ctx, &v, argv[2]))
            return JS_EXCEPTION;
    } else {
        if (JS_ToInt32(ctx, &v32, argv[2]))
            return JS_EXCEPTION;
        v = v32;
    }
    if (JS_ToFloat64(ctx, &d, argv[3]))
        return JS_EXCEPTION;
    /* must use INT64_MAX + 1 because INT64_MAX cannot be exactly represented as a double */
    if (isnan(d) || d >= 0x1p63)
        timeout = INT64_MAX;
    else if (d < 0)
        timeout = 0;
    else
        timeout = (int64_t)d;
    if (!ctx->rt->can_block)
        return JS_ThrowTypeError(ctx, "cannot block in this thread");

    /* XXX: inefficient if large number of waiters, should hash on
       'ptr' value */
    /* XXX: use Linux futexes when available ? */
    pthread_mutex_lock(&js_atomics_mutex);
    if (size_log2 == 3) {
        res = *(int64_t *)ptr != v;
    } else {
        res = *(int32_t *)ptr != v;
    }
    if (res) {
        pthread_mutex_unlock(&js_atomics_mutex);
        return JS_AtomToString(ctx, JS_ATOM_not_equal);
    }

    waiter = &waiter_s;
    waiter->ptr = ptr;
    pthread_cond_init(&waiter->cond, NULL);
    waiter->linked = TRUE;
    list_add_tail(&waiter->link, &js_atomics_waiter_list);

    if (timeout == INT64_MAX) {
        pthread_cond_wait(&waiter->cond, &js_atomics_mutex);
        ret = 0;
    } else {
        /* XXX: use clock monotonic */
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += timeout / 1000;
        ts.tv_nsec += (timeout % 1000) * 1000000;
        if (ts.tv_nsec >= 1000000000) {
            ts.tv_nsec -= 1000000000;
            ts.tv_sec++;
        }
        ret = pthread_cond_timedwait(&waiter->cond, &js_atomics_mutex,
                                     &ts);
    }
    if (waiter->linked)
        list_del(&waiter->link);
    pthread_mutex_unlock(&js_atomics_mutex);
    pthread_cond_destroy(&waiter->cond);
    if (ret == ETIMEDOUT) {
        return JS_AtomToString(ctx, JS_ATOM_timed_out);
    } else {
        return JS_AtomToString(ctx, JS_ATOM_ok);
    }
}

JSValue js_atomics_notify(JSContext *ctx,
                                 JSValueConst this_obj,
                                 int argc, JSValueConst *argv)
{
    struct list_head *el, *el1, waiter_list;
    int32_t count, n;
    uint64_t idx;
    int size_log2;
    void *ptr;
    JSAtomicsWaiter *waiter;
    JSArrayBuffer *abuf;
    JSObject *p;

    p = js_atomics_get_buf(ctx, argv[0], argv[1], &idx, 1);
    if (!p)
        return JS_EXCEPTION;
    size_log2 = typed_array_size_log2(p->class_id);

    if (JS_IsUndefined(argv[2])) {
        count = INT32_MAX;
    } else {
        if (JS_ToInt32Clamp(ctx, &count, argv[2], 0, INT32_MAX, 0))
            return JS_EXCEPTION;
    }

    n = 0;
    abuf = p->u.typed_array->buffer->u.array_buffer;
    if (abuf->shared && count > 0) {
        /* 'argv[0]' is a SharedArrayBuffer so it cannot be detached nor reduced */
        ptr = p->u.array.u.uint8_ptr + ((uintptr_t)idx << size_log2);
        pthread_mutex_lock(&js_atomics_mutex);
        init_list_head(&waiter_list);
        list_for_each_safe(el, el1, &js_atomics_waiter_list) {
            waiter = list_entry(el, JSAtomicsWaiter, link);
            if (waiter->ptr == ptr) {
                list_del(&waiter->link);
                waiter->linked = FALSE;
                list_add_tail(&waiter->link, &waiter_list);
                n++;
                if (n >= count)
                    break;
            }
        }
        list_for_each(el, &waiter_list) {
            waiter = list_entry(el, JSAtomicsWaiter, link);
            pthread_cond_signal(&waiter->cond);
        }
        pthread_mutex_unlock(&js_atomics_mutex);
    }
    return JS_NewInt32(ctx, n);
}

int JS_AddIntrinsicAtomics(JSContext *ctx)
{
    /* add Atomics as autoinit object */
    return JS_SetPropertyFunctionList(ctx, ctx->global_obj, js_atomics_obj, countof(js_atomics_obj));
}

#endif /* CONFIG_ATOMICS */

/* WeakRef */

void js_weakref_finalizer(JSRuntime *rt, JSValue val)
{
    JSWeakRefData *wrd = JS_GetOpaque(val, JS_CLASS_WEAK_REF);
    if (!wrd)
        return;
    js_weakref_free(rt, wrd->target);
    list_del(&wrd->weakref_header.link);
    js_free_rt(rt, wrd);
}

void weakref_delete_weakref(JSRuntime *rt, JSWeakRefHeader *wh)
{
    JSWeakRefData *wrd = container_of(wh, JSWeakRefData, weakref_header);

    if (!js_weakref_is_live(wrd->target)) {
        js_weakref_free(rt, wrd->target);
        wrd->target = JS_UNDEFINED;
    }
}

JSValue js_weakref_constructor(JSContext *ctx, JSValueConst new_target,
                                      int argc, JSValueConst *argv)
{
    JSValueConst arg;
    JSValue obj;

    if (JS_IsUndefined(new_target))
        return JS_ThrowTypeError(ctx, "constructor requires 'new'");
    arg = argv[0];
    if (!js_weakref_is_target(arg))
        return JS_ThrowTypeError(ctx, "invalid target");
    obj = js_create_from_ctor(ctx, new_target, JS_CLASS_WEAK_REF);
    if (JS_IsException(obj))
        return JS_EXCEPTION;
    JSWeakRefData *wrd = js_mallocz(ctx, sizeof(*wrd));
    if (!wrd) {
        JS_FreeValue(ctx, obj);
        return JS_EXCEPTION;
    }
    wrd->target = js_weakref_new(ctx, arg);
    wrd->weakref_header.weakref_type = JS_WEAKREF_TYPE_WEAKREF;
    list_add_tail(&wrd->weakref_header.link, &ctx->rt->weakref_list);
    JS_SetOpaque(obj, wrd);
    return obj;
}

JSValue js_weakref_deref(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    JSWeakRefData *wrd = JS_GetOpaque2(ctx, this_val, JS_CLASS_WEAK_REF);
    if (!wrd)
        return JS_EXCEPTION;
    if (js_weakref_is_live(wrd->target))
        return JS_DupValue(ctx, wrd->target);
    else
        return JS_UNDEFINED;
}

void js_finrec_finalizer(JSRuntime *rt, JSValue val)
{
    JSFinalizationRegistryData *frd = JS_GetOpaque(val, JS_CLASS_FINALIZATION_REGISTRY);
    if (frd) {
        struct list_head *el, *el1;
        list_for_each_safe(el, el1, &frd->entries) {
            JSFinRecEntry *fre = list_entry(el, JSFinRecEntry, link);
            js_weakref_free(rt, fre->target);
            js_weakref_free(rt, fre->token);
            JS_FreeValueRT(rt, fre->held_val);
            js_free_rt(rt, fre);
        }
        JS_FreeValueRT(rt, frd->cb);
        JS_FreeContext(frd->realm);
        list_del(&frd->weakref_header.link);
        js_free_rt(rt, frd);
    }
}

void js_finrec_mark(JSRuntime *rt, JSValueConst val,
                           JS_MarkFunc *mark_func)
{
    JSFinalizationRegistryData *frd = JS_GetOpaque(val, JS_CLASS_FINALIZATION_REGISTRY);
    struct list_head *el;
    if (frd) {
        list_for_each(el, &frd->entries) {
            JSFinRecEntry *fre = list_entry(el, JSFinRecEntry, link);
            JS_MarkValue(rt, fre->held_val, mark_func);
        }
        JS_MarkValue(rt, frd->cb, mark_func);
        mark_func(rt, &frd->realm->header);
    }
}

JSValue js_finrec_job(JSContext *ctx, int argc, JSValueConst *argv)
{
    return JS_Call(ctx, argv[0], JS_UNDEFINED, 1, &argv[1]);
}

void finrec_delete_weakref(JSRuntime *rt, JSWeakRefHeader *wh)
{
    JSFinalizationRegistryData *frd = container_of(wh, JSFinalizationRegistryData, weakref_header);
    struct list_head *el, *el1;

    list_for_each_safe(el, el1, &frd->entries) {
        JSFinRecEntry *fre = list_entry(el, JSFinRecEntry, link);

        if (!js_weakref_is_live(fre->token)) {
            js_weakref_free(rt, fre->token);
            fre->token = JS_UNDEFINED;
        }

        if (!js_weakref_is_live(fre->target)) {
            JSValueConst args[2];
            args[0] = frd->cb;
            args[1] = fre->held_val;
            /* no exception is raised to avoid recursing into the GC */
            JS_EnqueueJob2(frd->realm, js_finrec_job, 2, args, TRUE);

            js_weakref_free(rt, fre->target);
            js_weakref_free(rt, fre->token);
            JS_FreeValueRT(rt, fre->held_val);
            list_del(&fre->link);
            js_free_rt(rt, fre);
        }
    }
}

JSValue js_finrec_constructor(JSContext *ctx, JSValueConst new_target,
                                     int argc, JSValueConst *argv)
{
    JSValueConst cb;
    JSValue obj;
    JSFinalizationRegistryData *frd;

    if (JS_IsUndefined(new_target))
        return JS_ThrowTypeError(ctx, "constructor requires 'new'");
    cb = argv[0];
    if (!JS_IsFunction(ctx, cb))
        return JS_ThrowTypeError(ctx, "argument must be a function");

    obj = js_create_from_ctor(ctx, new_target, JS_CLASS_FINALIZATION_REGISTRY);
    if (JS_IsException(obj))
        return JS_EXCEPTION;
    frd = js_mallocz(ctx, sizeof(*frd));
    if (!frd) {
        JS_FreeValue(ctx, obj);
        return JS_EXCEPTION;
    }
    frd->weakref_header.weakref_type = JS_WEAKREF_TYPE_FINREC;
    list_add_tail(&frd->weakref_header.link, &ctx->rt->weakref_list);
    init_list_head(&frd->entries);
    frd->realm = JS_DupContext(ctx);
    frd->cb = JS_DupValue(ctx, cb);
    JS_SetOpaque(obj, frd);
    return obj;
}

JSValue js_finrec_register(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv)
{
    JSValueConst target, held_val, token;
    JSFinalizationRegistryData *frd;
    JSFinRecEntry *fre;

    frd = JS_GetOpaque2(ctx, this_val, JS_CLASS_FINALIZATION_REGISTRY);
    if (!frd)
        return JS_EXCEPTION;
    target = argv[0];
    held_val = argv[1];
    token = argc > 2 ? argv[2] : JS_UNDEFINED;

    if (!js_weakref_is_target(target))
        return JS_ThrowTypeError(ctx, "invalid target");
    if (js_same_value(ctx, target, held_val))
        return JS_ThrowTypeError(ctx, "held value cannot be the target");
    if (!JS_IsUndefined(token) && !js_weakref_is_target(token))
        return JS_ThrowTypeError(ctx, "invalid unregister token");
    fre = js_malloc(ctx, sizeof(*fre));
    if (!fre)
        return JS_EXCEPTION;
    fre->target = js_weakref_new(ctx, target);
    fre->held_val = JS_DupValue(ctx, held_val);
    fre->token = js_weakref_new(ctx, token);
    list_add_tail(&fre->link, &frd->entries);
    return JS_UNDEFINED;
}

JSValue js_finrec_unregister(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    JSFinalizationRegistryData *frd = JS_GetOpaque2(ctx, this_val, JS_CLASS_FINALIZATION_REGISTRY);
    JSValueConst token;
    BOOL removed;
    struct list_head *el, *el1;

    if (!frd)
        return JS_EXCEPTION;
    token = argv[0];
    if (!js_weakref_is_target(token))
        return JS_ThrowTypeError(ctx, "invalid unregister token");

    removed = FALSE;
    list_for_each_safe(el, el1, &frd->entries) {
        JSFinRecEntry *fre = list_entry(el, JSFinRecEntry, link);
        if (js_weakref_is_live(fre->token) && js_same_value(ctx, fre->token, token)) {
            js_weakref_free(ctx->rt, fre->target);
            js_weakref_free(ctx->rt, fre->token);
            JS_FreeValue(ctx, fre->held_val);
            list_del(&fre->link);
            js_free(ctx, fre);
            removed = TRUE;
        }
    }
    return JS_NewBool(ctx, removed);
}

JSValue js_array_buffer_resize(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv, int class_id)
{
    JSArrayBuffer *abuf;
    uint8_t *data;
    int64_t len;

    abuf = JS_GetOpaque2(ctx, this_val, class_id);
    if (!abuf)
        return JS_EXCEPTION;
    if (JS_ToInt64(ctx, &len, argv[0]))
        return JS_EXCEPTION;
    if (abuf->detached)
        return JS_ThrowTypeErrorDetachedArrayBuffer(ctx);
    if (!array_buffer_is_resizable(abuf))
        return JS_ThrowTypeError(ctx, "array buffer is not resizable");
    // TODO(bnoordhuis) support externally managed RABs
    if (abuf->free_func != js_array_buffer_free)
        return JS_ThrowTypeError(ctx, "external array buffer is not resizable");
    if (len < 0 || len > abuf->max_byte_length) {
    bad_length:
        return JS_ThrowRangeError(ctx, "invalid array buffer length");
    }
    // SABs can only grow and we don't need to realloc because
    // js_array_buffer_constructor3 commits all memory upfront;
    // regular RABs are resizable both ways and realloc
    if (abuf->shared) {
        if (len < abuf->byte_length)
            goto bad_length;
        // Note this is off-spec; there's supposed to be a single atomic
        // |byteLength| property that's shared across SABs but we store
        // it per SAB instead. That means when thread A calls sab.grow(2)
        // at time t0, and thread B calls sab.grow(1) at time t1, we don't
        // throw a TypeError in thread B as the spec says we should,
        // instead both threads get their own view of the backing memory,
        // 2 bytes big in A, and 1 byte big in B
        abuf->byte_length = len;
    } else {
        data = js_realloc(ctx, abuf->data, max_int(len, 1));
        if (!data)
            return JS_EXCEPTION;
        if (len > abuf->byte_length)
            memset(&data[abuf->byte_length], 0, len - abuf->byte_length);
        abuf->byte_length = len;
        abuf->data = data;
    }
    js_array_buffer_update_typed_arrays(abuf);
    return JS_UNDEFINED;
}

JSValue js_array_buffer_slice(JSContext *ctx,
                                     JSValueConst this_val,
                                     int argc, JSValueConst *argv, int class_id)
{
    JSArrayBuffer *abuf, *new_abuf;
    int64_t len, start, end, new_len;
    JSValue ctor, new_obj;

    abuf = JS_GetOpaque2(ctx, this_val, class_id);
    if (!abuf)
        return JS_EXCEPTION;
    if (abuf->detached)
        return JS_ThrowTypeErrorDetachedArrayBuffer(ctx);
    len = abuf->byte_length;

    if (JS_ToInt64Clamp(ctx, &start, argv[0], 0, len, len))
        return JS_EXCEPTION;

    end = len;
    if (!JS_IsUndefined(argv[1])) {
        if (JS_ToInt64Clamp(ctx, &end, argv[1], 0, len, len))
            return JS_EXCEPTION;
    }
    new_len = max_int64(end - start, 0);
    ctor = JS_SpeciesConstructor(ctx, this_val, JS_UNDEFINED);
    if (JS_IsException(ctor))
        return ctor;
    if (JS_IsUndefined(ctor)) {
        new_obj = js_array_buffer_constructor2(ctx, JS_UNDEFINED, new_len,
                                               NULL, class_id);
    } else {
        JSValue args[1];
        args[0] = JS_NewInt64(ctx, new_len);
        new_obj = JS_CallConstructor(ctx, ctor, 1, (JSValueConst *)args);
        JS_FreeValue(ctx, ctor);
        JS_FreeValue(ctx, args[0]);
    }
    if (JS_IsException(new_obj))
        return new_obj;
    new_abuf = JS_GetOpaque2(ctx, new_obj, class_id);
    if (!new_abuf)
        goto fail;
    if (js_same_value(ctx, new_obj, this_val)) {
        JS_ThrowTypeError(ctx, "cannot use identical ArrayBuffer");
        goto fail;
    }
    if (new_abuf->detached) {
        JS_ThrowTypeErrorDetachedArrayBuffer(ctx);
        goto fail;
    }
    if (new_abuf->byte_length < new_len) {
        JS_ThrowTypeError(ctx, "new ArrayBuffer is too small");
        goto fail;
    }
    /* must test again because of side effects */
    if (abuf->detached || abuf->byte_length < start + new_len) {
        JS_ThrowTypeErrorDetachedArrayBuffer(ctx);
        goto fail;
    }
    memcpy(new_abuf->data, abuf->data + start, new_len);
    return new_obj;
 fail:
    JS_FreeValue(ctx, new_obj);
    return JS_EXCEPTION;
}

JSObject *get_proto_obj(JSValueConst proto_val)
{
    if (JS_VALUE_GET_TAG(proto_val) != JS_TAG_OBJECT)
        return NULL;
    else
        return JS_VALUE_GET_OBJ(proto_val);
}

/* WARNING: the shape is not hashed. It is used for objects where
   factorizing the shape is not relevant (prototypes, constructors) */
JSValue JS_NewObjectProtoClassAlloc(JSContext *ctx, JSValueConst proto_val,
                                           JSClassID class_id, int n_alloc_props)
{
    JSShape *sh;
    JSObject *proto;
    int hash_size, hash_bits;

    JS_LOG("JS_NewObjectProtoClassAlloc", "Entered, class_id=%d, n_alloc_props=%d", class_id, n_alloc_props);

    if (n_alloc_props <= JS_PROP_INITIAL_SIZE) {
        n_alloc_props = JS_PROP_INITIAL_SIZE;
        hash_size = JS_PROP_INITIAL_HASH_SIZE;
    } else {
        hash_bits = 32 - clz32(n_alloc_props - 1);
        hash_size = 1 << hash_bits;
    }

    proto = get_proto_obj(proto_val); // ((JSObject *)(void *)(intptr_t)(proto_val));
    JS_LOG("JS_NewObjectProtoClassAlloc", "get_proto_obj returned %04X:%04X. ", FARPTR_SEG(proto), FARPTR_OFF(proto));

    sh = js_new_shape_nohash(ctx, proto, hash_size, n_alloc_props);
    if (!sh) {
        JS_LOG("JS_NewObjectProtoClassAlloc", "js_new_shape_nohash failed");
        return JS_EXCEPTION;
    }
    JS_LOG("JS_NewObjectProtoClassAlloc", "js_new_shape_nohash ok");

    JSValue obj = JS_NewObjectFromShape(ctx, sh, class_id, NULL);
	
	JS_LOG("JS_NewObjectProtoClassAlloc", "JS_NewObjectFromShape done");
    if (JS_IsException(obj)) {
        JS_LOG("JS_NewObjectProtoClassAlloc", "JS_NewObjectFromShape failed");
        return JS_EXCEPTION;
    }
    JS_LOG("JS_NewObjectProtoClassAlloc", "success");
    return obj;
}

#if 0
JSValue JS_GetObjectData(JSContext *ctx, JSValueConst obj)
{
    JSObject *p;

    if (JS_VALUE_GET_TAG(obj) == JS_TAG_OBJECT) {
        p = JS_VALUE_GET_OBJ(obj);
        switch(p->class_id) {
        case JS_CLASS_NUMBER:
        case JS_CLASS_STRING:
        case JS_CLASS_BOOLEAN:
        case JS_CLASS_SYMBOL:
        case JS_CLASS_DATE:
        case JS_CLASS_BIG_INT:
            return JS_DupValue(ctx, p->u.object_data);
        }
    }
    return JS_UNDEFINED;
}
#endif

/* remove the deleted properties. */
int compact_properties(JSContext *ctx, JSObject *p)
{
    JSShape *sh, *old_sh;
    intptr_t h;
    uint32_t new_hash_size, i, j, new_hash_mask, new_size;
    JSShapeProperty *old_pr, *pr;
    JSProperty *prop, *new_prop;

    sh = p->shape;
    assert(!sh->is_hashed);

    new_size = max_int(JS_PROP_INITIAL_SIZE,
                       sh->prop_count - sh->deleted_prop_count);
    assert(new_size <= sh->prop_size);

    new_hash_size = sh->prop_hash_mask + 1;
    while ((new_hash_size / 2) >= new_size)
        new_hash_size = new_hash_size / 2;
    new_hash_mask = new_hash_size - 1;

    /* resize the hash table and the properties */
    old_sh = sh;
    sh = js_malloc(ctx, get_shape_size(new_hash_size, new_size));
    if (!sh)
        return -1;
    remove_gc_object(&old_sh->header);

    js_rc(sh)->ref_count = 1;
    add_gc_object(ctx->rt, &sh->header, JS_GC_OBJ_TYPE_SHAPE);

    memcpy(&sh->header + 1, &old_sh->header + 1,
           sizeof(JSShape) - sizeof(JSGCObjectHeader));

    memset(sh->hash_table, 0, sizeof(sh->hash_table[0]) * new_hash_size);
    sh->prop_hash_mask = new_hash_mask;

    j = 0;
    old_pr = get_shape_prop(old_sh);
    pr = get_shape_prop(sh);
    prop = p->prop;
    for(i = 0; i < sh->prop_count; i++) {
        if (old_pr->atom != JS_ATOM_NULL) {
            pr->atom = old_pr->atom;
            pr->flags = old_pr->flags;
            h = ((uintptr_t)old_pr->atom & new_hash_mask);
            pr->hash_next = sh->hash_table[h];
            sh->hash_table[h] = j + 1;
            prop[j] = prop[i];
            j++;
            pr++;
        }
        old_pr++;
    }
    assert(j == (sh->prop_count - sh->deleted_prop_count));
    sh->prop_size = new_size;
    sh->deleted_prop_count = 0;
    sh->prop_count = j;

    p->shape = sh;
    js_free(ctx, old_sh);

    /* reduce the size of the object properties */
    new_prop = js_realloc(ctx, p->prop, sizeof(new_prop[0]) * new_size);
    if (new_prop)
        p->prop = new_prop;
    return 0;
}

int add_shape_property(JSContext *ctx, JSShape **psh,
                              JSObject *p, JSAtom atom, int prop_flags)
{
    JSRuntime *rt = ctx->rt;
    JSShape *sh = *psh;
    JSShapeProperty *pr, *prop;
    uint32_t hash_mask, new_shape_hash = 0;
    intptr_t h;

    /* update the shape hash */
    if (sh->is_hashed) {
        js_shape_hash_unlink(rt, sh);
        new_shape_hash = shape_hash(shape_hash(sh->hash, atom), prop_flags);
    }

    if (unlikely(sh->prop_count >= sh->prop_size)) {
        if (resize_properties(ctx, psh, p, sh->prop_count + 1)) {
            /* in case of error, reinsert in the hash table.
               sh is still valid if resize_properties() failed */
            if (sh->is_hashed)
                js_shape_hash_link(rt, sh);
            return -1;
        }
        sh = *psh;
    }
    if (sh->is_hashed) {
        sh->hash = new_shape_hash;
        js_shape_hash_link(rt, sh);
    }
    /* Initialize the new shape property.
       The object property at p->prop[sh->prop_count] is uninitialized */
    prop = get_shape_prop(sh);
    pr = &prop[sh->prop_count++];
    pr->atom = JS_DupAtom(ctx, atom);
    pr->flags = prop_flags;
    /* add in hash table */
    hash_mask = sh->prop_hash_mask;
    h = atom & hash_mask;
    pr->hash_next = sh->hash_table[h];
    sh->hash_table[h] = sh->prop_count;
    return 0;
}

/* find a hashed empty shape matching the prototype. Return NULL if
   not found */
JSShape *find_hashed_shape_proto(JSRuntime *rt, JSObject *proto)
{
    JSShape *sh1;
    uint32_t h, h1;

    h = shape_initial_hash(proto);
    h1 = get_shape_hash(h, rt->shape_hash_bits);
    for(sh1 = rt->shape_hash[h1]; sh1 != NULL; sh1 = sh1->shape_hash_next) {
        if (sh1->hash == h &&
            sh1->proto == proto &&
            sh1->prop_count == 0) {
            return sh1;
        }
    }
    return NULL;
}

JSValue js_object_getOwnPropertyDescriptors(JSContext *ctx, JSValueConst this_val,
                                                   int argc, JSValueConst *argv)
{
    //getOwnPropertyDescriptors(obj)
    JSValue obj, r;
    JSObject *p;
    JSPropertyEnum *props;
    uint32_t len, i;

    r = JS_UNDEFINED;
    obj = JS_ToObject(ctx, argv[0]);
    if (JS_IsException(obj))
        return JS_EXCEPTION;

    p = JS_VALUE_GET_OBJ(obj);
    if (JS_GetOwnPropertyNamesInternal(ctx, &props, &len, p,
                               JS_GPN_STRING_MASK | JS_GPN_SYMBOL_MASK))
        goto exception;
    r = JS_NewObject(ctx);
    if (JS_IsException(r))
        goto exception;
    for(i = 0; i < len; i++) {
        JSValue atomValue, desc;
        JSValueConst args[2];

        atomValue = JS_AtomToValue(ctx, props[i].atom);
        if (JS_IsException(atomValue))
            goto exception;
        args[0] = obj;
        args[1] = atomValue;
        desc = js_object_getOwnPropertyDescriptor(ctx, JS_UNDEFINED, 2, args, 0);
        JS_FreeValue(ctx, atomValue);
        if (JS_IsException(desc))
            goto exception;
        if (!JS_IsUndefined(desc)) {
            if (JS_DefinePropertyValue(ctx, r, props[i].atom, desc,
                                       JS_PROP_C_W_E | JS_PROP_THROW) < 0)
                goto exception;
        }
    }
    JS_FreePropertyEnum(ctx, props, len);
    JS_FreeValue(ctx, obj);
    return r;

exception:
    JS_FreePropertyEnum(ctx, props, len);
    JS_FreeValue(ctx, obj);
    JS_FreeValue(ctx, r);
    return JS_EXCEPTION;
}
