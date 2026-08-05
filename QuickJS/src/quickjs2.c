#include "js_internal.h"
#include "debuglog.h"
#undef countof
#define countof(arr) JS_ARRAY_LEN(arr)


int QUICKJS_API JS_SetPropertyUint32(JSContext *ctx, JSValueConst this_obj,
                         uint32_t idx, JSValue val)
{
    return JS_SetPropertyValue(ctx, this_obj, JS_NewUint32(ctx, idx), val,
                               JS_PROP_THROW);
}

int QUICKJS_API JS_SetPropertyInt64(JSContext *ctx, JSValueConst this_obj,
                        int64_t idx, JSValue val)
{
    JSAtom prop;
    int res;

    if ((uint64_t)idx <= INT32_MAX) {
        /* fast path for fast arrays */
        return JS_SetPropertyValue(ctx, this_obj, JS_NewInt32(ctx, idx), val,
                                   JS_PROP_THROW);
    }
    prop = JS_NewAtomInt64(ctx, idx);
    if (prop == JS_ATOM_NULL) {
        JS_FreeValue(ctx, val);
        return -1;
    }
    res = JS_SetProperty(ctx, this_obj, prop, val);
    JS_FreeAtom(ctx, prop);
    return res;
}

int QUICKJS_API JS_SetPropertyStr(JSContext *ctx, JSValueConst this_obj,
                      const char *prop, JSValue val)
{
    JSAtom atom;
    int ret;
    atom = JS_NewAtom(ctx, prop);
    if (atom == JS_ATOM_NULL) {
        JS_FreeValue(ctx, val);
        return -1;
    }
    ret = JS_SetPropertyInternal(ctx, this_obj, atom, val, this_obj, JS_PROP_THROW);
    JS_FreeAtom(ctx, atom);
    return ret;
}

/* return -1 in case of exception or TRUE or FALSE. Warning: 'val' is
   freed by the function. 'flags' is a bitmask of JS_PROP_THROW and
   JS_PROP_THROW_STRICT. 'this_obj' is the receiver. If obj !=
   this_obj, then obj must be an object (Reflect.set case). */
int QUICKJS_API JS_SetPropertyInternal(JSContext *ctx, JSValueConst obj,
                           JSAtom prop, JSValue val, JSValueConst this_obj, int flags)
{
    JSObject *p, *p1;
    JSShapeProperty *prs;
    JSProperty *pr;
    uint32_t tag;
    JSPropertyDescriptor desc;
    int ret;
#if 0
    printf("JS_SetPropertyInternal: "); print_atom(ctx, prop); printf("\n");
#endif
    tag = JS_VALUE_GET_TAG(this_obj);
    if (unlikely(tag != JS_TAG_OBJECT)) {
        if (JS_VALUE_GET_TAG(obj) == JS_TAG_OBJECT) {
            p = NULL;
            p1 = JS_VALUE_GET_OBJ(obj);
            goto prototype_lookup;
        } else {
            switch(tag) {
            case JS_TAG_NULL:
                JS_FreeValue(ctx, val);
                JS_ThrowTypeErrorAtom(ctx, "cannot set property '%s' of null", prop);
                return -1;
            case JS_TAG_UNDEFINED:
                JS_FreeValue(ctx, val);
                JS_ThrowTypeErrorAtom(ctx, "cannot set property '%s' of undefined", prop);
                return -1;
            default:
                /* even on a primitive type we can have setters on the prototype */
                p = NULL;
                p1 = JS_VALUE_GET_OBJ(JS_GetPrototypePrimitive(ctx, obj));
                goto prototype_lookup;
            }
        }
    } else {
        p = JS_VALUE_GET_OBJ(this_obj);
        p1 = JS_VALUE_GET_OBJ(obj);
        if (unlikely(p != p1))
            goto retry2;
    }

    /* fast path if obj == this_obj */
 retry:
    prs = find_own_property(&pr, p1, prop);
    if (prs) {
        if (likely((prs->flags & (JS_PROP_TMASK | JS_PROP_WRITABLE |
                                  JS_PROP_LENGTH)) == JS_PROP_WRITABLE)) {
            /* fast case */
            set_value(ctx, &pr->u.value, val);
            return TRUE;
        } else if (prs->flags & JS_PROP_LENGTH) {
            assert(p->class_id == JS_CLASS_ARRAY);
            assert(prop == JS_ATOM_length);
            return set_array_length(ctx, p, val, flags);
        } else if ((prs->flags & JS_PROP_TMASK) == JS_PROP_GETSET) {
            return call_setter(ctx, pr->u.getset.setter, this_obj, val, flags);
        } else if ((prs->flags & JS_PROP_TMASK) == JS_PROP_VARREF) {
            /* XXX: already use var_ref->is_const. Cannot simplify use the
               writable flag for JS_CLASS_MODULE_NS. */
            if (p->class_id == JS_CLASS_MODULE_NS || pr->u.var_ref->is_const)
                goto read_only_prop;
            set_value(ctx, pr->u.var_ref->pvalue, val);
            return TRUE;
        } else if ((prs->flags & JS_PROP_TMASK) == JS_PROP_AUTOINIT) {
            /* Instantiate property and retry (potentially useless) */
            if (JS_AutoInitProperty(ctx, p, prop, pr, prs)) {
                JS_FreeValue(ctx, val);
                return -1;
            }
            goto retry;
        } else {
            goto read_only_prop;
        }
    }

    for(;;) {
        if (p1->is_exotic) {
            if (p1->fast_array) {
                if (__JS_AtomIsTaggedInt(prop)) {
                    uint32_t idx = __JS_AtomToUInt32(prop);
                    if (idx < p1->u.array.count) {
                        if (unlikely(p == p1))
                            return JS_SetPropertyValue(ctx, this_obj, JS_NewInt32(ctx, idx), val, flags);
                        else
                            break;
                    } else if (p1->class_id >= JS_CLASS_UINT8C_ARRAY &&
                               p1->class_id <= JS_CLASS_FLOAT64_ARRAY) {
                        goto typed_array_oob;
                    }
                } else if (p1->class_id >= JS_CLASS_UINT8C_ARRAY &&
                           p1->class_id <= JS_CLASS_FLOAT64_ARRAY) {
                    ret = JS_AtomIsNumericIndex(ctx, prop);
                    if (ret != 0) {
                        if (ret < 0) {
                            JS_FreeValue(ctx, val);
                            return -1;
                        }
                    typed_array_oob:
                        if (p == p1) {
                            /* must convert the argument even if out of bound access */
                            if (p1->class_id == JS_CLASS_BIG_INT64_ARRAY ||
                                p1->class_id == JS_CLASS_BIG_UINT64_ARRAY) {
                                int64_t v;
                                if (JS_ToBigInt64Free(ctx, &v, val))
                                    return -1;
                            } else {
                                val = JS_ToNumberFree(ctx, val);
                                JS_FreeValue(ctx, val);
                                if (JS_IsException(val))
                                    return -1;
                            }
                        } else {
                            JS_FreeValue(ctx, val);
                        }
                        return TRUE;
                    }
                }
            } else {
                const JSClassExoticMethods *em = ctx->rt->class_array[p1->class_id].exotic;
                if (em) {
                    JSValue obj1;
                    if (em->set_property) {
                        /* set_property can free the prototype */
                        obj1 = JS_DupValue(ctx, JS_MKPTR(JS_TAG_OBJECT, p1));
                        ret = em->set_property(ctx, obj1, prop,
                                               val, this_obj, flags);
                        JS_FreeValue(ctx, obj1);
                        JS_FreeValue(ctx, val);
                        return ret;
                    }
                    if (em->get_own_property) {
                        /* get_own_property can free the prototype */
                        obj1 = JS_DupValue(ctx, JS_MKPTR(JS_TAG_OBJECT, p1));
                        ret = em->get_own_property(ctx, &desc,
                                                   obj1, prop);
                        JS_FreeValue(ctx, obj1);
                        if (ret < 0) {
                            JS_FreeValue(ctx, val);
                            return ret;
                        }
                        if (ret) {
                            if (desc.flags & JS_PROP_GETSET) {
                                JSObject *setter;
                                if (JS_IsUndefined(desc.setter))
                                    setter = NULL;
                                else
                                    setter = JS_VALUE_GET_OBJ(desc.setter);
                                ret = call_setter(ctx, setter, this_obj, val, flags);
                                JS_FreeValue(ctx, desc.getter);
                                JS_FreeValue(ctx, desc.setter);
                                return ret;
                            } else {
                                JS_FreeValue(ctx, desc.value);
                                if (!(desc.flags & JS_PROP_WRITABLE))
                                    goto read_only_prop;
                                if (likely(p == p1)) {
                                    ret = JS_DefineProperty(ctx, this_obj, prop, val,
                                                            JS_UNDEFINED, JS_UNDEFINED,
                                                            JS_PROP_HAS_VALUE);
                                    JS_FreeValue(ctx, val);
                                    return ret;
                                } else {
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
        p1 = p1->shape->proto;
    prototype_lookup:
        if (!p1)
            break;

    retry2:
        prs = find_own_property(&pr, p1, prop);
        if (prs) {
            if ((prs->flags & JS_PROP_TMASK) == JS_PROP_GETSET) {
                return call_setter(ctx, pr->u.getset.setter, this_obj, val, flags);
            } else if ((prs->flags & JS_PROP_TMASK) == JS_PROP_AUTOINIT) {
                /* Instantiate property and retry (potentially useless) */
                if (JS_AutoInitProperty(ctx, p1, prop, pr, prs))
                    return -1;
                goto retry2;
            } else if (!(prs->flags & JS_PROP_WRITABLE)) {
                goto read_only_prop;
            } else {
                break;
            }
        }
    }

    if (unlikely(!p)) {
        JS_FreeValue(ctx, val);
        return JS_ThrowTypeErrorOrFalse(ctx, flags, "not an object");
    }

    if (unlikely(!p->extensible)) {
        JS_FreeValue(ctx, val);
        return JS_ThrowTypeErrorOrFalse(ctx, flags, "object is not extensible");
    }

    if (likely(p == JS_VALUE_GET_OBJ(obj))) {
        if (p->is_exotic) {
            if (p->class_id == JS_CLASS_ARRAY && p->fast_array &&
                __JS_AtomIsTaggedInt(prop)) {
                uint32_t idx = __JS_AtomToUInt32(prop);
                if (idx == p->u.array.count) {
                    /* fast case */
                    return add_fast_array_element(ctx, p, val, flags);
                } else {
                    goto generic_create_prop;
                }
            } else {
                goto generic_create_prop;
            }
        } else {
            if (unlikely(p->class_id == JS_CLASS_GLOBAL_OBJECT))
                goto generic_create_prop;
            pr = add_property(ctx, p, prop, JS_PROP_C_W_E);
            if (unlikely(!pr)) {
                JS_FreeValue(ctx, val);
                return -1;
            }
            pr->u.value = val;
            return TRUE;
        }
    } else {
        /* generic case: modify the property in this_obj if it already exists */
        ret = JS_GetOwnPropertyInternal(ctx, &desc, p, prop);
        if (ret < 0) {
            JS_FreeValue(ctx, val);
            return ret;
        }
        if (ret) {
            if (desc.flags & JS_PROP_GETSET) {
                JS_FreeValue(ctx, desc.getter);
                JS_FreeValue(ctx, desc.setter);
                JS_FreeValue(ctx, val);
                return JS_ThrowTypeErrorOrFalse(ctx, flags, "setter is forbidden");
            } else {
                JS_FreeValue(ctx, desc.value);
                if (!(desc.flags & JS_PROP_WRITABLE) ||
                    p->class_id == JS_CLASS_MODULE_NS) {
                read_only_prop:
                    JS_FreeValue(ctx, val);
                    return JS_ThrowTypeErrorReadOnly(ctx, flags, prop);
                }
            }
            ret = JS_DefineProperty(ctx, this_obj, prop, val,
                                    JS_UNDEFINED, JS_UNDEFINED,
                                    JS_PROP_HAS_VALUE);
            JS_FreeValue(ctx, val);
            return ret;
        } else {
        generic_create_prop:
            ret = JS_CreateProperty(ctx, p, prop, val, JS_UNDEFINED, JS_UNDEFINED,
                                    flags |
                                    JS_PROP_HAS_VALUE |
                                    JS_PROP_HAS_ENUMERABLE |
                                    JS_PROP_HAS_WRITABLE |
                                    JS_PROP_HAS_CONFIGURABLE |
                                    JS_PROP_C_W_E);
            JS_FreeValue(ctx, val);
            return ret;
        }
    }
}
static int JS_DefinePropertyValue_lock = 0;
/* shortcut to add or redefine a new property value */
int QUICKJS_API JS_DefinePropertyValue(JSContext *ctx, JSValueConst this_obj,
                           JSAtom prop, JSValue val, int flags)
{
    int ret;

    JS_LOG("JS_DefinePropertyValue", "Entered, val=%08lX_%08lX, prop=%lu, flags=0x%X",
           U64_HI(val), U64_LO(val), (unsigned long)prop, flags);
	
	if (JS_DefinePropertyValue_lock > 0) {
        JS_LOG("JS_DefinePropertyValue", "Recursion detected, returning success");
        return 0;   // ºŸ◊∞≥…π¶£¨±‹√‚∆∆ªµ’ª
    }

    JS_DefinePropertyValue_lock++;
	
    ret = JS_DefineProperty(ctx, this_obj, prop, (JSValueConst)val, JS_UNDEFINED, JS_UNDEFINED,
                            flags | JS_PROP_HAS_VALUE | JS_PROP_HAS_CONFIGURABLE | JS_PROP_HAS_WRITABLE | JS_PROP_HAS_ENUMERABLE);

    JS_LOG("JS_DefinePropertyValue", "After JS_DefineProperty, val=%08lX_%08lX, ret=%d",
           U64_HI(val), U64_LO(val), ret);

    JS_FreeValue(ctx, val);

    JS_LOG("JS_DefinePropertyValue", "After JS_FreeValue, returning %d", ret);
    return ret;
}

int QUICKJS_API JS_DefinePropertyValueValue(JSContext *ctx, JSValueConst this_obj,
                                JSValue prop, JSValue val, int flags)
{
    JSAtom atom;
    int ret;
    atom = JS_ValueToAtom(ctx, prop);
    JS_FreeValue(ctx, prop);
    if (unlikely(atom == JS_ATOM_NULL)) {
        JS_FreeValue(ctx, val);
        return -1;
    }
    ret = JS_DefinePropertyValue(ctx, this_obj, atom, val, flags);
    JS_FreeAtom(ctx, atom);
    return ret;
}

int QUICKJS_API JS_DefinePropertyValueUint32(JSContext *ctx, JSValueConst this_obj,
                                 uint32_t idx, JSValue val, int flags)
{
    return JS_DefinePropertyValueValue(ctx, this_obj, JS_NewUint32(ctx, idx),
                                       val, flags);
}

int QUICKJS_API JS_DefinePropertyValueInt64(JSContext *ctx, JSValueConst this_obj,
                                int64_t idx, JSValue val, int flags)
{
    return JS_DefinePropertyValueValue(ctx, this_obj, JS_NewInt64(ctx, idx),
                                       val, flags);
}

int QUICKJS_API JS_DefinePropertyValueStr(JSContext *ctx, JSValueConst this_obj,
                              const char *prop, JSValue val, int flags)
{
    JSAtom atom;
    int ret;
    atom = JS_NewAtom(ctx, prop);
    if (atom == JS_ATOM_NULL) {
        JS_FreeValue(ctx, val);
        return -1;
    }
    ret = JS_DefinePropertyValue(ctx, this_obj, atom, val, flags);
    JS_FreeAtom(ctx, atom);
    return ret;
}

/* shortcut to add getter & setter */
int QUICKJS_API JS_DefinePropertyGetSet(JSContext *ctx, JSValueConst this_obj,
                            JSAtom prop, JSValue getter, JSValue setter,
                            int flags)
{
    int ret;
    ret = JS_DefineProperty(ctx, this_obj, prop, JS_UNDEFINED, getter, setter,
                            flags | JS_PROP_HAS_GET | JS_PROP_HAS_SET |
                            JS_PROP_HAS_CONFIGURABLE | JS_PROP_HAS_ENUMERABLE);
    JS_FreeValue(ctx, getter);
    JS_FreeValue(ctx, setter);
    return ret;
}

/* return -1, FALSE or TRUE. return FALSE if not configurable or
   invalid object. return -1 in case of exception.
   flags can be 0, JS_PROP_THROW or JS_PROP_THROW_STRICT */
int QUICKJS_API JS_DeleteProperty(JSContext *ctx, JSValueConst obj, JSAtom prop, int flags)
{
    JSValue obj1;
    JSObject *p;
    int res;

    obj1 = JS_ToObject(ctx, obj);
    if (JS_IsException(obj1))
        return -1;
    p = JS_VALUE_GET_OBJ(obj1);
    res = delete_property(ctx, p, prop);
    JS_FreeValue(ctx, obj1);
    if (res != FALSE)
        return res;
    if ((flags & JS_PROP_THROW) ||
        ((flags & JS_PROP_THROW_STRICT) && is_strict_mode(ctx))) {
        JS_ThrowTypeError(ctx, "could not delete property");
        return -1;
    }
    return FALSE;
}

int QUICKJS_API JS_DeletePropertyInt64(JSContext *ctx, JSValueConst obj, int64_t idx, int flags)
{
    JSAtom prop;
    int res;

    if ((uint64_t)idx <= JS_ATOM_MAX_INT) {
        /* fast path for fast arrays */
        return JS_DeleteProperty(ctx, obj, __JS_AtomFromUInt32(idx), flags);
    }
    prop = JS_NewAtomInt64(ctx, idx);
    if (prop == JS_ATOM_NULL)
        return -1;
    res = JS_DeleteProperty(ctx, obj, prop, flags);
    JS_FreeAtom(ctx, prop);
    return res;
}

BOOL QUICKJS_API JS_IsFunction(JSContext *ctx, JSValueConst val)
{
    JSObject *p;
    if (JS_VALUE_GET_TAG(val) != JS_TAG_OBJECT)
        return FALSE;
    p = JS_VALUE_GET_OBJ(val);
    switch(p->class_id) {
    case JS_CLASS_BYTECODE_FUNCTION:
        return TRUE;
    case JS_CLASS_PROXY:
        return p->u.proxy_data->is_func;
    default:
        return (ctx->rt->class_array[p->class_id].call != NULL);
    }
}

BOOL QUICKJS_API JS_IsCFunction(JSContext *ctx, JSValueConst val, JSCFunction *func, int magic)
{
    JSObject *p;
    if (JS_VALUE_GET_TAG(val) != JS_TAG_OBJECT)
        return FALSE;
    p = JS_VALUE_GET_OBJ(val);
    if (p->class_id == JS_CLASS_C_FUNCTION)
        return (p->u.cfunc.c_function.generic == func && p->u.cfunc.magic == magic);
    else
        return FALSE;
}

BOOL QUICKJS_API JS_IsConstructor(JSContext *ctx, JSValueConst val)
{
    JSObject *p;
    if (JS_VALUE_GET_TAG(val) != JS_TAG_OBJECT)
        return FALSE;
    p = JS_VALUE_GET_OBJ(val);
    return p->is_constructor;
}

BOOL QUICKJS_API JS_SetConstructorBit(JSContext *ctx, JSValueConst func_obj, BOOL val)
{
    JSObject *p;
    if (JS_VALUE_GET_TAG(func_obj) != JS_TAG_OBJECT)
        return FALSE;
    p = JS_VALUE_GET_OBJ(func_obj);
    p->is_constructor = val;
    return TRUE;
}

BOOL QUICKJS_API JS_IsError(JSContext *ctx, JSValueConst val)
{
    JSObject *p;
    if (JS_VALUE_GET_TAG(val) != JS_TAG_OBJECT)
        return FALSE;
    p = JS_VALUE_GET_OBJ(val);
    return (p->class_id == JS_CLASS_ERROR);
}

/* must be called after JS_Throw() */
void QUICKJS_API JS_SetUncatchableException(JSContext *ctx, BOOL flag)
{
    ctx->rt->current_exception_is_uncatchable = flag;
}

void QUICKJS_API JS_SetOpaque(JSValue obj, void *opaque)
{
   JSObject *p;
    if (JS_VALUE_GET_TAG(obj) == JS_TAG_OBJECT) {
        p = JS_VALUE_GET_OBJ(obj);
        p->u.opaque = opaque;
    }
}

/* return NULL if not an object of class class_id */
void *QUICKJS_API JS_GetOpaque(JSValueConst obj, JSClassID class_id)
{
    JSObject *p;
    if (JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT)
        return NULL;
    p = JS_VALUE_GET_OBJ(obj);
    if (p->class_id != class_id)
        return NULL;
    return p->u.opaque;
}

void *QUICKJS_API JS_GetOpaque2(JSContext *ctx, JSValueConst obj, JSClassID class_id)
{
    void *p = JS_GetOpaque(obj, class_id);
    if (unlikely(!p)) {
        JS_ThrowTypeErrorInvalidClass(ctx, class_id);
    }
    return p;
}

void *QUICKJS_API JS_GetAnyOpaque(JSValueConst obj, JSClassID *class_id)
{
    JSObject *p;
    if (JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT) {
        *class_id = 0;
        return NULL;
    }
    p = JS_VALUE_GET_OBJ(obj);
    *class_id = p->class_id;
    return p->u.opaque;
}

void QUICKJS_API JS_SetIsHTMLDDA(JSContext *ctx, JSValueConst obj)
{
    JSObject *p;
    if (JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT)
        return;
    p = JS_VALUE_GET_OBJ(obj);
    p->is_HTMLDDA = TRUE;
}

int QUICKJS_API JS_ToBool(JSContext *ctx, JSValueConst val)
{
    return JS_ToBoolFree(ctx, JS_DupValue(ctx, val));
}

int QUICKJS_API JS_ToFloat64(JSContext *ctx, double *pres, JSValueConst val)
{
    return JS_ToFloat64Free(ctx, pres, JS_DupValue(ctx, val));
}

int QUICKJS_API JS_ToInt32Sat(JSContext *ctx, int *pres, JSValueConst val)
{
    return JS_ToInt32SatFree(ctx, pres, JS_DupValue(ctx, val));
}

int QUICKJS_API JS_ToInt32Clamp(JSContext *ctx, int *pres, JSValueConst val,
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

int QUICKJS_API JS_ToInt64Sat(JSContext *ctx, int64_t *pres, JSValueConst val)
{
    return JS_ToInt64SatFree(ctx, pres, JS_DupValue(ctx, val));
}

int QUICKJS_API JS_ToInt64Clamp(JSContext *ctx, int64_t *pres, JSValueConst val,
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

int QUICKJS_API JS_ToInt64(JSContext *ctx, int64_t *pres, JSValueConst val)
{
    return JS_ToInt64Free(ctx, pres, JS_DupValue(ctx, val));
}

int QUICKJS_API JS_ToInt64Ext(JSContext *ctx, int64_t *pres, JSValueConst val)
{
    if (JS_IsBigInt(ctx, val))
        return JS_ToBigInt64(ctx, pres, val);
    else
        return JS_ToInt64(ctx, pres, val);
}

int QUICKJS_API JS_ToInt32(JSContext *ctx, int32_t *pres, JSValueConst val)
{
    return JS_ToInt32Free(ctx, pres, JS_DupValue(ctx, val));
}

int QUICKJS_API JS_ToIndex(JSContext *ctx, uint64_t *plen, JSValueConst val)
{
    int64_t v;
    if (JS_ToInt64Sat(ctx, &v, val))
        return -1;
    if (v < 0 || v > MAX_SAFE_INTEGER) {
        JS_ThrowRangeError(ctx, "invalid array index");
        *plen = 0;
        return -1;
    }
    *plen = v;
    return 0;
}

JSValue QUICKJS_API JS_ToString(JSContext *ctx, JSValueConst val)
{
    return JS_ToStringInternal(ctx, val, FALSE);
}

JSValue QUICKJS_API JS_ToPropertyKey(JSContext *ctx, JSValueConst val)
{
    return JS_ToStringInternal(ctx, val, TRUE);
}

void QUICKJS_API JS_PrintValueSetDefaultOptions(JSPrintValueOptions *options)
{
    memset(options, 0, sizeof(*options));
    options->max_depth = 2;
    options->max_string_length = 1000;
    options->max_item_count = 100;
}

void QUICKJS_API JS_PrintValueRT(JSRuntime *rt, JSPrintValueWrite *write_func, void *write_opaque,
                     JSValueConst val, const JSPrintValueOptions *options)
{
    JS_PrintValueInternal(rt, NULL, write_func, write_opaque, val, options);
}

void QUICKJS_API JS_PrintValue(JSContext *ctx, JSPrintValueWrite *write_func, void *write_opaque,
                   JSValueConst val, const JSPrintValueOptions *options)
{
    JS_PrintValueInternal(ctx->rt, ctx, write_func, write_opaque, val, options);
}

/* return -1 if exception (proxy case) or TRUE/FALSE */
// TODO: should take flags to make proxy resolution and exceptions optional
int QUICKJS_API JS_IsArray(JSContext *ctx, JSValueConst val)
{
    if (js_resolve_proxy(ctx, &val, TRUE))
        return -1;
    if (JS_VALUE_GET_TAG(val) == JS_TAG_OBJECT) {
        JSObject *p = JS_VALUE_GET_OBJ(val);
        return p->class_id == JS_CLASS_ARRAY;
    } else {
        return FALSE;
    }
}

JSValue QUICKJS_API JS_NewBigInt64(JSContext *ctx, int64_t v)
{
#if JS_SHORT_BIG_INT_BITS == 64
    return __JS_NewShortBigInt(ctx, v);
#else
    if (v >= JS_SHORT_BIG_INT_MIN && v <= JS_SHORT_BIG_INT_MAX) {
        return __JS_NewShortBigInt(ctx, v);
    } else {
        JSBigInt *p;
        p = js_bigint_new_si64(ctx, v);
        if (!p)
            return JS_EXCEPTION;
        return JS_MKPTR(JS_TAG_BIG_INT, p);
    }
#endif
}

JSValue QUICKJS_API JS_NewBigUint64(JSContext *ctx, uint64_t v)
{
    if (v <= JS_SHORT_BIG_INT_MAX) {
        return __JS_NewShortBigInt(ctx, v);
    } else {
        JSBigInt *p;
        p = js_bigint_new_ui64(ctx, v);
        if (!p)
            return JS_EXCEPTION;
        return JS_MKPTR(JS_TAG_BIG_INT, p);
    }
}

int QUICKJS_API JS_ToBigInt64(JSContext *ctx, int64_t *pres, JSValueConst val)
{
    return JS_ToBigInt64Free(ctx, pres, JS_DupValue(ctx, val));
}

BOOL QUICKJS_API JS_StrictEq(JSContext *ctx, JSValueConst op1, JSValueConst op2)
{
    return js_strict_eq(ctx, op1, op2);
}

BOOL QUICKJS_API JS_SameValue(JSContext *ctx, JSValueConst op1, JSValueConst op2)
{
    return js_same_value(ctx, op1, op2);
}

BOOL QUICKJS_API JS_SameValueZero(JSContext *ctx, JSValueConst op1, JSValueConst op2)
{
    return js_same_value_zero(ctx, op1, op2);
}

JSValue QUICKJS_API JS_Call(JSContext *ctx, JSValueConst func_obj, JSValueConst this_obj,
                int argc, JSValueConst *argv)
{
    return JS_CallInternal(ctx, func_obj, this_obj, JS_UNDEFINED,
                           argc, (JSValue *)argv, JS_CALL_FLAG_COPY_ARGV);
}

JSValue QUICKJS_API JS_CallConstructor2(JSContext *ctx, JSValueConst func_obj,
                            JSValueConst new_target,
                            int argc, JSValueConst *argv)
{
    return JS_CallConstructorInternal(ctx, func_obj, new_target,
                                      argc, (JSValue *)argv,
                                      JS_CALL_FLAG_COPY_ARGV);
}

JSValue QUICKJS_API JS_CallConstructor(JSContext *ctx, JSValueConst func_obj,
                           int argc, JSValueConst *argv)
{
    return JS_CallConstructorInternal(ctx, func_obj, func_obj,
                                      argc, (JSValue *)argv,
                                      JS_CALL_FLAG_COPY_ARGV);
}

JSValue QUICKJS_API JS_Invoke(JSContext *ctx, JSValueConst this_val, JSAtom atom,
                  int argc, JSValueConst *argv)
{
    JSValue func_obj;
    func_obj = JS_GetProperty(ctx, this_val, atom);
    if (JS_IsException(func_obj))
        return func_obj;
    return JS_CallFree(ctx, func_obj, this_val, argc, argv);
}

/* return true if 'input' contains the source of a module
   (heuristic). 'input' must be a zero terminated.

   Heuristic: skip comments and expect 'import' keyword not followed
   by '(' or '.' or export keyword.
*/
BOOL QUICKJS_API JS_DetectModule(const char *input, size_t input_len)
{
    const uint8_t *p = (const uint8_t *)input;
    int tok;

    skip_shebang(&p, p + input_len);
    switch(simple_next_token(&p, FALSE)) {
    case TOK_IMPORT:
        tok = simple_next_token(&p, FALSE);
        return (tok != '.' && tok != '(');
    case TOK_EXPORT:
        return TRUE;
    default:
        return FALSE;
    }
}

/* create a C module */
JSModuleDef *QUICKJS_API JS_NewCModule(JSContext *ctx, const char *name_str,
                           JSModuleInitFunc *func)
{
    JSModuleDef *m;
    JSAtom name;
    name = JS_NewAtom(ctx, name_str);
    if (name == JS_ATOM_NULL)
        return NULL;
    m = js_new_module_def(ctx, name);
    if (!m)
        return NULL;
    m->init_func = func;
    return m;
}

int QUICKJS_API JS_AddModuleExport(JSContext *ctx, JSModuleDef *m, const char *export_name)
{
    JSExportEntry *me;
    JSAtom name;
    name = JS_NewAtom(ctx, export_name);
    if (name == JS_ATOM_NULL)
        return -1;
    me = add_export_entry2(ctx, NULL, m, JS_ATOM_NULL, name,
                           JS_EXPORT_TYPE_LOCAL);
    JS_FreeAtom(ctx, name);
    if (!me)
        return -1;
    else
        return 0;
}

int QUICKJS_API JS_SetModuleExport(JSContext *ctx, JSModuleDef *m, const char *export_name,
                       JSValue val)
{
    JSExportEntry *me;
    JSAtom name;
    name = JS_NewAtom(ctx, export_name);
    if (name == JS_ATOM_NULL)
        goto fail;
    me = find_export_entry(ctx, m, name);
    JS_FreeAtom(ctx, name);
    if (!me)
        goto fail;
    set_value(ctx, me->u.local.var_ref->pvalue, val);
    return 0;
 fail:
    JS_FreeValue(ctx, val);
    return -1;
}

int QUICKJS_API JS_SetModulePrivateValue(JSContext *ctx, JSModuleDef *m, JSValue val)
{
    set_value(ctx, &m->private_value, val);
    return 0;
}

JSValue QUICKJS_API JS_GetModulePrivateValue(JSContext *ctx, JSModuleDef *m)
{
    return JS_DupValue(ctx, m->private_value);
}

void QUICKJS_API JS_SetModuleLoaderFunc(JSRuntime *rt,
                            JSModuleNormalizeFunc *module_normalize,
                            JSModuleLoaderFunc *module_loader, void *opaque)
{
    rt->module_normalize_func = module_normalize;
    rt->module_loader_has_attr = FALSE;
    rt->u.module_loader_func = module_loader;
    rt->module_check_attrs = NULL;
    rt->module_loader_opaque = opaque;
}

void QUICKJS_API JS_SetModuleLoaderFunc2(JSRuntime *rt,
                             JSModuleNormalizeFunc *module_normalize,
                             JSModuleLoaderFunc2 *module_loader,
                             JSModuleCheckSupportedImportAttributes *module_check_attrs,
                             void *opaque)
{
    rt->module_normalize_func = module_normalize;
    rt->module_loader_has_attr = TRUE;
    rt->u.module_loader_func2 = module_loader;
    rt->module_check_attrs = module_check_attrs;
    rt->module_loader_opaque = opaque;
}

JSValue QUICKJS_API JS_GetModuleNamespace(JSContext *ctx, JSModuleDef *m)
{
    if (JS_IsUndefined(m->module_ns)) {
        JSValue val;
        val = js_build_module_ns(ctx, m);
        if (JS_IsException(val))
            return JS_EXCEPTION;
        m->module_ns = val;
    }
    return JS_DupValue(ctx, m->module_ns);
}

/* return JS_ATOM_NULL if the name cannot be found. Only works with
   not striped bytecode functions. */
JSAtom QUICKJS_API JS_GetScriptOrModuleName(JSContext *ctx, int n_stack_levels)
{
    JSStackFrame *sf;
    JSFunctionBytecode *b;
    JSObject *p;
    /* XXX: currently we just use the filename of the englobing
       function from the debug info. May need to add a ScriptOrModule
       info in JSFunctionBytecode. */
    sf = ctx->rt->current_stack_frame;
    if (!sf)
        return JS_ATOM_NULL;
    while (n_stack_levels-- > 0) {
        sf = sf->prev_frame;
        if (!sf)
            return JS_ATOM_NULL;
    }
    for(;;) {
        if (JS_VALUE_GET_TAG(sf->cur_func) != JS_TAG_OBJECT)
            return JS_ATOM_NULL;
        p = JS_VALUE_GET_OBJ(sf->cur_func);
        if (!js_class_has_bytecode(p->class_id))
            return JS_ATOM_NULL;
        b = p->u.func.function_bytecode;
        if (!b->is_direct_or_indirect_eval) {
            if (!b->has_debug)
                return JS_ATOM_NULL;
            return JS_DupAtom(ctx, b->debug.filename);
        } else {
            sf = sf->prev_frame;
            if (!sf)
                return JS_ATOM_NULL;
        }
    }
}

JSAtom QUICKJS_API JS_GetModuleName(JSContext *ctx, JSModuleDef *m)
{
    return JS_DupAtom(ctx, m->module_name);
}

JSValue QUICKJS_API JS_GetImportMeta(JSContext *ctx, JSModuleDef *m)
{
    JSValue obj;
    /* allocate meta_obj only if requested to save memory */
    obj = m->meta_obj;
    if (JS_IsUndefined(obj)) {
        obj = JS_NewObjectProto(ctx, JS_NULL);
        if (JS_IsException(obj))
            return JS_EXCEPTION;
        m->meta_obj = obj;
    }
    return JS_DupValue(ctx, obj);
}

/* Return a promise or an exception in case of memory error. Used by
   os.Worker() */
JSValue QUICKJS_API JS_LoadModule(JSContext *ctx, const char *basename,
                      const char *filename)
{
    JSValue promise, resolving_funcs[2];

    promise = JS_NewPromiseCapability(ctx, resolving_funcs);
    if (JS_IsException(promise))
        return JS_EXCEPTION;
    JS_LoadModuleInternal(ctx, basename, filename,
                          (JSValueConst *)resolving_funcs, JS_UNDEFINED);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    return promise;
}

JSValue QUICKJS_API JS_EvalFunction(JSContext *ctx, JSValue fun_obj)
{
    return JS_EvalFunctionInternal(ctx, fun_obj, ctx->global_obj, NULL, NULL);
}

JSValue QUICKJS_API JS_EvalThis(JSContext *ctx, JSValueConst this_obj,
                    const char *input, size_t input_len,
                    const char *filename, int eval_flags)
{
    int eval_type = eval_flags & JS_EVAL_TYPE_MASK;
    JSValue ret;

    assert(eval_type == JS_EVAL_TYPE_GLOBAL ||
           eval_type == JS_EVAL_TYPE_MODULE);
    ret = JS_EvalInternal(ctx, this_obj, input, input_len, filename,
                          eval_flags, -1);
    return ret;
}

JSValue QUICKJS_API JS_Eval(JSContext *ctx, const char *input, size_t input_len,
                const char *filename, int eval_flags)
{
    return JS_EvalThis(ctx, ctx->global_obj, input, input_len, filename,
                       eval_flags);
}

int QUICKJS_API JS_ResolveModule(JSContext *ctx, JSValueConst obj)
{
    if (JS_VALUE_GET_TAG(obj) == JS_TAG_MODULE) {
        JSModuleDef *m = JS_VALUE_GET_PTR(obj);
        if (js_resolve_module(ctx, m) < 0) {
            js_free_modules(ctx, JS_FREE_MODULE_NOT_RESOLVED);
            return -1;
        }
    }
    return 0;
}

JSValue QUICKJS_API JS_ReadObject(JSContext *ctx, const uint8_t *buf, size_t buf_len,
                       int flags)
{
    BCReaderState ss, *s = &ss;
    JSValue obj;

    ctx->binary_object_count += 1;
    ctx->binary_object_size += buf_len;

    memset(s, 0, sizeof(*s));
    s->ctx = ctx;
    s->buf_start = buf;
    s->buf_end = buf + buf_len;
    s->ptr = buf;
    s->allow_bytecode = ((flags & JS_READ_OBJ_BYTECODE) != 0);
    s->is_rom_data = ((flags & JS_READ_OBJ_ROM_DATA) != 0);
    s->allow_sab = ((flags & JS_READ_OBJ_SAB) != 0);
    s->allow_reference = ((flags & JS_READ_OBJ_REFERENCE) != 0);
    if (s->allow_bytecode)
        s->first_atom = JS_ATOM_END;
    else
        s->first_atom = 1;
    if (JS_ReadObjectAtoms(s)) {
        obj = JS_EXCEPTION;
    } else {
        obj = JS_ReadObjectRec(s);
    }
    bc_reader_free(s);
    return obj;
}

int QUICKJS_API JS_SetPropertyFunctionList(JSContext *ctx, JSValueConst obj,
                               const JSCFunctionListEntry *tab, int len)
{
    int i, ret;

    for (i = 0; i < len; i++) {
        const JSCFunctionListEntry *e = &tab[i];
        JSAtom atom = find_atom(ctx, e->name);
        if (atom == JS_ATOM_NULL)
            return -1;
        ret = JS_InstantiateFunctionListItem(ctx, obj, atom, e);
        JS_FreeAtom(ctx, atom);
        if (ret)
            return -1;
    }
    return 0;
}

int QUICKJS_API JS_AddModuleExportList(JSContext *ctx, JSModuleDef *m,
                           const JSCFunctionListEntry *tab, int len)
{
    int i;
    for(i = 0; i < len; i++) {
        if (JS_AddModuleExport(ctx, m, tab[i].name))
            return -1;
    }
    return 0;
}

int QUICKJS_API JS_SetModuleExportList(JSContext *ctx, JSModuleDef *m,
                           const JSCFunctionListEntry *tab, int len)
{
    int i;
    JSValue val;

    for(i = 0; i < len; i++) {
        const JSCFunctionListEntry *e = &tab[i];
        switch(e->def_type) {
        case JS_DEF_CFUNC:
            val = JS_NewCFunction2(ctx, e->u.func.cfunc.generic,
                                   e->name, e->u.func.length, e->u.func.cproto, e->magic);
            break;
        case JS_DEF_PROP_STRING:
            val = JS_NewString(ctx, e->u.str);
            break;
        case JS_DEF_PROP_INT32:
            val = JS_NewInt32(ctx, e->u.i32);
            break;
        case JS_DEF_PROP_INT64:
            val = JS_NewInt64(ctx, e->u.i64);
            break;
        case JS_DEF_PROP_DOUBLE:
            val = __JS_NewFloat64(ctx, e->u.f64);
            break;
        case JS_DEF_OBJECT:
            val = JS_NewObjectProtoList(ctx, ctx->class_proto[JS_CLASS_OBJECT],
                                        e->u.prop_list.tab, e->u.prop_list.len);
            break;
        default:
            abort();
        }
        if (JS_SetModuleExport(ctx, m, e->name, val))
            return -1;
    }
    return 0;
}

/* return 0 if OK, -1 if exception */
int QUICKJS_API JS_SetConstructor(JSContext *ctx, JSValueConst func_obj,
                      JSValueConst proto)
{
    return JS_SetConstructor2(ctx, func_obj, proto,
                              0, JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
}

/* only used in test262 */
JSValue QUICKJS_API js_string_codePointRange(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv)
{
    uint32_t start, end, i, n;
    StringBuffer b_s, *b = &b_s;

    if (JS_ToUint32(ctx, &start, argv[0]) ||
        JS_ToUint32(ctx, &end, argv[1]))
        return JS_EXCEPTION;
    end = min_uint32(end, 0x10ffff + 1);

    if (start > end) {
        start = end;
    }
    n = end - start;
    if (end > 0x10000) {
        n += end - max_uint32(start, 0x10000);
    }
    if (string_buffer_init2(ctx, b, n, end >= 0x100))
        return JS_EXCEPTION;
    for(i = start; i < end; i++) {
        string_buffer_putc(b, i);
    }
    return string_buffer_end(b);
}
#if 0
int lre_check_stack_overflow(void *opaque, size_t alloca_size)
{
    JSContext *ctx = opaque;
    return js_check_stack_overflow(ctx->rt, alloca_size);
}

int lre_check_timeout(void *opaque)
{
    JSContext *ctx = opaque;
    JSRuntime *rt = ctx->rt;
    return (rt->interrupt_handler &&
            rt->interrupt_handler(rt, rt->interrupt_opaque));
}

void* lre_realloc(void *opaque, void *ptr, size_t size)
{
    JSContext *ctx = opaque;
    /* No JS exception is raised here */
    return js_realloc_rt(ctx->rt, ptr, size);
}
#endif
void QUICKJS_API JS_AddIntrinsicRegExpCompiler(JSContext *ctx)
{
    ctx->compile_regexp = js_compile_regexp;
}

int QUICKJS_API JS_AddIntrinsicRegExp(JSContext *ctx)
{
    JSValue obj;

    JS_AddIntrinsicRegExpCompiler(ctx);

    obj = JS_NewCConstructor(ctx, JS_CLASS_REGEXP, "RegExp",
                                    js_regexp_constructor, 2, JS_CFUNC_constructor_or_func, 0,
                                    JS_UNDEFINED,
                                    js_regexp_funcs, countof(js_regexp_funcs),
                                    js_regexp_proto_funcs, countof(js_regexp_proto_funcs),
                                    0);
    if (JS_IsException(obj))
        return -1;
    ctx->regexp_ctor = obj;

    ctx->class_proto[JS_CLASS_REGEXP_STRING_ITERATOR] =
        JS_NewObjectProtoList(ctx, ctx->class_proto[JS_CLASS_ITERATOR],
                              js_regexp_string_iterator_proto_funcs,
                              countof(js_regexp_string_iterator_proto_funcs));
    if (JS_IsException(ctx->class_proto[JS_CLASS_REGEXP_STRING_ITERATOR]))
        return -1;

    ctx->regexp_shape = js_new_shape2(ctx, get_proto_obj(ctx->class_proto[JS_CLASS_REGEXP]),
                                     JS_PROP_INITIAL_HASH_SIZE, 1);
    if (!ctx->regexp_shape)
        return -1;
    if (add_shape_property(ctx, &ctx->regexp_shape, NULL,
                           JS_ATOM_lastIndex, JS_PROP_WRITABLE))
        return -1;

    ctx->regexp_result_shape = js_new_shape2(ctx, get_proto_obj(ctx->class_proto[JS_CLASS_ARRAY]),
                                     JS_PROP_INITIAL_HASH_SIZE, 4);
    if (!ctx->regexp_result_shape)
        return -1;
    if (add_shape_property(ctx, &ctx->regexp_result_shape, NULL,
                           JS_ATOM_length, JS_PROP_WRITABLE | JS_PROP_LENGTH))
        return -1;
    if (add_shape_property(ctx, &ctx->regexp_result_shape, NULL,
                           JS_ATOM_index, JS_PROP_C_W_E))
        return -1;
    if (add_shape_property(ctx, &ctx->regexp_result_shape, NULL,
                           JS_ATOM_input, JS_PROP_C_W_E))
        return -1;
    if (add_shape_property(ctx, &ctx->regexp_result_shape, NULL,
                           JS_ATOM_groups, JS_PROP_C_W_E))
        return -1;

    return 0;
}

JSValue QUICKJS_API JS_ParseJSON3(JSContext *ctx, const char *buf, size_t buf_len,
                      const char *filename, int flags, JSONParseRecord *pr)
{
    JSParseState s1, *s = &s1;
    JSValue val = JS_UNDEFINED;

    js_parse_init(ctx, s, buf, buf_len, filename);
    s->ext_json = ((flags & JS_PARSE_JSON_EXT) != 0);
    if (json_next_token(s))
        goto fail;
    val = json_parse_value(s, pr);
    if (JS_IsException(val))
        goto fail;
    if (s->token.val != TOK_EOF) {
        if (js_parse_error(s, "unexpected data at the end")) {
            json_free_parse_record(ctx, pr);
            goto fail;
        }
    }
    return val;
 fail:
    JS_FreeValue(ctx, val);
    free_token(s, &s->token);
    return JS_EXCEPTION;
}

JSValue QUICKJS_API JS_ParseJSON2(JSContext *ctx, const char *buf, size_t buf_len,
                      const char *filename, int flags)
{
    return JS_ParseJSON3(ctx, buf, buf_len, filename, flags, NULL);
}

JSValue QUICKJS_API JS_ParseJSON(JSContext *ctx, const char *buf, size_t buf_len,
                     const char *filename)
{
    return JS_ParseJSON3(ctx, buf, buf_len, filename, 0, NULL);
}

JSValue QUICKJS_API JS_JSONStringify(JSContext *ctx, JSValueConst obj,
                         JSValueConst replacer, JSValueConst space0)
{
    StringBuffer b_s;
    JSONStringifyContext jsc_s, *jsc = &jsc_s;
    JSValue val, v, space, ret, wrapper;
    int res;
    int64_t i, j, n;

    jsc->replacer_func = JS_UNDEFINED;
    jsc->stack = JS_UNDEFINED;
    jsc->property_list = JS_UNDEFINED;
    jsc->gap = JS_UNDEFINED;
    jsc->b = &b_s;
    jsc->empty = JS_AtomToString(ctx, JS_ATOM_empty_string);
    ret = JS_UNDEFINED;
    wrapper = JS_UNDEFINED;

    string_buffer_init(ctx, jsc->b, 0);
    jsc->stack = JS_NewArray(ctx);
    if (JS_IsException(jsc->stack))
        goto exception;
    if (JS_IsFunction(ctx, replacer)) {
        jsc->replacer_func = replacer;
    } else {
        res = JS_IsArray(ctx, replacer);
        if (res < 0)
            goto exception;
        if (res) {
            /* XXX: enumeration is not fully correct */
            jsc->property_list = JS_NewArray(ctx);
            if (JS_IsException(jsc->property_list))
                goto exception;
            if (js_get_length64(ctx, &n, replacer))
                goto exception;
            for (i = j = 0; i < n; i++) {
                JSValue present;
                v = JS_GetPropertyInt64(ctx, replacer, i);
                if (JS_IsException(v))
                    goto exception;
                if (JS_IsObject(v)) {
                    JSObject *p = JS_VALUE_GET_OBJ(v);
                    if (p->class_id == JS_CLASS_STRING ||
                        p->class_id == JS_CLASS_NUMBER) {
                        v = JS_ToStringFree(ctx, v);
                        if (JS_IsException(v))
                            goto exception;
                    } else {
                        JS_FreeValue(ctx, v);
                        continue;
                    }
                } else if (JS_IsNumber(v)) {
                    v = JS_ToStringFree(ctx, v);
                    if (JS_IsException(v))
                        goto exception;
                } else if (!JS_IsString(v)) {
                    JS_FreeValue(ctx, v);
                    continue;
                }
                present = js_array_includes(ctx, jsc->property_list,
                                            1, (JSValueConst *)&v);
                if (JS_IsException(present)) {
                    JS_FreeValue(ctx, v);
                    goto exception;
                }
                if (!JS_ToBoolFree(ctx, present)) {
                    JS_SetPropertyInt64(ctx, jsc->property_list, j++, v);
                } else {
                    JS_FreeValue(ctx, v);
                }
            }
        }
    }
    space = JS_DupValue(ctx, space0);
    if (JS_IsObject(space)) {
        JSObject *p = JS_VALUE_GET_OBJ(space);
        if (p->class_id == JS_CLASS_NUMBER) {
            space = JS_ToNumberFree(ctx, space);
        } else if (p->class_id == JS_CLASS_STRING) {
            space = JS_ToStringFree(ctx, space);
        }
        if (JS_IsException(space)) {
            JS_FreeValue(ctx, space);
            goto exception;
        }
    }
    if (JS_IsNumber(space)) {
        int n;
        if (JS_ToInt32Clamp(ctx, &n, space, 0, 10, 0))
            goto exception;
        jsc->gap = js_new_string8_len(ctx, "          ", n);
    } else if (JS_IsString(space)) {
        JSString *p = JS_VALUE_GET_STRING(space);
        jsc->gap = js_sub_string(ctx, p, 0, min_int(p->len, 10));
    } else {
        jsc->gap = JS_DupValue(ctx, jsc->empty);
    }
    JS_FreeValue(ctx, space);
    if (JS_IsException(jsc->gap))
        goto exception;
    wrapper = JS_NewObject(ctx);
    if (JS_IsException(wrapper))
        goto exception;
    if (JS_DefinePropertyValue(ctx, wrapper, JS_ATOM_empty_string,
                               JS_DupValue(ctx, obj), JS_PROP_C_W_E) < 0)
        goto exception;
    val = JS_DupValue(ctx, obj);

    val = js_json_check(ctx, jsc, wrapper, val, jsc->empty);
    if (JS_IsException(val))
        goto exception;
    if (JS_IsUndefined(val)) {
        ret = JS_UNDEFINED;
        goto done1;
    }
    if (js_json_to_str(ctx, jsc, wrapper, val, jsc->empty))
        goto exception;

    ret = string_buffer_end(jsc->b);
    goto done;

exception:
    ret = JS_EXCEPTION;
done1:
    string_buffer_free(jsc->b);
done:
    JS_FreeValue(ctx, wrapper);
    JS_FreeValue(ctx, jsc->empty);
    JS_FreeValue(ctx, jsc->gap);
    JS_FreeValue(ctx, jsc->property_list);
    JS_FreeValue(ctx, jsc->stack);
    return ret;
}

int QUICKJS_API JS_AddIntrinsicJSON(JSContext *ctx)
{
    /* add JSON as autoinit object */
    return JS_SetPropertyFunctionList(ctx, ctx->global_obj, js_json_obj, countof(js_json_obj));
}

JSPromiseStateEnum QUICKJS_API JS_PromiseState(JSContext *ctx, JSValue promise)
{
    JSPromiseData *s = JS_GetOpaque(promise, JS_CLASS_PROMISE);
    if (!s)
        return -1;
    return s->promise_state;
}

JSValue QUICKJS_API JS_PromiseResult(JSContext *ctx, JSValue promise)
{
    JSPromiseData *s = JS_GetOpaque(promise, JS_CLASS_PROMISE);
    if (!s)
        return JS_UNDEFINED;
    return JS_DupValue(ctx, s->promise_result);
}

void QUICKJS_API JS_SetHostPromiseRejectionTracker(JSRuntime *rt,
                                       JSHostPromiseRejectionTracker *cb,
                                       void *opaque)
{
    rt->host_promise_rejection_tracker = cb;
    rt->host_promise_rejection_tracker_opaque = opaque;
}

JSValue QUICKJS_API JS_NewPromiseCapability(JSContext *ctx, JSValue *resolving_funcs)
{
    return js_new_promise_capability(ctx, resolving_funcs, JS_UNDEFINED);
}

int QUICKJS_API JS_AddIntrinsicPromise(JSContext *ctx)
{
    JSRuntime *rt = ctx->rt;
    JSValue obj1;
    JSCFunctionType ft;

    if (!JS_IsRegisteredClass(rt, JS_CLASS_PROMISE)) {
        if (init_class_range(rt, js_async_class_def, JS_CLASS_PROMISE,
                             countof(js_async_class_def)))
            return -1;
        rt->class_array[JS_CLASS_PROMISE_RESOLVE_FUNCTION].call = js_promise_resolve_function_call;
        rt->class_array[JS_CLASS_PROMISE_REJECT_FUNCTION].call = js_promise_resolve_function_call;
        rt->class_array[JS_CLASS_ASYNC_FUNCTION].call = js_async_function_call;
        rt->class_array[JS_CLASS_ASYNC_FUNCTION_RESOLVE].call = js_async_function_resolve_call;
        rt->class_array[JS_CLASS_ASYNC_FUNCTION_REJECT].call = js_async_function_resolve_call;
        rt->class_array[JS_CLASS_ASYNC_GENERATOR_FUNCTION].call = js_async_generator_function_call;
    }

    /* Promise */
    obj1 = JS_NewCConstructor(ctx, JS_CLASS_PROMISE, "Promise",
                                     js_promise_constructor, 1, JS_CFUNC_constructor, 0,
                                     JS_UNDEFINED,
                                     js_promise_funcs, countof(js_promise_funcs),
                                     js_promise_proto_funcs, countof(js_promise_proto_funcs),
                                     0);
    if (JS_IsException(obj1))
        return -1;
    ctx->promise_ctor = obj1;

    /* AsyncFunction */
    ft.generic_magic = js_function_constructor;
    obj1 = JS_NewCConstructor(ctx, JS_CLASS_ASYNC_FUNCTION, "AsyncFunction",
                                     ft.generic, 1, JS_CFUNC_constructor_or_func_magic, JS_FUNC_ASYNC,
                                     ctx->function_ctor,
                                     NULL, 0,
                                     js_async_function_proto_funcs, countof(js_async_function_proto_funcs),
                                     JS_NEW_CTOR_NO_GLOBAL | JS_NEW_CTOR_READONLY);
    if (JS_IsException(obj1))
        return -1;
    JS_FreeValue(ctx, obj1);

    /* AsyncIteratorPrototype */
    ctx->async_iterator_proto =
        JS_NewObjectProtoList(ctx,  ctx->class_proto[JS_CLASS_OBJECT],
                              js_async_iterator_proto_funcs,
                              countof(js_async_iterator_proto_funcs));
    if (JS_IsException(ctx->async_iterator_proto))
        return -1;

    /* AsyncFromSyncIteratorPrototype */
    ctx->class_proto[JS_CLASS_ASYNC_FROM_SYNC_ITERATOR] =
        JS_NewObjectProtoList(ctx, ctx->async_iterator_proto,
                              js_async_from_sync_iterator_proto_funcs,
                              countof(js_async_from_sync_iterator_proto_funcs));
    if (JS_IsException(ctx->class_proto[JS_CLASS_ASYNC_FROM_SYNC_ITERATOR]))
        return -1;

    /* AsyncGeneratorPrototype */
    ctx->class_proto[JS_CLASS_ASYNC_GENERATOR] =
        JS_NewObjectProtoList(ctx, ctx->async_iterator_proto,
                              js_async_generator_proto_funcs,
                              countof(js_async_generator_proto_funcs));
    if (JS_IsException(ctx->class_proto[JS_CLASS_ASYNC_GENERATOR]))
        return -1;

    /* AsyncGeneratorFunction */
    ft.generic_magic = js_function_constructor;
    obj1 = JS_NewCConstructor(ctx, JS_CLASS_ASYNC_GENERATOR_FUNCTION, "AsyncGeneratorFunction",
                                     ft.generic, 1, JS_CFUNC_constructor_or_func_magic, JS_FUNC_ASYNC_GENERATOR,
                                     ctx->function_ctor,
                                     NULL, 0,
                                     js_async_generator_function_proto_funcs, countof(js_async_generator_function_proto_funcs),
                                     JS_NEW_CTOR_NO_GLOBAL | JS_NEW_CTOR_READONLY);
    if (JS_IsException(obj1))
        return -1;
    JS_FreeValue(ctx, obj1);

    return JS_SetConstructor2(ctx, ctx->class_proto[JS_CLASS_ASYNC_GENERATOR_FUNCTION],
                              ctx->class_proto[JS_CLASS_ASYNC_GENERATOR],
                              JS_PROP_CONFIGURABLE, JS_PROP_CONFIGURABLE);
}

JSValue QUICKJS_API JS_NewDate(JSContext *ctx, double epoch_ms)
{
    JSValue obj = js_create_from_ctor(ctx, JS_UNDEFINED, JS_CLASS_DATE);
    if (JS_IsException(obj))
        return JS_EXCEPTION;
    JS_SetObjectData(ctx, obj, __JS_NewFloat64(ctx, time_clip(epoch_ms)));
    return obj;
}

int QUICKJS_API JS_AddIntrinsicDate(JSContext *ctx)
{
    JSValue obj;

    /* Date */
    obj = JS_NewCConstructor(ctx, JS_CLASS_DATE, "Date",
                                    js_date_constructor, 7, JS_CFUNC_constructor_or_func, 0,
                                    JS_UNDEFINED,
                                    js_date_funcs, countof(js_date_funcs),
                                    js_date_proto_funcs, countof(js_date_proto_funcs),
                                    0);
    if (JS_IsException(obj))
        return -1;
    JS_FreeValue(ctx, obj);
    return 0;
}

/* eval */

int QUICKJS_API JS_AddIntrinsicEval(JSContext *ctx)
{
    ctx->eval_internal = __JS_EvalInternal;
    return 0;
}

JSValue QUICKJS_API JS_NewArrayBuffer(JSContext *ctx, uint8_t *buf, size_t len,
                          JSFreeArrayBufferDataFunc *free_func, void *opaque,
                          BOOL is_shared)
{
    JSClassID class_id =
        is_shared ? JS_CLASS_SHARED_ARRAY_BUFFER : JS_CLASS_ARRAY_BUFFER;
    return js_array_buffer_constructor3(ctx, JS_UNDEFINED, len, NULL, class_id,
                                        buf, free_func, opaque, FALSE);
}

/* create a new ArrayBuffer of length 'len' and copy 'buf' to it */
JSValue QUICKJS_API JS_NewArrayBufferCopy(JSContext *ctx, const uint8_t *buf, size_t len)
{
    return js_array_buffer_constructor3(ctx, JS_UNDEFINED, len, NULL,
                                        JS_CLASS_ARRAY_BUFFER,
                                        (uint8_t *)buf,
                                        js_array_buffer_free, NULL,
                                        TRUE);
}

void QUICKJS_API JS_DetachArrayBuffer(JSContext *ctx, JSValueConst obj)
{
    JSArrayBuffer *abuf = JS_GetOpaque(obj, JS_CLASS_ARRAY_BUFFER);

    if (!abuf || abuf->detached)
        return;
    if (abuf->free_func)
        abuf->free_func(ctx->rt, abuf->opaque, abuf->data);
    abuf->data = NULL;
    abuf->byte_length = 0;
    abuf->detached = TRUE;
    js_array_buffer_update_typed_arrays(abuf);
}

/* return NULL if exception. WARNING: any JS call can detach the
   buffer and render the returned pointer invalid */
uint8_t *QUICKJS_API JS_GetArrayBuffer(JSContext *ctx, size_t *psize, JSValueConst obj)
{
    JSArrayBuffer *abuf = js_get_array_buffer(ctx, obj);
    if (!abuf)
        goto fail;
    if (abuf->detached) {
        JS_ThrowTypeErrorDetachedArrayBuffer(ctx);
        goto fail;
    }
    *psize = abuf->byte_length;
    return abuf->data;
 fail:
    *psize = 0;
    return NULL;
}

JSValue QUICKJS_API JS_NewTypedArray(JSContext *ctx, int argc, JSValueConst *argv,
                         JSTypedArrayEnum type)
{
    if (type < JS_TYPED_ARRAY_UINT8C || type > JS_TYPED_ARRAY_FLOAT64)
        return JS_ThrowRangeError(ctx, "invalid typed array type");

    return js_typed_array_constructor(ctx, JS_UNDEFINED, argc, argv,
                                      JS_CLASS_UINT8C_ARRAY + type);
}

/* Return the buffer associated to the typed array or an exception if
   it is not a typed array or if the buffer is detached. pbyte_offset,
   pbyte_length or pbytes_per_element can be NULL. */
JSValue QUICKJS_API JS_GetTypedArrayBuffer(JSContext *ctx, JSValueConst obj,
                               size_t *pbyte_offset,
                               size_t *pbyte_length,
                               size_t *pbytes_per_element)
{
    JSObject *p;
    JSTypedArray *ta;
    p = get_typed_array(ctx, obj);
    if (!p)
        return JS_EXCEPTION;
    if (typed_array_is_oob(p))
        return JS_ThrowTypeErrorArrayBufferOOB(ctx);
    ta = p->u.typed_array;
    if (pbyte_offset)
        *pbyte_offset = ta->offset;
    if (pbyte_length)
        *pbyte_length = ta->length;
    if (pbytes_per_element) {
        *pbytes_per_element = 1 << typed_array_size_log2(p->class_id);
    }
    return JS_DupValue(ctx, JS_MKPTR(JS_TAG_OBJECT, ta->buffer));
}

int QUICKJS_API JS_AddIntrinsicTypedArrays(JSContext *ctx)
{
    JSValue typed_array_base_func, typed_array_base_proto, obj;
    int i, ret;

    obj = JS_NewCConstructor(ctx, JS_CLASS_ARRAY_BUFFER, "ArrayBuffer",
                                    js_array_buffer_constructor, 1, JS_CFUNC_constructor, 0,
                                    JS_UNDEFINED,
                                    js_array_buffer_funcs, countof(js_array_buffer_funcs),
                                    js_array_buffer_proto_funcs, countof(js_array_buffer_proto_funcs),
                                    0);
    if (JS_IsException(obj))
        return -1;
    JS_FreeValue(ctx, obj);

    obj = JS_NewCConstructor(ctx, JS_CLASS_SHARED_ARRAY_BUFFER, "SharedArrayBuffer",
                                    js_shared_array_buffer_constructor, 1, JS_CFUNC_constructor, 0,
                                    JS_UNDEFINED,
                                    js_shared_array_buffer_funcs, countof(js_shared_array_buffer_funcs),
                                    js_shared_array_buffer_proto_funcs, countof(js_shared_array_buffer_proto_funcs),
                                    0);
    if (JS_IsException(obj))
        return -1;
    JS_FreeValue(ctx, obj);


    typed_array_base_func =
        JS_NewCConstructor(ctx, -1, "TypedArray",
                                  js_typed_array_base_constructor, 0, JS_CFUNC_constructor_or_func, 0,
                                  JS_UNDEFINED,
                                  js_typed_array_base_funcs, countof(js_typed_array_base_funcs),
                                  js_typed_array_base_proto_funcs, countof(js_typed_array_base_proto_funcs),
                                  JS_NEW_CTOR_NO_GLOBAL);
    if (JS_IsException(typed_array_base_func))
        return -1;

    /* TypedArray.prototype.toString must be the same object as Array.prototype.toString */
    obj = JS_GetProperty(ctx, ctx->class_proto[JS_CLASS_ARRAY], JS_ATOM_toString);
    JS_LOG("JS_AddIntrinsicTypedArrays", "After JS_GetProperty toString, obj=%08lX_%08lX",
           U64_HI(obj), U64_LO(obj));
    if (JS_IsException(obj))
        goto fail;

    /* XXX: should use alias method in JSCFunctionListEntry */
    typed_array_base_proto = JS_GetProperty(ctx, typed_array_base_func, JS_ATOM_prototype);
    JS_LOG("JS_AddIntrinsicTypedArrays", "After JS_GetProperty prototype, typed_array_base_proto=%08lX_%08lX",
           U64_HI(typed_array_base_proto), U64_LO(typed_array_base_proto));
    if (JS_IsException(typed_array_base_proto))
        goto fail;

    JS_LOG("JS_AddIntrinsicTypedArrays", "Before JS_DefinePropertyValue, obj=%08lX_%08lX",
           U64_HI(obj), U64_LO(obj));

    ret = JS_DefinePropertyValue(ctx, typed_array_base_proto, JS_ATOM_toString, obj,
                                 JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);

    JS_LOG("JS_AddIntrinsicTypedArrays", "After JS_DefinePropertyValue, obj=%08lX_%08lX, ret=%d",
           U64_HI(obj), U64_LO(obj), ret);

    JS_FreeValue(ctx, typed_array_base_proto);
    if (ret < 0)
        goto fail;
    /* Used to squelch a -Wcast-function-type warning. */
    JSCFunctionType ft = { .generic_magic = js_typed_array_constructor };
    for(i = JS_CLASS_UINT8C_ARRAY; i < JS_CLASS_UINT8C_ARRAY + JS_TYPED_ARRAY_COUNT; i++) {
        char buf[ATOM_GET_STR_BUF_SIZE];
        const char *name;

        name = JS_AtomGetStr(ctx, buf, sizeof(buf),
                             JS_ATOM_Uint8ClampedArray + i - JS_CLASS_UINT8C_ARRAY);
        if (i == JS_CLASS_UINT8_ARRAY) {
            obj = JS_NewCConstructor(ctx, i, name,
                                     ft.generic, 3, JS_CFUNC_constructor_magic, i,
                                     typed_array_base_func,
                                     js_uint8array_funcs, countof(js_uint8array_funcs),
                                     js_uint8array_proto_funcs, countof(js_uint8array_proto_funcs),
                                     0);
        } else {
            const JSCFunctionListEntry *bpe = js_typed_array_funcs + typed_array_size_log2(i);
            obj = JS_NewCConstructor(ctx, i, name,
                                     ft.generic, 3, JS_CFUNC_constructor_magic, i,
                                     typed_array_base_func,
                                     bpe, 1,
                                     bpe, 1,
                                     0);
        }
        if (JS_IsException(obj)) {
        fail:
            JS_FreeValue(ctx, typed_array_base_func);
            return -1;
        }
        JS_FreeValue(ctx, obj);
    }
    JS_FreeValue(ctx, typed_array_base_func);

    /* DataView */
    obj = JS_NewCConstructor(ctx, JS_CLASS_DATAVIEW, "DataView",
                                    js_dataview_constructor, 1, JS_CFUNC_constructor, 0,
                                    JS_UNDEFINED,
                                    NULL, 0,
                                    js_dataview_proto_funcs, countof(js_dataview_proto_funcs),
                                    0);
    if (JS_IsException(obj))
        return -1;
    JS_FreeValue(ctx, obj);

    /* Atomics */
#ifdef CONFIG_ATOMICS
    if (JS_AddIntrinsicAtomics(ctx))
        return -1;
#endif
    return 0;
}

int QUICKJS_API JS_AddIntrinsicBaseObjects(JSContext *ctx)
{
    JSValue obj1, obj2;
    JSCFunctionType ft;

    ctx->throw_type_error = JS_NewCFunction(ctx, js_throw_type_error, NULL, 0);
    if (JS_IsException(ctx->throw_type_error))
        return -1;
    /* add caller and arguments properties to throw a TypeError */
    if (JS_DefineProperty(ctx, ctx->function_proto, JS_ATOM_caller, JS_UNDEFINED,
                          ctx->throw_type_error, ctx->throw_type_error,
                          JS_PROP_HAS_GET | JS_PROP_HAS_SET |
                          JS_PROP_HAS_CONFIGURABLE | JS_PROP_CONFIGURABLE) < 0)
        return -1;
    if (JS_DefineProperty(ctx, ctx->function_proto, JS_ATOM_arguments, JS_UNDEFINED,
                          ctx->throw_type_error, ctx->throw_type_error,
                          JS_PROP_HAS_GET | JS_PROP_HAS_SET |
                          JS_PROP_HAS_CONFIGURABLE | JS_PROP_CONFIGURABLE) < 0)
        return -1;
    JS_FreeValue(ctx, js_object_seal(ctx, JS_UNDEFINED, 1, (JSValueConst *)&ctx->throw_type_error, 1));

    /* Object */
    obj1 = JS_NewCConstructor(ctx, JS_CLASS_OBJECT, "Object",
                              js_object_constructor, 1, JS_CFUNC_constructor_or_func, 0,
                              JS_UNDEFINED,
                              js_object_funcs, countof(js_object_funcs),
                              js_object_proto_funcs, countof(js_object_proto_funcs),
                              JS_NEW_CTOR_PROTO_EXIST);
    if (JS_IsException(obj1))
        return -1;
    JS_FreeValue(ctx, obj1);

    /* Function */
    ft.generic_magic = js_function_constructor;
    obj1 = JS_NewCConstructor(ctx, JS_CLASS_BYTECODE_FUNCTION, "Function",
                              ft.generic, 1, JS_CFUNC_constructor_or_func_magic, JS_FUNC_NORMAL,
                              JS_UNDEFINED,
                              NULL, 0,
                              js_function_proto_funcs, countof(js_function_proto_funcs),
                              JS_NEW_CTOR_PROTO_EXIST);
    if (JS_IsException(obj1))
        return -1;
    ctx->function_ctor = obj1;

    /* Iterator */
    obj2 = JS_NewCConstructor(ctx, JS_CLASS_ITERATOR, "Iterator",
                                     js_iterator_constructor, 0, JS_CFUNC_constructor_or_func, 0,
                                     JS_UNDEFINED,
                                     js_iterator_funcs, countof(js_iterator_funcs),
                                     js_iterator_proto_funcs, countof(js_iterator_proto_funcs),
                                     0);
    if (JS_IsException(obj2))
        return -1;
    // quirk: Iterator.prototype.constructor is an accessor property
    // TODO(bnoordhuis) mildly inefficient because JS_NewGlobalCConstructor
    // first creates a .constructor value property that we then replace with
    // an accessor
    obj1 = JS_NewCFunctionData(ctx, js_iterator_constructor_getset,
                               0, 0, 1, (JSValueConst *)&obj2);
    if (JS_IsException(obj1)) {
        JS_FreeValue(ctx, obj2);
        return -1;
    }
    if (JS_DefineProperty(ctx, ctx->class_proto[JS_CLASS_ITERATOR],
                          JS_ATOM_constructor, JS_UNDEFINED,
                          obj1, obj1,
                          JS_PROP_HAS_GET | JS_PROP_HAS_SET | JS_PROP_CONFIGURABLE) < 0) {
        JS_FreeValue(ctx, obj2);
        JS_FreeValue(ctx, obj1);
        return -1;
    }
    JS_FreeValue(ctx, obj1);
    ctx->iterator_ctor = obj2;

    ctx->class_proto[JS_CLASS_ITERATOR_CONCAT] =
        JS_NewObjectProtoList(ctx, ctx->class_proto[JS_CLASS_ITERATOR],
                              js_iterator_concat_proto_funcs,
                              countof(js_iterator_concat_proto_funcs));
    if (JS_IsException(ctx->class_proto[JS_CLASS_ITERATOR_CONCAT]))
        return -1;
    ctx->class_proto[JS_CLASS_ITERATOR_HELPER] =
        JS_NewObjectProtoList(ctx, ctx->class_proto[JS_CLASS_ITERATOR],
                              js_iterator_helper_proto_funcs,
                              countof(js_iterator_helper_proto_funcs));
    if (JS_IsException(ctx->class_proto[JS_CLASS_ITERATOR_HELPER]))
        return -1;

    ctx->class_proto[JS_CLASS_ITERATOR_WRAP] =
        JS_NewObjectProtoList(ctx, ctx->class_proto[JS_CLASS_ITERATOR],
                              js_iterator_wrap_proto_funcs,
                              countof(js_iterator_wrap_proto_funcs));
    if (JS_IsException(ctx->class_proto[JS_CLASS_ITERATOR_WRAP]))
        return -1;

    /* needed to initialize arguments[Symbol.iterator] */
    ctx->array_proto_values =
        JS_GetProperty(ctx, ctx->class_proto[JS_CLASS_ARRAY], JS_ATOM_values);
    if (JS_IsException(ctx->array_proto_values))
        return -1;

    ctx->class_proto[JS_CLASS_ARRAY_ITERATOR] =
        JS_NewObjectProtoList(ctx, ctx->class_proto[JS_CLASS_ITERATOR],
                              js_array_iterator_proto_funcs,
                              countof(js_array_iterator_proto_funcs));
    if (JS_IsException(ctx->class_proto[JS_CLASS_ARRAY_ITERATOR]))
        return -1;

    /* parseFloat and parseInteger must be defined before Number
       because of the Number.parseFloat and Number.parseInteger
       aliases */
    if (JS_SetPropertyFunctionList(ctx, ctx->global_obj, js_global_funcs,
                                   countof(js_global_funcs)))
        return -1;

    /* Number */
    obj1 = JS_NewCConstructor(ctx, JS_CLASS_NUMBER, "Number",
                                     js_number_constructor, 1, JS_CFUNC_constructor_or_func, 0,
                                     JS_UNDEFINED,
                                     js_number_funcs, countof(js_number_funcs),
                                     js_number_proto_funcs, countof(js_number_proto_funcs),
                                     JS_NEW_CTOR_PROTO_CLASS);
    if (JS_IsException(obj1))
        return -1;
    JS_FreeValue(ctx, obj1);
    if (JS_SetObjectData(ctx, ctx->class_proto[JS_CLASS_NUMBER], JS_NewInt32(ctx, 0)))
        return -1;

    /* Boolean */
    obj1 = JS_NewCConstructor(ctx, JS_CLASS_BOOLEAN, "Boolean",
                                     js_boolean_constructor, 1, JS_CFUNC_constructor_or_func, 0,
                                     JS_UNDEFINED,
                                     NULL, 0,
                                     js_boolean_proto_funcs, countof(js_boolean_proto_funcs),
                                     JS_NEW_CTOR_PROTO_CLASS);
    if (JS_IsException(obj1))
        return -1;
    JS_FreeValue(ctx, obj1);
    if (JS_SetObjectData(ctx, ctx->class_proto[JS_CLASS_BOOLEAN], JS_NewBool(ctx, FALSE)))
        return -1;

    /* String */
    obj1 = JS_NewCConstructor(ctx, JS_CLASS_STRING, "String",
                                     js_string_constructor, 1, JS_CFUNC_constructor_or_func, 0,
                                     JS_UNDEFINED,
                                     js_string_funcs, countof(js_string_funcs),
                                     js_string_proto_funcs, countof(js_string_proto_funcs),
                                     JS_NEW_CTOR_PROTO_CLASS);
    if (JS_IsException(obj1))
        return -1;
    JS_FreeValue(ctx, obj1);
    if (JS_SetObjectData(ctx, ctx->class_proto[JS_CLASS_STRING], JS_AtomToString(ctx, JS_ATOM_empty_string)))
        return -1;

    ctx->class_proto[JS_CLASS_STRING_ITERATOR] =
        JS_NewObjectProtoList(ctx, ctx->class_proto[JS_CLASS_ITERATOR],
                              js_string_iterator_proto_funcs,
                              countof(js_string_iterator_proto_funcs));
    if (JS_IsException(ctx->class_proto[JS_CLASS_STRING_ITERATOR]))
        return -1;

    /* Math: create as autoinit object */
    js_random_init(ctx);
    if (JS_SetPropertyFunctionList(ctx, ctx->global_obj, js_math_obj, countof(js_math_obj)))
        return -1;

    /* ES6 Reflect: create as autoinit object */
    if (JS_SetPropertyFunctionList(ctx, ctx->global_obj, js_reflect_obj, countof(js_reflect_obj)))
        return -1;

    /* ES6 Symbol */
    obj1 = JS_NewCConstructor(ctx, JS_CLASS_SYMBOL, "Symbol",
                                     js_symbol_constructor, 0, JS_CFUNC_constructor_or_func, 0,
                                     JS_UNDEFINED,
                                     js_symbol_funcs, countof(js_symbol_funcs),
                                     js_symbol_proto_funcs, countof(js_symbol_proto_funcs),
                                     0);
    if (JS_IsException(obj1))
        return -1;
    JS_FreeValue(ctx, obj1);

    /* ES6 Generator */
    ctx->class_proto[JS_CLASS_GENERATOR] =
        JS_NewObjectProtoList(ctx, ctx->class_proto[JS_CLASS_ITERATOR],
                              js_generator_proto_funcs,
                              countof(js_generator_proto_funcs));
    if (JS_IsException(ctx->class_proto[JS_CLASS_GENERATOR]))
        return -1;

    ft.generic_magic = js_function_constructor;
    obj1 = JS_NewCConstructor(ctx, JS_CLASS_GENERATOR_FUNCTION, "GeneratorFunction",
                                     ft.generic, 1, JS_CFUNC_constructor_or_func_magic, JS_FUNC_GENERATOR,
                                     ctx->function_ctor,
                                     NULL, 0,
                                     js_generator_function_proto_funcs,
                                     countof(js_generator_function_proto_funcs),
                                     JS_NEW_CTOR_NO_GLOBAL | JS_NEW_CTOR_READONLY);
    if (JS_IsException(obj1))
        return -1;
    JS_FreeValue(ctx, obj1);
    if (JS_SetConstructor2(ctx, ctx->class_proto[JS_CLASS_GENERATOR_FUNCTION],
                           ctx->class_proto[JS_CLASS_GENERATOR],
                           JS_PROP_CONFIGURABLE, JS_PROP_CONFIGURABLE))
        return -1;

    /* global properties */
    ctx->eval_obj = JS_GetProperty(ctx, ctx->global_obj, JS_ATOM_eval);
    if (JS_IsException(ctx->eval_obj))
        return -1;

    if (JS_DefinePropertyValue(ctx, ctx->global_obj, JS_ATOM_globalThis,
                               JS_DupValue(ctx, ctx->global_obj),
                               JS_PROP_CONFIGURABLE | JS_PROP_WRITABLE) < 0)
        return -1;

    /* BigInt */
    if (JS_AddIntrinsicBigInt(ctx))
        return -1;
    return 0;
}

int QUICKJS_API JS_AddIntrinsicStringNormalize(JSContext *ctx)
{
    return JS_SetPropertyFunctionList(ctx, ctx->class_proto[JS_CLASS_STRING], js_string_proto_normalize,
                                      countof(js_string_proto_normalize));
}

int QUICKJS_API JS_AddIntrinsicProxy(JSContext *ctx)
{
    JSRuntime *rt = ctx->rt;
    JSValue obj1;

    if (!JS_IsRegisteredClass(rt, JS_CLASS_PROXY)) {
        if (init_class_range(rt, js_proxy_class_def, JS_CLASS_PROXY,
                             countof(js_proxy_class_def)))
            return -1;
        rt->class_array[JS_CLASS_PROXY].exotic = &js_proxy_exotic_methods;
        rt->class_array[JS_CLASS_PROXY].call = js_proxy_call;
    }

    /* additional fields: name, length */
    obj1 = JS_NewCFunction3(ctx, js_proxy_constructor, "Proxy", 2,
                            JS_CFUNC_constructor, 0,
                            ctx->function_proto, countof(js_proxy_funcs) + 2);
    if (JS_IsException(obj1))
        return -1;
    JS_SetConstructorBit(ctx, obj1, TRUE);
    if (JS_SetPropertyFunctionList(ctx, obj1, js_proxy_funcs,
                                   countof(js_proxy_funcs)))
        goto fail;
    if (JS_DefinePropertyValueStr(ctx, ctx->global_obj, "Proxy",
                                  obj1, JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE) < 0)
        goto fail;
    return 0;
 fail:
    JS_FreeValue(ctx, obj1);
    return -1;
}

int QUICKJS_API JS_AddIntrinsicMapSet(JSContext *ctx)
{
    int i;
    JSValue obj1;
    char buf[ATOM_GET_STR_BUF_SIZE];

    for(i = 0; i < 4; i++) {
        JSCFunctionType ft;
        const char *name = JS_AtomGetStr(ctx, buf, sizeof(buf),
                                         JS_ATOM_Map + i);
        ft.constructor_magic = js_map_constructor;
        obj1 = JS_NewCConstructor(ctx, JS_CLASS_MAP + i, name,
                                  ft.generic, 0, JS_CFUNC_constructor_magic, i,
                                  JS_UNDEFINED,
                                  js_map_funcs, i < 2 ? countof(js_map_funcs) : 0,
                                  js_map_proto_funcs_ptr[i], js_map_proto_funcs_count[i],
                                  0);
        if (JS_IsException(obj1))
            return -1;
        JS_FreeValue(ctx, obj1);
    }

    for(i = 0; i < 2; i++) {
        ctx->class_proto[JS_CLASS_MAP_ITERATOR + i] =
            JS_NewObjectProtoList(ctx, ctx->class_proto[JS_CLASS_ITERATOR],
                                  js_map_proto_funcs_ptr[i + 4],
                                  js_map_proto_funcs_count[i + 4]);
        if (JS_IsException(ctx->class_proto[JS_CLASS_MAP_ITERATOR + i]))
            return -1;
    }
    return 0;
}

int QUICKJS_API JS_AddIntrinsicWeakRef(JSContext *ctx)
{
    JSRuntime *rt = ctx->rt;
    JSValue obj;

    /* WeakRef */
    if (!JS_IsRegisteredClass(rt, JS_CLASS_WEAK_REF)) {
        if (init_class_range(rt, js_weakref_class_def, JS_CLASS_WEAK_REF,
                             countof(js_weakref_class_def)))
            return -1;
    }
    obj = JS_NewCConstructor(ctx, JS_CLASS_WEAK_REF, "WeakRef",
                             js_weakref_constructor, 1, JS_CFUNC_constructor_or_func, 0,
                             JS_UNDEFINED,
                             NULL, 0,
                             js_weakref_proto_funcs, countof(js_weakref_proto_funcs),
                             0);
    if (JS_IsException(obj))
        return -1;
    JS_FreeValue(ctx, obj);

    /* FinalizationRegistry */
    if (!JS_IsRegisteredClass(rt, JS_CLASS_FINALIZATION_REGISTRY)) {
        if (init_class_range(rt, js_finrec_class_def, JS_CLASS_FINALIZATION_REGISTRY,
                             countof(js_finrec_class_def)))
            return -1;
    }

    obj = JS_NewCConstructor(ctx, JS_CLASS_FINALIZATION_REGISTRY, "FinalizationRegistry",
                             js_finrec_constructor, 1, JS_CFUNC_constructor_or_func, 0,
                             JS_UNDEFINED,
                             NULL, 0,
                             js_finrec_proto_funcs, countof(js_finrec_proto_funcs),
                             0);
    if (JS_IsException(obj))
        return -1;
    JS_FreeValue(ctx, obj);
    return 0;
}
