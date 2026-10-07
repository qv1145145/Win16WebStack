/* ftapi16.c - Win16 安全 API 包装层
 *
 * 所有需要向调用者返回指针的 FreeType API，
 * 在此包装为直接返回指针值（失败时返回 NULL），
 * 从而避免 DLL 向调用者栈段跨段写入导致的 GPF。
 */

#include <freetype/freetype.h>
#include <freetype/ftglyph.h>
#include <freetype/ftoutln.h>
#include <freetype/ftmodapi.h>
#include <freetype/ftsnames.h>
#include <freetype/internal/ftstream.h>  
#include <windows.h>           

typedef unsigned long ULONG;

/* ========== 声明 ========== */                              
FT_EXPORT_DEF( FT_ULong ) __far _pascal
FT_Get_CMap_Language_ID( FT_CharMap charmap );

FT_EXPORT_DEF( FT_Long ) __far _pascal
FT_Get_CMap_Format( FT_CharMap charmap );

/* ========== Face 创建 ========== */

FT_EXPORT_DEF( FT_Face ) __far __pascal
FT_New_Face16( FT_Library   library,
               const char*  pathname,
               FT_Long      face_index )
{
    FT_Face  face = NULL;
    FT_Error error;

    error = FT_New_Face( library, pathname, face_index, &face );
    if ( error )
        return NULL;

    return face;
}


FT_EXPORT_DEF( FT_Face ) __far __pascal
FT_New_Memory_Face16( FT_Library      library,
                      const FT_Byte*  file_base,
                      FT_Long         file_size,
                      FT_Long         face_index )
{
    FT_Face  face = NULL;
    FT_Error error;

    error = FT_New_Memory_Face( library, file_base, file_size,
                                face_index, &face );
    if ( error )
        return NULL;

    return face;
}


FT_EXPORT_DEF( FT_Face ) __far __pascal
FT_Open_Face16( FT_Library     				library,
                const FT_Open_Args __far*   args,
                FT_Long        				face_index )
{                                    
    FT_Face       face = NULL;
    FT_Error      error;     

    /* 传入全局分配的副本，所有访问都在 DLL 自己的内存段内进行 */
    error = FT_Open_Face( library, args, face_index, &face );          

    if ( error )
        return NULL;

    return face;
}



/* ========== Glyph ========== */

FT_EXPORT_DEF( FT_Glyph ) __far __pascal
FT_Get_Glyph16( FT_GlyphSlot  slot )
{
    FT_Glyph  glyph = NULL;
    FT_Error  error;

    error = FT_Get_Glyph( slot, &glyph );
    if ( error )
        return NULL;

    return glyph;
}


/* ========== 模块 ========== */

FT_EXPORT_DEF( FT_Module ) __far __pascal
FT_Get_Module16( FT_Library   library,
                 const char*  module_name )
{
    FT_Module  module = NULL;

    module = FT_Get_Module( library, module_name );
    return module;
}


/* ========== 字符枚举 ========== */

FT_EXPORT_DEF( FT_ULong ) __far __pascal
FT_Get_First_Char16( FT_Face   face,
                     FT_UInt  *agindex )
{
    FT_ULong  charcode;
    FT_UInt   gindex = 0;

    charcode = FT_Get_First_Char( face, &gindex );
    if ( agindex )
        *agindex = gindex;

    return charcode;
}


FT_EXPORT_DEF( FT_ULong ) __far __pascal
FT_Get_Next_Char16( FT_Face    face,
                    FT_ULong   charcode,
                    FT_UInt   *agindex )
{
    FT_UInt  gindex = 0;

    charcode = FT_Get_Next_Char( face, charcode, &gindex );
    if ( agindex )
        *agindex = gindex;

    return charcode;
}


/* ========== CMAP 属性 ========== */

FT_EXPORT_DEF( FT_ULong ) __far __pascal
FT_Get_CMap_Language_ID16( FT_CharMap  charmap )
{
    FT_ULong  lang_id = 0;

    FT_Get_CMap_Language_ID( charmap, &lang_id );
    return lang_id;
}


FT_EXPORT_DEF( FT_Long ) __far __pascal
FT_Get_CMap_Format16( FT_CharMap  charmap )
{
    FT_Long  format = 0;

    FT_Get_CMap_Format( charmap, &format );
    return format;
}


/* ========== Stream（内部使用，不导出到 .def） ========== */

FT_BASE_DEF( FT_Stream ) __far __pascal
FT_Stream_New16( FT_Library           library,
                 const FT_Open_Args*  args )
{
    FT_Error   error;
    FT_Stream  stream = NULL;
    FT_Memory  memory = library->memory;

    error = FT_Stream_New( library, memory, args, &stream );
    if ( error )
        return NULL;

    return stream;
}