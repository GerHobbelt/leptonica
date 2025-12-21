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
 *   reg_test_loop_test.c
 *
 */

#include "demo_settings.h"

#include "monolithic_examples.h"



#if defined(BUILD_MONOLITHIC)
#define main   lept_reg_test_loop_test_main
#endif

int main(int          argc,
		 const char** argv)
{
	L_REGPARAMS* rp;
	nanotimer_data_t time;

	if (regTestSetup(argc, argv, "reg_loop_test", NULL, &rp))
		return 1;

	nanotimer(&time);

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
			const char* destdir = leptDebugGenFilepath("");
			char* real_destdir = genPathname(destdir, NULL);
			lept_stderr("\n\n\nProcessing image #%d~#%s:\n  %s :: %s.<output>\n    --> %s.<output>\n", regGetCurrentArgIndex(rp), leptDebugGetStepIdAsString(), filepath, destdir, real_destdir);
			stringDestroy(&real_destdir);
		}

		l_int32 exists = 0;
		lept_file_exists(filepath, &exists);
		if (!exists) {
			L_ERROR("file does not exist: %s\n", __func__, filepath);
		}

		const char* source_fname = pathExtractTail(filepath, +2);
		L_INFO("Done: %s\n", __func__, source_fname);

		stringDestroy(&source_fname);

		leptDebugClearLastGenFilepathCache();
	}

	//leptDebugPopStepLevelTo(rp->base_step_level);

	nanotimer_destroy(&time);

	return regTestCleanup(rp);
}
