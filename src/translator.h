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

#ifdef __cplusplus
extern "C" {
#endif

/* Text translation for one language pair (an OPUS-MT model run by
 * CTranslate2, with its SentencePiece tokenizers). The pair is whatever the
 * model directory holds: the caller only sees text in and text out, so the
 * model can change without touching the callers. Not thread-safe; a run can
 * take hundreds of milliseconds, so never call it from a real-time or
 * rendering thread. Independent of OBS so it can be tested on its own. */

struct translator;

struct translator_params {
	/* Directory holding model.bin, its vocabulary, source.spm and
	 * target.spm, as a UTF-8 path. */
	const char *model_dir;
	int threads;
};

struct translator *translator_create(const struct translator_params *params);
void translator_destroy(struct translator *translator);
/* Returns the translation of one UTF-8 sentence (empty for empty input), or
 * NULL on failure. Release it with translator_free_text. */
char *translator_run(struct translator *translator, const char *text);
void translator_free_text(char *text);

#ifdef __cplusplus
}
#endif
