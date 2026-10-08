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

/* Runs the real model on reference audio: voice-activity-test <model> <wav>.
 * The wav file is 16 kHz mono 16-bit PCM speech. */

#include "speech-detector.h"
#include "voice-activity.h"

#include "check.h"
#include "wav.h"

#include <time.h>

struct run_result {
	int windows;
	int speech_windows;
	int starts;
	int ends;
	bool speaking_at_end;
};

static struct run_result run(struct voice_activity *vad, const float *samples, size_t count)
{
	struct run_result result = {0};
	struct speech_detector detector;
	struct speech_detector_params params = speech_detector_default_params();
	speech_detector_init(&detector, &params);

	for (size_t offset = 0; offset + VOICE_ACTIVITY_WINDOW_SAMPLES <= count;
	     offset += VOICE_ACTIVITY_WINDOW_SAMPLES) {
		float probability = 0.0f;
		if (!voice_activity_process(vad, samples + offset, &probability)) {
			CHECK(!"voice_activity_process failed");
			break;
		}
		/* Also fails on NaN, which compares false with everything. */
		CHECK(probability >= 0.0f && probability <= 1.0f);

		enum speech_event event = speech_detector_update(&detector, probability, VOICE_ACTIVITY_WINDOW_MS);
		result.starts += event == SPEECH_EVENT_START;
		result.ends += event == SPEECH_EVENT_END;
		result.speech_windows += speech_detector_is_speaking(&detector);
		result.windows++;
	}

	result.speaking_at_end = speech_detector_is_speaking(&detector);
	return result;
}

/* Leaves freed heap blocks full of NaN bit patterns, as in a long-running host
 * process, so that state the library forgets to initialize shows up here. */
static void dirty_heap(void)
{
	enum { BLOCKS = 4096 };
	static void *blocks[BLOCKS];

	for (int i = 0; i < BLOCKS; i++) {
		size_t size = (size_t)64 << (i % 12);
		blocks[i] = malloc(size);
		if (blocks[i])
			memset(blocks[i], 0xFF, size);
	}
	for (int i = 0; i < BLOCKS; i++)
		free(blocks[i]);
}

static void log_library_message(bool is_error, const char *message)
{
	printf("[%s] %s", is_error ? "error" : "warning", message);
}

int main(int argc, char **argv)
{
	if (argc != 3) {
		printf("usage: voice-activity-test <model> <wav>\n");
		return 2;
	}

	voice_activity_set_log(log_library_message);

	CHECK(voice_activity_create("this-model-does-not-exist.bin") == NULL);

	dirty_heap();
	struct voice_activity *vad = voice_activity_create(argv[1]);
	CHECK(vad != NULL);
	if (!vad)
		return check_result("voice activity");

	size_t silence_count = VOICE_ACTIVITY_SAMPLE_RATE * 2;
	float *silence = calloc(silence_count, sizeof(float));

	/* Reference speech followed by two seconds of silence: speech is
	 * detected for a good part of the recording and ends in the silence. */
	size_t speech_count = 0;
	float *speech = load_wav(argv[2], VOICE_ACTIVITY_SAMPLE_RATE, &speech_count);
	CHECK(speech != NULL);
	if (speech) {
		size_t total = speech_count + silence_count;
		float *padded = calloc(total, sizeof(float));
		memcpy(padded, speech, speech_count * sizeof(float));

		/* First use of a new instance, as the plugin does: it must work
		 * without an explicit reset. */
		clock_t begin = clock();
		struct run_result spoken = run(vad, padded, total);
		double elapsed_ms = (double)(clock() - begin) * 1000.0 / CLOCKS_PER_SEC;

		printf("speech run: %d windows, %d in speech, %d start(s), %d end(s), %.3f ms per %d ms window\n",
		       spoken.windows, spoken.speech_windows, spoken.starts, spoken.ends,
		       spoken.windows ? elapsed_ms / spoken.windows : 0.0, VOICE_ACTIVITY_WINDOW_MS);

		CHECK(spoken.starts >= 1);
		CHECK(spoken.ends == spoken.starts);
		CHECK(!spoken.speaking_at_end);
		CHECK(spoken.speech_windows * 4 > spoken.windows);
		CHECK(elapsed_ms / spoken.windows < VOICE_ACTIVITY_WINDOW_MS);

		/* After a reset the same audio gives the same result. */
		voice_activity_reset(vad);
		struct run_result again = run(vad, padded, total);
		CHECK(again.starts == spoken.starts);
		CHECK(again.speech_windows == spoken.speech_windows);

		free(padded);
		free(speech);
	}

	/* Two seconds of digital silence never start speech. */
	voice_activity_reset(vad);
	struct run_result quiet = run(vad, silence, silence_count);
	CHECK(quiet.windows > 0);
	CHECK(quiet.starts == 0);
	CHECK(quiet.speech_windows == 0);

	free(silence);
	voice_activity_destroy(vad);
	return check_result("voice activity");
}
