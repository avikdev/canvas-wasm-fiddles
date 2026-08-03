#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

#include "include/core/SkPoint.h"
#include "include/core/SkRect.h"

namespace skmorph::geometry {

// A cubic Bézier in the representation consumed by the morphing layer.
struct Cubic {
  std::array<SkPoint, 4> points{};
};

// Maps between a curve's native parameter and normalized arc length. Tables
// are immutable value objects and always contain the endpoints when valid.
class ArcLengthTable {
public:
  struct Sample {
    float parameter = 0.0F;
    float cumulative_length = 0.0F;
  };

  float length() const;
  bool empty() const;
  float ParameterAtLength(float distance) const;
  float ParameterAtNormalizedLength(float normalized_length) const;
  float NormalizedLengthAtParameter(float parameter) const;
  const std::vector<Sample> &samples() const;

private:
  friend class Curve;
  std::vector<Sample> samples_;
};

// A first-class line, quadratic, cubic, rational Skia conic, or SVG
// endpoint-arc primitive. Curve is deliberately independent from SkPath so
// parsing and shape correspondence remain separate concerns.
class Curve {
public:
  enum class Type {
    kLine,
    kQuadratic,
    kCubic,
    kConic,
    kSvgArc,
  };

  static Curve Line(SkPoint start, SkPoint end);
  static Curve Quadratic(SkPoint start, SkPoint control, SkPoint end);
  static Curve CubicBezier(SkPoint start, SkPoint control1, SkPoint control2,
                           SkPoint end);
  static std::optional<Curve> Conic(SkPoint start, SkPoint control, SkPoint end,
                                    float weight);

  // Constructs an SVG elliptic arc from endpoint-form parameters. Radii are
  // made positive and enlarged as required by the SVG implementation notes.
  // Coincident endpoints and zero radii do not define an arc and return null.
  static std::optional<Curve> SvgArc(SkPoint start, SkPoint end, float radius_x,
                                     float radius_y,
                                     float x_axis_rotation_degrees,
                                     bool large_arc, bool sweep);

  Type type() const;
  SkPoint start() const;
  SkPoint end() const;
  bool IsFinite() const;
  bool IsDegenerate(float epsilon = 1e-6F) const;

  SkPoint Evaluate(float parameter) const;
  SkVector Derivative(float parameter) const;
  SkVector SecondDerivative(float parameter) const;
  SkVector Tangent(float parameter) const;
  float Curvature(float parameter) const;

  std::pair<Curve, Curve> Split(float parameter) const;
  Curve Subcurve(float start_parameter, float end_parameter) const;
  Curve Reversed() const;
  Cubic ToCanonicalCubic() const;
  SkRect BoundingBox() const;

  ArcLengthTable BuildArcLengthTable(float tolerance = 0.01F,
                                     int max_depth = 14) const;
  float Length(float tolerance = 0.01F, int max_depth = 14) const;

  // Returns exact cubic inflections and robust stationary-point candidates in
  // the open interval (0, 1). Callers choose which feature classes they need.
  std::vector<float> InflectionParameters() const;
  std::vector<float> CuspParameters(float derivative_epsilon = 1e-4F) const;

  float Flatness() const;
  float EndpointTangentAngleDegrees() const;
  float CurvatureVariation() const;
  float ArcLengthError(float tolerance = 0.01F) const;
  float ControlPolygonTurningAngleDegrees() const;

private:
  struct ArcData {
    SkPoint center = {0.0F, 0.0F};
    float radius_x = 0.0F;
    float radius_y = 0.0F;
    float rotation_radians = 0.0F;
    float start_angle = 0.0F;
    float sweep_angle = 0.0F;
  };

  explicit Curve(Type type);

  Type type_ = Type::kLine;
  std::array<SkPoint, 4> points_{};
  float conic_weight_ = 1.0F;
  ArcData arc_{};
};

} // namespace skmorph::geometry
