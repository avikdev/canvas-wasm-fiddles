#include "fiddles/curve_interpolate_fiddle.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <numbers>
#include <string_view>

#include "graphics/webgl_canvas_context.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkFontStyle.h"
#include "include/core/SkMatrix.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkRect.h"
#include "include/core/SkString.h"
#include "include/core/SkSurface.h"
#include "include/effects/SkDashPathEffect.h"
#include "text/skia_font_manager.h"

namespace {

constexpr SkColor kCanvasColor = 0xffe5e7eb;
constexpr SkColor kPanelColor = SK_ColorWHITE;
constexpr SkColor kInkColor = 0xff15171a;
constexpr SkColor kMutedColor = 0xff8a8f98;
constexpr SkColor kBoxColor = 0x4d10bcd4;
constexpr SkColor kProfileColor = 0xff168cff;
constexpr SkColor kStartColor = 0xffff84ba;
constexpr SkColor kEndColor = 0xff99c2ff;

float WidthProfile(float length) {
  return 1.0F + 0.3F * std::sin(4.0F * std::numbers::pi_v<float> * length);
}

struct Layout {
  float scale = 1.0F;
  float padding = 0.0F;
  float gap = 0.0F;
  SkRect input;
  SkRect output;
};

Layout MakeLayout(int width, int height, float device_scale) {
  const float padding = 0.0F;
  const float gap = std::max(18.0F * device_scale, height * 0.035F);
  const float available = std::max(1.0F, height - gap);
  const float input_height = std::floor(available * 0.38F);
  return {
      .scale = device_scale,
      .padding = padding,
      .gap = gap,
      .input = SkRect::MakeXYWH(padding, padding, width - 2.0F * padding,
                                input_height),
      .output =
          SkRect::MakeXYWH(padding, padding + input_height + gap,
                           width - 2.0F * padding, available - input_height),
  };
}

float ResponsiveUiScale(int width, int height, float device_scale) {
  const float logical_width = width / std::max(device_scale, 0.001F);
  const float logical_height = height / std::max(device_scale, 0.001F);
  const float window_factor = std::clamp(
      std::sqrt((logical_width / 925.0F) * (logical_height / 640.0F)), 0.82F,
      1.45F);
  return device_scale * window_factor;
}

SkPath MakeSourceCurve() {
  return SkPathBuilder()
      .moveTo(-96.0F, 8.0F)
      .cubicTo(-88.0F, -45.0F, -58.0F, -54.0F, -39.0F, -12.0F)
      .cubicTo(-28.0F, 18.0F, -13.0F, 34.0F, 0.0F, 0.0F)
      // The opposing tangent leaves a deliberate sharp central notch where
      // the two irregular S-shaped wings meet.
      .cubicTo(15.0F, -31.0F, 33.0F, -20.0F, 44.0F, 10.0F)
      .cubicTo(57.0F, 50.0F, 86.0F, 42.0F, 98.0F, -6.0F)
      .detach();
}

SkPath MakeTargetCurve() {
  return SkPathBuilder()
      .moveTo(-106.0F, -16.0F)
      .cubicTo(-80.0F, -57.0F, -53.0F, 48.0F, -27.0F, 9.0F)
      .cubicTo(-11.0F, -15.0F, 17.0F, -58.0F, 25.0F, -3.0F)
      .cubicTo(31.0F, 45.0F, -28.0F, 47.0F, 1.0F, 4.0F)
      // Break the tangent after the loop to keep one visibly sharp kink.
      .cubicTo(35.0F, -38.0F, 72.0F, -43.0F, 61.0F, 10.0F)
      .cubicTo(54.0F, 45.0F, 99.0F, 50.0F, 112.0F, -2.0F)
      .detach();
}

SkPath MakeGuide(float left, float right, float center, float height) {
  const float span = right - left;
  return SkPathBuilder()
      .moveTo(left, center)
      .cubicTo(left + span * 0.05F, center - height * 0.08F,
               left + span * 0.11F, center - height * 0.31F,
               left + span * 0.22F, center - height * 0.19F)
      .cubicTo(left + span * 0.30F, center - height * 0.10F,
               left + span * 0.29F, center + height * 0.29F,
               left + span * 0.41F, center + height * 0.22F)
      .cubicTo(left + span * 0.49F, center + height * 0.17F,
               left + span * 0.51F, center - height * 0.36F,
               left + span * 0.63F, center - height * 0.27F)
      .cubicTo(left + span * 0.72F, center - height * 0.20F,
               left + span * 0.72F, center + height * 0.27F,
               left + span * 0.84F, center + height * 0.18F)
      .cubicTo(left + span * 0.92F, center + height * 0.12F,
               left + span * 0.96F, center - height * 0.12F, right, center)
      .detach();
}

SkPath FitPathAtScale(const SkPath &path, const SkRect &destination,
                      float scale) {
  const SkRect bounds = path.computeTightBounds();
  if (bounds.isEmpty() || destination.isEmpty() || scale <= 0.0F) {
    return {};
  }
  const SkMatrix matrix = SkMatrix::ScaleTranslate(
      scale, scale, destination.centerX() - bounds.centerX() * scale,
      destination.centerY() - bounds.centerY() * scale);
  SkPath result;
  path.transform(matrix, &result);
  return result;
}

SkColor LerpColor(SkColor first, SkColor second, float amount,
                  U8CPU alpha = 255) {
  return SkColorSetARGB(alpha,
                        static_cast<U8CPU>(std::lerp(
                            SkColorGetR(first), SkColorGetR(second), amount)),
                        static_cast<U8CPU>(std::lerp(
                            SkColorGetG(first), SkColorGetG(second), amount)),
                        static_cast<U8CPU>(std::lerp(
                            SkColorGetB(first), SkColorGetB(second), amount)));
}

void DrawFloatingPill(SkCanvas *canvas, SkPoint center,
                      const sk_sp<SkTypeface> &typeface, std::string_view label,
                      float scale) {
  const float font_size = 11.5F * scale;
  const SkFont font(typeface, font_size);
  const float text_width =
      font.measureText(label.data(), label.size(), SkTextEncoding::kUTF8);
  const float height = 22.0F * scale;
  const float width = text_width + 17.0F * scale;
  const SkRect pill =
      SkRect::MakeXYWH(center.x() - width * 0.5F, center.y(), width, height);
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setColor(SK_ColorBLACK);
  canvas->drawRoundRect(pill, height * 0.5F, height * 0.5F, paint);
  paint.setColor(SK_ColorWHITE);
  canvas->drawSimpleText(label.data(), label.size(), SkTextEncoding::kUTF8,
                         pill.centerX() - text_width * 0.5F,
                         pill.centerY() + font_size * 0.34F, font, paint);
}

void DrawPill(SkCanvas *canvas, const SkRect &panel,
              const sk_sp<SkTypeface> &typeface, std::string_view label,
              float scale) {
  const float font_size = 12.0F * scale;
  const SkFont font(typeface, font_size);
  const float text_width =
      font.measureText(label.data(), label.size(), SkTextEncoding::kUTF8);
  const float height = 24.0F * scale;
  const float width = text_width + 19.0F * scale;
  const SkRect pill =
      SkRect::MakeXYWH(panel.right() - width - 7.0F * scale,
                       panel.top() + 7.0F * scale, width, height);
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setColor(SK_ColorBLACK);
  canvas->drawRoundRect(pill, height * 0.5F, height * 0.5F, paint);
  paint.setColor(SK_ColorWHITE);
  canvas->drawSimpleText(label.data(), label.size(), SkTextEncoding::kUTF8,
                         pill.centerX() - text_width * 0.5F,
                         pill.centerY() + font_size * 0.34F, font, paint);
}

void DrawInputCurve(SkCanvas *canvas, const SkPath &fitted, float scale) {
  const SkRect bounds = fitted.computeTightBounds();
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setColor(kBoxColor);
  paint.setStyle(SkPaint::kFill_Style);
  canvas->drawRect(bounds, paint);
  paint.setColor(kInkColor);
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeWidth(std::max(1.5F * scale, 1.0F));
  paint.setStrokeCap(SkPaint::kRound_Cap);
  canvas->drawPath(fitted, paint);
  const float cross = 4.0F * scale;
  paint.setStrokeWidth(std::max(scale, 1.0F));
  canvas->drawLine(bounds.centerX() - cross, bounds.centerY(),
                   bounds.centerX() + cross, bounds.centerY(), paint);
  canvas->drawLine(bounds.centerX(), bounds.centerY() - cross, bounds.centerX(),
                   bounds.centerY() + cross, paint);
}

void DrawWidthChart(SkCanvas *canvas, const SkRect &bounds,
                    const sk_sp<SkTypeface> &typeface, float scale) {
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeWidth(std::max(scale, 1.0F));
  paint.setColor(kInkColor);
  canvas->drawLine(bounds.left(), bounds.bottom(), bounds.right(),
                   bounds.bottom(), paint);
  canvas->drawLine(bounds.left(), bounds.top(), bounds.left(), bounds.bottom(),
                   paint);
  SkPathBuilder chart;
  constexpr int kSamples = 80;
  for (int index = 0; index <= kSamples; ++index) {
    const float l = static_cast<float>(index) / kSamples;
    const float x = std::lerp(bounds.left(), bounds.right(), l);
    const float normalized = (WidthProfile(l) - 0.7F) / 0.6F;
    const float y = std::lerp(bounds.bottom(), bounds.top(), normalized);
    if (index == 0) {
      chart.moveTo(x, y);
    } else {
      chart.lineTo(x, y);
    }
  }
  paint.setColor(kProfileColor);
  paint.setStrokeCap(SkPaint::kRound_Cap);
  const std::array<SkScalar, 2> dots = {1.0F * scale, 6.0F * scale};
  paint.setPathEffect(SkDashPathEffect::Make(dots, 0.0F));
  canvas->drawPath(chart.detach(), paint);
  paint.setPathEffect(nullptr);

  constexpr std::string_view kLegend = "width(l)";
  const float font_size = 12.0F * scale;
  const SkFont font(typeface, font_size);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(kProfileColor);
  canvas->drawSimpleText(kLegend.data(), kLegend.size(), SkTextEncoding::kUTF8,
                         bounds.left() + 7.0F * scale, bounds.top() + font_size,
                         font, paint);
}

} // namespace

CurveInterpolateFiddle::CurveInterpolateFiddle() = default;
CurveInterpolateFiddle::~CurveInterpolateFiddle() = default;

bool CurveInterpolateFiddle::EnsureResources() {
  if (webgl_ != nullptr && typeface_ != nullptr) {
    return true;
  }
  if (initialization_attempted_) {
    return false;
  }
  initialization_attempted_ = true;
  const sk_sp<SkFontMgr> manager = SkiaFontManager::Instance().FontManager();
  if (manager == nullptr || manager->countFamilies() == 0) {
    return false;
  }
  SkString family;
  manager->getFamilyName(0, &family);
  typeface_ = manager->matchFamilyStyle(family.c_str(), SkFontStyle::Normal());
  auto webgl = std::make_unique<WebGlCanvasContext>();
  if (typeface_ == nullptr || !webgl->Initialize(WebGlResource())) {
    return false;
  }
  webgl_ = std::move(webgl);
  return true;
}

bool CurveInterpolateFiddle::Rebuild(int width, int height) {
  const float device_scale = static_cast<float>(width / std::max(1.0, Width()));
  const Layout layout = MakeLayout(width, height, device_scale);
  source_curve_ = MakeSourceCurve();
  target_curve_ = MakeTargetCurve();
  source_curve_.transform(SkMatrix::Scale(0.85F, 0.85F));
  target_curve_.transform(SkMatrix::Scale(1.70F, 1.70F));
  SkMatrix portrait_rotation;
  portrait_rotation.setRotate(90.0F);
  source_curve_.transform(portrait_rotation);
  target_curve_.transform(portrait_rotation);
  const float guide_inset =
      std::max(82.0F * device_scale, layout.output.width() * 0.09F);
  guide_path_ = MakeGuide(
      layout.output.left() + guide_inset, layout.output.right() - guide_inset,
      layout.output.centerY(), layout.output.height() * 0.82F);
  if (!interpolation_.Init(source_curve_, target_curve_, guide_path_,
                           WidthProfile)) {
    std::cerr << "[cc-engine/stderr] Curve Interpolate: "
              << interpolation_.error() << std::endl;
    return false;
  }
  const int split_count = std::clamp(
      static_cast<int>(std::lround(interpolation_.guideLength() /
                                   std::max(2.0F * device_scale, 1.0F))),
      64, 900);
  split_points_ =
      geometry::GenerateUniformSplitPoints(split_count, false, false);
  cached_width_ = width;
  cached_height_ = height;
  return true;
}

void CurveInterpolateFiddle::Render(double /*time_seconds*/) {
  if (!EnsureResources()) {
    return;
  }
  const int width = PixelWidth();
  const int height = PixelHeight();
  if ((width != cached_width_ || height != cached_height_) &&
      !Rebuild(width, height)) {
    return;
  }
  SkSurface *surface = webgl_->AcquireSurface(width, height);
  if (surface == nullptr) {
    return;
  }
  DrawFrame(surface->getCanvas(), width, height);
  if (!webgl_->FlushAndPresent().success) {
    std::cerr << "[cc-engine/stderr] Curve Interpolate could not submit its "
                 "WebGL frame."
              << std::endl;
  }
}

void CurveInterpolateFiddle::DrawFrame(SkCanvas *canvas, int width,
                                       int height) {
  const float device_scale = static_cast<float>(width / std::max(1.0, Width()));
  const float ui_scale = ResponsiveUiScale(width, height, device_scale);
  const Layout layout = MakeLayout(width, height, device_scale);
  canvas->clear(kCanvasColor);
  SkPaint paint;
  paint.setColor(kPanelColor);
  canvas->drawRect(layout.input, paint);
  canvas->drawRect(layout.output, paint);

  const float input_top = layout.input.top() + 29.0F * ui_scale;
  const float chart_height =
      std::max(18.0F * device_scale, layout.input.height() * 0.25F);
  const float curve_bottom =
      layout.input.bottom() - chart_height - 10.0F * device_scale;
  const float side = layout.input.width() * 0.13F;
  const SkRect source_slot =
      SkRect::MakeLTRB(layout.input.left() + 13.0F * device_scale, input_top,
                       layout.input.left() + side, curve_bottom);
  const SkRect target_slot = SkRect::MakeLTRB(
      layout.input.right() - side, input_top,
      layout.input.right() - 13.0F * device_scale, curve_bottom);

  const SkRect raw_source_bounds = source_curve_.computeTightBounds();
  const SkRect raw_target_bounds = target_curve_.computeTightBounds();
  const float preview_scale =
      std::min({source_slot.width() / raw_source_bounds.width(),
                source_slot.height() / raw_source_bounds.height(),
                target_slot.width() / raw_target_bounds.width(),
                target_slot.height() / raw_target_bounds.height()});
  const SkPath source_preview =
      FitPathAtScale(source_curve_, source_slot, preview_scale);
  const SkPath target_preview =
      FitPathAtScale(target_curve_, target_slot, preview_scale);
  const SkRect source_bounds = source_preview.computeTightBounds();
  const SkRect target_bounds = target_preview.computeTightBounds();
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeWidth(std::max(device_scale, 1.0F));
  paint.setColor(kMutedColor);
  const std::array<SkScalar, 2> dash = {4.0F * device_scale,
                                        4.0F * device_scale};
  paint.setPathEffect(SkDashPathEffect::Make(dash, 0.0F));
  canvas->drawLine(source_bounds.centerX(), source_bounds.centerY(),
                   target_bounds.centerX(), target_bounds.centerY(), paint);
  paint.setPathEffect(nullptr);

  paint.setColor(0xffb3b7be);
  const SkPath guide_preview =
      MakeGuide(source_bounds.centerX(), target_bounds.centerX(),
                (source_bounds.centerY() + target_bounds.centerY()) * 0.5F,
                (curve_bottom - input_top) * 0.72F);
  canvas->drawPath(guide_preview, paint);
  DrawInputCurve(canvas, source_preview, device_scale);
  DrawInputCurve(canvas, target_preview, device_scale);

  const float label_top = layout.input.top() + 4.0F * ui_scale;
  DrawFloatingPill(canvas, {source_bounds.centerX(), label_top}, typeface_,
                   "start curve", ui_scale);
  DrawFloatingPill(
      canvas,
      {(source_bounds.centerX() + target_bounds.centerX()) * 0.5F, label_top},
      typeface_, "guide", ui_scale);
  DrawFloatingPill(canvas, {target_bounds.centerX(), label_top}, typeface_,
                   "end curve", ui_scale);

  const SkRect chart = SkRect::MakeLTRB(
      layout.input.left() + layout.input.width() * 0.22F,
      layout.input.bottom() - chart_height - 5.0F * device_scale,
      layout.input.right() - layout.input.width() * 0.22F,
      layout.input.bottom() - 8.0F * device_scale);
  DrawWidthChart(canvas, chart, typeface_, ui_scale);

  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeCap(SkPaint::kRound_Cap);
  paint.setStrokeJoin(SkPaint::kRound_Join);
  paint.setStrokeWidth(std::max(1.0F * device_scale, 1.0F));
  for (float l : split_points_) {
    paint.setColor(LerpColor(kStartColor, kEndColor, l, 77));
    canvas->drawPath(interpolation_.GetCurve(l), paint);
  }

  DrawPill(canvas, layout.output, typeface_, "output", ui_scale);
}
