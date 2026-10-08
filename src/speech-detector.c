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

#include "speech-detector.h"

struct speech_detector_params speech_detector_default_params(void)
{
	struct speech_detector_params params = {
		.start_threshold = 0.5f,
		.end_threshold = 0.35f,
		.min_speech_ms = 64,
		.min_silence_ms = 500,
	};
	return params;
}

void speech_detector_init(struct speech_detector *detector, const struct speech_detector_params *params)
{
	detector->params = *params;
	speech_detector_reset(detector);
}

void speech_detector_reset(struct speech_detector *detector)
{
	detector->speaking = false;
	detector->pending_ms = 0;
}

bool speech_detector_is_speaking(const struct speech_detector *detector)
{
	return detector->speaking;
}

enum speech_event speech_detector_update(struct speech_detector *detector, float probability, uint32_t frame_ms)
{
	const struct speech_detector_params *params = &detector->params;

	if (!detector->speaking) {
		if (probability < params->start_threshold) {
			detector->pending_ms = 0;
			return SPEECH_EVENT_NONE;
		}

		detector->pending_ms += frame_ms;
		if (detector->pending_ms < params->min_speech_ms)
			return SPEECH_EVENT_NONE;

		detector->speaking = true;
		detector->pending_ms = 0;
		return SPEECH_EVENT_START;
	}

	if (probability >= params->end_threshold) {
		detector->pending_ms = 0;
		return SPEECH_EVENT_NONE;
	}

	detector->pending_ms += frame_ms;
	if (detector->pending_ms < params->min_silence_ms)
		return SPEECH_EVENT_NONE;

	detector->speaking = false;
	detector->pending_ms = 0;
	return SPEECH_EVENT_END;
}
