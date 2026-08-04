#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include "core/fiddle_base.h"
#include "include/core/SkPath.h"
#include "include/core/SkPoint.h"
#include "include/core/SkRefCnt.h"
#include "text/font_cycle.h"

class WebGlCanvasContext;
class SkTypeface;

namespace skia::textlayout {
class FontCollection;
} // namespace skia::textlayout

struct EnvelopeTextContour {
  std::vector<SkPoint> normalized_points;
};

struct EnvelopeTextShape {
  std::vector<EnvelopeTextContour> contours;
  SkPathFillType fill_type = SkPathFillType::kWinding;
};

struct EnvelopeDemoCell {
  int envelope_kind = 0;
  std::uint32_t seed = 0U;
  float phase = 0.0F;
};

class EnvelopeDistortFiddle final : public FiddleBaseWebGL {
public:
  EnvelopeDistortFiddle();
  ~EnvelopeDistortFiddle() override;

  void Render(double time_seconds) override;

private:
  bool EnsureResources();
  bool BuildTextShapes();
  bool RebuildGrid(float width, float height);

  std::unique_ptr<WebGlCanvasContext> webgl_;
  sk_sp<skia::textlayout::FontCollection> font_collection_;
  sk_sp<SkTypeface> label_typeface_;
  std::array<std::array<EnvelopeTextShape, 7>, text::kFontChoices.size()>
      text_shapes_;
  std::vector<EnvelopeDemoCell> cells_;
  std::uint32_t field_seed_ = 0U;
  float cell_width_ = 1.0F;
  float cell_height_ = 1.0F;
  int column_count_ = 1;
  int row_count_ = 1;
  bool initialization_attempted_ = false;
  bool text_shapes_ready_ = false;
  int cached_width_ = 0;
  int cached_height_ = 0;
};
