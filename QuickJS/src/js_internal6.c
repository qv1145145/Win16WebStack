#include "js_internal.h"



JSValue js_call_bound_function(JSContext *ctx, JSValueConst func_obj,
                                      JSValueConst this_obj,
                                      int argc, JSValueConst *argv, int flags)
{
    JSObject *p;
    JSBoundFunction *bf;
    JSValueConst *arg_buf, new_target;
    int arg_count, i;

    p = JS_VALUE_GET_OBJ(func_obj);
    bf = p->u.bound_function;
    arg_count = bf->argc + argc;
    if (js_check_stack_overflow(ctx->rt, sizeof(JSValue) * arg_count))
        return JS_ThrowStackOverflow(ctx);
    arg_buf = alloca(sizeof(JSValue) * arg_count);
    for(i = 0; i < bf->argc; i++) {
        arg_buf[i] = bf->argv[i];
    }
    for(i = 0; i < argc; i++) {
        arg_buf[bf->argc + i] = argv[i];
    }
    if (flags & JS_CALL_FLAG_CONSTRUCTOR) {
        new_target = this_obj;
        if (js_same_value(ctx, func_obj, new_target))
            new_target = bf->func_obj;
        return JS_CallConstructor2(ctx, bf->func_obj, new_target,
                                   arg_count, arg_buf);
    } else {
        return JS_Call(ctx, bf->func_obj, bf->this_val,
                       arg_count, arg_buf);
    }
}

JSValue JS_CallFree(JSContext *ctx, JSValue func_obj, JSValueConst this_obj,
                           int argc, JSValueConst *argv)
{
    JSValue res = JS_CallInternal(ctx, func_obj, this_obj, JS_UNDEFINED,
                                  argc, (JSValue *)argv, JS_CALL_FLAG_COPY_ARGV);
    JS_FreeValue(ctx, func_obj);
    return res;
}

/* warning: the refcount of the context is not incremented. Return
   NULL in case of exception (case of revoked proxy only) */
JSContext *JS_GetFunctionRealm(JSContext *ctx, JSValueConst func_obj)
{
    JSObject *p;
    JSContext *realm;

    if (JS_VALUE_GET_TAG(func_obj) != JS_TAG_OBJECT)
        return ctx;
    p = JS_VALUE_GET_OBJ(func_obj);
    switch(p->class_id) {
    case JS_CLASS_C_FUNCTION:
        realm = p->u.cfunc.realm;
        break;
    case JS_CLASS_BYTECODE_FUNCTION:
    case JS_CLASS_GENERATOR_FUNCTION:
    case JS_CLASS_ASYNC_FUNCTION:
    case JS_CLASS_ASYNC_GENERATOR_FUNCTION:
        {
            JSFunctionBytecode *b;
            b = p->u.func.function_bytecode;
            realm = b->realm;
        }
        break;
    case JS_CLASS_PROXY:
        {
            JSProxyData *s = p->u.opaque;
            if (!s)
                return ctx;
            if (s->is_revoked) {
                JS_ThrowTypeErrorRevokedProxy(ctx);
                return NULL;
            } else {
                realm = JS_GetFunctionRealm(ctx, s->target);
            }
        }
        break;
    case JS_CLASS_BOUND_FUNCTION:
        {
            JSBoundFunction *bf = p->u.bound_function;
            realm = JS_GetFunctionRealm(ctx, bf->func_obj);
        }
        break;
    default:
        realm = ctx;
        break;
    }
    return realm;
}

JSValue js_create_from_ctor(JSContext *ctx, JSValueConst ctor,
                                   int class_id)
{
    JSValue proto, obj;
    JSContext *realm;

    if (JS_IsUndefined(ctor)) {
        proto = JS_DupValue(ctx, ctx->class_proto[class_id]);
    } else {
        proto = JS_GetProperty(ctx, ctor, JS_ATOM_prototype);
        if (JS_IsException(proto))
            return proto;
        if (!JS_IsObject(proto)) {
            JS_FreeValue(ctx, proto);
            realm = JS_GetFunctionRealm(ctx, ctor);
            if (!realm)
                return JS_EXCEPTION;
            proto = JS_DupValue(ctx, realm->class_proto[class_id]);
        }
    }
    obj = JS_NewObjectProtoClass(ctx, proto, class_id);
    JS_FreeValue(ctx, proto);
    return obj;
}

/* argv[] is modified if (flags & JS_CALL_FLAG_COPY_ARGV) = 0. */
JSValue JS_CallConstructorInternal(JSContext *ctx,
                                          JSValueConst func_obj,
                                          JSValueConst new_target,
                                          int argc, JSValue *argv, int flags)
{
    JSObject *p;
    JSFunctionBytecode *b;

    if (js_poll_interrupts(ctx))
        return JS_EXCEPTION;
    flags |= JS_CALL_FLAG_CONSTRUCTOR;
    if (unlikely(JS_VALUE_GET_TAG(func_obj) != JS_TAG_OBJECT))
        goto not_a_function;
    p = JS_VALUE_GET_OBJ(func_obj);
    if (unlikely(!p->is_constructor))
        return JS_ThrowTypeErrorNotAConstructor(ctx, func_obj);
    if (unlikely(p->class_id != JS_CLASS_BYTECODE_FUNCTION)) {
        JSClassCall *call_func;
        call_func = ctx->rt->class_array[p->class_id].call;
        if (!call_func) {
        not_a_function:
            return JS_ThrowTypeError(ctx, "not a function");
        }
        return call_func(ctx, func_obj, new_target, argc,
                         (JSValueConst *)argv, flags);
    }

    b = p->u.func.function_bytecode;
    if (b->is_derived_class_constructor) {
        return JS_CallInternal(ctx, func_obj, JS_UNDEFINED, new_target, argc, argv, flags);
    } else {
        JSValue obj, ret;
        /* legacy constructor behavior */
        obj = js_create_from_ctor(ctx, new_target, JS_CLASS_OBJECT);
        if (JS_IsException(obj))
            return JS_EXCEPTION;
        ret = JS_CallInternal(ctx, func_obj, obj, new_target, argc, argv, flags);
        if (JS_VALUE_GET_TAG(ret) == JS_TAG_OBJECT ||
            JS_IsException(ret)) {
            JS_FreeValue(ctx, obj);
            return ret;
        } else {
            JS_FreeValue(ctx, ret);
            return obj;
        }
    }
}

JSValue JS_InvokeFree(JSContext *ctx, JSValue this_val, JSAtom atom,
                             int argc, JSValueConst *argv)
{
    JSValue res = JS_Invoke(ctx, this_val, atom, argc, argv);
    JS_FreeValue(ctx, this_val);
    return res;
}

/* JSAsyncFunctionState (used by generator and async functions) */
JSAsyncFunctionState *async_func_init(JSContext *ctx,
                                             JSValueConst func_obj, JSValueConst this_obj,
                                             int argc, JSValueConst *argv)
{
    JSAsyncFunctionState *s;
    JSObject *p;
    JSFunctionBytecode *b;
    JSStackFrame *sf;
    int i, arg_buf_len, n;

    p = JS_VALUE_GET_OBJ(func_obj);
    b = p->u.func.function_bytecode;
    arg_buf_len = max_int(b->arg_count, argc);
    s = js_malloc(ctx, sizeof(*s) + sizeof(JSValue) * (arg_buf_len + b->var_count + b->stack_size) + sizeof(JSVarRef *) * b->var_ref_count);
    if (!s)
        return NULL;
    memset(s, 0, sizeof(*s));
    js_rc(s)->ref_count = 1;
    add_gc_object(ctx->rt, &s->header, JS_GC_OBJ_TYPE_ASYNC_FUNCTION);

    sf = &s->frame;
    sf->js_mode = b->js_mode | JS_MODE_ASYNC;
    sf->cur_pc = b->byte_code_buf;
    sf->arg_buf = (JSValue *)(s + 1);
    sf->cur_func = JS_DupValue(ctx, func_obj);
    s->this_val = JS_DupValue(ctx, this_obj);
    s->argc = argc;
    sf->arg_count = arg_buf_len;
    sf->var_buf = sf->arg_buf + arg_buf_len;
    sf->cur_sp = sf->var_buf + b->var_count;
    sf->var_refs = (JSVarRef **)(sf->cur_sp + b->stack_size);
    for(i = 0; i < b->var_ref_count; i++)
        sf->var_refs[i] = NULL;
    for(i = 0; i < argc; i++)
        sf->arg_buf[i] = JS_DupValue(ctx, argv[i]);
    n = arg_buf_len + b->var_count;
    for(i = argc; i < n; i++)
        sf->arg_buf[i] = JS_UNDEFINED;
    s->resolving_funcs[0] = JS_UNDEFINED;
    s->resolving_funcs[1] = JS_UNDEFINED;
    s->is_completed = FALSE;
    return s;
}

void async_func_free_frame(JSRuntime *rt, JSAsyncFunctionState *s)
{
    JSStackFrame *sf = &s->frame;
    JSValue *sp;

    /* cannot free the function if it is running */
    assert(sf->cur_sp != NULL);
    for(sp = sf->arg_buf; sp < sf->cur_sp; sp++) {
        JS_FreeValueRT(rt, *sp);
    }
    JS_FreeValueRT(rt, sf->cur_func);
    JS_FreeValueRT(rt, s->this_val);
}

JSValue async_func_resume(JSContext *ctx, JSAsyncFunctionState *s)
{
    JSRuntime *rt = ctx->rt;
    JSStackFrame *sf = &s->frame;
    JSValue func_obj, ret;

    assert(!s->is_completed);
    if (js_check_stack_overflow(ctx->rt, 0)) {
        ret = JS_ThrowStackOverflow(ctx);
    } else {
        /* the tag does not matter provided it is not an object */
        func_obj = JS_MKPTR(JS_TAG_INT, s);
        ret = JS_CallInternal(ctx, func_obj, s->this_val, JS_UNDEFINED,
                              s->argc, sf->arg_buf, JS_CALL_FLAG_GENERATOR);
    }
    if (JS_IsException(ret) || JS_IsUndefined(ret)) {
        JSObject *p;
        JSFunctionBytecode *b;

        p = JS_VALUE_GET_OBJ(sf->cur_func);
        b = p->u.func.function_bytecode;

        if (JS_IsUndefined(ret)) {
            ret = sf->cur_sp[-1];
            sf->cur_sp[-1] = JS_UNDEFINED;
        }
        /* end of execution */
        s->is_completed = TRUE;

        /* close the closure variables. */
        close_var_refs(rt, b, sf);

        async_func_free_frame(rt, s);
    }
    return ret;
}

void __async_func_free(JSRuntime *rt, JSAsyncFunctionState *s)
{
    /* cannot close the closure variables here because it would
       potentially modify the object graph */
    if (!s->is_completed) {
        async_func_free_frame(rt, s);
    }

    JS_FreeValueRT(rt, s->resolving_funcs[0]);
    JS_FreeValueRT(rt, s->resolving_funcs[1]);

    remove_gc_object(&s->header);
    if (rt->gc_phase == JS_GC_PHASE_REMOVE_CYCLES && js_rc(s)->ref_count != 0) {
        list_add_tail(&s->header.link, &rt->gc_zero_ref_count_list);
    } else {
        js_free_rt(rt, s);
    }
}

void async_func_free(JSRuntime *rt, JSAsyncFunctionState *s)
{
    if (--js_rc(s)->ref_count == 0) {
        if (rt->gc_phase != JS_GC_PHASE_REMOVE_CYCLES) {
            list_del(&s->header.link);
            list_add(&s->header.link, &rt->gc_zero_ref_count_list);
            if (rt->gc_phase == JS_GC_PHASE_NONE) {
                free_zero_refcount(rt);
            }
        }
    }
}

/* Generators */

void free_generator_stack_rt(JSRuntime *rt, JSGeneratorData *s)
{
    if (s->state == JS_GENERATOR_STATE_COMPLETED)
        return;
    if (s->func_state) {
        async_func_free(rt, s->func_state);
        s->func_state = NULL;
    }
    s->state = JS_GENERATOR_STATE_COMPLETED;
}

void js_generator_finalizer(JSRuntime *rt, JSValue obj)
{
    JSGeneratorData *s = JS_GetOpaque(obj, JS_CLASS_GENERATOR);

    if (s) {
        free_generator_stack_rt(rt, s);
        js_free_rt(rt, s);
    }
}

void free_generator_stack(JSContext *ctx, JSGeneratorData *s)
{
    free_generator_stack_rt(ctx->rt, s);
}

void js_generator_mark(JSRuntime *rt, JSValueConst val,
                              JS_MarkFunc *mark_func)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JSGeneratorData *s = p->u.generator_data;

    if (!s || !s->func_state)
        return;
    mark_func(rt, &s->func_state->header);
}

JSValue js_generator_next(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv,
                                 BOOL *pdone, int magic)
{
    JSGeneratorData *s = JS_GetOpaque(this_val, JS_CLASS_GENERATOR);
    JSStackFrame *sf;
    JSValue ret, func_ret;

    *pdone = TRUE;
    if (!s)
        return JS_ThrowTypeError(ctx, "not a generator");
    switch(s->state) {
    default:
    case JS_GENERATOR_STATE_SUSPENDED_START:
        sf = &s->func_state->frame;
        if (magic == GEN_MAGIC_NEXT) {
            goto exec_no_arg;
        } else {
            free_generator_stack(ctx, s);
            goto done;
        }
        break;
    case JS_GENERATOR_STATE_SUSPENDED_YIELD_STAR:
    case JS_GENERATOR_STATE_SUSPENDED_YIELD:
        sf = &s->func_state->frame;
        /* cur_sp[-1] was set to JS_UNDEFINED in the previous call */
        ret = JS_DupValue(ctx, argv[0]);
        if (magic == GEN_MAGIC_THROW &&
            s->state == JS_GENERATOR_STATE_SUSPENDED_YIELD) {
            JS_Throw(ctx, ret);
            s->func_state->throw_flag = TRUE;
        } else {
            sf->cur_sp[-1] = ret;
            sf->cur_sp[0] = JS_NewInt32(ctx, magic);
            sf->cur_sp++;
        exec_no_arg:
            s->func_state->throw_flag = FALSE;
        }
        s->state = JS_GENERATOR_STATE_EXECUTING;
        func_ret = async_func_resume(ctx, s->func_state);
        s->state = JS_GENERATOR_STATE_SUSPENDED_YIELD;
        if (s->func_state->is_completed) {
            /* finalize the execution in case of exception or normal return */
            free_generator_stack(ctx, s);
            return func_ret;
        } else {
            assert(JS_VALUE_GET_TAG(func_ret) == JS_TAG_INT);
            /* get the returned yield value at the top of the stack */
            ret = sf->cur_sp[-1];
            sf->cur_sp[-1] = JS_UNDEFINED;
            if (JS_VALUE_GET_INT(func_ret) == FUNC_RET_YIELD_STAR) {
                s->state = JS_GENERATOR_STATE_SUSPENDED_YIELD_STAR;
                /* return (value, done) object */
                *pdone = 2;
            } else {
                *pdone = FALSE;
            }
        }
        break;
    case JS_GENERATOR_STATE_COMPLETED:
    done:
        /* execution is finished */
        switch(magic) {
        default:
        case GEN_MAGIC_NEXT:
            ret = JS_UNDEFINED;
            break;
        case GEN_MAGIC_RETURN:
            ret = JS_DupValue(ctx, argv[0]);
            break;
        case GEN_MAGIC_THROW:
            ret = JS_Throw(ctx, JS_DupValue(ctx, argv[0]));
            break;
        }
        break;
    case JS_GENERATOR_STATE_EXECUTING:
        ret = JS_ThrowTypeError(ctx, "cannot invoke a running generator");
        break;
    }
    return ret;
}

JSValue js_generator_function_call(JSContext *ctx, JSValueConst func_obj,
                                          JSValueConst this_obj,
                                          int argc, JSValueConst *argv,
                                          int flags)
{
    JSValue obj, func_ret;
    JSGeneratorData *s;

    s = js_mallocz(ctx, sizeof(*s));
    if (!s)
        return JS_EXCEPTION;
    s->state = JS_GENERATOR_STATE_SUSPENDED_START;
    s->func_state = async_func_init(ctx, func_obj, this_obj, argc, argv);
    if (!s->func_state) {
        s->state = JS_GENERATOR_STATE_COMPLETED;
        goto fail;
    }

    /* execute the function up to 'OP_initial_yield' */
    func_ret = async_func_resume(ctx, s->func_state);
    if (JS_IsException(func_ret))
        goto fail;
    JS_FreeValue(ctx, func_ret);

    obj = js_create_from_ctor(ctx, func_obj, JS_CLASS_GENERATOR);
    if (JS_IsException(obj))
        goto fail;
    JS_SetOpaque(obj, s);
    return obj;
 fail:
    free_generator_stack_rt(ctx->rt, s);
    js_free(ctx, s);
    return JS_EXCEPTION;
}

/* AsyncFunction */

void js_async_function_resolve_finalizer(JSRuntime *rt, JSValue val)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JSAsyncFunctionState *s = p->u.async_function_data;
    if (s) {
        async_func_free(rt, s);
    }
}

void js_async_function_resolve_mark(JSRuntime *rt, JSValueConst val,
                                           JS_MarkFunc *mark_func)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JSAsyncFunctionState *s = p->u.async_function_data;
    if (s) {
        mark_func(rt, &s->header);
    }
}

int js_async_function_resolve_create(JSContext *ctx,
                                            JSAsyncFunctionState *s,
                                            JSValue *resolving_funcs)
{
    int i;
    JSObject *p;

    for(i = 0; i < 2; i++) {
        resolving_funcs[i] =
            JS_NewObjectProtoClass(ctx, ctx->function_proto,
                                   JS_CLASS_ASYNC_FUNCTION_RESOLVE + i);
        if (JS_IsException(resolving_funcs[i])) {
            if (i == 1)
                JS_FreeValue(ctx, resolving_funcs[0]);
            return -1;
        }
        p = JS_VALUE_GET_OBJ(resolving_funcs[i]);
        js_rc(s)->ref_count++;
        p->u.async_function_data = s;
    }
    return 0;
}

void js_async_function_resume(JSContext *ctx, JSAsyncFunctionState *s)
{
    JSValue func_ret, ret2;

    func_ret = async_func_resume(ctx, s);
    if (s->is_completed) {
        if (JS_IsException(func_ret)) {
            JSValue error;
        fail:
            error = JS_GetException(ctx);
            ret2 = JS_Call(ctx, s->resolving_funcs[1], JS_UNDEFINED,
                           1, (JSValueConst *)&error);
            JS_FreeValue(ctx, error);
            JS_FreeValue(ctx, ret2); /* XXX: what to do if exception ? */
        } else {
            /* normal return */
            ret2 = JS_Call(ctx, s->resolving_funcs[0], JS_UNDEFINED,
                           1, (JSValueConst *)&func_ret);
            JS_FreeValue(ctx, func_ret);
            JS_FreeValue(ctx, ret2); /* XXX: what to do if exception ? */
        }
    } else {
        JSValue value, promise, resolving_funcs[2], resolving_funcs1[2];
        int i, res;

        value = s->frame.cur_sp[-1];
        s->frame.cur_sp[-1] = JS_UNDEFINED;

        /* await */
        JS_FreeValue(ctx, func_ret); /* not used */
        promise = js_promise_resolve(ctx, ctx->promise_ctor,
                                     1, (JSValueConst *)&value, 0);
        JS_FreeValue(ctx, value);
        if (JS_IsException(promise))
            goto fail;
        if (js_async_function_resolve_create(ctx, s, resolving_funcs)) {
            JS_FreeValue(ctx, promise);
            goto fail;
        }

        /* Note: no need to create 'thrownawayCapability' as in
           the spec */
        for(i = 0; i < 2; i++)
            resolving_funcs1[i] = JS_UNDEFINED;
        res = perform_promise_then(ctx, promise,
                                   (JSValueConst *)resolving_funcs,
                                   (JSValueConst *)resolving_funcs1);
        JS_FreeValue(ctx, promise);
        for(i = 0; i < 2; i++)
            JS_FreeValue(ctx, resolving_funcs[i]);
        if (res)
            goto fail;
    }
}

JSValue js_async_function_resolve_call(JSContext *ctx,
                                              JSValueConst func_obj,
                                              JSValueConst this_obj,
                                              int argc, JSValueConst *argv,
                                              int flags)
{
    JSObject *p = JS_VALUE_GET_OBJ(func_obj);
    JSAsyncFunctionState *s = p->u.async_function_data;
    BOOL is_reject = p->class_id - JS_CLASS_ASYNC_FUNCTION_RESOLVE;
    JSValueConst arg;

    if (argc > 0)
        arg = argv[0];
    else
        arg = JS_UNDEFINED;
    s->throw_flag = is_reject;
    if (is_reject) {
        JS_Throw(ctx, JS_DupValue(ctx, arg));
    } else {
        /* return value of await */
        s->frame.cur_sp[-1] = JS_DupValue(ctx, arg);
    }
    js_async_function_resume(ctx, s);
    return JS_UNDEFINED;
}

JSValue js_async_function_call(JSContext *ctx, JSValueConst func_obj,
                                      JSValueConst this_obj,
                                      int argc, JSValueConst *argv, int flags)
{
    JSValue promise;
    JSAsyncFunctionState *s;

    s = async_func_init(ctx, func_obj, this_obj, argc, argv);
    if (!s)
        return JS_EXCEPTION;

    promise = JS_NewPromiseCapability(ctx, s->resolving_funcs);
    if (JS_IsException(promise)) {
        async_func_free(ctx->rt, s);
        return JS_EXCEPTION;
    }

    js_async_function_resume(ctx, s);

    async_func_free(ctx->rt, s);

    return promise;
}

/* AsyncGenerator */

void js_async_generator_free(JSRuntime *rt,
                                    JSAsyncGeneratorData *s)
{
    struct list_head *el, *el1;
    JSAsyncGeneratorRequest *req;

    list_for_each_safe(el, el1, &s->queue) {
        req = list_entry(el, JSAsyncGeneratorRequest, link);
        JS_FreeValueRT(rt, req->result);
        JS_FreeValueRT(rt, req->promise);
        JS_FreeValueRT(rt, req->resolving_funcs[0]);
        JS_FreeValueRT(rt, req->resolving_funcs[1]);
        js_free_rt(rt, req);
    }
    if (s->func_state)
        async_func_free(rt, s->func_state);
    js_free_rt(rt, s);
}

void js_async_generator_finalizer(JSRuntime *rt, JSValue obj)
{
    JSAsyncGeneratorData *s = JS_GetOpaque(obj, JS_CLASS_ASYNC_GENERATOR);

    if (s) {
        js_async_generator_free(rt, s);
    }
}

void js_async_generator_mark(JSRuntime *rt, JSValueConst val,
                                    JS_MarkFunc *mark_func)
{
    JSAsyncGeneratorData *s = JS_GetOpaque(val, JS_CLASS_ASYNC_GENERATOR);
    struct list_head *el;
    JSAsyncGeneratorRequest *req;
    if (s) {
        list_for_each(el, &s->queue) {
            req = list_entry(el, JSAsyncGeneratorRequest, link);
            JS_MarkValue(rt, req->result, mark_func);
            JS_MarkValue(rt, req->promise, mark_func);
            JS_MarkValue(rt, req->resolving_funcs[0], mark_func);
            JS_MarkValue(rt, req->resolving_funcs[1], mark_func);
        }
        if (s->func_state) {
            mark_func(rt, &s->func_state->header);
        }
    }
}

JSValue js_async_generator_resolve_function(JSContext *ctx,
                                          JSValueConst this_obj,
                                          int argc, JSValueConst *argv,
                                          int magic, JSValue *func_data);

int js_async_generator_resolve_function_create(JSContext *ctx,
                                                      JSValueConst generator,
                                                      JSValue *resolving_funcs,
                                                      BOOL is_resume_next)
{
    int i;
    JSValue func;

    for(i = 0; i < 2; i++) {
        func = JS_NewCFunctionData(ctx, js_async_generator_resolve_function, 1,
                                   i + is_resume_next * 2, 1, &generator);
        if (JS_IsException(func)) {
            if (i == 1)
                JS_FreeValue(ctx, resolving_funcs[0]);
            return -1;
        }
        resolving_funcs[i] = func;
    }
    return 0;
}

int js_async_generator_await(JSContext *ctx,
                                    JSAsyncGeneratorData *s,
                                    JSValueConst value)
{
    JSValue promise, resolving_funcs[2], resolving_funcs1[2];
    int i, res;

    promise = js_promise_resolve(ctx, ctx->promise_ctor,
                                 1, &value, 0);
    if (JS_IsException(promise))
        goto fail;

    if (js_async_generator_resolve_function_create(ctx, JS_MKPTR(JS_TAG_OBJECT, s->generator),
                                                   resolving_funcs, FALSE)) {
        JS_FreeValue(ctx, promise);
        goto fail;
    }

    /* Note: no need to create 'thrownawayCapability' as in
       the spec */
    for(i = 0; i < 2; i++)
        resolving_funcs1[i] = JS_UNDEFINED;
    res = perform_promise_then(ctx, promise,
                               (JSValueConst *)resolving_funcs,
                               (JSValueConst *)resolving_funcs1);
    JS_FreeValue(ctx, promise);
    for(i = 0; i < 2; i++)
        JS_FreeValue(ctx, resolving_funcs[i]);
    if (res)
        goto fail;
    return 0;
 fail:
    return -1;
}

void js_async_generator_resolve_or_reject(JSContext *ctx,
                                                 JSAsyncGeneratorData *s,
                                                 JSValueConst result,
                                                 int is_reject)
{
    JSAsyncGeneratorRequest *next;
    JSValue ret;

    next = list_entry(s->queue.next, JSAsyncGeneratorRequest, link);
    list_del(&next->link);
    ret = JS_Call(ctx, next->resolving_funcs[is_reject], JS_UNDEFINED, 1,
                  &result);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, next->result);
    JS_FreeValue(ctx, next->promise);
    JS_FreeValue(ctx, next->resolving_funcs[0]);
    JS_FreeValue(ctx, next->resolving_funcs[1]);
    js_free(ctx, next);
}

void js_async_generator_resolve(JSContext *ctx,
                                       JSAsyncGeneratorData *s,
                                       JSValueConst value,
                                       BOOL done)
{
    JSValue result;
    result = js_create_iterator_result(ctx, JS_DupValue(ctx, value), done);
    /* XXX: better exception handling ? */
    js_async_generator_resolve_or_reject(ctx, s, result, 0);
    JS_FreeValue(ctx, result);
 }

void js_async_generator_reject(JSContext *ctx,
                                       JSAsyncGeneratorData *s,
                                       JSValueConst exception)
{
    js_async_generator_resolve_or_reject(ctx, s, exception, 1);
}

void js_async_generator_complete(JSContext *ctx,
                                        JSAsyncGeneratorData *s)
{
    if (s->state != JS_ASYNC_GENERATOR_STATE_COMPLETED) {
        s->state = JS_ASYNC_GENERATOR_STATE_COMPLETED;
        async_func_free(ctx->rt, s->func_state);
        s->func_state = NULL;
    }
}

int js_async_generator_completed_return(JSContext *ctx,
                                               JSAsyncGeneratorData *s,
                                               JSValueConst value)
{
    JSValue promise, resolving_funcs[2], resolving_funcs1[2];
    int res;

    // Can fail looking up JS_ATOM_constructor when is_reject==0.
    promise = js_promise_resolve(ctx, ctx->promise_ctor, 1, &value,
                                 /*is_reject*/0);
    // A poisoned .constructor property is observable and the resulting
    // exception should be delivered to the catch handler.
    if (JS_IsException(promise)) {
        JSValue err = JS_GetException(ctx);
        promise = js_promise_resolve(ctx, ctx->promise_ctor, 1, (JSValueConst *)&err,
                                     /*is_reject*/1);
        JS_FreeValue(ctx, err);
        if (JS_IsException(promise))
            return -1;
    }
    if (js_async_generator_resolve_function_create(ctx,
                                                   JS_MKPTR(JS_TAG_OBJECT, s->generator),
                                                   resolving_funcs1,
                                                   TRUE)) {
        JS_FreeValue(ctx, promise);
        return -1;
    }
    resolving_funcs[0] = JS_UNDEFINED;
    resolving_funcs[1] = JS_UNDEFINED;
    res = perform_promise_then(ctx, promise,
                               (JSValueConst *)resolving_funcs1,
                               (JSValueConst *)resolving_funcs);
    JS_FreeValue(ctx, resolving_funcs1[0]);
    JS_FreeValue(ctx, resolving_funcs1[1]);
    JS_FreeValue(ctx, promise);
    return res;
}

void js_async_generator_resume_next(JSContext *ctx,
                                           JSAsyncGeneratorData *s)
{
    JSAsyncGeneratorRequest *next;
    JSValue func_ret, value;

    for(;;) {
        if (list_empty(&s->queue))
            break;
        next = list_entry(s->queue.next, JSAsyncGeneratorRequest, link);
        switch(s->state) {
        case JS_ASYNC_GENERATOR_STATE_EXECUTING:
            /* only happens when restarting execution after await() */
            goto resume_exec;
        case JS_ASYNC_GENERATOR_STATE_AWAITING_RETURN:
            goto done;
        case JS_ASYNC_GENERATOR_STATE_SUSPENDED_START:
            if (next->completion_type == GEN_MAGIC_NEXT) {
                goto exec_no_arg;
            } else {
                js_async_generator_complete(ctx, s);
            }
            break;
        case JS_ASYNC_GENERATOR_STATE_COMPLETED:
            if (next->completion_type == GEN_MAGIC_NEXT) {
                js_async_generator_resolve(ctx, s, JS_UNDEFINED, TRUE);
            } else if (next->completion_type == GEN_MAGIC_RETURN) {
                s->state = JS_ASYNC_GENERATOR_STATE_AWAITING_RETURN;
                js_async_generator_completed_return(ctx, s, next->result);
            } else {
                js_async_generator_reject(ctx, s, next->result);
            }
            goto done;
        case JS_ASYNC_GENERATOR_STATE_SUSPENDED_YIELD:
        case JS_ASYNC_GENERATOR_STATE_SUSPENDED_YIELD_STAR:
            value = JS_DupValue(ctx, next->result);
            if (next->completion_type == GEN_MAGIC_THROW &&
                s->state == JS_ASYNC_GENERATOR_STATE_SUSPENDED_YIELD) {
                JS_Throw(ctx, value);
                s->func_state->throw_flag = TRUE;
            } else {
                /* 'yield' returns a value. 'yield *' also returns a value
                   in case the 'throw' method is called */
                s->func_state->frame.cur_sp[-1] = value;
                s->func_state->frame.cur_sp[0] =
                    JS_NewInt32(ctx, next->completion_type);
                s->func_state->frame.cur_sp++;
            exec_no_arg:
                s->func_state->throw_flag = FALSE;
            }
            s->state = JS_ASYNC_GENERATOR_STATE_EXECUTING;
        resume_exec:
            func_ret = async_func_resume(ctx, s->func_state);
            if (s->func_state->is_completed) {
                if (JS_IsException(func_ret)) {
                    value = JS_GetException(ctx);
                    js_async_generator_complete(ctx, s);
                    js_async_generator_reject(ctx, s, value);
                    JS_FreeValue(ctx, value);
                } else {
                    /* end of function */
                    js_async_generator_complete(ctx, s);
                    js_async_generator_resolve(ctx, s, func_ret, TRUE);
                    JS_FreeValue(ctx, func_ret);
                }
            } else {
                int func_ret_code, ret;
                assert(JS_VALUE_GET_TAG(func_ret) == JS_TAG_INT);
                func_ret_code = JS_VALUE_GET_INT(func_ret);
                value = s->func_state->frame.cur_sp[-1];
                s->func_state->frame.cur_sp[-1] = JS_UNDEFINED;
                switch(func_ret_code) {
                case FUNC_RET_YIELD:
                case FUNC_RET_YIELD_STAR:
                    if (func_ret_code == FUNC_RET_YIELD_STAR)
                        s->state = JS_ASYNC_GENERATOR_STATE_SUSPENDED_YIELD_STAR;
                    else
                        s->state = JS_ASYNC_GENERATOR_STATE_SUSPENDED_YIELD;
                    js_async_generator_resolve(ctx, s, value, FALSE);
                    JS_FreeValue(ctx, value);
                    break;
                case FUNC_RET_AWAIT:
                    ret = js_async_generator_await(ctx, s, value);
                    JS_FreeValue(ctx, value);
                    if (ret < 0) {
                        /* exception: throw it */
                        s->func_state->throw_flag = TRUE;
                        goto resume_exec;
                    }
                    goto done;
                default:
                    abort();
                }
            }
            break;
        default:
            abort();
        }
    }
 done: ;
}

JSValue js_async_generator_resolve_function(JSContext *ctx,
                                                   JSValueConst this_obj,
                                                   int argc, JSValueConst *argv,
                                                   int magic, JSValue *func_data)
{
    BOOL is_reject = magic & 1;
    JSAsyncGeneratorData *s = JS_GetOpaque(func_data[0], JS_CLASS_ASYNC_GENERATOR);
    JSValueConst arg = argv[0];

    /* XXX: what if s == NULL */

    if (magic >= 2) {
        /* resume next case in AWAITING_RETURN state */
        assert(s->state == JS_ASYNC_GENERATOR_STATE_AWAITING_RETURN ||
               s->state == JS_ASYNC_GENERATOR_STATE_COMPLETED);
        s->state = JS_ASYNC_GENERATOR_STATE_COMPLETED;
        if (is_reject) {
            js_async_generator_reject(ctx, s, arg);
        } else {
            js_async_generator_resolve(ctx, s, arg, TRUE);
        }
    } else if (s->state == JS_ASYNC_GENERATOR_STATE_EXECUTING) {
        /* restart function execution after await() */
        s->func_state->throw_flag = is_reject;
        if (is_reject) {
            JS_Throw(ctx, JS_DupValue(ctx, arg));
        } else {
            /* return value of await */
            s->func_state->frame.cur_sp[-1] = JS_DupValue(ctx, arg);
        }
        js_async_generator_resume_next(ctx, s);
    }
    return JS_UNDEFINED;
}

/* magic = GEN_MAGIC_x */
JSValue js_async_generator_next(JSContext *ctx, JSValueConst this_val,
                                       int argc, JSValueConst *argv,
                                       int magic)
{
    JSAsyncGeneratorData *s = JS_GetOpaque(this_val, JS_CLASS_ASYNC_GENERATOR);
    JSValue promise, resolving_funcs[2];
    JSAsyncGeneratorRequest *req;

    promise = JS_NewPromiseCapability(ctx, resolving_funcs);
    if (JS_IsException(promise))
        return JS_EXCEPTION;
    if (!s) {
        JSValue err, res2;
        JS_ThrowTypeError(ctx, "not an AsyncGenerator object");
        err = JS_GetException(ctx);
        res2 = JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED,
                       1, (JSValueConst *)&err);
        JS_FreeValue(ctx, err);
        JS_FreeValue(ctx, res2);
        JS_FreeValue(ctx, resolving_funcs[0]);
        JS_FreeValue(ctx, resolving_funcs[1]);
        return promise;
    }
    req = js_mallocz(ctx, sizeof(*req));
    if (!req)
        goto fail;
    req->completion_type = magic;
    req->result = JS_DupValue(ctx, argv[0]);
    req->promise = JS_DupValue(ctx, promise);
    req->resolving_funcs[0] = resolving_funcs[0];
    req->resolving_funcs[1] = resolving_funcs[1];
    list_add_tail(&req->link, &s->queue);
    if (s->state != JS_ASYNC_GENERATOR_STATE_EXECUTING) {
        js_async_generator_resume_next(ctx, s);
    }
    return promise;
 fail:
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, promise);
    return JS_EXCEPTION;
}

JSValue js_async_generator_function_call(JSContext *ctx, JSValueConst func_obj,
                                                JSValueConst this_obj,
                                                int argc, JSValueConst *argv,
                                                int flags)
{
    JSValue obj, func_ret;
    JSAsyncGeneratorData *s;

    s = js_mallocz(ctx, sizeof(*s));
    if (!s)
        return JS_EXCEPTION;
    s->state = JS_ASYNC_GENERATOR_STATE_SUSPENDED_START;
    init_list_head(&s->queue);
    s->func_state = async_func_init(ctx, func_obj, this_obj, argc, argv);
    if (!s->func_state)
        goto fail;
    /* execute the function up to 'OP_initial_yield' (no yield nor
       await are possible) */
    func_ret = async_func_resume(ctx, s->func_state);
    if (JS_IsException(func_ret))
        goto fail;
    JS_FreeValue(ctx, func_ret);

    obj = js_create_from_ctor(ctx, func_obj, JS_CLASS_ASYNC_GENERATOR);
    if (JS_IsException(obj))
        goto fail;
    s->generator = JS_VALUE_GET_OBJ(obj);
    JS_SetOpaque(obj, s);
    return obj;
 fail:
    js_async_generator_free(ctx->rt, s);
    return JS_EXCEPTION;
}

/* JS parser */

__exception int next_token(JSParseState *s);

void free_token(JSParseState *s, JSToken *token)
{
    switch(token->val) {
    case TOK_NUMBER:
        JS_FreeValue(s->ctx, token->u.num.val);
        break;
    case TOK_STRING:
    case TOK_TEMPLATE:
        JS_FreeValue(s->ctx, token->u.str.str);
        break;
    case TOK_REGEXP:
        JS_FreeValue(s->ctx, token->u.regexp.body);
        JS_FreeValue(s->ctx, token->u.regexp.flags);
        break;
    case TOK_IDENT:
    case TOK_PRIVATE_NAME:
        JS_FreeAtom(s->ctx, token->u.ident.atom);
        break;
    default:
        if (token->val >= TOK_FIRST_KEYWORD &&
            token->val <= TOK_LAST_KEYWORD) {
            JS_FreeAtom(s->ctx, token->u.ident.atom);
        }
        break;
    }
}

void __attribute__((unused)) dump_token(JSParseState *s,
                                             const JSToken *token)
{
    switch(token->val) {
    case TOK_NUMBER:
        {
            double d;
            JS_ToFloat64(s->ctx, &d, token->u.num.val);  /* no exception possible */
            printf("number: %.14g\n", d);
        }
        break;
    case TOK_IDENT:
    dump_atom:
        {
            char buf[ATOM_GET_STR_BUF_SIZE];
            printf("ident: '%s'\n",
                   JS_AtomGetStr(s->ctx, buf, sizeof(buf), token->u.ident.atom));
        }
        break;
    case TOK_STRING:
        {
            const char *str;
            /* XXX: quote the string */
            str = JS_ToCString(s->ctx, token->u.str.str);
            printf("string: '%s'\n", str);
            JS_FreeCString(s->ctx, str);
        }
        break;
    case TOK_TEMPLATE:
        {
            const char *str;
            str = JS_ToCString(s->ctx, token->u.str.str);
            printf("template: `%s`\n", str);
            JS_FreeCString(s->ctx, str);
        }
        break;
    case TOK_REGEXP:
        {
            const char *str, *str2;
            str = JS_ToCString(s->ctx, token->u.regexp.body);
            str2 = JS_ToCString(s->ctx, token->u.regexp.flags);
            printf("regexp: '%s' '%s'\n", str, str2);
            JS_FreeCString(s->ctx, str);
            JS_FreeCString(s->ctx, str2);
        }
        break;
    case TOK_EOF:
        printf("eof\n");
        break;
    default:
        if (s->token.val >= TOK_NULL && s->token.val <= TOK_LAST_KEYWORD) {
            goto dump_atom;
        } else if (s->token.val >= 256) {
            printf("token: %d\n", token->val);
        } else {
            printf("token: '%c'\n", token->val);
        }
        break;
    }
}

/* return the zero based line and column number in the source. */
/* Note: we no longer support '\r' as line terminator */
int get_line_col(int *pcol_num, const uint8_t *buf, size_t len)
{
    int line_num, col_num, c;
    size_t i;

    line_num = 0;
    col_num = 0;
    for(i = 0; i < len; i++) {
        c = buf[i];
        if (c == '\n') {
            line_num++;
            col_num = 0;
        } else if (c < 0x80 || c >= 0xc0) {
            col_num++;
        }
    }
    *pcol_num = col_num;
    return line_num;
}

int get_line_col_cached(GetLineColCache *s, int *pcol_num, const uint8_t *ptr)
{
    int line_num, col_num;
    if (ptr >= s->ptr) {
        line_num = get_line_col(&col_num, s->ptr, ptr - s->ptr);
        if (line_num == 0) {
            s->col_num += col_num;
        } else {
            s->line_num += line_num;
            s->col_num = col_num;
        }
    } else {
        line_num = get_line_col(&col_num, ptr, s->ptr - ptr);
        if (line_num == 0) {
            s->col_num -= col_num;
        } else {
            const uint8_t *p;
            s->line_num -= line_num;
            /* find the absolute column position */
            col_num = 0;
            for(p = ptr - 1; p >= s->buf_start; p--) {
                if (*p == '\n') {
                    break;
                } else if (*p < 0x80 || *p >= 0xc0) {
                    col_num++;
                }
            }
            s->col_num = col_num;
        }
    }
    s->ptr = ptr;
    *pcol_num = s->col_num;
    return s->line_num;
}

/* 'ptr' is the position of the error in the source */
int js_parse_error_v(JSParseState *s, const uint8_t *ptr, const char *fmt, va_list ap)
{
    JSContext *ctx = s->ctx;
    int line_num, col_num;
    line_num = get_line_col(&col_num, s->buf_start, ptr - s->buf_start);
    JS_ThrowError2(ctx, JS_SYNTAX_ERROR, fmt, ap, FALSE);
    build_backtrace(ctx, ctx->rt->current_exception, s->filename,
                    line_num + 1, col_num + 1, 0);
    return -1;
}

__attribute__((format(printf, 3, 4))) int js_parse_error_pos(JSParseState *s, const uint8_t *ptr, const char *fmt, ...)
{
    va_list ap;
    int ret;

    va_start(ap, fmt);
    ret = js_parse_error_v(s, ptr, fmt, ap);
    va_end(ap);
    return ret;
}

__attribute__((format(printf, 2, 3))) int js_parse_error(JSParseState *s, const char *fmt, ...)
{
    va_list ap;
    int ret;

    va_start(ap, fmt);
    ret = js_parse_error_v(s, s->token.ptr, fmt, ap);
    va_end(ap);
    return ret;
}

int js_parse_expect(JSParseState *s, int tok)
{
    if (s->token.val != tok) {
        /* XXX: dump token correctly in all cases */
        return js_parse_error(s, "expecting '%c'", tok);
    }
    return next_token(s);
}

int js_parse_expect_semi(JSParseState *s)
{
    if (s->token.val != ';') {
        /* automatic insertion of ';' */
        if (s->token.val == TOK_EOF || s->token.val == '}' || s->got_lf) {
            return 0;
        }
        return js_parse_error(s, "expecting '%c'", ';');
    }
    return next_token(s);
}

int js_parse_error_reserved_identifier(JSParseState *s)
{
    char buf1[ATOM_GET_STR_BUF_SIZE];
    return js_parse_error(s, "'%s' is a reserved identifier",
                          JS_AtomGetStr(s->ctx, buf1, sizeof(buf1),
                                        s->token.u.ident.atom));
}

__exception int js_parse_template_part(JSParseState *s, const uint8_t *p)
{
    uint32_t c;
    StringBuffer b_s, *b = &b_s;
    JSValue str;

    /* p points to the first byte of the template part */
    if (string_buffer_init(s->ctx, b, 32))
        goto fail;
    for(;;) {
        if (p >= s->buf_end)
            goto unexpected_eof;
        c = *p++;
        if (c == '`') {
            /* template end part */
            break;
        }
        if (c == '$' && *p == '{') {
            /* template start or middle part */
            p++;
            break;
        }
        if (c == '\\') {
            if (string_buffer_putc8(b, c))
                goto fail;
            if (p >= s->buf_end)
                goto unexpected_eof;
            c = *p++;
        }
        /* newline sequences are normalized as single '\n' bytes */
        if (c == '\r') {
            if (*p == '\n')
                p++;
            c = '\n';
        }
        if (c >= 0x80) {
            const uint8_t *p_next;
            c = unicode_from_utf8(p - 1, UTF8_CHAR_LEN_MAX, &p_next);
            if (c > 0x10FFFF) {
                js_parse_error_pos(s, p - 1, "invalid UTF-8 sequence");
                goto fail;
            }
            p = p_next;
        }
        if (string_buffer_putc(b, c))
            goto fail;
    }
    str = string_buffer_end(b);
    if (JS_IsException(str))
        return -1;
    s->token.val = TOK_TEMPLATE;
    s->token.u.str.sep = c;
    s->token.u.str.str = str;
    s->buf_ptr = p;
    return 0;

 unexpected_eof:
    js_parse_error(s, "unexpected end of string");
 fail:
    string_buffer_free(b);
    return -1;
}

__exception int js_parse_string(JSParseState *s, int sep,
                                       BOOL do_throw, const uint8_t *p,
                                       JSToken *token, const uint8_t **pp)
{
    int ret;
    uint32_t c;
    StringBuffer b_s, *b = &b_s;
    const uint8_t *p_escape;
    JSValue str;

    /* string */
    if (string_buffer_init(s->ctx, b, 32))
        goto fail;
    for(;;) {
        if (p >= s->buf_end)
            goto invalid_char;
        c = *p;
        if (c < 0x20) {
            if (sep == '`') {
                if (c == '\r') {
                    if (p[1] == '\n')
                        p++;
                    c = '\n';
                }
                /* do not update s->line_num */
            } else if (c == '\n' || c == '\r')
                goto invalid_char;
        }
        p++;
        if (c == sep)
            break;
        if (c == '$' && *p == '{' && sep == '`') {
            /* template start or middle part */
            p++;
            break;
        }
        if (c == '\\') {
            p_escape = p - 1;
            c = *p;
            /* XXX: need a specific JSON case to avoid
               accepting invalid escapes */
            switch(c) {
            case '\0':
                if (p >= s->buf_end)
                    goto invalid_char;
                p++;
                break;
            case '\'':
            case '\"':
            case '\\':
                p++;
                break;
            case '\r':  /* accept DOS and MAC newline sequences */
                if (p[1] == '\n') {
                    p++;
                }
                /* fall thru */
            case '\n':
                /* ignore escaped newline sequence */
                p++;
                continue;
            default:
                if (c >= '0' && c <= '9') {
                    if (!(s->cur_func->js_mode & JS_MODE_STRICT) && sep != '`')
                        goto parse_escape;
                    if (c == '0' && !(p[1] >= '0' && p[1] <= '9')) {
                        p++;
                        c = '\0';
                    } else {
                        if (c >= '8' || sep == '`') {
                            /* Note: according to ES2021, \8 and \9 are not
                               accepted in strict mode or in templates. */
                            goto invalid_escape;
                        } else {
                            if (do_throw)
                                js_parse_error_pos(s, p_escape, "octal escape sequences are not allowed in strict mode");
                        }
                        goto fail;
                    }
                } else if (c >= 0x80) {
                    const uint8_t *p_next;
                    c = unicode_from_utf8(p, UTF8_CHAR_LEN_MAX, &p_next);
                    if (c > 0x10FFFF) {
                        goto invalid_utf8;
                    }
                    p = p_next;
                    /* LS or PS are skipped */
                    if (c == CP_LS || c == CP_PS)
                        continue;
                } else {
                parse_escape:
                    ret = lre_parse_escape(&p, TRUE);
                    if (ret == -1) {
                    invalid_escape:
                        if (do_throw)
                            js_parse_error_pos(s, p_escape, "malformed escape sequence in string literal");
                        goto fail;
                    } else if (ret < 0) {
                        /* ignore the '\' (could output a warning) */
                        p++;
                    } else {
                        c = ret;
                    }
                }
                break;
            }
        } else if (c >= 0x80) {
            const uint8_t *p_next;
            c = unicode_from_utf8(p - 1, UTF8_CHAR_LEN_MAX, &p_next);
            if (c > 0x10FFFF)
                goto invalid_utf8;
            p = p_next;
        }
        if (string_buffer_putc(b, c))
            goto fail;
    }
    str = string_buffer_end(b);
    if (JS_IsException(str))
        return -1;
    token->val = TOK_STRING;
    token->u.str.sep = c;
    token->u.str.str = str;
    *pp = p;
    return 0;

 invalid_utf8:
    if (do_throw)
        js_parse_error(s, "invalid UTF-8 sequence");
    goto fail;
 invalid_char:
    if (do_throw)
        js_parse_error(s, "unexpected end of string");
 fail:
    string_buffer_free(b);
    return -1;
}

__exception int js_parse_regexp(JSParseState *s)
{
    const uint8_t *p;
    BOOL in_class;
    StringBuffer b_s, *b = &b_s;
    StringBuffer b2_s, *b2 = &b2_s;
    uint32_t c;
    JSValue body_str, flags_str;

    p = s->buf_ptr;
    p++;
    in_class = FALSE;
    if (string_buffer_init(s->ctx, b, 32))
        return -1;
    if (string_buffer_init(s->ctx, b2, 1))
        goto fail;
    for(;;) {
        if (p >= s->buf_end) {
        eof_error:
            js_parse_error(s, "unexpected end of regexp");
            goto fail;
        }
        c = *p++;
        if (c == '\n' || c == '\r') {
            goto eol_error;
        } else if (c == '/') {
            if (!in_class)
                break;
        } else if (c == '[') {
            in_class = TRUE;
        } else if (c == ']') {
            /* XXX: incorrect as the first character in a class */
            in_class = FALSE;
        } else if (c == '\\') {
            if (string_buffer_putc8(b, c))
                goto fail;
            c = *p++;
            if (c == '\n' || c == '\r')
                goto eol_error;
            else if (c == '\0' && p >= s->buf_end)
                goto eof_error;
            else if (c >= 0x80) {
                const uint8_t *p_next;
                c = unicode_from_utf8(p - 1, UTF8_CHAR_LEN_MAX, &p_next);
                if (c > 0x10FFFF) {
                    goto invalid_utf8;
                }
                p = p_next;
                if (c == CP_LS || c == CP_PS)
                    goto eol_error;
            }
        } else if (c >= 0x80) {
            const uint8_t *p_next;
            c = unicode_from_utf8(p - 1, UTF8_CHAR_LEN_MAX, &p_next);
            if (c > 0x10FFFF) {
            invalid_utf8:
                js_parse_error_pos(s, p - 1, "invalid UTF-8 sequence");
                goto fail;
            }
            /* LS or PS are considered as line terminator */
            if (c == CP_LS || c == CP_PS) {
            eol_error:
                js_parse_error_pos(s, p - 1, "unexpected line terminator in regexp");
                goto fail;
            }
            p = p_next;
        }
        if (string_buffer_putc(b, c))
            goto fail;
    }

    /* flags */
    for(;;) {
        const uint8_t *p_next = p;
        c = *p_next++;
        if (c >= 0x80) {
            c = unicode_from_utf8(p, UTF8_CHAR_LEN_MAX, &p_next);
            if (c > 0x10FFFF) {
                p++;
                goto invalid_utf8;
            }
        }
        if (!lre_js_is_ident_next(c))
            break;
        if (string_buffer_putc(b2, c))
            goto fail;
        p = p_next;
    }

    body_str = string_buffer_end(b);
    flags_str = string_buffer_end(b2);
    if (JS_IsException(body_str) ||
        JS_IsException(flags_str)) {
        JS_FreeValue(s->ctx, body_str);
        JS_FreeValue(s->ctx, flags_str);
        return -1;
    }
    s->token.val = TOK_REGEXP;
    s->token.u.regexp.body = body_str;
    s->token.u.regexp.flags = flags_str;
    s->buf_ptr = p;
    return 0;
 fail:
    string_buffer_free(b);
    string_buffer_free(b2);
    return -1;
}

__exception int ident_realloc(JSContext *ctx, char **pbuf, size_t *psize,
                                     char *static_buf)
{
    char *buf, *new_buf;
    size_t size, new_size;

    buf = *pbuf;
    size = *psize;
    if (size >= (SIZE_MAX / 3) * 2)
        new_size = SIZE_MAX;
    else
        new_size = size + (size >> 1);
    if (buf == static_buf) {
        new_buf = js_malloc(ctx, new_size);
        if (!new_buf)
            return -1;
        memcpy(new_buf, buf, size);
    } else {
        new_buf = js_realloc(ctx, buf, new_size);
        if (!new_buf)
            return -1;
    }
    *pbuf = new_buf;
    *psize = new_size;
    return 0;
}

/* convert a TOK_IDENT to a keyword when needed */
void update_token_ident(JSParseState *s)
{
    if (s->token.u.ident.atom <= JS_ATOM_LAST_KEYWORD ||
        (s->token.u.ident.atom <= JS_ATOM_LAST_STRICT_KEYWORD &&
         (s->cur_func->js_mode & JS_MODE_STRICT)) ||
        (s->token.u.ident.atom == JS_ATOM_yield &&
         ((s->cur_func->func_kind & JS_FUNC_GENERATOR) ||
          (s->cur_func->func_type == JS_PARSE_FUNC_ARROW &&
           !s->cur_func->in_function_body && s->cur_func->parent &&
           (s->cur_func->parent->func_kind & JS_FUNC_GENERATOR)))) ||
        (s->token.u.ident.atom == JS_ATOM_await &&
         (s->is_module ||
          (s->cur_func->func_kind & JS_FUNC_ASYNC) ||
          s->cur_func->func_type == JS_PARSE_FUNC_CLASS_STATIC_INIT ||
          (s->cur_func->func_type == JS_PARSE_FUNC_ARROW &&
           !s->cur_func->in_function_body && s->cur_func->parent &&
           ((s->cur_func->parent->func_kind & JS_FUNC_ASYNC) ||
            s->cur_func->parent->func_type == JS_PARSE_FUNC_CLASS_STATIC_INIT))))) {
        if (s->token.u.ident.has_escape) {
            s->token.u.ident.is_reserved = TRUE;
            s->token.val = TOK_IDENT;
        } else {
            /* The keywords atoms are pre allocated */
            s->token.val = s->token.u.ident.atom - 1 + TOK_FIRST_KEYWORD;
        }
    }
}

/* if the current token is an identifier or keyword, reparse it
   according to the current function type */
void reparse_ident_token(JSParseState *s)
{
    if (s->token.val == TOK_IDENT ||
        (s->token.val >= TOK_FIRST_KEYWORD &&
         s->token.val <= TOK_LAST_KEYWORD)) {
        s->token.val = TOK_IDENT;
        s->token.u.ident.is_reserved = FALSE;
        update_token_ident(s);
    }
}

/* 'c' is the first character. Return JS_ATOM_NULL in case of error */
JSAtom parse_ident(JSParseState *s, const uint8_t **pp,
                          BOOL *pident_has_escape, int c, BOOL is_private)
{
    const uint8_t *p, *p1;
    char ident_buf[128], *buf;
    size_t ident_size, ident_pos;
    JSAtom atom;

    p = *pp;
    buf = ident_buf;
    ident_size = sizeof(ident_buf);
    ident_pos = 0;
    if (is_private)
        buf[ident_pos++] = '#';
    for(;;) {
        p1 = p;

        if (c < 128) {
            buf[ident_pos++] = c;
        } else {
            ident_pos += unicode_to_utf8((uint8_t*)buf + ident_pos, c);
        }
        c = *p1++;
        if (c == '\\' && *p1 == 'u') {
            c = lre_parse_escape(&p1, TRUE);
            *pident_has_escape = TRUE;
        } else if (c >= 128) {
            c = unicode_from_utf8(p, UTF8_CHAR_LEN_MAX, &p1);
        }
        if (!lre_js_is_ident_next(c))
            break;
        p = p1;
        if (unlikely(ident_pos >= ident_size - UTF8_CHAR_LEN_MAX)) {
            if (ident_realloc(s->ctx, &buf, &ident_size, ident_buf)) {
                atom = JS_ATOM_NULL;
                goto done;
            }
        }
    }
    atom = JS_NewAtomLen(s->ctx, buf, ident_pos);
 done:
    if (unlikely(buf != ident_buf))
        js_free(s->ctx, buf);
    *pp = p;
    return atom;
}


__exception int next_token(JSParseState *s)
{
    const uint8_t *p;
    int c;
    BOOL ident_has_escape;
    JSAtom atom;

    if (js_check_stack_overflow(s->ctx->rt, 0)) {
        return js_parse_error(s, "stack overflow");
    }

    free_token(s, &s->token);

    p = s->last_ptr = s->buf_ptr;
    s->got_lf = FALSE;
 redo:
    s->token.ptr = p;
    c = *p;
    switch(c) {
    case 0:
        if (p >= s->buf_end) {
            s->token.val = TOK_EOF;
        } else {
            goto def_token;
        }
        break;
    case '`':
        if (js_parse_template_part(s, p + 1))
            goto fail;
        p = s->buf_ptr;
        break;
    case '\'':
    case '\"':
        if (js_parse_string(s, c, TRUE, p + 1, &s->token, &p))
            goto fail;
        break;
    case '\r':  /* accept DOS and MAC newline sequences */
        if (p[1] == '\n') {
            p++;
        }
        /* fall thru */
    case '\n':
        p++;
    line_terminator:
        s->got_lf = TRUE;
        goto redo;
    case '\f':
    case '\v':
    case ' ':
    case '\t':
        p++;
        goto redo;
    case '/':
        if (p[1] == '*') {
            /* comment */
            p += 2;
            for(;;) {
                if (*p == '\0' && p >= s->buf_end) {
                    js_parse_error(s, "unexpected end of comment");
                    goto fail;
                }
                if (p[0] == '*' && p[1] == '/') {
                    p += 2;
                    break;
                }
                if (*p == '\n' || *p == '\r') {
                    s->got_lf = TRUE; /* considered as LF for ASI */
                    p++;
                } else if (*p >= 0x80) {
                    c = unicode_from_utf8(p, UTF8_CHAR_LEN_MAX, &p);
                    if (c == CP_LS || c == CP_PS) {
                        s->got_lf = TRUE; /* considered as LF for ASI */
                    } else if (c == -1) {
                        p++; /* skip invalid UTF-8 */
                    }
                } else {
                    p++;
                }
            }
            goto redo;
        } else if (p[1] == '/') {
            /* line comment */
            p += 2;
        skip_line_comment:
            for(;;) {
                if (*p == '\0' && p >= s->buf_end)
                    break;
                if (*p == '\r' || *p == '\n')
                    break;
                if (*p >= 0x80) {
                    c = unicode_from_utf8(p, UTF8_CHAR_LEN_MAX, &p);
                    /* LS or PS are considered as line terminator */
                    if (c == CP_LS || c == CP_PS) {
                        break;
                    } else if (c == -1) {
                        p++; /* skip invalid UTF-8 */
                    }
                } else {
                    p++;
                }
            }
            goto redo;
        } else if (p[1] == '=') {
            p += 2;
            s->token.val = TOK_DIV_ASSIGN;
        } else {
            p++;
            s->token.val = c;
        }
        break;
    case '\\':
        if (p[1] == 'u') {
            const uint8_t *p1 = p + 1;
            int c1 = lre_parse_escape(&p1, TRUE);
            if (c1 >= 0 && lre_js_is_ident_first(c1)) {
                c = c1;
                p = p1;
                ident_has_escape = TRUE;
                goto has_ident;
            } else {
                /* XXX: syntax error? */
            }
        }
        goto def_token;
    case 'a': case 'b': case 'c': case 'd':
    case 'e': case 'f': case 'g': case 'h':
    case 'i': case 'j': case 'k': case 'l':
    case 'm': case 'n': case 'o': case 'p':
    case 'q': case 'r': case 's': case 't':
    case 'u': case 'v': case 'w': case 'x':
    case 'y': case 'z':
    case 'A': case 'B': case 'C': case 'D':
    case 'E': case 'F': case 'G': case 'H':
    case 'I': case 'J': case 'K': case 'L':
    case 'M': case 'N': case 'O': case 'P':
    case 'Q': case 'R': case 'S': case 'T':
    case 'U': case 'V': case 'W': case 'X':
    case 'Y': case 'Z':
    case '_':
    case '$':
        /* identifier */
        p++;
        ident_has_escape = FALSE;
    has_ident:
        atom = parse_ident(s, &p, &ident_has_escape, c, FALSE);
        if (atom == JS_ATOM_NULL)
            goto fail;
        s->token.u.ident.atom = atom;
        s->token.u.ident.has_escape = ident_has_escape;
        s->token.u.ident.is_reserved = FALSE;
        s->token.val = TOK_IDENT;
        update_token_ident(s);
        break;
    case '#':
        /* private name */
        {
            const uint8_t *p1;
            p++;
            p1 = p;
            c = *p1++;
            if (c == '\\' && *p1 == 'u') {
                c = lre_parse_escape(&p1, TRUE);
            } else if (c >= 128) {
                c = unicode_from_utf8(p, UTF8_CHAR_LEN_MAX, &p1);
            }
            if (!lre_js_is_ident_first(c)) {
                js_parse_error(s, "invalid first character of private name");
                goto fail;
            }
            p = p1;
            ident_has_escape = FALSE; /* not used */
            atom = parse_ident(s, &p, &ident_has_escape, c, TRUE);
            if (atom == JS_ATOM_NULL)
                goto fail;
            s->token.u.ident.atom = atom;
            s->token.val = TOK_PRIVATE_NAME;
        }
        break;
    case '.':
        if (p[1] == '.' && p[2] == '.') {
            p += 3;
            s->token.val = TOK_ELLIPSIS;
            break;
        }
        if (p[1] >= '0' && p[1] <= '9') {
            goto parse_number;
        } else {
            goto def_token;
        }
        break;
    case '0':
        /* in strict mode, octal literals are not accepted */
        if (is_digit(p[1]) && (s->cur_func->js_mode & JS_MODE_STRICT)) {
            js_parse_error(s, "octal literals are deprecated in strict mode");
            goto fail;
        }
        goto parse_number;
    case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8':
    case '9':
        /* number */
    parse_number:
        {
            JSValue ret;
            const uint8_t *p1;
            int flags;
            flags = ATOD_ACCEPT_BIN_OCT | ATOD_ACCEPT_LEGACY_OCTAL |
                ATOD_ACCEPT_UNDERSCORES | ATOD_ACCEPT_SUFFIX;
            ret = js_atof(s->ctx, (const char *)p, (const char **)&p, 0,
                          flags);
            if (JS_IsException(ret))
                goto fail;
            /* reject `10instanceof Number` */
            if (JS_VALUE_IS_NAN(ret) ||
                lre_js_is_ident_next(unicode_from_utf8(p, UTF8_CHAR_LEN_MAX, &p1))) {
                JS_FreeValue(s->ctx, ret);
                js_parse_error(s, "invalid number literal");
                goto fail;
            }
            s->token.val = TOK_NUMBER;
            s->token.u.num.val = ret;
        }
        break;
    case '*':
        if (p[1] == '=') {
            p += 2;
            s->token.val = TOK_MUL_ASSIGN;
        } else if (p[1] == '*') {
            if (p[2] == '=') {
                p += 3;
                s->token.val = TOK_POW_ASSIGN;
            } else {
                p += 2;
                s->token.val = TOK_POW;
            }
        } else {
            goto def_token;
        }
        break;
    case '%':
        if (p[1] == '=') {
            p += 2;
            s->token.val = TOK_MOD_ASSIGN;
        } else {
            goto def_token;
        }
        break;
    case '+':
        if (p[1] == '=') {
            p += 2;
            s->token.val = TOK_PLUS_ASSIGN;
        } else if (p[1] == '+') {
            p += 2;
            s->token.val = TOK_INC;
        } else {
            goto def_token;
        }
        break;
    case '-':
        if (p[1] == '=') {
            p += 2;
            s->token.val = TOK_MINUS_ASSIGN;
        } else if (p[1] == '-') {
            if (s->allow_html_comments && p[2] == '>' &&
                (s->got_lf || s->last_ptr == s->buf_start)) {
                /* Annex B: `-->` at beginning of line is an html comment end.
                   It extends to the end of the line.
                 */
                goto skip_line_comment;
            }
            p += 2;
            s->token.val = TOK_DEC;
        } else {
            goto def_token;
        }
        break;
    case '<':
        if (p[1] == '=') {
            p += 2;
            s->token.val = TOK_LTE;
        } else if (p[1] == '<') {
            if (p[2] == '=') {
                p += 3;
                s->token.val = TOK_SHL_ASSIGN;
            } else {
                p += 2;
                s->token.val = TOK_SHL;
            }
        } else if (s->allow_html_comments &&
                   p[1] == '!' && p[2] == '-' && p[3] == '-') {
            /* Annex B: handle `<!--` single line html comments */
            goto skip_line_comment;
        } else {
            goto def_token;
        }
        break;
    case '>':
        if (p[1] == '=') {
            p += 2;
            s->token.val = TOK_GTE;
        } else if (p[1] == '>') {
            if (p[2] == '>') {
                if (p[3] == '=') {
                    p += 4;
                    s->token.val = TOK_SHR_ASSIGN;
                } else {
                    p += 3;
                    s->token.val = TOK_SHR;
                }
            } else if (p[2] == '=') {
                p += 3;
                s->token.val = TOK_SAR_ASSIGN;
            } else {
                p += 2;
                s->token.val = TOK_SAR;
            }
        } else {
            goto def_token;
        }
        break;
    case '=':
        if (p[1] == '=') {
            if (p[2] == '=') {
                p += 3;
                s->token.val = TOK_STRICT_EQ;
            } else {
                p += 2;
                s->token.val = TOK_EQ;
            }
        } else if (p[1] == '>') {
            p += 2;
            s->token.val = TOK_ARROW;
        } else {
            goto def_token;
        }
        break;
    case '!':
        if (p[1] == '=') {
            if (p[2] == '=') {
                p += 3;
                s->token.val = TOK_STRICT_NEQ;
            } else {
                p += 2;
                s->token.val = TOK_NEQ;
            }
        } else {
            goto def_token;
        }
        break;
    case '&':
        if (p[1] == '=') {
            p += 2;
            s->token.val = TOK_AND_ASSIGN;
        } else if (p[1] == '&') {
            if (p[2] == '=') {
                p += 3;
                s->token.val = TOK_LAND_ASSIGN;
            } else {
                p += 2;
                s->token.val = TOK_LAND;
            }
        } else {
            goto def_token;
        }
        break;
    case '^':
        if (p[1] == '=') {
            p += 2;
            s->token.val = TOK_XOR_ASSIGN;
        } else {
            goto def_token;
        }
        break;
    case '|':
        if (p[1] == '=') {
            p += 2;
            s->token.val = TOK_OR_ASSIGN;
        } else if (p[1] == '|') {
            if (p[2] == '=') {
                p += 3;
                s->token.val = TOK_LOR_ASSIGN;
            } else {
                p += 2;
                s->token.val = TOK_LOR;
            }
        } else {
            goto def_token;
        }
        break;
    case '?':
        if (p[1] == '?') {
            if (p[2] == '=') {
                p += 3;
                s->token.val = TOK_DOUBLE_QUESTION_MARK_ASSIGN;
            } else {
                p += 2;
                s->token.val = TOK_DOUBLE_QUESTION_MARK;
            }
        } else if (p[1] == '.' && !(p[2] >= '0' && p[2] <= '9')) {
            p += 2;
            s->token.val = TOK_QUESTION_MARK_DOT;
        } else {
            goto def_token;
        }
        break;
    default:
        if (c >= 128) {
            /* unicode value */
            c = unicode_from_utf8(p, UTF8_CHAR_LEN_MAX, &p);
            switch(c) {
            case CP_PS:
            case CP_LS:
                /* XXX: should avoid incrementing line_number, but
                   needed to handle HTML comments */
                goto line_terminator;
            default:
                if (lre_is_space(c)) {
                    goto redo;
                } else if (lre_js_is_ident_first(c)) {
                    ident_has_escape = FALSE;
                    goto has_ident;
                } else {
                    js_parse_error(s, "unexpected character");
                    goto fail;
                }
            }
        }
    def_token:
        s->token.val = c;
        p++;
        break;
    }
    s->buf_ptr = p;

    //    dump_token(s, &s->token);
    return 0;

 fail:
    s->token.val = TOK_ERROR;
    return -1;
}

/* 'c' is the first character. Return JS_ATOM_NULL in case of error */
/* XXX: accept unicode identifiers as JSON5 ? */
JSAtom json_parse_ident(JSParseState *s, const uint8_t **pp, int c)
{
    const uint8_t *p;
    char ident_buf[128], *buf;
    size_t ident_size, ident_pos;
    JSAtom atom;

    p = *pp;
    buf = ident_buf;
    ident_size = sizeof(ident_buf);
    ident_pos = 0;
    for(;;) {
        buf[ident_pos++] = c;
        c = *p;
        if (c >= 128 || !lre_is_id_continue_byte(c))
            break;
        p++;
        if (unlikely(ident_pos >= ident_size - UTF8_CHAR_LEN_MAX)) {
            if (ident_realloc(s->ctx, &buf, &ident_size, ident_buf)) {
                atom = JS_ATOM_NULL;
                goto done;
            }
        }
    }
    atom = JS_NewAtomLen(s->ctx, buf, ident_pos);
 done:
    if (unlikely(buf != ident_buf))
        js_free(s->ctx, buf);
    *pp = p;
    return atom;
}

int json_parse_string(JSParseState *s, const uint8_t **pp, int sep)
{
    const uint8_t *p, *p_next;
    int i;
    uint32_t c;
    StringBuffer b_s, *b = &b_s;

    if (string_buffer_init(s->ctx, b, 32))
        goto fail;

    p = *pp;
    for(;;) {
        if (p >= s->buf_end) {
            goto end_of_input;
        }
        c = *p++;
        if (c == sep)
            break;
        if (c < 0x20) {
            js_parse_error_pos(s, p - 1, "Bad control character in string literal");
            goto fail;
        }
        if (c == '\\') {
            c = *p++;
            switch(c) {
            case 'b':   c = '\b'; break;
            case 'f':   c = '\f'; break;
            case 'n':   c = '\n'; break;
            case 'r':   c = '\r'; break;
            case 't':   c = '\t'; break;
            case '\\':  break;
            case '/':   break;
            case 'u':
                c = 0;
                for(i = 0; i < 4; i++) {
                    int h = from_hex(*p++);
                    if (h < 0) {
                        js_parse_error_pos(s, p - 1, "Bad Unicode escape");
                        goto fail;
                    }
                    c = (c << 4) | h;
                }
                break;
            case '\n':
                if (s->ext_json)
                    continue;
                goto bad_escape;
            case 'v':
                if (s->ext_json) {
                    c = '\v';
                    break;
                }
                goto bad_escape;
            default:
                if (c == sep)
                    break;
                if (p > s->buf_end)
                    goto end_of_input;
            bad_escape:
                js_parse_error_pos(s, p - 1, "Bad escaped character");
                goto fail;
            }
        } else
        if (c >= 0x80) {
            c = unicode_from_utf8(p - 1, UTF8_CHAR_LEN_MAX, &p_next);
            if (c > 0x10FFFF) {
                js_parse_error_pos(s, p - 1, "Bad UTF-8 sequence");
                goto fail;
            }
            p = p_next;
        }
        if (string_buffer_putc(b, c))
            goto fail;
    }
    s->token.val = TOK_STRING;
    s->token.u.str.sep = sep;
    s->token.u.str.str = string_buffer_end(b);
    *pp = p;
    return 0;

 end_of_input:
    js_parse_error(s, "Unexpected end of JSON input");
 fail:
    string_buffer_free(b);
    return -1;
}

int json_parse_number(JSParseState *s, const uint8_t **pp)
{
    const uint8_t *p = *pp;
    const uint8_t *p_start = p;
    int radix;
    double d;
    JSATODTempMem atod_mem;

    if (*p == '+' || *p == '-')
        p++;

    if (!is_digit(*p)) {
        if (s->ext_json) {
            if (strstart((const char *)p, "Infinity", (const char **)&p)) {
                d = 1.0 / 0.0;
                if (*p_start == '-')
                    d = -d;
                goto done;
            } else if (strstart((const char *)p, "NaN", (const char **)&p)) {
                d = NAN;
                goto done;
            } else if (*p != '.') {
                goto unexpected_token;
            }
        } else {
            goto unexpected_token;
        }
    }

    if (p[0] == '0') {
        if (s->ext_json) {
            /* also accepts base 16, 8 and 2 prefix for integers */
            radix = 10;
            if (p[1] == 'x' || p[1] == 'X') {
                p += 2;
                radix = 16;
            } else if ((p[1] == 'o' || p[1] == 'O')) {
                p += 2;
                radix = 8;
            } else if ((p[1] == 'b' || p[1] == 'B')) {
                p += 2;
                radix = 2;
            }
            if (radix != 10) {
                /* prefix is present */
                if (to_digit(*p) >= radix) {
                unexpected_token:
                    return js_parse_error_pos(s, p, "Unexpected token '%c'", *p);
                }
                d = js_atod((const char *)p_start, (const char **)&p, 0,
                            JS_ATOD_INT_ONLY | JS_ATOD_ACCEPT_BIN_OCT, &atod_mem);
                goto done;
            }
        }
        if (is_digit(p[1]))
            return js_parse_error_pos(s, p, "Unexpected number");
    }

    while (is_digit(*p))
        p++;

    if (*p == '.') {
        p++;
        if (!is_digit(*p))
            return js_parse_error_pos(s, p, "Unterminated fractional number");
        while (is_digit(*p))
            p++;
    }
    if (*p == 'e' || *p == 'E') {
        p++;
        if (*p == '+' || *p == '-')
            p++;
        if (!is_digit(*p))
            return js_parse_error_pos(s, p, "Exponent part is missing a number");
        while (is_digit(*p))
            p++;
    }
    d = js_atod((const char *)p_start, NULL, 10, 0, &atod_mem);
 done:
    s->token.val = TOK_NUMBER;
    s->token.u.num.val = JS_NewFloat64(s->ctx, d);
    *pp = p;
    return 0;
}

__exception int json_next_token(JSParseState *s)
{
    const uint8_t *p;
    int c;
    JSAtom atom;

    if (js_check_stack_overflow(s->ctx->rt, 0)) {
        return js_parse_error(s, "stack overflow");
    }

    free_token(s, &s->token);

    p = s->last_ptr = s->buf_ptr;
 redo:
    s->token.ptr = p;
    c = *p;
    switch(c) {
    case 0:
        if (p >= s->buf_end) {
            s->token.val = TOK_EOF;
        } else {
            goto def_token;
        }
        break;
    case '\'':
        if (!s->ext_json) {
            /* JSON does not accept single quoted strings */
            goto def_token;
        }
        /* fall through */
    case '\"':
        p++;
        if (json_parse_string(s, &p, c))
            goto fail;
        break;
    case '\r':  /* accept DOS and MAC newline sequences */
        if (p[1] == '\n') {
            p++;
        }
        /* fall thru */
    case '\n':
        p++;
        goto redo;
    case '\f':
    case '\v':
        if (!s->ext_json) {
            /* JSONWhitespace does not match <VT>, nor <FF> */
            goto def_token;
        }
        /* fall through */
    case ' ':
    case '\t':
        p++;
        goto redo;
    case '/':
        if (!s->ext_json) {
            /* JSON does not accept comments */
            goto def_token;
        }
        if (p[1] == '*') {
            /* comment */
            p += 2;
            for(;;) {
                if (*p == '\0' && p >= s->buf_end) {
                    js_parse_error(s, "unexpected end of comment");
                    goto fail;
                }
                if (p[0] == '*' && p[1] == '/') {
                    p += 2;
                    break;
                }
                if (*p >= 0x80) {
                    c = unicode_from_utf8(p, UTF8_CHAR_LEN_MAX, &p);
                    if (c == -1) {
                        p++; /* skip invalid UTF-8 */
                    }
                } else {
                    p++;
                }
            }
            goto redo;
        } else if (p[1] == '/') {
            /* line comment */
            p += 2;
            for(;;) {
                if (*p == '\0' && p >= s->buf_end)
                    break;
                if (*p == '\r' || *p == '\n')
                    break;
                if (*p >= 0x80) {
                    c = unicode_from_utf8(p, UTF8_CHAR_LEN_MAX, &p);
                    /* LS or PS are considered as line terminator */
                    if (c == CP_LS || c == CP_PS) {
                        break;
                    } else if (c == -1) {
                        p++; /* skip invalid UTF-8 */
                    }
                } else {
                    p++;
                }
            }
            goto redo;
        } else {
            goto def_token;
        }
        break;
    case 'a': case 'b': case 'c': case 'd':
    case 'e': case 'f': case 'g': case 'h':
    case 'i': case 'j': case 'k': case 'l':
    case 'm': case 'n': case 'o': case 'p':
    case 'q': case 'r': case 's': case 't':
    case 'u': case 'v': case 'w': case 'x':
    case 'y': case 'z':
    case 'A': case 'B': case 'C': case 'D':
    case 'E': case 'F': case 'G': case 'H':
    case 'I': case 'J': case 'K': case 'L':
    case 'M': case 'N': case 'O': case 'P':
    case 'Q': case 'R': case 'S': case 'T':
    case 'U': case 'V': case 'W': case 'X':
    case 'Y': case 'Z':
    case '_':
    case '$':
        p++;
        atom = json_parse_ident(s, &p, c);
        if (atom == JS_ATOM_NULL)
            goto fail;
        s->token.u.ident.atom = atom;
        s->token.u.ident.has_escape = FALSE;
        s->token.u.ident.is_reserved = FALSE;
        s->token.val = TOK_IDENT;
        break;
    case '+':
        if (!s->ext_json)
            goto def_token;
        goto parse_number;
    case '.':
        if (s->ext_json && is_digit(p[1]))
            goto parse_number;
        else
            goto def_token;
    case '-':
    case '0':
    case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8':
    case '9':
        /* number */
    parse_number:
        if (json_parse_number(s, &p))
            goto fail;
        break;
    default:
        if (c >= 128) {
            js_parse_error(s, "unexpected character");
            goto fail;
        }
    def_token:
        s->token.val = c;
        p++;
        break;
    }
    s->buf_ptr = p;

    //    dump_token(s, &s->token);
    return 0;

 fail:
    s->token.val = TOK_ERROR;
    return -1;
}

int match_identifier(const uint8_t *p, const char *s) {
    uint32_t c;
    while (*s) {
        if ((uint8_t)*s++ != *p++)
            return 0;
    }
    c = *p;
    if (c >= 128)
        c = unicode_from_utf8(p, UTF8_CHAR_LEN_MAX, &p);
    return !lre_js_is_ident_next(c);
}

/* simple_next_token() is used to check for the next token in simple cases.
   It is only used for ':' and '=>', 'let' or 'function' look-ahead.
   (*pp) is only set if TOK_IMPORT is returned for JS_DetectModule()
   Whitespace and comments are skipped correctly.
   Then the next token is analyzed, only for specific words.
   Return values:
   - '\n' if !no_line_terminator
   - TOK_ARROW, TOK_IN, TOK_IMPORT, TOK_OF, TOK_EXPORT, TOK_FUNCTION
   - TOK_IDENT is returned for other identifiers and keywords
   - otherwise the next character or unicode codepoint is returned.
 */
int simple_next_token(const uint8_t **pp, BOOL no_line_terminator)
{
    const uint8_t *p;
    uint32_t c;

    /* skip spaces and comments */
    p = *pp;
    for (;;) {
        switch(c = *p++) {
        case '\r':
        case '\n':
            if (no_line_terminator)
                return '\n';
            continue;
        case ' ':
        case '\t':
        case '\v':
        case '\f':
            continue;
        case '/':
            if (*p == '/') {
                if (no_line_terminator)
                    return '\n';
                while (*p && *p != '\r' && *p != '\n')
                    p++;
                continue;
            }
            if (*p == '*') {
                while (*++p) {
                    if ((*p == '\r' || *p == '\n') && no_line_terminator)
                        return '\n';
                    if (*p == '*' && p[1] == '/') {
                        p += 2;
                        break;
                    }
                }
                continue;
            }
            break;
        case '=':
            if (*p == '>')
                return TOK_ARROW;
            break;
        case 'i':
            if (match_identifier(p, "n"))
                return TOK_IN;
            if (match_identifier(p, "mport")) {
                *pp = p + 5;
                return TOK_IMPORT;
            }
            return TOK_IDENT;
        case 'o':
            if (match_identifier(p, "f"))
                return TOK_OF;
            return TOK_IDENT;
        case 'e':
            if (match_identifier(p, "xport"))
                return TOK_EXPORT;
            return TOK_IDENT;
        case 'f':
            if (match_identifier(p, "unction"))
                return TOK_FUNCTION;
            return TOK_IDENT;
        case '\\':
            if (*p == 'u') {
                if (lre_js_is_ident_first(lre_parse_escape(&p, TRUE)))
                    return TOK_IDENT;
            }
            break;
        default:
            if (c >= 128) {
                c = unicode_from_utf8(p - 1, UTF8_CHAR_LEN_MAX, &p);
                if (no_line_terminator && (c == CP_PS || c == CP_LS))
                    return '\n';
            }
            if (lre_is_space(c))
                continue;
            if (lre_js_is_ident_first(c))
                return TOK_IDENT;
            break;
        }
        return c;
    }
}

int peek_token(JSParseState *s, BOOL no_line_terminator)
{
    const uint8_t *p = s->buf_ptr;
    return simple_next_token(&p, no_line_terminator);
}

void skip_shebang(const uint8_t **pp, const uint8_t *buf_end)
{
    const uint8_t *p = *pp;
    int c;

    if (p[0] == '#' && p[1] == '!') {
        p += 2;
        while (p < buf_end) {
            if (*p == '\n' || *p == '\r') {
                break;
            } else if (*p >= 0x80) {
                c = unicode_from_utf8(p, UTF8_CHAR_LEN_MAX, &p);
                if (c == CP_LS || c == CP_PS) {
                    break;
                } else if (c == -1) {
                    p++; /* skip invalid UTF-8 */
                }
            } else {
                p++;
            }
        }
        *pp = p;
    }
}

BOOL js_is_live_code(JSParseState *s) {
    switch (get_prev_opcode(s->cur_func)) {
    case OP_tail_call:
    case OP_tail_call_method:
    case OP_return:
    case OP_return_undef:
    case OP_return_async:
    case OP_throw:
    case OP_throw_error:
    case OP_goto:
#if SHORT_OPCODES
    case OP_goto8:
    case OP_goto16:
#endif
    case OP_ret:
        return FALSE;
    default:
        return TRUE;
    }
}

void emit_u8(JSParseState *s, uint8_t val)
{
    dbuf_putc(&s->cur_func->byte_code, val);
}

void emit_u16(JSParseState *s, uint16_t val)
{
    dbuf_put_u16(&s->cur_func->byte_code, val);
}
