#include "fiddles/envelope_distort_fiddle.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <numbers>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "geometry/bicubic_bezier_patch.h"
#include "graphics/webgl_canvas_context.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkFontStyle.h"
#include "include/core/SkMatrix.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkPathMeasure.h"
#include "include/core/SkRect.h"
#include "include/core/SkString.h"
#include "include/core/SkSurface.h"
#include "modules/skparagraph/include/FontCollection.h"
#include "modules/skparagraph/include/Paragraph.h"
#include "modules/skparagraph/include/ParagraphBuilder.h"
#include "modules/skparagraph/include/ParagraphStyle.h"
#include "modules/skparagraph/include/TextStyle.h"
#include "modules/skunicode/include/SkUnicode_icu.h"
#include "text/font_cycle.h"
#include "text/skia_font_manager.h"
#include "utils/perlin_noise.h"

namespace {

using skia::textlayout::Paragraph;
using skia::textlayout::ParagraphBuilder;
using skia::textlayout::ParagraphStyle;
using skia::textlayout::TextAlign;
using skia::textlayout::TextDirection;
using skia::textlayout::TextStyle;

constexpr std::array<const char *, 7> kWords = {
    "PHOENIX", "PEGASUS", "SPHINX", "DRAGON", "CHIMERA", "HYDRA", "MANTICORE",
};
constexpr int kEnvelopeKindCount = 12;
constexpr int kDesktopColumnCount = 4;
constexpr int kCompactColumnCount = 3;
constexpr float kCompactCanvasWidth = 760.0F;
constexpr float kCellPaddingRatio = 0.085F;
constexpr float kEnvelopeAspectRatio = 5.0F / 2.0F;
constexpr float kAnimationRate = 1.5F;
constexpr float kNonSineAnimationMultiplier = 1.65F;
constexpr float kTextSourceSize = 180.0F;
constexpr float kTextSampleSpacing = 2.0F;
constexpr float kMinimumEnvelopeSideHeight = 10.0F;
constexpr int kSamplesPerPatch = 18;
constexpr double kWordCycleSeconds = 6.4;
constexpr SkColor kCanvasColor = SK_ColorWHITE;
constexpr std::array<SkColor4f, 2> kEnvelopeColors = {{
    {0.60F, 0.84F, 0.98F, 0.20F},
    {0.98F, 0.72F, 0.84F, 0.20F},
}};
constexpr SkColor4f kAxisColor = {0.0F, 0.0F, 0.0F, 0.20F};
constexpr SkColor4f kCellBorderColor = {0.42F, 0.42F, 0.42F, 0.40F};
constexpr SkColor4f kLegendChipColor = {0.86F, 0.86F, 0.86F, 0.92F};

enum class EnvelopeKind {
  kBlob,
  kSemicircle,
  kSineAxisParallel,
  kSineTangent,
  kRotatingRectangle,
  kArcAxisParallel,
  kArcTangent,
  kFlag,
  kHourglass,
  kPot,
  kHeart,
  kFlame,
};

std::string_view EnvelopeLabel(EnvelopeKind kind) {
  switch (kind) {
  case EnvelopeKind::kBlob:
    return "globe";
  case EnvelopeKind::kSemicircle:
    return "semicircle";
  case EnvelopeKind::kSineAxisParallel:
    return "sinusoid";
  case EnvelopeKind::kSineTangent:
    return "sine (tangent)";
  case EnvelopeKind::kRotatingRectangle:
    return "rotating rectangle";
  case EnvelopeKind::kArcAxisParallel:
    return "arc (axis parallel)";
  case EnvelopeKind::kArcTangent:
    return "arc (tangent)";
  case EnvelopeKind::kFlag:
    return "flag";
  case EnvelopeKind::kHourglass:
    return "hourglass";
  case EnvelopeKind::kPot:
    return "pot";
  case EnvelopeKind::kHeart:
    return "heart";
  case EnvelopeKind::kFlame:
    return "flame";
  }
  return "envelope";
}

bool IsSineEnvelope(EnvelopeKind kind) {
  return kind == EnvelopeKind::kSineAxisParallel ||
         kind == EnvelopeKind::kSineTangent;
}

struct EnvelopeSurface {
  std::vector<geometry::BicubicBezierPatch> patches;
  // Most multi-patch envelopes are split uniformly along u. The rotating
  // rectangle is split at its projected vertex heights instead, so its four
  // corners remain sharp while horizontal text rows stay axis-aligned.
  std::vector<float> v_patch_boundaries;

  SkPoint Evaluate(float u, float v) const {
    if (patches.empty()) {
      return {0.0F, 0.0F};
    }
    const float clamped_u = std::clamp(u, 0.0F, 1.0F);
    const float clamped_v = std::clamp(v, 0.0F, 1.0F);
    if (v_patch_boundaries.size() == patches.size() + 1U) {
      const auto upper = std::upper_bound(v_patch_boundaries.begin() + 1,
                                          v_patch_boundaries.end(), clamped_v);
      const std::size_t patch_index =
          clamped_v >= 1.0F
              ? patches.size() - 1U
              : std::min(static_cast<std::size_t>(
                             upper - v_patch_boundaries.begin() - 1),
                         patches.size() - 1U);
      const float start = v_patch_boundaries[patch_index];
      const float end = v_patch_boundaries[patch_index + 1U];
      const float local_v = std::clamp(
          (clamped_v - start) / std::max(0.0001F, end - start), 0.0F, 1.0F);
      return patches[patch_index].Evaluate(clamped_u, local_v);
    }

    const float scaled_u = clamped_u * static_cast<float>(patches.size());
    const std::size_t patch_index =
        clamped_u >= 1.0F
            ? patches.size() - 1U
            : std::min(static_cast<std::size_t>(scaled_u), patches.size() - 1U);
    const float local_u =
        clamped_u >= 1.0F ? 1.0F : scaled_u - static_cast<float>(patch_index);
    return patches[patch_index].Evaluate(local_u, clamped_v);
  }

  int SampleCount() const {
    return std::max(kSamplesPerPatch,
                    static_cast<int>(patches.size()) * kSamplesPerPatch);
  }
};

struct SpanRow {
  float center_x_offset = 0.0F;
  float width_factor = 1.0F;
  float y_offset = 0.0F;
};

float HashUnit(std::uint32_t value) {
  value ^= value >> 16;
  value *= 0x7feb352dU;
  value ^= value >> 15;
  value *= 0x846ca68bU;
  value ^= value >> 16;
  return static_cast<float>(value & 0x00ffffffU) /
         static_cast<float>(0x01000000U);
}

template <typename Curve>
SkPoint CurveDerivative(const Curve &curve, float parameter) {
  constexpr float kDifference = 0.001F;
  const float before = std::max(0.0F, parameter - kDifference);
  const float after = std::min(1.0F, parameter + kDifference);
  if (after <= before) {
    return {0.0F, 0.0F};
  }
  const SkPoint start = curve(before);
  const SkPoint end = curve(after);
  const float inverse_span = 1.0F / (after - before);
  return {
      (end.fX - start.fX) * inverse_span,
      (end.fY - start.fY) * inverse_span,
  };
}

template <typename TopCurve, typename BottomCurve>
EnvelopeSurface MakeCurveSurface(int segment_count, const TopCurve &top_curve,
                                 const BottomCurve &bottom_curve) {
  EnvelopeSurface surface;
  segment_count = std::max(1, segment_count);
  surface.patches.reserve(segment_count);
  for (int segment = 0; segment < segment_count; ++segment) {
    const float start_u =
        static_cast<float>(segment) / static_cast<float>(segment_count);
    const float end_u =
        static_cast<float>(segment + 1) / static_cast<float>(segment_count);
    const float span = end_u - start_u;
    const SkPoint top_start = top_curve(start_u);
    const SkPoint top_end = top_curve(end_u);
    const SkPoint bottom_start = bottom_curve(start_u);
    const SkPoint bottom_end = bottom_curve(end_u);
    const SkPoint top_start_derivative = CurveDerivative(top_curve, start_u);
    const SkPoint top_end_derivative = CurveDerivative(top_curve, end_u);
    const SkPoint bottom_start_derivative =
        CurveDerivative(bottom_curve, start_u);
    const SkPoint bottom_end_derivative = CurveDerivative(bottom_curve, end_u);
    const std::array<SkPoint, 4> top_controls = {{
        top_start,
        {top_start.fX + top_start_derivative.fX * span / 3.0F,
         top_start.fY + top_start_derivative.fY * span / 3.0F},
        {top_end.fX - top_end_derivative.fX * span / 3.0F,
         top_end.fY - top_end_derivative.fY * span / 3.0F},
        top_end,
    }};
    const std::array<SkPoint, 4> bottom_controls = {{
        bottom_start,
        {bottom_start.fX + bottom_start_derivative.fX * span / 3.0F,
         bottom_start.fY + bottom_start_derivative.fY * span / 3.0F},
        {bottom_end.fX - bottom_end_derivative.fX * span / 3.0F,
         bottom_end.fY - bottom_end_derivative.fY * span / 3.0F},
        bottom_end,
    }};

    geometry::BicubicBezierPatch::ControlPoints points;
    for (int row = 0; row < 4; ++row) {
      const float v = static_cast<float>(row) / 3.0F;
      for (int column = 0; column < 4; ++column) {
        points[row * 4 + column] = {
            std::lerp(top_controls[column].fX, bottom_controls[column].fX, v),
            std::lerp(top_controls[column].fY, bottom_controls[column].fY, v),
        };
      }
    }
    surface.patches.emplace_back(points);
  }
  return surface;
}

template <typename RowFunction>
EnvelopeSurface MakeSpanSurface(const SkRect &rect,
                                const RowFunction &row_function) {
  geometry::BicubicBezierPatch::ControlPoints points;
  const float half_width = rect.width() * 0.5F;
  for (int row = 0; row < 4; ++row) {
    const float v = static_cast<float>(row) / 3.0F;
    const SpanRow span = row_function(v);
    for (int column = 0; column < 4; ++column) {
      const float u = static_cast<float>(column) / 3.0F;
      const float sx = u * 2.0F - 1.0F;
      points[row * 4 + column] = {
          rect.centerX() + span.center_x_offset +
              sx * half_width * std::max(0.05F, span.width_factor),
          std::lerp(rect.top(), rect.bottom(), v) + span.y_offset,
      };
    }
  }
  EnvelopeSurface surface;
  surface.patches.emplace_back(points);
  return surface;
}

EnvelopeSurface MakeRotatingRectangleSurface(const SkRect &rect,
                                             float time_seconds) {
  constexpr int kVertexCount = 4;
  constexpr float kVertexMergeTolerance = 0.0001F;
  constexpr float kAspectRatio = 2.0F;
  const float original_width =
      std::min(rect.width(), rect.height() * kAspectRatio);
  const float original_height = original_width / kAspectRatio;
  const float angle = time_seconds * 0.22F;
  const float swap_amount = std::pow(std::sin(angle), 2.0F);
  const float half_width =
      std::lerp(original_width, original_height, swap_amount) * 0.5F;
  const float half_height =
      std::lerp(original_height, original_width, swap_amount) * 0.5F;
  const float cosine = std::cos(angle);
  const float sine = std::sin(angle);
  const std::array<SkPoint, kVertexCount> local_vertices = {{
      {-half_width, -half_height},
      {half_width, -half_height},
      {half_width, half_height},
      {-half_width, half_height},
  }};
  std::array<SkPoint, kVertexCount> vertices;
  for (int index = 0; index < kVertexCount; ++index) {
    const SkPoint local = local_vertices[index];
    vertices[index] = {
        rect.centerX() + local.fX * cosine - local.fY * sine,
        rect.centerY() + local.fX * sine + local.fY * cosine,
    };
  }

  std::vector<float> y_boundaries;
  y_boundaries.reserve(kVertexCount);
  for (const SkPoint &vertex : vertices) {
    y_boundaries.push_back(vertex.fY);
  }
  std::sort(y_boundaries.begin(), y_boundaries.end());
  y_boundaries.erase(std::unique(y_boundaries.begin(), y_boundaries.end(),
                                 [](float a, float b) {
                                   return std::abs(a - b) <
                                          kVertexMergeTolerance;
                                 }),
                     y_boundaries.end());
  if (y_boundaries.size() < 2U) {
    return {};
  }

  const auto horizontal_span = [&vertices](float y) {
    float left = vertices.front().fX;
    float right = vertices.front().fX;
    bool found = false;
    for (int edge = 0; edge < kVertexCount; ++edge) {
      const SkPoint start = vertices[edge];
      const SkPoint end = vertices[(edge + 1) % kVertexCount];
      if (std::abs(end.fY - start.fY) < 0.0001F) {
        if (std::abs(y - start.fY) < 0.0001F) {
          left = found ? std::min(left, std::min(start.fX, end.fX))
                       : std::min(start.fX, end.fX);
          right = found ? std::max(right, std::max(start.fX, end.fX))
                        : std::max(start.fX, end.fX);
          found = true;
        }
        continue;
      }
      if (y < std::min(start.fY, end.fY) - 0.0001F ||
          y > std::max(start.fY, end.fY) + 0.0001F) {
        continue;
      }
      const float edge_t = (y - start.fY) / (end.fY - start.fY);
      const float x = std::lerp(start.fX, end.fX, edge_t);
      if (!found) {
        left = x;
        right = x;
        found = true;
      } else {
        left = std::min(left, x);
        right = std::max(right, x);
      }
    }
    return std::pair<float, float>{left, right};
  };

  EnvelopeSurface surface;
  surface.patches.reserve(y_boundaries.size() - 1U);
  surface.v_patch_boundaries.reserve(y_boundaries.size());
  const float minimum_y = y_boundaries.front();
  const float height = y_boundaries.back() - minimum_y;
  for (float y : y_boundaries) {
    surface.v_patch_boundaries.push_back((y - minimum_y) / height);
  }
  for (std::size_t span = 0; span + 1U < y_boundaries.size(); ++span) {
    geometry::BicubicBezierPatch::ControlPoints points;
    for (int row = 0; row < 4; ++row) {
      const float local_v = static_cast<float>(row) / 3.0F;
      const float y =
          std::lerp(y_boundaries[span], y_boundaries[span + 1U], local_v);
      const auto [left, right] = horizontal_span(y);
      for (int column = 0; column < 4; ++column) {
        points[row * 4 + column] = {
            std::lerp(left, right, static_cast<float>(column) / 3.0F), y};
      }
    }
    surface.patches.emplace_back(points);
  }
  return surface;
}

EnvelopeSurface MakeEnvelopeSurface(EnvelopeKind kind, const SkRect &rect,
                                    float time_seconds, float phase,
                                    std::uint32_t seed,
                                    float minimum_side_height) {
  const float width = rect.width();
  const float height = rect.height();
  const float center_y = rect.centerY();
  const auto x_at = [&rect](float u) {
    return std::lerp(rect.left(), rect.right(), u);
  };

  switch (kind) {
  case EnvelopeKind::kBlob:
    return MakeSpanSurface(rect, [&](float v) {
      const float row = v * 3.0F;
      const float width_noise = noise::FractalPerlin2D(
          row * 0.54F + seed * 0.0004F, time_seconds * 0.30F + phase, 3, 2.0F,
          0.5F, seed);
      const float center_noise = noise::FractalPerlin2D(
          row * 0.61F - time_seconds * 0.26F, phase + seed * 0.0003F, 3, 2.0F,
          0.5F, seed ^ 0x9e3779b9U);
      const float y_noise =
          noise::FractalPerlin2D(row * 0.73F + phase, time_seconds * 0.22F, 2,
                                 2.0F, 0.5F, seed ^ 0x85ebca6bU);
      const float sy = v * 2.0F - 1.0F;
      return SpanRow{
          center_noise * width * 0.045F,
          0.88F + (1.0F - std::abs(sy)) * 0.20F + width_noise * 0.06F,
          y_noise * height * 0.055F,
      };
    });
  case EnvelopeKind::kSemicircle: {
    constexpr float kMinimumThickness = 0.38F;
    const float curvature =
        0.36F + (0.5F + 0.5F * std::sin(time_seconds * 0.52F + phase)) * 0.42F;
    const auto top = [&](float u) { return SkPoint{x_at(u), rect.top()}; };
    const auto bottom = [&](float u) {
      const float dome = std::sin(u * std::numbers::pi_v<float>);
      return SkPoint{x_at(u), rect.top() + height * (kMinimumThickness +
                                                     dome * curvature)};
    };
    return MakeCurveSurface(3, top, bottom);
  }
  case EnvelopeKind::kSineAxisParallel:
  case EnvelopeKind::kSineTangent: {
    const bool tangent_sides = kind == EnvelopeKind::kSineTangent;
    const int half_wave_count = 2 + static_cast<int>(seed % 3U);
    const float cycles = static_cast<float>(half_wave_count) * 0.5F;
    const float wave_phase = time_seconds * 0.72F + phase;
    const auto center = [&](float u) {
      return center_y + std::sin(u * std::numbers::pi_v<float> * 2.0F * cycles +
                                 wave_phase) *
                            height * 0.38F;
    };
    const auto center_point = [&](float u) {
      return SkPoint{x_at(u), center(u)};
    };
    const auto normal = [&](float u) {
      const SkPoint tangent = CurveDerivative(center_point, u);
      const float length = std::max(0.001F, std::hypot(tangent.fX, tangent.fY));
      return SkPoint{-tangent.fY / length, tangent.fX / length};
    };
    const auto top = [&](float u) {
      const SkPoint point = center_point(u);
      if (!tangent_sides) {
        return SkPoint{point.fX, point.fY - height * 0.46F};
      }
      const SkPoint curve_normal = normal(u);
      return SkPoint{point.fX - curve_normal.fX * height * 0.46F,
                     point.fY - curve_normal.fY * height * 0.46F};
    };
    const auto bottom = [&](float u) {
      const SkPoint point = center_point(u);
      if (!tangent_sides) {
        return SkPoint{point.fX, point.fY + height * 0.46F};
      }
      const SkPoint curve_normal = normal(u);
      return SkPoint{point.fX + curve_normal.fX * height * 0.46F,
                     point.fY + curve_normal.fY * height * 0.46F};
    };
    return MakeCurveSurface(half_wave_count, top, bottom);
  }
  case EnvelopeKind::kRotatingRectangle:
    return MakeRotatingRectangleSurface(rect, time_seconds);
  case EnvelopeKind::kArcAxisParallel:
  case EnvelopeKind::kArcTangent: {
    const bool tangent_sides = kind == EnvelopeKind::kArcTangent;
    constexpr float kArcHalfThicknessRatio = 0.38F;
    const float sweep =
        std::numbers::pi_v<float> *
        (0.70F +
         0.30F * (0.5F + 0.5F * std::sin(time_seconds * 0.42F + phase)));
    const float direction = (seed & 1U) == 0U ? -1.0F : 1.0F;
    const auto center = [&](float u) {
      const float angle = (u - 0.5F) * sweep;
      const float x_scale = std::max(0.001F, std::sin(sweep * 0.5F));
      const float normalized_height =
          (std::cos(angle) - std::cos(sweep * 0.5F)) /
          std::max(0.001F, 1.0F - std::cos(sweep * 0.5F));
      return SkPoint{
          rect.centerX() + std::sin(angle) / x_scale * width * 0.5F,
          center_y + direction * normalized_height * height * 0.70F,
      };
    };
    const auto normal = [&](float u) {
      const SkPoint tangent = CurveDerivative(center, u);
      const float length = std::max(0.001F, std::hypot(tangent.fX, tangent.fY));
      return SkPoint{-tangent.fY / length, tangent.fX / length};
    };
    const auto top = [&](float u) {
      const SkPoint point = center(u);
      if (!tangent_sides) {
        return SkPoint{point.fX, point.fY - height * kArcHalfThicknessRatio};
      }
      const SkPoint curve_normal = normal(u);
      return SkPoint{
          point.fX - curve_normal.fX * height * kArcHalfThicknessRatio,
          point.fY - curve_normal.fY * height * kArcHalfThicknessRatio};
    };
    const auto bottom = [&](float u) {
      const SkPoint point = center(u);
      if (!tangent_sides) {
        return SkPoint{point.fX, point.fY + height * kArcHalfThicknessRatio};
      }
      const SkPoint curve_normal = normal(u);
      return SkPoint{
          point.fX + curve_normal.fX * height * kArcHalfThicknessRatio,
          point.fY + curve_normal.fY * height * kArcHalfThicknessRatio};
    };
    return MakeCurveSurface(4, top, bottom);
  }
  case EnvelopeKind::kFlag: {
    const float wave_phase = phase - time_seconds * 0.78F;
    const auto center = [&](float u) {
      return center_y +
             std::sin(u * std::numbers::pi_v<float> * 2.5F + wave_phase) *
                 height * 0.30F;
    };
    const auto top = [&](float u) {
      return SkPoint{x_at(u), center(u) - height * 0.27F};
    };
    const auto bottom = [&](float u) {
      return SkPoint{
          x_at(u),
          center(u) +
              height * (0.27F +
                        0.06F * std::sin(u * std::numbers::pi_v<float> * 2.0F +
                                         time_seconds * 0.46F))};
    };
    return MakeCurveSurface(4, top, bottom);
  }
  case EnvelopeKind::kHourglass: {
    const float extension =
        0.5F + 0.5F * std::sin(time_seconds * 0.68F + phase);
    const float waist = 0.42F + extension * 0.26F;
    const float outer_width = 1.06F - extension * 0.08F;
    return MakeSpanSurface(rect, [&](float v) {
      const bool inner_row = v > 0.2F && v < 0.8F;
      return SpanRow{0.0F, inner_row ? waist : outer_width, 0.0F};
    });
  }
  case EnvelopeKind::kPot: {
    const float sway = std::sin(time_seconds * 0.34F + phase) * width * 0.035F;
    return MakeSpanSurface(rect, [&](float v) {
      if (v < 0.1F) {
        return SpanRow{sway, 0.72F, 0.0F};
      }
      if (v < 0.5F) {
        return SpanRow{sway * 0.65F, 0.48F, 0.0F};
      }
      if (v < 0.9F) {
        return SpanRow{sway * 0.25F, 1.08F, 0.0F};
      }
      return SpanRow{0.0F, 0.80F, 0.0F};
    });
  }
  case EnvelopeKind::kHeart: {
    const float pulse = 0.92F + std::sin(time_seconds * 0.62F + phase) * 0.08F;
    const float sway =
        std::sin(time_seconds * 0.31F + phase * 0.8F) * height * 0.06F;
    const float minimum_half_side_height =
        std::min(height * 0.45F, minimum_side_height * 0.5F);
    const auto top = [&](float u) {
      const float sx = u * 2.0F - 1.0F;
      const float edge_taper =
          std::max(0.0F, std::sin(u * std::numbers::pi_v<float>));
      const float lobe =
          std::exp(-std::pow((std::abs(sx) - 0.47F) / 0.27F, 2.0F));
      return SkPoint{x_at(u) + sway * (1.0F - std::abs(sx)),
                     center_y - minimum_half_side_height -
                         height * pulse * edge_taper * (0.06F + lobe * 0.48F)};
    };
    const auto bottom = [&](float u) {
      const float sx = u * 2.0F - 1.0F;
      const float edge_taper =
          std::max(0.0F, std::sin(u * std::numbers::pi_v<float>));
      return SkPoint{x_at(u) + sway * (1.0F - std::abs(sx)),
                     center_y + minimum_half_side_height +
                         height * pulse * edge_taper *
                             (0.10F + (1.0F - std::abs(sx)) * 0.42F)};
    };
    return MakeCurveSurface(4, top, bottom);
  }
  case EnvelopeKind::kFlame: {
    const float tip = 0.56F + std::sin(time_seconds * 0.43F + phase) * 0.09F;
    const float flicker =
        0.90F + std::sin(time_seconds * 0.91F + phase * 1.7F) * 0.10F;
    const float minimum_half_side_height =
        std::min(height * 0.45F, minimum_side_height * 0.5F);
    const auto top = [&](float u) {
      const float edge_taper =
          std::max(0.0F, std::sin(u * std::numbers::pi_v<float>));
      const float tip_distance = (u - tip) / 0.18F;
      const float shoulder =
          std::exp(-std::pow((u - 0.30F) / 0.22F, 2.0F)) * 0.22F;
      return SkPoint{x_at(u), center_y - minimum_half_side_height -
                                  height * edge_taper *
                                      (0.13F +
                                       std::exp(-tip_distance * tip_distance) *
                                           0.58F * flicker +
                                       shoulder)};
    };
    const auto bottom = [&](float u) {
      const float sx = u * 2.0F - 1.0F;
      const float edge_taper =
          std::max(0.0F, std::sin(u * std::numbers::pi_v<float>));
      return SkPoint{x_at(u), center_y + minimum_half_side_height +
                                  height * edge_taper *
                                      (0.30F + (1.0F - sx * sx) * 0.18F)};
    };
    return MakeCurveSurface(4, top, bottom);
  }
  }
  return {};
}

SkPath SampleSurfaceBoundary(const EnvelopeSurface &surface) {
  SkPathBuilder builder;
  bool started = false;
  const auto append = [&](float u, float v) {
    const SkPoint point = surface.Evaluate(u, v);
    if (!started) {
      builder.moveTo(point);
      started = true;
    } else {
      builder.lineTo(point);
    }
  };
  const int sample_count = surface.SampleCount();
  for (int sample = 0; sample <= sample_count; ++sample) {
    append(static_cast<float>(sample) / sample_count, 0.0F);
  }
  for (int sample = 1; sample <= sample_count; ++sample) {
    append(1.0F, static_cast<float>(sample) / sample_count);
  }
  for (int sample = 1; sample <= sample_count; ++sample) {
    append(1.0F - static_cast<float>(sample) / sample_count, 1.0F);
  }
  for (int sample = 1; sample < sample_count; ++sample) {
    append(0.0F, 1.0F - static_cast<float>(sample) / sample_count);
  }
  builder.close();
  return builder.detach();
}

SkPath SampleSurfaceAxis(const EnvelopeSurface &surface, float constant,
                         bool constant_u) {
  SkPathBuilder builder;
  const int sample_count = surface.SampleCount();
  for (int sample = 0; sample <= sample_count; ++sample) {
    const float parameter = static_cast<float>(sample) / sample_count;
    const SkPoint point = constant_u ? surface.Evaluate(constant, parameter)
                                     : surface.Evaluate(parameter, constant);
    if (sample == 0) {
      builder.moveTo(point);
    } else {
      builder.lineTo(point);
    }
  }
  return builder.detach();
}

SkPath WarpText(const EnvelopeTextShape &text, const EnvelopeSurface &surface) {
  SkPathBuilder builder;
  builder.setFillType(text.fill_type);
  for (const EnvelopeTextContour &contour : text.contours) {
    if (contour.normalized_points.size() < 3U) {
      continue;
    }
    builder.moveTo(surface.Evaluate(contour.normalized_points.front().fX,
                                    contour.normalized_points.front().fY));
    for (std::size_t index = 1; index < contour.normalized_points.size();
         ++index) {
      const SkPoint normalized = contour.normalized_points[index];
      builder.lineTo(surface.Evaluate(normalized.fX, normalized.fY));
    }
    builder.close();
  }
  return builder.detach();
}

bool BuildDenseTextShape(const SkPath &path, EnvelopeTextShape *shape) {
  const SkRect bounds = path.getBounds();
  if (path.isEmpty() || bounds.width() <= 0.0F || bounds.height() <= 0.0F) {
    return false;
  }
  shape->contours.clear();
  shape->fill_type = path.getFillType();

  SkPathMeasure measure(path, false, 1.0F);
  do {
    const float length = measure.getLength();
    if (!measure.isClosed() || length <= 0.0F) {
      continue;
    }
    const int sample_count =
        std::max(10, static_cast<int>(std::ceil(length / kTextSampleSpacing)));
    EnvelopeTextContour contour;
    contour.normalized_points.reserve(sample_count);
    for (int sample = 0; sample < sample_count; ++sample) {
      SkPoint point;
      if (!measure.getPosTan(length * static_cast<float>(sample) /
                                 static_cast<float>(sample_count),
                             &point, nullptr)) {
        continue;
      }
      contour.normalized_points.push_back({
          std::clamp((point.fX - bounds.left()) / bounds.width(), 0.0F, 1.0F),
          std::clamp((point.fY - bounds.top()) / bounds.height(), 0.0F, 1.0F),
      });
    }
    if (contour.normalized_points.size() >= 3U) {
      shape->contours.push_back(std::move(contour));
    }
  } while (measure.nextContour());
  return !shape->contours.empty();
}

void DrawLegendChip(SkCanvas *canvas, const SkRect &cell_rect,
                    std::string_view label, const SkFont &font,
                    float chip_height, float bottom_margin) {
  SkFont fitted_font = font;
  const float horizontal_padding = font.getSize() * 0.38F;
  const float maximum_text_width =
      std::max(1.0F, cell_rect.width() - 4.0F - horizontal_padding * 2.0F);
  SkRect text_bounds;
  float text_width = fitted_font.measureText(
      label.data(), label.size(), SkTextEncoding::kUTF8, &text_bounds);
  if (text_width > maximum_text_width) {
    fitted_font.setScaleX(maximum_text_width / text_width);
    text_width = fitted_font.measureText(label.data(), label.size(),
                                         SkTextEncoding::kUTF8, &text_bounds);
  }
  const float chip_width = std::min(cell_rect.width() - 4.0F,
                                    text_width + horizontal_padding * 2.0F);
  const SkRect chip =
      SkRect::MakeXYWH(cell_rect.centerX() - chip_width * 0.5F,
                       cell_rect.bottom() - bottom_margin - chip_height,
                       chip_width, chip_height);

  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor4f(kLegendChipColor);
  canvas->drawRoundRect(chip, chip_height * 0.34F, chip_height * 0.34F, paint);

  paint.setColor(SK_ColorBLACK);
  const float baseline =
      chip.centerY() - (text_bounds.top() + text_bounds.bottom()) * 0.5F;
  canvas->drawSimpleText(label.data(), label.size(), SkTextEncoding::kUTF8,
                         chip.centerX() - text_width * 0.5F, baseline,
                         fitted_font, paint);
}

} // namespace

EnvelopeDistortFiddle::EnvelopeDistortFiddle()
    : field_seed_(std::random_device{}()) {}

EnvelopeDistortFiddle::~EnvelopeDistortFiddle() = default;

bool EnvelopeDistortFiddle::EnsureResources() {
  if (webgl_ != nullptr && text_shapes_ready_ && label_typeface_ != nullptr) {
    return true;
  }
  if (initialization_attempted_) {
    return false;
  }
  initialization_attempted_ = true;

  sk_sp<SkFontMgr> font_manager = SkiaFontManager::Instance().FontManager();
  if (font_manager == nullptr || font_manager->countFamilies() == 0) {
    std::cerr << "[cc-engine/stderr] Envelope Distort could not access the "
                 "shared font manager."
              << std::endl;
    return false;
  }
  label_typeface_ =
      font_manager->matchFamilyStyle("Roboto", SkFontStyle::Normal());
  if (label_typeface_ == nullptr) {
    std::cerr << "[cc-engine/stderr] Envelope Distort could not resolve its "
                 "label typeface."
              << std::endl;
    return false;
  }
  font_collection_ = sk_make_sp<skia::textlayout::FontCollection>();
  font_collection_->setDefaultFontManager(font_manager, "Roboto");

  auto webgl = std::make_unique<WebGlCanvasContext>();
  if (!webgl->Initialize(WebGlResource())) {
    return false;
  }
  webgl_ = std::move(webgl);
  text_shapes_ready_ = BuildTextShapes();
  return text_shapes_ready_;
}

bool EnvelopeDistortFiddle::BuildTextShapes() {
  sk_sp<SkUnicode> unicode = SkUnicodes::ICU::Make();
  if (unicode == nullptr) {
    return false;
  }

  for (std::size_t font_index = 0; font_index < text::kFontChoices.size();
       ++font_index) {
    const std::string family_name =
        text::ResolveFontFamily(text::kFontChoices[font_index]);
    for (std::size_t word_index = 0; word_index < kWords.size(); ++word_index) {
      TextStyle text_style;
      if (family_name.empty()) {
        text_style.setFontFamilies({});
      } else {
        text_style.setFontFamilies({SkString(family_name.c_str())});
      }
      text_style.setFontSize(kTextSourceSize);
      text_style.setColor(SK_ColorBLACK);
      text_style.addFontFeature(SkString("liga"), 0);
      text_style.addFontFeature(SkString("clig"), 0);

      ParagraphStyle paragraph_style;
      paragraph_style.setTextDirection(TextDirection::kLtr);
      paragraph_style.setTextAlign(TextAlign::kLeft);
      paragraph_style.setFakeMissingFontStyles(true);

      auto paragraph_builder =
          ParagraphBuilder::make(paragraph_style, font_collection_, unicode);
      if (paragraph_builder == nullptr) {
        return false;
      }
      paragraph_builder->pushStyle(text_style);
      paragraph_builder->addText(kWords[word_index]);
      paragraph_builder->pop();
      std::unique_ptr<Paragraph> paragraph = paragraph_builder->Build();
      if (paragraph == nullptr) {
        return false;
      }
      paragraph->layout(kTextSourceSize * 12.0F);

      SkPathBuilder path_builder;
      paragraph->visit([&path_builder](int,
                                       const Paragraph::VisitorInfo *info) {
        if (info == nullptr) {
          return;
        }
        for (int glyph = 0; glyph < info->count; ++glyph) {
          const std::optional<SkPath> glyph_path =
              info->font.getPath(info->glyphs[glyph]);
          if (!glyph_path.has_value() || glyph_path->isEmpty()) {
            continue;
          }
          const SkPoint position = {
              info->positions[glyph].x() + info->origin.x(),
              info->positions[glyph].y() + info->origin.y(),
          };
          SkPath positioned;
          glyph_path->transform(SkMatrix::Translate(position.fX, position.fY),
                                &positioned);
          path_builder.addPath(positioned);
        }
      });
      if (!BuildDenseTextShape(path_builder.detach(),
                               &text_shapes_[font_index][word_index])) {
        std::cerr << "[cc-engine/stderr] Envelope Distort could not prepare "
                  << text::kFontChoices[font_index].display_name << " / "
                  << kWords[word_index] << "." << std::endl;
        return false;
      }
    }
  }
  std::cout << "[cc-engine/stdout] Envelope Distort prepared "
            << text::kFontChoices.size() * kWords.size()
            << " dense word/font outline sets." << std::endl;
  return true;
}

bool EnvelopeDistortFiddle::RebuildGrid(float width, float height) {
  if (width <= 0.0F || height <= 0.0F) {
    return false;
  }
  column_count_ = Width() <= kCompactCanvasWidth ? kCompactColumnCount
                                                 : kDesktopColumnCount;
  row_count_ = (kEnvelopeKindCount + column_count_ - 1) / column_count_;
  cell_width_ = width / static_cast<float>(column_count_);
  cell_height_ = height / static_cast<float>(row_count_);

  std::mt19937 random(field_seed_);
  std::vector<int> kinds;
  kinds.reserve(kEnvelopeKindCount);
  for (int index = 0; index < kEnvelopeKindCount; ++index) {
    kinds.push_back(index);
  }
  std::shuffle(kinds.begin(), kinds.end(), random);

  cells_.clear();
  cells_.reserve(kEnvelopeKindCount);
  for (int index = 0; index < kEnvelopeKindCount; ++index) {
    cells_.push_back({
        kinds[index],
        random(),
        HashUnit(random()) * std::numbers::pi_v<float> * 2.0F,
    });
  }
  cached_width_ = static_cast<int>(width);
  cached_height_ = static_cast<int>(height);
  return !cells_.empty();
}

void EnvelopeDistortFiddle::Render(double time_seconds) {
  if (!EnsureResources()) {
    return;
  }
  const int width = PixelWidth();
  const int height = PixelHeight();
  SkSurface *surface = webgl_->AcquireSurface(width, height);
  if (surface == nullptr) {
    return;
  }
  const int desired_column_count = Width() <= kCompactCanvasWidth
                                       ? kCompactColumnCount
                                       : kDesktopColumnCount;
  if (cells_.empty() || cached_width_ != width || cached_height_ != height) {
    if (!RebuildGrid(static_cast<float>(width), static_cast<float>(height))) {
      return;
    }
  } else if (column_count_ != desired_column_count) {
    if (!RebuildGrid(static_cast<float>(width), static_cast<float>(height))) {
      return;
    }
  }

  const int columns = column_count_;
  const int rows = row_count_;
  const float grid_width = columns * cell_width_;
  const float grid_height = rows * cell_height_;
  const float grid_left = (static_cast<float>(width) - grid_width) * 0.5F;
  const float grid_top = (static_cast<float>(height) - grid_height) * 0.5F;
  const float cell_short_edge = std::min(cell_width_, cell_height_);
  const float padding = cell_short_edge * kCellPaddingRatio;
  const float animated_time = static_cast<float>(time_seconds) * kAnimationRate;
  const int word_frame =
      static_cast<int>(std::floor(animated_time / kWordCycleSeconds));
  const int font_index =
      text::CyclingFontIndex(animated_time, kWordCycleSeconds);
  const float device_scale = static_cast<float>(width / std::max(1.0, Width()));
  const float legend_font_size =
      Width() <= kCompactCanvasWidth
          ? std::clamp(cell_short_edge * 0.08F, 7.0F * device_scale,
                       12.0F * device_scale)
          : std::clamp(std::min(static_cast<float>(width),
                                static_cast<float>(height)) /
                           4.0F * 0.1875F,
                       17.5F, 27.5F);
  const float legend_chip_height = legend_font_size * 1.55F;
  const float legend_bottom_margin = cell_height_ * 0.035F;
  const float legend_gap = cell_height_ * 0.025F;
  SkFont legend_font(label_typeface_, legend_font_size);
  legend_font.setEdging(SkFont::Edging::kAntiAlias);

  SkCanvas *canvas = surface->getCanvas();
  canvas->clear(kCanvasColor);
  SkPaint paint;
  paint.setAntiAlias(true);

  for (int row = 0; row < rows; ++row) {
    for (int column = 0; column < columns; ++column) {
      const int cell_index = row * columns + column;
      if (cell_index >= static_cast<int>(cells_.size())) {
        continue;
      }
      const EnvelopeDemoCell &cell = cells_[cell_index];
      const SkRect cell_rect = SkRect::MakeXYWH(
          grid_left + column * cell_width_, grid_top + row * cell_height_,
          cell_width_, cell_height_);

      paint.setStyle(SkPaint::kStroke_Style);
      paint.setStrokeWidth(1.0F);
      paint.setColor4f(kCellBorderColor);
      canvas->drawLine(cell_rect.left(), cell_rect.top(), cell_rect.right(),
                       cell_rect.top(), paint);
      canvas->drawLine(cell_rect.left(), cell_rect.top(), cell_rect.left(),
                       cell_rect.bottom(), paint);
      if (row == rows - 1) {
        canvas->drawLine(cell_rect.left(), cell_rect.bottom(),
                         cell_rect.right(), cell_rect.bottom(), paint);
      }
      if (column == columns - 1) {
        canvas->drawLine(cell_rect.right(), cell_rect.top(), cell_rect.right(),
                         cell_rect.bottom(), paint);
      }

      const SkRect target_rect = SkRect::MakeLTRB(
          cell_rect.left() + padding, cell_rect.top() + padding,
          cell_rect.right() - padding, cell_rect.bottom() - padding);
      const float artwork_bottom = std::max(
          target_rect.top() + 1.0F, cell_rect.bottom() - legend_bottom_margin -
                                        legend_chip_height - legend_gap);
      const SkRect artwork_rect =
          SkRect::MakeLTRB(target_rect.left(), target_rect.top(),
                           target_rect.right(), artwork_bottom);
      float envelope_width = target_rect.width();
      float envelope_height = envelope_width / kEnvelopeAspectRatio;
      if (envelope_height > artwork_rect.height()) {
        envelope_height = artwork_rect.height();
        envelope_width = envelope_height * kEnvelopeAspectRatio;
      }
      const SkRect envelope_rect =
          SkRect::MakeXYWH(artwork_rect.centerX() - envelope_width * 0.5F,
                           artwork_rect.centerY() - envelope_height * 0.5F,
                           envelope_width, envelope_height);
      const EnvelopeKind envelope_kind =
          static_cast<EnvelopeKind>(cell.envelope_kind);
      const float shape_time =
          animated_time *
          (IsSineEnvelope(envelope_kind) ? 1.0F : kNonSineAnimationMultiplier);
      const EnvelopeSurface surface = MakeEnvelopeSurface(
          envelope_kind, envelope_rect, shape_time, cell.phase, cell.seed,
          kMinimumEnvelopeSideHeight * device_scale);
      SkPath envelope_path = SampleSurfaceBoundary(surface);
      std::array<SkPath, 4> axis_paths = {
          SampleSurfaceAxis(surface, 1.0F / 3.0F, true),
          SampleSurfaceAxis(surface, 2.0F / 3.0F, true),
          SampleSurfaceAxis(surface, 1.0F / 3.0F, false),
          SampleSurfaceAxis(surface, 2.0F / 3.0F, false),
      };
      const int word_index =
          (cell_index + word_frame) % static_cast<int>(kWords.size());
      SkPath warped_text =
          WarpText(text_shapes_[font_index][word_index], surface);

      SkRect content_bounds = envelope_path.getBounds();
      if (!warped_text.isEmpty()) {
        content_bounds.join(warped_text.getBounds());
      }
      for (const SkPath &axis : axis_paths) {
        if (!axis.isEmpty()) {
          content_bounds.join(axis.getBounds());
        }
      }
      if (content_bounds.isEmpty() || content_bounds.width() <= 0.0F ||
          content_bounds.height() <= 0.0F) {
        continue;
      }
      const float fit_scale =
          std::min(artwork_rect.width() / content_bounds.width(),
                   artwork_rect.height() / content_bounds.height());
      const SkMatrix fit = SkMatrix::ScaleTranslate(
          fit_scale, fit_scale,
          artwork_rect.centerX() - content_bounds.centerX() * fit_scale,
          artwork_rect.centerY() - content_bounds.centerY() * fit_scale);
      envelope_path.transform(fit);
      warped_text.transform(fit);
      for (SkPath &axis : axis_paths) {
        axis.transform(fit);
      }

      paint.setStyle(SkPaint::kFill_Style);
      paint.setColor4f(kEnvelopeColors[static_cast<std::size_t>(cell_index) %
                                       kEnvelopeColors.size()]);
      canvas->drawPath(envelope_path, paint);

      paint.setStyle(SkPaint::kStroke_Style);
      paint.setStrokeWidth(1.0F);
      paint.setColor4f(kAxisColor);
      for (const SkPath &axis : axis_paths) {
        canvas->drawPath(axis, paint);
      }

      paint.setStyle(SkPaint::kFill_Style);
      paint.setColor(SK_ColorBLACK);
      canvas->drawPath(warped_text, paint);

      DrawLegendChip(canvas, cell_rect, EnvelopeLabel(envelope_kind),
                     legend_font, legend_chip_height, legend_bottom_margin);
    }
  }

  const WebGlPresentResult present = webgl_->FlushAndPresent();
  if (!present.success) {
    std::cerr << "[cc-engine/stderr] Envelope Distort could not submit its "
                 "WebGL frame."
              << std::endl;
  }
}
