#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include "core/fiddle_base.h"
#include "include/core/SkRefCnt.h"

class SkShader;
class SkTypeface;
class WebGlCanvasContext;

class ContourCompositeFiddle final : public FiddleBaseWebGL {
public:
  ContourCompositeFiddle();
  ~ContourCompositeFiddle() override;

  bool IsSvgWritable() const override { return true; }
  void Render(double time_seconds) override;

private:
  bool EnsureResources();
  bool BuildPatternShaders();
  void DrawFrame(SkCanvas *canvas, int width, int height) override;

  std::unique_ptr<WebGlCanvasContext> webgl_;
  sk_sp<SkTypeface> typeface_;
  std::array<sk_sp<SkShader>, 4> pattern_shaders_;
  std::vector<float> thresholds_;
  std::uint32_t field_seed_ = 0U;
  bool initialization_attempted_ = false;
  double time_seconds_ = 0.0;
};
