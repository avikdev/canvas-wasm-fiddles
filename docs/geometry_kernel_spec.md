# Geometry Kernel Specification

## Common Subdivision + Canonical Cubic

> **Draft v0.1**

# Overview

The Geometry Kernel is the mathematical foundation of the morphing
library. It has no knowledge of animation or timelines. Its
responsibility is to represent curves, measure them, subdivide them,
convert between curve types, and expose a uniform API to higher layers.

The central design decision is that **all downstream algorithms operate
on a common representation: small cubic Bézier segments ("canonical
cubics")**.

Instead of implementing separate algorithms for every pair of primitives
such as Arc→Quad, Arc→Cubic, Conic→Line, etc., every primitive is first
subdivided into small pieces and each piece is represented as a cubic.
Once this stage is reached, the remainder of the pipeline works with
only one curve type.

Pipeline:

    Input Primitive
           |
           v
    Detect special points
           |
           v
    Adaptive subdivision
           |
           v
    Canonical cubic conversion
           |
           v
    Arc-length parameterization
           |
           v
    Cubic micro-segments

# Primitive Types

The kernel accepts Line, Quadratic Bézier, Cubic Bézier, Skia Conic and
SVG Elliptic Arc as first-class primitives. SVG shorthand commands are
normalized during parsing.

# Adaptive Subdivision

Subdivision is recursive. A segment is accepted only when every enabled
geometric criterion passes.

Subdivision always begins by inserting exact split locations that are
mathematically significant. These include original verb boundaries,
cusps, inflection points and tangent discontinuities. These points are
never crossed by a micro-segment because they represent natural changes
in the geometry.

After mandatory splits, recursive subdivision begins.

The recursion evaluates a configurable collection of tests:

-   Flatness: maximum control-point distance from the end chord.
-   Tangent angle: angular difference between endpoint tangents.
-   Curvature variation: compare sampled curvature over the interval.
-   Arc-length error: difference between chord, control polygon and
    estimated arc length.
-   Maximum micro-segment arc length, including for perfectly straight
    primitives.
-   Maximum turning angle.
-   Optional application-specific tests.

If any enabled test fails, the segment is split (normally by De
Casteljau at t=0.5, or at an exact feature location when available), and
both children are processed recursively.

The result is a sequence of small segments that are geometrically simple
and nearly uniform.

# Canonical Cubics

Every accepted micro-segment is converted into a cubic Bézier.

Exact conversions: - Line → Cubic - Quadratic → Cubic

Approximate conversions: - Conic → Cubic - SVG Arc → Cubic

This creates a single internal representation for all later algorithms.

# Arc-length Parameterization

Every emitted segment stores both parameter t and normalized arc-length
parameter s.

The kernel builds cumulative arc-length tables so higher-level
algorithms can request points, tangents and split locations using
normalized distance instead of raw Bézier parameter. This greatly
improves stability because different curve types rarely share the same
parameterization.

# Public API

The kernel exposes operations such as:

-   Evaluate(t)
-   Tangent(t)
-   Curvature(t)
-   Split(t)
-   Length()
-   ToCanonicalCubic()
-   BoundingBox()
