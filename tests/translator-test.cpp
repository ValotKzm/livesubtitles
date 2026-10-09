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

/* Runs the real French to English model: translator-test <model directory>.
 * A hang here, caught by the test timeout, means that a computation thread
 * pool outlived its owner: see cmake/patches/ctranslate2-windows.patch. */

#include "translator.h"

#include "check.h"

#include <cctype>
#include <string>
#include <thread>

static bool contains_ignoring_case(const char *text, const char *needle)
{
	std::string lowered = text;
	for (char &c : lowered)
		c = (char)tolower((unsigned char)c);
	return lowered.find(needle) != std::string::npos;
}

static void check_translates(const char *model_dir, int threads)
{
	struct translator_params params = {model_dir, threads};
	struct translator *translator = translator_create(&params);
	CHECK(translator != nullptr);
	if (!translator)
		return;

	char *text = translator_run(translator, "Bonjour tout le monde, bienvenue sur le stream.");
	CHECK(text != nullptr);
	if (text) {
		CHECK(contains_ignoring_case(text, "welcome"));
		CHECK(contains_ignoring_case(text, "stream"));
	}
	translator_free_text(text);

	/* Accented characters go through as UTF-8. */
	text = translator_run(translator, "La batterie de ma manette est vidée.");
	CHECK(text != nullptr);
	if (text)
		CHECK(contains_ignoring_case(text, "battery"));
	translator_free_text(text);

	/* Nothing to translate is not an error. */
	text = translator_run(translator, "");
	CHECK(text != nullptr);
	if (text)
		CHECK(text[0] == '\0');
	translator_free_text(text);

	translator_destroy(translator);
}

int main(int argc, char **argv)
{
	if (argc != 2) {
		printf("usage: translator-test <model directory>\n");
		return 2;
	}

	struct translator_params params = {"this-model-does-not-exist", 2};
	CHECK(translator_create(&params) == nullptr);

	check_translates(argv[1], 1);
	check_translates(argv[1], 4);

	/* The plugin loads and runs the model on a worker thread that ends
	 * before the plugin is unloaded: that thread must be able to end. */
	std::thread worker(check_translates, argv[1], 4);
	worker.join();

	return check_result("translator");
}
