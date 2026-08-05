#include "js_internal.h"



JSValue js_regexp_Symbol_split(JSContext *ctx, JSValueConst this_val,
                                       int argc, JSValueConst *argv)
{
    // [Symbol.split](str, limit)
    JSValueConst rx = this_val;
    JSValueConst args[2];
    JSValue str, ctor, splitter, A, flags, z, sub;
    JSString *strp;
    uint32_t lim, size, p, q;
    int unicodeMatching;
    int64_t lengthA, e, numberOfCaptures, i;

    if (!JS_IsObject(rx))
        return JS_ThrowTypeErrorNotAnObject(ctx);

    ctor = JS_UNDEFINED;
    splitter = JS_UNDEFINED;
    A = JS_UNDEFINED;
    flags = JS_UNDEFINED;
    z = JS_UNDEFINED;
    str = JS_ToString(ctx, argv[0]);
    if (JS_IsException(str))
        goto exception;
    ctor = JS_SpeciesConstructor(ctx, rx, ctx->regexp_ctor);
    if (JS_IsException(ctor))
        goto exception;
    flags = JS_ToStringFree(ctx, JS_GetProperty(ctx, rx, JS_ATOM_flags));
    if (JS_IsException(flags))
        goto exception;
    strp = JS_VALUE_GET_STRING(flags);
    unicodeMatching = (string_indexof_char(strp, 'u', 0) >= 0 ||
                       string_indexof_char(strp, 'v', 0) >= 0);
    if (string_indexof_char(strp, 'y', 0) < 0) {
        flags = JS_ConcatString3(ctx, "", flags, "y");
        if (JS_IsException(flags))
            goto exception;
    }
    args[0] = rx;
    args[1] = flags;
    splitter = JS_CallConstructor(ctx, ctor, 2, args);
    if (JS_IsException(splitter))
        goto exception;
    A = JS_NewArray(ctx);
    if (JS_IsException(A))
        goto exception;
    lengthA = 0;
    if (JS_IsUndefined(argv[1])) {
        lim = 0xffffffff;
    } else {
        if (JS_ToUint32(ctx, &lim, argv[1]) < 0)
            goto exception;
        if (lim == 0)
            goto done;
    }
    strp = JS_VALUE_GET_STRING(str);
    p = q = 0;
    size = strp->len;
    if (size == 0) {
        z = JS_RegExpExec(ctx, splitter, str);
        if (JS_IsException(z))
            goto exception;
        if (JS_IsNull(z))
            goto add_tail;
        goto done;
    }
    while (q < size) {
        if (JS_SetProperty(ctx, splitter, JS_ATOM_lastIndex, JS_NewInt32(ctx, q)) < 0)
            goto exception;
        JS_FreeValue(ctx, z);
        z = JS_RegExpExec(ctx, splitter, str);
        if (JS_IsException(z))
            goto exception;
        if (JS_IsNull(z)) {
            q = string_advance_index(strp, q, unicodeMatching);
        } else {
            if (JS_ToLengthFree(ctx, &e, JS_GetProperty(ctx, splitter, JS_ATOM_lastIndex)))
                goto exception;
            if (e > size)
                e = size;
            if (e == p) {
                q = string_advance_index(strp, q, unicodeMatching);
            } else {
                sub = js_sub_string(ctx, strp, p, q);
                if (JS_IsException(sub))
                    goto exception;
                if (JS_DefinePropertyValueInt64(ctx, A, lengthA++, sub,
                                                JS_PROP_C_W_E | JS_PROP_THROW) < 0)
                    goto exception;
                if (lengthA == lim)
                    goto done;
                p = e;
                if (js_get_length64(ctx, &numberOfCaptures, z))
                    goto exception;
                for(i = 1; i < numberOfCaptures; i++) {
                    sub = JS_GetPropertyInt64(ctx, z, i);
                    if (JS_IsException(sub))
                        goto exception;
                    if (JS_DefinePropertyValueInt64(ctx, A, lengthA++, sub, JS_PROP_C_W_E | JS_PROP_THROW) < 0)
                        goto exception;
                    if (lengthA == lim)
                        goto done;
                }
                q = p;
            }
        }
    }
add_tail:
    if (p > size)
        p = size;
    sub = js_sub_string(ctx, strp, p, size);
    if (JS_IsException(sub))
        goto exception;
    if (JS_DefinePropertyValueInt64(ctx, A, lengthA++, sub, JS_PROP_C_W_E | JS_PROP_THROW) < 0)
        goto exception;
    goto done;
exception:
    JS_FreeValue(ctx, A);
    A = JS_EXCEPTION;
done:
    JS_FreeValue(ctx, str);
    JS_FreeValue(ctx, ctor);
    JS_FreeValue(ctx, splitter);
    JS_FreeValue(ctx, flags);
    JS_FreeValue(ctx, z);
    return A;
}

/* JSON */

int json_parse_expect(JSParseState *s, int tok)
{
    if (s->token.val != tok) {
        /* XXX: dump token correctly in all cases */
        return js_parse_error(s, "expecting '%c'", tok);
    }
    return json_next_token(s);
}


void json_parse_record_init_obj(JSContext *ctx, JSONParseRecord *pr, JSValueConst val)
{
    pr->value = JS_DupValue(ctx, val);
    pr->u.obj.count = 0;
    pr->u.obj.entries = NULL;
    pr->u.obj.hash_table = NULL;
    pr->u.obj.hash_size = 0;
}

void json_parse_record_init_array(JSContext *ctx, JSONParseRecord *pr, JSValueConst val)
{
    pr->value = JS_DupValue(ctx, val);
    pr->u.array.count = 0;
    pr->u.array.elements = NULL;
}

void json_parse_record_init_primitive(JSContext *ctx, JSONParseRecord *pr, JSValueConst val,
                                             uint32_t source_pos, uint32_t source_len)
{
    pr->value = JS_DupValue(ctx, val);
    pr->u.primitive.source_pos = source_pos;
    pr->u.primitive.source_len = source_len;
}

int json_parse_record_resize_hash(JSContext *ctx, JSONParseRecordObject *po, uint32_t new_hash_size)
{
    uint32_t i, h, *new_hash_table;
    JSONParseRecordEntry *e;

    new_hash_table = js_malloc(ctx, sizeof(new_hash_table[0]) * new_hash_size);
    if (!new_hash_table)
        return -1;
    js_free(ctx, po->hash_table);
    po->hash_table = new_hash_table;
    po->hash_size = new_hash_size;

    for(i = 0; i < po->hash_size; i++) {
        po->hash_table[i] = -1;
    }
    for(i = 0; i < po->count; i++) {
        e = &po->entries[i];
        h = e->atom & (po->hash_size - 1);
        e->hash_next = po->hash_table[h];
        po->hash_table[h] = i;
    }
    return 0;
}

JSONParseRecord *json_parse_record_add(JSContext *ctx, JSONParseRecord *pr, JSAtom key, int *psize)
{
    JSONParseRecordObject *po = &pr->u.obj;
    JSONParseRecordEntry *e;
    JSONParseRecord *pr1;
    uint32_t h;

    if (js_resize_array(ctx, (void **)&po->entries, sizeof(po->entries[0]),
                        psize, po->count + 1)) {
        return NULL;
    }
    /* don't use a hash table when the number of entries is small */
    if (po->count >= 8 && (po->count + 1) > po->hash_size) {
        int hash_bits = 32 - clz32(po->count);
        if (json_parse_record_resize_hash(ctx, po, 1 << hash_bits))
            return NULL;
    }

    e = &po->entries[po->count++];
    e->atom = JS_DupAtom(ctx, key);
    pr1 = &e->parse_record;
    pr1->value = JS_UNDEFINED;
    if (po->hash_size != 0) {
        h = key & (po->hash_size - 1);
        e->hash_next = po->hash_table[h];
        po->hash_table[h] = po->count - 1;
    }
    return pr1;
}

JSONParseRecord *json_parse_record_find(JSONParseRecord *pr, JSAtom key)
{
    JSONParseRecordObject *po = &pr->u.obj;
    JSONParseRecordEntry *e;
    uint32_t h, i;

    if (po->hash_size == 0) {
        for(i = 0; i < po->count; i++) {
            if (po->entries[i].atom == key)
                return &po->entries[i].parse_record;
        }
    } else {
        h = key & (po->hash_size - 1);
        i = po->hash_table[h];
        while (i != -1) {
            e = &po->entries[i];
            if (e->atom == key)
                return &e->parse_record;
            i = e->hash_next;
        }
    }
    return NULL;
}

void json_free_parse_record(JSContext *ctx, JSONParseRecord *pr)
{
    int i;
    if (!pr)
        return;
    if (JS_IsObject(pr->value)) {
        if (JS_IsArray(ctx, pr->value)) {
            for(i = 0; i < pr->u.array.count; i++) {
                json_free_parse_record(ctx, &pr->u.array.elements[i]);
            }
            js_free(ctx, pr->u.array.elements);
        } else {
            for(i = 0; i < pr->u.obj.count; i++) {
                JS_FreeAtom(ctx, pr->u.obj.entries[i].atom);
                json_free_parse_record(ctx, &pr->u.obj.entries[i].parse_record);
            }
            js_free(ctx, pr->u.obj.entries);
            js_free(ctx, pr->u.obj.hash_table);
        }
    }
    JS_FreeValue(ctx, pr->value);
    pr->value = JS_UNDEFINED; /* fail safe */
}

/* 'pr' can be NULL */
JSValue json_parse_value(JSParseState *s, JSONParseRecord *pr)
{
    JSContext *ctx = s->ctx;
    JSValue val = JS_NULL;
    int ret;

    if (pr) {
        pr->value = JS_UNDEFINED;
    }

    switch(s->token.val) {
    case '{':
        {
            JSValue prop_val;
            JSAtom prop_name;
            JSONParseRecord *pr1;
            int pr_size;

            if (json_next_token(s))
                goto fail;
            val = JS_NewObject(ctx);
            if (JS_IsException(val))
                goto fail;
            if (pr) {
                json_parse_record_init_obj(ctx, pr, val);
                pr_size = 0;
            }
            if (s->token.val != '}') {
                for(;;) {
                    if (s->token.val == TOK_STRING) {
                        prop_name = JS_ValueToAtom(ctx, s->token.u.str.str);
                        if (prop_name == JS_ATOM_NULL)
                            goto fail;
                    } else if (s->ext_json && s->token.val == TOK_IDENT) {
                        prop_name = JS_DupAtom(ctx, s->token.u.ident.atom);
                    } else {
                        js_parse_error(s, "expecting property name");
                        goto fail;
                    }
                    if (json_next_token(s))
                        goto fail1;
                    if (json_parse_expect(s, ':'))
                        goto fail1;
                    if (pr) {
                        pr1 = json_parse_record_add(ctx, pr, prop_name, &pr_size);
                        if (!pr1)
                            goto fail1;
                    } else {
                        pr1 = NULL;
                    }
                    prop_val = json_parse_value(s, pr1);
                    if (JS_IsException(prop_val)) {
                    fail1:
                        JS_FreeAtom(ctx, prop_name);
                        goto fail;
                    }
                    ret = JS_DefinePropertyValue(ctx, val, prop_name,
                                                 prop_val, JS_PROP_C_W_E);
                    JS_FreeAtom(ctx, prop_name);
                    if (ret < 0)
                        goto fail;

                    if (s->token.val != ',')
                        break;
                    if (json_next_token(s))
                        goto fail;
                    if (s->ext_json && s->token.val == '}')
                        break;
                }
            }
            if (json_parse_expect(s, '}'))
                goto fail;
        }
        break;
    case '[':
        {
            JSValue el;
            uint32_t idx;
            JSONParseRecord *pr1;
            int pr_size;

            if (json_next_token(s))
                goto fail;
            val = JS_NewArray(ctx);
            if (JS_IsException(val))
                goto fail;
            if (pr) {
                json_parse_record_init_array(ctx, pr, val);
                pr_size = 0;
            }
            if (s->token.val != ']') {
                idx = 0;
                for(;;) {
                    if (pr) {
                        if (js_resize_array(ctx, (void **)&pr->u.array.elements, sizeof(pr->u.array.elements[0]),
                                            &pr_size, pr->u.array.count + 1))
                            goto fail;
                        pr1 = &pr->u.array.elements[pr->u.array.count++];
                        pr1->value = JS_UNDEFINED;
                    } else {
                        pr1 = NULL;
                    }
                    el = json_parse_value(s, pr1);
                    if (JS_IsException(el))
                        goto fail;
                    ret = JS_DefinePropertyValueUint32(ctx, val, idx, el, JS_PROP_C_W_E);
                    if (ret < 0)
                        goto fail;
                    if (s->token.val != ',')
                        break;
                    if (json_next_token(s))
                        goto fail;
                    idx++;
                    if (s->ext_json && s->token.val == ']')
                        break;
                }
            }
            if (json_parse_expect(s, ']'))
                goto fail;
        }
        break;
    case TOK_STRING:
        val = JS_DupValue(ctx, s->token.u.str.str);
        if (pr) {
            json_parse_record_init_primitive(ctx, pr, val,
                                             s->token.ptr - s->buf_start,
                                             s->buf_ptr - s->token.ptr);
        }
        if (json_next_token(s))
            goto fail;
        break;
    case TOK_NUMBER:
        val = s->token.u.num.val;
        if (pr) {
            json_parse_record_init_primitive(ctx, pr, val,
                                             s->token.ptr - s->buf_start,
                                             s->buf_ptr - s->token.ptr);
        }
        if (json_next_token(s))
            goto fail;
        break;
    case TOK_IDENT:
        if (s->token.u.ident.atom == JS_ATOM_false ||
            s->token.u.ident.atom == JS_ATOM_true) {
            val = JS_NewBool(ctx, s->token.u.ident.atom == JS_ATOM_true);
            if (pr) {
                json_parse_record_init_primitive(ctx, pr, val,
                                                 s->token.ptr - s->buf_start,
                                                 s->buf_ptr - s->token.ptr);
            }
        } else if (s->token.u.ident.atom == JS_ATOM_null) {
            val = JS_NULL;
            if (pr) {
                json_parse_record_init_primitive(ctx, pr, val,
                                                 s->token.ptr - s->buf_start,
                                                 s->buf_ptr - s->token.ptr);
            }
        } else if (s->token.u.ident.atom == JS_ATOM_NaN && s->ext_json) {
            /* Note: json5 identifier handling is ambiguous e.g. is
               '{ NaN: 1 }' a valid JSON5 production ? */
            val = JS_NewFloat64(s->ctx, NAN);
        } else if (s->token.u.ident.atom == JS_ATOM_Infinity && s->ext_json) {
            val = JS_NewFloat64(s->ctx, INFINITY);
        } else {
            goto def_token;
        }
        if (json_next_token(s))
            goto fail;
        break;
    default:
    def_token:
        if (s->token.val == TOK_EOF) {
            js_parse_error(s, "Unexpected end of JSON input");
        } else {
            js_parse_error(s, "unexpected token: '%.*s'",
                           (int)(s->buf_ptr - s->token.ptr), s->token.ptr);
        }
        goto fail;
    }
    return val;
 fail:
    json_free_parse_record(ctx, pr);
    JS_FreeValue(ctx, val);
    return JS_EXCEPTION;
}

JSValue JS_ParseJSON3(JSContext *ctx, const char *buf, size_t buf_len,
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

/* if pr != NULL, then pr->value = holder by construction */
JSValue internalize_json_property(JSContext *ctx, JSValueConst holder,
                                         JSAtom name, JSValueConst reviver,
                                         const char *text_str, JSONParseRecord *pr)
{
    JSValue val, new_el, name_val, res, context;
    JSValueConst args[3];
    int ret, is_array;
    uint32_t i, len = 0;
    JSAtom prop;
    JSPropertyEnum *atoms = NULL;

    if (js_check_stack_overflow(ctx->rt, 0)) {
        return JS_ThrowStackOverflow(ctx);
    }

    val = JS_GetProperty(ctx, holder, name);
    if (JS_IsException(val))
        return val;

    if (pr) {
        if (JS_IsArray(ctx, pr->value)) {
            if (__JS_AtomIsTaggedInt(name)) {
                uint32_t idx = __JS_AtomToUInt32(name);
                if (idx < pr->u.array.count) {
                    pr = &pr->u.array.elements[idx];
                } else {
                    pr = NULL;
                }
            }
        } else {
            pr = json_parse_record_find(pr, name);
        }
        if (pr && !js_same_value(ctx, pr->value, val)) {
            pr = NULL;
        }
    }

    context = JS_NewObject(ctx);
    if (JS_IsException(context))
        goto fail;

    if (JS_IsObject(val)) {
        is_array = JS_IsArray(ctx, val);
        if (is_array < 0)
            goto fail;
        if (is_array) {
            if (js_get_length32(ctx, &len, val))
                goto fail;
        } else {
            ret = JS_GetOwnPropertyNamesInternal(ctx, &atoms, &len, JS_VALUE_GET_OBJ(val), JS_GPN_ENUM_ONLY | JS_GPN_STRING_MASK);
            if (ret < 0)
                goto fail;
        }
        for(i = 0; i < len; i++) {
            if (is_array) {
                prop = JS_NewAtomUInt32(ctx, i);
                if (prop == JS_ATOM_NULL)
                    goto fail;
            } else {
                prop = JS_DupAtom(ctx, atoms[i].atom);
            }
            new_el = internalize_json_property(ctx, val, prop, reviver, text_str, pr);
            if (JS_IsException(new_el)) {
                JS_FreeAtom(ctx, prop);
                goto fail;
            }
            if (JS_IsUndefined(new_el)) {
                ret = JS_DeleteProperty(ctx, val, prop, 0);
            } else {
                ret = JS_DefinePropertyValue(ctx, val, prop, new_el, JS_PROP_C_W_E);
            }
            JS_FreeAtom(ctx, prop);
            if (ret < 0)
                goto fail;
        }
    } else {
        if (pr) {
            new_el = JS_NewStringLen(ctx, text_str + pr->u.primitive.source_pos,
                                     pr->u.primitive.source_len);
            if (JS_IsException(new_el))
                goto fail;
            if (JS_DefinePropertyValue(ctx, context, JS_ATOM_source, new_el, JS_PROP_C_W_E) < 0)
                goto fail;
        }
    }
    JS_FreePropertyEnum(ctx, atoms, len);
    atoms = NULL;
    name_val = JS_AtomToValue(ctx, name);
    if (JS_IsException(name_val))
        goto fail;
    args[0] = name_val;
    args[1] = val;
    args[2] = context;
    res = JS_Call(ctx, reviver, holder, 3, args);
    JS_FreeValue(ctx, name_val);
    JS_FreeValue(ctx, val);
    JS_FreeValue(ctx, context);
    return res;
 fail:
    JS_FreePropertyEnum(ctx, atoms, len);
    JS_FreeValue(ctx, context);
    JS_FreeValue(ctx, val);
    return JS_EXCEPTION;
}

JSValue js_json_parse(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv)
{
    JSValue obj;
    const char *str;
    size_t len;

    str = JS_ToCStringLen(ctx, &len, argv[0]);
    if (!str)
        return JS_EXCEPTION;
    if (argc > 1 && JS_IsFunction(ctx, argv[1])) {
        JSONParseRecord pr_s, *pr = &pr_s, *pr1;
        JSValue root;
        JSValueConst reviver;
        int size;

        reviver = argv[1];
        root = JS_NewObject(ctx);
        if (JS_IsException(root))
            goto fail;
        json_parse_record_init_obj(ctx, pr, root);
        size = 0;
        pr1 = json_parse_record_add(ctx, pr, JS_ATOM_empty_string, &size);
        if (!pr1)
            goto fail1;

        obj = JS_ParseJSON3(ctx, str, len, "<input>", 0, pr1);
        if (JS_IsException(obj))
            goto fail1;

        if (JS_DefinePropertyValue(ctx, root, JS_ATOM_empty_string, obj,
                                   JS_PROP_C_W_E) < 0) {
            JS_FreeValue(ctx, obj);
        fail1:
            json_free_parse_record(ctx, pr);
            JS_FreeValue(ctx, root);
            goto fail;
        }

        obj = internalize_json_property(ctx, root, JS_ATOM_empty_string,
                                        reviver, str, pr);
        json_free_parse_record(ctx, pr);
        JS_FreeValue(ctx, root);
    } else {
        obj = JS_ParseJSON3(ctx, str, len, "<input>", 0, NULL);
    }
    JS_FreeCString(ctx, str);
    return obj;
 fail:
    JS_FreeCString(ctx, str);
    return JS_EXCEPTION;
}

JSValue js_json_isRawJSON(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv)
{
    JSValueConst obj = argv[0];
    if (JS_VALUE_GET_TAG(obj) == JS_TAG_OBJECT) {
        JSObject *p = JS_VALUE_GET_OBJ(obj);
        return JS_NewBool(ctx, p->class_id == JS_CLASS_RAWJSON);
    } else {
        return JS_FALSE;
    }
}

BOOL is_valid_raw_json_char(int c)
{
    return ((c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '-' ||
            c == '"');
}

JSValue js_json_rawJSON(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv)
{
    JSValue str, res, obj;
    JSString *p;
    str = JS_ToString(ctx, argv[0]);
    if (JS_IsException(str))
        return str;
    p = JS_VALUE_GET_STRING(str);
    if (p->len == 0 ||
        !is_valid_raw_json_char(string_get(p, 0)) ||
        !is_valid_raw_json_char(string_get(p, p->len - 1))) {
        goto syntax_error;
    }
    res = js_json_parse(ctx, JS_UNDEFINED, 1, (JSValueConst *)&str);
    if (JS_IsException(res)) {
    syntax_error:
        JS_ThrowSyntaxError(ctx, "invalid rawJSON string");
        goto fail;
    }
    JS_FreeValue(ctx, res);

    obj = JS_NewObjectProtoClass(ctx, JS_NULL, JS_CLASS_RAWJSON);
    if (JS_IsException(obj))
        goto fail;
    if (JS_DefinePropertyValue(ctx, obj, JS_ATOM_rawJSON, str, JS_PROP_ENUMERABLE) < 0) {
        JS_FreeValue(ctx, obj);
        return JS_EXCEPTION;
    }
    JS_PreventExtensions(ctx, obj);
    return obj;
 fail:
    JS_FreeValue(ctx, str);
    return JS_EXCEPTION;
}


int JS_ToQuotedString(JSContext *ctx, StringBuffer *b, JSValueConst val1)
{
    JSValue val;
    JSString *p;
    int i;
    uint32_t c;
    char buf[16];

    val = JS_ToStringCheckObject(ctx, val1);
    if (JS_IsException(val))
        return -1;
    p = JS_VALUE_GET_STRING(val);

    if (string_buffer_putc8(b, '\"'))
        goto fail;
    for(i = 0; i < p->len; ) {
        c = string_getc(p, &i);
        switch(c) {
        case '\t':
            c = 't';
            goto quote;
        case '\r':
            c = 'r';
            goto quote;
        case '\n':
            c = 'n';
            goto quote;
        case '\b':
            c = 'b';
            goto quote;
        case '\f':
            c = 'f';
            goto quote;
        case '\"':
        case '\\':
        quote:
            if (string_buffer_putc8(b, '\\'))
                goto fail;
            if (string_buffer_putc8(b, c))
                goto fail;
            break;
        default:
            if (c < 32 || is_surrogate(c)) {
                snprintf(buf, sizeof(buf), "\\u%04x", c);
                if (string_buffer_puts8(b, buf))
                    goto fail;
            } else {
                if (string_buffer_putc(b, c))
                    goto fail;
            }
            break;
        }
    }
    if (string_buffer_putc8(b, '\"'))
        goto fail;
    JS_FreeValue(ctx, val);
    return 0;
 fail:
    JS_FreeValue(ctx, val);
    return -1;
}

int JS_ToQuotedStringFree(JSContext *ctx, StringBuffer *b, JSValue val) {
    int ret = JS_ToQuotedString(ctx, b, val);
    JS_FreeValue(ctx, val);
    return ret;
}

JSValue js_json_check(JSContext *ctx, JSONStringifyContext *jsc,
                             JSValueConst holder, JSValue val, JSValueConst key)
{
    JSValue v;
    JSValueConst args[2];

    /* check for object.toJSON method */
    /* ECMA specifies this is done only for Object and BigInt */
    if (JS_IsObject(val) || JS_IsBigInt(ctx, val)) {
        JSValue f = JS_GetProperty(ctx, val, JS_ATOM_toJSON);
        if (JS_IsException(f))
            goto exception;
        if (JS_IsFunction(ctx, f)) {
            v = JS_CallFree(ctx, f, val, 1, &key);
            JS_FreeValue(ctx, val);
            val = v;
            if (JS_IsException(val))
                goto exception;
        } else {
            JS_FreeValue(ctx, f);
        }
    }

    if (!JS_IsUndefined(jsc->replacer_func)) {
        args[0] = key;
        args[1] = val;
        v = JS_Call(ctx, jsc->replacer_func, holder, 2, args);
        JS_FreeValue(ctx, val);
        val = v;
        if (JS_IsException(val))
            goto exception;
    }

    switch (JS_VALUE_GET_NORM_TAG(val)) {
    case JS_TAG_OBJECT:
        if (JS_IsFunction(ctx, val))
            break;
    case JS_TAG_STRING:
    case JS_TAG_STRING_ROPE:
    case JS_TAG_INT:
    case JS_TAG_FLOAT64:
    case JS_TAG_BOOL:
    case JS_TAG_NULL:
    case JS_TAG_SHORT_BIG_INT:
    case JS_TAG_BIG_INT:
    case JS_TAG_EXCEPTION:
        return val;
    default:
        break;
    }
    JS_FreeValue(ctx, val);
    return JS_UNDEFINED;

exception:
    JS_FreeValue(ctx, val);
    return JS_EXCEPTION;
}

int js_json_to_str(JSContext *ctx, JSONStringifyContext *jsc,
                          JSValueConst holder, JSValue val,
                          JSValueConst indent)
{
    JSValue indent1, sep, sep1, tab, v, prop;
    JSObject *p;
    int64_t i, len;
    int cl, ret;
    BOOL has_content;

    indent1 = JS_UNDEFINED;
    sep = JS_UNDEFINED;
    sep1 = JS_UNDEFINED;
    tab = JS_UNDEFINED;
    prop = JS_UNDEFINED;

    if (js_check_stack_overflow(ctx->rt, 0)) {
        JS_ThrowStackOverflow(ctx);
        goto exception;
    }

    if (JS_IsObject(val)) {
        p = JS_VALUE_GET_OBJ(val);
        cl = p->class_id;
        if (cl == JS_CLASS_STRING) {
            val = JS_ToStringFree(ctx, val);
            if (JS_IsException(val))
                goto exception;
            goto concat_primitive;
        } else if (cl == JS_CLASS_NUMBER) {
            val = JS_ToNumberFree(ctx, val);
            if (JS_IsException(val))
                goto exception;
            goto concat_primitive;
        } else if (cl == JS_CLASS_BOOLEAN || cl == JS_CLASS_BIG_INT) {
            /* This will thow the same error as for the primitive object */
            set_value(ctx, &val, JS_DupValue(ctx, p->u.object_data));
            goto concat_primitive;
        } else if (cl == JS_CLASS_RAWJSON) {
            JSValue val1;
            val1 = JS_GetProperty(ctx, val, JS_ATOM_rawJSON);
            if (JS_IsException(val1))
                goto exception;
            JS_FreeValue(ctx, val);
            val = val1;
            goto concat_value;
        }
        v = js_array_includes(ctx, jsc->stack, 1, (JSValueConst *)&val);
        if (JS_IsException(v))
            goto exception;
        if (JS_ToBoolFree(ctx, v)) {
            JS_ThrowTypeError(ctx, "circular reference");
            goto exception;
        }
        indent1 = JS_ConcatString(ctx, JS_DupValue(ctx, indent), JS_DupValue(ctx, jsc->gap));
        if (JS_IsException(indent1))
            goto exception;
        if (!JS_IsEmptyString(jsc->gap)) {
            sep = JS_ConcatString3(ctx, "\n", JS_DupValue(ctx, indent1), "");
            if (JS_IsException(sep))
                goto exception;
            sep1 = js_new_string8(ctx, " ");
            if (JS_IsException(sep1))
                goto exception;
        } else {
            sep = JS_DupValue(ctx, jsc->empty);
            sep1 = JS_DupValue(ctx, jsc->empty);
        }
        v = js_array_push(ctx, jsc->stack, 1, (JSValueConst *)&val, 0);
        if (check_exception_free(ctx, v))
            goto exception;
        ret = JS_IsArray(ctx, val);
        if (ret < 0)
            goto exception;
        if (ret) {
            if (js_get_length64(ctx, &len, val))
                goto exception;
            string_buffer_putc8(jsc->b, '[');
            for(i = 0; i < len; i++) {
                if (i > 0)
                    string_buffer_putc8(jsc->b, ',');
                string_buffer_concat_value(jsc->b, sep);
                v = JS_GetPropertyInt64(ctx, val, i);
                if (JS_IsException(v))
                    goto exception;
                /* XXX: could do this string conversion only when needed */
                prop = JS_ToStringFree(ctx, JS_NewInt64(ctx, i));
                if (JS_IsException(prop))
                    goto exception;
                v = js_json_check(ctx, jsc, val, v, prop);
                JS_FreeValue(ctx, prop);
                prop = JS_UNDEFINED;
                if (JS_IsException(v))
                    goto exception;
                if (JS_IsUndefined(v))
                    v = JS_NULL;
                if (js_json_to_str(ctx, jsc, val, v, indent1))
                    goto exception;
            }
            if (len > 0 && !JS_IsEmptyString(jsc->gap)) {
                string_buffer_putc8(jsc->b, '\n');
                string_buffer_concat_value(jsc->b, indent);
            }
            string_buffer_putc8(jsc->b, ']');
        } else {
            if (!JS_IsUndefined(jsc->property_list))
                tab = JS_DupValue(ctx, jsc->property_list);
            else
                tab = js_object_keys(ctx, JS_UNDEFINED, 1, (JSValueConst *)&val, JS_ITERATOR_KIND_KEY);
            if (JS_IsException(tab))
                goto exception;
            if (js_get_length64(ctx, &len, tab))
                goto exception;
            string_buffer_putc8(jsc->b, '{');
            has_content = FALSE;
            for(i = 0; i < len; i++) {
                JS_FreeValue(ctx, prop);
                prop = JS_GetPropertyInt64(ctx, tab, i);
                if (JS_IsException(prop))
                    goto exception;
                v = JS_GetPropertyValue(ctx, val, JS_DupValue(ctx, prop));
                if (JS_IsException(v))
                    goto exception;
                v = js_json_check(ctx, jsc, val, v, prop);
                if (JS_IsException(v))
                    goto exception;
                if (!JS_IsUndefined(v)) {
                    if (has_content)
                        string_buffer_putc8(jsc->b, ',');
                    string_buffer_concat_value(jsc->b, sep);
                    if (JS_ToQuotedString(ctx, jsc->b, prop)) {
                        JS_FreeValue(ctx, v);
                        goto exception;
                    }
                    string_buffer_putc8(jsc->b, ':');
                    string_buffer_concat_value(jsc->b, sep1);
                    if (js_json_to_str(ctx, jsc, val, v, indent1))
                        goto exception;
                    has_content = TRUE;
                }
            }
            if (has_content && !JS_IsEmptyString(jsc->gap)) {
                string_buffer_putc8(jsc->b, '\n');
                string_buffer_concat_value(jsc->b, indent);
            }
            string_buffer_putc8(jsc->b, '}');
        }
        if (check_exception_free(ctx, js_array_pop(ctx, jsc->stack, 0, NULL, 0)))
            goto exception;
        JS_FreeValue(ctx, val);
        JS_FreeValue(ctx, tab);
        JS_FreeValue(ctx, sep);
        JS_FreeValue(ctx, sep1);
        JS_FreeValue(ctx, indent1);
        JS_FreeValue(ctx, prop);
        return 0;
    }
 concat_primitive:
    switch (JS_VALUE_GET_NORM_TAG(val)) {
    case JS_TAG_STRING:
    case JS_TAG_STRING_ROPE:
        return JS_ToQuotedStringFree(ctx, jsc->b, val);
    case JS_TAG_FLOAT64:
        if (!isfinite(JS_VALUE_GET_FLOAT64(val))) {
            val = JS_NULL;
        }
        goto concat_value;
    case JS_TAG_INT:
    case JS_TAG_BOOL:
    case JS_TAG_NULL:
    concat_value:
        return string_buffer_concat_value_free(jsc->b, val);
    case JS_TAG_SHORT_BIG_INT:
    case JS_TAG_BIG_INT:
        /* reject big numbers: use toJSON method to override */
        JS_ThrowTypeError(ctx, "Do not know how to serialize a BigInt");
        goto exception;
    default:
        JS_FreeValue(ctx, val);
        return 0;
    }

exception:
    JS_FreeValue(ctx, val);
    JS_FreeValue(ctx, tab);
    JS_FreeValue(ctx, sep);
    JS_FreeValue(ctx, sep1);
    JS_FreeValue(ctx, indent1);
    JS_FreeValue(ctx, prop);
    return -1;
}

JSValue js_json_stringify(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv)
{
    // stringify(val, replacer, space)
    return JS_JSONStringify(ctx, argv[0], argv[1], argv[2]);
}

/* Reflect */

JSValue js_reflect_apply(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv)
{
    return js_function_apply(ctx, argv[0], max_int(0, argc - 1), argv + 1, 2);
}

JSValue js_reflect_construct(JSContext *ctx, JSValueConst this_val,
                                    int argc, JSValueConst *argv)
{
    JSValueConst func, array_arg, new_target;
    JSValue *tab, ret;
    uint32_t len;

    func = argv[0];
    array_arg = argv[1];
    if (argc > 2) {
        new_target = argv[2];
        if (!JS_IsConstructor(ctx, new_target))
            return JS_ThrowTypeErrorNotAConstructor(ctx, new_target);
    } else {
        new_target = func;
    }
    tab = build_arg_list(ctx, &len, array_arg);
    if (!tab)
        return JS_EXCEPTION;
    ret = JS_CallConstructor2(ctx, func, new_target, len, (JSValueConst *)tab);
    free_arg_list(ctx, tab, len);
    return ret;
}

JSValue js_reflect_deleteProperty(JSContext *ctx, JSValueConst this_val,
                                         int argc, JSValueConst *argv)
{
    JSValueConst obj;
    JSAtom atom;
    int ret;

    obj = argv[0];
    if (JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT)
        return JS_ThrowTypeErrorNotAnObject(ctx);
    atom = JS_ValueToAtom(ctx, argv[1]);
    if (unlikely(atom == JS_ATOM_NULL))
        return JS_EXCEPTION;
    ret = JS_DeleteProperty(ctx, obj, atom, 0);
    JS_FreeAtom(ctx, atom);
    if (ret < 0)
        return JS_EXCEPTION;
    else
        return JS_NewBool(ctx, ret);
}

JSValue js_reflect_get(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv)
{
    JSValueConst obj, prop, receiver;
    JSAtom atom;
    JSValue ret;

    obj = argv[0];
    prop = argv[1];
    if (JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT)
        return JS_ThrowTypeErrorNotAnObject(ctx);
    if (argc > 2)
        receiver = argv[2];
    else
        receiver = obj;
    atom = JS_ValueToAtom(ctx, prop);
    if (unlikely(atom == JS_ATOM_NULL))
        return JS_EXCEPTION;
    ret = JS_GetPropertyInternal(ctx, obj, atom, receiver, FALSE);
    JS_FreeAtom(ctx, atom);
    return ret;
}

JSValue js_reflect_has(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv)
{
    JSValueConst obj, prop;
    JSAtom atom;
    int ret;

    obj = argv[0];
    prop = argv[1];
    if (JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT)
        return JS_ThrowTypeErrorNotAnObject(ctx);
    atom = JS_ValueToAtom(ctx, prop);
    if (unlikely(atom == JS_ATOM_NULL))
        return JS_EXCEPTION;
    ret = JS_HasProperty(ctx, obj, atom);
    JS_FreeAtom(ctx, atom);
    if (ret < 0)
        return JS_EXCEPTION;
    else
        return JS_NewBool(ctx, ret);
}

JSValue js_reflect_set(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv)
{
    JSValueConst obj, prop, val, receiver;
    int ret;
    JSAtom atom;

    obj = argv[0];
    prop = argv[1];
    val = argv[2];
    if (argc > 3)
        receiver = argv[3];
    else
        receiver = obj;
    if (JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT)
        return JS_ThrowTypeErrorNotAnObject(ctx);
    atom = JS_ValueToAtom(ctx, prop);
    if (unlikely(atom == JS_ATOM_NULL))
        return JS_EXCEPTION;
    ret = JS_SetPropertyInternal(ctx, obj, atom,
                                 JS_DupValue(ctx, val), receiver, 0);
    JS_FreeAtom(ctx, atom);
    if (ret < 0)
        return JS_EXCEPTION;
    else
        return JS_NewBool(ctx, ret);
}

JSValue js_reflect_setPrototypeOf(JSContext *ctx, JSValueConst this_val,
                                         int argc, JSValueConst *argv)
{
    int ret;
    ret = JS_SetPrototypeInternal(ctx, argv[0], argv[1], FALSE);
    if (ret < 0)
        return JS_EXCEPTION;
    else
        return JS_NewBool(ctx, ret);
}

JSValue js_reflect_ownKeys(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv)
{
    if (JS_VALUE_GET_TAG(argv[0]) != JS_TAG_OBJECT)
        return JS_ThrowTypeErrorNotAnObject(ctx);
    return JS_GetOwnPropertyNames2(ctx, argv[0],
                                   JS_GPN_STRING_MASK | JS_GPN_SYMBOL_MASK,
                                   JS_ITERATOR_KIND_KEY);
}

/* Proxy */

void js_proxy_finalizer(JSRuntime *rt, JSValue val)
{
    JSProxyData *s = JS_GetOpaque(val, JS_CLASS_PROXY);
    if (s) {
        JS_FreeValueRT(rt, s->target);
        JS_FreeValueRT(rt, s->handler);
        js_free_rt(rt, s);
    }
}

void js_proxy_mark(JSRuntime *rt, JSValueConst val,
                          JS_MarkFunc *mark_func)
{
    JSProxyData *s = JS_GetOpaque(val, JS_CLASS_PROXY);
    if (s) {
        JS_MarkValue(rt, s->target, mark_func);
        JS_MarkValue(rt, s->handler, mark_func);
    }
}

JSValue JS_ThrowTypeErrorRevokedProxy(JSContext *ctx)
{
    return JS_ThrowTypeError(ctx, "revoked proxy");
}

JSProxyData *get_proxy_method(JSContext *ctx, JSValue *pmethod,
                                     JSValueConst obj, JSAtom name)
{
    JSProxyData *s = JS_GetOpaque(obj, JS_CLASS_PROXY);
    JSValue method;

    /* safer to test recursion in all proxy methods */
    if (js_check_stack_overflow(ctx->rt, 0)) {
        JS_ThrowStackOverflow(ctx);
        return NULL;
    }

    /* 's' should never be NULL */
    if (s->is_revoked) {
        JS_ThrowTypeErrorRevokedProxy(ctx);
        return NULL;
    }
    method = JS_GetProperty(ctx, s->handler, name);
    if (JS_IsException(method))
        return NULL;
    if (JS_IsNull(method))
        method = JS_UNDEFINED;
    *pmethod = method;
    return s;
}

JSValue js_proxy_get_prototype(JSContext *ctx, JSValueConst obj)
{
    JSProxyData *s;
    JSValue method, ret, proto1;
    int res;

    s = get_proxy_method(ctx, &method, obj, JS_ATOM_getPrototypeOf);
    if (!s)
        return JS_EXCEPTION;
    if (JS_IsUndefined(method))
        return JS_GetPrototype(ctx, s->target);
    ret = JS_CallFree(ctx, method, s->handler, 1, (JSValueConst *)&s->target);
    if (JS_IsException(ret))
        return ret;
    if (JS_VALUE_GET_TAG(ret) != JS_TAG_NULL &&
        JS_VALUE_GET_TAG(ret) != JS_TAG_OBJECT) {
        goto fail;
    }
    res = JS_IsExtensible(ctx, s->target);
    if (res < 0) {
        JS_FreeValue(ctx, ret);
        return JS_EXCEPTION;
    }
    if (!res) {
        /* check invariant */
        proto1 = JS_GetPrototype(ctx, s->target);
        if (JS_IsException(proto1)) {
            JS_FreeValue(ctx, ret);
            return JS_EXCEPTION;
        }
        if (!js_same_value(ctx, proto1, ret)) {
            JS_FreeValue(ctx, proto1);
        fail:
            JS_FreeValue(ctx, ret);
            return JS_ThrowTypeError(ctx, "proxy: inconsistent prototype");
        }
        JS_FreeValue(ctx, proto1);
    }
    return ret;
}

int js_proxy_set_prototype(JSContext *ctx, JSValueConst obj,
                                  JSValueConst proto_val)
{
    JSProxyData *s;
    JSValue method, ret, proto1;
    JSValueConst args[2];
    BOOL res;
    int res2;

    s = get_proxy_method(ctx, &method, obj, JS_ATOM_setPrototypeOf);
    if (!s)
        return -1;
    if (JS_IsUndefined(method))
        return JS_SetPrototypeInternal(ctx, s->target, proto_val, FALSE);
    args[0] = s->target;
    args[1] = proto_val;
    ret = JS_CallFree(ctx, method, s->handler, 2, args);
    if (JS_IsException(ret))
        return -1;
    res = JS_ToBoolFree(ctx, ret);
    if (!res)
        return FALSE;
    res2 = JS_IsExtensible(ctx, s->target);
    if (res2 < 0)
        return -1;
    if (!res2) {
        proto1 = JS_GetPrototype(ctx, s->target);
        if (JS_IsException(proto1))
            return -1;
        if (!js_same_value(ctx, proto_val, proto1)) {
            JS_FreeValue(ctx, proto1);
            JS_ThrowTypeError(ctx, "proxy: inconsistent prototype");
            return -1;
        }
        JS_FreeValue(ctx, proto1);
    }
    return TRUE;
}

int js_proxy_is_extensible(JSContext *ctx, JSValueConst obj)
{
    JSProxyData *s;
    JSValue method, ret;
    BOOL res;
    int res2;

    s = get_proxy_method(ctx, &method, obj, JS_ATOM_isExtensible);
    if (!s)
        return -1;
    if (JS_IsUndefined(method))
        return JS_IsExtensible(ctx, s->target);
    ret = JS_CallFree(ctx, method, s->handler, 1, (JSValueConst *)&s->target);
    if (JS_IsException(ret))
        return -1;
    res = JS_ToBoolFree(ctx, ret);
    res2 = JS_IsExtensible(ctx, s->target);
    if (res2 < 0)
        return res2;
    if (res != res2) {
        JS_ThrowTypeError(ctx, "proxy: inconsistent isExtensible");
        return -1;
    }
    return res;
}

int js_proxy_prevent_extensions(JSContext *ctx, JSValueConst obj)
{
    JSProxyData *s;
    JSValue method, ret;
    BOOL res;
    int res2;

    s = get_proxy_method(ctx, &method, obj, JS_ATOM_preventExtensions);
    if (!s)
        return -1;
    if (JS_IsUndefined(method))
        return JS_PreventExtensions(ctx, s->target);
    ret = JS_CallFree(ctx, method, s->handler, 1, (JSValueConst *)&s->target);
    if (JS_IsException(ret))
        return -1;
    res = JS_ToBoolFree(ctx, ret);
    if (res) {
        res2 = JS_IsExtensible(ctx, s->target);
        if (res2 < 0)
            return res2;
        if (res2) {
            JS_ThrowTypeError(ctx, "proxy: inconsistent preventExtensions");
            return -1;
        }
    }
    return res;
}

int js_proxy_has(JSContext *ctx, JSValueConst obj, JSAtom atom)
{
    JSProxyData *s;
    JSValue method, ret1, atom_val;
    int ret, res;
    JSObject *p;
    JSValueConst args[2];
    BOOL res2;

    s = get_proxy_method(ctx, &method, obj, JS_ATOM_has);
    if (!s)
        return -1;
    if (JS_IsUndefined(method))
        return JS_HasProperty(ctx, s->target, atom);
    atom_val = JS_AtomToValue(ctx, atom);
    if (JS_IsException(atom_val)) {
        JS_FreeValue(ctx, method);
        return -1;
    }
    args[0] = s->target;
    args[1] = atom_val;
    ret1 = JS_CallFree(ctx, method, s->handler, 2, args);
    JS_FreeValue(ctx, atom_val);
    if (JS_IsException(ret1))
        return -1;
    ret = JS_ToBoolFree(ctx, ret1);
    if (!ret) {
        JSPropertyDescriptor desc;
        p = JS_VALUE_GET_OBJ(s->target);
        res = JS_GetOwnPropertyInternal(ctx, &desc, p, atom);
        if (res < 0)
            return -1;
        if (res) {
            res2 = !(desc.flags & JS_PROP_CONFIGURABLE);
            js_free_desc(ctx, &desc);
            if (res2 || !p->extensible) {
                JS_ThrowTypeError(ctx, "proxy: inconsistent has");
                return -1;
            }
        }
    }
    return ret;
}

JSValue js_proxy_get(JSContext *ctx, JSValueConst obj, JSAtom atom,
                            JSValueConst receiver)
{
    JSProxyData *s;
    JSValue method, ret, atom_val;
    int res;
    JSValueConst args[3];
    JSPropertyDescriptor desc;

    s = get_proxy_method(ctx, &method, obj, JS_ATOM_get);
    if (!s)
        return JS_EXCEPTION;
    /* Note: recursion is possible thru the prototype of s->target */
    if (JS_IsUndefined(method))
        return JS_GetPropertyInternal(ctx, s->target, atom, receiver, FALSE);
    atom_val = JS_AtomToValue(ctx, atom);
    if (JS_IsException(atom_val)) {
        JS_FreeValue(ctx, method);
        return JS_EXCEPTION;
    }
    args[0] = s->target;
    args[1] = atom_val;
    args[2] = receiver;
    ret = JS_CallFree(ctx, method, s->handler, 3, args);
    JS_FreeValue(ctx, atom_val);
    if (JS_IsException(ret))
        return JS_EXCEPTION;
    res = JS_GetOwnPropertyInternal(ctx, &desc, JS_VALUE_GET_OBJ(s->target), atom);
    if (res < 0) {
        JS_FreeValue(ctx, ret);
        return JS_EXCEPTION;
    }
    if (res) {
        if ((desc.flags & (JS_PROP_GETSET | JS_PROP_CONFIGURABLE | JS_PROP_WRITABLE)) == 0) {
            if (!js_same_value(ctx, desc.value, ret)) {
                goto fail;
            }
        } else if ((desc.flags & (JS_PROP_GETSET | JS_PROP_CONFIGURABLE)) == JS_PROP_GETSET) {
            if (JS_IsUndefined(desc.getter) && !JS_IsUndefined(ret)) {
            fail:
                js_free_desc(ctx, &desc);
                JS_FreeValue(ctx, ret);
                return JS_ThrowTypeError(ctx, "proxy: inconsistent get");
            }
        }
        js_free_desc(ctx, &desc);
    }
    return ret;
}

int js_proxy_set(JSContext *ctx, JSValueConst obj, JSAtom atom,
                        JSValueConst value, JSValueConst receiver, int flags)
{
    JSProxyData *s;
    JSValue method, ret1, atom_val;
    int ret, res;
    JSValueConst args[4];

    s = get_proxy_method(ctx, &method, obj, JS_ATOM_set);
    if (!s)
        return -1;
    if (JS_IsUndefined(method)) {
        return JS_SetPropertyInternal(ctx, s->target, atom,
                                      JS_DupValue(ctx, value), receiver,
                                      flags);
    }
    atom_val = JS_AtomToValue(ctx, atom);
    if (JS_IsException(atom_val)) {
        JS_FreeValue(ctx, method);
        return -1;
    }
    args[0] = s->target;
    args[1] = atom_val;
    args[2] = value;
    args[3] = receiver;
    ret1 = JS_CallFree(ctx, method, s->handler, 4, args);
    JS_FreeValue(ctx, atom_val);
    if (JS_IsException(ret1))
        return -1;
    ret = JS_ToBoolFree(ctx, ret1);
    if (ret) {
        JSPropertyDescriptor desc;
        res = JS_GetOwnPropertyInternal(ctx, &desc, JS_VALUE_GET_OBJ(s->target), atom);
        if (res < 0)
            return -1;
        if (res) {
            if ((desc.flags & (JS_PROP_GETSET | JS_PROP_CONFIGURABLE | JS_PROP_WRITABLE)) == 0) {
                if (!js_same_value(ctx, desc.value, value)) {
                    goto fail;
                }
            } else if ((desc.flags & (JS_PROP_GETSET | JS_PROP_CONFIGURABLE)) == JS_PROP_GETSET && JS_IsUndefined(desc.setter)) {
                fail:
                    js_free_desc(ctx, &desc);
                    JS_ThrowTypeError(ctx, "proxy: inconsistent set");
                    return -1;
            }
            js_free_desc(ctx, &desc);
        }
    } else {
        if ((flags & JS_PROP_THROW) ||
            ((flags & JS_PROP_THROW_STRICT) && is_strict_mode(ctx))) {
            JS_ThrowTypeError(ctx, "proxy: cannot set property");
            return -1;
        }
    }
    return ret;
}

JSValue js_create_desc(JSContext *ctx, JSValueConst val,
                              JSValueConst getter, JSValueConst setter,
                              int flags)
{
    JSValue ret;
    ret = JS_NewObject(ctx);
    if (JS_IsException(ret))
        return ret;
    if (flags & JS_PROP_HAS_GET) {
        JS_DefinePropertyValue(ctx, ret, JS_ATOM_get, JS_DupValue(ctx, getter),
                               JS_PROP_C_W_E);
    }
    if (flags & JS_PROP_HAS_SET) {
        JS_DefinePropertyValue(ctx, ret, JS_ATOM_set, JS_DupValue(ctx, setter),
                               JS_PROP_C_W_E);
    }
    if (flags & JS_PROP_HAS_VALUE) {
        JS_DefinePropertyValue(ctx, ret, JS_ATOM_value, JS_DupValue(ctx, val),
                               JS_PROP_C_W_E);
    }
    if (flags & JS_PROP_HAS_WRITABLE) {
        JS_DefinePropertyValue(ctx, ret, JS_ATOM_writable,
                               JS_NewBool(ctx, flags & JS_PROP_WRITABLE),
                               JS_PROP_C_W_E);
    }
    if (flags & JS_PROP_HAS_ENUMERABLE) {
        JS_DefinePropertyValue(ctx, ret, JS_ATOM_enumerable,
                               JS_NewBool(ctx, flags & JS_PROP_ENUMERABLE),
                               JS_PROP_C_W_E);
    }
    if (flags & JS_PROP_HAS_CONFIGURABLE) {
        JS_DefinePropertyValue(ctx, ret, JS_ATOM_configurable,
                               JS_NewBool(ctx, flags & JS_PROP_CONFIGURABLE),
                               JS_PROP_C_W_E);
    }
    return ret;
}

int js_proxy_get_own_property(JSContext *ctx, JSPropertyDescriptor *pdesc,
                                     JSValueConst obj, JSAtom prop)
{
    JSProxyData *s;
    JSValue method, trap_result_obj, prop_val;
    int res, target_desc_ret, ret;
    JSObject *p;
    JSValueConst args[2];
    JSPropertyDescriptor result_desc, target_desc;

    s = get_proxy_method(ctx, &method, obj, JS_ATOM_getOwnPropertyDescriptor);
    if (!s)
        return -1;
    p = JS_VALUE_GET_OBJ(s->target);
    if (JS_IsUndefined(method)) {
        return JS_GetOwnPropertyInternal(ctx, pdesc, p, prop);
    }
    prop_val = JS_AtomToValue(ctx, prop);
    if (JS_IsException(prop_val)) {
        JS_FreeValue(ctx, method);
        return -1;
    }
    args[0] = s->target;
    args[1] = prop_val;
    trap_result_obj = JS_CallFree(ctx, method, s->handler, 2, args);
    JS_FreeValue(ctx, prop_val);
    if (JS_IsException(trap_result_obj))
        return -1;
    if (!JS_IsObject(trap_result_obj) && !JS_IsUndefined(trap_result_obj)) {
        JS_FreeValue(ctx, trap_result_obj);
        goto fail;
    }
    target_desc_ret = JS_GetOwnPropertyInternal(ctx, &target_desc, p, prop);
    if (target_desc_ret < 0) {
        JS_FreeValue(ctx, trap_result_obj);
        return -1;
    }
    if (target_desc_ret)
        js_free_desc(ctx, &target_desc);
    if (JS_IsUndefined(trap_result_obj)) {
        if (target_desc_ret) {
            if (!(target_desc.flags & JS_PROP_CONFIGURABLE) || !p->extensible)
                goto fail;
        }
        ret = FALSE;
    } else {
        int flags1, extensible_target;
        extensible_target = JS_IsExtensible(ctx, s->target);
        if (extensible_target < 0) {
            JS_FreeValue(ctx, trap_result_obj);
            return -1;
        }
        res = js_obj_to_desc(ctx, &result_desc, trap_result_obj);
        JS_FreeValue(ctx, trap_result_obj);
        if (res < 0)
            return -1;

        /* convert the result_desc.flags to property flags */
        if (result_desc.flags & (JS_PROP_HAS_GET | JS_PROP_HAS_SET)) {
            result_desc.flags |= JS_PROP_GETSET;
        } else {
            result_desc.flags |= JS_PROP_NORMAL;
        }
        result_desc.flags &= (JS_PROP_C_W_E | JS_PROP_TMASK);

        if (target_desc_ret) {
            /* convert result_desc.flags to defineProperty flags */
            flags1 = result_desc.flags | JS_PROP_HAS_CONFIGURABLE | JS_PROP_HAS_ENUMERABLE;
            if (result_desc.flags & JS_PROP_GETSET)
                flags1 |= JS_PROP_HAS_GET | JS_PROP_HAS_SET;
            else
                flags1 |= JS_PROP_HAS_VALUE | JS_PROP_HAS_WRITABLE;
            /* XXX: not complete check: need to compare value &
               getter/setter as in defineproperty */
            if (!check_define_prop_flags(target_desc.flags, flags1))
                goto fail1;
        } else {
            if (!extensible_target)
                goto fail1;
        }
        if (!(result_desc.flags & JS_PROP_CONFIGURABLE)) {
            if (!target_desc_ret || (target_desc.flags & JS_PROP_CONFIGURABLE))
                goto fail1;
            if ((result_desc.flags &
                 (JS_PROP_GETSET | JS_PROP_WRITABLE)) == 0 &&
                target_desc_ret &&
                (target_desc.flags & JS_PROP_WRITABLE) != 0) {
                /* proxy-missing-checks */
            fail1:
                js_free_desc(ctx, &result_desc);
            fail:
                JS_ThrowTypeError(ctx, "proxy: inconsistent getOwnPropertyDescriptor");
                return -1;
            }
        }
        ret = TRUE;
        if (pdesc) {
            *pdesc = result_desc;
        } else {
            js_free_desc(ctx, &result_desc);
        }
    }
    return ret;
}
