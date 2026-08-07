#include "graphics/canvas_legends.h"

#include <algorithm>
#include <cmath>

#include "include/core/SkCanvas.h"
#include "include/core/SkFont.h"
#include "include/core/SkPaint.h"
#include "include/core/SkRRect.h"

namespace graphics::canvas_legends {

SkRect RemainingFillRect(const SkRect &track, float remaining_fraction) {
  const float remaining = std::isfinite(remaining_fraction)
                              ? std::clamp(remaining_fraction, 0.0F, 1.0F)
                              : 0.0F;
  return SkRect::MakeLTRB(track.right() - track.width() * remaining,
                          track.top(), track.right(), track.bottom());
}

void DrawProgressChip(SkCanvas *canvas, const SkRect &bounds,
                      float remaining_fraction,
                      const ProgressChipStyle &style) {
  if (canvas == nullptr || !bounds.isFinite() || bounds.isEmpty()) {
    return;
  }
  const float radius =
      std::clamp(style.corner_radius, 0.0F, bounds.height() * 0.5F);
  const SkRRect chip = SkRRect::MakeRectXY(bounds, radius, radius);

  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(style.background_color);
  canvas->drawRRect(chip, paint);

  const float inset =
      std::clamp(style.padding + style.border_width * 0.5F, 0.0F,
                 std::min(bounds.width(), bounds.height()) * 0.5F);
  SkRect track = bounds.makeInset(inset, inset);
  if (!track.isEmpty()) {
    const float track_radius = std::max(0.0F, radius - inset);
    const SkRRect track_clip =
        SkRRect::MakeRectXY(track, track_radius, track_radius);
    canvas->save();
    canvas->clipRRect(track_clip, SkClipOp::kIntersect, true);
    paint.setColor(style.progress_color);
    canvas->drawRect(RemainingFillRect(track, remaining_fraction), paint);
    canvas->restore();
  }

  if (style.border_width > 0.0F) {
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(style.border_width);
    paint.setColor(style.border_color);
    canvas->drawRRect(chip, paint);
  }
}

SkRect MeasureTextChip(const char *text, std::size_t byte_length,
                       float center_x, float center_y, const SkFont &font,
                       const TextChipStyle &style) {
  if (text == nullptr || byte_length == 0U) {
    return SkRect::MakeEmpty();
  }
  SkRect text_bounds;
  const float text_width =
      font.measureText(text, byte_length, SkTextEncoding::kUTF8, &text_bounds);
  const float width = text_width + style.horizontal_padding * 2.0F;
  const float height = text_bounds.height() + style.vertical_padding * 2.0F;
  return SkRect::MakeXYWH(center_x - width * 0.5F, center_y - height * 0.5F,
                          width, height);
}

void DrawTextChip(SkCanvas *canvas, const char *text, std::size_t byte_length,
                  float center_x, float center_y, const SkFont &font,
                  const TextChipStyle &style) {
  if (canvas == nullptr) {
    return;
  }
  const SkRect chip =
      MeasureTextChip(text, byte_length, center_x, center_y, font, style);
  if (chip.isEmpty()) {
    return;
  }
  const float radius =
      std::clamp(style.corner_radius, 0.0F, chip.height() * 0.5F);
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(style.background_color);
  canvas->drawRoundRect(chip, radius, radius, paint);
  if (style.border_width > 0.0F) {
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(style.border_width);
    paint.setColor(style.border_color);
    canvas->drawRoundRect(chip, radius, radius, paint);
  }

  SkRect text_bounds;
  const float text_width =
      font.measureText(text, byte_length, SkTextEncoding::kUTF8, &text_bounds);
  const float baseline =
      chip.centerY() - (text_bounds.top() + text_bounds.bottom()) * 0.5F;
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(style.text_color);
  canvas->drawSimpleText(text, byte_length, SkTextEncoding::kUTF8,
                         chip.centerX() - text_width * 0.5F, baseline, font,
                         paint);
}

} // namespace graphics::canvas_legends
