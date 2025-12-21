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
	l_atomic forever_incrementing_val;
	// Note: the last depth level is permanently auto-incrementing and serves as a 'persisted' %item_id_is_forever_increasing counter.
} StepsArray;

struct l_diag_predef_parts {
	const char* basepath;            //!< the base path within where every generated file must land.
	const char* configured_tmpdir;   //!< the /tmp/-replacing 'base 'root' path within where every generated file must land.
	const char* expanded_tmpdir;     //!< internal use/cache: the (re)generated CVE-safe expansion/replacement for '/tmp/'
#if 0
	NUMA *steps;                     //!< a hierarchical step numbering system; the last number level will (auto)increment, or RESET every time when a more major step level incremented/changed (unless %item_id_is_forever_increasing is set).
#else
	struct StepsArray steps;         //!< a hierarchical step numbering system; the last number level will (auto)increment, or RESET every time when a more major step level incremented/changed (unless %item_id_is_forever_increasing is set).
#endif
	SARRAY* step_paths;	             //!< one path part per step level; appended to the basepath when constructing a target path.

	uint64_t active_hash_id;         //!< derived from the originally specified %active_filename path, or set explicitly (if user-land code has a better idea about what value this should be).

	SARRAY *last_generated_paths;	 //!< internal cache: stores the last generated file path (for re-use and reference)
	char* last_generated_step_id_string; //!< internal cache: stores the last generated steps[] id string (for re-use and reference)

	l_int32 debugging;				 //!< >0 if debugging mode is activated, resulting in several leptonica APIs producing debug/info messages and/or writing diagnostic plots and images to the basepath filesystem.
	l_int32 using_gplot;

	unsigned int step_id_is_forever_increasing : L_MAX_STEPS_DEPTH;

	unsigned int must_regenerate_id : 1;	//!< set when l_filename_prefix changes mandate a freshly (re)generated target file prefix for subsequent requests.

	unsigned int must_bump_step_id : 1;		//!< set when %step_id should be incremented before next use.
	unsigned int step_width : 3;     //!< (width + 1): specifies the printed width of each step number at each steps level.

	unsigned int regressiontest_mode : 1;  //!< 1 if in regression test mode; 0 otherwise; when active, this means the generated paths in /tmp/ and elsewhere WILL NOT be randomized.
	unsigned int display : 1;        //!< 1 if in display mode; 0 otherwise                

	unsigned int is_tmpdir_expanded : 1;  //!< 1 when the '/tmp/' path replacement has been (re)generated. This path is kept for the duration, unless this flag is RESET to 0.
	unsigned int is_init : 1;        //!< 1 if diagspec structure has been initialized; 0 otherwise                
};


/* global: */

/*!
 * image diagnostics helper spec associated with pix & plots; used to help display/diagnose behaviour in the more complex algorithms
 */
static struct l_diag_predef_parts diag_spec = { NULL };


void
leptCreateDiagnoticsSpecInstance(void)
{
	if (!diag_spec.is_init) {
		memset(&diag_spec, 0, sizeof(diag_spec));

		diag_spec.step_paths = sarrayCreateInitialized(L_MAX_STEPS_DEPTH, "");
		diag_spec.last_generated_paths = sarrayCreate(0);

		diag_spec.step_width = 1;

		diag_spec.must_regenerate_id = 1;
		diag_spec.must_bump_step_id = 1;

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
	stringDestroy(&diag_spec.expanded_tmpdir);

	sarrayDestroy(&diag_spec.step_paths);
	sarrayDestroy(&diag_spec.last_generated_paths);

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

	diag_spec.must_regenerate_id = 1;
}


void
leptDebugSetFilePathPartAtSLevel(int relative_depth, const char* directory)
{
	if (!directory) {
		directory = "";
	}

	leptCreateDiagnoticsSpecInstance();

	// negative depths are relative; positive are absolute.
	if (relative_depth < 0) {
		relative_depth += diag_spec.steps.actual_depth;
	}
	if (relative_depth < 0 || diag_spec.steps.actual_depth < relative_depth) {
		L_ERROR("specified depth outside currently active range.\n", __func__);
		return;
	}

	sarrayReplaceString(diag_spec.step_paths, relative_depth, (char *)directory, L_COPY);

	diag_spec.must_regenerate_id = 1;
}


void
leptDebugSetFilePathPartAtSLevelFromTail(int relative_depth, const char* filepath, l_int32 strip_off_parts_code)
{
	leptDebugSetFilePathPartAtSLevel(relative_depth, NULL);

	if (!filepath) {
		L_WARNING("source path is NULL/empty: your generated target paths will suffer.", __func__);
		return;
	}

	// help internal and /prog/ demo code: strip off any leading '/tmp/lept/' part before we proceed:
	if (strncmp("/tmp/lept/", filepath, 10) == 0) {
		filepath += 10;
	}
	// walk the path part list (stack) and skip/ignore any part matching the given path.
	// Ignore the primary part, even when it's not '/lept/' or '/lept/prog/':
	SARRAY* sa = sarrayCreate(0);
	if (!sa) {
		L_ERROR("sa could not be allocated.\n", __func__);
		return;
	}
	sarraySplitString(sa, filepath, "/\\");
	int cnt = sarrayGetCount(sa);
	int pos = 0;
	for (int i = 0; i <= diag_spec.steps.actual_depth && pos < cnt; i++) {
		const char* elem = sarrayGetString(diag_spec.step_paths, i, L_NOCOPY);
		const char* pfx = sarrayGetString(sa, pos, L_NOCOPY);
		if (strcmp(elem, pfx) == 0) {
			pos++;
		}
	}
	int remain = cnt - pos;
	if (abs(strip_off_parts_code) < remain) {
		remain = abs(strip_off_parts_code);
	}

	char* p1 = NULL;
	for (int i = remain; i > 0; i--) {
		int index = cnt - i;
		const char* pfx = sarrayGetString(sa, index, L_NOCOPY);

		// strip the last part according to specified rules:
		if (i == 1 && strip_off_parts_code <= 0) {
			char* tail = pathExtractTail(pfx, -1);
			char* p2 = pathJoin(p1, tail);
			stringDestroy(&tail);
			stringDestroy(&p1);
			p1 = p2;
		}
		else {
			char* p2 = pathJoin(p1, pfx);
			stringDestroy(&p1);
			p1 = p2;
		}
	}
	sarrayDestroy(&sa);
	leptDebugSetFilePathPartAtSLevel(relative_depth, p1);
}


void
leptDebugSetFilePathPart(const char* directory)
{
	leptDebugSetFilePathPartAtSLevel(diag_spec.steps.actual_depth, directory);
}


void
leptDebugSetFilePathPartFromTail(const char* filepath, l_int32 strip_off_parts_code)
{
	leptDebugSetFilePathPartAtSLevelFromTail(diag_spec.steps.actual_depth, filepath, strip_off_parts_code);
}


void
leptDebugSetFreshCleanFilePathPart(const char* directory_namebase)
{
	if (!directory_namebase || !*directory_namebase)
		directory_namebase = "l";

	// goal: an alphabetically sortable-in-time per-session-unique suffix.
	// (The overall-unique criterium should be upheld by using a unique, random, root path,
	// which we accomplish through mkTmpDirPath() elsewhere in this path' construction run-time path.
	char suffix[20];
	snprintf(suffix, sizeof(suffix), "-%04u", leptDebugGetForeverIncreasingIdValue());

	char* p = stringJoin(directory_namebase, suffix);
	leptDebugSetFilePathPartAtSLevel(diag_spec.steps.actual_depth, p);
	stringDestroy(&p);
}


const char*
leptDebugGetFilePathPartAtSLevel(int relative_depth)
{
	leptCreateDiagnoticsSpecInstance();

	// negative depths are relative; positive are absolute.
	if (relative_depth < 0) {
		relative_depth += diag_spec.steps.actual_depth;
	}
	if (relative_depth < 0 || diag_spec.steps.actual_depth < relative_depth) {
		return (const char *)ERROR_PTR("specified depth outside currently active range.", __func__, NULL);
	}

	return sarrayGetString(diag_spec.step_paths, relative_depth, L_NOCOPY);
}


const char*
leptDebugGetFilePathPart(void)
{
	leptCreateDiagnoticsSpecInstance();
	return sarrayGetString(diag_spec.step_paths, diag_spec.steps.actual_depth, L_NOCOPY);
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
		static /* not-const */ char path[] = "/tmp/lept-\x01XXXXX-nodef";

		char* p;
		p = strchr(path, '\x01');
		if (p) {
			leptDebugMkRndToken6(p);
		}

		return path;
	}
	return diag_spec.basepath;
}


static inline l_ok
steps_is_level_forever_increasing(unsigned int depth, unsigned int forever_increasing_mask)
{
	unsigned int mask = 1U << depth;
	return (forever_increasing_mask & mask) != 0;
}


// return TRUE when new numeric value may be assigned to this step's level/depth.
static inline l_ok
steps_level_numeric_value_compares(unsigned int depth, unsigned int forever_increasing_mask, unsigned int level_numeric_id, unsigned int new_numeric_id)
{
	if (steps_is_level_forever_increasing(depth, forever_increasing_mask))
		// we cannot ever DECREMENT any step level: that would break our premise of delivering a unique hierarchical number set.
		return (level_numeric_id < new_numeric_id);
	else
		return (level_numeric_id != new_numeric_id);
}


/*!
 * \brief   leptDebugSetStepIdAtSLevel()
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
leptDebugSetStepIdAtSLevel(int relative_depth, unsigned int numeric_id)
{
	leptCreateDiagnoticsSpecInstance();

	// negative depths are relative; positive are absolute.
	if (relative_depth < 0) {
		relative_depth += diag_spec.steps.actual_depth;
	}

	if (relative_depth < 0 || diag_spec.steps.actual_depth < relative_depth) {
		L_ERROR("specified depth outside currently active range.\n", __func__);
		return;
	}

	if (numeric_id == 0) {
		if (relative_depth == diag_spec.steps.actual_depth) {
			diag_spec.must_bump_step_id = 1;
		}
		else {
			assert(relative_depth < diag_spec.steps.actual_depth);
			++diag_spec.steps.vals[relative_depth];

			// bumping a parent level RESETS all relative children, unless they're set to 'forever increase', in which case they're kept as-is.
			while (++relative_depth < diag_spec.steps.actual_depth) {
				if (!steps_is_level_forever_increasing(relative_depth, diag_spec.step_id_is_forever_increasing)) {
					diag_spec.steps.vals[relative_depth] = 1;
				}
			}

			assert(relative_depth == diag_spec.steps.actual_depth);
			if (steps_is_level_forever_increasing(relative_depth, diag_spec.step_id_is_forever_increasing)) {
				diag_spec.must_bump_step_id = 1;
			}
			else {
				diag_spec.steps.vals[relative_depth] = 1;
				diag_spec.must_bump_step_id = 0;
			}
		}
	}
	else {
		if (steps_level_numeric_value_compares(relative_depth, diag_spec.step_id_is_forever_increasing, diag_spec.steps.vals[relative_depth], numeric_id)) {
			diag_spec.steps.vals[relative_depth] = numeric_id;

			// bumping a parent level RESETS all relative children, unless they're set to 'forever increase', in which case they're kept as-is.
			while (++relative_depth < diag_spec.steps.actual_depth) {
				if (!steps_is_level_forever_increasing(relative_depth, diag_spec.step_id_is_forever_increasing)) {
					diag_spec.steps.vals[relative_depth] = 1;
				}
			}

			if (relative_depth == diag_spec.steps.actual_depth) {
				if (steps_is_level_forever_increasing(relative_depth, diag_spec.step_id_is_forever_increasing)) {
					diag_spec.must_bump_step_id = 1;
				}
				else {
					diag_spec.steps.vals[relative_depth] = 1;
					diag_spec.must_bump_step_id = 0;
				}
			}
			else {
				diag_spec.must_bump_step_id = 0;
			}
		}
	}
	diag_spec.must_regenerate_id = 1;
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
leptDebugSetStepId(unsigned int numeric_id)
{
	leptCreateDiagnoticsSpecInstance();

	if (numeric_id == 0) {
		diag_spec.must_bump_step_id = 1;
	}
	else {
		if (steps_level_numeric_value_compares(diag_spec.steps.actual_depth, diag_spec.step_id_is_forever_increasing, diag_spec.steps.vals[diag_spec.steps.actual_depth], numeric_id)) {
			diag_spec.steps.vals[diag_spec.steps.actual_depth] = numeric_id;
			diag_spec.must_bump_step_id = 0;
		}
	}
	diag_spec.must_regenerate_id = 1;
}


void
leptDebugIncrementStepIdAtSLevel(int relative_depth)
{
	leptCreateDiagnoticsSpecInstance();

	// negative depths are relative; positive are absolute.
	if (relative_depth < 0) {
		relative_depth += diag_spec.steps.actual_depth;
	}

	if (relative_depth < 0 || diag_spec.steps.actual_depth < relative_depth) {
		L_ERROR("specified depth outside currently active range.\n", __func__);
		return;
	}

	++diag_spec.steps.vals[relative_depth];
	diag_spec.must_bump_step_id = 0;

	diag_spec.must_regenerate_id = 1;
}


void
leptDebugIncrementStepId(void)
{
	leptCreateDiagnoticsSpecInstance();

	++diag_spec.steps.vals[diag_spec.steps.actual_depth];
	diag_spec.must_bump_step_id = 0;

	diag_spec.must_regenerate_id = 1;
}


static void
updateStepId(void)
{
	if (diag_spec.must_bump_step_id) {
		leptDebugIncrementStepId();
		assert(diag_spec.must_bump_step_id == 0);
	}

	if (diag_spec.must_regenerate_id) {
		++diag_spec.steps.forever_incrementing_val;
		diag_spec.must_regenerate_id = 0;

		if (steps_is_level_forever_increasing(diag_spec.steps.actual_depth, diag_spec.step_id_is_forever_increasing)) {
			assert(diag_spec.steps.forever_incrementing_val >= diag_spec.steps.vals[diag_spec.steps.actual_depth]);
			diag_spec.steps.vals[diag_spec.steps.actual_depth] = diag_spec.steps.forever_incrementing_val;
		}

		stringDestroy(&diag_spec.last_generated_step_id_string);
	}
}


unsigned int
leptDebugGetForeverIncreasingIdValue(void)
{
	++diag_spec.steps.forever_incrementing_val;
	return diag_spec.steps.forever_incrementing_val;
}


/*!
 * \brief   leptDebugGetStepIdAtSLevel()
 *
 * \return  the previously set step sequence id at the specified depth. Will be 1(one) when the sequence has been reset.
 */
unsigned int
leptDebugGetStepIdAtSLevel(int relative_depth)
{
	leptCreateDiagnoticsSpecInstance();

	// negative depths are relative; positive are absolute.
	if (relative_depth < 0) {
		relative_depth += diag_spec.steps.actual_depth;
	}

	if (relative_depth < 0 || diag_spec.steps.actual_depth < relative_depth) {
		return ERROR_INT("specified depth outside currently active range.", __func__, 0);
	}

	updateStepId();

	return diag_spec.steps.vals[relative_depth];
}


/*!
 * \brief   leptDebugGetStepId()
 *
 * \return  the previously set step sequence id at the current depth. Will be 1(one) when the sequence has been reset.
 */
unsigned int
leptDebugGetStepId(void)
{
	leptCreateDiagnoticsSpecInstance();

	updateStepId();

	return diag_spec.steps.vals[diag_spec.steps.actual_depth];
}


static void
printStepIdAsString(char *buf, size_t bufsize)
{
	char* p = buf;
	char* e = buf + bufsize;
	int max_level = diag_spec.steps.actual_depth + 1;
	int w = diag_spec.step_width + 1;
	for (int i = 0; i < max_level; i++) {
		unsigned int v = diag_spec.steps.vals[i];    // MSVC complains about feeding a l_atomic into a variadic function like printf() (because I was compiling in forced C++ mode, anyway). This hotfixes that.
		assert(e - p > w);
		int n = snprintf(p, e - p, "%0*u.", w, v);
		assert(n > 0);
		p += n;
	}
	// drop the trailing '.':
	p[-1] = 0;
}


// returned string is kept in the cache, so no need to destroy/free by caller.
const char*
leptDebugGetStepIdAsString(void)
{
	leptCreateDiagnoticsSpecInstance();

	updateStepId();

	if (!diag_spec.last_generated_step_id_string) {
		const size_t bufsize = L_MAX_STEPS_DEPTH * 3 * sizeof(diag_spec.steps.vals[0]) + 1; // max 2.5 digits per byte (for unsigned int type values) + '.' per level + '\0'
		char* buf = (char*)LEPT_MALLOC(bufsize);
		if (!buf) {
			return (const char*)ERROR_PTR("could not allocate string buffer", __func__, NULL);
		}
		printStepIdAsString(buf, bufsize);
		diag_spec.last_generated_step_id_string = buf;
	}

	return diag_spec.last_generated_step_id_string;
}


void
leptDebugMarkStepIdForIncrementing(void)
{
	leptCreateDiagnoticsSpecInstance();

	diag_spec.must_bump_step_id = 1;

	diag_spec.must_regenerate_id = 1;
}


void
leptDebugSetStepLevelAtSLevelAsForeverIncreasing(int relative_depth, l_ok enable)
{
	leptCreateDiagnoticsSpecInstance();

	// negative depths are relative; positive are absolute.
	if (relative_depth < 0) {
		relative_depth += diag_spec.steps.actual_depth;
	}

	if (relative_depth < 0 || diag_spec.steps.actual_depth < relative_depth) {
		L_ERROR("specified depth outside currently active range.\n", __func__);
	}

	if (enable) {
		unsigned int mask = 1U << relative_depth;
		diag_spec.step_id_is_forever_increasing |= mask;
	}
	else {
		unsigned int mask = 1U << relative_depth;
		diag_spec.step_id_is_forever_increasing &= ~mask;
	}
}


void
leptDebugSetStepLevelAsForeverIncreasing(l_ok enable)
{
	leptCreateDiagnoticsSpecInstance();

	if (enable) {
		unsigned int mask = 1U << diag_spec.steps.actual_depth;
		diag_spec.step_id_is_forever_increasing |= mask;
	}
	else {
		unsigned int mask = 1U << diag_spec.steps.actual_depth;
		diag_spec.step_id_is_forever_increasing &= ~mask;
	}
}


static inline void
steps_reset_sub_levels(StepsArray* steps, unsigned int depth, unsigned int max_depth, unsigned int forever_increasing_mask)
{
	for (unsigned int d = depth; d <= max_depth; d++) {
		unsigned int mask = 1U << d;
		if ((forever_increasing_mask & mask) == 0) {
			steps->vals[d] = 0;
		}
	}
}


NUMA*
leptDebugGetStepNuma(void)
{
	leptCreateDiagnoticsSpecInstance();

	NUMA* numa = numaCreate(L_MAX_STEPS_DEPTH);
	for (unsigned int i = 0; i < diag_spec.steps.actual_depth; i++) {
		numaAddNumber(numa, diag_spec.steps.vals[i]);
	}
	return numa;
}


unsigned int
leptDebugGetStepLevel(void)
{
	leptCreateDiagnoticsSpecInstance();

	return diag_spec.steps.actual_depth;
}


unsigned int
leptDebugAddStepLevel(void)
{
	leptCreateDiagnoticsSpecInstance();

	// before we push another level, we might need to bow to the pending 'bump' request:
	if (diag_spec.must_bump_step_id) {
		leptDebugIncrementStepId();
		assert(diag_spec.must_bump_step_id == 0);
	}
	// ... while any 'must_regenerate_id' must remain pending, hence NO calling `updateStepId()` allowed around these premises! ;-)

	if (diag_spec.steps.actual_depth < L_MAX_STEPS_DEPTH - 2) {
		++diag_spec.steps.actual_depth;
		diag_spec.steps.vals[diag_spec.steps.actual_depth] = 0;
	}
	else {
		return ERROR_INT("cannot push another step level: maximum stack depth reached.", __func__, 0);
	}

	diag_spec.must_bump_step_id = 1;

	diag_spec.must_regenerate_id = 1;

	return diag_spec.steps.actual_depth;
}


unsigned int
leptDebugPopStepLevel(void)
{
	leptCreateDiagnoticsSpecInstance();

	if (diag_spec.steps.actual_depth > 0) {
		unsigned int rv = diag_spec.steps.vals[diag_spec.steps.actual_depth];

		unsigned int depth = diag_spec.steps.actual_depth--;

		// clear forever-increasing bits for levels we do not have any more:
		unsigned int mask = 1U << depth;
		--mask;		/* 0x100.. -> 0xFF.. */
		diag_spec.step_id_is_forever_increasing &= mask;

		/*
		* This is where our 'delayed incrementing of the id' needs some extra work from us:
		*
		* When you push and pop a level, you want the immediate next push/add
		* to reside at the *next* (incremented) parent level no matter what.
		*
		* Hence, on POP, we DO NOT delay our step id increment, but execute it immediately!
		*
		* In an example of a step hierarchy + productions:
		*
		* - steps = 1.1.
		* - Add (a.k.a. push)
		* - steps = 1.1.0.
		*   -->     1.1.1. (after delayed bumping on first path generate action)
		* - inc --> 1.1.2. (after delayed bumping...)
		* - inc --> 1.1.3. (ditto; you're getting it...)
		* - Pop            (NO delayed bump pending: that was immediate inc execution)
		*   -->     1.2.   (POP returns '3', BTW)
		* - Add (a.k.a. push)    :: (user just popping and pushing levels immediately after one another.)
		* - steps = 1.2.0. (with delayed bump pending...)
		*   -->     1.2.1. (after delayed bumping on first path generate action)
		* - inc --> 1.2.2. (after delayed bumping...)
		* - inc --> 1.2.3. (ditto; you're getting it...)
		* - Pop            (NO delayed bump pending: that was immediate inc execution)
		*   -->     1.3.   (POP returns '3', BTW)
		* - GenFile
		*   -->     1.3.name
		* - inc --> 1.4. 
		* - GenFile
		*   -->     1.4.name
		* - Pop            (NO delayed bump pending: that was immediate inc execution)
		*   -->     2.     (POP returns '4', BTW)
		*/
		++diag_spec.steps.vals[diag_spec.steps.actual_depth];
		diag_spec.must_bump_step_id = 0;

		diag_spec.must_regenerate_id = 1;

		return rv;
	}
	else {
		return ERROR_INT("cannot pop the last (root) step level: stack depth has been depleted; @dev: check your Add-vs-Pop call pairs.", __func__, 0);
	}
}


void
leptDebugPopStepLevelTo(int relative_depth)
{
	leptCreateDiagnoticsSpecInstance();

	// negative depths are relative; positive are absolute.
	if (relative_depth < 0) {
		relative_depth += diag_spec.steps.actual_depth;
	}

	if (relative_depth < 0 || diag_spec.steps.actual_depth < relative_depth) {
		L_ERROR("specified depth outside currently active range.\n", __func__);
		return;
	}

	if (diag_spec.steps.actual_depth != relative_depth) {
		diag_spec.steps.actual_depth = relative_depth;

		unsigned int depth = relative_depth + 1;

		// clear forever-increasing bits for levels we do not have any more:
		unsigned int mask = 1U << depth;
		--mask;		/* 0x100.. -> 0xFF.. */
		diag_spec.step_id_is_forever_increasing &= mask;

		// see note in leptDebugPopStepLevel:
		++diag_spec.steps.vals[diag_spec.steps.actual_depth];
		diag_spec.must_bump_step_id = 0;

		diag_spec.must_regenerate_id = 1;
	}
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

	diag_spec.must_regenerate_id = 1;
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
		diag_spec.active_hash_id = hash_id;

		diag_spec.must_regenerate_id = 1;
	}
}


uint64_t
leptDebugGetHashId(void)
{
	leptCreateDiagnoticsSpecInstance();

	return diag_spec.active_hash_id;
}


/*!
 * \brief   leptDebugGenFilepath()
 *
 * \param[in]    template_printf_fmt_str
 *
 * <pre>
 * Notes:
 * 
 * - leptonica APIs will always follow-up with a call to genPathname() afterwards, before using the generated path.
 *   This implies that leptDebugGenFilepath() MAY safely generate an unsafe.relative/incomplete path string.
 * 
 * - the returned string is stored in a cache array and will remain owned by the leptDebug code: callers MUST NOT free/release the returned string.
 *
 *
 *
 * </pre>
 */
char *
leptDebugGenFilepath(const char* path_fmt_str, ...)
{
	leptCreateDiagnoticsSpecInstance();

	updateStepId();

	/* if (diag_spec.must_regenerate_path)   -- we don't know the filename part so we have to the work anyway! */
	{
		char* fn = NULL;
		size_t fn_len = 0;

		if (path_fmt_str && path_fmt_str[0]) {
			va_list va;
			va_start(va, path_fmt_str);
			fn = string_vasprintf(path_fmt_str, va);
			va_end(va);

			convertSepCharsInPath(fn, UNIX_PATH_SEPCHAR);
			if (getPathRootLength(fn) != 0) {
				L_WARNING("The intent of %s() is to generate full paths from RELATIVE paths; this is not: '%s'\n", __func__, fn);
			}

			fn_len = strlen(fn);
		}

		const char* bp = leptDebugGetFileBasePath();
		size_t bp_len = strlen(bp);

		// sorta like leptDebugGetStepIdAsString(), but with filepath elements thrown in:
		size_t bufsize = L_MAX_STEPS_DEPTH * 4 * sizeof(diag_spec.steps.vals[0]) + 5 + 5;  // rough upper limit estimate...
		//bufsize += bp_len;
		bufsize += fn_len;
		for (int i = 0; i < diag_spec.steps.actual_depth; i++) {
			const char* str = sarrayGetString(diag_spec.step_paths, i, L_NOCOPY);
			bufsize += strlen(str);
		}
		char* buf = (char*)LEPT_MALLOC(bufsize);
		if (!buf) {
			return (char*)ERROR_PTR("could not allocate string buffer", __func__, NULL);
		}

		{
			char* p = buf;
			char* e = buf + bufsize;
			int max_level = diag_spec.steps.actual_depth + 1;
			int w = diag_spec.step_width + 1;
			for (int i = 0; i < max_level; i++) {
				const char* str = sarrayGetString(diag_spec.step_paths, i, L_NOCOPY);
				unsigned int v = diag_spec.steps.vals[i];    // MSVC complains about feeding a l_atomic into a variadic function like printf() (because I was compiling in forced C++ mode, anyway). This hotfixes that.
				assert(e - p > w + 3 + strlen(str));
				if (str && *str) {
					int n = snprintf(p, e - p, "%s-%0*u/", str, w, v);
					assert(n > 0);
					p += n;
				}
				else {
					// don't just produce numbered directories,
					// instead append the depth number to the previous dir:
					--p;
					int n = snprintf(p, e - p, ".%0*u/", w, v);
					assert(n > 0);
					p += n;
				}
			}

			char* fn_part = p;

			// drop the trailing '/':
			{
				--p;

				// the last level is not used as a directory, but as a filename PREFIX:
				*p++ = '.';

				// inject unique number in the last path element, just after the file prefix:
				int l = snprintf(p, e - p, "%04u.", leptDebugGetForeverIncreasingIdValue());
				assert(l < e - p);
				assert(l > 0);
				assert(p[l] == 0);
				p += l;

				// drop that last '.' if there's no filename suffix specified:
				if (fn_len == 0) {
					--p;
					*p = 0;
				}
			}
			assert(e - p > 2 + fn_len);
			if (fn_len > 0) {
				strcpy(p, fn);
			}
			else {
				*p = 0;
			}

			assert(strlen(buf) + 1 < bufsize);

			// now we sanitize the mother: '..' anywhere becomes '__' and non-ASCII, non-UTF8 is gentrified to '_' as well.
			p = buf;
			while (*p) {
				unsigned char c = p[0];
				if (c == '.' && p[1] == '.') {
					p[0] = '_';
					p[1] = '_';
					p += 2;
					continue;
				}
				else if (c == '.' && p > buf && p[-1] == '/') {
					// dir/.dotfile --> dir/_dotfile  :: unhide UNIX-style 'hidden' files
					p[0] = '_';
					p += 1;
					continue;
				}
				else if (c == '.' && (p[1] == '/' || p[1] == '\\' || p[1] == 0)) {
					// dirs & files cannot end in a dot
					p[0] = '_';
					p += 1;
					continue;
				}
				else if (c <= ' ') {   // replace spaces and low-ASCII chars
					p[0] = '_';
				}
				else if (strchr("$~%^&*?|;:'\"<>`", c)) {
					p[0] = '_';
				}
				else if (c == '\\') {
					if (p < fn_part) {
						p[0] = '/';
					}
					else {
						p[0] = '_';
					}
				}
				else if (c == '/' && p >= fn_part) {
					p[0] = '_';
				}
				p++;
			}
		}

		char* np = pathSafeJoin(bp, buf);

		stringDestroy(&fn);
		stringDestroy(&buf);

		sarrayAddString(diag_spec.last_generated_paths, np, L_INSERT);

		return np;
	}
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

	int index = sarrayGetCount(diag_spec.last_generated_paths);
	if (index == 0) {
		// no previous path has been generated, ever.
		return (const char*)ERROR_PTR("no generated filepaths have been generated before: cannot comply with this request to produce the previously generated path.", __func__, NULL);
	}
	return sarrayGetString(diag_spec.last_generated_paths, index - 1, L_NOCOPY);
}


void
leptDebugClearLastGenFilepathCache(void)
{
	leptCreateDiagnoticsSpecInstance();

	sarrayClear(diag_spec.last_generated_paths);
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


l_ok
leptIsGplotModeActive(void)
{
	return diag_spec.using_gplot > 0;
}


void
leptActivateGplotMode(l_int32 activate_count_add, l_int32 activate_count_subtract)
{
	diag_spec.using_gplot += activate_count_add;
	diag_spec.using_gplot -= activate_count_subtract;
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


#ifdef _WIN32

static uint64_t
qword(FILETIME ts)
{
	uint64_t t = ts.dwHighDateTime;
	t <<= 32;
	t |= ts.dwLowDateTime;
	return t;
}

static uint64_t
latest3(FILETIME ftCreationTime, FILETIME ftLastAccessTime, FILETIME ftLastWriteTime)
{
	uint64_t ct = qword(ftCreationTime);
	uint64_t wt = qword(ftLastAccessTime);
	uint64_t at = qword(ftLastWriteTime);

	uint64_t t = ct;
	if (wt > t)
		t = wt;
	if (at > t)
		t = at;
	return t;
}

#endif


// WARNING: this function CANNOT use any of the other file/path APIs as those invoke genPathname() under the hood
// and that's precisely what we DO NOT want here: we're setting up for that one!!!1! ;-)
//
// Hence we replicate/unroll all important APIs in-place, but with the caveat that *those* DO NOT invoke genPathname()!
static void 
mkTmpDirPath(void)
{
	// generate a randomized /tmp/... subdir and CREATE it right away: kinda mkdtemp() within /tmp/ to ensure nobody can 'hijack' our temp output.

	for (int round = 0; round < 42 /* heuristic; see below */; round++) {
		char* cdir = NULL;
		const char* tmpDir = getenv("TMPDIR");
		if (tmpDir != NULL) {
			/* must be an absolute path and it must exist! */
			if (getPathRootLength(tmpDir) > 0) {
				l_int32 exists = 0;
#if 0
				lept_dir_exists(tmpDir, &exists);
#else
#ifndef _WIN32
				{
					struct stat s;
					l_int32 err = stat(tmpDir, &s);
					if (err != -1 && S_ISDIR(s.st_mode))
						exists = 1;
				}
#else  /* _WIN32 */
				{
					l_uint32  attributes;
					attributes = GetFileAttributesA(tmpDir);
					if (attributes != INVALID_FILE_ATTRIBUTES &&
						(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
						exists = 1;
					}
				}
#endif  /* _WIN32 */
#endif
				if (exists) {
					cdir = stringNew(tmpDir);
				}
			}
		}
		if (!cdir) {
#if defined(__APPLE__)
			size = L_MAX(256, MAX_PATH);
			if ((cdir = (char*)LEPT_CALLOC(size, sizeof(char))) == NULL) {
				return (char*)ERROR_PTR("cdir not made", __func__, NULL);
			}
			size_t n = confstr(_CS_DARWIN_USER_TEMP_DIR, cdir, size);
			if (n == 0 || n > size) {
				/* Fall back to using /tmp */
				stringCopy(cdir, "/tmp", size);
			}
#elif defined(_WIN32)
			l_int32 tmpdirlen;
			char tmpdir[MAX_PATH];
			GetTempPathA(sizeof(tmpdir), tmpdir);  /* get the Windows temp dir */
			tmpdirlen = strlen(tmpdir);
			cdir = stringNew(tmpdir);
#endif  /* _WIN32 */
		}

		if (!cdir) {
			cdir = stringNew("/tmp");
		}

		// NOTE:
		//
		// DO NOT use mkdtemp() as we want an 'arbitrary' subdirectory that is alphabetically sortable over time,
		// i.e. it's first dirname part should 'increase' with every regeneration, so bulk & batch runs
		// show up in increasingly 'late' /tmp/ subdir trees.
		//
		// To accomplish this **beyond the current session** we first do scan the /tmp/ directory to see which
		// ones exist already (and then produce a name that's 'later' than all previous ones.
		// 
		// To prevent adversarial 'name starvation' (by someone creating a, say, 'ZZZZZY`-named template-matching dir)
		// we only look for the 'latest' that's currently present and jump/wrap-around from that 'number', so
		// there's always ample namespace for us to generate additional dirnames: this scan is only done *once*
		// per session.
		// 
		// The random part of the directory name will take care of attacks and parallel running leptonica
		// libraries-in-debug-mode, even when the basename happens to be the same.
		// 

		static char counter_prefix[5] = { 0 };

		if (counter_prefix[0] == 0) {
			// copy the 'currently existing latest counter' marker into our register.
			//
			// before we do that, though, we fill the register with an IV, because
			// current_match MAY have been obtained from an adversarial directory entry:
			// (len != 4)
			strcpy(counter_prefix, "ZZZZ");

#if 0
			{
				SARRAY* sa = getFilenamesInDirectoryEx(cdir, TRUE, FALSE);
				if (sa != NULL) {
					const char* current_match = "";
					int count = sarrayGetCount(sa);
					for (int i = 0; i < count; i++) {
						const char* str = sarrayGetString(sa, i, L_NOCOPY);
						if (strncasecmp(str, "lept-", 5) == 0) {
							// a match: decide if this one is 'later' than what we have
							if (strcasecmp(current_match, str + 5) > 0) {
								current_match = str + 5;
							}
						}
					}

					// copy the 'currently existing latest counter' marker into our register.
					//
					// filter out any adversarial input: [A-Z] is the accepted set.
					for (int i = 0; i < 4 && *current_match; current_match++) {
						unsigned char c = *current_match;
						if (c >= 'A' && c <= 'Z') {
							counter_prefix[i++] = c;
						}
					}

					sarrayDestroy(&sa);
				}
			}
#else
#ifndef _WIN32

			{
				char* realdir;
				char* stat_path;
				size_t          size;
				DIR* pdir;
				struct dirent* pdirentry;
				int             dfd, stat_ret;
				struct stat     st;

				/* Who would have thought it was this fiddly to open a directory
				   and get the files inside?  fstatat() works with relative
				   directory paths, and stat() requires using the absolute path.
				   realpath() works as follows for files and directories:
					* If the file or directory exists, realpath returns its path;
					  else it returns NULL.
					* For realpath() we use the POSIX 2008 implementation, where
					  the second arg is NULL and the path is malloc'd and returned
					  if the file or directory exists.  All versions of glibc
					  support this.  */
				realdir = realpath(cdir, NULL);
				if (realdir == NULL) {
					L_ERROR("realdir not made\n", __func__);
				}
				else if ((pdir = opendir(realdir)) != NULL) {
					const char* current_match = stringNew("");

					while ((pdirentry = readdir(pdir))) {
						if (strcmp(pdirentry->d_name, "..") == 0 || strcmp(pdirentry->d_name, ".") == 0)
							continue;
#if HAVE_DIRFD && HAVE_FSTATAT
						/* Platform issues: although Linux has these POSIX functions,
						 * AIX doesn't have fstatat() and Solaris doesn't have dirfd(). */
						dfd = dirfd(pdir);
						stat_ret = fstatat(dfd, pdirentry->d_name, &st, 0);
#else
						size = strlen(realdir) + strlen(pdirentry->d_name) + 2;
						stat_path = (char*)LEPT_CALLOC(size, 1);
						snprintf(stat_path, size, "%s/%s", realdir, pdirentry->d_name);
						stat_ret = stat(stat_path, &st);
						LEPT_FREE(stat_path);
#endif
						if (stat_ret == 0 && (!!wantSubdirs ^ !!S_ISDIR(st.st_mode)))
							continue;

						const char* str = pdirentry->d_name;
						if (strncasecmp(str, "lept-", 5) == 0) {
							// a match: decide if this one is 'later' than what we have
							if (strcasecmp(current_match, str + 5) > 0) {
								stringDestroy(&current_match);
								current_match = stringNew(str + 5);
							}
						}
					}
					closedir(pdir);
					LEPT_FREE(realdir);

					// copy the 'currently existing latest counter' marker into our register.
					//
					// filter out any adversarial input: [A-Z] is the accepted set.
					{
						const char* cm = current_match;
						for (int i = 0; i < 4 && *cm; cm++) {
							unsigned char c = *cm;
							if (c >= 'A' && c <= 'Z') {
								counter_prefix[i++] = c;
							}
						}
					}
					stringDestroy(&current_match);
				}
			}

#else  /* _WIN32 */

			/* http://msdn2.microsoft.com/en-us/library/aa365200(VS.85).aspx */
			{
				char* pszDir;

				pszDir = stringJoin(cdir, "\\*");

				if (strlen(pszDir) + 1 > MAX_PATH) {
					LEPT_FREE(pszDir);
					L_ERROR("dirname is too long\n", __func__);
				}
				else {
					HANDLE            hFind;
					WIN32_FIND_DATAA  ffd;

					hFind = FindFirstFileA(pszDir, &ffd);
					if (INVALID_HANDLE_VALUE != hFind) {
						const char* current_match = stringNew("");

						// FILETIME contains the number of 100-nanosecond intervals since January 1, 1601 (UTC).
						const uint64_t one_hour = 1 * 3600.0 * 1e9 / 100;
						//const uint64_t one_month = 30 * 24 * one_hour;
						uint64_t current_time = 0;

						if (0 != (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && strcmp(ffd.cFileName, "..") != 0 && strcmp(ffd.cFileName, ".") != 0)  /* pick up subdir */
						{
							convertSepCharsInPath(ffd.cFileName, UNIX_PATH_SEPCHAR);
							const char* str = ffd.cFileName;
							if (strncasecmp(str, "lept-", 5) == 0) {
								// a match: decide if this one is 'later' than what we have
								uint64_t dt = latest3(ffd.ftCreationTime, ffd.ftLastWriteTime, ffd.ftLastAccessTime);
								if (dt > current_time) {
									stringDestroy(&current_match);
									current_match = stringNew(str + 5);
									current_time = dt;
								}
								// here we take the 'sorts as higher' directory entry when it occurred within the last hour.
								else if (dt + one_hour >= current_time) {
									if (strcasecmp(current_match, str + 5) < 0) {
										stringDestroy(&current_match);
										current_match = stringNew(str + 5);
										current_time = dt;
									}
								}
							}
						}

						while (FindNextFileA(hFind, &ffd) != 0) {
							if ((0 == (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || strcmp(ffd.cFileName, "..") == 0 || strcmp(ffd.cFileName, ".") == 0))  /* pick up subdir */
								continue;

							//convertSepCharsInPath(ffd.cFileName, UNIX_PATH_SEPCHAR);
							const char* str = ffd.cFileName;
							if (strncasecmp(str, "lept-", 5) == 0) {
								// a match: decide if this one is 'later' than what we have
								uint64_t dt = latest3(ffd.ftCreationTime, ffd.ftLastWriteTime, ffd.ftLastAccessTime);
								if (dt > current_time) {
									stringDestroy(&current_match);
									current_match = stringNew(str + 5);
									current_time = dt;
								}
								// here we take the 'sorts as higher' directory entry when it occurred within the last hour.
								else if (dt + one_hour >= current_time) {
									if (strcasecmp(current_match, str + 5) < 0) {
										stringDestroy(&current_match);
										current_match = stringNew(str + 5);
										current_time = dt;
									}
								}
							}
						}

						FindClose(hFind);
						LEPT_FREE(pszDir);

						// copy the 'currently existing latest counter' marker into our register.
						//
						// filter out any adversarial input: [A-Z] is the accepted set.
						{
							const char* cm = current_match;
							for (int i = 0; i < 4 && *cm; cm++) {
								unsigned char c = *cm;
								if (c >= 'A' && c <= 'Z') {
									counter_prefix[i++] = c;
								}
							}
						}
						stringDestroy(&current_match);
					}
				}
			}

#endif  /* _WIN32 */
#endif

			assert(strlen(counter_prefix) == 4);

			// now bump the counter by 1:
			{
				unsigned char c = ++counter_prefix[3];
				if (c > 'Z') {
					counter_prefix[3] = 'A';
					++counter_prefix[2];
				}
				c = counter_prefix[2];
				if (c > 'Z') {
					counter_prefix[2] = 'A';
					++counter_prefix[1];
				}
				c = counter_prefix[1];
				if (c > 'Z') {
					counter_prefix[1] = 'A';
					++counter_prefix[0];
				}
				c = counter_prefix[0];
				if (c > 'Z') {
					counter_prefix[0] = 'A';
				}
			}

			// once-per-session setup has completed.
			//
			// The rest is up to the 'random'-generating part below...
		}

		// now generate a candidate (or a few) to allocate.
		//
		// Heuristic: if this fails to deliver, we re-run the entire init cycle from scratch,
		// until we run out of breath, at which point it's entirely unsafe to run this
		// library and we hit APPLICATION EXIT/ABORT.

		// max 5 attempts when things go badly wrong.
		for (int i = 0; i < 5; i++) {
			char arbitrar[7];
			leptDebugMkRndToken6(arbitrar);
			arbitrar[6] = 0;

			char* path = stringConcatNew(cdir, "/lept-", counter_prefix, "-", arbitrar, NULL);
			// cleanup double slashes and other cruft that might have come in from the env.var. and/or MSWindows API:
			{
				char* p2 = pathSafeJoin(path, NULL);
				stringDestroy(&path);
				path = p2;
			}

			l_ok ret;
#ifndef _WIN32
			ret = (mkdir(path, 0770) == 0);
#else
			ret = (CreateDirectoryA(path, NULL) != 0);
#endif
			if (ret) {
				// the proof is in the pudding: does the directory exist now?
				ret = FALSE;
#ifndef _WIN32
				{
					struct stat s;
					l_int32 err = stat(path, &s);
					if (err != -1 && S_ISDIR(s.st_mode)) {
						ret = TRUE;
					}
				}
#else  /* _WIN32 */
				{
					l_uint32  attributes;
					attributes = GetFileAttributesA(path);
					if (attributes != INVALID_FILE_ATTRIBUTES &&
						(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
						ret = TRUE;
					}
				}
#endif  /* _WIN32 */
			}

			// success?
			if (ret) {
				stringDestroy(&diag_spec.expanded_tmpdir);
				diag_spec.expanded_tmpdir = stringNew(path);
				diag_spec.is_tmpdir_expanded = 1;

				stringDestroy(&cdir);
				return;
			}

			L_WARNING("Setting up the TMP directory basedir turns out to take a little more work... Retrying '%s' with another value.\n", __func__, path);

			stringDestroy(&path);
		}

		// whoops... more than 5 attempts at the current counter_prefix...
		// 
		// Better run the entire cycle again, but HARD FAIL when we fail
		// to accomplish any decent result within, say, 42 rounds of this!

		stringDestroy(&cdir);

		// Nuke our counter_prefix so that one gets rescanned as well!
		memset(counter_prefix, 0, sizeof(counter_prefix));

		L_ERROR("Setting up the TMP directory basedir turns out to be real hassle: multiple retries failed; now resetting everything and executing this setup once again...\n", __func__);
	}

	// when we get here, we've attempted to set up our environment plenty
	// of times and failed consistently.

	L_ERROR("Utterly failed to set up the TMP directory basedir. Aborting the application as it's unsafe to continue executing it...\n", __func__);
	exit(66);
}


const char*
leptDebugGenTmpDirPath(void)
{
	if (diag_spec.is_tmpdir_expanded && diag_spec.expanded_tmpdir)
		return diag_spec.expanded_tmpdir;

	// generate a randomized /tmp/... subdir and CREATE it right away: kinda mkdtemp() within /tmp/ to ensure nobody can 'hijack' our temp output.
	mkTmpDirPath();

	assert(diag_spec.expanded_tmpdir != NULL);
	return diag_spec.expanded_tmpdir;
}


void
leptDebugSetTmpDirBasePath(const char* basepath)
{
	stringDestroy(&diag_spec.expanded_tmpdir);
	stringDestroy(&diag_spec.configured_tmpdir);

	if (!basepath || !*basepath) {
		diag_spec.configured_tmpdir = NULL;
	}
	else {
		// we need to bootstrap the new base path as userland code MAY pass in a relative or otherwise insufficiently specified path:
		char* p1 = pathSafeJoin(basepath, NULL);
		char* p2 = leptDebugGenFilepath("%s", p1); // sanitizes, among other extra efforts...

		stringDestroy(&p1);
		// kill the tmpdir, which was regenerated as part of the above path manip calls:
		stringDestroy(&diag_spec.expanded_tmpdir);
		stringDestroy(&diag_spec.configured_tmpdir);

		diag_spec.configured_tmpdir = p2;

		(void)leptDebugGenTmpDirPath();
	}
}
