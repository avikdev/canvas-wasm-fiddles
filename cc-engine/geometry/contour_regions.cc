#include "geometry/contour_regions.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

#include "geometry/catmull_rom_spline.h"
#include "include/core/SkPathBuilder.h"

namespace geometry {
namespace {

struct ScalarVertex {
  SkPoint point;
  float value = 0.0F;
};

struct PointKey {
  std::int64_t x = 0;
  std::int64_t y = 0;

  bool operator==(const PointKey &) const = default;
  bool operator<(const PointKey &other) const {
    return x < other.x || (x == other.x && y < other.y);
  }
};

struct PointKeyHash {
  std::size_t operator()(const PointKey &key) const {
    const std::uint64_t x = static_cast<std::uint64_t>(key.x);
    const std::uint64_t y = static_cast<std::uint64_t>(key.y);
    return static_cast<std::size_t>((x * 0x9e3779b97f4a7c15ULL) ^
                                    (y * 0xc2b2ae3d27d4eb4fULL));
  }
};

struct EdgeKey {
  PointKey first;
  PointKey second;

  bool operator==(const EdgeKey &) const = default;
};

struct EdgeKeyHash {
  std::size_t operator()(const EdgeKey &key) const {
    const PointKeyHash hash;
    return hash(key.first) ^ (hash(key.second) + 0x9e3779b9U);
  }
};

struct BoundaryEdge {
  PointKey first_key;
  PointKey second_key;
  SkPoint first;
  SkPoint second;
};

constexpr double kBoundaryQuantization = 1024.0;

PointKey KeyForPoint(SkPoint point) {
  return {
      static_cast<std::int64_t>(
          std::llround(static_cast<double>(point.fX) * kBoundaryQuantization)),
      static_cast<std::int64_t>(
          std::llround(static_cast<double>(point.fY) * kBoundaryQuantization)),
  };
}

std::vector<ScalarVertex>
ClipAboveThreshold(const std::vector<ScalarVertex> &input, float threshold) {
  std::vector<ScalarVertex> output;
  if (input.empty()) {
    return output;
  }
  output.reserve(input.size() + 1U);
  for (std::size_t index = 0; index < input.size(); ++index) {
    const ScalarVertex &current = input[index];
    const ScalarVertex &previous =
        input[(index + input.size() - 1U) % input.size()];
    const bool current_inside = current.value >= threshold;
    const bool previous_inside = previous.value >= threshold;
    if (current_inside != previous_inside) {
      const float denominator = current.value - previous.value;
      const float amount =
          std::abs(denominator) <= 0.000001F
              ? 0.5F
              : std::clamp((threshold - previous.value) / denominator, 0.0F,
                           1.0F);
      output.push_back({
          {std::lerp(previous.point.fX, current.point.fX, amount),
           std::lerp(previous.point.fY, current.point.fY, amount)},
          threshold,
      });
    }
    if (current_inside) {
      output.push_back(current);
    }
  }
  return output;
}

std::vector<ScalarVertex>
ClipBelowThreshold(const std::vector<ScalarVertex> &input, float threshold) {
  std::vector<ScalarVertex> output;
  if (input.empty()) {
    return output;
  }
  output.reserve(input.size() + 1U);
  for (std::size_t index = 0; index < input.size(); ++index) {
    const ScalarVertex &current = input[index];
    const ScalarVertex &previous =
        input[(index + input.size() - 1U) % input.size()];
    const bool current_inside = current.value < threshold;
    const bool previous_inside = previous.value < threshold;
    if (current_inside != previous_inside) {
      const float denominator = current.value - previous.value;
      const float amount =
          std::abs(denominator) <= 0.000001F
              ? 0.5F
              : std::clamp((threshold - previous.value) / denominator, 0.0F,
                           1.0F);
      output.push_back({
          {std::lerp(previous.point.fX, current.point.fX, amount),
           std::lerp(previous.point.fY, current.point.fY, amount)},
          threshold,
      });
    }
    if (current_inside) {
      output.push_back(current);
    }
  }
  return output;
}

void ToggleBoundaryEdge(
    SkPoint first, SkPoint second,
    std::unordered_map<EdgeKey, BoundaryEdge, EdgeKeyHash> *edges) {
  PointKey first_key = KeyForPoint(first);
  PointKey second_key = KeyForPoint(second);
  if (first_key == second_key) {
    return;
  }
  const EdgeKey key = first_key < second_key ? EdgeKey{first_key, second_key}
                                             : EdgeKey{second_key, first_key};
  const auto existing = edges->find(key);
  if (existing != edges->end()) {
    edges->erase(existing);
    return;
  }
  edges->emplace(key, BoundaryEdge{first_key, second_key, first, second});
}

void AppendPolygonBoundary(
    const std::vector<ScalarVertex> &polygon,
    std::unordered_map<EdgeKey, BoundaryEdge, EdgeKeyHash> *boundary_edges) {
  if (polygon.size() < 3U || boundary_edges == nullptr) {
    return;
  }
  for (std::size_t index = 0; index < polygon.size(); ++index) {
    ToggleBoundaryEdge(polygon[index].point,
                       polygon[(index + 1U) % polygon.size()].point,
                       boundary_edges);
  }
}

void AppendTriangleAbove(
    const std::array<ScalarVertex, 3> &triangle, float threshold,
    std::unordered_map<EdgeKey, BoundaryEdge, EdgeKeyHash> *boundary_edges) {
  AppendPolygonBoundary(
      ClipAboveThreshold(
          std::vector<ScalarVertex>(triangle.begin(), triangle.end()),
          threshold),
      boundary_edges);
}

std::vector<std::vector<SkPoint>> BuildBoundaryContours(
    const std::unordered_map<EdgeKey, BoundaryEdge, EdgeKeyHash> &edge_map) {
  std::vector<BoundaryEdge> edges;
  edges.reserve(edge_map.size());
  for (const auto &[key, edge] : edge_map) {
    static_cast<void>(key);
    edges.push_back(edge);
  }

  std::unordered_map<PointKey, std::vector<std::size_t>, PointKeyHash> outgoing;
  outgoing.reserve(edges.size());
  for (std::size_t index = 0; index < edges.size(); ++index) {
    outgoing[edges[index].first_key].push_back(index);
  }

  std::vector<bool> visited(edges.size(), false);
  std::vector<std::vector<SkPoint>> contours;
  contours.reserve(edges.size() / 4U + 1U);
  for (std::size_t seed = 0; seed < edges.size(); ++seed) {
    if (visited[seed]) {
      continue;
    }
    const BoundaryEdge &seed_edge = edges[seed];
    const PointKey start_key = seed_edge.first_key;
    std::vector<SkPoint> points;
    points.reserve(16U);
    points.push_back(seed_edge.first);

    std::size_t current_edge = seed;
    while (!visited[current_edge]) {
      visited[current_edge] = true;
      const BoundaryEdge &edge = edges[current_edge];
      const PointKey next_key = edge.second_key;
      if (next_key == start_key) {
        break;
      }
      points.push_back(edge.second);

      std::size_t next_edge = edges.size();
      const auto adjacent = outgoing.find(next_key);
      if (adjacent != outgoing.end()) {
        for (std::size_t candidate : adjacent->second) {
          if (!visited[candidate]) {
            next_edge = candidate;
            break;
          }
        }
      }
      if (next_edge == edges.size()) {
        points.clear();
        break;
      }
      current_edge = next_edge;
    }
    if (points.size() >= 3U) {
      contours.push_back(std::move(points));
    }
  }
  return contours;
}

SkPath BuildThresholdRegion(const ScalarGrid &grid, float threshold,
                            float spline_tension) {
  std::unordered_map<EdgeKey, BoundaryEdge, EdgeKeyHash> boundary_edges;
  boundary_edges.reserve(static_cast<std::size_t>(grid.column_count - 1) *
                         static_cast<std::size_t>(grid.row_count - 1));
  for (int row = 0; row < grid.row_count - 1; ++row) {
    for (int column = 0; column < grid.column_count - 1; ++column) {
      const ScalarVertex top_left = {grid.PointAt(column, row),
                                     grid.ValueAt(column, row)};
      const ScalarVertex top_right = {grid.PointAt(column + 1, row),
                                      grid.ValueAt(column + 1, row)};
      const ScalarVertex bottom_right = {grid.PointAt(column + 1, row + 1),
                                         grid.ValueAt(column + 1, row + 1)};
      const ScalarVertex bottom_left = {grid.PointAt(column, row + 1),
                                        grid.ValueAt(column, row + 1)};
      AppendTriangleAbove(std::array{top_left, top_right, bottom_right},
                          threshold, &boundary_edges);
      AppendTriangleAbove(std::array{top_left, bottom_right, bottom_left},
                          threshold, &boundary_edges);
    }
  }

  CatmullRomOptions spline_options;
  spline_options.closed = true;
  spline_options.tension = spline_tension;
  SkPathBuilder region;
  region.setFillType(SkPathFillType::kWinding);
  for (const std::vector<SkPoint> &contour :
       BuildBoundaryContours(boundary_edges)) {
    const SkPath smoothed = CatmullRomToCubicPath(contour, spline_options);
    if (!smoothed.isEmpty()) {
      region.addPath(smoothed);
    }
  }
  return region.detach();
}

SkPath BuildExclusiveRegion(const ScalarGrid &grid,
                            std::optional<float> lower_threshold,
                            std::optional<float> upper_threshold) {
  std::unordered_map<EdgeKey, BoundaryEdge, EdgeKeyHash> boundary_edges;
  boundary_edges.reserve(static_cast<std::size_t>(grid.column_count - 1) *
                         static_cast<std::size_t>(grid.row_count - 1));
  for (int row = 0; row < grid.row_count - 1; ++row) {
    for (int column = 0; column < grid.column_count - 1; ++column) {
      const ScalarVertex top_left = {grid.PointAt(column, row),
                                     grid.ValueAt(column, row)};
      const ScalarVertex top_right = {grid.PointAt(column + 1, row),
                                      grid.ValueAt(column + 1, row)};
      const ScalarVertex bottom_right = {grid.PointAt(column + 1, row + 1),
                                         grid.ValueAt(column + 1, row + 1)};
      const ScalarVertex bottom_left = {grid.PointAt(column, row + 1),
                                        grid.ValueAt(column, row + 1)};
      const std::array<std::array<ScalarVertex, 3>, 2> triangles = {{
          {top_left, top_right, bottom_right},
          {top_left, bottom_right, bottom_left},
      }};
      for (const auto &triangle : triangles) {
        std::vector<ScalarVertex> polygon(triangle.begin(), triangle.end());
        if (lower_threshold.has_value()) {
          polygon = ClipAboveThreshold(polygon, *lower_threshold);
        }
        if (upper_threshold.has_value()) {
          polygon = ClipBelowThreshold(polygon, *upper_threshold);
        }
        AppendPolygonBoundary(polygon, &boundary_edges);
      }
    }
  }

  SkPathBuilder region;
  region.setFillType(SkPathFillType::kWinding);
  for (const std::vector<SkPoint> &contour :
       BuildBoundaryContours(boundary_edges)) {
    if (contour.size() < 3U) {
      continue;
    }
    region.moveTo(contour.front());
    for (std::size_t index = 1; index < contour.size(); ++index) {
      region.lineTo(contour[index]);
    }
    region.close();
  }
  return region.detach();
}

bool InputsAreValid(std::span<const float> levels, float spline_tension) {
  if (!std::isfinite(spline_tension)) {
    return false;
  }
  for (std::size_t index = 0; index < levels.size(); ++index) {
    if (!std::isfinite(levels[index]) ||
        (index > 0U && levels[index] <= levels[index - 1U])) {
      return false;
    }
  }
  return true;
}

} // namespace

ContourRegionSet::ContourRegionSet(std::vector<SkPath> inclusive_regions,
                                   std::vector<SkPath> exclusive_regions)
    : inclusive_regions_(std::move(inclusive_regions)),
      exclusive_regions_(std::move(exclusive_regions)) {}

const SkPath *ContourRegionSet::InclusiveRegion(std::size_t index) const {
  return index < inclusive_regions_.size() ? &inclusive_regions_[index]
                                           : nullptr;
}

std::optional<SkPath>
ContourRegionSet::ExclusiveRegion(std::size_t index) const {
  if (index >= exclusive_regions_.size()) {
    return std::nullopt;
  }
  return exclusive_regions_[index];
}

std::optional<SkPath> ContourRegionSet::Region(std::size_t index,
                                               ContourRegionMode mode) const {
  if (mode == ContourRegionMode::kExclusive) {
    return ExclusiveRegion(index);
  }
  const SkPath *inclusive = InclusiveRegion(index);
  return inclusive == nullptr ? std::nullopt
                              : std::optional<SkPath>(*inclusive);
}

std::optional<ContourRegionSet>
BuildInclusiveContourRegions(const ScalarGrid &grid,
                             std::span<const float> ascending_levels,
                             float spline_tension) {
  if (!grid.IsValid() || !InputsAreValid(ascending_levels, spline_tension)) {
    return std::nullopt;
  }

  std::vector<SkPath> regions;
  regions.reserve(ascending_levels.size() + 1U);
  for (auto level = ascending_levels.rbegin(); level != ascending_levels.rend();
       ++level) {
    regions.push_back(BuildThresholdRegion(grid, *level, spline_tension));
  }

  SkPathBuilder full_field;
  full_field.addRect(grid.bounds);
  regions.push_back(full_field.detach());

  std::vector<SkPath> exclusive_regions;
  exclusive_regions.reserve(ascending_levels.size() + 1U);
  for (std::size_t index = 0; index <= ascending_levels.size(); ++index) {
    const std::optional<float> lower_threshold =
        index < ascending_levels.size()
            ? std::optional<float>(
                  ascending_levels[ascending_levels.size() - index - 1U])
            : std::nullopt;
    const std::optional<float> upper_threshold =
        index == 0U
            ? std::nullopt
            : std::optional<float>(
                  ascending_levels[ascending_levels.size() - index]);
    exclusive_regions.push_back(
        BuildExclusiveRegion(grid, lower_threshold, upper_threshold));
  }
  return ContourRegionSet(std::move(regions),
                          std::move(exclusive_regions));
}

} // namespace geometry
