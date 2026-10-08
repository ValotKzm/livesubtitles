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
#include <plugin-support.h>

/* Text is drawn by a private instance of the built-in OBS text source. */
#define TEXT_SOURCE_ID "text_gdiplus"
#define TEXT_FONT_FACE "Arial"
#define TEXT_FONT_SIZE 72

struct subtitle_source {
	/* Set once in create and released in destroy; never reassigned. */
	obs_source_t *text;
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
	obs_data_set_string(settings, "text", obs_module_text("TestText"));

	obs_source_t *text = obs_source_create_private(id, NULL, settings);
	if (!text)
		obs_log(LOG_ERROR, "failed to create text source '%s'", id);

	obs_data_release(font);
	obs_data_release(settings);
	return text;
}

static void *subtitle_source_create(obs_data_t *settings, obs_source_t *source)
{
	UNUSED_PARAMETER(settings);
	UNUSED_PARAMETER(source);

	struct subtitle_source *context = bzalloc(sizeof(struct subtitle_source));
	context->text = create_text_source();
	return context;
}

static void subtitle_source_destroy(void *data)
{
	struct subtitle_source *context = data;

	obs_source_release(context->text);
	bfree(context);
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
	.get_width = subtitle_source_get_width,
	.get_height = subtitle_source_get_height,
	.video_render = subtitle_source_video_render,
};
