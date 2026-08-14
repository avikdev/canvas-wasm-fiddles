#include "text/shaped_text_line.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>

#include "include/core/SkColor.h"
#include "include/core/SkFontMetrics.h"
#include "include/core/SkMatrix.h"
#include "include/core/SkString.h"
#include "modules/skparagraph/include/FontCollection.h"
#include "modules/skparagraph/include/Paragraph.h"
#include "modules/skparagraph/include/ParagraphBuilder.h"
#include "modules/skparagraph/include/ParagraphStyle.h"
#include "modules/skparagraph/include/TextStyle.h"
#include "modules/skunicode/include/SkUnicode_icu.h"

namespace text {
namespace {

using skia::textlayout::Paragraph;
using skia::textlayout::ParagraphBuilder;
using skia::textlayout::ParagraphStyle;
using skia::textlayout::TextAlign;
using skia::textlayout::TextDirection;
using skia::textlayout::TextStyle;

bool IsAsciiWhitespace(char value) {
  return value == ' ' || value == '\t' || value == '\r' || value == '\n' ||
         value == '\f' || value == '\v';
}

} // namespace

std::string NormalizeSingleLineText(const std::string &input) {
  std::string normalized;
  normalized.reserve(input.size());
  bool pending_space = false;
  for (char value : input) {
    if (IsAsciiWhitespace(value)) {
      pending_space = !normalized.empty();
      continue;
    }
    if (pending_space)
      normalized.push_back(' ');
    normalized.push_back(value);
    pending_space = false;
  }
  return normalized;
}

bool ShapeTextLine(const std::string &input, const std::string &font_family,
                   float font_size,
                   const sk_sp<skia::textlayout::FontCollection> &fonts,
                   ShapedTextLine *result) {
  if (result == nullptr || fonts == nullptr || !std::isfinite(font_size) ||
      font_size <= 0.0F) {
    return false;
  }
  const std::string normalized = NormalizeSingleLineText(input);
  if (normalized.empty())
    return false;
  const sk_sp<SkUnicode> unicode = SkUnicodes::ICU::Make();
  if (unicode == nullptr)
    return false;

  TextStyle text_style;
  if (font_family.empty()) {
    text_style.setFontFamilies({});
  } else {
    text_style.setFontFamilies({SkString(font_family.c_str())});
  }
  text_style.setFontSize(font_size);
  text_style.setColor(SK_ColorBLACK);

  ParagraphStyle paragraph_style;
  paragraph_style.setTextDirection(TextDirection::kLtr);
  paragraph_style.setTextAlign(TextAlign::kLeft);
  paragraph_style.setFakeMissingFontStyles(true);

  auto builder = ParagraphBuilder::make(paragraph_style, fonts, unicode);
  if (builder == nullptr)
    return false;
  builder->pushStyle(text_style);
  builder->addText(normalized.data(), normalized.size());
  builder->pop();
  std::unique_ptr<Paragraph> paragraph = builder->Build();
  if (paragraph == nullptr)
    return false;
  const float shaping_width =
      font_size * std::max(8.0F, static_cast<float>(normalized.size()) * 2.0F);
  paragraph->layout(shaping_width);

  ShapedTextLine shaped;
  shaped.text = normalized;
  shaped.advance = paragraph->getMaxIntrinsicWidth();
  std::optional<float> baseline;
  float ascent = 0.0F;
  float descent = 0.0F;
  float x_height = 0.0F;
  paragraph->visit([&](int, const Paragraph::VisitorInfo *info) {
    if (info == nullptr)
      return;
    SkFontMetrics metrics;
    info->font.getMetrics(&metrics);
    ascent = std::max(ascent, std::max(0.0F, -metrics.fAscent));
    descent = std::max(descent, std::max(0.0F, metrics.fDescent));
    // Skia reports vertical font metrics in canvas coordinates, so x-height
    // is normally negative (above the baseline).
    x_height = std::max(x_height, std::max(0.0F, -metrics.fXHeight));
    if (!baseline.has_value())
      baseline = info->origin.y();
    for (int glyph = 0; glyph < info->count; ++glyph) {
      const std::optional<SkPath> outline =
          info->font.getPath(info->glyphs[glyph]);
      if (!outline.has_value() || outline->isEmpty())
        continue;
      const float origin_x = info->origin.x() + info->positions[glyph].x();
      const float origin_y = info->origin.y() + info->positions[glyph].y() -
                             baseline.value_or(info->origin.y());
      SkPath positioned;
      outline->transform(SkMatrix::Translate(origin_x, origin_y), &positioned);
      const float next_x =
          glyph + 1 < info->count
              ? info->origin.x() + info->positions[glyph + 1].x()
              : info->origin.x() + info->advanceX;
      shaped.glyphs.push_back(
          {std::move(positioned), origin_x, std::max(0.0F, next_x - origin_x)});
    }
  });
  if (!shaped.valid())
    return false;

  // Some fonts do not provide an x-height. Use a conservative approximation
  // in that case, while keeping all other measurements based on real metrics.
  if (ascent <= 0.0F)
    ascent = font_size * 0.8F;
  if (descent <= 0.0F)
    descent = font_size * 0.2F;
  if (x_height <= 0.0F)
    x_height = ascent * 0.5F;

  const float reference_above_baseline = x_height * 0.5F;
  const float distance_to_ascender =
      std::max(0.0F, ascent - reference_above_baseline);
  const float distance_to_descender = descent + reference_above_baseline;
  shaped.symmetric_half_height =
      std::max(distance_to_ascender, distance_to_descender);
  for (ShapedGlyph &glyph : shaped.glyphs) {
    SkPath aligned;
    glyph.outline.transform(SkMatrix::Translate(0.0F, reference_above_baseline),
                            &aligned);
    glyph.outline = std::move(aligned);
  }
  *result = std::move(shaped);
  return true;
}

} // namespace text
