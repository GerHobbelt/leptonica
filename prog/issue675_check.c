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
#define main   lept_issue675_check_main
#endif

int main(int    argc,
         const char **argv)
{
L_REGPARAMS  *rp;
PIX* pix[20] = { NULL };
l_ok ret = 0;
l_int32 same;

    if (regTestSetup(argc, argv, &rp))
        return 1;

	lept_rmdir("lept/bmp-test");
	lept_mkdir("lept/bmp-test");

	pix[1] = pixRead(DEMOPATH("bmp_format2.png"));
	ret = pixWrite("/tmp/lept/bmp-test/target-png.bmp", pix[1], IFF_BMP);

	pix[2] = pixRead(DEMOPATH("bmp_format2.bmp"));
	ret |= pixWrite("/tmp/lept/bmp-test/target-bmp.bmp", pix[2], IFF_BMP);

	pix[3] = pixRead("/tmp/lept/bmp-test/target-png.bmp");
	pix[4] = pixRead("/tmp/lept/bmp-test/target-bmp.bmp");
	pixEqual(pix[3], pix[1], &same);
	lept_stderr("bmp_format2: PNG->BMP: same: %d\n", same);
	pixEqual(pix[4], pix[2], &same);
	lept_stderr("bmp_format2: BMP->BMP: same: %d\n", same);

	//-------------------------------------

	pix[5] = pixReadWithHint(DEMOPATH("test-rgba.bmp"), IFF_BMP);
	ret |= pixWrite("/tmp/lept/bmp-test/target-rgba1.bmp", pix[5], IFF_BMP);

	pix[6] = pixRead(DEMOPATH("test-rgba.bmp"));
	ret |= pixWrite("/tmp/lept/bmp-test/target-rgba2.bmp", pix[6], IFF_BMP);
	ret |= pixWrite("/tmp/lept/bmp-test/target-rgba2.png", pix[6], IFF_PNG);

	pix[7] = pixRead("/tmp/lept/bmp-test/target-rgba1.bmp");
	pix[8] = pixRead("/tmp/lept/bmp-test/target-rgba2.bmp");
	pix[9] = pixRead("/tmp/lept/bmp-test/target-rgba2.png");
	pixEqual(pix[7], pix[5], &same);
	lept_stderr("test_rgba: BMP 1: .... same: %d\n", same);
	pixEqual(pix[8], pix[6], &same);
	lept_stderr("test_rgba: BMP 2A: ... same: %d\n", same);
	pixEqual(pix[9], pix[6], &same);
	lept_stderr("test_rgba: BMP 2B: ... same: %d\n", same);

	pixEqual(pix[6], pix[5], &same);
	lept_stderr("test_rgba: BMP 1 source vs. BMP 2 source: same: %d\n", same);

	//-------------------------------------

	pix[10] = pixRead(DEMOPATH("target.bmp"));
	int d = pixGetDepth(pix[10]);
	assert(d == 32);
	int spp = pixGetSpp(pix[10]);
	assert(spp == 3);
	ret |= pixWrite("/tmp/lept/bmp-test/target-rgba3.bmp", pix[10], IFF_BMP);
	ret |= pixWrite("/tmp/lept/bmp-test/target-rgba3.png", pix[10], IFF_PNG);

	d = pixGetDepth(pix[5]);
	assert(d == 32);
	spp = pixGetSpp(pix[5]);
	assert(spp == 4);
	pix[11] = pixConvert32To24(pix[5]);
	d = pixGetDepth(pix[11]);
	assert(d == 24);
	spp = pixGetSpp(pix[11]);
	assert(spp == 3);
	ret |= pixWrite("/tmp/lept/bmp-test/target-rgba24.bmp", pix[11], IFF_BMP);
	ret |= pixWrite("/tmp/lept/bmp-test/target-rgba24.png", pix[11], IFF_PNG);

	pix[12] = pixEndianByteSwapNew(pix[5]);
	d = pixGetDepth(pix[12]);
	assert(d == 32);
	spp = pixGetSpp(pix[12]);
	assert(spp == 4);
	ret |= pixWrite("/tmp/lept/bmp-test/target-rgba24BE.bmp", pix[12], IFF_BMP);
	ret |= pixWrite("/tmp/lept/bmp-test/target-rgba24BE.png", pix[12], IFF_PNG);

	pix[13] = pixRead("/tmp/lept/bmp-test/target-rgba3.bmp");
	pix[14] = pixRead("/tmp/lept/bmp-test/target-rgba3.png");
	pixEqual(pix[13], pix[10], &same);
	lept_stderr("target: BMP 1A: ...... same: %d\n", same);
	pixEqual(pix[14], pix[10], &same);
	lept_stderr("target: BMP 1B: ...... same: %d\n", same);

	pix[15] = pixRead("/tmp/lept/bmp-test/target-rgba24.bmp");
	pix[16] = pixRead("/tmp/lept/bmp-test/target-rgba24.png");
	pixEqual(pix[15], pix[5], &same);
	lept_stderr("target: BMP 24A: ..... same: %d\n", same);
	pixEqual(pix[16], pix[5], &same);
	lept_stderr("target: BMP 24B: ..... same: %d\n", same);

	pix[17] = pixRead("/tmp/lept/bmp-test/target-rgba24BE.bmp");
	pix[18] = pixRead("/tmp/lept/bmp-test/target-rgba24BE.png");
	pixEqual(pix[17], pix[5], &same);
	lept_stderr("target: BMP BE1: ..... same: %d (known bad)\n", same);
	pixEqual(pix[18], pix[5], &same);
	lept_stderr("target: BMP BE2: ..... same: %d (known bad)\n", same);

	// blunt release of all pix's: this short code is why we NULL them at declaration.
	for (int i = 0; i < sizeof(pix) / sizeof(pix[0]); i++)
		pixDestroy(&pix[i]);

	return regTestCleanup(rp) || ret;
}
