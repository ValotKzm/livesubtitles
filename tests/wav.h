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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline uint32_t wav_read_u32(const uint8_t *bytes)
{
	return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 | (uint32_t)bytes[2] << 16 | (uint32_t)bytes[3] << 24;
}

/* Reads a mono 16-bit PCM wav file at the given sample rate. Returns malloc'd
 * samples in [-1, 1], or NULL if the file is not in that format. */
static inline float *load_wav(const char *path, uint32_t sample_rate, size_t *count)
{
	FILE *file = fopen(path, "rb");
	if (!file)
		return NULL;

	fseek(file, 0, SEEK_END);
	long size = ftell(file);
	fseek(file, 0, SEEK_SET);

	uint8_t *bytes = size > 12 ? malloc((size_t)size) : NULL;
	if (!bytes || fread(bytes, 1, (size_t)size, file) != (size_t)size) {
		free(bytes);
		fclose(file);
		return NULL;
	}
	fclose(file);

	float *samples = NULL;
	bool format_ok = false;
	if (memcmp(bytes, "RIFF", 4) == 0 && memcmp(bytes + 8, "WAVE", 4) == 0) {
		size_t offset = 12;
		while (offset + 8 <= (size_t)size) {
			const uint8_t *chunk = bytes + offset;
			size_t chunk_size = wav_read_u32(chunk + 4);
			if (offset + 8 + chunk_size > (size_t)size)
				break;

			if (memcmp(chunk, "fmt ", 4) == 0 && chunk_size >= 16) {
				uint16_t format = (uint16_t)(chunk[8] | chunk[9] << 8);
				uint16_t channels = (uint16_t)(chunk[10] | chunk[11] << 8);
				uint16_t bits = (uint16_t)(chunk[22] | chunk[23] << 8);
				format_ok = format == 1 && channels == 1 && bits == 16 &&
					    wav_read_u32(chunk + 12) == sample_rate;
			} else if (memcmp(chunk, "data", 4) == 0 && format_ok) {
				*count = chunk_size / 2;
				samples = malloc(*count * sizeof(float));
				for (size_t i = 0; samples && i < *count; i++) {
					int16_t value = (int16_t)(chunk[8 + i * 2] | chunk[9 + i * 2] << 8);
					samples[i] = (float)value / 32768.0f;
				}
				break;
			}
			offset += 8 + chunk_size + (chunk_size & 1);
		}
	}

	free(bytes);
	return samples;
}
