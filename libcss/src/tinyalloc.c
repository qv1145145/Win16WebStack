#include "tinyalloc.h"
#include <stdint.h>

#ifdef TA_DEBUG
extern void print_s(char *);
extern void print_i(size_t);
#else
#define print_s(X)
#define print_i(X)
#endif

typedef struct Block Block;

struct Block {
    void *addr;          /* far 指针，在 large 模型下为 32 位 */
    Block *next;
    size_t size;
};

typedef struct {
    Block *free;         /* 第一个空闲块 */
    Block *used;         /* 第一个已用块 */
    Block *fresh;        /* 第一个未使用的控制块 */
    unsigned long top;   /* 【改】空闲区域顶端绝对地址 (far 指针值) */
} Heap;

static Heap *heap = NULL;
static const void *heap_limit = NULL;
static size_t heap_split_thresh;
static size_t heap_alignment;
static size_t heap_max_blocks;

static void insert_block(Block *block) {
#ifndef TA_DISABLE_COMPACT
    Block *ptr  = heap->free;
    Block *prev = NULL;
    while (ptr != NULL) {
        /* 【改】地址比较使用 unsigned long，保留完整 32 位 */
        if ((unsigned long)block->addr <= (unsigned long)ptr->addr) {
            print_s("insert");
            print_i((unsigned long)ptr);
            break;
        }
        prev = ptr;
        ptr  = ptr->next;
    }
    if (prev != NULL) {
        if (ptr == NULL) {
            print_s("new tail");
        }
        prev->next = block;
    } else {
        print_s("new head");
        heap->free = block;
    }
    block->next = ptr;
#else
    block->next = heap->free;
    heap->free  = block;
#endif
}

#ifndef TA_DISABLE_COMPACT
static void release_blocks(Block *scan, Block *to) {
    Block *scan_next;
    while (scan != to) {
        print_s("release");
        print_i((unsigned long)scan);
        scan_next   = scan->next;
        scan->next  = heap->fresh;
        heap->fresh = scan;
        scan->addr  = 0;
        scan->size  = 0;
        scan        = scan_next;
    }
}

static void compact() {
    Block *ptr = heap->free;
    Block *prev;
    Block *scan;
    while (ptr != NULL) {
        prev = ptr;
        scan = ptr->next;
        while (scan != NULL &&
               /* 【改】地址加法使用 unsigned long */
               (unsigned long)prev->addr + prev->size == (unsigned long)scan->addr) {
            print_s("merge");
            print_i((unsigned long)scan);
            prev = scan;
            scan = scan->next;
        }
        if (prev != ptr) {
            /* 【改】地址差仍用 size_t，因差值 < 64K */
            size_t new_size =
                (unsigned long)prev->addr - (unsigned long)ptr->addr + prev->size;
            print_s("new size");
            print_i(new_size);
            ptr->size   = new_size;
            Block *next = prev->next;
            release_blocks(ptr->next, prev->next);
            ptr->next = next;
        }
        ptr = ptr->next;
    }
}
#endif

bool ta_init(const void *base, const void *limit, const size_t heap_blocks, const size_t split_thresh, const size_t alignment) {
    heap = (Heap *)base;
    heap_limit = limit;
    heap_split_thresh = split_thresh;
    heap_alignment = alignment;
    heap_max_blocks = heap_blocks;

    heap->free   = NULL;
    heap->used   = NULL;
    heap->fresh  = (Block *)(heap + 1);
    /* 【改】top 现在存储完整的 far 指针值（段:偏移） */
    heap->top    = (unsigned long)(heap->fresh + heap_blocks);

    Block *block = heap->fresh;
    size_t i     = heap_max_blocks - 1;
    while (i--) {
        block->next = block + 1;
        block++;
    }
    block->next = NULL;
    return true;
}

bool ta_free(void *free) {
    Block *block = heap->used;
    Block *prev  = NULL;
    while (block != NULL) {
        if (free == block->addr) {
            if (prev) {
                prev->next = block->next;
            } else {
                heap->used = block->next;
            }
            insert_block(block);
#ifndef TA_DISABLE_COMPACT
            compact();
#endif
            return true;
        }
        prev  = block;
        block = block->next;
    }
    return false;
}

static Block *alloc_block(size_t num) {
    Block *ptr  = heap->free;
    Block *prev = NULL;
    unsigned long top = heap->top;   /* 【改】使用 32 位 */
    num         = (num + heap_alignment - 1) & -heap_alignment;

    while (ptr != NULL) {
        /* 【改】全部使用 unsigned long 进行地址运算 */
        const int is_top = ((unsigned long)ptr->addr + ptr->size >= top)
                         && ((unsigned long)ptr->addr + num <= (unsigned long)heap_limit);
        if (is_top || ptr->size >= num) {
            if (prev != NULL) {
                prev->next = ptr->next;
            } else {
                heap->free = ptr->next;
            }
            ptr->next  = heap->used;
            heap->used = ptr;
            if (is_top) {
                print_s("resize top block");
                ptr->size = num;
                heap->top = (unsigned long)ptr->addr + num;
#ifndef TA_DISABLE_SPLIT
            } else if (heap->fresh != NULL) {
                size_t excess = ptr->size - num;
                if (excess >= heap_split_thresh) {
                    ptr->size    = num;
                    Block *split = heap->fresh;
                    heap->fresh  = split->next;
                    /* 【改】split 地址也使用 unsigned long 运算 */
                    split->addr  = (void *)((unsigned long)ptr->addr + num);
                    print_s("split");
                    print_i((unsigned long)split->addr);
                    split->size = excess;
                    insert_block(split);
#ifndef TA_DISABLE_COMPACT
                    compact();
#endif
                }
#endif
            }
            return ptr;
        }
        prev = ptr;
        ptr  = ptr->next;
    }

    /* 没有匹配的空闲块，尝试从 fresh 区域分配新块 */
    unsigned long new_top = top + num;   /* 【改】使用 unsigned long */
    if (heap->fresh != NULL && new_top <= (unsigned long)heap_limit) {
        ptr         = heap->fresh;
        heap->fresh = ptr->next;
        ptr->addr   = (void *)top;       /* top 已是完整 32 位地址 */
        ptr->next   = heap->used;
        ptr->size   = num;
        heap->used  = ptr;
        heap->top   = new_top;
        return ptr;
    }
    return NULL;
}

void *ta_alloc(size_t num) {
    Block *block = alloc_block(num);
    if (block != NULL) {
        return block->addr;
    }
    return NULL;
}

static void memclear(void *ptr, size_t num) {
    size_t *ptrw = (size_t *)ptr;
    size_t numw  = (num & -sizeof(size_t)) / sizeof(size_t);
    while (numw--) {
        *ptrw++ = 0;
    }
    num &= (sizeof(size_t) - 1);
    uint8_t *ptrb = (uint8_t *)ptrw;
    while (num--) {
        *ptrb++ = 0;
    }
}

void *ta_calloc(size_t num, size_t size) {
    num *= size;
    Block *block = alloc_block(num);
    if (block != NULL) {
        memclear(block->addr, num);
        return block->addr;
    }
    return NULL;
}

static size_t count_blocks(Block *ptr) {
    size_t num = 0;
    while (ptr != NULL) {
        num++;
        ptr = ptr->next;
    }
    return num;
}

size_t ta_alloc_size(void *ptr) {
    Block *block = heap->used;
    while (block != NULL) {
        if (ptr == block->addr) {
            return block->size;
        }
        block = block->next;
    }
    return 0;
}

size_t ta_num_free() {
    return count_blocks(heap->free);
}

size_t ta_num_used() {
    return count_blocks(heap->used);
}

size_t ta_num_fresh() {
    return count_blocks(heap->fresh);
}

bool ta_check() {
    return heap_max_blocks == ta_num_free() + ta_num_used() + ta_num_fresh();
}
