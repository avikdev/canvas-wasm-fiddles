#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "core/fiddle_base.h"
#include "graphics/mesh_warper.h"
#include "include/core/SkColor.h"
#include "include/core/SkPath.h"
#include "include/core/SkPoint.h"
#include "include/core/SkRefCnt.h"
#include "utils/fake_mouse_actions.h"

class SkTypeface;
class WebGlCanvasContext;

struct MeshWarpArtworkShape {
  SkPath path;
  SkPath hole;
  SkColor4f color;
  SkPoint hole_center = {0.0F, 0.0F};
  float stroke_width = 0.0F;
};

// Interaction and mesh values intentionally collected here for eventual
// exposure as live fiddle controls. Ratios are relative to the source-artwork
// dimensions; logical pixel values are multiplied by the device scale.
struct MeshWarpFiddleOptions {
  // Smaller values produce a denser lattice and finer path subdivision.
  float epsilon_gap = 4.0F;
  // Core force multiplier. Per-gesture intensity below leaves headroom for
  // several successive deformations.
  float push_strength = 1.0F;
  float drag_intensity = 0.45F;
  // Hard pointer/deformation cap relative to the longer source dimension.
  float maximum_drag_distance_ratio = 0.50F;
  // Full response continues to this fraction of the hard cap. Motion beyond
  // the knee receives only far_drag_response.
  float damping_start_distance_ratio = 0.45F;
  float far_drag_response = 0.12F;
  // Permits dense compression while retaining positive triangle areas.
  float minimum_cell_area_ratio = 0.000001F;
  float brush_radius_ratio = 0.25F;

  // Synthetic-pointer presentation and endpoint distribution.
  // Pointer travel per rendered frame as a fraction of the shorter source
  // dimension. A ratio keeps gesture speed stable across viewport sizes.
  float pointer_step_ratio = 0.015F;
  float edge_inset_ratio = 0.12F;
  float inward_drag_weight = 0.30F;
  float outward_drag_weight = 0.30F;
  float center_drag_weight = 0.25F;
  float sideways_drag_weight = 0.15F;
  float center_region_ratio = 0.45F;
  float minimum_drag_length_ratio = 0.45F;
  int idle_frames_between_drags = 10;

  std::size_t drag_count_before_reset = 10U;
  double reset_delay_seconds = 1.0;
};

class MeshWarpFiddle final : public FiddleBaseWebGL {
public:
  MeshWarpFiddle();
  explicit MeshWarpFiddle(const MeshWarpFiddleOptions &options);
  ~MeshWarpFiddle() override;

  bool IsSvgWritable() const override { return true; }
  void Render(double time_seconds) override;

private:
  bool EnsureResources();
  bool RebuildScene(int width, int height);
  void AdvanceSyntheticDrag(double time_seconds);
  void DrawFrame(SkCanvas *canvas, int width, int height) override;

  std::unique_ptr<WebGlCanvasContext> webgl_;
  MeshWarpFiddleOptions options_;
  sk_sp<SkTypeface> artwork_typeface_;
  std::unique_ptr<input::FakeMouseActions> fake_mouse_;
  graphics::MeshWarper deformer_;
  std::vector<MeshWarpArtworkShape> shapes_;
  input::FakeMouseFrame active_frame_;
  SkPoint output_origin_ = {0.0F, 0.0F};
  float source_width_ = 0.0F;
  float source_height_ = 0.0F;
  float device_scale_ = 1.0F;
  bool initialization_attempted_ = false;
  bool deformer_drag_active_ = false;
  bool reset_pending_ = false;
  double reset_started_at_ = 0.0;
  float cycle_remaining_fraction_ = 1.0F;
  std::size_t completed_drag_count_ = 0U;
  std::uint32_t scene_generation_ = 0U;
  int cached_width_ = 0;
  int cached_height_ = 0;
};
