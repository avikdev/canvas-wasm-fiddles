#include "geometry/shape_builder.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <vector>

#include "include/core/SkPathBuilder.h"

namespace geometry::shapes {
namespace {

constexpr float kTau = 2.0F * std::numbers::pi_v<float>;
constexpr float kQuarterCircleControl = 0.55228475F;

bool IsValid(SkPoint center, float radius, float rotation) {
  return std::isfinite(center.fX) && std::isfinite(center.fY) &&
         std::isfinite(radius) && radius > 0.0F && std::isfinite(rotation);
}

std::uint32_t Hash(std::uint32_t value) {
  value ^= value >> 16;
  value *= 0x7feb352dU;
  value ^= value >> 15;
  value *= 0x846ca68bU;
  value ^= value >> 16;
  return value;
}

float HashUnit(std::uint32_t seed, int index, std::uint32_t channel) {
  const std::uint32_t mixed = Hash(
      seed ^ (static_cast<std::uint32_t>(index + 1) * 0x9e3779b9U) ^ channel);
  return static_cast<float>(mixed & 0x00ffffffU) /
         static_cast<float>(0x01000000U);
}

SkPoint Transform(float local_x, float local_y, SkPoint center,
                  float rotation) {
  const float cosine = std::cos(rotation);
  const float sine = std::sin(rotation);
  return {
      center.fX + local_x * cosine - local_y * sine,
      center.fY + local_x * sine + local_y * cosine,
  };
}

SkPoint TentaclePoint(float radial, float lateral, float angle, SkPoint center,
                      float rotation) {
  const float local_x = std::cos(angle) * radial - std::sin(angle) * lateral;
  const float local_y = std::sin(angle) * radial + std::cos(angle) * lateral;
  return Transform(local_x, local_y, center, rotation);
}

void AppendPolygonContour(SkPathBuilder *builder, SkPoint center, float radius,
                          int side_count, float rotation) {
  for (int side = 0; side < side_count; ++side) {
    const float angle = rotation + kTau * static_cast<float>(side) /
                                       static_cast<float>(side_count);
    const SkPoint point = {
        center.fX + std::cos(angle) * radius,
        center.fY + std::sin(angle) * radius,
    };
    if (side == 0) {
      builder->moveTo(point);
    } else {
      builder->lineTo(point);
    }
  }
  builder->close();
}

void AppendSemicircleContour(SkPathBuilder *builder, SkPoint center,
                             float radius, float rotation) {
  std::array<SkPoint, 7> points = {
      SkPoint{-radius, 0.0F},
      SkPoint{-radius, -radius * kQuarterCircleControl},
      SkPoint{-radius * kQuarterCircleControl, -radius},
      SkPoint{0.0F, -radius},
      SkPoint{radius * kQuarterCircleControl, -radius},
      SkPoint{radius, -radius * kQuarterCircleControl},
      SkPoint{radius, 0.0F},
  };
  for (SkPoint &point : points) {
    point = Transform(point.fX, point.fY, center, rotation);
  }
  builder->moveTo(points[0])
      .cubicTo(points[1], points[2], points[3])
      .cubicTo(points[4], points[5], points[6])
      .lineTo(points[0])
      .close();
}

struct Tentacle {
  SkPoint root_left;
  SkPoint control1_left;
  SkPoint control2_left;
  SkPoint tip_left;
  SkPoint cap;
  SkPoint tip_right;
  SkPoint control2_right;
  SkPoint control1_right;
  SkPoint root_right;
  SkPoint connector_before;
  SkPoint connector_after;
};

} // namespace

SkPath MakeTentacledBlob(SkPoint center, float outer_radius, int tentacle_count,
                         float rotation_radians, std::uint32_t seed) {
  if (!IsValid(center, outer_radius, rotation_radians)) {
    return {};
  }
  tentacle_count = std::clamp(tentacle_count, 3, 12);
  const float sector = kTau / static_cast<float>(tentacle_count);
  const float phase = HashUnit(seed, 0, 0x243f6a88U) * sector;
  std::vector<Tentacle> tentacles(tentacle_count);

  for (int index = 0; index < tentacle_count; ++index) {
    const float angle = phase + static_cast<float>(index) * sector;
    const float body_radius =
        outer_radius * (0.38F + HashUnit(seed, index, 0x85a308d3U) * 0.05F);
    const float length =
        outer_radius * (0.80F + HashUnit(seed, index, 0x13198a2eU) * 0.14F);
    const float root_half_width =
        outer_radius * (0.11F + HashUnit(seed, index, 0x03707344U) * 0.025F);
    const float tip_half_width =
        outer_radius * (0.040F + HashUnit(seed, index, 0xa4093822U) * 0.015F);
    const float bend_sign =
        ((index + static_cast<int>(seed & 1U)) % 2 == 0) ? 1.0F : -1.0F;
    const float bend = bend_sign * outer_radius *
                       (0.08F + HashUnit(seed, index, 0x299f31d0U) * 0.08F);
    const float control1_radius = std::lerp(body_radius, length, 0.30F);
    const float control2_radius = std::lerp(body_radius, length, 0.72F);
    const float tip_lateral = bend * 0.70F;

    Tentacle &tentacle = tentacles[index];
    tentacle.root_left = TentaclePoint(body_radius, -root_half_width, angle,
                                       center, rotation_radians);
    tentacle.control1_left =
        TentaclePoint(control1_radius, bend - root_half_width * 0.70F, angle,
                      center, rotation_radians);
    tentacle.control2_left =
        TentaclePoint(control2_radius, -bend * 0.55F - tip_half_width * 1.60F,
                      angle, center, rotation_radians);
    tentacle.tip_left = TentaclePoint(length, tip_lateral - tip_half_width,
                                      angle, center, rotation_radians);
    tentacle.cap =
        TentaclePoint(std::min(outer_radius, length + tip_half_width * 1.45F),
                      tip_lateral, angle, center, rotation_radians);
    tentacle.tip_right = TentaclePoint(length, tip_lateral + tip_half_width,
                                       angle, center, rotation_radians);
    tentacle.control2_right =
        TentaclePoint(control2_radius, -bend * 0.55F + tip_half_width * 1.60F,
                      angle, center, rotation_radians);
    tentacle.control1_right =
        TentaclePoint(control1_radius, bend + root_half_width * 0.70F, angle,
                      center, rotation_radians);
    tentacle.root_right = TentaclePoint(body_radius, root_half_width, angle,
                                        center, rotation_radians);
    tentacle.connector_before = TentaclePoint(
        body_radius, 0.0F, angle - sector * 0.30F, center, rotation_radians);
    tentacle.connector_after = TentaclePoint(
        body_radius, 0.0F, angle + sector * 0.30F, center, rotation_radians);
  }

  SkPathBuilder builder;
  builder.moveTo(tentacles.front().root_left);
  for (int index = 0; index < tentacle_count; ++index) {
    const Tentacle &tentacle = tentacles[index];
    if (index > 0) {
      const Tentacle &previous = tentacles[index - 1];
      builder.cubicTo(previous.connector_after, tentacle.connector_before,
                      tentacle.root_left);
    }
    builder.cubicTo(tentacle.control1_left, tentacle.control2_left,
                    tentacle.tip_left);
    builder.quadTo(tentacle.cap, tentacle.tip_right);
    builder.cubicTo(tentacle.control2_right, tentacle.control1_right,
                    tentacle.root_right);
  }
  builder.cubicTo(tentacles.back().connector_after,
                  tentacles.front().connector_before,
                  tentacles.front().root_left);
  builder.close();
  return builder.detach();
}

SkPath MakeCross(SkPoint center, float outer_radius, float rotation_radians,
                 float arm_width_ratio) {
  if (!IsValid(center, outer_radius, rotation_radians) ||
      !std::isfinite(arm_width_ratio)) {
    return {};
  }
  const float half_width =
      outer_radius * std::clamp(arm_width_ratio, 0.08F, 0.72F);
  const std::array<SkPoint, 12> local_points = {{
      {-half_width, -outer_radius},
      {half_width, -outer_radius},
      {half_width, -half_width},
      {outer_radius, -half_width},
      {outer_radius, half_width},
      {half_width, half_width},
      {half_width, outer_radius},
      {-half_width, outer_radius},
      {-half_width, half_width},
      {-outer_radius, half_width},
      {-outer_radius, -half_width},
      {-half_width, -half_width},
  }};

  SkPathBuilder builder;
  for (std::size_t index = 0; index < local_points.size(); ++index) {
    const SkPoint point =
        Transform(local_points[index].fX, local_points[index].fY, center,
                  rotation_radians);
    if (index == 0U) {
      builder.moveTo(point);
    } else {
      builder.lineTo(point);
    }
  }
  builder.close();
  return builder.detach();
}

SkPath MakePolygonWithHole(SkPoint center, float outer_radius, int side_count,
                           float rotation_radians, float hole_radius_ratio) {
  if (!IsValid(center, outer_radius, rotation_radians) ||
      !std::isfinite(hole_radius_ratio)) {
    return {};
  }
  side_count = std::clamp(side_count, 3, 16);
  const float inner_radius =
      outer_radius * std::clamp(hole_radius_ratio, 0.08F, 0.82F);
  SkPathBuilder builder;
  builder.setFillType(SkPathFillType::kEvenOdd);
  AppendPolygonContour(&builder, center, outer_radius, side_count,
                       rotation_radians);
  AppendPolygonContour(&builder, center, inner_radius, side_count,
                       rotation_radians);
  return builder.detach();
}

SkPath MakeSemicircleWithHole(SkPoint center, float outer_radius,
                              float rotation_radians, float hole_radius_ratio) {
  if (!IsValid(center, outer_radius, rotation_radians) ||
      !std::isfinite(hole_radius_ratio)) {
    return {};
  }
  const float inner_radius =
      outer_radius * std::clamp(hole_radius_ratio, 0.08F, 0.82F);
  SkPathBuilder builder;
  builder.setFillType(SkPathFillType::kEvenOdd);
  AppendSemicircleContour(&builder, center, outer_radius, rotation_radians);
  AppendSemicircleContour(&builder, center, inner_radius, rotation_radians);
  return builder.detach();
}

SkPath MakeStar(SkPoint center, float outer_radius, int point_count,
                float rotation_radians, float inner_radius_ratio) {
  if (!IsValid(center, outer_radius, rotation_radians) ||
      !std::isfinite(inner_radius_ratio)) {
    return {};
  }
  point_count = std::clamp(point_count, 3, 16);
  const float inner_radius =
      outer_radius * std::clamp(inner_radius_ratio, 0.30F, 0.86F);
  const int vertex_count = point_count * 2;

  SkPathBuilder builder;
  for (int vertex = 0; vertex < vertex_count; ++vertex) {
    const float radius = vertex % 2 == 0 ? outer_radius : inner_radius;
    const float angle = rotation_radians + kTau * static_cast<float>(vertex) /
                                               static_cast<float>(vertex_count);
    const SkPoint point = {
        center.fX + std::cos(angle) * radius,
        center.fY + std::sin(angle) * radius,
    };
    if (vertex == 0) {
      builder.moveTo(point);
    } else {
      builder.lineTo(point);
    }
  }
  builder.close();
  return builder.detach();
}

} // namespace geometry::shapes
