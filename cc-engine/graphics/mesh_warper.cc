#include "graphics/mesh_warper.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>

#include "geometry/catmull_rom_spline.h"
#include "include/core/SkBlendMode.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkImage.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkSamplingOptions.h"
#include "include/core/SkShader.h"
#include "include/core/SkVertices.h"

namespace graphics {
namespace {

SkPoint Add(const SkPoint &a, const SkPoint &b) {
  return {a.fX + b.fX, a.fY + b.fY};
}

SkPoint Scale(const SkPoint &point, float scale) {
  return {point.fX * scale, point.fY * scale};
}

SkPoint WeightedSum(const SkPoint &a, float weight_a, const SkPoint &b,
                    float weight_b, const SkPoint &c, float weight_c) {
  return {a.fX * weight_a + b.fX * weight_b + c.fX * weight_c,
          a.fY * weight_a + b.fY * weight_b + c.fY * weight_c};
}

float Distance(const SkPoint &a, const SkPoint &b) {
  return std::hypot(a.fX - b.fX, a.fY - b.fY);
}

void AppendSpline(const std::vector<SkPoint> &points, SkPathBuilder *builder) {
  if (points.size() < 2U) {
    return;
  }
  const SkPath spline = geometry::CatmullRomToCubicPath(points);
  SkPath::RawIter iterator(spline);
  SkPoint controls[4];
  for (SkPath::Verb verb = iterator.next(controls); verb != SkPath::kDone_Verb;
       verb = iterator.next(controls)) {
    if (verb == SkPath::kCubic_Verb) {
      builder->cubicTo(controls[1], controls[2], controls[3]);
    } else if (verb == SkPath::kLine_Verb) {
      builder->lineTo(controls[1]);
    }
  }
}

} // namespace

bool MeshWarper::Build(const SkRect &bounds, const MeshWarperOptions &options) {
  if (!bounds.isFinite() || bounds.isEmpty() ||
      !std::isfinite(options.epsilon_gap) || options.epsilon_gap <= 0.0F ||
      bounds.width() < options.epsilon_gap * 2.0F ||
      bounds.height() < options.epsilon_gap * 2.0F ||
      !std::isfinite(options.push_strength) || options.push_strength < 0.0F ||
      !std::isfinite(options.maximum_drag_distance) ||
      options.maximum_drag_distance < 0.0F ||
      !std::isfinite(options.damping_start_distance) ||
      options.damping_start_distance < 0.0F ||
      !std::isfinite(options.far_drag_response) ||
      !std::isfinite(options.minimum_cell_area_ratio) ||
      options.minimum_cell_area_ratio <= 0.0F ||
      options.minimum_cell_area_ratio >= 0.5F) {
    Reset();
    return false;
  }

  const int columns = std::max(
      2, static_cast<int>(std::floor(bounds.width() / options.epsilon_gap)));
  const int rows = std::max(
      2, static_cast<int>(std::floor(bounds.height() / options.epsilon_gap)));
  const std::size_t vertex_count = static_cast<std::size_t>(columns + 1) *
                                   static_cast<std::size_t>(rows + 1);
  if (vertex_count > std::numeric_limits<std::uint16_t>::max()) {
    Reset();
    return false;
  }

  options_ = options;
  bounds_ = bounds;
  columns_ = columns;
  rows_ = rows;
  original_positions_.clear();
  texture_positions_.clear();
  topology_.clear();
  indices_.clear();
  original_positions_.reserve(vertex_count);
  texture_positions_.reserve(vertex_count);
  topology_.reserve(vertex_count);
  indices_.reserve(static_cast<std::size_t>(columns) *
                   static_cast<std::size_t>(rows) * 6U);

  step_x_ = bounds.width() / static_cast<float>(columns);
  step_y_ = bounds.height() / static_cast<float>(rows);
  for (int row = 0; row <= rows; ++row) {
    for (int column = 0; column <= columns; ++column) {
      original_positions_.push_back(
          {bounds.left() + static_cast<float>(column) * step_x_,
           bounds.top() + static_cast<float>(row) * step_y_});
      texture_positions_.push_back({static_cast<float>(column) * step_x_,
                                    static_cast<float>(row) * step_y_});
      topology_.push_back({.boundary = row == 0 || row == rows || column == 0 ||
                                       column == columns});
    }
  }

  const auto index_at = [columns](int column, int row) {
    return static_cast<std::uint16_t>(row * (columns + 1) + column);
  };
  for (int row = 0; row < rows; ++row) {
    for (int column = 0; column < columns; ++column) {
      const std::uint16_t top_left = index_at(column, row);
      const std::uint16_t top_right = index_at(column + 1, row);
      const std::uint16_t bottom_left = index_at(column, row + 1);
      const std::uint16_t bottom_right = index_at(column + 1, row + 1);
      // Alternating diagonals avoid giving the regular grid a persistent
      // top-right/bottom-left directional bias.
      if ((row + column) % 2 == 0) {
        AddTriangle(top_left, top_right, bottom_right);
        AddTriangle(top_left, bottom_right, bottom_left);
      } else {
        AddTriangle(top_left, top_right, bottom_left);
        AddTriangle(top_right, bottom_right, bottom_left);
      }
    }
  }

  committed_displacements_.assign(vertex_count, {0.0F, 0.0F});
  incremental_displacements_.assign(vertex_count, {0.0F, 0.0F});
  drag_reference_positions_ = original_positions_;
  positions_ = original_positions_;
  drag_weights_.assign(vertex_count, 0.0F);
  drag_active_ = false;
  return true;
}

void MeshWarper::Reset() {
  original_positions_.clear();
  texture_positions_.clear();
  committed_displacements_.clear();
  incremental_displacements_.clear();
  drag_reference_positions_.clear();
  positions_.clear();
  drag_weights_.clear();
  topology_.clear();
  indices_.clear();
  bounds_ = SkRect::MakeEmpty();
  columns_ = 0;
  rows_ = 0;
  step_x_ = 0.0F;
  step_y_ = 0.0F;
  drag_active_ = false;
}

bool MeshWarper::BeginDrag(const SkPoint &position, float radius) {
  if (!IsBuilt() || drag_active_ || !position.isFinite() ||
      !std::isfinite(radius) || radius <= 0.0F) {
    return false;
  }

  drag_start_ = position;
  drag_radius_ = radius;
  drag_reference_positions_ = positions_;
  const float radius_squared = radius * radius;
  bool has_handle = false;
  for (std::size_t index = 0; index < positions_.size(); ++index) {
    if (topology_[index].boundary) {
      drag_weights_[index] = 0.0F;
      continue;
    }
    const float dx = positions_[index].fX - position.fX;
    const float dy = positions_[index].fY - position.fY;
    const float distance_squared = dx * dx + dy * dy;
    if (distance_squared >= radius_squared) {
      drag_weights_[index] = 0.0F;
      continue;
    }
    const float radial = 1.0F - distance_squared / radius_squared;
    drag_weights_[index] = radial * radial;
    has_handle = true;
  }
  if (!has_handle) {
    return false;
  }

  std::fill(incremental_displacements_.begin(),
            incremental_displacements_.end(), SkPoint{0.0F, 0.0F});
  drag_active_ = true;
  return true;
}

bool MeshWarper::UpdateDrag(const SkPoint &position, float intensity) {
  if (!drag_active_ || !position.isFinite() || !std::isfinite(intensity) ||
      intensity < 0.0F) {
    return false;
  }

  SkPoint raw_target = {(position.fX - drag_start_.fX) * intensity,
                        (position.fY - drag_start_.fY) * intensity};
  const float raw_length = std::hypot(raw_target.fX, raw_target.fY);
  SkPoint direction = {0.0F, 0.0F};
  if (raw_length > 0.0001F) {
    direction = Scale(raw_target, 1.0F / raw_length);
  }
  float effective_length = raw_length;
  const float damping_start = options_.damping_start_distance;
  if (damping_start > 0.0F && raw_length > damping_start) {
    effective_length =
        damping_start + (raw_length - damping_start) *
                            std::clamp(options_.far_drag_response, 0.0F, 1.0F);
  }
  SkPoint target = Scale(direction, effective_length);
  if (options_.maximum_drag_distance > 0.0F &&
      effective_length > options_.maximum_drag_distance) {
    effective_length = options_.maximum_drag_distance;
    target = Scale(direction, effective_length);
  }

  // A compact swept brush avoids the nonlocal response of a harmonic solve.
  // The force is strongest near the start of the gesture and progressively
  // weaker near its leading end, so the lattice bunches up in front of the
  // pointer rather than translating as a rigid slab.
  std::vector<SkPoint> candidate = drag_reference_positions_;
  const float target_length_squared =
      target.fX * target.fX + target.fY * target.fY;
  for (std::size_t index = 0; index < candidate.size(); ++index) {
    if (topology_[index].boundary || target_length_squared <= 0.000001F) {
      continue;
    }
    const SkPoint &reference = drag_reference_positions_[index];
    const SkPoint from_start = {reference.fX - drag_start_.fX,
                                reference.fY - drag_start_.fY};
    const float along =
        std::clamp((from_start.fX * target.fX + from_start.fY * target.fY) /
                       target_length_squared,
                   0.0F, 1.0F);
    const SkPoint closest = {drag_start_.fX + target.fX * along,
                             drag_start_.fY + target.fY * along};
    const float dx = reference.fX - closest.fX;
    const float dy = reference.fY - closest.fY;
    const float distance_squared = dx * dx + dy * dy;
    const float radius_squared = drag_radius_ * drag_radius_;
    if (distance_squared >= radius_squared) {
      continue;
    }
    const float radial = 1.0F - distance_squared / radius_squared;
    const float compact_weight = radial * radial;
    constexpr float leading_response = 0.16F;
    const float longitudinal = 1.0F - along * (1.0F - leading_response);
    const float force = compact_weight * longitudinal * options_.push_strength;
    candidate[index] = Add(reference, Scale(target, force));
  }

  // If topology protection is needed, advance from the last valid preview
  // rather than scaling again from the pointer-down snapshot. Each control
  // point is independently moved as far as its immediate neighbor ordering
  // and incident triangle areas permit. Thus one saturated pocket does not
  // suppress the rest of the brush, and subsequent pointer frames leave a
  // saturated point at its maximum instead of making it bounce backward.
  if (!PreservesTopology(candidate)) {
    const std::vector<SkPoint> desired = candidate;
    candidate = positions_;
    for (std::size_t index = 0; index < candidate.size(); ++index) {
      if (topology_[index].boundary || desired[index] == candidate[index]) {
        continue;
      }
      if (PreservesVertexTopology(index, desired[index], candidate)) {
        candidate[index] = desired[index];
        continue;
      }
      const SkPoint start = candidate[index];
      float lower = 0.0F;
      float upper = 1.0F;
      for (int iteration = 0; iteration < 20; ++iteration) {
        const float alpha = (lower + upper) * 0.5F;
        const SkPoint trial = {std::lerp(start.fX, desired[index].fX, alpha),
                               std::lerp(start.fY, desired[index].fY, alpha)};
        if (PreservesVertexTopology(index, trial, candidate)) {
          lower = alpha;
        } else {
          upper = alpha;
        }
      }
      candidate[index] = {std::lerp(start.fX, desired[index].fX, lower),
                          std::lerp(start.fY, desired[index].fY, lower)};
    }
  }

  for (std::size_t index = 0; index < candidate.size(); ++index) {
    incremental_displacements_[index] = {
        candidate[index].fX - drag_reference_positions_[index].fX,
        candidate[index].fY - drag_reference_positions_[index].fY};
  }
  RefreshPositions();
  return true;
}

bool MeshWarper::CommitDrag() {
  if (!drag_active_) {
    return false;
  }
  for (std::size_t index = 0; index < committed_displacements_.size();
       ++index) {
    committed_displacements_[index] =
        Add(committed_displacements_[index], incremental_displacements_[index]);
  }
  std::fill(incremental_displacements_.begin(),
            incremental_displacements_.end(), SkPoint{0.0F, 0.0F});
  drag_active_ = false;
  RefreshPositions();
  return true;
}

void MeshWarper::CancelDrag() {
  if (!drag_active_) {
    return;
  }
  std::fill(incremental_displacements_.begin(),
            incremental_displacements_.end(), SkPoint{0.0F, 0.0F});
  drag_active_ = false;
  RefreshPositions();
}

SkPoint MeshWarper::MapPoint(const SkPoint &point) const {
  if (!IsBuilt() || !point.isFinite()) {
    return point;
  }
  const float normalized_x =
      std::clamp((point.fX - bounds_.left()) / bounds_.width(), 0.0F, 1.0F);
  const float normalized_y =
      std::clamp((point.fY - bounds_.top()) / bounds_.height(), 0.0F, 1.0F);
  const float grid_x = normalized_x * static_cast<float>(columns_);
  const float grid_y = normalized_y * static_cast<float>(rows_);
  const int column =
      std::min(columns_ - 1, static_cast<int>(std::floor(grid_x)));
  const int row = std::min(rows_ - 1, static_cast<int>(std::floor(grid_y)));
  const float fraction_x = grid_x - static_cast<float>(column);
  const float fraction_y = grid_y - static_cast<float>(row);
  const auto index_at = [this](int x, int y) {
    return static_cast<std::size_t>(y * (columns_ + 1) + x);
  };
  const SkPoint &top_left = positions_[index_at(column, row)];
  const SkPoint &top_right = positions_[index_at(column + 1, row)];
  const SkPoint &bottom_left = positions_[index_at(column, row + 1)];
  const SkPoint &bottom_right = positions_[index_at(column + 1, row + 1)];

  if ((row + column) % 2 == 0) {
    if (fraction_y <= fraction_x) {
      return WeightedSum(top_left, 1.0F - fraction_x, top_right,
                         fraction_x - fraction_y, bottom_right, fraction_y);
    }
    return WeightedSum(top_left, 1.0F - fraction_y, bottom_right, fraction_x,
                       bottom_left, fraction_y - fraction_x);
  }
  if (fraction_x + fraction_y <= 1.0F) {
    return WeightedSum(top_left, 1.0F - fraction_x - fraction_y, top_right,
                       fraction_x, bottom_left, fraction_y);
  }
  return WeightedSum(top_right, 1.0F - fraction_y, bottom_right,
                     fraction_x + fraction_y - 1.0F, bottom_left,
                     1.0F - fraction_x);
}

SkPath MeshWarper::DeformPath(const SkPath &path) const {
  if (!IsBuilt() || path.isEmpty()) {
    return {};
  }
  const float sample_spacing = options_.epsilon_gap;

  SkPathBuilder builder;
  builder.setFillType(path.getFillType());
  SkPath::RawIter iterator(path);
  SkPoint points[4];
  SkPoint contour_start = {0.0F, 0.0F};
  SkPoint current = {0.0F, 0.0F};
  bool contour_open = false;

  const auto append_segment =
      [this, sample_spacing,
       &builder](float approximate_length,
                 const std::function<SkPoint(float)> &evaluate) {
        const int sample_count = std::max(
            3,
            static_cast<int>(std::ceil(approximate_length / sample_spacing)) +
                1);
        std::vector<SkPoint> samples;
        samples.reserve(static_cast<std::size_t>(sample_count));
        for (int sample = 0; sample < sample_count; ++sample) {
          const float parameter =
              static_cast<float>(sample) /
              static_cast<float>(std::max(1, sample_count - 1));
          samples.push_back(MapPoint(evaluate(parameter)));
        }
        AppendSpline(samples, &builder);
      };

  for (SkPath::Verb verb = iterator.next(points); verb != SkPath::kDone_Verb;
       verb = iterator.next(points)) {
    switch (verb) {
    case SkPath::kMove_Verb:
      contour_start = points[0];
      current = points[0];
      builder.moveTo(MapPoint(current));
      contour_open = true;
      break;
    case SkPath::kLine_Verb: {
      const SkPoint start = points[0];
      const SkPoint end = points[1];
      append_segment(Distance(start, end), [start, end](float t) {
        return SkPoint{std::lerp(start.fX, end.fX, t),
                       std::lerp(start.fY, end.fY, t)};
      });
      current = end;
      break;
    }
    case SkPath::kQuad_Verb: {
      const SkPoint start = points[0];
      const SkPoint control = points[1];
      const SkPoint end = points[2];
      append_segment(
          Distance(start, control) + Distance(control, end),
          [start, control, end](float t) {
            const float u = 1.0F - t;
            return SkPoint{
                u * u * start.fX + 2.0F * u * t * control.fX + t * t * end.fX,
                u * u * start.fY + 2.0F * u * t * control.fY + t * t * end.fY};
          });
      current = end;
      break;
    }
    case SkPath::kConic_Verb: {
      const SkPoint start = points[0];
      const SkPoint control = points[1];
      const SkPoint end = points[2];
      const float weight = iterator.conicWeight();
      append_segment(
          Distance(start, control) + Distance(control, end),
          [start, control, end, weight](float t) {
            const float u = 1.0F - t;
            const float denominator = u * u + 2.0F * weight * u * t + t * t;
            return SkPoint{
                (u * u * start.fX + 2.0F * weight * u * t * control.fX +
                 t * t * end.fX) /
                    denominator,
                (u * u * start.fY + 2.0F * weight * u * t * control.fY +
                 t * t * end.fY) /
                    denominator};
          });
      current = end;
      break;
    }
    case SkPath::kCubic_Verb: {
      const SkPoint start = points[0];
      const SkPoint first = points[1];
      const SkPoint second = points[2];
      const SkPoint end = points[3];
      append_segment(
          Distance(start, first) + Distance(first, second) +
              Distance(second, end),
          [start, first, second, end](float t) {
            const float u = 1.0F - t;
            return SkPoint{
                u * u * u * start.fX + 3.0F * u * u * t * first.fX +
                    3.0F * u * t * t * second.fX + t * t * t * end.fX,
                u * u * u * start.fY + 3.0F * u * u * t * first.fY +
                    3.0F * u * t * t * second.fY + t * t * t * end.fY};
          });
      current = end;
      break;
    }
    case SkPath::kClose_Verb:
      if (contour_open && current != contour_start) {
        const SkPoint start = current;
        const SkPoint end = contour_start;
        append_segment(Distance(start, end), [start, end](float t) {
          return SkPoint{std::lerp(start.fX, end.fX, t),
                         std::lerp(start.fY, end.fY, t)};
        });
      }
      builder.close();
      current = contour_start;
      contour_open = false;
      break;
    case SkPath::kDone_Verb:
      break;
    }
  }
  return builder.detach();
}

SkPath MeshWarper::ControlGridPath() const {
  if (!IsBuilt()) {
    return {};
  }
  const auto index_at = [this](int column, int row) {
    return static_cast<std::size_t>(row * (columns_ + 1) + column);
  };
  SkPathBuilder builder;
  for (int row = 0; row <= rows_; ++row) {
    builder.moveTo(positions_[index_at(0, row)]);
    for (int column = 1; column <= columns_; ++column) {
      builder.lineTo(positions_[index_at(column, row)]);
    }
  }
  for (int column = 0; column <= columns_; ++column) {
    builder.moveTo(positions_[index_at(column, 0)]);
    for (int row = 1; row <= rows_; ++row) {
      builder.lineTo(positions_[index_at(column, row)]);
    }
  }
  return builder.detach();
}

void MeshWarper::Draw(SkCanvas *canvas, SkImage *image) const {
  if (canvas == nullptr || image == nullptr || !IsBuilt()) {
    return;
  }
  const sk_sp<SkVertices> vertices = SkVertices::MakeCopy(
      SkVertices::kTriangles_VertexMode, static_cast<int>(positions_.size()),
      positions_.data(), texture_positions_.data(), nullptr,
      static_cast<int>(indices_.size()), indices_.data());
  if (vertices == nullptr) {
    return;
  }

  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setShader(image->makeShader(
      SkTileMode::kClamp, SkTileMode::kClamp,
      SkSamplingOptions(SkFilterMode::kLinear, SkMipmapMode::kNone)));
  canvas->drawVertices(vertices, SkBlendMode::kModulate, paint);
}

void MeshWarper::AddTriangle(std::uint16_t a, std::uint16_t b,
                             std::uint16_t c) {
  const std::size_t triangle_offset = indices_.size();
  indices_.insert(indices_.end(), {a, b, c});
  topology_[a].incident_triangle_offsets.push_back(triangle_offset);
  topology_[b].incident_triangle_offsets.push_back(triangle_offset);
  topology_[c].incident_triangle_offsets.push_back(triangle_offset);
  const auto add_neighbor = [this](std::uint16_t from, std::uint16_t to) {
    std::vector<std::uint16_t> &neighbors = topology_[from].neighbors;
    if (std::find(neighbors.begin(), neighbors.end(), to) == neighbors.end()) {
      neighbors.push_back(to);
    }
  };
  add_neighbor(a, b);
  add_neighbor(a, c);
  add_neighbor(b, a);
  add_neighbor(b, c);
  add_neighbor(c, a);
  add_neighbor(c, b);
}

bool MeshWarper::PreservesTopology(
    const std::vector<SkPoint> &candidate) const {
  if (candidate.size() != positions_.size()) {
    return false;
  }
  const float neighbor_gap = std::max(0.0001F, options_.epsilon_gap * 0.0001F);
  const auto index_at = [this](int column, int row) {
    return static_cast<std::size_t>(row * (columns_ + 1) + column);
  };
  for (int row = 0; row <= rows_; ++row) {
    for (int column = 0; column <= columns_; ++column) {
      const std::size_t index = index_at(column, row);
      const SkPoint &point = candidate[index];
      if (!point.isFinite()) {
        return false;
      }
      if (topology_[index].boundary && point != original_positions_[index]) {
        return false;
      }
      if (column < columns_ &&
          candidate[index_at(column + 1, row)].fX - point.fX < neighbor_gap) {
        return false;
      }
      if (row < rows_ &&
          candidate[index_at(column, row + 1)].fY - point.fY < neighbor_gap) {
        return false;
      }
    }
  }

  const float minimum_twice_area =
      step_x_ * step_y_ * options_.minimum_cell_area_ratio;
  for (std::size_t offset = 0; offset < indices_.size(); offset += 3U) {
    const SkPoint &a = candidate[indices_[offset]];
    const SkPoint &b = candidate[indices_[offset + 1U]];
    const SkPoint &c = candidate[indices_[offset + 2U]];
    const float twice_area =
        (b.fX - a.fX) * (c.fY - a.fY) - (b.fY - a.fY) * (c.fX - a.fX);
    if (twice_area < minimum_twice_area) {
      return false;
    }
  }
  return true;
}

bool MeshWarper::PreservesVertexTopology(
    std::size_t index, const SkPoint &point,
    const std::vector<SkPoint> &candidate) const {
  if (index >= candidate.size() || !point.isFinite() ||
      topology_[index].boundary) {
    return false;
  }
  const int stride = columns_ + 1;
  const int row = static_cast<int>(index) / stride;
  const int column = static_cast<int>(index) % stride;
  const float neighbor_gap = std::max(0.0001F, options_.epsilon_gap * 0.0001F);
  const auto index_at = [stride](int x, int y) {
    return static_cast<std::size_t>(y * stride + x);
  };
  if ((column > 0 &&
       point.fX - candidate[index_at(column - 1, row)].fX < neighbor_gap) ||
      (column < columns_ &&
       candidate[index_at(column + 1, row)].fX - point.fX < neighbor_gap) ||
      (row > 0 &&
       point.fY - candidate[index_at(column, row - 1)].fY < neighbor_gap) ||
      (row < rows_ &&
       candidate[index_at(column, row + 1)].fY - point.fY < neighbor_gap)) {
    return false;
  }

  const float minimum_twice_area =
      step_x_ * step_y_ * options_.minimum_cell_area_ratio;
  for (const std::size_t offset : topology_[index].incident_triangle_offsets) {
    const std::uint16_t a_index = indices_[offset];
    const std::uint16_t b_index = indices_[offset + 1U];
    const std::uint16_t c_index = indices_[offset + 2U];
    const SkPoint &a = a_index == index ? point : candidate[a_index];
    const SkPoint &b = b_index == index ? point : candidate[b_index];
    const SkPoint &c = c_index == index ? point : candidate[c_index];
    const float twice_area =
        (b.fX - a.fX) * (c.fY - a.fY) - (b.fY - a.fY) * (c.fX - a.fX);
    if (twice_area < minimum_twice_area) {
      return false;
    }
  }
  return true;
}

void MeshWarper::RefreshPositions() {
  for (std::size_t index = 0; index < positions_.size(); ++index) {
    positions_[index] =
        Add(original_positions_[index], Add(committed_displacements_[index],
                                            incremental_displacements_[index]));
  }
}

} // namespace graphics
