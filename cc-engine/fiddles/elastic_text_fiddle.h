#pragma once

#include <array>
#include <memory>
#include <vector>

#include "fiddle_base.h"
#include "include/core/SkRefCnt.h"

class SkTypeface;
class WebGlCanvasContext;

namespace skia::textlayout {
class FontCollection;
class Paragraph;
} // namespace skia::textlayout

class ElasticTextFiddle final : public FiddleBase {
public:
  ElasticTextFiddle();
  ~ElasticTextFiddle() override;

  void Render(double time_seconds) override;

protected:
  bool UsesWebGl() const override;

private:
  bool EnsureResources();
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
};
