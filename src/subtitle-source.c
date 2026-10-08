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

/* Text is drawn by a private instance of the built-in OBS text source. */
#define TEXT_SOURCE_ID "text_gdiplus"
#define TEXT_FONT_FACE "Arial"
#define TEXT_FONT_SIZE 72

#define SETTING_AUDIO_SOURCE "audio_source"
#define SETTING_ENABLED "enabled"

/* Speech processing works on 16 kHz mono; at most this much audio is kept. */
#define CAPTURE_SAMPLE_RATE 16000
#define CAPTURE_BUFFER_SECONDS 10

#define STATUS_INTERVAL_SECONDS 0.25f
#define ATTACH_RETRY_SECONDS 1.0f
#define LEVEL_FLOOR_DB -60.0f

struct subtitle_source {
	/* Set once in create and released in destroy; never reassigned. */
	obs_source_t *text;

	/* Guards the fields below, shared between the settings, graphics and
	 * audio threads. Never held while calling into another source. */
	pthread_mutex_t mutex;
	bool enabled;
	char *audio_source_uuid;
	struct audio_ring ring;
	float peak;

	/* Capture state. Changed only by video_tick and destroy, which never
	 * run concurrently; the resampler is otherwise used by the audio
	 * callback alone, between add and remove of that callback. */
	obs_source_t *capture_source;
	audio_resampler_t *resampler;
	float retry_elapsed;

	float status_elapsed;
	char *status_text;
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
	obs_data_set_int(font, "size", TEXT_FONT_SIZE);
	obs_data_set_obj(settings, "font", font);

	obs_source_t *text = obs_source_create_private(id, NULL, settings);
	if (!text)
		obs_log(LOG_ERROR, "failed to create text source '%s'", id);

	obs_data_release(font);
	obs_data_release(settings);
	return text;
}

static void set_status_text(struct subtitle_source *context, const char *status)
{
	if (!context->text || (context->status_text && strcmp(context->status_text, status) == 0))
		return;

	bfree(context->status_text);
	context->status_text = bstrdup(status);

	obs_data_t *settings = obs_data_create();
	obs_data_set_string(settings, "text", status);
	obs_source_update(context->text, settings);
	obs_data_release(settings);
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

	pthread_mutex_lock(&context->mutex);
	audio_ring_reset(&context->ring);
	context->peak = 0.0f;
	pthread_mutex_unlock(&context->mutex);

	obs_log(LOG_INFO, "audio capture stopped");
}

static bool start_capture(struct subtitle_source *context, const char *uuid)
{
	struct obs_audio_info oai;
	if (!obs_get_audio_info(&oai))
		return false;

	obs_source_t *source = obs_get_source_by_uuid(uuid);
	if (!source)
		return false;

	if (obs_source_removed(source) || !(obs_source_get_output_flags(source) & OBS_SOURCE_AUDIO)) {
		obs_source_release(source);
		return false;
	}

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
		obs_source_release(source);
		return false;
	}

	/* The reference keeps the captured source alive while the callback is
	 * registered; video_tick drops it when the user removes that source. */
	context->capture_source = source;
	obs_source_add_audio_capture_callback(source, audio_capture_callback, context);
	obs_log(LOG_INFO, "audio capture started");
	return true;
}

/* Brings the capture state in line with the settings. */
static void reconcile_capture(struct subtitle_source *context, float seconds)
{
	pthread_mutex_lock(&context->mutex);
	bool enabled = context->enabled;
	char *uuid = bstrdup(context->audio_source_uuid);
	pthread_mutex_unlock(&context->mutex);

	bool wanted = enabled && uuid && *uuid;

	if (context->capture_source) {
		const char *current = obs_source_get_uuid(context->capture_source);
		if (!wanted || obs_source_removed(context->capture_source) || strcmp(current, uuid) != 0)
			stop_capture(context);
	}

	if (wanted && !context->capture_source) {
		/* The chosen source may not exist yet while a scene collection
		 * loads, so keep trying at a low rate. */
		context->retry_elapsed += seconds;
		if (context->retry_elapsed >= ATTACH_RETRY_SECONDS) {
			context->retry_elapsed = 0.0f;
			start_capture(context, uuid);
		}
	} else {
		context->retry_elapsed = ATTACH_RETRY_SECONDS;
	}

	bfree(uuid);
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
	float peak = context->peak;
	context->peak = 0.0f;
	pthread_mutex_unlock(&context->mutex);

	if (!enabled) {
		set_status_text(context, obs_module_text("StatusDisabled"));
	} else if (!has_choice) {
		set_status_text(context, obs_module_text("StatusNoMicrophone"));
	} else if (!context->capture_source) {
		set_status_text(context, obs_module_text("StatusMicrophoneUnavailable"));
	} else {
		float db = peak > 0.0f ? 20.0f * log10f(peak) : LEVEL_FLOOR_DB;
		if (db < LEVEL_FLOOR_DB)
			db = LEVEL_FLOOR_DB;
		if (db > 0.0f)
			db = 0.0f;
		int level = (int)((db - LEVEL_FLOOR_DB) * 100.0f / -LEVEL_FLOOR_DB);

		char status[128];
		snprintf(status, sizeof(status), "%s %d %%", obs_module_text("StatusListening"), level);
		set_status_text(context, status);
	}
}

static void subtitle_source_update(void *data, obs_data_t *settings)
{
	struct subtitle_source *context = data;
	bool enabled = obs_data_get_bool(settings, SETTING_ENABLED);
	char *uuid = bstrdup(obs_data_get_string(settings, SETTING_AUDIO_SOURCE));

	pthread_mutex_lock(&context->mutex);
	char *previous = context->audio_source_uuid;
	context->enabled = enabled;
	context->audio_source_uuid = uuid;
	pthread_mutex_unlock(&context->mutex);

	bfree(previous);
}

static void *subtitle_source_create(obs_data_t *settings, obs_source_t *source)
{
	UNUSED_PARAMETER(source);

	struct subtitle_source *context = bzalloc(sizeof(struct subtitle_source));
	if (pthread_mutex_init(&context->mutex, NULL) != 0) {
		bfree(context);
		return NULL;
	}
	if (!audio_ring_init(&context->ring, CAPTURE_SAMPLE_RATE * CAPTURE_BUFFER_SECONDS)) {
		pthread_mutex_destroy(&context->mutex);
		bfree(context);
		return NULL;
	}

	context->text = create_text_source();
	context->retry_elapsed = ATTACH_RETRY_SECONDS;
	context->status_elapsed = STATUS_INTERVAL_SECONDS;
	subtitle_source_update(context, settings);
	return context;
}

static void subtitle_source_destroy(void *data)
{
	struct subtitle_source *context = data;

	stop_capture(context);
	obs_source_release(context->text);
	audio_ring_free(&context->ring);
	pthread_mutex_destroy(&context->mutex);
	bfree(context->audio_source_uuid);
	bfree(context->status_text);
	bfree(context);
}

static void subtitle_source_video_tick(void *data, float seconds)
{
	struct subtitle_source *context = data;

	reconcile_capture(context, seconds);
	update_status(context, seconds);
}

static void subtitle_source_get_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, SETTING_AUDIO_SOURCE, "");
	obs_data_set_default_bool(settings, SETTING_ENABLED, false);
}

static bool add_audio_source_to_list(void *param, obs_source_t *source)
{
	obs_property_t *list = param;

	if (obs_source_get_output_flags(source) & OBS_SOURCE_AUDIO)
		obs_property_list_add_string(list, obs_source_get_name(source), obs_source_get_uuid(source));
	return true;
}

static obs_properties_t *subtitle_source_get_properties(void *data)
{
	UNUSED_PARAMETER(data);

	obs_properties_t *props = obs_properties_create();
	obs_property_t *list = obs_properties_add_list(props, SETTING_AUDIO_SOURCE, obs_module_text("Microphone"),
						       OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(list, obs_module_text("MicrophoneNone"), "");
	obs_enum_sources(add_audio_source_to_list, list);

	obs_properties_add_bool(props, SETTING_ENABLED, obs_module_text("Enabled"));
	return props;
}

static uint32_t subtitle_source_get_width(void *data)
{
	struct subtitle_source *context = data;
	return context->text ? obs_source_get_width(context->text) : 0;
}

static uint32_t subtitle_source_get_height(void *data)
{
	struct subtitle_source *context = data;
	return context->text ? obs_source_get_height(context->text) : 0;
}

static void subtitle_source_video_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);

	struct subtitle_source *context = data;
	if (context->text)
		obs_source_video_render(context->text);
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
