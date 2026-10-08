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

#pragma once

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Speech to text (Whisper through whisper.cpp) on 16 kHz mono audio. With
 * translate set, the same inference writes the text in English instead of the
 * spoken language. A separate translation stage can replace that later: the
 * caller only sees audio in and text out. Not thread-safe; a run can take
 * seconds, so never call it from a real-time or rendering thread. Independent
 * of OBS so it can be tested on its own. */
#define TRANSCRIBER_SAMPLE_RATE 16000
#define TRANSCRIBER_FIT_AUDIO_CONTEXT -1

struct transcriber;

struct transcriber_params {
	const char *model_path;
	/* Spoken language as a Whisper code such as "fr". */
	const char *language;
	bool translate;
	int threads;
	/* Encoder context size, in 20 ms frames. 0 keeps the model default
	 * (30 s of audio); TRANSCRIBER_FIT_AUDIO_CONTEXT sizes it to each
	 * recording, which is much faster on short audio. */
	int audio_context;
};

struct transcriber *transcriber_create(const struct transcriber_params *params);
void transcriber_destroy(struct transcriber *transcriber);
/* Returns the recognized text as UTF-8 (possibly empty), or NULL on failure.
 * Release it with transcriber_free_text. */
char *transcriber_run(struct transcriber *transcriber, const float *samples, size_t count);
void transcriber_free_text(char *text);

#ifdef __cplusplus
}
#endif
