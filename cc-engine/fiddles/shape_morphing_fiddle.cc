#include "fiddles/shape_morphing_fiddle.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <optional>
#include <random>
#include <sstream>
#include <string>

#include "graphics/webgl_canvas_context.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkFontStyle.h"
#include "include/core/SkMatrix.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/core/SkRect.h"
#include "include/core/SkString.h"
#include "include/core/SkSurface.h"
#include "text/skia_font_manager.h"

namespace {

constexpr char kFirstUppercaseLetter = 'A';
constexpr int kUppercaseLetterCount = 26;
constexpr float kGlyphSourceSize = 1000.0F;
constexpr double kOneWaySeconds = 2.8;
constexpr double kMorphSpeedScale = 0.5;
constexpr double kEndpointHoldSeconds = 0.5;
constexpr SkColor kCanvasColor = SkColorSetRGB(247, 248, 252);
constexpr SkColor kSourceColor = SkColorSetRGB(96, 139, 232);
constexpr SkColor kTargetColor = SkColorSetRGB(243, 119, 142);
constexpr SkColor kMorphColor = SkColorSetRGB(157, 128, 232);
constexpr SkColor kInkColor = SkColorSetRGB(28, 31, 40);
constexpr SkColor kMutedColor = SkColorSetRGB(104, 112, 132);
constexpr SkColor kShiftedStartColor = SkColorSetRGB(34, 197, 94);
constexpr bool kEnableFeaturePivotCorrespondence = true;

struct ColumnLayout {
  float padding = 0.0F;
  float gap = 0.0F;
  float width = 0.0F;
  float center_y = 0.0F;
  float content_height = 0.0F;
  float bottom_space = 0.0F;

  float CenterX(int column) const {
    return padding + width * 0.5F + static_cast<float>(column) * (width + gap);
  }
};

ColumnLayout MakeLayout(float width, float height) {
  const float shortest = std::min(width, height);
  const float padding = std::clamp(shortest * 0.045F, 18.0F, 48.0F);
  const float gap = std::clamp(shortest * 0.030F, 14.0F, 34.0F);
  const float column_width =
      std::max(1.0F, (width - 2.0F * padding - 2.0F * gap) / 3.0F);
  const float top_space = std::clamp(shortest * 0.13F, 56.0F, 104.0F);
  const float bottom_space = std::clamp(shortest * 0.19F, 100.0F, 150.0F);
  const float content_height =
      std::max(1.0F, height - top_space - bottom_space);
  return {padding,        gap,
          column_width,   top_space + content_height * 0.5F,
          content_height, bottom_space};
}

float HeldTriangleWave(double time_seconds) {
  const double movement_seconds = kOneWaySeconds / kMorphSpeedScale;
  const double cycle_seconds =
      movement_seconds * 2.0 + kEndpointHoldSeconds * 2.0;
  double phase = std::fmod(std::max(0.0, time_seconds), cycle_seconds);
  if (phase < movement_seconds) {
    return static_cast<float>(phase / movement_seconds);
  }
  phase -= movement_seconds;
  if (phase < kEndpointHoldSeconds) {
    return 1.0F;
  }
  phase -= kEndpointHoldSeconds;
  if (phase < movement_seconds) {
    return static_cast<float>(1.0 - phase / movement_seconds);
  }
  return 0.0F;
}

void DrawColumnCard(SkCanvas *canvas, float center_x,
                    const ColumnLayout &layout, bool active) {
  const float top = layout.center_y - layout.content_height * 0.5F;
  const SkRect card = SkRect::MakeXYWH(center_x - layout.width * 0.5F, top,
                                       layout.width, layout.content_height);
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(active ? SkColorSetRGB(239, 236, 255) : SK_ColorWHITE);
  canvas->drawRoundRect(card, 22.0F, 22.0F, paint);

  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeWidth(active ? 2.0F : 1.0F);
  paint.setColor(active ? SkColorSetRGB(192, 179, 246)
                        : SkColorSetRGB(225, 228, 237));
  canvas->drawRoundRect(card, 22.0F, 22.0F, paint);
}

void DrawCenteredPath(SkCanvas *canvas, const SkPath &path, float center_x,
                      float center_y, SkColor color, float outline_width) {
  canvas->save();
  canvas->translate(center_x, center_y);

  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(color);
  canvas->drawPath(path, paint);

  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeWidth(outline_width);
  paint.setStrokeJoin(SkPaint::kRound_Join);
  paint.setColor(SkColorSetARGB(75, 18, 20, 28));
  canvas->drawPath(path, paint);
  canvas->restore();
}

void DrawCenteredText(SkCanvas *canvas, const sk_sp<SkTypeface> &typeface,
                      const std::string &text, float center_x, float baseline,
                      float size, SkColor color) {
  const SkFont font(typeface, size);
  const float text_width =
      font.measureText(text.data(), text.size(), SkTextEncoding::kUTF8);
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setColor(color);
  canvas->drawSimpleText(text.data(), text.size(), SkTextEncoding::kUTF8,
                         center_x - text_width * 0.5F, baseline, font, paint);
}

void DrawLeftText(SkCanvas *canvas, const sk_sp<SkTypeface> &typeface,
                  const std::string &text, float left, float baseline,
                  float size, SkColor color) {
  const SkFont font(typeface, size);
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setColor(color);
  canvas->drawSimpleText(text.data(), text.size(), SkTextEncoding::kUTF8, left,
                         baseline, font, paint);
}

void DrawStartEye(SkCanvas *canvas, SkPoint point, float radius,
                  SkColor pupil_color) {
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(kInkColor);
  canvas->drawCircle(point.x(), point.y(), radius, paint);
  paint.setColor(SK_ColorWHITE);
  canvas->drawCircle(point.x(), point.y(), radius * 0.68F, paint);
  paint.setColor(pupil_color);
  canvas->drawCircle(point.x(), point.y(), radius * 0.34F, paint);
}

void DrawContourDiagnostics(
    SkCanvas *canvas,
    const std::vector<skmorph::ContourStartPoints> &diagnostics, float center_x,
    float center_y, float shortest) {
  const float eye_radius = std::clamp(shortest * 0.0085F, 5.0F, 8.0F);

  for (const skmorph::ContourStartPoints &contour : diagnostics) {
    DrawStartEye(
        canvas,
        {center_x + contour.original.x(), center_y + contour.original.y()},
        eye_radius, kInkColor);
    if (contour.shifted.has_value()) {
      DrawStartEye(
          canvas,
          {center_x + contour.shifted->x(), center_y + contour.shifted->y()},
          eye_radius, kShiftedStartColor);
    }
  }
}

void DrawDiagnosticsLegend(SkCanvas *canvas, const sk_sp<SkTypeface> &typeface,
                           const ColumnLayout &layout, float canvas_height,
                           float shortest) {
  const float eye_radius = std::clamp(shortest * 0.0095F, 6.0F, 9.0F);
  const float font_size = std::clamp(shortest * 0.026F, 16.0F, 23.0F);
  const float bottom_top = canvas_height - layout.bottom_space;
  const float row_height = std::max(20.0F, font_size * 1.35F);
  const float icon_x = layout.padding + eye_radius;
  const float first_y =
      bottom_top +
      std::max(14.0F, (layout.bottom_space - row_height * 2.0F) * 0.5F);
  const float label_x = icon_x + eye_radius + 9.0F;

  DrawStartEye(canvas, {icon_x, first_y}, eye_radius, kInkColor);
  DrawLeftText(canvas, typeface, "Original contour start", label_x,
               first_y + font_size * 0.36F, font_size, kMutedColor);
  DrawStartEye(canvas, {icon_x, first_y + row_height}, eye_radius,
               kShiftedStartColor);
  DrawLeftText(canvas, typeface, "Shifted correspondence start", label_x,
               first_y + row_height + font_size * 0.36F, font_size,
               kMutedColor);
}

} // namespace

ShapeMorphingFiddle::ShapeMorphingFiddle() {
  const uint64_t clock_seed = static_cast<uint64_t>(
      std::chrono::high_resolution_clock::now().time_since_epoch().count());
  std::seed_seq seed = {
      static_cast<uint32_t>(clock_seed),
      static_cast<uint32_t>(clock_seed >> 32U),
      std::random_device{}(),
  };
  std::mt19937 random(seed);
  std::uniform_int_distribution<int> source_distribution(
      0, kUppercaseLetterCount - 1);
  std::uniform_int_distribution<int> target_offset_distribution(
      1, kUppercaseLetterCount - 1);
  const int source_index = source_distribution(random);
  const int target_index = (source_index + target_offset_distribution(random)) %
                           kUppercaseLetterCount;
  source_letter_ = static_cast<char>(kFirstUppercaseLetter + source_index);
  target_letter_ = static_cast<char>(kFirstUppercaseLetter + target_index);
}

ShapeMorphingFiddle::~ShapeMorphingFiddle() = default;

bool ShapeMorphingFiddle::UsesWebGl() const { return true; }

bool ShapeMorphingFiddle::EnsureResources() {
  if (webgl_ != nullptr && typeface_ != nullptr) {
    return true;
  }
  if (initialization_attempted_) {
    return false;
  }
  initialization_attempted_ = true;

  sk_sp<SkFontMgr> font_manager = SkiaFontManager::Instance().FontManager();
  if (font_manager == nullptr || font_manager->countFamilies() == 0) {
    std::cerr << "[cc-engine/stderr] Shape morphing could not access the "
                 "default font manager."
              << std::endl;
    return false;
  }
  SkString default_family;
  font_manager->getFamilyName(0, &default_family);
  typeface_ = font_manager->matchFamilyStyle(default_family.c_str(),
                                             SkFontStyle::Normal());
  if (typeface_ == nullptr) {
    std::cerr << "[cc-engine/stderr] Shape morphing could not resolve the "
                 "default typeface."
              << std::endl;
    return false;
  }

  auto webgl = std::make_unique<WebGlCanvasContext>();
  if (!webgl->Initialize(Canvas())) {
    return false;
  }
  webgl_ = std::move(webgl);
  return true;
}

bool ShapeMorphingFiddle::RebuildGlyphs(float width, float height) {
  const SkFont source_font(typeface_, kGlyphSourceSize);
  const std::optional<SkPath> source_glyph =
      source_font.getPath(source_font.unicharToGlyph(source_letter_));
  const std::optional<SkPath> target_glyph =
      source_font.getPath(source_font.unicharToGlyph(target_letter_));
  if (!source_glyph.has_value() || !target_glyph.has_value() ||
      source_glyph->isEmpty() || target_glyph->isEmpty()) {
    return false;
  }

  const SkRect source_bounds = source_glyph->getBounds();
  const SkRect target_bounds = target_glyph->getBounds();
  const ColumnLayout layout = MakeLayout(width, height);
  const float maximum_width =
      std::max(source_bounds.width(), target_bounds.width());
  const float maximum_height =
      std::max(source_bounds.height(), target_bounds.height());
  if (maximum_width <= 0.0F || maximum_height <= 0.0F) {
    return false;
  }
  const float scale = std::min(layout.width * 0.70F / maximum_width,
                               layout.content_height * 0.70F / maximum_height);
  const SkMatrix source_placement =
      SkMatrix::ScaleTranslate(scale, scale, -source_bounds.centerX() * scale,
                               -source_bounds.centerY() * scale);
  const SkMatrix target_placement =
      SkMatrix::ScaleTranslate(scale, scale, -target_bounds.centerX() * scale,
                               -target_bounds.centerY() * scale);
  source_glyph->transform(source_placement, &source_path_);
  target_glyph->transform(target_placement, &target_path_);

  skmorph::MorphOptions options;
  options.enableFeaturePivotCorrespondence = kEnableFeaturePivotCorrespondence;
  options.maximumSegmentsPerContour = 2048;
  if (!morpher_.Init(source_path_, target_path_, options)) {
    std::cerr << "[cc-engine/stderr] Shape morphing initialization failed: "
              << morpher_.error() << std::endl;
    return false;
  }
  source_diagnostics_ = morpher_.GetStartPoints(0.0F);
  target_diagnostics_ = morpher_.GetStartPoints(1.0F);

  cached_width_ = static_cast<int>(width);
  cached_height_ = static_cast<int>(height);
  return true;
}

void ShapeMorphingFiddle::Render(double time_seconds) {
  if (!EnsureResources()) {
    return;
  }

  const int width = Canvas()["width"].as<int>();
  const int height = Canvas()["height"].as<int>();
  SkSurface *surface = webgl_->AcquireSurface(width, height);
  if (surface == nullptr) {
    return;
  }
  if (!morpher_.isInitialized() || cached_width_ != width ||
      cached_height_ != height) {
    if (!RebuildGlyphs(static_cast<float>(width), static_cast<float>(height))) {
      return;
    }
  }

  const float canvas_width = static_cast<float>(width);
  const float canvas_height = static_cast<float>(height);
  const float shortest = std::min(canvas_width, canvas_height);
  const ColumnLayout layout = MakeLayout(canvas_width, canvas_height);

  SkCanvas *canvas = surface->getCanvas();
  {
    // SkPath and diagnostics are ref-counted/value-owned frame temporaries.
    // Keeping them in this inner scope releases their CPU allocations before
    // presentation instead of retaining one interpolated shape between ticks.
    const float t = HeldTriangleWave(time_seconds);
    const SkPath intermediate = morpher_.GetMorphed(t);
    const std::vector<skmorph::ContourStartPoints> intermediate_diagnostics =
        morpher_.GetStartPoints(t);

    canvas->clear(kCanvasColor);
    for (int column = 0; column < 3; ++column) {
      DrawColumnCard(canvas, layout.CenterX(column), layout, column == 1);
    }

    const float outline_width = std::clamp(shortest * 0.002F, 1.0F, 2.5F);
    DrawCenteredPath(canvas, source_path_, layout.CenterX(0), layout.center_y,
                     kSourceColor, outline_width);
    DrawCenteredPath(canvas, intermediate, layout.CenterX(1), layout.center_y,
                     kMorphColor, outline_width);
    DrawCenteredPath(canvas, target_path_, layout.CenterX(2), layout.center_y,
                     kTargetColor, outline_width);
    DrawContourDiagnostics(canvas, source_diagnostics_, layout.CenterX(0),
                           layout.center_y, shortest);
    DrawContourDiagnostics(canvas, intermediate_diagnostics, layout.CenterX(1),
                           layout.center_y, shortest);
    DrawContourDiagnostics(canvas, target_diagnostics_, layout.CenterX(2),
                           layout.center_y, shortest);

    const float title_size = std::clamp(shortest * 0.035F, 19.0F, 31.0F);
    const float detail_size = std::clamp(shortest * 0.034F, 20.0F, 29.0F);
    const float title_baseline = std::clamp(shortest * 0.065F, 35.0F, 60.0F);
    DrawCenteredText(canvas, typeface_, "A", layout.CenterX(0), title_baseline,
                     title_size, kInkColor);
    DrawCenteredText(canvas, typeface_, "Morph (A, B, t)", layout.CenterX(1),
                     title_baseline, title_size, kInkColor);
    DrawCenteredText(canvas, typeface_, "B", layout.CenterX(2), title_baseline,
                     title_size, kInkColor);

    std::ostringstream progress;
    progress << std::fixed << std::setprecision(2) << "t = " << t;
    const float bottom_top = canvas_height - layout.bottom_space;
    DrawCenteredText(canvas, typeface_, progress.str(), layout.CenterX(1),
                     bottom_top + layout.bottom_space * 0.58F, detail_size,
                     kInkColor);
    DrawDiagnosticsLegend(canvas, typeface_, layout, canvas_height, shortest);
  }

  const WebGlPresentResult present = webgl_->FlushAndPresent();
  if (!present.success) {
    std::cerr << "[cc-engine/stderr] Shape morphing could not submit its "
                 "WebGL frame."
              << std::endl;
  }
}
