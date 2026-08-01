#pragma once

#include <memory>

#include "fiddle_base.h"
#include "include/core/SkRefCnt.h"

class SkTypeface;
class WebGlCanvasContext;

class ShapeIntersectionFiddle final : public FiddleBase {
public:
  ShapeIntersectionFiddle();
  ~ShapeIntersectionFiddle() override;

  void Render(double time_seconds) override;

protected:
  bool UsesWebGl() const override;

private:
  bool EnsureWebGl();

  std::unique_ptr<WebGlCanvasContext> webgl_;
  sk_sp<SkTypeface> label_typeface_;
  bool initialization_attempted_ = false;
};
