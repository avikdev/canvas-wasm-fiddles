#pragma once

#include "include/core/SkColor.h"
#include "include/core/SkRect.h"

class SkCanvas;

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

} // namespace graphics::canvas_legends
