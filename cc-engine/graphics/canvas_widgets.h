#pragma once

#include "include/core/SkColor.h"
#include "include/core/SkRect.h"

class SkCanvas;
class SkFont;

namespace graphics::canvas_widgets {

struct TwoSidedSliderStyle {
  SkColor background_color = SK_ColorBLACK;
  SkColor track_color = 0xff4b4b4b;
  SkColor negative_color = 0xffdf4d56;
  SkColor positive_color = 0xff3979dc;
  SkColor center_mark_color = 0xccffffff;
  SkColor label_color = SK_ColorWHITE;
  float track_height = 8.0F;
  float center_mark_width = 1.0F;
  float corner_radius = 4.0F;
  float label_gap = 7.0F;
  float outer_padding = 7.0F;
};

struct TwoSidedSliderGeometry {
  SkRect track = SkRect::MakeEmpty();
  SkRect negative_fill = SkRect::MakeEmpty();
  SkRect positive_fill = SkRect::MakeEmpty();
  float center_x = 0.0F;
  float marker_x = 0.0F;
};

// Computes slider geometry for a normalized value. Values are clamped to
// [-1, 1]; negative fill grows left from zero and positive fill grows right.
TwoSidedSliderGeometry MakeTwoSidedSliderGeometry(const SkRect &track,
                                                  float value);

// Draws a reusable negative/positive slider legend with a center separator and
// compact "-1" / "+1" range labels.
void DrawTwoSidedSlider(SkCanvas *canvas, const SkRect &bounds, float value,
                        const SkFont &label_font,
                        const TwoSidedSliderStyle &style = {});

} // namespace graphics::canvas_widgets
