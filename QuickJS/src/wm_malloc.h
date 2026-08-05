#ifndef WM_ALLOC_H
#define WM_ALLOC_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 内存池初始化与销毁 */
int  wm_heap_init(size_t pool_size, size_t heap_blocks,
                  size_t split_thresh, size_t alignment);
void wm_heap_destroy(void);

/* 安全的分配函数（前缀 wm_） */
void *wm_malloc(size_t size);
void *wm_calloc(size_t num, size_t size);
void *wm_realloc(void *ptr, size_t new_size);
void  wm_free(void *ptr);

#ifdef __cplusplus
}
#endif

#endif
