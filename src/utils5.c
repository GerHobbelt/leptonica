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

/*
*  processing response files, expanding file paths by way of locating them in a (multiple) searchpaths.
*/

#include "allheaders.h"

#if defined(_WIN32) || defined(_WIN64)
#  define strcasecmp _stricmp
#  define strncasecmp _strnicmp
#else
#include <strings.h>
#endif
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <assert.h>

#include <wildmatch/wildmatch.h>


// return pointer to the terminating NUL sentinel
static const char *strend(const char *str)
{
	return str + strlen(str) + 1;
}

static const char* strrpbrk(const char* str, const char* set)
{
	const char* p = strpbrk(str, set);
	while (p) {
		str = p;
		p = strpbrk(str + 1, set);
	}
	return str;
}

static const char* strnrpbrk(const char* str, size_t srclen, const char* set)
{
	const char* e = str + srclen;
	const char* p = strpbrk(str, set);
	while (p && p < e) {
		str = p;
		p = strpbrk(str + 1, set);
	}
	return str;
}

static char *
pathJoin3(const char* p1, const char* p2, const char* p3)
{
	char* r = pathJoin(p1, p2);
	char* r2 = pathJoin(r, p3);
	stringDestroy(&r);
	return r2;
}

static char* resolve_path(const char* str)
{
	// resolve /tmp, clean up the path, etc...
	char* gp = genPathname(str, NULL);

	return gp;
}

static char* concat_paths_and_resolve(const char* p1, const char* p2)
{
	char* str = stringNew(p1);
	convertSepCharsInPath(str, UNIX_PATH_SEPCHAR);

	char* cs = pathJoin(str, p2);
	stringDestroy(&str);
	str = cs;

	// resolve /tmp, clean up the path, etc...
	char* gp = resolve_path(str);
	stringDestroy(&str);

	return gp;
}

static int file_exists(const char* path)
{
	l_int32 exists = 0;
	lept_file_exists(path, &exists);
	return exists;
}

static int dir_exists(const char* path)
{
	l_int32 exists = 0;
	lept_dir_exists(path, &exists);
	return exists;
}

static SARRAY*
get_dirlist_recursive(const char* basedir)
{
	SARRAY* raw_list = getFilenamesInDirectoryEx(basedir, TRUE /* get subdirs only */);
	l_int32 nfiles = sarrayGetCount(raw_list);
	l_int32 index = 0;
	for (l_int32 i = 0; i < nfiles; i++) {
		char* fname = sarrayGetString(raw_list, i, L_NOCOPY);

		char* p = pathJoin(basedir, fname);
		SARRAY* subarr = get_dirlist_recursive(p);
		sarrayJoin(raw_list, subarr);
		nfiles = sarrayGetCount(raw_list);
		stringDestroy(&p);
		sarrayDestroy(&subarr);
	}
	return raw_list;
}

// expects a (potentially wildcarded) ABSOLUTE PATH as input.
static char* locate_wildcarded_filepath(const char* abspath, SARRAY** accept_multiple_results)
{
	assert(!accept_multiple_results || *accept_multiple_results);

	int file_root_len = getPathRootLength(abspath);
	assert(file_root_len > 0);

	const char* dirpath = abspath + file_root_len;
	char* rv = NULL;

	// The path either has wildcards, which must be resolved, or it is a direct filespec.
	const char* wildcard_marker = strpbrk(dirpath, "?*[(,:;|@!+");
	if (wildcard_marker) {
		const char* dir1_end = strnrpbrk(dirpath, wildcard_marker - dirpath, "/\\");
		char* parentdir = stringCopySegment(abspath, 0, dir1_end - abspath);

		char* realparentdir = resolve_path(parentdir);

		const char* e = strpbrk(wildcard_marker + 1, "/\\");
		if (!e)
			e = strend(wildcard_marker);

		dir1_end++;
		char* wildcard_part = stringCopySegment(abspath, dir1_end - abspath, e - dir1_end);
		char* remainder = stringCopySegment(abspath, e - abspath, strend(e) - e);

		// check if the wildcard is '**': when it is, we also accept the EMPTY subdir, i.e. the directory itself.
		// When the wildcard is '**' we also accept MULTIPLE nested directories, i.e. recursive dirscan is to be applied.
		int is_starstar = (strcmp(wildcard_part, "**") == 0);
		if (is_starstar) {
			// when '**' is the last we ever saw in the input, it means we'll match ANY file in this subdir tree!
			if (!remainder[0])
				remainder = stringNew("*");

			// wildcard '**' applies to directory: get a list of viable subdirectories
			SARRAY* raw_list = get_dirlist_recursive(realparentdir);
			l_int32 nfiles = sarrayGetCount(raw_list);
			for (l_int32 i = 0; i < nfiles; i++) {
				char* ppath = sarrayGetString(raw_list, i, L_NOCOPY);

				char* testpath = pathJoin(ppath, remainder);
				char* file = locate_wildcarded_filepath(testpath, accept_multiple_results);
				if (file != NULL) {
					if (!accept_multiple_results) {
						sarrayDestroy(&raw_list);
						stringDestroy(&realparentdir);
						stringDestroy(&wildcard_part);
						stringDestroy(&parentdir);
						stringDestroy(&remainder);
						stringDestroy(&testpath);
						return file;
					}
					if (!rv)
						rv = file;
					else
						stringDestroy(&file);
				}
				stringDestroy(&testpath);
			}
			sarrayDestroy(&raw_list);
		}
		else {
			// another wildcard, not '**': use the wildcard matching library to match these against the set of directories / filenames.

			// did the wildcard apply to a directory?
			if (!remainder[0]) {
				// the wildcard applied to the filename part itself.
				SARRAY* raw_list = getFilenamesInDirectoryEx(parentdir, FALSE);
				l_int32 nfiles = sarrayGetCount(raw_list);
				l_int32 index = 0;
				for (l_int32 i = 0; i < nfiles; i++) {
					char* fname = sarrayGetString(raw_list, i, L_NOCOPY);

					// in case the wildcards turn out NOT to be wildcards, but literal ()[]{}...
					int chk = strcasecmp(wildcard_part, fname);
					if (chk != 0) {
						chk = (wildmatch(wildcard_part, fname, WM_PATHNAME | WM_PERIOD | WM_CASEFOLD | WM_KSH_BRACKETS | WM_BRACES | WM_ALT_SUBEXPR_SEPARATOR | WM_NEGATION) == WM_MATCH ? 0 : 1);
					}
					if (chk == 0) {
						char* fullname = pathJoin(realparentdir, fname);
						if (!accept_multiple_results) {
							sarrayDestroy(&raw_list);
							stringDestroy(&realparentdir);
							stringDestroy(&wildcard_part);
							stringDestroy(&parentdir);
							stringDestroy(&remainder);
							return fullname;
						}
						else {
							sarrayAddString(*accept_multiple_results, fullname, L_COPY);
							if (!rv)
								rv = fullname;
							else
								stringDestroy(&fullname);
						}
					}
				}
				sarrayDestroy(&raw_list);
			}
			else {
				// the wildcard applied to a directory part.
				assert(remainder != NULL);
				SARRAY* raw_list = getFilenamesInDirectoryEx(parentdir, TRUE);
				l_int32 nfiles = sarrayGetCount(raw_list);
				l_int32 index = 0;
				for (l_int32 i = 0; i < nfiles; i++) {
					char* fname = sarrayGetString(raw_list, i, L_NOCOPY);

					// in case the wildcards turn out NOT to be wildcards, but literal ()[]{}...
					int chk = strcasecmp(wildcard_part, fname);
					if (chk != 0) {
						chk = (wildmatch(wildcard_part, fname, WM_PATHNAME | WM_PERIOD | WM_CASEFOLD | WM_KSH_BRACKETS | WM_BRACES | WM_ALT_SUBEXPR_SEPARATOR | WM_NEGATION) == WM_MATCH ? 0 : 1);
					}
					if (chk == 0) {
						char* testpath = pathJoin3(realparentdir, fname, remainder);
						char* file = locate_wildcarded_filepath(testpath, accept_multiple_results);
						if (file != NULL) {
							if (!accept_multiple_results)
							{
								sarrayDestroy(&raw_list);
								stringDestroy(&realparentdir);
								stringDestroy(&wildcard_part);
								stringDestroy(&parentdir);
								stringDestroy(&remainder);
								stringDestroy(&testpath);
								return file;
							}
							if (!rv)
								rv = file;
							else
								stringDestroy(&file);
						}
						stringDestroy(&testpath);
					}
				}
				sarrayDestroy(&raw_list);
			}
		}
		stringDestroy(&realparentdir);
		stringDestroy(&wildcard_part);
		stringDestroy(&parentdir);
		stringDestroy(&remainder);
	}
	else {
		// no wildcard: just resolve the (absolute) path.
		char *slot_path = resolve_path(abspath);
		if (file_exists(slot_path))
		{
			return slot_path;
		}
		stringDestroy(&slot_path);
	}

	return rv;
}


char*
leptLocateFileInSearchpath(const char* file, const SARRAY* searchpaths, l_ok ignore_cwd, const char** pLocatedSearchPath)
{
	if (pLocatedSearchPath)
		*pLocatedSearchPath = NULL;

	if (!file)
		return ERROR_PTR("file path is not defined", __func__, NULL);

	// when file already has an absolute path, we don't need to apply the search paths:
	int file_root_len = getPathRootLength(file);
	if (file_root_len > 0) {
		return locate_wildcarded_filepath(file, NULL);
	}

	if (!searchpaths) {
		// when no searchpaths are specified, assume this path is relative to the 'current directory':
		if (ignore_cwd) {
			// well, nothing to report then, I suppose. Sorry, old chap. It was jolly while we were at it. Toodles!
			return NULL;
		}

		char* cdir = getcwd(NULL, 0);
		if (cdir == NULL)
			return ERROR_PTR("no current dir found", __func__, NULL);

		char* p = pathJoin(cdir, file);
		char* rv = locate_wildcarded_filepath(p, NULL);
		stringDestroy(&p);
		return rv;
	}

	// else: file path is relative: apply search paths (in order) to discover the first/only actual file location.
	int search_path_count = sarrayGetCount(searchpaths);
	for (int i = 0; i < search_path_count; i++)
	{
		const char* search_path = sarrayGetString(searchpaths, i, L_NOCOPY);
		if (search_path == NULL)
			continue;

		char* slot_path = pathJoin(search_path, file);
		char *rv = leptLocateFileInSearchpath(slot_path, NULL, ignore_cwd, NULL);
		stringDestroy(&slot_path);
		if (rv) {
			if (pLocatedSearchPath) {
				*pLocatedSearchPath = stringNew(search_path);
			}
			return rv;
		}
		stringDestroy(&rv);
	}

	return NULL;
}


SARRAY*
leptLocateAllMatchingFilesInAnySearchpath(const char* filespec, const SARRAY* searchpaths, l_LocateMode_t mode, SARRAY** pLocatedSearchPaths)
{
	if (pLocatedSearchPaths)
		*pLocatedSearchPaths = NULL;

	if (!filespec)
		return ERROR_PTR("filespec is not defined", __func__, NULL);

	l_LocateMode_t m = mode;
	m &= ~L_LOCATE_IGNORE_CURRENT_DIR_FLAG;

	// do we really need to report them all, or only *one*?!
	if (mode == L_LOCATE_IN_FIRST_ONE) {
		const char* locatedSearchPath;
		char* entry = leptLocateFileInSearchpath(filespec, searchpaths, !!(mode & L_LOCATE_IGNORE_CURRENT_DIR_FLAG), &locatedSearchPath);
		if (pLocatedSearchPaths && locatedSearchPath) {
			*pLocatedSearchPaths = sarrayCreateInitialized(1, locatedSearchPath);
		}
		return sarrayCreateInitialized(1, entry);
	}

	SARRAY* rv_arr = sarrayCreate(0);
	if (!rv_arr)
		return ERROR_PTR("rv_arr[] cannot be allocated", __func__, NULL);

	// when file already has an absolute path, we don't need to apply the search paths:
	int file_root_len = getPathRootLength(filespec);
	if (file_root_len > 0) {
		char* p = locate_wildcarded_filepath(filespec, &rv_arr);
		stringDestroy(&p);
		return rv_arr;
	}

	if (!searchpaths) {
		if (mode & L_LOCATE_IGNORE_CURRENT_DIR_FLAG) {
			// nothing to be 'found' then, I suppose...
			assert(sarrayGetCount(rv_arr) == 0);
			return rv_arr;
		}

		// when no searchpaths are specified, assume this path is relative to the 'current directory':
		char* cdir = getcwd(NULL, 0);
		if (cdir == NULL)
			return ERROR_PTR("no current dir found", __func__, NULL);

		char* p = pathJoin(cdir, filespec);
		char* rv = locate_wildcarded_filepath(p, &rv_arr);
		stringDestroy(&rv);
		stringDestroy(&p);
		return rv_arr;
	}

	// else: file path is relative: apply search paths (in order) to discover the actual file locations.
	int search_path_count = sarrayGetCount(searchpaths);
	for (int i = 0; i < search_path_count; i++)
	{
		const char* search_path = sarrayGetString(searchpaths, i, L_NOCOPY);
		if (search_path == NULL)
			continue;

		char* slot_path = pathJoin(search_path, filespec);

		SARRAY* lcl_arr = sarrayCreate(0);

		// when file already has an absolute path, we don't need to apply the search paths:
		file_root_len = getPathRootLength(slot_path);
		if (file_root_len > 0) {
			char* p = locate_wildcarded_filepath(slot_path, &lcl_arr);
			stringDestroy(&p);
		}
		else {
			// not an absolute path: assume this path is relative to the 'current directory':
			char* cdir = getcwd(NULL, 0);
			if (cdir == NULL)
				return ERROR_PTR("no current dir found", __func__, NULL);

			char* p = pathJoin(cdir, slot_path);
			char* rv = locate_wildcarded_filepath(p, &lcl_arr);
			stringDestroy(&rv);
			stringDestroy(&p);
		}
		stringDestroy(&slot_path);

		if (sarrayGetCount(lcl_arr) > 0) {
			if (pLocatedSearchPaths) {
				if (!*pLocatedSearchPaths) {
					*pLocatedSearchPaths = sarrayCreate(1);
				}
				sarrayAddString(*pLocatedSearchPaths, search_path, L_COPY);
			}

			sarrayJoin(rv_arr, lcl_arr);

			// L_LOCATE_IN_FIRST_ANY | L_LOCATE_IN_FIRST_ONE:
			if (m != L_LOCATE_IN_ALL) {
				sarrayDestroy(&lcl_arr);
				break;
			}
		}
		sarrayDestroy(&lcl_arr);
	}

	return rv_arr;
}


SARRAY*
leptReadResponseFile(const char* filepath)
{
	if (filepath == NULL)
		return NULL;

	size_t response_size = 0;
	l_uint8* respbuf = l_binaryRead(filepath, &response_size);
	char* respstr = stringCopySegment((const char*)respbuf, 0, response_size);
	SARRAY* lines = sarrayCreateLinesFromString(respstr, FALSE);
	stringDestroy(&respstr);
	LEPT_FREE(respbuf);

#if 0
	// filter out the comment lines: speed this up by working backwards, so we have the lowest number of in-array shifts.
	int line_no = sarrayGetCount(lines) - 1;
	while (line_no >= 0)
	{
		char* line = sarrayGetString(lines, line_no, L_NOCOPY);
		int lead = strspn(line, " \t\r\n");
		// ditch comments and whitespace-only lines
		if (line[lead] == '#' || line[lead] == '\0')
		{
			line = sarrayRemoveString(lines, line_no);
			stringDestroy(&line);
			// we know all slots ABOVE line_no have already been filtered, so we can ALWAYS decrement the line index.
		}
		else
		{
			// make sure all paths are UNIXified:
			convertSepCharsInPath(line, UNIX_PATH_SEPCHAR);
		}
		--line_no;
	}
#endif
	return lines;
}


static l_ok
streq(const char* s1, const char* s2)
{
	if (!s1 || !s2)
		return FALSE;
	return 0 == strcmp(s1, s2);
}


/*
		// make sure all paths are UNIXified:
		convertSepCharsInPath(line, UNIX_PATH_SEPCHAR);
*/


static SARRAY*
pathDeducePathSet(const SARRAY *pathset, const char *abs_basefile_path)
{
	const char* respfile_dirname;
	splitPathAtDirectory(abs_basefile_path, &respfile_dirname, NULL);

	SARRAY* searchpaths = sarrayCopy(pathset);
	sarrayInsertString(searchpaths, 0, respfile_dirname, L_INSERT);
	// replace any './' relative path references with our new basedir, based of the responsefile path itself:
	// all relative-path filespecs in a responsefile are *local* to the responsefile.
	for (int i = 1; i < sarrayGetCount(searchpaths); i++) {
		const char* sp_entry = sarrayGetString(searchpaths, i, L_NOCOPY);
		int rl = getPathRootLength(sp_entry);
		if (rl <= 0) {
			char* newpath = pathJoin(respfile_dirname, sp_entry);
			sarrayReplaceString(searchpaths, i, newpath, L_NOCOPY);
		}
	}
	SARRAY* dedupped_searchpaths;
	sarrayRemoveDupsByAset(searchpaths, &dedupped_searchpaths);
	sarrayDestroy(&searchpaths);
	return dedupped_searchpaths;
}


// parse each input line and expand all to a set of source/destination file paths, expanding wildcards, etc.
//
// The tactic used is:
// 1. assume an input line is a 'sourcefile' search spec.
//    - try to locate any matching EXISTING files using the searchpaths set and %search_mode.
// 2. iff step 1 did not produce ANY result, assume the line specifies a not-yet-existing OUTPUT filespec.
//    - prepend the given %output_basedir (which is assumed to be an ABSOLUTE PATH)
//
// Notes:
// - if you need to unambiguously differentiate between paths which were treated as INPUT paths and OUTPUT
//   paths, you MAY want to prepend the %output_basedir with an path-illegal identifying char, e.g. '\x01'.
//   The calling code can then easily observe which SARRAY entries are OUTPUT paths and act accordingly.
//
// - the 'responsefile format' includes support for 'locally active' "SEARCHPATH=..." lines: the specified
//   set of search paths (relative or absolute) will be used instead of the globally active %searchpaths
//   set.
//   Specifying a "SEARCHPATH=" line revert back to using the globally active %searchpaths set.
//
// - if you want the parser to look only at the 'current directory', then "SEARCHPATH=." is your obvious
//   friend.
//
// - SEARCHPATH entries are separated by ';' on all platforms. When you copy any kind of UNIX PATH spec
//   in the searchpaths array, you are advised to ensure all UNIX ':' path spec separators have been
//   replaced with ';'.
// - Obviously, this API does not accept any convoluted searchpaths which include ';' themselves as those
//   ';' will be parsed as SEARCHPATH path splitting markers.
//
// - generated OUTPUT paths have NOT been sanitized: they are taken as-is and %output_basedir is prepended
//   without any modification/sanitation.
//
// - any lines which include a '=' are interpreted as (yet unused) variable assignment statements, rather
//   than plain input/output file [search] specs. More precisely, 'assignment statements' are those which
//   match the `^[[:alnum:]_@$-][:!~@]?=(.*)$` extended regex. Anything else which contains an '=' is
//   treated as an (perhaps oddly named) in/out file search spec and dealt with accordingly.
//   + assignment statements are copied to the resultant SARRAY verbatim, using L_COPY mode to ensure
//     no heap corruption occurs when both input and output (result) SARRAYs are destroyed via
//     sarrayDestroy(&array).
//   + IFF you want, as we do with the OUTPUT paths, to unambiguously identify these lines, then you MAY
//     specify an non-NULL, non-EMPTY %stmt_prefix string, which will be prepended to every variable
//     statement line for easy identification afterwards.
//
// - '@'-prefixed lines are interpreted as 'reponse files', i.e. `#include`-like instructions in the
//   lines[]. These response files are located and expanded in-place, expanding the lines[] array being
//   processed. This DOES NOT impact the input %lines array as we will use a copy-on-write approach
//   to rewrite/expand lines[].
//   This '@' response file processing is recursive, i.e. '@'-prefixed lines in the expanded
//   reponse file are themselves treated as response file expansion instructions.
//
//   + When a response file cannot be located or otherwise fails to be expanded, the line is copied
//     to the results SARRAY verbatim (L_COPY style), with one change: a %fail_marker string will
//     be prepended to the line.
//     Callers MAY consider using "# " (comment) or other simple marker strings for easy and unambgious
//     recognition. (As both '#' and ' ' are sometimes-legal filepath characters, one might want to
//     follow the same scheme suggested further above and inject "\x02" instead.
//
// - copy '#' and ';'-prefixed comments verbatim BUT prepend with the %ignore_marker for easy identification afterwards.
//
// - ditto for empty lines.
//
// - any leading `[ \r\r\n]` whitespace in the input lines[] is trimmed/ignored.
//
// - NOTE about filespec lines and search paths:
//   for file specs listed in a response file (SARRAY), the FIRST search path always is the basedir of the response file itself.
//
//   The caller SHOULD ensure this behaviour by having the original responsefile's basedir listed as the first
//   slot in %searchpaths[].
//   This function will ensure this rule DOES apply for all 'sub'-responsefiles, i.e. the ones found in %lines[] and
//   expanded in-place.
//
// - This also applies to any SEARCHPATH specified in the (expanded) responsefile(s)!
//
// - use locate %mode L_LOCATE_
//
SARRAY* leptProcessResponsefileLines(SARRAY* const lines, const SARRAY* const searchpath_set, l_LocateMode_t search_mode, const char *output_basedir, const char *stmt_prefix, const char *fail_marker, const char* ignore_marker)
{
	if (!lines)
		return ERROR_PTR("lines input array is not defined", __func__, NULL);

	SARRAY* in = lines;
	int count = sarrayGetCount(in);
	SARRAY* rv = sarrayCreate(count);
	if (!rv)
		return ERROR_PTR("result array rv[] cannot be allocated", __func__, NULL);
	SARRAY* locatedSearchPaths = sarrayCreate(0);
	if (!rv)
		return ERROR_PTR("locatedSearchPaths[] cannot be allocated", __func__, NULL);
	const char* active_responsefile = NULL;
	SARRAY* active_searchpaths;
	if (!(search_mode & L_LOCATE_IGNORE_CURRENT_DIR_FLAG)) {
		// as we will be using the 'local relative to respfile dirname path', this is
		// the only time we MAY be requiring the CWD, so we better patch that one into
		// the searchpath set, hence we can permanently flag the search_mode with
		// the 'ignore CWD' flag.
		char* cdir = getcwd(NULL, 0);
		if (cdir == NULL) {
			return ERROR_PTR("no current dir found", __func__, NULL);
		}
		const char* fake_respfile = pathJoin(cdir, "(dummy)");
		active_searchpaths = pathDeducePathSet(searchpath_set, fake_respfile);
		active_responsefile = fake_respfile;
		search_mode |= L_LOCATE_IGNORE_CURRENT_DIR_FLAG;
	}
	else {
		active_searchpaths = sarrayCopy(searchpath_set);
	}
	//
	// the trick with the searchpaths stack is:
	// - a @responsefile will PUSH before and POP after
	// - a SEARCHPATH= statemtn anywhere will REPLACE the active searchpaths set.
	// - a SEARCHPATHS=[^] 'empty' assignment will revert to the searchspec set as it was
	//   *at the start* of that particular reponsefile.
	// This definition ensures consistent behaviour irrespective of the responsefile
	// being processed on its own or as part of (@-embedded in) another responsefile.
	//
	// The way this is implementted is to have each stack SLOT store 2(TWO!) searchpath
	// sets: the 'base' one and the 'active' one, where the latter is REPLACED when we
	// hit a 'SEARCHPATH=' line.
	// Whne there's no 'active' set logged in the stack, it measn the 'base' one is
	// the active one (==> no need for superfluous duplication of searchpath sets).
	//
	// Note that we track the 'active responsefile' alongside in the same stack:
	// that one is used to 'correct' any searchpath that's a *relative path* itself.
	//
	#define sp_stack_size   32				// constexpr
	struct {
		SARRAY* sp_base;
		SARRAY* sp_active;
		const char* file;
	} sp_stack[sp_stack_size] = { { active_searchpaths, NULL, active_responsefile }, { NULL } };
	unsigned int sp_stackpos = 0;

	for (int i = 0; i < count; i++)
	{
		const char* entry = sarrayGetString(in, i, L_NOCOPY);
		if (!entry || !*entry)
			continue;

		// ditch leading whitespace
		int lead = strspn(entry, " \t\r\n");
		entry += lead;

		switch (entry[0])
		{
		case ';':
		case '#':
			// copy comments verbatim BUT prepend with the %ignore_marker for easy identification afterwards:
copy_verbatim_with_fail:
			;
			{
				const char* l = stringJoin(ignore_marker, entry);
				sarrayAddString(rv, l, L_INSERT);
			}
			continue;

		case '\0':
			// discard empty lines.
			continue;

		default:
		treat_as_a_regular_line:
			;
			// all the other lines can be either '='-carrying assignment statements or filespecs.
			// Here we decide which and treat both types.
			if (TRUE) {
				int equ_offset = strcspn(entry, "=");
				if (equ_offset) {
					// scan the keyword:
					int pos;
					for (pos = 0; pos < equ_offset; pos++) {
						unsigned char c = entry[pos];
						if (isalnum(c))
							continue;
						if (c == '_')
							continue;
					}
					// skip optional/allowed whitespace.
					for (; pos < equ_offset; pos++) {
						unsigned char c = entry[pos];
						if (isspace(c))
							continue;
					}
					if (pos == equ_offset) {
						// keyword found: this is an assignment statement indeed! copy verbatim + prefix.
						const char* line = stringJoin(stmt_prefix, entry);
						sarrayAddString(rv, line, L_INSERT);
						continue;
					}
				}

				// else: a filespec! time to do some wildcard resolving, if any! :-)
				const char* locatedSearchPath = NULL;
				// search_mode determines whether we'll accept a SET or a SINGLE filespec as a result.
				SARRAY* file_set = leptLocateAllMatchingFilesInAnySearchpath(entry, active_searchpaths, search_mode, &locatedSearchPaths);
				// either way, we append the produce to the output lines set.
				sarrayJoin(rv, file_set);
				sarrayDestroy(&file_set);
			}
			continue;

			// check if this is a (non-recursive) response file; if it is, do expand it!
		case '@':
			const char* filepath = leptLocateFileInSearchpath(entry + 1, active_searchpaths, !!(search_mode & L_LOCATE_IGNORE_CURRENT_DIR_FLAG), NULL);
			if (!filepath)
			{
				L_WARNING("Failed to locate responsefile in the searchpath. File: %s", __func__, entry + 1);
				// copy reponsefile line verbatim, i.e. including leading '@' marker:
				goto copy_verbatim_with_fail;
			}

			// before we do anything, check for a cycle:
			for (int i = 0; i < sp_stackpos; i++) {
				const char* rf = sp_stack[i].file;
				if (streq(rf, filepath)) {
					stringDestroy(&filepath);
					sarrayDestroy(&rv);
					L_ERROR("cyclic inclusion of responsefiles. The detected cycle:", __func__);
					for (int j = sp_stackpos - 1; j >= 0; j--) {
						if (sp_stack[j].file) {
							L_ERROR("    (part of cycle): %s", __func__, sp_stack[j].file);
						}
					}
					L_ERROR("    (-------------)", __func__);
					return NULL;
				}
			}

			SARRAY* sublines = leptReadResponseFile(filepath);

			// 'push' instruction: ASCII ShiftOut, and quite illegal as a filepath spec :-)
			++sp_stackpos;
			if (sp_stackpos == sp_stack_size) {
				sarrayDestroy(&sublines);
				stringDestroy(&filepath);
				sarrayDestroy(&rv);
				return ERROR_PTR("SEARCHPATH=<paths> stack depth exhausted. You need to flatten/simplify your response files.", __func__, NULL);
			}
			else {
				const char* l = stringJoin(ignore_marker, entry);
				sarrayInsertString(sublines, 0, l, L_INSERT);
#if 0
				sarrayInsertString(sublines, 0, "SEARCHPATH=\x0E", L_COPY);	// 'push' instruction: ASCII ShiftOut, and quite illegal as a filepath spec :-)
#endif
				sarrayAddString(sublines, "SEARCHPATH=\x0F", L_COPY);	// 'pop' instruction, ShiftIn, and quite illegal as a filepath spec. :-)

				active_searchpaths = sp_stack[sp_stackpos].sp_base = pathDeducePathSet(active_searchpaths, filepath);
				active_responsefile = sp_stack[sp_stackpos].file = filepath;

				// inject expanded respfile content into lines[] via copy-on-write:
				if (in == lines) {
					in = sarrayCopy(in);
				}
				// no need to discard the reponsefile line in in[] yet: keep it until we destroy all at the end: simpler code here.
				sarrayInsertRange(in, i + 1, sublines, 0, -1);
				sarrayDestroy(&sublines);
				count = sarrayGetCount(in);
			}
			continue;

		case 'S':
			if (strncmp(entry + 1, "EARCHPATH=", 10) != 0)
				goto treat_as_a_regular_line;

			// detect special subcommands: push, pop, revert-to-default
			const char* searchdir_list_str = entry + 11;
			int len = strlen(searchdir_list_str);
			if (len < 2) {
				unsigned char subcommand = searchdir_list_str[0];  // "SEARCHPATH=<subcommand>"
				switch (subcommand) {
				case '\x0E':
					// 'push' instruction: ASCII ShiftOut, and quite illegal as a filepath spec :-)
					++sp_stackpos;
					if (sp_stackpos == sp_stack_size) {
						sarrayDestroy(&rv);
						return ERROR_PTR("SEARCHPATH=<paths> stack depth exhausted. You need to flatten/simplify your response files.", __func__, NULL);
					}
					else {
						SARRAY* lcl_arr = pathDeducePathSet(active_searchpaths, active_responsefile);
						sarrayDestroy(&sp_stack[sp_stackpos].sp_active);
						active_searchpaths = sp_stack[sp_stackpos].sp_active = lcl_arr;
					}
					continue;

				case '\x0F':
					// 'pop' instruction, ShiftIn.
					if (sp_stackpos > 0) {
						--sp_stackpos;
						active_searchpaths = sp_stack[sp_stackpos].sp_active;
						if (!active_searchpaths)
							active_searchpaths = sp_stack[sp_stackpos].sp_base;
						active_responsefile = sp_stack[sp_stackpos].file;

						++sp_stackpos;
						stringDestroy(&sp_stack[sp_stackpos].file);
						sarrayDestroy(&sp_stack[sp_stackpos].sp_active);
						sarrayDestroy(&sp_stack[sp_stackpos].sp_base);
						--sp_stackpos;
					}
					continue;

				case '^':
				case '\0':
					// (temporarily?) revert to base.
					if (active_searchpaths == sp_stack[sp_stackpos].sp_active) {
						sarrayDestroy(&sp_stack[sp_stackpos].sp_active);
						active_searchpaths = sp_stack[sp_stackpos].sp_base;
					}
					assert(active_searchpaths == sp_stack[sp_stackpos].sp_base);
					continue;

				default:
					break;
				}
			}

			// process the searchpaths list: first, determine which separator has been used: | ;
			char* sep_marker_p = strpbrk(searchdir_list_str, "|;");
			char sep_marker[2] = { (sep_marker_p ? *sep_marker_p : ';'), 0 };
			SARRAY* srch_arr = sarrayCreate(1);
			sarraySplitString(srch_arr, searchdir_list_str, sep_marker);

			SARRAY* lcl_arr = pathDeducePathSet(srch_arr, active_responsefile);

			if (active_searchpaths == sp_stack[sp_stackpos].sp_active) {
				sarrayDestroy(&sp_stack[sp_stackpos].sp_active);
			}
			active_searchpaths = sp_stack[sp_stackpos].sp_active = lcl_arr;

			{
				const char* l = stringJoin(ignore_marker, entry);
				sarrayAddString(rv, l, L_INSERT);
			}
			continue;
		}
	}

	// unwind stack; thsi implicitly invalidates the `active_` local variables as those are mere aliases into the stack!
	for (int i = sp_stackpos; i >= 0; i--) {
		stringDestroy(&sp_stack[sp_stackpos].file);
		sarrayDestroy(&sp_stack[sp_stackpos].sp_active);
		sarrayDestroy(&sp_stack[sp_stackpos].sp_base);
	}

	// undo the copy-on-write for `in[]`:
	if (in != lines) {
		sarrayDestroy(&in);
	}

	return rv;
}


