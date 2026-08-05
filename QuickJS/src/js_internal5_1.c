#include "js_internal5.h"


/*
 * 辅助函数 do_push, do_stack, do_arith
 * 实现推入、栈操作和算术运算的操作码
 */

/* ========== do_push：推入操作 ========== */
int do_push(CallState *cs, int op)
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
    JSValue *stack_buf = cs->stack_buf;
    int argc = cs->argc;
    JSValue *argv = cs->argv;
    JSValue this_obj = cs->this_obj;
    JSValue new_target = cs->new_target;
    int flags = cs->flags;
    JSValue ret_val;

    switch (op) {
#if SHORT_OPCODES
    case OP_push_minus1:
    case OP_push_0:
    case OP_push_1:
    case OP_push_2:
    case OP_push_3:
    case OP_push_4:
    case OP_push_5:
    case OP_push_6:
    case OP_push_7:
        *sp++ = JS_NewInt32(ctx, op - OP_push_0);
        break;
    case OP_push_i8:
        *sp++ = JS_NewInt32(ctx, get_i8(pc));
        pc += 1;
        break;
    case OP_push_i16:
        *sp++ = JS_NewInt32(ctx, get_i16(pc));
        pc += 2;
        break;
    case OP_push_const8:
        *sp++ = JS_DupValue(ctx, b->cpool[*pc++]);
        break;
    case OP_fclosure8:
        *sp++ = js_closure(ctx, JS_DupValue(ctx, b->cpool[*pc++]), var_refs, sf, FALSE);
        if (unlikely(JS_IsException(sp[-1])))
            goto exception;
        break;
    case OP_push_empty_string:
        *sp++ = JS_AtomToString(ctx, JS_ATOM_empty_string);
        break;
#endif
    case OP_push_i32:
        *sp++ = JS_NewInt32(ctx, get_u32(pc));
        pc += 4;
        break;
    case OP_push_bigint_i32:
        *sp++ = __JS_NewShortBigInt(ctx, (int)get_u32(pc));
        pc += 4;
        break;
    case OP_push_const:
        *sp++ = JS_DupValue(ctx, b->cpool[get_u32(pc)]);
        pc += 4;
        break;
    case OP_push_atom_value:
        *sp++ = JS_AtomToValue(ctx, get_u32(pc));
        pc += 4;
        break;
    case OP_undefined:
        *sp++ = JS_UNDEFINED;
        break;
    case OP_null:
        *sp++ = JS_NULL;
        break;
    case OP_push_false:
        *sp++ = JS_FALSE;
        break;
    case OP_push_true:
        *sp++ = JS_TRUE;
        break;
    case OP_push_this:
        {
            JSValue val;
            if (!(b->js_mode & JS_MODE_STRICT)) {
                uint32_t tag = JS_VALUE_GET_TAG(this_obj);
                if (likely(tag == JS_TAG_OBJECT))
                    goto normal_this;
                if (tag == JS_TAG_NULL || tag == JS_TAG_UNDEFINED) {
                    val = JS_DupValue(ctx, ctx->global_obj);
                } else {
                    val = JS_ToObject(ctx, this_obj);
                    if (JS_IsException(val))
                        goto exception;
                }
            } else {
            normal_this:
                val = JS_DupValue(ctx, this_obj);
            }
            *sp++ = val;
        }
        break;
    case OP_object:
        *sp++ = JS_NewObject(ctx);
        if (unlikely(JS_IsException(sp[-1])))
            goto exception;
        break;
    case OP_special_object:
        {
            int arg = *pc++;
            switch(arg) {
            case OP_SPECIAL_OBJECT_ARGUMENTS:
                *sp++ = js_build_arguments(ctx, argc, (JSValueConst *)argv);
                if (unlikely(JS_IsException(sp[-1])))
                    goto exception;
                break;
            case OP_SPECIAL_OBJECT_MAPPED_ARGUMENTS:
                *sp++ = js_build_mapped_arguments(ctx, argc, (JSValueConst *)argv,
                                                  sf, min_int(argc, b->arg_count));
                if (unlikely(JS_IsException(sp[-1])))
                    goto exception;
                break;
            case OP_SPECIAL_OBJECT_THIS_FUNC:
                *sp++ = JS_DupValue(ctx, sf->cur_func);
                break;
            case OP_SPECIAL_OBJECT_NEW_TARGET:
                *sp++ = JS_DupValue(ctx, new_target);
                break;
            case OP_SPECIAL_OBJECT_HOME_OBJECT:
                {
                    JSObject *p1;
                    p1 = JS_VALUE_GET_OBJ(sf->cur_func)->u.func.home_object;
                    if (unlikely(!p1))
                        *sp++ = JS_UNDEFINED;
                    else
                        *sp++ = JS_DupValue(ctx, JS_MKPTR(JS_TAG_OBJECT, p1));
                }
                break;
            case OP_SPECIAL_OBJECT_VAR_OBJECT:
                *sp++ = JS_NewObjectProto(ctx, JS_NULL);
                if (unlikely(JS_IsException(sp[-1])))
                    goto exception;
                break;
            case OP_SPECIAL_OBJECT_IMPORT_META:
                *sp++ = js_import_meta(ctx);
                if (unlikely(JS_IsException(sp[-1])))
                    goto exception;
                break;
            default:
                abort();
            }
        }
        break;
    case OP_rest:
        {
            int first = get_u16(pc);
            pc += 2;
            first = min_int(first, argc);
            *sp++ = js_create_array(ctx, argc - first, (JSValueConst *)(argv + first));
            if (unlikely(JS_IsException(sp[-1])))
                goto exception;
        }
        break;
    case OP_fclosure:
        {
            JSValue bfunc = JS_DupValue(ctx, b->cpool[get_u32(pc)]);
            pc += 4;
            *sp++ = js_closure(ctx, bfunc, var_refs, sf, FALSE);
            if (unlikely(JS_IsException(sp[-1])))
                goto exception;
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

/* ========== do_stack：栈操作 ========== */
int do_stack(CallState *cs, int op)
{
    JSContext *ctx = cs->ctx;
    JSValue *sp = cs->sp;

    switch (op) {
    case OP_drop:
        JS_FreeValue(ctx, sp[-1]);
        sp--;
        break;
    case OP_nip:
        JS_FreeValue(ctx, sp[-2]);
        sp[-2] = sp[-1];
        sp--;
        break;
    case OP_nip1:
        JS_FreeValue(ctx, sp[-3]);
        sp[-3] = sp[-2];
        sp[-2] = sp[-1];
        sp--;
        break;
    case OP_dup:
        sp[0] = JS_DupValue(ctx, sp[-1]);
        sp++;
        break;
    case OP_dup2:
        sp[0] = JS_DupValue(ctx, sp[-2]);
        sp[1] = JS_DupValue(ctx, sp[-1]);
        sp += 2;
        break;
    case OP_dup3:
        sp[0] = JS_DupValue(ctx, sp[-3]);
        sp[1] = JS_DupValue(ctx, sp[-2]);
        sp[2] = JS_DupValue(ctx, sp[-1]);
        sp += 3;
        break;
    case OP_dup1:
        sp[0] = sp[-1];
        sp[-1] = JS_DupValue(ctx, sp[-2]);
        sp++;
        break;
    case OP_insert2:
        sp[0] = sp[-1];
        sp[-1] = sp[-2];
        sp[-2] = JS_DupValue(ctx, sp[0]);
        sp++;
        break;
    case OP_insert3:
        sp[0] = sp[-1];
        sp[-1] = sp[-2];
        sp[-2] = sp[-3];
        sp[-3] = JS_DupValue(ctx, sp[0]);
        sp++;
        break;
    case OP_insert4:
        sp[0] = sp[-1];
        sp[-1] = sp[-2];
        sp[-2] = sp[-3];
        sp[-3] = sp[-4];
        sp[-4] = JS_DupValue(ctx, sp[0]);
        sp++;
        break;
    case OP_perm3:
        {
            JSValue tmp = sp[-2];
            sp[-2] = sp[-3];
            sp[-3] = tmp;
        }
        break;
    case OP_rot3l:
        {
            JSValue tmp = sp[-3];
            sp[-3] = sp[-2];
            sp[-2] = sp[-1];
            sp[-1] = tmp;
        }
        break;
    case OP_rot4l:
        {
            JSValue tmp = sp[-4];
            sp[-4] = sp[-3];
            sp[-3] = sp[-2];
            sp[-2] = sp[-1];
            sp[-1] = tmp;
        }
        break;
    case OP_rot5l:
        {
            JSValue tmp = sp[-5];
            sp[-5] = sp[-4];
            sp[-4] = sp[-3];
            sp[-3] = sp[-2];
            sp[-2] = sp[-1];
            sp[-1] = tmp;
        }
        break;
    case OP_rot3r:
        {
            JSValue tmp = sp[-1];
            sp[-1] = sp[-2];
            sp[-2] = sp[-3];
            sp[-3] = tmp;
        }
        break;
    case OP_perm4:
        {
            JSValue tmp = sp[-2];
            sp[-2] = sp[-3];
            sp[-3] = sp[-4];
            sp[-4] = tmp;
        }
        break;
    case OP_perm5:
        {
            JSValue tmp = sp[-2];
            sp[-2] = sp[-3];
            sp[-3] = sp[-4];
            sp[-4] = sp[-5];
            sp[-5] = tmp;
        }
        break;
    case OP_swap:
        {
            JSValue tmp = sp[-2];
            sp[-2] = sp[-1];
            sp[-1] = tmp;
        }
        break;
    case OP_swap2:
        {
            JSValue tmp1 = sp[-4], tmp2 = sp[-3];
            sp[-4] = sp[-2];
            sp[-3] = sp[-1];
            sp[-2] = tmp1;
            sp[-1] = tmp2;
        }
        break;
    default:
        abort();
    }

    cs->sp = sp;
    return 0;
}

/* ========== do_arith：算术和位运算 ========== */
int do_arith(CallState *cs, int op)
{
    JSContext *ctx = cs->ctx;
    JSStackFrame *sf = cs->sf;
    JSValue *sp = cs->sp;
    JSValue *var_buf = cs->var_buf;
    JSValue ret_val;
    int idx;

    switch (op) {
    case OP_add:
        {
            JSValue op1 = sp[-2], op2 = sp[-1];
            if (likely(JS_VALUE_IS_BOTH_INT(op1, op2))) {
                int64_t r = (int64_t)JS_VALUE_GET_INT(op1) + JS_VALUE_GET_INT(op2);
                if (unlikely((int)r != r))
                    sp[-2] = __JS_NewFloat64(ctx, (double)r);
                else
                    sp[-2] = JS_NewInt32(ctx, r);
                sp--;
            } else if (JS_TAG_IS_FLOAT64(JS_VALUE_GET_TAG(op1)) ||
                       JS_TAG_IS_FLOAT64(JS_VALUE_GET_TAG(op2))) {
                double d1, d2;
                if (JS_TAG_IS_FLOAT64(JS_VALUE_GET_TAG(op1)))
                    d1 = JS_VALUE_GET_FLOAT64(op1);
                else if (JS_VALUE_GET_TAG(op1) == JS_TAG_INT)
                    d1 = JS_VALUE_GET_INT(op1);
                else
                    goto add_slow;
                if (JS_TAG_IS_FLOAT64(JS_VALUE_GET_TAG(op2)))
                    d2 = JS_VALUE_GET_FLOAT64(op2);
                else if (JS_VALUE_GET_TAG(op2) == JS_TAG_INT)
                    d2 = JS_VALUE_GET_INT(op2);
                else
                    goto add_slow;
                sp[-2] = __JS_NewFloat64(ctx, d1 + d2);
                sp--;
            } else if (JS_IsString(op1) && JS_IsString(op2)) {
                JS_LOG("OP_add", "Before concat: cs->sp=%04X:%04X, sf->cur_sp=%04X:%04X",
                       FARPTR_SEG(cs->sp), FARPTR_OFF(cs->sp),
                       sf->cur_sp ? FARPTR_SEG(sf->cur_sp) : 0, sf->cur_sp ? FARPTR_OFF(sf->cur_sp) : 0);
                JSValue concat_result = JS_ConcatString(ctx, op1, op2);
                JS_LOG("OP_add", "After concat: cs->sp=%04X:%04X, sf->cur_sp=%04X:%04X",
                       FARPTR_SEG(cs->sp), FARPTR_OFF(cs->sp),
                       sf->cur_sp ? FARPTR_SEG(sf->cur_sp) : 0, sf->cur_sp ? FARPTR_OFF(sf->cur_sp) : 0);
                JS_LOG("OP_add", "JS_ConcatString returned %08lX_%08lX",
                       U64_HI(concat_result), U64_LO(concat_result));
                sp[-2] = concat_result;
                sp--;
                JS_LOG("OP_add", "After string concat, sp[0]=%08lX_%08lX, var_refs[1]=%08lX_%08lX",
                       U64_HI(sp[0]), U64_LO(sp[0]),
                       U64_HI(*cs->var_refs[1]->pvalue), U64_LO(*cs->var_refs[1]->pvalue));
                if (JS_IsException(sp[-1]))
                    goto exception;
            } else {
            add_slow:
                sf->cur_pc = cs->pc;
                if (js_add_slow(ctx, sp))
                    goto exception;
                sp--;
            }
        }
        break;
    case OP_add_loc:
        {
            idx = *cs->pc;
            cs->pc += 1;
            JSValue op2 = sp[-1];
            JSValue *pv = &var_buf[idx];
            if (likely(JS_VALUE_IS_BOTH_INT(*pv, op2))) {
                int64_t r = (int64_t)JS_VALUE_GET_INT(*pv) + JS_VALUE_GET_INT(op2);
                if (unlikely((int)r != r))
                    *pv = __JS_NewFloat64(ctx, (double)r);
                else
                    *pv = JS_NewInt32(ctx, r);
                sp--;
            } else if (JS_VALUE_IS_BOTH_FLOAT(*pv, op2)) {
                *pv = __JS_NewFloat64(ctx, JS_VALUE_GET_FLOAT64(*pv) +
                                      JS_VALUE_GET_FLOAT64(op2));
                sp--;
            } else if (JS_VALUE_GET_TAG(*pv) == JS_TAG_STRING &&
                       JS_VALUE_GET_TAG(op2) == JS_TAG_STRING) {
                sp--;
                sf->cur_pc = cs->pc;
                if (JS_ConcatStringInPlace(ctx, JS_VALUE_GET_STRING(*pv), op2)) {
                    JS_FreeValue(ctx, op2);
                } else {
                    op2 = JS_ConcatString(ctx, JS_DupValue(ctx, *pv), op2);
                    if (JS_IsException(op2))
                        goto exception;
                    set_value(ctx, pv, op2);
                }
            } else {
                JSValue ops[2];
                sf->cur_pc = cs->pc;
                ops[0] = JS_DupValue(ctx, *pv);
                ops[1] = op2;
                sp--;
                if (js_add_slow(ctx, ops + 2))
                    goto exception;
                set_value(ctx, pv, ops[0]);
            }
        }
        break;
    case OP_sub:
        {
            JSValue op1 = sp[-2], op2 = sp[-1];
            if (likely(JS_VALUE_IS_BOTH_INT(op1, op2))) {
                int64_t r = (int64_t)JS_VALUE_GET_INT(op1) - JS_VALUE_GET_INT(op2);
                if (unlikely((int)r != r))
                    sp[-2] = __JS_NewFloat64(ctx, (double)r);
                else
                    sp[-2] = JS_NewInt32(ctx, r);
                sp--;
            } else if (JS_TAG_IS_FLOAT64(JS_VALUE_GET_TAG(op1)) ||
                       JS_TAG_IS_FLOAT64(JS_VALUE_GET_TAG(op2))) {
                double d1, d2;
                if (JS_TAG_IS_FLOAT64(JS_VALUE_GET_TAG(op1)))
                    d1 = JS_VALUE_GET_FLOAT64(op1);
                else if (JS_VALUE_GET_TAG(op1) == JS_TAG_INT)
                    d1 = JS_VALUE_GET_INT(op1);
                else
                    goto binary_arith_slow;
                if (JS_TAG_IS_FLOAT64(JS_VALUE_GET_TAG(op2)))
                    d2 = JS_VALUE_GET_FLOAT64(op2);
                else if (JS_VALUE_GET_TAG(op2) == JS_TAG_INT)
                    d2 = JS_VALUE_GET_INT(op2);
                else
                    goto binary_arith_slow;
                sp[-2] = __JS_NewFloat64(ctx, d1 - d2);
                sp--;
            } else {
                goto binary_arith_slow;
            }
        }
        break;
    case OP_mul:
        {
            JSValue op1 = sp[-2], op2 = sp[-1];
            double d;
            if (likely(JS_VALUE_IS_BOTH_INT(op1, op2))) {
                int32_t v1 = JS_VALUE_GET_INT(op1), v2 = JS_VALUE_GET_INT(op2);
                int64_t r = (int64_t)v1 * v2;
                if (unlikely((int)r != r)) {
                    d = (double)r;
                    goto mul_fp_res;
                }
                if (unlikely(r == 0 && (v1 | v2) < 0)) {
                    d = -0.0;
                    goto mul_fp_res;
                }
                sp[-2] = JS_NewInt32(ctx, r);
                sp--;
            } else if (JS_TAG_IS_FLOAT64(JS_VALUE_GET_TAG(op1)) ||
                       JS_TAG_IS_FLOAT64(JS_VALUE_GET_TAG(op2))) {
                double d1, d2;
                if (JS_TAG_IS_FLOAT64(JS_VALUE_GET_TAG(op1)))
                    d1 = JS_VALUE_GET_FLOAT64(op1);
                else if (JS_VALUE_GET_TAG(op1) == JS_TAG_INT)
                    d1 = JS_VALUE_GET_INT(op1);
                else
                    goto binary_arith_slow;
                if (JS_TAG_IS_FLOAT64(JS_VALUE_GET_TAG(op2)))
                    d2 = JS_VALUE_GET_FLOAT64(op2);
                else if (JS_VALUE_GET_TAG(op2) == JS_TAG_INT)
                    d2 = JS_VALUE_GET_INT(op2);
                else
                    goto binary_arith_slow;
                d = d1 * d2;
            mul_fp_res:
                sp[-2] = __JS_NewFloat64(ctx, d);
                sp--;
            } else {
                goto binary_arith_slow;
            }
        }
        break;
    case OP_div:
        {
            JSValue op1 = sp[-2], op2 = sp[-1];
            if (likely(JS_VALUE_IS_BOTH_INT(op1, op2))) {
                int v1 = JS_VALUE_GET_INT(op1), v2 = JS_VALUE_GET_INT(op2);
                sp[-2] = JS_NewFloat64(ctx, (double)v1 / (double)v2);
                sp--;
            } else {
                goto binary_arith_slow;
            }
        }
        break;
    case OP_mod:
        {
            JSValue op1 = sp[-2], op2 = sp[-1];
            if (likely(JS_VALUE_IS_BOTH_INT(op1, op2))) {
                int v1 = JS_VALUE_GET_INT(op1), v2 = JS_VALUE_GET_INT(op2);
                if (unlikely(v1 < 0 || v2 <= 0))
                    goto binary_arith_slow;
                sp[-2] = JS_NewInt32(ctx, v1 % v2);
                sp--;
            } else {
                goto binary_arith_slow;
            }
        }
        break;
    case OP_pow:
    binary_arith_slow:
        sf->cur_pc = cs->pc;
        if (js_binary_arith_slow(ctx, sp, op))
            goto exception;
        sp--;
        break;
    case OP_plus:
        {
            JSValue op1 = sp[-1];
            uint32_t tag = JS_VALUE_GET_TAG(op1);
            if (tag == JS_TAG_INT || JS_TAG_IS_FLOAT64(tag)) {
                // nothing
            } else if (tag == JS_TAG_NULL || tag == JS_TAG_BOOL) {
                sp[-1] = JS_NewInt32(ctx, JS_VALUE_GET_INT(op1));
            } else {
                sf->cur_pc = cs->pc;
                if (js_unary_arith_slow(ctx, sp, op))
                    goto exception;
            }
        }
        break;
    case OP_neg:
        {
            JSValue op1 = sp[-1];
            uint32_t tag = JS_VALUE_GET_TAG(op1);
            int val;
            double d;
            if (tag == JS_TAG_INT || tag == JS_TAG_BOOL || tag == JS_TAG_NULL) {
                val = JS_VALUE_GET_INT(op1);
                if (unlikely(val == 0)) {
                    d = -0.0;
                    goto neg_fp_res;
                }
                if (unlikely(val == INT32_MIN)) {
                    d = -(double)val;
                    goto neg_fp_res;
                }
                sp[-1] = JS_NewInt32(ctx, -val);
            } else if (JS_TAG_IS_FLOAT64(tag)) {
                d = -JS_VALUE_GET_FLOAT64(op1);
            neg_fp_res:
                sp[-1] = __JS_NewFloat64(ctx, d);
            } else {
                sf->cur_pc = cs->pc;
                if (js_unary_arith_slow(ctx, sp, op))
                    goto exception;
            }
        }
        break;
    case OP_inc:
        {
            JSValue op1 = sp[-1];
            if (JS_VALUE_GET_TAG(op1) == JS_TAG_INT) {
                int val = JS_VALUE_GET_INT(op1);
                if (unlikely(val == INT32_MAX))
                    goto inc_slow;
                sp[-1] = JS_NewInt32(ctx, val + 1);
            } else {
            inc_slow:
                sf->cur_pc = cs->pc;
                if (js_unary_arith_slow(ctx, sp, op))
                    goto exception;
            }
        }
        break;
    case OP_dec:
        {
            JSValue op1 = sp[-1];
            if (JS_VALUE_GET_TAG(op1) == JS_TAG_INT) {
                int val = JS_VALUE_GET_INT(op1);
                if (unlikely(val == INT32_MIN))
                    goto dec_slow;
                sp[-1] = JS_NewInt32(ctx, val - 1);
            } else {
            dec_slow:
                sf->cur_pc = cs->pc;
                if (js_unary_arith_slow(ctx, sp, op))
                    goto exception;
            }
        }
        break;
    case OP_post_inc:
        {
            JSValue op1 = sp[-1];
            if (JS_VALUE_GET_TAG(op1) == JS_TAG_INT) {
                int val = JS_VALUE_GET_INT(op1);
                if (unlikely(val == INT32_MAX))
                    goto post_inc_slow;
                sp[0] = JS_NewInt32(ctx, val + 1);
            } else {
            post_inc_slow:
                sf->cur_pc = cs->pc;
                if (js_post_inc_slow(ctx, sp, op))
                    goto exception;
            }
            sp++;
        }
        break;
    case OP_post_dec:
        {
            JSValue op1 = sp[-1];
            if (JS_VALUE_GET_TAG(op1) == JS_TAG_INT) {
                int val = JS_VALUE_GET_INT(op1);
                if (unlikely(val == INT32_MIN))
                    goto post_dec_slow;
                sp[0] = JS_NewInt32(ctx, val - 1);
            } else {
            post_dec_slow:
                sf->cur_pc = cs->pc;
                if (js_post_inc_slow(ctx, sp, op))
                    goto exception;
            }
            sp++;
        }
        break;
    case OP_inc_loc:
        {
            idx = *cs->pc;
            cs->pc += 1;
            JSValue op1 = var_buf[idx];
            if (JS_VALUE_GET_TAG(op1) == JS_TAG_INT) {
                int val = JS_VALUE_GET_INT(op1);
                if (unlikely(val == INT32_MAX))
                    goto inc_loc_slow;
                var_buf[idx] = JS_NewInt32(ctx, val + 1);
            } else {
            inc_loc_slow:
                sf->cur_pc = cs->pc;
                op1 = JS_DupValue(ctx, op1);
                if (js_unary_arith_slow(ctx, &op1 + 1, OP_inc))
                    goto exception;
                set_value(ctx, &var_buf[idx], op1);
            }
        }
        break;
    case OP_dec_loc:
        {
            idx = *cs->pc;
            cs->pc += 1;
            JSValue op1 = var_buf[idx];
            if (JS_VALUE_GET_TAG(op1) == JS_TAG_INT) {
                int val = JS_VALUE_GET_INT(op1);
                if (unlikely(val == INT32_MIN))
                    goto dec_loc_slow;
                var_buf[idx] = JS_NewInt32(ctx, val - 1);
            } else {
            dec_loc_slow:
                sf->cur_pc = cs->pc;
                op1 = JS_DupValue(ctx, op1);
                if (js_unary_arith_slow(ctx, &op1 + 1, OP_dec))
                    goto exception;
                set_value(ctx, &var_buf[idx], op1);
            }
        }
        break;
    case OP_not:
        {
            JSValue op1 = sp[-1];
            if (JS_VALUE_GET_TAG(op1) == JS_TAG_INT) {
                sp[-1] = JS_NewInt32(ctx, ~JS_VALUE_GET_INT(op1));
            } else {
                sf->cur_pc = cs->pc;
                if (js_not_slow(ctx, sp))
                    goto exception;
            }
        }
        break;
    case OP_shl:
        {
            JSValue op1 = sp[-2], op2 = sp[-1];
            if (likely(JS_VALUE_IS_BOTH_INT(op1, op2))) {
                uint32_t v1 = JS_VALUE_GET_INT(op1), v2 = JS_VALUE_GET_INT(op2) & 0x1f;
                sp[-2] = JS_NewInt32(ctx, v1 << v2);
                sp--;
            } else {
                sf->cur_pc = cs->pc;
                if (js_binary_logic_slow(ctx, sp, op))
                    goto exception;
                sp--;
            }
        }
        break;
    case OP_shr:
        {
            JSValue op1 = sp[-2], op2 = sp[-1];
            if (likely(JS_VALUE_IS_BOTH_INT(op1, op2))) {
                uint32_t v2 = JS_VALUE_GET_INT(op2) & 0x1f;
                sp[-2] = JS_NewUint32(ctx, (uint32_t)JS_VALUE_GET_INT(op1) >> v2);
                sp--;
            } else {
                sf->cur_pc = cs->pc;
                if (js_shr_slow(ctx, sp))
                    goto exception;
                sp--;
            }
        }
        break;
    case OP_sar:
        {
            JSValue op1 = sp[-2], op2 = sp[-1];
            if (likely(JS_VALUE_IS_BOTH_INT(op1, op2))) {
                uint32_t v2 = JS_VALUE_GET_INT(op2) & 0x1f;
                sp[-2] = JS_NewInt32(ctx, (int)JS_VALUE_GET_INT(op1) >> v2);
                sp--;
            } else {
                sf->cur_pc = cs->pc;
                if (js_binary_logic_slow(ctx, sp, op))
                    goto exception;
                sp--;
            }
        }
        break;
    case OP_and:
        {
            JSValue op1 = sp[-2], op2 = sp[-1];
            if (likely(JS_VALUE_IS_BOTH_INT(op1, op2))) {
                sp[-2] = JS_NewInt32(ctx, JS_VALUE_GET_INT(op1) & JS_VALUE_GET_INT(op2));
                sp--;
            } else {
                sf->cur_pc = cs->pc;
                if (js_binary_logic_slow(ctx, sp, op))
                    goto exception;
                sp--;
            }
        }
        break;
    case OP_or:
        {
            JSValue op1 = sp[-2], op2 = sp[-1];
            if (likely(JS_VALUE_IS_BOTH_INT(op1, op2))) {
                sp[-2] = JS_NewInt32(ctx, JS_VALUE_GET_INT(op1) | JS_VALUE_GET_INT(op2));
                sp--;
            } else {
                sf->cur_pc = cs->pc;
                if (js_binary_logic_slow(ctx, sp, op))
                    goto exception;
                sp--;
            }
        }
        break;
    case OP_xor:
        {
            JSValue op1 = sp[-2], op2 = sp[-1];
            if (likely(JS_VALUE_IS_BOTH_INT(op1, op2))) {
                sp[-2] = JS_NewInt32(ctx, JS_VALUE_GET_INT(op1) ^ JS_VALUE_GET_INT(op2));
                sp--;
            } else {
                sf->cur_pc = cs->pc;
                if (js_binary_logic_slow(ctx, sp, op))
                    goto exception;
                sp--;
            }
        }
        break;
    default:
        abort();
    }

    cs->sp = sp;
    return 0;

exception:
    cs->exception = 1;
    return -1;
}

/*
 * 辅助函数 do_compare, do_call, do_object
 * 实现比较/逻辑运算、函数调用、对象操作的操作码
 */

/* ========== do_compare：比较/逻辑运算 ========== */
int do_compare(CallState *cs, int op)
{
    JSContext *ctx = cs->ctx;
    JSStackFrame *sf = cs->sf;
    JSValue *sp = cs->sp;
    JSValue op1, op2;
    int res, ret;
    JSAtom atom;
    JSValue ret_val;

    switch (op) {
    case OP_lt:
    case OP_lte:
    case OP_gt:
    case OP_gte:
        op1 = sp[-2];
        op2 = sp[-1];
        if (likely(JS_VALUE_IS_BOTH_INT(op1, op2))) {
            switch (op) {
            case OP_lt: res = JS_VALUE_GET_INT(op1) < JS_VALUE_GET_INT(op2); break;
            case OP_lte: res = JS_VALUE_GET_INT(op1) <= JS_VALUE_GET_INT(op2); break;
            case OP_gt: res = JS_VALUE_GET_INT(op1) > JS_VALUE_GET_INT(op2); break;
            case OP_gte: res = JS_VALUE_GET_INT(op1) >= JS_VALUE_GET_INT(op2); break;
            default: res = 0; break;
            }
            sp[-2] = JS_NewBool(ctx, res);
            sp--;
        } else if (JS_TAG_IS_FLOAT64(JS_VALUE_GET_TAG(op1)) ||
                   JS_TAG_IS_FLOAT64(JS_VALUE_GET_TAG(op2))) {
            double d1, d2;
            if (JS_TAG_IS_FLOAT64(JS_VALUE_GET_TAG(op1)))
                d1 = JS_VALUE_GET_FLOAT64(op1);
            else if (JS_VALUE_GET_TAG(op1) == JS_TAG_INT)
                d1 = JS_VALUE_GET_INT(op1);
            else
                goto relational_slow;
            if (JS_TAG_IS_FLOAT64(JS_VALUE_GET_TAG(op2)))
                d2 = JS_VALUE_GET_FLOAT64(op2);
            else if (JS_VALUE_GET_TAG(op2) == JS_TAG_INT)
                d2 = JS_VALUE_GET_INT(op2);
            else
                goto relational_slow;
            switch (op) {
            case OP_lt: res = d1 < d2; break;
            case OP_lte: res = d1 <= d2; break;
            case OP_gt: res = d1 > d2; break;
            case OP_gte: res = d1 >= d2; break;
            default: res = 0; break;
            }
            sp[-2] = JS_NewBool(ctx, res);
            sp--;
        } else {
        relational_slow:
            sf->cur_pc = cs->pc;
            if (js_relational_slow(ctx, sp, op))
                goto exception;
            sp--;
        }
        break;
    case OP_eq:
    case OP_neq:
        {
            int inv = (op == OP_neq);
            op1 = sp[-2];
            op2 = sp[-1];
            uint32_t tag1 = JS_VALUE_GET_TAG(op1);
            uint32_t tag2 = JS_VALUE_GET_TAG(op2);
            if (likely(tag1 == JS_TAG_INT)) {
                if (tag2 == JS_TAG_INT) {
                    res = JS_VALUE_GET_INT(op1) == JS_VALUE_GET_INT(op2);
                } else if (JS_TAG_IS_FLOAT64(tag2)) {
                    res = (JS_VALUE_GET_INT(op1) == JS_VALUE_GET_FLOAT64(op2));
                } else {
                    goto slow_eq;
                }
            } else if (JS_TAG_IS_FLOAT64(tag1)) {
                if (tag2 == JS_TAG_INT) {
                    res = JS_VALUE_GET_FLOAT64(op1) == JS_VALUE_GET_INT(op2);
                } else if (JS_TAG_IS_FLOAT64(tag2)) {
                    res = (JS_VALUE_GET_FLOAT64(op1) == JS_VALUE_GET_FLOAT64(op2));
                } else {
                    goto slow_eq;
                }
            } else if (tag1 == JS_TAG_OBJECT) {
                if (tag2 == JS_TAG_NULL || tag2 == JS_TAG_UNDEFINED) {
                    JSObject *p = JS_VALUE_GET_OBJ(op1);
                    res = p->is_HTMLDDA;
                    JS_FreeValue(ctx, op1);
                } else if (tag2 == JS_TAG_OBJECT) {
                    res = (JS_VALUE_GET_OBJ(op1) == JS_VALUE_GET_OBJ(op2));
                    JS_FreeValue(ctx, op1);
                    JS_FreeValue(ctx, op2);
                } else {
                    goto slow_eq;
                }
            } else if (tag1 == JS_TAG_NULL || tag1 == JS_TAG_UNDEFINED) {
                if (tag2 == JS_TAG_NULL || tag2 == JS_TAG_UNDEFINED) {
                    res = TRUE;
                } else if (tag2 == JS_TAG_OBJECT) {
                    JSObject *p = JS_VALUE_GET_OBJ(op2);
                    res = p->is_HTMLDDA;
                    JS_FreeValue(ctx, op2);
                } else {
                    goto slow_eq;
                }
            } else if (tag1 == JS_TAG_STRING && tag2 == JS_TAG_STRING) {
                res = js_string_eq(ctx, JS_VALUE_GET_STRING(op1),
                                   JS_VALUE_GET_STRING(op2));
                JS_FreeValue(ctx, op1);
                JS_FreeValue(ctx, op2);
            } else {
                goto slow_eq;
            }
            sp[-2] = JS_NewBool(ctx, res ^ inv);
            sp--;
            break;
        slow_eq:
            sf->cur_pc = cs->pc;
            if (js_eq_slow(ctx, sp, inv))
                goto exception;
            sp--;
            break;
        }
    case OP_strict_eq:
    case OP_strict_neq:
        {
            int inv = (op == OP_strict_neq);
            op1 = sp[-2];
            op2 = sp[-1];
            uint32_t tag1 = JS_VALUE_GET_TAG(op1);
            uint32_t tag2 = JS_VALUE_GET_TAG(op2);
            if (likely(tag1 == JS_TAG_INT)) {
                if (tag2 == JS_TAG_INT) {
                    res = JS_VALUE_GET_INT(op1) == JS_VALUE_GET_INT(op2);
                } else if (JS_TAG_IS_FLOAT64(tag2)) {
                    res = (JS_VALUE_GET_INT(op1) == JS_VALUE_GET_FLOAT64(op2));
                } else {
                    JS_FreeValue(ctx, op2);
                    res = FALSE;
                }
            } else if (JS_TAG_IS_FLOAT64(tag1)) {
                if (tag2 == JS_TAG_INT) {
                    res = JS_VALUE_GET_FLOAT64(op1) == JS_VALUE_GET_INT(op2);
                } else if (JS_TAG_IS_FLOAT64(tag2)) {
                    res = (JS_VALUE_GET_FLOAT64(op1) == JS_VALUE_GET_FLOAT64(op2));
                } else {
                    JS_FreeValue(ctx, op2);
                    res = FALSE;
                }
            } else if (tag1 == JS_TAG_OBJECT) {
                if (tag2 == JS_TAG_OBJECT) {
                    res = (JS_VALUE_GET_OBJ(op1) == JS_VALUE_GET_OBJ(op2));
                } else {
                    res = FALSE;
                }
                JS_FreeValue(ctx, op1);
                JS_FreeValue(ctx, op2);
            } else if (tag1 == JS_TAG_NULL || tag1 == JS_TAG_UNDEFINED) {
                res = (tag1 == tag2);
                JS_FreeValue(ctx, op2);
            } else if (tag1 == JS_TAG_STRING && tag2 == JS_TAG_STRING) {
                res = js_string_eq(ctx, JS_VALUE_GET_STRING(op1),
                                   JS_VALUE_GET_STRING(op2));
                JS_FreeValue(ctx, op1);
                JS_FreeValue(ctx, op2);
            } else {
                res = js_strict_eq2(ctx, op1, op2, JS_EQ_STRICT);
                JS_FreeValue(ctx, op1);
                JS_FreeValue(ctx, op2);
            }
            sp[-2] = JS_NewBool(ctx, res ^ inv);
            sp--;
        }
        break;
    case OP_in:
        sf->cur_pc = cs->pc;
        if (js_operator_in(ctx, sp))
            goto exception;
        sp--;
        break;
    case OP_private_in:
        sf->cur_pc = cs->pc;
        if (js_operator_private_in(ctx, sp))
            goto exception;
        sp--;
        break;
    case OP_instanceof:
        sf->cur_pc = cs->pc;
        if (js_operator_instanceof(ctx, sp))
            goto exception;
        sp--;
        break;
    case OP_typeof:
        {
            op1 = sp[-1];
            atom = js_operator_typeof(ctx, op1);
            JS_FreeValue(ctx, op1);
            sp[-1] = JS_AtomToString(ctx, atom);
        }
        break;
    case OP_delete:
        sf->cur_pc = cs->pc;
        if (js_operator_delete(ctx, sp))
            goto exception;
        sp--;
        break;
    case OP_delete_var:
        {
            atom = get_u32(cs->pc);
            cs->pc += 4;
            sf->cur_pc = cs->pc;
            ret = JS_DeleteGlobalVar(ctx, atom);
            if (unlikely(ret < 0))
                goto exception;
            *sp++ = JS_NewBool(ctx, ret);
        }
        break;
    case OP_lnot:
        {
            op1 = sp[-1];
            if ((uint32_t)JS_VALUE_GET_TAG(op1) <= JS_TAG_UNDEFINED) {
                res = JS_VALUE_GET_INT(op1) != 0;
            } else {
                res = JS_ToBoolFree(ctx, op1);
            }
            sp[-1] = JS_NewBool(ctx, !res);
        }
        break;
    default:
        abort();
    }

    cs->sp = sp;
    return 0;

exception:
    cs->exception = 1;
    return -1;
}

/* ========== do_call：函数调用 ========== */
int do_call(CallState *cs, int op)
{
    JSContext *ctx = cs->ctx;
    JSStackFrame *sf = cs->sf;
    JSFunctionBytecode *b = cs->b;
    const uint8_t *pc = cs->pc;
    JSValue *sp = cs->sp;
    JSValue ret_val;
    int call_argc, i, scope_idx;
    JSValue *call_argv;
    uint32_t len;
    JSValue *tab;
    JSValueConst obj;

    switch (op) {
#if SHORT_OPCODES
    case OP_call0:
    case OP_call1:
    case OP_call2:
    case OP_call3:
        call_argc = op - OP_call0;
        goto has_call_argc;
#endif
    case OP_call:
    case OP_tail_call:
        call_argc = get_u16(pc);
        pc += 2;
        goto has_call_argc;
    has_call_argc:
        call_argv = sp - call_argc;
        sf->cur_pc = pc;
        ret_val = JS_CallInternal(ctx, call_argv[-1], JS_UNDEFINED,
                                  JS_UNDEFINED, call_argc, call_argv, 0);
        if (unlikely(JS_IsException(ret_val)))
            goto exception;
        if (op == OP_tail_call)
            goto done;
        for(i = -1; i < call_argc; i++)
            JS_FreeValue(ctx, call_argv[i]);
        sp -= call_argc + 1;
        *sp++ = ret_val;
        break;
    case OP_call_constructor:
        call_argc = get_u16(pc);
        pc += 2;
        call_argv = sp - call_argc;
        sf->cur_pc = pc;
        ret_val = JS_CallConstructorInternal(ctx, call_argv[-2],
                                             call_argv[-1],
                                             call_argc, call_argv, 0);
        if (unlikely(JS_IsException(ret_val)))
            goto exception;
        for(i = -2; i < call_argc; i++)
            JS_FreeValue(ctx, call_argv[i]);
        sp -= call_argc + 2;
        *sp++ = ret_val;
        break;
    case OP_call_method:
    case OP_tail_call_method:
        call_argc = get_u16(pc);
        pc += 2;
        call_argv = sp - call_argc;
        sf->cur_pc = pc;
        ret_val = JS_CallInternal(ctx, call_argv[-1], call_argv[-2],
                                  JS_UNDEFINED, call_argc, call_argv, 0);
        if (unlikely(JS_IsException(ret_val)))
            goto exception;
        if (op == OP_tail_call_method)
            goto done;
        for(i = -2; i < call_argc; i++)
            JS_FreeValue(ctx, call_argv[i]);
        sp -= call_argc + 2;
        *sp++ = ret_val;
        break;
    case OP_apply:
        {
            int magic = get_u16(pc);
            pc += 2;
            sf->cur_pc = pc;
            ret_val = js_function_apply(ctx, sp[-3], 2, (JSValueConst *)&sp[-2], magic);
            if (unlikely(JS_IsException(ret_val)))
                goto exception;
            JS_FreeValue(ctx, sp[-3]);
            JS_FreeValue(ctx, sp[-2]);
            JS_FreeValue(ctx, sp[-1]);
            sp -= 3;
            *sp++ = ret_val;
        }
        break;
    case OP_eval:
        {
            call_argc = get_u16(pc);
            scope_idx = get_u16(pc + 2) + ARG_SCOPE_END;
            pc += 4;
            call_argv = sp - call_argc;
            sf->cur_pc = pc;
            if (js_same_value(ctx, call_argv[-1], ctx->eval_obj)) {
                if (call_argc >= 1)
                    obj = call_argv[0];
                else
                    obj = JS_UNDEFINED;
                ret_val = JS_EvalObject(ctx, JS_UNDEFINED, obj,
                                        JS_EVAL_TYPE_DIRECT, scope_idx);
            } else {
                ret_val = JS_CallInternal(ctx, call_argv[-1], JS_UNDEFINED,
                                          JS_UNDEFINED, call_argc, call_argv, 0);
            }
            if (unlikely(JS_IsException(ret_val)))
                goto exception;
            for(i = -1; i < call_argc; i++)
                JS_FreeValue(ctx, call_argv[i]);
            sp -= call_argc + 1;
            *sp++ = ret_val;
        }
        break;
    case OP_apply_eval:
        {
            scope_idx = get_u16(pc) + ARG_SCOPE_END;
            pc += 2;
            sf->cur_pc = pc;
            tab = build_arg_list(ctx, &len, sp[-1]);
            if (!tab)
                goto exception;
            if (js_same_value(ctx, sp[-2], ctx->eval_obj)) {
                if (len >= 1)
                    obj = tab[0];
                else
                    obj = JS_UNDEFINED;
                ret_val = JS_EvalObject(ctx, JS_UNDEFINED, obj,
                                        JS_EVAL_TYPE_DIRECT, scope_idx);
            } else {
                ret_val = JS_Call(ctx, sp[-2], JS_UNDEFINED, len,
                                  (JSValueConst *)tab);
            }
            free_arg_list(ctx, tab, len);
            if (unlikely(JS_IsException(ret_val)))
                goto exception;
            JS_FreeValue(ctx, sp[-2]);
            JS_FreeValue(ctx, sp[-1]);
            sp -= 2;
            *sp++ = ret_val;
        }
        break;
    case OP_import:
        {
            sf->cur_pc = pc;
            ret_val = js_dynamic_import(ctx, sp[-2], sp[-1]);
            if (JS_IsException(ret_val))
                goto exception;
            JS_FreeValue(ctx, sp[-2]);
            JS_FreeValue(ctx, sp[-1]);
            sp--;
            sp[-1] = ret_val;
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
    return 0; /* 返回0表示函数结束，需要外部处理 */

exception:
    cs->exception = 1;
    return -1;
}

/* ========== do_object：对象操作 ========== */
int do_object(CallState *cs, int op)
{
    JSContext *ctx = cs->ctx;
    JSStackFrame *sf = cs->sf;
    JSFunctionBytecode *b = cs->b;
    const uint8_t *pc = cs->pc;
    JSValue *sp = cs->sp;
    JSValue ret_val;
    JSAtom atom;
    int ret, op_flags, class_flags;
    JSObject *p;
    JSValue val, obj, prop, getter, setter, value;
    uint32_t tag;

    switch (op) {
    case OP_get_field:
    case OP_get_field2:
    case OP_get_length:
        {
            int keep = (op == OP_get_field2);
            int is_length = (op == OP_get_length);
            if (is_length) {
                atom = JS_ATOM_length;
            } else {
                atom = get_u32(pc);
                pc += 4;
            }
            obj = sp[-1];
            if (likely(JS_VALUE_GET_TAG(obj) == JS_TAG_OBJECT)) {
                p = JS_VALUE_GET_OBJ(obj);
                for(;;) {
                    JSProperty *pr;
                    JSShapeProperty *prs = find_own_property(&pr, p, atom);
                    if (prs) {
                        if (unlikely(prs->flags & JS_PROP_TMASK))
                            goto field_slow_path;
                        val = JS_DupValue(ctx, pr->u.value);
                        break;
                    }
                    if (unlikely(p->is_exotic)) {
                        obj = JS_MKPTR(JS_TAG_OBJECT, p);
                        goto field_slow_path;
                    }
                    p = p->shape->proto;
                    if (!p) {
                        val = JS_UNDEFINED;
                        break;
                    }
                }
            } else {
            field_slow_path:
                sf->cur_pc = pc;
                val = JS_GetPropertyInternal(ctx, obj, atom, sp[-1], 0);
                if (unlikely(JS_IsException(val)))
                    goto exception;
            }
            if (keep) {
                *sp++ = val;
            } else {
                JS_FreeValue(ctx, sp[-1]);
                sp[-1] = val;
            }
        }
        break;
    case OP_put_field:
        {
            atom = get_u32(pc);
            pc += 4;
            obj = sp[-2];
            if (likely(JS_VALUE_GET_TAG(obj) == JS_TAG_OBJECT)) {
                p = JS_VALUE_GET_OBJ(obj);
                JSProperty *pr;
                JSShapeProperty *prs = find_own_property(&pr, p, atom);
                if (!prs)
                    goto put_field_slow_path;
                if (likely((prs->flags & (JS_PROP_TMASK | JS_PROP_WRITABLE |
                                          JS_PROP_LENGTH)) == JS_PROP_WRITABLE)) {
                    set_value(ctx, &pr->u.value, sp[-1]);
                } else {
                    goto put_field_slow_path;
                }
                JS_FreeValue(ctx, obj);
                sp -= 2;
            } else {
            put_field_slow_path:
                sf->cur_pc = pc;
                ret = JS_SetPropertyInternal(ctx, obj, atom, sp[-1], obj,
                                             JS_PROP_THROW_STRICT);
                JS_FreeValue(ctx, obj);
                sp -= 2;
                if (unlikely(ret < 0))
                    goto exception;
            }
        }
        break;
    case OP_private_symbol:
        {
            atom = get_u32(pc);
            pc += 4;
            val = JS_NewSymbolFromAtom(ctx, atom, JS_ATOM_TYPE_PRIVATE);
            if (JS_IsException(val))
                goto exception;
            *sp++ = val;
        }
        break;
    case OP_get_private_field:
        {
            val = JS_GetPrivateField(ctx, sp[-2], sp[-1]);
            JS_FreeValue(ctx, sp[-1]);
            JS_FreeValue(ctx, sp[-2]);
            sp[-2] = val;
            sp--;
            if (unlikely(JS_IsException(val)))
                goto exception;
        }
        break;
    case OP_put_private_field:
        {
            ret = JS_SetPrivateField(ctx, sp[-3], sp[-1], sp[-2]);
            JS_FreeValue(ctx, sp[-3]);
            JS_FreeValue(ctx, sp[-1]);
            sp -= 3;
            if (unlikely(ret < 0))
                goto exception;
        }
        break;
    case OP_define_private_field:
        {
            ret = JS_DefinePrivateField(ctx, sp[-3], sp[-2], sp[-1]);
            JS_FreeValue(ctx, sp[-2]);
            sp -= 2;
            if (unlikely(ret < 0))
                goto exception;
        }
        break;
    case OP_define_field:
        {
            atom = get_u32(pc);
            pc += 4;
            ret = JS_DefinePropertyValue(ctx, sp[-2], atom, sp[-1],
                                         JS_PROP_C_W_E | JS_PROP_THROW);
            sp--;
            if (unlikely(ret < 0))
                goto exception;
        }
        break;
    case OP_set_name:
        {
            atom = get_u32(pc);
            pc += 4;
            ret = JS_DefineObjectName(ctx, sp[-1], atom, JS_PROP_CONFIGURABLE);
            if (unlikely(ret < 0))
                goto exception;
        }
        break;
    case OP_set_name_computed:
        {
            ret = JS_DefineObjectNameComputed(ctx, sp[-1], sp[-2], JS_PROP_CONFIGURABLE);
            if (unlikely(ret < 0))
                goto exception;
        }
        break;
    case OP_set_proto:
        {
            sf->cur_pc = pc;
            prop = sp[-1];
            if (JS_IsObject(prop) || JS_IsNull(prop)) {
                if (JS_SetPrototypeInternal(ctx, sp[-2], prop, TRUE) < 0)
                    goto exception;
            }
            JS_FreeValue(ctx, prop);
            sp--;
        }
        break;
    case OP_set_home_object:
        js_method_set_home_object(ctx, sp[-1], sp[-2]);
        break;
    case OP_define_method:
    case OP_define_method_computed:
        {
            int is_computed = (op == OP_define_method_computed);
            if (is_computed) {
                atom = JS_ValueToAtom(ctx, sp[-2]);
                if (unlikely(atom == JS_ATOM_NULL))
                    goto exception;
                op = OP_define_method; // 防止后续用错
            } else {
                atom = get_u32(pc);
                pc += 4;
            }
            op_flags = *pc++;
            obj = sp[-2 - is_computed];
            int flags = JS_PROP_HAS_CONFIGURABLE | JS_PROP_CONFIGURABLE |
                JS_PROP_HAS_ENUMERABLE | JS_PROP_THROW;
            if (op_flags & OP_DEFINE_METHOD_ENUMERABLE)
                flags |= JS_PROP_ENUMERABLE;
            op_flags &= 3;
            value = JS_UNDEFINED;
            getter = JS_UNDEFINED;
            setter = JS_UNDEFINED;
            if (op_flags == OP_DEFINE_METHOD_METHOD) {
                value = sp[-1];
                flags |= JS_PROP_HAS_VALUE | JS_PROP_HAS_WRITABLE | JS_PROP_WRITABLE;
            } else if (op_flags == OP_DEFINE_METHOD_GETTER) {
                getter = sp[-1];
                flags |= JS_PROP_HAS_GET;
            } else {
                setter = sp[-1];
                flags |= JS_PROP_HAS_SET;
            }
            ret = js_method_set_properties(ctx, sp[-1], atom, flags, obj);
            if (ret >= 0) {
                ret = JS_DefineProperty(ctx, obj, atom, value,
                                        getter, setter, flags);
            }
            JS_FreeValue(ctx, sp[-1]);
            if (is_computed) {
                JS_FreeAtom(ctx, atom);
                JS_FreeValue(ctx, sp[-2]);
            }
            sp -= 1 + is_computed;
            if (unlikely(ret < 0))
                goto exception;
        }
        break;
    case OP_define_class:
    case OP_define_class_computed:
        {
            atom = get_u32(pc);
            class_flags = pc[4];
            pc += 5;
            if (js_op_define_class(ctx, sp, atom, class_flags,
                                   cs->var_refs, sf,
                                   (op == OP_define_class_computed)) < 0)
                goto exception;
        }
        break;
    case OP_get_super:
        {
            sf->cur_pc = pc;
            ret_val = JS_GetPrototype(ctx, sp[-1]);
            if (JS_IsException(ret_val))
                goto exception;
            JS_FreeValue(ctx, sp[-1]);
            sp[-1] = ret_val;
        }
        break;
    case OP_get_super_value:
        {
            sf->cur_pc = pc;
            atom = JS_ValueToAtom(ctx, sp[-1]);
            if (unlikely(atom == JS_ATOM_NULL))
                goto exception;
            val = JS_GetPropertyInternal(ctx, sp[-2], atom, sp[-3], FALSE);
            JS_FreeAtom(ctx, atom);
            if (unlikely(JS_IsException(val)))
                goto exception;
            JS_FreeValue(ctx, sp[-1]);
            JS_FreeValue(ctx, sp[-2]);
            JS_FreeValue(ctx, sp[-3]);
            sp[-3] = val;
            sp -= 2;
        }
        break;
    case OP_put_super_value:
        {
            sf->cur_pc = pc;
            if (JS_VALUE_GET_TAG(sp[-3]) != JS_TAG_OBJECT) {
                JS_ThrowTypeErrorNotAnObject(ctx);
                goto exception;
            }
            atom = JS_ValueToAtom(ctx, sp[-2]);
            if (unlikely(atom == JS_ATOM_NULL))
                goto exception;
            ret = JS_SetPropertyInternal(ctx, sp[-3], atom, sp[-1], sp[-4],
                                         JS_PROP_THROW_STRICT);
            JS_FreeAtom(ctx, atom);
            JS_FreeValue(ctx, sp[-4]);
            JS_FreeValue(ctx, sp[-3]);
            JS_FreeValue(ctx, sp[-2]);
            sp -= 4;
            if (ret < 0)
                goto exception;
        }
        break;
    case OP_get_ref_value:
        {
            sf->cur_pc = pc;
            atom = JS_ValueToAtom(ctx, sp[-1]);
            if (atom == JS_ATOM_NULL)
                goto exception;
            if (unlikely(JS_IsUndefined(sp[-2]))) {
                JS_ThrowReferenceErrorNotDefined(ctx, atom);
                JS_FreeAtom(ctx, atom);
                goto exception;
            }
            ret = JS_HasProperty(ctx, sp[-2], atom);
            if (unlikely(ret <= 0)) {
                if (ret < 0) {
                    JS_FreeAtom(ctx, atom);
                    goto exception;
                }
                if (is_strict_mode(ctx)) {
                    JS_ThrowReferenceErrorNotDefined(ctx, atom);
                    JS_FreeAtom(ctx, atom);
                    goto exception;
                }
                val = JS_UNDEFINED;
            } else {
                val = JS_GetProperty(ctx, sp[-2], atom);
            }
            JS_FreeAtom(ctx, atom);
            if (unlikely(JS_IsException(val)))
                goto exception;
            sp[0] = val;
            sp++;
        }
        break;
    case OP_put_ref_value:
        {
            sf->cur_pc = pc;
            atom = JS_ValueToAtom(ctx, sp[-2]);
            if (unlikely(atom == JS_ATOM_NULL))
                goto exception;
            if (unlikely(JS_IsUndefined(sp[-3]))) {
                if (is_strict_mode(ctx)) {
                    JS_ThrowReferenceErrorNotDefined(ctx, atom);
                    JS_FreeAtom(ctx, atom);
                    goto exception;
                } else {
                    sp[-3] = JS_DupValue(ctx, ctx->global_obj);
                }
            }
            ret = JS_HasProperty(ctx, sp[-3], atom);
            if (unlikely(ret <= 0)) {
                if (unlikely(ret < 0)) {
                    JS_FreeAtom(ctx, atom);
                    goto exception;
                }
                if (is_strict_mode(ctx)) {
                    JS_ThrowReferenceErrorNotDefined(ctx, atom);
                    JS_FreeAtom(ctx, atom);
                    goto exception;
                }
            }
            ret = JS_SetPropertyInternal(ctx, sp[-3], atom, sp[-1], sp[-3],
                                         JS_PROP_THROW_STRICT);
            JS_FreeAtom(ctx, atom);
            JS_FreeValue(ctx, sp[-2]);
            JS_FreeValue(ctx, sp[-3]);
            sp -= 3;
            if (unlikely(ret < 0))
                goto exception;
        }
        break;
    case OP_check_ctor_return:
        if (!JS_IsObject(sp[-1])) {
            if (!JS_IsUndefined(sp[-1])) {
                JS_ThrowTypeError(cs->ctx, "derived class constructor must return an object or undefined");
                goto exception;
            }
            sp[0] = JS_TRUE;
        } else {
            sp[0] = JS_FALSE;
        }
        sp++;
        break;
    case OP_check_ctor:
        if (JS_IsUndefined(cs->new_target)) {
            JS_ThrowTypeError(ctx, "class constructors must be invoked with 'new'");
            goto exception;
        }
        break;
    case OP_init_ctor:
        {
            JSValue super;
            sf->cur_pc = pc;
            if (JS_IsUndefined(cs->new_target))
                goto non_ctor_call;
            super = JS_GetPrototype(ctx, cs->sf->cur_func);
            if (JS_IsException(super))
                goto exception;
            ret_val = JS_CallConstructor2(ctx, super, cs->new_target,
                                          cs->argc, (JSValueConst *)cs->argv);
            JS_FreeValue(ctx, super);
            if (JS_IsException(ret_val))
                goto exception;
            *sp++ = ret_val;
        }
        break;
    case OP_check_brand:
        {
            ret = JS_CheckBrand(ctx, sp[-2], sp[-1]);
            if (ret < 0)
                goto exception;
            if (!ret) {
                JS_ThrowTypeError(ctx, "invalid brand on object");
                goto exception;
            }
        }
        break;
    case OP_add_brand:
        if (JS_AddBrand(ctx, sp[-2], sp[-1]) < 0)
            goto exception;
        JS_FreeValue(ctx, sp[-2]);
        JS_FreeValue(ctx, sp[-1]);
        sp -= 2;
        break;
    default:
        abort();
    }

    cs->pc = pc;
    cs->sp = sp;
    return 0;

non_ctor_call:
    JS_ThrowTypeError(ctx, "class constructors must be invoked with 'new'");
    goto exception;

exception:
    cs->exception = 1;
    return -1;
}
