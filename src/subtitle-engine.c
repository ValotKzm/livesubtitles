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

#include "subtitle-engine.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static bool is_space(char c)
{
	return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

/* Bytes that continue a UTF-8 character do not start a new one. */
static bool starts_character(char c)
{
	return ((unsigned char)c & 0xC0) != 0x80;
}

static uint32_t count_characters(const char *text, size_t size)
{
	uint32_t count = 0;
	for (size_t i = 0; i < size; i++) {
		if (starts_character(text[i]))
			count++;
	}
	return count;
}

/* Breaks the text between words into lines of at most line_chars characters;
 * a longer word gets a line of its own. Never longer than the input, since
 * each run of spaces becomes a single space or line feed. */
static char *wrap_text(const char *text, uint32_t line_chars)
{
	char *wrapped = malloc(strlen(text) + 1);
	if (!wrapped)
		return NULL;

	size_t size = 0;
	uint32_t line_length = 0;
	while (*text) {
		while (is_space(*text))
			text++;
		size_t word_size = 0;
		while (text[word_size] && !is_space(text[word_size]))
			word_size++;
		if (!word_size)
			break;

		uint32_t word_length = count_characters(text, word_size);
		if (line_length && line_length + 1 + word_length > line_chars) {
			wrapped[size++] = '\n';
			line_length = 0;
		} else if (line_length) {
			wrapped[size++] = ' ';
			line_length++;
		}
		memcpy(wrapped + size, text, word_size);
		size += word_size;
		line_length += word_length;
		text += word_size;
	}
	wrapped[size] = '\0';
	return wrapped;
}

/* Drops the first lines so that at most max_lines remain. */
static void keep_last_lines(char *text, uint32_t max_lines)
{
	uint32_t lines = 1;
	for (const char *c = text; *c; c++) {
		if (*c == '\n')
			lines++;
	}

	const char *first = text;
	for (; lines > max_lines; lines--)
		first = strchr(first, '\n') + 1;
	memmove(text, first, strlen(first) + 1);
}

struct subtitle_engine_params subtitle_engine_default_params(void)
{
	struct subtitle_engine_params params = {
		.line_chars = 40,
		.max_lines = 2,
		.min_hold_ms = 3000,
		.max_hold_ms = 7000,
		.hold_per_char_ms = 60,
		.fade_ms = 600,
	};
	return params;
}

void subtitle_engine_init(struct subtitle_engine *engine, const struct subtitle_engine_params *params)
{
	memset(engine, 0, sizeof(*engine));
	engine->params = *params;
	if (!engine->params.line_chars)
		engine->params.line_chars = 1;
	if (!engine->params.max_lines)
		engine->params.max_lines = 1;
}

void subtitle_engine_free(struct subtitle_engine *engine)
{
	subtitle_engine_clear(engine);
}

void subtitle_engine_clear(struct subtitle_engine *engine)
{
	free(engine->text);
	engine->text = NULL;
}

void subtitle_engine_show(struct subtitle_engine *engine, const char *text, uint64_t now_ms)
{
	subtitle_engine_clear(engine);
	if (!text)
		return;

	char *wrapped = wrap_text(text, engine->params.line_chars);
	if (!wrapped)
		return;
	if (!*wrapped) {
		free(wrapped);
		return;
	}
	keep_last_lines(wrapped, engine->params.max_lines);

	uint64_t hold_ms = (uint64_t)count_characters(wrapped, strlen(wrapped)) * engine->params.hold_per_char_ms;
	if (hold_ms > engine->params.max_hold_ms)
		hold_ms = engine->params.max_hold_ms;
	if (hold_ms < engine->params.min_hold_ms)
		hold_ms = engine->params.min_hold_ms;

	engine->text = wrapped;
	engine->shown_ms = now_ms;
	engine->hold_ms = (uint32_t)hold_ms;
}

const char *subtitle_engine_text(struct subtitle_engine *engine, uint64_t now_ms, float *opacity)
{
	*opacity = 0.0f;
	if (!engine->text)
		return NULL;

	uint64_t elapsed_ms = now_ms > engine->shown_ms ? now_ms - engine->shown_ms : 0;
	if (elapsed_ms <= engine->hold_ms) {
		*opacity = 1.0f;
		return engine->text;
	}

	uint64_t fading_ms = elapsed_ms - engine->hold_ms;
	if (fading_ms >= engine->params.fade_ms) {
		subtitle_engine_clear(engine);
		return NULL;
	}
	*opacity = 1.0f - (float)fading_ms / (float)engine->params.fade_ms;
	return engine->text;
}
