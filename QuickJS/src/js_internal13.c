#include "js_internal.h"



JSValue js_object_isExtensible(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv, int reflect)
{
    JSValueConst obj;
    int ret;

    obj = argv[0];
    if (JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT) {
        if (reflect)
            return JS_ThrowTypeErrorNotAnObject(ctx);
        else
            return JS_FALSE;
    }
    ret = JS_IsExtensible(ctx, obj);
    if (ret < 0)
        return JS_EXCEPTION;
    else
        return JS_NewBool(ctx, ret);
}

JSValue js_object_preventExtensions(JSContext *ctx, JSValueConst this_val,
                                           int argc, JSValueConst *argv, int reflect)
{
    JSValueConst obj;
    int ret;

    obj = argv[0];
    if (JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT) {
        if (reflect)
            return JS_ThrowTypeErrorNotAnObject(ctx);
        else
            return JS_DupValue(ctx, obj);
    }
    ret = JS_PreventExtensions(ctx, obj);
    if (ret < 0)
        return JS_EXCEPTION;
    if (reflect) {
        return JS_NewBool(ctx, ret);
    } else {
        if (!ret)
            return JS_ThrowTypeError(ctx, "proxy preventExtensions handler returned false");
        return JS_DupValue(ctx, obj);
    }
}

JSValue js_object_hasOwnProperty(JSContext *ctx, JSValueConst this_val,
                                        int argc, JSValueConst *argv)
{
    JSValue obj;
    JSAtom atom;
    JSObject *p;
    BOOL ret;

    atom = JS_ValueToAtom(ctx, argv[0]); /* must be done first */
    if (unlikely(atom == JS_ATOM_NULL))
        return JS_EXCEPTION;
    obj = JS_ToObject(ctx, this_val);
    if (JS_IsException(obj)) {
        JS_FreeAtom(ctx, atom);
        return obj;
    }
    p = JS_VALUE_GET_OBJ(obj);
    ret = JS_GetOwnPropertyInternal(ctx, NULL, p, atom);
    JS_FreeAtom(ctx, atom);
    JS_FreeValue(ctx, obj);
    if (ret < 0)
        return JS_EXCEPTION;
    else
        return JS_NewBool(ctx, ret);
}

JSValue js_object_hasOwn(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv)
{
    JSValue obj;
    JSAtom atom;
    JSObject *p;
    BOOL ret;

    obj = JS_ToObject(ctx, argv[0]);
    if (JS_IsException(obj))
        return obj;
    atom = JS_ValueToAtom(ctx, argv[1]);
    if (unlikely(atom == JS_ATOM_NULL)) {
        JS_FreeValue(ctx, obj);
        return JS_EXCEPTION;
    }
    p = JS_VALUE_GET_OBJ(obj);
    ret = JS_GetOwnPropertyInternal(ctx, NULL, p, atom);
    JS_FreeAtom(ctx, atom);
    JS_FreeValue(ctx, obj);
    if (ret < 0)
        return JS_EXCEPTION;
    else
        return JS_NewBool(ctx, ret);
}

JSValue js_object_valueOf(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv)
{
    return JS_ToObject(ctx, this_val);
}

JSValue js_object_toString(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv)
{
    JSValue obj, tag;
    int is_array;
    JSAtom atom;
    JSObject *p;

    if (JS_IsNull(this_val)) {
        tag = js_new_string8(ctx, "Null");
    } else if (JS_IsUndefined(this_val)) {
        tag = js_new_string8(ctx, "Undefined");
    } else {
        obj = JS_ToObject(ctx, this_val);
        if (JS_IsException(obj))
            return obj;
        is_array = JS_IsArray(ctx, obj);
        if (is_array < 0) {
            JS_FreeValue(ctx, obj);
            return JS_EXCEPTION;
        }
        if (is_array) {
            atom = JS_ATOM_Array;
        } else if (JS_IsFunction(ctx, obj)) {
            atom = JS_ATOM_Function;
        } else {
            p = JS_VALUE_GET_OBJ(obj);
            switch(p->class_id) {
            case JS_CLASS_STRING:
            case JS_CLASS_ARGUMENTS:
            case JS_CLASS_MAPPED_ARGUMENTS:
            case JS_CLASS_ERROR:
            case JS_CLASS_BOOLEAN:
            case JS_CLASS_NUMBER:
            case JS_CLASS_DATE:
            case JS_CLASS_REGEXP:
                atom = ctx->rt->class_array[p->class_id].class_name;
                break;
            default:
                atom = JS_ATOM_Object;
                break;
            }
        }
        tag = JS_GetProperty(ctx, obj, JS_ATOM_Symbol_toStringTag);
        JS_FreeValue(ctx, obj);
        if (JS_IsException(tag))
            return JS_EXCEPTION;
        if (!JS_IsString(tag)) {
            JS_FreeValue(ctx, tag);
            tag = JS_AtomToString(ctx, atom);
        }
    }
    return JS_ConcatString3(ctx, "[object ", tag, "]");
}

JSValue js_object_toLocaleString(JSContext *ctx, JSValueConst this_val,
                                        int argc, JSValueConst *argv)
{
    return JS_Invoke(ctx, this_val, JS_ATOM_toString, 0, NULL);
}

JSValue js_object_assign(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv)
{
    // Object.assign(obj, source1)
    JSValue obj, s;
    int i;

    s = JS_UNDEFINED;
    obj = JS_ToObject(ctx, argv[0]);
    if (JS_IsException(obj))
        goto exception;
    for (i = 1; i < argc; i++) {
        if (!JS_IsNull(argv[i]) && !JS_IsUndefined(argv[i])) {
            s = JS_ToObject(ctx, argv[i]);
            if (JS_IsException(s))
                goto exception;
            if (JS_CopyDataProperties(ctx, obj, s, JS_UNDEFINED, TRUE))
                goto exception;
            JS_FreeValue(ctx, s);
        }
    }
    return obj;
exception:
    JS_FreeValue(ctx, obj);
    JS_FreeValue(ctx, s);
    return JS_EXCEPTION;
}

JSValue js_object_seal(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv, int freeze_flag)
{
    JSValueConst obj = argv[0];
    JSObject *p;
    JSPropertyEnum *props;
    uint32_t len, i;
    int flags, desc_flags, res;

    if (!JS_IsObject(obj))
        return JS_DupValue(ctx, obj);

    res = JS_PreventExtensions(ctx, obj);
    if (res < 0)
        return JS_EXCEPTION;
    if (!res) {
        return JS_ThrowTypeError(ctx, "proxy preventExtensions handler returned false");
    }

    p = JS_VALUE_GET_OBJ(obj);
    flags = JS_GPN_STRING_MASK | JS_GPN_SYMBOL_MASK;
    if (JS_GetOwnPropertyNamesInternal(ctx, &props, &len, p, flags))
        return JS_EXCEPTION;

    for(i = 0; i < len; i++) {
        JSPropertyDescriptor desc;
        JSAtom prop = props[i].atom;

        desc_flags = JS_PROP_THROW | JS_PROP_HAS_CONFIGURABLE;
        if (freeze_flag) {
            res = JS_GetOwnPropertyInternal(ctx, &desc, p, prop);
            if (res < 0)
                goto exception;
            if (res) {
                if (desc.flags & JS_PROP_WRITABLE)
                    desc_flags |= JS_PROP_HAS_WRITABLE;
                js_free_desc(ctx, &desc);
            }
        }
        if (JS_DefineProperty(ctx, obj, prop, JS_UNDEFINED,
                              JS_UNDEFINED, JS_UNDEFINED, desc_flags) < 0)
            goto exception;
    }
    JS_FreePropertyEnum(ctx, props, len);
    return JS_DupValue(ctx, obj);

 exception:
    JS_FreePropertyEnum(ctx, props, len);
    return JS_EXCEPTION;
}

JSValue js_object_isSealed(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv, int is_frozen)
{
    JSValueConst obj = argv[0];
    JSObject *p;
    JSPropertyEnum *props;
    uint32_t len, i;
    int flags, res;

    if (!JS_IsObject(obj))
        return JS_TRUE;

    p = JS_VALUE_GET_OBJ(obj);
    flags = JS_GPN_STRING_MASK | JS_GPN_SYMBOL_MASK;
    if (JS_GetOwnPropertyNamesInternal(ctx, &props, &len, p, flags))
        return JS_EXCEPTION;

    for(i = 0; i < len; i++) {
        JSPropertyDescriptor desc;
        JSAtom prop = props[i].atom;

        res = JS_GetOwnPropertyInternal(ctx, &desc, p, prop);
        if (res < 0)
            goto exception;
        if (res) {
            js_free_desc(ctx, &desc);
            if ((desc.flags & JS_PROP_CONFIGURABLE)
            ||  (is_frozen && (desc.flags & JS_PROP_WRITABLE))) {
                res = FALSE;
                goto done;
            }
        }
    }
    res = JS_IsExtensible(ctx, obj);
    if (res < 0)
        return JS_EXCEPTION;
    res ^= 1;
done:
    JS_FreePropertyEnum(ctx, props, len);
    return JS_NewBool(ctx, res);

exception:
    JS_FreePropertyEnum(ctx, props, len);
    return JS_EXCEPTION;
}

JSValue js_object_fromEntries(JSContext *ctx, JSValueConst this_val,
                                     int argc, JSValueConst *argv)
{
    JSValue obj, iter, next_method = JS_UNDEFINED;
    JSValueConst iterable;
    BOOL done;

    /*  RequireObjectCoercible() not necessary because it is tested in
        JS_GetIterator() by JS_GetProperty() */
    iterable = argv[0];

    obj = JS_NewObject(ctx);
    if (JS_IsException(obj))
        return obj;

    iter = JS_GetIterator(ctx, iterable, FALSE);
    if (JS_IsException(iter))
        goto fail;
    next_method = JS_GetProperty(ctx, iter, JS_ATOM_next);
    if (JS_IsException(next_method))
        goto fail;

    for(;;) {
        JSValue key, value, item;
        item = JS_IteratorNext(ctx, iter, next_method, 0, NULL, &done);
        if (JS_IsException(item))
            goto fail;
        if (done)
            break;

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
        if (JS_IsException(value)) {
            JS_FreeValue(ctx, key);
            goto fail1;
        }
        if (JS_DefinePropertyValueValue(ctx, obj, key, value,
                                        JS_PROP_C_W_E | JS_PROP_THROW) < 0) {
        fail1:
            JS_FreeValue(ctx, item);
            goto fail;
        }
        JS_FreeValue(ctx, item);
    }
    JS_FreeValue(ctx, next_method);
    JS_FreeValue(ctx, iter);
    return obj;
 fail:
    if (JS_IsObject(iter)) {
        /* close the iterator object, preserving pending exception */
        JS_IteratorClose(ctx, iter, TRUE);
    }
    JS_FreeValue(ctx, next_method);
    JS_FreeValue(ctx, iter);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

JSValue js_object_is(JSContext *ctx, JSValueConst this_val,
                            int argc, JSValueConst *argv)
{
    return JS_NewBool(ctx, js_same_value(ctx, argv[0], argv[1]));
}

JSValue JS_SpeciesConstructor(JSContext *ctx, JSValueConst obj,
                                     JSValueConst defaultConstructor)
{
    JSValue ctor, species;

    if (!JS_IsObject(obj))
        return JS_ThrowTypeErrorNotAnObject(ctx);
    ctor = JS_GetProperty(ctx, obj, JS_ATOM_constructor);
    if (JS_IsException(ctor))
        return ctor;
    if (JS_IsUndefined(ctor))
        return JS_DupValue(ctx, defaultConstructor);
    if (!JS_IsObject(ctor)) {
        JS_FreeValue(ctx, ctor);
        return JS_ThrowTypeErrorNotAnObject(ctx);
    }
    species = JS_GetProperty(ctx, ctor, JS_ATOM_Symbol_species);
    JS_FreeValue(ctx, ctor);
    if (JS_IsException(species))
        return species;
    if (JS_IsUndefined(species) || JS_IsNull(species))
        return JS_DupValue(ctx, defaultConstructor);
    if (! JS_IsConstructor(ctx, species)) {
        JS_ThrowTypeErrorNotAConstructor(ctx, species);
        JS_FreeValue(ctx, species);
        return JS_EXCEPTION;
    }
    return species;
}

JSValue js_object_get___proto__(JSContext *ctx, JSValueConst this_val)
{
    JSValue val, ret;

    val = JS_ToObject(ctx, this_val);
    if (JS_IsException(val))
        return val;
    ret = JS_GetPrototype(ctx, val);
    JS_FreeValue(ctx, val);
    return ret;
}

JSValue js_object_set___proto__(JSContext *ctx, JSValueConst this_val,
                                       JSValueConst proto)
{
    if (JS_IsUndefined(this_val) || JS_IsNull(this_val))
        return JS_ThrowTypeErrorNotAnObject(ctx);
    if (!JS_IsObject(proto) && !JS_IsNull(proto))
        return JS_UNDEFINED;
    if (JS_SetPrototypeInternal(ctx, this_val, proto, TRUE) < 0)
        return JS_EXCEPTION;
    else
        return JS_UNDEFINED;
}

JSValue js_object_isPrototypeOf(JSContext *ctx, JSValueConst this_val,
                                       int argc, JSValueConst *argv)
{
    JSValue obj, v1;
    JSValueConst v;
    int res;

    v = argv[0];
    if (!JS_IsObject(v))
        return JS_FALSE;
    obj = JS_ToObject(ctx, this_val);
    if (JS_IsException(obj))
        return JS_EXCEPTION;
    v1 = JS_DupValue(ctx, v);
    for(;;) {
        v1 = JS_GetPrototypeFree(ctx, v1);
        if (JS_IsException(v1))
            goto exception;
        if (JS_IsNull(v1)) {
            res = FALSE;
            break;
        }
        if (JS_VALUE_GET_OBJ(obj) == JS_VALUE_GET_OBJ(v1)) {
            res = TRUE;
            break;
        }
        /* avoid infinite loop (possible with proxies) */
        if (js_poll_interrupts(ctx))
            goto exception;
    }
    JS_FreeValue(ctx, v1);
    JS_FreeValue(ctx, obj);
    return JS_NewBool(ctx, res);

exception:
    JS_FreeValue(ctx, v1);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

JSValue js_object_propertyIsEnumerable(JSContext *ctx, JSValueConst this_val,
                                              int argc, JSValueConst *argv)
{
    JSValue obj = JS_UNDEFINED, res = JS_EXCEPTION;
    JSAtom prop;
    JSPropertyDescriptor desc;
    int has_prop;

    prop = JS_ValueToAtom(ctx, argv[0]);
    if (unlikely(prop == JS_ATOM_NULL))
        goto exception;
    obj = JS_ToObject(ctx, this_val);
    if (JS_IsException(obj))
        goto exception;

    has_prop = JS_GetOwnPropertyInternal(ctx, &desc, JS_VALUE_GET_OBJ(obj), prop);
    if (has_prop < 0)
        goto exception;
    if (has_prop) {
        res = JS_NewBool(ctx, desc.flags & JS_PROP_ENUMERABLE);
        js_free_desc(ctx, &desc);
    } else {
        res = JS_FALSE;
    }

exception:
    JS_FreeAtom(ctx, prop);
    JS_FreeValue(ctx, obj);
    return res;
}

JSValue js_object___lookupGetter__(JSContext *ctx, JSValueConst this_val,
                                          int argc, JSValueConst *argv, int setter)
{
    JSValue obj, res = JS_EXCEPTION;
    JSAtom prop = JS_ATOM_NULL;
    JSPropertyDescriptor desc;
    int has_prop;

    obj = JS_ToObject(ctx, this_val);
    if (JS_IsException(obj))
        goto exception;
    prop = JS_ValueToAtom(ctx, argv[0]);
    if (unlikely(prop == JS_ATOM_NULL))
        goto exception;

    for (;;) {
        has_prop = JS_GetOwnPropertyInternal(ctx, &desc, JS_VALUE_GET_OBJ(obj), prop);
        if (has_prop < 0)
            goto exception;
        if (has_prop) {
            if (desc.flags & JS_PROP_GETSET)
                res = JS_DupValue(ctx, setter ? desc.setter : desc.getter);
            else
                res = JS_UNDEFINED;
            js_free_desc(ctx, &desc);
            break;
        }
        obj = JS_GetPrototypeFree(ctx, obj);
        if (JS_IsException(obj))
            goto exception;
        if (JS_IsNull(obj)) {
            res = JS_UNDEFINED;
            break;
        }
        /* avoid infinite loop (possible with proxies) */
        if (js_poll_interrupts(ctx))
            goto exception;
    }

exception:
    JS_FreeAtom(ctx, prop);
    JS_FreeValue(ctx, obj);
    return res;
}

/* Function class */

JSValue js_function_proto(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv)
{
    return JS_UNDEFINED;
}

/* XXX: add a specific eval mode so that Function("}), ({") is rejected */
JSValue js_function_constructor(JSContext *ctx, JSValueConst new_target,
                                       int argc, JSValueConst *argv, int magic)
{
    JSFunctionKindEnum func_kind = magic;
    int i, n, ret;
    JSValue s, proto, obj = JS_UNDEFINED;
    StringBuffer b_s, *b = &b_s;

    string_buffer_init(ctx, b, 0);
    string_buffer_putc8(b, '(');

    if (func_kind == JS_FUNC_ASYNC || func_kind == JS_FUNC_ASYNC_GENERATOR) {
        string_buffer_puts8(b, "async ");
    }
    string_buffer_puts8(b, "function");

    if (func_kind == JS_FUNC_GENERATOR || func_kind == JS_FUNC_ASYNC_GENERATOR) {
        string_buffer_putc8(b, '*');
    }
    string_buffer_puts8(b, " anonymous(");

    n = argc - 1;
    for(i = 0; i < n; i++) {
        if (i != 0) {
            string_buffer_putc8(b, ',');
        }
        if (string_buffer_concat_value(b, argv[i]))
            goto fail;
    }
    string_buffer_puts8(b, "\n) {\n");
    if (n >= 0) {
        if (string_buffer_concat_value(b, argv[n]))
            goto fail;
    }
    string_buffer_puts8(b, "\n})");
    s = string_buffer_end(b);
    if (JS_IsException(s))
        goto fail1;

    obj = JS_EvalObject(ctx, ctx->global_obj, s, JS_EVAL_TYPE_INDIRECT, -1);
    JS_FreeValue(ctx, s);
    if (JS_IsException(obj))
        goto fail1;
    if (!JS_IsUndefined(new_target)) {
        /* set the prototype */
        proto = JS_GetProperty(ctx, new_target, JS_ATOM_prototype);
        if (JS_IsException(proto))
            goto fail1;
        if (!JS_IsObject(proto)) {
            JSContext *realm;
            JS_FreeValue(ctx, proto);
            realm = JS_GetFunctionRealm(ctx, new_target);
            if (!realm)
                goto fail1;
            proto = JS_DupValue(ctx, realm->class_proto[func_kind_to_class_id[func_kind]]);
        }
        ret = JS_SetPrototypeInternal(ctx, obj, proto, TRUE);
        JS_FreeValue(ctx, proto);
        if (ret < 0)
            goto fail1;
    }
    return obj;

 fail:
    string_buffer_free(b);
 fail1:
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

__exception int js_get_length32(JSContext *ctx, uint32_t *pres,
                                       JSValueConst obj)
{
    JSValue len_val;
    len_val = JS_GetProperty(ctx, obj, JS_ATOM_length);
    if (JS_IsException(len_val)) {
        *pres = 0;
        return -1;
    }
    return JS_ToUint32Free(ctx, pres, len_val);
}

__exception int js_get_length64(JSContext *ctx, int64_t *pres,
                                       JSValueConst obj)
{
    JSValue len_val;
    len_val = JS_GetProperty(ctx, obj, JS_ATOM_length);
    if (JS_IsException(len_val)) {
        *pres = 0;
        return -1;
    }
    return JS_ToLengthFree(ctx, pres, len_val);
}

void free_arg_list(JSContext *ctx, JSValue *tab, uint32_t len)
{
    uint32_t i;
    for(i = 0; i < len; i++) {
        JS_FreeValue(ctx, tab[i]);
    }
    js_free(ctx, tab);
}

/* XXX: should use ValueArray */
JSValue *build_arg_list(JSContext *ctx, uint32_t *plen,
                               JSValueConst array_arg)
{
    uint32_t len, i;
    int64_t len64;
    JSValue *tab, ret;
    JSObject *p;

    if (JS_VALUE_GET_TAG(array_arg) != JS_TAG_OBJECT) {
        JS_ThrowTypeError(ctx, "not a object");
        return NULL;
    }
    if (js_get_length64(ctx, &len64, array_arg))
        return NULL;
    if (len64 > JS_MAX_LOCAL_VARS) {
        // XXX: check for stack overflow?
        JS_ThrowRangeError(ctx, "too many arguments in function call (only %d allowed)",
                           JS_MAX_LOCAL_VARS);
        return NULL;
    }
    len = len64;
    /* avoid allocating 0 bytes */
    tab = js_mallocz(ctx, sizeof(tab[0]) * max_uint32(1, len));
    if (!tab)
        return NULL;
    p = JS_VALUE_GET_OBJ(array_arg);
    if ((p->class_id == JS_CLASS_ARRAY || p->class_id == JS_CLASS_ARGUMENTS || p->class_id == JS_CLASS_MAPPED_ARGUMENTS) &&
        p->fast_array &&
        len == p->u.array.count) {
        if (p->class_id == JS_CLASS_MAPPED_ARGUMENTS) {
            for(i = 0; i < len; i++) {
                tab[i] = JS_DupValue(ctx, *p->u.array.u.var_refs[i]->pvalue);
            }
        } else {
            for(i = 0; i < len; i++) {
                tab[i] = JS_DupValue(ctx, p->u.array.u.values[i]);
            }
        }
    } else {
        for(i = 0; i < len; i++) {
            ret = JS_GetPropertyUint32(ctx, array_arg, i);
            if (JS_IsException(ret)) {
                free_arg_list(ctx, tab, i);
                return NULL;
            }
            tab[i] = ret;
        }
    }
    *plen = len;
    return tab;
}

/* magic value: 0 = normal apply, 1 = apply for constructor, 2 =
   Reflect.apply */
JSValue js_function_apply(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv, int magic)
{
    JSValueConst this_arg, array_arg;
    uint32_t len;
    JSValue *tab, ret;

    if (check_function(ctx, this_val))
        return JS_EXCEPTION;
    this_arg = argv[0];
    array_arg = argv[1];
    if ((JS_VALUE_GET_TAG(array_arg) == JS_TAG_UNDEFINED ||
         JS_VALUE_GET_TAG(array_arg) == JS_TAG_NULL) && magic != 2) {
        return JS_Call(ctx, this_val, this_arg, 0, NULL);
    }
    tab = build_arg_list(ctx, &len, array_arg);
    if (!tab)
        return JS_EXCEPTION;
    if (magic & 1) {
        ret = JS_CallConstructor2(ctx, this_val, this_arg, len, (JSValueConst *)tab);
    } else {
        ret = JS_Call(ctx, this_val, this_arg, len, (JSValueConst *)tab);
    }
    free_arg_list(ctx, tab, len);
    return ret;
}

JSValue js_function_call(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv)
{
    if (argc <= 0) {
        return JS_Call(ctx, this_val, JS_UNDEFINED, 0, NULL);
    } else {
        return JS_Call(ctx, this_val, argv[0], argc - 1, argv + 1);
    }
}

JSValue js_function_bind(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv)
{
    JSBoundFunction *bf;
    JSValue func_obj, name1, len_val;
    JSObject *p;
    int arg_count, i, ret;

    if (check_function(ctx, this_val))
        return JS_EXCEPTION;

    func_obj = JS_NewObjectProtoClass(ctx, ctx->function_proto,
                                 JS_CLASS_BOUND_FUNCTION);
    if (JS_IsException(func_obj))
        return JS_EXCEPTION;
    p = JS_VALUE_GET_OBJ(func_obj);
    p->is_constructor = JS_IsConstructor(ctx, this_val);
    arg_count = max_int(0, argc - 1);
    bf = js_malloc(ctx, sizeof(*bf) + arg_count * sizeof(JSValue));
    if (!bf)
        goto exception;
    bf->func_obj = JS_DupValue(ctx, this_val);
    bf->this_val = JS_DupValue(ctx, argv[0]);
    bf->argc = arg_count;
    for(i = 0; i < arg_count; i++) {
        bf->argv[i] = JS_DupValue(ctx, argv[i + 1]);
    }
    p->u.bound_function = bf;

    /* XXX: the spec could be simpler by only using GetOwnProperty */
    ret = JS_GetOwnProperty(ctx, NULL, this_val, JS_ATOM_length);
    if (ret < 0)
        goto exception;
    if (!ret) {
        len_val = JS_NewInt32(ctx, 0);
    } else {
        len_val = JS_GetProperty(ctx, this_val, JS_ATOM_length);
        if (JS_IsException(len_val))
            goto exception;
        if (JS_VALUE_GET_TAG(len_val) == JS_TAG_INT) {
            /* most common case */
            int len1 = JS_VALUE_GET_INT(len_val);
            if (len1 <= arg_count)
                len1 = 0;
            else
                len1 -= arg_count;
            len_val = JS_NewInt32(ctx, len1);
        } else if (JS_VALUE_GET_NORM_TAG(len_val) == JS_TAG_FLOAT64) {
            double d = JS_VALUE_GET_FLOAT64(len_val);
            if (isnan(d)) {
                d = 0.0;
            } else {
                d = trunc(d);
                if (d <= (double)arg_count)
                    d = 0.0;
                else
                    d -= (double)arg_count; /* also converts -0 to +0 */
            }
            len_val = JS_NewFloat64(ctx, d);
        } else {
            JS_FreeValue(ctx, len_val);
            len_val = JS_NewInt32(ctx, 0);
        }
    }
    JS_DefinePropertyValue(ctx, func_obj, JS_ATOM_length,
                           len_val, JS_PROP_CONFIGURABLE);

    name1 = JS_GetProperty(ctx, this_val, JS_ATOM_name);
    if (JS_IsException(name1))
        goto exception;
    if (!JS_IsString(name1)) {
        JS_FreeValue(ctx, name1);
        name1 = JS_AtomToString(ctx, JS_ATOM_empty_string);
    }
    name1 = JS_ConcatString3(ctx, "bound ", name1, "");
    if (JS_IsException(name1))
        goto exception;
    JS_DefinePropertyValue(ctx, func_obj, JS_ATOM_name, name1,
                           JS_PROP_CONFIGURABLE);
    return func_obj;
 exception:
    JS_FreeValue(ctx, func_obj);
    return JS_EXCEPTION;
}

JSValue js_function_toString(JSContext *ctx, JSValueConst this_val,
                                    int argc, JSValueConst *argv)
{
    JSObject *p;
    JSFunctionKindEnum func_kind = JS_FUNC_NORMAL;

    if (check_function(ctx, this_val))
        return JS_EXCEPTION;

    p = JS_VALUE_GET_OBJ(this_val);
    if (js_class_has_bytecode(p->class_id)) {
        JSFunctionBytecode *b = p->u.func.function_bytecode;
        if (b->has_debug && b->debug.source) {
            return JS_NewStringLen(ctx, b->debug.source, b->debug.source_len);
        }
        func_kind = b->func_kind;
    }
    {
        JSValue name;
        const char *pref, *suff;

        switch(func_kind) {
        default:
        case JS_FUNC_NORMAL:
            pref = "function ";
            break;
        case JS_FUNC_GENERATOR:
            pref = "function *";
            break;
        case JS_FUNC_ASYNC:
            pref = "async function ";
            break;
        case JS_FUNC_ASYNC_GENERATOR:
            pref = "async function *";
            break;
        }
        suff = "() {\n    [native code]\n}";
        name = JS_GetProperty(ctx, this_val, JS_ATOM_name);
        if (JS_IsUndefined(name))
            name = JS_AtomToString(ctx, JS_ATOM_empty_string);
        return JS_ConcatString3(ctx, pref, name, suff);
    }
}

JSValue js_function_hasInstance(JSContext *ctx, JSValueConst this_val,
                                       int argc, JSValueConst *argv)
{
    int ret;
    ret = JS_OrdinaryIsInstanceOf(ctx, argv[0], this_val);
    if (ret < 0)
        return JS_EXCEPTION;
    else
        return JS_NewBool(ctx, ret);
}

/* Error class */

JSValue iterator_to_array(JSContext *ctx, JSValueConst items)
{
    JSValue iter, next_method = JS_UNDEFINED;
    JSValue v, r = JS_UNDEFINED;
    int64_t k;
    BOOL done;

    iter = JS_GetIterator(ctx, items, FALSE);
    if (JS_IsException(iter))
        goto exception;
    next_method = JS_GetProperty(ctx, iter, JS_ATOM_next);
    if (JS_IsException(next_method))
        goto exception;
    r = JS_NewArray(ctx);
    if (JS_IsException(r))
        goto exception;
    for (k = 0;; k++) {
        v = JS_IteratorNext(ctx, iter, next_method, 0, NULL, &done);
        if (JS_IsException(v))
            goto exception_close;
        if (done)
            break;
        if (JS_DefinePropertyValueInt64(ctx, r, k, v,
                                        JS_PROP_C_W_E | JS_PROP_THROW) < 0)
            goto exception_close;
    }
 done:
    JS_FreeValue(ctx, next_method);
    JS_FreeValue(ctx, iter);
    return r;
 exception_close:
    JS_IteratorClose(ctx, iter, TRUE);
 exception:
    JS_FreeValue(ctx, r);
    r = JS_EXCEPTION;
    goto done;
}

JSValue js_error_constructor(JSContext *ctx, JSValueConst new_target,
                                    int argc, JSValueConst *argv, int magic)
{
    JSValue obj, msg, proto;
    JSValueConst message, options;
    int arg_index;

    if (JS_IsUndefined(new_target))
        new_target = JS_GetActiveFunction(ctx);
    proto = JS_GetProperty(ctx, new_target, JS_ATOM_prototype);
    if (JS_IsException(proto))
        return proto;
    if (!JS_IsObject(proto)) {
        JSContext *realm;
        JSValueConst proto1;

        JS_FreeValue(ctx, proto);
        realm = JS_GetFunctionRealm(ctx, new_target);
        if (!realm)
            return JS_EXCEPTION;
        if (magic < 0) {
            proto1 = realm->class_proto[JS_CLASS_ERROR];
        } else {
            proto1 = realm->native_error_proto[magic];
        }
        proto = JS_DupValue(ctx, proto1);
    }
    obj = JS_NewObjectProtoClass(ctx, proto, JS_CLASS_ERROR);
    JS_FreeValue(ctx, proto);
    if (JS_IsException(obj))
        return obj;
    arg_index = (magic == JS_AGGREGATE_ERROR);

    message = argv[arg_index++];
    if (!JS_IsUndefined(message)) {
        msg = JS_ToString(ctx, message);
        if (unlikely(JS_IsException(msg)))
            goto exception;
        JS_DefinePropertyValue(ctx, obj, JS_ATOM_message, msg,
                               JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
    }

    if (arg_index < argc) {
        options = argv[arg_index];
        if (JS_IsObject(options)) {
            int present = JS_HasProperty(ctx, options, JS_ATOM_cause);
            if (present < 0)
                goto exception;
            if (present) {
                JSValue cause = JS_GetProperty(ctx, options, JS_ATOM_cause);
                if (JS_IsException(cause))
                    goto exception;
                JS_DefinePropertyValue(ctx, obj, JS_ATOM_cause, cause,
                                       JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
            }
        }
    }

    if (magic == JS_AGGREGATE_ERROR) {
        JSValue error_list = iterator_to_array(ctx, argv[0]);
        if (JS_IsException(error_list))
            goto exception;
        JS_DefinePropertyValue(ctx, obj, JS_ATOM_errors, error_list,
                               JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
    }

    /* skip the Error() function in the backtrace */
    build_backtrace(ctx, obj, NULL, 0, 0, JS_BACKTRACE_FLAG_SKIP_FIRST_LEVEL);
    return obj;
 exception:
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

JSValue js_error_toString(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv)
{
    JSValue name, msg;

    if (!JS_IsObject(this_val))
        return JS_ThrowTypeErrorNotAnObject(ctx);
    name = JS_GetProperty(ctx, this_val, JS_ATOM_name);
    if (JS_IsUndefined(name))
        name = JS_AtomToString(ctx, JS_ATOM_Error);
    else
        name = JS_ToStringFree(ctx, name);
    if (JS_IsException(name))
        return JS_EXCEPTION;

    msg = JS_GetProperty(ctx, this_val, JS_ATOM_message);
    if (JS_IsUndefined(msg))
        msg = JS_AtomToString(ctx, JS_ATOM_empty_string);
    else
        msg = JS_ToStringFree(ctx, msg);
    if (JS_IsException(msg)) {
        JS_FreeValue(ctx, name);
        return JS_EXCEPTION;
    }
    if (!JS_IsEmptyString(name) && !JS_IsEmptyString(msg))
        name = JS_ConcatString3(ctx, "", name, ": ");
    return JS_ConcatString(ctx, name, msg);
}

JSValue js_error_isError(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv)
{
    return JS_NewBool(ctx, JS_IsError(ctx, argv[0]));
}

/* AggregateError */

/* used by C code. */
JSValue js_aggregate_error_constructor(JSContext *ctx,
                                              JSValueConst errors)
{
    JSValue obj;

    obj = JS_NewObjectProtoClass(ctx,
                                 ctx->native_error_proto[JS_AGGREGATE_ERROR],
                                 JS_CLASS_ERROR);
    if (JS_IsException(obj))
        return obj;
    JS_DefinePropertyValue(ctx, obj, JS_ATOM_errors, JS_DupValue(ctx, errors),
                           JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
    return obj;
}

/* Array */

int JS_CopySubArray(JSContext *ctx,
                           JSValueConst obj, int64_t to_pos,
                           int64_t from_pos, int64_t count, int dir)
{
    JSObject *p;
    int64_t i, from, to, len;
    JSValue val;
    int fromPresent;

    p = NULL;
    if (JS_VALUE_GET_TAG(obj) == JS_TAG_OBJECT) {
        p = JS_VALUE_GET_OBJ(obj);
        if (p->class_id != JS_CLASS_ARRAY || !p->fast_array) {
            p = NULL;
        }
    }

    for (i = 0; i < count; ) {
        if (dir < 0) {
            from = from_pos + count - i - 1;
            to = to_pos + count - i - 1;
        } else {
            from = from_pos + i;
            to = to_pos + i;
        }
        if (p && p->fast_array &&
            from >= 0 && from < (len = p->u.array.count)  &&
            to >= 0 && to < len) {
            int64_t l, j;
            /* Fast path for fast arrays. Since we don't look at the
               prototype chain, we can optimize only the cases where
               all the elements are present in the array. */
            l = count - i;
            if (dir < 0) {
                l = min_int64(l, from + 1);
                l = min_int64(l, to + 1);
                for(j = 0; j < l; j++) {
                    set_value(ctx, &p->u.array.u.values[to - j],
                              JS_DupValue(ctx, p->u.array.u.values[from - j]));
                }
            } else {
                l = min_int64(l, len - from);
                l = min_int64(l, len - to);
                for(j = 0; j < l; j++) {
                    set_value(ctx, &p->u.array.u.values[to + j],
                              JS_DupValue(ctx, p->u.array.u.values[from + j]));
                }
            }
            i += l;
        } else {
            fromPresent = JS_TryGetPropertyInt64(ctx, obj, from, &val);
            if (fromPresent < 0)
                goto exception;

            if (fromPresent) {
                if (JS_SetPropertyInt64(ctx, obj, to, val) < 0)
                    goto exception;
            } else {
                if (JS_DeletePropertyInt64(ctx, obj, to, JS_PROP_THROW) < 0)
                    goto exception;
            }
            i++;
        }
    }
    return 0;

 exception:
    return -1;
}

JSValue js_array_constructor(JSContext *ctx, JSValueConst new_target,
                                    int argc, JSValueConst *argv)
{
    JSValue obj;
    int i;

    obj = js_create_from_ctor(ctx, new_target, JS_CLASS_ARRAY);
    if (JS_IsException(obj))
        return obj;
    if (argc == 1 && JS_IsNumber(argv[0])) {
        uint32_t len;
        if (JS_ToArrayLengthFree(ctx, &len, JS_DupValue(ctx, argv[0]), TRUE))
            goto fail;
        if (JS_SetProperty(ctx, obj, JS_ATOM_length, JS_NewUint32(ctx, len)) < 0)
            goto fail;
    } else {
        for(i = 0; i < argc; i++) {
            if (JS_SetPropertyUint32(ctx, obj, i, JS_DupValue(ctx, argv[i])) < 0)
                goto fail;
        }
    }
    return obj;
fail:
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

JSValue js_array_from(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv)
{
    // from(items, mapfn = void 0, this_arg = void 0)
    JSValueConst items = argv[0], mapfn, this_arg;
    JSValueConst args[2];
    JSValue iter, r, v, v2, arrayLike, next_method, enum_obj;
    int64_t k, len;
    int done, mapping;

    mapping = FALSE;
    mapfn = JS_UNDEFINED;
    this_arg = JS_UNDEFINED;
    r = JS_UNDEFINED;
    arrayLike = JS_UNDEFINED;
    iter = JS_UNDEFINED;
    enum_obj = JS_UNDEFINED;
    next_method = JS_UNDEFINED;

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
        if (! JS_IsFunction(ctx, iter)) {
            JS_ThrowTypeError(ctx, "value is not iterable");
            goto exception;
        }
        if (JS_IsConstructor(ctx, this_val))
            r = JS_CallConstructor(ctx, this_val, 0, NULL);
        else
            r = JS_NewArray(ctx);
        if (JS_IsException(r))
            goto exception;
        enum_obj = JS_GetIterator2(ctx, items, iter);
        if (JS_IsException(enum_obj))
            goto exception;
        next_method = JS_GetProperty(ctx, enum_obj, JS_ATOM_next);
        if (JS_IsException(next_method))
            goto exception;
        for (k = 0;; k++) {
            v = JS_IteratorNext(ctx, enum_obj, next_method, 0, NULL, &done);
            if (JS_IsException(v))
                goto exception;
            if (done)
                break;
            if (mapping) {
                args[0] = v;
                args[1] = JS_NewInt32(ctx, k);
                v2 = JS_Call(ctx, mapfn, this_arg, 2, args);
                JS_FreeValue(ctx, v);
                v = v2;
                if (JS_IsException(v))
                    goto exception_close;
            }
            if (JS_DefinePropertyValueInt64(ctx, r, k, v,
                                            JS_PROP_C_W_E | JS_PROP_THROW) < 0)
                goto exception_close;
        }
    } else {
        arrayLike = JS_ToObject(ctx, items);
        if (JS_IsException(arrayLike))
            goto exception;
        if (js_get_length64(ctx, &len, arrayLike) < 0)
            goto exception;
        v = JS_NewInt64(ctx, len);
        args[0] = v;
        if (JS_IsConstructor(ctx, this_val)) {
            r = JS_CallConstructor(ctx, this_val, 1, args);
        } else {
            r = js_array_constructor(ctx, JS_UNDEFINED, 1, args);
        }
        JS_FreeValue(ctx, v);
        if (JS_IsException(r))
            goto exception;
        for(k = 0; k < len; k++) {
            v = JS_GetPropertyInt64(ctx, arrayLike, k);
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
            if (JS_DefinePropertyValueInt64(ctx, r, k, v,
                                            JS_PROP_C_W_E | JS_PROP_THROW) < 0)
                goto exception;
        }
    }
    if (JS_SetProperty(ctx, r, JS_ATOM_length, JS_NewUint32(ctx, k)) < 0)
        goto exception;
    goto done;

 exception_close:
    JS_IteratorClose(ctx, enum_obj, TRUE);
 exception:
    JS_FreeValue(ctx, r);
    r = JS_EXCEPTION;
 done:
    JS_FreeValue(ctx, arrayLike);
    JS_FreeValue(ctx, iter);
    JS_FreeValue(ctx, enum_obj);
    JS_FreeValue(ctx, next_method);
    return r;
}

JSValue js_array_of(JSContext *ctx, JSValueConst this_val,
                           int argc, JSValueConst *argv)
{
    JSValue obj, args[1];
    int i;

    if (JS_IsConstructor(ctx, this_val)) {
        args[0] = JS_NewInt32(ctx, argc);
        obj = JS_CallConstructor(ctx, this_val, 1, (JSValueConst *)args);
    } else {
        obj = JS_NewArray(ctx);
    }
    if (JS_IsException(obj))
        return JS_EXCEPTION;
    for(i = 0; i < argc; i++) {
        if (JS_CreateDataPropertyUint32(ctx, obj, i, JS_DupValue(ctx, argv[i]),
                                        JS_PROP_THROW) < 0) {
            goto fail;
        }
    }
    if (JS_SetProperty(ctx, obj, JS_ATOM_length, JS_NewUint32(ctx, argc)) < 0) {
    fail:
        JS_FreeValue(ctx, obj);
        return JS_EXCEPTION;
    }
    return obj;
}

JSValue js_array_isArray(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv)
{
    int ret;
    ret = JS_IsArray(ctx, argv[0]);
    if (ret < 0)
        return JS_EXCEPTION;
    else
        return JS_NewBool(ctx, ret);
}

JSValue js_get_this(JSContext *ctx,
                           JSValueConst this_val)
{
    return JS_DupValue(ctx, this_val);
}

/* XXX: optimize */
JSValue JS_ArraySpeciesGetCtor(JSContext *ctx, JSValueConst obj)
{
    JSValue ctor, species;
    int res;
    JSContext *realm;

    res = JS_IsArray(ctx, obj);
    if (res < 0)
        return JS_EXCEPTION;
    if (!res)
        return JS_UNDEFINED;
    ctor = JS_GetProperty(ctx, obj, JS_ATOM_constructor);
    if (JS_IsException(ctor))
        return ctor;
    if (JS_IsConstructor(ctx, ctor)) {
        /* legacy web compatibility */
        realm = JS_GetFunctionRealm(ctx, ctor);
        if (!realm) {
            JS_FreeValue(ctx, ctor);
            return JS_EXCEPTION;
        }
        if (realm != ctx &&
            js_same_value(ctx, ctor, realm->array_ctor)) {
            JS_FreeValue(ctx, ctor);
            ctor = JS_UNDEFINED;
        }
    }
    if (JS_IsObject(ctor)) {
        species = JS_GetProperty(ctx, ctor, JS_ATOM_Symbol_species);
        JS_FreeValue(ctx, ctor);
        if (JS_IsException(species))
            return species;
        ctor = species;
        if (JS_IsNull(ctor))
            ctor = JS_UNDEFINED;
    }
    if (!JS_IsUndefined(ctor) &&
        js_same_value(ctx, ctor, ctx->array_ctor)) {
        JS_FreeValue(ctx, ctor);
        ctor = JS_UNDEFINED;
    }
    return ctor;
}

JSValue JS_ArrayCreateFromCtor(JSContext *ctx, JSValueConst ctor, int64_t len)
{
    JSValue len_val, ret;

    len_val = JS_NewInt64(ctx, len);
    if (JS_IsUndefined(ctor)) {
        ret = js_array_constructor(ctx, JS_UNDEFINED, 1, (JSValueConst *)&len_val);
    } else {
        ret = JS_CallConstructor(ctx, ctor, 1, (JSValueConst *)&len_val);
    }
    JS_FreeValue(ctx, len_val);
    return ret;
}

/* len must be >= 0 */
JSValue JS_ArraySpeciesCreate(JSContext *ctx, JSValueConst obj, int64_t len)
{
    JSValue ctor, ret;

    ctor = JS_ArraySpeciesGetCtor(ctx, obj);
    if (JS_IsException(ctor))
        return ctor;
    ret = JS_ArrayCreateFromCtor(ctx, ctor, len);
    JS_FreeValue(ctx, ctor);
    return ret;
}

int JS_isConcatSpreadable(JSContext *ctx, JSValueConst obj)
{
    JSValue val;

    if (!JS_IsObject(obj))
        return FALSE;
    val = JS_GetProperty(ctx, obj, JS_ATOM_Symbol_isConcatSpreadable);
    if (JS_IsException(val))
        return -1;
    if (!JS_IsUndefined(val))
        return JS_ToBoolFree(ctx, val);
    return JS_IsArray(ctx, obj);
}

JSValue js_array_at(JSContext *ctx, JSValueConst this_val,
                           int argc, JSValueConst *argv)
{
    JSValue obj, ret;
    int64_t len, idx;
    JSValue *arrp;
    uint32_t count;

    obj = JS_ToObject(ctx, this_val);
    if (js_get_length64(ctx, &len, obj))
        goto exception;

    if (JS_ToInt64Sat(ctx, &idx, argv[0]))
        goto exception;

    if (idx < 0)
        idx = len + idx;
    if (idx < 0 || idx >= len) {
        ret = JS_UNDEFINED;
    } else if (js_get_fast_array(ctx, obj, &arrp, &count) && idx < count) {
        ret = JS_DupValue(ctx, arrp[idx]);
    } else {
        int present = JS_TryGetPropertyInt64(ctx, obj, idx, &ret);
        if (present < 0)
            goto exception;
        if (!present)
            ret = JS_UNDEFINED;
    }
    JS_FreeValue(ctx, obj);
    return ret;
 exception:
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}
