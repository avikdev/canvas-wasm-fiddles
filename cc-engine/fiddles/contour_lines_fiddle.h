#pragma once

#include <array>
#include <cstdint>
#include <memory>

#include "core/fiddle_base.h"
#include "include/core/SkColor.h"

class WebGlCanvasContext;

class ContourLinesFiddle final : public FiddleBaseWebGL {
public:
  ContourLinesFiddle();
  ~ContourLinesFiddle() override;

  bool IsSvgWritable() const override { return true; }
  void Render(double time_seconds) override;

private:
  static constexpr int kContourLevelCount = 5;
  static constexpr int kBandCount = kContourLevelCount + 1;

  bool EnsureResources();
  bool UpdateState(double time_seconds, int width, int height);
  void DrawFrame(SkCanvas *canvas, int width, int height) override;

  std::unique_ptr<WebGlCanvasContext> webgl_;
  std::array<SkColor4f, kBandCount> band_colors_;
  std::uint32_t field_seed_ = 0U;
  bool initialization_attempted_ = false;
  double time_seconds_ = 0.0;
};
