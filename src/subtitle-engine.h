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

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Decides what is on screen: lays a text out in lines, keeps only the last
 * ones when there are too many, and lets it fade out once it has been shown
 * long enough. Provisional and final texts are handled alike: each new text
 * replaces the previous one and restarts its time on screen. The caller gives
 * the current time, so nothing here depends on a clock, on OBS or on threads;
 * use it from a single thread. */
struct subtitle_engine_params {
	/* Longest line, in characters, and number of lines shown at once. */
	uint32_t line_chars;
	uint32_t max_lines;
	/* Time on screen before fading: per_char_ms for each character shown,
	 * kept between min_hold_ms and max_hold_ms. */
	uint32_t min_hold_ms;
	uint32_t max_hold_ms;
	uint32_t hold_per_char_ms;
	uint32_t fade_ms;
};

struct subtitle_engine {
	struct subtitle_engine_params params;
	char *text;
	uint64_t shown_ms;
	uint32_t hold_ms;
};

struct subtitle_engine_params subtitle_engine_default_params(void);
void subtitle_engine_init(struct subtitle_engine *engine, const struct subtitle_engine_params *params);
void subtitle_engine_free(struct subtitle_engine *engine);
/* Replaces what is shown with a UTF-8 text; an empty text clears it. */
void subtitle_engine_show(struct subtitle_engine *engine, const char *text, uint64_t now_ms);
void subtitle_engine_clear(struct subtitle_engine *engine);
/* Returns the lines to show, separated by line feeds, and their opacity from
 * 1 down to 0 while fading; NULL when there is nothing to show. The pointer
 * stays valid until the next call that changes the engine. */
const char *subtitle_engine_text(struct subtitle_engine *engine, uint64_t now_ms, float *opacity);

#ifdef __cplusplus
}
#endif
