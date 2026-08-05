#include "js_internal.h"



#if 0
JSValue js___date_getTimezoneOffset(JSContext *ctx, JSValueConst this_val,
                                           int argc, JSValueConst *argv)
{
    double dd;

    if (JS_ToFloat64(ctx, &dd, argv[0]))
        return JS_EXCEPTION;
    if (isnan(dd))
        return __JS_NewFloat64(ctx, dd);
    else
        return JS_NewInt32(ctx, getTimezoneOffset((int64_t)dd));
}

JSValue js_get_prototype_from_ctor(JSContext *ctx, JSValueConst ctor,
                                          JSValueConst def_proto)
{
    JSValue proto;
    proto = JS_GetProperty(ctx, ctor, JS_ATOM_prototype);
    if (JS_IsException(proto))
        return proto;
    if (!JS_IsObject(proto)) {
        JS_FreeValue(ctx, proto);
        proto = JS_DupValue(ctx, def_proto);
    }
    return proto;
}

/* create a new date object */
JSValue js___date_create(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv)
{
    JSValue obj, proto;
    proto = js_get_prototype_from_ctor(ctx, argv[0], argv[1]);
    if (JS_IsException(proto))
        return proto;
    obj = JS_NewObjectProtoClass(ctx, proto, JS_CLASS_DATE);
    JS_FreeValue(ctx, proto);
    if (!JS_IsException(obj))
        JS_SetObjectData(ctx, obj, JS_DupValue(ctx, argv[2]));
    return obj;
}
#endif

/* RegExp */

void js_regexp_finalizer(JSRuntime *rt, JSValue val)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JSRegExp *re = &p->u.regexp;
    if (re->bytecode != NULL)
        JS_FreeValueRT(rt, JS_MKPTR(JS_TAG_STRING, re->bytecode));
    if (re->pattern != NULL)
        JS_FreeValueRT(rt, JS_MKPTR(JS_TAG_STRING, re->pattern));
}

/* create a string containing the RegExp bytecode */
JSValue js_compile_regexp(JSContext *ctx, JSValueConst pattern,
                                 JSValueConst flags)
{
    const char *str;
    int re_flags, mask;
    uint8_t *re_bytecode_buf;
    size_t i, len;
    int re_bytecode_len;
    JSValue ret;
    char error_msg[64];

    re_flags = 0;
    if (!JS_IsUndefined(flags)) {
        str = JS_ToCStringLen(ctx, &len, flags);
        if (!str)
            return JS_EXCEPTION;
        /* XXX: re_flags = LRE_FLAG_OCTAL unless strict mode? */
        for (i = 0; i < len; i++) {
            switch(str[i]) {
            case 'd':
                mask = LRE_FLAG_INDICES;
                break;
            case 'g':
                mask = LRE_FLAG_GLOBAL;
                break;
            case 'i':
                mask = LRE_FLAG_IGNORECASE;
                break;
            case 'm':
                mask = LRE_FLAG_MULTILINE;
                break;
            case 's':
                mask = LRE_FLAG_DOTALL;
                break;
            case 'u':
                mask = LRE_FLAG_UNICODE;
                break;
            case 'v':
                mask = LRE_FLAG_UNICODE_SETS;
                break;
            case 'y':
                mask = LRE_FLAG_STICKY;
                break;
            default:
                goto bad_flags;
            }
            if ((re_flags & mask) != 0) {
            bad_flags:
                JS_FreeCString(ctx, str);
                goto bad_flags1;
            }
            re_flags |= mask;
        }
        JS_FreeCString(ctx, str);
    }

    /* 'u' and 'v' cannot be both set */
    if ((re_flags & LRE_FLAG_UNICODE_SETS) && (re_flags & LRE_FLAG_UNICODE)) {
    bad_flags1:
        return JS_ThrowSyntaxError(ctx, "invalid regular expression flags");
    }

    str = JS_ToCStringLen2(ctx, &len, pattern, !(re_flags & (LRE_FLAG_UNICODE | LRE_FLAG_UNICODE_SETS)));
    if (!str)
        return JS_EXCEPTION;
    re_bytecode_buf = lre_compile(&re_bytecode_len, error_msg,
                                  sizeof(error_msg), str, len, re_flags, ctx);
    JS_FreeCString(ctx, str);
    if (!re_bytecode_buf) {
        JS_ThrowSyntaxError(ctx, "%s", error_msg);
        return JS_EXCEPTION;
    }

    ret = js_new_string8_len(ctx, (const char *)re_bytecode_buf, re_bytecode_len);
    js_free(ctx, re_bytecode_buf);
    return ret;
}

/* fast regexp creation */
JSValue JS_NewRegexp(JSContext *ctx, JSValue pattern, JSValue bc)
{
    JSValue obj;
    JSProperty props[1];
    JSObject *p;
    JSRegExp *re;

    /* sanity check */
    if (unlikely(JS_VALUE_GET_TAG(bc) != JS_TAG_STRING ||
                 JS_VALUE_GET_TAG(pattern) != JS_TAG_STRING)) {
        JS_ThrowTypeError(ctx, "string expected");
        goto fail;
    }
    props[0].u.value = JS_NewInt32(ctx, 0); /* lastIndex */
    obj = JS_NewObjectFromShape(ctx, js_dup_shape(ctx->regexp_shape), JS_CLASS_REGEXP, props);
    if (JS_IsException(obj))
        goto fail;
    p = JS_VALUE_GET_OBJ(obj);
    re = &p->u.regexp;
    re->pattern = JS_VALUE_GET_STRING(pattern);
    re->bytecode = JS_VALUE_GET_STRING(bc);
    return obj;
 fail:
    JS_FreeValue(ctx, bc);
    JS_FreeValue(ctx, pattern);
    return JS_EXCEPTION;
}

/* set the RegExp fields */
JSValue js_regexp_set_internal(JSContext *ctx,
                                      JSValue obj,
                                      JSValue pattern, JSValue bc)
{
    JSObject *p;
    JSRegExp *re;

    /* sanity check */
    if (unlikely(JS_VALUE_GET_TAG(bc) != JS_TAG_STRING ||
                 JS_VALUE_GET_TAG(pattern) != JS_TAG_STRING)) {
        JS_ThrowTypeError(ctx, "string expected");
        JS_FreeValue(ctx, obj);
        JS_FreeValue(ctx, bc);
        JS_FreeValue(ctx, pattern);
        return JS_EXCEPTION;
    }

    p = JS_VALUE_GET_OBJ(obj);
    re = &p->u.regexp;
    re->pattern = JS_VALUE_GET_STRING(pattern);
    re->bytecode = JS_VALUE_GET_STRING(bc);
    /* Note: cannot fail because the field is preallocated */
    JS_DefinePropertyValue(ctx, obj, JS_ATOM_lastIndex, JS_NewInt32(ctx, 0),
                           JS_PROP_WRITABLE);
    return obj;
}

JSRegExp *js_get_regexp(JSContext *ctx, JSValueConst obj, BOOL throw_error)
{
    if (JS_VALUE_GET_TAG(obj) == JS_TAG_OBJECT) {
        JSObject *p = JS_VALUE_GET_OBJ(obj);
        if (p->class_id == JS_CLASS_REGEXP)
            return &p->u.regexp;
    }
    if (throw_error) {
        JS_ThrowTypeErrorInvalidClass(ctx, JS_CLASS_REGEXP);
    }
    return NULL;
}

/* return < 0 if exception or TRUE/FALSE */
int js_is_regexp(JSContext *ctx, JSValueConst obj)
{
    JSValue m;

    if (!JS_IsObject(obj))
        return FALSE;
    m = JS_GetProperty(ctx, obj, JS_ATOM_Symbol_match);
    if (JS_IsException(m))
        return -1;
    if (!JS_IsUndefined(m))
        return JS_ToBoolFree(ctx, m);
    return js_get_regexp(ctx, obj, FALSE) != NULL;
}

JSValue js_regexp_constructor(JSContext *ctx, JSValueConst new_target,
                                     int argc, JSValueConst *argv)
{
    JSValue pattern, flags, bc, val, obj = JS_UNDEFINED;
    JSValueConst pat, flags1;
    JSRegExp *re;
    int pat_is_regexp;

    pat = argv[0];
    flags1 = argv[1];
    pat_is_regexp = js_is_regexp(ctx, pat);
    if (pat_is_regexp < 0)
        return JS_EXCEPTION;
    if (JS_IsUndefined(new_target)) {
        /* called as a function */
        new_target = JS_GetActiveFunction(ctx);
        if (pat_is_regexp && JS_IsUndefined(flags1)) {
            JSValue ctor;
            BOOL res;
            ctor = JS_GetProperty(ctx, pat, JS_ATOM_constructor);
            if (JS_IsException(ctor))
                return ctor;
            res = js_same_value(ctx, ctor, new_target);
            JS_FreeValue(ctx, ctor);
            if (res)
                return JS_DupValue(ctx, pat);
        }
    }
    re = js_get_regexp(ctx, pat, FALSE);
    flags = JS_UNDEFINED;
    if (re) {
        pattern = JS_DupValue(ctx, JS_MKPTR(JS_TAG_STRING, re->pattern));
        if (JS_IsUndefined(flags1)) {
            bc = JS_DupValue(ctx, JS_MKPTR(JS_TAG_STRING, re->bytecode));
            obj = js_create_from_ctor(ctx, new_target, JS_CLASS_REGEXP);
            if (JS_IsException(obj))
                goto fail;
            goto no_compilation;
        } else {
            flags = JS_DupValue(ctx, flags1);
        }
    } else {
        if (pat_is_regexp) {
            pattern = JS_GetProperty(ctx, pat, JS_ATOM_source);
            if (JS_IsException(pattern))
                goto fail;
            if (JS_IsUndefined(flags1)) {
                flags = JS_GetProperty(ctx, pat, JS_ATOM_flags);
                if (JS_IsException(flags))
                    goto fail;
            } else {
                flags = JS_DupValue(ctx, flags1);
            }
        } else {
            pattern = JS_DupValue(ctx, pat);
            flags = JS_DupValue(ctx, flags1);
        }
        if (JS_IsUndefined(pattern)) {
            pattern = JS_AtomToString(ctx, JS_ATOM_empty_string);
        } else {
            val = pattern;
            pattern = JS_ToString(ctx, val);
            JS_FreeValue(ctx, val);
            if (JS_IsException(pattern))
                goto fail;
        }
    }
    obj = js_create_from_ctor(ctx, new_target, JS_CLASS_REGEXP);
    if (JS_IsException(obj))
        goto fail;
    bc = js_compile_regexp(ctx, pattern, flags);
    if (JS_IsException(bc))
        goto fail;
    JS_FreeValue(ctx, flags);
 no_compilation:
    return js_regexp_set_internal(ctx, obj, pattern, bc);
 fail:
    JS_FreeValue(ctx, pattern);
    JS_FreeValue(ctx, flags);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

JSValue js_regexp_compile(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv)
{
    JSRegExp *re1, *re;
    JSValueConst pattern1, flags1;
    JSValue bc, pattern;

    re = js_get_regexp(ctx, this_val, TRUE);
    if (!re)
        return JS_EXCEPTION;
    pattern1 = argv[0];
    flags1 = argv[1];
    re1 = js_get_regexp(ctx, pattern1, FALSE);
    if (re1) {
        if (!JS_IsUndefined(flags1))
            return JS_ThrowTypeError(ctx, "flags must be undefined");
        pattern = JS_DupValue(ctx, JS_MKPTR(JS_TAG_STRING, re1->pattern));
        bc = JS_DupValue(ctx, JS_MKPTR(JS_TAG_STRING, re1->bytecode));
    } else {
        bc = JS_UNDEFINED;
        if (JS_IsUndefined(pattern1))
            pattern = JS_AtomToString(ctx, JS_ATOM_empty_string);
        else
            pattern = JS_ToString(ctx, pattern1);
        if (JS_IsException(pattern))
            goto fail;
        bc = js_compile_regexp(ctx, pattern, flags1);
        if (JS_IsException(bc))
            goto fail;
    }
    JS_FreeValue(ctx, JS_MKPTR(JS_TAG_STRING, re->pattern));
    JS_FreeValue(ctx, JS_MKPTR(JS_TAG_STRING, re->bytecode));
    re->pattern = JS_VALUE_GET_STRING(pattern);
    re->bytecode = JS_VALUE_GET_STRING(bc);
    if (JS_SetProperty(ctx, this_val, JS_ATOM_lastIndex,
                       JS_NewInt32(ctx, 0)) < 0)
        return JS_EXCEPTION;
    return JS_DupValue(ctx, this_val);
 fail:
    JS_FreeValue(ctx, pattern);
    JS_FreeValue(ctx, bc);
    return JS_EXCEPTION;
}

JSValue js_regexp_get_source(JSContext *ctx, JSValueConst this_val)
{
    JSRegExp *re;
    JSString *p;
    StringBuffer b_s, *b = &b_s;
    int i, n, c, c2, bra;

    if (JS_VALUE_GET_TAG(this_val) != JS_TAG_OBJECT)
        return JS_ThrowTypeErrorNotAnObject(ctx);

    if (js_same_value(ctx, this_val, ctx->class_proto[JS_CLASS_REGEXP]))
        goto empty_regex;

    re = js_get_regexp(ctx, this_val, TRUE);
    if (!re)
        return JS_EXCEPTION;

    p = re->pattern;

    if (p->len == 0) {
    empty_regex:
        return js_new_string8(ctx, "(?:)");
    }
    string_buffer_init2(ctx, b, p->len, p->is_wide_char);

    /* Escape '/' and newline sequences as needed */
    bra = 0;
    for (i = 0, n = p->len; i < n;) {
        c2 = -1;
        switch (c = string_get(p, i++)) {
        case '\\':
            if (i < n)
                c2 = string_get(p, i++);
            break;
        case ']':
            bra = 0;
            break;
        case '[':
            if (!bra) {
                if (i < n && string_get(p, i) == ']')
                    c2 = string_get(p, i++);
                bra = 1;
            }
            break;
        case '\n':
            c = '\\';
            c2 = 'n';
            break;
        case '\r':
            c = '\\';
            c2 = 'r';
            break;
        case '/':
            if (!bra) {
                c = '\\';
                c2 = '/';
            }
            break;
        }
        string_buffer_putc16(b, c);
        if (c2 >= 0)
            string_buffer_putc16(b, c2);
    }
    return string_buffer_end(b);
}

JSValue js_regexp_get_flag(JSContext *ctx, JSValueConst this_val, int mask)
{
    JSRegExp *re;
    int flags;

    if (JS_VALUE_GET_TAG(this_val) != JS_TAG_OBJECT)
        return JS_ThrowTypeErrorNotAnObject(ctx);

    re = js_get_regexp(ctx, this_val, FALSE);
    if (!re) {
        if (js_same_value(ctx, this_val, ctx->class_proto[JS_CLASS_REGEXP]))
            return JS_UNDEFINED;
        else
            return JS_ThrowTypeErrorInvalidClass(ctx, JS_CLASS_REGEXP);
    }

    flags = lre_get_flags(re->bytecode->u.str8);
    return JS_NewBool(ctx, flags & mask);
}

#define RE_FLAG_COUNT 8

JSValue js_regexp_get_flags(JSContext *ctx, JSValueConst this_val)
{
    char str[RE_FLAG_COUNT], *p = str;
    int res, i;
    const int flag_atom[RE_FLAG_COUNT] = {
        JS_ATOM_hasIndices,
        JS_ATOM_global,
        JS_ATOM_ignoreCase,
        JS_ATOM_multiline,
        JS_ATOM_dotAll,
        JS_ATOM_unicode,
        JS_ATOM_unicodeSets,
        JS_ATOM_sticky,
    };
    const char flag_char[RE_FLAG_COUNT] = { 'd', 'g', 'i', 'm', 's', 'u', 'v', 'y' };

    if (JS_VALUE_GET_TAG(this_val) != JS_TAG_OBJECT)
        return JS_ThrowTypeErrorNotAnObject(ctx);

    for(i = 0; i < RE_FLAG_COUNT; i++) {
        res = JS_ToBoolFree(ctx, JS_GetProperty(ctx, this_val, flag_atom[i]));
        if (res < 0)
            goto exception;
        if (res)
            *p++ = flag_char[i];
    }
    return JS_NewStringLen(ctx, str, p - str);

exception:
    return JS_EXCEPTION;
}

JSValue js_regexp_toString(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv)
{
    JSValue pattern, flags;
    StringBuffer b_s, *b = &b_s;

    if (!JS_IsObject(this_val))
        return JS_ThrowTypeErrorNotAnObject(ctx);

    string_buffer_init(ctx, b, 0);
    string_buffer_putc8(b, '/');
    pattern = JS_GetProperty(ctx, this_val, JS_ATOM_source);
    if (string_buffer_concat_value_free(b, pattern))
        goto fail;
    string_buffer_putc8(b, '/');
    flags = JS_GetProperty(ctx, this_val, JS_ATOM_flags);
    if (string_buffer_concat_value_free(b, flags))
        goto fail;
    return string_buffer_end(b);

fail:
    string_buffer_free(b);
    return JS_EXCEPTION;
}

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

void *lre_realloc(void *opaque, void *ptr, size_t size)
{
    JSContext *ctx = opaque;
    /* No JS exception is raised here */
    return js_realloc_rt(ctx->rt, ptr, size);
}

JSValue js_regexp_escape(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv)
{
    JSValue str;
    StringBuffer b_s, *b = &b_s;
    JSString *p;
    uint32_t c;
    char s[16];
    int i, i0;

    if (!JS_IsString(argv[0]))
        return JS_ThrowTypeError(ctx, "not a string");
    str = JS_ToString(ctx, argv[0]); /* must call it to linearlize ropes */
    if (JS_IsException(str))
        return JS_EXCEPTION;
    p = JS_VALUE_GET_STRING(str);
    string_buffer_init2(ctx, b, 0, p->is_wide_char);
    for (i = 0; i < p->len; ) {
        i0 = i;
        c = string_getc(p, &i);
        if (c < 33) {
            if (c >= 9 && c <= 13) {
                string_buffer_putc8(b, '\\');
                string_buffer_putc8(b, "tnvfr"[c - 9]);
            } else {
                goto hex2;
            }
        } else if (c < 128) {
            if ((c >= '0' && c <= '9')
             || (c >= 'A' && c <= 'Z')
             || (c >= 'a' && c <= 'z')) {
                if (i0 == 0)
                    goto hex2;
            } else if (strchr(",-=<>#&!%:;@~'`\"", c)) {
                goto hex2;
            } else if (c != '_') {
                string_buffer_putc8(b, '\\');
            }
            string_buffer_putc8(b, c);
        } else if (c < 256) {
        hex2:
            snprintf(s, sizeof(s), "\\x%02x", c);
            string_buffer_puts8(b, s);
        } else if (is_surrogate(c) || lre_is_space(c)) {
            snprintf(s, sizeof(s), "\\u%04x", c);
            string_buffer_puts8(b, s);
        } else {
            string_buffer_putc(b, c);
        }
    }
    JS_FreeValue(ctx, str);
    return string_buffer_end(b);
}

/* this_val must be of JS_CLASS_REGEXP */
force_inline int js_regexp_get_lastIndex(JSContext *ctx, int64_t *plast_index,
                                                JSValueConst this_val)
{
    JSObject *p = JS_VALUE_GET_OBJ(this_val);

    /* lastIndex is always the first property (it is not configurable) */
    if (likely(JS_VALUE_GET_TAG(p->prop[0].u.value) == JS_TAG_INT)) {
        *plast_index = max_int(JS_VALUE_GET_INT(p->prop[0].u.value), 0);
        return 0;
    } else {
        return JS_ToLengthFree(ctx, plast_index, JS_DupValue(ctx, p->prop[0].u.value));
    }
}

/* this_val must be of JS_CLASS_REGEXP */
force_inline int js_regexp_set_lastIndex(JSContext *ctx, JSValueConst this_val,
                                                int last_index)
{
    JSObject *p = JS_VALUE_GET_OBJ(this_val);

    /* lastIndex is always the first property (it is not configurable) */
    if (likely(JS_VALUE_GET_TAG(p->prop[0].u.value) == JS_TAG_INT &&
               (get_shape_prop(p->shape)->flags & JS_PROP_WRITABLE))) {
        set_value(ctx, &p->prop[0].u.value, JS_NewInt32(ctx, last_index));
    } else {
        if (JS_SetProperty(ctx, this_val, JS_ATOM_lastIndex,
                           JS_NewInt32(ctx, last_index)) < 0)
            return -1;
    }
    return 0;
}

JSValue js_regexp_exec(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv)
{
    JSRegExp *re = js_get_regexp(ctx, this_val, TRUE);
    JSString *str;
    JSValue t, ret, str_val, obj, groups;
    JSValue indices, indices_groups;
    uint8_t *re_bytecode;
    uint8_t **capture, *str_buf;
    int rc, capture_count, shift, i, re_flags, alloc_count;
    int64_t last_index;
    const char *group_name_ptr;
    JSObject *p_obj;
    JSAtom group_name;

    if (!re)
        return JS_EXCEPTION;

    str_val = JS_ToString(ctx, argv[0]);
    if (JS_IsException(str_val))
        return JS_EXCEPTION;

    ret = JS_EXCEPTION;
    obj = JS_NULL;
    groups = JS_UNDEFINED;
    indices = JS_UNDEFINED;
    indices_groups = JS_UNDEFINED;
    capture = NULL;
    group_name = JS_ATOM_NULL;

    if (js_regexp_get_lastIndex(ctx, &last_index, this_val))
        goto fail;

    re_bytecode = re->bytecode->u.str8;
    re_flags = lre_get_flags(re_bytecode);
    if ((re_flags & (LRE_FLAG_GLOBAL | LRE_FLAG_STICKY)) == 0) {
        last_index = 0;
    }
    str = JS_VALUE_GET_STRING(str_val);
    alloc_count = lre_get_alloc_count(re_bytecode);
    if (alloc_count > 0) {
        capture = js_malloc(ctx, sizeof(capture[0]) * alloc_count);
        if (!capture)
            goto fail;
    }
    capture_count = lre_get_capture_count(re_bytecode);
    shift = str->is_wide_char;
    str_buf = str->u.str8;
    if (last_index > str->len) {
        rc = 2;
    } else {
        rc = lre_exec(capture, re_bytecode,
                      str_buf, last_index, str->len,
                      shift, ctx);
    }
    if (rc != 1) {
        if (rc >= 0) {
            if (rc == 2 || (re_flags & (LRE_FLAG_GLOBAL | LRE_FLAG_STICKY))) {
                if (js_regexp_set_lastIndex(ctx, this_val, 0) < 0)
                    goto fail;
            }
        } else {
            if (rc == LRE_RET_TIMEOUT) {
                JS_ThrowInterrupted(ctx);
            } else {
                JS_ThrowInternalError(ctx, "out of memory in regexp execution");
            }
            goto fail;
        }
    } else {
        int prop_flags;
        JSProperty props[4];

        if (re_flags & (LRE_FLAG_GLOBAL | LRE_FLAG_STICKY)) {
            if (js_regexp_set_lastIndex(ctx, this_val,
                                        (capture[1] - str_buf) >> shift) < 0)
                goto fail;
        }
        prop_flags = JS_PROP_C_W_E | JS_PROP_THROW;
        group_name_ptr = lre_get_groupnames(re_bytecode);
        if (group_name_ptr) {
            groups = JS_NewObjectProto(ctx, JS_NULL);
            if (JS_IsException(groups))
                goto fail;
        }
        if (re_flags & LRE_FLAG_INDICES) {
            indices = JS_NewArray(ctx);
            if (JS_IsException(indices))
                goto fail;
            if (group_name_ptr) {
                indices_groups = JS_NewObjectProto(ctx, JS_NULL);
                if (JS_IsException(indices_groups))
                    goto fail;
            }
        }

        props[0].u.value = JS_NewInt32(ctx, capture_count); /* length */
        props[1].u.value = JS_NewInt32(ctx, (capture[0] - str_buf) >> shift); /* index */
        props[2].u.value = str_val; /* input */
        props[3].u.value = JS_DupValue(ctx, groups); /* groups */

        str_val = JS_UNDEFINED;
        obj = JS_NewObjectFromShape(ctx, js_dup_shape(ctx->regexp_result_shape),
                                    JS_CLASS_ARRAY, props);
        if (JS_IsException(obj))
            goto fail;

        p_obj = JS_VALUE_GET_OBJ(obj);
        if (expand_fast_array(ctx, p_obj, capture_count))
            goto fail;

        for(i = 0; i < capture_count; i++) {
            uint8_t **match = &capture[2 * i];
            int start = -1;
            int end = -1;
            JSValue val;

            if (group_name_ptr && i > 0) {
                if (*group_name_ptr) {
                    /* XXX: slow, should create a shape when the regexp is
                       compiled */
                    group_name = JS_NewAtom(ctx, group_name_ptr);
                    if (group_name == JS_ATOM_NULL)
                        goto fail;
                }
                group_name_ptr += strlen(group_name_ptr) + LRE_GROUP_NAME_TRAILER_LEN;
            }

            if (match[0] && match[1]) {
                start = (match[0] - str_buf) >> shift;
                end = (match[1] - str_buf) >> shift;
            }

            if (!JS_IsUndefined(indices)) {
                val = JS_UNDEFINED;
                if (start != -1) {
                    val = JS_NewArray(ctx);
                    if (JS_IsException(val))
                        goto fail;
                    if (JS_DefinePropertyValueUint32(ctx, val, 0,
                                                     JS_NewInt32(ctx, start),
                                                     prop_flags) < 0) {
                        JS_FreeValue(ctx, val);
                        goto fail;
                    }
                    if (JS_DefinePropertyValueUint32(ctx, val, 1,
                                                     JS_NewInt32(ctx, end),
                                                     prop_flags) < 0) {
                        JS_FreeValue(ctx, val);
                        goto fail;
                    }
                }
                if (group_name != JS_ATOM_NULL) {
                    /* JS_HasProperty() cannot fail here */
                    if (!JS_IsUndefined(val) ||
                        ! JS_HasProperty(ctx, indices_groups, group_name)) {
                        if (JS_DefinePropertyValue(ctx, indices_groups,
                                                   group_name, JS_DupValue(ctx, val), prop_flags) < 0) {
                            JS_FreeValue(ctx, val);
                            goto fail;
                        }
                    }
                }
                if (JS_DefinePropertyValueUint32(ctx, indices, i, val,
                                                 prop_flags) < 0) {
                    goto fail;
                }
            }

            val = JS_UNDEFINED;
            if (start != -1) {
                val = js_sub_string(ctx, str, start, end);
                if (JS_IsException(val))
                    goto fail;
            }

            if (group_name != JS_ATOM_NULL) {
                /* JS_HasProperty() cannot fail here */
                if (!JS_IsUndefined(val) ||
                    ! JS_HasProperty(ctx, groups, group_name)) {
                    if (JS_DefinePropertyValue(ctx, groups, group_name,
                                               JS_DupValue(ctx, val),
                                               prop_flags) < 0) {
                        JS_FreeValue(ctx, val);
                        goto fail;
                    }
                }
                JS_FreeAtom(ctx, group_name);
                group_name = JS_ATOM_NULL;
            }
            p_obj->u.array.u.values[p_obj->u.array.count++] = val;
        }

        if (!JS_IsUndefined(indices)) {
            t = indices_groups, indices_groups = JS_UNDEFINED;
            if (JS_DefinePropertyValue(ctx, indices, JS_ATOM_groups,
                                       t, prop_flags) < 0) {
                goto fail;
            }
            t = indices, indices = JS_UNDEFINED;
            if (JS_DefinePropertyValue(ctx, obj, JS_ATOM_indices,
                                       t, prop_flags) < 0) {
                goto fail;
            }
        }
    }
    ret = obj;
    obj = JS_UNDEFINED;
fail:
    JS_FreeAtom(ctx, group_name);
    JS_FreeValue(ctx, indices_groups);
    JS_FreeValue(ctx, indices);
    JS_FreeValue(ctx, str_val);
    JS_FreeValue(ctx, groups);
    JS_FreeValue(ctx, obj);
    js_free(ctx, capture);
    return ret;
}

/* XXX: add group names support */
JSValue js_regexp_replace(JSContext *ctx, JSValueConst this_val, JSValueConst arg,
                                 JSValueConst rep_val)
{
    JSRegExp *re = js_get_regexp(ctx, this_val, TRUE);
    JSString *str;
    JSValue str_val;
    uint8_t *re_bytecode;
    int ret;
    uint8_t **capture, *str_buf;
    int capture_count, alloc_count, shift, re_flags;
    int next_src_pos, start, end;
    int64_t last_index;
    StringBuffer b_s, *b = &b_s;
    JSString *rp = JS_VALUE_GET_STRING(rep_val);
    const char *group_name_ptr;
    BOOL fullUnicode;

    if (!re)
        return JS_EXCEPTION;
    re_bytecode = re->bytecode->u.str8;
    group_name_ptr = lre_get_groupnames(re_bytecode);
    if (group_name_ptr)
        return JS_UNDEFINED; /* group names are not supported yet */

    string_buffer_init(ctx, b, 0);

    capture = NULL;
    str_val = JS_ToString(ctx, arg);
    if (JS_IsException(str_val))
        goto fail;
    str = JS_VALUE_GET_STRING(str_val);
    re_flags = lre_get_flags(re_bytecode);

    if (re_flags & LRE_FLAG_GLOBAL) {
        if (js_regexp_set_lastIndex(ctx, this_val, 0))
            goto fail;
    }
    if ((re_flags & (LRE_FLAG_GLOBAL | LRE_FLAG_STICKY)) == 0) {
        last_index = 0;
    } else {
        if (js_regexp_get_lastIndex(ctx, &last_index, this_val))
            goto fail;
    }
    alloc_count = lre_get_alloc_count(re_bytecode);
    if (alloc_count > 0) {
        capture = js_malloc(ctx, sizeof(capture[0]) * alloc_count);
        if (!capture)
            goto fail;
    }
    capture_count = lre_get_capture_count(re_bytecode);
    fullUnicode = ((re_flags & (LRE_FLAG_UNICODE | LRE_FLAG_UNICODE_SETS)) != 0);
    shift = str->is_wide_char;
    str_buf = str->u.str8;
    next_src_pos = 0;
    for (;;) {
        if (last_index > str->len) {
            ret = 0;
        } else {
            ret = lre_exec(capture, re_bytecode,
                           str_buf, last_index, str->len, shift, ctx);
        }
        if (ret != 1) {
            if (ret >= 0) {
                if (ret == 2 || (re_flags & (LRE_FLAG_GLOBAL | LRE_FLAG_STICKY))) {
                    if (js_regexp_set_lastIndex(ctx, this_val, 0) < 0)
                        goto fail;
                }
            } else {
                if (ret == LRE_RET_TIMEOUT) {
                    JS_ThrowInterrupted(ctx);
                } else {
                    JS_ThrowInternalError(ctx, "out of memory in regexp execution");
                }
                goto fail;
            }
            break;
        }
        start = (capture[0] - str_buf) >> shift;
        end = (capture[1] - str_buf) >> shift;
        last_index = end;
        if (next_src_pos < start) {
            if (string_buffer_concat(b, str, next_src_pos, start))
                goto fail;
        }
        if (rp->len != 0) {
            if (js_string_GetSubstitution(ctx, b, JS_UNDEFINED, str, start,
                                          JS_UNDEFINED, JS_UNDEFINED, rep_val,
                                          capture, capture_count)) {
                goto fail;
            }
        }
        next_src_pos = end;
        if (!(re_flags & LRE_FLAG_GLOBAL)) {
            if (re_flags & LRE_FLAG_STICKY) {
                if (js_regexp_set_lastIndex(ctx, this_val, end) < 0)
                    goto fail;
            }
            break;
        }
        if (end == start) {
            end = string_advance_index(str, end, fullUnicode);
        }
        last_index = end;
    }
    if (string_buffer_concat(b, str, next_src_pos, str->len))
        goto fail;
    JS_FreeValue(ctx, str_val);
    js_free(ctx, capture);
    return string_buffer_end(b);
fail:
    JS_FreeValue(ctx, str_val);
    js_free(ctx, capture);
    string_buffer_free(b);
    return JS_EXCEPTION;
}

JSValue JS_RegExpExec(JSContext *ctx, JSValueConst r, JSValueConst s)
{
    JSValue method, ret;

    method = JS_GetProperty(ctx, r, JS_ATOM_exec);
    if (JS_IsException(method))
        return method;
    if (JS_IsFunction(ctx, method)) {
        ret = JS_CallFree(ctx, method, r, 1, &s);
        if (JS_IsException(ret))
            return ret;
        if (!JS_IsObject(ret) && !JS_IsNull(ret)) {
            JS_FreeValue(ctx, ret);
            return JS_ThrowTypeError(ctx, "RegExp exec method must return an object or null");
        }
        return ret;
    }
    JS_FreeValue(ctx, method);
    return js_regexp_exec(ctx, r, 1, &s);
}

JSValue js_regexp_test(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv)
{
    JSValue val;
    BOOL ret;

    val = JS_RegExpExec(ctx, this_val, argv[0]);
    if (JS_IsException(val))
        return JS_EXCEPTION;
    ret = !JS_IsNull(val);
    JS_FreeValue(ctx, val);
    return JS_NewBool(ctx, ret);
}

JSValue js_regexp_Symbol_match(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv)
{
    // [Symbol.match](str)
    JSValueConst rx = this_val;
    JSValue A, S, flags, result, matchStr;
    int global, n, fullUnicode, isEmpty;
    JSString *p;

    if (!JS_IsObject(rx))
        return JS_ThrowTypeErrorNotAnObject(ctx);

    A = JS_UNDEFINED;
    flags = JS_UNDEFINED;
    result = JS_UNDEFINED;
    matchStr = JS_UNDEFINED;
    S = JS_ToString(ctx, argv[0]);
    if (JS_IsException(S))
        goto exception;

    flags = JS_GetProperty(ctx, rx, JS_ATOM_flags);
    if (JS_IsException(flags))
        goto exception;
    flags = JS_ToStringFree(ctx, flags);
    if (JS_IsException(flags))
        goto exception;
    p = JS_VALUE_GET_STRING(flags);

    global = (-1 != string_indexof_char(p, 'g', 0));
    if (!global) {
        A = JS_RegExpExec(ctx, rx, S);
    } else {
        fullUnicode = (string_indexof_char(p, 'u', 0) >= 0 ||
                       string_indexof_char(p, 'v', 0) >= 0);

        if (JS_SetProperty(ctx, rx, JS_ATOM_lastIndex, JS_NewInt32(ctx, 0)) < 0)
            goto exception;
        A = JS_NewArray(ctx);
        if (JS_IsException(A))
            goto exception;
        n = 0;
        for(;;) {
            JS_FreeValue(ctx, result);
            result = JS_RegExpExec(ctx, rx, S);
            if (JS_IsException(result))
                goto exception;
            if (JS_IsNull(result))
                break;
            matchStr = JS_ToStringFree(ctx, JS_GetPropertyInt64(ctx, result, 0));
            if (JS_IsException(matchStr))
                goto exception;
            isEmpty = JS_IsEmptyString(matchStr);
            if (JS_DefinePropertyValueInt64(ctx, A, n++, matchStr, JS_PROP_C_W_E | JS_PROP_THROW) < 0)
                goto exception;
            if (isEmpty) {
                int64_t thisIndex, nextIndex;
                if (JS_ToLengthFree(ctx, &thisIndex,
                                    JS_GetProperty(ctx, rx, JS_ATOM_lastIndex)) < 0)
                    goto exception;
                p = JS_VALUE_GET_STRING(S);
                nextIndex = string_advance_index(p, thisIndex, fullUnicode);
                if (JS_SetProperty(ctx, rx, JS_ATOM_lastIndex, JS_NewInt64(ctx, nextIndex)) < 0)
                    goto exception;
            }
        }
        if (n == 0) {
            JS_FreeValue(ctx, A);
            A = JS_NULL;
        }
    }
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, flags);
    JS_FreeValue(ctx, S);
    return A;

exception:
    JS_FreeValue(ctx, A);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, flags);
    JS_FreeValue(ctx, S);
    return JS_EXCEPTION;
}

void js_regexp_string_iterator_finalizer(JSRuntime *rt, JSValue val)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JSRegExpStringIteratorData *it = p->u.regexp_string_iterator_data;
    if (it) {
        JS_FreeValueRT(rt, it->iterating_regexp);
        JS_FreeValueRT(rt, it->iterated_string);
        js_free_rt(rt, it);
    }
}

void js_regexp_string_iterator_mark(JSRuntime *rt, JSValueConst val,
                                           JS_MarkFunc *mark_func)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JSRegExpStringIteratorData *it = p->u.regexp_string_iterator_data;
    if (it) {
        JS_MarkValue(rt, it->iterating_regexp, mark_func);
        JS_MarkValue(rt, it->iterated_string, mark_func);
    }
}

JSValue js_regexp_string_iterator_next(JSContext *ctx,
                                              JSValueConst this_val,
                                              int argc, JSValueConst *argv,
                                              BOOL *pdone, int magic)
{
    JSRegExpStringIteratorData *it;
    JSValueConst R, S;
    JSValue matchStr = JS_UNDEFINED, match = JS_UNDEFINED;
    JSString *sp;

    it = JS_GetOpaque2(ctx, this_val, JS_CLASS_REGEXP_STRING_ITERATOR);
    if (!it)
        goto exception;
    if (it->done) {
        *pdone = TRUE;
        return JS_UNDEFINED;
    }
    R = it->iterating_regexp;
    S = it->iterated_string;
    match = JS_RegExpExec(ctx, R, S);
    if (JS_IsException(match))
        goto exception;
    if (JS_IsNull(match)) {
        it->done = TRUE;
        *pdone = TRUE;
        return JS_UNDEFINED;
    } else if (it->global) {
        matchStr = JS_ToStringFree(ctx, JS_GetPropertyInt64(ctx, match, 0));
        if (JS_IsException(matchStr))
            goto exception;
        if (JS_IsEmptyString(matchStr)) {
            int64_t thisIndex, nextIndex;
            if (JS_ToLengthFree(ctx, &thisIndex,
                                JS_GetProperty(ctx, R, JS_ATOM_lastIndex)) < 0)
                goto exception;
            sp = JS_VALUE_GET_STRING(S);
            nextIndex = string_advance_index(sp, thisIndex, it->unicode);
            if (JS_SetProperty(ctx, R, JS_ATOM_lastIndex,
                               JS_NewInt64(ctx, nextIndex)) < 0)
                goto exception;
        }
        JS_FreeValue(ctx, matchStr);
    } else {
        it->done = TRUE;
    }
    *pdone = FALSE;
    return match;
 exception:
    JS_FreeValue(ctx, match);
    JS_FreeValue(ctx, matchStr);
    *pdone = FALSE;
    return JS_EXCEPTION;
}

JSValue js_regexp_Symbol_matchAll(JSContext *ctx, JSValueConst this_val,
                                         int argc, JSValueConst *argv)
{
    // [Symbol.matchAll](str)
    JSValueConst R = this_val;
    JSValue S, C, flags, matcher, iter;
    JSValueConst args[2];
    JSString *strp;
    int64_t lastIndex;
    JSRegExpStringIteratorData *it;

    if (!JS_IsObject(R))
        return JS_ThrowTypeErrorNotAnObject(ctx);

    C = JS_UNDEFINED;
    flags = JS_UNDEFINED;
    matcher = JS_UNDEFINED;
    iter = JS_UNDEFINED;

    S = JS_ToString(ctx, argv[0]);
    if (JS_IsException(S))
        goto exception;
    C = JS_SpeciesConstructor(ctx, R, ctx->regexp_ctor);
    if (JS_IsException(C))
        goto exception;
    flags = JS_ToStringFree(ctx, JS_GetProperty(ctx, R, JS_ATOM_flags));
    if (JS_IsException(flags))
        goto exception;
    args[0] = R;
    args[1] = flags;
    matcher = JS_CallConstructor(ctx, C, 2, args);
    if (JS_IsException(matcher))
        goto exception;
    if (JS_ToLengthFree(ctx, &lastIndex,
                        JS_GetProperty(ctx, R, JS_ATOM_lastIndex)))
        goto exception;
    if (JS_SetProperty(ctx, matcher, JS_ATOM_lastIndex,
                       JS_NewInt64(ctx, lastIndex)) < 0)
        goto exception;

    iter = JS_NewObjectClass(ctx, JS_CLASS_REGEXP_STRING_ITERATOR);
    if (JS_IsException(iter))
        goto exception;
    it = js_malloc(ctx, sizeof(*it));
    if (!it)
        goto exception;
    it->iterating_regexp = matcher;
    it->iterated_string = S;
    strp = JS_VALUE_GET_STRING(flags);
    it->global = string_indexof_char(strp, 'g', 0) >= 0;
    it->unicode = (string_indexof_char(strp, 'u', 0) >= 0 ||
                   string_indexof_char(strp, 'v', 0) >= 0);
    it->done = FALSE;
    JS_SetOpaque(iter, it);

    JS_FreeValue(ctx, C);
    JS_FreeValue(ctx, flags);
    return iter;
 exception:
    JS_FreeValue(ctx, S);
    JS_FreeValue(ctx, C);
    JS_FreeValue(ctx, flags);
    JS_FreeValue(ctx, matcher);
    JS_FreeValue(ctx, iter);
    return JS_EXCEPTION;
}

int value_buffer_init(JSContext *ctx, ValueBuffer *b)
{
    b->ctx = ctx;
    b->len = 0;
    b->size = 4;
    b->error_status = 0;
    b->arr = b->def;
    return 0;
}

void value_buffer_free(ValueBuffer *b)
{
    while (b->len > 0)
        JS_FreeValue(b->ctx, b->arr[--b->len]);
    if (b->arr != b->def)
        js_free(b->ctx, b->arr);
    b->arr = b->def;
    b->size = 4;
}

int value_buffer_append(ValueBuffer *b, JSValue val)
{
    if (b->error_status)
        return -1;

    if (b->len >= b->size) {
        int new_size = (b->len + (b->len >> 1) + 31) & ~16;
        size_t slack;
        JSValue *new_arr;

        if (b->arr == b->def) {
            new_arr = js_realloc2(b->ctx, NULL, sizeof(*b->arr) * new_size, &slack);
            if (new_arr)
                memcpy(new_arr, b->def, sizeof b->def);
        } else {
            new_arr = js_realloc2(b->ctx, b->arr, sizeof(*b->arr) * new_size, &slack);
        }
        if (!new_arr) {
            value_buffer_free(b);
            JS_FreeValue(b->ctx, val);
            b->error_status = -1;
            return -1;
        }
        new_size += slack / sizeof(*new_arr);
        b->arr = new_arr;
        b->size = new_size;
    }
    b->arr[b->len++] = val;
    return 0;
}

/* find in 'p' or its prototypes */
JSShapeProperty *find_property_regexp(JSProperty **ppr,
                                             JSObject *p, JSAtom atom)
{
    JSShapeProperty *prs;

    for(;;) {
        prs = find_own_property(ppr, p, atom);
        if (prs)
            return prs;
        p = p->shape->proto;
        if (!p)
            return NULL;
        if (p->is_exotic)
            return NULL;
    }
}

BOOL check_regexp_getter(JSContext *ctx,
                                JSObject *p, JSAtom atom,
                                JSCFunction *func, int magic)
{
    JSProperty *pr;
    JSShapeProperty *prs;

    prs = find_property_regexp(&pr, p, atom);
    if (!prs)
        return FALSE;
    if ((prs->flags & JS_PROP_TMASK) != JS_PROP_GETSET)
        return FALSE;
    if (!pr->u.getset.getter)
        return FALSE;
    return JS_IsCFunction(ctx, JS_MKPTR(JS_TAG_OBJECT, pr->u.getset.getter),
                          func, magic);
}

BOOL js_is_standard_regexp(JSContext *ctx, JSValueConst obj)
{
    JSObject *p;
    JSProperty *pr;
    JSShapeProperty *prs;
    JSCFunctionType ft;

    if (JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT)
        return FALSE;
    p = JS_VALUE_GET_OBJ(obj);
    if (p->class_id != JS_CLASS_REGEXP)
        return FALSE;
    /* check that the lastIndex is a number (no side effect while getting it) */
    prs = find_own_property(&pr, p, JS_ATOM_lastIndex);
    if (!prs)
        return FALSE;
    if (!JS_IsNumber(pr->u.value))
        return FALSE;

    /* check the 'exec' method. */
    prs = find_property_regexp(&pr, p, JS_ATOM_exec);
    if (!prs)
        return FALSE;
    if ((prs->flags & JS_PROP_TMASK) != JS_PROP_NORMAL)
        return FALSE;
    if (!JS_IsCFunction(ctx, pr->u.value, js_regexp_exec, 0))
        return FALSE;
    /* check the flag getters */
    ft.getter = js_regexp_get_flags;
    if (!check_regexp_getter(ctx, p, JS_ATOM_flags, ft.generic, 0))
        return FALSE;
    ft.getter_magic = js_regexp_get_flag;
    if (!check_regexp_getter(ctx, p, JS_ATOM_global, ft.generic, LRE_FLAG_GLOBAL))
        return FALSE;
    if (!check_regexp_getter(ctx, p, JS_ATOM_unicode, ft.generic, LRE_FLAG_UNICODE))
        return FALSE;
    /* XXX: need to check all accessors, need a faster way.  */
    return TRUE;
}
