#include "fiddles/shape_tracing_fiddle.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

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
#include "include/core/SkPoint.h"
#include "include/core/SkRect.h"
#include "include/core/SkString.h"
#include "include/core/SkSurface.h"
#include "text/skia_font_manager.h"
#include "utils/color_utils.h"

namespace {

constexpr double kTraceCycleSeconds = 12.0;
constexpr float kGlyphSourceSize = 1000.0F;
constexpr float kMarkerSaturation = 0.86F;
constexpr float kMarkerValue = 0.94F;
constexpr int kMarkerCount = 9;
constexpr float kInactiveSegmentValue = 0.82F;
constexpr float kActiveSegmentValue = 1.0F;
constexpr float kSegmentOpacity = 0.25F;
constexpr float kActiveSegmentOpacity = 1.0F;
constexpr float kArrowScale = 2.5F;
constexpr float kMovingEyeScale = 1.0F;
constexpr SkColor kCanvasColor = SK_ColorWHITE;
constexpr SkColor4f kLetterColor = {0.0F, 0.78F, 0.88F, 0.40F};
constexpr SkColor4f kLetterOutlineColor = {0.0F, 0.45F, 0.54F, 0.78F};

struct TracePosition {
  SkPoint position = {0.0F, 0.0F};
  SkVector tangent = {1.0F, 0.0F};
  size_t segment_index = std::numeric_limits<size_t>::max();
  bool valid = false;
};

float SegmentHue(size_t segment_index) {
  // Red, magenta, purple, pink, red. The repeated red intentionally closes the
  // shorter palette before it cycles.
  constexpr std::array<float, 5> kSegmentHues = {0.0F, 310.0F, 275.0F, 335.0F,
                                                 0.0F};
  return kSegmentHues[segment_index % kSegmentHues.size()];
}

TracePosition
PositionAtCombinedDistance(const std::vector<TracedPathSegment> &segments,
                           float distance) {
  TracePosition result;
  float remaining = std::max(0.0F, distance);
  for (size_t index = 0; index < segments.size(); ++index) {
    const TracedPathSegment &segment = segments[index];
    if (segment.length <= 0.0F) {
      continue;
    }
    if (remaining <= segment.length || index + 1U == segments.size()) {
      SkPathMeasure measure(segment.path, false, 2.0F);
      result.valid =
          measure.getPosTan(std::clamp(remaining, 0.0F, segment.length),
                            &result.position, &result.tangent);
      result.segment_index = index;
      return result;
    }
    remaining -= segment.length;
  }
  return result;
}

void DrawAtomicSegments(SkCanvas *canvas,
                        const std::vector<TracedPathSegment> &segments,
                        size_t active_segment, float shortest) {
  const float stroke_width = std::clamp(shortest * 0.015F, 8.0F, 18.0F);

  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeCap(SkPaint::kRound_Cap);
  paint.setStrokeJoin(SkPaint::kRound_Join);
  paint.setStrokeWidth(stroke_width);

  for (size_t index = 0; index < segments.size(); ++index) {
    if (index == active_segment) {
      continue;
    }
    SkColor4f color =
        color_utils::FromHsv(segments[index].hue, 0.96F, kInactiveSegmentValue);
    color.fA = kSegmentOpacity;
    paint.setColor4f(color);
    canvas->drawPath(segments[index].path, paint);
  }

  if (active_segment >= segments.size()) {
    return;
  }
  SkColor4f active_color = color_utils::FromHsv(segments[active_segment].hue,
                                                1.0F, kActiveSegmentValue);
  active_color.fA = kActiveSegmentOpacity;
  paint.setColor4f(active_color);
  paint.setStrokeWidth(stroke_width * 1.25F);
  canvas->drawPath(segments[active_segment].path, paint);
}

void DrawContourStartEyes(SkCanvas *canvas, const SkPath &path, float radius) {
  SkPathMeasure measure(path, false, 2.0F);
  do {
    SkPoint position;
    SkVector tangent;
    if (!measure.getPosTan(0.0F, &position, &tangent)) {
      continue;
    }

    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setStyle(SkPaint::kFill_Style);
    paint.setColor(SkColorSetRGB(255, 183, 0));
    canvas->drawCircle(position.x(), position.y(), radius * 1.65F, paint);
    paint.setColor(SK_ColorBLACK);
    canvas->drawCircle(position.x(), position.y(), radius * 0.78F, paint);
  } while (measure.nextContour());
}

void DrawContourMarkers(SkCanvas *canvas, const SkPath &path, float radius) {
  SkPathMeasure measure(path, false, 2.0F);
  int contour_index = 0;
  do {
    const float contour_length = measure.getLength();
    if (contour_length <= 0.0F) {
      ++contour_index;
      continue;
    }
    for (int marker = 1; marker <= kMarkerCount; ++marker) {
      SkPoint position;
      SkVector tangent;
      const float ratio = static_cast<float>(marker) * 0.10F;
      if (!measure.getPosTan(contour_length * ratio, &position, &tangent)) {
        continue;
      }

      const float hue = std::fmod(static_cast<float>(marker - 1) * 38.0F +
                                      static_cast<float>(contour_index) * 17.0F,
                                  360.0F);
      SkPaint paint;
      paint.setAntiAlias(true);
      paint.setStyle(SkPaint::kFill_Style);
      paint.setColor4f(
          color_utils::FromHsv(hue, kMarkerSaturation, kMarkerValue));
      canvas->drawCircle(position.x(), position.y(), radius, paint);

      paint.setStyle(SkPaint::kStroke_Style);
      paint.setStrokeWidth(std::max(1.0F, radius * 0.20F));
      paint.setColor(SK_ColorWHITE);
      canvas->drawCircle(position.x(), position.y(), radius, paint);
    }
    ++contour_index;
  } while (measure.nextContour());
}

void DrawMovingArrow(SkCanvas *canvas, const TracePosition &trace,
                     float shortest) {
  if (!trace.valid) {
    return;
  }

  SkVector direction = trace.tangent;
  if (!direction.normalize()) {
    return;
  }
  const SkVector normal = {-direction.y(), direction.x()};
  const float arrow_length =
      std::clamp(shortest * 0.075F, 38.0F, 82.0F) * kArrowScale;
  const float head_length = arrow_length * 0.24F;
  const float head_half_width = arrow_length * 0.055F;
  const float base_radius = std::clamp(shortest * 0.012F, 7.0F, 15.0F) *
                            kArrowScale * kMovingEyeScale;
  const SkPoint tip = trace.position + direction * arrow_length;
  const SkPoint head_base = tip - direction * head_length;

  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setColor(SkColorSetRGB(15, 20, 28));
  paint.setStrokeCap(SkPaint::kButt_Cap);
  paint.setStrokeWidth(std::clamp(shortest * 0.0015F, 1.2F, 2.5F) *
                       kArrowScale);
  canvas->drawLine(trace.position, head_base, paint);

  SkPathBuilder head;
  head.moveTo(tip);
  head.lineTo(head_base + normal * head_half_width);
  head.lineTo(head_base - normal * head_half_width);
  head.close();
  paint.setStyle(SkPaint::kFill_Style);
  canvas->drawPath(head.detach(), paint);

  paint.setColor(SK_ColorWHITE);
  canvas->drawCircle(trace.position.x(), trace.position.y(),
                     base_radius * 1.35F, paint);
  paint.setColor(SkColorSetRGB(15, 20, 28));
  canvas->drawCircle(trace.position.x(), trace.position.y(), base_radius,
                     paint);
  paint.setColor(SkColorSetRGB(255, 64, 129));
  canvas->drawCircle(trace.position.x(), trace.position.y(),
                     base_radius * 0.45F, paint);
}

enum class ChipAlignment {
  kLeft,
  kRight,
};

void DrawLegendChip(SkCanvas *canvas, const sk_sp<SkTypeface> &typeface,
                    const std::string &label, ChipAlignment alignment,
                    float width, float height) {
  const float shortest = std::min(width, height);
  const float font_size = std::clamp(shortest * 0.027F, 17.0F, 27.0F);
  const float horizontal_padding = font_size * 0.70F;
  const float vertical_padding = font_size * 0.45F;
  const float margin = std::clamp(shortest * 0.025F, 14.0F, 26.0F);

  const SkFont font(typeface, font_size);
  SkRect text_bounds;
  const float text_width = font.measureText(
      label.data(), label.size(), SkTextEncoding::kUTF8, &text_bounds);
  const float chip_width = text_width + horizontal_padding * 2.0F;
  const float chip_height = text_bounds.height() + vertical_padding * 2.0F;
  const float chip_left =
      alignment == ChipAlignment::kLeft ? margin : width - margin - chip_width;
  const SkRect chip =
      SkRect::MakeXYWH(chip_left, margin, chip_width, chip_height);

  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(SkColorSetRGB(20, 23, 28));
  canvas->drawRoundRect(chip, 7.0F, 7.0F, paint);

  paint.setColor(SK_ColorWHITE);
  const float baseline =
      chip.centerY() - (text_bounds.top() + text_bounds.bottom()) * 0.5F;
  canvas->drawSimpleText(label.data(), label.size(), SkTextEncoding::kUTF8,
                         chip.left() + horizontal_padding, baseline, font,
                         paint);
}

std::string VerbName(SkPath::Verb verb) {
  switch (verb) {
  case SkPath::kMove_Verb:
    return "MoveTo";
  case SkPath::kLine_Verb:
    return "LineTo";
  case SkPath::kQuad_Verb:
    return "QuadBezierTo";
  case SkPath::kConic_Verb:
    return "ConicTo";
  case SkPath::kCubic_Verb:
    return "CuBezierTo";
  case SkPath::kClose_Verb:
    return "Close";
  case SkPath::kDone_Verb:
    return "Done";
  }
  return "Unknown";
}

void DrawLegends(SkCanvas *canvas, const sk_sp<SkTypeface> &typeface,
                 float total_length, float percent,
                 const std::vector<TracedPathSegment> &segments,
                 size_t active_segment, float width, float height) {
  std::ostringstream progress_stream;
  progress_stream << std::fixed << std::setprecision(0)
                  << "Length: " << total_length << "  ·  Covered: " << percent
                  << "%";
  DrawLegendChip(canvas, typeface, progress_stream.str(), ChipAlignment::kLeft,
                 width, height);

  std::ostringstream verb_stream;
  verb_stream << "Verb: ";
  if (active_segment < segments.size()) {
    verb_stream << active_segment << " of " << segments.size() << " ("
                << VerbName(segments[active_segment].verb) << ")";
  } else {
    verb_stream << "—";
  }
  DrawLegendChip(canvas, typeface, verb_stream.str(), ChipAlignment::kRight,
                 width, height);
}

} // namespace

ShapeTracingFiddle::ShapeTracingFiddle() = default;

ShapeTracingFiddle::~ShapeTracingFiddle() = default;

bool ShapeTracingFiddle::EnsureResources() {
  if (webgl_ != nullptr && typeface_ != nullptr) {
    return true;
  }
  if (initialization_attempted_) {
    return false;
  }
  initialization_attempted_ = true;

  sk_sp<SkFontMgr> font_manager = SkiaFontManager::Instance().FontManager();
  if (font_manager == nullptr || font_manager->countFamilies() == 0) {
    std::cerr << "[cc-engine/stderr] Shape tracing could not access the shared "
                 "font manager."
              << std::endl;
    return false;
  }
  SkString default_family;
  font_manager->getFamilyName(0, &default_family);
  typeface_ = font_manager->matchFamilyStyle(default_family.c_str(),
                                             SkFontStyle::Normal());
  if (typeface_ == nullptr) {
    std::cerr << "[cc-engine/stderr] Shape tracing could not resolve the "
                 "default typeface."
              << std::endl;
    return false;
  }

  auto webgl = std::make_unique<WebGlCanvasContext>();
  if (!webgl->Initialize(WebGlResource())) {
    return false;
  }
  webgl_ = std::move(webgl);
  return true;
}

bool ShapeTracingFiddle::RebuildLetterPath(float width, float height) {
  const SkFont source_font(typeface_, kGlyphSourceSize);
  const SkGlyphID glyph = source_font.unicharToGlyph('B');
  const std::optional<SkPath> source_path = source_font.getPath(glyph);
  if (!source_path.has_value() || source_path->isEmpty()) {
    return false;
  }

  const SkRect source_bounds = source_path->getBounds();
  if (source_bounds.isEmpty()) {
    return false;
  }
  const float scale = std::min(width * 0.80F / source_bounds.width(),
                               height * 0.80F / source_bounds.height());
  const float translate_x = width * 0.5F - source_bounds.centerX() * scale;
  const float translate_y = height * 0.5F - source_bounds.centerY() * scale;
  const SkMatrix placement =
      SkMatrix::ScaleTranslate(scale, scale, translate_x, translate_y);
  source_path->transform(placement, &letter_path_);

  if (!RebuildAtomicSegments()) {
    return false;
  }

  cached_width_ = static_cast<int>(width);
  cached_height_ = static_cast<int>(height);
  return total_length_ > 0.0F;
}

bool ShapeTracingFiddle::RebuildAtomicSegments() {
  atomic_segments_.clear();
  total_length_ = 0.0F;
  SkPoint contour_start = {0.0F, 0.0F};
  SkPoint current_point = {0.0F, 0.0F};
  bool has_contour = false;

  const auto append_segment = [&](SkPath path, SkPath::Verb verb) {
    SkPathMeasure measure(path, false, 2.0F);
    const float length = measure.getLength();
    if (length <= 0.0F) {
      return;
    }
    const float hue = SegmentHue(atomic_segments_.size());
    atomic_segments_.push_back({std::move(path), verb, length, hue});
    total_length_ += length;
  };

  SkPath::RawIter iterator(letter_path_);
  SkPoint points[4];
  for (SkPath::Verb verb = iterator.next(points); verb != SkPath::kDone_Verb;
       verb = iterator.next(points)) {
    SkPathBuilder segment;
    switch (verb) {
    case SkPath::kMove_Verb:
      contour_start = points[0];
      current_point = points[0];
      has_contour = true;
      break;
    case SkPath::kLine_Verb:
      segment.moveTo(points[0]);
      segment.lineTo(points[1]);
      append_segment(segment.detach(), verb);
      current_point = points[1];
      break;
    case SkPath::kQuad_Verb:
      segment.moveTo(points[0]);
      segment.quadTo(points[1], points[2]);
      append_segment(segment.detach(), verb);
      current_point = points[2];
      break;
    case SkPath::kConic_Verb:
      segment.moveTo(points[0]);
      segment.conicTo(points[1], points[2], iterator.conicWeight());
      append_segment(segment.detach(), verb);
      current_point = points[2];
      break;
    case SkPath::kCubic_Verb:
      segment.moveTo(points[0]);
      segment.cubicTo(points[1], points[2], points[3]);
      append_segment(segment.detach(), verb);
      current_point = points[3];
      break;
    case SkPath::kClose_Verb:
      if (has_contour && current_point != contour_start) {
        segment.moveTo(current_point);
        segment.lineTo(contour_start);
        append_segment(segment.detach(), verb);
      }
      current_point = contour_start;
      has_contour = false;
      break;
    case SkPath::kDone_Verb:
      break;
    }
  }

  return !atomic_segments_.empty() && total_length_ > 0.0F;
}

void ShapeTracingFiddle::Render(double time_seconds) {
  if (!EnsureResources()) {
    return;
  }

  const int width = PixelWidth();
  const int height = PixelHeight();
  if (!UpdateState(time_seconds, width, height)) {
    return;
  }
  SkSurface *surface = webgl_->AcquireSurface(width, height);
  if (surface == nullptr) {
    return;
  }
  DrawFrame(surface->getCanvas(), width, height);

  const WebGlPresentResult present = webgl_->FlushAndPresent();
  if (!present.success) {
    std::cerr << "[cc-engine/stderr] Shape tracing could not submit its WebGL "
                 "frame."
              << std::endl;
  }
}

bool ShapeTracingFiddle::UpdateState(double time_seconds, int width,
                                     int height) {
  time_seconds_ = time_seconds;
  if (letter_path_.isEmpty() || cached_width_ != width ||
      cached_height_ != height) {
    if (!RebuildLetterPath(static_cast<float>(width),
                           static_cast<float>(height))) {
      return false;
    }
  }
  return true;
}

void ShapeTracingFiddle::DrawFrame(SkCanvas *canvas, int width, int height) {
  const float progress = static_cast<float>(
      std::fmod(time_seconds_, kTraceCycleSeconds) / kTraceCycleSeconds);
  const float distance = progress * total_length_;
  const TracePosition trace =
      PositionAtCombinedDistance(atomic_segments_, distance);
  const float canvas_width = static_cast<float>(width);
  const float canvas_height = static_cast<float>(height);
  const float shortest = std::min(canvas_width, canvas_height);

  canvas->clear(kCanvasColor);

  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor4f(kLetterColor);
  canvas->drawPath(letter_path_, paint);

  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeWidth(std::clamp(shortest * 0.0025F, 1.5F, 3.5F));
  paint.setColor4f(kLetterOutlineColor);
  canvas->drawPath(letter_path_, paint);

  DrawAtomicSegments(canvas, atomic_segments_, trace.segment_index, shortest);

  const float marker_radius = std::clamp(shortest * 0.0075F, 5.0F, 10.0F);
  DrawContourMarkers(canvas, letter_path_, marker_radius);
  DrawContourStartEyes(canvas, letter_path_, marker_radius * 1.35F);
  DrawMovingArrow(canvas, trace, shortest);
  DrawLegends(canvas, typeface_, total_length_, progress * 100.0F,
              atomic_segments_, trace.segment_index, canvas_width,
              canvas_height);
}
