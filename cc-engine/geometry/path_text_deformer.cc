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

bool PathTextDeformer::SmoothedCornerTangent(
    float distance, const PathTextDeformOptions &options,
    SkVector *tangent) const {
  if (tangent == nullptr || options.corner_transition_length <= kEpsilon)
    return false;

  constexpr int kSearchSegments = 16;
  const float transition = options.corner_transition_length;
  const float half_transition = transition * 0.5F;
  const float threshold = options.corner_angle_threshold_degrees *
                          std::numbers::pi_v<float> / 180.0F;
  float strongest_turn = 0.0F;
  float corner_left = distance - half_transition;
  float corner_right = corner_left;
  SkPoint unused_position;
  SkVector previous_tangent;
  if (!RawFrame(corner_left, &unused_position, &previous_tangent))
    return false;

  SkVector incoming = previous_tangent;
  SkVector outgoing = previous_tangent;
  for (int sample = 1; sample <= kSearchSegments; ++sample) {
    const float sample_distance = distance - half_transition +
                                  transition * static_cast<float>(sample) /
                                      static_cast<float>(kSearchSegments);
    SkVector sample_tangent;
    if (!RawFrame(sample_distance, &unused_position, &sample_tangent))
      return false;
    const float turn = std::atan2(Cross(previous_tangent, sample_tangent),
                                  Dot(previous_tangent, sample_tangent));
    if (std::abs(turn) > std::abs(strongest_turn)) {
      strongest_turn = turn;
      corner_left =
          sample_distance - transition / static_cast<float>(kSearchSegments);
      corner_right = sample_distance;
      incoming = previous_tangent;
      outgoing = sample_tangent;
    }
    previous_tangent = sample_tangent;
  }
  if (std::abs(strongest_turn) < threshold)
    return false;

  // Refine the location of the tangent discontinuity so the transition does
  // not drift with the search sampling grid.
  for (int iteration = 0; iteration < 10; ++iteration) {
    const float middle = (corner_left + corner_right) * 0.5F;
    SkVector middle_tangent;
    if (!RawFrame(middle, &unused_position, &middle_tangent))
      break;
    const float turn_from_incoming = std::abs(std::atan2(
        Cross(incoming, middle_tangent), Dot(incoming, middle_tangent)));
    if (turn_from_incoming >= std::abs(strongest_turn) * 0.5F) {
      corner_right = middle;
      outgoing = middle_tangent;
    } else {
      corner_left = middle;
      incoming = middle_tangent;
    }
  }

  const float corner = (corner_left + corner_right) * 0.5F;
  const float progress = std::clamp(
      (distance - (corner - half_transition)) / transition, 0.0F, 1.0F);
  const float eased = progress * progress * (3.0F - 2.0F * progress);
  const float incoming_angle = std::atan2(incoming.fY, incoming.fX);
  const float corner_turn =
      std::atan2(Cross(incoming, outgoing), Dot(incoming, outgoing));
  const float angle = incoming_angle + corner_turn * eased;
  *tangent = {std::cos(angle), std::sin(angle)};
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
  if (options.protect_sharp_turns &&
      SmoothedCornerTangent(distance, options, &frame.tangent)) {
    // The spatial transition rotates continuously while the baseline position
    // stays on the original path.
  } else if (options.protect_sharp_turns && std::abs(turn) >= threshold) {
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
