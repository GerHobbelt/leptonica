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

/*!
 * \file convolve_old.c
 * <pre>
 *
 *      Grayscale block convolution
 *          PIX          *pixBlockconvGray_old()
 *          static void   blockconvLow_old()
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <math.h>
#include "allheaders.h"

    /* These globals determine the subsampling factors for
     * generic convolution of pix and fpix.  Declare extern to use.
     * To change the values, use l_setConvolveSampling(). */
LEPT_DLL l_int32  ConvolveSamplingFactX = 1;
LEPT_DLL l_int32  ConvolveSamplingFactY = 1;

    /* Low-level static functions */
static void blockconvLow_old(l_uint32 *data, l_int32 w, l_int32 h, l_int32 wpl,
                         l_uint32 *dataa, l_int32 wpla, l_int32 wc,
                         l_int32 hc);


/*----------------------------------------------------------------------*
 *                     Grayscale block convolution                      *
 *----------------------------------------------------------------------*/
/*!
 * \brief   pixBlockconvGray_old()
 *
 * \param[in]    pixs     8 bpp
 * \param[in]    pixacc   pix 32 bpp; can be null
 * \param[in]    wc, hc   half width/height of convolution kernel
 * \return  pix 8 bpp, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) If accum pix is null, make one and destroy it before
 *          returning; otherwise, just use the input accum pix.
 *      (2) The full width and height of the convolution kernel
 *          are (2 * wc + 1) and (2 * hc + 1).
 *      (3) Returns a copy if either wc or hc are 0
 *      (4) Require that w >= 2 * wc + 1 and h >= 2 * hc + 1,
 *          where (w,h) are the dimensions of pixs.  Attempt to
 *          reduce the kernel size if necessary.
 * </pre>
 */
PIX *
pixBlockconvGray_old(PIX     *pixs,
                 PIX     *pixacc,
                 l_int32  wc,
                 l_int32  hc)
{
l_int32    w, h, d, wpl, wpla, edge_fix;
l_uint32  *datad, *dataa;
PIX       *pixd, *pixt;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    pixGetDimensions(pixs, &w, &h, &d);
    if (d != 8)
        return (PIX *)ERROR_PTR("pixs not 8 bpp", __func__, NULL);
    if (wc <= 0 || hc <= 0)   /* no-op */
        return pixCopy(NULL, pixs);
    if (w < 2 * wc + 1 || h < 2 * hc + 1) {
        L_WARNING("kernel too large: wc = %d, hc = %d, w = %d, h = %d; "
                  "reducing!\n", __func__, wc, hc, w, h);
        wc = L_MIN(wc, (w - 1) / 2);
        hc = L_MIN(hc, (h - 1) / 2);
    }
    if (wc == 0 || hc == 0)
        return pixCopy(NULL, pixs);

	// only exec the 'edge fix' additional work when we will produce our own accumulator values for pixs:
	edge_fix = (!pixacc || pixGetDepth(pixacc) != 32);

	if (edge_fix) {
		pixs = pixAddContinuedBorder(pixs, wc + 1, wc, hc + 1, hc);
		if (pixs == NULL)
			return (PIX*)ERROR_PTR("pix border extend not made", __func__, NULL);
		w += 2 * wc + 1;
		h += 2 * hc + 1;
	}

    if (pixacc) {
        if (pixGetDepth(pixacc) == 32) {
            pixt = pixClone(pixacc);
        } else {
            L_WARNING("pixacc not 32 bpp; making new one\n", __func__);
			if ((pixt = pixBlockconvAccum(pixs)) == NULL) {
				if (edge_fix) 
					pixDestroy(&pixs);
				return (PIX*)ERROR_PTR("pixt not made", __func__, NULL);
			}
        }
    } else {
		if ((pixt = pixBlockconvAccum(pixs)) == NULL) {
		if (edge_fix) pixDestroy(&pixs);
			return (PIX*)ERROR_PTR("pixt not made", __func__, NULL);
		}
    }

    if ((pixd = pixCreateTemplate(pixs)) == NULL) {
		if (edge_fix) pixDestroy(&pixs);
		pixDestroy(&pixt);
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    }

    pixSetPadBits(pixt, 0);
    wpl = pixGetWpl(pixd);
    wpla = pixGetWpl(pixt);
    datad = pixGetData(pixd);
    dataa = pixGetData(pixt);
    blockconvLow_old(datad, w, h, wpl, dataa, wpla, wc, hc);

    pixDestroy(&pixt);
	if (edge_fix) {
		pixDestroy(&pixs);
		pixt = pixRemoveBorderGeneral(pixd, wc + 1, wc, hc + 1, hc);
		if (pixt == NULL) {
			pixDestroy(&pixd);
			return (PIX*)ERROR_PTR("pix border removal not made", __func__, NULL);
		}
		pixDestroy(&pixd);
		pixd = pixt;
	}
    return pixd;
}


/*!
 * \brief   blockconvLow_old()
 *
 * \param[in]    data      data of input image, to be convolved
 * \param[in]    w, h, wpl
 * \param[in]    dataa     data of 32 bpp accumulator
 * \param[in]    wpla      accumulator
 * \param[in]    wc        convolution "half-width"
 * \param[in]    hc        convolution "half-height"
 * \return  void
 *
 * <pre>
 * Notes:
 *      (1) The full width and height of the convolution kernel
 *          are (2 * wc + 1) and (2 * hc + 1).
 *      (2) The lack of symmetry between the handling of the
 *          first (hc + 1) lines and the last (hc) lines,
 *          and similarly with the columns, is due to fact that
 *          for the pixel at (x,y), the accumulator values are
 *          taken at (x + wc, y + hc), (x - wc - 1, y + hc),
 *          (x + wc, y - hc - 1) and (x - wc - 1, y - hc - 1).
 *      (3) We compute sums, normalized as if there were no reduced
 *          area at the boundary.  This under-estimates the value
 *          of the boundary pixels, so we multiply them by another
 *          normalization factor that is greater than 1.
 *      (4) This second normalization is done first for the first
 *          hc + 1 lines; then for the last hc lines; and finally
 *          for the first wc + 1 and last wc columns in the intermediate
 *          lines.
 *      (5) The caller should verify that wc < w and hc < h.
 *          Failing either condition, illegal reads and writes can occur.
 *      (6) Implementation note: to get the same results in the interior
 *          between this function and pixConvolve(), it is necessary to
 *          add 0.5 for roundoff in the main loop that runs over all pixels.
 *          However, if we do that and have white (255) pixels near the
 *          image boundary, some overflow occurs for pixels very close
 *          to the boundary.  We can't fix this by subtracting from the
 *          normalized values for the boundary pixels, because this results
 *          in underflow if the boundary pixels are black (0).  Empirically,
 *          adding 0.25 (instead of 0.5) before truncating in the main
 *          loop will not cause overflow, but this gives some
 *          off-by-1-level errors in interior pixel values.  So we add
 *          0.5 for roundoff in the main loop, and for pixels within a
 *          half filter width of the boundary, use a L_MIN of the
 *          computed value and 255 to avoid overflow during normalization.
 * </pre>
 */
static void
blockconvLow_old(l_uint32  *data,
             l_int32    w,
             l_int32    h,
             l_int32    wpl,
             l_uint32  *dataa,
             l_int32    wpla,
             l_int32    wc,
             l_int32    hc)
{
l_int32    i, j, imax, imin, jmax, jmin;
l_int32    fwc, fhc, wmwc, hmhc;
l_float32  norm;
l_uint32   val;
l_uint32  *linemina, *linemaxa, *line;

    wmwc = w - wc;
    hmhc = h - hc;
    if (wmwc <= 0 || hmhc <= 0) {
        L_ERROR("wc >= w || hc >= h\n", __func__);
        return;
    }
    fwc = 2 * wc + 1;
    fhc = 2 * hc + 1;
    norm = 1.0 / ((l_float32)(fwc) * fhc);

        /*------------------------------------------------------------*
         *  Compute, using b.c. only to set limits on the accum image *
         *------------------------------------------------------------*/
    for (i = hc + 1; i < hmhc; i++) {
        imin = L_MAX(i - 1 - hc, 0);
        imax = L_MIN(i + hc, h - 1);
        line = data + wpl * i;
        linemina = dataa + wpla * imin;
        linemaxa = dataa + wpla * imax;
        for (j = wc + 1; j < wmwc; j++) {
            jmin = L_MAX(j - 1 - wc, 0);
            jmax = L_MIN(j + wc, w - 1);
            val = linemaxa[jmax] - linemaxa[jmin]
                  + linemina[jmin] - linemina[jmax];
            val = (l_uint8)(norm * val + 0.5);  /* see comment (6) above */
            SET_DATA_BYTE(line, j, val);
        }
    }

		/*------------------------------------------------------------*
		 *             Fix normalization for boundary pixels          *
		 *------------------------------------------------------------*/
	/* first hc + 1 lines, middle portion */
	fwc = 2 * wc + 1;
	for (i = 0; i <= hc; i++) {
		imin = L_MAX(i - 1 - hc, 0);
		imax = L_MIN(i + hc, h - 1);
		//fwc = 2 * wc + 1;
		fhc = imax - imin;
		norm = 1.0 / ((l_float32)(fwc)*fhc);
		line = data + wpl * i;
		linemina = dataa + wpla * imin;
		linemaxa = dataa + wpla * imax;
		for (j = wc + 1; j < wmwc; j++) {
			jmin = L_MAX(j - 1 - wc, 0);
			jmax = L_MIN(j + wc, w - 1);
			val = linemaxa[jmax] - linemaxa[jmin]
				+ linemina[jmin] - linemina[jmax];
			val = (l_uint8)(norm * val + 0.5);
			val = L_MIN(255, val);  /* see comment (6) above */
			SET_DATA_BYTE(line, j, val);
		}
	}

	/* last hc lines, middle portion */
	for (i = hmhc; i < h; i++) {
		imin = L_MAX(i - 1 - hc, 0);
		imax = L_MIN(i + hc, h - 1);
		//fwc = 2 * wc + 1;
		fhc = imax - imin;
		norm = 1.0 / ((l_float32)(fwc)*fhc);
		line = data + wpl * i;
		linemina = dataa + wpla * imin;
		linemaxa = dataa + wpla * imax;
		for (j = wc + 1; j < wmwc; j++) {
			jmin = L_MAX(j - 1 - wc, 0);
			jmax = L_MIN(j + wc, w - 1);
			val = linemaxa[jmax] - linemaxa[jmin]
				+ linemina[jmin] - linemina[jmax];
			val = (l_uint8)(norm * val + 0.5);
			val = L_MIN(255, val);  /* see comment (6) above */
			SET_DATA_BYTE(line, j, val);
		}
	}

	// TODO: we COULD /MAYBE/ have done this a little smarter by first doing the intermediate lines' left & right edges, swapping the j and i loops
	// so we could've pulled norm calculus as an invariant into the outer loop. However, the decision was made, this jumping back & forth through
	// the image memory zone wouldn't help performance either, so we stick with the 'naive' approach here. Until someone smarter comes along...

	for (i = 0; i < h; i++) {    /* left edge, right edge, 4 corners */
		imin = L_MAX(i - 1 - hc, 0);
		imax = L_MIN(i + hc, h - 1);
		fhc = imax - imin;
		line = data + wpl * i;
		linemina = dataa + wpla * imin;
		linemaxa = dataa + wpla * imax;
		for (j = 0; j <= wc; j++) {
			jmin = L_MAX(j - 1 - wc, 0);
			jmax = L_MIN(j + wc, w - 1);
			fwc = jmax - jmin;
			norm = 1.0 / ((l_float32)(fwc)*fhc);
			val = linemaxa[jmax] - linemaxa[jmin]
				+ linemina[jmin] - linemina[jmax];
			val = (l_uint8)(norm * val + 0.5);
			val = L_MIN(255, val);  /* see comment (6) above */
			SET_DATA_BYTE(line, j, val);
		}
		for (j = wmwc; j < w; j++) {
			jmin = L_MAX(j - 1 - wc, 0);
			jmax = L_MIN(j + wc, w - 1);
			fwc = jmax - jmin;
			norm = 1.0 / ((l_float32)(fwc)*fhc);
			val = linemaxa[jmax] - linemaxa[jmin]
				+ linemina[jmin] - linemina[jmax];
			val = (l_uint8)(norm * val + 0.5);
			val = L_MIN(255, val);  /* see comment (6) above */
			SET_DATA_BYTE(line, j, val);
		}
	}
}
