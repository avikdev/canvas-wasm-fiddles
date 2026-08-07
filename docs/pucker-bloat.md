# Split-segment pucker and bloat

The reusable implementation lives in
`cc-engine/geometry/pucker_bloat.{h,cc}`. It operates entirely on `SkPath`
geometry and returns another vector path; it does not rasterize, sample pixels,
or depend on the fiddle renderer.

## Public model

The operation accepts a source path, a normalized `amount` in `[-1, 1]`, and a
pivot:

- `GeometricCenter` uses the path's signed-area centroid. Lines participate
  exactly and curves are sampled into short chords for the area integration.
  This puts a regular triangle's pivot at its centroid rather than at the
  center of its bounding box. Degenerate or zero-area geometry falls back to
  the bounds center.
- `CustomPoint` uses the caller-provided point.

`displacement_scale` multiplies the normalized amount before geometry is
changed. Its core-library default is `1`, preserving the full mathematical
range. The fiddle uses `0.5`, so its slider can still display `[-1, 1]` while
the maximum geometric displacement is capped to half of the original
presentation.

The current sign convention follows the revised split-segment algorithm:

- `amount = +1` is full bloat. Original segment anchors collapse to the pivot,
  while newly introduced segment midpoints move to twice their original radial
  distance.
- `amount = -1` is full pucker. Original anchors move to twice their radial
  distance, while the new midpoints collapse to the pivot.
- `amount = 0` returns the input path exactly.

For an arbitrary value `v`, a source anchor `A`, split midpoint `M`, and pivot
`P`, the two radial transforms are:

```text
A' = P + (A - P) · (1 - v)
M' = P + (M - P) · (1 + v)
```

Thus both families use relative distance from the pivot. No canvas-size
constant is embedded in the geometry library.

## Segment normalization

Every drawable source segment becomes one cubic:

- A line `A → B` receives controls at one-third and two-thirds of the segment.
- A quadratic is converted with the standard two-thirds control formula.
- A rational conic is approximated by a cubic whose controls make the cubic
  pass through the conic's midpoint. For a quarter-circle conic this produces
  the familiar `0.5522848…` cubic-circle handle ratio.
- A cubic is retained directly.
- A contour's implicit closing edge is explicitly normalized and deformed
  before `close()` is emitted.

The builder preserves the source fill type and contour closure.

## Midpoint selection

By default, each normalized cubic is split at parameter `t = 0.5` using
De Casteljau subdivision. Given cubic controls
`A, H1, H2, B`, subdivision constructs:

```text
Q0 = lerp(A,  H1, t)
Q1 = lerp(H1, H2, t)
Q2 = lerp(H2, B,  t)
R0 = lerp(Q0, Q1, t)
R1 = lerp(Q1, Q2, t)
M  = lerp(R0, R1, t)
```

The two exact cubic halves are then:

```text
A, Q0, R0, M
M, R1, Q2, B
```

`split_by_arc_length` switches midpoint selection to an approximate half-arc
length parameter. The implementation samples between 8 and 256 chords
(`arc_length_sample_count`, default 32), accumulates their lengths, finds the
half-length interval, and interpolates the corresponding parameter. De
Casteljau still performs the actual split, so both halves reproduce the
normalized source cubic exactly before deformation.

## Control reconstruction

Name the split controls:

```text
A, C1, C2, M
M, C3, C4, B
```

The inner controls translate with the displaced midpoint and scale by the
midpoint's change in radial distance. Define:

```text
r = |P - M'| / |P - M|

C2' = M' + (C2 - M) · r
C3' = M' + (C3 - M) · r
```

When `M` originally coincides with `P`, the ratio is undefined and the
implementation safely uses `r = 1`. Both inner offsets otherwise receive the
same non-negative scale, so `C2'–M'–C3'` remains collinear and preserves
first-derivative direction continuity at the split. During bloat, moving `M`
farther from `P` lengthens both handles proportionally instead of merely
translating them. During pucker, the same rule shortens them as `M` approaches
the pivot.

The outer controls retain their original handle lengths while reorienting
toward their old control positions from the displaced anchors:

```text
|A' - C1'| = |A - C1|
direction(C1' - A') = direction(C1 - A')

|B' - C4'| = |B - C4|
direction(C4' - B') = direction(C4 - B')
```

If either requested direction degenerates, the original anchor-to-control
vector is used as a fallback. Coincident handles remain coincident, avoiding
division by zero.

The output for every original segment is consequently two cubics:

```text
A'  → C1' → C2' → M'
M'  → C3' → C4' → B'
```

Adjacent original segments transform their shared anchor with the same radial
formula, so the path remains connected.

## Pluggable algorithm boundary

`PuckerBloatAlgorithm` is the strategy interface. It receives the already
resolved pivot, clamped amount, source path, and common options, and returns a
`PuckerBloatResult`. `PuckerBloatDetailed` selects
`SplitSegmentPuckerBloatAlgorithm()` by default, but callers can pass another
strategy without changing parameters or presentation code.

`PuckerBloatResult` contains:

- the deformed path;
- the resolved pivot;
- displaced original anchors;
- displaced split midpoints.

The last two arrays are diagnostics rather than hidden renderer state. They let
the fiddle draw solid pivot-to-anchor guides, dashed pivot-to-midpoint guides,
and endpoint crosshairs from the exact geometry used by the algorithm.

`PuckerBloat` is the simpler path-only wrapper. Empty paths, zero-area bounds,
non-finite amounts or pivots, and zero amount safely return the source path.

## Fiddle presentation

The fiddle presents six source shapes in three rows:

1. original, with no deformation or pivot animation;
2. geometric-center pivot;
3. synchronized custom pivots orbiting on a diagonally rotated ellipse.

Every cell uses a shared grey text chip for its row legend. The deformed paths
use the exact blue fill `#2196F3`. The geometric-centroid row is labelled
`center based`. Center-based and custom cells show the resolved pivot as an eye
on a crosshair, solid 30%-opacity anchor guides, dashed midpoint guides, and
small endpoint crosshairs.

The sixth source is a rounded rectangle. Its two tangent anchors at every
corner delimit the rounded span, which follows the same conic-to-cubic
normalization and midpoint deformation as the other paths.

Layout values are responsive in logical canvas units. Narrow or short canvases
reduce outer and cell padding, header height, text and chip padding, slider
height, track thickness, guide sizes, and border strokes. Shapes use 28% of
their available square radius; even at the fiddle's maximum half-strength
deformation, their split midpoints occupy most of the cell without being
clipped.

The amount animation follows:

```text
0 → +1 → 0 → -1 → 0
```

with eased movement, two-second pauses at `-1` and `+1`, and shorter neutral
pauses. A `Pucker` or `Bloat` sign label sits to the left of the reusable
`TwoSidedSlider` (both are shown at exact zero). The black slider pill fills red
from zero toward `-1` and blue from zero toward `+1`; its range labels are
white.
