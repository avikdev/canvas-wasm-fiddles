#include "geometry/marching_squares.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

namespace geometry {
namespace {

enum class CellEdge {
  kTop,
  kRight,
  kBottom,
  kLeft,
};

struct EdgeKey {
  int x = 0;
  int y = 0;
  bool vertical = false;

  bool operator==(const EdgeKey &) const = default;
};

struct EdgeKeyHash {
  std::size_t operator()(const EdgeKey &key) const {
    std::uint64_t value = static_cast<std::uint32_t>(key.x) * 0x9e3779b9ULL ^
                          static_cast<std::uint32_t>(key.y) * 0x85ebca6bULL;
    value ^= static_cast<std::uint64_t>(key.vertical) * 0xc2b2ae35ULL;
    value ^= value >> 29;
    return static_cast<std::size_t>(value);
  }
};

struct ContourVertex {
  EdgeKey key;
  SkPoint point;
};

struct ContourSegment {
  std::array<ContourVertex, 2> vertices;
};

EdgeKey KeyForEdge(int column, int row, CellEdge edge) {
  switch (edge) {
  case CellEdge::kTop:
    return {column, row, false};
  case CellEdge::kRight:
    return {column + 1, row, true};
  case CellEdge::kBottom:
    return {column, row + 1, false};
  case CellEdge::kLeft:
    return {column, row, true};
  }
  return {};
}

ContourVertex VertexForEdge(const ScalarGrid &grid, int column, int row,
                            CellEdge edge, float threshold) {
  int first_column = column;
  int first_row = row;
  int second_column = column + 1;
  int second_row = row;
  switch (edge) {
  case CellEdge::kTop:
    break;
  case CellEdge::kRight:
    first_column = column + 1;
    second_column = column + 1;
    second_row = row + 1;
    break;
  case CellEdge::kBottom:
    first_row = row + 1;
    second_row = row + 1;
    break;
  case CellEdge::kLeft:
    second_column = column;
    second_row = row + 1;
    break;
  }
  const float first_value = grid.ValueAt(first_column, first_row);
  const float second_value = grid.ValueAt(second_column, second_row);
  const float denominator = second_value - first_value;
  const float amount =
      std::abs(denominator) <= 0.000001F
          ? 0.5F
          : std::clamp((threshold - first_value) / denominator, 0.0F, 1.0F);
  const SkPoint first = grid.PointAt(first_column, first_row);
  const SkPoint second = grid.PointAt(second_column, second_row);
  return {
      KeyForEdge(column, row, edge),
      {std::lerp(first.fX, second.fX, amount),
       std::lerp(first.fY, second.fY, amount)},
  };
}

void AddSegment(const ScalarGrid &grid, int column, int row, CellEdge first,
                CellEdge second, float threshold,
                std::vector<ContourSegment> *segments) {
  segments->push_back({{
      VertexForEdge(grid, column, row, first, threshold),
      VertexForEdge(grid, column, row, second, threshold),
  }});
}

std::vector<ContourPolyline>
StitchSegments(const std::vector<ContourSegment> &segments) {
  std::unordered_map<EdgeKey, std::vector<std::size_t>, EdgeKeyHash> adjacency;
  adjacency.reserve(segments.size() * 2U);
  for (std::size_t index = 0; index < segments.size(); ++index) {
    adjacency[segments[index].vertices[0].key].push_back(index);
    adjacency[segments[index].vertices[1].key].push_back(index);
  }

  std::vector<bool> visited(segments.size(), false);
  std::vector<ContourPolyline> contours;
  contours.reserve(segments.size() / 2U + 1U);
  for (std::size_t seed = 0; seed < segments.size(); ++seed) {
    if (visited[seed]) {
      continue;
    }
    const ContourSegment &seed_segment = segments[seed];
    EdgeKey start_key = seed_segment.vertices[0].key;
    if (adjacency[start_key].size() != 1U &&
        adjacency[seed_segment.vertices[1].key].size() == 1U) {
      start_key = seed_segment.vertices[1].key;
    }

    ContourPolyline contour;
    EdgeKey current_key = start_key;
    std::size_t current_segment = seed;
    const auto seed_vertex = seed_segment.vertices[0].key == start_key
                                 ? seed_segment.vertices[0]
                                 : seed_segment.vertices[1];
    contour.points.push_back(seed_vertex.point);

    while (!visited[current_segment]) {
      visited[current_segment] = true;
      const ContourSegment &segment = segments[current_segment];
      const ContourVertex &other = segment.vertices[0].key == current_key
                                       ? segment.vertices[1]
                                       : segment.vertices[0];
      if (other.key == start_key) {
        contour.closed = true;
        break;
      }
      contour.points.push_back(other.point);
      current_key = other.key;

      std::size_t next_segment = segments.size();
      const auto adjacent = adjacency.find(current_key);
      if (adjacent != adjacency.end()) {
        for (std::size_t candidate : adjacent->second) {
          if (!visited[candidate]) {
            next_segment = candidate;
            break;
          }
        }
      }
      if (next_segment == segments.size()) {
        break;
      }
      current_segment = next_segment;
    }

    const std::size_t minimum_points = contour.closed ? 3U : 2U;
    if (contour.points.size() >= minimum_points) {
      contours.push_back(std::move(contour));
    }
  }
  return contours;
}

} // namespace

bool ScalarGrid::IsValid() const {
  if (column_count < 2 || row_count < 2 || bounds.isEmpty() ||
      !bounds.isFinite() ||
      values.size() != static_cast<std::size_t>(column_count) *
                           static_cast<std::size_t>(row_count)) {
    return false;
  }
  return std::all_of(values.begin(), values.end(),
                     [](float value) { return std::isfinite(value); });
}

float ScalarGrid::ValueAt(int column, int row) const {
  return values[static_cast<std::size_t>(row) *
                    static_cast<std::size_t>(column_count) +
                static_cast<std::size_t>(column)];
}

SkPoint ScalarGrid::PointAt(int column, int row) const {
  return {
      std::lerp(bounds.left(), bounds.right(),
                static_cast<float>(column) /
                    static_cast<float>(column_count - 1)),
      std::lerp(bounds.top(), bounds.bottom(),
                static_cast<float>(row) / static_cast<float>(row_count - 1)),
  };
}

std::vector<ContourPolyline>
ExtractMarchingSquaresContours(const ScalarGrid &grid, float threshold) {
  if (!grid.IsValid() || !std::isfinite(threshold)) {
    return {};
  }

  std::vector<ContourSegment> segments;
  segments.reserve(static_cast<std::size_t>(grid.column_count - 1) *
                   static_cast<std::size_t>(grid.row_count - 1) / 2U);
  for (int row = 0; row < grid.row_count - 1; ++row) {
    for (int column = 0; column < grid.column_count - 1; ++column) {
      const float top_left = grid.ValueAt(column, row);
      const float top_right = grid.ValueAt(column + 1, row);
      const float bottom_right = grid.ValueAt(column + 1, row + 1);
      const float bottom_left = grid.ValueAt(column, row + 1);
      const int cell_case = (top_left >= threshold ? 1 : 0) |
                            (top_right >= threshold ? 2 : 0) |
                            (bottom_right >= threshold ? 4 : 0) |
                            (bottom_left >= threshold ? 8 : 0);
      const float center =
          (top_left + top_right + bottom_right + bottom_left) * 0.25F;
      switch (cell_case) {
      case 0:
      case 15:
        break;
      case 1:
      case 14:
        AddSegment(grid, column, row, CellEdge::kTop, CellEdge::kLeft,
                   threshold, &segments);
        break;
      case 2:
      case 13:
        AddSegment(grid, column, row, CellEdge::kTop, CellEdge::kRight,
                   threshold, &segments);
        break;
      case 3:
      case 12:
        AddSegment(grid, column, row, CellEdge::kLeft, CellEdge::kRight,
                   threshold, &segments);
        break;
      case 4:
      case 11:
        AddSegment(grid, column, row, CellEdge::kRight, CellEdge::kBottom,
                   threshold, &segments);
        break;
      case 5:
        if (center >= threshold) {
          AddSegment(grid, column, row, CellEdge::kTop, CellEdge::kRight,
                     threshold, &segments);
          AddSegment(grid, column, row, CellEdge::kBottom, CellEdge::kLeft,
                     threshold, &segments);
        } else {
          AddSegment(grid, column, row, CellEdge::kTop, CellEdge::kLeft,
                     threshold, &segments);
          AddSegment(grid, column, row, CellEdge::kRight, CellEdge::kBottom,
                     threshold, &segments);
        }
        break;
      case 6:
      case 9:
        AddSegment(grid, column, row, CellEdge::kTop, CellEdge::kBottom,
                   threshold, &segments);
        break;
      case 7:
      case 8:
        AddSegment(grid, column, row, CellEdge::kBottom, CellEdge::kLeft,
                   threshold, &segments);
        break;
      case 10:
        if (center >= threshold) {
          AddSegment(grid, column, row, CellEdge::kTop, CellEdge::kLeft,
                     threshold, &segments);
          AddSegment(grid, column, row, CellEdge::kRight, CellEdge::kBottom,
                     threshold, &segments);
        } else {
          AddSegment(grid, column, row, CellEdge::kTop, CellEdge::kRight,
                     threshold, &segments);
          AddSegment(grid, column, row, CellEdge::kBottom, CellEdge::kLeft,
                     threshold, &segments);
        }
        break;
      }
    }
  }
  return StitchSegments(segments);
}

} // namespace geometry
