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


#if defined(BUILD_MONOLITHIC)
#define main   lept_thresholding_test_main
#endif

int main(int    argc,
         const char **argv)
{
L_REGPARAMS  *rp;

    if (regTestSetup(argc, argv, &rp))
        return 1;

	lept_rmdir("lept/bmp-test");
	lept_mkdir("lept/bmp-test");

	PIX* pix1 = pixRead(DEMOPATH("bmp_format2.png"));
	l_ok ret = pixWrite("/tmp/lept/bmp-test/target-png.bmp", pix1, IFF_BMP);
	pixDestroy(&pix1);

	pix1 = pixRead(DEMOPATH("bmp_format2.bmp"));
	ret |= pixWrite("/tmp/lept/bmp-test/target-bmp.bmp", pix1, IFF_BMP);
	pixDestroy(&pix1);

	PIX *pix = pixReadWithHint(DEMOPATH("test-rgba.bmp"), IFF_BMP);
	ret |= pixWrite("/tmp/lept/bmp-test/target-rgba1.bmp", pix, IFF_BMP);
	pixDestroy(&pix);

	pix = pixRead(DEMOPATH("test-rgba.bmp"));
	ret |= pixWrite("/tmp/lept/bmp-test/target-rgba2.bmp", pix, IFF_BMP);
	ret |= pixWrite("/tmp/lept/bmp-test/target-rgba2.png", pix, IFF_PNG);

	PIX *pix3 = pixRead(DEMOPATH("target.bmp"));
	int d = pixGetDepth(pix3);
	assert(d == 32);
	int spp = pixGetSpp(pix3);
	assert(spp == 3);
	ret |= pixWrite("/tmp/lept/bmp-test/target-rgba3.bmp", pix3, IFF_BMP);
	ret |= pixWrite("/tmp/lept/bmp-test/target-rgba3.png", pix3, IFF_PNG);

	d = pixGetDepth(pix);
	assert(d == 32);
	spp = pixGetSpp(pix);
	assert(spp == 4);
	PIX* pix2 = pixConvert32To24(pix);
	d = pixGetDepth(pix2);
	assert(d == 24);
	spp = pixGetSpp(pix2);
	assert(spp == 3);
	ret |= pixWrite("/tmp/lept/bmp-test/target-rgba24.bmp", pix2, IFF_BMP);
	ret |= pixWrite("/tmp/lept/bmp-test/target-rgba24.png", pix2, IFF_PNG);

	pixDestroy(&pix);
	pixDestroy(&pix2);
	pixDestroy(&pix3);
	return regTestCleanup(rp) || ret;
}
