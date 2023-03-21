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
#include <assert.h>
#include "allheaders.h"
#include "demo_settings.h"

#include "monolithic_examples.h"




static l_ok
IsPixBinary(PIX* pix)
{
	int d = pixGetDepth(pix);
	return d == 1;
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
	pixDestroy(&pixg);
	pixg = pixCopy(NULL, pixd);
	pixDestroy(&pixd);

	/// Calculate flat version
	pixGetDimensions(pixg, &w1, &h1, NULL);
	pixd = pixScaleGeneral(pixg, 0.5, 0.5, 0.0, 0);
	pixd2 = pixRankFilter(pixd, 20, 2, 0.8);
	pixDestroy(&pixd);
	pixd = pixRankFilter(pixd2, 2, 20, 0.8);
	pixDestroy(&pixd2);
	pixGetDimensions(pixd, &w2, &h2, NULL);
	pixd2 = pixScaleGrayLI(pixd, (l_float32)w1 / (l_float32)w2,
		(l_float32)h1 / (l_float32)h2);
	pixDestroy(&pixd);
	pixInvert(pixd2, pixd2);
	pixAddGray(pixg, pixg, pixd2);
	pixDestroy(&pixd2);

	/// Local contrast enhancement
	//  Ignore a border of 10 % and get a mean threshold,
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
		(l_int32)std::min((l_int32)((bgval - thresh) * .5), (255 - bgval));
	factor = 255. / (bgval - fgval);
	if (pthresh) {
		*pthresh = (l_int32)threshpos * factor - threshpos * .1;
	}
	pixAddConstantGray(pixg, -1 * fgval);
	pixMultConstantGray(pixg, factor);

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
	factor = 255 / (bgval - fgval);
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

	return r;
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

	return r;
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

	return r;
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
#if 1
		pixb = pixDitherToBinarySpec(pixb, bgval - ((bgval - thresh) * 0.75), fgval + ((thresh - fgval) * 0.25));
#else
		pixb = pixThresholdToBinary(pixb, fgval+((thresh-fgval)*.1));  /* for bg and light fg */
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




#define HISTOGRAM_SIZE   256 // The size of a histogram of pixel values.


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
			int hi_value = best_omega_0 < H * 0.5;
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
HistogramRect(PIX *pix, int channel, int histogram[HISTOGRAM_SIZE]) {
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
OtsuStats(const int histogram[HISTOGRAM_SIZE], int* H_out, int* omega0_out) {
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
static l_ok
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
}

















#if defined(BUILD_MONOLITHIC)
#define main   lept_thresholding_test_main
#endif

int main(int    argc,
         const char **argv)
{
L_REGPARAMS  *rp;
PIX* pix[20] = { NULL };
l_ok ret = 0;
const char* sourcefile = DEMOPATH("Dance.Troupe.jpg");

    if (regTestSetup(argc, argv, &rp))
        return 1;

	lept_rmdir("lept/binarization");
	lept_mkdir("lept/binarization");

	if (argc == 3)
	{
		sourcefile = argv[2];
	}

	pix[0] = pixRead(sourcefile);
	if (!pix[0])
	{
		ret = 1;
	}
	else
	{
		ret |= pixWrite("/tmp/lept/binarization/orig.png", pix[0], IFF_PNG);

		// Otsu first; tesseract 'vanilla' behaviour mimic

		if (!IsPixBinary(pix[0])) {
			pix[1] = GetPixRectGrey(pix[0]);

			pix[2]
			auto [ok, pix_grey, pix_binary, pix_thresholds] = thresholder_->Threshold(thresholding_method);

			if (!ok) {
				return false;
			}

			if (go)
				*pix = pix_binary;

			tesseract_->set_pix_thresholds(pix_thresholds);
			tesseract_->set_pix_grey(pix_grey);





			if (tesseract_->tessedit_dump_pageseg_images) {
				tesseract_->AddPixDebugPage(tesseract_->pix_grey(), (caption + " : Grey = pre-image").c_str());
				tesseract_->AddPixDebugPage(tesseract_->pix_thresholds(), (caption + " : Thresholds").c_str());
				tesseract_->AddPixDebugPage(pix_binary, (caption + " : Binary = post-image").c_str());
			}

			if (!go)
				pix_binary.destroy();
		}
	}

	}

	for (int i = 0; i < sizeof(pix) / sizeof(pix[0]); i++)
		pixDestroy(&pix[i]);

	return regTestCleanup(rp) || ret;
}




/*----------------------------------------------------------------------*
 *                  Non-linear contrast normalization                   *
 *                            and thresholding                          *
 *----------------------------------------------------------------------*/
 /*!
  * \brief   pixNLBin()
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
PIX*
ImageThresholder::pixNLBin(PIX* pixs, bool adaptive)
{
	int thresh;
	int fgval, bgval;
	PIX* pixb;

	PROCNAME("pixNLBin");

	pixb = pixNLNorm1(pixs, &thresh, &fgval, &bgval);
	if (!pixb)
		return (PIX*)ERROR_PTR("invalid normalization result", procName, NULL);

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
		pixb = pixDitherToBinarySpec(pixb, bgval - ((bgval - thresh) * 0.75), fgval + ((thresh - fgval) * 0.25));
		//pixb = pixThresholdToBinary(pixb, fgval+((thresh-fgval)*.1));  /* for bg and light fg */
	}

	return pixb;
}





std::tuple<bool, Image, Image, Image> ImageThresholder::Threshold(
	ThresholdMethod method) {
	Image pix_binary = nullptr;
	Image pix_thresholds = nullptr;

	if (pix_channels_ == 0) {
		// We have a binary image, but it still has to be copied, as this API
		// allows the caller to modify the output.
		Image original = GetPixRect();
		pix_binary = original.copy();
		original.destroy();
		return std::make_tuple(true, nullptr, pix_binary, nullptr);
	}

	auto pix_grey = GetPixRectGrey();

	int r = 0;
	l_int32 threshold_val = 0;

	l_int32 pix_w, pix_h;
	pixGetDimensions(pix_ /* pix_grey */, &pix_w, &pix_h, nullptr);

	if (tesseract_->thresholding_debug) {
		tprintf("\nimage width: {}  height: {}  ppi: {}\n", pix_w, pix_h, yres_);
	}

	if (method == ThresholdMethod::Sauvola) {
		int window_size;
		window_size = tesseract_->thresholding_window_size * yres_;
		window_size = std::max(7, window_size);
		window_size = std::min(pix_w < pix_h ? pix_w - 3 : pix_h - 3, window_size);
		int half_window_size = window_size / 2;

		// factor for image division into tiles; >= 1
		l_int32 nx, ny;
		// tiles size will be approx. 250 x 250 pixels
		nx = std::max(1, (pix_w + 125) / 250);
		ny = std::max(1, (pix_h + 125) / 250);
		auto xrat = pix_w / nx;
		auto yrat = pix_h / ny;
		if (xrat < half_window_size + 2) {
			nx = pix_w / (half_window_size + 2);
		}
		if (yrat < half_window_size + 2) {
			ny = pix_h / (half_window_size + 2);
		}

		double kfactor = tesseract_->thresholding_kfactor;
		kfactor = std::max(0.0, kfactor);

		if (tesseract_->thresholding_debug) {
			tprintf("window size: {}  kfactor: {}  nx: {}  ny: {}\n", window_size, kfactor, nx, ny);
		}

		r = pixSauvolaBinarizeTiled(pix_grey, half_window_size, kfactor, nx, ny,
			(PIX**)pix_thresholds,
			(PIX**)pix_binary);
	}
	else if (method == ThresholdMethod::OtsuOnNormalizedBackground) {
		pix_binary = pixOtsuThreshOnBackgroundNorm(pix_grey, nullptr, 10, 15, 100,
			50, 255, 2, 2, 0.1f,
			&threshold_val);
	}
	else if (method == ThresholdMethod::MaskingAndOtsuOnNormalizedBackground) {
		pix_binary = pixMaskedThreshOnBackgroundNorm(pix_grey, nullptr, 10, 15,
			100, 50, 2, 2, 0.1f,
			&threshold_val);
	}
	else if (method == ThresholdMethod::LeptonicaOtsu) {
		int tile_size;
		double tile_size_factor = tesseract_->thresholding_tile_size;
		tile_size = tile_size_factor * yres_;
		tile_size = std::max(16, tile_size);

		int smooth_size;
		double smooth_size_factor = tesseract_->thresholding_smooth_kernel_size;
		smooth_size_factor = std::max(0.0, smooth_size_factor);
		smooth_size = smooth_size_factor * yres_;
		int half_smooth_size = smooth_size / 2;

		double score_fraction = tesseract_->thresholding_score_fraction;

		if (tesseract_->thresholding_debug) {
			tprintf("LeptonicaOtsu thresholding: tile size: {}, smooth_size: {}, score_fraction: {}\n", tile_size, smooth_size, score_fraction);
		}

		r = pixOtsuAdaptiveThreshold(pix_grey, tile_size, tile_size,
			half_smooth_size, half_smooth_size,
			score_fraction,
			(PIX**)pix_thresholds,
			(PIX**)pix_binary);
	}
	else if (method == ThresholdMethod::Nlbin) {
		auto pix = GetPixRect();
		pix_binary = pixNLBin(pix, false);
		r = 0;
	}
	else {
		// Unsupported threshold method.
		r = 1;
	}

	bool ok = (r == 0) && pix_binary;
	return std::make_tuple(ok, pix_grey, pix_binary, pix_thresholds);
}






// Threshold the source image as efficiently as possible to the output Pix.
// Creates a Pix and sets pix to point to the resulting pointer.
// Caller must use pixDestroy to free the created Pix.
//
/// Returns false on error.
bool ImageThresholder::ThresholdToPix(Image* pix) {
	// tolerate overlarge images when they're about to be cropped by GetPixRect():
	if (IsFullImage()) {
		if (tesseract_->CheckAndReportIfImageTooLarge(pix_)) {
			return false;
		}
	}
	else {
		// validate against the future cropped image size:
		if (tesseract_->CheckAndReportIfImageTooLarge(rect_width_, rect_height_)) {
			return false;
		}
	}

	Image original = GetPixRect();

	if (pix_channels_ == 0) {
		// We have a binary image, but it still has to be copied, as this API
		// allows the caller to modify the output.
		*pix = original.copy();
	}
	else {
		if (pixGetColormap(original)) {
			Image tmp;
			Image without_cmap = pixRemoveColormap(original, REMOVE_CMAP_BASED_ON_SRC);
			int depth = pixGetDepth(without_cmap);
			if (depth > 1 && depth < 8) {
				tmp = pixConvertTo8(without_cmap, false);
			}
			else {
				tmp = without_cmap.copy();
			}
			without_cmap.destroy();
			OtsuThresholdRectToPix(tmp, pix);
			tmp.destroy();
		}
		else {
			OtsuThresholdRectToPix(pix_, pix);
		}
	}
	original.destroy();
	return true;
}




// Gets a pix that contains an 8 bit threshold value at each pixel. The
// returned pix may be an integer reduction of the binary image such that
// the scale factor may be inferred from the ratio of the sizes, even down
// to the extreme of a 1x1 pixel thresholds image.
// Ideally the 8 bit threshold should be the exact threshold used to generate
// the binary image in ThresholdToPix, but this is not a hard constraint.
// Returns nullptr if the input is binary. PixDestroy after use.
Image ImageThresholder::GetPixRectThresholds() {
	if (IsBinary()) {
		return nullptr;
	}
	Image pix_grey = GetPixRectGrey();
	int width = pixGetWidth(pix_grey);
	int height = pixGetHeight(pix_grey);
	std::vector<int> thresholds;
	std::vector<int> hi_values;
	OtsuThreshold(pix_grey, 0, 0, width, height, thresholds, hi_values);
	pix_grey.destroy();
	Image pix_thresholds = pixCreate(width, height, 8);
	int threshold = thresholds[0] > 0 ? thresholds[0] : 128;
	pixSetAllArbitrary(pix_thresholds, threshold);
	return pix_thresholds;
}





// Get a clone/copy of the source image rectangle.
// The returned Pix must be pixDestroyed.
// This function will be used in the future by the page layout analysis, and
// the layout analysis that uses it will only be available with Leptonica,
// so there is no raw equivalent.
Image ImageThresholder::GetPixRect() {
	if (IsFullImage()) {
		// Just clone the whole thing.
		return pix_.clone();
	}
	else {
		// Crop to the given rectangle.
		Box* box = boxCreate(rect_left_, rect_top_, rect_width_, rect_height_);
		Image cropped = pixClipRectangle(pix_, box, nullptr);
		boxDestroy(&box);
		return cropped;
	}
}






// Get a clone/copy of the source image rectangle, reduced to greyscale,
// and at the same resolution as the output binary.
// The returned Pix must be pixDestroyed.
// Provided to the classifier to extract features from the greyscale image.
Image ImageThresholder::GetPixRectGrey() {
	auto pix = GetPixRect(); // May have to be reduced to grey.
	int depth = pixGetDepth(pix);
	if (depth != 8 || pixGetColormap(pix)) {
		if (depth == 24) {
			auto tmp = pixConvert24To32(pix);
			pix.destroy();
			pix = tmp;
		}
		auto result = pixConvertTo8(pix, false);
		pix.destroy();
		return result;
	}
	return pix;
}





// Get a clone/copy of the source image rectangle, reduced to normalized greyscale,
// and at the same resolution as the output binary.
// The returned Pix must be pixDestroyed.
// Provided to the classifier to extract features from the greyscale image.
Image ImageThresholder::GetPixNormRectGrey() {
	auto pix = GetPixRect();
	auto result = ImageThresholder::pixNLNorm2(pix, nullptr);
	pix.destroy();
	return result;
}




// Otsu thresholds the rectangle, taking the rectangle from *this.
void ImageThresholder::OtsuThresholdRectToPix(Image src_pix, Image* out_pix) const {
	std::vector<int> thresholds;
	std::vector<int> hi_values;

	int num_channels = OtsuThreshold(src_pix, rect_left_, rect_top_, rect_width_, rect_height_,
		thresholds, hi_values);
	// only use opencl if compiled w/ OpenCL and selected device is opencl
#ifdef USE_OPENCL
	OpenclDevice od;
	if (num_channels == 4 && od.selectedDeviceIsOpenCL() && rect_top_ == 0 && rect_left_ == 0) {
		od.ThresholdRectToPixOCL((unsigned char*)pixGetData(src_pix), num_channels,
			pixGetWpl(src_pix) * 4, &thresholds[0], &hi_values[0], out_pix /*pix_OCL*/,
			rect_height_, rect_width_, rect_top_, rect_left_);
	}
	else {
#endif
		ThresholdRectToPix(src_pix, num_channels, thresholds, hi_values, out_pix);
#ifdef USE_OPENCL
	}
#endif
}




/// Threshold the rectangle, taking everything except the src_pix
/// from the class, using thresholds/hi_values to the output pix.
/// NOTE that num_channels is the size of the thresholds and hi_values
// arrays and also the bytes per pixel in src_pix.
void ImageThresholder::ThresholdRectToPix(Image src_pix, int num_channels, const std::vector<int>& thresholds,
	const std::vector<int>& hi_values, Image* pix) const {
	*pix = pixCreate(rect_width_, rect_height_, 1);
	uint32_t* pixdata = pixGetData(*pix);
	int wpl = pixGetWpl(*pix);
	int src_wpl = pixGetWpl(src_pix);
	uint32_t* srcdata = pixGetData(src_pix);
	pixSetXRes(*pix, pixGetXRes(src_pix));
	pixSetYRes(*pix, pixGetYRes(src_pix));
	for (int y = 0; y < rect_height_; ++y) {
		const uint32_t* linedata = srcdata + (y + rect_top_) * src_wpl;
		uint32_t* pixline = pixdata + y * wpl;
		for (int x = 0; x < rect_width_; ++x) {
			bool white_result = true;
			for (int ch = 0; ch < num_channels; ++ch) {
				int pixel = GET_DATA_BYTE(linedata, (x + rect_left_) * num_channels + ch);
				if (hi_values[ch] >= 0 && (pixel > thresholds[ch]) == (hi_values[ch] == 0)) {
					white_result = false;
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
}

