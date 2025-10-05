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

static const char* predefined_search_paths[] = {
	"lept/demo-data",
	"../../thirdparty/leptonica/prog",
#if defined(_WIN32)
	"N:/image-4-ocr-corpus/**/",
#endif
	"/tmp/lept/**/",
};

#define CACHE_SIZE 10

static unsigned int next_cache_index = 0;

struct cache_slot
{
	size_t size;
	char* path;
};

static struct cache_slot pathcache[CACHE_SIZE] = { {0} };

static unsigned int get_next_fresh_cache_slot_index(void)
{
	unsigned int c_idx = next_cache_index;
	next_cache_index++;
	if (next_cache_index >= CACHE_SIZE)
		next_cache_index = 0;
	return c_idx;
}

static struct cache_slot* redim_cache_slot(struct cache_slot* slot, size_t pathlen)
{
	assert(slot);
	if (!slot->path)
	{
		slot->size = pathlen + 1;
		slot->path = malloc(slot->size);
		if (!slot->path)
		{
			L_ERROR("Out of memory\n", __func__);
			exit(666);
		}
	}
	else if (slot->size < pathlen + 1)
	{
		slot->size = pathlen + 1;
		slot->path = realloc(slot->path, slot->size);
		if (!slot->path)
		{
			L_ERROR("Out of memory\n", __func__);
			exit(666);
		}
	}
}

static void concat_paths(struct cache_slot* slot, const char *p1, const char* p2, const char* p3)
{
	assert(p1);
	size_t len1 = strlen(p1);
	size_t len2 = 0;
	size_t len3 = 0;
	size_t reqlen = len1 + 2;
	if (p2)
	{
		len2 = strlen(p2);
		reqlen += len2 + 1;
	}
	if (p3)
	{
		len3 = strlen(p3);
		reqlen += len3 + 1;
	}
	redim_cache_slot(slot, reqlen);

	assert(slot->size >= reqlen);
	char* buffer = slot->path;
	memcpy(buffer, p1, len1);
	size_t off = len1;
	int has_dir_end = (off > 0 && strchr("/\\", buffer[off - 1]));
	if (p2)
	{
		if (!has_dir_end)
		{
			buffer[off++] = '/';
		}
		memcpy(buffer + off, p2, len2);
		off += len2;
		has_dir_end = (off > 0 && strchr("/\\", buffer[off - 1]));
	}
	if (p3)
	{
		if (!has_dir_end)
		{
			buffer[off++] = '/';
		}
		memcpy(buffer + off, p3, len3);
		off += len3;
		has_dir_end = (off > 0 && strchr("/\\", buffer[off - 1]));
	}
	buffer[off] = 0;

	// resolve /tmp, clean up the path, etc...
	{
		char* gp = genPathname(buffer, NULL);
		size_t plen = strlen(gp);
		redim_cache_slot(slot, plen + 1);
		buffer = slot->path;
		strcpy(buffer, gp);
		LEPT_FREE(gp);
	}
}


const char* lept_locate_file_in_seatchpath(const char* file)
{
	unsigned int c_idx = get_next_fresh_cache_slot_index();
	assert(c_idx < CACHE_SIZE);
	struct cache_slot* slot = &pathcache[c_idx];
	unsigned int c_idx2 = get_next_fresh_cache_slot_index();

	for (int i = 0; i < sizeof(predefined_search_paths) / sizeof(predefined_search_paths[0]); i++)
	{
		concat_paths(slot, predefined_search_paths[i], file);
		// The path either has wildcards, which must be resolved, or it is a direct filespec.
		char* wildcard_marker = strpbrk(slot->path, "?*");
			if (wildcard_marker)
		{
				while (wildcard_marker > slot->path && !strchr("/\\", wildcard_marker[-1]))
					wildcard_marker--;

				struct cache_slot* slot2 = &pathcache[c_idx2];
				size_t pathlen = wildcard_marker - slot->path;
				redim_cache_slot(slot2, slot->size);
				memcpy(slot2->path, slot->path, pathlen);
				slot2->path[pathlen] = 0;

				char* wildcard_end = strpbrk(wildcard_marker + 1, "/\\");
				if (wildcard_end)
				{
					// wildcard applies to directory: get a list of viable subdirectories
					SARRAY* raw_list = getFilenamesInDirectory(slot2->path);
					l_int32 nfiles = sarrayGetCount(raw_list);
					l_int32 index = 0;
					for (l_int32 i = 0; i < nfiles; i++) {
						char *fname = sarrayGetString(safiles, i, L_NOCOPY);
						char *fullname = genPathname(dirin, fname);
						lept_stderr("name: %s\n", fullname);

						nfiles1 = sarrayGetCount(sa);
						sarrayDestroy(&sa);


				}
					wildcard_end = 

		}

	}

}



