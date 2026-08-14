# Adaptive text layout on a curve

Text on a curve is not ordinary text with one rotation applied to each glyph.
The guide is a nonlinear coordinate system: horizontal glyph coordinates
become arc length along a guide, while vertical glyph coordinates become
distance along the guide's local normal. Preserving readable outlines near
tight bends and discontinuous corners requires more than dense sampling.

This document describes the reusable shaping and deformation pipeline used by
the Text On Curve fiddle. The UI, colors, animation speed, and default heart
are presentation details and are intentionally not part of the technique.

## Coordinate model

Let the guide be parameterized by arc length as `P(s)`. Its unit tangent and
left normal are `T(s)` and `N(s)`. A point `(x, y)` in a shaped glyph maps to:

```text
s = lineOffset + x
mapped(x, y) = P(s) + y * N(s)
```

The shaped paths are first baseline-relative. The implementation reads the
font's x-height and moves the alignment axis halfway from the baseline to the
mean line. After that translation, glyph paths use `y = 0` for this optical
center line. Negative y values lie above it in Skia's screen coordinate system.
Arc-length placement is important: using a raw Bézier parameter would visibly
bunch letters where a segment's parameter advances faster than its geometric
distance.

The font ascent and descent are measured from the same axis. The larger of the
two distances becomes a symmetric half-height, giving an equal-sized metric
box above and below the guide. This keeps the background band centered even
though normal font metrics are asymmetric around the baseline.

## 1. Shape the text before bending it

The input is normalized to one line first. Newlines, tabs, and repeated spaces
collapse to one ordinary space, with leading and trailing whitespace removed.

SkParagraph and HarfBuzz then shape the entire line. Shaping the line—not each
Unicode character independently—preserves kerning, combining marks, fallback
fonts, script reordering, and glyph substitutions. The result records:

- the vector outline selected for every drawable glyph;
- each glyph's positioned x origin on the shaped line;
- glyph and complete-line advances;
- all outlines translated so their baseline-to-mean-line midpoint is `y = 0`;
- a symmetric vertical half-height derived from ascent, descent, and x-height.

Whitespace need not have an outline; its advance still contributes to later
glyph positions and to the repeated line width.

## 2. Measure the guide

The SVG path is decoded into an `SkPath`, so lines, quadratics, cubics, conics
produced by SVG arcs, and sharp joins share one representation. `SkPathMeasure`
provides positions and tangents in arc-length coordinates.

At distance `s`, local signed curvature is estimated from tangents on either
side of a small probe interval:

```text
turn = atan2(cross(T(s-e), T(s+e)), dot(T(s-e), T(s+e)))
curvature = turn / (2e)
```

This estimate works on smooth curves and deliberately becomes large near a
sharp `C0` join.

## 3. Adaptively subdivide every outline edge

A straight line in glyph space generally becomes curved after the nonlinear
mapping, so lines must be subdivided along with Bézier glyph edges. Each glyph
contour is first divided into bounded-length intervals. An interval is then
recursively split at its midpoint until:

- its mapped midpoint is sufficiently close to the mapped endpoint chord;
- curvature times its horizontal glyph span is small;
- or the configured minimum segment length/depth limit is reached.

In pseudocode:

```text
subdivide(a, b):
    m = midpoint on source contour between a and b
    A, M, B = map(a), map(m), map(b)
    bendError = distance(M, line(A, B))
    turnError = abs(curvatureAt(m.x) * (b.x - a.x))

    if shortEnough and bendError <= tolerance and turnError <= threshold:
        emit B
    else:
        subdivide(a, m)
        subdivide(m, b)
```

Flat sections stay inexpensive. Tight bends and sharp transitions receive the
extra vertices needed to describe their mapped geometry.

## 4. Why subdivision alone fails at inner bends

For local curvature magnitude `|k|`, the radius of curvature is `R = 1/|k|`.
On the inside of a bend, a glyph point whose normal distance approaches `R`
collapses toward the curvature center. Beyond it, the outline inverts and can
self-intersect. More subdivision merely draws that inversion more accurately.

The mapper therefore caps only points on the inside of a turn. With safety
fraction `q`, the uncapped region ends at `qR`. Beyond that, an asymptotic tanh
cap approaches the curvature center without crossing it:

```text
limit = q * R
softness = (1 - q) * R

if curvature * y > 0 and abs(y) > limit:
    yEffective = sign(y) *
        (limit + softness * tanh((abs(y) - limit) / softness))
else:
    yEffective = y
```

The safety fraction is a useful artistic control. A lower value protects
topology more aggressively but compresses inner glyph edges sooner.

## 5. Sharp corners and transition fans

At a sharp join there are two legitimate tangents: incoming and outgoing.
Switching instantly between their normals produces a seam, while intersecting
raw offset lines can pinch broad glyph tops into an illegible point.

When the tangent deflection exceeds a threshold (30 degrees by default), the
implementation uses the normalized tangent bisector. Adjacent adaptively
subdivided points see overlapping probe windows, producing a compact radial
transition fan rather than a one-sample normal discontinuity. The same
inversion cap limits the fan's inner edge.

This is a geometric safeguard, not a full font-structure solver. A more
elaborate system can classify stems, crossbars, and diagonal features, attach a
rigidity weight to each, and constrain a crossbar's mapped length to roughly
80–120% of its original length. That extension fits before point mapping:

```text
outline -> feature tags -> adaptive subdivision -> curvature/corner checks
        -> inversion cap -> optional structural constraints -> mapped path
```

The current reusable implementation focuses on the two topology-critical
parts: adaptive geometry and bounded inner offsets.

| Guide condition | Subdivision | Mapping behavior | Expected result |
| --- | --- | --- | --- |
| Straight or nearly flat | Sparse | Ordinary tangent/normal frame | Optically centered glyph |
| Moderate curvature | Medium | Ordinary mapping, monitored radius | Smooth bend |
| High curvature | Dense | Asymptotic inner-distance cap | Bounded taper without inversion |
| Sharp `C0` corner | Densest near join | Tangent-bisector transition fan plus cap | Crisp turn without a collapsed inner edge |

## 6. Repetition and motion

A normalized phrase plus a trailing space is shaped once. Copies are placed at
successive line advances until the guide is covered. Animation subtracts a
continuously increasing arc-length offset from every copy. Glyphs whose right
edge has passed distance zero are discarded, while later copies maintain a
continuous stream.

Because movement, repetition, curvature sampling, and glyph placement all use
the same arc-length coordinate, speed remains uniform across lines, Béziers,
conics, and corners.

## Practical limits

- The first guide contour is the text alignment axis; additional contours are not
  concatenated because their tangent continuity is undefined.
- Extremely small guides or very large fonts can still overlap neighboring
  glyphs even when individual outlines remain non-inverted.
- Adaptive output is polygonal. Lower flatness tolerance produces smoother
  edges at a higher vertex and Path Ops cost.
- Font fallback can split a shaped line into runs. The runs first share a
  baseline, then use their combined metrics to establish one alignment axis
  before deformation.
