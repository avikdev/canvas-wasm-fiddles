#include "morphing/shape_morpher.h"

#include <cmath>
#include <iostream>
#include <limits>

#include "include/core/SkPathBuilder.h"

namespace {

bool Expect(bool condition, const char *message) {
  if (!condition) {
    std::cerr << message << '\n';
  }
  return condition;
}

bool Near(float actual, float expected, float tolerance = 0.03F) {
  return std::abs(actual - expected) <= tolerance;
}

SkPath Rectangle(float left, float top, float right, float bottom,
                 bool clockwise = true) {
  SkPathBuilder builder;
  if (clockwise) {
    builder.moveTo(left, top)
        .lineTo(right, top)
        .lineTo(right, bottom)
        .lineTo(left, bottom);
  } else {
    builder.moveTo(left, top)
        .lineTo(left, bottom)
        .lineTo(right, bottom)
        .lineTo(right, top);
  }
  builder.close();
  return builder.detach();
}

SkPath RoundedDiamond() {
  SkPathBuilder builder;
  builder.moveTo(0.0F, -2.0F)
      .cubicTo(1.1F, -2.0F, 2.0F, -1.1F, 2.0F, 0.0F)
      .cubicTo(2.0F, 1.1F, 1.1F, 2.0F, 0.0F, 2.0F)
      .cubicTo(-1.1F, 2.0F, -2.0F, 1.1F, -2.0F, 0.0F)
      .cubicTo(-2.0F, -1.1F, -1.1F, -2.0F, 0.0F, -2.0F)
      .close();
  return builder.detach();
}

SkPath RectangleWithHole() {
  SkPathBuilder builder;
  builder.addPath(Rectangle(-3.0F, -3.0F, 3.0F, 3.0F));
  builder.addPath(Rectangle(-1.0F, -1.0F, 1.0F, 1.0F, false));
  return builder.detach();
}

} // namespace

int main() {
  bool success = true;

  skmorph::ShapeMorpher morpher;
  success &= Expect(
      morpher.Init(Rectangle(-2.0F, -1.0F, 2.0F, 1.0F), RoundedDiamond()),
      "A closed rectangle should morph to a closed cubic shape.");
  success &= Expect(morpher.isInitialized(),
                    "Successful initialization should set state.");
  success &= Expect(morpher.contourCount() == 1U,
                    "Two simple paths should prepare one contour.");
  success &= Expect(morpher.segmentCount() >= 4U,
                    "Original verb boundaries should be synchronized.");
  success &= Expect(!morpher.GetMorphed(0.5F).isEmpty(),
                    "A prepared midpoint should be non-empty.");
  const std::vector<skmorph::ContourStartPoints> midpoint_starts =
      morpher.GetStartPoints(0.5F);
  success &=
      Expect(midpoint_starts.size() == 1U,
             "Start-point diagnostics should include the outer contour.");
  success &= Expect(morpher.GetMorphed(-1.0F) == morpher.GetMorphed(0.0F),
                    "Morph t should clamp below zero.");
  success &= Expect(morpher.GetMorphed(2.0F) == morpher.GetMorphed(1.0F),
                    "Morph t should clamp above one.");
  success &=
      Expect(morpher.GetMorphed(std::numeric_limits<float>::quiet_NaN()) ==
                 morpher.GetMorphed(0.0F),
             "Non-finite morph t should be deterministic.");

  skmorph::ShapeMorpher holes;
  success &= Expect(
      holes.Init(RectangleWithHole(), Rectangle(-3.0F, -3.0F, 3.0F, 3.0F)),
      "An unmatched source hole should collapse instead of rejecting.");
  success &= Expect(holes.contourCount() == 2U,
                    "A disappearing hole remains a prepared contour.");
  success &=
      Expect(holes.GetStartPoints(0.0F).size() == 2U &&
                 holes.GetStartPoints(1.0F).size() == 1U,
             "Start-point diagnostics should hide a collapsed unmatched hole.");

  skmorph::ShapeMorpher growing_hole;
  success &= Expect(growing_hole.Init(Rectangle(-3.0F, -3.0F, 3.0F, 3.0F),
                                      RectangleWithHole()),
                    "An unmatched target hole should grow from its center.");
  success &= Expect(growing_hole.contourCount() == 2U,
                    "A growing hole remains a prepared contour.");

  skmorph::ShapeMorpher orientation;
  success &=
      Expect(orientation.Init(Rectangle(-2.0F, -1.0F, 2.0F, 1.0F),
                              Rectangle(-2.0F, -1.0F, 2.0F, 1.0F, false)),
             "Opposite contour orientations should be corrected.");

  skmorph::MorphOptions rotated_options;
  rotated_options.sourceRotationDegrees = 90.0F;
  skmorph::ShapeMorpher rotated;
  success &=
      Expect(rotated.Init(Rectangle(-2.0F, -1.0F, 2.0F, 1.0F),
                          Rectangle(-2.0F, -1.0F, 2.0F, 1.0F), rotated_options),
             "Finite correspondence rotations should initialize.");
  const SkRect rotated_bounds = rotated.GetMorphed(0.0F).getBounds();
  success &= Expect(
      Near(rotated_bounds.width(), 2.0F) && Near(rotated_bounds.height(), 4.0F),
      "Source endpoint should include its requested correspondence rotation.");

  SkPathBuilder open_builder;
  open_builder.moveTo(0.0F, 0.0F).lineTo(1.0F, 0.0F).lineTo(0.0F, 1.0F);
  skmorph::ShapeMorpher invalid;
  success &= Expect(!invalid.Init(open_builder.detach(), RoundedDiamond()),
                    "Open contours should be rejected with no implicit close.");
  success &= Expect(!invalid.error().empty(),
                    "Rejected initialization should report a diagnostic.");

  skmorph::MorphOptions invalid_options;
  invalid_options.flatnessEpsilon = std::numeric_limits<float>::quiet_NaN();
  success &= Expect(!invalid.Init(Rectangle(0.0F, 0.0F, 2.0F, 1.0F),
                                  RoundedDiamond(), invalid_options),
                    "Non-finite tolerances should be rejected.");

  skmorph::MorphOptions bounded_options;
  bounded_options.flatnessEpsilon = 0.0F;
  bounded_options.tangentAngleEpsilonDegrees = 0.0F;
  bounded_options.maximumSubdivisionDepth = 24;
  bounded_options.maximumSegmentsPerContour = 8;
  skmorph::ShapeMorpher bounded;
  success &=
      Expect(bounded.Init(Rectangle(-2.0F, -1.0F, 2.0F, 1.0F), RoundedDiamond(),
                          bounded_options),
             "Aggressive criteria should still terminate at the segment cap.");
  success &= Expect(bounded.segmentCount() <= 8U,
                    "The per-contour segment safety cap must be enforced.");

  skmorph::MorphOptions length_options;
  length_options.subdivisionFlags = skmorph::kSplitMaxArcLength;
  length_options.maximumSegmentArcLength = 10.0F;
  skmorph::ShapeMorpher length_bounded;
  success &= Expect(length_bounded.Init(Rectangle(0.0F, 0.0F, 100.0F, 20.0F),
                                        Rectangle(0.0F, 0.0F, 100.0F, 20.0F),
                                        length_options),
                    "Maximum arc length should accept long straight contours.");
  success &= Expect(
      length_bounded.segmentCount() >= 20U,
      "Long line verbs should split even when every shape criterion is flat.");

  skmorph::MorphOptions revision_options;
  revision_options.enableFeaturePivotCorrespondence = true;
  skmorph::ShapeMorpher revised;
  success &= Expect(
      revised.Init(Rectangle(-2.0F, -1.0F, 2.0F, 1.0F), RoundedDiamond(),
                   revision_options),
      "The flag-gated feature-pivot correspondence path should initialize.");
  success &= Expect(
      !revised.GetMorphed(0.5F).isEmpty() &&
          revised.segmentCount() <=
              static_cast<size_t>(revision_options.maximumSegmentsPerContour),
      "The revised path should produce bounded cubic output.");
  success &=
      Expect(Near(revised.GetMorphed(0.0F).getBounds().width(), 4.0F) &&
                 Near(revised.GetMorphed(1.0F).getBounds().height(), 4.0F),
             "Feature correspondence must preserve both endpoint geometries.");

  skmorph::MorphOptions first_verb_options;
  first_verb_options.startPointMode = skmorph::StartPointMode::kFirstVerb;
  skmorph::ShapeMorpher first_verb;
  success &= Expect(first_verb.Init(Rectangle(-2.0F, -1.0F, 2.0F, 1.0F),
                                    RoundedDiamond(), first_verb_options),
                    "First-verb start diagnostics should initialize.");
  const std::vector<skmorph::ContourStartPoints> first_verb_starts =
      first_verb.GetStartPoints(0.5F);
  success &=
      Expect(first_verb_starts.size() == 1U &&
                 !first_verb_starts.front().shifted.has_value(),
             "An unchanged first-verb start should omit the shifted marker.");

  skmorph::MorphOptions invalid_length_options;
  invalid_length_options.maximumSegmentArcLength = 0.0F;
  success &=
      Expect(!invalid.Init(Rectangle(0.0F, 0.0F, 2.0F, 1.0F), RoundedDiamond(),
                           invalid_length_options),
             "An enabled maximum-length criterion requires a positive length.");

  return success ? 0 : 1;
}
