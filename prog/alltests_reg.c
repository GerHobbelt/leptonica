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
 *  alltests_reg.c
 *
 *    Tests all the reg tests:
 *
 *        alltests_reg command
 *
 *    where
 *        <command> == "generate" to make the golden files in /tmp/golden
 *        <command> == "compare" to make local files and compare with
 *                     the golden files
 *        <command> == "display" to make local files and display
 *
 *    You can also run each test individually with any one of these
 *    arguments.  Warning: if you run this with "display", a very
 *    large number of images will be displayed on the screen.
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif /* HAVE_CONFIG_H */

#include <string.h>
#include "allheaders.h"
#include "demo_settings.h"

#include "monolithic_examples.h"


static const char *tests[] = {
                              "adaptmap",
                              "adaptnorm",
                              "affine",
                              "alphaops",
                              "alphaxform",
                              "baseline",
                              "bilateral2",
                              "bilinear",
                              "binarize",
                              "binmorph1",
                              "binmorph3",
                              "binmorph6",
                              "blackwhite",
                              "blend1",
                              "blend2",
                              "blend3",
                              "blend4",
                              "blend5",
                              "boxa1",
                              "boxa2",
                              "boxa3",
                              "boxa4",
                              "bytea",
                              "ccbord",
                              "ccthin1",
                              "ccthin2",
                              "checkerboard",
                              "circle",
                              "cmapquant",
                              "colorcontent",
                              "colorfill",
                              "coloring",
                              "colorize",
                              "colormask",
                              "colormorph",
                              "colorquant",
                              "colorseg",
                              "colorspace",
                              "compare",
                              "compfilter",
                              "conncomp",
                              "conversion",
                              "convolve",
                              "crop",
                              "dewarp",
                              "distance",
                              "dither",
                              "dna",
                              "dwamorph1",
                              "edge",
                              "encoding",
                              "enhance",
                              "equal",
                              "expand",
                              "extrema",
                              "falsecolor",
                              "fhmtauto",
                         /*   "files",  */
                              "findcorners",
                              "findpattern",
                              "flipdetect",
                              "fpix1",
                              "fpix2",
                              "genfonts",
#if HAVE_LIBGIF
                              "gifio",
#endif  /* HAVE_LIBGIF */
                              "grayfill",
                              "graymorph1",
                              "graymorph2",
                              "grayquant",
                              "hardlight",
                              "hash",
                              "heap",
                              "insert",
                              "ioformats",
                              "iomisc",
                              "italic",
                              "jbclass",
#if HAVE_LIBJP2K
                              "jp2kio",
#endif  /* HAVE_LIBJP2K */
                              "jpegio",
                              "kernel",
                              "label",
                              "lineremoval",
                              "locminmax",
                              "logicops",
                              "lowaccess",
                              "lowsat",
                              "maze",
                              "mtiff",
                              "multitype",
                              "numa1",
                              "numa2",
                              "numa3",
                              "nearline",
                              "newspaper",
                              "overlap",
                              "pageseg",
                              "paint",
                              "paintmask",
                              "partition",
                              "pdfio1",
                              "pdfio2",
                              "pdfseg",
                              "pixa1",
                              "pixa2",
                              "pixadisp",
                              "pixcomp",
                              "pixmem",
                              "pixserial",
                              "pngio",
                              "pnmio",
                              "projection",
                              "projective",
                              "psio",
                              "psioseg",
                              "pta",
                              "ptra1",
                              "ptra2",
                              "quadtree",
                              "rank",
                              "rankbin",
                              "rankhisto",
                              "rasterop",
                              "rasteropip",
                              "rectangle",
                              "rotate1",
                              "rotate2",
                              "rotateorth",
                              "scale",
                              "seedspread",
                              "selio",
                              "shear1",
                              "shear2",
                              "skew",
                              "smallpix",
                              "speckle",
                              "splitcomp",
                              "string",
                              "subpixel",
                              "texturefill",
                              "threshnorm",
                              "translate",
                              "warper",
                              "watershed",
#if HAVE_LIBWEBP_ANIM
                              "webpanimio",
#endif  /* HAVE_LIBWEBP_ANIM */
#if HAVE_LIBWEBP
                              "webpio",
#endif  /* HAVE_LIBWEBP */
                              "wordboxes",
                              "writetext",
                              "xformbox",
                             };

static const char *header = {"\n=======================\n"
                             "Regression Test Results\n"
                             "======================="};


#if defined(BUILD_MONOLITHIC)
#define main   lept_alltests_reg_main
#endif

int main(int    argc,
         const char **argv)
{
char    *str, *results_file = NULL;
char     command[256], buf[256];
l_int32  i, ntests, dotest, nfail, ret, start, stop;
SARRAY  *sa;

    if (argc != 2)
        return ERROR_INT(" Syntax alltests_reg [generate | compare | display]",
                         __func__, 1);

    setLeptDebugOK(1);  /* required for testing */
    l_getCurrentTime(&start, NULL);
    ntests = sizeof(tests) / sizeof(char *);
    lept_stderr("Running alltests_reg:\n"
            "This currently tests %d regression test\n"
            "programs in the /prog directory.\n", ntests);

	results_file = genPathname("/tmp/lept", "reg_results.txt");

        /* Clear the output file if we're doing the set of reg tests */
    dotest = strcmp(argv[1], "compare") ? 0 : 1;
    if (dotest) {
        sa = sarrayCreate(3);
        sarrayAddString(sa, header, L_COPY);
        sarrayAddString(sa, getLeptonicaVersion(), L_INSERT);
        sarrayAddString(sa, getImagelibVersions(), L_INSERT);
        str = sarrayToString(sa, 1);
        sarrayDestroy(&sa);
        l_binaryWrite(results_file, "w", str, strlen(str));
        lept_free(str);
    }

    nfail = 0;
    for (i = 0; i < ntests; i++) {
#if !defined(BUILD_MONOLITHIC)
#ifndef  _WIN32
        snprintf(command, sizeof(command) - 2, "./%s_reg %s", tests[i], argv[1]);
#else  /* windows interprets '/' as a commandline flag */
        snprintf(command, sizeof(command) - 2, "%s_reg %s", tests[i], argv[1]);
#endif  /* ! _WIN32 */
#else
		const char* xp = argv[0];
		const char* pe = strrchr(xp, '/');
		if (pe)
			pe++;
		else
			pe = xp;
		const char* pe2 = strrchr(pe, '\\');
		if (pe2)
			pe = pe2 + 1;
		int plen = (pe - xp);
		const char* ext =
#if defined(_WIN32)
			".exe";
#else
			"";
#endif
		snprintf(command, sizeof(command) - 2, "%.*smutool_ex%s lept_%s %s", plen, xp, ext, tests[i], argv[1]);
#endif
        ret = system(command);
        if (ret) {
            snprintf(buf, sizeof(buf), "Failed to complete %s_reg\n", tests[i]);
            if (dotest) {
                l_binaryWrite(results_file, "a",
                              buf, strlen(buf));
                nfail++;
            }
            else
                lept_stderr("%s", buf);
        }
    }

    if (dotest) {
#ifndef _WIN32
        snprintf(command, sizeof(command) - 2, "cat %s", results_file);
#else
        snprintf(command, sizeof(command) - 2, "type \"%s\"", results_file);
#endif  /* !_WIN32 */
        ret = system(command);
        lept_stderr("Success in %d of %d *_reg programs (output matches"
                " the \"golden\" files)\n", ntests - nfail, ntests);
    }

    stringDestroy(&results_file);

    l_getCurrentTime(&stop, NULL);
    lept_stderr("Time for all regression tests: %d sec\n", stop - start);
    return 0;
}
