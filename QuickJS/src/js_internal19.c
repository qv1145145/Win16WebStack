#include "js_internal.h"



int js_proxy_define_own_property(JSContext *ctx, JSValueConst obj,
                                        JSAtom prop, JSValueConst val,
                                        JSValueConst getter, JSValueConst setter,
                                        int flags)
{
    JSProxyData *s;
    JSValue method, ret1, prop_val, desc_val;
    int res, ret;
    JSObject *p;
    JSValueConst args[3];
    JSPropertyDescriptor desc;
    BOOL setting_not_configurable;

    s = get_proxy_method(ctx, &method, obj, JS_ATOM_defineProperty);
    if (!s)
        return -1;
    if (JS_IsUndefined(method)) {
        return JS_DefineProperty(ctx, s->target, prop, val, getter, setter, flags);
    }
    prop_val = JS_AtomToValue(ctx, prop);
    if (JS_IsException(prop_val)) {
        JS_FreeValue(ctx, method);
        return -1;
    }
    desc_val = js_create_desc(ctx, val, getter, setter, flags);
    if (JS_IsException(desc_val)) {
        JS_FreeValue(ctx, prop_val);
        JS_FreeValue(ctx, method);
        return -1;
    }
    args[0] = s->target;
    args[1] = prop_val;
    args[2] = desc_val;
    ret1 = JS_CallFree(ctx, method, s->handler, 3, args);
    JS_FreeValue(ctx, prop_val);
    JS_FreeValue(ctx, desc_val);
    if (JS_IsException(ret1))
        return -1;
    ret = JS_ToBoolFree(ctx, ret1);
    if (!ret) {
        if (flags & JS_PROP_THROW) {
            JS_ThrowTypeError(ctx, "proxy: defineProperty exception");
            return -1;
        } else {
            return 0;
        }
    }
    p = JS_VALUE_GET_OBJ(s->target);
    res = JS_GetOwnPropertyInternal(ctx, &desc, p, prop);
    if (res < 0)
        return -1;
    setting_not_configurable = ((flags & (JS_PROP_HAS_CONFIGURABLE |
                                          JS_PROP_CONFIGURABLE)) ==
                                JS_PROP_HAS_CONFIGURABLE);
    if (!res) {
        if (!p->extensible || setting_not_configurable)
            goto fail;
    } else {
        if (!check_define_prop_flags(desc.flags, flags))
            goto fail1;
        /* do the missing check from check_define_prop_flags() */
        if (!(desc.flags & JS_PROP_CONFIGURABLE)) {
            if ((desc.flags & JS_PROP_TMASK) == JS_PROP_GETSET) {
                if ((flags & JS_PROP_HAS_GET) &&
                    !js_same_value(ctx, getter, desc.getter)) {
                    goto fail1;
                }
                if ((flags & JS_PROP_HAS_SET) &&
                    !js_same_value(ctx, setter, desc.setter)) {
                    goto fail1;
                }
            } else if (!(desc.flags & JS_PROP_WRITABLE)) {
                if ((flags & JS_PROP_HAS_VALUE) &&
                    !js_same_value(ctx, val, desc.value)) {
                    goto fail1;
                }
            }
        }

        /* additional checks */
        if ((desc.flags & JS_PROP_CONFIGURABLE) && setting_not_configurable)
            goto fail1;

        if ((desc.flags & JS_PROP_TMASK) != JS_PROP_GETSET &&
            (desc.flags & (JS_PROP_CONFIGURABLE | JS_PROP_WRITABLE)) == JS_PROP_WRITABLE &&
            (flags & (JS_PROP_HAS_WRITABLE | JS_PROP_WRITABLE)) == JS_PROP_HAS_WRITABLE) {
        fail1:
            js_free_desc(ctx, &desc);
        fail:
            JS_ThrowTypeError(ctx, "proxy: inconsistent defineProperty");
            return -1;
        }
        js_free_desc(ctx, &desc);
    }
    return 1;
}

int js_proxy_delete_property(JSContext *ctx, JSValueConst obj,
                                    JSAtom atom)
{
    JSProxyData *s;
    JSValue method, ret, atom_val;
    int res, res2, is_extensible;
    JSValueConst args[2];

    s = get_proxy_method(ctx, &method, obj, JS_ATOM_deleteProperty);
    if (!s)
        return -1;
    if (JS_IsUndefined(method)) {
        return JS_DeleteProperty(ctx, s->target, atom, 0);
    }
    atom_val = JS_AtomToValue(ctx, atom);;
    if (JS_IsException(atom_val)) {
        JS_FreeValue(ctx, method);
        return -1;
    }
    args[0] = s->target;
    args[1] = atom_val;
    ret = JS_CallFree(ctx, method, s->handler, 2, args);
    JS_FreeValue(ctx, atom_val);
    if (JS_IsException(ret))
        return -1;
    res = JS_ToBoolFree(ctx, ret);
    if (res) {
        JSPropertyDescriptor desc;
        res2 = JS_GetOwnPropertyInternal(ctx, &desc, JS_VALUE_GET_OBJ(s->target), atom);
        if (res2 < 0)
            return -1;
        if (res2) {
            if (!(desc.flags & JS_PROP_CONFIGURABLE))
                goto fail;
            is_extensible = JS_IsExtensible(ctx, s->target);
            if (is_extensible < 0)
                goto fail1;
            if (!is_extensible) {
                /* proxy-missing-checks */
            fail:
                JS_ThrowTypeError(ctx, "proxy: inconsistent deleteProperty");
            fail1:
                js_free_desc(ctx, &desc);
                return -1;
            }
            js_free_desc(ctx, &desc);
        }
    }
    return res;
}

/* return the index of the property or -1 if not found */
int find_prop_key(const JSPropertyEnum *tab, int n, JSAtom atom)
{
    int i;
    for(i = 0; i < n; i++) {
        if (tab[i].atom == atom)
            return i;
    }
    return -1;
}

int js_proxy_get_own_property_names(JSContext *ctx,
                                           JSPropertyEnum **ptab,
                                           uint32_t *plen,
                                           JSValueConst obj)
{
    JSProxyData *s;
    JSValue method, prop_array, val;
    uint32_t len, i, len2;
    JSPropertyEnum *tab, *tab2;
    JSAtom atom;
    JSPropertyDescriptor desc;
    int res, is_extensible, idx;

    s = get_proxy_method(ctx, &method, obj, JS_ATOM_ownKeys);
    if (!s)
        return -1;
    if (JS_IsUndefined(method)) {
        return JS_GetOwnPropertyNamesInternal(ctx, ptab, plen,
                                      JS_VALUE_GET_OBJ(s->target),
                                      JS_GPN_STRING_MASK | JS_GPN_SYMBOL_MASK);
    }
    prop_array = JS_CallFree(ctx, method, s->handler, 1, (JSValueConst *)&s->target);
    if (JS_IsException(prop_array))
        return -1;
    tab = NULL;
    len = 0;
    tab2 = NULL;
    len2 = 0;
    if (js_get_length32(ctx, &len, prop_array))
        goto fail;
    if (len > 0) {
        tab = js_mallocz(ctx, sizeof(tab[0]) * len);
        if (!tab)
            goto fail;
    }
    for(i = 0; i < len; i++) {
        val = JS_GetPropertyUint32(ctx, prop_array, i);
        if (JS_IsException(val))
            goto fail;
        if (!JS_IsString(val) && !JS_IsSymbol(val)) {
            JS_FreeValue(ctx, val);
            JS_ThrowTypeError(ctx, "proxy: properties must be strings or symbols");
            goto fail;
        }
        atom = JS_ValueToAtom(ctx, val);
        JS_FreeValue(ctx, val);
        if (atom == JS_ATOM_NULL)
            goto fail;
        tab[i].atom = atom;
        tab[i].is_enumerable = FALSE; /* XXX: redundant? */
    }

    /* check duplicate properties (XXX: inefficient, could store the
     * properties an a temporary object to use the hash) */
    for(i = 1; i < len; i++) {
        if (find_prop_key(tab, i, tab[i].atom) >= 0) {
            JS_ThrowTypeError(ctx, "proxy: duplicate property");
            goto fail;
        }
    }

    is_extensible = JS_IsExtensible(ctx, s->target);
    if (is_extensible < 0)
        goto fail;

    /* check if there are non configurable properties */
    if (s->is_revoked) {
        JS_ThrowTypeErrorRevokedProxy(ctx);
        goto fail;
    }
    if (JS_GetOwnPropertyNamesInternal(ctx, &tab2, &len2, JS_VALUE_GET_OBJ(s->target),
                               JS_GPN_STRING_MASK | JS_GPN_SYMBOL_MASK))
        goto fail;
    for(i = 0; i < len2; i++) {
        if (s->is_revoked) {
            JS_ThrowTypeErrorRevokedProxy(ctx);
            goto fail;
        }
        res = JS_GetOwnPropertyInternal(ctx, &desc, JS_VALUE_GET_OBJ(s->target),
                                tab2[i].atom);
        if (res < 0)
            goto fail;
        if (res) {  /* safety, property should be found */
            js_free_desc(ctx, &desc);
            if (!(desc.flags & JS_PROP_CONFIGURABLE) || !is_extensible) {
                idx = find_prop_key(tab, len, tab2[i].atom);
                if (idx < 0) {
                    JS_ThrowTypeError(ctx, "proxy: target property must be present in proxy ownKeys");
                    goto fail;
                }
                /* mark the property as found */
                if (!is_extensible)
                    tab[idx].is_enumerable = TRUE;
            }
        }
    }
    if (!is_extensible) {
        /* check that all property in 'tab' were checked */
        for(i = 0; i < len; i++) {
            if (!tab[i].is_enumerable) {
                JS_ThrowTypeError(ctx, "proxy: property not present in target were returned by non extensible proxy");
                goto fail;
            }
        }
    }

    JS_FreePropertyEnum(ctx, tab2, len2);
    JS_FreeValue(ctx, prop_array);
    *ptab = tab;
    *plen = len;
    return 0;
 fail:
    JS_FreePropertyEnum(ctx, tab2, len2);
    JS_FreePropertyEnum(ctx, tab, len);
    JS_FreeValue(ctx, prop_array);
    return -1;
}

JSValue js_proxy_call_constructor(JSContext *ctx, JSValueConst func_obj,
                                         JSValueConst new_target,
                                         int argc, JSValueConst *argv)
{
    JSProxyData *s;
    JSValue method, arg_array, ret;
    JSValueConst args[3];

    s = get_proxy_method(ctx, &method, func_obj, JS_ATOM_construct);
    if (!s)
        return JS_EXCEPTION;
    if (!JS_IsConstructor(ctx, s->target))
        return JS_ThrowTypeErrorNotAConstructor(ctx, s->target);
    if (JS_IsUndefined(method))
        return JS_CallConstructor2(ctx, s->target, new_target, argc, argv);
    arg_array = js_create_array(ctx, argc, argv);
    if (JS_IsException(arg_array)) {
        ret = JS_EXCEPTION;
        goto fail;
    }
    args[0] = s->target;
    args[1] = arg_array;
    args[2] = new_target;
    ret = JS_Call(ctx, method, s->handler, 3, args);
    if (!JS_IsException(ret) && JS_VALUE_GET_TAG(ret) != JS_TAG_OBJECT) {
        JS_FreeValue(ctx, ret);
        ret = JS_ThrowTypeErrorNotAnObject(ctx);
    }
 fail:
    JS_FreeValue(ctx, method);
    JS_FreeValue(ctx, arg_array);
    return ret;
}

JSValue js_proxy_call(JSContext *ctx, JSValueConst func_obj,
                             JSValueConst this_obj,
                             int argc, JSValueConst *argv, int flags)
{
    JSProxyData *s;
    JSValue method, arg_array, ret;
    JSValueConst args[3];

    if (flags & JS_CALL_FLAG_CONSTRUCTOR)
        return js_proxy_call_constructor(ctx, func_obj, this_obj, argc, argv);

    s = get_proxy_method(ctx, &method, func_obj, JS_ATOM_apply);
    if (!s)
        return JS_EXCEPTION;
    if (!s->is_func) {
        JS_FreeValue(ctx, method);
        return JS_ThrowTypeError(ctx, "not a function");
    }
    if (JS_IsUndefined(method))
        return JS_Call(ctx, s->target, this_obj, argc, argv);
    arg_array = js_create_array(ctx, argc, argv);
    if (JS_IsException(arg_array)) {
        ret = JS_EXCEPTION;
        goto fail;
    }
    args[0] = s->target;
    args[1] = this_obj;
    args[2] = arg_array;
    ret = JS_Call(ctx, method, s->handler, 3, args);
 fail:
    JS_FreeValue(ctx, method);
    JS_FreeValue(ctx, arg_array);
    return ret;
}

/* `js_resolve_proxy`: resolve the proxy chain
   `*pval` is updated with to ultimate proxy target
   `throw_exception` controls whether exceptions are thown or not
   - return -1 in case of error
   - otherwise return 0
 */
int js_resolve_proxy(JSContext *ctx, JSValueConst *pval, BOOL throw_exception) {
    int depth = 0;
    JSObject *p;
    JSProxyData *s;

    while (JS_VALUE_GET_TAG(*pval) == JS_TAG_OBJECT) {
        p = JS_VALUE_GET_OBJ(*pval);
        if (p->class_id != JS_CLASS_PROXY)
            break;
        if (depth++ > 1000) {
            if (throw_exception)
                JS_ThrowStackOverflow(ctx);
            return -1;
        }
        s = p->u.opaque;
        if (s->is_revoked) {
            if (throw_exception)
                JS_ThrowTypeErrorRevokedProxy(ctx);
            return -1;
        }
        *pval = s->target;
    }
    return 0;
}

JSValue js_proxy_constructor(JSContext *ctx, JSValueConst this_val,
                                    int argc, JSValueConst *argv)
{
    JSValueConst target, handler;
    JSValue obj;
    JSProxyData *s;

    target = argv[0];
    handler = argv[1];
    if (JS_VALUE_GET_TAG(target) != JS_TAG_OBJECT ||
        JS_VALUE_GET_TAG(handler) != JS_TAG_OBJECT)
        return JS_ThrowTypeErrorNotAnObject(ctx);

    obj = JS_NewObjectProtoClass(ctx, JS_NULL, JS_CLASS_PROXY);
    if (JS_IsException(obj))
        return obj;
    s = js_malloc(ctx, sizeof(JSProxyData));
    if (!s) {
        JS_FreeValue(ctx, obj);
        return JS_EXCEPTION;
    }
    s->target = JS_DupValue(ctx, target);
    s->handler = JS_DupValue(ctx, handler);
    s->is_func = JS_IsFunction(ctx, target);
    s->is_revoked = FALSE;
    JS_SetOpaque(obj, s);
    JS_SetConstructorBit(ctx, obj, JS_IsConstructor(ctx, target));
    return obj;
}

JSValue js_proxy_revoke(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv, int magic,
                               JSValue *func_data)
{
    JSProxyData *s = JS_GetOpaque(func_data[0], JS_CLASS_PROXY);
    if (s) {
        /* We do not free the handler and target in case they are
           referenced as constants in the C call stack */
        s->is_revoked = TRUE;
        JS_FreeValue(ctx, func_data[0]);
        func_data[0] = JS_NULL;
    }
    return JS_UNDEFINED;
}

JSValue js_proxy_revoke_constructor(JSContext *ctx,
                                           JSValueConst proxy_obj)
{
    return JS_NewCFunctionData(ctx, js_proxy_revoke, 0, 0, 1, &proxy_obj);
}

JSValue js_proxy_revocable(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv)
{
    JSValue proxy_obj, revoke_obj = JS_UNDEFINED, obj;

    proxy_obj = js_proxy_constructor(ctx, JS_UNDEFINED, argc, argv);
    if (JS_IsException(proxy_obj))
        goto fail;
    revoke_obj = js_proxy_revoke_constructor(ctx, proxy_obj);
    if (JS_IsException(revoke_obj))
        goto fail;
    obj = JS_NewObject(ctx);
    if (JS_IsException(obj))
        goto fail;
    // XXX: exceptions?
    JS_DefinePropertyValue(ctx, obj, JS_ATOM_proxy, proxy_obj, JS_PROP_C_W_E);
    JS_DefinePropertyValue(ctx, obj, JS_ATOM_revoke, revoke_obj, JS_PROP_C_W_E);
    return obj;
 fail:
    JS_FreeValue(ctx, proxy_obj);
    JS_FreeValue(ctx, revoke_obj);
    return JS_EXCEPTION;
}

/* Symbol */

JSValue js_symbol_constructor(JSContext *ctx, JSValueConst new_target,
                                     int argc, JSValueConst *argv)
{
    JSValue str;
    JSString *p;

    if (!JS_IsUndefined(new_target))
        return JS_ThrowTypeErrorNotAConstructor(ctx, new_target);
    if (argc == 0 || JS_IsUndefined(argv[0])) {
        p = NULL;
    } else {
        str = JS_ToString(ctx, argv[0]);
        if (JS_IsException(str))
            return JS_EXCEPTION;
        p = JS_VALUE_GET_STRING(str);
    }
    return JS_NewSymbol(ctx, p, JS_ATOM_TYPE_SYMBOL);
}

JSValue js_thisSymbolValue(JSContext *ctx, JSValueConst this_val)
{
    if (JS_VALUE_GET_TAG(this_val) == JS_TAG_SYMBOL)
        return JS_DupValue(ctx, this_val);

    if (JS_VALUE_GET_TAG(this_val) == JS_TAG_OBJECT) {
        JSObject *p = JS_VALUE_GET_OBJ(this_val);
        if (p->class_id == JS_CLASS_SYMBOL) {
            if (JS_VALUE_GET_TAG(p->u.object_data) == JS_TAG_SYMBOL)
                return JS_DupValue(ctx, p->u.object_data);
        }
    }
    return JS_ThrowTypeError(ctx, "not a symbol");
}

JSValue js_symbol_toString(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv)
{
    JSValue val, ret;
    val = js_thisSymbolValue(ctx, this_val);
    if (JS_IsException(val))
        return val;
    /* XXX: use JS_ToStringInternal() with a flags */
    ret = js_string_constructor(ctx, JS_UNDEFINED, 1, (JSValueConst *)&val);
    JS_FreeValue(ctx, val);
    return ret;
}

JSValue js_symbol_valueOf(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv)
{
    return js_thisSymbolValue(ctx, this_val);
}

JSValue js_symbol_get_description(JSContext *ctx, JSValueConst this_val)
{
    JSValue val, ret;
    JSAtomStruct *p;

    val = js_thisSymbolValue(ctx, this_val);
    if (JS_IsException(val))
        return val;
    p = JS_VALUE_GET_PTR(val);
    if (p->len == 0 && p->is_wide_char != 0) {
        ret = JS_UNDEFINED;
    } else {
        ret = JS_AtomToString(ctx, js_get_atom_index(ctx->rt, p));
    }
    JS_FreeValue(ctx, val);
    return ret;
}

JSValue js_symbol_for(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv)
{
    JSValue str;

    str = JS_ToString(ctx, argv[0]);
    if (JS_IsException(str))
        return JS_EXCEPTION;
    return JS_NewSymbol(ctx, JS_VALUE_GET_STRING(str), JS_ATOM_TYPE_GLOBAL_SYMBOL);
}

JSValue js_symbol_keyFor(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv)
{
    JSAtomStruct *p;

    if (!JS_IsSymbol(argv[0]))
        return JS_ThrowTypeError(ctx, "not a symbol");
    p = JS_VALUE_GET_PTR(argv[0]);
    if (p->atom_type != JS_ATOM_TYPE_GLOBAL_SYMBOL)
        return JS_UNDEFINED;
    return JS_DupValue(ctx, JS_MKPTR(JS_TAG_STRING, p));
}

/* Set/Map/WeakSet/WeakMap */

BOOL js_weakref_is_target(JSValueConst val)
{
    switch (JS_VALUE_GET_TAG(val)) {
    case JS_TAG_OBJECT:
        return TRUE;
    case JS_TAG_SYMBOL:
        {
            JSAtomStruct *p = JS_VALUE_GET_PTR(val);
            if (p->atom_type == JS_ATOM_TYPE_SYMBOL &&
                p->hash != JS_ATOM_HASH_PRIVATE)
                return TRUE;
        }
        break;
    default:
        break;
    }
    return FALSE;
}

/* JS_UNDEFINED is considered as a live weakref */
/* XXX: add a specific JSWeakRef value type ? */
BOOL js_weakref_is_live(JSValueConst val)
{
    void *p;
    if (JS_IsUndefined(val))
        return TRUE;
    p = JS_VALUE_GET_PTR(val);
    return (js_rc(p)->ref_count != 0);
}

/* 'val' can be JS_UNDEFINED */
void js_weakref_free(JSRuntime *rt, JSValue val)
{
    if (JS_VALUE_GET_TAG(val) == JS_TAG_OBJECT) {
        JSObject *p = JS_VALUE_GET_OBJ(val);
        assert(p->weakref_count >= 1);
        p->weakref_count--;
        /* 'mark' is tested to avoid freeing the object structure when
           it is about to be freed in a cycle or in
           free_zero_refcount() */
        if (p->weakref_count == 0 && js_rc(p)->ref_count == 0 &&
            js_rc(p)->mark == 0) {
            js_free_rt(rt, p);
        }
    } else if (JS_VALUE_GET_TAG(val) == JS_TAG_SYMBOL) {
        JSString *p = JS_VALUE_GET_STRING(val);
        assert(p->hash >= 1);
        p->hash--;
        if (p->hash == 0 && js_rc(p)->ref_count == 0) {
            /* can remove the dummy structure */
            js_free_rt(rt, p);
        }
    }
}

/* val must be an object, a symbol or undefined (see
   js_weakref_is_target). */
JSValue js_weakref_new(JSContext *ctx, JSValueConst val)
{
    if (JS_VALUE_GET_TAG(val) == JS_TAG_OBJECT) {
        JSObject *p = JS_VALUE_GET_OBJ(val);
        p->weakref_count++;
    } else if (JS_VALUE_GET_TAG(val) == JS_TAG_SYMBOL) {
        JSString *p = JS_VALUE_GET_STRING(val);
        /* XXX: could return an exception if too many references */
        assert(p->hash < JS_ATOM_HASH_MASK - 2);
        p->hash++;
    } else {
        assert(JS_IsUndefined(val));
    }
    return (JSValue)val;
}

#define MAGIC_SET (1 << 0)
#define MAGIC_WEAK (1 << 1)

JSValue js_map_constructor(JSContext *ctx, JSValueConst new_target,
                                  int argc, JSValueConst *argv, int magic)
{
    JSMapState *s;
    JSValue obj, adder = JS_UNDEFINED, iter = JS_UNDEFINED, next_method = JS_UNDEFINED;
    JSValueConst arr;
    BOOL is_set, is_weak;

    is_set = magic & MAGIC_SET;
    is_weak = ((magic & MAGIC_WEAK) != 0);
    obj = js_create_from_ctor(ctx, new_target, JS_CLASS_MAP + magic);
    if (JS_IsException(obj))
        return JS_EXCEPTION;
    s = js_mallocz(ctx, sizeof(*s));
    if (!s)
        goto fail;
    init_list_head(&s->records);
    s->is_weak = is_weak;
    if (is_weak) {
        s->weakref_header.weakref_type = JS_WEAKREF_TYPE_MAP;
        list_add_tail(&s->weakref_header.link, &ctx->rt->weakref_list);
    }
    JS_SetOpaque(obj, s);
    s->hash_bits = 1;
    s->hash_size = 1U << s->hash_bits;
    s->hash_table = js_mallocz(ctx, sizeof(s->hash_table[0]) * s->hash_size);
    if (!s->hash_table)
        goto fail;
    s->record_count_threshold = 4;

    arr = JS_UNDEFINED;
    if (argc > 0)
        arr = argv[0];
    if (!JS_IsUndefined(arr) && !JS_IsNull(arr)) {
        JSValue item, ret;
        BOOL done;

        adder = JS_GetProperty(ctx, obj, is_set ? JS_ATOM_add : JS_ATOM_set);
        if (JS_IsException(adder))
            goto fail;
        if (! JS_IsFunction(ctx, adder)) {
            JS_ThrowTypeError(ctx, "set/add is not a function");
            goto fail;
        }

        iter = JS_GetIterator(ctx, arr, FALSE);
        if (JS_IsException(iter))
            goto fail;
        next_method = JS_GetProperty(ctx, iter, JS_ATOM_next);
        if (JS_IsException(next_method))
            goto fail;

        for(;;) {
            item = JS_IteratorNext(ctx, iter, next_method, 0, NULL, &done);
            if (JS_IsException(item))
                goto fail;
            if (done)
                break;
            if (is_set) {
                ret = JS_Call(ctx, adder, obj, 1, (JSValueConst *)&item);
                if (JS_IsException(ret)) {
                    JS_FreeValue(ctx, item);
                    goto fail_close;
                }
            } else {
                JSValue key, value;
                JSValueConst args[2];
                key = JS_UNDEFINED;
                value = JS_UNDEFINED;
                if (!JS_IsObject(item)) {
                    JS_ThrowTypeErrorNotAnObject(ctx);
                    goto fail1;
                }
                key = JS_GetPropertyUint32(ctx, item, 0);
                if (JS_IsException(key))
                    goto fail1;
                value = JS_GetPropertyUint32(ctx, item, 1);
                if (JS_IsException(value))
                    goto fail1;
                args[0] = key;
                args[1] = value;
                ret = JS_Call(ctx, adder, obj, 2, args);
                if (JS_IsException(ret)) {
                fail1:
                    JS_FreeValue(ctx, item);
                    JS_FreeValue(ctx, key);
                    JS_FreeValue(ctx, value);
                    goto fail_close;
                }
                JS_FreeValue(ctx, key);
                JS_FreeValue(ctx, value);
            }
            JS_FreeValue(ctx, ret);
            JS_FreeValue(ctx, item);
        }
        JS_FreeValue(ctx, next_method);
        JS_FreeValue(ctx, iter);
        JS_FreeValue(ctx, adder);
    }
    return obj;
 fail_close:
    /* close the iterator object, preserving pending exception */
    JS_IteratorClose(ctx, iter, TRUE);
 fail:
    JS_FreeValue(ctx, next_method);
    JS_FreeValue(ctx, iter);
    JS_FreeValue(ctx, adder);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

/* XXX: could normalize strings to speed up comparison */
JSValue map_normalize_key(JSContext *ctx, JSValue key)
{
    uint32_t tag = JS_VALUE_GET_TAG(key);
    /* convert -0.0 to +0.0 */
    if (JS_TAG_IS_FLOAT64(tag) && JS_VALUE_GET_FLOAT64(key) == 0.0) {
        key = JS_NewInt32(ctx, 0);
    }
    return key;
}

JSValueConst map_normalize_key_const(JSContext *ctx, JSValueConst key)
{
    return (JSValueConst)map_normalize_key(ctx, (JSValue)key);
}

/* hash multipliers, same as the Linux kernel (see Knuth vol 3,
   section 6.4, exercise 9) */
#define HASH_MUL32 0x61C88647
#define HASH_MUL64 UINT64_C(0x61C8864680B583EB)

uint32_t map_hash32(uint32_t a, int hash_bits)
{
    return (a * HASH_MUL32) >> (32 - hash_bits);
}

uint32_t map_hash64(uint64_t a, int hash_bits)
{
    return (a * HASH_MUL64) >> (64 - hash_bits);
}

uint32_t map_hash_pointer(uintptr_t a, int hash_bits)
{
#ifdef JS_PTR64
    return map_hash64(a, hash_bits);
#else
    return map_hash32(a, hash_bits);
#endif
}

/* XXX: better hash ? */
/* precondition: 1 <= hash_bits <= 32 */
uint32_t map_hash_key(JSValueConst key, int hash_bits)
{
    uint32_t tag = JS_VALUE_GET_NORM_TAG(key);
    uint32_t h;
    double d;
    JSBigInt *p;
    JSBigIntBuf buf;

    switch(tag) {
    case JS_TAG_BOOL:
        h = map_hash32(JS_VALUE_GET_INT(key) ^ JS_TAG_BOOL, hash_bits);
        break;
    case JS_TAG_STRING:
        h = map_hash32(hash_string(JS_VALUE_GET_STRING(key), 0) ^ JS_TAG_STRING, hash_bits);
        break;
    case JS_TAG_STRING_ROPE:
        h = map_hash32(hash_string_rope(key, 0) ^ JS_TAG_STRING, hash_bits);
        break;
    case JS_TAG_OBJECT:
    case JS_TAG_SYMBOL:
        h = map_hash_pointer((uintptr_t)JS_VALUE_GET_PTR(key) ^ tag, hash_bits);
        break;
    case JS_TAG_INT:
        d = JS_VALUE_GET_INT(key);
        goto hash_float64;
    case JS_TAG_FLOAT64:
        d = JS_VALUE_GET_FLOAT64(key);
        /* normalize the NaN */
        if (isnan(d))
            d = JS_FLOAT64_NAN;
    hash_float64:
        h = map_hash64(float64_as_uint64(d) ^ JS_TAG_FLOAT64, hash_bits);
        break;
    case JS_TAG_SHORT_BIG_INT:
        p = js_bigint_set_short(&buf, key);
        goto hash_bigint;
    case JS_TAG_BIG_INT:
        p = JS_VALUE_GET_PTR(key);
    hash_bigint:
        {
            int i;
            h = 1;
            for(i = p->len - 1; i >= 0; i--) {
                h = h * 263 + p->tab[i];
            }
            /* the final step is necessary otherwise h mod n only
               depends of p->tab[i] mod n */
            h = map_hash32(h ^ JS_TAG_BIG_INT, hash_bits);
        }
        break;
    default:
        h = 0;
        break;
    }
    return h;
}

JSMapRecord *map_find_record(JSContext *ctx, JSMapState *s,
                                    JSValueConst key)
{
    JSMapRecord *mr;
    uint32_t h;
    h = map_hash_key(key, s->hash_bits);
    for(mr = s->hash_table[h]; mr != NULL; mr = mr->hash_next) {
        if (mr->empty || (s->is_weak && !js_weakref_is_live(mr->key))) {
            /* cannot match */
        } else {
            if (js_same_value_zero(ctx, mr->key, key))
                return mr;
        }
    }
    return NULL;
}

void map_hash_resize(JSContext *ctx, JSMapState *s)
{
    uint32_t new_hash_size, h;
    int new_hash_bits;
    struct list_head *el;
    JSMapRecord *mr, **new_hash_table;

    /* XXX: no reporting of memory allocation failure */
    new_hash_bits = min_int(s->hash_bits + 1, 31);
    new_hash_size = 1U << new_hash_bits;
    new_hash_table = js_realloc(ctx, s->hash_table,
                                sizeof(new_hash_table[0]) * new_hash_size);
    if (!new_hash_table)
        return;

    memset(new_hash_table, 0, sizeof(new_hash_table[0]) * new_hash_size);

    list_for_each(el, &s->records) {
        mr = list_entry(el, JSMapRecord, link);
        if (mr->empty || (s->is_weak && !js_weakref_is_live(mr->key))) {
        } else {
            h = map_hash_key(mr->key, new_hash_bits);
            mr->hash_next = new_hash_table[h];
            new_hash_table[h] = mr;
        }
    }
    s->hash_table = new_hash_table;
    s->hash_bits = new_hash_bits;
    s->hash_size = new_hash_size;
    s->record_count_threshold = new_hash_size * 2;
}

JSMapRecord *map_add_record(JSContext *ctx, JSMapState *s,
                                   JSValueConst key)
{
    uint32_t h;
    JSMapRecord *mr;

    mr = js_malloc(ctx, sizeof(*mr));
    if (!mr)
        return NULL;
    mr->ref_count = 1;
    mr->empty = FALSE;
    if (s->is_weak) {
        mr->key = js_weakref_new(ctx, key);
    } else {
        mr->key = JS_DupValue(ctx, key);
    }
    h = map_hash_key(key, s->hash_bits);
    mr->hash_next = s->hash_table[h];
    s->hash_table[h] = mr;
    list_add_tail(&mr->link, &s->records);
    s->record_count++;
    if (s->record_count >= s->record_count_threshold) {
        map_hash_resize(ctx, s);
    }
    return mr;
}

JSMapRecord *set_add_record(JSContext *ctx, JSMapState *s,
                                   JSValueConst key)
{
    JSMapRecord *mr;
    mr = map_add_record(ctx, s, key);
    if (!mr)
        return NULL;
    mr->value = JS_UNDEFINED;
    return mr;
}

/* warning: the record must be removed from the hash table before */
void map_delete_record_internal(JSRuntime *rt, JSMapState *s, JSMapRecord *mr)
{
    if (mr->empty)
        return;

    if (s->is_weak) {
        js_weakref_free(rt, mr->key);
    } else {
        JS_FreeValueRT(rt, mr->key);
    }
    JS_FreeValueRT(rt, mr->value);
    if (--mr->ref_count == 0) {
        list_del(&mr->link);
        js_free_rt(rt, mr);
    } else {
        /* keep a zombie record for iterators */
        mr->empty = TRUE;
        mr->key = JS_UNDEFINED;
        mr->value = JS_UNDEFINED;
    }
    s->record_count--;
}

void map_decref_record(JSRuntime *rt, JSMapRecord *mr)
{
    if (--mr->ref_count == 0) {
        /* the record can be safely removed */
        assert(mr->empty);
        list_del(&mr->link);
        js_free_rt(rt, mr);
    }
}

void map_delete_weakrefs(JSRuntime *rt, JSWeakRefHeader *wh)
{
    JSMapState *s = container_of(wh, JSMapState, weakref_header);
    struct list_head *el, *el1;
    JSMapRecord *mr1, **pmr;
    uint32_t h;

    list_for_each_safe(el, el1, &s->records) {
        JSMapRecord *mr = list_entry(el, JSMapRecord, link);
        if (!js_weakref_is_live(mr->key)) {

            /* even if key is not live it can be hashed as a pointer */
            h = map_hash_key(mr->key, s->hash_bits);
            pmr = &s->hash_table[h];
            for(;;) {
                mr1 = *pmr;
                /* the entry may already be removed from the hash
                   table if the map was resized */
                if (mr1 == NULL)
                    goto done;
                if (mr1 == mr)
                    break;
                pmr = &mr1->hash_next;
            }
            /* remove from the hash table */
            *pmr = mr1->hash_next;
        done:
            map_delete_record_internal(rt, s, mr);
        }
    }
}

JSValue js_map_set(JSContext *ctx, JSValueConst this_val,
                          int argc, JSValueConst *argv, int magic)
{
    JSMapState *s = JS_GetOpaque2(ctx, this_val, JS_CLASS_MAP + magic);
    JSMapRecord *mr;
    JSValueConst key, value;

    if (!s)
        return JS_EXCEPTION;
    key = map_normalize_key_const(ctx, argv[0]);
    if (s->is_weak && !js_weakref_is_target(key))
        return JS_ThrowTypeError(ctx, "invalid value used as %s key", (magic & MAGIC_SET) ? "WeakSet" : "WeakMap");
    if (magic & MAGIC_SET)
        value = JS_UNDEFINED;
    else
        value = argv[1];
    mr = map_find_record(ctx, s, key);
    if (mr) {
        JS_FreeValue(ctx, mr->value);
    } else {
        mr = map_add_record(ctx, s, key);
        if (!mr)
            return JS_EXCEPTION;
    }
    mr->value = JS_DupValue(ctx, value);
    return JS_DupValue(ctx, this_val);
}

JSValue js_map_get(JSContext *ctx, JSValueConst this_val,
                          int argc, JSValueConst *argv, int magic)
{
    JSMapState *s = JS_GetOpaque2(ctx, this_val, JS_CLASS_MAP + magic);
    JSMapRecord *mr;
    JSValueConst key;

    if (!s)
        return JS_EXCEPTION;
    key = map_normalize_key_const(ctx, argv[0]);
    mr = map_find_record(ctx, s, key);
    if (!mr)
        return JS_UNDEFINED;
    else
        return JS_DupValue(ctx, mr->value);
}

/* return JS_TRUE or JS_FALSE */
JSValue map_delete_record(JSContext *ctx, JSMapState *s, JSValueConst key)
{
    JSMapRecord *mr, **pmr;
    uint32_t h;

    key = map_normalize_key_const(ctx, key);

    h = map_hash_key(key, s->hash_bits);
    pmr = &s->hash_table[h];
    for(;;) {
        mr = *pmr;
        if (mr == NULL)
            return JS_FALSE;
        if (mr->empty || (s->is_weak && !js_weakref_is_live(mr->key))) {
            /* not valid */
        } else {
            if (js_same_value_zero(ctx, mr->key, key))
                break;
        }
        pmr = &mr->hash_next;
    }

    /* remove from the hash table */
    *pmr = mr->hash_next;

    map_delete_record_internal(ctx->rt, s, mr);
    return JS_TRUE;
}

JSValue js_map_getOrInsert(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv, int magic)
{
    BOOL computed = magic & 1;
    JSClassID class_id = magic >> 1;
    JSMapState *s = JS_GetOpaque2(ctx, this_val, class_id);
    JSMapRecord *mr;
    JSValueConst key;
    JSValue value;

    if (!s)
        return JS_EXCEPTION;
    if (computed && !JS_IsFunction(ctx, argv[1]))
        return JS_ThrowTypeError(ctx, "not a function");
    key = map_normalize_key_const(ctx, argv[0]);
    if (s->is_weak && !js_weakref_is_target(key))
        return JS_ThrowTypeError(ctx, "invalid value used as WeakMap key");
    mr = map_find_record(ctx, s, key);
    if (!mr) {
        if (computed) {
            value = JS_Call(ctx, argv[1], JS_UNDEFINED, 1, &key);
            if (JS_IsException(value))
                return JS_EXCEPTION;
            map_delete_record(ctx, s, key);
        } else {
            value = JS_DupValue(ctx, argv[1]);
        }
        mr = map_add_record(ctx, s, key);
        if (!mr) {
            JS_FreeValue(ctx, value);
            return JS_EXCEPTION;
        }
        mr->value = value;
    }
    return JS_DupValue(ctx, mr->value);
}

JSValue js_map_has(JSContext *ctx, JSValueConst this_val,
                          int argc, JSValueConst *argv, int magic)
{
    JSMapState *s = JS_GetOpaque2(ctx, this_val, JS_CLASS_MAP + magic);
    JSMapRecord *mr;
    JSValueConst key;

    if (!s)
        return JS_EXCEPTION;
    key = map_normalize_key_const(ctx, argv[0]);
    mr = map_find_record(ctx, s, key);
    return JS_NewBool(ctx, mr != NULL);
}

JSValue js_map_delete(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv, int magic)
{
    JSMapState *s = JS_GetOpaque2(ctx, this_val, JS_CLASS_MAP + magic);
    if (!s)
        return JS_EXCEPTION;
    return map_delete_record(ctx, s, argv[0]);
}

JSValue js_map_clear(JSContext *ctx, JSValueConst this_val,
                            int argc, JSValueConst *argv, int magic)
{
    JSMapState *s = JS_GetOpaque2(ctx, this_val, JS_CLASS_MAP + magic);
    struct list_head *el, *el1;
    JSMapRecord *mr;

    if (!s)
        return JS_EXCEPTION;

    /* remove from the hash table */
    memset(s->hash_table, 0, sizeof(s->hash_table[0]) * s->hash_size);

    list_for_each_safe(el, el1, &s->records) {
        mr = list_entry(el, JSMapRecord, link);
        map_delete_record_internal(ctx->rt, s, mr);
    }
    return JS_UNDEFINED;
}

JSValue js_map_get_size(JSContext *ctx, JSValueConst this_val, int magic)
{
    JSMapState *s = JS_GetOpaque2(ctx, this_val, JS_CLASS_MAP + magic);
    if (!s)
        return JS_EXCEPTION;
    return JS_NewUint32(ctx, s->record_count);
}

JSValue js_map_forEach(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv, int magic)
{
    JSMapState *s = JS_GetOpaque2(ctx, this_val, JS_CLASS_MAP + magic);
    JSValueConst func, this_arg;
    JSValue ret, args[3];
    struct list_head *el;
    JSMapRecord *mr;

    if (!s)
        return JS_EXCEPTION;
    func = argv[0];
    if (argc > 1)
        this_arg = argv[1];
    else
        this_arg = JS_UNDEFINED;
    if (check_function(ctx, func))
        return JS_EXCEPTION;
    /* Note: the list can be modified while traversing it, but the
       current element is locked */
    el = s->records.next;
    while (el != &s->records) {
        mr = list_entry(el, JSMapRecord, link);
        if (!mr->empty) {
            mr->ref_count++;
            /* must duplicate in case the record is deleted */
            args[1] = JS_DupValue(ctx, mr->key);
            if (magic)
                args[0] = args[1];
            else
                args[0] = JS_DupValue(ctx, mr->value);
            args[2] = (JSValue)this_val;
            ret = JS_Call(ctx, func, this_arg, 3, (JSValueConst *)args);
            JS_FreeValue(ctx, args[0]);
            if (!magic)
                JS_FreeValue(ctx, args[1]);
            el = el->next;
            map_decref_record(ctx->rt, mr);
            if (JS_IsException(ret))
                return ret;
            JS_FreeValue(ctx, ret);
        } else {
            el = el->next;
        }
    }
    return JS_UNDEFINED;
}

JSValue js_object_groupBy(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv, int is_map)
{
    JSValueConst cb, args[2];
    JSValue res, iter, next, groups, key, v, prop;
    JSAtom key_atom = JS_ATOM_NULL;
    int64_t idx;
    BOOL done;

    // "is function?" check must be observed before argv[0] is accessed
    cb = argv[1];
    if (check_function(ctx, cb))
        return JS_EXCEPTION;

    iter = JS_GetIterator(ctx, argv[0], /*is_async*/FALSE);
    if (JS_IsException(iter))
        return JS_EXCEPTION;

    key = JS_UNDEFINED;
    key_atom = JS_ATOM_NULL;
    v = JS_UNDEFINED;
    prop = JS_UNDEFINED;
    groups = JS_UNDEFINED;

    next = JS_GetProperty(ctx, iter, JS_ATOM_next);
    if (JS_IsException(next))
        goto exception;

    if (is_map) {
        groups = js_map_constructor(ctx, JS_UNDEFINED, 0, NULL, 0);
    } else {
        groups = JS_NewObjectProto(ctx, JS_NULL);
    }
    if (JS_IsException(groups))
        goto exception;

    for (idx = 0; ; idx++) {
        if (idx >= MAX_SAFE_INTEGER) {
            JS_ThrowTypeError(ctx, "too many elements");
            goto iterator_close_exception;
        }
        v = JS_IteratorNext(ctx, iter, next, 0, NULL, &done);
        if (JS_IsException(v))
            goto exception;
        if (done)
            break; // v is JS_UNDEFINED

        args[0] = v;
        args[1] = JS_NewInt64(ctx, idx);
        key = JS_Call(ctx, cb, ctx->global_obj, 2, args);
        if (JS_IsException(key))
            goto iterator_close_exception;

        if (is_map) {
            prop = js_map_get(ctx, groups, 1, (JSValueConst *)&key, 0);
        } else {
            key_atom = JS_ValueToAtom(ctx, key);
            JS_FreeValue(ctx, key);
            key = JS_UNDEFINED;
            if (key_atom == JS_ATOM_NULL)
                goto iterator_close_exception;
            prop = JS_GetProperty(ctx, groups, key_atom);
        }
        if (JS_IsException(prop))
            goto exception;

        if (JS_IsUndefined(prop)) {
            prop = JS_NewArray(ctx);
            if (JS_IsException(prop))
                goto exception;
            if (is_map) {
                args[0] = key;
                args[1] = prop;
                res = js_map_set(ctx, groups, 2, args, 0);
                if (JS_IsException(res))
                    goto exception;
                JS_FreeValue(ctx, res);
            } else {
                prop = JS_DupValue(ctx, prop);
                if (JS_DefinePropertyValue(ctx, groups, key_atom, prop,
                                           JS_PROP_C_W_E) < 0) {
                    goto exception;
                }
            }
        }
        res = js_array_push(ctx, prop, 1, (JSValueConst *)&v, /*unshift*/0);
        if (JS_IsException(res))
            goto exception;
        // res is an int64

        JS_FreeValue(ctx, prop);
        JS_FreeValue(ctx, key);
        JS_FreeAtom(ctx, key_atom);
        JS_FreeValue(ctx, v);
        prop = JS_UNDEFINED;
        key = JS_UNDEFINED;
        key_atom = JS_ATOM_NULL;
        v = JS_UNDEFINED;
    }

    JS_FreeValue(ctx, iter);
    JS_FreeValue(ctx, next);
    return groups;

 iterator_close_exception:
    JS_IteratorClose(ctx, iter, TRUE);
 exception:
    JS_FreeAtom(ctx, key_atom);
    JS_FreeValue(ctx, prop);
    JS_FreeValue(ctx, key);
    JS_FreeValue(ctx, v);
    JS_FreeValue(ctx, groups);
    JS_FreeValue(ctx, iter);
    JS_FreeValue(ctx, next);
    return JS_EXCEPTION;
}

void js_map_finalizer(JSRuntime *rt, JSValue val)
{
    JSObject *p;
    JSMapState *s;
    struct list_head *el, *el1;
    JSMapRecord *mr;

    p = JS_VALUE_GET_OBJ(val);
    s = p->u.map_state;
    if (s) {
        /* if the object is deleted we are sure that no iterator is
           using it */
        list_for_each_safe(el, el1, &s->records) {
            mr = list_entry(el, JSMapRecord, link);
            if (!mr->empty) {
                if (s->is_weak)
                    js_weakref_free(rt, mr->key);
                else
                    JS_FreeValueRT(rt, mr->key);
                JS_FreeValueRT(rt, mr->value);
            }
            js_free_rt(rt, mr);
        }
        js_free_rt(rt, s->hash_table);
        if (s->is_weak) {
            list_del(&s->weakref_header.link);
        }
        js_free_rt(rt, s);
    }
}

void js_map_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JSMapState *s;
    struct list_head *el;
    JSMapRecord *mr;

    s = p->u.map_state;
    if (s) {
        list_for_each(el, &s->records) {
            mr = list_entry(el, JSMapRecord, link);
            if (!s->is_weak)
                JS_MarkValue(rt, mr->key, mark_func);
            JS_MarkValue(rt, mr->value, mark_func);
        }
    }
}

/* Map Iterator */

void js_map_iterator_finalizer(JSRuntime *rt, JSValue val)
{
    JSObject *p;
    JSMapIteratorData *it;

    p = JS_VALUE_GET_OBJ(val);
    it = p->u.map_iterator_data;
    if (it) {
        /* During the GC sweep phase the Map finalizer may be
           called before the Map iterator finalizer */
        if (JS_IsLiveObject(rt, it->obj) && it->cur_record) {
            map_decref_record(rt, it->cur_record);
        }
        JS_FreeValueRT(rt, it->obj);
        js_free_rt(rt, it);
    }
}

void js_map_iterator_mark(JSRuntime *rt, JSValueConst val,
                                 JS_MarkFunc *mark_func)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JSMapIteratorData *it;
    it = p->u.map_iterator_data;
    if (it) {
        /* the record is already marked by the object */
        JS_MarkValue(rt, it->obj, mark_func);
    }
}

JSValue js_create_map_iterator(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv, int magic)
{
    JSIteratorKindEnum kind;
    JSMapState *s;
    JSMapIteratorData *it;
    JSValue enum_obj;

    kind = magic >> 2;
    magic &= 3;
    s = JS_GetOpaque2(ctx, this_val, JS_CLASS_MAP + magic);
    if (!s)
        return JS_EXCEPTION;
    enum_obj = JS_NewObjectClass(ctx, JS_CLASS_MAP_ITERATOR + magic);
    if (JS_IsException(enum_obj))
        goto fail;
    it = js_malloc(ctx, sizeof(*it));
    if (!it) {
        JS_FreeValue(ctx, enum_obj);
        goto fail;
    }
    it->obj = JS_DupValue(ctx, this_val);
    it->kind = kind;
    it->cur_record = NULL;
    JS_SetOpaque(enum_obj, it);
    return enum_obj;
 fail:
    return JS_EXCEPTION;
}

JSValue js_map_iterator_next(JSContext *ctx, JSValueConst this_val,
                                    int argc, JSValueConst *argv,
                                    BOOL *pdone, int magic)
{
    JSMapIteratorData *it;
    JSMapState *s;
    JSMapRecord *mr;
    struct list_head *el;

    it = JS_GetOpaque2(ctx, this_val, JS_CLASS_MAP_ITERATOR + magic);
    if (!it) {
        *pdone = FALSE;
        return JS_EXCEPTION;
    }
    if (JS_IsUndefined(it->obj))
        goto done;
    s = JS_GetOpaque(it->obj, JS_CLASS_MAP + magic);
    assert(s != NULL);
    if (!it->cur_record) {
        el = s->records.next;
    } else {
        mr = it->cur_record;
        el = mr->link.next;
        map_decref_record(ctx->rt, mr); /* the record can be freed here */
    }
    for(;;) {
        if (el == &s->records) {
            /* no more record  */
            it->cur_record = NULL;
            JS_FreeValue(ctx, it->obj);
            it->obj = JS_UNDEFINED;
        done:
            /* end of enumeration */
            *pdone = TRUE;
            return JS_UNDEFINED;
        }
        mr = list_entry(el, JSMapRecord, link);
        if (!mr->empty)
            break;
        /* get the next record */
        el = mr->link.next;
    }

    /* lock the record so that it won't be freed */
    mr->ref_count++;
    it->cur_record = mr;
    *pdone = FALSE;

    if (it->kind == JS_ITERATOR_KIND_KEY) {
        return JS_DupValue(ctx, mr->key);
    } else {
        JSValueConst args[2];
        args[0] = mr->key;
        if (magic)
            args[1] = mr->key;
        else
            args[1] = mr->value;
        if (it->kind == JS_ITERATOR_KIND_VALUE) {
            return JS_DupValue(ctx, args[1]);
        } else {
            return js_create_array(ctx, 2, args);
        }
    }
}

int get_set_record(JSContext *ctx, JSValueConst obj,
                          int64_t *psize, JSValue *phas, JSValue *pkeys)
{
    JSMapState *s;
    int64_t size;
    JSValue has = JS_UNDEFINED, keys = JS_UNDEFINED;

    s = JS_GetOpaque(obj, JS_CLASS_SET);
    if (s) {
        size = s->record_count;
    } else {
        JSValue v;
        double d;

        v = JS_GetProperty(ctx, obj, JS_ATOM_size);
        if (JS_IsException(v))
            goto exception;
        if (JS_ToFloat64Free(ctx, &d, v) < 0)
            goto exception;
        if (isnan(d)) {
            JS_ThrowTypeError(ctx, ".size is not a number");
            goto exception;
        }
        if (d < INT64_MIN)
            size = INT64_MIN;
        else if (d >= 0x1p63) /* must use INT64_MAX + 1 because INT64_MAX cannot be exactly represented as a double */
            size = INT64_MAX;
        else
            size = (int64_t)d;
        if (size < 0) {
            JS_ThrowRangeError(ctx, ".size must be positive");
            goto exception;
        }
    }

    has = JS_GetProperty(ctx, obj, JS_ATOM_has);
    if (JS_IsException(has))
        goto exception;
    if (JS_IsUndefined(has)) {
        JS_ThrowTypeError(ctx, ".has is undefined");
        goto exception;
    }
    if (! JS_IsFunction(ctx, has)) {
        JS_ThrowTypeError(ctx, ".has is not a function");
        goto exception;
    }

    keys = JS_GetProperty(ctx, obj, JS_ATOM_keys);
    if (JS_IsException(keys))
        goto exception;
    if (JS_IsUndefined(keys)) {
        JS_ThrowTypeError(ctx, ".keys is undefined");
        goto exception;
    }
    if (! JS_IsFunction(ctx, keys)) {
        JS_ThrowTypeError(ctx, ".keys is not a function");
        goto exception;
    }
    *psize = size;
    *phas = has;
    *pkeys = keys;
    return 0;

 exception:
    JS_FreeValue(ctx, has);
    JS_FreeValue(ctx, keys);
    *psize = 0;
    *phas = JS_UNDEFINED;
    *pkeys = JS_UNDEFINED;
    return -1;
}

/* copy 'this_val' in a new set without side effects */
JSValue js_copy_set(JSContext *ctx, JSValueConst this_val)
{
    JSValue newset;
    JSMapState *s, *t;
    struct list_head *el;
    JSMapRecord *mr;

    s = JS_GetOpaque2(ctx, this_val, JS_CLASS_SET);
    if (!s)
        return JS_EXCEPTION;

    newset = js_map_constructor(ctx, JS_UNDEFINED, 0, NULL, MAGIC_SET);
    if (JS_IsException(newset))
        return JS_EXCEPTION;
    t = JS_GetOpaque(newset, JS_CLASS_SET);

    // can't clone this_val using js_map_constructor(),
    // test262 mandates we don't call the .add method
    list_for_each(el, &s->records) {
        mr = list_entry(el, JSMapRecord, link);
        if (mr->empty)
            continue;
        if (!set_add_record(ctx, t, mr->key))
            goto exception;
    }
    return newset;
 exception:
    JS_FreeValue(ctx, newset);
    return JS_EXCEPTION;
}

JSValue js_set_isDisjointFrom(JSContext *ctx, JSValueConst this_val,
                                     int argc, JSValueConst *argv)
{
    JSValue item, iter, keys, has, next, rv, rval;
    int done;
    BOOL found;
    JSMapState *s;
    int64_t size;
    int ok;

    iter = JS_UNDEFINED;
    next = JS_UNDEFINED;
    rval = JS_EXCEPTION;
    s = JS_GetOpaque2(ctx, this_val, JS_CLASS_SET);
    if (!s)
        return JS_EXCEPTION;
    if (get_set_record(ctx, argv[0], &size, &has, &keys) < 0)
        goto exception;
    if (s->record_count <= size) {
        iter = js_create_map_iterator(ctx, this_val, 0, NULL, MAGIC_SET);
        if (JS_IsException(iter))
            goto exception;
        found = FALSE;
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
        } while (!found);
    } else {
        iter = JS_Call(ctx, keys, argv[0], 0, NULL);
        if (JS_IsException(iter))
            goto exception;
        next = JS_GetProperty(ctx, iter, JS_ATOM_next);
        if (JS_IsException(next))
            goto exception;
        found = FALSE;
        for(;;) {
            item = JS_IteratorNext(ctx, iter, next, 0, NULL, &done);
            if (JS_IsException(item))
                goto exception;
            if (done) // item is JS_UNDEFINED
                break;
            item = map_normalize_key(ctx, item);
            found = (NULL != map_find_record(ctx, s, item));
            JS_FreeValue(ctx, item);
            if (found) {
                JS_IteratorClose(ctx, iter, FALSE);
                break;
            }
        }
    }
    rval = !found ? JS_TRUE : JS_FALSE;
exception:
    JS_FreeValue(ctx, has);
    JS_FreeValue(ctx, keys);
    JS_FreeValue(ctx, iter);
    JS_FreeValue(ctx, next);
    return rval;
}
