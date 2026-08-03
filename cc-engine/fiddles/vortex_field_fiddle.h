#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include "fiddle_base.h"
#include "include/core/SkColor.h"
#include "include/core/SkPath.h"
#include "include/core/SkPoint.h"
#include "include/core/SkRefCnt.h"

class WebGlCanvasContext;
class SkPathEffect;

struct VortexContourSamples {
  std::vector<SkPoint> points;
  bool closed = false;
};

struct VortexGridShape {
  SkPath path;
  std::vector<VortexContourSamples> contours;
  SkColor4f color;
};

struct VortexMotionState {
  SkPoint center = {0.0F, 0.0F};
  SkPoint velocity = {0.0F, 0.0F};
  float diameter_cell_ratio = 1.0F;
  float speed_scale = 1.0F;
  float twist_direction = 1.0F;
  float wander_phase = 0.0F;
  float waypoint_age_seconds = 0.0F;
  int waypoint_index = 0;
};

class VortexFieldFiddle final : public FiddleBase {
public:
  VortexFieldFiddle();
  ~VortexFieldFiddle() override;

  void Render(double time_seconds) override;

protected:
  bool UsesWebGl() const override;

private:
  bool EnsureResources();
  bool RebuildGrid(float width, float height);
  void InitializeVortices(float width, float height, double time_seconds);
  void UpdateVortices(float width, float height, double time_seconds);

  std::unique_ptr<WebGlCanvasContext> webgl_;
  sk_sp<SkPathEffect> dash_path_effect_;
  std::vector<VortexGridShape> shapes_;
  std::array<VortexMotionState, 4> vortices_;
  std::uint32_t field_seed_ = 0;
  float cell_size_ = 1.0F;
  double last_motion_time_seconds_ = -1.0;
  bool motion_initialized_ = false;
  bool initialization_attempted_ = false;
  int cached_width_ = 0;
  int cached_height_ = 0;
};
