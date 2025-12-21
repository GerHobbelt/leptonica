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

int main(int          argc,
         const char** argv)
{
	char       textstr[L_MAX(256, MAX_PATH)];
	l_int32    i, thresh, fgval, bgval;
	l_float32  scorefract;
	L_BMF* bmf;
	PIX* pixs, * pixb, * pixg, * pixp, * pix1, * pix2, * pixth;
	PIXA* pixa1, * pixad;
	L_REGPARAMS* rp;

	if (regTestSetup(argc, argv, "otsu3", NULL, &rp))
		return 1;

	//lept_mkdir("lept/otsu3");

		//sarrayAddString(sargv, "1555.007.jpg", L_COPY);

	// every input file is treated as another round and represents the parent level in the step hierarchy:
	//int steplevel = leptDebugGetStepLevel();

	int argv_count = regGetArgCount(rp);
	if (argv_count == 0) {
		L_WARNING("no image files specified on the command line for processing: assuming a default input set.\n", __func__);
	}
	for (regMarkStartOfFirstTestround(rp, +1); regHasFileArgsAvailable(rp); regMarkEndOfTestround(rp))
	{
		// precaution: make sure we are at the desired depth in every round, even if called code forgot or failed to pop their additional level(s)
		leptDebugPopStepLevelTo(rp->base_step_level);

		const char* filepath = regGetFileArgOrDefault(rp, "1555.007.jpg");
		leptDebugSetStepIdAtSLevel(-1, regGetCurrentArgIndex(rp));   // inc parent level
		leptDebugSetFilePathPartFromTail(filepath, -2);

		{
			char* destdir = leptDebugGenFilepath("");
			char* real_destdir = genPathname(destdir, "(output)");
			lept_stderr("\n\n\nProcessing image #%d~#%s:\n  %s :: %s/(output)\n    --> %s\n", regGetCurrentArgIndex(rp), leptDebugGetStepIdAsString(), filepath, destdir, real_destdir);
			stringDestroy(&real_destdir);
		}

		pixs = pixRead(filepath);

		snprintf(textstr, sizeof(textstr), "source: %s", filepath);
		pixSetText(pixs, textstr);

		pixg = pixConvertTo8(pixs, 0);
		pixSetText(pixg, "(grayscale)");

		int prev_sx = 0, prev_sy = 0;
		int w, h;
		pixGetDimensions(pixs, &w, &h, NULL);

		bmf = bmfCreate(NULL, 8);
		pixad = pixaCreate(0);
		for (int grid = 3; grid <= L_MIN(w / 2, h / 2); grid += L_MAX(1, grid / 2)) {
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
				lept_stderr("\nScorefrac: %1.1f, Grid: %d\n", scorefract, grid);

				/* Show the histogram of gray values and the split location */
				pixSplitDistributionFgBg(pixg, scorefract, 1,
					&thresh, &fgval, &bgval, &pixp);

				snprintf(textstr, sizeof(textstr),
					"histogram: frac=%1.1f thresh=%d fgval=%d bgval=%d",
					scorefract, thresh, fgval, bgval);
				pixSetText(pixp, textstr);

				lept_stderr("thresh = %d, fgval = %d, bgval = %d\n",
					thresh, fgval, bgval);
				pixaAddPix(pixa1, pixs, L_COPY);
				pixaAddPix(pixa1, pixg, L_COPY);
				pixaAddPix(pixa1, pixp, L_INSERT);

				for (int j = 0; j <= 3; j++) {

					/* Get a 1 bpp version; use a single tile */
					pixOtsuAdaptiveThreshold(pixg, sx, sy, j, j, scorefract,
						&pixth, &pixb);

					snprintf(textstr, sizeof(textstr),
						"OtsuAdaptiveThreshold: frac=%1.1f sx=%d sy=%d smooth=%d",
						scorefract, sx, sy, j);
					pixSetText(pixb, textstr);

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

					snprintf(textstr, sizeof(textstr),
						"OtsuAdaptiveThreshold %dx%d THRESHOLDS\n @ frac=%1.1f sx=%d sy=%d smooth=%d scale=%.1fx%.1f",
						thw, thh, scorefract, sx, sy, j, scalex, scaley);
					pixSetText(pix2, textstr);

					pixaAddPix(pixa1, pix2, L_INSERT);

					/* improved version of the API: */

					/* Get a 1 bpp version; use a single tile */
					pixOtsuAdaptiveThreshold2(pixg, sx, sy, j, j, scorefract,
						&pixth, &pixb);

					snprintf(textstr, sizeof(textstr),
						"OtsuAdaptiveThreshold2: frac=%1.1f sx=%d sy=%d smooth=%d",
						scorefract, sx, sy, j);
					pixSetText(pixb, textstr);

					pixaAddPix(pixa1, pixg, L_COPY);
					pixaAddPix(pixa1, pixb, L_INSERT);

					// sampled-scaling of the threshold 'image' to the same size as the source image, so one can properly inspect the cell threshold colors:
					pixGetDimensions(pixth, &thw, &thh, NULL);
					scalex = (l_float32)w / (l_float32)thw;
					scaley = (l_float32)h / (l_float32)thh;

					pix2 = pixScaleBySamplingWithShift(pixth, scalex, scaley, 0, 0);
					pixDestroy(&pixth);

					snprintf(textstr, sizeof(textstr),
						"OtsuAdaptiveThreshold2 %dx%d THRESHOLDS\n @ frac=%1.1f sx=%d sy=%d smooth=%d scale=%.1fx%.1f",
						thw, thh, scorefract, sx, sy, j, scalex, scaley);
					pixSetText(pix2, textstr);

					pixaAddPix(pixa1, pix2, L_INSERT);
				}

				/* Join these together and add some text */
				pix1 = pixaDisplayTiledInColumnsWithText(pixa1, 3, 1.0, 20, 2, 6, 0x0f066700);

				snprintf(textstr, sizeof(textstr),
					"Scorefract = %1.1f\nGrid: %d x %d (cell #: %d x %d)\nH x W: %d x %d\nThresh = %d (%s)", scorefract, sx, sy, grid, grid, pixGetHeight(pixg), pixGetWidth(pixg), thresh, filepath);
				pix2 = pixAddSingleTextblock(pix1, bmf, textstr, 0x06670f00,
					L_ADD_BELOW, NULL);

				/* Save and display the result */
				pixaAddPix(pixad, pix2, L_INSERT);
				const char *pixpath = leptDebugGenFilepath("ScoreFrac-%03d.png", (int)i);
				pixWrite(pixpath, pix2, IFF_PNG);
#if 01
				pixDisplayWithTitle(pix2, 100, 100, "Split distribution in FG/BG");
#endif
				pixDestroy(&pix1);
				pixaDestroy(&pixa1);
			}

			prev_sx = sx;
			prev_sy = sy;
		}

		const char* pdfpath = leptDebugGenFilepath("result.pdf");
		lept_stderr("Writing to: %s\n", pdfpath);
		pixaConvertToPdf(pixad, 75, 1.0, 0, 0, "Otsu thresholding", pdfpath);
		bmfDestroy(&bmf);
		pixDestroy(&pixs);
		pixDestroy(&pixg);
		pixaDestroy(&pixad);

		LEPT_FREE(filepath);
	}

	leptDebugPopStepLevel();

	return regTestCleanup(rp);
}
