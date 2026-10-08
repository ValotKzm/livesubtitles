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

#include "transcriber.h"

#include <stdlib.h>
#include <string.h>

#include <whisper.h>

/* whisper.cpp refuses audio shorter than one second; pad a little past it. */
#define MIN_SAMPLES (TRANSCRIBER_SAMPLE_RATE * 5 / 4)

/* The encoder sees 50 frames per second of audio, 1500 at most. A context
 * that fits the audio too tightly makes the decoder repeat or invent text and
 * retry for seconds, hence the generous margin and floor: see the
 * measurements in docs/development.md before lowering them. */
#define CONTEXT_FRAMES_PER_SECOND 50
#define CONTEXT_MARGIN_FRAMES 256
#define CONTEXT_MIN_FRAMES 512
#define CONTEXT_MAX_FRAMES 1500

struct transcriber {
	struct whisper_context *context;
	char *language;
	bool translate;
	int threads;
	int audio_context;
};

static char *copy_string(const char *text)
{
	size_t size = strlen(text) + 1;
	char *copy = malloc(size);
	if (copy)
		memcpy(copy, text, size);
	return copy;
}

static int fitted_audio_context(size_t count)
{
	size_t frames = count * CONTEXT_FRAMES_PER_SECOND / TRANSCRIBER_SAMPLE_RATE + CONTEXT_MARGIN_FRAMES;
	if (frames < CONTEXT_MIN_FRAMES)
		return CONTEXT_MIN_FRAMES;
	/* 0 selects the full default context. */
	return frames >= CONTEXT_MAX_FRAMES ? 0 : (int)frames;
}

struct transcriber *transcriber_create(const struct transcriber_params *params)
{
	if (!params->model_path || !params->language)
		return NULL;

	struct transcriber *transcriber = calloc(1, sizeof(*transcriber));
	if (!transcriber)
		return NULL;

	transcriber->language = copy_string(params->language);
	if (!transcriber->language) {
		free(transcriber);
		return NULL;
	}
	transcriber->translate = params->translate;
	transcriber->threads = params->threads > 0 ? params->threads : 1;
	transcriber->audio_context = params->audio_context;

	struct whisper_context_params context_params = whisper_context_default_params();
	context_params.use_gpu = false;

	transcriber->context = whisper_init_from_file_with_params(params->model_path, context_params);
	if (!transcriber->context) {
		free(transcriber->language);
		free(transcriber);
		return NULL;
	}
	return transcriber;
}

void transcriber_destroy(struct transcriber *transcriber)
{
	if (!transcriber)
		return;

	whisper_free(transcriber->context);
	free(transcriber->language);
	free(transcriber);
}

char *transcriber_run(struct transcriber *transcriber, const float *samples, size_t count)
{
	float *padded = NULL;
	if (count < MIN_SAMPLES) {
		padded = calloc(MIN_SAMPLES, sizeof(float));
		if (!padded)
			return NULL;
		memcpy(padded, samples, count * sizeof(float));
		samples = padded;
		count = MIN_SAMPLES;
	}

	struct whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
	params.n_threads = transcriber->threads;
	params.language = transcriber->language;
	params.translate = transcriber->translate;
	params.audio_ctx = transcriber->audio_context == TRANSCRIBER_FIT_AUDIO_CONTEXT
				   ? fitted_audio_context(count)
				   : transcriber->audio_context;
	/* Each call is an independent utterance and only its text is used. */
	params.no_context = true;
	params.no_timestamps = true;
	params.suppress_nst = true;
	params.print_special = false;
	params.print_progress = false;
	params.print_realtime = false;
	params.print_timestamps = false;

	int result = whisper_full(transcriber->context, params, samples, (int)count);
	free(padded);
	if (result != 0)
		return NULL;

	int segments = whisper_full_n_segments(transcriber->context);
	size_t size = 1;
	for (int i = 0; i < segments; i++)
		size += strlen(whisper_full_get_segment_text(transcriber->context, i));

	char *text = malloc(size);
	if (!text)
		return NULL;

	char *end = text;
	for (int i = 0; i < segments; i++) {
		const char *segment = whisper_full_get_segment_text(transcriber->context, i);
		size_t length = strlen(segment);
		memcpy(end, segment, length);
		end += length;
	}
	*end = '\0';

	/* Segments start with a space; drop the one leading the whole text. */
	char *start = text;
	while (*start == ' ')
		start++;
	memmove(text, start, strlen(start) + 1);
	return text;
}

void transcriber_free_text(char *text)
{
	free(text);
}
