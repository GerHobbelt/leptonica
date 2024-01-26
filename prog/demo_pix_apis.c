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
*  demo_pix_apis.c
*
*   Test:
*   * all leptonica functions which use / process a PIX.
*   * useful to visualize the various processes/effects using selected image files for input.
*/

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include "mupdf/mutool.h"
#include "mupdf/fitz.h"
#include "mupdf/helpers/jmemcust.h"

#include "allheaders.h"
#include "demo_settings.h"

#if defined(_WIN32) || defined(_WIN64)
#  define strcasecmp _stricmp
#  define strncasecmp _strnicmp
#else
#include <strings.h>
#endif
#include <stdint.h>
#include <ctype.h>
//#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>
#include <math.h>

#include <wildmatch/wildmatch.h>

/* Cope with systems (such as Windows) with no S_ISDIR */
#ifndef S_ISDIR
#define S_ISDIR(mode) ((mode) & S_IFDIR)
#endif

#ifndef MAX
#define MAX(a, b)  ((a) >= (b) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a, b)  ((a) < (b) ? (a) : (b))
#endif



#include "monolithic_examples.h"


static const char *fnames[] = {DEMOPATH("lyra.005.jpg"), DEMOPATH("lyra.036.jpg")};


// Type used in this demo app to scan the CLI arguments and account for named ones and in-irder-indexed ones, both.
typedef struct CLI_NAMED_ARG_INFO
{
	const char* name;        // allocated
	const char* value;       // *NOT* allocated
} CLI_NAMED_ARG_INFO;

// Type used in this demo app to scan the CLI arguments and account for named ones and in-irder-indexed ones, both.
typedef struct CLI_ARGV_SET
{
	// in-order values:
	const char** argv;       // allocated array; NULL-sentinel

	// named values:
	CLI_NAMED_ARG_INFO* named_args;  // allocated array; NULL-sentinel on element.name

	int current_in_order_index;   // points at the next in-order argv[] element
} CLI_ARGV_SET;


static const char* scanPastVariableName(const char* str)
{
	if (isalpha(*str))
	{
		str++;
		while (isalnum(*str))
			str++;
	}
	return str;
}

static char* LEPT_STRNDUP(const char* str, int length)
{
	char* p = LEPT_MALLOC(length + 1);
	if (!p)
		return ERROR_PTR("out of memory", __func__, NULL);

	strncpy(p, str, length);
	p[length] = 0;
	return p;
}

static char* LEPT_STRDUP(const char* str)
{
	int length = strlen(str);
	char* p = LEPT_MALLOC(length + 1);
	if (!p)
		return ERROR_PTR("out of memory", __func__, NULL);

	strcpy(p, str);
	return p;
}

static char* strdup_with_extra_space(const char* str, int extra_space)
{
	int length = strlen(str);
	char* p = LEPT_CALLOC(1, length + 1 + extra_space);
	if (!p)
		return ERROR_PTR("out of memory", __func__, NULL);

	strcpy(p, str);
	return p;
}

// concatenate str1 (clipped at str1_end) and str2
static char* strndupcat(const char* str1, const char *str1_end, const char *str2)
{
	int offset = (str1_end - str1);
	int length = strlen(str2) + offset;
	char* p = LEPT_MALLOC(length + 1);
	if (!p)
		return ERROR_PTR("out of memory", __func__, NULL);

	strncpy(p, str1, offset);
	strcpy(p + offset, str2);
	return p;
}

// return last char in string; NUL if string is empty or NULL.
static int strchlast(const char* str)
{
	if (!str || !*str)
		return 0;

	str += strlen(str) - 1;
	return *str;
}

// return pointer to first character PAST the last occurrence
// of any of the chars in set.
// If none of the characters in set are found anywhere in the source string,
// the start of the source string is returned.
static char* strrpbrkpast(char* str, const char* set)
{
	char* rv = str;
	char* p = str;
	for (;;) {
		p = strpbrk(p, set);
		if (!p)
			break;
		p++;
		rv = p;
	}
	return rv;
}

static char* strend(char* str) {
	return str + strlen(str);
}

#if defined(_WIN32)

// convert '\\' windows path separators to '/' in-place
static void mkUnixPath(char* str)
{
	for (; *str; str++) {
		if (*str == '\\')
			*str = '/';
	}
}

#else

static void mkUnixPath(char* str)
{
	//nada
}

#endif



static void cliCleanupArgvSet(CLI_ARGV_SET* rv)
{
	if (!rv)
		return;

	if (rv->named_args)
	{
		for (int i = 0; rv->named_args[i].name; i++)
		{
			LEPT_FREE(rv->named_args[i].name);
		}
		LEPT_FREE(rv->named_args);
	}

	if (rv->argv)
	{
		LEPT_FREE(rv->argv);
	}

	LEPT_FREE(rv);
}

// return NULL if not found. Otherwise return the arg value (string).
//
// The arg name is optional; may be NULL.
static const char *cliGetArg(CLI_ARGV_SET* rv, const char *name)
{
	if (!rv)
		return NULL;

	if (rv->named_args && name)
	{
		for (int i = 0; rv->named_args[i].name; i++)
		{
			if (strcasecmp(rv->named_args[i].name, name) == 0)
			{
				return rv->named_args[i].value;
			}
		}
	}

	if (rv->argv)
	{
		for (int i = 0; rv->argv[i]; i++)
		{
			if (rv->current_in_order_index == i)
			{
				rv->current_in_order_index++;
				return rv->argv[i];
			}
		}
	}

	return NULL;
}


static CLI_ARGV_SET* cliPreParse(int argcount, const char** argv)
{
	if (argcount <= 0 || argv == NULL)
		return ERROR_PTR("invalid input arguments", __func__, NULL);

	CLI_ARGV_SET* rv = LEPT_CALLOC(1, sizeof(*rv));
	if (!rv)
		return ERROR_PTR("out of memory", __func__, NULL);

	rv->argv = LEPT_CALLOC(argcount + 1, sizeof(rv->argv[0])); // always include NULL sentinel
	if (!rv->argv)
		return ERROR_PTR("out of memory", __func__, NULL);

	rv->named_args = LEPT_CALLOC(argcount + 1, sizeof(rv->named_args[0])); // always include NULL sentinel
	if (!rv->named_args)
		return ERROR_PTR("out of memory", __func__, NULL);

	int argv_fill_index = 0;
	int named_args_fill_index = 0;
	for (int i = 0; i < argcount; i++)
	{
		// look for the following 'named variable' patterns:
		//
		//   Format:                 # of argv[] elements needed:
		// -------------------------------------------------------
		//  name=value						    1
		//  name = value						3
		//  -name=value						    1
		//  --name=value						1
		//  -name value						    2
		//  --name value						2
		//  +name=value						    1
		//
		// Note: everywhere you see a '=' assignment operator above, we accept any of:
		//     =   :   :=
		//
		const char* arg = argv[i];
		if (!arg)
		{
			L_ERROR("invalid null input argument at index %d", __func__, i);
			goto err;
		}

		if (*arg == '-')
		{
			// named variable+value for sure:
			CLI_NAMED_ARG_INFO* rec = &rv->named_args[named_args_fill_index++];

			if (arg[1] == '-')
				arg++;

			// scan past variable name:
			const char* vp = scanPastVariableName(arg);
			if (vp == arg)
			{
				L_ERROR("invalid named variable input argument at index %d: \"%s\"", __func__, i, argv[i]);
				goto err;
			}
			rec->name = LEPT_STRNDUP(arg, vp - arg);
			if (*vp == 0)
			{
				// value must be in next argument; with or without assignment operator before it.
				i++;
				if (i >= argcount)
				{
					L_ERROR("named variable \"%s\" (at index %d) is missing a value argument", __func__, rec->name, i - 1);
					goto err;
				}
				arg = argv[i];
				if (strcmp(arg, "=") == 0 || strcmp(arg, ":") == 0 || strcmp(arg, ":=") == 0)
				{
					i++;
					if (i >= argcount)
					{
						L_ERROR("named variable \"%s\" (at index %d) is missing a value argument, following the assignment operator", __func__, rec->name, i - 2);
						goto err;
					}
				}
				rec->value = arg;
				continue;
			}
			// value must be part of the argument; with assignment operator before it.
			while (*vp == ':' || *vp == '=')
				vp++;
			rec->value = vp;
			continue;
		}

		if (*arg == '+')
		{
			// *possibly* a named variable+value:
			CLI_NAMED_ARG_INFO* rec = &rv->named_args[named_args_fill_index];

			// scan past variable name:
			const char* vp = scanPastVariableName(arg);
			if (vp == arg)
			{
				// we'll assume this is a regular in-order value instead!
			}
			else
			{
				const char* op = vp;
				// value must be part of the argument; with assignment operator before it.
				// Let's see if if it matches the expected format...
				while (*vp == ':' || *vp == '=')
					vp++;
				if (vp == op)
				{
					// we'll assume this is a regular in-order value instead!
				}
				else
				{
					CLI_NAMED_ARG_INFO* rec = &rv->named_args[named_args_fill_index++];
					rec->name = LEPT_STRNDUP(arg, op - arg);
					rec->value = vp;
					continue;
				}
			}
		}

		// now check for the single-arg named value pattern 'name=value':
		{
			// scan past the expected variable name:
			const char* vp = scanPastVariableName(arg);
			if (vp == arg)
			{
				// doesn't match the pattern, thus assume this is a regular in-order value
			}
			else
			{
				const char* op = vp;
				// value must be part of the argument; with assignment operator before it.
				// Let's see if if it matches the expected format...
				while (*vp == ':' || *vp == '=')
					vp++;
				if (vp == op)
				{
					// we'll assume this is a regular in-order value instead!
				}
				else
				{
					CLI_NAMED_ARG_INFO* rec = &rv->named_args[named_args_fill_index++];
					rec->name = LEPT_STRNDUP(arg, op - arg);
					rec->value = vp;
					continue;
				}
			}
		}

		// now check for the multi-arg named value pattern 'name=value':
		{
			// scan past the expected variable name:
			const char* vp = scanPastVariableName(arg);
			if (*vp != 0)
			{
				// entire arg is not a suitable variable name, so it doesn't match the pattern.
				// we'll assume this is a regular in-order value instead!
			}
			else
			{
				// value must be in next argument; with an assignment operator argument before it.
				i++;
				if (i >= argcount)
				{
					// we'll assume this is a regular in-order value instead!
				}
				else
				{
					vp = argv[i];
					if (strcmp(vp, "=") == 0 || strcmp(vp, ":") == 0 || strcmp(vp, ":=") == 0)
					{
						i++;
						if (i >= argcount)
						{
							L_ERROR("named variable \"%s\" (at index %d) is missing a value argument, following the assignment operator", __func__, arg, i - 2);
							goto err;
						}

						CLI_NAMED_ARG_INFO* rec = &rv->named_args[named_args_fill_index++];
						rec->name = LEPT_STRDUP(arg);
						rec->value = argv[i];
						continue;
					}
				}
			}
		}

		// we're looking at a regular in-order value, after all:
		rv->argv[argv_fill_index++] = arg;
	}

	// we're done.
	// 
	// The remainder of the rv->argv[], etc. elements will have been nulled by the calloc(),
	// so we have our nil sentinels for free.
	return rv;

err:
	cliCleanupArgvSet(rv);
	return NULL;
}


typedef struct PIX_INFO {
	PIX* image;
	char* filepath;
} PIX_INFO;

typedef struct PIX_INFO_A {
	int count;
	int alloc_size;

	PIX_INFO images[0];
} PIX_INFO_A;



// Take a given path and see if it's a directory (in which case we deliver all image filswithin),
// a wildcarded path or a direct file path.
//
// We always produce a non-empty PIX_INFO_A array of images; NULL on error.
static PIX_INFO_A* cliGetSrcPix(const char* path, int max_count)
{
	PIX_INFO_A* arr = NULL;

	if (!path || !*path)
		return ERROR_PTR("invalid empty argument", __func__, NULL);

	if (max_count <= 0)
		max_count = INT_MAX;

	char* dirname = strdup_with_extra_space(path, 10);
	mkUnixPath(dirname);

	// stat() will fail for wildcarded specs, but we don't worry: this is
	// used to discover straight vanilla directory-only path specs, for those
	// should get so wildcards appended!
	struct stat stbuf;
	if (stat(path, &stbuf) >= 0) {
		if (S_ISDIR(stbuf.st_mode)) {
			const char* wildcards = "/*.*";
			if ('/' == strchlast(dirname))
				wildcards++;
			strcat(dirname, wildcards);
		}
	}

	char* fname_pos = strrpbrkpast(dirname, "/");

	arr = LEPT_MALLOC(sizeof(arr) + 100 * sizeof(arr->images[0]));
	if (!arr)
		return ERROR_PTR("out of memory", __func__, NULL);
	arr->alloc_size = 100;
	arr->count = 0;

	fname_pos[-1] = 0;
	SARRAY* sa;
	if ((sa = getSortedPathnamesInDirectory(dirname, NULL, 0, 0)) == NULL) {
		fprintf(stderr, "Cannot scan %s (%s)\n", dirname, strerror(errno));
		//sa = getFilenamesInDirectory(dirname);
		return NULL;
	}

	int nfiles = sarrayGetCount(sa);
	const char* fname;
	for (int i = 0; i < nfiles && arr->count < max_count; i++) {
		fname = sarrayGetString(sa, i, L_NOCOPY);

		const char* name_pos = strrpbrkpast(fname, "/");
		int match = wildmatch(fname_pos, name_pos, WM_IGNORECASE | WM_PATHNAME | WM_PERIOD);
		if (match != WM_MATCH)
			continue;

		L_INFO("Loading image %d/%d: %s\n", __func__, i, nfiles, fname);

		FILE* fp = fopenReadStream(fname);
		if (fp) {
			l_int32 format = IFF_UNKNOWN;
			findFileFormatStream(fp, &format);
			switch (format)
			{
			default:
			case IFF_BMP:
			case IFF_JFIF_JPEG:
			case IFF_PNG:
				break;

			case IFF_TIFF:
			case IFF_TIFF_PACKBITS:
			case IFF_TIFF_RLE:
			case IFF_TIFF_G3:
			case IFF_TIFF_G4:
			case IFF_TIFF_LZW:
			case IFF_TIFF_ZIP:
			case IFF_TIFF_JPEG:
				l_int32 npages = 0;
				tiffGetCount(fp, &npages);
				L_INFO(" Tiff: %d pages\n", __func__, npages);

				fclose(fp);
				fp = NULL;

				if (npages > 1) {
					PIXA* pixa = pixaReadMultipageTiff(fname);
					if (!pixa) {
						L_WARNING("multipage image tiff file %d (%s) not read\n", __func__, i, fname);
						continue;
					}

					l_int32 imgcount = pixaGetCount(pixa);
					const int p10 = (int)(1 + log10(imgcount));
					for (int i = 0; i < imgcount; i++) {
						PIX* pixs = pixaGetPix(pixa, i, L_CLONE);

						if (arr->count == arr->alloc_size) {
							int size = arr->alloc_size * 2;
							arr = LEPT_REALLOC(arr, sizeof(arr) + size * sizeof(arr->images[0]));
							if (!arr)
								return ERROR_PTR("out of memory", __func__, NULL);
							arr->alloc_size = size;
						}

						char* p = strdup_with_extra_space(fname, 16);
						if (!p)
							return ERROR_PTR("out of memory", __func__, NULL);

						PIX_INFO* info = &arr->images[arr->count++];
						info->image = pixs;
						info->filepath = p;

						// append page-within-file as a suffix:
						snprintf(strend(info->filepath), 16, "::%0*d", p10, i);
					}
					pixaDestroy(&pixa);
					continue;
				}
				break;

			case IFF_GIF:
				PIXA* pixa = pixaReadMultipageStreamGif(fp);
				fclose(fp);
				fp = NULL;
				if (!pixa) {
					L_WARNING("multipage image gif file %d (%s) not read\n", __func__, i, fname);
					continue;
				}

				l_int32 imgcount = pixaGetCount(pixa);
				const int p10 = (int)(1 + log10(imgcount));
				for (int i = 0; i < imgcount; i++) {
					PIX* pixs = pixaGetPix(pixa, i, L_CLONE);

					if (arr->count == arr->alloc_size) {
						int size = arr->alloc_size * 2;
						arr = LEPT_REALLOC(arr, sizeof(arr) + size * sizeof(arr->images[0]));
						if (!arr)
							return ERROR_PTR("out of memory", __func__, NULL);
						arr->alloc_size = size;
					}

					char* p = strdup_with_extra_space(fname, 16);
					if (!p)
						return ERROR_PTR("out of memory", __func__, NULL);

					PIX_INFO* info = &arr->images[arr->count++];
					info->image = pixs;
					info->filepath = p;

					// append page-within-file as a suffix:
					snprintf(strend(info->filepath), 16, "::%0*d", p10, i);
				}
				pixaDestroy(&pixa);
				continue;
			}

			if (fp)
				fclose(fp);
		}

		PIX* pixs = pixRead(fname);
		if (!pixs) {
			L_WARNING("image file %d (%s) not read\n", __func__, i, fname);
			continue;
		}

		if (arr->count == arr->alloc_size) {
			int size = arr->alloc_size * 2;
			arr = LEPT_REALLOC(arr, sizeof(arr) + size * sizeof(arr->images[0]));
			if (!arr)
				return ERROR_PTR("out of memory", __func__, NULL);
			arr->alloc_size = size;
		}

		char* p = strdup(fname);
		if (!p)
			return ERROR_PTR("out of memory", __func__, NULL);

		PIX_INFO* info = &arr->images[arr->count++];
		info->image = pixs;
		info->filepath = p;
	}

	sarrayDestroy(&sa);
	LEPT_FREE(dirname);

	return arr;
}


static void pixInfoArrayDestroy(PIX_INFO_A** aref)
{
	if (!*aref)
		return;

	PIX_INFO_A* arr = *aref;
	*aref = NULL;

	for (int i = 0; i < arr->count; i++) {
		PIX_INFO info = arr->images[i];
		LEPT_FREE(info.filepath);
		pixDestroy(&info.image);
	}
	LEPT_FREE(arr);
}

static int usage(void)
{
	fprintf(stderr, "USAGE: ...........\n");
	return 1;
}




#if defined(BUILD_MONOLITHIC)
#define main   lept_demo_pix_apis_main
#endif

int main(int argc, const char **argv)
{
	L_REGPARAMS  *rp;

	// register a mupdf-aligned default heap memory manager for jpeg/jpeg-turbo
	fz_set_default_jpeg_sys_mem_mgr();
	fz_set_leptonica_mem(fz_get_global_context());

	if (regTestSetup(MIN(argc, 1), argv, &rp))
		return 1;
	rp->mode = L_REG_DISPLAY;
	rp->display = TRUE;

	l_chooseDisplayProg(L_DISPLAY_WITH_OPEN);

	lept_mkdir("lept/demo_pix");

	CLI_ARGV_SET* args_info = cliPreParse(argc - 1, argv + 1);
	if (!args_info) {
		return usage();
	}

	const char* pix_path = cliGetArg(args_info, "pixs");
	if (!pix_path) {
		fprintf(stderr, "Missing pixs argument.\n");
		return 1;
	}
	PIX_INFO_A* pixs_arg = cliGetSrcPix(pix_path, 10);
	if (!pixs_arg) {
		fprintf(stderr, "No images located at %s.\n", pix_path);
		return 1;
	}

	lept_stderr("CLI: pixs: %s\n", pix_path);

	for (int src_index = 0; src_index < pixs_arg->count; src_index++) {
		PIX_INFO pix_info = pixs_arg->images[src_index];

		lept_stderr("IMAGE: pixs: %s\n", pix_info.filepath);

		PIX* pixs = pixClone(pix_info.image);
		pixDisplayWithTitle(pixs, 50, 0, "pixs", rp->display);
		pixDestroy(&pixs);
	}

	pixInfoArrayDestroy(&pixs_arg);
	cliCleanupArgvSet(args_info);

	return regTestCleanup(rp);
}




#if  RENDER_PAGES
/* Show the results on a 2x reduced image, where each
 * word is outlined and the color of the box depends on the
 * computed textline. */
pix1 = pixReduceRankBinary2(pixs, 2, NULL);
pixGetDimensions(pix1, &w, &h, NULL);
pixd = pixCreate(w, h, 8);
cmap = pixcmapCreateRandom(8, 1, 1);  /* first color is black */
pixSetColormap(pixd, cmap);

pix2 = pixUnpackBinary(pix1, 8, 1);
pixRasterop(pixd, 0, 0, w, h, PIX_SRC | PIX_DST, pix2, 0, 0);
ncomp = boxaGetCount(boxa);
for (j = 0; j < ncomp; j++) {
	box = boxaGetBox(boxa, j, L_CLONE);
	numaGetIValue(nai, j, &ival);
	index = 1 + (ival % 254);  /* omit black and white */
	pixcmapGetColor(cmap, index, &rval, &gval, &bval);
	pixRenderBoxArb(pixd, box, 2, rval, gval, bval);
	boxDestroy(&box);
}

snprintf(filename, BUF_SIZE, "%s.%05d", rootname, i);
lept_stderr("filename: %s\n", filename);
pixWrite(filename, pixd, IFF_PNG);
pixDestroy(&pix1);
pixDestroy(&pix2);
pixDestroy(&pixs);
pixDestroy(&pixd);
#endif  /* RENDER_PAGES */

#if 0
PIXA* pixaReadMultipageTiff(const char* filename);
PIXA* pixaReadFiles(const char* dirname, const char* substr);
PIXA* pixaReadFilesSA(SARRAY * sa);
PIX* pixRead(const char* filename);
PIX* pixReadWithHint(const char* filename, l_int32 hint);
PIX* pixReadIndexed(SARRAY * sa, l_int32 index);

findFileFormatStream(fp, &format);
switch (format)
{
case IFF_BMP:
	if ((pix = pixReadStreamBmp(fp)) == NULL)
		return (PIX*)ERROR_PTR("bmp: no pix returned", __func__, NULL);
	break;

case IFF_JFIF_JPEG:
	if ((pix = pixReadStreamJpeg(fp, 0, 1, NULL, hint)) == NULL)
		return (PIX*)ERROR_PTR("jpeg: no pix returned", __func__, NULL);
	ret = fgetJpegComment(fp, &comment);
	if (!ret && comment)
		pixSetText(pix, (char*)comment);
	LEPT_FREE(comment);
	break;

case IFF_PNG:
	if ((pix = pixReadStreamPng(fp)) == NULL)
		return (PIX*)ERROR_PTR("png: no pix returned", __func__, NULL);
	break;

case IFF_TIFF:
case IFF_TIFF_PACKBITS:
case IFF_TIFF_RLE:
case IFF_TIFF_G3:
case IFF_TIFF_G4:
case IFF_TIFF_LZW:
case IFF_TIFF_ZIP:
case IFF_TIFF_JPEG:
	if ((pix = pixReadStreamTiff(fp, 0)) == NULL)  /* page 0 by default */
		return (PIX*)ERROR_PTR("tiff: no pix returned", __func__, NULL);
	break;


	{
		l_int32  i, npages;
		FILE* fp;
		PIX* pix;
		PIXA* pixa;
		TIFF* tif;

		if (!filename)
			return (PIXA*)ERROR_PTR("filename not defined", __func__, NULL);

		if ((fp = fopenReadStream(filename)) == NULL)

			return (PIXA*)ERROR_PTR_1("stream not opened",
				filename, __func__, NULL);
		if (fileFormatIsTiff(fp)) {
			tiffGetCount(fp, &npages);
			L_INFO(" Tiff: %d pages\n", __func__, npages);
		}
		else {
			return (PIXA*)ERROR_PTR_1("file is not tiff",
				filename, __func__, NULL);
		}

		if ((tif = fopenTiff(fp, "r")) == NULL)
			return (PIXA*)ERROR_PTR_1("tif not opened",
				filename, __func__, NULL);
#endif




#if 0

	pixr = pixRotate90(pixs, (pageno % 2) ? 1 : -1);
	pixg = pixConvertTo8(pixr, 0);
	pixGetDimensions(pixg, &w, &h, NULL);

	/* Get info on vertical intensity profile */
	pixgi = pixInvert(NULL, pixg);

	/* Output visuals */
	pixa2 = pixaCreate(3);
	pixaAddPix(pixa2, pixr, L_INSERT);
	pixaAddPix(pixa2, pix1, L_INSERT);
	pixaAddPix(pixa2, pix2, L_INSERT);
	pixd = pixaDisplayTiledInColumns(pixa2, 2, 1.0, 25, 0);
	pixaDestroy(&pixa2);
	pixaAddPix(pixa1, pixd, L_INSERT);
	pixDisplayWithTitle(pixd, 800 * i, 100, NULL, rp->display);
	pixDestroy(&pixs);
	pixDestroy(&pixg);
	pixDestroy(&pixgi);
	numaDestroy(&narl);
	numaDestroy(&nart);
	numaDestroy(&nait);
}

lept_stderr("Writing profiles to /tmp/lept/crop/croptest.pdf\n");
pixaConvertToPdf(pixa1, 75, 1.0, L_JPEG_ENCODE, 0, "Profiles",
	"/tmp/lept/crop/croptest.pdf");
pixaDestroy(&pixa1);

/* Test rectangle clipping with border */
pix1 = pixRead(DEMOPATH("lyra.005.jpg"));
pix2 = pixScale(pix1, 0.5, 0.5);
box1 = boxCreate(125, 50, 180, 230);  /* fully contained */
pix3 = pixClipRectangleWithBorder(pix2, box1, 30, &box2);
pixRenderBoxArb(pix2, box1, 2, 255, 0, 0);
pixRenderBoxArb(pix3, box2, 2, 255, 0, 0);
pixa1 = pixaCreate(2);
pixaAddPix(pixa1, pix2, L_INSERT);
pixaAddPix(pixa1, pix3, L_INSERT);
pix4 = pixaDisplayTiledInColumns(pixa1, 2, 1.0, 15, 2);
regTestWritePixAndCheck(rp, pix4, IFF_PNG);  /* 6 */
pixDisplayWithTitle(pix4, 325, 700, NULL, rp->display);
boxDestroy(&box1);
boxDestroy(&box2);
pixDestroy(&pix4);
pixaDestroy(&pixa1);

pix2 = pixScale(pix1, 0.5, 0.5);
box1 = boxCreate(125, 10, 180, 270);  /* not full border */
pix3 = pixClipRectangleWithBorder(pix2, box1, 30, &box2);
pixRenderBoxArb(pix2, box1, 2, 255, 0, 0);
pixRenderBoxArb(pix3, box2, 2, 255, 0, 0);
pixa1 = pixaCreate(2);
pixaAddPix(pixa1, pix2, L_INSERT);
pixaAddPix(pixa1, pix3, L_INSERT);
pix4 = pixaDisplayTiledInColumns(pixa1, 2, 1.0, 15, 2);
regTestWritePixAndCheck(rp, pix4, IFF_PNG);  /* 7 */
pixDisplayWithTitle(pix4, 975, 700, NULL, rp->display);
boxDestroy(&box1);
boxDestroy(&box2);
pixDestroy(&pix4);
pixaDestroy(&pixa1);

pix2 = pixScale(pix1, 0.5, 0.5);
box1 = boxCreate(125, 200, 180, 270);  /* not entirely within pix2 */
pix3 = pixClipRectangleWithBorder(pix2, box1, 30, &box2);
pixRenderBoxArb(pix2, box1, 2, 255, 0, 0);
pixRenderBoxArb(pix3, box2, 2, 255, 0, 0);
pixa1 = pixaCreate(2);
pixaAddPix(pixa1, pix2, L_INSERT);
pixaAddPix(pixa1, pix3, L_INSERT);
pix4 = pixaDisplayTiledInColumns(pixa1, 2, 1.0, 15, 2);
regTestWritePixAndCheck(rp, pix4, IFF_PNG);  /* 8 */
pixDisplayWithTitle(pix4, 1600, 700, NULL, rp->display);
boxDestroy(&box1);
boxDestroy(&box2);
pixDestroy(&pix4);
pixaDestroy(&pixa1);
pixDestroy(&pix1);

return regTestCleanup(rp);
}

#endif











		PIX* pixCleanBackgroundToWhite(PIX* pixs, PIX* pixim, PIX* pixg, l_float32 gamma, l_int32 blackval, l_int32 whiteval);
		PIX* pixBackgroundNormSimple(PIX* pixs, PIX* pixim, PIX* pixg);
		PIX* pixBackgroundNorm(PIX* pixs, PIX* pixim, PIX* pixg, l_int32 sx, l_int32 sy, l_int32 thresh, l_int32 mincount, l_int32 bgval, l_int32 smoothx, l_int32 smoothy);
		PIX* pixBackgroundNormMorph(PIX* pixs, PIX* pixim, l_int32 reduction, l_int32 size, l_int32 bgval);
		l_ok pixBackgroundNormGrayArray(PIX* pixs, PIX* pixim, l_int32 sx, l_int32 sy, l_int32 thresh, l_int32 mincount, l_int32 bgval, l_int32 smoothx, l_int32 smoothy, PIX** ppixd);
		l_ok pixBackgroundNormRGBArrays(PIX* pixs, PIX* pixim, PIX* pixg, l_int32 sx, l_int32 sy, l_int32 thresh, l_int32 mincount, l_int32 bgval, l_int32 smoothx, l_int32 smoothy, PIX** ppixr, PIX** ppixg, PIX** ppixb);
		l_ok pixBackgroundNormGrayArrayMorph(PIX* pixs, PIX* pixim, l_int32 reduction, l_int32 size, l_int32 bgval, PIX** ppixd);
		l_ok pixBackgroundNormRGBArraysMorph(PIX* pixs, PIX* pixim, l_int32 reduction, l_int32 size, l_int32 bgval, PIX** ppixr, PIX** ppixg, PIX** ppixb);
		l_ok pixGetBackgroundGrayMap(PIX* pixs, PIX* pixim, l_int32 sx, l_int32 sy, l_int32 thresh, l_int32 mincount, PIX** ppixd);
		l_ok pixGetBackgroundRGBMap(PIX* pixs, PIX* pixim, PIX* pixg, l_int32 sx, l_int32 sy, l_int32 thresh, l_int32 mincount, PIX** ppixmr, PIX** ppixmg, PIX** ppixmb);
		l_ok pixGetBackgroundGrayMapMorph(PIX* pixs, PIX* pixim, l_int32 reduction, l_int32 size, PIX** ppixm);
		l_ok pixGetBackgroundRGBMapMorph(PIX* pixs, PIX* pixim, l_int32 reduction, l_int32 size, PIX** ppixmr, PIX** ppixmg, PIX** ppixmb);
		l_ok pixFillMapHoles(PIX* pix, l_int32 nx, l_int32 ny, l_int32 filltype);
		PIX* pixExtendByReplication(PIX* pixs, l_int32 addw, l_int32 addh);
		l_ok pixSmoothConnectedRegions(PIX* pixs, PIX* pixm, l_int32 factor);
		PIX* pixGetInvBackgroundMap(PIX* pixs, l_int32 bgval, l_int32 smoothx, l_int32 smoothy);
		PIX* pixApplyInvBackgroundGrayMap(PIX* pixs, PIX* pixm, l_int32 sx, l_int32 sy);
		PIX* pixApplyInvBackgroundRGBMap(PIX* pixs, PIX* pixmr, PIX* pixmg, PIX* pixmb, l_int32 sx, l_int32 sy);
		PIX* pixApplyVariableGrayMap(PIX* pixs, PIX* pixg, l_int32 target);
		PIX* pixGlobalNormRGB(PIX* pixd, PIX* pixs, l_int32 rval, l_int32 gval, l_int32 bval, l_int32 mapval);
		PIX* pixGlobalNormNoSatRGB(PIX* pixd, PIX* pixs, l_int32 rval, l_int32 gval, l_int32 bval, l_int32 factor, l_float32 rank);
		l_ok pixThresholdSpreadNorm(PIX* pixs, l_int32 filtertype, l_int32 edgethresh, l_int32 smoothx, l_int32 smoothy, l_float32 gamma, l_int32 minval, l_int32 maxval, l_int32 targetthresh, PIX** ppixth, PIX** ppixb, PIX** ppixd);
		PIX* pixBackgroundNormFlex(PIX* pixs, l_int32 sx, l_int32 sy, l_int32 smoothx, l_int32 smoothy, l_int32 delta);
		PIX* pixContrastNorm(PIX* pixd, PIX* pixs, l_int32 sx, l_int32 sy, l_int32 mindiff, l_int32 smoothx, l_int32 smoothy);
		PIX* pixBackgroundNormTo1MinMax(PIX* pixs, l_int32 contrast, l_int32 scalefactor);
		PIX* pixConvertTo8MinMax(PIX* pixs);
		PIX* pixAffineSampledPta(PIX* pixs, PTA* ptad, PTA* ptas, l_int32 incolor);
		PIX* pixAffineSampled(PIX* pixs, l_float32* vc, l_int32 incolor);
		PIX* pixAffinePta(PIX* pixs, PTA* ptad, PTA* ptas, l_int32 incolor);
		PIX* pixAffine(PIX* pixs, l_float32* vc, l_int32 incolor);
		PIX* pixAffinePtaColor(PIX* pixs, PTA* ptad, PTA* ptas, l_uint32 colorval);
		PIX* pixAffineColor(PIX* pixs, l_float32* vc, l_uint32 colorval);
		PIX* pixAffinePtaGray(PIX* pixs, PTA* ptad, PTA* ptas, l_uint8 grayval);
		PIX* pixAffineGray(PIX* pixs, l_float32* vc, l_uint8 grayval);
		PIX* pixAffinePtaWithAlpha(PIX* pixs, PTA* ptad, PTA* ptas, PIX* pixg, l_float32 fract, l_int32 border);

		l_ok linearInterpolatePixelColor(l_uint32* datas, l_int32 wpls, l_int32 w, l_int32 h, l_float32 x, l_float32 y, l_uint32 colorval, l_uint32* pval);
		l_ok linearInterpolatePixelGray(l_uint32* datas, l_int32 wpls, l_int32 w, l_int32 h, l_float32 x, l_float32 y, l_int32 grayval, l_int32* pval);

		PIX* pixAffineSequential(PIX* pixs, PTA* ptad, PTA* ptas, l_int32 bw, l_int32 bh);

		NUMA* pixFindBaselines(PIX* pixs, PTA** ppta, PIXA* pixadb);
		PIX* pixDeskewLocal(PIX* pixs, l_int32 nslices, l_int32 redsweep, l_int32 redsearch, l_float32 sweeprange, l_float32 sweepdelta, l_float32 minbsdelta);
		l_ok pixGetLocalSkewTransform(PIX* pixs, l_int32 nslices, l_int32 redsweep, l_int32 redsearch, l_float32 sweeprange, l_float32 sweepdelta, l_float32 minbsdelta, PTA** pptas, PTA** pptad);
		NUMA* pixGetLocalSkewAngles(PIX* pixs, l_int32 nslices, l_int32 redsweep, l_int32 redsearch, l_float32 sweeprange, l_float32 sweepdelta, l_float32 minbsdelta, l_float32* pa, l_float32* pb, l_int32 debug);

		PIX* pixBilateral(PIX* pixs, l_float32 spatial_stdev, l_float32 range_stdev, l_int32 ncomps, l_int32 reduction);
		PIX* pixBilateralGray(PIX* pixs, l_float32 spatial_stdev, l_float32 range_stdev, l_int32 ncomps, l_int32 reduction);
		PIX* pixBilateralExact(PIX* pixs, L_KERNEL* spatial_kel, L_KERNEL* range_kel);
		PIX* pixBilateralGrayExact(PIX* pixs, L_KERNEL* spatial_kel, L_KERNEL* range_kel);
		PIX* pixBlockBilateralExact(PIX* pixs, l_float32 spatial_stdev, l_float32 range_stdev);
		PIX* pixBilinearSampledPta(PIX* pixs, PTA* ptad, PTA* ptas, l_int32 incolor);
		PIX* pixBilinearSampled(PIX* pixs, l_float32* vc, l_int32 incolor);
		PIX* pixBilinearPta(PIX* pixs, PTA* ptad, PTA* ptas, l_int32 incolor);
		PIX* pixBilinear(PIX* pixs, l_float32* vc, l_int32 incolor);
		PIX* pixBilinearPtaColor(PIX* pixs, PTA* ptad, PTA* ptas, l_uint32 colorval);
		PIX* pixBilinearColor(PIX* pixs, l_float32* vc, l_uint32 colorval);
		PIX* pixBilinearPtaGray(PIX* pixs, PTA* ptad, PTA* ptas, l_uint8 grayval);
		PIX* pixBilinearGray(PIX* pixs, l_float32* vc, l_uint8 grayval);
		PIX* pixBilinearPtaWithAlpha(PIX* pixs, PTA* ptad, PTA* ptas, PIX* pixg, l_float32 fract, l_int32 border);

		l_ok pixOtsuAdaptiveThreshold(PIX* pixs, l_int32 sx, l_int32 sy, l_int32 smoothx, l_int32 smoothy, l_float32 scorefract, PIX** ppixth, PIX** ppixd);
		PIX* pixOtsuThreshOnBackgroundNorm(PIX* pixs, PIX* pixim, l_int32 sx, l_int32 sy, l_int32 thresh, l_int32 mincount, l_int32 bgval, l_int32 smoothx, l_int32 smoothy, l_float32 scorefract, l_int32* pthresh);
		PIX* pixMaskedThreshOnBackgroundNorm(PIX* pixs, PIX* pixim, l_int32 sx, l_int32 sy, l_int32 thresh, l_int32 mincount, l_int32 smoothx, l_int32 smoothy, l_float32 scorefract, l_int32* pthresh);
		l_ok pixSauvolaBinarizeTiled(PIX* pixs, l_int32 whsize, l_float32 factor, l_int32 nx, l_int32 ny, PIX** ppixth, PIX** ppixd);
		l_ok pixSauvolaBinarize(PIX* pixs, l_int32 whsize, l_float32 factor, l_int32 addborder, PIX** ppixm, PIX** ppixsd, PIX** ppixth, PIX** ppixd);
		PIX* pixSauvolaOnContrastNorm(PIX* pixs, l_int32 mindiff, PIX** ppixn, PIX** ppixth);
		PIX* pixThreshOnDoubleNorm(PIX* pixs, l_int32 mindiff);
		l_ok pixThresholdByConnComp(PIX* pixs, PIX* pixm, l_int32 start, l_int32 end, l_int32 incr, l_float32 thresh48, l_float32 threshdiff, l_int32* pglobthresh, PIX** ppixd, l_int32 debugflag);
		l_ok pixThresholdByHisto(PIX* pixs, l_int32 factor, l_int32 halfw, l_int32 skip, l_int32* pthresh, PIX** ppixd, NUMA** pnahisto, PIX** ppixhisto);
		PIX* pixExpandBinaryReplicate(PIX* pixs, l_int32 xfact, l_int32 yfact);
		PIX* pixExpandBinaryPower2(PIX* pixs, l_int32 factor);
		PIX* pixReduceBinary2(PIX* pixs, l_uint8* intab);
		PIX* pixReduceRankBinaryCascade(PIX* pixs, l_int32 level1, l_int32 level2, l_int32 level3, l_int32 level4);
		PIX* pixReduceRankBinary2(PIX* pixs, l_int32 level, l_uint8* intab);

		PIX* pixBlend(PIX* pixs1, PIX* pixs2, l_int32 x, l_int32 y, l_float32 fract);
		PIX* pixBlendMask(PIX* pixd, PIX* pixs1, PIX* pixs2, l_int32 x, l_int32 y, l_float32 fract, l_int32 type);
		PIX* pixBlendGray(PIX* pixd, PIX* pixs1, PIX* pixs2, l_int32 x, l_int32 y, l_float32 fract, l_int32 type, l_int32 transparent, l_uint32 transpix);
		PIX* pixBlendGrayInverse(PIX* pixd, PIX* pixs1, PIX* pixs2, l_int32 x, l_int32 y, l_float32 fract);
		PIX* pixBlendColor(PIX* pixd, PIX* pixs1, PIX* pixs2, l_int32 x, l_int32 y, l_float32 fract, l_int32 transparent, l_uint32 transpix);
		PIX* pixBlendColorByChannel(PIX* pixd, PIX* pixs1, PIX* pixs2, l_int32 x, l_int32 y, l_float32 rfract, l_float32 gfract, l_float32 bfract, l_int32 transparent, l_uint32 transpix);
		PIX* pixBlendGrayAdapt(PIX* pixd, PIX* pixs1, PIX* pixs2, l_int32 x, l_int32 y, l_float32 fract, l_int32 shift);
		PIX* pixFadeWithGray(PIX* pixs, PIX* pixb, l_float32 factor, l_int32 type);
		PIX* pixBlendHardLight(PIX* pixd, PIX* pixs1, PIX* pixs2, l_int32 x, l_int32 y, l_float32 fract);
		l_ok pixBlendCmap(PIX* pixs, PIX* pixb, l_int32 x, l_int32 y, l_int32 sindex);
		PIX* pixBlendWithGrayMask(PIX* pixs1, PIX* pixs2, PIX* pixg, l_int32 x, l_int32 y);
		PIX* pixBlendBackgroundToColor(PIX* pixd, PIX* pixs, BOX* box, l_uint32 color, l_float32 gamma, l_int32 minval, l_int32 maxval);
		PIX* pixMultiplyByColor(PIX* pixd, PIX* pixs, BOX* box, l_uint32 color);
		PIX* pixAlphaBlendUniform(PIX* pixs, l_uint32 color);
		PIX* pixAddAlphaToBlend(PIX* pixs, l_float32 fract, l_int32 invert);
		PIX* pixSetAlphaOverWhite(PIX* pixs);
		l_ok pixLinearEdgeFade(PIX* pixs, l_int32 dir, l_int32 fadeto, l_float32 distfract, l_float32 maxfade);

		L_BMF* bmfCreate(const char* dir, l_int32 fontsize);
		void bmfDestroy(L_BMF** pbmf);
		PIX* bmfGetPix(L_BMF* bmf, char chr);

		PIXA* pixaGetFont(const char* dir, l_int32 fontsize, l_int32* pbl0, l_int32* pbl1, l_int32* pbl2);

		PIX* pixReadStreamBmp(FILE* fp);
		l_ok pixWriteStreamBmp(FILE* fp, PIX* pix);

		PIX* pixReadMemBmp(const l_uint8* cdata, size_t size);
		l_ok pixWriteMemBmp(l_uint8** pdata, size_t* psize, PIX* pix);

		BOXA* boxaCombineOverlaps(BOXA* boxas, PIXA* pixadb);
		l_ok boxaCombineOverlapsInPair(BOXA* boxas1, BOXA* boxas2, BOXA** pboxad1, BOXA** pboxad2, PIXA* pixadb);

		PIX* pixMaskConnComp(PIX* pixs, l_int32 connectivity, BOXA** pboxa);
		PIX* pixMaskBoxa(PIX* pixd, PIX* pixs, BOXA* boxa, l_int32 op);
		PIX* pixPaintBoxa(PIX* pixs, BOXA* boxa, l_uint32 val);
		PIX* pixSetBlackOrWhiteBoxa(PIX* pixs, BOXA* boxa, l_int32 op);
		PIX* pixPaintBoxaRandom(PIX* pixs, BOXA* boxa);
		PIX* pixBlendBoxaRandom(PIX* pixs, BOXA* boxa, l_float32 fract);
		PIX* pixDrawBoxa(PIX* pixs, BOXA* boxa, l_int32 width, l_uint32 val);
		PIX* pixDrawBoxaRandom(PIX* pixs, BOXA* boxa, l_int32 width);
		PIX* boxaaDisplay(PIX* pixs, BOXAA* baa, l_int32 linewba, l_int32 linewb, l_uint32 colorba, l_uint32 colorb, l_int32 w, l_int32 h);
		PIXA* pixaDisplayBoxaa(PIXA* pixas, BOXAA* baa, l_int32 colorflag, l_int32 width);
		BOXA* pixSplitIntoBoxa(PIX* pixs, l_int32 minsum, l_int32 skipdist, l_int32 delta, l_int32 maxbg, l_int32 maxcomps, l_int32 remainder);
		BOXA* pixSplitComponentIntoBoxa(PIX* pix, BOX* box, l_int32 minsum, l_int32 skipdist, l_int32 delta, l_int32 maxbg, l_int32 maxcomps, l_int32 remainder);

		l_ok boxaCompareRegions(BOXA* boxa1, BOXA* boxa2, l_int32 areathresh, l_int32* pnsame, l_float32* pdiffarea, l_float32* pdiffxor, PIX** ppixdb);
		BOX* pixSelectLargeULComp(PIX* pixs, l_float32 areaslop, l_int32 yslop, l_int32 connectivity);

		PIX* boxaDisplayTiled(BOXA* boxas, PIXA* pixa, l_int32 first, l_int32 last, l_int32 maxwidth, l_int32 linewidth, l_float32 scalefactor, l_int32 background, l_int32 spacing, l_int32 border);

		BOXA* boxaReconcileAllByMedian(BOXA* boxas, l_int32 select1, l_int32 select2, l_int32 thresh, l_int32 extra, PIXA* pixadb);
		BOXA* boxaReconcileSidesByMedian(BOXA* boxas, l_int32 select, l_int32 thresh, l_int32 extra, PIXA* pixadb);

		l_ok boxaPlotSides(BOXA* boxa, const char* plotname, NUMA** pnal, NUMA** pnat, NUMA** pnar, NUMA** pnab, PIX** ppixd);
		l_ok boxaPlotSizes(BOXA* boxa, const char* plotname, NUMA** pnaw, NUMA** pnah, PIX** ppixd);

		CCBORDA* pixGetAllCCBorders(PIX* pixs);
		PTAA* pixGetOuterBordersPtaa(PIX* pixs);
		l_ok pixGetOuterBorder(CCBORD* ccb, PIX* pixs, BOX* box);

		PIX* ccbaDisplayBorder(CCBORDA* ccba);
		PIX* ccbaDisplaySPBorder(CCBORDA* ccba);
		PIX* ccbaDisplayImage1(CCBORDA* ccba);
		PIX* ccbaDisplayImage2(CCBORDA* ccba);

		PIXA* pixaThinConnected(PIXA* pixas, l_int32 type, l_int32 connectivity, l_int32 maxiters);
		PIX* pixThinConnected(PIX* pixs, l_int32 type, l_int32 connectivity, l_int32 maxiters);
		PIX* pixThinConnectedBySet(PIX* pixs, l_int32 type, SELA* sela, l_int32 maxiters);

		l_ok pixFindCheckerboardCorners(PIX* pixs, l_int32 size, l_int32 dilation, l_int32 nsels, PIX** ppix_corners, PTA** ppta_corners, PIXA* pixadb);

		l_ok pixGetWordsInTextlines(PIX* pixs, l_int32 minwidth, l_int32 minheight, l_int32 maxwidth, l_int32 maxheight, BOXA** pboxad, PIXA** ppixad, NUMA** pnai);
		l_ok pixGetWordBoxesInTextlines(PIX* pixs, l_int32 minwidth, l_int32 minheight, l_int32 maxwidth, l_int32 maxheight, BOXA** pboxad, NUMA** pnai);
		l_ok pixFindWordAndCharacterBoxes(PIX* pixs, BOX* boxs, l_int32 thresh, BOXA** pboxaw, BOXAA** pboxaac, const char* debugdir);

		l_ok pixColorContent(PIX* pixs, l_int32 rref, l_int32 gref, l_int32 bref, l_int32 mingray, PIX** ppixr, PIX** ppixg, PIX** ppixb);
		PIX* pixColorMagnitude(PIX* pixs, l_int32 rref, l_int32 gref, l_int32 bref, l_int32 type);
		l_ok pixColorFraction(PIX* pixs, l_int32 darkthresh, l_int32 lightthresh, l_int32 diffthresh, l_int32 factor, l_float32* ppixfract, l_float32* pcolorfract);
		PIX* pixColorShiftWhitePoint(PIX* pixs, l_int32 rref, l_int32 gref, l_int32 bref);
		PIX* pixMaskOverColorPixels(PIX* pixs, l_int32 threshdiff, l_int32 mindist);
		PIX* pixMaskOverGrayPixels(PIX* pixs, l_int32 maxlimit, l_int32 satlimit);
		PIX* pixMaskOverColorRange(PIX* pixs, l_int32 rmin, l_int32 rmax, l_int32 gmin, l_int32 gmax, l_int32 bmin, l_int32 bmax);
		l_ok pixFindColorRegions(PIX* pixs, PIX* pixm, l_int32 factor, l_int32 lightthresh, l_int32 darkthresh, l_int32 mindiff, l_int32 colordiff, l_float32 edgefract, l_float32* pcolorfract, PIX** pcolormask1, PIX** pcolormask2, PIXA* pixadb);
		l_ok pixNumSignificantGrayColors(PIX* pixs, l_int32 darkthresh, l_int32 lightthresh, l_float32 minfract, l_int32 factor, l_int32* pncolors);
		l_ok pixColorsForQuantization(PIX* pixs, l_int32 thresh, l_int32* pncolors, l_int32* piscolor, l_int32 debug);
		l_ok pixNumColors(PIX* pixs, l_int32 factor, l_int32* pncolors);
		PIX* pixConvertRGBToCmapLossless(PIX* pixs);
		l_ok pixGetMostPopulatedColors(PIX* pixs, l_int32 sigbits, l_int32 factor, l_int32 ncolors, l_uint32** parray, PIXCMAP** pcmap);
		PIX* pixSimpleColorQuantize(PIX* pixs, l_int32 sigbits, l_int32 factor, l_int32 ncolors);
		NUMA* pixGetRGBHistogram(PIX* pixs, l_int32 sigbits, l_int32 factor);

		l_ok pixHasHighlightRed(PIX* pixs, l_int32 factor, l_float32 minfract, l_float32 fthresh, l_int32* phasred, l_float32* pratio, PIX** ppixdb);
		L_COLORFILL* l_colorfillCreate(PIX* pixs, l_int32 nx, l_int32 ny);

		PIX* pixColorFill(PIX* pixs, l_int32 minmax, l_int32 maxdiff, l_int32 smooth, l_int32 minarea, l_int32 debug);
		PIXA* makeColorfillTestData(l_int32 w, l_int32 h, l_int32 nseeds, l_int32 range);
		PIX* pixColorGrayRegions(PIX* pixs, BOXA* boxa, l_int32 type, l_int32 thresh, l_int32 rval, l_int32 gval, l_int32 bval);
		l_ok pixColorGray(PIX* pixs, BOX* box, l_int32 type, l_int32 thresh, l_int32 rval, l_int32 gval, l_int32 bval);
		PIX* pixColorGrayMasked(PIX* pixs, PIX* pixm, l_int32 type, l_int32 thresh, l_int32 rval, l_int32 gval, l_int32 bval);
		PIX* pixSnapColor(PIX* pixd, PIX* pixs, l_uint32 srcval, l_uint32 dstval, l_int32 diff);
		PIX* pixSnapColorCmap(PIX* pixd, PIX* pixs, l_uint32 srcval, l_uint32 dstval, l_int32 diff);
		PIX* pixLinearMapToTargetColor(PIX* pixd, PIX* pixs, l_uint32 srcval, l_uint32 dstval);

		PIX* pixShiftByComponent(PIX* pixd, PIX* pixs, l_uint32 srcval, l_uint32 dstval);

		PIX* pixMapWithInvariantHue(PIX* pixd, PIX* pixs, l_uint32 srcval, l_float32 fract);

		l_ok pixcmapIsValid(const PIXCMAP* cmap, PIX* pix, l_int32* pvalid);

		PIX* pixColorMorph(PIX* pixs, l_int32 type, l_int32 hsize, l_int32 vsize);
		PIX* pixOctreeColorQuant(PIX* pixs, l_int32 colors, l_int32 ditherflag);
		PIX* pixOctreeColorQuantGeneral(PIX* pixs, l_int32 colors, l_int32 ditherflag, l_float32 validthresh, l_float32 colorthresh);

		PIX* pixOctreeQuantByPopulation(PIX* pixs, l_int32 level, l_int32 ditherflag);
		PIX* pixOctreeQuantNumColors(PIX* pixs, l_int32 maxcolors, l_int32 subsample);
		PIX* pixOctcubeQuantMixedWithGray(PIX* pixs, l_int32 depth, l_int32 graylevels, l_int32 delta);
		PIX* pixFixedOctcubeQuant256(PIX* pixs, l_int32 ditherflag);
		PIX* pixFewColorsOctcubeQuant1(PIX* pixs, l_int32 level);
		PIX* pixFewColorsOctcubeQuant2(PIX* pixs, l_int32 level, NUMA* na, l_int32 ncolors, l_int32* pnerrors);
		PIX* pixFewColorsOctcubeQuantMixed(PIX* pixs, l_int32 level, l_int32 darkthresh, l_int32 lightthresh, l_int32 diffthresh, l_float32 minfract, l_int32 maxspan);
		PIX* pixFixedOctcubeQuantGenRGB(PIX* pixs, l_int32 level);
		PIX* pixQuantFromCmap(PIX* pixs, PIXCMAP* cmap, l_int32 mindepth, l_int32 level, l_int32 metric);
		PIX* pixOctcubeQuantFromCmap(PIX* pixs, PIXCMAP* cmap, l_int32 mindepth, l_int32 level, l_int32 metric);
		NUMA* pixOctcubeHistogram(PIX* pixs, l_int32 level, l_int32* pncolors);

		l_ok pixRemoveUnusedColors(PIX* pixs);
		l_ok pixNumberOccupiedOctcubes(PIX* pix, l_int32 level, l_int32 mincount, l_float32 minfract, l_int32* pncolors);
		PIX* pixMedianCutQuant(PIX* pixs, l_int32 ditherflag);
		PIX* pixMedianCutQuantGeneral(PIX* pixs, l_int32 ditherflag, l_int32 outdepth, l_int32 maxcolors, l_int32 sigbits, l_int32 maxsub, l_int32 checkbw);
		PIX* pixMedianCutQuantMixed(PIX* pixs, l_int32 ncolor, l_int32 ngray, l_int32 darkthresh, l_int32 lightthresh, l_int32 diffthresh);
		PIX* pixFewColorsMedianCutQuantMixed(PIX* pixs, l_int32 ncolor, l_int32 ngray, l_int32 maxncolors, l_int32 darkthresh, l_int32 lightthresh, l_int32 diffthresh);
		l_int32* pixMedianCutHisto(PIX* pixs, l_int32 sigbits, l_int32 subsample);
		PIX* pixColorSegment(PIX* pixs, l_int32 maxdist, l_int32 maxcolors, l_int32 selsize, l_int32 finalcolors, l_int32 debugflag);
		PIX* pixColorSegmentCluster(PIX* pixs, l_int32 maxdist, l_int32 maxcolors, l_int32 debugflag);
		l_ok pixAssignToNearestColor(PIX* pixd, PIX* pixs, PIX* pixm, l_int32 level, l_int32* countarray);
		l_ok pixColorSegmentClean(PIX* pixs, l_int32 selsize, l_int32* countarray);
		l_ok pixColorSegmentRemoveColors(PIX* pixd, PIX* pixs, l_int32 finalcolors);
		PIX* pixConvertRGBToHSV(PIX* pixd, PIX* pixs);
		PIX* pixConvertHSVToRGB(PIX* pixd, PIX* pixs);

		PIX* pixConvertRGBToHue(PIX* pixs);
		PIX* pixConvertRGBToSaturation(PIX* pixs);
		PIX* pixConvertRGBToValue(PIX* pixs);

		PIX* pixMakeRangeMaskHS(PIX* pixs, l_int32 huecenter, l_int32 huehw, l_int32 satcenter, l_int32 sathw, l_int32 regionflag);
		PIX* pixMakeRangeMaskHV(PIX* pixs, l_int32 huecenter, l_int32 huehw, l_int32 valcenter, l_int32 valhw, l_int32 regionflag);
		PIX* pixMakeRangeMaskSV(PIX* pixs, l_int32 satcenter, l_int32 sathw, l_int32 valcenter, l_int32 valhw, l_int32 regionflag);
		PIX* pixMakeHistoHS(PIX* pixs, l_int32 factor, NUMA** pnahue, NUMA** pnasat);
		PIX* pixMakeHistoHV(PIX* pixs, l_int32 factor, NUMA** pnahue, NUMA** pnaval);
		PIX* pixMakeHistoSV(PIX* pixs, l_int32 factor, NUMA** pnasat, NUMA** pnaval);
		l_ok pixFindHistoPeaksHSV(PIX* pixs, l_int32 type, l_int32 width, l_int32 height, l_int32 npeaks, l_float32 erasefactor, PTA** ppta, NUMA** pnatot, PIXA** ppixa);
		PIX* displayHSVColorRange(l_int32 hval, l_int32 sval, l_int32 vval, l_int32 huehw, l_int32 sathw, l_int32 nsamp, l_int32 factor);
		PIX* pixConvertRGBToYUV(PIX* pixd, PIX* pixs);
		PIX* pixConvertYUVToRGB(PIX* pixd, PIX* pixs);

		FPIXA* pixConvertRGBToXYZ(PIX* pixs);
		PIX* fpixaConvertXYZToRGB(FPIXA* fpixa);

		FPIXA* fpixaConvertXYZToLAB(FPIXA* fpixas);
		FPIXA* fpixaConvertLABToXYZ(FPIXA* fpixas);

		FPIXA* pixConvertRGBToLAB(PIX* pixs);
		PIX* fpixaConvertLABToRGB(FPIXA* fpixa);

		PIX* pixMakeGamutRGB(l_int32 scale);
		l_ok pixEqual(PIX* pix1, PIX* pix2, l_int32* psame);
		l_ok pixEqualWithAlpha(PIX* pix1, PIX* pix2, l_int32 use_alpha, l_int32* psame);
		l_ok pixEqualWithCmap(PIX* pix1, PIX* pix2, l_int32* psame);

		l_ok pixUsesCmapColor(PIX* pixs, l_int32* pcolor);
		l_ok pixCorrelationBinary(PIX* pix1, PIX* pix2, l_float32* pval);
		PIX* pixDisplayDiff(PIX* pix1, PIX* pix2, l_int32 showall, l_int32 mindiff, l_uint32 diffcolor);
		PIX* pixDisplayDiffBinary(PIX* pix1, PIX* pix2);
		l_ok pixCompareBinary(PIX* pix1, PIX* pix2, l_int32 comptype, l_float32* pfract, PIX** ppixdiff);
		l_ok pixCompareGrayOrRGB(PIX* pix1, PIX* pix2, l_int32 comptype, l_int32 plottype, l_int32* psame, l_float32* pdiff, l_float32* prmsdiff, PIX** ppixdiff);
		l_ok pixCompareGray(PIX* pix1, PIX* pix2, l_int32 comptype, l_int32 plottype, l_int32* psame, l_float32* pdiff, l_float32* prmsdiff, PIX** ppixdiff);
		l_ok pixCompareRGB(PIX* pix1, PIX* pix2, l_int32 comptype, l_int32 plottype, l_int32* psame, l_float32* pdiff, l_float32* prmsdiff, PIX** ppixdiff);
		l_ok pixCompareTiled(PIX* pix1, PIX* pix2, l_int32 sx, l_int32 sy, l_int32 type, PIX** ppixdiff);
		NUMA* pixCompareRankDifference(PIX* pix1, PIX* pix2, l_int32 factor);
		l_ok pixTestForSimilarity(PIX* pix1, PIX* pix2, l_int32 factor, l_int32 mindiff, l_float32 maxfract, l_float32 maxave, l_int32* psimilar, l_int32 details);
		l_ok pixGetDifferenceStats(PIX* pix1, PIX* pix2, l_int32 factor, l_int32 mindiff, l_float32* pfractdiff, l_float32* pavediff, l_int32 details);
		NUMA* pixGetDifferenceHistogram(PIX* pix1, PIX* pix2, l_int32 factor);
		l_ok pixGetPerceptualDiff(PIX* pixs1, PIX* pixs2, l_int32 sampling, l_int32 dilation, l_int32 mindiff, l_float32* pfract, PIX** ppixdiff1, PIX** ppixdiff2);
		l_ok pixGetPSNR(PIX* pix1, PIX* pix2, l_int32 factor, l_float32* ppsnr);
		l_ok pixaComparePhotoRegionsByHisto(PIXA* pixa, l_float32 minratio, l_float32 textthresh, l_int32 factor, l_int32 n, l_float32 simthresh, NUMA** pnai, l_float32** pscores, PIX** ppixd, l_int32 debug);
		l_ok pixComparePhotoRegionsByHisto(PIX* pix1, PIX* pix2, BOX* box1, BOX* box2, l_float32 minratio, l_int32 factor, l_int32 n, l_float32* pscore, l_int32 debugflag);
		l_ok pixGenPhotoHistos(PIX* pixs, BOX* box, l_int32 factor, l_float32 thresh, l_int32 n, NUMAA** pnaa, l_int32* pw, l_int32* ph, l_int32 debugindex);
		PIX* pixPadToCenterCentroid(PIX* pixs, l_int32 factor);
		l_ok pixCentroid8(PIX* pixs, l_int32 factor, l_float32* pcx, l_float32* pcy);
		l_ok pixDecideIfPhotoImage(PIX* pix, l_int32 factor, l_float32 thresh, l_int32 n, NUMAA** pnaa, PIXA* pixadebug);
		l_ok compareTilesByHisto(NUMAA* naa1, NUMAA* naa2, l_float32 minratio, l_int32 w1, l_int32 h1, l_int32 w2, l_int32 h2, l_float32* pscore, PIXA* pixadebug);
		l_ok pixCompareGrayByHisto(PIX* pix1, PIX* pix2, BOX* box1, BOX* box2, l_float32 minratio, l_int32 maxgray, l_int32 factor, l_int32 n, l_float32* pscore, l_int32 debugflag);
		l_ok pixCropAlignedToCentroid(PIX* pix1, PIX* pix2, l_int32 factor, BOX** pbox1, BOX** pbox2);

		l_ok pixCompareWithTranslation(PIX* pix1, PIX* pix2, l_int32 thresh, l_int32* pdelx, l_int32* pdely, l_float32* pscore, l_int32 debugflag);
		l_ok pixBestCorrelation(PIX* pix1, PIX* pix2, l_int32 area1, l_int32 area2, l_int32 etransx, l_int32 etransy, l_int32 maxshift, l_int32* tab8, l_int32* pdelx, l_int32* pdely, l_float32* pscore, l_int32 debugflag);
		BOXA* pixConnComp(PIX* pixs, PIXA** ppixa, l_int32 connectivity);
		BOXA* pixConnCompPixa(PIX* pixs, PIXA** ppixa, l_int32 connectivity);
		BOXA* pixConnCompBB(PIX* pixs, l_int32 connectivity);
		l_ok pixCountConnComp(PIX* pixs, l_int32 connectivity, l_int32* pcount);
		l_int32 nextOnPixelInRaster(PIX* pixs, l_int32 xstart, l_int32 ystart, l_int32* px, l_int32* py);
		BOX* pixSeedfillBB(PIX* pixs, L_STACK* stack, l_int32 x, l_int32 y, l_int32 connectivity);
		BOX* pixSeedfill4BB(PIX* pixs, L_STACK* stack, l_int32 x, l_int32 y);
		BOX* pixSeedfill8BB(PIX* pixs, L_STACK* stack, l_int32 x, l_int32 y);
		l_ok pixSeedfill(PIX* pixs, L_STACK* stack, l_int32 x, l_int32 y, l_int32 connectivity);
		l_ok pixSeedfill4(PIX* pixs, L_STACK* stack, l_int32 x, l_int32 y);
		l_ok pixSeedfill8(PIX* pixs, L_STACK* stack, l_int32 x, l_int32 y);

		l_ok convertFilesTo1bpp(const char* dirin, const char* substr, l_int32 upscaling, l_int32 thresh, l_int32 firstpage, l_int32 npages, const char* dirout, l_int32 outformat);

		PIX* pixBlockconv(PIX* pix, l_int32 wc, l_int32 hc);
		PIX* pixBlockconvGray(PIX* pixs, PIX* pixacc, l_int32 wc, l_int32 hc);
		PIX* pixBlockconvAccum(PIX* pixs);
		PIX* pixBlockconvGrayUnnormalized(PIX* pixs, l_int32 wc, l_int32 hc);
		PIX* pixBlockconvTiled(PIX* pix, l_int32 wc, l_int32 hc, l_int32 nx, l_int32 ny);
		PIX* pixBlockconvGrayTile(PIX* pixs, PIX* pixacc, l_int32 wc, l_int32 hc);
		l_ok pixWindowedStats(PIX* pixs, l_int32 wc, l_int32 hc, l_int32 hasborder, PIX** ppixm, PIX** ppixms, FPIX** pfpixv, FPIX** pfpixrv);
		PIX* pixWindowedMean(PIX* pixs, l_int32 wc, l_int32 hc, l_int32 hasborder, l_int32 normflag);
		PIX* pixWindowedMeanSquare(PIX* pixs, l_int32 wc, l_int32 hc, l_int32 hasborder);
		l_ok pixWindowedVariance(PIX* pixm, PIX* pixms, FPIX** pfpixv, FPIX** pfpixrv);
		DPIX* pixMeanSquareAccum(PIX* pixs);
		PIX* pixBlockrank(PIX* pixs, PIX* pixacc, l_int32 wc, l_int32 hc, l_float32 rank);
		PIX* pixBlocksum(PIX* pixs, PIX* pixacc, l_int32 wc, l_int32 hc);
		PIX* pixCensusTransform(PIX* pixs, l_int32 halfsize, PIX* pixacc);
		PIX* pixConvolve(PIX* pixs, L_KERNEL* kel, l_int32 outdepth, l_int32 normflag);
		PIX* pixConvolveSep(PIX* pixs, L_KERNEL* kelx, L_KERNEL* kely, l_int32 outdepth, l_int32 normflag);
		PIX* pixConvolveRGB(PIX* pixs, L_KERNEL* kel);
		PIX* pixConvolveRGBSep(PIX* pixs, L_KERNEL* kelx, L_KERNEL* kely);
		FPIX* fpixConvolve(FPIX* fpixs, L_KERNEL* kel, l_int32 normflag);
		FPIX* fpixConvolveSep(FPIX* fpixs, L_KERNEL* kelx, L_KERNEL* kely, l_int32 normflag);
		PIX* pixConvolveWithBias(PIX* pixs, L_KERNEL* kel1, L_KERNEL* kel2, l_int32 force8, l_int32* pbias);

		PIX* pixAddGaussianNoise(PIX* pixs, l_float32 stdev);

		l_ok pixCorrelationScore(PIX* pix1, PIX* pix2, l_int32 area1, l_int32 area2, l_float32 delx, l_float32 dely, l_int32 maxdiffw, l_int32 maxdiffh, l_int32* tab, l_float32* pscore);
		l_int32 pixCorrelationScoreThresholded(PIX* pix1, PIX* pix2, l_int32 area1, l_int32 area2, l_float32 delx, l_float32 dely, l_int32 maxdiffw, l_int32 maxdiffh, l_int32* tab, l_int32* downcount, l_float32 score_threshold);
		l_ok pixCorrelationScoreSimple(PIX* pix1, PIX* pix2, l_int32 area1, l_int32 area2, l_float32 delx, l_float32 dely, l_int32 maxdiffw, l_int32 maxdiffh, l_int32* tab, l_float32* pscore);
		l_ok pixCorrelationScoreShifted(PIX* pix1, PIX* pix2, l_int32 area1, l_int32 area2, l_int32 delx, l_int32 dely, l_int32* tab, l_float32* pscore);
		L_DEWARP* dewarpCreate(PIX* pixs, l_int32 pageno);

		L_DEWARPA* dewarpaCreateFromPixacomp(PIXAC* pixac, l_int32 useboth, l_int32 sampling, l_int32 minlines, l_int32 maxdist);

		PTAA* dewarpGetTextlineCenters(PIX* pixs, l_int32 debugflag);
		PTAA* dewarpRemoveShortLines(PIX* pixs, PTAA* ptaas, l_float32 fract, l_int32 debugflag);
		l_ok dewarpFindHorizSlopeDisparity(L_DEWARP* dew, PIX* pixb, l_float32 fractthresh, l_int32 parity);

		l_ok dewarpaApplyDisparity(L_DEWARPA* dewa, l_int32 pageno, PIX* pixs, l_int32 grayin, l_int32 x, l_int32 y, PIX** ppixd, const char* debugfile);
		l_ok dewarpaApplyDisparityBoxa(L_DEWARPA* dewa, l_int32 pageno, PIX* pixs, BOXA* boxas, l_int32 mapdir, l_int32 x, l_int32 y, BOXA** pboxad, const char* debugfile);

		l_ok dewarpPopulateFullRes(L_DEWARP* dew, PIX* pix, l_int32 x, l_int32 y);
		l_ok dewarpSinglePage(PIX* pixs, l_int32 thresh, l_int32 adaptive, l_int32 useboth, l_int32 check_columns, PIX** ppixd, L_DEWARPA** pdewa, l_int32 debug);
		l_ok dewarpSinglePageInit(PIX* pixs, l_int32 thresh, l_int32 adaptive, l_int32 useboth, l_int32 check_columns, PIX** ppixb, L_DEWARPA** pdewa);
		l_ok dewarpSinglePageRun(PIX* pixs, PIX* pixb, L_DEWARPA* dewa, PIX** ppixd, l_int32 debug);

		L_DNA* pixConvertDataToDna(PIX* pix);

		PIX* pixMorphDwa_2(PIX* pixd, PIX* pixs, l_int32 operation, const char* selname);
		PIX* pixFMorphopGen_2(PIX* pixd, PIX* pixs, l_int32 operation, const char* selname);

		PIX* pixSobelEdgeFilter(PIX* pixs, l_int32 orientflag);
		PIX* pixTwoSidedEdgeFilter(PIX* pixs, l_int32 orientflag);
		l_ok pixMeasureEdgeSmoothness(PIX* pixs, l_int32 side, l_int32 minjump, l_int32 minreversal, l_float32* pjpl, l_float32* pjspl, l_float32* prpl, const char* debugfile);
		NUMA* pixGetEdgeProfile(PIX* pixs, l_int32 side, const char* debugfile);
		l_ok pixGetLastOffPixelInRun(PIX* pixs, l_int32 x, l_int32 y, l_int32 direction, l_int32* ploc);
		l_int32 pixGetLastOnPixelInRun(PIX* pixs, l_int32 x, l_int32 y, l_int32 direction, l_int32* ploc);

		PIX* pixGammaTRC(PIX* pixd, PIX* pixs, l_float32 gamma, l_int32 minval, l_int32 maxval);
		PIX* pixGammaTRCMasked(PIX* pixd, PIX* pixs, PIX* pixm, l_float32 gamma, l_int32 minval, l_int32 maxval);
		PIX* pixGammaTRCWithAlpha(PIX* pixd, PIX* pixs, l_float32 gamma, l_int32 minval, l_int32 maxval);

		PIX* pixContrastTRC(PIX* pixd, PIX* pixs, l_float32 factor);
		PIX* pixContrastTRCMasked(PIX* pixd, PIX* pixs, PIX* pixm, l_float32 factor);

		PIX* pixEqualizeTRC(PIX* pixd, PIX* pixs, l_float32 fract, l_int32 factor);
		NUMA* numaEqualizeTRC(PIX* pix, l_float32 fract, l_int32 factor);
		l_int32 pixTRCMap(PIX* pixs, PIX* pixm, NUMA* na);
		l_int32 pixTRCMapGeneral(PIX* pixs, PIX* pixm, NUMA* nar, NUMA* nag, NUMA* nab);
		PIX* pixUnsharpMasking(PIX* pixs, l_int32 halfwidth, l_float32 fract);
		PIX* pixUnsharpMaskingGray(PIX* pixs, l_int32 halfwidth, l_float32 fract);
		PIX* pixUnsharpMaskingFast(PIX* pixs, l_int32 halfwidth, l_float32 fract, l_int32 direction);
		PIX* pixUnsharpMaskingGrayFast(PIX* pixs, l_int32 halfwidth, l_float32 fract, l_int32 direction);
		PIX* pixUnsharpMaskingGray1D(PIX* pixs, l_int32 halfwidth, l_float32 fract, l_int32 direction);
		PIX* pixUnsharpMaskingGray2D(PIX* pixs, l_int32 halfwidth, l_float32 fract);
		PIX* pixModifyHue(PIX* pixd, PIX* pixs, l_float32 fract);
		PIX* pixModifySaturation(PIX* pixd, PIX* pixs, l_float32 fract);
		l_int32 pixMeasureSaturation(PIX* pixs, l_int32 factor, l_float32* psat);
		PIX* pixModifyBrightness(PIX* pixd, PIX* pixs, l_float32 fract);
		PIX* pixMosaicColorShiftRGB(PIX* pixs, l_float32 roff, l_float32 goff, l_float32 boff, l_float32 delta, l_int32 nincr);
		PIX* pixColorShiftRGB(PIX* pixs, l_float32 rfract, l_float32 gfract, l_float32 bfract);
		PIX* pixDarkenGray(PIX* pixd, PIX* pixs, l_int32 thresh, l_int32 satlimit);
		PIX* pixMultConstantColor(PIX* pixs, l_float32 rfact, l_float32 gfact, l_float32 bfact);
		PIX* pixMultMatrixColor(PIX* pixs, L_KERNEL* kel);
		PIX* pixHalfEdgeByBandpass(PIX* pixs, l_int32 sm1h, l_int32 sm1v, l_int32 sm2h, l_int32 sm2v);

		PIX* pixHMTDwa_1(PIX* pixd, PIX* pixs, const char* selname);
		PIX* pixFHMTGen_1(PIX* pixd, PIX* pixs, const char* selname);

		l_ok pixItalicWords(PIX* pixs, BOXA* boxaw, PIX* pixw, BOXA** pboxa, l_int32 debugflag);
		PIX* pixOrientCorrect(PIX* pixs, l_float32 minupconf, l_float32 minratio, l_float32* pupconf, l_float32* pleftconf, l_int32* protation, l_int32 debug);
		l_ok pixOrientDetect(PIX* pixs, l_float32* pupconf, l_float32* pleftconf, l_int32 mincount, l_int32 debug);

		l_ok pixUpDownDetect(PIX* pixs, l_float32* pconf, l_int32 mincount, l_int32 npixels, l_int32 debug);
		l_ok pixMirrorDetect(PIX* pixs, l_float32* pconf, l_int32 mincount, l_int32 debug);

		PIX* pixMorphDwa_1(PIX* pixd, PIX* pixs, l_int32 operation, const char* selname);
		PIX* pixFMorphopGen_1(PIX* pixd, PIX* pixs, l_int32 operation, const char* selname);

		FPIX* fpixCreate(l_int32 width, l_int32 height);
		FPIX* fpixCreateTemplate(FPIX* fpixs);
		FPIX* fpixClone(FPIX* fpix);
		FPIX* fpixCopy(FPIX* fpixs);
		void fpixDestroy(FPIX** pfpix);
		l_ok fpixGetDimensions(FPIX* fpix, l_int32* pw, l_int32* ph);
		l_ok fpixSetDimensions(FPIX* fpix, l_int32 w, l_int32 h);
		l_int32 fpixGetWpl(FPIX* fpix);
		l_ok fpixSetWpl(FPIX* fpix, l_int32 wpl);
		l_ok fpixGetResolution(FPIX* fpix, l_int32* pxres, l_int32* pyres);
		l_ok fpixSetResolution(FPIX* fpix, l_int32 xres, l_int32 yres);
		l_ok fpixCopyResolution(FPIX* fpixd, FPIX* fpixs);
		l_float32* fpixGetData(FPIX* fpix);
		l_ok fpixSetData(FPIX* fpix, l_float32* data);
		l_ok fpixGetPixel(FPIX* fpix, l_int32 x, l_int32 y, l_float32* pval);
		l_ok fpixSetPixel(FPIX* fpix, l_int32 x, l_int32 y, l_float32 val);
		FPIXA* fpixaCreate(l_int32 n);
		FPIXA* fpixaCopy(FPIXA* fpixa, l_int32 copyflag);
		void fpixaDestroy(FPIXA** pfpixa);
		l_ok fpixaAddFPix(FPIXA* fpixa, FPIX* fpix, l_int32 copyflag);
		l_int32 fpixaGetCount(FPIXA* fpixa);
		FPIX* fpixaGetFPix(FPIXA* fpixa, l_int32 index, l_int32 accesstype);
		l_ok fpixaGetFPixDimensions(FPIXA* fpixa, l_int32 index, l_int32* pw, l_int32* ph);
		l_float32* fpixaGetData(FPIXA* fpixa, l_int32 index);
		l_ok fpixaGetPixel(FPIXA* fpixa, l_int32 index, l_int32 x, l_int32 y, l_float32* pval);
		l_ok fpixaSetPixel(FPIXA* fpixa, l_int32 index, l_int32 x, l_int32 y, l_float32 val);

		DPIX* dpixCreate(l_int32 width, l_int32 height);
		DPIX* dpixCreateTemplate(DPIX* dpixs);
		DPIX* dpixClone(DPIX* dpix);
		DPIX* dpixCopy(DPIX* dpixs);
		void dpixDestroy(DPIX** pdpix);
		l_ok dpixGetDimensions(DPIX* dpix, l_int32* pw, l_int32* ph);
		l_ok dpixSetDimensions(DPIX* dpix, l_int32 w, l_int32 h);
		l_int32 dpixGetWpl(DPIX* dpix);
		l_ok dpixSetWpl(DPIX* dpix, l_int32 wpl);
		l_ok dpixGetResolution(DPIX* dpix, l_int32* pxres, l_int32* pyres);
		l_ok dpixSetResolution(DPIX* dpix, l_int32 xres, l_int32 yres);
		l_ok dpixCopyResolution(DPIX* dpixd, DPIX* dpixs);
		l_float64* dpixGetData(DPIX* dpix);
		l_ok dpixSetData(DPIX* dpix, l_float64* data);
		l_ok dpixGetPixel(DPIX* dpix, l_int32 x, l_int32 y, l_float64* pval);
		l_ok dpixSetPixel(DPIX* dpix, l_int32 x, l_int32 y, l_float64 val);

		FPIX* fpixRead(const char* filename);
		FPIX* fpixReadStream(FILE* fp);
		FPIX* fpixReadMem(const l_uint8* data, size_t size);
		l_ok fpixWrite(const char* filename, FPIX* fpix);
		l_ok fpixWriteStream(FILE* fp, FPIX* fpix);
		l_ok fpixWriteMem(l_uint8** pdata, size_t* psize, FPIX* fpix);
		FPIX* fpixEndianByteSwap(FPIX* fpixd, FPIX* fpixs);

		DPIX* dpixRead(const char* filename);
		DPIX* dpixReadStream(FILE* fp);
		DPIX* dpixReadMem(const l_uint8* data, size_t size);
		l_ok dpixWrite(const char* filename, DPIX* dpix);
		l_ok dpixWriteStream(FILE* fp, DPIX* dpix);
		l_ok dpixWriteMem(l_uint8** pdata, size_t* psize, DPIX* dpix);
		DPIX* dpixEndianByteSwap(DPIX* dpixd, DPIX* dpixs);

		l_ok fpixPrintStream(FILE* fp, FPIX* fpix, l_int32 factor);
		FPIX* pixConvertToFPix(PIX* pixs, l_int32 ncomps);
		DPIX* pixConvertToDPix(PIX* pixs, l_int32 ncomps);
		PIX* fpixConvertToPix(FPIX* fpixs, l_int32 outdepth, l_int32 negvals, l_int32 errorflag);
		PIX* fpixDisplayMaxDynamicRange(FPIX* fpixs);
		DPIX* fpixConvertToDPix(FPIX* fpix);
		PIX* dpixConvertToPix(DPIX* dpixs, l_int32 outdepth, l_int32 negvals, l_int32 errorflag);
		FPIX* dpixConvertToFPix(DPIX* dpix);

		l_ok fpixGetMin(FPIX* fpix, l_float32* pminval, l_int32* pxminloc, l_int32* pyminloc);
		l_ok fpixGetMax(FPIX* fpix, l_float32* pmaxval, l_int32* pxmaxloc, l_int32* pymaxloc);
		l_ok dpixGetMin(DPIX* dpix, l_float64* pminval, l_int32* pxminloc, l_int32* pyminloc);
		l_ok dpixGetMax(DPIX* dpix, l_float64* pmaxval, l_int32* pxmaxloc, l_int32* pymaxloc);

		FPIX* fpixScaleByInteger(FPIX* fpixs, l_int32 factor);
		DPIX* dpixScaleByInteger(DPIX* dpixs, l_int32 factor);
		FPIX* fpixLinearCombination(FPIX* fpixd, FPIX* fpixs1, FPIX* fpixs2, l_float32 a, l_float32 b);
		l_ok fpixAddMultConstant(FPIX* fpix, l_float32 addc, l_float32 multc);
		DPIX* dpixLinearCombination(DPIX* dpixd, DPIX* dpixs1, DPIX* dpixs2, l_float32 a, l_float32 b);
		l_ok dpixAddMultConstant(DPIX* dpix, l_float64 addc, l_float64 multc);
		l_ok fpixSetAllArbitrary(FPIX* fpix, l_float32 inval);
		l_ok dpixSetAllArbitrary(DPIX* dpix, l_float64 inval);
		FPIX* fpixAddBorder(FPIX* fpixs, l_int32 left, l_int32 right, l_int32 top, l_int32 bot);
		FPIX* fpixRemoveBorder(FPIX* fpixs, l_int32 left, l_int32 right, l_int32 top, l_int32 bot);
		FPIX* fpixAddMirroredBorder(FPIX* fpixs, l_int32 left, l_int32 right, l_int32 top, l_int32 bot);
		FPIX* fpixAddContinuedBorder(FPIX* fpixs, l_int32 left, l_int32 right, l_int32 top, l_int32 bot);
		FPIX* fpixAddSlopeBorder(FPIX* fpixs, l_int32 left, l_int32 right, l_int32 top, l_int32 bot);
		l_ok fpixRasterop(FPIX* fpixd, l_int32 dx, l_int32 dy, l_int32 dw, l_int32 dh, FPIX* fpixs, l_int32 sx, l_int32 sy);
		FPIX* fpixRotateOrth(FPIX* fpixs, l_int32 quads);
		FPIX* fpixRotate180(FPIX* fpixd, FPIX* fpixs);
		FPIX* fpixRotate90(FPIX* fpixs, l_int32 direction);
		FPIX* fpixFlipLR(FPIX* fpixd, FPIX* fpixs);
		FPIX* fpixFlipTB(FPIX* fpixd, FPIX* fpixs);
		FPIX* fpixAffinePta(FPIX* fpixs, PTA* ptad, PTA* ptas, l_int32 border, l_float32 inval);
		FPIX* fpixAffine(FPIX* fpixs, l_float32* vc, l_float32 inval);
		FPIX* fpixProjectivePta(FPIX* fpixs, PTA* ptad, PTA* ptas, l_int32 border, l_float32 inval);
		FPIX* fpixProjective(FPIX* fpixs, l_float32* vc, l_float32 inval);

		PIX* fpixThresholdToPix(FPIX* fpix, l_float32 thresh);
		FPIX* pixComponentFunction(PIX* pix, l_float32 rnum, l_float32 gnum, l_float32 bnum, l_float32 rdenom, l_float32 gdenom, l_float32 bdenom);
		PIX* pixReadStreamGif(FILE* fp);
		PIX* pixReadMemGif(const l_uint8* cdata, size_t size);
		l_ok pixWriteStreamGif(FILE* fp, PIX* pix);
		l_ok pixWriteMemGif(l_uint8** pdata, size_t* psize, PIX* pix);

		l_ok pixRenderPlotFromNuma(PIX** ppix, NUMA* na, l_int32 plotloc, l_int32 linewidth, l_int32 max, l_uint32 color);
		l_ok pixRenderPlotFromNumaGen(PIX** ppix, NUMA* na, l_int32 orient, l_int32 linewidth, l_int32 refpos, l_int32 max, l_int32 drawref, l_uint32 color);
		l_ok pixRenderPta(PIX* pix, PTA* pta, l_int32 op);
		l_ok pixRenderPtaArb(PIX* pix, PTA* pta, l_uint8 rval, l_uint8 gval, l_uint8 bval);
		l_ok pixRenderPtaBlend(PIX* pix, PTA* pta, l_uint8 rval, l_uint8 gval, l_uint8 bval, l_float32 fract);
		l_ok pixRenderLine(PIX* pix, l_int32 x1, l_int32 y1, l_int32 x2, l_int32 y2, l_int32 width, l_int32 op);
		l_ok pixRenderLineArb(PIX* pix, l_int32 x1, l_int32 y1, l_int32 x2, l_int32 y2, l_int32 width, l_uint8 rval, l_uint8 gval, l_uint8 bval);
		l_ok pixRenderLineBlend(PIX* pix, l_int32 x1, l_int32 y1, l_int32 x2, l_int32 y2, l_int32 width, l_uint8 rval, l_uint8 gval, l_uint8 bval, l_float32 fract);
		l_ok pixRenderBox(PIX* pix, BOX* box, l_int32 width, l_int32 op);
		l_ok pixRenderBoxArb(PIX* pix, BOX* box, l_int32 width, l_uint8 rval, l_uint8 gval, l_uint8 bval);
		l_ok pixRenderBoxBlend(PIX* pix, BOX* box, l_int32 width, l_uint8 rval, l_uint8 gval, l_uint8 bval, l_float32 fract);
		l_ok pixRenderBoxa(PIX* pix, BOXA* boxa, l_int32 width, l_int32 op);
		l_ok pixRenderBoxaArb(PIX* pix, BOXA* boxa, l_int32 width, l_uint8 rval, l_uint8 gval, l_uint8 bval);
		l_ok pixRenderBoxaBlend(PIX* pix, BOXA* boxa, l_int32 width, l_uint8 rval, l_uint8 gval, l_uint8 bval, l_float32 fract, l_int32 removedups);
		l_ok pixRenderHashBox(PIX* pix, BOX* box, l_int32 spacing, l_int32 width, l_int32 orient, l_int32 outline, l_int32 op);
		l_ok pixRenderHashBoxArb(PIX* pix, BOX* box, l_int32 spacing, l_int32 width, l_int32 orient, l_int32 outline, l_int32 rval, l_int32 gval, l_int32 bval);
		l_ok pixRenderHashBoxBlend(PIX* pix, BOX* box, l_int32 spacing, l_int32 width, l_int32 orient, l_int32 outline, l_int32 rval, l_int32 gval, l_int32 bval, l_float32 fract);
		l_ok pixRenderHashMaskArb(PIX* pix, PIX* pixm, l_int32 x, l_int32 y, l_int32 spacing, l_int32 width, l_int32 orient, l_int32 outline, l_int32 rval, l_int32 gval, l_int32 bval);
		l_ok pixRenderHashBoxa(PIX* pix, BOXA* boxa, l_int32 spacing, l_int32 width, l_int32 orient, l_int32 outline, l_int32 op);
		l_ok pixRenderHashBoxaArb(PIX* pix, BOXA* boxa, l_int32 spacing, l_int32 width, l_int32 orient, l_int32 outline, l_int32 rval, l_int32 gval, l_int32 bval);
		l_ok pixRenderHashBoxaBlend(PIX* pix, BOXA* boxa, l_int32 spacing, l_int32 width, l_int32 orient, l_int32 outline, l_int32 rval, l_int32 gval, l_int32 bval, l_float32 fract);
		l_ok pixRenderPolyline(PIX* pix, PTA* ptas, l_int32 width, l_int32 op, l_int32 closeflag);
		l_ok pixRenderPolylineArb(PIX* pix, PTA* ptas, l_int32 width, l_uint8 rval, l_uint8 gval, l_uint8 bval, l_int32 closeflag);
		l_ok pixRenderPolylineBlend(PIX* pix, PTA* ptas, l_int32 width, l_uint8 rval, l_uint8 gval, l_uint8 bval, l_float32 fract, l_int32 closeflag, l_int32 removedups);
		l_ok pixRenderGridArb(PIX* pix, l_int32 nx, l_int32 ny, l_int32 width, l_uint8 rval, l_uint8 gval, l_uint8 bval);
		PIX* pixRenderRandomCmapPtaa(PIX* pix, PTAA* ptaa, l_int32 polyflag, l_int32 width, l_int32 closeflag);
		PIX* pixRenderPolygon(PTA* ptas, l_int32 width, l_int32* pxmin, l_int32* pymin);
		PIX* pixFillPolygon(PIX* pixs, PTA* pta, l_int32 xmin, l_int32 ymin);
		PIX* pixRenderContours(PIX* pixs, l_int32 startval, l_int32 incr, l_int32 outdepth);
		PIX* fpixAutoRenderContours(FPIX* fpix, l_int32 ncontours);
		PIX* fpixRenderContours(FPIX* fpixs, l_float32 incr, l_float32 proxim);
		PTA* pixGeneratePtaBoundary(PIX* pixs, l_int32 width);
		PIX* pixErodeGray(PIX* pixs, l_int32 hsize, l_int32 vsize);
		PIX* pixDilateGray(PIX* pixs, l_int32 hsize, l_int32 vsize);
		PIX* pixOpenGray(PIX* pixs, l_int32 hsize, l_int32 vsize);
		PIX* pixCloseGray(PIX* pixs, l_int32 hsize, l_int32 vsize);
		PIX* pixErodeGray3(PIX* pixs, l_int32 hsize, l_int32 vsize);
		PIX* pixDilateGray3(PIX* pixs, l_int32 hsize, l_int32 vsize);
		PIX* pixOpenGray3(PIX* pixs, l_int32 hsize, l_int32 vsize);
		PIX* pixCloseGray3(PIX* pixs, l_int32 hsize, l_int32 vsize);
		PIX* pixDitherToBinary(PIX* pixs);
		PIX* pixDitherToBinarySpec(PIX* pixs, l_int32 lowerclip, l_int32 upperclip);

		PIX* pixThresholdToBinary(PIX* pixs, l_int32 thresh);

		PIX* pixVarThresholdToBinary(PIX* pixs, PIX* pixg);
		PIX* pixAdaptThresholdToBinary(PIX* pixs, PIX* pixm, l_float32 gamma);
		PIX* pixAdaptThresholdToBinaryGen(PIX* pixs, PIX* pixm, l_float32 gamma, l_int32 blackval, l_int32 whiteval, l_int32 thresh);
		PIX* pixGenerateMaskByValue(PIX* pixs, l_int32 val, l_int32 usecmap);
		PIX* pixGenerateMaskByBand(PIX* pixs, l_int32 lower, l_int32 upper, l_int32 inband, l_int32 usecmap);
		PIX* pixDitherTo2bpp(PIX* pixs, l_int32 cmapflag);
		PIX* pixDitherTo2bppSpec(PIX* pixs, l_int32 lowerclip, l_int32 upperclip, l_int32 cmapflag);
		PIX* pixThresholdTo2bpp(PIX* pixs, l_int32 nlevels, l_int32 cmapflag);
		PIX* pixThresholdTo4bpp(PIX* pixs, l_int32 nlevels, l_int32 cmapflag);
		PIX* pixThresholdOn8bpp(PIX* pixs, l_int32 nlevels, l_int32 cmapflag);
		PIX* pixThresholdGrayArb(PIX* pixs, const char* edgevals, l_int32 outdepth, l_int32 use_average, l_int32 setblack, l_int32 setwhite);

		PIX* pixGenerateMaskByBand32(PIX* pixs, l_uint32 refval, l_int32 delm, l_int32 delp, l_float32 fractm, l_float32 fractp);
		PIX* pixGenerateMaskByDiscr32(PIX* pixs, l_uint32 refval1, l_uint32 refval2, l_int32 distflag);
		PIX* pixGrayQuantFromHisto(PIX* pixd, PIX* pixs, PIX* pixm, l_float32 minfract, l_int32 maxsize);
		PIX* pixGrayQuantFromCmap(PIX* pixs, PIXCMAP* cmap, l_int32 mindepth);

		l_ok jbAddPage(JBCLASSER* classer, PIX* pixs);
		l_ok jbAddPageComponents(JBCLASSER* classer, PIX* pixs, BOXA* boxas, PIXA* pixas);
		l_ok jbClassifyRankHaus(JBCLASSER* classer, BOXA* boxa, PIXA* pixas);
		l_int32 pixHaustest(PIX* pix1, PIX* pix2, PIX* pix3, PIX* pix4, l_float32 delx, l_float32 dely, l_int32 maxdiffw, l_int32 maxdiffh);
		l_int32 pixRankHaustest(PIX* pix1, PIX* pix2, PIX* pix3, PIX* pix4, l_float32 delx, l_float32 dely, l_int32 maxdiffw, l_int32 maxdiffh, l_int32 area1, l_int32 area3, l_float32 rank, l_int32* tab8);
		l_ok jbClassifyCorrelation(JBCLASSER* classer, BOXA* boxa, PIXA* pixas);
		l_ok jbGetComponents(PIX* pixs, l_int32 components, l_int32 maxwidth, l_int32 maxheight, BOXA** pboxad, PIXA** ppixad);
		l_ok pixWordMaskByDilation(PIX* pixs, PIX** ppixm, l_int32* psize, PIXA* pixadb);
		l_ok pixWordBoxesByDilation(PIX* pixs, l_int32 minwidth, l_int32 minheight, l_int32 maxwidth, l_int32 maxheight, BOXA** pboxa, l_int32* psize, PIXA* pixadb);
		PIXA* jbAccumulateComposites(PIXAA* pixaa, NUMA** pna, PTA** pptat);
		PIXA* jbTemplatesFromComposites(PIXA* pixac, NUMA* na);

		PIXA* jbDataRender(JBDATA* data, l_int32 debugflag);
		l_ok jbGetULCorners(JBCLASSER* classer, PIX* pixs, BOXA* boxa);

		PIX* pixReadJp2k(const char* filename, l_uint32 reduction, BOX* box, l_int32 hint, l_int32 debug);
		PIX* pixReadStreamJp2k(FILE* fp, l_uint32 reduction, BOX* box, l_int32 hint, l_int32 debug);
		l_ok pixWriteJp2k(const char* filename, PIX* pix, l_int32 quality, l_int32 nlevels, l_int32 hint, l_int32 debug);
		l_ok pixWriteStreamJp2k(FILE* fp, PIX* pix, l_int32 quality, l_int32 nlevels, l_int32 codec, l_int32 hint, l_int32 debug);
		PIX* pixReadMemJp2k(const l_uint8* data, size_t size, l_uint32 reduction, BOX* box, l_int32 hint, l_int32 debug);
		l_ok pixWriteMemJp2k(l_uint8** pdata, size_t* psize, PIX* pix, l_int32 quality, l_int32 nlevels, l_int32 hint, l_int32 debug);
		PIX* pixReadJpeg(const char* filename, l_int32 cmflag, l_int32 reduction, l_int32* pnwarn, l_int32 hint);
		PIX* pixReadStreamJpeg(FILE* fp, l_int32 cmflag, l_int32 reduction, l_int32* pnwarn, l_int32 hint);

		l_ok pixWriteJpeg(const char* filename, PIX* pix, l_int32 quality, l_int32 progressive);
		l_ok pixWriteStreamJpeg(FILE* fp, PIX* pix, l_int32 quality, l_int32 progressive);
		PIX* pixReadMemJpeg(const l_uint8* cdata, size_t size, l_int32 cmflag, l_int32 reduction, l_int32* pnwarn, l_int32 hint);

		l_ok pixWriteMemJpeg(l_uint8** pdata, size_t* psize, PIX* pix, l_int32 quality, l_int32 progressive);
		l_ok pixSetChromaSampling(PIX* pix, l_int32 sampling);

		L_KERNEL* kernelCreateFromPix(PIX* pix, l_int32 cy, l_int32 cx);
		PIX* kernelDisplayInPix(L_KERNEL* kel, l_int32 size, l_int32 gthick);

		PIX* generateBinaryMaze(l_int32 w, l_int32 h, l_int32 xi, l_int32 yi, l_float32 wallps, l_float32 ranis);
		PTA* pixSearchBinaryMaze(PIX* pixs, l_int32 xi, l_int32 yi, l_int32 xf, l_int32 yf, PIX** ppixd);
		PTA* pixSearchGrayMaze(PIX* pixs, l_int32 xi, l_int32 yi, l_int32 xf, l_int32 yf, PIX** ppixd);
		PIX* pixDilate(PIX* pixd, PIX* pixs, SEL* sel);
		PIX* pixErode(PIX* pixd, PIX* pixs, SEL* sel);
		PIX* pixHMT(PIX* pixd, PIX* pixs, SEL* sel);
		PIX* pixOpen(PIX* pixd, PIX* pixs, SEL* sel);
		PIX* pixClose(PIX* pixd, PIX* pixs, SEL* sel);
		PIX* pixCloseSafe(PIX* pixd, PIX* pixs, SEL* sel);
		PIX* pixOpenGeneralized(PIX* pixd, PIX* pixs, SEL* sel);
		PIX* pixCloseGeneralized(PIX* pixd, PIX* pixs, SEL* sel);
		PIX* pixDilateBrick(PIX* pixd, PIX* pixs, l_int32 hsize, l_int32 vsize);
		PIX* pixErodeBrick(PIX* pixd, PIX* pixs, l_int32 hsize, l_int32 vsize);
		PIX* pixOpenBrick(PIX* pixd, PIX* pixs, l_int32 hsize, l_int32 vsize);
		PIX* pixCloseBrick(PIX* pixd, PIX* pixs, l_int32 hsize, l_int32 vsize);
		PIX* pixCloseSafeBrick(PIX* pixd, PIX* pixs, l_int32 hsize, l_int32 vsize);

		PIX* pixDilateCompBrick(PIX* pixd, PIX* pixs, l_int32 hsize, l_int32 vsize);
		PIX* pixErodeCompBrick(PIX* pixd, PIX* pixs, l_int32 hsize, l_int32 vsize);
		PIX* pixOpenCompBrick(PIX* pixd, PIX* pixs, l_int32 hsize, l_int32 vsize);
		PIX* pixCloseCompBrick(PIX* pixd, PIX* pixs, l_int32 hsize, l_int32 vsize);
		PIX* pixCloseSafeCompBrick(PIX* pixd, PIX* pixs, l_int32 hsize, l_int32 vsize);

		PIX* pixExtractBoundary(PIX* pixs, l_int32 type);
		PIX* pixMorphSequenceMasked(PIX* pixs, PIX* pixm, const char* sequence, l_int32 dispsep);
		PIX* pixMorphSequenceByComponent(PIX* pixs, const char* sequence, l_int32 connectivity, l_int32 minw, l_int32 minh, BOXA** pboxa);
		PIXA* pixaMorphSequenceByComponent(PIXA* pixas, const char* sequence, l_int32 minw, l_int32 minh);
		PIX* pixMorphSequenceByRegion(PIX* pixs, PIX* pixm, const char* sequence, l_int32 connectivity, l_int32 minw, l_int32 minh, BOXA** pboxa);
		PIXA* pixaMorphSequenceByRegion(PIX* pixs, PIXA* pixam, const char* sequence, l_int32 minw, l_int32 minh);
		PIX* pixUnionOfMorphOps(PIX* pixs, SELA* sela, l_int32 type);
		PIX* pixIntersectionOfMorphOps(PIX* pixs, SELA* sela, l_int32 type);
		PIX* pixSelectiveConnCompFill(PIX* pixs, l_int32 connectivity, l_int32 minw, l_int32 minh);
		l_ok pixRemoveMatchedPattern(PIX* pixs, PIX* pixp, PIX* pixe, l_int32 x0, l_int32 y0, l_int32 dsize);
		PIX* pixDisplayMatchedPattern(PIX* pixs, PIX* pixp, PIX* pixe, l_int32 x0, l_int32 y0, l_uint32 color, l_float32 scale, l_int32 nlevels);
		PIXA* pixaExtendByMorph(PIXA* pixas, l_int32 type, l_int32 niters, SEL* sel, l_int32 include);
		PIXA* pixaExtendByScaling(PIXA* pixas, NUMA* nasc, l_int32 type, l_int32 include);
		PIX* pixSeedfillMorph(PIX* pixs, PIX* pixm, l_int32 maxiters, l_int32 connectivity);
		NUMA* pixRunHistogramMorph(PIX* pixs, l_int32 runtype, l_int32 direction, l_int32 maxsize);
		PIX* pixTophat(PIX* pixs, l_int32 hsize, l_int32 vsize, l_int32 type);
		PIX* pixHDome(PIX* pixs, l_int32 height, l_int32 connectivity);
		PIX* pixFastTophat(PIX* pixs, l_int32 xsize, l_int32 ysize, l_int32 type);
		PIX* pixMorphGradient(PIX* pixs, l_int32 hsize, l_int32 vsize, l_int32 smoothing);
		PTA* pixaCentroids(PIXA* pixa);
		l_ok pixCentroid(PIX* pix, l_int32* centtab, l_int32* sumtab, l_float32* pxave, l_float32* pyave);
		PIX* pixDilateBrickDwa(PIX* pixd, PIX* pixs, l_int32 hsize, l_int32 vsize);
		PIX* pixErodeBrickDwa(PIX* pixd, PIX* pixs, l_int32 hsize, l_int32 vsize);
		PIX* pixOpenBrickDwa(PIX* pixd, PIX* pixs, l_int32 hsize, l_int32 vsize);
		PIX* pixCloseBrickDwa(PIX* pixd, PIX* pixs, l_int32 hsize, l_int32 vsize);
		PIX* pixDilateCompBrickDwa(PIX* pixd, PIX* pixs, l_int32 hsize, l_int32 vsize);
		PIX* pixErodeCompBrickDwa(PIX* pixd, PIX* pixs, l_int32 hsize, l_int32 vsize);
		PIX* pixOpenCompBrickDwa(PIX* pixd, PIX* pixs, l_int32 hsize, l_int32 vsize);
		PIX* pixCloseCompBrickDwa(PIX* pixd, PIX* pixs, l_int32 hsize, l_int32 vsize);
		PIX* pixDilateCompBrickExtendDwa(PIX* pixd, PIX* pixs, l_int32 hsize, l_int32 vsize);
		PIX* pixErodeCompBrickExtendDwa(PIX* pixd, PIX* pixs, l_int32 hsize, l_int32 vsize);
		PIX* pixOpenCompBrickExtendDwa(PIX* pixd, PIX* pixs, l_int32 hsize, l_int32 vsize);
		PIX* pixCloseCompBrickExtendDwa(PIX* pixd, PIX* pixs, l_int32 hsize, l_int32 vsize);

		PIX* pixMorphSequence(PIX* pixs, const char* sequence, l_int32 dispsep);
		PIX* pixMorphCompSequence(PIX* pixs, const char* sequence, l_int32 dispsep);
		PIX* pixMorphSequenceDwa(PIX* pixs, const char* sequence, l_int32 dispsep);
		PIX* pixMorphCompSequenceDwa(PIX* pixs, const char* sequence, l_int32 dispsep);

		PIX* pixGrayMorphSequence(PIX* pixs, const char* sequence, l_int32 dispsep, l_int32 dispy);
		PIX* pixColorMorphSequence(PIX* pixs, const char* sequence, l_int32 dispsep, l_int32 dispy);

		l_ok pixGetRegionsBinary(PIX* pixs, PIX** ppixhm, PIX** ppixtm, PIX** ppixtb, PIXA* pixadb);
		PIX* pixGenHalftoneMask(PIX* pixs, PIX** ppixtext, l_int32* phtfound, l_int32 debug);
		PIX* pixGenerateHalftoneMask(PIX* pixs, PIX** ppixtext, l_int32* phtfound, PIXA* pixadb);
		PIX* pixGenTextlineMask(PIX* pixs, PIX** ppixvws, l_int32* ptlfound, PIXA* pixadb);
		PIX* pixGenTextblockMask(PIX* pixs, PIX* pixvws, PIXA* pixadb);
		PIX* pixCropImage(PIX* pixs, l_int32 lr_clear, l_int32 tb_clear, l_int32 edgeclean, l_int32 lr_add, l_int32 tb_add, l_float32 maxwiden, const char* debugfile, BOX** pcropbox);
		PIX* pixCleanImage(PIX* pixs, l_int32 contrast, l_int32 rotation, l_int32 scale, l_int32 opensize);
		BOX* pixFindPageForeground(PIX* pixs, l_int32 threshold, l_int32 mindist, l_int32 erasedist, l_int32 showmorph, PIXAC* pixac);
		l_ok pixSplitIntoCharacters(PIX* pixs, l_int32 minw, l_int32 minh, BOXA** pboxa, PIXA** ppixa, PIX** ppixdebug);
		BOXA* pixSplitComponentWithProfile(PIX* pixs, l_int32 delta, l_int32 mindel, PIX** ppixdebug);
		PIXA* pixExtractTextlines(PIX* pixs, l_int32 maxw, l_int32 maxh, l_int32 minw, l_int32 minh, l_int32 adjw, l_int32 adjh, PIXA* pixadb);
		PIXA* pixExtractRawTextlines(PIX* pixs, l_int32 maxw, l_int32 maxh, l_int32 adjw, l_int32 adjh, PIXA* pixadb);
		l_ok pixCountTextColumns(PIX* pixs, l_float32 deltafract, l_float32 peakfract, l_float32 clipfract, l_int32* pncols, PIXA* pixadb);
		l_ok pixDecideIfText(PIX* pixs, BOX* box, l_int32* pistext, PIXA* pixadb);
		l_ok pixFindThreshFgExtent(PIX* pixs, l_int32 thresh, l_int32* ptop, l_int32* pbot);
		l_ok pixDecideIfTable(PIX* pixs, BOX* box, l_int32 orient, l_int32* pscore, PIXA* pixadb);
		PIX* pixPrepare1bpp(PIX* pixs, BOX* box, l_float32 cropfract, l_int32 outres);
		l_ok pixEstimateBackground(PIX* pixs, l_int32 darkthresh, l_float32 edgecrop, l_int32* pbg);
		l_ok pixFindLargeRectangles(PIX* pixs, l_int32 polarity, l_int32 nrect, BOXA** pboxa, PIX** ppixdb);
		l_ok pixFindLargestRectangle(PIX* pixs, l_int32 polarity, BOX** pbox, PIX** ppixdb);
		BOX* pixFindRectangleInCC(PIX* pixs, BOX* boxs, l_float32 fract, l_int32 dir, l_int32 select, l_int32 debug);
		PIX* pixAutoPhotoinvert(PIX* pixs, l_int32 thresh, PIX** ppixm, PIXA* pixadb);
		l_ok pixSetSelectCmap(PIX* pixs, BOX* box, l_int32 sindex, l_int32 rval, l_int32 gval, l_int32 bval);
		l_ok pixColorGrayRegionsCmap(PIX* pixs, BOXA* boxa, l_int32 type, l_int32 rval, l_int32 gval, l_int32 bval);
		l_ok pixColorGrayCmap(PIX* pixs, BOX* box, l_int32 type, l_int32 rval, l_int32 gval, l_int32 bval);
		l_ok pixColorGrayMaskedCmap(PIX* pixs, PIX* pixm, l_int32 type, l_int32 rval, l_int32 gval, l_int32 bval);
		l_ok addColorizedGrayToCmap(PIXCMAP* cmap, l_int32 type, l_int32 rval, l_int32 gval, l_int32 bval, NUMA** pna);
		l_ok pixSetSelectMaskedCmap(PIX* pixs, PIX* pixm, l_int32 x, l_int32 y, l_int32 sindex, l_int32 rval, l_int32 gval, l_int32 bval);
		l_ok pixSetMaskedCmap(PIX* pixs, PIX* pixm, l_int32 x, l_int32 y, l_int32 rval, l_int32 gval, l_int32 bval);

		l_ok partifyPixac(PIXAC* pixac, l_int32 nparts, const char* outroot, PIXA* pixadb);

		l_ok selectDefaultPdfEncoding(PIX* pix, l_int32* ptype);

		l_ok pixaConvertToPdf(PIXA* pixa, l_int32 res, l_float32 scalefactor, l_int32 type, l_int32 quality, const char* title, const char* fileout);
		l_ok pixaConvertToPdfData(PIXA* pixa, l_int32 res, l_float32 scalefactor, l_int32 type, l_int32 quality, const char* title, l_uint8** pdata, size_t* pnbytes);

		l_ok pixConvertToPdf(PIX* pix, l_int32 type, l_int32 quality, const char* fileout, l_int32 x, l_int32 y, l_int32 res, const char* title, L_PDF_DATA** plpd, l_int32 position);
		l_ok pixWriteStreamPdf(FILE* fp, PIX* pix, l_int32 res, const char* title);
		l_ok pixWriteMemPdf(l_uint8** pdata, size_t* pnbytes, PIX* pix, l_int32 res, const char* title);

		l_ok pixConvertToPdfSegmented(PIX* pixs, l_int32 res, l_int32 type, l_int32 thresh, BOXA* boxa, l_int32 quality, l_float32 scalefactor, const char* title, const char* fileout);

		l_ok pixConvertToPdfDataSegmented(PIX* pixs, l_int32 res, l_int32 type, l_int32 thresh, BOXA* boxa, l_int32 quality, l_float32 scalefactor, const char* title, l_uint8** pdata, size_t* pnbytes);

		l_ok pixConvertToPdfData(PIX* pix, l_int32 type, l_int32 quality, l_uint8** pdata, size_t* pnbytes, l_int32 x, l_int32 y, l_int32 res, const char* title, L_PDF_DATA** plpd, l_int32 position);

		l_ok l_generateCIDataForPdf(const char* fname, PIX* pix, l_int32 quality, L_COMP_DATA** pcid);
		L_COMP_DATA* l_generateFlateDataPdf(const char* fname, PIX* pix);

		l_ok pixGenerateCIData(PIX* pixs, l_int32 type, l_int32 quality, l_int32 ascii85, L_COMP_DATA** pcid);

		PIX* pixCreate(l_int32 width, l_int32 height, l_int32 depth);
		PIX* pixCreateNoInit(l_int32 width, l_int32 height, l_int32 depth);
		PIX* pixCreateTemplate(const PIX* pixs);
		PIX* pixCreateTemplateNoInit(const PIX* pixs);
		PIX* pixCreateWithCmap(l_int32 width, l_int32 height, l_int32 depth, l_int32 initcolor);
		PIX* pixCreateHeader(l_int32 width, l_int32 height, l_int32 depth);
		PIX* pixClone(PIX* pixs);
		void pixDestroy(PIX** ppix);
		PIX* pixCopy(PIX* pixd, const PIX* pixs);
		l_ok pixResizeImageData(PIX* pixd, const PIX* pixs);
		l_ok pixCopyColormap(PIX* pixd, const PIX* pixs);
		l_ok pixTransferAllData(PIX* pixd, PIX** ppixs, l_int32 copytext, l_int32 copyformat);
		l_ok pixSwapAndDestroy(PIX** ppixd, PIX** ppixs);
		l_int32 pixGetWidth(const PIX* pix);
		l_int32 pixSetWidth(PIX* pix, l_int32 width);
		l_int32 pixGetHeight(const PIX* pix);
		l_int32 pixSetHeight(PIX* pix, l_int32 height);
		l_int32 pixGetDepth(const PIX* pix);
		l_int32 pixSetDepth(PIX* pix, l_int32 depth);
		l_ok pixGetDimensions(const PIX* pix, l_int32* pw, l_int32* ph, l_int32* pd);
		l_ok pixSetDimensions(PIX* pix, l_int32 w, l_int32 h, l_int32 d);
		l_ok pixCopyDimensions(PIX* pixd, const PIX* pixs);
		l_int32 pixGetSpp(const PIX* pix);
		l_int32 pixSetSpp(PIX* pix, l_int32 spp);
		l_ok pixCopySpp(PIX* pixd, const PIX* pixs);
		l_int32 pixGetWpl(const PIX* pix);
		l_int32 pixSetWpl(PIX* pix, l_int32 wpl);
		l_int32 pixGetXRes(const PIX* pix);
		l_int32 pixSetXRes(PIX* pix, l_int32 res);
		l_int32 pixGetYRes(const PIX* pix);
		l_int32 pixSetYRes(PIX* pix, l_int32 res);
		l_ok pixGetResolution(const PIX* pix, l_int32* pxres, l_int32* pyres);
		l_ok pixSetResolution(PIX* pix, l_int32 xres, l_int32 yres);
		l_int32 pixCopyResolution(PIX* pixd, const PIX* pixs);
		l_int32 pixScaleResolution(PIX* pix, l_float32 xscale, l_float32 yscale);
		l_int32 pixGetInputFormat(const PIX* pix);
		l_int32 pixSetInputFormat(PIX* pix, l_int32 informat);
		l_int32 pixCopyInputFormat(PIX* pixd, const PIX* pixs);
		l_int32 pixSetSpecial(PIX* pix, l_int32 special);
		char* pixGetText(PIX* pix);
		l_ok pixSetText(PIX* pix, const char* textstring);
		l_ok pixAddText(PIX* pix, const char* textstring);
		l_int32 pixCopyText(PIX* pixd, const PIX* pixs);
		l_uint8* pixGetTextCompNew(PIX* pix, size_t* psize);
		l_ok pixSetTextCompNew(PIX* pix, const l_uint8* data, size_t size);
		PIXCMAP* pixGetColormap(PIX* pix);
		l_ok pixSetColormap(PIX* pix, PIXCMAP* colormap);
		l_ok pixDestroyColormap(PIX* pix);
		l_uint32* pixGetData(PIX* pix);
		l_int32 pixFreeAndSetData(PIX* pix, l_uint32* data);
		l_int32 pixSetData(PIX* pix, l_uint32* data);
		l_int32 pixFreeData(PIX* pix);
		l_uint32* pixExtractData(PIX* pixs);
		void** pixGetLinePtrs(PIX* pix, l_int32* psize);
		l_int32 pixSizesEqual(const PIX* pix1, const PIX* pix2);
		l_ok pixMaxAspectRatio(PIX* pixs, l_float32* pratio);
		l_ok pixPrintStreamInfo(FILE* fp, const PIX* pix, const char* text);
		l_ok pixGetPixel(PIX* pix, l_int32 x, l_int32 y, l_uint32* pval);
		l_ok pixSetPixel(PIX* pix, l_int32 x, l_int32 y, l_uint32 val);
		l_ok pixGetRGBPixel(PIX* pix, l_int32 x, l_int32 y, l_int32* prval, l_int32* pgval, l_int32* pbval);
		l_ok pixSetRGBPixel(PIX* pix, l_int32 x, l_int32 y, l_int32 rval, l_int32 gval, l_int32 bval);
		l_ok pixSetCmapPixel(PIX* pix, l_int32 x, l_int32 y, l_int32 rval, l_int32 gval, l_int32 bval);
		l_ok pixGetRandomPixel(PIX* pix, l_uint32* pval, l_int32* px, l_int32* py);
		l_ok pixClearPixel(PIX* pix, l_int32 x, l_int32 y);
		l_ok pixFlipPixel(PIX* pix, l_int32 x, l_int32 y);
		void setPixelLow(l_uint32* line, l_int32 x, l_int32 depth, l_uint32 val);
		l_ok pixGetBlackOrWhiteVal(PIX* pixs, l_int32 op, l_uint32* pval);
		l_ok pixClearAll(PIX* pix);
		l_ok pixSetAll(PIX* pix);
		l_ok pixSetAllGray(PIX* pix, l_int32 grayval);
		l_ok pixSetAllArbitrary(PIX* pix, l_uint32 val);
		l_ok pixSetBlackOrWhite(PIX* pixs, l_int32 op);
		l_ok pixSetComponentArbitrary(PIX* pix, l_int32 comp, l_int32 val);
		l_ok pixClearInRect(PIX* pix, BOX* box);
		l_ok pixSetInRect(PIX* pix, BOX* box);
		l_ok pixSetInRectArbitrary(PIX* pix, BOX* box, l_uint32 val);
		l_ok pixBlendInRect(PIX* pixs, BOX* box, l_uint32 val, l_float32 fract);
		l_ok pixSetPadBits(PIX* pix, l_int32 val);
		l_ok pixSetPadBitsBand(PIX* pix, l_int32 by, l_int32 bh, l_int32 val);
		l_ok pixSetOrClearBorder(PIX* pixs, l_int32 left, l_int32 right, l_int32 top, l_int32 bot, l_int32 op);
		l_ok pixSetBorderVal(PIX* pixs, l_int32 left, l_int32 right, l_int32 top, l_int32 bot, l_uint32 val);
		l_ok pixSetBorderRingVal(PIX* pixs, l_int32 dist, l_uint32 val);
		l_ok pixSetMirroredBorder(PIX* pixs, l_int32 left, l_int32 right, l_int32 top, l_int32 bot);
		PIX* pixCopyBorder(PIX* pixd, PIX* pixs, l_int32 left, l_int32 right, l_int32 top, l_int32 bot);
		PIX* pixAddBorder(PIX* pixs, l_int32 npix, l_uint32 val);
		PIX* pixAddBlackOrWhiteBorder(PIX* pixs, l_int32 left, l_int32 right, l_int32 top, l_int32 bot, l_int32 op);
		PIX* pixAddBorderGeneral(PIX* pixs, l_int32 left, l_int32 right, l_int32 top, l_int32 bot, l_uint32 val);
		PIX* pixAddMultipleBlackWhiteBorders(PIX* pixs, l_int32 nblack1, l_int32 nwhite1, l_int32 nblack2, l_int32 nwhite2, l_int32 nblack3, l_int32 nwhite3);
		PIX* pixRemoveBorder(PIX* pixs, l_int32 npix);
		PIX* pixRemoveBorderGeneral(PIX* pixs, l_int32 left, l_int32 right, l_int32 top, l_int32 bot);
		PIX* pixRemoveBorderToSize(PIX* pixs, l_int32 wd, l_int32 hd);
		PIX* pixAddMirroredBorder(PIX* pixs, l_int32 left, l_int32 right, l_int32 top, l_int32 bot);
		PIX* pixAddRepeatedBorder(PIX* pixs, l_int32 left, l_int32 right, l_int32 top, l_int32 bot);
		PIX* pixAddMixedBorder(PIX* pixs, l_int32 left, l_int32 right, l_int32 top, l_int32 bot);
		PIX* pixAddContinuedBorder(PIX* pixs, l_int32 left, l_int32 right, l_int32 top, l_int32 bot);
		l_ok pixShiftAndTransferAlpha(PIX* pixd, PIX* pixs, l_float32 shiftx, l_float32 shifty);
		PIX* pixDisplayLayersRGBA(PIX* pixs, l_uint32 val, l_int32 maxw);
		PIX* pixCreateRGBImage(PIX* pixr, PIX* pixg, PIX* pixb);
		PIX* pixGetRGBComponent(PIX* pixs, l_int32 comp);
		l_ok pixSetRGBComponent(PIX* pixd, PIX* pixs, l_int32 comp);
		PIX* pixGetRGBComponentCmap(PIX* pixs, l_int32 comp);
		l_ok pixCopyRGBComponent(PIX* pixd, PIX* pixs, l_int32 comp);

		l_ok pixGetRGBLine(PIX* pixs, l_int32 row, l_uint8* bufr, l_uint8* bufg, l_uint8* bufb);

		PIX* pixEndianByteSwapNew(PIX* pixs);
		l_ok pixEndianByteSwap(PIX* pixs);

		PIX* pixEndianTwoByteSwapNew(PIX* pixs);
		l_ok pixEndianTwoByteSwap(PIX* pixs);
		l_ok pixGetRasterData(PIX* pixs, l_uint8** pdata, size_t* pnbytes);
		l_ok pixInferResolution(PIX* pix, l_float32 longside, l_int32* pres);
		l_ok pixAlphaIsOpaque(PIX* pix, l_int32* popaque);
		l_uint8** pixSetupByteProcessing(PIX* pix, l_int32* pw, l_int32* ph);
		l_ok pixCleanupByteProcessing(PIX* pix, l_uint8** lineptrs);

		l_ok pixSetMasked(PIX* pixd, PIX* pixm, l_uint32 val);
		l_ok pixSetMaskedGeneral(PIX* pixd, PIX* pixm, l_uint32 val, l_int32 x, l_int32 y);
		l_ok pixCombineMasked(PIX* pixd, PIX* pixs, PIX* pixm);
		l_ok pixCombineMaskedGeneral(PIX* pixd, PIX* pixs, PIX* pixm, l_int32 x, l_int32 y);
		l_ok pixPaintThroughMask(PIX* pixd, PIX* pixm, l_int32 x, l_int32 y, l_uint32 val);
		PIX* pixCopyWithBoxa(PIX* pixs, BOXA* boxa, l_int32 background);
		l_ok pixPaintSelfThroughMask(PIX* pixd, PIX* pixm, l_int32 x, l_int32 y, l_int32 searchdir, l_int32 mindist, l_int32 tilesize, l_int32 ntiles, l_int32 distblend);
		PIX* pixMakeMaskFromVal(PIX* pixs, l_int32 val);
		PIX* pixMakeMaskFromLUT(PIX* pixs, l_int32* tab);
		PIX* pixMakeArbMaskFromRGB(PIX* pixs, l_float32 rc, l_float32 gc, l_float32 bc, l_float32 thresh);
		PIX* pixSetUnderTransparency(PIX* pixs, l_uint32 val, l_int32 debug);
		PIX* pixMakeAlphaFromMask(PIX* pixs, l_int32 dist, BOX** pbox);
		l_ok pixGetColorNearMaskBoundary(PIX* pixs, PIX* pixm, BOX* box, l_int32 dist, l_uint32* pval, l_int32 debug);
		PIX* pixDisplaySelectedPixels(PIX* pixs, PIX* pixm, SEL* sel, l_uint32 val);
		PIX* pixInvert(PIX* pixd, PIX* pixs);
		PIX* pixOr(PIX* pixd, PIX* pixs1, PIX* pixs2);
		PIX* pixAnd(PIX* pixd, PIX* pixs1, PIX* pixs2);
		PIX* pixXor(PIX* pixd, PIX* pixs1, PIX* pixs2);
		PIX* pixSubtract(PIX* pixd, PIX* pixs1, PIX* pixs2);
		l_ok pixZero(PIX* pix, l_int32* pempty);
		l_ok pixForegroundFraction(PIX* pix, l_float32* pfract);
		NUMA* pixaCountPixels(PIXA* pixa);
		l_ok pixCountPixels(PIX* pixs, l_int32* pcount, l_int32* tab8);
		l_ok pixCountPixelsInRect(PIX* pixs, BOX* box, l_int32* pcount, l_int32* tab8);
		NUMA* pixCountByRow(PIX* pix, BOX* box);
		NUMA* pixCountByColumn(PIX* pix, BOX* box);
		NUMA* pixCountPixelsByRow(PIX* pix, l_int32* tab8);
		NUMA* pixCountPixelsByColumn(PIX* pix);
		l_ok pixCountPixelsInRow(PIX* pix, l_int32 row, l_int32* pcount, l_int32* tab8);
		NUMA* pixGetMomentByColumn(PIX* pix, l_int32 order);
		l_ok pixThresholdPixelSum(PIX* pix, l_int32 thresh, l_int32* pabove, l_int32* tab8);

		NUMA* pixAverageByRow(PIX* pix, BOX* box, l_int32 type);
		NUMA* pixAverageByColumn(PIX* pix, BOX* box, l_int32 type);
		l_ok pixAverageInRect(PIX* pixs, PIX* pixm, BOX* box, l_int32 minval, l_int32 maxval, l_int32 subsamp, l_float32* pave);
		l_ok pixAverageInRectRGB(PIX* pixs, PIX* pixm, BOX* box, l_int32 subsamp, l_uint32* pave);
		NUMA* pixVarianceByRow(PIX* pix, BOX* box);
		NUMA* pixVarianceByColumn(PIX* pix, BOX* box);
		l_ok pixVarianceInRect(PIX* pix, BOX* box, l_float32* prootvar);
		NUMA* pixAbsDiffByRow(PIX* pix, BOX* box);
		NUMA* pixAbsDiffByColumn(PIX* pix, BOX* box);
		l_ok pixAbsDiffInRect(PIX* pix, BOX* box, l_int32 dir, l_float32* pabsdiff);
		l_ok pixAbsDiffOnLine(PIX* pix, l_int32 x1, l_int32 y1, l_int32 x2, l_int32 y2, l_float32* pabsdiff);
		l_int32 pixCountArbInRect(PIX* pixs, BOX* box, l_int32 val, l_int32 factor, l_int32* pcount);
		PIX* pixMirroredTiling(PIX* pixs, l_int32 w, l_int32 h);
		l_ok pixFindRepCloseTile(PIX* pixs, BOX* box, l_int32 searchdir, l_int32 mindist, l_int32 tsize, l_int32 ntiles, BOX** pboxtile, l_int32 debug);
		NUMA* pixGetGrayHistogram(PIX* pixs, l_int32 factor);
		NUMA* pixGetGrayHistogramMasked(PIX* pixs, PIX* pixm, l_int32 x, l_int32 y, l_int32 factor);
		NUMA* pixGetGrayHistogramInRect(PIX* pixs, BOX* box, l_int32 factor);
		NUMAA* pixGetGrayHistogramTiled(PIX* pixs, l_int32 factor, l_int32 nx, l_int32 ny);
		l_ok pixGetColorHistogram(PIX* pixs, l_int32 factor, NUMA** pnar, NUMA** pnag, NUMA** pnab);
		l_ok pixGetColorHistogramMasked(PIX* pixs, PIX* pixm, l_int32 x, l_int32 y, l_int32 factor, NUMA** pnar, NUMA** pnag, NUMA** pnab);
		NUMA* pixGetCmapHistogram(PIX* pixs, l_int32 factor);
		NUMA* pixGetCmapHistogramMasked(PIX* pixs, PIX* pixm, l_int32 x, l_int32 y, l_int32 factor);
		NUMA* pixGetCmapHistogramInRect(PIX* pixs, BOX* box, l_int32 factor);
		l_ok pixCountRGBColorsByHash(PIX* pixs, l_int32* pncolors);
		l_ok pixCountRGBColors(PIX* pixs, l_int32 factor, l_int32* pncolors);
		L_AMAP* pixGetColorAmapHistogram(PIX* pixs, l_int32 factor);
		l_int32 amapGetCountForColor(L_AMAP* amap, l_uint32 val);
		l_ok pixGetRankValue(PIX* pixs, l_int32 factor, l_float32 rank, l_uint32* pvalue);
		l_ok pixGetRankValueMaskedRGB(PIX* pixs, PIX* pixm, l_int32 x, l_int32 y, l_int32 factor, l_float32 rank, l_float32* prval, l_float32* pgval, l_float32* pbval);
		l_ok pixGetRankValueMasked(PIX* pixs, PIX* pixm, l_int32 x, l_int32 y, l_int32 factor, l_float32 rank, l_float32* pval, NUMA** pna);
		l_ok pixGetPixelAverage(PIX* pixs, PIX* pixm, l_int32 x, l_int32 y, l_int32 factor, l_uint32* pval);
		l_ok pixGetPixelStats(PIX* pixs, l_int32 factor, l_int32 type, l_uint32* pvalue);
		l_ok pixGetAverageMaskedRGB(PIX* pixs, PIX* pixm, l_int32 x, l_int32 y, l_int32 factor, l_int32 type, l_float32* prval, l_float32* pgval, l_float32* pbval);
		l_ok pixGetAverageMasked(PIX* pixs, PIX* pixm, l_int32 x, l_int32 y, l_int32 factor, l_int32 type, l_float32* pval);
		l_ok pixGetAverageTiledRGB(PIX* pixs, l_int32 sx, l_int32 sy, l_int32 type, PIX** ppixr, PIX** ppixg, PIX** ppixb);
		PIX* pixGetAverageTiled(PIX* pixs, l_int32 sx, l_int32 sy, l_int32 type);
		l_int32 pixRowStats(PIX* pixs, BOX* box, NUMA** pnamean, NUMA** pnamedian, NUMA** pnamode, NUMA** pnamodecount, NUMA** pnavar, NUMA** pnarootvar);
		l_int32 pixColumnStats(PIX* pixs, BOX* box, NUMA** pnamean, NUMA** pnamedian, NUMA** pnamode, NUMA** pnamodecount, NUMA** pnavar, NUMA** pnarootvar);
		l_ok pixGetRangeValues(PIX* pixs, l_int32 factor, l_int32 color, l_int32* pminval, l_int32* pmaxval);
		l_ok pixGetExtremeValue(PIX* pixs, l_int32 factor, l_int32 type, l_int32* prval, l_int32* pgval, l_int32* pbval, l_int32* pgrayval);
		l_ok pixGetMaxValueInRect(PIX* pixs, BOX* box, l_uint32* pmaxval, l_int32* pxmax, l_int32* pymax);
		l_ok pixGetMaxColorIndex(PIX* pixs, l_int32* pmaxindex);
		l_ok pixGetBinnedComponentRange(PIX* pixs, l_int32 nbins, l_int32 factor, l_int32 color, l_int32* pminval, l_int32* pmaxval, l_uint32** pcarray, l_int32 fontsize);
		l_ok pixGetRankColorArray(PIX* pixs, l_int32 nbins, l_int32 type, l_int32 factor, l_uint32** pcarray, PIXA* pixadb, l_int32 fontsize);
		l_ok pixGetBinnedColor(PIX* pixs, PIX* pixg, l_int32 factor, l_int32 nbins, l_uint32** pcarray, PIXA* pixadb);
		PIX* pixDisplayColorArray(l_uint32* carray, l_int32 ncolors, l_int32 side, l_int32 ncols, l_int32 fontsize);
		PIX* pixRankBinByStrip(PIX* pixs, l_int32 direction, l_int32 size, l_int32 nbins, l_int32 type);
		PIX* pixaGetAlignedStats(PIXA* pixa, l_int32 type, l_int32 nbins, l_int32 thresh);
		l_ok pixaExtractColumnFromEachPix(PIXA* pixa, l_int32 col, PIX* pixd);
		l_ok pixGetRowStats(PIX* pixs, l_int32 type, l_int32 nbins, l_int32 thresh, l_float32* colvect);
		l_ok pixGetColumnStats(PIX* pixs, l_int32 type, l_int32 nbins, l_int32 thresh, l_float32* rowvect);
		l_ok pixSetPixelColumn(PIX* pix, l_int32 col, l_float32* colvect);
		l_ok pixThresholdForFgBg(PIX* pixs, l_int32 factor, l_int32 thresh, l_int32* pfgval, l_int32* pbgval);
		l_ok pixSplitDistributionFgBg(PIX* pixs, l_float32 scorefract, l_int32 factor, l_int32* pthresh, l_int32* pfgval, l_int32* pbgval, PIX** ppixdb);
		l_ok pixaFindDimensions(PIXA* pixa, NUMA** pnaw, NUMA** pnah);
		l_ok pixFindAreaPerimRatio(PIX* pixs, l_int32* tab, l_float32* pfract);
		NUMA* pixaFindPerimToAreaRatio(PIXA* pixa);
		l_ok pixFindPerimToAreaRatio(PIX* pixs, l_int32* tab, l_float32* pfract);
		NUMA* pixaFindPerimSizeRatio(PIXA* pixa);
		l_ok pixFindPerimSizeRatio(PIX* pixs, l_int32* tab, l_float32* pratio);
		NUMA* pixaFindAreaFraction(PIXA* pixa);
		l_ok pixFindAreaFraction(PIX* pixs, l_int32* tab, l_float32* pfract);
		NUMA* pixaFindAreaFractionMasked(PIXA* pixa, PIX* pixm, l_int32 debug);
		l_ok pixFindAreaFractionMasked(PIX* pixs, BOX* box, PIX* pixm, l_int32* tab, l_float32* pfract);
		NUMA* pixaFindWidthHeightRatio(PIXA* pixa);
		NUMA* pixaFindWidthHeightProduct(PIXA* pixa);
		l_ok pixFindOverlapFraction(PIX* pixs1, PIX* pixs2, l_int32 x2, l_int32 y2, l_int32* tab, l_float32* pratio, l_int32* pnoverlap);
		BOXA* pixFindRectangleComps(PIX* pixs, l_int32 dist, l_int32 minw, l_int32 minh);
		l_ok pixConformsToRectangle(PIX* pixs, BOX* box, l_int32 dist, l_int32* pconforms);
		PIXA* pixClipRectangles(PIX* pixs, BOXA* boxa);
		PIX* pixClipRectangle(PIX* pixs, BOX* box, BOX** pboxc);
		PIX* pixClipRectangleWithBorder(PIX* pixs, BOX* box, l_int32 maxbord, BOX** pboxn);
		PIX* pixClipMasked(PIX* pixs, PIX* pixm, l_int32 x, l_int32 y, l_uint32 outval);
		l_ok pixCropToMatch(PIX* pixs1, PIX* pixs2, PIX** ppixd1, PIX** ppixd2);
		PIX* pixCropToSize(PIX* pixs, l_int32 w, l_int32 h);
		PIX* pixResizeToMatch(PIX* pixs, PIX* pixt, l_int32 w, l_int32 h);
		PIX* pixSelectComponentBySize(PIX* pixs, l_int32 rankorder, l_int32 type, l_int32 connectivity, BOX** pbox);
		PIX* pixFilterComponentBySize(PIX* pixs, l_int32 rankorder, l_int32 type, l_int32 connectivity, BOX** pbox);
		PIX* pixMakeSymmetricMask(l_int32 w, l_int32 h, l_float32 hf, l_float32 vf, l_int32 type);
		PIX* pixMakeFrameMask(l_int32 w, l_int32 h, l_float32 hf1, l_float32 hf2, l_float32 vf1, l_float32 vf2);
		PIX* pixMakeCoveringOfRectangles(PIX* pixs, l_int32 maxiters);
		l_ok pixFractionFgInMask(PIX* pix1, PIX* pix2, l_float32* pfract);
		l_ok pixClipToForeground(PIX* pixs, PIX** ppixd, BOX** pbox);
		l_ok pixTestClipToForeground(PIX* pixs, l_int32* pcanclip);
		l_ok pixClipBoxToForeground(PIX* pixs, BOX* boxs, PIX** ppixd, BOX** pboxd);
		l_ok pixScanForForeground(PIX* pixs, BOX* box, l_int32 scanflag, l_int32* ploc);
		l_ok pixClipBoxToEdges(PIX* pixs, BOX* boxs, l_int32 lowthresh, l_int32 highthresh, l_int32 maxwidth, l_int32 factor, PIX** ppixd, BOX** pboxd);
		l_ok pixScanForEdge(PIX* pixs, BOX* box, l_int32 lowthresh, l_int32 highthresh, l_int32 maxwidth, l_int32 factor, l_int32 scanflag, l_int32* ploc);
		NUMA* pixExtractOnLine(PIX* pixs, l_int32 x1, l_int32 y1, l_int32 x2, l_int32 y2, l_int32 factor);
		l_float32 pixAverageOnLine(PIX* pixs, l_int32 x1, l_int32 y1, l_int32 x2, l_int32 y2, l_int32 factor);
		NUMA* pixAverageIntensityProfile(PIX* pixs, l_float32 fract, l_int32 dir, l_int32 first, l_int32 last, l_int32 factor1, l_int32 factor2);
		NUMA* pixReversalProfile(PIX* pixs, l_float32 fract, l_int32 dir, l_int32 first, l_int32 last, l_int32 minreversal, l_int32 factor1, l_int32 factor2);
		l_ok pixWindowedVarianceOnLine(PIX* pixs, l_int32 dir, l_int32 loc, l_int32 c1, l_int32 c2, l_int32 size, NUMA** pnad);
		l_ok pixMinMaxNearLine(PIX* pixs, l_int32 x1, l_int32 y1, l_int32 x2, l_int32 y2, l_int32 dist, l_int32 direction, NUMA** pnamin, NUMA** pnamax, l_float32* pminave, l_float32* pmaxave);
		PIX* pixRankRowTransform(PIX* pixs);
		PIX* pixRankColumnTransform(PIX* pixs);

		PIXA* pixaCreate(l_int32 n);
		PIXA* pixaCreateFromPix(PIX* pixs, l_int32 n, l_int32 cellw, l_int32 cellh);
		PIXA* pixaCreateFromBoxa(PIX* pixs, BOXA* boxa, l_int32 start, l_int32 num, l_int32* pcropwarn);
		PIXA* pixaSplitPix(PIX* pixs, l_int32 nx, l_int32 ny, l_int32 borderwidth, l_uint32 bordercolor);
		void pixaDestroy(PIXA** ppixa);
		PIXA* pixaCopy(PIXA* pixa, l_int32 copyflag);
		l_ok pixaAddPix(PIXA* pixa, PIX* pix, l_int32 copyflag);
		l_ok pixaAddBox(PIXA* pixa, BOX* box, l_int32 copyflag);
		l_ok pixaExtendArrayToSize(PIXA* pixa, size_t size);
		l_int32 pixaGetCount(PIXA* pixa);
		PIX* pixaGetPix(PIXA* pixa, l_int32 index, l_int32 accesstype);
		l_ok pixaGetPixDimensions(PIXA* pixa, l_int32 index, l_int32* pw, l_int32* ph, l_int32* pd);
		BOXA* pixaGetBoxa(PIXA* pixa, l_int32 accesstype);
		l_int32 pixaGetBoxaCount(PIXA* pixa);
		BOX* pixaGetBox(PIXA* pixa, l_int32 index, l_int32 accesstype);
		l_ok pixaGetBoxGeometry(PIXA* pixa, l_int32 index, l_int32* px, l_int32* py, l_int32* pw, l_int32* ph);
		l_ok pixaSetBoxa(PIXA* pixa, BOXA* boxa, l_int32 accesstype);
		PIX** pixaGetPixArray(PIXA* pixa);
		l_ok pixaVerifyDepth(PIXA* pixa, l_int32* psame, l_int32* pmaxd);
		l_ok pixaVerifyDimensions(PIXA* pixa, l_int32* psame, l_int32* pmaxw, l_int32* pmaxh);
		l_ok pixaIsFull(PIXA* pixa, l_int32* pfullpa, l_int32* pfullba);
		l_ok pixaCountText(PIXA* pixa, l_int32* pntext);
		l_ok pixaSetText(PIXA* pixa, const char* text, SARRAY* sa);
		void*** pixaGetLinePtrs(PIXA* pixa, l_int32* psize);
		l_ok pixaWriteStreamInfo(FILE* fp, PIXA* pixa);
		l_ok pixaReplacePix(PIXA* pixa, l_int32 index, PIX* pix, BOX* box);
		l_ok pixaInsertPix(PIXA* pixa, l_int32 index, PIX* pixs, BOX* box);
		l_ok pixaRemovePix(PIXA* pixa, l_int32 index);
		l_ok pixaRemovePixAndSave(PIXA* pixa, l_int32 index, PIX** ppix, BOX** pbox);
		l_ok pixaRemoveSelected(PIXA* pixa, NUMA* naindex);
		l_ok pixaInitFull(PIXA* pixa, PIX* pix, BOX* box);
		l_ok pixaClear(PIXA* pixa);
		l_ok pixaJoin(PIXA* pixad, PIXA* pixas, l_int32 istart, l_int32 iend);
		PIXA* pixaInterleave(PIXA* pixa1, PIXA* pixa2, l_int32 copyflag);
		l_ok pixaaJoin(PIXAA* paad, PIXAA* paas, l_int32 istart, l_int32 iend);
		PIXAA* pixaaCreate(l_int32 n);
		PIXAA* pixaaCreateFromPixa(PIXA* pixa, l_int32 n, l_int32 type, l_int32 copyflag);
		void pixaaDestroy(PIXAA** ppaa);
		l_ok pixaaAddPixa(PIXAA* paa, PIXA* pixa, l_int32 copyflag);
		l_ok pixaaAddPix(PIXAA* paa, l_int32 index, PIX* pix, BOX* box, l_int32 copyflag);
		l_ok pixaaAddBox(PIXAA* paa, BOX* box, l_int32 copyflag);
		l_int32 pixaaGetCount(PIXAA* paa, NUMA** pna);
		PIXA* pixaaGetPixa(PIXAA* paa, l_int32 index, l_int32 accesstype);
		BOXA* pixaaGetBoxa(PIXAA* paa, l_int32 accesstype);
		PIX* pixaaGetPix(PIXAA* paa, l_int32 index, l_int32 ipix, l_int32 accessflag);
		l_ok pixaaVerifyDepth(PIXAA* paa, l_int32* psame, l_int32* pmaxd);
		l_ok pixaaVerifyDimensions(PIXAA* paa, l_int32* psame, l_int32* pmaxw, l_int32* pmaxh);
		l_int32 pixaaIsFull(PIXAA* paa, l_int32* pfull);
		l_ok pixaaInitFull(PIXAA* paa, PIXA* pixa);
		l_ok pixaaReplacePixa(PIXAA* paa, l_int32 index, PIXA* pixa);
		l_ok pixaaClear(PIXAA* paa);
		l_ok pixaaTruncate(PIXAA* paa);
		PIXA* pixaRead(const char* filename);
		PIXA* pixaReadStream(FILE* fp);
		PIXA* pixaReadMem(const l_uint8* data, size_t size);
		l_ok pixaWriteDebug(const char* fname, PIXA* pixa);
		l_ok pixaWrite(const char* filename, PIXA* pixa);
		l_ok pixaWriteStream(FILE* fp, PIXA* pixa);
		l_ok pixaWriteMem(l_uint8** pdata, size_t* psize, PIXA* pixa);
		PIXA* pixaReadBoth(const char* filename);
		PIXAA* pixaaReadFromFiles(const char* dirname, const char* substr, l_int32 first, l_int32 nfiles);
		PIXAA* pixaaRead(const char* filename);
		PIXAA* pixaaReadStream(FILE* fp);
		PIXAA* pixaaReadMem(const l_uint8* data, size_t size);
		l_ok pixaaWrite(const char* filename, PIXAA* paa);
		l_ok pixaaWriteStream(FILE* fp, PIXAA* paa);
		l_ok pixaaWriteMem(l_uint8** pdata, size_t* psize, PIXAA* paa);
		PIXACC* pixaccCreate(l_int32 w, l_int32 h, l_int32 negflag);
		PIXACC* pixaccCreateFromPix(PIX* pix, l_int32 negflag);
		void pixaccDestroy(PIXACC** ppixacc);
		PIX* pixaccFinal(PIXACC* pixacc, l_int32 outdepth);
		PIX* pixaccGetPix(PIXACC* pixacc);
		l_int32 pixaccGetOffset(PIXACC* pixacc);
		l_ok pixaccAdd(PIXACC* pixacc, PIX* pix);
		l_ok pixaccSubtract(PIXACC* pixacc, PIX* pix);
		l_ok pixaccMultConst(PIXACC* pixacc, l_float32 factor);
		l_ok pixaccMultConstAccumulate(PIXACC* pixacc, PIX* pix, l_float32 factor);
		PIX* pixSelectBySize(PIX* pixs, l_int32 width, l_int32 height, l_int32 connectivity, l_int32 type, l_int32 relation, l_int32* pchanged);
		PIXA* pixaSelectBySize(PIXA* pixas, l_int32 width, l_int32 height, l_int32 type, l_int32 relation, l_int32* pchanged);
		NUMA* pixaMakeSizeIndicator(PIXA* pixa, l_int32 width, l_int32 height, l_int32 type, l_int32 relation);
		PIX* pixSelectByPerimToAreaRatio(PIX* pixs, l_float32 thresh, l_int32 connectivity, l_int32 type, l_int32* pchanged);
		PIXA* pixaSelectByPerimToAreaRatio(PIXA* pixas, l_float32 thresh, l_int32 type, l_int32* pchanged);
		PIX* pixSelectByPerimSizeRatio(PIX* pixs, l_float32 thresh, l_int32 connectivity, l_int32 type, l_int32* pchanged);
		PIXA* pixaSelectByPerimSizeRatio(PIXA* pixas, l_float32 thresh, l_int32 type, l_int32* pchanged);
		PIX* pixSelectByAreaFraction(PIX* pixs, l_float32 thresh, l_int32 connectivity, l_int32 type, l_int32* pchanged);
		PIXA* pixaSelectByAreaFraction(PIXA* pixas, l_float32 thresh, l_int32 type, l_int32* pchanged);
		PIX* pixSelectByArea(PIX* pixs, l_float32 thresh, l_int32 connectivity, l_int32 type, l_int32* pchanged);
		PIXA* pixaSelectByArea(PIXA* pixas, l_float32 thresh, l_int32 type, l_int32* pchanged);
		PIX* pixSelectByWidthHeightRatio(PIX* pixs, l_float32 thresh, l_int32 connectivity, l_int32 type, l_int32* pchanged);
		PIXA* pixaSelectByWidthHeightRatio(PIXA* pixas, l_float32 thresh, l_int32 type, l_int32* pchanged);
		PIXA* pixaSelectByNumConnComp(PIXA* pixas, l_int32 nmin, l_int32 nmax, l_int32 connectivity, l_int32* pchanged);
		PIXA* pixaSelectWithIndicator(PIXA* pixas, NUMA* na, l_int32* pchanged);
		l_ok pixRemoveWithIndicator(PIX* pixs, PIXA* pixa, NUMA* na);
		l_ok pixAddWithIndicator(PIX* pixs, PIXA* pixa, NUMA* na);
		PIXA* pixaSelectWithString(PIXA* pixas, const char* str, l_int32* perror);
		PIX* pixaRenderComponent(PIX* pixs, PIXA* pixa, l_int32 index);
		PIXA* pixaSort(PIXA* pixas, l_int32 sorttype, l_int32 sortorder, NUMA** pnaindex, l_int32 copyflag);
		PIXA* pixaBinSort(PIXA* pixas, l_int32 sorttype, l_int32 sortorder, NUMA** pnaindex, l_int32 copyflag);
		PIXA* pixaSortByIndex(PIXA* pixas, NUMA* naindex, l_int32 copyflag);
		PIXAA* pixaSort2dByIndex(PIXA* pixas, NUMAA* naa, l_int32 copyflag);
		PIXA* pixaSelectRange(PIXA* pixas, l_int32 first, l_int32 last, l_int32 copyflag);
		PIXAA* pixaaSelectRange(PIXAA* paas, l_int32 first, l_int32 last, l_int32 copyflag);
		PIXAA* pixaaScaleToSize(PIXAA* paas, l_int32 wd, l_int32 hd);
		PIXAA* pixaaScaleToSizeVar(PIXAA* paas, NUMA* nawd, NUMA* nahd);
		PIXA* pixaScaleToSize(PIXA* pixas, l_int32 wd, l_int32 hd);
		PIXA* pixaScaleToSizeRel(PIXA* pixas, l_int32 delw, l_int32 delh);
		PIXA* pixaScale(PIXA* pixas, l_float32 scalex, l_float32 scaley);
		PIXA* pixaScaleBySampling(PIXA* pixas, l_float32 scalex, l_float32 scaley);
		PIXA* pixaRotate(PIXA* pixas, l_float32 angle, l_int32 type, l_int32 incolor, l_int32 width, l_int32 height);
		PIXA* pixaRotateOrth(PIXA* pixas, l_int32 rotation);
		PIXA* pixaTranslate(PIXA* pixas, l_int32 hshift, l_int32 vshift, l_int32 incolor);
		PIXA* pixaAddBorderGeneral(PIXA* pixad, PIXA* pixas, l_int32 left, l_int32 right, l_int32 top, l_int32 bot, l_uint32 val);
		PIXA* pixaaFlattenToPixa(PIXAA* paa, NUMA** pnaindex, l_int32 copyflag);
		l_ok pixaaSizeRange(PIXAA* paa, l_int32* pminw, l_int32* pminh, l_int32* pmaxw, l_int32* pmaxh);
		l_ok pixaSizeRange(PIXA* pixa, l_int32* pminw, l_int32* pminh, l_int32* pmaxw, l_int32* pmaxh);
		PIXA* pixaClipToPix(PIXA* pixas, PIX* pixs);
		l_ok pixaClipToForeground(PIXA* pixas, PIXA** ppixad, BOXA** pboxa);
		l_ok pixaGetRenderingDepth(PIXA* pixa, l_int32* pdepth);
		l_ok pixaHasColor(PIXA* pixa, l_int32* phascolor);
		l_ok pixaAnyColormaps(PIXA* pixa, l_int32* phascmap);
		l_ok pixaGetDepthInfo(PIXA* pixa, l_int32* pmaxdepth, l_int32* psame);
		PIXA* pixaConvertToSameDepth(PIXA* pixas);
		PIXA* pixaConvertToGivenDepth(PIXA* pixas, l_int32 depth);
		l_ok pixaEqual(PIXA* pixa1, PIXA* pixa2, l_int32 maxdist, NUMA** pnaindex, l_int32* psame);
		l_ok pixaSetFullSizeBoxa(PIXA* pixa);
		PIX* pixaDisplay(PIXA* pixa, l_int32 w, l_int32 h);
		PIX* pixaDisplayRandomCmap(PIXA* pixa, l_int32 w, l_int32 h);
		PIX* pixaDisplayLinearly(PIXA* pixas, l_int32 direction, l_float32 scalefactor, l_int32 background, l_int32 spacing, l_int32 border, BOXA** pboxa);
		PIX* pixaDisplayOnLattice(PIXA* pixa, l_int32 cellw, l_int32 cellh, l_int32* pncols, BOXA** pboxa);
		PIX* pixaDisplayUnsplit(PIXA* pixa, l_int32 nx, l_int32 ny, l_int32 borderwidth, l_uint32 bordercolor);
		PIX* pixaDisplayTiled(PIXA* pixa, l_int32 maxwidth, l_int32 background, l_int32 spacing);
		PIX* pixaDisplayTiledInRows(PIXA* pixa, l_int32 outdepth, l_int32 maxwidth, l_float32 scalefactor, l_int32 background, l_int32 spacing, l_int32 border);
		PIX* pixaDisplayTiledInColumns(PIXA* pixas, l_int32 nx, l_float32 scalefactor, l_int32 spacing, l_int32 border);
		PIX* pixaDisplayTiledAndScaled(PIXA* pixa, l_int32 outdepth, l_int32 tilewidth, l_int32 ncols, l_int32 background, l_int32 spacing, l_int32 border);
		PIX* pixaDisplayTiledWithText(PIXA* pixa, l_int32 maxwidth, l_float32 scalefactor, l_int32 spacing, l_int32 border, l_int32 fontsize, l_uint32 textcolor);
		PIX* pixaDisplayTiledByIndex(PIXA* pixa, NUMA* na, l_int32 width, l_int32 spacing, l_int32 border, l_int32 fontsize, l_uint32 textcolor);
		PIX* pixaDisplayPairTiledInColumns(PIXA* pixas1, PIXA* pixas2, l_int32 nx, l_float32 scalefactor, l_int32 spacing1, l_int32 spacing2, l_int32 border1, l_int32 border2, l_int32 fontsize, l_int32 startindex, SARRAY* sa);
		PIX* pixaaDisplay(PIXAA* paa, l_int32 w, l_int32 h);
		PIX* pixaaDisplayByPixa(PIXAA* paa, l_int32 maxnx, l_float32 scalefactor, l_int32 hspacing, l_int32 vspacing, l_int32 border);
		PIXA* pixaaDisplayTiledAndScaled(PIXAA* paa, l_int32 outdepth, l_int32 tilewidth, l_int32 ncols, l_int32 background, l_int32 spacing, l_int32 border);
		PIXA* pixaConvertTo1(PIXA* pixas, l_int32 thresh);
		PIXA* pixaConvertTo8(PIXA* pixas, l_int32 cmapflag);
		PIXA* pixaConvertTo8Colormap(PIXA* pixas, l_int32 dither);
		PIXA* pixaConvertTo32(PIXA* pixas);
		PIXA* pixaConstrainedSelect(PIXA* pixas, l_int32 first, l_int32 last, l_int32 nmax, l_int32 use_pairs, l_int32 copyflag);
		l_ok pixaSelectToPdf(PIXA* pixas, l_int32 first, l_int32 last, l_int32 res, l_float32 scalefactor, l_int32 type, l_int32 quality, l_uint32 color, l_int32 fontsize, const char* fileout);
		PIXA* pixaMakeFromTiledPixa(PIXA* pixas, l_int32 w, l_int32 h, l_int32 nsamp);
		PIXA* pixaMakeFromTiledPix(PIX* pixs, l_int32 w, l_int32 h, l_int32 start, l_int32 num, BOXA* boxa);
		l_ok pixGetTileCount(PIX* pix, l_int32* pn);
		PIXA* pixaDisplayMultiTiled(PIXA* pixas, l_int32 nx, l_int32 ny, l_int32 maxw, l_int32 maxh, l_float32 scalefactor, l_int32 spacing, l_int32 border);
		l_ok pixaSplitIntoFiles(PIXA* pixas, l_int32 nsplit, l_float32 scale, l_int32 outwidth, l_int32 write_pixa, l_int32 write_pix, l_int32 write_pdf);

		PIXA* convertToNUpPixa(const char* dir, const char* substr, l_int32 nx, l_int32 ny, l_int32 tw, l_int32 spacing, l_int32 border, l_int32 fontsize);
		PIXA* pixaConvertToNUpPixa(PIXA* pixas, SARRAY* sa, l_int32 nx, l_int32 ny, l_int32 tw, l_int32 spacing, l_int32 border, l_int32 fontsize);
		l_ok pixaCompareInPdf(PIXA* pixa1, PIXA* pixa2, l_int32 nx, l_int32 ny, l_int32 tw, l_int32 spacing, l_int32 border, l_int32 fontsize, const char* fileout);

		l_ok pixAddConstantGray(PIX* pixs, l_int32 val);
		l_ok pixMultConstantGray(PIX* pixs, l_float32 val);
		PIX* pixAddGray(PIX* pixd, PIX* pixs1, PIX* pixs2);
		PIX* pixSubtractGray(PIX* pixd, PIX* pixs1, PIX* pixs2);
		PIX* pixMultiplyGray(PIX* pixs, PIX* pixg, l_float32 norm);
		PIX* pixThresholdToValue(PIX* pixd, PIX* pixs, l_int32 threshval, l_int32 setval);
		PIX* pixInitAccumulate(l_int32 w, l_int32 h, l_uint32 offset);
		PIX* pixFinalAccumulate(PIX* pixs, l_uint32 offset, l_int32 depth);
		PIX* pixFinalAccumulateThreshold(PIX* pixs, l_uint32 offset, l_uint32 threshold);
		l_ok pixAccumulate(PIX* pixd, PIX* pixs, l_int32 op);
		l_ok pixMultConstAccumulate(PIX* pixs, l_float32 factor, l_uint32 offset);
		PIX* pixAbsDifference(PIX* pixs1, PIX* pixs2);
		PIX* pixAddRGB(PIX* pixs1, PIX* pixs2);
		PIX* pixMinOrMax(PIX* pixd, PIX* pixs1, PIX* pixs2, l_int32 type);
		PIX* pixMaxDynamicRange(PIX* pixs, l_int32 type);
		PIX* pixMaxDynamicRangeRGB(PIX* pixs, l_int32 type);

		PIXC* pixcompCreateFromPix(PIX* pix, l_int32 comptype);
		PIXC* pixcompCreateFromString(l_uint8* data, size_t size, l_int32 copyflag);
		PIXC* pixcompCreateFromFile(const char* filename, l_int32 comptype);
		void pixcompDestroy(PIXC** ppixc);
		PIXC* pixcompCopy(PIXC* pixcs);
		l_ok pixcompGetDimensions(PIXC* pixc, l_int32* pw, l_int32* ph, l_int32* pd);
		l_ok pixcompGetParameters(PIXC* pixc, l_int32* pxres, l_int32* pyres, l_int32* pcomptype, l_int32* pcmapflag);

		PIX* pixCreateFromPixcomp(PIXC* pixc);
		PIXAC* pixacompCreate(l_int32 n);
		PIXAC* pixacompCreateWithInit(l_int32 n, l_int32 offset, PIX* pix, l_int32 comptype);
		PIXAC* pixacompCreateFromPixa(PIXA* pixa, l_int32 comptype, l_int32 accesstype);
		PIXAC* pixacompCreateFromFiles(const char* dirname, const char* substr, l_int32 comptype);
		PIXAC* pixacompCreateFromSA(SARRAY* sa, l_int32 comptype);
		void pixacompDestroy(PIXAC** ppixac);
		l_ok pixacompAddPix(PIXAC* pixac, PIX* pix, l_int32 comptype);
		l_ok pixacompAddPixcomp(PIXAC* pixac, PIXC* pixc, l_int32 copyflag);
		l_ok pixacompReplacePix(PIXAC* pixac, l_int32 index, PIX* pix, l_int32 comptype);
		l_ok pixacompReplacePixcomp(PIXAC* pixac, l_int32 index, PIXC* pixc);
		l_ok pixacompAddBox(PIXAC* pixac, BOX* box, l_int32 copyflag);
		l_int32 pixacompGetCount(PIXAC* pixac);
		PIXC* pixacompGetPixcomp(PIXAC* pixac, l_int32 index, l_int32 copyflag);
		PIX* pixacompGetPix(PIXAC* pixac, l_int32 index);
		l_ok pixacompGetPixDimensions(PIXAC* pixac, l_int32 index, l_int32* pw, l_int32* ph, l_int32* pd);
		BOXA* pixacompGetBoxa(PIXAC* pixac, l_int32 accesstype);
		l_int32 pixacompGetBoxaCount(PIXAC* pixac);
		BOX* pixacompGetBox(PIXAC* pixac, l_int32 index, l_int32 accesstype);
		l_ok pixacompGetBoxGeometry(PIXAC* pixac, l_int32 index, l_int32* px, l_int32* py, l_int32* pw, l_int32* ph);
		l_int32 pixacompGetOffset(PIXAC* pixac);
		l_ok pixacompSetOffset(PIXAC* pixac, l_int32 offset);
		PIXA* pixaCreateFromPixacomp(PIXAC* pixac, l_int32 accesstype);
		l_ok pixacompJoin(PIXAC* pixacd, PIXAC* pixacs, l_int32 istart, l_int32 iend);
		PIXAC* pixacompInterleave(PIXAC* pixac1, PIXAC* pixac2);
		PIXAC* pixacompRead(const char* filename);
		PIXAC* pixacompReadStream(FILE* fp);
		PIXAC* pixacompReadMem(const l_uint8* data, size_t size);
		l_ok pixacompWrite(const char* filename, PIXAC* pixac);
		l_ok pixacompWriteStream(FILE* fp, PIXAC* pixac);
		l_ok pixacompWriteMem(l_uint8** pdata, size_t* psize, PIXAC* pixac);
		l_ok pixacompConvertToPdf(PIXAC* pixac, l_int32 res, l_float32 scalefactor, l_int32 type, l_int32 quality, const char* title, const char* fileout);
		l_ok pixacompConvertToPdfData(PIXAC* pixac, l_int32 res, l_float32 scalefactor, l_int32 type, l_int32 quality, const char* title, l_uint8** pdata, size_t* pnbytes);
		l_ok pixacompFastConvertToPdfData(PIXAC* pixac, const char* title, l_uint8** pdata, size_t* pnbytes);
		l_ok pixacompWriteStreamInfo(FILE* fp, PIXAC* pixac, const char* text);
		l_ok pixcompWriteStreamInfo(FILE* fp, PIXC* pixc, const char* text);
		PIX* pixacompDisplayTiledAndScaled(PIXAC* pixac, l_int32 outdepth, l_int32 tilewidth, l_int32 ncols, l_int32 background, l_int32 spacing, l_int32 border);
		l_ok pixacompWriteFiles(PIXAC* pixac, const char* subdir);
		l_ok pixcompWriteFile(const char* rootname, PIXC* pixc);
		PIX* pixThreshold8(PIX* pixs, l_int32 d, l_int32 nlevels, l_int32 cmapflag);
		PIX* pixRemoveColormapGeneral(PIX* pixs, l_int32 type, l_int32 ifnocmap);
		PIX* pixRemoveColormap(PIX* pixs, l_int32 type);
		l_ok pixAddGrayColormap8(PIX* pixs);
		PIX* pixAddMinimalGrayColormap8(PIX* pixs);
		PIX* pixConvertRGBToLuminance(PIX* pixs);
		PIX* pixConvertRGBToGrayGeneral(PIX* pixs, l_int32 type, l_float32 rwt, l_float32 gwt, l_float32 bwt);
		PIX* pixConvertRGBToGray(PIX* pixs, l_float32 rwt, l_float32 gwt, l_float32 bwt);
		PIX* pixConvertRGBToGrayFast(PIX* pixs);
		PIX* pixConvertRGBToGrayMinMax(PIX* pixs, l_int32 type);
		PIX* pixConvertRGBToGraySatBoost(PIX* pixs, l_int32 refval);
		PIX* pixConvertRGBToGrayArb(PIX* pixs, l_float32 rc, l_float32 gc, l_float32 bc);
		PIX* pixConvertRGBToBinaryArb(PIX* pixs, l_float32 rc, l_float32 gc, l_float32 bc, l_int32 thresh, l_int32 relation);
		PIX* pixConvertGrayToColormap(PIX* pixs);
		PIX* pixConvertGrayToColormap8(PIX* pixs, l_int32 mindepth);
		PIX* pixColorizeGray(PIX* pixs, l_uint32 color, l_int32 cmapflag);
		PIX* pixConvertRGBToColormap(PIX* pixs, l_int32 ditherflag);
		PIX* pixConvertCmapTo1(PIX* pixs);
		l_ok pixQuantizeIfFewColors(PIX* pixs, l_int32 maxcolors, l_int32 mingraycolors, l_int32 octlevel, PIX** ppixd);
		PIX* pixConvert16To8(PIX* pixs, l_int32 type);
		PIX* pixConvertGrayToFalseColor(PIX* pixs, l_float32 gamma);
		PIX* pixUnpackBinary(PIX* pixs, l_int32 depth, l_int32 invert);
		PIX* pixConvert1To16(PIX* pixd, PIX* pixs, l_uint16 val0, l_uint16 val1);
		PIX* pixConvert1To32(PIX* pixd, PIX* pixs, l_uint32 val0, l_uint32 val1);
		PIX* pixConvert1To2Cmap(PIX* pixs);
		PIX* pixConvert1To2(PIX* pixd, PIX* pixs, l_int32 val0, l_int32 val1);
		PIX* pixConvert1To4Cmap(PIX* pixs);
		PIX* pixConvert1To4(PIX* pixd, PIX* pixs, l_int32 val0, l_int32 val1);
		PIX* pixConvert1To8Cmap(PIX* pixs);
		PIX* pixConvert1To8(PIX* pixd, PIX* pixs, l_uint8 val0, l_uint8 val1);
		PIX* pixConvert2To8(PIX* pixs, l_uint8 val0, l_uint8 val1, l_uint8 val2, l_uint8 val3, l_int32 cmapflag);
		PIX* pixConvert4To8(PIX* pixs, l_int32 cmapflag);
		PIX* pixConvert8To16(PIX* pixs, l_int32 leftshift);
		PIX* pixConvertTo2(PIX* pixs);
		PIX* pixConvert8To2(PIX* pix);
		PIX* pixConvertTo4(PIX* pixs);
		PIX* pixConvert8To4(PIX* pix);
		PIX* pixConvertTo1Adaptive(PIX* pixs);
		PIX* pixConvertTo1(PIX* pixs, l_int32 threshold);
		PIX* pixConvertTo1BySampling(PIX* pixs, l_int32 factor, l_int32 threshold);
		PIX* pixConvertTo8(PIX* pixs, l_int32 cmapflag);
		PIX* pixConvertTo8BySampling(PIX* pixs, l_int32 factor, l_int32 cmapflag);
		PIX* pixConvertTo8Colormap(PIX* pixs, l_int32 dither);
		PIX* pixConvertTo16(PIX* pixs);
		PIX* pixConvertTo32(PIX* pixs);
		PIX* pixConvertTo32BySampling(PIX* pixs, l_int32 factor);
		PIX* pixConvert8To32(PIX* pixs);
		PIX* pixConvertTo8Or32(PIX* pixs, l_int32 copyflag, l_int32 warnflag);
		PIX* pixConvert24To32(PIX* pixs);
		PIX* pixConvert32To24(PIX* pixs);
		PIX* pixConvert32To16(PIX* pixs, l_int32 type);
		PIX* pixConvert32To8(PIX* pixs, l_int32 type16, l_int32 type8);
		PIX* pixRemoveAlpha(PIX* pixs);
		PIX* pixAddAlphaTo1bpp(PIX* pixd, PIX* pixs);
		PIX* pixConvertLossless(PIX* pixs, l_int32 d);
		PIX* pixConvertForPSWrap(PIX* pixs);
		PIX* pixConvertToSubpixelRGB(PIX* pixs, l_float32 scalex, l_float32 scaley, l_int32 order);
		PIX* pixConvertGrayToSubpixelRGB(PIX* pixs, l_float32 scalex, l_float32 scaley, l_int32 order);
		PIX* pixConvertColorToSubpixelRGB(PIX* pixs, l_float32 scalex, l_float32 scaley, l_int32 order);

		PIX* pixConnCompTransform(PIX* pixs, l_int32 connect, l_int32 depth);
		PIX* pixConnCompAreaTransform(PIX* pixs, l_int32 connect);
		l_ok pixConnCompIncrInit(PIX* pixs, l_int32 conn, PIX** ppixd, PTAA** pptaa, l_int32* pncc);
		l_int32 pixConnCompIncrAdd(PIX* pixs, PTAA* ptaa, l_int32* pncc, l_float32 x, l_float32 y, l_int32 debug);
		l_ok pixGetSortedNeighborValues(PIX* pixs, l_int32 x, l_int32 y, l_int32 conn, l_int32** pneigh, l_int32* pnvals);
		PIX* pixLocToColorTransform(PIX* pixs);
		PIXTILING* pixTilingCreate(PIX* pixs, l_int32 nx, l_int32 ny, l_int32 w, l_int32 h, l_int32 xoverlap, l_int32 yoverlap);
		void pixTilingDestroy(PIXTILING** ppt);
		l_ok pixTilingGetCount(PIXTILING* pt, l_int32* pnx, l_int32* pny);
		l_ok pixTilingGetSize(PIXTILING* pt, l_int32* pw, l_int32* ph);
		PIX* pixTilingGetTile(PIXTILING* pt, l_int32 i, l_int32 j);
		l_ok pixTilingNoStripOnPaint(PIXTILING* pt);
		l_ok pixTilingPaintTile(PIX* pixd, l_int32 i, l_int32 j, PIX* pixs, PIXTILING* pt);
		PIX* pixReadStreamPng(FILE* fp);

		l_ok pixWritePng(const char* filename, PIX* pix, l_float32 gamma);
		l_ok pixWriteStreamPng(FILE* fp, PIX* pix, l_float32 gamma);
		l_ok pixSetZlibCompression(PIX* pix, l_int32 compval);

		PIX* pixReadMemPng(const l_uint8* filedata, size_t filesize);
		l_ok pixWriteMemPng(l_uint8** pfiledata, size_t* pfilesize, PIX* pix, l_float32 gamma);
		PIX* pixReadStreamPnm(FILE* fp);

		l_ok pixWriteStreamPnm(FILE* fp, PIX* pix);
		l_ok pixWriteStreamAsciiPnm(FILE* fp, PIX* pix);
		l_ok pixWriteStreamPam(FILE* fp, PIX* pix);
		PIX* pixReadMemPnm(const l_uint8* cdata, size_t size);

		l_ok pixWriteMemPnm(l_uint8** pdata, size_t* psize, PIX* pix);
		l_ok pixWriteMemPam(l_uint8** pdata, size_t* psize, PIX* pix);
		PIX* pixProjectiveSampledPta(PIX* pixs, PTA* ptad, PTA* ptas, l_int32 incolor);
		PIX* pixProjectiveSampled(PIX* pixs, l_float32* vc, l_int32 incolor);
		PIX* pixProjectivePta(PIX* pixs, PTA* ptad, PTA* ptas, l_int32 incolor);
		PIX* pixProjective(PIX* pixs, l_float32* vc, l_int32 incolor);
		PIX* pixProjectivePtaColor(PIX* pixs, PTA* ptad, PTA* ptas, l_uint32 colorval);
		PIX* pixProjectiveColor(PIX* pixs, l_float32* vc, l_uint32 colorval);
		PIX* pixProjectivePtaGray(PIX* pixs, PTA* ptad, PTA* ptas, l_uint8 grayval);
		PIX* pixProjectiveGray(PIX* pixs, l_float32* vc, l_uint8 grayval);
		PIX* pixProjectivePtaWithAlpha(PIX* pixs, PTA* ptad, PTA* ptas, PIX* pixg, l_float32 fract, l_int32 border);

		l_ok pixWriteSegmentedPageToPS(PIX* pixs, PIX* pixm, l_float32 textscale, l_float32 imagescale, l_int32 threshold, l_int32 pageno, const char* fileout);
		l_ok pixWriteMixedToPS(PIX* pixb, PIX* pixc, l_float32 scale, l_int32 pageno, const char* fileout);

		l_ok pixaWriteCompressedToPS(PIXA* pixa, const char* fileout, l_int32 res, l_int32 level);
		l_ok pixWriteCompressedToPS(PIX* pix, const char* fileout, l_int32 res, l_int32 level, l_int32* pindex);
		l_ok pixWriteStreamPS(FILE* fp, PIX* pix, BOX* box, l_int32 res, l_float32 scale);
		char* pixWriteStringPS(PIX* pixs, BOX* box, l_int32 res, l_float32 scale);

		l_ok pixWriteMemPS(l_uint8** pdata, size_t* psize, PIX* pix, BOX* box, l_int32 res, l_float32 scale);

		PTA* pixFindCornerPixels(PIX* pixs);

		PTA* ptaCropToMask(PTA* ptas, PIX* pixm);

		l_ok pixPlotAlongPta(PIX* pixs, PTA* pta, l_int32 outformat, const char* title);
		PTA* ptaGetPixelsFromPix(PIX* pixs, BOX* box);
		PIX* pixGenerateFromPta(PTA* pta, l_int32 w, l_int32 h);
		PTA* ptaGetBoundaryPixels(PIX* pixs, l_int32 type);
		PTAA* ptaaGetBoundaryPixels(PIX* pixs, l_int32 type, l_int32 connectivity, BOXA** pboxa, PIXA** ppixa);
		PTAA* ptaaIndexLabeledPixels(PIX* pixs, l_int32* pncc);
		PTA* ptaGetNeighborPixLocs(PIX* pixs, l_int32 x, l_int32 y, l_int32 conn);

		PIX* pixDisplayPta(PIX* pixd, PIX* pixs, PTA* pta);
		PIX* pixDisplayPtaaPattern(PIX* pixd, PIX* pixs, PTAA* ptaa, PIX* pixp, l_int32 cx, l_int32 cy);
		PIX* pixDisplayPtaPattern(PIX* pixd, PIX* pixs, PTA* pta, PIX* pixp, l_int32 cx, l_int32 cy, l_uint32 color);
		PTA* ptaReplicatePattern(PTA* ptas, PIX* pixp, PTA* ptap, l_int32 cx, l_int32 cy, l_int32 w, l_int32 h);
		PIX* pixDisplayPtaa(PIX* pixs, PTAA* ptaa);

		l_ok pixQuadtreeMean(PIX* pixs, l_int32 nlevels, PIX* pix_ma, FPIXA** pfpixa);
		l_ok pixQuadtreeVariance(PIX* pixs, l_int32 nlevels, PIX* pix_ma, DPIX* dpix_msa, FPIXA** pfpixa_v, FPIXA** pfpixa_rv);
		l_ok pixMeanInRectangle(PIX* pixs, BOX* box, PIX* pixma, l_float32* pval);
		l_ok pixVarianceInRectangle(PIX* pixs, BOX* box, PIX* pix_ma, DPIX* dpix_msa, l_float32* pvar, l_float32* prvar);

		l_ok quadtreeGetParent(FPIXA* fpixa, l_int32 level, l_int32 x, l_int32 y, l_float32* pval);
		l_ok quadtreeGetChildren(FPIXA* fpixa, l_int32 level, l_int32 x, l_int32 y, l_float32* pval00, l_float32* pval10, l_float32* pval01, l_float32* pval11);

		PIX* fpixaDisplayQuadtree(FPIXA* fpixa, l_int32 factor, l_int32 fontsize);

		PIX* pixRankFilter(PIX* pixs, l_int32 wf, l_int32 hf, l_float32 rank);
		PIX* pixRankFilterRGB(PIX* pixs, l_int32 wf, l_int32 hf, l_float32 rank);
		PIX* pixRankFilterGray(PIX* pixs, l_int32 wf, l_int32 hf, l_float32 rank);
		PIX* pixMedianFilter(PIX* pixs, l_int32 wf, l_int32 hf);
		PIX* pixRankFilterWithScaling(PIX* pixs, l_int32 wf, l_int32 hf, l_float32 rank, l_float32 scalefactor);

		SARRAY* pixProcessBarcodes(PIX* pixs, l_int32 format, l_int32 method, SARRAY** psaw, l_int32 debugflag);
		PIXA* pixExtractBarcodes(PIX* pixs, l_int32 debugflag);
		SARRAY* pixReadBarcodes(PIXA* pixa, l_int32 format, l_int32 method, SARRAY** psaw, l_int32 debugflag);
		NUMA* pixReadBarcodeWidths(PIX* pixs, l_int32 method, l_int32 debugflag);
		BOXA* pixLocateBarcodes(PIX* pixs, l_int32 thresh, PIX** ppixb, PIX** ppixm);
		PIX* pixDeskewBarcode(PIX* pixs, PIX* pixb, BOX* box, l_int32 margin, l_int32 threshold, l_float32* pangle, l_float32* pconf);
		NUMA* pixExtractBarcodeWidths1(PIX* pixs, l_float32 thresh, l_float32 binfract, NUMA** pnaehist, NUMA** pnaohist, l_int32 debugflag);
		NUMA* pixExtractBarcodeWidths2(PIX* pixs, l_float32 thresh, l_float32* pwidth, NUMA** pnac, l_int32 debugflag);
		NUMA* pixExtractBarcodeCrossings(PIX* pixs, l_float32 thresh, l_int32 debugflag);

		PIXA* pixaReadFiles(const char* dirname, const char* substr);
		PIXA* pixaReadFilesSA(SARRAY* sa);
		PIX* pixRead(const char* filename);
		PIX* pixReadWithHint(const char* filename, l_int32 hint);
		PIX* pixReadIndexed(SARRAY* sa, l_int32 index);
		PIX* pixReadStream(FILE* fp, l_int32 hint);

		PIX* pixReadMem(const l_uint8* data, size_t size);

		L_RECOG* recogCreateFromPixa(PIXA* pixa, l_int32 scalew, l_int32 scaleh, l_int32 linew, l_int32 threshold, l_int32 maxyshift);
		L_RECOG* recogCreateFromPixaNoFinish(PIXA* pixa, l_int32 scalew, l_int32 scaleh, l_int32 linew, l_int32 threshold, l_int32 maxyshift);

		PIXA* recogExtractPixa(L_RECOG* recog);
		BOXA* recogDecode(L_RECOG* recog, PIX* pixs, l_int32 nlevels, PIX** ppixdb);
		l_ok recogCreateDid(L_RECOG* recog, PIX* pixs);

		l_ok recogIdentifyMultiple(L_RECOG* recog, PIX* pixs, l_int32 minh, l_int32 skipsplit, BOXA** pboxa, PIXA** ppixa, PIX** ppixdb, l_int32 debugsplit);
		l_ok recogSplitIntoCharacters(L_RECOG* recog, PIX* pixs, l_int32 minh, l_int32 skipsplit, BOXA** pboxa, PIXA** ppixa, l_int32 debug);
		l_ok recogCorrelationBestRow(L_RECOG* recog, PIX* pixs, BOXA** pboxa, NUMA** pnascore, NUMA** pnaindex, SARRAY** psachar, l_int32 debug);
		l_ok recogCorrelationBestChar(L_RECOG* recog, PIX* pixs, BOX** pbox, l_float32* pscore, l_int32* pindex, char** pcharstr, PIX** ppixdb);
		l_ok recogIdentifyPixa(L_RECOG* recog, PIXA* pixa, PIX** ppixdb);
		l_ok recogIdentifyPix(L_RECOG* recog, PIX* pixs, PIX** ppixdb);

		PIX* recogProcessToIdentify(L_RECOG* recog, PIX* pixs, l_int32 pad);

		PIXA* showExtractNumbers(PIX* pixs, SARRAY* sa, BOXAA* baa, NUMAA* naa, PIX** ppixdb);
		l_ok recogTrainLabeled(L_RECOG* recog, PIX* pixs, BOX* box, char* text, l_int32 debug);
		l_ok recogProcessLabeled(L_RECOG* recog, PIX* pixs, BOX* box, char* text, PIX** ppix);
		l_ok recogAddSample(L_RECOG* recog, PIX* pix, l_int32 debug);
		PIX* recogModifyTemplate(L_RECOG* recog, PIX* pixs);

		l_int32 pixaAccumulateSamples(PIXA* pixa, PTA* pta, PIX** ppixd, l_float32* px, l_float32* py);

		PIXA* recogFilterPixaBySize(PIXA* pixas, l_int32 setsize, l_int32 maxkeep, l_float32 max_ht_ratio, NUMA** pna);
		PIXAA* recogSortPixaByClass(PIXA* pixa, l_int32 setsize);
		l_ok recogRemoveOutliers1(L_RECOG** precog, l_float32 minscore, l_int32 mintarget, l_int32 minsize, PIX** ppixsave, PIX** ppixrem);
		PIXA* pixaRemoveOutliers1(PIXA* pixas, l_float32 minscore, l_int32 mintarget, l_int32 minsize, PIX** ppixsave, PIX** ppixrem);
		l_ok recogRemoveOutliers2(L_RECOG** precog, l_float32 minscore, l_int32 minsize, PIX** ppixsave, PIX** ppixrem);
		PIXA* pixaRemoveOutliers2(PIXA* pixas, l_float32 minscore, l_int32 minsize, PIX** ppixsave, PIX** ppixrem);
		PIXA* recogTrainFromBoot(L_RECOG* recogboot, PIXA* pixas, l_float32 minscore, l_int32 threshold, l_int32 debug);

		PIXA* recogAddDigitPadTemplates(L_RECOG* recog, SARRAY* sa);

		PIXA* recogMakeBootDigitTemplates(l_int32 nsamp, l_int32 debug);

		l_ok recogShowMatchesInRange(L_RECOG* recog, PIXA* pixa, l_float32 minscore, l_float32 maxscore, l_int32 display);
		PIX* recogShowMatch(L_RECOG* recog, PIX* pix1, PIX* pix2, BOX* box, l_int32 index, l_float32 score);

		l_ok regTestComparePix(L_REGPARAMS* rp, PIX* pix1, PIX* pix2);
		l_ok regTestCompareSimilarPix(L_REGPARAMS* rp, PIX* pix1, PIX* pix2, l_int32 mindiff, l_float32 maxfract, l_int32 printstats);

		l_ok regTestWritePixAndCheck(L_REGPARAMS* rp, PIX* pix, l_int32 format);

		l_ok pixRasterop(PIX* pixd, l_int32 dx, l_int32 dy, l_int32 dw, l_int32 dh, l_int32 op, PIX* pixs, l_int32 sx, l_int32 sy);
		l_ok pixRasteropVip(PIX* pixd, l_int32 bx, l_int32 bw, l_int32 vshift, l_int32 incolor);
		l_ok pixRasteropHip(PIX* pixd, l_int32 by, l_int32 bh, l_int32 hshift, l_int32 incolor);
		PIX* pixTranslate(PIX* pixd, PIX* pixs, l_int32 hshift, l_int32 vshift, l_int32 incolor);
		l_ok pixRasteropIP(PIX* pixd, l_int32 hshift, l_int32 vshift, l_int32 incolor);
		l_ok pixRasteropFullImage(PIX* pixd, PIX* pixs, l_int32 op);

		PIX* pixRotate(PIX* pixs, l_float32 angle, l_int32 type, l_int32 incolor, l_int32 width, l_int32 height);
		PIX* pixEmbedForRotation(PIX* pixs, l_float32 angle, l_int32 incolor, l_int32 width, l_int32 height);
		PIX* pixRotateBySampling(PIX* pixs, l_int32 xcen, l_int32 ycen, l_float32 angle, l_int32 incolor);
		PIX* pixRotateBinaryNice(PIX* pixs, l_float32 angle, l_int32 incolor);
		PIX* pixRotateWithAlpha(PIX* pixs, l_float32 angle, PIX* pixg, l_float32 fract);
		PIX* pixRotateAM(PIX* pixs, l_float32 angle, l_int32 incolor);
		PIX* pixRotateAMColor(PIX* pixs, l_float32 angle, l_uint32 colorval);
		PIX* pixRotateAMGray(PIX* pixs, l_float32 angle, l_uint8 grayval);
		PIX* pixRotateAMCorner(PIX* pixs, l_float32 angle, l_int32 incolor);
		PIX* pixRotateAMColorCorner(PIX* pixs, l_float32 angle, l_uint32 fillval);
		PIX* pixRotateAMGrayCorner(PIX* pixs, l_float32 angle, l_uint8 grayval);
		PIX* pixRotateAMColorFast(PIX* pixs, l_float32 angle, l_uint32 colorval);
		PIX* pixRotateOrth(PIX* pixs, l_int32 quads);
		PIX* pixRotate180(PIX* pixd, PIX* pixs);
		PIX* pixRotate90(PIX* pixs, l_int32 direction);
		PIX* pixFlipLR(PIX* pixd, PIX* pixs);
		PIX* pixFlipTB(PIX* pixd, PIX* pixs);
		PIX* pixRotateShear(PIX* pixs, l_int32 xcen, l_int32 ycen, l_float32 angle, l_int32 incolor);
		PIX* pixRotate2Shear(PIX* pixs, l_int32 xcen, l_int32 ycen, l_float32 angle, l_int32 incolor);
		PIX* pixRotate3Shear(PIX* pixs, l_int32 xcen, l_int32 ycen, l_float32 angle, l_int32 incolor);
		l_ok pixRotateShearIP(PIX* pixs, l_int32 xcen, l_int32 ycen, l_float32 angle, l_int32 incolor);
		PIX* pixRotateShearCenter(PIX* pixs, l_float32 angle, l_int32 incolor);
		l_ok pixRotateShearCenterIP(PIX* pixs, l_float32 angle, l_int32 incolor);
		PIX* pixStrokeWidthTransform(PIX* pixs, l_int32 color, l_int32 depth, l_int32 nangles);
		PIX* pixRunlengthTransform(PIX* pixs, l_int32 color, l_int32 direction, l_int32 depth);
		l_ok pixFindHorizontalRuns(PIX* pix, l_int32 y, l_int32* xstart, l_int32* xend, l_int32* pn);
		l_ok pixFindVerticalRuns(PIX* pix, l_int32 x, l_int32* ystart, l_int32* yend, l_int32* pn);
		NUMA* pixFindMaxRuns(PIX* pix, l_int32 direction, NUMA** pnastart);
		l_ok pixFindMaxHorizontalRunOnLine(PIX* pix, l_int32 y, l_int32* pxstart, l_int32* psize);
		l_ok pixFindMaxVerticalRunOnLine(PIX* pix, l_int32 x, l_int32* pystart, l_int32* psize);

		PIX* pixScale(PIX* pixs, l_float32 scalex, l_float32 scaley);
		PIX* pixScaleToSizeRel(PIX* pixs, l_int32 delw, l_int32 delh);
		PIX* pixScaleToSize(PIX* pixs, l_int32 wd, l_int32 hd);
		PIX* pixScaleToResolution(PIX* pixs, l_float32 target, l_float32 assumed, l_float32* pscalefact);
		PIX* pixScaleGeneral(PIX* pixs, l_float32 scalex, l_float32 scaley, l_float32 sharpfract, l_int32 sharpwidth);
		PIX* pixScaleLI(PIX* pixs, l_float32 scalex, l_float32 scaley);
		PIX* pixScaleColorLI(PIX* pixs, l_float32 scalex, l_float32 scaley);
		PIX* pixScaleColor2xLI(PIX* pixs);
		PIX* pixScaleColor4xLI(PIX* pixs);
		PIX* pixScaleGrayLI(PIX* pixs, l_float32 scalex, l_float32 scaley);
		PIX* pixScaleGray2xLI(PIX* pixs);
		PIX* pixScaleGray4xLI(PIX* pixs);
		PIX* pixScaleGray2xLIThresh(PIX* pixs, l_int32 thresh);
		PIX* pixScaleGray2xLIDither(PIX* pixs);
		PIX* pixScaleGray4xLIThresh(PIX* pixs, l_int32 thresh);
		PIX* pixScaleGray4xLIDither(PIX* pixs);
		PIX* pixScaleBySampling(PIX* pixs, l_float32 scalex, l_float32 scaley);
		PIX* pixScaleBySamplingWithShift(PIX* pixs, l_float32 scalex, l_float32 scaley, l_float32 shiftx, l_float32 shifty);
		PIX* pixScaleBySamplingToSize(PIX* pixs, l_int32 wd, l_int32 hd);
		PIX* pixScaleByIntSampling(PIX* pixs, l_int32 factor);
		PIX* pixScaleRGBToGrayFast(PIX* pixs, l_int32 factor, l_int32 color);
		PIX* pixScaleRGBToBinaryFast(PIX* pixs, l_int32 factor, l_int32 thresh);
		PIX* pixScaleGrayToBinaryFast(PIX* pixs, l_int32 factor, l_int32 thresh);
		PIX* pixScaleSmooth(PIX* pix, l_float32 scalex, l_float32 scaley);
		PIX* pixScaleSmoothToSize(PIX* pixs, l_int32 wd, l_int32 hd);
		PIX* pixScaleRGBToGray2(PIX* pixs, l_float32 rwt, l_float32 gwt, l_float32 bwt);
		PIX* pixScaleAreaMap(PIX* pix, l_float32 scalex, l_float32 scaley);
		PIX* pixScaleAreaMap2(PIX* pix);
		PIX* pixScaleAreaMapToSize(PIX* pixs, l_int32 wd, l_int32 hd);
		PIX* pixScaleBinary(PIX* pixs, l_float32 scalex, l_float32 scaley);
		PIX* pixScaleBinaryWithShift(PIX* pixs, l_float32 scalex, l_float32 scaley, l_float32 shiftx, l_float32 shifty);
		PIX* pixScaleToGray(PIX* pixs, l_float32 scalefactor);
		PIX* pixScaleToGrayFast(PIX* pixs, l_float32 scalefactor);
		PIX* pixScaleToGray2(PIX* pixs);
		PIX* pixScaleToGray3(PIX* pixs);
		PIX* pixScaleToGray4(PIX* pixs);
		PIX* pixScaleToGray6(PIX* pixs);
		PIX* pixScaleToGray8(PIX* pixs);
		PIX* pixScaleToGray16(PIX* pixs);
		PIX* pixScaleToGrayMipmap(PIX* pixs, l_float32 scalefactor);
		PIX* pixScaleMipmap(PIX* pixs1, PIX* pixs2, l_float32 scale);
		PIX* pixExpandReplicate(PIX* pixs, l_int32 factor);
		PIX* pixScaleGrayMinMax(PIX* pixs, l_int32 xfact, l_int32 yfact, l_int32 type);
		PIX* pixScaleGrayMinMax2(PIX* pixs, l_int32 type);
		PIX* pixScaleGrayRankCascade(PIX* pixs, l_int32 level1, l_int32 level2, l_int32 level3, l_int32 level4);
		PIX* pixScaleGrayRank2(PIX* pixs, l_int32 rank);
		l_ok pixScaleAndTransferAlpha(PIX* pixd, PIX* pixs, l_float32 scalex, l_float32 scaley);
		PIX* pixScaleWithAlpha(PIX* pixs, l_float32 scalex, l_float32 scaley, PIX* pixg, l_float32 fract);
		PIX* pixSeedfillBinary(PIX* pixd, PIX* pixs, PIX* pixm, l_int32 connectivity);
		PIX* pixSeedfillBinaryRestricted(PIX* pixd, PIX* pixs, PIX* pixm, l_int32 connectivity, l_int32 xmax, l_int32 ymax);
		PIX* pixHolesByFilling(PIX* pixs, l_int32 connectivity);
		PIX* pixFillClosedBorders(PIX* pixs, l_int32 connectivity);
		PIX* pixExtractBorderConnComps(PIX* pixs, l_int32 connectivity);
		PIX* pixRemoveBorderConnComps(PIX* pixs, l_int32 connectivity);
		PIX* pixFillBgFromBorder(PIX* pixs, l_int32 connectivity);
		PIX* pixFillHolesToBoundingRect(PIX* pixs, l_int32 minsize, l_float32 maxhfract, l_float32 minfgfract);
		l_ok pixSeedfillGray(PIX* pixs, PIX* pixm, l_int32 connectivity);
		l_ok pixSeedfillGrayInv(PIX* pixs, PIX* pixm, l_int32 connectivity);
		l_ok pixSeedfillGraySimple(PIX* pixs, PIX* pixm, l_int32 connectivity);
		l_ok pixSeedfillGrayInvSimple(PIX* pixs, PIX* pixm, l_int32 connectivity);
		PIX* pixSeedfillGrayBasin(PIX* pixb, PIX* pixm, l_int32 delta, l_int32 connectivity);
		PIX* pixDistanceFunction(PIX* pixs, l_int32 connectivity, l_int32 outdepth, l_int32 boundcond);
		PIX* pixSeedspread(PIX* pixs, l_int32 connectivity);
		l_ok pixLocalExtrema(PIX* pixs, l_int32 maxmin, l_int32 minmax, PIX** ppixmin, PIX** ppixmax);
		l_ok pixSelectedLocalExtrema(PIX* pixs, l_int32 mindist, PIX** ppixmin, PIX** ppixmax);
		PIX* pixFindEqualValues(PIX* pixs1, PIX* pixs2);
		l_ok pixSelectMinInConnComp(PIX* pixs, PIX* pixm, PTA** ppta, NUMA** pnav);
		PIX* pixRemoveSeededComponents(PIX* pixd, PIX* pixs, PIX* pixm, l_int32 connectivity, l_int32 bordersize);

		SEL* selCreateFromPix(PIX* pix, l_int32 cy, l_int32 cx, const char* name);

		SEL* selCreateFromColorPix(PIX* pixs, const char* selname);
		SELA* selaCreateFromColorPixa(PIXA* pixa, SARRAY* sa);
		PIX* selDisplayInPix(SEL* sel, l_int32 size, l_int32 gthick);
		PIX* selaDisplayInPix(SELA* sela, l_int32 size, l_int32 gthick, l_int32 spacing, l_int32 ncols);

		SEL* pixGenerateSelWithRuns(PIX* pixs, l_int32 nhlines, l_int32 nvlines, l_int32 distance, l_int32 minlength, l_int32 toppix, l_int32 botpix, l_int32 leftpix, l_int32 rightpix, PIX** ppixe);
		SEL* pixGenerateSelRandom(PIX* pixs, l_float32 hitfract, l_float32 missfract, l_int32 distance, l_int32 toppix, l_int32 botpix, l_int32 leftpix, l_int32 rightpix, PIX** ppixe);
		SEL* pixGenerateSelBoundary(PIX* pixs, l_int32 hitdist, l_int32 missdist, l_int32 hitskip, l_int32 missskip, l_int32 topflag, l_int32 botflag, l_int32 leftflag, l_int32 rightflag, PIX** ppixe);
		NUMA* pixGetRunCentersOnLine(PIX* pixs, l_int32 x, l_int32 y, l_int32 minlength);
		NUMA* pixGetRunsOnLine(PIX* pixs, l_int32 x1, l_int32 y1, l_int32 x2, l_int32 y2);
		PTA* pixSubsampleBoundaryPixels(PIX* pixs, l_int32 skip);
		l_int32 adjacentOnPixelInRaster(PIX* pixs, l_int32 x, l_int32 y, l_int32* pxa, l_int32* pya);
		PIX* pixDisplayHitMissSel(PIX* pixs, SEL* sel, l_int32 scalefactor, l_uint32 hitcolor, l_uint32 misscolor);
		PIX* pixHShear(PIX* pixd, PIX* pixs, l_int32 yloc, l_float32 radang, l_int32 incolor);
		PIX* pixVShear(PIX* pixd, PIX* pixs, l_int32 xloc, l_float32 radang, l_int32 incolor);
		PIX* pixHShearCorner(PIX* pixd, PIX* pixs, l_float32 radang, l_int32 incolor);
		PIX* pixVShearCorner(PIX* pixd, PIX* pixs, l_float32 radang, l_int32 incolor);
		PIX* pixHShearCenter(PIX* pixd, PIX* pixs, l_float32 radang, l_int32 incolor);
		PIX* pixVShearCenter(PIX* pixd, PIX* pixs, l_float32 radang, l_int32 incolor);
		l_ok pixHShearIP(PIX* pixs, l_int32 yloc, l_float32 radang, l_int32 incolor);
		l_ok pixVShearIP(PIX* pixs, l_int32 xloc, l_float32 radang, l_int32 incolor);
		PIX* pixHShearLI(PIX* pixs, l_int32 yloc, l_float32 radang, l_int32 incolor);
		PIX* pixVShearLI(PIX* pixs, l_int32 xloc, l_float32 radang, l_int32 incolor);
		PIX* pixDeskewBoth(PIX* pixs, l_int32 redsearch);
		PIX* pixDeskew(PIX* pixs, l_int32 redsearch);
		PIX* pixFindSkewAndDeskew(PIX* pixs, l_int32 redsearch, l_float32* pangle, l_float32* pconf);
		PIX* pixDeskewGeneral(PIX* pixs, l_int32 redsweep, l_float32 sweeprange, l_float32 sweepdelta, l_int32 redsearch, l_int32 thresh, l_float32* pangle, l_float32* pconf);
		l_ok pixFindSkew(PIX* pixs, l_float32* pangle, l_float32* pconf);
		l_ok pixFindSkewSweep(PIX* pixs, l_float32* pangle, l_int32 reduction, l_float32 sweeprange, l_float32 sweepdelta);
		l_ok pixFindSkewSweepAndSearch(PIX* pixs, l_float32* pangle, l_float32* pconf, l_int32 redsweep, l_int32 redsearch, l_float32 sweeprange, l_float32 sweepdelta, l_float32 minbsdelta);
		l_ok pixFindSkewSweepAndSearchScore(PIX* pixs, l_float32* pangle, l_float32* pconf, l_float32* pendscore, l_int32 redsweep, l_int32 redsearch, l_float32 sweepcenter, l_float32 sweeprange, l_float32 sweepdelta, l_float32 minbsdelta);
		l_ok pixFindSkewSweepAndSearchScorePivot(PIX* pixs, l_float32* pangle, l_float32* pconf, l_float32* pendscore, l_int32 redsweep, l_int32 redsearch, l_float32 sweepcenter, l_float32 sweeprange, l_float32 sweepdelta, l_float32 minbsdelta, l_int32 pivot);
		l_int32 pixFindSkewOrthogonalRange(PIX* pixs, l_float32* pangle, l_float32* pconf, l_int32 redsweep, l_int32 redsearch, l_float32 sweeprange, l_float32 sweepdelta, l_float32 minbsdelta, l_float32 confprior);
		l_ok pixFindDifferentialSquareSum(PIX* pixs, l_float32* psum);
		l_ok pixFindNormalizedSquareSum(PIX* pixs, l_float32* phratio, l_float32* pvratio, l_float32* pfract);
		PIX* pixReadStreamSpix(FILE* fp);

		l_ok pixWriteStreamSpix(FILE* fp, PIX* pix);
		PIX* pixReadMemSpix(const l_uint8* data, size_t size);
		l_ok pixWriteMemSpix(l_uint8** pdata, size_t* psize, PIX* pix);
		l_ok pixSerializeToMemory(PIX* pixs, l_uint32** pdata, size_t* pnbytes);
		PIX* pixDeserializeFromMemory(const l_uint32* data, size_t nbytes);

		l_ok pixFindStrokeLength(PIX* pixs, l_int32* tab8, l_int32* plength);
		l_ok pixFindStrokeWidth(PIX* pixs, l_float32 thresh, l_int32* tab8, l_float32* pwidth, NUMA** pnahisto);
		NUMA* pixaFindStrokeWidth(PIXA* pixa, l_float32 thresh, l_int32* tab8, l_int32 debug);
		PIXA* pixaModifyStrokeWidth(PIXA* pixas, l_float32 targetw);
		PIX* pixModifyStrokeWidth(PIX* pixs, l_float32 width, l_float32 targetw);
		PIXA* pixaSetStrokeWidth(PIXA* pixas, l_int32 width, l_int32 thinfirst, l_int32 connectivity);
		PIX* pixSetStrokeWidth(PIX* pixs, l_int32 width, l_int32 thinfirst, l_int32 connectivity);

		PIX* pixAddSingleTextblock(PIX* pixs, L_BMF* bmf, const char* textstr, l_uint32 val, l_int32 location, l_int32* poverflow);
		PIX* pixAddTextlines(PIX* pixs, L_BMF* bmf, const char* textstr, l_uint32 val, l_int32 location);
		l_ok pixSetTextblock(PIX* pixs, L_BMF* bmf, const char* textstr, l_uint32 val, l_int32 x0, l_int32 y0, l_int32 wtext, l_int32 firstindent, l_int32* poverflow);
		l_ok pixSetTextline(PIX* pixs, L_BMF* bmf, const char* textstr, l_uint32 val, l_int32 x0, l_int32 y0, l_int32* pwidth, l_int32* poverflow);
		PIXA* pixaAddTextNumber(PIXA* pixas, L_BMF* bmf, NUMA* na, l_uint32 val, l_int32 location);
		PIXA* pixaAddTextlines(PIXA* pixas, L_BMF* bmf, SARRAY* sa, l_uint32 val, l_int32 location);
		l_ok pixaAddPixWithText(PIXA* pixa, PIX* pixs, l_int32 reduction, L_BMF* bmf, const char* textstr, l_uint32 val, l_int32 location);

		PIX* pixReadTiff(const char* filename, l_int32 n);
		PIX* pixReadStreamTiff(FILE* fp, l_int32 n);
		l_ok pixWriteTiff(const char* filename, PIX* pix, l_int32 comptype, const char* modestring);
		l_ok pixWriteTiffCustom(const char* filename, PIX* pix, l_int32 comptype, const char* modestring, NUMA* natags, SARRAY* savals, SARRAY* satypes, NUMA* nasizes);
		l_ok pixWriteStreamTiff(FILE* fp, PIX* pix, l_int32 comptype);
		l_ok pixWriteStreamTiffWA(FILE* fp, PIX* pix, l_int32 comptype, const char* modestr);
		PIX* pixReadFromMultipageTiff(const char* filename, size_t* poffset);
		PIXA* pixaReadMultipageTiff(const char* filename);
		l_ok pixaWriteMultipageTiff(const char* filename, PIXA* pixa);

		PIX* pixReadMemTiff(const l_uint8* cdata, size_t size, l_int32 n);
		PIX* pixReadMemFromMultipageTiff(const l_uint8* cdata, size_t size, size_t* poffset);
		PIXA* pixaReadMemMultipageTiff(const l_uint8* data, size_t size);
		l_ok pixaWriteMemMultipageTiff(l_uint8** pdata, size_t* psize, PIXA* pixa);
		l_ok pixWriteMemTiff(l_uint8** pdata, size_t* psize, PIX* pix, l_int32 comptype);
		l_ok pixWriteMemTiffCustom(l_uint8** pdata, size_t* psize, PIX* pix, l_int32 comptype, NUMA* natags, SARRAY* savals, SARRAY* satypes, NUMA* nasizes);

		PIX* pixSimpleCaptcha(PIX* pixs, l_int32 border, l_int32 nterms, l_uint32 seed, l_uint32 color, l_int32 cmapflag);
		PIX* pixRandomHarmonicWarp(PIX* pixs, l_float32 xmag, l_float32 ymag, l_float32 xfreq, l_float32 yfreq, l_int32 nx, l_int32 ny, l_uint32 seed, l_int32 grayval);
		PIX* pixWarpStereoscopic(PIX* pixs, l_int32 zbend, l_int32 zshiftt, l_int32 zshiftb, l_int32 ybendt, l_int32 ybendb, l_int32 redleft);
		PIX* pixStretchHorizontal(PIX* pixs, l_int32 dir, l_int32 type, l_int32 hmax, l_int32 operation, l_int32 incolor);
		PIX* pixStretchHorizontalSampled(PIX* pixs, l_int32 dir, l_int32 type, l_int32 hmax, l_int32 incolor);
		PIX* pixStretchHorizontalLI(PIX* pixs, l_int32 dir, l_int32 type, l_int32 hmax, l_int32 incolor);
		PIX* pixQuadraticVShear(PIX* pixs, l_int32 dir, l_int32 vmaxt, l_int32 vmaxb, l_int32 operation, l_int32 incolor);
		PIX* pixQuadraticVShearSampled(PIX* pixs, l_int32 dir, l_int32 vmaxt, l_int32 vmaxb, l_int32 incolor);
		PIX* pixQuadraticVShearLI(PIX* pixs, l_int32 dir, l_int32 vmaxt, l_int32 vmaxb, l_int32 incolor);
		PIX* pixStereoFromPair(PIX* pix1, PIX* pix2, l_float32 rwt, l_float32 gwt, l_float32 bwt);
		L_WSHED* wshedCreate(PIX* pixs, PIX* pixm, l_int32 mindepth, l_int32 debugflag);

		l_ok wshedBasins(L_WSHED* wshed, PIXA** ppixa, NUMA** pnalevels);
		PIX* wshedRenderFill(L_WSHED* wshed);
		PIX* wshedRenderColors(L_WSHED* wshed);
		l_ok pixaWriteWebPAnim(const char* filename, PIXA* pixa, l_int32 loopcount, l_int32 duration, l_int32 quality, l_int32 lossless);
		l_ok pixaWriteStreamWebPAnim(FILE* fp, PIXA* pixa, l_int32 loopcount, l_int32 duration, l_int32 quality, l_int32 lossless);
		l_ok pixaWriteMemWebPAnim(l_uint8** pencdata, size_t* pencsize, PIXA* pixa, l_int32 loopcount, l_int32 duration, l_int32 quality, l_int32 lossless);
		PIX* pixReadStreamWebP(FILE* fp);
		PIX* pixReadMemWebP(const l_uint8* filedata, size_t filesize);

		l_ok pixWriteWebP(const char* filename, PIX* pixs, l_int32 quality, l_int32 lossless);
		l_ok pixWriteStreamWebP(FILE* fp, PIX* pixs, l_int32 quality, l_int32 lossless);
		l_ok pixWriteMemWebP(l_uint8** pencdata, size_t* pencsize, PIX* pixs, l_int32 quality, l_int32 lossless);

		l_ok pixaWriteFiles(const char* rootname, PIXA* pixa, l_int32 format);
		l_ok pixWriteDebug(const char* fname, PIX* pix, l_int32 format);
		l_ok pixWrite(const char* fname, PIX* pix, l_int32 format);
		l_ok pixWriteAutoFormat(const char* filename, PIX* pix);
		l_ok pixWriteStream(FILE* fp, PIX* pix, l_int32 format);
		l_ok pixWriteImpliedFormat(const char* filename, PIX* pix, l_int32 quality, l_int32 progressive);
		l_int32 pixChooseOutputFormat(PIX* pix);

		l_ok pixGetAutoFormat(PIX* pix, l_int32* pformat);

		l_ok pixWriteMem(l_uint8** pdata, size_t* psize, PIX* pix, l_int32 format);

		l_ok pixDisplay(PIX* pixs, l_int32 x, l_int32 y);
		l_ok pixDisplayWithTitle(PIX* pixs, l_int32 x, l_int32 y, const char* title, l_int32 dispflag);
		PIX* pixMakeColorSquare(l_uint32 color, l_int32 size, l_int32 addlabel, l_int32 location, l_uint32 textcolor);

		l_ok pixDisplayWrite(PIX* pixs, l_int32 reduction);






/*

		PIX* pixCleanBackgroundToWhite(PIX* pixs, PIX* pixim, PIX* pixg, l_float32 gamma, l_int32 blackval, l_int32 whiteval);
		PIX* pixBackgroundNormSimple(PIX* pixs, PIX* pixim, PIX* pixg);
		PIX* pixBackgroundNorm(PIX* pixs, PIX* pixim, PIX* pixg, l_int32 sx, l_int32 sy, l_int32 thresh, l_int32 mincount, l_int32 bgval, l_int32 smoothx, l_int32 smoothy);
		PIX* pixBackgroundNormMorph(PIX* pixs, PIX* pixim, l_int32 reduction, l_int32 size, l_int32 bgval);
		l_ok pixBackgroundNormGrayArray(PIX* pixs, PIX* pixim, l_int32 sx, l_int32 sy, l_int32 thresh, l_int32 mincount, l_int32 bgval, l_int32 smoothx, l_int32 smoothy, PIX** ppixd);
		l_ok pixBackgroundNormRGBArrays(PIX* pixs, PIX* pixim, PIX* pixg, l_int32 sx, l_int32 sy, l_int32 thresh, l_int32 mincount, l_int32 bgval, l_int32 smoothx, l_int32 smoothy, PIX** ppixr, PIX** ppixg, PIX** ppixb);
		l_ok pixBackgroundNormGrayArrayMorph(PIX* pixs, PIX* pixim, l_int32 reduction, l_int32 size, l_int32 bgval, PIX** ppixd);
		l_ok pixBackgroundNormRGBArraysMorph(PIX* pixs, PIX* pixim, l_int32 reduction, l_int32 size, l_int32 bgval, PIX** ppixr, PIX** ppixg, PIX** ppixb);
		l_ok pixGetBackgroundGrayMap(PIX* pixs, PIX* pixim, l_int32 sx, l_int32 sy, l_int32 thresh, l_int32 mincount, PIX** ppixd);
		l_ok pixGetBackgroundRGBMap(PIX* pixs, PIX* pixim, PIX* pixg, l_int32 sx, l_int32 sy, l_int32 thresh, l_int32 mincount, PIX** ppixmr, PIX** ppixmg, PIX** ppixmb);
		l_ok pixGetBackgroundGrayMapMorph(PIX* pixs, PIX* pixim, l_int32 reduction, l_int32 size, PIX** ppixm);
		l_ok pixGetBackgroundRGBMapMorph(PIX* pixs, PIX* pixim, l_int32 reduction, l_int32 size, PIX** ppixmr, PIX** ppixmg, PIX** ppixmb);
		l_ok pixFillMapHoles(PIX* pix, l_int32 nx, l_int32 ny, l_int32 filltype);
		PIX* pixExtendByReplication(PIX* pixs, l_int32 addw, l_int32 addh);
		l_ok pixSmoothConnectedRegions(PIX* pixs, PIX* pixm, l_int32 factor);
		PIX* pixGetInvBackgroundMap(PIX* pixs, l_int32 bgval, l_int32 smoothx, l_int32 smoothy);
		PIX* pixApplyInvBackgroundGrayMap(PIX* pixs, PIX* pixm, l_int32 sx, l_int32 sy);
		PIX* pixApplyInvBackgroundRGBMap(PIX* pixs, PIX* pixmr, PIX* pixmg, PIX* pixmb, l_int32 sx, l_int32 sy);
		PIX* pixApplyVariableGrayMap(PIX* pixs, PIX* pixg, l_int32 target);
		PIX* pixGlobalNormRGB(PIX* pixd, PIX* pixs, l_int32 rval, l_int32 gval, l_int32 bval, l_int32 mapval);
		PIX* pixGlobalNormNoSatRGB(PIX* pixd, PIX* pixs, l_int32 rval, l_int32 gval, l_int32 bval, l_int32 factor, l_float32 rank);
		l_ok pixThresholdSpreadNorm(PIX* pixs, l_int32 filtertype, l_int32 edgethresh, l_int32 smoothx, l_int32 smoothy, l_float32 gamma, l_int32 minval, l_int32 maxval, l_int32 targetthresh, PIX** ppixth, PIX** ppixb, PIX** ppixd);
		PIX* pixBackgroundNormFlex(PIX* pixs, l_int32 sx, l_int32 sy, l_int32 smoothx, l_int32 smoothy, l_int32 delta);
		PIX* pixContrastNorm(PIX* pixd, PIX* pixs, l_int32 sx, l_int32 sy, l_int32 mindiff, l_int32 smoothx, l_int32 smoothy);
		PIX* pixBackgroundNormTo1MinMax(PIX* pixs, l_int32 contrast, l_int32 scalefactor);
		PIX* pixConvertTo8MinMax(PIX* pixs);
		PIX* pixAffineSampledPta(PIX* pixs, PTA* ptad, PTA* ptas, l_int32 incolor);
		PIX* pixAffineSampled(PIX* pixs, l_float32* vc, l_int32 incolor);
		PIX* pixAffinePta(PIX* pixs, PTA* ptad, PTA* ptas, l_int32 incolor);
		PIX* pixAffine(PIX* pixs, l_float32* vc, l_int32 incolor);
		PIX* pixAffinePtaColor(PIX* pixs, PTA* ptad, PTA* ptas, l_uint32 colorval);
		PIX* pixAffineColor(PIX* pixs, l_float32* vc, l_uint32 colorval);
		PIX* pixAffinePtaGray(PIX* pixs, PTA* ptad, PTA* ptas, l_uint8 grayval);
		PIX* pixAffineGray(PIX* pixs, l_float32* vc, l_uint8 grayval);
		PIX* pixAffinePtaWithAlpha(PIX* pixs, PTA* ptad, PTA* ptas, PIX* pixg, l_float32 fract, l_int32 border);

		l_ok linearInterpolatePixelColor(l_uint32* datas, l_int32 wpls, l_int32 w, l_int32 h, l_float32 x, l_float32 y, l_uint32 colorval, l_uint32* pval);
		l_ok linearInterpolatePixelGray(l_uint32* datas, l_int32 wpls, l_int32 w, l_int32 h, l_float32 x, l_float32 y, l_int32 grayval, l_int32* pval);

		PIX* pixAffineSequential(PIX* pixs, PTA* ptad, PTA* ptas, l_int32 bw, l_int32 bh);

		NUMA* pixFindBaselines(PIX* pixs, PTA** ppta, PIXA* pixadb);
		PIX* pixDeskewLocal(PIX* pixs, l_int32 nslices, l_int32 redsweep, l_int32 redsearch, l_float32 sweeprange, l_float32 sweepdelta, l_float32 minbsdelta);
		l_ok pixGetLocalSkewTransform(PIX* pixs, l_int32 nslices, l_int32 redsweep, l_int32 redsearch, l_float32 sweeprange, l_float32 sweepdelta, l_float32 minbsdelta, PTA** pptas, PTA** pptad);
		NUMA* pixGetLocalSkewAngles(PIX* pixs, l_int32 nslices, l_int32 redsweep, l_int32 redsearch, l_float32 sweeprange, l_float32 sweepdelta, l_float32 minbsdelta, l_float32* pa, l_float32* pb, l_int32 debug);

		PIX* pixBilateral(PIX* pixs, l_float32 spatial_stdev, l_float32 range_stdev, l_int32 ncomps, l_int32 reduction);
		PIX* pixBilateralGray(PIX* pixs, l_float32 spatial_stdev, l_float32 range_stdev, l_int32 ncomps, l_int32 reduction);
		PIX* pixBilateralExact(PIX* pixs, L_KERNEL* spatial_kel, L_KERNEL* range_kel);
		PIX* pixBilateralGrayExact(PIX* pixs, L_KERNEL* spatial_kel, L_KERNEL* range_kel);
		PIX* pixBlockBilateralExact(PIX* pixs, l_float32 spatial_stdev, l_float32 range_stdev);
		PIX* pixBilinearSampledPta(PIX* pixs, PTA* ptad, PTA* ptas, l_int32 incolor);
		PIX* pixBilinearSampled(PIX* pixs, l_float32* vc, l_int32 incolor);
		PIX* pixBilinearPta(PIX* pixs, PTA* ptad, PTA* ptas, l_int32 incolor);
		PIX* pixBilinear(PIX* pixs, l_float32* vc, l_int32 incolor);
		PIX* pixBilinearPtaColor(PIX* pixs, PTA* ptad, PTA* ptas, l_uint32 colorval);
		PIX* pixBilinearColor(PIX* pixs, l_float32* vc, l_uint32 colorval);
		PIX* pixBilinearPtaGray(PIX* pixs, PTA* ptad, PTA* ptas, l_uint8 grayval);
		PIX* pixBilinearGray(PIX* pixs, l_float32* vc, l_uint8 grayval);
		PIX* pixBilinearPtaWithAlpha(PIX* pixs, PTA* ptad, PTA* ptas, PIX* pixg, l_float32 fract, l_int32 border);

		l_ok pixOtsuAdaptiveThreshold(PIX* pixs, l_int32 sx, l_int32 sy, l_int32 smoothx, l_int32 smoothy, l_float32 scorefract, PIX** ppixth, PIX** ppixd);
		PIX* pixOtsuThreshOnBackgroundNorm(PIX* pixs, PIX* pixim, l_int32 sx, l_int32 sy, l_int32 thresh, l_int32 mincount, l_int32 bgval, l_int32 smoothx, l_int32 smoothy, l_float32 scorefract, l_int32* pthresh);
		PIX* pixMaskedThreshOnBackgroundNorm(PIX* pixs, PIX* pixim, l_int32 sx, l_int32 sy, l_int32 thresh, l_int32 mincount, l_int32 smoothx, l_int32 smoothy, l_float32 scorefract, l_int32* pthresh);
		l_ok pixSauvolaBinarizeTiled(PIX* pixs, l_int32 whsize, l_float32 factor, l_int32 nx, l_int32 ny, PIX** ppixth, PIX** ppixd);
		l_ok pixSauvolaBinarize(PIX* pixs, l_int32 whsize, l_float32 factor, l_int32 addborder, PIX** ppixm, PIX** ppixsd, PIX** ppixth, PIX** ppixd);
		PIX* pixSauvolaOnContrastNorm(PIX* pixs, l_int32 mindiff, PIX** ppixn, PIX** ppixth);
		PIX* pixThreshOnDoubleNorm(PIX* pixs, l_int32 mindiff);
		l_ok pixThresholdByConnComp(PIX* pixs, PIX* pixm, l_int32 start, l_int32 end, l_int32 incr, l_float32 thresh48, l_float32 threshdiff, l_int32* pglobthresh, PIX** ppixd, l_int32 debugflag);
		l_ok pixThresholdByHisto(PIX* pixs, l_int32 factor, l_int32 halfw, l_int32 skip, l_int32* pthresh, PIX** ppixd, NUMA** pnahisto, PIX** ppixhisto);
		PIX* pixExpandBinaryReplicate(PIX* pixs, l_int32 xfact, l_int32 yfact);
		PIX* pixExpandBinaryPower2(PIX* pixs, l_int32 factor);
		PIX* pixReduceBinary2(PIX* pixs, l_uint8* intab);
		PIX* pixReduceRankBinaryCascade(PIX* pixs, l_int32 level1, l_int32 level2, l_int32 level3, l_int32 level4);
		PIX* pixReduceRankBinary2(PIX* pixs, l_int32 level, l_uint8* intab);

		PIX* pixBlend(PIX* pixs1, PIX* pixs2, l_int32 x, l_int32 y, l_float32 fract);
		PIX* pixBlendMask(PIX* pixd, PIX* pixs1, PIX* pixs2, l_int32 x, l_int32 y, l_float32 fract, l_int32 type);
		PIX* pixBlendGray(PIX* pixd, PIX* pixs1, PIX* pixs2, l_int32 x, l_int32 y, l_float32 fract, l_int32 type, l_int32 transparent, l_uint32 transpix);
		PIX* pixBlendGrayInverse(PIX* pixd, PIX* pixs1, PIX* pixs2, l_int32 x, l_int32 y, l_float32 fract);
		PIX* pixBlendColor(PIX* pixd, PIX* pixs1, PIX* pixs2, l_int32 x, l_int32 y, l_float32 fract, l_int32 transparent, l_uint32 transpix);
		PIX* pixBlendColorByChannel(PIX* pixd, PIX* pixs1, PIX* pixs2, l_int32 x, l_int32 y, l_float32 rfract, l_float32 gfract, l_float32 bfract, l_int32 transparent, l_uint32 transpix);
		PIX* pixBlendGrayAdapt(PIX* pixd, PIX* pixs1, PIX* pixs2, l_int32 x, l_int32 y, l_float32 fract, l_int32 shift);
		PIX* pixFadeWithGray(PIX* pixs, PIX* pixb, l_float32 factor, l_int32 type);
		PIX* pixBlendHardLight(PIX* pixd, PIX* pixs1, PIX* pixs2, l_int32 x, l_int32 y, l_float32 fract);
		l_ok pixBlendCmap(PIX* pixs, PIX* pixb, l_int32 x, l_int32 y, l_int32 sindex);
		PIX* pixBlendWithGrayMask(PIX* pixs1, PIX* pixs2, PIX* pixg, l_int32 x, l_int32 y);
		PIX* pixBlendBackgroundToColor(PIX* pixd, PIX* pixs, BOX* box, l_uint32 color, l_float32 gamma, l_int32 minval, l_int32 maxval);
		PIX* pixMultiplyByColor(PIX* pixd, PIX* pixs, BOX* box, l_uint32 color);
		PIX* pixAlphaBlendUniform(PIX* pixs, l_uint32 color);
		PIX* pixAddAlphaToBlend(PIX* pixs, l_float32 fract, l_int32 invert);
		PIX* pixSetAlphaOverWhite(PIX* pixs);
		l_ok pixLinearEdgeFade(PIX* pixs, l_int32 dir, l_int32 fadeto, l_float32 distfract, l_float32 maxfade);

		L_BMF* bmfCreate(const char* dir, l_int32 fontsize);
		void bmfDestroy(L_BMF** pbmf);
		PIX* bmfGetPix(L_BMF* bmf, char chr);

		PIXA* pixaGetFont(const char* dir, l_int32 fontsize, l_int32* pbl0, l_int32* pbl1, l_int32* pbl2);

		PIX* pixReadStreamBmp(FILE* fp);
		l_ok pixWriteStreamBmp(FILE* fp, PIX* pix);

		PIX* pixReadMemBmp(const l_uint8* cdata, size_t size);
		l_ok pixWriteMemBmp(l_uint8** pdata, size_t* psize, PIX* pix);

		BOXA* boxaCombineOverlaps(BOXA* boxas, PIXA* pixadb);
		l_ok boxaCombineOverlapsInPair(BOXA* boxas1, BOXA* boxas2, BOXA** pboxad1, BOXA** pboxad2, PIXA* pixadb);

		PIX* pixMaskConnComp(PIX* pixs, l_int32 connectivity, BOXA** pboxa);
		PIX* pixMaskBoxa(PIX* pixd, PIX* pixs, BOXA* boxa, l_int32 op);
		PIX* pixPaintBoxa(PIX* pixs, BOXA* boxa, l_uint32 val);
		PIX* pixSetBlackOrWhiteBoxa(PIX* pixs, BOXA* boxa, l_int32 op);
		PIX* pixPaintBoxaRandom(PIX* pixs, BOXA* boxa);
		PIX* pixBlendBoxaRandom(PIX* pixs, BOXA* boxa, l_float32 fract);
		PIX* pixDrawBoxa(PIX* pixs, BOXA* boxa, l_int32 width, l_uint32 val);
		PIX* pixDrawBoxaRandom(PIX* pixs, BOXA* boxa, l_int32 width);
		PIX* boxaaDisplay(PIX* pixs, BOXAA* baa, l_int32 linewba, l_int32 linewb, l_uint32 colorba, l_uint32 colorb, l_int32 w, l_int32 h);
		PIXA* pixaDisplayBoxaa(PIXA* pixas, BOXAA* baa, l_int32 colorflag, l_int32 width);
		BOXA* pixSplitIntoBoxa(PIX* pixs, l_int32 minsum, l_int32 skipdist, l_int32 delta, l_int32 maxbg, l_int32 maxcomps, l_int32 remainder);
		BOXA* pixSplitComponentIntoBoxa(PIX* pix, BOX* box, l_int32 minsum, l_int32 skipdist, l_int32 delta, l_int32 maxbg, l_int32 maxcomps, l_int32 remainder);

		l_ok boxaCompareRegions(BOXA* boxa1, BOXA* boxa2, l_int32 areathresh, l_int32* pnsame, l_float32* pdiffarea, l_float32* pdiffxor, PIX** ppixdb);
		BOX* pixSelectLargeULComp(PIX* pixs, l_float32 areaslop, l_int32 yslop, l_int32 connectivity);

		PIX* boxaDisplayTiled(BOXA* boxas, PIXA* pixa, l_int32 first, l_int32 last, l_int32 maxwidth, l_int32 linewidth, l_float32 scalefactor, l_int32 background, l_int32 spacing, l_int32 border);

		BOXA* boxaReconcileAllByMedian(BOXA* boxas, l_int32 select1, l_int32 select2, l_int32 thresh, l_int32 extra, PIXA* pixadb);
		BOXA* boxaReconcileSidesByMedian(BOXA* boxas, l_int32 select, l_int32 thresh, l_int32 extra, PIXA* pixadb);

		l_ok boxaPlotSides(BOXA* boxa, const char* plotname, NUMA** pnal, NUMA** pnat, NUMA** pnar, NUMA** pnab, PIX** ppixd);
		l_ok boxaPlotSizes(BOXA* boxa, const char* plotname, NUMA** pnaw, NUMA** pnah, PIX** ppixd);

		CCBORDA* pixGetAllCCBorders(PIX* pixs);
		PTAA* pixGetOuterBordersPtaa(PIX* pixs);
		l_ok pixGetOuterBorder(CCBORD* ccb, PIX* pixs, BOX* box);

		PIX* ccbaDisplayBorder(CCBORDA* ccba);
		PIX* ccbaDisplaySPBorder(CCBORDA* ccba);
		PIX* ccbaDisplayImage1(CCBORDA* ccba);
		PIX* ccbaDisplayImage2(CCBORDA* ccba);

		PIXA* pixaThinConnected(PIXA* pixas, l_int32 type, l_int32 connectivity, l_int32 maxiters);
		PIX* pixThinConnected(PIX* pixs, l_int32 type, l_int32 connectivity, l_int32 maxiters);
		PIX* pixThinConnectedBySet(PIX* pixs, l_int32 type, SELA* sela, l_int32 maxiters);

		l_ok pixFindCheckerboardCorners(PIX* pixs, l_int32 size, l_int32 dilation, l_int32 nsels, PIX** ppix_corners, PTA** ppta_corners, PIXA* pixadb);

		l_ok pixGetWordsInTextlines(PIX* pixs, l_int32 minwidth, l_int32 minheight, l_int32 maxwidth, l_int32 maxheight, BOXA** pboxad, PIXA** ppixad, NUMA** pnai);
		l_ok pixGetWordBoxesInTextlines(PIX* pixs, l_int32 minwidth, l_int32 minheight, l_int32 maxwidth, l_int32 maxheight, BOXA** pboxad, NUMA** pnai);
		l_ok pixFindWordAndCharacterBoxes(PIX* pixs, BOX* boxs, l_int32 thresh, BOXA** pboxaw, BOXAA** pboxaac, const char* debugdir);

		l_ok pixColorContent(PIX* pixs, l_int32 rref, l_int32 gref, l_int32 bref, l_int32 mingray, PIX** ppixr, PIX** ppixg, PIX** ppixb);
		PIX* pixColorMagnitude(PIX* pixs, l_int32 rref, l_int32 gref, l_int32 bref, l_int32 type);
		l_ok pixColorFraction(PIX* pixs, l_int32 darkthresh, l_int32 lightthresh, l_int32 diffthresh, l_int32 factor, l_float32* ppixfract, l_float32* pcolorfract);
		PIX* pixColorShiftWhitePoint(PIX* pixs, l_int32 rref, l_int32 gref, l_int32 bref);
		PIX* pixMaskOverColorPixels(PIX* pixs, l_int32 threshdiff, l_int32 mindist);
		PIX* pixMaskOverGrayPixels(PIX* pixs, l_int32 maxlimit, l_int32 satlimit);
		PIX* pixMaskOverColorRange(PIX* pixs, l_int32 rmin, l_int32 rmax, l_int32 gmin, l_int32 gmax, l_int32 bmin, l_int32 bmax);
		l_ok pixFindColorRegions(PIX* pixs, PIX* pixm, l_int32 factor, l_int32 lightthresh, l_int32 darkthresh, l_int32 mindiff, l_int32 colordiff, l_float32 edgefract, l_float32* pcolorfract, PIX** pcolormask1, PIX** pcolormask2, PIXA* pixadb);
		l_ok pixNumSignificantGrayColors(PIX* pixs, l_int32 darkthresh, l_int32 lightthresh, l_float32 minfract, l_int32 factor, l_int32* pncolors);
		l_ok pixColorsForQuantization(PIX* pixs, l_int32 thresh, l_int32* pncolors, l_int32* piscolor, l_int32 debug);
		l_ok pixNumColors(PIX* pixs, l_int32 factor, l_int32* pncolors);
		PIX* pixConvertRGBToCmapLossless(PIX* pixs);
		l_ok pixGetMostPopulatedColors(PIX* pixs, l_int32 sigbits, l_int32 factor, l_int32 ncolors, l_uint32** parray, PIXCMAP** pcmap);
		PIX* pixSimpleColorQuantize(PIX* pixs, l_int32 sigbits, l_int32 factor, l_int32 ncolors);
		NUMA* pixGetRGBHistogram(PIX* pixs, l_int32 sigbits, l_int32 factor);

		l_ok pixHasHighlightRed(PIX* pixs, l_int32 factor, l_float32 minfract, l_float32 fthresh, l_int32* phasred, l_float32* pratio, PIX** ppixdb);
		L_COLORFILL* l_colorfillCreate(PIX* pixs, l_int32 nx, l_int32 ny);

		PIX* pixColorFill(PIX* pixs, l_int32 minmax, l_int32 maxdiff, l_int32 smooth, l_int32 minarea, l_int32 debug);
		PIXA* makeColorfillTestData(l_int32 w, l_int32 h, l_int32 nseeds, l_int32 range);
		PIX* pixColorGrayRegions(PIX* pixs, BOXA* boxa, l_int32 type, l_int32 thresh, l_int32 rval, l_int32 gval, l_int32 bval);
		l_ok pixColorGray(PIX* pixs, BOX* box, l_int32 type, l_int32 thresh, l_int32 rval, l_int32 gval, l_int32 bval);
		PIX* pixColorGrayMasked(PIX* pixs, PIX* pixm, l_int32 type, l_int32 thresh, l_int32 rval, l_int32 gval, l_int32 bval);
		PIX* pixSnapColor(PIX* pixd, PIX* pixs, l_uint32 srcval, l_uint32 dstval, l_int32 diff);
		PIX* pixSnapColorCmap(PIX* pixd, PIX* pixs, l_uint32 srcval, l_uint32 dstval, l_int32 diff);
		PIX* pixLinearMapToTargetColor(PIX* pixd, PIX* pixs, l_uint32 srcval, l_uint32 dstval);

		PIX* pixShiftByComponent(PIX* pixd, PIX* pixs, l_uint32 srcval, l_uint32 dstval);

		PIX* pixMapWithInvariantHue(PIX* pixd, PIX* pixs, l_uint32 srcval, l_float32 fract);

		l_ok pixcmapIsValid(const PIXCMAP* cmap, PIX* pix, l_int32* pvalid);

		PIX* pixColorMorph(PIX* pixs, l_int32 type, l_int32 hsize, l_int32 vsize);
		PIX* pixOctreeColorQuant(PIX* pixs, l_int32 colors, l_int32 ditherflag);
		PIX* pixOctreeColorQuantGeneral(PIX* pixs, l_int32 colors, l_int32 ditherflag, l_float32 validthresh, l_float32 colorthresh);

		PIX* pixOctreeQuantByPopulation(PIX* pixs, l_int32 level, l_int32 ditherflag);
		PIX* pixOctreeQuantNumColors(PIX* pixs, l_int32 maxcolors, l_int32 subsample);
		PIX* pixOctcubeQuantMixedWithGray(PIX* pixs, l_int32 depth, l_int32 graylevels, l_int32 delta);
		PIX* pixFixedOctcubeQuant256(PIX* pixs, l_int32 ditherflag);
		PIX* pixFewColorsOctcubeQuant1(PIX* pixs, l_int32 level);
		PIX* pixFewColorsOctcubeQuant2(PIX* pixs, l_int32 level, NUMA* na, l_int32 ncolors, l_int32* pnerrors);
		PIX* pixFewColorsOctcubeQuantMixed(PIX* pixs, l_int32 level, l_int32 darkthresh, l_int32 lightthresh, l_int32 diffthresh, l_float32 minfract, l_int32 maxspan);
		PIX* pixFixedOctcubeQuantGenRGB(PIX* pixs, l_int32 level);
		PIX* pixQuantFromCmap(PIX* pixs, PIXCMAP* cmap, l_int32 mindepth, l_int32 level, l_int32 metric);
		PIX* pixOctcubeQuantFromCmap(PIX* pixs, PIXCMAP* cmap, l_int32 mindepth, l_int32 level, l_int32 metric);
		NUMA* pixOctcubeHistogram(PIX* pixs, l_int32 level, l_int32* pncolors);

		l_ok pixRemoveUnusedColors(PIX* pixs);
		l_ok pixNumberOccupiedOctcubes(PIX* pix, l_int32 level, l_int32 mincount, l_float32 minfract, l_int32* pncolors);
		PIX* pixMedianCutQuant(PIX* pixs, l_int32 ditherflag);
		PIX* pixMedianCutQuantGeneral(PIX* pixs, l_int32 ditherflag, l_int32 outdepth, l_int32 maxcolors, l_int32 sigbits, l_int32 maxsub, l_int32 checkbw);
		PIX* pixMedianCutQuantMixed(PIX* pixs, l_int32 ncolor, l_int32 ngray, l_int32 darkthresh, l_int32 lightthresh, l_int32 diffthresh);
		PIX* pixFewColorsMedianCutQuantMixed(PIX* pixs, l_int32 ncolor, l_int32 ngray, l_int32 maxncolors, l_int32 darkthresh, l_int32 lightthresh, l_int32 diffthresh);
		l_int32* pixMedianCutHisto(PIX* pixs, l_int32 sigbits, l_int32 subsample);
		PIX* pixColorSegment(PIX* pixs, l_int32 maxdist, l_int32 maxcolors, l_int32 selsize, l_int32 finalcolors, l_int32 debugflag);
		PIX* pixColorSegmentCluster(PIX* pixs, l_int32 maxdist, l_int32 maxcolors, l_int32 debugflag);
		l_ok pixAssignToNearestColor(PIX* pixd, PIX* pixs, PIX* pixm, l_int32 level, l_int32* countarray);
		l_ok pixColorSegmentClean(PIX* pixs, l_int32 selsize, l_int32* countarray);
		l_ok pixColorSegmentRemoveColors(PIX* pixd, PIX* pixs, l_int32 finalcolors);
		PIX* pixConvertRGBToHSV(PIX* pixd, PIX* pixs);
		PIX* pixConvertHSVToRGB(PIX* pixd, PIX* pixs);

		PIX* pixConvertRGBToHue(PIX* pixs);
		PIX* pixConvertRGBToSaturation(PIX* pixs);
		PIX* pixConvertRGBToValue(PIX* pixs);

		PIX* pixMakeRangeMaskHS(PIX* pixs, l_int32 huecenter, l_int32 huehw, l_int32 satcenter, l_int32 sathw, l_int32 regionflag);
		PIX* pixMakeRangeMaskHV(PIX* pixs, l_int32 huecenter, l_int32 huehw, l_int32 valcenter, l_int32 valhw, l_int32 regionflag);
		PIX* pixMakeRangeMaskSV(PIX* pixs, l_int32 satcenter, l_int32 sathw, l_int32 valcenter, l_int32 valhw, l_int32 regionflag);
		PIX* pixMakeHistoHS(PIX* pixs, l_int32 factor, NUMA** pnahue, NUMA** pnasat);
		PIX* pixMakeHistoHV(PIX* pixs, l_int32 factor, NUMA** pnahue, NUMA** pnaval);
		PIX* pixMakeHistoSV(PIX* pixs, l_int32 factor, NUMA** pnasat, NUMA** pnaval);
		l_ok pixFindHistoPeaksHSV(PIX* pixs, l_int32 type, l_int32 width, l_int32 height, l_int32 npeaks, l_float32 erasefactor, PTA** ppta, NUMA** pnatot, PIXA** ppixa);
		PIX* displayHSVColorRange(l_int32 hval, l_int32 sval, l_int32 vval, l_int32 huehw, l_int32 sathw, l_int32 nsamp, l_int32 factor);
		PIX* pixConvertRGBToYUV(PIX* pixd, PIX* pixs);
		PIX* pixConvertYUVToRGB(PIX* pixd, PIX* pixs);

		FPIXA* pixConvertRGBToXYZ(PIX* pixs);
		PIX* fpixaConvertXYZToRGB(FPIXA* fpixa);

		FPIXA* fpixaConvertXYZToLAB(FPIXA* fpixas);
		FPIXA* fpixaConvertLABToXYZ(FPIXA* fpixas);

		FPIXA* pixConvertRGBToLAB(PIX* pixs);
		PIX* fpixaConvertLABToRGB(FPIXA* fpixa);

		PIX* pixMakeGamutRGB(l_int32 scale);
		l_ok pixEqual(PIX* pix1, PIX* pix2, l_int32* psame);
		l_ok pixEqualWithAlpha(PIX* pix1, PIX* pix2, l_int32 use_alpha, l_int32* psame);
		l_ok pixEqualWithCmap(PIX* pix1, PIX* pix2, l_int32* psame);

		l_ok pixUsesCmapColor(PIX* pixs, l_int32* pcolor);
		l_ok pixCorrelationBinary(PIX* pix1, PIX* pix2, l_float32* pval);
		PIX* pixDisplayDiff(PIX* pix1, PIX* pix2, l_int32 showall, l_int32 mindiff, l_uint32 diffcolor);
		PIX* pixDisplayDiffBinary(PIX* pix1, PIX* pix2);
		l_ok pixCompareBinary(PIX* pix1, PIX* pix2, l_int32 comptype, l_float32* pfract, PIX** ppixdiff);
		l_ok pixCompareGrayOrRGB(PIX* pix1, PIX* pix2, l_int32 comptype, l_int32 plottype, l_int32* psame, l_float32* pdiff, l_float32* prmsdiff, PIX** ppixdiff);
		l_ok pixCompareGray(PIX* pix1, PIX* pix2, l_int32 comptype, l_int32 plottype, l_int32* psame, l_float32* pdiff, l_float32* prmsdiff, PIX** ppixdiff);
		l_ok pixCompareRGB(PIX* pix1, PIX* pix2, l_int32 comptype, l_int32 plottype, l_int32* psame, l_float32* pdiff, l_float32* prmsdiff, PIX** ppixdiff);
		l_ok pixCompareTiled(PIX* pix1, PIX* pix2, l_int32 sx, l_int32 sy, l_int32 type, PIX** ppixdiff);
		NUMA* pixCompareRankDifference(PIX* pix1, PIX* pix2, l_int32 factor);
		l_ok pixTestForSimilarity(PIX* pix1, PIX* pix2, l_int32 factor, l_int32 mindiff, l_float32 maxfract, l_float32 maxave, l_int32* psimilar, l_int32 details);
		l_ok pixGetDifferenceStats(PIX* pix1, PIX* pix2, l_int32 factor, l_int32 mindiff, l_float32* pfractdiff, l_float32* pavediff, l_int32 details);
		NUMA* pixGetDifferenceHistogram(PIX* pix1, PIX* pix2, l_int32 factor);
		l_ok pixGetPerceptualDiff(PIX* pixs1, PIX* pixs2, l_int32 sampling, l_int32 dilation, l_int32 mindiff, l_float32* pfract, PIX** ppixdiff1, PIX** ppixdiff2);
		l_ok pixGetPSNR(PIX* pix1, PIX* pix2, l_int32 factor, l_float32* ppsnr);
		l_ok pixaComparePhotoRegionsByHisto(PIXA* pixa, l_float32 minratio, l_float32 textthresh, l_int32 factor, l_int32 n, l_float32 simthresh, NUMA** pnai, l_float32** pscores, PIX** ppixd, l_int32 debug);
		l_ok pixComparePhotoRegionsByHisto(PIX* pix1, PIX* pix2, BOX* box1, BOX* box2, l_float32 minratio, l_int32 factor, l_int32 n, l_float32* pscore, l_int32 debugflag);
		l_ok pixGenPhotoHistos(PIX* pixs, BOX* box, l_int32 factor, l_float32 thresh, l_int32 n, NUMAA** pnaa, l_int32* pw, l_int32* ph, l_int32 debugindex);
		PIX* pixPadToCenterCentroid(PIX* pixs, l_int32 factor);
		l_ok pixCentroid8(PIX* pixs, l_int32 factor, l_float32* pcx, l_float32* pcy);
		l_ok pixDecideIfPhotoImage(PIX* pix, l_int32 factor, l_float32 thresh, l_int32 n, NUMAA** pnaa, PIXA* pixadebug);
		l_ok compareTilesByHisto(NUMAA* naa1, NUMAA* naa2, l_float32 minratio, l_int32 w1, l_int32 h1, l_int32 w2, l_int32 h2, l_float32* pscore, PIXA* pixadebug);
		l_ok pixCompareGrayByHisto(PIX* pix1, PIX* pix2, BOX* box1, BOX* box2, l_float32 minratio, l_int32 maxgray, l_int32 factor, l_int32 n, l_float32* pscore, l_int32 debugflag);
		l_ok pixCropAlignedToCentroid(PIX* pix1, PIX* pix2, l_int32 factor, BOX** pbox1, BOX** pbox2);

		l_ok pixCompareWithTranslation(PIX* pix1, PIX* pix2, l_int32 thresh, l_int32* pdelx, l_int32* pdely, l_float32* pscore, l_int32 debugflag);
		l_ok pixBestCorrelation(PIX* pix1, PIX* pix2, l_int32 area1, l_int32 area2, l_int32 etransx, l_int32 etransy, l_int32 maxshift, l_int32* tab8, l_int32* pdelx, l_int32* pdely, l_float32* pscore, l_int32 debugflag);
		BOXA* pixConnComp(PIX* pixs, PIXA** ppixa, l_int32 connectivity);
		BOXA* pixConnCompPixa(PIX* pixs, PIXA** ppixa, l_int32 connectivity);
		BOXA* pixConnCompBB(PIX* pixs, l_int32 connectivity);
		l_ok pixCountConnComp(PIX* pixs, l_int32 connectivity, l_int32* pcount);
		l_int32 nextOnPixelInRaster(PIX* pixs, l_int32 xstart, l_int32 ystart, l_int32* px, l_int32* py);
		BOX* pixSeedfillBB(PIX* pixs, L_STACK* stack, l_int32 x, l_int32 y, l_int32 connectivity);
		BOX* pixSeedfill4BB(PIX* pixs, L_STACK* stack, l_int32 x, l_int32 y);
		BOX* pixSeedfill8BB(PIX* pixs, L_STACK* stack, l_int32 x, l_int32 y);
		l_ok pixSeedfill(PIX* pixs, L_STACK* stack, l_int32 x, l_int32 y, l_int32 connectivity);
		l_ok pixSeedfill4(PIX* pixs, L_STACK* stack, l_int32 x, l_int32 y);
		l_ok pixSeedfill8(PIX* pixs, L_STACK* stack, l_int32 x, l_int32 y);

		l_ok convertFilesTo1bpp(const char* dirin, const char* substr, l_int32 upscaling, l_int32 thresh, l_int32 firstpage, l_int32 npages, const char* dirout, l_int32 outformat);

		PIX* pixBlockconv(PIX* pix, l_int32 wc, l_int32 hc);
		PIX* pixBlockconvGray(PIX* pixs, PIX* pixacc, l_int32 wc, l_int32 hc);
		PIX* pixBlockconvAccum(PIX* pixs);
		PIX* pixBlockconvGrayUnnormalized(PIX* pixs, l_int32 wc, l_int32 hc);
		PIX* pixBlockconvTiled(PIX* pix, l_int32 wc, l_int32 hc, l_int32 nx, l_int32 ny);
		PIX* pixBlockconvGrayTile(PIX* pixs, PIX* pixacc, l_int32 wc, l_int32 hc);
		l_ok pixWindowedStats(PIX* pixs, l_int32 wc, l_int32 hc, l_int32 hasborder, PIX** ppixm, PIX** ppixms, FPIX** pfpixv, FPIX** pfpixrv);
		PIX* pixWindowedMean(PIX* pixs, l_int32 wc, l_int32 hc, l_int32 hasborder, l_int32 normflag);
		PIX* pixWindowedMeanSquare(PIX* pixs, l_int32 wc, l_int32 hc, l_int32 hasborder);
		l_ok pixWindowedVariance(PIX* pixm, PIX* pixms, FPIX** pfpixv, FPIX** pfpixrv);
		DPIX* pixMeanSquareAccum(PIX* pixs);
		PIX* pixBlockrank(PIX* pixs, PIX* pixacc, l_int32 wc, l_int32 hc, l_float32 rank);
		PIX* pixBlocksum(PIX* pixs, PIX* pixacc, l_int32 wc, l_int32 hc);
		PIX* pixCensusTransform(PIX* pixs, l_int32 halfsize, PIX* pixacc);
		PIX* pixConvolve(PIX* pixs, L_KERNEL* kel, l_int32 outdepth, l_int32 normflag);
		PIX* pixConvolveSep(PIX* pixs, L_KERNEL* kelx, L_KERNEL* kely, l_int32 outdepth, l_int32 normflag);
		PIX* pixConvolveRGB(PIX* pixs, L_KERNEL* kel);
		PIX* pixConvolveRGBSep(PIX* pixs, L_KERNEL* kelx, L_KERNEL* kely);
		FPIX* fpixConvolve(FPIX* fpixs, L_KERNEL* kel, l_int32 normflag);
		FPIX* fpixConvolveSep(FPIX* fpixs, L_KERNEL* kelx, L_KERNEL* kely, l_int32 normflag);
		PIX* pixConvolveWithBias(PIX* pixs, L_KERNEL* kel1, L_KERNEL* kel2, l_int32 force8, l_int32* pbias);

		PIX* pixAddGaussianNoise(PIX* pixs, l_float32 stdev);

		l_ok pixCorrelationScore(PIX* pix1, PIX* pix2, l_int32 area1, l_int32 area2, l_float32 delx, l_float32 dely, l_int32 maxdiffw, l_int32 maxdiffh, l_int32* tab, l_float32* pscore);
		l_int32 pixCorrelationScoreThresholded(PIX* pix1, PIX* pix2, l_int32 area1, l_int32 area2, l_float32 delx, l_float32 dely, l_int32 maxdiffw, l_int32 maxdiffh, l_int32* tab, l_int32* downcount, l_float32 score_threshold);
		l_ok pixCorrelationScoreSimple(PIX* pix1, PIX* pix2, l_int32 area1, l_int32 area2, l_float32 delx, l_float32 dely, l_int32 maxdiffw, l_int32 maxdiffh, l_int32* tab, l_float32* pscore);
		l_ok pixCorrelationScoreShifted(PIX* pix1, PIX* pix2, l_int32 area1, l_int32 area2, l_int32 delx, l_int32 dely, l_int32* tab, l_float32* pscore);
		L_DEWARP* dewarpCreate(PIX* pixs, l_int32 pageno);

		L_DEWARPA* dewarpaCreateFromPixacomp(PIXAC* pixac, l_int32 useboth, l_int32 sampling, l_int32 minlines, l_int32 maxdist);

		PTAA* dewarpGetTextlineCenters(PIX* pixs, l_int32 debugflag);
		PTAA* dewarpRemoveShortLines(PIX* pixs, PTAA* ptaas, l_float32 fract, l_int32 debugflag);
		l_ok dewarpFindHorizSlopeDisparity(L_DEWARP* dew, PIX* pixb, l_float32 fractthresh, l_int32 parity);

		l_ok dewarpaApplyDisparity(L_DEWARPA* dewa, l_int32 pageno, PIX* pixs, l_int32 grayin, l_int32 x, l_int32 y, PIX** ppixd, const char* debugfile);
		l_ok dewarpaApplyDisparityBoxa(L_DEWARPA* dewa, l_int32 pageno, PIX* pixs, BOXA* boxas, l_int32 mapdir, l_int32 x, l_int32 y, BOXA** pboxad, const char* debugfile);

		l_ok dewarpPopulateFullRes(L_DEWARP* dew, PIX* pix, l_int32 x, l_int32 y);
		l_ok dewarpSinglePage(PIX* pixs, l_int32 thresh, l_int32 adaptive, l_int32 useboth, l_int32 check_columns, PIX** ppixd, L_DEWARPA** pdewa, l_int32 debug);
		l_ok dewarpSinglePageInit(PIX* pixs, l_int32 thresh, l_int32 adaptive, l_int32 useboth, l_int32 check_columns, PIX** ppixb, L_DEWARPA** pdewa);
		l_ok dewarpSinglePageRun(PIX* pixs, PIX* pixb, L_DEWARPA* dewa, PIX** ppixd, l_int32 debug);

		L_DNA* pixConvertDataToDna(PIX* pix);

		PIX* pixMorphDwa_2(PIX* pixd, PIX* pixs, l_int32 operation, const char* selname);
		PIX* pixFMorphopGen_2(PIX* pixd, PIX* pixs, l_int32 operation, const char* selname);

		PIX* pixSobelEdgeFilter(PIX* pixs, l_int32 orientflag);
		PIX* pixTwoSidedEdgeFilter(PIX* pixs, l_int32 orientflag);
		l_ok pixMeasureEdgeSmoothness(PIX* pixs, l_int32 side, l_int32 minjump, l_int32 minreversal, l_float32* pjpl, l_float32* pjspl, l_float32* prpl, const char* debugfile);
		NUMA* pixGetEdgeProfile(PIX* pixs, l_int32 side, const char* debugfile);
		l_ok pixGetLastOffPixelInRun(PIX* pixs, l_int32 x, l_int32 y, l_int32 direction, l_int32* ploc);
		l_int32 pixGetLastOnPixelInRun(PIX* pixs, l_int32 x, l_int32 y, l_int32 direction, l_int32* ploc);

		PIX* pixGammaTRC(PIX* pixd, PIX* pixs, l_float32 gamma, l_int32 minval, l_int32 maxval);
		PIX* pixGammaTRCMasked(PIX* pixd, PIX* pixs, PIX* pixm, l_float32 gamma, l_int32 minval, l_int32 maxval);
		PIX* pixGammaTRCWithAlpha(PIX* pixd, PIX* pixs, l_float32 gamma, l_int32 minval, l_int32 maxval);

		PIX* pixContrastTRC(PIX* pixd, PIX* pixs, l_float32 factor);
		PIX* pixContrastTRCMasked(PIX* pixd, PIX* pixs, PIX* pixm, l_float32 factor);

		PIX* pixEqualizeTRC(PIX* pixd, PIX* pixs, l_float32 fract, l_int32 factor);
		NUMA* numaEqualizeTRC(PIX* pix, l_float32 fract, l_int32 factor);
		l_int32 pixTRCMap(PIX* pixs, PIX* pixm, NUMA* na);
		l_int32 pixTRCMapGeneral(PIX* pixs, PIX* pixm, NUMA* nar, NUMA* nag, NUMA* nab);
		PIX* pixUnsharpMasking(PIX* pixs, l_int32 halfwidth, l_float32 fract);
		PIX* pixUnsharpMaskingGray(PIX* pixs, l_int32 halfwidth, l_float32 fract);
		PIX* pixUnsharpMaskingFast(PIX* pixs, l_int32 halfwidth, l_float32 fract, l_int32 direction);
		PIX* pixUnsharpMaskingGrayFast(PIX* pixs, l_int32 halfwidth, l_float32 fract, l_int32 direction);
		PIX* pixUnsharpMaskingGray1D(PIX* pixs, l_int32 halfwidth, l_float32 fract, l_int32 direction);
		PIX* pixUnsharpMaskingGray2D(PIX* pixs, l_int32 halfwidth, l_float32 fract);
		PIX* pixModifyHue(PIX* pixd, PIX* pixs, l_float32 fract);
		PIX* pixModifySaturation(PIX* pixd, PIX* pixs, l_float32 fract);
		l_int32 pixMeasureSaturation(PIX* pixs, l_int32 factor, l_float32* psat);
		PIX* pixModifyBrightness(PIX* pixd, PIX* pixs, l_float32 fract);
		PIX* pixMosaicColorShiftRGB(PIX* pixs, l_float32 roff, l_float32 goff, l_float32 boff, l_float32 delta, l_int32 nincr);
		PIX* pixColorShiftRGB(PIX* pixs, l_float32 rfract, l_float32 gfract, l_float32 bfract);
		PIX* pixDarkenGray(PIX* pixd, PIX* pixs, l_int32 thresh, l_int32 satlimit);
		PIX* pixMultConstantColor(PIX* pixs, l_float32 rfact, l_float32 gfact, l_float32 bfact);
		PIX* pixMultMatrixColor(PIX* pixs, L_KERNEL* kel);
		PIX* pixHalfEdgeByBandpass(PIX* pixs, l_int32 sm1h, l_int32 sm1v, l_int32 sm2h, l_int32 sm2v);

		PIX* pixHMTDwa_1(PIX* pixd, PIX* pixs, const char* selname);
		PIX* pixFHMTGen_1(PIX* pixd, PIX* pixs, const char* selname);

		l_ok pixItalicWords(PIX* pixs, BOXA* boxaw, PIX* pixw, BOXA** pboxa, l_int32 debugflag);
		PIX* pixOrientCorrect(PIX* pixs, l_float32 minupconf, l_float32 minratio, l_float32* pupconf, l_float32* pleftconf, l_int32* protation, l_int32 debug);
		l_ok pixOrientDetect(PIX* pixs, l_float32* pupconf, l_float32* pleftconf, l_int32 mincount, l_int32 debug);

		l_ok pixUpDownDetect(PIX* pixs, l_float32* pconf, l_int32 mincount, l_int32 npixels, l_int32 debug);
		l_ok pixMirrorDetect(PIX* pixs, l_float32* pconf, l_int32 mincount, l_int32 debug);

		PIX* pixMorphDwa_1(PIX* pixd, PIX* pixs, l_int32 operation, const char* selname);
		PIX* pixFMorphopGen_1(PIX* pixd, PIX* pixs, l_int32 operation, const char* selname);

		FPIX* fpixCreate(l_int32 width, l_int32 height);
		FPIX* fpixCreateTemplate(FPIX* fpixs);
		FPIX* fpixClone(FPIX* fpix);
		FPIX* fpixCopy(FPIX* fpixs);
		void fpixDestroy(FPIX** pfpix);
		l_ok fpixGetDimensions(FPIX* fpix, l_int32* pw, l_int32* ph);
		l_ok fpixSetDimensions(FPIX* fpix, l_int32 w, l_int32 h);
		l_int32 fpixGetWpl(FPIX* fpix);
		l_ok fpixSetWpl(FPIX* fpix, l_int32 wpl);
		l_ok fpixGetResolution(FPIX* fpix, l_int32* pxres, l_int32* pyres);
		l_ok fpixSetResolution(FPIX* fpix, l_int32 xres, l_int32 yres);
		l_ok fpixCopyResolution(FPIX* fpixd, FPIX* fpixs);
		l_float32* fpixGetData(FPIX* fpix);
		l_ok fpixSetData(FPIX* fpix, l_float32* data);
		l_ok fpixGetPixel(FPIX* fpix, l_int32 x, l_int32 y, l_float32* pval);
		l_ok fpixSetPixel(FPIX* fpix, l_int32 x, l_int32 y, l_float32 val);
		FPIXA* fpixaCreate(l_int32 n);
		FPIXA* fpixaCopy(FPIXA* fpixa, l_int32 copyflag);
		void fpixaDestroy(FPIXA** pfpixa);
		l_ok fpixaAddFPix(FPIXA* fpixa, FPIX* fpix, l_int32 copyflag);
		l_int32 fpixaGetCount(FPIXA* fpixa);
		FPIX* fpixaGetFPix(FPIXA* fpixa, l_int32 index, l_int32 accesstype);
		l_ok fpixaGetFPixDimensions(FPIXA* fpixa, l_int32 index, l_int32* pw, l_int32* ph);
		l_float32* fpixaGetData(FPIXA* fpixa, l_int32 index);
		l_ok fpixaGetPixel(FPIXA* fpixa, l_int32 index, l_int32 x, l_int32 y, l_float32* pval);
		l_ok fpixaSetPixel(FPIXA* fpixa, l_int32 index, l_int32 x, l_int32 y, l_float32 val);

		DPIX* dpixCreate(l_int32 width, l_int32 height);
		DPIX* dpixCreateTemplate(DPIX* dpixs);
		DPIX* dpixClone(DPIX* dpix);
		DPIX* dpixCopy(DPIX* dpixs);
		void dpixDestroy(DPIX** pdpix);
		l_ok dpixGetDimensions(DPIX* dpix, l_int32* pw, l_int32* ph);
		l_ok dpixSetDimensions(DPIX* dpix, l_int32 w, l_int32 h);
		l_int32 dpixGetWpl(DPIX* dpix);
		l_ok dpixSetWpl(DPIX* dpix, l_int32 wpl);
		l_ok dpixGetResolution(DPIX* dpix, l_int32* pxres, l_int32* pyres);
		l_ok dpixSetResolution(DPIX* dpix, l_int32 xres, l_int32 yres);
		l_ok dpixCopyResolution(DPIX* dpixd, DPIX* dpixs);
		l_float64* dpixGetData(DPIX* dpix);
		l_ok dpixSetData(DPIX* dpix, l_float64* data);
		l_ok dpixGetPixel(DPIX* dpix, l_int32 x, l_int32 y, l_float64* pval);
		l_ok dpixSetPixel(DPIX* dpix, l_int32 x, l_int32 y, l_float64 val);

		FPIX* fpixRead(const char* filename);
		FPIX* fpixReadStream(FILE* fp);
		FPIX* fpixReadMem(const l_uint8* data, size_t size);
		l_ok fpixWrite(const char* filename, FPIX* fpix);
		l_ok fpixWriteStream(FILE* fp, FPIX* fpix);
		l_ok fpixWriteMem(l_uint8** pdata, size_t* psize, FPIX* fpix);
		FPIX* fpixEndianByteSwap(FPIX* fpixd, FPIX* fpixs);

		DPIX* dpixRead(const char* filename);
		DPIX* dpixReadStream(FILE* fp);
		DPIX* dpixReadMem(const l_uint8* data, size_t size);
		l_ok dpixWrite(const char* filename, DPIX* dpix);
		l_ok dpixWriteStream(FILE* fp, DPIX* dpix);
		l_ok dpixWriteMem(l_uint8** pdata, size_t* psize, DPIX* dpix);
		DPIX* dpixEndianByteSwap(DPIX* dpixd, DPIX* dpixs);

		l_ok fpixPrintStream(FILE* fp, FPIX* fpix, l_int32 factor);
		FPIX* pixConvertToFPix(PIX* pixs, l_int32 ncomps);
		DPIX* pixConvertToDPix(PIX* pixs, l_int32 ncomps);
		PIX* fpixConvertToPix(FPIX* fpixs, l_int32 outdepth, l_int32 negvals, l_int32 errorflag);
		PIX* fpixDisplayMaxDynamicRange(FPIX* fpixs);
		DPIX* fpixConvertToDPix(FPIX* fpix);
		PIX* dpixConvertToPix(DPIX* dpixs, l_int32 outdepth, l_int32 negvals, l_int32 errorflag);
		FPIX* dpixConvertToFPix(DPIX* dpix);

		l_ok fpixGetMin(FPIX* fpix, l_float32* pminval, l_int32* pxminloc, l_int32* pyminloc);
		l_ok fpixGetMax(FPIX* fpix, l_float32* pmaxval, l_int32* pxmaxloc, l_int32* pymaxloc);
		l_ok dpixGetMin(DPIX* dpix, l_float64* pminval, l_int32* pxminloc, l_int32* pyminloc);
		l_ok dpixGetMax(DPIX* dpix, l_float64* pmaxval, l_int32* pxmaxloc, l_int32* pymaxloc);

		FPIX* fpixScaleByInteger(FPIX* fpixs, l_int32 factor);
		DPIX* dpixScaleByInteger(DPIX* dpixs, l_int32 factor);
		FPIX* fpixLinearCombination(FPIX* fpixd, FPIX* fpixs1, FPIX* fpixs2, l_float32 a, l_float32 b);
		l_ok fpixAddMultConstant(FPIX* fpix, l_float32 addc, l_float32 multc);
		DPIX* dpixLinearCombination(DPIX* dpixd, DPIX* dpixs1, DPIX* dpixs2, l_float32 a, l_float32 b);
		l_ok dpixAddMultConstant(DPIX* dpix, l_float64 addc, l_float64 multc);
		l_ok fpixSetAllArbitrary(FPIX* fpix, l_float32 inval);
		l_ok dpixSetAllArbitrary(DPIX* dpix, l_float64 inval);
		FPIX* fpixAddBorder(FPIX* fpixs, l_int32 left, l_int32 right, l_int32 top, l_int32 bot);
		FPIX* fpixRemoveBorder(FPIX* fpixs, l_int32 left, l_int32 right, l_int32 top, l_int32 bot);
		FPIX* fpixAddMirroredBorder(FPIX* fpixs, l_int32 left, l_int32 right, l_int32 top, l_int32 bot);
		FPIX* fpixAddContinuedBorder(FPIX* fpixs, l_int32 left, l_int32 right, l_int32 top, l_int32 bot);
		FPIX* fpixAddSlopeBorder(FPIX* fpixs, l_int32 left, l_int32 right, l_int32 top, l_int32 bot);
		l_ok fpixRasterop(FPIX* fpixd, l_int32 dx, l_int32 dy, l_int32 dw, l_int32 dh, FPIX* fpixs, l_int32 sx, l_int32 sy);
		FPIX* fpixRotateOrth(FPIX* fpixs, l_int32 quads);
		FPIX* fpixRotate180(FPIX* fpixd, FPIX* fpixs);
		FPIX* fpixRotate90(FPIX* fpixs, l_int32 direction);
		FPIX* fpixFlipLR(FPIX* fpixd, FPIX* fpixs);
		FPIX* fpixFlipTB(FPIX* fpixd, FPIX* fpixs);
		FPIX* fpixAffinePta(FPIX* fpixs, PTA* ptad, PTA* ptas, l_int32 border, l_float32 inval);
		FPIX* fpixAffine(FPIX* fpixs, l_float32* vc, l_float32 inval);
		FPIX* fpixProjectivePta(FPIX* fpixs, PTA* ptad, PTA* ptas, l_int32 border, l_float32 inval);
		FPIX* fpixProjective(FPIX* fpixs, l_float32* vc, l_float32 inval);

		PIX* fpixThresholdToPix(FPIX* fpix, l_float32 thresh);
		FPIX* pixComponentFunction(PIX* pix, l_float32 rnum, l_float32 gnum, l_float32 bnum, l_float32 rdenom, l_float32 gdenom, l_float32 bdenom);
		PIX* pixReadStreamGif(FILE* fp);
		PIX* pixReadMemGif(const l_uint8* cdata, size_t size);
		l_ok pixWriteStreamGif(FILE* fp, PIX* pix);
		l_ok pixWriteMemGif(l_uint8** pdata, size_t* psize, PIX* pix);

		l_ok pixRenderPlotFromNuma(PIX** ppix, NUMA* na, l_int32 plotloc, l_int32 linewidth, l_int32 max, l_uint32 color);
		l_ok pixRenderPlotFromNumaGen(PIX** ppix, NUMA* na, l_int32 orient, l_int32 linewidth, l_int32 refpos, l_int32 max, l_int32 drawref, l_uint32 color);
		l_ok pixRenderPta(PIX* pix, PTA* pta, l_int32 op);
		l_ok pixRenderPtaArb(PIX* pix, PTA* pta, l_uint8 rval, l_uint8 gval, l_uint8 bval);
		l_ok pixRenderPtaBlend(PIX* pix, PTA* pta, l_uint8 rval, l_uint8 gval, l_uint8 bval, l_float32 fract);
		l_ok pixRenderLine(PIX* pix, l_int32 x1, l_int32 y1, l_int32 x2, l_int32 y2, l_int32 width, l_int32 op);
		l_ok pixRenderLineArb(PIX* pix, l_int32 x1, l_int32 y1, l_int32 x2, l_int32 y2, l_int32 width, l_uint8 rval, l_uint8 gval, l_uint8 bval);
		l_ok pixRenderLineBlend(PIX* pix, l_int32 x1, l_int32 y1, l_int32 x2, l_int32 y2, l_int32 width, l_uint8 rval, l_uint8 gval, l_uint8 bval, l_float32 fract);
		l_ok pixRenderBox(PIX* pix, BOX* box, l_int32 width, l_int32 op);
		l_ok pixRenderBoxArb(PIX* pix, BOX* box, l_int32 width, l_uint8 rval, l_uint8 gval, l_uint8 bval);
		l_ok pixRenderBoxBlend(PIX* pix, BOX* box, l_int32 width, l_uint8 rval, l_uint8 gval, l_uint8 bval, l_float32 fract);
		l_ok pixRenderBoxa(PIX* pix, BOXA* boxa, l_int32 width, l_int32 op);
		l_ok pixRenderBoxaArb(PIX* pix, BOXA* boxa, l_int32 width, l_uint8 rval, l_uint8 gval, l_uint8 bval);
		l_ok pixRenderBoxaBlend(PIX* pix, BOXA* boxa, l_int32 width, l_uint8 rval, l_uint8 gval, l_uint8 bval, l_float32 fract, l_int32 removedups);
		l_ok pixRenderHashBox(PIX* pix, BOX* box, l_int32 spacing, l_int32 width, l_int32 orient, l_int32 outline, l_int32 op);
		l_ok pixRenderHashBoxArb(PIX* pix, BOX* box, l_int32 spacing, l_int32 width, l_int32 orient, l_int32 outline, l_int32 rval, l_int32 gval, l_int32 bval);
		l_ok pixRenderHashBoxBlend(PIX* pix, BOX* box, l_int32 spacing, l_int32 width, l_int32 orient, l_int32 outline, l_int32 rval, l_int32 gval, l_int32 bval, l_float32 fract);
		l_ok pixRenderHashMaskArb(PIX* pix, PIX* pixm, l_int32 x, l_int32 y, l_int32 spacing, l_int32 width, l_int32 orient, l_int32 outline, l_int32 rval, l_int32 gval, l_int32 bval);
		l_ok pixRenderHashBoxa(PIX* pix, BOXA* boxa, l_int32 spacing, l_int32 width, l_int32 orient, l_int32 outline, l_int32 op);
		l_ok pixRenderHashBoxaArb(PIX* pix, BOXA* boxa, l_int32 spacing, l_int32 width, l_int32 orient, l_int32 outline, l_int32 rval, l_int32 gval, l_int32 bval);
		l_ok pixRenderHashBoxaBlend(PIX* pix, BOXA* boxa, l_int32 spacing, l_int32 width, l_int32 orient, l_int32 outline, l_int32 rval, l_int32 gval, l_int32 bval, l_float32 fract);
		l_ok pixRenderPolyline(PIX* pix, PTA* ptas, l_int32 width, l_int32 op, l_int32 closeflag);
		l_ok pixRenderPolylineArb(PIX* pix, PTA* ptas, l_int32 width, l_uint8 rval, l_uint8 gval, l_uint8 bval, l_int32 closeflag);
		l_ok pixRenderPolylineBlend(PIX* pix, PTA* ptas, l_int32 width, l_uint8 rval, l_uint8 gval, l_uint8 bval, l_float32 fract, l_int32 closeflag, l_int32 removedups);
		l_ok pixRenderGridArb(PIX* pix, l_int32 nx, l_int32 ny, l_int32 width, l_uint8 rval, l_uint8 gval, l_uint8 bval);
		PIX* pixRenderRandomCmapPtaa(PIX* pix, PTAA* ptaa, l_int32 polyflag, l_int32 width, l_int32 closeflag);
		PIX* pixRenderPolygon(PTA* ptas, l_int32 width, l_int32* pxmin, l_int32* pymin);
		PIX* pixFillPolygon(PIX* pixs, PTA* pta, l_int32 xmin, l_int32 ymin);
		PIX* pixRenderContours(PIX* pixs, l_int32 startval, l_int32 incr, l_int32 outdepth);
		PIX* fpixAutoRenderContours(FPIX* fpix, l_int32 ncontours);
		PIX* fpixRenderContours(FPIX* fpixs, l_float32 incr, l_float32 proxim);
		PTA* pixGeneratePtaBoundary(PIX* pixs, l_int32 width);
		PIX* pixErodeGray(PIX* pixs, l_int32 hsize, l_int32 vsize);
		PIX* pixDilateGray(PIX* pixs, l_int32 hsize, l_int32 vsize);
		PIX* pixOpenGray(PIX* pixs, l_int32 hsize, l_int32 vsize);
		PIX* pixCloseGray(PIX* pixs, l_int32 hsize, l_int32 vsize);
		PIX* pixErodeGray3(PIX* pixs, l_int32 hsize, l_int32 vsize);
		PIX* pixDilateGray3(PIX* pixs, l_int32 hsize, l_int32 vsize);
		PIX* pixOpenGray3(PIX* pixs, l_int32 hsize, l_int32 vsize);
		PIX* pixCloseGray3(PIX* pixs, l_int32 hsize, l_int32 vsize);
		PIX* pixDitherToBinary(PIX* pixs);
		PIX* pixDitherToBinarySpec(PIX* pixs, l_int32 lowerclip, l_int32 upperclip);

		PIX* pixThresholdToBinary(PIX* pixs, l_int32 thresh);

		PIX* pixVarThresholdToBinary(PIX* pixs, PIX* pixg);
		PIX* pixAdaptThresholdToBinary(PIX* pixs, PIX* pixm, l_float32 gamma);
		PIX* pixAdaptThresholdToBinaryGen(PIX* pixs, PIX* pixm, l_float32 gamma, l_int32 blackval, l_int32 whiteval, l_int32 thresh);
		PIX* pixGenerateMaskByValue(PIX* pixs, l_int32 val, l_int32 usecmap);
		PIX* pixGenerateMaskByBand(PIX* pixs, l_int32 lower, l_int32 upper, l_int32 inband, l_int32 usecmap);
		PIX* pixDitherTo2bpp(PIX* pixs, l_int32 cmapflag);
		PIX* pixDitherTo2bppSpec(PIX* pixs, l_int32 lowerclip, l_int32 upperclip, l_int32 cmapflag);
		PIX* pixThresholdTo2bpp(PIX* pixs, l_int32 nlevels, l_int32 cmapflag);
		PIX* pixThresholdTo4bpp(PIX* pixs, l_int32 nlevels, l_int32 cmapflag);
		PIX* pixThresholdOn8bpp(PIX* pixs, l_int32 nlevels, l_int32 cmapflag);
		PIX* pixThresholdGrayArb(PIX* pixs, const char* edgevals, l_int32 outdepth, l_int32 use_average, l_int32 setblack, l_int32 setwhite);

		PIX* pixGenerateMaskByBand32(PIX* pixs, l_uint32 refval, l_int32 delm, l_int32 delp, l_float32 fractm, l_float32 fractp);
		PIX* pixGenerateMaskByDiscr32(PIX* pixs, l_uint32 refval1, l_uint32 refval2, l_int32 distflag);
		PIX* pixGrayQuantFromHisto(PIX* pixd, PIX* pixs, PIX* pixm, l_float32 minfract, l_int32 maxsize);
		PIX* pixGrayQuantFromCmap(PIX* pixs, PIXCMAP* cmap, l_int32 mindepth);

		l_ok jbAddPage(JBCLASSER* classer, PIX* pixs);
		l_ok jbAddPageComponents(JBCLASSER* classer, PIX* pixs, BOXA* boxas, PIXA* pixas);
		l_ok jbClassifyRankHaus(JBCLASSER* classer, BOXA* boxa, PIXA* pixas);
		l_int32 pixHaustest(PIX* pix1, PIX* pix2, PIX* pix3, PIX* pix4, l_float32 delx, l_float32 dely, l_int32 maxdiffw, l_int32 maxdiffh);
		l_int32 pixRankHaustest(PIX* pix1, PIX* pix2, PIX* pix3, PIX* pix4, l_float32 delx, l_float32 dely, l_int32 maxdiffw, l_int32 maxdiffh, l_int32 area1, l_int32 area3, l_float32 rank, l_int32* tab8);
		l_ok jbClassifyCorrelation(JBCLASSER* classer, BOXA* boxa, PIXA* pixas);
		l_ok jbGetComponents(PIX* pixs, l_int32 components, l_int32 maxwidth, l_int32 maxheight, BOXA** pboxad, PIXA** ppixad);
		l_ok pixWordMaskByDilation(PIX* pixs, PIX** ppixm, l_int32* psize, PIXA* pixadb);
		l_ok pixWordBoxesByDilation(PIX* pixs, l_int32 minwidth, l_int32 minheight, l_int32 maxwidth, l_int32 maxheight, BOXA** pboxa, l_int32* psize, PIXA* pixadb);
		PIXA* jbAccumulateComposites(PIXAA* pixaa, NUMA** pna, PTA** pptat);
		PIXA* jbTemplatesFromComposites(PIXA* pixac, NUMA* na);

		PIXA* jbDataRender(JBDATA* data, l_int32 debugflag);
		l_ok jbGetULCorners(JBCLASSER* classer, PIX* pixs, BOXA* boxa);

		PIX* pixReadJp2k(const char* filename, l_uint32 reduction, BOX* box, l_int32 hint, l_int32 debug);
		PIX* pixReadStreamJp2k(FILE* fp, l_uint32 reduction, BOX* box, l_int32 hint, l_int32 debug);
		l_ok pixWriteJp2k(const char* filename, PIX* pix, l_int32 quality, l_int32 nlevels, l_int32 hint, l_int32 debug);
		l_ok pixWriteStreamJp2k(FILE* fp, PIX* pix, l_int32 quality, l_int32 nlevels, l_int32 codec, l_int32 hint, l_int32 debug);
		PIX* pixReadMemJp2k(const l_uint8* data, size_t size, l_uint32 reduction, BOX* box, l_int32 hint, l_int32 debug);
		l_ok pixWriteMemJp2k(l_uint8** pdata, size_t* psize, PIX* pix, l_int32 quality, l_int32 nlevels, l_int32 hint, l_int32 debug);
		PIX* pixReadJpeg(const char* filename, l_int32 cmflag, l_int32 reduction, l_int32* pnwarn, l_int32 hint);
		PIX* pixReadStreamJpeg(FILE* fp, l_int32 cmflag, l_int32 reduction, l_int32* pnwarn, l_int32 hint);

		l_ok pixWriteJpeg(const char* filename, PIX* pix, l_int32 quality, l_int32 progressive);
		l_ok pixWriteStreamJpeg(FILE* fp, PIX* pix, l_int32 quality, l_int32 progressive);
		PIX* pixReadMemJpeg(const l_uint8* cdata, size_t size, l_int32 cmflag, l_int32 reduction, l_int32* pnwarn, l_int32 hint);

		l_ok pixWriteMemJpeg(l_uint8** pdata, size_t* psize, PIX* pix, l_int32 quality, l_int32 progressive);
		l_ok pixSetChromaSampling(PIX* pix, l_int32 sampling);

		L_KERNEL* kernelCreateFromPix(PIX* pix, l_int32 cy, l_int32 cx);
		PIX* kernelDisplayInPix(L_KERNEL* kel, l_int32 size, l_int32 gthick);

		PIX* generateBinaryMaze(l_int32 w, l_int32 h, l_int32 xi, l_int32 yi, l_float32 wallps, l_float32 ranis);
		PTA* pixSearchBinaryMaze(PIX* pixs, l_int32 xi, l_int32 yi, l_int32 xf, l_int32 yf, PIX** ppixd);
		PTA* pixSearchGrayMaze(PIX* pixs, l_int32 xi, l_int32 yi, l_int32 xf, l_int32 yf, PIX** ppixd);
		PIX* pixDilate(PIX* pixd, PIX* pixs, SEL* sel);
		PIX* pixErode(PIX* pixd, PIX* pixs, SEL* sel);
		PIX* pixHMT(PIX* pixd, PIX* pixs, SEL* sel);
		PIX* pixOpen(PIX* pixd, PIX* pixs, SEL* sel);
		PIX* pixClose(PIX* pixd, PIX* pixs, SEL* sel);
		PIX* pixCloseSafe(PIX* pixd, PIX* pixs, SEL* sel);
		PIX* pixOpenGeneralized(PIX* pixd, PIX* pixs, SEL* sel);
		PIX* pixCloseGeneralized(PIX* pixd, PIX* pixs, SEL* sel);
		PIX* pixDilateBrick(PIX* pixd, PIX* pixs, l_int32 hsize, l_int32 vsize);
		PIX* pixErodeBrick(PIX* pixd, PIX* pixs, l_int32 hsize, l_int32 vsize);
		PIX* pixOpenBrick(PIX* pixd, PIX* pixs, l_int32 hsize, l_int32 vsize);
		PIX* pixCloseBrick(PIX* pixd, PIX* pixs, l_int32 hsize, l_int32 vsize);
		PIX* pixCloseSafeBrick(PIX* pixd, PIX* pixs, l_int32 hsize, l_int32 vsize);

		PIX* pixDilateCompBrick(PIX* pixd, PIX* pixs, l_int32 hsize, l_int32 vsize);
		PIX* pixErodeCompBrick(PIX* pixd, PIX* pixs, l_int32 hsize, l_int32 vsize);
		PIX* pixOpenCompBrick(PIX* pixd, PIX* pixs, l_int32 hsize, l_int32 vsize);
		PIX* pixCloseCompBrick(PIX* pixd, PIX* pixs, l_int32 hsize, l_int32 vsize);
		PIX* pixCloseSafeCompBrick(PIX* pixd, PIX* pixs, l_int32 hsize, l_int32 vsize);

		PIX* pixExtractBoundary(PIX* pixs, l_int32 type);
		PIX* pixMorphSequenceMasked(PIX* pixs, PIX* pixm, const char* sequence, l_int32 dispsep);
		PIX* pixMorphSequenceByComponent(PIX* pixs, const char* sequence, l_int32 connectivity, l_int32 minw, l_int32 minh, BOXA** pboxa);
		PIXA* pixaMorphSequenceByComponent(PIXA* pixas, const char* sequence, l_int32 minw, l_int32 minh);
		PIX* pixMorphSequenceByRegion(PIX* pixs, PIX* pixm, const char* sequence, l_int32 connectivity, l_int32 minw, l_int32 minh, BOXA** pboxa);
		PIXA* pixaMorphSequenceByRegion(PIX* pixs, PIXA* pixam, const char* sequence, l_int32 minw, l_int32 minh);
		PIX* pixUnionOfMorphOps(PIX* pixs, SELA* sela, l_int32 type);
		PIX* pixIntersectionOfMorphOps(PIX* pixs, SELA* sela, l_int32 type);
		PIX* pixSelectiveConnCompFill(PIX* pixs, l_int32 connectivity, l_int32 minw, l_int32 minh);
		l_ok pixRemoveMatchedPattern(PIX* pixs, PIX* pixp, PIX* pixe, l_int32 x0, l_int32 y0, l_int32 dsize);
		PIX* pixDisplayMatchedPattern(PIX* pixs, PIX* pixp, PIX* pixe, l_int32 x0, l_int32 y0, l_uint32 color, l_float32 scale, l_int32 nlevels);
		PIXA* pixaExtendByMorph(PIXA* pixas, l_int32 type, l_int32 niters, SEL* sel, l_int32 include);
		PIXA* pixaExtendByScaling(PIXA* pixas, NUMA* nasc, l_int32 type, l_int32 include);
		PIX* pixSeedfillMorph(PIX* pixs, PIX* pixm, l_int32 maxiters, l_int32 connectivity);
		NUMA* pixRunHistogramMorph(PIX* pixs, l_int32 runtype, l_int32 direction, l_int32 maxsize);
		PIX* pixTophat(PIX* pixs, l_int32 hsize, l_int32 vsize, l_int32 type);
		PIX* pixHDome(PIX* pixs, l_int32 height, l_int32 connectivity);
		PIX* pixFastTophat(PIX* pixs, l_int32 xsize, l_int32 ysize, l_int32 type);
		PIX* pixMorphGradient(PIX* pixs, l_int32 hsize, l_int32 vsize, l_int32 smoothing);
		PTA* pixaCentroids(PIXA* pixa);
		l_ok pixCentroid(PIX* pix, l_int32* centtab, l_int32* sumtab, l_float32* pxave, l_float32* pyave);
		PIX* pixDilateBrickDwa(PIX* pixd, PIX* pixs, l_int32 hsize, l_int32 vsize);
		PIX* pixErodeBrickDwa(PIX* pixd, PIX* pixs, l_int32 hsize, l_int32 vsize);
		PIX* pixOpenBrickDwa(PIX* pixd, PIX* pixs, l_int32 hsize, l_int32 vsize);
		PIX* pixCloseBrickDwa(PIX* pixd, PIX* pixs, l_int32 hsize, l_int32 vsize);
		PIX* pixDilateCompBrickDwa(PIX* pixd, PIX* pixs, l_int32 hsize, l_int32 vsize);
		PIX* pixErodeCompBrickDwa(PIX* pixd, PIX* pixs, l_int32 hsize, l_int32 vsize);
		PIX* pixOpenCompBrickDwa(PIX* pixd, PIX* pixs, l_int32 hsize, l_int32 vsize);
		PIX* pixCloseCompBrickDwa(PIX* pixd, PIX* pixs, l_int32 hsize, l_int32 vsize);
		PIX* pixDilateCompBrickExtendDwa(PIX* pixd, PIX* pixs, l_int32 hsize, l_int32 vsize);
		PIX* pixErodeCompBrickExtendDwa(PIX* pixd, PIX* pixs, l_int32 hsize, l_int32 vsize);
		PIX* pixOpenCompBrickExtendDwa(PIX* pixd, PIX* pixs, l_int32 hsize, l_int32 vsize);
		PIX* pixCloseCompBrickExtendDwa(PIX* pixd, PIX* pixs, l_int32 hsize, l_int32 vsize);

		PIX* pixMorphSequence(PIX* pixs, const char* sequence, l_int32 dispsep);
		PIX* pixMorphCompSequence(PIX* pixs, const char* sequence, l_int32 dispsep);
		PIX* pixMorphSequenceDwa(PIX* pixs, const char* sequence, l_int32 dispsep);
		PIX* pixMorphCompSequenceDwa(PIX* pixs, const char* sequence, l_int32 dispsep);

		PIX* pixGrayMorphSequence(PIX* pixs, const char* sequence, l_int32 dispsep, l_int32 dispy);
		PIX* pixColorMorphSequence(PIX* pixs, const char* sequence, l_int32 dispsep, l_int32 dispy);

		l_ok pixGetRegionsBinary(PIX* pixs, PIX** ppixhm, PIX** ppixtm, PIX** ppixtb, PIXA* pixadb);
		PIX* pixGenHalftoneMask(PIX* pixs, PIX** ppixtext, l_int32* phtfound, l_int32 debug);
		PIX* pixGenerateHalftoneMask(PIX* pixs, PIX** ppixtext, l_int32* phtfound, PIXA* pixadb);
		PIX* pixGenTextlineMask(PIX* pixs, PIX** ppixvws, l_int32* ptlfound, PIXA* pixadb);
		PIX* pixGenTextblockMask(PIX* pixs, PIX* pixvws, PIXA* pixadb);
		PIX* pixCropImage(PIX* pixs, l_int32 lr_clear, l_int32 tb_clear, l_int32 edgeclean, l_int32 lr_add, l_int32 tb_add, l_float32 maxwiden, const char* debugfile, BOX** pcropbox);
		PIX* pixCleanImage(PIX* pixs, l_int32 contrast, l_int32 rotation, l_int32 scale, l_int32 opensize);
		BOX* pixFindPageForeground(PIX* pixs, l_int32 threshold, l_int32 mindist, l_int32 erasedist, l_int32 showmorph, PIXAC* pixac);
		l_ok pixSplitIntoCharacters(PIX* pixs, l_int32 minw, l_int32 minh, BOXA** pboxa, PIXA** ppixa, PIX** ppixdebug);
		BOXA* pixSplitComponentWithProfile(PIX* pixs, l_int32 delta, l_int32 mindel, PIX** ppixdebug);
		PIXA* pixExtractTextlines(PIX* pixs, l_int32 maxw, l_int32 maxh, l_int32 minw, l_int32 minh, l_int32 adjw, l_int32 adjh, PIXA* pixadb);
		PIXA* pixExtractRawTextlines(PIX* pixs, l_int32 maxw, l_int32 maxh, l_int32 adjw, l_int32 adjh, PIXA* pixadb);
		l_ok pixCountTextColumns(PIX* pixs, l_float32 deltafract, l_float32 peakfract, l_float32 clipfract, l_int32* pncols, PIXA* pixadb);
		l_ok pixDecideIfText(PIX* pixs, BOX* box, l_int32* pistext, PIXA* pixadb);
		l_ok pixFindThreshFgExtent(PIX* pixs, l_int32 thresh, l_int32* ptop, l_int32* pbot);
		l_ok pixDecideIfTable(PIX* pixs, BOX* box, l_int32 orient, l_int32* pscore, PIXA* pixadb);
		PIX* pixPrepare1bpp(PIX* pixs, BOX* box, l_float32 cropfract, l_int32 outres);
		l_ok pixEstimateBackground(PIX* pixs, l_int32 darkthresh, l_float32 edgecrop, l_int32* pbg);
		l_ok pixFindLargeRectangles(PIX* pixs, l_int32 polarity, l_int32 nrect, BOXA** pboxa, PIX** ppixdb);
		l_ok pixFindLargestRectangle(PIX* pixs, l_int32 polarity, BOX** pbox, PIX** ppixdb);
		BOX* pixFindRectangleInCC(PIX* pixs, BOX* boxs, l_float32 fract, l_int32 dir, l_int32 select, l_int32 debug);
		PIX* pixAutoPhotoinvert(PIX* pixs, l_int32 thresh, PIX** ppixm, PIXA* pixadb);
		l_ok pixSetSelectCmap(PIX* pixs, BOX* box, l_int32 sindex, l_int32 rval, l_int32 gval, l_int32 bval);
		l_ok pixColorGrayRegionsCmap(PIX* pixs, BOXA* boxa, l_int32 type, l_int32 rval, l_int32 gval, l_int32 bval);
		l_ok pixColorGrayCmap(PIX* pixs, BOX* box, l_int32 type, l_int32 rval, l_int32 gval, l_int32 bval);
		l_ok pixColorGrayMaskedCmap(PIX* pixs, PIX* pixm, l_int32 type, l_int32 rval, l_int32 gval, l_int32 bval);
		l_ok addColorizedGrayToCmap(PIXCMAP* cmap, l_int32 type, l_int32 rval, l_int32 gval, l_int32 bval, NUMA** pna);
		l_ok pixSetSelectMaskedCmap(PIX* pixs, PIX* pixm, l_int32 x, l_int32 y, l_int32 sindex, l_int32 rval, l_int32 gval, l_int32 bval);
		l_ok pixSetMaskedCmap(PIX* pixs, PIX* pixm, l_int32 x, l_int32 y, l_int32 rval, l_int32 gval, l_int32 bval);

		l_ok partifyPixac(PIXAC* pixac, l_int32 nparts, const char* outroot, PIXA* pixadb);

		l_ok selectDefaultPdfEncoding(PIX* pix, l_int32* ptype);

		l_ok pixaConvertToPdf(PIXA* pixa, l_int32 res, l_float32 scalefactor, l_int32 type, l_int32 quality, const char* title, const char* fileout);
		l_ok pixaConvertToPdfData(PIXA* pixa, l_int32 res, l_float32 scalefactor, l_int32 type, l_int32 quality, const char* title, l_uint8** pdata, size_t* pnbytes);

		l_ok pixConvertToPdf(PIX* pix, l_int32 type, l_int32 quality, const char* fileout, l_int32 x, l_int32 y, l_int32 res, const char* title, L_PDF_DATA** plpd, l_int32 position);
		l_ok pixWriteStreamPdf(FILE* fp, PIX* pix, l_int32 res, const char* title);
		l_ok pixWriteMemPdf(l_uint8** pdata, size_t* pnbytes, PIX* pix, l_int32 res, const char* title);

		l_ok pixConvertToPdfSegmented(PIX* pixs, l_int32 res, l_int32 type, l_int32 thresh, BOXA* boxa, l_int32 quality, l_float32 scalefactor, const char* title, const char* fileout);

		l_ok pixConvertToPdfDataSegmented(PIX* pixs, l_int32 res, l_int32 type, l_int32 thresh, BOXA* boxa, l_int32 quality, l_float32 scalefactor, const char* title, l_uint8** pdata, size_t* pnbytes);

		l_ok pixConvertToPdfData(PIX* pix, l_int32 type, l_int32 quality, l_uint8** pdata, size_t* pnbytes, l_int32 x, l_int32 y, l_int32 res, const char* title, L_PDF_DATA** plpd, l_int32 position);

		l_ok l_generateCIDataForPdf(const char* fname, PIX* pix, l_int32 quality, L_COMP_DATA** pcid);
		L_COMP_DATA* l_generateFlateDataPdf(const char* fname, PIX* pix);

		l_ok pixGenerateCIData(PIX* pixs, l_int32 type, l_int32 quality, l_int32 ascii85, L_COMP_DATA** pcid);

		PIX* pixCreate(l_int32 width, l_int32 height, l_int32 depth);
		PIX* pixCreateNoInit(l_int32 width, l_int32 height, l_int32 depth);
		PIX* pixCreateTemplate(const PIX* pixs);
		PIX* pixCreateTemplateNoInit(const PIX* pixs);
		PIX* pixCreateWithCmap(l_int32 width, l_int32 height, l_int32 depth, l_int32 initcolor);
		PIX* pixCreateHeader(l_int32 width, l_int32 height, l_int32 depth);
		PIX* pixClone(PIX* pixs);
		void pixDestroy(PIX** ppix);
		PIX* pixCopy(PIX* pixd, const PIX* pixs);
		l_ok pixResizeImageData(PIX* pixd, const PIX* pixs);
		l_ok pixCopyColormap(PIX* pixd, const PIX* pixs);
		l_ok pixTransferAllData(PIX* pixd, PIX** ppixs, l_int32 copytext, l_int32 copyformat);
		l_ok pixSwapAndDestroy(PIX** ppixd, PIX** ppixs);
		l_int32 pixGetWidth(const PIX* pix);
		l_int32 pixSetWidth(PIX* pix, l_int32 width);
		l_int32 pixGetHeight(const PIX* pix);
		l_int32 pixSetHeight(PIX* pix, l_int32 height);
		l_int32 pixGetDepth(const PIX* pix);
		l_int32 pixSetDepth(PIX* pix, l_int32 depth);
		l_ok pixGetDimensions(const PIX* pix, l_int32* pw, l_int32* ph, l_int32* pd);
		l_ok pixSetDimensions(PIX* pix, l_int32 w, l_int32 h, l_int32 d);
		l_ok pixCopyDimensions(PIX* pixd, const PIX* pixs);
		l_int32 pixGetSpp(const PIX* pix);
		l_int32 pixSetSpp(PIX* pix, l_int32 spp);
		l_ok pixCopySpp(PIX* pixd, const PIX* pixs);
		l_int32 pixGetWpl(const PIX* pix);
		l_int32 pixSetWpl(PIX* pix, l_int32 wpl);
		l_int32 pixGetXRes(const PIX* pix);
		l_int32 pixSetXRes(PIX* pix, l_int32 res);
		l_int32 pixGetYRes(const PIX* pix);
		l_int32 pixSetYRes(PIX* pix, l_int32 res);
		l_ok pixGetResolution(const PIX* pix, l_int32* pxres, l_int32* pyres);
		l_ok pixSetResolution(PIX* pix, l_int32 xres, l_int32 yres);
		l_int32 pixCopyResolution(PIX* pixd, const PIX* pixs);
		l_int32 pixScaleResolution(PIX* pix, l_float32 xscale, l_float32 yscale);
		l_int32 pixGetInputFormat(const PIX* pix);
		l_int32 pixSetInputFormat(PIX* pix, l_int32 informat);
		l_int32 pixCopyInputFormat(PIX* pixd, const PIX* pixs);
		l_int32 pixSetSpecial(PIX* pix, l_int32 special);
		char* pixGetText(PIX* pix);
		l_ok pixSetText(PIX* pix, const char* textstring);
		l_ok pixAddText(PIX* pix, const char* textstring);
		l_int32 pixCopyText(PIX* pixd, const PIX* pixs);
		l_uint8* pixGetTextCompNew(PIX* pix, size_t* psize);
		l_ok pixSetTextCompNew(PIX* pix, const l_uint8* data, size_t size);
		PIXCMAP* pixGetColormap(PIX* pix);
		l_ok pixSetColormap(PIX* pix, PIXCMAP* colormap);
		l_ok pixDestroyColormap(PIX* pix);
		l_uint32* pixGetData(PIX* pix);
		l_int32 pixFreeAndSetData(PIX* pix, l_uint32* data);
		l_int32 pixSetData(PIX* pix, l_uint32* data);
		l_int32 pixFreeData(PIX* pix);
		l_uint32* pixExtractData(PIX* pixs);
		void** pixGetLinePtrs(PIX* pix, l_int32* psize);
		l_int32 pixSizesEqual(const PIX* pix1, const PIX* pix2);
		l_ok pixMaxAspectRatio(PIX* pixs, l_float32* pratio);
		l_ok pixPrintStreamInfo(FILE* fp, const PIX* pix, const char* text);
		l_ok pixGetPixel(PIX* pix, l_int32 x, l_int32 y, l_uint32* pval);
		l_ok pixSetPixel(PIX* pix, l_int32 x, l_int32 y, l_uint32 val);
		l_ok pixGetRGBPixel(PIX* pix, l_int32 x, l_int32 y, l_int32* prval, l_int32* pgval, l_int32* pbval);
		l_ok pixSetRGBPixel(PIX* pix, l_int32 x, l_int32 y, l_int32 rval, l_int32 gval, l_int32 bval);
		l_ok pixSetCmapPixel(PIX* pix, l_int32 x, l_int32 y, l_int32 rval, l_int32 gval, l_int32 bval);
		l_ok pixGetRandomPixel(PIX* pix, l_uint32* pval, l_int32* px, l_int32* py);
		l_ok pixClearPixel(PIX* pix, l_int32 x, l_int32 y);
		l_ok pixFlipPixel(PIX* pix, l_int32 x, l_int32 y);
		void setPixelLow(l_uint32* line, l_int32 x, l_int32 depth, l_uint32 val);
		l_ok pixGetBlackOrWhiteVal(PIX* pixs, l_int32 op, l_uint32* pval);
		l_ok pixClearAll(PIX* pix);
		l_ok pixSetAll(PIX* pix);
		l_ok pixSetAllGray(PIX* pix, l_int32 grayval);
		l_ok pixSetAllArbitrary(PIX* pix, l_uint32 val);
		l_ok pixSetBlackOrWhite(PIX* pixs, l_int32 op);
		l_ok pixSetComponentArbitrary(PIX* pix, l_int32 comp, l_int32 val);
		l_ok pixClearInRect(PIX* pix, BOX* box);
		l_ok pixSetInRect(PIX* pix, BOX* box);
		l_ok pixSetInRectArbitrary(PIX* pix, BOX* box, l_uint32 val);
		l_ok pixBlendInRect(PIX* pixs, BOX* box, l_uint32 val, l_float32 fract);
		l_ok pixSetPadBits(PIX* pix, l_int32 val);
		l_ok pixSetPadBitsBand(PIX* pix, l_int32 by, l_int32 bh, l_int32 val);
		l_ok pixSetOrClearBorder(PIX* pixs, l_int32 left, l_int32 right, l_int32 top, l_int32 bot, l_int32 op);
		l_ok pixSetBorderVal(PIX* pixs, l_int32 left, l_int32 right, l_int32 top, l_int32 bot, l_uint32 val);
		l_ok pixSetBorderRingVal(PIX* pixs, l_int32 dist, l_uint32 val);
		l_ok pixSetMirroredBorder(PIX* pixs, l_int32 left, l_int32 right, l_int32 top, l_int32 bot);
		PIX* pixCopyBorder(PIX* pixd, PIX* pixs, l_int32 left, l_int32 right, l_int32 top, l_int32 bot);
		PIX* pixAddBorder(PIX* pixs, l_int32 npix, l_uint32 val);
		PIX* pixAddBlackOrWhiteBorder(PIX* pixs, l_int32 left, l_int32 right, l_int32 top, l_int32 bot, l_int32 op);
		PIX* pixAddBorderGeneral(PIX* pixs, l_int32 left, l_int32 right, l_int32 top, l_int32 bot, l_uint32 val);
		PIX* pixAddMultipleBlackWhiteBorders(PIX* pixs, l_int32 nblack1, l_int32 nwhite1, l_int32 nblack2, l_int32 nwhite2, l_int32 nblack3, l_int32 nwhite3);
		PIX* pixRemoveBorder(PIX* pixs, l_int32 npix);
		PIX* pixRemoveBorderGeneral(PIX* pixs, l_int32 left, l_int32 right, l_int32 top, l_int32 bot);
		PIX* pixRemoveBorderToSize(PIX* pixs, l_int32 wd, l_int32 hd);
		PIX* pixAddMirroredBorder(PIX* pixs, l_int32 left, l_int32 right, l_int32 top, l_int32 bot);
		PIX* pixAddRepeatedBorder(PIX* pixs, l_int32 left, l_int32 right, l_int32 top, l_int32 bot);
		PIX* pixAddMixedBorder(PIX* pixs, l_int32 left, l_int32 right, l_int32 top, l_int32 bot);
		PIX* pixAddContinuedBorder(PIX* pixs, l_int32 left, l_int32 right, l_int32 top, l_int32 bot);
		l_ok pixShiftAndTransferAlpha(PIX* pixd, PIX* pixs, l_float32 shiftx, l_float32 shifty);
		PIX* pixDisplayLayersRGBA(PIX* pixs, l_uint32 val, l_int32 maxw);
		PIX* pixCreateRGBImage(PIX* pixr, PIX* pixg, PIX* pixb);
		PIX* pixGetRGBComponent(PIX* pixs, l_int32 comp);
		l_ok pixSetRGBComponent(PIX* pixd, PIX* pixs, l_int32 comp);
		PIX* pixGetRGBComponentCmap(PIX* pixs, l_int32 comp);
		l_ok pixCopyRGBComponent(PIX* pixd, PIX* pixs, l_int32 comp);

		l_ok pixGetRGBLine(PIX* pixs, l_int32 row, l_uint8* bufr, l_uint8* bufg, l_uint8* bufb);

		PIX* pixEndianByteSwapNew(PIX* pixs);
		l_ok pixEndianByteSwap(PIX* pixs);

		PIX* pixEndianTwoByteSwapNew(PIX* pixs);
		l_ok pixEndianTwoByteSwap(PIX* pixs);
		l_ok pixGetRasterData(PIX* pixs, l_uint8** pdata, size_t* pnbytes);
		l_ok pixInferResolution(PIX* pix, l_float32 longside, l_int32* pres);
		l_ok pixAlphaIsOpaque(PIX* pix, l_int32* popaque);
		l_uint8** pixSetupByteProcessing(PIX* pix, l_int32* pw, l_int32* ph);
		l_ok pixCleanupByteProcessing(PIX* pix, l_uint8** lineptrs);

		l_ok pixSetMasked(PIX* pixd, PIX* pixm, l_uint32 val);
		l_ok pixSetMaskedGeneral(PIX* pixd, PIX* pixm, l_uint32 val, l_int32 x, l_int32 y);
		l_ok pixCombineMasked(PIX* pixd, PIX* pixs, PIX* pixm);
		l_ok pixCombineMaskedGeneral(PIX* pixd, PIX* pixs, PIX* pixm, l_int32 x, l_int32 y);
		l_ok pixPaintThroughMask(PIX* pixd, PIX* pixm, l_int32 x, l_int32 y, l_uint32 val);
		PIX* pixCopyWithBoxa(PIX* pixs, BOXA* boxa, l_int32 background);
		l_ok pixPaintSelfThroughMask(PIX* pixd, PIX* pixm, l_int32 x, l_int32 y, l_int32 searchdir, l_int32 mindist, l_int32 tilesize, l_int32 ntiles, l_int32 distblend);
		PIX* pixMakeMaskFromVal(PIX* pixs, l_int32 val);
		PIX* pixMakeMaskFromLUT(PIX* pixs, l_int32* tab);
		PIX* pixMakeArbMaskFromRGB(PIX* pixs, l_float32 rc, l_float32 gc, l_float32 bc, l_float32 thresh);
		PIX* pixSetUnderTransparency(PIX* pixs, l_uint32 val, l_int32 debug);
		PIX* pixMakeAlphaFromMask(PIX* pixs, l_int32 dist, BOX** pbox);
		l_ok pixGetColorNearMaskBoundary(PIX* pixs, PIX* pixm, BOX* box, l_int32 dist, l_uint32* pval, l_int32 debug);
		PIX* pixDisplaySelectedPixels(PIX* pixs, PIX* pixm, SEL* sel, l_uint32 val);
		PIX* pixInvert(PIX* pixd, PIX* pixs);
		PIX* pixOr(PIX* pixd, PIX* pixs1, PIX* pixs2);
		PIX* pixAnd(PIX* pixd, PIX* pixs1, PIX* pixs2);
		PIX* pixXor(PIX* pixd, PIX* pixs1, PIX* pixs2);
		PIX* pixSubtract(PIX* pixd, PIX* pixs1, PIX* pixs2);
		l_ok pixZero(PIX* pix, l_int32* pempty);
		l_ok pixForegroundFraction(PIX* pix, l_float32* pfract);
		NUMA* pixaCountPixels(PIXA* pixa);
		l_ok pixCountPixels(PIX* pixs, l_int32* pcount, l_int32* tab8);
		l_ok pixCountPixelsInRect(PIX* pixs, BOX* box, l_int32* pcount, l_int32* tab8);
		NUMA* pixCountByRow(PIX* pix, BOX* box);
		NUMA* pixCountByColumn(PIX* pix, BOX* box);
		NUMA* pixCountPixelsByRow(PIX* pix, l_int32* tab8);
		NUMA* pixCountPixelsByColumn(PIX* pix);
		l_ok pixCountPixelsInRow(PIX* pix, l_int32 row, l_int32* pcount, l_int32* tab8);
		NUMA* pixGetMomentByColumn(PIX* pix, l_int32 order);
		l_ok pixThresholdPixelSum(PIX* pix, l_int32 thresh, l_int32* pabove, l_int32* tab8);

		NUMA* pixAverageByRow(PIX* pix, BOX* box, l_int32 type);
		NUMA* pixAverageByColumn(PIX* pix, BOX* box, l_int32 type);
		l_ok pixAverageInRect(PIX* pixs, PIX* pixm, BOX* box, l_int32 minval, l_int32 maxval, l_int32 subsamp, l_float32* pave);
		l_ok pixAverageInRectRGB(PIX* pixs, PIX* pixm, BOX* box, l_int32 subsamp, l_uint32* pave);
		NUMA* pixVarianceByRow(PIX* pix, BOX* box);
		NUMA* pixVarianceByColumn(PIX* pix, BOX* box);
		l_ok pixVarianceInRect(PIX* pix, BOX* box, l_float32* prootvar);
		NUMA* pixAbsDiffByRow(PIX* pix, BOX* box);
		NUMA* pixAbsDiffByColumn(PIX* pix, BOX* box);
		l_ok pixAbsDiffInRect(PIX* pix, BOX* box, l_int32 dir, l_float32* pabsdiff);
		l_ok pixAbsDiffOnLine(PIX* pix, l_int32 x1, l_int32 y1, l_int32 x2, l_int32 y2, l_float32* pabsdiff);
		l_int32 pixCountArbInRect(PIX* pixs, BOX* box, l_int32 val, l_int32 factor, l_int32* pcount);
		PIX* pixMirroredTiling(PIX* pixs, l_int32 w, l_int32 h);
		l_ok pixFindRepCloseTile(PIX* pixs, BOX* box, l_int32 searchdir, l_int32 mindist, l_int32 tsize, l_int32 ntiles, BOX** pboxtile, l_int32 debug);
		NUMA* pixGetGrayHistogram(PIX* pixs, l_int32 factor);
		NUMA* pixGetGrayHistogramMasked(PIX* pixs, PIX* pixm, l_int32 x, l_int32 y, l_int32 factor);
		NUMA* pixGetGrayHistogramInRect(PIX* pixs, BOX* box, l_int32 factor);
		NUMAA* pixGetGrayHistogramTiled(PIX* pixs, l_int32 factor, l_int32 nx, l_int32 ny);
		l_ok pixGetColorHistogram(PIX* pixs, l_int32 factor, NUMA** pnar, NUMA** pnag, NUMA** pnab);
		l_ok pixGetColorHistogramMasked(PIX* pixs, PIX* pixm, l_int32 x, l_int32 y, l_int32 factor, NUMA** pnar, NUMA** pnag, NUMA** pnab);
		NUMA* pixGetCmapHistogram(PIX* pixs, l_int32 factor);
		NUMA* pixGetCmapHistogramMasked(PIX* pixs, PIX* pixm, l_int32 x, l_int32 y, l_int32 factor);
		NUMA* pixGetCmapHistogramInRect(PIX* pixs, BOX* box, l_int32 factor);
		l_ok pixCountRGBColorsByHash(PIX* pixs, l_int32* pncolors);
		l_ok pixCountRGBColors(PIX* pixs, l_int32 factor, l_int32* pncolors);
		L_AMAP* pixGetColorAmapHistogram(PIX* pixs, l_int32 factor);
		l_int32 amapGetCountForColor(L_AMAP* amap, l_uint32 val);
		l_ok pixGetRankValue(PIX* pixs, l_int32 factor, l_float32 rank, l_uint32* pvalue);
		l_ok pixGetRankValueMaskedRGB(PIX* pixs, PIX* pixm, l_int32 x, l_int32 y, l_int32 factor, l_float32 rank, l_float32* prval, l_float32* pgval, l_float32* pbval);
		l_ok pixGetRankValueMasked(PIX* pixs, PIX* pixm, l_int32 x, l_int32 y, l_int32 factor, l_float32 rank, l_float32* pval, NUMA** pna);
		l_ok pixGetPixelAverage(PIX* pixs, PIX* pixm, l_int32 x, l_int32 y, l_int32 factor, l_uint32* pval);
		l_ok pixGetPixelStats(PIX* pixs, l_int32 factor, l_int32 type, l_uint32* pvalue);
		l_ok pixGetAverageMaskedRGB(PIX* pixs, PIX* pixm, l_int32 x, l_int32 y, l_int32 factor, l_int32 type, l_float32* prval, l_float32* pgval, l_float32* pbval);
		l_ok pixGetAverageMasked(PIX* pixs, PIX* pixm, l_int32 x, l_int32 y, l_int32 factor, l_int32 type, l_float32* pval);
		l_ok pixGetAverageTiledRGB(PIX* pixs, l_int32 sx, l_int32 sy, l_int32 type, PIX** ppixr, PIX** ppixg, PIX** ppixb);
		PIX* pixGetAverageTiled(PIX* pixs, l_int32 sx, l_int32 sy, l_int32 type);
		l_int32 pixRowStats(PIX* pixs, BOX* box, NUMA** pnamean, NUMA** pnamedian, NUMA** pnamode, NUMA** pnamodecount, NUMA** pnavar, NUMA** pnarootvar);
		l_int32 pixColumnStats(PIX* pixs, BOX* box, NUMA** pnamean, NUMA** pnamedian, NUMA** pnamode, NUMA** pnamodecount, NUMA** pnavar, NUMA** pnarootvar);
		l_ok pixGetRangeValues(PIX* pixs, l_int32 factor, l_int32 color, l_int32* pminval, l_int32* pmaxval);
		l_ok pixGetExtremeValue(PIX* pixs, l_int32 factor, l_int32 type, l_int32* prval, l_int32* pgval, l_int32* pbval, l_int32* pgrayval);
		l_ok pixGetMaxValueInRect(PIX* pixs, BOX* box, l_uint32* pmaxval, l_int32* pxmax, l_int32* pymax);
		l_ok pixGetMaxColorIndex(PIX* pixs, l_int32* pmaxindex);
		l_ok pixGetBinnedComponentRange(PIX* pixs, l_int32 nbins, l_int32 factor, l_int32 color, l_int32* pminval, l_int32* pmaxval, l_uint32** pcarray, l_int32 fontsize);
		l_ok pixGetRankColorArray(PIX* pixs, l_int32 nbins, l_int32 type, l_int32 factor, l_uint32** pcarray, PIXA* pixadb, l_int32 fontsize);
		l_ok pixGetBinnedColor(PIX* pixs, PIX* pixg, l_int32 factor, l_int32 nbins, l_uint32** pcarray, PIXA* pixadb);
		PIX* pixDisplayColorArray(l_uint32* carray, l_int32 ncolors, l_int32 side, l_int32 ncols, l_int32 fontsize);
		PIX* pixRankBinByStrip(PIX* pixs, l_int32 direction, l_int32 size, l_int32 nbins, l_int32 type);
		PIX* pixaGetAlignedStats(PIXA* pixa, l_int32 type, l_int32 nbins, l_int32 thresh);
		l_ok pixaExtractColumnFromEachPix(PIXA* pixa, l_int32 col, PIX* pixd);
		l_ok pixGetRowStats(PIX* pixs, l_int32 type, l_int32 nbins, l_int32 thresh, l_float32* colvect);
		l_ok pixGetColumnStats(PIX* pixs, l_int32 type, l_int32 nbins, l_int32 thresh, l_float32* rowvect);
		l_ok pixSetPixelColumn(PIX* pix, l_int32 col, l_float32* colvect);
		l_ok pixThresholdForFgBg(PIX* pixs, l_int32 factor, l_int32 thresh, l_int32* pfgval, l_int32* pbgval);
		l_ok pixSplitDistributionFgBg(PIX* pixs, l_float32 scorefract, l_int32 factor, l_int32* pthresh, l_int32* pfgval, l_int32* pbgval, PIX** ppixdb);
		l_ok pixaFindDimensions(PIXA* pixa, NUMA** pnaw, NUMA** pnah);
		l_ok pixFindAreaPerimRatio(PIX* pixs, l_int32* tab, l_float32* pfract);
		NUMA* pixaFindPerimToAreaRatio(PIXA* pixa);
		l_ok pixFindPerimToAreaRatio(PIX* pixs, l_int32* tab, l_float32* pfract);
		NUMA* pixaFindPerimSizeRatio(PIXA* pixa);
		l_ok pixFindPerimSizeRatio(PIX* pixs, l_int32* tab, l_float32* pratio);
		NUMA* pixaFindAreaFraction(PIXA* pixa);
		l_ok pixFindAreaFraction(PIX* pixs, l_int32* tab, l_float32* pfract);
		NUMA* pixaFindAreaFractionMasked(PIXA* pixa, PIX* pixm, l_int32 debug);
		l_ok pixFindAreaFractionMasked(PIX* pixs, BOX* box, PIX* pixm, l_int32* tab, l_float32* pfract);
		NUMA* pixaFindWidthHeightRatio(PIXA* pixa);
		NUMA* pixaFindWidthHeightProduct(PIXA* pixa);
		l_ok pixFindOverlapFraction(PIX* pixs1, PIX* pixs2, l_int32 x2, l_int32 y2, l_int32* tab, l_float32* pratio, l_int32* pnoverlap);
		BOXA* pixFindRectangleComps(PIX* pixs, l_int32 dist, l_int32 minw, l_int32 minh);
		l_ok pixConformsToRectangle(PIX* pixs, BOX* box, l_int32 dist, l_int32* pconforms);
		PIXA* pixClipRectangles(PIX* pixs, BOXA* boxa);
		PIX* pixClipRectangle(PIX* pixs, BOX* box, BOX** pboxc);
		PIX* pixClipRectangleWithBorder(PIX* pixs, BOX* box, l_int32 maxbord, BOX** pboxn);
		PIX* pixClipMasked(PIX* pixs, PIX* pixm, l_int32 x, l_int32 y, l_uint32 outval);
		l_ok pixCropToMatch(PIX* pixs1, PIX* pixs2, PIX** ppixd1, PIX** ppixd2);
		PIX* pixCropToSize(PIX* pixs, l_int32 w, l_int32 h);
		PIX* pixResizeToMatch(PIX* pixs, PIX* pixt, l_int32 w, l_int32 h);
		PIX* pixSelectComponentBySize(PIX* pixs, l_int32 rankorder, l_int32 type, l_int32 connectivity, BOX** pbox);
		PIX* pixFilterComponentBySize(PIX* pixs, l_int32 rankorder, l_int32 type, l_int32 connectivity, BOX** pbox);
		PIX* pixMakeSymmetricMask(l_int32 w, l_int32 h, l_float32 hf, l_float32 vf, l_int32 type);
		PIX* pixMakeFrameMask(l_int32 w, l_int32 h, l_float32 hf1, l_float32 hf2, l_float32 vf1, l_float32 vf2);
		PIX* pixMakeCoveringOfRectangles(PIX* pixs, l_int32 maxiters);
		l_ok pixFractionFgInMask(PIX* pix1, PIX* pix2, l_float32* pfract);
		l_ok pixClipToForeground(PIX* pixs, PIX** ppixd, BOX** pbox);
		l_ok pixTestClipToForeground(PIX* pixs, l_int32* pcanclip);
		l_ok pixClipBoxToForeground(PIX* pixs, BOX* boxs, PIX** ppixd, BOX** pboxd);
		l_ok pixScanForForeground(PIX* pixs, BOX* box, l_int32 scanflag, l_int32* ploc);
		l_ok pixClipBoxToEdges(PIX* pixs, BOX* boxs, l_int32 lowthresh, l_int32 highthresh, l_int32 maxwidth, l_int32 factor, PIX** ppixd, BOX** pboxd);
		l_ok pixScanForEdge(PIX* pixs, BOX* box, l_int32 lowthresh, l_int32 highthresh, l_int32 maxwidth, l_int32 factor, l_int32 scanflag, l_int32* ploc);
		NUMA* pixExtractOnLine(PIX* pixs, l_int32 x1, l_int32 y1, l_int32 x2, l_int32 y2, l_int32 factor);
		l_float32 pixAverageOnLine(PIX* pixs, l_int32 x1, l_int32 y1, l_int32 x2, l_int32 y2, l_int32 factor);
		NUMA* pixAverageIntensityProfile(PIX* pixs, l_float32 fract, l_int32 dir, l_int32 first, l_int32 last, l_int32 factor1, l_int32 factor2);
		NUMA* pixReversalProfile(PIX* pixs, l_float32 fract, l_int32 dir, l_int32 first, l_int32 last, l_int32 minreversal, l_int32 factor1, l_int32 factor2);
		l_ok pixWindowedVarianceOnLine(PIX* pixs, l_int32 dir, l_int32 loc, l_int32 c1, l_int32 c2, l_int32 size, NUMA** pnad);
		l_ok pixMinMaxNearLine(PIX* pixs, l_int32 x1, l_int32 y1, l_int32 x2, l_int32 y2, l_int32 dist, l_int32 direction, NUMA** pnamin, NUMA** pnamax, l_float32* pminave, l_float32* pmaxave);
		PIX* pixRankRowTransform(PIX* pixs);
		PIX* pixRankColumnTransform(PIX* pixs);

		PIXA* pixaCreate(l_int32 n);
		PIXA* pixaCreateFromPix(PIX* pixs, l_int32 n, l_int32 cellw, l_int32 cellh);
		PIXA* pixaCreateFromBoxa(PIX* pixs, BOXA* boxa, l_int32 start, l_int32 num, l_int32* pcropwarn);
		PIXA* pixaSplitPix(PIX* pixs, l_int32 nx, l_int32 ny, l_int32 borderwidth, l_uint32 bordercolor);
		void pixaDestroy(PIXA** ppixa);
		PIXA* pixaCopy(PIXA* pixa, l_int32 copyflag);
		l_ok pixaAddPix(PIXA* pixa, PIX* pix, l_int32 copyflag);
		l_ok pixaAddBox(PIXA* pixa, BOX* box, l_int32 copyflag);
		l_ok pixaExtendArrayToSize(PIXA* pixa, size_t size);
		l_int32 pixaGetCount(PIXA* pixa);
		PIX* pixaGetPix(PIXA* pixa, l_int32 index, l_int32 accesstype);
		l_ok pixaGetPixDimensions(PIXA* pixa, l_int32 index, l_int32* pw, l_int32* ph, l_int32* pd);
		BOXA* pixaGetBoxa(PIXA* pixa, l_int32 accesstype);
		l_int32 pixaGetBoxaCount(PIXA* pixa);
		BOX* pixaGetBox(PIXA* pixa, l_int32 index, l_int32 accesstype);
		l_ok pixaGetBoxGeometry(PIXA* pixa, l_int32 index, l_int32* px, l_int32* py, l_int32* pw, l_int32* ph);
		l_ok pixaSetBoxa(PIXA* pixa, BOXA* boxa, l_int32 accesstype);
		PIX** pixaGetPixArray(PIXA* pixa);
		l_ok pixaVerifyDepth(PIXA* pixa, l_int32* psame, l_int32* pmaxd);
		l_ok pixaVerifyDimensions(PIXA* pixa, l_int32* psame, l_int32* pmaxw, l_int32* pmaxh);
		l_ok pixaIsFull(PIXA* pixa, l_int32* pfullpa, l_int32* pfullba);
		l_ok pixaCountText(PIXA* pixa, l_int32* pntext);
		l_ok pixaSetText(PIXA* pixa, const char* text, SARRAY* sa);
		void*** pixaGetLinePtrs(PIXA* pixa, l_int32* psize);
		l_ok pixaWriteStreamInfo(FILE* fp, PIXA* pixa);
		l_ok pixaReplacePix(PIXA* pixa, l_int32 index, PIX* pix, BOX* box);
		l_ok pixaInsertPix(PIXA* pixa, l_int32 index, PIX* pixs, BOX* box);
		l_ok pixaRemovePix(PIXA* pixa, l_int32 index);
		l_ok pixaRemovePixAndSave(PIXA* pixa, l_int32 index, PIX** ppix, BOX** pbox);
		l_ok pixaRemoveSelected(PIXA* pixa, NUMA* naindex);
		l_ok pixaInitFull(PIXA* pixa, PIX* pix, BOX* box);
		l_ok pixaClear(PIXA* pixa);
		l_ok pixaJoin(PIXA* pixad, PIXA* pixas, l_int32 istart, l_int32 iend);
		PIXA* pixaInterleave(PIXA* pixa1, PIXA* pixa2, l_int32 copyflag);
		l_ok pixaaJoin(PIXAA* paad, PIXAA* paas, l_int32 istart, l_int32 iend);
		PIXAA* pixaaCreate(l_int32 n);
		PIXAA* pixaaCreateFromPixa(PIXA* pixa, l_int32 n, l_int32 type, l_int32 copyflag);
		void pixaaDestroy(PIXAA** ppaa);
		l_ok pixaaAddPixa(PIXAA* paa, PIXA* pixa, l_int32 copyflag);
		l_ok pixaaAddPix(PIXAA* paa, l_int32 index, PIX* pix, BOX* box, l_int32 copyflag);
		l_ok pixaaAddBox(PIXAA* paa, BOX* box, l_int32 copyflag);
		l_int32 pixaaGetCount(PIXAA* paa, NUMA** pna);
		PIXA* pixaaGetPixa(PIXAA* paa, l_int32 index, l_int32 accesstype);
		BOXA* pixaaGetBoxa(PIXAA* paa, l_int32 accesstype);
		PIX* pixaaGetPix(PIXAA* paa, l_int32 index, l_int32 ipix, l_int32 accessflag);
		l_ok pixaaVerifyDepth(PIXAA* paa, l_int32* psame, l_int32* pmaxd);
		l_ok pixaaVerifyDimensions(PIXAA* paa, l_int32* psame, l_int32* pmaxw, l_int32* pmaxh);
		l_int32 pixaaIsFull(PIXAA* paa, l_int32* pfull);
		l_ok pixaaInitFull(PIXAA* paa, PIXA* pixa);
		l_ok pixaaReplacePixa(PIXAA* paa, l_int32 index, PIXA* pixa);
		l_ok pixaaClear(PIXAA* paa);
		l_ok pixaaTruncate(PIXAA* paa);
		PIXA* pixaRead(const char* filename);
		PIXA* pixaReadStream(FILE* fp);
		PIXA* pixaReadMem(const l_uint8* data, size_t size);
		l_ok pixaWriteDebug(const char* fname, PIXA* pixa);
		l_ok pixaWrite(const char* filename, PIXA* pixa);
		l_ok pixaWriteStream(FILE* fp, PIXA* pixa);
		l_ok pixaWriteMem(l_uint8** pdata, size_t* psize, PIXA* pixa);
		PIXA* pixaReadBoth(const char* filename);
		PIXAA* pixaaReadFromFiles(const char* dirname, const char* substr, l_int32 first, l_int32 nfiles);
		PIXAA* pixaaRead(const char* filename);
		PIXAA* pixaaReadStream(FILE* fp);
		PIXAA* pixaaReadMem(const l_uint8* data, size_t size);
		l_ok pixaaWrite(const char* filename, PIXAA* paa);
		l_ok pixaaWriteStream(FILE* fp, PIXAA* paa);
		l_ok pixaaWriteMem(l_uint8** pdata, size_t* psize, PIXAA* paa);
		PIXACC* pixaccCreate(l_int32 w, l_int32 h, l_int32 negflag);
		PIXACC* pixaccCreateFromPix(PIX* pix, l_int32 negflag);
		void pixaccDestroy(PIXACC** ppixacc);
		PIX* pixaccFinal(PIXACC* pixacc, l_int32 outdepth);
		PIX* pixaccGetPix(PIXACC* pixacc);
		l_int32 pixaccGetOffset(PIXACC* pixacc);
		l_ok pixaccAdd(PIXACC* pixacc, PIX* pix);
		l_ok pixaccSubtract(PIXACC* pixacc, PIX* pix);
		l_ok pixaccMultConst(PIXACC* pixacc, l_float32 factor);
		l_ok pixaccMultConstAccumulate(PIXACC* pixacc, PIX* pix, l_float32 factor);
		PIX* pixSelectBySize(PIX* pixs, l_int32 width, l_int32 height, l_int32 connectivity, l_int32 type, l_int32 relation, l_int32* pchanged);
		PIXA* pixaSelectBySize(PIXA* pixas, l_int32 width, l_int32 height, l_int32 type, l_int32 relation, l_int32* pchanged);
		NUMA* pixaMakeSizeIndicator(PIXA* pixa, l_int32 width, l_int32 height, l_int32 type, l_int32 relation);
		PIX* pixSelectByPerimToAreaRatio(PIX* pixs, l_float32 thresh, l_int32 connectivity, l_int32 type, l_int32* pchanged);
		PIXA* pixaSelectByPerimToAreaRatio(PIXA* pixas, l_float32 thresh, l_int32 type, l_int32* pchanged);
		PIX* pixSelectByPerimSizeRatio(PIX* pixs, l_float32 thresh, l_int32 connectivity, l_int32 type, l_int32* pchanged);
		PIXA* pixaSelectByPerimSizeRatio(PIXA* pixas, l_float32 thresh, l_int32 type, l_int32* pchanged);
		PIX* pixSelectByAreaFraction(PIX* pixs, l_float32 thresh, l_int32 connectivity, l_int32 type, l_int32* pchanged);
		PIXA* pixaSelectByAreaFraction(PIXA* pixas, l_float32 thresh, l_int32 type, l_int32* pchanged);
		PIX* pixSelectByArea(PIX* pixs, l_float32 thresh, l_int32 connectivity, l_int32 type, l_int32* pchanged);
		PIXA* pixaSelectByArea(PIXA* pixas, l_float32 thresh, l_int32 type, l_int32* pchanged);
		PIX* pixSelectByWidthHeightRatio(PIX* pixs, l_float32 thresh, l_int32 connectivity, l_int32 type, l_int32* pchanged);
		PIXA* pixaSelectByWidthHeightRatio(PIXA* pixas, l_float32 thresh, l_int32 type, l_int32* pchanged);
		PIXA* pixaSelectByNumConnComp(PIXA* pixas, l_int32 nmin, l_int32 nmax, l_int32 connectivity, l_int32* pchanged);
		PIXA* pixaSelectWithIndicator(PIXA* pixas, NUMA* na, l_int32* pchanged);
		l_ok pixRemoveWithIndicator(PIX* pixs, PIXA* pixa, NUMA* na);
		l_ok pixAddWithIndicator(PIX* pixs, PIXA* pixa, NUMA* na);
		PIXA* pixaSelectWithString(PIXA* pixas, const char* str, l_int32* perror);
		PIX* pixaRenderComponent(PIX* pixs, PIXA* pixa, l_int32 index);
		PIXA* pixaSort(PIXA* pixas, l_int32 sorttype, l_int32 sortorder, NUMA** pnaindex, l_int32 copyflag);
		PIXA* pixaBinSort(PIXA* pixas, l_int32 sorttype, l_int32 sortorder, NUMA** pnaindex, l_int32 copyflag);
		PIXA* pixaSortByIndex(PIXA* pixas, NUMA* naindex, l_int32 copyflag);
		PIXAA* pixaSort2dByIndex(PIXA* pixas, NUMAA* naa, l_int32 copyflag);
		PIXA* pixaSelectRange(PIXA* pixas, l_int32 first, l_int32 last, l_int32 copyflag);
		PIXAA* pixaaSelectRange(PIXAA* paas, l_int32 first, l_int32 last, l_int32 copyflag);
		PIXAA* pixaaScaleToSize(PIXAA* paas, l_int32 wd, l_int32 hd);
		PIXAA* pixaaScaleToSizeVar(PIXAA* paas, NUMA* nawd, NUMA* nahd);
		PIXA* pixaScaleToSize(PIXA* pixas, l_int32 wd, l_int32 hd);
		PIXA* pixaScaleToSizeRel(PIXA* pixas, l_int32 delw, l_int32 delh);
		PIXA* pixaScale(PIXA* pixas, l_float32 scalex, l_float32 scaley);
		PIXA* pixaScaleBySampling(PIXA* pixas, l_float32 scalex, l_float32 scaley);
		PIXA* pixaRotate(PIXA* pixas, l_float32 angle, l_int32 type, l_int32 incolor, l_int32 width, l_int32 height);
		PIXA* pixaRotateOrth(PIXA* pixas, l_int32 rotation);
		PIXA* pixaTranslate(PIXA* pixas, l_int32 hshift, l_int32 vshift, l_int32 incolor);
		PIXA* pixaAddBorderGeneral(PIXA* pixad, PIXA* pixas, l_int32 left, l_int32 right, l_int32 top, l_int32 bot, l_uint32 val);
		PIXA* pixaaFlattenToPixa(PIXAA* paa, NUMA** pnaindex, l_int32 copyflag);
		l_ok pixaaSizeRange(PIXAA* paa, l_int32* pminw, l_int32* pminh, l_int32* pmaxw, l_int32* pmaxh);
		l_ok pixaSizeRange(PIXA* pixa, l_int32* pminw, l_int32* pminh, l_int32* pmaxw, l_int32* pmaxh);
		PIXA* pixaClipToPix(PIXA* pixas, PIX* pixs);
		l_ok pixaClipToForeground(PIXA* pixas, PIXA** ppixad, BOXA** pboxa);
		l_ok pixaGetRenderingDepth(PIXA* pixa, l_int32* pdepth);
		l_ok pixaHasColor(PIXA* pixa, l_int32* phascolor);
		l_ok pixaAnyColormaps(PIXA* pixa, l_int32* phascmap);
		l_ok pixaGetDepthInfo(PIXA* pixa, l_int32* pmaxdepth, l_int32* psame);
		PIXA* pixaConvertToSameDepth(PIXA* pixas);
		PIXA* pixaConvertToGivenDepth(PIXA* pixas, l_int32 depth);
		l_ok pixaEqual(PIXA* pixa1, PIXA* pixa2, l_int32 maxdist, NUMA** pnaindex, l_int32* psame);
		l_ok pixaSetFullSizeBoxa(PIXA* pixa);
		PIX* pixaDisplay(PIXA* pixa, l_int32 w, l_int32 h);
		PIX* pixaDisplayRandomCmap(PIXA* pixa, l_int32 w, l_int32 h);
		PIX* pixaDisplayLinearly(PIXA* pixas, l_int32 direction, l_float32 scalefactor, l_int32 background, l_int32 spacing, l_int32 border, BOXA** pboxa);
		PIX* pixaDisplayOnLattice(PIXA* pixa, l_int32 cellw, l_int32 cellh, l_int32* pncols, BOXA** pboxa);
		PIX* pixaDisplayUnsplit(PIXA* pixa, l_int32 nx, l_int32 ny, l_int32 borderwidth, l_uint32 bordercolor);
		PIX* pixaDisplayTiled(PIXA* pixa, l_int32 maxwidth, l_int32 background, l_int32 spacing);
		PIX* pixaDisplayTiledInRows(PIXA* pixa, l_int32 outdepth, l_int32 maxwidth, l_float32 scalefactor, l_int32 background, l_int32 spacing, l_int32 border);
		PIX* pixaDisplayTiledInColumns(PIXA* pixas, l_int32 nx, l_float32 scalefactor, l_int32 spacing, l_int32 border);
		PIX* pixaDisplayTiledAndScaled(PIXA* pixa, l_int32 outdepth, l_int32 tilewidth, l_int32 ncols, l_int32 background, l_int32 spacing, l_int32 border);
		PIX* pixaDisplayTiledWithText(PIXA* pixa, l_int32 maxwidth, l_float32 scalefactor, l_int32 spacing, l_int32 border, l_int32 fontsize, l_uint32 textcolor);
		PIX* pixaDisplayTiledByIndex(PIXA* pixa, NUMA* na, l_int32 width, l_int32 spacing, l_int32 border, l_int32 fontsize, l_uint32 textcolor);
		PIX* pixaDisplayPairTiledInColumns(PIXA* pixas1, PIXA* pixas2, l_int32 nx, l_float32 scalefactor, l_int32 spacing1, l_int32 spacing2, l_int32 border1, l_int32 border2, l_int32 fontsize, l_int32 startindex, SARRAY* sa);
		PIX* pixaaDisplay(PIXAA* paa, l_int32 w, l_int32 h);
		PIX* pixaaDisplayByPixa(PIXAA* paa, l_int32 maxnx, l_float32 scalefactor, l_int32 hspacing, l_int32 vspacing, l_int32 border);
		PIXA* pixaaDisplayTiledAndScaled(PIXAA* paa, l_int32 outdepth, l_int32 tilewidth, l_int32 ncols, l_int32 background, l_int32 spacing, l_int32 border);
		PIXA* pixaConvertTo1(PIXA* pixas, l_int32 thresh);
		PIXA* pixaConvertTo8(PIXA* pixas, l_int32 cmapflag);
		PIXA* pixaConvertTo8Colormap(PIXA* pixas, l_int32 dither);
		PIXA* pixaConvertTo32(PIXA* pixas);
		PIXA* pixaConstrainedSelect(PIXA* pixas, l_int32 first, l_int32 last, l_int32 nmax, l_int32 use_pairs, l_int32 copyflag);
		l_ok pixaSelectToPdf(PIXA* pixas, l_int32 first, l_int32 last, l_int32 res, l_float32 scalefactor, l_int32 type, l_int32 quality, l_uint32 color, l_int32 fontsize, const char* fileout);
		PIXA* pixaMakeFromTiledPixa(PIXA* pixas, l_int32 w, l_int32 h, l_int32 nsamp);
		PIXA* pixaMakeFromTiledPix(PIX* pixs, l_int32 w, l_int32 h, l_int32 start, l_int32 num, BOXA* boxa);
		l_ok pixGetTileCount(PIX* pix, l_int32* pn);
		PIXA* pixaDisplayMultiTiled(PIXA* pixas, l_int32 nx, l_int32 ny, l_int32 maxw, l_int32 maxh, l_float32 scalefactor, l_int32 spacing, l_int32 border);
		l_ok pixaSplitIntoFiles(PIXA* pixas, l_int32 nsplit, l_float32 scale, l_int32 outwidth, l_int32 write_pixa, l_int32 write_pix, l_int32 write_pdf);

		PIXA* convertToNUpPixa(const char* dir, const char* substr, l_int32 nx, l_int32 ny, l_int32 tw, l_int32 spacing, l_int32 border, l_int32 fontsize);
		PIXA* pixaConvertToNUpPixa(PIXA* pixas, SARRAY* sa, l_int32 nx, l_int32 ny, l_int32 tw, l_int32 spacing, l_int32 border, l_int32 fontsize);
		l_ok pixaCompareInPdf(PIXA* pixa1, PIXA* pixa2, l_int32 nx, l_int32 ny, l_int32 tw, l_int32 spacing, l_int32 border, l_int32 fontsize, const char* fileout);

		l_ok pixAddConstantGray(PIX* pixs, l_int32 val);
		l_ok pixMultConstantGray(PIX* pixs, l_float32 val);
		PIX* pixAddGray(PIX* pixd, PIX* pixs1, PIX* pixs2);
		PIX* pixSubtractGray(PIX* pixd, PIX* pixs1, PIX* pixs2);
		PIX* pixMultiplyGray(PIX* pixs, PIX* pixg, l_float32 norm);
		PIX* pixThresholdToValue(PIX* pixd, PIX* pixs, l_int32 threshval, l_int32 setval);
		PIX* pixInitAccumulate(l_int32 w, l_int32 h, l_uint32 offset);
		PIX* pixFinalAccumulate(PIX* pixs, l_uint32 offset, l_int32 depth);
		PIX* pixFinalAccumulateThreshold(PIX* pixs, l_uint32 offset, l_uint32 threshold);
		l_ok pixAccumulate(PIX* pixd, PIX* pixs, l_int32 op);
		l_ok pixMultConstAccumulate(PIX* pixs, l_float32 factor, l_uint32 offset);
		PIX* pixAbsDifference(PIX* pixs1, PIX* pixs2);
		PIX* pixAddRGB(PIX* pixs1, PIX* pixs2);
		PIX* pixMinOrMax(PIX* pixd, PIX* pixs1, PIX* pixs2, l_int32 type);
		PIX* pixMaxDynamicRange(PIX* pixs, l_int32 type);
		PIX* pixMaxDynamicRangeRGB(PIX* pixs, l_int32 type);

		PIXC* pixcompCreateFromPix(PIX* pix, l_int32 comptype);
		PIXC* pixcompCreateFromString(l_uint8* data, size_t size, l_int32 copyflag);
		PIXC* pixcompCreateFromFile(const char* filename, l_int32 comptype);
		void pixcompDestroy(PIXC** ppixc);
		PIXC* pixcompCopy(PIXC* pixcs);
		l_ok pixcompGetDimensions(PIXC* pixc, l_int32* pw, l_int32* ph, l_int32* pd);
		l_ok pixcompGetParameters(PIXC* pixc, l_int32* pxres, l_int32* pyres, l_int32* pcomptype, l_int32* pcmapflag);

		PIX* pixCreateFromPixcomp(PIXC* pixc);
		PIXAC* pixacompCreate(l_int32 n);
		PIXAC* pixacompCreateWithInit(l_int32 n, l_int32 offset, PIX* pix, l_int32 comptype);
		PIXAC* pixacompCreateFromPixa(PIXA* pixa, l_int32 comptype, l_int32 accesstype);
		PIXAC* pixacompCreateFromFiles(const char* dirname, const char* substr, l_int32 comptype);
		PIXAC* pixacompCreateFromSA(SARRAY* sa, l_int32 comptype);
		void pixacompDestroy(PIXAC** ppixac);
		l_ok pixacompAddPix(PIXAC* pixac, PIX* pix, l_int32 comptype);
		l_ok pixacompAddPixcomp(PIXAC* pixac, PIXC* pixc, l_int32 copyflag);
		l_ok pixacompReplacePix(PIXAC* pixac, l_int32 index, PIX* pix, l_int32 comptype);
		l_ok pixacompReplacePixcomp(PIXAC* pixac, l_int32 index, PIXC* pixc);
		l_ok pixacompAddBox(PIXAC* pixac, BOX* box, l_int32 copyflag);
		l_int32 pixacompGetCount(PIXAC* pixac);
		PIXC* pixacompGetPixcomp(PIXAC* pixac, l_int32 index, l_int32 copyflag);
		PIX* pixacompGetPix(PIXAC* pixac, l_int32 index);
		l_ok pixacompGetPixDimensions(PIXAC* pixac, l_int32 index, l_int32* pw, l_int32* ph, l_int32* pd);
		BOXA* pixacompGetBoxa(PIXAC* pixac, l_int32 accesstype);
		l_int32 pixacompGetBoxaCount(PIXAC* pixac);
		BOX* pixacompGetBox(PIXAC* pixac, l_int32 index, l_int32 accesstype);
		l_ok pixacompGetBoxGeometry(PIXAC* pixac, l_int32 index, l_int32* px, l_int32* py, l_int32* pw, l_int32* ph);
		l_int32 pixacompGetOffset(PIXAC* pixac);
		l_ok pixacompSetOffset(PIXAC* pixac, l_int32 offset);
		PIXA* pixaCreateFromPixacomp(PIXAC* pixac, l_int32 accesstype);
		l_ok pixacompJoin(PIXAC* pixacd, PIXAC* pixacs, l_int32 istart, l_int32 iend);
		PIXAC* pixacompInterleave(PIXAC* pixac1, PIXAC* pixac2);
		PIXAC* pixacompRead(const char* filename);
		PIXAC* pixacompReadStream(FILE* fp);
		PIXAC* pixacompReadMem(const l_uint8* data, size_t size);
		l_ok pixacompWrite(const char* filename, PIXAC* pixac);
		l_ok pixacompWriteStream(FILE* fp, PIXAC* pixac);
		l_ok pixacompWriteMem(l_uint8** pdata, size_t* psize, PIXAC* pixac);
		l_ok pixacompConvertToPdf(PIXAC* pixac, l_int32 res, l_float32 scalefactor, l_int32 type, l_int32 quality, const char* title, const char* fileout);
		l_ok pixacompConvertToPdfData(PIXAC* pixac, l_int32 res, l_float32 scalefactor, l_int32 type, l_int32 quality, const char* title, l_uint8** pdata, size_t* pnbytes);
		l_ok pixacompFastConvertToPdfData(PIXAC* pixac, const char* title, l_uint8** pdata, size_t* pnbytes);
		l_ok pixacompWriteStreamInfo(FILE* fp, PIXAC* pixac, const char* text);
		l_ok pixcompWriteStreamInfo(FILE* fp, PIXC* pixc, const char* text);
		PIX* pixacompDisplayTiledAndScaled(PIXAC* pixac, l_int32 outdepth, l_int32 tilewidth, l_int32 ncols, l_int32 background, l_int32 spacing, l_int32 border);
		l_ok pixacompWriteFiles(PIXAC* pixac, const char* subdir);
		l_ok pixcompWriteFile(const char* rootname, PIXC* pixc);
		PIX* pixThreshold8(PIX* pixs, l_int32 d, l_int32 nlevels, l_int32 cmapflag);
		PIX* pixRemoveColormapGeneral(PIX* pixs, l_int32 type, l_int32 ifnocmap);
		PIX* pixRemoveColormap(PIX* pixs, l_int32 type);
		l_ok pixAddGrayColormap8(PIX* pixs);
		PIX* pixAddMinimalGrayColormap8(PIX* pixs);
		PIX* pixConvertRGBToLuminance(PIX* pixs);
		PIX* pixConvertRGBToGrayGeneral(PIX* pixs, l_int32 type, l_float32 rwt, l_float32 gwt, l_float32 bwt);
		PIX* pixConvertRGBToGray(PIX* pixs, l_float32 rwt, l_float32 gwt, l_float32 bwt);
		PIX* pixConvertRGBToGrayFast(PIX* pixs);
		PIX* pixConvertRGBToGrayMinMax(PIX* pixs, l_int32 type);
		PIX* pixConvertRGBToGraySatBoost(PIX* pixs, l_int32 refval);
		PIX* pixConvertRGBToGrayArb(PIX* pixs, l_float32 rc, l_float32 gc, l_float32 bc);
		PIX* pixConvertRGBToBinaryArb(PIX* pixs, l_float32 rc, l_float32 gc, l_float32 bc, l_int32 thresh, l_int32 relation);
		PIX* pixConvertGrayToColormap(PIX* pixs);
		PIX* pixConvertGrayToColormap8(PIX* pixs, l_int32 mindepth);
		PIX* pixColorizeGray(PIX* pixs, l_uint32 color, l_int32 cmapflag);
		PIX* pixConvertRGBToColormap(PIX* pixs, l_int32 ditherflag);
		PIX* pixConvertCmapTo1(PIX* pixs);
		l_ok pixQuantizeIfFewColors(PIX* pixs, l_int32 maxcolors, l_int32 mingraycolors, l_int32 octlevel, PIX** ppixd);
		PIX* pixConvert16To8(PIX* pixs, l_int32 type);
		PIX* pixConvertGrayToFalseColor(PIX* pixs, l_float32 gamma);
		PIX* pixUnpackBinary(PIX* pixs, l_int32 depth, l_int32 invert);
		PIX* pixConvert1To16(PIX* pixd, PIX* pixs, l_uint16 val0, l_uint16 val1);
		PIX* pixConvert1To32(PIX* pixd, PIX* pixs, l_uint32 val0, l_uint32 val1);
		PIX* pixConvert1To2Cmap(PIX* pixs);
		PIX* pixConvert1To2(PIX* pixd, PIX* pixs, l_int32 val0, l_int32 val1);
		PIX* pixConvert1To4Cmap(PIX* pixs);
		PIX* pixConvert1To4(PIX* pixd, PIX* pixs, l_int32 val0, l_int32 val1);
		PIX* pixConvert1To8Cmap(PIX* pixs);
		PIX* pixConvert1To8(PIX* pixd, PIX* pixs, l_uint8 val0, l_uint8 val1);
		PIX* pixConvert2To8(PIX* pixs, l_uint8 val0, l_uint8 val1, l_uint8 val2, l_uint8 val3, l_int32 cmapflag);
		PIX* pixConvert4To8(PIX* pixs, l_int32 cmapflag);
		PIX* pixConvert8To16(PIX* pixs, l_int32 leftshift);
		PIX* pixConvertTo2(PIX* pixs);
		PIX* pixConvert8To2(PIX* pix);
		PIX* pixConvertTo4(PIX* pixs);
		PIX* pixConvert8To4(PIX* pix);
		PIX* pixConvertTo1Adaptive(PIX* pixs);
		PIX* pixConvertTo1(PIX* pixs, l_int32 threshold);
		PIX* pixConvertTo1BySampling(PIX* pixs, l_int32 factor, l_int32 threshold);
		PIX* pixConvertTo8(PIX* pixs, l_int32 cmapflag);
		PIX* pixConvertTo8BySampling(PIX* pixs, l_int32 factor, l_int32 cmapflag);
		PIX* pixConvertTo8Colormap(PIX* pixs, l_int32 dither);
		PIX* pixConvertTo16(PIX* pixs);
		PIX* pixConvertTo32(PIX* pixs);
		PIX* pixConvertTo32BySampling(PIX* pixs, l_int32 factor);
		PIX* pixConvert8To32(PIX* pixs);
		PIX* pixConvertTo8Or32(PIX* pixs, l_int32 copyflag, l_int32 warnflag);
		PIX* pixConvert24To32(PIX* pixs);
		PIX* pixConvert32To24(PIX* pixs);
		PIX* pixConvert32To16(PIX* pixs, l_int32 type);
		PIX* pixConvert32To8(PIX* pixs, l_int32 type16, l_int32 type8);
		PIX* pixRemoveAlpha(PIX* pixs);
		PIX* pixAddAlphaTo1bpp(PIX* pixd, PIX* pixs);
		PIX* pixConvertLossless(PIX* pixs, l_int32 d);
		PIX* pixConvertForPSWrap(PIX* pixs);
		PIX* pixConvertToSubpixelRGB(PIX* pixs, l_float32 scalex, l_float32 scaley, l_int32 order);
		PIX* pixConvertGrayToSubpixelRGB(PIX* pixs, l_float32 scalex, l_float32 scaley, l_int32 order);
		PIX* pixConvertColorToSubpixelRGB(PIX* pixs, l_float32 scalex, l_float32 scaley, l_int32 order);

		PIX* pixConnCompTransform(PIX* pixs, l_int32 connect, l_int32 depth);
		PIX* pixConnCompAreaTransform(PIX* pixs, l_int32 connect);
		l_ok pixConnCompIncrInit(PIX* pixs, l_int32 conn, PIX** ppixd, PTAA** pptaa, l_int32* pncc);
		l_int32 pixConnCompIncrAdd(PIX* pixs, PTAA* ptaa, l_int32* pncc, l_float32 x, l_float32 y, l_int32 debug);
		l_ok pixGetSortedNeighborValues(PIX* pixs, l_int32 x, l_int32 y, l_int32 conn, l_int32** pneigh, l_int32* pnvals);
		PIX* pixLocToColorTransform(PIX* pixs);
		PIXTILING* pixTilingCreate(PIX* pixs, l_int32 nx, l_int32 ny, l_int32 w, l_int32 h, l_int32 xoverlap, l_int32 yoverlap);
		void pixTilingDestroy(PIXTILING** ppt);
		l_ok pixTilingGetCount(PIXTILING* pt, l_int32* pnx, l_int32* pny);
		l_ok pixTilingGetSize(PIXTILING* pt, l_int32* pw, l_int32* ph);
		PIX* pixTilingGetTile(PIXTILING* pt, l_int32 i, l_int32 j);
		l_ok pixTilingNoStripOnPaint(PIXTILING* pt);
		l_ok pixTilingPaintTile(PIX* pixd, l_int32 i, l_int32 j, PIX* pixs, PIXTILING* pt);
		PIX* pixReadStreamPng(FILE* fp);

		l_ok pixWritePng(const char* filename, PIX* pix, l_float32 gamma);
		l_ok pixWriteStreamPng(FILE* fp, PIX* pix, l_float32 gamma);
		l_ok pixSetZlibCompression(PIX* pix, l_int32 compval);

		PIX* pixReadMemPng(const l_uint8* filedata, size_t filesize);
		l_ok pixWriteMemPng(l_uint8** pfiledata, size_t* pfilesize, PIX* pix, l_float32 gamma);
		PIX* pixReadStreamPnm(FILE* fp);

		l_ok pixWriteStreamPnm(FILE* fp, PIX* pix);
		l_ok pixWriteStreamAsciiPnm(FILE* fp, PIX* pix);
		l_ok pixWriteStreamPam(FILE* fp, PIX* pix);
		PIX* pixReadMemPnm(const l_uint8* cdata, size_t size);

		l_ok pixWriteMemPnm(l_uint8** pdata, size_t* psize, PIX* pix);
		l_ok pixWriteMemPam(l_uint8** pdata, size_t* psize, PIX* pix);
		PIX* pixProjectiveSampledPta(PIX* pixs, PTA* ptad, PTA* ptas, l_int32 incolor);
		PIX* pixProjectiveSampled(PIX* pixs, l_float32* vc, l_int32 incolor);
		PIX* pixProjectivePta(PIX* pixs, PTA* ptad, PTA* ptas, l_int32 incolor);
		PIX* pixProjective(PIX* pixs, l_float32* vc, l_int32 incolor);
		PIX* pixProjectivePtaColor(PIX* pixs, PTA* ptad, PTA* ptas, l_uint32 colorval);
		PIX* pixProjectiveColor(PIX* pixs, l_float32* vc, l_uint32 colorval);
		PIX* pixProjectivePtaGray(PIX* pixs, PTA* ptad, PTA* ptas, l_uint8 grayval);
		PIX* pixProjectiveGray(PIX* pixs, l_float32* vc, l_uint8 grayval);
		PIX* pixProjectivePtaWithAlpha(PIX* pixs, PTA* ptad, PTA* ptas, PIX* pixg, l_float32 fract, l_int32 border);

		l_ok pixWriteSegmentedPageToPS(PIX* pixs, PIX* pixm, l_float32 textscale, l_float32 imagescale, l_int32 threshold, l_int32 pageno, const char* fileout);
		l_ok pixWriteMixedToPS(PIX* pixb, PIX* pixc, l_float32 scale, l_int32 pageno, const char* fileout);

		l_ok pixaWriteCompressedToPS(PIXA* pixa, const char* fileout, l_int32 res, l_int32 level);
		l_ok pixWriteCompressedToPS(PIX* pix, const char* fileout, l_int32 res, l_int32 level, l_int32* pindex);
		l_ok pixWriteStreamPS(FILE* fp, PIX* pix, BOX* box, l_int32 res, l_float32 scale);
		char* pixWriteStringPS(PIX* pixs, BOX* box, l_int32 res, l_float32 scale);

		l_ok pixWriteMemPS(l_uint8** pdata, size_t* psize, PIX* pix, BOX* box, l_int32 res, l_float32 scale);

		PTA* pixFindCornerPixels(PIX* pixs);

		PTA* ptaCropToMask(PTA* ptas, PIX* pixm);

		l_ok pixPlotAlongPta(PIX* pixs, PTA* pta, l_int32 outformat, const char* title);
		PTA* ptaGetPixelsFromPix(PIX* pixs, BOX* box);
		PIX* pixGenerateFromPta(PTA* pta, l_int32 w, l_int32 h);
		PTA* ptaGetBoundaryPixels(PIX* pixs, l_int32 type);
		PTAA* ptaaGetBoundaryPixels(PIX* pixs, l_int32 type, l_int32 connectivity, BOXA** pboxa, PIXA** ppixa);
		PTAA* ptaaIndexLabeledPixels(PIX* pixs, l_int32* pncc);
		PTA* ptaGetNeighborPixLocs(PIX* pixs, l_int32 x, l_int32 y, l_int32 conn);

		PIX* pixDisplayPta(PIX* pixd, PIX* pixs, PTA* pta);
		PIX* pixDisplayPtaaPattern(PIX* pixd, PIX* pixs, PTAA* ptaa, PIX* pixp, l_int32 cx, l_int32 cy);
		PIX* pixDisplayPtaPattern(PIX* pixd, PIX* pixs, PTA* pta, PIX* pixp, l_int32 cx, l_int32 cy, l_uint32 color);
		PTA* ptaReplicatePattern(PTA* ptas, PIX* pixp, PTA* ptap, l_int32 cx, l_int32 cy, l_int32 w, l_int32 h);
		PIX* pixDisplayPtaa(PIX* pixs, PTAA* ptaa);

		l_ok pixQuadtreeMean(PIX* pixs, l_int32 nlevels, PIX* pix_ma, FPIXA** pfpixa);
		l_ok pixQuadtreeVariance(PIX* pixs, l_int32 nlevels, PIX* pix_ma, DPIX* dpix_msa, FPIXA** pfpixa_v, FPIXA** pfpixa_rv);
		l_ok pixMeanInRectangle(PIX* pixs, BOX* box, PIX* pixma, l_float32* pval);
		l_ok pixVarianceInRectangle(PIX* pixs, BOX* box, PIX* pix_ma, DPIX* dpix_msa, l_float32* pvar, l_float32* prvar);

		l_ok quadtreeGetParent(FPIXA* fpixa, l_int32 level, l_int32 x, l_int32 y, l_float32* pval);
		l_ok quadtreeGetChildren(FPIXA* fpixa, l_int32 level, l_int32 x, l_int32 y, l_float32* pval00, l_float32* pval10, l_float32* pval01, l_float32* pval11);

		PIX* fpixaDisplayQuadtree(FPIXA* fpixa, l_int32 factor, l_int32 fontsize);

		PIX* pixRankFilter(PIX* pixs, l_int32 wf, l_int32 hf, l_float32 rank);
		PIX* pixRankFilterRGB(PIX* pixs, l_int32 wf, l_int32 hf, l_float32 rank);
		PIX* pixRankFilterGray(PIX* pixs, l_int32 wf, l_int32 hf, l_float32 rank);
		PIX* pixMedianFilter(PIX* pixs, l_int32 wf, l_int32 hf);
		PIX* pixRankFilterWithScaling(PIX* pixs, l_int32 wf, l_int32 hf, l_float32 rank, l_float32 scalefactor);

		SARRAY* pixProcessBarcodes(PIX* pixs, l_int32 format, l_int32 method, SARRAY** psaw, l_int32 debugflag);
		PIXA* pixExtractBarcodes(PIX* pixs, l_int32 debugflag);
		SARRAY* pixReadBarcodes(PIXA* pixa, l_int32 format, l_int32 method, SARRAY** psaw, l_int32 debugflag);
		NUMA* pixReadBarcodeWidths(PIX* pixs, l_int32 method, l_int32 debugflag);
		BOXA* pixLocateBarcodes(PIX* pixs, l_int32 thresh, PIX** ppixb, PIX** ppixm);
		PIX* pixDeskewBarcode(PIX* pixs, PIX* pixb, BOX* box, l_int32 margin, l_int32 threshold, l_float32* pangle, l_float32* pconf);
		NUMA* pixExtractBarcodeWidths1(PIX* pixs, l_float32 thresh, l_float32 binfract, NUMA** pnaehist, NUMA** pnaohist, l_int32 debugflag);
		NUMA* pixExtractBarcodeWidths2(PIX* pixs, l_float32 thresh, l_float32* pwidth, NUMA** pnac, l_int32 debugflag);
		NUMA* pixExtractBarcodeCrossings(PIX* pixs, l_float32 thresh, l_int32 debugflag);

		PIXA* pixaReadFiles(const char* dirname, const char* substr);
		PIXA* pixaReadFilesSA(SARRAY* sa);
		PIX* pixRead(const char* filename);
		PIX* pixReadWithHint(const char* filename, l_int32 hint);
		PIX* pixReadIndexed(SARRAY* sa, l_int32 index);
		PIX* pixReadStream(FILE* fp, l_int32 hint);

		PIX* pixReadMem(const l_uint8* data, size_t size);

		L_RECOG* recogCreateFromPixa(PIXA* pixa, l_int32 scalew, l_int32 scaleh, l_int32 linew, l_int32 threshold, l_int32 maxyshift);
		L_RECOG* recogCreateFromPixaNoFinish(PIXA* pixa, l_int32 scalew, l_int32 scaleh, l_int32 linew, l_int32 threshold, l_int32 maxyshift);

		PIXA* recogExtractPixa(L_RECOG* recog);
		BOXA* recogDecode(L_RECOG* recog, PIX* pixs, l_int32 nlevels, PIX** ppixdb);
		l_ok recogCreateDid(L_RECOG* recog, PIX* pixs);

		l_ok recogIdentifyMultiple(L_RECOG* recog, PIX* pixs, l_int32 minh, l_int32 skipsplit, BOXA** pboxa, PIXA** ppixa, PIX** ppixdb, l_int32 debugsplit);
		l_ok recogSplitIntoCharacters(L_RECOG* recog, PIX* pixs, l_int32 minh, l_int32 skipsplit, BOXA** pboxa, PIXA** ppixa, l_int32 debug);
		l_ok recogCorrelationBestRow(L_RECOG* recog, PIX* pixs, BOXA** pboxa, NUMA** pnascore, NUMA** pnaindex, SARRAY** psachar, l_int32 debug);
		l_ok recogCorrelationBestChar(L_RECOG* recog, PIX* pixs, BOX** pbox, l_float32* pscore, l_int32* pindex, char** pcharstr, PIX** ppixdb);
		l_ok recogIdentifyPixa(L_RECOG* recog, PIXA* pixa, PIX** ppixdb);
		l_ok recogIdentifyPix(L_RECOG* recog, PIX* pixs, PIX** ppixdb);

		PIX* recogProcessToIdentify(L_RECOG* recog, PIX* pixs, l_int32 pad);

		PIXA* showExtractNumbers(PIX* pixs, SARRAY* sa, BOXAA* baa, NUMAA* naa, PIX** ppixdb);
		l_ok recogTrainLabeled(L_RECOG* recog, PIX* pixs, BOX* box, char* text, l_int32 debug);
		l_ok recogProcessLabeled(L_RECOG* recog, PIX* pixs, BOX* box, char* text, PIX** ppix);
		l_ok recogAddSample(L_RECOG* recog, PIX* pix, l_int32 debug);
		PIX* recogModifyTemplate(L_RECOG* recog, PIX* pixs);

		l_int32 pixaAccumulateSamples(PIXA* pixa, PTA* pta, PIX** ppixd, l_float32* px, l_float32* py);

		PIXA* recogFilterPixaBySize(PIXA* pixas, l_int32 setsize, l_int32 maxkeep, l_float32 max_ht_ratio, NUMA** pna);
		PIXAA* recogSortPixaByClass(PIXA* pixa, l_int32 setsize);
		l_ok recogRemoveOutliers1(L_RECOG** precog, l_float32 minscore, l_int32 mintarget, l_int32 minsize, PIX** ppixsave, PIX** ppixrem);
		PIXA* pixaRemoveOutliers1(PIXA* pixas, l_float32 minscore, l_int32 mintarget, l_int32 minsize, PIX** ppixsave, PIX** ppixrem);
		l_ok recogRemoveOutliers2(L_RECOG** precog, l_float32 minscore, l_int32 minsize, PIX** ppixsave, PIX** ppixrem);
		PIXA* pixaRemoveOutliers2(PIXA* pixas, l_float32 minscore, l_int32 minsize, PIX** ppixsave, PIX** ppixrem);
		PIXA* recogTrainFromBoot(L_RECOG* recogboot, PIXA* pixas, l_float32 minscore, l_int32 threshold, l_int32 debug);

		PIXA* recogAddDigitPadTemplates(L_RECOG* recog, SARRAY* sa);

		PIXA* recogMakeBootDigitTemplates(l_int32 nsamp, l_int32 debug);

		l_ok recogShowMatchesInRange(L_RECOG* recog, PIXA* pixa, l_float32 minscore, l_float32 maxscore, l_int32 display);
		PIX* recogShowMatch(L_RECOG* recog, PIX* pix1, PIX* pix2, BOX* box, l_int32 index, l_float32 score);

		l_ok regTestComparePix(L_REGPARAMS* rp, PIX* pix1, PIX* pix2);
		l_ok regTestCompareSimilarPix(L_REGPARAMS* rp, PIX* pix1, PIX* pix2, l_int32 mindiff, l_float32 maxfract, l_int32 printstats);

		l_ok regTestWritePixAndCheck(L_REGPARAMS* rp, PIX* pix, l_int32 format);

		l_ok pixRasterop(PIX* pixd, l_int32 dx, l_int32 dy, l_int32 dw, l_int32 dh, l_int32 op, PIX* pixs, l_int32 sx, l_int32 sy);
		l_ok pixRasteropVip(PIX* pixd, l_int32 bx, l_int32 bw, l_int32 vshift, l_int32 incolor);
		l_ok pixRasteropHip(PIX* pixd, l_int32 by, l_int32 bh, l_int32 hshift, l_int32 incolor);
		PIX* pixTranslate(PIX* pixd, PIX* pixs, l_int32 hshift, l_int32 vshift, l_int32 incolor);
		l_ok pixRasteropIP(PIX* pixd, l_int32 hshift, l_int32 vshift, l_int32 incolor);
		l_ok pixRasteropFullImage(PIX* pixd, PIX* pixs, l_int32 op);

		PIX* pixRotate(PIX* pixs, l_float32 angle, l_int32 type, l_int32 incolor, l_int32 width, l_int32 height);
		PIX* pixEmbedForRotation(PIX* pixs, l_float32 angle, l_int32 incolor, l_int32 width, l_int32 height);
		PIX* pixRotateBySampling(PIX* pixs, l_int32 xcen, l_int32 ycen, l_float32 angle, l_int32 incolor);
		PIX* pixRotateBinaryNice(PIX* pixs, l_float32 angle, l_int32 incolor);
		PIX* pixRotateWithAlpha(PIX* pixs, l_float32 angle, PIX* pixg, l_float32 fract);
		PIX* pixRotateAM(PIX* pixs, l_float32 angle, l_int32 incolor);
		PIX* pixRotateAMColor(PIX* pixs, l_float32 angle, l_uint32 colorval);
		PIX* pixRotateAMGray(PIX* pixs, l_float32 angle, l_uint8 grayval);
		PIX* pixRotateAMCorner(PIX* pixs, l_float32 angle, l_int32 incolor);
		PIX* pixRotateAMColorCorner(PIX* pixs, l_float32 angle, l_uint32 fillval);
		PIX* pixRotateAMGrayCorner(PIX* pixs, l_float32 angle, l_uint8 grayval);
		PIX* pixRotateAMColorFast(PIX* pixs, l_float32 angle, l_uint32 colorval);
		PIX* pixRotateOrth(PIX* pixs, l_int32 quads);
		PIX* pixRotate180(PIX* pixd, PIX* pixs);
		PIX* pixRotate90(PIX* pixs, l_int32 direction);
		PIX* pixFlipLR(PIX* pixd, PIX* pixs);
		PIX* pixFlipTB(PIX* pixd, PIX* pixs);
		PIX* pixRotateShear(PIX* pixs, l_int32 xcen, l_int32 ycen, l_float32 angle, l_int32 incolor);
		PIX* pixRotate2Shear(PIX* pixs, l_int32 xcen, l_int32 ycen, l_float32 angle, l_int32 incolor);
		PIX* pixRotate3Shear(PIX* pixs, l_int32 xcen, l_int32 ycen, l_float32 angle, l_int32 incolor);
		l_ok pixRotateShearIP(PIX* pixs, l_int32 xcen, l_int32 ycen, l_float32 angle, l_int32 incolor);
		PIX* pixRotateShearCenter(PIX* pixs, l_float32 angle, l_int32 incolor);
		l_ok pixRotateShearCenterIP(PIX* pixs, l_float32 angle, l_int32 incolor);
		PIX* pixStrokeWidthTransform(PIX* pixs, l_int32 color, l_int32 depth, l_int32 nangles);
		PIX* pixRunlengthTransform(PIX* pixs, l_int32 color, l_int32 direction, l_int32 depth);
		l_ok pixFindHorizontalRuns(PIX* pix, l_int32 y, l_int32* xstart, l_int32* xend, l_int32* pn);
		l_ok pixFindVerticalRuns(PIX* pix, l_int32 x, l_int32* ystart, l_int32* yend, l_int32* pn);
		NUMA* pixFindMaxRuns(PIX* pix, l_int32 direction, NUMA** pnastart);
		l_ok pixFindMaxHorizontalRunOnLine(PIX* pix, l_int32 y, l_int32* pxstart, l_int32* psize);
		l_ok pixFindMaxVerticalRunOnLine(PIX* pix, l_int32 x, l_int32* pystart, l_int32* psize);

		PIX* pixScale(PIX* pixs, l_float32 scalex, l_float32 scaley);
		PIX* pixScaleToSizeRel(PIX* pixs, l_int32 delw, l_int32 delh);
		PIX* pixScaleToSize(PIX* pixs, l_int32 wd, l_int32 hd);
		PIX* pixScaleToResolution(PIX* pixs, l_float32 target, l_float32 assumed, l_float32* pscalefact);
		PIX* pixScaleGeneral(PIX* pixs, l_float32 scalex, l_float32 scaley, l_float32 sharpfract, l_int32 sharpwidth);
		PIX* pixScaleLI(PIX* pixs, l_float32 scalex, l_float32 scaley);
		PIX* pixScaleColorLI(PIX* pixs, l_float32 scalex, l_float32 scaley);
		PIX* pixScaleColor2xLI(PIX* pixs);
		PIX* pixScaleColor4xLI(PIX* pixs);
		PIX* pixScaleGrayLI(PIX* pixs, l_float32 scalex, l_float32 scaley);
		PIX* pixScaleGray2xLI(PIX* pixs);
		PIX* pixScaleGray4xLI(PIX* pixs);
		PIX* pixScaleGray2xLIThresh(PIX* pixs, l_int32 thresh);
		PIX* pixScaleGray2xLIDither(PIX* pixs);
		PIX* pixScaleGray4xLIThresh(PIX* pixs, l_int32 thresh);
		PIX* pixScaleGray4xLIDither(PIX* pixs);
		PIX* pixScaleBySampling(PIX* pixs, l_float32 scalex, l_float32 scaley);
		PIX* pixScaleBySamplingWithShift(PIX* pixs, l_float32 scalex, l_float32 scaley, l_float32 shiftx, l_float32 shifty);
		PIX* pixScaleBySamplingToSize(PIX* pixs, l_int32 wd, l_int32 hd);
		PIX* pixScaleByIntSampling(PIX* pixs, l_int32 factor);
		PIX* pixScaleRGBToGrayFast(PIX* pixs, l_int32 factor, l_int32 color);
		PIX* pixScaleRGBToBinaryFast(PIX* pixs, l_int32 factor, l_int32 thresh);
		PIX* pixScaleGrayToBinaryFast(PIX* pixs, l_int32 factor, l_int32 thresh);
		PIX* pixScaleSmooth(PIX* pix, l_float32 scalex, l_float32 scaley);
		PIX* pixScaleSmoothToSize(PIX* pixs, l_int32 wd, l_int32 hd);
		PIX* pixScaleRGBToGray2(PIX* pixs, l_float32 rwt, l_float32 gwt, l_float32 bwt);
		PIX* pixScaleAreaMap(PIX* pix, l_float32 scalex, l_float32 scaley);
		PIX* pixScaleAreaMap2(PIX* pix);
		PIX* pixScaleAreaMapToSize(PIX* pixs, l_int32 wd, l_int32 hd);
		PIX* pixScaleBinary(PIX* pixs, l_float32 scalex, l_float32 scaley);
		PIX* pixScaleBinaryWithShift(PIX* pixs, l_float32 scalex, l_float32 scaley, l_float32 shiftx, l_float32 shifty);
		PIX* pixScaleToGray(PIX* pixs, l_float32 scalefactor);
		PIX* pixScaleToGrayFast(PIX* pixs, l_float32 scalefactor);
		PIX* pixScaleToGray2(PIX* pixs);
		PIX* pixScaleToGray3(PIX* pixs);
		PIX* pixScaleToGray4(PIX* pixs);
		PIX* pixScaleToGray6(PIX* pixs);
		PIX* pixScaleToGray8(PIX* pixs);
		PIX* pixScaleToGray16(PIX* pixs);
		PIX* pixScaleToGrayMipmap(PIX* pixs, l_float32 scalefactor);
		PIX* pixScaleMipmap(PIX* pixs1, PIX* pixs2, l_float32 scale);
		PIX* pixExpandReplicate(PIX* pixs, l_int32 factor);
		PIX* pixScaleGrayMinMax(PIX* pixs, l_int32 xfact, l_int32 yfact, l_int32 type);
		PIX* pixScaleGrayMinMax2(PIX* pixs, l_int32 type);
		PIX* pixScaleGrayRankCascade(PIX* pixs, l_int32 level1, l_int32 level2, l_int32 level3, l_int32 level4);
		PIX* pixScaleGrayRank2(PIX* pixs, l_int32 rank);
		l_ok pixScaleAndTransferAlpha(PIX* pixd, PIX* pixs, l_float32 scalex, l_float32 scaley);
		PIX* pixScaleWithAlpha(PIX* pixs, l_float32 scalex, l_float32 scaley, PIX* pixg, l_float32 fract);
		PIX* pixSeedfillBinary(PIX* pixd, PIX* pixs, PIX* pixm, l_int32 connectivity);
		PIX* pixSeedfillBinaryRestricted(PIX* pixd, PIX* pixs, PIX* pixm, l_int32 connectivity, l_int32 xmax, l_int32 ymax);
		PIX* pixHolesByFilling(PIX* pixs, l_int32 connectivity);
		PIX* pixFillClosedBorders(PIX* pixs, l_int32 connectivity);
		PIX* pixExtractBorderConnComps(PIX* pixs, l_int32 connectivity);
		PIX* pixRemoveBorderConnComps(PIX* pixs, l_int32 connectivity);
		PIX* pixFillBgFromBorder(PIX* pixs, l_int32 connectivity);
		PIX* pixFillHolesToBoundingRect(PIX* pixs, l_int32 minsize, l_float32 maxhfract, l_float32 minfgfract);
		l_ok pixSeedfillGray(PIX* pixs, PIX* pixm, l_int32 connectivity);
		l_ok pixSeedfillGrayInv(PIX* pixs, PIX* pixm, l_int32 connectivity);
		l_ok pixSeedfillGraySimple(PIX* pixs, PIX* pixm, l_int32 connectivity);
		l_ok pixSeedfillGrayInvSimple(PIX* pixs, PIX* pixm, l_int32 connectivity);
		PIX* pixSeedfillGrayBasin(PIX* pixb, PIX* pixm, l_int32 delta, l_int32 connectivity);
		PIX* pixDistanceFunction(PIX* pixs, l_int32 connectivity, l_int32 outdepth, l_int32 boundcond);
		PIX* pixSeedspread(PIX* pixs, l_int32 connectivity);
		l_ok pixLocalExtrema(PIX* pixs, l_int32 maxmin, l_int32 minmax, PIX** ppixmin, PIX** ppixmax);
		l_ok pixSelectedLocalExtrema(PIX* pixs, l_int32 mindist, PIX** ppixmin, PIX** ppixmax);
		PIX* pixFindEqualValues(PIX* pixs1, PIX* pixs2);
		l_ok pixSelectMinInConnComp(PIX* pixs, PIX* pixm, PTA** ppta, NUMA** pnav);
		PIX* pixRemoveSeededComponents(PIX* pixd, PIX* pixs, PIX* pixm, l_int32 connectivity, l_int32 bordersize);

		SEL* selCreateFromPix(PIX* pix, l_int32 cy, l_int32 cx, const char* name);

		SEL* selCreateFromColorPix(PIX* pixs, const char* selname);
		SELA* selaCreateFromColorPixa(PIXA* pixa, SARRAY* sa);
		PIX* selDisplayInPix(SEL* sel, l_int32 size, l_int32 gthick);
		PIX* selaDisplayInPix(SELA* sela, l_int32 size, l_int32 gthick, l_int32 spacing, l_int32 ncols);

		SEL* pixGenerateSelWithRuns(PIX* pixs, l_int32 nhlines, l_int32 nvlines, l_int32 distance, l_int32 minlength, l_int32 toppix, l_int32 botpix, l_int32 leftpix, l_int32 rightpix, PIX** ppixe);
		SEL* pixGenerateSelRandom(PIX* pixs, l_float32 hitfract, l_float32 missfract, l_int32 distance, l_int32 toppix, l_int32 botpix, l_int32 leftpix, l_int32 rightpix, PIX** ppixe);
		SEL* pixGenerateSelBoundary(PIX* pixs, l_int32 hitdist, l_int32 missdist, l_int32 hitskip, l_int32 missskip, l_int32 topflag, l_int32 botflag, l_int32 leftflag, l_int32 rightflag, PIX** ppixe);
		NUMA* pixGetRunCentersOnLine(PIX* pixs, l_int32 x, l_int32 y, l_int32 minlength);
		NUMA* pixGetRunsOnLine(PIX* pixs, l_int32 x1, l_int32 y1, l_int32 x2, l_int32 y2);
		PTA* pixSubsampleBoundaryPixels(PIX* pixs, l_int32 skip);
		l_int32 adjacentOnPixelInRaster(PIX* pixs, l_int32 x, l_int32 y, l_int32* pxa, l_int32* pya);
		PIX* pixDisplayHitMissSel(PIX* pixs, SEL* sel, l_int32 scalefactor, l_uint32 hitcolor, l_uint32 misscolor);
		PIX* pixHShear(PIX* pixd, PIX* pixs, l_int32 yloc, l_float32 radang, l_int32 incolor);
		PIX* pixVShear(PIX* pixd, PIX* pixs, l_int32 xloc, l_float32 radang, l_int32 incolor);
		PIX* pixHShearCorner(PIX* pixd, PIX* pixs, l_float32 radang, l_int32 incolor);
		PIX* pixVShearCorner(PIX* pixd, PIX* pixs, l_float32 radang, l_int32 incolor);
		PIX* pixHShearCenter(PIX* pixd, PIX* pixs, l_float32 radang, l_int32 incolor);
		PIX* pixVShearCenter(PIX* pixd, PIX* pixs, l_float32 radang, l_int32 incolor);
		l_ok pixHShearIP(PIX* pixs, l_int32 yloc, l_float32 radang, l_int32 incolor);
		l_ok pixVShearIP(PIX* pixs, l_int32 xloc, l_float32 radang, l_int32 incolor);
		PIX* pixHShearLI(PIX* pixs, l_int32 yloc, l_float32 radang, l_int32 incolor);
		PIX* pixVShearLI(PIX* pixs, l_int32 xloc, l_float32 radang, l_int32 incolor);
		PIX* pixDeskewBoth(PIX* pixs, l_int32 redsearch);
		PIX* pixDeskew(PIX* pixs, l_int32 redsearch);
		PIX* pixFindSkewAndDeskew(PIX* pixs, l_int32 redsearch, l_float32* pangle, l_float32* pconf);
		PIX* pixDeskewGeneral(PIX* pixs, l_int32 redsweep, l_float32 sweeprange, l_float32 sweepdelta, l_int32 redsearch, l_int32 thresh, l_float32* pangle, l_float32* pconf);
		l_ok pixFindSkew(PIX* pixs, l_float32* pangle, l_float32* pconf);
		l_ok pixFindSkewSweep(PIX* pixs, l_float32* pangle, l_int32 reduction, l_float32 sweeprange, l_float32 sweepdelta);
		l_ok pixFindSkewSweepAndSearch(PIX* pixs, l_float32* pangle, l_float32* pconf, l_int32 redsweep, l_int32 redsearch, l_float32 sweeprange, l_float32 sweepdelta, l_float32 minbsdelta);
		l_ok pixFindSkewSweepAndSearchScore(PIX* pixs, l_float32* pangle, l_float32* pconf, l_float32* pendscore, l_int32 redsweep, l_int32 redsearch, l_float32 sweepcenter, l_float32 sweeprange, l_float32 sweepdelta, l_float32 minbsdelta);
		l_ok pixFindSkewSweepAndSearchScorePivot(PIX* pixs, l_float32* pangle, l_float32* pconf, l_float32* pendscore, l_int32 redsweep, l_int32 redsearch, l_float32 sweepcenter, l_float32 sweeprange, l_float32 sweepdelta, l_float32 minbsdelta, l_int32 pivot);
		l_int32 pixFindSkewOrthogonalRange(PIX* pixs, l_float32* pangle, l_float32* pconf, l_int32 redsweep, l_int32 redsearch, l_float32 sweeprange, l_float32 sweepdelta, l_float32 minbsdelta, l_float32 confprior);
		l_ok pixFindDifferentialSquareSum(PIX* pixs, l_float32* psum);
		l_ok pixFindNormalizedSquareSum(PIX* pixs, l_float32* phratio, l_float32* pvratio, l_float32* pfract);
		PIX* pixReadStreamSpix(FILE* fp);

		l_ok pixWriteStreamSpix(FILE* fp, PIX* pix);
		PIX* pixReadMemSpix(const l_uint8* data, size_t size);
		l_ok pixWriteMemSpix(l_uint8** pdata, size_t* psize, PIX* pix);
		l_ok pixSerializeToMemory(PIX* pixs, l_uint32** pdata, size_t* pnbytes);
		PIX* pixDeserializeFromMemory(const l_uint32* data, size_t nbytes);

		l_ok pixFindStrokeLength(PIX* pixs, l_int32* tab8, l_int32* plength);
		l_ok pixFindStrokeWidth(PIX* pixs, l_float32 thresh, l_int32* tab8, l_float32* pwidth, NUMA** pnahisto);
		NUMA* pixaFindStrokeWidth(PIXA* pixa, l_float32 thresh, l_int32* tab8, l_int32 debug);
		PIXA* pixaModifyStrokeWidth(PIXA* pixas, l_float32 targetw);
		PIX* pixModifyStrokeWidth(PIX* pixs, l_float32 width, l_float32 targetw);
		PIXA* pixaSetStrokeWidth(PIXA* pixas, l_int32 width, l_int32 thinfirst, l_int32 connectivity);
		PIX* pixSetStrokeWidth(PIX* pixs, l_int32 width, l_int32 thinfirst, l_int32 connectivity);

		PIX* pixAddSingleTextblock(PIX* pixs, L_BMF* bmf, const char* textstr, l_uint32 val, l_int32 location, l_int32* poverflow);
		PIX* pixAddTextlines(PIX* pixs, L_BMF* bmf, const char* textstr, l_uint32 val, l_int32 location);
		l_ok pixSetTextblock(PIX* pixs, L_BMF* bmf, const char* textstr, l_uint32 val, l_int32 x0, l_int32 y0, l_int32 wtext, l_int32 firstindent, l_int32* poverflow);
		l_ok pixSetTextline(PIX* pixs, L_BMF* bmf, const char* textstr, l_uint32 val, l_int32 x0, l_int32 y0, l_int32* pwidth, l_int32* poverflow);
		PIXA* pixaAddTextNumber(PIXA* pixas, L_BMF* bmf, NUMA* na, l_uint32 val, l_int32 location);
		PIXA* pixaAddTextlines(PIXA* pixas, L_BMF* bmf, SARRAY* sa, l_uint32 val, l_int32 location);
		l_ok pixaAddPixWithText(PIXA* pixa, PIX* pixs, l_int32 reduction, L_BMF* bmf, const char* textstr, l_uint32 val, l_int32 location);

		PIX* pixReadTiff(const char* filename, l_int32 n);
		PIX* pixReadStreamTiff(FILE* fp, l_int32 n);
		l_ok pixWriteTiff(const char* filename, PIX* pix, l_int32 comptype, const char* modestring);
		l_ok pixWriteTiffCustom(const char* filename, PIX* pix, l_int32 comptype, const char* modestring, NUMA* natags, SARRAY* savals, SARRAY* satypes, NUMA* nasizes);
		l_ok pixWriteStreamTiff(FILE* fp, PIX* pix, l_int32 comptype);
		l_ok pixWriteStreamTiffWA(FILE* fp, PIX* pix, l_int32 comptype, const char* modestr);
		PIX* pixReadFromMultipageTiff(const char* filename, size_t* poffset);
		PIXA* pixaReadMultipageTiff(const char* filename);
		l_ok pixaWriteMultipageTiff(const char* filename, PIXA* pixa);

		PIX* pixReadMemTiff(const l_uint8* cdata, size_t size, l_int32 n);
		PIX* pixReadMemFromMultipageTiff(const l_uint8* cdata, size_t size, size_t* poffset);
		PIXA* pixaReadMemMultipageTiff(const l_uint8* data, size_t size);
		l_ok pixaWriteMemMultipageTiff(l_uint8** pdata, size_t* psize, PIXA* pixa);
		l_ok pixWriteMemTiff(l_uint8** pdata, size_t* psize, PIX* pix, l_int32 comptype);
		l_ok pixWriteMemTiffCustom(l_uint8** pdata, size_t* psize, PIX* pix, l_int32 comptype, NUMA* natags, SARRAY* savals, SARRAY* satypes, NUMA* nasizes);

		PIX* pixSimpleCaptcha(PIX* pixs, l_int32 border, l_int32 nterms, l_uint32 seed, l_uint32 color, l_int32 cmapflag);
		PIX* pixRandomHarmonicWarp(PIX* pixs, l_float32 xmag, l_float32 ymag, l_float32 xfreq, l_float32 yfreq, l_int32 nx, l_int32 ny, l_uint32 seed, l_int32 grayval);
		PIX* pixWarpStereoscopic(PIX* pixs, l_int32 zbend, l_int32 zshiftt, l_int32 zshiftb, l_int32 ybendt, l_int32 ybendb, l_int32 redleft);
		PIX* pixStretchHorizontal(PIX* pixs, l_int32 dir, l_int32 type, l_int32 hmax, l_int32 operation, l_int32 incolor);
		PIX* pixStretchHorizontalSampled(PIX* pixs, l_int32 dir, l_int32 type, l_int32 hmax, l_int32 incolor);
		PIX* pixStretchHorizontalLI(PIX* pixs, l_int32 dir, l_int32 type, l_int32 hmax, l_int32 incolor);
		PIX* pixQuadraticVShear(PIX* pixs, l_int32 dir, l_int32 vmaxt, l_int32 vmaxb, l_int32 operation, l_int32 incolor);
		PIX* pixQuadraticVShearSampled(PIX* pixs, l_int32 dir, l_int32 vmaxt, l_int32 vmaxb, l_int32 incolor);
		PIX* pixQuadraticVShearLI(PIX* pixs, l_int32 dir, l_int32 vmaxt, l_int32 vmaxb, l_int32 incolor);
		PIX* pixStereoFromPair(PIX* pix1, PIX* pix2, l_float32 rwt, l_float32 gwt, l_float32 bwt);
		L_WSHED* wshedCreate(PIX* pixs, PIX* pixm, l_int32 mindepth, l_int32 debugflag);

		l_ok wshedBasins(L_WSHED* wshed, PIXA** ppixa, NUMA** pnalevels);
		PIX* wshedRenderFill(L_WSHED* wshed);
		PIX* wshedRenderColors(L_WSHED* wshed);
		l_ok pixaWriteWebPAnim(const char* filename, PIXA* pixa, l_int32 loopcount, l_int32 duration, l_int32 quality, l_int32 lossless);
		l_ok pixaWriteStreamWebPAnim(FILE* fp, PIXA* pixa, l_int32 loopcount, l_int32 duration, l_int32 quality, l_int32 lossless);
		l_ok pixaWriteMemWebPAnim(l_uint8** pencdata, size_t* pencsize, PIXA* pixa, l_int32 loopcount, l_int32 duration, l_int32 quality, l_int32 lossless);
		PIX* pixReadStreamWebP(FILE* fp);
		PIX* pixReadMemWebP(const l_uint8* filedata, size_t filesize);

		l_ok pixWriteWebP(const char* filename, PIX* pixs, l_int32 quality, l_int32 lossless);
		l_ok pixWriteStreamWebP(FILE* fp, PIX* pixs, l_int32 quality, l_int32 lossless);
		l_ok pixWriteMemWebP(l_uint8** pencdata, size_t* pencsize, PIX* pixs, l_int32 quality, l_int32 lossless);

		l_ok pixaWriteFiles(const char* rootname, PIXA* pixa, l_int32 format);
		l_ok pixWriteDebug(const char* fname, PIX* pix, l_int32 format);
		l_ok pixWrite(const char* fname, PIX* pix, l_int32 format);
		l_ok pixWriteAutoFormat(const char* filename, PIX* pix);
		l_ok pixWriteStream(FILE* fp, PIX* pix, l_int32 format);
		l_ok pixWriteImpliedFormat(const char* filename, PIX* pix, l_int32 quality, l_int32 progressive);
		l_int32 pixChooseOutputFormat(PIX* pix);

		l_ok pixGetAutoFormat(PIX* pix, l_int32* pformat);

		l_ok pixWriteMem(l_uint8** pdata, size_t* psize, PIX* pix, l_int32 format);

		l_ok pixDisplay(PIX* pixs, l_int32 x, l_int32 y);
		l_ok pixDisplayWithTitle(PIX* pixs, l_int32 x, l_int32 y, const char* title, l_int32 dispflag);
		PIX* pixMakeColorSquare(l_uint32 color, l_int32 size, l_int32 addlabel, l_int32 location, l_uint32 textcolor);

		l_ok pixDisplayWrite(PIX* pixs, l_int32 reduction);

		------------------------------------------------------------------------------------------------------------

		PIXA * pixaReadMultipageMemGif(const l_uint8* cdata, size_t size);
		PIXA * pixReadMultipageStreamGif(FILE* fp);

=======================================================================================================================================

		BOX*            pixFindPageForeground
		BOX*            pixFindRectangleInCC
		BOX*            pixSeedfill4BB
		BOX*            pixSeedfill8BB
		BOX*            pixSeedfillBB
		BOX*            pixSelectLargeULComp
		BOX*            pixaGetBox
		BOX*            pixacompGetBox
		BOXA*           boxaCombineOverlaps
		BOXA*           boxaReconcileAllByMedian
		BOXA*           boxaReconcileSidesByMedian
		BOXA*           pixConnComp
		BOXA*           pixConnCompBB
		BOXA*           pixConnCompPixa
		BOXA*           pixFindRectangleComps
		BOXA*           pixLocateBarcodes
		BOXA*           pixSplitComponentIntoBoxa
		BOXA*           pixSplitComponentWithProfile
		BOXA*           pixSplitIntoBoxa
		BOXA*           pixaGetBoxa
		BOXA*           pixaaGetBoxa
		BOXA*           pixacompGetBoxa
		BOXA*           recogDecode
		CCBORDA*        pixGetAllCCBorders
		DPIX*           dpixClone
		DPIX*           dpixCopy
		DPIX*           dpixCreate
		DPIX*           dpixCreateTemplate
		DPIX*           dpixEndianByteSwap
		DPIX*           dpixLinearCombination
		DPIX*           dpixRead
		DPIX*           dpixReadMem
		DPIX*           dpixReadStream
		DPIX*           dpixScaleByInteger
		DPIX*           fpixConvertToDPix
		DPIX*           pixConvertToDPix
		DPIX*           pixMeanSquareAccum
		FPIX*           dpixConvertToFPix
		FPIX*           fpixAddBorder
		FPIX*           fpixAddContinuedBorder
		FPIX*           fpixAddMirroredBorder
		FPIX*           fpixAddSlopeBorder
		FPIX*           fpixAffine
		FPIX*           fpixAffinePta
		FPIX*           fpixClone
		FPIX*           fpixConvolve
		FPIX*           fpixConvolveSep
		FPIX*           fpixCopy
		FPIX*           fpixCreate
		FPIX*           fpixCreateTemplate
		FPIX*           fpixEndianByteSwap
		FPIX*           fpixFlipLR
		FPIX*           fpixFlipTB
		FPIX*           fpixLinearCombination
		FPIX*           fpixProjective
		FPIX*           fpixProjectivePta
		FPIX*           fpixRead
		FPIX*           fpixReadMem
		FPIX*           fpixReadStream
		FPIX*           fpixRemoveBorder
		FPIX*           fpixRotate180
		FPIX*           fpixRotate90
		FPIX*           fpixRotateOrth
		FPIX*           fpixScaleByInteger
		FPIX*           fpixaGetFPix
		FPIX*           pixComponentFunction
		FPIX*           pixConvertToFPix
		FPIXA*          fpixaConvertLABToXYZ
		FPIXA*          fpixaConvertXYZToLAB
		FPIXA*          fpixaCopy
		FPIXA*          fpixaCreate
		FPIXA*          pixConvertRGBToLAB
		FPIXA*          pixConvertRGBToXYZ
		L_AMAP*         pixGetColorAmapHistogram
		L_BMF*          bmfCreate
		L_COLORFILL*    l_colorfillCreate
		L_COMP_DATA*    l_generateFlateDataPdf
		L_DEWARP*       dewarpCreate
		L_DEWARPA*      dewarpaCreateFromPixacomp
		L_DNA*          pixConvertDataToDna
		L_KERNEL*       kernelCreateFromPix
		L_RECOG*        recogCreateFromPixa
		L_RECOG*        recogCreateFromPixaNoFinish
		L_WSHED*        wshedCreate
		NUMA*           numaEqualizeTRC
		NUMA*           pixAbsDiffByColumn
		NUMA*           pixAbsDiffByRow
		NUMA*           pixAverageByColumn
		NUMA*           pixAverageByRow
		NUMA*           pixAverageIntensityProfile
		NUMA*           pixCompareRankDifference
		NUMA*           pixCountByColumn
		NUMA*           pixCountByRow
		NUMA*           pixCountPixelsByColumn
		NUMA*           pixCountPixelsByRow
		NUMA*           pixExtractBarcodeCrossings
		NUMA*           pixExtractBarcodeWidths1
		NUMA*           pixExtractBarcodeWidths2
		NUMA*           pixExtractOnLine
		NUMA*           pixFindBaselines
		NUMA*           pixFindMaxRuns
		NUMA*           pixGetCmapHistogram
		NUMA*           pixGetCmapHistogramInRect
		NUMA*           pixGetCmapHistogramMasked
		NUMA*           pixGetDifferenceHistogram
		NUMA*           pixGetEdgeProfile
		NUMA*           pixGetGrayHistogram
		NUMA*           pixGetGrayHistogramInRect
		NUMA*           pixGetGrayHistogramMasked
		NUMA*           pixGetLocalSkewAngles
		NUMA*           pixGetMomentByColumn
		NUMA*           pixGetRGBHistogram
		NUMA*           pixGetRunCentersOnLine
		NUMA*           pixGetRunsOnLine
		NUMA*           pixOctcubeHistogram
		NUMA*           pixReadBarcodeWidths
		NUMA*           pixReversalProfile
		NUMA*           pixRunHistogramMorph
		NUMA*           pixVarianceByColumn
		NUMA*           pixVarianceByRow
		NUMA*           pixaCountPixels
		NUMA*           pixaFindAreaFraction
		NUMA*           pixaFindAreaFractionMasked
		NUMA*           pixaFindPerimSizeRatio
		NUMA*           pixaFindPerimToAreaRatio
		NUMA*           pixaFindStrokeWidth
		NUMA*           pixaFindWidthHeightProduct
		NUMA*           pixaFindWidthHeightRatio
		NUMA*           pixaMakeSizeIndicator
		NUMAA*          pixGetGrayHistogramTiled
		PIX*            bmfGetPix
		PIX*            boxaDisplayTiled
		PIX*            boxaaDisplay
		PIX*            ccbaDisplayBorder
		PIX*            ccbaDisplayImage1
		PIX*            ccbaDisplayImage2
		PIX*            ccbaDisplaySPBorder
		PIX*            displayHSVColorRange
		PIX*            dpixConvertToPix
		PIX*            fpixAutoRenderContours
		PIX*            fpixConvertToPix
		PIX*            fpixDisplayMaxDynamicRange
		PIX*            fpixRenderContours
		PIX*            fpixThresholdToPix
		PIX*            fpixaConvertLABToRGB
		PIX*            fpixaConvertXYZToRGB
		PIX*            fpixaDisplayQuadtree
		PIX*            generateBinaryMaze
		PIX*            kernelDisplayInPix
		PIX*            pixAbsDifference
		PIX*            pixAdaptThresholdToBinary
		PIX*            pixAdaptThresholdToBinaryGen
		PIX*            pixAddAlphaTo1bpp
		PIX*            pixAddAlphaToBlend
		PIX*            pixAddBlackOrWhiteBorder
		PIX*            pixAddBorder
		PIX*            pixAddBorderGeneral
		PIX*            pixAddContinuedBorder
		PIX*            pixAddGaussianNoise
		PIX*            pixAddGray
		PIX*            pixAddMinimalGrayColormap8
		PIX*            pixAddMirroredBorder
		PIX*            pixAddMixedBorder
		PIX*            pixAddMultipleBlackWhiteBorders
		PIX*            pixAddRGB
		PIX*            pixAddRepeatedBorder
		PIX*            pixAddSingleTextblock
		PIX*            pixAddTextlines
		PIX*            pixAffine
		PIX*            pixAffineColor
		PIX*            pixAffineGray
		PIX*            pixAffinePta
		PIX*            pixAffinePtaColor
		PIX*            pixAffinePtaGray
		PIX*            pixAffinePtaWithAlpha
		PIX*            pixAffineSampled
		PIX*            pixAffineSampledPta
		PIX*            pixAffineSequential
		PIX*            pixAlphaBlendUniform
		PIX*            pixAnd
		PIX*            pixApplyInvBackgroundGrayMap
		PIX*            pixApplyInvBackgroundRGBMap
		PIX*            pixApplyVariableGrayMap
		PIX*            pixAutoPhotoinvert
		PIX*            pixBackgroundNorm
		PIX*            pixBackgroundNormFlex
		PIX*            pixBackgroundNormMorph
		PIX*            pixBackgroundNormSimple
		PIX*            pixBackgroundNormTo1MinMax
		PIX*            pixBilateral
		PIX*            pixBilateralExact
		PIX*            pixBilateralGray
		PIX*            pixBilateralGrayExact
		PIX*            pixBilinear
		PIX*            pixBilinearColor
		PIX*            pixBilinearGray
		PIX*            pixBilinearPta
		PIX*            pixBilinearPtaColor
		PIX*            pixBilinearPtaGray
		PIX*            pixBilinearPtaWithAlpha
		PIX*            pixBilinearSampled
		PIX*            pixBilinearSampledPta
		PIX*            pixBlend
		PIX*            pixBlendBackgroundToColor
		PIX*            pixBlendBoxaRandom
		PIX*            pixBlendColor
		PIX*            pixBlendColorByChannel
		PIX*            pixBlendGray
		PIX*            pixBlendGrayAdapt
		PIX*            pixBlendGrayInverse
		PIX*            pixBlendHardLight
		PIX*            pixBlendMask
		PIX*            pixBlendWithGrayMask
		PIX*            pixBlockBilateralExact
		PIX*            pixBlockconv
		PIX*            pixBlockconvAccum
		PIX*            pixBlockconvGray
		PIX*            pixBlockconvGrayTile
		PIX*            pixBlockconvGrayUnnormalized
		PIX*            pixBlockconvTiled
		PIX*            pixBlockrank
		PIX*            pixBlocksum
		PIX*            pixCensusTransform
		PIX*            pixCleanBackgroundToWhite
		PIX*            pixCleanImage
		PIX*            pixClipMasked
		PIX*            pixClipRectangle
		PIX*            pixClipRectangleWithBorder
		PIX*            pixClone
		PIX*            pixClose
		PIX*            pixCloseBrick
		PIX*            pixCloseBrickDwa
		PIX*            pixCloseCompBrick
		PIX*            pixCloseCompBrickDwa
		PIX*            pixCloseCompBrickExtendDwa
		PIX*            pixCloseGeneralized
		PIX*            pixCloseGray
		PIX*            pixCloseGray3
		PIX*            pixCloseSafe
		PIX*            pixCloseSafeBrick
		PIX*            pixCloseSafeCompBrick
		PIX*            pixColorFill
		PIX*            pixColorGrayMasked
		PIX*            pixColorGrayRegions
		PIX*            pixColorMagnitude
		PIX*            pixColorMorph
		PIX*            pixColorMorphSequence
		PIX*            pixColorSegment
		PIX*            pixColorSegmentCluster
		PIX*            pixColorShiftRGB
		PIX*            pixColorShiftWhitePoint
		PIX*            pixColorizeGray
		PIX*            pixConnCompAreaTransform
		PIX*            pixConnCompTransform
		PIX*            pixContrastNorm
		PIX*            pixContrastTRC
		PIX*            pixContrastTRCMasked
		PIX*            pixConvert16To8
		PIX*            pixConvert1To16
		PIX*            pixConvert1To2
		PIX*            pixConvert1To2Cmap
		PIX*            pixConvert1To32
		PIX*            pixConvert1To4
		PIX*            pixConvert1To4Cmap
		PIX*            pixConvert1To8
		PIX*            pixConvert1To8Cmap
		PIX*            pixConvert24To32
		PIX*            pixConvert2To8
		PIX*            pixConvert32To16
		PIX*            pixConvert32To24
		PIX*            pixConvert32To8
		PIX*            pixConvert4To8
		PIX*            pixConvert8To16
		PIX*            pixConvert8To2
		PIX*            pixConvert8To32
		PIX*            pixConvert8To4
		PIX*            pixConvertCmapTo1
		PIX*            pixConvertColorToSubpixelRGB
		PIX*            pixConvertForPSWrap
		PIX*            pixConvertGrayToColormap
		PIX*            pixConvertGrayToColormap8
		PIX*            pixConvertGrayToFalseColor
		PIX*            pixConvertGrayToSubpixelRGB
		PIX*            pixConvertHSVToRGB
		PIX*            pixConvertLossless
		PIX*            pixConvertRGBToBinaryArb
		PIX*            pixConvertRGBToCmapLossless
		PIX*            pixConvertRGBToColormap
		PIX*            pixConvertRGBToGray
		PIX*            pixConvertRGBToGrayArb
		PIX*            pixConvertRGBToGrayFast
		PIX*            pixConvertRGBToGrayGeneral
		PIX*            pixConvertRGBToGrayMinMax
		PIX*            pixConvertRGBToGraySatBoost
		PIX*            pixConvertRGBToHSV
		PIX*            pixConvertRGBToHue
		PIX*            pixConvertRGBToLuminance
		PIX*            pixConvertRGBToSaturation
		PIX*            pixConvertRGBToValue
		PIX*            pixConvertRGBToYUV
		PIX*            pixConvertTo1
		PIX*            pixConvertTo16
		PIX*            pixConvertTo1Adaptive
		PIX*            pixConvertTo1BySampling
		PIX*            pixConvertTo2
		PIX*            pixConvertTo32
		PIX*            pixConvertTo32BySampling
		PIX*            pixConvertTo4
		PIX*            pixConvertTo8
		PIX*            pixConvertTo8BySampling
		PIX*            pixConvertTo8Colormap
		PIX*            pixConvertTo8MinMax
		PIX*            pixConvertTo8Or32
		PIX*            pixConvertToSubpixelRGB
		PIX*            pixConvertYUVToRGB
		PIX*            pixConvolve
		PIX*            pixConvolveRGB
		PIX*            pixConvolveRGBSep
		PIX*            pixConvolveSep
		PIX*            pixConvolveWithBias
		PIX*            pixCopy
		PIX*            pixCopyBorder
		PIX*            pixCopyWithBoxa
		PIX*            pixCreate
		PIX*            pixCreateFromPixcomp
		PIX*            pixCreateHeader
		PIX*            pixCreateNoInit
		PIX*            pixCreateRGBImage
		PIX*            pixCreateTemplate
		PIX*            pixCreateTemplateNoInit
		PIX*            pixCreateWithCmap
		PIX*            pixCropImage
		PIX*            pixCropToSize
		PIX*            pixDarkenGray
		PIX*            pixDeserializeFromMemory
		PIX*            pixDeskew
		PIX*            pixDeskewBarcode
		PIX*            pixDeskewBoth
		PIX*            pixDeskewGeneral
		PIX*            pixDeskewLocal
		PIX*            pixDilate
		PIX*            pixDilateBrick
		PIX*            pixDilateBrickDwa
		PIX*            pixDilateCompBrick
		PIX*            pixDilateCompBrickDwa
		PIX*            pixDilateCompBrickExtendDwa
		PIX*            pixDilateGray
		PIX*            pixDilateGray3
		PIX*            pixDisplayColorArray
		PIX*            pixDisplayDiff
		PIX*            pixDisplayDiffBinary
		PIX*            pixDisplayHitMissSel
		PIX*            pixDisplayLayersRGBA
		PIX*            pixDisplayMatchedPattern
		PIX*            pixDisplayPta
		PIX*            pixDisplayPtaPattern
		PIX*            pixDisplayPtaa
		PIX*            pixDisplayPtaaPattern
		PIX*            pixDisplaySelectedPixels
		PIX*            pixDistanceFunction
		PIX*            pixDitherTo2bpp
		PIX*            pixDitherTo2bppSpec
		PIX*            pixDitherToBinary
		PIX*            pixDitherToBinarySpec
		PIX*            pixDrawBoxa
		PIX*            pixDrawBoxaRandom
		PIX*            pixEmbedForRotation
		PIX*            pixEndianByteSwapNew
		PIX*            pixEndianTwoByteSwapNew
		PIX*            pixEqualizeTRC
		PIX*            pixErode
		PIX*            pixErodeBrick
		PIX*            pixErodeBrickDwa
		PIX*            pixErodeCompBrick
		PIX*            pixErodeCompBrickDwa
		PIX*            pixErodeCompBrickExtendDwa
		PIX*            pixErodeGray
		PIX*            pixErodeGray3
		PIX*            pixExpandBinaryPower2
		PIX*            pixExpandBinaryReplicate
		PIX*            pixExpandReplicate
		PIX*            pixExtendByReplication
		PIX*            pixExtractBorderConnComps
		PIX*            pixExtractBoundary
		PIX*            pixFHMTGen_1
		PIX*            pixFMorphopGen_1
		PIX*            pixFMorphopGen_2
		PIX*            pixFadeWithGray
		PIX*            pixFastTophat
		PIX*            pixFewColorsMedianCutQuantMixed
		PIX*            pixFewColorsOctcubeQuant1
		PIX*            pixFewColorsOctcubeQuant2
		PIX*            pixFewColorsOctcubeQuantMixed
		PIX*            pixFillBgFromBorder
		PIX*            pixFillClosedBorders
		PIX*            pixFillHolesToBoundingRect
		PIX*            pixFillPolygon
		PIX*            pixFilterComponentBySize
		PIX*            pixFinalAccumulate
		PIX*            pixFinalAccumulateThreshold
		PIX*            pixFindEqualValues
		PIX*            pixFindSkewAndDeskew
		PIX*            pixFixedOctcubeQuant256
		PIX*            pixFixedOctcubeQuantGenRGB
		PIX*            pixFlipLR
		PIX*            pixFlipTB
		PIX*            pixGammaTRC
		PIX*            pixGammaTRCMasked
		PIX*            pixGammaTRCWithAlpha
		PIX*            pixGenHalftoneMask
		PIX*            pixGenTextblockMask
		PIX*            pixGenTextlineMask
		PIX*            pixGenerateFromPta
		PIX*            pixGenerateHalftoneMask
		PIX*            pixGenerateMaskByBand
		PIX*            pixGenerateMaskByBand32
		PIX*            pixGenerateMaskByDiscr32
		PIX*            pixGenerateMaskByValue
		PIX*            pixGetAverageTiled
		PIX*            pixGetInvBackgroundMap
		PIX*            pixGetRGBComponent
		PIX*            pixGetRGBComponentCmap
		PIX*            pixGlobalNormNoSatRGB
		PIX*            pixGlobalNormRGB
		PIX*            pixGrayMorphSequence
		PIX*            pixGrayQuantFromCmap
		PIX*            pixGrayQuantFromHisto
		PIX*            pixHDome
		PIX*            pixHMT
		PIX*            pixHMTDwa_1
		PIX*            pixHShear
		PIX*            pixHShearCenter
		PIX*            pixHShearCorner
		PIX*            pixHShearLI
		PIX*            pixHalfEdgeByBandpass
		PIX*            pixHolesByFilling
		PIX*            pixInitAccumulate
		PIX*            pixIntersectionOfMorphOps
		PIX*            pixInvert
		PIX*            pixLinearMapToTargetColor
		PIX*            pixLocToColorTransform
		PIX*            pixMakeAlphaFromMask
		PIX*            pixMakeArbMaskFromRGB
		PIX*            pixMakeColorSquare
		PIX*            pixMakeCoveringOfRectangles
		PIX*            pixMakeFrameMask
		PIX*            pixMakeGamutRGB
		PIX*            pixMakeHistoHS
		PIX*            pixMakeHistoHV
		PIX*            pixMakeHistoSV
		PIX*            pixMakeMaskFromLUT
		PIX*            pixMakeMaskFromVal
		PIX*            pixMakeRangeMaskHS
		PIX*            pixMakeRangeMaskHV
		PIX*            pixMakeRangeMaskSV
		PIX*            pixMakeSymmetricMask
		PIX*            pixMapWithInvariantHue
		PIX*            pixMaskBoxa
		PIX*            pixMaskConnComp
		PIX*            pixMaskOverColorPixels
		PIX*            pixMaskOverColorRange
		PIX*            pixMaskOverGrayPixels
		PIX*            pixMaskedThreshOnBackgroundNorm
		PIX*            pixMaxDynamicRange
		PIX*            pixMaxDynamicRangeRGB
		PIX*            pixMedianCutQuant
		PIX*            pixMedianCutQuantGeneral
		PIX*            pixMedianCutQuantMixed
		PIX*            pixMedianFilter
		PIX*            pixMinOrMax
		PIX*            pixMirroredTiling
		PIX*            pixModifyBrightness
		PIX*            pixModifyHue
		PIX*            pixModifySaturation
		PIX*            pixModifyStrokeWidth
		PIX*            pixMorphCompSequence
		PIX*            pixMorphCompSequenceDwa
		PIX*            pixMorphDwa_1
		PIX*            pixMorphDwa_2
		PIX*            pixMorphGradient
		PIX*            pixMorphSequence
		PIX*            pixMorphSequenceByComponent
		PIX*            pixMorphSequenceByRegion
		PIX*            pixMorphSequenceDwa
		PIX*            pixMorphSequenceMasked
		PIX*            pixMosaicColorShiftRGB
		PIX*            pixMultConstantColor
		PIX*            pixMultMatrixColor
		PIX*            pixMultiplyByColor
		PIX*            pixMultiplyGray
		PIX*            pixOctcubeQuantFromCmap
		PIX*            pixOctcubeQuantMixedWithGray
		PIX*            pixOctreeColorQuant
		PIX*            pixOctreeColorQuantGeneral
		PIX*            pixOctreeQuantByPopulation
		PIX*            pixOctreeQuantNumColors
		PIX*            pixOpen
		PIX*            pixOpenBrick
		PIX*            pixOpenBrickDwa
		PIX*            pixOpenCompBrick
		PIX*            pixOpenCompBrickDwa
		PIX*            pixOpenCompBrickExtendDwa
		PIX*            pixOpenGeneralized
		PIX*            pixOpenGray
		PIX*            pixOpenGray3
		PIX*            pixOr
		PIX*            pixOrientCorrect
		PIX*            pixOtsuThreshOnBackgroundNorm
		PIX*            pixPadToCenterCentroid
		PIX*            pixPaintBoxa
		PIX*            pixPaintBoxaRandom
		PIX*            pixPrepare1bpp
		PIX*            pixProjective
		PIX*            pixProjectiveColor
		PIX*            pixProjectiveGray
		PIX*            pixProjectivePta
		PIX*            pixProjectivePtaColor
		PIX*            pixProjectivePtaGray
		PIX*            pixProjectivePtaWithAlpha
		PIX*            pixProjectiveSampled
		PIX*            pixProjectiveSampledPta
		PIX*            pixQuadraticVShear
		PIX*            pixQuadraticVShearLI
		PIX*            pixQuadraticVShearSampled
		PIX*            pixQuantFromCmap
		PIX*            pixRandomHarmonicWarp
		PIX*            pixRankBinByStrip
		PIX*            pixRankColumnTransform
		PIX*            pixRankFilter
		PIX*            pixRankFilterGray
		PIX*            pixRankFilterRGB
		PIX*            pixRankFilterWithScaling
		PIX*            pixRankRowTransform
		PIX*            pixRead
		PIX*            pixReadFromMultipageTiff
		PIX*            pixReadIndexed
		PIX*            pixReadJp2k
		PIX*            pixReadJpeg
		PIX*            pixReadMem
		PIX*            pixReadMemBmp
		PIX*            pixReadMemFromMultipageTiff
		PIX*            pixReadMemGif
		PIX*            pixReadMemJp2k
		PIX*            pixReadMemJpeg
		PIX*            pixReadMemPng
		PIX*            pixReadMemPnm
		PIX*            pixReadMemSpix
		PIX*            pixReadMemTiff
		PIX*            pixReadMemWebP
		PIX*            pixReadStream
		PIX*            pixReadStreamBmp
		PIX*            pixReadStreamGif
		PIX*            pixReadStreamJp2k
		PIX*            pixReadStreamJpeg
		PIX*            pixReadStreamPng
		PIX*            pixReadStreamPnm
		PIX*            pixReadStreamSpix
		PIX*            pixReadStreamTiff
		PIX*            pixReadStreamWebP
		PIX*            pixReadTiff
		PIX*            pixReadWithHint
		PIX*            pixReduceBinary2
		PIX*            pixReduceRankBinary2
		PIX*            pixReduceRankBinaryCascade
		PIX*            pixRemoveAlpha
		PIX*            pixRemoveBorder
		PIX*            pixRemoveBorderConnComps
		PIX*            pixRemoveBorderGeneral
		PIX*            pixRemoveBorderToSize
		PIX*            pixRemoveColormap
		PIX*            pixRemoveColormapGeneral
		PIX*            pixRemoveSeededComponents
		PIX*            pixRenderContours
		PIX*            pixRenderPolygon
		PIX*            pixRenderRandomCmapPtaa
		PIX*            pixResizeToMatch
		PIX*            pixRotate
		PIX*            pixRotate180
		PIX*            pixRotate2Shear
		PIX*            pixRotate3Shear
		PIX*            pixRotate90
		PIX*            pixRotateAM
		PIX*            pixRotateAMColor
		PIX*            pixRotateAMColorCorner
		PIX*            pixRotateAMColorFast
		PIX*            pixRotateAMCorner
		PIX*            pixRotateAMGray
		PIX*            pixRotateAMGrayCorner
		PIX*            pixRotateBinaryNice
		PIX*            pixRotateBySampling
		PIX*            pixRotateOrth
		PIX*            pixRotateShear
		PIX*            pixRotateShearCenter
		PIX*            pixRotateWithAlpha
		PIX*            pixRunlengthTransform
		PIX*            pixSauvolaOnContrastNorm
		PIX*            pixScale
		PIX*            pixScaleAreaMap
		PIX*            pixScaleAreaMap2
		PIX*            pixScaleAreaMapToSize
		PIX*            pixScaleBinary
		PIX*            pixScaleBinaryWithShift
		PIX*            pixScaleByIntSampling
		PIX*            pixScaleBySampling
		PIX*            pixScaleBySamplingToSize
		PIX*            pixScaleBySamplingWithShift
		PIX*            pixScaleColor2xLI
		PIX*            pixScaleColor4xLI
		PIX*            pixScaleColorLI
		PIX*            pixScaleGeneral
		PIX*            pixScaleGray2xLI
		PIX*            pixScaleGray2xLIDither
		PIX*            pixScaleGray2xLIThresh
		PIX*            pixScaleGray4xLI
		PIX*            pixScaleGray4xLIDither
		PIX*            pixScaleGray4xLIThresh
		PIX*            pixScaleGrayLI
		PIX*            pixScaleGrayMinMax
		PIX*            pixScaleGrayMinMax2
		PIX*            pixScaleGrayRank2
		PIX*            pixScaleGrayRankCascade
		PIX*            pixScaleGrayToBinaryFast
		PIX*            pixScaleLI
		PIX*            pixScaleMipmap
		PIX*            pixScaleRGBToBinaryFast
		PIX*            pixScaleRGBToGray2
		PIX*            pixScaleRGBToGrayFast
		PIX*            pixScaleSmooth
		PIX*            pixScaleSmoothToSize
		PIX*            pixScaleToGray
		PIX*            pixScaleToGray16
		PIX*            pixScaleToGray2
		PIX*            pixScaleToGray3
		PIX*            pixScaleToGray4
		PIX*            pixScaleToGray6
		PIX*            pixScaleToGray8
		PIX*            pixScaleToGrayFast
		PIX*            pixScaleToGrayMipmap
		PIX*            pixScaleToResolution
		PIX*            pixScaleToSize
		PIX*            pixScaleToSizeRel
		PIX*            pixScaleWithAlpha
		PIX*            pixSeedfillBinary
		PIX*            pixSeedfillBinaryRestricted
		PIX*            pixSeedfillGrayBasin
		PIX*            pixSeedfillMorph
		PIX*            pixSeedspread
		PIX*            pixSelectByArea
		PIX*            pixSelectByAreaFraction
		PIX*            pixSelectByPerimSizeRatio
		PIX*            pixSelectByPerimToAreaRatio
		PIX*            pixSelectBySize
		PIX*            pixSelectByWidthHeightRatio
		PIX*            pixSelectComponentBySize
		PIX*            pixSelectiveConnCompFill
		PIX*            pixSetAlphaOverWhite
		PIX*            pixSetBlackOrWhiteBoxa
		PIX*            pixSetStrokeWidth
		PIX*            pixSetUnderTransparency
		PIX*            pixShiftByComponent
		PIX*            pixSimpleCaptcha
		PIX*            pixSimpleColorQuantize
		PIX*            pixSnapColor
		PIX*            pixSnapColorCmap
		PIX*            pixSobelEdgeFilter
		PIX*            pixStereoFromPair
		PIX*            pixStretchHorizontal
		PIX*            pixStretchHorizontalLI
		PIX*            pixStretchHorizontalSampled
		PIX*            pixStrokeWidthTransform
		PIX*            pixSubtract
		PIX*            pixSubtractGray
		PIX*            pixThinConnected
		PIX*            pixThinConnectedBySet
		PIX*            pixThreshOnDoubleNorm
		PIX*            pixThreshold8
		PIX*            pixThresholdGrayArb
		PIX*            pixThresholdOn8bpp
		PIX*            pixThresholdTo2bpp
		PIX*            pixThresholdTo4bpp
		PIX*            pixThresholdToBinary
		PIX*            pixThresholdToValue
		PIX*            pixTilingGetTile
		PIX*            pixTophat
		PIX*            pixTranslate
		PIX*            pixTwoSidedEdgeFilter
		PIX*            pixUnionOfMorphOps
		PIX*            pixUnpackBinary
		PIX*            pixUnsharpMasking
		PIX*            pixUnsharpMaskingFast
		PIX*            pixUnsharpMaskingGray
		PIX*            pixUnsharpMaskingGray1D
		PIX*            pixUnsharpMaskingGray2D
		PIX*            pixUnsharpMaskingGrayFast
		PIX*            pixVShear
		PIX*            pixVShearCenter
		PIX*            pixVShearCorner
		PIX*            pixVShearLI
		PIX*            pixVarThresholdToBinary
		PIX*            pixWarpStereoscopic
		PIX*            pixWindowedMean
		PIX*            pixWindowedMeanSquare
		PIX*            pixXor
		PIX*            pixaDisplay
		PIX*            pixaDisplayLinearly
		PIX*            pixaDisplayOnLattice
		PIX*            pixaDisplayPairTiledInColumns
		PIX*            pixaDisplayRandomCmap
		PIX*            pixaDisplayTiled
		PIX*            pixaDisplayTiledAndScaled
		PIX*            pixaDisplayTiledByIndex
		PIX*            pixaDisplayTiledInColumns
		PIX*            pixaDisplayTiledInRows
		PIX*            pixaDisplayTiledWithText
		PIX*            pixaDisplayUnsplit
		PIX*            pixaGetAlignedStats
		PIX*            pixaGetPix
		PIX*            pixaRenderComponent
		PIX*            pixaaDisplay
		PIX*            pixaaDisplayByPixa
		PIX*            pixaaGetPix
		PIX*            pixaccFinal
		PIX*            pixaccGetPix
		PIX*            pixacompDisplayTiledAndScaled
		PIX*            pixacompGetPix
		PIX*            recogModifyTemplate
		PIX*            recogProcessToIdentify
		PIX*            recogShowMatch
		PIX*            selDisplayInPix
		PIX*            selaDisplayInPix
		PIX*            wshedRenderColors
		PIX*            wshedRenderFill
		PIX**           pixaGetPixArray
		PIXA*           convertToNUpPixa
		PIXA*           jbAccumulateComposites
		PIXA*           jbDataRender
		PIXA*           jbTemplatesFromComposites
		PIXA*           makeColorfillTestData
		PIXA*           pixClipRectangles
		PIXA*           pixExtractBarcodes
		PIXA*           pixExtractRawTextlines
		PIXA*           pixExtractTextlines
		PIXA*           pixaAddBorderGeneral
		PIXA*           pixaAddTextNumber
		PIXA*           pixaAddTextlines
		PIXA*           pixaBinSort
		PIXA*           pixaClipToPix
		PIXA*           pixaConstrainedSelect
		PIXA*           pixaConvertTo1
		PIXA*           pixaConvertTo32
		PIXA*           pixaConvertTo8
		PIXA*           pixaConvertTo8Colormap
		PIXA*           pixaConvertToGivenDepth
		PIXA*           pixaConvertToNUpPixa
		PIXA*           pixaConvertToSameDepth
		PIXA*           pixaCopy
		PIXA*           pixaCreate
		PIXA*           pixaCreateFromBoxa
		PIXA*           pixaCreateFromPix
		PIXA*           pixaCreateFromPixacomp
		PIXA*           pixaDisplayBoxaa
		PIXA*           pixaDisplayMultiTiled
		PIXA*           pixaExtendByMorph
		PIXA*           pixaExtendByScaling
		PIXA*           pixaGetFont
		PIXA*           pixaInterleave
		PIXA*           pixaMakeFromTiledPix
		PIXA*           pixaMakeFromTiledPixa
		PIXA*           pixaModifyStrokeWidth
		PIXA*           pixaMorphSequenceByComponent
		PIXA*           pixaMorphSequenceByRegion
		PIXA*           pixaRead
		PIXA*           pixaReadBoth
		PIXA*           pixaReadFiles
		PIXA*           pixaReadFilesSA
		PIXA*           pixaReadMem
		PIXA*           pixaReadMemMultipageTiff
		PIXA*           pixaReadMultipageTiff
		PIXA*           pixaReadStream
		PIXA*           pixaRemoveOutliers1
		PIXA*           pixaRemoveOutliers2
		PIXA*           pixaRotate
		PIXA*           pixaRotateOrth
		PIXA*           pixaScale
		PIXA*           pixaScaleBySampling
		PIXA*           pixaScaleToSize
		PIXA*           pixaScaleToSizeRel
		PIXA*           pixaSelectByArea
		PIXA*           pixaSelectByAreaFraction
		PIXA*           pixaSelectByNumConnComp
		PIXA*           pixaSelectByPerimSizeRatio
		PIXA*           pixaSelectByPerimToAreaRatio
		PIXA*           pixaSelectBySize
		PIXA*           pixaSelectByWidthHeightRatio
		PIXA*           pixaSelectRange
		PIXA*           pixaSelectWithIndicator
		PIXA*           pixaSelectWithString
		PIXA*           pixaSetStrokeWidth
		PIXA*           pixaSort
		PIXA*           pixaSortByIndex
		PIXA*           pixaSplitPix
		PIXA*           pixaThinConnected
		PIXA*           pixaTranslate
		PIXA*           pixaaDisplayTiledAndScaled
		PIXA*           pixaaFlattenToPixa
		PIXA*           pixaaGetPixa
		PIXA*           recogAddDigitPadTemplates
		PIXA*           recogExtractPixa
		PIXA*           recogFilterPixaBySize
		PIXA*           recogMakeBootDigitTemplates
		PIXA*           recogTrainFromBoot
		PIXA*           showExtractNumbers
		PIXAA*          pixaSort2dByIndex
		PIXAA*          pixaaCreate
		PIXAA*          pixaaCreateFromPixa
		PIXAA*          pixaaRead
		PIXAA*          pixaaReadFromFiles
		PIXAA*          pixaaReadMem
		PIXAA*          pixaaReadStream
		PIXAA*          pixaaScaleToSize
		PIXAA*          pixaaScaleToSizeVar
		PIXAA*          pixaaSelectRange
		PIXAA*          recogSortPixaByClass
		PIXAC*          pixacompCreate
		PIXAC*          pixacompCreateFromFiles
		PIXAC*          pixacompCreateFromPixa
		PIXAC*          pixacompCreateFromSA
		PIXAC*          pixacompCreateWithInit
		PIXAC*          pixacompInterleave
		PIXAC*          pixacompRead
		PIXAC*          pixacompReadMem
		PIXAC*          pixacompReadStream
		PIXACC*         pixaccCreate
		PIXACC*         pixaccCreateFromPix
		PIXC*           pixacompGetPixcomp
		PIXC*           pixcompCopy
		PIXC*           pixcompCreateFromFile
		PIXC*           pixcompCreateFromPix
		PIXC*           pixcompCreateFromString
		PIXCMAP*        pixGetColormap
		PIXTILING*      pixTilingCreate
		PTA*            pixFindCornerPixels
		PTA*            pixGeneratePtaBoundary
		PTA*            pixSearchBinaryMaze
		PTA*            pixSearchGrayMaze
		PTA*            pixSubsampleBoundaryPixels
		PTA*            pixaCentroids
		PTA*            ptaCropToMask
		PTA*            ptaGetBoundaryPixels
		PTA*            ptaGetNeighborPixLocs
		PTA*            ptaGetPixelsFromPix
		PTA*            ptaReplicatePattern
		PTAA*           dewarpGetTextlineCenters
		PTAA*           dewarpRemoveShortLines
		PTAA*           pixGetOuterBordersPtaa
		PTAA*           ptaaGetBoundaryPixels
		PTAA*           ptaaIndexLabeledPixels
		SARRAY*         pixProcessBarcodes
		SARRAY*         pixReadBarcodes
		SEL*            pixGenerateSelBoundary
		SEL*            pixGenerateSelRandom
		SEL*            pixGenerateSelWithRuns
		SEL*            selCreateFromColorPix
		SEL*            selCreateFromPix
		SELA*           selaCreateFromColorPixa
		char*           pixGetText
		char*           pixWriteStringPS
		l_float32       pixAverageOnLine
		l_float32*      fpixGetData
		l_float32*      fpixaGetData
		l_float64*      dpixGetData
		l_int32         adjacentOnPixelInRaster
		l_int32         amapGetCountForColor
		l_int32         dpixGetWpl
		l_int32         fpixGetWpl
		l_int32         fpixaGetCount
		l_int32         nextOnPixelInRaster
		l_int32         pixChooseOutputFormat
		l_int32         pixColumnStats
		l_int32         pixConnCompIncrAdd
		l_int32         pixCopyInputFormat
		l_int32         pixCopyResolution
		l_int32         pixCopyText
		l_int32         pixCorrelationScoreThresholded
		l_int32         pixCountArbInRect
		l_int32         pixFindSkewOrthogonalRange
		l_int32         pixFreeAndSetData
		l_int32         pixFreeData
		l_int32         pixGetDepth
		l_int32         pixGetHeight
		l_int32         pixGetInputFormat
		l_int32         pixGetLastOnPixelInRun
		l_int32         pixGetSpp
		l_int32         pixGetWidth
		l_int32         pixGetWpl
		l_int32         pixGetXRes
		l_int32         pixGetYRes
		l_int32         pixHaustest
		l_int32         pixMeasureSaturation
		l_int32         pixRankHaustest
		l_int32         pixRowStats
		l_int32         pixScaleResolution
		l_int32         pixSetData
		l_int32         pixSetDepth
		l_int32         pixSetHeight
		l_int32         pixSetInputFormat
		l_int32         pixSetSpecial
		l_int32         pixSetSpp
		l_int32         pixSetWidth
		l_int32         pixSetWpl
		l_int32         pixSetXRes
		l_int32         pixSetYRes
		l_int32         pixSizesEqual
		l_int32         pixTRCMap
		l_int32         pixTRCMapGeneral
		l_int32         pixaAccumulateSamples
		l_int32         pixaGetBoxaCount
		l_int32         pixaGetCount
		l_int32         pixaaGetCount
		l_int32         pixaaIsFull
		l_int32         pixaccGetOffset
		l_int32         pixacompGetBoxaCount
		l_int32         pixacompGetCount
		l_int32         pixacompGetOffset
		l_int32*        pixMedianCutHisto
		l_ok            addColorizedGrayToCmap
		l_ok            boxaCombineOverlapsInPair
		l_ok            boxaCompareRegions
		l_ok            boxaPlotSides
		l_ok            boxaPlotSizes
		l_ok            compareTilesByHisto
		l_ok            convertFilesTo1bpp
		l_ok            dewarpFindHorizSlopeDisparity
		l_ok            dewarpPopulateFullRes
		l_ok            dewarpSinglePage
		l_ok            dewarpSinglePageInit
		l_ok            dewarpSinglePageRun
		l_ok            dewarpaApplyDisparity
		l_ok            dewarpaApplyDisparityBoxa
		l_ok            dpixAddMultConstant
		l_ok            dpixCopyResolution
		l_ok            dpixGetDimensions
		l_ok            dpixGetMax
		l_ok            dpixGetMin
		l_ok            dpixGetPixel
		l_ok            dpixGetResolution
		l_ok            dpixSetAllArbitrary
		l_ok            dpixSetData
		l_ok            dpixSetDimensions
		l_ok            dpixSetPixel
		l_ok            dpixSetResolution
		l_ok            dpixSetWpl
		l_ok            dpixWrite
		l_ok            dpixWriteMem
		l_ok            dpixWriteStream
		l_ok            fpixAddMultConstant
		l_ok            fpixCopyResolution
		l_ok            fpixGetDimensions
		l_ok            fpixGetMax
		l_ok            fpixGetMin
		l_ok            fpixGetPixel
		l_ok            fpixGetResolution
		l_ok            fpixPrintStream
		l_ok            fpixRasterop
		l_ok            fpixSetAllArbitrary
		l_ok            fpixSetData
		l_ok            fpixSetDimensions
		l_ok            fpixSetPixel
		l_ok            fpixSetResolution
		l_ok            fpixSetWpl
		l_ok            fpixWrite
		l_ok            fpixWriteMem
		l_ok            fpixWriteStream
		l_ok            fpixaAddFPix
		l_ok            fpixaGetFPixDimensions
		l_ok            fpixaGetPixel
		l_ok            fpixaSetPixel
		l_ok            jbAddPage
		l_ok            jbAddPageComponents
		l_ok            jbClassifyCorrelation
		l_ok            jbClassifyRankHaus
		l_ok            jbGetComponents
		l_ok            jbGetULCorners
		l_ok            l_generateCIDataForPdf
		l_ok            linearInterpolatePixelColor
		l_ok            linearInterpolatePixelGray
		l_ok            partifyPixac
		l_ok            pixAbsDiffInRect
		l_ok            pixAbsDiffOnLine
		l_ok            pixAccumulate
		l_ok            pixAddConstantGray
		l_ok            pixAddGrayColormap8
		l_ok            pixAddText
		l_ok            pixAddWithIndicator
		l_ok            pixAlphaIsOpaque
		l_ok            pixAssignToNearestColor
		l_ok            pixAverageInRect
		l_ok            pixAverageInRectRGB
		l_ok            pixBackgroundNormGrayArray
		l_ok            pixBackgroundNormGrayArrayMorph
		l_ok            pixBackgroundNormRGBArrays
		l_ok            pixBackgroundNormRGBArraysMorph
		l_ok            pixBestCorrelation
		l_ok            pixBlendCmap
		l_ok            pixBlendInRect
		l_ok            pixCentroid
		l_ok            pixCentroid8
		l_ok            pixCleanupByteProcessing
		l_ok            pixClearAll
		l_ok            pixClearInRect
		l_ok            pixClearPixel
		l_ok            pixClipBoxToEdges
		l_ok            pixClipBoxToForeground
		l_ok            pixClipToForeground
		l_ok            pixColorContent
		l_ok            pixColorFraction
		l_ok            pixColorGray
		l_ok            pixColorGrayCmap
		l_ok            pixColorGrayMaskedCmap
		l_ok            pixColorGrayRegionsCmap
		l_ok            pixColorSegmentClean
		l_ok            pixColorSegmentRemoveColors
		l_ok            pixColorsForQuantization
		l_ok            pixCombineMasked
		l_ok            pixCombineMaskedGeneral
		l_ok            pixCompareBinary
		l_ok            pixCompareGray
		l_ok            pixCompareGrayByHisto
		l_ok            pixCompareGrayOrRGB
		l_ok            pixComparePhotoRegionsByHisto
		l_ok            pixCompareRGB
		l_ok            pixCompareTiled
		l_ok            pixCompareWithTranslation
		l_ok            pixConformsToRectangle
		l_ok            pixConnCompIncrInit
		l_ok            pixConvertToPdf
		l_ok            pixConvertToPdfData
		l_ok            pixConvertToPdfDataSegmented
		l_ok            pixConvertToPdfSegmented
		l_ok            pixCopyColormap
		l_ok            pixCopyDimensions
		l_ok            pixCopyRGBComponent
		l_ok            pixCopySpp
		l_ok            pixCorrelationBinary
		l_ok            pixCorrelationScore
		l_ok            pixCorrelationScoreShifted
		l_ok            pixCorrelationScoreSimple
		l_ok            pixCountConnComp
		l_ok            pixCountPixels
		l_ok            pixCountPixelsInRect
		l_ok            pixCountPixelsInRow
		l_ok            pixCountRGBColors
		l_ok            pixCountRGBColorsByHash
		l_ok            pixCountTextColumns
		l_ok            pixCropAlignedToCentroid
		l_ok            pixCropToMatch
		l_ok            pixDecideIfPhotoImage
		l_ok            pixDecideIfTable
		l_ok            pixDecideIfText
		l_ok            pixDestroyColormap
		l_ok            pixDisplay
		l_ok            pixDisplayWithTitle
		l_ok            pixDisplayWrite
		l_ok            pixEndianByteSwap
		l_ok            pixEndianTwoByteSwap
		l_ok            pixEqual
		l_ok            pixEqualWithAlpha
		l_ok            pixEqualWithCmap
		l_ok            pixEstimateBackground
		l_ok            pixFillMapHoles
		l_ok            pixFindAreaFraction
		l_ok            pixFindAreaFractionMasked
		l_ok            pixFindAreaPerimRatio
		l_ok            pixFindCheckerboardCorners
		l_ok            pixFindColorRegions
		l_ok            pixFindDifferentialSquareSum
		l_ok            pixFindHistoPeaksHSV
		l_ok            pixFindHorizontalRuns
		l_ok            pixFindLargeRectangles
		l_ok            pixFindLargestRectangle
		l_ok            pixFindMaxHorizontalRunOnLine
		l_ok            pixFindMaxVerticalRunOnLine
		l_ok            pixFindNormalizedSquareSum
		l_ok            pixFindOverlapFraction
		l_ok            pixFindPerimSizeRatio
		l_ok            pixFindPerimToAreaRatio
		l_ok            pixFindRepCloseTile
		l_ok            pixFindSkew
		l_ok            pixFindSkewSweep
		l_ok            pixFindSkewSweepAndSearch
		l_ok            pixFindSkewSweepAndSearchScore
		l_ok            pixFindSkewSweepAndSearchScorePivot
		l_ok            pixFindStrokeLength
		l_ok            pixFindStrokeWidth
		l_ok            pixFindThreshFgExtent
		l_ok            pixFindVerticalRuns
		l_ok            pixFindWordAndCharacterBoxes
		l_ok            pixFlipPixel
		l_ok            pixForegroundFraction
		l_ok            pixFractionFgInMask
		l_ok            pixGenPhotoHistos
		l_ok            pixGenerateCIData
		l_ok            pixGetAutoFormat
		l_ok            pixGetAverageMasked
		l_ok            pixGetAverageMaskedRGB
		l_ok            pixGetAverageTiledRGB
		l_ok            pixGetBackgroundGrayMap
		l_ok            pixGetBackgroundGrayMapMorph
		l_ok            pixGetBackgroundRGBMap
		l_ok            pixGetBackgroundRGBMapMorph
		l_ok            pixGetBinnedColor
		l_ok            pixGetBinnedComponentRange
		l_ok            pixGetBlackOrWhiteVal
		l_ok            pixGetColorHistogram
		l_ok            pixGetColorHistogramMasked
		l_ok            pixGetColorNearMaskBoundary
		l_ok            pixGetColumnStats
		l_ok            pixGetDifferenceStats
		l_ok            pixGetDimensions
		l_ok            pixGetExtremeValue
		l_ok            pixGetLastOffPixelInRun
		l_ok            pixGetLocalSkewTransform
		l_ok            pixGetMaxColorIndex
		l_ok            pixGetMaxValueInRect
		l_ok            pixGetMostPopulatedColors
		l_ok            pixGetOuterBorder
		l_ok            pixGetPSNR
		l_ok            pixGetPerceptualDiff
		l_ok            pixGetPixel
		l_ok            pixGetPixelAverage
		l_ok            pixGetPixelStats
		l_ok            pixGetRGBLine
		l_ok            pixGetRGBPixel
		l_ok            pixGetRandomPixel
		l_ok            pixGetRangeValues
		l_ok            pixGetRankColorArray
		l_ok            pixGetRankValue
		l_ok            pixGetRankValueMasked
		l_ok            pixGetRankValueMaskedRGB
		l_ok            pixGetRasterData
		l_ok            pixGetRegionsBinary
		l_ok            pixGetResolution
		l_ok            pixGetRowStats
		l_ok            pixGetSortedNeighborValues
		l_ok            pixGetTileCount
		l_ok            pixGetWordBoxesInTextlines
		l_ok            pixGetWordsInTextlines
		l_ok            pixHShearIP
		l_ok            pixHasHighlightRed
		l_ok            pixInferResolution
		l_ok            pixItalicWords
		l_ok            pixLinearEdgeFade
		l_ok            pixLocalExtrema
		l_ok            pixMaxAspectRatio
		l_ok            pixMeanInRectangle
		l_ok            pixMeasureEdgeSmoothness
		l_ok            pixMinMaxNearLine
		l_ok            pixMirrorDetect
		l_ok            pixMultConstAccumulate
		l_ok            pixMultConstantGray
		l_ok            pixNumColors
		l_ok            pixNumSignificantGrayColors
		l_ok            pixNumberOccupiedOctcubes
		l_ok            pixOrientDetect
		l_ok            pixOtsuAdaptiveThreshold
		l_ok            pixPaintSelfThroughMask
		l_ok            pixPaintThroughMask
		l_ok            pixPlotAlongPta
		l_ok            pixPrintStreamInfo
		l_ok            pixQuadtreeMean
		l_ok            pixQuadtreeVariance
		l_ok            pixQuantizeIfFewColors
		l_ok            pixRasterop
		l_ok            pixRasteropFullImage
		l_ok            pixRasteropHip
		l_ok            pixRasteropIP
		l_ok            pixRasteropVip
		l_ok            pixRemoveMatchedPattern
		l_ok            pixRemoveUnusedColors
		l_ok            pixRemoveWithIndicator
		l_ok            pixRenderBox
		l_ok            pixRenderBoxArb
		l_ok            pixRenderBoxBlend
		l_ok            pixRenderBoxa
		l_ok            pixRenderBoxaArb
		l_ok            pixRenderBoxaBlend
		l_ok            pixRenderGridArb
		l_ok            pixRenderHashBox
		l_ok            pixRenderHashBoxArb
		l_ok            pixRenderHashBoxBlend
		l_ok            pixRenderHashBoxa
		l_ok            pixRenderHashBoxaArb
		l_ok            pixRenderHashBoxaBlend
		l_ok            pixRenderHashMaskArb
		l_ok            pixRenderLine
		l_ok            pixRenderLineArb
		l_ok            pixRenderLineBlend
		l_ok            pixRenderPlotFromNuma
		l_ok            pixRenderPlotFromNumaGen
		l_ok            pixRenderPolyline
		l_ok            pixRenderPolylineArb
		l_ok            pixRenderPolylineBlend
		l_ok            pixRenderPta
		l_ok            pixRenderPtaArb
		l_ok            pixRenderPtaBlend
		l_ok            pixResizeImageData
		l_ok            pixRotateShearCenterIP
		l_ok            pixRotateShearIP
		l_ok            pixSauvolaBinarize
		l_ok            pixSauvolaBinarizeTiled
		l_ok            pixScaleAndTransferAlpha
		l_ok            pixScanForEdge
		l_ok            pixScanForForeground
		l_ok            pixSeedfill
		l_ok            pixSeedfill4
		l_ok            pixSeedfill8
		l_ok            pixSeedfillGray
		l_ok            pixSeedfillGrayInv
		l_ok            pixSeedfillGrayInvSimple
		l_ok            pixSeedfillGraySimple
		l_ok            pixSelectMinInConnComp
		l_ok            pixSelectedLocalExtrema
		l_ok            pixSerializeToMemory
		l_ok            pixSetAll
		l_ok            pixSetAllArbitrary
		l_ok            pixSetAllGray
		l_ok            pixSetBlackOrWhite
		l_ok            pixSetBorderRingVal
		l_ok            pixSetBorderVal
		l_ok            pixSetChromaSampling
		l_ok            pixSetCmapPixel
		l_ok            pixSetColormap
		l_ok            pixSetComponentArbitrary
		l_ok            pixSetDimensions
		l_ok            pixSetInRect
		l_ok            pixSetInRectArbitrary
		l_ok            pixSetMasked
		l_ok            pixSetMaskedCmap
		l_ok            pixSetMaskedGeneral
		l_ok            pixSetMirroredBorder
		l_ok            pixSetOrClearBorder
		l_ok            pixSetPadBits
		l_ok            pixSetPadBitsBand
		l_ok            pixSetPixel
		l_ok            pixSetPixelColumn
		l_ok            pixSetRGBComponent
		l_ok            pixSetRGBPixel
		l_ok            pixSetResolution
		l_ok            pixSetSelectCmap
		l_ok            pixSetSelectMaskedCmap
		l_ok            pixSetText
		l_ok            pixSetTextCompNew
		l_ok            pixSetTextblock
		l_ok            pixSetTextline
		l_ok            pixSetZlibCompression
		l_ok            pixShiftAndTransferAlpha
		l_ok            pixSmoothConnectedRegions
		l_ok            pixSplitDistributionFgBg
		l_ok            pixSplitIntoCharacters
		l_ok            pixSwapAndDestroy
		l_ok            pixTestClipToForeground
		l_ok            pixTestForSimilarity
		l_ok            pixThresholdByConnComp
		l_ok            pixThresholdByHisto
		l_ok            pixThresholdForFgBg
		l_ok            pixThresholdPixelSum
		l_ok            pixThresholdSpreadNorm
		l_ok            pixTilingGetCount
		l_ok            pixTilingGetSize
		l_ok            pixTilingNoStripOnPaint
		l_ok            pixTilingPaintTile
		l_ok            pixTransferAllData
		l_ok            pixUpDownDetect
		l_ok            pixUsesCmapColor
		l_ok            pixVShearIP
		l_ok            pixVarianceInRect
		l_ok            pixVarianceInRectangle
		l_ok            pixWindowedStats
		l_ok            pixWindowedVariance
		l_ok            pixWindowedVarianceOnLine
		l_ok            pixWordBoxesByDilation
		l_ok            pixWordMaskByDilation
		l_ok            pixWrite
		l_ok            pixWriteAutoFormat
		l_ok            pixWriteCompressedToPS
		l_ok            pixWriteDebug
		l_ok            pixWriteImpliedFormat
		l_ok            pixWriteJp2k
		l_ok            pixWriteJpeg
		l_ok            pixWriteMem
		l_ok            pixWriteMemBmp
		l_ok            pixWriteMemGif
		l_ok            pixWriteMemJp2k
		l_ok            pixWriteMemJpeg
		l_ok            pixWriteMemPS
		l_ok            pixWriteMemPam
		l_ok            pixWriteMemPdf
		l_ok            pixWriteMemPng
		l_ok            pixWriteMemPnm
		l_ok            pixWriteMemSpix
		l_ok            pixWriteMemTiff
		l_ok            pixWriteMemTiffCustom
		l_ok            pixWriteMemWebP
		l_ok            pixWriteMixedToPS
		l_ok            pixWritePng
		l_ok            pixWriteSegmentedPageToPS
		l_ok            pixWriteStream
		l_ok            pixWriteStreamAsciiPnm
		l_ok            pixWriteStreamBmp
		l_ok            pixWriteStreamGif
		l_ok            pixWriteStreamJp2k
		l_ok            pixWriteStreamJpeg
		l_ok            pixWriteStreamPS
		l_ok            pixWriteStreamPam
		l_ok            pixWriteStreamPdf
		l_ok            pixWriteStreamPng
		l_ok            pixWriteStreamPnm
		l_ok            pixWriteStreamSpix
		l_ok            pixWriteStreamTiff
		l_ok            pixWriteStreamTiffWA
		l_ok            pixWriteStreamWebP
		l_ok            pixWriteTiff
		l_ok            pixWriteTiffCustom
		l_ok            pixWriteWebP
		l_ok            pixZero
		l_ok            pixaAddBox
		l_ok            pixaAddPix
		l_ok            pixaAddPixWithText
		l_ok            pixaAnyColormaps
		l_ok            pixaClear
		l_ok            pixaClipToForeground
		l_ok            pixaCompareInPdf
		l_ok            pixaComparePhotoRegionsByHisto
		l_ok            pixaConvertToPdf
		l_ok            pixaConvertToPdfData
		l_ok            pixaCountText
		l_ok            pixaEqual
		l_ok            pixaExtendArrayToSize
		l_ok            pixaExtractColumnFromEachPix
		l_ok            pixaFindDimensions
		l_ok            pixaGetBoxGeometry
		l_ok            pixaGetDepthInfo
		l_ok            pixaGetPixDimensions
		l_ok            pixaGetRenderingDepth
		l_ok            pixaHasColor
		l_ok            pixaInitFull
		l_ok            pixaInsertPix
		l_ok            pixaIsFull
		l_ok            pixaJoin
		l_ok            pixaRemovePix
		l_ok            pixaRemovePixAndSave
		l_ok            pixaRemoveSelected
		l_ok            pixaReplacePix
		l_ok            pixaSelectToPdf
		l_ok            pixaSetBoxa
		l_ok            pixaSetFullSizeBoxa
		l_ok            pixaSetText
		l_ok            pixaSizeRange
		l_ok            pixaSplitIntoFiles
		l_ok            pixaVerifyDepth
		l_ok            pixaVerifyDimensions
		l_ok            pixaWrite
		l_ok            pixaWriteCompressedToPS
		l_ok            pixaWriteDebug
		l_ok            pixaWriteFiles
		l_ok            pixaWriteMem
		l_ok            pixaWriteMemMultipageTiff
		l_ok            pixaWriteMemWebPAnim
		l_ok            pixaWriteMultipageTiff
		l_ok            pixaWriteStream
		l_ok            pixaWriteStreamInfo
		l_ok            pixaWriteStreamWebPAnim
		l_ok            pixaWriteWebPAnim
		l_ok            pixaaAddBox
		l_ok            pixaaAddPix
		l_ok            pixaaAddPixa
		l_ok            pixaaClear
		l_ok            pixaaInitFull
		l_ok            pixaaJoin
		l_ok            pixaaReplacePixa
		l_ok            pixaaSizeRange
		l_ok            pixaaTruncate
		l_ok            pixaaVerifyDepth
		l_ok            pixaaVerifyDimensions
		l_ok            pixaaWrite
		l_ok            pixaaWriteMem
		l_ok            pixaaWriteStream
		l_ok            pixaccAdd
		l_ok            pixaccMultConst
		l_ok            pixaccMultConstAccumulate
		l_ok            pixaccSubtract
		l_ok            pixacompAddBox
		l_ok            pixacompAddPix
		l_ok            pixacompAddPixcomp
		l_ok            pixacompConvertToPdf
		l_ok            pixacompConvertToPdfData
		l_ok            pixacompFastConvertToPdfData
		l_ok            pixacompGetBoxGeometry
		l_ok            pixacompGetPixDimensions
		l_ok            pixacompJoin
		l_ok            pixacompReplacePix
		l_ok            pixacompReplacePixcomp
		l_ok            pixacompSetOffset
		l_ok            pixacompWrite
		l_ok            pixacompWriteFiles
		l_ok            pixacompWriteMem
		l_ok            pixacompWriteStream
		l_ok            pixacompWriteStreamInfo
		l_ok            pixcmapIsValid
		l_ok            pixcompGetDimensions
		l_ok            pixcompGetParameters
		l_ok            pixcompWriteFile
		l_ok            pixcompWriteStreamInfo
		l_ok            quadtreeGetChildren
		l_ok            quadtreeGetParent
		l_ok            recogAddSample
		l_ok            recogCorrelationBestChar
		l_ok            recogCorrelationBestRow
		l_ok            recogCreateDid
		l_ok            recogIdentifyMultiple
		l_ok            recogIdentifyPix
		l_ok            recogIdentifyPixa
		l_ok            recogProcessLabeled
		l_ok            recogRemoveOutliers1
		l_ok            recogRemoveOutliers2
		l_ok            recogShowMatchesInRange
		l_ok            recogSplitIntoCharacters
		l_ok            recogTrainLabeled
		l_ok            regTestComparePix
		l_ok            regTestCompareSimilarPix
		l_ok            regTestWritePixAndCheck
		l_ok            selectDefaultPdfEncoding
		l_ok            wshedBasins
		l_uint32*       pixExtractData
		l_uint32*       pixGetData
		l_uint8*        pixGetTextCompNew
		l_uint8**       pixSetupByteProcessing
		void            bmfDestroy
		void            dpixDestroy
		void            fpixDestroy
		void            fpixaDestroy
		void            pixDestroy
		void            pixTilingDestroy
		void            pixaDestroy
		void            pixaaDestroy
		void            pixaccDestroy
		void            pixacompDestroy
		void            pixcompDestroy
		void            setPixelLow
		void**          pixGetLinePtrs
		void***         pixaGetLinePtrs


BOX*            box
BOX*            box1
BOX*            box2
BOX*            boxs
BOX**           pbox
BOX**           pbox1
BOX**           pbox2
BOX**           pboxc
BOX**           pboxd
BOX**           pboxn
BOX**           pboxtile
BOX**           pcropbox
BOXA*           boxa
BOXA*           boxa2
BOXA*           boxas
BOXA*           boxas2
BOXA*           boxaw
BOXA**          pboxa
BOXA**          pboxad
BOXA**          pboxad1
BOXA**          pboxad2
BOXA**          pboxaw
BOXAA*          baa
BOXAA**         pboxaac
DPIX*           dpix
DPIX*           dpix_msa
DPIX*           dpixs
DPIX*           dpixs1
DPIX*           dpixs2
FPIX*           fpix
FPIX*           fpixs
FPIX*           fpixs1
FPIX*           fpixs2
FPIX**          pfpixrv
FPIX**          pfpixv
FPIXA**         pfpixa
FPIXA**         pfpixa_rv
FPIXA**         pfpixa_v
L_BMF*          bmf
L_COMP_DATA**   pcid
L_DEWARPA*      dewa
L_DEWARPA**     pdewa
L_KERNEL*       kel
L_KERNEL*       kel1
L_KERNEL*       kel2
L_KERNEL*       kelx
L_KERNEL*       kely
L_KERNEL*       range_kel
L_KERNEL*       spatial_kel
L_PDF_DATA**    plpd
L_STACK*        stack
NUMA*           na
NUMA*           nab
NUMA*           nag
NUMA*           nahd
NUMA*           naindex
NUMA*           nar
NUMA*           nasc
NUMA*           nasizes
NUMA*           natags
NUMA*           nawd
NUMA**          pna
NUMA**          pnab
NUMA**          pnac
NUMA**          pnad
NUMA**          pnaehist
NUMA**          pnag
NUMA**          pnah
NUMA**          pnahisto
NUMA**          pnahue
NUMA**          pnai
NUMA**          pnaindex
NUMA**          pnal
NUMA**          pnalevels
NUMA**          pnamax
NUMA**          pnamean
NUMA**          pnamedian
NUMA**          pnamin
NUMA**          pnamode
NUMA**          pnamodecount
NUMA**          pnaohist
NUMA**          pnar
NUMA**          pnarootvar
NUMA**          pnasat
NUMA**          pnascore
NUMA**          pnastart
NUMA**          pnat
NUMA**          pnatot
NUMA**          pnav
NUMA**          pnaval
NUMA**          pnavar
NUMA**          pnaw
NUMAA*          naa
NUMAA*          naa2
NUMAA**         pnaa
PIX*            pix
PIX*            pix1
PIX*            pix2
PIX*            pix3
PIX*            pix4
PIX*            pix_ma
PIX*            pixacc
PIX*            pixb
PIX*            pixc
PIX*            pixd
PIX*            pixe
PIX*            pixg
PIX*            pixim
PIX*            pixm
PIX*            pixma
PIX*            pixmb
PIX*            pixmg
PIX*            pixmr
PIX*            pixms
PIX*            pixp
PIX*            pixs
PIX*            pixs1
PIX*            pixs2
PIX*            pixt
PIX*            pixvws
PIX*            pixw
PIX**           pcolormask1
PIX**           pcolormask2
PIX**           ppix
PIX**           ppix_corners
PIX**           ppixb
PIX**           ppixd
PIX**           ppixd1
PIX**           ppixd2
PIX**           ppixdb
PIX**           ppixdebug
PIX**           ppixdiff
PIX**           ppixdiff1
PIX**           ppixdiff2
PIX**           ppixe
PIX**           ppixg
PIX**           ppixhisto
PIX**           ppixhm
PIX**           ppixm
PIX**           ppixmax
PIX**           ppixmb
PIX**           ppixmg
PIX**           ppixmin
PIX**           ppixmr
PIX**           ppixms
PIX**           ppixn
PIX**           ppixr
PIX**           ppixrem
PIX**           ppixs
PIX**           ppixsave
PIX**           ppixsd
PIX**           ppixtb
PIX**           ppixtext
PIX**           ppixth
PIX**           ppixtm
PIX**           ppixvws
PIXA*           pixa
PIXA*           pixa2
PIXA*           pixadb
PIXA*           pixadebug
PIXA*           pixam
PIXA*           pixas
PIXA*           pixas2
PIXA**          ppixa
PIXA**          ppixad
PIXAA*          paa
PIXAA*          paas
PIXAC*          pixac
PIXAC*          pixac2
PIXAC*          pixacs
PIXC*           pixc
PIXCMAP*        cmap
PIXCMAP*        colormap
PIXCMAP**       pcmap
PIXTILING*      pt
PTA*            pta
PTA*            ptad
PTA*            ptap
PTA*            ptas
PTA**           ppta
PTA**           ppta_corners
PTA**           pptad
PTA**           pptas
PTA**           pptat
PTAA*           ptaa
PTAA*           ptaas
PTAA**          pptaa
SARRAY*         sa
SARRAY*         satypes
SARRAY*         savals
SARRAY**        psachar
SARRAY**        psaw
SEL*            sel
SELA*           sela
char            chr
char*           text
char**          pcharstr
PIX*            pix
PIX*            pix2
PIX*            pixs
char*           debugdir
char*           debugfile
char*           dirout
char*           edgevals
char*           fileout
char*           modestr
char*           modestring
char*           name
char*           outroot
char*           plotname
char*           selname
char*           sequence
char*           str
char*           subdir
char*           substr
char*           text
char*           textstr
char*           textstring
char*           title
l_uint8*        data
l_float32       a
l_float32       addc
l_float32       angle
l_float32       areaslop
l_float32       assumed
l_float32       b
l_float32       bc
l_float32       bdenom
l_float32       bfact
l_float32       bfract
l_float32       binfract
l_float32       bnum
l_float32       boff
l_float32       bwt
l_float32       clipfract
l_float32       colorthresh
l_float32       confprior
l_float32       cropfract
l_float32       delta
l_float32       deltafract
l_float32       delx
l_float32       dely
l_float32       distfract
l_float32       edgecrop
l_float32       edgefract
l_float32       erasefactor
l_float32       factor
l_float32       fract
l_float32       fractm
l_float32       fractp
l_float32       fractthresh
l_float32       fthresh
l_float32       gamma
l_float32       gc
l_float32       gdenom
l_float32       gfact
l_float32       gfract
l_float32       gnum
l_float32       goff
l_float32       gwt
l_float32       hf
l_float32       hf1
l_float32       hf2
l_float32       hitfract
l_float32       imagescale
l_float32       incr
l_float32       inval
l_float32       longside
l_float32       max_ht_ratio
l_float32       maxave
l_float32       maxfade
l_float32       maxfract
l_float32       maxhfract
l_float32       maxscore
l_float32       maxwiden
l_float32       minbsdelta
l_float32       minfgfract
l_float32       minfract
l_float32       minratio
l_float32       minscore
l_float32       minupconf
l_float32       missfract
l_float32       multc
l_float32       norm
l_float32       peakfract
l_float32       proxim
l_float32       radang
l_float32       range_stdev
l_float32       ranis
l_float32       rank
l_float32       rc
l_float32       rdenom
l_float32       rfact
l_float32       rfract
l_float32       rnum
l_float32       roff
l_float32       rwt
l_float32       scale
l_float32       scalefactor
l_float32       scalex
l_float32       scaley
l_float32       score
l_float32       score_threshold
l_float32       scorefract
l_float32       sharpfract
l_float32       shiftx
l_float32       shifty
l_float32       simthresh
l_float32       spatial_stdev
l_float32       stdev
l_float32       sweepcenter
l_float32       sweepdelta
l_float32       sweeprange
l_float32       target
l_float32       targetw
l_float32       textscale
l_float32       textthresh
l_float32       thresh
l_float32       thresh48
l_float32       threshdiff
l_float32       val
l_float32       validthresh
l_float32       vf
l_float32       vf1
l_float32       vf2
l_float32       wallps
l_float32       width
l_float32       x
l_float32       xfreq
l_float32       xmag
l_float32       xscale
l_float32       y
l_float32       yfreq
l_float32       ymag
l_float32       yscale
l_float32*      colvect
l_float32*      data
l_float32*      pa
l_float32*      pabsdiff
l_float32*      pangle
l_float32*      pave
l_float32*      pavediff
l_float32*      pb
l_float32*      pbval
l_float32*      pcolorfract
l_float32*      pconf
l_float32*      pcx
l_float32*      pcy
l_float32*      pdiff
l_float32*      pdiffarea
l_float32*      pdiffxor
l_float32*      pendscore
l_float32*      pfract
l_float32*      pfractdiff
l_float32*      pgval
l_float32*      phratio
l_float32*      pjpl
l_float32*      pjspl
l_float32*      pleftconf
l_float32*      pmaxave
l_float32*      pmaxval
l_float32*      pminave
l_float32*      pminval
l_float32*      ppixfract
l_float32*      ppsnr
l_float32*      pratio
l_float32*      prmsdiff
l_float32*      prootvar
l_float32*      prpl
l_float32*      prval
l_float32*      prvar
l_float32*      psat
l_float32*      pscalefact
l_float32*      pscore
l_float32*      psum
l_float32*      pupconf
l_float32*      pval
l_float32*      pval00
l_float32*      pval01
l_float32*      pval10
l_float32*      pval11
l_float32*      pvar
l_float32*      pvratio
l_float32*      pwidth
l_float32*      px
l_float32*      pxave
l_float32*      py
l_float32*      pyave
l_float32*      rowvect
l_float32*      vc
l_float32**     pscores
l_float64       addc
l_float64       inval
l_float64       multc
l_float64       val
l_float64*      data
l_float64*      pmaxval
l_float64*      pminval
l_float64*      pval
l_int32         accessflag
l_int32         accesstype
l_int32         adaptive
l_int32         addborder
l_int32         addh
l_int32         addlabel
l_int32         addw
l_int32         adjh
l_int32         adjw
l_int32         area1
l_int32         area2
l_int32         area3
l_int32         areathresh
l_int32         ascii85
l_int32         background
l_int32         bgval
l_int32         bh
l_int32         blackval
l_int32         bmax
l_int32         bmin
l_int32         border
l_int32         border1
l_int32         border2
l_int32         bordersize
l_int32         borderwidth
l_int32         bot
l_int32         botflag
l_int32         botpix
l_int32         boundcond
l_int32         bref
l_int32         bval
l_int32         bw
l_int32         bx
l_int32         by
l_int32         c1
l_int32         c2
l_int32         cellh
l_int32         cellw
l_int32         check_columns
l_int32         checkbw
l_int32         closeflag
l_int32         cmapflag
l_int32         cmflag
l_int32         codec
l_int32         col
l_int32         color
l_int32         colordiff
l_int32         colorflag
l_int32         colors
l_int32         comp
l_int32         components
l_int32         comptype
l_int32         compval
l_int32         conn
l_int32         connect
l_int32         connectivity
l_int32         contrast
l_int32         copyflag
l_int32         copyformat
l_int32         copytext
l_int32         cx
l_int32         cy
l_int32         d
l_int32         darkthresh
l_int32         debug
l_int32         debugflag
l_int32         debugindex
l_int32         debugsplit
l_int32         delh
l_int32         delm
l_int32         delp
l_int32         delta
l_int32         delw
l_int32         delx
l_int32         dely
l_int32         depth
l_int32         details
l_int32         dh
l_int32         diff
l_int32         diffthresh
l_int32         dilation
l_int32         dir
l_int32         direction
l_int32         dispflag
l_int32         display
l_int32         dispsep
l_int32         dispy
l_int32         dist
l_int32         distance
l_int32         distblend
l_int32         distflag
l_int32         dither
l_int32         ditherflag
l_int32         drawref
l_int32         dsize
l_int32         duration
l_int32         dw
l_int32         dx
l_int32         dy
l_int32         edgeclean
l_int32         edgethresh
l_int32         end
l_int32         erasedist
l_int32         errorflag
l_int32         etransx
l_int32         etransy
l_int32         extra
l_int32         factor
l_int32         factor1
l_int32         factor2
l_int32         fadeto
l_int32         filltype
l_int32         filtertype
l_int32         finalcolors
l_int32         first
l_int32         firstindent
l_int32         firstpage
l_int32         fontsize
l_int32         force8
l_int32         format
l_int32         gmax
l_int32         gmin
l_int32         grayin
l_int32         graylevels
l_int32         grayval
l_int32         gref
l_int32         gthick
l_int32         gval
l_int32         h
l_int32         h1
l_int32         h2
l_int32         halfsize
l_int32         halfw
l_int32         halfwidth
l_int32         hasborder
l_int32         hc
l_int32         hd
l_int32         height
l_int32         hf
l_int32         highthresh
l_int32         hint
l_int32         hitdist
l_int32         hitskip
l_int32         hmax
l_int32         hshift
l_int32         hsize
l_int32         hspacing
l_int32         huecenter
l_int32         huehw
l_int32         i
l_int32         iend
l_int32         ifnocmap
l_int32         inband
l_int32         include
l_int32         incolor
l_int32         incr
l_int32         index
l_int32         informat
l_int32         initcolor
l_int32         invert
l_int32         ipix
l_int32         istart
l_int32         j
l_int32         last
l_int32         left
l_int32         leftflag
l_int32         leftpix
l_int32         leftshift
l_int32         level
l_int32         level1
l_int32         level2
l_int32         level3
l_int32         level4
l_int32         lightthresh
l_int32         linew
l_int32         linewb
l_int32         linewba
l_int32         linewidth
l_int32         loc
l_int32         location
l_int32         loopcount
l_int32         lossless
l_int32         lower
l_int32         lowerclip
l_int32         lowthresh
l_int32         lr_add
l_int32         lr_clear
l_int32         mapdir
l_int32         mapval
l_int32         margin
l_int32         max
l_int32         maxbg
l_int32         maxbord
l_int32         maxcolors
l_int32         maxcomps
l_int32         maxdiff
l_int32         maxdiffh
l_int32         maxdiffw
l_int32         maxdist
l_int32         maxgray
l_int32         maxh
l_int32         maxheight
l_int32         maxiters
l_int32         maxkeep
l_int32         maxlimit
l_int32         maxmin
l_int32         maxncolors
l_int32         maxnx
l_int32         maxshift
l_int32         maxsize
l_int32         maxspan
l_int32         maxsub
l_int32         maxval
l_int32         maxw
l_int32         maxwidth
l_int32         maxyshift
l_int32         method
l_int32         metric
l_int32         minarea
l_int32         mincount
l_int32         mindel
l_int32         mindepth
l_int32         mindiff
l_int32         mindist
l_int32         mingray
l_int32         mingraycolors
l_int32         minh
l_int32         minheight
l_int32         minjump
l_int32         minlength
l_int32         minlines
l_int32         minmax
l_int32         minreversal
l_int32         minsize
l_int32         minsum
l_int32         mintarget
l_int32         minval
l_int32         minw
l_int32         minwidth
l_int32         missdist
l_int32         missskip
l_int32         n
l_int32         nangles
l_int32         nbins
l_int32         nblack1
l_int32         nblack2
l_int32         nblack3
l_int32         ncolor
l_int32         ncolors
l_int32         ncols
l_int32         ncomps
l_int32         ncontours
l_int32         negflag
l_int32         negvals
l_int32         nfiles
l_int32         ngray
l_int32         nhlines
l_int32         nincr
l_int32         niters
l_int32         nlevels
l_int32         nmax
l_int32         nmin
l_int32         normflag
l_int32         npages
l_int32         nparts
l_int32         npeaks
l_int32         npix
l_int32         npixels
l_int32         nrect
l_int32         nsamp
l_int32         nseeds
l_int32         nsels
l_int32         nslices
l_int32         nsplit
l_int32         nterms
l_int32         ntiles
l_int32         num
l_int32         nvlines
l_int32         nwhite1
l_int32         nwhite2
l_int32         nwhite3
l_int32         nx
l_int32         ny
l_int32         octlevel
l_int32         offset
l_int32         op
l_int32         opensize
l_int32         operation
l_int32         order
l_int32         orient
l_int32         orientflag
l_int32         outdepth
l_int32         outformat
l_int32         outline
l_int32         outres
l_int32         outwidth
l_int32         pad
l_int32         pageno
l_int32         parity
l_int32         pivot
l_int32         plotloc
l_int32         plottype
l_int32         polarity
l_int32         polyflag
l_int32         position
l_int32         printstats
l_int32         progressive
l_int32         quads
l_int32         quality
l_int32         range
l_int32         rank
l_int32         rankorder
l_int32         redleft
l_int32         redsearch
l_int32         redsweep
l_int32         reduction
l_int32         refpos
l_int32         refval
l_int32         regionflag
l_int32         relation
l_int32         remainder
l_int32         removedups
l_int32         res
l_int32         right
l_int32         rightflag
l_int32         rightpix
l_int32         rmax
l_int32         rmin
l_int32         rotation
l_int32         row
l_int32         rref
l_int32         runtype
l_int32         rval
l_int32         sampling
l_int32         satcenter
l_int32         sathw
l_int32         satlimit
l_int32         scale
l_int32         scalefactor
l_int32         scaleh
l_int32         scalew
l_int32         scanflag
l_int32         searchdir
l_int32         select
l_int32         select1
l_int32         select2
l_int32         selsize
l_int32         setblack
l_int32         setsize
l_int32         setval
l_int32         setwhite
l_int32         sharpwidth
l_int32         shift
l_int32         showall
l_int32         showmorph
l_int32         side
l_int32         sigbits
l_int32         sindex
l_int32         size
l_int32         skip
l_int32         skipdist
l_int32         skipsplit
l_int32         sm1h
l_int32         sm1v
l_int32         sm2h
l_int32         sm2v
l_int32         smooth
l_int32         smoothing
l_int32         smoothx
l_int32         smoothy
l_int32         sortorder
l_int32         sorttype
l_int32         spacing
l_int32         spacing1
l_int32         spacing2
l_int32         special
l_int32         spp
l_int32         start
l_int32         startindex
l_int32         startval
l_int32         subsamp
l_int32         subsample
l_int32         sval
l_int32         sx
l_int32         sy
l_int32         target
l_int32         targetthresh
l_int32         tb_add
l_int32         tb_clear
l_int32         thinfirst
l_int32         thresh
l_int32         threshdiff
l_int32         threshold
l_int32         threshval
l_int32         tilesize
l_int32         tilewidth
l_int32         top
l_int32         topflag
l_int32         toppix
l_int32         transparent
l_int32         tsize
l_int32         tw
l_int32         type
l_int32         type16
l_int32         type8
l_int32         upper
l_int32         upperclip
l_int32         upscaling
l_int32         use_alpha
l_int32         use_average
l_int32         use_pairs
l_int32         useboth
l_int32         usecmap
l_int32         val
l_int32         val0
l_int32         val1
l_int32         valcenter
l_int32         valhw
l_int32         vmaxb
l_int32         vmaxt
l_int32         vshift
l_int32         vsize
l_int32         vspacing
l_int32         vval
l_int32         w
l_int32         w1
l_int32         w2
l_int32         warnflag
l_int32         wc
l_int32         wd
l_int32         wf
l_int32         whiteval
l_int32         whsize
l_int32         width
l_int32         wpl
l_int32         wpls
l_int32         write_pdf
l_int32         write_pix
l_int32         write_pixa
l_int32         wtext
l_int32         x
l_int32         x0
l_int32         x1
l_int32         x2
l_int32         xcen
l_int32         xf
l_int32         xfact
l_int32         xi
l_int32         xloc
l_int32         xmax
l_int32         xmin
l_int32         xoverlap
l_int32         xres
l_int32         xsize
l_int32         xstart
l_int32         y
l_int32         y0
l_int32         y1
l_int32         y2
l_int32         ybendb
l_int32         ybendt
l_int32         ycen
l_int32         yf
l_int32         yfact
l_int32         yi
l_int32         yloc
l_int32         ymax
l_int32         ymin
l_int32         yoverlap
l_int32         yres
l_int32         ysize
l_int32         yslop
l_int32         ystart
l_int32         zbend
l_int32         zshiftb
l_int32         zshiftt
l_int32*        centtab
l_int32*        countarray
l_int32*        downcount
l_int32*        pabove
l_int32*        pbg
l_int32*        pbgval
l_int32*        pbias
l_int32*        pbl0
l_int32*        pbl1
l_int32*        pbl2
l_int32*        pbot
l_int32*        pbval
l_int32*        pcanclip
l_int32*        pchanged
l_int32*        pcmapflag
l_int32*        pcolor
l_int32*        pcomptype
l_int32*        pconforms
l_int32*        pcount
l_int32*        pcropwarn
l_int32*        pd
l_int32*        pdelx
l_int32*        pdely
l_int32*        pdepth
l_int32*        pempty
l_int32*        perror
l_int32*        pfgval
l_int32*        pformat
l_int32*        pfull
l_int32*        pfullba
l_int32*        pfullpa
l_int32*        pglobthresh
l_int32*        pgrayval
l_int32*        pgval
l_int32*        ph
l_int32*        phascmap
l_int32*        phascolor
l_int32*        phasred
l_int32*        phtfound
l_int32*        pindex
l_int32*        piscolor
l_int32*        pistext
l_int32*        plength
l_int32*        ploc
l_int32*        pmaxd
l_int32*        pmaxdepth
l_int32*        pmaxh
l_int32*        pmaxindex
l_int32*        pmaxval
l_int32*        pmaxw
l_int32*        pminh
l_int32*        pminval
l_int32*        pminw
l_int32*        pn
l_int32*        pncc
l_int32*        pncolors
l_int32*        pncols
l_int32*        pnerrors
l_int32*        pnoverlap
l_int32*        pnsame
l_int32*        pntext
l_int32*        pnvals
l_int32*        pnwarn
l_int32*        pnx
l_int32*        pny
l_int32*        popaque
l_int32*        poverflow
l_int32*        pres
l_int32*        protation
l_int32*        prval
l_int32*        psame
l_int32*        pscore
l_int32*        psimilar
l_int32*        psize
l_int32*        pthresh
l_int32*        ptlfound
l_int32*        ptop
l_int32*        ptype
l_int32*        pval
l_int32*        pvalid
l_int32*        pw
l_int32*        pwidth
l_int32*        px
l_int32*        pxa
l_int32*        pxmax
l_int32*        pxmaxloc
l_int32*        pxmin
l_int32*        pxminloc
l_int32*        pxres
l_int32*        pxstart
l_int32*        py
l_int32*        pya
l_int32*        pymax
l_int32*        pymaxloc
l_int32*        pymin
l_int32*        pyminloc
l_int32*        pyres
l_int32*        pystart
l_int32*        sumtab
l_int32*        tab
l_int32*        tab8
l_int32*        xend
l_int32*        xstart
l_int32*        yend
l_int32*        ystart
l_int32**       pneigh
l_uint16        val0
l_uint16        val1
l_uint32        bordercolor
l_uint32        color
l_uint32        colorb
l_uint32        colorba
l_uint32        colorval
l_uint32        diffcolor
l_uint32        dstval
l_uint32        fillval
l_uint32        hitcolor
l_uint32        misscolor
l_uint32        offset
l_uint32        outval
l_uint32        reduction
l_uint32        refval
l_uint32        refval1
l_uint32        refval2
l_uint32        seed
l_uint32        srcval
l_uint32        textcolor
l_uint32        threshold
l_uint32        transpix
l_uint32        val
l_uint32        val0
l_uint32        val1
l_uint32*       data
l_uint32*       pave
l_uint32*       pmaxval
l_uint32*       pval
l_uint32*       pvalue
l_uint32**      parray
l_uint32**      pcarray
l_uint32**      pdata
l_uint8         bval
l_uint8         grayval
l_uint8         gval
l_uint8         rval
l_uint8         val0
l_uint8         val1
l_uint8         val2
l_uint8         val3
l_uint8*        bufb
l_uint8*        bufg
l_uint8*        bufr
l_uint8*        intab
l_uint8**       lineptrs
l_uint8**       pdata
size_t          filesize
size_t          nbytes
size_t          size
size_t*         pencsize
size_t*         pfilesize
size_t*         pnbytes
size_t*         poffset
size_t*         psize
BOXA*           boxa
BOXA*           boxa1
BOXA*           boxas
BOXA*           boxas1
CCBORD*         ccb
CCBORDA*        ccba
DPIX*           dpix
DPIX*           dpixd
DPIX*           dpixs
DPIX**          pdpix
FILE*           fp
FPIX*           fpix
FPIX*           fpixd
FPIX*           fpixs
FPIX**          pfpix
FPIXA*          fpixa
FPIXA*          fpixas
FPIXA**         pfpixa
JBCLASSER*      classer
JBDATA*         data
L_AMAP*         amap
L_BMF*          bmf
L_BMF**         pbmf
L_DEWARP*       dew
L_DEWARPA*      dewa
L_KERNEL*       kel
L_RECOG*        recog
L_RECOG*        recogboot
L_RECOG**       precog
L_REGPARAMS*    rp
L_WSHED*        wshed
NUMAA*          naa1
PIX*            pix
PIX*            pix1
PIX*            pixb
PIX*            pixd
PIX*            pixm
PIX*            pixr
PIX*            pixs
PIX*            pixs1
PIX**           ppix
PIX**           ppixd
PIXA*           pixa
PIXA*           pixa1
PIXA*           pixac
PIXA*           pixad
PIXA*           pixas
PIXA*           pixas1
PIXA**          ppixa
PIXAA*          paa
PIXAA*          paad
PIXAA*          paas
PIXAA*          pixaa
PIXAA**         ppaa
PIXAC*          pixac
PIXAC*          pixac1
PIXAC*          pixacd
PIXAC**         ppixac
PIXACC*         pixacc
PIXACC**        ppixacc
PIXC*           pixc
PIXC*           pixcs
PIXC**          ppixc
PIXCMAP*        cmap
PIXTILING*      pt
PIXTILING**     ppt
PTA*            pta
PTA*            ptas
SARRAY*         sa
SEL*            sel
SELA*           sela
PIX*            pix
PIX*            pix1
PIX*            pixs
PIXCMAP*        cmap
char*           dir
char*           dirin
char*           dirname
char*           filename
char*           fname
char*           rootname
l_uint32*       data
l_uint8*        cdata
l_uint8*        data
l_uint8*        filedata
l_int32         hval
l_int32         n
l_int32         nsamp
l_int32         scale
l_int32         w
l_int32         width
l_uint32        color
l_uint32*       carray
l_uint32*       datas
l_uint32*       line
l_uint8*        data
l_uint8**       pdata
l_uint8**       pencdata
l_uint8**       pfiledata

*/
