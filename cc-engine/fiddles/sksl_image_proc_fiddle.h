#pragma once

#include <array>
#include <memory>

#include "core/fiddle_base.h"
#include "include/core/SkRefCnt.h"

class SkImage;
class SkShader;
class SkTypeface;
class WebGlCanvasContext;

class SkslImageProcFiddle final : public FiddleBaseWebGL {
public:
  SkslImageProcFiddle();
  ~SkslImageProcFiddle() override;

  void Render(double time_seconds) override;

private:
  bool EnsureResources();
  bool BuildChannelShaders();

  std::unique_ptr<WebGlCanvasContext> webgl_;
  sk_sp<SkImage> image_;
  sk_sp<SkTypeface> label_typeface_;
  std::array<sk_sp<SkShader>, 5> channel_shaders_;
  bool initialization_attempted_ = false;
};
