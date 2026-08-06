# Topology-preserving control-lattice deformation

This document describes the vector deformation system used by the
`mesh-deform-vec` fiddle. The reusable implementation lives in
`cc-engine/graphics/mesh_deformer.{h,cc}`; artwork generation, panel layout,
synthetic gestures, and presentation policy remain in the fiddle.

Revision 3 deliberately changes the mathematical model. Earlier revisions used
a screened Laplacian. That produced smooth results, but a harmonic field has
global support: a constraint on one side is theoretically visible everywhere.
Even tiny far-side changes can look like the whole canvas is shaking. The
current implementation instead behaves like a raster editor's forward-warp
brush applied to a persistent, dense control lattice:

- the brush force has compact spatial support;
- only final lattice control points are committed;
- exterior control points have infinite mass and never move;
- neighbor order and triangle orientation cannot invert;
- vector paths are subdivided at the same density scale as the lattice before
  being mapped.

The result remains fully vector. No source shape is rasterized.

## Persistent control lattice

For bounds of width `W`, height `H`, and configured `epsilon_gap = ε`, the
regular lattice dimensions are

```text
columns = floor(W / ε)
rows    = floor(H / ε)
```

with a minimum of two cells per axis. Build fails when the bounds cannot fit
two `ε`-sized cells. The actual original gaps are

```text
Δx = W / columns
Δy = H / rows
```

so `Δx ≥ ε` and `Δy ≥ ε`. This is the density cap: asking for a very large
canvas does not silently create control points closer than `epsilonGap`.

Each rectangular cell is divided into two triangles. Alternating the diagonal
from cell to cell avoids a persistent upper-left/lower-right bias.

Each vertex has:

```text
oᵢ = immutable original position
cᵢ = accumulated committed displacement
qᵢ = preview displacement for the active pointer session
pᵢ = oᵢ + cᵢ + qᵢ
```

`UpdateDrag` replaces `q`; it does not accumulate pointer-event frequency.
`CommitDrag` performs `c ← c + q` exactly once on pointer-up. `CancelDrag`
discards `q`. Therefore a gesture is transactional and its result depends on
the current pointer coordinate, not on how many intermediate animation frames
happened to run.

At `BeginDrag`, the current positions are copied into a drag-reference lattice.
Desired brush calculations for that gesture use this snapshot. Accepted
topology-limited previews additionally use the immediately preceding valid
preview as their lower bound. After commit, the new final positions become the
starting lattice for the next gesture. Repeated actions therefore edit final
control points rather than re-deforming the original source independently.

## Infinite-mass boundary

Every vertex in the first or last row or column is a hard boundary vertex.
Candidate generation skips these vertices, and the topology validator also
requires their positions to equal their immutable originals exactly.

This is stronger than giving an edge a large spring constant. It has truly
infinite mass in the discrete model:

```text
qᵢ = 0 and cᵢ = 0 for every boundary vertex
```

No iteration tolerance can make the frame drift.

## Compact swept-brush force

Let:

```text
s = pointer-down position
m = current pointer position
d = effective drag vector from s toward m
R = brush radius
```

Long-drag damping from Revision 2 remains available. If raw length is `L`,
`D = damping_start_distance`, and `ρ = far_drag_response`, the effective
length is

```text
       L                    when L ≤ D or D = 0
L' = {
       D + ρ(L - D)         when L > D
```

The effective vector `d` has length `L'`. An optional
`maximum_drag_distance` can impose a final hard cap; zero disables it.

For a control point `p`, project it onto the finite segment from `s` to
`s + d`:

```text
τ = clamp(dot(p - s, d) / dot(d, d), 0, 1)
k = s + τd
r² = ||p - k||²
```

`k` is the closest point on the swept centerline. The compact radial kernel is

```text
       (1 - r²/R²)²       when r < R
w(r) = {
       0                  otherwise
```

Outside the swept capsule, displacement is exactly zero—not merely small.
This is why artwork on the opposite side no longer shakes.

The brush uses a longitudinal response

```text
ℓ(τ) = 1 - 0.84τ
```

so material near pointer-down receives the most force while material close to
the advancing pointer receives 16%. The candidate displacement is

```text
vᵢ = push_strength · w(rᵢ) · ℓ(τᵢ) · d
```

This gradient creates the characteristic forward-warp behavior: trailing
material travels farther while the leading region moves less and becomes
denser. It replaces the previous separate, infinite-tail compression lobe.

The finite capsule is important. A globally solved membrane can be smooth but
cannot guarantee that a far region is motionless. The compact brush provides a
hard locality guarantee.

## Preventing control-point crossing

A strong candidate can ask a point to pass its neighbor or flip a triangle.
Clamping `x` and `y` independently is insufficient: a heavily sheared cell can
keep row and column order while its diagonal triangle still inverts.

Revision 3 validates both structured-grid order and triangle orientation.

For every horizontal pair:

```text
p(row, column + 1).x - p(row, column).x > 0
```

For every vertical pair:

```text
p(row + 1, column).y - p(row, column).y > 0
```

A tiny numerical margin proportional to `epsilonGap` replaces mathematical
strict inequality in floating-point arithmetic.

For every oriented triangle `(a,b,c)`, the signed doubled area is

```text
A₂ = cross(b - a, c - a)
```

and must satisfy

```text
A₂ ≥ Δx · Δy · minimum_cell_area_ratio
```

Since every original triangle has positive orientation, retaining positive
area prevents triangle inversion and is stronger than only checking immediate
row/column order.

### Selective topology clamping

If the full local candidate is valid, it is used unchanged. Otherwise,
processing begins at the last valid preview and each affected interior vertex
attempts to advance to its desired position. A local validator checks that
vertex's four axis neighbors and every incident triangle. When the full move is
invalid, twenty bisection steps find its furthest locally valid position.

This creates density instead of crossing while avoiding two artifacts of a
single global scale:

- a compressed cell no longer suppresses the entire brush field, so other
  vertices—and nearby perpendicular gestures—can still move;
- a continuing drag begins protection from its last displayed preview, so a
  saturated control point remains at its maximum valid state instead of
  bouncing toward the pointer-down lattice on the next frame.

Each accepted single-vertex move preserves the invariant established by all
previous moves. The resulting pass is therefore globally valid even though the
clamping decision is local.

## Mapping vector artwork

The lattice is a deformation field, not visible geometry.

`MapPoint` locates an original point's regular cell in constant time, chooses
the cell's alternating triangle, calculates barycentric weights, and applies
those weights to the triangle's current control points. The mapping is
piecewise affine and respects the validated triangle topology.

Mapping only a path's original endpoints or Bézier controls would miss small,
local lattice pockets. A long source line with two endpoints could cross many
deformed cells while remaining visually straight. A large cubic could have all
four controls outside a pocket even though the curve passes through it.

`DeformPath` therefore subdivides every source verb:

1. Estimate its length from its control polygon.
2. Generate enough parameter-space samples that their nominal spacing is no
   larger than `epsilonGap`.
3. Map every sample through the lattice.
4. Reconstruct the mapped span as cubic Bézier segments using Catmull–Rom
   tangents.
5. Preserve original verb boundaries, intentional corners, contour closure,
   and fill type.

Lines, quadratics, conics, cubics, and implicit closing lines all follow this
process. The grid and vector sampling density are controlled by the same
`epsilonGap`, so artwork cannot undersample a deformation pocket that the
lattice was dense enough to represent.

The final renderer still calls `SkCanvas::drawPath`. There is no texture
stretching, pixel resampling, or fixed raster resolution. Curves remain
anti-aliased, vector-sharp, and exportable as SVG.

## Holes and compound shapes

Triangle preservation keeps the mesh map locally orientation preserving, but
compound path presentation remains conservative under severe accumulated
warps.

The fiddle deforms an outer contour and its hole separately, densely samples
the candidate hole with `SkPathMeasure`, and requires every sample to remain
inside the outer contour. If validation fails, it scales the hole toward its
mapped anchor and retries. A valid hole is subtracted using Skia Path Ops. If
no safe candidate is found, the hole is omitted for that frame.

Thus a hole may shrink or temporarily disappear, but never crosses the outer
contour.

## Pointer overlay and scene lifetime

The synthetic pointer generator supplies complete drag transactions. It first
selects a gesture family using configurable weights, then samples pointer-down
and pointer-up as a related pair:

- inward: edge to a point in the central region;
- outward: central region toward the nearest horizontal or vertical exterior;
- center: one central-region point to another;
- sideways: a predominantly horizontal or vertical central gesture with a
  small transverse component.

The default weights are 30% inward, 30% outward, 25% center-to-center, and 15%
sideways. This avoids accidentally producing an inward vector after selecting
an outward gesture. Endpoints remain inside the usable inset bounds, and the
final segment is capped without changing its direction.

The configured gesture length is capped at 50% of the longer artwork
dimension. The same cap is installed in `MeshDeformer`, so both the visible
arrow endpoint and the deformation target obey it.

The output panel draws a four-pixel black arrow only while the current
transaction is active. A six-pixel white under-stroke creates a two-pixel-total
halo around the black stroke, and both passes use rounded caps and joins. On the
endpoint frame:

1. the deformation is committed;
2. the completed-drag count increments;
3. the active pointer frame is cleared;
4. the arrow disappears immediately.

No historical arrows are stored.

After ten completed gestures, pointer generation pauses for one second. During
that appreciation interval the fully deformed artwork remains unchanged and
the control-grid overlay is hidden. A reusable progress chip in the reserved
top legend band is visible for the entire scene lifetime, not only during the
hold. The cycle has eleven progress units: ten complete drag transactions and
one one-second hold. Within a drag, pointer travel supplies fractional
progress; between drags, the bar rests at the completed-transaction boundary.
During the hold, wall-clock time drains the final unit. Its fill is anchored on
the right, so the left edge recedes from one reset to the next. At zero, the
next frame atomically creates newly seeded source artwork, rebuilds the
undeformed lattice, clears the pointer session, and resets the indicator.

While gestures are running, the output panel overlays the current horizontal
and vertical control lattice using 0.5-pixel black strokes at 20% opacity. The
grid comes directly from the deformed control points, disappears immediately
after gesture ten, and reappears with the fresh lattice after reset.

## Complexity

Let `V` be lattice vertices, `T` be triangles, and `S` be source-path samples.

- Lattice build: `O(V + T)`.
- Ordinary drag preview: `O(V)`.
- Preview requiring selective topology clamping: `O(20V)` local checks, each
  touching at most four axis neighbors and the vertex's incident triangles.
- Point mapping: `O(1)`.
- Vector deformation and drawing: `O(S)`.
- Persistent memory: `O(V + T)`.

Unlike the former Jacobi solve, there is no iteration count controlling visual
reach and no global harmonic update.

## Core API example

```cpp
graphics::MeshDeformerOptions options;
options.epsilon_gap = 4.0F;
options.push_strength = 1.0F;
options.maximum_drag_distance =
    std::max(vector_width, vector_height) * 0.50F;
options.damping_start_distance =
    options.maximum_drag_distance * 0.45F;
options.far_drag_response = 0.12F;
options.minimum_cell_area_ratio = 0.000001F;

graphics::MeshDeformer lattice;
lattice.Build(SkRect::MakeWH(vector_width, vector_height), options);

lattice.BeginDrag(pointer_down, brush_radius);
lattice.UpdateDrag(pointer_move, 0.45F);
SkPath preview = lattice.DeformPath(source_path);
canvas->drawPath(preview, paint);

// Pointer-up:
lattice.CommitDrag();

// Pointer-cancel instead:
// lattice.CancelDrag();
```

`epsilonGap` is expressed in the lattice bounds' coordinate system. Clients
rendering in physical pixels should scale it consistently with their device
scale, as the fiddle does.

The current fiddle uses `4 × device_scale` pixels, half the previous spacing,
for a denser control lattice. The topology threshold is one thousandth of its
earlier value (`10⁻⁶` instead of `10⁻³`), allowing later gestures to compress
cells much further before selective topology clamping intervenes.
The damping knee is expressed as 45% of the unchanged maximum drag distance.
Finally, the fiddle passes a per-gesture intensity of `0.45` to `UpdateDrag`.
That local input multiplier deliberately leaves topology headroom for later
gestures without changing the reusable deformer's `push_strength`, hard drag
cap, or brush radius.

## Fiddle option defaults

The important experimental controls are gathered in
`MeshDeformFiddleOptions`, rather than scattered as anonymous constants. The
defaults currently used by the demo are:

```text
epsilon_gap                         4 logical px
push_strength                      1.0
drag_intensity                     0.45
maximum_drag_distance_ratio        0.50 of the longer dimension
damping_start_distance_ratio       0.45 of the hard drag cap
far_drag_response                  0.12
minimum_cell_area_ratio            0.000001
brush_radius_ratio                 0.25 of the shorter dimension
pointer_step                       5 logical px/frame
edge_inset_ratio                   0.12
inward/outward/center/side weights 0.30 / 0.30 / 0.25 / 0.15
center_region_ratio                0.45
minimum_drag_length_ratio          0.45
idle_frames_between_drags          10
drag_count_before_reset            10
reset_delay_seconds                1.0
```

The fiddle copies these settings into `MeshDeformerOptions` and
`FakeMouseActionsOptions` when a scene is built. This is the intended boundary
for future sliders, chips, or presets: user-facing controls can replace the
struct values and rebuild without changing deformation internals.

## Source artwork and color separation

The source remains a collection of crisp Skia paths. Each nominal artwork cell
selects a random hue through the shared HSL color utility, uses saturation in a
narrow range, and varies lightness more visibly. Candidate hues are checked
against the cell immediately above and to the left. Random retries are followed
by an exhaustive fallback search, so every horizontal and vertical neighbor
pair is separated by at least 52 degrees around the hue circle.

The artwork vocabulary includes polygons, stars, crosses, organic blobs,
outlined oblongs, shapes with protected holes, hollow circles, text outlines,
and `HatchLineRect`. Text strings such as `Ah`, `Hi`, `@`, and individual
letters are converted from the shared typeface directly to paths before
deformation. A hatch rectangle is represented as several parallel filled
rectangles with uniform strip thickness and spacing, then rotated as one path.
Because both are genuine filled vector geometry, local subdivision and mesh
mapping deform their contours rather than raster pixels or stroke metadata.

## Tuning deformation reach and edge smoothness

To increase how far an area can deform:

- Raise `maximum_drag_distance`; it is only the final hard ceiling.
- Raise `damping_start_distance` so one-to-one response continues farther.
- Raise `far_drag_response` toward `1.0` so the portion beyond the damping knee
  loses less motion.
- Raise `push_strength` to move points inside the swept brush more strongly.
- Increase the `BeginDrag` brush radius to involve a broader lattice region.
- Lower `minimum_cell_area_ratio` cautiously. This permits denser compression
  before topology protection shortens the drag, but reduces the safety margin
  against nearly collapsed triangles.

For smoother deformed edges:

- Reduce `epsilonGap`. It simultaneously creates a denser lattice and shortens
  the uniform source-path sampling interval. Cost grows roughly with the
  inverse square for the lattice.
- Keep the path reconstruction cubic; mapping only original Bézier controls
  will expose cell-boundary corners.
- Selective subdivision is feasible as a future replacement for purely uniform
  sampling: recursively map each span's midpoint and subdivide when its
  distance from the mapped endpoint chord exceeds a device-space flatness
  tolerance. A depth cap and a minimum parameter interval prevent runaway
  subdivision. That concentrates samples around high-curvature deformation
  pockets while leaving straight, nearly affine regions inexpensive.

## Deliberate limitations

This is not a continuum-mechanics simulation. It does not conserve volume,
estimate pressure, or preserve local rigidity. Its goals are directness,
locality, fixed edges, repeatable transactional edits, and topology-safe vector
mapping.

Selective clamping is deterministic but order-dependent: when two adjacent
vertices both request extreme motion, the vertex processed first may consume
more of their shared geometric headroom. A nonlinear barrier solve could
distribute that limit more symmetrically, but would add substantial complexity
and could make pointer response depend on solver convergence.
