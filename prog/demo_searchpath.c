/*====================================================================*
-  Copyright (C) 2001 Leptonica.  All rights reserved.
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
*  demo_searchpath.c
*
*   Helper functions:
*   * lept_locate_file_in_searchpath() : locate the given file in one of the predefined directories, return the first matching path.
*/

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include "allheaders.h"
#include "demo_settings.h"

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
#include <time.h>
#include <sys/stat.h>
#include <assert.h>

#include <wildmatch/wildmatch.h>

#ifndef PATH_MAX
#define PATH_MAX 2048
#endif

#define CACHE_SIZE 20

static unsigned int next_cache_index = 0;

struct cache_slot
{
	char* path;
	const char* src_search_path;
};

static struct cache_slot pathcache[CACHE_SIZE] = { {0} };

static unsigned int get_next_fresh_cache_slot_index(const char *path)
{
	for (int i = 0; i < sizeof(pathcache) / sizeof(pathcache[0]); i++)
	{
		if (pathcache[i].src_search_path && 0 == strcmp(pathcache[i].src_search_path, path))
			return i;
	}
	unsigned int c_idx = next_cache_index;
	next_cache_index++;
	if (next_cache_index >= CACHE_SIZE)
		next_cache_index = 0;

	// discard the old cached src path: signal this is a fresh, empty, slot.
	// 
	// Keep the scratch buffer though: no need to free+realloc that one!

	if (pathcache[c_idx].src_search_path) {
		stringDestroy(&pathcache[c_idx].src_search_path);
		assert(pathcache[c_idx].src_search_path == NULL);
	}

	if (pathcache[c_idx].path) {
		stringDestroy(&pathcache[c_idx].path);
		assert(pathcache[c_idx].path == NULL);
	}
	
	return c_idx;
}

static char* concat_paths_and_resolve(const char* p1, const char* p2)
{
	char* str = stringNew(p1);
	convertSepCharsInPath(str, UNIX_PATH_SEPCHAR);

	char* cs = pathJoin(str, p2);
	stringDestroy(&str);
	str = cs;

	// resolve /tmp, clean up the path, etc...
	char* gp = genPathname(str, NULL);
	stringDestroy(&str);

	return gp;
}

static char *concat_paths_and_resolve_3(const char *p1, const char* p2, const char* p3)
{
	char* str = stringNew(p2);
	convertSepCharsInPath(str, UNIX_PATH_SEPCHAR);

	char* cs = pathJoin(str, p3);
	stringDestroy(&str);
	str = cs;

	char* gp = concat_paths_and_resolve(p1, str);
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

char* locate_file_in_searchpath(const char* file, const SARRAY* searchpaths)
{
	// empty searchpaths[] set for recursive calls, as those will already have one of the actual searchpaths applied to their `file` path.
	SARRAY* sub_searchpaths = sarrayCreateInitialized(1, "");

	// when file already has an absolute path, we don't need to apply the search paths:
	int file_root_len = getPathRootLength(file);
	if (file_root_len > 0) {
		searchpaths = sub_searchpaths;
	}

	int search_path_count = sarrayGetCount(searchpaths);
	for (int i = 0; i < search_path_count; i++)
	{
		const char* search_path = sarrayGetString((SARRAY *)searchpaths, i, L_NOCOPY);
		if (search_path == NULL)
			continue;

		char *slot_path = concat_paths_and_resolve(search_path, file);

		// The path either has wildcards, which must be resolved, or it is a direct filespec.
		char* wildcard_marker = strpbrk(slot_path, "?*");
		if (wildcard_marker)
		{
			while (wildcard_marker > slot_path && !strchr("/\\", wildcard_marker[-1]))
				wildcard_marker--;

			size_t pathlen = wildcard_marker - slot_path;
			char *basedir = stringCopySegment(slot_path, 0, pathlen);

			char* wildcard_end = strpbrk(wildcard_marker + 1, "/\\");
			if (wildcard_end)
			{
				char* wildcard_str = stringCopySegment(wildcard_marker, 0, wildcard_end - wildcard_marker);

				// check if the wildcard is '**': when it is, we also accept the EMPTY subdir, i.e. the directory itself:
				int is_starstar = (strcmp(wildcard_str, "**") == 0);
				if (is_starstar)
				{
					char* fullname = pathJoin(basedir, wildcard_end);
					lept_stderr("name: %s\n", fullname);

					char *rv = locate_file_in_searchpath(fullname, sub_searchpaths);

					stringDestroy(&fullname);

					return rv;
				}

				// wildcard applies to directory: get a list of viable subdirectories
				SARRAY* raw_list = getFilenamesInDirectoryEx(basedir, TRUE /* get subdirs only */);
				l_int32 nfiles = sarrayGetCount(raw_list);
				l_int32 index = 0;
				for (l_int32 i = 0; i < nfiles; i++) {
					char* fname = sarrayGetString(raw_list, i, L_NOCOPY);
					char* fullname = genPathname(basedir, fname);
					lept_stderr("name: %s\n", fullname);

					LEPT_FREE(fullname);
				}
				sarrayDestroy(&raw_list);
			}
		}
		else if (file_exists(slot_path))
		{
			return slot_path;
		}
	}

	return NULL;
}

static const char* predefined_search_paths =
    "./\n"
	"lept/demo-data\n"
	"../../thirdparty/leptonica/prog\n"
	"/tmp/lept/**/\n"
;


const char* lept_locate_file_in_searchpath(const char* file)
{
	unsigned int c_idx = get_next_fresh_cache_slot_index(file);
	assert(c_idx < CACHE_SIZE);
	// have we cached the actual path for this file? If so, return that previous result.
	struct cache_slot* slot = &pathcache[c_idx];
	if (slot->src_search_path != NULL)
		return slot->path;

	SARRAY* searchpaths = sarrayCreateLinesFromString(predefined_search_paths, FALSE);
	char* rv = locate_file_in_searchpath(file, searchpaths);
	slot->path = rv;
	slot->src_search_path = stringNew(file);
	sarrayDestroy(&searchpaths);
	return rv;
}


SARRAY* sarrayAppendSubPathsFromString(SARRAY* sa, const char* string, const char* basedir)
{
	static const char *separators = "\r\n;";

	if (!string)
		return (SARRAY*)ERROR_PTR("textstr not defined", __func__, NULL);

	/* Find the number of segments */
	if (sa == NULL)
	{
		sa = sarrayCreate(1);
	}
	if (sa == NULL)
		return (SARRAY*)ERROR_PTR("sa not made", __func__, NULL);

	l_int32 pos = 0;
	while (string[pos])
	{
		int off = strcspn(string + pos, separators);
		if (off > 0)
		{
			char* substr = stringCopySegment(string, pos, off);
			char* dstr = pathJoin(basedir, substr);
			stringDestroy(&substr);
			sarrayAddString(sa, dstr, L_INSERT);
		}
		pos += off;

		// skip one or more consecutive separators:
		off = strspn(string + pos, separators);
		pos += off;
	}

	return sa;
}


SARRAY* expand_response_file(const char* filepath)
{
	if (filepath == NULL)
		return NULL;

	size_t response_size = 0;
	l_uint8* respbuf = l_binaryRead(filepath, &response_size);
	char* respstr = stringCopySegment((const char*)respbuf, 0, response_size);
	SARRAY* lines = sarrayCreateLinesFromString(respstr, FALSE);
	stringDestroy(&respstr);
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
	return lines;
}


// return the set of matching file paths, located in the first search path that produces ANY results.
SARRAY* locate_matching_files_in_searchpath(SARRAY* sa, const char* line, const SARRAY* searchpaths)
{
	int sa_n_check_soll = sarrayGetCount(sa);
	int search_path_count = sarrayGetCount(searchpaths);
	for (int i = 0; i < search_path_count; i++)
	{
		int sa_n_check_ist = sarrayGetCount(sa);
		assert(sa_n_check_soll <= sa_n_check_ist);
		if (sa_n_check_ist > sa_n_check_soll)
			break;

		const char* search_path = sarrayGetString((SARRAY*)searchpaths, i, L_NOCOPY);
		if (search_path == NULL)
			continue;

		char* filepath = pathJoin(search_path, line);

		// The path either has wildcards, which must be resolved, or it is a direct filespec.
		char* wildcard_marker = strpbrk(filepath, "?*");
		if (wildcard_marker)
		{
			while (wildcard_marker > filepath && wildcard_marker[-1] != '/')
				wildcard_marker--;
			if (*wildcard_marker == '/')
				wildcard_marker++;

			size_t pathlen = wildcard_marker - filepath;
			char* basedir = stringCopySegment(filepath, 0, pathlen);

			char* wildcard_end = strchr(wildcard_marker, '/');

			// first we do a fast sanity check: when the base path does not exist, it's no use scanning it for files or subdirectories anyhow!
			if (dir_exists(basedir)) {
				if (wildcard_end)
				{
					wildcard_end++;

					// wildcard applies to directory: get a list of viable subdirectories
					SARRAY* raw_list = getFilenamesInDirectoryEx(basedir, TRUE /* get subdirs only */);
					l_int32 nfiles = sarrayGetCount(raw_list);
					l_int32 index = 0;
					for (l_int32 i = 0; i < nfiles; i++) {
						char* fname = sarrayGetString(raw_list, i, L_NOCOPY);
						char* bname = pathJoin(basedir, fname);
						char* fullname = pathJoin(bname, wildcard_end);
						lept_stderr("name: %s\n", fullname);

						sa = locate_matching_files_in_searchpath(sa, fullname, searchpaths);

						stringDestroy(&fullname);
						stringDestroy(&bname);
					}
					sarrayDestroy(&raw_list);
				}
				else
				{
					// wildcard applies to last segment: the filename itself
					SARRAY* raw_list = getFilenamesInDirectoryEx(basedir, FALSE);
					l_int32 nfiles = sarrayGetCount(raw_list);
					l_int32 index = 0;
					for (l_int32 i = 0; i < nfiles; i++) {
						char* fname = sarrayGetString(raw_list, i, L_NOCOPY);
#if defined(_WIN32)
						int flags = WM_CASEFOLD;
#else
						int flags = 0;
#endif
						if (wildmatch(wildcard_marker, fname, flags) == WM_MATCH)
						{
							char* fullname = pathJoin(basedir, fname);
							lept_stderr("name: %s\n", fullname);

							sarrayAddString(sa, fullname, L_INSERT);
						}
					}
					sarrayDestroy(&raw_list);
				}
			}
			stringDestroy(&filepath);
			stringDestroy(&basedir);
		}
		else if (file_exists(filepath))
		{
			sarrayAddString(sa, filepath, L_INSERT);
		}
	}

	return sa;
}


SARRAY* lept_locate_all_files_in_searchpaths(int count, const char** array)
{
	SARRAY* rv = NULL;
	SARRAY* base_searchpaths = sarrayCreateLinesFromString(predefined_search_paths, FALSE);

	for (int i = 0; i < count; i++)
	{
		const char* entry = array[i];
		if (!entry || !*entry)
			continue;

		// check if this is a response file; if it is, do expand it!
		if (*entry == '@')
		{
			const char* filepath = locate_file_in_searchpath(entry + 1, base_searchpaths);
			if (filepath == NULL)
			{
				return NULL;
			}

			// search paths: for file specs listed in a response file, the FIRST search path always is the basedir of the response file itself.
			//
			// Note that all paths listed in the responsefile are relative to that basedir, i.e. to the directory where the responsefile
			// is located.
			// 
			// This also applies to any SEARCHPATH specified in the responsefile!
			SARRAY* acting_searchpaths = sarrayCreateInitialized(1, filepath);
			char* respdirpath = sarrayGetString(acting_searchpaths, 0, L_NOCOPY);
			{
				char* p = strrchr(respdirpath, '/');
				if (!p)
					p = respdirpath;
				else
					p++;
				*p = 0;
			}

			char* cdir = getcwd(NULL, 0);
			if (cdir == NULL)
				return ERROR_PTR("no current dir found", __func__, NULL);

			int n = sarrayGetCount(base_searchpaths);
			for (int j = 0; j < n; j++)
			{
				const char *str = sarrayGetString(base_searchpaths, j, L_NOCOPY);
				char* dstr = pathJoin(cdir, str);
				sarrayAddString(acting_searchpaths, dstr, L_INSERT);
			}

			stringDestroy(&cdir);

			SARRAY* lines = expand_response_file(filepath);
			int lines_count = sarrayGetCount(lines);
			for (int l = 0; l < lines_count; l++)
			{
				const char* line = sarrayGetString(lines, l, L_NOCOPY);
				// there are 2 types of lines: SEARCHPATH=... and file path specs (with optional wildcards)
				if (strncmp(line, "SEARCHPATH=", 11) == 0)
				{
					respdirpath = sarrayGetString(acting_searchpaths, 0, L_COPY);
					sarrayClear(acting_searchpaths);
					sarrayAddString(acting_searchpaths, respdirpath, L_INSERT);

					acting_searchpaths = sarrayAppendSubPathsFromString(acting_searchpaths, line + 11, respdirpath);
				}
				else
				{
					// line is a file path spec:
					if (rv == NULL)
					{
						rv = sarrayCreate(0);
					}
					rv = locate_matching_files_in_searchpath(rv, line, acting_searchpaths);
				}
			}
		}
		else
		{
			// regular file path spec:
			if (rv == NULL)
			{
				rv = sarrayCreate(0);
			}
			rv = locate_matching_files_in_searchpath(rv, entry, base_searchpaths);
		}
	}

	return rv;
}


