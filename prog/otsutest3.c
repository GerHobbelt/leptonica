/*====================================================================*f
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
 *   otsutest3.c
 *
 *   This demonstrates the usefulness of the modified version of Otsu
 *   for thresholding an image that doesn't have a well-defined
 *   background color.
 *
 *   This is a progression on otsutest2, where otsutest3 adds the ability
 *   to apply Otsu, Adaptive Otsu, et al IN BULK: you may specify one or
 *   more 'response files' (listing wildcarded filespecs to match and collect),
 *   wildcarded specs and/or direct filenames/paths to process:
 *   each of these will be subjected to a series of Otsu et al binarization/thresholding
 *   processes, with various degrees of preprocessing, where all results
 *   are reported as image strips for easy human evaluation and comparison.
 * 
 *   Standard Otsu binarization is done with scorefract = 0.0, which
 *   returns the threshold at the maximum value of the score.  However.
 *   this value is up on the shoulder of the background, and its
 *   use causes some of the dark background to be binarized as foreground.
 *
 *   Using the modified Otsu with scorefract = 0.1 returns a threshold
 *   at the lowest value of this histogram such that the score
 *   is at least 0.9 times the maximum value of the score.  This allows
 *   the threshold to be taken in the histogram minimum between
 *   the fg and bg peaks, producing a much cleaner binarization.
 */


#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include "allheaders.h"
#include "demo_settings.h"

#include "monolithic_examples.h"



#if defined(BUILD_MONOLITHIC)
#define main   lept_otsutest3_main
#endif

int main(int    argc,
	const char** argv)
{
	char       textstr[L_MAX(256, MAX_PATH)];
	l_int32    i, thresh, fgval, bgval;
	l_float32  scorefract;
	L_BMF* bmf;
	PIX* pixs, * pixb, * pixg, * pixp, * pix1, * pix2, * pix3, * pixth;
	PIXA* pixa1, * pixad;

	setLeptDebugOK(1);
	lept_mkdir("lept/otsu");

	SARRAY* sargv = NULL;
	if (argc <= 1)
	{
		// default:
		sargv = sarrayCreate(0);
		sarrayAddString(sargv, "1555.007.jpg", L_COPY);
	}
	else
	{
		sargv = lept_locate_all_files_in_searchpaths(argc - 1, argv + 1);
	}

	int argv_count = sarrayGetCount(sargv);
	for (int argidx = 0; argidx < argv_count; argidx++)
	{
		const char* filename = sarrayGetString(sargv, argidx, L_NOCOPY);
		const char* filepath = DEMOPATH(filename);
		filename = getPathBasename(filepath, FALSE);
		char* sani_filename = sanitizePathToIdentifier(NULL, 70, argidx + 1, filepath, "@#_-");


		lept_stderr("\n\n\nProcessing image #%d: %s = %s :: %s\n", argidx + 1, filename, filepath, sani_filename);


		pixs = pixRead(filepath);
		pixg = pixConvertTo8(pixs, 0);

		int prev_sx = 0, prev_sy = 0;
		int w, h;
		pixGetDimensions(pixs, &w, &h, NULL);

		bmf = bmfCreate(NULL, 8);
		pixad = pixaCreate(0);
		for (int grid = 1; grid <= L_MIN(w / 2, h / 2); grid += L_MAX(1, grid / 2)) {
			// aim for single-tile processing:
			int sx = w / grid;
			int sy = h / grid;
			// and just to show a clear single-grid-cell sx value: 2000, for the first round.
			if (grid == 1) {
				sx = L_MAX(2000, sx);
				sy = L_MAX(2000, sy);
			}

			// apply sanity limits that are required by the Otsu APIs: if we don't do this, those will error out anyway!
			sx = L_MAX(16, sx);
			sy = L_MAX(16, sy);

			if (sx == prev_sx && sy == prev_sy) {
				// force loop termination!
				break;
			}

			for (i = 0; i < 3; i++) {
				pixa1 = pixaCreate(2);
				scorefract = 0.1 * i;
				lept_stderr("\nScorefrac: %1.3f, Grid: %d\n", scorefract, grid);

				/* Show the histogram of gray values and the split location */
				pixSplitDistributionFgBg(pixg, scorefract, 1,
					&thresh, &fgval, &bgval, &pixp);
				lept_stderr("thresh = %d, fgval = %d, bgval = %d\n",
					thresh, fgval, bgval);
				pixaAddPix(pixa1, pixs, L_COPY);
				pixaAddPix(pixa1, pixg, L_COPY);
				pixaAddPix(pixa1, pixp, L_INSERT);

				for (int j = 0; j <= 3; j++) {

					/* Get a 1 bpp version; use a single tile */
					pixOtsuAdaptiveThreshold(pixg, sx, sy, j, j, scorefract,
						&pixth, &pixb);
					pixaAddPix(pixa1, pixg, L_COPY);
					pixaAddPix(pixa1, pixb, L_INSERT);

					// sampled-scaling of the threshold 'image' to the same size as the source image, so one can properly inspect the cell threshold colors:
					l_int32    thw, thh;
					l_float32  scalex, scaley;

					pixGetDimensions(pixth, &thw, &thh, NULL);
					scalex = (l_float32)w / (l_float32)thw;
					scaley = (l_float32)h / (l_float32)thh;

					pix2 = pixScaleBySamplingWithShift(pixth, scalex, scaley, 0, 0);
					pixDestroy(&pixth);

					pixaAddPix(pixa1, pix2, L_INSERT);

					/* improved version of the API: */

					/* Get a 1 bpp version; use a single tile */
					pixOtsuAdaptiveThreshold2(pixg, sx, sy, j, j, scorefract,
						&pixth, &pixb);
					pixaAddPix(pixa1, pixg, L_COPY);
					pixaAddPix(pixa1, pixb, L_INSERT);

					// sampled-scaling of the threshold 'image' to the same size as the source image, so one can properly inspect the cell threshold colors:
					pixGetDimensions(pixth, &thw, &thh, NULL);
					scalex = (l_float32)w / (l_float32)thw;
					scaley = (l_float32)h / (l_float32)thh;

					pix2 = pixScaleBySamplingWithShift(pixth, scalex, scaley, 0, 0);
					pixDestroy(&pixth);

					pixaAddPix(pixa1, pix2, L_INSERT);
				}

				/* Join these together and add some text */
				pix1 = pixaDisplayTiledInColumns(pixa1, 3, 1.0, 20, 2);
				snprintf(textstr, sizeof(textstr),
					"Scorefract = %3.1f\nGrid: %d x %d (cell #: %d x %d)\nH x W: %d x %d\nThresh = %d (%s)", scorefract, sx, sy, grid, grid, pixGetHeight(pixg), pixGetWidth(pixg), thresh, filename);
				pix2 = pixAddSingleTextblock(pix1, bmf, textstr, 0x06670f00,
					L_ADD_BELOW, NULL);

				/* Save and display the result */
				pixaAddPix(pixad, pix2, L_INSERT);
				snprintf(textstr, sizeof(textstr), "/tmp/lept/otsu/%s.%03d.Grid-%02d.ScoreFrac-%03d.png", sani_filename, argidx + 1, grid, (int)i);
				pixWrite(textstr, pix2, IFF_PNG);
				pixDisplayWithTitle(pix2, 100, 100, filepath, TRUE);
				pixDestroy(&pix1);
				pixaDestroy(&pixa1);
			}

			prev_sx = sx;
			prev_sy = sy;
		}

		/* Use a smaller tile for Otsu */
		for (i = 0; i < 2; i++) {
			scorefract = 0.1 * i;
			pixOtsuAdaptiveThreshold(pixg, 300, 300, 0, 0, scorefract,
				NULL, &pixb);
			pix1 = pixAddBlackOrWhiteBorder(pixb, 2, 2, 2, 2, L_GET_BLACK_VAL);
			//pix2 = pixScale(pix1, 0.5, 0.5);
			pix2 = pixClone(pix1);
			snprintf(textstr, sizeof(textstr),
				"Scorefract = %3.1f (%s)", scorefract, filename);
			pix3 = pixAddSingleTextblock(pix2, bmf, textstr, 1,
				L_ADD_BELOW, NULL);
			snprintf(textstr, sizeof(textstr), "/tmp/lept/otsu/%s.%03d.%03d.300.png", sani_filename, argidx + 1, (int)i);
			pixWrite(textstr, pix3, IFF_PNG);
			pixaAddPix(pixad, pix3, L_INSERT);
			pixDestroy(&pixb);
			pixDestroy(&pix1);
			pixDestroy(&pix2);
		}

		snprintf(textstr, sizeof(textstr), "/tmp/lept/otsu/%s.result.pdf", sani_filename);
		char* out_fullname = genPathname(textstr, NULL);
		lept_stderr("Writing to: %s --> %s\n", textstr, out_fullname);
		pixaConvertToPdf(pixad, 75, 1.0, 0, 0, "Otsu thresholding", out_fullname);
		stringDestroy(&out_fullname);
		bmfDestroy(&bmf);
		pixDestroy(&pixs);
		pixDestroy(&pixg);
		pixaDestroy(&pixad);

		LEPT_FREE(filename);
		LEPT_FREE(sani_filename);
		LEPT_FREE(filepath);
	}

	sarrayDestroy(&sargv);

	return 0;
}
