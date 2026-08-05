/*
 * QuickJS Javascript Engine Header File
 *
 * Copyright (c) 2017-2025 Fabrice Bellard
 * Copyright (c) 2017-2025 Charlie Gordon
 *
 * Win16 port modifications copyright (c) 2026 邹瑾烨
 * This file is modified for Windows 16-bit platform.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#if defined(_WIN16) && !defined(_WIN32)
#define _WIN32
#endif

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>
#include <time.h>
#include <fenv.h>
#include <math.h>
#if defined(__APPLE__)
#include <malloc/malloc.h>
#elif defined(__linux__) || defined(__GLIBC__)
#include <malloc.h>
#elif defined(__FreeBSD__)
#include <malloc_np.h>
#endif

#include "cutils.h"
#include "list.h"
#include "quickjs.h"
#include "libregexp.h"
#include "libunicode.h"
#include "dtoa.h"

/* win16_quickjs.h – Win16 fixes for QuickJS core */

#ifndef WIN16_QUICKJS_H
#define WIN16_QUICKJS_H

#ifdef _WIN16

#include "win16_compat.h"   /* 你已有的控制台桩 */

/* 禁用多线程 – 在包含任何 pthread 头之前定义 */
// #define JS_THREADSAFE 0
#define __EMSCRIPTEN__

#define minimum_length(n)  n

/* 必须在包含任何使用 NAN 的头文件之前定义 */
#ifdef __WATCOMC__
#undef  NAN
#define NAN       (0.0/0.0)
#undef  INFINITY
#define INFINITY  (1.0/0.0)
#endif

/* 提供 QuickJS 版本字符串（消除 fprintf 错误） */
#define CONFIG_VERSION "2021-03-27"

/* 如果 cutils.c 使用了 _msize，提供空实现 */
#define _msize(ptr)   (0)

#endif /* _WIN16 */

#endif /* WIN16_QUICKJS_H */

#define OPTIMIZE         1
#define SHORT_OPCODES    1
#if defined(__EMSCRIPTEN__)
#define DIRECT_DISPATCH  0
#else
#define DIRECT_DISPATCH  1
#endif

#if defined(__APPLE__)
#define MALLOC_OVERHEAD  0
#else
#define MALLOC_OVERHEAD  8
#endif

#if !defined(_WIN32)
/* define it if printf uses the RNDN rounding mode instead of RNDNA */
#define CONFIG_PRINTF_RNDN
#endif

/* define to include Atomics.* operations which depend on the OS
   threads */
#if !defined(__EMSCRIPTEN__)
#define CONFIG_ATOMICS
#endif

#if !defined(__EMSCRIPTEN__)
/* enable stack limitation */
#define CONFIG_STACK_CHECK
#endif


/* dump object free */
//#define DUMP_FREE
//#define DUMP_CLOSURE
/* dump the bytecode of the compiled functions: combination of bits
   1: dump pass 3 final byte code
   2: dump pass 2 code
   4: dump pass 1 code
   8: dump stdlib functions
  16: dump bytecode in hex
  32: dump line number table
  64: dump compute_stack_size
 */
//#define DUMP_BYTECODE  (1)
/* dump the occurence of the automatic GC */
//#define DUMP_GC
/* dump objects freed by the garbage collector */
//#define DUMP_GC_FREE
/* dump objects leaking when freeing the runtime */
//#define DUMP_LEAKS  1
/* dump memory usage before running the garbage collector */
//#define DUMP_MEM
//#define DUMP_OBJECTS    /* dump objects in JS_FreeContext */
//#define DUMP_ATOMS      /* dump atoms in JS_FreeContext */
//#define DUMP_SHAPES     /* dump shapes in JS_FreeContext */
//#define DUMP_MODULE_RESOLVE
//#define DUMP_MODULE_EXEC
//#define DUMP_PROMISE
//#define DUMP_READ_OBJECT
//#define DUMP_ROPE_REBALANCE
/* add asm labels to each opcode so that it is easier to see the generated code */
//#define OPCODE_ASM_LABEL

/* test the GC by forcing it before each object allocation */
// #define FORCE_GC_AT_MALLOC

#ifdef CONFIG_ATOMICS
#include <pthread.h>
#include <stdatomic.h>
#include <errno.h>
#endif

enum {
    /* classid tag        */    /* union usage   | properties */
    JS_CLASS_OBJECT = 1,        /* must be first */
    JS_CLASS_ARRAY,             /* u.array       | length */
    JS_CLASS_ERROR,
    JS_CLASS_NUMBER,            /* u.object_data */
    JS_CLASS_STRING,            /* u.object_data */
    JS_CLASS_BOOLEAN,           /* u.object_data */
    JS_CLASS_SYMBOL,            /* u.object_data */
    JS_CLASS_ARGUMENTS,         /* u.array       | length */
    JS_CLASS_MAPPED_ARGUMENTS,  /* u.array       | length */
    JS_CLASS_DATE,              /* u.object_data */
    JS_CLASS_MODULE_NS,
    JS_CLASS_C_FUNCTION,        /* u.cfunc */
    JS_CLASS_BYTECODE_FUNCTION, /* u.func */
    JS_CLASS_BOUND_FUNCTION,    /* u.bound_function */
    JS_CLASS_C_FUNCTION_DATA,   /* u.c_function_data_record */
    JS_CLASS_GENERATOR_FUNCTION, /* u.func */
    JS_CLASS_FOR_IN_ITERATOR,   /* u.for_in_iterator */
    JS_CLASS_REGEXP,            /* u.regexp */
    JS_CLASS_ARRAY_BUFFER,      /* u.array_buffer */
    JS_CLASS_SHARED_ARRAY_BUFFER, /* u.array_buffer */
    JS_CLASS_UINT8C_ARRAY,      /* u.array (typed_array) */
    JS_CLASS_INT8_ARRAY,        /* u.array (typed_array) */
    JS_CLASS_UINT8_ARRAY,       /* u.array (typed_array) */
    JS_CLASS_INT16_ARRAY,       /* u.array (typed_array) */
    JS_CLASS_UINT16_ARRAY,      /* u.array (typed_array) */
    JS_CLASS_INT32_ARRAY,       /* u.array (typed_array) */
    JS_CLASS_UINT32_ARRAY,      /* u.array (typed_array) */
    JS_CLASS_BIG_INT64_ARRAY,   /* u.array (typed_array) */
    JS_CLASS_BIG_UINT64_ARRAY,  /* u.array (typed_array) */
    JS_CLASS_FLOAT16_ARRAY,     /* u.array (typed_array) */
    JS_CLASS_FLOAT32_ARRAY,     /* u.array (typed_array) */
    JS_CLASS_FLOAT64_ARRAY,     /* u.array (typed_array) */
    JS_CLASS_DATAVIEW,          /* u.typed_array */
    JS_CLASS_BIG_INT,           /* u.object_data */
    JS_CLASS_MAP,               /* u.map_state */
    JS_CLASS_SET,               /* u.map_state */
    JS_CLASS_WEAKMAP,           /* u.map_state */
    JS_CLASS_WEAKSET,           /* u.map_state */
    JS_CLASS_ITERATOR,          /* u.map_iterator_data */
    JS_CLASS_ITERATOR_CONCAT,   /* u.iterator_concat_data */
    JS_CLASS_ITERATOR_HELPER,   /* u.iterator_helper_data */
    JS_CLASS_ITERATOR_WRAP,     /* u.iterator_wrap_data */
    JS_CLASS_MAP_ITERATOR,      /* u.map_iterator_data */
    JS_CLASS_SET_ITERATOR,      /* u.map_iterator_data */
    JS_CLASS_ARRAY_ITERATOR,    /* u.array_iterator_data */
    JS_CLASS_STRING_ITERATOR,   /* u.array_iterator_data */
    JS_CLASS_REGEXP_STRING_ITERATOR,   /* u.regexp_string_iterator_data */
    JS_CLASS_GENERATOR,         /* u.generator_data */
    JS_CLASS_GLOBAL_OBJECT,     /* u.global_object */
    JS_CLASS_RAWJSON,
    JS_CLASS_PROXY,             /* u.proxy_data */
    JS_CLASS_PROMISE,           /* u.promise_data */
    JS_CLASS_PROMISE_RESOLVE_FUNCTION,  /* u.promise_function_data */
    JS_CLASS_PROMISE_REJECT_FUNCTION,   /* u.promise_function_data */
    JS_CLASS_ASYNC_FUNCTION,            /* u.func */
    JS_CLASS_ASYNC_FUNCTION_RESOLVE,    /* u.async_function_data */
    JS_CLASS_ASYNC_FUNCTION_REJECT,     /* u.async_function_data */
    JS_CLASS_ASYNC_FROM_SYNC_ITERATOR,  /* u.async_from_sync_iterator_data */
    JS_CLASS_ASYNC_GENERATOR_FUNCTION,  /* u.func */
    JS_CLASS_ASYNC_GENERATOR,   /* u.async_generator_data */
    JS_CLASS_WEAK_REF,
    JS_CLASS_FINALIZATION_REGISTRY,

    JS_CLASS_INIT_COUNT, /* last entry for predefined classes */
};

/* number of typed array types */
#define JS_TYPED_ARRAY_COUNT  (JS_CLASS_FLOAT64_ARRAY - JS_CLASS_UINT8C_ARRAY + 1)
extern uint8_t const typed_array_size_log2[JS_TYPED_ARRAY_COUNT];
#define typed_array_size_log2(classid)  (typed_array_size_log2[(classid)- JS_CLASS_UINT8C_ARRAY])

typedef enum JSErrorEnum {
    JS_EVAL_ERROR,
    JS_RANGE_ERROR,
    JS_REFERENCE_ERROR,
    JS_SYNTAX_ERROR,
    JS_TYPE_ERROR,
    JS_URI_ERROR,
    JS_INTERNAL_ERROR,
    JS_AGGREGATE_ERROR,

    JS_NATIVE_ERROR_COUNT, /* number of different NativeError objects */
} JSErrorEnum;

/* the variable and scope indexes must fit on 16 bits. The (-1) and
   ARG_SCOPE_END values are reserved. */
#define JS_MAX_LOCAL_VARS 65534
#define JS_STACK_SIZE_MAX 65534
#define JS_STRING_LEN_MAX ((1 << 30) - 1)

/* strings <= this length are not concatenated using ropes. if too
   small, the rope memory overhead becomes high. */
#define JS_STRING_ROPE_SHORT_LEN  512
/* specific threshold for initial rope use */
#define JS_STRING_ROPE_SHORT2_LEN 8192
/* rope depth at which we rebalance */
#define JS_STRING_ROPE_MAX_DEPTH 60

#define ROPE_N_BUCKETS 44

#define JS_RADIX_MAX 36

#define JS_ATOM_TAG_INT (1U << 31)
#define JS_ATOM_MAX_INT (JS_ATOM_TAG_INT - 1)
#define JS_ATOM_MAX     ((1U << 30) - 1)

/* return the max count from the hash size */
#define JS_ATOM_COUNT_RESIZE(n) ((n) * 2)

#define __exception __attribute__((warn_unused_result))

typedef struct JSShape JSShape;
typedef struct JSString JSString;
typedef struct JSString JSAtomStruct;
typedef struct JSObject JSObject;

#define JS_VALUE_GET_OBJ(v) ((JSObject *)JS_VALUE_GET_PTR(v))
#define JS_VALUE_GET_STRING(v) ((JSString *)JS_VALUE_GET_PTR(v))
#define JS_VALUE_GET_STRING_ROPE(v) ((JSStringRope *)JS_VALUE_GET_PTR(v))

typedef enum {
    JS_GC_PHASE_NONE,
    JS_GC_PHASE_DECREF,
    JS_GC_PHASE_REMOVE_CYCLES,
} JSGCPhaseEnum;

enum OPCodeEnum {
#define FMT(f)
#define DEF(id, size, n_pop, n_push, f) OP_ ## id,
#define def(id, size, n_pop, n_push, f)
#include "quickjs-opcode.h"
#undef def
#undef DEF
#undef FMT
    OP_COUNT,
    OP_TEMP_START = OP_nop + 1,
    OP___dummy = OP_TEMP_START - 1,
#define FMT(f)
#define DEF(id, size, n_pop, n_push, f)
#define def(id, size, n_pop, n_push, f) OP_ ## id,
#include "quickjs-opcode.h"
#undef def
#undef DEF
#undef FMT
    OP_TEMP_END,
};
typedef enum OPCodeEnum OPCodeEnum;

/* JS malloc */

#define JS_MALLOC_ALIGN 8
#define JS_MALLOC_ARENA_SIZE 4096
#define JS_MALLOC_BLOCK_SIZE_COUNT 31
#define JS_MALLOC_MIN_SMALL_SIZE 16
#define JS_MALLOC_MAX_SMALL_SIZE 512
#if defined(__SANITIZE_ADDRESS__)
/* use the host malloc() for all allocations */
#define JS_MALLOC_LARGE_BLOCKS_ONLY 1
#else
#define JS_MALLOC_LARGE_BLOCKS_ONLY 0
#endif

/* allow iteration among the allocated blocks. Currently not used. May
   be used to suppress the memory overhead of JSGCObjectHeader */
//#define JS_MALLOC_USE_ITER

#define FREE_NIL 0xffff

/* 8 byte header */
/* Notes:
   - the header is necessary at least to recover a pointer to
     JSMallocArena because we don't want to enforce a page
     alignment on the system malloc().
   - could store the block offset instead of (block_idx,
   block_size_idx), but it would require a division to recover the block
   index.
*/
typedef struct JSMallocBlockHeader {
    union {
        uint16_t block_idx; /* FREE_NIL if large block */
        uint16_t free_next; /* FREE_NIL if none */
    } u;
    uint8_t block_size_idx;
    uint8_t gc_obj_type : 7;
    uint8_t mark : 1;
    int32_t ref_count;
    __attribute__((aligned(JS_MALLOC_ALIGN))) uint8_t user_data[];
} JSMallocBlockHeader;

typedef struct JSMallocLargeBlockHeader {
#ifdef JS_MALLOC_USE_ITER
    struct list_head link;
#endif
    JSMallocBlockHeader header;
} JSMallocLargeBlockHeader;

typedef struct {
    struct list_head free_link;
    struct list_head link;
    uint8_t block_size_idx;
    uint16_t n_used_blocks; /* number of allocated blocks */
    uint16_t n_blocks; /* total number of blocks */
    uint16_t first_free_block; /* FREE_NIL if none */
#ifdef JS_MALLOC_USE_ITER
    /* bit set to 1 for allocated block */
    uint32_t bitmap[((JS_MALLOC_ARENA_SIZE / JS_MALLOC_MIN_SMALL_SIZE) + 31) / 32];
#endif
    /* n_blocks memory blocks of identical size */
    __attribute__((aligned(JS_MALLOC_ALIGN))) uint8_t blocks[];
} JSMallocArena;

typedef struct {
    struct list_head arena_list[JS_MALLOC_BLOCK_SIZE_COUNT]; /* list of JSMallocArena.link (all arenas) */
    struct list_head free_arena_list[JS_MALLOC_BLOCK_SIZE_COUNT]; /* list of JSMallocArena.free_link (arenas where n_used_blocks < n_blocks) */
#ifdef JS_MALLOC_USE_ITER
    struct list_head large_block_list; /* list of JSMallocLargeBlockHeader.link */
#endif
    __attribute__((aligned(JS_MALLOC_ALIGN))) uint8_t zero_size_block[sizeof(JSMallocBlockHeader)];

    /* callbacks to the host malloc */
    JSMallocFunctions mf;
    JSMallocState malloc_state;
} JSMallocContext;

/* end JS Malloc */

struct JSRuntime {
    JSMallocContext malloc_ctx;
    const char *rt_info;

    int atom_hash_size; /* power of two */
    int atom_count;
    int atom_size;
    int atom_count_resize; /* resize hash table at this count */
    uint32_t *atom_hash;
    JSAtomStruct **atom_array;
    int atom_free_index; /* 0 = none */

    int class_count;    /* size of class_array */
    JSClass *class_array;

    struct list_head context_list; /* list of JSContext.link */
    /* list of JSGCObjectHeader.link. List of allocated GC objects (used
       by the garbage collector) */
    struct list_head gc_obj_list;
    /* list of JSGCObjectHeader.link. Used during JS_FreeValueRT() */
    struct list_head gc_zero_ref_count_list;
    struct list_head tmp_obj_list; /* used during GC */
    JSGCPhaseEnum gc_phase : 8;
    uint32_t malloc_gc_threshold;
    struct list_head weakref_list; /* list of JSWeakRefHeader.link */
#ifdef DUMP_LEAKS
    struct list_head string_list; /* list of JSString.link */
#endif
    /* stack limitation */
    uintptr_t stack_size; /* in bytes, 0 if no limit */
    uintptr_t stack_top;
    uintptr_t stack_limit; /* lower stack limit */

    JSValue current_exception;
    /* true if the current exception cannot be catched */
    BOOL current_exception_is_uncatchable : 8;
    /* true if inside an out of memory error, to avoid recursing */
    BOOL in_out_of_memory : 8;

    struct JSStackFrame *current_stack_frame;

    JSInterruptHandler *interrupt_handler;
    void *interrupt_opaque;

    JSHostPromiseRejectionTracker *host_promise_rejection_tracker;
    void *host_promise_rejection_tracker_opaque;

    struct list_head job_list; /* list of JSJobEntry.link */

    JSModuleNormalizeFunc *module_normalize_func;
    BOOL module_loader_has_attr;
    union {
        JSModuleLoaderFunc *module_loader_func;
        JSModuleLoaderFunc2 *module_loader_func2;
    } u;
    JSModuleCheckSupportedImportAttributes *module_check_attrs;
    void *module_loader_opaque;
    /* timestamp for internal use in module evaluation */
    int64_t module_async_evaluation_next_timestamp;

    BOOL can_block : 8; /* TRUE if Atomics.wait can block */
    /* used to allocate, free and clone SharedArrayBuffers */
    JSSharedArrayBufferFunctions sab_funcs;
    /* see JS_SetStripInfo() */
    uint8_t strip_flags;

    /* Shape hash table */
    int shape_hash_bits;
    int shape_hash_size;
    int shape_hash_count; /* number of hashed shapes */
    JSShape **shape_hash;
    void *user_opaque;
};

struct JSClass {
    uint32_t class_id; /* 0 means free entry */
    JSAtom class_name;
    JSClassFinalizer *finalizer;
    JSClassGCMark *gc_mark;
    JSClassCall *call;
    /* pointers for exotic behavior, can be NULL if none are present */
    const JSClassExoticMethods *exotic;
};

#define JS_MODE_STRICT (1 << 0)
#define JS_MODE_ASYNC  (1 << 2) /* async function */
#define JS_MODE_BACKTRACE_BARRIER (1 << 3) /* stop backtrace before this frame */

typedef struct JSStackFrame {
    struct JSStackFrame *prev_frame; /* NULL if first stack frame */
    JSValue cur_func; /* current function, JS_UNDEFINED if the frame is detached */
    JSValue *arg_buf; /* arguments */
    JSValue *var_buf; /* variables */
    struct JSVarRef **var_refs; /* references to arguments or local variables */
    const uint8_t *cur_pc; /* only used in bytecode functions : PC of the
                        instruction after the call */
    int arg_count;
    int js_mode; /* not supported for C functions */
    /* only used in generators. Current stack pointer value. NULL if
       the function is running. */
    JSValue *cur_sp;
} JSStackFrame;

typedef enum {
    JS_GC_OBJ_TYPE_JS_OBJECT,
    JS_GC_OBJ_TYPE_FUNCTION_BYTECODE,
    JS_GC_OBJ_TYPE_SHAPE,
    JS_GC_OBJ_TYPE_VAR_REF,
    JS_GC_OBJ_TYPE_ASYNC_FUNCTION,
    JS_GC_OBJ_TYPE_JS_CONTEXT,
    JS_GC_OBJ_TYPE_MODULE,
} JSGCObjectTypeEnum;

/* header for GC objects. GC objects are C data structures with a
   reference count that can reference other GC objects. JS Objects are
   a particular type of GC object. */
struct JSGCObjectHeader {
    struct list_head link;
};

typedef enum {
    JS_WEAKREF_TYPE_MAP,
    JS_WEAKREF_TYPE_WEAKREF,
    JS_WEAKREF_TYPE_FINREC,
} JSWeakRefHeaderTypeEnum;

typedef struct {
    struct list_head link;
    JSWeakRefHeaderTypeEnum weakref_type;
} JSWeakRefHeader;

typedef struct JSVarRef {
    JSGCObjectHeader header; /* must come first */
    uint8_t is_detached;
    uint8_t is_lexical; /* only used with global variables */
    uint8_t is_const; /* only used with global variables */
    JSValue *pvalue; /* pointer to the value, either on the stack or
                        to 'value' */
    union {
        JSValue value; /* used when is_detached = TRUE */
        struct {
            uint16_t var_ref_idx; /* index in JSStackFrame.var_refs[] */
            JSStackFrame *stack_frame;
        }; /* used when is_detached = FALSE */
    };
} JSVarRef;

/* bigint */

#if JS_LIMB_BITS == 32

typedef int32_t js_slimb_t;
typedef uint32_t js_limb_t;
typedef int64_t js_sdlimb_t;
typedef uint64_t js_dlimb_t;

#define JS_LIMB_DIGITS 9

#else

typedef __int128 int128_t;
typedef unsigned __int128 uint128_t;
typedef int64_t js_slimb_t;
typedef uint64_t js_limb_t;
typedef int128_t js_sdlimb_t;
typedef uint128_t js_dlimb_t;

#define JS_LIMB_DIGITS 19

#endif

typedef struct JSBigInt {
    uint32_t len; /* number of limbs, >= 1 */
    js_limb_t tab[]; /* two's complement representation, always
                        normalized so that 'len' is the minimum
                        possible length >= 1 */
} JSBigInt;

/* this bigint structure can hold a 64 bit integer */
typedef struct {
    js_limb_t big_int_buf[sizeof(JSBigInt) / sizeof(js_limb_t)]; /* for JSBigInt */
    /* must come just after */
    js_limb_t tab[(64 + JS_LIMB_BITS - 1) / JS_LIMB_BITS];
} JSBigIntBuf;

typedef enum {
    JS_AUTOINIT_ID_PROTOTYPE,
    JS_AUTOINIT_ID_MODULE_NS,
    JS_AUTOINIT_ID_PROP,
} JSAutoInitIDEnum;

/* must be large enough to have a negligible runtime cost and small
   enough to call the interrupt callback often. */
#define JS_INTERRUPT_COUNTER_INIT 10000

struct JSContext {
    JSGCObjectHeader header; /* must come first */
    JSRuntime *rt;
    struct list_head link;

    uint16_t binary_object_count;
    int binary_object_size;

    JSShape *array_shape;   /* initial shape for Array objects */
    JSShape *arguments_shape;  /* shape for arguments objects */
    JSShape *mapped_arguments_shape;  /* shape for mapped arguments objects */
    JSShape *regexp_shape;  /* shape for regexp objects */
    JSShape *regexp_result_shape;  /* shape for regexp result objects */

    JSValue *class_proto;
    JSValue function_proto;
    JSValue function_ctor;
    JSValue array_ctor;
    JSValue regexp_ctor;
    JSValue promise_ctor;
    JSValue native_error_proto[JS_NATIVE_ERROR_COUNT];
    JSValue iterator_ctor;
    JSValue async_iterator_proto;
    JSValue array_proto_values;
    JSValue throw_type_error;
    JSValue eval_obj;

    JSValue global_obj; /* global object */
    JSValue global_var_obj; /* contains the global let/const definitions */

    uint64_t random_state;

    /* when the counter reaches zero, JSRutime.interrupt_handler is called */
    int interrupt_counter;

    struct list_head loaded_modules; /* list of JSModuleDef.link */

    /* if NULL, RegExp compilation is not supported */
    JSValue (*compile_regexp)(JSContext *ctx, JSValueConst pattern,
                              JSValueConst flags);
    /* if NULL, eval is not supported */
    JSValue (*eval_internal)(JSContext *ctx, JSValueConst this_obj,
                             const char *input, size_t input_len,
                             const char *filename, int flags, int scope_idx);
    void *user_opaque;
};

typedef union JSFloat64Union {
    double d;
    uint64_t u64;
    uint32_t u32[2];
} JSFloat64Union;

enum {
    JS_ATOM_TYPE_STRING = 1,
    JS_ATOM_TYPE_GLOBAL_SYMBOL,
    JS_ATOM_TYPE_SYMBOL,
    JS_ATOM_TYPE_PRIVATE,
};

typedef enum {
    JS_ATOM_KIND_STRING,
    JS_ATOM_KIND_SYMBOL,
    JS_ATOM_KIND_PRIVATE,
} JSAtomKindEnum;

#define JS_ATOM_HASH_MASK  ((1 << 30) - 1)
#define JS_ATOM_HASH_PRIVATE JS_ATOM_HASH_MASK

struct JSString {
    uint32_t len : 31;
    uint8_t is_wide_char : 1; /* 0 = 8 bits, 1 = 16 bits characters */
    /* for JS_ATOM_TYPE_SYMBOL: hash = weakref_count, atom_type = 3,
       for JS_ATOM_TYPE_PRIVATE: hash = JS_ATOM_HASH_PRIVATE, atom_type = 3
       XXX: could change encoding to have one more bit in hash */
    uint32_t hash : 30;
    uint8_t atom_type : 2; /* != 0 if atom, JS_ATOM_TYPE_x */
    uint32_t hash_next; /* atom_index for JS_ATOM_TYPE_SYMBOL */
#ifdef DUMP_LEAKS
    struct list_head link; /* string list */
#endif
    union {
        uint8_t str8[1]; /* 8 bit strings will get an extra null terminator */
        uint16_t str16[1];
    } u;
};

typedef struct JSStringRope {
    uint32_t len;
    uint8_t is_wide_char; /* 0 = 8 bits, 1 = 16 bits characters */
    uint8_t depth; /* max depth of the rope tree */
    /* XXX: could reduce memory usage by using a direct pointer with
       bit 0 to select rope or string */
    JSValue left;
    JSValue right; /* might be the empty string */
} JSStringRope;

typedef enum {
    JS_CLOSURE_LOCAL, /* 'var_idx' is the index of a local variable in the parent function */
    JS_CLOSURE_ARG, /* 'var_idx' is the index of a argument variable in the parent function */
    JS_CLOSURE_REF, /* 'var_idx' is the index of a closure variable in the parent function */
    JS_CLOSURE_GLOBAL_REF, /* 'var_idx' in the index of a closure
                              variable in the parent function
                              referencing a global variable */
    JS_CLOSURE_GLOBAL_DECL, /* global variable declaration (eval code only) */
    JS_CLOSURE_GLOBAL, /* global variable (eval code only) */
    JS_CLOSURE_MODULE_DECL, /* definition of a module variable (eval code only) */
    JS_CLOSURE_MODULE_IMPORT, /* definition of a module import (eval code only) */
} JSClosureTypeEnum;

#pragma pack(4)
typedef struct JSClosureVar {
    uint32_t closure_type : 3;
    uint8_t is_lexical : 1; /* lexical variable */
    uint8_t is_const : 1; /* const variable (is_lexical = 1 if is_const = 1 */
    uint8_t var_kind : 4; /* see JSVarKindEnum */
    uint16_t var_idx; /* is_local = TRUE: index to a normal variable of the
                    parent function. otherwise: index to a closure
                    variable of the parent function */
    JSAtom var_name;
} JSClosureVar;
#pragma pack()

#define ARG_SCOPE_INDEX 1
#define ARG_SCOPE_END (-2)

typedef enum {
    /* XXX: add more variable kinds here instead of using bit fields */
    JS_VAR_NORMAL,
    JS_VAR_FUNCTION_DECL, /* lexical var with function declaration */
    JS_VAR_NEW_FUNCTION_DECL, /* lexical var with async/generator
                                 function declaration */
    JS_VAR_CATCH,
    JS_VAR_FUNCTION_NAME, /* function expression name */
    JS_VAR_PRIVATE_FIELD,
    JS_VAR_PRIVATE_METHOD,
    JS_VAR_PRIVATE_GETTER,
    JS_VAR_PRIVATE_SETTER, /* must come after JS_VAR_PRIVATE_GETTER */
    JS_VAR_PRIVATE_GETTER_SETTER, /* must come after JS_VAR_PRIVATE_SETTER */
    JS_VAR_GLOBAL_FUNCTION_DECL, /* global function definition, only in JSVarDef */
} JSVarKindEnum;

typedef struct JSBytecodeVarDef {
    JSAtom var_name;
    /* index into JSFunctionBytecode.vars of the next variable in the same or
       enclosing lexical scope
    */
    int scope_next; /* XXX: store on 16 bits */
    uint8_t is_const : 1;
    uint8_t is_lexical : 1;
    uint8_t is_captured : 1; /* XXX: could remove and use a var_ref_idx value */
    uint8_t has_scope: 1; /* true if JSVarDef.scope_level != 0 */
    uint8_t var_kind : 4; /* see JSVarKindEnum */
    /* If is_captured = TRUE, provides, the index of the corresponding
       JSVarRef on stack. It would be more compact to have a separate
       table with the corresponding inverted table but it requires
       more modifications in the code. */
    uint16_t var_ref_idx;
} JSBytecodeVarDef;

/* for the encoding of the pc2line table */
#define PC2LINE_BASE     (-1)
#define PC2LINE_RANGE    5
#define PC2LINE_OP_FIRST 1
#define PC2LINE_DIFF_PC_MAX ((255 - PC2LINE_OP_FIRST) / PC2LINE_RANGE)

typedef enum JSFunctionKindEnum {
    JS_FUNC_NORMAL = 0,
    JS_FUNC_GENERATOR = (1 << 0),
    JS_FUNC_ASYNC = (1 << 1),
    JS_FUNC_ASYNC_GENERATOR = (JS_FUNC_GENERATOR | JS_FUNC_ASYNC),
} JSFunctionKindEnum;

typedef struct JSFunctionBytecode {
    JSGCObjectHeader header; /* must come first */
    uint8_t js_mode;
    uint8_t has_prototype : 1; /* true if a prototype field is necessary */
    uint8_t has_simple_parameter_list : 1;
    uint8_t is_derived_class_constructor : 1;
    /* true if home_object needs to be initialized */
    uint8_t need_home_object : 1;
    uint8_t func_kind : 2;
    uint8_t new_target_allowed : 1;
    uint8_t super_call_allowed : 1;
    uint8_t super_allowed : 1;
    uint8_t arguments_allowed : 1;
    uint8_t has_debug : 1;
    uint8_t read_only_bytecode : 1;
    uint8_t is_direct_or_indirect_eval : 1; /* used by JS_GetScriptOrModuleName() */
    /* XXX: 10 bits available */
    uint8_t *byte_code_buf; /* (self pointer) */
    int32_t byte_code_len;
    JSAtom func_name;
    JSBytecodeVarDef *vardefs; /* arguments + local variables (arg_count + var_count) (self pointer) */
    JSClosureVar *closure_var; /* list of variables in the closure (self pointer) */
    uint16_t arg_count;
    uint16_t var_count;
    uint16_t defined_arg_count; /* for length function property */
    uint16_t stack_size; /* maximum stack size */
    uint16_t var_ref_count; /* number of local variable references */
    JSContext *realm; /* function realm */
    JSValue *cpool; /* constant pool (self pointer) */
    int32_t cpool_count;
    int32_t closure_var_count;
    struct {
        /* debug info, move to separate structure to save memory? */
        JSAtom filename;
        int32_t source_len;
        int32_t pc2line_len;
        uint8_t *pc2line_buf;
        char *source;
    } debug;
} JSFunctionBytecode;

typedef struct JSBoundFunction {
    JSValue func_obj;
    JSValue this_val;
    int argc;
    JSValue argv[1];
} JSBoundFunction;

typedef enum JSIteratorKindEnum {
    JS_ITERATOR_KIND_KEY,
    JS_ITERATOR_KIND_VALUE,
    JS_ITERATOR_KIND_KEY_AND_VALUE,
} JSIteratorKindEnum;

typedef struct JSForInIterator {
    JSValue obj;
    uint32_t idx;
    uint32_t atom_count;
    uint8_t in_prototype_chain;
    uint8_t is_array;
    JSPropertyEnum *tab_atom; /* is_array = FALSE */
} JSForInIterator;

typedef struct JSRegExp {
    JSString *pattern;
    JSString *bytecode; /* also contains the flags */
} JSRegExp;

typedef struct JSProxyData {
    JSValue target;
    JSValue handler;
    uint8_t is_func;
    uint8_t is_revoked;
} JSProxyData;

typedef struct JSArrayBuffer {
    int byte_length; /* 0 if detached */
    int max_byte_length; /* -1 if not resizable; >= byte_length otherwise */
    uint8_t detached;
    uint8_t shared; /* if shared, the array buffer cannot be detached */
    uint8_t *data; /* NULL if detached */
    struct list_head array_list;
    void *opaque;
    JSFreeArrayBufferDataFunc *free_func;
} JSArrayBuffer;

typedef struct JSTypedArray {
    struct list_head link; /* link to arraybuffer */
    JSObject *obj; /* back pointer to the TypedArray/DataView object */
    JSObject *buffer; /* based array buffer */
    uint32_t offset; /* byte offset in the array buffer */
    uint32_t length; /* byte length in the array buffer */
    BOOL track_rab; /* auto-track length of backing array buffer */
} JSTypedArray;

typedef struct JSGlobalObject {
    JSValue uninitialized_vars; /* hidden object containing the list of uninitialized variables */
} JSGlobalObject;

typedef struct JSAsyncFunctionState {
    JSGCObjectHeader header;
    JSValue this_val; /* 'this' argument */
    int argc; /* number of function arguments */
    BOOL throw_flag; /* used to throw an exception in JS_CallInternal() */
    BOOL is_completed; /* TRUE if the function has returned. The stack
                          frame is no longer valid */
    JSValue resolving_funcs[2]; /* only used in JS async functions */
    JSStackFrame frame;
    /* arg_buf, var_buf, stack_buf and var_refs follow */
} JSAsyncFunctionState;

typedef enum {
   /* binary operators */
   JS_OVOP_ADD,
   JS_OVOP_SUB,
   JS_OVOP_MUL,
   JS_OVOP_DIV,
   JS_OVOP_MOD,
   JS_OVOP_POW,
   JS_OVOP_OR,
   JS_OVOP_AND,
   JS_OVOP_XOR,
   JS_OVOP_SHL,
   JS_OVOP_SAR,
   JS_OVOP_SHR,
   JS_OVOP_EQ,
   JS_OVOP_LESS,

   JS_OVOP_BINARY_COUNT,
   /* unary operators */
   JS_OVOP_POS = JS_OVOP_BINARY_COUNT,
   JS_OVOP_NEG,
   JS_OVOP_INC,
   JS_OVOP_DEC,
   JS_OVOP_NOT,

   JS_OVOP_COUNT,
} JSOverloadableOperatorEnum;

typedef struct {
    uint32_t operator_index;
    JSObject *ops[JS_OVOP_BINARY_COUNT]; /* self operators */
} JSBinaryOperatorDefEntry;

typedef struct {
    int count;
    JSBinaryOperatorDefEntry *tab;
} JSBinaryOperatorDef;

typedef struct {
    uint32_t operator_counter;
    BOOL is_primitive; /* OperatorSet for a primitive type */
    /* NULL if no operator is defined */
    JSObject *self_ops[JS_OVOP_COUNT]; /* self operators */
    JSBinaryOperatorDef left;
    JSBinaryOperatorDef right;
} JSOperatorSetData;

typedef struct JSReqModuleEntry {
    JSAtom module_name;
    JSModuleDef *module; /* used using resolution */
    JSValue attributes; /* JS_UNDEFINED or an object contains the attributes as key/value */
} JSReqModuleEntry;

typedef enum JSExportTypeEnum {
    JS_EXPORT_TYPE_LOCAL,
    JS_EXPORT_TYPE_INDIRECT,
} JSExportTypeEnum;

typedef struct JSExportEntry {
    union {
        struct {
            int var_idx; /* closure variable index */
            JSVarRef *var_ref; /* if != NULL, reference to the variable */
        } local; /* for local export */
        int req_module_idx; /* module for indirect export */
    } u;
    JSExportTypeEnum export_type;
    JSAtom local_name; /* '*' if export ns from. not used for local
                          export after compilation */
    JSAtom export_name; /* exported variable name */
} JSExportEntry;

typedef struct JSStarExportEntry {
    int req_module_idx; /* in req_module_entries */
} JSStarExportEntry;

typedef struct JSImportEntry {
    int var_idx; /* closure variable index */
    BOOL is_star; /* import_name = '*' is a valid import name, so need a flag */
    JSAtom import_name;
    int req_module_idx; /* in req_module_entries */
} JSImportEntry;

typedef enum {
    JS_MODULE_STATUS_UNLINKED,
    JS_MODULE_STATUS_LINKING,
    JS_MODULE_STATUS_LINKED,
    JS_MODULE_STATUS_EVALUATING,
    JS_MODULE_STATUS_EVALUATING_ASYNC,
    JS_MODULE_STATUS_EVALUATED,
} JSModuleStatus;

struct JSModuleDef {
    JSGCObjectHeader header; /* must come first */
    JSAtom module_name;
    struct list_head link;

    JSReqModuleEntry *req_module_entries;
    int req_module_entries_count;
    int req_module_entries_size;

    JSExportEntry *export_entries;
    int export_entries_count;
    int export_entries_size;

    JSStarExportEntry *star_export_entries;
    int star_export_entries_count;
    int star_export_entries_size;

    JSImportEntry *import_entries;
    int import_entries_count;
    int import_entries_size;

    JSValue module_ns;
    JSValue func_obj; /* only used for JS modules */
    JSModuleInitFunc *init_func; /* only used for C modules */
    BOOL has_tla : 8; /* true if func_obj contains await */
    BOOL resolved : 8;
    BOOL func_created : 8;
    JSModuleStatus status : 8;
    /* temp use during js_module_link() & js_module_evaluate() */
    int dfs_index, dfs_ancestor_index;
    JSModuleDef *stack_prev;
    /* temp use during js_module_evaluate() */
    JSModuleDef **async_parent_modules;
    int async_parent_modules_count;
    int async_parent_modules_size;
    int pending_async_dependencies;
    BOOL async_evaluation; /* true: async_evaluation_timestamp corresponds to [[AsyncEvaluationOrder]]
                              false: [[AsyncEvaluationOrder]] is UNSET or DONE */
    int64_t async_evaluation_timestamp;
    JSModuleDef *cycle_root;
    JSValue promise; /* corresponds to spec field: capability */
    JSValue resolving_funcs[2]; /* corresponds to spec field: capability */

    /* true if evaluation yielded an exception. It is saved in
       eval_exception */
    BOOL eval_has_exception : 8;
    JSValue eval_exception;
    JSValue meta_obj; /* for import.meta */
    JSValue private_value; /* private value for C modules */
};

typedef struct JSJobEntry {
    struct list_head link;
    JSContext *realm;
    JSJobFunc *job_func;
    int argc;
    JSValue argv[1];
} JSJobEntry;

typedef struct JSProperty {
    union {
        JSValue value;      /* JS_PROP_NORMAL */
        struct {            /* JS_PROP_GETSET */
            JSObject *getter; /* NULL if undefined */
            JSObject *setter; /* NULL if undefined */
        } getset;
        JSVarRef *var_ref;  /* JS_PROP_VARREF */
        struct {            /* JS_PROP_AUTOINIT */
            /* in order to use only 2 pointers, we compress the realm
               and the init function pointer */
            uintptr_t realm_and_id; /* realm and init_id (JS_AUTOINIT_ID_x)
                                       in the 2 low bits */
            void *opaque;
        } init;
    } u;
} JSProperty;

#define JS_PROP_INITIAL_SIZE 2
#define JS_PROP_INITIAL_HASH_SIZE 4 /* must be a power of two */

typedef struct JSShapeProperty {
    uint32_t hash_next : 26; /* 0 if last in list */
    uint32_t flags : 6;   /* JS_PROP_XXX */
    JSAtom atom; /* JS_ATOM_NULL = free property entry */
} JSShapeProperty;

struct JSShape {
    JSGCObjectHeader header;
    /* true if the shape is inserted in the shape hash table. If not,
       JSShape.hash is not valid */
    uint8_t is_hashed;
    uint32_t hash; /* current hash value */
    uint32_t prop_hash_mask; /* >= 2 */
    int prop_size; /* allocated properties */
    int prop_count; /* include deleted properties */
    int deleted_prop_count;
    JSShape *shape_hash_next; /* in JSRuntime.shape_hash[h] list */
    JSObject *proto;
    uint32_t hash_table[]; /* prop_hash_mask + 1 elements */
    /* followed by JSShapeProperty prop[prop_size]; */
};

struct JSObject {
    JSGCObjectHeader header;
    /* TRUE if the array prototype is "normal":
       - no small index properties which are get/set or non writable
       - its prototype is Object.prototype
       - Object.prototype has no small index properties which are get/set or non writable
       - the prototype of Object.prototype is null (always true as it is immutable)
    */
    uint8_t is_std_array_prototype : 1;

    uint8_t extensible : 1;
    uint8_t free_mark : 1; /* only used when freeing objects with cycles */
    uint8_t is_exotic : 1; /* TRUE if object has exotic property handlers */
    uint8_t fast_array : 1; /* TRUE if u.array is used for get/put (for JS_CLASS_ARRAY, JS_CLASS_ARGUMENTS, JS_CLASS_MAPPED_ARGUMENTS and typed arrays) */
    uint8_t is_constructor : 1; /* TRUE if object is a constructor function */
    uint8_t has_immutable_prototype : 1; /* cannot modify the prototype */
    uint8_t tmp_mark : 1; /* used in JS_WriteObjectRec() */
    uint8_t is_HTMLDDA : 1; /* specific annex B IsHtmlDDA behavior */
    uint16_t class_id; /* see JS_CLASS_x */
    /* count the number of weak references to this object. The object
       structure is freed only if header.ref_count = 0 and
       weakref_count = 0 */
    uint32_t weakref_count;
    JSShape *shape; /* prototype and property names + flag */
    JSProperty *prop; /* array of properties */
    union {
        void *opaque;
        struct JSBoundFunction *bound_function; /* JS_CLASS_BOUND_FUNCTION */
        struct JSCFunctionDataRecord *c_function_data_record; /* JS_CLASS_C_FUNCTION_DATA */
        struct JSForInIterator *for_in_iterator; /* JS_CLASS_FOR_IN_ITERATOR */
        struct JSArrayBuffer *array_buffer; /* JS_CLASS_ARRAY_BUFFER, JS_CLASS_SHARED_ARRAY_BUFFER */
        struct JSTypedArray *typed_array; /* JS_CLASS_UINT8C_ARRAY..JS_CLASS_DATAVIEW */
        struct JSMapState *map_state;   /* JS_CLASS_MAP..JS_CLASS_WEAKSET */
        struct JSMapIteratorData *map_iterator_data; /* JS_CLASS_MAP_ITERATOR, JS_CLASS_SET_ITERATOR */
        struct JSArrayIteratorData *array_iterator_data; /* JS_CLASS_ARRAY_ITERATOR, JS_CLASS_STRING_ITERATOR */
        struct JSRegExpStringIteratorData *regexp_string_iterator_data; /* JS_CLASS_REGEXP_STRING_ITERATOR */
        struct JSGeneratorData *generator_data; /* JS_CLASS_GENERATOR */
        struct JSIteratorConcatData *iterator_concat_data; /* JS_CLASS_ITERATOR_CONCAT */
        struct JSIteratorHelperData *iterator_helper_data; /* JS_CLASS_ITERATOR_HELPER */
        struct JSIteratorWrapData *iterator_wrap_data; /* JS_CLASS_ITERATOR_WRAP */
        struct JSProxyData *proxy_data; /* JS_CLASS_PROXY */
        struct JSPromiseData *promise_data; /* JS_CLASS_PROMISE */
        struct JSPromiseFunctionData *promise_function_data; /* JS_CLASS_PROMISE_RESOLVE_FUNCTION, JS_CLASS_PROMISE_REJECT_FUNCTION */
        struct JSAsyncFunctionState *async_function_data; /* JS_CLASS_ASYNC_FUNCTION_RESOLVE, JS_CLASS_ASYNC_FUNCTION_REJECT */
        struct JSAsyncFromSyncIteratorData *async_from_sync_iterator_data; /* JS_CLASS_ASYNC_FROM_SYNC_ITERATOR */
        struct JSAsyncGeneratorData *async_generator_data; /* JS_CLASS_ASYNC_GENERATOR */
        struct { /* JS_CLASS_BYTECODE_FUNCTION: 12/24 bytes */
            /* also used by JS_CLASS_GENERATOR_FUNCTION, JS_CLASS_ASYNC_FUNCTION and JS_CLASS_ASYNC_GENERATOR_FUNCTION */
            struct JSFunctionBytecode *function_bytecode;
            JSVarRef **var_refs;
            JSObject *home_object; /* for 'super' access */
        } func;
        struct { /* JS_CLASS_C_FUNCTION: 12/20 bytes */
            JSContext *realm;
            JSCFunctionType c_function;
            uint8_t length;
            uint8_t cproto;
            int16_t magic;
        } cfunc;
        /* array part for fast arrays and typed arrays */
        struct { /* JS_CLASS_ARRAY, JS_CLASS_ARGUMENTS, JS_CLASS_MAPPED_ARGUMENTS, JS_CLASS_UINT8C_ARRAY..JS_CLASS_FLOAT64_ARRAY */
            union {
                uint32_t size;          /* JS_CLASS_ARRAY */
                struct JSTypedArray *typed_array; /* JS_CLASS_UINT8C_ARRAY..JS_CLASS_FLOAT64_ARRAY */
            } u1;
            union {
                JSValue *values;        /* JS_CLASS_ARRAY, JS_CLASS_ARGUMENTS */
                JSVarRef **var_refs;     /* JS_CLASS_MAPPED_ARGUMENTS */
                void *ptr;              /* JS_CLASS_UINT8C_ARRAY..JS_CLASS_FLOAT64_ARRAY */
                int8_t *int8_ptr;       /* JS_CLASS_INT8_ARRAY */
                uint8_t *uint8_ptr;     /* JS_CLASS_UINT8_ARRAY, JS_CLASS_UINT8C_ARRAY */
                int16_t *int16_ptr;     /* JS_CLASS_INT16_ARRAY */
                uint16_t *uint16_ptr;   /* JS_CLASS_UINT16_ARRAY */
                int32_t *int32_ptr;     /* JS_CLASS_INT32_ARRAY */
                uint32_t *uint32_ptr;   /* JS_CLASS_UINT32_ARRAY */
                int64_t *int64_ptr;     /* JS_CLASS_INT64_ARRAY */
                uint64_t *uint64_ptr;   /* JS_CLASS_UINT64_ARRAY */
                uint16_t *fp16_ptr;     /* JS_CLASS_FLOAT16_ARRAY */
                float *float_ptr;       /* JS_CLASS_FLOAT32_ARRAY */
                double *double_ptr;     /* JS_CLASS_FLOAT64_ARRAY */
            } u;
            uint32_t count; /* <= 2^31-1. 0 for a detached typed array */
        } array;    /* 12/20 bytes */
        JSRegExp regexp;    /* JS_CLASS_REGEXP: 8/16 bytes */
        JSValue object_data;    /* for JS_SetObjectData(): 8/16/16 bytes */
        JSGlobalObject global_object;
    } u;
};

typedef struct JSMapRecord {
    int ref_count; /* used during enumeration to avoid freeing the record */
    BOOL empty : 8; /* TRUE if the record is deleted */
    struct list_head link;
    struct JSMapRecord *hash_next;
    JSValue key;
    JSValue value;
} JSMapRecord;

typedef struct JSMapState {
    BOOL is_weak; /* TRUE if WeakSet/WeakMap */
    struct list_head records; /* list of JSMapRecord.link */
    uint32_t record_count;
    JSMapRecord **hash_table;
    int hash_bits;
    uint32_t hash_size; /* = 2 ^ hash_bits */
    uint32_t record_count_threshold; /* count at which a hash table
                                        resize is needed */
    JSWeakRefHeader weakref_header; /* only used if is_weak = TRUE */
} JSMapState;

enum {
    __JS_ATOM_NULL = JS_ATOM_NULL,
#define DEF(name, str) JS_ATOM_ ## name,
#include "quickjs-atom.h"
#undef DEF
    JS_ATOM_END,
};
#define JS_ATOM_LAST_KEYWORD JS_ATOM_super
#define JS_ATOM_LAST_STRICT_KEYWORD JS_ATOM_yield

extern const char js_atom_init[];

typedef enum OPCodeFormat {
#define FMT(f) OP_FMT_ ## f,
#define DEF(id, size, n_pop, n_push, f)
#include "quickjs-opcode.h"
#undef DEF
#undef FMT
} OPCodeFormat;

#define HINT_STRING  0
#define HINT_NUMBER  1
#define HINT_NONE    2
#define HINT_FORCE_ORDINARY (1 << 4) // don't try Symbol.toPrimitive

#define ATOM_GET_STR_BUF_SIZE 64

#define MAX_SAFE_INTEGER (((int64_t)1 << 53) - 1)

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

#define JS_CALL_FLAG_COPY_ARGV   (1 << 1)
#define JS_CALL_FLAG_GENERATOR   (1 << 2)

#define JS_NEW_CTOR_NO_GLOBAL   (1 << 0) /* don't create a global binding */
#define JS_NEW_CTOR_PROTO_CLASS (1 << 1) /* the prototype class is 'class_id' instead of JS_CLASS_OBJECT */
#define JS_NEW_CTOR_PROTO_EXIST (1 << 2) /* the prototype is already defined */
#define JS_NEW_CTOR_READONLY    (1 << 3) /* read-only constructor field */

typedef struct JSClassShortDef {
    JSAtom class_name;
    JSClassFinalizer *finalizer;
    JSClassGCMark *gc_mark;
} JSClassShortDef;

typedef struct StringBuffer {
    JSContext *ctx;
    JSString *str;
    int len;
    int size;
    int is_wide_char;
    int error_status;
} StringBuffer;

typedef struct {
    JSValueConst stack[JS_STRING_ROPE_MAX_DEPTH];
    int stack_len;
} JSStringRopeIter;

typedef struct JSCFunctionDataRecord {
    JSCFunctionData *func;
    uint8_t length;
    uint8_t data_len;
    uint16_t magic;
    JSValue data[1];
} JSCFunctionDataRecord;

typedef struct JSMemoryUsage_helper {
    double memory_used_count;
    double str_count;
    double str_size;
    int64_t js_func_count;
    double js_func_size;
    int64_t js_func_code_size;
    int64_t js_func_pc2line_count;
    int64_t js_func_pc2line_size;
} JSMemoryUsage_helper;

#define JS_PRINT_MAX_DEPTH 8

typedef struct {
    JSRuntime *rt;
    JSContext *ctx; /* may be NULL */
    JSPrintValueOptions options;
    JSPrintValueWrite *write_func;
    void *write_opaque;
    int level;
    JSObject *print_stack[JS_PRINT_MAX_DEPTH]; /* level values */
} JSPrintValueState;

typedef enum JSGeneratorStateEnum {
    JS_GENERATOR_STATE_SUSPENDED_START,
    JS_GENERATOR_STATE_SUSPENDED_YIELD,
    JS_GENERATOR_STATE_SUSPENDED_YIELD_STAR,
    JS_GENERATOR_STATE_EXECUTING,
    JS_GENERATOR_STATE_COMPLETED,
} JSGeneratorStateEnum;

typedef struct JSGeneratorData {
    JSGeneratorStateEnum state;
    JSAsyncFunctionState *func_state;
} JSGeneratorData;

typedef enum JSAsyncGeneratorStateEnum {
    JS_ASYNC_GENERATOR_STATE_SUSPENDED_START,
    JS_ASYNC_GENERATOR_STATE_SUSPENDED_YIELD,
    JS_ASYNC_GENERATOR_STATE_SUSPENDED_YIELD_STAR,
    JS_ASYNC_GENERATOR_STATE_EXECUTING,
    JS_ASYNC_GENERATOR_STATE_AWAITING_RETURN,
    JS_ASYNC_GENERATOR_STATE_COMPLETED,
} JSAsyncGeneratorStateEnum;

typedef struct JSAsyncGeneratorRequest {
    struct list_head link;
    /* completion */
    int completion_type; /* GEN_MAGIC_x */
    JSValue result;
    /* promise capability */
    JSValue promise;
    JSValue resolving_funcs[2];
} JSAsyncGeneratorRequest;

typedef struct JSAsyncGeneratorData {
    JSObject *generator; /* back pointer to the object (const) */
    JSAsyncGeneratorStateEnum state;
    /* func_state is NULL is state AWAITING_RETURN and COMPLETED */
    JSAsyncFunctionState *func_state;
    struct list_head queue; /* list of JSAsyncGeneratorRequest.link */
} JSAsyncGeneratorData;

typedef struct BlockEnv {
    struct BlockEnv *prev;
    JSAtom label_name; /* JS_ATOM_NULL if none */
    int label_break; /* -1 if none */
    int label_cont; /* -1 if none */
    int drop_count; /* number of stack elements to drop */
    int label_finally; /* -1 if none */
    int scope_level;
    uint8_t has_iterator : 1;
    uint8_t is_regular_stmt : 1; /* i.e. not a loop statement */
} BlockEnv;

typedef struct JSGlobalVar {
    int cpool_idx; /* if >= 0, index in the constant pool for hoisted
                      function defintion*/
    uint8_t force_init : 1; /* force initialization to undefined */
    uint8_t is_lexical : 1; /* global let/const definition */
    uint8_t is_const   : 1; /* const definition */
    int scope_level;    /* scope of definition */
    JSAtom var_name;  /* variable name */
} JSGlobalVar;

typedef struct RelocEntry {
    struct RelocEntry *next;
    uint32_t addr; /* address to patch */
    int size;   /* address size: 1, 2 or 4 bytes */
} RelocEntry;

typedef struct JumpSlot {
    int op;
    int size;
    int pos;
    int label;
} JumpSlot;

typedef struct LabelSlot {
    int ref_count;
    int pos;    /* phase 1 address, -1 means not resolved yet */
    int pos2;   /* phase 2 address, -1 means not resolved yet */
    int addr;   /* phase 3 address, -1 means not resolved yet */
    RelocEntry *first_reloc;
} LabelSlot;

typedef struct LineNumberSlot {
    uint32_t pc;
    uint32_t source_pos;
} LineNumberSlot;

typedef struct {
    /* last source position */
    const uint8_t *ptr;
    int line_num;
    int col_num;
    const uint8_t *buf_start;
} GetLineColCache;

typedef enum JSParseFunctionEnum {
    JS_PARSE_FUNC_STATEMENT,
    JS_PARSE_FUNC_VAR,
    JS_PARSE_FUNC_EXPR,
    JS_PARSE_FUNC_ARROW,
    JS_PARSE_FUNC_GETTER,
    JS_PARSE_FUNC_SETTER,
    JS_PARSE_FUNC_METHOD,
    JS_PARSE_FUNC_CLASS_STATIC_INIT,
    JS_PARSE_FUNC_CLASS_CONSTRUCTOR,
    JS_PARSE_FUNC_DERIVED_CLASS_CONSTRUCTOR,
} JSParseFunctionEnum;

typedef enum JSParseExportEnum {
    JS_PARSE_EXPORT_NONE,
    JS_PARSE_EXPORT_NAMED,
    JS_PARSE_EXPORT_DEFAULT,
} JSParseExportEnum;

typedef struct JSVarScope {
    int parent;  /* index into fd->scopes of the enclosing scope */
    int first;   /* index into fd->vars of the last variable in this scope */
} JSVarScope;

typedef struct JSVarDef {
    JSAtom var_name;
    /* index into fd->scopes of this variable lexical scope */
    int scope_level;
    /* - if scope_level = 0: scope in which the variable is defined
       - if scope_level != 0: index into fd->vars of the next
       variable in the same or enclosing lexical scope
    */
    int scope_next;
    uint8_t is_const : 1;
    uint8_t is_lexical : 1;
    uint8_t is_captured : 1; /* XXX: could remove and use a var_ref_idx value */
    uint8_t is_static_private : 1; /* only used during private class field parsing */
    uint8_t var_kind : 4; /* see JSVarKindEnum */
    /* if is_captured = TRUE, provides, the index of the corresponding
       JSVarRef on stack */
    uint16_t var_ref_idx;
    /* function pool index for lexical variables with var_kind =
       JS_VAR_FUNCTION_DECL/JS_VAR_NEW_FUNCTION_DECL or scope level of
       the definition of the 'var' variables (they have scope_level =
       0) */
    int func_pool_idx;
} JSVarDef;

typedef struct JSFunctionDef {
    JSContext *ctx;
    struct JSFunctionDef *parent;
    int parent_cpool_idx; /* index in the constant pool of the parent
                             or -1 if none */
    int parent_scope_level; /* scope level in parent at point of definition */
    struct list_head child_list; /* list of JSFunctionDef.link */
    struct list_head link;

    BOOL is_eval; /* TRUE if eval code */
    int eval_type; /* only valid if is_eval = TRUE */
    BOOL is_global_var; /* TRUE if variables are not defined locally:
                           eval global, eval module or non strict eval */
    BOOL is_func_expr; /* TRUE if function expression */
    BOOL has_home_object; /* TRUE if the home object is available */
    BOOL has_prototype; /* true if a prototype field is necessary */
    BOOL has_simple_parameter_list;
    BOOL has_parameter_expressions; /* if true, an argument scope is created */
    BOOL has_use_strict; /* to reject directive in special cases */
    BOOL has_eval_call; /* true if the function contains a call to eval() */
    BOOL has_arguments_binding; /* true if the 'arguments' binding is
                                   available in the function */
    BOOL has_this_binding; /* true if the 'this' and new.target binding are
                              available in the function */
    BOOL new_target_allowed; /* true if the 'new.target' does not
                                throw a syntax error */
    BOOL super_call_allowed; /* true if super() is allowed */
    BOOL super_allowed; /* true if super. or super[] is allowed */
    BOOL arguments_allowed; /* true if the 'arguments' identifier is allowed */
    BOOL is_derived_class_constructor;
    BOOL in_function_body;
    JSFunctionKindEnum func_kind : 8;
    JSParseFunctionEnum func_type : 8;
    uint8_t js_mode; /* bitmap of JS_MODE_x */
    JSAtom func_name; /* JS_ATOM_NULL if no name */

    JSVarDef *vars;
    int var_size; /* allocated size for vars[] */
    int var_count;
    JSVarDef *args;
    int arg_size; /* allocated size for args[] */
    int arg_count; /* number of arguments */
    int defined_arg_count;
    int var_ref_count; /* number of local/arg variable references */
    int var_object_idx; /* -1 if none */
    int arg_var_object_idx; /* -1 if none (var object for the argument scope) */
    int arguments_var_idx; /* -1 if none */
    int arguments_arg_idx; /* argument variable definition in argument scope,
                              -1 if none */
    int func_var_idx; /* variable containing the current function (-1
                         if none, only used if is_func_expr is true) */
    int eval_ret_idx; /* variable containing the return value of the eval, -1 if none */
    int this_var_idx; /* variable containg the 'this' value, -1 if none */
    int new_target_var_idx; /* variable containg the 'new.target' value, -1 if none */
    int this_active_func_var_idx; /* variable containg the 'this.active_func' value, -1 if none */
    int home_object_var_idx;
    BOOL need_home_object;

    int scope_level;    /* index into fd->scopes if the current lexical scope */
    int scope_first;    /* index into vd->vars of first lexically scoped variable */
    int scope_size;     /* allocated size of fd->scopes array */
    int scope_count;    /* number of entries used in the fd->scopes array */
    JSVarScope *scopes;
    JSVarScope def_scope_array[4];
    int body_scope; /* scope of the body of the function or eval */

    int global_var_count;
    int global_var_size;
    JSGlobalVar *global_vars;

    DynBuf byte_code;
    int last_opcode_pos; /* -1 if no last opcode */
    const uint8_t *last_opcode_source_ptr;
    BOOL use_short_opcodes; /* true if short opcodes are used in byte_code */

    LabelSlot *label_slots;
    int label_size; /* allocated size for label_slots[] */
    int label_count;
    BlockEnv *top_break; /* break/continue label stack */

    /* constant pool (strings, functions, numbers) */
    JSValue *cpool;
    int cpool_count;
    int cpool_size;

    /* list of variables in the closure */
    int closure_var_count;
    int closure_var_size;
    JSClosureVar *closure_var;

    JumpSlot *jump_slots;
    int jump_size;
    int jump_count;

    LineNumberSlot *line_number_slots;
    int line_number_size;
    int line_number_count;
    int line_number_last;
    int line_number_last_pc;

    /* pc2line table */
    BOOL strip_debug : 1; /* strip all debug info (implies strip_source = TRUE) */
    BOOL strip_source : 1; /* strip only source code */
    JSAtom filename;
    uint32_t source_pos; /* pointer in the eval() source */
    GetLineColCache *get_line_col_cache; /* XXX: could remove to save memory */
    DynBuf pc2line;

    char *source;  /* raw source, utf-8 encoded */
    int source_len;

    JSModuleDef *module; /* != NULL when parsing a module */
    BOOL has_await; /* TRUE if await is used (used in module eval) */
} JSFunctionDef;

typedef struct JSToken {
    int val;
    const uint8_t *ptr; /* position in the source */
    union {
        struct {
            JSValue str;
            int sep;
        } str;
        struct {
            JSValue val;
        } num;
        struct {
            JSAtom atom;
            BOOL has_escape;
            BOOL is_reserved;
        } ident;
        struct {
            JSValue body;
            JSValue flags;
        } regexp;
    } u;
} JSToken;

typedef struct JSParseState {
    JSContext *ctx;
    const char *filename;
    JSToken token;
    BOOL got_lf; /* true if got line feed before the current token */
    const uint8_t *last_ptr;
    const uint8_t *buf_start;
    const uint8_t *buf_ptr;
    const uint8_t *buf_end;

    /* current function code */
    JSFunctionDef *cur_func;
    BOOL is_module; /* parsing a module */
    BOOL allow_html_comments;
    BOOL ext_json; /* JSON parsing: true if accepting JSON superset */
    GetLineColCache get_line_col_cache;
} JSParseState;

typedef struct JSOpCode {
#ifdef DUMP_BYTECODE
    const char *name;
#endif
    uint8_t size; /* in bytes */
    /* the opcodes remove n_pop items from the top of the stack, then
       pushes n_push items */
    uint8_t n_pop;
    uint8_t n_push;
    uint8_t fmt;
} JSOpCode;

typedef struct JSParsePos {
    BOOL got_lf;
    const uint8_t *ptr;
} JSParsePos;

typedef struct {
    JSFunctionDef *fields_init_fd;
    int computed_fields_count;
    BOOL need_brand;
    int brand_push_pos;
    BOOL is_static;
} ClassFieldsDef;

typedef struct JSResolveEntry {
    JSModuleDef *module;
    JSAtom name;
} JSResolveEntry;

typedef struct JSResolveState {
    JSResolveEntry *array;
    int size;
    int count;
} JSResolveState;

typedef enum {
    EXPORTED_NAME_AMBIGUOUS,
    EXPORTED_NAME_NORMAL,
    EXPORTED_NAME_DELAYED,
} ExportedNameEntryEnum;

typedef struct ExportedNameEntry {
    JSAtom export_name;
    ExportedNameEntryEnum export_type;
    union {
        JSExportEntry *me; /* using when the list is built */
        JSVarRef *var_ref; /* EXPORTED_NAME_NORMAL */
    } u;
} ExportedNameEntry;

typedef struct GetExportNamesState {
    JSModuleDef **modules;
    int modules_size;
    int modules_count;

    ExportedNameEntry *exported_names;
    int exported_names_size;
    int exported_names_count;
} GetExportNamesState;

typedef struct {
    JSModuleDef **tab;
    int count;
    int size;
} ExecModuleList;

typedef struct CodeContext {
    const uint8_t *bc_buf; /* code buffer */
    int bc_len;   /* length of the code buffer */
    int pos;      /* position past the matched code pattern */
    int line_num; /* last visited OP_line_num parameter or -1 */
    int op;
    int idx;
    int label;
    int val;
    JSAtom atom;
} CodeContext;

/* compute the maximum stack size needed by the function */

typedef struct StackSizeState {
    int bc_len;
    int stack_len_max;
    uint16_t *stack_level_tab;
    int32_t *catch_pos_tab;
    int *pc_stack;
    int pc_stack_len;
    int pc_stack_size;
} StackSizeState;

typedef struct {
    JSObject *obj;
    uint32_t hash_next; /* -1 if no next entry */
} JSObjectListEntry;

/* XXX: reuse it to optimize weak references */
typedef struct {
    JSObjectListEntry *object_tab;
    int object_count;
    int object_size;
    uint32_t *hash_table;
    uint32_t hash_size;
} JSObjectList;

typedef enum BCTagEnum {
    BC_TAG_NULL = 1,
    BC_TAG_UNDEFINED,
    BC_TAG_BOOL_FALSE,
    BC_TAG_BOOL_TRUE,
    BC_TAG_INT32,
    BC_TAG_FLOAT64,
    BC_TAG_STRING,
    BC_TAG_OBJECT,
    BC_TAG_ARRAY,
    BC_TAG_BIG_INT,
    BC_TAG_TEMPLATE_OBJECT,
    BC_TAG_FUNCTION_BYTECODE,
    BC_TAG_MODULE,
    BC_TAG_TYPED_ARRAY,
    BC_TAG_ARRAY_BUFFER,
    BC_TAG_SHARED_ARRAY_BUFFER,
    BC_TAG_DATE,
    BC_TAG_OBJECT_VALUE,
    BC_TAG_OBJECT_REFERENCE,
} BCTagEnum;

#define BC_VERSION 5

typedef struct BCWriterState {
    JSContext *ctx;
    DynBuf dbuf;
    BOOL allow_bytecode : 8;
    BOOL allow_sab : 8;
    BOOL allow_reference : 8;
    uint32_t first_atom;
    uint32_t *atom_to_idx;
    int atom_to_idx_size;
    JSAtom *idx_to_atom;
    int idx_to_atom_count;
    int idx_to_atom_size;
    uint8_t **sab_tab;
    int sab_tab_len;
    int sab_tab_size;
    /* list of referenced objects (used if allow_reference = TRUE) */
    JSObjectList object_list;
} BCWriterState;

typedef struct BCReaderState {
    JSContext *ctx;
    const uint8_t *buf_start, *ptr, *buf_end;
    uint32_t first_atom;
    uint32_t idx_to_atom_count;
    JSAtom *idx_to_atom;
    int error_state;
    BOOL allow_sab : 8;
    BOOL allow_bytecode : 8;
    BOOL is_rom_data : 8;
    BOOL allow_reference : 8;
    /* object references */
    JSObject **objects;
    int objects_count;
    int objects_size;

#ifdef DUMP_READ_OBJECT
    const uint8_t *ptr_last;
    int level;
#endif
} BCReaderState;

typedef struct ValueSlot {
    JSValue val;
    JSString *str;
    int64_t pos;
} ValueSlot;

struct array_sort_context {
    JSContext *ctx;
    int exception;
    int has_method;
    JSValueConst method;
};

typedef struct JSArrayIteratorData {
    JSValue obj;
    JSIteratorKindEnum kind;
    uint32_t idx;
} JSArrayIteratorData;

typedef struct JSIteratorWrapData {
    JSValue wrapped_iter;
    JSValue wrapped_next;
} JSIteratorWrapData;

// note: deliberately doesn't use space-saving bit fields for
// |index|, |count| and |running| because tcc miscompiles them
typedef struct JSIteratorConcatData {
    int index, count;             // elements (not pairs!) in values[] array
    BOOL running;
    JSValue iter, next, values[]; // array of (object, method) pairs
} JSIteratorConcatData;

typedef enum JSIteratorHelperKindEnum {
    JS_ITERATOR_HELPER_KIND_DROP,
    JS_ITERATOR_HELPER_KIND_EVERY,
    JS_ITERATOR_HELPER_KIND_FILTER,
    JS_ITERATOR_HELPER_KIND_FIND,
    JS_ITERATOR_HELPER_KIND_FLAT_MAP,
    JS_ITERATOR_HELPER_KIND_FOR_EACH,
    JS_ITERATOR_HELPER_KIND_MAP,
    JS_ITERATOR_HELPER_KIND_SOME,
    JS_ITERATOR_HELPER_KIND_TAKE,
} JSIteratorHelperKindEnum;

typedef struct JSIteratorHelperData {
    JSValue obj;
    JSValue next;
    JSValue func; // predicate (filter) or mapper (flatMap, map)
    JSValue inner; // innerValue (flatMap)
    int64_t count; // limit (drop, take) or counter (filter, map, flatMap)
    JSIteratorHelperKindEnum kind : 8;
    uint8_t executing : 1;
    uint8_t done : 1;
} JSIteratorHelperData;

typedef enum {
    SUM_PRECISE_STATE_FINITE,
    SUM_PRECISE_STATE_INFINITY,
    SUM_PRECISE_STATE_MINUS_INFINITY, /* must be after SUM_PRECISE_STATE_INFINITY */
    SUM_PRECISE_STATE_NAN, /* must be after SUM_PRECISE_STATE_MINUS_INFINITY */
} SumPreciseStateEnum;

#define SP_LIMB_BITS 56
#define SP_RND_BITS (SP_LIMB_BITS - 53)
/* we add one extra limb to avoid having to test for overflows during the sum */
#define SUM_PRECISE_ACC_LEN 39

#define SUM_PRECISE_COUNTER_INIT 250

#define DEFINE_GLOBAL_LEX_VAR (1 << 7)
#define DEFINE_GLOBAL_FUNC_VAR (1 << 6)

#define GLOBAL_VAR_OFFSET 0x40000000
#define ARGUMENT_VAR_OFFSET 0x20000000

/* allow the 'in' binary operator */
#define PF_IN_ACCEPTED  (1 << 0)
/* allow function calls parsing in js_parse_postfix_expr() */
#define PF_POSTFIX_CALL (1 << 1)
/* allow the exponentiation operator in js_parse_unary() */
#define PF_POW_ALLOWED  (1 << 2)
/* forbid the exponentiation operator in js_parse_unary() */
#define PF_POW_FORBIDDEN (1 << 3)

#define JS_DEFINE_CLASS_HAS_HERITAGE     (1 << 0)

#define SKIP_HAS_SEMI       (1 << 0)
#define SKIP_HAS_ELLIPSIS   (1 << 1)
#define SKIP_HAS_ASSIGNMENT (1 << 2)

#define JS_THROW_VAR_RO             0
#define JS_THROW_VAR_REDECL         1
#define JS_THROW_VAR_UNINITIALIZED  2
#define JS_THROW_ERROR_DELETE_SUPER   3
#define JS_THROW_ERROR_ITERATOR_THROW 4

#define PROP_TYPE_IDENT 0
#define PROP_TYPE_VAR   1
#define PROP_TYPE_GET   2
#define PROP_TYPE_SET   3
#define PROP_TYPE_STAR  4
#define PROP_TYPE_ASYNC 5
#define PROP_TYPE_ASYNC_STAR 6

#define PROP_TYPE_PRIVATE (1 << 4)

#define DECL_MASK_FUNC  (1 << 0) /* allow normal function declaration */
/* ored with DECL_MASK_FUNC if function declarations are allowed with a label */
#define DECL_MASK_FUNC_WITH_LABEL (1 << 1)
#define DECL_MASK_OTHER (1 << 2) /* all other declarations */
#define DECL_MASK_ALL   (DECL_MASK_FUNC | DECL_MASK_FUNC_WITH_LABEL | DECL_MASK_OTHER)

typedef struct {
    SumPreciseStateEnum state;
    uint32_t counter;
    int n_limbs; /* 'acc' contains n_limbs and is not necessarily
                    acc[n_limb - 1] may be 0. 0 indicates minus zero
                    result when state = SUM_PRECISE_STATE_FINITE */
    int64_t acc[SUM_PRECISE_ACC_LEN];
} SumPreciseState;

typedef struct JSRegExpStringIteratorData {
    JSValue iterating_regexp;
    JSValue iterated_string;
    BOOL global;
    BOOL unicode;
    BOOL done;
} JSRegExpStringIteratorData;

typedef struct ValueBuffer {
    JSContext *ctx;
    JSValue *arr;
    JSValue def[4];
    int len;
    int size;
    int error_status;
} ValueBuffer;

typedef struct {
    int count;
    uint32_t hash_size;
    struct JSONParseRecordEntry *entries;
    uint32_t *hash_table;
} JSONParseRecordObject;

typedef struct JSONParseRecord {
    JSValue value;
    union {
        JSONParseRecordObject obj;
        struct {
            int count;
            struct JSONParseRecord *elements;
        } array;
        struct {
            uint32_t source_pos;
            uint32_t source_len;
        } primitive;
    } u;
} JSONParseRecord;

typedef struct JSONParseRecordEntry {
    JSAtom atom;
    uint32_t hash_next;
    JSONParseRecord parse_record;
} JSONParseRecordEntry;

typedef struct JSONStringifyContext {
    JSValueConst replacer_func;
    JSValue stack;
    JSValue property_list;
    JSValue gap;
    JSValue empty;
    StringBuffer *b;
} JSONStringifyContext;

typedef struct JSMapIteratorData {
    JSValue obj;
    JSIteratorKindEnum kind;
    JSMapRecord *cur_record;
} JSMapIteratorData;

typedef struct JSPromiseData {
    JSPromiseStateEnum promise_state;
    /* 0=fulfill, 1=reject, list of JSPromiseReactionData.link */
    struct list_head promise_reactions[2];
    BOOL is_handled; /* Note: only useful to debug */
    JSValue promise_result;
} JSPromiseData;

typedef struct JSPromiseFunctionDataResolved {
    int ref_count;
    BOOL already_resolved;
} JSPromiseFunctionDataResolved;

typedef struct JSPromiseFunctionData {
    JSValue promise;
    JSPromiseFunctionDataResolved *presolved;
} JSPromiseFunctionData;

typedef struct JSPromiseReactionData {
    struct list_head link; /* not used in promise_reaction_job */
    JSValue resolving_funcs[2];
    JSValue handler;
} JSPromiseReactionData;

typedef struct JSAsyncFromSyncIteratorData {
    JSValue sync_iter;
    JSValue next_method;
} JSAsyncFromSyncIteratorData;

typedef struct {
    char name[6];
    int16_t offset;
} js_tzabbr_entry;

extern const js_tzabbr_entry js_tzabbr[];
// extern const size_t JS_ARRAY_LEN_js_tzabbr;

struct TA_sort_context {
    JSContext *ctx;
    int exception; /* 1 = exception, 2 = detached typed array */
    uint8_t *array;
    JSValueConst cmp;
    JSValue (*getfun)(JSContext *ctx, const void *a);
    int elt_size;
};

#ifdef CONFIG_ATOMICS
typedef struct JSAtomicsWaiter {
    struct list_head link;
    BOOL linked;
    pthread_cond_t cond;
    int32_t *ptr;
} JSAtomicsWaiter;

static pthread_mutex_t js_atomics_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct list_head js_atomics_waiter_list =
    LIST_HEAD_INIT(js_atomics_waiter_list);
#endif

typedef struct JSWeakRefData {
    JSWeakRefHeader weakref_header;
    JSValue target;
} JSWeakRefData;

typedef struct JSFinRecEntry {
    struct list_head link;
    JSValue target;
    JSValue held_val;
    JSValue token;
} JSFinRecEntry;

typedef struct JSFinalizationRegistryData {
    JSWeakRefHeader weakref_header;
    struct list_head entries; /* list of JSFinRecEntry.link */
    JSContext *realm;
    JSValue cb;
} JSFinalizationRegistryData;

typedef enum JSToNumberHintEnum {
    TON_FLAG_NUMBER,
    TON_FLAG_NUMERIC,
} JSToNumberHintEnum;

/* argument of OP_special_object */
typedef enum {
    OP_SPECIAL_OBJECT_ARGUMENTS,
    OP_SPECIAL_OBJECT_MAPPED_ARGUMENTS,
    OP_SPECIAL_OBJECT_THIS_FUNC,
    OP_SPECIAL_OBJECT_NEW_TARGET,
    OP_SPECIAL_OBJECT_HOME_OBJECT,
    OP_SPECIAL_OBJECT_VAR_OBJECT,
    OP_SPECIAL_OBJECT_IMPORT_META,
} OPSpecialObjectEnum;

#define FUNC_RET_AWAIT         0
#define FUNC_RET_YIELD         1
#define FUNC_RET_YIELD_STAR    2
#define FUNC_RET_INITIAL_YIELD 3

#ifdef OPCODE_ASM_LABEL
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-label"
#endif

/* XXX: use enum */
#define GEN_MAGIC_NEXT   0
#define GEN_MAGIC_RETURN 1
#define GEN_MAGIC_THROW  2

#define special_every    0
#define special_some     1
#define special_forEach  2
#define special_map      3
#define special_filter   4
#define special_TA       8

#define special_reduce       0
#define special_reduceRight  1

#define MAGIC_SET (1 << 0)
#define MAGIC_WEAK (1 << 1)

#define PROMISE_MAGIC_all        0
#define PROMISE_MAGIC_allSettled 1
#define PROMISE_MAGIC_any        2

#define special_indexOf 0
#define special_lastIndexOf 1
#define special_includes -1

#define ATOD_INT_ONLY        (1 << 0)
/* accept Oo and Ob prefixes in addition to 0x prefix if radix = 0 */
#define ATOD_ACCEPT_BIN_OCT  (1 << 2)
/* accept O prefix as octal if radix == 0 and properly formed (Annex B) */
#define ATOD_ACCEPT_LEGACY_OCTAL  (1 << 4)
/* accept _ between digits as a digit separator */
#define ATOD_ACCEPT_UNDERSCORES  (1 << 5)
/* allow a suffix to override the type */
#define ATOD_ACCEPT_SUFFIX    (1 << 6)
/* default type */
#define ATOD_TYPE_MASK        (3 << 7)
#define ATOD_TYPE_FLOAT64     (0 << 7)
#define ATOD_TYPE_BIG_INT     (1 << 7)
/* accept -0x1 */
#define ATOD_ACCEPT_PREFIX_AFTER_SIGN (1 << 10)

#define M2(op1, op2)            ((op1) | ((op2) << 8))
#define M3(op1, op2, op3)       ((op1) | ((op2) << 8) | ((op3) << 16))
#define M4(op1, op2, op3, op4)  ((op1) | ((op2) << 8) | ((op3) << 16) | ((op4) << 24))

enum {
    TOK_NUMBER = -128,
    TOK_STRING,
    TOK_TEMPLATE,
    TOK_IDENT,
    TOK_REGEXP,
    /* warning: order matters (see js_parse_assign_expr) */
    TOK_MUL_ASSIGN,
    TOK_DIV_ASSIGN,
    TOK_MOD_ASSIGN,
    TOK_PLUS_ASSIGN,
    TOK_MINUS_ASSIGN,
    TOK_SHL_ASSIGN,
    TOK_SAR_ASSIGN,
    TOK_SHR_ASSIGN,
    TOK_AND_ASSIGN,
    TOK_XOR_ASSIGN,
    TOK_OR_ASSIGN,
    TOK_POW_ASSIGN,
    TOK_LAND_ASSIGN,
    TOK_LOR_ASSIGN,
    TOK_DOUBLE_QUESTION_MARK_ASSIGN,
    TOK_DEC,
    TOK_INC,
    TOK_SHL,
    TOK_SAR,
    TOK_SHR,
    TOK_LT,
    TOK_LTE,
    TOK_GT,
    TOK_GTE,
    TOK_EQ,
    TOK_STRICT_EQ,
    TOK_NEQ,
    TOK_STRICT_NEQ,
    TOK_LAND,
    TOK_LOR,
    TOK_POW,
    TOK_ARROW,
    TOK_ELLIPSIS,
    TOK_DOUBLE_QUESTION_MARK,
    TOK_QUESTION_MARK_DOT,
    TOK_ERROR,
    TOK_PRIVATE_NAME,
    TOK_EOF,
    /* keywords: WARNING: same order as atoms */
    TOK_NULL, /* must be first */
    TOK_FALSE,
    TOK_TRUE,
    TOK_IF,
    TOK_ELSE,
    TOK_RETURN,
    TOK_VAR,
    TOK_THIS,
    TOK_DELETE,
    TOK_VOID,
    TOK_TYPEOF,
    TOK_NEW,
    TOK_IN,
    TOK_INSTANCEOF,
    TOK_DO,
    TOK_WHILE,
    TOK_FOR,
    TOK_BREAK,
    TOK_CONTINUE,
    TOK_SWITCH,
    TOK_CASE,
    TOK_DEFAULT,
    TOK_THROW,
    TOK_TRY,
    TOK_CATCH,
    TOK_FINALLY,
    TOK_FUNCTION,
    TOK_DEBUGGER,
    TOK_WITH,
    /* FutureReservedWord */
    TOK_CLASS,
    TOK_CONST,
    TOK_ENUM,
    TOK_EXPORT,
    TOK_EXTENDS,
    TOK_IMPORT,
    TOK_SUPER,
    /* FutureReservedWords when parsing strict mode code */
    TOK_IMPLEMENTS,
    TOK_INTERFACE,
    TOK_LET,
    TOK_PACKAGE,
    TOK_PRIVATE,
    TOK_PROTECTED,
    TOK_PUBLIC,
    TOK_STATIC,
    TOK_YIELD,
    TOK_AWAIT, /* must be last */
    TOK_OF,     /* only used for js_parse_skip_parens_token() */
};

#define TOK_FIRST_KEYWORD   TOK_NULL
#define TOK_LAST_KEYWORD    TOK_AWAIT

/* unicode code points */
#define CP_NBSP 0x00a0
#define CP_BOM  0xfeff

#define CP_LS   0x2028
#define CP_PS   0x2029


#if SHORT_OPCODES
/* After the final compilation pass, short opcodes are used. Their
   opcodes overlap with the temporary opcodes which cannot appear in
   the final bytecode. Their description is after the temporary
   opcodes in opcode_info[]. */
#define short_opcode_info(op)           \
    opcode_info[(op) >= OP_TEMP_START ? \
                (op) + (OP_TEMP_END - OP_TEMP_START) : (op)]
#else
#define short_opcode_info(op) opcode_info[op]
#endif

#define JS_BACKTRACE_FLAG_SKIP_FIRST_LEVEL (1 << 0)

typedef enum {
    JS_VAR_DEF_WITH,
    JS_VAR_DEF_LET,
    JS_VAR_DEF_CONST,
    JS_VAR_DEF_FUNCTION_DECL, /* function declaration */
    JS_VAR_DEF_NEW_FUNCTION_DECL, /* async/generator function declaration */
    JS_VAR_DEF_CATCH,
    JS_VAR_DEF_VAR,
} JSVarDefEnum;

typedef enum {
    PUT_LVALUE_NOKEEP, /* [depth] v -> */
    PUT_LVALUE_NOKEEP_DEPTH, /* [depth] v -> , keep depth (currently
                                just disable optimizations) */
    PUT_LVALUE_KEEP_TOP,  /* [depth] v -> v */
    PUT_LVALUE_KEEP_SECOND, /* [depth] v0 v -> v0 */
    PUT_LVALUE_NOKEEP_BOTTOM, /* v [depth] -> */
} PutLValueEnum;

typedef enum FuncCallType {
    FUNC_CALL_NORMAL,
    FUNC_CALL_NEW,
    FUNC_CALL_SUPER_CTOR,
    FUNC_CALL_TEMPLATE,
} FuncCallType;

typedef enum JSResolveResultEnum {
    JS_RESOLVE_RES_EXCEPTION = -1, /* memory alloc error */
    JS_RESOLVE_RES_FOUND = 0,
    JS_RESOLVE_RES_NOT_FOUND,
    JS_RESOLVE_RES_CIRCULAR,
    JS_RESOLVE_RES_AMBIGUOUS,
} JSResolveResultEnum;

enum {
    ArrayFind,
    ArrayFindIndex,
    ArrayFindLast,
    ArrayFindLastIndex,
};

enum {
    magic_string_anchor,
    magic_string_big,
    magic_string_blink,
    magic_string_bold,
    magic_string_fixed,
    magic_string_fontcolor,
    magic_string_fontsize,
    magic_string_italics,
    magic_string_link,
    magic_string_small,
    magic_string_strike,
    magic_string_sub,
    magic_string_sup,
};

enum {
    B64_ALPHABET_BASE64 = 0,
    B64_ALPHABET_BASE64URL = 1,
};

enum {
    B64_LAST_LOOSE = 0,
    B64_LAST_STRICT = 1,
    B64_LAST_STOP_BEFORE_PARTIAL = 2,
};

#ifdef CONFIG_ATOMICS
typedef enum AtomicsOpEnum {
    ATOMICS_OP_ADD,
    ATOMICS_OP_AND,
    ATOMICS_OP_OR,
    ATOMICS_OP_SUB,
    ATOMICS_OP_XOR,
    ATOMICS_OP_EXCHANGE,
    ATOMICS_OP_COMPARE_EXCHANGE,
    ATOMICS_OP_LOAD,
} AtomicsOpEnum;
#endif

typedef enum JSFreeModuleEnum {
    JS_FREE_MODULE_ALL,
    JS_FREE_MODULE_NOT_RESOLVED,
} JSFreeModuleEnum;

typedef enum JSStrictEqModeEnum {
    JS_EQ_STRICT,
    JS_EQ_SAME_VALUE,
    JS_EQ_SAME_VALUE_ZERO,
} JSStrictEqModeEnum;

/* return the value associated to the autoinit property or an exception */
typedef JSValue JSAutoInitFunc(JSContext *ctx, JSObject *p, JSAtom atom, void *opaque);

extern JSAutoInitFunc *js_autoinit_func_table[];

/* 第一批：内存管理、字符串工具、形状（Shape）及基本对象操作 */
size_t js_def_malloc_usable_size(const void *ptr);
void *js_def_malloc(JSMallocState *s, size_t size);
void *js_def_realloc(JSMallocState *s, void *ptr, size_t size);
void js_def_free(JSMallocState *s, void *ptr);
int get_block_size_index(size_t size);
JSMallocBlockHeader *get_zero_size_block(JSMallocContext *s);
void js_malloc_init(JSMallocContext *s);
void *get_arena_block(JSMallocArena *ar, unsigned int idx, unsigned int block_size);
JSMallocArena *js_malloc_new_arena(JSMallocContext *s, int block_size_idx);
void *js_malloc_large(JSMallocContext *s, size_t size);
void *__js_malloc(JSMallocContext *s, size_t size);
void __js_free(JSMallocContext *s, void *ptr);
void *__js_realloc(JSMallocContext *s, void *ptr, size_t size);
size_t __js_malloc_usable_size(JSMallocContext *s, const char *ptr);
void js_malloc_dump_arenas(JSMallocContext *s);
#ifdef JS_MALLOC_USE_ITER
typedef void JSMallocIterFunc(void *opaque, void *ptr);
void js_malloc_iter(JSMallocContext *s, JSMallocIterFunc *iter_func, void *iter_opaque);
#endif
void js_trigger_gc(JSRuntime *rt, size_t size);
int js_realloc_array(JSContext *ctx, void **parray, int elem_size, int *psize, int req_size);
int js_resize_array(JSContext *ctx, void **parray, int elem_size, int *psize, int req_size);
void js_dbuf_init(JSContext *ctx, DynBuf *s);
void *js_realloc_bytecode_rt(void *opaque, void *ptr, size_t size);
void js_dbuf_bytecode_init(JSContext *ctx, DynBuf *s);
int is_digit(int c);
int string_get(const JSString *p, int idx);
int init_class_range(JSRuntime *rt, const JSClassShortDef *tab, int start, int count);
uintptr_t js_get_stack_pointer(void);
BOOL js_check_stack_overflow(JSRuntime *rt, size_t alloca_size);
int JS_ResizeAtomHash(JSRuntime *rt, int new_hash_size);
int JS_InitAtoms(JSRuntime *rt);
JSAtom JS_DupAtomRT(JSRuntime *rt, JSAtom v);
JSAtomKindEnum JS_AtomGetKind(JSContext *ctx, JSAtom v);
BOOL JS_AtomIsString(JSContext *ctx, JSAtom v);
JSAtom js_get_atom_index(JSRuntime *rt, JSAtomStruct *p);
JSAtom __JS_NewAtom(JSRuntime *rt, JSString *str, int atom_type);
JSAtom __JS_NewAtomInit(JSRuntime *rt, const char *str, int len, int atom_type);
JSAtom __JS_FindAtom(JSRuntime *rt, const char *str, size_t len, int atom_type);
void JS_FreeAtomStruct(JSRuntime *rt, JSAtomStruct *p);
void __JS_FreeAtom(JSRuntime *rt, uint32_t i);
size_t count_ascii(const uint8_t *buf, size_t len);
int string_buffer_set_error(StringBuffer *s);
int string_buffer_widen(StringBuffer *s, int size);
int string_buffer_realloc(StringBuffer *s, int new_len, int c);
int string_buffer_putc16_slow(StringBuffer *s, uint32_t c);
int string_buffer_putc8(StringBuffer *s, uint32_t c);
int string_buffer_putc16(StringBuffer *s, uint32_t c);
int string_buffer_putc_slow(StringBuffer *s, uint32_t c);
int string_buffer_putc(StringBuffer *s, uint32_t c);
int string_getc(const JSString *p, int *pidx);
int string_buffer_write8(StringBuffer *s, const uint8_t *p, int len);
int string_buffer_write16(StringBuffer *s, const uint16_t *p, int len);
int string_buffer_puts8(StringBuffer *s, const char *str);
int string_buffer_concat(StringBuffer *s, const JSString *p, uint32_t from, uint32_t to);
int string_buffer_concat_value(StringBuffer *s, JSValueConst v);
int string_buffer_concat_value_free(StringBuffer *s, JSValue v);
int string_buffer_fill(StringBuffer *s, int c, int count);
JSValue string_buffer_end(StringBuffer *s);
int memcmp16_8(const uint16_t *src1, const uint8_t *src2, int len);
int memcmp16(const uint16_t *src1, const uint16_t *src2, int len);
int js_string_memcmp(const JSString *p1, int pos1, const JSString *p2, int pos2, int len);
BOOL js_string_eq(JSContext *ctx, const JSString *p1, const JSString *p2);
int js_string_compare(JSContext *ctx, const JSString *p1, const JSString *p2);
void copy_str16(uint16_t *dst, const JSString *p, int offset, int len);
JSValue JS_ConcatString1(JSContext *ctx, const JSString *p1, const JSString *p2);
BOOL JS_ConcatStringInPlace(JSContext *ctx, JSString *p1, JSValueConst op2);
JSValue JS_ConcatString3(JSContext *ctx, const char *str1, JSValue str2, const char *str3);
JSValue JS_ConcatString2(JSContext *ctx, JSValue op1, JSValue op2);
JSValue JS_ConcatString(JSContext *ctx, JSValue op1, JSValue op2);
int string_rope_get(JSValueConst val, uint32_t idx);
void string_rope_iter_init(JSStringRopeIter *s, JSValueConst val);
JSString *string_rope_iter_next(JSStringRopeIter *s);
uint32_t string_rope_get_len(JSValueConst val);
int js_string_rope_compare(JSContext *ctx, JSValueConst op1, JSValueConst op2, BOOL eq_only);
JSValue js_linearize_string_rope(JSContext *ctx, JSValue rope);
JSValue js_rebalancee_string_rope(JSContext *ctx, JSValueConst rope);
JSValue js_new_string_rope(JSContext *ctx, JSValue op1, JSValue op2);
int js_rebalancee_string_rope_rec(JSContext *ctx, JSValue *buckets, JSValueConst val);
JSValue js_rebalancee_string_rope(JSContext *ctx, JSValueConst rope);

JSShape *js_new_shape_nohash(JSContext *ctx, JSObject *proto, int hash_size, int prop_size);
JSShape *js_new_shape2(JSContext *ctx, JSObject *proto, int hash_size, int prop_size);
JSShape *js_new_shape(JSContext *ctx, JSObject *proto);
JSShape *js_clone_shape(JSContext *ctx, JSShape *sh1);
JSShape *js_dup_shape(JSShape *sh);
void js_free_shape0(JSRuntime *rt, JSShape *sh);
void js_free_shape(JSRuntime *rt, JSShape *sh);
void js_free_shape_null(JSRuntime *rt, JSShape *sh);
int resize_properties(JSContext *ctx, JSShape **psh, JSObject *p, uint32_t count);
int compact_properties(JSContext *ctx, JSObject *p);
int add_shape_property(JSContext *ctx, JSShape **psh, JSObject *p, JSAtom atom, int prop_flags);
JSShape *find_hashed_shape_proto(JSRuntime *rt, JSObject *proto);
JSShape *find_hashed_shape_prop(JSRuntime *rt, JSShape *sh, JSAtom atom, int prop_flags);
void JS_DumpShape(JSRuntime *rt, int i, JSShape *sh);
void JS_DumpShapes(JSRuntime *rt);
JSValue JS_NewObjectFromShape(JSContext *ctx, JSShape *sh, JSClassID class_id, JSProperty *props);
JSObject *get_proto_obj(JSValueConst proto_val);
JSValue JS_NewObjectProtoClassAlloc(JSContext *ctx, JSValueConst proto_val, JSClassID class_id, int n_alloc_props);
int JS_SetObjectData(JSContext *ctx, JSValueConst obj, JSValue val);
void js_function_set_properties(JSContext *ctx, JSValueConst func_obj, JSAtom name, int len);
BOOL js_class_has_bytecode(JSClassID class_id);
JSFunctionBytecode *JS_GetFunctionBytecode(JSValueConst val);
void js_method_set_home_object(JSContext *ctx, JSValueConst func_obj, JSValueConst home_obj);
JSValue js_get_function_name(JSContext *ctx, JSAtom name);
int js_method_set_properties(JSContext *ctx, JSValueConst func_obj, JSAtom name, int flags, JSValueConst home_obj);
JSValue JS_NewCFunction3(JSContext *ctx, JSCFunction *func, const char *name, int length, JSCFunctionEnum cproto, int magic, JSValueConst proto_val, int n_fields);
void js_c_function_data_finalizer(JSRuntime *rt, JSValue val);
void js_c_function_data_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
JSValue js_c_function_data_call(JSContext *ctx, JSValueConst func_obj, JSValueConst this_val, int argc, JSValueConst *argv, int flags);
void js_autoinit_free(JSRuntime *rt, JSProperty *pr);
void js_autoinit_mark(JSRuntime *rt, JSProperty *pr, JS_MarkFunc *mark_func);
void free_property(JSRuntime *rt, JSProperty *pr, int prop_flags);
JSShapeProperty *find_own_property1(JSObject *p, JSAtom atom);
JSShapeProperty *find_own_property(JSProperty **ppr, JSObject *p, JSAtom atom);
void set_cycle_flag(JSContext *ctx, JSValueConst obj);
void free_var_ref(JSRuntime *rt, JSVarRef *var_ref);
void js_array_finalizer(JSRuntime *rt, JSValue val);
void js_array_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
void js_object_data_finalizer(JSRuntime *rt, JSValue val);
void js_object_data_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
void js_c_function_finalizer(JSRuntime *rt, JSValue val);
void js_c_function_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
void js_bytecode_function_finalizer(JSRuntime *rt, JSValue val);
void js_bytecode_function_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
void js_bound_function_finalizer(JSRuntime *rt, JSValue val);
void js_bound_function_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
void js_for_in_iterator_finalizer(JSRuntime *rt, JSValue val);
void js_for_in_iterator_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
void free_object(JSRuntime *rt, JSObject *p);
void free_gc_object(JSRuntime *rt, JSGCObjectHeader *gp);
void free_zero_refcount(JSRuntime *rt);
void gc_remove_weak_objects(JSRuntime *rt);
void add_gc_object(JSRuntime *rt, JSGCObjectHeader *h, JSGCObjectTypeEnum type);
void remove_gc_object(JSGCObjectHeader *h);
void mark_children(JSRuntime *rt, JSGCObjectHeader *gp, JS_MarkFunc *mark_func);
void gc_decref_child(JSRuntime *rt, JSGCObjectHeader *p);
void gc_decref(JSRuntime *rt);
void gc_scan_incref_child(JSRuntime *rt, JSGCObjectHeader *p);
void gc_scan_incref_child2(JSRuntime *rt, JSGCObjectHeader *p);
void gc_scan(JSRuntime *rt);
void gc_free_cycles(JSRuntime *rt);
void JS_RunGCInternal(JSRuntime *rt, BOOL remove_weak_objects);
void JS_MarkContext(JSRuntime *rt, JSContext *ctx, JS_MarkFunc *mark_func);
void compute_value_size(JSValueConst val, JSMemoryUsage_helper *hp);
void compute_jsstring_size(JSString *str, JSMemoryUsage_helper *hp);
void compute_bytecode_size(JSFunctionBytecode *b, JSMemoryUsage_helper *hp);
void update_stack_limit(JSRuntime *rt);
BOOL is_strict_mode(JSContext *ctx);
BOOL __JS_AtomIsTaggedInt(JSAtom v);
JSAtom __JS_AtomFromUInt32(uint32_t v);
uint32_t __JS_AtomToUInt32(JSAtom atom);
int is_num(int c);
BOOL is_num_string(uint32_t *pval, const JSString *p);
uint32_t hash_string8(const uint8_t *str, size_t len, uint32_t h);
uint32_t hash_string16(const uint16_t *str, size_t len, uint32_t h);
uint32_t hash_string(const JSString *str, uint32_t h);
uint32_t hash_string_rope(JSValueConst val, uint32_t h);
void JS_DumpChar(FILE *fo, int c, int sep);
void JS_DumpString(JSRuntime *rt, const JSString *p);
void JS_DumpAtoms(JSRuntime *rt);
/* 第二批：内存统计、对象列表、二进制读写、函数调用辅助、数组/字符串工具 */

void js_putc(JSPrintValueState *s, char c);
void js_puts(JSPrintValueState *s, const char *str);
void js_printf(JSPrintValueState *s, const char *fmt, ...);
void js_print_float64(JSPrintValueState *s, double d);
uint32_t js_string_get_length(JSValueConst val);
void js_print_string1(JSPrintValueState *s, JSString *p, int len, int sep);
void js_print_string_rec(JSPrintValueState *s, JSValueConst val, int sep, uint32_t pos);
void js_print_string(JSPrintValueState *s, JSValueConst val);
void js_print_raw_string(JSPrintValueState *s, JSValueConst val);
BOOL is_ascii_ident(const JSString *p);
void js_print_atom(JSPrintValueState *s, JSAtom atom);
uint32_t js_print_array_get_length(JSObject *p);
void js_print_comma(JSPrintValueState *s, int *pcomma_state);
void js_print_more_items(JSPrintValueState *s, int *pcomma_state, uint32_t n);
void js_print_regexp(JSPrintValueState *s, JSObject *p1);
void js_print_error(JSPrintValueState *s, JSObject *p);
void js_print_object(JSPrintValueState *s, JSObject *p);
int js_print_stack_index(JSPrintValueState *s, JSObject *p);
void js_print_value(JSPrintValueState *s, JSValueConst val);
void js_dump_value_write(void *opaque, const char *buf, size_t len);
void print_atom(JSContext *ctx, JSAtom atom);
void JS_DumpAtom(JSContext *ctx, const char *str, JSAtom atom);
void JS_DumpValue(JSContext *ctx, const char *str, JSValueConst val);
void JS_DumpValueRT(JSRuntime *rt, const char *str, JSValueConst val);
void JS_DumpObjectHeader(JSRuntime *rt);
void JS_DumpObject(JSRuntime *rt, JSObject *p);
void JS_DumpGCObject(JSRuntime *rt, JSGCObjectHeader *p);
int js_resolve_proxy(JSContext *ctx, JSValueConst *pval, int throw_exception);
int JS_CreateProperty(JSContext *ctx, JSObject *p, JSAtom prop, JSValueConst val, JSValueConst getter, JSValueConst setter, int flags);
int js_update_property_flags(JSContext *ctx, JSObject *p, JSShapeProperty **pprs, int flags);
BOOL check_define_prop_flags(int prop_flags, int flags);
int js_shape_prepare_update(JSContext *ctx, JSObject *p, JSShapeProperty **pprs);
int JS_DefineAutoInitProperty(JSContext *ctx, JSValueConst this_obj, JSAtom prop, JSAutoInitIDEnum id, void *opaque, int flags);
JSValue JS_ThrowSyntaxErrorVarRedeclaration(JSContext *ctx, JSAtom prop);
int JS_CheckDefineGlobalVar(JSContext *ctx, JSAtom prop, int flags);
int JS_GetGlobalVarRef(JSContext *ctx, JSAtom prop, JSValue *sp);
int JS_DeleteGlobalVar(JSContext *ctx, JSAtom prop);
int js_unary_arith_slow(JSContext *ctx, JSValue *sp, OPCodeEnum op);
int js_post_inc_slow(JSContext *ctx, JSValue *sp, OPCodeEnum op);
int js_not_slow(JSContext *ctx, JSValue *sp);
int js_binary_arith_slow(JSContext *ctx, JSValue *sp, OPCodeEnum op);
BOOL tag_is_string(uint32_t tag);
int js_add_slow(JSContext *ctx, JSValue *sp);
int js_binary_logic_slow(JSContext *ctx, JSValue *sp, OPCodeEnum op);
JSBigInt *JS_ToBigIntBuf(JSContext *ctx, JSBigIntBuf *buf1, JSValue op1);
int js_compare_bigint(JSContext *ctx, OPCodeEnum op, JSValue op1, JSValue op2);
int js_relational_slow(JSContext *ctx, JSValue *sp, OPCodeEnum op);
BOOL tag_is_number(uint32_t tag);
int js_eq_slow(JSContext *ctx, JSValue *sp, BOOL is_neq);
int js_shr_slow(JSContext *ctx, JSValue *sp);
BOOL js_strict_eq2(JSContext *ctx, JSValueConst op1, JSValueConst op2, JSStrictEqModeEnum eq_mode);
BOOL js_strict_eq(JSContext *ctx, JSValueConst op1, JSValueConst op2);
BOOL js_same_value(JSContext *ctx, JSValueConst op1, JSValueConst op2);
BOOL js_same_value_zero(JSContext *ctx, JSValueConst op1, JSValueConst op2);
int js_operator_in(JSContext *ctx, JSValue *sp);
int js_operator_private_in(JSContext *ctx, JSValue *sp);
int js_has_unscopable(JSContext *ctx, JSValueConst obj, JSAtom atom);
int js_operator_instanceof(JSContext *ctx, JSValue *sp);
int js_operator_typeof(JSContext *ctx, JSValueConst op1);
int js_operator_delete(JSContext *ctx, JSValue *sp);
JSValue js_throw_type_error(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_function_proto_fileName(JSContext *ctx, JSValueConst this_val);
JSValue js_function_proto_lineNumber(JSContext *ctx, JSValueConst this_val, int is_col);
int js_arguments_define_own_property(JSContext *ctx, JSValueConst this_obj, JSAtom prop, JSValueConst val, JSValueConst getter, JSValueConst setter, int flags);
JSValue js_build_arguments(JSContext *ctx, int argc, JSValueConst *argv);
void js_mapped_arguments_finalizer(JSRuntime *rt, JSValue val);
void js_mapped_arguments_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
JSValue js_build_mapped_arguments(JSContext *ctx, int argc, JSValueConst *argv, JSStackFrame *sf, int arg_count);
JSValue build_for_in_iterator(JSContext *ctx, JSValue obj);
int js_for_in_start(JSContext *ctx, JSValue *sp);
int js_for_in_prepare_prototype_chain_enum(JSContext *ctx, JSValueConst enum_obj);
int js_for_in_next(JSContext *ctx, JSValue *sp);
JSValue JS_GetIterator2(JSContext *ctx, JSValueConst obj, JSValueConst method);
JSValue JS_GetIterator(JSContext *ctx, JSValueConst obj, BOOL is_async);
JSValue JS_IteratorNext2(JSContext *ctx, JSValueConst enum_obj, JSValueConst method, int argc, JSValueConst *argv, int *pdone);
JSValue JS_IteratorNext(JSContext *ctx, JSValueConst enum_obj, JSValueConst method, int argc, JSValueConst *argv, BOOL *pdone);
int JS_IteratorClose(JSContext *ctx, JSValueConst enum_obj, BOOL is_exception_pending);
int js_for_of_start(JSContext *ctx, JSValue *sp, BOOL is_async);
int js_for_of_next(JSContext *ctx, JSValue *sp, int offset);
int js_for_await_of_next(JSContext *ctx, JSValue *sp);
JSValue JS_IteratorGetCompleteValue(JSContext *ctx, JSValueConst obj, BOOL *pdone);
int js_iterator_get_value_done(JSContext *ctx, JSValue *sp);
JSValue js_create_iterator_result(JSContext *ctx, JSValue val, BOOL done);
JSValue js_array_iterator_next(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, BOOL *pdone, int magic);
JSValue js_create_array_iterator(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic);
BOOL js_get_fast_array(JSContext *ctx, JSValueConst obj, JSValue **arrpp, uint32_t *countp);
int js_append_enumerate(JSContext *ctx, JSValue *sp);
int JS_CopyDataProperties(JSContext *ctx, JSValueConst target, JSValueConst source, JSValueConst excluded, BOOL setprop);
JSValueConst JS_GetActiveFunction(JSContext *ctx);
JSVarRef *js_create_var_ref(JSContext *ctx, BOOL is_lexical);
JSVarRef *get_var_ref(JSContext *ctx, JSStackFrame *sf, int var_idx, BOOL is_arg);
void js_global_object_finalizer(JSRuntime *rt, JSValue obj);
void js_global_object_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
JSVarRef *js_global_object_get_uninitialized_var(JSContext *ctx, JSObject *p1, JSAtom atom);
JSVarRef *js_global_object_find_uninitialized_var(JSContext *ctx, JSObject *p, JSAtom atom, BOOL is_lexical);
JSVarRef *js_closure_define_global_var(JSContext *ctx, JSClosureVar *cv, BOOL is_direct_or_indirect_eval);
JSVarRef *js_closure_global_var(JSContext *ctx, JSClosureVar *cv);
JSValue js_closure2(JSContext *ctx, JSValue func_obj, JSFunctionBytecode *b, JSVarRef **cur_var_refs, JSStackFrame *sf, BOOL is_eval, JSModuleDef *m);
JSValue js_instantiate_prototype(JSContext *ctx, JSObject *p, JSAtom atom, void *opaque);
JSValue js_closure(JSContext *ctx, JSValue bfunc, JSVarRef **cur_var_refs, JSStackFrame *sf, BOOL is_eval);
int js_op_define_class(JSContext *ctx, JSValue *sp, JSAtom class_name, int class_flags, JSVarRef **cur_var_refs, JSStackFrame *sf, BOOL is_computed_name);
void close_var_ref(JSRuntime *rt, JSStackFrame *sf, JSVarRef *var_ref);
void close_var_refs(JSRuntime *rt, JSFunctionBytecode *b, JSStackFrame *sf);
void close_lexical_var(JSContext *ctx, JSFunctionBytecode *b, JSStackFrame *sf, int var_idx);
JSValue js_call_c_function(JSContext *ctx, JSValueConst func_obj, JSValueConst this_obj, int argc, JSValueConst *argv, int flags);
JSValue js_call_bound_function(JSContext *ctx, JSValueConst func_obj, JSValueConst this_obj, int argc, JSValueConst *argv, int flags);
JSValue JS_CallInternal(JSContext *caller_ctx, JSValueConst func_obj, JSValueConst this_obj, JSValueConst new_target, int argc, JSValue *argv, int flags);
JSContext *JS_GetFunctionRealm(JSContext *ctx, JSValueConst func_obj);
JSValue js_create_from_ctor(JSContext *ctx, JSValueConst ctor, int class_id);
JSValue JS_CallConstructorInternal(JSContext *ctx, JSValueConst func_obj, JSValueConst new_target, int argc, JSValue *argv, int flags);
JSAsyncFunctionState *async_func_init(JSContext *ctx, JSValueConst func_obj, JSValueConst this_obj, int argc, JSValueConst *argv);
void async_func_free_frame(JSRuntime *rt, JSAsyncFunctionState *s);
JSValue async_func_resume(JSContext *ctx, JSAsyncFunctionState *s);
void __async_func_free(JSRuntime *rt, JSAsyncFunctionState *s);
void async_func_free(JSRuntime *rt, JSAsyncFunctionState *s);
void free_generator_stack_rt(JSRuntime *rt, JSGeneratorData *s);
void js_generator_finalizer(JSRuntime *rt, JSValue obj);
void free_generator_stack(JSContext *ctx, JSGeneratorData *s);
void js_generator_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
JSValue js_generator_next(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, BOOL *pdone, int magic);
JSValue js_generator_function_call(JSContext *ctx, JSValueConst func_obj, JSValueConst this_obj, int argc, JSValueConst *argv, int flags);
void js_async_function_resolve_finalizer(JSRuntime *rt, JSValue val);
void js_async_function_resolve_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
int js_async_function_resolve_create(JSContext *ctx, JSAsyncFunctionState *s, JSValue *resolving_funcs);
void js_async_function_resume(JSContext *ctx, JSAsyncFunctionState *s);
JSValue js_async_function_resolve_call(JSContext *ctx, JSValueConst func_obj, JSValueConst this_obj, int argc, JSValueConst *argv, int flags);
JSValue js_async_function_call(JSContext *ctx, JSValueConst func_obj, JSValueConst this_obj, int argc, JSValueConst *argv, int flags);
void js_async_generator_free(JSRuntime *rt, JSAsyncGeneratorData *s);
void js_async_generator_finalizer(JSRuntime *rt, JSValue obj);
void js_async_generator_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
JSValue js_async_generator_resolve_function(JSContext *ctx, JSValueConst this_obj, int argc, JSValueConst *argv, int magic, JSValue *func_data);
int js_async_generator_resolve_function_create(JSContext *ctx, JSValueConst generator, JSValue *resolving_funcs, BOOL is_resume_next);
int js_async_generator_await(JSContext *ctx, JSAsyncGeneratorData *s, JSValueConst value);
void js_async_generator_resolve_or_reject(JSContext *ctx, JSAsyncGeneratorData *s, JSValueConst result, int is_reject);
void js_async_generator_resolve(JSContext *ctx, JSAsyncGeneratorData *s, JSValueConst value, BOOL done);
void js_async_generator_reject(JSContext *ctx, JSAsyncGeneratorData *s, JSValueConst exception);
void js_async_generator_complete(JSContext *ctx, JSAsyncGeneratorData *s);
int js_async_generator_completed_return(JSContext *ctx, JSAsyncGeneratorData *s, JSValueConst value);
void js_async_generator_resume_next(JSContext *ctx, JSAsyncGeneratorData *s);
JSValue js_async_generator_next(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic);
JSValue js_async_generator_function_call(JSContext *ctx, JSValueConst func_obj, JSValueConst this_obj, int argc, JSValueConst *argv, int flags);
/* 第三批：解析器核心函数 */

int js_parse_error_v(JSParseState *s, const uint8_t *ptr, const char *fmt, va_list ap);
int js_parse_error_pos(JSParseState *s, const uint8_t *ptr, const char *fmt, ...);
int js_parse_error(JSParseState *s, const char *fmt, ...);
int js_parse_expect(JSParseState *s, int tok);
int js_parse_expect_semi(JSParseState *s);
int js_parse_error_reserved_identifier(JSParseState *s);
int js_parse_template_part(JSParseState *s, const uint8_t *p);
int js_parse_string(JSParseState *s, int sep, BOOL do_throw, const uint8_t *p, JSToken *token, const uint8_t **pp);
BOOL token_is_pseudo_keyword(JSParseState *s, JSAtom atom);
int js_parse_regexp(JSParseState *s);
int ident_realloc(JSContext *ctx, char **pbuf, size_t *psize, char *static_buf);
void update_token_ident(JSParseState *s);
void reparse_ident_token(JSParseState *s);
JSAtom parse_ident(JSParseState *s, const uint8_t **pp, BOOL *pident_has_escape, int c, BOOL is_private);
int next_token(JSParseState *s);
JSAtom json_parse_ident(JSParseState *s, const uint8_t **pp, int c);
int json_parse_string(JSParseState *s, const uint8_t **pp, int sep);
int json_parse_number(JSParseState *s, const uint8_t **pp);
int json_next_token(JSParseState *s);
int match_identifier(const uint8_t *p, const char *s);
int simple_next_token(const uint8_t **pp, BOOL no_line_terminator);
int peek_token(JSParseState *s, BOOL no_line_terminator);
void skip_shebang(const uint8_t **pp, const uint8_t *buf_end);
int get_prev_opcode(JSFunctionDef *fd);
BOOL js_is_live_code(JSParseState *s);
void emit_u8(JSParseState *s, uint8_t val);
void emit_u16(JSParseState *s, uint16_t val);
void emit_u32(JSParseState *s, uint32_t val);
void emit_source_pos(JSParseState *s, const uint8_t *source_ptr);
void emit_op(JSParseState *s, uint8_t val);
void emit_atom(JSParseState *s, JSAtom name);
int update_label(JSFunctionDef *s, int label, int delta);
int new_label_fd(JSFunctionDef *fd);
int new_label(JSParseState *s);
void emit_label_raw(JSParseState *s, int label);
int emit_label(JSParseState *s, int label);
int emit_goto(JSParseState *s, int opcode, int label);
int cpool_add(JSParseState *s, JSValue val);
int emit_push_const(JSParseState *s, JSValueConst val, BOOL as_atom);
int find_arg(JSContext *ctx, JSFunctionDef *fd, JSAtom name);
int find_var(JSContext *ctx, JSFunctionDef *fd, JSAtom name);
int find_var_in_scope(JSContext *ctx, JSFunctionDef *fd, JSAtom name, int scope_level);
BOOL is_child_scope(JSContext *ctx, JSFunctionDef *fd, int scope, int parent_scope);
int find_var_in_child_scope(JSContext *ctx, JSFunctionDef *fd, JSAtom name, int scope_level);
JSGlobalVar *find_global_var(JSFunctionDef *fd, JSAtom name);
JSGlobalVar *find_lexical_global_var(JSFunctionDef *fd, JSAtom name);
int find_lexical_decl(JSContext *ctx, JSFunctionDef *fd, JSAtom name, int scope_idx, BOOL check_catch_var);
int push_scope(JSParseState *s);
int get_first_lexical_var(JSFunctionDef *fd, int scope);
void pop_scope(JSParseState *s);
void close_scopes(JSParseState *s, int scope, int scope_stop);
int add_var(JSContext *ctx, JSFunctionDef *fd, JSAtom name);
int add_scope_var(JSContext *ctx, JSFunctionDef *fd, JSAtom name, JSVarKindEnum var_kind);
int add_func_var(JSContext *ctx, JSFunctionDef *fd, JSAtom name);
int add_arguments_var(JSContext *ctx, JSFunctionDef *fd);
int add_arguments_arg(JSContext *ctx, JSFunctionDef *fd);
int add_arg(JSContext *ctx, JSFunctionDef *fd, JSAtom name);
JSGlobalVar *add_global_var(JSContext *ctx, JSFunctionDef *s, JSAtom name);
int define_var(JSParseState *s, JSFunctionDef *fd, JSAtom name, JSVarDefEnum var_def_type);
int add_private_class_field(JSParseState *s, JSFunctionDef *fd, JSAtom name, JSVarKindEnum var_kind, BOOL is_static);
int js_parse_expr(JSParseState *s);
int js_parse_function_decl(JSParseState *s, JSParseFunctionEnum func_type, JSFunctionKindEnum func_kind, JSAtom func_name, const uint8_t *ptr);
JSFunctionDef *js_parse_function_class_fields_init(JSParseState *s);
int js_parse_function_decl2(JSParseState *s, JSParseFunctionEnum func_type, JSFunctionKindEnum func_kind, JSAtom func_name, const uint8_t *ptr, JSParseExportEnum export_flag, JSFunctionDef **pfd);
int js_parse_assign_expr2(JSParseState *s, int parse_flags);
int js_parse_assign_expr(JSParseState *s);
int js_parse_unary(JSParseState *s, int parse_flags);
void push_break_entry(JSFunctionDef *fd, BlockEnv *be, JSAtom label_name, int label_break, int label_cont, int drop_count);
void pop_break_entry(JSFunctionDef *fd);
int emit_break(JSParseState *s, JSAtom name, int is_cont);
void emit_return(JSParseState *s, BOOL hasval);
int js_parse_statement_or_decl(JSParseState *s, int decl_mask);
int js_parse_statement(JSParseState *s);
int js_parse_block(JSParseState *s);
int js_parse_var(JSParseState *s, int parse_flags, int tok, BOOL export_flag);
BOOL is_label(JSParseState *s);
int is_let(JSParseState *s, int decl_mask);
int js_parse_for_in_of(JSParseState *s, int label_name, BOOL is_async);
void set_eval_ret_undefined(JSParseState *s);
BOOL has_with_scope(JSFunctionDef *s, int scope_level);
int get_lvalue(JSParseState *s, int *popcode, int *pscope, JSAtom *pname, int *plabel, int *pdepth, BOOL keep, int tok);
void put_lvalue(JSParseState *s, int opcode, int scope, JSAtom name, int label, PutLValueEnum special, BOOL is_let);
int js_parse_expr_paren(JSParseState *s);
int js_unsupported_keyword(JSParseState *s, JSAtom atom);
int js_define_var(JSParseState *s, JSAtom name, int tok);
void js_emit_spread_code(JSParseState *s, int depth);
int js_parse_check_duplicate_parameter(JSParseState *s, JSAtom name);
BOOL need_var_reference(JSParseState *s, int tok);
JSAtom js_parse_destructuring_var(JSParseState *s, int tok, int is_arg);
int js_parse_destructuring_element(JSParseState *s, int tok, int is_arg, int hasval, int has_ellipsis, BOOL allow_initializer, BOOL export_flag);
void optional_chain_test(JSParseState *s, int *poptional_chaining_label, int drop_count);
int js_parse_postfix_expr(JSParseState *s, int parse_flags);
int js_parse_delete(JSParseState *s);
int js_parse_expr_binary(JSParseState *s, int level, int parse_flags);
int js_parse_logical_and_or(JSParseState *s, int op, int parse_flags);
int js_parse_coalesce_expr(JSParseState *s, int parse_flags);
int js_parse_cond_expr(JSParseState *s, int parse_flags);
int js_parse_property_name(JSParseState *s, JSAtom *pname, BOOL allow_method, BOOL allow_var, BOOL allow_private);
int js_parse_get_pos(JSParseState *s, JSParsePos *sp);
int js_parse_seek_token(JSParseState *s, const JSParsePos *sp);
BOOL is_regexp_allowed(int tok);
BOOL has_lf_in_range(const uint8_t *p1, const uint8_t *p2);
int js_parse_skip_parens_token(JSParseState *s, int *pbits, BOOL no_line_terminator);
void set_object_name(JSParseState *s, JSAtom name);
void set_object_name_computed(JSParseState *s);
int js_parse_object_literal(JSParseState *s);
int js_parse_class_default_ctor(JSParseState *s, BOOL has_super, JSFunctionDef **pfd);
int find_private_class_field(JSContext *ctx, JSFunctionDef *fd, JSAtom name, int scope_level);
void emit_class_field_init(JSParseState *s);
JSAtom get_private_setter_name(JSContext *ctx, JSAtom name);
int emit_class_init_start(JSParseState *s, ClassFieldsDef *cf);
void emit_class_init_end(JSParseState *s, ClassFieldsDef *cf);
int js_parse_class(JSParseState *s, BOOL is_class_expr, JSParseExportEnum export_flag);
int js_parse_array_literal(JSParseState *s);
int js_parse_export(JSParseState *s);
int add_closure_var(JSContext *ctx, JSFunctionDef *s, JSClosureTypeEnum closure_type, int var_idx, JSAtom var_name, BOOL is_const, BOOL is_lexical, JSVarKindEnum var_kind);
int find_closure_var(JSContext *ctx, JSFunctionDef *s, JSAtom var_name);
int get_closure_var(JSContext *ctx, JSFunctionDef *s, JSFunctionDef *fd, JSClosureTypeEnum closure_type, int var_idx, JSAtom var_name, BOOL is_const, BOOL is_lexical, JSVarKindEnum var_kind);
int get_with_scope_opcode(int op);
BOOL can_opt_put_ref_value(const uint8_t *bc_buf, int pos);
BOOL can_opt_put_global_ref_value(const uint8_t *bc_buf, int pos);
int optimize_scope_make_ref(JSContext *ctx, JSFunctionDef *s, DynBuf *bc, uint8_t *bc_buf, LabelSlot *ls, int pos_next, int get_op, int var_idx);
int add_var_this(JSContext *ctx, JSFunctionDef *fd);
int resolve_pseudo_var(JSContext *ctx, JSFunctionDef *s, JSAtom var_name);
void var_object_test(JSContext *ctx, JSFunctionDef *s, JSAtom var_name, int op, DynBuf *bc, int *plabel_done, BOOL is_with);
void capture_var(JSFunctionDef *s, JSVarDef *vd);
int resolve_scope_var(JSContext *ctx, JSFunctionDef *s, JSAtom var_name, int scope_level, int op, DynBuf *bc, uint8_t *bc_buf, LabelSlot *ls, int pos_next);
int find_private_class_field_all(JSContext *ctx, JSFunctionDef *fd, JSAtom name, int scope_level);
void get_loc_or_ref(DynBuf *bc, BOOL is_ref, int idx);
int resolve_scope_private_field1(JSContext *ctx, BOOL *pis_ref, int *pvar_kind, JSFunctionDef *s, JSAtom var_name, int scope_level);
int resolve_scope_private_field(JSContext *ctx, JSFunctionDef *s, JSAtom var_name, int scope_level, int op, DynBuf *bc);
void mark_eval_captured_variables(JSContext *ctx, JSFunctionDef *s, int scope_level);
BOOL is_var_in_arg_scope(JSAtom var_name, JSVarKindEnum var_kind);
void add_eval_variables(JSContext *ctx, JSFunctionDef *s);
void set_closure_from_var(JSContext *ctx, JSClosureVar *cv, JSBytecodeVarDef *vd, int var_idx);
int add_closure_variables(JSContext *ctx, JSFunctionDef *s, JSFunctionBytecode *b, int scope_idx);
BOOL code_match(CodeContext *s, int pos, ...);
void instantiate_hoisted_definitions(JSContext *ctx, JSFunctionDef *s, DynBuf *bc);
int skip_dead_code(JSFunctionDef *s, const uint8_t *bc_buf, int bc_len, int pos, int *linep);
int get_label_pos(JSFunctionDef *s, int label);
int resolve_variables(JSContext *ctx, JSFunctionDef *s);
void add_pc2line_info(JSFunctionDef *s, uint32_t pc, uint32_t source_pos);
void compute_pc2line_info(JSFunctionDef *s);
RelocEntry *add_reloc(JSContext *ctx, LabelSlot *ls, uint32_t addr, int size);
BOOL code_has_label(CodeContext *s, int pos, int label);
int find_jump_target(JSFunctionDef *s, int label0, int *pop, int *pline);
void push_short_int(DynBuf *bc_out, int val);
void put_short_code(DynBuf *bc_out, int op, int idx);
int resolve_labels(JSContext *ctx, JSFunctionDef *s);
int ss_check(JSContext *ctx, StackSizeState *s, int pos, int op, int stack_len, int catch_pos);
int compute_stack_size(JSContext *ctx, JSFunctionDef *fd, int *pstack_size);
int add_global_variables(JSContext *ctx, JSFunctionDef *fd);
JSValue js_create_function(JSContext *ctx, JSFunctionDef *fd);
void free_function_bytecode(JSRuntime *rt, JSFunctionBytecode *b);
int js_parse_directives(JSParseState *s);
BOOL is_strict_future_keyword(JSAtom atom);
int js_parse_function_check_names(JSParseState *s, JSFunctionDef *fd, JSAtom func_name);
JSFunctionDef *js_parse_function_class_fields_init(JSParseState *s);
int js_parse_function_decl2(JSParseState *s, JSParseFunctionEnum func_type, JSFunctionKindEnum func_kind, JSAtom func_name, const uint8_t *ptr, JSParseExportEnum export_flag, JSFunctionDef **pfd);
int js_parse_function_decl(JSParseState *s, JSParseFunctionEnum func_type, JSFunctionKindEnum func_kind, JSAtom func_name, const uint8_t *ptr);
int js_parse_program(JSParseState *s);
void js_parse_init(JSContext *ctx, JSParseState *s, const char *input, size_t input_len, const char *filename);
JSValue JS_EvalFunctionInternal(JSContext *ctx, JSValue fun_obj, JSValueConst this_obj, JSVarRef **var_refs, JSStackFrame *sf);
JSValue __JS_EvalInternal(JSContext *ctx, JSValueConst this_obj, const char *input, size_t input_len, const char *filename, int flags, int scope_idx);
JSValue JS_EvalObject(JSContext *ctx, JSValueConst this_obj, JSValueConst val, int flags, int scope_idx);
// int JS_ResolveModule(JSContext *ctx, JSValueConst obj);
/* 第四批：模块、Promise、Proxy、Map/Set、WeakRef、FinalizationRegistry、TypedArray、DataView、Atomics */

JSModuleDef *js_new_module_def(JSContext *ctx, JSAtom name);
void js_mark_module_def(JSRuntime *rt, JSModuleDef *m, JS_MarkFunc *mark_func);
void js_free_module_def(JSRuntime *rt, JSModuleDef *m);
int add_req_module_entry(JSContext *ctx, JSModuleDef *m, JSAtom module_name);
JSExportEntry *find_export_entry(JSContext *ctx, JSModuleDef *m, JSAtom export_name);
JSExportEntry *add_export_entry2(JSContext *ctx, JSParseState *s, JSModuleDef *m, JSAtom local_name, JSAtom export_name, JSExportTypeEnum export_type);
JSExportEntry *add_export_entry(JSParseState *s, JSModuleDef *m, JSAtom local_name, JSAtom export_name, JSExportTypeEnum export_type);
int add_star_export_entry(JSContext *ctx, JSModuleDef *m, int req_module_idx);
char *js_default_module_normalize_name(JSContext *ctx, const char *base_name, const char *name);
JSModuleDef *js_find_loaded_module(JSContext *ctx, JSAtom name);
JSModuleDef *js_host_resolve_imported_module(JSContext *ctx, const char *base_cname, const char *cname1, JSValueConst attributes);
JSModuleDef *js_host_resolve_imported_module_atom(JSContext *ctx, JSAtom base_module_name, JSAtom module_name1, JSValueConst attributes);
int find_resolve_entry(JSResolveState *s, JSModuleDef *m, JSAtom name);
int add_resolve_entry(JSContext *ctx, JSResolveState *s, JSModuleDef *m, JSAtom name);
JSResolveResultEnum js_resolve_export1(JSContext *ctx, JSModuleDef **pmodule, JSExportEntry **pme, JSModuleDef *m, JSAtom export_name, JSResolveState *s);
JSResolveResultEnum js_resolve_export(JSContext *ctx, JSModuleDef **pmodule, JSExportEntry **pme, JSModuleDef *m, JSAtom export_name);
void js_resolve_export_throw_error(JSContext *ctx, JSResolveResultEnum res, JSModuleDef *m, JSAtom export_name);
int find_exported_name(GetExportNamesState *s, JSAtom name);
int get_exported_names(JSContext *ctx, GetExportNamesState *s, JSModuleDef *m, BOOL from_star);
int js_module_ns_has(JSContext *ctx, JSValueConst obj, JSAtom atom);
int exported_names_cmp(const void *p1, const void *p2, void *opaque);
JSValue js_module_ns_autoinit(JSContext *ctx, JSObject *p, JSAtom atom, void *opaque);
JSValue js_build_module_ns(JSContext *ctx, JSModuleDef *m);
// JSValue JS_GetModuleNamespace(JSContext *ctx, JSModuleDef *m);
int js_resolve_module(JSContext *ctx, JSModuleDef *m);
int js_create_module_bytecode_function(JSContext *ctx, JSModuleDef *m);
int js_create_module_function(JSContext *ctx, JSModuleDef *m);
int js_inner_module_linking(JSContext *ctx, JSModuleDef *m, JSModuleDef **pstack_top, int index);
int js_link_module(JSContext *ctx, JSModuleDef *m);
JSValue js_import_meta(JSContext *ctx);
JSValue JS_NewModuleValue(JSContext *ctx, JSModuleDef *m);
JSValue js_load_module_rejected(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic, JSValue *func_data);
JSValue js_load_module_fulfilled(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic, JSValue *func_data);
void JS_LoadModuleInternal(JSContext *ctx, const char *basename, const char *filename, JSValueConst *resolving_funcs, JSValueConst attributes);
JSValue js_dynamic_import_job(JSContext *ctx, int argc, JSValueConst *argv);
JSValue js_dynamic_import(JSContext *ctx, JSValueConst specifier, JSValueConst options);
void js_set_module_evaluated(JSContext *ctx, JSModuleDef *m);
BOOL find_in_exec_module_list(ExecModuleList *exec_list, JSModuleDef *m);
int gather_available_ancestors(JSContext *ctx, JSModuleDef *module, ExecModuleList *exec_list);
int exec_module_list_cmp(const void *p1, const void *p2, void *opaque);
int js_execute_async_module(JSContext *ctx, JSModuleDef *m);
int js_execute_sync_module(JSContext *ctx, JSModuleDef *m, JSValue *pvalue);
int js_inner_module_evaluation(JSContext *ctx, JSModuleDef *m, int index, JSModuleDef **pstack_top, JSValue *pvalue);
JSValue js_evaluate_module(JSContext *ctx, JSModuleDef *m);
int js_parse_with_clause(JSParseState *s, JSReqModuleEntry *rme);
int js_parse_from_clause(JSParseState *s, JSModuleDef *m);
int js_parse_export(JSParseState *s);
int add_import(JSParseState *s, JSModuleDef *m, JSAtom local_name, JSAtom import_name, BOOL is_star);
int js_parse_import(JSParseState *s);
int js_parse_source_element(JSParseState *s);

void js_object_list_init(JSObjectList *s);
uint32_t js_object_list_get_hash(JSObject *p, uint32_t hash_size);
int js_object_list_resize_hash(JSContext *ctx, JSObjectList *s, uint32_t new_hash_size);
int js_object_list_add(JSContext *ctx, JSObjectList *s, JSObject *obj);
int js_object_list_find(JSContext *ctx, JSObjectList *s, JSObject *obj);
void js_object_list_end(JSContext *ctx, JSObjectList *s);

void bc_put_u8(BCWriterState *s, uint8_t v);
void bc_put_u16(BCWriterState *s, uint16_t v);
void bc_put_u32(BCWriterState *s, uint32_t v);
void bc_put_u64(BCWriterState *s, uint64_t v);
void bc_put_leb128(BCWriterState *s, uint32_t v);
void bc_put_sleb128(BCWriterState *s, int32_t v);
void bc_set_flags(uint32_t *pflags, int *pidx, uint32_t val, int n);
int bc_atom_to_idx(BCWriterState *s, uint32_t *pres, JSAtom atom);
int bc_put_atom(BCWriterState *s, JSAtom atom);
void bc_byte_swap(uint8_t *bc_buf, int bc_len);
int JS_WriteFunctionBytecode(BCWriterState *s, const uint8_t *bc_buf1, int bc_len);
void JS_WriteString(BCWriterState *s, JSString *p);
int JS_WriteBigInt(BCWriterState *s, JSValueConst obj);
int JS_WriteObjectRec(BCWriterState *s, JSValueConst obj);
int JS_WriteFunctionTag(BCWriterState *s, JSValueConst obj);
int JS_WriteModule(BCWriterState *s, JSValueConst obj);
int JS_WriteArray(BCWriterState *s, JSValueConst obj);
int JS_WriteObjectTag(BCWriterState *s, JSValueConst obj);
int JS_WriteTypedArray(BCWriterState *s, JSValueConst obj);
int JS_WriteArrayBuffer(BCWriterState *s, JSValueConst obj);
int JS_WriteSharedArrayBuffer(BCWriterState *s, JSValueConst obj);
int JS_WriteObjectAtoms(BCWriterState *s);

int bc_read_error_end(BCReaderState *s);
int bc_get_u8(BCReaderState *s, uint8_t *pval);
int bc_get_u16(BCReaderState *s, uint16_t *pval);
int bc_get_u32(BCReaderState *s, uint32_t *pval);
int bc_get_u64(BCReaderState *s, uint64_t *pval);
int bc_get_leb128(BCReaderState *s, uint32_t *pval);
int bc_get_sleb128(BCReaderState *s, int32_t *pval);
int bc_get_leb128_int(BCReaderState *s, int *pval);
int bc_get_leb128_u16(BCReaderState *s, uint16_t *pval);
int bc_get_buf(BCReaderState *s, uint8_t *buf, uint32_t buf_len);
int bc_idx_to_atom(BCReaderState *s, JSAtom *patom, uint32_t idx);
int bc_get_atom(BCReaderState *s, JSAtom *patom);
JSString *JS_ReadString(BCReaderState *s);
uint32_t bc_get_flags(uint32_t flags, int *pidx, int n);
int JS_ReadFunctionBytecode(BCReaderState *s, JSFunctionBytecode *b, int byte_code_offset, uint32_t bc_len);
JSValue JS_ReadBigInt(BCReaderState *s);
JSValue JS_ReadObjectRec(BCReaderState *s);
int BC_add_object_ref1(BCReaderState *s, JSObject *p);
int BC_add_object_ref(BCReaderState *s, JSValueConst obj);
JSValue JS_ReadFunctionTag(BCReaderState *s);
JSValue JS_ReadModule(BCReaderState *s);
JSValue JS_ReadObjectTag(BCReaderState *s);
JSValue JS_ReadArray(BCReaderState *s, int tag);
JSValue JS_ReadTypedArray(BCReaderState *s);
JSValue JS_ReadArrayBuffer(BCReaderState *s);
JSValue JS_ReadSharedArrayBuffer(BCReaderState *s);
JSValue JS_ReadDate(BCReaderState *s);
JSValue JS_ReadObjectValue(BCReaderState *s);
int JS_ReadObjectAtoms(BCReaderState *s);
void bc_reader_free(BCReaderState *s);

void js_array_buffer_free(JSRuntime *rt, void *opaque, void *ptr);
JSArrayBuffer *js_get_array_buffer(JSContext *ctx, JSValueConst obj);
BOOL array_buffer_is_resizable(const JSArrayBuffer *abuf);
JSValue js_typed_array_constructor(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int classid);
JSValue js_typed_array_constructor_ta(JSContext *ctx, JSValueConst new_target, JSValueConst src_obj, int classid, uint32_t len);
BOOL typed_array_is_oob(JSObject *p);
int js_typed_array_get_length_unsafe(JSContext *ctx, JSValueConst obj);
JSValue JS_ThrowTypeErrorDetachedArrayBuffer(JSContext *ctx);
JSValue JS_ThrowTypeErrorArrayBufferOOB(JSContext *ctx);
JSValue js_array_buffer_constructor3(JSContext *ctx, JSValueConst new_target, uint64_t len, uint64_t *max_len, JSClassID class_id, uint8_t *buf, JSFreeArrayBufferDataFunc *free_func, void *opaque, BOOL alloc_flag);
JSValue js_array_buffer_constructor2(JSContext *ctx, JSValueConst new_target, uint64_t len, uint64_t *max_len, JSClassID class_id);
JSValue js_array_buffer_constructor1(JSContext *ctx, JSValueConst new_target, uint64_t len, uint64_t *max_len);
JSValue js_array_buffer_constructor0(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv, JSClassID class_id);
JSValue js_array_buffer_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv);
JSValue js_shared_array_buffer_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv);
void js_array_buffer_finalizer(JSRuntime *rt, JSValue val);
JSValue js_array_buffer_isView(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_array_buffer_get_detached(JSContext *ctx, JSValueConst this_val);
JSValue js_array_buffer_get_byteLength(JSContext *ctx, JSValueConst this_val, int class_id);
JSValue js_array_buffer_get_maxByteLength(JSContext *ctx, JSValueConst this_val, int class_id);
JSValue js_array_buffer_get_resizable(JSContext *ctx, JSValueConst this_val, int class_id);
void js_array_buffer_update_typed_arrays(JSArrayBuffer *abuf);
JSValue js_array_buffer_transfer(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int transfer_to_fixed_length);
JSValue js_array_buffer_resize(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int class_id);
JSValue js_array_buffer_slice(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int class_id);

JSObject *get_typed_array(JSContext *ctx, JSValueConst this_val);
JSValue js_typed_array_get_length(JSContext *ctx, JSValueConst this_val);
JSValue js_typed_array_get_buffer(JSContext *ctx, JSValueConst this_val);
JSValue js_typed_array_get_byteLength(JSContext *ctx, JSValueConst this_val);
JSValue js_typed_array_get_byteOffset(JSContext *ctx, JSValueConst this_val);
JSValue js_typed_array_get_toStringTag(JSContext *ctx, JSValueConst this_val);
JSValue js_typed_array_set_internal(JSContext *ctx, JSValueConst dst, JSValueConst src, JSValueConst off);
JSValue js_typed_array_at(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_typed_array_with(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_typed_array_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_create_typed_array_iterator(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic);
JSValue js_typed_array_create(JSContext *ctx, JSValueConst ctor, int argc, JSValueConst *argv);
JSValue js_typed_array___speciesCreate(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_typed_array_from(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_typed_array_of(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_typed_array_copyWithin(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_typed_array_fill(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_typed_array_find(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int mode);
JSValue js_typed_array_indexOf(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int special);
JSValue js_typed_array_join(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int toLocaleString);
JSValue js_typed_array_reverse(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_typed_array_toReversed(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
void slice_memcpy(uint8_t *dst, const uint8_t *src, size_t len);
JSValue js_typed_array_slice(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_typed_array_subarray(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
int js_cmp_doubles(double x, double y);
int js_TA_cmp_int8(const void *a, const void *b, void *opaque);
int js_TA_cmp_uint8(const void *a, const void *b, void *opaque);
int js_TA_cmp_int16(const void *a, const void *b, void *opaque);
int js_TA_cmp_uint16(const void *a, const void *b, void *opaque);
int js_TA_cmp_int32(const void *a, const void *b, void *opaque);
int js_TA_cmp_uint32(const void *a, const void *b, void *opaque);
int js_TA_cmp_int64(const void *a, const void *b, void *opaque);
int js_TA_cmp_uint64(const void *a, const void *b, void *opaque);
int js_TA_cmp_float16(const void *a, const void *b, void *opaque);
int js_TA_cmp_float32(const void *a, const void *b, void *opaque);
int js_TA_cmp_float64(const void *a, const void *b, void *opaque);
JSValue js_TA_get_int8(JSContext *ctx, const void *a);
JSValue js_TA_get_uint8(JSContext *ctx, const void *a);
JSValue js_TA_get_int16(JSContext *ctx, const void *a);
JSValue js_TA_get_uint16(JSContext *ctx, const void *a);
JSValue js_TA_get_int32(JSContext *ctx, const void *a);
JSValue js_TA_get_uint32(JSContext *ctx, const void *a);
JSValue js_TA_get_int64(JSContext *ctx, const void *a);
JSValue js_TA_get_uint64(JSContext *ctx, const void *a);
JSValue js_TA_get_float16(JSContext *ctx, const void *a);
JSValue js_TA_get_float32(JSContext *ctx, const void *a);
JSValue js_TA_get_float64(JSContext *ctx, const void *a);
int js_TA_cmp_generic(const void *a, const void *b, void *opaque);
JSValue js_typed_array_sort(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_typed_array_toSorted(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

JSValue js_uint8array_to_base64(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_uint8array_to_hex(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_uint8array_from_base64(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_uint8array_from_hex(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_make_read_written(JSContext *ctx, size_t read, size_t written);
JSValue js_uint8array_set_from_base64(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_uint8array_set_from_hex(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

JSObject *get_dataview(JSContext *ctx, JSValueConst this_val);
JSValue js_dataview_get_buffer(JSContext *ctx, JSValueConst this_val);
JSValue js_dataview_get_byteLength(JSContext *ctx, JSValueConst this_val);
JSValue js_dataview_get_byteOffset(JSContext *ctx, JSValueConst this_val);
JSValue js_dataview_getValue(JSContext *ctx, JSValueConst this_obj, int argc, JSValueConst *argv, int class_id);
JSValue js_dataview_setValue(JSContext *ctx, JSValueConst this_obj, int argc, JSValueConst *argv, int class_id);

int validate_typed_array(JSContext *ctx, JSValueConst this_val);
JSObject *check_uint8array(JSContext *ctx, JSValueConst this_val);
int get_uint8array_bytes(JSContext *ctx, JSObject *p, uint8_t **pdata, size_t *plen);
int check_options_object(JSContext *ctx, JSValueConst options);
int parse_alphabet_option(JSContext *ctx, JSValueConst options);
int parse_last_chunk_option(JSContext *ctx, JSValueConst options);

void js_dataview_finalizer(JSRuntime *rt, JSValue val);
void js_dataview_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);

JSObject *js_atomics_get_buf(JSContext *ctx, JSValueConst obj, JSValueConst idx_val, uint64_t *pidx, int is_waitable);
JSValue js_atomics_op(JSContext *ctx, JSValueConst this_obj, int argc, JSValueConst *argv, int op);
JSValue js_atomics_store(JSContext *ctx, JSValueConst this_obj, int argc, JSValueConst *argv);
JSValue js_atomics_isLockFree(JSContext *ctx, JSValueConst this_obj, int argc, JSValueConst *argv);
JSValue js_atomics_pause(JSContext *ctx, JSValueConst this_obj, int argc, JSValueConst *argv);
JSValue js_atomics_wait(JSContext *ctx, JSValueConst this_obj, int argc, JSValueConst *argv);
JSValue js_atomics_notify(JSContext *ctx, JSValueConst this_obj, int argc, JSValueConst *argv);

void js_weakref_finalizer(JSRuntime *rt, JSValue val);
void weakref_delete_weakref(JSRuntime *rt, JSWeakRefHeader *wh);
JSValue js_weakref_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv);
JSValue js_weakref_deref(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

void js_finrec_finalizer(JSRuntime *rt, JSValue val);
void js_finrec_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
JSValue js_finrec_job(JSContext *ctx, int argc, JSValueConst *argv);
void finrec_delete_weakref(JSRuntime *rt, JSWeakRefHeader *wh);
JSValue js_finrec_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv);
JSValue js_finrec_register(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_finrec_unregister(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

void js_map_finalizer(JSRuntime *rt, JSValue val);
void js_map_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
void js_map_iterator_finalizer(JSRuntime *rt, JSValue val);
void js_map_iterator_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
void js_array_iterator_finalizer(JSRuntime *rt, JSValue val);
void js_array_iterator_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
void js_iterator_concat_finalizer(JSRuntime *rt, JSValue val);
void js_iterator_concat_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
void js_iterator_helper_finalizer(JSRuntime *rt, JSValue val);
void js_iterator_helper_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
void js_iterator_wrap_finalizer(JSRuntime *rt, JSValue val);
void js_iterator_wrap_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
void js_regexp_string_iterator_finalizer(JSRuntime *rt, JSValue val);
void js_regexp_string_iterator_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
void js_generator_finalizer(JSRuntime *rt, JSValue obj);
void js_generator_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
void js_global_object_finalizer(JSRuntime *rt, JSValue obj);
void js_global_object_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
void js_promise_finalizer(JSRuntime *rt, JSValue val);
void js_promise_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
void js_promise_resolve_function_finalizer(JSRuntime *rt, JSValue val);
void js_promise_resolve_function_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);

JSValue JS_ToPrimitiveFree(JSContext *ctx, JSValue val, int hint);
JSValue JS_ToPrimitive(JSContext *ctx, JSValueConst val, int hint);
// void JS_SetIsHTMLDDA(JSContext *ctx, JSValueConst obj);
BOOL JS_IsHTMLDDA(JSContext *ctx, JSValueConst obj);
int JS_ToBoolFree(JSContext *ctx, JSValue val);
int skip_spaces(const char *pc);
int to_digit(int c);
JSBigInt *js_bigint_new(JSContext *ctx, int len);
JSBigInt *js_bigint_set_si(JSBigIntBuf *buf, js_slimb_t a);
JSBigInt *js_bigint_set_si64(JSBigIntBuf *buf, int64_t a);
JSBigInt *js_bigint_set_short(JSBigIntBuf *buf, JSValueConst val);
void js_bigint_dump1(JSContext *ctx, const char *str, const js_limb_t *tab, int len);
void js_bigint_dump(JSContext *ctx, const char *str, const JSBigInt *p);
JSBigInt *js_bigint_new_si(JSContext *ctx, js_slimb_t a);
JSBigInt *js_bigint_new_si64(JSContext *ctx, int64_t a);
JSBigInt *js_bigint_new_ui64(JSContext *ctx, uint64_t a);
JSBigInt *js_bigint_new_di(JSContext *ctx, js_sdlimb_t a);
JSBigInt *js_bigint_normalize1(JSContext *ctx, JSBigInt *a, int l);
JSBigInt *js_bigint_normalize(JSContext *ctx, JSBigInt *a);
int js_bigint_sign(const JSBigInt *a);
js_slimb_t js_bigint_get_si_sat(const JSBigInt *a);
JSBigInt *js_bigint_extend(JSContext *ctx, JSBigInt *r, js_limb_t op1);
JSBigInt *js_bigint_add(JSContext *ctx, const JSBigInt *a, const JSBigInt *b, int b_neg);
JSBigInt *js_bigint_neg(JSContext *ctx, const JSBigInt *a);
JSBigInt *js_bigint_mul(JSContext *ctx, const JSBigInt *a, const JSBigInt *b);
JSBigInt *js_bigint_divrem(JSContext *ctx, const JSBigInt *a, const JSBigInt *b, BOOL is_rem);
JSBigInt *js_bigint_logic(JSContext *ctx, const JSBigInt *a, const JSBigInt *b, OPCodeEnum op);
JSBigInt *js_bigint_not(JSContext *ctx, const JSBigInt *a);
JSBigInt *js_bigint_shl(JSContext *ctx, const JSBigInt *a, unsigned int shift1);
JSBigInt *js_bigint_shr(JSContext *ctx, const JSBigInt *a, unsigned int shift1);
JSBigInt *js_bigint_pow(JSContext *ctx, const JSBigInt *a, JSBigInt *b);
uint64_t js_bigint_get_mant_exp(JSContext *ctx, int *pexp, const JSBigInt *a);
uint64_t shr_rndn(uint64_t a, int n);
double js_bigint_to_float64(JSContext *ctx, const JSBigInt *a);
JSBigInt *js_bigint_from_float64(JSContext *ctx, int *pres, double a1);
int js_bigint_float64_cmp(JSContext *ctx, const JSBigInt *a, double b);
int js_bigint_cmp(JSContext *ctx, const JSBigInt *a, const JSBigInt *b);
JSBigInt *js_bigint_from_string(JSContext *ctx, const char *str, int radix);
char *js_u64toa(char *q, int64_t n, unsigned int base);
char *limb_to_a(char *q, js_limb_t n, unsigned int radix, int len);
JSValue js_bigint_to_string1(JSContext *ctx, JSValueConst val, int radix);
JSValue JS_CompactBigInt(JSContext *ctx, JSBigInt *p);
JSValue js_atof(JSContext *ctx, const char *str, const char **pp, int radix, int flags);
JSValue JS_ToNumberHintFree(JSContext *ctx, JSValue val, JSToNumberHintEnum flag);
JSValue JS_ToNumberFree(JSContext *ctx, JSValue val);
JSValue JS_ToNumericFree(JSContext *ctx, JSValue val);
JSValue JS_ToNumeric(JSContext *ctx, JSValueConst val);
int __JS_ToFloat64Free(JSContext *ctx, double *pres, JSValue val);
int JS_ToFloat64Free(JSContext *ctx, double *pres, JSValue val);
JSValue JS_ToIntegerFree(JSContext *ctx, JSValue val);
int JS_ToInt32SatFree(JSContext *ctx, int *pres, JSValue val);
int JS_ToInt64SatFree(JSContext *ctx, int64_t *pres, JSValue val);
int JS_ToInt64Free(JSContext *ctx, int64_t *pres, JSValue val);
int JS_ToInt32Free(JSContext *ctx, int32_t *pres, JSValue val);
int JS_ToUint8ClampFree(JSContext *ctx, int32_t *pres, JSValue val);
int JS_ToArrayLengthFree(JSContext *ctx, uint32_t *plen, JSValue val, BOOL is_array_ctor);
int JS_ToLengthFree(JSContext *ctx, int64_t *plen, JSValue val);
int JS_NumberIsInteger(JSContext *ctx, JSValueConst val);
BOOL JS_NumberIsNegativeOrMinusZero(JSContext *ctx, JSValueConst val);
JSValue js_bigint_to_string(JSContext *ctx, JSValueConst val);
JSValue js_dtoa2(JSContext *ctx, double d, int radix, int n_digits, int flags);
JSValue JS_ToStringInternal(JSContext *ctx, JSValueConst val, BOOL is_ToPropertyKey);
JSValue JS_ToStringFree(JSContext *ctx, JSValue val);
JSValue JS_ToLocaleStringFree(JSContext *ctx, JSValue val);
JSValue JS_ToStringCheckObject(JSContext *ctx, JSValueConst val);
/* 第五批：内置对象核心函数 */

/* Function 相关 */
JSValue js_function_apply(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic);
JSValue js_function_call(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_function_bind(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_function_toString(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_function_hasInstance(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_function_proto(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_function_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv, int magic);
int js_get_length32(JSContext *ctx, uint32_t *pres, JSValueConst obj);
int js_get_length64(JSContext *ctx, int64_t *pres, JSValueConst obj);
void free_arg_list(JSContext *ctx, JSValue *tab, uint32_t len);
JSValue *build_arg_list(JSContext *ctx, uint32_t *plen, JSValueConst array_arg);

/* Array 相关 */
JSValue js_array_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv);
JSValue js_array_from(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_array_of(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_array_isArray(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_get_this(JSContext *ctx, JSValueConst this_val);
JSValue JS_ArraySpeciesGetCtor(JSContext *ctx, JSValueConst obj);
JSValue JS_ArrayCreateFromCtor(JSContext *ctx, JSValueConst ctor, int64_t len);
JSValue JS_ArraySpeciesCreate(JSContext *ctx, JSValueConst obj, int64_t len);
int JS_isConcatSpreadable(JSContext *ctx, JSValueConst obj);
JSValue js_array_at(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_array_with(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_array_concat(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_array_every(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int special);
JSValue js_array_reduce(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int special);
JSValue js_array_fill(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_array_includes(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_array_indexOf(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_array_lastIndexOf(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_array_find(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int mode);
JSValue js_array_toString(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_array_join(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int toLocaleString);
JSValue js_array_pop(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int shift);
JSValue js_array_push(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int unshift);
JSValue js_array_reverse(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_array_toReversed(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_array_slice(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_array_splice(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_array_toSpliced(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_array_copyWithin(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
int64_t JS_FlattenIntoArray(JSContext *ctx, JSValueConst target, JSValueConst source, int64_t sourceLen, int64_t targetIndex, int depth, JSValueConst mapperFunction, JSValueConst thisArg);
JSValue js_array_flatten(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int map);
JSValue js_array_sort(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_array_toSorted(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
void js_array_iterator_finalizer(JSRuntime *rt, JSValue val);
void js_array_iterator_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
JSValue js_create_array_iterator(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic);
JSValue js_array_iterator_next(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, BOOL *pdone, int magic);
int js_array_cmp_generic(const void *a, const void *b, void *opaque);

/* String 相关 */
JSValue js_string_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv);
JSValue js_thisStringValue(JSContext *ctx, JSValueConst this_val);
JSValue js_string_fromCharCode(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_string_fromCodePoint(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_string_raw(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
// JSValue js_string_codePointRange(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_string_charCodeAt(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_string_charAt(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int is_at);
JSValue js_string_codePointAt(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_string_concat(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
int string_cmp(JSString *p1, JSString *p2, int x1, int x2, int len);
int string_indexof_char(JSString *p, int c, int from);
int string_indexof(JSString *p1, JSString *p2, int from);
int64_t string_advance_index(JSString *p, int64_t index, BOOL unicode);
int js_string_find_invalid_codepoint(JSString *p);
JSValue js_string_isWellFormed(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_string_toWellFormed(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_string_indexOf(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int lastIndexOf);
int js_is_regexp(JSContext *ctx, JSValueConst obj);
JSValue js_string_includes(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic);
int check_regexp_g_flag(JSContext *ctx, JSValueConst regexp);
JSValue js_string_match(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int atom);
int js_string_GetSubstitution(JSContext *ctx, StringBuffer *b, JSValueConst matched, JSString *sp, uint32_t position, JSValueConst captures_val, JSValueConst namedCaptures, JSValueConst rep, uint8_t **captures, uint32_t captures_len);
JSValue js_string_replace(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int is_replaceAll);
JSValue js_string_split(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_string_substring(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_string_substr(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_string_slice(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_string_pad(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int padEnd);
JSValue js_string_repeat(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_string_trim(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic);
int string_prevc(JSString *p, int *pidx);
BOOL test_final_sigma(JSString *p, int sigma_pos);
JSValue js_string_toLowerCase(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int to_lower);
#ifdef CONFIG_ALL_UNICODE
int JS_ToUTF32String(JSContext *ctx, uint32_t **pbuf, JSValueConst val1);
JSValue JS_NewUTF32String(JSContext *ctx, const uint32_t *buf, int len);
int js_string_normalize1(JSContext *ctx, uint32_t **pout_buf, JSValueConst val, UnicodeNormalizationEnum n_type);
JSValue js_string_normalize(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
int js_UTF32_compare(const uint32_t *buf1, int buf1_len, const uint32_t *buf2, int buf2_len);
#endif
JSValue js_string_localeCompare(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_string_toString(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_string_iterator_next(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, BOOL *pdone, int magic);
JSValue js_string_CreateHTML(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic);

/* Number 相关 */
JSValue js_number_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv);
JSValue js_thisNumberValue(JSContext *ctx, JSValueConst this_val);
JSValue js_number_valueOf(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
int js_get_radix(JSContext *ctx, JSValueConst val);
JSValue js_number_toString(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic);
JSValue js_number_toFixed(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_number_toExponential(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_number_toPrecision(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_number_isNaN(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_number_isFinite(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_number_isInteger(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_number_isSafeInteger(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

/* Boolean 相关 */
JSValue js_boolean_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv);
JSValue js_thisBooleanValue(JSContext *ctx, JSValueConst this_val);
JSValue js_boolean_toString(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_boolean_valueOf(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

/* Math 相关 */
JSValue js_math_min_max(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic);
double js_math_sign(double a);
double js_math_round(double a);
JSValue js_math_hypot(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
double js_math_f16round(double a);
double js_math_fround(double a);
JSValue js_math_imul(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_math_clz32(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
void sum_precise_init(SumPreciseState *s);
void sum_precise_renorm(SumPreciseState *s);
void sum_precise_add(SumPreciseState *s, double d);
double sum_precise_get_result(SumPreciseState *s);
JSValue js_math_sumPrecise(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
uint64_t xorshift64star(uint64_t *pstate);
void js_random_init(JSContext *ctx);
JSValue js_math_random(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

/* Date 相关 */
int64_t math_mod(int64_t a, int64_t b);
int64_t floor_div(int64_t a, int64_t b);
JSValue js_Date_parse(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
int JS_ThisTimeValue(JSContext *ctx, double *valp, JSValueConst this_val);
JSValue JS_SetThisTimeValue(JSContext *ctx, JSValueConst this_val, double v);
int64_t days_from_year(int64_t y);
int64_t days_in_year(int64_t y);
int64_t year_from_days(int64_t *days);
int get_date_fields(JSContext *ctx, JSValueConst obj, double fields[minimum_length(9)], int is_local, int force);
double time_clip(double t);
double set_date_fields(double fields[minimum_length(7)], int is_local);
double set_date_fields_checked(double fields[minimum_length(7)], int is_local);
JSValue get_date_field(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic);
JSValue set_date_field(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic);
JSValue get_date_string(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic);
int64_t date_now(void);
JSValue js_date_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv);
JSValue js_Date_UTC(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
BOOL string_skip_char(const uint8_t *sp, int *pp, int c);
int string_skip_spaces(const uint8_t *sp, int *pp);
int string_skip_separators(const uint8_t *sp, int *pp);
int string_skip_until(const uint8_t *sp, int *pp, const char *stoplist);
BOOL string_get_digits(const uint8_t *sp, int *pp, int *pval, int min_digits, int max_digits);
BOOL string_get_milliseconds(const uint8_t *sp, int *pp, int *pval);
uint8_t upper_ascii(uint8_t c);
BOOL string_get_tzoffset(const uint8_t *sp, int *pp, int *tzp, BOOL strict);
BOOL string_match(const uint8_t *sp, int *pp, const char *s);
int find_abbrev(const uint8_t *sp, int p, const char *list, int count);
BOOL string_get_month(const uint8_t *sp, int *pp, int *pval);
BOOL js_date_parse_isostring(const uint8_t *sp, int fields[9], BOOL *is_local);
BOOL string_get_tzabbr(const uint8_t *sp, int *pp, int *offset);
BOOL js_date_parse_otherstring(const uint8_t *sp, int fields[minimum_length(9)], BOOL *is_local);
JSValue js_Date_parse(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_Date_now(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_date_Symbol_toPrimitive(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_date_getTimezoneOffset(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_date_getTime(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_date_setTime(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_date_setYear(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_date_toJSON(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

/* RegExp 相关 */
JSValue js_compile_regexp(JSContext *ctx, JSValueConst pattern, JSValueConst flags);
JSValue JS_NewRegexp(JSContext *ctx, JSValue pattern, JSValue bc);
JSRegExp *js_get_regexp(JSContext *ctx, JSValueConst obj, BOOL throw_error);
int js_is_regexp(JSContext *ctx, JSValueConst obj);
JSValue js_regexp_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv);
JSValue js_regexp_compile(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_regexp_get_source(JSContext *ctx, JSValueConst this_val);
JSValue js_regexp_get_flag(JSContext *ctx, JSValueConst this_val, int mask);
JSValue js_regexp_get_flags(JSContext *ctx, JSValueConst this_val);
JSValue js_regexp_toString(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_regexp_escape(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
int js_regexp_get_lastIndex(JSContext *ctx, int64_t *plast_index, JSValueConst this_val);
int js_regexp_set_lastIndex(JSContext *ctx, JSValueConst this_val, int last_index);
JSValue js_regexp_exec(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_regexp_replace(JSContext *ctx, JSValueConst this_val, JSValueConst arg, JSValueConst rep_val);
JSValue JS_RegExpExec(JSContext *ctx, JSValueConst r, JSValueConst s);
JSValue js_regexp_test(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_regexp_Symbol_match(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_regexp_Symbol_matchAll(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
BOOL check_regexp_getter(JSContext *ctx, JSObject *p, JSAtom atom, JSCFunction *func, int magic);
BOOL js_is_standard_regexp(JSContext *ctx, JSValueConst obj);
JSValue js_regexp_Symbol_replace(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_regexp_Symbol_search(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_regexp_Symbol_split(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
void js_regexp_finalizer(JSRuntime *rt, JSValue val);
void js_regexp_string_iterator_finalizer(JSRuntime *rt, JSValue val);
void js_regexp_string_iterator_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
JSValue js_regexp_string_iterator_next(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, BOOL *pdone, int magic);

/* Error 相关 */
JSValue js_error_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv, int magic);
JSValue js_error_toString(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_error_isError(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_aggregate_error_constructor(JSContext *ctx, JSValueConst errors);

/* JSON 相关 */
int json_parse_expect(JSParseState *s, int tok);
void json_parse_record_init_obj(JSContext *ctx, JSONParseRecord *pr, JSValueConst val);
void json_parse_record_init_array(JSContext *ctx, JSONParseRecord *pr, JSValueConst val);
void json_parse_record_init_primitive(JSContext *ctx, JSONParseRecord *pr, JSValueConst val, uint32_t source_pos, uint32_t source_len);
int json_parse_record_resize_hash(JSContext *ctx, JSONParseRecordObject *po, uint32_t new_hash_size);
JSONParseRecord *json_parse_record_add(JSContext *ctx, JSONParseRecord *pr, JSAtom key, int *psize);
JSONParseRecord *json_parse_record_find(JSONParseRecord *pr, JSAtom key);
void json_free_parse_record(JSContext *ctx, JSONParseRecord *pr);
JSValue json_parse_value(JSParseState *s, JSONParseRecord *pr);
JSValue internalize_json_property(JSContext *ctx, JSValueConst holder, JSAtom name, JSValueConst reviver, const char *text_str, JSONParseRecord *pr);
JSValue js_json_parse(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_json_isRawJSON(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
BOOL is_valid_raw_json_char(int c);
JSValue js_json_rawJSON(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
int JS_ToQuotedString(JSContext *ctx, StringBuffer *b, JSValueConst val1);
int JS_ToQuotedStringFree(JSContext *ctx, StringBuffer *b, JSValue val);
JSValue js_json_check(JSContext *ctx, JSONStringifyContext *jsc, JSValueConst holder, JSValue val, JSValueConst key);
int js_json_to_str(JSContext *ctx, JSONStringifyContext *jsc, JSValueConst holder, JSValue val, JSValueConst indent);
// JSValue JS_JSONStringify(JSContext *ctx, JSValueConst obj, JSValueConst replacer, JSValueConst space0);
JSValue js_json_stringify(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

/* BigInt 相关 */
JSValue JS_ToBigIntCtorFree(JSContext *ctx, JSValue val);
JSValue js_bigint_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv);
JSValue js_thisBigIntValue(JSContext *ctx, JSValueConst this_val);
JSValue js_bigint_toString(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_bigint_valueOf(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_bigint_asUintN(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int asIntN);

/* Symbol 相关 */
JSValue js_symbol_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv);
JSValue js_thisSymbolValue(JSContext *ctx, JSValueConst this_val);
JSValue js_symbol_toString(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_symbol_valueOf(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_symbol_get_description(JSContext *ctx, JSValueConst this_val);
JSValue js_symbol_for(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_symbol_keyFor(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

/* Reflect 相关 */
JSValue js_reflect_apply(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_reflect_construct(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_reflect_deleteProperty(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_reflect_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_reflect_has(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_reflect_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_reflect_setPrototypeOf(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_reflect_ownKeys(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

/* Proxy 相关 */
JSProxyData *get_proxy_method(JSContext *ctx, JSValue *pmethod, JSValueConst obj, JSAtom name);
JSValue js_proxy_get_prototype(JSContext *ctx, JSValueConst obj);
int js_proxy_set_prototype(JSContext *ctx, JSValueConst obj, JSValueConst proto_val);
int js_proxy_is_extensible(JSContext *ctx, JSValueConst obj);
int js_proxy_prevent_extensions(JSContext *ctx, JSValueConst obj);
int js_proxy_has(JSContext *ctx, JSValueConst obj, JSAtom atom);
JSValue js_proxy_get(JSContext *ctx, JSValueConst obj, JSAtom atom, JSValueConst receiver);
int js_proxy_set(JSContext *ctx, JSValueConst obj, JSAtom atom, JSValueConst value, JSValueConst receiver, int flags);
JSValue js_create_desc(JSContext *ctx, JSValueConst val, JSValueConst getter, JSValueConst setter, int flags);
int js_proxy_get_own_property(JSContext *ctx, JSPropertyDescriptor *pdesc, JSValueConst obj, JSAtom prop);
int js_proxy_define_own_property(JSContext *ctx, JSValueConst obj, JSAtom prop, JSValueConst val, JSValueConst getter, JSValueConst setter, int flags);
int js_proxy_delete_property(JSContext *ctx, JSValueConst obj, JSAtom atom);
int find_prop_key(const JSPropertyEnum *tab, int n, JSAtom atom);
int js_proxy_get_own_property_names(JSContext *ctx, JSPropertyEnum **ptab, uint32_t *plen, JSValueConst obj);
JSValue js_proxy_call_constructor(JSContext *ctx, JSValueConst func_obj, JSValueConst new_target, int argc, JSValueConst *argv);
JSValue js_proxy_call(JSContext *ctx, JSValueConst func_obj, JSValueConst this_obj, int argc, JSValueConst *argv, int flags);
JSValue js_proxy_constructor(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_proxy_revoke(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic, JSValue *func_data);
JSValue js_proxy_revoke_constructor(JSContext *ctx, JSValueConst proxy_obj);
JSValue js_proxy_revocable(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

/* Promise 相关 */
int js_create_resolving_functions(JSContext *ctx, JSValue *args, JSValueConst promise);
void promise_reaction_data_free(JSRuntime *rt, JSPromiseReactionData *rd);
JSValue promise_reaction_job(JSContext *ctx, int argc, JSValueConst *argv);
void fulfill_or_reject_promise(JSContext *ctx, JSValueConst promise, JSValueConst value, BOOL is_reject);
void reject_promise(JSContext *ctx, JSValueConst promise, JSValueConst value);
JSValue js_promise_resolve_thenable_job(JSContext *ctx, int argc, JSValueConst *argv);
void js_promise_resolve_function_free_resolved(JSRuntime *rt, JSPromiseFunctionDataResolved *sr);
int js_create_resolving_functions(JSContext *ctx, JSValue *resolving_funcs, JSValueConst promise);
void js_promise_resolve_function_finalizer(JSRuntime *rt, JSValue val);
void js_promise_resolve_function_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
JSValue js_promise_resolve_function_call(JSContext *ctx, JSValueConst func_obj, JSValueConst this_val, int argc, JSValueConst *argv, int flags);
void js_promise_finalizer(JSRuntime *rt, JSValue val);
void js_promise_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
JSValue js_promise_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv);
JSValue js_promise_executor(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic, JSValue *func_data);
JSValue js_promise_executor_new(JSContext *ctx);
JSValue js_new_promise_capability(JSContext *ctx, JSValue *resolving_funcs, JSValueConst ctor);
// JSValue JS_NewPromiseCapability(JSContext *ctx, JSValue *resolving_funcs);
JSValue js_promise_resolve(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic);
JSValue js_promise_withResolvers(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_promise_try(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
int remainingElementsCount_add(JSContext *ctx, JSValueConst resolve_element_env, int addend);
JSValue js_promise_all_resolve_element(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic, JSValue *func_data);
JSValue js_promise_all(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic);
JSValue js_promise_race(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
int perform_promise_then(JSContext *ctx, JSValueConst promise, JSValueConst *resolve_reject, JSValueConst *cap_resolving_funcs);
JSValue js_promise_then(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_promise_catch(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_promise_finally_value_thunk(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic, JSValue *func_data);
JSValue js_promise_finally_thrower(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic, JSValue *func_data);
JSValue js_promise_then_finally_func(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic, JSValue *func_data);
JSValue js_promise_finally(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

/* Generator/Async 相关 */
void js_async_from_sync_iterator_finalizer(JSRuntime *rt, JSValue val);
void js_async_from_sync_iterator_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
JSValue JS_CreateAsyncFromSyncIterator(JSContext *ctx, JSValueConst sync_iter);
JSValue js_async_from_sync_iterator_unwrap(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic, JSValue *func_data);
JSValue js_async_from_sync_iterator_unwrap_func_create(JSContext *ctx, BOOL done);
JSValue js_async_from_sync_iterator_close_wrap(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic, JSValue *func_data);
JSValue js_async_from_sync_iterator_close_wrap_func_create(JSContext *ctx, JSValueConst sync_iter);
JSValue js_async_from_sync_iterator_next(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic);

/* 全局函数 */
JSValue js_global_eval(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_global_isNaN(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_global_isFinite(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_parseInt(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_parseFloat(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_global_decodeURI(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int isComponent);
JSValue js_global_encodeURI(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int isComponent);
JSValue js_global_escape(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_global_unescape(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
int isURIReserved(int c);
int js_throw_URIError(JSContext *ctx, const char *fmt, ...);
int hex_decode(JSContext *ctx, JSString *p, int k);
int isUnescaped(int c);
int isURIUnescaped(int c, int isComponent);
int encodeURI_hex(StringBuffer *b, int c);
/*
static BOOL js_strict_eq2(JSContext *ctx, JSValueConst op1, JSValueConst op2,
                          JSStrictEqModeEnum eq_mode);
static BOOL js_strict_eq(JSContext *ctx, JSValueConst op1, JSValueConst op2);
static BOOL js_same_value(JSContext *ctx, JSValueConst op1, JSValueConst op2);
static BOOL js_same_value_zero(JSContext *ctx, JSValueConst op1, JSValueConst op2);
static JSValue JS_ToObject(JSContext *ctx, JSValueConst val);
static JSValue JS_ToObjectFree(JSContext *ctx, JSValue val);
static JSProperty *add_property(JSContext *ctx,
                                JSObject *p, JSAtom prop, int prop_flags);
static void free_property(JSRuntime *rt, JSProperty *pr, int prop_flags);
static int JS_ToBigInt64Free(JSContext *ctx, int64_t *pres, JSValue val);
JSValue JS_ThrowOutOfMemory(JSContext *ctx);
static JSValue JS_ThrowTypeErrorRevokedProxy(JSContext *ctx);

static int JS_CreateProperty(JSContext *ctx, JSObject *p,
                             JSAtom prop, JSValueConst val,
                             JSValueConst getter, JSValueConst setter,
                             int flags);
static int js_string_memcmp(const JSString *p1, int pos1, const JSString *p2,
                            int pos2, int len);
static JSValue js_array_buffer_constructor3(JSContext *ctx,
                                            JSValueConst new_target,
                                            uint64_t len, uint64_t *max_len,
                                            JSClassID class_id,
                                            uint8_t *buf,
                                            JSFreeArrayBufferDataFunc *free_func,
                                            void *opaque, BOOL alloc_flag);
static void js_array_buffer_free(JSRuntime *rt, void *opaque, void *ptr);
static JSArrayBuffer *js_get_array_buffer(JSContext *ctx, JSValueConst obj);
static BOOL array_buffer_is_resizable(const JSArrayBuffer *abuf);
static JSValue js_typed_array_constructor(JSContext *ctx,
                                          JSValueConst this_val,
                                          int argc, JSValueConst *argv,
                                          int classid);
static JSValue js_typed_array_constructor_ta(JSContext *ctx,
                                             JSValueConst new_target,
                                             JSValueConst src_obj,
                                             int classid, uint32_t len);
static BOOL typed_array_is_oob(JSObject *p);
static int js_typed_array_get_length_unsafe(JSContext *ctx, JSValueConst obj);
static JSValue JS_ThrowTypeErrorDetachedArrayBuffer(JSContext *ctx);
static JSValue JS_ThrowTypeErrorArrayBufferOOB(JSContext *ctx);
static JSVarRef *js_create_var_ref(JSContext *ctx, BOOL is_lexical);
static JSVarRef *get_var_ref(JSContext *ctx, JSStackFrame *sf, int var_idx,
                             BOOL is_arg);
static void __async_func_free(JSRuntime *rt, JSAsyncFunctionState *s);
static void async_func_free(JSRuntime *rt, JSAsyncFunctionState *s);
static JSValue js_generator_function_call(JSContext *ctx, JSValueConst func_obj,
                                          JSValueConst this_obj,
                                          int argc, JSValueConst *argv,
                                          int flags);
static void js_async_function_resolve_finalizer(JSRuntime *rt, JSValue val);
static void js_async_function_resolve_mark(JSRuntime *rt, JSValueConst val,
                                           JS_MarkFunc *mark_func);
static JSValue JS_EvalInternal(JSContext *ctx, JSValueConst this_obj,
                               const char *input, size_t input_len,
                               const char *filename, int flags, int scope_idx);
static void js_free_module_def(JSRuntime *rt, JSModuleDef *m);
static void js_mark_module_def(JSRuntime *rt, JSModuleDef *m,
                               JS_MarkFunc *mark_func);
static JSValue js_import_meta(JSContext *ctx);
static JSValue js_dynamic_import(JSContext *ctx, JSValueConst specifier, JSValueConst options);
static void free_var_ref(JSRuntime *rt, JSVarRef *var_ref);
static JSValue js_new_promise_capability(JSContext *ctx,
                                         JSValue *resolving_funcs,
                                         JSValueConst ctor);
static __exception int perform_promise_then(JSContext *ctx,
                                            JSValueConst promise,
                                            JSValueConst *resolve_reject,
                                            JSValueConst *cap_resolving_funcs);
static JSValue js_promise_resolve(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv, int magic);
static JSValue js_promise_then(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv);
static BOOL js_string_eq(JSContext *ctx,
                         const JSString *p1, const JSString *p2);
static int js_string_compare(JSContext *ctx,
                             const JSString *p1, const JSString *p2);
static JSValue JS_ToNumber(JSContext *ctx, JSValueConst val);
static int JS_SetPropertyValue(JSContext *ctx, JSValueConst this_obj,
                               JSValue prop, JSValue val, int flags);
static int JS_NumberIsInteger(JSContext *ctx, JSValueConst val);
static BOOL JS_NumberIsNegativeOrMinusZero(JSContext *ctx, JSValueConst val);
static JSValue JS_ToNumberFree(JSContext *ctx, JSValue val);
static int JS_GetOwnPropertyInternal(JSContext *ctx, JSPropertyDescriptor *desc,
                                     JSObject *p, JSAtom prop);
static void js_free_desc(JSContext *ctx, JSPropertyDescriptor *desc);
static int JS_AddIntrinsicBasicObjects(JSContext *ctx);
static void js_free_shape(JSRuntime *rt, JSShape *sh);
static void js_free_shape_null(JSRuntime *rt, JSShape *sh);
static int js_shape_prepare_update(JSContext *ctx, JSObject *p,
                                   JSShapeProperty **pprs);
static int init_shape_hash(JSRuntime *rt);
static __exception int js_get_length32(JSContext *ctx, uint32_t *pres,
                                       JSValueConst obj);
static __exception int js_get_length64(JSContext *ctx, int64_t *pres,
                                       JSValueConst obj);
static void free_arg_list(JSContext *ctx, JSValue *tab, uint32_t len);
static JSValue *build_arg_list(JSContext *ctx, uint32_t *plen,
                               JSValueConst array_arg);
static BOOL js_get_fast_array(JSContext *ctx, JSValueConst obj,
                              JSValue **arrpp, uint32_t *countp);
static JSValue JS_CreateAsyncFromSyncIterator(JSContext *ctx,
                                              JSValueConst sync_iter);
static void js_c_function_data_finalizer(JSRuntime *rt, JSValue val);
static void js_c_function_data_mark(JSRuntime *rt, JSValueConst val,
                                    JS_MarkFunc *mark_func);
static JSValue js_c_function_data_call(JSContext *ctx, JSValueConst func_obj,
                                       JSValueConst this_val,
                                       int argc, JSValueConst *argv, int flags);
static JSAtom js_symbol_to_atom(JSContext *ctx, JSValue val);
static void add_gc_object(JSRuntime *rt, JSGCObjectHeader *h,
                          JSGCObjectTypeEnum type);
static void remove_gc_object(JSGCObjectHeader *h);
static JSValue js_instantiate_prototype(JSContext *ctx, JSObject *p, JSAtom atom, void *opaque);
static JSValue js_module_ns_autoinit(JSContext *ctx, JSObject *p, JSAtom atom,
                                 void *opaque);
static JSValue JS_InstantiateFunctionListItem2(JSContext *ctx, JSObject *p,
                                               JSAtom atom, void *opaque);
static JSValue js_object_groupBy(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv, int is_map);
static void map_delete_weakrefs(JSRuntime *rt, JSWeakRefHeader *wh);
static void weakref_delete_weakref(JSRuntime *rt, JSWeakRefHeader *wh);
static void finrec_delete_weakref(JSRuntime *rt, JSWeakRefHeader *wh);
static void JS_RunGCInternal(JSRuntime *rt, BOOL remove_weak_objects);
static JSValue js_array_from_iterator(JSContext *ctx, uint32_t *plen,
                                      JSValueConst obj, JSValueConst method);
static int js_string_find_invalid_codepoint(JSString *p);
static JSValue js_regexp_toString(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv);
static JSValue get_date_string(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv, int magic);
static JSValue js_error_toString(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv);
static JSVarRef *js_global_object_find_uninitialized_var(JSContext *ctx, JSObject *p,
                                                         JSAtom atom, BOOL is_lexical);
static int typed_array_init(JSContext *ctx, JSValueConst obj,
                            JSValue buffer, uint64_t offset, uint64_t len,
                            BOOL track_rab);*/
/* Object 相关 */
JSValue js_object_preventExtensions(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int reflect);
JSValue js_object_getOwnPropertyDescriptor(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic);
JSValue js_object_getOwnPropertyDescriptors(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_object_is(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_object_assign(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_object_seal(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int freeze_flag);
JSValue js_object_isSealed(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int is_frozen);
JSValue js_object_fromEntries(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_object_hasOwn(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_object_toString(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_object_toLocaleString(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_object_valueOf(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_object_hasOwnProperty(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_object_isPrototypeOf(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_object_propertyIsEnumerable(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_object_get___proto__(JSContext *ctx, JSValueConst this_val);
JSValue js_object_set___proto__(JSContext *ctx, JSValueConst this_val, JSValueConst proto);
JSValue js_object___defineGetter__(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic);
JSValue js_object___lookupGetter__(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int setter);

/* Iterator 相关 */
JSValue js_iterator_wrap_next(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int *pdone, int magic);
JSValue js_iterator_concat_next(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int *pdone, int magic);
JSValue js_iterator_concat_return(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_iterator_concat(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_iterator_from(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_create_iterator_helper(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic);
JSValue js_iterator_proto_func(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic);
JSValue js_iterator_proto_reduce(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_iterator_proto_toArray(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_iterator_proto_iterator(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_iterator_proto_get_toStringTag(JSContext *ctx, JSValueConst this_val);
JSValue js_iterator_proto_set_toStringTag(JSContext *ctx, JSValueConst this_val, JSValueConst val);
JSValue js_iterator_helper_next(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int *pdone, int magic);

/* String 外来方法 */
int js_string_get_own_property(JSContext *ctx, JSPropertyDescriptor *desc, JSValueConst obj, JSAtom prop);
int js_string_define_own_property(JSContext *ctx, JSValueConst this_obj, JSAtom prop, JSValueConst val, JSValueConst getter, JSValueConst setter, int flags);
int js_string_delete_property(JSContext *ctx, JSValueConst obj, JSAtom prop);

/* Math 辅助 */
double js_pow(double a, double b);

/* Proxy 相关 */
void js_proxy_finalizer(JSRuntime *rt, JSValue val);
void js_proxy_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);

/* Map/Set 相关 */
JSValue js_map_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic);
JSValue js_map_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic);
JSValue js_map_getOrInsert(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic);
JSValue js_map_has(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic);
JSValue js_map_delete(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic);
JSValue js_map_clear(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic);
JSValue js_map_get_size(JSContext *ctx, JSValueConst this_val, int magic);
JSValue js_map_forEach(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic);
JSValue js_create_map_iterator(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic);
JSValue js_map_iterator_next(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, BOOL *pdone, int magic);
JSValue js_set_isDisjointFrom(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_set_isSubsetOf(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_set_isSupersetOf(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_set_intersection(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_set_difference(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_set_symmetricDifference(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
/* 其它函数声明 */
void js_typed_array_finalizer(JSRuntime *rt, JSValue val);
void js_typed_array_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
JSValue js_object_create(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv);
JSValue js_object_setPrototypeOf(JSContext *ctx, JSValueConst this_val,
                                        int argc, JSValueConst *argv);
JSValue js_object_getPrototypeOf(JSContext *ctx, JSValueConst this_val,
                                        int argc, JSValueConst *argv, int magic);
JSValue js_object_defineProperty(JSContext *ctx, JSValueConst this_val,
                                        int argc, JSValueConst *argv, int magic);
JSValue js_object_defineProperties(JSContext *ctx, JSValueConst this_val,
                                          int argc, JSValueConst *argv);
JSValue js_object_getOwnPropertyNames(JSContext *ctx, JSValueConst this_val,
                                             int argc, JSValueConst *argv);
JSValue js_object_getOwnPropertyNames(JSContext *ctx, JSValueConst this_val,
                                             int argc, JSValueConst *argv);
JSValue js_object_getOwnPropertyNames(JSContext *ctx, JSValueConst this_val,
                                             int argc, JSValueConst *argv);
JSValue js_object_keys(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv, int kind);
JSValue js_object_isExtensible(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv, int reflect);
JSValue js_object_getOwnPropertySymbols(JSContext *ctx, JSValueConst this_val,
                                             int argc, JSValueConst *argv);
JSValue js_object_groupBy(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv, int is_map);
JSValue js_set_union(JSContext *ctx, JSValueConst this_val,
                            int argc, JSValueConst *argv);
JSValue JS_InstantiateFunctionListItem2(JSContext *ctx, JSObject *p,
                                               JSAtom atom, void *opaque);
JSValue js_iterator_constructor_getset(JSContext *ctx,
                                              JSValueConst this_val,
                                              int argc, JSValueConst *argv,
                                              int magic,
                                              JSValue *func_data);
JSValue js_new_string8_len(JSContext *ctx, const char *buf, int len);
JSAtom js_symbol_to_atom(JSContext *ctx, JSValue val);
JSProperty *add_property(JSContext *ctx,
                                JSObject *p, JSAtom prop, int prop_flags);
JSValue JS_ToObject(JSContext *ctx, JSValueConst val);
JSValue js_map_constructor(JSContext *ctx, JSValueConst new_target,
                                  int argc, JSValueConst *argv, int magic);
JSValue js_object_constructor(JSContext *ctx, JSValueConst new_target,
                                     int argc, JSValueConst *argv);
JSValue js_iterator_constructor(JSContext *ctx, JSValueConst new_target,
                                       int argc, JSValueConst *argv);
JSValue js_typed_array_base_constructor(JSContext *ctx,
                                               JSValueConst this_val,
                                               int argc, JSValueConst *argv);
JSValue js_dataview_constructor(JSContext *ctx,
                                       JSValueConst new_target,
                                       int argc, JSValueConst *argv);
JSValue __attribute__((format(printf, 3, 4))) JS_ThrowTypeErrorAtom(JSContext *ctx, JSAtom atom, const char *fmt, ...);
JSValue __attribute__((format(printf, 3, 4))) JS_ThrowSyntaxErrorAtom(JSContext *ctx, JSAtom atom, const char *fmt, ...);
JSValue __JS_AtomToValue(JSContext *ctx, JSAtom atom, BOOL force_string);
const char *JS_AtomGetStrRT(JSRuntime *rt, char *buf, int buf_size,
                                   JSAtom atom);
const char *JS_AtomGetStr(JSContext *ctx, char *buf, int buf_size, JSAtom atom);
JSValue JS_NewCConstructor(JSContext *ctx, int class_id, const char *name,
                                  JSCFunction *func, int length, JSCFunctionEnum cproto, int magic,
                                  JSValueConst parent_ctor,
                                  const JSCFunctionListEntry *ctor_fields, int n_ctor_fields,
                                  const JSCFunctionListEntry *proto_fields, int n_proto_fields,
                                  int flags);
int JS_SetConstructor2(JSContext *ctx,
                              JSValueConst func_obj,
                              JSValueConst proto,
                              int proto_flags, int ctor_flags);
JSValue JS_NewObjectProtoList(JSContext *ctx, JSValueConst proto,
                              const JSCFunctionListEntry *fields, int n_fields);
JSFunctionDef *js_new_function_def(JSContext *ctx,
                                          JSFunctionDef *parent,
                                          BOOL is_eval,
                                          BOOL is_func_expr,
                                          const char *filename,
                                          const uint8_t *source_ptr,
                                          GetLineColCache *get_line_col_cache);


extern const JSClassExoticMethods js_arguments_exotic_methods;
extern const JSClassExoticMethods js_string_exotic_methods;
extern const JSClassExoticMethods js_proxy_exotic_methods;
extern const JSClassExoticMethods js_module_ns_exotic_methods;
static JSClassID js_class_id_alloc = JS_CLASS_INIT_COUNT;

/* JS malloc */

/* max overhead for size >= 64: 12.5% */
extern const uint16_t js_malloc_block_sizes[JS_MALLOC_BLOCK_SIZE_COUNT];
extern JSClassShortDef const js_std_class_def[];
extern const uint16_t func_kind_to_class_id[];
extern const JSOpCode opcode_info[OP_COUNT + (OP_TEMP_END - OP_TEMP_START)];
extern const JSClassExoticMethods js_module_ns_exotic_methods;
#ifdef DUMP_READ_OBJECT
extern const char * const bc_tag_str[];
#endif
extern const JSCFunctionListEntry js_object_funcs[];
extern const JSCFunctionListEntry js_object_proto_funcs[];
extern const JSCFunctionListEntry js_function_proto_funcs[];
extern const JSCFunctionListEntry js_error_proto_funcs[];
/* 2 entries for each native error class */
/* Note: we use an atom to avoid the autoinit definition which does
   not work in get_prop_string() */
extern const JSCFunctionListEntry js_native_error_proto_funcs[];
extern const JSCFunctionListEntry js_error_funcs[];
extern const JSCFunctionListEntry js_array_funcs[];
extern const JSCFunctionListEntry js_iterator_wrap_proto_funcs[];
extern const JSCFunctionListEntry js_iterator_concat_proto_funcs[];
extern const JSCFunctionListEntry js_iterator_funcs[];
extern const JSCFunctionListEntry js_iterator_proto_funcs[];
extern const JSCFunctionListEntry js_iterator_helper_proto_funcs[];
extern const JSCFunctionListEntry js_array_unscopables_funcs[];
extern const JSCFunctionListEntry js_array_proto_funcs[];
extern const JSCFunctionListEntry js_array_iterator_proto_funcs[];
extern const JSCFunctionListEntry js_number_funcs[];
extern const JSCFunctionListEntry js_number_proto_funcs[];
extern const JSCFunctionListEntry js_boolean_proto_funcs[];
extern const JSClassExoticMethods js_string_exotic_methods;
extern const JSCFunctionListEntry js_string_funcs[];
extern const JSCFunctionListEntry js_string_proto_funcs[];
extern const JSCFunctionListEntry js_string_iterator_proto_funcs[];
extern const JSCFunctionListEntry js_string_proto_normalize[];
extern const JSCFunctionListEntry js_math_funcs[];
extern const JSCFunctionListEntry js_math_obj[];
extern const JSCFunctionListEntry js_regexp_funcs[];
extern const JSCFunctionListEntry js_regexp_proto_funcs[];
extern const JSCFunctionListEntry js_regexp_string_iterator_proto_funcs[];
extern const JSCFunctionListEntry js_json_funcs[];
extern const JSCFunctionListEntry js_json_obj[];
extern const JSCFunctionListEntry js_reflect_funcs[];
extern const JSCFunctionListEntry js_reflect_obj[];
extern const JSClassExoticMethods js_proxy_exotic_methods;
extern const JSCFunctionListEntry js_proxy_funcs[];
extern const JSClassShortDef js_proxy_class_def[];
extern const JSCFunctionListEntry js_symbol_proto_funcs[];
extern const JSCFunctionListEntry js_symbol_funcs[];
extern const JSCFunctionListEntry js_map_funcs[];
extern const JSCFunctionListEntry js_map_proto_funcs[];
extern const JSCFunctionListEntry js_map_iterator_proto_funcs[];
extern const JSCFunctionListEntry js_set_proto_funcs[];
extern const JSCFunctionListEntry js_set_iterator_proto_funcs[];
extern const JSCFunctionListEntry js_weak_map_proto_funcs[];
extern const JSCFunctionListEntry js_weak_set_proto_funcs[];
extern const JSCFunctionListEntry * const js_map_proto_funcs_ptr[6];
extern const uint8_t js_map_proto_funcs_count[6];
/* Generator */
extern const JSCFunctionListEntry js_generator_function_proto_funcs[];
extern const JSCFunctionListEntry js_generator_proto_funcs[];
extern const JSCFunctionListEntry js_promise_funcs[];
extern const JSCFunctionListEntry js_promise_proto_funcs[];
/* AsyncFunction */
extern const JSCFunctionListEntry js_async_function_proto_funcs[];
/* AsyncIteratorPrototype */

extern const JSCFunctionListEntry js_async_iterator_proto_funcs[];
extern const JSCFunctionListEntry js_async_from_sync_iterator_proto_funcs[];
/* AsyncGeneratorFunction */

extern const JSCFunctionListEntry js_async_generator_function_proto_funcs[];
/* AsyncGenerator prototype */

extern const JSCFunctionListEntry js_async_generator_proto_funcs[];
extern JSClassShortDef const js_async_class_def[];
/* global object */

extern const JSCFunctionListEntry js_global_funcs[];
extern const JSCFunctionListEntry js_date_funcs[];
extern const JSCFunctionListEntry js_date_proto_funcs[];
extern const JSCFunctionListEntry js_bigint_funcs[];
extern const JSCFunctionListEntry js_bigint_proto_funcs[];
extern const JSCFunctionListEntry js_array_buffer_funcs[];
extern const JSCFunctionListEntry js_array_buffer_proto_funcs[];
extern const JSCFunctionListEntry js_shared_array_buffer_funcs[];
extern const JSCFunctionListEntry js_shared_array_buffer_proto_funcs[];
extern const unsigned char b64_enc[64];
extern const unsigned char b64url_enc[64];

#define K_WS 64
#define K_ER 65

extern const uint8_t b64_dec[256];
extern const uint8_t b64url_dec[256];
extern const char u8a_hex_digits[];
extern const JSCFunctionListEntry js_typed_array_base_funcs[];
extern const JSCFunctionListEntry js_typed_array_base_proto_funcs[];
extern const JSCFunctionListEntry js_typed_array_funcs[];
extern const JSCFunctionListEntry js_uint8array_proto_funcs[];
extern const JSCFunctionListEntry js_uint8array_funcs[];
extern const JSCFunctionListEntry js_dataview_proto_funcs[];
extern const JSCFunctionListEntry js_atomics_funcs[];
extern const JSCFunctionListEntry js_atomics_obj[];
extern const JSCFunctionListEntry js_weakref_proto_funcs[];
extern const JSClassShortDef js_weakref_class_def[];
extern const JSCFunctionListEntry js_finrec_proto_funcs[];
extern const JSClassShortDef js_finrec_class_def[];
extern const JSMallocFunctions def_malloc_funcs;
extern uint8_t const typed_array_size_log2[JS_TYPED_ARRAY_COUNT];
extern const uint32_t rope_bucket_len[ROPE_N_BUCKETS];

/* contains 10^i */
extern const js_limb_t js_pow_dec[JS_LIMB_DIGITS + 1];
extern const uint8_t digits_per_limb_table[JS_RADIX_MAX - 1];
extern const js_limb_t radix_base_table[JS_RADIX_MAX - 1];
extern const JSClassExoticMethods js_arguments_exotic_methods;
extern int const month_days[];
extern char const month_names[];
extern char const day_names[];

/* ========== 数组长度宏 ========== */

/* 显式指定大小的数组 */
#define JS_MALLOC_BLOCK_SIZES_LEN           JS_MALLOC_BLOCK_SIZE_COUNT
#define JS_STD_CLASS_DEF_LEN                50
#define JS_FUNC_KIND_TO_CLASS_ID_LEN        4
#define OPCODE_INFO_LEN                     (OP_COUNT + (OP_TEMP_END - OP_TEMP_START))
#define BC_TAG_STR_LEN                      20
#define JS_OBJECT_FUNCS_LEN                 23
#define JS_OBJECT_PROTO_FUNCS_LEN           11
#define JS_FUNCTION_PROTO_FUNCS_LEN         8
#define JS_ERROR_PROTO_FUNCS_LEN            3
#define JS_NATIVE_ERROR_PROTO_FUNCS_LEN     16
#define JS_ERROR_FUNCS_LEN                  1
#define JS_ARRAY_FUNCS_LEN                  4
#define JS_ITERATOR_WRAP_PROTO_FUNCS_LEN    2
#define JS_ITERATOR_CONCAT_PROTO_FUNCS_LEN  3
#define JS_ITERATOR_FUNCS_LEN               2
#define JS_ITERATOR_PROTO_FUNCS_LEN         13
#define JS_ITERATOR_HELPER_PROTO_FUNCS_LEN  3
#define JS_ARRAY_UNSCOPABLES_FUNCS_LEN      16
#define JS_ARRAY_PROTO_FUNCS_LEN            40
#define JS_ARRAY_ITERATOR_PROTO_FUNCS_LEN   2
#define JS_NUMBER_FUNCS_LEN                 14
#define JS_NUMBER_PROTO_FUNCS_LEN           6
#define JS_BOOLEAN_PROTO_FUNCS_LEN          2
#define JS_STRING_FUNCS_LEN                 3
#define JS_STRING_PROTO_FUNCS_LEN           50
#define JS_STRING_ITERATOR_PROTO_FUNCS_LEN  2
#ifdef CONFIG_ALL_UNICODE
#define JS_STRING_PROTO_NORMALIZE_LEN       1
#else
#define JS_STRING_PROTO_NORMALIZE_LEN       2
#endif
#define JS_MATH_FUNCS_LEN                   46
#define JS_MATH_OBJ_LEN                     1
#define JS_REGEXP_FUNCS_LEN                 2
#define JS_REGEXP_PROTO_FUNCS_LEN           19
#define JS_REGEXP_STRING_ITERATOR_PROTO_FUNCS_LEN 2
#define JS_JSON_FUNCS_LEN                   5
#define JS_JSON_OBJ_LEN                     1
#define JS_REFLECT_FUNCS_LEN                14
#define JS_REFLECT_OBJ_LEN                  1
#define JS_PROXY_FUNCS_LEN                  1
#define JS_PROXY_CLASS_DEF_LEN              1
#define JS_SYMBOL_PROTO_FUNCS_LEN           5
#define JS_SYMBOL_FUNCS_LEN                 15
#define JS_MAP_FUNCS_LEN                    2
#define JS_MAP_PROTO_FUNCS_LEN              14
#define JS_MAP_ITERATOR_PROTO_FUNCS_LEN     2
#define JS_SET_PROTO_FUNCS_LEN              18
#define JS_SET_ITERATOR_PROTO_FUNCS_LEN     2
#define JS_WEAK_MAP_PROTO_FUNCS_LEN         7
#define JS_WEAK_SET_PROTO_FUNCS_LEN         4
#define JS_MAP_PROTO_FUNCS_PTR_LEN          6
#define JS_MAP_PROTO_FUNCS_COUNT_LEN        6
#define JS_GENERATOR_FUNCTION_PROTO_FUNCS_LEN 1
#define JS_GENERATOR_PROTO_FUNCS_LEN        4
#define JS_PROMISE_FUNCS_LEN                9
#define JS_PROMISE_PROTO_FUNCS_LEN          4
#define JS_ASYNC_FUNCTION_PROTO_FUNCS_LEN   1
#define JS_ASYNC_ITERATOR_PROTO_FUNCS_LEN   1
#define JS_ASYNC_FROM_SYNC_ITERATOR_PROTO_FUNCS_LEN 3
#define JS_ASYNC_GENERATOR_FUNCTION_PROTO_FUNCS_LEN 1
#define JS_ASYNC_GENERATOR_PROTO_FUNCS_LEN  4
#define JS_ASYNC_CLASS_DEF_LEN              9
#define JS_GLOBAL_FUNCS_LEN                 15
#define JS_DATE_FUNCS_LEN                   3
#define JS_DATE_PROTO_FUNCS_LEN             47
#define JS_BIGINT_FUNCS_LEN                 2
#define JS_BIGINT_PROTO_FUNCS_LEN           3
#define JS_ARRAY_BUFFER_FUNCS_LEN           2
#define JS_ARRAY_BUFFER_PROTO_FUNCS_LEN     9
#define JS_SHARED_ARRAY_BUFFER_FUNCS_LEN    1
#define JS_SHARED_ARRAY_BUFFER_PROTO_FUNCS_LEN 6
#define B64_ENC_LEN                         64
#define B64URL_ENC_LEN                      64
#define B64_DEC_LEN                         256
#define B64URL_DEC_LEN                      256
#define U8A_HEX_DIGITS_LEN                  17          /* 包含结尾 '\0' */
#define JS_TYPED_ARRAY_BASE_FUNCS_LEN       3
#define JS_TYPED_ARRAY_BASE_PROTO_FUNCS_LEN 36
#define JS_TYPED_ARRAY_FUNCS_LEN            4
#define JS_UINT8ARRAY_PROTO_FUNCS_LEN       5
#define JS_UINT8ARRAY_FUNCS_LEN             3
#define JS_DATAVIEW_PROTO_FUNCS_LEN         26
#ifdef CONFIG_ATOMICS
#define JS_ATOMICS_FUNCS_LEN                14
#define JS_ATOMICS_OBJ_LEN                  1
#endif
#define JS_WEAKREF_PROTO_FUNCS_LEN          2
#define JS_WEAKREF_CLASS_DEF_LEN            1
#define JS_FINREC_PROTO_FUNCS_LEN           3
#define JS_FINREC_CLASS_DEF_LEN             1
#define ROPE_BUCKET_LEN                     ROPE_N_BUCKETS  /* 当前为 44 */
#define JS_POW_DEC_LEN                      (JS_LIMB_DIGITS + 1)
#define DIGITS_PER_LIMB_TABLE_LEN           (JS_RADIX_MAX - 1)
#define RADIX_BASE_TABLE_LEN                (JS_RADIX_MAX - 1)
#define JS_AUTOINIT_FUNC_TABLE_LEN          3
#define MONTH_DAYS_LEN                      12
#define MONTH_NAMES_LEN                     37          /* 包含结尾 '\0' */
#define DAY_NAMES_LEN                       22          /* 包含结尾 '\0' */
#define JS_TZABBR_LEN                                           18

/* ========== 映射宏 ========== */
#define JS_ARRAY_LEN(name)  JS_ARRAY_LEN_##name

#define JS_ARRAY_LEN_js_malloc_block_sizes               JS_MALLOC_BLOCK_SIZES_LEN
#define JS_ARRAY_LEN_js_std_class_def                    JS_STD_CLASS_DEF_LEN
#define JS_ARRAY_LEN_func_kind_to_class_id               JS_FUNC_KIND_TO_CLASS_ID_LEN
#define JS_ARRAY_LEN_opcode_info                         OPCODE_INFO_LEN
#define JS_ARRAY_LEN_bc_tag_str                          BC_TAG_STR_LEN
#define JS_ARRAY_LEN_js_object_funcs                     JS_OBJECT_FUNCS_LEN
#define JS_ARRAY_LEN_js_object_proto_funcs               JS_OBJECT_PROTO_FUNCS_LEN
#define JS_ARRAY_LEN_js_function_proto_funcs             JS_FUNCTION_PROTO_FUNCS_LEN
#define JS_ARRAY_LEN_js_error_proto_funcs                JS_ERROR_PROTO_FUNCS_LEN
#define JS_ARRAY_LEN_js_native_error_proto_funcs         JS_NATIVE_ERROR_PROTO_FUNCS_LEN
#define JS_ARRAY_LEN_js_error_funcs                      JS_ERROR_FUNCS_LEN
#define JS_ARRAY_LEN_js_array_funcs                      JS_ARRAY_FUNCS_LEN
#define JS_ARRAY_LEN_js_iterator_wrap_proto_funcs        JS_ITERATOR_WRAP_PROTO_FUNCS_LEN
#define JS_ARRAY_LEN_js_iterator_concat_proto_funcs      JS_ITERATOR_CONCAT_PROTO_FUNCS_LEN
#define JS_ARRAY_LEN_js_iterator_funcs                   JS_ITERATOR_FUNCS_LEN
#define JS_ARRAY_LEN_js_iterator_proto_funcs             JS_ITERATOR_PROTO_FUNCS_LEN
#define JS_ARRAY_LEN_js_iterator_helper_proto_funcs      JS_ITERATOR_HELPER_PROTO_FUNCS_LEN
#define JS_ARRAY_LEN_js_array_unscopables_funcs          JS_ARRAY_UNSCOPABLES_FUNCS_LEN
#define JS_ARRAY_LEN_js_array_proto_funcs                JS_ARRAY_PROTO_FUNCS_LEN
#define JS_ARRAY_LEN_js_array_iterator_proto_funcs       JS_ARRAY_ITERATOR_PROTO_FUNCS_LEN
#define JS_ARRAY_LEN_js_number_funcs                     JS_NUMBER_FUNCS_LEN
#define JS_ARRAY_LEN_js_number_proto_funcs               JS_NUMBER_PROTO_FUNCS_LEN
#define JS_ARRAY_LEN_js_boolean_proto_funcs              JS_BOOLEAN_PROTO_FUNCS_LEN
#define JS_ARRAY_LEN_js_string_funcs                     JS_STRING_FUNCS_LEN
#define JS_ARRAY_LEN_js_string_proto_funcs               JS_STRING_PROTO_FUNCS_LEN
#define JS_ARRAY_LEN_js_string_iterator_proto_funcs      JS_STRING_ITERATOR_PROTO_FUNCS_LEN
#define JS_ARRAY_LEN_js_string_proto_normalize           JS_STRING_PROTO_NORMALIZE_LEN
#define JS_ARRAY_LEN_js_math_funcs                       JS_MATH_FUNCS_LEN
#define JS_ARRAY_LEN_js_math_obj                         JS_MATH_OBJ_LEN
#define JS_ARRAY_LEN_js_regexp_funcs                     JS_REGEXP_FUNCS_LEN
#define JS_ARRAY_LEN_js_regexp_proto_funcs               JS_REGEXP_PROTO_FUNCS_LEN
#define JS_ARRAY_LEN_js_regexp_string_iterator_proto_funcs JS_REGEXP_STRING_ITERATOR_PROTO_FUNCS_LEN
#define JS_ARRAY_LEN_js_json_funcs                       JS_JSON_FUNCS_LEN
#define JS_ARRAY_LEN_js_json_obj                         JS_JSON_OBJ_LEN
#define JS_ARRAY_LEN_js_reflect_funcs                    JS_REFLECT_FUNCS_LEN
#define JS_ARRAY_LEN_js_reflect_obj                      JS_REFLECT_OBJ_LEN
#define JS_ARRAY_LEN_js_proxy_funcs                      JS_PROXY_FUNCS_LEN
#define JS_ARRAY_LEN_js_proxy_class_def                  JS_PROXY_CLASS_DEF_LEN
#define JS_ARRAY_LEN_js_symbol_proto_funcs               JS_SYMBOL_PROTO_FUNCS_LEN
#define JS_ARRAY_LEN_js_symbol_funcs                     JS_SYMBOL_FUNCS_LEN
#define JS_ARRAY_LEN_js_map_funcs                        JS_MAP_FUNCS_LEN
#define JS_ARRAY_LEN_js_map_proto_funcs                  JS_MAP_PROTO_FUNCS_LEN
#define JS_ARRAY_LEN_js_map_iterator_proto_funcs         JS_MAP_ITERATOR_PROTO_FUNCS_LEN
#define JS_ARRAY_LEN_js_set_proto_funcs                  JS_SET_PROTO_FUNCS_LEN
#define JS_ARRAY_LEN_js_set_iterator_proto_funcs         JS_SET_ITERATOR_PROTO_FUNCS_LEN
#define JS_ARRAY_LEN_js_weak_map_proto_funcs             JS_WEAK_MAP_PROTO_FUNCS_LEN
#define JS_ARRAY_LEN_js_weak_set_proto_funcs             JS_WEAK_SET_PROTO_FUNCS_LEN
#define JS_ARRAY_LEN_js_map_proto_funcs_ptr              JS_MAP_PROTO_FUNCS_PTR_LEN
#define JS_ARRAY_LEN_js_map_proto_funcs_count            JS_MAP_PROTO_FUNCS_COUNT_LEN
#define JS_ARRAY_LEN_js_generator_function_proto_funcs   JS_GENERATOR_FUNCTION_PROTO_FUNCS_LEN
#define JS_ARRAY_LEN_js_generator_proto_funcs            JS_GENERATOR_PROTO_FUNCS_LEN
#define JS_ARRAY_LEN_js_promise_funcs                    JS_PROMISE_FUNCS_LEN
#define JS_ARRAY_LEN_js_promise_proto_funcs              JS_PROMISE_PROTO_FUNCS_LEN
#define JS_ARRAY_LEN_js_async_function_proto_funcs       JS_ASYNC_FUNCTION_PROTO_FUNCS_LEN
#define JS_ARRAY_LEN_js_async_iterator_proto_funcs       JS_ASYNC_ITERATOR_PROTO_FUNCS_LEN
#define JS_ARRAY_LEN_js_async_from_sync_iterator_proto_funcs JS_ASYNC_FROM_SYNC_ITERATOR_PROTO_FUNCS_LEN
#define JS_ARRAY_LEN_js_async_generator_function_proto_funcs JS_ASYNC_GENERATOR_FUNCTION_PROTO_FUNCS_LEN
#define JS_ARRAY_LEN_js_async_generator_proto_funcs      JS_ASYNC_GENERATOR_PROTO_FUNCS_LEN
#define JS_ARRAY_LEN_js_async_class_def                  JS_ASYNC_CLASS_DEF_LEN
#define JS_ARRAY_LEN_js_global_funcs                     JS_GLOBAL_FUNCS_LEN
#define JS_ARRAY_LEN_js_date_funcs                       JS_DATE_FUNCS_LEN
#define JS_ARRAY_LEN_js_date_proto_funcs                 JS_DATE_PROTO_FUNCS_LEN
#define JS_ARRAY_LEN_js_bigint_funcs                     JS_BIGINT_FUNCS_LEN
#define JS_ARRAY_LEN_js_bigint_proto_funcs               JS_BIGINT_PROTO_FUNCS_LEN
#define JS_ARRAY_LEN_js_array_buffer_funcs               JS_ARRAY_BUFFER_FUNCS_LEN
#define JS_ARRAY_LEN_js_array_buffer_proto_funcs         JS_ARRAY_BUFFER_PROTO_FUNCS_LEN
#define JS_ARRAY_LEN_js_shared_array_buffer_funcs        JS_SHARED_ARRAY_BUFFER_FUNCS_LEN
#define JS_ARRAY_LEN_js_shared_array_buffer_proto_funcs  JS_SHARED_ARRAY_BUFFER_PROTO_FUNCS_LEN
#define JS_ARRAY_LEN_b64_enc                             B64_ENC_LEN
#define JS_ARRAY_LEN_b64url_enc                          B64URL_ENC_LEN
#define JS_ARRAY_LEN_b64_dec                             B64_DEC_LEN
#define JS_ARRAY_LEN_b64url_dec                          B64URL_DEC_LEN
#define JS_ARRAY_LEN_u8a_hex_digits                      U8A_HEX_DIGITS_LEN
#define JS_ARRAY_LEN_js_typed_array_base_funcs           JS_TYPED_ARRAY_BASE_FUNCS_LEN
#define JS_ARRAY_LEN_js_typed_array_base_proto_funcs     JS_TYPED_ARRAY_BASE_PROTO_FUNCS_LEN
#define JS_ARRAY_LEN_js_typed_array_funcs                JS_TYPED_ARRAY_FUNCS_LEN
#define JS_ARRAY_LEN_js_uint8array_proto_funcs           JS_UINT8ARRAY_PROTO_FUNCS_LEN
#define JS_ARRAY_LEN_js_uint8array_funcs                 JS_UINT8ARRAY_FUNCS_LEN
#define JS_ARRAY_LEN_js_dataview_proto_funcs             JS_DATAVIEW_PROTO_FUNCS_LEN
#ifdef CONFIG_ATOMICS
#define JS_ARRAY_LEN_js_atomics_funcs                    JS_ATOMICS_FUNCS_LEN
#define JS_ARRAY_LEN_js_atomics_obj                      JS_ATOMICS_OBJ_LEN
#endif
#define JS_ARRAY_LEN_js_weakref_proto_funcs              JS_WEAKREF_PROTO_FUNCS_LEN
#define JS_ARRAY_LEN_js_weakref_class_def                JS_WEAKREF_CLASS_DEF_LEN
#define JS_ARRAY_LEN_js_finrec_proto_funcs               JS_FINREC_PROTO_FUNCS_LEN
#define JS_ARRAY_LEN_js_finrec_class_def                 JS_FINREC_CLASS_DEF_LEN
#define JS_ARRAY_LEN_rope_bucket_len                     ROPE_BUCKET_LEN
#define JS_ARRAY_LEN_js_pow_dec                          JS_POW_DEC_LEN
#define JS_ARRAY_LEN_digits_per_limb_table               DIGITS_PER_LIMB_TABLE_LEN
#define JS_ARRAY_LEN_radix_base_table                    RADIX_BASE_TABLE_LEN
#define JS_ARRAY_LEN_js_autoinit_func_table              JS_AUTOINIT_FUNC_TABLE_LEN
#define JS_ARRAY_LEN_month_days                          MONTH_DAYS_LEN
#define JS_ARRAY_LEN_month_names                         MONTH_NAMES_LEN
#define JS_ARRAY_LEN_day_names                           DAY_NAMES_LEN
#define JS_ARRAY_LEN_js_tzabbr  JS_TZABBR_LEN

/* %s is replaced by 'atom'. The macro is used so that gcc can check
    the format string. */
// #define JS_ThrowTypeErrorAtom(ctx, fmt, atom) __JS_ThrowTypeErrorAtom(ctx, atom, fmt, "")
// #define JS_ThrowSyntaxErrorAtom(ctx, fmt, atom) __JS_ThrowSyntaxErrorAtom(ctx, atom, fmt, "")

static inline JSMallocBlockHeader *js_rc(void *ptr)
{
    return container_of(ptr, JSMallocBlockHeader, user_data);
}

/* resize the array and update its size if req_size > *psize */
static inline int js_resize_array(JSContext *ctx, void **parray, int elem_size,
                                  int *psize, int req_size)
{
    if (unlikely(req_size > *psize))
        return js_realloc_array(ctx, parray, elem_size, psize, req_size);
    else
        return 0;
}

static inline void js_dbuf_init(JSContext *ctx, DynBuf *s)
{
    dbuf_init2(s, ctx->rt, (DynBufReallocFunc *)js_realloc_rt);
}

static void *js_realloc_bytecode_rt(void *opaque, void *ptr, size_t size)
{
    JSRuntime *rt = opaque;
    if (size > (INT32_MAX / 2)) {
        /* the bytecode cannot be larger than 2G. Leave some slack to
           avoid some overflows. */
        return NULL;
    } else {
        return js_realloc_rt(rt, ptr, size);
    }
}

static inline void js_dbuf_bytecode_init(JSContext *ctx, DynBuf *s)
{
    dbuf_init2(s, ctx->rt, js_realloc_bytecode_rt);
}

static inline int is_digit(int c) {
    return c >= '0' && c <= '9';
}

static inline int string_get(const JSString *p, int idx) {
    return p->is_wide_char ? p->u.str16[idx] : p->u.str8[idx];
}

#if !defined(CONFIG_STACK_CHECK)
/* no stack limitation */
static inline uintptr_t js_get_stack_pointer(void)
{
    return 0;
}

static inline BOOL js_check_stack_overflow(JSRuntime *rt, size_t alloca_size)
{
    return FALSE;
}
#else
/* Note: OS and CPU dependent */
static inline uintptr_t js_get_stack_pointer(void)
{
    return (uintptr_t)__builtin_frame_address(0);
}

static inline BOOL js_check_stack_overflow(JSRuntime *rt, size_t alloca_size)
{
    uintptr_t sp;
    sp = js_get_stack_pointer() - alloca_size;
    return unlikely(sp < rt->stack_limit);
}
#endif

static inline uint32_t atom_get_free(const JSAtomStruct *p)
{
    return (uintptr_t)p >> 1;
}

static inline BOOL atom_is_free(const JSAtomStruct *p)
{
    return (uintptr_t)p & 1;
}

static inline JSAtomStruct *atom_set_free(uint32_t v)
{
    return (JSAtomStruct *)(((uintptr_t)v << 1) | 1);
}

/* same as JS_FreeValueRT() but faster */
static inline void js_free_string(JSRuntime *rt, JSString *str)
{
    if (--js_rc(str)->ref_count <= 0) {
        if (str->atom_type) {
            JS_FreeAtomStruct(rt, str);
        } else {
#ifdef DUMP_LEAKS
            list_del(&str->link);
#endif
            js_free_rt(rt, str);
        }
    }
}

static inline void set_value(JSContext *ctx, JSValue *pval, JSValue new_val)
{
    JSValue old_val;
    old_val = *pval;
    *pval = new_val;
    JS_FreeValue(ctx, old_val);
}


static inline BOOL is_strict_mode(JSContext *ctx)
{
    JSStackFrame *sf = ctx->rt->current_stack_frame;
    return (sf && (sf->js_mode & JS_MODE_STRICT));
}

static inline BOOL __JS_AtomIsConst(JSAtom v)
{
#if defined(DUMP_LEAKS) && DUMP_LEAKS > 1
        return (int32_t)v <= 0;
#else
        return (int32_t)v < JS_ATOM_END;
#endif
}

static inline BOOL __JS_AtomIsTaggedInt(JSAtom v)
{
    return (v & JS_ATOM_TAG_INT) != 0;
}

static inline JSAtom __JS_AtomFromUInt32(uint32_t v)
{
    return v | JS_ATOM_TAG_INT;
}

static inline uint32_t __JS_AtomToUInt32(JSAtom atom)
{
    return atom & ~JS_ATOM_TAG_INT;
}

static inline int is_num(int c)
{
    return c >= '0' && c <= '9';
}

/* return TRUE if the string is a number n with 0 <= n <= 2^32-1 */
static inline BOOL is_num_string(uint32_t *pval, const JSString *p)
{
    uint32_t n;
    uint64_t n64;
    int c, i, len;

    len = p->len;
    if (len == 0 || len > 10)
        return FALSE;
    c = string_get(p, 0);
    if (is_num(c)) {
        if (c == '0') {
            if (len != 1)
                return FALSE;
            n = 0;
        } else {
            n = c - '0';
            for(i = 1; i < len; i++) {
                c = string_get(p, i);
                if (!is_num(c))
                    return FALSE;
                n64 = (uint64_t)n * 10 + (c - '0');
                if ((n64 >> 32) != 0)
                    return FALSE;
                n = n64;
            }
        }
        *pval = n;
        return TRUE;
    } else {
        return FALSE;
    }
}

/* XXX: could use faster version ? */
static inline uint32_t hash_string8(const uint8_t *str, size_t len, uint32_t h)
{
    size_t i;

    for(i = 0; i < len; i++)
        h = h * 263 + str[i];
    return h;
}

static inline uint32_t hash_string16(const uint16_t *str,
                                     size_t len, uint32_t h)
{
    size_t i;

    for(i = 0; i < len; i++)
        h = h * 263 + str[i];
    return h;
}

static inline BOOL JS_IsEmptyString(JSValueConst v)
{
    return JS_VALUE_GET_TAG(v) == JS_TAG_STRING && JS_VALUE_GET_STRING(v)->len == 0;
}

static inline int string_buffer_init(JSContext *ctx, StringBuffer *s, int size)
{
    return string_buffer_init2(ctx, s, size, 0);
}

/* 0 <= c <= 0x10ffff */
static inline int string_buffer_putc(StringBuffer *s, uint32_t c)
{
    if (likely(s->len < s->size)) {
        if (s->is_wide_char) {
            if (c < 0x10000) {
                s->str->u.str16[s->len++] = c;
                return 0;
            } else if (likely((s->len + 1) < s->size)) {
                s->str->u.str16[s->len++] = get_hi_surrogate(c);
                s->str->u.str16[s->len++] = get_lo_surrogate(c);
                return 0;
            }
        } else if (c < 0x100) {
            s->str->u.str8[s->len++] = c;
            return 0;
        }
    }
    return string_buffer_putc_slow(s, c);
}

static inline size_t get_shape_size(size_t hash_size, size_t prop_size)
{
    return sizeof(JSShape) + hash_size * sizeof(uint32_t) +
        prop_size * sizeof(JSShapeProperty);
}

static inline JSShapeProperty *get_shape_prop(JSShape *sh)
{
    return (JSShapeProperty *)((uint32_t *)(sh + 1) + sh->prop_hash_mask + 1);
}

/* create a new empty shape with prototype 'proto'. It is not hashed */
static inline JSShape *js_new_shape_nohash(JSContext *ctx, JSObject *proto,
                                           int hash_size, int prop_size)
{
    JSRuntime *rt = ctx->rt;
    JSShape *sh;
    size_t alloc_size = get_shape_size(hash_size, prop_size);

    JS_LOG("js_new_shape_nohash", "hash_size=%d, prop_size=%d, alloc_size=%u",
           hash_size, prop_size, (unsigned)alloc_size);

    sh = js_malloc(ctx, alloc_size);
    if (!sh) {
        JS_LOG("js_new_shape_nohash", "js_malloc failed for %u bytes", (unsigned)alloc_size);
        return NULL;
    }
    js_rc(sh)->ref_count = 1;
    add_gc_object(rt, &sh->header, JS_GC_OBJ_TYPE_SHAPE);
    if (proto)
        JS_DupValue(ctx, JS_MKPTR(JS_TAG_OBJECT, proto));
    sh->proto = proto;
    memset(sh->hash_table, 0, sizeof(sh->hash_table[0]) * hash_size);
    sh->prop_hash_mask = hash_size - 1;
    sh->prop_size = prop_size;
    sh->prop_count = 0;
    sh->deleted_prop_count = 0;
    sh->is_hashed = FALSE;
    return sh;
}

static inline __exception int js_poll_interrupts(JSContext *ctx)
{
    if (unlikely(--ctx->interrupt_counter <= 0)) {
        return __js_poll_interrupts(ctx);
    } else {
        return 0;
    }
}

/* Preconditions: 'p' must be of class JS_CLASS_ARRAY, p->fast_array =
   TRUE and p->extensible = TRUE */
static inline int add_fast_array_element(JSContext *ctx, JSObject *p,
                                         JSValue val, int flags)
{
    uint32_t new_len, array_len;
    /* extend the array by one */
    /* XXX: convert to slow array if new_len > 2^31-1 elements */
    new_len = p->u.array.count + 1;
    /* update the length if necessary. We assume that if the length is
       not an integer, then if it >= 2^31.  */
    if (likely(JS_VALUE_GET_TAG(p->prop[0].u.value) == JS_TAG_INT)) {
        array_len = JS_VALUE_GET_INT(p->prop[0].u.value);
        if (new_len > array_len) {
            if (unlikely(!(get_shape_prop(p->shape)->flags & JS_PROP_WRITABLE))) {
                JS_FreeValue(ctx, val);
                return JS_ThrowTypeErrorReadOnly(ctx, flags, (unsigned long)JS_ATOM_length);
            }
            p->prop[0].u.value = JS_NewInt32(ctx, new_len);
        }
    }
    if (unlikely(new_len > p->u.array.u1.size)) {
        if (expand_fast_array(ctx, p, new_len)) {
            JS_FreeValue(ctx, val);
            return -1;
        }
    }
    p->u.array.u.values[new_len - 1] = val;
    p->u.array.count = new_len;
    return TRUE;
}

static inline BOOL JS_IsHTMLDDA(JSContext *ctx, JSValueConst obj)
{
    JSObject *p;
    if (JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT)
        return FALSE;
    p = JS_VALUE_GET_OBJ(obj);
    return p->is_HTMLDDA;
}

#if JS_LIMB_BITS == 32
/* a != 0 */
static inline js_limb_t js_limb_clz(js_limb_t a)
{
    return clz32(a);
}
#else
static inline js_limb_t js_limb_clz(js_limb_t a)
{
    return clz64(a);
}
#endif

/* handle a = 0 too */
static inline js_limb_t js_limb_safe_clz(js_limb_t a)
{
    if (a == 0)
        return JS_LIMB_BITS;
    else
        return js_limb_clz(a);
}

/* WARNING: d must be >= 2^(JS_LIMB_BITS-1) */
static inline js_limb_t udiv1norm_init(js_limb_t d)
{
    js_limb_t a0, a1;
    a1 = -d - 1;
    a0 = -1;
    return (((js_dlimb_t)a1 << JS_LIMB_BITS) | a0) / d;
}

/* return the quotient and the remainder in '*pr'of 'a1*2^JS_LIMB_BITS+a0
   / d' with 0 <= a1 < d. */
static inline js_limb_t udiv1norm(js_limb_t *pr, js_limb_t a1, js_limb_t a0,
                                js_limb_t d, js_limb_t d_inv)
{
    js_limb_t n1m, n_adj, q, r, ah;
    js_dlimb_t a;
    n1m = ((js_slimb_t)a0 >> (JS_LIMB_BITS - 1));
    n_adj = a0 + (n1m & d);
    a = (js_dlimb_t)d_inv * (a1 - n1m) + n_adj;
    q = (a >> JS_LIMB_BITS) + a1;
    /* compute a - q * r and update q so that the remainder is\
       between 0 and d - 1 */
    a = ((js_dlimb_t)a1 << JS_LIMB_BITS) | a0;
    a = a - (js_dlimb_t)q * d - d;
    ah = a >> JS_LIMB_BITS;
    q += 1 + ah;
    r = (js_limb_t)a + (ah & d);
    *pr = r;
    return q;
}

/* return 0 or 1 depending on the sign */
static inline int js_bigint_sign(const JSBigInt *a)
{
    return a->tab[a->len - 1] >> (JS_LIMB_BITS - 1);
}

static inline int JS_ToFloat64Free(JSContext *ctx, double *pres, JSValue val)
{
    uint32_t tag;

    tag = JS_VALUE_GET_TAG(val);
    if (tag <= JS_TAG_NULL) {
        *pres = JS_VALUE_GET_INT(val);
        return 0;
    } else if (JS_TAG_IS_FLOAT64(tag)) {
        *pres = JS_VALUE_GET_FLOAT64(val);
        return 0;
    } else {
        return __JS_ToFloat64Free(ctx, pres, val);
    }
}

static inline int JS_ToUint32Free(JSContext *ctx, uint32_t *pres, JSValue val)
{
    return JS_ToInt32Free(ctx, (int32_t *)pres, val);
}

static inline BOOL tag_is_string(uint32_t tag)
{
    return tag == JS_TAG_STRING || tag == JS_TAG_STRING_ROPE;
}

static inline BOOL token_is_pseudo_keyword(JSParseState *s, JSAtom atom) {
    return s->token.val == TOK_IDENT && s->token.u.ident.atom == atom &&
        !s->token.u.ident.has_escape;
}

static inline int get_prev_opcode(JSFunctionDef *fd) {
    if (fd->last_opcode_pos < 0 || dbuf_error(&fd->byte_code))
        return OP_invalid;
    else
        return fd->byte_code.buf[fd->last_opcode_pos];
}

static inline void capture_var(JSFunctionDef *s, JSVarDef *vd)
{
    if (!vd->is_captured) {
        vd->is_captured = 1;
        vd->var_ref_idx = s->var_ref_count++;
    }
}

static inline BOOL is_be(void)
{
    union {
        uint16_t a;
        uint8_t  b;
    } u = {0x100};
    return u.b;
}

#if defined(__aarch64__)
static inline void cpu_pause(void)
{
    asm volatile("yield" ::: "memory");
}
#elif defined(__x86_64) || defined(__i386__)
static inline void cpu_pause(void)
{
    asm volatile("pause" ::: "memory");
}
#else
static inline void cpu_pause(void)
{
}
#endif

static inline int to_digit(int c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    else if (c >= 'A' && c <= 'Z')
        return c - 'A' + 10;
    else if (c >= 'a' && c <= 'z')
        return c - 'a' + 10;
    else
        return 36;
}
