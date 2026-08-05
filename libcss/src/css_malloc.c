#include "css_malloc.h"
#include "tinyalloc.h"
#include "ft_malloc.h"
#include <string.h>
#include <stdint.h>

/* 内存池基址（far，但 large 模型默认） */
static void *pool_base = NULL;

/* ---------- 初始化/销毁（不变）---------- */
int css_heap_init(size_t pool_size, size_t heap_blocks,
                 size_t split_thresh, size_t alignment)
{
    if (pool_size > 0xFFF0)
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

void css_heap_destroy(void)
{
    if (pool_base) {
        ft_free(pool_base);
        pool_base = NULL;
    }
}

/* ---------- 修复后的分配函数（不再嵌入大小）---------- */

void *css_malloc(size_t size)
{
    if (size == 0)
        return NULL;

    /* 检查是否还有空闲控制块 */
    if (ta_num_free() + ta_num_fresh() == 0) 
        return NULL;

    void *p = ta_alloc(size);

    return p;
}

void *css_calloc(size_t num, size_t size)
{
    void *p = css_malloc(num * size);
    if (p)
		memset(p, 0, num * size);
    return p;
}

void *css_realloc(void *ptr, size_t new_size)
{
    if (ptr == NULL)
        return css_malloc(new_size);

    if (new_size == 0) {
        css_free(ptr);
        return NULL;
    }

    size_t old_size = ta_alloc_size(ptr);   /* 获取原块真实大小 */
    if (old_size == 0) {
        /* 无效指针或 tinyalloc 无法找到该块 */
        return NULL;
    }

    void *new_ptr = css_malloc(new_size);
    if (!new_ptr)
        return NULL;

    size_t copy_size = (old_size < new_size) ? old_size : new_size;
    memcpy(new_ptr, ptr, copy_size);
    css_free(ptr);
    return new_ptr;
}

void css_free(void *ptr)
{
    if (!ptr)
        return;
    ta_free(ptr);
}
