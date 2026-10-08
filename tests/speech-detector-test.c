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

#include "check.h"

#define FRAME_MS 32

static const struct speech_detector_params test_params = {
	.start_threshold = 0.5f,
	.end_threshold = 0.35f,
	.min_speech_ms = 64,
	.min_silence_ms = 96,
};

static void test_silence_never_starts(void)
{
	struct speech_detector detector;
	speech_detector_init(&detector, &test_params);

	for (int i = 0; i < 100; i++)
		CHECK(speech_detector_update(&detector, 0.05f, FRAME_MS) == SPEECH_EVENT_NONE);
	CHECK(!speech_detector_is_speaking(&detector));
}

static void test_speech_starts_after_min_duration(void)
{
	struct speech_detector detector;
	speech_detector_init(&detector, &test_params);

	CHECK(speech_detector_update(&detector, 0.9f, FRAME_MS) == SPEECH_EVENT_NONE);
	CHECK(!speech_detector_is_speaking(&detector));
	CHECK(speech_detector_update(&detector, 0.9f, FRAME_MS) == SPEECH_EVENT_START);
	CHECK(speech_detector_is_speaking(&detector));
	CHECK(speech_detector_update(&detector, 0.9f, FRAME_MS) == SPEECH_EVENT_NONE);
}

static void test_short_spike_is_ignored(void)
{
	struct speech_detector detector;
	speech_detector_init(&detector, &test_params);

	CHECK(speech_detector_update(&detector, 0.9f, FRAME_MS) == SPEECH_EVENT_NONE);
	CHECK(speech_detector_update(&detector, 0.1f, FRAME_MS) == SPEECH_EVENT_NONE);
	/* The spike was forgotten: one loud frame is again not enough. */
	CHECK(speech_detector_update(&detector, 0.9f, FRAME_MS) == SPEECH_EVENT_NONE);
	CHECK(!speech_detector_is_speaking(&detector));
}

static void start_speaking(struct speech_detector *detector)
{
	speech_detector_init(detector, &test_params);
	speech_detector_update(detector, 0.9f, FRAME_MS);
	speech_detector_update(detector, 0.9f, FRAME_MS);
}

static void test_speech_ends_after_min_silence(void)
{
	struct speech_detector detector;
	start_speaking(&detector);

	CHECK(speech_detector_update(&detector, 0.1f, FRAME_MS) == SPEECH_EVENT_NONE);
	CHECK(speech_detector_update(&detector, 0.1f, FRAME_MS) == SPEECH_EVENT_NONE);
	CHECK(speech_detector_is_speaking(&detector));
	CHECK(speech_detector_update(&detector, 0.1f, FRAME_MS) == SPEECH_EVENT_END);
	CHECK(!speech_detector_is_speaking(&detector));
}

static void test_short_pause_keeps_speaking(void)
{
	struct speech_detector detector;
	start_speaking(&detector);

	CHECK(speech_detector_update(&detector, 0.1f, FRAME_MS) == SPEECH_EVENT_NONE);
	CHECK(speech_detector_update(&detector, 0.1f, FRAME_MS) == SPEECH_EVENT_NONE);
	CHECK(speech_detector_update(&detector, 0.9f, FRAME_MS) == SPEECH_EVENT_NONE);
	/* The pause was forgotten: two quiet frames are again not enough. */
	CHECK(speech_detector_update(&detector, 0.1f, FRAME_MS) == SPEECH_EVENT_NONE);
	CHECK(speech_detector_update(&detector, 0.1f, FRAME_MS) == SPEECH_EVENT_NONE);
	CHECK(speech_detector_is_speaking(&detector));
}

static void test_hysteresis_between_thresholds(void)
{
	struct speech_detector detector;
	speech_detector_init(&detector, &test_params);

	/* 0.4 is below the start threshold: it does not start speech... */
	for (int i = 0; i < 10; i++)
		CHECK(speech_detector_update(&detector, 0.4f, FRAME_MS) == SPEECH_EVENT_NONE);
	CHECK(!speech_detector_is_speaking(&detector));

	/* ...but it is above the end threshold: it does not end speech. */
	start_speaking(&detector);
	for (int i = 0; i < 10; i++)
		CHECK(speech_detector_update(&detector, 0.4f, FRAME_MS) == SPEECH_EVENT_NONE);
	CHECK(speech_detector_is_speaking(&detector));
}

static void test_repeated_transitions(void)
{
	struct speech_detector detector;
	speech_detector_init(&detector, &test_params);

	int starts = 0, ends = 0;
	for (int cycle = 0; cycle < 3; cycle++) {
		for (int i = 0; i < 10; i++)
			starts += speech_detector_update(&detector, 0.9f, FRAME_MS) == SPEECH_EVENT_START;
		for (int i = 0; i < 10; i++)
			ends += speech_detector_update(&detector, 0.1f, FRAME_MS) == SPEECH_EVENT_END;
	}
	CHECK(starts == 3);
	CHECK(ends == 3);
}

static void test_reset(void)
{
	struct speech_detector detector;
	start_speaking(&detector);

	speech_detector_reset(&detector);
	CHECK(!speech_detector_is_speaking(&detector));
	CHECK(speech_detector_update(&detector, 0.9f, FRAME_MS) == SPEECH_EVENT_NONE);
}

int main(void)
{
	test_silence_never_starts();
	test_speech_starts_after_min_duration();
	test_short_spike_is_ignored();
	test_speech_ends_after_min_silence();
	test_short_pause_keeps_speaking();
	test_hysteresis_between_thresholds();
	test_repeated_transitions();
	test_reset();
	return check_result("speech detector");
}
