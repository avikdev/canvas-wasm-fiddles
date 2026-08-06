#pragma once

#include <cstdint>
#include <vector>

#include "include/core/SkPath.h"
#include "include/core/SkPoint.h"
#include "include/core/SkRect.h"

class SkCanvas;
class SkImage;

namespace graphics {

struct MeshDeformerOptions {
  // Minimum spacing used when the original regular control lattice is built.
  // The actual horizontal and vertical gaps can be slightly larger so the
  // bounds are divided into an integral number of cells.
  float epsilon_gap = 8.0F;
  float push_strength = 1.0F;
  // Zero means no hard cap. Distances are in the mesh bounds' coordinate
  // system.
  float maximum_drag_distance = 0.0F;
  // Zero disables long-drag damping. Beyond this distance, response continues
  // at far_drag_response rather than stopping abruptly.
  float damping_start_distance = 0.0F;
  float far_drag_response = 0.2F;
  // Small positive cell areas keep the piecewise-affine map orientation
  // preserving. This is relative to an original rectangular cell's area.
  float minimum_cell_area_ratio = 0.001F;
};

// A fixed-boundary regular triangular control lattice. A drag applies a
// compact swept-brush field to the current control points. Before display, a
// topology line search prevents immediate neighbors from changing order and
// prevents any lattice triangle from changing orientation.
class MeshDeformer {
public:
  bool Build(const SkRect &bounds, const MeshDeformerOptions &options = {});
  void Reset();

  bool BeginDrag(const SkPoint &position, float radius);
  bool UpdateDrag(const SkPoint &position, float intensity = 1.0F);
  bool CommitDrag();
  void CancelDrag();

  // Maps an original-space point through the current piecewise-affine field.
  SkPoint MapPoint(const SkPoint &point) const;

  // Densely samples every original path span, maps those samples, and rebuilds
  // vector-only cubic contours. Original verb boundaries remain boundaries, so
  // intentional corners are retained while deformation-induced kinks between
  // sparse source vertices are smoothed.
  // Straight spans and large curves are subdivided at epsilon_gap before
  // mapping, allowing local lattice pockets to affect geometry whose original
  // path controls were sparse.
  SkPath DeformPath(const SkPath &path) const;

  // Builds horizontal and vertical control-lattice lines in their current
  // deformed positions.
  SkPath ControlGridPath() const;

  void Draw(SkCanvas *canvas, SkImage *image) const;

  bool IsBuilt() const { return !original_positions_.empty(); }
  bool IsDragging() const { return drag_active_; }
  float EpsilonGap() const { return options_.epsilon_gap; }
  const std::vector<SkPoint> &positions() const { return positions_; }
  const std::vector<std::uint16_t> &indices() const { return indices_; }

private:
  struct VertexTopology {
    bool boundary = false;
    std::vector<std::uint16_t> neighbors;
    std::vector<std::size_t> incident_triangle_offsets;
  };

  void AddTriangle(std::uint16_t a, std::uint16_t b, std::uint16_t c);
  bool PreservesTopology(const std::vector<SkPoint> &candidate) const;
  bool PreservesVertexTopology(std::size_t index, const SkPoint &point,
                               const std::vector<SkPoint> &candidate) const;
  void RefreshPositions();

  MeshDeformerOptions options_;
  std::vector<SkPoint> original_positions_;
  std::vector<SkPoint> texture_positions_;
  std::vector<SkPoint> committed_displacements_;
  std::vector<SkPoint> incremental_displacements_;
  std::vector<SkPoint> drag_reference_positions_;
  std::vector<SkPoint> positions_;
  std::vector<float> drag_weights_;
  std::vector<VertexTopology> topology_;
  std::vector<std::uint16_t> indices_;
  SkRect bounds_ = SkRect::MakeEmpty();
  SkPoint drag_start_ = {0.0F, 0.0F};
  float drag_radius_ = 0.0F;
  float step_x_ = 0.0F;
  float step_y_ = 0.0F;
  int columns_ = 0;
  int rows_ = 0;
  bool drag_active_ = false;
};

} // namespace graphics
