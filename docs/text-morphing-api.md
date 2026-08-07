# Text Morphing API Proposal

> Feature Request

## Summary

Introduce a high-level Text Morphing API built directly on **Google
Skia** types. The API morphs one `SkPath` into another while preserving
geometric quality using common subdivision and canonical cubic
micro-segments.

------------------------------------------------------------------------

# API Contract

The public API should use native Skia data structures wherever practical
so that applications can adopt the feature without introducing parallel
geometry types.

``` cpp
namespace skmorph {

enum class StartPointMode {
    kAutomatic,
    kFirstVerb,
    kUserSpecified,
};

enum MorphSubdivisionFlags : uint32_t {
    kSplitFlatness       = 1 << 0,
    kSplitTangentAngle   = 1 << 1,
    kSplitCurvature      = 1 << 2,
    kSplitInflection     = 1 << 3,
    kSplitCusps          = 1 << 4,
    kSplitArcLengthError = 1 << 5,
    kSplitTurningAngle   = 1 << 6,
    kSplitMaxArcLength   = 1 << 7,
};

struct MorphOptions {
    // Correspondence alignment.
    float sourceRotationDegrees = 0.0f;
    float targetRotationDegrees = 0.0f;

    StartPointMode startPointMode = StartPointMode::kAutomatic;

    uint32_t subdivisionFlags =
        kSplitFlatness |
        kSplitTangentAngle |
        kSplitCurvature |
        kSplitInflection |
        kSplitCusps |
        kSplitMaxArcLength;

    float flatnessEpsilon = 0.25f;
    float tangentAngleEpsilonDegrees = 8.0f;
    float curvatureEpsilon = 0.02f;
    float arcLengthEpsilon = 0.10f;
    float turningAngleDegrees = 30.0f;
    float maximumSegmentArcLength = 24.0f;
    float holeClearanceEpsilon = 1.0f;
};

class ShapeMorpher {
    bool Init(
        const SkPath& source,
        const SkPath& target,
        const MorphOptions& options);

    // Main api.
    sk_sp<SkPath> GetMorphed(float t) const;
};
```

For efficiency, the engine will be a stateful class, initialized with A and B, then an api `GetMorphed(float t)` to compute the morphed shape.

------------------------------------------------------------------------

# Processing Pipeline

    SkPath A                SkPath B
        │                       │
    Normalize geometry    Normalize geometry
        │                       │
    Rotate by θ₀          Rotate by θ₁
        │                       │
    Choose canonical start points
                │
    Synchronously subdivide
                │
    Canonical cubic conversion
                │
    Piecewise interpolation
                │
    Return SkPath

------------------------------------------------------------------------

# Future Motion Along a Path

The `sourceRotationDegrees` and `targetRotationDegrees` fields are
intentionally part of the API because they will also participate in a
future path-following animation system.

The planned API will resemble:

``` cpp
struct MotionOptions {
    SkPath motionPath;
    bool alignToTangent = true;
    float startRotationDegrees = 0.0f;
    float endRotationDegrees = 0.0f;
};

SkPath MorphAlongPath(
    const SkPath& source,
    const SkPath& target,
    const MorphOptions& morph,
    const MotionOptions& motion);
```

During animation:

-   The shape morphs from `source` to `target`.
-   Its position advances along `motionPath`.
-   When `alignToTangent` is enabled, the local X-axis of the shape
    follows the tangent returned by `SkContourMeasure::getPosTan()`.
-   `startRotationDegrees` and `endRotationDegrees` are composed with
    the tangent frame so the object may face a user-defined direction
    while travelling.

This separation keeps **shape interpolation**, **geometric
correspondence**, and **motion** as independent subsystems that compose
cleanly.
