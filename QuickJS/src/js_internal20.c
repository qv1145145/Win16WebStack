#include "js_internal.h"



JSValue js_set_isSubsetOf(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv)
{
    JSValue item, iter, keys, has, next, rv, rval;
    BOOL found;
    JSMapState *s;
    int64_t size;
    int done, ok;

    iter = JS_UNDEFINED;
    next = JS_UNDEFINED;
    rval = JS_EXCEPTION;
    s = JS_GetOpaque2(ctx, this_val, JS_CLASS_SET);
    if (!s)
        return JS_EXCEPTION;
    if (get_set_record(ctx, argv[0], &size, &has, &keys) < 0)
        goto exception;
    found = FALSE;
    if (s->record_count > size)
        goto fini;
    iter = js_create_map_iterator(ctx, this_val, 0, NULL, MAGIC_SET);
    if (JS_IsException(iter))
        goto exception;
    found = TRUE;
    do {
        item = js_map_iterator_next(ctx, iter, 0, NULL, &done, MAGIC_SET);
        if (JS_IsException(item))
            goto exception;
        if (done) // item is JS_UNDEFINED
            break;
        rv = JS_Call(ctx, has, argv[0], 1, (JSValueConst *)&item);
        JS_FreeValue(ctx, item);
        ok = JS_ToBoolFree(ctx, rv); // returns -1 if rv is JS_EXCEPTION
        if (ok < 0)
            goto exception;
        found = (ok > 0);
    } while (found);
fini:
    rval = found ? JS_TRUE : JS_FALSE;
exception:
    JS_FreeValue(ctx, has);
    JS_FreeValue(ctx, keys);
    JS_FreeValue(ctx, iter);
    JS_FreeValue(ctx, next);
    return rval;
}

JSValue js_set_isSupersetOf(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv)
{
    JSValue item, iter, keys, has, next, rval;
    int done;
    BOOL found;
    JSMapState *s;
    int64_t size;

    iter = JS_UNDEFINED;
    next = JS_UNDEFINED;
    rval = JS_EXCEPTION;
    s = JS_GetOpaque2(ctx, this_val, JS_CLASS_SET);
    if (!s)
        return JS_EXCEPTION;
    if (get_set_record(ctx, argv[0], &size, &has, &keys) < 0)
        goto exception;
    found = FALSE;
    if (s->record_count < size)
        goto fini;
    iter = JS_Call(ctx, keys, argv[0], 0, NULL);
    if (JS_IsException(iter))
        goto exception;
    next = JS_GetProperty(ctx, iter, JS_ATOM_next);
    if (JS_IsException(next))
        goto exception;
    found = TRUE;
    for(;;) {
        item = JS_IteratorNext(ctx, iter, next, 0, NULL, &done);
        if (JS_IsException(item))
            goto exception;
        if (done) // item is JS_UNDEFINED
            break;
        item = map_normalize_key(ctx, item);
        found = (NULL != map_find_record(ctx, s, item));
        JS_FreeValue(ctx, item);
        if (!found) {
            JS_IteratorClose(ctx, iter, FALSE);
            break;
        }
    }
fini:
    rval = found ? JS_TRUE : JS_FALSE;
exception:
    JS_FreeValue(ctx, has);
    JS_FreeValue(ctx, keys);
    JS_FreeValue(ctx, iter);
    JS_FreeValue(ctx, next);
    return rval;
}

JSValue js_set_intersection(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv)
{
    JSValue newset, item, iter, keys, has, next, rv;
    JSMapState *s, *t;
    JSMapRecord *mr;
    int64_t size;
    int done, ok;

    iter = JS_UNDEFINED;
    next = JS_UNDEFINED;
    newset = JS_UNDEFINED;
    s = JS_GetOpaque2(ctx, this_val, JS_CLASS_SET);
    if (!s)
        return JS_EXCEPTION;
    if (get_set_record(ctx, argv[0], &size, &has, &keys) < 0)
        goto exception;
    if (s->record_count > size) {
        iter = JS_Call(ctx, keys, argv[0], 0, NULL);
        if (JS_IsException(iter))
            goto exception;
        next = JS_GetProperty(ctx, iter, JS_ATOM_next);
        if (JS_IsException(next))
            goto exception;
        newset = js_map_constructor(ctx, JS_UNDEFINED, 0, NULL, MAGIC_SET);
        if (JS_IsException(newset))
            goto exception;
        t = JS_GetOpaque(newset, JS_CLASS_SET);
        for (;;) {
            item = JS_IteratorNext(ctx, iter, next, 0, NULL, &done);
            if (JS_IsException(item))
                goto exception;
            if (done) // item is JS_UNDEFINED
                break;
            item = map_normalize_key(ctx, item);
            if (!map_find_record(ctx, s, item)) {
                JS_FreeValue(ctx, item);
            } else if (map_find_record(ctx, t, item)) {
                JS_FreeValue(ctx, item); // no duplicates
            } else {
                mr = set_add_record(ctx, t, item);
                JS_FreeValue(ctx, item);
                if (!mr)
                    goto exception;
            }
        }
    } else {
        iter = js_create_map_iterator(ctx, this_val, 0, NULL, MAGIC_SET);
        if (JS_IsException(iter))
            goto exception;
        newset = js_map_constructor(ctx, JS_UNDEFINED, 0, NULL, MAGIC_SET);
        if (JS_IsException(newset))
            goto exception;
        t = JS_GetOpaque(newset, JS_CLASS_SET);
        for (;;) {
            item = js_map_iterator_next(ctx, iter, 0, NULL, &done, MAGIC_SET);
            if (JS_IsException(item))
                goto exception;
            if (done) // item is JS_UNDEFINED
                break;
            rv = JS_Call(ctx, has, argv[0], 1, (JSValueConst *)&item);
            ok = JS_ToBoolFree(ctx, rv); // returns -1 if rv is JS_EXCEPTION
            if (ok > 0) {
                item = map_normalize_key(ctx, item);
                if (map_find_record(ctx, t, item)) {
                    JS_FreeValue(ctx, item); // no duplicates
                } else {
                    mr = set_add_record(ctx, t, item);
                    JS_FreeValue(ctx, item);
                    if (!mr)
                        goto exception;
                }
            } else {
                JS_FreeValue(ctx, item);
                if (ok < 0)
                    goto exception;
            }
        }
    }
    goto fini;
exception:
    JS_FreeValue(ctx, newset);
    newset = JS_EXCEPTION;
fini:
    JS_FreeValue(ctx, has);
    JS_FreeValue(ctx, keys);
    JS_FreeValue(ctx, iter);
    JS_FreeValue(ctx, next);
    return newset;
}

JSValue js_set_difference(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv)
{
    JSValue newset, item, iter, keys, has, next, rv;
    JSMapState *s, *t;
    int64_t size;
    int done;
    int ok;

    iter = JS_UNDEFINED;
    next = JS_UNDEFINED;
    newset = JS_UNDEFINED;
    s = JS_GetOpaque2(ctx, this_val, JS_CLASS_SET);
    if (!s)
        return JS_EXCEPTION;
    if (get_set_record(ctx, argv[0], &size, &has, &keys) < 0)
        goto exception;

    newset = js_copy_set(ctx, this_val);
    if (JS_IsException(newset))
        goto exception;
    t = JS_GetOpaque(newset, JS_CLASS_SET);

    if (s->record_count <= size) {
        iter = js_create_map_iterator(ctx, newset, 0, NULL, MAGIC_SET);
        if (JS_IsException(iter))
            goto exception;
        for (;;) {
            item = js_map_iterator_next(ctx, iter, 0, NULL, &done, MAGIC_SET);
            if (JS_IsException(item))
                goto exception;
            if (done) // item is JS_UNDEFINED
                break;
            rv = JS_Call(ctx, has, argv[0], 1, (JSValueConst *)&item);
            ok = JS_ToBoolFree(ctx, rv); // returns -1 if rv is JS_EXCEPTION
            if (ok < 0) {
                JS_FreeValue(ctx, item);
                goto exception;
            }
            if (ok) {
                map_delete_record(ctx, t, item);
            }
            JS_FreeValue(ctx, item);
        }
    } else {
        iter = JS_Call(ctx, keys, argv[0], 0, NULL);
        if (JS_IsException(iter))
            goto exception;
        next = JS_GetProperty(ctx, iter, JS_ATOM_next);
        if (JS_IsException(next))
            goto exception;
        for (;;) {
            item = JS_IteratorNext(ctx, iter, next, 0, NULL, &done);
            if (JS_IsException(item))
                goto exception;
            if (done) // item is JS_UNDEFINED
                break;
            map_delete_record(ctx, t, item);
            JS_FreeValue(ctx, item);
        }
    }
    goto fini;
exception:
    JS_FreeValue(ctx, newset);
    newset = JS_EXCEPTION;
fini:
    JS_FreeValue(ctx, has);
    JS_FreeValue(ctx, keys);
    JS_FreeValue(ctx, iter);
    JS_FreeValue(ctx, next);
    return newset;
}

JSValue js_set_symmetricDifference(JSContext *ctx, JSValueConst this_val,
                                          int argc, JSValueConst *argv)
{
    JSValue newset, item, iter, next, has, keys;
    JSMapState *s, *t;
    JSMapRecord *mr;
    int64_t size;
    int done;
    BOOL present;

    s = JS_GetOpaque2(ctx, this_val, JS_CLASS_SET);
    if (!s)
        return JS_EXCEPTION;
    if (get_set_record(ctx, argv[0], &size, &has, &keys) < 0)
        return JS_EXCEPTION;
    JS_FreeValue(ctx, has);

    next = JS_UNDEFINED;
    newset = JS_UNDEFINED;
    iter = JS_Call(ctx, keys, argv[0], 0, NULL);
    if (JS_IsException(iter))
        goto exception;
    next = JS_GetProperty(ctx, iter, JS_ATOM_next);
    if (JS_IsException(next))
        goto exception;
    newset = js_copy_set(ctx, this_val);
    if (JS_IsException(newset))
        goto exception;
    t = JS_GetOpaque(newset, JS_CLASS_SET);
    for (;;) {
        item = JS_IteratorNext(ctx, iter, next, 0, NULL, &done);
        if (JS_IsException(item))
            goto exception;
        if (done) // item is JS_UNDEFINED
            break;
        // note the subtlety here: due to mutating iterators, it's
        // possible for keys to disappear during iteration; test262
        // still expects us to maintain insertion order though, so
        // we first check |this|, then |new|; |new| is a copy of |this|
        // - if item exists in |this|, delete (if it exists) from |new|
        // - if item misses in |this| and |new|, add to |new|
        // - if item exists in |new| but misses in |this|, *don't* add it,
        //   mutating iterator erased it
        item = map_normalize_key(ctx, item);
        present = (NULL != map_find_record(ctx, s, item));
        mr = map_find_record(ctx, t, item);
        if (present) {
            map_delete_record(ctx, t, item);
            JS_FreeValue(ctx, item);
        } else if (mr) {
            JS_FreeValue(ctx, item);
        } else {
            mr = set_add_record(ctx, t, item);
            JS_FreeValue(ctx, item);
            if (!mr)
                goto exception;
        }
    }
    goto fini;
exception:
    JS_FreeValue(ctx, newset);
    newset = JS_EXCEPTION;
fini:
    JS_FreeValue(ctx, next);
    JS_FreeValue(ctx, iter);
    JS_FreeValue(ctx, keys);
    return newset;
}

JSValue js_set_union(JSContext *ctx, JSValueConst this_val,
                            int argc, JSValueConst *argv)
{
    JSValue newset, item, iter, next, has, keys, rv;
    JSMapState *s;
    int64_t size;
    int done;

    s = JS_GetOpaque2(ctx, this_val, JS_CLASS_SET);
    if (!s)
        return JS_EXCEPTION;
    if (get_set_record(ctx, argv[0], &size, &has, &keys) < 0)
        return JS_EXCEPTION;
    JS_FreeValue(ctx, has);

    next = JS_UNDEFINED;
    newset = JS_UNDEFINED;
    iter = JS_Call(ctx, keys, argv[0], 0, NULL);
    if (JS_IsException(iter))
        goto exception;
    next = JS_GetProperty(ctx, iter, JS_ATOM_next);
    if (JS_IsException(next))
        goto exception;

    newset = js_copy_set(ctx, this_val);
    if (JS_IsException(newset))
        goto exception;

    for (;;) {
        item = JS_IteratorNext(ctx, iter, next, 0, NULL, &done);
        if (JS_IsException(item))
            goto exception;
        if (done) // item is JS_UNDEFINED
            break;
        rv = js_map_set(ctx, newset, 1, (JSValueConst *)&item, MAGIC_SET);
        JS_FreeValue(ctx, item);
        if (JS_IsException(rv))
            goto exception;
        JS_FreeValue(ctx, rv);
    }
    goto fini;
exception:
    JS_FreeValue(ctx, newset);
    newset = JS_EXCEPTION;
fini:
    JS_FreeValue(ctx, next);
    JS_FreeValue(ctx, iter);
    JS_FreeValue(ctx, keys);
    return newset;
}

int js_create_resolving_functions(JSContext *ctx, JSValue *args,
                                         JSValueConst promise);

void promise_reaction_data_free(JSRuntime *rt,
                                       JSPromiseReactionData *rd)
{
    JS_FreeValueRT(rt, rd->resolving_funcs[0]);
    JS_FreeValueRT(rt, rd->resolving_funcs[1]);
    JS_FreeValueRT(rt, rd->handler);
    js_free_rt(rt, rd);
}

JSValue promise_reaction_job(JSContext *ctx, int argc,
                                    JSValueConst *argv)
{
    JSValueConst handler, arg, func;
    JSValue res, res2;
    BOOL is_reject;

    assert(argc == 5);
    handler = argv[2];
    is_reject = JS_ToBool(ctx, argv[3]);
    arg = argv[4];
#ifdef DUMP_PROMISE
    printf("promise_reaction_job: is_reject=%d\n", is_reject);
#endif

    if (JS_IsUndefined(handler)) {
        if (is_reject) {
            res = JS_Throw(ctx, JS_DupValue(ctx, arg));
        } else {
            res = JS_DupValue(ctx, arg);
        }
    } else {
        res = JS_Call(ctx, handler, JS_UNDEFINED, 1, &arg);
    }
    is_reject = JS_IsException(res);
    if (is_reject)
        res = JS_GetException(ctx);
    func = argv[is_reject];
    /* as an extension, we support undefined as value to avoid
       creating a dummy promise in the 'await' implementation of async
       functions */
    if (!JS_IsUndefined(func)) {
        res2 = JS_Call(ctx, func, JS_UNDEFINED,
                       1, (JSValueConst *)&res);
    } else {
        res2 = JS_UNDEFINED;
    }
    JS_FreeValue(ctx, res);

    return res2;
}

void fulfill_or_reject_promise(JSContext *ctx, JSValueConst promise,
                                      JSValueConst value, BOOL is_reject)
{
    JSPromiseData *s = JS_GetOpaque(promise, JS_CLASS_PROMISE);
    struct list_head *el, *el1;
    JSPromiseReactionData *rd;
    JSValueConst args[5];

    if (!s || s->promise_state != JS_PROMISE_PENDING)
        return; /* should never happen */
    set_value(ctx, &s->promise_result, JS_DupValue(ctx, value));
    s->promise_state = JS_PROMISE_FULFILLED + is_reject;
#ifdef DUMP_PROMISE
    printf("fulfill_or_reject_promise: is_reject=%d\n", is_reject);
#endif
    if (s->promise_state == JS_PROMISE_REJECTED && !s->is_handled) {
        JSRuntime *rt = ctx->rt;
        if (rt->host_promise_rejection_tracker) {
            rt->host_promise_rejection_tracker(ctx, promise, value, FALSE,
                                               rt->host_promise_rejection_tracker_opaque);
        }
    }

    list_for_each_safe(el, el1, &s->promise_reactions[is_reject]) {
        rd = list_entry(el, JSPromiseReactionData, link);
        args[0] = rd->resolving_funcs[0];
        args[1] = rd->resolving_funcs[1];
        args[2] = rd->handler;
        args[3] = JS_NewBool(ctx, is_reject);
        args[4] = value;
        JS_EnqueueJob(ctx, promise_reaction_job, 5, args);
        list_del(&rd->link);
        promise_reaction_data_free(ctx->rt, rd);
    }

    list_for_each_safe(el, el1, &s->promise_reactions[1 - is_reject]) {
        rd = list_entry(el, JSPromiseReactionData, link);
        list_del(&rd->link);
        promise_reaction_data_free(ctx->rt, rd);
    }
}

void reject_promise(JSContext *ctx, JSValueConst promise,
                           JSValueConst value)
{
    fulfill_or_reject_promise(ctx, promise, value, TRUE);
}

JSValue js_promise_resolve_thenable_job(JSContext *ctx,
                                               int argc, JSValueConst *argv)
{
    JSValueConst promise, thenable, then;
    JSValue args[2], res;

#ifdef DUMP_PROMISE
    printf("js_promise_resolve_thenable_job\n");
#endif
    assert(argc == 3);
    promise = argv[0];
    thenable = argv[1];
    then = argv[2];
    if (js_create_resolving_functions(ctx, args, promise) < 0)
        return JS_EXCEPTION;
    res = JS_Call(ctx, then, thenable, 2, (JSValueConst *)args);
    if (JS_IsException(res)) {
        JSValue error = JS_GetException(ctx);
        res = JS_Call(ctx, args[1], JS_UNDEFINED, 1, (JSValueConst *)&error);
        JS_FreeValue(ctx, error);
    }
    JS_FreeValue(ctx, args[0]);
    JS_FreeValue(ctx, args[1]);
    return res;
}

void js_promise_resolve_function_free_resolved(JSRuntime *rt,
                                                      JSPromiseFunctionDataResolved *sr)
{
    if (--sr->ref_count == 0) {
        js_free_rt(rt, sr);
    }
}

int js_create_resolving_functions(JSContext *ctx,
                                         JSValue *resolving_funcs,
                                         JSValueConst promise)

{
    JSValue obj;
    JSPromiseFunctionData *s;
    JSPromiseFunctionDataResolved *sr;
    int i, ret;

    sr = js_malloc(ctx, sizeof(*sr));
    if (!sr)
        return -1;
    sr->ref_count = 1;
    sr->already_resolved = FALSE; /* must be shared between the two functions */
    ret = 0;
    for(i = 0; i < 2; i++) {
        obj = JS_NewObjectProtoClass(ctx, ctx->function_proto,
                                     JS_CLASS_PROMISE_RESOLVE_FUNCTION + i);
        if (JS_IsException(obj))
            goto fail;
        s = js_malloc(ctx, sizeof(*s));
        if (!s) {
            JS_FreeValue(ctx, obj);
        fail:

            if (i != 0)
                JS_FreeValue(ctx, resolving_funcs[0]);
            ret = -1;
            break;
        }
        sr->ref_count++;
        s->presolved = sr;
        s->promise = JS_DupValue(ctx, promise);
        JS_SetOpaque(obj, s);
        js_function_set_properties(ctx, obj, JS_ATOM_empty_string, 1);
        resolving_funcs[i] = obj;
    }
    js_promise_resolve_function_free_resolved(ctx->rt, sr);
    return ret;
}

void js_promise_resolve_function_finalizer(JSRuntime *rt, JSValue val)
{
    JSPromiseFunctionData *s = JS_VALUE_GET_OBJ(val)->u.promise_function_data;
    if (s) {
        js_promise_resolve_function_free_resolved(rt, s->presolved);
        JS_FreeValueRT(rt, s->promise);
        js_free_rt(rt, s);
    }
}

void js_promise_resolve_function_mark(JSRuntime *rt, JSValueConst val,
                                             JS_MarkFunc *mark_func)
{
    JSPromiseFunctionData *s = JS_VALUE_GET_OBJ(val)->u.promise_function_data;
    if (s) {
        JS_MarkValue(rt, s->promise, mark_func);
    }
}

JSValue js_promise_resolve_function_call(JSContext *ctx,
                                                JSValueConst func_obj,
                                                JSValueConst this_val,
                                                int argc, JSValueConst *argv,
                                                int flags)
{
    JSObject *p = JS_VALUE_GET_OBJ(func_obj);
    JSPromiseFunctionData *s;
    JSValueConst resolution, args[3];
    JSValue then;
    BOOL is_reject;

    s = p->u.promise_function_data;
    if (!s || s->presolved->already_resolved)
        return JS_UNDEFINED;
    s->presolved->already_resolved = TRUE;
    is_reject = p->class_id - JS_CLASS_PROMISE_RESOLVE_FUNCTION;
    if (argc > 0)
        resolution = argv[0];
    else
        resolution = JS_UNDEFINED;
#ifdef DUMP_PROMISE
    printf("js_promise_resolving_function_call: is_reject=%d ", is_reject);
    JS_DumpValue(ctx, "resolution", resolution);
    printf("\n");
#endif
    if (is_reject || !JS_IsObject(resolution)) {
        goto done;
    } else if (js_same_value(ctx, resolution, s->promise)) {
        JS_ThrowTypeError(ctx, "promise self resolution");
        goto fail_reject;
    }
    then = JS_GetProperty(ctx, resolution, JS_ATOM_then);
    if (JS_IsException(then)) {
        JSValue error;
    fail_reject:
        error = JS_GetException(ctx);
        reject_promise(ctx, s->promise, error);
        JS_FreeValue(ctx, error);
    } else if (! JS_IsFunction(ctx, then)) {
        JS_FreeValue(ctx, then);
    done:
        fulfill_or_reject_promise(ctx, s->promise, resolution, is_reject);
    } else {
        args[0] = s->promise;
        args[1] = resolution;
        args[2] = then;
        JS_EnqueueJob(ctx, js_promise_resolve_thenable_job, 3, args);
        JS_FreeValue(ctx, then);
    }
    return JS_UNDEFINED;
}

void js_promise_finalizer(JSRuntime *rt, JSValue val)
{
    JSPromiseData *s = JS_GetOpaque(val, JS_CLASS_PROMISE);
    struct list_head *el, *el1;
    int i;

    if (!s)
        return;
    for(i = 0; i < 2; i++) {
        list_for_each_safe(el, el1, &s->promise_reactions[i]) {
            JSPromiseReactionData *rd =
                list_entry(el, JSPromiseReactionData, link);
            promise_reaction_data_free(rt, rd);
        }
    }
    JS_FreeValueRT(rt, s->promise_result);
    js_free_rt(rt, s);
}

void js_promise_mark(JSRuntime *rt, JSValueConst val,
                            JS_MarkFunc *mark_func)
{
    JSPromiseData *s = JS_GetOpaque(val, JS_CLASS_PROMISE);
    struct list_head *el;
    int i;

    if (!s)
        return;
    for(i = 0; i < 2; i++) {
        list_for_each(el, &s->promise_reactions[i]) {
            JSPromiseReactionData *rd =
                list_entry(el, JSPromiseReactionData, link);
            JS_MarkValue(rt, rd->resolving_funcs[0], mark_func);
            JS_MarkValue(rt, rd->resolving_funcs[1], mark_func);
            JS_MarkValue(rt, rd->handler, mark_func);
        }
    }
    JS_MarkValue(rt, s->promise_result, mark_func);
}

JSValue js_promise_constructor(JSContext *ctx, JSValueConst new_target,
                                      int argc, JSValueConst *argv)
{
    JSValueConst executor;
    JSValue obj;
    JSPromiseData *s;
    JSValue args[2], ret;
    int i;

    executor = argv[0];
    if (check_function(ctx, executor))
        return JS_EXCEPTION;
    obj = js_create_from_ctor(ctx, new_target, JS_CLASS_PROMISE);
    if (JS_IsException(obj))
        return JS_EXCEPTION;
    s = js_mallocz(ctx, sizeof(*s));
    if (!s)
        goto fail;
    s->promise_state = JS_PROMISE_PENDING;
    s->is_handled = FALSE;
    for(i = 0; i < 2; i++)
        init_list_head(&s->promise_reactions[i]);
    s->promise_result = JS_UNDEFINED;
    JS_SetOpaque(obj, s);
    if (js_create_resolving_functions(ctx, args, obj))
        goto fail;
    ret = JS_Call(ctx, executor, JS_UNDEFINED, 2, (JSValueConst *)args);
    if (JS_IsException(ret)) {
        JSValue ret2, error;
        error = JS_GetException(ctx);
        ret2 = JS_Call(ctx, args[1], JS_UNDEFINED, 1, (JSValueConst *)&error);
        JS_FreeValue(ctx, error);
        if (JS_IsException(ret2))
            goto fail1;
        JS_FreeValue(ctx, ret2);
    }
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, args[0]);
    JS_FreeValue(ctx, args[1]);
    return obj;
 fail1:
    JS_FreeValue(ctx, args[0]);
    JS_FreeValue(ctx, args[1]);
 fail:
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

JSValue js_promise_executor(JSContext *ctx,
                                   JSValueConst this_val,
                                   int argc, JSValueConst *argv,
                                   int magic, JSValue *func_data)
{
    int i;

    for(i = 0; i < 2; i++) {
        if (!JS_IsUndefined(func_data[i]))
            return JS_ThrowTypeError(ctx, "resolving function already set");
        func_data[i] = JS_DupValue(ctx, argv[i]);
    }
    return JS_UNDEFINED;
}

JSValue js_promise_executor_new(JSContext *ctx)
{
    JSValueConst func_data[2];

    func_data[0] = JS_UNDEFINED;
    func_data[1] = JS_UNDEFINED;
    return JS_NewCFunctionData(ctx, js_promise_executor, 2,
                               0, 2, func_data);
}

JSValue js_new_promise_capability(JSContext *ctx,
                                         JSValue *resolving_funcs,
                                         JSValueConst ctor)
{
    JSValue executor, result_promise;
    JSCFunctionDataRecord *s;
    int i;

    executor = js_promise_executor_new(ctx);
    if (JS_IsException(executor))
        return executor;

    if (JS_IsUndefined(ctor)) {
        result_promise = js_promise_constructor(ctx, ctor, 1,
                                                (JSValueConst *)&executor);
    } else {
        result_promise = JS_CallConstructor(ctx, ctor, 1,
                                            (JSValueConst *)&executor);
    }
    if (JS_IsException(result_promise))
        goto fail;
    s = JS_GetOpaque(executor, JS_CLASS_C_FUNCTION_DATA);
    for(i = 0; i < 2; i++) {
        if (check_function(ctx, s->data[i]))
            goto fail;
    }
    for(i = 0; i < 2; i++)
        resolving_funcs[i] = JS_DupValue(ctx, s->data[i]);
    JS_FreeValue(ctx, executor);
    return result_promise;
 fail:
    JS_FreeValue(ctx, executor);
    JS_FreeValue(ctx, result_promise);
    return JS_EXCEPTION;
}

JSValue js_promise_resolve(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv, int magic)
{
    JSValue result_promise, resolving_funcs[2], ret;
    BOOL is_reject = magic;

    if (!JS_IsObject(this_val))
        return JS_ThrowTypeErrorNotAnObject(ctx);
    if (!is_reject && JS_GetOpaque(argv[0], JS_CLASS_PROMISE)) {
        JSValue ctor;
        BOOL is_same;
        ctor = JS_GetProperty(ctx, argv[0], JS_ATOM_constructor);
        if (JS_IsException(ctor))
            return ctor;
        is_same = js_same_value(ctx, ctor, this_val);
        JS_FreeValue(ctx, ctor);
        if (is_same)
            return JS_DupValue(ctx, argv[0]);
    }
    result_promise = js_new_promise_capability(ctx, resolving_funcs, this_val);
    if (JS_IsException(result_promise))
        return result_promise;
    ret = JS_Call(ctx, resolving_funcs[is_reject], JS_UNDEFINED, 1, argv);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    if (JS_IsException(ret)) {
        JS_FreeValue(ctx, result_promise);
        return ret;
    }
    JS_FreeValue(ctx, ret);
    return result_promise;
}

JSValue js_promise_withResolvers(JSContext *ctx,
                                        JSValueConst this_val,
                                        int argc, JSValueConst *argv)
{
    JSValue result_promise, resolving_funcs[2], obj;
    if (!JS_IsObject(this_val))
        return JS_ThrowTypeErrorNotAnObject(ctx);
    result_promise = js_new_promise_capability(ctx, resolving_funcs, this_val);
    if (JS_IsException(result_promise))
        return result_promise;
    obj = JS_NewObject(ctx);
    if (JS_IsException(obj))
        goto exception;
    if (JS_DefinePropertyValue(ctx, obj, JS_ATOM_promise, result_promise,
                               JS_PROP_C_W_E) < 0) {
        goto exception;
    }
    result_promise = JS_UNDEFINED;
    if (JS_DefinePropertyValue(ctx, obj, JS_ATOM_resolve, resolving_funcs[0],
                               JS_PROP_C_W_E) < 0) {
        goto exception;
    }
    resolving_funcs[0] = JS_UNDEFINED;
    if (JS_DefinePropertyValue(ctx, obj, JS_ATOM_reject, resolving_funcs[1],
                               JS_PROP_C_W_E) < 0) {
        goto exception;
    }
    return obj;
exception:
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, result_promise);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

JSValue js_promise_try(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv)
{
    JSValue result_promise, resolving_funcs[2], ret, ret2;
    BOOL is_reject = 0;

    if (!JS_IsObject(this_val))
        return JS_ThrowTypeErrorNotAnObject(ctx);
    result_promise = js_new_promise_capability(ctx, resolving_funcs, this_val);
    if (JS_IsException(result_promise))
        return result_promise;
    ret = JS_Call(ctx, argv[0], JS_UNDEFINED, argc - 1, argv + 1);
    if (JS_IsException(ret)) {
        is_reject = 1;
        ret = JS_GetException(ctx);
    }
    ret2 = JS_Call(ctx, resolving_funcs[is_reject], JS_UNDEFINED, 1, (JSValueConst *)&ret);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, ret);
    if (JS_IsException(ret2)) {
        JS_FreeValue(ctx, result_promise);
        return ret2;
    }
    JS_FreeValue(ctx, ret2);
    return result_promise;
}

__exception int remainingElementsCount_add(JSContext *ctx,
                                                  JSValueConst resolve_element_env,
                                                  int addend)
{
    JSValue val;
    int32_t remainingElementsCount;

    val = JS_GetPropertyUint32(ctx, resolve_element_env, 0);
    if (JS_IsException(val))
        return -1;
    if (JS_ToInt32Free(ctx, &remainingElementsCount, val))
        return -1;
    remainingElementsCount += addend;
    if (JS_SetPropertyUint32(ctx, resolve_element_env, 0,
                             JS_NewInt32(ctx, remainingElementsCount)) < 0)
        return -1;
    return (remainingElementsCount == 0);
}

#define PROMISE_MAGIC_all        0
#define PROMISE_MAGIC_allSettled 1
#define PROMISE_MAGIC_any        2

JSValue js_promise_all_resolve_element(JSContext *ctx,
                                              JSValueConst this_val,
                                              int argc, JSValueConst *argv,
                                              int magic,
                                              JSValue *func_data)
{
    int resolve_type = magic & 3;
    int is_reject = magic & 4;
    BOOL alreadyCalled = JS_ToBool(ctx, func_data[0]);
    JSValueConst values = func_data[2];
    JSValueConst resolve = func_data[3];
    JSValueConst resolve_element_env = func_data[4];
    JSValue ret, obj;
    int is_zero; int32_t index;

    if (JS_ToInt32(ctx, &index, func_data[1]))
        return JS_EXCEPTION;
    if (alreadyCalled)
        return JS_UNDEFINED;
    func_data[0] = JS_NewBool(ctx, TRUE);

    if (resolve_type == PROMISE_MAGIC_allSettled) {
        JSValue str;

        obj = JS_NewObject(ctx);
        if (JS_IsException(obj))
            return JS_EXCEPTION;
        str = js_new_string8(ctx, is_reject ? "rejected" : "fulfilled");
        if (JS_IsException(str))
            goto fail1;
        if (JS_DefinePropertyValue(ctx, obj, JS_ATOM_status,
                                   str,
                                   JS_PROP_C_W_E) < 0)
            goto fail1;
        if (JS_DefinePropertyValue(ctx, obj,
                                   is_reject ? JS_ATOM_reason : JS_ATOM_value,
                                   JS_DupValue(ctx, argv[0]),
                                   JS_PROP_C_W_E) < 0) {
        fail1:
            JS_FreeValue(ctx, obj);
            return JS_EXCEPTION;
        }
    } else {
        obj = JS_DupValue(ctx, argv[0]);
    }
    if (JS_DefinePropertyValueUint32(ctx, values, index,
                                     obj, JS_PROP_C_W_E) < 0)
        return JS_EXCEPTION;

    is_zero = remainingElementsCount_add(ctx, resolve_element_env, -1);
    if (is_zero < 0)
        return JS_EXCEPTION;
    if (is_zero) {
        if (resolve_type == PROMISE_MAGIC_any) {
            JSValue error;
            error = js_aggregate_error_constructor(ctx, values);
            if (JS_IsException(error))
                return JS_EXCEPTION;
            ret = JS_Call(ctx, resolve, JS_UNDEFINED, 1, (JSValueConst *)&error);
            JS_FreeValue(ctx, error);
        } else {
            ret = JS_Call(ctx, resolve, JS_UNDEFINED, 1, (JSValueConst *)&values);
        }
        if (JS_IsException(ret))
            return ret;
        JS_FreeValue(ctx, ret);
    }
    return JS_UNDEFINED;
}

/* magic = 0: Promise.all 1: Promise.allSettled */
JSValue js_promise_all(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv, int magic)
{
    JSValue result_promise, resolving_funcs[2], item, next_promise, ret;
    JSValue next_method = JS_UNDEFINED, values = JS_UNDEFINED;
    JSValue resolve_element_env = JS_UNDEFINED, resolve_element, reject_element;
    JSValue promise_resolve = JS_UNDEFINED, iter = JS_UNDEFINED;
    JSValueConst then_args[2], resolve_element_data[5];
    BOOL done;
    int index, is_zero, is_promise_any = (magic == PROMISE_MAGIC_any);

    if (!JS_IsObject(this_val))
        return JS_ThrowTypeErrorNotAnObject(ctx);
    result_promise = js_new_promise_capability(ctx, resolving_funcs, this_val);
    if (JS_IsException(result_promise))
        return result_promise;
    promise_resolve = JS_GetProperty(ctx, this_val, JS_ATOM_resolve);
    if (JS_IsException(promise_resolve) ||
        check_function(ctx, promise_resolve))
        goto fail_reject;
    iter = JS_GetIterator(ctx, argv[0], FALSE);
    if (JS_IsException(iter)) {
        JSValue error;
    fail_reject:
        error = JS_GetException(ctx);
        ret = JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1,
                       (JSValueConst *)&error);
        JS_FreeValue(ctx, error);
        if (JS_IsException(ret))
            goto fail;
        JS_FreeValue(ctx, ret);
    } else {
        next_method = JS_GetProperty(ctx, iter, JS_ATOM_next);
        if (JS_IsException(next_method))
            goto fail_reject;
        values = JS_NewArray(ctx);
        if (JS_IsException(values))
            goto fail_reject;
        resolve_element_env = JS_NewArray(ctx);
        if (JS_IsException(resolve_element_env))
            goto fail_reject;
        /* remainingElementsCount field */
        if (JS_DefinePropertyValueUint32(ctx, resolve_element_env, 0,
                                         JS_NewInt32(ctx, 1),
                                         JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE | JS_PROP_WRITABLE) < 0)
            goto fail_reject;

        index = 0;
        for(;;) {
            /* XXX: conformance: should close the iterator if error on 'done'
               access, but not on 'value' access */
            item = JS_IteratorNext(ctx, iter, next_method, 0, NULL, &done);
            if (JS_IsException(item))
                goto fail_reject;
            if (done)
                break;
            next_promise = JS_Call(ctx, promise_resolve,
                                   this_val, 1, (JSValueConst *)&item);
            JS_FreeValue(ctx, item);
            if (JS_IsException(next_promise)) {
            fail_reject1:
                JS_IteratorClose(ctx, iter, TRUE);
                goto fail_reject;
            }
            resolve_element_data[0] = JS_NewBool(ctx, FALSE);
            resolve_element_data[1] = (JSValueConst)JS_NewInt32(ctx, index);
            resolve_element_data[2] = values;
            resolve_element_data[3] = resolving_funcs[is_promise_any];
            resolve_element_data[4] = resolve_element_env;
            resolve_element =
                JS_NewCFunctionData(ctx, js_promise_all_resolve_element, 1,
                                    magic, 5, resolve_element_data);
            if (JS_IsException(resolve_element)) {
                JS_FreeValue(ctx, next_promise);
                goto fail_reject1;
            }

            if (magic == PROMISE_MAGIC_allSettled) {
                reject_element =
                    JS_NewCFunctionData(ctx, js_promise_all_resolve_element, 1,
                                        magic | 4, 5, resolve_element_data);
                if (JS_IsException(reject_element)) {
                    JS_FreeValue(ctx, next_promise);
                    goto fail_reject1;
                }
            } else if (magic == PROMISE_MAGIC_any) {
                if (JS_DefinePropertyValueUint32(ctx, values, index,
                                                 JS_UNDEFINED, JS_PROP_C_W_E) < 0)
                    goto fail_reject1;
                reject_element = resolve_element;
                resolve_element = JS_DupValue(ctx, resolving_funcs[0]);
            } else {
                reject_element = JS_DupValue(ctx, resolving_funcs[1]);
            }

            if (remainingElementsCount_add(ctx, resolve_element_env, 1) < 0) {
                JS_FreeValue(ctx, next_promise);
                JS_FreeValue(ctx, resolve_element);
                JS_FreeValue(ctx, reject_element);
                goto fail_reject1;
            }

            then_args[0] = resolve_element;
            then_args[1] = reject_element;
            ret = JS_InvokeFree(ctx, next_promise, JS_ATOM_then, 2, then_args);
            JS_FreeValue(ctx, resolve_element);
            JS_FreeValue(ctx, reject_element);
            if (check_exception_free(ctx, ret))
                goto fail_reject1;
            index++;
        }

        is_zero = remainingElementsCount_add(ctx, resolve_element_env, -1);
        if (is_zero < 0)
            goto fail_reject;
        if (is_zero) {
            if (magic == PROMISE_MAGIC_any) {
                JSValue error;
                error = js_aggregate_error_constructor(ctx, values);
                if (JS_IsException(error))
                    goto fail_reject;
                JS_FreeValue(ctx, values);
                values = error;
            }
            ret = JS_Call(ctx, resolving_funcs[is_promise_any], JS_UNDEFINED,
                          1, (JSValueConst *)&values);
            if (check_exception_free(ctx, ret))
                goto fail_reject;
        }
    }
 done:
    JS_FreeValue(ctx, promise_resolve);
    JS_FreeValue(ctx, resolve_element_env);
    JS_FreeValue(ctx, values);
    JS_FreeValue(ctx, next_method);
    JS_FreeValue(ctx, iter);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    return result_promise;
 fail:
    JS_FreeValue(ctx, result_promise);
    result_promise = JS_EXCEPTION;
    goto done;
}

JSValue js_promise_race(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv)
{
    JSValue result_promise, resolving_funcs[2], item, next_promise, ret;
    JSValue next_method = JS_UNDEFINED, iter = JS_UNDEFINED;
    JSValue promise_resolve = JS_UNDEFINED;
    BOOL done;

    if (!JS_IsObject(this_val))
        return JS_ThrowTypeErrorNotAnObject(ctx);
    result_promise = js_new_promise_capability(ctx, resolving_funcs, this_val);
    if (JS_IsException(result_promise))
        return result_promise;
    promise_resolve = JS_GetProperty(ctx, this_val, JS_ATOM_resolve);
    if (JS_IsException(promise_resolve) ||
        check_function(ctx, promise_resolve))
        goto fail_reject;
    iter = JS_GetIterator(ctx, argv[0], FALSE);
    if (JS_IsException(iter)) {
        JSValue error;
    fail_reject:
        error = JS_GetException(ctx);
        ret = JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1,
                       (JSValueConst *)&error);
        JS_FreeValue(ctx, error);
        if (JS_IsException(ret))
            goto fail;
        JS_FreeValue(ctx, ret);
    } else {
        next_method = JS_GetProperty(ctx, iter, JS_ATOM_next);
        if (JS_IsException(next_method))
            goto fail_reject;

        for(;;) {
            /* XXX: conformance: should close the iterator if error on 'done'
               access, but not on 'value' access */
            item = JS_IteratorNext(ctx, iter, next_method, 0, NULL, &done);
            if (JS_IsException(item))
                goto fail_reject;
            if (done)
                break;
            next_promise = JS_Call(ctx, promise_resolve,
                                   this_val, 1, (JSValueConst *)&item);
            JS_FreeValue(ctx, item);
            if (JS_IsException(next_promise)) {
            fail_reject1:
                JS_IteratorClose(ctx, iter, TRUE);
                goto fail_reject;
            }
            ret = JS_InvokeFree(ctx, next_promise, JS_ATOM_then, 2,
                                (JSValueConst *)resolving_funcs);
            if (check_exception_free(ctx, ret))
                goto fail_reject1;
        }
    }
 done:
    JS_FreeValue(ctx, promise_resolve);
    JS_FreeValue(ctx, next_method);
    JS_FreeValue(ctx, iter);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    return result_promise;
 fail:
    //JS_FreeValue(ctx, next_method); // why not???
    JS_FreeValue(ctx, result_promise);
    result_promise = JS_EXCEPTION;
    goto done;
}
