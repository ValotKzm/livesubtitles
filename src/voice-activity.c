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

#include "voice-activity.h"

#include <stdlib.h>

#include <whisper.h>

struct voice_activity {
	struct whisper_vad_context *context;
};

static voice_activity_log_t log_callback;

static void forward_log(enum ggml_log_level level, const char *text, void *user_data)
{
	(void)user_data;

	if (!log_callback || !text)
		return;
	if (level == GGML_LOG_LEVEL_WARN || level == GGML_LOG_LEVEL_ERROR)
		log_callback(level == GGML_LOG_LEVEL_ERROR, text);
}

void voice_activity_set_log(voice_activity_log_t callback)
{
	log_callback = callback;
	whisper_log_set(forward_log, NULL);
}

struct voice_activity *voice_activity_create(const char *model_path)
{
	if (!model_path)
		return NULL;

	struct voice_activity *vad = calloc(1, sizeof(*vad));
	if (!vad)
		return NULL;

	/* One window costs well under its own duration on one CPU thread. */
	struct whisper_vad_context_params params = whisper_vad_default_context_params();
	params.n_threads = 1;
	params.use_gpu = false;

	vad->context = whisper_vad_init_from_file_with_params(model_path, params);
	if (!vad->context) {
		free(vad);
		return NULL;
	}
	return vad;
}

void voice_activity_destroy(struct voice_activity *vad)
{
	if (!vad)
		return;

	whisper_vad_free(vad->context);
	free(vad);
}

void voice_activity_reset(struct voice_activity *vad)
{
	whisper_vad_reset_state(vad->context);
}

bool voice_activity_process(struct voice_activity *vad, const float *window, float *probability)
{
	if (!whisper_vad_detect_speech_no_reset(vad->context, window, VOICE_ACTIVITY_WINDOW_SAMPLES))
		return false;
	if (whisper_vad_n_probs(vad->context) < 1)
		return false;

	*probability = whisper_vad_probs(vad->context)[0];
	return true;
}
