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
 * \file regutils.c
 * <pre>
 *
 *       Regression test utilities
 *           l_int32    regTestSetup()
 *           l_int32    regTestCleanup()
 *           l_int32    regTestCompareValues()
 *           l_int32    regTestCompareStrings()
 *           l_int32    regTestComparePix()
 *           l_int32    regTestCompareSimilarPix()
 *           l_int32    regTestCheckFile()
 *           l_int32    regTestCompareFiles()
 *           l_int32    regTestWritePixAndCheck()
 *           l_int32    regTestWriteDataAndCheck()
 *           char      *regTestGenLocalFilename()
 *
 *       Static function
 *           char      *getRootNameFromArgv0()
 *
 *  These functions are for testing and development.  They are not intended
 *  for use with programs that run in a production environment, such as a
 *  cloud service with unrestricted access.
 *
 *  See regutils.h for how to use this.  Here is a minimal setup:
 *
 *  main(int argc, const char** argv) {
 *  ...
 *  L_REGPARAMS  *rp;
 *
 *      if (regTestSetup(argc, argv, NULL, &rp))
 *          return 1;
 *      ...
 *      regTestWritePixAndCheck(rp, pix, IFF_PNG);  // 0
 *      ...
 *      return regTestCleanup(rp);
 *  }
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <string.h>
#include <getopt.h>
#include "allheaders.h"

static char *getRootNameFromArgv0(const char *argv0);

// special helper to help display the trailing part of the commandline when something has happened...
static const char* argstr(const char** argv, int index)
{
	// index > 0 MAY point *past* the end of argv, so we must check against that by walking there:
	while (index > 0 && argv[index] != NULL) {
		index--;
	}
	if (argv[index] == NULL)
		return "";
	return argv[index];
}

/*--------------------------------------------------------------------*
 *                      Regression test utilities                     *
 *--------------------------------------------------------------------*/
/*!
 * \brief   regTestSetup()
 *
 * \param[in]    argc    from invocation; can be either 1 or 2
 * \param[in]    argv    to regtest: %argv[1] is one of these:
 *                       "generate", "compare", "display"
 * \param[out]   prp     all regression params
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Call this function with the args to the reg test.  The first arg
 *          is the name of the reg test.  There are three cases:
 *          Case 1:
 *              There is either only one arg, or the second arg is "compare".
 *              This is the mode in which you run a regression test
 *              (or a set of them), looking for failures and logging
 *              the results to a file.  The output, which includes
 *              logging of all reg test failures plus a SUCCESS or
 *              FAILURE summary for each test, is appended to the file
 *              "/tmp/lept/reg_results.txt.  For this case, as in Case 2,
 *              the display field in rp is set to FALSE, preventing
 *              image display.
 *          Case 2:
 *              The second arg is "generate".  This will cause
 *              generation of new golden files for the reg test.
 *              The results of the reg test are not recorded, and
 *              the display field in rp is set to FALSE.
 *          Case 3:
 *              The second arg is "display".  The test will run and
 *              files will be written.  Comparisons with golden files
 *              will not be carried out, so the only notion of success
 *              or failure is with tests that do not involve golden files.
 *              The display field in rp is TRUE, and this is used by
 *              pixDisplayWithTitle().
 *      (2) See regutils.h for examples of usage.
 * </pre>
 */
l_ok
regTestSetup(int                        argc,
			 const char              ** argv,
			 const char               * output_path_base,
			 const L_REG_EXTRA_CONFIG * extras,
			 L_REGPARAMS             ** prp)
{
	char*         testname;
	const char*   vers;
	L_REGPARAMS*  rp;
	l_ok          fail = FALSE;
	int           opt;
	int           longopt_index;
	int           debug_mode = 1;
	int           gplot_mode = 1;
	int           list_argv_mode = 0;

	if (!prp)
		return ERROR_INT("ptrs not all defined", __func__, 1);

	leptCreateDiagnoticsSpecInstance();

	rp = (L_REGPARAMS*)LEPT_CALLOC(1, sizeof(L_REGPARAMS));
	*prp = rp;
	if (!rp)
		return ERROR_INT("rp could not be allocated", __func__, 1);

	rp->cmd_mode = -1;
	rp->testappmode = 0;
	rp->argv_search_mode = (extras ? extras->argv_search_mode : L_LOCATE_IN_FIRST_ANY);
	rp->help_mode = (argc <= 1 ? stringNew("?") : NULL);
	rp->outpath = NULL;
	rp->searchpaths = sarrayCreate(4);
	if (!rp->searchpaths)
		return ERROR_INT("searchpaths[] array could not be allocated.", __func__, 1);
	rp->argvfiles = sarrayCreate(4);
	if (!rp->argvfiles)
		return ERROR_INT("argvfiles[] array could not be allocated.", __func__, 1);

	static const char* const short_options = "s::d::h::q:l?::";
	const struct option options[] = {
		{ "debug", optional_argument, &debug_mode, 1 },
		{ "no-debug", no_argument, &debug_mode, 0 },
		{ "gplot", optional_argument, &gplot_mode, 1 },
		{ "no-gplot", no_argument, &gplot_mode, 0 },
		{ "tmppath", required_argument, NULL, 1 },
		{ "outpath", required_argument, NULL, 2 },
		{ "inpath", required_argument, NULL, 3 },
		{ "help", optional_argument, NULL, 'h' },
		{ "testing", no_argument, &rp->testappmode, 1 },
		{ "arg-stepping", optional_argument, NULL, 's' },				// how many argv[] elements to step forward per round. default: 0 represents 'all argv[] used in the current round.
		{ "search-mode", required_argument, NULL, 'q' },
		{ "compare", no_argument, &rp->cmd_mode, L_REG_COMPARE },
		{ "generate", no_argument, &rp->cmd_mode, L_REG_GENERATE },
		{ "display", no_argument, &rp->cmd_mode, L_REG_DISPLAY },
		{ NULL }
	};
	const char* options_str = stringNew(short_options);
	const struct option* options_arr = options;
	const L_REGCMD_OPTION_SPEC extra_options = (extras ? extras->extra_options : NULL);
	if (extra_options != NULL) {
		int count1 = 0;
		for (const struct option* opt = options_arr; opt->name; opt++)
			count1++;
		int count2 = 1;
		const char *hit_optional_nonarg = NULL;
		for (L_REGCMD_OPTION_SPEC opt = extra_options; opt->type != L_CMD_OPT_NIL; opt++) {
			switch (opt->type) {
			case L_CMD_OPT_W_NO_ARG:
			case L_CMD_OPT_W_REQUIRED_ARG:
			case L_CMD_OPT_W_OPTIONAL_ARG:
				count2++;
				continue;

			case L_CMD_PLAIN_OPTIONAL_ARGUMENT:
				assert(opt->name);
				hit_optional_nonarg = opt->name;
				continue;

			case L_CMD_PLAIN_REQUIRED_ARGUMENT:
				assert(opt->name);
				if (hit_optional_nonarg) {
					L_ERROR("caller-supplied extra options: mandatory positional argument '%s' follows previous OPTIONAL positional argument '%s'; this does not parse. Code MUST list all mandatory positional arguments before the optional ones!\n", __func__, opt->name, hit_optional_nonarg);
					fail = TRUE;
				}
				continue;

			default:
				assert(opt->name);
				continue;
			}
		}

		size_t size = (count1 + count2) * sizeof(options_arr[0]);
		options_arr = (const struct option*)LEPT_MALLOC(size);
		if (!options_arr)
			return ERROR_INT("options_arr[] array could not be allocated.", __func__, 1);
		size_t shorts_len = strlen(short_options);
		char* options2 = (char*)LEPT_MALLOC(count2 * 3 + shorts_len + 1);
		if (!options2)
			return ERROR_INT("options string could not be allocated.", __func__, 1);

		memcpy((void*)options_arr, options, count1 * sizeof(options_arr[0]));
		strcpy(options2, short_options);

		const struct option* d__ = options_arr + count1;
		struct option* dst = (struct option *)d__;				// const-removal-cast, because we can and we need it
		char* optstr2 = options2 + shorts_len;
		for (int i = 0; extra_options[i].type != L_CMD_OPT_NIL; i++) {
			L_REGCMD_OPTION_SPEC opt = extra_options + i;
			if (strlen(opt->name) == 1) {
				// short option, e.g. 'x' for '-x'
				if (strchr(short_options, opt->name[0])) {
					L_ERROR("caller-supplied extra options: short option '%s' clashes with the default set '%s': this extra option would be unreachable from the command line!\n", __func__, opt->name, short_options);
					fail = TRUE;
					continue;
				}
				*optstr2++ = opt->name[0];
				switch (opt->type) {
				case L_CMD_OPT_W_NO_ARG:
					continue;
				case L_CMD_OPT_W_REQUIRED_ARG:
					*optstr2++ = ':';
					continue;
				case L_CMD_OPT_W_OPTIONAL_ARG:
					*optstr2++ = ':';
					*optstr2++ = ':';
					continue;

				default:
					continue;
				}
			}
			else {
				// long option, e.g. '--long-named-opt'
				dst->name = opt->name;
				dst->flag = NULL;
				dst->val = 128 + i;
				switch (opt->type) {
				case L_CMD_OPT_W_NO_ARG:
					dst->has_arg = no_argument;
					dst++;
					continue;
				case L_CMD_OPT_W_REQUIRED_ARG:
					dst->has_arg = required_argument;
					dst++;
					continue;
				case L_CMD_OPT_W_OPTIONAL_ARG:
					dst->has_arg = optional_argument;
					dst++;
					continue;

				default:
					continue;
				}
			}
		}
		dst->name = NULL;
		dst->flag = NULL;
		dst->val = 0;
		dst->has_arg = 0;

		*optstr2++ = 0;

		stringDestroy(&options_str);

		options_str = options2;
	}

	// reset the getopt_long parser:
	optind = 0;
	longopt_index = -1;

	for (;;) {
		opt = getopt_long(argc, argv, options_str, options_arr, &longopt_index);
		if (opt == -1)
			break;

		switch (opt) {
		case 0:
			// logopt assignment has been handled internally by getopt()
			continue;

		case 1:
			leptDebugSetTmpDirBasePath(optarg);
			continue;

		case 2:
			if (rp->outpath) {
				L_ERROR("Must not define outpath twice on the command line.\n", __func__);
				fail = TRUE;
				continue;
			}
			rp->outpath = stringNew(optarg);
			continue;

		case 3:
			// process the searchpaths list: first, determine which separator has been used: | ;
			if (TRUE) {
				const char* sep_marker_p = strpbrk(optarg, "|;");
				char sep_marker[2] = { (sep_marker_p ? *sep_marker_p : ';'), 0 };
				SARRAY* srch_arr = sarrayCreate(1);
				sarraySplitString(srch_arr, optarg, sep_marker);
				// append to existing set:
				sarrayJoin(rp->searchpaths, srch_arr);
				sarrayDestroy(&srch_arr);
			}
			continue;

		case 'd':
			debug_mode = (optarg != NULL ? atoi(optarg) : 1);
			continue;

		case 'l':
			list_argv_mode = 1;
			break;

		case 's':
			rp->argv_step_size_per_round = (optarg != NULL ? atoi(optarg) : 0);
			continue;

		case 'q':
			if (TRUE) {
				int m = atoi(optarg);
				switch (m) {
				case L_LOCATE_IN_ALL:
				case L_LOCATE_IN_FIRST_ANY:
				case L_LOCATE_IN_FIRST_ONE:
				case L_LOCATE_IGNORE_CURRENT_DIR_FLAG | L_LOCATE_IN_ALL:
				case L_LOCATE_IGNORE_CURRENT_DIR_FLAG | L_LOCATE_IN_FIRST_ANY:
				case L_LOCATE_IGNORE_CURRENT_DIR_FLAG | L_LOCATE_IN_FIRST_ONE:
					rp->argv_search_mode = (l_LocateMode_t)m;
					break;

				default:
					L_ERROR("Unknown/unsupported file arg locate/expand mode: %d. Supported modes are: %d (all), %d (all-in-first), %d (first one), %d (all, ignore cwd), %d (all-in-first, ignore cwd), %d (first one, ignore cwd)\n", __func__, m,
						L_LOCATE_IN_ALL,
						L_LOCATE_IN_FIRST_ANY,
						L_LOCATE_IN_FIRST_ONE,
						L_LOCATE_IGNORE_CURRENT_DIR_FLAG | L_LOCATE_IN_ALL,
						L_LOCATE_IGNORE_CURRENT_DIR_FLAG | L_LOCATE_IN_FIRST_ANY,
						L_LOCATE_IGNORE_CURRENT_DIR_FLAG | L_LOCATE_IN_FIRST_ONE);
					break;
				}
			}
			continue;

		case 'h':
			rp->help_mode = stringNew(optarg != NULL ? optarg : "?");
			continue;

		default:
			if (TRUE) {
				int extras_idx = -1;
				if (opt >= 128) {
					// one of our long-named extras was mentioned on the command line. yay!
					extras_idx = opt - 128;
				}
				else {
					// extra short option, e.g. 'x' for '-x'?
					for (int i = 0; extra_options[i].name != L_CMD_OPT_NIL; i++) {
						L_REGCMD_OPTION_SPEC o = extra_options + i;
						switch (o->type) {
						case L_CMD_OPT_W_NO_ARG:
						case L_CMD_OPT_W_REQUIRED_ARG:
						case L_CMD_OPT_W_OPTIONAL_ARG:
							if (strlen(o->name) == 1 && o->name[0] == opt) {
								extras_idx = i;
								break;
							}
							continue;

						default:
							continue;
						}
						break;
					}
				}
				if (extras_idx >= 0) {
					L_REGCMD_OPTION_SPEC o = extra_options + extras_idx;
					int pos = argc - optind;
					assert(o->handler);
					if (o->handler(o, optarg, &pos, argv + pos)) {
						L_ERROR("caller-supplied extra option ('%s') handler has reported failure to process.\n", __func__, o->name);
						fail = TRUE;
						continue;
					}
				}
				else {
					L_ERROR("Unknown/unsupported command line option %c has been specified: %s %s%s ...\n", __func__,
						(opt > ' ' ? opt : '?'), argstr(argv, optind), argstr(argv, optind + 1), argstr(argv, optind + 2));
					fail = TRUE;
				}
			}
			continue;
		}
	}

	stringDestroy(&options_str);
	if (options_arr != options) {
		LEPT_FREE((void *)options_arr);
		options_arr = NULL;
	}

	// process the remaining non-options in argv[]:
	{
		L_REGCMD_OPTION_SPEC extra_positional_arguments = extra_options;
		for (; optind < argc; optind++) {
			optarg = argv[optind];

			if (rp->cmd_mode == -1) {
				if (!strcmp(optarg, "compare")) {
					rp->cmd_mode = L_REG_COMPARE;
					continue;
				}
				else if (!strcmp(optarg, "generate")) {
					rp->cmd_mode = L_REG_GENERATE;
					continue;
				}
				else if (!strcmp(optarg, "display")) {
					rp->cmd_mode = L_REG_DISPLAY;
					continue;
				}
			}

			if (extra_positional_arguments) {
				const char* p = strchr(optarg, '=');
				if (p) {
					int dealt_with = 0;

					// check assignment-style argument expressions:
					for (L_REGCMD_OPTION_SPEC o = extra_options; o->type != L_CMD_OPT_NIL; o++) {
						if (o->type == L_CMD_VAR_ASSIGNMENT) {
							size_t len = p - optarg;
							if (strlen(o->name) == len && 0 == strncmp(o->name, optarg, len)) {
								assert(o->handler);
								int pos = argc - optind;
								if (o->handler(o, optarg, &pos, argv + pos)) {
									L_ERROR("caller-supplied extra option ('%s') handler has reported failure to process.\n", __func__, o->name);
									fail = TRUE;
								}
								dealt_with = 1;
								break;
							}
						}
					}

					if (!dealt_with) {
						L_WARNING("Looks like you have an assignment statement as part of your commandline argument, yet we have not been able to locate a predefined handler for it:\n", __func__);
						L_WARNING("    %s\n", __func__, optarg);
						L_WARNING("are you sure this is correct input?\n", __func__);
						L_WARNING("Alas, we will be proceeding anyway...\n", __func__);
					}
					else {
						continue;
					}
				}

				// resolve to the next available 'positional' argument:
				assert(L_CMD_PLAIN_REQUIRED_ARGUMENT == L_CMD_PLAIN_OPTIONAL_ARGUMENT + 1);
				assert(L_CMD_OPT_NIL < L_CMD_PLAIN_OPTIONAL_ARGUMENT);
				assert(L_CMD_OPT_W_NO_ARG < L_CMD_PLAIN_OPTIONAL_ARGUMENT);
				assert(L_CMD_OPT_W_REQUIRED_ARG < L_CMD_PLAIN_OPTIONAL_ARGUMENT);
				assert(L_CMD_OPT_W_OPTIONAL_ARG < L_CMD_PLAIN_OPTIONAL_ARGUMENT);
				assert(L_CMD_VAR_ASSIGNMENT < L_CMD_PLAIN_OPTIONAL_ARGUMENT);

				while (extra_positional_arguments->type < L_CMD_PLAIN_OPTIONAL_ARGUMENT && extra_positional_arguments->type != L_CMD_OPT_NIL) {
					extra_positional_arguments++;
				}

				const L_REGCMD_OPTION_SPEC o = extra_positional_arguments;
				if (o->type != L_CMD_OPT_NIL) {
					assert(o->handler);
					int pos = argc - optind;
					if (o->handler(o, optarg, &pos, argv + pos)) {
						L_ERROR("caller-supplied extra option ('%s') handler has reported failure to process.\n", __func__, o->name);
						fail = TRUE;
					}

					// once a positional has been dealt with, it's time to proceed and attempt to fill the next one in the set on the next round...
					++extra_positional_arguments;
					continue;
				}
			}

			// do we expand @xyz responsefiles? yes, we do; we deal with those at the end
			sarrayAddString(rp->argvfiles, optarg, L_COPY);
		}

		// when all available positional argv[] arguments have been consumed and one or more *mandatory* positional options are still yet to be filled, that's an incomplete command line
		if (extra_positional_arguments) {
			while (extra_positional_arguments->type < L_CMD_PLAIN_OPTIONAL_ARGUMENT && extra_positional_arguments->type != L_CMD_OPT_NIL) {
				extra_positional_arguments++;
			}
			if (extra_positional_arguments->type == L_CMD_PLAIN_REQUIRED_ARGUMENT) {
				L_ERROR("The commandline lacks: these mandatory positional arguments must be specified as well:\n", __func__);
				fail = TRUE;
				do {
					if (extra_positional_arguments->type == L_CMD_PLAIN_REQUIRED_ARGUMENT) {
						L_ERROR("    %s: [mandatory] %s\n", __func__, extra_positional_arguments->name, extra_positional_arguments->help_description);
					}
					++extra_positional_arguments;
				} while (extra_positional_arguments->type != L_CMD_OPT_NIL);
			}
		}
	}

	if (rp->cmd_mode == -1) {
		rp->cmd_mode = L_REG_BASIC_EXEC;
	}

	testname = NULL;
	if (extras && extras->testname) {
		testname = stringNew(extras->testname);
	}
	if (!testname && (testname = getRootNameFromArgv0(argv[0])) == NULL)
		return ERROR_INT("invalid root", __func__, 1);

	if (fail) {
		stringDestroy(&testname);
		return ERROR_INT("Failed to parse the command line entirely. Run the application with -h or --help to get some general information.", __func__, 1);
	}

	if (rp->help_mode) {
		// TODO: advanced help
		lept_stderr("Syntax: %s [ [compare] | generate | display ] ...\n", testname);
		lept_stderr("\n"
					"The following options are supported:\n"
			        "-s [n]        argv[] step size per test round (default: 0 = all consumed)\n"
			        "-d [n]        set debug mode (ON(1) by default; can be turned off with n=0\n"
			        "              or passing --no-debug instead.\n"
			        "-q n          set filespec expansion/location mode: ALL(0), all-in-first-dir(1)\n"
			        "              first-in-first(2); default: ALL (which will expand wildcarded\n"
			        "              expand into multiple lines iff possible)"
			        "-l            (diagnostic) list the expanded set of argv lines (after response-\n"
			        "              file processing) before proceeding with the application.\n"
			        "-h [subject]  show some help info\n"
		);
		lept_stderr("\n"
			        "The original leptonica regression test command modes can also be specified\n"
			        "as long options instead of just the command words:\n"
			        "--compare\n"
			        "--generate\n"
			        "--display\n"
		);
		lept_stderr("\n"
			        "The following long options are supported:\n"
		);
		for (const struct option* opt = options; opt->name != NULL; opt++) {
			static const char* argtypes[] = {
				"",
				"",			// no_argument
				"arg",		// required_argument
				"[arg]",	// optional_argument
			};
			assert(optional_argument == 3);
			assert(no_argument == 1);
			assert(required_argument == 2);
			char cmd_chr = ' ';
			const char* eqvmsg = "";
			if (!opt->has_arg && isalnum(opt->val)) {
				cmd_chr = opt->val;
				eqvmsg = "    is equivalent to option -";
			}
			lept_stderr("--%s %s%s%c\n", opt->name, argtypes[opt->has_arg & 0x03], eqvmsg, cmd_chr);
		}
		if (extra_options) {
			lept_stderr("\n"
				"These additional options are also supported:\n"
			);
			int has_var_assignments = 0;
			int has_positionals = 0;
			for (int i = 0; extra_options[i].name != L_CMD_OPT_NIL; i++) {
				L_REGCMD_OPTION_SPEC o = extra_options + i;
				switch (o->type) {
				case L_CMD_OPT_W_NO_ARG:
					lept_stderr("-%s           %s\n", o->name, o->help_description);
					continue;
				case L_CMD_OPT_W_REQUIRED_ARG:
					lept_stderr("-%s val       %s\n", o->name, o->help_description);
					continue;
				case L_CMD_OPT_W_OPTIONAL_ARG:
					lept_stderr("-%s [v]       %s\n", o->name, o->help_description);
					continue;

				case L_CMD_VAR_ASSIGNMENT:
					has_var_assignments = 1;
					continue;

				case L_CMD_PLAIN_OPTIONAL_ARGUMENT:
				case L_CMD_PLAIN_REQUIRED_ARGUMENT:
					has_positionals = 1;
					continue;

				default:
					break;
				}
			}
			if (has_var_assignments) {
				lept_stderr("\n"
					"Plus these non-option 'assignment' arguments:\n"
				);
				for (int i = 0; extra_options[i].name != L_CMD_OPT_NIL; i++) {
					L_REGCMD_OPTION_SPEC o = extra_options + i;
					switch (o->type) {
					case L_CMD_OPT_W_NO_ARG:
					case L_CMD_OPT_W_REQUIRED_ARG:
					case L_CMD_OPT_W_OPTIONAL_ARG:
						continue;

					case L_CMD_VAR_ASSIGNMENT:
						lept_stderr("%s=value\n"
							"              %s\n", o->name, o->help_description);
						continue;
					case L_CMD_PLAIN_OPTIONAL_ARGUMENT:
					case L_CMD_PLAIN_REQUIRED_ARGUMENT:
						continue;
					default:
						break;
					}
				}
			}
			if (has_positionals) {
				lept_stderr("\n"
					"Also please do note these non-option ('positional') arguments are compulsatory:\n"
				);
				for (int i = 0; extra_options[i].name != L_CMD_OPT_NIL; i++) {
					L_REGCMD_OPTION_SPEC o = extra_options + i;
					switch (o->type) {
					case L_CMD_OPT_W_NO_ARG:
					case L_CMD_OPT_W_REQUIRED_ARG:
					case L_CMD_OPT_W_OPTIONAL_ARG:
					case L_CMD_VAR_ASSIGNMENT:
						continue;

					case L_CMD_PLAIN_OPTIONAL_ARGUMENT:
						lept_stderr("[value]       %s: [optional] %s\n", o->name, o->help_description);
						continue;
					case L_CMD_PLAIN_REQUIRED_ARGUMENT:
						lept_stderr("value         %s: [mandatory] %s\n", o->name, o->help_description);
						continue;
					default:
						break;
					}
				}
			}
		}

        return 1;
    }

	leptDebugSetStepLevelAsForeverIncreasing(FALSE);

	leptActivateDebugMode(!!debug_mode, 0);
	leptActivateGplotMode(!!gplot_mode, 0);

    setLeptDebugOK(1);  /* required for testing */

	rp->testname = testname;
	rp->index = -1;  /* increment before each test */

        /* Initialize to true.  A failure in any test is registered
         * as a failure of the regression test. */
    rp->success = !fail;

#if 0
	/* Make sure the lept/regout subdirectory exists */
    lept_mkdir(leptDebugGetFileBasePath());
#endif

        /* Only open a stream to a temp file for the 'compare' case */
	switch (rp->cmd_mode) {
	case L_REG_COMPARE:
		leptDebugSetFileBasepath("/tmp/lept/regout");
		rp->tempfile = stringNew("/tmp/lept/regout/regtest_output.txt");
		rp->fp = fopenWriteStream(rp->tempfile, "wb");
		if (rp->fp == NULL) {
			rp->success = FALSE;
			return ERROR_INT_1("stream not opened for tempfile", rp->tempfile, __func__, 1);
		}
		break;

	case L_REG_GENERATE:
		leptDebugSetFileBasepath("/tmp/lept/golden");
		//lept_mkdir("lept/golden");
		break;

	case L_REG_DISPLAY:
		leptDebugSetFileBasepath("/tmp/lept/display");
		leptSetInDisplayMode(TRUE);
		break;

	default: // L_REG_BASIC_EXEC;
		leptDebugSetFileBasepath("/tmp/lept/prog");
		break;
	}

#if 0
	/* Make sure the lept/regout subdirectory exists */
	lept_mkdir(leptDebugGetFileBasePath());
#endif

	rp->base_step_level = leptDebugGetStepLevel();

	if (output_path_base && *output_path_base) {
		leptDebugSetFilePathPart(output_path_base);
		rp->base_step_level = leptDebugAddStepLevel();
	}

        /* Print out test name and both the leptonica and
         * image library versions */
    lept_stderr("\n////////////////////////////////////////////////\n"
                "////////////////   %s_reg   ///////////////\n"
                "////////////////////////////////////////////////\n",
                rp->testname);
    vers = getLeptonicaVersion();
    lept_stderr("%s : ", vers);
    stringDestroy(&vers);
    vers = getImagelibVersions();
    lept_stderr("%s\n", vers);
    stringDestroy(&vers);

	// post-work: clean up our search paths set (deduplicate, etc.)
	{
		char* cdir = getcwd(NULL, 0);
		if (cdir == NULL) {
			rp->success = FALSE;
			return ERROR_INT("no current dir found", __func__, 1);
		}
		SARRAY* lcl_arr = pathDeducePathSet(rp->searchpaths, cdir, !(rp->argv_search_mode & L_LOCATE_IGNORE_CURRENT_DIR_FLAG));
		sarrayDestroy(&rp->searchpaths);
		rp->searchpaths = lcl_arr;
		free(cdir);
	}

	if (rp->cmd_mode != L_REG_BASIC_EXEC && rp->results_file_path == NULL) {
		rp->results_file_path = stringNew("/tmp/lept/reg_results.txt");
	}
	else if (rp->results_file_path == NULL) {
		rp->results_file_path = string_asprintf("/tmp/lept/%s_results.txt", rp->testname);
	}

	if (rp->results_file_path == NULL) {
		rp->success = FALSE;
		return ERROR_INT("rp->results_file_path could not be allocated", __func__, 1);
	}

	if (list_argv_mode) {
		lept_stderr("\n========== CLI input 'lines' (a.k.a. argv[] set) ================\n");
		int count = sarrayGetCount(rp->argvfiles);
		int width = (int)ceil(log10(count + 0.1));
		for (int i = 0; i < count; i++) {
			const char* line = sarrayGetString(rp->argvfiles, i, L_NOCOPY);
			lept_stderr("#%0*d: %s\n", width, i + 1, line);
		}
		if (count == 0) {
			lept_stderr("(-- none --)\n");
		}
		lept_stderr("======================= search paths: ===========================\n");
		int scount = sarrayGetCount(rp->searchpaths);
		width = (int)ceil(log10(scount + 0.1));
		for (int i = 0; i < scount; i++) {
			const char* line = sarrayGetString(rp->searchpaths, i, L_NOCOPY);
			lept_stderr("#%0*d: %s\n", width, i + 1, line);
		}
		if (scount == 0) {
			lept_stderr("(-- none --)\n");
		}
		lept_stderr("================= (total: %3d input lines) =========================\n", count);
	}

	// now deal with the responsefiles (if any) and attempt to resolve every argv[] path we've got:
	SARRAY* lcl_arr = leptProcessResponsefileLines(rp->argvfiles, rp->searchpaths, rp->argv_search_mode, rp->outpath, "\x01" /* stmt marker */, "\x02 FAIL: " /* fail marker */, "\x03# " /* ignore marker */);
	sarrayDestroy(&rp->argvfiles);
	rp->argvfiles = lcl_arr;
	if (lcl_arr == NULL) {
		rp->success = FALSE;
		return ERROR_INT("lcl_arr could not be allocated", __func__, 1);
	}

	// heuristic estimate for a good display width of the step numbers:
	// as these are, at least at level 2, related to our input set, we take
	// the size of *that* for a start.
	{
		int count = sarrayGetCount(lcl_arr);
		unsigned int dw = (unsigned int)(ceil(log10(count + 0.1)));
		if (dw < 2)
			dw = 2;
		leptDebugSetStepDisplayWidth(dw);
	}

	if (list_argv_mode) {
		lept_stderr("\n========== EXPANDED CLI input 'lines' (a.k.a. argv[] set) ============\n");
		int count = sarrayGetCount(rp->argvfiles);
		int width = (int)ceil(log10(count + 0.1));
		for (int i = 0; i < count; i++) {
			const char* line = sarrayGetString(rp->argvfiles, i, L_NOCOPY);
			// strip off the special marker bytes, but keep the rest of the marker to sees:
			switch (line[0]) {
			case 0x01:	/* stmt marker */
				line += 1;
				break;
			case 0x02: /* fail marker: '\x02 FAIL: ' */
				line += 2; // 8
				break;
			case 0x03: /* ignore marker: '\x03# ' */
				line += 1; // 3
				break;
			default:
				break;
			}
			lept_stderr("#%0*d: %s\n", width, i + 1, line);
		}
		lept_stderr("======================= (total: %3d lines) ===========================\n", count);
	}

	rp->argv_index_base = 0;
	rp->argv_index = 0;
	rp->argv_fake_extra = 0;

	if (extras) {
		// verify the extra conditions we received re commandline content:
		int argv_count = regGetFileArgCount(rp);

		if (argv_count < extras->min_required_argc) {
			L_ERROR("The commandline does not list the minimum required number of file paths: %d are required, while %d are actually provided.\n", __func__, extras->min_required_argc, argv_count);
			return 1;
		}
		if (argv_count > extras->max_required_argc) {
			L_ERROR("The commandline lists too many file paths: %d are allowed, while %d are actually provided.\n", __func__, extras->max_required_argc, argv_count);
			return 1;
		}
	}

	rp->tstart = startTimerNested();
    return 0;
}


/*!
 * \brief   regTestCleanup()
 *
 * \param[in]    rp    regression test parameters
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This copies anything written to the temporary file to the
 *          output file /tmp/lept/reg_results.txt.
 * </pre>
 */
l_ok
regTestCleanup(L_REGPARAMS  *rp)
{
char     result[512];
char    *text, *message;
l_int32  retval;
l_ok     append_results = FALSE;
size_t   nbytes;

    if (!rp)
        return ERROR_INT("rp not defined", __func__, 1);

	leptDebugPopStepLevelTo(rp->base_step_level);

	lept_stderr("Time: %7.3f sec\n", stopTimerNested(rp->tstart));

		/* If generating golden files or running in display mode, release rp */

	text = NULL;
	if (rp->fp != NULL) {
		/* Compare mode: read back data from temp file */
		append_results = TRUE;
		fclose(rp->fp);
		rp->fp = NULL;
		if (rp->tempfile) {
			text = (char*)l_binaryRead(rp->tempfile, &nbytes);
			if (!text) {
				rp->success = FALSE;
				L_ERROR("text not returned", __func__);
			}
		}
	}

        /* Prepare result message */
	if (rp->success)
		snprintf(result, sizeof(result), "SUCCESS: %s\n", rp->testname);
	else
		snprintf(result, sizeof(result), "FAILURE: %s\n", rp->testname);
    message = stringJoin(text, result);
    LEPT_FREE(text);
	if (rp->results_file_path != NULL && message != NULL && append_results) {
		fileAppendString(rp->results_file_path, message);
	}
	lept_stderr("\n%s", result);
	stringDestroy(&message);

	leptDestroyDiagnoticsSpecInstance();

    stringDestroy(&rp->testname);
	stringDestroy(&rp->results_file_path);
	assert(rp->fp == NULL);
	stringDestroy(&rp->tempfile);
	stringDestroy(&rp->help_mode);
	stringDestroy(&rp->tmpdirpath);
	stringDestroy(&rp->outpath);
	stringDestroy(&rp->results_file_path);

	sarrayDestroy(&rp->searchpaths);
	sarrayDestroy(&rp->argvfiles);

	retval = (rp->success ? EXIT_SUCCESS : EXIT_FAILURE);

    LEPT_FREE(rp);
    return retval;
}


/*!
 * \brief   regTestCompareValues()
 *
 * \param[in]    rp      regtest parameters
 * \param[in]    val1    typ. the golden value
 * \param[in]    val2    typ. the value computed
 * \param[in]    delta   allowed max absolute difference
 * \return  0 if OK, 1 on error
 *               Note: a failure in comparison is not an error
 */
l_ok
regTestCompareValues(L_REGPARAMS  *rp,
                     l_float32     val1,
                     l_float32     val2,
                     l_float32     delta)
{
l_float32  diff;

    if (!rp)
        return ERROR_INT("rp not defined", __func__, 1);

    rp->index++;
    diff = L_ABS(val2 - val1);

        /* Record on failure */
    if (diff > delta) {
        if (rp->fp) {
            fprintf(rp->fp,
                    "Failure in %s: value comparison for index %d\n"
                    "difference = %f but allowed delta = %f\n",
                    rp->testname, (int)rp->index, diff, delta);
        }
        lept_stderr("Failure in %s: value comparison for index %d\n"
                    "difference = %f but allowed delta = %f\n",
                    rp->testname, (int)rp->index, diff, delta);
        rp->success = FALSE;
    }
    return 0;
}


/*!
 * \brief   regTestCompareStrings()
 *
 * \param[in]    rp        regtest parameters
 * \param[in]    string1   typ. the expected string
 * \param[in]    bytes1    size of string1
 * \param[in]    string2   typ. the computed string
 * \param[in]    bytes2    size of string2
 * \return  0 if OK, 1 on error
 *               Note: a failure in comparison is not an error
 */
l_ok
regTestCompareStrings(L_REGPARAMS  *rp,
                      l_uint8      *string1,
                      size_t        bytes1,
                      l_uint8      *string2,
                      size_t        bytes2)
{
l_int32  same;
char     buf[256];

    if (!rp)
        return ERROR_INT("rp not defined", __func__, 1);

    rp->index++;
    l_binaryCompare(string1, bytes1, string2, bytes2, &same);

        /* Output on failure */
    if (!same) {
            /* Write the two strings to file */
        snprintf(buf, sizeof(buf), "/tmp/lept/regout/string1_%d_%zu",
                 (int)rp->index, bytes1);
        l_binaryWrite(buf, "w", string1, bytes1);
        snprintf(buf, sizeof(buf), "/tmp/lept/regout/string2_%d_%zu",
			(int)rp->index, bytes2);
        l_binaryWrite(buf, "w", string2, bytes2);

            /* Report comparison failure */
        snprintf(buf, sizeof(buf), "/tmp/lept/regout/string*_%d_*", (int)rp->index);
        if (rp->fp) {
            fprintf(rp->fp,
                    "Failure in %s: string comp for index %d; "
                    "written to %s\n", rp->testname, (int)rp->index, buf);
        }
        lept_stderr("Failure in %s: string comp for index %d; "
                    "written to %s\n", rp->testname, (int)rp->index, buf);
        rp->success = FALSE;
    }
    return 0;
}


/*!
 * \brief   regTestComparePix()
 *
 * \param[in]    rp            regtest parameters
 * \param[in]    pix1, pix2    to be tested for equality
 * \return  0 if OK, 1 on error
 *               Note: a failure in comparison is not an error
 *
 * <pre>
 * Notes:
 *      (1) This function compares two pix for equality.  On failure,
 *          this writes to stderr.
 * </pre>
 */
l_ok
regTestComparePix(L_REGPARAMS  *rp,
                  PIX          *pix1,
                  PIX          *pix2)
{
l_int32  same;

    if (!rp)
        return ERROR_INT("rp not defined", __func__, 1);
    if (!pix1 || !pix2) {
        rp->success = FALSE;
        return ERROR_INT("pix1 and pix2 not both defined", __func__, 1);
    }

    rp->index++;
    pixEqual(pix1, pix2, &same);

        /* Record on failure */
    if (!same) {
        if (rp->fp) {
            fprintf(rp->fp, "Failure in %s: pix comparison for index %d\n",
                    rp->testname, (int)rp->index);
        }
        lept_stderr("Failure in %s: pix comparison for index %d\n",
                    rp->testname, (int)rp->index);
        rp->success = FALSE;
    }
    return 0;
}


/*!
 * \brief   regTestCompareSimilarPix()
 *
 * \param[in]    rp           regtest parameters
 * \param[in]    pix1, pix2   to be tested for near equality
 * \param[in]    mindiff      minimum pixel difference to be counted; > 0
 * \param[in]    maxfract     maximum fraction of pixels allowed to have
 *                            diff greater than or equal to mindiff
 * \param[in]    printstats   use 1 to print normalized histogram to stderr
 * \return  0 if OK, 1 on error
 *               Note: a failure in similarity comparison is not an error
 *
 * <pre>
 * Notes:
 *      (1) This function compares two pix for near equality.  On failure,
 *          this writes to stderr.
 *      (2) The pix are similar if the fraction of non-conforming pixels
 *          does not exceed %maxfract.  Pixels are non-conforming if
 *          the difference in pixel values equals or exceeds %mindiff.
 *          Typical values might be %mindiff = 15 and %maxfract = 0.01.
 *      (3) The input images must have the same size and depth.  The
 *          pixels for comparison are typically subsampled from the images.
 *      (4) Normally, use %printstats = 0.  In debugging mode, to see
 *          the relation between %mindiff and the minimum value of
 *          %maxfract for success, set this to 1.
 * </pre>
 */
l_ok
regTestCompareSimilarPix(L_REGPARAMS  *rp,
                         PIX          *pix1,
                         PIX          *pix2,
                         l_int32       mindiff,
                         l_float32     maxfract,
                         l_int32       printstats)
{
l_int32  w, h, factor, similar;

    if (!rp)
        return ERROR_INT("rp not defined", __func__, 1);
    if (!pix1 || !pix2) {
        rp->success = FALSE;
        return ERROR_INT("pix1 and pix2 not both defined", __func__, 1);
    }

    rp->index++;
    pixGetDimensions(pix1, &w, &h, NULL);
    factor = L_MAX(w, h) / 400;
    factor = L_MAX(1, L_MIN(factor, 4));   /* between 1 and 4 */
    pixTestForSimilarity(pix1, pix2, factor, mindiff, maxfract, 0.0,
                         &similar, printstats);

        /* Record on failure */
    if (!similar) {
        if (rp->fp) {
            fprintf(rp->fp,
                    "Failure in %s: pix similarity comp for index %d\n",
                    rp->testname, (int)rp->index);
        }
        lept_stderr("Failure in %s: pix similarity comp for index %d\n",
                    rp->testname, (int)rp->index);
        rp->success = FALSE;
    }
    return 0;
}


/*!
 * \brief   regTestCheckFile()
 *
 * \param[in]    rp         regtest parameters
 * \param[in]    localname  name of output file from reg test
 * \return  0 if OK, 1 on error
 *               Note: a failure in comparison is not an error
 *
 * <pre>
 * Notes:
 *      (1) This function does one of three things, depending on the mode:
 *           * "generate": makes a "golden" file as a copy of %localname.
 *           * "compare": compares %localname contents with the golden file
 *           * "display": this does nothing
 *      (2) The canonical format of the golden filenames is:
 *            /tmp/lept/golden/[root of main name]_golden.[index].
 *                                                       [ext of localname]
 *          e.g.,
 *             /tmp/lept/golden/maze_golden.0.png
 *      (3) The local file can be made in any subdirectory of /tmp/lept,
 *          including /tmp/lept/regout/.
 *      (4) It is important to add an extension to the local name, such as
 *             /tmp/lept/maze/file1.png    (extension ".png")
 *          because the extension is added to the name of the golden file.
 * </pre>
 */
l_ok
regTestCheckFile(L_REGPARAMS  *rp,
                 const char   *localname)
{
char    *ext;
char     namebuf[256];
l_int32  ret, same, format;
PIX     *pix1, *pix2;

    if (!rp)
        return ERROR_INT("rp not defined", __func__, 1);
    if (!localname) {
        rp->success = FALSE;
        return ERROR_INT("local name not defined", __func__, 1);
    }
    if (rp->cmd_mode != L_REG_GENERATE && rp->cmd_mode != L_REG_COMPARE &&
        rp->cmd_mode != L_REG_DISPLAY) {
        rp->success = FALSE;
        return ERROR_INT("invalid mode", __func__, 1);
    }
    rp->index++;

        /* If display mode, no generation and no testing */
    if (rp->cmd_mode == L_REG_DISPLAY) return 0;

        /* Generate the golden file name; used in 'generate' and 'compare' */
    splitPathAtExtension(localname, NULL, &ext);
    snprintf(namebuf, sizeof(namebuf), "/tmp/lept/golden/%s_golden.%02d%s",
             rp->testname, (int)rp->index, ext);
    LEPT_FREE(ext);

        /* Generate mode.  No testing. */
    if (rp->cmd_mode == L_REG_GENERATE) {
            /* Save the file as a golden file */
        ret = fileCopy(localname, namebuf);
#if 0       /* Enable for details on writing of golden files */
        if (!ret) {
            char *local = genPathname(localname, NULL);
            char *golden = genPathname(namebuf, NULL);
            L_INFO("Copy: %s to %s\n", __func__, local, golden);
            LEPT_FREE(local);
            LEPT_FREE(golden);
        }
#endif
        return ret;
    }

        /* Compare mode: test and record on failure.  This can be used
         * for all image formats, as well as for all files of serialized
         * data, such as boxa, pta, etc.  In all cases except for
         * GIF compressed images, we compare the files to see if they
         * are identical.  GIF doesn't support RGB images; to write
         * a 32 bpp RGB image in GIF, we do a lossy quantization to
         * 256 colors, so the cycle read-RGB/write-GIF is not idempotent.
         * And although the read/write cycle for GIF images with bpp <= 8
         * is idempotent in the image pixels, it is not idempotent in the
         * actual file bytes; tests comparing file bytes before and after
         * a GIF read/write cycle will fail.  So for GIF we uncompress
         * the two images and compare the actual pixels.  PNG is both
         * lossless and idempotent in file bytes on read/write, so it is
         * not necessary to compare pixels.  (Comparing pixels requires
         * decompression, and thus would increase the regression test
         * time.  JPEG is lossy and not idempotent in the image pixels,
         * so no tests are constructed that would require it. */
    findFileFormat(localname, &format);
    if (format == IFF_GIF) {
        same = 0;
        pix1 = pixRead(localname);
        pix2 = pixRead(namebuf);
        pixEqual(pix1, pix2, &same);
        pixDestroy(&pix1);
        pixDestroy(&pix2);
    } else {
        filesAreIdentical(localname, namebuf, &same);
    }
    if (!same) {
        fprintf(rp->fp, "Failure in %s, index %d: comparing %s with %s\n",
                rp->testname, (int)rp->index, localname, namebuf);
        lept_stderr("Failure in %s, index %d: comparing %s with %s\n",
                    rp->testname, (int)rp->index, localname, namebuf);
        rp->success = FALSE;
    }

    return 0;
}


/*!
 * \brief   regTestCompareFiles()
 *
 * \param[in]    rp        regtest parameters
 * \param[in]    index1    of one output file from reg test
 * \param[in]    index2    of another output file from reg test
 * \return  0 if OK, 1 on error
 *               Note: a failure in comparison is not an error
 *
 * <pre>
 * Notes:
 *      (1) This only does something in "compare" mode.
 *      (2) The canonical format of the golden filenames is:
 *            /tmp/lept/golden/[root of main name]_golden.[index].
 *                                                      [ext of localname]
 *          e.g.,
 *            /tmp/lept/golden/maze_golden.0.png
 * </pre>
 */
l_ok
regTestCompareFiles(L_REGPARAMS  *rp,
                    l_int32       index1,
                    l_int32       index2)
{
char    *name1, *name2;
char     namebuf[256];
l_int32  same;
SARRAY  *sa;

    if (!rp)
        return ERROR_INT("rp not defined", __func__, 1);
    if (index1 < 0 || index2 < 0) {
        rp->success = FALSE;
        return ERROR_INT("index1 and/or index2 is negative", __func__, 1);
    }
    if (index1 == index2) {
        rp->success = FALSE;
        return ERROR_INT("index1 must differ from index2", __func__, 1);
    }

    rp->index++;
    if (rp->cmd_mode != L_REG_COMPARE) return 0;

        /* Generate the golden file names */
    snprintf(namebuf, sizeof(namebuf), "%s_golden.%02d", rp->testname, index1);
    sa = getSortedPathnamesInDirectory("/tmp/lept/golden", namebuf, 0, 0);
    if (sarrayGetCount(sa) != 1) {
        sarrayDestroy(&sa);
        rp->success = FALSE;
        L_ERROR("golden file %s not found\n", __func__, namebuf);
        return 1;
    }
    name1 = sarrayGetString(sa, 0, L_COPY);
    sarrayDestroy(&sa);

    snprintf(namebuf, sizeof(namebuf), "%s_golden.%02d", rp->testname, index2);
    sa = getSortedPathnamesInDirectory("/tmp/lept/golden", namebuf, 0, 0);
    if (sarrayGetCount(sa) != 1) {
        sarrayDestroy(&sa);
        rp->success = FALSE;
        LEPT_FREE(name1);
        L_ERROR("golden file %s not found\n", __func__, namebuf);
        return 1;
    }
    name2 = sarrayGetString(sa, 0, L_COPY);
    sarrayDestroy(&sa);

        /* Test and record on failure */
    filesAreIdentical(name1, name2, &same);
    if (!same) {
        fprintf(rp->fp,
                "Failure in %s, index %d: comparing %s with %s\n",
                rp->testname, (int)rp->index, name1, name2);
        lept_stderr("Failure in %s, index %d: comparing %s with %s\n",
                    rp->testname, (int)rp->index, name1, name2);
        rp->success = FALSE;
    }

    LEPT_FREE(name1);
    LEPT_FREE(name2);
    return 0;
}


/*!
 * \brief   regTestWritePixAndCheck()
 *
 * \param[in]    rp       regtest parameters
 * \param[in]    pix      to be written
 * \param[in]    format   of output pix
 * \return  0 if OK, 1 on error
 *               Note: a failure in comparison is not an error
 *
 * <pre>
 * Notes:
 *      (1) This function makes it easy to write the pix in a numbered
 *          sequence of files, and either to:
 *             (a) write the golden file ("generate" arg to regression test)
 *             (b) make a local file and "compare" with the golden file
 *             (c) make a local file and "display" the results
 *      (2) The canonical format of the local filename is:
 *            /tmp/lept/regout/[root of main name].[count].[format extension]
 *          e.g., for scale_reg,
 *            /tmp/lept/regout/scale.0.png
 *          The golden file name mirrors this in the usual way.
 *      (3) The check is done between the written files, which requires
 *          the files to be identical. The exception is for GIF, which
 *          only requires that all pixels in the decoded pix are identical.
 * </pre>
 */
l_ok
regTestWritePixAndCheck(L_REGPARAMS  *rp,
                        PIX          *pix,
                        l_int32       format)
{
char  namebuf[256];

    if (!rp)
        return ERROR_INT("rp not defined", __func__, 1);
    if (!pix) {
        rp->success = FALSE;
        return ERROR_INT("pix not defined", __func__, 1);
    }
    if (!isSupportedFormat(format)) {
        rp->success = FALSE;
        return ERROR_INT("invalid format", __func__, 1);
    }

        /* Use bmp format for testing if library for requested
         * format for jpeg, png or tiff is not available */
    changeFormatForMissingLib(&format);

        /* Generate the local file name */
	assert(getFormatExtension(format) != NULL);
    snprintf(namebuf, sizeof(namebuf), "/tmp/lept/regout/%s.%02d.%s",
             rp->testname, rp->index + 1, getFormatExtension(format));

        /* Write the local file */
    if (pixGetDepth(pix) < 8)
        pixSetPadBits(pix, 0);
    pixWrite(namebuf, pix, format);

        /* Either write the golden file ("generate") or check the
           local file against an existing golden file ("compare") */
    regTestCheckFile(rp, namebuf);

    return 0;
}


/*!
 * \brief   regTestWriteDataAndCheck()
 *
 * \param[in]    rp      regtest parameters
 * \param[in]    data    to be written
 * \param[in]    nbytes  of data to be written
 * \param[in]    ext     filename extension (e.g.: "ba", "pta")
 * \return  0 if OK, 1 on error
 *               Note: a failure in comparison is not an error
 *
 * <pre>
 * Notes:
 *      (1) This function makes it easy to write data in a numbered
 *          sequence of files, and either to:
 *             (a) write the golden file ("generate" arg to regression test)
 *             (b) make a local file and "compare" with the golden file
 *             (c) make a local file and "display" the results
 *      (2) The canonical format of the local filename is:
 *            /tmp/lept/regout/[root of main name].[count].[ext]
 *          e.g., for the first boxaa in quadtree_reg,
 *            /tmp/lept/regout/quadtree.0.baa
 *          The golden file name mirrors this in the usual way.
 *      (3) The data can be anything.  It is most useful for serialized
 *          output of data, such as boxa, pta, etc.
 *      (4) The file extension is arbitrary.  It is included simply
 *          to make the content type obvious when examining written files.
 *      (5) The check is done between the written files, which requires
 *          the files to be identical.
 * </pre>
 */
l_ok
regTestWriteDataAndCheck(L_REGPARAMS  *rp,
                         void         *data,
                         size_t        nbytes,
                         const char   *ext)
{
char  namebuf[256];

    if (!rp)
        return ERROR_INT("rp not defined", __func__, 1);
    if (!data || nbytes == 0) {
        rp->success = FALSE;
        return ERROR_INT("data not defined or size == 0", __func__, 1);
    }

        /* Generate the local file name */
    snprintf(namebuf, sizeof(namebuf), "/tmp/lept/regout/%s.%02d.%s",
             rp->testname, rp->index + 1, ext);

        /* Write the local file */
    l_binaryWrite(namebuf, "w", data, nbytes);

        /* Either write the golden file ("generate") or check the
           local file against an existing golden file ("compare") */
    regTestCheckFile(rp, namebuf);
    return 0;
}


/*!
 * \brief   regTestGenLocalFilename()
 *
 * \param[in]       rp      regtest parameters
 * \param[in]       index   use -1 for current index
 * \param[in]       format  of image; e.g., IFF_PNG
 * \return  filename if OK, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This is used to get the name of a file in the regout
 *          subdirectory, that has been made and is used to test against
 *          the golden file.  You can either specify a particular index
 *          value, or with %index == -1, this returns the most recently
 *          written file.  The latter case lets you read a pix from a
 *          file that has just been written with regTestWritePixAndCheck(),
 *          which is useful for testing formatted read/write functions.
 *
 * </pre>
 */
char *
regTestGenLocalFilename(L_REGPARAMS  *rp,
                        l_int32       index,
                        l_int32       format)
{
char     buf[64];
l_int32  ind;

    if (!rp)
        return (char *)ERROR_PTR("rp not defined", __func__, NULL);

    ind = (index >= 0) ? index : (l_int32)rp->index;
    snprintf(buf, sizeof(buf), "/tmp/lept/regout/%s.%02d.%s",
             rp->testname, ind, getFormatExtension(format));
    return stringNew(buf);
}


/*!
 * \brief   getRootNameFromArgv0()
 *
 * \param[in]    argv0
 * \return  root name without the '_reg', or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) For example, from psioseg_reg, we want to extract
 *          just 'psioseg' as the root.
 *      (2) In unix with autotools, the executable is not X,
 *          but ./.libs/lt-X.   So in addition to stripping out the
 *          last 4 characters of the tail, we have to check for
 *          the '-' and strip out the "lt-" prefix if we find it.
 * </pre>
 */
static char *
getRootNameFromArgv0(const char  *argv0)
{
l_int32  len;
char    *root;

    splitPathAtDirectory(argv0, NULL, &root);
    if ((len = strlen(root)) <= 4) {
        LEPT_FREE(root);
        return (char *)ERROR_PTR("invalid argv0; too small", __func__, NULL);
    }

#ifndef _WIN32
    {
        char    *newroot;
        l_int32  loc;
        if (stringFindSubstr(root, "-", &loc)) {
            newroot = stringNew(root + loc + 1);  /* strip out "lt-" */
            LEPT_FREE(root);
            root = newroot;
            len = strlen(root);
        }
        len -= 4;  /* remove the "_reg" suffix */
    }
#else
    if (strstr(root, ".exe") != NULL)
        len -= 4;
    if (strstr(root, "_reg") == root + len - 4)
        len -= 4;
#endif  /* ! _WIN32 */

    root[len] = '\0';  /* terminate */

	if (strncmp(root, "lept_", 5) == 0)
		memmove(root, root + 5, strlen(root + 5) + 1);

	return root;
}


const char *
regGetRawArgOrDefault(L_REGPARAMS* rp, const char *default_value)
{
	if (!rp)
		return (char*)ERROR_PTR("rp not defined", __func__, NULL);

	int idx = rp->argv_index_base + rp->argv_index;
	if (idx >= sarrayGetCount(rp->argvfiles)) {
		return default_value ? stringNew(default_value) : NULL;
	}
	rp->argv_index++;

	const char* line = sarrayGetString(rp->argvfiles, idx, L_NOCOPY);
	switch (line ? line[0] : 0) {
	case 0x01:	/* stmt marker */
		return stringNew(line + 1);

	case 0x02: /* fail marker: '\x02 FAIL: ' */
	{
		char* fstr = stringNew(line);
		if (!fstr) {
			return (char*)ERROR_PTR("line text cannot be allocated", __func__, NULL);
		}
		fstr[0] = ';';  // --> '; FAIL: xyz'
		return fstr;
	}

	case 0x03: /* ignore marker: '\x03# ' */
		return stringNew(line + 1);

	default:
		return stringNew(line ?  line : "");
	}
}


const char*
regGetFileArgOrDefault(L_REGPARAMS* rp, const char* default_filepath)
{
	if (!rp)
		return (char*)ERROR_PTR("rp not defined", __func__, NULL);

	for (;;) {
		int idx = rp->argv_index_base + rp->argv_index;
		if (idx >= sarrayGetCount(rp->argvfiles)) {
			if (default_filepath) {
				SARRAY* sa = sarrayCreateInitialized(1, default_filepath);
				SARRAY* lcl_arr = leptProcessResponsefileLines(sa, rp->searchpaths, L_LOCATE_IN_FIRST_ONE, rp->outpath, "\x01" /* stmt marker */, "\x02 FAIL: " /* fail marker */, "\x03# " /* ignore marker */);
				int n = sarrayGetCount(lcl_arr);
				for (int i = 0; i < n; i++) {
					const char* line = sarrayGetString(lcl_arr, i, L_NOCOPY);
					switch (line ? line[0] : 0) {
					case 0:
					case 0x01:	/* stmt marker */
					case 0x02: /* fail marker */
					case 0x03: /* ignore marker */
						continue;

					default:
						line = stringNew(line);
						sarrayDestroy(&lcl_arr);
						return line;
					}
				}
			}

			return default_filepath ? stringNew(default_filepath) : NULL;
		}
		rp->argv_index++;

		const char* line = sarrayGetString(rp->argvfiles, idx, L_NOCOPY);
		switch (line ? line[0] : 0) {
		case 0:
		case 0x01:	/* stmt marker */
		case 0x02: /* fail marker */
		case 0x03: /* ignore marker */
			continue;

		default:
			return stringNew(line);
		}
	}
}


l_int32
regGetArgCount(L_REGPARAMS* rp)
{
	if (!rp)
		return ERROR_INT("rp not defined", __func__, 0);

	return sarrayGetCount(rp->argvfiles);
}


l_int32
regGetFileArgCount(L_REGPARAMS* rp)
{
	if (!rp)
		return ERROR_INT("rp not defined", __func__, 0);

	// save the old index values for recovery when we're done:
	int old_base = rp->argv_index_base;
	int old_index = rp->argv_index;
	int old_fake = rp->argv_fake_extra;

	// reset? nope, we deliver the count as of this moment in time...
#if 0
	rp->argv_index_base = 0;
	rp->argv_index = 0;
#endif
	rp->argv_fake_extra = 0;

	int argv_count = 0;

	for (;;) {
		// count only the actual files mentioned; ignore the rest
		const char* arg = regGetFileArgOrDefault(rp, NULL);
		if (!arg)
			break;
		++argv_count;
		stringDestroy(&arg);
	}

	// recover the argv scan indexes:
	rp->argv_index_base = old_base;
	rp->argv_index = old_index;
	rp->argv_fake_extra = old_fake;

	return argv_count;
}


void
regMarkEndOfTestround(L_REGPARAMS* rp)
{
	if (!rp) {
		L_ERROR("rp not defined\n", __func__);
		return;
	}

	if (rp->argv_step_size_per_round <= 0) {
		rp->argv_index_base += rp->argv_index;
	}
	else {
		rp->argv_index_base += rp->argv_step_size_per_round;
	}

	if (rp->argv_index_base >= sarrayGetCount(rp->argvfiles)) {
		if (rp->argv_fake_extra > 0) {
			rp->argv_fake_extra--;
		}
	}

	rp->argv_index = 0;
}


void
regMarkStartOfFirstTestround(L_REGPARAMS* rp, l_int32 extra_rounds)
{
	if (!rp) {
		L_ERROR("rp not defined\n", __func__);
		return;
	}

	// don't revisit any args which have already been grabbed.
	rp->argv_index_base = rp->argv_index;
	rp->argv_index = 0;
	rp->argv_fake_extra = extra_rounds;
}


l_ok
regHasFileArgsAvailable(L_REGPARAMS* rp)
{
	if (!rp)
		return ERROR_INT("rp not defined", __func__, 0);

	for (int i = 0; ; i++) {
		int idx = rp->argv_index_base + rp->argv_index + i;
		if (idx >= sarrayGetCount(rp->argvfiles)) {
			return (rp->argv_fake_extra > 0);   // time to start faking?
		}

		const char* line = sarrayGetString(rp->argvfiles, idx, L_NOCOPY);
		switch (line ? line[0] : 0) {
		case 0:
		case 0x01:	/* stmt marker */
		case 0x02: /* fail marker */
		case 0x03: /* ignore marker */
			continue;

		default:
			return TRUE;
		}
	}

	return FALSE;
}


l_ok
regHasAnyArgsAvailable(L_REGPARAMS* rp)
{
	if (!rp)
		return ERROR_INT("rp not defined", __func__, 0);

	int idx = rp->argv_index_base + rp->argv_index;
	if (idx >= sarrayGetCount(rp->argvfiles)) {
		return (rp->argv_fake_extra > 0);   // time to start faking?
	}

	return TRUE;
}


l_int32
regGetCurrentArgIndex(L_REGPARAMS* rp)
{
	if (!rp)
		return ERROR_INT("rp not defined", __func__, 0);

	int idx = rp->argv_index_base + rp->argv_index;
	if (idx >= sarrayGetCount(rp->argvfiles)) {
		return INT_MAX;   
	}

	return idx;
}

