#include "js_internal5.h"
#include "debuglog.h"

/*
 * 辅助函数 do_array, do_var, do_control, do_misc
 * 实现数组操作、变量操作、控制流和杂项操作码
 */

/* ========== do_array：数组操作 ========== */
int do_array(CallState *cs, int op)
{
    JSContext *ctx = cs->ctx;
    JSStackFrame *sf = cs->sf;
    const uint8_t *pc = cs->pc;
    JSValue *sp = cs->sp;
    JSValue ret_val, obj, prop, val;
    JSObject *p;
    uint32_t idx;
    int ret, mask;

    switch (op) {
    case OP_get_array_el:
    case OP_get_array_el2:
        {
            int keep = (op == OP_get_array_el2);
            obj = sp[-2];
            prop = sp[-1];
            if (likely(JS_VALUE_GET_TAG(obj) == JS_TAG_OBJECT &&
                       JS_VALUE_GET_TAG(prop) == JS_TAG_INT)) {
                p = JS_VALUE_GET_OBJ(obj);
                idx = JS_VALUE_GET_INT(prop);
                if (unlikely(p->class_id != JS_CLASS_ARRAY))
                    goto array_el_slow;
                if (unlikely(idx >= p->u.array.count))
                    goto array_el_slow;
                val = JS_DupValue(ctx, p->u.array.u.values[idx]);
            } else {
            array_el_slow:
                sf->cur_pc = pc;
                val = JS_GetPropertyValue(ctx, obj, prop);
                if (unlikely(JS_IsException(val))) {
                    if (keep) {
                        sp[-1] = JS_UNDEFINED;
                    } else {
                        sp--;
                    }
                    goto exception;
                }
            }
            if (keep) {
                sp[-1] = val;
            } else {
                JS_FreeValue(ctx, obj);
                sp[-2] = val;
                sp--;
            }
        }
        break;
    case OP_get_array_el3:
        {
            obj = sp[-2];
            prop = sp[-1];
            if (likely(JS_VALUE_GET_TAG(obj) == JS_TAG_OBJECT &&
                       JS_VALUE_GET_TAG(prop) == JS_TAG_INT)) {
                p = JS_VALUE_GET_OBJ(obj);
                idx = JS_VALUE_GET_INT(prop);
                if (unlikely(p->class_id != JS_CLASS_ARRAY))
                    goto get_array_el3_slow;
                if (unlikely(idx >= p->u.array.count))
                    goto get_array_el3_slow;
                val = JS_DupValue(ctx, p->u.array.u.values[idx]);
            } else {
            get_array_el3_slow:
                switch (JS_VALUE_GET_TAG(prop)) {
                case JS_TAG_INT:
                case JS_TAG_STRING:
                case JS_TAG_SYMBOL:
                    break;
                default:
                    if (unlikely(JS_IsUndefined(obj) || JS_IsNull(obj))) {
                        JS_ThrowTypeError(ctx, "value has no property");
                        goto exception;
                    }
                    sf->cur_pc = pc;
                    ret_val = JS_ToPropertyKey(ctx, prop);
                    if (JS_IsException(ret_val))
                        goto exception;
                    JS_FreeValue(ctx, prop);
                    sp[-1] = ret_val;
                    break;
                }
                sf->cur_pc = pc;
                val = JS_GetPropertyValue(ctx, obj, JS_DupValue(ctx, sp[-1]));
                if (unlikely(JS_IsException(val)))
                    goto exception;
            }
            *sp++ = val;
        }
        break;
    case OP_put_array_el:
        {
            obj = sp[-3];
            prop = sp[-2];
            val = sp[-1];
            if (likely(JS_VALUE_GET_TAG(obj) == JS_TAG_OBJECT &&
                       JS_VALUE_GET_TAG(prop) == JS_TAG_INT)) {
                p = JS_VALUE_GET_OBJ(obj);
                idx = JS_VALUE_GET_INT(prop);
                if (unlikely(p->class_id != JS_CLASS_ARRAY))
                    goto put_array_el_slow;
                if (unlikely(idx >= (uint32_t)p->u.array.count)) {
                    uint32_t new_len, array_len;
                    if (unlikely(idx != (uint32_t)p->u.array.count ||
                                 !p->fast_array ||
                                 !can_extend_fast_array(p))) {
                        goto put_array_el_slow;
                    }
                    if (likely(JS_VALUE_GET_TAG(p->prop[0].u.value) != JS_TAG_INT))
                        goto put_array_el_slow;
                    new_len = idx + 1;
                    if (unlikely(new_len > p->u.array.u1.size))
                        goto put_array_el_slow;
                    array_len = JS_VALUE_GET_INT(p->prop[0].u.value);
                    if (new_len > array_len) {
                        if (unlikely(!(get_shape_prop(p->shape)->flags & JS_PROP_WRITABLE)))
                            goto put_array_el_slow;
                        p->prop[0].u.value = JS_NewInt32(ctx, new_len);
                    }
                    p->u.array.count = new_len;
                    p->u.array.u.values[idx] = val;
                } else {
                    set_value(ctx, &p->u.array.u.values[idx], val);
                }
                JS_FreeValue(ctx, obj);
                sp -= 3;
            } else {
            put_array_el_slow:
                sf->cur_pc = pc;
                ret = JS_SetPropertyValue(ctx, obj, prop, val, JS_PROP_THROW_STRICT);
                JS_FreeValue(ctx, obj);
                sp -= 3;
                if (unlikely(ret < 0))
                    goto exception;
            }
        }
        break;
    case OP_define_array_el:
        {
            ret = JS_DefinePropertyValueValue(ctx, sp[-3], JS_DupValue(ctx, sp[-2]), sp[-1],
                                              JS_PROP_C_W_E | JS_PROP_THROW);
            sp -= 1;
            if (unlikely(ret < 0))
                goto exception;
        }
        break;
    case OP_append:
        {
            sf->cur_pc = pc;
            if (js_append_enumerate(ctx, sp))
                goto exception;
            JS_FreeValue(ctx, *--sp);
        }
        break;
    case OP_copy_data_properties:
        {
            mask = *pc++;
            sf->cur_pc = pc;
            if (JS_CopyDataProperties(ctx, sp[-1 - (mask & 3)],
                                      sp[-1 - ((mask >> 2) & 7)],
                                      sp[-1 - ((mask >> 5) & 7)], 0))
                goto exception;
        }
        break;
    case OP_array_from:
        {
            int call_argc = get_u16(pc);
            pc += 2;
            ret_val = js_create_array_free(ctx, call_argc, sp - call_argc);
            sp -= call_argc;
            if (unlikely(JS_IsException(ret_val)))
                goto exception;
            *sp++ = ret_val;
        }
        break;
    default:
        abort();
    }

    cs->pc = pc;
    cs->sp = sp;
    return 0;

exception:
    cs->exception = 1;
    return -1;
}

/* ========== do_var：变量操作 ========== */
int do_var(CallState *cs, int op)
{
	JSContext *ctx = cs->ctx;
    JSRuntime *rt = cs->rt;
    JSStackFrame *sf = cs->sf;
    JSFunctionBytecode *b = cs->b;
    const uint8_t *pc = cs->pc;
    JSValue *sp = cs->sp;
    JSValue *var_buf = cs->var_buf;
    JSValue *arg_buf = cs->arg_buf;
    JSVarRef **var_refs = cs->var_refs;
    JSValue val, obj;
    JSAtom atom;
    int idx, ret;
    JSClosureVar *cv;
    JSVarRef *var_ref;
    JSProperty *pr;
    int is_with, diff;

    switch (op) {
    case OP_get_var_undef:
    case OP_get_var:
            {
				int idx;
                JSValue val;
                idx = get_u16(pc);
                pc += 2;
				JS_LOG("do_var", "OP_get_var: idx=%d, var_refs=%04X:%04X",
                   idx, FARPTR_SEG(var_refs), FARPTR_OFF(var_refs));
                val = *var_refs[idx]->pvalue;
				JS_LOG("do_var", "val=%08lX_%08lX, var_refs[%d]->pvalue=%04X:%04X",
                   U64_HI(val), U64_LO(val), idx,
                   FARPTR_SEG(var_refs[idx]->pvalue), FARPTR_OFF(var_refs[idx]->pvalue));
                if (unlikely(JS_IsUninitialized(val))) {
					JS_LOG("do_var", "JS_IsUninitialized(val) is true, will set cv");
                    JSClosureVar *cv = &b->closure_var[idx];
                    if (cv->is_lexical) {
						JS_LOG("do_var", "cv->is_lexical is true, will into JS_ThrowReferenceErrorUninitialized");
                        JS_ThrowReferenceErrorUninitialized(ctx, cv->var_name);
						JS_LOG("do_var", "func done");
                        goto exception;
                    } else {
						JS_LOG("do_var", "cv->is_lexical is false");
                        sf->cur_pc = pc;
						JS_LOG("do_var", "will into JS_GetPropertyInternal");
                        sp[0] = JS_GetPropertyInternal(ctx, ctx->global_obj,
                                                       cv->var_name,
                                                       ctx->global_obj,
                                                       op - OP_get_var_undef);
                        JS_LOG("do_var", "func done");
						if (JS_IsException(sp[0]))
                            goto exception;
                    }
                } else {
                    JS_LOG("do_var", "Calling JS_DupValue on val=%08lX_%08lX", U64_HI(val), U64_LO(val));
					sp[0] = JS_DupValue(ctx, val);
					JS_LOG("do_var", "set sp[0] from JS_DupValue");
                }
                sp++;
            }
        break;
    case OP_put_var:
    case OP_put_var_init:
        {
            idx = get_u16(pc);
            pc += 2;
            var_ref = var_refs[idx];
			JS_LOG("do_var", "OP_put_var: idx=%d, var_ref=%04X:%04X, pvalue=%04X:%04X, *pvalue before=%08lX_%08lX",
                   idx, FARPTR_SEG(var_ref), FARPTR_OFF(var_ref),
                   FARPTR_SEG(var_ref->pvalue), FARPTR_OFF(var_ref->pvalue),
                   U64_HI(*var_ref->pvalue), U64_LO(*var_ref->pvalue));
            if (unlikely(JS_IsUninitialized(*var_ref->pvalue) ||
                         var_ref->is_const)) {
                cv = &b->closure_var[idx];
                if (var_ref->is_lexical) {
                    if (op == OP_put_var_init)
                        goto put_var_ok;
                    if (JS_IsUninitialized(*var_ref->pvalue))
                        JS_ThrowReferenceErrorUninitialized(ctx, cv->var_name);
                    else
                        JS_ThrowTypeErrorReadOnly(ctx, JS_PROP_THROW, cv->var_name);
                    goto exception;
                } else {
                    sf->cur_pc = pc;
                    ret = JS_HasProperty(ctx, ctx->global_obj, cv->var_name);
                    if (ret < 0)
                        goto exception;
                    if (ret == 0 && is_strict_mode(ctx)) {
                        JS_ThrowReferenceErrorNotDefined(ctx, cv->var_name);
                        goto exception;
                    }
                    ret = JS_SetPropertyInternal(ctx, ctx->global_obj, cv->var_name, sp[-1],
                                                 ctx->global_obj, JS_PROP_THROW_STRICT);
                    sp--;
                    if (ret < 0)
                        goto exception;
                }
            } else {
            put_var_ok:
				JS_LOG("do_var", "OP_put_var: writing sp[-1]=%08lX_%08lX to pvalue",
                       U64_HI(sp[-1]), U64_LO(sp[-1]));
                /* 正确管理引用：释放旧值，写入新值并增加引用 */
                JSValue old_val = *var_ref->pvalue;
                *var_ref->pvalue = JS_DupValue(ctx, sp[-1]);
                JS_FreeValue(ctx, old_val);
				// set_value(ctx, var_ref->pvalue, JS_DupValue(ctx, sp[-1]));
                sp--;
            }
			JS_LOG("do_var", "OP_put_var: *pvalue after=%08lX_%08lX",
                   U64_HI(*var_ref->pvalue), U64_LO(*var_ref->pvalue));
        }
        break;
    case OP_get_loc:
        {
            idx = get_u16(pc);
            pc += 2;
            sp[0] = JS_DupValue(ctx, var_buf[idx]);
            sp++;
        }
        break;
    case OP_put_loc:
        {
            idx = get_u16(pc);
            pc += 2;
            set_value(ctx, &var_buf[idx], sp[-1]);
            sp--;
        }
        break;
    case OP_set_loc:
        {
            idx = get_u16(pc);
            pc += 2;
            set_value(ctx, &var_buf[idx], JS_DupValue(ctx, sp[-1]));
        }
        break;
    case OP_get_arg:
        {
            idx = get_u16(pc);
            pc += 2;
            sp[0] = JS_DupValue(ctx, arg_buf[idx]);
            sp++;
        }
        break;
    case OP_put_arg:
        {
            idx = get_u16(pc);
            pc += 2;
            set_value(ctx, &arg_buf[idx], sp[-1]);
            sp--;
        }
        break;
    case OP_set_arg:
        {
            idx = get_u16(pc);
            pc += 2;
            set_value(ctx, &arg_buf[idx], JS_DupValue(ctx, sp[-1]));
        }
        break;
#if SHORT_OPCODES
    case OP_get_loc8:
        *sp++ = JS_DupValue(ctx, var_buf[*pc++]);
        break;
    case OP_put_loc8:
        set_value(ctx, &var_buf[*pc++], *--sp);
        break;
    case OP_set_loc8:
        set_value(ctx, &var_buf[*pc++], JS_DupValue(ctx, sp[-1]));
        break;
    case OP_get_loc0:
    case OP_get_loc1:
    case OP_get_loc2:
    case OP_get_loc3:
        *sp++ = JS_DupValue(ctx, var_buf[op - OP_get_loc0]);
        break;
    case OP_put_loc0:
    case OP_put_loc1:
    case OP_put_loc2:
    case OP_put_loc3:
        set_value(ctx, &var_buf[op - OP_put_loc0], *--sp);
        break;
    case OP_set_loc0:
    case OP_set_loc1:
    case OP_set_loc2:
    case OP_set_loc3:
        set_value(ctx, &var_buf[op - OP_set_loc0], JS_DupValue(ctx, sp[-1]));
        break;
    case OP_get_arg0:
    case OP_get_arg1:
    case OP_get_arg2:
    case OP_get_arg3:
        *sp++ = JS_DupValue(ctx, arg_buf[op - OP_get_arg0]);
        break;
    case OP_put_arg0:
    case OP_put_arg1:
    case OP_put_arg2:
    case OP_put_arg3:
        set_value(ctx, &arg_buf[op - OP_put_arg0], *--sp);
        break;
    case OP_set_arg0:
    case OP_set_arg1:
    case OP_set_arg2:
    case OP_set_arg3:
        set_value(ctx, &arg_buf[op - OP_set_arg0], JS_DupValue(ctx, sp[-1]));
        break;
    case OP_get_var_ref0:
    case OP_get_var_ref1:
    case OP_get_var_ref2:
    case OP_get_var_ref3:
        *sp++ = JS_DupValue(ctx, *var_refs[op - OP_get_var_ref0]->pvalue);
        break;
    case OP_put_var_ref0:
    case OP_put_var_ref1:
    case OP_put_var_ref2:
    case OP_put_var_ref3:
        set_value(ctx, var_refs[op - OP_put_var_ref0]->pvalue, *--sp);
        break;
    case OP_set_var_ref0:
    case OP_set_var_ref1:
    case OP_set_var_ref2:
    case OP_set_var_ref3:
        set_value(ctx, var_refs[op - OP_set_var_ref0]->pvalue, JS_DupValue(ctx, sp[-1]));
        break;
#endif
    case OP_get_var_ref:
        {
            idx = get_u16(pc);
            pc += 2;
            val = *var_refs[idx]->pvalue;
            sp[0] = JS_DupValue(ctx, val);
            sp++;
        }
        break;
    case OP_put_var_ref:
        {
            idx = get_u16(pc);
            pc += 2;
            set_value(ctx, var_refs[idx]->pvalue, sp[-1]);
            sp--;
        }
        break;
    case OP_set_var_ref:
        {
            idx = get_u16(pc);
            pc += 2;
            set_value(ctx, var_refs[idx]->pvalue, JS_DupValue(ctx, sp[-1]));
        }
        break;
    case OP_get_var_ref_check:
        {
            idx = get_u16(pc);
            pc += 2;
            val = *var_refs[idx]->pvalue;
            if (unlikely(JS_IsUninitialized(val))) {
                JS_ThrowReferenceErrorUninitialized2(ctx, b, idx, TRUE);
                goto exception;
            }
            sp[0] = JS_DupValue(ctx, val);
            sp++;
        }
        break;
    case OP_put_var_ref_check:
        {
            idx = get_u16(pc);
            pc += 2;
            if (unlikely(JS_IsUninitialized(*var_refs[idx]->pvalue))) {
                JS_ThrowReferenceErrorUninitialized2(ctx, b, idx, TRUE);
                goto exception;
            }
            set_value(ctx, var_refs[idx]->pvalue, sp[-1]);
            sp--;
        }
        break;
    case OP_put_var_ref_check_init:
        {
            idx = get_u16(pc);
            pc += 2;
            if (unlikely(!JS_IsUninitialized(*var_refs[idx]->pvalue))) {
                JS_ThrowReferenceErrorUninitialized2(ctx, b, idx, TRUE);
                goto exception;
            }
            set_value(ctx, var_refs[idx]->pvalue, sp[-1]);
            sp--;
        }
        break;
    case OP_set_loc_uninitialized:
        {
            idx = get_u16(pc);
            pc += 2;
            set_value(ctx, &var_buf[idx], JS_UNINITIALIZED);
        }
        break;
    case OP_get_loc_check:
        {
            idx = get_u16(pc);
            pc += 2;
            if (unlikely(JS_IsUninitialized(var_buf[idx]))) {
                JS_ThrowReferenceErrorUninitialized2(ctx, b, idx, FALSE);
                goto exception;
            }
            sp[0] = JS_DupValue(ctx, var_buf[idx]);
            sp++;
        }
        break;
    case OP_get_loc_checkthis:
        {
            idx = get_u16(pc);
            pc += 2;
            if (unlikely(JS_IsUninitialized(var_buf[idx]))) {
                JS_ThrowReferenceErrorUninitialized2(cs->ctx, b, idx, FALSE);
                goto exception;
            }
            sp[0] = JS_DupValue(ctx, var_buf[idx]);
            sp++;
        }
        break;
    case OP_put_loc_check:
        {
            idx = get_u16(pc);
            pc += 2;
            if (unlikely(JS_IsUninitialized(var_buf[idx]))) {
                JS_ThrowReferenceErrorUninitialized2(ctx, b, idx, FALSE);
                goto exception;
            }
            set_value(ctx, &var_buf[idx], sp[-1]);
            sp--;
        }
        break;
    case OP_set_loc_check:
        {
            idx = get_u16(pc);
            pc += 2;
            if (unlikely(JS_IsUninitialized(var_buf[idx]))) {
                JS_ThrowReferenceErrorUninitialized2(ctx, b, idx, FALSE);
                goto exception;
            }
            set_value(ctx, &var_buf[idx], JS_DupValue(ctx, sp[-1]));
        }
        break;
    case OP_put_loc_check_init:
        {
            idx = get_u16(pc);
            pc += 2;
            if (unlikely(!JS_IsUninitialized(var_buf[idx]))) {
                JS_ThrowReferenceError(ctx, "'this' can be initialized only once");
                goto exception;
            }
            set_value(ctx, &var_buf[idx], sp[-1]);
            sp--;
        }
        break;
    case OP_close_loc:
        {
            idx = get_u16(pc);
            pc += 2;
            close_lexical_var(ctx, b, sf, idx);
        }
        break;
    case OP_make_loc_ref:
    case OP_make_arg_ref:
    case OP_make_var_ref_ref:
        {
            atom = get_u32(pc);
            idx = get_u16(pc + 4);
            pc += 6;
            *sp++ = JS_NewObjectProto(ctx, JS_NULL);
            if (unlikely(JS_IsException(sp[-1])))
                goto exception;
            if (op == OP_make_var_ref_ref) {
                var_ref = var_refs[idx];
                js_rc(var_ref)->ref_count++;
            } else {
                var_ref = get_var_ref(ctx, sf, idx, op == OP_make_arg_ref);
                if (!var_ref)
                    goto exception;
            }
            pr = add_property(ctx, JS_VALUE_GET_OBJ(sp[-1]), atom,
                              JS_PROP_WRITABLE | JS_PROP_VARREF);
            if (!pr) {
                free_var_ref(rt, var_ref);
                goto exception;
            }
            pr->u.var_ref = var_ref;
            *sp++ = JS_AtomToValue(ctx, atom);
        }
        break;
    case OP_make_var_ref:
        {
            atom = get_u32(pc);
            pc += 4;
            sf->cur_pc = pc;
            if (JS_GetGlobalVarRef(ctx, atom, sp))
                goto exception;
            sp += 2;
        }
        break;
    case OP_with_get_var:
    case OP_with_put_var:
    case OP_with_delete_var:
    case OP_with_make_ref:
    case OP_with_get_ref:
        {
            atom = get_u32(pc);
            diff = get_u32(pc + 4);
            is_with = pc[8];
            pc += 9;
            sf->cur_pc = pc;
            obj = sp[-1];
            ret = JS_HasProperty(ctx, obj, atom);
            if (unlikely(ret < 0))
                goto exception;
            if (ret) {
                if (is_with) {
                    ret = js_has_unscopable(ctx, obj, atom);
                    if (unlikely(ret < 0))
                        goto exception;
                    if (ret)
                        goto no_with;
                }
                switch (op) {
                case OP_with_get_var:
                    ret = JS_HasProperty(ctx, obj, atom);
                    if (unlikely(ret <= 0)) {
                        if (ret < 0)
                            goto exception;
                        if (is_strict_mode(ctx)) {
                            JS_ThrowReferenceErrorNotDefined(ctx, atom);
                            goto exception;
                        }
                        val = JS_UNDEFINED;
                    } else {
                        val = JS_GetProperty(ctx, obj, atom);
                        if (unlikely(JS_IsException(val)))
                            goto exception;
                    }
                    set_value(ctx, &sp[-1], val);
                    break;
                case OP_with_put_var:
                    ret = JS_HasProperty(ctx, obj, atom);
                    if (unlikely(ret <= 0)) {
                        if (ret < 0)
                            goto exception;
                        if (is_strict_mode(ctx)) {
                            JS_ThrowReferenceErrorNotDefined(ctx, atom);
                            goto exception;
                        }
                    }
                    ret = JS_SetPropertyInternal(ctx, obj, atom, sp[-2], obj,
                                                 JS_PROP_THROW_STRICT);
                    JS_FreeValue(ctx, sp[-1]);
                    sp -= 2;
                    if (unlikely(ret < 0))
                        goto exception;
                    break;
                case OP_with_delete_var:
                    ret = JS_DeleteProperty(ctx, obj, atom, 0);
                    if (unlikely(ret < 0))
                        goto exception;
                    JS_FreeValue(ctx, sp[-1]);
                    sp[-1] = JS_NewBool(ctx, ret);
                    break;
                case OP_with_make_ref:
                    *sp++ = JS_AtomToValue(ctx, atom);
                    break;
                case OP_with_get_ref:
                    ret = JS_HasProperty(ctx, obj, atom);
                    if (unlikely(ret < 0))
                        goto exception;
                    if (!ret) {
                        val = JS_UNDEFINED;
                    } else {
                        val = JS_GetProperty(ctx, obj, atom);
                        if (unlikely(JS_IsException(val)))
                            goto exception;
                    }
                    *sp++ = val;
                    break;
                }
                pc += diff - 5;
            } else {
            no_with:
                JS_FreeValue(ctx, sp[-1]);
                sp--;
            }
        }
        break;
    default:
        abort();
    }

    cs->pc = pc;
    cs->sp = sp;
    return 0;

exception:
    cs->exception = 1;
    return -1;
}

/* ========== do_control：控制流 ========== */
int do_control(CallState *cs, int op)
{
    JSContext *ctx = cs->ctx;
    JSStackFrame *sf = cs->sf;
    JSFunctionBytecode *b = cs->b;
    const uint8_t *pc = cs->pc;
    JSValue *sp = cs->sp;
    JSValue op1, ret_val;
    int res, pos;
    uint32_t diff;
    JSAtom atom;
    int type;

    switch (op) {
    case OP_goto:
        pc += (int32_t)get_u32(pc);
        if (unlikely(js_poll_interrupts(ctx)))
            goto exception;
        break;
#if SHORT_OPCODES
    case OP_goto16:
        pc += (int16_t)get_u16(pc);
        if (unlikely(js_poll_interrupts(ctx)))
            goto exception;
        break;
    case OP_goto8:
        pc += (int8_t)pc[0];
        if (unlikely(js_poll_interrupts(ctx)))
            goto exception;
        break;
#endif
    case OP_if_true:
        {
            op1 = sp[-1];
            pc += 4;
            if ((uint32_t)JS_VALUE_GET_TAG(op1) <= JS_TAG_UNDEFINED) {
                res = JS_VALUE_GET_INT(op1);
            } else {
                res = JS_ToBoolFree(ctx, op1);
            }
            sp--;
            if (res) {
                pc += (int32_t)get_u32(pc - 4) - 4;
            }
            if (unlikely(js_poll_interrupts(ctx)))
                goto exception;
        }
        break;
    case OP_if_false:
        {
            op1 = sp[-1];
            pc += 4;
            if ((uint32_t)JS_VALUE_GET_TAG(op1) <= JS_TAG_UNDEFINED) {
                res = JS_VALUE_GET_INT(op1);
            } else {
                res = JS_ToBoolFree(ctx, op1);
            }
            sp--;
            if (!res) {
                pc += (int32_t)get_u32(pc - 4) - 4;
            }
            if (unlikely(js_poll_interrupts(ctx)))
                goto exception;
        }
        break;
#if SHORT_OPCODES
    case OP_if_true8:
        {
            op1 = sp[-1];
            pc += 1;
            if ((uint32_t)JS_VALUE_GET_TAG(op1) <= JS_TAG_UNDEFINED) {
                res = JS_VALUE_GET_INT(op1);
            } else {
                res = JS_ToBoolFree(ctx, op1);
            }
            sp--;
            if (res) {
                pc += (int8_t)pc[-1] - 1;
            }
            if (unlikely(js_poll_interrupts(ctx)))
                goto exception;
        }
        break;
    case OP_if_false8:
        {
            op1 = sp[-1];
            pc += 1;
            if ((uint32_t)JS_VALUE_GET_TAG(op1) <= JS_TAG_UNDEFINED) {
                res = JS_VALUE_GET_INT(op1);
            } else {
                res = JS_ToBoolFree(ctx, op1);
            }
            sp--;
            if (!res) {
                pc += (int8_t)pc[-1] - 1;
            }
            if (unlikely(js_poll_interrupts(ctx)))
                goto exception;
        }
        break;
#endif
    case OP_catch:
        {
            diff = get_u32(pc);
            sp[0] = JS_NewCatchOffset(ctx, pc + diff - b->byte_code_buf);
            sp++;
            pc += 4;
        }
        break;
    case OP_gosub:
        {
            diff = get_u32(pc);
            sp[0] = JS_NewInt32(ctx, pc + 4 - b->byte_code_buf);
            sp++;
            pc += diff;
        }
        break;
    case OP_ret:
        {
            op1 = sp[-1];
            if (unlikely(JS_VALUE_GET_TAG(op1) != JS_TAG_INT))
                goto ret_fail;
            pos = JS_VALUE_GET_INT(op1);
            if (unlikely(pos >= b->byte_code_len)) {
            ret_fail:
                JS_ThrowInternalError(ctx, "invalid ret value");
                goto exception;
            }
            sp--;
            pc = b->byte_code_buf + pos;
        }
        break;
    case OP_return:
		JS_LOG("JS_CallInternal", "OP_return reached, ret_val in sp[-1]=%08lX_%08lX",
           U64_HI(sp[-1]), U64_LO(sp[-1]));
        ret_val = *--sp;
        goto done;
    case OP_return_undef:
		JS_LOG("OP_DETAIL", "OP_return_undef: sp=%04X:%04X, stack_buf=%04X:%04X, sp-stack_buf=%d",
           FARPTR_SEG(cs->sp), FARPTR_OFF(cs->sp),
           FARPTR_SEG(cs->stack_buf), FARPTR_OFF(cs->stack_buf),
           cs->sp - cs->stack_buf);
        ret_val = JS_UNDEFINED;
        goto done;
    case OP_throw:
        JS_Throw(ctx, *--sp);
        goto exception;
    case OP_throw_error:
        {
            atom = get_u32(pc);
            type = pc[4];
            pc += 5;
            if (type == JS_THROW_VAR_RO)
                JS_ThrowTypeErrorReadOnly(ctx, JS_PROP_THROW, atom);
            else if (type == JS_THROW_VAR_REDECL)
                JS_ThrowSyntaxErrorVarRedeclaration(ctx, atom);
            else if (type == JS_THROW_VAR_UNINITIALIZED)
                JS_ThrowReferenceErrorUninitialized(ctx, atom);
            else if (type == JS_THROW_ERROR_DELETE_SUPER)
                JS_ThrowReferenceError(ctx, "unsupported reference to 'super'");
            else if (type == JS_THROW_ERROR_ITERATOR_THROW)
                JS_ThrowTypeError(ctx, "iterator does not have a throw method");
            else
                JS_ThrowInternalError(ctx, "invalid throw var type %d", type);
            goto exception;
        }
        break;
    case OP_for_in_start:
        sf->cur_pc = pc;
        if (js_for_in_start(ctx, sp))
            goto exception;
        break;
    case OP_for_in_next:
        sf->cur_pc = pc;
        if (js_for_in_next(ctx, sp))
            goto exception;
        sp += 2;
        break;
    case OP_for_of_start:
        sf->cur_pc = pc;
        if (js_for_of_start(ctx, sp, FALSE))
            goto exception;
        sp += 1;
        *sp++ = JS_NewCatchOffset(ctx, 0);
        break;
    case OP_for_of_next:
        {
            int offset = -3 - pc[0];
            pc += 1;
            sf->cur_pc = pc;
            if (js_for_of_next(ctx, sp, offset))
                goto exception;
            sp += 2;
        }
        break;
    case OP_for_await_of_next:
        sf->cur_pc = pc;
        if (js_for_await_of_next(ctx, sp))
            goto exception;
        sp++;
        break;
    case OP_for_await_of_start:
        sf->cur_pc = pc;
        if (js_for_of_start(ctx, sp, TRUE))
            goto exception;
        sp += 1;
        *sp++ = JS_NewCatchOffset(ctx, 0);
        break;
    case OP_iterator_get_value_done:
        sf->cur_pc = pc;
        if (js_iterator_get_value_done(ctx, sp))
            goto exception;
        sp += 1;
        break;
    case OP_iterator_check_object:
        if (unlikely(!JS_IsObject(sp[-1]))) {
            JS_ThrowTypeError(ctx, "iterator must return an object");
            goto exception;
        }
        break;
    case OP_iterator_close:
        sp--; /* drop catch offset */
        JS_FreeValue(ctx, sp[-1]);
        sp--;
        if (!JS_IsUndefined(sp[-1])) {
            sf->cur_pc = pc;
            if (JS_IteratorClose(ctx, sp[-1], FALSE))
                goto exception;
            JS_FreeValue(ctx, sp[-1]);
        }
        sp--;
        break;
    case OP_nip_catch:
        {
            ret_val = *--sp;
            while (sp > cs->stack_buf &&
                   JS_VALUE_GET_TAG(sp[-1]) != JS_TAG_CATCH_OFFSET) {
                JS_FreeValue(ctx, *--sp);
            }
            if (unlikely(sp == cs->stack_buf)) {
                JS_ThrowInternalError(ctx, "nip_catch");
                JS_FreeValue(ctx, ret_val);
                goto exception;
            }
            sp[-1] = ret_val;
        }
        break;
    case OP_iterator_next:
        {
            sf->cur_pc = pc;
            ret_val = JS_Call(ctx, sp[-3], sp[-4],
                              1, (JSValueConst *)(sp - 1));
            if (JS_IsException(ret_val))
                goto exception;
            JS_FreeValue(ctx, sp[-1]);
            sp[-1] = ret_val;
        }
        break;
    case OP_iterator_call:
        {
            JSValue method, ret;
            BOOL ret_flag;
            int flags = *pc++;
            sf->cur_pc = pc;
            method = JS_GetProperty(ctx, sp[-4], (flags & 1) ?
                                    JS_ATOM_throw : JS_ATOM_return);
            if (JS_IsException(method))
                goto exception;
            if (JS_IsUndefined(method) || JS_IsNull(method)) {
                ret_flag = TRUE;
            } else {
                if (flags & 2) {
                    ret = JS_CallFree(ctx, method, sp[-4],
                                      0, NULL);
                } else {
                    ret = JS_CallFree(ctx, method, sp[-4],
                                      1, (JSValueConst *)(sp - 1));
                }
                if (JS_IsException(ret))
                    goto exception;
                JS_FreeValue(ctx, sp[-1]);
                sp[-1] = ret;
                ret_flag = FALSE;
            }
            sp[0] = JS_NewBool(ctx, ret_flag);
            sp += 1;
        }
        break;
    default:
        abort();
    }

    cs->pc = pc;
    cs->sp = sp;
    return 0;

done:
    cs->ret_val = ret_val;
    cs->exception = 0;
	JS_LOG("OP_DETAIL", "done: sp=%04X:%04X, stack_buf=%04X:%04X, sp-stack_buf=%d, ret_val=%08lX_%08lX",
           FARPTR_SEG(cs->sp), FARPTR_OFF(cs->sp),
           FARPTR_SEG(cs->stack_buf), FARPTR_OFF(cs->stack_buf),
           cs->sp - cs->stack_buf,
           U64_HI(cs->ret_val), U64_LO(cs->ret_val));
    return 0; /* 表示函数结束 */

exception:
    cs->exception = 1;
    return -1;
}

/* ========== do_misc：杂项操作 ========== */
int do_misc(CallState *cs, int op)
{
    JSContext *ctx = cs->ctx;
    JSStackFrame *sf = cs->sf;
    const uint8_t *pc = cs->pc;
    JSValue *sp = cs->sp;
    JSValue ret_val;
    JSAtom atom;
    int res;

    switch (op) {
    case OP_regexp:
        {
            sp[-2] = JS_NewRegexp(ctx, sp[-2], sp[-1]);
            sp--;
            if (JS_IsException(sp[-1]))
                goto exception;
        }
        break;
    case OP_to_object:
        if (JS_VALUE_GET_TAG(sp[-1]) != JS_TAG_OBJECT) {
            sf->cur_pc = pc;
            ret_val = JS_ToObject(ctx, sp[-1]);
            if (JS_IsException(ret_val))
                goto exception;
            JS_FreeValue(ctx, sp[-1]);
            sp[-1] = ret_val;
        }
        break;
    case OP_to_propkey:
        switch (JS_VALUE_GET_TAG(sp[-1])) {
        case JS_TAG_INT:
        case JS_TAG_STRING:
        case JS_TAG_SYMBOL:
            break;
        default:
            sf->cur_pc = pc;
            ret_val = JS_ToPropertyKey(ctx, sp[-1]);
            if (JS_IsException(ret_val))
                goto exception;
            JS_FreeValue(ctx, sp[-1]);
            sp[-1] = ret_val;
            break;
        }
        break;
    case OP_is_undefined_or_null:
        if (JS_VALUE_GET_TAG(sp[-1]) == JS_TAG_UNDEFINED ||
            JS_VALUE_GET_TAG(sp[-1]) == JS_TAG_NULL) {
            sp[-1] = JS_TRUE;
        } else {
            JS_FreeValue(ctx, sp[-1]);
            sp[-1] = JS_FALSE;
        }
        break;
#if SHORT_OPCODES
    case OP_is_undefined:
        if (JS_VALUE_GET_TAG(sp[-1]) == JS_TAG_UNDEFINED) {
            sp[-1] = JS_TRUE;
        } else {
            JS_FreeValue(ctx, sp[-1]);
            sp[-1] = JS_FALSE;
        }
        break;
    case OP_is_null:
        if (JS_VALUE_GET_TAG(sp[-1]) == JS_TAG_NULL) {
            sp[-1] = JS_TRUE;
        } else {
            JS_FreeValue(ctx, sp[-1]);
            sp[-1] = JS_FALSE;
        }
        break;
    case OP_typeof_is_undefined:
        if (js_operator_typeof(ctx, sp[-1]) == JS_ATOM_undefined) {
            JS_FreeValue(ctx, sp[-1]);
            sp[-1] = JS_TRUE;
        } else {
            JS_FreeValue(ctx, sp[-1]);
            sp[-1] = JS_FALSE;
        }
        break;
    case OP_typeof_is_function:
        if (js_operator_typeof(ctx, sp[-1]) == JS_ATOM_function) {
            JS_FreeValue(ctx, sp[-1]);
            sp[-1] = JS_TRUE;
        } else {
            JS_FreeValue(ctx, sp[-1]);
            sp[-1] = JS_FALSE;
        }
        break;
#endif
    case OP_await:
        ret_val = JS_NewInt32(ctx, FUNC_RET_AWAIT);
        goto done_generator;
    case OP_yield:
        ret_val = JS_NewInt32(ctx, FUNC_RET_YIELD);
        goto done_generator;
    case OP_yield_star:
    case OP_async_yield_star:
        ret_val = JS_NewInt32(ctx, FUNC_RET_YIELD_STAR);
        goto done_generator;
    case OP_return_async:
        ret_val = JS_UNDEFINED;
        goto done_generator;
    case OP_initial_yield:
        ret_val = JS_NewInt32(ctx, FUNC_RET_INITIAL_YIELD);
        goto done_generator;
    case OP_nop:
        break;
    case OP_invalid:
    default:
        JS_ThrowInternalError(ctx, "invalid opcode");
        goto exception;
    }

    cs->pc = pc;
    cs->sp = sp;
    return 0;

done_generator:
    cs->ret_val = ret_val;
    cs->exception = 0;
    return 0; /* 表示生成器返回 */

exception:
    cs->exception = 1;
    return -1;
}
