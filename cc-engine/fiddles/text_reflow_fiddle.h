#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "core/fiddle_base.h"
#include "include/core/SkRefCnt.h"

class SkTypeface;
class WebGlCanvasContext;

namespace skia::textlayout {
class FontCollection;
class Paragraph;
} // namespace skia::textlayout

class TextReflowFiddle final : public FiddleBaseWebGL {
public:
  TextReflowFiddle();
  ~TextReflowFiddle() override;

  bool IsSvgWritable() const override { return true; }
  std::vector<FiddleWidget> Widgets() const override;
  bool SetInput(const std::string &name, const std::string &value) override;
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
  float height_multiplier_ = 1.0F;
  float letter_spacing_ = 0.0F;
  float word_spacing_ = 0.0F;
  float baseline_shift_ = 0.0F;
  float current_width_ratio_ = 0.75F;
  int current_alignment_index_ = 0;
  int current_font_index_ = 0;
  std::string text_ =
      "Ideas do not always ask for more room. They learn the shape of the "
      "space, find a new line, and keep their meaning as the edges move.";
};
