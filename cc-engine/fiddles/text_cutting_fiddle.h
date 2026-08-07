#pragma once

#include <array>
#include <memory>

#include "core/fiddle_base.h"
#include "include/core/SkPath.h"
#include "include/core/SkRefCnt.h"

class WebGlCanvasContext;
class SkPathEffect;

namespace skia::textlayout {
class FontCollection;
} // namespace skia::textlayout

class TextCuttingFiddle final : public FiddleBaseWebGL {
public:
  TextCuttingFiddle();
  ~TextCuttingFiddle() override;

  bool IsSvgWritable() const override { return true; }
  void Render(double time_seconds) override;

private:
  bool EnsureResources();
  bool UpdateState(double time_seconds, int width, int height);
  void DrawFrame(SkCanvas *canvas, int width, int height) override;
  bool RebuildLetterPaths(int font_index, float width, float height);

  std::unique_ptr<WebGlCanvasContext> webgl_;
  sk_sp<skia::textlayout::FontCollection> font_collection_;
  sk_sp<SkPathEffect> corner_path_effect_;
  std::array<SkPath, 5> letter_paths_;
  std::array<SkPath, 5> letter_stroke_paths_;
  bool initialization_attempted_ = false;
  int cached_font_index_ = -1;
  int cached_width_ = 0;
  int cached_height_ = 0;
  double time_seconds_ = 0.0;
};
