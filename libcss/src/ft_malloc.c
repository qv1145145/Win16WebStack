  /**************************************************************************
   *
   *                      MEMORY MANAGEMENT INTERFACE
   *
   *   使用 GlobalAlloc / GlobalFree / GlobalReAlloc
   *   分配可移动内存，通过 GlobalLock 获取指针。
   *   这比 C 运行时 malloc 更可靠，因为 DLL 与调用者的堆完全隔离。
   *   最初来自移植FreeType时写的内存分配
   */
#include "ft_malloc.h"

  void* ft_malloc( long size )
  {
    HGLOBAL hMem;

    /* 使用 GPTR (= GMEM_FIXED | GMEM_ZEROINIT)，确保段可写 */
    hMem = GlobalAlloc( GPTR, (DWORD)size );
    if ( !hMem )
      return NULL;

    /* 即使固定内存，也通过 GlobalLock 获取规范化的 far 指针 */
    return GlobalLock( hMem );
  }


  void ft_free( void* block )
  {
    HGLOBAL hMem;

    if ( block == NULL )
      return;

    /* 通过 GlobalHandle 反查句柄（文档标准做法） */
    hMem = GlobalHandle( HIWORD( (UINT)block ) );
    if ( hMem )
    {
      GlobalUnlock( hMem );      /* 对应 ft_alloc 中的 GlobalLock */
      GlobalFree( hMem );        /* 传入句柄释放 */
    }
  }


  void* ft_realloc( long       new_size,
                    void*      block )
  {
    DWORD               dwHandle;
    HGLOBAL hMem;

    if ( block == NULL )
      return ft_malloc( new_size );

    if ( new_size <= 0 )
    {
      ft_free( block );
      return NULL;
    }

    dwHandle = GlobalHandle( HIWORD( (UINT)block ) );
    if ( !hMem )
      return NULL;

    hMem = (HGLOBAL)LOWORD( dwHandle );

    GlobalUnlock( hMem );                                           /* 先解锁 */
    hMem = GlobalReAlloc( hMem, (DWORD)new_size, GMEM_ZEROINIT );   /* 重新分配 */
    if ( !hMem )
      return NULL;

    return GlobalLock( hMem );                                      /* 重新锁定 */
  }
