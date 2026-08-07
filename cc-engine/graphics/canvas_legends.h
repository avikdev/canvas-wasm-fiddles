#pragma once

#include <cstddef>

#include "include/core/SkColor.h"
#include "include/core/SkRect.h"

class SkCanvas;
class SkFont;

namespace graphics::canvas_legends {

struct ProgressChipStyle {
  SkColor background_color = SK_ColorWHITE;
  SkColor border_color = 0x66000000;
  SkColor progress_color = SK_ColorBLACK;
  float border_width = 1.0F;
  float padding = 2.0F;
  float corner_radius = 5.0F;
};

// Returns the remaining-progress fill anchored to the track's right edge.
// As remaining_fraction falls, the fill's left edge recedes to the right.
SkRect RemainingFillRect(const SkRect &track, float remaining_fraction);

// Draws a compact reusable progress legend. Values are clamped to [0, 1].
void DrawProgressChip(SkCanvas *canvas, const SkRect &bounds,
                      float remaining_fraction,
                      const ProgressChipStyle &style = {});

struct TextChipStyle {
  SkColor background_color = 0x12000000;
  SkColor text_color = 0xff333333;
  SkColor border_color = 0x30000000;
  float horizontal_padding = 8.0F;
  float vertical_padding = 3.0F;
  float border_width = 1.0F;
  float corner_radius = 8.0F;
};

// Returns a chip rectangle centered on center_x and center_y.
SkRect MeasureTextChip(const char *text, std::size_t byte_length,
                       float center_x, float center_y, const SkFont &font,
                       const TextChipStyle &style = {});

void DrawTextChip(SkCanvas *canvas, const char *text, std::size_t byte_length,
                  float center_x, float center_y, const SkFont &font,
                  const TextChipStyle &style = {});

} // namespace graphics::canvas_legends
