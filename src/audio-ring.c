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

#include <stdlib.h>
#include <string.h>

bool audio_ring_init(struct audio_ring *ring, size_t capacity)
{
	memset(ring, 0, sizeof(*ring));
	if (!capacity)
		return false;

	ring->data = malloc(capacity * sizeof(float));
	if (!ring->data)
		return false;

	ring->capacity = capacity;
	return true;
}

void audio_ring_free(struct audio_ring *ring)
{
	free(ring->data);
	memset(ring, 0, sizeof(*ring));
}

void audio_ring_reset(struct audio_ring *ring)
{
	ring->start = 0;
	ring->size = 0;
}

size_t audio_ring_size(const struct audio_ring *ring)
{
	return ring->size;
}

void audio_ring_write(struct audio_ring *ring, const float *samples, size_t count)
{
	if (!ring->capacity || !count)
		return;

	/* Only the newest samples can survive a write larger than the ring. */
	if (count >= ring->capacity) {
		memcpy(ring->data, samples + (count - ring->capacity), ring->capacity * sizeof(float));
		ring->start = 0;
		ring->size = ring->capacity;
		return;
	}

	size_t end = (ring->start + ring->size) % ring->capacity;
	size_t first = ring->capacity - end;
	if (first > count)
		first = count;

	memcpy(ring->data + end, samples, first * sizeof(float));
	memcpy(ring->data, samples + first, (count - first) * sizeof(float));

	size_t free_space = ring->capacity - ring->size;
	if (count > free_space) {
		ring->start = (ring->start + (count - free_space)) % ring->capacity;
		ring->size = ring->capacity;
	} else {
		ring->size += count;
	}
}

size_t audio_ring_read(struct audio_ring *ring, float *out, size_t count)
{
	if (count > ring->size)
		count = ring->size;
	if (!count)
		return 0;

	size_t first = ring->capacity - ring->start;
	if (first > count)
		first = count;

	memcpy(out, ring->data + ring->start, first * sizeof(float));
	memcpy(out + first, ring->data, (count - first) * sizeof(float));

	ring->start = (ring->start + count) % ring->capacity;
	ring->size -= count;
	return count;
}
