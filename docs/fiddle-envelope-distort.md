# Envelope Distort

## Purpose

Envelope Distort demonstrates a vector-only free-form deformation (FFD)
pipeline. Skia Paragraph shapes each word, the resulting glyph outlines are
converted to dense polygonal contours, and a bicubic Bézier surface maps those
contours into an animated envelope. The text is not rendered to an image, so
glyph counters and sharp outline detail remain ordinary Skia path geometry.

The fiddle key is `env-distort`.

## Architecture

The implementation is split into three layers:

1. `geometry::BicubicBezierPatch` is a reusable, renderer-independent geometry
   primitive. It owns a row-major 4 × 4 lattice, evaluates the tensor-product
   cubic Bernstein basis, and exposes first derivatives in both parameter
   directions.
2. `noise::Perlin2D` and `noise::FractalPerlin2D` provide deterministic,
   seedable gradient noise. They have no drawing dependencies and can be reused
   by other procedural animations.
3. `EnvelopeDistortFiddle` owns paragraph shaping, outline subdivision,
   normalized text data, animated envelope construction, orientation
   preservation, cell fitting, and drawing.

The geometry and noise libraries each have a native unit test. The fiddle is a
WebAssembly-only target because paragraph shaping and the Skia Ganesh surface
are tied to the browser demo runtime.

## Bicubic patch

The patch stores control points `P[row * 4 + column]`; the column is the
`u` direction and the row is the `v` direction. A position on the patch is:

```text
                 3   3
S(u, v) =       sum sum B_i(u) B_j(v) P[j, i]
                i=0 j=0
```

where the cubic Bernstein weights are:

```text
B0(t) = (1 - t)^3
B1(t) = 3t(1 - t)^2
B2(t) = 3t^2(1 - t)
B3(t) = t^3
```

The public evaluator clamps finite parameters to `[0, 1]`. Non-finite
parameters use zero, and a lattice containing a non-finite point evaluates to
the zero point. This makes callers fail safely rather than feeding NaNs into a
Skia path. The partial-derivative APIs use the analytic derivatives of the
Bernstein weights and are available for future tangent, normal, or orientation
work.

The class follows the tensor-product formulation described in the
[Visualization Library Bézier surface guide](https://visualizationlibrary.org/docs/2.1/html/pag_guide_bezier_surfaces.html).

An envelope may contain one patch or several consecutive patches. A normalized
`u` value selects a span and is remapped to that patch's local `[0, 1]` domain.
Adjacent spans are built from the same endpoint positions and derivatives, so
their edges and tangents meet continuously. This piecewise construction is
required for shapes such as a sinusoidal band with two to four complete cycles:
a single cubic polynomial cannot represent that many extrema.

## Text preparation

Text preparation happens once, after the shared default font manager becomes
available:

1. A Skia Paragraph is built for every combination of the seven words and the
   shared font-cycle choices: default/Roboto fallback, IBM Plex Mono, and
   Public Sans. Standard/contextual ligatures are disabled so the displayed
   mythological names retain stable glyph identities.
2. `Paragraph::visit` provides positioned glyph IDs and fonts. Each available
   glyph outline is translated to its paragraph position and appended to one
   compound `SkPath`.
3. `SkPathMeasure` walks every closed outer and inner glyph contour. It samples
   positions at no more than two source pixels of arc length apart, with a
   minimum of ten samples per contour. This flattens lines, quadratics, conics,
   and cubics uniformly by geometric distance rather than by path verb count.
   The dense sampling is what allows an originally straight stem to bend
   smoothly after deformation.
4. The compound path bounds define the text's local coordinate system. Every
   sampled point is stored once as:

   ```text
   u = (x - bounds.left) / bounds.width
   v = (y - bounds.top)  / bounds.height
   ```

   Both parameters are clamped to `[0, 1]`.
5. The source path fill type is retained. Each normalized contour is also kept
   separate, so counters such as the holes in `P`, `O`, `A`, and `R` remain
   closed contours when the text is rebuilt.

The normalized representation is immutable during animation. A frame allocates
only the transient warped `SkPath` required for that cell; paragraph objects and
source glyph paths are released after preprocessing.

## Envelope families

Each grid cell has a stable random seed, phase, and envelope kind. The kind
distribution is balanced before being shuffled, preventing a random canvas
from accidentally omitting most of the available forms.

- **Globe:** seeded fractal Perlin noise modulates the width, center, and
  vertical position of each control row without allowing a row to reverse. The
  polar rows retain at least 88% of the nominal half-width so the first and last
  letters remain distinct instead of collapsing together.
- **Semicircle:** the upper edge remains a straight horizontal line while the
  lower edge forms a breathing half-dome. Its curvature changes continuously,
  but a 38%-of-frame minimum thickness keeps letters readable at both ends.
- **Sinusoidal band:** the upper and lower edges follow the same high-amplitude
  travelling wave with screen-axis-parallel cross sections. A cell
  deterministically receives one, one-and-a-half, or two full cycles—half the
  preceding frequency. Its vertical band thickness remains doubled relative to
  the original version.
- **Sine (tangent):** uses the same lower-frequency centerline, but offsets both
  band edges along the centerline normal. The glyphs therefore stand
  perpendicular to the wave.
- **Rotating rectangle:** a sharp-cornered 2:1 rectangle rotates around its
  center. Its local width and height exchange continuously with
  `sin²(rotation)`: at quarter turns the dimensions have swapped before the
  rotation is applied, so every axis-aligned pose is landscape. Projected
  vertex heights divide the surface into piecewise-linear vertical spans.
  Horizontal scanlines rebuild every span from its left and right edge
  intersections while the word remains left-to-right and upright.
- **Arc (axis parallel):** four joined spans trace a left-to-right arc that
  breathes from 126 to 180 degrees. Its cross sections stay parallel to the
  screen's y-axis. The band is twice the previous thickness.
- **Arc (tangent):** follows the same centerline, but every cross section uses
  the normal derived from the current centerline tangent. Letters therefore
  stand along the local radius while retaining left-to-right order. It uses the
  same doubled thickness.
- **Flag:** a multi-span travelling wave offsets the centerline while the band
  thickness changes independently.
- **Hourglass:** moderately inset middle controls produce curved sides without
  crushing the center letters. The waist expands and contracts symmetrically,
  with no lateral center shift.
- **Pot:** successive control rows form a rim, narrow neck, broad belly, and
  narrower foot.
- **Heart:** multiple spans describe two upper lobes, a center notch, and a
  lower point while the shape subtly pulses and sways. Both side edges retain
  at least ten logical pixels of height for the first and last letters.
- **Flame:** an animated off-center tip, secondary shoulder, and rounded base
  create an asymmetric flickering silhouette. Its side edges use the same
  ten-logical-pixel minimum height.

These generators deliberately output the same 16-point representation. Adding
an envelope therefore does not change subdivision, text mapping, or rendering;
the more oscillatory families simply join multiple 16-point patches.

## Orientation contract

The patch's parameterization, rather than its contour winding, determines text
orientation. Every generator obeys two invariants:

- increasing `u` always progresses from the cell's left side toward its right;
- increasing `v` always progresses from the envelope's upper boundary toward
  its lower boundary.

No generator applies a rigid rotation to the complete control lattice. For the
rotating rectangle, the source polygon rotates while its two local dimensions
exchange; new horizontal left/right spans are calculated for the current
polygon orientation. Patch breaks land exactly at projected vertex heights,
preserving its sharp corners. For either arc, `u`
follows a monotonic sine of the arc angle, even at a 180-degree sweep. The
axis-parallel arc keeps global vertical cross sections. The tangent arc and
tangent sine compute `normal = normalize(-tangent.y, tangent.x)` and offset
their upper and lower curves along that vector. Thus all variants preserve
reading order, while the tangent variants intentionally rotate each letter's
local vertical axis.

## Per-frame mapping and rendering

For every cell, every frame:

1. The cell's envelope generator produces its current 4 × 4 lattice.
2. The four patch edges are densely evaluated and joined into a closed path.
   That path is drawn first using alternating pale blue and pale pink at 20%
   opacity.
3. Two constant-`u` and two constant-`v` isolines are sampled from the exact
   patch and drawn as one-pixel black strokes at 20% opacity. Because these
   lines use the same evaluator as the text, they reveal the local warp
   direction rather than approximating it.
4. Every normalized contour point `(u, v)` is evaluated through the patch. A
   closed Skia path is reconstructed with the original compound fill rule and
   drawn with a solid black fill.
5. A grey chip is centered at the bottom of the cell and names the active
   envelope family. Larger canvases retain the preceding 2.5× legend sizing
   calculation unchanged. At the 760-logical-pixel compact breakpoint, the
   font switches to a smaller cell-relative scale with device-scale-aware
   limits. Long labels retain the responsive height but apply horizontal font
   scaling to stay inside the cell. The label is ordinary, undeformed black
   text.

The draw order keeps the envelope readable behind the text without washing out
the lettering.

Each envelope starts in a centered 5:2 frame. After all three visual layers
have been generated, their bounds are unioned. One uniform scale and translation
then centers that complete union inside the cell's padded target rectangle.
Because the fit is based on the final animated geometry rather than the source
frame, even a 180-degree arc or high-amplitude wave remains inside its own cell
and cannot overlap a neighbor.

Every cell is outlined with a one-pixel grey stroke at 40% opacity. The
artwork fit excludes the legend band, so labels neither cover the warped text
nor affect its scale.

## Grid and word sequencing

The grid contains exactly one instance of each of the 12 envelope families.
Large canvases use four columns and three rows. Canvases no wider than 760
logical pixels use three columns and four rows, which guarantees three entries
per row on mobile. Cell width and height are calculated independently from the
available canvas, making each entry wider than the previous square-cell grid
while keeping the complete grid centered. Uniform inset padding prevents
neighboring envelopes from touching.

Envelope motion and word sequencing use a shared clock accelerated by 1.5.
The logical word phase is 6.4 animated seconds (approximately 4.27 wall
seconds), twice the preceding duration. For cell index `i` and cycle frame `j`,
the selected word is:

```text
word[(i + j) mod 7]
```

This creates the requested stagger: adjacent cells show different words while
all cells rotate through the complete list. The same cycle frame selects a font
through the common `text::CyclingFontIndex` API. All 21 word/font outline sets
are prepared once, so a cycle boundary only switches cached normalized
contours; it does not rebuild paragraphs during rendering.

All envelope families except the already-fast sinusoid apply a further 1.65×
motion multiplier. This affects control-point animation only; it does not alter
the word-selection cadence.

## Resize, lifetime, and failure behavior

- Text contours are prepared once and retained as normalized points.
- Grid metadata is rebuilt only when the canvas dimensions change.
- Each patch is a 16-point value object local to one render iteration.
- Skia paths, paragraphs, and paints use automatic or smart-pointer lifetime
  management.
- Failure to acquire a font manager, Unicode service, WebGL surface, or glyph
  outline data stops rendering cleanly. A failed WebGL presentation is reported
  to the worker's error stream.
- Degenerate paragraph bounds and empty/open contours are rejected during
  preprocessing so division by zero and malformed fills cannot enter the
  animation.

## Validation

The bicubic unit test checks corner interpolation, planar evaluation, constant
partials, domain clamping, and non-finite input handling. The Perlin test checks
determinism, range normalization, lattice behavior, fractal range, and invalid
coordinates. The exported WebAssembly build validates that Paragraph, ICU,
Skia path measurement, the geometry helpers, and Ganesh link together in the
production toolchain.
