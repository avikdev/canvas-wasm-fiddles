#pragma once

#include <array>
#include <memory>
#include <vector>

#include "core/fiddle_base.h"
#include "include/core/SkRefCnt.h"

class SkTypeface;
class WebGlCanvasContext;

namespace skia::textlayout {
class FontCollection;
class Paragraph;
} // namespace skia::textlayout

class ElasticTextFiddle final : public FiddleBaseWebGL {
public:
  ElasticTextFiddle();
  ~ElasticTextFiddle() override;

  bool IsSvgWritable() const override { return true; }
  void Render(double time_seconds) override;

private:
  bool EnsureResources();
  bool UpdateState(double time_seconds, int width, int height);
  void DrawFrame(SkCanvas *canvas, int width, int height) override;
  bool RebuildParagraphs(float font_size, float gradient_width);

  std::unique_ptr<WebGlCanvasContext> webgl_;
  sk_sp<skia::textlayout::FontCollection> font_collection_;
  sk_sp<SkTypeface> label_typeface_;
  sk_sp<SkTypeface> bold_label_typeface_;
  std::array<std::array<std::unique_ptr<skia::textlayout::Paragraph>, 9>, 3>
      paragraphs_;
  std::vector<int> alignment_sequence_;
  bool initialization_attempted_ = false;
  float paragraph_font_size_ = 0.0F;
  float paragraph_gradient_width_ = 0.0F;
  float current_width_ratio_ = 0.75F;
  int current_alignment_index_ = 0;
  int current_font_index_ = 0;
};
