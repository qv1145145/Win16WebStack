#include "js_internal.h"
#include "debuglog.h"



int call_setter(JSContext *ctx, JSObject *setter,
                       JSValueConst this_obj, JSValue val, int flags)
{
    JSValue ret, func;
    if (likely(setter)) {
        func = JS_MKPTR(JS_TAG_OBJECT, setter);
        /* Note: the field could be removed in the setter */
        func = JS_DupValue(ctx, func);
        ret = JS_CallFree(ctx, func, this_obj, 1, (JSValueConst *)&val);
        JS_FreeValue(ctx, val);
        if (JS_IsException(ret))
            return -1;
        JS_FreeValue(ctx, ret);
        return TRUE;
    } else {
        JS_FreeValue(ctx, val);
        if ((flags & JS_PROP_THROW) ||
            ((flags & JS_PROP_THROW_STRICT) && is_strict_mode(ctx))) {
            JS_ThrowTypeError(ctx, "no setter for property");
            return -1;
        }
        return FALSE;
    }
}

/* set the array length and remove the array elements if necessary. */
int set_array_length(JSContext *ctx, JSObject *p, JSValue val,
                            int flags)
{
    uint32_t len, idx, cur_len;
    int i, ret;

    /* Note: this call can reallocate the properties of 'p' */
    ret = JS_ToArrayLengthFree(ctx, &len, val, FALSE);
    if (ret)
        return -1;
    /* JS_ToArrayLengthFree() must be done before the read-only test */
    if (unlikely(!(get_shape_prop(p->shape)[0].flags & JS_PROP_WRITABLE)))
        return JS_ThrowTypeErrorReadOnly(ctx, flags, JS_ATOM_length);

    if (likely(p->fast_array)) {
        uint32_t old_len = p->u.array.count;
        if (len < old_len) {
            for(i = len; i < old_len; i++) {
                JS_FreeValue(ctx, p->u.array.u.values[i]);
            }
            p->u.array.count = len;
        }
        p->prop[0].u.value = JS_NewUint32(ctx, len);
    } else {
        /* Note: length is always a uint32 because the object is an
           array */
        JS_ToUint32(ctx, &cur_len, p->prop[0].u.value);
        if (len < cur_len) {
            uint32_t d;
            JSShape *sh;
            JSShapeProperty *pr;

            d = cur_len - len;
            sh = p->shape;
            if (d <= sh->prop_count) {
                JSAtom atom;

                /* faster to iterate */
                while (cur_len > len) {
                    atom = JS_NewAtomUInt32(ctx, cur_len - 1);
                    ret = delete_property(ctx, p, atom);
                    JS_FreeAtom(ctx, atom);
                    if (unlikely(!ret)) {
                        /* unlikely case: property is not
                           configurable */
                        break;
                    }
                    cur_len--;
                }
            } else {
                /* faster to iterate thru all the properties. Need two
                   passes in case one of the property is not
                   configurable */
                cur_len = len;
                for(i = 0, pr = get_shape_prop(sh); i < sh->prop_count;
                    i++, pr++) {
                    if (pr->atom != JS_ATOM_NULL &&
                        JS_AtomIsArrayIndex(ctx, &idx, pr->atom)) {
                        if (idx >= cur_len &&
                            !(pr->flags & JS_PROP_CONFIGURABLE)) {
                            cur_len = idx + 1;
                        }
                    }
                }

                for(i = 0, pr = get_shape_prop(sh); i < sh->prop_count;
                    i++, pr++) {
                    if (pr->atom != JS_ATOM_NULL &&
                        JS_AtomIsArrayIndex(ctx, &idx, pr->atom)) {
                        if (idx >= cur_len) {
                            /* remove the property */
                            delete_property(ctx, p, pr->atom);
                            /* WARNING: the shape may have been modified */
                            sh = p->shape;
                            pr = get_shape_prop(sh) + i;
                        }
                    }
                }
            }
        } else {
            cur_len = len;
        }
        set_value(ctx, &p->prop[0].u.value, JS_NewUint32(ctx, cur_len));
        if (unlikely(cur_len > len)) {
            return JS_ThrowTypeErrorOrFalse(ctx, flags, "not configurable");
        }
    }
    return TRUE;
}

/* return -1 if exception */
int expand_fast_array(JSContext *ctx, JSObject *p, uint32_t new_len)
{
    uint32_t new_size;
    size_t slack;
    JSValue *new_array_prop;
    /* XXX: potential arithmetic overflow */
    new_size = max_int(new_len, p->u.array.u1.size * 3 / 2);
    new_array_prop = js_realloc2(ctx, p->u.array.u.values, sizeof(JSValue) * new_size, &slack);
    if (!new_array_prop)
        return -1;
    new_size += slack / sizeof(*new_array_prop);
    p->u.array.u.values = new_array_prop;
    p->u.array.u1.size = new_size;
    return 0;
}

/* Allocate a new fast array initialized to JS_UNDEFINED. Its maximum
   size is 2^31-1 elements. For convenience, 'len' is a 64 bit
   integer. */
JSValue js_allocate_fast_array(JSContext *ctx, int64_t len)
{
    JSValue arr;
    JSObject *p;
    int i;

    if (len > INT32_MAX)
        return JS_ThrowRangeError(ctx, "invalid array length");
    arr = JS_NewArray(ctx);
    if (JS_IsException(arr))
        return arr;
    if (len > 0) {
        p = JS_VALUE_GET_OBJ(arr);
        if (expand_fast_array(ctx, p, len) < 0) {
            JS_FreeValue(ctx, arr);
            return JS_EXCEPTION;
        }
        p->u.array.count = len;
        for(i = 0; i < len; i++)
            p->u.array.u.values[i] = JS_UNDEFINED;
        /* update the 'length' field */
        set_value(ctx, &p->prop[0].u.value, JS_NewInt32(ctx, len));
    }
    return arr;
}

JSValue js_create_array(JSContext *ctx, int len, JSValueConst *tab)
{
    JSValue obj;
    JSObject *p;
    int i;

    obj = JS_NewArray(ctx);
    if (JS_IsException(obj))
        return JS_EXCEPTION;
    if (len > 0) {
        p = JS_VALUE_GET_OBJ(obj);
        if (expand_fast_array(ctx, p, len) < 0) {
            JS_FreeValue(ctx, obj);
            return JS_EXCEPTION;
        }
        p->u.array.count = len;
        for(i = 0; i < len; i++)
            p->u.array.u.values[i] = JS_DupValue(ctx, tab[i]);
        /* update the 'length' field */
        set_value(ctx, &p->prop[0].u.value, JS_NewInt32(ctx, len));
    }
    return obj;
}

JSValue js_create_array_free(JSContext *ctx, int len, JSValue *tab)
{
    JSValue obj;
    JSObject *p;
    int i;

    obj = JS_NewArray(ctx);
    if (JS_IsException(obj))
        goto fail;
    if (len > 0) {
        p = JS_VALUE_GET_OBJ(obj);
        if (expand_fast_array(ctx, p, len) < 0) {
            JS_FreeValue(ctx, obj);
        fail:
            for(i = 0; i < len; i++)
                JS_FreeValue(ctx, tab[i]);
            return JS_EXCEPTION;
        }
        p->u.array.count = len;
        for(i = 0; i < len; i++)
            p->u.array.u.values[i] = tab[i];
        /* update the 'length' field */
        set_value(ctx, &p->prop[0].u.value, JS_NewInt32(ctx, len));
    }
    return obj;
}

void js_free_desc(JSContext *ctx, JSPropertyDescriptor *desc)
{
    JS_FreeValue(ctx, desc->getter);
    JS_FreeValue(ctx, desc->setter);
    JS_FreeValue(ctx, desc->value);
}

/* return true if an element can be added to a fast array without further tests */
BOOL can_extend_fast_array(JSObject *p)
{
    JSObject *proto;
    if (!p->extensible)
        return FALSE;
    proto = p->shape->proto;
    if (!proto)
        return TRUE;
    return proto->is_std_array_prototype;
}

/* flags can be JS_PROP_THROW or JS_PROP_THROW_STRICT */
int JS_SetPropertyValue(JSContext *ctx, JSValueConst this_obj,
                               JSValue prop, JSValue val, int flags)
{
    if (likely(JS_VALUE_GET_TAG(this_obj) == JS_TAG_OBJECT &&
               JS_VALUE_GET_TAG(prop) == JS_TAG_INT)) {
        JSObject *p;
        uint32_t idx;
        double d;
        int32_t v;

        /* fast path for array access */
        p = JS_VALUE_GET_OBJ(this_obj);
        idx = JS_VALUE_GET_INT(prop);
        switch(p->class_id) {
        case JS_CLASS_ARRAY:
            if (unlikely(idx >= (uint32_t)p->u.array.count)) {
                /* fast path to add an element to the array */
                if (unlikely(idx != (uint32_t)p->u.array.count ||
                             !p->fast_array ||
                             !can_extend_fast_array(p))) {
                    goto slow_path;
                }
                /* add element */
                return add_fast_array_element(ctx, p, val, flags);
            }
            set_value(ctx, &p->u.array.u.values[idx], val);
            break;
        case JS_CLASS_ARGUMENTS:
            if (unlikely(idx >= (uint32_t)p->u.array.count))
                goto slow_path;
            set_value(ctx, &p->u.array.u.values[idx], val);
            break;
        case JS_CLASS_MAPPED_ARGUMENTS:
            if (unlikely(idx >= (uint32_t)p->u.array.count))
                goto slow_path;
            set_value(ctx, p->u.array.u.var_refs[idx]->pvalue, val);
            break;
        case JS_CLASS_UINT8C_ARRAY:
            if (JS_ToUint8ClampFree(ctx, &v, val))
                return -1;
            /* Note: the conversion can detach the typed array, so the
               array bound check must be done after */
            if (unlikely(idx >= (uint32_t)p->u.array.count))
                goto ta_out_of_bound;
            p->u.array.u.uint8_ptr[idx] = v;
            break;
        case JS_CLASS_INT8_ARRAY:
        case JS_CLASS_UINT8_ARRAY:
            if (JS_ToInt32Free(ctx, &v, val))
                return -1;
            if (unlikely(idx >= (uint32_t)p->u.array.count))
                goto ta_out_of_bound;
            p->u.array.u.uint8_ptr[idx] = v;
            break;
        case JS_CLASS_INT16_ARRAY:
        case JS_CLASS_UINT16_ARRAY:
            if (JS_ToInt32Free(ctx, &v, val))
                return -1;
            if (unlikely(idx >= (uint32_t)p->u.array.count))
                goto ta_out_of_bound;
            p->u.array.u.uint16_ptr[idx] = v;
            break;
        case JS_CLASS_INT32_ARRAY:
        case JS_CLASS_UINT32_ARRAY:
            if (JS_ToInt32Free(ctx, &v, val))
                return -1;
            if (unlikely(idx >= (uint32_t)p->u.array.count))
                goto ta_out_of_bound;
            p->u.array.u.uint32_ptr[idx] = v;
            break;
        case JS_CLASS_BIG_INT64_ARRAY:
        case JS_CLASS_BIG_UINT64_ARRAY:
            /* XXX: need specific conversion function */
            {
                int64_t v;
                if (JS_ToBigInt64Free(ctx, &v, val))
                    return -1;
                if (unlikely(idx >= (uint32_t)p->u.array.count))
                    goto ta_out_of_bound;
                p->u.array.u.uint64_ptr[idx] = v;
            }
            break;
        case JS_CLASS_FLOAT16_ARRAY:
            if (JS_ToFloat64Free(ctx, &d, val))
                return -1;
            if (unlikely(idx >= (uint32_t)p->u.array.count))
                goto ta_out_of_bound;
            p->u.array.u.fp16_ptr[idx] = tofp16(d);
            break;
        case JS_CLASS_FLOAT32_ARRAY:
            if (JS_ToFloat64Free(ctx, &d, val))
                return -1;
            if (unlikely(idx >= (uint32_t)p->u.array.count))
                goto ta_out_of_bound;
            p->u.array.u.float_ptr[idx] = d;
            break;
        case JS_CLASS_FLOAT64_ARRAY:
            if (JS_ToFloat64Free(ctx, &d, val))
                return -1;
            if (unlikely(idx >= (uint32_t)p->u.array.count)) {
            ta_out_of_bound:
                return TRUE;
            }
            p->u.array.u.double_ptr[idx] = d;
            break;
        default:
            goto slow_path;
        }
        return TRUE;
    } else {
        JSAtom atom;
        int ret;
    slow_path:
        atom = JS_ValueToAtom(ctx, prop);
        JS_FreeValue(ctx, prop);
        if (unlikely(atom == JS_ATOM_NULL)) {
            JS_FreeValue(ctx, val);
            return -1;
        }
        ret = JS_SetPropertyInternal(ctx, this_obj, atom, val, this_obj, flags);
        JS_FreeAtom(ctx, atom);
        return ret;
    }
}

/* compute the property flags. For each flag: (JS_PROP_HAS_x forces
   it, otherwise def_flags is used)
   Note: makes assumption about the bit pattern of the flags
*/
int get_prop_flags(int flags, int def_flags)
{
    int mask;
    mask = (flags >> JS_PROP_HAS_SHIFT) & JS_PROP_C_W_E;
    return (flags & mask) | (def_flags & ~mask);
}

int JS_CreateProperty(JSContext *ctx, JSObject *p,
                             JSAtom prop, JSValueConst val,
                             JSValueConst getter, JSValueConst setter,
                             int flags)
{
    JSProperty *pr;
    int ret, prop_flags;
    JSVarRef *var_ref;
    JSObject *delete_obj;

    /* 如果本函数正在执行中（tmp_mark == 1），直接返回成功，防止递归 */
    if (p->tmp_mark) {
        return TRUE;
    }
    p->tmp_mark = 1;  /* 加锁 */
	
    JS_LOG("JS_CreateProperty", "Entered, p=%04X:%04X, prop=%u, flags=0x%X, class_id=%d, val=%08lX%08lX",
           FARPTR_SEG(p), FARPTR_OFF(p), (unsigned)prop, flags, p->class_id, U64_HI(val), U64_LO(val));

    /* add a new property or modify an existing exotic one */
    if (p->is_exotic) {
        if (p->class_id == JS_CLASS_ARRAY) {
            uint32_t idx, len;

            if (p->fast_array) {
                if (__JS_AtomIsTaggedInt(prop)) {
                    idx = __JS_AtomToUInt32(prop);
                    if (idx == p->u.array.count) {
                        if (!p->extensible) goto not_extensible;
                        if (flags & (JS_PROP_HAS_GET | JS_PROP_HAS_SET))
                            goto convert_to_array;
                        prop_flags = get_prop_flags(flags, 0);
                        if (prop_flags != JS_PROP_C_W_E)
                            goto convert_to_array;
                        return add_fast_array_element(ctx, p,
                                                      JS_DupValue(ctx, val), flags);
                    } else {
                        goto convert_to_array;
                    }
                } else if (JS_AtomIsArrayIndex(ctx, &idx, prop)) {
                convert_to_array:
                    if (convert_fast_array_to_array(ctx, p))
                        return -1;
                    goto generic_array;
                }
            } else if (JS_AtomIsArrayIndex(ctx, &idx, prop)) {
                JSProperty *plen;
                JSShapeProperty *pslen;
            generic_array:
                plen = &p->prop[0];
                JS_ToUint32(ctx, &len, plen->u.value);
                if ((idx + 1) > len) {
                    pslen = get_shape_prop(p->shape);
                    if (unlikely(!(pslen->flags & JS_PROP_WRITABLE)))
                        return JS_ThrowTypeErrorReadOnly(ctx, flags, JS_ATOM_length);
                    len = idx + 1;
                    set_value(ctx, &plen->u.value, JS_NewUint32(ctx, len));
                }
            }
        } else if (p->class_id >= JS_CLASS_UINT8C_ARRAY &&
                   p->class_id <= JS_CLASS_FLOAT64_ARRAY) {
            ret = JS_AtomIsNumericIndex(ctx, prop);
            if (ret != 0) {
                if (ret < 0) return -1;
                return JS_ThrowTypeErrorOrFalse(ctx, flags, "cannot create numeric index in typed array");
            }
        } else if (!(flags & JS_PROP_NO_EXOTIC)) {
            const JSClassExoticMethods *em = ctx->rt->class_array[p->class_id].exotic;
            if (em) {
                if (em->define_own_property) {
					if (p->tmp_mark){
						JS_LOG("JS_CreateProperty", "Recursive define_own_property prevented");
						return TRUE;  // 防止递归
					}
					p->tmp_mark = 1;
                    int ret = em->define_own_property(ctx, JS_MKPTR(JS_TAG_OBJECT, p),
                                                   prop, val, getter, setter, flags);
					p->tmp_mark = 0;
					return ret;
				}
                ret = JS_IsExtensible(ctx, JS_MKPTR(JS_TAG_OBJECT, p));
                if (ret < 0) return -1;
                if (!ret) goto not_extensible;
            }
        }
    }

    if (!p->extensible) {
    not_extensible:
        return JS_ThrowTypeErrorOrFalse(ctx, flags, "object is not extensible");
    }

    var_ref = NULL;
    delete_obj = NULL;
    if (flags & (JS_PROP_HAS_GET | JS_PROP_HAS_SET)) {
        prop_flags = (flags & (JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE)) |
            JS_PROP_GETSET;
    } else {
        prop_flags = flags & JS_PROP_C_W_E;
        if (p->class_id == JS_CLASS_GLOBAL_OBJECT) {
            JSObject *p1 = JS_VALUE_GET_OBJ(p->u.global_object.uninitialized_vars);
            JSShapeProperty *prs1;
            JSProperty *pr1;
            prs1 = find_own_property(&pr1, p1, prop);
            if (prs1) {
                delete_obj = p1;
                var_ref = pr1->u.var_ref;
                js_rc(var_ref)->ref_count++;
            } else {
                var_ref = js_create_var_ref(ctx, FALSE);
                if (!var_ref) return -1;
            }
            var_ref->is_const = !(prop_flags & JS_PROP_WRITABLE);
            prop_flags |= JS_PROP_VARREF;
        }
    }

    pr = add_property(ctx, p, prop, prop_flags);
    if (unlikely(!pr)) {
        if (var_ref)
            free_var_ref(ctx->rt, var_ref);
        return -1;
    }

    if (flags & (JS_PROP_HAS_GET | JS_PROP_HAS_SET)) {
        pr->u.getset.getter = NULL;
        if ((flags & JS_PROP_HAS_GET) && JS_IsFunction(ctx, getter)) {
            pr->u.getset.getter =
                JS_VALUE_GET_OBJ(JS_DupValue(ctx, getter));
        }
        pr->u.getset.setter = NULL;
        if ((flags & JS_PROP_HAS_SET) && JS_IsFunction(ctx, setter)) {
            pr->u.getset.setter =
                JS_VALUE_GET_OBJ(JS_DupValue(ctx, setter));
        }
    } else if (p->class_id == JS_CLASS_GLOBAL_OBJECT) {
        if (delete_obj)
            delete_property(ctx, delete_obj, prop);
        pr->u.var_ref = var_ref;
        if (flags & JS_PROP_HAS_VALUE) {
            *var_ref->pvalue = JS_DupValue(ctx, val);
        } else {
            *var_ref->pvalue = JS_UNDEFINED;
        }
    } else {
        JS_LOG("JS_CreateProperty", "flags & JS_PROP_HAS_VALUE = %d", (flags & JS_PROP_HAS_VALUE) != 0);
        if (flags & JS_PROP_HAS_VALUE) {
            pr->u.value = JS_DupValue(ctx, val);
        } else {
            pr->u.value = JS_UNDEFINED;
        }
    }
}

/* return FALSE if not OK */
BOOL check_define_prop_flags(int prop_flags, int flags)
{
    BOOL has_accessor, is_getset;

    if (!(prop_flags & JS_PROP_CONFIGURABLE)) {
        if ((flags & (JS_PROP_HAS_CONFIGURABLE | JS_PROP_CONFIGURABLE)) ==
            (JS_PROP_HAS_CONFIGURABLE | JS_PROP_CONFIGURABLE)) {
            return FALSE;
        }
        if ((flags & JS_PROP_HAS_ENUMERABLE) &&
            (flags & JS_PROP_ENUMERABLE) != (prop_flags & JS_PROP_ENUMERABLE))
            return FALSE;
        if (flags & (JS_PROP_HAS_VALUE | JS_PROP_HAS_WRITABLE |
                     JS_PROP_HAS_GET | JS_PROP_HAS_SET)) {
            has_accessor = ((flags & (JS_PROP_HAS_GET | JS_PROP_HAS_SET)) != 0);
            is_getset = ((prop_flags & JS_PROP_TMASK) == JS_PROP_GETSET);
            if (has_accessor != is_getset)
                return FALSE;
            if (!is_getset && !(prop_flags & JS_PROP_WRITABLE)) {
                /* not writable: cannot set the writable bit */
                if ((flags & (JS_PROP_HAS_WRITABLE | JS_PROP_WRITABLE)) ==
                    (JS_PROP_HAS_WRITABLE | JS_PROP_WRITABLE))
                    return FALSE;
            }
        }
    }
    return TRUE;
}

/* ensure that the shape can be safely modified */
int js_shape_prepare_update(JSContext *ctx, JSObject *p,
                                   JSShapeProperty **pprs)
{
    JSShape *sh;
    uint32_t idx = 0;    /* prevent warning */

    sh = p->shape;
    if (sh->is_hashed) {
        if (js_rc(sh)->ref_count != 1) {
            if (pprs)
                idx = *pprs - get_shape_prop(sh);
            /* clone the shape (the resulting one is no longer hashed) */
            sh = js_clone_shape(ctx, sh);
            if (!sh)
                return -1;
            js_free_shape(ctx->rt, p->shape);
            p->shape = sh;
            if (pprs)
                *pprs = get_shape_prop(sh) + idx;
        } else {
            js_shape_hash_unlink(ctx->rt, sh);
            sh->is_hashed = FALSE;
        }
    }
    return 0;
}

int js_update_property_flags(JSContext *ctx, JSObject *p,
                                    JSShapeProperty **pprs, int flags)
{
    if (flags != (*pprs)->flags) {
        if (js_shape_prepare_update(ctx, p, pprs))
            return -1;
        (*pprs)->flags = flags;
    }
    return 0;
}

int JS_DefineAutoInitProperty(JSContext *ctx, JSValueConst this_obj,
                                     JSAtom prop, JSAutoInitIDEnum id,
                                     void *opaque, int flags)
{
    JSObject *p;
    JSProperty *pr;

    if (JS_VALUE_GET_TAG(this_obj) != JS_TAG_OBJECT)
        return FALSE;

    p = JS_VALUE_GET_OBJ(this_obj);

    if (find_own_property(&pr, p, prop)) {
        /* property already exists */
        abort();
        return FALSE;
    }

    /* Specialized CreateProperty */
    pr = add_property(ctx, p, prop, (flags & JS_PROP_C_W_E) | JS_PROP_AUTOINIT);
    if (unlikely(!pr))
        return -1;
    pr->u.init.realm_and_id = (uintptr_t)JS_DupContext(ctx);
    assert((pr->u.init.realm_and_id & 3) == 0);
    assert(id <= 3);
    pr->u.init.realm_and_id |= id;
    pr->u.init.opaque = opaque;
    return TRUE;
}

int JS_DefinePropertyValueValue(JSContext *ctx, JSValueConst this_obj,
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

int JS_DefinePropertyValueInt64(JSContext *ctx, JSValueConst this_obj,
                                int64_t idx, JSValue val, int flags)
{
    return JS_DefinePropertyValueValue(ctx, this_obj, JS_NewInt64(ctx, idx),
                                       val, flags);
}

int JS_CreateDataPropertyUint32(JSContext *ctx, JSValueConst this_obj,
                                       int64_t idx, JSValue val, int flags)
{
    return JS_DefinePropertyValueValue(ctx, this_obj, JS_NewInt64(ctx, idx),
                                       val, flags | JS_PROP_CONFIGURABLE |
                                       JS_PROP_ENUMERABLE | JS_PROP_WRITABLE);
}


/* return TRUE if 'obj' has a non empty 'name' string */
BOOL js_object_has_name(JSContext *ctx, JSValueConst obj)
{
    JSProperty *pr;
    JSShapeProperty *prs;
    JSValueConst val;
    JSString *p;

    prs = find_own_property(&pr, JS_VALUE_GET_OBJ(obj), JS_ATOM_name);
    if (!prs)
        return FALSE;
    if ((prs->flags & JS_PROP_TMASK) != JS_PROP_NORMAL)
        return TRUE;
    val = pr->u.value;
    if (JS_VALUE_GET_TAG(val) != JS_TAG_STRING)
        return TRUE;
    p = JS_VALUE_GET_STRING(val);
    return (p->len != 0);
}

int JS_DefineObjectName(JSContext *ctx, JSValueConst obj,
                               JSAtom name, int flags)
{
    if (name != JS_ATOM_NULL
    &&  JS_IsObject(obj)
    &&  !js_object_has_name(ctx, obj)
    && JS_DefinePropertyValue(ctx, obj, JS_ATOM_name, JS_AtomToString(ctx, name), flags) < 0) {
        return -1;
    }
    return 0;
}

int JS_DefineObjectNameComputed(JSContext *ctx, JSValueConst obj,
                                       JSValueConst str, int flags)
{
    if (JS_IsObject(obj) &&
        !js_object_has_name(ctx, obj)) {
        JSAtom prop;
        JSValue name_str;
        prop = JS_ValueToAtom(ctx, str);
        if (prop == JS_ATOM_NULL)
            return -1;
        name_str = js_get_function_name(ctx, prop);
        JS_FreeAtom(ctx, prop);
        if (JS_IsException(name_str))
            return -1;
        if (JS_DefinePropertyValue(ctx, obj, JS_ATOM_name, name_str, flags) < 0)
            return -1;
    }
    return 0;
}

#define DEFINE_GLOBAL_LEX_VAR (1 << 7)
#define DEFINE_GLOBAL_FUNC_VAR (1 << 6)

JSValue JS_ThrowSyntaxErrorVarRedeclaration(JSContext *ctx, JSAtom prop)
{
    return JS_ThrowSyntaxErrorAtom(ctx, "redeclaration of '%s'", prop);
}

/* flags is 0, DEFINE_GLOBAL_LEX_VAR or DEFINE_GLOBAL_FUNC_VAR */
/* XXX: could support exotic global object. */
int JS_CheckDefineGlobalVar(JSContext *ctx, JSAtom prop, int flags)
{
    JSObject *p;
    JSShapeProperty *prs;

    p = JS_VALUE_GET_OBJ(ctx->global_obj);
    prs = find_own_property1(p, prop);
    /* XXX: should handle JS_PROP_AUTOINIT */
    if (flags & DEFINE_GLOBAL_LEX_VAR) {
        if (prs && !(prs->flags & JS_PROP_CONFIGURABLE))
            goto fail_redeclaration;
    } else {
        if (!prs && !p->extensible)
            goto define_error;
        if (flags & DEFINE_GLOBAL_FUNC_VAR) {
            if (prs) {
                if (!(prs->flags & JS_PROP_CONFIGURABLE) &&
                    ((prs->flags & JS_PROP_TMASK) == JS_PROP_GETSET ||
                     ((prs->flags & (JS_PROP_WRITABLE | JS_PROP_ENUMERABLE)) !=
                      (JS_PROP_WRITABLE | JS_PROP_ENUMERABLE)))) {
                define_error:
                    JS_ThrowTypeErrorAtom(ctx, "cannot define variable '%s'",
                                          prop);
                    return -1;
                }
            }
        }
    }
    /* check if there already is a lexical declaration */
    p = JS_VALUE_GET_OBJ(ctx->global_var_obj);
    prs = find_own_property1(p, prop);
    if (prs) {
    fail_redeclaration:
        JS_ThrowSyntaxErrorVarRedeclaration(ctx, prop);
        return -1;
    }
    return 0;
}

/* construct a reference to a global variable */
int JS_GetGlobalVarRef(JSContext *ctx, JSAtom prop, JSValue *sp)
{
    JSObject *p;
    JSShapeProperty *prs;
    JSProperty *pr;

    /* no exotic behavior is possible in global_var_obj */
    p = JS_VALUE_GET_OBJ(ctx->global_var_obj);
    prs = find_own_property(&pr, p, prop);
    if (prs) {
        /* XXX: conformance: do these tests in
           OP_put_var_ref/OP_get_var_ref ? */
        if (unlikely(JS_IsUninitialized(*pr->u.var_ref->pvalue))) {
            JS_ThrowReferenceErrorUninitialized(ctx, prs->atom);
            return -1;
        }
        if (unlikely(!(prs->flags & JS_PROP_WRITABLE))) {
            return JS_ThrowTypeErrorReadOnly(ctx, JS_PROP_THROW, prop);
        }
        sp[0] = JS_DupValue(ctx, ctx->global_var_obj);
    } else {
        int ret;
        ret = JS_HasProperty(ctx, ctx->global_obj, prop);
        if (ret < 0)
            return -1;
        if (ret) {
            sp[0] = JS_DupValue(ctx, ctx->global_obj);
        } else {
            sp[0] = JS_UNDEFINED;
        }
    }
    sp[1] = JS_AtomToValue(ctx, prop);
    return 0;
}

/* return -1, FALSE or TRUE */
int JS_DeleteGlobalVar(JSContext *ctx, JSAtom prop)
{
    JSObject *p;
    JSShapeProperty *prs;
    JSProperty *pr;
    int ret;

    /* 9.1.1.4.7 DeleteBinding ( N ) */
    p = JS_VALUE_GET_OBJ(ctx->global_var_obj);
    prs = find_own_property(&pr, p, prop);
    if (prs)
        return FALSE; /* lexical variables cannot be deleted */
    ret = JS_HasProperty(ctx, ctx->global_obj, prop);
    if (ret < 0)
        return -1;
    if (ret) {
        return JS_DeleteProperty(ctx, ctx->global_obj, prop, 0);
    } else {
        return TRUE;
    }
}

int JS_DeletePropertyInt64(JSContext *ctx, JSValueConst obj, int64_t idx, int flags)
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

BOOL JS_IsCFunction(JSContext *ctx, JSValueConst val, JSCFunction *func, int magic)
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

JSValue JS_ToPrimitiveFree(JSContext *ctx, JSValue val, int hint)
{
    int i;
    BOOL force_ordinary;

    JSAtom method_name;
    JSValue method, ret;
    if (JS_VALUE_GET_TAG(val) != JS_TAG_OBJECT)
        return val;
    force_ordinary = hint & HINT_FORCE_ORDINARY;
    hint &= ~HINT_FORCE_ORDINARY;
    if (!force_ordinary) {
        method = JS_GetProperty(ctx, val, JS_ATOM_Symbol_toPrimitive);
        if (JS_IsException(method))
            goto exception;
        /* ECMA says *If exoticToPrim is not undefined* but tests in
           test262 use null as a non callable converter */
        if (!JS_IsUndefined(method) && !JS_IsNull(method)) {
            JSAtom atom;
            JSValue arg;
            switch(hint) {
            case HINT_STRING:
                atom = JS_ATOM_string;
                break;
            case HINT_NUMBER:
                atom = JS_ATOM_number;
                break;
            default:
            case HINT_NONE:
                atom = JS_ATOM_default;
                break;
            }
            arg = JS_AtomToString(ctx, atom);
            ret = JS_CallFree(ctx, method, val, 1, (JSValueConst *)&arg);
            JS_FreeValue(ctx, arg);
            if (JS_IsException(ret))
                goto exception;
            JS_FreeValue(ctx, val);
            if (JS_VALUE_GET_TAG(ret) != JS_TAG_OBJECT)
                return ret;
            JS_FreeValue(ctx, ret);
            return JS_ThrowTypeError(ctx, "toPrimitive");
        }
    }
    if (hint != HINT_STRING)
        hint = HINT_NUMBER;
    for(i = 0; i < 2; i++) {
        if ((i ^ hint) == 0) {
            method_name = JS_ATOM_toString;
        } else {
            method_name = JS_ATOM_valueOf;
        }
        method = JS_GetProperty(ctx, val, method_name);
        if (JS_IsException(method))
            goto exception;
        if (JS_IsFunction(ctx, method)) {
            ret = JS_CallFree(ctx, method, val, 0, NULL);
            if (JS_IsException(ret))
                goto exception;
            if (JS_VALUE_GET_TAG(ret) != JS_TAG_OBJECT) {
                JS_FreeValue(ctx, val);
                return ret;
            }
            JS_FreeValue(ctx, ret);
        } else {
            JS_FreeValue(ctx, method);
        }
    }
    JS_ThrowTypeError(ctx, "toPrimitive");
exception:
    JS_FreeValue(ctx, val);
    return JS_EXCEPTION;
}

JSValue JS_ToPrimitive(JSContext *ctx, JSValueConst val, int hint)
{
    return JS_ToPrimitiveFree(ctx, JS_DupValue(ctx, val), hint);
}

int JS_ToBoolFree(JSContext *ctx, JSValue val)
{
    uint32_t tag = JS_VALUE_GET_TAG(val);
    switch(tag) {
    case JS_TAG_INT:
        return JS_VALUE_GET_INT(val) != 0;
    case JS_TAG_BOOL:
    case JS_TAG_NULL:
    case JS_TAG_UNDEFINED:
        return JS_VALUE_GET_INT(val);
    case JS_TAG_EXCEPTION:
        return -1;
    case JS_TAG_STRING:
        {
            BOOL ret = JS_VALUE_GET_STRING(val)->len != 0;
            JS_FreeValue(ctx, val);
            return ret;
        }
    case JS_TAG_STRING_ROPE:
        {
            BOOL ret = JS_VALUE_GET_STRING_ROPE(val)->len != 0;
            JS_FreeValue(ctx, val);
            return ret;
        }
    case JS_TAG_SHORT_BIG_INT:
        return JS_VALUE_GET_SHORT_BIG_INT(val) != 0;
    case JS_TAG_BIG_INT:
        {
            JSBigInt *p = JS_VALUE_GET_PTR(val);
            BOOL ret;
            int i;

            /* fail safe: we assume it is not necessarily
               normalized. Beginning from the MSB ensures that the
               test is fast. */
            ret = FALSE;
            for(i = p->len - 1; i >= 0; i--) {
                if (p->tab[i] != 0) {
                    ret = TRUE;
                    break;
                }
            }
            JS_FreeValue(ctx, val);
            return ret;
        }
    case JS_TAG_OBJECT:
        {
            JSObject *p = JS_VALUE_GET_OBJ(val);
            BOOL ret;
            ret = !p->is_HTMLDDA;
            JS_FreeValue(ctx, val);
            return ret;
        }
        break;
    default:
        if (JS_TAG_IS_FLOAT64(tag)) {
            double d = JS_VALUE_GET_FLOAT64(val);
            return !isnan(d) && d != 0;
        } else {
            JS_FreeValue(ctx, val);
            return TRUE;
        }
    }
}

int skip_spaces(const char *pc)
{
    const uint8_t *p, *p_next, *p_start;
    uint32_t c;

    p = p_start = (const uint8_t *)pc;
    for (;;) {
        c = *p;
        if (c < 128) {
            if (!((c >= 0x09 && c <= 0x0d) || (c == 0x20)))
                break;
            p++;
        } else {
            c = unicode_from_utf8(p, UTF8_CHAR_LEN_MAX, &p_next);
            if (!lre_is_space(c))
                break;
            p = p_next;
        }
    }
    return p - p_start;
}

/* bigint support */

#define JS_BIGINT_MAX_SIZE ((1024 * 1024) / JS_LIMB_BITS) /* in limbs */

/* it is currently assumed that JS_SHORT_BIG_INT_BITS = JS_LIMB_BITS */
#if JS_SHORT_BIG_INT_BITS == 32
#define JS_SHORT_BIG_INT_MIN INT32_MIN
#define JS_SHORT_BIG_INT_MAX INT32_MAX
#elif JS_SHORT_BIG_INT_BITS == 64
#define JS_SHORT_BIG_INT_MIN INT64_MIN
#define JS_SHORT_BIG_INT_MAX INT64_MAX
#else
#error unsupported
#endif

#define ADDC(res, carry_out, op1, op2, carry_in)        \
do {                                                    \
    js_limb_t __v, __a, __k, __k1;                      \
    __v = (op1);                                        \
    __a = __v + (op2);                                  \
    __k1 = __a < __v;                                   \
    __k = (carry_in);                                   \
    __a = __a + __k;                                    \
    carry_out = (__a < __k) | __k1;                     \
    res = __a;                                          \
} while (0)

js_limb_t mp_add(js_limb_t *res, const js_limb_t *op1, const js_limb_t *op2,
                     js_limb_t n, js_limb_t carry)
{
    int i;
    for(i = 0;i < n; i++) {
        ADDC(res[i], carry, op1[i], op2[i], carry);
    }
    return carry;
}

js_limb_t mp_sub(js_limb_t *res, const js_limb_t *op1, const js_limb_t *op2,
                        int n, js_limb_t carry)
{
    int i;
    js_limb_t k, a, v, k1;

    k = carry;
    for(i=0;i<n;i++) {
        v = op1[i];
        a = v - op2[i];
        k1 = a > v;
        v = a - k;
        k = (v > a) | k1;
        res[i] = v;
    }
    return k;
}

/* compute 0 - op2. carry = 0 or 1. */
js_limb_t mp_neg(js_limb_t *res, const js_limb_t *op2, int n)
{
    int i;
    js_limb_t v, carry;

    carry = 1;
    for(i=0;i<n;i++) {
        v = ~op2[i] + carry;
        carry = v < carry;
        res[i] = v;
    }
    return carry;
}

/* tabr[] = taba[] * b + l. Return the high carry */
js_limb_t mp_mul1(js_limb_t *tabr, const js_limb_t *taba, js_limb_t n,
                      js_limb_t b, js_limb_t l)
{
    js_limb_t i;
    js_dlimb_t t;

    for(i = 0; i < n; i++) {
        t = (js_dlimb_t)taba[i] * (js_dlimb_t)b + l;
        tabr[i] = t;
        l = t >> JS_LIMB_BITS;
    }
    return l;
}

js_limb_t mp_div1(js_limb_t *tabr, const js_limb_t *taba, js_limb_t n,
                      js_limb_t b, js_limb_t r)
{
    js_slimb_t i;
    js_dlimb_t a1;
    for(i = n - 1; i >= 0; i--) {
        a1 = ((js_dlimb_t)r << JS_LIMB_BITS) | taba[i];
        tabr[i] = a1 / b;
        r = a1 % b;
    }
    return r;
}

/* tabr[] += taba[] * b, return the high word. */
js_limb_t mp_add_mul1(js_limb_t *tabr, const js_limb_t *taba, js_limb_t n,
                          js_limb_t b)
{
    js_limb_t i, l;
    js_dlimb_t t;

    l = 0;
    for(i = 0; i < n; i++) {
        t = (js_dlimb_t)taba[i] * (js_dlimb_t)b + l + tabr[i];
        tabr[i] = t;
        l = t >> JS_LIMB_BITS;
    }
    return l;
}

/* size of the result : op1_size + op2_size. */
void mp_mul_basecase(js_limb_t *result,
                            const js_limb_t *op1, js_limb_t op1_size,
                            const js_limb_t *op2, js_limb_t op2_size)
{
    int i;
    js_limb_t r;

    result[op1_size] = mp_mul1(result, op1, op1_size, op2[0], 0);
    for(i=1;i<op2_size;i++) {
        r = mp_add_mul1(result + i, op1, op1_size, op2[i]);
        result[i + op1_size] = r;
    }
}

/* tabr[] -= taba[] * b. Return the value to substract to the high
   word. */
js_limb_t mp_sub_mul1(js_limb_t *tabr, const js_limb_t *taba, js_limb_t n,
                          js_limb_t b)
{
    js_limb_t i, l;
    js_dlimb_t t;

    l = 0;
    for(i = 0; i < n; i++) {
        t = tabr[i] - (js_dlimb_t)taba[i] * (js_dlimb_t)b - l;
        tabr[i] = t;
        l = -(t >> JS_LIMB_BITS);
    }
    return l;
}

#define UDIV1NORM_THRESHOLD 3

/* b must be >= 1 << (JS_LIMB_BITS - 1) */
js_limb_t mp_div1norm(js_limb_t *tabr, const js_limb_t *taba, js_limb_t n,
                          js_limb_t b, js_limb_t r)
{
    js_slimb_t i;

    if (n >= UDIV1NORM_THRESHOLD) {
        js_limb_t b_inv;
        b_inv = udiv1norm_init(b);
        for(i = n - 1; i >= 0; i--) {
            tabr[i] = udiv1norm(&r, r, taba[i], b, b_inv);
        }
    } else {
        js_dlimb_t a1;
        for(i = n - 1; i >= 0; i--) {
            a1 = ((js_dlimb_t)r << JS_LIMB_BITS) | taba[i];
            tabr[i] = a1 / b;
            r = a1 % b;
        }
    }
    return r;
}

/* base case division: divides taba[0..na-1] by tabb[0..nb-1]. tabb[nb
   - 1] must be >= 1 << (JS_LIMB_BITS - 1). na - nb must be >= 0. 'taba'
   is modified and contains the remainder (nb limbs). tabq[0..na-nb]
   contains the quotient with tabq[na - nb] <= 1. */
void mp_divnorm(js_limb_t *tabq, js_limb_t *taba, js_limb_t na,
                       const js_limb_t *tabb, js_limb_t nb)
{
    js_limb_t r, a, c, q, v, b1, b1_inv, n, dummy_r;
    int i, j;

    b1 = tabb[nb - 1];
    if (nb == 1) {
        taba[0] = mp_div1norm(tabq, taba, na, b1, 0);
        return;
    }
    n = na - nb;

    if (n >= UDIV1NORM_THRESHOLD)
        b1_inv = udiv1norm_init(b1);
    else
        b1_inv = 0;

    /* first iteration: the quotient is only 0 or 1 */
    q = 1;
    for(j = nb - 1; j >= 0; j--) {
        if (taba[n + j] != tabb[j]) {
            if (taba[n + j] < tabb[j])
                q = 0;
            break;
        }
    }
    tabq[n] = q;
    if (q) {
        mp_sub(taba + n, taba + n, tabb, nb, 0);
    }

    for(i = n - 1; i >= 0; i--) {
        if (unlikely(taba[i + nb] >= b1)) {
            q = -1;
        } else if (b1_inv) {
            q = udiv1norm(&dummy_r, taba[i + nb], taba[i + nb - 1], b1, b1_inv);
        } else {
            js_dlimb_t al;
            al = ((js_dlimb_t)taba[i + nb] << JS_LIMB_BITS) | taba[i + nb - 1];
            q = al / b1;
            r = al % b1;
        }
        r = mp_sub_mul1(taba + i, tabb, nb, q);

        v = taba[i + nb];
        a = v - r;
        c = (a > v);
        taba[i + nb] = a;

        if (c != 0) {
            /* negative result */
            for(;;) {
                q--;
                c = mp_add(taba + i, taba + i, tabb, nb, 0);
                /* propagate carry and test if positive result */
                if (c != 0) {
                    if (++taba[i + nb] == 0) {
                        break;
                    }
                }
            }
        }
        tabq[i] = q;
    }
}

/* 1 <= shift <= JS_LIMB_BITS - 1 */
js_limb_t mp_shl(js_limb_t *tabr, const js_limb_t *taba, int n,
                        int shift)
{
    int i;
    js_limb_t l, v;
    l = 0;
    for(i = 0; i < n; i++) {
        v = taba[i];
        tabr[i] = (v << shift) | l;
        l = v >> (JS_LIMB_BITS - shift);
    }
    return l;
}

/* r = (a + high*B^n) >> shift. Return the remainder r (0 <= r < 2^shift).
   1 <= shift <= LIMB_BITS - 1 */
js_limb_t mp_shr(js_limb_t *tab_r, const js_limb_t *tab, int n,
                        int shift, js_limb_t high)
{
    int i;
    js_limb_t l, a;

    l = high;
    for(i = n - 1; i >= 0; i--) {
        a = tab[i];
        tab_r[i] = (a >> shift) | (l << (JS_LIMB_BITS - shift));
        l = a;
    }
    return l & (((js_limb_t)1 << shift) - 1);
}

JSBigInt *js_bigint_new(JSContext *ctx, int len)
{
    JSBigInt *r;
    if (len > JS_BIGINT_MAX_SIZE) {
        JS_ThrowRangeError(ctx, "BigInt is too large to allocate");
        return NULL;
    }
    r = js_malloc(ctx, sizeof(JSBigInt) + len * sizeof(js_limb_t));
    if (!r)
        return NULL;
    js_rc(r)->ref_count = 1;
    r->len = len;
    return r;
}

JSBigInt *js_bigint_set_si(JSBigIntBuf *buf, js_slimb_t a)
{
    JSBigInt *r = (JSBigInt *)buf->big_int_buf;
    r->len = 1;
    r->tab[0] = a;
    return r;
}

JSBigInt *js_bigint_set_si64(JSBigIntBuf *buf, int64_t a)
{
#if JS_LIMB_BITS == 64
    return js_bigint_set_si(buf, a);
#else
    JSBigInt *r = (JSBigInt *)buf->big_int_buf;
    if (a >= INT32_MIN && a <= INT32_MAX) {
        r->len = 1;
        r->tab[0] = a;
    } else {
        r->len = 2;
        r->tab[0] = a;
        r->tab[1] = a >> JS_LIMB_BITS;
    }
    return r;
#endif
}

/* val must be a short big int */
JSBigInt *js_bigint_set_short(JSBigIntBuf *buf, JSValueConst val)
{
    return js_bigint_set_si(buf, JS_VALUE_GET_SHORT_BIG_INT(val));
}

__maybe_unused void js_bigint_dump1(JSContext *ctx, const char *str,
                                           const js_limb_t *tab, int len)
{
    int i;
    printf("%s: ", str);
    for(i = len - 1; i >= 0; i--) {
#if JS_LIMB_BITS == 32
        printf(" %08x", tab[i]);
#else
        printf(" %016" PRIx64, tab[i]);
#endif
    }
    printf("\n");
}

__maybe_unused void js_bigint_dump(JSContext *ctx, const char *str,
                                          const JSBigInt *p)
{
    js_bigint_dump1(ctx, str, p->tab, p->len);
}

JSBigInt *js_bigint_new_si(JSContext *ctx, js_slimb_t a)
{
    JSBigInt *r;
    r = js_bigint_new(ctx, 1);
    if (!r)
        return NULL;
    r->tab[0] = a;
    return r;
}

JSBigInt *js_bigint_new_si64(JSContext *ctx, int64_t a)
{
#if JS_LIMB_BITS == 64
    return js_bigint_new_si(ctx, a);
#else
    if (a >= INT32_MIN && a <= INT32_MAX) {
        return js_bigint_new_si(ctx, a);
    } else {
        JSBigInt *r;
        r = js_bigint_new(ctx, 2);
        if (!r)
            return NULL;
        r->tab[0] = a;
        r->tab[1] = a >> 32;
        return r;
    }
#endif
}

JSBigInt *js_bigint_new_ui64(JSContext *ctx, uint64_t a)
{
    if (a <= INT64_MAX) {
        return js_bigint_new_si64(ctx, a);
    } else {
        JSBigInt *r;
        r = js_bigint_new(ctx, (65 + JS_LIMB_BITS - 1) / JS_LIMB_BITS);
        if (!r)
            return NULL;
#if JS_LIMB_BITS == 64
        r->tab[0] = a;
        r->tab[1] = 0;
#else
        r->tab[0] = a;
        r->tab[1] = a >> 32;
        r->tab[2] = 0;
#endif
        return r;
    }
}

JSBigInt *js_bigint_new_di(JSContext *ctx, js_sdlimb_t a)
{
    JSBigInt *r;
    if (a == (js_slimb_t)a) {
        r = js_bigint_new(ctx, 1);
        if (!r)
            return NULL;
        r->tab[0] = a;
    } else {
        r = js_bigint_new(ctx, 2);
        if (!r)
            return NULL;
        r->tab[0] = a;
        r->tab[1] = a >> JS_LIMB_BITS;
    }
    return r;
}

/* Remove redundant high order limbs. Warning: 'a' may be
   reallocated. Can never fail.
*/
JSBigInt *js_bigint_normalize1(JSContext *ctx, JSBigInt *a, int l)
{
    js_limb_t v;

    assert(js_rc(a)->ref_count == 1);
    while (l > 1) {
        v = a->tab[l - 1];
        if ((v != 0 && v != -1) ||
            (v & 1) != (a->tab[l - 2] >> (JS_LIMB_BITS - 1))) {
            break;
        }
        l--;
    }
    if (l != a->len) {
        JSBigInt *a1;
        /* realloc to reduce the size */
        a->len = l;
        a1 = js_realloc(ctx, a, sizeof(JSBigInt) + l * sizeof(js_limb_t));
        if (a1)
            a = a1;
    }
    return a;
}

JSBigInt *js_bigint_normalize(JSContext *ctx, JSBigInt *a)
{
    return js_bigint_normalize1(ctx, a, a->len);
}

js_slimb_t js_bigint_get_si_sat(const JSBigInt *a)
{
    if (a->len == 1) {
        return a->tab[0];
    } else {
#if JS_LIMB_BITS == 32
        if (js_bigint_sign(a))
            return INT32_MIN;
        else
            return INT32_MAX;
#else
        if (js_bigint_sign(a))
            return INT64_MIN;
        else
            return INT64_MAX;
#endif
    }
}

/* add the op1 limb */
JSBigInt *js_bigint_extend(JSContext *ctx, JSBigInt *r,
                                  js_limb_t op1)
{
    int n2 = r->len;
    if ((op1 != 0 && op1 != -1) ||
        (op1 & 1) != r->tab[n2 - 1] >> (JS_LIMB_BITS - 1)) {
        JSBigInt *r1;
        r1 = js_realloc(ctx, r,
                        sizeof(JSBigInt) + (n2 + 1) * sizeof(js_limb_t));
        if (!r1) {
            js_free(ctx, r);
            return NULL;
        }
        r = r1;
        r->len = n2 + 1;
        r->tab[n2] = op1;
    } else {
        /* otherwise still need to normalize the result */
        r = js_bigint_normalize(ctx, r);
    }
    return r;
}

/* return NULL in case of error. Compute a + b (b_neg = 0) or a - b
   (b_neg = 1) */
/* XXX: optimize */
JSBigInt *js_bigint_add(JSContext *ctx, const JSBigInt *a,
                               const JSBigInt *b, int b_neg)
{
    JSBigInt *r;
    int n1, n2, i;
    js_limb_t carry, op1, op2, a_sign, b_sign;

    n2 = max_int(a->len, b->len);
    n1 = min_int(a->len, b->len);
    r = js_bigint_new(ctx, n2);
    if (!r)
        return NULL;
    /* XXX: optimize */
    /* common part */
    carry = b_neg;
    for(i = 0; i < n1; i++) {
        op1 = a->tab[i];
        op2 = b->tab[i] ^ (-b_neg);
        ADDC(r->tab[i], carry, op1, op2, carry);
    }
    a_sign = -js_bigint_sign(a);
    b_sign = (-js_bigint_sign(b)) ^ (-b_neg);
    /* part with sign extension of one operand  */
    if (a->len > b->len) {
        for(i = n1; i < n2; i++) {
            op1 = a->tab[i];
            ADDC(r->tab[i], carry, op1, b_sign, carry);
        }
    } else if (a->len < b->len) {
        for(i = n1; i < n2; i++) {
            op2 = b->tab[i] ^ (-b_neg);
            ADDC(r->tab[i], carry, a_sign, op2, carry);
        }
    }

    /* part with sign extension for both operands. Extend the result
       if necessary */
    return js_bigint_extend(ctx, r, a_sign + b_sign + carry);
}

/* XXX: optimize */
JSBigInt *js_bigint_neg(JSContext *ctx, const JSBigInt *a)
{
    JSBigIntBuf buf;
    JSBigInt *b;
    b = js_bigint_set_si(&buf, 0);
    return js_bigint_add(ctx, b, a, 1);
}

JSBigInt *js_bigint_mul(JSContext *ctx, const JSBigInt *a,
                               const JSBigInt *b)
{
    JSBigInt *r;

    r = js_bigint_new(ctx, a->len + b->len);
    if (!r)
        return NULL;
    mp_mul_basecase(r->tab, a->tab, a->len, b->tab, b->len);
    /* correct the result if negative operands (no overflow is
       possible) */
    if (js_bigint_sign(a))
        mp_sub(r->tab + a->len, r->tab + a->len, b->tab, b->len, 0);
    if (js_bigint_sign(b))
        mp_sub(r->tab + b->len, r->tab + b->len, a->tab, a->len, 0);
    return js_bigint_normalize(ctx, r);
}

/* return the division or the remainder. 'b' must be != 0. return NULL
   in case of exception (division by zero or memory error) */
JSBigInt *js_bigint_divrem(JSContext *ctx, const JSBigInt *a,
                                  const JSBigInt *b, BOOL is_rem)
{
    JSBigInt *r, *q;
    js_limb_t *tabb, h;
    int na, nb, a_sign, b_sign, shift;

    if (b->len == 1 && b->tab[0] == 0) {
        JS_ThrowRangeError(ctx, "BigInt division by zero");
        return NULL;
    }

    a_sign = js_bigint_sign(a);
    b_sign = js_bigint_sign(b);
    na = a->len;
    nb = b->len;

    r = js_bigint_new(ctx, na + 2);
    if (!r)
        return NULL;
    if (a_sign) {
        mp_neg(r->tab, a->tab, na);
    } else {
        memcpy(r->tab, a->tab, na * sizeof(a->tab[0]));
    }
    /* normalize */
    while (na > 1 && r->tab[na - 1] == 0)
        na--;

    tabb = js_malloc(ctx, nb * sizeof(tabb[0]));
    if (!tabb) {
        js_free(ctx, r);
        return NULL;
    }
    if (b_sign) {
        mp_neg(tabb, b->tab, nb);
    } else {
        memcpy(tabb, b->tab, nb * sizeof(tabb[0]));
    }
    /* normalize */
    while (nb > 1 && tabb[nb - 1] == 0)
        nb--;

    /* trivial case if 'a' is small */
    if (na < nb) {
        js_free(ctx, r);
        js_free(ctx, tabb);
        if (is_rem) {
            /* r = a */
            r = js_bigint_new(ctx, a->len);
            if (!r)
                return NULL;
            memcpy(r->tab, a->tab, a->len * sizeof(a->tab[0]));
            return r;
        } else {
            /* q = 0 */
            return js_bigint_new_si(ctx, 0);
        }
    }

    /* normalize 'b' */
    shift = js_limb_clz(tabb[nb - 1]);
    if (shift != 0) {
        mp_shl(tabb, tabb, nb, shift);
        h = mp_shl(r->tab, r->tab, na, shift);
        if (h != 0)
            r->tab[na++] = h;
    }

    q = js_bigint_new(ctx, na - nb + 2); /* one more limb for the sign */
    if (!q) {
        js_free(ctx, r);
        js_free(ctx, tabb);
        return NULL;
    }

    //    js_bigint_dump1(ctx, "a", r->tab, na);
    //    js_bigint_dump1(ctx, "b", tabb, nb);
    mp_divnorm(q->tab, r->tab, na, tabb, nb);
    js_free(ctx, tabb);

    if (is_rem) {
        js_free(ctx, q);
        if (shift != 0)
            mp_shr(r->tab, r->tab, nb, shift, 0);
        r->tab[nb++] = 0;
        if (a_sign)
            mp_neg(r->tab, r->tab, nb);
        r = js_bigint_normalize1(ctx, r, nb);
        return r;
    } else {
        js_free(ctx, r);
        q->tab[na - nb + 1] = 0;
        if (a_sign ^ b_sign) {
            mp_neg(q->tab, q->tab, q->len);
        }
        q = js_bigint_normalize(ctx, q);
        return q;
    }
}

/* and, or, xor */
JSBigInt *js_bigint_logic(JSContext *ctx, const JSBigInt *a,
                                 const JSBigInt *b, OPCodeEnum op)
{
    JSBigInt *r;
    js_limb_t b_sign;
    int a_len, b_len, i;

    if (a->len < b->len) {
        const JSBigInt *tmp;
        tmp = a;
        a = b;
        b = tmp;
    }
    /* a_len >= b_len */
    a_len = a->len;
    b_len = b->len;
    b_sign = -js_bigint_sign(b);

    r = js_bigint_new(ctx, a_len);
    if (!r)
        return NULL;
    switch(op) {
    case OP_or:
        for(i = 0; i < b_len; i++) {
            r->tab[i] = a->tab[i] | b->tab[i];
        }
        for(i = b_len; i < a_len; i++) {
            r->tab[i] = a->tab[i] | b_sign;
        }
        break;
    case OP_and:
        for(i = 0; i < b_len; i++) {
            r->tab[i] = a->tab[i] & b->tab[i];
        }
        for(i = b_len; i < a_len; i++) {
            r->tab[i] = a->tab[i] & b_sign;
        }
        break;
    case OP_xor:
        for(i = 0; i < b_len; i++) {
            r->tab[i] = a->tab[i] ^ b->tab[i];
        }
        for(i = b_len; i < a_len; i++) {
            r->tab[i] = a->tab[i] ^ b_sign;
        }
        break;
    default:
        abort();
    }
    return js_bigint_normalize(ctx, r);
}

JSBigInt *js_bigint_not(JSContext *ctx, const JSBigInt *a)
{
    JSBigInt *r;
    int i;

    r = js_bigint_new(ctx, a->len);
    if (!r)
        return NULL;
    for(i = 0; i < a->len; i++) {
        r->tab[i] = ~a->tab[i];
    }
    /* no normalization is needed */
    return r;
}

JSBigInt *js_bigint_shl(JSContext *ctx, const JSBigInt *a,
                               unsigned int shift1)
{
    int d, i, shift;
    JSBigInt *r;
    js_limb_t l;

    if (a->len == 1 && a->tab[0] == 0)
        return js_bigint_new_si(ctx, 0); /* zero case */
    d = shift1 / JS_LIMB_BITS;
    shift = shift1 % JS_LIMB_BITS;
    r = js_bigint_new(ctx, a->len + d);
    if (!r)
        return NULL;
    for(i = 0; i < d; i++)
        r->tab[i] = 0;
    if (shift == 0) {
        for(i = 0; i < a->len; i++) {
            r->tab[i + d] = a->tab[i];
        }
    } else {
        l = mp_shl(r->tab + d, a->tab, a->len, shift);
        if (js_bigint_sign(a))
            l |= (js_limb_t)(-1) << shift;
        r = js_bigint_extend(ctx, r, l);
    }
    return r;
}

JSBigInt *js_bigint_shr(JSContext *ctx, const JSBigInt *a,
                               unsigned int shift1)
{
    int d, i, shift, a_sign, n1;
    JSBigInt *r;

    d = shift1 / JS_LIMB_BITS;
    shift = shift1 % JS_LIMB_BITS;
    a_sign = js_bigint_sign(a);
    if (d >= a->len)
        return js_bigint_new_si(ctx, -a_sign);
    n1 = a->len - d;
    r = js_bigint_new(ctx, n1);
    if (!r)
        return NULL;
    if (shift == 0) {
        for(i = 0; i < n1; i++) {
            r->tab[i] = a->tab[i + d];
        }
        /* no normalization is needed */
    } else {
        mp_shr(r->tab, a->tab + d, n1, shift, -a_sign);
        r = js_bigint_normalize(ctx, r);
    }
    return r;
}

JSBigInt *js_bigint_pow(JSContext *ctx, const JSBigInt *a, JSBigInt *b)
{
    uint32_t e;
    int n_bits, i;
    JSBigInt *r, *r1;

    /* b must be >= 0 */
    if (js_bigint_sign(b)) {
        JS_ThrowRangeError(ctx, "BigInt negative exponent");
        return NULL;
    }
    if (b->len == 1 && b->tab[0] == 0) {
        /* a^0 = 1 */
        return js_bigint_new_si(ctx, 1);
    } else if (a->len == 1) {
        js_limb_t v;
        BOOL is_neg;

        v = a->tab[0];
        if (v <= 1)
            return js_bigint_new_si(ctx, v);
        else if (v == -1)
            return js_bigint_new_si(ctx, 1 - 2 * (b->tab[0] & 1));
        is_neg = (js_slimb_t)v < 0;
        if (is_neg)
            v = -v;
        if ((v & (v - 1)) == 0) {
            uint64_t e1;
            int n;
            /* v = 2^n */
            n = JS_LIMB_BITS - 1 - js_limb_clz(v);
            if (b->len > 1)
                goto overflow;
            if (b->tab[0] > INT32_MAX)
                goto overflow;
            e = b->tab[0];
            e1 = (uint64_t)e * n;
            if (e1 > JS_BIGINT_MAX_SIZE * JS_LIMB_BITS)
                goto overflow;
            e = e1;
            if (is_neg)
                is_neg = b->tab[0] & 1;
            r = js_bigint_new(ctx,
                              (e + JS_LIMB_BITS + 1 - is_neg) / JS_LIMB_BITS);
            if (!r)
                return NULL;
            memset(r->tab, 0, sizeof(r->tab[0]) * r->len);
            r->tab[e / JS_LIMB_BITS] =
                (js_limb_t)(1 - 2 * is_neg) << (e % JS_LIMB_BITS);
            return r;
        }
    }
    if (b->len > 1)
        goto overflow;
    if (b->tab[0] > INT32_MAX)
        goto overflow;
    e = b->tab[0];
    n_bits = 32 - clz32(e);

    r = js_bigint_new(ctx, a->len);
    if (!r)
        return NULL;
    memcpy(r->tab, a->tab, a->len * sizeof(a->tab[0]));
    for(i = n_bits - 2; i >= 0; i--) {
        r1 = js_bigint_mul(ctx, r, r);
        if (!r1)
            return NULL;
        js_free(ctx, r);
        r = r1;
        if ((e >> i) & 1) {
            r1 = js_bigint_mul(ctx, r, a);
            if (!r1)
                return NULL;
            js_free(ctx, r);
            r = r1;
        }
    }
    return r;
 overflow:
    JS_ThrowRangeError(ctx, "BigInt is too large");
    return NULL;
}

/* return (mant, exp) so that abs(a) ~ mant*2^(exp - (limb_bits -
   1). a must be != 0. */
uint64_t js_bigint_get_mant_exp(JSContext *ctx,
                                       int *pexp, const JSBigInt *a)
{
    js_limb_t t[4 - JS_LIMB_BITS / 32], carry, v, low_bits;
    int n1, n2, sgn, shift, i, j, e;
    uint64_t a1, a0;

    n2 = 4 - JS_LIMB_BITS / 32;
    n1 = a->len - n2;
    sgn = js_bigint_sign(a);

    /* low_bits != 0 if there are a non zero low bit in abs(a) */
    low_bits = 0;
    carry = sgn;
    for(i = 0; i < n1; i++) {
        v = (a->tab[i] ^ (-sgn)) + carry;
        carry = v < carry;
        low_bits |= v;
    }
    /* get the n2 high limbs of abs(a) */
    for(j = 0; j < n2; j++) {
        i = j + n1;
        if (i < 0) {
            v = 0;
        } else {
            v = (a->tab[i] ^ (-sgn)) + carry;
            carry = v < carry;
        }
        t[j] = v;
    }

#if JS_LIMB_BITS == 32
    a1 = ((uint64_t)t[2] << 32) | t[1];
    a0 = (uint64_t)t[0] << 32;
#else
    a1 = t[1];
    a0 = t[0];
#endif
    a0 |= (low_bits != 0);
    /* normalize */
    if (a1 == 0) {
        /* JS_LIMB_BITS = 64 bit only */
        shift = 64;
        a1 = a0;
        a0 = 0;
    } else {
        shift = clz64(a1);
        if (shift != 0) {
            a1 = (a1 << shift) | (a0 >> (64 - shift));
            a0 <<= shift;
        }
    }
    a1 |= (a0 != 0); /* keep the bits for the final rounding */
    /* compute the exponent */
    e = a->len * JS_LIMB_BITS - shift - 1;
    *pexp = e;
    return a1;
}

/* shift left with round to nearest, ties to even. n >= 1 */
uint64_t shr_rndn(uint64_t a, int n)
{
    uint64_t addend = ((a >> n) & 1) + ((1 << (n - 1)) - 1);
    return (a + addend) >> n;
}

/* convert to float64 with round to nearest, ties to even. Return
   +/-infinity if too large. */
double js_bigint_to_float64(JSContext *ctx, const JSBigInt *a)
{
    int sgn, e;
    uint64_t mant;

    if (a->len == 1) {
        /* fast case, including zero */
        return (double)(js_slimb_t)a->tab[0];
    }

    sgn = js_bigint_sign(a);
    mant = js_bigint_get_mant_exp(ctx, &e, a);
    if (e > 1023) {
        /* overflow: return infinity */
        mant = 0;
        e = 1024;
    } else {
        mant = (mant >> 1) | (mant & 1); /* avoid overflow in rounding */
        mant = shr_rndn(mant, 10);
        /* rounding can cause an overflow */
        if (mant >= ((uint64_t)1 << 53)) {
            mant >>= 1;
            e++;
        }
        mant &= (((uint64_t)1 << 52) - 1);
    }
    return uint64_as_float64(((uint64_t)sgn << 63) |
                             ((uint64_t)(e + 1023) << 52) |
                             mant);
}

/* return (1, NULL) if not an integer, (2, NULL) if NaN or Infinity,
   (0, n) if an integer, (0, NULL) in case of memory error */
JSBigInt *js_bigint_from_float64(JSContext *ctx, int *pres, double a1)
{
    uint64_t a = float64_as_uint64(a1);
    int sgn, e, shift;
    uint64_t mant;
    JSBigIntBuf buf;
    JSBigInt *r;

    sgn = a >> 63;
    e = (a >> 52) & ((1 << 11) - 1);
    mant = a & (((uint64_t)1 << 52) - 1);
    if (e == 2047) {
        /* NaN, Infinity */
        *pres = 2;
        return NULL;
    }
    if (e == 0 && mant == 0) {
        /* zero */
        *pres = 0;
        return js_bigint_new_si(ctx, 0);
    }
    e -= 1023;
    /* 0 < a < 1 : not an integer */
    if (e < 0)
        goto not_an_integer;
    mant |= (uint64_t)1 << 52;
    if (e < 52) {
        shift = 52 - e;
        /* check that there is no fractional part */
        if (mant & (((uint64_t)1 << shift) - 1)) {
        not_an_integer:
            *pres = 1;
            return NULL;
        }
        mant >>= shift;
        e = 0;
    } else {
        e -= 52;
    }
    if (sgn)
        mant = -mant;
    /* the integer is mant*2^e */
    r = js_bigint_set_si64(&buf, (int64_t)mant);
    *pres = 0;
    return js_bigint_shl(ctx, r, e);
}

/* return -1, 0, 1 or (2) (unordered) */
int js_bigint_float64_cmp(JSContext *ctx, const JSBigInt *a,
                                 double b)
{
    int b_sign, a_sign, e, f;
    uint64_t mant, b1, a_mant;

    b1 = float64_as_uint64(b);
    b_sign = b1 >> 63;
    e = (b1 >> 52) & ((1 << 11) - 1);
    mant = b1 & (((uint64_t)1 << 52) - 1);
    a_sign = js_bigint_sign(a);
    if (e == 2047) {
        if (mant != 0) {
            /* NaN */
            return 2;
        } else {
            /* +/- infinity */
            return 2 * b_sign - 1;
        }
    } else if (e == 0 && mant == 0) {
        /* b = +/-0 */
        if (a->len == 1 && a->tab[0] == 0)
            return 0;
        else
            return 1 - 2 * a_sign;
    } else if (a->len == 1 && a->tab[0] == 0) {
        /* a = 0, b != 0 */
        return 2 * b_sign - 1;
    } else if (a_sign != b_sign) {
        return 1 - 2 * a_sign;
    } else {
        e -= 1023;
        /* Note: handling denormals is not necessary because we
           compare to integers hence f >= 0 */
        /* compute f so that 2^f <= abs(a) < 2^(f+1) */
        a_mant = js_bigint_get_mant_exp(ctx, &f, a);
        if (f != e) {
            if (f < e)
                return -1;
            else
                return 1;
        } else {
            mant = (mant | ((uint64_t)1 << 52)) << 11; /* align to a_mant */
            if (a_mant < mant)
                return 2 * a_sign - 1;
            else if (a_mant > mant)
                return 1 - 2 * a_sign;
            else
                return 0;
        }
    }
}

/* return -1, 0 or 1 */
int js_bigint_cmp(JSContext *ctx, const JSBigInt *a,
                         const JSBigInt *b)
{
    int a_sign, b_sign, res, i;
    a_sign = js_bigint_sign(a);
    b_sign = js_bigint_sign(b);
    if (a_sign != b_sign) {
        res = 1 - 2 * a_sign;
    } else {
        /* we assume the numbers are normalized */
        if (a->len != b->len) {
            if (a->len < b->len)
                res = 2 * a_sign - 1;
            else
                res = 1 - 2 * a_sign;
        } else {
            res = 0;
            for(i = a->len -1; i >= 0; i--) {
                if (a->tab[i] != b->tab[i]) {
                    if (a->tab[i] < b->tab[i])
                        res = -1;
                    else
                        res = 1;
                    break;
                }
            }
        }
    }
    return res;
}

/* syntax: [-]digits in base radix. Return NULL if memory error. radix
   = 10, 2, 8 or 16. */
JSBigInt *js_bigint_from_string(JSContext *ctx,
                                       const char *str, int radix)
{
    const char *p = str;
    size_t n_digits1;
    int is_neg, n_digits, n_limbs, len, log2_radix, n_bits, i;
    JSBigInt *r;
    js_limb_t v, c, h;

    is_neg = 0;
    if (*p == '-') {
        is_neg = 1;
        p++;
    }
    while (*p == '0')
        p++;
    n_digits1 = strlen(p);
    /* the real check for overflox is done js_bigint_new(). Here
       we just avoid integer overflow */
    if (n_digits1 > JS_BIGINT_MAX_SIZE * JS_LIMB_BITS) {
        JS_ThrowRangeError(ctx, "BigInt is too large to allocate");
        return NULL;
    }
    n_digits = n_digits1;
    log2_radix = 32 - clz32(radix - 1); /* ceil(log2(radix)) */
    /* compute the maximum number of limbs */
    if (radix == 10) {
        n_bits = (n_digits * 27 + 7) / 8; /* >= ceil(n_digits * log2(10)) */
    } else {
        n_bits = n_digits * log2_radix;
    }
    /* we add one extra bit for the sign */
    n_limbs = max_int(1, n_bits / JS_LIMB_BITS + 1);
    r = js_bigint_new(ctx, n_limbs);
    if (!r)
        return NULL;
    if (radix == 10) {
        int digits_per_limb = JS_LIMB_DIGITS;
        len = 1;
        r->tab[0] = 0;
        for(;;) {
            /* XXX: slow */
            v = 0;
            for(i = 0; i < digits_per_limb; i++) {
                c = to_digit(*p);
                if (c >= radix)
                    break;
                p++;
                v = v * 10 + c;
            }
            if (i == 0)
                break;
            if (len == 1 && r->tab[0] == 0) {
                r->tab[0] = v;
            } else {
                h = mp_mul1(r->tab, r->tab, len, js_pow_dec[i], v);
                if (h != 0) {
                    r->tab[len++] = h;
                }
            }
        }
        /* add one extra limb to have the correct sign*/
        if ((r->tab[len - 1] >> (JS_LIMB_BITS - 1)) != 0)
            r->tab[len++] = 0;
        r->len = len;
    } else {
        unsigned int bit_pos, shift, pos;

        /* power of two base: no multiplication is needed */
        r->len = n_limbs;
        memset(r->tab, 0, sizeof(r->tab[0]) * n_limbs);
        for(i = 0; i < n_digits; i++) {
            c = to_digit(p[n_digits - 1 - i]);
            assert(c < radix);
            bit_pos = i * log2_radix;
            shift = bit_pos & (JS_LIMB_BITS - 1);
            pos = bit_pos / JS_LIMB_BITS;
            r->tab[pos] |= c << shift;
            /* if log2_radix does not divide JS_LIMB_BITS, needed an
               additional op */
            if (shift + log2_radix > JS_LIMB_BITS) {
                r->tab[pos + 1] |= c >> (JS_LIMB_BITS - shift);
            }
        }
    }
    r = js_bigint_normalize(ctx, r);
    /* XXX: could do it in place */
    if (is_neg) {
        JSBigInt *r1;
        r1 = js_bigint_neg(ctx, r);
        js_free(ctx, r);
        r = r1;
    }
    return r;
}

/* 2 <= base <= 36 */
char const digits[36] = {
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b',
  'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
  'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z'
};

/* special version going backwards */
/* XXX: use dtoa.c */
char *js_u64toa(char *q, int64_t n, unsigned int base)
{
    int digit;
    if (base == 10) {
        /* division by known base uses multiplication */
        do {
            digit = (uint64_t)n % 10;
            n = (uint64_t)n / 10;
            *--q = '0' + digit;
        } while (n != 0);
    } else {
        do {
            digit = (uint64_t)n % base;
            n = (uint64_t)n / base;
            *--q = digits[digit];
        } while (n != 0);
    }
    return q;
}

/* len >= 1. 2 <= radix <= 36 */
char *limb_to_a(char *q, js_limb_t n, unsigned int radix, int len)
{
    int digit, i;

    if (radix == 10) {
        /* specific case with constant divisor */
        /* XXX: optimize */
        for(i = 0; i < len; i++) {
            digit = (js_limb_t)n % 10;
            n = (js_limb_t)n / 10;
            *--q = digit + '0';
        }
    } else {
        for(i = 0; i < len; i++) {
            digit = (js_limb_t)n % radix;
            n = (js_limb_t)n / radix;
            *--q = digits[digit];
        }
    }
    return q;
}

#define JS_RADIX_MAX 36

JSValue js_bigint_to_string1(JSContext *ctx, JSValueConst val, int radix)
{
    if (JS_VALUE_GET_TAG(val) == JS_TAG_SHORT_BIG_INT) {
        char buf[66];
        int len;
        len = i64toa_radix(buf, JS_VALUE_GET_SHORT_BIG_INT(val), radix);
        return js_new_string8_len(ctx, buf, len);
    } else {
        JSBigInt *r, *tmp = NULL;
        char *buf, *q, *buf_end;
        int is_neg, n_bits, log2_radix, n_digits;
        BOOL is_binary_radix;
        JSValue res;

        assert(JS_VALUE_GET_TAG(val) == JS_TAG_BIG_INT);
        r = JS_VALUE_GET_PTR(val);
        if (r->len == 1 && r->tab[0] == 0) {
            /* '0' case */
            return js_new_string8_len(ctx, "0", 1);
        }
        is_binary_radix = ((radix & (radix - 1)) == 0);
        is_neg = js_bigint_sign(r);
        if (is_neg) {
            tmp = js_bigint_neg(ctx, r);
            if (!tmp)
                return JS_EXCEPTION;
            r = tmp;
        } else if (!is_binary_radix) {
            /* need to modify 'r' */
            tmp = js_bigint_new(ctx, r->len);
            if (!tmp)
                return JS_EXCEPTION;
            memcpy(tmp->tab, r->tab, r->len * sizeof(r->tab[0]));
            r = tmp;
        }
        log2_radix = 31 - clz32(radix); /* floor(log2(radix)) */
        n_bits = r->len * JS_LIMB_BITS - js_limb_safe_clz(r->tab[r->len - 1]);
        /* n_digits is exact only if radix is a power of
           two. Otherwise it is >= the exact number of digits */
        n_digits = (n_bits + log2_radix - 1) / log2_radix;
        /* XXX: could directly build the JSString */
        buf = js_malloc(ctx, n_digits + is_neg + 1);
        if (!buf) {
            js_free(ctx, tmp);
            return JS_EXCEPTION;
        }
        q = buf + n_digits + is_neg + 1;
        *--q = '\0';
        buf_end = q;
        if (!is_binary_radix) {
            int len;
            js_limb_t radix_base, v;
            radix_base = radix_base_table[radix - 2];
            len = r->len;
            for(;;) {
                /* remove leading zero limbs */
                while (len > 1 && r->tab[len - 1] == 0)
                    len--;
                if (len == 1 && r->tab[0] < radix_base) {
                    v = r->tab[0];
                    if (v != 0) {
                        q = js_u64toa(q, v, radix);
                    }
                    break;
                } else {
                    v = mp_div1(r->tab, r->tab, len, radix_base, 0);
                    q = limb_to_a(q, v, radix, digits_per_limb_table[radix - 2]);
                }
            }
        } else {
            int i, shift;
            unsigned int bit_pos, pos, c;

            /* radix is a power of two */
            for(i = 0; i < n_digits; i++) {
                bit_pos = i * log2_radix;
                pos = bit_pos / JS_LIMB_BITS;
                shift = bit_pos % JS_LIMB_BITS;
                c = r->tab[pos] >> shift;
                if ((shift + log2_radix) > JS_LIMB_BITS &&
                    (pos + 1) < r->len) {
                    c |= r->tab[pos + 1] << (JS_LIMB_BITS - shift);
                }
                c &= (radix - 1);
                *--q = digits[c];
            }
        }
        if (is_neg)
            *--q = '-';
        js_free(ctx, tmp);
        res = js_new_string8_len(ctx, q, buf_end - q);
        js_free(ctx, buf);
        return res;
    }
}
