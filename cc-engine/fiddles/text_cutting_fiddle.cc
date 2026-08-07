#include "fiddles/text_cutting_fiddle.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "graphics/webgl_canvas_context.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkMatrix.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkPathEffect.h"
#include "include/core/SkPathUtils.h"
#include "include/core/SkPoint.h"
#include "include/core/SkRect.h"
#include "include/core/SkString.h"
#include "include/core/SkStrokeRec.h"
#include "include/core/SkSurface.h"
#include "include/effects/SkCornerPathEffect.h"
#include "include/pathops/SkPathOps.h"
#include "modules/skparagraph/include/FontCollection.h"
#include "modules/skparagraph/include/Paragraph.h"
#include "modules/skparagraph/include/ParagraphBuilder.h"
#include "modules/skparagraph/include/ParagraphStyle.h"
#include "modules/skparagraph/include/TextStyle.h"
#include "modules/skunicode/include/SkUnicode_icu.h"
#include "text/font_cycle.h"
#include "text/skia_font_manager.h"
#include "utils/color_utils.h"

namespace {

using skia::textlayout::Paragraph;
using skia::textlayout::ParagraphBuilder;
using skia::textlayout::ParagraphStyle;
using skia::textlayout::TextAlign;
using skia::textlayout::TextDirection;
using skia::textlayout::TextStyle;

constexpr int kDesktopRibbonCount = 16;
constexpr int kPhoneRibbonCount = kDesktopRibbonCount / 2;
constexpr int kLetterCount = 5;
constexpr float kPhoneWidthThreshold = 600.0F;
constexpr float kRibbonGap = 4.0F;
constexpr float kHalfRibbonGap = kRibbonGap * 0.5F;
constexpr float kPieceCornerRadius = 12.0F;
constexpr float kMinimumPieceDimension = 2.0F;
constexpr float kMinimumPieceBoundsArea = 8.0F;
constexpr float kRibbonSaturation = 0.40F;
constexpr float kRibbonValue = 0.72F;
constexpr float kLetterSaturation = 1.0F;
constexpr float kLetterValue = 0.96F;
constexpr float kWaveSegmentWidth = 32.0F;
constexpr double kFontCycleSeconds = 5.0;
constexpr char kWord[] = "Hello";
constexpr SkColor kCanvasColor = SK_ColorWHITE;

struct WaveGeometry {
  float spacing;
  float amplitude;
};

struct ColoredPiece {
  SkPath path;
  SkColor4f color;
};

WaveGeometry GetWaveGeometry(float height, int ribbon_count) {
  const float usable_height = std::max(1.0F, height - kRibbonGap * 2.0F);
  float total_weight = 0.0F;
  for (int ribbon = 0; ribbon < ribbon_count; ++ribbon) {
    const float distance_from_middle =
        std::abs(static_cast<float>(ribbon) -
                 (static_cast<float>(ribbon_count) - 1.0F) * 0.5F);
    const float normalized_distance =
        std::clamp((distance_from_middle - 0.5F) /
                       ((static_cast<float>(ribbon_count) - 2.0F) * 0.5F),
                   0.0F, 1.0F);
    total_weight += 1.0F + normalized_distance;
  }
  const float spacing = usable_height / total_weight;
  return {spacing, std::min(48.0F, spacing * 0.75F)};
}

float RibbonWeight(int ribbon, int ribbon_count) {
  const float distance_from_middle =
      std::abs(static_cast<float>(ribbon) -
               (static_cast<float>(ribbon_count) - 1.0F) * 0.5F);
  const float normalized_distance =
      std::clamp((distance_from_middle - 0.5F) /
                     ((static_cast<float>(ribbon_count) - 2.0F) * 0.5F),
                 0.0F, 1.0F);
  return 1.0F + normalized_distance;
}

float BoundaryBaseY(int boundary, float height, int ribbon_count) {
  const WaveGeometry geometry = GetWaveGeometry(height, ribbon_count);
  float weighted_position = 0.0F;
  for (int ribbon = 0; ribbon <= boundary; ++ribbon) {
    weighted_position += RibbonWeight(ribbon, ribbon_count);
  }
  return kRibbonGap + geometry.spacing * weighted_position;
}

float BoundaryY(int boundary, float x, double time_seconds, float height,
                int ribbon_count) {
  const WaveGeometry geometry = GetWaveGeometry(height, ribbon_count);
  const float line = static_cast<float>(boundary);
  const float time = static_cast<float>(time_seconds);
  const float first_phase = x * 0.0105F + time * 0.42F;
  const float second_phase = x * 0.022F - time * 0.31F;
  const float detail_phase = x * 0.016F + time * 0.21F + line * 0.45F;
  const float detail_amplitude =
      std::min(geometry.amplitude * 0.08F, geometry.spacing * 0.08F);
  const float wave = std::sin(first_phase) * geometry.amplitude +
                     std::sin(second_phase) * geometry.amplitude * 0.36F +
                     std::sin(detail_phase) * detail_amplitude;
  return BoundaryBaseY(boundary, height, ribbon_count) + wave;
}

float BoundarySlope(int boundary, float x, double time_seconds, float height,
                    int ribbon_count) {
  const WaveGeometry geometry = GetWaveGeometry(height, ribbon_count);
  const float line = static_cast<float>(boundary);
  const float time = static_cast<float>(time_seconds);
  const float first_phase = x * 0.0105F + time * 0.42F;
  const float second_phase = x * 0.022F - time * 0.31F;
  const float detail_phase = x * 0.016F + time * 0.21F + line * 0.45F;
  const float detail_amplitude =
      std::min(geometry.amplitude * 0.08F, geometry.spacing * 0.08F);
  return std::cos(first_phase) * geometry.amplitude * 0.0105F +
         std::cos(second_phase) * geometry.amplitude * 0.36F * 0.022F +
         std::cos(detail_phase) * detail_amplitude * 0.016F;
}

void AppendWave(SkPathBuilder *builder, int boundary, float start_x,
                float end_x, float vertical_offset, double time_seconds,
                float height, int ribbon_count) {
  const float distance = end_x - start_x;
  const int segment_count = std::max(
      1, static_cast<int>(std::ceil(std::abs(distance) / kWaveSegmentWidth)));
  float x0 = start_x;
  float y0 = BoundaryY(boundary, x0, time_seconds, height, ribbon_count) +
             vertical_offset;
  float slope0 =
      BoundarySlope(boundary, x0, time_seconds, height, ribbon_count);

  for (int segment = 1; segment <= segment_count; ++segment) {
    const float ratio =
        static_cast<float>(segment) / static_cast<float>(segment_count);
    const float x1 = std::lerp(start_x, end_x, ratio);
    const float y1 =
        BoundaryY(boundary, x1, time_seconds, height, ribbon_count) +
        vertical_offset;
    const float slope1 =
        BoundarySlope(boundary, x1, time_seconds, height, ribbon_count);
    const float delta_x = x1 - x0;
    builder->cubicTo(x0 + delta_x / 3.0F, y0 + slope0 * delta_x / 3.0F,
                     x1 - delta_x / 3.0F, y1 - slope1 * delta_x / 3.0F, x1, y1);
    x0 = x1;
    y0 = y1;
    slope0 = slope1;
  }
}

SkPath BuildRibbonPath(int ribbon, float width, float height,
                       double time_seconds, int ribbon_count) {
  const float left = kRibbonGap;
  const float right = std::max(left, width - kRibbonGap);
  const float top = ribbon == 0 ? kRibbonGap
                                : BoundaryY(ribbon - 1, left, time_seconds,
                                            height, ribbon_count) +
                                      kHalfRibbonGap;
  const float bottom =
      ribbon == ribbon_count - 1
          ? height - kRibbonGap
          : BoundaryY(ribbon, right, time_seconds, height, ribbon_count) -
                kHalfRibbonGap;

  SkPathBuilder builder;
  builder.moveTo(left, top);
  if (ribbon == 0) {
    builder.lineTo(right, kRibbonGap);
  } else {
    AppendWave(&builder, ribbon - 1, left, right, kHalfRibbonGap, time_seconds,
               height, ribbon_count);
  }

  builder.lineTo(right, bottom);
  if (ribbon == ribbon_count - 1) {
    builder.lineTo(left, height - kRibbonGap);
  } else {
    AppendWave(&builder, ribbon, right, left, -kHalfRibbonGap, time_seconds,
               height, ribbon_count);
  }
  builder.close();
  return builder.detach();
}

SkRect BoundsForPaths(const std::array<SkPath, kLetterCount> &paths) {
  SkRect bounds = SkRect::MakeEmpty();
  for (const SkPath &path : paths) {
    if (!path.isEmpty()) {
      bounds.join(path.getBounds());
    }
  }
  return bounds;
}

int LetterIndexForUtf8Offset(std::uint32_t offset) {
  return std::clamp(static_cast<int>(offset), 0, kLetterCount - 1);
}

SkPath NormalizeFilledPath(const SkPath &path) {
  const std::optional<SkPath> simplified = Simplify(path);
  return simplified.has_value() ? std::move(*simplified) : path;
}

bool IsRenderablePiece(const SkPath &path) {
  if (path.isEmpty()) {
    return false;
  }
  const SkRect bounds = path.getBounds();
  return bounds.width() >= kMinimumPieceDimension &&
         bounds.height() >= kMinimumPieceDimension &&
         bounds.width() * bounds.height() >= kMinimumPieceBoundsArea;
}

std::optional<SkPath> RoundedPiece(const SkPath &path,
                                   const SkPathEffect &effect) {
  SkStrokeRec stroke_record(SkStrokeRec::kFill_InitStyle);
  SkPathBuilder builder;
  if (!effect.filterPath(&builder, path, &stroke_record)) {
    return std::nullopt;
  }
  SkPath rounded = NormalizeFilledPath(builder.detach());
  if (stroke_record.needToApply()) {
    SkPathBuilder stroked_builder;
    if (!stroke_record.applyToPath(&stroked_builder, rounded)) {
      return std::nullopt;
    }
    rounded = NormalizeFilledPath(stroked_builder.detach());
  }
  if (!IsRenderablePiece(rounded)) {
    return std::nullopt;
  }
  return rounded;
}

void AppendRoundedPiece(std::vector<ColoredPiece> *pieces, SkPath path,
                        SkColor4f color, const SkPathEffect &effect) {
  std::optional<SkPath> rounded = RoundedPiece(path, effect);
  if (rounded.has_value()) {
    pieces->push_back({std::move(*rounded), color});
  }
}

template <std::size_t Size>
SkPath UnionFilledPaths(const std::array<SkPath, Size> &paths) {
  SkPath united;
  bool has_path = false;
  for (const SkPath &path : paths) {
    if (path.isEmpty()) {
      continue;
    }
    if (!has_path) {
      united = path;
      has_path = true;
      continue;
    }
    const std::optional<SkPath> result = Op(united, path, kUnion_SkPathOp);
    if (result.has_value()) {
      united = std::move(*result);
    } else {
      SkPathBuilder fallback;
      fallback.addPath(united);
      fallback.addPath(path);
      united = fallback.detach();
    }
  }
  return NormalizeFilledPath(united);
}

SkColor4f GeneratedColor(int index, int color_count, float saturation,
                         float value) {
  const float hue =
      360.0F * static_cast<float>(index) / static_cast<float>(color_count);
  return color_utils::FromHsv(hue, saturation, value);
}

} // namespace

TextCuttingFiddle::TextCuttingFiddle() = default;

TextCuttingFiddle::~TextCuttingFiddle() = default;

bool TextCuttingFiddle::EnsureResources() {
  if (webgl_ != nullptr && font_collection_ != nullptr &&
      corner_path_effect_ != nullptr) {
    return true;
  }
  if (initialization_attempted_) {
    return false;
  }
  initialization_attempted_ = true;

  auto webgl = std::make_unique<WebGlCanvasContext>();
  if (!webgl->Initialize(WebGlResource())) {
    return false;
  }

  sk_sp<SkFontMgr> font_manager = SkiaFontManager::Instance().FontManager();
  if (font_manager == nullptr || font_manager->countFamilies() == 0) {
    std::cerr << "[cc-engine/stderr] Text Cutting could not access the shared "
                 "font manager."
              << std::endl;
    return false;
  }

  font_collection_ = sk_make_sp<skia::textlayout::FontCollection>();
  font_collection_->setDefaultFontManager(font_manager, "Roboto");
  corner_path_effect_ = SkCornerPathEffect::Make(kPieceCornerRadius);
  if (corner_path_effect_ == nullptr) {
    return false;
  }
  webgl_ = std::move(webgl);
  std::cout << "[cc-engine/stdout] Text Cutting Skia path, paragraph, and "
               "Path Ops resources ready."
            << std::endl;
  return true;
}

bool TextCuttingFiddle::RebuildLetterPaths(int font_index, float width,
                                           float height) {
  sk_sp<SkUnicode> unicode = SkUnicodes::ICU::Make();
  if (unicode == nullptr) {
    return false;
  }

  const text::FontChoice &font_choice = text::kFontChoices[font_index];
  const std::string family_name = text::ResolveFontFamily(font_choice);

  TextStyle text_style;
  if (family_name.empty()) {
    text_style.setFontFamilies({});
  } else {
    text_style.setFontFamilies({SkString(family_name.c_str())});
  }
  const float nominal_font_size = std::max(width, height) * 0.75F;
  text_style.setFontSize(nominal_font_size);
  text_style.setColor(SK_ColorWHITE);
  text_style.addFontFeature(SkString("liga"), 0);
  text_style.addFontFeature(SkString("clig"), 0);

  ParagraphStyle paragraph_style;
  paragraph_style.setTextDirection(TextDirection::kLtr);
  paragraph_style.setTextAlign(TextAlign::kLeft);
  paragraph_style.setFakeMissingFontStyles(true);

  auto builder =
      ParagraphBuilder::make(paragraph_style, font_collection_, unicode);
  if (builder == nullptr) {
    return false;
  }
  builder->pushStyle(text_style);
  builder->addText(kWord);
  builder->pop();
  std::unique_ptr<Paragraph> paragraph = builder->Build();
  if (paragraph == nullptr) {
    return false;
  }
  paragraph->layout(nominal_font_size * 8.0F);

  std::array<SkPathBuilder, kLetterCount> letter_builders;
  paragraph->visit([&letter_builders](int, const Paragraph::VisitorInfo *info) {
    if (info == nullptr) {
      return;
    }
    for (int glyph = 0; glyph < info->count; ++glyph) {
      const std::optional<SkPath> glyph_path =
          info->font.getPath(info->glyphs[glyph]);
      if (!glyph_path.has_value() || glyph_path->isEmpty()) {
        continue;
      }
      const int letter_index =
          LetterIndexForUtf8Offset(info->utf8Starts[glyph]);
      const SkPoint position =
          SkPoint::Make(info->positions[glyph].x() + info->origin.x(),
                        info->positions[glyph].y() + info->origin.y());
      SkPath positioned_path;
      glyph_path->transform(SkMatrix::Translate(position.x(), position.y()),
                            &positioned_path);
      letter_builders[letter_index].addPath(
          NormalizeFilledPath(positioned_path));
    }
  });

  std::array<SkPath, kLetterCount> raw_paths;
  for (int letter = 0; letter < kLetterCount; ++letter) {
    raw_paths[letter] = NormalizeFilledPath(letter_builders[letter].detach());
  }

  const SkRect text_bounds = BoundsForPaths(raw_paths);
  if (text_bounds.isEmpty() || text_bounds.width() <= 0.0F ||
      text_bounds.height() <= 0.0F) {
    return false;
  }
  const float scale = std::min(width * 0.80F / text_bounds.width(),
                               height * 0.80F / text_bounds.height());
  const float translate_x = width * 0.5F - text_bounds.centerX() * scale;
  const float translate_y = height * 0.5F - text_bounds.centerY() * scale;
  const SkMatrix placement =
      SkMatrix::ScaleTranslate(scale, scale, translate_x, translate_y);

  SkPaint stroke_paint;
  stroke_paint.setStyle(SkPaint::kStroke_Style);
  stroke_paint.setStrokeWidth(kRibbonGap);
  stroke_paint.setStrokeCap(SkPaint::kRound_Cap);
  stroke_paint.setStrokeJoin(SkPaint::kRound_Join);

  for (int letter = 0; letter < kLetterCount; ++letter) {
    SkPath placed_path;
    raw_paths[letter].transform(placement, &placed_path);
    placed_path = NormalizeFilledPath(placed_path);
    letter_stroke_paths_[letter] = NormalizeFilledPath(
        skpathutils::FillPathWithPaint(placed_path, stroke_paint));
    const std::optional<SkPath> inset =
        Op(placed_path, letter_stroke_paths_[letter], kDifference_SkPathOp);
    if (!inset.has_value()) {
      letter_paths_[letter] = std::move(placed_path);
    } else if (IsRenderablePiece(*inset)) {
      letter_paths_[letter] = NormalizeFilledPath(*inset);
    } else {
      letter_paths_[letter].reset();
    }
  }
  cached_font_index_ = font_index;
  cached_width_ = static_cast<int>(width);
  cached_height_ = static_cast<int>(height);
  return true;
}

void TextCuttingFiddle::Render(double time_seconds) {
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
    std::cerr << "[cc-engine/stderr] Text Cutting could not submit its WebGL "
                 "frame."
              << std::endl;
  }
}

bool TextCuttingFiddle::UpdateState(double time_seconds, int width,
                                    int height) {
  time_seconds_ = time_seconds;
  const int font_index =
      text::CyclingFontIndex(time_seconds, kFontCycleSeconds);
  if (cached_font_index_ != font_index || cached_width_ != width ||
      cached_height_ != height) {
    if (!RebuildLetterPaths(font_index, static_cast<float>(width),
                            static_cast<float>(height))) {
      return false;
    }
  }
  return true;
}

void TextCuttingFiddle::DrawFrame(SkCanvas *canvas, int width, int height) {
  const int ribbon_count =
      Width() <= kPhoneWidthThreshold ? kPhoneRibbonCount : kDesktopRibbonCount;
  const SkPath all_letter_strokes = UnionFilledPaths(letter_stroke_paths_);

  std::array<SkPath, kDesktopRibbonCount> ribbons;
  for (int ribbon = 0; ribbon < ribbon_count; ++ribbon) {
    const SkPath original = BuildRibbonPath(ribbon, static_cast<float>(width),
                                            static_cast<float>(height),
                                            time_seconds_, ribbon_count);
    const std::optional<SkPath> without_letter_strokes =
        Op(original, all_letter_strokes, kDifference_SkPathOp);
    ribbons[ribbon] = without_letter_strokes.has_value()
                          ? std::move(*without_letter_strokes)
                          : original;
  }

  const SkPath all_letters = UnionFilledPaths(letter_paths_);

  canvas->clear(kCanvasColor);
  std::vector<ColoredPiece> pieces;
  pieces.reserve(ribbon_count * (kLetterCount + 1));

  for (int ribbon = 0; ribbon < ribbon_count; ++ribbon) {
    const SkColor4f ribbon_color =
        GeneratedColor(ribbon, ribbon_count, kRibbonSaturation, kRibbonValue);
    std::vector<ColoredPiece> ribbon_pieces;
    ribbon_pieces.reserve(kLetterCount + 1);

    const std::optional<SkPath> outside =
        Op(ribbons[ribbon], all_letters, kDifference_SkPathOp);
    if (!outside.has_value() || !IsRenderablePiece(*outside)) {
      if (IsRenderablePiece(ribbons[ribbon])) {
        AppendRoundedPiece(&pieces, ribbons[ribbon], ribbon_color,
                           *corner_path_effect_);
      }
      continue;
    }
    ribbon_pieces.push_back({std::move(*outside), ribbon_color});

    bool path_ops_failed = false;
    for (int letter = 0; letter < kLetterCount; ++letter) {
      if (letter_paths_[letter].isEmpty()) {
        continue;
      }
      if (!SkRect::Intersects(letter_paths_[letter].getBounds(),
                              ribbons[ribbon].getBounds())) {
        continue;
      }
      const std::optional<SkPath> overlap =
          Op(letter_paths_[letter], ribbons[ribbon], kIntersect_SkPathOp);
      if (!overlap.has_value()) {
        path_ops_failed = true;
        break;
      }
      if (!IsRenderablePiece(*overlap)) {
        continue;
      }

      int color_index = letter % 2 == 0 ? ribbon + letter : ribbon - letter;
      color_index = (color_index % ribbon_count + ribbon_count) % ribbon_count;
      if (color_index == ribbon) {
        color_index = (ribbon + 1) % ribbon_count;
      }
      ribbon_pieces.push_back(
          {std::move(*overlap),
           GeneratedColor(color_index, ribbon_count, kLetterSaturation,
                          kLetterValue)});
    }

    if (path_ops_failed) {
      // Do not replace colored letter shards with an intact ribbon for one
      // frame: that fallback was the visible color flash. Retain only the
      // successfully computed pieces from this global ribbon.
      for (ColoredPiece &piece : ribbon_pieces) {
        AppendRoundedPiece(&pieces, std::move(piece.path), piece.color,
                           *corner_path_effect_);
      }
      continue;
    }
    for (ColoredPiece &piece : ribbon_pieces) {
      AppendRoundedPiece(&pieces, std::move(piece.path), piece.color,
                         *corner_path_effect_);
    }
  }

  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kFill_Style);
  for (const ColoredPiece &piece : pieces) {
    paint.setColor4f(piece.color);
    canvas->drawPath(piece.path, paint);
  }
}
