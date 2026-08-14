#pragma once

#include <memory>
#include <string>
#include <vector>

#include "core/fiddle_base.h"
#include "include/core/SkPath.h"
#include "include/core/SkRefCnt.h"
#include "text/shaped_text_line.h"

class WebGlCanvasContext;

namespace geometry {
class PathTextDeformer;
}

namespace skia::textlayout {
class FontCollection;
}

class TextOnCurveFiddle final : public FiddleBaseWebGL {
public:
  TextOnCurveFiddle();
  ~TextOnCurveFiddle() override;

  bool IsSvgWritable() const override { return true; }
  std::vector<FiddleWidget> Widgets() const override;
  bool SetInput(const std::string &key, const std::string &value) override;
  void Render(double time_seconds) override;

private:
  bool EnsureResources();
  bool RebuildGuide(int width, int height);
  bool RebuildText(int width, int height);
  void DrawFrame(SkCanvas *canvas, int width, int height) override;
  std::string PostProcessSvg(std::string svg) const override;

  std::unique_ptr<WebGlCanvasContext> webgl_;
  sk_sp<skia::textlayout::FontCollection> font_collection_;
  std::unique_ptr<geometry::PathTextDeformer> deformer_;
  SkPath guide_path_;
  text::ShapedTextLine shaped_line_;
  bool initialization_attempted_ = false;
  bool guide_dirty_ = true;
  bool text_dirty_ = true;
  int cached_width_ = 0;
  int cached_height_ = 0;
  double previous_frame_time_seconds_ = 0.0;
  bool has_previous_frame_time_ = false;
  float motion_offset_ = 0.0F;
  float effective_font_size_ = 16.0F;

  std::string text_ =
      "Love is a sudden turn in a road you thought was straight.";
  std::string path_string_ =
      "M 50 92 L 12 55 C -8 35 5 10 27 10 A 23 23 0 0 1 50 27 A 23 23 "
      "0 0 1 73 10 C 95 10 108 35 88 55 Z";
  std::string font_face_ = "default (Roboto)";
  bool use_fixed_font_size_ = false;
  float font_size_ = 16.0F;
  float animation_speed_ = 50.0F;
  bool black_text_ = false;
  bool protect_sharp_turns_ = true;
  float sharp_turn_safety_ = 0.80F;
  float corner_transition_ems_ = 1.5F;
  float subdivision_tolerance_ = 0.35F;
};
