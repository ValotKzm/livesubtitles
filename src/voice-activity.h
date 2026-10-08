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

#ifdef __cplusplus
extern "C" {
#endif

/* Speech probability model (Silero VAD through whisper.cpp). Works on
 * consecutive windows of 16 kHz mono audio and keeps state between them, so
 * one instance follows one continuous stream. Not thread-safe. Independent
 * of OBS so it can be tested on its own. */
#define VOICE_ACTIVITY_SAMPLE_RATE 16000
#define VOICE_ACTIVITY_WINDOW_SAMPLES 512
#define VOICE_ACTIVITY_WINDOW_MS (VOICE_ACTIVITY_WINDOW_SAMPLES * 1000 / VOICE_ACTIVITY_SAMPLE_RATE)

struct voice_activity;

/* Sends warnings and errors of the underlying library to the callback and
 * drops everything else. Call once before creating any instance. */
typedef void (*voice_activity_log_t)(bool is_error, const char *message);
void voice_activity_set_log(voice_activity_log_t callback);

struct voice_activity *voice_activity_create(const char *model_path);
void voice_activity_destroy(struct voice_activity *vad);
void voice_activity_reset(struct voice_activity *vad);
/* window holds VOICE_ACTIVITY_WINDOW_SAMPLES samples. */
bool voice_activity_process(struct voice_activity *vad, const float *window, float *probability);

#ifdef __cplusplus
}
#endif
