#pragma once

#include <memory>

#include "core/fiddle_base.h"
#include "include/core/SkRefCnt.h"

class SkTypeface;
class WebGlCanvasContext;

class ShapeIntersectionFiddle final : public FiddleBaseWebGL {
public:
  ShapeIntersectionFiddle();
  ~ShapeIntersectionFiddle() override;

  bool IsSvgWritable() const override { return true; }
  std::vector<FiddleWidget> Widgets() const override;
  bool SetInput(const std::string &key, const std::string &value) override;
  void Render(double time_seconds) override;

private:
  bool EnsureWebGl();
  bool UpdateState(double time_seconds, int width, int height);
  void DrawFrame(SkCanvas *canvas, int width, int height) override;

  std::unique_ptr<WebGlCanvasContext> webgl_;
  sk_sp<SkTypeface> label_typeface_;
  bool initialization_attempted_ = false;
  double time_seconds_ = 0.0;
  int shapes_count_ = 10;
  float gap_thickness_ = 2.0F;
};
