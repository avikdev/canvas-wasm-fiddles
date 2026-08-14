#pragma once

#include <memory>
#include <vector>

#include "core/fiddle_base.h"
#include "include/core/SkColor.h"
#include "include/core/SkPath.h"
#include "include/core/SkRefCnt.h"

class SkTypeface;
class WebGlCanvasContext;

struct NoiseDeformLetter {
  SkPath path;
  SkColor color = SK_ColorBLACK;
};

class NoiseDeformFiddle final : public FiddleBaseWebGL {
public:
  NoiseDeformFiddle();
  ~NoiseDeformFiddle() override;

  bool IsSvgWritable() const override { return true; }
  std::vector<FiddleWidget> Widgets() const override;
  bool SetInput(const std::string &name, const std::string &value) override;
  void Render(double time_seconds) override;

private:
  bool EnsureResources();
  bool Rebuild(int width, int height, int configuration_index);
  void DrawFrame(SkCanvas *canvas, int width, int height) override;

  std::unique_ptr<WebGlCanvasContext> webgl_;
  sk_sp<SkTypeface> typeface_;
  std::vector<NoiseDeformLetter> letters_;
  double time_seconds_ = 0.0;
  int configuration_index_ = -1;
  int cached_width_ = 0;
  int cached_height_ = 0;
  bool initialization_attempted_ = false;
  float intensity_ = 1.0F;
  float rotation_degrees_ = 0.0F;
};
