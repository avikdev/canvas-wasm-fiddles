#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "core/fiddle_base.h"
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

class TextTracingFiddle final : public FiddleBaseWebGL {
public:
  TextTracingFiddle();
  ~TextTracingFiddle() override;

  bool IsSvgWritable() const override { return true; }
  std::vector<FiddleWidget> Widgets() const override;
  bool SetInput(const std::string &name, const std::string &value) override;
  void Render(double time_seconds) override;

private:
  bool EnsureResources();
  bool UpdateState(double time_seconds, int width, int height);
  void DrawFrame(SkCanvas *canvas, int width, int height) override;
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
  double time_seconds_ = 0.0;
  char letter_ = 'B';
};
