#pragma once

#include <memory>
#include <vector>

#include "core/fiddle_base.h"
#include "geometry/curve_interpolate.h"
#include "include/core/SkPath.h"
#include "include/core/SkRefCnt.h"

class SkTypeface;
class WebGlCanvasContext;

class CurveInterpolateFiddle final : public FiddleBaseWebGL {
public:
  CurveInterpolateFiddle();
  ~CurveInterpolateFiddle() override;

  bool IsSvgWritable() const override { return true; }
  std::vector<FiddleWidget> Widgets() const override;
  bool SetInput(const std::string &key, const std::string &value) override;
  void Render(double time_seconds) override;

private:
  bool EnsureResources();
  bool Rebuild(int width, int height);
  void DrawFrame(SkCanvas *canvas, int width, int height) override;

  std::unique_ptr<WebGlCanvasContext> webgl_;
  sk_sp<SkTypeface> typeface_;
  geometry::CurveInterpolate interpolation_;
  SkPath source_curve_;
  SkPath target_curve_;
  SkPath guide_path_;
  std::vector<float> split_points_;
  bool initialization_attempted_ = false;
  int cached_width_ = 0;
  int cached_height_ = 0;
  float aspect_ = 2.0F;
};
