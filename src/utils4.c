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

#if defined(__APPLE__) || defined(_WIN32)
/* Rewrite paths starting with /tmp for macOS, iOS and Windows. */
#define REWRITE_TMP
#endif

/*--------------------------------------------------------------------*
 *         Image Debugging & Diagnostics Helper functions             *
 *--------------------------------------------------------------------*/



struct l_diag_predef_parts {
	const char* basepath;            //!< the base path within where every generated file must land.
	size_t batch_unique_id;          // 0 if unused?
	size_t step_id;                  //!< will be RESET every time the %batch_unique_id is incremented/changed.
	size_t item_id;                  //!< will be RESET every time the %batch_unique_id or %step_id is incremented/changed (unless %item_id_is_forever_increasing is set).
	uint64_t active_hash_id;         //!< derived from the originally specified %active_filename path, or set explicitly (if user-land code has a better idea about what value this should be).
	const char* active_filename;     //!< the currently processed source file name. To be used (in reduced form) as a target filename prefix.
	const char* process_name;        //!< the part which identifies what we are doing right now (at a high abstraction level)
	const char* default_path_template_str; //!< you can customize the preferred file path formatting template.

	unsigned int must_regenerate : 1;		//!< set when l_filename_prefix changes mandate a freshly (re)generated target file prefix for subsequent requests.
	unsigned int must_bump_batch_id : 1;	//!< set when %batch_unique_id should be incremented before next use.
	unsigned int must_bump_step_id : 1;		//!< set when %step_id should be incremented before next use.
	unsigned int must_bump_item_id : 1;		//!< set when %item_id should be incremented before next use.
	unsigned int item_id_is_forever_increasing : 1;
};



LDIAG_CTX leptCreateDiagnoticsSpecInstance(void)
{
	LDIAG_CTX dst = (LDIAG_CTX)LEPT_CALLOC(1, sizeof(*dst));
	if (!dst)
	{
		return (LDIAG_CTX)ERROR_PTR("l_diag_filename_predef_parts not made", __func__, NULL);
	}
	return dst;
}


void leptDestroyDiagnoticsSpecInstance(LDIAG_CTX* spec_ptr)
{
	if (!spec_ptr) {
		L_WARNING("ptr address is null!\n", __func__);
		return;
	}

	LDIAG_CTX spec = *spec_ptr;
	if (spec == NULL)
		return;

	LEPT_FREE(spec);
	*spec_ptr = NULL;
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
leptDebugSetFileBasepath(LDIAG_CTX spec, const char* directory)
{
	if (spec->basepath)
		stringDestroy(&spec->basepath);
	if (!directory) {
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
			spec->basepath = pathJoin("/tmp/lept/debug", directory);
		}
	}

	spec->must_regenerate = 1;
}


/*!
 * \brief   leptDebugGetFileBasePath()
 *
 * \return  the previously set target filename base path; usually pointing somewhere inside the /tmp/lept/ directory tree.
 */
const char*
leptDebugGetFileBasePath(LDIAG_CTX spec)
{
	if (!spec->basepath) {
		return "/tmp/lept/debug";
	}
	return spec->basepath;
}


/*!
 * \brief   leptDebugSetFilenameForPrefix()
 *
 * \param[in]    source_filename
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


/*!
 * \brief   leptDebugSetBetchUniqueId()
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
leptDebugSetBetchUniqueId(LDIAG_CTX spec, size_t numeric_id)
{
	if (spec->batch_unique_id != numeric_id) {
		leptDebugSetStepId(0);

		spec->must_bump_batch_id = (numeric_id == 0);
		spec->must_regenerate = 1;

		spec->batch_unique_id = numeric_id;
	}
}


/*!
 * \brief   leptDebugIncrementBatchUniqueId()
 */
void
leptDebugIncrementBatchUniqueId(LDIAG_CTX spec)
{
	++spec->batch_unique_id;
	spec->must_bump_batch_id = 0;
	spec->must_regenerate = 1;

	leptDebugSetStepId(0);
}


/*!
 * \brief   leptDebugGetBatchUniqueId()
 *
 * \return  the previously set batch sequence id. Will be 1(one) when the sequence has been reset.
 */
size_t
leptDebugGetBatchUniqueId(LDIAG_CTX spec)
{
	if (spec->must_bump_batch_id) {
		leptDebugIncrementBatchUniqueId();
	}
	return spec->batch_unique_id;
}


/*!
 * \brief   leptDebugSetStepId()
 *
 * \param[in]    numeric_id     (batch) step sequence number
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
leptDebugSetStepId(LDIAG_CTX spec, size_t numeric_id)
{
	if (spec->step_id != numeric_id) {
		leptDebugSetItemId(0);

		spec->must_bump_step_id = (numeric_id == 0);
		spec->must_regenerate = 1;

		spec->step_id = numeric_id;
	}
}


/*!
 * \brief   leptDebugIncrementStepId()
 */
void
leptDebugIncrementStepId(LDIAG_CTX spec)
{
	++spec->step_id;
	spec->must_bump_step_id = 0;
	spec->must_regenerate = 1;
	leptDebugSetItemId(0);
}


/*!
 * \brief   leptDebugGetStepId()
 *
 * \return  the previously set (batch) step sequence id. Will be 1(one) when the step sequence has been reset.
 */
size_t
leptDebugGetStepId(LDIAG_CTX spec)
{
	if (spec->must_bump_step_id) {
		leptDebugIncrementStepId();
	}
	return spec->step_id;
}


/*!
 * \brief   leptDebugSetItemId()
 *
 * \param[in]    numeric_id     (batch) substep item sequence number
 *
 * <pre>
 * Notes:
 *      (1) The given substep item id will be added to every debug plot, image, etc. file produced by leptonica.
 *          This is useful when, for example, processing source images in bulk and you wish to quickly
 *          locate the relevant debug/diagnostics outputs for a given source image.
 *      (2) By passing 0, the substep item sequence is reset.
 *      (3) On every change (increment or otherwise) of the batch id, the substep item id will already have been RESET,
 *          unless leptDebugSetItemIdAsForeverIncreasing(TRUE) was set before that particular id was changed/incremented.
 *      (4) On every change (increment or otherwise) of the (batch) step id, the substep item id will already have been RESET,
 *          unless leptDebugSetItemIdAsForeverIncreasing(TRUE) was set before that particular id was changed/incremented.
 * </pre>
 */
void
leptDebugSetItemId(LDIAG_CTX spec, l_uint64 numeric_id)
{
	// when `item_id_is_forever_increasing` is set, every SET operation, even when
	// attempting to RESET the item_id, is an attempt at CHANGE. Therefore we
	// choose to signal the ONLY ALLOWED CHANGE should occur instead: an increment!
	//
	// Also note that we accept any numeric jump *upwards* in value: the only way is up!
	if (spec->item_id_is_forever_increasing && spec->step_id <= numeric_id) {
		spec->must_bump_item_id = 1;
		spec->must_regenerate = 1;
	}
	else if (spec->item_id != numeric_id) {
		spec->must_bump_item_id = (numeric_id == 0);
		spec->must_regenerate = 1;

		spec->item_id = numeric_id;
	}
}


/*!
 * \brief   leptDebugSetItemIdAsForeverIncreasing()
 *
 * \param[in]    enable     TRUE/FALSE
 */
void
leptDebugSetItemIdAsForeverIncreasing(LDIAG_CTX spec, l_ok enable)
{
	spec->item_id_is_forever_increasing = !!enable;
}


/*!
 * \brief   leptDebugIncrementItemId()
 *
 * \return  the previously set (sub-step) item sequence id. May be 0(zero) when the substep item sequence has been reset.
 */
void
leptDebugIncrementItemId(LDIAG_CTX spec)
{
	++spec->item_id;
	spec->must_bump_item_id = 0;
	spec->must_regenerate = 1;
}


/*!
 * \brief   leptDebugGetStepId()
 *
 * \return  the previously set (batch) step sequence id. May be 0(zero) when the step sequence has been reset.
 */
l_uint64
leptDebugGetItemId(LDIAG_CTX spec)
{
	if (spec->must_bump_item_id) {
		leptDebugIncrementItemId();
	}
	return spec->item_id;
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
 *            a '{8H}.' template instead to ensure your filenames at least will suit the target filesystem. But I digres...
 *          - Whatever the OPTIONAL text sanitizer filter expression '@' field may be, empty or otherwise, any sanitized string will:
 *            + have any sanitized character sequence replaced by a single replacement character,
 *            + have both head and tail ends trimmed to either a single '_' underscore or nothing-at-all.
 *
 *            The latter rule is to prevent ever generating filenames which start or end with '.' dots or one or more '~' or '$' characters, which
 *            have special meaning and/or effects in many operation systems. In other words: the sanitizer prevents you from ever being able
 *            to produce filenames such as UNIX-hiiden '.gitignore' or NTFS-endangering '$Index' or Unix-hairy '~home` when passing anything
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
 *            Hence '{5i}' is equivalent to C printf("%.5s"
 *          where these '{xyz}' template elements are recong
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

}


/*!
 * \brief   leptDebugGetFilenamePrefix()
 *
 * \return  the previously set filename prefix string or an empty string if no prefix has been set up.
 */
const char*
leptDebugGetLastGenFilepath(LDIAG_CTX spec)
{

}
















