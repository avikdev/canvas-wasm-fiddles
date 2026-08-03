#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "fiddle_base.h"
#include "include/core/SkPath.h"
#include "include/core/SkRefCnt.h"

class SkTypeface;
class WebGlCanvasContext;

struct TracedPathSegment {
  SkPath path;
  SkPath::Verb verb = SkPath::kDone_Verb;
  float length = 0.0F;
  float hue = 0.0F;
};

class ShapeTracingFiddle final : public FiddleBase {
public:
  ShapeTracingFiddle();
  ~ShapeTracingFiddle() override;

  void Render(double time_seconds) override;

protected:
  bool UsesWebGl() const override;

private:
  bool EnsureResources();
  bool RebuildLetterPath(float width, float height);
  bool RebuildAtomicSegments();

  std::unique_ptr<WebGlCanvasContext> webgl_;
  sk_sp<SkTypeface> typeface_;
  SkPath letter_path_;
  std::vector<TracedPathSegment> atomic_segments_;
  float total_length_ = 0.0F;
  bool initialization_attempted_ = false;
  int cached_width_ = 0;
  int cached_height_ = 0;
};
