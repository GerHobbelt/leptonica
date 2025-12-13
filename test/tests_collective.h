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

#pragma once

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#ifdef _MSC_VER
#ifndef _CRTDBG_MAP_ALLOC
#define _CRTDBG_MAP_ALLOC
#endif
#include <crtdbg.h>
#include <process.h>
#include <direct.h>
#ifndef getcwd
#define getcwd _getcwd  /* fix MSVC warning */
#endif
#else
#include <unistd.h>
#endif   /* _MSC_VER */

#ifdef _WIN32
// MSWin fix: make sure we include winsock2.h *before* windows.h implicitly includes the antique winsock.h and causes all kinds of weird errors at compile time:
#include <winsock2.h>
#include <windows.h>
#include <fcntl.h>     /* _O_CREAT, ... */
#include <io.h>        /* _open */
#include <sys/stat.h>  /* _S_IREAD, _S_IWRITE */
#  define strcasecmp _stricmp
#  define strncasecmp _strnicmp
#else
#include <strings.h>
#include <sys/stat.h>  /* for stat, mkdir(2) */
#include <sys/types.h>
#endif

#ifdef __APPLE__
#include <unistd.h>
#endif

#include <errno.h>     /* for errno */
#include <string.h>
#include <stddef.h>

#include <gtest/gtest.h>

#include "allheaders.h"





#if defined(_CRTDBG_MAP_ALLOC)

class HeapLeakTestFixture: public testing::Test {
	int tmpHeapDbgFlag;

	_CrtMemState hs1;

protected:
	virtual void SetUp() override
	{
		tmpHeapDbgFlag = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
		_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF | _CRTDBG_DELAY_FREE_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_CHECK_CRT_DF);
#if 01
		_CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
		_CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
		_CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#else
		_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG);
		_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);
		_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_DEBUG);
#endif

		// _CrtSetReportHook[[W]2]:
		// For IJW, we need two versions:  one for clrcall and one for cdecl.
		// For pure and native, we just need clrcall and cdecl, respectively.
		// 
		//_CRT_REPORT_HOOK hook1 = _CrtSetReportHook(crtdbg_report_hook_func);
		_CrtSetReportHook2(_CRT_RPTHOOK_INSTALL, crtdbg_report_hook_func);
		_CrtSetReportHookW2(_CRT_RPTHOOK_INSTALL, crtdbgw_report_hook_func);

		_CrtMemCheckpoint(&hs1);
	}

	virtual void TearDown() override
	{
		_CrtMemState hs2;
		_CrtMemCheckpoint(&hs2);

		_CrtMemState hs3;
		if (_CrtMemDifference(&hs3, &hs1, &hs2)) {
			_CrtMemDumpStatistics(&hs3);
			GTEST_EXPECT_TRUE_W_MSG(!&_CrtMemDifference, "heap memory leakage detected!");
			GTEST_ASSERT_EQ(::testing::Test::HasFailure(), true);

			_CrtMemDumpAllObjectsSince(&hs1);
		}

#if 0
		_CrtMemDumpAllObjectsSince(&hs1);

		_CrtDumpMemoryLeaks();
#endif

		_CrtSetReportHook2(_CRT_RPTHOOK_REMOVE, crtdbg_report_hook_func);
		_CrtSetReportHookW2(_CRT_RPTHOOK_REMOVE, crtdbgw_report_hook_func);

		_CrtSetDbgFlag(tmpHeapDbgFlag);
	}

	static int crtdbg_report_hook_func(int reportType, char *message, int *returnValue)
	{
		switch (reportType) {
		default:
			reportType = _CRT_ASSERT + 1;
			[[fallthrough]];
		case _CRT_WARN:
		case _CRT_ERROR:
		case _CRT_ASSERT:
			static const char *typestr[4] = {
				"WARNING",
				"ERROR  ",
				"ASSERT ",
				"FAIL   "
			};
			// regrettably gtest's ColoredPrintf isn't available as that is a googletest *internal* class. *Dang!*
			printf("%s: %s", typestr[reportType], message);
			break;
		}
		return FALSE;
	}

	static int crtdbgw_report_hook_func(int reportType, wchar_t *message, int *returnValue)
	{
		switch (reportType) {
		case _CRT_WARN:
		case _CRT_ERROR:
		case _CRT_ASSERT:
		default:
			break;
		}
		return FALSE;
	}
};

#else

class HeapLeakTestFixture: public testing::Test {

protected:
	virtual void SetUp() override
	{
	}

	virtual void TearDown() override
	{
	}
};

#endif

#define TEST_FIXTURE(test_fixture, test_name, test_tag) \
  GTEST_TEST_F_C(test_fixture, "S", test_name ## _ ## test_tag, test_tag)



template <typename T>
class HeapLeakParameterizedTestFixture: public testing::Test, public testing::WithParamInterface<T> {
public:
	virtual ~HeapLeakParameterizedTestFixture() = default;
};

