#include <pebble.h>

#define BUFFER_LEN (40)
#define ENABLE_BUTTON_HINTS (false)

static Window* s_window;
static Layer* s_layer;
static time_t s_end;
static char s_buffer[BUFFER_LEN];
static GFont s_font_lg;
static GFont s_font_md;
static time_t s_vibe_start_time;

// Vibe pattern in ms
// Alternates ON/OFF durations
// starting with ON
static const uint32_t segments[] = { 400, 300, 400, 300, 400 };
static const VibePattern vibe_pattern = {
  .durations = segments,
  .num_segments = ARRAY_LENGTH(segments),
};

static void end_time_add(int seconds_change) {
  if (s_vibe_start_time != 0) {
    return;
  }
  s_end += seconds_change;
  if (s_layer) { layer_mark_dirty(s_layer); }
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  end_time_add(600);
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  end_time_add(60);
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  end_time_add(-60);
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_SELECT, 500, select_click_handler);

  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 250, up_click_handler);

  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 250, down_click_handler);
}

static void draw_text(GContext* ctx, const char* buf, GFont font, GRect bbox, int shift_up, GTextAlignment align) {
  GRect fixed = bbox;
  fixed.origin.y -= shift_up;
  graphics_draw_text(ctx, buf, font, fixed, GTextOverflowModeFill, align, NULL);
}

static void draw_time_of_day(GContext* ctx, GRect bbox, time_t now) {
  struct tm* local = localtime(&now);
  if (clock_is_24h_style()) {
    strftime(s_buffer, BUFFER_LEN, "%H:%M", local);
  } else {
    strftime(s_buffer, BUFFER_LEN, "%l:%M%P", local);
  }
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, bbox, 0, GCornerNone);
  graphics_context_set_text_color(ctx, GColorBlack);
  draw_text(ctx, s_buffer, s_font_md, bbox, 6, GTextAlignmentCenter);
}

static void draw_button_hints(GContext* ctx, GRect bounds) {
  int border = 2;
  GRect bbox;
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
  graphics_context_set_text_color(ctx, GColorWhite);
  GSize sz = GSize(60, 30);
  bbox.size = sz;

  bbox.origin = GPoint(bounds.origin.x + border, bounds.origin.y + 25);
  draw_text(ctx, "X", font, bbox, 4, GTextAlignmentLeft);

  bbox.origin = GPoint(bounds.origin.x + bounds.size.w - sz.w - border, bounds.origin.y + 25);
  draw_text(ctx, "+1", font, bbox, 4, GTextAlignmentRight);

  bbox.origin = GPoint(bounds.origin.x + bounds.size.w - sz.w - border, bounds.origin.y + bounds.size.h / 2 - sz.h / 2);
  draw_text(ctx, "+10", font, bbox, 4, GTextAlignmentRight);
}

static void update_layer(Layer* layer, GContext* ctx) {
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  if (ENABLE_BUTTON_HINTS) {
    draw_button_hints(ctx, bounds);
  }

  int date_height = 40;
  bounds.size.h -= date_height;

  time_t now = time(NULL);
  int remaining = s_end - now;
  int minutes = remaining / 60;
  int seconds = remaining % 60;
  if (remaining <= 0) {
    if (s_vibe_start_time == 0) {
      s_vibe_start_time = now;
      vibes_enqueue_custom_pattern(vibe_pattern);
    } else if (now >= s_vibe_start_time + 3) {
      exit_reason_set(APP_EXIT_ACTION_PERFORMED_SUCCESSFULLY);
      window_stack_pop_all(true);
    }
    minutes = 0;
    seconds = 0;
  }

  graphics_context_set_text_color(ctx, GColorWhite);
  snprintf(s_buffer, BUFFER_LEN, "%02d\n%02d", minutes, seconds);
  draw_text(ctx, s_buffer, s_font_lg, bounds, 10, GTextAlignmentCenter);

  GRect date_bbox = GRect(bounds.origin.x, bounds.origin.y + bounds.size.h, bounds.size.w, date_height);
  draw_time_of_day(ctx, date_bbox, now);
}

static void tick_handler(struct tm* now, TimeUnits units_changed) {
  layer_mark_dirty(s_layer);
}

static void window_load(Window* window) {
  Layer* window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  s_layer = layer_create(bounds);
  layer_set_update_proc(s_layer, update_layer);
  layer_add_child(window_layer, s_layer);
  tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
}

static void window_unload(Window *window) {
  tick_timer_service_unsubscribe();
  layer_destroy(s_layer);
  s_layer = NULL;
}

static void init(void) {
  s_end = time(NULL) + 60;
  s_window = window_create();
  s_font_lg = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_CHIVO_MONO_90));
  s_font_md = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_CHIVO_MONO_40));
  window_set_click_config_provider(s_window, click_config_provider);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
}

static void deinit(void) {
  window_destroy(s_window);
  if (s_font_lg) { fonts_unload_custom_font(s_font_lg); }
  if (s_font_md) { fonts_unload_custom_font(s_font_md); }
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
