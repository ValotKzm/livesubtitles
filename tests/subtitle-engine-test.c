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

#include "check.h"

#include <stdbool.h>
#include <string.h>

static const struct subtitle_engine_params test_params = {
	.line_chars = 10,
	.max_lines = 2,
	.min_hold_ms = 1000,
	.max_hold_ms = 2000,
	.hold_per_char_ms = 100,
	.fade_ms = 500,
};

static bool shows(struct subtitle_engine *engine, uint64_t now_ms, const char *expected)
{
	float opacity;
	const char *text = subtitle_engine_text(engine, now_ms, &opacity);
	return text && strcmp(text, expected) == 0;
}

static void test_nothing_to_show(void)
{
	struct subtitle_engine engine;
	subtitle_engine_init(&engine, &test_params);

	float opacity = 1.0f;
	CHECK(subtitle_engine_text(&engine, 0, &opacity) == NULL);
	CHECK(opacity == 0.0f);

	subtitle_engine_show(&engine, "", 0);
	CHECK(subtitle_engine_text(&engine, 0, &opacity) == NULL);
	subtitle_engine_show(&engine, "  \n ", 0);
	CHECK(subtitle_engine_text(&engine, 0, &opacity) == NULL);
	subtitle_engine_show(&engine, NULL, 0);
	CHECK(subtitle_engine_text(&engine, 0, &opacity) == NULL);

	subtitle_engine_free(&engine);
}

static void test_wraps_between_words(void)
{
	struct subtitle_engine engine;
	subtitle_engine_init(&engine, &test_params);

	subtitle_engine_show(&engine, "short", 0);
	CHECK(shows(&engine, 0, "short"));

	/* A line holds exactly line_chars characters, spaces included. */
	subtitle_engine_show(&engine, "abcd efghi jk", 0);
	CHECK(shows(&engine, 0, "abcd efghi\njk"));

	subtitle_engine_show(&engine, "abcd efghij k", 0);
	CHECK(shows(&engine, 0, "abcd\nefghij k"));

	/* Runs of spaces and line feeds count as one separator. */
	subtitle_engine_show(&engine, "  one   two\nthree  ", 0);
	CHECK(shows(&engine, 0, "one two\nthree"));

	subtitle_engine_free(&engine);
}

static void test_long_word_gets_its_own_line(void)
{
	struct subtitle_engine engine;
	subtitle_engine_init(&engine, &test_params);

	subtitle_engine_show(&engine, "a incomprehensible b", 0);
	CHECK(shows(&engine, 0, "incomprehensible\nb"));

	subtitle_engine_free(&engine);
}

static void test_counts_characters_not_bytes(void)
{
	struct subtitle_engine engine;
	subtitle_engine_init(&engine, &test_params);

	/* Ten characters, of which five take two bytes each in UTF-8. */
	subtitle_engine_show(&engine, "\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9 abcd x", 0);
	CHECK(shows(&engine, 0, "\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9 abcd\nx"));

	subtitle_engine_free(&engine);
}

static void test_keeps_the_last_lines(void)
{
	struct subtitle_engine engine;
	subtitle_engine_init(&engine, &test_params);

	subtitle_engine_show(&engine, "first line second one third line fourth", 0);
	CHECK(shows(&engine, 0, "third line\nfourth"));

	subtitle_engine_free(&engine);
}

static void test_fades_out_after_its_time(void)
{
	struct subtitle_engine engine;
	subtitle_engine_init(&engine, &test_params);
	float opacity = 0.0f;

	/* Fifteen characters shown: 1500 ms before fading. */
	subtitle_engine_show(&engine, "abcd efghi jklm", 10000);
	CHECK(subtitle_engine_text(&engine, 10000, &opacity) != NULL);
	CHECK(opacity == 1.0f);
	CHECK(subtitle_engine_text(&engine, 11500, &opacity) != NULL);
	CHECK(opacity == 1.0f);

	CHECK(subtitle_engine_text(&engine, 11750, &opacity) != NULL);
	CHECK(opacity > 0.45f && opacity < 0.55f);

	CHECK(subtitle_engine_text(&engine, 12000, &opacity) == NULL);
	CHECK(opacity == 0.0f);
	/* Once gone, it stays gone, even if the clock were to go back. */
	CHECK(subtitle_engine_text(&engine, 10000, &opacity) == NULL);

	subtitle_engine_free(&engine);
}

static void test_hold_time_is_bounded(void)
{
	struct subtitle_engine engine;
	subtitle_engine_init(&engine, &test_params);
	float opacity = 0.0f;

	/* Two characters would give 200 ms: the minimum applies. */
	subtitle_engine_show(&engine, "ab", 0);
	CHECK(subtitle_engine_text(&engine, 1000, &opacity) != NULL);
	CHECK(opacity == 1.0f);
	CHECK(subtitle_engine_text(&engine, 1001, &opacity) != NULL);
	CHECK(opacity < 1.0f);

	/* Two full lines would give 2100 ms: the maximum applies. */
	subtitle_engine_show(&engine, "abcdefghij klmnopqrst", 0);
	CHECK(subtitle_engine_text(&engine, 2000, &opacity) != NULL);
	CHECK(opacity == 1.0f);
	CHECK(subtitle_engine_text(&engine, 2001, &opacity) != NULL);
	CHECK(opacity < 1.0f);

	subtitle_engine_free(&engine);
}

static void test_new_text_replaces_and_restarts(void)
{
	struct subtitle_engine engine;
	subtitle_engine_init(&engine, &test_params);
	float opacity = 0.0f;

	subtitle_engine_show(&engine, "one", 0);
	/* Replaced while fading: fully visible again. */
	CHECK(subtitle_engine_text(&engine, 1200, &opacity) != NULL);
	CHECK(opacity < 1.0f);
	subtitle_engine_show(&engine, "one two", 1200);
	CHECK(shows(&engine, 1200, "one two"));
	CHECK(subtitle_engine_text(&engine, 2200, &opacity) != NULL);
	CHECK(opacity == 1.0f);

	subtitle_engine_clear(&engine);
	CHECK(subtitle_engine_text(&engine, 2200, &opacity) == NULL);

	subtitle_engine_free(&engine);
}

int main(void)
{
	test_nothing_to_show();
	test_wraps_between_words();
	test_long_word_gets_its_own_line();
	test_counts_characters_not_bytes();
	test_keeps_the_last_lines();
	test_fades_out_after_its_time();
	test_hold_time_is_bounded();
	test_new_text_replaces_and_restarts();
	return check_result("subtitle-engine");
}
