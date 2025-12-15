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
#include <stdint.h>

#ifdef _WIN32
#include <intrin.h>			// __rdtsc()
#else
#include <x86intrin.h>      // __rdtsc()
#endif

#include "allheaders.h"
#include "pix_internal.h"


/*
 * The design/concept with the diagspec APIs is that those MUST tolerate NULL valued LDIAG_CTX arguments, 
 * so that the rest of the code can use the simplest possible usage of these APIs: minimal cluttering of 
 * other parts of the codebase is the intent. 
 * 
 * This preprocessor setting is set to 1 to allow extra *warnings* to be logged while we are still 
 * debugging/trailing this API set.
 */
#define DEBUGGING              1


#if DEBUGGING
#define DBG_WARN(...)				L_WARNING(__VA_ARGS__)
#else
#define DBG_WARN(...)				/**/
#endif


/*--------------------------------------------------------------------*
 *         Image Debugging & Diagnostics Helper functions             *
 *--------------------------------------------------------------------*/


typedef struct StepsArray {
#define L_MAX_STEPS_DEPTH 10         // [batch:0, step:1, item:2, level:3, sublevel:4, ...]
	unsigned int actual_depth;
	l_atomic vals[L_MAX_STEPS_DEPTH];
	// Note: the last depth level is permanently auto-incrementing and serves as a 'persisted' %item_id_is_forever_increasing counter.
} StepsArray;

typedef struct FilepathCache {
#define L_PATH_CACHE_SIZE 16         // minimum requirement due to some regression test implementations in /prog/: 2
	unsigned int active_index;
	const char * path[L_PATH_CACHE_SIZE];
} FilepathCache;

struct l_diag_predef_parts {
	const char* basepath;            //!< the base path within where every generated file must land.
#if 0
	NUMA *steps;                     //!< a hierarchical step numbering system; the last number level will (auto)increment, or RESET every time when a more major step level incremented/changed (unless %item_id_is_forever_increasing is set).
#else
	struct StepsArray steps;         //!< a hierarchical step numbering system; the last number level will (auto)increment, or RESET every time when a more major step level incremented/changed (unless %item_id_is_forever_increasing is set).
#endif
	unsigned int basepath_minlength; //!< minimum length of the basepath, i.e. append/remove/replace APIs are prohibited from editing the leading part of basepath that is shorter than this length.

	uint64_t active_hash_id;         //!< derived from the originally specified %active_filename path, or set explicitly (if user-land code has a better idea about what value this should be).
	const char* filename_prefix;     //!< a file name prefix, NULL if unspecified. To be used (in reduced form) as a target filename prefix.
	const char* filename_basename;   //!< a file name basename, NULL if unspecified. To be used (in reduced form) as a target filename base, after the prefix.
	const char* process_name;        //!< the part which identifies what we are doing right now (at a high abstraction level)
	const char* default_path_template_str; //!< you can customize the preferred file path formatting template.

	FilepathCache last_generated_paths;  //!< internal cache: stores the last generated file path (for re-use and reference)
	char* last_generated_step_id_string; //!< internal cache: stores the last generated steps[] id string (for re-use and reference)

	l_int32 debugging;				 //!< 1 if debugging mode is activated, resulting in several leptonica APIs producing debug/info messages and/or writing diagnostic plots and images to the basepath filesystem.

	unsigned int must_regenerate : 1;		//!< set when l_filename_prefix changes mandate a freshly (re)generated target file prefix for subsequent requests.
	unsigned int must_bump_step_id : 1;		//!< set when %step_id should be incremented before next use.
	unsigned int step_id_is_forever_increasing : L_MAX_STEPS_DEPTH;
	unsigned int step_width : 3;     //!< (width + 1): specifies the printed width of each step number at each steps level.

	unsigned int regressiontest_mode : 1;  //!< 1 if in regression test mode; 0 otherwise; when active, this means the generated paths in /tmp/ and elsewhere WILL NOT be randomized.
	unsigned int display : 1;        //!< 1 if in display mode; 0 otherwise                

	unsigned int is_init : 1;        //!< 1 if diagspec structure has been initialized; 0 otherwise                
};


/* global: */

/*!
 * image diagnostics helper spec associated with pix & plots; used to help display/diagnose behaviour in the more complex algorithms
 */
struct l_diag_predef_parts diag_spec = { NULL };


void
leptCreateDiagnoticsSpecInstance(void)
{
	if (!diag_spec.is_init) {
		diag_spec.step_width = 1;

		diag_spec.is_init = 1;
	}
	return;
}


void
leptDestroyDiagnoticsSpecInstance(void)
{
	if (!diag_spec.is_init) {
		return;
	}

	stringDestroy(&diag_spec.basepath);
	stringDestroy(&diag_spec.filename_prefix);
	stringDestroy(&diag_spec.filename_basename);
	stringDestroy(&diag_spec.process_name);
	stringDestroy(&diag_spec.default_path_template_str);

	for (unsigned int i = 0; i < L_PATH_CACHE_SIZE; i++) {
		stringDestroy(&diag_spec.last_generated_paths.path[i]);
	}

	stringDestroy(&diag_spec.last_generated_step_id_string);

	memset(&diag_spec, 0, sizeof(diag_spec));
	diag_spec.is_init = 0;
}


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
leptDebugSetFileBasepath(const char* directory)
{
	if (!directory) {
		directory = "";
	}

	leptCreateDiagnoticsSpecInstance();

	if (diag_spec.basepath)
		stringDestroy(&diag_spec.basepath);

	if (!directory[0]) {
		diag_spec.basepath = NULL;
	}
	else {
		l_int32 root_len = getPathRootLength(directory);
		// when the given directory path is an absolute path, we use it as-is: the user clearly wishes
		// to overide the usual /tmp/lept/... destination tree.
		if (root_len > 0) {
			diag_spec.basepath = stringNew(directory);
		}
		else {
			// TODO:
			// do we allow '../' elements in a relative directory spec?
			// 
			// Current affairs assume this path is set by (safe) application code,
			// rather than (unsafe) arbitrary end user input...
			diag_spec.basepath = pathJoin(leptDebugGetFileBasePath(), directory);
		}
	}

	diag_spec.basepath_minlength = diag_spec.basepath ? strlen(diag_spec.basepath) : 0;

	diag_spec.must_regenerate = 1;
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
leptDebugAppendFileBasepath(const char* directory)
{
	if (!directory) {
		directory = "";
	}

	leptCreateDiagnoticsSpecInstance();

	if (directory[0]) {
		if (diag_spec.basepath) {
			const char* base = diag_spec.basepath;
			// TODO:
			// do we allow '../' elements in a relative directory spec?
			// 
			// Current affairs assume this path is set by (safe) application code,
			// rather than (unsafe) arbitrary end user input...
			diag_spec.basepath = pathJoin(base, directory);
			stringDestroy(&base);
		}
		else {
			// TODO:
			// do we allow '../' elements in a relative directory spec?
			// 
			// Current affairs assume this path is set by (safe) application code,
			// rather than (unsafe) arbitrary end user input...
			diag_spec.basepath = pathJoin(leptDebugGetFileBasePath(), directory);
		}
	}
	else if (!diag_spec.basepath) {
		diag_spec.basepath = stringNew(leptDebugGetFileBasePath());
	}

	// ALWAYS recalculate the minlength as AppendFileBasepath() MAY immediately follow a FilePathPart() API call,
	// thus including that previously-non-mandatory part in the basepath now, which implies the basebath HAS
	// changed anyhow, even when %directory == "".
	diag_spec.basepath_minlength = strlen(diag_spec.basepath);

	diag_spec.must_regenerate = 1;
}


void
leptDebugAppendFilePathPart(const char* directory)
{
	if (!directory) {
		directory = "";
	}

	leptCreateDiagnoticsSpecInstance();

	if (!diag_spec.basepath) {
		diag_spec.basepath = stringNew(leptDebugGetFileBasePath());
		diag_spec.basepath_minlength = strlen(diag_spec.basepath);
	}

	if (directory[0]) {
		const char* base = diag_spec.basepath;
		// TODO:
		// do we allow '../' elements in a relative directory spec?
		// 
		// Current affairs assume this path is set by (safe) application code,
		// rather than (unsafe) arbitrary end user input...
		diag_spec.basepath = pathJoin(base, directory);
		stringDestroy(&base);
	}

	diag_spec.must_regenerate = 1;
}

void
leptDebugReplaceEntireFilePathPart(const char* directory)
{
	if (!directory) {
		directory = "";
	}

	leptCreateDiagnoticsSpecInstance();

	if (!diag_spec.basepath) {
		diag_spec.basepath = stringNew(leptDebugGetFileBasePath());
		diag_spec.basepath_minlength = strlen(diag_spec.basepath);
	}

	const char* base = diag_spec.basepath;
	diag_spec.basepath = stringCopySegment(base, 0, diag_spec.basepath_minlength);
	stringDestroy(&base);

	if (directory[0]) {
		leptDebugAppendFilePathPart(directory);
	}
	diag_spec.must_regenerate = 1;
}


void
leptDebugReplaceOneFilePathPart(const char* directory)
{
	if (!directory) {
		directory = "";
	}

	leptCreateDiagnoticsSpecInstance();

	if (!diag_spec.basepath) {
		diag_spec.basepath = stringNew(leptDebugGetFileBasePath());
		diag_spec.basepath_minlength = strlen(diag_spec.basepath);
	}

	const char* base = diag_spec.basepath;
	char* dir;
	splitPathAtDirectory(base, &dir, NULL);

	size_t baselen = strlen(dir);
	if (baselen >= diag_spec.basepath_minlength) {
		if (directory[0]) {
			diag_spec.basepath = pathJoin(dir, directory);
			LEPT_FREE(dir); // stringDestroy(&dir);
		}
		else {
			diag_spec.basepath = dir;
		}
		stringDestroy(&base);
	}
	else {
		L_WARNING("Attempting to replace forbidden base path part! (base: \"%s\", replace: \"%s\")\n", __func__, base, directory);
		LEPT_FREE(dir); // stringDestroy(&dir);
	}

	diag_spec.must_regenerate = 1;
}


void
leptDebugEraseFilePathPart(void)
{
	leptCreateDiagnoticsSpecInstance();

	if (!diag_spec.basepath) {
		diag_spec.basepath = stringNew(leptDebugGetFileBasePath());
		diag_spec.basepath_minlength = strlen(diag_spec.basepath);
	}
	else {
		const char* base = diag_spec.basepath;
		diag_spec.basepath = stringCopySegment(base, 0, diag_spec.basepath_minlength);
		stringDestroy(&base);
	}

	diag_spec.must_regenerate = 1;
}


void
leptDebugEraseOneFilePathPart(void)
{
	leptCreateDiagnoticsSpecInstance();

	if (!diag_spec.basepath) {
		diag_spec.basepath = stringNew(leptDebugGetFileBasePath());
		diag_spec.basepath_minlength = strlen(diag_spec.basepath);
	}

	const char* base = diag_spec.basepath;
	char* dir;
	splitPathAtDirectory(base, &dir, NULL);

	size_t baselen = strlen(dir);
	if (baselen >= diag_spec.basepath_minlength) {
		diag_spec.basepath = dir;
		stringDestroy(&base);
	}
	else {
		L_WARNING("Attempting to erase part of the restricted base path! (base: \"%s\")\n", __func__, base);
		LEPT_FREE(dir); // stringDestroy(&dir);
	}

	diag_spec.must_regenerate = 1;
}


const char*
leptDebugGetFilePathPart(void)
{
	leptCreateDiagnoticsSpecInstance();

	const char* rv = diag_spec.basepath + diag_spec.basepath_minlength;
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
leptDebugGetFileBasePath(void)
{
	leptCreateDiagnoticsSpecInstance();

	if (!diag_spec.basepath) {
		return "/tmp/lept-dbg-\x1F/nodef";  // 0x1F: special low-ASCII signal to genPathname to ALWAYS generate a random-coded directory name there!
	}
	return diag_spec.basepath;
}


/*!
 * \brief   leptDebugSetFilenameForPrefix()
 *
 * \param[in]    source_filename           the path to the file; may be relative or absolute or just the file name itself.
 * \param[in]    strip_off_parts_code
 *
 * <pre>
 * Notes:
 *      (1) The given prefix will be added to every debug plot, image, etc. file produced by leptonica.
 *          This is useful when, for example, processing source images in bulk and you wish to quickly
 *          locate the relevant debug/diagnostics outputs for a given source image.
 *      (2) By passing NULL, the prefix is erased.
 *      (3) %strip_off_parts_code is the number of path elements to keep for the prefix, where negative
 *          counts indicate the filename extension should be stripped off. code 0 is treated the same as -1.
 * </pre>
 */
void
leptDebugSetFilenameForPrefix(const char* source_filename, l_int32 strip_off_parts_code)
{
	leptCreateDiagnoticsSpecInstance();

	if (diag_spec.filename_prefix)
		stringDestroy(&diag_spec.filename_prefix);
	if (!source_filename) {
		diag_spec.filename_prefix = NULL;
	}
	else {
		// when a full path has been specified, strip off any directories: we assume the filename is
		// pretty unique by itself. When it isn't, there's little loss, as we also have the batch run #
		// and the process step # to help make the target uniquely named.
		diag_spec.filename_prefix = getPathBasename(source_filename, strip_off_parts_code);
	}

	diag_spec.must_regenerate = 1;
}


/*!
 * \brief   leptDebugGetFilenamePrefix()
 *
 * \return  the previously set source filename prefix meant for use as part of the target filepath.
 *
 * <pre>
 * Notes:
 *      (1) This path element (string) has NOT been sanitized yet: it may contain 'dangerous characters'
 *          when used directly for any target file path construction.
 *      (2) Will return NULL when not yet set (or when RESET).
 *      (3) When you want to use the cleaned-up and preformatted filename prefix, use the
 *          leptDebugGenFilename() API instead.
 * </pre>
 */
const char*
leptDebugGetFilenameForPrefix(void)
{
	leptCreateDiagnoticsSpecInstance();

	return diag_spec.filename_prefix;
}


/*!
 * \brief   leptDebugSetFilenameBasename()
 *
 * \param[in]    filename_fmt           the path to the file; may be relative or absolute or just the file name itself.
 * \param[in]    ...
 *
 * <pre>
 * Notes:
 *      (1) The resulting, formatted filename will be added to every generated filename/path if the path format includes the @BASENAME@ macro.
 *          This is useful when, for example, processing multiple files, all part of the same run, e.g. in a regression test.
 *      (2) By passing NULL, the basename is erased.
 * </pre>
 */
void
leptDebugSetFilenameBasename(const char* filename_fmt, ...)
{
	leptCreateDiagnoticsSpecInstance();

	if (diag_spec.filename_basename)
		stringDestroy(&diag_spec.filename_basename);
	if (!filename_fmt) {
		diag_spec.filename_basename = NULL;
	}
	else {
		// when a full path has been specified, strip off any directories: we assume the filename is
		// pretty unique by itself. When it isn't, there's little loss, as we also have the batch run #
		// and the process step # to help make the target uniquely named.
		va_list va;
		va_start(va, filename_fmt);
		diag_spec.filename_basename = string_vasprintf(filename_fmt, va);
		va_end(va);
	}

	diag_spec.must_regenerate = 1;
}


/*!
 * \brief   leptDebugGetFilenameBasename()
 *
 * \return  the previously set source filename basename meant for use as part of the target filepath.
 *
 * <pre>
 * Notes:
 *      (1) This path element (string) has NOT been sanitized yet: it may contain 'dangerous characters'
 *          when used directly for any target file path construction.
 *      (2) Will return NULL when not yet set (or when RESET).
 *      (3) When you want to use the cleaned-up and preformatted filename prefix, use the
 *          leptDebugGenFilename() API instead.
 * </pre>
 */
const char*
leptDebugGetFilenameBasename(void)
{
	leptCreateDiagnoticsSpecInstance();

	return diag_spec.filename_basename;
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
leptDebugSetStepId(uint32_t numeric_id)
{
	leptCreateDiagnoticsSpecInstance();

	// note: when we're at the maximum depth, we cannot change the step id any further, but for auto-incrementing (which is enforced at that level)
	if (diag_spec.steps.actual_depth == L_MAX_STEPS_DEPTH - 1) {
		diag_spec.must_bump_step_id = 0;
		diag_spec.must_regenerate = 1;
		++diag_spec.steps.vals[L_MAX_STEPS_DEPTH - 1];
	}
	else if (numeric_id == 0) {
		diag_spec.must_bump_step_id = 1;
		diag_spec.must_regenerate = 1;
		++diag_spec.steps.vals[L_MAX_STEPS_DEPTH - 1];
	}
	else if (diag_spec.steps.vals[diag_spec.steps.actual_depth] != numeric_id) {
		if (steps_level_numeric_value_compares(diag_spec.steps.actual_depth, diag_spec.step_id_is_forever_increasing, diag_spec.steps.vals[diag_spec.steps.actual_depth], numeric_id)) {
			diag_spec.steps.vals[diag_spec.steps.actual_depth] = numeric_id;
			diag_spec.must_bump_step_id = 0;
			diag_spec.must_regenerate = 1;
			++diag_spec.steps.vals[L_MAX_STEPS_DEPTH - 1];
		}
		else {
			diag_spec.must_bump_step_id = 1;
			diag_spec.must_regenerate = 1;
			++diag_spec.steps.vals[L_MAX_STEPS_DEPTH - 1];
		}
	}
}


void
leptDebugIncrementStepId(void)
{
	leptCreateDiagnoticsSpecInstance();

	++diag_spec.steps.vals[diag_spec.steps.actual_depth];
	diag_spec.must_bump_step_id = 0;
	diag_spec.must_regenerate = 1;
	if (diag_spec.steps.actual_depth < L_MAX_STEPS_DEPTH - 1) {
		++diag_spec.steps.vals[L_MAX_STEPS_DEPTH - 1];
	}
}


/*!
 * \brief   leptDebugGetStepId()
 *
 * \return  the previously set step sequence id at the current depth. Will be 1(one) when the sequence has been reset.
 */
uint32_t
leptDebugGetStepId(void)
{
	leptCreateDiagnoticsSpecInstance();

	if (diag_spec.must_bump_step_id) {
		leptDebugIncrementStepId();
	}
	return diag_spec.steps.vals[diag_spec.steps.actual_depth];
}


// returned string is kept in the cache, so no need to destroy/free by caller.
static void
printStepIdAsString(char *buf, size_t bufsize, l_ok exclude_last_steps_level)
{
	char* p = buf;
	char* e = buf + bufsize;
	uint16_t maxdepth = diag_spec.steps.actual_depth + 1 - !!exclude_last_steps_level;
	if (maxdepth == 0)
		maxdepth = 1;
	int w = diag_spec.step_width + 1;
	for (uint16_t i = 0; i < maxdepth; i++) {
		unsigned int v = diag_spec.steps.vals[i];    // MSVC complains about feeding a l_atomic into a variadic function like printf() (because I was compiling in forced C++ mode, anyway). This hotfixes that.
		int n = snprintf(p, e - p, "%0*u.", w, v);
		p += n;
	}
	p--;  // discard the trailing '.'
	*p = 0;
}


// returned string is kept in the cache, so no need to destroy/free by caller.
const char*
leptDebugGetStepIdAsString(void)
{
	leptCreateDiagnoticsSpecInstance();

	if (diag_spec.must_bump_step_id) {
		leptDebugIncrementStepId();
	}
	const size_t bufsize = L_MAX_STEPS_DEPTH * 11 + 1; // max 10 digits (for uint32_t value) + '.' per level + '\0'
	char* buf = diag_spec.last_generated_step_id_string;
	if (!buf) {
		buf = (char*)LEPT_CALLOC(bufsize, sizeof(char));
		diag_spec.last_generated_step_id_string = buf;
	}
	printStepIdAsString(buf, bufsize, FALSE);

	return buf;
}


void
leptDebugMarkStepIdForIncrementing(void)
{
	leptCreateDiagnoticsSpecInstance();

	diag_spec.must_bump_step_id = 1;
}


void
leptDebugSetStepLevelAsForeverIncreasing(l_ok enable)
{
	leptCreateDiagnoticsSpecInstance();

	if (enable) {
		unsigned int mask = 1U << diag_spec.steps.actual_depth;
		diag_spec.step_id_is_forever_increasing |= mask;
	}
	else if (diag_spec.steps.actual_depth < L_MAX_STEPS_DEPTH - 1) {
		unsigned int mask = ~(1U << diag_spec.steps.actual_depth);
		diag_spec.step_id_is_forever_increasing &= mask;
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
leptDebugSetStepIdAtDepth(int relative_depth, uint32_t numeric_id)
{
	leptCreateDiagnoticsSpecInstance();

	// we can only address current or parent levels!
	if (relative_depth > 0)
		relative_depth = -relative_depth;

	int target_depth = diag_spec.steps.actual_depth + relative_depth;
	if (target_depth < 0)
		target_depth = 0;
	else if (target_depth >= L_MAX_STEPS_DEPTH)
		target_depth = L_MAX_STEPS_DEPTH - 1;

	if (target_depth == diag_spec.steps.actual_depth) {
		leptDebugSetStepId(numeric_id);
		return;
	}

	// here we only address any parent levels, hence we won't be touching level (L_MAX_STEPS_DEPTH - 1) which simplifies matters.
	if (numeric_id == 0) {
		++diag_spec.steps.vals[target_depth];
		steps_reset_sub_levels(&diag_spec.steps, target_depth + 1, diag_spec.steps.actual_depth, diag_spec.step_id_is_forever_increasing);
		diag_spec.must_bump_step_id = 1;
		diag_spec.must_regenerate = 1;
		++diag_spec.steps.vals[L_MAX_STEPS_DEPTH - 1];
	}
	else if (diag_spec.steps.vals[target_depth] != numeric_id) {
		if (steps_level_numeric_value_compares(target_depth, diag_spec.step_id_is_forever_increasing, diag_spec.steps.vals[target_depth], numeric_id)) {
			diag_spec.steps.vals[target_depth] = numeric_id;
			steps_reset_sub_levels(&diag_spec.steps, target_depth + 1, diag_spec.steps.actual_depth, diag_spec.step_id_is_forever_increasing);
			diag_spec.must_bump_step_id = 1;
			diag_spec.must_regenerate = 1;
			++diag_spec.steps.vals[L_MAX_STEPS_DEPTH - 1];
		}
		else {
			++diag_spec.steps.vals[target_depth];
			steps_reset_sub_levels(&diag_spec.steps, target_depth + 1, diag_spec.steps.actual_depth, diag_spec.step_id_is_forever_increasing);
			diag_spec.must_bump_step_id = 1;
			diag_spec.must_regenerate = 1;
			++diag_spec.steps.vals[L_MAX_STEPS_DEPTH - 1];
		}
	}
}


void
leptDebugIncrementStepIdAtDepth(int relative_depth)
{
	leptCreateDiagnoticsSpecInstance();

	// we can only address current or parent levels!
	if (relative_depth > 0)
		relative_depth = -relative_depth;

	int target_depth = diag_spec.steps.actual_depth + relative_depth;
	if (target_depth < 0)
		target_depth = 0;
	else if (target_depth >= L_MAX_STEPS_DEPTH)
		target_depth = L_MAX_STEPS_DEPTH - 1;

	if (target_depth == diag_spec.steps.actual_depth) {
		leptDebugIncrementStepId();
		return;
	}

	// here we only address any parent levels, hence we won't be touching level (L_MAX_STEPS_DEPTH - 1) which simplifies matters.
	++diag_spec.steps.vals[target_depth];
	steps_reset_sub_levels(&diag_spec.steps, target_depth + 1, diag_spec.steps.actual_depth, diag_spec.step_id_is_forever_increasing);
	diag_spec.must_bump_step_id = 1;
	diag_spec.must_regenerate = 1;
	++diag_spec.steps.vals[L_MAX_STEPS_DEPTH - 1];
}


uint32_t
leptDebugGetStepIdAtLevel(int relative_depth)
{
	leptCreateDiagnoticsSpecInstance();

	// we can only address current or parent levels!
	if (relative_depth > 0)
		relative_depth = -relative_depth;

	int target_depth = diag_spec.steps.actual_depth + relative_depth;
	if (target_depth < 0)
		target_depth = 0;
	else if (target_depth >= L_MAX_STEPS_DEPTH)
		target_depth = L_MAX_STEPS_DEPTH - 1;

	if (target_depth == diag_spec.steps.actual_depth) {
		return leptDebugGetStepId();
	}

	// diag_spec.must_bump_step_id only applies to the active depth level, not any parent level.
	return diag_spec.steps.vals[target_depth];
}


void
leptDebugSetStepLevelAtLevelAsForeverIncreasing(int relative_depth, l_ok enable)
{
	leptCreateDiagnoticsSpecInstance();

	// we can only address current or parent levels!
	if (relative_depth > 0)
		relative_depth = -relative_depth;

	int target_depth = diag_spec.steps.actual_depth + relative_depth;
	if (target_depth < 0)
		target_depth = 0;
	else if (target_depth >= L_MAX_STEPS_DEPTH)
		target_depth = L_MAX_STEPS_DEPTH - 1;

	if (target_depth == diag_spec.steps.actual_depth) {
		leptDebugSetStepLevelAsForeverIncreasing(enable);
		return;
	}

	if (enable) {
		unsigned int mask = 1U << target_depth;
		diag_spec.step_id_is_forever_increasing |= mask;
	}
	else if (target_depth < L_MAX_STEPS_DEPTH - 1) {
		unsigned int mask = ~(1U << target_depth);
		diag_spec.step_id_is_forever_increasing &= mask;
	}
}


NUMA*
leptDebugGetStepNuma(void)
{
	leptCreateDiagnoticsSpecInstance();

	NUMA* numa = numaCreate(L_MAX_STEPS_DEPTH);
	for (uint16_t i = 0; i < diag_spec.steps.actual_depth; i++) {
		numaAddNumber(numa, diag_spec.steps.vals[i]);
	}
	return numa;
}


uint16_t
leptDebugGetStepDepth(void)
{
	leptCreateDiagnoticsSpecInstance();

	return diag_spec.steps.actual_depth;
}


uint16_t leptDebugAddStepLevel(void)
{
	leptCreateDiagnoticsSpecInstance();

	if (diag_spec.steps.actual_depth < L_MAX_STEPS_DEPTH - 2) {
		++diag_spec.steps.actual_depth;
		diag_spec.steps.vals[diag_spec.steps.actual_depth] = 0;
		diag_spec.must_bump_step_id = 1;
		diag_spec.must_regenerate = 1;
	}
	else {
		diag_spec.steps.actual_depth = L_MAX_STEPS_DEPTH - 1;
		diag_spec.must_bump_step_id = 1;
		diag_spec.must_regenerate = 1;
	}
	return diag_spec.steps.actual_depth;
}


uint32_t
leptDebugPopStepLevel(void)
{
	leptCreateDiagnoticsSpecInstance();

	uint32_t rv = diag_spec.steps.vals[diag_spec.steps.actual_depth];
	if (diag_spec.steps.actual_depth > 0) {
		--diag_spec.steps.actual_depth;
		diag_spec.must_bump_step_id = 1;
		diag_spec.must_regenerate = 1;
	}
	return rv;
}


void
leptDebugSetStepDisplayWidth(unsigned int width_per_level)
{
	leptCreateDiagnoticsSpecInstance();

	if (width_per_level < 1)
		width_per_level = 1;
	else if (width_per_level > 8)
		width_per_level = 8;
	diag_spec.step_width = width_per_level - 1;  // value 1..8: fits in 3 bits. :-)
}


unsigned int
leptDebugGetStepDisplayWidth(void)
{
	leptCreateDiagnoticsSpecInstance();

	return diag_spec.step_width + 1;
}


void
leptDebugSetHashId(uint64_t hash_id)
{
	leptCreateDiagnoticsSpecInstance();

	if (diag_spec.active_hash_id != hash_id) {
		diag_spec.must_regenerate = 1;
		diag_spec.active_hash_id = hash_id;
	}
}


uint64_t
leptDebugGetHashId(void)
{
	leptCreateDiagnoticsSpecInstance();

	return diag_spec.active_hash_id;
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
leptDebugSetProcessName(const char* name)
{
	leptCreateDiagnoticsSpecInstance();

	if (diag_spec.process_name) {
		stringDestroy(&diag_spec.process_name);
	}
	if (name && *name) {
		diag_spec.process_name = stringNew(name);
	}

	diag_spec.must_regenerate = 1;
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
leptDebugGetProcessName(void)
{
	leptCreateDiagnoticsSpecInstance();

	return diag_spec.process_name;
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
leptDebugSetFilepathDefaultFormat(const char* path_template_str)
{
	leptCreateDiagnoticsSpecInstance();

	// TODO: parse format string and check for errors.

	if (diag_spec.default_path_template_str) {
		stringDestroy(&diag_spec.default_path_template_str);
	}
	if (path_template_str && *path_template_str) {
		diag_spec.default_path_template_str = stringNew(path_template_str);
	}
	else {
		diag_spec.default_path_template_str = stringNew("arbitrar/{5b}.{2s}.{3i}#{5H}.{30f:@#-_.~}.{20p:@#-_.~}-{20t:@#-_.~}");
	}

	diag_spec.must_regenerate = 1;

}


/*!
 * \brief   leptDebugGetFilenamePrefix()
 *
 * \return  the previously set filename prefix string or an empty string if no prefix has been set up.
 */
const char*
leptDebugGetFilepathDefaultFormat(void)
{
	leptCreateDiagnoticsSpecInstance();

	return diag_spec.default_path_template_str;
}


/*!
 * \brief   leptDebugGenFilename()
 *
 * \return  the previously set filename prefix string or an empty string if no prefix has been set up.
 */
const char*
leptDebugGenFilename(const char* filename_fmt_str, ...)
{
	leptCreateDiagnoticsSpecInstance();

	va_list va;
	va_start(va, filename_fmt_str);
	const char* fn = string_vasprintf(filename_fmt_str, va);
	va_end(va);
	const char* f = pathJoin(leptDebugGetFileBasePath(), fn);
	stringDestroy(&fn);
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
leptDebugGenFilepath(const char* path_fmt_str, ...)
{
	va_list va;
	va_start(va, path_fmt_str);
	const char *fn = string_vasprintf(path_fmt_str, va);
	va_end(va);
	const char* f = pathJoin(leptDebugGetFileBasePath(), fn);
	stringDestroy(&fn);
	// TODO: cache & cycle through about 16 filepath slots. AT LEAST 2 as boxa1_reg.c is assuming the last 2 generated paths remain valid for a while!
	return f;
}


const char*
leptDebugGenFilepathEx(const char* directory, const char* path_fmt_str, ...)
{
	va_list va;
	va_start(va, path_fmt_str);
	const char* fn = string_vasprintf(path_fmt_str, va);
	va_end(va);
	const char* f = pathJoin(leptDebugGetFileBasePath(), fn);
	stringDestroy(&fn);
	return f;
}


/*!
 * \brief   leptDebugGetFilenamePrefix()
 *
 * \return  the previously set filename prefix string or an empty string if no prefix has been set up.
 */
const char*
leptDebugGetLastGenFilepath(void)
{
	leptCreateDiagnoticsSpecInstance();

	int index = diag_spec.last_generated_paths.active_index + L_PATH_CACHE_SIZE - 1;
	index %= L_PATH_CACHE_SIZE;
	const char* s = diag_spec.last_generated_paths.path[index];
	if (s == NULL) {
		// no previous path has been generated, ever.
		return (const char*)ERROR_PTR("no generated filepaths have been generated before: cannot comply with this request to produce the previously generated path.", __func__, NULL);
	}
	return s;
}


l_ok
leptIsInDisplayMode(void)
{
	leptCreateDiagnoticsSpecInstance();

	return diag_spec.display;
}


void
leptSetInDisplayMode(l_ok activate)
{
	leptCreateDiagnoticsSpecInstance();

	diag_spec.display = !!activate;
}


l_ok
leptIsInDRegressionTestMode(void)
{
	leptCreateDiagnoticsSpecInstance();

	return diag_spec.regressiontest_mode;
}


void
leptSetInRegressionTestMode(l_ok activate)
{
	leptCreateDiagnoticsSpecInstance();

	diag_spec.regressiontest_mode = !!activate;
}


/*
* DebugMode APIs tolerate a NULL spec pointer, returning FALSE or doing nothing, IFF activate == FALSE.
*
* This means that any attempt to *activate* debug mode without a valid spec pointer will produce an error message,
* while any attempt to DE-activate debug mode without a valid spec pointer will be silently accepted.
*/

l_ok
leptIsDebugModeActive(void)
{
	return diag_spec.debugging > 0;
}


void
leptActivateDebugMode(l_int32 activate_count_add, l_int32 activate_count_subtract)
{
	diag_spec.debugging += activate_count_add;
	diag_spec.debugging -= activate_count_subtract;
}



// generates a (probably unique) semi-random ID string.
void
leptDebugMkRndToken6(char dest[6])
{
	static unsigned int prev_h = 0;

#if 0
	LARGE_INTEGER li = { 0 };
	QueryPerformanceCounter(&li);
	LONGLONG hh = li.QuadPart;
	// remix all bits into the lower 30 bits, which will be used to fill XXXXXX name part
	hh ^= hh >> 17;
	hh ^= hh >> (64 - 6 * 5);
	unsigned int h = (unsigned int)hh;
#else
	// https://stackoverflow.com/questions/13772567/how-to-get-the-cpu-cycle-count-in-x86-64-from-c
	uint64_t hh = __rdtsc();
	// remix all bits into the lower 30 bits, which will be used to fill XXXXXX name part
	hh ^= hh >> 17;
	hh ^= hh >> (64 - 6 * 5);
	unsigned int h = (unsigned int)hh;
#endif

	// remix with previously stored value: this ensures the numbers keep changing no matter how fast/often you query that perf counter.
	prev_h *= 0x9E3779B1U;    // prime
	prev_h ^= h;
	h = prev_h;

	static const char* const lu = "0123456789ABCDEFGHJKLMNPQRSTUVWZ";
	assert(strlen(lu) == 32);
	dest[0] = lu[h & 0x1F];
	h >>= 5;
	dest[1] = lu[h & 0x1F];
	h >>= 5;
	dest[2] = lu[h & 0x1F];
	h >>= 5;
	dest[3] = lu[h & 0x1F];
	h >>= 5;
	dest[4] = lu[h & 0x1F];
	h >>= 5;
	dest[5] = lu[h & 0x1F];
}
