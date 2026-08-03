#include "morphing/topology_guard.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

#include "include/core/SkPathBuilder.h"
#include "include/core/SkPathMeasure.h"
#include "include/pathops/SkPathOps.h"

namespace skmorph::internal {
namespace {

constexpr float kGeometryEpsilon = 1e-5F;
constexpr int kCubicSamples = 12;
constexpr int kCorrectionIterations = 10;

struct BoundarySegment {
  SkPoint start = {0.0F, 0.0F};
  SkPoint end = {0.0F, 0.0F};
};

struct NearestBoundary {
  SkPoint point = {0.0F, 0.0F};
  SkVector tangent = {1.0F, 0.0F};
  float distance = std::numeric_limits<float>::infinity();
};

SkPoint Evaluate(const geometry::Cubic &cubic, float t) {
  const float u = 1.0F - t;
  return {
      u * u * u * cubic.points[0].x() + 3.0F * u * u * t * cubic.points[1].x() +
          3.0F * u * t * t * cubic.points[2].x() +
          t * t * t * cubic.points[3].x(),
      u * u * u * cubic.points[0].y() + 3.0F * u * u * t * cubic.points[1].y() +
          3.0F * u * t * t * cubic.points[2].y() +
          t * t * t * cubic.points[3].y(),
  };
}

std::array<float, 4> BernsteinWeights(float t) {
  const float u = 1.0F - t;
  return {u * u * u, 3.0F * u * u * t, 3.0F * u * t * t, t * t * t};
}

float Distance(SkPoint first, SkPoint second) {
  return std::hypot(second.x() - first.x(), second.y() - first.y());
}

float Cross(SkVector first, SkVector second) {
  return first.x() * second.y() - first.y() * second.x();
}

float Orientation(SkPoint first, SkPoint second, SkPoint third) {
  return Cross(second - first, third - first);
}

bool OnSegment(SkPoint start, SkPoint end, SkPoint point) {
  return point.x() >= std::min(start.x(), end.x()) - kGeometryEpsilon &&
         point.x() <= std::max(start.x(), end.x()) + kGeometryEpsilon &&
         point.y() >= std::min(start.y(), end.y()) - kGeometryEpsilon &&
         point.y() <= std::max(start.y(), end.y()) + kGeometryEpsilon;
}

bool SegmentsIntersect(const BoundarySegment &first,
                       const BoundarySegment &second) {
  const float o1 = Orientation(first.start, first.end, second.start);
  const float o2 = Orientation(first.start, first.end, second.end);
  const float o3 = Orientation(second.start, second.end, first.start);
  const float o4 = Orientation(second.start, second.end, first.end);
  if (((o1 > kGeometryEpsilon && o2 < -kGeometryEpsilon) ||
       (o1 < -kGeometryEpsilon && o2 > kGeometryEpsilon)) &&
      ((o3 > kGeometryEpsilon && o4 < -kGeometryEpsilon) ||
       (o3 < -kGeometryEpsilon && o4 > kGeometryEpsilon))) {
    return true;
  }
  return (std::abs(o1) <= kGeometryEpsilon &&
          OnSegment(first.start, first.end, second.start)) ||
         (std::abs(o2) <= kGeometryEpsilon &&
          OnSegment(first.start, first.end, second.end)) ||
         (std::abs(o3) <= kGeometryEpsilon &&
          OnSegment(second.start, second.end, first.start)) ||
         (std::abs(o4) <= kGeometryEpsilon &&
          OnSegment(second.start, second.end, first.end));
}

std::vector<SkPoint> FlattenCubicContour(const CubicContour &contour,
                                         float maximum_piece_length) {
  std::vector<SkPoint> points;
  if (contour.empty()) {
    return points;
  }
  points.push_back(contour.front().points[0]);
  for (const geometry::Cubic &cubic : contour) {
    const geometry::Curve curve = geometry::Curve::CubicBezier(
        cubic.points[0], cubic.points[1], cubic.points[2], cubic.points[3]);
    const int sample_count =
        std::clamp(static_cast<int>(std::ceil(
                       curve.Length() / std::max(0.25F, maximum_piece_length))),
                   4, 64);
    for (int sample = 1; sample <= sample_count; ++sample) {
      points.push_back(
          Evaluate(cubic, static_cast<float>(sample) / sample_count));
    }
  }
  if (points.size() > 1U && points.front() != points.back()) {
    points.push_back(points.front());
  }
  return points;
}

std::vector<BoundarySegment>
SegmentsFromPoints(const std::vector<SkPoint> &points) {
  std::vector<BoundarySegment> segments;
  if (points.size() < 2U) {
    return segments;
  }
  segments.reserve(points.size() - 1U);
  for (size_t index = 1; index < points.size(); ++index) {
    if (Distance(points[index - 1U], points[index]) > kGeometryEpsilon) {
      segments.push_back({points[index - 1U], points[index]});
    }
  }
  return segments;
}

std::vector<std::vector<BoundarySegment>>
FlattenPathContours(const SkPath &path) {
  std::vector<std::vector<BoundarySegment>> contours;
  SkPathMeasure measure(path, true, 2.0F);
  do {
    std::vector<BoundarySegment> segments;
    const float length = measure.getLength();
    if (length <= kGeometryEpsilon) {
      continue;
    }
    const int sample_count =
        std::clamp(static_cast<int>(std::ceil(length / 2.0F)), 12, 512);
    SkPoint previous;
    if (!measure.getPosTan(0.0F, &previous, nullptr)) {
      continue;
    }
    for (int sample = 1; sample <= sample_count; ++sample) {
      SkPoint current;
      if (!measure.getPosTan(length * static_cast<float>(sample) / sample_count,
                             &current, nullptr)) {
        continue;
      }
      if (Distance(previous, current) > kGeometryEpsilon) {
        segments.push_back({previous, current});
      }
      previous = current;
    }
    if (!segments.empty()) {
      contours.push_back(std::move(segments));
    }
  } while (measure.nextContour());
  return contours;
}

std::vector<BoundarySegment> FlattenPath(const SkPath &path) {
  std::vector<BoundarySegment> segments;
  for (std::vector<BoundarySegment> &contour : FlattenPathContours(path)) {
    segments.insert(segments.end(), contour.begin(), contour.end());
  }
  return segments;
}

NearestBoundary
FindNearestBoundary(SkPoint point,
                    const std::vector<BoundarySegment> &segments) {
  NearestBoundary nearest;
  for (const BoundarySegment &segment : segments) {
    const SkVector edge = segment.end - segment.start;
    const float length_squared = edge.x() * edge.x() + edge.y() * edge.y();
    if (length_squared <= kGeometryEpsilon * kGeometryEpsilon) {
      continue;
    }
    const SkVector offset = point - segment.start;
    const float projection = std::clamp(
        (offset.x() * edge.x() + offset.y() * edge.y()) / length_squared, 0.0F,
        1.0F);
    const SkPoint candidate = segment.start + edge * projection;
    const float distance = Distance(point, candidate);
    if (distance < nearest.distance) {
      nearest.point = candidate;
      nearest.tangent = edge;
      nearest.tangent.normalize();
      nearest.distance = distance;
    }
  }
  return nearest;
}

SkVector InwardDirection(const SkPath &outer, SkPoint point,
                         const NearestBoundary &nearest,
                         float required_clearance) {
  SkVector direction = point - nearest.point;
  if (outer.contains(point.x(), point.y()) && direction.normalize()) {
    return direction;
  }

  const SkVector normal = {-nearest.tangent.y(), nearest.tangent.x()};
  const float probe = std::max(0.5F, required_clearance + 0.25F);
  const SkPoint first_candidate = nearest.point + normal * probe;
  if (outer.contains(first_candidate.x(), first_candidate.y())) {
    return normal;
  }
  const SkPoint second_candidate = nearest.point - normal * probe;
  if (outer.contains(second_candidate.x(), second_candidate.y())) {
    return -normal;
  }
  return {0.0F, 0.0F};
}

float SampledClearance(const CubicContour &hole, const SkPath &outer,
                       const std::vector<BoundarySegment> &outer_segments) {
  if (hole.empty() || outer_segments.empty()) {
    return 0.0F;
  }
  float clearance = std::numeric_limits<float>::infinity();
  for (const geometry::Cubic &cubic : hole) {
    for (int sample = 0; sample <= kCubicSamples; ++sample) {
      const SkPoint point =
          Evaluate(cubic, static_cast<float>(sample) / kCubicSamples);
      if (!outer.contains(point.x(), point.y())) {
        return 0.0F;
      }
      clearance = std::min(clearance,
                           FindNearestBoundary(point, outer_segments).distance);
    }
  }
  return std::isfinite(clearance) ? clearance : 0.0F;
}

void SynchronizeAnchors(CubicContour *contour) {
  if (contour->empty()) {
    return;
  }
  for (size_t index = 0; index < contour->size(); ++index) {
    const size_t next = (index + 1U) % contour->size();
    const SkPoint shared = {
        ((*contour)[index].points[3].x() + (*contour)[next].points[0].x()) *
            0.5F,
        ((*contour)[index].points[3].y() + (*contour)[next].points[0].y()) *
            0.5F,
    };
    (*contour)[index].points[3] = shared;
    (*contour)[next].points[0] = shared;
  }
}

SkPoint ContourCenter(const CubicContour &contour) {
  if (contour.empty()) {
    return {0.0F, 0.0F};
  }
  SkVector accumulator = {0.0F, 0.0F};
  for (const geometry::Cubic &cubic : contour) {
    accumulator += cubic.points[0] - SkPoint::Make(0.0F, 0.0F);
  }
  return {accumulator.x() / static_cast<float>(contour.size()),
          accumulator.y() / static_cast<float>(contour.size())};
}

void ScaleAbout(CubicContour *contour, SkPoint center, float scale) {
  for (geometry::Cubic &cubic : *contour) {
    for (SkPoint &point : cubic.points) {
      point = center + (point - center) * scale;
    }
  }
}

SkPoint
FindSafeInteriorPoint(const SkPath &outer,
                      const std::vector<BoundarySegment> &outer_segments,
                      float required_clearance) {
  const SkRect bounds = outer.getBounds();
  const SkPoint center = {bounds.centerX(), bounds.centerY()};
  if (outer.contains(center.x(), center.y()) &&
      FindNearestBoundary(center, outer_segments).distance >=
          required_clearance) {
    return center;
  }
  constexpr int kGridSize = 16;
  SkPoint best = center;
  float best_clearance = -1.0F;
  for (int row = 1; row < kGridSize; ++row) {
    for (int column = 1; column < kGridSize; ++column) {
      const SkPoint point = {
          std::lerp(bounds.left(), bounds.right(),
                    static_cast<float>(column) / kGridSize),
          std::lerp(bounds.top(), bounds.bottom(),
                    static_cast<float>(row) / kGridSize),
      };
      if (!outer.contains(point.x(), point.y())) {
        continue;
      }
      const float clearance =
          FindNearestBoundary(point, outer_segments).distance;
      if (clearance > best_clearance) {
        best = point;
        best_clearance = clearance;
      }
    }
  }
  return best;
}

} // namespace

SkPath BuildCubicContourPath(const CubicContour &contour) {
  if (contour.empty()) {
    return {};
  }
  SkPathBuilder builder;
  builder.moveTo(contour.front().points[0]);
  for (const geometry::Cubic &cubic : contour) {
    builder.cubicTo(cubic.points[1], cubic.points[2], cubic.points[3]);
  }
  builder.close();
  return builder.detach();
}

bool HasSelfIntersection(const CubicContour &contour,
                         float maximum_flattened_piece_length) {
  const std::vector<BoundarySegment> segments = SegmentsFromPoints(
      FlattenCubicContour(contour, maximum_flattened_piece_length));
  for (size_t first = 0; first < segments.size(); ++first) {
    for (size_t second = first + 1U; second < segments.size(); ++second) {
      const bool adjacent = second == first + 1U ||
                            (first == 0U && second + 1U == segments.size());
      if (!adjacent && SegmentsIntersect(segments[first], segments[second])) {
        return true;
      }
    }
  }
  return false;
}

bool PathHasSelfIntersection(const SkPath &path) {
  const std::vector<std::vector<BoundarySegment>> contours =
      FlattenPathContours(path);
  for (size_t first_contour = 0; first_contour < contours.size();
       ++first_contour) {
    for (size_t second_contour = first_contour;
         second_contour < contours.size(); ++second_contour) {
      const std::vector<BoundarySegment> &first_segments =
          contours[first_contour];
      const std::vector<BoundarySegment> &second_segments =
          contours[second_contour];
      for (size_t first = 0; first < first_segments.size(); ++first) {
        const size_t second_start =
            first_contour == second_contour ? first + 1U : 0U;
        for (size_t second = second_start; second < second_segments.size();
             ++second) {
          const bool adjacent =
              first_contour == second_contour &&
              (second == first + 1U ||
               (first == 0U && second + 1U == first_segments.size()));
          if (!adjacent && SegmentsIntersect(first_segments[first],
                                             second_segments[second])) {
            return true;
          }
        }
      }
    }
  }
  return false;
}

SkPath MakeSimpleOuterPath(const CubicContour &contour) {
  SkPath path = BuildCubicContourPath(contour);
  path.setFillType(SkPathFillType::kEvenOdd);
  if (!HasSelfIntersection(contour)) {
    return path;
  }
  const std::optional<SkPath> simplified = Simplify(path);
  if (simplified.has_value() && !simplified->isEmpty()) {
    return *simplified;
  }
  return path;
}

float HoleClearance(const CubicContour &hole, const SkPath &outer) {
  return SampledClearance(hole, outer, FlattenPath(outer));
}

void EnforceHoleClearance(CubicContour *hole, const SkPath &outer,
                          float required_clearance) {
  if (hole == nullptr || hole->empty() || outer.isEmpty() ||
      required_clearance <= 0.0F) {
    return;
  }
  const std::vector<BoundarySegment> outer_segments = FlattenPath(outer);
  if (outer_segments.empty() ||
      SampledClearance(*hole, outer, outer_segments) >= required_clearance) {
    return;
  }

  for (int iteration = 0; iteration < kCorrectionIterations; ++iteration) {
    std::vector<std::array<SkVector, 4>> corrections(hole->size());
    std::vector<std::array<float, 4>> weights(hole->size());
    bool had_violation = false;
    for (size_t segment_index = 0; segment_index < hole->size();
         ++segment_index) {
      const geometry::Cubic cubic = (*hole)[segment_index];
      for (int sample = 0; sample <= kCubicSamples; ++sample) {
        const float t = static_cast<float>(sample) / kCubicSamples;
        const SkPoint point = Evaluate(cubic, t);
        const NearestBoundary nearest =
            FindNearestBoundary(point, outer_segments);
        const bool inside = outer.contains(point.x(), point.y());
        if (inside && nearest.distance >= required_clearance) {
          continue;
        }
        had_violation = true;
        const SkVector inward =
            InwardDirection(outer, point, nearest, required_clearance);
        if (inward.x() == 0.0F && inward.y() == 0.0F) {
          continue;
        }
        const SkPoint target =
            nearest.point +
            inward * (required_clearance + kGeometryEpsilon * 8.0F);
        const SkVector correction = target - point;
        const std::array<float, 4> basis = BernsteinWeights(t);
        for (size_t control = 0; control < basis.size(); ++control) {
          corrections[segment_index][control] += correction * basis[control];
          weights[segment_index][control] += basis[control] * basis[control];
        }
      }
    }
    if (!had_violation) {
      return;
    }
    for (size_t segment_index = 0; segment_index < hole->size();
         ++segment_index) {
      for (size_t control = 0; control < 4U; ++control) {
        if (weights[segment_index][control] > 0.0F) {
          (*hole)[segment_index].points[control] +=
              corrections[segment_index][control] *
              (0.65F / weights[segment_index][control]);
        }
      }
    }
    SynchronizeAnchors(hole);
    if (SampledClearance(*hole, outer, outer_segments) >= required_clearance) {
      return;
    }
  }

  const CubicContour locally_deformed = *hole;
  SkPoint center = ContourCenter(*hole);
  if (!outer.contains(center.x(), center.y()) ||
      FindNearestBoundary(center, outer_segments).distance <
          required_clearance) {
    center = FindSafeInteriorPoint(outer, outer_segments, required_clearance);
  }
  for (int step = 9; step >= 0; --step) {
    *hole = locally_deformed;
    ScaleAbout(hole, center, static_cast<float>(step) / 10.0F);
    if (SampledClearance(*hole, outer, outer_segments) >= required_clearance) {
      return;
    }
  }
  for (geometry::Cubic &cubic : *hole) {
    cubic = {{center, center, center, center}};
  }
}

} // namespace skmorph::internal
