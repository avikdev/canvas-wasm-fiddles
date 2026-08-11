#include "geometry/curve_interpolate.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <optional>
#include <utility>

#include "include/core/SkMatrix.h"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkPathMeasure.h"
#include "include/core/SkRect.h"
#include "morphing/geometry_kernel.h"
#include "morphing/path_geometry.h"
#include "morphing/shape_morpher.h"

namespace geometry {
namespace {

using Curve = skmorph::geometry::Curve;
using Cubic = skmorph::geometry::Cubic;

struct ParsedContour {
  std::vector<Curve> curves;
  bool closed = false;
};

bool IsFinitePoint(SkPoint point) {
  return std::isfinite(point.x()) && std::isfinite(point.y());
}

bool ParseSingleContour(const SkPath &path, ParsedContour *result,
                        std::string *error) {
  if (path.isEmpty() || path.isInverseFillType()) {
    *error = "Curve must be a finite, non-empty path.";
    return false;
  }
  result->curves.clear();
  result->closed = false;
  bool saw_move = false;
  bool finished = false;
  SkPoint start = {0.0F, 0.0F};
  SkPoint current = {0.0F, 0.0F};
  SkPath::RawIter iterator(path);
  SkPoint points[4];
  for (SkPath::Verb verb = iterator.next(points); verb != SkPath::kDone_Verb;
       verb = iterator.next(points)) {
    if (verb == SkPath::kMove_Verb) {
      if (saw_move) {
        *error = "Open curve interpolation supports exactly one contour.";
        return false;
      }
      if (!IsFinitePoint(points[0])) {
        *error = "Curve contains a non-finite point.";
        return false;
      }
      saw_move = true;
      start = current = points[0];
      continue;
    }
    if (!saw_move || finished) {
      *error = "Curve contains invalid contour verbs.";
      return false;
    }
    switch (verb) {
    case SkPath::kLine_Verb:
      result->curves.push_back(Curve::Line(points[0], points[1]));
      current = points[1];
      break;
    case SkPath::kQuad_Verb:
      result->curves.push_back(
          Curve::Quadratic(points[0], points[1], points[2]));
      current = points[2];
      break;
    case SkPath::kConic_Verb: {
      const std::optional<Curve> conic =
          Curve::Conic(points[0], points[1], points[2], iterator.conicWeight());
      if (!conic.has_value()) {
        *error = "Curve contains an invalid conic.";
        return false;
      }
      result->curves.push_back(*conic);
      current = points[2];
      break;
    }
    case SkPath::kCubic_Verb:
      result->curves.push_back(
          Curve::CubicBezier(points[0], points[1], points[2], points[3]));
      current = points[3];
      break;
    case SkPath::kClose_Verb:
      if (current != start) {
        result->curves.push_back(Curve::Line(current, start));
      }
      result->closed = true;
      finished = true;
      break;
    case SkPath::kMove_Verb:
    case SkPath::kDone_Verb:
      break;
    }
  }
  result->curves.erase(
      std::remove_if(result->curves.begin(), result->curves.end(),
                     [](const Curve &curve) {
                       return !curve.IsFinite() || curve.IsDegenerate();
                     }),
      result->curves.end());
  if (result->curves.empty()) {
    *error = "Curve has no non-degenerate segments.";
    return false;
  }
  return true;
}

std::vector<float>
CorrespondencePositions(const skmorph::internal::Contour &source,
                        const skmorph::internal::Contour &target) {
  std::vector<float> positions = {0.0F, 1.0F};
  const auto append_spans =
      [&positions](const skmorph::internal::Contour &contour) {
        for (const auto &span : contour.spans()) {
          positions.push_back(span.start_length / contour.length());
          positions.push_back(span.end_length / contour.length());
        }
      };
  append_spans(source);
  append_spans(target);
  std::sort(positions.begin(), positions.end());
  positions.erase(std::unique(positions.begin(), positions.end(),
                              [](float first, float second) {
                                return std::abs(first - second) <= 1e-5F;
                              }),
                  positions.end());
  return positions;
}

SkPoint LerpPoint(SkPoint first, SkPoint second, float amount) {
  return {std::lerp(first.x(), second.x(), amount),
          std::lerp(first.y(), second.y(), amount)};
}

SkRect LerpRect(const SkRect &first, const SkRect &second, float amount) {
  return SkRect::MakeLTRB(std::lerp(first.left(), second.left(), amount),
                          std::lerp(first.top(), second.top(), amount),
                          std::lerp(first.right(), second.right(), amount),
                          std::lerp(first.bottom(), second.bottom(), amount));
}

} // namespace

std::vector<float> GenerateUniformSplitPoints(int split_count,
                                              bool exclude_start,
                                              bool exclude_end) {
  std::vector<float> result;
  if (split_count <= 0) {
    return result;
  }
  const int first = exclude_start ? 1 : 0;
  const int last = exclude_end ? split_count - 1 : split_count;
  if (first > last) {
    return result;
  }
  result.reserve(static_cast<std::size_t>(last - first + 1));
  for (int index = first; index <= last; ++index) {
    result.push_back(static_cast<float>(index) /
                     static_cast<float>(split_count));
  }
  return result;
}

class CurveInterpolate::Impl {
public:
  bool Init(const SkPath &start_curve, const SkPath &end_curve,
            const SkPath &guide_path, WidthProfile profile) {
    initialized_ = false;
    error_.clear();
    if (!profile) {
      error_ = "Width profile is required.";
      return false;
    }

    ParsedContour source;
    ParsedContour target;
    if (!ParseSingleContour(start_curve, &source, &error_) ||
        !ParseSingleContour(end_curve, &target, &error_)) {
      return false;
    }
    if (source.closed != target.closed) {
      error_ = "Start and end curves must both be open or both be closed.";
      return false;
    }

    source_bounds_ = start_curve.computeTightBounds();
    target_bounds_ = end_curve.computeTightBounds();
    if (source_bounds_.isEmpty() || target_bounds_.isEmpty()) {
      error_ = "Start and end curves must have non-empty bounds.";
      return false;
    }

    guide_path_ = guide_path;
    SkPathMeasure guide_measure(guide_path_, false, 0.25F);
    guide_length_ = guide_measure.getLength();
    if (!(guide_length_ > 0.0F) || !std::isfinite(guide_length_)) {
      error_ = "Guide path must have a non-degenerate first contour.";
      return false;
    }
    if (guide_measure.nextContour()) {
      error_ = "Guide path must contain exactly one contour.";
      return false;
    }

    closed_ = source.closed;
    if (closed_) {
      skmorph::MorphOptions options;
      options.startPointMode = skmorph::StartPointMode::kFirstVerb;
      if (!closed_morpher_.Init(start_curve, end_curve, options)) {
        error_ = closed_morpher_.error();
        return false;
      }
    } else {
      skmorph::internal::Contour source_contour(std::move(source.curves));
      skmorph::internal::Contour target_contour(std::move(target.curves));
      if (!(source_contour.length() > 0.0F) ||
          !(target_contour.length() > 0.0F)) {
        error_ = "Open curves must have positive arc length.";
        return false;
      }
      const std::vector<float> positions =
          CorrespondencePositions(source_contour, target_contour);
      open_segments_.clear();
      open_segments_.reserve(positions.size() - 1U);
      for (std::size_t index = 1; index < positions.size(); ++index) {
        open_segments_.push_back(
            {source_contour
                 .CurveBetween(positions[index - 1U], positions[index])
                 .ToCanonicalCubic(),
             target_contour
                 .CurveBetween(positions[index - 1U], positions[index])
                 .ToCanonicalCubic()});
      }
    }

    width_profile_ = std::move(profile);
    initialized_ = true;
    return true;
  }

  SkPath GetCurve(float l) const {
    if (!initialized_) {
      return {};
    }
    l = std::isfinite(l) ? std::clamp(l, 0.0F, 1.0F) : 0.0F;
    const float width_scale = width_profile_(l);
    if (!(width_scale > 0.0F) || !std::isfinite(width_scale)) {
      return {};
    }

    SkPath morphed = closed_ ? closed_morpher_.GetMorphed(l) : MorphOpen(l);
    if (morphed.isEmpty()) {
      return {};
    }
    SkPathMeasure measure(guide_path_, false, 0.25F);
    SkPoint position;
    SkVector tangent;
    if (!measure.getPosTan(l * guide_length_, &position, &tangent) ||
        !tangent.normalize()) {
      return {};
    }

    const SkRect actual = morphed.computeTightBounds();
    const SkRect expected = LerpRect(source_bounds_, target_bounds_, l);
    if (actual.isEmpty() || expected.isEmpty()) {
      return {};
    }
    const float scale_x = expected.width() / actual.width() * width_scale;
    const float scale_y = expected.height() / actual.height();
    if (!std::isfinite(scale_x) || !std::isfinite(scale_y)) {
      return {};
    }

    const float rotation =
        std::atan2(tangent.y(), tangent.x()) + std::numbers::pi_v<float> * 0.5F;
    const float cosine = std::cos(rotation);
    const float sine = std::sin(rotation);
    const float center_x = actual.centerX();
    const float center_y = actual.centerY();
    const SkMatrix placement = SkMatrix::MakeAll(
        cosine * scale_x, -sine * scale_y,
        position.x() - cosine * scale_x * center_x + sine * scale_y * center_y,
        sine * scale_x, cosine * scale_y,
        position.y() - sine * scale_x * center_x - cosine * scale_y * center_y,
        0.0F, 0.0F, 1.0F);
    SkPath result;
    morphed.transform(placement, &result);
    return result;
  }

  SkPath MorphOpen(float amount) const {
    if (open_segments_.empty()) {
      return {};
    }
    SkPathBuilder builder;
    bool first = true;
    for (const auto &[source, target] : open_segments_) {
      Cubic cubic;
      for (std::size_t index = 0; index < cubic.points.size(); ++index) {
        cubic.points[index] =
            LerpPoint(source.points[index], target.points[index], amount);
      }
      if (first) {
        builder.moveTo(cubic.points[0]);
        first = false;
      }
      builder.cubicTo(cubic.points[1], cubic.points[2], cubic.points[3]);
    }
    return builder.detach();
  }

  bool initialized_ = false;
  bool closed_ = false;
  float guide_length_ = 0.0F;
  SkRect source_bounds_ = SkRect::MakeEmpty();
  SkRect target_bounds_ = SkRect::MakeEmpty();
  SkPath guide_path_;
  WidthProfile width_profile_;
  skmorph::ShapeMorpher closed_morpher_;
  std::vector<std::pair<Cubic, Cubic>> open_segments_;
  std::string error_;
};

CurveInterpolate::CurveInterpolate() : impl_(std::make_unique<Impl>()) {}
CurveInterpolate::~CurveInterpolate() = default;
CurveInterpolate::CurveInterpolate(CurveInterpolate &&) noexcept = default;
CurveInterpolate &
CurveInterpolate::operator=(CurveInterpolate &&) noexcept = default;

bool CurveInterpolate::Init(const SkPath &start_curve, const SkPath &end_curve,
                            const SkPath &guide_path,
                            WidthProfile width_profile) {
  return impl_->Init(start_curve, end_curve, guide_path,
                     std::move(width_profile));
}

SkPath CurveInterpolate::GetCurve(float l) const { return impl_->GetCurve(l); }
bool CurveInterpolate::isInitialized() const { return impl_->initialized_; }
float CurveInterpolate::guideLength() const { return impl_->guide_length_; }
const std::string &CurveInterpolate::error() const { return impl_->error_; }

} // namespace geometry
