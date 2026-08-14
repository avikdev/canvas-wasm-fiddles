#include "fiddles/swirl_deform_fiddle.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
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
// The old straight-line velocity was hypot(0.42, 0.31) cells/second. Motion is
// deliberately reduced to 70% of that speed before per-swirl variation.
constexpr float kSwirlSpeedCellsPerSecond = 0.36541F;
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
constexpr std::size_t kSwirlCount = 4U;
constexpr std::array<float, kSwirlCount> kSwirlDiameterCellRatios = {
    0.50F, 0.80F, 1.00F, 1.40F};
constexpr std::array<float, kSwirlCount> kSwirlSpeedScales = {0.92F, 1.04F,
                                                              0.97F, 1.08F};
constexpr std::array<float, kSwirlCount> kTwistDirections = {1.0F, 1.0F, -1.0F,
                                                             -1.0F};
constexpr std::array<SkScalar, 2> kSwirlDashIntervals = {2.0F, 4.0F};
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

struct SwirlFrame {
  SkPoint center;
  float radius;
  float twist_direction;
  SkPath path;
};

struct SwirlContourSamples {
  std::vector<SkPoint> points;
  bool closed = false;
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

float SwirlRadius(const SwirlMotionState &swirl, float cell_size) {
  return swirl.diameter_cell_ratio * cell_size * 0.5F;
}

SkPoint CoverageTarget(const SwirlMotionState &swirl, float width, float height,
                       float radius) {
  const SkPoint normalized =
      kCoverageWaypoints[static_cast<std::size_t>(swirl.waypoint_index) %
                         kCoverageWaypoints.size()];
  return {
      std::lerp(radius, std::max(radius, width - radius), normalized.fX),
      std::lerp(radius, std::max(radius, height - radius), normalized.fY),
  };
}

void AdvanceWaypoint(SwirlMotionState *swirl, std::size_t swirl_index) {
  constexpr std::array<int, kSwirlCount> kWaypointSteps = {5, 7, 9, 11};
  swirl->waypoint_index =
      (swirl->waypoint_index + kWaypointSteps[swirl_index]) %
      static_cast<int>(kCoverageWaypoints.size());
  swirl->waypoint_age_seconds = 0.0F;
}

void KeepInsideCanvas(SwirlMotionState *swirl, float width, float height,
                      float radius, float deflection) {
  bool hit_horizontal = false;
  bool hit_vertical = false;
  if (swirl->center.fX < radius) {
    swirl->center.fX = radius;
    swirl->velocity.fX = std::abs(swirl->velocity.fX);
    hit_vertical = true;
  } else if (swirl->center.fX > width - radius) {
    swirl->center.fX = width - radius;
    swirl->velocity.fX = -std::abs(swirl->velocity.fX);
    hit_vertical = true;
  }
  if (swirl->center.fY < radius) {
    swirl->center.fY = radius;
    swirl->velocity.fY = std::abs(swirl->velocity.fY);
    hit_horizontal = true;
  } else if (swirl->center.fY > height - radius) {
    swirl->center.fY = height - radius;
    swirl->velocity.fY = -std::abs(swirl->velocity.fY);
    hit_horizontal = true;
  }
  if (hit_horizontal || hit_vertical) {
    swirl->velocity = RotateVector(swirl->velocity, deflection);
    if (swirl->center.fX <= radius) {
      swirl->velocity.fX = std::abs(swirl->velocity.fX);
    } else if (swirl->center.fX >= width - radius) {
      swirl->velocity.fX = -std::abs(swirl->velocity.fX);
    }
    if (swirl->center.fY <= radius) {
      swirl->velocity.fY = std::abs(swirl->velocity.fY);
    } else if (swirl->center.fY >= height - radius) {
      swirl->velocity.fY = -std::abs(swirl->velocity.fY);
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

std::vector<SwirlContourSamples> SampleContours(const SkPath &path) {
  std::vector<SwirlContourSamples> contours;
  SkPathMeasure measure(path, false, 1.0F);
  do {
    const float length = measure.getLength();
    if (!std::isfinite(length) || length <= 0.0F) {
      continue;
    }
    const int sample_count =
        std::max(8, static_cast<int>(std::ceil(length / kCurveSampleSpacing)));
    SwirlContourSamples contour;
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

SkPoint ApplySwirl(const SkPoint &point, const SkPoint &center, float radius,
                   float twist_direction, float maximum_twist_radians) {
  const float delta_x = point.fX - center.fX;
  const float delta_y = point.fY - center.fY;
  const float distance = std::hypot(delta_x, delta_y);
  if (distance >= radius || radius <= 0.0F) {
    return point;
  }
  const float rotation =
      twist_direction * maximum_twist_radians * SmoothFalloff(distance / radius);
  const float cosine = std::cos(rotation);
  const float sine = std::sin(rotation);
  return {
      center.fX + delta_x * cosine - delta_y * sine,
      center.fY + delta_x * sine + delta_y * cosine,
  };
}

SkPath
DeformedIntersection(const SkPath &intersection,
                     const std::array<SwirlFrame, kSwirlCount> &swirl_frames,
                     std::size_t active_swirl, float maximum_twist_radians) {
  SkPathBuilder result;
  result.setFillType(intersection.getFillType());
  for (const SwirlContourSamples &contour : SampleContours(intersection)) {
    std::vector<SkPoint> displaced;
    displaced.reserve(contour.points.size());
    for (const SkPoint &point : contour.points) {
      const SwirlFrame &swirl = swirl_frames[active_swirl];
      const SkPoint displaced_point =
          ApplySwirl(point, swirl.center, swirl.radius, swirl.twist_direction,
                     maximum_twist_radians);
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

SwirlDeformFiddle::SwirlDeformFiddle() : field_seed_(std::random_device{}()) {}

SwirlDeformFiddle::~SwirlDeformFiddle() = default;

std::vector<FiddleWidget> SwirlDeformFiddle::Widgets() const {
  return {{"rotation", "Maximum rotation", "range",
           std::to_string(maximum_rotation_turns_), {}, 0.0, 3.0, 0.1}};
}

bool SwirlDeformFiddle::SetInput(const std::string &name,
                                 const std::string &value) {
  if (name != "rotation") return false;
  char *end = nullptr;
  const float parsed = std::strtof(value.c_str(), &end);
  if (end == value.c_str() || *end != '\0') return false;
  maximum_rotation_turns_ = std::clamp(parsed, 0.0F, 3.0F);
  motion_initialized_ = false;
  last_motion_time_seconds_ = -1.0;
  return true;
}

bool SwirlDeformFiddle::EnsureResources() {
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
  dash_path_effect_ = SkDashPathEffect::Make(kSwirlDashIntervals, 0.0F);
  if (dash_path_effect_ == nullptr) {
    return false;
  }
  webgl_ = std::move(webgl);
  std::cout << "[cc-engine/stdout] Swirl deform Skia and Path Ops resources "
               "ready."
            << std::endl;
  return true;
}

bool SwirlDeformFiddle::RebuildGrid(float width, float height) {
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

  std::vector<SwirlGridShape> rebuilt;
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
      if (path.isEmpty()) {
        continue;
      }
      const float hue = 360.0F * static_cast<float>(hue_distribution(random)) /
                        static_cast<float>(kHueCount);
      rebuilt.push_back({
          std::move(path),
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

void SwirlDeformFiddle::InitializeSwirls(float width, float height,
                                         double time_seconds) {
  constexpr std::array<SkPoint, kSwirlCount> kInitialPositions = {{
      {0.15F, 0.18F},
      {0.72F, 0.16F},
      {0.22F, 0.74F},
      {0.76F, 0.72F},
  }};
  constexpr std::array<float, kSwirlCount> kInitialAngles = {0.18F, 2.42F,
                                                             5.05F, 3.72F};

  for (std::size_t index = 0; index < kSwirlCount; ++index) {
    SwirlMotionState &swirl = swirls_[index];
    swirl.diameter_cell_ratio = kSwirlDiameterCellRatios[index];
    swirl.speed_scale = kSwirlSpeedScales[index];
    swirl.twist_direction = kTwistDirections[index];
    swirl.wander_phase = 0.73F + static_cast<float>(index) * 1.61F;
    swirl.waypoint_index =
        static_cast<int>((index * 4U + 2U) % kCoverageWaypoints.size());
    swirl.waypoint_age_seconds = 0.0F;
    const float radius = SwirlRadius(swirl, cell_size_);
    swirl.center = {
        std::clamp(kInitialPositions[index].fX * width, radius, width - radius),
        std::clamp(kInitialPositions[index].fY * height, radius,
                   height - radius),
    };
    const float speed =
        cell_size_ * kSwirlSpeedCellsPerSecond * swirl.speed_scale;
    swirl.velocity = {
        std::cos(kInitialAngles[index]) * speed,
        std::sin(kInitialAngles[index]) * speed,
    };
  }
  last_motion_time_seconds_ = time_seconds;
  motion_initialized_ = true;
}

void SwirlDeformFiddle::UpdateSwirls(float width, float height,
                                     double time_seconds) {
  if (!motion_initialized_) {
    InitializeSwirls(width, height, time_seconds);
    return;
  }
  const float delta_seconds = static_cast<float>(
      std::clamp(time_seconds - last_motion_time_seconds_, 0.0, 0.1));
  last_motion_time_seconds_ = time_seconds;
  if (delta_seconds <= 0.0F) {
    return;
  }

  for (std::size_t index = 0; index < kSwirlCount; ++index) {
    SwirlMotionState &swirl = swirls_[index];
    const float radius = SwirlRadius(swirl, cell_size_);
    const SkPoint target = CoverageTarget(swirl, width, height, radius);
    const SkPoint to_target = {target.fX - swirl.center.fX,
                               target.fY - swirl.center.fY};
    swirl.waypoint_age_seconds += delta_seconds;
    if (PointLength(to_target) < std::max(cell_size_ * 0.18F, radius * 0.25F) ||
        swirl.waypoint_age_seconds >= kWaypointTimeoutSeconds) {
      AdvanceWaypoint(&swirl, index);
    }

    const SkPoint desired_direction = UnitVector(to_target);
    const SkPoint current_direction = UnitVector(swirl.velocity);
    const float steering =
        std::clamp(kCoverageSteeringPerSecond * delta_seconds, 0.0F, 1.0F);
    SkPoint direction = UnitVector(
        {std::lerp(current_direction.fX, desired_direction.fX, steering),
         std::lerp(current_direction.fY, desired_direction.fY, steering)},
        current_direction);
    const float time = static_cast<float>(time_seconds);
    const float wander_rate =
        std::sin(time * 0.79F + swirl.wander_phase) *
            kPrimaryWanderRadiansPerSecond +
        std::sin(time * 1.83F + swirl.wander_phase * 1.71F) *
            kSecondaryWanderRadiansPerSecond;
    direction = RotateVector(direction, wander_rate * delta_seconds);
    const float speed =
        cell_size_ * kSwirlSpeedCellsPerSecond * swirl.speed_scale;
    swirl.velocity = {direction.fX * speed, direction.fY * speed};
    swirl.center.fX += swirl.velocity.fX * delta_seconds;
    swirl.center.fY += swirl.velocity.fY * delta_seconds;

    const float wall_sign = index % 2U == 0U ? 1.0F : -1.0F;
    const float wall_variation =
        0.72F + 0.28F * std::sin(time * 0.47F + swirl.wander_phase * 0.83F);
    KeepInsideCanvas(&swirl, width, height, radius,
                     wall_sign * kWallDeflectionRadians * wall_variation);
  }

  // Two solver passes are enough for four bodies and also handle a body being
  // separated into a third one during the first pass.
  for (int pass = 0; pass < 2; ++pass) {
    for (std::size_t first = 0; first < kSwirlCount; ++first) {
      for (std::size_t second = first + 1U; second < kSwirlCount; ++second) {
        SwirlMotionState &a = swirls_[first];
        SwirlMotionState &b = swirls_[second];
        const float radius_a = SwirlRadius(a, cell_size_);
        const float radius_b = SwirlRadius(b, cell_size_);
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
        a.velocity = WithLength(
            a.velocity, cell_size_ * kSwirlSpeedCellsPerSecond * a.speed_scale);
        b.velocity = WithLength(
            b.velocity, cell_size_ * kSwirlSpeedCellsPerSecond * b.speed_scale);
        AdvanceWaypoint(&a, first);
        AdvanceWaypoint(&b, second);
        KeepInsideCanvas(&a, width, height, radius_a, deflection);
        KeepInsideCanvas(&b, width, height, radius_b, -deflection);
      }
    }
  }
}

void SwirlDeformFiddle::Render(double time_seconds) {
  if (!EnsureResources()) {
    return;
  }

  const int width = PixelWidth();
  const int height = PixelHeight();
  if (!UpdateState(time_seconds, width, height)) {
    return;
  }
  SkSurface *surface = webgl_->AcquireSurface(width, height);
  if (surface == nullptr) {
    return;
  }
  DrawFrame(surface->getCanvas(), width, height);

  const WebGlPresentResult present = webgl_->FlushAndPresent();
  if (!present.success) {
    std::cerr << "[cc-engine/stderr] Swirl deform could not submit its WebGL "
                 "frame."
              << std::endl;
  }
}

bool SwirlDeformFiddle::UpdateState(double time_seconds, int width,
                                    int height) {
  time_seconds_ = time_seconds;
  if (shapes_.empty() || cached_width_ != width || cached_height_ != height) {
    if (!RebuildGrid(static_cast<float>(width), static_cast<float>(height))) {
      return false;
    }
  }

  const float canvas_width = static_cast<float>(width);
  const float canvas_height = static_cast<float>(height);
  UpdateSwirls(canvas_width, canvas_height, time_seconds);
  return true;
}

void SwirlDeformFiddle::DrawFrame(SkCanvas *canvas, int, int) {
  std::array<SwirlFrame, kSwirlCount> swirl_frames;
  for (std::size_t index = 0; index < kSwirlCount; ++index) {
    const SwirlMotionState &motion = swirls_[index];
    const float radius = SwirlRadius(motion, cell_size_);
    SkPathBuilder builder;
    builder.addCircle(motion.center.fX, motion.center.fY, radius);
    swirl_frames[index] = {
        motion.center,
        radius,
        motion.twist_direction,
        builder.detach(),
    };
  }

  canvas->clear(kCanvasColor);

  SkPaint shape_paint;
  shape_paint.setAntiAlias(true);
  shape_paint.setStyle(SkPaint::kFill_Style);

  for (const SwirlGridShape &shape : shapes_) {
    SkPath remaining = shape.path;
    SkPathBuilder composed;
    composed.setFillType(shape.path.getFillType());
    bool split_shape = false;
    for (std::size_t index = 0; index < kSwirlCount; ++index) {
      const SwirlFrame &swirl = swirl_frames[index];
      if (!SkRect::Intersects(remaining.getBounds(), swirl.path.getBounds())) {
        continue;
      }
      const std::optional<SkPath> intersection =
          Op(remaining, swirl.path, kIntersect_SkPathOp);
      const std::optional<SkPath> outside =
          Op(remaining, swirl.path, kDifference_SkPathOp);
      if (!intersection.has_value() || intersection->isEmpty() ||
          !outside.has_value()) {
        continue;
      }

      const SkPath deformed =
          DeformedIntersection(
              *intersection, swirl_frames, index,
              maximum_rotation_turns_ * 2.0F * std::numbers::pi_v<float>);
      composed.addPath(deformed.isEmpty() ? *intersection : deformed);
      remaining = *outside;
      split_shape = true;
      if (remaining.isEmpty()) {
        break;
      }
    }
    if (split_shape) {
      composed.addPath(remaining);
    }
    const SkPath deformed_shape = composed.detach();
    const SkPath &path_to_draw =
        split_shape && !deformed_shape.isEmpty() ? deformed_shape : shape.path;
    shape_paint.setColor4f(shape.color);
    canvas->drawPath(path_to_draw, shape_paint);
  }

  SkPaint swirl_paint;
  swirl_paint.setAntiAlias(true);
  swirl_paint.setStyle(SkPaint::kStroke_Style);
  swirl_paint.setStrokeWidth(2.0F);
  swirl_paint.setColor(SK_ColorBLACK);
  swirl_paint.setPathEffect(dash_path_effect_);
  for (const SwirlFrame &swirl : swirl_frames) {
    canvas->drawPath(swirl.path, swirl_paint);
  }
}
