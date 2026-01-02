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

struct stat_data_elem {
	uint16_t filter_type;
	uint16_t strategy;
	uint16_t compression;
	uint16_t window;

	float filesize;		// as we want to compare these to discover the 'tightest' output for various input files, we need this to be the 'normalized' filesize.
	float time_spent;

	unsigned int flags;
};

static int compare_png_test_results(const void* a, const void* b)
{
	struct stat_data_elem* ea = (struct stat_data_elem*)a;
	struct stat_data_elem* eb = (struct stat_data_elem*)b;

	// what is smaller?
	//
	// we focus on SPEED here, so first is time, then is size.
	// and to keep the elements sorted in stable order, we also compare the other members (guaranteed unique as a set, per element)
	if (ea->time_spent < eb->time_spent)
		return -1;
	if (ea->time_spent > eb->time_spent)
		return +1;

	if (ea->filesize < eb->filesize)
		return -1;
	if (ea->filesize > eb->filesize)
		return +1;

	if (ea->strategy < eb->strategy)
		return -1;
	if (ea->strategy > eb->strategy)
		return +1;

	if (ea->compression < eb->compression)
		return -1;
	if (ea->compression > eb->compression)
		return +1;

	if (ea->window < eb->window)
		return -1;
	if (ea->window > eb->window)
		return +1;

	if (ea->filter_type < eb->filter_type)
		return -1;
	if (ea->filter_type > eb->filter_type)
		return +1;

	if (ea->flags < eb->flags)
		return -1;
	if (ea->flags > eb->flags)
		return +1;

	return 0;
}

static float calc_BH_attract(const float log_time[], int c_idx, int end_idx)
{
	if (end_idx >= c_idx) {
		float sum = 0;
		float cv = log_time[c_idx];
		int i = 1;
		int l = end_idx - c_idx;
		if (i < l) {
			float ev = log_time[end_idx];
			float ed = ev - cv;
			ed /= 3;

			float iv = log_time[c_idx + i];
			float d1 = iv - cv;
			if (d1 <= ed) {
				float pwr1 = i * i;
				pwr1 *= d1 * d1;
				sum = 1.0 / (pwr1 + 1e-9);

				for (i = 2; i < l; i++) {
					float iv = log_time[c_idx + i];
					float d = iv - cv;
					if (d <= ed) {
						float pwr = i * i;
						pwr *= d * d;
						sum += 1.0 / (pwr + 1e-9);
					}
					else {
						break;
					}
				}
			}
		}
		return sum;
	}
	else {
		// end_idx < c_idx
		float sum = 0;
		float cv = log_time[c_idx];
		int i = 1;
		int l = c_idx - end_idx;
		if (i < l) {
			float ev = log_time[end_idx];
			float ed = cv - ev;
			ed /= 3;

			float iv = log_time[c_idx - i];
			float d1 = cv - iv;
			if (d1 <= ed) {
				float pwr1 = i * i;
				pwr1 *= d1 * d1;
				sum = 1.0 / (pwr1 + 1e-9);

				for (i = 2; i < l; i++) {
					float iv = log_time[c_idx - i];
					float d = cv - iv;
					if (d <= ed) {
						float pwr = i * i;
						pwr *= d * d;
						sum += 1.0 / (pwr + 1e-9);
					}
					else {
						break;
					}
				}
			}
		}
		return sum;
	}
}

static int locate_nearby_best_compression(const float log_time[], const struct stat_data_elem st[], int c_idx, int start_idx, int end_idx, int prev_opti_idx)
{
	// - end_idx and start_idx are EXCLUSIVE indices.
	// - as the previous round of the calling code has updated the previous center index, which is 'start_idx' now,
	//   we virtually guarantee that two subsequent clusters cannot ever have the same center index after we are done here!
	//
	// Hence we can drop the distance/6 criterium: this may deliver a (rare) better compression at a slightly lower
	// speed, since we now will be looking *beyond* the current cluster: halfway into the next cluster!

	float cfs = st[c_idx].filesize;

	float cv = log_time[c_idx];
	float ev = log_time[end_idx - 1];
	float sv = log_time[start_idx + 1];
	float ed = ev - sv;
	ed /= 3 * 2;
	ed *= ed;

	for (int i = 1, l = end_idx - start_idx; i < l; i++) {
		float iv = log_time[start_idx + i];
		float d = iv - cv;
		d *= d;
		/* if (d < ed)    <-- see notes above */ {
			float ifs = st[start_idx + i].filesize;
			if (ifs < cfs) {
				c_idx = start_idx + i;
				cfs = ifs;
			}
		}
	}

	// now that we have this number (index), *anything* faster & tighter wins anyhow, *iff* it hasn't been selected as the next-faster setting already:
	for (int i = prev_opti_idx + 1, l = c_idx; i < l; i++) {
		float ifs = st[i].filesize;
		if (ifs < cfs) {
			c_idx = i;
			cfs = ifs;
		}
	}

	return c_idx;
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
				if (01)
				{
					l_int32 w, h;

					pixGetDimensions(pixf, &w, &h, NULL);
					size_t filesize_norm = w;
					filesize_norm *= h;
					filesize_norm *= 4; // RGBA = 4 bytes

					struct stat_data_elem st[10000];
					static struct stat_data_elem st_accu[10000] = { { 0 } };
					static int init = 0;
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

						// don't run this 10K test loop for (very) large images as it'll take ages!
						if (!init || (w <= 1400 && h <= 1400)) {
							nanotimer_start(&time);
							pixSetSpecial(pixf, spec_bits + 100);

							size_t png_size;
							l_uint8* png_data;
							(void)pixWriteMem(&png_data, &png_size, pixf, i);
							LEPT_FREE(png_data);

							double elapsed = nanotimer_get_elapsed_ms(&time);
							double filesize_normalized = png_size;
							filesize_normalized /= filesize_norm;

							struct stat_data_elem info = {
							.filter_type = filter_type,
							.strategy = strategy,
							.compression = compression,
							.window = window,

							.filesize = filesize_normalized,
							.time_spent = elapsed,

							.flags = spec_bits
							};

							st[max_pos] = info;

							st_accu[max_pos].filesize += filesize_normalized;
							st_accu[max_pos].time_spent += elapsed;

							if ((max_pos & 0x1F) == 0) {
								float progress = max_pos;
								progress /= 10000;
								progress *= 100.0;
								lept_stderr("Testing @ %0.3f%%\n", progress);
							}

							// Also:
							// tweak the current stats[] as we are interested in producing a 'global optimum' anyway,
							// not something bespoke for the current pix.
							st[max_pos].filesize = st_accu[max_pos].filesize;
							st[max_pos].time_spent = st_accu[max_pos].time_spent;
						}
						else {
							st[max_pos] = st_accu[max_pos];
						}

						max_pos++;
					}

					assert(max_pos < sizeof(st) / sizeof(st[0]));

					if (!init) {
						memcpy(st_accu, st, sizeof(st_accu));
						init = 1;
					}

					qsort(st, max_pos, sizeof(st[0]), compare_png_test_results);

					// now we have a bunch of results, sorted by time ~ performance.
					//
					// we want to know, per time 'slot', which set of parameters produced the 'best' ~ *tightest* PNG:
					// first we need to determine the range of each 'time slot':
					// the obvious approach would be to get the N quantiles, but the time range has several clusters
					// and we want one slot per cluster, without any slower cluster-element(s) leaking into our
					// 'faster' slot(s), hence we need to do the non-obvious instead and discover the cluster boundaries
					// for N clusters.

					// we assume a log(time) distribution, so we construct a histogram on log(time) basis instead of (time) itself:
					float log_time[10000];
					for (int i = 0; i < max_pos; i++) {
						log_time[i] = log(st[i].time_spent + 1.0);
					}

					// we want N clusters: N=20
#define N 20
					struct cluster_node {
						//short int low_idx;	// inclusive
						//short int max_idx;	// exclusive

						short int center_idx;
					} clusters[N] = { { 0 } };

					for (int i = 1; i < N - 1; i++) {
						int low_idx = (max_pos * i) / N;
						int span = max_pos / N;
						int center_idx = low_idx + span / 2;
						clusters[i].center_idx = center_idx;

						//clusters[i].low_idx = center_idx;
						//clusters[i].max_idx = center_idx + 1;
					}
					clusters[0].center_idx = 0;
					//clusters[0].low_idx = 0;
					//clusters[0].max_idx = 1;
					clusters[N - 1].center_idx = max_pos - 1;
					//clusters[N - 1].low_idx = max_pos - 1;
					//clusters[N - 1].max_idx = max_pos;

					// we now have equal *sized* clusters in terms of node count.
					// This is, of course, horrible and must be corrected: we use a Barnes-Hut type
					// approach to 'center' each cluster node.

					for (;;) {
						int change = 0;

						for (int i = 1; i < N - 1; i++) {
							int c_idx = clusters[i].center_idx;
							int pc_idx = clusters[i - 1].center_idx;
							int nc_idx = clusters[i + 1].center_idx;

							float cv = log_time[c_idx];
							float lv = log_time[pc_idx];
							float rv = log_time[nc_idx];
							float ld = cv - lv;
							float rd = rv - cv;
							while (ld < rd) {
								c_idx++;
								cv = log_time[c_idx];
								ld = cv - lv;
								rd = rv - cv;
							}
							while (ld > rd) {
								c_idx--;
								cv = log_time[c_idx];
								ld = cv - lv;
								rd = rv - cv;
							}
							if (c_idx != clusters[i].center_idx) {
								change = 1;
								clusters[i].center_idx = c_idx;
							}
						}

						if (!change) {
							break;
						}
					}

					for (;;) {
						int change = 0;

						for (int i = 1; i < N - 1; i++) {
							int c_idx = clusters[i].center_idx;
							int pc_idx = clusters[i - 1].center_idx;
							int nc_idx = clusters[i + 1].center_idx;

							float ld = calc_BH_attract(log_time, c_idx, pc_idx);
							float rd = calc_BH_attract(log_time, c_idx, nc_idx);
							while (ld < rd) {
								c_idx++;
								ld = calc_BH_attract(log_time, c_idx, pc_idx);
								rd = calc_BH_attract(log_time, c_idx, nc_idx);
							}
							while (ld > rd) {
								c_idx--;
								ld = calc_BH_attract(log_time, c_idx, pc_idx);
								rd = calc_BH_attract(log_time, c_idx, nc_idx);
							}
							if (c_idx != clusters[i].center_idx) {
								change = 1;
								clusters[i].center_idx = c_idx;
							}
						}

						if (!change) {
							break;
						}
					}

					// now find the 'best compression' that's close to the cluster center:
					{
						struct cluster_node opti[N];

						int c_idx = clusters[0].center_idx;
						int nc_idx = clusters[1].center_idx;
						c_idx = locate_nearby_best_compression(log_time, st, c_idx, -1, nc_idx, -1);
						opti[0].center_idx = c_idx;

						for (int i = 1; i < N - 1; i++) {
							int c_idx = clusters[i].center_idx;
							int pc_idx = clusters[i - 1].center_idx;
							int nc_idx = clusters[i + 1].center_idx;

							c_idx = locate_nearby_best_compression(log_time, st, c_idx, pc_idx, nc_idx, opti[i - 1].center_idx);
							opti[i].center_idx = c_idx;
						}

						c_idx = clusters[N - 1].center_idx;
						int pc_idx = clusters[N - 2].center_idx;

						c_idx = locate_nearby_best_compression(log_time, st, c_idx, pc_idx, max_pos, opti[N - 2].center_idx);
						opti[N - 1].center_idx = c_idx;

						for (int i = 0; i < N; i++) {
							clusters[i].center_idx = opti[i].center_idx;
						}

						//---------------
						const char* tablepath = leptDebugGenFilepath("%s-%03d.png-opti-flags.c", source_fname, i);
						FILE* f = lept_fopen(tablepath, "w");
						if (f) {
							fprintf(f, "\n"
								"/*\n"
								"struct stat_data_elem {\n"
								"	uint16_t filter_type;\n"
								"	uint16_t strategy;\n"
								"	uint16_t compression;\n"
								"	uint16_t window;\n"
								"\n"
								"	float filesize;    // as we want to compare these to discover the 'tightest' output for various input files, we need this to be the 'normalized' filesize.\n"
								"	float time_spent;\n"
								"\n"
								"	unsigned int flags;\n"
								"};\n"
								"*/\n"
								"\n");
							fprintf(f, "\n"
								"static const unsigned int pngBespokeSpecials[%d + 1] = {\n",
								N);
							for (int i = 0; i < N; i++) {
								unsigned int idx = clusters[i].center_idx;
								struct stat_data_elem* info = &st[idx];
								unsigned int spec = info->flags;
								unsigned int flags = spec;
								spec += 100;
								fprintf(f, "  %d, // filter_type: 0x%02X, strategy: %d, compression: %d, window: %d, filesize:ratio: %f, time_spent: %f, flags: 0x%04X\n",
									spec,

									info->filter_type,
									info->strategy,
									info->compression,
									info->window,
									info->filesize,
									info->time_spent,
									info->flags);
							}
							fprintf(f, "  0  // all defaults\n"
								"};\n"
								"\n");

							fprintf(f, "\n"
								"static const struct kpi_datapoint {\n"
								"  unsigned int spec;\n"
								"  float filesize_norm_sum;\n"
								"  float elapsed_sum;\n"
								"} pngSpecialsKPI[%d] = {\n",
								max_pos);
							for (int i = 0; i < max_pos; i++) {
								struct stat_data_elem* info = &st[i];
								unsigned int spec = info->flags;
								unsigned int flags = spec;
								spec += 100;
								fprintf(f, "  { %d, %f, %f }, // filter_type: 0x%02X, strategy: %d, compression: %d, window: %d, filesize:ratio: %f, time_spent: %f, flags: 0x%04X\n",
									spec,
									info->filesize,
									info->time_spent,

									info->filter_type,
									info->strategy,
									info->compression,
									info->window,
									info->filesize,
									info->time_spent,
									info->flags);
							}
							fprintf(f, "};\n"
								"\n");

							fclose(f);
						}
					}

					for (int q = N - 1; q >= 0; ) {
						char field[40];
						unsigned int idx = clusters[q].center_idx;
						struct stat_data_elem* info = &st[idx];
						unsigned int spec = info->flags;
						unsigned int flags = spec;
						spec += 100;

						snprintf(field, sizeof(field), "%s@%d.%04X", getFormatExtension(i), q, flags);
						pixpath = leptDebugGenFilepath("%s-Qual-%03d.%04X-%03d.%s", source_fname, q, flags, i, getFormatExtension(i));
						lept_stderr("Writing to: %s     @ quality: %3d%% (special flags: 0x%04X)\n", pixpath, q, flags);
						nanotimer_start(&time);
						pixSetSpecial(pixf, spec);
						pixWrite(pixpath, pixf, i);
						collect(tsv_column_names, tsv_timing_values, tsv_fsize_values, field, nanotimer_get_elapsed_ms(&time), pixpath);

						q--;
					}
				}
				else
				{
					/*
					struct stat_data_elem {
						uint16_t filter_type;
						uint16_t strategy;
						uint16_t compression;
						uint16_t window;

						float filesize;   // as we want to compare these to discover the 'tightest' output for various input files, we need this to be the 'normalized' filesize.
						float time_spent;

						unsigned int flags;
					};

					// strategies:
					#define Z_FILTERED            1
					#define Z_HUFFMAN_ONLY        2
					#define Z_RLE                 3
					#define Z_FIXED               4
					#define Z_DEFAULT_STRATEGY    0

					*/

					static const unsigned int pngBespokeSpecials[20 + 1] = {
					  31131, // filter_type: 0xB8, strategy: 1, compression: 9, window: 15, filesize:ratio: 33.802606, time_spent: 33.802605, flags: 0x7937
					  31130, // filter_type: 0xB0, strategy: 1, compression: 9, window: 15, filesize:ratio: 33.844317, time_spent: 33.844318, flags: 0x7936
					  22934, // filter_type: 0x90, strategy: 1, compression: 9, window: 13, filesize:ratio: 35.037457, time_spent: 35.037457, flags: 0x5932
					  22419, // filter_type: 0x78, strategy: 1, compression: 7, window: 13, filesize:ratio: 36.293918, time_spent: 36.293919, flags: 0x572F
					  29575, // filter_type: 0x18, strategy: 1, compression: 3, window: 15, filesize:ratio: 37.615841, time_spent: 37.615841, flags: 0x7323
					  5525, // filter_type: 0x88, strategy: 1, compression: 5, window: 9, filesize:ratio: 39.003235, time_spent: 39.003235, flags: 0x1531
					  6026, // filter_type: 0x30, strategy: 1, compression: 7, window: 9, filesize:ratio: 40.438882, time_spent: 40.438881, flags: 0x1726
					  4753, // filter_type: 0x68, strategy: 1, compression: 2, window: 9, filesize:ratio: 41.920971, time_spent: 41.920971, flags: 0x122D
					  25595, // filter_type: 0xB8, strategy: 4, compression: 3, window: 14, filesize:ratio: 43.517432, time_spent: 43.517433, flags: 0x6397
					  18684, // filter_type: 0xC0, strategy: 4, compression: 8, window: 12, filesize:ratio: 45.159742, time_spent: 45.159740, flags: 0x4898
					  26348, // filter_type: 0x40, strategy: 4, compression: 6, window: 14, filesize:ratio: 46.864986, time_spent: 46.864986, flags: 0x6688
					  25064, // filter_type: 0x20, strategy: 4, compression: 1, window: 14, filesize:ratio: 48.672084, time_spent: 48.672085, flags: 0x6184
					  13799, // filter_type: 0x18, strategy: 4, compression: 5, window: 11, filesize:ratio: 50.588918, time_spent: 50.588917, flags: 0x3583
					  12792, // filter_type: 0xA0, strategy: 4, compression: 1, window: 11, filesize:ratio: 52.588924, time_spent: 52.588924, flags: 0x3194
					  5117, // filter_type: 0xC8, strategy: 4, compression: 3, window: 9, filesize:ratio: 54.679033, time_spent: 54.679031, flags: 0x1399
					  17840, // filter_type: 0x60, strategy: 2, compression: 5, window: 12, filesize:ratio: 56.958549, time_spent: 56.958549, flags: 0x454C
					  8678, // filter_type: 0x10, strategy: 4, compression: 1, window: 10, filesize:ratio: 59.277160, time_spent: 59.277161, flags: 0x2182
					  30405, // filter_type: 0x08, strategy: 3, compression: 6, window: 15, filesize:ratio: 61.972674, time_spent: 61.972675, flags: 0x7661
					  492, // filter_type: 0x40, strategy: 4, compression: 1, window: 8, filesize:ratio: 65.095407, time_spent: 65.095406, flags: 0x0188
					  18597, // filter_type: 0x08, strategy: 2, compression: 8, window: 12, filesize:ratio: 74.805086, time_spent: 74.805084, flags: 0x4841
					  0  // all defaults
					};

					for (int q = 100; q >= 0; ) {
						char field[40];
						unsigned int idx = q / 5;
						unsigned int spec = pngBespokeSpecials[idx];
						unsigned int flags = (spec ? spec - 100 : 0);

						snprintf(field, sizeof(field), "%s@%d.%04X", getFormatExtension(i), q, flags);
						pixpath = leptDebugGenFilepath("%s-Qual-%03d.%04X-%03d.%s", source_fname, q, flags, i, getFormatExtension(i));
						lept_stderr("Writing to: %s     @ quality: %3d%% (special flags: 0x%04X)\n", pixpath, q, flags);
						nanotimer_start(&time);
						pixSetSpecial(pixf, spec);
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
