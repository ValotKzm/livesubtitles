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
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Turns per-frame speech probabilities into stable speech/silence states.
 * Speech starts once the probability stays at or above start_threshold for
 * min_speech_ms, and ends once it stays below end_threshold for
 * min_silence_ms, so short spikes and short pauses do not flip the state.
 * Independent of OBS and of the model producing the probabilities. */
struct speech_detector_params {
	float start_threshold;
	float end_threshold;
	uint32_t min_speech_ms;
	uint32_t min_silence_ms;
};

struct speech_detector {
	struct speech_detector_params params;
	bool speaking;
	uint32_t pending_ms;
};

enum speech_event {
	SPEECH_EVENT_NONE,
	SPEECH_EVENT_START,
	SPEECH_EVENT_END,
};

struct speech_detector_params speech_detector_default_params(void);
void speech_detector_init(struct speech_detector *detector, const struct speech_detector_params *params);
void speech_detector_reset(struct speech_detector *detector);
bool speech_detector_is_speaking(const struct speech_detector *detector);
enum speech_event speech_detector_update(struct speech_detector *detector, float probability, uint32_t frame_ms);

#ifdef __cplusplus
}
#endif
