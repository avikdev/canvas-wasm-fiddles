#include "graphics/canvas_legends.h"

#include <algorithm>
#include <cmath>

#include "include/core/SkCanvas.h"
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

} // namespace graphics::canvas_legends
