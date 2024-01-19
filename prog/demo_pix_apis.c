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

#include "allheaders.h"
#include "demo_settings.h"

#include "monolithic_examples.h"


static const char *fnames[] = {DEMOPATH("lyra.005.jpg"), DEMOPATH("lyra.036.jpg")};


#if defined(BUILD_MONOLITHIC)
#define main   lept_demo_pix_apis_main
#endif

int main(int    argc,
	const char **argv)
{
	l_int32       i, pageno, w, h, left, right;
	BOX          *box1, *box2;
	NUMA         *na1, *nar, *naro, *narl, *nart, *nai, *naio, *nait;
	PIX          *pixs, *pixr, *pixg, *pixgi, *pixd, *pix1, *pix2, *pix3, *pix4;
	PIXA         *pixa1, *pixa2;
	L_REGPARAMS  *rp;

	if (regTestSetup(argc, argv, &rp))
		return 1;

	lept_mkdir("lept/crop");

	for (i = 0; i < 2; i++) {
		pageno = extractNumberFromFilename(fnames[i], 5, 0);
		lept_stderr("Page %d\n", pageno);
		pixs = pixRead(fnames[i]);

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


