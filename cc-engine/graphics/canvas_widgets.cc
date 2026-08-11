#include "graphics/canvas_widgets.h"

#include <algorithm>
#include <cmath>
#include <string_view>

#include "include/core/SkCanvas.h"
#include "include/core/SkFont.h"
#include "include/core/SkPaint.h"
#include "include/core/SkRRect.h"

namespace graphics::canvas_widgets {
namespace {

void DrawLabel(SkCanvas *canvas, std::string_view text, float x, float center_y,
               bool align_right, const SkFont &font, SkColor color) {
  SkRect bounds;
  const float width = font.measureText(text.data(), text.size(),
                                       SkTextEncoding::kUTF8, &bounds);
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setColor(color);
  const float baseline = center_y - (bounds.top() + bounds.bottom()) * 0.5F;
  canvas->drawSimpleText(text.data(), text.size(), SkTextEncoding::kUTF8,
                         align_right ? x - width : x, baseline, font, paint);
}

} // namespace

TwoSidedSliderGeometry MakeTwoSidedSliderGeometry(const SkRect &track,
                                                  float value) {
  TwoSidedSliderGeometry geometry;
  if (!track.isFinite() || track.isEmpty()) {
    return geometry;
  }
  const float normalized =
      std::isfinite(value) ? std::clamp(value, -1.0F, 1.0F) : 0.0F;
  geometry.track = track;
  geometry.center_x = track.centerX();
  geometry.marker_x = geometry.center_x + normalized * track.width() * 0.5F;
  if (normalized < 0.0F) {
    geometry.negative_fill = SkRect::MakeLTRB(
        geometry.marker_x, track.top(), geometry.center_x, track.bottom());
  } else if (normalized > 0.0F) {
    geometry.positive_fill = SkRect::MakeLTRB(
        geometry.center_x, track.top(), geometry.marker_x, track.bottom());
  }
  return geometry;
}

SkRect MakeOneSidedSliderFill(const SkRect &track, float value) {
  if (!track.isFinite() || track.isEmpty()) {
    return SkRect::MakeEmpty();
  }
  const float normalized =
      std::isfinite(value) ? std::clamp(value, 0.0F, 1.0F) : 0.0F;
  return SkRect::MakeLTRB(track.left(), track.top(),
                          track.left() + track.width() * normalized,
                          track.bottom());
}

void DrawTwoSidedSlider(SkCanvas *canvas, const SkRect &bounds, float value,
                        const SkFont &label_font,
                        const TwoSidedSliderStyle &style) {
  if (canvas == nullptr || !bounds.isFinite() || bounds.isEmpty()) {
    return;
  }
  constexpr std::string_view negative_label = "-1";
  constexpr std::string_view positive_label = "+1";
  const float negative_width = label_font.measureText(
      negative_label.data(), negative_label.size(), SkTextEncoding::kUTF8);
  const float positive_width = label_font.measureText(
      positive_label.data(), positive_label.size(), SkTextEncoding::kUTF8);
  const float padding =
      std::clamp(style.outer_padding, 0.0F,
                 std::min(bounds.width(), bounds.height()) * 0.5F);
  const SkRect content = bounds.makeInset(padding, 0.0F);
  const float track_left = content.left() + negative_width + style.label_gap;
  const float track_right = content.right() - positive_width - style.label_gap;
  if (track_right <= track_left) {
    return;
  }
  const float track_height =
      std::clamp(style.track_height, 1.0F, bounds.height());
  const SkRect track =
      SkRect::MakeLTRB(track_left, bounds.centerY() - track_height * 0.5F,
                       track_right, bounds.centerY() + track_height * 0.5F);
  const TwoSidedSliderGeometry geometry =
      MakeTwoSidedSliderGeometry(track, value);
  const float radius =
      std::clamp(style.corner_radius, 0.0F, track.height() * 0.5F);

  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(style.background_color);
  canvas->drawRoundRect(bounds, bounds.height() * 0.5F, bounds.height() * 0.5F,
                        paint);
  paint.setColor(style.track_color);
  canvas->drawRRect(SkRRect::MakeRectXY(track, radius, radius), paint);

  canvas->save();
  canvas->clipRRect(SkRRect::MakeRectXY(track, radius, radius),
                    SkClipOp::kIntersect, true);
  if (!geometry.negative_fill.isEmpty()) {
    paint.setColor(style.negative_color);
    canvas->drawRect(geometry.negative_fill, paint);
  }
  if (!geometry.positive_fill.isEmpty()) {
    paint.setColor(style.positive_color);
    canvas->drawRect(geometry.positive_fill, paint);
  }
  canvas->restore();

  paint.setColor(style.center_mark_color);
  const float mark_width = std::max(0.5F, style.center_mark_width);
  canvas->drawRect(SkRect::MakeXYWH(geometry.center_x - mark_width * 0.5F,
                                    track.top(), mark_width, track.height()),
                   paint);

  DrawLabel(canvas, negative_label, content.left(), bounds.centerY(), false,
            label_font, style.label_color);
  DrawLabel(canvas, positive_label, content.right(), bounds.centerY(), true,
            label_font, style.label_color);
}

void DrawOneSidedSlider(SkCanvas *canvas, const SkRect &bounds, float value,
                        std::string_view label, const SkFont &label_font,
                        const OneSidedSliderStyle &style) {
  if (canvas == nullptr || !bounds.isFinite() || bounds.isEmpty()) {
    return;
  }
  const float padding =
      std::clamp(style.outer_padding, 0.0F,
                 std::min(bounds.width(), bounds.height()) * 0.5F);
  const SkRect track = bounds.makeInset(padding, padding);
  if (track.isEmpty()) {
    return;
  }
  const SkRect fill = MakeOneSidedSliderFill(track, value);
  const float radius =
      std::clamp(style.corner_radius, 0.0F, track.height() * 0.5F);

  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(style.background_color);
  canvas->drawRoundRect(bounds, bounds.height() * 0.5F, bounds.height() * 0.5F,
                        paint);
  paint.setColor(style.track_color);
  canvas->drawRRect(SkRRect::MakeRectXY(track, radius, radius), paint);
  canvas->save();
  canvas->clipRRect(SkRRect::MakeRectXY(track, radius, radius),
                    SkClipOp::kIntersect, true);
  paint.setColor(style.fill_color);
  canvas->drawRect(fill, paint);
  canvas->restore();

  SkRect text_bounds;
  const float text_width = label_font.measureText(
      label.data(), label.size(), SkTextEncoding::kUTF8, &text_bounds);
  const float baseline =
      bounds.centerY() - (text_bounds.top() + text_bounds.bottom()) * 0.5F;
  paint.setColor(style.label_color);
  canvas->drawSimpleText(label.data(), label.size(), SkTextEncoding::kUTF8,
                         bounds.centerX() - text_width * 0.5F, baseline,
                         label_font, paint);
}

} // namespace graphics::canvas_widgets
