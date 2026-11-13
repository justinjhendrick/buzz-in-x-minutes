#include <pebble.h>

#define BUFFER_LEN (40)

static Window* s_window;
static Layer* s_layer;
static time_t s_end;
static char s_buffer[BUFFER_LEN];
static GFont s_font_lg;
static GFont s_font_md;

static bool s_timer_expired = false;

static int s_up_button_seconds = 60;
static int s_select_button_seconds = 600;
static int s_down_button_seconds = -60;
static int s_reminder_seconds = 0;
static int s_initial_timer_seconds = 60;

// Vibe pattern in ms
// Alternates ON/OFF durations
// starting with ON
static const uint32_t segments[] = { 400, 300, 400, 300, 400 };
static const VibePattern vibe_pattern = {
  .durations = segments,
  .num_segments = ARRAY_LENGTH(segments),
};

static void end_time_add(int seconds_change) {
  if (s_timer_expired) return;
  s_end += seconds_change;
  if (s_layer) layer_mark_dirty(s_layer);
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  end_time_add(s_up_button_seconds);
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  end_time_add(s_select_button_seconds);
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  end_time_add(s_down_button_seconds);
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 375, up_click_handler);

  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_SELECT, 375, select_click_handler);

  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 375, down_click_handler);
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

static void trigger_exit() {
  exit_reason_set(APP_EXIT_ACTION_PERFORMED_SUCCESSFULLY);
  window_stack_pop_all(true);
}

static void reminder() {
  vibes_long_pulse();
  app_timer_register(s_reminder_seconds * 1000, reminder, NULL);
}

static void update_layer(Layer* layer, GContext* ctx) {
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  int date_height = 40;
  bounds.size.h -= date_height;

  time_t now = time(NULL);
  int remaining = s_end - now;
  int minutes = remaining / 60;
  int seconds = remaining % 60;
  if (remaining <= 0) {
    if (!s_timer_expired) {
      // First frame after the timer expired
      s_timer_expired = true;
      vibes_enqueue_custom_pattern(vibe_pattern);
      if (s_reminder_seconds <= 0) {
        app_timer_register(3000, trigger_exit, NULL);
      } else {
        app_timer_register(s_reminder_seconds * 1000, reminder, NULL);
      }
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
  if (s_layer) layer_mark_dirty(s_layer);
}

static void load_settings() {
  if (persist_exists(MESSAGE_KEY_initial_timer_seconds)) s_initial_timer_seconds = persist_read_int(MESSAGE_KEY_initial_timer_seconds);
  if (persist_exists(MESSAGE_KEY_up_button_seconds))     s_up_button_seconds     = persist_read_int(MESSAGE_KEY_up_button_seconds);
  if (persist_exists(MESSAGE_KEY_select_button_seconds)) s_select_button_seconds = persist_read_int(MESSAGE_KEY_select_button_seconds);
  if (persist_exists(MESSAGE_KEY_down_button_seconds))   s_down_button_seconds   = persist_read_int(MESSAGE_KEY_down_button_seconds);
  if (persist_exists(MESSAGE_KEY_reminder_seconds))      s_reminder_seconds      = persist_read_int(MESSAGE_KEY_reminder_seconds);
}

static void save_settings() {
  persist_write_int(MESSAGE_KEY_initial_timer_seconds, s_initial_timer_seconds);
  persist_write_int(MESSAGE_KEY_up_button_seconds,     s_up_button_seconds);
  persist_write_int(MESSAGE_KEY_select_button_seconds, s_select_button_seconds);
  persist_write_int(MESSAGE_KEY_down_button_seconds,   s_down_button_seconds);
  persist_write_int(MESSAGE_KEY_reminder_seconds,      s_reminder_seconds);
}

static void inbox_received_handler(DictionaryIterator *iter, void *context) {
  Tuple* t;
  if ((t = dict_find(iter, MESSAGE_KEY_initial_timer_seconds))) s_initial_timer_seconds = t->value->int32;
  if ((t = dict_find(iter, MESSAGE_KEY_up_button_seconds)))     s_up_button_seconds     = t->value->int32;
  if ((t = dict_find(iter, MESSAGE_KEY_select_button_seconds))) s_select_button_seconds = t->value->int32;
  if ((t = dict_find(iter, MESSAGE_KEY_down_button_seconds)))   s_down_button_seconds   = t->value->int32;
  if ((t = dict_find(iter, MESSAGE_KEY_reminder_seconds)))      s_reminder_seconds      = t->value->int32;
  save_settings();
  if (s_layer) layer_mark_dirty(s_layer);
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
  load_settings();
  s_end = time(NULL) + s_initial_timer_seconds;
  app_message_register_inbox_received(inbox_received_handler);
  app_message_open(app_message_inbox_size_maximum() / 2, app_message_outbox_size_maximum() / 2);

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
