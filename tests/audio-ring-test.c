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

#include "audio-ring.h"

#include <stdio.h>

static int failures;

/* assert() is compiled out in release configurations, so check by hand. */
#define CHECK(cond) \
	do { \
		if (!(cond)) { \
			printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
			failures++; \
		} \
	} while (0)

static void fill(float *samples, size_t count, float first)
{
	for (size_t i = 0; i < count; i++)
		samples[i] = first + (float)i;
}

static void test_init(void)
{
	struct audio_ring ring;
	CHECK(!audio_ring_init(&ring, 0));
	CHECK(audio_ring_init(&ring, 4));
	CHECK(audio_ring_size(&ring) == 0);

	float out[4];
	CHECK(audio_ring_read(&ring, out, 4) == 0);
	audio_ring_free(&ring);
}

static void test_write_then_read_in_order(void)
{
	struct audio_ring ring;
	float in[3], out[4];
	audio_ring_init(&ring, 4);

	fill(in, 3, 1.0f);
	audio_ring_write(&ring, in, 3);
	CHECK(audio_ring_size(&ring) == 3);

	CHECK(audio_ring_read(&ring, out, 2) == 2);
	CHECK(out[0] == 1.0f && out[1] == 2.0f);
	CHECK(audio_ring_size(&ring) == 1);

	/* Asking for more than is stored returns only what is there. */
	CHECK(audio_ring_read(&ring, out, 4) == 1);
	CHECK(out[0] == 3.0f);
	CHECK(audio_ring_size(&ring) == 0);
	audio_ring_free(&ring);
}

static void test_wraparound(void)
{
	struct audio_ring ring;
	float in[3], out[4];
	audio_ring_init(&ring, 4);

	fill(in, 3, 1.0f);
	audio_ring_write(&ring, in, 3);
	audio_ring_read(&ring, out, 2);

	/* Stored: 3. Writing 10,11,12 wraps past the end of the storage. */
	fill(in, 3, 10.0f);
	audio_ring_write(&ring, in, 3);
	CHECK(audio_ring_size(&ring) == 4);
	CHECK(audio_ring_read(&ring, out, 4) == 4);
	CHECK(out[0] == 3.0f && out[1] == 10.0f && out[2] == 11.0f && out[3] == 12.0f);
	audio_ring_free(&ring);
}

static void test_overwrites_oldest_when_full(void)
{
	struct audio_ring ring;
	float in[6], out[4];
	audio_ring_init(&ring, 4);

	fill(in, 3, 1.0f);
	audio_ring_write(&ring, in, 3);
	fill(in, 3, 4.0f);
	audio_ring_write(&ring, in, 3);

	/* 1..6 written into 4 slots: 1 and 2 are gone. */
	CHECK(audio_ring_size(&ring) == 4);
	CHECK(audio_ring_read(&ring, out, 4) == 4);
	CHECK(out[0] == 3.0f && out[1] == 4.0f && out[2] == 5.0f && out[3] == 6.0f);
	audio_ring_free(&ring);
}

static void test_write_larger_than_capacity(void)
{
	struct audio_ring ring;
	float in[6], out[4];
	audio_ring_init(&ring, 4);

	fill(in, 1, 99.0f);
	audio_ring_write(&ring, in, 1);
	fill(in, 6, 1.0f);
	audio_ring_write(&ring, in, 6);

	CHECK(audio_ring_size(&ring) == 4);
	CHECK(audio_ring_read(&ring, out, 4) == 4);
	CHECK(out[0] == 3.0f && out[1] == 4.0f && out[2] == 5.0f && out[3] == 6.0f);
	audio_ring_free(&ring);
}

static void test_size_stays_bounded(void)
{
	struct audio_ring ring;
	float in[7];
	audio_ring_init(&ring, 16);

	fill(in, 7, 0.0f);
	for (int i = 0; i < 1000; i++) {
		audio_ring_write(&ring, in, 7);
		CHECK(audio_ring_size(&ring) <= 16);
	}
	CHECK(audio_ring_size(&ring) == 16);
	audio_ring_free(&ring);
}

static void test_reset(void)
{
	struct audio_ring ring;
	float in[3], out[4];
	audio_ring_init(&ring, 4);

	fill(in, 3, 1.0f);
	audio_ring_write(&ring, in, 3);
	audio_ring_reset(&ring);
	CHECK(audio_ring_size(&ring) == 0);
	CHECK(audio_ring_read(&ring, out, 4) == 0);

	fill(in, 2, 7.0f);
	audio_ring_write(&ring, in, 2);
	CHECK(audio_ring_read(&ring, out, 4) == 2);
	CHECK(out[0] == 7.0f && out[1] == 8.0f);
	audio_ring_free(&ring);
}

int main(void)
{
	test_init();
	test_write_then_read_in_order();
	test_wraparound();
	test_overwrites_oldest_when_full();
	test_write_larger_than_capacity();
	test_size_stays_bounded();
	test_reset();

	if (failures) {
		printf("%d check(s) failed\n", failures);
		return 1;
	}
	printf("all audio ring checks passed\n");
	return 0;
}
