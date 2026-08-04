#include "fiddles/vortex_field_fiddle.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <numbers>
#include <optional>
#include <random>
#include <utility>
#include <vector>

#include "geometry/catmull_rom_spline.h"
#include "geometry/shape_builder.h"
#include "graphics/webgl_canvas_context.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkPathMeasure.h"
#include "include/core/SkPoint.h"
#include "include/core/SkRect.h"
#include "include/core/SkSurface.h"
#include "include/effects/SkDashPathEffect.h"
#include "include/pathops/SkPathOps.h"
#include "utils/color_utils.h"

namespace {

// The target grid cell is one quarter of the shorter canvas dimension. Cells
// stay square and the complete grid is centered in the unused canvas space.
constexpr float kGridDivisor = 4.0F;
// Equal inset on all four sides of every square cell.
constexpr float kShapePaddingRatio = 0.07F;
// Closely spaced samples let the local field bend curves without exposing the
// original path's much coarser verb boundaries. The three-turn twist requires
// denser sampling than a mild local bend.
constexpr float kCurveSampleSpacing = 2.25F;
// Samples nearest the center rotate through three full turns. The smoothly
// decreasing radial angle is what turns an intersecting edge into a spiral.
constexpr float kMaximumTwistRadians = 3.0F * 2.0F * std::numbers::pi_v<float>;
// The old straight-line velocity was hypot(0.42, 0.31) cells/second. Motion is
// deliberately reduced to 70% of that speed before per-vortex variation.
constexpr float kVortexSpeedCellsPerSecond = 0.36541F;
// Heading blends toward coverage waypoints while two sine waves continuously
// perturb it. The combination visits the full canvas without sharp jitter.
constexpr float kCoverageSteeringPerSecond = 0.62F;
constexpr float kPrimaryWanderRadiansPerSecond = 0.52F;
constexpr float kSecondaryWanderRadiansPerSecond = 0.19F;
constexpr float kWaypointTimeoutSeconds = 8.0F;
constexpr float kCollisionClearanceCells = 0.035F;
constexpr float kCollisionDeflectionRadians = 0.27F;
constexpr float kWallDeflectionRadians = 0.20F;
constexpr float kShapeSaturation = 0.86F;
constexpr float kShapeLightness = 0.55F;
constexpr int kHueCount = 12;
constexpr SkColor kCanvasColor = SK_ColorWHITE;
constexpr std::size_t kVortexCount = 4U;
constexpr std::array<float, kVortexCount> kVortexDiameterCellRatios = {
    0.50F, 0.80F, 1.00F, 1.40F};
constexpr std::array<float, kVortexCount> kVortexSpeedScales = {0.92F, 1.04F,
                                                                0.97F, 1.08F};
constexpr std::array<float, kVortexCount> kTwistDirections = {1.0F, 1.0F, -1.0F,
                                                              -1.0F};
constexpr std::array<SkScalar, 2> kVortexDashIntervals = {2.0F, 4.0F};
constexpr std::array<SkPoint, 16> kCoverageWaypoints = {{
    {0.08F, 0.10F},
    {0.38F, 0.16F},
    {0.70F, 0.08F},
    {0.92F, 0.24F},
    {0.78F, 0.42F},
    {0.94F, 0.70F},
    {0.82F, 0.92F},
    {0.52F, 0.80F},
    {0.42F, 0.96F},
    {0.12F, 0.84F},
    {0.22F, 0.62F},
    {0.06F, 0.38F},
    {0.34F, 0.48F},
    {0.58F, 0.30F},
    {0.66F, 0.66F},
    {0.42F, 0.70F},
}};

enum class ShapeKind {
  kCircle,
  kTriangle,
  kRectangle,
  kHexagon,
  kSemicircle,
  kBlob,
  kTentacledBlob,
  kCross,
  kPolygonWithHole,
  kSemicircleWithHole,
  kStar,
};

struct VortexFrame {
  SkPoint center;
  float radius;
  float twist_direction;
  SkPath path;
};

float RandomUnit(std::mt19937 *random) {
  return std::uniform_real_distribution<float>(0.0F, 1.0F)(*random);
}

float PointLength(const SkPoint &point) {
  return std::hypot(point.fX, point.fY);
}

SkPoint UnitVector(const SkPoint &point, SkPoint fallback = {1.0F, 0.0F}) {
  const float length = PointLength(point);
  if (length <= 0.0001F || !std::isfinite(length)) {
    return fallback;
  }
  return {point.fX / length, point.fY / length};
}

SkPoint RotateVector(const SkPoint &point, float radians) {
  const float cosine = std::cos(radians);
  const float sine = std::sin(radians);
  return {
      point.fX * cosine - point.fY * sine,
      point.fX * sine + point.fY * cosine,
  };
}

SkPoint WithLength(const SkPoint &point, float length) {
  const SkPoint direction = UnitVector(point);
  return {direction.fX * length, direction.fY * length};
}

float VortexRadius(const VortexMotionState &vortex, float cell_size) {
  return vortex.diameter_cell_ratio * cell_size * 0.5F;
}

SkPoint CoverageTarget(const VortexMotionState &vortex, float width,
                       float height, float radius) {
  const SkPoint normalized =
      kCoverageWaypoints[static_cast<std::size_t>(vortex.waypoint_index) %
                         kCoverageWaypoints.size()];
  return {
      std::lerp(radius, std::max(radius, width - radius), normalized.fX),
      std::lerp(radius, std::max(radius, height - radius), normalized.fY),
  };
}

void AdvanceWaypoint(VortexMotionState *vortex, std::size_t vortex_index) {
  constexpr std::array<int, kVortexCount> kWaypointSteps = {5, 7, 9, 11};
  vortex->waypoint_index =
      (vortex->waypoint_index + kWaypointSteps[vortex_index]) %
      static_cast<int>(kCoverageWaypoints.size());
  vortex->waypoint_age_seconds = 0.0F;
}

void KeepInsideCanvas(VortexMotionState *vortex, float width, float height,
                      float radius, float deflection) {
  bool hit_horizontal = false;
  bool hit_vertical = false;
  if (vortex->center.fX < radius) {
    vortex->center.fX = radius;
    vortex->velocity.fX = std::abs(vortex->velocity.fX);
    hit_vertical = true;
  } else if (vortex->center.fX > width - radius) {
    vortex->center.fX = width - radius;
    vortex->velocity.fX = -std::abs(vortex->velocity.fX);
    hit_vertical = true;
  }
  if (vortex->center.fY < radius) {
    vortex->center.fY = radius;
    vortex->velocity.fY = std::abs(vortex->velocity.fY);
    hit_horizontal = true;
  } else if (vortex->center.fY > height - radius) {
    vortex->center.fY = height - radius;
    vortex->velocity.fY = -std::abs(vortex->velocity.fY);
    hit_horizontal = true;
  }
  if (hit_horizontal || hit_vertical) {
    vortex->velocity = RotateVector(vortex->velocity, deflection);
    if (vortex->center.fX <= radius) {
      vortex->velocity.fX = std::abs(vortex->velocity.fX);
    } else if (vortex->center.fX >= width - radius) {
      vortex->velocity.fX = -std::abs(vortex->velocity.fX);
    }
    if (vortex->center.fY <= radius) {
      vortex->velocity.fY = std::abs(vortex->velocity.fY);
    } else if (vortex->center.fY >= height - radius) {
      vortex->velocity.fY = -std::abs(vortex->velocity.fY);
    }
  }
}

SkPoint TransformPoint(float local_x, float local_y, const SkPoint &center,
                       float rotation) {
  const float cosine = std::cos(rotation);
  const float sine = std::sin(rotation);
  return {
      center.fX + local_x * cosine - local_y * sine,
      center.fY + local_x * sine + local_y * cosine,
  };
}

SkPath RegularPolygon(int side_count, const SkPoint &center, float radius,
                      float rotation) {
  SkPathBuilder builder;
  for (int side = 0; side < side_count; ++side) {
    const float angle = rotation + 2.0F * std::numbers::pi_v<float> *
                                       static_cast<float>(side) /
                                       static_cast<float>(side_count);
    const SkPoint point = {
        center.fX + std::cos(angle) * radius,
        center.fY + std::sin(angle) * radius,
    };
    if (side == 0) {
      builder.moveTo(point);
    } else {
      builder.lineTo(point);
    }
  }
  builder.close();
  return builder.detach();
}

SkPath Rectangle(const SkPoint &center, float half_width, float half_height,
                 float rotation) {
  constexpr std::array<SkPoint, 4> kCorners = {
      SkPoint{-1.0F, -1.0F}, SkPoint{1.0F, -1.0F}, SkPoint{1.0F, 1.0F},
      SkPoint{-1.0F, 1.0F}};
  SkPathBuilder builder;
  for (std::size_t index = 0; index < kCorners.size(); ++index) {
    const SkPoint point =
        TransformPoint(kCorners[index].fX * half_width,
                       kCorners[index].fY * half_height, center, rotation);
    if (index == 0U) {
      builder.moveTo(point);
    } else {
      builder.lineTo(point);
    }
  }
  builder.close();
  return builder.detach();
}

SkPath Semicircle(const SkPoint &center, float radius, float rotation) {
  constexpr float kQuarterCircleControl = 0.55228475F;
  std::array<SkPoint, 7> local = {
      SkPoint{-radius, 0.0F},
      SkPoint{-radius, -radius * kQuarterCircleControl},
      SkPoint{-radius * kQuarterCircleControl, -radius},
      SkPoint{0.0F, -radius},
      SkPoint{radius * kQuarterCircleControl, -radius},
      SkPoint{radius, -radius * kQuarterCircleControl},
      SkPoint{radius, 0.0F},
  };
  for (SkPoint &point : local) {
    point = TransformPoint(point.fX, point.fY, center, rotation);
  }

  SkPathBuilder builder;
  builder.moveTo(local[0])
      .cubicTo(local[1], local[2], local[3])
      .cubicTo(local[4], local[5], local[6])
      .lineTo(local[0])
      .close();
  return builder.detach();
}

SkPath Blob(const SkPoint &center, float radius, float rotation,
            std::mt19937 *random) {
  constexpr int kAnchorCount = 9;
  std::vector<SkPoint> anchors;
  anchors.reserve(kAnchorCount);
  for (int anchor = 0; anchor < kAnchorCount; ++anchor) {
    const float angle = rotation + 2.0F * std::numbers::pi_v<float> *
                                       static_cast<float>(anchor) /
                                       static_cast<float>(kAnchorCount);
    const float displaced_radius =
        radius * (0.74F + RandomUnit(random) * 0.28F);
    anchors.push_back({
        center.fX + std::cos(angle) * displaced_radius,
        center.fY + std::sin(angle) * displaced_radius,
    });
  }
  geometry::CatmullRomOptions options;
  options.closed = true;
  return geometry::CatmullRomToCubicPath(anchors, options);
}

SkPath BuildShape(ShapeKind kind, const SkPoint &center, float radius,
                  std::mt19937 *random) {
  const float rotation = RandomUnit(random) * 2.0F * std::numbers::pi_v<float>;
  switch (kind) {
  case ShapeKind::kCircle: {
    SkPathBuilder builder;
    builder.addCircle(center.fX, center.fY, radius);
    return builder.detach();
  }
  case ShapeKind::kTriangle:
    return RegularPolygon(3, center, radius, rotation);
  case ShapeKind::kRectangle: {
    const float aspect = 0.62F + RandomUnit(random) * 0.30F;
    const float half_width = radius / std::sqrt(1.0F + aspect * aspect);
    return Rectangle(center, half_width, half_width * aspect, rotation);
  }
  case ShapeKind::kHexagon:
    return RegularPolygon(6, center, radius, rotation);
  case ShapeKind::kSemicircle:
    return Semicircle(center, radius, rotation);
  case ShapeKind::kBlob:
    return Blob(center, radius, rotation, random);
  case ShapeKind::kTentacledBlob:
    return geometry::shapes::MakeTentacledBlob(
        center, radius, 3 + static_cast<int>((*random)() % 4U), rotation,
        (*random)());
  case ShapeKind::kCross:
    return geometry::shapes::MakeCross(center, radius, rotation, 0.27F);
  case ShapeKind::kPolygonWithHole:
    return geometry::shapes::MakePolygonWithHole(
        center, radius, 3 + static_cast<int>((*random)() % 5U), rotation,
        0.42F);
  case ShapeKind::kSemicircleWithHole:
    return geometry::shapes::MakeSemicircleWithHole(center, radius, rotation,
                                                    0.48F);
  case ShapeKind::kStar:
    return geometry::shapes::MakeStar(center, radius,
                                      6 + static_cast<int>((*random)() % 2U),
                                      rotation, 0.58F);
  }
  return {};
}

std::vector<VortexContourSamples> SampleContours(const SkPath &path) {
  std::vector<VortexContourSamples> contours;
  SkPathMeasure measure(path, false, 1.0F);
  do {
    const float length = measure.getLength();
    if (!std::isfinite(length) || length <= 0.0F) {
      continue;
    }
    const int sample_count =
        std::max(8, static_cast<int>(std::ceil(length / kCurveSampleSpacing)));
    VortexContourSamples contour;
    contour.closed = measure.isClosed();
    contour.points.reserve(sample_count);
    const float denominator =
        contour.closed ? static_cast<float>(sample_count)
                       : static_cast<float>(std::max(1, sample_count - 1));
    for (int sample = 0; sample < sample_count; ++sample) {
      SkPoint point;
      if (measure.getPosTan(length * static_cast<float>(sample) / denominator,
                            &point, nullptr)) {
        contour.points.push_back(point);
      }
    }
    if ((!contour.closed && contour.points.size() >= 2U) ||
        (contour.closed && contour.points.size() >= 3U)) {
      contours.push_back(std::move(contour));
    }
  } while (measure.nextContour());
  return contours;
}

float SmoothFalloff(float normalized_distance) {
  const float value = std::clamp(normalized_distance, 0.0F, 1.0F);
  const float smooth_distance = value * value * (3.0F - 2.0F * value);
  return 1.0F - smooth_distance;
}

SkPoint ApplyVortex(const SkPoint &point, const SkPoint &center, float radius,
                    float twist_direction) {
  const float delta_x = point.fX - center.fX;
  const float delta_y = point.fY - center.fY;
  const float distance = std::hypot(delta_x, delta_y);
  if (distance >= radius || radius <= 0.0F) {
    return point;
  }
  const float rotation =
      twist_direction * kMaximumTwistRadians * SmoothFalloff(distance / radius);
  const float cosine = std::cos(rotation);
  const float sine = std::sin(rotation);
  return {
      center.fX + delta_x * cosine - delta_y * sine,
      center.fY + delta_x * sine + delta_y * cosine,
  };
}

SkPath DeformedPath(const VortexGridShape &shape,
                    const std::array<VortexFrame, kVortexCount> &vortex_frames,
                    const std::array<bool, kVortexCount> &active_vortices) {
  SkPathBuilder result;
  result.setFillType(shape.path.getFillType());
  for (const VortexContourSamples &contour : shape.contours) {
    std::vector<SkPoint> displaced;
    displaced.reserve(contour.points.size());
    for (const SkPoint &point : contour.points) {
      SkPoint displaced_point = point;
      for (std::size_t vortex_index = 0; vortex_index < kVortexCount;
           ++vortex_index) {
        if (!active_vortices[vortex_index]) {
          continue;
        }
        const VortexFrame &vortex = vortex_frames[vortex_index];
        displaced_point = ApplyVortex(displaced_point, vortex.center,
                                      vortex.radius, vortex.twist_direction);
      }
      displaced.push_back(displaced_point);
    }
    geometry::CatmullRomOptions options;
    options.closed = contour.closed;
    const SkPath rebuilt = geometry::CatmullRomToCubicPath(displaced, options);
    if (!rebuilt.isEmpty()) {
      result.addPath(rebuilt);
    }
  }
  return result.detach();
}

} // namespace

VortexFieldFiddle::VortexFieldFiddle() : field_seed_(std::random_device{}()) {}

VortexFieldFiddle::~VortexFieldFiddle() = default;

bool VortexFieldFiddle::EnsureResources() {
  if (webgl_ != nullptr) {
    return true;
  }
  if (initialization_attempted_) {
    return false;
  }
  initialization_attempted_ = true;

  auto webgl = std::make_unique<WebGlCanvasContext>();
  if (!webgl->Initialize(WebGlResource())) {
    return false;
  }
  dash_path_effect_ = SkDashPathEffect::Make(kVortexDashIntervals, 0.0F);
  if (dash_path_effect_ == nullptr) {
    return false;
  }
  webgl_ = std::move(webgl);
  std::cout << "[cc-engine/stdout] Vortex field Skia and Path Ops resources "
               "ready."
            << std::endl;
  return true;
}

bool VortexFieldFiddle::RebuildGrid(float width, float height) {
  if (width <= 0.0F || height <= 0.0F) {
    return false;
  }

  cell_size_ = std::min(width, height) / kGridDivisor;
  const int column_count =
      std::max(1, static_cast<int>(std::floor(width / cell_size_)));
  const int row_count =
      std::max(1, static_cast<int>(std::floor(height / cell_size_)));
  const float grid_width = static_cast<float>(column_count) * cell_size_;
  const float grid_height = static_cast<float>(row_count) * cell_size_;
  const float grid_left = (width - grid_width) * 0.5F;
  const float grid_top = (height - grid_height) * 0.5F;
  const float radius = cell_size_ * (0.5F - kShapePaddingRatio);

  std::mt19937 random(field_seed_);
  std::uniform_int_distribution<int> hue_distribution(0, kHueCount - 1);
  const int shape_kind_count = static_cast<int>(ShapeKind::kStar) + 1;
  const int cell_count = column_count * row_count;
  std::vector<ShapeKind> shape_kinds;
  shape_kinds.reserve(cell_count);
  const int kind_offset = static_cast<int>(random() % shape_kind_count);
  for (int index = 0; index < cell_count; ++index) {
    shape_kinds.push_back(
        static_cast<ShapeKind>((index + kind_offset) % shape_kind_count));
  }
  std::shuffle(shape_kinds.begin(), shape_kinds.end(), random);

  std::vector<VortexGridShape> rebuilt;
  rebuilt.reserve(static_cast<std::size_t>(cell_count));

  int cell_index = 0;
  for (int row = 0; row < row_count; ++row) {
    for (int column = 0; column < column_count; ++column) {
      const SkPoint center = {
          grid_left + (static_cast<float>(column) + 0.5F) * cell_size_,
          grid_top + (static_cast<float>(row) + 0.5F) * cell_size_,
      };
      const ShapeKind kind = shape_kinds[cell_index++];
      SkPath path = BuildShape(kind, center, radius, &random);
      std::vector<VortexContourSamples> contours = SampleContours(path);
      if (path.isEmpty() || contours.empty()) {
        continue;
      }
      const float hue = 360.0F * static_cast<float>(hue_distribution(random)) /
                        static_cast<float>(kHueCount);
      rebuilt.push_back({
          std::move(path),
          std::move(contours),
          color_utils::FromHsl(hue, kShapeSaturation, kShapeLightness),
      });
    }
  }

  shapes_ = std::move(rebuilt);
  motion_initialized_ = false;
  cached_width_ = static_cast<int>(width);
  cached_height_ = static_cast<int>(height);
  return !shapes_.empty();
}

void VortexFieldFiddle::InitializeVortices(float width, float height,
                                           double time_seconds) {
  constexpr std::array<SkPoint, kVortexCount> kInitialPositions = {{
      {0.15F, 0.18F},
      {0.72F, 0.16F},
      {0.22F, 0.74F},
      {0.76F, 0.72F},
  }};
  constexpr std::array<float, kVortexCount> kInitialAngles = {0.18F, 2.42F,
                                                              5.05F, 3.72F};

  for (std::size_t index = 0; index < kVortexCount; ++index) {
    VortexMotionState &vortex = vortices_[index];
    vortex.diameter_cell_ratio = kVortexDiameterCellRatios[index];
    vortex.speed_scale = kVortexSpeedScales[index];
    vortex.twist_direction = kTwistDirections[index];
    vortex.wander_phase = 0.73F + static_cast<float>(index) * 1.61F;
    vortex.waypoint_index =
        static_cast<int>((index * 4U + 2U) % kCoverageWaypoints.size());
    vortex.waypoint_age_seconds = 0.0F;
    const float radius = VortexRadius(vortex, cell_size_);
    vortex.center = {
        std::clamp(kInitialPositions[index].fX * width, radius, width - radius),
        std::clamp(kInitialPositions[index].fY * height, radius,
                   height - radius),
    };
    const float speed =
        cell_size_ * kVortexSpeedCellsPerSecond * vortex.speed_scale;
    vortex.velocity = {
        std::cos(kInitialAngles[index]) * speed,
        std::sin(kInitialAngles[index]) * speed,
    };
  }
  last_motion_time_seconds_ = time_seconds;
  motion_initialized_ = true;
}

void VortexFieldFiddle::UpdateVortices(float width, float height,
                                       double time_seconds) {
  if (!motion_initialized_) {
    InitializeVortices(width, height, time_seconds);
    return;
  }
  const float delta_seconds = static_cast<float>(
      std::clamp(time_seconds - last_motion_time_seconds_, 0.0, 0.1));
  last_motion_time_seconds_ = time_seconds;
  if (delta_seconds <= 0.0F) {
    return;
  }

  for (std::size_t index = 0; index < kVortexCount; ++index) {
    VortexMotionState &vortex = vortices_[index];
    const float radius = VortexRadius(vortex, cell_size_);
    const SkPoint target = CoverageTarget(vortex, width, height, radius);
    const SkPoint to_target = {target.fX - vortex.center.fX,
                               target.fY - vortex.center.fY};
    vortex.waypoint_age_seconds += delta_seconds;
    if (PointLength(to_target) < std::max(cell_size_ * 0.18F, radius * 0.25F) ||
        vortex.waypoint_age_seconds >= kWaypointTimeoutSeconds) {
      AdvanceWaypoint(&vortex, index);
    }

    const SkPoint desired_direction = UnitVector(to_target);
    const SkPoint current_direction = UnitVector(vortex.velocity);
    const float steering =
        std::clamp(kCoverageSteeringPerSecond * delta_seconds, 0.0F, 1.0F);
    SkPoint direction = UnitVector(
        {std::lerp(current_direction.fX, desired_direction.fX, steering),
         std::lerp(current_direction.fY, desired_direction.fY, steering)},
        current_direction);
    const float time = static_cast<float>(time_seconds);
    const float wander_rate =
        std::sin(time * 0.79F + vortex.wander_phase) *
            kPrimaryWanderRadiansPerSecond +
        std::sin(time * 1.83F + vortex.wander_phase * 1.71F) *
            kSecondaryWanderRadiansPerSecond;
    direction = RotateVector(direction, wander_rate * delta_seconds);
    const float speed =
        cell_size_ * kVortexSpeedCellsPerSecond * vortex.speed_scale;
    vortex.velocity = {direction.fX * speed, direction.fY * speed};
    vortex.center.fX += vortex.velocity.fX * delta_seconds;
    vortex.center.fY += vortex.velocity.fY * delta_seconds;

    const float wall_sign = index % 2U == 0U ? 1.0F : -1.0F;
    const float wall_variation =
        0.72F + 0.28F * std::sin(time * 0.47F + vortex.wander_phase * 0.83F);
    KeepInsideCanvas(&vortex, width, height, radius,
                     wall_sign * kWallDeflectionRadians * wall_variation);
  }

  // Two solver passes are enough for four bodies and also handle a body being
  // separated into a third one during the first pass.
  for (int pass = 0; pass < 2; ++pass) {
    for (std::size_t first = 0; first < kVortexCount; ++first) {
      for (std::size_t second = first + 1U; second < kVortexCount; ++second) {
        VortexMotionState &a = vortices_[first];
        VortexMotionState &b = vortices_[second];
        const float radius_a = VortexRadius(a, cell_size_);
        const float radius_b = VortexRadius(b, cell_size_);
        const float minimum_distance =
            radius_a + radius_b + cell_size_ * kCollisionClearanceCells;
        const SkPoint delta = {b.center.fX - a.center.fX,
                               b.center.fY - a.center.fY};
        const float distance = PointLength(delta);
        if (distance >= minimum_distance) {
          continue;
        }

        const SkPoint normal =
            UnitVector(delta, {std::cos(static_cast<float>(first + second)),
                               std::sin(static_cast<float>(first + second))});
        const float separation = (minimum_distance - distance) * 0.5F;
        a.center.fX -= normal.fX * separation;
        a.center.fY -= normal.fY * separation;
        b.center.fX += normal.fX * separation;
        b.center.fY += normal.fY * separation;

        const float closing_speed =
            (a.velocity.fX - b.velocity.fX) * normal.fX +
            (a.velocity.fY - b.velocity.fY) * normal.fY;
        if (closing_speed > 0.0F) {
          a.velocity.fX -= closing_speed * normal.fX;
          a.velocity.fY -= closing_speed * normal.fY;
          b.velocity.fX += closing_speed * normal.fX;
          b.velocity.fY += closing_speed * normal.fY;
        }
        const float deflection = (first + second) % 2U == 0U
                                     ? kCollisionDeflectionRadians
                                     : -kCollisionDeflectionRadians;
        a.velocity = RotateVector(a.velocity, deflection);
        b.velocity = RotateVector(b.velocity, -deflection);
        a.velocity =
            WithLength(a.velocity,
                       cell_size_ * kVortexSpeedCellsPerSecond * a.speed_scale);
        b.velocity =
            WithLength(b.velocity,
                       cell_size_ * kVortexSpeedCellsPerSecond * b.speed_scale);
        AdvanceWaypoint(&a, first);
        AdvanceWaypoint(&b, second);
        KeepInsideCanvas(&a, width, height, radius_a, deflection);
        KeepInsideCanvas(&b, width, height, radius_b, -deflection);
      }
    }
  }
}

void VortexFieldFiddle::Render(double time_seconds) {
  if (!EnsureResources()) {
    return;
  }

  const int width = PixelWidth();
  const int height = PixelHeight();
  SkSurface *surface = webgl_->AcquireSurface(width, height);
  if (surface == nullptr) {
    return;
  }
  if (shapes_.empty() || cached_width_ != width || cached_height_ != height) {
    if (!RebuildGrid(static_cast<float>(width), static_cast<float>(height))) {
      return;
    }
  }

  const float canvas_width = static_cast<float>(width);
  const float canvas_height = static_cast<float>(height);
  UpdateVortices(canvas_width, canvas_height, time_seconds);

  std::array<VortexFrame, kVortexCount> vortex_frames;
  for (std::size_t index = 0; index < kVortexCount; ++index) {
    const VortexMotionState &motion = vortices_[index];
    const float radius = VortexRadius(motion, cell_size_);
    SkPathBuilder builder;
    builder.addCircle(motion.center.fX, motion.center.fY, radius);
    vortex_frames[index] = {
        motion.center,
        radius,
        motion.twist_direction,
        builder.detach(),
    };
  }

  SkCanvas *canvas = surface->getCanvas();
  canvas->clear(kCanvasColor);

  SkPaint shape_paint;
  shape_paint.setAntiAlias(true);
  shape_paint.setStyle(SkPaint::kFill_Style);

  for (const VortexGridShape &shape : shapes_) {
    const SkPath *path_to_draw = &shape.path;
    SkPath deformed;
    std::array<bool, kVortexCount> active_vortices = {};
    bool has_active_vortex = false;
    for (std::size_t index = 0; index < kVortexCount; ++index) {
      const VortexFrame &vortex = vortex_frames[index];
      if (!SkRect::Intersects(shape.path.getBounds(),
                              vortex.path.getBounds())) {
        continue;
      }
      const std::optional<SkPath> intersection =
          Op(shape.path, vortex.path, kIntersect_SkPathOp);
      if (intersection.has_value() && !intersection->isEmpty()) {
        active_vortices[index] = true;
        has_active_vortex = true;
      }
    }
    if (has_active_vortex) {
      deformed = DeformedPath(shape, vortex_frames, active_vortices);
      if (!deformed.isEmpty()) {
        path_to_draw = &deformed;
      }
    }
    shape_paint.setColor4f(shape.color);
    canvas->drawPath(*path_to_draw, shape_paint);
  }

  SkPaint vortex_paint;
  vortex_paint.setAntiAlias(true);
  vortex_paint.setStyle(SkPaint::kStroke_Style);
  vortex_paint.setStrokeWidth(2.0F);
  vortex_paint.setColor(SK_ColorBLACK);
  vortex_paint.setPathEffect(dash_path_effect_);
  for (const VortexFrame &vortex : vortex_frames) {
    canvas->drawPath(vortex.path, vortex_paint);
  }

  const WebGlPresentResult present = webgl_->FlushAndPresent();
  if (!present.success) {
    std::cerr << "[cc-engine/stderr] Vortex field could not submit its WebGL "
                 "frame."
              << std::endl;
  }
}
