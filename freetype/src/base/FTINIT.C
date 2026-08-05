/****************************************************************************
 *
 * ftinit.c
 *
 *   FreeType initialization layer (body).
 *
 * Copyright (C) 1996-2026 by
 * David Turner, Robert Wilhelm, and Werner Lemberg.
 *
 * This file is part of the FreeType project, and may only be used,
 * modified, and distributed under the terms of the FreeType project
 * license, LICENSE.TXT.  By continuing to use, modify, or distribute
 * this file you indicate that you have read the license and
 * understand and accept it fully.
 *
 */

  /**************************************************************************
   *
   * The purpose of this file is to implement the following two
   * functions:
   *
   * FT_Add_Default_Modules():
   *   This function is used to add the set of default modules to a
   *   fresh new library object.  The set is taken from the header file
   *   `freetype/config/ftmodule.h'.  See the document `FreeType 2.0
   *   Build System' for more information.
   *
   * FT_Init_FreeType():
   *   This function creates a system object for the current platform,
   *   builds a library out of it, then calls FT_Default_Drivers().
   *
   * Note that even if FT_Init_FreeType() uses the implementation of the
   * system object defined at build time, client applications are still
   * able to provide their own `ftsystem.c'.
   *
   */


#include <ft2build.h>   
#include <debuglog.h>
#include FT_CONFIG_CONFIG_H
#include <freetype/internal/ftobjs.h>
#include <freetype/internal/ftdebug.h>
#include <freetype/ftmodapi.h>   
#include <windows.h>


  /**************************************************************************
   *
   * The macro FT_COMPONENT is used in trace mode.  It is an implicit
   * parameter of the FT_TRACE() and FT_ERROR() macros, used to print/log
   * messages during execution.
   */
#undef  FT_COMPONENT
#define FT_COMPONENT  init


#undef  FT_USE_MODULE
#ifdef __cplusplus
#define FT_USE_MODULE( type, x )  extern "C" const type  x;
#else
#define FT_USE_MODULE( type, x )  extern const type  x;
#endif

#include FT_CONFIG_MODULES_H

#undef  FT_USE_MODULE
#define FT_USE_MODULE( type, x )  (const FT_Module_Class*)&(x),

  static
  const FT_Module_Class*  const ft_default_modules[] =
  {
#include FT_CONFIG_MODULES_H
    0
  };


  /* documentation is in ftmodapi.h */

  FT_EXPORT_DEF( void ) __far __pascal
  FT_Add_Default_Modules( FT_Library  library )
  {
    FT_Error                       error;
    const FT_Module_Class* const*  cur;


    /* GCC 4.6 warns the type difference:
     *   FT_Module_Class** != const FT_Module_Class* const*
     */
    cur = (const FT_Module_Class* const*)ft_default_modules;

    /* test for valid `library' delayed to FT_Add_Module() */
    while ( *cur )
    {
      // LOG((*cur)->module_name, "Module Init");
      error = FT_Add_Module( library, *cur );
      /* notify errors, but don't stop */
      if ( error )
        FT_TRACE0(( "FT_Add_Default_Module:"
                    " Cannot install `%s', error = 0x%x\n",
                    (*cur)->module_name, error ));
      cur++;
    }
  }


#ifdef FT_CONFIG_OPTION_ENVIRONMENT_PROPERTIES

#define MAX_LENGTH  128

  /* documentation is in ftmodapi.h */

  FT_EXPORT_DEF( void ) __far __pascal
  FT_Set_Default_Properties( FT_Library  library )
  {
    const char*  env;
    const char*  p;
    const char*  q;

    char  module_name[MAX_LENGTH + 1];
    char  property_name[MAX_LENGTH + 1];
    char  property_value[MAX_LENGTH + 1];

    int  i;


    env = ft_getenv( "FREETYPE_PROPERTIES" );
    if ( !env )
      return;

    for ( p = env; *p; p++ )
    {
      /* skip leading whitespace and separators */
      if ( *p == ' ' || *p == '\t' )
        continue;

      /* read module name, followed by `:' */
      q = p;
      for ( i = 0; i < MAX_LENGTH; i++ )
      {
        if ( !*p || *p == ':' )
          break;
        module_name[i] = *p++;
      }
      module_name[i] = '\0';

      if ( !*p || *p != ':' || p == q )
        break;

      /* read property name, followed by `=' */
      q = ++p;
      for ( i = 0; i < MAX_LENGTH; i++ )
      {
        if ( !*p || *p == '=' )
          break;
        property_name[i] = *p++;
      }
      property_name[i] = '\0';

      if ( !*p || *p != '=' || p == q )
        break;

      /* read property value, followed by whitespace (if any) */
      q = ++p;
      for ( i = 0; i < MAX_LENGTH; i++ )
      {
        if ( !*p || *p == ' ' || *p == '\t' )
          break;
        property_value[i] = *p++;
      }
      property_value[i] = '\0';

      if ( !( *p == '\0' || *p == ' ' || *p == '\t' ) || p == q )
        break;

      /* we completely ignore errors */
      ft_property_string_set( library,
                              module_name,
                              property_name,
                              property_value );

      if ( !*p )
        break;
    }
  }

#else

  FT_EXPORT_DEF( void )
  FT_Set_Default_Properties( FT_Library  library )
  {
    FT_UNUSED( library );
  }

#endif


  /* documentation is in freetype.h */

  FT_EXPORT_DEF( FT_Error ) __far __pascal
  FT_Init_FreeType( FT_Library  *alibrary )
  {
    FT_Error   error;
    FT_Memory  memory;


#ifdef FT_DEBUG_LOGGING
    ft_logging_init();
#endif

    /* check of `alibrary' delayed to `FT_New_Library' */

    /* First of all, allocate a new system object -- this function is part */
    /* of the system-specific component, i.e. `ftsystem.c'.                */
    
    memory = FT_New_Memory();
    if ( !memory )
    {
      FT_ERROR(( "FT_Init_FreeType: cannot find memory manager\n" ));
      return FT_THROW( Unimplemented_Feature );
    }
    
    /* build a library out of it, then fill it with the set of */
    /* default drivers.                                        */
    
    error = FT_New_Library( memory, alibrary );
    if ( error )
      FT_Done_Memory( memory );
    else
      FT_Add_Default_Modules( *alibrary );

    FT_Set_Default_Properties( *alibrary );

    return error;
  }
  
  FT_EXPORT_DEF( FT_Library ) __far __pascal
  FT_Init_FreeType16( void )
  {
    FT_Memory  memory;
    FT_Library library = NULL;
    FT_Error   error;                           
    
    /* 分配并初始化内存管理器 */
    memory = FT_New_Memory();
    if ( !memory )
    {
      FT_ERROR(( "FT_Init_FreeType16: cannot find memory manager\n" ));
      return NULL;
    }
   
    /* 在 DLL 栈上分配 library 结构体 */
    error = FT_New_Library_Internal( memory, &library );
    if ( error )
    {
      FT_Done_Memory( memory );
      return NULL;
    }                               
 
    FT_Add_Default_Modules( library );
    FT_Set_Default_Properties( library );    
    
    /*{
    	char buf[256];
    	unsigned long libAddr = (unsigned long)library;
    	unsigned int seg, off, ds_val;
    	seg = (unsigned int)(libAddr >> 16);
    	off = (unsigned int)(libAddr & 0xFFFF);
    	__asm{
    		mov ds_val, ds
    	}
    	
    	if (IsBadReadPtr(library, sizeof(FT_LibraryRec)))
    		LOG("Library pointer is BAD READ!", "DEBUG");
    	if (IsBadReadPtr(&library->memory, sizeof(FT_Memory)))
    		LOG("library->memory is BAD READ!", "DEBUG");
    	wsprintf(buf, 
    		"DEBUG :FT_Init_FreeType16\n"
    		"library ptr = %04X:%04X\n"
    		"DS = &04X\n"
    		"sizeof(FT_LibraryRec) = %u",
    		seg, off, ds_val,
    		sizeof(FT_LibraryRec));
    	LOG(buf, "DEBUG");
    }*/

    return library;  /* 通过 DX:AX 寄存器返回，不写入调用者内存 */
  }


  /* documentation is in freetype.h */

  FT_EXPORT_DEF( FT_Error ) __far __pascal
  FT_Done_FreeType( FT_Library  library )
  {
    FT_Memory  memory;


    if ( !library )
      return FT_THROW( Invalid_Library_Handle );

    memory = library->memory;

    /* Discard the library object */
    FT_Done_Library( library );

    /* discard memory manager */
    FT_Done_Memory( memory );

#ifdef FT_DEBUG_LOGGING
    ft_logging_deinit();
#endif

    return FT_Err_Ok;
  }


/* END */
