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

/* Fixed-capacity ring of audio samples. When full, new samples overwrite the
 * oldest ones. Not thread-safe: the owner serializes access. Independent of
 * OBS so it can be unit tested on its own. */
struct audio_ring {
	float *data;
	size_t capacity;
	size_t start;
	size_t size;
};

bool audio_ring_init(struct audio_ring *ring, size_t capacity);
void audio_ring_free(struct audio_ring *ring);
void audio_ring_reset(struct audio_ring *ring);
size_t audio_ring_size(const struct audio_ring *ring);
void audio_ring_write(struct audio_ring *ring, const float *samples, size_t count);
/* Removes up to count of the oldest samples; returns how many were copied. */
size_t audio_ring_read(struct audio_ring *ring, float *out, size_t count);

#ifdef __cplusplus
}
#endif
