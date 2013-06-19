
#include "button.h"
#include "gui.h"
#include "gfx.h"

#include <stdlib.h>


#define BORDER_COLOR   COLOR(0x88, 0x88, 0x88)
#define BTN_DOWN_COLOR COLOR(0xCC, 0xCC, 0xCC)

#define BTN_FIRST_REPEAT_DELAY MS2ST(500)
#define BTN_REPEAT_DELAY       MS2ST(50)


typedef struct {
  bool is_down;
  const char* text;
  const Image_t* icon;
  uint16_t color;
  systime_t next_event_time;

  button_event_handler_t down_handler;
  button_event_handler_t repeat_handler;
  button_event_handler_t up_handler;
  button_event_handler_t click_handler;
} button_t;


static void button_touch(touch_event_t* event);
static void button_paint(paint_event_t* event);
static void button_destroy(widget_t* w);


static const widget_class_t button_widget_class = {
    .on_touch = button_touch,
    .on_paint = button_paint,
    .on_destroy = button_destroy,
};

widget_t*
button_create(widget_t* parent, rect_t rect, const char* text, const Image_t* icon, uint16_t color,
    button_event_handler_t down_handler,
    button_event_handler_t repeat_handler,
    button_event_handler_t up_handler,
    button_event_handler_t click_handler)
{
  button_t* b = calloc(1, sizeof(button_t));

  b->text = text;
  b->icon = icon;
  b->color = color;
  b->down_handler = down_handler;
  b->repeat_handler = repeat_handler;
  b->up_handler = up_handler;
  b->click_handler = click_handler;

  widget_t* w = widget_create(parent, &button_widget_class, b, rect);
  widget_set_background(w, color, FALSE);

  return w;
}

static void
button_destroy(widget_t* w)
{
  button_t* b = widget_get_instance_data(w);
  free(b);
}

static void
button_touch(touch_event_t* event)
{
  button_t* b = widget_get_instance_data(event->widget);

  button_event_t be = {
      .widget = event->widget,
      .pos = event->pos,
  };

  if (event->id == EVT_TOUCH_DOWN) {
    if (!b->is_down) {
      b->is_down = true;
      gui_acquire_touch_capture(event->widget);
      widget_set_background(event->widget, DARK_GRAY, FALSE);

      if (b->down_handler) {
        be.id = EVT_BUTTON_DOWN;
        b->down_handler(&be);
      }
      b->next_event_time = chTimeNow() + BTN_FIRST_REPEAT_DELAY;
    }
    else if (chTimeNow() > b->next_event_time) {
      if (b->repeat_handler) {
        be.id = EVT_BUTTON_REPEAT;
        b->repeat_handler(&be);
      }
      b->next_event_time = chTimeNow() + BTN_REPEAT_DELAY;
    }
  }
  else {
    if (b->is_down) {
      b->is_down = false;
      gui_release_touch_capture();
      widget_set_background(event->widget, b->color, FALSE);
      if (b->up_handler) {
        be.id = EVT_BUTTON_UP;
        b->up_handler(&be);
      }
//      if (rect_inside(widget_get_rect(event->widget), event->pos)) {
        if (b->click_handler) {
          be.id = EVT_BUTTON_CLICK;
          b->click_handler(&be);
        }
//      }
    }
  }
}

static void
button_paint(paint_event_t* event)
{
  button_t* b = widget_get_instance_data(event->widget);

  rect_t rect = widget_get_rect(event->widget);
  point_t center = rect_center(rect);

  /* draw text */
//  if (b->text != NULL) {
//    Extents_t text_extents = font_text_extents(font_terminal, b->text);
//    print(b->text,
//        center.x - (text_extents.width / 2),
//        center.y - (text_extents.height / 2));
//  }

  /* draw icon */
  if (b->icon != NULL) {
    gfx_draw_bitmap(
        center.x - (b->icon->width / 2),
        center.y - (b->icon->height / 2),
        b->icon);
  }
}
