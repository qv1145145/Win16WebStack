#include "wm_malloc.h"
#include "tinyalloc.h"
#include "ft_malloc.h"
#include <string.h>
#include <stdint.h>

/* 内存池基址（far，但 large 模型默认） */
static void *pool_base = NULL;

/* ---------- 初始化/销毁 ---------- */
int wm_heap_init(size_t pool_size, size_t heap_blocks,
                 size_t split_thresh, size_t alignment)
{
    /* 杜绝过大池导致跨段（64KB 限制） */
    if (pool_size > 0xFFF0)   /* 65520 字节，留出段内头部空间 */
        return 0;

    pool_base = ft_malloc((long)pool_size);
    if (!pool_base)
        return 0;

    if (!ta_init(pool_base, (const char *)pool_base + pool_size,
                 heap_blocks, split_thresh, alignment)) {
        ft_free(pool_base);
        pool_base = NULL;
        return 0;
    }

    return 1;
}

void wm_heap_destroy(void)
{
    if (pool_base) {
        ft_free(pool_base);
        pool_base = NULL;
    }
}

/* ---------- 内部辅助：在分配块前嵌入大小 ---------- */
static size_t *header_of(void *ptr) {
    return ((size_t *)ptr) - 1;
}

/* ---------- 包装函数 ---------- */
void *wm_malloc(size_t size)
{
    size_t *p;

    if (size == 0)
        return NULL;

    /* 检查内存池是否还有空闲控制块（可选，但不影响分配） */
    if (ta_num_free() + ta_num_fresh() == 0) {
        /* 没有可用的 Block，分配必定失败，直接返回 NULL */
        return NULL;
    }

    p = (size_t *)ta_alloc(size + sizeof(size_t));
    if (!p)
        return NULL;

    *p = size;                  /* 记录块大小 */
    return (void *)(p + 1);
}

void *wm_calloc(size_t num, size_t size)
{
    void *p = wm_malloc(num * size);
    if (p)
        memset(p, 0, num * size);
    return p;
}

void *wm_realloc(void *ptr, size_t new_size)
{
    size_t old_size;
    void *new_ptr;

    if (ptr == NULL)
        return wm_malloc(new_size);

    if (new_size == 0) {
        wm_free(ptr);
        return NULL;
    }

    /* 从头部读取原大小 */
    old_size = *header_of(ptr);

    /* 如果新大小 ≤ 原大小，可原地保留（但 tinyalloc 不支持 truncate，我们仍分配-复制-释放） */
    new_ptr = wm_malloc(new_size);
    if (!new_ptr)
        return NULL;

    /* 拷贝旧数据（取较小长度） */
    memcpy(new_ptr, ptr, old_size < new_size ? old_size : new_size);
    wm_free(ptr);
    return new_ptr;
}

void wm_free(void *ptr)
{
    if (!ptr)
        return;

    ta_free(header_of(ptr));
}
