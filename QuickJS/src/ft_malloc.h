  /**************************************************************************
   *
   *                      MEMORY MANAGEMENT INTERFACE
   *
   *   使用 GlobalAlloc / GlobalFree / GlobalReAlloc
   *   分配可移动内存，通过 GlobalLock 获取指针。
   *   这比 C 运行时 malloc 更可靠，因为 DLL 与调用者的堆完全隔离。
   *   最初来自移植FreeType时写的内存分配
   *
   */

  void* ft_malloc( long       size );


  void ft_free( void*      block );


  void* ft_realloc( long       new_size,
                     void*      block );
