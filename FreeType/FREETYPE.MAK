# Microsoft Visual C++ generated build script - Do not modify

PROJ = FREETYPE
DEBUG = 1
PROGTYPE = 1
CALLER = 
ARGS = 
DLLS = 
D_RCDEFINES = -d_DEBUG
R_RCDEFINES = -dNDEBUG
ORIGIN = MSVC
ORIGIN_VER = 1.00
PROJPATH = D:\PROJECT\FREETYPE\
USEMFC = 0
CC = cl
CPP = cl
CXX = cl
CCREATEPCHFLAG = 
CPPCREATEPCHFLAG = 
CUSEPCHFLAG = 
CPPUSEPCHFLAG = 
FIRSTC = FTDEBUG.C   
FIRSTCPP =             
RC = rc
CFLAGS_D_WDLL = /nologo /Gs /G2 /Zp2 /Gy /Zi /ALw /Od /D "_DEBUG" /D "FT2_BUILD_LIBRARY" /I "d:\project\freetype" /GD /GEf /Fd"FREETYPE.PDB"
CFLAGS_R_WDLL = /nologo /G2 /W0 /ALw /O1 /D "NDEBUG" /I "d:\project\freetype" /FR /GD 
LFLAGS_D_WDLL = /NOLOGO /NOD /NOE /PACKC:61440 /ALIGN:16 /ONERROR:NOEXE /CO /MAP /LINE /MAP:FULL
LFLAGS_R_WDLL = /NOLOGO /NOD /NOE /PACKC:61440 /ALIGN:16 /ONERROR:NOEXE /MAP:FULL
LIBS_D_WDLL = oldnames libw ldllcew commdlg.lib olecli.lib olesvr.lib shell.lib 
LIBS_R_WDLL = oldnames libw ldllcew commdlg.lib olecli.lib olesvr.lib shell.lib 
RCFLAGS = /nologo
RESFLAGS = /nologo
RUNFLAGS = 
DEFFILE = FREETYPE.DEF
OBJS_EXT = 
LIBS_EXT = 
!if "$(DEBUG)" == "1"
CFLAGS = $(CFLAGS_D_WDLL)
LFLAGS = $(LFLAGS_D_WDLL)
LIBS = $(LIBS_D_WDLL)
MAPFILE = nul
RCDEFINES = $(D_RCDEFINES)
!else
CFLAGS = $(CFLAGS_R_WDLL)
LFLAGS = $(LFLAGS_R_WDLL)
LIBS = $(LIBS_R_WDLL)
MAPFILE = nul
RCDEFINES = $(R_RCDEFINES)
!endif
!if [if exist MSVC.BND del MSVC.BND]
!endif
SBRS = FTDEBUG.SBR \
		FTSYSTEM.SBR \
		AUTOFIT.SBR \
		FTBASE.SBR \
		PSAUX.SBR \
		PSNAMES.SBR \
		RASTER.SBR \
		SFNT.SBR \
		SMOOTH.SBR \
		FTGZIP.SBR \
		FTCBASIC.SBR \
		FTCCACHE.SBR \
		FTCCMAP.SBR \
		FTCGLYPH.SBR \
		FTCIMAGE.SBR \
		FTCMANAG.SBR \
		FTCMRU.SBR \
		FTCSBITS.SBR \
		TTDRIVER.SBR \
		TTGLOAD.SBR \
		TTGXVAR.SBR \
		TTINTERP.SBR \
		TTOBJS.SBR \
		TTPLOAD.SBR \
		FTBITMAP.SBR \
		FTGLYPH.SBR \
		FTBBOX.SBR \
		FTAPI16.SBR


FTDEBUG_DEP = d:\project\freetype\freetype\freetype.h \
	d:\project\freetype\ft2build.h \
	d:\project\freetype\freetype\config\ftheader.h \
	d:\project\freetype\freetype\config\ftconfig.h \
	d:\project\freetype\freetype\config\ftoption.h \
	d:\project\freetype\freetype\config\ftstdlib.h \
	d:\project\freetype\freetype\config\intege~1.h \
	d:\project\freetype\freetype\config\public~1.h \
	d:\project\freetype\freetype\config\mac-su~1.h \
	d:\project\freetype\freetype\fttypes.h \
	freetype/ftsystem.h \
	freetype/ftimage.h \
	freetype/fterrors.h \
	freetype/ftloggin.h \
	freetype/internal/ftdebug.h \
	freetype/internal/ftobjs.h \
	freetype/internal/fttrace.h


FTSYSTEM_DEP = freetype/internal/ftdebug.h \
	freetype/internal/ftstream.h \
	freetype/ftsystem.h \
	freetype/fterrors.h \
	freetype/fttypes.h


AUTOFIT_DEP = d:\project\freetype\src\autofit\ft-hb.c \
	freetype/freetype.h \
	freetype/internal/ftmemory.h \
	d:\project\freetype\src\autofit\afglobal.h \
	d:\project\freetype\src\autofit\aftypes.h \
	freetype/ftoutln.h \
	freetype/internal/fthash.h \
	freetype/internal/ftobjs.h \
	freetype/internal/ftdebug.h \
	d:\project\freetype\src\autofit\afblue.h \
	d:\project\freetype\src\autofit\afws-i~1.h \
	d:\project\freetype\src\autofit\afscript.h \
	d:\project\freetype\src\autofit\afcover.h \
	d:\project\freetype\src\autofit\afstyles.h \
	d:\project\freetype\src\autofit\afmodule.h \
	freetype/ftmodapi.h \
	d:\project\freetype\src\autofit\ft-hb.h \
	freetype/internal/compiler.h \
	d:\project\freetype\src\autofit\afshaper.h \
	d:\project\freetype\src\autofit\ft-hb-ft.c \
	freetype/tttables.h \
	d:\project\freetype\src\autofit\ft-hb-ft.h \
	d:\project\freetype\src\autofit\afadjust.c \
	d:\project\freetype\src\autofit\afadjust.h \
	freetype/fttypes.h \
	d:\project\freetype\src\autofit\afgsub.h \
	d:\project\freetype\src\autofit\afblue.c \
	d:\project\freetype\src\autofit\afcjk.c \
	freetype/ftadvanc.h \
	d:\project\freetype\src\autofit\aflatin.h \
	d:\project\freetype\src\autofit\afhints.h \
	d:\project\freetype\src\autofit\afcjk.h \
	d:\project\freetype\src\autofit\aferrors.h \
	freetype/ftmoderr.h \
	freetype/fterrors.h \
	d:\project\freetype\src\autofit\afdummy.c \
	d:\project\freetype\src\autofit\afdummy.h \
	d:\project\freetype\src\autofit\afglobal.c \
	d:\project\freetype\src\autofit\afranges.h \
	d:\project\freetype\src\autofit\afws-d~1.h \
	d:\project\freetype\src\autofit\afindic.h \
	d:\project\freetype\src\autofit\afgsub.c \
	freetype/tttags.h \
	freetype/internal/ftstream.h \
	d:\project\freetype\src\autofit\afhints.c \
	freetype/internal/ftcalc.h \
	d:\project\freetype\src\autofit\afindic.c \
	d:\project\freetype\src\autofit\aflatin.c \
	d:\project\freetype\src\autofit\afloader.c \
	d:\project\freetype\src\autofit\afloader.h \
	d:\project\freetype\src\autofit\afmodule.c \
	freetype/ftdriver.h \
	freetype/internal/services/svprop.h \
	d:\project\freetype\src\autofit\afranges.c \
	d:\project\freetype\src\autofit\afshaper.c


FTBASE_DEP = d:\project\freetype\src\base\ftadvanc.c \
	freetype/internal/ftdebug.h \
	freetype/ftadvanc.h \
	freetype/internal/ftobjs.h \
	freetype/internal/ftcalc.h \
	d:\project\freetype\src\base\ftcolor.c \
	freetype/internal/sfnt.h \
	freetype/internal/tttypes.h \
	freetype/ftcolor.h \
	d:\project\freetype\src\base\ftdbgmem.c \
	freetype/internal/ftmemory.h \
	freetype/ftsystem.h \
	freetype/fterrors.h \
	freetype/fttypes.h \
	d:\project\freetype\src\base\fterrors.c \
	d:\project\freetype\src\base\ftfntfmt.c \
	freetype/ftfntfmt.h \
	freetype/internal/services/svfntfmt.h \
	d:\project\freetype\src\base\ftgloadr.c \
	freetype/internal/ftgloadr.h \
	d:\project\freetype\src\base\fthash.c \
	freetype/internal/fthash.h \
	d:\project\freetype\src\base\ftinit.c \
	freetype/ftmodapi.h \
	d:\project\freetype\src\base\ftlcdfil.c \
	freetype/ftlcdfil.h \
	freetype/ftimage.h \
	d:\project\freetype\src\base\ftobjs.c \
	freetype/ftlist.h \
	freetype/ftoutln.h \
	freetype/otsvg.h \
	freetype/internal/ftvalid.h \
	freetype/internal/ftrfork.h \
	freetype/internal/ftstream.h \
	freetype/internal/psaux.h \
	freetype/internal/svgint~1.h \
	freetype/tttables.h \
	freetype/tttags.h \
	freetype/ttnameid.h \
	freetype/internal/services/svprop.h \
	freetype/internal/services/svsfnt.h \
	freetype/internal/services/svpostnm.h \
	freetype/internal/services/svgldict.h \
	freetype/internal/services/svttcmap.h \
	freetype/internal/services/svkern.h \
	freetype/internal/services/svtteng.h \
	freetype/ftdriver.h \
	d:\project\freetype\src\base\ftbase.h \
	freetype/ftbitmap.h \
	d:\project\freetype\src\base\md5.c \
	d:\project\freetype\src\base\md5.h \
	openssl/md5.h \
	d:\project\freetype\src\base\ftoutln.c \
	freetype/fttrigon.h \
	d:\project\freetype\src\base\ftpsprop.c \
	freetype/internal/ftpsprop.h \
	d:\project\freetype\src\base\ftrfork.c \
	d:\project\freetype\src\base\ftsnames.c \
	freetype/ftsnames.h \
	d:\project\freetype\src\base\ftstream.c \
	d:\project\freetype\src\base\fttrigon.c \
	d:\project\freetype\src\base\ftcalc.c \
	freetype/ftglyph.h \
	d:\project\freetype\src\base\ftutil.c


PSAUX_DEP = d:\project\freetype\src\psaux\afmparse.c \
	freetype/freetype.h \
	freetype/internal/ftdebug.h \
	freetype/internal/psaux.h \
	d:\project\freetype\src\psaux\afmparse.h \
	d:\project\freetype\src\psaux\psconv.h \
	d:\project\freetype\src\psaux\psauxerr.h \
	freetype/ftmoderr.h \
	freetype/fterrors.h \
	d:\project\freetype\src\psaux\psauxmod.c \
	d:\project\freetype\src\psaux\psauxmod.h \
	freetype/ftmodapi.h \
	d:\project\freetype\src\psaux\psobjs.h \
	freetype/internal/cffotype.h \
	d:\project\freetype\src\psaux\t1decode.h \
	freetype/internal/t1types.h \
	d:\project\freetype\src\psaux\t1cmap.h \
	freetype/internal/ftobjs.h \
	d:\project\freetype\src\psaux\psft.h \
	freetype/internal/compiler.h \
	d:\project\freetype\src\psaux\pstypes.h \
	freetype/ftsystem.h \
	d:\project\freetype\src\psaux\psglue.h \
	d:\project\freetype\src\psaux\pserror.h \
	d:\project\freetype\src\psaux\psfixed.h \
	d:\project\freetype\src\psaux\psarrst.h \
	d:\project\freetype\src\psaux\psread.h \
	d:\project\freetype\src\psaux\cffdec~1.h \
	d:\project\freetype\src\psaux\psconv.c \
	d:\project\freetype\src\psaux\psobjs.c \
	freetype/internal/ftcalc.h \
	freetype/ftdriver.h \
	d:\project\freetype\src\psaux\t1cmap.c \
	d:\project\freetype\src\psaux\t1decode.c \
	freetype/internal/pshints.h \
	freetype/internal/fthash.h \
	freetype/ftoutln.h \
	d:\project\freetype\src\psaux\cffdec~1.c \
	freetype/internal/ftserv.h \
	freetype/internal/services/svcfftl.h \
	d:\project\freetype\src\psaux\psarrst.c \
	d:\project\freetype\src\psaux\psblues.c \
	d:\project\freetype\src\psaux\psblues.h \
	d:\project\freetype\src\psaux\pshints.h \
	d:\project\freetype\src\psaux\psfont.h \
	d:\project\freetype\src\psaux\pserror.c \
	d:\project\freetype\src\psaux\psfont.c \
	d:\project\freetype\src\psaux\psintrp.h \
	d:\project\freetype\src\psaux\psft.c \
	freetype/ftmm.h \
	freetype/internal/services/svmm.h \
	d:\project\freetype\src\psaux\pshints.c \
	d:\project\freetype\src\psaux\psintrp.c \
	d:\project\freetype\src\psaux\psstack.h \
	d:\project\freetype\src\psaux\psread.c \
	d:\project\freetype\src\psaux\psstack.c


PSNAMES_DEP = d:\project\freetype\src\psnames\psmodule.c \
	freetype/internal/ftdebug.h \
	freetype/internal/ftobjs.h \
	freetype/internal/services/svpscmap.h \
	d:\project\freetype\src\psnames\psmodule.h \
	freetype/ftmodapi.h \
	d:\project\freetype\src\psnames\pstables.h \
	d:\project\freetype\src\psnames\psnamerr.h \
	freetype/ftmoderr.h \
	freetype/fterrors.h


RASTER_DEP = d:\project\freetype\src\raster\ftraster.c \
	d:\project\freetype\src\raster\ftmisc.h \
	d:\project\freetype\src\raster\ftraster.h \
	freetype/ftimage.h \
	freetype/internal/compiler.h \
	freetype/internal/ftcalc.h \
	freetype/internal/ftobjs.h \
	freetype/internal/ftdebug.h \
	d:\project\freetype\src\raster\rasterrs.h \
	freetype/ftmoderr.h \
	freetype/fterrors.h \
	d:\project\freetype\src\raster\ftrend1.c \
	freetype/ftoutln.h \
	d:\project\freetype\src\raster\ftrend1.h \
	freetype/ftrender.h


SFNT_DEP = d:\project\freetype\src\sfnt\pngshim.c \
	freetype/internal/ftdebug.h \
	freetype/internal/ftstream.h \
	freetype/tttags.h \
	d:\project\freetype\src\sfnt\pngshim.h \
	d:\project\freetype\src\sfnt\ttload.h \
	freetype/internal/tttypes.h \
	d:\project\freetype\src\sfnt\sferrors.h \
	freetype/ftmoderr.h \
	freetype/fterrors.h \
	d:\project\freetype\src\sfnt\sfdriver.c \
	freetype/internal/sfnt.h \
	freetype/internal/ftobjs.h \
	freetype/ttnameid.h \
	d:\project\freetype\src\sfnt\sfdriver.h \
	freetype/ftmodapi.h \
	d:\project\freetype\src\sfnt\sfobjs.h \
	d:\project\freetype\src\sfnt\ttsbit.h \
	d:\project\freetype\src\sfnt\ttcolr.h \
	d:\project\freetype\src\sfnt\ttcpal.h \
	d:\project\freetype\src\sfnt\ttsvg.h \
	d:\project\freetype\src\sfnt\ttpost.h \
	d:\project\freetype\src\sfnt\ttbdf.h \
	freetype/ftbdf.h \
	freetype/internal/services/svbdf.h \
	d:\project\freetype\src\sfnt\ttgpos.h \
	d:\project\freetype\src\sfnt\ttcmap.h \
	freetype/internal/ftvalid.h \
	freetype/internal/services/svttcmap.h \
	d:\project\freetype\src\sfnt\ttcmapc.h \
	d:\project\freetype\src\sfnt\ttkern.h \
	d:\project\freetype\src\sfnt\ttmtx.h \
	freetype/internal/services/svgldict.h \
	freetype/internal/services/svpostnm.h \
	freetype/internal/services/svsfnt.h \
	freetype/ftmm.h \
	freetype/internal/services/svmm.h \
	d:\project\freetype\src\sfnt\sfobjs.c \
	d:\project\freetype\src\sfnt\sfwoff.h \
	d:\project\freetype\src\sfnt\sfwoff2.h \
	freetype/internal/services/svpscmap.h \
	freetype/ftsnames.h \
	freetype/internal/services/svmetric.h \
	d:\project\freetype\src\sfnt\sfwoff.c \
	freetype/internal/ftcalc.h \
	freetype/ftgzip.h \
	d:\project\freetype\src\sfnt\sfwoff2.c \
	d:\project\freetype\src\sfnt\woff2t~1.h \
	freetype/internal/compiler.h \
	brotli/decode.h \
	d:\project\freetype\src\sfnt\ttbdf.c \
	d:\project\freetype\src\sfnt\ttcmap.c \
	d:\project\freetype\src\sfnt\ttcolr.c \
	freetype/ftcolor.h \
	freetype/config/integer-.h \
	d:\project\freetype\src\sfnt\ttcpal.c \
	d:\project\freetype\src\sfnt\ttsvg.c \
	freetype/otsvg.h \
	d:\project\freetype\src\sfnt\ttgpos.c \
	freetype/freetype.h \
	freetype/tttables.h \
	d:\project\freetype\src\sfnt\ttkern.c \
	d:\project\freetype\src\sfnt\ttload.c \
	d:\project\freetype\src\sfnt\ttmtx.c \
	d:\project\freetype\src\sfnt\ttpost.c \
	d:\project\freetype\src\sfnt\ttsbit.c \
	freetype/ftbitmap.h \
	d:\project\freetype\src\sfnt\woff2t~1.c


SMOOTH_DEP = d:\project\freetype\src\smooth\ftgrays.c \
	d:\project\freetype\src\smooth\ftgrays.h \
	freetype/ftimage.h \
	freetype/internal/ftobjs.h \
	freetype/internal/ftdebug.h \
	freetype/internal/ftcalc.h \
	freetype/ftoutln.h \
	d:\project\freetype\src\smooth\ftsmerrs.h \
	freetype/ftmoderr.h \
	freetype/fterrors.h \
	d:\project\freetype\src\smooth\ftsmooth.c \
	d:\project\freetype\src\smooth\ftsmooth.h \
	freetype/ftrender.h


FTGZIP_DEP = freetype/internal/ftmemory.h \
	freetype/internal/ftstream.h \
	freetype/internal/ftdebug.h \
	freetype/ftgzip.h \
	freetype/ftmoderr.h \
	freetype/fterrors.h \
	d:\project\freetype\src\gzip\zutil.c \
	d:\project\freetype\src\gzip\zutil.h \
	d:\project\freetype\src\gzip\zlib.h \
	d:\project\freetype\src\gzip\ftzconf.h \
	d:\project\freetype\src\gzip\gzguts.h \
	d:\project\freetype\src\gzip\inffast.c \
	d:\project\freetype\src\gzip\inftrees.h \
	d:\project\freetype\src\gzip\inflate.h \
	d:\project\freetype\src\gzip\inffast.h \
	d:\project\freetype\src\gzip\inflate.c \
	d:\project\freetype\src\gzip\inffixed.h \
	d:\project\freetype\src\gzip\inftrees.c \
	d:\project\freetype\src\gzip\adler32.c \
	d:\project\freetype\src\gzip\crc32.c \
	d:\project\freetype\src\gzip\crc32.h


FTCBASIC_DEP = freetype/internal/ftobjs.h \
	freetype/internal/ftdebug.h \
	freetype/ftcache.h \
	d:\project\freetype\src\cache\ftcglyph.h \
	d:\project\freetype\src\cache\ftcmanag.h \
	d:\project\freetype\src\cache\ftcmru.h \
	freetype/freetype.h \
	freetype/internal/compiler.h \
	d:\project\freetype\src\cache\ftccache.h \
	d:\project\freetype\src\cache\ftcimage.h \
	d:\project\freetype\src\cache\ftcsbits.h \
	d:\project\freetype\src\cache\ftccback.h \
	d:\project\freetype\src\cache\ftcerror.h \
	freetype/ftmoderr.h \
	freetype/fterrors.h


FTCCACHE_DEP = d:\project\freetype\src\cache\ftcmanag.h \
	freetype/ftcache.h \
	d:\project\freetype\src\cache\ftcmru.h \
	freetype/freetype.h \
	freetype/internal/compiler.h \
	d:\project\freetype\src\cache\ftccache.h \
	freetype/internal/ftobjs.h \
	freetype/internal/ftdebug.h \
	d:\project\freetype\src\cache\ftccback.h \
	d:\project\freetype\src\cache\ftcerror.h \
	freetype/ftmoderr.h \
	freetype/fterrors.h


FTCCMAP_DEP = freetype/freetype.h \
	freetype/ftcache.h \
	d:\project\freetype\src\cache\ftcmanag.h \
	d:\project\freetype\src\cache\ftcmru.h \
	freetype/internal/compiler.h \
	d:\project\freetype\src\cache\ftccache.h \
	freetype/internal/ftmemory.h \
	freetype/internal/ftobjs.h \
	freetype/internal/ftdebug.h \
	d:\project\freetype\src\cache\ftccback.h \
	d:\project\freetype\src\cache\ftcerror.h \
	freetype/ftmoderr.h \
	freetype/fterrors.h


FTCGLYPH_DEP = freetype/internal/ftobjs.h \
	freetype/ftcache.h \
	d:\project\freetype\src\cache\ftcglyph.h \
	d:\project\freetype\src\cache\ftcmanag.h \
	d:\project\freetype\src\cache\ftcmru.h \
	freetype/freetype.h \
	freetype/internal/compiler.h \
	d:\project\freetype\src\cache\ftccache.h \
	freetype/fterrors.h \
	d:\project\freetype\src\cache\ftccback.h \
	d:\project\freetype\src\cache\ftcerror.h \
	freetype/ftmoderr.h


FTCIMAGE_DEP = freetype/ftcache.h \
	d:\project\freetype\src\cache\ftcimage.h \
	d:\project\freetype\src\cache\ftcglyph.h \
	freetype/internal/ftmemory.h \
	freetype/internal/ftobjs.h \
	d:\project\freetype\src\cache\ftccback.h \
	d:\project\freetype\src\cache\ftccache.h \
	d:\project\freetype\src\cache\ftcerror.h \
	freetype/ftmoderr.h \
	freetype/fterrors.h


FTCMANAG_DEP = freetype/ftcache.h \
	d:\project\freetype\src\cache\ftcmanag.h \
	d:\project\freetype\src\cache\ftcmru.h \
	freetype/freetype.h \
	freetype/internal/compiler.h \
	d:\project\freetype\src\cache\ftccache.h \
	freetype/internal/ftobjs.h \
	freetype/internal/ftdebug.h \
	freetype/ftsizes.h \
	d:\project\freetype\src\cache\ftcerror.h \
	freetype/ftmoderr.h \
	freetype/fterrors.h


FTCMRU_DEP = freetype/ftcache.h \
	d:\project\freetype\src\cache\ftcmru.h \
	freetype/freetype.h \
	freetype/internal/compiler.h \
	freetype/internal/ftobjs.h \
	freetype/internal/ftdebug.h \
	d:\project\freetype\src\cache\ftcerror.h \
	freetype/ftmoderr.h \
	freetype/fterrors.h


FTCSBITS_DEP = freetype/ftcache.h \
	d:\project\freetype\src\cache\ftcsbits.h \
	d:\project\freetype\src\cache\ftcglyph.h \
	freetype/internal/ftobjs.h \
	freetype/internal/ftdebug.h \
	freetype/fterrors.h \
	d:\project\freetype\src\cache\ftccback.h \
	d:\project\freetype\src\cache\ftccache.h \
	d:\project\freetype\src\cache\ftcerror.h \
	freetype/ftmoderr.h


TTDRIVER_DEP = freetype/internal/ftdebug.h \
	freetype/internal/ftstream.h \
	freetype/internal/sfnt.h \
	freetype/internal/services/svfntfmt.h \
	freetype/ftmm.h \
	freetype/internal/services/svmm.h \
	freetype/internal/services/svmetric.h \
	freetype/internal/services/svtteng.h \
	freetype/internal/services/svttglyf.h \
	freetype/internal/services/svprop.h \
	freetype/ftdriver.h \
	d:\project\freetype\src\truetype\ttdriver.h \
	freetype/internal/ftdrv.h \
	d:\project\freetype\src\truetype\ttgload.h \
	d:\project\freetype\src\truetype\ttobjs.h \
	freetype/internal/ftobjs.h \
	freetype/internal/tttypes.h \
	d:\project\freetype\src\truetype\ttinterp.h \
	d:\project\freetype\src\truetype\ttpload.h \
	d:\project\freetype\src\truetype\ttgxvar.h \
	freetype/internal/ftmmtype.h \
	d:\project\freetype\src\truetype\tterrors.h \
	freetype/ftmoderr.h \
	freetype/fterrors.h


TTGLOAD_DEP = freetype/internal/ftdebug.h \
	freetype/internal/ftcalc.h \
	freetype/internal/ftstream.h \
	freetype/internal/sfnt.h \
	freetype/tttags.h \
	freetype/ftoutln.h \
	freetype/ftdriver.h \
	freetype/ftlist.h \
	d:\project\freetype\src\truetype\ttgload.h \
	d:\project\freetype\src\truetype\ttobjs.h \
	freetype/internal/ftobjs.h \
	freetype/internal/tttypes.h \
	d:\project\freetype\src\truetype\ttinterp.h \
	d:\project\freetype\src\truetype\ttpload.h \
	d:\project\freetype\src\truetype\ttgxvar.h \
	freetype/internal/ftmmtype.h \
	d:\project\freetype\src\truetype\tterrors.h \
	freetype/ftmoderr.h \
	freetype/fterrors.h


TTGXVAR_DEP = freetype/internal/ftdebug.h \
	freetype/internal/ftcalc.h \
	freetype/internal/ftstream.h \
	freetype/internal/sfnt.h \
	freetype/internal/services/svmetric.h \
	freetype/tttags.h \
	freetype/ttnameid.h \
	freetype/ftmm.h \
	freetype/ftlist.h \
	d:\project\freetype\src\truetype\ttpload.h \
	freetype/internal/tttypes.h \
	d:\project\freetype\src\truetype\ttgxvar.h \
	freetype/internal/ftmmtype.h \
	d:\project\freetype\src\truetype\ttobjs.h \
	d:\project\freetype\src\truetype\tterrors.h \
	freetype/ftmoderr.h \
	freetype/fterrors.h


TTINTERP_DEP = freetype/internal/ftdebug.h \
	freetype/internal/ftcalc.h \
	freetype/fttrigon.h \
	freetype/ftsystem.h \
	freetype/ftdriver.h \
	freetype/ftmm.h \
	d:\project\freetype\src\truetype\ttinterp.h \
	d:\project\freetype\src\truetype\ttobjs.h \
	d:\project\freetype\src\truetype\tterrors.h \
	freetype/ftmoderr.h \
	freetype/fterrors.h \
	d:\project\freetype\src\truetype\ttgxvar.h \
	freetype/internal/ftmmtype.h


TTOBJS_DEP = freetype/internal/ftdebug.h \
	freetype/internal/ftstream.h \
	freetype/tttags.h \
	freetype/internal/sfnt.h \
	freetype/ftdriver.h \
	d:\project\freetype\src\truetype\ttgload.h \
	d:\project\freetype\src\truetype\ttobjs.h \
	freetype/internal/ftobjs.h \
	freetype/internal/tttypes.h \
	d:\project\freetype\src\truetype\ttinterp.h \
	d:\project\freetype\src\truetype\ttpload.h \
	d:\project\freetype\src\truetype\tterrors.h \
	freetype/ftmoderr.h \
	freetype/fterrors.h \
	d:\project\freetype\src\truetype\ttgxvar.h \
	freetype/internal/ftmmtype.h


TTPLOAD_DEP = freetype/internal/ftdebug.h \
	freetype/internal/ftobjs.h \
	freetype/internal/ftstream.h \
	freetype/tttags.h \
	d:\project\freetype\src\truetype\ttpload.h \
	freetype/internal/tttypes.h \
	d:\project\freetype\src\truetype\ttgxvar.h \
	freetype/internal/ftmmtype.h \
	d:\project\freetype\src\truetype\ttobjs.h \
	d:\project\freetype\src\truetype\tterrors.h \
	freetype/ftmoderr.h \
	freetype/fterrors.h


FTBITMAP_DEP = freetype/internal/ftdebug.h \
	freetype/ftbitmap.h \
	freetype/ftimage.h \
	freetype/internal/ftobjs.h


FTGLYPH_DEP = freetype/internal/ftdebug.h \
	freetype/ftglyph.h \
	freetype/ftoutln.h \
	freetype/ftbitmap.h \
	freetype/internal/ftobjs.h \
	freetype/otsvg.h \
	d:\project\freetype\src\base\ftbase.h


FTBBOX_DEP = freetype/internal/ftdebug.h \
	freetype/ftbbox.h \
	freetype/ftimage.h \
	freetype/ftoutln.h \
	freetype/internal/ftcalc.h \
	freetype/internal/ftobjs.h


FTAPI16_DEP = freetype/freetype.h \
	freetype/ftglyph.h \
	freetype/ftoutln.h \
	freetype/ftmodapi.h \
	freetype/ftsnames.h \
	freetype/internal/ftstream.h


all:	$(PROJ).DLL

FTDEBUG.OBJ:	SRC\FTDEBUG.C $(FTDEBUG_DEP)
	$(CC) $(CFLAGS) $(CCREATEPCHFLAG) /c SRC\FTDEBUG.C

FTSYSTEM.OBJ:	SRC\FTSYSTEM.C $(FTSYSTEM_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c SRC\FTSYSTEM.C

AUTOFIT.OBJ:	SRC\AUTOFIT\AUTOFIT.C $(AUTOFIT_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c SRC\AUTOFIT\AUTOFIT.C

FTBASE.OBJ:	SRC\BASE\FTBASE.C $(FTBASE_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c SRC\BASE\FTBASE.C

PSAUX.OBJ:	SRC\PSAUX\PSAUX.C $(PSAUX_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c SRC\PSAUX\PSAUX.C

PSNAMES.OBJ:	SRC\PSNAMES\PSNAMES.C $(PSNAMES_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c SRC\PSNAMES\PSNAMES.C

RASTER.OBJ:	SRC\RASTER\RASTER.C $(RASTER_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c SRC\RASTER\RASTER.C

SFNT.OBJ:	SRC\SFNT\SFNT.C $(SFNT_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c SRC\SFNT\SFNT.C

SMOOTH.OBJ:	SRC\SMOOTH\SMOOTH.C $(SMOOTH_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c SRC\SMOOTH\SMOOTH.C

FTGZIP.OBJ:	SRC\GZIP\FTGZIP.C $(FTGZIP_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c SRC\GZIP\FTGZIP.C

FTCBASIC.OBJ:	SRC\CACHE\FTCBASIC.C $(FTCBASIC_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c SRC\CACHE\FTCBASIC.C

FTCCACHE.OBJ:	SRC\CACHE\FTCCACHE.C $(FTCCACHE_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c SRC\CACHE\FTCCACHE.C

FTCCMAP.OBJ:	SRC\CACHE\FTCCMAP.C $(FTCCMAP_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c SRC\CACHE\FTCCMAP.C

FTCGLYPH.OBJ:	SRC\CACHE\FTCGLYPH.C $(FTCGLYPH_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c SRC\CACHE\FTCGLYPH.C

FTCIMAGE.OBJ:	SRC\CACHE\FTCIMAGE.C $(FTCIMAGE_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c SRC\CACHE\FTCIMAGE.C

FTCMANAG.OBJ:	SRC\CACHE\FTCMANAG.C $(FTCMANAG_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c SRC\CACHE\FTCMANAG.C

FTCMRU.OBJ:	SRC\CACHE\FTCMRU.C $(FTCMRU_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c SRC\CACHE\FTCMRU.C

FTCSBITS.OBJ:	SRC\CACHE\FTCSBITS.C $(FTCSBITS_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c SRC\CACHE\FTCSBITS.C

TTDRIVER.OBJ:	SRC\TRUETYPE\TTDRIVER.C $(TTDRIVER_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c SRC\TRUETYPE\TTDRIVER.C

TTGLOAD.OBJ:	SRC\TRUETYPE\TTGLOAD.C $(TTGLOAD_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c SRC\TRUETYPE\TTGLOAD.C

TTGXVAR.OBJ:	SRC\TRUETYPE\TTGXVAR.C $(TTGXVAR_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c SRC\TRUETYPE\TTGXVAR.C

TTINTERP.OBJ:	SRC\TRUETYPE\TTINTERP.C $(TTINTERP_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c SRC\TRUETYPE\TTINTERP.C

TTOBJS.OBJ:	SRC\TRUETYPE\TTOBJS.C $(TTOBJS_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c SRC\TRUETYPE\TTOBJS.C

TTPLOAD.OBJ:	SRC\TRUETYPE\TTPLOAD.C $(TTPLOAD_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c SRC\TRUETYPE\TTPLOAD.C

FTBITMAP.OBJ:	SRC\BASE\FTBITMAP.C $(FTBITMAP_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c SRC\BASE\FTBITMAP.C

FTGLYPH.OBJ:	SRC\BASE\FTGLYPH.C $(FTGLYPH_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c SRC\BASE\FTGLYPH.C

FTBBOX.OBJ:	SRC\BASE\FTBBOX.C $(FTBBOX_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c SRC\BASE\FTBBOX.C

FTAPI16.OBJ:	SRC\FTAPI16.C $(FTAPI16_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c SRC\FTAPI16.C


$(PROJ).DLL::	FTDEBUG.OBJ FTSYSTEM.OBJ AUTOFIT.OBJ FTBASE.OBJ PSAUX.OBJ PSNAMES.OBJ \
	RASTER.OBJ SFNT.OBJ SMOOTH.OBJ FTGZIP.OBJ FTCBASIC.OBJ FTCCACHE.OBJ FTCCMAP.OBJ FTCGLYPH.OBJ \
	FTCIMAGE.OBJ FTCMANAG.OBJ FTCMRU.OBJ FTCSBITS.OBJ TTDRIVER.OBJ TTGLOAD.OBJ TTGXVAR.OBJ \
	TTINTERP.OBJ TTOBJS.OBJ TTPLOAD.OBJ FTBITMAP.OBJ FTGLYPH.OBJ FTBBOX.OBJ FTAPI16.OBJ $(OBJS_EXT) $(DEFFILE)
	echo >NUL @<<$(PROJ).CRF
FTDEBUG.OBJ +
FTSYSTEM.OBJ +
AUTOFIT.OBJ +
FTBASE.OBJ +
PSAUX.OBJ +
PSNAMES.OBJ +
RASTER.OBJ +
SFNT.OBJ +
SMOOTH.OBJ +
FTGZIP.OBJ +
FTCBASIC.OBJ +
FTCCACHE.OBJ +
FTCCMAP.OBJ +
FTCGLYPH.OBJ +
FTCIMAGE.OBJ +
FTCMANAG.OBJ +
FTCMRU.OBJ +
FTCSBITS.OBJ +
TTDRIVER.OBJ +
TTGLOAD.OBJ +
TTGXVAR.OBJ +
TTINTERP.OBJ +
TTOBJS.OBJ +
TTPLOAD.OBJ +
FTBITMAP.OBJ +
FTGLYPH.OBJ +
FTBBOX.OBJ +
FTAPI16.OBJ +
$(OBJS_EXT)
$(PROJ).DLL
$(MAPFILE)
C:\MSVC\CDK16\LIB\+
c:\msvc\lib\+
c:\msvc\mfc\lib\+
$(LIBS)
$(DEFFILE);
<<
	link $(LFLAGS) @$(PROJ).CRF
	$(RC) $(RESFLAGS) $@
	implib /nowep $(PROJ).LIB $(PROJ).DLL


run: $(PROJ).DLL
	$(PROJ) $(RUNFLAGS)


$(PROJ).BSC: $(SBRS)
	bscmake @<<
/o$@ $(SBRS)
<<
