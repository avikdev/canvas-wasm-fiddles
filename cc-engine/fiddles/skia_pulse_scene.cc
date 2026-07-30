#include "fiddles/skia_pulse_scene.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkRect.h"

namespace {

SkColor ColorFromHsv(float hue, float saturation, float value,
                     std::uint8_t alpha = 255) {
  hue = std::fmod(hue, 360.0F);
  if (hue < 0.0F) {
    hue += 360.0F;
  }

  const float chroma = value * saturation;
  const float sector = hue / 60.0F;
  const float secondary =
      chroma * (1.0F - std::abs(std::fmod(sector, 2.0F) - 1.0F));
  float red = 0.0F;
  float green = 0.0F;
  float blue = 0.0F;

  if (sector < 1.0F) {
    red = chroma;
    green = secondary;
  } else if (sector < 2.0F) {
    red = secondary;
    green = chroma;
  } else if (sector < 3.0F) {
    green = chroma;
    blue = secondary;
  } else if (sector < 4.0F) {
    green = secondary;
    blue = chroma;
  } else if (sector < 5.0F) {
    red = secondary;
    blue = chroma;
  } else {
    red = chroma;
    blue = secondary;
  }

  const float match = value - chroma;
  const auto channel = [match](float component) {
    return static_cast<std::uint8_t>(
        std::round(std::clamp(component + match, 0.0F, 1.0F) * 255.0F));
  };
  return SkColorSetARGB(alpha, channel(red), channel(green), channel(blue));
}

} // namespace

void DrawSkiaPulseScene(SkCanvas *canvas, int width, int height,
                        double time_seconds) {
  const float draw_width = static_cast<float>(width);
  const float draw_height = static_cast<float>(height);
  const float shortest = std::min(draw_width, draw_height);
  const float center_x = draw_width * 0.5F;
  const float center_y = draw_height * 0.5F;
  const float time = static_cast<float>(time_seconds);

  canvas->clear(SkColorSetRGB(7, 10, 22));

  SkPaint paint;
  paint.setAntiAlias(true);

  constexpr int kHaloCount = 14;
  for (int halo = 0; halo < kHaloCount; ++halo) {
    const float ratio =
        static_cast<float>(halo + 1) / static_cast<float>(kHaloCount);
    const float wobble =
        std::sin(time * 0.8F + static_cast<float>(halo) * 0.72F) * 8.0F;
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(1.0F + ratio * 2.5F);
    paint.setColor(
        ColorFromHsv(176.0F + ratio * 170.0F + time * 14.0F, 0.72F, 0.92F,
                     static_cast<std::uint8_t>(28.0F + ratio * 82.0F)));
    canvas->drawCircle(center_x, center_y,
                       shortest * (0.04F + ratio * 0.43F) + wobble, paint);
  }

  constexpr int kRayCount = 24;
  for (int ray = 0; ray < kRayCount; ++ray) {
    const float offset = static_cast<float>(ray) * 2.0F *
                         std::numbers::pi_v<float> /
                         static_cast<float>(kRayCount);
    const float angle = offset + time * (0.18F + (ray % 3) * 0.025F);
    const float inner_radius = shortest * 0.13F;
    const float outer_radius =
        shortest * (0.34F + 0.08F * std::sin(time * 0.62F + ray * 0.5F));
    const float inner_x = center_x + std::cos(angle) * inner_radius;
    const float inner_y = center_y + std::sin(angle) * inner_radius;
    const float outer_x = center_x + std::cos(angle) * outer_radius;
    const float outer_y = center_y + std::sin(angle) * outer_radius;

    SkPathBuilder path;
    path.moveTo(inner_x, inner_y);
    const float bend = std::sin(time + ray * 0.8F) * shortest * 0.045F;
    path.quadTo((inner_x + outer_x) * 0.5F - std::sin(angle) * bend,
                (inner_y + outer_y) * 0.5F + std::cos(angle) * bend, outer_x,
                outer_y);
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeCap(SkPaint::kRound_Cap);
    paint.setStrokeWidth(2.0F + static_cast<float>(ray % 4));
    paint.setColor(
        ColorFromHsv(188.0F + ray * 12.0F + time * 18.0F, 0.78F, 1.0F, 205));
    canvas->drawPath(path.detach(), paint);

    const float node_radius = 4.0F + 3.0F * std::sin(time * 1.7F + ray * 0.65F);
    paint.setStyle(SkPaint::kFill_Style);
    paint.setColor(
        ColorFromHsv(40.0F + ray * 14.0F + time * 25.0F, 0.62F, 1.0F, 245));
    canvas->drawCircle(outer_x, outer_y, std::max(2.0F, node_radius), paint);
  }

  const float pulse = 1.0F + std::sin(time * 2.0F) * 0.08F;
  const SkRect core = SkRect::MakeXYWH(
      center_x - shortest * 0.09F * pulse, center_y - shortest * 0.09F * pulse,
      shortest * 0.18F * pulse, shortest * 0.18F * pulse);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(ColorFromHsv(214.0F + time * 22.0F, 0.54F, 0.86F, 235));
  canvas->drawRoundRect(core, shortest * 0.035F, shortest * 0.035F, paint);
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeWidth(3.0F);
  paint.setColor(SkColorSetARGB(235, 246, 243, 234));
  canvas->drawRoundRect(core, shortest * 0.035F, shortest * 0.035F, paint);
}
