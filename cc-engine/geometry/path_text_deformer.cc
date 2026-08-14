#include "geometry/path_text_deformer.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <numbers>

#include "include/core/SkPathBuilder.h"
#include "include/core/SkPathMeasure.h"

namespace geometry {
namespace {

constexpr float kEpsilon = 1e-4F;

float Cross(SkVector first, SkVector second) {
  return first.fX * second.fY - first.fY * second.fX;
}

float Dot(SkVector first, SkVector second) {
  return first.fX * second.fX + first.fY * second.fY;
}

SkVector UnitOr(SkVector vector, SkVector fallback) {
  if (!vector.normalize())
    return fallback;
  return vector;
}

float PointLineDistance(SkPoint point, SkPoint start, SkPoint end) {
  const SkVector span = end - start;
  const float length = std::hypot(span.fX, span.fY);
  if (length <= kEpsilon) {
    const SkVector delta = point - start;
    return std::hypot(delta.fX, delta.fY);
  }
  return std::abs(Cross(point - start, span)) / length;
}

} // namespace

PathTextDeformer::PathTextDeformer(const SkPath &guide) : guide_(guide) {
  measure_ = std::make_unique<SkPathMeasure>(guide_, false, 0.25F);
  length_ = measure_->getLength();
  closed_ = measure_->isClosed();
}

PathTextDeformer::~PathTextDeformer() = default;

bool PathTextDeformer::valid() const {
  return measure_ != nullptr && std::isfinite(length_) && length_ > kEpsilon;
}

float PathTextDeformer::length() const { return length_; }

float PathTextDeformer::NormalizeDistance(float distance) const {
  if (!valid())
    return 0.0F;
  if (closed_) {
    distance = std::fmod(distance, length_);
    if (distance < 0.0F)
      distance += length_;
    return distance;
  }
  return std::clamp(distance, 0.0F, std::max(0.0F, length_ - kEpsilon));
}

bool PathTextDeformer::RawFrame(float distance, SkPoint *position,
                                SkVector *tangent) const {
  if (!valid())
    return false;
  distance = NormalizeDistance(distance);
  if (!measure_->getPosTan(distance, position, tangent))
    return false;
  *tangent = UnitOr(*tangent, {1.0F, 0.0F});
  return true;
}

PathFrame
PathTextDeformer::FrameAt(float distance,
                          const PathTextDeformOptions &options) const {
  PathFrame frame;
  if (!RawFrame(distance, &frame.position, &frame.tangent))
    return frame;
  const float probe =
      std::clamp(options.curvature_probe, std::max(kEpsilon, length_ * 1e-5F),
                 std::max(kEpsilon, length_ * 0.1F));
  SkPoint previous_position;
  SkPoint next_position;
  SkVector previous_tangent;
  SkVector next_tangent;
  if (!RawFrame(distance - probe, &previous_position, &previous_tangent) ||
      !RawFrame(distance + probe, &next_position, &next_tangent)) {
    frame.normal = {-frame.tangent.fY, frame.tangent.fX};
    return frame;
  }
  const float turn = std::atan2(Cross(previous_tangent, next_tangent),
                                Dot(previous_tangent, next_tangent));
  frame.turn_angle = turn;
  frame.signed_curvature = turn / (2.0F * probe);
  const float threshold = options.corner_angle_threshold_degrees *
                          std::numbers::pi_v<float> / 180.0F;
  if (options.protect_sharp_turns && std::abs(turn) >= threshold) {
    // Bisect the incoming/outgoing tangents. Across neighboring samples this
    // creates the transition fan needed at a C0 join instead of switching the
    // glyph normal discontinuously at one point.
    frame.tangent = UnitOr(previous_tangent + next_tangent, frame.tangent);
  }
  frame.normal = {-frame.tangent.fY, frame.tangent.fX};
  return frame;
}

SkPoint PathTextDeformer::MapPoint(SkPoint glyph_point, float horizontal_offset,
                                   const PathTextDeformOptions &options) const {
  const PathFrame frame = FrameAt(horizontal_offset + glyph_point.fX, options);
  float normal_distance = glyph_point.fY;
  if (options.protect_sharp_turns &&
      frame.signed_curvature * normal_distance > 0.0F) {
    const float curvature = std::abs(frame.signed_curvature);
    if (curvature > kEpsilon) {
      const float radius = 1.0F / curvature;
      const float safety = std::clamp(options.inversion_safety, 0.50F, 0.98F);
      const float limit = radius * safety;
      const float magnitude = std::abs(normal_distance);
      if (magnitude > limit) {
        const float softness = std::max(radius * (1.0F - safety), kEpsilon);
        const float capped =
            limit + softness * std::tanh((magnitude - limit) / softness);
        normal_distance = std::copysign(capped, normal_distance);
      }
    }
  }
  return {frame.position.fX + frame.normal.fX * normal_distance,
          frame.position.fY + frame.normal.fY * normal_distance};
}

SkPath PathTextDeformer::Deform(const SkPath &glyph_outline,
                                float horizontal_offset,
                                const PathTextDeformOptions &options) const {
  if (!valid() || glyph_outline.isEmpty())
    return {};
  const float minimum = std::max(0.05F, options.minimum_segment_length);
  const float maximum = std::max(minimum, options.maximum_segment_length);
  const float tolerance = std::max(0.01F, options.flatness_tolerance);
  SkPathBuilder output;
  output.setFillType(glyph_outline.getFillType());
  SkPathMeasure source(glyph_outline, false, 0.25F);
  do {
    const float contour_length = source.getLength();
    if (contour_length <= kEpsilon)
      continue;
    SkPoint first_source;
    if (!source.getPosTan(0.0F, &first_source, nullptr))
      continue;
    const SkPoint first = MapPoint(first_source, horizontal_offset, options);
    output.moveTo(first);

    const auto append_adaptive = [&](auto &&self, float start_distance,
                                     SkPoint start_source, SkPoint start,
                                     float end_distance, SkPoint end_source,
                                     SkPoint end, int depth) -> void {
      const float span = end_distance - start_distance;
      const float middle_distance = (start_distance + end_distance) * 0.5F;
      SkPoint middle_source;
      if (!source.getPosTan(middle_distance, &middle_source, nullptr)) {
        output.lineTo(end);
        return;
      }
      const SkPoint middle =
          MapPoint(middle_source, horizontal_offset, options);
      const PathFrame frame =
          FrameAt(horizontal_offset + middle_source.fX, options);
      const bool sufficiently_flat =
          PointLineDistance(middle, start, end) <= tolerance &&
          std::abs(frame.signed_curvature) *
                  std::abs(end_source.fX - start_source.fX) <=
              0.12F;
      if (depth >= 12 || span <= minimum ||
          (span <= maximum && sufficiently_flat)) {
        output.lineTo(end);
        return;
      }
      self(self, start_distance, start_source, start, middle_distance,
           middle_source, middle, depth + 1);
      self(self, middle_distance, middle_source, middle, end_distance,
           end_source, end, depth + 1);
    };

    const int initial_segments =
        std::max(1, static_cast<int>(std::ceil(contour_length / maximum)));
    float start_distance = 0.0F;
    SkPoint start_source = first_source;
    SkPoint start = first;
    for (int segment = 1; segment <= initial_segments; ++segment) {
      const float end_distance = contour_length * static_cast<float>(segment) /
                                 static_cast<float>(initial_segments);
      SkPoint end_source;
      if (!source.getPosTan(end_distance, &end_source, nullptr))
        continue;
      const SkPoint end = MapPoint(end_source, horizontal_offset, options);
      append_adaptive(append_adaptive, start_distance, start_source, start,
                      end_distance, end_source, end, 0);
      start_distance = end_distance;
      start_source = end_source;
      start = end;
    }
    if (source.isClosed())
      output.close();
  } while (source.nextContour());
  return output.detach();
}

} // namespace geometry
