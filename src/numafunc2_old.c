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
 * \file  numafunc2_old.c
 * <pre>
 *
 *      --------------------------------------
 *      This file has these Numa utilities:
 *         - morphological operations
 *         - arithmetic transforms
 *         - windowed statistical operations
 *         - histogram extraction
 *         - histogram comparison
 *         - extrema finding
 *         - frequency and crossing analysis
 *      --------------------------------------
 *
 *      Splitting a distribution
 *          l_int32      numaSplitDistribution_Old()
 *
 *    Things to remember when using the Numa:
 *
 *    (1) The numa is a struct, not an array.  Always use accessors
 *        (see numabasic.c), never the fields directly.
 *
 *    (2) The number array holds l_float32 values.  It can also
 *        be used to store l_int32 values.  See numabasic.c for
 *        details on using the accessors.  Integers larger than
 *        about 10M will lose accuracy due on retrieval due to round-off.
 *        For large integers, use the dna (array of l_float64) instead.
 *
 *    (3) Occasionally, in the comments we denote the i-th element of a
 *        numa by na[i].  This is conceptual only -- the numa is not an array!
 *
 *    Some general comments on histograms:
 *
 *    (1) Histograms are the generic statistical representation of
 *        the data about some attribute.  Typically they're not
 *        normalized -- they simply give the number of occurrences
 *        within each range of values of the attribute.  This range
 *        of values is referred to as a 'bucket'.  For example,
 *        the histogram could specify how many connected components
 *        are found for each value of their width; in that case,
 *        the bucket size is 1.
 *
 *    (2) In leptonica, all buckets have the same size.  Histograms
 *        are therefore specified by a numa of occurrences, along
 *        with two other numbers: the 'value' associated with the
 *        occupants of the first bucket and the size (i.e., 'width')
 *        of each bucket.  These two numbers then allow us to calculate
 *        the value associated with the occupants of each bucket.
 *        These numbers are fields in the numa, initialized to
 *        a startx value of 0.0 and a binsize of 1.0.  Accessors for
 *        these fields are functions numa*Parameters().  All histograms
 *        must have these two numbers properly set.
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <math.h>
#include "allheaders.h"

l_ok
numaSplitDistribution_Old(NUMA* na,
	l_float32   scorefract,
	l_int32* psplitindex,
	l_float32* pave1,
	l_float32* pave2,
	l_float32* pnum1,
	l_float32* pnum2,
	NUMA** pnascore);
l_ok
grayHistogramsToEMD_Old(NUMAA* naa1,
	NUMAA* naa2,
	NUMA** pnad);
l_ok
numaEarthMoverDistance_Old(NUMA* na1,
	NUMA* na2,
	l_float32* pdist);
l_ok
grayInterHistogramStats_Old(NUMAA* naa,
	l_int32  wc,
	NUMA** pnam,
	NUMA** pnams,
	NUMA** pnav,
	NUMA** pnarv);
NUMA*
numaFindPeaks_Old(NUMA* nas,
	l_int32    nmax,
	l_float32  fract1,
	l_float32  fract2);
NUMA*
numaFindExtrema_Old(NUMA* nas,
	l_float32  delta,
	NUMA** pnav);
l_ok
numaFindLocForThreshold_Old(NUMA* na,
	l_int32     skip,
	l_int32* pthresh,
	l_float32* pfract);
l_ok
numaCountReversals_Old(NUMA* nas,
	l_float32   minreversal,
	l_int32* pnr,
	l_float32* prd);
l_ok
numaSelectCrossingThreshold_Old(NUMA* nax,
	NUMA* nay,
	l_float32   estthresh,
	l_float32* pbestthresh);
NUMA*
numaCrossingsByThreshold_Old(NUMA* nax,
	NUMA* nay,
	l_float32  thresh);
NUMA*
numaCrossingsByPeaks_Old(NUMA* nax,
	NUMA* nay,
	l_float32  delta);
l_ok
numaEvalBestHaarParameters_Old(NUMA* nas,
	l_float32   relweight,
	l_int32     nwidth,
	l_int32     nshift,
	l_float32   minwidth,
	l_float32   maxwidth,
	l_float32* pbestwidth,
	l_float32* pbestshift,
	l_float32* pbestscore);
l_ok
numaEvalHaarSum_Old(NUMA* nas,
	l_float32   width,
	l_float32   shift,
	l_float32   relweight,
	l_float32* pscore);
NUMA*
genConstrainedNumaInRange_Old(l_int32  first,
	l_int32  last,
	l_int32  nmax,
	l_int32  use_pairs);


/*----------------------------------------------------------------------*
 *                      Splitting a distribution                        *
 *----------------------------------------------------------------------*/
/*!
 * \brief   numaSplitDistribution_Old()
 *
 * \param[in]    na           histogram
 * \param[in]    scorefract   fraction of the max score, used to determine
 *                            range over which the histogram min is searched
 * \param[out]   psplitindex  [optional] index for splitting
 * \param[out]   pave1        [optional] average of lower distribution
 * \param[out]   pave2        [optional] average of upper distribution
 * \param[out]   pnum1        [optional] population of lower distribution
 * \param[out]   pnum2        [optional] population of upper distribution
 * \param[out]   pnascore     [optional] for debugging; otherwise use NULL
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This function is intended to be used on a distribution of
 *          values that represent two sets, such as a histogram of
 *          pixel values for an image with a fg and bg, and the goal
 *          is to determine the averages of the two sets and the
 *          best splitting point.
 *      (2) The Otsu method finds a split point that divides the distribution
 *          into two parts by maximizing a score function that is the
 *          product of two terms:
 *            (a) the square of the difference of centroids, (ave1 - ave2)^2
 *            (b) fract1 * (1 - fract1)
 *          where fract1 is the fraction in the lower distribution.
 *      (3) This works well for images where the fg and bg are
 *          each relatively homogeneous and well-separated in color.
 *          However, if the actual fg and bg sets are very different
 *          in size, and the bg is highly varied, as can occur in some
 *          scanned document images, this will bias the split point
 *          into the larger "bump" (i.e., toward the point where the
 *          (b) term reaches its maximum of 0.25 at fract1 = 0.5.
 *          To avoid this, we define a range of values near the
 *          maximum of the score function, and choose the value within
 *          this range such that the histogram itself has a minimum value.
 *          The range is determined by scorefract: we include all abscissa
 *          values to the left and right of the value that maximizes the
 *          score, such that the score stays above (1 - scorefract) * maxscore.
 *          The intuition behind this modification is to try to find
 *          a split point that both has a high variance score and is
 *          at or near a minimum in the histogram, so that the histogram
 *          slope is small at the split point.
 *      (4) We normalize the score so that if the two distributions
 *          were of equal size and at opposite ends of the numa, the
 *          score would be 1.0.
 * </pre>
 */
l_ok
numaSplitDistribution_Old(NUMA       *na,
                      l_float32   scorefract,
                      l_int32    *psplitindex,
                      l_float32  *pave1,
                      l_float32  *pave2,
                      l_float32  *pnum1,
                      l_float32  *pnum2,
                      NUMA      **pnascore)
{
l_int32    i, n, bestsplit, minrange, maxrange, maxindex;
l_float32  ave1, ave2, ave1prev, ave2prev;
l_float32  num1, num2, num1prev, num2prev;
l_float32  val, minval, sum, fract1;
l_float32  norm, score, minscore, maxscore;
NUMA      *nascore, *naave1, *naave2, *nanum1, *nanum2;

    if (psplitindex) *psplitindex = 0;
    if (pave1) *pave1 = 0.0;
    if (pave2) *pave2 = 0.0;
    if (pnum1) *pnum1 = 0.0;
    if (pnum2) *pnum2 = 0.0;
    if (pnascore) *pnascore = NULL;
    if (!na)
        return ERROR_INT("na not defined", __func__, 1);

    n = numaGetCount(na);
    if (n <= 1)
        return ERROR_INT("n = 1 in histogram", __func__, 1);
    numaGetSum(na, &sum);
    if (sum <= 0.0)
        return ERROR_INT("sum <= 0.0", __func__, 1);
    norm = 4.0f / ((l_float32)(n - 1) * (n - 1));
    ave1prev = 0.0;
    numaGetHistogramStats(na, 0.0, 1.0, &ave2prev, NULL, NULL, NULL);
    num1prev = 0.0;
    num2prev = sum;
    maxindex = n / 2;  /* initialize with something */

        /* Split the histogram with [0 ... i] in the lower part
         * and [i+1 ... n-1] in upper part.  First, compute an otsu
         * score for each possible splitting.  */
    if ((nascore = numaCreate(n)) == NULL)
        return ERROR_INT("nascore not made", __func__, 1);
    naave1 = (pave1) ? numaCreate(n) : NULL;
    naave2 = (pave2) ? numaCreate(n) : NULL;
    nanum1 = (pnum1) ? numaCreate(n) : NULL;
    nanum2 = (pnum2) ? numaCreate(n) : NULL;
    maxscore = 0.0;
    for (i = 0; i < n; i++) {
        numaGetFValue(na, i, &val);
        num1 = num1prev + val;
        if (num1 == 0)
            ave1 = ave1prev;
        else
            ave1 = (num1prev * ave1prev + i * val) / num1;
        num2 = num2prev - val;
        if (num2 == 0)
            ave2 = ave2prev;
        else
            ave2 = (num2prev * ave2prev - i * val) / num2;
        fract1 = num1 / sum;
        score = norm * (fract1 * (1 - fract1)) * (ave2 - ave1) * (ave2 - ave1);
        numaAddNumber(nascore, score);
        if (pave1) numaAddNumber(naave1, ave1);
        if (pave2) numaAddNumber(naave2, ave2);
        if (pnum1) numaAddNumber(nanum1, num1);
        if (pnum2) numaAddNumber(nanum2, num2);
        if (score > maxscore) {
            maxscore = score;
            maxindex = i;
        }
        num1prev = num1;
        num2prev = num2;
        ave1prev = ave1;
        ave2prev = ave2;
    }

        /* Next, for all contiguous scores within a specified fraction
         * of the max, choose the split point as the value with the
         * minimum in the histogram. */
    minscore = (1.f - scorefract) * maxscore;
    for (i = maxindex - 1; i >= 0; i--) {
        numaGetFValue(nascore, i, &val);
        if (val < minscore)
            break;
    }
    minrange = i + 1;
    for (i = maxindex + 1; i < n; i++) {
        numaGetFValue(nascore, i, &val);
        if (val < minscore)
            break;
    }
    maxrange = i - 1;
    numaGetFValue(na, minrange, &minval);
    bestsplit = minrange;
    for (i = minrange + 1; i <= maxrange; i++) {
        numaGetFValue(na, i, &val);
        if (val < minval) {
            minval = val;
            bestsplit = i;
        }
    }

        /* Add one to the bestsplit value to get the threshold value,
         * because when we take a threshold, as in pixThresholdToBinary(),
         * we always choose the set with values below the threshold. */
    bestsplit = L_MIN(255, bestsplit + 1);

    if (psplitindex) *psplitindex = bestsplit;
    if (pave1) numaGetFValue(naave1, bestsplit, pave1);
    if (pave2) numaGetFValue(naave2, bestsplit, pave2);
    if (pnum1) numaGetFValue(nanum1, bestsplit, pnum1);
    if (pnum2) numaGetFValue(nanum2, bestsplit, pnum2);

    if (pnascore) {  /* debug mode */
        lept_stderr("minrange = %d, maxrange = %d\n", minrange, maxrange);
        lept_stderr("minval = %10.0f\n", minval);
        gplotSimple1(nascore, GPLOT_PNG, "/tmp/lept/nascore",
                     "Score for split distribution");
        *pnascore = nascore;
    } else {
        numaDestroy(&nascore);
    }

    if (pave1) numaDestroy(&naave1);
    if (pave2) numaDestroy(&naave2);
    if (pnum1) numaDestroy(&nanum1);
    if (pnum2) numaDestroy(&nanum2);
    return 0;
}


/*----------------------------------------------------------------------*
 *                         Comparing histograms                         *
 *----------------------------------------------------------------------*/
/*!
 * \brief   grayHistogramsToEMD_Old()
 *
 * \param[in]    naa1, naa2    two numaa, each with one or more 256-element
 *                             histograms
 * \param[out]   pnad          nad of EM distances for each histogram
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *     (1) The two numaas must be the same size and have corresponding
 *         256-element histograms.  Pairs do not need to be normalized
 *         to the same sum.
 *     (2) This is typically used on two sets of histograms from
 *         corresponding tiles of two images.  The similarity of two
 *         images can be found with the scoring function used in
 *         pixCompareGrayByHisto():
 *             score S = 1.0 - k * D, where
 *                 k is a constant, say in the range 5-10
 *                 D = EMD
 *             for each tile; for multiple tiles, take the Min(S) over
 *             the set of tiles to be the final score.
 * </pre>
 */
l_ok
grayHistogramsToEMD_Old(NUMAA  *naa1,
                    NUMAA  *naa2,
                    NUMA  **pnad)
{
l_int32     i, n, nt;
l_float32   dist;
NUMA       *na1, *na2, *nad;

    if (!pnad)
        return ERROR_INT("&nad not defined", __func__, 1);
    *pnad = NULL;
    if (!naa1 || !naa2)
        return ERROR_INT("na1 and na2 not both defined", __func__, 1);
    n = numaaGetCount(naa1);
    if (n != numaaGetCount(naa2))
        return ERROR_INT("naa1 and naa2 numa counts differ", __func__, 1);
    nt = numaaGetNumberCount(naa1);
    if (nt != numaaGetNumberCount(naa2))
        return ERROR_INT("naa1 and naa2 number counts differ", __func__, 1);
    if (256 * n != nt)  /* good enough check */
        return ERROR_INT("na sizes must be 256", __func__, 1);

    nad = numaCreate(n);
    *pnad = nad;
    for (i = 0; i < n; i++) {
        na1 = numaaGetNuma(naa1, i, L_CLONE);
        na2 = numaaGetNuma(naa2, i, L_CLONE);
        numaEarthMoverDistance_Old(na1, na2, &dist);
        numaAddNumber(nad, dist / 255.f);  /* normalize to [0.0 - 1.0] */
        numaDestroy(&na1);
        numaDestroy(&na2);
    }
    return 0;
}


/*!
 * \brief   numaEarthMoverDistance_Old()
 *
 * \param[in]    na1, na2    two numas of the same size, typically histograms
 * \param[out]   pdist       earthmover distance
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *     (1) The two numas must have the same size.  They do not need to be
 *         normalized to the same sum before applying the function.
 *     (2) For a 1D discrete function, the implementation of the EMD
 *         is trivial.  Just keep filling or emptying buckets in one numa
 *         to match the amount in the other, moving sequentially along
 *         both arrays.
 *     (3) We divide the sum of the absolute value of everything moved
 *         (by 1 unit at a time) by the sum of the numa (amount of "earth")
 *         to get the average distance that the "earth" was moved.
 *         This is the value returned here.
 *     (4) The caller can do a further normalization, by the number of
 *         buckets (minus 1), to get the EM distance as a fraction of
 *         the maximum possible distance, which is n-1.  This fraction
 *         is 1.0 for the situation where all the 'earth' in the first
 *         array is at one end, and all in the second array is at the
 *         other end.
 * </pre>
 */
l_ok
numaEarthMoverDistance_Old(NUMA       *na1,
                       NUMA       *na2,
                       l_float32  *pdist)
{
l_int32     n, norm, i;
l_float32   sum1, sum2, diff, total;
l_float32  *array1, *array3;
NUMA       *na3;

    if (!pdist)
        return ERROR_INT("&dist not defined", __func__, 1);
    *pdist = 0.0;
    if (!na1 || !na2)
        return ERROR_INT("na1 and na2 not both defined", __func__, 1);
    n = numaGetCount(na1);
    if (n != numaGetCount(na2))
        return ERROR_INT("na1 and na2 have different size", __func__, 1);

        /* Generate na3; normalize to na1 if necessary */
    numaGetSum(na1, &sum1);
    numaGetSum(na2, &sum2);
    norm = (L_ABS(sum1 - sum2) < 0.00001 * L_ABS(sum1)) ? 1 : 0;
    if (!norm)
        na3 = numaTransform(na2, 0, sum1 / sum2);
    else
        na3 = numaCopy(na2);
    array1 = numaGetFArray(na1, L_NOCOPY);
    array3 = numaGetFArray(na3, L_NOCOPY);

        /* Move earth in n3 from array elements, to match n1 */
    total = 0;
    for (i = 1; i < n; i++) {
        diff = array1[i - 1] - array3[i - 1];
        array3[i] -= diff;
        total += L_ABS(diff);
    }
    *pdist = total / sum1;

    numaDestroy(&na3);
    return 0;
}


/*!
 * \brief   grayInterHistogramStats_Old()
 *
 * \param[in]    naa      numaa with two or more 256-element histograms
 * \param[in]    wc       half-width of the smoothing window
 * \param[out]   pnam     [optional] mean values
 * \param[out]   pnams    [optional] mean square values
 * \param[out]   pnav     [optional] variances
 * \param[out]   pnarv    [optional] rms deviations from the mean
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *     (1) The %naa has two or more 256-element numa histograms, which
 *         are to be compared value-wise at each of the 256 gray levels.
 *         The result are stats (mean, mean square, variance, root variance)
 *         aggregated across the set of histograms, and each is output
 *         as a 256 entry numa.  Think of these histograms as a matrix,
 *         where each histogram is one row of the array.  The stats are
 *         then aggregated column-wise, between the histograms.
 *     (2) These stats are:
 *            ~ average value: <v>  (nam)
 *            ~ average squared value: <v*v> (nams)
 *            ~ variance: <(v - <v>)*(v - <v>)> = <v*v> - <v>*<v>  (nav)
 *            ~ square-root of variance: (narv)
 *         where the brackets < .. > indicate that the average value is
 *         to be taken over each column of the array.
 *     (3) The input histograms are optionally smoothed before these
 *         statistical operations.
 *     (4) The input histograms are normalized to a sum of 10000.  By
 *         doing this, the resulting numbers are independent of the
 *         number of samples used in building the individual histograms.
 *     (5) A typical application is on a set of histograms from tiles
 *         of an image, to distinguish between text/tables and photo
 *         regions.  If the tiles are much larger than the text line
 *         spacing, text/table regions typically have smaller variance
 *         across tiles than photo regions.  For this application, it
 *         may be useful to ignore values near white, which are large for
 *         text and would magnify the variance due to variations in
 *         illumination.  However, because the variance of a drawing or
 *         a light photo can be similar to that of grayscale text, this
 *         function is only a discriminator between darker photos/drawings
 *         and light photos/text/line-graphics.
 * </pre>
 */
l_ok
grayInterHistogramStats_Old(NUMAA   *naa,
                        l_int32  wc,
                        NUMA   **pnam,
                        NUMA   **pnams,
                        NUMA   **pnav,
                        NUMA   **pnarv)
{
l_int32      i, j, n, nn;
l_float32  **arrays;
l_float32    mean, var, rvar;
NUMA        *na1, *na2, *na3, *na4;

    if (pnam) *pnam = NULL;
    if (pnams) *pnams = NULL;
    if (pnav) *pnav = NULL;
    if (pnarv) *pnarv = NULL;
    if (!pnam && !pnams && !pnav && !pnarv)
        return ERROR_INT("nothing requested", __func__, 1);
    if (!naa)
        return ERROR_INT("naa not defined", __func__, 1);
    n = numaaGetCount(naa);
    for (i = 0; i < n; i++) {
        nn = numaaGetNumaCount(naa, i);
        if (nn != 256) {
            L_ERROR("%d numbers in numa[%d]\n", __func__, nn, i);
            return 1;
        }
    }

    if (pnam) *pnam = numaCreate(256);
    if (pnams) *pnams = numaCreate(256);
    if (pnav) *pnav = numaCreate(256);
    if (pnarv) *pnarv = numaCreate(256);

        /* First, use mean smoothing, normalize each histogram,
         * and save all results in a 2D matrix. */
    arrays = (l_float32 **)LEPT_CALLOC(n, sizeof(l_float32 *));
    for (i = 0; i < n; i++) {
        na1 = numaaGetNuma(naa, i, L_CLONE);
        na2 = numaWindowedMean(na1, wc);
        na3 = numaNormalizeHistogram(na2, 10000.);
        arrays[i] = numaGetFArray(na3, L_COPY);
        numaDestroy(&na1);
        numaDestroy(&na2);
        numaDestroy(&na3);
    }

        /* Get stats between histograms */
    for (j = 0; j < 256; j++) {
        na4 = numaCreate(n);
        for (i = 0; i < n; i++) {
            numaAddNumber(na4, arrays[i][j]);
        }
        numaSimpleStats(na4, 0, -1, &mean, &var, &rvar);
        if (pnam) numaAddNumber(*pnam, mean);
        if (pnams) numaAddNumber(*pnams, mean * mean);
        if (pnav) numaAddNumber(*pnav, var);
        if (pnarv) numaAddNumber(*pnarv, rvar);
        numaDestroy(&na4);
    }

    for (i = 0; i < n; i++)
        LEPT_FREE(arrays[i]);
    LEPT_FREE(arrays);
    return 0;
}


/*----------------------------------------------------------------------*
 *                             Extrema finding                          *
 *----------------------------------------------------------------------*/
/*!
 * \brief   numaFindPeaks_Old()
 *
 * \param[in]    nas      source numa
 * \param[in]    nmax     max number of peaks to be found
 * \param[in]    fract1   min fraction of peak value
 * \param[in]    fract2   min slope
 * \return  peak na, or NULL on error.
 *
 * <pre>
 * Notes:
 *     (1) The returned na consists of sets of four numbers representing
 *         the peak, in the following order:
 *            left edge; peak center; right edge; normalized peak area
 * </pre>
 */
NUMA *
numaFindPeaks_Old(NUMA      *nas,
              l_int32    nmax,
              l_float32  fract1,
              l_float32  fract2)
{
l_int32    i, k, n, maxloc, lloc, rloc;
l_float32  fmaxval, sum, total, newtotal, val, lastval;
l_float32  peakfract;
NUMA      *na, *napeak;

    if (!nas)
        return (NUMA *)ERROR_PTR("nas not defined", __func__, NULL);
    n = numaGetCount(nas);
    numaGetSum(nas, &total);

        /* We munge this copy */
    if ((na = numaCopy(nas)) == NULL)
        return (NUMA *)ERROR_PTR("na not made", __func__, NULL);
    if ((napeak = numaCreate(4 * nmax)) == NULL) {
        numaDestroy(&na);
        return (NUMA *)ERROR_PTR("napeak not made", __func__, NULL);
    }

    for (k = 0; k < nmax; k++) {
        numaGetSum(na, &newtotal);
        if (newtotal == 0.0)   /* sanity check */
            break;
        numaGetMax(na, &fmaxval, &maxloc);
        sum = fmaxval;
        lastval = fmaxval;
        lloc = 0;
        for (i = maxloc - 1; i >= 0; --i) {
            numaGetFValue(na, i, &val);
            if (val == 0.0) {
                lloc = i + 1;
                break;
            }
            if (val > fract1 * fmaxval) {
                sum += val;
                lastval = val;
                continue;
            }
            if (lastval - val > fract2 * lastval) {
                sum += val;
                lastval = val;
                continue;
            }
            lloc = i;
            break;
        }
        lastval = fmaxval;
        rloc = n - 1;
        for (i = maxloc + 1; i < n; ++i) {
            numaGetFValue(na, i, &val);
            if (val == 0.0) {
                rloc = i - 1;
                break;
            }
            if (val > fract1 * fmaxval) {
                sum += val;
                lastval = val;
                continue;
            }
            if (lastval - val > fract2 * lastval) {
                sum += val;
                lastval = val;
                continue;
            }
            rloc = i;
            break;
        }
        peakfract = sum / total;
        numaAddNumber(napeak, lloc);
        numaAddNumber(napeak, maxloc);
        numaAddNumber(napeak, rloc);
        numaAddNumber(napeak, peakfract);

        for (i = lloc; i <= rloc; i++)
            numaSetValue(na, i, 0.0);
    }

    numaDestroy(&na);
    return napeak;
}


/*!
 * \brief   numaFindExtrema_Old()
 *
 * \param[in]    nas     input values
 * \param[in]    delta   relative amount to resolve peaks and valleys
 * \param[out]   pnav    [optional] values of extrema
 * \return  nad (locations of extrema, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This returns a sequence of extrema (peaks and valleys).
 *      (2) The algorithm is analogous to that for determining
 *          mountain peaks.  Suppose we have a local peak, with
 *          bumps on the side.  Under what conditions can we consider
 *          those 'bumps' to be actual peaks?  The answer: if the
 *          bump is separated from the peak by a saddle that is at
 *          least 500 feet below the bump.
 *      (3) Operationally, suppose we are trying to identify a peak.
 *          We have a previous valley, and also the largest value that
 *          we have seen since that valley.  We can identify this as
 *          a peak if we find a value that is delta BELOW it.  When
 *          we find such a value, label the peak, use the current
 *          value to label the starting point for the search for
 *          a valley, and do the same operation in reverse.  Namely,
 *          keep track of the lowest point seen, and look for a value
 *          that is delta ABOVE it.  Once found, the lowest point is
 *          labeled the valley, and continue, looking for the next peak.
 * </pre>
 */
NUMA *
numaFindExtrema_Old(NUMA      *nas,
                l_float32  delta,
                NUMA     **pnav)
{
l_int32    i, n, found, loc, direction;
l_float32  startval, val, maxval, minval;
NUMA      *nav, *nad;

    if (pnav) *pnav = NULL;
    if (!nas)
        return (NUMA *)ERROR_PTR("nas not defined", __func__, NULL);
    if (delta < 0.0)
        return (NUMA *)ERROR_PTR("delta < 0", __func__, NULL);

    n = numaGetCount(nas);
    nad = numaCreate(0);
    nav =  NULL;
    if (pnav) {
        nav = numaCreate(0);
        *pnav = nav;
    }

        /* We don't know if we'll find a peak or valley first,
         * but use the first element of nas as the reference point.
         * Break when we deviate by 'delta' from the first point. */
    numaGetFValue(nas, 0, &startval);
    found = FALSE;
    for (i = 1; i < n; i++) {
        numaGetFValue(nas, i, &val);
        if (L_ABS(val - startval) >= delta) {
            found = TRUE;
            break;
        }
    }

    if (!found)
        return nad;  /* it's empty */

        /* Are we looking for a peak or a valley? */
    if (val > startval) {  /* peak */
        direction = 1;
        maxval = val;
    } else {
        direction = -1;
        minval = val;
    }
    loc = i;

        /* Sweep through the rest of the array, recording alternating
         * peak/valley extrema. */
    for (i = i + 1; i < n; i++) {
        numaGetFValue(nas, i, &val);
        if (direction == 1 && val > maxval ) {  /* new local max */
            maxval = val;
            loc = i;
        } else if (direction == -1 && val < minval ) {  /* new local min */
            minval = val;
            loc = i;
        } else if (direction == 1 && (maxval - val >= delta)) {
            numaAddNumber(nad, loc);  /* save the current max location */
            if (nav) numaAddNumber(nav, maxval);
            direction = -1;  /* reverse: start looking for a min */
            minval = val;
            loc = i;  /* current min location */
        } else if (direction == -1 && (val - minval >= delta)) {
            numaAddNumber(nad, loc);  /* save the current min location */
            if (nav) numaAddNumber(nav, minval);
            direction = 1;  /* reverse: start looking for a max */
            maxval = val;
            loc = i;  /* current max location */
        }
    }

        /* Save the final extremum */
/*    numaAddNumber(nad, loc); */
    return nad;
}


/*!
 * \brief   numaFindLocForThreshold_Old()
 *
 * \param[in]    nas       input histogram
 * \param[in]    skip      look-ahead distance to avoid false mininma;
 *                         use 0 for default
 * \param[out]   pthresh   threshold value
 * \param[out]   pfract    [optional] fraction below or at threshold
 * \return  0 if OK, 1 on error or if no threshold can be found
 *
 * <pre>
 * Notes:
 *      (1) This finds a good place to set a threshold for a histogram
 *          of values that has two peaks.  The peaks can differ greatly
 *          in area underneath them.  The number of buckets in the
 *          histogram is expected to be 256 (e.g, from an 8 bpp gray image).
 *      (2) The input histogram should have been smoothed with a window
 *          to avoid false peak and valley detection due to noise.  For
 *          example, see pixThresholdByHisto().
 *      (3) A skip value can be input to determine the look-ahead distance
 *          to ignore a false peak on the rise or descent from the first peak.
 *          Input 0 to use the default value (it assumes a histo size of 256).
 *      (4) Optionally, the fractional area under the first peak can
 *          be returned.
 * </pre>
 */
l_ok
numaFindLocForThreshold_Old(NUMA       *na,
                        l_int32     skip,
                        l_int32    *pthresh,
                        l_float32  *pfract)
{
l_int32     i, n, start, index, minloc, found;
l_float32   val, pval, jval, minval, maxval, sum, partsum;
l_float32  *fa;

    if (pfract) *pfract = 0.0;
    if (!pthresh)
        return ERROR_INT("&thresh not defined", __func__, 1);
    *pthresh = 0;
    if (!na)
        return ERROR_INT("na not defined", __func__, 1);
    if (skip <= 0) skip = 20;

        /* Test for constant value */
    numaGetMin(na, &minval, NULL);
    numaGetMax(na, &maxval, NULL);
    if (minval == maxval)
       return ERROR_INT("all array values are the same", __func__, 1);

        /* Look for the top of the first peak */
    n = numaGetCount(na);
    if (n < 256)
        L_WARNING("array size %d < 256\n", __func__, n);
    fa = numaGetFArray(na, L_NOCOPY);
    pval = fa[0];
    for (i = 1; i < n; i++) {
        val = fa[i];
        index = L_MIN(i + skip, n - 1);
        jval = fa[index];
        if (val < pval && jval < pval)  /* near the top if not there */
            break;
        pval = val;
    }

    if (i > n - 5)  /* just an increasing function */
       return ERROR_INT("top of first peak not found", __func__, 1);

        /* Look for the low point in the valley */
    found = FALSE;
    start = i;
    pval = fa[start];
    for (i = start + 1; i < n; i++) {
        val = fa[i];
        if (val <= pval) {  /* appears to be going down */
            pval = val;
        } else {  /* appears to be going up */
            index = L_MIN(i + skip, n - 1);
            jval = fa[index];  /* junp ahead by 'skip' */
            if (val > jval) {  /* still going down; jump ahead */
                pval = jval;
                i = index;
            } else {  /* really going up; passed the min */
                found = TRUE;
                break;
            }
        }
    }
    if (!found)
       return ERROR_INT("no minimum found", __func__, 1);

        /* Find the location of the minimum in the interval */
    minloc = index;  /* likely passed the min; look backward */
    minval = fa[index];
    for (i = index - 1; i > index - skip; i--) {
        if (fa[i] < minval) {
            minval = fa[i];
            minloc = i;
        }
    }

        /* Is the minimum very near the end of the array? */
    if (minloc > n - 10)
       return ERROR_INT("minimum at end of array; invalid", __func__, 1);
    *pthresh = minloc;

        /* Find the fraction under the first peak */
    if (pfract) {
        numaGetSumOnInterval(na, 0, minloc, &partsum);
        numaGetSum(na, &sum);
        if (sum > 0.0)
           *pfract = partsum / sum;
    }
    return 0;
}


/*!
 * \brief   numaCountReversals_Old()
 *
 * \param[in]    nas          input values
 * \param[in]    minreversal  relative amount to resolve peaks and valleys
 * \param[out]   pnr          [optional] number of reversals
 * \param[out]   prd          [optional] reversal density: reversals/length
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The input numa can be generated from pixExtractAlongLine().
 *          If so, the x parameters can be used to find the reversal
 *          frequency along a line.
 *      (2) If the input numa was generated from a 1 bpp pix, the
 *          values will be 0 and 1.  Use %minreversal == 1 to get
 *          the number of pixel flips.  If the only values are 0 and 1,
 *          but %minreversal > 1, set the reversal count to 0 and
 *          issue a warning.
 * </pre>
 */
l_ok
numaCountReversals_Old(NUMA       *nas,
                   l_float32   minreversal,
                   l_int32    *pnr,
                   l_float32  *prd)
{
l_int32    i, n, nr, ival, binvals;
l_int32   *ia;
l_float32  fval, delx, len;
NUMA      *nat;

    if (pnr) *pnr = 0;
    if (prd) *prd = 0.0;
    if (!pnr && !prd)
        return ERROR_INT("neither &nr nor &rd are defined", __func__, 1);
    if (!nas)
        return ERROR_INT("nas not defined", __func__, 1);
    if ((n = numaGetCount(nas)) == 0) {
        L_INFO("nas is empty\n", __func__);
        return 0;
    }
    if (minreversal < 0.0)
        return ERROR_INT("minreversal < 0", __func__, 1);

        /* Decide if the only values are 0 and 1 */
    binvals = TRUE;
    for (i = 0; i < n; i++) {
        numaGetFValue(nas, i, &fval);
        if (fval != 0.0 && fval != 1.0) {
            binvals = FALSE;
            break;
        }
    }

    nr = 0;
    if (binvals) {
        if (minreversal > 1.0) {
            L_WARNING("binary values but minreversal > 1\n", __func__);
        } else {
            ia = numaGetIArray(nas);
            ival = ia[0];
            for (i = 1; i < n; i++) {
                if (ia[i] != ival) {
                    nr++;
                    ival = ia[i];
                }
            }
            LEPT_FREE(ia);
        }
    } else {
        nat = numaFindExtrema_Old(nas, minreversal, NULL);
        nr = numaGetCount(nat);
        numaDestroy(&nat);
    }
    if (pnr) *pnr = nr;
    if (prd) {
        numaGetParameters(nas, NULL, &delx);
        len = delx * n;
        *prd = (l_float32)nr / len;
    }

    return 0;
}


/*----------------------------------------------------------------------*
 *                Threshold crossings and frequency analysis            *
 *----------------------------------------------------------------------*/
/*!
 * \brief   numaSelectCrossingThreshold_Old()
 *
 * \param[in]    nax          [optional] numa of abscissa values; can be NULL
 * \param[in]    nay          signal
 * \param[in]    estthresh    estimated pixel threshold for crossing:
 *                            e.g., for images, white <--> black; typ. ~120
 * \param[out]   pbestthresh  robust estimate of threshold to use
 * \return  0 if OK, 1 on error or warning
 *
 * <pre>
 * Notes:
 *     (1) When a valid threshold is used, the number of crossings is
 *         a maximum, because none are missed.  If no threshold intersects
 *         all the crossings, the crossings must be determined with
 *         numaCrossingsByPeaks_Old().
 *     (2) %estthresh is an input estimate of the threshold that should
 *         be used.  We compute the crossings with 41 thresholds
 *         (20 below and 20 above).  There is a range in which the
 *         number of crossings is a maximum.  Return a threshold
 *         in the center of this stable plateau of crossings.
 *         This can then be used with numaCrossingsByThreshold_Old()
 *         to get a good estimate of crossing locations.
 *     (3) If the count of nay is less than 2, a warning is issued.
 * </pre>
 */
l_ok
numaSelectCrossingThreshold_Old(NUMA       *nax,
                            NUMA       *nay,
                            l_float32   estthresh,
                            l_float32  *pbestthresh)
{
l_int32    i, inrun, istart, iend, maxstart, maxend, runlen, maxrunlen;
l_int32    val, maxval, nmax, count;
l_float32  thresh, fmaxval, fmodeval;
NUMA      *nat, *nac;

    if (!pbestthresh)
        return ERROR_INT("&bestthresh not defined", __func__, 1);
    *pbestthresh = 0.0;
    if (!nay)
        return ERROR_INT("nay not defined", __func__, 1);
    if (numaGetCount(nay) < 2) {
        L_WARNING("nay count < 2; no threshold crossing\n", __func__);
        return 1;
    }

        /* Compute the number of crossings for different thresholds */
    nat = numaCreate(41);
    for (i = 0; i < 41; i++) {
        thresh = estthresh - 80.0f + 4.0f * i;
        nac = numaCrossingsByThreshold_Old(nax, nay, thresh);
        numaAddNumber(nat, numaGetCount(nac));
        numaDestroy(&nac);
    }

        /* Find the center of the plateau of max crossings, which
         * extends from thresh[istart] to thresh[iend]. */
    numaGetMax(nat, &fmaxval, NULL);
    maxval = (l_int32)fmaxval;
    nmax = 0;
    for (i = 0; i < 41; i++) {
        numaGetIValue(nat, i, &val);
        if (val == maxval)
            nmax++;
    }
    if (nmax < 3) {  /* likely accidental max; try the mode */
        numaGetMode(nat, &fmodeval, &count);
        if (count > nmax && fmodeval > 0.5 * fmaxval)
            maxval = (l_int32)fmodeval;  /* use the mode */
    }

    inrun = FALSE;
    iend = 40;
    maxrunlen = 0, maxstart = 0, maxend = 0;
    for (i = 0; i < 41; i++) {
        numaGetIValue(nat, i, &val);
        if (val == maxval) {
            if (!inrun) {
                istart = i;
                inrun = TRUE;
            }
            continue;
        }
        if (inrun && (val != maxval)) {
            iend = i - 1;
            runlen = iend - istart + 1;
            inrun = FALSE;
            if (runlen > maxrunlen) {
                maxstart = istart;
                maxend = iend;
                maxrunlen = runlen;
            }
        }
    }
    if (inrun) {
        runlen = i - istart;
        if (runlen > maxrunlen) {
            maxstart = istart;
            maxend = i - 1;
            maxrunlen = runlen;
        }
    }

    *pbestthresh = estthresh - 80.0f + 2.0f * (l_float32)(maxstart + maxend);

#if DEBUG_CROSSINGS
    lept_stderr("\nCrossings attain a maximum at %d thresholds, between:\n"
                "  thresh[%d] = %5.1f and thresh[%d] = %5.1f\n",
                nmax, maxstart, estthresh - 80.0 + 4.0 * maxstart,
                maxend, estthresh - 80.0 + 4.0 * maxend);
    lept_stderr("The best choice: %5.1f\n", *pbestthresh);
    lept_stderr("Number of crossings at the 41 thresholds:");
    numaWriteStderr(nat);
#endif  /* DEBUG_CROSSINGS */

    numaDestroy(&nat);
    return 0;
}


/*!
 * \brief   numaCrossingsByThreshold_Old()
 *
 * \param[in]    nax     [optional] numa of abscissa values; can be NULL
 * \param[in]    nay     numa of ordinate values, corresponding to nax
 * \param[in]    thresh  threshold value for nay
 * \return  nad abscissa pts at threshold, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) If nax == NULL, we use startx and delx from nay to compute
 *          the crossing values in nad.
 * </pre>
 */
NUMA *
numaCrossingsByThreshold_Old(NUMA      *nax,
                         NUMA      *nay,
                         l_float32  thresh)
{
l_int32    i, n;
l_float32  startx, delx;
l_float32  xval1, xval2, yval1, yval2, delta1, delta2, crossval, fract;
NUMA      *nad;

    if (!nay)
        return (NUMA *)ERROR_PTR("nay not defined", __func__, NULL);
    n = numaGetCount(nay);

    if (nax && (numaGetCount(nax) != n))
        return (NUMA *)ERROR_PTR("nax and nay sizes differ", __func__, NULL);

    nad = numaCreate(0);
    if (n < 2) return nad;
    numaGetFValue(nay, 0, &yval1);
    numaGetParameters(nay, &startx, &delx);
    if (nax)
        numaGetFValue(nax, 0, &xval1);
    else
        xval1 = startx;
    for (i = 1; i < n; i++) {
        numaGetFValue(nay, i, &yval2);
        if (nax)
            numaGetFValue(nax, i, &xval2);
        else
            xval2 = startx + i * delx;
        delta1 = yval1 - thresh;
        delta2 = yval2 - thresh;
        if (delta1 == 0.0) {
            numaAddNumber(nad, xval1);
        } else if (delta2 == 0.0) {
            numaAddNumber(nad, xval2);
        } else if (delta1 * delta2 < 0.0) {  /* crossing */
            fract = L_ABS(delta1) / L_ABS(yval1 - yval2);
            crossval = xval1 + fract * (xval2 - xval1);
            numaAddNumber(nad, crossval);
        }
        xval1 = xval2;
        yval1 = yval2;
    }

    return nad;
}


/*!
 * \brief   numaCrossingsByPeaks_Old()
 *
 * \param[in]    nax     [optional] numa of abscissa values
 * \param[in]    nay     numa of ordinate values, corresponding to nax
 * \param[in]    delta   parameter used to identify when a new peak can be found
 * \return  nad abscissa pts at threshold, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) If nax == NULL, we use startx and delx from nay to compute
 *          the crossing values in nad.
 * </pre>
 */
NUMA *
numaCrossingsByPeaks_Old(NUMA      *nax,
                     NUMA      *nay,
                     l_float32  delta)
{
l_int32    i, j, n, np, previndex, curindex;
l_float32  startx, delx;
l_float32  xval1, xval2, yval1, yval2, delta1, delta2;
l_float32  prevval, curval, thresh, crossval, fract;
NUMA      *nap, *nad;

    if (!nay)
        return (NUMA *)ERROR_PTR("nay not defined", __func__, NULL);

    n = numaGetCount(nay);
    if (nax && (numaGetCount(nax) != n))
        return (NUMA *)ERROR_PTR("nax and nay sizes differ", __func__, NULL);

        /* Find the extrema.  Also add last point in nay to get
         * the last transition (from the last peak to the end).
         * The number of crossings is 1 more than the number of extrema. */
    nap = numaFindExtrema_Old(nay, delta, NULL);
    numaAddNumber(nap, n - 1);
    np = numaGetCount(nap);
    L_INFO("Number of crossings: %d\n", __func__, np);

        /* Do all computation in index units of nax or the delx of nay */
    nad = numaCreate(np);  /* output crossing locations, in nax units */
    previndex = 0;  /* prime the search with 1st point */
    numaGetFValue(nay, 0, &prevval);  /* prime the search with 1st point */
    numaGetParameters(nay, &startx, &delx);
    for (i = 0; i < np; i++) {
        numaGetIValue(nap, i, &curindex);
        numaGetFValue(nay, curindex, &curval);
        thresh = (prevval + curval) / 2.0f;
        if (nax)
            numaGetFValue(nax, previndex, &xval1);
        else
            xval1 = startx + previndex * delx;
        numaGetFValue(nay, previndex, &yval1);
        for (j = previndex + 1; j <= curindex; j++) {
            if (nax)
                numaGetFValue(nax, j, &xval2);
            else
                xval2 = startx + j * delx;
            numaGetFValue(nay, j, &yval2);
            delta1 = yval1 - thresh;
            delta2 = yval2 - thresh;
            if (delta1 == 0.0) {
                numaAddNumber(nad, xval1);
                break;
            } else if (delta2 == 0.0) {
                numaAddNumber(nad, xval2);
                break;
            } else if (delta1 * delta2 < 0.0) {  /* crossing */
                fract = L_ABS(delta1) / L_ABS(yval1 - yval2);
                crossval = xval1 + fract * (xval2 - xval1);
                numaAddNumber(nad, crossval);
                break;
            }
            xval1 = xval2;
            yval1 = yval2;
        }
        previndex = curindex;
        prevval = curval;
    }

    numaDestroy(&nap);
    return nad;
}


/*!
 * \brief   numaEvalBestHaarParameters_Old()
 *
 * \param[in]    nas         numa of non-negative signal values
 * \param[in]    relweight   relative weight of (-1 comb) / (+1 comb)
 *                           contributions to the 'convolution'.  In effect,
 *                           the convolution kernel is a comb consisting of
 *                           alternating +1 and -weight.
 * \param[in]    nwidth      number of widths to consider
 * \param[in]    nshift      number of shifts to consider for each width
 * \param[in]    minwidth    smallest width to consider
 * \param[in]    maxwidth    largest width to consider
 * \param[out]   pbestwidth  width giving largest score
 * \param[out]   pbestshift  shift giving largest score
 * \param[out]   pbestscore  [optional] convolution with "Haar"-like comb
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This does a linear sweep of widths, evaluating at %nshift
 *          shifts for each width, computing the score from a convolution
 *          with a long comb, and finding the (width, shift) pair that
 *          gives the maximum score.  The best width is the "half-wavelength"
 *          of the signal.
 *      (2) The convolving function is a comb of alternating values
 *          +1 and -1 * relweight, separated by the width and phased by
 *          the shift.  This is similar to a Haar transform, except
 *          there the convolution is performed with a square wave.
 *      (3) The function is useful for finding the line spacing
 *          and strength of line signal from pixel sum projections.
 *      (4) The score is normalized to the size of nas divided by
 *          the number of half-widths.  For image applications, the input is
 *          typically an array of pixel projections, so one should
 *          normalize by dividing the score by the image width in the
 *          pixel projection direction.
 * </pre>
 */
l_ok
numaEvalBestHaarParameters_Old(NUMA       *nas,
                           l_float32   relweight,
                           l_int32     nwidth,
                           l_int32     nshift,
                           l_float32   minwidth,
                           l_float32   maxwidth,
                           l_float32  *pbestwidth,
                           l_float32  *pbestshift,
                           l_float32  *pbestscore)
{
l_int32    i, j;
l_float32  delwidth, delshift, width, shift, score;
l_float32  bestwidth, bestshift, bestscore;

    if (pbestscore) *pbestscore = 0.0;
    if (pbestwidth) *pbestwidth = 0.0;
    if (pbestshift) *pbestshift = 0.0;
    if (!pbestwidth || !pbestshift)
        return ERROR_INT("&bestwidth and &bestshift not defined", __func__, 1);
    if (!nas)
        return ERROR_INT("nas not defined", __func__, 1);

    bestscore = bestwidth = bestshift = 0.0;
    delwidth = (maxwidth - minwidth) / (nwidth - 1.0f);
    for (i = 0; i < nwidth; i++) {
        width = minwidth + delwidth * i;
        delshift = width / (l_float32)(nshift);
        for (j = 0; j < nshift; j++) {
            shift = j * delshift;
            numaEvalHaarSum_Old(nas, width, shift, relweight, &score);
            if (score > bestscore) {
                bestscore = score;
                bestwidth = width;
                bestshift = shift;
#if DEBUG_FREQUENCY
                lept_stderr("width = %7.3f, shift = %7.3f, score = %7.3f\n",
                            width, shift, score);
#endif  /* DEBUG_FREQUENCY */
            }
        }
    }

    *pbestwidth = bestwidth;
    *pbestshift = bestshift;
    if (pbestscore)
        *pbestscore = bestscore;
    return 0;
}


/*!
 * \brief   numaEvalHaarSum_Old()
 *
 * \param[in]    nas         numa of non-negative signal values
 * \param[in]    width       distance between +1 and -1 in convolution comb
 * \param[in]    shift       phase of the comb: location of first +1
 * \param[in]    relweight   relative weight of (-1 comb) / (+1 comb)
 *                           contributions to the 'convolution'.  In effect,
 *                           the convolution kernel is a comb consisting of
 *                           alternating +1 and -weight.
 * \param[out]   pscore      convolution with "Haar"-like comb
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This does a convolution with a comb of alternating values
 *          +1 and -relweight, separated by the width and phased by the shift.
 *          This is similar to a Haar transform, except that for Haar,
 *            (1) the convolution kernel is symmetric about 0, so the
 *                relweight is 1.0, and
 *            (2) the convolution is performed with a square wave.
 *      (2) The score is normalized to the size of nas divided by
 *          twice the "width".  For image applications, the input is
 *          typically an array of pixel projections, so one should
 *          normalize by dividing the score by the image width in the
 *          pixel projection direction.
 *      (3) To get a Haar-like result, use relweight = 1.0.  For detecting
 *          signals where you expect every other sample to be close to
 *          zero, as with barcodes or filtered text lines, you can
 *          use relweight > 1.0.
 * </pre>
 */
l_ok
numaEvalHaarSum_Old(NUMA       *nas,
                l_float32   width,
                l_float32   shift,
                l_float32   relweight,
                l_float32  *pscore)
{
l_int32    i, n, nsamp, index;
l_float32  score, weight, val;

    if (!pscore)
        return ERROR_INT("&score not defined", __func__, 1);
    *pscore = 0.0;
    if (!nas)
        return ERROR_INT("nas not defined", __func__, 1);
    if ((n = numaGetCount(nas)) < 2 * width)
        return ERROR_INT("nas size too small", __func__, 1);

    score = 0.0;
    nsamp = (l_int32)((n - shift) / width);
    for (i = 0; i < nsamp; i++) {
        index = (l_int32)(shift + i * width);
        weight = (i % 2) ? 1.0f : -1.0f * relweight;
        numaGetFValue(nas, index, &val);
        score += weight * val;
    }

    *pscore = 2.0f * width * score / (l_float32)n;
    return 0;
}


/*----------------------------------------------------------------------*
 *            Generating numbers in a range under constraints           *
 *----------------------------------------------------------------------*/
/*!
 * \brief   genConstrainedNumaInRange_Old()
 *
 * \param[in]    first     first number to choose; >= 0
 * \param[in]    last      biggest possible number to reach; >= first
 * \param[in]    nmax      maximum number of numbers to select; > 0
 * \param[in]    use_pairs 1 = select pairs of adjacent numbers;
 *                         0 = select individual numbers
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *     (1) Selection is made uniformly in the range.  This can be used
 *         to select pages distributed as uniformly as possible
 *         through a book, where you are constrained to:
 *          ~ choose between [first, ... biggest],
 *          ~ choose no more than nmax numbers, and
 *         and you have the option of requiring pairs of adjacent numbers.
 * </pre>
 */
NUMA *
genConstrainedNumaInRange_Old(l_int32  first,
                          l_int32  last,
                          l_int32  nmax,
                          l_int32  use_pairs)
{
l_int32    i, nsets, val;
l_float32  delta;
NUMA      *na;

    first = L_MAX(0, first);
    if (last < first)
        return (NUMA *)ERROR_PTR("last < first!", __func__, NULL);
    if (nmax < 1)
        return (NUMA *)ERROR_PTR("nmax < 1!", __func__, NULL);

    nsets = L_MIN(nmax, last - first + 1);
    if (use_pairs == 1)
        nsets = nsets / 2;
    if (nsets == 0)
        return (NUMA *)ERROR_PTR("nsets == 0", __func__, NULL);

        /* Select delta so that selection covers the full range if possible */
    if (nsets == 1) {
        delta = 0.0;
    } else {
        if (use_pairs == 0)
            delta = (l_float32)(last - first) / (nsets - 1);
        else
            delta = (l_float32)(last - first - 1) / (nsets - 1);
    }

    na = numaCreate(nsets);
    for (i = 0; i < nsets; i++) {
        val = (l_int32)(first + i * delta + 0.5);
        numaAddNumber(na, val);
        if (use_pairs == 1)
            numaAddNumber(na, val + 1);
    }

    return na;
}

