#pragma once

#include <string>
#include <vector>

#include "include/core/SkPath.h"
#include "include/core/SkRefCnt.h"

namespace skia::textlayout {
class FontCollection;
}

namespace text {

struct ShapedGlyph {
  // Outline coordinates are relative to the line origin. The y=0 axis is
  // halfway between the baseline and the font's x-height (mean line).
  SkPath outline;
  float origin_x = 0.0F;
  float advance = 0.0F;
};

struct ShapedTextLine {
  std::string text;
  std::vector<ShapedGlyph> glyphs;
  float advance = 0.0F;
  // Maximum equal distance above and below the y=0 alignment axis needed to
  // contain the font metrics of every run in the shaped line.
  float symmetric_half_height = 0.0F;

  bool valid() const { return !glyphs.empty() && advance > 0.0F; }
};

// Collapses ASCII whitespace (including line breaks) to one ordinary space
// and removes leading/trailing whitespace. UTF-8 bytes are otherwise retained.
std::string NormalizeSingleLineText(const std::string &input);

// Shapes a single unwrapped LTR line through SkParagraph/HarfBuzz and extracts
// positioned vector glyph outlines. The common y=0 alignment axis is halfway
// between the baseline and mean line, with a symmetric vertical metric box.
bool ShapeTextLine(const std::string &input, const std::string &font_family,
                   float font_size,
                   const sk_sp<skia::textlayout::FontCollection> &fonts,
                   ShapedTextLine *result);

} // namespace text
