#include <pebble.h>
#include "main.h"
#include "num2words.h"
#include "fctx.h"
#include "ffont.h"
#include "fpath.h"

static struct Globals {
	Window* window;
	Layer* layer;
	
  FFont* font;
  FPoint center;
  
  int hour;
  int minute;

  GRect bounds;
  
} g;

 static char s_hour_text[32];
 static char s_minute_text[32];
 static int trig_angle, trig_angle_end;

/* TAU = 2*PI * FIXED_POINT_SCALE * TRIG_MAX_ANGLE
   325 is the largest number you can multiply by TAU without overflow.
*/
static const int32_t TAU = 6588397;

int draw_string_radial(
    FContext* fctx,
    const char* text,
    FFont* font,
    int16_t font_size,
    FPoint center,
    uint16_t radius,
    uint32_t angle)
{
    fctx_set_text_size(fctx, font, font_size);
    fctx_set_offset(fctx, center);
    fctx_set_rotation(fctx, 0);

    fixed_t s = radius * TAU / TRIG_MAX_ANGLE;
    fixed_t t = s * fctx->transform_scale_from.x / fctx->transform_scale_to.x;
    fixed_t r = INT_TO_FIXED(radius) * fctx->transform_scale_from.x / fctx->transform_scale_to.x;
    fixed_t arc = 0;
    const char* p;
    fixed_t rotation = 0; // YG moved here so final angle can be returned

    for (p = text; *p; ++p) {
        char ch = *p;
        FGlyph* glyph = ffont_glyph_info(font, ch);
        if (glyph) {

            if (p != text) {
                arc += glyph->horiz_adv_x / 2;
            }
            rotation = angle + arc * TRIG_MAX_ANGLE / t;
            fctx_set_rotation(fctx, rotation);
            arc += glyph->horiz_adv_x / 2;

            FPoint advance;
            advance.x = glyph->horiz_adv_x / -2;
            advance.y = r;

            void* path_data = ffont_glyph_outline(font, glyph);
            fctx_draw_commands(fctx, advance, path_data, glyph->path_data_length);
        }
    }
  
  return rotation; //YG returning final angle
}


static void time_draw(Layer* layer, GContext* ctx) {
  
  if (clock_is_24h_style()) {
    hour_to_24h_word(g.hour, s_hour_text);
  } else {
    hour_to_12h_word(g.hour, s_hour_text);
  }  
  
  minute_to_common_words(g.minute, s_minute_text);

  FContext fctx;
  fctx_init_context(&fctx, ctx);
  fctx_set_color_bias(&fctx, 0);
  
  
  //drawing minute text at minute angle
  trig_angle = TRIG_MAX_ANGLE * g.minute / 60;
  
  fctx_begin_fill(&fctx);
  #ifdef PBL_COLOR
  fctx_set_fill_color(&fctx, GColorYellow); 
  #else
  fctx_set_fill_color(&fctx, GColorWhite); 
  #endif
  trig_angle_end = draw_string_radial(&fctx,
        s_minute_text,
        g.font, 35,
        g.center, 90 - 30, trig_angle);
  fctx_end_fill(&fctx);
 
   // drawing miute arc
  #ifdef PBL_COLOR
  graphics_context_set_fill_color(ctx, GColorYellow);
  graphics_fill_radial(ctx, GRect(g.bounds.origin.x + 12, g.bounds.origin.y + 12, g.bounds.size.w - 24, g.bounds.size.h - 24), GOvalScaleModeFitCircle,  19, trig_angle_end + DEG_TO_TRIGANGLE(7), trig_angle + TRIG_MAX_ANGLE - DEG_TO_TRIGANGLE(7));
  #endif
  
  
  //drawing hour hand at hour angle
  trig_angle = (TRIG_MAX_ANGLE * (((g.hour % 12) * 6) + (g.minute / 10))) / (12 * 6);
  
  fctx_begin_fill(&fctx);
  #ifdef PBL_COLOR
  fctx_set_fill_color(&fctx, GColorGreen); 
  #else
  fctx_set_fill_color(&fctx, GColorWhite); 
  #endif
  trig_angle_end = draw_string_radial(&fctx,
        s_hour_text,
        g.font, 35,
        g.center, 90 - 60, trig_angle);
  
  fctx_end_fill(&fctx); 
  
  // drawing hour arc
  #ifdef PBL_COLOR
  graphics_context_set_fill_color(ctx, GColorGreen);
  graphics_fill_radial(ctx, GRect(g.bounds.origin.x + 42, g.bounds.origin.y + 42, g.bounds.size.w - 84, g.bounds.size.h - 84), GOvalScaleModeFitCircle,  19, trig_angle_end + DEG_TO_TRIGANGLE(11), trig_angle + TRIG_MAX_ANGLE - DEG_TO_TRIGANGLE(11));
  #endif
  
  // setting battery color according to battery state
  #ifdef PBL_COLOR
  switch (battery_state_service_peek().charge_percent) {
       case 100: 
       case 90: 
       case 80: 
       case 70: 
       case 60: 
       case 50: fctx_set_fill_color(&fctx, GColorWhite); break;
       case 40: 
       case 30: 
       case 20: fctx_set_fill_color(&fctx, GColorOrange); break;
       case 10: 
       case 0:  fctx_set_fill_color(&fctx, GColorDarkCandyAppleRed); break;     
   }
  #else
  fctx_set_fill_color(&fctx, GColorWhite); 
  #endif
  
  
  // drawing the battery circle
  fctx_begin_fill(&fctx);
  fctx_plot_circle(&fctx, &g.center, 250);
  fctx_end_fill(&fctx); 
  
  fctx_deinit_context(&fctx);

}


// timer tick: getting current time and refreshing graphics layer
static void handle_minute_tick(struct tm* tick_time, TimeUnits units_changed) {
  
  g.hour = tick_time->tm_hour;
  g.minute = tick_time->tm_min;

  layer_mark_dirty(g.layer);  
  
}

static void main_window_load(Window *window) {
  
}

static void main_window_unload(Window *window) {
  
}

void battery_update(BatteryChargeState state) {
  layer_mark_dirty(g.layer);
}

static void init() {
  
  setlocale(LC_ALL, "");

  g.window = window_create();
  window_set_background_color(g.window, GColorBlack);
  
  Layer* window_layer = window_get_root_layer(g.window);
  g.bounds = layer_get_frame(window_layer);

  g.center.x = INT_TO_FIXED(g.bounds.size.w) / 2;
  g.center.y = INT_TO_FIXED(g.bounds.size.h) / 2;
  
  APP_LOG(APP_LOG_LEVEL_DEBUG, "free heep before font = %d", heap_bytes_free());
  g.font = ffont_create_from_resource(RESOURCE_ID_OSP_DIN_REDUCED_FFONT); //YG Using limited font to save on size
  APP_LOG(APP_LOG_LEVEL_DEBUG, "free heep after font = %d", heap_bytes_free());
  if (g.font)
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Font Is Loaded");
  else
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Font NOT Loaded");
  
  
  g.layer = layer_create(g.bounds);
  layer_set_update_proc(g.layer, &time_draw);
  
  layer_add_child(window_layer, g.layer);
  
  window_set_window_handlers(g.window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload,
  });
  window_stack_push(g.window, true);
  
  time_t now = time(NULL);
  struct tm *current_time = localtime(&now);
  handle_minute_tick(current_time, MINUTE_UNIT);
  tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);
  
  battery_state_service_subscribe(battery_update);
  
}

static void deinit() {
  layer_destroy(g.layer);
  window_destroy(g.window);
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}