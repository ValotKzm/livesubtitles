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

#include "translator.h"

#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <ctranslate2/translator.h>
#include <ctranslate2/utils.h>
#include <sentencepiece_processor.h>

/* Width of the beam search. The measurements in docs/development.md were
 * taken with this value. */
#define BEAM_SIZE 4

/* OPUS-MT models expect this token at the end of the source sentence. */
#define END_OF_SENTENCE "</s>"

struct translator {
	sentencepiece::SentencePieceProcessor source;
	sentencepiece::SentencePieceProcessor target;
	std::unique_ptr<ctranslate2::Translator> model;
};

static char *copy_string(const std::string &text)
{
	char *copy = static_cast<char *>(malloc(text.size() + 1));
	if (copy)
		memcpy(copy, text.c_str(), text.size() + 1);
	return copy;
}

struct translator *translator_create(const struct translator_params *params)
{
	if (!params->model_dir)
		return nullptr;

	/* No C++ exception may reach the C callers. */
	try {
		const std::string model_dir = params->model_dir;
		auto translator = std::make_unique<struct translator>();
		if (!translator->source.Load(model_dir + "/source.spm").ok() ||
		    !translator->target.Load(model_dir + "/target.spm").ok())
			return nullptr;

		ctranslate2::ReplicaPoolConfig config;
		config.num_threads_per_replica = params->threads > 0 ? (size_t)params->threads : 1;
		try {
			translator->model = std::make_unique<ctranslate2::Translator>(
				model_dir, ctranslate2::Device::CPU, ctranslate2::ComputeType::INT8,
				std::vector<int>{0}, false, config);
		} catch (...) {
			ctranslate2::release_thread_resources();
			throw;
		}
		/* Loading the model leaves a pool of computation threads owned
		 * by the calling thread. Translations run on the model's own
		 * thread, so drop that pool now: left to the end of the calling
		 * thread, its destruction deadlocks on Windows. */
		ctranslate2::release_thread_resources();
		return translator.release();
	} catch (...) {
		return nullptr;
	}
}

void translator_destroy(struct translator *translator)
{
	delete translator;
}

char *translator_run(struct translator *translator, const char *text)
{
	if (!translator || !text)
		return nullptr;

	try {
		std::vector<std::string> tokens;
		if (!translator->source.Encode(text, &tokens).ok())
			return nullptr;
		if (tokens.empty())
			return copy_string("");
		tokens.emplace_back(END_OF_SENTENCE);

		ctranslate2::TranslationOptions options;
		options.beam_size = BEAM_SIZE;
		const auto results = translator->model->translate_batch({tokens}, options);

		std::string translation;
		if (results.empty() || !translator->target.Decode(results[0].output(), &translation).ok())
			return nullptr;
		return copy_string(translation);
	} catch (...) {
		return nullptr;
	}
}

void translator_free_text(char *text)
{
	free(text);
}
