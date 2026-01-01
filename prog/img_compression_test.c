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
 *   img_compression_test.c
 *
 */

#include "demo_settings.h"

#include <png.h>
#include <zlib-ng.h>

#include "monolithic_examples.h"


static const char* tsv_report_file_path = NULL;

static int handle_report_option(const L_REGCMD_OPTION_SPEC spec, const char* value, int* argc_ref, const char** argv_ref) {
	stringDestroy(&tsv_report_file_path);
	char* path = stringNew(value);
	convertSepCharsInPath(path, UNIX_PATH_SEPCHAR);
	tsv_report_file_path = path;
	return 0;
}

static struct L_RegCmdOptionSpec extras[] = {
	{
		L_CMD_OPT_W_REQUIRED_ARG,	// type
		"report",					// name
		"TSV file where the timing results are collected @ one row per input file.",
		handle_report_option,		// handler
		NULL						// data
	},

	{ L_CMD_OPT_NIL },
};

static L_REG_EXTRA_CONFIG extra_config = {
	"img_compression_test",	// testname
	1,						// min_required_argc
	INT_MAX,				// max_required_argc
	extras,					// extra_options
	L_LOCATE_IN_ALL			// argv_search_mode
};


static void collect(SARRAY * tsv_column_names, NUMA * tsv_timing_values, NUMA* tsv_fsize_values, const char *field_name, double elapsed, const char* target_fpath) {
	if (tsv_column_names) {
		sarrayAddString(tsv_column_names, field_name, L_COPY);
	}

	size_t fsize = 0;
	(void)lept_get_filesize(target_fpath, &fsize);
	numaAddNumber(tsv_fsize_values, fsize);

	numaAddNumber(tsv_timing_values, elapsed);
	if (!isnan(elapsed)) {
		lept_stderr("Time taken: %0.3lf msec\n", elapsed);
	}
}



#if defined(BUILD_MONOLITHIC)
#define main   lept_img_compression_test_main
#endif

int main(int          argc,
		 const char** argv)
{
	char       textstr[L_MAX(256, MAX_PATH)];
	PIX* pixs, * pixg, * pixf;
	L_REGPARAMS* rp;
	FILE* report_t_file = NULL;
	FILE* report_s_file = NULL;
	char* time_report_path = NULL;
	char* size_report_path = NULL;
	nanotimer_data_t time;

	if (regTestSetup(argc, argv, "img_compress", &extra_config, &rp))
		return 1;

	nanotimer(&time);

	if (tsv_report_file_path == NULL) {
		char* fname_base = pathExtractTail(rp->results_file_path, -1);
		tsv_report_file_path = string_asprintf("/tmp/lept/%s/%s.report.tsv", rp->testname, fname_base);
		stringDestroy(&fname_base);
	}

	if (tsv_report_file_path != NULL) {
		char* basedir = pathBasedir(tsv_report_file_path);
		char* fname_base = pathExtractTail(tsv_report_file_path, -1);
		time_report_path = string_asprintf("%s/%s_time.tsv", basedir, fname_base);
		size_report_path = string_asprintf("%s/%s_filesize.tsv", basedir, fname_base);
		stringDestroy(&fname_base);
		stringDestroy(&basedir);
		report_t_file = fopenWriteStream(time_report_path, "w");
		if (!report_t_file) {
			L_ERROR("failed to open output/report file '%s'\n", __func__, time_report_path);
			rp->success = FALSE;
			goto ende;
		}
		report_s_file = fopenWriteStream(size_report_path, "w");
		if (!report_s_file) {
			L_ERROR("failed to open output/report file '%s'\n", __func__, size_report_path);
			rp->success = FALSE;
			goto ende;
		}
	}

	// every input file is treated as another round and represents the parent level in the step hierarchy:
	//int steplevel = leptDebugGetStepLevel();

	l_ok first_row = TRUE;

	int argv_count = regGetArgCount(rp);
	if (argv_count == 0) {
		L_WARNING("no image files specified on the command line for processing: assuming a default input set.\n", __func__);
	}
	for (regMarkStartOfFirstTestround(rp, +1); regHasFileArgsAvailable(rp); regMarkEndOfTestround(rp))
	{
		// precaution: make sure we are at the desired depth in every round, even if called code forgot or failed to pop their additional level(s)
		leptDebugPopStepLevelTo(rp->base_step_level);

		SARRAY* tsv_column_names = NULL;
		NUMA* tsv_timing_values = numaCreate(0);
		NUMA* tsv_fsize_values = numaCreate(0);
		if (first_row) {
			tsv_column_names = sarrayCreate(0);
			sarrayAddString(tsv_column_names, "#", L_COPY);
			sarrayAddString(tsv_column_names, "filename", L_COPY);
			first_row = FALSE;
		}

		const char* filepath = regGetFileArgOrDefault(rp, "1555.007.jpg");
		leptDebugSetStepIdAtSLevel(-1, regGetCurrentArgIndex(rp));   // inc parent level
		leptDebugSetFilePathPartFromTail(filepath, -2);

		{
			char* destdir = leptDebugGenFilepath("");
			char* real_destdir = genPathname(destdir, NULL);
			lept_stderr("\n\n\nProcessing image #%d~#%s:\n  %s :: %s.<output>\n    --> %s.<output>\n", regGetCurrentArgIndex(rp), leptDebugGetStepIdAsString(), filepath, destdir, real_destdir);
			stringDestroy(&real_destdir);
		}
		numaAddNumber(tsv_timing_values, regGetCurrentArgIndex(rp));

		nanotimer_start(&time);
		pixs = pixRead(filepath);
		collect(tsv_column_names, tsv_timing_values, tsv_fsize_values, "Fetch", nanotimer_get_elapsed_ms(&time), filepath);

		snprintf(textstr, sizeof(textstr), "source: %s", filepath);
		pixSetText(pixs, textstr);

		l_int32 img_depth = pixGetDepth(pixs);

		pixg = pixConvertTo8(pixs, 0);
		pixSetText(pixg, "(grayscale)");

		pixf = pixConvertTo32(pixs);
		pixSetText(pixf, "(RGB)");

		const char* source_fname = pathExtractTail(filepath, -1);
		const char* pixpath = NULL;

		for (int i = 1; i < 100; i++) {
			switch (i) {
			default:
				continue;

			case IFF_BMP:
				pixpath = leptDebugGenFilepath("%s-%03d.%s", source_fname, i, getFormatExtension(i));
				lept_stderr("Writing to: %s\n", pixpath);
				nanotimer_start(&time);
				pixWrite(pixpath, pixf, i);
				collect(tsv_column_names, tsv_timing_values, tsv_fsize_values, getFormatExtension(i), nanotimer_get_elapsed_ms(&time), pixpath);
				continue;

			case IFF_JFIF_JPEG:
				for (int q = 100; q >= 0; ) {
					char field[40];
					snprintf(field, sizeof(field), "%s@%d", getFormatExtension(i), q);
					pixpath = leptDebugGenFilepath("%s-Qual-%03d-%03d.%s", source_fname, q, i, getFormatExtension(i));
					lept_stderr("Writing to: %s     @ quality: %3d%%\n", pixpath, q);
					nanotimer_start(&time);
					l_jpegSetQuality(q);
					pixWrite(pixpath, pixf, i);
					collect(tsv_column_names, tsv_timing_values, tsv_fsize_values, field, nanotimer_get_elapsed_ms(&time), pixpath);

					if (q == 100)
						q = 99;
					else if (q == 99)
						q = 95;
					else if (q > 80)
						q -= 5;
					else if (q > 0)
						q -= 10;
					else
						--q;
				}
				continue;

			case IFF_PNG:
#if 0
				for (int q = 100; q >= 0; ) {
					char field[40];
					snprintf(field, sizeof(field), "%s@%d", getFormatExtension(i), q);
					pixpath = leptDebugGenFilepath("%s-Qual-%03d-%03d.%s", source_fname, q, i, getFormatExtension(i));
					lept_stderr("Writing to: %s     @ quality: %3d%%\n", pixpath, q);
					nanotimer_start(&time);
					l_pngSetQuality(q);
					pixWrite(pixpath, pixf, i);
					collect(tsv_column_names, tsv_timing_values, tsv_fsize_values, field, nanotimer_get_elapsed_ms(&time), pixpath);

					q -= 10;
				}
#else
				{
					unsigned int sp[10000] = { 0 };
					int max_pos = 0;
					for (unsigned int spec_bits = 0; ; spec_bits++) {
						/*
						 * Definition of methods:
						 *
						 * - PNG defines 4 filters, which may be combined, plus 'none':
						 *   PNG_FILTER_NONE or { PNG_FILTER_SUB, PNG_FILTER_UP, PNG_FILTER_AVG, PNG_FILTER_PAETH }
						 *
						 * - 9 compression levels 1..9 + 0 = no compression, which makes 10 levels.
						 *
						 * - 4 strategies, where only Z_FILTERED will apply the filters, AFAICT.
						 *
						 * - RLE compression window 8..15, which is unused when the strategy is Z_HUFFMAN_ONLY.
						 *   Meanwhile RLE compression is more or less independent of the compression level 1..9:
						 *   pngcrush says 1..3 are the same and so are 4..9.
						 *
						 */
						unsigned int spec = spec_bits;
						int filter_type = spec & 0x1F;
						spec >>= 5;
						int strategy = spec & 0x07;   // 4 strategies + 0 = 'default' makes 5, taking up 3 bits.
						spec >>= 3;
						int compression = spec & 0x0F;   // 10 compression levels, taking up 4 bits.
						spec >>= 4;
						int window = spec;			   // 8 window sizes, taking up 3 bits.
						spec >>= 3;
						if (spec > 0)
							break;

						filter_type <<= 3;				// 0b01 --> PNG_FILTER_NONE, etc.
						if (filter_type < PNG_FILTER_NONE || filter_type > PNG_ALL_FILTERS) {
							continue;
						}

						if (compression < Z_NO_COMPRESSION || compression > Z_BEST_COMPRESSION) {
							continue;
						}

						window += 8;
						if (window < 8 || window > 15) {
							continue;
						}

						if (strategy < Z_FILTERED || strategy > Z_FIXED) {
							continue;
						}

						sp[max_pos++] = spec_bits + 100;
					}

					sp[max_pos] = 0;
					assert(max_pos < sizeof(sp) / sizeof(sp[0]));

					for (int q = max_pos; q >= 0; ) {
						char field[40];
						unsigned int spec = sp[q];
						unsigned int flags = (spec >= 100 ? spec - 100 : 0);
						snprintf(field, sizeof(field), "%s@%d.%04X", getFormatExtension(i), q, flags);
						pixpath = leptDebugGenFilepath("%s-Qual-%03d.%04X-%03d.%s", source_fname, q, flags, i, getFormatExtension(i));
						lept_stderr("Writing to: %s     @ quality: %3d%% (special flags: 0x%04X)\n", pixpath, q, flags);
						nanotimer_start(&time);
						pixSetSpecial(pixf, spec);

						size_t png_size;
						l_uint8* png_data;
						(void)pixWriteMem(&png_data, &png_size, pixf, i);
						LEPT_FREE(png_data);

						pixWrite(pixpath, pixf, i);
						collect(tsv_column_names, tsv_timing_values, tsv_fsize_values, field, nanotimer_get_elapsed_ms(&time), pixpath);

						q--;
					}
				}
#endif
				continue;

			case IFF_TIFF:
				for (int q = 100; q >= 0; ) {
					char field[40];
					snprintf(field, sizeof(field), "%s@%d", getFormatExtension(i), q);
					pixpath = leptDebugGenFilepath("%s-Qual-%03d-std-%03d.%s", source_fname, q, i, getFormatExtension(i));
					lept_stderr("Writing to: %s     @ quality: %3d%%\n", pixpath, q);
					nanotimer_start(&time);
					l_tiffSetQuality(q);
					pixWrite(pixpath, pixf, i);
					collect(tsv_column_names, tsv_timing_values, tsv_fsize_values, field, nanotimer_get_elapsed_ms(&time), pixpath);

					if (q == 100)
						q = 99;
					else if (q == 99)
						q = 95;
					else if (q > 80)
						q -= 5;
					else if (q > 0)
						q -= 10;
					else
						--q;
				}
				continue;

			case IFF_TIFF_PACKBITS:
#if 01		// this mode only supports binary images and we're not testing those ATM!
				if (img_depth == 1) {
					pixpath = leptDebugGenFilepath("%s-packbits-%03d.%s", source_fname, i, getFormatExtension(i));
					lept_stderr("Writing to: %s\n", pixpath);
					nanotimer_start(&time);
					pixWrite(pixpath, pixs, i);
					collect(tsv_column_names, tsv_timing_values, tsv_fsize_values, "tiff-packbits", nanotimer_get_elapsed_ms(&time), pixpath);
				}
				else {
					collect(tsv_column_names, tsv_timing_values, tsv_fsize_values, "tiff-packbits", NAN, NULL);
				}
#endif
				continue;

			case IFF_TIFF_RLE:
#if 01		// this mode only supports binary images and we're not testing those ATM!
				if (img_depth == 1) {
					pixpath = leptDebugGenFilepath("%s-rle-%03d.%s", source_fname, i, getFormatExtension(i));
					lept_stderr("Writing to: %s\n", pixpath);
					nanotimer_start(&time);
					pixWrite(pixpath, pixs, i);
					collect(tsv_column_names, tsv_timing_values, tsv_fsize_values, "tiff-rle", nanotimer_get_elapsed_ms(&time), pixpath);
				}
				else {
					collect(tsv_column_names, tsv_timing_values, tsv_fsize_values, "tiff-rle", NAN, NULL);
				}
#endif
				continue;

			case IFF_TIFF_G3:
#if 01		// this mode only supports binary images and we're not testing those ATM!
				if (img_depth == 1) {
					pixpath = leptDebugGenFilepath("%s-G3-%03d.%s", source_fname, i, getFormatExtension(i));
					lept_stderr("Writing to: %s\n", pixpath);
					nanotimer_start(&time);
					pixWrite(pixpath, pixs, i);
					collect(tsv_column_names, tsv_timing_values, tsv_fsize_values, "tiff-g3", nanotimer_get_elapsed_ms(&time), pixpath);
				}
				else {
					collect(tsv_column_names, tsv_timing_values, tsv_fsize_values, "tiff-g3", NAN, NULL);
				}
#endif
				continue;

			case IFF_TIFF_G4:
#if 01		// this mode only supports binary images and we're not testing those ATM!
				if (img_depth == 1) {
					pixpath = leptDebugGenFilepath("%s-G4-%03d.%s", source_fname, i, getFormatExtension(i));
					lept_stderr("Writing to: %s\n", pixpath);
					nanotimer_start(&time);
					pixWrite(pixpath, pixs, i);
					collect(tsv_column_names, tsv_timing_values, tsv_fsize_values, "tiff-g4", nanotimer_get_elapsed_ms(&time), pixpath);
				}
				else {
					collect(tsv_column_names, tsv_timing_values, tsv_fsize_values, "tiff-g4", NAN, NULL);
				}
#endif
				continue;

			case IFF_TIFF_LZW:
				pixpath = leptDebugGenFilepath("%s-lzw-%03d.%s", source_fname, i, getFormatExtension(i));
				lept_stderr("Writing to: %s\n", pixpath);
				nanotimer_start(&time);
				pixWrite(pixpath, pixf, i);
				collect(tsv_column_names, tsv_timing_values, tsv_fsize_values, "tiff-lzw", nanotimer_get_elapsed_ms(&time), pixpath);
				continue;

			case IFF_TIFF_ZIP:
				for (int q = 100; q >= 0; ) {
					char field[40];
					snprintf(field, sizeof(field), "%s-zip@%d", getFormatExtension(i), q);
					pixpath = leptDebugGenFilepath("%s-Qual-%03d-zip-%03d.%s", source_fname, q, i, getFormatExtension(i));
					lept_stderr("Writing to: %s     @ quality: %3d%%\n", pixpath, q);
					nanotimer_start(&time);
					l_tiffSetQuality(q);
					pixWrite(pixpath, pixf, i);
					collect(tsv_column_names, tsv_timing_values, tsv_fsize_values, field, nanotimer_get_elapsed_ms(&time), pixpath);

					q -= 10;
				}
				continue;

			case IFF_PNM:
				pixpath = leptDebugGenFilepath("%s-%03d.%s", source_fname, i, getFormatExtension(i));
				lept_stderr("Writing to: %s\n", pixpath);
				nanotimer_start(&time);
				pixWrite(pixpath, pixf, i);
				collect(tsv_column_names, tsv_timing_values, tsv_fsize_values, getFormatExtension(i), nanotimer_get_elapsed_ms(&time), pixpath);
				continue;

			case IFF_PS:
				pixpath = leptDebugGenFilepath("%s-%03d.%s", source_fname, i, getFormatExtension(i));
				lept_stderr("Writing to: %s\n", pixpath);
				nanotimer_start(&time);
				pixWrite(pixpath, pixf, i);
				collect(tsv_column_names, tsv_timing_values, tsv_fsize_values, getFormatExtension(i), nanotimer_get_elapsed_ms(&time), pixpath);
				continue;

			case IFF_GIF:
				pixpath = leptDebugGenFilepath("%s-%03d.%s", source_fname, i, getFormatExtension(i));
				lept_stderr("Writing to: %s\n", pixpath);
				nanotimer_start(&time);
				pixWrite(pixpath, pixf, i);
				collect(tsv_column_names, tsv_timing_values, tsv_fsize_values, getFormatExtension(i), nanotimer_get_elapsed_ms(&time), pixpath);
				continue;

			case IFF_JP2:
				for (int q = 100; q >= 0; ) {
					char field[40];
					snprintf(field, sizeof(field), "%s@%d", getFormatExtension(i), q);
					pixpath = leptDebugGenFilepath("%s-Qual-%03d-%03d.%s", source_fname, q, i, getFormatExtension(i));
					lept_stderr("Writing to: %s     @ quality: %3d%%\n", pixpath, q);
					nanotimer_start(&time);
					l_jp2SetQuality(q);
					pixWrite(pixpath, pixf, i);
					collect(tsv_column_names, tsv_timing_values, tsv_fsize_values, field, nanotimer_get_elapsed_ms(&time), pixpath);

					// SNR range seems to be 45..27:
					if (q > 60)
						q -= 20;
					else if (q == 60)
						q = 45;
					else if (q >= 40)
						q -= 2;
					else if (q >= 24)
						q -= 1;
					else if (q >= 15)
						q -= 5;
					else 
						break;
				}
				continue;

			case IFF_WEBP:
				for (int q = 100; q >= 0; ) {
					char field[40];
					snprintf(field, sizeof(field), "%s@%d", getFormatExtension(i), q);
					pixpath = leptDebugGenFilepath("%s-Qual-%03d-%03d.%s", source_fname, q, i, getFormatExtension(i));
					lept_stderr("Writing to: %s     @ quality: %3d%%\n", pixpath, q);
					nanotimer_start(&time);
					l_webpSetQuality(q);
					pixWrite(pixpath, pixf, i);
					collect(tsv_column_names, tsv_timing_values, tsv_fsize_values, field, nanotimer_get_elapsed_ms(&time), pixpath);

					if (q == 100)
						q = 99;
					else if (q == 99)
						q = 95;
					else if (q > 80)
						q -= 5;
					else if (q > 15)
						q -= 10;
					else if (q > 0)
						q -= 2;
					else
						--q;
				}
				continue;

			case IFF_LPDF:
				pixpath = leptDebugGenFilepath("%s-%03d.%s", source_fname, i, getFormatExtension(i));
				lept_stderr("Writing to: %s\n", pixpath);
				nanotimer_start(&time);
				pixWrite(pixpath, pixf, i);
				collect(tsv_column_names, tsv_timing_values, tsv_fsize_values, getFormatExtension(i), nanotimer_get_elapsed_ms(&time), pixpath);
				continue;

			case IFF_TIFF_JPEG:
				for (int q = 100; q >= 0; ) {
					char field[40];
					snprintf(field, sizeof(field), "%s-jpeg@%d", getFormatExtension(i), q);
					pixpath = leptDebugGenFilepath("%s-Qual-%03d-jpeg-%03d.%s", source_fname, q, i, getFormatExtension(i));
					lept_stderr("Writing to: %s     @ quality: %3d%%\n", pixpath, q);
					nanotimer_start(&time);
					l_tiffSetQuality(q);
					pixWrite(pixpath, pixf, i);
					collect(tsv_column_names, tsv_timing_values, tsv_fsize_values, field, nanotimer_get_elapsed_ms(&time), pixpath);

					if (q == 100)
						q = 99;
					else if (q == 99)
						q = 95;
					else if (q > 80)
						q -= 5;
					else if (q > 40)
						q -= 10;
					else if (q > 0)
						q -= 5;
					else
						--q;
				}
				continue;

			case IFF_DEFAULT:
#if 0  // --> pixChooseOutputFormat(pix) --> PNG / TIFF.G4
				pixpath = leptDebugGenFilepath("%s-%03d.%s", source_fname, i, getFormatExtension(i));
				lept_stderr("Writing to: %s\n", pixpath);
				nanotimer_start(&time);
				pixWrite(pixpath, pixf, i);
				collect(tsv_column_names, tsv_timing_values, tsv_fsize_values, getFormatExtension(i), nanotimer_get_elapsed_ms(&time), pixpath);
#endif
				continue;

			case IFF_SPIX:
				pixpath = leptDebugGenFilepath("%s-%03d.%s", source_fname, i, getFormatExtension(i));
				lept_stderr("Writing to: %s\n", pixpath);
				nanotimer_start(&time);
				pixWrite(pixpath, pixf, i);
				collect(tsv_column_names, tsv_timing_values, tsv_fsize_values, "spix", nanotimer_get_elapsed_ms(&time), pixpath);
				continue;
			}
		}

		pixDestroy(&pixs);
		pixDestroy(&pixg);
		pixDestroy(&pixf);

		if (tsv_column_names) {
			for (int i = 0; i < sarrayGetCount(tsv_column_names); i++) {
				fprintf(report_t_file, "%s\t", sarrayGetString(tsv_column_names, i, L_NOCOPY));
				fprintf(report_s_file, "%s\t", sarrayGetString(tsv_column_names, i, L_NOCOPY));
			}
			fprintf(report_t_file, "\n");
			fprintf(report_s_file, "\n");

			sarrayDestroy(&tsv_column_names);
		}
		{
			l_int32 line;
			numaGetIValue(tsv_timing_values, 0, &line);
			fprintf(report_t_file, "%d\t%s\t", line, filepath);
			fprintf(report_s_file, "%d\t%s\t", line, filepath);

			for (int i = 1; i < numaGetCount(tsv_timing_values); i++) {
				l_float32 t_ms;
				numaGetFValue(tsv_timing_values, i, &t_ms);
				fprintf(report_t_file, "%0.4f\t", (float)t_ms);
			}
			fprintf(report_t_file, "\n");

			for (int i = 1; i < numaGetCount(tsv_fsize_values); i++) {
				l_float32 t_fs;
				numaGetFValue(tsv_fsize_values, i, &t_fs);
				size_t fsize = round(t_fs);
				fprintf(report_s_file, "%zu\t", fsize);
			}
			fprintf(report_s_file, "\n");

			numaDestroy(&tsv_timing_values);
			numaDestroy(&tsv_fsize_values);
		}

		stringDestroy(&source_fname);

		leptDebugClearLastGenFilepathCache();
	}

ende:
	//leptDebugPopStepLevelTo(rp->base_step_level);

	if (report_t_file) {
		fclose(report_t_file);
		report_t_file = NULL;
	}
	if (report_s_file) {
		fclose(report_s_file);
		report_s_file = NULL;
	}
	stringDestroy(&tsv_report_file_path);
	stringDestroy(&time_report_path);
	stringDestroy(&size_report_path);

	nanotimer_destroy(&time);

	return regTestCleanup(rp);
}
