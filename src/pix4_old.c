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
 * \file pix4_old.c
 * <pre>
 *
 *    This file has these operations:
 *
 *      (1) Foreground/background estimation
 *
 *    Foreground/background estimation
 *           l_int32     pixSplitDistributionFgBg_Old()
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <string.h>
#include <math.h>
#include "allheaders.h"


/*------------------------------------------------------------------*
 *                  Pixel histogram and averaging                   *
 *------------------------------------------------------------------*/


/*!
 * \brief   pixSplitDistributionFgBg_Old()
 *
 * \param[in]    pixs        any depth; cmapped ok
 * \param[in]    scorefract  fraction of the max score, used to determine
 *                           the range over which the histogram min is searched
 * \param[in]    factor      subsampling factor; integer >= 1
 * \param[out]   pthresh     [optional] best threshold for separating
 * \param[out]   pfgval      [optional] average foreground value
 * \param[out]   pbgval      [optional] average background value
 * \param[out]   ppixdb      [optional] plot of distribution and split point
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) See numaSplitDistribution() for details on the underlying
 *          method of choosing a threshold.
 * </pre>
 */
l_ok
pixSplitDistributionFgBg_Old(PIX       *pixs,
                         l_float32  scorefract,
                         l_int32    factor,
                         l_int32   *pthresh,
                         l_int32   *pfgval,
                         l_int32   *pbgval,
                         PIX      **ppixdb)
{
char       buf[256];
l_int32    thresh;
l_float32  avefg, avebg, maxnum;
GPLOT     *gplot;
NUMA      *na, *nascore, *nax, *nay;
PIX       *pixg;

    if (pthresh) *pthresh = 0;
    if (pfgval) *pfgval = 0;
    if (pbgval) *pbgval = 0;
    if (ppixdb) *ppixdb = NULL;
    if (!pthresh && !pfgval && !pbgval)
        return ERROR_INT("no data requested", __func__, 1);
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);

        /* Generate a subsampled 8 bpp version */
    pixg = pixConvertTo8BySampling(pixs, factor, 0);

        /* Make the fg/bg estimates */
    na = pixGetGrayHistogram(pixg, 1);
    if (ppixdb) {
        numaSplitDistribution(na, scorefract, &thresh, &avefg, &avebg,
                              NULL, NULL, &nascore);
        numaDestroy(&nascore);
    } else {
        numaSplitDistribution(na, scorefract, &thresh, &avefg, &avebg,
                              NULL, NULL, NULL);
    }

    if (pthresh) *pthresh = thresh;
    if (pfgval) *pfgval = (l_int32)(avefg + 0.5);
    if (pbgval) *pbgval = (l_int32)(avebg + 0.5);

    if (ppixdb) {
        lept_mkdir("lept/redout");
        gplot = gplotCreate("/tmp/lept/redout/histplot", GPLOT_PNG, "Histogram",
                            "Grayscale value", "Number of pixels");
        gplotAddPlot(gplot, NULL, na, GPLOT_LINES, NULL);
        nax = numaMakeConstant(thresh, 2);
        numaGetMax(na, &maxnum, NULL);
        nay = numaMakeConstant(0, 2);
        numaReplaceNumber(nay, 1, (l_int32)(0.5 * maxnum));
        snprintf(buf, sizeof(buf), "score fract = %3.1f", scorefract);
        gplotAddPlot(gplot, nax, nay, GPLOT_LINES, buf);
        *ppixdb = gplotMakeOutputPix(gplot);
        gplotDestroy(&gplot);
        numaDestroy(&nax);
        numaDestroy(&nay);
    }

    pixDestroy(&pixg);
    numaDestroy(&na);
    return 0;
}
