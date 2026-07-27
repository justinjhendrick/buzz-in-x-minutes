#include <pebble.h>

#define BUFFER_LEN (40)

static Window* s_window;
static Layer* s_layer;
static time_t s_end;
static char s_buffer[BUFFER_LEN];
static GFont s_font_lg;
static GFont s_font_md;

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  s_end += 60;
  if (s_layer) { layer_mark_dirty(s_layer); }
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  s_end -= 60;
  if (s_layer) { layer_mark_dirty(s_layer); }
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}

static void draw_text(GContext* ctx, const char* buf, GFont font, GRect bbox, int shift_up) {
  GRect fixed = bbox;
  fixed.origin.y -= shift_up;
  graphics_draw_text(ctx, buf, font, fixed, GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

static void draw_date(GContext* ctx, GRect bbox, time_t now) {
  struct tm* local = localtime(&now);
  if (clock_is_24h_style()) {
    strftime(s_buffer, BUFFER_LEN, "%H:%M", local);
  } else {
    strftime(s_buffer, BUFFER_LEN, "%l:%M%P", local);
  }
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, bbox, 0, GCornerNone);
  graphics_context_set_text_color(ctx, GColorBlack);
  draw_text(ctx, s_buffer, s_font_md, bbox, 6);
}

static void update_layer(Layer* layer, GContext* ctx) {
  GRect bounds = layer_get_bounds(layer);
  int date_height = 40;
  bounds.size.h -= date_height;

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  graphics_context_set_text_color(ctx, GColorWhite);

  time_t now = time(NULL);
  int remaining = s_end - now;
  if (remaining <= 0) {
    exit_reason_set(APP_EXIT_ACTION_PERFORMED_SUCCESSFULLY);
    window_stack_pop_all(false);
  }
  int minutes = remaining / 60;
  int seconds = remaining % 60;
  snprintf(s_buffer, BUFFER_LEN, "%02d\n%02d", minutes, seconds);
  draw_text(ctx, s_buffer, s_font_lg, bounds, 10);

  GRect date_bbox = GRect(bounds.origin.x, bounds.origin.y + bounds.size.h, bounds.size.w, date_height);
  draw_date(ctx, date_bbox, now);
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
  window_stack_push(s_window, false);
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
