#include "fiddles/skia_drawing_builder.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <utility>
#include <vector>

#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPathBuilder.h"
#include "utils/color_utils.h"

namespace {

using EyeColorPair = std::pair<SkColor4f, SkColor4f>;

const std::vector<EyeColorPair> &GetEyeColors() {
  static const std::vector<EyeColorPair> *kInstance = [] {
    auto *colors = new std::vector<EyeColorPair>();
    colors->reserve(64);
    constexpr std::array<float, 8> kBaseHues = {18.0F,  42.0F,  104.0F, 158.0F,
                                                191.0F, 224.0F, 277.0F, 329.0F};
    for (int index = 0; index < 64; ++index) {
      const float hue =
          kBaseHues[index % kBaseHues.size()] + (index / 8) * 5.5F;
      colors->emplace_back(color_utils::FromHsv(hue, 0.48F, 1.0F),
                           color_utils::FromHsv(hue, 0.58F, 0.74F));
    }
    return colors;
  }();
  return *kInstance;
}

} // namespace

void DrawSkiaDrawing(SkCanvas *canvas, int width, int height,
                     double time_seconds) {
  const float draw_width = static_cast<float>(width);
  const float draw_height = static_cast<float>(height);
  const float shortest = std::min(draw_width, draw_height);
  const float center_x = draw_width * 0.5F;
  const float center_y = draw_height * 0.5F;
  const float time = static_cast<float>(time_seconds);
  const auto &eye_colors = GetEyeColors();

  canvas->clear(SkColorSetRGB(7, 10, 22));

  SkPaint paint;
  paint.setAntiAlias(true);
  constexpr int kHaloCount = 10;
  for (int halo = 0; halo < kHaloCount; ++halo) {
    const float ratio =
        static_cast<float>(halo + 1) / static_cast<float>(kHaloCount);
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(0.8F + ratio * 1.4F);
    paint.setColor4f(color_utils::FromHsv(190.0F + ratio * 125.0F, 0.65F, 0.85F,
                                          0.08F + ratio * 0.18F),
                     nullptr);
    canvas->drawCircle(center_x, center_y,
                       shortest * (0.055F + ratio * 0.40F) +
                           std::sin(time * 0.55F + static_cast<float>(halo)) *
                               5.0F,
                       paint);
  }

  constexpr int kGroupCount = 8;
  constexpr int kTentaclesPerGroup = 8;
  for (int group = 0; group < kGroupCount; ++group) {
    const float group_angle =
        static_cast<float>(group) * 2.0F * std::numbers::pi_v<float> /
            static_cast<float>(kGroupCount) +
        time * (0.075F + static_cast<float>(group % 3) * 0.008F);
    for (int tentacle = 0; tentacle < kTentaclesPerGroup; ++tentacle) {
      const int index = group * kTentaclesPerGroup + tentacle;
      const float spread = (static_cast<float>(tentacle) - 3.5F) * 0.045F;
      const float angle = group_angle + spread +
                          std::sin(time * 1.18F + index * 0.47F) * 0.085F;
      const float inner_radius =
          shortest * (0.105F + static_cast<float>(tentacle % 3) * 0.009F);
      const float outer_radius =
          shortest *
          (0.275F + static_cast<float>(tentacle) * 0.020F +
           std::sin(time * (0.55F + group * 0.025F) + index * 0.61F) * 0.045F);
      const float inner_x = center_x + std::cos(angle) * inner_radius;
      const float inner_y = center_y + std::sin(angle) * inner_radius;
      const float tip_angle =
          angle +
          std::sin(time * 0.92F + group * 1.13F + tentacle * 0.39F) * 0.24F;
      const float tip_x = center_x + std::cos(tip_angle) * outer_radius;
      const float tip_y = center_y + std::sin(tip_angle) * outer_radius;
      const float normal_x = -std::sin(angle);
      const float normal_y = std::cos(angle);
      const float sway =
          shortest * (0.06F + tentacle * 0.005F) *
          std::sin(time * 1.35F + group * 0.83F + tentacle * 0.71F);
      const float reverse_sway =
          shortest * 0.075F *
          std::cos(time * 1.07F + group * 0.61F - tentacle * 0.46F);

      SkPathBuilder path;
      path.moveTo(inner_x, inner_y);
      path.cubicTo(
          inner_x + std::cos(angle) * outer_radius * 0.25F + normal_x * sway,
          inner_y + std::sin(angle) * outer_radius * 0.25F + normal_y * sway,
          inner_x + std::cos(tip_angle) * outer_radius * 0.64F -
              normal_x * reverse_sway,
          inner_y + std::sin(tip_angle) * outer_radius * 0.64F -
              normal_y * reverse_sway,
          tip_x, tip_y);
      paint.setStyle(SkPaint::kStroke_Style);
      paint.setStrokeCap(SkPaint::kRound_Cap);
      paint.setStrokeWidth(1.8F + static_cast<float>(tentacle % 4) * 0.8F);
      SkColor4f line_color = eye_colors[index].second;
      line_color.fA = 0.92F;
      paint.setColor4f(line_color, nullptr);
      canvas->drawPath(path.detach(), paint);

      const float eye_diameter =
          20.0F + static_cast<float>((index * 11) % 21) * 2.0F;
      const float eye_radius = eye_diameter * 0.5F;
      paint.setStyle(SkPaint::kFill_Style);
      paint.setColor4f(eye_colors[index].first, nullptr);
      canvas->drawCircle(tip_x, tip_y, eye_radius, paint);
      paint.setColor4f(eye_colors[index].second, nullptr);
      canvas->drawCircle(tip_x + std::cos(tip_angle) * eye_radius * 0.10F,
                         tip_y + std::sin(tip_angle) * eye_radius * 0.10F,
                         std::max(2.0F, eye_radius * 0.38F), paint);
    }
  }

  constexpr int kCoreCircleCount = 6;
  constexpr std::array<int, kCoreCircleCount> kCoreColorIndices = {0,  11, 22,
                                                                   33, 44, 55};
  paint.setStyle(SkPaint::kFill_Style);
  for (int circle = 0; circle < kCoreCircleCount; ++circle) {
    const float base_radius =
        shortest * (0.092F - static_cast<float>(circle) * 0.0125F);
    const float primary_pulse =
        std::sin(time * (1.82F + circle * 0.035F) + circle * 0.58F) *
        (0.050F + circle * 0.008F);
    const float secondary_pulse =
        std::sin(time * 0.67F + circle * 1.31F) * 0.018F;
    const float radius = base_radius * (1.0F + primary_pulse + secondary_pulse);
    SkColor4f core_color = eye_colors[kCoreColorIndices[circle]].first;
    core_color.fA = 0.97F;
    paint.setColor4f(core_color, nullptr);
    canvas->drawCircle(center_x, center_y, radius, paint);
  }
}
