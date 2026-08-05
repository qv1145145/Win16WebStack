#include "js_internal.h"



__exception int js_parse_statement_or_decl(JSParseState *s,
                                                  int decl_mask)
{
    JSContext *ctx = s->ctx;
    JSAtom label_name;
    int tok;

    /* specific label handling */
    /* XXX: support multiple labels on loop statements */
    label_name = JS_ATOM_NULL;
    if (is_label(s)) {
        BlockEnv *be;

        label_name = JS_DupAtom(ctx, s->token.u.ident.atom);

        for (be = s->cur_func->top_break; be; be = be->prev) {
            if (be->label_name == label_name) {
                js_parse_error(s, "duplicate label name");
                goto fail;
            }
        }

        if (next_token(s))
            goto fail;
        if (js_parse_expect(s, ':'))
            goto fail;
        if (s->token.val != TOK_FOR
        &&  s->token.val != TOK_DO
        &&  s->token.val != TOK_WHILE) {
            /* labelled regular statement */
            int label_break, mask;
            BlockEnv break_entry;

            label_break = new_label(s);
            push_break_entry(s->cur_func, &break_entry,
                             label_name, label_break, -1, 0);
            break_entry.is_regular_stmt = TRUE;
            if (!(s->cur_func->js_mode & JS_MODE_STRICT) &&
                (decl_mask & DECL_MASK_FUNC_WITH_LABEL)) {
                mask = DECL_MASK_FUNC | DECL_MASK_FUNC_WITH_LABEL;
            } else {
                mask = 0;
            }
            if (js_parse_statement_or_decl(s, mask))
                goto fail;
            emit_label(s, label_break);
            pop_break_entry(s->cur_func);
            goto done;
        }
    }

    switch(tok = s->token.val) {
    case '{':
        if (js_parse_block(s))
            goto fail;
        break;
    case TOK_RETURN:
        {
            const uint8_t *op_token_ptr;
            if (s->cur_func->is_eval) {
                js_parse_error(s, "return not in a function");
                goto fail;
            }
            if (s->cur_func->func_type == JS_PARSE_FUNC_CLASS_STATIC_INIT) {
                js_parse_error(s, "return in a initializer block");
                goto fail;
            }
            op_token_ptr = s->token.ptr;
            if (next_token(s))
                goto fail;
            if (s->token.val != ';' && s->token.val != '}' && !s->got_lf) {
                if (js_parse_expr(s))
                    goto fail;
                emit_source_pos(s, op_token_ptr);
                emit_return(s, TRUE);
            } else {
                emit_source_pos(s, op_token_ptr);
                emit_return(s, FALSE);
            }
            if (js_parse_expect_semi(s))
                goto fail;
        }
        break;
    case TOK_THROW:
        {
            const uint8_t *op_token_ptr;
            op_token_ptr = s->token.ptr;
            if (next_token(s))
                goto fail;
            if (s->got_lf) {
                js_parse_error(s, "line terminator not allowed after throw");
                goto fail;
            }
            if (js_parse_expr(s))
                goto fail;
            emit_source_pos(s, op_token_ptr);
            emit_op(s, OP_throw);
            if (js_parse_expect_semi(s))
                goto fail;
        }
        break;
    case TOK_LET:
    case TOK_CONST:
    haslet:
        if (!(decl_mask & DECL_MASK_OTHER)) {
            js_parse_error(s, "lexical declarations can't appear in single-statement context");
            goto fail;
        }
        /* fall thru */
    case TOK_VAR:
        if (next_token(s))
            goto fail;
        if (js_parse_var(s, TRUE, tok, FALSE))
            goto fail;
        if (js_parse_expect_semi(s))
            goto fail;
        break;
    case TOK_IF:
        {
            int label1, label2, mask;
            if (next_token(s))
                goto fail;
            /* create a new scope for `let f;if(1) function f(){}` */
            push_scope(s);
            set_eval_ret_undefined(s);
            if (js_parse_expr_paren(s))
                goto fail;
            label1 = emit_goto(s, OP_if_false, -1);
            if (s->cur_func->js_mode & JS_MODE_STRICT)
                mask = 0;
            else
                mask = DECL_MASK_FUNC; /* Annex B.3.4 */

            if (js_parse_statement_or_decl(s, mask))
                goto fail;

            if (s->token.val == TOK_ELSE) {
                label2 = emit_goto(s, OP_goto, -1);
                if (next_token(s))
                    goto fail;

                emit_label(s, label1);
                if (js_parse_statement_or_decl(s, mask))
                    goto fail;

                label1 = label2;
            }
            emit_label(s, label1);
            pop_scope(s);
        }
        break;
    case TOK_WHILE:
        {
            int label_cont, label_break;
            BlockEnv break_entry;

            label_cont = new_label(s);
            label_break = new_label(s);

            push_break_entry(s->cur_func, &break_entry,
                             label_name, label_break, label_cont, 0);

            if (next_token(s))
                goto fail;

            set_eval_ret_undefined(s);

            emit_label(s, label_cont);
            if (js_parse_expr_paren(s))
                goto fail;
            emit_goto(s, OP_if_false, label_break);

            if (js_parse_statement(s))
                goto fail;
            emit_goto(s, OP_goto, label_cont);

            emit_label(s, label_break);

            pop_break_entry(s->cur_func);
        }
        break;
    case TOK_DO:
        {
            int label_cont, label_break, label1;
            BlockEnv break_entry;

            label_cont = new_label(s);
            label_break = new_label(s);
            label1 = new_label(s);

            push_break_entry(s->cur_func, &break_entry,
                             label_name, label_break, label_cont, 0);

            if (next_token(s))
                goto fail;

            emit_label(s, label1);

            set_eval_ret_undefined(s);

            if (js_parse_statement(s))
                goto fail;

            emit_label(s, label_cont);
            if (js_parse_expect(s, TOK_WHILE))
                goto fail;
            if (js_parse_expr_paren(s))
                goto fail;
            /* Insert semicolon if missing */
            if (s->token.val == ';') {
                if (next_token(s))
                    goto fail;
            }
            emit_goto(s, OP_if_true, label1);

            emit_label(s, label_break);

            pop_break_entry(s->cur_func);
        }
        break;
    case TOK_FOR:
        {
            int label_cont, label_break, label_body, label_test;
            int pos_cont, pos_body, block_scope_level;
            BlockEnv break_entry;
            int tok, bits;
            BOOL is_async;

            if (next_token(s))
                goto fail;

            set_eval_ret_undefined(s);
            bits = 0;
            is_async = FALSE;
            if (s->token.val == '(') {
                js_parse_skip_parens_token(s, &bits, FALSE);
            } else if (s->token.val == TOK_AWAIT) {
                if (!(s->cur_func->func_kind & JS_FUNC_ASYNC)) {
                    js_parse_error(s, "for await is only valid in asynchronous functions");
                    goto fail;
                }
                is_async = TRUE;
                if (next_token(s))
                    goto fail;
                s->cur_func->has_await = TRUE;
            }
            if (js_parse_expect(s, '('))
                goto fail;

            if (!(bits & SKIP_HAS_SEMI)) {
                /* parse for/in or for/of */
                if (js_parse_for_in_of(s, label_name, is_async))
                    goto fail;
                break;
            }
            block_scope_level = s->cur_func->scope_level;

            /* create scope for the lexical variables declared in the initial,
               test and increment expressions */
            push_scope(s);
            /* initial expression */
            tok = s->token.val;
            if (tok != ';') {
                switch (is_let(s, DECL_MASK_OTHER)) {
                case TRUE:
                    tok = TOK_LET;
                    break;
                case FALSE:
                    break;
                default:
                    goto fail;
                }
                if (tok == TOK_VAR || tok == TOK_LET || tok == TOK_CONST) {
                    if (next_token(s))
                        goto fail;
                    if (js_parse_var(s, FALSE, tok, FALSE))
                        goto fail;
                } else {
                    if (js_parse_expr2(s, FALSE))
                        goto fail;
                    emit_op(s, OP_drop);
                }

                /* close the closures before the first iteration */
                close_scopes(s, s->cur_func->scope_level, block_scope_level);
            }
            if (js_parse_expect(s, ';'))
                goto fail;

            label_test = new_label(s);
            label_cont = new_label(s);
            label_body = new_label(s);
            label_break = new_label(s);

            push_break_entry(s->cur_func, &break_entry,
                             label_name, label_break, label_cont, 0);

            /* test expression */
            if (s->token.val == ';') {
                /* no test expression */
                label_test = label_body;
            } else {
                emit_label(s, label_test);
                if (js_parse_expr(s))
                    goto fail;
                emit_goto(s, OP_if_false, label_break);
            }
            if (js_parse_expect(s, ';'))
                goto fail;

            if (s->token.val == ')') {
                /* no end expression */
                break_entry.label_cont = label_cont = label_test;
                pos_cont = 0; /* avoid warning */
            } else {
                /* skip the end expression */
                emit_goto(s, OP_goto, label_body);

                pos_cont = s->cur_func->byte_code.size;
                emit_label(s, label_cont);
                if (js_parse_expr(s))
                    goto fail;
                emit_op(s, OP_drop);
                if (label_test != label_body)
                    emit_goto(s, OP_goto, label_test);
            }
            if (js_parse_expect(s, ')'))
                goto fail;

            pos_body = s->cur_func->byte_code.size;
            emit_label(s, label_body);
            if (js_parse_statement(s))
                goto fail;

            /* close the closures before the next iteration */
            /* XXX: check continue case */
            close_scopes(s, s->cur_func->scope_level, block_scope_level);

            if (OPTIMIZE && label_test != label_body && label_cont != label_test) {
                /* move the increment code here */
                DynBuf *bc = &s->cur_func->byte_code;
                int chunk_size = pos_body - pos_cont;
                int offset = bc->size - pos_cont;
                int i;
                if (dbuf_claim(bc, chunk_size))
                    goto fail;
                dbuf_put(bc, bc->buf + pos_cont, chunk_size);
                memset(bc->buf + pos_cont, OP_nop, chunk_size);
                /* increment part ends with a goto */
                s->cur_func->last_opcode_pos = bc->size - 5;
                /* relocate labels */
                for (i = label_cont; i < s->cur_func->label_count; i++) {
                    LabelSlot *ls = &s->cur_func->label_slots[i];
                    if (ls->pos >= pos_cont && ls->pos < pos_body)
                        ls->pos += offset;
                }
            } else {
                emit_goto(s, OP_goto, label_cont);
            }

            emit_label(s, label_break);

            pop_break_entry(s->cur_func);
            pop_scope(s);
        }
        break;
    case TOK_BREAK:
    case TOK_CONTINUE:
        {
            int is_cont = s->token.val - TOK_BREAK;
            int label;

            if (next_token(s))
                goto fail;
            if (!s->got_lf && s->token.val == TOK_IDENT && !s->token.u.ident.is_reserved)
                label = s->token.u.ident.atom;
            else
                label = JS_ATOM_NULL;
            if (emit_break(s, label, is_cont))
                goto fail;
            if (label != JS_ATOM_NULL) {
                if (next_token(s))
                    goto fail;
            }
            if (js_parse_expect_semi(s))
                goto fail;
        }
        break;
    case TOK_SWITCH:
        {
            int label_case, label_break, label1;
            int default_label_pos;
            BlockEnv break_entry;

            if (next_token(s))
                goto fail;

            set_eval_ret_undefined(s);
            if (js_parse_expr_paren(s))
                goto fail;

            push_scope(s);
            label_break = new_label(s);
            push_break_entry(s->cur_func, &break_entry,
                             label_name, label_break, -1, 1);

            if (js_parse_expect(s, '{'))
                goto fail;

            default_label_pos = -1;
            label_case = -1;
            while (s->token.val != '}') {
                if (s->token.val == TOK_CASE) {
                    label1 = -1;
                    if (label_case >= 0) {
                        /* skip the case if needed */
                        label1 = emit_goto(s, OP_goto, -1);
                    }
                    emit_label(s, label_case);
                    label_case = -1;
                    for (;;) {
                        /* parse a sequence of case clauses */
                        if (next_token(s))
                            goto fail;
                        emit_op(s, OP_dup);
                        if (js_parse_expr(s))
                            goto fail;
                        if (js_parse_expect(s, ':'))
                            goto fail;
                        emit_op(s, OP_strict_eq);
                        if (s->token.val == TOK_CASE) {
                            label1 = emit_goto(s, OP_if_true, label1);
                        } else {
                            label_case = emit_goto(s, OP_if_false, -1);
                            emit_label(s, label1);
                            break;
                        }
                    }
                } else if (s->token.val == TOK_DEFAULT) {
                    if (next_token(s))
                        goto fail;
                    if (js_parse_expect(s, ':'))
                        goto fail;
                    if (default_label_pos >= 0) {
                        js_parse_error(s, "duplicate default");
                        goto fail;
                    }
                    if (label_case < 0) {
                        /* falling thru direct from switch expression */
                        label_case = emit_goto(s, OP_goto, -1);
                    }
                    /* Emit a dummy label opcode. Label will be patched after
                       the end of the switch body. Do not use emit_label(s, 0)
                       because it would clobber label 0 address, preventing
                       proper optimizer operation.
                     */
                    emit_op(s, OP_label);
                    emit_u32(s, 0);
                    default_label_pos = s->cur_func->byte_code.size - 4;
                } else {
                    if (label_case < 0) {
                        /* falling thru direct from switch expression */
                        js_parse_error(s, "invalid switch statement");
                        goto fail;
                    }
                    if (js_parse_statement_or_decl(s, DECL_MASK_ALL))
                        goto fail;
                }
            }
            if (js_parse_expect(s, '}'))
                goto fail;
            if (default_label_pos >= 0) {
                /* Ugly patch for the `default` label, shameful and risky */
                put_u32(s->cur_func->byte_code.buf + default_label_pos,
                        label_case);
                s->cur_func->label_slots[label_case].pos = default_label_pos + 4;
            } else {
                emit_label(s, label_case);
            }
            emit_label(s, label_break);
            emit_op(s, OP_drop); /* drop the switch expression */

            pop_break_entry(s->cur_func);
            pop_scope(s);
        }
        break;
    case TOK_TRY:
        {
            int label_catch, label_catch2, label_finally, label_end;
            JSAtom name;
            BlockEnv block_env;

            set_eval_ret_undefined(s);
            if (next_token(s))
                goto fail;
            label_catch = new_label(s);
            label_catch2 = new_label(s);
            label_finally = new_label(s);
            label_end = new_label(s);

            emit_goto(s, OP_catch, label_catch);

            push_break_entry(s->cur_func, &block_env,
                             JS_ATOM_NULL, -1, -1, 1);
            block_env.label_finally = label_finally;

            if (js_parse_block(s))
                goto fail;

            pop_break_entry(s->cur_func);

            if (js_is_live_code(s)) {
                /* drop the catch offset */
                emit_op(s, OP_drop);
                /* must push dummy value to keep same stack size */
                emit_op(s, OP_undefined);
                emit_goto(s, OP_gosub, label_finally);
                emit_op(s, OP_drop);

                emit_goto(s, OP_goto, label_end);
            }

            if (s->token.val == TOK_CATCH) {
                if (next_token(s))
                    goto fail;

                push_scope(s);  /* catch variable */
                emit_label(s, label_catch);

                if (s->token.val == '{') {
                    /* support optional-catch-binding feature */
                    emit_op(s, OP_drop);    /* pop the exception object */
                } else {
                    if (js_parse_expect(s, '('))
                        goto fail;
                    if (!(s->token.val == TOK_IDENT && !s->token.u.ident.is_reserved)) {
                        if (s->token.val == '[' || s->token.val == '{') {
                            /* XXX: TOK_LET is not completely correct */
                            if (js_parse_destructuring_element(s, TOK_LET, 0, TRUE, -1, TRUE, FALSE) < 0)
                                goto fail;
                        } else {
                            js_parse_error(s, "identifier expected");
                            goto fail;
                        }
                    } else {
                        name = JS_DupAtom(ctx, s->token.u.ident.atom);
                        if (next_token(s)
                        ||  js_define_var(s, name, TOK_CATCH) < 0) {
                            JS_FreeAtom(ctx, name);
                            goto fail;
                        }
                        /* store the exception value in the catch variable */
                        emit_op(s, OP_scope_put_var);
                        emit_u32(s, name);
                        emit_u16(s, s->cur_func->scope_level);
                    }
                    if (js_parse_expect(s, ')'))
                        goto fail;
                }
                /* XXX: should keep the address to nop it out if there is no finally block */
                emit_goto(s, OP_catch, label_catch2);

                push_scope(s);  /* catch block */
                push_break_entry(s->cur_func, &block_env, JS_ATOM_NULL,
                                 -1, -1, 1);
                block_env.label_finally = label_finally;

                if (js_parse_block(s))
                    goto fail;

                pop_break_entry(s->cur_func);
                pop_scope(s);  /* catch block */
                pop_scope(s);  /* catch variable */

                if (js_is_live_code(s)) {
                    /* drop the catch2 offset */
                    emit_op(s, OP_drop);
                    /* XXX: should keep the address to nop it out if there is no finally block */
                    /* must push dummy value to keep same stack size */
                    emit_op(s, OP_undefined);
                    emit_goto(s, OP_gosub, label_finally);
                    emit_op(s, OP_drop);
                    emit_goto(s, OP_goto, label_end);
                }
                /* catch exceptions thrown in the catch block to execute the
                 * finally clause and rethrow the exception */
                emit_label(s, label_catch2);
                /* catch value is at TOS, no need to push undefined */
                emit_goto(s, OP_gosub, label_finally);
                emit_op(s, OP_throw);

            } else if (s->token.val == TOK_FINALLY) {
                /* finally without catch : execute the finally clause
                 * and rethrow the exception */
                emit_label(s, label_catch);
                /* catch value is at TOS, no need to push undefined */
                emit_goto(s, OP_gosub, label_finally);
                emit_op(s, OP_throw);
            } else {
                js_parse_error(s, "expecting catch or finally");
                goto fail;
            }
            emit_label(s, label_finally);
            if (s->token.val == TOK_FINALLY) {
                int saved_eval_ret_idx = 0; /* avoid warning */

                if (next_token(s))
                    goto fail;
                /* on the stack: ret_value gosub_ret_value */
                push_break_entry(s->cur_func, &block_env, JS_ATOM_NULL,
                                 -1, -1, 2);

                if (s->cur_func->eval_ret_idx >= 0) {
                    /* 'finally' updates eval_ret only if not a normal
                       termination */
                    saved_eval_ret_idx =
                        add_var(s->ctx, s->cur_func, JS_ATOM__ret_);
                    if (saved_eval_ret_idx < 0)
                        goto fail;
                    emit_op(s, OP_get_loc);
                    emit_u16(s, s->cur_func->eval_ret_idx);
                    emit_op(s, OP_put_loc);
                    emit_u16(s, saved_eval_ret_idx);
                    set_eval_ret_undefined(s);
                }

                if (js_parse_block(s))
                    goto fail;

                if (s->cur_func->eval_ret_idx >= 0) {
                    emit_op(s, OP_get_loc);
                    emit_u16(s, saved_eval_ret_idx);
                    emit_op(s, OP_put_loc);
                    emit_u16(s, s->cur_func->eval_ret_idx);
                }
                pop_break_entry(s->cur_func);
            }
            emit_op(s, OP_ret);
            emit_label(s, label_end);
        }
        break;
    case ';':
        /* empty statement */
        if (next_token(s))
            goto fail;
        break;
    case TOK_WITH:
        if (s->cur_func->js_mode & JS_MODE_STRICT) {
            js_parse_error(s, "invalid keyword: with");
            goto fail;
        } else {
            int with_idx;

            if (next_token(s))
                goto fail;

            if (js_parse_expr_paren(s))
                goto fail;

            push_scope(s);
            with_idx = define_var(s, s->cur_func, JS_ATOM__with_,
                                  JS_VAR_DEF_WITH);
            if (with_idx < 0)
                goto fail;
            emit_op(s, OP_to_object);
            emit_op(s, OP_put_loc);
            emit_u16(s, with_idx);

            set_eval_ret_undefined(s);
            if (js_parse_statement(s))
                goto fail;

            /* Popping scope drops lexical context for the with object variable */
            pop_scope(s);
        }
        break;
    case TOK_FUNCTION:
        /* ES6 Annex B.3.2 and B.3.3 semantics */
        if (!(decl_mask & DECL_MASK_FUNC))
            goto func_decl_error;
        if (!(decl_mask & DECL_MASK_OTHER) && peek_token(s, FALSE) == '*')
            goto func_decl_error;
        goto parse_func_var;
    case TOK_IDENT:
        if (s->token.u.ident.is_reserved) {
            js_parse_error_reserved_identifier(s);
            goto fail;
        }
        /* Determine if `let` introduces a Declaration or an ExpressionStatement */
        switch (is_let(s, decl_mask)) {
        case TRUE:
            tok = TOK_LET;
            goto haslet;
        case FALSE:
            break;
        default:
            goto fail;
        }
        if (token_is_pseudo_keyword(s, JS_ATOM_async) &&
            peek_token(s, TRUE) == TOK_FUNCTION) {
            if (!(decl_mask & DECL_MASK_OTHER)) {
            func_decl_error:
                js_parse_error(s, "function declarations can't appear in single-statement context");
                goto fail;
            }
        parse_func_var:
            if (js_parse_function_decl(s, JS_PARSE_FUNC_VAR,
                                       JS_FUNC_NORMAL, JS_ATOM_NULL,
                                       s->token.ptr))
                goto fail;
            break;
        }
        goto hasexpr;

    case TOK_CLASS:
        if (!(decl_mask & DECL_MASK_OTHER)) {
            js_parse_error(s, "class declarations can't appear in single-statement context");
            goto fail;
        }
        if (js_parse_class(s, FALSE, JS_PARSE_EXPORT_NONE))
            return -1;
        break;

    case TOK_DEBUGGER:
        /* currently no debugger, so just skip the keyword */
        if (next_token(s))
            goto fail;
        if (js_parse_expect_semi(s))
            goto fail;
        break;

    case TOK_ENUM:
    case TOK_EXPORT:
    case TOK_EXTENDS:
        js_unsupported_keyword(s, s->token.u.ident.atom);
        goto fail;

    default:
    hasexpr:
        emit_source_pos(s, s->token.ptr);
        if (js_parse_expr(s))
            goto fail;
        if (s->cur_func->eval_ret_idx >= 0) {
            /* store the expression value so that it can be returned
               by eval() */
            emit_op(s, OP_put_loc);
            emit_u16(s, s->cur_func->eval_ret_idx);
        } else {
            emit_op(s, OP_drop); /* drop the result */
        }
        if (js_parse_expect_semi(s))
            goto fail;
        break;
    }
done:
    JS_FreeAtom(ctx, label_name);
    return 0;
fail:
    JS_FreeAtom(ctx, label_name);
    return -1;
}

/* return NULL in case of exception (e.g. module could not be loaded) */
JSModuleDef *js_host_resolve_imported_module(JSContext *ctx,
                                                    const char *base_cname,
                                                    const char *cname1,
                                                    JSValueConst attributes)
{
    JSRuntime *rt = ctx->rt;
    JSModuleDef *m;
    char *cname;
    JSAtom module_name;

    if (!rt->module_normalize_func) {
        cname = js_default_module_normalize_name(ctx, base_cname, cname1);
    } else {
        cname = rt->module_normalize_func(ctx, base_cname, cname1,
                                          rt->module_loader_opaque);
    }
    if (!cname)
        return NULL;

    module_name = JS_NewAtom(ctx, cname);
    if (module_name == JS_ATOM_NULL) {
        js_free(ctx, cname);
        return NULL;
    }

    /* first look at the loaded modules */
    m = js_find_loaded_module(ctx, module_name);
    if (m) {
        js_free(ctx, cname);
        JS_FreeAtom(ctx, module_name);
        return m;
    }

    JS_FreeAtom(ctx, module_name);

    /* load the module */
    if (!rt->u.module_loader_func) {
        /* XXX: use a syntax error ? */
        JS_ThrowReferenceError(ctx, "could not load module '%s'",
                               cname);
        js_free(ctx, cname);
        return NULL;
    }
    if (rt->module_loader_has_attr) {
        m = rt->u.module_loader_func2(ctx, cname, rt->module_loader_opaque, attributes);
    } else {
        m = rt->u.module_loader_func(ctx, cname, rt->module_loader_opaque);
    }
    js_free(ctx, cname);
    return m;
}

JSModuleDef *js_host_resolve_imported_module_atom(JSContext *ctx,
                                                         JSAtom base_module_name,
                                                         JSAtom module_name1,
                                                         JSValueConst attributes)
{
    const char *base_cname, *cname;
    JSModuleDef *m;

    base_cname = JS_AtomToCString(ctx, base_module_name);
    if (!base_cname)
        return NULL;
    cname = JS_AtomToCString(ctx, module_name1);
    if (!cname) {
        JS_FreeCString(ctx, base_cname);
        return NULL;
    }
    m = js_host_resolve_imported_module(ctx, base_cname, cname, attributes);
    JS_FreeCString(ctx, base_cname);
    JS_FreeCString(ctx, cname);
    return m;
}

int find_resolve_entry(JSResolveState *s,
                              JSModuleDef *m, JSAtom name)
{
    int i;
    for(i = 0; i < s->count; i++) {
        JSResolveEntry *re = &s->array[i];
        if (re->module == m && re->name == name)
            return i;
    }
    return -1;
}

int add_resolve_entry(JSContext *ctx, JSResolveState *s,
                             JSModuleDef *m, JSAtom name)
{
    JSResolveEntry *re;

    if (js_resize_array(ctx, (void **)&s->array,
                        sizeof(JSResolveEntry),
                        &s->size, s->count + 1))
        return -1;
    re = &s->array[s->count++];
    re->module = m;
    re->name = JS_DupAtom(ctx, name);
    return 0;
}

JSResolveResultEnum js_resolve_export1(JSContext *ctx,
                                              JSModuleDef **pmodule,
                                              JSExportEntry **pme,
                                              JSModuleDef *m,
                                              JSAtom export_name,
                                              JSResolveState *s)
{
    JSExportEntry *me;

    *pmodule = NULL;
    *pme = NULL;
    if (find_resolve_entry(s, m, export_name) >= 0)
        return JS_RESOLVE_RES_CIRCULAR;
    if (add_resolve_entry(ctx, s, m, export_name) < 0)
        return JS_RESOLVE_RES_EXCEPTION;
    me = find_export_entry(ctx, m, export_name);
    if (me) {
        if (me->export_type == JS_EXPORT_TYPE_LOCAL) {
            /* local export */
            *pmodule = m;
            *pme = me;
            return JS_RESOLVE_RES_FOUND;
        } else {
            /* indirect export */
            JSModuleDef *m1;
            m1 = m->req_module_entries[me->u.req_module_idx].module;
            if (me->local_name == JS_ATOM__star_) {
                /* export ns from */
                *pmodule = m;
                *pme = me;
                return JS_RESOLVE_RES_FOUND;
            } else {
                return js_resolve_export1(ctx, pmodule, pme, m1,
                                          me->local_name, s);
            }
        }
    } else {
        if (export_name != JS_ATOM_default) {
            /* not found in direct or indirect exports: try star exports */
            int i;

            for(i = 0; i < m->star_export_entries_count; i++) {
                JSStarExportEntry *se = &m->star_export_entries[i];
                JSModuleDef *m1, *res_m;
                JSExportEntry *res_me;
                JSResolveResultEnum ret;

                m1 = m->req_module_entries[se->req_module_idx].module;
                ret = js_resolve_export1(ctx, &res_m, &res_me, m1,
                                         export_name, s);
                if (ret == JS_RESOLVE_RES_AMBIGUOUS ||
                    ret == JS_RESOLVE_RES_EXCEPTION) {
                    return ret;
                } else if (ret == JS_RESOLVE_RES_FOUND) {
                    if (*pme != NULL) {
                        if (*pmodule != res_m ||
                            res_me->local_name != (*pme)->local_name) {
                            *pmodule = NULL;
                            *pme = NULL;
                            return JS_RESOLVE_RES_AMBIGUOUS;
                        }
                    } else {
                        *pmodule = res_m;
                        *pme = res_me;
                    }
                }
            }
            if (*pme != NULL)
                return JS_RESOLVE_RES_FOUND;
        }
        return JS_RESOLVE_RES_NOT_FOUND;
    }
}

/* If the return value is JS_RESOLVE_RES_FOUND, return the module
  (*pmodule) and the corresponding local export entry
  (*pme). Otherwise return (NULL, NULL) */
JSResolveResultEnum js_resolve_export(JSContext *ctx,
                                             JSModuleDef **pmodule,
                                             JSExportEntry **pme,
                                             JSModuleDef *m,
                                             JSAtom export_name)
{
    JSResolveState ss, *s = &ss;
    int i;
    JSResolveResultEnum ret;

    s->array = NULL;
    s->size = 0;
    s->count = 0;

    ret = js_resolve_export1(ctx, pmodule, pme, m, export_name, s);

    for(i = 0; i < s->count; i++)
        JS_FreeAtom(ctx, s->array[i].name);
    js_free(ctx, s->array);

    return ret;
}

void js_resolve_export_throw_error(JSContext *ctx,
                                          JSResolveResultEnum res,
                                          JSModuleDef *m, JSAtom export_name)
{
    char buf1[ATOM_GET_STR_BUF_SIZE];
    char buf2[ATOM_GET_STR_BUF_SIZE];
    switch(res) {
    case JS_RESOLVE_RES_EXCEPTION:
        break;
    default:
    case JS_RESOLVE_RES_NOT_FOUND:
        JS_ThrowSyntaxError(ctx, "Could not find export '%s' in module '%s'",
                            JS_AtomGetStr(ctx, buf1, sizeof(buf1), export_name),
                            JS_AtomGetStr(ctx, buf2, sizeof(buf2), m->module_name));
        break;
    case JS_RESOLVE_RES_CIRCULAR:
        JS_ThrowSyntaxError(ctx, "circular reference when looking for export '%s' in module '%s'",
                            JS_AtomGetStr(ctx, buf1, sizeof(buf1), export_name),
                            JS_AtomGetStr(ctx, buf2, sizeof(buf2), m->module_name));
        break;
    case JS_RESOLVE_RES_AMBIGUOUS:
        JS_ThrowSyntaxError(ctx, "export '%s' in module '%s' is ambiguous",
                            JS_AtomGetStr(ctx, buf1, sizeof(buf1), export_name),
                            JS_AtomGetStr(ctx, buf2, sizeof(buf2), m->module_name));
        break;
    }
}


int find_exported_name(GetExportNamesState *s, JSAtom name)
{
    int i;
    for(i = 0; i < s->exported_names_count; i++) {
        if (s->exported_names[i].export_name == name)
            return i;
    }
    return -1;
}

__exception int get_exported_names(JSContext *ctx,
                                          GetExportNamesState *s,
                                          JSModuleDef *m, BOOL from_star)
{
    ExportedNameEntry *en;
    int i, j;

    /* check circular reference */
    for(i = 0; i < s->modules_count; i++) {
        if (s->modules[i] == m)
            return 0;
    }
    if (js_resize_array(ctx, (void **)&s->modules, sizeof(s->modules[0]),
                        &s->modules_size, s->modules_count + 1))
        return -1;
    s->modules[s->modules_count++] = m;

    for(i = 0; i < m->export_entries_count; i++) {
        JSExportEntry *me = &m->export_entries[i];
        if (from_star && me->export_name == JS_ATOM_default)
            continue;
        j = find_exported_name(s, me->export_name);
        if (j < 0) {
            if (js_resize_array(ctx, (void **)&s->exported_names, sizeof(s->exported_names[0]),
                                &s->exported_names_size,
                                s->exported_names_count + 1))
                return -1;
            en = &s->exported_names[s->exported_names_count++];
            en->export_name = me->export_name;
            /* avoid a second lookup for simple module exports */
            if (from_star || me->export_type != JS_EXPORT_TYPE_LOCAL)
                en->u.me = NULL;
            else
                en->u.me = me;
        } else {
            en = &s->exported_names[j];
            en->u.me = NULL;
        }
    }
    for(i = 0; i < m->star_export_entries_count; i++) {
        JSStarExportEntry *se = &m->star_export_entries[i];
        JSModuleDef *m1;
        m1 = m->req_module_entries[se->req_module_idx].module;
        if (get_exported_names(ctx, s, m1, TRUE))
            return -1;
    }
    return 0;
}

/* Unfortunately, the spec gives a different behavior from GetOwnProperty ! */
int js_module_ns_has(JSContext *ctx, JSValueConst obj, JSAtom atom)
{
    return (find_own_property1(JS_VALUE_GET_OBJ(obj), atom) != NULL);
}

int exported_names_cmp(const void *p1, const void *p2, void *opaque)
{
    JSContext *ctx = opaque;
    const ExportedNameEntry *me1 = p1;
    const ExportedNameEntry *me2 = p2;
    JSValue str1, str2;
    int ret;

    /* XXX: should avoid allocation memory in atom comparison */
    str1 = JS_AtomToString(ctx, me1->export_name);
    str2 = JS_AtomToString(ctx, me2->export_name);
    if (JS_IsException(str1) || JS_IsException(str2)) {
        /* XXX: raise an error ? */
        ret = 0;
    } else {
        ret = js_string_compare(ctx, JS_VALUE_GET_STRING(str1),
                                JS_VALUE_GET_STRING(str2));
    }
    JS_FreeValue(ctx, str1);
    JS_FreeValue(ctx, str2);
    return ret;
}

JSValue js_module_ns_autoinit(JSContext *ctx, JSObject *p, JSAtom atom,
                                     void *opaque)
{
    JSModuleDef *m = opaque;
    JSResolveResultEnum res;
    JSExportEntry *res_me;
    JSModuleDef *res_m;
    JSVarRef *var_ref;

    res = js_resolve_export(ctx, &res_m, &res_me, m, atom);
    if (res != JS_RESOLVE_RES_FOUND) {
        /* fail safe: normally no error should happen here except for memory */
        js_resolve_export_throw_error(ctx, res, m, atom);
        return JS_EXCEPTION;
    }
    if (res_me->local_name == JS_ATOM__star_) {
        return JS_GetModuleNamespace(ctx, res_m->req_module_entries[res_me->u.req_module_idx].module);
    } else {
        if (res_me->u.local.var_ref) {
            var_ref = res_me->u.local.var_ref;
        } else {
            JSObject *p1 = JS_VALUE_GET_OBJ(res_m->func_obj);
            var_ref = p1->u.func.var_refs[res_me->u.local.var_idx];
        }
        /* WARNING: a varref is returned as a string ! */
        return JS_MKPTR(JS_TAG_STRING, var_ref);
    }
}

JSValue js_build_module_ns(JSContext *ctx, JSModuleDef *m)
{
    JSValue obj;
    JSObject *p;
    GetExportNamesState s_s, *s = &s_s;
    int i, ret;
    JSProperty *pr;

    obj = JS_NewObjectClass(ctx, JS_CLASS_MODULE_NS);
    if (JS_IsException(obj))
        return obj;
    p = JS_VALUE_GET_OBJ(obj);

    memset(s, 0, sizeof(*s));
    ret = get_exported_names(ctx, s, m, FALSE);
    js_free(ctx, s->modules);
    if (ret)
        goto fail;

    /* Resolve the exported names. The ambiguous exports are removed */
    for(i = 0; i < s->exported_names_count; i++) {
        ExportedNameEntry *en = &s->exported_names[i];
        JSResolveResultEnum res;
        JSExportEntry *res_me;
        JSModuleDef *res_m;

        if (en->u.me) {
            res_me = en->u.me; /* fast case: no resolution needed */
            res_m = m;
            res = JS_RESOLVE_RES_FOUND;
        } else {
            res = js_resolve_export(ctx, &res_m, &res_me, m,
                                    en->export_name);
        }
        if (res != JS_RESOLVE_RES_FOUND) {
            if (res != JS_RESOLVE_RES_AMBIGUOUS) {
                js_resolve_export_throw_error(ctx, res, m, en->export_name);
                goto fail;
            }
            en->export_type = EXPORTED_NAME_AMBIGUOUS;
        } else {
            if (res_me->local_name == JS_ATOM__star_) {
                en->export_type = EXPORTED_NAME_DELAYED;
            } else {
                if (res_me->u.local.var_ref) {
                    en->u.var_ref = res_me->u.local.var_ref;
                } else {
                    JSObject *p1 = JS_VALUE_GET_OBJ(res_m->func_obj);
                    en->u.var_ref = p1->u.func.var_refs[res_me->u.local.var_idx];
                }
                if (en->u.var_ref == NULL)
                    en->export_type = EXPORTED_NAME_DELAYED;
                else
                    en->export_type = EXPORTED_NAME_NORMAL;
            }
        }
    }

    /* sort the exported names */
    rqsort(s->exported_names, s->exported_names_count,
           sizeof(s->exported_names[0]), exported_names_cmp, ctx);

    for(i = 0; i < s->exported_names_count; i++) {
        ExportedNameEntry *en = &s->exported_names[i];
        switch(en->export_type) {
        case EXPORTED_NAME_NORMAL:
            {
                JSVarRef *var_ref = en->u.var_ref;
                pr = add_property(ctx, p, en->export_name,
                                  JS_PROP_ENUMERABLE | JS_PROP_WRITABLE |
                                  JS_PROP_VARREF);
                if (!pr)
                    goto fail;
                js_rc(var_ref)->ref_count++;
                pr->u.var_ref = var_ref;
            }
            break;
        case EXPORTED_NAME_DELAYED:
            /* the exported namespace or reference may depend on
               circular references, so we resolve it lazily */
            if (JS_DefineAutoInitProperty(ctx, obj,
                                          en->export_name,
                                          JS_AUTOINIT_ID_MODULE_NS,
                                          m, JS_PROP_ENUMERABLE | JS_PROP_WRITABLE) < 0)
                goto fail;
            break;
        default:
            break;
        }
    }

    js_free(ctx, s->exported_names);

    JS_DefinePropertyValue(ctx, obj, JS_ATOM_Symbol_toStringTag,
                           JS_AtomToString(ctx, JS_ATOM_Module),
                           0);

    p->extensible = FALSE;
    return obj;
 fail:
    js_free(ctx, s->exported_names);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

/* Load all the required modules for module 'm' */
int js_resolve_module(JSContext *ctx, JSModuleDef *m)
{
    int i;
    JSModuleDef *m1;

    if (m->resolved)
        return 0;
#ifdef DUMP_MODULE_RESOLVE
    {
        char buf1[ATOM_GET_STR_BUF_SIZE];
        printf("resolving module '%s':\n", JS_AtomGetStr(ctx, buf1, sizeof(buf1), m->module_name));
    }
#endif
    m->resolved = TRUE;
    /* resolve each requested module */
    for(i = 0; i < m->req_module_entries_count; i++) {
        JSReqModuleEntry *rme = &m->req_module_entries[i];
        m1 = js_host_resolve_imported_module_atom(ctx, m->module_name,
                                                  rme->module_name,
                                                  rme->attributes);
        if (!m1)
            return -1;
        rme->module = m1;
        /* already done in js_host_resolve_imported_module() except if
           the module was loaded with JS_EvalBinary() */
        if (js_resolve_module(ctx, m1) < 0)
            return -1;
    }
    return 0;
}

/* Create the <eval> function associated with the module */
int js_create_module_bytecode_function(JSContext *ctx, JSModuleDef *m)
{
    JSFunctionBytecode *b;
    JSValue func_obj, bfunc;

    bfunc = m->func_obj;
    func_obj = JS_NewObjectProtoClass(ctx, ctx->function_proto,
                                      JS_CLASS_BYTECODE_FUNCTION);

    if (JS_IsException(func_obj))
        return -1;
    m->func_obj = func_obj;
    b = JS_VALUE_GET_PTR(bfunc);
    func_obj = js_closure2(ctx, func_obj, b, NULL, NULL, TRUE, m);
    if (JS_IsException(func_obj)) {
        m->func_obj = JS_UNDEFINED; /* XXX: keep it ? */
        JS_FreeValue(ctx, func_obj);
        return -1;
    }
    return 0;
}

/* must be done before js_link_module() because of cyclic references */
int js_create_module_function(JSContext *ctx, JSModuleDef *m)
{
    BOOL is_c_module;
    int i;
    JSVarRef *var_ref;

    if (m->func_created)
        return 0;

    is_c_module = (m->init_func != NULL);

    if (is_c_module) {
        /* initialize the exported variables */
        for(i = 0; i < m->export_entries_count; i++) {
            JSExportEntry *me = &m->export_entries[i];
            if (me->export_type == JS_EXPORT_TYPE_LOCAL) {
                var_ref = js_create_var_ref(ctx, FALSE);
                if (!var_ref)
                    return -1;
                me->u.local.var_ref = var_ref;
            }
        }
    } else {
        if (js_create_module_bytecode_function(ctx, m))
            return -1;
    }
    m->func_created = TRUE;

    /* do it on the dependencies */

    for(i = 0; i < m->req_module_entries_count; i++) {
        JSReqModuleEntry *rme = &m->req_module_entries[i];
        if (js_create_module_function(ctx, rme->module) < 0)
            return -1;
    }

    return 0;
}


/* Prepare a module to be executed by resolving all the imported
   variables. */
int js_inner_module_linking(JSContext *ctx, JSModuleDef *m,
                                   JSModuleDef **pstack_top, int index)
{
    int i;
    JSImportEntry *mi;
    JSModuleDef *m1;
    JSVarRef **var_refs, *var_ref;
    JSObject *p;
    BOOL is_c_module;
    JSValue ret_val;

    if (js_check_stack_overflow(ctx->rt, 0)) {
        JS_ThrowStackOverflow(ctx);
        return -1;
    }

#ifdef DUMP_MODULE_RESOLVE
    {
        char buf1[ATOM_GET_STR_BUF_SIZE];
        printf("js_inner_module_linking '%s':\n", JS_AtomGetStr(ctx, buf1, sizeof(buf1), m->module_name));
    }
#endif

    if (m->status == JS_MODULE_STATUS_LINKING ||
        m->status == JS_MODULE_STATUS_LINKED ||
        m->status == JS_MODULE_STATUS_EVALUATING_ASYNC ||
        m->status == JS_MODULE_STATUS_EVALUATED)
        return index;

    assert(m->status == JS_MODULE_STATUS_UNLINKED);
    m->status = JS_MODULE_STATUS_LINKING;
    m->dfs_index = index;
    m->dfs_ancestor_index = index;
    index++;
    /* push 'm' on stack */
    m->stack_prev = *pstack_top;
    *pstack_top = m;

    for(i = 0; i < m->req_module_entries_count; i++) {
        JSReqModuleEntry *rme = &m->req_module_entries[i];
        m1 = rme->module;
        index = js_inner_module_linking(ctx, m1, pstack_top, index);
        if (index < 0)
            goto fail;
        assert(m1->status == JS_MODULE_STATUS_LINKING ||
               m1->status == JS_MODULE_STATUS_LINKED ||
               m1->status == JS_MODULE_STATUS_EVALUATING_ASYNC ||
               m1->status == JS_MODULE_STATUS_EVALUATED);
        if (m1->status == JS_MODULE_STATUS_LINKING) {
            m->dfs_ancestor_index = min_int(m->dfs_ancestor_index,
                                            m1->dfs_ancestor_index);
        }
    }

#ifdef DUMP_MODULE_RESOLVE
    {
        char buf1[ATOM_GET_STR_BUF_SIZE];
        printf("instantiating module '%s':\n", JS_AtomGetStr(ctx, buf1, sizeof(buf1), m->module_name));
    }
#endif
    /* check the indirect exports */
    for(i = 0; i < m->export_entries_count; i++) {
        JSExportEntry *me = &m->export_entries[i];
        if (me->export_type == JS_EXPORT_TYPE_INDIRECT &&
            me->local_name != JS_ATOM__star_) {
            JSResolveResultEnum ret;
            JSExportEntry *res_me;
            JSModuleDef *res_m, *m1;
            m1 = m->req_module_entries[me->u.req_module_idx].module;
            ret = js_resolve_export(ctx, &res_m, &res_me, m1, me->local_name);
            if (ret != JS_RESOLVE_RES_FOUND) {
                js_resolve_export_throw_error(ctx, ret, m, me->export_name);
                goto fail;
            }
        }
    }

#ifdef DUMP_MODULE_RESOLVE
    {
        printf("exported bindings:\n");
        for(i = 0; i < m->export_entries_count; i++) {
            JSExportEntry *me = &m->export_entries[i];
            printf(" name="); print_atom(ctx, me->export_name);
            printf(" local="); print_atom(ctx, me->local_name);
            printf(" type=%d idx=%d\n", me->export_type, me->u.local.var_idx);
        }
    }
#endif

    is_c_module = (m->init_func != NULL);

    if (!is_c_module) {
        p = JS_VALUE_GET_OBJ(m->func_obj);
        var_refs = p->u.func.var_refs;

        for(i = 0; i < m->import_entries_count; i++) {
            mi = &m->import_entries[i];
#ifdef DUMP_MODULE_RESOLVE
            printf("import var_idx=%d name=", mi->var_idx);
            print_atom(ctx, mi->import_name);
            printf(": ");
#endif
            m1 = m->req_module_entries[mi->req_module_idx].module;
            if (mi->is_star) {
                JSValue val;
                /* name space import */
                val = JS_GetModuleNamespace(ctx, m1);
                if (JS_IsException(val))
                    goto fail;
                set_value(ctx, &var_refs[mi->var_idx]->value, val);
#ifdef DUMP_MODULE_RESOLVE
                printf("namespace\n");
#endif
            } else {
                JSResolveResultEnum ret;
                JSExportEntry *res_me;
                JSModuleDef *res_m;
                JSObject *p1;

                ret = js_resolve_export(ctx, &res_m,
                                        &res_me, m1, mi->import_name);
                if (ret != JS_RESOLVE_RES_FOUND) {
                    js_resolve_export_throw_error(ctx, ret, m1, mi->import_name);
                    goto fail;
                }
                if (res_me->local_name == JS_ATOM__star_) {
                    JSValue val;
                    JSModuleDef *m2;
                    /* name space import from */
                    m2 = res_m->req_module_entries[res_me->u.req_module_idx].module;
                    val = JS_GetModuleNamespace(ctx, m2);
                    if (JS_IsException(val))
                        goto fail;
                    var_ref = js_create_var_ref(ctx, TRUE);
                    if (!var_ref) {
                        JS_FreeValue(ctx, val);
                        goto fail;
                    }
                    set_value(ctx, &var_ref->value, val);
                    var_refs[mi->var_idx] = var_ref;
#ifdef DUMP_MODULE_RESOLVE
                    printf("namespace from\n");
#endif
                } else {
                    var_ref = res_me->u.local.var_ref;
                    if (!var_ref) {
                        p1 = JS_VALUE_GET_OBJ(res_m->func_obj);
                        var_ref = p1->u.func.var_refs[res_me->u.local.var_idx];
                    }
                    js_rc(var_ref)->ref_count++;
                    var_refs[mi->var_idx] = var_ref;
#ifdef DUMP_MODULE_RESOLVE
                    printf("local export (var_ref=%p)\n", var_ref);
#endif
                }
            }
        }

        /* keep the exported variables in the module export entries (they
           are used when the eval function is deleted and cannot be
           initialized before in case imports are exported) */
        for(i = 0; i < m->export_entries_count; i++) {
            JSExportEntry *me = &m->export_entries[i];
            if (me->export_type == JS_EXPORT_TYPE_LOCAL) {
                var_ref = var_refs[me->u.local.var_idx];
                js_rc(var_ref)->ref_count++;
                me->u.local.var_ref = var_ref;
            }
        }

        /* initialize the global variables */
        ret_val = JS_Call(ctx, m->func_obj, JS_TRUE, 0, NULL);
        if (JS_IsException(ret_val))
            goto fail;
        JS_FreeValue(ctx, ret_val);
    }

    assert(m->dfs_ancestor_index <= m->dfs_index);
    if (m->dfs_index == m->dfs_ancestor_index) {
        for(;;) {
            /* pop m1 from stack */
            m1 = *pstack_top;
            *pstack_top = m1->stack_prev;
            m1->status = JS_MODULE_STATUS_LINKED;
            if (m1 == m)
                break;
        }
    }

#ifdef DUMP_MODULE_RESOLVE
    printf("js_inner_module_linking done\n");
#endif
    return index;
 fail:
    return -1;
}

/* Prepare a module to be executed by resolving all the imported
   variables. */
int js_link_module(JSContext *ctx, JSModuleDef *m)
{
    JSModuleDef *stack_top, *m1;

#ifdef DUMP_MODULE_RESOLVE
    {
        char buf1[ATOM_GET_STR_BUF_SIZE];
        printf("js_link_module '%s':\n", JS_AtomGetStr(ctx, buf1, sizeof(buf1), m->module_name));
    }
#endif
    assert(m->status == JS_MODULE_STATUS_UNLINKED ||
           m->status == JS_MODULE_STATUS_LINKED ||
           m->status == JS_MODULE_STATUS_EVALUATING_ASYNC ||
           m->status == JS_MODULE_STATUS_EVALUATED);
    stack_top = NULL;
    if (js_inner_module_linking(ctx, m, &stack_top, 0) < 0) {
        while (stack_top != NULL) {
            m1 = stack_top;
            assert(m1->status == JS_MODULE_STATUS_LINKING);
            m1->status = JS_MODULE_STATUS_UNLINKED;
            stack_top = m1->stack_prev;
        }
        return -1;
    }
    assert(stack_top == NULL);
    assert(m->status == JS_MODULE_STATUS_LINKED ||
           m->status == JS_MODULE_STATUS_EVALUATING_ASYNC ||
           m->status == JS_MODULE_STATUS_EVALUATED);
    return 0;
}

JSValue js_import_meta(JSContext *ctx)
{
    JSAtom filename;
    JSModuleDef *m;

    filename = JS_GetScriptOrModuleName(ctx, 0);
    if (filename == JS_ATOM_NULL)
        goto fail;

    /* XXX: inefficient, need to add a module or script pointer in
       JSFunctionBytecode */
    m = js_find_loaded_module(ctx, filename);
    JS_FreeAtom(ctx, filename);
    if (!m) {
    fail:
        JS_ThrowTypeError(ctx, "import.meta not supported in this context");
        return JS_EXCEPTION;
    }
    return JS_GetImportMeta(ctx, m);
}

JSValue JS_NewModuleValue(JSContext *ctx, JSModuleDef *m)
{
    return JS_DupValue(ctx, JS_MKPTR(JS_TAG_MODULE, m));
}

JSValue js_load_module_rejected(JSContext *ctx, JSValueConst this_val,
                                       int argc, JSValueConst *argv, int magic, JSValue *func_data)
{
    JSValueConst *resolving_funcs = (JSValueConst *)func_data;
    JSValueConst error;
    JSValue ret;

    /* XXX: check if the test is necessary */
    if (argc >= 1)
        error = argv[0];
    else
        error = JS_UNDEFINED;
    ret = JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED,
                  1, &error);
    JS_FreeValue(ctx, ret);
    return JS_UNDEFINED;
}

JSValue js_load_module_fulfilled(JSContext *ctx, JSValueConst this_val,
                                        int argc, JSValueConst *argv, int magic, JSValue *func_data)
{
    JSValueConst *resolving_funcs = (JSValueConst *)func_data;
    JSModuleDef *m = JS_VALUE_GET_PTR(func_data[2]);
    JSValue ret, ns;

    /* return the module namespace */
    ns = JS_GetModuleNamespace(ctx, m);
    if (JS_IsException(ns)) {
        JSValue err = JS_GetException(ctx);
        js_load_module_rejected(ctx, JS_UNDEFINED, 1, (JSValueConst *)&err, 0, func_data);
        return JS_UNDEFINED;
    }
    ret = JS_Call(ctx, resolving_funcs[0], JS_UNDEFINED,
                   1, (JSValueConst *)&ns);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, ns);
    return JS_UNDEFINED;
}

void JS_LoadModuleInternal(JSContext *ctx, const char *basename,
                                  const char *filename,
                                  JSValueConst *resolving_funcs,
                                  JSValueConst attributes)
{
    JSValue evaluate_promise;
    JSModuleDef *m;
    JSValue ret, err, func_obj, evaluate_resolving_funcs[2];
    JSValueConst func_data[3];

    m = js_host_resolve_imported_module(ctx, basename, filename, attributes);
    if (!m)
        goto fail;

    if (js_resolve_module(ctx, m) < 0) {
        js_free_modules(ctx, JS_FREE_MODULE_NOT_RESOLVED);
        goto fail;
    }

    /* Evaluate the module code */
    func_obj = JS_NewModuleValue(ctx, m);
    evaluate_promise = JS_EvalFunction(ctx, func_obj);
    if (JS_IsException(evaluate_promise)) {
    fail:
        err = JS_GetException(ctx);
        ret = JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED,
                      1, (JSValueConst *)&err);
        JS_FreeValue(ctx, ret); /* XXX: what to do if exception ? */
        JS_FreeValue(ctx, err);
        return;
    }

    func_obj = JS_NewModuleValue(ctx, m);
    func_data[0] = resolving_funcs[0];
    func_data[1] = resolving_funcs[1];
    func_data[2] = func_obj;
    evaluate_resolving_funcs[0] = JS_NewCFunctionData(ctx, js_load_module_fulfilled, 0, 0, 3, func_data);
    evaluate_resolving_funcs[1] = JS_NewCFunctionData(ctx, js_load_module_rejected, 0, 0, 3, func_data);
    JS_FreeValue(ctx, func_obj);
    ret = js_promise_then(ctx, evaluate_promise, 2, (JSValueConst *)evaluate_resolving_funcs);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, evaluate_resolving_funcs[0]);
    JS_FreeValue(ctx, evaluate_resolving_funcs[1]);
    JS_FreeValue(ctx, evaluate_promise);
}

JSValue js_dynamic_import_job(JSContext *ctx,
                                     int argc, JSValueConst *argv)
{
    JSValueConst *resolving_funcs = argv;
    JSValueConst basename_val = argv[2];
    JSValueConst specifier = argv[3];
    JSValueConst attributes = argv[4];
    const char *basename = NULL, *filename;
    JSValue ret, err;

    if (!JS_IsString(basename_val)) {
        JS_ThrowTypeError(ctx, "no function filename for import()");
        goto exception;
    }
    basename = JS_ToCString(ctx, basename_val);
    if (!basename)
        goto exception;

    filename = JS_ToCString(ctx, specifier);
    if (!filename)
        goto exception;

    JS_LoadModuleInternal(ctx, basename, filename,
                          resolving_funcs, attributes);
    JS_FreeCString(ctx, filename);
    JS_FreeCString(ctx, basename);
    return JS_UNDEFINED;
 exception:
    err = JS_GetException(ctx);
    ret = JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED,
                   1, (JSValueConst *)&err);
    JS_FreeValue(ctx, ret); /* XXX: what to do if exception ? */
    JS_FreeValue(ctx, err);
    JS_FreeCString(ctx, basename);
    return JS_UNDEFINED;
}

JSValue js_dynamic_import(JSContext *ctx, JSValueConst specifier, JSValueConst options)
{
    JSAtom basename;
    JSValue promise, resolving_funcs[2], basename_val, err, ret;
    JSValue specifier_str = JS_UNDEFINED, attributes = JS_UNDEFINED, attributes_obj = JS_UNDEFINED;
    JSValueConst args[5];

    basename = JS_GetScriptOrModuleName(ctx, 0);
    if (basename == JS_ATOM_NULL)
        basename_val = JS_NULL;
    else
        basename_val = JS_AtomToValue(ctx, basename);
    JS_FreeAtom(ctx, basename);
    if (JS_IsException(basename_val))
        return basename_val;

    promise = JS_NewPromiseCapability(ctx, resolving_funcs);
    if (JS_IsException(promise)) {
        JS_FreeValue(ctx, basename_val);
        return promise;
    }

    /* the string conversion must occur here */
    specifier_str = JS_ToString(ctx, specifier);
    if (JS_IsException(specifier_str))
        goto exception;

    if (!JS_IsUndefined(options)) {
        if (!JS_IsObject(options)) {
            JS_ThrowTypeError(ctx, "options must be an object");
            goto exception;
        }
        attributes_obj = JS_GetProperty(ctx, options, JS_ATOM_with);
        if (JS_IsException(attributes_obj))
            goto exception;
        if (!JS_IsUndefined(attributes_obj)) {
            JSPropertyEnum *atoms;
            uint32_t atoms_len, i;
            JSValue val;

            if (!JS_IsObject(attributes_obj)) {
                JS_ThrowTypeError(ctx, "options.with must be an object");
                goto exception;
            }
            attributes = JS_NewObjectProto(ctx, JS_NULL);
            if (JS_GetOwnPropertyNamesInternal(ctx, &atoms, &atoms_len, JS_VALUE_GET_OBJ(attributes_obj),
                                               JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY)) {
                goto exception;
            }
            for(i = 0; i < atoms_len; i++) {
                val = JS_GetProperty(ctx, attributes_obj, atoms[i].atom);
                if (JS_IsException(val))
                    goto exception1;
                if (!JS_IsString(val)) {
                    JS_FreeValue(ctx, val);
                    JS_ThrowTypeError(ctx, "module attribute values must be strings");
                    goto exception1;
                }
                if (JS_DefinePropertyValue(ctx, attributes,  atoms[i].atom, val,
                                           JS_PROP_C_W_E) < 0) {
                exception1:
                    JS_FreePropertyEnum(ctx, atoms, atoms_len);
                    goto exception;
                }
            }
            JS_FreePropertyEnum(ctx, atoms, atoms_len);
            if (ctx->rt->module_check_attrs &&
                ctx->rt->module_check_attrs(ctx, ctx->rt->module_loader_opaque, attributes) < 0) {
                goto exception;
            }
            JS_FreeValue(ctx, attributes_obj);
        }
    }

    args[0] = resolving_funcs[0];
    args[1] = resolving_funcs[1];
    args[2] = basename_val;
    args[3] = specifier_str;
    args[4] = attributes;

    /* cannot run JS_LoadModuleInternal synchronously because it would
       cause an unexpected recursion in js_evaluate_module() */
    JS_EnqueueJob(ctx, js_dynamic_import_job, 5, args);
 done:
    JS_FreeValue(ctx, basename_val);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, specifier_str);
    JS_FreeValue(ctx, attributes);
    return promise;
 exception:
    JS_FreeValue(ctx, attributes_obj);
    err = JS_GetException(ctx);
    ret = JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED,
                   1, (JSValueConst *)&err);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, err);
    goto done;
}

void js_set_module_evaluated(JSContext *ctx, JSModuleDef *m)
{
    m->status = JS_MODULE_STATUS_EVALUATED;
    if (!JS_IsUndefined(m->promise)) {
        JSValue value, ret_val;
        assert(m->cycle_root == m);
        value = JS_UNDEFINED;
        ret_val = JS_Call(ctx, m->resolving_funcs[0], JS_UNDEFINED,
                          1, (JSValueConst *)&value);
        JS_FreeValue(ctx, ret_val);
    }
}

/* XXX: slow. Could use a linked list instead of ExecModuleList */
BOOL find_in_exec_module_list(ExecModuleList *exec_list, JSModuleDef *m)
{
    int i;
    for(i = 0; i < exec_list->count; i++) {
        if (exec_list->tab[i] == m)
            return TRUE;
    }
    return FALSE;
}

int gather_available_ancestors(JSContext *ctx, JSModuleDef *module,
                                      ExecModuleList *exec_list)
{
    int i;

    if (js_check_stack_overflow(ctx->rt, 0)) {
        JS_ThrowStackOverflow(ctx);
        return -1;
    }
    for(i = 0; i < module->async_parent_modules_count; i++) {
        JSModuleDef *m = module->async_parent_modules[i];
        if (!find_in_exec_module_list(exec_list, m) &&
            !m->cycle_root->eval_has_exception) {
            assert(m->status == JS_MODULE_STATUS_EVALUATING_ASYNC);
            assert(!m->eval_has_exception);
            assert(m->async_evaluation);
            assert(m->pending_async_dependencies > 0);
            m->pending_async_dependencies--;
            if (m->pending_async_dependencies == 0) {
                if (js_resize_array(ctx, (void **)&exec_list->tab, sizeof(exec_list->tab[0]), &exec_list->size, exec_list->count + 1)) {
                    return -1;
                }
                exec_list->tab[exec_list->count++] = m;
                if (!m->has_tla) {
                    if (gather_available_ancestors(ctx, m, exec_list))
                        return -1;
                }
            }
        }
    }
    return 0;
}

int exec_module_list_cmp(const void *p1, const void *p2, void *opaque)
{
    JSModuleDef *m1 = *(JSModuleDef **)p1;
    JSModuleDef *m2 = *(JSModuleDef **)p2;
    return (m1->async_evaluation_timestamp > m2->async_evaluation_timestamp) -
        (m1->async_evaluation_timestamp < m2->async_evaluation_timestamp);
}

int js_execute_async_module(JSContext *ctx, JSModuleDef *m);
int js_execute_sync_module(JSContext *ctx, JSModuleDef *m,
                                  JSValue *pvalue);
#ifdef DUMP_MODULE_EXEC
void js_dump_module(JSContext *ctx, const char *str, JSModuleDef *m)
{
    char buf1[ATOM_GET_STR_BUF_SIZE];
    const char *module_status_str[] = { "unlinked", "linking", "linked", "evaluating", "evaluating_async", "evaluated" };
    printf("%s: %s status=%s\n", str, JS_AtomGetStr(ctx, buf1, sizeof(buf1), m->module_name), module_status_str[m->status]);
}
#endif

JSValue js_async_module_execution_rejected(JSContext *ctx, JSValueConst this_val,
                                                  int argc, JSValueConst *argv, int magic, JSValue *func_data)
{
    JSModuleDef *module = JS_VALUE_GET_PTR(func_data[0]);
    JSValueConst error = argv[0];
    int i;

#ifdef DUMP_MODULE_EXEC
    js_dump_module(ctx, __func__, module);
#endif
    if (js_check_stack_overflow(ctx->rt, 0))
        return JS_ThrowStackOverflow(ctx);

    if (module->status == JS_MODULE_STATUS_EVALUATED) {
        assert(module->eval_has_exception);
        return JS_UNDEFINED;
    }

    assert(module->status == JS_MODULE_STATUS_EVALUATING_ASYNC);
    assert(!module->eval_has_exception);
    assert(module->async_evaluation);

    module->eval_has_exception = TRUE;
    module->eval_exception = JS_DupValue(ctx, error);
    module->status = JS_MODULE_STATUS_EVALUATED;
    module->async_evaluation = FALSE;

    if (!JS_IsUndefined(module->promise)) {
        JSValue ret_val;
        assert(module->cycle_root == module);
        ret_val = JS_Call(ctx, module->resolving_funcs[1], JS_UNDEFINED,
                          1, &error);
        JS_FreeValue(ctx, ret_val);
    }

    for(i = 0; i < module->async_parent_modules_count; i++) {
        JSModuleDef *m = module->async_parent_modules[i];
        JSValue m_obj = JS_NewModuleValue(ctx, m);
        js_async_module_execution_rejected(ctx, JS_UNDEFINED, 1, &error, 0,
                                           &m_obj);
        JS_FreeValue(ctx, m_obj);
    }
    return JS_UNDEFINED;
}

JSValue js_async_module_execution_fulfilled(JSContext *ctx, JSValueConst this_val,
                                                   int argc, JSValueConst *argv, int magic, JSValue *func_data)
{
    JSModuleDef *module = JS_VALUE_GET_PTR(func_data[0]);
    ExecModuleList exec_list_s, *exec_list = &exec_list_s;
    int i;

#ifdef DUMP_MODULE_EXEC
    js_dump_module(ctx, __func__, module);
#endif
    if (module->status == JS_MODULE_STATUS_EVALUATED) {
        assert(module->eval_has_exception);
        return JS_UNDEFINED;
    }
    assert(module->status == JS_MODULE_STATUS_EVALUATING_ASYNC);
    assert(!module->eval_has_exception);
    assert(module->async_evaluation);
    module->async_evaluation = FALSE;
    js_set_module_evaluated(ctx, module);

    exec_list->tab = NULL;
    exec_list->count = 0;
    exec_list->size = 0;

    if (gather_available_ancestors(ctx, module, exec_list) < 0) {
        js_free(ctx, exec_list->tab);
        return JS_EXCEPTION;
    }

    /* sort by increasing async_evaluation timestamp */
    rqsort(exec_list->tab, exec_list->count, sizeof(exec_list->tab[0]),
           exec_module_list_cmp, NULL);

    for(i = 0; i < exec_list->count; i++) {
        JSModuleDef *m = exec_list->tab[i];
#ifdef DUMP_MODULE_EXEC
        printf("  %d/%d", i, exec_list->count); js_dump_module(ctx, "", m);
#endif
        if (m->status == JS_MODULE_STATUS_EVALUATED) {
            assert(m->eval_has_exception);
        } else if (m->has_tla) {
            js_execute_async_module(ctx, m);
        } else {
            JSValue error;
            if (js_execute_sync_module(ctx, m, &error) < 0) {
                JSValue m_obj = JS_NewModuleValue(ctx, m);
                js_async_module_execution_rejected(ctx, JS_UNDEFINED,
                                                   1, (JSValueConst *)&error, 0,
                                                   &m_obj);
                JS_FreeValue(ctx, m_obj);
                JS_FreeValue(ctx, error);
            } else {
                m->async_evaluation = FALSE;
                js_set_module_evaluated(ctx, m);
            }
        }
    }
    js_free(ctx, exec_list->tab);
    return JS_UNDEFINED;
}

int js_execute_async_module(JSContext *ctx, JSModuleDef *m)
{
    JSValue promise, m_obj;
    JSValue resolve_funcs[2], ret_val;
#ifdef DUMP_MODULE_EXEC
    js_dump_module(ctx, __func__, m);
#endif
    promise = js_async_function_call(ctx, m->func_obj, JS_UNDEFINED, 0, NULL, 0);
    if (JS_IsException(promise))
        return -1;
    m_obj = JS_NewModuleValue(ctx, m);
    resolve_funcs[0] = JS_NewCFunctionData(ctx, js_async_module_execution_fulfilled, 0, 0, 1, (JSValueConst *)&m_obj);
    resolve_funcs[1] = JS_NewCFunctionData(ctx, js_async_module_execution_rejected, 0, 0, 1, (JSValueConst *)&m_obj);
    ret_val = js_promise_then(ctx, promise, 2, (JSValueConst *)resolve_funcs);
    JS_FreeValue(ctx, ret_val);
    JS_FreeValue(ctx, m_obj);
    JS_FreeValue(ctx, resolve_funcs[0]);
    JS_FreeValue(ctx, resolve_funcs[1]);
    JS_FreeValue(ctx, promise);
    return 0;
}

/* return < 0 in case of exception. *pvalue contains the exception. */
int js_execute_sync_module(JSContext *ctx, JSModuleDef *m,
                                  JSValue *pvalue)
{
#ifdef DUMP_MODULE_EXEC
    js_dump_module(ctx, __func__, m);
#endif
    if (m->init_func) {
        /* C module init : no asynchronous execution */
        if (m->init_func(ctx, m) < 0)
            goto fail;
    } else {
        JSValue promise;
        JSPromiseStateEnum state;

        promise = js_async_function_call(ctx, m->func_obj, JS_UNDEFINED, 0, NULL, 0);
        if (JS_IsException(promise))
            goto fail;
        state = JS_PromiseState(ctx, promise);
        if (state == JS_PROMISE_FULFILLED) {
            JS_FreeValue(ctx, promise);
        } else if (state == JS_PROMISE_REJECTED) {
            *pvalue = JS_PromiseResult(ctx, promise);
            JS_FreeValue(ctx, promise);
            return -1;
        } else {
            JS_FreeValue(ctx, promise);
            JS_ThrowTypeError(ctx, "promise is pending");
        fail:
            *pvalue = JS_GetException(ctx);
            return -1;
        }
    }
    *pvalue = JS_UNDEFINED;
    return 0;
}

/* spec: InnerModuleEvaluation. Return (index, JS_UNDEFINED) or (-1,
   exception) */
int js_inner_module_evaluation(JSContext *ctx, JSModuleDef *m,
                                      int index, JSModuleDef **pstack_top,
                                      JSValue *pvalue)
{
    JSModuleDef *m1;
    int i;

#ifdef DUMP_MODULE_EXEC
    js_dump_module(ctx, __func__, m);
#endif

    if (js_check_stack_overflow(ctx->rt, 0)) {
        JS_ThrowStackOverflow(ctx);
        *pvalue = JS_GetException(ctx);
        return -1;
    }

    if (m->status == JS_MODULE_STATUS_EVALUATING_ASYNC ||
        m->status == JS_MODULE_STATUS_EVALUATED) {
        if (m->eval_has_exception) {
            *pvalue = JS_DupValue(ctx, m->eval_exception);
            return -1;
        } else {
            *pvalue = JS_UNDEFINED;
            return index;
        }
    }
    if (m->status == JS_MODULE_STATUS_EVALUATING) {
        *pvalue = JS_UNDEFINED;
        return index;
    }
    assert(m->status == JS_MODULE_STATUS_LINKED);

    m->status = JS_MODULE_STATUS_EVALUATING;
    m->dfs_index = index;
    m->dfs_ancestor_index = index;
    m->pending_async_dependencies = 0;
    index++;
    /* push 'm' on stack */
    m->stack_prev = *pstack_top;
    *pstack_top = m;

    for(i = 0; i < m->req_module_entries_count; i++) {
        JSReqModuleEntry *rme = &m->req_module_entries[i];
        m1 = rme->module;
        index = js_inner_module_evaluation(ctx, m1, index, pstack_top, pvalue);
        if (index < 0)
            return -1;
        assert(m1->status == JS_MODULE_STATUS_EVALUATING ||
               m1->status == JS_MODULE_STATUS_EVALUATING_ASYNC ||
               m1->status == JS_MODULE_STATUS_EVALUATED);
        if (m1->status == JS_MODULE_STATUS_EVALUATING) {
            m->dfs_ancestor_index = min_int(m->dfs_ancestor_index,
                                            m1->dfs_ancestor_index);
        } else {
            m1 = m1->cycle_root;
            assert(m1->status == JS_MODULE_STATUS_EVALUATING_ASYNC ||
                   m1->status == JS_MODULE_STATUS_EVALUATED);
            if (m1->eval_has_exception) {
                *pvalue = JS_DupValue(ctx, m1->eval_exception);
                return -1;
            }
        }
        if (m1->async_evaluation) {
            m->pending_async_dependencies++;
            if (js_resize_array(ctx, (void **)&m1->async_parent_modules, sizeof(m1->async_parent_modules[0]), &m1->async_parent_modules_size, m1->async_parent_modules_count + 1)) {
                *pvalue = JS_GetException(ctx);
                return -1;
            }
            m1->async_parent_modules[m1->async_parent_modules_count++] = m;
        }
    }

    if (m->pending_async_dependencies > 0) {
        assert(!m->async_evaluation);
        m->async_evaluation = TRUE;
        m->async_evaluation_timestamp =
            ctx->rt->module_async_evaluation_next_timestamp++;
    } else if (m->has_tla) {
        assert(!m->async_evaluation);
        m->async_evaluation = TRUE;
        m->async_evaluation_timestamp =
            ctx->rt->module_async_evaluation_next_timestamp++;
        js_execute_async_module(ctx, m);
    } else {
        if (js_execute_sync_module(ctx, m, pvalue) < 0)
            return -1;
    }

    assert(m->dfs_ancestor_index <= m->dfs_index);
    if (m->dfs_index == m->dfs_ancestor_index) {
        for(;;) {
            /* pop m1 from stack */
            m1 = *pstack_top;
            *pstack_top = m1->stack_prev;
            if (!m1->async_evaluation) {
                m1->status = JS_MODULE_STATUS_EVALUATED;
            } else {
                m1->status = JS_MODULE_STATUS_EVALUATING_ASYNC;
            }
            /* spec bug: cycle_root must be assigned before the test */
            m1->cycle_root = m;
            if (m1 == m)
                break;
        }
    }
    *pvalue = JS_UNDEFINED;
    return index;
}

/* Run the <eval> function of the module and of all its requested
   modules. Return a promise or an exception. */
JSValue js_evaluate_module(JSContext *ctx, JSModuleDef *m)
{
    JSModuleDef *m1, *stack_top;
    JSValue ret_val, result;

#ifdef DUMP_MODULE_EXEC
    js_dump_module(ctx, __func__, m);
#endif
    assert(m->status == JS_MODULE_STATUS_LINKED ||
           m->status == JS_MODULE_STATUS_EVALUATING_ASYNC ||
           m->status == JS_MODULE_STATUS_EVALUATED);
    if (m->status == JS_MODULE_STATUS_EVALUATING_ASYNC ||
        m->status == JS_MODULE_STATUS_EVALUATED) {
        m = m->cycle_root;
    }
    /* a promise may be created only on the cycle_root of a cycle */
    if (!JS_IsUndefined(m->promise))
        return JS_DupValue(ctx, m->promise);
    m->promise = JS_NewPromiseCapability(ctx, m->resolving_funcs);
    if (JS_IsException(m->promise))
        return JS_EXCEPTION;

    stack_top = NULL;
    if (js_inner_module_evaluation(ctx, m, 0, &stack_top, &result) < 0) {
        while (stack_top != NULL) {
            m1 = stack_top;
            assert(m1->status == JS_MODULE_STATUS_EVALUATING);
            m1->status = JS_MODULE_STATUS_EVALUATED;
            m1->eval_has_exception = TRUE;
            m1->eval_exception = JS_DupValue(ctx, result);
            m1->cycle_root = m; /* spec bug: should be present */
            stack_top = m1->stack_prev;
        }
        JS_FreeValue(ctx, result);
        assert(m->status == JS_MODULE_STATUS_EVALUATED);
        assert(m->eval_has_exception);
        ret_val = JS_Call(ctx, m->resolving_funcs[1], JS_UNDEFINED,
                          1, (JSValueConst *)&m->eval_exception);
        JS_FreeValue(ctx, ret_val);
    } else {
#ifdef DUMP_MODULE_EXEC
        js_dump_module(ctx, "  done", m);
#endif
        assert(m->status == JS_MODULE_STATUS_EVALUATING_ASYNC ||
               m->status == JS_MODULE_STATUS_EVALUATED);
        assert(!m->eval_has_exception);
        if (!m->async_evaluation) {
            JSValue value;
            assert(m->status == JS_MODULE_STATUS_EVALUATED);
            value = JS_UNDEFINED;
            ret_val = JS_Call(ctx, m->resolving_funcs[0], JS_UNDEFINED,
                              1, (JSValueConst *)&value);
            JS_FreeValue(ctx, ret_val);
        }
        assert(stack_top == NULL);
    }
    return JS_DupValue(ctx, m->promise);
}

__exception int js_parse_with_clause(JSParseState *s, JSReqModuleEntry *rme)
{
    JSContext *ctx = s->ctx;
    JSAtom key;
    int ret;
    const uint8_t *key_token_ptr;

    if (next_token(s))
        return -1;
    if (js_parse_expect(s, '{'))
        return -1;
    while (s->token.val != '}') {
        key_token_ptr = s->token.ptr;
        if (s->token.val == TOK_STRING) {
            key = JS_ValueToAtom(ctx, s->token.u.str.str);
            if (key == JS_ATOM_NULL)
                return -1;
        } else {
            if (!token_is_ident(s->token.val)) {
                js_parse_error(s, "identifier expected");
                return -1;
            }
            key = JS_DupAtom(ctx, s->token.u.ident.atom);
        }
        if (next_token(s))
            return -1;
        if (js_parse_expect(s, ':')) {
            JS_FreeAtom(ctx, key);
            return -1;
        }
        if (s->token.val != TOK_STRING) {
            js_parse_error_pos(s, key_token_ptr, "string expected");
            return -1;
        }
        if (JS_IsUndefined(rme->attributes)) {
            JSValue attributes = JS_NewObjectProto(ctx, JS_NULL);
            if (JS_IsException(attributes)) {
                JS_FreeAtom(ctx, key);
                return -1;
            }
            rme->attributes = attributes;
        }
        ret = JS_HasProperty(ctx, rme->attributes, key);
        if (ret != 0) {
            JS_FreeAtom(ctx, key);
            if (ret < 0)
                return -1;
            else
                return js_parse_error(s, "duplicate with key");
        }
        ret = JS_DefinePropertyValue(ctx, rme->attributes, key,
                                     JS_DupValue(ctx, s->token.u.str.str), JS_PROP_C_W_E);
        JS_FreeAtom(ctx, key);
        if (ret < 0)
            return -1;
        if (next_token(s))
            return -1;
        if (s->token.val != ',')
            break;
        if (next_token(s))
            return -1;
    }
    if (!JS_IsUndefined(rme->attributes) &&
        ctx->rt->module_check_attrs &&
        ctx->rt->module_check_attrs(ctx, ctx->rt->module_loader_opaque, rme->attributes) < 0) {
        return -1;
    }
    return js_parse_expect(s, '}');
}

/* return the module index in m->req_module_entries[] or < 0 if error */
__exception int js_parse_from_clause(JSParseState *s, JSModuleDef *m)
{
    JSAtom module_name;
    int idx;

    if (!token_is_pseudo_keyword(s, JS_ATOM_from)) {
        js_parse_error(s, "from clause expected");
        return -1;
    }
    if (next_token(s))
        return -1;
    if (s->token.val != TOK_STRING) {
        js_parse_error(s, "string expected");
        return -1;
    }
    module_name = JS_ValueToAtom(s->ctx, s->token.u.str.str);
    if (module_name == JS_ATOM_NULL)
        return -1;
    if (next_token(s)) {
        JS_FreeAtom(s->ctx, module_name);
        return -1;
    }

    idx = add_req_module_entry(s->ctx, m, module_name);
    JS_FreeAtom(s->ctx, module_name);
    if (idx < 0)
        return -1;
    if (s->token.val == TOK_WITH) {
        if (js_parse_with_clause(s, &m->req_module_entries[idx]))
            return -1;
    }
    return idx;
}

__exception int js_parse_export(JSParseState *s)
{
    JSContext *ctx = s->ctx;
    JSModuleDef *m = s->cur_func->module;
    JSAtom local_name, export_name;
    int first_export, idx, i, tok;
    JSExportEntry *me;

    if (next_token(s))
        return -1;

    tok = s->token.val;
    if (tok == TOK_CLASS) {
        return js_parse_class(s, FALSE, JS_PARSE_EXPORT_NAMED);
    } else if (tok == TOK_FUNCTION ||
               (token_is_pseudo_keyword(s, JS_ATOM_async) &&
                peek_token(s, TRUE) == TOK_FUNCTION)) {
        return js_parse_function_decl2(s, JS_PARSE_FUNC_STATEMENT,
                                       JS_FUNC_NORMAL, JS_ATOM_NULL,
                                       s->token.ptr,
                                       JS_PARSE_EXPORT_NAMED, NULL);
    }

    if (next_token(s))
        return -1;

    switch(tok) {
    case '{':
        first_export = m->export_entries_count;
        while (s->token.val != '}') {
            if (!token_is_ident(s->token.val)) {
                js_parse_error(s, "identifier expected");
                return -1;
            }
            local_name = JS_DupAtom(ctx, s->token.u.ident.atom);
            export_name = JS_ATOM_NULL;
            if (next_token(s))
                goto fail;
            if (token_is_pseudo_keyword(s, JS_ATOM_as)) {
                if (next_token(s))
                    goto fail;
                if (s->token.val == TOK_STRING) {
                    if (js_string_find_invalid_codepoint(JS_VALUE_GET_STRING(s->token.u.str.str)) >= 0) {
                        js_parse_error(s, "contains unpaired surrogate");
                        goto fail;
                    }
                    export_name = JS_ValueToAtom(s->ctx, s->token.u.str.str);
                    if (export_name == JS_ATOM_NULL)
                        goto fail;
                } else {
                    if (!token_is_ident(s->token.val)) {
                        js_parse_error(s, "identifier expected");
                        goto fail;
                    }
                    export_name = JS_DupAtom(ctx, s->token.u.ident.atom);
                }
                if (next_token(s)) {
                fail:
                    JS_FreeAtom(ctx, local_name);
                fail1:
                    JS_FreeAtom(ctx, export_name);
                    return -1;
                }
            } else {
                export_name = JS_DupAtom(ctx, local_name);
            }
            me = add_export_entry(s, m, local_name, export_name,
                                  JS_EXPORT_TYPE_LOCAL);
            JS_FreeAtom(ctx, local_name);
            JS_FreeAtom(ctx, export_name);
            if (!me)
                return -1;
            if (s->token.val != ',')
                break;
            if (next_token(s))
                return -1;
        }
        if (js_parse_expect(s, '}'))
            return -1;
        if (token_is_pseudo_keyword(s, JS_ATOM_from)) {
            idx = js_parse_from_clause(s, m);
            if (idx < 0)
                return -1;
            for(i = first_export; i < m->export_entries_count; i++) {
                me = &m->export_entries[i];
                me->export_type = JS_EXPORT_TYPE_INDIRECT;
                me->u.req_module_idx = idx;
            }
        }
        break;
    case '*':
        if (token_is_pseudo_keyword(s, JS_ATOM_as)) {
            /* export ns from */
            if (next_token(s))
                return -1;
            if (!token_is_ident(s->token.val)) {
                js_parse_error(s, "identifier expected");
                return -1;
            }
            export_name = JS_DupAtom(ctx, s->token.u.ident.atom);
            if (next_token(s))
                goto fail1;
            idx = js_parse_from_clause(s, m);
            if (idx < 0)
                goto fail1;
            me = add_export_entry(s, m, JS_ATOM__star_, export_name,
                                  JS_EXPORT_TYPE_INDIRECT);
            JS_FreeAtom(ctx, export_name);
            if (!me)
                return -1;
            me->u.req_module_idx = idx;
        } else {
            idx = js_parse_from_clause(s, m);
            if (idx < 0)
                return -1;
            if (add_star_export_entry(ctx, m, idx) < 0)
                return -1;
        }
        break;
    case TOK_DEFAULT:
        if (s->token.val == TOK_CLASS) {
            return js_parse_class(s, FALSE, JS_PARSE_EXPORT_DEFAULT);
        } else if (s->token.val == TOK_FUNCTION ||
                   (token_is_pseudo_keyword(s, JS_ATOM_async) &&
                    peek_token(s, TRUE) == TOK_FUNCTION)) {
            return js_parse_function_decl2(s, JS_PARSE_FUNC_STATEMENT,
                                           JS_FUNC_NORMAL, JS_ATOM_NULL,
                                           s->token.ptr,
                                           JS_PARSE_EXPORT_DEFAULT, NULL);
        } else {
            if (js_parse_assign_expr(s))
                return -1;
        }
        /* set the name of anonymous functions */
        set_object_name(s, JS_ATOM_default);

        /* store the value in the _default_ global variable and export
           it */
        local_name = JS_ATOM__default_;
        if (define_var(s, s->cur_func, local_name, JS_VAR_DEF_LET) < 0)
            return -1;
        emit_op(s, OP_scope_put_var_init);
        emit_atom(s, local_name);
        emit_u16(s, 0);

        if (!add_export_entry(s, m, local_name, JS_ATOM_default,
                              JS_EXPORT_TYPE_LOCAL))
            return -1;
        break;
    case TOK_VAR:
    case TOK_LET:
    case TOK_CONST:
        return js_parse_var(s, TRUE, tok, TRUE);
    default:
        return js_parse_error(s, "invalid export syntax");
    }
    return js_parse_expect_semi(s);
}

int add_closure_var(JSContext *ctx, JSFunctionDef *s,
                           JSClosureTypeEnum closure_type,
                           int var_idx, JSAtom var_name,
                           BOOL is_const, BOOL is_lexical,
                           JSVarKindEnum var_kind);

int add_import(JSParseState *s, JSModuleDef *m,
                      JSAtom local_name, JSAtom import_name, BOOL is_star)
{
    JSContext *ctx = s->ctx;
    int i, var_idx;
    JSImportEntry *mi;

    if (local_name == JS_ATOM_arguments || local_name == JS_ATOM_eval)
        return js_parse_error(s, "invalid import binding");

    if (local_name != JS_ATOM_default) {
        for (i = 0; i < s->cur_func->closure_var_count; i++) {
            if (s->cur_func->closure_var[i].var_name == local_name)
                return js_parse_error(s, "duplicate import binding");
        }
    }

    var_idx = add_closure_var(ctx, s->cur_func,
                              is_star ? JS_CLOSURE_MODULE_DECL : JS_CLOSURE_MODULE_IMPORT,
                              m->import_entries_count,
                              local_name, TRUE, TRUE, JS_VAR_NORMAL);
    if (var_idx < 0)
        return -1;
    if (js_resize_array(ctx, (void **)&m->import_entries,
                        sizeof(JSImportEntry),
                        &m->import_entries_size,
                        m->import_entries_count + 1))
        return -1;
    mi = &m->import_entries[m->import_entries_count++];
    mi->import_name = JS_DupAtom(ctx, import_name);
    mi->var_idx = var_idx;
    mi->is_star = is_star;
    return 0;
}

__exception int js_parse_import(JSParseState *s)
{
    JSContext *ctx = s->ctx;
    JSModuleDef *m = s->cur_func->module;
    JSAtom local_name, import_name, module_name;
    int first_import, i, idx;

    if (next_token(s))
        return -1;

    first_import = m->import_entries_count;
    if (s->token.val == TOK_STRING) {
        module_name = JS_ValueToAtom(ctx, s->token.u.str.str);
        if (module_name == JS_ATOM_NULL)
            return -1;
        if (next_token(s)) {
            JS_FreeAtom(ctx, module_name);
            return -1;
        }
        idx = add_req_module_entry(ctx, m, module_name);
        JS_FreeAtom(ctx, module_name);
        if (idx < 0)
            return -1;
        if (s->token.val == TOK_WITH) {
            if (js_parse_with_clause(s, &m->req_module_entries[idx]))
                return -1;
        }
    } else {
        if (s->token.val == TOK_IDENT) {
            if (s->token.u.ident.is_reserved) {
                return js_parse_error_reserved_identifier(s);
            }
            /* "default" import */
            local_name = JS_DupAtom(ctx, s->token.u.ident.atom);
            import_name = JS_ATOM_default;
            if (next_token(s))
                goto fail;
            if (add_import(s, m, local_name, import_name, FALSE))
                goto fail;
            JS_FreeAtom(ctx, local_name);

            if (s->token.val != ',')
                goto end_import_clause;
            if (next_token(s))
                return -1;
        }

        if (s->token.val == '*') {
            /* name space import */
            if (next_token(s))
                return -1;
            if (!token_is_pseudo_keyword(s, JS_ATOM_as))
                return js_parse_error(s, "expecting 'as'");
            if (next_token(s))
                return -1;
            if (!token_is_ident(s->token.val)) {
                js_parse_error(s, "identifier expected");
                return -1;
            }
            local_name = JS_DupAtom(ctx, s->token.u.ident.atom);
            import_name = JS_ATOM__star_;
            if (next_token(s))
                goto fail;
            if (add_import(s, m, local_name, import_name, TRUE))
                goto fail;
            JS_FreeAtom(ctx, local_name);
        } else if (s->token.val == '{') {
            if (next_token(s))
                return -1;

            while (s->token.val != '}') {
                BOOL is_string;
                if (s->token.val == TOK_STRING) {
                    is_string = TRUE;
                    if (js_string_find_invalid_codepoint(JS_VALUE_GET_STRING(s->token.u.str.str)) >= 0) {
                        js_parse_error(s, "contains unpaired surrogate");
                        return -1;
                    }
                    import_name = JS_ValueToAtom(s->ctx, s->token.u.str.str);
                    if (import_name == JS_ATOM_NULL)
                        return -1;
                } else {
                    is_string = FALSE;
                    if (!token_is_ident(s->token.val)) {
                        js_parse_error(s, "identifier expected");
                        return -1;
                    }
                    import_name = JS_DupAtom(ctx, s->token.u.ident.atom);
                }
                local_name = JS_ATOM_NULL;
                if (next_token(s))
                    goto fail;
                if (token_is_pseudo_keyword(s, JS_ATOM_as)) {
                    if (next_token(s))
                        goto fail;
                    if (!token_is_ident(s->token.val)) {
                        js_parse_error(s, "identifier expected");
                        goto fail;
                    }
                    local_name = JS_DupAtom(ctx, s->token.u.ident.atom);
                    if (next_token(s))
                        goto fail;
                } else {
                    if (is_string) {
                        js_parse_error(s, "expecting 'as'");
                    fail:
                        JS_FreeAtom(ctx, local_name);
                        JS_FreeAtom(ctx, import_name);
                        return -1;
                    }
                    local_name = JS_DupAtom(ctx, import_name);
                }
                if (add_import(s, m, local_name, import_name, FALSE))
                    goto fail;
                JS_FreeAtom(ctx, local_name);
                JS_FreeAtom(ctx, import_name);
                if (s->token.val != ',')
                    break;
                if (next_token(s))
                    return -1;
            }
            if (js_parse_expect(s, '}'))
                return -1;
        }
    end_import_clause:
        idx = js_parse_from_clause(s, m);
        if (idx < 0)
            return -1;
    }
    for(i = first_import; i < m->import_entries_count; i++)
        m->import_entries[i].req_module_idx = idx;

    return js_parse_expect_semi(s);
}

__exception int js_parse_source_element(JSParseState *s)
{
    JSFunctionDef *fd = s->cur_func;
    int tok;

    if (s->token.val == TOK_FUNCTION ||
        (token_is_pseudo_keyword(s, JS_ATOM_async) &&
         peek_token(s, TRUE) == TOK_FUNCTION)) {
        if (js_parse_function_decl(s, JS_PARSE_FUNC_STATEMENT,
                                   JS_FUNC_NORMAL, JS_ATOM_NULL,
                                   s->token.ptr))
            return -1;
    } else if (s->token.val == TOK_EXPORT && fd->module) {
        if (js_parse_export(s))
            return -1;
    } else if (s->token.val == TOK_IMPORT && fd->module &&
               ((tok = peek_token(s, FALSE)) != '(' && tok != '.'))  {
        /* the peek_token is needed to avoid confusion with ImportCall
           (dynamic import) or import.meta */
        if (js_parse_import(s))
            return -1;
    } else {
        if (js_parse_statement_or_decl(s, DECL_MASK_ALL))
            return -1;
    }
    return 0;
}
