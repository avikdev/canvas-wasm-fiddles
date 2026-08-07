#pragma once

#include <array>
#include <memory>

#include "core/fiddle_base.h"
#include "include/core/SkRefCnt.h"

class SkImage;
class SkShader;
class SkTypeface;
class WebGlCanvasContext;

class SkslShaderFiddle final : public FiddleBaseWebGL {
public:
  SkslShaderFiddle();
  ~SkslShaderFiddle() override;

  bool IsSvgWritable() const override { return false; }
  void Render(double time_seconds) override;

private:
  bool EnsureResources();
  bool UpdateState(double time_seconds, int width, int height);
  void DrawFrame(SkCanvas *canvas, int width, int height) override;
  bool BuildChannelShaders();

  std::unique_ptr<WebGlCanvasContext> webgl_;
  sk_sp<SkImage> image_;
  sk_sp<SkTypeface> label_typeface_;
  std::array<sk_sp<SkShader>, 5> channel_shaders_;
  bool initialization_attempted_ = false;
  double time_seconds_ = 0.0;
};
