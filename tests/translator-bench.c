/*
LiveSubtitles
Copyright (C) 2026 LiveSubtitles contributors

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

/* Measures one translation model, with the plugin's own build settings:
 * translator-bench <model directory> <text file> <threads>
 * The text file holds one UTF-8 sentence per line. Prints the translation and
 * the time of each sentence, three times over; the first pass includes
 * warm-up. */

#include "translator.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define RUNS 3
#define MAX_LINE 4096

static double elapsed_ms(clock_t begin)
{
	return (double)(clock() - begin) * 1000.0 / CLOCKS_PER_SEC;
}

int main(int argc, char **argv)
{
	if (argc != 4) {
		printf("usage: translator-bench <model directory> <text file> <threads>\n");
		return 2;
	}

	struct translator_params params = {
		.model_dir = argv[1],
		.threads = atoi(argv[3]),
	};

	clock_t begin = clock();
	struct translator *translator = translator_create(&params);
	if (!translator) {
		printf("cannot load model '%s'\n", argv[1]);
		return 1;
	}
	printf("load_ms=%.0f\n", elapsed_ms(begin));

	for (int i = 0; i < RUNS; i++) {
		FILE *file = fopen(argv[2], "rb");
		if (!file) {
			printf("cannot read '%s'\n", argv[2]);
			translator_destroy(translator);
			return 1;
		}

		char line[MAX_LINE];
		while (fgets(line, sizeof(line), file)) {
			line[strcspn(line, "\r\n")] = '\0';
			if (!line[0])
				continue;
			begin = clock();
			char *text = translator_run(translator, line);
			printf("run_ms=%.0f text=%s\n", elapsed_ms(begin), text ? text : "<failed>");
			translator_free_text(text);
		}
		fclose(file);
		printf("--\n");
	}

	translator_destroy(translator);
	return 0;
}
