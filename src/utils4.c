/*====================================================================*
 -  Copyright (C) 2025 Leptonica.  All rights reserved.
 -
 -  Redistribution and use in source and binary forms, with or without
 -  modification, are permitted provided that the following conditions
 -  are met:
 -  1. Redistributions of source code must retain the above copyright
 -     notice, this list of conditions and the following disclaimer.
 -  2. Redistributions in binary form must reproduce the above
 -     copyright notice, this list of conditions and the following
 -     disclaimer in the documentation and/or other materials
 -     provided with the distribution.
 -
 -  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 -  ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 -  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 -  A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL ANY
 -  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 -  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 -  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 -  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 -  OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 -  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 -  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *====================================================================*/

/*!
 * \file utils3.c
 * <pre>
 *
 *      ------------------------------------------
 *      This file has these utilities:
 *         - multi-platform file and directory operations
 *         - file name operations
 *      ------------------------------------------
 *
 *       General file name operations
 *           l_int32    splitPathAtDirectory()
 *           l_int32    splitPathAtExtension()
 *           char      *pathJoin()
 *           char      *appendSubdirs()
 *
 *       Special file name operations
 *           l_int32    convertSepCharsInPath()
 *           char      *genPathname()
 *           l_int32    makeTempDirname()
 *           l_int32    modifyTrailingSlash()
 *           char      *l_makeTempFilename()
 *           l_int32    extractNumberFromFilename()
 *
 *
 *  Notes on multi-platform development
 *  -----------------------------------
 *  This is important:
 *  (1) With the exception of splitPathAtDirectory(), splitPathAtExtension()
  *     and genPathname(), all input pathnames must have unix separators.
 *  (2) On macOS, iOS and Windows, for read or write to "/tmp/..."
 *      the filename is rewritten to use the OS specific temp directory:
 *         /tmp  ==>   [Temp]/...
 *  (3) This filename rewrite, along with the conversion from unix
 *      to OS specific pathnames, happens in genPathname().
 *  (4) Use fopenReadStream() and fopenWriteStream() to open files,
 *      because these use genPathname() to find the platform-dependent
 *      filenames.  Likewise for l_binaryRead() and l_binaryWrite().
 *  (5) For moving, copying and removing files and directories that are in
 *      subdirectories of /tmp, use the lept_*() file system shell wrappers:
 *         lept_mkdir(), lept_rmdir(), lept_mv(), lept_rm() and lept_cp().
 *  (6) For programs use the lept_fopen(), lept_fclose(), lept_calloc()
 *      and lept_free() C library wrappers.  These work properly on Windows,
 *      where the same DLL must perform complementary operations on
 *      file streams (open/close) and heap memory (malloc/free).
 *  (7) Why read and write files to temp directories?
 *      The library needs the ability to read and write ephemeral
 *      files to default places, both for generating debugging output
 *      and for supporting regression tests.  Applications also need
 *      this ability for debugging.
 *  (8) Why do the pathname rewrite on macOS, iOS and Windows?
 *      The goal is to have the library, and programs using the library,
 *      run on multiple platforms without changes.  The location of
 *      temporary files depends on the platform as well as the user's
 *      configuration.  Temp files on some operating systems are in some
 *      directory not known a priori.  To make everything work seamlessly on
 *      any OS, every time you open a file for reading or writing,
 *      use a special function such as fopenReadStream() or
 *      fopenWriteStream(); these call genPathname() to ensure that
 *      if it is a temp file, the correct path is used.  To indicate
 *      that this is a temp file, the application is written with the
 *      root directory of the path in a canonical form: "/tmp".
 *  (9) Why is it that multi-platform directory functions like lept_mkdir()
 *      and lept_rmdir(), as well as associated file functions like
 *      lept_rm(), lept_mv() and lept_cp(), only work in the temp dir?
 *      These functions were designed to provide easy manipulation of
 *      temp files.  The restriction to temp files is for safety -- to
 *      prevent an accidental deletion of important files.  For example,
 *      lept_rmdir() first deletes all files in a specified subdirectory
 *      of temp, and then removes the directory.
 *
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#ifdef _MSC_VER
#ifndef _CRTDBG_MAP_ALLOC
#define _CRTDBG_MAP_ALLOC
#endif
#include <crtdbg.h>
#include <process.h>
#include <direct.h>
#ifndef getcwd
#define getcwd _getcwd  /* fix MSVC warning */
#endif
#else
#include <unistd.h>
#endif   /* _MSC_VER */

#ifdef _WIN32
// MSWin fix: make sure we include winsock2.h *before* windows.h implicitly includes the antique winsock.h and causes all kinds of weird errors at compile time:
#include <winsock2.h>
#include <windows.h>
#include <fcntl.h>     /* _O_CREAT, ... */
#include <io.h>        /* _open */
#include <sys/stat.h>  /* _S_IREAD, _S_IWRITE */
#  define strcasecmp _stricmp
#  define strncasecmp _strnicmp
#else
#include <strings.h>
#include <sys/stat.h>  /* for stat, mkdir(2) */
#include <sys/types.h>
#endif

#ifdef __APPLE__
#include <unistd.h>
#endif

#include <errno.h>     /* for errno */
#include <string.h>
#include <stddef.h>

#include "allheaders.h"
#include "pix_internal.h"

#if defined(__APPLE__) || defined(_WIN32)
/* Rewrite paths starting with /tmp for macOS, iOS and Windows. */
#define REWRITE_TMP
#endif

/*--------------------------------------------------------------------*
 *         Image Debugging & Diagnostics Helper functions             *
 *--------------------------------------------------------------------*/


typedef struct StepsArray {
#define L_MAX_STEPS_DEPTH 10         // [batch:0, step:1, item:2, level:3, sublevel:4, ...]
	uint16_t actual_depth;
	l_atomic vals[L_MAX_STEPS_DEPTH];
	// Note: the last depth level is permanently auto-incrementing and serves as a 'persisted' %item_id_is_forever_increasing counter.
} StepsArray;

struct l_diag_predef_parts {
	const char* basepath;            //!< the base path within where every generated file must land.
#if 0
	NUMA *steps;                     //!< a hierarchical step numbering system; the last number level will (auto)increment, or RESET every time when a more major step level incremented/changed (unless %item_id_is_forever_increasing is set).
#else
	struct StepsArray steps;         //!< a hierarchical step numbering system; the last number level will (auto)increment, or RESET every time when a more major step level incremented/changed (unless %item_id_is_forever_increasing is set).
#endif
	unsigned int basepath_minlength; //!< minimum length of the basepath, i.e. append/remove/replace APIs are prohibited from editing the leading part of basepath that is shorter than this length.

	uint64_t active_hash_id;         //!< derived from the originally specified %active_filename path, or set explicitly (if user-land code has a better idea about what value this should be).
	const char* active_filename;     //!< the currently processed source file name. To be used (in reduced form) as a target filename prefix.
	const char* process_name;        //!< the part which identifies what we are doing right now (at a high abstraction level)
	const char* default_path_template_str; //!< you can customize the preferred file path formatting template.

	char* last_generated_filepath;   //!< internal cache: stores the last generated file path (for re-use and reference)
	char* last_generated_step_id_string; //!< internal cache: stores the last generated steps[] id string (for re-use and reference)

	l_atomic             refcount;   /*!< reference count (1 if no clones)  */

	unsigned int must_regenerate : 1;		//!< set when l_filename_prefix changes mandate a freshly (re)generated target file prefix for subsequent requests.
	unsigned int must_bump_step_id : 1;		//!< set when %step_id should be incremented before next use.
	unsigned int step_id_is_forever_increasing : L_MAX_STEPS_DEPTH;

	unsigned int display : 1;        /*!< 1 if in display mode; 0 otherwise                */
	unsigned int debugging : 1;		 //!< 1 if debugging mode is activated, resulting in several leptonica APIs producing debug/info messages and/or writing diagnostic plots and images to the basepath filesystem.
};



LDIAG_CTX
leptCreateDiagnoticsSpecInstance(void)
{
	LDIAG_CTX dst = (LDIAG_CTX)LEPT_CALLOC(1, sizeof(*dst));
	if (!dst)
	{
		return (LDIAG_CTX)ERROR_PTR("LDIAG_CTX not made", __func__, NULL);
	}
	dst->refcount = 1;
	return dst;
}


void
leptDestroyDiagnoticsSpecInstance(LDIAG_CTX* spec_ptr)
{
	if (!spec_ptr) {
		L_WARNING("ptr address is null!\n", __func__);
		return;
	}

	LDIAG_CTX spec = *spec_ptr;
	if (spec == NULL)
		return;

	/* Decrement the ref count.  If it is 0, destroy the spec. */
	if (--spec->refcount == 0) {
		stringDestroy(&spec->basepath);
		stringDestroy(&spec->active_filename);
		stringDestroy(&spec->process_name);
		stringDestroy(&spec->default_path_template_str);
		LEPT_FREE(spec);
	}
	*spec_ptr = NULL;
}


LDIAG_CTX
leptCopyDiagnoticsSpecInstance(LDIAG_CTX spec)
{
	if (!spec)
	{
		return (LDIAG_CTX)ERROR_PTR("image diagnostics spec not defined", __func__, NULL);
	}
	LDIAG_CTX dst = (LDIAG_CTX)LEPT_MALLOC(sizeof(*dst));
	if (!dst)
	{
		return (LDIAG_CTX)ERROR_PTR("LDIAG_CTX not made", __func__, NULL);
	}

	memcpy(dst, spec, sizeof(*spec));

	dst->refcount = 1;

	dst->basepath = stringNew(spec->basepath);
	dst->active_filename = stringNew(spec->active_filename);
	dst->process_name = stringNew(spec->process_name);
	dst->default_path_template_str = stringNew(spec->default_path_template_str);

	return dst;
}


LDIAG_CTX
leptCloneDiagnoticsSpecInstance(LDIAG_CTX spec)
{
	if (!spec)
	{
		return (LDIAG_CTX)ERROR_PTR("image diagnostics spec not defined", __func__, NULL);
	}
	spec->refcount++;
	return spec;
}


void
leptReplaceDiagnoticsSpecInstance(LDIAG_CTX* specd, LDIAG_CTX specs)
{
	if (!specd)
	{
		L_ERROR("image diagnostics spec reference not defined", __func__);
		return;
	}

	if (*specd == specs)
		return;

	leptDestroyDiagnoticsSpecInstance(specd);

	*specd = leptCloneDiagnoticsSpecInstance(specs);
}


// produce the diagspec which is non-NULL and has debugging mode enabled.
// if that one is not available, produce the non-NULL diagspec.
// if none is available, return NULL.
static inline LDIAG_CTX
get_prevalent_diagspec(LDIAG_CTX spec1, LDIAG_CTX spec2)
{
	if (spec1 != NULL && leptIsDebugModeActive(spec1))
		return spec1;
	if (spec2 != NULL && leptIsDebugModeActive(spec2))
		return spec2;
	if (spec1 != NULL)
		return spec1;
	return spec2;
}


LDIAG_CTX
leptDebugGetDiagnosticsSpecFromAny(unsigned int arg_count, LDIAG_CTX spec1, LDIAG_CTX spec2, ...)
{
	LDIAG_CTX rv = spec1;
	va_list ap;
	va_start(ap, spec2);
	if (arg_count < 2)
		arg_count = 2;
	for (arg_count--; arg_count > 0; arg_count--) {
		rv = get_prevalent_diagspec(rv, spec2);

		spec2 = va_arg(ap, LDIAG_CTX);
	}
	va_end(ap);
	return rv;
}


LDIAG_CTX
pixGetDiagnosticsSpec(const PIX* pixs)
{
	if (!pixs)
		return (LDIAG_CTX)ERROR_PTR("pixs not defined", __func__, NULL);

	return pixs->diag_spec;
}


LDIAG_CTX
pixGetDiagnosticsSpecFromAny(unsigned int arg_count, const PIX* pix1, const PIX* pix2, ...)
{
	LDIAG_CTX rv = NULL;
	if (pix1 != NULL)
		rv = pix1->diag_spec;

	va_list ap;
	va_start(ap, pix2);
	if (arg_count < 2)
		arg_count = 2;
	for (arg_count--; arg_count > 0; arg_count--) {
		if (pix2)
			rv = get_prevalent_diagspec(rv, pix2->diag_spec);

		pix2 = va_arg(ap, const PIX *);
	}
	va_end(ap);

	return rv;
}



l_ok
pixSetDiagnosticsSpec(PIX* pix, LDIAG_CTX spec)
{
	if (!pix)
		return ERROR_INT("pix not defined", __func__, 1);

	if (pix->diag_spec == spec)
		return 0;

	leptDestroyDiagnoticsSpecInstance(&pix->diag_spec);

	if (spec) {
		pix->diag_spec = leptCloneDiagnoticsSpecInstance(spec);
	}
	else {
		pix->diag_spec = NULL;
	}
	return 0;
}


l_ok
pixCopyDiagnosticsSpec(PIX* pixd, const PIX* pixs)
{
	if (!pixs)
		return ERROR_INT("pixs not defined", __func__, 1);
	if (!pixd)
		return ERROR_INT("pixd not defined", __func__, 1);

	// Copy before Destroy: both diagspecs MAY be the same and the cleaner-looking Destroy-then-Copy would invalidate the source spec! (race condition)
	LDIAG_CTX spec = pixd->diag_spec;

	if (pixs->diag_spec) {
		pixd->diag_spec = leptCopyDiagnoticsSpecInstance(pixs->diag_spec);
	}
	else {
		pixd->diag_spec = NULL;
	}

	leptDestroyDiagnoticsSpecInstance(&spec);

	return 0;
}


l_ok
pixCloneDiagnosticsSpec(PIX* pixd, const PIX* pixs)
{
	if (!pixs)
		return ERROR_INT("pixs not defined", __func__, 1);
	if (!pixd)
		return ERROR_INT("pixd not defined", __func__, 1);

	if (pixd->diag_spec == pixs->diag_spec)
		return 0;

	leptDestroyDiagnoticsSpecInstance(&pixd->diag_spec);

	if (pixs->diag_spec) {
		pixd->diag_spec = leptCloneDiagnoticsSpecInstance(pixs->diag_spec);
	}
	else {
		pixd->diag_spec = NULL;
	}
	return 0;
}


LDIAG_CTX
fpixGetDiagnosticsSpec(const FPIX* fpixs)
{
	if (!fpixs)
		return (LDIAG_CTX)ERROR_PTR("fpixs not defined", __func__, NULL);

	return fpixs->diag_spec;
}


LDIAG_CTX
fpixGetDiagnosticsSpecFromAny(unsigned int arg_count, const FPIX* pix1, const FPIX* pix2, ...)
{
	LDIAG_CTX rv = NULL;
	if (pix1 != NULL)
		rv = pix1->diag_spec;

	va_list ap;
	va_start(ap, pix2);
	if (arg_count < 2)
		arg_count = 2;
	for (arg_count--; arg_count > 0; arg_count--) {
		if (pix2)
			rv = get_prevalent_diagspec(rv, pix2->diag_spec);

		pix2 = va_arg(ap, const FPIX*);
	}
	va_end(ap);

	return rv;
}



l_ok
fpixSetDiagnosticsSpec(FPIX* pix, LDIAG_CTX spec)
{
	if (!pix)
		return ERROR_INT("fpix not defined", __func__, 1);

	if (pix->diag_spec == spec)
		return 0;

	leptDestroyDiagnoticsSpecInstance(&pix->diag_spec);

	if (spec) {
		pix->diag_spec = leptCloneDiagnoticsSpecInstance(spec);
	}
	else {
		pix->diag_spec = NULL;
	}
	return 0;
}


l_ok
fpixCopyDiagnosticsSpec(FPIX* pixd, const FPIX* pixs)
{
	if (!pixs)
		return ERROR_INT("fpixs not defined", __func__, 1);
	if (!pixd)
		return ERROR_INT("fpixd not defined", __func__, 1);

	// Copy before Destroy: both diagspecs MAY be the same and the cleaner-looking Destroy-then-Copy would invalidate the source spec! (race condition)
	LDIAG_CTX spec = pixd->diag_spec;

	if (pixs->diag_spec) {
		pixd->diag_spec = leptCopyDiagnoticsSpecInstance(pixs->diag_spec);
	}
	else {
		pixd->diag_spec = NULL;
	}

	leptDestroyDiagnoticsSpecInstance(&spec);

	return 0;
}


l_ok
fpixCloneDiagnosticsSpec(FPIX* pixd, const FPIX* pixs)
{
	if (!pixs)
		return ERROR_INT("fpixs not defined", __func__, 1);
	if (!pixd)
		return ERROR_INT("fpixd not defined", __func__, 1);

	if (pixd->diag_spec == pixs->diag_spec)
		return 0;

	leptDestroyDiagnoticsSpecInstance(&pixd->diag_spec);

	if (pixs->diag_spec) {
		pixd->diag_spec = leptCloneDiagnoticsSpecInstance(pixs->diag_spec);
	}
	else {
		pixd->diag_spec = NULL;
	}
	return 0;
}


LDIAG_CTX
dpixGetDiagnosticsSpec(const DPIX* dpixs)
{
	if (!dpixs)
		return (LDIAG_CTX)ERROR_PTR("dpixs not defined", __func__, NULL);

	return dpixs->diag_spec;
}


LDIAG_CTX
dpixGetDiagnosticsSpecFromAny(unsigned int arg_count, const DPIX* pix1, const DPIX* pix2, ...)
{
	LDIAG_CTX rv = NULL;
	if (pix1 != NULL)
		rv = pix1->diag_spec;

	va_list ap;
	va_start(ap, pix2);
	if (arg_count < 2)
		arg_count = 2;
	for (arg_count--; arg_count > 0; arg_count--) {
		if (pix2)
			rv = get_prevalent_diagspec(rv, pix2->diag_spec);

		pix2 = va_arg(ap, const DPIX*);
	}
	va_end(ap);

	return rv;
}


l_ok
dpixSetDiagnosticsSpec(DPIX* pix, LDIAG_CTX spec)
{
	if (!pix)
		return ERROR_INT("dpix not defined", __func__, 1);

	if (pix->diag_spec == spec)
		return 0;

	leptDestroyDiagnoticsSpecInstance(&pix->diag_spec);

	if (spec) {
		pix->diag_spec = leptCloneDiagnoticsSpecInstance(spec);
	}
	else {
		pix->diag_spec = NULL;
	}
	return 0;
}


l_ok
dpixCopyDiagnosticsSpec(DPIX* pixd, const DPIX* pixs)
{
	if (!pixs)
		return ERROR_INT("dpixs not defined", __func__, 1);
	if (!pixd)
		return ERROR_INT("dpixd not defined", __func__, 1);

	// Copy before Destroy: both diagspecs MAY be the same and the cleaner-looking Destroy-then-Copy would invalidate the source spec! (race condition)
	LDIAG_CTX spec = pixd->diag_spec;

	if (pixs->diag_spec) {
		pixd->diag_spec = leptCopyDiagnoticsSpecInstance(pixs->diag_spec);
	}
	else {
		pixd->diag_spec = NULL;
	}

	leptDestroyDiagnoticsSpecInstance(&spec);

	return 0;
}


l_ok
dpixCloneDiagnosticsSpec(DPIX* pixd, const DPIX* pixs)
{
	if (!pixs)
		return ERROR_INT("dpixs not defined", __func__, 1);
	if (!pixd)
		return ERROR_INT("dpixd not defined", __func__, 1);

	if (pixd->diag_spec == pixs->diag_spec)
		return 0;

	leptDestroyDiagnoticsSpecInstance(&pixd->diag_spec);

	if (pixs->diag_spec) {
		pixd->diag_spec = leptCloneDiagnoticsSpecInstance(pixs->diag_spec);
	}
	else {
		pixd->diag_spec = NULL;
	}
	return 0;
}


LDIAG_CTX
pixaGetDiagnosticsSpec(const PIXA* pixa)
{
	if (!pixa)
		return (LDIAG_CTX)ERROR_PTR("pixa not defined", __func__, NULL);

	LDIAG_CTX rv = pixa->diag_spec;
	l_int32 cnt = pixaGetCount(pixa);
	for (l_int32 i = 0; i < cnt; cnt++) {
		PIX* pix1 = pixaGetPix(pixa, i, L_CLONE);
		rv = get_prevalent_diagspec(rv, pix1->diag_spec);
		pixDestroy(&pix1);
	}
	return rv;
}


LDIAG_CTX
pixaGetDiagnosticsSpecFromAny(unsigned int arg_count, const PIXA* pixa1, const PIXA* pixa2, ...)
{
	LDIAG_CTX rv = NULL;
	if (pixa1 != NULL)
		rv = pixaGetDiagnosticsSpec(pixa1);

	va_list ap;
	va_start(ap, pixa2);
	if (arg_count < 2)
		arg_count = 2;
	for (arg_count--; arg_count > 0; arg_count--) {
		if (pixa2)
			rv = get_prevalent_diagspec(rv, pixaGetDiagnosticsSpec(pixa2));

		pixa2 = va_arg(ap, const PIXA*);
	}
	va_end(ap);

	return rv;
}


l_ok
pixaSetDiagnosticsSpec(PIXA* pixa, LDIAG_CTX spec)
{
	if (!pixa)
		return ERROR_INT("pixa not defined", __func__, 1);

	if (pixa->diag_spec == spec)
		return 0;

	leptDestroyDiagnoticsSpecInstance(&pixa->diag_spec);

	if (spec) {
		pixa->diag_spec = leptCloneDiagnoticsSpecInstance(spec);
	}
	else {
		pixa->diag_spec = NULL;
	}
	return 0;
}


l_ok
pixaCopyDiagnosticsSpec(PIXA* pixd, const PIXA* pixs)
{
	if (!pixs)
		return ERROR_INT("pixs not defined", __func__, 1);
	if (!pixd)
		return ERROR_INT("pixd not defined", __func__, 1);

	LDIAG_CTX spec = pixaGetDiagnosticsSpec(pixs);

	// Copy before Destroy: both diagspecs MAY be the same and the cleaner-looking Destroy-then-Copy would invalidate the source spec! (race condition)
	LDIAG_CTX old_spec = pixd->diag_spec;

	if (spec) {
		pixd->diag_spec = leptCopyDiagnoticsSpecInstance(spec);
	}
	else {
		pixd->diag_spec = NULL;
	}

	leptDestroyDiagnoticsSpecInstance(&old_spec);

	return 0;
}


l_ok
pixaCloneDiagnosticsSpec(PIXA* pixd, const PIXA* pixs)
{
	if (!pixs)
		return ERROR_INT("pixs not defined", __func__, 1);
	if (!pixd)
		return ERROR_INT("pixd not defined", __func__, 1);

	LDIAG_CTX spec = pixaGetDiagnosticsSpec(pixs);

	if (pixd->diag_spec == spec)
		return 0;

	leptDestroyDiagnoticsSpecInstance(&pixd->diag_spec);

	if (spec) {
		pixd->diag_spec = leptCloneDiagnoticsSpecInstance(spec);
	}
	else {
		pixd->diag_spec = NULL;
	}
	return 0;
}


// same as pixaSetDiagnosticsSpec, except this function also sets the diagspec for each individual pix in the pixa.
l_ok
pixaSetDiagnosticsSpecPervasively(PIXA* pixa, LDIAG_CTX spec)
{
	if (!pixa)
		return ERROR_INT("pixa not defined", __func__, 1);
	l_ok ret = pixaSetDiagnosticsSpec(pixa, spec);
	if (ret != 0)
		return ret;
	l_int32 cnt = pixaGetCount(pixa);
	for (l_int32 i = 0; i < cnt; i++) {
		PIX* pix1 = pixaGetPix(pixa, i, L_CLONE);
		ret = pixSetDiagnosticsSpec(pix1, spec);
		pixDestroy(&pix1);
		if (ret != 0)
			return ret;
	}
	return 0;
}


LDIAG_CTX
fpixaGetDiagnosticsSpec(const FPIXA* pixa)
{
	if (!pixa)
		return (LDIAG_CTX)ERROR_PTR("fpixa not defined", __func__, NULL);

	LDIAG_CTX rv = pixa->diag_spec;
	l_int32 cnt = fpixaGetCount(pixa);
	for (l_int32 i = 0; i < cnt; cnt++) {
		FPIX* pix1 = fpixaGetFPix(pixa, i, L_CLONE);
		rv = get_prevalent_diagspec(rv, pix1->diag_spec);
		fpixDestroy(&pix1);
	}
	return rv;
}


LDIAG_CTX
fpixaGetDiagnosticsSpecFromAny(unsigned int arg_count, const FPIXA* pixa1, const FPIXA* pixa2, ...)
{
	LDIAG_CTX rv = NULL;
	if (pixa1 != NULL)
		rv = fpixaGetDiagnosticsSpec(pixa1);

	va_list ap;
	va_start(ap, pixa2);
	if (arg_count < 2)
		arg_count = 2;
	for (arg_count--; arg_count > 0; arg_count--) {
		if (pixa2)
			rv = get_prevalent_diagspec(rv, fpixaGetDiagnosticsSpec(pixa2));

		pixa2 = va_arg(ap, const FPIXA*);
	}
	va_end(ap);

	return rv;
}


l_ok
fpixaSetDiagnosticsSpec(FPIXA* pixa, LDIAG_CTX spec)
{
	if (!pixa)
		return ERROR_INT("fpixa not defined", __func__, 1);

	if (pixa->diag_spec == spec)
		return 0;

	leptDestroyDiagnoticsSpecInstance(&pixa->diag_spec);

	if (spec) {
		pixa->diag_spec = leptCloneDiagnoticsSpecInstance(spec);
	}
	else {
		pixa->diag_spec = NULL;
	}
	return 0;
}


l_ok
fpixaCopyDiagnosticsSpec(FPIXA* pixd, const FPIXA* pixs)
{
	if (!pixs)
		return ERROR_INT("fpixs not defined", __func__, 1);
	if (!pixd)
		return ERROR_INT("fpixd not defined", __func__, 1);

	LDIAG_CTX spec = fpixaGetDiagnosticsSpec(pixs);

	// Copy before Destroy: both diagspecs MAY be the same and the cleaner-looking Destroy-then-Copy would invalidate the source spec! (race condition)
	LDIAG_CTX old_spec = pixd->diag_spec;

	if (spec) {
		pixd->diag_spec = leptCopyDiagnoticsSpecInstance(spec);
	}
	else {
		pixd->diag_spec = NULL;
	}

	leptDestroyDiagnoticsSpecInstance(&old_spec);

	return 0;
}


l_ok
fpixaCloneDiagnosticsSpec(FPIXA* pixd, const FPIXA* pixs)
{
	if (!pixs)
		return ERROR_INT("fpixs not defined", __func__, 1);
	if (!pixd)
		return ERROR_INT("fpixd not defined", __func__, 1);

	LDIAG_CTX spec = fpixaGetDiagnosticsSpec(pixs);

	if (pixd->diag_spec == spec)
		return 0;

	leptDestroyDiagnoticsSpecInstance(&pixd->diag_spec);

	if (spec) {
		pixd->diag_spec = leptCloneDiagnoticsSpecInstance(spec);
	}
	else {
		pixd->diag_spec = NULL;
	}
	return 0;
}


l_ok
fpixaSetDiagnosticsSpecPervasively(FPIXA* fpixa, LDIAG_CTX spec)
{
	if (!fpixa)
		return ERROR_INT("pixa not defined", __func__, 1);
	l_ok ret = fpixaSetDiagnosticsSpec(fpixa, spec);
	if (ret != 0)
		return ret;
	l_int32 cnt = fpixaGetCount(fpixa);
	for (l_int32 i = 0; i < cnt; i++) {
		FPIX* fpix1 = fpixaGetFPix(fpixa, i, L_CLONE);
		ret = fpixSetDiagnosticsSpec(fpix1, spec);
		fpixDestroy(&fpix1);
		if (ret != 0)
			return ret;
	}
	return 0;
}


LDIAG_CTX
gplotGetDiagnosticsSpec(const GPLOT* gplot)
{
	if (!gplot)
		return (LDIAG_CTX)ERROR_PTR("gplot not defined", __func__, NULL);

	return gplot->diag_spec;
}


LDIAG_CTX
gplotGetDiagnosticsSpecFromAny(unsigned int arg_count, const GPLOT* gplot1, const GPLOT* gplot2, ...)
{
	LDIAG_CTX rv = NULL;
	if (gplot1 != NULL)
		rv = gplot1->diag_spec;

	va_list ap;
	va_start(ap, gplot2);
	if (arg_count < 2)
		arg_count = 2;
	for (arg_count--; arg_count > 0; arg_count--) {
		if (gplot2)
			rv = get_prevalent_diagspec(rv, gplot2->diag_spec);

		gplot2 = va_arg(ap, const GPLOT*);
	}
	va_end(ap);

	return rv;
}


l_ok
gplotSetDiagnosticsSpec(GPLOT* gplot, LDIAG_CTX spec)
{
	if (!gplot)
		return ERROR_INT("gplot not defined", __func__, 1);

	if (gplot->diag_spec == spec)
		return 0;

	leptDestroyDiagnoticsSpecInstance(&gplot->diag_spec);

	if (spec) {
		gplot->diag_spec = leptCloneDiagnoticsSpecInstance(spec);
	}
	else {
		gplot->diag_spec = NULL;
	}
	return 0;
}


l_ok
gplotCopyDiagnosticsSpec(GPLOT* gplotd, const GPLOT* gplots)
{
	if (!gplots)
		return ERROR_INT("gplots not defined", __func__, 1);
	if (!gplotd)
		return ERROR_INT("gplotd not defined", __func__, 1);

	// Copy before Destroy: both diagspecs MAY be the same and the cleaner-looking Destroy-then-Copy would invalidate the source spec! (race condition)
	LDIAG_CTX spec = gplotd->diag_spec;

	if (gplots->diag_spec) {
		gplotd->diag_spec = leptCopyDiagnoticsSpecInstance(gplots->diag_spec);
	}
	else {
		gplotd->diag_spec = NULL;
	}

	leptDestroyDiagnoticsSpecInstance(&spec);

	return 0;
}


l_ok
gplotCloneDiagnosticsSpec(GPLOT* gplotd, const GPLOT* gplots)
{
	if (!gplots)
		return ERROR_INT("gplots not defined", __func__, 1);
	if (!gplotd)
		return ERROR_INT("gplotd not defined", __func__, 1);

	if (gplotd->diag_spec == gplots->diag_spec)
		return 0;

	leptDestroyDiagnoticsSpecInstance(&gplotd->diag_spec);

	if (gplots->diag_spec) {
		gplotd->diag_spec = leptCloneDiagnoticsSpecInstance(gplots->diag_spec);
	}
	else {
		gplotd->diag_spec = NULL;
	}
	return 0;
}


#if 0

LDIAG_CTX
boxaGetDiagnosticsSpec(const BOXA* boxa)
{
	if (!boxa)
		return (LDIAG_CTX)ERROR_PTR("boxa not defined", __func__, NULL);
	return boxa->diag_spec;
}


LDIAG_CTX
boxaGetDiagnosticsSpecFromAny(const BOXA* boxa1, const BOXA* boxa2, ...)
{
	LDIAG_CTX rv = NULL;
	if (boxa1 != NULL)
		rv = boxa1->diag_spec;
	va_list ap;
	va_start(ap, boxa2);
	if (arg_count < 2)
		arg_count = 2;
	for (arg_count--; arg_count > 0; arg_count--) {
		if (boxa2)
			rv = get_prevalent_diagspec(rv, boxa2->diag_spec);

		boxa2 = va_arg(ap, const BOXA*);
	}
	va_end(ap);
	return rv;
}


l_ok
boxaSetDiagnosticsSpec(BOXA* boxa, LDIAG_CTX spec)
{
	if (!boxa)
		return ERROR_INT("boxa not defined", __func__, 1);
	if (boxa->diag_spec == spec)
		return 0;
	leptDestroyDiagnoticsSpecInstance(&boxa->diag_spec);
	if (spec) {
		boxa->diag_spec = leptCloneDiagnoticsSpecInstance(spec);
	}
	else {
		boxa->diag_spec = NULL;
	}
	return 0;
}


l_ok
boxaCopyDiagnosticsSpec(BOXA* boxad, const BOXA* boxas)
{
	if (!boxas)
		return ERROR_INT("boxas not defined", __func__, 1);
	if (!boxad)
		return ERROR_INT("boxad not defined", __func__, 1);

	// Copy before Destroy: both diagspecs MAY be the same and the cleaner-looking Destroy-then-Copy would invalidate the source spec! (race condition)
	LDIAG_CTX old_spec = boxad->diag_spec;

	if (boxas->diag_spec) {
		boxad->diag_spec = leptCopyDiagnoticsSpecInstance(boxas->diag_spec);
	}
	else {
		boxad->diag_spec = NULL;
	}

	leptDestroyDiagnoticsSpecInstance(&spec);

	return 0;
}


l_ok
boxaCloneDiagnosticsSpec(BOXA* boxad, const BOXA* boxas)
{
	if (!boxas)
		return ERROR_INT("boxas not defined", __func__, 1);
	if (!boxad)
		return ERROR_INT("boxad not defined", __func__, 1);
	if (boxad->diag_spec == boxas->diag_spec)
		return 0;
	leptDestroyDiagnoticsSpecInstance(&boxad->diag_spec);
	if (boxas->diag_spec) {
		boxad->diag_spec = leptCloneDiagnoticsSpecInstance(boxas->diag_spec);
	}
	else {
		boxad->diag_spec = NULL;
	}
	return 0;
}

#endif


/*!
 * \brief   leptDebugSetFileBasepath()
 *
 * \param[in]    directory         when relative: a path inside /tmp/lept/ where all target files are meant to land.
 *
 * <pre>
 * Notes:
 *      (1) The given directory will be used for every debug plot, image, etc. file produced by leptonica.
 *          This is useful when, for example, processing source images in bulk and you wish to quickly
 *          locate the relevant debug/diagnostics outputs for a given source image.
 *      (2) By passing NULL, the base path is reset to the default: /tmp/lept/debug/
 *      (3) Changing the base path is assumed to imply you're going to run another batch: the %batch_unique_id
 *          will be incremented upon next use.
 * </pre>
 */
void
leptDebugSetFileBasepath(LDIAG_CTX spec, const char* directory)
{
	if (!directory) {
		directory = "";
	}

	if (spec->basepath)
		stringDestroy(&spec->basepath);

	if (!directory[0]) {
		spec->basepath = NULL;
	}
	else {
		l_int32 root_len = getPathRootLength(directory);
		// when the given directory path is an absolute path, we use it as-is: the user clearly wishes
		// to overide the usual /tmp/lept/... destination tree.
		if (root_len > 0) {
			spec->basepath = stringNew(directory);
		}
		else {
			// TODO:
			// do we allow '../' elements in a relative directory spec?
			// 
			// Current affairs assume this path is set by (safe) application code,
			// rather than (unsafe) arbitrary end user input...
			spec->basepath = pathJoin(leptDebugGetFileBasePath(spec), directory);
		}
	}

	spec->basepath_minlength = spec->basepath ? strlen(spec->basepath) : 0;

	spec->must_regenerate = 1;
}


/*!
 * \brief   leptDebugAppendFileBasepath()
 *
 * \param[in]    directory         always interpreted as relative to the currently configured base path (using /tmp/lept/ when NULL) where all target files are meant to land.
 *
 * <pre>
 * Notes:
 *      (1) The given directory will be used for every debug plot, image, etc. file produced by leptonica.
 *          This is useful when, for example, processing source images in bulk and you wish to quickly
 *          locate the relevant debug/diagnostics outputs for a given source image.
 *      (2) Changing the base path is not assumed to imply you're going to run another batch, unlike when you use leptDebugSetFileBasepath().
 * </pre>
 */
void
leptDebugAppendFileBasepath(LDIAG_CTX spec, const char* directory)
{
	if (!directory) {
		directory = "";
	}

	if (directory[0]) {
		if (spec->basepath) {
			const char* base = spec->basepath;
			// TODO:
			// do we allow '../' elements in a relative directory spec?
			// 
			// Current affairs assume this path is set by (safe) application code,
			// rather than (unsafe) arbitrary end user input...
			spec->basepath = pathJoin(base, directory);
			stringDestroy(&base);
		}
		else {
			// TODO:
			// do we allow '../' elements in a relative directory spec?
			// 
			// Current affairs assume this path is set by (safe) application code,
			// rather than (unsafe) arbitrary end user input...
			spec->basepath = pathJoin(leptDebugGetFileBasePath(spec), directory);
		}
	}
	else if (!spec->basepath) {
		spec->basepath = stringNew(leptDebugGetFileBasePath(spec));
	}

	// ALWAYS recalculate the minlength as AppendFileBasepath() MAY immediately follow a FilePathPart() API call,
	// thus including that previously-non-mandatory part in the basepath now, which implies the basebath HAS
	// changed anyhow, even when %directory == "".
	spec->basepath_minlength = strlen(spec->basepath);

	spec->must_regenerate = 1;
}


void
leptDebugAppendFilePathPart(LDIAG_CTX spec, const char* directory)
{
	if (!directory) {
		directory = "";
	}

	if (!spec->basepath) {
		spec->basepath = stringNew(leptDebugGetFileBasePath(spec));
		spec->basepath_minlength = strlen(spec->basepath);
	}

	if (directory[0]) {
		const char* base = spec->basepath;
		// TODO:
		// do we allow '../' elements in a relative directory spec?
		// 
		// Current affairs assume this path is set by (safe) application code,
		// rather than (unsafe) arbitrary end user input...
		spec->basepath = pathJoin(base, directory);
		stringDestroy(&base);
	}

	spec->must_regenerate = 1;
}

void
leptDebugReplaceEntireFilePathPart(LDIAG_CTX spec, const char* directory)
{
	if (!directory) {
		directory = "";
	}

	if (!spec->basepath) {
		spec->basepath = stringNew(leptDebugGetFileBasePath(spec));
		spec->basepath_minlength = strlen(spec->basepath);
	}

	const char* base = spec->basepath;
	spec->basepath = stringCopySegment(base, 0, spec->basepath_minlength);
	stringDestroy(&base);

	if (directory[0]) {
		leptDebugAppendFilePathPart(spec, directory);
	}
	spec->must_regenerate = 1;
}


void
leptDebugReplaceOneFilePathPart(LDIAG_CTX spec, const char* directory)
{
	if (!directory) {
		directory = "";
	}

	if (!spec->basepath) {
		spec->basepath = stringNew(leptDebugGetFileBasePath(spec));
		spec->basepath_minlength = strlen(spec->basepath);
	}

	const char* base = spec->basepath;
	char* dir;
	splitPathAtDirectory(base, &dir, NULL);

	size_t baselen = strlen(dir);
	if (baselen >= spec->basepath_minlength) {
		if (directory[0]) {
			spec->basepath = pathJoin(dir, directory);
			LEPT_FREE(dir); // stringDestroy(&dir);
		}
		else {
			spec->basepath = dir;
		}
		stringDestroy(&base);
	}
	else {
		L_WARNING("Attempting to replace forbidden base path part! (base: \"%s\", replace: \"%s\")\n", __func__, base, directory);
		LEPT_FREE(dir); // stringDestroy(&dir);
	}

	spec->must_regenerate = 1;
}


void
leptDebugEraseFilePathPart(LDIAG_CTX spec)
{
	if (!spec->basepath) {
		spec->basepath = stringNew(leptDebugGetFileBasePath(spec));
		spec->basepath_minlength = strlen(spec->basepath);
	}
	else {
		const char* base = spec->basepath;
		spec->basepath = stringCopySegment(base, 0, spec->basepath_minlength);
		stringDestroy(&base);
	}

	spec->must_regenerate = 1;
}


void leptDebugEraseOneFilePathPart(LDIAG_CTX spec)
{
	if (!spec->basepath) {
		spec->basepath = stringNew(leptDebugGetFileBasePath(spec));
		spec->basepath_minlength = strlen(spec->basepath);
	}

	const char* base = spec->basepath;
	char* dir;
	splitPathAtDirectory(base, &dir, NULL);

	size_t baselen = strlen(dir);
	if (baselen >= spec->basepath_minlength) {
		spec->basepath = dir;
		stringDestroy(&base);
	}
	else {
		L_WARNING("Attempting to erase part of the restricted base path! (base: \"%s\")\n", __func__, base);
		LEPT_FREE(dir); // stringDestroy(&dir);
	}

	spec->must_regenerate = 1;
}


const char*
leptDebugGetFilePathPart(LDIAG_CTX spec)
{
	const char* rv = spec->basepath + spec->basepath_minlength;
	if (strchr("\\/", *rv))
		rv++;
	return rv;
}


/*!
 * \brief   leptDebugGetFileBasePath()
 *
 * \return  the previously set target filename base path; usually pointing somewhere inside the /tmp/lept/ directory tree.
 */
const char*
leptDebugGetFileBasePath(LDIAG_CTX spec)
{
	if (!spec || !spec->basepath) {
		return "/tmp/lept/debug";
	}
	return spec->basepath;
}


/*!
 * \brief   leptDebugSetFilenameForPrefix()
 *
 * \param[in]    source_filename           the path to the file; may be relative or absolute or just the file name itself.
 * \param[in]    strip_off_extension
 *
 * <pre>
 * Notes:
 *      (1) The given prefix will be added to every debug plot, image, etc. file produced by leptonica.
 *          This is useful when, for example, processing source images in bulk and you wish to quickly
 *          locate the relevant debug/diagnostics outputs for a given source image.
 *      (2) By passing NULL, the prefix is erased.
 * </pre>
 */
void
leptDebugSetFilenameForPrefix(LDIAG_CTX spec, const char* source_filename, l_ok strip_off_extension)
{
	if (spec->active_filename)
		stringDestroy(&spec->active_filename);
	if (!source_filename) {
		spec->active_filename = NULL;
	}
	else {
		// when a full path has been specified, strip off any directories: we assume the filename is
		// pretty unique by itself. When it isn't, there's little loss, as we also have the batch run #
		// and the process step # to help make the target uniquely named.
		spec->active_filename = getPathBasename(source_filename, strip_off_extension);
	}

	spec->must_regenerate = 1;
}


/*!
 * \brief   leptDebugGetFilenamePrefix()
 *
 * \return  the previously set source filename meant for use as part of the target prefix string.
 *
 * <pre>
 * Notes:
 *      (1) This source filename has NOT been sanitized yet: it may contain 'dangerous characters'
 *          when used directly for any target file path construction.
 *      (2) Will return NULL when not yet set (or when RESET).
 *      (3) When you want to use the cleaned-up and preformatted filename prefix, use the
 *          leptDebugGenFilename() API instead.
 * </pre>
 */
const char*
leptDebugGetFilenameForPrefix(LDIAG_CTX spec)
{
	return spec->active_filename;
}


static inline l_ok
steps_is_level_forever_increasing(uint16_t depth, unsigned int forever_increasing_mask)
{
	unsigned int mask = 1U << depth;
	return (forever_increasing_mask & mask) != 0;
}


// return TRUE when new numeric value may be assigned to this steps level/depth.
static inline l_ok
steps_level_numeric_value_compares(uint16_t depth, unsigned int forever_increasing_mask, uint32_t level_numeric_id, uint32_t new_numeric_id)
{
	if (steps_is_level_forever_increasing(depth, forever_increasing_mask))
		// we cannot ever DECREMENT any step level: that would break our premise of delivering a unique hierarchical number set.
		return (level_numeric_id < new_numeric_id);
	else
		return (level_numeric_id != new_numeric_id);
}


/*!
 * \brief   leptDebugSetStepId()
 *
 * \param[in]    numeric_id     sequence number; set to 0 to reset the sequence.
 *
 * <pre>
 * Notes:
 *      (1) The given id will be added to every debug plot, image, etc. file produced by leptonica.
 *          This is useful when, for example, processing source images in bulk and you wish to quickly
 *          locate the relevant debug/diagnostics outputs for a given source image.
 *      (2) On every change (increment or otherwise) of the batch id, both the step id and
 *          substep item id will be RESET.
 * </pre>
 */
void
leptDebugSetStepId(LDIAG_CTX spec, uint32_t numeric_id)
{
	// note: when we're at the maximum depth, we cannot change the step id any further, but for auto-incrementing (which is enforced at that level)
	if (spec->steps.actual_depth == L_MAX_STEPS_DEPTH - 1) {
		spec->must_bump_step_id = 0;
		spec->must_regenerate = 1;
		++spec->steps.vals[L_MAX_STEPS_DEPTH - 1];
	}
	else if (numeric_id == 0) {
		spec->must_bump_step_id = 1;
		spec->must_regenerate = 1;
		++spec->steps.vals[L_MAX_STEPS_DEPTH - 1];
	}
	else if (spec->steps.vals[spec->steps.actual_depth] != numeric_id) {
		if (steps_level_numeric_value_compares(spec->steps.actual_depth, spec->step_id_is_forever_increasing, spec->steps.vals[spec->steps.actual_depth], numeric_id)) {
			spec->steps.vals[spec->steps.actual_depth] = numeric_id;
			spec->must_bump_step_id = 0;
			spec->must_regenerate = 1;
			++spec->steps.vals[L_MAX_STEPS_DEPTH - 1];
		}
		else {
			spec->must_bump_step_id = 1;
			spec->must_regenerate = 1;
			++spec->steps.vals[L_MAX_STEPS_DEPTH - 1];
		}
	}
}


void
leptDebugIncrementStepId(LDIAG_CTX spec)
{
	++spec->steps.vals[spec->steps.actual_depth];
	spec->must_bump_step_id = 0;
	spec->must_regenerate = 1;
	if (spec->steps.actual_depth < L_MAX_STEPS_DEPTH - 1) {
		++spec->steps.vals[L_MAX_STEPS_DEPTH - 1];
	}
}


/*!
 * \brief   leptDebugGetStepId()
 *
 * \return  the previously set step sequence id at the current depth. Will be 1(one) when the sequence has been reset.
 */
uint32_t
leptDebugGetStepId(LDIAG_CTX spec)
{
	if (spec->must_bump_step_id) {
		leptDebugIncrementStepId(spec);
	}
	return spec->steps.vals[spec->steps.actual_depth];
}


// returned string is kept in the cache, so no need to destroy/free by caller.
const char*
leptDebugGetStepIdAsString(LDIAG_CTX spec)
{
	if (spec->must_bump_step_id) {
		leptDebugIncrementStepId(spec);
	}
	const size_t bufsize = L_MAX_STEPS_DEPTH * 11 + 1; // max 10 digits (for uint32_t value) + '.' per level + '\0'
	char* buf = spec->last_generated_step_id_string;
	if (!buf) {
		buf = (char*)LEPT_CALLOC(bufsize, sizeof(char));
		spec->last_generated_step_id_string = buf;
	}
	char* p = buf;
	char* e = buf + bufsize;
	for (uint16_t i = 0; i <= spec->steps.actual_depth; i++) {
		unsigned int v = spec->steps.vals[i];    // MSVC complains about feeding a l_atomic into a variadic function like printf() (because I was compiling in forced C++ mode, anyway). This hotfixes that.
		int n = snprintf(p, e - p, "%u.", v);
		p += n;
	}
	p--;  // discard the trailing '.'
	*p = 0;

	return buf;
}


void
leptDebugMarkStepIdForIncrementing(LDIAG_CTX spec)
{
	spec->must_bump_step_id = 1;
}


void
leptDebugSetStepLevelAsForeverIncreasing(LDIAG_CTX spec, l_ok enable)
{
	if (enable) {
		unsigned int mask = 1U << spec->steps.actual_depth;
		spec->step_id_is_forever_increasing |= mask;
	}
	else if (spec->steps.actual_depth < L_MAX_STEPS_DEPTH - 1) {
		unsigned int mask = ~(1U << spec->steps.actual_depth);
		spec->step_id_is_forever_increasing &= mask;
	}
}


static inline void
steps_reset_sub_levels(StepsArray* steps, uint16_t depth, uint16_t max_depth, unsigned int forever_increasing_mask)
{
	for (uint16_t d = depth; d <= max_depth; d++) {
		unsigned int mask = 1U << d;
		if ((forever_increasing_mask & mask) == 0) {
			steps->vals[d] = 0;
		}
	}
}


/*!
 * \brief   leptDebugSetStepIdAtDepth()
 *
 * \param[in]    relative_depth   0: current depth, -1/+1: parent depth, ...
 * \param[in]    numeric_id       (batch) step sequence number
 *
 * <pre>
 * Notes:
 *      (1) The given step id will be added to every debug plot, image, etc. file produced by leptonica.
 *          This is useful when, for example, processing source images in bulk and you wish to quickly
 *          locate the relevant debug/diagnostics outputs for a given source image.
 *      (2) By passing 0, the step sequence is reset.
 *      (3) On every change (increment or otherwise) of the step id, the substep item id will be RESET.
 *      (3) On every change (increment or otherwise) of the batch id, the step id will already have been RESET.
 * </pre>
 */
void
leptDebugSetStepIdAtDepth(LDIAG_CTX spec, int relative_depth, uint32_t numeric_id)
{
	// we can only address current or parent levels!
	if (relative_depth > 0)
		relative_depth = -relative_depth;

	int target_depth = spec->steps.actual_depth + relative_depth;
	if (target_depth < 0)
		target_depth = 0;
	else if (target_depth >= L_MAX_STEPS_DEPTH)
		target_depth = L_MAX_STEPS_DEPTH - 1;

	if (target_depth == spec->steps.actual_depth) {
		leptDebugSetStepId(spec, numeric_id);
		return;
	}

	// here we only address any parent levels, hence we won't be touching level (L_MAX_STEPS_DEPTH - 1) which simplifies matters.
	if (numeric_id == 0) {
		++spec->steps.vals[target_depth];
		steps_reset_sub_levels(&spec->steps, target_depth + 1, spec->steps.actual_depth, spec->step_id_is_forever_increasing);
		spec->must_bump_step_id = 1;
		spec->must_regenerate = 1;
		++spec->steps.vals[L_MAX_STEPS_DEPTH - 1];
	}
	else if (spec->steps.vals[target_depth] != numeric_id) {
		if (steps_level_numeric_value_compares(target_depth, spec->step_id_is_forever_increasing, spec->steps.vals[target_depth], numeric_id)) {
			spec->steps.vals[target_depth] = numeric_id;
			steps_reset_sub_levels(&spec->steps, target_depth + 1, spec->steps.actual_depth, spec->step_id_is_forever_increasing);
			spec->must_bump_step_id = 1;
			spec->must_regenerate = 1;
			++spec->steps.vals[L_MAX_STEPS_DEPTH - 1];
		}
		else {
			++spec->steps.vals[target_depth];
			steps_reset_sub_levels(&spec->steps, target_depth + 1, spec->steps.actual_depth, spec->step_id_is_forever_increasing);
			spec->must_bump_step_id = 1;
			spec->must_regenerate = 1;
			++spec->steps.vals[L_MAX_STEPS_DEPTH - 1];
		}
	}
}


void
leptDebugIncrementStepIdAtDepth(LDIAG_CTX spec, int relative_depth)
{
	// we can only address current or parent levels!
	if (relative_depth > 0)
		relative_depth = -relative_depth;

	int target_depth = spec->steps.actual_depth + relative_depth;
	if (target_depth < 0)
		target_depth = 0;
	else if (target_depth >= L_MAX_STEPS_DEPTH)
		target_depth = L_MAX_STEPS_DEPTH - 1;

	if (target_depth == spec->steps.actual_depth) {
		leptDebugIncrementStepId(spec);
		return;
	}

	// here we only address any parent levels, hence we won't be touching level (L_MAX_STEPS_DEPTH - 1) which simplifies matters.
	++spec->steps.vals[target_depth];
	steps_reset_sub_levels(&spec->steps, target_depth + 1, spec->steps.actual_depth, spec->step_id_is_forever_increasing);
	spec->must_bump_step_id = 1;
	spec->must_regenerate = 1;
	++spec->steps.vals[L_MAX_STEPS_DEPTH - 1];
}


uint32_t
leptDebugGetStepIdAtLevel(LDIAG_CTX spec, int relative_depth)
{
	// we can only address current or parent levels!
	if (relative_depth > 0)
		relative_depth = -relative_depth;

	int target_depth = spec->steps.actual_depth + relative_depth;
	if (target_depth < 0)
		target_depth = 0;
	else if (target_depth >= L_MAX_STEPS_DEPTH)
		target_depth = L_MAX_STEPS_DEPTH - 1;

	if (target_depth == spec->steps.actual_depth) {
		return leptDebugGetStepId(spec);
	}

	// spec->must_bump_step_id only applies to the active depth level, not any parent level.
	return spec->steps.vals[target_depth];
}


void
leptDebugSetStepLevelAtLevelAsForeverIncreasing(LDIAG_CTX spec, int relative_depth, l_ok enable)
{
	// we can only address current or parent levels!
	if (relative_depth > 0)
		relative_depth = -relative_depth;

	int target_depth = spec->steps.actual_depth + relative_depth;
	if (target_depth < 0)
		target_depth = 0;
	else if (target_depth >= L_MAX_STEPS_DEPTH)
		target_depth = L_MAX_STEPS_DEPTH - 1;

	if (target_depth == spec->steps.actual_depth) {
		leptDebugSetStepLevelAsForeverIncreasing(spec, enable);
		return;
	}

	if (enable) {
		unsigned int mask = 1U << target_depth;
		spec->step_id_is_forever_increasing |= mask;
	}
	else if (target_depth < L_MAX_STEPS_DEPTH - 1) {
		unsigned int mask = ~(1U << target_depth);
		spec->step_id_is_forever_increasing &= mask;
	}
}


NUMA*
leptDebugGetStepNuma(LDIAG_CTX spec)
{
	NUMA* numa = numaCreate(L_MAX_STEPS_DEPTH);
	for (uint16_t i = 0; i < spec->steps.actual_depth; i++) {
		numaAddNumber(numa, spec->steps.vals[i]);
	}
	return numa;
}


uint16_t
leptDebugGetStepDepth(LDIAG_CTX spec)
{
	return spec->steps.actual_depth;
}


uint16_t leptDebugAddStepLevel(LDIAG_CTX spec)
{
	if (spec->steps.actual_depth < L_MAX_STEPS_DEPTH - 2) {
		++spec->steps.actual_depth;
		spec->steps.vals[spec->steps.actual_depth] = 0;
		spec->must_bump_step_id = 1;
		spec->must_regenerate = 1;
	}
	else {
		spec->steps.actual_depth = L_MAX_STEPS_DEPTH - 1;
		spec->must_bump_step_id = 1;
		spec->must_regenerate = 1;
	}
	return spec->steps.actual_depth;
}


uint32_t
leptDebugPopStepLevel(LDIAG_CTX spec)
{
	uint32_t rv = spec->steps.vals[spec->steps.actual_depth];
	if (spec->steps.actual_depth > 0) {
		--spec->steps.actual_depth;
		spec->must_bump_step_id = 1;
		spec->must_regenerate = 1;
	}
	return rv;
}


void
leptDebugSetHashId(LDIAG_CTX spec, uint64_t hash_id)
{
	if (spec->active_hash_id != hash_id) {
		spec->must_regenerate = 1;
		spec->active_hash_id = hash_id;
	}
}


uint64_t
leptDebugGetHashId(LDIAG_CTX spec)
{
	return spec->active_hash_id;
}


/*!
 * \brief   leptDebugSetProcessName()
 *
 * \param[in]    name
 *
 * <pre>
 * Notes:
 *      (1) The given name will (in shortened and sanitized form) be added to every debug plot, image, etc. file produced by leptonica.
 *          This is useful when, for example, processing source images in bulk and you wish to quickly
 *          locate the relevant debug/diagnostics outputs for a given source image.
 *      (2) By passing NULL, the name is erased.
 * </pre>
 */
void
leptDebugSetProcessName(LDIAG_CTX spec, const char* name)
{
	if (spec->process_name) {
		stringDestroy(&spec->process_name);
	}
	if (name && *name) {
		spec->process_name = stringNew(name);
	}

	spec->must_regenerate = 1;
}


/*!
 * \brief   leptDebugGetProcessName()
 *
 * \return  the previously set process name meant for use as part of the target prefix string.
 *
 * <pre>
 * Notes:
 *      (1) This process name has NOT been sanitized yet: it may contain 'dangerous characters'
 *          when used directly for any target file path construction.
 *      (2) Will return NULL when not yet set (or when RESET).
 *      (3) When you want to use the cleaned-up and preformatted process name, use the
 *          leptDebugGenFilename() API instead.
 * </pre>
 */
const char*
leptDebugGetProcessName(LDIAG_CTX spec)
{
	return spec->process_name;
}


/*!
 * \brief   leptDebugSetFilepathDefaultFormat()
 *
 * \param[in]    path_template_str     the filepath/name template to use when generating new target file paths.
 *
 * <pre>
 * Notes:
 *      (1) The given %path_template_str is always processed as a relative path, appended to the preset base path.
 *          This ensures that all generated file paths will remain inside the preset base path directory tree.
 *          Indeed, attempts to walk outside this dir tree by using one or more '../' path elements in your
 *          %path_template_str is rejected with an error and the given %path_template_str will be rejected.
 *      (2) By passing NULL, the default %path_template_str will be used:
 *               'arbitrar/{5b}.{2s}.{3i}#{5H}.{30f:@#-_.~}.{20p:@#-_.~}-{20t:@#-_.~}.'
 *      (3) These '{nx:@}' template elements are recognized:
 *          - The field type 'x' can be:
 *            + 'b' : batch [unique] id (numeric, managed by the application code via leptDebugSetBetchUniqueId() et al). Default width: 5.
 *            + 's' : step id (numeric, managed by the application code via leptDebugSetStepUniqueId() et al). Default width: 2.
 *            + 'i' : item id (numeric, auto-incremented for each leptDebugGenFilename() / leptDebugGenFilepath() request;
 *                    optionally managed by the application code via leptDebugSetItemId() et al). Default width: 3.
 *            + 'f' : the filename as set by leptDebugSetFilenameForPrefix(), properly sanitized and shortended to fit the indicated field width.
 *                    When empty/NULL, this field, plus its immediately preceding non-alphanumeric separator character in the template format string
 *                    will be removed from the resulting filename/path, i.e. when NULL this chunk will be utterly absent from the output.
 *                    Default width: 30.
 *            + 'p' : the process name as set by leptDebugSetProcessName(), properly sanitized and shortended to fit the indicated field width.
 *                    When empty/NULL, this field, plus its immediately preceding non-alphanumeric separator character in the template format string
 *                    will be removed from the resulting filename/path, i.e. when NULL this chunk will be utterly absent from the output.
 *                    Default width: 20.
 *            + 't' : the filename/'title' part of the template string provided by the leptDebugGenFilename() / leptDebugGenFilpath() API call.
 *                    This allows us to use simple identifier-like strings for those API calls, while we keep the default template format
 *                    involved to help produce unique and deterministically identifiable filenames & ~paths for all output files in a large
 *                    multi-batch application run.
 *                    Default width: 20.
 *            + 'H' : hash, as calculated from the collective source elements (batch#, step#, item#, filename, processname, title) and base58-encoded.
 *                    Default width: 5.
 *          - The field width 'n' can be:
 *            - for *numeric* fields: the target width, i.e. the width to which the numeric field will be *expanded*, by using leading '0' zeroes.
 *              Ergo, this is similar to C `printf("%0*u", n, field)` behaviour: the field will be AT LEAST this wide and all-numeric-digits.
 *            - for *hash* fields: the target width, i.e. the number of alphanumeric (ASCII) characters the hash will be compacted into.
 *            - for *text/string* fields: the maximum target width, i.e. the (sanitized) field will be shortended to fit within this maximum width.
 *              The 'filename' field ('f') is left-truncated, i.e. the tail end is kept intact as much as possible.
 *              The 'processname' field ('p') is middle-truncated, i.e. both leading part and tail end are kept intact as much as possible, while
 *              the middle section will be removed to make the 'processname' fit.
 *          - The OPTIONAL text sanitizer filter expression '@' field, following the ':' colon in the format spec, is only accepted and used
 *            with the text fields ('f', 'p' and 't'): this is a series of *additionally accepted characters* that (next to all 26+26+10 ASCII
 *            alphanumeric characters) will be passed unsanitized from the source.
 *
 *            The very last character in this series is also designated as the replacement character for any character sequence in the
 *            source that gets sanitized.
 *
 *            When the sanitizer filter spec is empty, the default '_' string is assumed and text fields will each be sanitized suitable for
 *            all (non-antiquated) file systems -- MSDOS/FAT comes to mind: there we have a 8.3 filename size restriction,
 *            which is not met by our default template; if you wish to support such a filesystem, you are well-advised to use
 *            a '{8H}.' template instead to ensure your filenames at least will suit the target filesystem. But I digress...
 *          - Whatever the OPTIONAL text sanitizer filter expression '@' field may be, empty or otherwise, any sanitized string will:
 *            + have any sanitized character sequence replaced by a single replacement character,
 *            + have both head and tail ends trimmed to either a single '_' underscore or nothing-at-all.
 *
 *            The latter rule is to prevent ever generating filenames which start or end with '.' dots or one or more '~' or '$' characters, which
 *            have special meaning and/or effects in many operation systems. In other words: the sanitizer prevents you from ever being able
 *            to produce filenames such as UNIX-hidden '.gitignore' or NTFS-endangering '$Index' or Unix-hairy '~home` when passing anything
 *            through our sanitization filter. We also do not ever produce filenames that might look like command line arguments, such as '-xvzf'
 *            when using a format spec like this one: '{t:@_-}'
 *		(4) Only when the leptDebugGenFilename() / leptDebugGenFilpath() API calls supply their own template strings containing
 *          at least one or more of the supported template fields as specified above, will the default template be overridden by the
 *          ad hoc specified template string. This behaviour has been chosen to allow both very simple and bespoke template strings
 *         as first leptDebugGenFilename() / leptDebugGenFilpath() argument.
 *
 *          I.e. when you want to (locally) OVERRIDE the configured default behaviour,
 *          your leptDebugGenFilename() / leptDebugGenFilpath() API call MUST use a template format string
 *          which contains AT LEAST one valid, supported '{nx}' template format field!
 *          Hence
 *
 *               const char *s = leptDebugGenFilpath("/b0rk.png")
 *
 *          will use the preconfigured/default template and produce a path like this one:
 *
 *               s == "/tmp/lept/debug/arbitrar/00001.01.001#Xq74A.fname~4~prefix.{5b}.{2s}.{3i}#{5H}.{30f:@#-_~}.{20p:@#-_~}-{20t:@#-_~}.'
 *
 *
 *            + 'b' : batch [unique] id (numeric, managed by the application code via leptDebugSetBetchUniqueId() et al)
 *            + 'b' : batch [unique] id (numeric, managed by the application code via leptDebugSetBetchUniqueId() et al)
 *            + 'b' : batch [unique] id (numeric, managed by the application code via leptDebugSetBetchUniqueId() et al)
 *            + 'b' : batch [unique] id (numeric)
 *          - if 'nn' is a zero-prefixed decimal number, the field is always extended to this width, using leading
 *            zeroes if necessary. Thus '{05i}' is equivalent to C `printf("%05u", item_id)`.
 *          - If 'nn' is a non-zero-prefixed decimal number and the field type (x) is numeric, , it : the maximum width of the field.
 *            If 'nn' is 0-prefixed, the field is always extended to its maximum width, using leading zeroes.
 *            Hence '{5i}' is equivalent to C `printf("%.5s", ...)`
 *      (3) To keep matters sane all around, the prefix size is limited to 40 characters and is sanitized
 *          by sanitizePathToIdentifier(...,%numeric_id,%filename_prefix) before use.
 * </pre>
 */
void
leptDebugSetFilepathDefaultFormat(LDIAG_CTX spec, const char* path_template_str)
{
	// TODO: parse format string and check for errors.

	if (spec->default_path_template_str) {
		stringDestroy(&spec->default_path_template_str);
	}
	if (path_template_str && *path_template_str) {
		spec->default_path_template_str = stringNew(path_template_str);
	}
	else {
		spec->default_path_template_str = stringNew("arbitrar/{5b}.{2s}.{3i}#{5H}.{30f:@#-_.~}.{20p:@#-_.~}-{20t:@#-_.~}");
	}

	spec->must_regenerate = 1;

}


/*!
 * \brief   leptDebugGetFilenamePrefix()
 *
 * \return  the previously set filename prefix string or an empty string if no prefix has been set up.
 */
const char*
leptDebugGetFilepathDefaultFormat(LDIAG_CTX spec)
{
	return spec->default_path_template_str;
}


/*!
 * \brief   leptDebugGenFilename()
 *
 * \return  the previously set filename prefix string or an empty string if no prefix has been set up.
 */
const char*
leptDebugGenFilename(LDIAG_CTX spec, const char* filename_fmt_str, ...)
{
	va_list va;
	va_start(va, filename_fmt_str);
	const char* fn = string_vasprintf(filename_fmt_str, va);
	va_end(va);
	const char* path = leptDebugGetFileBasePath(spec);
	const char* f = pathJoin(path, fn);
	stringDestroy(&fn);
	stringDestroy(&path);
	return f;
}


/*!
 * \brief   leptDebugGenFilepath()
 *
 * \param[in]    template_printf_fmt_str
 *
 * <pre>
 * Notes:
 *
 *
 * </pre>
 */
const char*
leptDebugGenFilepath(LDIAG_CTX spec, const char* path_fmt_str, ...)
{
	va_list va;
	va_start(va, path_fmt_str);
	const char *fn = string_vasprintf(path_fmt_str, va);
	va_end(va);
	const char* path = leptDebugGetFileBasePath(spec);
	const char* f = pathJoin(path, fn);
	stringDestroy(&fn);
	stringDestroy(&path);
	return f;
}


const char*
leptDebugGenFilenameEx(const char* directory, LDIAG_CTX spec, const char* filename_fmt_str, ...)
{
	va_list va;
	va_start(va, filename_fmt_str);
	const char* fn = string_vasprintf(filename_fmt_str, va);
	va_end(va);
	const char* path = leptDebugGetFileBasePath(spec);
	const char* f = pathJoin(path, fn);
	stringDestroy(&fn);
	stringDestroy(&path);
	return f;
}


const char*
leptDebugGenFilepathEx(const char* directory, LDIAG_CTX spec, const char* path_fmt_str, ...)
{
	va_list va;
	va_start(va, path_fmt_str);
	const char* fn = string_vasprintf(path_fmt_str, va);
	va_end(va);
	const char* path = leptDebugGetFileBasePath(spec);
	const char* f = pathJoin(path, fn);
	stringDestroy(&fn);
	stringDestroy(&path);
	return f;
}


/*!
 * \brief   leptDebugGetFilenamePrefix()
 *
 * \return  the previously set filename prefix string or an empty string if no prefix has been set up.
 */
const char*
leptDebugGetLastGenFilepath(LDIAG_CTX spec)
{
	return spec->active_filename;
}


l_ok
leptIsInDisplayMode(LDIAG_CTX spec)
{
	if (!spec)
		return ERROR_INT("spec not defined", __func__, 0);
	return spec->display;
}


void
leptSetInDisplayMode(LDIAG_CTX spec, l_ok activate)
{
	if (!spec) {
		L_ERROR("spec not defined", __func__);
		return;
	}
	spec->display = !!activate;
}


/*
* DebugMode APIs tolerate a NULL spec pointer, returning FALSE or doing nothing, IFF activate == FALSE.
*
* This means that any attempt to *activate* debug mode without a valid spec pointer will produce an error message,
* while any attempt to DE-activate debug mode without a valid spec pointer will be silently accepted.
*/

l_ok
leptIsDebugModeActive(LDIAG_CTX spec)
{
	if (!spec) {
		//return ERROR_INT("spec not defined", __func__, 0);
		return 0;
	}
	return spec->debugging;
}


void
leptActivateDebugMode(LDIAG_CTX spec, l_ok activate)
{
	if (!spec && activate) {
		L_ERROR("spec not defined", __func__);
		return;
	}
	if (spec) {
		spec->debugging = !!activate;
	}
}


/*
* Shorthand function which returns the given diagspec if debug mode has been activated.
*
* Intended use: inside function calls where the diagspec is passed as an argument.
*/
LDIAG_CTX
leptPassIfDebugModeActive(LDIAG_CTX spec)
{
	if (leptIsDebugModeActive(spec))
		return spec;
	return NULL;
}


/*
* Shorthand versions for PIX and PIXA structures.
*/


l_ok pixIsDebugModeActive(PIX* pix)
{
	LDIAG_CTX diagspec = pixGetDiagnosticsSpec(pix);
	return leptIsDebugModeActive(diagspec);
}


void pixActivateDebugMode(PIX* pix, l_ok activate)
{
	LDIAG_CTX diagspec = pixGetDiagnosticsSpec(pix);
	leptActivateDebugMode(diagspec, activate);
}


LDIAG_CTX
pixPassDiagIfDebugModeActive(PIX* pix)
{
	LDIAG_CTX diagspec = pixGetDiagnosticsSpec(pix);
	return leptPassIfDebugModeActive(diagspec);
}


l_ok
pixaIsDebugModeActive(PIXA* pixa)
{
	LDIAG_CTX diagspec = pixaGetDiagnosticsSpec(pixa);
	return leptIsDebugModeActive(diagspec);
}


void
pixaActivateDebugMode(PIXA* pixa, l_ok activate)
{
	LDIAG_CTX diagspec = pixaGetDiagnosticsSpec(pixa);
	leptActivateDebugMode(diagspec, activate);
}


LDIAG_CTX
pixaPassDiagIfDebugModeActive(PIXA* pixa)
{
	LDIAG_CTX diagspec = pixaGetDiagnosticsSpec(pixa);
	return leptPassIfDebugModeActive(diagspec);
}

















