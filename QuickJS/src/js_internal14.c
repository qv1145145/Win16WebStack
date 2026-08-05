#include "js_internal.h"



JSValue js_array_every(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv, int special)
{
    JSValue obj, val, index_val, res, ret;
    JSValueConst args[3];
    JSValueConst func, this_arg;
    int64_t len, k, n;
    int present;

    ret = JS_UNDEFINED;
    val = JS_UNDEFINED;
    if (special & special_TA) {
        obj = JS_DupValue(ctx, this_val);
        len = js_typed_array_get_length_unsafe(ctx, obj);
        if (len < 0)
            goto exception;
    } else {
        obj = JS_ToObject(ctx, this_val);
        if (js_get_length64(ctx, &len, obj))
            goto exception;
    }
    func = argv[0];
    this_arg = JS_UNDEFINED;
    if (argc > 1)
        this_arg = argv[1];

    if (check_function(ctx, func))
        goto exception;

    switch (special) {
    case special_every:
    case special_every | special_TA:
        ret = JS_TRUE;
        break;
    case special_some:
    case special_some | special_TA:
        ret = JS_FALSE;
        break;
    case special_map:
        ret = JS_ArraySpeciesCreate(ctx, obj, len);
        if (JS_IsException(ret))
            goto exception;
        break;
    case special_filter:
        ret = JS_ArraySpeciesCreate(ctx, obj, 0);
        if (JS_IsException(ret))
            goto exception;
        break;
    case special_map | special_TA:
        args[0] = obj;
        args[1] = JS_NewInt32(ctx, len);
        ret = js_typed_array___speciesCreate(ctx, JS_UNDEFINED, 2, args);
        if (JS_IsException(ret))
            goto exception;
        break;
    case special_filter | special_TA:
        ret = JS_NewArray(ctx);
        if (JS_IsException(ret))
            goto exception;
        break;
    }
    n = 0;

    for(k = 0; k < len; k++) {
        if (special & special_TA) {
            val = JS_GetPropertyInt64(ctx, obj, k);
            if (JS_IsException(val))
                goto exception;
            present = TRUE;
        } else {
            present = JS_TryGetPropertyInt64(ctx, obj, k, &val);
            if (present < 0)
                goto exception;
        }
        if (present) {
            index_val = JS_NewInt64(ctx, k);
            if (JS_IsException(index_val))
                goto exception;
            args[0] = val;
            args[1] = index_val;
            args[2] = obj;
            res = JS_Call(ctx, func, this_arg, 3, args);
            JS_FreeValue(ctx, index_val);
            if (JS_IsException(res))
                goto exception;
            switch (special) {
            case special_every:
            case special_every | special_TA:
                if (!JS_ToBoolFree(ctx, res)) {
                    ret = JS_FALSE;
                    goto done;
                }
                break;
            case special_some:
            case special_some | special_TA:
                if (JS_ToBoolFree(ctx, res)) {
                    ret = JS_TRUE;
                    goto done;
                }
                break;
            case special_map:
                if (JS_DefinePropertyValueInt64(ctx, ret, k, res,
                                                JS_PROP_C_W_E | JS_PROP_THROW) < 0)
                    goto exception;
                break;
            case special_map | special_TA:
                if (JS_SetPropertyValue(ctx, ret, JS_NewInt32(ctx, k), res, JS_PROP_THROW) < 0)
                    goto exception;
                break;
            case special_filter:
            case special_filter | special_TA:
                if (JS_ToBoolFree(ctx, res)) {
                    if (JS_DefinePropertyValueInt64(ctx, ret, n++, JS_DupValue(ctx, val),
                                                    JS_PROP_C_W_E | JS_PROP_THROW) < 0)
                        goto exception;
                }
                break;
            default:
                JS_FreeValue(ctx, res);
                break;
            }
            JS_FreeValue(ctx, val);
            val = JS_UNDEFINED;
        }
    }
done:
    if (special == (special_filter | special_TA)) {
        JSValue arr;
        args[0] = obj;
        args[1] = JS_NewInt32(ctx, n);
        arr = js_typed_array___speciesCreate(ctx, JS_UNDEFINED, 2, args);
        if (JS_IsException(arr))
            goto exception;
        args[0] = ret;
        res = JS_Invoke(ctx, arr, JS_ATOM_set, 1, args);
        if (check_exception_free(ctx, res)) {
            JS_FreeValue(ctx, arr);
            goto exception;
        }
        JS_FreeValue(ctx, ret);
        ret = arr;
    }
    JS_FreeValue(ctx, val);
    JS_FreeValue(ctx, obj);
    return ret;

exception:
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, val);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

JSValue js_array_reduce(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv, int special)
{
    JSValue obj, val, index_val, acc, acc1;
    JSValueConst args[4];
    JSValueConst func;
    int64_t len, k, k1;
    int present;

    acc = JS_UNDEFINED;
    val = JS_UNDEFINED;
    if (special & special_TA) {
        obj = JS_DupValue(ctx, this_val);
        len = js_typed_array_get_length_unsafe(ctx, obj);
        if (len < 0)
            goto exception;
    } else {
        obj = JS_ToObject(ctx, this_val);
        if (js_get_length64(ctx, &len, obj))
            goto exception;
    }
    func = argv[0];

    if (check_function(ctx, func))
        goto exception;

    k = 0;
    if (argc > 1) {
        acc = JS_DupValue(ctx, argv[1]);
    } else {
        for(;;) {
            if (k >= len) {
                JS_ThrowTypeError(ctx, "empty array");
                goto exception;
            }
            k1 = (special & special_reduceRight) ? len - k - 1 : k;
            k++;
            if (special & special_TA) {
                acc = JS_GetPropertyInt64(ctx, obj, k1);
                if (JS_IsException(acc))
                    goto exception;
                break;
            } else {
                present = JS_TryGetPropertyInt64(ctx, obj, k1, &acc);
                if (present < 0)
                    goto exception;
                if (present)
                    break;
            }
        }
    }
    for (; k < len; k++) {
        k1 = (special & special_reduceRight) ? len - k - 1 : k;
        if (special & special_TA) {
            val = JS_GetPropertyInt64(ctx, obj, k1);
            if (JS_IsException(val))
                goto exception;
            present = TRUE;
        } else {
            present = JS_TryGetPropertyInt64(ctx, obj, k1, &val);
            if (present < 0)
                goto exception;
        }
        if (present) {
            index_val = JS_NewInt64(ctx, k1);
            if (JS_IsException(index_val))
                goto exception;
            args[0] = acc;
            args[1] = val;
            args[2] = index_val;
            args[3] = obj;
            acc1 = JS_Call(ctx, func, JS_UNDEFINED, 4, args);
            JS_FreeValue(ctx, index_val);
            JS_FreeValue(ctx, val);
            val = JS_UNDEFINED;
            if (JS_IsException(acc1))
                goto exception;
            JS_FreeValue(ctx, acc);
            acc = acc1;
        }
    }
    JS_FreeValue(ctx, obj);
    return acc;

exception:
    JS_FreeValue(ctx, acc);
    JS_FreeValue(ctx, val);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

JSValue js_array_fill(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv)
{
    JSValue obj;
    int64_t len, start, end;

    obj = JS_ToObject(ctx, this_val);
    if (js_get_length64(ctx, &len, obj))
        goto exception;

    start = 0;
    if (argc > 1 && !JS_IsUndefined(argv[1])) {
        if (JS_ToInt64Clamp(ctx, &start, argv[1], 0, len, len))
            goto exception;
    }

    end = len;
    if (argc > 2 && !JS_IsUndefined(argv[2])) {
        if (JS_ToInt64Clamp(ctx, &end, argv[2], 0, len, len))
            goto exception;
    }

    /* XXX: should special case fast arrays */
    while (start < end) {
        if (JS_SetPropertyInt64(ctx, obj, start,
                                JS_DupValue(ctx, argv[0])) < 0)
            goto exception;
        start++;
    }
    return obj;

 exception:
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

JSValue js_array_includes(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv)
{
    JSValue obj, val;
    int64_t len, n;
    JSValue *arrp;
    uint32_t count;
    int res;

    obj = JS_ToObject(ctx, this_val);
    if (js_get_length64(ctx, &len, obj))
        goto exception;

    res = FALSE;
    if (len > 0) {
        n = 0;
        if (argc > 1) {
            if (JS_ToInt64Clamp(ctx, &n, argv[1], 0, len, len))
                goto exception;
        }
        if (js_get_fast_array(ctx, obj, &arrp, &count)) {
            for (; n < count; n++) {
                if (js_strict_eq2(ctx, argv[0], arrp[n],
                                  JS_EQ_SAME_VALUE_ZERO)) {
                    res = TRUE;
                    goto done;
                }
            }
        }
        for (; n < len; n++) {
            val = JS_GetPropertyInt64(ctx, obj, n);
            if (JS_IsException(val))
                goto exception;
            if (js_strict_eq2(ctx, argv[0], val,
                              JS_EQ_SAME_VALUE_ZERO)) {
                JS_FreeValue(ctx, val);
                res = TRUE;
                break;
            }
            JS_FreeValue(ctx, val);
        }
    }
 done:
    JS_FreeValue(ctx, obj);
    return JS_NewBool(ctx, res);

 exception:
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

JSValue js_array_indexOf(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv)
{
    JSValue obj, val;
    int64_t len, n, res;
    JSValue *arrp;
    uint32_t count;

    obj = JS_ToObject(ctx, this_val);
    if (js_get_length64(ctx, &len, obj))
        goto exception;

    res = -1;
    if (len > 0) {
        n = 0;
        if (argc > 1) {
            if (JS_ToInt64Clamp(ctx, &n, argv[1], 0, len, len))
                goto exception;
        }
        if (js_get_fast_array(ctx, obj, &arrp, &count)) {
            for (; n < count; n++) {
                if (js_strict_eq2(ctx, argv[0], arrp[n], JS_EQ_STRICT)) {
                    res = n;
                    goto done;
                }
            }
        }
        for (; n < len; n++) {
            int present = JS_TryGetPropertyInt64(ctx, obj, n, &val);
            if (present < 0)
                goto exception;
            if (present) {
                if (js_strict_eq2(ctx, argv[0], val, JS_EQ_STRICT)) {
                    JS_FreeValue(ctx, val);
                    res = n;
                    break;
                }
                JS_FreeValue(ctx, val);
            }
        }
    }
 done:
    JS_FreeValue(ctx, obj);
    return JS_NewInt64(ctx, res);

 exception:
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

JSValue js_array_lastIndexOf(JSContext *ctx, JSValueConst this_val,
                                    int argc, JSValueConst *argv)
{
    JSValue obj, val;
    int64_t len, n, res;
    int present;

    obj = JS_ToObject(ctx, this_val);
    if (js_get_length64(ctx, &len, obj))
        goto exception;

    res = -1;
    if (len > 0) {
        n = len - 1;
        if (argc > 1) {
            if (JS_ToInt64Clamp(ctx, &n, argv[1], -1, len - 1, len))
                goto exception;
        }
        /* XXX: should special case fast arrays */
        for (; n >= 0; n--) {
            present = JS_TryGetPropertyInt64(ctx, obj, n, &val);
            if (present < 0)
                goto exception;
            if (present) {
                if (js_strict_eq2(ctx, argv[0], val, JS_EQ_STRICT)) {
                    JS_FreeValue(ctx, val);
                    res = n;
                    break;
                }
                JS_FreeValue(ctx, val);
            }
        }
    }
    JS_FreeValue(ctx, obj);
    return JS_NewInt64(ctx, res);

 exception:
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

JSValue js_array_find(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv, int mode)
{
    JSValueConst func, this_arg;
    JSValueConst args[3];
    JSValue obj, val, index_val, res;
    int64_t len, k, end;
    int dir;

    index_val = JS_UNDEFINED;
    val = JS_UNDEFINED;
    obj = JS_ToObject(ctx, this_val);
    if (js_get_length64(ctx, &len, obj))
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

    // TODO(bnoordhuis) add fast path for fast arrays
    for(; k != end; k += dir) {
        index_val = JS_NewInt64(ctx, k);
        if (JS_IsException(index_val))
            goto exception;
        val = JS_GetPropertyValue(ctx, obj, index_val);
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
                JS_FreeValue(ctx, obj);
                return index_val;
            } else {
                JS_FreeValue(ctx, index_val);
                JS_FreeValue(ctx, obj);
                return val;
            }
        }
        JS_FreeValue(ctx, val);
        JS_FreeValue(ctx, index_val);
    }
    JS_FreeValue(ctx, obj);
    if (mode == ArrayFindIndex || mode == ArrayFindLastIndex)
        return JS_NewInt32(ctx, -1);
    else
        return JS_UNDEFINED;

exception:
    JS_FreeValue(ctx, index_val);
    JS_FreeValue(ctx, val);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

JSValue js_array_toString(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv)
{
    JSValue obj, method, ret;

    obj = JS_ToObject(ctx, this_val);
    if (JS_IsException(obj))
        return JS_EXCEPTION;
    method = JS_GetProperty(ctx, obj, JS_ATOM_join);
    if (JS_IsException(method)) {
        ret = JS_EXCEPTION;
    } else
    if (! JS_IsFunction(ctx, method)) {
        /* Use intrinsic Object.prototype.toString */
        JS_FreeValue(ctx, method);
        ret = js_object_toString(ctx, obj, 0, NULL);
    } else {
        ret = JS_CallFree(ctx, method, obj, 0, NULL);
    }
    JS_FreeValue(ctx, obj);
    return ret;
}

JSValue js_array_join(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv, int toLocaleString)
{
    JSValue obj, sep = JS_UNDEFINED, el;
    StringBuffer b_s, *b = &b_s;
    JSString *p = NULL;
    int64_t i, n;
    int c;

    obj = JS_ToObject(ctx, this_val);
    if (js_get_length64(ctx, &n, obj))
        goto exception;

    c = ',';    /* default separator */
    if (!toLocaleString && argc > 0 && !JS_IsUndefined(argv[0])) {
        sep = JS_ToString(ctx, argv[0]);
        if (JS_IsException(sep))
            goto exception;
        p = JS_VALUE_GET_STRING(sep);
        if (p->len == 1 && !p->is_wide_char)
            c = p->u.str8[0];
        else
            c = -1;
    }
    string_buffer_init(ctx, b, 0);

    for(i = 0; i < n; i++) {
        if (i > 0) {
            if (c >= 0) {
                string_buffer_putc8(b, c);
            } else {
                string_buffer_concat(b, p, 0, p->len);
            }
        }
        el = JS_GetPropertyUint32(ctx, obj, i);
        if (JS_IsException(el))
            goto fail;
        if (!JS_IsNull(el) && !JS_IsUndefined(el)) {
            if (toLocaleString) {
                el = JS_ToLocaleStringFree(ctx, el);
            }
            if (string_buffer_concat_value_free(b, el))
                goto fail;
        }
    }
    JS_FreeValue(ctx, sep);
    JS_FreeValue(ctx, obj);
    return string_buffer_end(b);

fail:
    string_buffer_free(b);
    JS_FreeValue(ctx, sep);
exception:
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

JSValue js_array_pop(JSContext *ctx, JSValueConst this_val,
                            int argc, JSValueConst *argv, int shift)
{
    JSValue obj, res = JS_UNDEFINED;
    int64_t len, newLen;
    JSValue *arrp;
    uint32_t count32;

    obj = JS_ToObject(ctx, this_val);
    if (js_get_length64(ctx, &len, obj))
        goto exception;
    newLen = 0;
    if (len > 0) {
        newLen = len - 1;
        /* Special case fast arrays */
        if (js_get_fast_array(ctx, obj, &arrp, &count32) && count32 == len) {
            JSObject *p = JS_VALUE_GET_OBJ(obj);
            if (shift) {
                res = arrp[0];
                memmove(arrp, arrp + 1, (count32 - 1) * sizeof(*arrp));
                p->u.array.count--;
            } else {
                res = arrp[count32 - 1];
                p->u.array.count--;
            }
        } else {
            if (shift) {
                res = JS_GetPropertyInt64(ctx, obj, 0);
                if (JS_IsException(res))
                    goto exception;
                if (JS_CopySubArray(ctx, obj, 0, 1, len - 1, +1))
                    goto exception;
            } else {
                res = JS_GetPropertyInt64(ctx, obj, newLen);
                if (JS_IsException(res))
                    goto exception;
            }
            if (JS_DeletePropertyInt64(ctx, obj, newLen, JS_PROP_THROW) < 0)
                goto exception;
        }
    }
    if (JS_SetProperty(ctx, obj, JS_ATOM_length, JS_NewInt64(ctx, newLen)) < 0)
        goto exception;

    JS_FreeValue(ctx, obj);
    return res;

 exception:
    JS_FreeValue(ctx, res);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

JSValue js_array_push(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv, int unshift)
{
    JSValue obj;
    int i;
    int64_t len, from, newLen;

    if (likely(JS_VALUE_GET_TAG(this_val) == JS_TAG_OBJECT && !unshift)) {
        JSObject *p = JS_VALUE_GET_OBJ(this_val);
        if (likely(p->class_id == JS_CLASS_ARRAY && p->fast_array &&
                   can_extend_fast_array(p) &&
                   JS_VALUE_GET_TAG(p->prop[0].u.value) == JS_TAG_INT &&
                   JS_VALUE_GET_INT(p->prop[0].u.value) == p->u.array.count &&
                   (get_shape_prop(p->shape)->flags & JS_PROP_WRITABLE) != 0)) {
            /* fast case */
            uint32_t new_len;
            new_len = p->u.array.count + argc;
            if (likely(new_len <= INT32_MAX)) {
                if (unlikely(new_len > p->u.array.u1.size)) {
                    if (expand_fast_array(ctx, p, new_len))
                        return JS_EXCEPTION;
                }
                for(i = 0; i < argc; i++)
                    p->u.array.u.values[p->u.array.count + i] = JS_DupValue(ctx, argv[i]);
                p->prop[0].u.value = JS_NewInt32(ctx, new_len);
                p->u.array.count = new_len;
                return JS_NewInt32(ctx, new_len);
            }
        }
    }
    obj = JS_ToObject(ctx, this_val);
    if (js_get_length64(ctx, &len, obj))
        goto exception;
    newLen = len + argc;
    if (newLen > MAX_SAFE_INTEGER) {
        JS_ThrowTypeError(ctx, "Array loo long");
        goto exception;
    }
    from = len;
    if (unshift && argc > 0) {
        if (JS_CopySubArray(ctx, obj, argc, 0, len, -1))
            goto exception;
        from = 0;
    }
    for(i = 0; i < argc; i++) {
        if (JS_SetPropertyInt64(ctx, obj, from + i,
                                JS_DupValue(ctx, argv[i])) < 0)
            goto exception;
    }
    if (JS_SetProperty(ctx, obj, JS_ATOM_length, JS_NewInt64(ctx, newLen)) < 0)
        goto exception;

    JS_FreeValue(ctx, obj);
    return JS_NewInt64(ctx, newLen);

 exception:
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

JSValue js_array_reverse(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv)
{
    JSValue obj, lval, hval;
    JSValue *arrp;
    int64_t len, l, h;
    int l_present, h_present;
    uint32_t count32;

    lval = JS_UNDEFINED;
    obj = JS_ToObject(ctx, this_val);
    if (js_get_length64(ctx, &len, obj))
        goto exception;

    /* Special case fast arrays */
    if (js_get_fast_array(ctx, obj, &arrp, &count32) && count32 == len) {
        uint32_t ll, hh;

        if (count32 > 1) {
            for (ll = 0, hh = count32 - 1; ll < hh; ll++, hh--) {
                lval = arrp[ll];
                arrp[ll] = arrp[hh];
                arrp[hh] = lval;
            }
        }
        return obj;
    }

    for (l = 0, h = len - 1; l < h; l++, h--) {
        l_present = JS_TryGetPropertyInt64(ctx, obj, l, &lval);
        if (l_present < 0)
            goto exception;
        h_present = JS_TryGetPropertyInt64(ctx, obj, h, &hval);
        if (h_present < 0)
            goto exception;
        if (h_present) {
            if (JS_SetPropertyInt64(ctx, obj, l, hval) < 0)
                goto exception;

            if (l_present) {
                if (JS_SetPropertyInt64(ctx, obj, h, lval) < 0) {
                    lval = JS_UNDEFINED;
                    goto exception;
                }
                lval = JS_UNDEFINED;
            } else {
                if (JS_DeletePropertyInt64(ctx, obj, h, JS_PROP_THROW) < 0)
                    goto exception;
            }
        } else {
            if (l_present) {
                if (JS_DeletePropertyInt64(ctx, obj, l, JS_PROP_THROW) < 0)
                    goto exception;
                if (JS_SetPropertyInt64(ctx, obj, h, lval) < 0) {
                    lval = JS_UNDEFINED;
                    goto exception;
                }
                lval = JS_UNDEFINED;
            }
        }
    }
    return obj;

 exception:
    JS_FreeValue(ctx, lval);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

// Note: a.toReversed() is a.slice().reverse() with the twist that a.slice()
// leaves holes in sparse arrays intact whereas a.toReversed() replaces them
// with undefined, thus in effect creating a dense array.
// Does not use Array[@@species], always returns a base Array.
JSValue js_array_toReversed(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv)
{
    JSValue arr, obj, ret, *arrp, *pval;
    JSObject *p;
    int64_t i, len;
    uint32_t count32;

    ret = JS_EXCEPTION;
    arr = JS_UNDEFINED;
    obj = JS_ToObject(ctx, this_val);
    if (js_get_length64(ctx, &len, obj))
        goto exception;

    arr = js_allocate_fast_array(ctx, len);
    if (JS_IsException(arr))
        goto exception;

    if (len > 0) {
        p = JS_VALUE_GET_OBJ(arr);

        i = len - 1;
        pval = p->u.array.u.values;
        if (js_get_fast_array(ctx, obj, &arrp, &count32) && count32 == len) {
            for (; i >= 0; i--, pval++)
                *pval = JS_DupValue(ctx, arrp[i]);
        } else {
            // Query order is observable; test262 expects descending order.
            for (; i >= 0; i--, pval++) {
                if (-1 == JS_TryGetPropertyInt64(ctx, obj, i, pval))
                    goto exception;
            }
        }
    }

    ret = arr;
    arr = JS_UNDEFINED;

exception:
    JS_FreeValue(ctx, arr);
    JS_FreeValue(ctx, obj);
    return ret;
}

JSValue js_array_slice(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv)
{
    JSValue obj, arr, val, ctor;
    int64_t len, start, k, final, n, count;
    int kPresent;
    JSValue *arrp;
    uint32_t count32;

    arr = JS_UNDEFINED;
    obj = JS_ToObject(ctx, this_val);
    if (js_get_length64(ctx, &len, obj))
        goto exception;

    if (JS_ToInt64Clamp(ctx, &start, argv[0], 0, len, len))
        goto exception;

    final = len;
    if (!JS_IsUndefined(argv[1])) {
        if (JS_ToInt64Clamp(ctx, &final, argv[1], 0, len, len))
            goto exception;
    }
    count = max_int64(final - start, 0);

    ctor = JS_ArraySpeciesGetCtor(ctx, obj);
    if (JS_IsException(ctor))
        goto exception;

    final = start + count;
    if (JS_IsUndefined(ctor) &&
        js_get_fast_array(ctx, obj, &arrp, &count32) &&
        final <= count32) {
        /* fast case */
        arr = js_create_array(ctx, count, (JSValueConst *)arrp + start);
    } else {
        arr = JS_ArrayCreateFromCtor(ctx, ctor, count);
        JS_FreeValue(ctx, ctor);
        if (JS_IsException(arr))
            goto exception;

        n = 0;
        for (k = start; k < final; k++, n++) {
            kPresent = JS_TryGetPropertyInt64(ctx, obj, k, &val);
            if (kPresent < 0)
                goto exception;
            if (kPresent) {
                if (JS_CreateDataPropertyUint32(ctx, arr, n, val, JS_PROP_THROW) < 0)
                    goto exception;
            }
        }
        if (JS_SetProperty(ctx, arr, JS_ATOM_length, JS_NewInt64(ctx, n)) < 0)
            goto exception;
    }
    JS_FreeValue(ctx, obj);
    return arr;

 exception:
    JS_FreeValue(ctx, obj);
    JS_FreeValue(ctx, arr);
    return JS_EXCEPTION;
}

JSValue js_array_splice(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv)
{
    JSValue obj, arr, val, ctor;
    int64_t len, start, k, final, n, del_count, new_len;
    int kPresent;
    uint32_t i, item_count;
    JSObject *p;

    arr = JS_UNDEFINED;
    obj = JS_ToObject(ctx, this_val);
    if (js_get_length64(ctx, &len, obj))
        goto exception;

    if (JS_ToInt64Clamp(ctx, &start, argv[0], 0, len, len))
        goto exception;

    if (argc == 0) {
        item_count = 0;
        del_count = 0;
    } else if (argc == 1) {
        item_count = 0;
        del_count = len - start;
    } else {
        item_count = argc - 2;
        if (JS_ToInt64Clamp(ctx, &del_count, argv[1], 0, len - start, 0))
            goto exception;
    }
    if (len + item_count - del_count > MAX_SAFE_INTEGER) {
        JS_ThrowTypeError(ctx, "Array loo long");
        goto exception;
    }
    final = start + del_count;
    /* warning: 'len' may be different from the actual array length
       because it may have been modified */
    new_len = len + item_count - del_count;

    ctor = JS_ArraySpeciesGetCtor(ctx, obj);
    if (JS_IsException(ctor))
        goto exception;

    p = JS_VALUE_GET_PTR(obj);
    if (JS_IsUndefined(ctor) &&
        p->class_id == JS_CLASS_ARRAY &&
        p->fast_array &&
        final <= p->u.array.count &&
        (get_shape_prop(p->shape)->flags & JS_PROP_WRITABLE) && /* writable array length */
        can_extend_fast_array(p)) {
        uint32_t count32 = p->u.array.count;
        JSValue *arrp = p->u.array.u.values;

        /* fast case */
        arr = js_create_array(ctx, del_count, (JSValueConst *)arrp + start);
        if (JS_IsException(arr))
            goto exception;

        if (item_count != del_count) {
            /* resize */
            uint32_t new_count32;
            new_count32 = count32 + item_count - del_count;
            if (del_count > item_count) {
                for(i = 0; i < del_count - item_count; i++)
                    JS_FreeValue(ctx, arrp[start + item_count + i]);
                memmove(arrp + start + item_count, arrp + final,
                        (count32 - final) * sizeof(arrp[0]));
            } else {
                if (unlikely(new_count32 > p->u.array.u1.size)) {
                    if (expand_fast_array(ctx, p, new_count32))
                        goto exception;
                    arrp = p->u.array.u.values;
                }
                memmove(arrp + start + item_count, arrp + final,
                        (count32 - final) * sizeof(arrp[0]));
                for(i = 0; i < item_count - del_count; i++)
                    arrp[start + del_count + i] = JS_UNDEFINED;
            }
            p->u.array.count = new_count32;
        }
        for(i = 0; i < item_count; i++)
            set_value(ctx, &arrp[start + i], JS_DupValue(ctx, argv[i + 2]));
    } else {
        arr = JS_ArrayCreateFromCtor(ctx, ctor, del_count);
        JS_FreeValue(ctx, ctor);
        if (JS_IsException(arr))
            goto exception;

        n = 0;
        for (k = start; k < final; k++, n++) {
            kPresent = JS_TryGetPropertyInt64(ctx, obj, k, &val);
            if (kPresent < 0)
                goto exception;
            if (kPresent) {
                if (JS_CreateDataPropertyUint32(ctx, arr, n, val, JS_PROP_THROW) < 0)
                    goto exception;
            }
        }
        if (JS_SetProperty(ctx, arr, JS_ATOM_length, JS_NewInt64(ctx, n)) < 0)
            goto exception;

        if (item_count != del_count) {
            if (JS_CopySubArray(ctx, obj, start + item_count,
                                start + del_count, len - (start + del_count),
                                item_count <= del_count ? +1 : -1) < 0)
                goto exception;

            for (k = len; k-- > new_len; ) {
                if (JS_DeletePropertyInt64(ctx, obj, k, JS_PROP_THROW) < 0)
                    goto exception;
            }
        }
        for (i = 0; i < item_count; i++) {
            if (JS_SetPropertyInt64(ctx, obj, start + i, JS_DupValue(ctx, argv[i + 2])) < 0)
                goto exception;
        }
    }
    if (JS_SetProperty(ctx, obj, JS_ATOM_length, JS_NewInt64(ctx, new_len)) < 0)
        goto exception;
    JS_FreeValue(ctx, obj);
    return arr;

 exception:
    JS_FreeValue(ctx, obj);
    JS_FreeValue(ctx, arr);
    return JS_EXCEPTION;
}

JSValue js_array_toSpliced(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv)
{
    JSValue arr, obj, ret, *arrp, *pval, *last;
    JSObject *p;
    int64_t i, j, len, newlen, start, add, del;
    uint32_t count32;

    pval = NULL;
    last = NULL;
    ret = JS_EXCEPTION;
    arr = JS_UNDEFINED;

    obj = JS_ToObject(ctx, this_val);
    if (js_get_length64(ctx, &len, obj))
        goto exception;

    start = 0;
    if (argc > 0)
        if (JS_ToInt64Clamp(ctx, &start, argv[0], 0, len, len))
            goto exception;

    del = 0;
    if (argc > 0)
        del = len - start;
    if (argc > 1)
        if (JS_ToInt64Clamp(ctx, &del, argv[1], 0, del, 0))
            goto exception;

    add = 0;
    if (argc > 2)
        add = argc - 2;

    newlen = len + add - del;
    if (newlen > MAX_SAFE_INTEGER) {
        JS_ThrowTypeError(ctx, "invalid array length");
        goto exception;
    }

    arr = js_allocate_fast_array(ctx, newlen);
    if (JS_IsException(arr))
        goto exception;

    if (newlen <= 0)
        goto done;

    p = JS_VALUE_GET_OBJ(arr);
    pval = &p->u.array.u.values[0];
    last = &p->u.array.u.values[newlen];

    if (js_get_fast_array(ctx, obj, &arrp, &count32) && count32 == len) {
        for (i = 0; i < start; i++, pval++)
            *pval = JS_DupValue(ctx, arrp[i]);
        for (j = 0; j < add; j++, pval++)
            *pval = JS_DupValue(ctx, argv[2 + j]);
        for (i += del; i < len; i++, pval++)
            *pval = JS_DupValue(ctx, arrp[i]);
    } else {
        for (i = 0; i < start; i++, pval++)
            if (-1 == JS_TryGetPropertyInt64(ctx, obj, i, pval))
                goto exception;
        for (j = 0; j < add; j++, pval++)
            *pval = JS_DupValue(ctx, argv[2 + j]);
        for (i += del; i < len; i++, pval++)
            if (-1 == JS_TryGetPropertyInt64(ctx, obj, i, pval))
                goto exception;
    }

    assert(pval == last);

done:
    ret = arr;
    arr = JS_UNDEFINED;

exception:
    JS_FreeValue(ctx, arr);
    JS_FreeValue(ctx, obj);
    return ret;
}

JSValue js_array_copyWithin(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv)
{
    JSValue obj;
    int64_t len, from, to, final, count;

    obj = JS_ToObject(ctx, this_val);
    if (js_get_length64(ctx, &len, obj))
        goto exception;

    if (JS_ToInt64Clamp(ctx, &to, argv[0], 0, len, len))
        goto exception;

    if (JS_ToInt64Clamp(ctx, &from, argv[1], 0, len, len))
        goto exception;

    final = len;
    if (argc > 2 && !JS_IsUndefined(argv[2])) {
        if (JS_ToInt64Clamp(ctx, &final, argv[2], 0, len, len))
            goto exception;
    }

    count = min_int64(final - from, len - to);

    if (JS_CopySubArray(ctx, obj, to, from, count,
                        (from < to && to < from + count) ? -1 : +1))
        goto exception;

    return obj;

 exception:
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

int64_t JS_FlattenIntoArray(JSContext *ctx, JSValueConst target,
                                   JSValueConst source, int64_t sourceLen,
                                   int64_t targetIndex, int depth,
                                   JSValueConst mapperFunction,
                                   JSValueConst thisArg)
{
    JSValue element;
    int64_t sourceIndex, elementLen;
    int present, is_array;

    if (js_check_stack_overflow(ctx->rt, 0)) {
        JS_ThrowStackOverflow(ctx);
        return -1;
    }

    for (sourceIndex = 0; sourceIndex < sourceLen; sourceIndex++) {
        present = JS_TryGetPropertyInt64(ctx, source, sourceIndex, &element);
        if (present < 0)
            return -1;
        if (!present)
            continue;
        if (!JS_IsUndefined(mapperFunction)) {
            JSValueConst args[3] = { element, JS_NewInt64(ctx, sourceIndex), source };
            element = JS_Call(ctx, mapperFunction, thisArg, 3, args);
            JS_FreeValue(ctx, (JSValue)args[0]);
            JS_FreeValue(ctx, (JSValue)args[1]);
            if (JS_IsException(element))
                return -1;
        }
        if (depth > 0) {
            is_array = JS_IsArray(ctx, element);
            if (is_array < 0)
                goto fail;
            if (is_array) {
                if (js_get_length64(ctx, &elementLen, element) < 0)
                    goto fail;
                targetIndex = JS_FlattenIntoArray(ctx, target, element,
                                                  elementLen, targetIndex,
                                                  depth - 1,
                                                  JS_UNDEFINED, JS_UNDEFINED);
                if (targetIndex < 0)
                    goto fail;
                JS_FreeValue(ctx, element);
                continue;
            }
        }
        if (targetIndex >= MAX_SAFE_INTEGER) {
            JS_ThrowTypeError(ctx, "Array too long");
            goto fail;
        }
        if (JS_DefinePropertyValueInt64(ctx, target, targetIndex, element,
                                        JS_PROP_C_W_E | JS_PROP_THROW) < 0)
            return -1;
        targetIndex++;
    }
    return targetIndex;

fail:
    JS_FreeValue(ctx, element);
    return -1;
}

JSValue js_array_flatten(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv, int map)
{
    JSValue obj, arr;
    JSValueConst mapperFunction, thisArg;
    int64_t sourceLen;
    int depthNum;

    arr = JS_UNDEFINED;
    obj = JS_ToObject(ctx, this_val);
    if (js_get_length64(ctx, &sourceLen, obj))
        goto exception;

    depthNum = 1;
    mapperFunction = JS_UNDEFINED;
    thisArg = JS_UNDEFINED;
    if (map) {
        mapperFunction = argv[0];
        if (argc > 1) {
            thisArg = argv[1];
        }
        if (check_function(ctx, mapperFunction))
            goto exception;
    } else {
        if (argc > 0 && !JS_IsUndefined(argv[0])) {
            if (JS_ToInt32Sat(ctx, &depthNum, argv[0]) < 0)
                goto exception;
        }
    }
    arr = JS_ArraySpeciesCreate(ctx, obj, 0);
    if (JS_IsException(arr))
        goto exception;
    if (JS_FlattenIntoArray(ctx, arr, obj, sourceLen, 0, depthNum,
                            mapperFunction, thisArg) < 0)
        goto exception;
    JS_FreeValue(ctx, obj);
    return arr;

exception:
    JS_FreeValue(ctx, obj);
    JS_FreeValue(ctx, arr);
    return JS_EXCEPTION;
}

/* Array sort */

int js_array_cmp_generic(const void *a, const void *b, void *opaque) {
    struct array_sort_context *psc = opaque;
    JSContext *ctx = psc->ctx;
    JSValueConst argv[2];
    JSValue res;
    ValueSlot *ap = (ValueSlot *)(void *)a;
    ValueSlot *bp = (ValueSlot *)(void *)b;
    int cmp;

    if (psc->exception)
        return 0;

    if (psc->has_method) {
        /* custom sort function is specified as returning 0 for identical
         * objects: avoid method call overhead.
         */
        if (!memcmp(&ap->val, &bp->val, sizeof(ap->val)))
            goto cmp_same;
        argv[0] = ap->val;
        argv[1] = bp->val;
        res = JS_Call(ctx, psc->method, JS_UNDEFINED, 2, argv);
        if (JS_IsException(res))
            goto exception;
        if (JS_VALUE_GET_TAG(res) == JS_TAG_INT) {
            int val = JS_VALUE_GET_INT(res);
            cmp = (val > 0) - (val < 0);
        } else {
            double val;
            if (JS_ToFloat64Free(ctx, &val, res) < 0)
                goto exception;
            cmp = (val > 0) - (val < 0);
        }
    } else {
        /* Not supposed to bypass ToString even for identical objects as
         * tested in test262/test/built-ins/Array/prototype/sort/bug_596_1.js
         */
        if (!ap->str) {
            JSValue str = JS_ToString(ctx, ap->val);
            if (JS_IsException(str))
                goto exception;
            ap->str = JS_VALUE_GET_STRING(str);
        }
        if (!bp->str) {
            JSValue str = JS_ToString(ctx, bp->val);
            if (JS_IsException(str))
                goto exception;
            bp->str = JS_VALUE_GET_STRING(str);
        }
        cmp = js_string_compare(ctx, ap->str, bp->str);
    }
    if (cmp != 0)
        return cmp;
cmp_same:
    /* make sort stable: compare array offsets */
    return (ap->pos > bp->pos) - (ap->pos < bp->pos);

exception:
    psc->exception = 1;
    return 0;
}
