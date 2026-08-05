#include "js_internal.h"



/* Iterator Wrap */

void js_iterator_wrap_finalizer(JSRuntime *rt, JSValue val)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JSIteratorWrapData *it = p->u.iterator_wrap_data;
    if (it) {
        JS_FreeValueRT(rt, it->wrapped_iter);
        JS_FreeValueRT(rt, it->wrapped_next);
        js_free_rt(rt, it);
    }
}

void js_iterator_wrap_mark(JSRuntime *rt, JSValueConst val,
                                  JS_MarkFunc *mark_func)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JSIteratorWrapData *it = p->u.iterator_wrap_data;
    if (it) {
        JS_MarkValue(rt, it->wrapped_iter, mark_func);
        JS_MarkValue(rt, it->wrapped_next, mark_func);
    }
}

JSValue js_iterator_wrap_next(JSContext *ctx, JSValueConst this_val,
                                     int argc, JSValueConst *argv,
                                     int *pdone, int magic)
{
    JSIteratorWrapData *it;
    JSValue method, ret;
    it = JS_GetOpaque2(ctx, this_val, JS_CLASS_ITERATOR_WRAP);
    if (!it)
        return JS_EXCEPTION;
    if (magic == GEN_MAGIC_NEXT) {
        return JS_IteratorNext(ctx, it->wrapped_iter, it->wrapped_next, 0, NULL, pdone);
    } else {
        method = JS_GetProperty(ctx, it->wrapped_iter, JS_ATOM_return);
        if (JS_IsException(method))
            return JS_EXCEPTION;
        if (JS_IsNull(method) || JS_IsUndefined(method)) {
            *pdone = TRUE;
            return JS_UNDEFINED;
        }
        ret = JS_IteratorNext2(ctx, it->wrapped_iter, method, 0, NULL, pdone);
        JS_FreeValue(ctx, method);
        return ret;
    }
}

/* Iterator */

JSValue js_iterator_constructor_getset(JSContext *ctx,
                                              JSValueConst this_val,
                                              int argc, JSValueConst *argv,
                                              int magic,
                                              JSValue *func_data)
{
    int ret;

    if (argc > 0) { // if setter
        if (!JS_IsObject(argv[0]))
            return JS_ThrowTypeErrorNotAnObject(ctx);
        ret = JS_DefinePropertyValue(ctx, this_val, JS_ATOM_constructor,
                                     JS_DupValue(ctx, argv[0]),
                                     JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
        if (ret < 0)
            return JS_EXCEPTION;
        return JS_UNDEFINED;
    } else {
        return JS_DupValue(ctx, func_data[0]);
    }
}

JSValue js_iterator_constructor(JSContext *ctx, JSValueConst new_target,
                                       int argc, JSValueConst *argv)
{
    JSObject *p;

    if (JS_TAG_OBJECT != JS_VALUE_GET_TAG(new_target))
        return JS_ThrowTypeError(ctx, "constructor requires 'new'");
    p = JS_VALUE_GET_OBJ(new_target);
    if (p->class_id == JS_CLASS_C_FUNCTION &&
        p->u.cfunc.c_function.generic == js_iterator_constructor) {
        return JS_ThrowTypeError(ctx, "abstract class not constructable");
    }
    return js_create_from_ctor(ctx, new_target, JS_CLASS_ITERATOR);
}

void js_iterator_concat_finalizer(JSRuntime *rt, JSValue val)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JSIteratorConcatData *it = p->u.iterator_concat_data;
    if (it) {
        JS_FreeValueRT(rt, it->iter);
        JS_FreeValueRT(rt, it->next);
        for (int i = it->index; i < it->count; i++)
            JS_FreeValueRT(rt, it->values[i]);
        js_free_rt(rt, it);
    }
}

void js_iterator_concat_mark(JSRuntime *rt, JSValueConst val,
                                    JS_MarkFunc *mark_func)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JSIteratorConcatData *it = p->u.iterator_concat_data;
    if (it) {
        JS_MarkValue(rt, it->iter, mark_func);
        JS_MarkValue(rt, it->next, mark_func);
        for (int i = it->index; i < it->count; i++)
            JS_MarkValue(rt, it->values[i], mark_func);
    }
}

JSValue js_iterator_concat_next(JSContext *ctx, JSValueConst this_val,
                                       int argc, JSValueConst *argv,
                                       int *pdone, int magic)
{
    JSValue iter, item, next, val, *obj, *meth, ret;
    JSIteratorConcatData *it;
    int done;

    *pdone = FALSE;

    it = JS_GetOpaque2(ctx, this_val, JS_CLASS_ITERATOR_CONCAT);
    if (!it)
        return JS_EXCEPTION;
    if (it->running)
        return JS_ThrowTypeError(ctx, "already running");

    it->running = TRUE;
    for(;;) {
        if (it->index >= it->count) {
            *pdone = TRUE;
            ret = JS_UNDEFINED;
            break;
        }
        obj = &it->values[it->index + 0];
        meth = &it->values[it->index + 1];
        iter = it->iter;
        if (JS_IsUndefined(iter)) {
            iter = JS_GetIterator2(ctx, *obj, *meth);
            if (JS_IsException(iter))
                goto fail;
            it->iter = iter;
        }
        next = it->next;
        if (JS_IsUndefined(next)) {
            next = JS_GetProperty(ctx, iter, JS_ATOM_next);
            if (JS_IsException(next))
                goto fail;
            it->next = next;
        }
        item = JS_IteratorNext2(ctx, iter, next, 0, NULL, &done);
        if (JS_IsException(item))
            goto fail;
        if (done == 0) {
            ret = item;
            break;
        } else if (done == 2) {
            val = JS_GetProperty(ctx, item, JS_ATOM_done);
            if (JS_IsException(val)) {
                JS_FreeValue(ctx, item);
            fail:
                ret = JS_EXCEPTION;
                break;
            }
            done = JS_ToBoolFree(ctx, val);
            if (done)
                goto done_next;
            ret = JS_GetProperty(ctx, item, JS_ATOM_value);
            JS_FreeValue(ctx, item);
            break;
        } else {
        done_next:
            JS_FreeValue(ctx, item);
            JS_FreeValue(ctx, iter);
            JS_FreeValue(ctx, next);
            it->iter = JS_UNDEFINED;
            it->next = JS_UNDEFINED;
            JS_FreeValue(ctx, *meth);
            JS_FreeValue(ctx, *obj);
            it->index += 2;
        }
    }
    it->running = FALSE;
    return ret;
}

JSValue js_iterator_concat_return(JSContext *ctx, JSValueConst this_val,
                                         int argc, JSValueConst *argv)
{
    JSIteratorConcatData *it;
    JSValue ret;

    it = JS_GetOpaque2(ctx, this_val, JS_CLASS_ITERATOR_CONCAT);
    if (!it)
        return JS_EXCEPTION;
    if (it->running)
        return JS_ThrowTypeError(ctx, "already running");
    ret = JS_UNDEFINED;
    if (!JS_IsUndefined(it->iter)) {
        it->running = TRUE;
        ret = JS_GetProperty(ctx, it->iter, JS_ATOM_return);
        if (JS_IsException(ret)) {
            it->running = FALSE;
            return JS_EXCEPTION;
        }
        ret = JS_CallFree(ctx, ret, it->iter, 0, NULL);
        it->running = FALSE;
    }
    while (it->index < it->count)
        JS_FreeValue(ctx, it->values[it->index++]);
    JS_FreeValue(ctx, it->iter);
    JS_FreeValue(ctx, it->next);
    it->iter = JS_UNDEFINED;
    it->next = JS_UNDEFINED;
    return ret;
}

JSValue js_iterator_concat(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv)
{
    JSIteratorConcatData *it;
    JSValue obj, method;

    it = js_malloc(ctx, sizeof(*it) + 2*argc * sizeof(it->values[0]));
    if (!it)
        return JS_EXCEPTION;
    it->running = FALSE;
    it->index = 0;
    it->count = 0;
    it->iter = JS_UNDEFINED;
    it->next = JS_UNDEFINED;
    for (int i = 0; i < argc; i++) {
        JSValueConst obj = argv[i];
        if (!JS_IsObject(obj)) {
            JS_ThrowTypeErrorNotAnObject(ctx);
            goto fail;
        }
        method = JS_GetProperty(ctx, obj, JS_ATOM_Symbol_iterator);
        if (JS_IsException(method))
            goto fail;
        if (! JS_IsFunction(ctx, method)) {
            JS_ThrowTypeError(ctx, "not a function");
            JS_FreeValue(ctx, method);
            goto fail;
        }
        it->values[it->count++] = JS_DupValue(ctx, obj);
        it->values[it->count++] = method;
    }
    obj = JS_NewObjectClass(ctx, JS_CLASS_ITERATOR_CONCAT);
    if (JS_IsException(obj))
        goto fail;
    JS_SetOpaque(obj, it);
    return obj;
fail:
    for (int i = 0; i < it->count; i++)
        JS_FreeValue(ctx, it->values[i]);
    js_free(ctx, it);
    return JS_EXCEPTION;
}

JSValue js_iterator_from(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv)
{
    JSValueConst obj = argv[0];
    JSValue method, iter, wrapper;
    JSIteratorWrapData *it;
    int ret;

    if (!JS_IsObject(obj)) {
        if (!JS_IsString(obj))
            return JS_ThrowTypeError(ctx, "Iterator.from called on non-object");
    }
    method = JS_GetProperty(ctx, obj, JS_ATOM_Symbol_iterator);
    if (JS_IsException(method))
        return JS_EXCEPTION;
    if (JS_IsNull(method) || JS_IsUndefined(method)) {
        iter = JS_DupValue(ctx, obj);
    } else {
        iter = JS_GetIterator2(ctx, obj, method);
        JS_FreeValue(ctx, method);
        if (JS_IsException(iter))
            return JS_EXCEPTION;
    }

    wrapper = JS_UNDEFINED;
    method = JS_GetProperty(ctx, iter, JS_ATOM_next);
    if (JS_IsException(method))
        goto fail;

    ret = JS_OrdinaryIsInstanceOf(ctx, iter, ctx->iterator_ctor);
    if (ret < 0)
        goto fail;
    if (ret) {
        JS_FreeValue(ctx, method);
        return iter;
    }

    wrapper = JS_NewObjectClass(ctx, JS_CLASS_ITERATOR_WRAP);
    if (JS_IsException(wrapper))
        goto fail;
    it = js_malloc(ctx, sizeof(*it));
    if (!it)
        goto fail;
    it->wrapped_iter = iter;
    it->wrapped_next = method;
    JS_SetOpaque(wrapper, it);
    return wrapper;

 fail:
    JS_FreeValue(ctx, method);
    JS_FreeValue(ctx, iter);
    JS_FreeValue(ctx, wrapper);
    return JS_EXCEPTION;
}

JSValue js_create_iterator_helper(JSContext *ctx, JSValueConst this_val,
                                         int argc, JSValueConst *argv, int magic)
{
    JSValueConst func;
    JSValue obj, method;
    int64_t count;
    JSIteratorHelperData *it;

    if (!JS_IsObject(this_val))
        return JS_ThrowTypeErrorNotAnObject(ctx);
    func = JS_UNDEFINED;
    count = 0;

    switch(magic) {
    case JS_ITERATOR_HELPER_KIND_DROP:
    case JS_ITERATOR_HELPER_KIND_TAKE:
        {
            JSValue v;
            double dlimit;
            v = JS_ToNumber(ctx, argv[0]);
            if (JS_IsException(v))
                goto fail;
            // Check for Infinity.
            if (JS_ToFloat64(ctx, &dlimit, v)) {
                JS_FreeValue(ctx, v);
                goto fail;
            }
            if (isnan(dlimit)) {
                JS_FreeValue(ctx, v);
                goto range_error;
            }
            if (!isfinite(dlimit)) {
                JS_FreeValue(ctx, v);
                if (dlimit < 0)
                    goto range_error;
                else
                    count = MAX_SAFE_INTEGER;
            } else {
                v = JS_ToIntegerFree(ctx, v);
                if (JS_IsException(v))
                    goto fail;
                if (JS_ToInt64Free(ctx, &count, v))
                    goto fail;
            }
            if (count < 0)
                goto range_error;
        }
        break;
    case JS_ITERATOR_HELPER_KIND_FILTER:
    case JS_ITERATOR_HELPER_KIND_FLAT_MAP:
    case JS_ITERATOR_HELPER_KIND_MAP:
        {
            func = argv[0];
            if (check_function(ctx, func))
                goto fail;
        }
        break;
    default:
        abort();
        break;
    }

    method = JS_GetProperty(ctx, this_val, JS_ATOM_next);
    if (JS_IsException(method))
        goto fail;
    obj = JS_NewObjectClass(ctx, JS_CLASS_ITERATOR_HELPER);
    if (JS_IsException(obj)) {
        JS_FreeValue(ctx, method);
        goto fail;
    }
    it = js_malloc(ctx, sizeof(*it));
    if (!it) {
        JS_FreeValue(ctx, obj);
        JS_FreeValue(ctx, method);
        goto fail;
    }
    it->kind = magic;
    it->obj = JS_DupValue(ctx, this_val);
    it->func = JS_DupValue(ctx, func);
    it->next = method;
    it->inner = JS_UNDEFINED;
    it->count = count;
    it->executing = 0;
    it->done = 0;
    JS_SetOpaque(obj, it);
    return obj;
range_error:
    JS_ThrowRangeError(ctx, "must be positive");
fail:
    JS_IteratorClose(ctx, this_val, TRUE);
    return JS_EXCEPTION;
}

JSValue js_iterator_proto_func(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv, int magic)
{
    JSValue item, method, ret, func, index_val, r;
    JSValueConst args[2];
    int64_t idx;
    int done;

    if (!JS_IsObject(this_val))
        return JS_ThrowTypeErrorNotAnObject(ctx);
    func = JS_UNDEFINED;
    method = JS_UNDEFINED;

    if (check_function(ctx, argv[0]))
        goto fail;
    func = JS_DupValue(ctx, argv[0]);
    method = JS_GetProperty(ctx, this_val, JS_ATOM_next);
    if (JS_IsException(method))
        goto fail_no_close;

    r = JS_UNDEFINED;

    switch(magic) {
    case JS_ITERATOR_HELPER_KIND_EVERY:
        {
            r = JS_TRUE;
            for (idx = 0; /*empty*/; idx++) {
                item = JS_IteratorNext(ctx, this_val, method, 0, NULL, &done);
                if (JS_IsException(item))
                    goto fail_no_close;
                if (done)
                    break;
                index_val = JS_NewInt64(ctx, idx);
                args[0] = item;
                args[1] = index_val;
                ret = JS_Call(ctx, func, JS_UNDEFINED, countof(args), args);
                JS_FreeValue(ctx, item);
                JS_FreeValue(ctx, index_val);
                if (JS_IsException(ret))
                    goto fail;
                if (!JS_ToBoolFree(ctx, ret)) {
                    if (JS_IteratorClose(ctx, this_val, FALSE) < 0)
                        r = JS_EXCEPTION;
                    else
                        r = JS_FALSE;
                    break;
                }
                index_val = JS_UNDEFINED;
                ret = JS_UNDEFINED;
                item = JS_UNDEFINED;
            }
        }
        break;
    case JS_ITERATOR_HELPER_KIND_FIND:
        {
            for (idx = 0; /*empty*/; idx++) {
                item = JS_IteratorNext(ctx, this_val, method, 0, NULL, &done);
                if (JS_IsException(item))
                    goto fail_no_close;
                if (done)
                    break;
                index_val = JS_NewInt64(ctx, idx);
                args[0] = item;
                args[1] = index_val;
                ret = JS_Call(ctx, func, JS_UNDEFINED, countof(args), args);
                JS_FreeValue(ctx, index_val);
                if (JS_IsException(ret)) {
                    JS_FreeValue(ctx, item);
                    goto fail;
                }
                if (JS_ToBoolFree(ctx, ret)) {
                    if (JS_IteratorClose(ctx, this_val, FALSE) < 0) {
                        JS_FreeValue(ctx, item);
                        r = JS_EXCEPTION;
                    } else {
                        r = item;
                    }
                    break;
                }
                JS_FreeValue(ctx, item);
                index_val = JS_UNDEFINED;
                ret = JS_UNDEFINED;
                item = JS_UNDEFINED;
            }
        }
        break;
    case JS_ITERATOR_HELPER_KIND_FOR_EACH:
        {
            for (idx = 0; /*empty*/; idx++) {
                item = JS_IteratorNext(ctx, this_val, method, 0, NULL, &done);
                if (JS_IsException(item))
                    goto fail_no_close;
                if (done)
                    break;
                index_val = JS_NewInt64(ctx, idx);
                args[0] = item;
                args[1] = index_val;
                ret = JS_Call(ctx, func, JS_UNDEFINED, countof(args), args);
                JS_FreeValue(ctx, item);
                JS_FreeValue(ctx, index_val);
                if (JS_IsException(ret))
                    goto fail;
                JS_FreeValue(ctx, ret);
                index_val = JS_UNDEFINED;
                ret = JS_UNDEFINED;
                item = JS_UNDEFINED;
            }
        }
        break;
    case JS_ITERATOR_HELPER_KIND_SOME:
        {
            r = JS_FALSE;
            for (idx = 0; /*empty*/; idx++) {
                item = JS_IteratorNext(ctx, this_val, method, 0, NULL, &done);
                if (JS_IsException(item))
                    goto fail_no_close;
                if (done)
                    break;
                index_val = JS_NewInt64(ctx, idx);
                args[0] = item;
                args[1] = index_val;
                ret = JS_Call(ctx, func, JS_UNDEFINED, countof(args), args);
                JS_FreeValue(ctx, item);
                JS_FreeValue(ctx, index_val);
                if (JS_IsException(ret))
                    goto fail;
                if (JS_ToBoolFree(ctx, ret)) {
                    if (JS_IteratorClose(ctx, this_val, FALSE) < 0)
                        r = JS_EXCEPTION;
                    else
                        r = JS_TRUE;
                    break;
                }
                index_val = JS_UNDEFINED;
                ret = JS_UNDEFINED;
                item = JS_UNDEFINED;
            }
        }
        break;
    default:
        abort();
        break;
    }

    JS_FreeValue(ctx, func);
    JS_FreeValue(ctx, method);
    return r;
 fail:
    JS_IteratorClose(ctx, this_val, TRUE);
 fail_no_close:
    JS_FreeValue(ctx, func);
    JS_FreeValue(ctx, method);
    return JS_EXCEPTION;
}

JSValue js_iterator_proto_reduce(JSContext *ctx, JSValueConst this_val,
                                        int argc, JSValueConst *argv)
{
    JSValue item, method, ret, func, index_val, acc;
    JSValueConst args[3];
    int64_t idx;
    int done;

    if (!JS_IsObject(this_val))
        return JS_ThrowTypeErrorNotAnObject(ctx);
    acc = JS_UNDEFINED;
    func = JS_UNDEFINED;
    method = JS_UNDEFINED;
    if (check_function(ctx, argv[0]))
        goto exception;
    func = JS_DupValue(ctx, argv[0]);
    method = JS_GetProperty(ctx, this_val, JS_ATOM_next);
    if (JS_IsException(method))
        goto exception;
    if (argc > 1) {
        acc = JS_DupValue(ctx, argv[1]);
        idx = 0;
    } else {
        acc = JS_IteratorNext(ctx, this_val, method, 0, NULL, &done);
        if (JS_IsException(acc))
            goto exception_no_close;
        if (done) {
            JS_ThrowTypeError(ctx, "empty iterator");
            goto exception;
        }
        idx = 1;
    }
    for (/* empty */; /*empty*/; idx++) {
        item = JS_IteratorNext(ctx, this_val, method, 0, NULL, &done);
        if (JS_IsException(item))
            goto exception_no_close;
        if (done)
            break;
        index_val = JS_NewInt64(ctx, idx);
        args[0] = acc;
        args[1] = item;
        args[2] = index_val;
        ret = JS_Call(ctx, func, JS_UNDEFINED, countof(args), args);
        JS_FreeValue(ctx, item);
        JS_FreeValue(ctx, index_val);
        if (JS_IsException(ret))
            goto exception;
        JS_FreeValue(ctx, acc);
        acc = ret;
        index_val = JS_UNDEFINED;
        ret = JS_UNDEFINED;
        item = JS_UNDEFINED;
    }
    JS_FreeValue(ctx, func);
    JS_FreeValue(ctx, method);
    return acc;
 exception:
    JS_IteratorClose(ctx, this_val, TRUE);
 exception_no_close:
    JS_FreeValue(ctx, acc);
    JS_FreeValue(ctx, func);
    JS_FreeValue(ctx, method);
    return JS_EXCEPTION;
}

JSValue js_iterator_proto_toArray(JSContext *ctx, JSValueConst this_val,
                                         int argc, JSValueConst *argv)
{
    JSValue item, method, result;
    int64_t idx;
    int done;

    result = JS_UNDEFINED;
    if (!JS_IsObject(this_val))
        return JS_ThrowTypeErrorNotAnObject(ctx);
    method = JS_GetProperty(ctx, this_val, JS_ATOM_next);
    if (JS_IsException(method))
        return JS_EXCEPTION;
    result = JS_NewArray(ctx);
    if (JS_IsException(result))
        goto exception;
    for (idx = 0; /*empty*/; idx++) {
        item = JS_IteratorNext(ctx, this_val, method, 0, NULL, &done);
        if (JS_IsException(item))
            goto exception;
        if (done)
            break;
        if (JS_DefinePropertyValueInt64(ctx, result, idx, item,
                                        JS_PROP_C_W_E | JS_PROP_THROW) < 0)
            goto exception;
    }
    if (JS_SetProperty(ctx, result, JS_ATOM_length, JS_NewUint32(ctx, idx)) < 0)
        goto exception;
    JS_FreeValue(ctx, method);
    return result;
exception:
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, method);
    return JS_EXCEPTION;
}

JSValue js_iterator_proto_iterator(JSContext *ctx, JSValueConst this_val,
                                          int argc, JSValueConst *argv)
{
    return JS_DupValue(ctx, this_val);
}

JSValue js_iterator_proto_get_toStringTag(JSContext *ctx, JSValueConst this_val)
{
    return JS_AtomToString(ctx, JS_ATOM_Iterator);
}

JSValue js_iterator_proto_set_toStringTag(JSContext *ctx, JSValueConst this_val, JSValueConst val)
{
    int res;

    if (!JS_IsObject(this_val))
        return JS_ThrowTypeErrorNotAnObject(ctx);
    if (js_same_value(ctx, this_val, ctx->class_proto[JS_CLASS_ITERATOR]))
        return JS_ThrowTypeError(ctx, "Cannot assign to read only property");
    res = JS_GetOwnProperty(ctx, NULL, this_val, JS_ATOM_Symbol_toStringTag);
    if (res < 0)
        return JS_EXCEPTION;
    if (res) {
        if (JS_SetProperty(ctx, this_val, JS_ATOM_Symbol_toStringTag, JS_DupValue(ctx, val)) < 0)
            return JS_EXCEPTION;
    } else {
        if (JS_DefinePropertyValue(ctx, this_val, JS_ATOM_Symbol_toStringTag, JS_DupValue(ctx, val), JS_PROP_C_W_E) < 0)
            return JS_EXCEPTION;
    }
    return JS_UNDEFINED;
}

/* Iterator Helper */

void js_iterator_helper_finalizer(JSRuntime *rt, JSValue val)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JSIteratorHelperData *it = p->u.iterator_helper_data;
    if (it) {
        JS_FreeValueRT(rt, it->obj);
        JS_FreeValueRT(rt, it->func);
        JS_FreeValueRT(rt, it->next);
        JS_FreeValueRT(rt, it->inner);
        js_free_rt(rt, it);
    }
}

void js_iterator_helper_mark(JSRuntime *rt, JSValueConst val,
                                   JS_MarkFunc *mark_func)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JSIteratorHelperData *it = p->u.iterator_helper_data;
    if (it) {
        JS_MarkValue(rt, it->obj, mark_func);
        JS_MarkValue(rt, it->func, mark_func);
        JS_MarkValue(rt, it->next, mark_func);
        JS_MarkValue(rt, it->inner, mark_func);
    }
}

JSValue js_iterator_helper_next(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv,
                                      int *pdone, int magic)
{
    JSIteratorHelperData *it;
    JSValue ret;

    *pdone = FALSE;

    it = JS_GetOpaque2(ctx, this_val, JS_CLASS_ITERATOR_HELPER);
    if (!it)
        return JS_EXCEPTION;
    if (it->executing)
        return JS_ThrowTypeError(ctx, "cannot invoke a running iterator");
    if (it->done) {
        *pdone = TRUE;
        return JS_UNDEFINED;
    }

    it->executing = 1;

    switch (it->kind) {
    case JS_ITERATOR_HELPER_KIND_DROP:
        {
            JSValue item, method;
            if (magic == GEN_MAGIC_NEXT) {
                method = JS_DupValue(ctx, it->next);
            } else {
                method = JS_GetProperty(ctx, it->obj, JS_ATOM_return);
                if (JS_IsException(method))
                    goto fail;
            }
            while (it->count > 0) {
                it->count--;
                item = JS_IteratorNext(ctx, it->obj, method, 0, NULL, pdone);
                if (JS_IsException(item)) {
                    JS_FreeValue(ctx, method);
                    goto fail_no_close;
                }
                JS_FreeValue(ctx, item);
                if (magic == GEN_MAGIC_RETURN)
                    *pdone = TRUE;
                if (*pdone) {
                    JS_FreeValue(ctx, method);
                    ret = JS_UNDEFINED;
                    goto done;
                }
            }

            item = JS_IteratorNext(ctx, it->obj, method, 0, NULL, pdone);
            JS_FreeValue(ctx, method);
            if (JS_IsException(item))
                goto fail_no_close;
            ret = item;
            goto done;
        }
        break;
    case JS_ITERATOR_HELPER_KIND_FILTER:
        {
            JSValue item, method, selected, index_val;
            JSValueConst args[2];
            if (magic == GEN_MAGIC_NEXT) {
                method = JS_DupValue(ctx, it->next);
            } else {
                method = JS_GetProperty(ctx, it->obj, JS_ATOM_return);
                if (JS_IsException(method))
                    goto fail;
            }
        filter_again:
            item = JS_IteratorNext(ctx, it->obj, method, 0, NULL, pdone);
            if (JS_IsException(item)) {
                JS_FreeValue(ctx, method);
                goto fail_no_close;
            }
            if (*pdone || magic == GEN_MAGIC_RETURN) {
                JS_FreeValue(ctx, method);
                ret = item;
                goto done;
            }
            index_val = JS_NewInt64(ctx, it->count++);
            args[0] = item;
            args[1] = index_val;
            selected = JS_Call(ctx, it->func, JS_UNDEFINED, countof(args), args);
            JS_FreeValue(ctx, index_val);
            if (JS_IsException(selected)) {
                JS_FreeValue(ctx, item);
                JS_FreeValue(ctx, method);
                goto fail;
            }
            if (JS_ToBoolFree(ctx, selected)) {
                JS_FreeValue(ctx, method);
                ret = item;
                goto done;
            }
            JS_FreeValue(ctx, item);
            goto filter_again;
        }
        break;
    case JS_ITERATOR_HELPER_KIND_FLAT_MAP:
        {
            JSValue item, method, index_val, iter;
            JSValueConst args[2];
        flat_map_again:
            if (JS_IsUndefined(it->inner)) {
                if (magic == GEN_MAGIC_NEXT) {
                    method = JS_DupValue(ctx, it->next);
                } else {
                    method = JS_GetProperty(ctx, it->obj, JS_ATOM_return);
                    if (JS_IsException(method))
                        goto fail;
                }
                item = JS_IteratorNext(ctx, it->obj, method, 0, NULL, pdone);
                JS_FreeValue(ctx, method);
                if (JS_IsException(item))
                    goto fail_no_close;
                if (*pdone || magic == GEN_MAGIC_RETURN) {
                    ret = item;
                    goto done;
                }
                index_val = JS_NewInt64(ctx, it->count++);
                args[0] = item;
                args[1] = index_val;
                ret = JS_Call(ctx, it->func, JS_UNDEFINED, countof(args), args);
                JS_FreeValue(ctx, item);
                JS_FreeValue(ctx, index_val);
                if (JS_IsException(ret))
                    goto fail;
                if (!JS_IsObject(ret)) {
                    JS_FreeValue(ctx, ret);
                    JS_ThrowTypeError(ctx, "not an object");
                    goto fail;
                }
                method = JS_GetProperty(ctx, ret, JS_ATOM_Symbol_iterator);
                if (JS_IsException(method)) {
                    JS_FreeValue(ctx, ret);
                    goto fail;
                }
                if (JS_IsNull(method) || JS_IsUndefined(method)) {
                    JS_FreeValue(ctx, method);
                    iter = ret;
                } else {
                    iter = JS_GetIterator2(ctx, ret, method);
                    JS_FreeValue(ctx, method);
                    JS_FreeValue(ctx, ret);
                    if (JS_IsException(iter))
                        goto fail;
                }

                it->inner = iter;
            }

            if (magic == GEN_MAGIC_NEXT)
                method = JS_GetProperty(ctx, it->inner, JS_ATOM_next);
            else
                method = JS_GetProperty(ctx, it->inner, JS_ATOM_return);
            if (JS_IsException(method)) {
            inner_fail:
                JS_IteratorClose(ctx, it->inner, FALSE);
                JS_FreeValue(ctx, it->inner);
                it->inner = JS_UNDEFINED;
                goto fail;
            }
            if (magic == GEN_MAGIC_RETURN && (JS_IsUndefined(method) || JS_IsNull(method))) {
                goto inner_end;
            } else {
                item = JS_IteratorNext(ctx, it->inner, method, 0, NULL, pdone);
                JS_FreeValue(ctx, method);
                if (JS_IsException(item))
                    goto inner_fail;
            }
            if (*pdone) {
            inner_end:
                *pdone = FALSE; // The outer iterator must continue.
                JS_IteratorClose(ctx, it->inner, FALSE);
                JS_FreeValue(ctx, it->inner);
                it->inner = JS_UNDEFINED;
                goto flat_map_again;
            }
            ret = item;
            goto done;
        }
        break;
    case JS_ITERATOR_HELPER_KIND_MAP:
        {
            JSValue item, method, index_val;
            JSValueConst args[2];
            if (magic == GEN_MAGIC_NEXT) {
                method = JS_DupValue(ctx, it->next);
            } else {
                method = JS_GetProperty(ctx, it->obj, JS_ATOM_return);
                if (JS_IsException(method))
                    goto fail;
            }
            item = JS_IteratorNext(ctx, it->obj, method, 0, NULL, pdone);
            JS_FreeValue(ctx, method);
            if (JS_IsException(item))
                goto fail_no_close;
            if (*pdone || magic == GEN_MAGIC_RETURN) {
                ret = item;
                goto done;
            }
            index_val = JS_NewInt64(ctx, it->count++);
            args[0] = item;
            args[1] = index_val;
            ret = JS_Call(ctx, it->func, JS_UNDEFINED, countof(args), args);
            JS_FreeValue(ctx, index_val);
            JS_FreeValue(ctx, item);
            if (JS_IsException(ret))
                goto fail;
            goto done;
        }
        break;
    case JS_ITERATOR_HELPER_KIND_TAKE:
        {
            JSValue item, method;
            if (it->count > 0) {
                if (magic == GEN_MAGIC_NEXT) {
                    method = JS_DupValue(ctx, it->next);
                } else {
                    method = JS_GetProperty(ctx, it->obj, JS_ATOM_return);
                    if (JS_IsException(method))
                        goto fail;
                }
                it->count--;
                item = JS_IteratorNext(ctx, it->obj, method, 0, NULL, pdone);
                JS_FreeValue(ctx, method);
                if (JS_IsException(item))
                    goto fail_no_close;
                ret = item;
                goto done;
            }

            *pdone = TRUE;
            if (JS_IteratorClose(ctx, it->obj, FALSE))
                ret = JS_EXCEPTION;
            else
                ret = JS_UNDEFINED;
            goto done;
        }
        break;
    default:
        abort();
    }

 done:
    it->done = magic == GEN_MAGIC_NEXT ? *pdone : 1;
    it->executing = 0;
    return ret;
 fail:
    /* close the iterator object, preserving pending exception */
    JS_IteratorClose(ctx, it->obj, TRUE);
 fail_no_close:
    ret = JS_EXCEPTION;
    goto done;
}

/* Number */

JSValue js_number_constructor(JSContext *ctx, JSValueConst new_target,
                                     int argc, JSValueConst *argv)
{
    JSValue val, obj;
    if (argc == 0) {
        val = JS_NewInt32(ctx, 0);
    } else {
        val = JS_ToNumeric(ctx, argv[0]);
        if (JS_IsException(val))
            return val;
        switch(JS_VALUE_GET_TAG(val)) {
        case JS_TAG_SHORT_BIG_INT:
            val = JS_NewInt64(ctx, JS_VALUE_GET_SHORT_BIG_INT(val));
            if (JS_IsException(val))
                return val;
            break;
        case JS_TAG_BIG_INT:
            {
                JSBigInt *p = JS_VALUE_GET_PTR(val);
                double d;
                d = js_bigint_to_float64(ctx, p);
                JS_FreeValue(ctx, val);
                val = JS_NewFloat64(ctx, d);
            }
            break;
        default:
            break;
        }
    }
    if (!JS_IsUndefined(new_target)) {
        obj = js_create_from_ctor(ctx, new_target, JS_CLASS_NUMBER);
        if (!JS_IsException(obj))
            JS_SetObjectData(ctx, obj, val);
        return obj;
    } else {
        return val;
    }
}

#if 0
JSValue js_number___toInteger(JSContext *ctx, JSValueConst this_val,
                                     int argc, JSValueConst *argv)
{
    return JS_ToIntegerFree(ctx, JS_DupValue(ctx, argv[0]));
}

JSValue js_number___toLength(JSContext *ctx, JSValueConst this_val,
                                    int argc, JSValueConst *argv)
{
    int64_t v;
    if (JS_ToLengthFree(ctx, &v, JS_DupValue(ctx, argv[0])))
        return JS_EXCEPTION;
    return JS_NewInt64(ctx, v);
}
#endif

JSValue js_number_isNaN(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv)
{
    if (!JS_IsNumber(argv[0]))
        return JS_FALSE;
    return js_global_isNaN(ctx, this_val, argc, argv);
}

JSValue js_number_isFinite(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv)
{
    if (!JS_IsNumber(argv[0]))
        return JS_FALSE;
    return js_global_isFinite(ctx, this_val, argc, argv);
}

JSValue js_number_isInteger(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv)
{
    int ret;
    ret = JS_NumberIsInteger(ctx, argv[0]);
    if (ret < 0)
        return JS_EXCEPTION;
    else
        return JS_NewBool(ctx, ret);
}

JSValue js_number_isSafeInteger(JSContext *ctx, JSValueConst this_val,
                                       int argc, JSValueConst *argv)
{
    double d;
    if (!JS_IsNumber(argv[0]))
        return JS_FALSE;
    if (unlikely(JS_ToFloat64(ctx, &d, argv[0])))
        return JS_EXCEPTION;
    return JS_NewBool(ctx, is_safe_integer(d));
}

JSValue js_thisNumberValue(JSContext *ctx, JSValueConst this_val)
{
    if (JS_IsNumber(this_val))
        return JS_DupValue(ctx, this_val);

    if (JS_VALUE_GET_TAG(this_val) == JS_TAG_OBJECT) {
        JSObject *p = JS_VALUE_GET_OBJ(this_val);
        if (p->class_id == JS_CLASS_NUMBER) {
            if (JS_IsNumber(p->u.object_data))
                return JS_DupValue(ctx, p->u.object_data);
        }
    }
    return JS_ThrowTypeError(ctx, "not a number");
}

JSValue js_number_valueOf(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv)
{
    return js_thisNumberValue(ctx, this_val);
}

int js_get_radix(JSContext *ctx, JSValueConst val)
{
    int radix;
    if (JS_ToInt32Sat(ctx, &radix, val))
        return -1;
    if (radix < 2 || radix > 36) {
        JS_ThrowRangeError(ctx, "radix must be between 2 and 36");
        return -1;
    }
    return radix;
}

JSValue js_number_toString(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv, int magic)
{
    JSValue val;
    int base, flags;
    double d;

    val = js_thisNumberValue(ctx, this_val);
    if (JS_IsException(val))
        return val;
    if (magic || JS_IsUndefined(argv[0])) {
        base = 10;
    } else {
        base = js_get_radix(ctx, argv[0]);
        if (base < 0)
            goto fail;
    }
    if (JS_VALUE_GET_TAG(val) == JS_TAG_INT) {
        char buf1[70];
        int len;
        len = i64toa_radix(buf1, JS_VALUE_GET_INT(val), base);
        return js_new_string8_len(ctx, buf1, len);
    }
    if (JS_ToFloat64Free(ctx, &d, val))
        return JS_EXCEPTION;
    flags = JS_DTOA_FORMAT_FREE;
    if (base != 10)
        flags |= JS_DTOA_EXP_DISABLED;
    return js_dtoa2(ctx, d, base, 0, flags);
 fail:
    JS_FreeValue(ctx, val);
    return JS_EXCEPTION;
}

JSValue js_number_toFixed(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv)
{
    JSValue val;
    int f, flags;
    double d;

    val = js_thisNumberValue(ctx, this_val);
    if (JS_IsException(val))
        return val;
    if (JS_ToFloat64Free(ctx, &d, val))
        return JS_EXCEPTION;
    if (JS_ToInt32Sat(ctx, &f, argv[0]))
        return JS_EXCEPTION;
    if (f < 0 || f > 100)
        return JS_ThrowRangeError(ctx, "invalid number of digits");
    if (fabs(d) >= 1e21)
        flags = JS_DTOA_FORMAT_FREE;
    else
        flags = JS_DTOA_FORMAT_FRAC;
    return js_dtoa2(ctx, d, 10, f, flags);
}

JSValue js_number_toExponential(JSContext *ctx, JSValueConst this_val,
                                       int argc, JSValueConst *argv)
{
    JSValue val;
    int f, flags;
    double d;

    val = js_thisNumberValue(ctx, this_val);
    if (JS_IsException(val))
        return val;
    if (JS_ToFloat64Free(ctx, &d, val))
        return JS_EXCEPTION;
    if (JS_ToInt32Sat(ctx, &f, argv[0]))
        return JS_EXCEPTION;
    if (!isfinite(d)) {
        return JS_ToStringFree(ctx,  __JS_NewFloat64(ctx, d));
    }
    if (JS_IsUndefined(argv[0])) {
        flags = JS_DTOA_FORMAT_FREE;
        f = 0;
    } else {
        if (f < 0 || f > 100)
            return JS_ThrowRangeError(ctx, "invalid number of digits");
        f++;
        flags = JS_DTOA_FORMAT_FIXED;
    }
    return js_dtoa2(ctx, d, 10, f, flags | JS_DTOA_EXP_ENABLED);
}

JSValue js_number_toPrecision(JSContext *ctx, JSValueConst this_val,
                                     int argc, JSValueConst *argv)
{
    JSValue val;
    int p;
    double d;

    val = js_thisNumberValue(ctx, this_val);
    if (JS_IsException(val))
        return val;
    if (JS_ToFloat64Free(ctx, &d, val))
        return JS_EXCEPTION;
    if (JS_IsUndefined(argv[0]))
        goto to_string;
    if (JS_ToInt32Sat(ctx, &p, argv[0]))
        return JS_EXCEPTION;
    if (!isfinite(d)) {
    to_string:
        return JS_ToStringFree(ctx,  __JS_NewFloat64(ctx, d));
    }
    if (p < 1 || p > 100)
        return JS_ThrowRangeError(ctx, "invalid number of digits");
    return js_dtoa2(ctx, d, 10, p, JS_DTOA_FORMAT_FIXED);
}
