#ifndef ICON_H
#define ICON_H

#include "widget.h"
#include "image.h"


widget_t*
icon_create(widget_t* parent, rect_t rect, const Image_t* icon, color_t icon_color, color_t bg_color);

void
icon_set_image(widget_t* w, const Image_t* image);

void
icon_set_color(widget_t* w, color_t icon_color);

void
icon_set_disabled_color(widget_t* w, color_t icon_color);

#endif
