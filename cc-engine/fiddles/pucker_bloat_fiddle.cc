#include "fiddles/pucker_bloat_fiddle.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <numbers>
#include <string_view>
#include <vector>

#include "geometry/catmull_rom_spline.h"
#include "geometry/pucker_bloat.h"
#include "graphics/canvas_legends.h"
#include "graphics/canvas_widgets.h"
#include "graphics/webgl_canvas_context.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkFontStyle.h"
#include "include/core/SkMatrix.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkRRect.h"
#include "include/core/SkRect.h"
#include "include/core/SkString.h"
#include "include/core/SkSurface.h"
#include "include/effects/SkDashPathEffect.h"
#include "text/skia_font_manager.h"
#include "utils/perlin_noise.h"

namespace {

constexpr SkColor kCanvasColor = 0xffeeeeec;
constexpr SkColor kTableColor = SK_ColorWHITE;
constexpr SkColor kGridColor = 0x4d000000;
constexpr SkColor kShapeColor = 0xff2196f3;
constexpr SkColor kGuideColor = 0x4d000000;
constexpr SkColor kPivotColor = 0xffe24b58;
constexpr SkColor kHeaderInkColor = 0xff222222;
constexpr int kColumnCount = 6;
constexpr int kRowCount = 3;
constexpr float kLogicalPadding = 16.0F;
constexpr float kLogicalHeaderHeight = 42.0F;
constexpr float kLogicalCellPadding = 4.0F;
constexpr float kLogicalSliderWidth = 190.0F;
constexpr float kLogicalSliderHeight = 24.0F;
constexpr float kShapeRadius = 100.0F;
constexpr double kMovementSeconds = 1.8;
constexpr double kEndpointHoldSeconds = 2.0;
constexpr double kZeroHoldSeconds = 0.55;

constexpr std::array<std::string_view, kRowCount> kRowLabels = {
    "original", "center based", "custom pivot"};

SkPath MakePolygon(int side_count, float rotation = 0.0F) {
  SkPathBuilder builder;
  for (int index = 0; index < side_count; ++index) {
    const float angle = rotation + static_cast<float>(index) * 2.0F *
                                       std::numbers::pi_v<float> /
                                       static_cast<float>(side_count);
    const SkPoint point = {std::cos(angle) * kShapeRadius,
                           std::sin(angle) * kShapeRadius};
    if (index == 0) {
      builder.moveTo(point);
    } else {
      builder.lineTo(point);
    }
  }
  builder.close();
  return builder.detach();
}

SkPath MakeNoiseBlob() {
  constexpr int point_count = 10;
  std::vector<SkPoint> points;
  points.reserve(point_count);
  for (int index = 0; index < point_count; ++index) {
    const float angle = static_cast<float>(index) * 2.0F *
                        std::numbers::pi_v<float> /
                        static_cast<float>(point_count);
    const float unit_x = std::cos(angle);
    const float unit_y = std::sin(angle);
    const float radial_noise = noise::FractalPerlin2D(
        unit_x * 1.7F + 3.1F, unit_y * 1.7F - 2.4F, 3, 2.0F, 0.5F, 0x5042U);
    const float tangential_noise = noise::FractalPerlin2D(
        unit_x * 2.1F - 5.2F, unit_y * 2.1F + 4.4F, 2, 2.0F, 0.5F, 0x424cU);
    const float radius = kShapeRadius * (0.86F + radial_noise * 0.20F);
    const float tangent = tangential_noise * kShapeRadius * 0.10F;
    points.push_back({unit_x * radius - unit_y * tangent,
                      unit_y * radius + unit_x * tangent});
  }
  geometry::CatmullRomOptions options;
  options.closed = true;
  options.tension = 0.08F;
  return geometry::CatmullRomToCubicPath(points, options);
}

std::array<SkPath, kColumnCount> MakeSourceShapes() {
  std::array<SkPath, kColumnCount> shapes;
  SkPathBuilder circle_builder;
  circle_builder.addCircle(0.0F, 0.0F, kShapeRadius);
  shapes[0] = circle_builder.detach();
  shapes[1] = MakePolygon(3, -std::numbers::pi_v<float> * 0.5F);
  shapes[2] = MakePolygon(4, std::numbers::pi_v<float> * 0.25F);
  shapes[3] = MakePolygon(8, std::numbers::pi_v<float> * 0.125F);
  shapes[4] = MakeNoiseBlob();
  SkPathBuilder rounded_rect_builder;
  // Skia's rounded rectangle has a pair of tangent anchors at every corner;
  // the arc between each pair is normalized to a cubic by the core algorithm.
  rounded_rect_builder.addRRect(SkRRect::MakeRectXY(
      SkRect::MakeLTRB(-100.0F, -76.0F, 100.0F, 76.0F), 34.0F, 34.0F));
  shapes[5] = rounded_rect_builder.detach();
  return shapes;
}

float ResponsiveScale(int width, int height, float device_scale) {
  const float safe_scale = std::max(device_scale, 0.001F);
  const float logical_width = static_cast<float>(width) / safe_scale;
  const float logical_height = static_cast<float>(height) / safe_scale;
  return std::clamp(std::min(logical_width / 720.0F, logical_height / 600.0F),
                    0.62F, 1.0F);
}

float SmoothStep(float value) {
  value = std::clamp(value, 0.0F, 1.0F);
  return value * value * (3.0F - 2.0F * value);
}

float AnimatedAmount(double time_seconds) {
  // 0 → +1 → 0 → -1 → 0. The extrema linger so viewers can inspect
  // maximum deformation; the neutral state uses a shorter pause.
  constexpr double cycle_seconds = kMovementSeconds * 4.0 +
                                   kEndpointHoldSeconds * 2.0 +
                                   kZeroHoldSeconds * 2.0;
  double phase = std::fmod(std::max(0.0, time_seconds), cycle_seconds);
  if (phase < kMovementSeconds) {
    return SmoothStep(static_cast<float>(phase / kMovementSeconds));
  }
  phase -= kMovementSeconds;
  if (phase < kEndpointHoldSeconds) {
    return 1.0F;
  }
  phase -= kEndpointHoldSeconds;
  if (phase < kMovementSeconds) {
    return 1.0F - SmoothStep(static_cast<float>(phase / kMovementSeconds));
  }
  phase -= kMovementSeconds;
  if (phase < kZeroHoldSeconds) {
    return 0.0F;
  }
  phase -= kZeroHoldSeconds;
  if (phase < kMovementSeconds) {
    return -SmoothStep(static_cast<float>(phase / kMovementSeconds));
  }
  phase -= kMovementSeconds;
  if (phase < kEndpointHoldSeconds) {
    return -1.0F;
  }
  phase -= kEndpointHoldSeconds;
  if (phase < kMovementSeconds) {
    return -1.0F + SmoothStep(static_cast<float>(phase / kMovementSeconds));
  }
  return 0.0F;
}

void DrawRightAlignedText(SkCanvas *canvas, std::string_view text, float right,
                          float center_y, const SkFont &font, SkColor color) {
  SkRect bounds;
  const float width = font.measureText(text.data(), text.size(),
                                       SkTextEncoding::kUTF8, &bounds);
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setColor(color);
  const float baseline = center_y - (bounds.top() + bounds.bottom()) * 0.5F;
  canvas->drawSimpleText(text.data(), text.size(), SkTextEncoding::kUTF8,
                         right - width, baseline, font, paint);
}

void DrawCrosshair(SkCanvas *canvas, const SkPoint &point, float radius,
                   float stroke_width, SkColor color) {
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeWidth(stroke_width);
  paint.setColor(color);
  canvas->drawLine(point.fX - radius, point.fY, point.fX + radius, point.fY,
                   paint);
  canvas->drawLine(point.fX, point.fY - radius, point.fX, point.fY + radius,
                   paint);
}

void DrawGuides(SkCanvas *canvas, const geometry::PuckerBloatResult &result,
                float device_scale) {
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeWidth(std::max(0.5F, 0.75F * device_scale));
  paint.setColor(kGuideColor);
  for (const SkPoint &anchor : result.anchors) {
    canvas->drawLine(result.pivot, anchor, paint);
  }

  const std::array<SkScalar, 2> dash_intervals = {
      std::max(1.5F, 3.0F * device_scale), std::max(1.5F, 3.0F * device_scale)};
  paint.setPathEffect(SkDashPathEffect::Make(dash_intervals, 0.0F));
  for (const SkPoint &midpoint : result.midpoints) {
    canvas->drawLine(result.pivot, midpoint, paint);
  }
  paint.setPathEffect(nullptr);

  const float endpoint_radius = 2.5F * device_scale;
  for (const SkPoint &anchor : result.anchors) {
    DrawCrosshair(canvas, anchor, endpoint_radius,
                  std::max(0.5F, 0.65F * device_scale), kGuideColor);
  }
  for (const SkPoint &midpoint : result.midpoints) {
    DrawCrosshair(canvas, midpoint, endpoint_radius,
                  std::max(0.5F, 0.65F * device_scale), kGuideColor);
  }

  DrawCrosshair(canvas, result.pivot, 7.0F * device_scale,
                std::max(0.7F, device_scale), kGuideColor);
  SkPaint eye;
  eye.setAntiAlias(true);
  eye.setStyle(SkPaint::kFill_Style);
  eye.setColor(SK_ColorWHITE);
  canvas->drawCircle(result.pivot.fX, result.pivot.fY, 4.2F * device_scale,
                     eye);
  eye.setStyle(SkPaint::kStroke_Style);
  eye.setStrokeWidth(std::max(0.75F, device_scale));
  eye.setColor(0x99000000);
  canvas->drawCircle(result.pivot.fX, result.pivot.fY, 4.2F * device_scale,
                     eye);
  eye.setStyle(SkPaint::kFill_Style);
  eye.setColor(kPivotColor);
  canvas->drawCircle(result.pivot.fX, result.pivot.fY, 2.0F * device_scale,
                     eye);
}

} // namespace

PuckerBloatFiddle::PuckerBloatFiddle() : source_shapes_(MakeSourceShapes()) {}
PuckerBloatFiddle::~PuckerBloatFiddle() = default;

bool PuckerBloatFiddle::EnsureResources() {
  if (webgl_ != nullptr && typeface_ != nullptr) {
    return true;
  }
  if (initialization_attempted_) {
    return false;
  }
  initialization_attempted_ = true;

  const sk_sp<SkFontMgr> font_manager =
      SkiaFontManager::Instance().FontManager();
  if (font_manager == nullptr || font_manager->countFamilies() == 0) {
    std::cerr << "[cc-engine/stderr] Pucker and Bloat could not access the "
                 "shared font manager."
              << std::endl;
    return false;
  }
  typeface_ = font_manager->matchFamilyStyle("Roboto", SkFontStyle::Normal());
  if (typeface_ == nullptr) {
    SkString family;
    font_manager->getFamilyName(0, &family);
    typeface_ =
        font_manager->matchFamilyStyle(family.c_str(), SkFontStyle::Normal());
  }
  if (typeface_ == nullptr) {
    return false;
  }

  auto webgl = std::make_unique<WebGlCanvasContext>();
  if (!webgl->Initialize(WebGlResource())) {
    return false;
  }
  webgl_ = std::move(webgl);
  return true;
}

void PuckerBloatFiddle::Render(double time_seconds) {
  if (!EnsureResources()) {
    return;
  }
  time_seconds_ = time_seconds;
  const int width = PixelWidth();
  const int height = PixelHeight();
  SkSurface *surface = webgl_->AcquireSurface(width, height);
  if (surface == nullptr) {
    return;
  }
  DrawFrame(surface->getCanvas(), width, height);
  const WebGlPresentResult result = webgl_->FlushAndPresent();
  if (!result.success) {
    std::cerr << "[cc-engine/stderr] Pucker and Bloat could not submit its "
                 "WebGL frame."
              << std::endl;
  }
}

void PuckerBloatFiddle::DrawFrame(SkCanvas *canvas, int width, int height) {
  canvas->clear(kCanvasColor);
  const float device_scale = static_cast<float>(width / std::max(1.0, Width()));
  const float responsive_scale = ResponsiveScale(width, height, device_scale);
  const float ui_scale = device_scale * responsive_scale;
  const float padding =
      std::clamp(kLogicalPadding * responsive_scale * responsive_scale, 6.0F,
                 kLogicalPadding) *
      device_scale;
  const float header_height =
      std::clamp(kLogicalHeaderHeight * responsive_scale, 30.0F,
                 kLogicalHeaderHeight) *
      device_scale;
  const SkRect table = SkRect::MakeLTRB(padding, padding + header_height,
                                        static_cast<float>(width) - padding,
                                        static_cast<float>(height) - padding);
  if (table.isEmpty()) {
    return;
  }

  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(kTableColor);
  canvas->drawRect(table, paint);

  const float amount = AnimatedAmount(time_seconds_);
  const float chip_size =
      std::clamp(10.0F * responsive_scale, 7.0F, 10.0F) * device_scale;
  SkFont chip_font(typeface_, chip_size);
  chip_font.setEdging(SkFont::Edging::kAntiAlias);
  SkFont slider_font(typeface_,
                     std::clamp(10.0F * responsive_scale, 7.0F, 10.0F) *
                         device_scale);
  slider_font.setEdging(SkFont::Edging::kAntiAlias);

  const float slider_width =
      std::min(kLogicalSliderWidth * ui_scale, table.width() * 0.62F);
  const float slider_height =
      std::clamp(kLogicalSliderHeight * responsive_scale, 18.0F,
                 kLogicalSliderHeight) *
      device_scale;
  const SkRect slider_bounds =
      SkRect::MakeXYWH(table.right() - slider_width,
                       padding + (header_height - slider_height) * 0.5F,
                       slider_width, slider_height);
  graphics::canvas_widgets::TwoSidedSliderStyle slider_style;
  slider_style.track_height = std::max(5.0F * device_scale, 8.0F * ui_scale);
  slider_style.center_mark_width =
      std::max(0.75F * device_scale, 1.0F * ui_scale);
  slider_style.corner_radius = 4.0F * ui_scale;
  slider_style.label_gap = 6.0F * ui_scale;
  slider_style.outer_padding = 7.0F * ui_scale;
  graphics::canvas_widgets::DrawTwoSidedSlider(canvas, slider_bounds, amount,
                                               slider_font, slider_style);
  SkFont mode_font(typeface_,
                   std::clamp(11.0F * responsive_scale, 8.0F, 11.0F) *
                       device_scale);
  mode_font.setEdging(SkFont::Edging::kAntiAlias);
  mode_font.setEmbolden(true);
  const std::string_view mode_label = amount < -0.0001F  ? "Pucker"
                                      : amount > 0.0001F ? "Bloat"
                                                         : "Pucker / Bloat";
  DrawRightAlignedText(canvas, mode_label,
                       slider_bounds.left() - 8.0F * device_scale,
                       slider_bounds.centerY(), mode_font, kHeaderInkColor);

  const float cell_width = table.width() / kColumnCount;
  const float cell_height = table.height() / kRowCount;
  const float cell_padding =
      std::max(2.0F * device_scale, kLogicalCellPadding * ui_scale);
  const float chip_band = chip_size * 2.2F;
  const float square_size =
      std::max(1.0F, std::min(cell_width - cell_padding * 2.0F,
                              cell_height - cell_padding * 2.0F - chip_band));
  const float orbit_angle = static_cast<float>(time_seconds_ * 0.72);
  const float ellipse_x = std::cos(orbit_angle) * square_size * 0.13F;
  const float ellipse_y = std::sin(orbit_angle) * square_size * 0.07F;
  constexpr float diagonal_cos = 0.70710678F;
  const SkPoint pivot_offset = {(ellipse_x - ellipse_y) * diagonal_cos,
                                (ellipse_x + ellipse_y) * diagonal_cos};

  for (int row = 0; row < kRowCount; ++row) {
    for (int column = 0; column < kColumnCount; ++column) {
      const SkRect cell = SkRect::MakeXYWH(table.left() + column * cell_width,
                                           table.top() + row * cell_height,
                                           cell_width, cell_height);
      const SkPoint square_center = {cell.centerX(), cell.top() + cell_padding +
                                                         square_size * 0.5F};
      // At maximum fiddle deformation the split midpoint reaches 1.5 radii.
      // A 0.28-radius source therefore fills most of the cell without clipping.
      const float scale = square_size * 0.28F / kShapeRadius;
      const SkMatrix placement = SkMatrix::ScaleTranslate(
          scale, scale, square_center.fX, square_center.fY);
      SkPath placed;
      source_shapes_[column].transform(placement, &placed);

      if (row == 0) {
        paint.setStyle(SkPaint::kFill_Style);
        paint.setColor(kShapeColor);
        canvas->drawPath(placed, paint);
      } else {
        geometry::PuckerBloatOptions options;
        options.amount = amount;
        options.displacement_scale = 0.5F;
        options.pivot_mode =
            row == 1 ? geometry::PuckerBloatPivotMode::kGeometricCenter
                     : geometry::PuckerBloatPivotMode::kCustomPoint;
        options.custom_pivot = {square_center.fX + pivot_offset.fX,
                                square_center.fY + pivot_offset.fY};
        const geometry::PuckerBloatResult result =
            geometry::PuckerBloatDetailed(placed, options);
        paint.setStyle(SkPaint::kFill_Style);
        paint.setColor(kShapeColor);
        canvas->drawPath(result.path, paint);
        DrawGuides(canvas, result, ui_scale);
      }

      graphics::canvas_legends::TextChipStyle chip_style;
      chip_style.horizontal_padding = 7.0F * ui_scale;
      chip_style.vertical_padding = 2.5F * ui_scale;
      chip_style.border_width = std::max(0.6F * device_scale, 1.0F * ui_scale);
      chip_style.corner_radius = 8.0F * ui_scale;
      const std::string_view legend = kRowLabels[row];
      graphics::canvas_legends::DrawTextChip(
          canvas, legend.data(), legend.size(), cell.centerX(),
          cell.bottom() - cell_padding - chip_band * 0.38F, chip_font,
          chip_style);
    }
  }

  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeWidth(std::max(0.5F * device_scale, ui_scale * 0.75F));
  paint.setColor(kGridColor);
  for (int column = 1; column < kColumnCount; ++column) {
    const float x = table.left() + column * cell_width;
    canvas->drawLine(x, table.top(), x, table.bottom(), paint);
  }
  for (int row = 1; row < kRowCount; ++row) {
    const float y = table.top() + row * cell_height;
    canvas->drawLine(table.left(), y, table.right(), y, paint);
  }
  canvas->drawRect(table, paint);
}
