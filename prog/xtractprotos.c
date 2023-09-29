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
 * xtractprotos.c
 *
 *   This program accepts a list of C files on the command line
 *   and outputs the C prototypes to stdout.  It uses cpp to
 *   handle the preprocessor macros, and then parses the cpp output.
 *   In leptonica, it is used to make allheaders.h (and optionally
 *   leptprotos.h, which contains just the function prototypes.)
 *   In leptonica, only the file allheaders.h is included with
 *   source files.
 *
 *   An optional 'prestring' can be prepended to each declaration.
 *   And the function prototypes can either be sent to stdout, written
 *   to a named file, or placed in-line within allheaders.h.
 *
 *   The signature is:
 *
 *     xtractprotos [-prestring=<string>] [-protos=<where>] [list of C files]
 *
 *   Without -protos, the prototypes are written to stdout.
 *   With -protos, allheaders.h is rewritten:
 *      * if you use -protos=inline, the prototypes are placed within
 *        allheaders.h.
 *      * if you use -protos=leptprotos.h, the prototypes written to
 *        the file leptprotos.h, and alltypes.h has
 *           #include "leptprotos.h"
 *
 *   For constructing allheaders.h, two text files are provided:
 *      allheaders_top.txt
 *      allheaders_bot.txt
 *   The former contains the leptonica version number, so it must
 *   be updated when a new version is made.
 *
 *   For simple C prototype extraction, xtractprotos has essentially
 *   the same functionality as Adam Bryant's cextract, but the latter
 *   has not been officially supported for over 15 years, has been
 *   patched numerous times, and doesn't work with sys/sysmacros.h
 *   for 64 bit architecture.
 *
 *   This is used to extract all prototypes in liblept.
 *   The function that does all the work is parseForProtos(),
 *   which takes as input the output from cpp.
 *
 *   xtractprotos can run in leptonica to do an 'ab initio' generation
 *   of allheaders.h; that is, it can make allheaders.h without
 *   leptprotos.h and with an allheaders.h file of 0 length.
 *   Of course, the usual situation is to run it with a valid allheaders.h,
 *   which includes all the function prototypes.  To avoid including
 *   all the prototypes in the input for each file, cpp runs here
 *   with -DNO_PROTOS, so the prototypes are not included -- this is
 *   much faster.
 *
 *   The xtractprotos version number, defined below, is incremented
 *   whenever a new version is made.
 *
 *   Note: this uses cpp to preprocess the input.  (The name of the cpp
 *   tempfile is constructed below.  It has a "." in the tail, which
 *   Cygwin needs to prevent it from appending ".exe" to the filename.)
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <string.h>
#include "allheaders.h"
#include "../src/environ.h"
#include "demo_settings.h"

#include "mupdf/fitz.h"
#include "tessocr.h"


#define L_BUFSIZE 5120

static const char *version = "1.5.1";

#if defined(BUILD_MONOLITHIC)
#define main    lept_xtractprotos_main
#endif

int main(int    argc,
         const char **argv)
{
const char     *filein, *str, *tempfile, *prestring, *outprotos, *protostr, *incstring;
char      buf[L_BUFSIZE];
l_int32   i, maxindex, in_line, nflags, protos_added, firstfile, len, ret;
size_t    nbytes;
L_BYTEA  *ba, *ba2;
SARRAY   *sa;
// include paths spec copy of the build environment for the leptonica project:
char include_paths[L_BUFSIZE] = ".;../../thirdparty/leptonica/src;../../scripts/leptonica/include/leptonica;../../source/fitz;../../include;../../scripts/leptonica/include/leptonica;../../scripts/libjpeg-turbo;../../scripts/libpng;../../scripts/libtiff;../../thirdparty/leptonica/src;../../thirdparty/jbig2dec;../../thirdparty/owemdjee/libjpeg-turbo;../../thirdparty/openjpeg/src/lib/openjp2;../../thirdparty/libpng;../../thirdparty/libtiff/libtiff;../../thirdparty/zlib;../../scripts/zlib;../../thirdparty/owemdjee/libgif;../../thirdparty/owemdjee/libwebp/src;../../thirdparty/owemdjee/plf_nanotimer";

	fz_set_leptonica_mem(fz_get_global_context());
#if 0
	leptSetStderrHandler(NULL);

// #ifdef LEPTONICA_INTERCEPT_ALLOC
#if !defined(LEPTONICA_NO_CUSTOM_MEM_MANAGER)
    setPixMemoryManager(leptonica_malloc, leptonica_free);
#endif
#endif

    if (argc == 1) {
        lept_stderr(
                "xtractprotos [-prestring=<string>] [-protos=<where>] [-include=<paths>] "
                "[list of C files]/n"
                "where the prestring is prepended to each prototype, and /n"
                "protos can be either 'inline' or the name of an output "
                "prototype file.\n"
		        "The specified include paths are passed to the C preprocessor instead of "
				"the default set\nbuilt-in into the tool.\n");
			return 1;
    }

    setLeptDebugOK(1);

    /* ---------------------------------------------------------------- */
    /* Parse input flags and find prestring and outprotos, if requested */
    /* ---------------------------------------------------------------- */
    prestring = outprotos = NULL;
    in_line = FALSE;
    nflags = 0;
    maxindex = L_MIN(3, argc);
    for (i = 1; i < maxindex; i++) {
        if (argv[i][0] == '-') {
			char scanfbuf[100];
            if (!strncmp(argv[i], "-prestring", 10)) {
                nflags++;
				snprintf(scanfbuf, sizeof(scanfbuf), "prestring=%%%ds", (int)(L_BUFSIZE - 10));
				ret = sscanf(argv[i] + 1, scanfbuf, buf);
                if (ret != 1) {
                    lept_stderr("parse failure for prestring\n");
                    return 1;
                }
                if ((len = strlen(buf)) > L_BUFSIZE - 3) {
                    L_WARNING("prestring too large; omitting!\n", __func__);
                } else {
                    buf[len] = ' ';
                    buf[len + 1] = '\0';
                    prestring = stringNew(buf);
                }
			}
			else if (!strncmp(argv[i], "-protos", 7)) {
				nflags++;
				snprintf(scanfbuf, sizeof(scanfbuf), "protos=%%%ds", (int)(L_BUFSIZE - 10));
				ret = sscanf(argv[i] + 1, scanfbuf, buf);
				if (ret != 1) {
					lept_stderr("parse failure for protos\n");
					return 1;
				}
				outprotos = stringNew(buf);
				if (!strncmp(outprotos, "inline", 7))
					in_line = TRUE;
			}
			else if (!strncmp(argv[i], "-include", 8)) {
				nflags++;
				snprintf(scanfbuf, sizeof(scanfbuf), "include=%%%ds", (int)(L_BUFSIZE - 10));
				ret = sscanf(argv[i] + 1, scanfbuf, buf);
				if (ret != 1) {
					lept_stderr("parse failure for include\n");
					return 1;
				}
				if ((len = strlen(buf)) > L_BUFSIZE - 3) {
					L_WARNING("include paths list too large; omitting!\n", __func__);
				}
				else {
					strcpy(include_paths, buf);
				}
			}
		}
    }

    if (argc - nflags < 2) {
        lept_stderr("no files specified!\n");
        return 1;
    }

	// construct cpp (a.k.a.: gcc -E) command line include paths part *once* (as strtok() will nuke our source for this ;-) ):
	strcpy(buf, "");

	{
		const char* incpath = strtok(include_paths, ";");
		size_t blen = strlen(buf);
		char* dst = buf + blen;

		while (incpath && blen < L_BUFSIZE - 1)
		{
			snprintf(dst, L_BUFSIZE - blen, "-I%s ", incpath);
			buf[L_BUFSIZE - 1] = 0;
			blen += strlen(dst);
			dst = buf + blen;

			incpath = strtok(NULL, ";");
		}
		if (blen == L_BUFSIZE - 1)
		{
			lept_stderr("failure to construct cpp comman line: all combined include paths result in a too long command line: %s\n", buf);
			return 1;
		}
		// nuke last separating space in the output:
		buf[blen - 1] = 0;
	}

	incstring = stringNew(buf);

    /* ---------------------------------------------------------------- */
    /*                   Generate the prototype string                  */
    /* ---------------------------------------------------------------- */
    ba = l_byteaCreate(500);

        /* First the extern C head */
    sa = sarrayCreate(0);
    sarrayAddString(sa, "/*", L_COPY);
    snprintf(buf, L_BUFSIZE,
             " * These prototypes were autogen'd by xtractprotos, v. %s",
             version);
    sarrayAddString(sa, buf, L_COPY);
    sarrayAddString(sa, " */", L_COPY);
    sarrayAddString(sa, "#ifdef __cplusplus", L_COPY);
    sarrayAddString(sa, "extern \"C\" {", L_COPY);
    sarrayAddString(sa, "#endif  /* __cplusplus */\n", L_COPY);
    str = sarrayToString(sa, 1);
    l_byteaAppendString(ba, str);
    lept_free(str);
    sarrayDestroy(&sa);

        /* Then the prototypes */
    firstfile = 1 + nflags;
    protos_added = FALSE;
    if ((tempfile = l_makeTempFilename()) == NULL) {
        lept_stderr("failure to make a writeable temp file\n");
        return 1;
    }
    for (i = firstfile; i < argc; i++) {
        filein = argv[i];
        len = strlen(filein);
        if (filein[len - 1] == 'h')  /* skip .h files */
            continue;

		// construct cpp (a.k.a.: gcc -E) command line:
        snprintf(buf, L_BUFSIZE, "cpp -DNO_PROTOS %s %s %s",
                 incstring, filein, tempfile);
		//fprintf(stderr, "EXEC:\n%s\n", buf);

        ret = system(buf);  /* cpp */
        if (ret) {
            lept_stderr("cpp failure for %s; continuing\n", filein);
            continue;
        }

        if ((str = parseForProtos(tempfile, prestring)) == NULL) {
            lept_stderr("parse failure for %s; continuing\n", filein);
            continue;
        }
        if (strlen(str) > 1) {  /* strlen(str) == 1 is a file without protos */
            l_byteaAppendString(ba, str);
            protos_added = TRUE;
        }
        lept_free(str);
    }
    lept_rmfile(tempfile);
    lept_free(tempfile);

        /* Lastly the extern C tail */
    sa = sarrayCreate(0);
    sarrayAddString(sa, "\n#ifdef __cplusplus", L_COPY);
    sarrayAddString(sa, "}", L_COPY);
    sarrayAddString(sa, "#endif  /* __cplusplus */", L_COPY);
    str = sarrayToString(sa, 1);
    l_byteaAppendString(ba, str);
    lept_free(str);
    sarrayDestroy(&sa);

    protostr = (char *)l_byteaCopyData(ba, &nbytes);
    l_byteaDestroy(&ba);


    /* ---------------------------------------------------------------- */
    /*                       Generate the output                        */
    /* ---------------------------------------------------------------- */
    if (!outprotos) {  /* just write to stdout */
        //lept_stderr("%s\n", protostr);    <-- will truncate the meessage and besides: we expect the output on STDOUT, **not** STDERR!
		fprintf(stdout, "%s\n", protostr);
		lept_free(protostr);
        return 0;
    }

        /* If no protos were found, do nothing further */
    if (!protos_added) {
        lept_stderr("No protos found\n");
        lept_free(protostr);
        return 1;
    }

        /* Make the output files */
    ba = l_byteaInitFromFile("allheaders_top.txt");
    if (!in_line) {
        snprintf(buf, sizeof(buf), "#include \"%s\"\n", outprotos);

#include "monolithic_examples.h"

        l_byteaAppendString(ba, buf);
        l_binaryWrite(outprotos, "w", protostr, nbytes);
    } else {
        l_byteaAppendString(ba, protostr);
    }
    ba2 = l_byteaInitFromFile("allheaders_bot.txt");
    l_byteaJoin(ba, &ba2);
    l_byteaWrite("allheaders.h", ba, 0, 0);
    l_byteaDestroy(&ba);
    lept_free(protostr);
    return 0;
}
