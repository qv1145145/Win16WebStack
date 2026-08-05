/*
 * QuickJS JS_CallInternal 拆分主函数
 * 按操作码类别将原有3000行函数拆分为若干辅助静态函数，解决16位编译器代码段溢出问题。
 * 本文件仅包含主函数 JS_CallInternal，辅助函数的声明与实现见后续文件。
 */

#include "js_internal.h"


#ifndef OP_DEFINE_METHOD_METHOD
#define OP_DEFINE_METHOD_METHOD 0
#define OP_DEFINE_METHOD_GETTER 1
#define OP_DEFINE_METHOD_SETTER 2
#define OP_DEFINE_METHOD_ENUMERABLE 4
#endif

#ifndef JS_THROW_VAR_RO
#define JS_THROW_VAR_RO 0
#define JS_THROW_VAR_REDECL 1
#define JS_THROW_VAR_UNINITIALIZED 2
#define JS_THROW_ERROR_DELETE_SUPER 3
#define JS_THROW_ERROR_ITERATOR_THROW 4
#endif

/* 状态结构体，用于减少辅助函数参数数量 */
typedef struct CallState {
    JSContext *ctx;
    JSRuntime *rt;
    JSStackFrame *sf;
    JSFunctionBytecode *b;
    const uint8_t *pc;
    JSValue *sp;
    JSValue *var_buf;
    JSValue *arg_buf;
    JSVarRef **var_refs;
    JSValue *stack_buf;
    JSValue ret_val;
    int argc;
    JSValue *argv;
    JSValue this_obj;
    JSValue new_target;
    int flags;
    int exception;
	JSValue *local_buf;
} CallState;

/* 前向声明辅助函数（具体实现在后续文件） */
int do_push(CallState *cs, int op);
int do_stack(CallState *cs, int op);
int do_arith(CallState *cs, int op);
int do_compare(CallState *cs, int op);
int do_call(CallState *cs, int op);
int do_object(CallState *cs, int op);
int do_array(CallState *cs, int op);
int do_var(CallState *cs, int op);
int do_control(CallState *cs, int op);
int do_misc(CallState *cs, int op);

