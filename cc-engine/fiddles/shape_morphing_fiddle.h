#pragma once

#include <memory>
#include <vector>

#include "fiddle_base.h"
#include "include/core/SkPath.h"
#include "include/core/SkRefCnt.h"
#include "morphing/shape_morpher.h"

class SkTypeface;
class WebGlCanvasContext;

class ShapeMorphingFiddle final : public FiddleBase {
public:
  ShapeMorphingFiddle();
  ~ShapeMorphingFiddle() override;

  void Render(double time_seconds) override;

protected:
  bool UsesWebGl() const override;

private:
  bool EnsureResources();
  bool RebuildGlyphs(float width, float height);

  std::unique_ptr<WebGlCanvasContext> webgl_;
  sk_sp<SkTypeface> typeface_;
  SkPath source_path_;
  SkPath target_path_;
  skmorph::ShapeMorpher morpher_;
  std::vector<skmorph::ContourStartPoints> source_diagnostics_;
  std::vector<skmorph::ContourStartPoints> target_diagnostics_;
  char source_letter_ = 'A';
  char target_letter_ = 'B';
  bool initialization_attempted_ = false;
  int cached_width_ = 0;
  int cached_height_ = 0;
};
