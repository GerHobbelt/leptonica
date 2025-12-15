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


static const char *fnames[] = {"lyra.005.jpg", "lyra.036.jpg"};


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
static char* strrpbrkpast(const char* str, const char* set)
{
	const char* rv = str;
	const char* p = str;
	for (;;) {
		p = strpbrk(p, set);
		if (!p)
			break;
		p++;
		rv = p;
	}
	return (char *)rv;
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
			LEPT_FREE((void *)rv->named_args[i].name);
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



// Take a given path and see if it's a directory (in which case we deliver all image files within),
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
		L_ERROR("Cannot scan %s (%s)\n", __func__, dirname, strerror(errno));
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
	L_REGPARAMS* rp;

	if (regTestSetup(argc, argv, "api_demo", &rp))
		return 1;

	l_chooseDisplayProg(L_DISPLAY_WITH_OPEN);

	//lept_mkdir("lept/demo_pix");

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
		pixDisplayWithTitle(pixs, 50, 0, "pixs");
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
	pixDisplayWithTitle(pixd, 800 * i, 100, NULL);
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
pixDisplayWithTitle(pix4, 325, 700, NULL);
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
pixDisplayWithTitle(pix4, 975, 700, NULL);
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
pixDisplayWithTitle(pix4, 1600, 700, NULL);
boxDestroy(&box1);
boxDestroy(&box2);
pixDestroy(&pix4);
pixaDestroy(&pixa1);
pixDestroy(&pix1);

return regTestCleanup(rp);
}

#endif

















/*


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
