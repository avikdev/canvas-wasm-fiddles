#pragma once

#include <array>
#include <memory>

#include "core/fiddle_base.h"
#include "include/core/SkPath.h"
#include "include/core/SkRefCnt.h"

class SkTypeface;
class WebGlCanvasContext;

class PuckerBloatFiddle final : public FiddleBaseWebGL {
public:
  PuckerBloatFiddle();
  ~PuckerBloatFiddle() override;

  bool IsSvgWritable() const override { return true; }
  void Render(double time_seconds) override;

private:
  bool EnsureResources();
  void DrawFrame(SkCanvas *canvas, int width, int height) override;

  std::unique_ptr<WebGlCanvasContext> webgl_;
  sk_sp<SkTypeface> typeface_;
  std::array<SkPath, 6> source_shapes_;
  bool initialization_attempted_ = false;
  double time_seconds_ = 0.0;
};
