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

#include "tests_collective.h"

#include <format>
#include <string>
#include <ostream>


struct PathJoinTestData {
	const char *in_p1 = nullptr;
	const char *in_p2 = nullptr;
	const char *out_expected = nullptr;
	const char *out_safe_expected = nullptr;
	bool err_report = false;
	bool safe_err_report = false;
};

static inline const char *str_or_null(const char *str)
{
	if (str)
		return str;
	return "(nullptr)";
}

// see gtest-printers.h
void PrintTo(const PathJoinTestData& value, ::std::ostream* os) {
	*os << std::format("(\"{}\", \"{}\"      -->  joined: \"{}\" [{}] / safe: \"{}\" [{}])",
		str_or_null(value.in_p1),
		str_or_null(value.in_p2),
		str_or_null(value.out_expected),
		value.err_report ? "ERR" : "OK",
		str_or_null(value.out_safe_expected),
		value.safe_err_report ? "ERR" : "OK"
		);
}


using PathJoinTestFixture = HeapLeakParameterizedTestFixture<PathJoinTestData>;

// instantiate the static members...
leptStderrHandler_f HeapLeakTestFixture::lept_old_err_handler = nullptr;
int HeapLeakTestFixture::error_count = 0;



static const PathJoinTestData testdata[] = {
	{ "Z:/abc", "../../x", "Z:/x", "Z:/abc/x", true, true }, // NOT "/x" NOR "x".
	{ "/tmp/abc", "../../x", "/tmp/x", "/tmp/abc/x", true, true },
	{ "/abc/def", "../../x", "/x", "/abc/def/x", false, true },
	{ "/abc/", "Z:/x/y", "/abc/drv_Z/x/y", "/abc/drv_Z/x/y", true, true },
	{ "/abc/", "//?/Z:/x/y", "/abc/drv_Z/x/y", "/abc/drv_Z/x/y", true, true },
	{ "/abc/", "//?/$Server/$Share/x/y", "/abc/drv_Server_Share_FT/x/y", "/abc/drv_Server_Share_FT/x/y", true, true },
	{ "//tmp//", "//abc/", "/tmp/abc", "/tmp/abc", true, true },
	{ "tmp/", "/abc/", "tmp/abc", "tmp/abc", true, true },
	{ "tmp/", "abc/", "tmp/abc", "tmp/abc" },
	{ "/tmp/", "///", "/tmp", "/tmp", true, true },
	{ "/tmp/", nullptr, "/tmp", "/tmp" },
	{ "//", "/abc//", "/abc", "/abc", true, true },
	{ "//", nullptr, "/", "/" },
	{ nullptr, "/abc/def/", "/abc/def", "abc/def", false, true },
	{ nullptr, "abc//", "abc", "abc" },
	{ nullptr, "//", "/", "", false, true },
	{ nullptr, nullptr, "", "" },
	{ "", "", "", "" },
	{ "", "/", "/", "", false, true },
	{ "..", "/etc/foo", "../etc/foo", "../etc/foo", true, true },
	{ "/tmp", "..", "/tmp", "/tmp", true, true },
	{ "..", "abc/def", "../abc/def", "../abc/def" },
	{ "abc", "..", "", "abc", false, true },
	{ "abc/def", "..", "abc", "abc/def", false, true },
	{ "a/b/c", "../../d/e", "a/d/e", "a/b/c/d/e", false, true },
	{ "/a/b/c", "d/../../../../../e", "/e", "/a/b/c/e", true, true },
	{ "/a/b/c", "d/../../../../e", "/e", "/a/b/c/e", false, true },
	{ "/tmp/a/b/c", "d/../../../../../e", "/tmp/e", "/tmp/a/b/c/e", true, true },
	{ "/a/b/c", "/d/e", "/a/b/c/d/e", "/a/b/c/d/e", true, true },
	{ nullptr, "/d/e", "/d/e", "d/e", false, true },

	// extra: other UNIX top level system directories are 'unescapable', just like '/tmp/':
	{ "/dev/a", "../..", "/dev", "/dev/a", true, true },
	{ "/sys/a", "../..", "/sys", "/sys/a", true, true },
	{ "/var/a", "../..", "/var", "/var/a", true, true },
	{ "/etc/a", "../..", "/etc", "/etc/a", true, true },
	{ "/usr/a", "../..", "/usr", "/usr/a", true, true },

	// extra: './' paths resolution, etc.; misc. nastiness...
	{ "./a", "./b", "a/b", "a/b" },
	{ nullptr, "./b", "b", "b" },
	{ ".", nullptr, ".", "." },
	{ nullptr, ".", ".", "." },
	{ "..", nullptr, "..", ".." },
	{ nullptr, "..", "..", "", false, true },
	// now for some nastiness: messing around in %dir won't get you an error report...
	{ "../a/../../b", nullptr, "../../b", "../../b", false, false },
	// ...while trying to pull off the same in %fname will at least get you an error report as you're attempting to cross the boundary using extra '../' element.
	{ nullptr, "../a/../../b", "../../b", "b", false, true },
	// what about a multiple-'../' attack?
	{ "../a/b/../../../../../c", nullptr, "../../../../c", "../../../../c", false, false },
	{ nullptr, "../a/b/../../../../../c", "../../../../c", "c", false, true },
	{ "x/y/z", "../a/b/../../../../../c", "../c", "x/y/z/c", false, true},
	// is './' treated as ignorable, or do4es it get eaten by '../': that would be incorrect, so test for it not to fail that way.
	{ "a/b/c/./d/./../e/f/././../../g", "./z", "a/b/c/g/z", "a/b/c/g/z" },
	{ "x", "a/b/c/./d/./../e/f/././../../g", "x/a/b/c/g", "x/a/b/c/g" },
	{ "x/y", "a/b/c/./d/./../e/f/././../../g/../../../../../z", "x/z", "x/y/z", false, true },

	// are wildcards copied verbatim?
	{ "a/b/c", "**/d/e*[0-9]?f", "a/b/c/**/d/e*[0-9]?f", "a/b/c/**/d/e*[0-9]?f" },
	// nasty with wildcards: pathJoin() doesn't know about '**' and other wildcards, so it treats them as regular (single) directory elements:
	{ "a/b/c", "**/../../d/./e*[0-9]?f", "a/b/d/e*[0-9]?f", "a/b/c/d/e*[0-9]?f", false, true },

	// we DO NOT support UNIX 'user home dir' shorthand, hence '~/' will be treated like just another directory.
	{ "~/a", "~/b", "~/a/~/b", "~/a/~/b" },
	{ nullptr, "~/b", "~/b", "~/b" },

	// files_reg tests:
	{ "/a/b//c///d//", "//e//f//g//", "/a/b/c/d/e/f/g", "/a/b/c/d/e/f/g", true, true },
	{ "/tmp/", "junk//", "/tmp/junk", "/tmp/junk" },
	{ "//tmp/", "junk//", "/tmp/junk", "/tmp/junk" },
	{ "tmp/", "//junk//", "tmp/junk", "tmp/junk", true, true },
	{ "tmp/", "junk/////", "tmp/junk", "tmp/junk" },
	{ "/tmp/", "///", "/tmp", "/tmp", true, true },
	{ "////", nullptr, "/", "/" },
	{ "//", "/junk//", "/junk", "/junk", true, true },
	{ nullptr, "/junk//", "/junk", "junk", false, true },
	{ nullptr, "//junk//", "/junk", "junk", false, true },
	{ nullptr, "junk//", "junk", "junk" },
	{ nullptr, "//", "/", "", false, true },
	{ nullptr, nullptr, "", "" },
	{ "", "", "", "" },
	{ "/", "", "/", "/" },
	{ "", "//", "/", "", false, true },
	{ "a/..", "//", "", "", true, true },  // not in files_reg, but added here to show/test the difference in behaviour vs. the previous line.
	{ "", "a", "a", "a" },

	{ "..", "a", "../a", "../a", false, false },	// original expected NULL/error
	{ "/tmp", ".." , "/tmp", "/tmp", true, true },	// original expected NULL/error
	{ "./", "..", "..", "", false, true },			// original expected NULL/error
};

INSTANTIATE_TEST_SUITE_P(PathJoinTestSuite,
						 PathJoinTestFixture,
						 testing::ValuesIn<PathJoinTestData>(testdata));

TEST_P(PathJoinTestFixture, PathJoin) {
	const PathJoinTestData& argv = GetParam();
	const char *s1 = stringNew(argv.in_p1);
	const char *s2 = stringNew(argv.in_p2);
	const char *p1 = pathJoin(s1, s2);
	int ec = get_errormsg_count();
	std::string result{str_or_null(p1)};
	std::string sollwert{str_or_null(argv.out_expected)};
	EXPECT_EQ(result, sollwert) << std::format(" for input paths join(\"{}\", \"{}\")", str_or_null(s1), str_or_null(s2));
	EXPECT_EQ(argv.err_report, (ec != 0));
	stringDestroy(&s1);
	stringDestroy(&s2);
	stringDestroy(&p1);
}

TEST_P(PathJoinTestFixture, PathSafeJoin) {
	const PathJoinTestData& argv = GetParam();
	const char *s1 = stringNew(argv.in_p1);
	const char *s2 = stringNew(argv.in_p2);
 	const char *p1 = pathSafeJoin(s1, s2);
	int ec = get_errormsg_count();
	std::string result{str_or_null(p1)};
	std::string sollwert{str_or_null(argv.out_safe_expected)};
	EXPECT_EQ(result, sollwert) << std::format(" for input paths join(\"{}\", \"{}\")", str_or_null(s1), str_or_null(s2));
	EXPECT_EQ(argv.safe_err_report, (ec != 0));
	stringDestroy(&s1);
	stringDestroy(&s2);
	stringDestroy(&p1);
}


