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

/* Runs the real model on reference audio: transcriber-test <model> <wav>.
 * The wav file is the English recording shipped with whisper.cpp ("ask not
 * what your country can do for you..."), 16 kHz mono 16-bit PCM. */

#include "transcriber.h"

#include "check.h"
#include "wav.h"

#include <ctype.h>

static bool contains_ignoring_case(const char *text, const char *needle)
{
	size_t length = strlen(needle);
	for (; *text; text++) {
		size_t i = 0;
		while (i < length && text[i] && tolower((unsigned char)text[i]) == needle[i])
			i++;
		if (i == length)
			return true;
	}
	return false;
}

static void check_recognizes(const struct transcriber_params *params, const float *samples, size_t count)
{
	struct transcriber *transcriber = transcriber_create(params);
	CHECK(transcriber != NULL);
	if (!transcriber)
		return;

	char *text = transcriber_run(transcriber, samples, count);
	CHECK(text != NULL);
	if (text) {
		CHECK(text[0] != ' ');
		CHECK(contains_ignoring_case(text, "ask not what your country can do for you"));
	}
	transcriber_free_text(text);

	/* Audio far shorter than the model accepts is padded, not refused. */
	float *silence = calloc(TRANSCRIBER_SAMPLE_RATE / 5, sizeof(float));
	char *quiet = transcriber_run(transcriber, silence, TRANSCRIBER_SAMPLE_RATE / 5);
	CHECK(quiet != NULL);
	if (quiet)
		CHECK(!contains_ignoring_case(quiet, "country"));
	transcriber_free_text(quiet);
	free(silence);

	transcriber_destroy(transcriber);
}

int main(int argc, char **argv)
{
	if (argc != 3) {
		printf("usage: transcriber-test <model> <wav>\n");
		return 2;
	}

	struct transcriber_params params = {
		.model_path = "this-model-does-not-exist.bin",
		.language = "en",
		.translate = false,
		.threads = 2,
		.audio_context = 0,
	};
	CHECK(transcriber_create(&params) == NULL);

	size_t count = 0;
	float *samples = load_wav(argv[2], TRANSCRIBER_SAMPLE_RATE, &count);
	CHECK(samples != NULL);
	if (!samples)
		return check_result("transcriber");

	params.model_path = argv[1];
	check_recognizes(&params, samples, count);

	/* The faster, fitted context gives the same sentence on this recording. */
	params.audio_context = TRANSCRIBER_FIT_AUDIO_CONTEXT;
	check_recognizes(&params, samples, count);

	free(samples);
	return check_result("transcriber");
}
