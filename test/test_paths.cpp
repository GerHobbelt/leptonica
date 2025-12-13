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
	const char *out_expected_normalized = nullptr;
};

static inline const char *str_or_null(const char *str)
{
	if (str)
		return str;
	return "(nullptr)";
}

// see gtest-printers.h
void PrintTo(const PathJoinTestData& value, ::std::ostream* os) {
	*os << std::format("(\"{}\", \"{}\"      -->  \"{}\" / \"{}\")",
		str_or_null(value.in_p1),
		str_or_null(value.in_p2),
		str_or_null(value.out_expected),
		str_or_null(value.out_expected_normalized));
}


using PathJoinTestFixture = HeapLeakParameterizedTestFixture<PathJoinTestData>;

static const PathJoinTestData testdata[] = {
	{ "Z:/abc", "../../x", "Z:/x" }, // NOT "/x" NOR "x".
	{ "/tmp/abc", "../../x", "/tmp/x" },
	{ "/abc/def", "../../x", "/x" },
	{ "/abc/", "Z:/x/y", "/abc/Z:/x/y", "/abc/drv_Z/x/y" },
	{ "/abc/", "//?/Z:/x/y", "/abc/?/Z:/x/y", "/abc/drv_Z/x/y" },
	{ "/abc/", "//?/$Server/$Share/x/y", "/abc/?/$Server/$Share/x/y", "/abc/$Server/$Share/x/y" },
	{ "//tmp//", "//abc/", "/tmp/abc" },
	{ "tmp/", "/abc/", "tmp/abc" },
	{ "tmp/", "abc/", "tmp/abc" },
	{ "/tmp/", "///", "/tmp" },
	{ "/tmp/", nullptr, "/tmp" },
	{ "//", "/abc//", "/abc" },
	{ "//", nullptr, "/" },
	{ nullptr, "/abc/def/", "/abc/def" },
	{ nullptr, "abc//", "abc" },
	{ nullptr, "//", "/" },
	{ nullptr, nullptr, "" },
	{ "", "", "" },
	{ "", "/", "/" },
	{ "..", "/etc/foo", nullptr },
	{ "/tmp", "..", nullptr },
	{ "..", "abc/def", "../abc/def" },
	{ "abc", "..", nullptr }, //         (abc/.. will not be folded as it would translate to / (rootdir))
	{ "abc/def", "..", "abc/def/..", "abc" },
};

INSTANTIATE_TEST_SUITE_P(MeenyMinyMoe,
						 PathJoinTestFixture,
						 testing::ValuesIn<PathJoinTestData>(testdata));

TEST_P(PathJoinTestFixture, PathJoin) {
	const PathJoinTestData& argv = GetParam();
	const char *s1 = stringNew(argv.in_p1);
	const char *s2 = stringNew(argv.in_p2);
	const char *p1 = pathJoin(s1, s2);
	std::string result{p1 ? p1 : "(nullptr)"};
	std::string sollwert{argv.out_expected_normalized ? argv.out_expected_normalized : str_or_null(argv.out_expected)};
	EXPECT_EQ(result, sollwert) << std::format(" for input paths join(\"{}\", \"{}\")", str_or_null(s1), str_or_null(s2));
	stringDestroy(&s1);
	stringDestroy(&s2);
	stringDestroy(&p1);
}


