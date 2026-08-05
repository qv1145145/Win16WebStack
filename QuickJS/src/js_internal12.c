#include "js_internal.h"
#include "debuglog.h"



int bc_atom_to_idx(BCWriterState *s, uint32_t *pres, JSAtom atom)
{
    uint32_t v;

    if (atom < s->first_atom || __JS_AtomIsTaggedInt(atom)) {
        *pres = atom;
        return 0;
    }
    atom -= s->first_atom;
    if (atom < s->atom_to_idx_size && s->atom_to_idx[atom] != 0) {
        *pres = s->atom_to_idx[atom];
        return 0;
    }
    if (atom >= s->atom_to_idx_size) {
        int old_size, i;
        old_size = s->atom_to_idx_size;
        if (js_resize_array(s->ctx, (void **)&s->atom_to_idx,
                            sizeof(s->atom_to_idx[0]), &s->atom_to_idx_size,
                            atom + 1))
            return -1;
        /* XXX: could add a specific js_resize_array() function to do it */
        for(i = old_size; i < s->atom_to_idx_size; i++)
            s->atom_to_idx[i] = 0;
    }
    if (js_resize_array(s->ctx, (void **)&s->idx_to_atom,
                        sizeof(s->idx_to_atom[0]),
                        &s->idx_to_atom_size, s->idx_to_atom_count + 1))
        goto fail;

    v = s->idx_to_atom_count++;
    s->idx_to_atom[v] = atom + s->first_atom;
    v += s->first_atom;
    s->atom_to_idx[atom] = v;
    *pres = v;
    return 0;
 fail:
    *pres = 0;
    return -1;
}

int bc_put_atom(BCWriterState *s, JSAtom atom)
{
    uint32_t v;

    if (__JS_AtomIsTaggedInt(atom)) {
        v = (__JS_AtomToUInt32(atom) << 1) | 1;
    } else {
        if (bc_atom_to_idx(s, &v, atom))
            return -1;
        v <<= 1;
    }
    bc_put_leb128(s, v);
    return 0;
}

void bc_byte_swap(uint8_t *bc_buf, int bc_len)
{
    int pos, len, op, fmt;

    pos = 0;
    while (pos < bc_len) {
        op = bc_buf[pos];
        len = short_opcode_info(op).size;
        fmt = short_opcode_info(op).fmt;
        switch(fmt) {
        case OP_FMT_u16:
        case OP_FMT_i16:
        case OP_FMT_label16:
        case OP_FMT_npop:
        case OP_FMT_loc:
        case OP_FMT_arg:
        case OP_FMT_var_ref:
            put_u16(bc_buf + pos + 1,
                    bswap16(get_u16(bc_buf + pos + 1)));
            break;
        case OP_FMT_i32:
        case OP_FMT_u32:
        case OP_FMT_const:
        case OP_FMT_label:
        case OP_FMT_atom:
        case OP_FMT_atom_u8:
            put_u32(bc_buf + pos + 1,
                    bswap32(get_u32(bc_buf + pos + 1)));
            break;
        case OP_FMT_atom_u16:
        case OP_FMT_label_u16:
            put_u32(bc_buf + pos + 1,
                    bswap32(get_u32(bc_buf + pos + 1)));
            put_u16(bc_buf + pos + 1 + 4,
                    bswap16(get_u16(bc_buf + pos + 1 + 4)));
            break;
        case OP_FMT_atom_label_u8:
        case OP_FMT_atom_label_u16:
            put_u32(bc_buf + pos + 1,
                    bswap32(get_u32(bc_buf + pos + 1)));
            put_u32(bc_buf + pos + 1 + 4,
                    bswap32(get_u32(bc_buf + pos + 1 + 4)));
            if (fmt == OP_FMT_atom_label_u16) {
                put_u16(bc_buf + pos + 1 + 4 + 4,
                        bswap16(get_u16(bc_buf + pos + 1 + 4 + 4)));
            }
            break;
        case OP_FMT_npop_u16:
            put_u16(bc_buf + pos + 1,
                    bswap16(get_u16(bc_buf + pos + 1)));
            put_u16(bc_buf + pos + 1 + 2,
                    bswap16(get_u16(bc_buf + pos + 1 + 2)));
            break;
        default:
            break;
        }
        pos += len;
    }
}

int JS_WriteFunctionBytecode(BCWriterState *s,
                                    const uint8_t *bc_buf1, int bc_len)
{
    int pos, len, op;
    JSAtom atom;
    uint8_t *bc_buf;
    uint32_t val;

    bc_buf = js_malloc(s->ctx, bc_len);
    if (!bc_buf)
        return -1;
    memcpy(bc_buf, bc_buf1, bc_len);

    pos = 0;
    while (pos < bc_len) {
        op = bc_buf[pos];
        len = short_opcode_info(op).size;
        switch(short_opcode_info(op).fmt) {
        case OP_FMT_atom:
        case OP_FMT_atom_u8:
        case OP_FMT_atom_u16:
        case OP_FMT_atom_label_u8:
        case OP_FMT_atom_label_u16:
            atom = get_u32(bc_buf + pos + 1);
            if (bc_atom_to_idx(s, &val, atom))
                goto fail;
            put_u32(bc_buf + pos + 1, val);
            break;
        default:
            break;
        }
        pos += len;
    }

    if (is_be())
        bc_byte_swap(bc_buf, bc_len);

    dbuf_put(&s->dbuf, bc_buf, bc_len);

    js_free(s->ctx, bc_buf);
    return 0;
 fail:
    js_free(s->ctx, bc_buf);
    return -1;
}

void JS_WriteString(BCWriterState *s, JSString *p)
{
    int i;
    bc_put_leb128(s, ((uint32_t)p->len << 1) | p->is_wide_char);
    if (p->is_wide_char) {
        for(i = 0; i < p->len; i++)
            bc_put_u16(s, p->u.str16[i]);
    } else {
        dbuf_put(&s->dbuf, p->u.str8, p->len);
    }
}

int JS_WriteBigInt(BCWriterState *s, JSValueConst obj)
{
    JSBigIntBuf buf;
    JSBigInt *p;
    uint32_t len, i;
    js_limb_t v, b;
    int shift;

    bc_put_u8(s, BC_TAG_BIG_INT);

    if (JS_VALUE_GET_TAG(obj) == JS_TAG_SHORT_BIG_INT)
        p = js_bigint_set_short(&buf, obj);
    else
        p = JS_VALUE_GET_PTR(obj);
    if (p->len == 1 && p->tab[0] == 0) {
        /* zero case */
        len = 0;
    } else {
        /* compute the length of the two's complement representation
           in bytes */
        len = p->len * (JS_LIMB_BITS / 8);
        v = p->tab[p->len - 1];
        shift = JS_LIMB_BITS - 8;
        while (shift > 0) {
            b = (v >> shift) & 0xff;
            if (b != 0x00 && b != 0xff)
                break;
            if ((b & 1) != ((v >> (shift - 1)) & 1))
                break;
            shift -= 8;
            len--;
        }
    }
    bc_put_leb128(s, len);
    if (len > 0) {
        for(i = 0; i < (len / (JS_LIMB_BITS / 8)); i++) {
#if JS_LIMB_BITS == 32
            bc_put_u32(s, p->tab[i]);
#else
            bc_put_u64(s, p->tab[i]);
#endif
        }
        for(i = 0; i < len % (JS_LIMB_BITS / 8); i++) {
            bc_put_u8(s, (p->tab[p->len - 1] >> (i * 8)) & 0xff);
        }
    }
    return 0;
}

int JS_WriteObjectRec(BCWriterState *s, JSValueConst obj);

int JS_WriteFunctionTag(BCWriterState *s, JSValueConst obj)
{
    JSFunctionBytecode *b = JS_VALUE_GET_PTR(obj);
    uint32_t flags;
    int idx, i;

    bc_put_u8(s, BC_TAG_FUNCTION_BYTECODE);
    flags = idx = 0;
    bc_set_flags(&flags, &idx, b->has_prototype, 1);
    bc_set_flags(&flags, &idx, b->has_simple_parameter_list, 1);
    bc_set_flags(&flags, &idx, b->is_derived_class_constructor, 1);
    bc_set_flags(&flags, &idx, b->need_home_object, 1);
    bc_set_flags(&flags, &idx, b->func_kind, 2);
    bc_set_flags(&flags, &idx, b->new_target_allowed, 1);
    bc_set_flags(&flags, &idx, b->super_call_allowed, 1);
    bc_set_flags(&flags, &idx, b->super_allowed, 1);
    bc_set_flags(&flags, &idx, b->arguments_allowed, 1);
    bc_set_flags(&flags, &idx, b->has_debug, 1);
    bc_set_flags(&flags, &idx, b->is_direct_or_indirect_eval, 1);
    assert(idx <= 16);
    bc_put_u16(s, flags);
    bc_put_u8(s, b->js_mode);
    bc_put_atom(s, b->func_name);

    bc_put_leb128(s, b->arg_count);
    bc_put_leb128(s, b->var_count);
    bc_put_leb128(s, b->defined_arg_count);
    bc_put_leb128(s, b->stack_size);
    bc_put_leb128(s, b->var_ref_count);
    bc_put_leb128(s, b->closure_var_count);
    bc_put_leb128(s, b->cpool_count);
    bc_put_leb128(s, b->byte_code_len);
    if (b->vardefs) {
        bc_put_leb128(s, b->arg_count + b->var_count);
        for(i = 0; i < b->arg_count + b->var_count; i++) {
            JSBytecodeVarDef *vd = &b->vardefs[i];
            bc_put_atom(s, vd->var_name);
            bc_put_leb128(s, vd->scope_next + 1);
            bc_put_leb128(s, vd->var_ref_idx);
            flags = idx = 0;
            bc_set_flags(&flags, &idx, vd->var_kind, 4);
            bc_set_flags(&flags, &idx, vd->is_const, 1);
            bc_set_flags(&flags, &idx, vd->is_lexical, 1);
            bc_set_flags(&flags, &idx, vd->is_captured, 1);
            bc_set_flags(&flags, &idx, vd->has_scope, 1);
            assert(idx <= 8);
            bc_put_u8(s, flags);
        }
    } else {
        bc_put_leb128(s, 0);
    }

    for(i = 0; i < b->closure_var_count; i++) {
        JSClosureVar *cv = &b->closure_var[i];
        bc_put_atom(s, cv->var_name);
        bc_put_leb128(s, cv->var_idx);
        flags = idx = 0;
        bc_set_flags(&flags, &idx, cv->closure_type, 3);
        bc_set_flags(&flags, &idx, cv->is_const, 1);
        bc_set_flags(&flags, &idx, cv->is_lexical, 1);
        bc_set_flags(&flags, &idx, cv->var_kind, 4);
        assert(idx <= 16);
        bc_put_u16(s, flags);
    }

    if (JS_WriteFunctionBytecode(s, b->byte_code_buf, b->byte_code_len))
        goto fail;

    if (b->has_debug) {
        bc_put_atom(s, b->debug.filename);
        bc_put_leb128(s, b->debug.pc2line_len);
        dbuf_put(&s->dbuf, b->debug.pc2line_buf, b->debug.pc2line_len);
        if (b->debug.source) {
            bc_put_leb128(s, b->debug.source_len);
            dbuf_put(&s->dbuf, (uint8_t *)b->debug.source, b->debug.source_len);
        } else {
            bc_put_leb128(s, 0);
        }
    }

    for(i = 0; i < b->cpool_count; i++) {
        if (JS_WriteObjectRec(s, b->cpool[i]))
            goto fail;
    }
    return 0;
 fail:
    return -1;
}

int JS_WriteModule(BCWriterState *s, JSValueConst obj)
{
    JSModuleDef *m = JS_VALUE_GET_PTR(obj);
    int i;

    bc_put_u8(s, BC_TAG_MODULE);
    bc_put_atom(s, m->module_name);

    bc_put_leb128(s, m->req_module_entries_count);
    for(i = 0; i < m->req_module_entries_count; i++) {
        JSReqModuleEntry *rme = &m->req_module_entries[i];
        bc_put_atom(s, rme->module_name);
        if (JS_WriteObjectRec(s, rme->attributes))
            goto fail;
    }

    bc_put_leb128(s, m->export_entries_count);
    for(i = 0; i < m->export_entries_count; i++) {
        JSExportEntry *me = &m->export_entries[i];
        bc_put_u8(s, me->export_type);
        if (me->export_type == JS_EXPORT_TYPE_LOCAL) {
            bc_put_leb128(s, me->u.local.var_idx);
        } else {
            bc_put_leb128(s, me->u.req_module_idx);
            bc_put_atom(s, me->local_name);
        }
        bc_put_atom(s, me->export_name);
    }

    bc_put_leb128(s, m->star_export_entries_count);
    for(i = 0; i < m->star_export_entries_count; i++) {
        JSStarExportEntry *se = &m->star_export_entries[i];
        bc_put_leb128(s, se->req_module_idx);
    }

    bc_put_leb128(s, m->import_entries_count);
    for(i = 0; i < m->import_entries_count; i++) {
        JSImportEntry *mi = &m->import_entries[i];
        bc_put_leb128(s, mi->var_idx);
        bc_put_u8(s, mi->is_star);
        bc_put_atom(s, mi->import_name);
        bc_put_leb128(s, mi->req_module_idx);
    }

    bc_put_u8(s, m->has_tla);

    if (JS_WriteObjectRec(s, m->func_obj))
        goto fail;
    return 0;
 fail:
    return -1;
}

/* XXX: be compatible with the structured clone algorithm */
int JS_WriteArray(BCWriterState *s, JSValueConst obj)
{
    JSContext *ctx = s->ctx;
    JSObject *p = JS_VALUE_GET_OBJ(obj);
    uint32_t i, len;
    int ret;
    BOOL is_template;
    JSShapeProperty *prs;
    JSProperty *pr;

    if (s->allow_bytecode && !p->extensible) {
        /* not extensible array: we consider it is a
           template when we are saving bytecode */
        bc_put_u8(s, BC_TAG_TEMPLATE_OBJECT);
        is_template = TRUE;
    } else {
        bc_put_u8(s, BC_TAG_ARRAY);
        is_template = FALSE;
    }
    if (js_get_length32(ctx, &len, obj)) /* no side effect */
        goto fail;
    bc_put_leb128(s, len);
    if (p->fast_array) {
        for(i = 0; i < p->u.array.count; i++) {
            ret = JS_WriteObjectRec(s, p->u.array.u.values[i]);
            if (ret)
                goto fail;
        }
        for(i = p->u.array.count; i < len; i++) {
            ret = JS_WriteObjectRec(s, JS_UNDEFINED);
            if (ret)
                goto fail;
        }
    } else {
        for(i = 0; i < len; i++) {
            JSAtom atom;
            atom = JS_NewAtomUInt32(ctx, i);
            if (atom == JS_ATOM_NULL)
                goto fail;
            prs = find_own_property(&pr, p, atom);
            JS_FreeAtom(ctx, atom);
            if (prs && (prs->flags & JS_PROP_ENUMERABLE)) {
                if (prs->flags & JS_PROP_TMASK) {
                    JS_ThrowTypeError(ctx, "only value properties are supported");
                    goto fail;
                }
                ret = JS_WriteObjectRec(s, pr->u.value);
                if (ret)
                    goto fail;
            } else {
                ret = JS_WriteObjectRec(s, JS_UNDEFINED);
                if (ret)
                    goto fail;
            }
        }
    }
    if (is_template) {
        /* the 'raw' property is not enumerable */
        prs = find_own_property(&pr, p, JS_ATOM_raw);
        if (prs) {
            if (prs->flags & JS_PROP_TMASK) {
                JS_ThrowTypeError(ctx, "only value properties are supported");
                goto fail;
            }
            ret = JS_WriteObjectRec(s, pr->u.value);
            if (ret)
                goto fail;
        } else {
            ret = JS_WriteObjectRec(s, JS_UNDEFINED);
            if (ret)
                goto fail;
        }
    }
    return 0;
 fail:
    return -1;
}

int JS_WriteObjectTag(BCWriterState *s, JSValueConst obj)
{
    JSObject *p = JS_VALUE_GET_OBJ(obj);
    uint32_t i, prop_count;
    JSShape *sh;
    JSShapeProperty *pr;
    int pass;
    JSAtom atom;

    bc_put_u8(s, BC_TAG_OBJECT);
    prop_count = 0;
    sh = p->shape;
    for(pass = 0; pass < 2; pass++) {
        if (pass == 1)
            bc_put_leb128(s, prop_count);
        for(i = 0, pr = get_shape_prop(sh); i < sh->prop_count; i++, pr++) {
            atom = pr->atom;
            if (atom != JS_ATOM_NULL &&
                JS_AtomIsString(s->ctx, atom) &&
                (pr->flags & JS_PROP_ENUMERABLE)) {
                if (pr->flags & JS_PROP_TMASK) {
                    JS_ThrowTypeError(s->ctx, "only value properties are supported");
                    goto fail;
                }
                if (pass == 0) {
                    prop_count++;
                } else {
                    bc_put_atom(s, atom);
                    if (JS_WriteObjectRec(s, p->prop[i].u.value))
                        goto fail;
                }
            }
        }
    }
    return 0;
 fail:
    return -1;
}

int JS_WriteTypedArray(BCWriterState *s, JSValueConst obj)
{
    JSObject *p = JS_VALUE_GET_OBJ(obj);
    JSTypedArray *ta = p->u.typed_array;

    bc_put_u8(s, BC_TAG_TYPED_ARRAY);
    bc_put_u8(s, p->class_id - JS_CLASS_UINT8C_ARRAY);
    bc_put_leb128(s, p->u.array.count);
    bc_put_leb128(s, ta->offset);
    if (JS_WriteObjectRec(s, JS_MKPTR(JS_TAG_OBJECT, ta->buffer)))
        return -1;
    return 0;
}

int JS_WriteArrayBuffer(BCWriterState *s, JSValueConst obj)
{
    JSObject *p = JS_VALUE_GET_OBJ(obj);
    JSArrayBuffer *abuf = p->u.array_buffer;
    if (abuf->detached) {
        JS_ThrowTypeErrorDetachedArrayBuffer(s->ctx);
        return -1;
    }
    bc_put_u8(s, BC_TAG_ARRAY_BUFFER);
    bc_put_leb128(s, abuf->byte_length);
    bc_put_leb128(s, abuf->max_byte_length);
    dbuf_put(&s->dbuf, abuf->data, abuf->byte_length);
    return 0;
}

int JS_WriteSharedArrayBuffer(BCWriterState *s, JSValueConst obj)
{
    JSObject *p = JS_VALUE_GET_OBJ(obj);
    JSArrayBuffer *abuf = p->u.array_buffer;
    assert(!abuf->detached); /* SharedArrayBuffer are never detached */
    bc_put_u8(s, BC_TAG_SHARED_ARRAY_BUFFER);
    bc_put_leb128(s, abuf->byte_length);
    bc_put_leb128(s, abuf->max_byte_length);
    bc_put_u64(s, (uintptr_t)abuf->data);
    if (js_resize_array(s->ctx, (void **)&s->sab_tab, sizeof(s->sab_tab[0]),
                        &s->sab_tab_size, s->sab_tab_len + 1))
        return -1;
    /* keep the SAB pointer so that the user can clone it or free it */
    s->sab_tab[s->sab_tab_len++] = abuf->data;
    return 0;
}

int JS_WriteObjectRec(BCWriterState *s, JSValueConst obj)
{
    uint32_t tag;

    if (js_check_stack_overflow(s->ctx->rt, 0)) {
        JS_ThrowStackOverflow(s->ctx);
        return -1;
    }

    tag = JS_VALUE_GET_NORM_TAG(obj);
    switch(tag) {
    case JS_TAG_NULL:
        bc_put_u8(s, BC_TAG_NULL);
        break;
    case JS_TAG_UNDEFINED:
        bc_put_u8(s, BC_TAG_UNDEFINED);
        break;
    case JS_TAG_BOOL:
        bc_put_u8(s, BC_TAG_BOOL_FALSE + JS_VALUE_GET_INT(obj));
        break;
    case JS_TAG_INT:
        bc_put_u8(s, BC_TAG_INT32);
        bc_put_sleb128(s, JS_VALUE_GET_INT(obj));
        break;
    case JS_TAG_FLOAT64:
        {
            JSFloat64Union u;
            bc_put_u8(s, BC_TAG_FLOAT64);
            u.d = JS_VALUE_GET_FLOAT64(obj);
            bc_put_u64(s, u.u64);
        }
        break;
    case JS_TAG_STRING:
        {
            JSString *p = JS_VALUE_GET_STRING(obj);
            bc_put_u8(s, BC_TAG_STRING);
            JS_WriteString(s, p);
        }
        break;
    case JS_TAG_STRING_ROPE:
        {
            JSValue str;
            int ret;
            str = JS_ToString(s->ctx, obj);
            if (JS_IsException(str))
                goto fail;
            ret = JS_WriteObjectRec(s, str);
            JS_FreeValue(s->ctx, str);
            if (ret)
                goto fail;
        }
        break;
    case JS_TAG_FUNCTION_BYTECODE:
        if (!s->allow_bytecode)
            goto invalid_tag;
        if (JS_WriteFunctionTag(s, obj))
            goto fail;
        break;
    case JS_TAG_MODULE:
        if (!s->allow_bytecode)
            goto invalid_tag;
        if (JS_WriteModule(s, obj))
            goto fail;
        break;
    case JS_TAG_OBJECT:
        {
            JSObject *p = JS_VALUE_GET_OBJ(obj);
            int ret, idx;

            if (s->allow_reference) {
                idx = js_object_list_find(s->ctx, &s->object_list, p);
                if (idx >= 0) {
                    bc_put_u8(s, BC_TAG_OBJECT_REFERENCE);
                    bc_put_leb128(s, idx);
                    break;
                } else {
                    if (js_object_list_add(s->ctx, &s->object_list, p))
                        goto fail;
                }
            } else {
                if (p->tmp_mark) {
                    JS_ThrowTypeError(s->ctx, "circular reference");
                    goto fail;
                }
                p->tmp_mark = 1;
            }
            switch(p->class_id) {
            case JS_CLASS_ARRAY:
                ret = JS_WriteArray(s, obj);
                break;
            case JS_CLASS_OBJECT:
                ret = JS_WriteObjectTag(s, obj);
                break;
            case JS_CLASS_ARRAY_BUFFER:
                ret = JS_WriteArrayBuffer(s, obj);
                break;
            case JS_CLASS_SHARED_ARRAY_BUFFER:
                if (!s->allow_sab)
                    goto invalid_tag;
                ret = JS_WriteSharedArrayBuffer(s, obj);
                break;
            case JS_CLASS_DATE:
                bc_put_u8(s, BC_TAG_DATE);
                ret = JS_WriteObjectRec(s, p->u.object_data);
                break;
            case JS_CLASS_NUMBER:
            case JS_CLASS_STRING:
            case JS_CLASS_BOOLEAN:
            case JS_CLASS_BIG_INT:
                bc_put_u8(s, BC_TAG_OBJECT_VALUE);
                ret = JS_WriteObjectRec(s, p->u.object_data);
                break;
            default:
                if (p->class_id >= JS_CLASS_UINT8C_ARRAY &&
                    p->class_id <= JS_CLASS_FLOAT64_ARRAY) {
                    ret = JS_WriteTypedArray(s, obj);
                } else {
                    JS_ThrowTypeError(s->ctx, "unsupported object class");
                    ret = -1;
                }
                break;
            }
            p->tmp_mark = 0;
            if (ret)
                goto fail;
        }
        break;
    case JS_TAG_SHORT_BIG_INT:
    case JS_TAG_BIG_INT:
        if (JS_WriteBigInt(s, obj))
            goto fail;
        break;
    default:
    invalid_tag:
        JS_ThrowInternalError(s->ctx, "unsupported tag (%d)", tag);
        goto fail;
    }
    return 0;

 fail:
    return -1;
}

/* create the atom table */
int JS_WriteObjectAtoms(BCWriterState *s)
{
    JSRuntime *rt = s->ctx->rt;
    DynBuf dbuf1;
    int i, atoms_size;

    dbuf1 = s->dbuf;
    js_dbuf_init(s->ctx, &s->dbuf);
    bc_put_u8(s, BC_VERSION);

    bc_put_leb128(s, s->idx_to_atom_count);
    for(i = 0; i < s->idx_to_atom_count; i++) {
        JSAtomStruct *p = rt->atom_array[s->idx_to_atom[i]];
        JS_WriteString(s, p);
    }
    /* XXX: should check for OOM in above phase */

    /* move the atoms at the start */
    /* XXX: could just append dbuf1 data, but it uses more memory if
       dbuf1 is larger than dbuf */
    atoms_size = s->dbuf.size;
    if (dbuf_claim(&dbuf1, atoms_size))
        goto fail;
    memmove(dbuf1.buf + atoms_size, dbuf1.buf, dbuf1.size);
    memcpy(dbuf1.buf, s->dbuf.buf, atoms_size);
    dbuf1.size += atoms_size;
    dbuf_free(&s->dbuf);
    s->dbuf = dbuf1;
    return 0;
 fail:
    dbuf_free(&dbuf1);
    return -1;
}

#ifdef DUMP_READ_OBJECT
void __attribute__((format(printf, 2, 3))) bc_read_trace(BCReaderState *s, const char *fmt, ...) {
    va_list ap;
    int i, n, n0;

    if (!s->ptr_last)
        s->ptr_last = s->buf_start;

    n = n0 = 0;
    if (s->ptr > s->ptr_last || s->ptr == s->buf_start) {
        n0 = printf("%04x: ", (int)(s->ptr_last - s->buf_start));
        n += n0;
    }
    for (i = 0; s->ptr_last < s->ptr; i++) {
        if ((i & 7) == 0 && i > 0) {
            printf("\n%*s", n0, "");
            n = n0;
        }
        n += printf(" %02x", *s->ptr_last++);
    }
    if (*fmt == '}')
        s->level--;
    if (n < 32 + s->level * 2) {
        printf("%*s", 32 + s->level * 2 - n, "");
    }
    va_start(ap, fmt);
    vfprintf(stdout, fmt, ap);
    va_end(ap);
    if (strchr(fmt, '{'))
        s->level++;
}
#else
#define bc_read_trace(...)
#endif

int bc_read_error_end(BCReaderState *s)
{
    if (!s->error_state) {
        JS_ThrowSyntaxError(s->ctx, "read after the end of the buffer");
    }
    return s->error_state = -1;
}

int bc_get_u8(BCReaderState *s, uint8_t *pval)
{
    if (unlikely(s->buf_end - s->ptr < 1)) {
        *pval = 0; /* avoid warning */
        return bc_read_error_end(s);
    }
    *pval = *s->ptr++;
    return 0;
}

int bc_get_u16(BCReaderState *s, uint16_t *pval)
{
    uint16_t v;
    if (unlikely(s->buf_end - s->ptr < 2)) {
        *pval = 0; /* avoid warning */
        return bc_read_error_end(s);
    }
    v = get_u16(s->ptr);
    if (is_be())
        v = bswap16(v);
    *pval = v;
    s->ptr += 2;
    return 0;
}

__maybe_unused int bc_get_u32(BCReaderState *s, uint32_t *pval)
{
    uint32_t v;
    if (unlikely(s->buf_end - s->ptr < 4)) {
        *pval = 0; /* avoid warning */
        return bc_read_error_end(s);
    }
    v = get_u32(s->ptr);
    if (is_be())
        v = bswap32(v);
    *pval = v;
    s->ptr += 4;
    return 0;
}

int bc_get_u64(BCReaderState *s, uint64_t *pval)
{
    uint64_t v;
    if (unlikely(s->buf_end - s->ptr < 8)) {
        *pval = 0; /* avoid warning */
        return bc_read_error_end(s);
    }
    v = get_u64(s->ptr);
    if (is_be())
        v = bswap64(v);
    *pval = v;
    s->ptr += 8;
    return 0;
}

int bc_get_leb128(BCReaderState *s, uint32_t *pval)
{
    int ret;
    ret = get_leb128(pval, s->ptr, s->buf_end);
    if (unlikely(ret < 0))
        return bc_read_error_end(s);
    s->ptr += ret;
    return 0;
}

int bc_get_sleb128(BCReaderState *s, int32_t *pval)
{
    int ret;
    ret = get_sleb128(pval, s->ptr, s->buf_end);
    if (unlikely(ret < 0))
        return bc_read_error_end(s);
    s->ptr += ret;
    return 0;
}

/* XXX: used to read an `int` with a positive value */
int bc_get_leb128_int(BCReaderState *s, int *pval)
{
    return bc_get_leb128(s, (uint32_t *)pval);
}

int bc_get_leb128_u16(BCReaderState *s, uint16_t *pval)
{
    uint32_t val;
    if (bc_get_leb128(s, &val)) {
        *pval = 0;
        return -1;
    }
    *pval = val;
    return 0;
}

int bc_get_buf(BCReaderState *s, uint8_t *buf, uint32_t buf_len)
{
    if (buf_len != 0) {
        if (unlikely(!buf || s->buf_end - s->ptr < buf_len))
            return bc_read_error_end(s);
        memcpy(buf, s->ptr, buf_len);
        s->ptr += buf_len;
    }
    return 0;
}

int bc_idx_to_atom(BCReaderState *s, JSAtom *patom, uint32_t idx)
{
    JSAtom atom;

    if (__JS_AtomIsTaggedInt(idx)) {
        atom = idx;
    } else if (idx < s->first_atom) {
        atom = JS_DupAtom(s->ctx, idx);
    } else {
        idx -= s->first_atom;
        if (idx >= s->idx_to_atom_count) {
            JS_ThrowSyntaxError(s->ctx, "invalid atom index (pos=%u)",
                                (unsigned int)(s->ptr - s->buf_start));
            *patom = JS_ATOM_NULL;
            return s->error_state = -1;
        }
        atom = JS_DupAtom(s->ctx, s->idx_to_atom[idx]);
    }
    *patom = atom;
    return 0;
}

int bc_get_atom(BCReaderState *s, JSAtom *patom)
{
    uint32_t v;
    if (bc_get_leb128(s, &v))
        return -1;
    if (v & 1) {
        *patom = __JS_AtomFromUInt32(v >> 1);
        return 0;
    } else {
        return bc_idx_to_atom(s, patom, v >> 1);
    }
}

JSString *JS_ReadString(BCReaderState *s)
{
    uint32_t len;
    size_t size;
    BOOL is_wide_char;
    JSString *p;

    if (bc_get_leb128(s, &len))
        return NULL;
    is_wide_char = len & 1;
    len >>= 1;
    if (len > JS_STRING_LEN_MAX) {
        JS_ThrowInternalError(s->ctx, "string too long");
        return NULL;
    }
    p = js_alloc_string(s->ctx, len, is_wide_char);
    if (!p) {
        s->error_state = -1;
        return NULL;
    }
    size = (size_t)len << is_wide_char;
    if ((s->buf_end - s->ptr) < size) {
        bc_read_error_end(s);
        js_free_string(s->ctx->rt, p);
        return NULL;
    }
    memcpy(p->u.str8, s->ptr, size);
    s->ptr += size;
    if (is_wide_char) {
        if (is_be()) {
            uint32_t i;
            for (i = 0; i < len; i++)
                p->u.str16[i] = bswap16(p->u.str16[i]);
        }
    } else {
        p->u.str8[size] = '\0'; /* add the trailing zero for 8 bit strings */
    }
#ifdef DUMP_READ_OBJECT
    JS_DumpString(s->ctx->rt, p); printf("\n");
#endif
    return p;
}

uint32_t bc_get_flags(uint32_t flags, int *pidx, int n)
{
    uint32_t val;
    /* XXX: this does not work for n == 32 */
    val = (flags >> *pidx) & ((1U << n) - 1);
    *pidx += n;
    return val;
}

int JS_ReadFunctionBytecode(BCReaderState *s, JSFunctionBytecode *b,
                                   int byte_code_offset, uint32_t bc_len)
{
    uint8_t *bc_buf;
    int pos, len, op;
    JSAtom atom;
    uint32_t idx;

    if (s->is_rom_data) {
        /* directly use the input buffer */
        if (unlikely(s->buf_end - s->ptr < bc_len))
            return bc_read_error_end(s);
        bc_buf = (uint8_t *)s->ptr;
        s->ptr += bc_len;
    } else {
        bc_buf = (void *)((uint8_t*)b + byte_code_offset);
        if (bc_get_buf(s, bc_buf, bc_len))
            return -1;
    }
    b->byte_code_buf = bc_buf;

    if (is_be())
        bc_byte_swap(bc_buf, bc_len);

    pos = 0;
    while (pos < bc_len) {
        op = bc_buf[pos];
        len = short_opcode_info(op).size;
        switch(short_opcode_info(op).fmt) {
        case OP_FMT_atom:
        case OP_FMT_atom_u8:
        case OP_FMT_atom_u16:
        case OP_FMT_atom_label_u8:
        case OP_FMT_atom_label_u16:
            idx = get_u32(bc_buf + pos + 1);
            if (s->is_rom_data) {
                /* just increment the reference count of the atom */
                JS_DupAtom(s->ctx, (JSAtom)idx);
            } else {
                if (bc_idx_to_atom(s, &atom, idx)) {
                    /* Note: the atoms will be freed up to this position */
                    b->byte_code_len = pos;
                    return -1;
                }
                put_u32(bc_buf + pos + 1, atom);
#ifdef DUMP_READ_OBJECT
                bc_read_trace(s, "at %d, fixup atom: ", pos + 1); print_atom(s->ctx, atom); printf("\n");
#endif
            }
            break;
        default:
            break;
        }
        pos += len;
    }
    return 0;
}

JSValue JS_ReadBigInt(BCReaderState *s)
{
    JSValue obj = JS_UNDEFINED;
    uint32_t len, i, n;
    JSBigInt *p;
    js_limb_t v;
    uint8_t v8;

    if (bc_get_leb128(s, &len))
        goto fail;
    bc_read_trace(s, "len=%" PRId64 "\n", (int64_t)len);
    if (len == 0) {
        /* zero case */
        bc_read_trace(s, "}\n");
        return __JS_NewShortBigInt(s->ctx, 0);
    }
    p = js_bigint_new(s->ctx, (len - 1) / (JS_LIMB_BITS / 8) + 1);
    if (!p)
        goto fail;
    for(i = 0; i < len / (JS_LIMB_BITS / 8); i++) {
#if JS_LIMB_BITS == 32
        if (bc_get_u32(s, &v))
            goto fail;
#else
        if (bc_get_u64(s, &v))
            goto fail;
#endif
        p->tab[i] = v;
    }
    n = len % (JS_LIMB_BITS / 8);
    if (n != 0) {
        int shift;
        v = 0;
        for(i = 0; i < n; i++) {
            if (bc_get_u8(s, &v8))
                goto fail;
            v |= (js_limb_t)v8 << (i * 8);
        }
        shift = JS_LIMB_BITS - n * 8;
        /* extend the sign */
        if (shift != 0) {
            v = (js_slimb_t)(v << shift) >> shift;
        }
        p->tab[p->len - 1] = v;
    }
    bc_read_trace(s, "}\n");
    return JS_CompactBigInt(s->ctx, p);
 fail:
    JS_FreeValue(s->ctx, obj);
    return JS_EXCEPTION;
}

JSValue JS_ReadObjectRec(BCReaderState *s);

int BC_add_object_ref1(BCReaderState *s, JSObject *p)
{
    if (s->allow_reference) {
        if (js_resize_array(s->ctx, (void *)&s->objects,
                            sizeof(s->objects[0]),
                            &s->objects_size, s->objects_count + 1))
            return -1;
        s->objects[s->objects_count++] = p;
    }
    return 0;
}

int BC_add_object_ref(BCReaderState *s, JSValueConst obj)
{
    return BC_add_object_ref1(s, JS_VALUE_GET_OBJ(obj));
}

JSValue JS_ReadFunctionTag(BCReaderState *s)
{
    JSContext *ctx = s->ctx;
    JSFunctionBytecode bc, *b;
    JSValue obj = JS_UNDEFINED;
    uint16_t v16;
    uint8_t v8;
    int idx, i, local_count;
    int cpool_offset, byte_code_offset;
    int closure_var_offset, vardefs_offset;
    uint64_t function_size;

    memset(&bc, 0, sizeof(bc));

    if (bc_get_u16(s, &v16))
        goto fail;
    idx = 0;
    bc.has_prototype = bc_get_flags(v16, &idx, 1);
    bc.has_simple_parameter_list = bc_get_flags(v16, &idx, 1);
    bc.is_derived_class_constructor = bc_get_flags(v16, &idx, 1);
    bc.need_home_object = bc_get_flags(v16, &idx, 1);
    bc.func_kind = bc_get_flags(v16, &idx, 2);
    bc.new_target_allowed = bc_get_flags(v16, &idx, 1);
    bc.super_call_allowed = bc_get_flags(v16, &idx, 1);
    bc.super_allowed = bc_get_flags(v16, &idx, 1);
    bc.arguments_allowed = bc_get_flags(v16, &idx, 1);
    bc.has_debug = bc_get_flags(v16, &idx, 1);
    bc.is_direct_or_indirect_eval = bc_get_flags(v16, &idx, 1);
    bc.read_only_bytecode = s->is_rom_data;
    if (bc_get_u8(s, &v8))
        goto fail;
    bc.js_mode = v8;
    if (bc_get_atom(s, &bc.func_name))  //@ atom leak if failure
        goto fail;
    if (bc_get_leb128_u16(s, &bc.arg_count))
        goto fail;
    if (bc_get_leb128_u16(s, &bc.var_count))
        goto fail;
    if (bc_get_leb128_u16(s, &bc.defined_arg_count))
        goto fail;
    if (bc_get_leb128_u16(s, &bc.stack_size))
        goto fail;
    if (bc_get_leb128_u16(s, &bc.var_ref_count))
        goto fail;
    if (bc_get_leb128_int(s, (int*)&bc.closure_var_count))
        goto fail;
    if (bc_get_leb128_int(s, (int*)&bc.cpool_count))
        goto fail;
    if (bc_get_leb128_int(s, (int*)&bc.byte_code_len))
        goto fail;
    if (bc_get_leb128_int(s, &local_count))
        goto fail;

    if (bc.has_debug) {
        function_size = sizeof(*b);
    } else {
        function_size = offsetof(JSFunctionBytecode, debug);
    }
    cpool_offset = function_size;
    function_size += (uint64_t)bc.cpool_count * sizeof(*bc.cpool);
    vardefs_offset = function_size;
    function_size += (uint64_t)local_count * sizeof(*bc.vardefs);
    closure_var_offset = function_size;
    function_size += (uint64_t)bc.closure_var_count * sizeof(*bc.closure_var);
    byte_code_offset = function_size;
    if (!bc.read_only_bytecode) {
        function_size += bc.byte_code_len;
    }

    if (function_size > INT32_MAX)
        return JS_ThrowOutOfMemory(ctx);

    b = js_mallocz(ctx, function_size);
    if (!b)
        return JS_EXCEPTION;

    memcpy(b, &bc, offsetof(JSFunctionBytecode, debug));
    if (local_count != 0) {
        b->vardefs = (void *)((uint8_t*)b + vardefs_offset);
    }
    if (b->closure_var_count != 0) {
        b->closure_var = (void *)((uint8_t*)b + closure_var_offset);
    }
    if (b->cpool_count != 0) {
        b->cpool = (void *)((uint8_t*)b + cpool_offset);
    }

    js_rc(b)->ref_count = 1;
    add_gc_object(ctx->rt, &b->header, JS_GC_OBJ_TYPE_FUNCTION_BYTECODE);

    obj = JS_MKPTR(JS_TAG_FUNCTION_BYTECODE, b);

#ifdef DUMP_READ_OBJECT
    bc_read_trace(s, "name: "); print_atom(s->ctx, b->func_name); printf("\n");
#endif
    bc_read_trace(s, "args=%d vars=%d defargs=%d closures=%d cpool=%d\n",
                  b->arg_count, b->var_count, b->defined_arg_count,
                  b->closure_var_count, b->cpool_count);
    bc_read_trace(s, "stack=%d bclen=%d locals=%d\n",
                  b->stack_size, b->byte_code_len, local_count);

    if (local_count != 0) {
        bc_read_trace(s, "vars {\n");
        for(i = 0; i < local_count; i++) {
            JSBytecodeVarDef *vd = &b->vardefs[i];
            if (bc_get_atom(s, &vd->var_name))
                goto fail;
            if (bc_get_leb128_int(s, &vd->scope_next))
                goto fail;
            vd->scope_next--;
            if (bc_get_leb128_u16(s, &vd->var_ref_idx))
                goto fail;
            if (bc_get_u8(s, &v8))
                goto fail;
            idx = 0;
            vd->var_kind = bc_get_flags(v8, &idx, 4);
            vd->is_const = bc_get_flags(v8, &idx, 1);
            vd->is_lexical = bc_get_flags(v8, &idx, 1);
            vd->is_captured = bc_get_flags(v8, &idx, 1);
            vd->has_scope = bc_get_flags(v8, &idx, 1);
#ifdef DUMP_READ_OBJECT
            bc_read_trace(s, "name: "); print_atom(s->ctx, vd->var_name); printf("\n");
#endif
        }
        bc_read_trace(s, "}\n");
    }
    if (b->closure_var_count != 0) {
        bc_read_trace(s, "closure vars {\n");
        for(i = 0; i < b->closure_var_count; i++) {
            JSClosureVar *cv = &b->closure_var[i];
            int var_idx;
            if (bc_get_atom(s, &cv->var_name))
                goto fail;
            if (bc_get_leb128_int(s, &var_idx))
                goto fail;
            cv->var_idx = var_idx;
            if (bc_get_u16(s, &v16))
                goto fail;
            idx = 0;
            cv->closure_type = bc_get_flags(v16, &idx, 3);
            cv->is_const = bc_get_flags(v16, &idx, 1);
            cv->is_lexical = bc_get_flags(v16, &idx, 1);
            cv->var_kind = bc_get_flags(v16, &idx, 4);
#ifdef DUMP_READ_OBJECT
            bc_read_trace(s, "name: "); print_atom(s->ctx, cv->var_name); printf("\n");
#endif
        }
        bc_read_trace(s, "}\n");
    }
    {
        bc_read_trace(s, "bytecode {\n");
        if (JS_ReadFunctionBytecode(s, b, byte_code_offset, b->byte_code_len))
            goto fail;
        bc_read_trace(s, "}\n");
    }
    if (b->has_debug) {
        /* read optional debug information */
        bc_read_trace(s, "debug {\n");
        if (bc_get_atom(s, &b->debug.filename))
            goto fail;
#ifdef DUMP_READ_OBJECT
        bc_read_trace(s, "filename: "); print_atom(s->ctx, b->debug.filename); printf("\n");
#endif
        if (bc_get_leb128_int(s, (int*)&b->debug.pc2line_len))
            goto fail;
        if (b->debug.pc2line_len) {
            b->debug.pc2line_buf = js_mallocz(ctx, b->debug.pc2line_len);
            if (!b->debug.pc2line_buf)
                goto fail;
            if (bc_get_buf(s, b->debug.pc2line_buf, b->debug.pc2line_len))
                goto fail;
        }
        if (bc_get_leb128_int(s, (int*)&b->debug.source_len))
            goto fail;
        if (b->debug.source_len) {
            bc_read_trace(s, "source: %d bytes\n", b->source_len);
            b->debug.source = js_mallocz(ctx, b->debug.source_len);
            if (!b->debug.source)
                goto fail;
            if (bc_get_buf(s, (uint8_t *)b->debug.source, b->debug.source_len))
                goto fail;
        }
        bc_read_trace(s, "}\n");
    }
    if (b->cpool_count != 0) {
        bc_read_trace(s, "cpool {\n");
        for(i = 0; i < b->cpool_count; i++) {
            JSValue val;
            val = JS_ReadObjectRec(s);
            if (JS_IsException(val))
                goto fail;
            b->cpool[i] = val;
        }
        bc_read_trace(s, "}\n");
    }
    b->realm = JS_DupContext(ctx);
    return obj;
 fail:
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

JSValue JS_ReadModule(BCReaderState *s)
{
    JSContext *ctx = s->ctx;
    JSValue obj;
    JSModuleDef *m = NULL;
    JSAtom module_name;
    int i;
    uint8_t v8;

    if (bc_get_atom(s, &module_name))
        goto fail;
#ifdef DUMP_READ_OBJECT
    bc_read_trace(s, "name: "); print_atom(s->ctx, module_name); printf("\n");
#endif
    m = js_new_module_def(ctx, module_name);
    if (!m)
        goto fail;
    obj = JS_NewModuleValue(ctx, m);
    if (bc_get_leb128_int(s, &m->req_module_entries_count))
        goto fail;
    if (m->req_module_entries_count != 0) {
        m->req_module_entries_size = m->req_module_entries_count;
        m->req_module_entries = js_mallocz(ctx, sizeof(m->req_module_entries[0]) * m->req_module_entries_size);
        if (!m->req_module_entries)
            goto fail;
        for(i = 0; i < m->req_module_entries_count; i++) {
            JSReqModuleEntry *rme = &m->req_module_entries[i];
            JSValue val;
            if (bc_get_atom(s, &rme->module_name))
                goto fail;
            val = JS_ReadObjectRec(s);
            if (JS_IsException(val))
                goto fail;
            rme->attributes = val;
        }
    }

    if (bc_get_leb128_int(s, &m->export_entries_count))
        goto fail;
    if (m->export_entries_count != 0) {
        m->export_entries_size = m->export_entries_count;
        m->export_entries = js_mallocz(ctx, sizeof(m->export_entries[0]) * m->export_entries_size);
        if (!m->export_entries)
            goto fail;
        for(i = 0; i < m->export_entries_count; i++) {
            JSExportEntry *me = &m->export_entries[i];
            if (bc_get_u8(s, &v8))
                goto fail;
            me->export_type = v8;
            if (me->export_type == JS_EXPORT_TYPE_LOCAL) {
                if (bc_get_leb128_int(s, &me->u.local.var_idx))
                    goto fail;
            } else {
                if (bc_get_leb128_int(s, &me->u.req_module_idx))
                    goto fail;
                if (bc_get_atom(s, &me->local_name))
                    goto fail;
            }
            if (bc_get_atom(s, &me->export_name))
                goto fail;
        }
    }

    if (bc_get_leb128_int(s, &m->star_export_entries_count))
        goto fail;
    if (m->star_export_entries_count != 0) {
        m->star_export_entries_size = m->star_export_entries_count;
        m->star_export_entries = js_mallocz(ctx, sizeof(m->star_export_entries[0]) * m->star_export_entries_size);
        if (!m->star_export_entries)
            goto fail;
        for(i = 0; i < m->star_export_entries_count; i++) {
            JSStarExportEntry *se = &m->star_export_entries[i];
            if (bc_get_leb128_int(s, &se->req_module_idx))
                goto fail;
        }
    }

    if (bc_get_leb128_int(s, &m->import_entries_count))
        goto fail;
    if (m->import_entries_count != 0) {
        m->import_entries_size = m->import_entries_count;
        m->import_entries = js_mallocz(ctx, sizeof(m->import_entries[0]) * m->import_entries_size);
        if (!m->import_entries)
            goto fail;
        for(i = 0; i < m->import_entries_count; i++) {
            JSImportEntry *mi = &m->import_entries[i];
            uint8_t v8;
            if (bc_get_leb128_int(s, &mi->var_idx))
                goto fail;
            if (bc_get_u8(s, &v8))
                goto fail;
            mi->is_star = (v8 != 0);
            if (bc_get_atom(s, &mi->import_name))
                goto fail;
            if (bc_get_leb128_int(s, &mi->req_module_idx))
                goto fail;
        }
    }

    if (bc_get_u8(s, &v8))
        goto fail;
    m->has_tla = (v8 != 0);

    m->func_obj = JS_ReadObjectRec(s);
    if (JS_IsException(m->func_obj))
        goto fail;
    return obj;
 fail:
    if (m) {
        JS_FreeValue(ctx, JS_MKPTR(JS_TAG_MODULE, m));
    }
    return JS_EXCEPTION;
}

JSValue JS_ReadObjectTag(BCReaderState *s)
{
    JSContext *ctx = s->ctx;
    JSValue obj;
    uint32_t prop_count, i;
    JSAtom atom;
    JSValue val;
    int ret;

    obj = JS_NewObject(ctx);
    if (BC_add_object_ref(s, obj))
        goto fail;
    if (bc_get_leb128(s, &prop_count))
        goto fail;
    for(i = 0; i < prop_count; i++) {
        if (bc_get_atom(s, &atom))
            goto fail;
#ifdef DUMP_READ_OBJECT
        bc_read_trace(s, "propname: "); print_atom(s->ctx, atom); printf("\n");
#endif
        val = JS_ReadObjectRec(s);
        if (JS_IsException(val)) {
            JS_FreeAtom(ctx, atom);
            goto fail;
        }
        ret = JS_DefinePropertyValue(ctx, obj, atom, val, JS_PROP_C_W_E);
        JS_FreeAtom(ctx, atom);
        if (ret < 0)
            goto fail;
    }
    return obj;
 fail:
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

JSValue JS_ReadArray(BCReaderState *s, int tag)
{
    JSContext *ctx = s->ctx;
    JSValue obj;
    uint32_t len, i;
    JSValue val;
    int ret, prop_flags;
    BOOL is_template;

    obj = JS_NewArray(ctx);
    if (BC_add_object_ref(s, obj))
        goto fail;
    is_template = (tag == BC_TAG_TEMPLATE_OBJECT);
    if (bc_get_leb128(s, &len))
        goto fail;
    for(i = 0; i < len; i++) {
        val = JS_ReadObjectRec(s);
        if (JS_IsException(val))
            goto fail;
        if (is_template)
            prop_flags = JS_PROP_ENUMERABLE;
        else
            prop_flags = JS_PROP_C_W_E;
        ret = JS_DefinePropertyValueUint32(ctx, obj, i, val,
                                           prop_flags);
        if (ret < 0)
            goto fail;
    }
    if (is_template) {
        val = JS_ReadObjectRec(s);
        if (JS_IsException(val))
            goto fail;
        if (!JS_IsUndefined(val)) {
            ret = JS_DefinePropertyValue(ctx, obj, JS_ATOM_raw, val, 0);
            if (ret < 0)
                goto fail;
        }
        JS_PreventExtensions(ctx, obj);
    }
    return obj;
 fail:
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

JSValue JS_ReadTypedArray(BCReaderState *s)
{
    JSContext *ctx = s->ctx;
    JSValue obj = JS_UNDEFINED, array_buffer = JS_UNDEFINED;
    uint8_t array_tag;
    JSValueConst args[3];
    uint32_t offset, len, idx;

    if (bc_get_u8(s, &array_tag))
        return JS_EXCEPTION;
    if (array_tag >= JS_TYPED_ARRAY_COUNT)
        return JS_ThrowTypeError(ctx, "invalid typed array");
    if (bc_get_leb128(s, &len))
        return JS_EXCEPTION;
    if (bc_get_leb128(s, &offset))
        return JS_EXCEPTION;
    /* XXX: this hack could be avoided if the typed array could be
       created before the array buffer */
    idx = s->objects_count;
    if (BC_add_object_ref1(s, NULL))
        goto fail;
    array_buffer = JS_ReadObjectRec(s);
    if (JS_IsException(array_buffer))
        return JS_EXCEPTION;
    if (!js_get_array_buffer(ctx, array_buffer)) {
        JS_FreeValue(ctx, array_buffer);
        return JS_EXCEPTION;
    }
    args[0] = array_buffer;
    args[1] = JS_NewInt64(ctx, offset);
    args[2] = JS_NewInt64(ctx, len);
    obj = js_typed_array_constructor(ctx, JS_UNDEFINED,
                                     3, args,
                                     JS_CLASS_UINT8C_ARRAY + array_tag);
    if (JS_IsException(obj))
        goto fail;
    if (s->allow_reference) {
        s->objects[idx] = JS_VALUE_GET_OBJ(obj);
    }
    JS_FreeValue(ctx, array_buffer);
    return obj;
 fail:
    JS_FreeValue(ctx, array_buffer);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

JSValue JS_ReadArrayBuffer(BCReaderState *s)
{
    JSContext *ctx = s->ctx;
    uint32_t byte_length, max_byte_length;
    uint64_t max_byte_length_u64, *pmax_byte_length = NULL;
    JSValue obj;

    if (bc_get_leb128(s, &byte_length))
        return JS_EXCEPTION;
    if (bc_get_leb128(s, &max_byte_length))
        return JS_EXCEPTION;
    if (max_byte_length < byte_length)
        return JS_ThrowTypeError(ctx, "invalid array buffer");
    if (max_byte_length != UINT32_MAX) {
        max_byte_length_u64 = max_byte_length;
        pmax_byte_length = &max_byte_length_u64;
    }
    if (unlikely(s->buf_end - s->ptr < byte_length)) {
        bc_read_error_end(s);
        return JS_EXCEPTION;
    }
    // makes a copy of the input
    obj = js_array_buffer_constructor3(ctx, JS_UNDEFINED,
                                       byte_length, pmax_byte_length,
                                       JS_CLASS_ARRAY_BUFFER,
                                       (uint8_t*)s->ptr,
                                       js_array_buffer_free, NULL,
                                       /*alloc_flag*/TRUE);
    if (JS_IsException(obj))
        goto fail;
    if (BC_add_object_ref(s, obj))
        goto fail;
    s->ptr += byte_length;
    return obj;
 fail:
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

JSValue JS_ReadSharedArrayBuffer(BCReaderState *s)
{
    JSContext *ctx = s->ctx;
    uint32_t byte_length, max_byte_length;
    uint64_t max_byte_length_u64, *pmax_byte_length = NULL;
    uint8_t *data_ptr;
    JSValue obj;
    uint64_t u64;

    if (bc_get_leb128(s, &byte_length))
        return JS_EXCEPTION;
    if (bc_get_leb128(s, &max_byte_length))
        return JS_EXCEPTION;
    if (max_byte_length < byte_length)
        return JS_ThrowTypeError(ctx, "invalid array buffer");
    if (max_byte_length != UINT32_MAX) {
        max_byte_length_u64 = max_byte_length;
        pmax_byte_length = &max_byte_length_u64;
    }
    if (bc_get_u64(s, &u64))
        return JS_EXCEPTION;
    data_ptr = (uint8_t *)(uintptr_t)u64;
    /* the SharedArrayBuffer is cloned */
    obj = js_array_buffer_constructor3(ctx, JS_UNDEFINED,
                                       byte_length, pmax_byte_length,
                                       JS_CLASS_SHARED_ARRAY_BUFFER,
                                       data_ptr,
                                       NULL, NULL, FALSE);
    if (JS_IsException(obj))
        goto fail;
    if (BC_add_object_ref(s, obj))
        goto fail;
    return obj;
 fail:
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

JSValue JS_ReadDate(BCReaderState *s)
{
    JSContext *ctx = s->ctx;
    JSValue val, obj = JS_UNDEFINED;

    val = JS_ReadObjectRec(s);
    if (JS_IsException(val))
        goto fail;
    if (!JS_IsNumber(val)) {
        JS_ThrowTypeError(ctx, "Number tag expected for date");
        goto fail;
    }
    obj = JS_NewObjectProtoClass(ctx, ctx->class_proto[JS_CLASS_DATE],
                                 JS_CLASS_DATE);
    if (JS_IsException(obj))
        goto fail;
    if (BC_add_object_ref(s, obj))
        goto fail;
    JS_SetObjectData(ctx, obj, val);
    return obj;
 fail:
    JS_FreeValue(ctx, val);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

JSValue JS_ReadObjectValue(BCReaderState *s)
{
    JSContext *ctx = s->ctx;
    JSValue val, obj = JS_UNDEFINED;

    val = JS_ReadObjectRec(s);
    if (JS_IsException(val))
        goto fail;
    obj = JS_ToObject(ctx, val);
    if (JS_IsException(obj))
        goto fail;
    if (BC_add_object_ref(s, obj))
        goto fail;
    JS_FreeValue(ctx, val);
    return obj;
 fail:
    JS_FreeValue(ctx, val);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

JSValue JS_ReadObjectRec(BCReaderState *s)
{
    JSContext *ctx = s->ctx;
    uint8_t tag;
    JSValue obj = JS_UNDEFINED;

    if (js_check_stack_overflow(ctx->rt, 0))
        return JS_ThrowStackOverflow(ctx);

    if (bc_get_u8(s, &tag))
        return JS_EXCEPTION;

    bc_read_trace(s, "%s {\n", bc_tag_str[tag]);

    switch(tag) {
    case BC_TAG_NULL:
        obj = JS_NULL;
        break;
    case BC_TAG_UNDEFINED:
        obj = JS_UNDEFINED;
        break;
    case BC_TAG_BOOL_FALSE:
    case BC_TAG_BOOL_TRUE:
        obj = JS_NewBool(ctx, tag - BC_TAG_BOOL_FALSE);
        break;
    case BC_TAG_INT32:
        {
            int32_t val;
            if (bc_get_sleb128(s, &val))
                return JS_EXCEPTION;
            bc_read_trace(s, "%d\n", val);
            obj = JS_NewInt32(ctx, val);
        }
        break;
    case BC_TAG_FLOAT64:
        {
            JSFloat64Union u;
            if (bc_get_u64(s, &u.u64))
                return JS_EXCEPTION;
            bc_read_trace(s, "%g\n", u.d);
            obj = __JS_NewFloat64(ctx, u.d);
        }
        break;
    case BC_TAG_STRING:
        {
            JSString *p;
            p = JS_ReadString(s);
            if (!p)
                return JS_EXCEPTION;
            obj = JS_MKPTR(JS_TAG_STRING, p);
        }
        break;
    case BC_TAG_FUNCTION_BYTECODE:
        if (!s->allow_bytecode)
            goto invalid_tag;
        obj = JS_ReadFunctionTag(s);
        break;
    case BC_TAG_MODULE:
        if (!s->allow_bytecode)
            goto invalid_tag;
        obj = JS_ReadModule(s);
        break;
    case BC_TAG_OBJECT:
        obj = JS_ReadObjectTag(s);
        break;
    case BC_TAG_ARRAY:
    case BC_TAG_TEMPLATE_OBJECT:
        obj = JS_ReadArray(s, tag);
        break;
    case BC_TAG_TYPED_ARRAY:
        obj = JS_ReadTypedArray(s);
        break;
    case BC_TAG_ARRAY_BUFFER:
        obj = JS_ReadArrayBuffer(s);
        break;
    case BC_TAG_SHARED_ARRAY_BUFFER:
        if (!s->allow_sab || !ctx->rt->sab_funcs.sab_dup)
            goto invalid_tag;
        obj = JS_ReadSharedArrayBuffer(s);
        break;
    case BC_TAG_DATE:
        obj = JS_ReadDate(s);
        break;
    case BC_TAG_OBJECT_VALUE:
        obj = JS_ReadObjectValue(s);
        break;
    case BC_TAG_BIG_INT:
        obj = JS_ReadBigInt(s);
        break;
    case BC_TAG_OBJECT_REFERENCE:
        {
            uint32_t val;
            if (!s->allow_reference)
                return JS_ThrowSyntaxError(ctx, "object references are not allowed");
            if (bc_get_leb128(s, &val))
                return JS_EXCEPTION;
            bc_read_trace(s, "%u\n", val);
            if (val >= s->objects_count) {
                return JS_ThrowSyntaxError(ctx, "invalid object reference (%u >= %u)",
                                           val, s->objects_count);
            }
            obj = JS_DupValue(ctx, JS_MKPTR(JS_TAG_OBJECT, s->objects[val]));
        }
        break;
    default:
    invalid_tag:
        return JS_ThrowSyntaxError(ctx, "invalid tag (tag=%d pos=%u)",
                                   tag, (unsigned int)(s->ptr - s->buf_start));
    }
    bc_read_trace(s, "}\n");
    return obj;
}

int JS_ReadObjectAtoms(BCReaderState *s)
{
    uint8_t v8;
    JSString *p;
    int i;
    JSAtom atom;

    if (bc_get_u8(s, &v8))
        return -1;
    if (v8 != BC_VERSION) {
        JS_ThrowSyntaxError(s->ctx, "invalid version (%d expected=%d)",
                            v8, BC_VERSION);
        return -1;
    }
    if (bc_get_leb128(s, &s->idx_to_atom_count))
        return -1;

    bc_read_trace(s, "%d atom indexes {\n", s->idx_to_atom_count);

    if (s->idx_to_atom_count != 0) {
        s->idx_to_atom = js_mallocz(s->ctx, s->idx_to_atom_count *
                                    sizeof(s->idx_to_atom[0]));
        if (!s->idx_to_atom)
            return s->error_state = -1;
    }
    for(i = 0; i < s->idx_to_atom_count; i++) {
        p = JS_ReadString(s);
        if (!p)
            return -1;
        atom = JS_NewAtomStr(s->ctx, p);
        if (atom == JS_ATOM_NULL)
            return s->error_state = -1;
        s->idx_to_atom[i] = atom;
        if (s->is_rom_data && (atom != (i + s->first_atom)))
            s->is_rom_data = FALSE; /* atoms must be relocated */
    }
    bc_read_trace(s, "}\n");
    return 0;
}

void bc_reader_free(BCReaderState *s)
{
    int i;
    if (s->idx_to_atom) {
        for(i = 0; i < s->idx_to_atom_count; i++) {
            JS_FreeAtom(s->ctx, s->idx_to_atom[i]);
        }
        js_free(s->ctx, s->idx_to_atom);
    }
    js_free(s->ctx, s->objects);
}

/*******************************************************************/
/* runtime functions & objects */

JSValue js_string_constructor(JSContext *ctx, JSValueConst this_val,
                                     int argc, JSValueConst *argv);
JSValue js_boolean_constructor(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv);
JSValue js_number_constructor(JSContext *ctx, JSValueConst this_val,
                                     int argc, JSValueConst *argv);

int check_function(JSContext *ctx, JSValueConst obj)
{
    if (likely(JS_IsFunction(ctx, obj)))
        return 0;
    JS_ThrowTypeError(ctx, "not a function");
    return -1;
}

int check_exception_free(JSContext *ctx, JSValue obj)
{
    JS_FreeValue(ctx, obj);
    return JS_IsException(obj);
}

JSAtom find_atom(JSContext *ctx, const char *name)
{
    JSAtom atom;
    int len;

    if (*name == '[') {
        name++;
        len = strlen(name) - 1;
        /* We assume 8 bit non null strings, which is the case for these
           symbols */
        for(atom = JS_ATOM_Symbol_toPrimitive; atom < JS_ATOM_END; atom++) {
            JSAtomStruct *p = ctx->rt->atom_array[atom];
            JSString *str = p;
            if (str->len == len && !memcmp(str->u.str8, name, len))
                return JS_DupAtom(ctx, atom);
        }
        abort();
    } else {
        atom = JS_NewAtom(ctx, name);
    }
    return atom;
}

JSValue JS_NewObjectProtoList(JSContext *ctx, JSValueConst proto,
                              const JSCFunctionListEntry *fields, int n_fields)
{
    JSValue obj;
    obj = JS_NewObjectProtoClassAlloc(ctx, proto, JS_CLASS_OBJECT, n_fields);
    JS_LOG("JS_NewObjectProtoList", "Alloc returned obj=%08lX_%08lX", U64_HI(obj), U64_LO(obj));
    if (JS_IsException(obj)){
                JS_LOG("JS_NewObjectProtoList", "JS_IsException(obj) is true");
        return obj;
        }
    if (JS_SetPropertyFunctionList(ctx, obj, fields, n_fields)) {
        JS_LOG("JS_NewObjectProtoList", "SetPropertyFunctionList failed, freeing obj");
        JS_FreeValue(ctx, obj);
        return JS_EXCEPTION;
    }
    JS_LOG("JS_NewObjectProtoList", "Success");
    return obj;
}

JSValue JS_InstantiateFunctionListItem2(JSContext *ctx, JSObject *p,
                                               JSAtom atom, void *opaque)
{
    const JSCFunctionListEntry *e = opaque;
    JSValue val, proto;

    switch(e->def_type) {
    case JS_DEF_CFUNC:
        val = JS_NewCFunction2(ctx, e->u.func.cfunc.generic,
                               e->name, e->u.func.length, e->u.func.cproto, e->magic);
        break;
    case JS_DEF_PROP_STRING:
        val = JS_NewAtomString(ctx, e->u.str);
        break;
    case JS_DEF_OBJECT:
        /* XXX: could add a flag */
        if (atom == JS_ATOM_Symbol_unscopables)
            proto = JS_NULL;
        else
            proto = ctx->class_proto[JS_CLASS_OBJECT];
        val = JS_NewObjectProtoList(ctx, proto,
                                    e->u.prop_list.tab, e->u.prop_list.len);
        break;
    default:
        abort();
    }
    return val;
}

int JS_InstantiateFunctionListItem(JSContext *ctx, JSValueConst obj,
                                          JSAtom atom,
                                          const JSCFunctionListEntry *e)
{
    JSValue val;
    int prop_flags = e->prop_flags;

    JS_LOG("JS_InstantiateFunctionListItem", "Entered: name=%s, def_type=%d, atom=%lu, prop_flags=0x%X",
           e->name, e->def_type, (unsigned long)atom, prop_flags);

    switch(e->def_type) {
    case JS_DEF_ALIAS: /* using autoinit for aliases is not safe */
        {
            JSAtom atom1;
            JS_LOG("JS_InstantiateFunctionListItem", "JS_DEF_ALIAS: alias.name=%s, alias.base=%d",
                   e->u.alias.name, e->u.alias.base);
            atom1 = find_atom(ctx, e->u.alias.name);
            if (atom1 == JS_ATOM_NULL) {
                JS_LOG("JS_InstantiateFunctionListItem", "find_atom for alias failed");
                return -1;
            }
            JS_LOG("JS_InstantiateFunctionListItem", "Calling JS_GetProperty for alias");
            switch (e->u.alias.base) {
            case -1:
                val = JS_GetProperty(ctx, obj, atom1);
                break;
            case 0:
                val = JS_GetProperty(ctx, ctx->global_obj, atom1);
                break;
            case 1:
                val = JS_GetProperty(ctx, ctx->class_proto[JS_CLASS_ARRAY], atom1);
                break;
            default:
                abort();
            }
            JS_FreeAtom(ctx, atom1);
            if (JS_IsException(val)) {
                JS_LOG("JS_InstantiateFunctionListItem", "JS_GetProperty for alias returned exception");
                return -1;
            }
            if (atom == JS_ATOM_Symbol_toPrimitive) {
                prop_flags = JS_PROP_CONFIGURABLE;
            } else if (atom == JS_ATOM_Symbol_hasInstance) {
                prop_flags = 0;
            }
        }
        break;
    case JS_DEF_CFUNC:
        JS_LOG("JS_InstantiateFunctionListItem", "JS_DEF_CFUNC");
        if (atom == JS_ATOM_Symbol_toPrimitive) {
            prop_flags = JS_PROP_CONFIGURABLE;
        } else if (atom == JS_ATOM_Symbol_hasInstance) {
            prop_flags = 0;
        }
        if (JS_DefineAutoInitProperty(ctx, obj, atom, JS_AUTOINIT_ID_PROP,
                                      (void *)e, prop_flags) < 0) {
            JS_LOG("JS_InstantiateFunctionListItem", "JS_DefineAutoInitProperty failed");
            return -1;
        }
        return 0;
    case JS_DEF_CGETSET: /* XXX: use autoinit again ? */
    case JS_DEF_CGETSET_MAGIC:
        {
            JSValue getter, setter;
            char buf[64];

            JS_LOG("JS_InstantiateFunctionListItem", "JS_DEF_CGETSET/MAGIC");
            getter = JS_UNDEFINED;
            if (e->u.getset.get.generic) {
                snprintf(buf, sizeof(buf), "get %s", e->name);
                getter = JS_NewCFunction2(ctx, e->u.getset.get.generic,
                                          buf, 0, e->def_type == JS_DEF_CGETSET_MAGIC ? JS_CFUNC_getter_magic : JS_CFUNC_getter,
                                          e->magic);
                if (JS_IsException(getter))
                    return -1;
            }
            setter = JS_UNDEFINED;
            if (e->u.getset.set.generic) {
                snprintf(buf, sizeof(buf), "set %s", e->name);
                setter = JS_NewCFunction2(ctx, e->u.getset.set.generic,
                                          buf, 1, e->def_type == JS_DEF_CGETSET_MAGIC ? JS_CFUNC_setter_magic : JS_CFUNC_setter,
                                          e->magic);
                if (JS_IsException(setter)) {
                    JS_FreeValue(ctx, getter);
                    return -1;
                }
            }
            if (JS_DefinePropertyGetSet(ctx, obj, atom, getter, setter, prop_flags) < 0)
                return -1;
            return 0;
        }
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
    case JS_DEF_PROP_UNDEFINED:
        val = JS_UNDEFINED;
        break;
    case JS_DEF_PROP_ATOM:
        val = JS_AtomToValue(ctx, e->u.i32);
        break;
    case JS_DEF_PROP_BOOL:
        val = JS_NewBool(ctx, e->u.i32);
        break;
    case JS_DEF_PROP_STRING:
    case JS_DEF_OBJECT:
        JS_LOG("JS_InstantiateFunctionListItem", "JS_DEF_PROP_STRING/OBJECT, using autoinit");
        if (JS_DefineAutoInitProperty(ctx, obj, atom, JS_AUTOINIT_ID_PROP,
                                      (void *)e, prop_flags) < 0)
            return -1;
        return 0;
    default:
        abort();
    }
    JS_LOG("JS_InstantiateFunctionListItem", "Calling JS_DefinePropertyValue with atom=%lu, val=...", (unsigned long)atom);
    if (JS_DefinePropertyValue(ctx, obj, atom, val, prop_flags) < 0)
        return -1;
    JS_LOG("JS_InstantiateFunctionListItem", "Done");
    return 0;
}

/* Note: 'func_obj' is not necessarily a constructor */
int JS_SetConstructor2(JSContext *ctx,
                              JSValueConst func_obj,
                              JSValueConst proto,
                              int proto_flags, int ctor_flags)
{
    JS_LOG("JS_SetConstructor2", "func_obj=%08lX_%08lX, proto=%08lX_%08lX",
                        U64_HI(func_obj), U64_LO(func_obj), U64_HI(proto), U64_LO(proto));
        if (JS_DefinePropertyValue(ctx, func_obj, JS_ATOM_prototype,
                               JS_DupValue(ctx, proto), proto_flags) < 0)
        return -1;
    if (JS_DefinePropertyValue(ctx, proto, JS_ATOM_constructor,
                               JS_DupValue(ctx, func_obj),
                               ctor_flags) < 0)
        return -1;
    set_cycle_flag(ctx, func_obj);
    set_cycle_flag(ctx, proto);
    return 0;
}

#define JS_NEW_CTOR_NO_GLOBAL   (1 << 0) /* don't create a global binding */
#define JS_NEW_CTOR_PROTO_CLASS (1 << 1) /* the prototype class is 'class_id' instead of JS_CLASS_OBJECT */
#define JS_NEW_CTOR_PROTO_EXIST (1 << 2) /* the prototype is already defined */
#define JS_NEW_CTOR_READONLY    (1 << 3) /* read-only constructor field */

/* Return the constructor and. Define it as a global variable unless
   JS_NEW_CTOR_NO_GLOBAL is set. The new class inherit from
   parent_ctor if it is not JS_UNDEFINED. if class_id is != -1,
   class_proto[class_id] is set. */
JSValue JS_NewCConstructor(JSContext *ctx, int class_id, const char *name,
                                  JSCFunction *func, int length, JSCFunctionEnum cproto, int magic,
                                  JSValueConst parent_ctor,
                                  const JSCFunctionListEntry *ctor_fields, int n_ctor_fields,
                                  const JSCFunctionListEntry *proto_fields, int n_proto_fields,
                                  int flags)
{
    JSValue ctor = JS_UNDEFINED, proto, parent_proto;
    int proto_class_id, proto_flags, ctor_flags;

    JS_LOG("JS_NewCConstructor", "Entered: name=%s, class_id=%d, length=%d", name, class_id, length);

    proto_flags = 0;
    if (flags & JS_NEW_CTOR_READONLY) {
        ctor_flags = JS_PROP_CONFIGURABLE;
    } else {
        ctor_flags = JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE;
    }

    if (JS_IsUndefined(parent_ctor)) {
        JS_LOG("JS_NewCConstructor", "parent_ctor is undefined");
        parent_proto = JS_DupValue(ctx, ctx->class_proto[JS_CLASS_OBJECT]);
        parent_ctor = ctx->function_proto;
    } else {
        JS_LOG("JS_NewCConstructor", "Getting prototype from parent_ctor");
        parent_proto = JS_GetProperty(ctx, parent_ctor, JS_ATOM_prototype);
        if (JS_IsException(parent_proto)) {
            JS_LOG("JS_NewCConstructor", "GetProperty failed");
            return JS_EXCEPTION;
        }
    }

    if (flags & JS_NEW_CTOR_PROTO_EXIST) {
        JS_LOG("JS_NewCConstructor", "Proto exists, duplicating");
        proto = JS_DupValue(ctx, ctx->class_proto[class_id]);
    } else {
        if (flags & JS_NEW_CTOR_PROTO_CLASS)
            proto_class_id = class_id;
        else
            proto_class_id = JS_CLASS_OBJECT;
        JS_LOG("JS_NewCConstructor", "Allocating proto, proto_class_id=%d, n_proto_fields+1=%d", proto_class_id, n_proto_fields + 1);
        proto = JS_NewObjectProtoClassAlloc(ctx, parent_proto, proto_class_id,
                                            n_proto_fields + 1);
        if (JS_IsException(proto))
            goto fail;
        if (class_id >= 0) {
            JS_LOG("JS_NewCConstructor", "Storing proto in class_proto[%d]", class_id);
            ctx->class_proto[class_id] = JS_DupValue(ctx, proto);
        }
    }

    JS_LOG("JS_NewCConstructor", "Setting proto property list (n=%d)", n_proto_fields);
    if (JS_SetPropertyFunctionList(ctx, proto, proto_fields, n_proto_fields)) {
        JS_LOG("JS_NewCConstructor", "SetPropertyFunctionList on proto failed");
        goto fail;
    }

    JS_LOG("JS_NewCConstructor", "Creating ctor function: n_ctor_fields+3=%d", n_ctor_fields + 3);
    ctor = JS_NewCFunction3(ctx, func, name, length, cproto, magic, parent_ctor,
                            n_ctor_fields + 3);
    if (JS_IsException(ctor)) {
        JS_LOG("JS_NewCConstructor", "JS_NewCFunction3 failed");
        goto fail;
    }

    JS_LOG("JS_NewCConstructor", "Setting ctor property list (n=%d)", n_ctor_fields);
    if (JS_SetPropertyFunctionList(ctx, ctor, ctor_fields, n_ctor_fields)) {
        JS_LOG("JS_NewCConstructor", "SetPropertyFunctionList on ctor failed");
        goto fail;
    }

    if (!(flags & JS_NEW_CTOR_NO_GLOBAL)) {
        JS_LOG("JS_NewCConstructor", "Defining '%s' on global_obj", name);
        if (JS_DefinePropertyValueStr(ctx, ctx->global_obj, name,
                                      JS_DupValue(ctx, ctor),
                                      JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE) < 0) {
            JS_LOG("JS_NewCConstructor", "DefinePropertyValueStr failed");
            goto fail;
        }
    }

    JS_LOG("JS_NewCConstructor", "Setting constructor");
    JS_SetConstructor2(ctx, ctor, proto, proto_flags, ctor_flags);

    JS_LOG("JS_NewCConstructor", "Freeing proto and parent_proto");
    JS_FreeValue(ctx, proto);
    JS_FreeValue(ctx, parent_proto);
    JS_LOG("JS_NewCConstructor", "Success, returning ctor");
    return ctor;

 fail:
    JS_LOG("JS_NewCConstructor", "Failure path, freeing allocated values");
    JS_FreeValue(ctx, proto);
    JS_FreeValue(ctx, parent_proto);
    JS_FreeValue(ctx, ctor);
    return JS_EXCEPTION;
}

JSValue js_global_eval(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv)
{
    return JS_EvalObject(ctx, ctx->global_obj, argv[0], JS_EVAL_TYPE_INDIRECT, -1);
}

JSValue js_global_isNaN(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv)
{
    double d;

    if (unlikely(JS_ToFloat64(ctx, &d, argv[0])))
        return JS_EXCEPTION;
    return JS_NewBool(ctx, isnan(d));
}

JSValue js_global_isFinite(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv)
{
    double d;
    if (unlikely(JS_ToFloat64(ctx, &d, argv[0])))
        return JS_EXCEPTION;
    return JS_NewBool(ctx, isfinite(d));
}

/* Object class */

JSValue JS_ToObject(JSContext *ctx, JSValueConst val)
{
    int tag = JS_VALUE_GET_NORM_TAG(val);
    JSValue obj;

    switch(tag) {
    default:
    case JS_TAG_NULL:
    case JS_TAG_UNDEFINED:
        return JS_ThrowTypeError(ctx, "cannot convert to object");
    case JS_TAG_OBJECT:
    case JS_TAG_EXCEPTION:
        return JS_DupValue(ctx, val);
    case JS_TAG_SHORT_BIG_INT:
    case JS_TAG_BIG_INT:
        obj = JS_NewObjectClass(ctx, JS_CLASS_BIG_INT);
        goto set_value;
    case JS_TAG_INT:
    case JS_TAG_FLOAT64:
        obj = JS_NewObjectClass(ctx, JS_CLASS_NUMBER);
        goto set_value;
    case JS_TAG_STRING:
    case JS_TAG_STRING_ROPE:
        /* XXX: should call the string constructor */
        {
            JSValue str;
            str = JS_ToString(ctx, val); /* ensure that we never store a rope */
            if (JS_IsException(str))
                return JS_EXCEPTION;
            obj = JS_NewObjectClass(ctx, JS_CLASS_STRING);
            if (!JS_IsException(obj)) {
                JS_DefinePropertyValue(ctx, obj, JS_ATOM_length,
                                       JS_NewInt32(ctx, JS_VALUE_GET_STRING(str)->len), 0);
                JS_SetObjectData(ctx, obj, JS_DupValue(ctx, str));
            }
            JS_FreeValue(ctx, str);
            return obj;
        }
    case JS_TAG_BOOL:
        obj = JS_NewObjectClass(ctx, JS_CLASS_BOOLEAN);
        goto set_value;
    case JS_TAG_SYMBOL:
        obj = JS_NewObjectClass(ctx, JS_CLASS_SYMBOL);
    set_value:
        if (!JS_IsException(obj))
            JS_SetObjectData(ctx, obj, JS_DupValue(ctx, val));
        return obj;
    }
}

JSValue JS_ToObjectFree(JSContext *ctx, JSValue val)
{
    JSValue obj = JS_ToObject(ctx, val);
    JS_FreeValue(ctx, val);
    return obj;
}

int js_obj_to_desc(JSContext *ctx, JSPropertyDescriptor *d,
                          JSValueConst desc)
{
    JSValue val, getter, setter;
    int flags;

    if (!JS_IsObject(desc)) {
        JS_ThrowTypeErrorNotAnObject(ctx);
        return -1;
    }
    flags = 0;
    val = JS_UNDEFINED;
    getter = JS_UNDEFINED;
    setter = JS_UNDEFINED;
    if (JS_HasProperty(ctx, desc, JS_ATOM_enumerable)) {
        JSValue prop = JS_GetProperty(ctx, desc, JS_ATOM_enumerable);
        if (JS_IsException(prop))
            goto fail;
        flags |= JS_PROP_HAS_ENUMERABLE;
        if (JS_ToBoolFree(ctx, prop))
            flags |= JS_PROP_ENUMERABLE;
    }
    if (JS_HasProperty(ctx, desc, JS_ATOM_configurable)) {
        JSValue prop = JS_GetProperty(ctx, desc, JS_ATOM_configurable);
        if (JS_IsException(prop))
            goto fail;
        flags |= JS_PROP_HAS_CONFIGURABLE;
        if (JS_ToBoolFree(ctx, prop))
            flags |= JS_PROP_CONFIGURABLE;
    }
    if (JS_HasProperty(ctx, desc, JS_ATOM_value)) {
        flags |= JS_PROP_HAS_VALUE;
        val = JS_GetProperty(ctx, desc, JS_ATOM_value);
        if (JS_IsException(val))
            goto fail;
    }
    if (JS_HasProperty(ctx, desc, JS_ATOM_writable)) {
        JSValue prop = JS_GetProperty(ctx, desc, JS_ATOM_writable);
        if (JS_IsException(prop))
            goto fail;
        flags |= JS_PROP_HAS_WRITABLE;
        if (JS_ToBoolFree(ctx, prop))
            flags |= JS_PROP_WRITABLE;
    }
    if (JS_HasProperty(ctx, desc, JS_ATOM_get)) {
        flags |= JS_PROP_HAS_GET;
        getter = JS_GetProperty(ctx, desc, JS_ATOM_get);
        if (JS_IsException(getter) ||
            !(JS_IsUndefined(getter) || JS_IsFunction(ctx, getter))) {
            JS_ThrowTypeError(ctx, "invalid getter");
            goto fail;
        }
    }
    if (JS_HasProperty(ctx, desc, JS_ATOM_set)) {
        flags |= JS_PROP_HAS_SET;
        setter = JS_GetProperty(ctx, desc, JS_ATOM_set);
        if (JS_IsException(setter) ||
            !(JS_IsUndefined(setter) || JS_IsFunction(ctx, setter))) {
            JS_ThrowTypeError(ctx, "invalid setter");
            goto fail;
        }
    }
    if ((flags & (JS_PROP_HAS_SET | JS_PROP_HAS_GET)) &&
        (flags & (JS_PROP_HAS_VALUE | JS_PROP_HAS_WRITABLE))) {
        JS_ThrowTypeError(ctx, "cannot have setter/getter and value or writable");
        goto fail;
    }
    d->flags = flags;
    d->value = val;
    d->getter = getter;
    d->setter = setter;
    return 0;
 fail:
    JS_FreeValue(ctx, val);
    JS_FreeValue(ctx, getter);
    JS_FreeValue(ctx, setter);
    return -1;
}

__exception int JS_DefinePropertyDesc(JSContext *ctx, JSValueConst obj,
                                             JSAtom prop, JSValueConst desc,
                                             int flags)
{
    JSPropertyDescriptor d;
    int ret;

    if (js_obj_to_desc(ctx, &d, desc) < 0)
        return -1;

    ret = JS_DefineProperty(ctx, obj, prop,
                            d.value, d.getter, d.setter, d.flags | flags);
    js_free_desc(ctx, &d);
    return ret;
}

__exception int JS_ObjectDefineProperties(JSContext *ctx,
                                                 JSValueConst obj,
                                                 JSValueConst properties)
{
    JSValue props, desc;
    JSObject *p;
    JSPropertyEnum *atoms;
    uint32_t len, i;
    int ret = -1;

    if (!JS_IsObject(obj)) {
        JS_ThrowTypeErrorNotAnObject(ctx);
        return -1;
    }
    desc = JS_UNDEFINED;
    props = JS_ToObject(ctx, properties);
    if (JS_IsException(props))
        return -1;
    p = JS_VALUE_GET_OBJ(props);
    /* XXX: not done in the same order as the spec */
    if (JS_GetOwnPropertyNamesInternal(ctx, &atoms, &len, p, JS_GPN_ENUM_ONLY | JS_GPN_STRING_MASK | JS_GPN_SYMBOL_MASK) < 0)
        goto exception;
    for(i = 0; i < len; i++) {
        JS_FreeValue(ctx, desc);
        desc = JS_GetProperty(ctx, props, atoms[i].atom);
        if (JS_IsException(desc))
            goto exception;
        if (JS_DefinePropertyDesc(ctx, obj, atoms[i].atom, desc, JS_PROP_THROW) < 0)
            goto exception;
    }
    ret = 0;

exception:
    JS_FreePropertyEnum(ctx, atoms, len);
    JS_FreeValue(ctx, props);
    JS_FreeValue(ctx, desc);
    return ret;
}

JSValue js_object_constructor(JSContext *ctx, JSValueConst new_target,
                                     int argc, JSValueConst *argv)
{
    JSValue ret;
    if (!JS_IsUndefined(new_target) &&
        JS_VALUE_GET_OBJ(new_target) !=
        JS_VALUE_GET_OBJ(JS_GetActiveFunction(ctx))) {
        ret = js_create_from_ctor(ctx, new_target, JS_CLASS_OBJECT);
    } else {
        int tag = JS_VALUE_GET_NORM_TAG(argv[0]);
        switch(tag) {
        case JS_TAG_NULL:
        case JS_TAG_UNDEFINED:
            ret = JS_NewObject(ctx);
            break;
        default:
            ret = JS_ToObject(ctx, argv[0]);
            break;
        }
    }
    return ret;
}

JSValue js_object_create(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv)
{
    JSValueConst proto, props;
    JSValue obj;

    proto = argv[0];
    if (!JS_IsObject(proto) && !JS_IsNull(proto))
        return JS_ThrowTypeError(ctx, "not a prototype");
    obj = JS_NewObjectProto(ctx, proto);
    if (JS_IsException(obj))
        return JS_EXCEPTION;
    props = argv[1];
    if (!JS_IsUndefined(props)) {
        if (JS_ObjectDefineProperties(ctx, obj, props)) {
            JS_FreeValue(ctx, obj);
            return JS_EXCEPTION;
        }
    }
    return obj;
}

JSValue js_object_getPrototypeOf(JSContext *ctx, JSValueConst this_val,
                                        int argc, JSValueConst *argv, int magic)
{
    JSValueConst val;

    val = argv[0];
    if (JS_VALUE_GET_TAG(val) != JS_TAG_OBJECT) {
        /* ES6 feature non compatible with ES5.1: primitive types are
           accepted */
        if (magic || JS_VALUE_GET_TAG(val) == JS_TAG_NULL ||
            JS_VALUE_GET_TAG(val) == JS_TAG_UNDEFINED)
            return JS_ThrowTypeErrorNotAnObject(ctx);
    }
    return JS_GetPrototype(ctx, val);
}

JSValue js_object_setPrototypeOf(JSContext *ctx, JSValueConst this_val,
                                        int argc, JSValueConst *argv)
{
    JSValueConst obj;
    obj = argv[0];
    if (JS_SetPrototypeInternal(ctx, obj, argv[1], TRUE) < 0)
        return JS_EXCEPTION;
    return JS_DupValue(ctx, obj);
}

/* magic = 1 if called as Reflect.defineProperty */
JSValue js_object_defineProperty(JSContext *ctx, JSValueConst this_val,
                                        int argc, JSValueConst *argv, int magic)
{
    JSValueConst obj, prop, desc;
    int ret, flags;
    JSAtom atom;

    obj = argv[0];
    prop = argv[1];
    desc = argv[2];

    if (JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT)
        return JS_ThrowTypeErrorNotAnObject(ctx);
    atom = JS_ValueToAtom(ctx, prop);
    if (unlikely(atom == JS_ATOM_NULL))
        return JS_EXCEPTION;
    flags = 0;
    if (!magic)
        flags |= JS_PROP_THROW;
    ret = JS_DefinePropertyDesc(ctx, obj, atom, desc, flags);
    JS_FreeAtom(ctx, atom);
    if (ret < 0) {
        return JS_EXCEPTION;
    } else if (magic) {
        return JS_NewBool(ctx, ret);
    } else {
        return JS_DupValue(ctx, obj);
    }
}

JSValue js_object_defineProperties(JSContext *ctx, JSValueConst this_val,
                                          int argc, JSValueConst *argv)
{
    // defineProperties(obj, properties)
    JSValueConst obj = argv[0];

    if (JS_ObjectDefineProperties(ctx, obj, argv[1]))
        return JS_EXCEPTION;
    else
        return JS_DupValue(ctx, obj);
}

/* magic = 1 if called as __defineSetter__ */
JSValue js_object___defineGetter__(JSContext *ctx, JSValueConst this_val,
                                          int argc, JSValueConst *argv, int magic)
{
    JSValue obj;
    JSValueConst prop, value, get, set;
    int ret, flags;
    JSAtom atom;

    prop = argv[0];
    value = argv[1];

    obj = JS_ToObject(ctx, this_val);
    if (JS_IsException(obj))
        return JS_EXCEPTION;

    if (check_function(ctx, value)) {
        JS_FreeValue(ctx, obj);
        return JS_EXCEPTION;
    }
    atom = JS_ValueToAtom(ctx, prop);
    if (unlikely(atom == JS_ATOM_NULL)) {
        JS_FreeValue(ctx, obj);
        return JS_EXCEPTION;
    }
    flags = JS_PROP_THROW |
        JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE |
        JS_PROP_HAS_CONFIGURABLE | JS_PROP_CONFIGURABLE;
    if (magic) {
        get = JS_UNDEFINED;
        set = value;
        flags |= JS_PROP_HAS_SET;
    } else {
        get = value;
        set = JS_UNDEFINED;
        flags |= JS_PROP_HAS_GET;
    }
    ret = JS_DefineProperty(ctx, obj, atom, JS_UNDEFINED, get, set, flags);
    JS_FreeValue(ctx, obj);
    JS_FreeAtom(ctx, atom);
    if (ret < 0) {
        return JS_EXCEPTION;
    } else {
        return JS_UNDEFINED;
    }
}

JSValue js_object_getOwnPropertyDescriptor(JSContext *ctx, JSValueConst this_val,
                                                  int argc, JSValueConst *argv, int magic)
{
    JSValueConst prop;
    JSAtom atom;
    JSValue ret, obj;
    JSPropertyDescriptor desc;
    int res, flags;

    if (magic) {
        /* Reflect.getOwnPropertyDescriptor case */
        if (JS_VALUE_GET_TAG(argv[0]) != JS_TAG_OBJECT)
            return JS_ThrowTypeErrorNotAnObject(ctx);
        obj = JS_DupValue(ctx, argv[0]);
    } else {
        obj = JS_ToObject(ctx, argv[0]);
        if (JS_IsException(obj))
            return obj;
    }
    prop = argv[1];
    atom = JS_ValueToAtom(ctx, prop);
    if (unlikely(atom == JS_ATOM_NULL))
        goto exception;
    ret = JS_UNDEFINED;
    if (JS_VALUE_GET_TAG(obj) == JS_TAG_OBJECT) {
        res = JS_GetOwnPropertyInternal(ctx, &desc, JS_VALUE_GET_OBJ(obj), atom);
        if (res < 0)
            goto exception;
        if (res) {
            ret = JS_NewObject(ctx);
            if (JS_IsException(ret))
                goto exception1;
            flags = JS_PROP_C_W_E | JS_PROP_THROW;
            if (desc.flags & JS_PROP_GETSET) {
                if (JS_DefinePropertyValue(ctx, ret, JS_ATOM_get, JS_DupValue(ctx, desc.getter), flags) < 0
                ||  JS_DefinePropertyValue(ctx, ret, JS_ATOM_set, JS_DupValue(ctx, desc.setter), flags) < 0)
                    goto exception1;
            } else {
                if (JS_DefinePropertyValue(ctx, ret, JS_ATOM_value, JS_DupValue(ctx, desc.value), flags) < 0
                ||  JS_DefinePropertyValue(ctx, ret, JS_ATOM_writable,
                                           JS_NewBool(ctx, desc.flags & JS_PROP_WRITABLE), flags) < 0)
                    goto exception1;
            }
            if (JS_DefinePropertyValue(ctx, ret, JS_ATOM_enumerable,
                                       JS_NewBool(ctx, desc.flags & JS_PROP_ENUMERABLE), flags) < 0
            ||  JS_DefinePropertyValue(ctx, ret, JS_ATOM_configurable,
                                       JS_NewBool(ctx, desc.flags & JS_PROP_CONFIGURABLE), flags) < 0)
                goto exception1;
            js_free_desc(ctx, &desc);
        }
    }
    JS_FreeAtom(ctx, atom);
    JS_FreeValue(ctx, obj);
    return ret;

exception1:
    js_free_desc(ctx, &desc);
    JS_FreeValue(ctx, ret);
exception:
    JS_FreeAtom(ctx, atom);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}
