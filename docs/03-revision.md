# Spec Revision: Correspondence Solver with Feature Pivots

> Proposed revision to the existing shape morphing pipeline.

# Background

The current implementation establishes correspondence primarily by
synchronized adaptive subdivision followed by normalized arc-length
matching. While this works well for many shapes, some morphs still
produce undesirable intermediate geometry because pivot points migrate
to semantically unrelated regions of the target contour.

Examples include:

-   outer contour self-intersections,
-   crossing micro-segments,
-   inward features stretched across the shape,
-   holes approaching or crossing the outer contour.

This proposal **does not replace** the current subdivision pipeline.
Instead it adds a dedicated **Correspondence Solver** between
subdivision and interpolation.

------------------------------------------------------------------------

# Revised Pipeline

    Adaptive subdivision
              │
    Canonical cubic conversion
              │
    Feature extraction
              │
    Correspondence Solver   <-- NEW
              │
    Piecewise interpolation
              │
    Constraint solver
              │
    Output contour

------------------------------------------------------------------------

# Design Goals

The correspondence solver should minimize long-distance pivot migration
while preserving the visual role of each boundary feature.

Instead of matching pivots using only normalized arc length, the solver
uses a multi-criteria cost function together with a set of immutable
feature pivots.

------------------------------------------------------------------------

# Feature Pivots

Not every pivot is equally important.

After adaptive subdivision, classify pivots into two categories.

## Ordinary pivots

Generated purely by subdivision.

These may move freely during correspondence optimization.

## Feature pivots

Feature pivots describe visually meaningful locations and should be
preserved as far as possible.

Typical examples include:

-   original path verb boundaries,
-   sharp corners,
-   curvature extrema,
-   inflection points,
-   cusp locations,
-   tangent discontinuities,
-   endpoints of long straight runs,
-   extrema in X or Y,
-   user-specified anchors (future).

Feature pivots become hard or high-weight constraints during matching.

------------------------------------------------------------------------

# Correspondence Solver

Rather than pairing pivots solely by normalized arc length, compute a
matching cost for every feasible pair.

Suggested cost:

    Cost =
        wArc       * Δ(normalized arc length)
      + wAngle     * Δ(radial angle)
      + wTangent   * Δ(endpoint tangent)
      + wCurvature * Δ(local curvature)
      + wFeature   * featurePenalty

where `featurePenalty` is zero for matching compatible feature pivots
and large for incompatible matches.

The solver seeks a monotonic correspondence that minimizes the total
cost while preserving contour order.

Dynamic programming or shortest-path optimization are natural candidates
because they enforce monotonic mappings efficiently.

------------------------------------------------------------------------

# Radial Angle Constraint

For every pivot compute a radial angle about a chosen contour center.

The angle is **not** the primary parameter. Instead it serves as an
additional constraint that discourages pivots from migrating to the
opposite side of the shape.

This greatly reduces long crossing micro-segments while avoiding the
limitations of a purely radial parameterization.

------------------------------------------------------------------------

# Long Straight Segments

Introduce a mandatory subdivision rule:

    pieceArcLength <= maxPieceArcLength

even when all other subdivision criteria pass.

This increases the available degrees of freedom on glyphs containing
long straight edges (for example I, H, L and many logos) and improves
the quality of both correspondence optimization and later interpolation.

------------------------------------------------------------------------

# Expected Benefits

Compared with the current implementation, this revision aims to provide:

-   fewer long-distance pivot migrations,
-   better preservation of semantic features,
-   reduced contour self-intersections,
-   fewer crossing micro-segments,
-   improved morphs for concave glyphs,
-   more stable handling of logos and iconography,
-   better foundation for later topology-preserving constraints.

------------------------------------------------------------------------

# Future Extensions

The correspondence solver can later incorporate additional constraints
without changing the subdivision engine, including:

-   local thickness preservation,
-   hole-aware correspondence,
-   symmetry detection,
-   skeleton / medial-axis guidance,
-   user-pinned correspondence points,
-   machine-learned feature importance.

This keeps subdivision, correspondence, interpolation and topology
correction as independent stages of the overall morphing pipeline.
