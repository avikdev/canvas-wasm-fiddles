# Contour Lines

## Purpose

Contour Lines demonstrates how a sampled scalar field becomes continuous
vector geometry. The implementation separates coherent field generation,
topology extraction, segment stitching, curve reconstruction, and region
construction so each stage can be reused independently.

The fiddle key is `contour-lines`.

## Scalar field

The existing Perlin utility now exposes deterministic three-dimensional
gradient noise in addition to its two-dimensional API. A frame evaluates four
fractal octaves at:

```text
noise(x * spatial_scale, y * spatial_scale, time * temporal_scale)
```

Treating time as the third coordinate produces a smooth slice through one
coherent 3D field rather than unrelated 2D frames. The base spatial coefficient
is `0.003`, 40% of the initial value. It is multiplied by
`clamp(600 / min(canvas_width, canvas_height), 1, 2.25)`, preserving the
large-canvas scale while adding enough field variation on compact canvases.
The time coefficient is `0.03`, one quarter of the initial value, slowing
evolution along the third dimension. The finite slice is normalized to
`[0, 1]` before extraction because a bounded view rarely contains the
theoretical global extrema of Perlin noise; normalization keeps all five
configured thresholds active.

## Marching Squares

`geometry::ScalarGrid` stores row-major values over a rectangular domain.
The target cell size is
`clamp(min(canvas_width, canvas_height) / 28, 10px, 18px)`. This keeps roughly
the same useful anchor count across devices, avoids overfitting tiny direction
changes on a large canvas, and bounds the maximum deviation from the sampled
field.
`ExtractMarchingSquaresContours` visits every four-sample cell, classifies its
corners against a threshold, and linearly interpolates each crossed grid edge.
The two ambiguous saddle cases compare the threshold with the bilinear center
approximation and choose the corresponding connection.

Each intersection retains the identity of its source horizontal or vertical
grid edge. Neighboring cells therefore produce exactly matching endpoint keys
without relying on approximate floating-point coordinate comparisons.
Adjacency lists walk those keys to turn unordered segment output into maximal
ordered open polylines and closed loops.

## Cubic reconstruction

Every stitched polyline is passed to the shared
`geometry::CatmullRomToCubicPath` converter. It emits one cubic Bézier segment
between adjacent samples and handles cyclic neighbors for closed loops. A
0.25 tension balances smooth curvature against uniform Catmull–Rom overshoot
at tight, uneven bends. The result is clipped to the scalar-field rectangle.

## Isobands

Five threshold curves define six scalar bands. Each sampled square is divided
into triangles, and each triangle is clipped in scalar-value space against the
band's lower and upper thresholds. Polygon fragments for the same band are
accumulated into one compound Skia path, producing a complete vector partition
without raster masks. Colors are precomputed once with evenly spaced HSL hues;
only geometry changes per frame.

## Complexity and failure behavior

For `G` grid cells and `L` contour levels, extraction costs `O(G × L)`.
Stitching is linear in the number of emitted segments because edge-key
adjacency uses hash lookup. Field samples, paths, and contour lists are
frame-local value objects and release their memory when rendering returns.

Invalid dimensions, non-finite scalar values, non-finite thresholds, and
unsafe Perlin lattice coordinates fail to empty geometry or zero noise rather
than passing malformed values to Skia.
