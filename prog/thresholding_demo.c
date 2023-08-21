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
 * threshnorm__reg.c
 *
 *      Regression test for adaptive threshold normalization.
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <assert.h>
#include "allheaders.h"
#include "demo_settings.h"

#include "monolithic_examples.h"



#define HISTOGRAM_SIZE   256 // The size of a histogram of pixel values.


static PIX*
pixNLBin(PIX* pixs, l_ok adaptive);
static PIX*
OtsuThresholdRectToPix(PIX* pix);
static int
CalcOtsuThreshold(PIX* pix, int thresholds[4], int hi_values[4]);
static void
HistogramRect(PIX* pix, int channel, int histogram[HISTOGRAM_SIZE]);
static int
OtsuStats(const int histogram[HISTOGRAM_SIZE], int* H_out, int* omega0_out);
static PIX*
ThresholdRectToPix(PIX* pix, int num_channels, int thresholds[4], const int hi_values[4]);
static l_ok
OtsuThreshold(PIX* pixs, float tile_size, float smooth_size, float score_fraction, PIX** pixd, PIX** pixthres, PIX** pixgrey);
static const char*
mk_dst_filename(const char* name);



// Clip a numeric value to the interval [lower_bound, upper_bound].
static inline int ClipToRange(const int x, const int lower_bound, const int upper_bound) {
	if (x < lower_bound) {
		return lower_bound;
	}
	if (x > upper_bound) {
		return upper_bound;
	}
	return x;
}



static l_ok
IsPixBinary(PIX* pix)
{
	int d = pixGetDepth(pix);
	return d == 1;
}

static l_ok
IsPixGreyscale(PIX* pix)
{
	int d = pixGetDepth(pix);
	PIXCMAP *cmap = pixGetColormap(pix);
	return d == 8 && cmap == NULL;
}


 // Get a clone/copy of the source image, reduced to greyscale,
 // and at the same resolution as the output binary.
 // The returned Pix must be pixDestroyed.
static PIX*
GetPixRectGrey(PIX* pix)
{
	PIX* result = NULL;
	//result = pixConvertRGBToLuminance(pix);
	result = pixConvertTo8(pix, 0);
	return result;
}


 /*!
  * \brief   pixNLNorm2() - Non-linear contrast normalization
  *
  * \param[in]    pixs          8 or 32 bpp
  * \param[out]   ptresh        l_int32 global threshold value
  * \return       pixd          8 bpp grayscale, or NULL on error
  *
  * <pre>
  * Notes:
  *      (1) This composite operation is good for adaptively removing
  *          dark background. Adaption of Thomas Breuel's nlbin version
  *          from ocropus.
  *      (2) A good thresholder together NLNorm is WAN
  * </pre>
  */
static PIX*
pixNLNorm2(PIX* pixs, int* pthresh) {
	l_int32 d, thresh, w1, h1, w2, h2, fgval, bgval;
	//l_uint32 black_val, white_val;
	l_float32 factor, threshpos, avefg, avebg;
	PIX* pixg, * pixd, * pixd2;
	BOX* pixbox;
	NUMA* na;

	if (!pixs || (d = pixGetDepth(pixs)) < 8) {
		return (PIX*)ERROR_PTR("pixs undefined or d < 8 bpp", __func__, NULL);
	}
	if (d == 32) {
#if 1
		// ITU-R 601-2 luma
		pixg = pixConvertRGBToGray(pixs, 0.299, 0.587, 0.114);
#else
		// Legacy converting
		pixg = pixConvertRGBToGray(pixs, 0.3, 0.4, 0.3);
#endif
	}
	else {
		pixg = pixConvertTo8(pixs, 0);
	}

	/// Normalize contrast
	//  pixGetBlackOrWhiteVal(pixg, L_GET_BLACK_VAL, &black_val);
	//  if (black_val>0) pixAddConstantGray(pixg, -1 * black_val);
	//  pixGetBlackOrWhiteVal(pixg, L_GET_WHITE_VAL, &white_val);
	//  if (white_val<255) pixMultConstantGray(pixg, (255. / white_val));
	pixd = pixMaxDynamicRange(pixg, L_LINEAR_SCALE);
	if (01) pixWrite(mk_dst_filename("NLNorm2-max-dyn-range.png"), pixd, IFF_PNG);
	pixDestroy(&pixg);
	pixg = pixCopy(NULL, pixd);
	pixDestroy(&pixd);

	/// Calculate flat version
	pixGetDimensions(pixg, &w1, &h1, NULL);
	pixd = pixScaleGeneral(pixg, 0.5, 0.5, 0.0, 0);
	pixd2 = pixRankFilter(pixd, 20, 2, 0.8);
	if (01) pixWrite(mk_dst_filename("NLNorm2-rank-filterA.png"), pixd2, IFF_PNG);
	pixDestroy(&pixd);
	pixd = pixRankFilter(pixd2, 2, 20, 0.8);
	if (01) pixWrite(mk_dst_filename("NLNorm2-rank-filterB.png"), pixd, IFF_PNG);
	pixDestroy(&pixd2);
	pixGetDimensions(pixd, &w2, &h2, NULL);
	pixd2 = pixScaleGrayLI(pixd, (l_float32)w1 / (l_float32)w2,
		(l_float32)h1 / (l_float32)h2);
	if (01) pixWrite(mk_dst_filename("NLNorm2-scale-grey-LI.png"), pixd2, IFF_PNG);
	pixDestroy(&pixd);
	pixInvert(pixd2, pixd2);
	if (01) pixWrite(mk_dst_filename("NLNorm2-invert.png"), pixd2, IFF_PNG);
	pixAddGray(pixg, pixg, pixd2);
	if (01) pixWrite(mk_dst_filename("NLNorm2-add-grey.png"), pixg, IFF_PNG);
	pixDestroy(&pixd2);

	/// Local contrast enhancement
	//  Ignore a border of 10% and get a mean threshold,
	//  background and foreground value
	pixbox = boxCreate(w1 * 0.1, h1 * 0.1, w1 * 0.9, h1 * 0.9);
	na = pixGetGrayHistogramInRect(pixg, pixbox, 1);
	numaSplitDistribution(na, 0.1, &thresh, &avefg, &avebg, NULL, NULL, NULL);
	boxDestroy(&pixbox);
	numaDestroy(&na);

	/// Subtract by a foreground value and multiply by factor to
	//  set a background value to 255
	fgval = (l_int32)(avefg + 0.5);
	bgval = (l_int32)(avebg + 0.5);
	threshpos = (l_float32)(thresh - fgval) / (bgval - fgval);
	// Todo: fgval or fgval + slightly offset
	fgval = fgval; // + (l_int32) ((thresh - fgval)*.25);
	bgval = bgval +
		L_MIN((l_int32)((bgval - thresh) * .5), (255 - bgval));
	factor = 255. / (bgval - fgval);
	if (pthresh) {
		*pthresh = (l_int32)threshpos * factor - threshpos * .1;
	}
	pixAddConstantGray(pixg, -1 * fgval);
	if (01) pixWrite(mk_dst_filename("NLNorm2-add-grey-const.png"), pixg, IFF_PNG);
	pixMultConstantGray(pixg, factor);
	if (01) pixWrite(mk_dst_filename("NLNorm2-mult-grey-const.png"), pixg, IFF_PNG);

	return pixg;
}

/*----------------------------------------------------------------------*
 *                  Non-linear contrast normalization                   *
 *----------------------------------------------------------------------*/
 /*!
  * \brief   pixNLNorm1() - Non-linear contrast normalization
  *
  * \param[in]    pixs          8 or 32 bpp
  * \param[out]   ptresh        l_int32 global threshold value
  * \param[out]   pfgval        l_int32 global foreground value
  * \param[out]   pbgval        l_int32 global background value
  * \return  pixd    8 bpp grayscale, or NULL on error
  *
  * <pre>
  * Notes:
  *      (1) This composite operation is good for adaptively removing
  *          dark background. Adaption of Thomas Breuel's nlbin version from ocropus.
  * </pre>
  */
static PIX*
pixNLNorm1(PIX* pixs, int* pthresh, int* pfgval, int* pbgval)
{
	l_int32 d, fgval, bgval, thresh, w1, h1, w2, h2;
	l_float32 factor;
	PIX* pixg, * pixd;

	if (!pixs || (d = pixGetDepth(pixs)) < 8)
		return (PIX*)ERROR_PTR("pixs undefined or d < 8 bpp", __func__, NULL);
	if (d == 32)
		pixg = pixConvertRGBToGray(pixs, 0.3, 0.4, 0.3);
	else
		pixg = pixConvertTo8(pixs, 0);

	/* Normalize contrast */
	pixd = pixMaxDynamicRange(pixg, L_LINEAR_SCALE);

	/* Calculate flat version */
	pixGetDimensions(pixd, &w1, &h1, NULL);
	pixd = pixScaleSmooth(pixd, 0.5, 0.5);
	pixd = pixRankFilter(pixd, 2, 20, 0.8);
	pixd = pixRankFilter(pixd, 20, 2, 0.8);
	pixGetDimensions(pixd, &w2, &h2, NULL);
	pixd = pixScaleGrayLI(pixd, (l_float32)w1 / (l_float32)w2, (l_float32)h1 / (l_float32)h2);
	pixInvert(pixd, pixd);
	pixg = pixAddGray(NULL, pixg, pixd);
	pixDestroy(&pixd);

	/* Local contrast enhancement */
	pixSplitDistributionFgBg(pixg, 0.1, 2, &thresh, &fgval, &bgval, NULL);
	if (pthresh)
		*pthresh = thresh;
	if (pfgval)
		*pfgval = fgval;
	if (pbgval)
		*pbgval = bgval;
	fgval = fgval + ((thresh - fgval) * 0.25);
	if (fgval < 0)
		fgval = 0;
	pixAddConstantGray(pixg, -1 * fgval);
	factor = 255.0f / (bgval - fgval);
	pixMultConstantGray(pixg, factor);
	pixd = pixGammaTRC(NULL, pixg, 1.0, 0, bgval - ((bgval - thresh) * 0.5));
	pixDestroy(&pixg);

	return pixd;
}





// Get a clone/copy of the source image, reduced to normalized greyscale.
// The returned Pix must be pixDestroyed.
static PIX *
GetPixNormRectGrey(PIX *pix) {
	return pixNLNorm2(pix, NULL);
}



static l_ok
OtsuThreshold(PIX* pixs, float tile_size, float smooth_size, float score_fraction, PIX** pixd, PIX** pixthres, PIX** pixgrey)
{
	PIX* pixg = GetPixRectGrey(pixs);
	if (pixgrey)
		*pixgrey = pixg;

	l_int32 threshold_val = 0;

	l_int32 w, h;
	pixGetDimensions(pixs, &w, &h, NULL);

	{
		l_int32 w2, h2;
		pixGetDimensions(pixg, &w2, &h2, NULL);
		assert(w2 == w);
		assert(h2 == h);
	}

	printf("image width: %d, height: %d\n", w, h);

	int tilesize = L_MAX(16, tile_size);

	int half_smooth_size = L_MAX(0.0, smooth_size / 2);

	printf("LeptonicaOtsu thresholding: tile size: %d, smooth_size/2: %d, score_fraction: %f\n", tilesize, half_smooth_size, score_fraction);

	l_ok r = pixOtsuAdaptiveThreshold(pixg,
		tilesize, tilesize,
		half_smooth_size, half_smooth_size,
		score_fraction,
		pixthres, pixd);

	// upscale the threshold image to match the source image size
	if (pixthres)
	{
		PIX* pixt = *pixthres;
		l_int32 w2, h2;
		pixGetDimensions(pixt, &w2, &h2, NULL);
		if (w2 != w || h2 != h)
		{
			// we DO NOT want to be confused by the smoothness introduced by regular scaling, so we apply brutal sampled scale then:
			pixt = pixScaleBySamplingWithShift(pixt, tilesize, tilesize, 0.0f, 0.0f);
			pixDestroy(pixthres);
			*pixthres = pixt;

			// now see if we need to extend the right and/or bottom edges: that's how tiling is done in leptonica:
			// the right-most and bottom tiles carry the surplus.
			pixGetDimensions(pixt, &w2, &h2, NULL);
			if (w2 != w || h2 != h)
			{
				pixt = pixExtendByReplication(pixt, w - w2, h - h2);
				pixDestroy(pixthres);
				*pixthres = pixt;
			}
		}
	}

	if (!pixgrey)
		pixDestroy(&pixg);

	return r;
}





static l_ok
SauvolaThreshold(PIX* pixs, float window_size, float kfactor, float score_fraction, PIX** pixd, PIX** pixthres, PIX** pixgrey)
{
	PIX* pixg = GetPixRectGrey(pixs);
	if (pixgrey)
		*pixgrey = pixg;

	l_int32 threshold_val = 0;

	l_int32 w, h;
	pixGetDimensions(pixs, &w, &h, NULL);

	{
		l_int32 w2, h2;
		pixGetDimensions(pixg, &w2, &h2, NULL);
		assert(w2 == w);
		assert(h2 == h);
	}

	printf("image width: %d, height: %d\n", w, h);

	window_size = L_MIN(w < h ? w - 3 : h - 3, window_size);
	int half_window_size = L_MAX(2, window_size / 2);

	// factor for image division into tiles; >= 1
	// tiles size will be approx. 250 x 250 pixels
	float nx = L_MAX(1, (w + 125.0f) / 250);
	float ny = L_MAX(1, (h + 125.0f) / 250);
	float xrat = w / nx;
	float yrat = h / ny;
	if (xrat < half_window_size + 2) {
		nx = w / (half_window_size + 2);
	}
	if (yrat < half_window_size + 2) {
		ny = h / (half_window_size + 2);
	}

	assert(w >= 2 * half_window_size + 3);
	assert(h >= 2 * half_window_size + 3);

	int nxi = nx;
	int nyi = ny;

	kfactor = L_MAX(0.0, kfactor);

	printf("window size/2: %d, kfactor: %f, nx: %d, ny: %d\n", half_window_size, kfactor, nxi, nyi);

	l_ok r = pixSauvolaBinarizeTiled(pixg, half_window_size, kfactor, nxi, nyi, pixthres, pixd);

	// upscale the threshold image to match the source image size
	if (pixthres)
	{
		PIX* pixt = *pixthres;
		l_int32 w2, h2;
		pixGetDimensions(pixt, &w2, &h2, NULL);
		if (w2 != w || h2 != h)
		{
			// we DO NOT want to be confused by the smoothness introduced by regular scaling, so we apply brutal sampled scale then:
			pixt = pixScaleBySamplingWithShift(pixt, w * 1.0f / w2, h * 1.0f / h2, 0.0f, 0.0f);
			pixDestroy(pixthres);
			*pixthres = pixt;
		}
	}

	if (!pixgrey)
		pixDestroy(&pixg);

	return r;
}






static l_ok
OtsuOnNormalizedBackground(PIX* pixs, PIX** pixd, PIX** pixgrey)
{
	PIX* pixg = GetPixRectGrey(pixs);
	if (pixgrey)
		*pixgrey = pixg;

	l_int32 threshold_val = 0;

	l_int32 w, h;
	pixGetDimensions(pixs, &w, &h, NULL);

	{
		l_int32 w2, h2;
		pixGetDimensions(pixg, &w2, &h2, NULL);
		assert(w2 == w);
		assert(h2 == h);
	}

	printf("image width: %d, height: %d\n", w, h);

	*pixd = pixOtsuThreshOnBackgroundNorm(pixg, NULL, 10, 15, 100, 50, 255, 2, 2, 0.1f, &threshold_val);

	if (!pixgrey)
		pixDestroy(&pixg);

	return !*pixd;
}





static l_ok
MaskingAndOtsuOnNormalizedBackground(PIX* pixs, PIX** pixd, PIX** pixgrey)
{
	PIX* pixg = GetPixRectGrey(pixs);
	if (pixgrey)
		*pixgrey = pixg;

	l_int32 threshold_val = 0;

	l_int32 w, h;
	pixGetDimensions(pixs, &w, &h, NULL);

	{
		l_int32 w2, h2;
		pixGetDimensions(pixg, &w2, &h2, NULL);
		assert(w2 == w);
		assert(h2 == h);
	}

	printf("image width: %d, height: %d\n", w, h);

	*pixd = pixMaskedThreshOnBackgroundNorm(pixg, NULL, 10, 15, 100, 50, 2, 2, 0.1f, &threshold_val);

	if (!pixgrey)
		pixDestroy(&pixg);

	return !*pixd;
}



static l_ok
NlBinThresholding(PIX* pixs, int adaptive, PIX** pixd, PIX** pixgrey)
{
	PIX* pixg = GetPixRectGrey(pixs);
	if (pixgrey)
		*pixgrey = pixg;

	l_int32 threshold_val = 0;

	l_int32 w, h;
	pixGetDimensions(pixs, &w, &h, NULL);

	{
		l_int32 w2, h2;
		pixGetDimensions(pixg, &w2, &h2, NULL);
		assert(w2 == w);
		assert(h2 == h);
	}

	printf("image width: %d, height: %d\n", w, h);

	*pixd = pixNLBin(pixg, !!adaptive);

	if (!pixgrey)
		pixDestroy(&pixg);

	return !*pixd;
}








 /*!
  * \brief   pixNLBin() - Non-linear contrast normalization and thresholding
  *
  * \param[in]    pixs          8 or 32 bpp
  * \oaram[in]    adaptive      bool if set to true it uses adaptive thresholding
  *                             recommended for images, which contain dark and light text
  *                             at the same time (it doubles the processing time)
  * \return  pixd    1 bpp thresholded image, or NULL on error
  *
  * <pre>
  * Notes:
  *      (1) This composite operation is good for adaptively removing
  *          dark background. Adaption of Thomas Breuel's nlbin version from ocropus.
  *      (2) The threshold for the binarization uses an
  *          Sauvola adaptive thresholding.
  * </pre>
  */
static PIX*
pixNLBin(PIX* pixs, l_ok adaptive)
{
	int thresh;
	int fgval, bgval;
	int fgclip, bgclip;
	PIX* pixb;

	pixb = pixNLNorm1(pixs, &thresh, &fgval, &bgval);
	if (!pixb)
		return (PIX*)ERROR_PTR("invalid normalization result", __func__, NULL);

	/* Binarize */

	if (adaptive) {
		l_int32    w, h, nx, ny;
		pixGetDimensions(pixb, &w, &h, NULL);
		nx = L_MAX(1, (w + 64) / 128);
		ny = L_MAX(1, (h + 64) / 128);
		/* whsize needs to be this small to use it also for lineimages for tesseract */
		pixSauvolaBinarizeTiled(pixb, 16, 0.5, nx, ny, NULL, &pixb);
	}
	else {
#if 0  // never *dither* an image to make it binary/monochrome: it comes out ugly and unusable for OCR, at least.
		bgclip = bgval - ((bgval - thresh) * 0.75);
		fgclip = fgval + ((thresh - fgval) * 0.25);
		pixb = pixDitherToBinarySpec(pixb, bgclip, fgclip);
#else
		fgclip = fgval + ((thresh - fgval) * 0.1);
		bgclip = bgval - ((bgval - thresh) * 0.1);
		if (fgclip > thresh) {
			fprintf(stderr, "b0rk!\n");
		}
		pixb = pixThresholdToBinary(pixb, fgclip);  /* for bg and light fg */
#endif
	}

	return pixb;
}







// Threshold the source image as efficiently as possible to the output Pix.
// Caller must use pixDestroy to free the created Pix.
static PIX *
ThresholdToPix(PIX* pix)
{
	PIX* pixd;
	int d = pixGetDepth(pix);
	if (d == 1)
	{
		pixd = pixCopy(NULL, pix);
	}
	else if (pixGetColormap(pix)) {
		PIX* tmp = NULL;
		PIX *without_cmap = pixRemoveColormap(pix, REMOVE_CMAP_BASED_ON_SRC);
		int depth = pixGetDepth(without_cmap);
		if (depth > 1 && depth < 8) {
			tmp = pixConvertTo8(without_cmap, FALSE);
		}
		else {
			tmp = pixCopy(NULL, without_cmap);
		}

		pixd = OtsuThresholdRectToPix(tmp);
		pixDestroy(&tmp);
		pixDestroy(&without_cmap);
	}
	else {
		pixd = OtsuThresholdRectToPix(pix);
	}

	return pixd;
}






// Gets a pix that contains an 8 bit threshold value at each pixel. The
// returned pix may be an integer reduction of the binary image such that
// the scale factor may be inferred from the ratio of the sizes, even down
// to the extreme of a 1x1 pixel thresholds image.
// Ideally the 8 bit threshold should be the exact threshold used to generate
// the binary image in ThresholdToPix, but this is not a hard constraint.
// Returns NULL if the input is binary. PixDestroy after use.
static PIX *
GetPixRectThresholds(PIX *pix)
{
	if (IsPixBinary(pix)) {
		return NULL;
	}
	PIX *pix_grey = GetPixRectGrey(pix);
	int w, h;
	pixGetDimensions(pix_grey, &w, &h, NULL);
	int thresholds[4] = { 0 };
	int hi_values[4] = { 0 };
	int num_channels = CalcOtsuThreshold(pix_grey, thresholds, hi_values);
	pixDestroy(&pix_grey);

	PIX *pix_thresholds = pixCreate(w, h, 8);
	int threshold = thresholds[0] >= 0 ? thresholds[0] : 128;
	pixSetAllArbitrary(pix_thresholds, threshold);
	return pix_thresholds;
}





// Get a clone/copy of the source image rectangle.
// The returned Pix must be pixDestroyed.
PIX *GetPixRect(PIX *pix, int x, int y, int w, int h)
{
		// Crop to the given rectangle.
		BOX* box = boxCreate(x, y, w, h);
		PIX *cropped = pixClipRectangle(pix, box, NULL);
		boxDestroy(&box);
		return cropped;
}








// Computes the Otsu threshold(s) for the given image rectangle, making one
// for each channel. Each channel is always one byte per pixel.
// Returns an array of threshold values and an array of hi_values, such
// that a pixel value >threshold[channel] is considered foreground if
// hi_values[channel] is 0 or background if 1. A hi_value of -1 indicates
// that there is no apparent foreground. At least one hi_value will not be -1.
// The return value is the number of channels in the input image, being
// the size of the output thresholds and hi_values arrays.
static int
CalcOtsuThreshold(PIX *pix, int thresholds[4], int hi_values[4])
{
	int num_channels = pixGetDepth(pix) / 8;
	// Of all channels with no good hi_value, keep the best so we can always
	// produce at least one answer.
	int best_hi_value = 1;
	int best_hi_index = 0;
	l_ok any_good_hivalue = FALSE;
	double best_hi_dist = 0.0;
	int w, h;
	pixGetDimensions(pix, &w, &h, NULL);

	// only use opencl if compiled w/ OpenCL and selected device is opencl
#ifdef USE_OPENCL
  // all of channel 0 then all of channel 1...
	std::vector<int> histogramAllChannels(HISTOGRAM_SIZE * num_channels);

	// Calculate Histogram on GPU
	OpenclDevice od;
	if (od.selectedDeviceIsOpenCL() && (num_channels == 1 || num_channels == 4)) {
		od.HistogramRectOCL(pixGetData(src_pix), num_channels, pixGetWpl(src_pix) * 4, 0, 0, w,
			h, HISTOGRAM_SIZE, &histogramAllChannels[0]);

		// Calculate Threshold from Histogram on cpu
		for (int ch = 0; ch < num_channels; ++ch) {
			thresholds[ch] = -1;
			hi_values[ch] = -1;
			int* histogram = &histogramAllChannels[HISTOGRAM_SIZE * ch];
			int H;
			int best_omega_0;
			int best_t = OtsuStats(histogram, &H, &best_omega_0);
			if (best_omega_0 == 0 || best_omega_0 == H) {
				// This channel is empty.
				continue;
			}
			// To be a convincing foreground we must have a small fraction of H
			// or to be a convincing background we must have a large fraction of H.
			// In between we assume this channel contains no thresholding information.
			int hi_value = best_omega_0 < H * 0.5;
			thresholds[ch] = best_t;
			if (best_omega_0 > H * 0.75) {
				any_good_hivalue = true;
				hi_values[ch] = 0;
			}
			else if (best_omega_0 < H * 0.25) {
				any_good_hivalue = true;
				hi_values[ch] = 1;
			}
			else {
				// In case all channels are like this, keep the best of the bad lot.
				double hi_dist = hi_value ? (H - best_omega_0) : best_omega_0;
				if (hi_dist > best_hi_dist) {
					best_hi_dist = hi_dist;
					best_hi_value = hi_value;
					best_hi_index = ch;
				}
			}
		}
	}
	else {
#endif
		for (int ch = 0; ch < num_channels; ++ch) {
			thresholds[ch] = -1;
			hi_values[ch] = -1;
			// Compute the histogram of the image rectangle.
			int histogram[HISTOGRAM_SIZE];
			HistogramRect(pix, ch, histogram);
			int H;
			int best_omega_0;
			int best_t = OtsuStats(histogram, &H, &best_omega_0);
			if (best_omega_0 == 0 || best_omega_0 == H) {
				// This channel is empty.
				continue;
			}
			// To be a convincing foreground we must have a small fraction of H
			// or to be a convincing background we must have a large fraction of H.
			// In between we assume this channel contains no thresholding information.
			int hi_value = (best_omega_0 < H * 0.5);
			thresholds[ch] = best_t;
			if (best_omega_0 > H * 0.75) {
				any_good_hivalue = TRUE;
				hi_values[ch] = 0;
			}
			else if (best_omega_0 < H * 0.25) {
				any_good_hivalue = TRUE;
				hi_values[ch] = 1;
			}
			else {
				// In case all channels are like this, keep the best of the bad lot.
				double hi_dist = hi_value ? (H - best_omega_0) : best_omega_0;
				if (hi_dist > best_hi_dist) {
					best_hi_dist = hi_dist;
					best_hi_value = hi_value;
					best_hi_index = ch;
				}
			}
		}
#ifdef USE_OPENCL
	}
#endif // USE_OPENCL

	if (!any_good_hivalue) {
		// Use the best of the ones that were not good enough.
		hi_values[best_hi_index] = best_hi_value;
	}
	return num_channels;
}




// Computes the histogram for the given image rectangle, and the given
// single channel. Each channel is always one byte per pixel.
// Histogram is always a HISTOGRAM_SIZE(256) element array to count
// occurrences of each pixel value.
static void
HistogramRect(PIX *pix, int channel, int histogram[HISTOGRAM_SIZE])
{
	int num_channels = pixGetDepth(pix) / 8;
	channel = ClipToRange(channel, 0, num_channels - 1);
	int w, h;
	pixGetDimensions(pix, &w, &h, NULL);
	memset(histogram, 0, sizeof(*histogram) * HISTOGRAM_SIZE);
	int src_wpl = pixGetWpl(pix);
	l_uint32* srcdata = pixGetData(pix);
	for (int y = 0; y < h; ++y) {
		const l_uint32* linedata = srcdata + y * src_wpl;
		for (int x = 0; x < w; ++x) {
			int pixel = GET_DATA_BYTE(linedata, x * num_channels + channel);
			++histogram[pixel];
		}
	}
}





// Computes the Otsu threshold(s) for the given histogram.
// Also returns H = total count in histogram, and
// omega0 = count of histogram below threshold.
static int
OtsuStats(const int histogram[HISTOGRAM_SIZE], int* H_out, int* omega0_out)
{
	int H = 0;
	double mu_T = 0.0;
	for (int i = 0; i < HISTOGRAM_SIZE; ++i) {
		H += histogram[i];
		mu_T += (double)(i) * histogram[i];
	}

	// Now maximize sig_sq_B over t.
	// http://www.ctie.monash.edu.au/hargreave/Cornall_Terry_328.pdf
	int best_t = -1;
	int omega_0, omega_1;
	int best_omega_0 = 0;
	double best_sig_sq_B = 0.0;
	double mu_0, mu_1, mu_t;
	omega_0 = 0;
	mu_t = 0.0;
	for (int t = 0; t < HISTOGRAM_SIZE - 1; ++t) {
		omega_0 += histogram[t];
		mu_t += (double)t * histogram[t];
		if (omega_0 == 0) {
			continue;
		}
		omega_1 = H - omega_0;
		if (omega_1 == 0) {
			break;
		}
		mu_0 = mu_t / omega_0;
		mu_1 = (mu_T - mu_t) / omega_1;
		double sig_sq_B = mu_1 - mu_0;
		sig_sq_B *= sig_sq_B * omega_0 * omega_1;
		if (best_t < 0 || sig_sq_B > best_sig_sq_B) {
			best_sig_sq_B = sig_sq_B;
			best_t = t;
			best_omega_0 = omega_0;
		}
	}
	if (H_out != NULL) {
		*H_out = H;
	}
	if (omega0_out != NULL) {
		*omega0_out = best_omega_0;
	}
	return best_t;
}




















// Otsu thresholds the rectangle.
static PIX*
OtsuThresholdRectToPix(PIX *pix)
{
	int thresholds[4];
	int hi_values[4];
	PIX* pixd;

	int num_channels = CalcOtsuThreshold(pix, thresholds, hi_values);
	// only use opencl if compiled w/ OpenCL and selected device is opencl
#ifdef USE_OPENCL
	OpenclDevice od;
	if (num_channels == 4 && od.selectedDeviceIsOpenCL()) {
		od.ThresholdRectToPixOCL((unsigned char*)pixGetData(pix), num_channels,
			pixGetWpl(pix) * 4, &thresholds[0], &hi_values[0], &pixd /*pix_OCL*/,
			h, w, 0, 0);
	}
	else {
#endif
		pixd = ThresholdRectToPix(pix, num_channels, thresholds, hi_values);
#ifdef USE_OPENCL
	}
#endif
	return pixd;
}





/// Threshold the rectangle, using thresholds/hi_values and outputs a pix.
/// NOTE that num_channels is the size of the thresholds and hi_values
// arrays and also the bytes per pixel in src_pix.
static PIX *
ThresholdRectToPix(PIX *pix, int num_channels, int thresholds[4], const int hi_values[4])
{
	int w, h;
	pixGetDimensions(pix, &w, &h, NULL);
	PIX *pixd = pixCreate(w, h, 1);
	l_uint32 * pixdata = pixGetData(pixd);
	int wpl = pixGetWpl(pixd);
	int src_wpl = pixGetWpl(pix);
	l_uint32* srcdata = pixGetData(pix);
	pixSetXRes(pixd, pixGetXRes(pix));
	pixSetYRes(pixd, pixGetYRes(pix));
	for (int y = 0; y < h; ++y) {
		const l_uint32* linedata = srcdata + y * src_wpl;
		l_uint32* pixline = pixdata + y * wpl;
		for (int x = 0; x < w; ++x) {
			l_ok white_result = TRUE;
			for (int ch = 0; ch < num_channels; ++ch) {
				int pixel = GET_DATA_BYTE(linedata, x * num_channels + ch);
				if (hi_values[ch] >= 0 && (pixel > thresholds[ch]) == (hi_values[ch] == 0)) {
					white_result = FALSE;
					break;
				}
			}
			if (white_result) {
				CLEAR_DATA_BIT(pixline, x);
			}
			else {
				SET_DATA_BIT(pixline, x);
			}
		}
	}
	return pixd;
}






//
// produce destination paths in a /tmp/... directory tree, where:
//
// - all output files are NUMBERED in the order in which they are produced,
//   so as to produce an intelligible sequence.
// - the last directory in the generated output path is based on the source file,
//   so as multiple runs with different test files each produce their own
//   output image sequence in separate directories to keep it manageable and
//   intelligible.
//

static char dstdirname[256];

static void register_source_filename_for_dst_path(const char* name)
{
	// rough hash of file path:
	uint32_t hash = 0x003355AA;
	for (int i = 0; name[i]; i++) {
		hash <<= 5;
		uint8_t c = name[i];
		hash += c;
		uint32_t c2 = c;
		hash += c2 << 17;
		hash ^= hash >> 21;
	}

	const char* fn = strrchr(name, '/');
	if (!fn)
		fn = strrchr(name, '\\');
	if (!fn)
		fn = name;
	else
		fn++;
	const char* ext = strrchr(fn, '.');
	size_t len = ext - fn;
	if (len >= sizeof(dstdirname))
		len = sizeof(dstdirname) - 1;

	memcpy(dstdirname, fn, len);
	dstdirname[len] = 0;

	// sanitize:
	int dot_allowed = 0;
	for (int i = 0; dstdirname[i]; i++) {
		int c = dstdirname[i];
		if (isalnum(c)) {
			dot_allowed = 1;
			continue;
		}
		if (strchr("-_.", c) && dot_allowed) {
			dot_allowed = 0;
			continue;
		}
		dot_allowed = 0;
		dstdirname[i] = '_';
	}
	for (int i = strlen(dstdirname) - 1; i >= 0; i--) {
		int c = dstdirname[i];
		if (!strchr("-_.", c)) {
			break;
		}
		dstdirname[i] = 0;
	}

	// append path hash:
	hash &= 0xFFFF;
	size_t offset = strlen(dstdirname);
	if (offset >= sizeof(dstdirname) - 7)
		offset = sizeof(dstdirname) - 7;
	snprintf(dstdirname + offset, 6, "_H%04X", hash);
	dstdirname[sizeof(dstdirname) - 1] = 0;
}


static const char* mk_dst_filename(const char* name)
{
	static char dstpath[1024];
	static int index = 0;

	snprintf(dstpath, sizeof(dstpath), "/tmp/lept/binarization/%s/%03d-%s", dstdirname, index, name);
	index++;
	return dstpath;
}








#if defined(BUILD_MONOLITHIC)
#define main   lept_thresholding_test_main
#endif

int main(int    argc,
         const char **argv)
{
L_REGPARAMS  *rp;
PIX* pix[50] = { NULL };
l_ok ret = 0;
const char* sourcefile = DEMOPATH("Dance.Troupe.jpg");

    if (regTestSetup(argc, argv, &rp))
        return 1;

	if (argc == 3)
	{
		sourcefile = argv[2];
	}

	register_source_filename_for_dst_path(sourcefile);

	{
		char dstpath[256 + sizeof(dstdirname)];
		snprintf(dstpath, sizeof(dstpath), "lept/binarization/%s", dstdirname);
		
		lept_rmdir(dstpath);
		lept_mkdir(dstpath);
	}

	pix[0] = pixRead(sourcefile);
	if (!pix[0])
	{
		ret = 1;
	}
	else
	{
		ret |= pixWrite(mk_dst_filename("orig.png"), pix[0], IFF_PNG);

		// Otsu first; tesseract 'vanilla' behaviour mimic

		if (!IsPixBinary(pix[0])) {
			pix[1] = GetPixRectGrey(pix[0]);
		}
		else {
			pix[1] = pixClone(pix[0]);
		}
		ret |= pixWrite(mk_dst_filename("grey256.png"), pix[1], IFF_PNG);

if (01)
{
		{
			int w, h;
			pixGetDimensions(pix[1], &w, &h, NULL);

			const struct {
				float tile_size;
				float smooth_size;
				float score_fraction;
			} scenarios[] = {
				{ 0.1 * h, 2.0f, 0.1f },
				{ 0.33 * h, 2.0f, 0.1f },
				{ 32, 2.0f, 0.1f },

				{ 0.1 * h, 0.0f, 0.1f },
				{ 0.33 * h, 0.0f, 0.1f },
				{ 32, 0.0f, 0.1f },
			};
			for (int i = 0; i < sizeof(scenarios) / sizeof(scenarios[0]); i++)
			{
				float tile_size = scenarios[i].tile_size;
				float smooth_size = scenarios[i].smooth_size;
				float score_fraction = scenarios[i].score_fraction;

				for (int i = 2; i < sizeof(pix) / sizeof(pix[0]); i++)
					pixDestroy(&pix[i]);

				ret |= OtsuThreshold(pix[1], tile_size, smooth_size, score_fraction, &pix[4], &pix[3], &pix[2]);

				ret |= pixWrite(mk_dst_filename("grey256.png"), pix[2], IFF_PNG);
				ret |= pixWrite(mk_dst_filename("thresholds.png"), pix[3], IFF_PNG);
				ret |= pixWrite(mk_dst_filename("binarized-result.png"), pix[4], IFF_PNG);
			}

			pix[5] = pixAddMirroredBorder(pix[0], w / 2, w / 2, h / 2, h / 2);
			ret |= pixWrite(mk_dst_filename("border-50pct.png"), pix[5], IFF_PNG);

			// add padding border: size must be Otsu smoothing kernel size + 1 as the smoothing --> convolution 'accumulator'
			// pix that will be constructed by the Otsu code internally will have the same size as
			// the provided source pix and we wish to prevent nasty border artifacts due to rounding
			// errors in blockconvLow()'s re-normalization of border rows/columns, which takes
			// (rounded) integer greyscale values from intermediate calculations, thus producing
			// rounding / math accuracy artifacts at the border. Hence it is 'smart' to make sure
			// we have a more-or-less plain, *thick*, border around the actual image before we apply
			// binarization of any kind.
			l_int32 border = 2 * L_MAX(32, 0.33 * h + 0.5);
			pix[6] = pixAddContinuedBorder(pix[0], border, border, border, border);
			ret |= pixWrite(mk_dst_filename("padding-2px.png"), pix[6], IFF_PNG);
		}

		if (!IsPixBinary(pix[6])) {
			pix[7] = GetPixRectGrey(pix[6]);
		}
		else {
			pix[7] = pixClone(pix[6]);
		}
		ret |= pixWrite(mk_dst_filename("grey256.png"), pix[7], IFF_PNG);

		{
			int w, h;
			pixGetDimensions(pix[7], &w, &h, NULL);

			const struct {
				float tile_size;
				float smooth_size;
				float score_fraction;
			} scenarios[] = {
				{ 0.1 * h, 2.0f, 0.1f },
				{ 0.33 * h, 2.0f, 0.1f },
				{ 32, 2.0f, 0.1f },

				{ 0.1 * h, 0.0f, 0.1f },
				{ 0.33 * h, 0.0f, 0.1f },
				{ 32, 0.0f, 0.1f },
			};
			for (int i = 0; i < sizeof(scenarios) / sizeof(scenarios[0]); i++)
			{
				float tile_size = scenarios[i].tile_size;
				float smooth_size = scenarios[i].smooth_size;
				float score_fraction = scenarios[i].score_fraction;

				for (int i = 8; i < sizeof(pix) / sizeof(pix[0]); i++)
					pixDestroy(&pix[i]);

				ret |= OtsuThreshold(pix[7], tile_size, smooth_size, score_fraction, &pix[10], &pix[9], &pix[8]);

				ret |= pixWrite(mk_dst_filename("grey256.png"), pix[8], IFF_PNG);
				ret |= pixWrite(mk_dst_filename("thresholds.png"), pix[9], IFF_PNG);
				ret |= pixWrite(mk_dst_filename("binarized-result.png"), pix[10], IFF_PNG);
			}
		}

		//--------------------------------------------------------

		// Non-linear contrast normalization
		{
			int thresh;
			pix[11] = pixNLNorm2(pix[1], &thresh);
			ret |= pixWrite(mk_dst_filename("NLNorm2.png"), pix[11], IFF_PNG);
		}

		// Non-linear contrast normalization
		{
			int thresh, fgval, bgval;
			pix[12] = pixNLNorm1(pix[1], &thresh, &fgval, &bgval);
			ret |= pixWrite(mk_dst_filename("NLNorm1.png"), pix[12], IFF_PNG);
		}

		// source image, reduced to normalized greyscale.
		{
			pix[13] = GetPixNormRectGrey(pix[1]);
			ret |= pixWrite(mk_dst_filename("normalized-grey.png"), pix[13], IFF_PNG);
		}

		// tesseract
		{
			const struct {
				float window_size;
				float kfactor;
				float score_fraction;
			} scenarios[] = {
				{ 0.33, 0.34, 0.1 },  // tesseract
				{ 0.1, 0.1, 0.1 },
				{ 0.5, 0.1, 0.1 },
				{ 0.2, 0.1, 0.1 },
				{ 0.2, 0.5, 0.1 },
				{ 0.01, 0.3, 0.1 },
				{ 0.01, 0.01, 0.1 },
				{ 1.0, 1.0, 0.1 },
				{ 0.3, 0.01, 0.1 },
				{ 0.3, 0.1, 0.1 },
				{ 0.3, 1.0, 0.1 },
				{ 0.3, 3.0, 0.1 },
				{ 0.1, 3.0, 0.1 },
				{ 0.2, 3.0, 0.1 },
				{ 0.5, 3.0, 0.1 },
			};
			for (int i = 0; i < sizeof(scenarios) / sizeof(scenarios[0]); i++)
			{
				float window_size = scenarios[i].window_size;
				float kfactor = scenarios[i].kfactor;
				float score_fraction = scenarios[i].score_fraction;

				for (int i = 15; i < sizeof(pix) / sizeof(pix[0]); i++)
					pixDestroy(&pix[i]);

				if (!SauvolaThreshold(pix[1], window_size, kfactor, score_fraction, &pix[17], &pix[16], &pix[15])) {
					ret |= pixWrite(mk_dst_filename("sauvola-grey.png"), pix[15], IFF_PNG);
					ret |= pixWrite(mk_dst_filename("sauvola-threshold.png"), pix[16], IFF_PNG);
					ret |= pixWrite(mk_dst_filename("sauvola-output.png"), pix[17], IFF_PNG);
				}
			}
		}

		// leptonica
		{
			// Contrast normalization followed by Sauvola binarization
			l_int32 mindiff = 1;
			pix[18] = pixSauvolaOnContrastNorm(pix[1], mindiff, &pix[19], &pix[20]);
			ret |= pixWrite(mk_dst_filename("CNorm-sauvola-contrast-norm.png"), pix[19], IFF_PNG);
			ret |= pixWrite(mk_dst_filename("CNorm-sauvola-threshold.png"), pix[20], IFF_PNG);
			ret |= pixWrite(mk_dst_filename("CNorm-sauvola-output.png"), pix[18], IFF_PNG);
		}

		{
			if (!OtsuOnNormalizedBackground(pix[1], &pix[21], &pix[22])) {
				ret |= pixWrite(mk_dst_filename("otsu-normbg-grey.png"), pix[22], IFF_PNG);
				ret |= pixWrite(mk_dst_filename("otsu-normbg-output.png"), pix[21], IFF_PNG);
			}
		}

		{
			if (!MaskingAndOtsuOnNormalizedBackground(pix[1], &pix[23], &pix[24])) {
				ret |= pixWrite(mk_dst_filename("otsu-masked-normbg-grey.png"), pix[24], IFF_PNG);
				ret |= pixWrite(mk_dst_filename("otsu-masked-normbg-output.png"), pix[23], IFF_PNG);
			}
		}

		{
			int adaptive = 1;
			if (!NlBinThresholding(pix[1], adaptive, &pix[25], &pix[26])) {
				ret |= pixWrite(mk_dst_filename("nlbin-th-grey.png"), pix[26], IFF_PNG);
				ret |= pixWrite(mk_dst_filename("nlbin-th-output.png"), pix[25], IFF_PNG);
			}
		}

		{
			int adaptive = 1;
			pix[27] = pixNLBin(pix[1], adaptive);
			ret |= pixWrite(mk_dst_filename("nlbin-output.png"), pix[27], IFF_PNG);
		}


		{
			for (int i = 25; i < sizeof(pix) / sizeof(pix[0]); i++)
				pixDestroy(&pix[i]);

			int adaptive = 0;
			if (!NlBinThresholding(pix[1], adaptive, &pix[25], &pix[26])) {
				ret |= pixWrite(mk_dst_filename("nlbin-th-grey.png"), pix[26], IFF_PNG);
				ret |= pixWrite(mk_dst_filename("nlbin-th-output.png"), pix[25], IFF_PNG);
			}
		}

		{
			int adaptive = 0;
			pix[27] = pixNLBin(pix[1], adaptive);
			ret |= pixWrite(mk_dst_filename("nlbin-output.png"), pix[27], IFF_PNG);
		}

		{
			pix[28] = ThresholdToPix(pix[1]);
			ret |= pixWrite(mk_dst_filename("threshold-output.png"), pix[28], IFF_PNG);
		}

		{
			pix[29] = GetPixRectThresholds(pix[1]);
			ret |= pixWrite(mk_dst_filename("get-rect-thresholds-output.png"), pix[29], IFF_PNG);
		}

		{
			int w, h;
			pixGetDimensions(pix[1], &w, &h, NULL);
			pix[30] = GetPixRect(pix[1], w / 4, h / 4, w / 2, h / 2);
			ret |= pixWrite(mk_dst_filename("cropped-by-half.png"), pix[30], IFF_PNG);
		}

		{
			pix[31] = OtsuThresholdRectToPix(pix[1]);
			ret |= pixWrite(mk_dst_filename("otsu-threshold-rect.png"), pix[31], IFF_PNG);
		}

		// unwrapping OtsuThresholdRectToPix(), possibly tweaking the process...
		{
			int num_channels = 1;

			// Compute the histogram of the image rectangle.
			int histogram[HISTOGRAM_SIZE];
			HistogramRect(pix[1], num_channels, histogram);
			int H;
			int best_omega_0;
			int best_t = OtsuStats(histogram, &H, &best_omega_0);
			if (best_omega_0 == 0 || best_omega_0 == H) {
				// This channel is empty.
			}
			// To be a convincing foreground we must have a small fraction of H
			// or to be a convincing background we must have a large fraction of H.
			// In between we assume this channel contains no thresholding information.
			int hi_value = (best_omega_0 < H * 0.5);

			int thresholds[4] = { best_t };
			int hi_values[4] = { hi_value };
			pix[32] = ThresholdRectToPix(pix[1], num_channels, thresholds, hi_values);
			ret |= pixWrite(mk_dst_filename("threshold-rect-to-pix.png"), pix[32], IFF_PNG);
		}

		// targetting pixAutoPhotoinvert() ...
		{
			int num_channels = 1;

			// Compute the histogram of the image rectangle.
			int histogram[HISTOGRAM_SIZE];
			HistogramRect(pix[1], num_channels, histogram);
			int H;
			int best_omega_0;
			int best_t = OtsuStats(histogram, &H, &best_omega_0);
			if (best_omega_0 == 0 || best_omega_0 == H) {
				// This channel is empty.
			}

			l_int32 thresh = best_t; // 128
			PIXA* pixa = pixaCreate(10);
			pix[33] = pixAutoPhotoinvert(pix[1], thresh, &pix[34], pixa);
			ret |= pixWrite(mk_dst_filename("photo-invert-mask.png"), pix[34], IFF_PNG);
			int n = pixaGetCount(pixa);
			for (int i = 0; i < n; i++) {
				PIX* p = pixaGetPix(pixa, i, L_CLONE);
				ret |= pixWrite(mk_dst_filename("photo-invert-dbg.png"), p, IFF_PNG);
				pixDestroy(&p);
			}
			pixaDestroy(&pixa);
			ret |= pixWrite(mk_dst_filename("photo-invert-output.png"), pix[33], IFF_PNG);
		}
}

if (01)
{
		// unwrapping pixAutoPhotoinvert() and tweaking the process...
		{
			int num_channels = 1;

			// Compute the histogram of the image rectangle.
			int histogram[HISTOGRAM_SIZE];
			HistogramRect(pix[1], num_channels, histogram);
			int H;
			int best_omega_0;
			int best_t = OtsuStats(histogram, &H, &best_omega_0);
			if (best_omega_0 == 0 || best_omega_0 == H) {
				// This channel is empty.
			}

			l_int32 thresh = best_t; // 128
			l_int32    i, n, empty, x, y, w, h;
			l_float32  fgfract;
			BOX* box1;
			BOXA* boxa1;
			PIXA* pixa = pixaCreate(10);

			pix[36] = pixConvertTo1(pix[1], thresh);
			ret |= pixWrite(mk_dst_filename("photo-invert-to.1-th.128.png"), pix[36], IFF_PNG);

			/* Identify regions for photo-inversion:
				* (1) Start with the halftone mask.
				* (2) Eliminate ordinary text and halftones in the mask.
				* (3) Some regions of inverted text may have been removed in
				*     steps (1) and (2).  Conditionally fill holes in the mask,
				*     but do not fill out to the bounding rect. */
			pix[37] = pixGenerateHalftoneMask(pix[36], &pix[38], NULL, pixa);

			n = pixaGetCount(pixa);
			for (i = 0; i < n; i++) {
				PIX* p = pixaGetPix(pixa, i, L_CLONE);
				ret |= pixWrite(mk_dst_filename("photo-invert-halftone-dbg.png"), p, IFF_PNG);
				pixDestroy(&p);
			}
			pixaDestroy(&pixa);

			ret |= pixWrite(mk_dst_filename("photo-invert-halftone-mask.png"), pix[37], IFF_PNG);
			ret |= pixWrite(mk_dst_filename("photo-invert-halftone-text.png"), pix[38], IFF_PNG);
#if 01  // noise removal not really needed for sample image; keep it minimal anyway. 
			pix[39] = pixMorphSequence(pix[37], "o3.3 + c5.5" /* "o15.15 + c25.25" */, 1);  /* remove noise */
#else
			pix[39] = pixClone(pix[37]);
#endif
			ret |= pixWrite(mk_dst_filename("photo-invert-noise-removal.png"), pix[39], IFF_PNG);
			pix[40] = pixFillHolesToBoundingRect(pix[39], 1, 0.5, 1.0);
			ret |= pixWrite(mk_dst_filename("photo-invert-fill-holes.png"), pix[40], IFF_PNG);
			pixZero(pix[40], &empty);
			if (!empty) {
				/* Examine each component and validate the inversion.
					* Require at least 60% of pixels under each component to be FG. */
				boxa1 = pixConnComp(pix[40], &pixa, 8);

				n = pixaGetCount(pixa);
				for (i = 0; i < n; i++) {
					PIX* p = pixaGetPix(pixa, i, L_CLONE);
					ret |= pixWrite(mk_dst_filename("photo-invert-connected-components-dbg.png"), p, IFF_PNG);
					pixDestroy(&p);
				}
				pixaDestroy(&pixa);

				n = boxaGetCount(boxa1);
				for (i = 0; i < n; i++) {
					box1 = boxaGetBox(boxa1, i, L_COPY);
					pix[41] = pixClipRectangle(pix[36], box1, NULL);
					ret |= pixWrite(mk_dst_filename("photo-invert-clip-rect.png"), pix[41], IFF_PNG);
					pixForegroundFraction(pix[41], &fgfract);
					lept_stderr("fg fraction: %5.3f\n", fgfract);
					boxGetGeometry(box1, &x, &y, &w, &h);
					lept_stderr("bbox #%d: x: %d, y: %d, w: %d, h: %d\n", i, x, y, w, h);
					if (fgfract < 0.6)
					{
						/* erase from the mask */
						pixRasterop(pix[40], x, y, w, h, PIX_CLR, NULL, 0, 0);
						ret |= pixWrite(mk_dst_filename("photo-invert-rasterop-erase.png"), pix[40], IFF_PNG);
					}
					pixDestroy(&pix[41]);
					boxDestroy(&box1);
				}
				boxaDestroy(&boxa1);
				pixZero(pix[40], &empty);
				if (!empty) {
					/* Combine pixels of the photo-inverted pix with the binarized input */
					pix[41] = pixInvert(NULL, pix[36]);
					pix[42] = pixClone(pix[36]);
					ret |= pixWrite(mk_dst_filename("photo-invert-inverted.png"), pix[41], IFF_PNG);
					ret |= pixWrite(mk_dst_filename("photo-invert-inverted-src.png"), pix[42], IFF_PNG);
					pixCombineMasked(pix[42], pix[41], pix[40]);
					ret |= pixWrite(mk_dst_filename("photo-invert-combined-src.png"), pix[41], IFF_PNG);
					ret |= pixWrite(mk_dst_filename("photo-invert-combined-mask.png"), pix[40], IFF_PNG);
					ret |= pixWrite(mk_dst_filename("photo-invert-combined.png"), pix[42], IFF_PNG);

					/* Combine pixels of the photo-inverted pix with the greyscale input */
					pix[43] = pixInvert(NULL, pix[1]);
					pix[44] = pixClone(pix[1]);
					ret |= pixWrite(mk_dst_filename("photo-invert-invert-grey.png"), pix[43], IFF_PNG);
					ret |= pixWrite(mk_dst_filename("photo-invert-invert-src-grey.png"), pix[44], IFF_PNG);
					pixCombineMasked(pix[44], pix[43], pix[40]);
					ret |= pixWrite(mk_dst_filename("photo-invert-combined-src-grey.png"), pix[43], IFF_PNG);
					ret |= pixWrite(mk_dst_filename("photo-invert-combined-mask-grey.png"), pix[40], IFF_PNG);
					ret |= pixWrite(mk_dst_filename("photo-invert-combined-grey.png"), pix[44], IFF_PNG);

					// -------------------------------------------------------------------------------------------
					// now use this 'photo-inverted' greyscale image as the source for the binarization methods:
					// -------------------------------------------------------------------------------------------

					for (int i = 2; i < 44; i++)
						pixDestroy(&pix[i]);


					{
						{
							int w, h;
							pixGetDimensions(pix[44], &w, &h, NULL);

							const struct {
								float tile_size;
								float smooth_size;
								float score_fraction;
							} scenarios[] = {
								{ 0.1 * h, 2.0f, 0.1f },
								{ 0.33 * h, 2.0f, 0.1f },
								{ 32, 2.0f, 0.1f },

								{ 0.1 * h, 0.0f, 0.1f },
								{ 0.33 * h, 0.0f, 0.1f },
								{ 32, 0.0f, 0.1f },
							};
							for (int i = 0; i < sizeof(scenarios) / sizeof(scenarios[0]); i++)
							{
								float tile_size = scenarios[i].tile_size;
								float smooth_size = scenarios[i].smooth_size;
								float score_fraction = scenarios[i].score_fraction;

								for (int i = 2; i < sizeof(pix) / sizeof(pix[0]); i++)
									pixDestroy(&pix[i]);

								ret |= OtsuThreshold(pix[1], tile_size, smooth_size, score_fraction, &pix[4], &pix[3], &pix[2]);

								ret |= pixWrite(mk_dst_filename("grey256.png"), pix[2], IFF_PNG);
								ret |= pixWrite(mk_dst_filename("thresholds.png"), pix[3], IFF_PNG);
								ret |= pixWrite(mk_dst_filename("binarized-result.png"), pix[4], IFF_PNG);
							}

							pix[5] = pixAddMirroredBorder(pix[44], w / 2, w / 2, h / 2, h / 2);
							ret |= pixWrite(mk_dst_filename("border-50pct.png"), pix[5], IFF_PNG);

							// add padding border: size must be Otsu smoothing kernel size + 1 as the smoothing --> convolution 'accumulator'
							// pix that will be constructed by the Otsu code internally will have the same size as
							// the provided source pix and we wish to prevent nasty border artifacts due to rounding
							// errors in blockconvLow()'s re-normalization of border rows/columns, which takes
							// (rounded) integer greyscale values from intermediate calculations, thus producing
							// rounding / math accuracy artifacts at the border. Hence it is 'smart' to make sure
							// we have a more-or-less plain, *thick*, border around the actual image before we apply
							// binarization of any kind.
							l_int32 border = 2 * L_MAX(32, 0.33 * h + 0.5);
							pix[6] = pixAddContinuedBorder(pix[44], border, border, border, border);
							ret |= pixWrite(mk_dst_filename("padding-2px.png"), pix[6], IFF_PNG);
						}

						if (!IsPixBinary(pix[6])) {
							pix[7] = GetPixRectGrey(pix[6]);
						}
						else {
							pix[7] = pixClone(pix[6]);
						}
						ret |= pixWrite(mk_dst_filename("grey256.png"), pix[7], IFF_PNG);

						{
							int w, h;
							pixGetDimensions(pix[7], &w, &h, NULL);

							const struct {
								float tile_size;
								float smooth_size;
								float score_fraction;
							} scenarios[] = {
								{ 0.1 * h, 2.0f, 0.1f },
								{ 0.33 * h, 2.0f, 0.1f },
								{ 32, 2.0f, 0.1f },

								{ 0.1 * h, 0.0f, 0.1f },
								{ 0.33 * h, 0.0f, 0.1f },
								{ 32, 0.0f, 0.1f },
							};
							for (int i = 0; i < sizeof(scenarios) / sizeof(scenarios[0]); i++)
							{
								float tile_size = scenarios[i].tile_size;
								float smooth_size = scenarios[i].smooth_size;
								float score_fraction = scenarios[i].score_fraction;

								for (int i = 8; i < 44; i++)
									pixDestroy(&pix[i]);

								ret |= OtsuThreshold(pix[7], tile_size, smooth_size, score_fraction, &pix[10], &pix[9], &pix[8]);

								ret |= pixWrite(mk_dst_filename("grey256.png"), pix[8], IFF_PNG);
								ret |= pixWrite(mk_dst_filename("thresholds.png"), pix[9], IFF_PNG);
								ret |= pixWrite(mk_dst_filename("binarized-result.png"), pix[10], IFF_PNG);
							}
						}

						//--------------------------------------------------------

						// Non-linear contrast normalization
						{
							int thresh;
							pix[11] = pixNLNorm2(pix[44], &thresh);
							ret |= pixWrite(mk_dst_filename("NLNorm2.png"), pix[11], IFF_PNG);
						}

						// Non-linear contrast normalization
						{
							int thresh, fgval, bgval;
							pix[12] = pixNLNorm1(pix[44], &thresh, &fgval, &bgval);
							ret |= pixWrite(mk_dst_filename("NLNorm1.png"), pix[12], IFF_PNG);
						}

						// source image, reduced to normalized greyscale.
						{
							pix[13] = GetPixNormRectGrey(pix[44]);
							ret |= pixWrite(mk_dst_filename("normalized-grey.png"), pix[13], IFF_PNG);
						}

						// tesseract
						{
							const struct {
								float window_size;
								float kfactor;
								float score_fraction;
							} scenarios[] = {
								{ 0.33, 0.34, 0.1 },  // tesseract
								{ 0.1, 0.1, 0.1 },
								{ 0.5, 0.1, 0.1 },
								{ 0.2, 0.1, 0.1 },
								{ 0.2, 0.5, 0.1 },
								{ 0.01, 0.3, 0.1 },
								{ 0.01, 0.01, 0.1 },
								{ 1.0, 1.0, 0.1 },
								{ 0.3, 0.01, 0.1 },
								{ 0.3, 0.1, 0.1 },
								{ 0.3, 1.0, 0.1 },
								{ 0.3, 3.0, 0.1 },
								{ 0.1, 3.0, 0.1 },
								{ 0.2, 3.0, 0.1 },
								{ 0.5, 3.0, 0.1 },
							};
							for (int i = 0; i < sizeof(scenarios) / sizeof(scenarios[0]); i++)
							{
								float window_size = scenarios[i].window_size;
								float kfactor = scenarios[i].kfactor;
								float score_fraction = scenarios[i].score_fraction;

								for (int i = 15; i < sizeof(pix) / sizeof(pix[0]); i++)
									pixDestroy(&pix[i]);

								if (!SauvolaThreshold(pix[44], window_size, kfactor, score_fraction, &pix[17], &pix[16], &pix[15])) {
									ret |= pixWrite(mk_dst_filename("sauvola-grey.png"), pix[15], IFF_PNG);
									ret |= pixWrite(mk_dst_filename("sauvola-threshold.png"), pix[16], IFF_PNG);
									ret |= pixWrite(mk_dst_filename("sauvola-output.png"), pix[17], IFF_PNG);
								}
							}
						}

						// leptonica
						{
							// Contrast normalization followed by Sauvola binarization
							l_int32 mindiff = 1;
							pix[18] = pixSauvolaOnContrastNorm(pix[44], mindiff, &pix[19], &pix[20]);
							ret |= pixWrite(mk_dst_filename("CNorm-sauvola-contrast-norm.png"), pix[19], IFF_PNG);
							ret |= pixWrite(mk_dst_filename("CNorm-sauvola-threshold.png"), pix[20], IFF_PNG);
							ret |= pixWrite(mk_dst_filename("CNorm-sauvola-output.png"), pix[18], IFF_PNG);
						}

						{
							if (!OtsuOnNormalizedBackground(pix[44], &pix[21], &pix[22])) {
								ret |= pixWrite(mk_dst_filename("otsu-normbg-grey.png"), pix[22], IFF_PNG);
								ret |= pixWrite(mk_dst_filename("otsu-normbg-output.png"), pix[21], IFF_PNG);
							}
						}

						{
							if (!MaskingAndOtsuOnNormalizedBackground(pix[44], &pix[23], &pix[24])) {
								ret |= pixWrite(mk_dst_filename("otsu-masked-normbg-grey.png"), pix[24], IFF_PNG);
								ret |= pixWrite(mk_dst_filename("otsu-masked-normbg-output.png"), pix[23], IFF_PNG);
							}
						}

						{
							int adaptive = 1;
							if (!NlBinThresholding(pix[44], adaptive, &pix[25], &pix[26])) {
								ret |= pixWrite(mk_dst_filename("nlbin-th-grey.png"), pix[26], IFF_PNG);
								ret |= pixWrite(mk_dst_filename("nlbin-th-output.png"), pix[25], IFF_PNG);
							}
						}

						{
							int adaptive = 1;
							pix[27] = pixNLBin(pix[44], adaptive);
							ret |= pixWrite(mk_dst_filename("nlbin-output.png"), pix[27], IFF_PNG);
						}


						{
							for (int i = 25; i < 44; i++)
								pixDestroy(&pix[i]);

							int adaptive = 0;
							if (!NlBinThresholding(pix[44], adaptive, &pix[25], &pix[26])) {
								ret |= pixWrite(mk_dst_filename("nlbin-th-grey.png"), pix[26], IFF_PNG);
								ret |= pixWrite(mk_dst_filename("nlbin-th-output.png"), pix[25], IFF_PNG);
							}
						}

						{
							int adaptive = 0;
							pix[27] = pixNLBin(pix[44], adaptive);
							ret |= pixWrite(mk_dst_filename("nlbin-output.png"), pix[27], IFF_PNG);
						}

						{
							pix[28] = ThresholdToPix(pix[44]);
							ret |= pixWrite(mk_dst_filename("threshold-output.png"), pix[28], IFF_PNG);
						}

						{
							pix[29] = GetPixRectThresholds(pix[44]);
							ret |= pixWrite(mk_dst_filename("get-rect-thresholds-output.png"), pix[29], IFF_PNG);
						}

						{
							int w, h;
							pixGetDimensions(pix[44], &w, &h, NULL);
							pix[30] = GetPixRect(pix[44], w / 4, h / 4, w / 2, h / 2);
							ret |= pixWrite(mk_dst_filename("cropped-by-half.png"), pix[30], IFF_PNG);
						}

						{
							pix[31] = OtsuThresholdRectToPix(pix[44]);
							ret |= pixWrite(mk_dst_filename("otsu-threshold-rect.png"), pix[31], IFF_PNG);
						}

						{
							int num_channels = 1;

							// Compute the histogram of the image rectangle.
							int histogram[HISTOGRAM_SIZE];
							HistogramRect(pix[44], num_channels, histogram);
							int H;
							int best_omega_0;
							int best_t = OtsuStats(histogram, &H, &best_omega_0);
							if (best_omega_0 == 0 || best_omega_0 == H) {
								// This channel is empty.
							}
							// To be a convincing foreground we must have a small fraction of H
							// or to be a convincing background we must have a large fraction of H.
							// In between we assume this channel contains no thresholding information.
							int hi_value = (best_omega_0 < H * 0.5);

							int thresholds[4] = { best_t };
							int hi_values[4] = { hi_value };
							pix[32] = ThresholdRectToPix(pix[44], num_channels, thresholds, hi_values);
							ret |= pixWrite(mk_dst_filename("threshold-rect-to-pix.png"), pix[32], IFF_PNG);
						}

						{
							int num_channels = 1;

							// Compute the histogram of the image rectangle.
							int histogram[HISTOGRAM_SIZE];
							HistogramRect(pix[44], num_channels, histogram);
							int H;
							int best_omega_0;
							int best_t = OtsuStats(histogram, &H, &best_omega_0);
							if (best_omega_0 == 0 || best_omega_0 == H) {
								// This channel is empty.
							}

							l_int32 thresh = best_t; // 128
							PIXA* pixa = pixaCreate(10);
							pix[33] = pixAutoPhotoinvert(pix[44], thresh, &pix[34], pixa);
							ret |= pixWrite(mk_dst_filename("photo-invert-mask.png"), pix[34], IFF_PNG);
							int n = pixaGetCount(pixa);
							for (int i = 0; i < n; i++) {
								PIX* p = pixaGetPix(pixa, i, L_CLONE);
								ret |= pixWrite(mk_dst_filename("photo-invert-dbg.png"), p, IFF_PNG);
								pixDestroy(&p);
							}
							pixaDestroy(&pixa);

							ret |= pixWrite(mk_dst_filename("photo-invert-output.png"), pix[33], IFF_PNG);
						}
					}
				}
			}
		}
}


		for (int i = 2; i < sizeof(pix) / sizeof(pix[0]); i++)
			pixDestroy(&pix[i]);


		// --------------------------------------------------------------------------------
		// testing pixBlockconv() and its artifacts in the top rows and left columns.
		// --------------------------------------------------------------------------------

		// create checkerboard, then smooth it using convolution
		sourcefile = DEMOPATH("blur-mask.png");

		pix[2] = pixRead(sourcefile);
		ret |= pixWrite(mk_dst_filename("bm-source.png"), pix[2], IFF_PNG);

		if (!IsPixGreyscale(pix[2])) {
			pix[3] = GetPixRectGrey(pix[2]);
		}
		else {
			pix[3] = pixClone(pix[2]);
		}
		ret |= pixWrite(mk_dst_filename("grey256.png"), pix[3], IFF_PNG);

		{
			pix[4] = pixBlockconv(pix[3], 1, 1);

			ret |= pixWrite(mk_dst_filename("block-convolution-3x3.png"), pix[4], IFF_PNG);
		}

		{
			int w, h;

			pix[5] = pixCreateTemplate(pix[3]);

			pixGetDimensions(pix[5], &w, &h, NULL);

			int i, j, k;

			for (i = 0; i < h; i++)
			{
				k = i % 2;
				for (j = 0; j < w; j++)
				{
					pixSetPixel(pix[5], j, i, k % 2 ? 0 : 255);
					k++;
				}
			}

			ret |= pixWrite(mk_dst_filename("checkerboard.png"), pix[5], IFF_PNG);

			pix[6] = pixBlockconv(pix[5], 1, 1);

			ret |= pixWrite(mk_dst_filename("block-convolution-3x3.png"), pix[6], IFF_PNG);

			// extend a border of 2px wide:
			pix[7] = pixExtendByReplication(pix[5], 4, 4);
			ret |= pixWrite(mk_dst_filename("checkerboard-border2.png"), pix[7], IFF_PNG);
			pixDestroy(&pix[7]);

			pix[7] = pixAddContinuedBorder(pix[5], 2, 2, 2, 2);
			ret |= pixWrite(mk_dst_filename("checkerboard-border2.png"), pix[7], IFF_PNG);

			pix[8] = pixBlockconv(pix[7], 1, 1);

			ret |= pixWrite(mk_dst_filename("block-convolution-3x3.png"), pix[8], IFF_PNG);



			pix[9] = pixCreateNoInit(200, 30, 8);

			l_uint32* data;
			l_uint32* line;
			l_uint32 wpl;

			h = pixGetHeight(pix[9]);
			w = pixGetWidth(pix[9]);
			wpl = pixGetWpl(pix[9]);
			data = pixGetData(pix[9]);
			for (i = 0; i < h; i++)
			{
				line = data + i * wpl;
				k = i % 2 ? 0 : 255;
				for (j = 0; j < w; j++)
				{
					SET_DATA_BYTE(line, j, k);
					if (k)
						k = 0;
					else
						k = 255;
				}
			}

			ret |= pixWrite(mk_dst_filename("checkerboard-200x30.png"), pix[9], IFF_PNG);

			pix[10] = pixBlockconv(pix[9], 1, 1);

			ret |= pixWrite(mk_dst_filename("block-convolution-3x3.png"), pix[10], IFF_PNG);

			pix[11] = pixBlockconv(pix[9], 2, 2);

			ret |= pixWrite(mk_dst_filename("block-convolution-5x5.png"), pix[11], IFF_PNG);

			pix[12] = pixBlockconv(pix[9], 3, 3);

			ret |= pixWrite(mk_dst_filename("block-convolution-7x7.png"), pix[12], IFF_PNG);

		}

	}






	

	for (int i = 0; i < sizeof(pix) / sizeof(pix[0]); i++)
		pixDestroy(&pix[i]);

	return regTestCleanup(rp) || ret;
}

