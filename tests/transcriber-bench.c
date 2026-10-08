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

/* Measures one model on one recording, with the plugin's own build settings:
 * transcriber-bench <model> <wav> <language> <translate 0|1> <threads> [audio context]
 * Audio context: 0 for the model default, -1 to fit it to the recording.
 * Prints the text and the time of each run; the first run includes warm-up. */

#include "transcriber.h"

#include "wav.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define RUNS 3

static double elapsed_ms(clock_t begin)
{
	return (double)(clock() - begin) * 1000.0 / CLOCKS_PER_SEC;
}

int main(int argc, char **argv)
{
	if (argc < 6) {
		printf("usage: transcriber-bench <model> <wav> <language> <translate 0|1> <threads> [audio context]\n");
		return 2;
	}

	size_t count = 0;
	float *samples = load_wav(argv[2], TRANSCRIBER_SAMPLE_RATE, &count);
	if (!samples) {
		printf("cannot read '%s' as 16 kHz mono 16-bit PCM\n", argv[2]);
		return 1;
	}

	struct transcriber_params params = {
		.model_path = argv[1],
		.language = argv[3],
		.translate = atoi(argv[4]) != 0,
		.threads = atoi(argv[5]),
		.audio_context = argc > 6 ? atoi(argv[6]) : 0,
	};

	clock_t begin = clock();
	struct transcriber *transcriber = transcriber_create(&params);
	if (!transcriber) {
		printf("cannot load model '%s'\n", argv[1]);
		free(samples);
		return 1;
	}
	printf("load_ms=%.0f audio_s=%.1f\n", elapsed_ms(begin), (double)count / TRANSCRIBER_SAMPLE_RATE);

	for (int i = 0; i < RUNS; i++) {
		begin = clock();
		char *text = transcriber_run(transcriber, samples, count);
		printf("run_ms=%.0f text=%s\n", elapsed_ms(begin), text ? text : "<failed>");
		transcriber_free_text(text);
	}

	transcriber_destroy(transcriber);
	free(samples);
	return 0;
}
