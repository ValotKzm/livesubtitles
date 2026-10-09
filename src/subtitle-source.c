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

#include <obs-module.h>
#include <media-io/audio-resampler.h>
#include <util/platform.h>
#include <util/threading.h>
#include <plugin-support.h>

#include <math.h>

#include "audio-ring.h"
#include "speech-detector.h"
#include "subtitle-engine.h"
#include "transcriber.h"
#include "translator.h"
#include "voice-activity.h"

/* Text is drawn by a private instance of the built-in OBS text source. */
#define TEXT_SOURCE_ID "text_gdiplus"
#define TEXT_FONT_FACE "Arial"
/* Font height in pixels. Two lines of the largest size still fit in the box. */
#define DEFAULT_FONT_SIZE 72
#define MIN_FONT_SIZE 36
#define MAX_FONT_SIZE 120
/* Width of an average character of running text in this font, as a share
 * of the font size (measured with GDI+ on English and French sentences),
 * and the share of the box a full line may take. Text in capitals is wider:
 * what does not fit is scaled down when drawn. */
#define AVERAGE_CHAR_WIDTH 0.445f
#define LINE_WIDTH_SHARE 0.97f

#define SETTING_AUDIO_SOURCE "audio_source"
#define SETTING_ENABLED "enabled"
#define SETTING_SPOKEN_LANGUAGE "spoken_language"
#define SETTING_SUBTITLE_LANGUAGE "subtitle_language"
#define SETTING_FONT_SIZE "font_size"
#define SETTING_TEXT_COLOR "text_color"
#define SETTING_BACKGROUND_COLOR "background_color"
#define SETTING_BACKGROUND_OPACITY "background_opacity"

/* White on a half-transparent black band reads on any image. */
#define DEFAULT_TEXT_COLOR 0xFFFFFF
#define DEFAULT_BACKGROUND_COLOR 0x000000
#define DEFAULT_BACKGROUND_OPACITY 50

/* Speech processing works on 16 kHz mono; at most this much audio is kept. */
#define CAPTURE_SAMPLE_RATE 16000
#define CAPTURE_BUFFER_SECONDS 10

#define VAD_MODEL_FILE "models/ggml-silero-v6.2.0.bin"
#define STT_MODEL_FILE "models/ggml-small-q5_1.bin"
/* One translation model per pair of languages, named after their codes. */
#define MT_MODEL_DIR_FORMAT "models/opus-mt-%s-%s"
#define DEFAULT_SPOKEN_LANGUAGE "fr"
#define DEFAULT_SUBTITLE_LANGUAGE "en"
#define STT_MAX_THREADS 8
/* More threads than this did not make translation faster. */
#define MT_MAX_THREADS 4

/* Audio kept from just before speech is detected, so that the first syllable
 * is not cut, and the longest stretch of speech sent to recognition at once. */
#define PREROLL_SAMPLES (CAPTURE_SAMPLE_RATE * 3 / 10)
#define UTTERANCE_MAX_SAMPLES (CAPTURE_SAMPLE_RATE * 20)

/* While the user keeps talking, what was said so far is recognized again
 * from its beginning and shown as provisional text: first once this much
 * speech is collected, then each time this much more has been added. Each
 * pass costs a full inference, so shorter steps mostly burn processor time. */
#define PARTIAL_FIRST_SAMPLES (CAPTURE_SAMPLE_RATE * 3)
#define PARTIAL_STEP_SAMPLES (CAPTURE_SAMPLE_RATE * 2)

/* Box the subtitles are laid out in, in pixels: the text is centered at its
 * bottom, so that it stays in place in the scene whatever its length. */
#define TEXT_BOX_WIDTH 1600
#define TEXT_BOX_HEIGHT 300

#define STATUS_INTERVAL_SECONDS 0.25f
/* The text fades out in this many steps, each one redrawing it. */
#define OPACITY_STEPS 10
#define START_RETRY_NS 1000000000ULL
#define WORKER_ACTIVE_WAIT_MS 30
#define WORKER_IDLE_WAIT_MS 250
#define LEVEL_FLOOR_DB -60.0f

/* Languages offered, as the codes the speech model and the names of the
 * translation models use, with the locale key of their name. */
static const struct {
	const char *code;
	const char *name_key;
} languages[] = {
	{"fr", "LanguageFrench"},
	{"en", "LanguageEnglish"},
};
#define LANGUAGE_COUNT (sizeof(languages) / sizeof(languages[0]))

/* Returns the code as a constant string, or the fallback if it is not one
 * of the languages offered. */
static const char *known_language(const char *code, const char *fallback)
{
	for (size_t i = 0; i < LANGUAGE_COUNT; i++) {
		if (code && strcmp(code, languages[i].code) == 0)
			return languages[i].code;
	}
	return fallback;
}

enum pipeline_state {
	PIPELINE_IDLE,
	PIPELINE_SOURCE_UNAVAILABLE,
	PIPELINE_MODEL_ERROR,
	PIPELINE_LISTENING,
};

struct subtitle_source {
	/* Set once in create and released in destroy; never reassigned. */
	obs_source_t *text;
	char *vad_model_path;
	char *stt_model_path;

	/* Guards the fields below, shared between the settings, graphics,
	 * audio and worker threads. Never held while calling into another
	 * source or into the speech model. */
	pthread_mutex_t mutex;
	bool enabled;
	char *audio_source_uuid;
	/* Constant strings from the list of languages. */
	const char *spoken_language;
	const char *subtitle_language;
	uint32_t font_size;
	uint32_t text_color;
	uint32_t background_color;
	uint32_t background_opacity;
	struct audio_ring ring;
	float peak;
	enum pipeline_state state;
	bool speaking;
	/* Latest translation, and a count of its changes so that the graphics
	 * thread notices each new one. */
	char *subtitle;
	uint32_t subtitle_version;

	/* The worker owns capture and speech processing for the whole life of
	 * the source, so that neither ever runs on the graphics thread. */
	pthread_t worker;
	bool worker_started;
	os_event_t *wake;
	volatile bool stopping;

	/* Used by the worker only, except the resampler, which the audio
	 * callback alone uses between add and remove of that callback. */
	obs_source_t *capture_source;
	audio_resampler_t *resampler;
	struct voice_activity *vad;
	struct speech_detector detector;
	struct transcriber *transcriber;
	/* None when the subtitles are in the spoken language. */
	struct translator *translator;
	const char *capture_spoken_language;
	const char *capture_subtitle_language;
	struct audio_ring preroll;
	float *utterance;
	size_t utterance_size;
	size_t partial_size;
	uint32_t utterance_count;
	uint32_t partial_count;
	uint64_t inference_total_ns;
	uint64_t inference_max_ns;
	uint64_t translation_total_ns;
	uint64_t last_start_attempt_ns;
	bool model_error_logged;

	/* Used by the graphics thread only. */
	float status_elapsed;
	char *status_text;
	int status_opacity;
	uint32_t drawn_font_size;
	uint32_t drawn_text_color;
	uint32_t drawn_background_color;
	uint32_t drawn_background_opacity;
	struct subtitle_engine engine;
	uint32_t shown_version;
	bool subtitle_seen;
};

static const char *subtitle_source_get_name(void *type_data)
{
	UNUSED_PARAMETER(type_data);
	return obs_module_text("LiveSubtitlesSource");
}

static obs_source_t *create_text_source(void)
{
	const char *id = obs_get_latest_input_type_id(TEXT_SOURCE_ID);
	if (!id) {
		obs_log(LOG_ERROR, "built-in text source '%s' is not available", TEXT_SOURCE_ID);
		return NULL;
	}

	obs_data_t *settings = obs_data_create();
	obs_data_t *font = obs_data_create();
	obs_data_set_string(font, "face", TEXT_FONT_FACE);
	obs_data_set_int(font, "size", DEFAULT_FONT_SIZE);
	obs_data_set_obj(settings, "font", font);
	/* No fixed extents: the text source is as large as its text, and so
	 * is the background it draws. The lines are already cut to length. */
	obs_data_set_string(settings, "align", "center");

	obs_source_t *text = obs_source_create_private(id, NULL, settings);
	if (!text)
		obs_log(LOG_ERROR, "failed to create text source '%s'", id);

	obs_data_release(font);
	obs_data_release(settings);
	return text;
}

/* Puts a space at both ends of each line, so that the background does not
 * stop right at the first and last letters. */
static char *pad_lines(const char *text)
{
	size_t lines = 1;
	for (const char *c = text; *c; c++) {
		if (*c == '\n')
			lines++;
	}

	char *padded = bmalloc(strlen(text) + lines * 2 + 1);
	char *out = padded;
	*out++ = ' ';
	for (const char *c = text; *c; c++) {
		if (*c == '\n') {
			*out++ = ' ';
			*out++ = '\n';
			*out++ = ' ';
		} else {
			*out++ = *c;
		}
	}
	*out++ = ' ';
	*out = '\0';
	return padded;
}

/* Opacity in percent. Does nothing when what is drawn would not change. */
static void set_display_text(struct subtitle_source *context, const char *text, int opacity)
{
	if (!context->text)
		return;

	pthread_mutex_lock(&context->mutex);
	uint32_t font_size = context->font_size;
	uint32_t text_color = context->text_color;
	uint32_t background_color = context->background_color;
	uint32_t background_opacity = context->background_opacity;
	pthread_mutex_unlock(&context->mutex);

	if (context->status_text && strcmp(context->status_text, text) == 0 && context->status_opacity == opacity &&
	    context->drawn_font_size == font_size && context->drawn_text_color == text_color && context->drawn_background_color == background_color &&
	    context->drawn_background_opacity == background_opacity)
		return;

	bfree(context->status_text);
	context->status_text = bstrdup(text);
	context->status_opacity = opacity;
	context->drawn_font_size = font_size;
	context->drawn_text_color = text_color;
	context->drawn_background_color = background_color;
	context->drawn_background_opacity = background_opacity;

	char *padded = *text ? pad_lines(text) : bstrdup("");
	obs_data_t *settings = obs_data_create();
	obs_data_t *font = obs_data_create();
	obs_data_set_string(font, "face", TEXT_FONT_FACE);
	obs_data_set_int(font, "size", font_size);
	obs_data_set_obj(settings, "font", font);
	obs_data_release(font);
	obs_data_set_string(settings, "text", padded);
	obs_data_set_int(settings, "color", text_color);
	obs_data_set_int(settings, "opacity", opacity);
	obs_data_set_int(settings, "bk_color", background_color);
	/* The background fades out with the text. */
	obs_data_set_int(settings, "bk_opacity", background_opacity * opacity / 100);
	obs_source_update(context->text, settings);
	obs_data_release(settings);
	bfree(padded);
}

static void set_status_text(struct subtitle_source *context, const char *status)
{
	set_display_text(context, status, 100);
}

/* Called by libobs on the audio thread of the captured source, with audio in
 * the OBS output format (planar float). Kept short: resample and store. */
static void audio_capture_callback(void *param, obs_source_t *source, const struct audio_data *audio, bool muted)
{
	UNUSED_PARAMETER(source);

	struct subtitle_source *context = param;
	if (muted || !audio->frames)
		return;

	uint8_t *output[MAX_AV_PLANES] = {0};
	uint32_t out_frames = 0;
	uint64_t ts_offset = 0;
	if (!audio_resampler_resample(context->resampler, output, &out_frames, &ts_offset,
				      (const uint8_t *const *)audio->data, audio->frames))
		return;
	if (!output[0] || !out_frames)
		return;

	const float *samples = (const float *)output[0];
	float peak = 0.0f;
	for (uint32_t i = 0; i < out_frames; i++) {
		float value = fabsf(samples[i]);
		if (value > peak)
			peak = value;
	}

	pthread_mutex_lock(&context->mutex);
	audio_ring_write(&context->ring, samples, out_frames);
	if (peak > context->peak)
		context->peak = peak;
	pthread_mutex_unlock(&context->mutex);
}

static void set_pipeline_state(struct subtitle_source *context, enum pipeline_state state)
{
	pthread_mutex_lock(&context->mutex);
	context->state = state;
	pthread_mutex_unlock(&context->mutex);
}

static void stop_capture(struct subtitle_source *context)
{
	if (!context->capture_source)
		return;

	/* libobs runs the callback under the same lock remove takes, so the
	 * callback is no longer running once this returns. */
	obs_source_remove_audio_capture_callback(context->capture_source, audio_capture_callback, context);
	obs_source_release(context->capture_source);
	context->capture_source = NULL;

	audio_resampler_destroy(context->resampler);
	context->resampler = NULL;
	voice_activity_destroy(context->vad);
	context->vad = NULL;
	transcriber_destroy(context->transcriber);
	context->transcriber = NULL;
	translator_destroy(context->translator);
	context->translator = NULL;
	audio_ring_reset(&context->preroll);
	context->utterance_size = 0;
	context->partial_size = 0;
	context->last_start_attempt_ns = 0;

	pthread_mutex_lock(&context->mutex);
	audio_ring_reset(&context->ring);
	context->peak = 0.0f;
	context->speaking = false;
	char *subtitle = context->subtitle;
	context->subtitle = NULL;
	context->subtitle_version++;
	pthread_mutex_unlock(&context->mutex);
	bfree(subtitle);

	/* Timings only: what was said is never written to the log. */
	if (context->utterance_count) {
		obs_log(LOG_INFO,
			"audio capture stopped; %u utterance(s) and %u provisional pass(es), recognition and translation took %.0f ms on average, %.0f ms at most, of which translation %.0f ms on average",
			context->utterance_count, context->partial_count,
			(double)context->inference_total_ns / (context->utterance_count + context->partial_count) / 1000000.0,
			(double)context->inference_max_ns / 1000000.0,
			(double)context->translation_total_ns / (context->utterance_count + context->partial_count) /
				1000000.0);
	} else {
		obs_log(LOG_INFO, "audio capture stopped");
	}
	context->utterance_count = 0;
	context->partial_count = 0;
	context->inference_total_ns = 0;
	context->inference_max_ns = 0;
	context->translation_total_ns = 0;
}

static int recognition_threads(void)
{
	int cores = os_get_physical_cores();
	int threads = cores / 2;
	if (threads < 1)
		threads = 1;
	return threads > STT_MAX_THREADS ? STT_MAX_THREADS : threads;
}

static void log_model_error(struct subtitle_source *context, const char *file)
{
	if (context->model_error_logged)
		return;

	obs_log(LOG_ERROR, "failed to load the model '%s'", file);
	context->model_error_logged = true;
}

static enum pipeline_state start_capture(struct subtitle_source *context, const char *uuid, const char *spoken,
					  const char *subtitle)
{
	struct obs_audio_info oai;
	if (!obs_get_audio_info(&oai))
		return PIPELINE_SOURCE_UNAVAILABLE;

	obs_source_t *source = obs_get_source_by_uuid(uuid);
	if (!source)
		return PIPELINE_SOURCE_UNAVAILABLE;

	if (obs_source_removed(source) || !(obs_source_get_output_flags(source) & OBS_SOURCE_AUDIO)) {
		obs_source_release(source);
		return PIPELINE_SOURCE_UNAVAILABLE;
	}

	context->vad = voice_activity_create(context->vad_model_path);
	if (!context->vad) {
		log_model_error(context, VAD_MODEL_FILE);
		obs_source_release(source);
		return PIPELINE_MODEL_ERROR;
	}

	/* The recognition model writes what was said; translation follows. */
	struct transcriber_params transcriber_params = {
		.model_path = context->stt_model_path,
		.language = spoken,
		.translate = false,
		.threads = recognition_threads(),
		/* The fitted context is faster on average but showed slow
		 * outliers; the default is steadier. */
		.audio_context = 0,
	};
	context->transcriber = transcriber_create(&transcriber_params);
	if (!context->transcriber) {
		log_model_error(context, STT_MODEL_FILE);
		voice_activity_destroy(context->vad);
		context->vad = NULL;
		obs_source_release(source);
		return PIPELINE_MODEL_ERROR;
	}

	/* Subtitles in the spoken language need no translation. */
	if (strcmp(spoken, subtitle) != 0) {
		char model_dir[64];
		snprintf(model_dir, sizeof(model_dir), MT_MODEL_DIR_FORMAT, spoken, subtitle);
		char *model_path = obs_module_file(model_dir);
		int translation_threads = recognition_threads();
		struct translator_params translator_params = {
			.model_dir = model_path,
			.threads = translation_threads > MT_MAX_THREADS ? MT_MAX_THREADS : translation_threads,
		};
		context->translator = model_path ? translator_create(&translator_params) : NULL;
		bfree(model_path);
		if (!context->translator) {
			log_model_error(context, model_dir);
			voice_activity_destroy(context->vad);
			context->vad = NULL;
			transcriber_destroy(context->transcriber);
			context->transcriber = NULL;
			obs_source_release(source);
			return PIPELINE_MODEL_ERROR;
		}
	}
	context->model_error_logged = false;

	struct resample_info src = {
		.samples_per_sec = oai.samples_per_sec,
		.format = AUDIO_FORMAT_FLOAT_PLANAR,
		.speakers = oai.speakers,
	};
	struct resample_info dst = {
		.samples_per_sec = CAPTURE_SAMPLE_RATE,
		.format = AUDIO_FORMAT_FLOAT_PLANAR,
		.speakers = SPEAKERS_MONO,
	};
	context->resampler = audio_resampler_create(&dst, &src);
	if (!context->resampler) {
		obs_log(LOG_ERROR, "failed to create audio resampler");
		voice_activity_destroy(context->vad);
		context->vad = NULL;
		transcriber_destroy(context->transcriber);
		context->transcriber = NULL;
		translator_destroy(context->translator);
		context->translator = NULL;
		obs_source_release(source);
		return PIPELINE_SOURCE_UNAVAILABLE;
	}

	struct speech_detector_params params = speech_detector_default_params();
	speech_detector_init(&context->detector, &params);

	/* The reference keeps the captured source alive while the callback is
	 * registered; the worker drops it when the user removes that source. */
	context->capture_source = source;
	context->capture_spoken_language = spoken;
	context->capture_subtitle_language = subtitle;
	obs_source_add_audio_capture_callback(source, audio_capture_callback, context);
	obs_log(LOG_INFO, "audio capture started, %s to %s", spoken, subtitle);
	return PIPELINE_LISTENING;
}

/* Brings the capture state in line with the settings. */
static void reconcile_capture(struct subtitle_source *context)
{
	pthread_mutex_lock(&context->mutex);
	bool enabled = context->enabled;
	char *uuid = bstrdup(context->audio_source_uuid);
	const char *spoken = context->spoken_language;
	const char *subtitle = context->subtitle_language;
	pthread_mutex_unlock(&context->mutex);

	bool wanted = enabled && uuid && *uuid;

	if (context->capture_source) {
		/* A change of language starts over with the models it needs. */
		const char *current = obs_source_get_uuid(context->capture_source);
		if (!wanted || obs_source_removed(context->capture_source) || strcmp(current, uuid) != 0 ||
		    context->capture_spoken_language != spoken || context->capture_subtitle_language != subtitle)
			stop_capture(context);
	}

	if (!wanted) {
		context->last_start_attempt_ns = 0;
		set_pipeline_state(context, PIPELINE_IDLE);
	} else if (!context->capture_source) {
		/* The chosen source may not exist yet while a scene collection
		 * loads, so keep trying at a low rate. */
		uint64_t now = os_gettime_ns();
		if (!context->last_start_attempt_ns || now - context->last_start_attempt_ns >= START_RETRY_NS) {
			context->last_start_attempt_ns = now;
			set_pipeline_state(context, start_capture(context, uuid, spoken, subtitle));
		}
	}

	bfree(uuid);
}

/* Recognizes the speech collected so far and publishes its translation. A
 * provisional pass keeps the speech, so that the next pass starts over from
 * its beginning and replaces the text instead of adding to it. */
static void recognize_utterance(struct subtitle_source *context, bool final)
{
	uint64_t begin = os_gettime_ns();
	char *text = transcriber_run(context->transcriber, context->utterance, context->utterance_size);

	/* The model describes non-speech sounds in brackets or parentheses,
	 * such as "[BLANK_AUDIO]"; those are not subtitles. */
	char *subtitle = NULL;
	if (text && *text && *text != '[' && *text != '(') {
		if (context->translator) {
			uint64_t translation_begin = os_gettime_ns();
			char *translation = translator_run(context->translator, text);
			context->translation_total_ns += os_gettime_ns() - translation_begin;
			if (translation && *translation)
				subtitle = bstrdup(translation);
			translator_free_text(translation);
		} else {
			subtitle = bstrdup(text);
		}
	}
	transcriber_free_text(text);
	uint64_t elapsed = os_gettime_ns() - begin;

	if (final) {
		context->utterance_size = 0;
		context->partial_size = 0;
		context->utterance_count++;
	} else {
		context->partial_size = context->utterance_size;
		context->partial_count++;
	}
	context->inference_total_ns += elapsed;
	if (elapsed > context->inference_max_ns)
		context->inference_max_ns = elapsed;

	if (subtitle) {
		pthread_mutex_lock(&context->mutex);
		char *previous = context->subtitle;
		context->subtitle = subtitle;
		context->subtitle_version++;
		pthread_mutex_unlock(&context->mutex);
		bfree(previous);
	}
}

/* Shows provisional text during a long stretch of speech. Only called once
 * the waiting audio is processed, so that recognition never falls behind. */
static void recognize_partial_if_due(struct subtitle_source *context)
{
	if (!speech_detector_is_speaking(&context->detector))
		return;
	if (context->utterance_size < PARTIAL_FIRST_SAMPLES ||
	    context->utterance_size - context->partial_size < PARTIAL_STEP_SAMPLES)
		return;

	recognize_utterance(context, false);
}

/* Runs the speech models over every complete window waiting in the ring. */
static void process_audio(struct subtitle_source *context)
{
	float window[VOICE_ACTIVITY_WINDOW_SAMPLES];

	while (!os_atomic_load_bool(&context->stopping)) {
		pthread_mutex_lock(&context->mutex);
		bool available = audio_ring_size(&context->ring) >= VOICE_ACTIVITY_WINDOW_SAMPLES;
		if (available)
			audio_ring_read(&context->ring, window, VOICE_ACTIVITY_WINDOW_SAMPLES);
		pthread_mutex_unlock(&context->mutex);
		if (!available) {
			recognize_partial_if_due(context);
			break;
		}

		float probability = 0.0f;
		if (!voice_activity_process(context->vad, window, &probability))
			break;

		enum speech_event event =
			speech_detector_update(&context->detector, probability, VOICE_ACTIVITY_WINDOW_MS);
		bool speaking = speech_detector_is_speaking(&context->detector);

		pthread_mutex_lock(&context->mutex);
		context->speaking = speaking;
		pthread_mutex_unlock(&context->mutex);

		if (event == SPEECH_EVENT_START) {
			context->utterance_size = audio_ring_read(&context->preroll, context->utterance, PREROLL_SAMPLES);
			context->partial_size = 0;
		}

		if (!speaking && event != SPEECH_EVENT_END) {
			audio_ring_write(&context->preroll, window, VOICE_ACTIVITY_WINDOW_SAMPLES);
			continue;
		}

		memcpy(context->utterance + context->utterance_size, window, sizeof(window));
		context->utterance_size += VOICE_ACTIVITY_WINDOW_SAMPLES;

		/* Recognize at the end of speech, or when the buffer is full
		 * while the user keeps talking. */
		bool full = context->utterance_size + VOICE_ACTIVITY_WINDOW_SAMPLES > UTTERANCE_MAX_SAMPLES;
		if (event == SPEECH_EVENT_END || full)
			recognize_utterance(context, true);
	}
}

static void *worker_thread(void *data)
{
	struct subtitle_source *context = data;
	os_set_thread_name("livesubtitles worker");

	while (!os_atomic_load_bool(&context->stopping)) {
		reconcile_capture(context);
		if (context->capture_source)
			process_audio(context);

		os_event_timedwait(context->wake,
				   context->capture_source ? WORKER_ACTIVE_WAIT_MS : WORKER_IDLE_WAIT_MS);
	}

	stop_capture(context);
	return NULL;
}

/* Hands each new translation to the subtitle engine and draws what the engine
 * wants on screen. Returns false when there is no subtitle to draw. */
static bool update_subtitle(struct subtitle_source *context)
{
	uint64_t now_ms = os_gettime_ns() / 1000000;

	pthread_mutex_lock(&context->mutex);
	bool changed = context->subtitle_version != context->shown_version;
	char *subtitle = changed ? bstrdup(context->subtitle) : NULL;
	context->shown_version = context->subtitle_version;
	uint32_t font_size = context->font_size;
	pthread_mutex_unlock(&context->mutex);

	if (changed) {
		/* As many characters as fill the width of the box at this size. */
		context->engine.params.line_chars =
			(uint32_t)(TEXT_BOX_WIDTH * LINE_WIDTH_SHARE / (AVERAGE_CHAR_WIDTH * (float)font_size));
		/* No text means that the capture stopped. */
		subtitle_engine_show(&context->engine, subtitle, now_ms);
		context->subtitle_seen = subtitle != NULL;
		bfree(subtitle);
	}

	float opacity = 0.0f;
	const char *text = subtitle_engine_text(&context->engine, now_ms, &opacity);
	if (!text)
		return false;

	int step = (int)(opacity * OPACITY_STEPS + 0.5f);
	set_display_text(context, text, step * 100 / OPACITY_STEPS);
	return true;
}

static void update_status(struct subtitle_source *context, float seconds)
{
	context->status_elapsed += seconds;
	if (context->status_elapsed < STATUS_INTERVAL_SECONDS)
		return;
	context->status_elapsed = 0.0f;

	pthread_mutex_lock(&context->mutex);
	bool enabled = context->enabled;
	bool has_choice = context->audio_source_uuid && *context->audio_source_uuid;
	enum pipeline_state state = context->state;
	bool speaking = context->speaking;
	float peak = context->peak;
	context->peak = 0.0f;
	pthread_mutex_unlock(&context->mutex);

	if (!enabled) {
		set_status_text(context, obs_module_text("StatusDisabled"));
	} else if (!has_choice) {
		set_status_text(context, obs_module_text("StatusNoMicrophone"));
	} else if (state == PIPELINE_MODEL_ERROR) {
		set_status_text(context, obs_module_text("StatusModelError"));
	} else if (state == PIPELINE_SOURCE_UNAVAILABLE) {
		set_status_text(context, obs_module_text("StatusMicrophoneUnavailable"));
	} else if (state == PIPELINE_IDLE) {
		set_status_text(context, obs_module_text("StatusStarting"));
	} else if (context->subtitle_seen) {
		/* Once subtitles have appeared, leave the image clear between
		 * them instead of going back to the sound level. */
		set_status_text(context, "");
	} else {
		float db = peak > 0.0f ? 20.0f * log10f(peak) : LEVEL_FLOOR_DB;
		if (db < LEVEL_FLOOR_DB)
			db = LEVEL_FLOOR_DB;
		if (db > 0.0f)
			db = 0.0f;
		int level = (int)((db - LEVEL_FLOOR_DB) * 100.0f / -LEVEL_FLOOR_DB);

		char status[128];
		snprintf(status, sizeof(status), "%s %d %%",
			 obs_module_text(speaking ? "StatusSpeech" : "StatusSilence"), level);
		set_status_text(context, status);
	}
}

static void subtitle_source_update(void *data, obs_data_t *settings)
{
	struct subtitle_source *context = data;
	bool enabled = obs_data_get_bool(settings, SETTING_ENABLED);
	char *uuid = bstrdup(obs_data_get_string(settings, SETTING_AUDIO_SOURCE));
	const char *spoken =
		known_language(obs_data_get_string(settings, SETTING_SPOKEN_LANGUAGE), DEFAULT_SPOKEN_LANGUAGE);
	const char *subtitle =
		known_language(obs_data_get_string(settings, SETTING_SUBTITLE_LANGUAGE), DEFAULT_SUBTITLE_LANGUAGE);
	long long font_size = obs_data_get_int(settings, SETTING_FONT_SIZE);
	if (font_size < MIN_FONT_SIZE)
		font_size = MIN_FONT_SIZE;
	if (font_size > MAX_FONT_SIZE)
		font_size = MAX_FONT_SIZE;
	uint32_t text_color = (uint32_t)obs_data_get_int(settings, SETTING_TEXT_COLOR) & 0xFFFFFF;
	uint32_t background_color = (uint32_t)obs_data_get_int(settings, SETTING_BACKGROUND_COLOR) & 0xFFFFFF;
	long long background_opacity = obs_data_get_int(settings, SETTING_BACKGROUND_OPACITY);
	if (background_opacity < 0)
		background_opacity = 0;
	if (background_opacity > 100)
		background_opacity = 100;

	pthread_mutex_lock(&context->mutex);
	char *previous = context->audio_source_uuid;
	context->enabled = enabled;
	context->audio_source_uuid = uuid;
	context->spoken_language = spoken;
	context->subtitle_language = subtitle;
	context->font_size = (uint32_t)font_size;
	context->text_color = text_color;
	context->background_color = background_color;
	context->background_opacity = (uint32_t)background_opacity;
	pthread_mutex_unlock(&context->mutex);

	bfree(previous);
	os_event_signal(context->wake);
}

static void *subtitle_source_create(obs_data_t *settings, obs_source_t *source)
{
	UNUSED_PARAMETER(source);

	struct subtitle_source *context = bzalloc(sizeof(struct subtitle_source));
	if (pthread_mutex_init(&context->mutex, NULL) != 0) {
		bfree(context);
		return NULL;
	}
	if (os_event_init(&context->wake, OS_EVENT_TYPE_AUTO) != 0) {
		pthread_mutex_destroy(&context->mutex);
		bfree(context);
		return NULL;
	}
	if (!audio_ring_init(&context->ring, CAPTURE_SAMPLE_RATE * CAPTURE_BUFFER_SECONDS) ||
	    !audio_ring_init(&context->preroll, PREROLL_SAMPLES)) {
		audio_ring_free(&context->ring);
		os_event_destroy(context->wake);
		pthread_mutex_destroy(&context->mutex);
		bfree(context);
		return NULL;
	}

	context->utterance = bmalloc(UTTERANCE_MAX_SAMPLES * sizeof(float));
	context->text = create_text_source();
	context->vad_model_path = obs_module_file(VAD_MODEL_FILE);
	context->stt_model_path = obs_module_file(STT_MODEL_FILE);
	context->status_elapsed = STATUS_INTERVAL_SECONDS;
	struct subtitle_engine_params engine_params = subtitle_engine_default_params();
	subtitle_engine_init(&context->engine, &engine_params);
	subtitle_source_update(context, settings);

	if (pthread_create(&context->worker, NULL, worker_thread, context) == 0)
		context->worker_started = true;
	else
		obs_log(LOG_ERROR, "failed to start the worker thread");
	return context;
}

static void subtitle_source_destroy(void *data)
{
	struct subtitle_source *context = data;

	/* The worker stops capture itself before it exits. */
	if (context->worker_started) {
		os_atomic_set_bool(&context->stopping, true);
		os_event_signal(context->wake);
		pthread_join(context->worker, NULL);
	}

	obs_source_release(context->text);
	audio_ring_free(&context->ring);
	audio_ring_free(&context->preroll);
	os_event_destroy(context->wake);
	pthread_mutex_destroy(&context->mutex);
	bfree(context->utterance);
	bfree(context->subtitle);
	bfree(context->vad_model_path);
	bfree(context->stt_model_path);
	bfree(context->audio_source_uuid);
	bfree(context->status_text);
	subtitle_engine_free(&context->engine);
	bfree(context);
}

static void subtitle_source_video_tick(void *data, float seconds)
{
	struct subtitle_source *context = data;

	if (update_subtitle(context)) {
		/* Refresh the status as soon as the subtitle is gone. */
		context->status_elapsed = STATUS_INTERVAL_SECONDS;
		return;
	}
	update_status(context, seconds);
}

static void subtitle_source_get_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, SETTING_AUDIO_SOURCE, "");
	obs_data_set_default_string(settings, SETTING_SPOKEN_LANGUAGE, DEFAULT_SPOKEN_LANGUAGE);
	obs_data_set_default_string(settings, SETTING_SUBTITLE_LANGUAGE, DEFAULT_SUBTITLE_LANGUAGE);
	obs_data_set_default_bool(settings, SETTING_ENABLED, false);
	obs_data_set_default_int(settings, SETTING_FONT_SIZE, DEFAULT_FONT_SIZE);
	obs_data_set_default_int(settings, SETTING_TEXT_COLOR, DEFAULT_TEXT_COLOR);
	obs_data_set_default_int(settings, SETTING_BACKGROUND_COLOR, DEFAULT_BACKGROUND_COLOR);
	obs_data_set_default_int(settings, SETTING_BACKGROUND_OPACITY, DEFAULT_BACKGROUND_OPACITY);
}

static bool add_audio_source_to_list(void *param, obs_source_t *source)
{
	obs_property_t *list = param;

	if (obs_source_get_output_flags(source) & OBS_SOURCE_AUDIO)
		obs_property_list_add_string(list, obs_source_get_name(source), obs_source_get_uuid(source));
	return true;
}

static void add_language_list(obs_properties_t *props, const char *setting, const char *label_key)
{
	obs_property_t *list = obs_properties_add_list(props, setting, obs_module_text(label_key), OBS_COMBO_TYPE_LIST,
						       OBS_COMBO_FORMAT_STRING);
	for (size_t i = 0; i < LANGUAGE_COUNT; i++)
		obs_property_list_add_string(list, obs_module_text(languages[i].name_key), languages[i].code);
}

static obs_properties_t *subtitle_source_get_properties(void *data)
{
	UNUSED_PARAMETER(data);

	obs_properties_t *props = obs_properties_create();
	obs_property_t *list = obs_properties_add_list(props, SETTING_AUDIO_SOURCE, obs_module_text("Microphone"),
						       OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(list, obs_module_text("MicrophoneNone"), "");
	obs_enum_sources(add_audio_source_to_list, list);

	add_language_list(props, SETTING_SPOKEN_LANGUAGE, "SpokenLanguage");
	add_language_list(props, SETTING_SUBTITLE_LANGUAGE, "SubtitleLanguage");
	obs_properties_add_bool(props, SETTING_ENABLED, obs_module_text("Enabled"));

	obs_properties_add_int_slider(props, SETTING_FONT_SIZE, obs_module_text("FontSize"), MIN_FONT_SIZE, MAX_FONT_SIZE,
				      2);
	obs_properties_add_color(props, SETTING_TEXT_COLOR, obs_module_text("TextColor"));
	obs_properties_add_color(props, SETTING_BACKGROUND_COLOR, obs_module_text("BackgroundColor"));
	obs_property_t *opacity = obs_properties_add_int_slider(props, SETTING_BACKGROUND_OPACITY,
								obs_module_text("BackgroundOpacity"), 0, 100, 1);
	obs_property_int_set_suffix(opacity, " %");
	return props;
}

static uint32_t subtitle_source_get_width(void *data)
{
	UNUSED_PARAMETER(data);
	return TEXT_BOX_WIDTH;
}

static uint32_t subtitle_source_get_height(void *data)
{
	UNUSED_PARAMETER(data);
	return TEXT_BOX_HEIGHT;
}

static void subtitle_source_video_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);

	struct subtitle_source *context = data;
	if (!context->text)
		return;

	float width = (float)obs_source_get_width(context->text);
	float height = (float)obs_source_get_height(context->text);
	if (width <= 0.0f || height <= 0.0f)
		return;

	/* Centered at the bottom of the box, and scaled down if a line of
	 * wide characters would stick out of it. */
	float scale = 1.0f;
	if (width > (float)TEXT_BOX_WIDTH)
		scale = (float)TEXT_BOX_WIDTH / width;
	if (height * scale > (float)TEXT_BOX_HEIGHT)
		scale = (float)TEXT_BOX_HEIGHT / height;
	float x = ((float)TEXT_BOX_WIDTH - width * scale) / 2.0f;
	float y = (float)TEXT_BOX_HEIGHT - height * scale;
	gs_matrix_push();
	gs_matrix_translate3f(x, y, 0.0f);
	gs_matrix_scale3f(scale, scale, 1.0f);
	obs_source_video_render(context->text);
	gs_matrix_pop();
}

struct obs_source_info subtitle_source_info = {
	.id = "livesubtitles_source",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,
	.icon_type = OBS_ICON_TYPE_TEXT,
	.get_name = subtitle_source_get_name,
	.create = subtitle_source_create,
	.destroy = subtitle_source_destroy,
	.update = subtitle_source_update,
	.get_defaults = subtitle_source_get_defaults,
	.get_properties = subtitle_source_get_properties,
	.get_width = subtitle_source_get_width,
	.get_height = subtitle_source_get_height,
	.video_tick = subtitle_source_video_tick,
	.video_render = subtitle_source_video_render,
};
