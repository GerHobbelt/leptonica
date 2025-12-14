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
#include "allheaders.h"

#if defined(__APPLE__) || defined(_WIN32)
/* Rewrite paths starting with /tmp for macOS, iOS and Windows. */
#define REWRITE_TMP
#endif

/*--------------------------------------------------------------------*
 *                     Special file name operations                   *
 *--------------------------------------------------------------------*/


static inline char mk_upper(char c)
{
	// A: 0x41
	// a: 0x61
	// This simple operation works because we know we'll be using this routine only when comparing
	// the US/ASCII alphabet.
	return c & ~0x20;
}

static inline char is_Win32_drive_letter(char c)
{
	c = mk_upper(c);
	return (c >= 'A' && c <= 'Z');
}

#if 0
// Accepts both Unix and Windows pathname separators, on any platform.
//
// Notes:
// If you expected \-escaped paths on UNIX or other platforms, you'ld better have de-escaped those paths before using in leptonica *anywhere*!
//
static inline int leptIsSeparator(char c)
{
	return (c == '/' || c == '\\');
}
#endif

l_ok
lept_is_special_UNIX_directory(const char* path, size_t pathlen)
{
	// https://en.wikipedia.org/wiki/Filesystem_Hierarchy_Standard
	static const char* specials[] = {
		"tmp",
		"dev",
		"mount",
		"var",
		"boot",
		"etc",
		"bin",
		"sbin",
		"home",
		"lib",
		"usr",
		"opt",
		"root",
		"proc",
		"sys",
		"mnt",
		"media",
		"include",
		"srv",
		NULL
	};

	for (int i = 0; specials[i]; i++) {
		const char* s = specials[i];
		size_t l = strlen(s);
		if (l == pathlen && strncmp(s, path, l) == 0)
			return TRUE;
	}
	return FALSE;
}


/*!
 * \brief   gobble_server_share_path_part()
 *
 * \param[in]    path
 * \return       a pointer pointing past the end of the UNC-path' /Server/Share/ path part, i.e. pointing at the start of the directory/file path on the share/drive itself.
 *
 * <pre>
 * Notes:
 *      (1) Accepts both Unix and Windows pathname separators
 *      (2) On Unix, the root is always "/" (length = 1), but on Windows the root can be a drive, e.g. "C:\" (length = 3) or an UNC path prefix, e.g. "//?/Server/Share/" or "//./Z:/",
 *          which is why we return the *root length* instead of a mere boolean for 'is_absolute_path'.
 *
 * See also https://learn.microsoft.com/en-us/dotnet/standard/io/file-path-formats.
 * </pre>
 */
static
const char*
gobble_server_share_path_part(const char* path)
{
	// gobble up server\share\ part of this path (while reckoning with DOS drive specs)
	const char* p = strpbrk(path, "\\/");
	if (p)
	{
		if (is_Win32_drive_letter(path[0]) && path[1] == ':' && leptIsSeparator(path[2]))
		{
			return path + 3;
		}
		if (lept_is_special_UNIX_directory(path, p - path)) {
			return path; // error: not a network share spec, but a //-double-slash botched UNIX system dir.
		}
		if (leptIsSeparator(p[1])) {
			// we do not accept double slashes here! '//server//share/' is an error
			return path; // error: not a network share spec
		}
		p = strpbrk(p + 1, "\\/");
	}
	if (p)
		p++;
	else
		p = path;
	return p;
}

/*!
 * \brief   getPathRootLength()
 *
 * \param[in]    path
 * \return       0 when the path is a relative path, otherwise the length of the root path identifier is returned (length > 1).
 *
 * <pre>
 * Notes:
 *      (1) Accepts both Unix and Windows pathname separators
 *      (2) On Unix, the root is always "/" (length = 1), but on Windows the root can be a drive, e.g. "C:\" (length = 3) or an UNC path prefix, e.g. "//?/Server/Share/" or "//./Z:/",
 *          which is why we return the *root length* instead of a mere boolean for 'is_absolute_path'.
 * 
 * See also https://learn.microsoft.com/en-us/dotnet/standard/io/file-path-formats.
 * </pre>
 */
l_int32
getPathRootLength(const char* path)
{
	if (!path)
		return 0;

	int is_rooted = leptIsSeparator(path[0]);
	if (is_rooted)
	{
		int is_unc_path = leptIsSeparator(path[1]);
		if (is_unc_path)
		{
			if (path[2] && strchr(".?", path[2]) && leptIsSeparator(path[3]))
			{
				if (strncasecmp(path + 4, "UNC", 3) == 0 && leptIsSeparator(path[4 + 3]))
				{
					// gobble up server\share\ part of this \\?\UNC\ path (while reckoning with DOS drive specs)
					const char* p = gobble_server_share_path_part(path + 8);
					return p - path;
				}
				else
				{
					// gobble up server\share\ part of this \\?\ device path (while reckoning with DOS drive specs)
					const char* p = gobble_server_share_path_part(path + 4);
					return p - path;
				}
			}
			else
			{
				// gobble up \\server\share\ part of this network path (while reckoning with DOS drive specs)
				const char* p = gobble_server_share_path_part(path + 2);
				if (p == path + 2) {
					// botched UNIX root, e.g. '//tmp/'
					return 1;
				}
				return p - path;
			}
		}
		else
		{
			// sounds like a regular Unix rooted path:
			return 1;
		}
	}
	else
	{
		// MAY be a DOS root path!
		if (is_Win32_drive_letter(path[0]) && path[1] == ':' && leptIsSeparator(path[2]))
		{
			return 3;
		}
	}
	// else: not a rooted path
	return 0;
}


/*!
 * \brief   getPathBasename()
 *
 * \param[in]    path
 * \param[in]    strip_off_parts_code
 * \return  the basename part of the given path, e.g. /a/b/ccc.x --> ccc.x (or ccc when strip_off_parts_code is negative)
 *
 * <pre>
 * Notes:
 *      (1) The returned filename must be freed by the caller, using lept_free.
 * </pre>
 */
char *
getPathBasename(const char* path, l_int32 strip_off_parts_code)
{
	if (!path)
		return (char *)ERROR_PTR("path not defined", __func__, NULL);

	char *cpathname = stringNew(path);
	convertSepCharsInPath(cpathname, UNIX_PATH_SEPCHAR);

	SARRAY* sa = sarrayCreate(0);
	if (sarraySplitString(sa, cpathname, "/"))
		return (char*)ERROR_PTR("could not split input path into elements", __func__, NULL);
	stringDestroy(&cpathname);

	int n = sarrayGetCount(sa);
	if (n > 0) {
		if (strip_off_parts_code <= 0) {
			// strip off filename extension. Special care is taken when running into compressed tar archives: .tar.gz, etc...
			char* fname = sarrayGetString(sa, n - 1, L_NOCOPY);
			char* ext = strrchr(fname, '.');
			if (ext)
				*ext = 0;
			ext = strrchr(fname, '.');
			if (ext && 0 == strcmp(ext + 1, "tar"))
				*ext = 0;
		}
		int prefix_cnt = abs(strip_off_parts_code);
		if (!prefix_cnt)
			prefix_cnt = 1;
		n = n - prefix_cnt;
		if (n < 0)
			n = 0;
		char* rv = sarrayToStringRange(sa, n, -1, 1 /* '\n' */);
		// now replace every '\n' with '/', except the very last one, which should be stripped off:
		{
			char* restrict s = rv;
			for ( ; *s; s++) {
				if (*s == '\n')
					*s = '/';
			}
			*--s = 0;
		}
		sarrayDestroy(&sa);
		return rv;
	}

	sarrayDestroy(&sa);
	return (char*)ERROR_PTR("input path could not be split into elements", __func__, NULL);
}


/*!
 * \brief   sanitizePathToASCII()
 *
 * \param[in]    str            the string/path to sanitize.
 * \param[in]    additional_acceptable_set     the set of characters accepted in the output, next to '_' and the alphanumerics.
 * \return  the sanitized path.
 *
 * <pre>
 * Notes:
 *      (1) The returned name must be freed by the caller, using lept_free.
 *      (2) When '#' is part of the %additional_acceptable_set, this also signals the sanitizer that the sanitized path MAY begin with a number.
 *          Otherwise the sanitizer will prefix the path with 'u' so that "123.txt" will be sanitized to "u123.txt".
 *      (3) When '/' is part of the %additional_acceptable_set, this also signals the sanitizer that the path elements
 *          in %str should be each be sanitized individually, thus keeping the path mostly intact.
 *      (4) When '.' is part of the %additional_acceptable_set, this signals the sanitizer that '.' dots in the %str are accepted,
 *          but we WILL NOT accept any '.' dots as the start of any %str path element, i.e. we will not allow dotted paths like '../x'
 *          or UNIX hidden files like '.bash_history' to pass without replacing these 'dots with special meaning'
 *          --> '__/' and '_bash_history'.
 * </pre>
 */
char*
sanitizePathToASCII(const char* str, const char* additional_acceptable_set)
{
	if (!str)
		return (char*)ERROR_PTR("str not defined", __func__, NULL);
	if (!*str)
		return (char*)ERROR_PTR("str is unacceptably empty", __func__, NULL);

	int slen = strlen(str);
	char* buffer = (char*)LEPT_MALLOC(slen + 2 /* 'u' (for the filename) + str + NUL */);
	if (!buffer)
		return (char*)ERROR_PTR("buffer not made", __func__, NULL);

	if (!additional_acceptable_set || !*additional_acceptable_set)
		additional_acceptable_set = "_";

	const char* basename = strpbrk(str, "\\/");
	int is_path = (basename != NULL);
	int can_lead_with_number = (strchr(additional_acceptable_set, '#') != NULL);
	int accept_dot_in_filename = (strchr(additional_acceptable_set, '.') != NULL);
	int accept_path_separators = (strchr(additional_acceptable_set, '/') != NULL);

	if (basename)
		basename++;
	else
		basename = str;

	char* d = buffer;
	int has_replaced = FALSE;
	// used when sanitizing otherwise-UNIX-'hidden' path parts:
	int is_start_of_path_element = TRUE;
	// used when sanitizing filenames which start with a number:
	int is_start_of_basename_element = (basename == str || !accept_path_separators);

	for (const char* p = str; *p; p++) {
		if (*p >= 'A' && *p <= 'Z')
			goto do_generic;
		if (*p >= 'a' && *p <= 'z')
			goto do_generic;
		if (*p >= '0' && *p <= '9')
		{
			if (is_start_of_basename_element)
			{
				if (!can_lead_with_number)
					*d++ = 'u';
				is_start_of_basename_element = FALSE;
			}
			goto do_generic;
		}
		if (*p == '_')
			goto do_generic;
		if (*p == '.' && accept_dot_in_filename)
		{
			if (is_start_of_path_element)
			{
				*d++ = '_';
				has_replaced = TRUE;
				is_start_of_path_element = FALSE;
				continue;
			}
			goto do_generic;
		}
		if ((*p == '/' || *p == '\\') && accept_path_separators)
		{
			has_replaced = FALSE;
			is_start_of_path_element = TRUE;
			is_start_of_basename_element = (basename == p + 1);

			if (accept_dot_in_filename && d > buffer && d[-1] == '.')
			{
				d[-1] = '_';
			}
			*d++ = '/';
			continue;
		}
		if (strchr(additional_acceptable_set, *p))
			goto do_generic;
		// sanitize:
		if (!has_replaced) {
			*d++ = '_';
			has_replaced = TRUE;
		}
		continue;

	do_generic:
		has_replaced = FALSE;
		*d++ = *p;
	}
	*d = 0;

	return buffer;
}


/*!
 * \brief   sanitizeStringToIdentifier()
 *
 * \param[in]    str            the string to mangle into an identifier or eqv.
 * \return  the sanitized identifier, generated from the input value: %str.
 *
 * <pre>
 * Notes:
 *      (1) The returned name must be freed by the caller, using lept_free.
 *      (2) The generated identifier will be usable as a regular C/C++ identifier and have no leading nor any trailing '_' underscores.
 *      (3) If the %str would have produced an identifier which starts with a numeric part, the identifier will be prefixed with 'u'
 *          to ensure the generated identifier complies with C/C++ identifier rules.
 * </pre>
 */
char *
sanitizeStringToIdentifier(const char* str)
{
	if (!str)
		return (char*)ERROR_PTR("str not defined", __func__, NULL);
	if (!*str)
		return (char*)ERROR_PTR("str is unacceptably empty", __func__, NULL);

	int slen = strlen(str);
	char* buffer = (char*)LEPT_MALLOC(slen + 1 /* 'u' + str + NUL */);
	if (!buffer)
		return (char*)ERROR_PTR("buffer not made", __func__, NULL);

	char* d = buffer;
	int prev_char = 0;

	for (const char* p = str; *p; p++) {
		if (*p >= 'A' && *p <= 'Z')
			goto do_generic;
		if (*p >= 'a' && *p <= 'z')
			goto do_generic;
		if (*p >= '0' && *p <= '9')
		{
			// are we still at the start of the destination identifier string?
			if (prev_char == 0)
			{
				prev_char = 'u';
				*d++ = prev_char;
			}
			goto do_generic;
		}
		if (*p == '_')
		{
			// are we still at the start of the destination identifier string?
			// have we just previously 'sanitized' another character?
			// if so, don't produce reams of underscores in the sanitized identifier output.
			if (prev_char == 0 || prev_char == '_')
				continue;
			goto do_generic;
		}
		// sanitize:
		if (prev_char != '_') {
			*d++ = '_';
			prev_char = '_';
		}
		continue;

	do_generic:
		prev_char = *p;
		*d++ = prev_char;
	}

	// remove any trailing '_':
	while (d > buffer && d[-1] == '_')
		d--;

	// when we had to sanitize *everything*, there's nothing left: instead, produce the identifier 'x':
	if (d == buffer) {
		*d++ = 'x';
	}

	*d = 0;

	return buffer;
}


/*!
 * \brief   getPathHash()
 *
 * \param[in]    str             the string to hash.
 * \return  a 64-bit hash for the given string, or 0(zero) on error.
 *
 * <pre>
 * Notes:
 *      (1) The hash is calculated a pretty fast algorithm and NOT guaranteed unique: expect hash collisions, 
 *          though these SHOULD be rare -- NOT in the cryptographical sense, however!
 *          This hash is suitable for indexing generic hash tables and comparable purposes.
 *      (2) The hash is guaranteed to be non-zero; a zero(0) value indicating an error occurred.
 * </pre>
 */
uint64_t
getPathHash(const char* str)
{
	if (!str)
		return ERROR_INT("str not defined", __func__, 0);
	if (!*str)
		return ERROR_INT("str is unacceptably empty", __func__, 0);

	int slen = strlen(str);

	l_uint64 key = 0;
	if (l_hashStringToUint64Fast(str, &key))
		return ERROR_INT("hash not made", __func__, 0);

	if (key == 0)
		key = 1;
	
#if 0
	// fold hash into a 20 bit number:
	key ^= key >> 20;
	key ^= key >> 33;
	key ^= key >> 44;
	key &= (1UL << 20) - 1;
#endif

	return key;
}


/*!
 * \brief   sanitizePathToIdentifier()
 *
 * \param[in]    str             the string to mangle into an identifier or eqv.
 * \param[in]    max_ident_size  the maximum length of the generated 'identifier'.
 * \param[in]    additional_acceptable_set   an optional set of characters accepted in the identifier output.
 * \param[in]    max_ident_size  the maximum length of the generated 'identifier'.
 * \return  the sanitized and shortened identifier, generated from the input values: str & numeric_id.
 *
 * <pre>
 * Notes:
 *      (1) The returned name must be freed by the caller, using lept_free, unless it's an alias of `dst`.
 *      (2) When '#' is part of the %additional_acceptable_set, this also signals the sanitizer that the sanitized path MAY begin with a number.
 *          Otherwise the sanitizer will prefix the path with 'u' so that "123.txt" will be sanitized to "u123.txt".
 *      (3) '/' and '\' UNIX and MSWindows path separators are NEVER accepted as part of the %additional_acceptable_set.
 *      (4) When '.' is part of the %additional_acceptable_set, this signals the sanitizer that '.' dots in the %str are accepted,
 *          but we WILL NOT accept any '.' dots as the start of any %str path element, i.e. we will not allow dotted paths like '../x'
 *          or UNIX hidden files like '.bash_history' to pass without replacing these 'dots with special meaning'
 *          --> '__/' and '_bash_history'.
 * </pre>
 */
char*
sanitizePathToIdentifier(char* dst, size_t dstsize, const char* str, const char* additional_acceptable_set)
{
	if (!str)
		return (char*)ERROR_PTR("str not defined", __func__, NULL);
	if (!*str)
		return (char*)ERROR_PTR("str is unacceptably empty", __func__, NULL);
	if (dstsize < 20)
		return (char*)ERROR_PTR("dstsize is too small", __func__, NULL);
	char* buffer = dst;
	if (!buffer) {
		buffer = (char*)LEPT_MALLOC(dstsize);
		if (!buffer)
			return (char*)ERROR_PTR("buffer not made", __func__, NULL);
	}
	if (!additional_acceptable_set || !*additional_acceptable_set)
		additional_acceptable_set = "_";

	int slen = strlen(str);

	l_uint64 key = 0;
	if (l_hashStringToUint64Fast(str, &key))
		return (char*)ERROR_PTR("hash not made", __func__, NULL);
	// fold hash into a 20 bit number:
	key ^= key >> 20;
	key ^= key >> 33;
	key ^= key >> 44;
	key &= (1UL << 20) - 1;

	// if str is a path, grab the tail end instead of head+tail.
	//
	// Extra heuristic: when this is a file path, we deem 2 parent directories plenty sufficient for a 'legible' id.
	int is_path = (strpbrk(str, "\\/") != NULL);
	if (is_path) {
		int count = 3;
		for (const char* p = str + slen - 1; p >= str; p--) {
			if (strchr("\\/", *p)) {
				count--;
				if (count == 0) {
					str = p + 1;
					slen = strlen(str);
					break;
				}
			}
		}
	}

	// plan the layout:
	// - how mush space remains for the string, or should we grab its head+tail instead?
	size_t sw = dstsize - 1 - 4 /* max # of intermed */ - 5 /* hash */;

	// pre-sanitize the string:
	char sani_str[256 + 2];
	char* sani_tail;
	int sani_tail_len;
	{
		char* d = sani_str;
		char* e = sani_str + 128;
		if (slen < sw) {
			// we won't need the tail done separately, so we can use the entire buffer here.
			e = sani_str + 256 + 1;
		}
		int has_replaced = FALSE;
		for (const char* p = str; *p && d < e; p++) {
			// faking a goto-type flow here:
			switch (0) {
			case 0:
				if (*p >= 'A' && *p <= 'Z')
					break;
				if (*p >= 'a' && *p <= 'z')
					break;
				if (*p >= '0' && *p <= '9')
					break;
				if (*p == '_')
					break;
				if (strchr(additional_acceptable_set, *p))
					break;
				if (!has_replaced) {
					*d++ = '_';
					has_replaced = TRUE;
				}
				continue;
			}
			has_replaced = FALSE;
			*d++ = *p;
		}
		assert(d <= e);
		*d = 0;

		if (!(slen < sw)) {
			// ditto for the tail, i.e. sanitize in reverse:
			d = sani_str + 128 + 2;
			e = d + 128;
			*--e = 0;
			has_replaced = FALSE;
			for (const char* p = str + slen - 1; p >= str && d < e; p--) {
				// faking a goto-type flow here:
				switch (0) {
				case 0:
					if (*p >= 'A' && *p <= 'Z')
						break;
					if (*p >= 'a' && *p <= 'z')
						break;
					if (*p >= '0' && *p <= '9')
						break;
					if (*p == '_')
						break;
					if (strchr(additional_acceptable_set, *p))
						break;
					if (!has_replaced) {
						*--e = '_';
						has_replaced = TRUE;
					}
					continue;
				}
				has_replaced = FALSE;
				*--e = *p;
			}
			sani_tail = e;
			sani_tail_len = sani_str + 256 + 2 - e;
		}
		else {
			sani_tail = NULL;
			sani_tail_len = 0;
		}
	}

	if (strchr(additional_acceptable_set, '#')) {
		// can lead with sequence number
		if (slen < sw) {
			snprintf(buffer, dstsize, "%s.#%05X", sani_str, (unsigned int)key);
		}
		else {
			sw--;
			int leadlen = sw / 3;
			if (leadlen < 5 || is_path)
				leadlen = 0;
			if (leadlen > 128)
				leadlen = 128;
			int sani_len = strlen(sani_str);
			if (leadlen >= sani_len) {
				leadlen = sani_len;
			}
			else {
				// as the above will cut the sanitized string *anywhere*, we apply a little 'readability/beautification' heuristic:
				// if there's a 'word boundary' close by, use that as the new edge.
				for (char* p = sani_str + L_MIN(leadlen, sani_len - 1); p > sani_str; p--) {
					if (*p >= 'A' && *p <= 'Z')
						continue;
					if (*p >= 'a' && *p <= 'z')
						continue;
					if (*p >= '0' && *p <= '9')
						continue;
					if (*p == '_')
						continue;
					leadlen = p - sani_str;
					break;
				}
			}

			int taillen = sw - leadlen;
			if (taillen < sani_tail_len) {
				sani_tail += sani_tail_len - taillen;
				// as the above will cut the sanitized string *anywhere*, we apply a little 'readability/beautification' heuristic:
				// if there's a 'word boundary' close by, use that as the new start/edge.
				for (char* p = sani_tail; *p && p < sani_tail + taillen / 3; p++) {
					if (*p >= 'A' && *p <= 'Z')
						continue;
					if (*p >= 'a' && *p <= 'z')
						continue;
					if (*p >= '0' && *p <= '9')
						continue;
					if (*p == '_')
						continue;
					sani_tail = p + 1;
					break;
				}
			}

			if (leadlen > 0)
				snprintf(buffer, dstsize, "%.*s.%s.#%05X", leadlen, sani_str, sani_tail, (unsigned int)key);
			else
				snprintf(buffer, dstsize, "%s.#%05X", sani_tail, (unsigned int)key);
		}
	}
	else {
		// must produce a decent indentifier at all times:
		if (slen < sw) {
			snprintf(buffer, dstsize, "u_%s_%05X", sani_str, (unsigned int)key);
		}
		else {
			sw--;
			int leadlen = sw / 3;
			if (leadlen < 5 || is_path)
				leadlen = 0;
			if (leadlen > 128)
				leadlen = 128;
			int sani_len = strlen(sani_str);
			if (leadlen >= sani_len) {
				leadlen = sani_len;
			}
			else {
				// as the above will cut the sanitized string *anywhere*, we apply a little 'readability/beautification' heuristic:
				// if there's a 'word boundary' close by, use that as the new edge.
				for (char* p = sani_str + L_MIN(leadlen, sani_len - 1); p > sani_str; p--) {
					if (*p >= 'A' && *p <= 'Z')
						continue;
					if (*p >= 'a' && *p <= 'z')
						continue;
					if (*p >= '0' && *p <= '9')
						continue;
					if (*p == '_')
						continue;
					leadlen = p - sani_str;
					break;
				}
			}

			int taillen = sw - leadlen;
			if (taillen < sani_tail_len) {
				sani_tail += sani_tail_len - taillen;
				// as the above will cut the sanitized string *anywhere*, we apply a little 'readability/beautification' heuristic:
				// if there's a 'word boundary' close by, use that as the new start/edge.
				for (char* p = sani_tail; *p && p < sani_tail + taillen / 3; p++) {
					if (*p >= 'A' && *p <= 'Z')
						continue;
					if (*p >= 'a' && *p <= 'z')
						continue;
					if (*p >= '0' && *p <= '9')
						continue;
					if (*p == '_')
						continue;
					sani_tail = p + 1;
					break;
				}
			}

			if (leadlen > 0)
				snprintf(buffer, dstsize, "u_%.*s_%s_%05X", leadlen, sani_str, sani_tail, (unsigned int)key);
			else
				snprintf(buffer, dstsize, "u_%s_%05X", sani_tail, (unsigned int)key);
		}
	}

	return buffer;
}



