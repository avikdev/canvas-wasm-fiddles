# Fiddle: Swirl Deform

First implement a C++ library to perform Catmull-Rom spline on a set of points, and yield a cubic bezier path which can be drawn with Skia.

This fiddle demonstrates swirl field on a section of curve. There is a similar feature in Adobe Illustrator to apply such local deform on a path with a effect radius.

First draw several static shapes on the canvas. Divide the canvas into approximate square grid of cell size min(width, height) / 4. In each cell draw a random shape leaving little padding.
- Pick from: Circle, Triangle, rectangle, Hexagon, Semi circle, deformed blob (use cubic bezier close circle like path with random displacement)
- The original set of shapes are precomputed, not per frame
- Render using 1px stroke, random HSL color, fixed saturation 75%, lightness 60%, random hue one of 12 evenly spaced values. Ensure to reuse common color util library.

Then have an animated circle of diameter 0.8 times the cell size, moving across the canvas in unoform linear motion, bouncing back from the sides.
- Render this circle with 30% opacity, grey fill, no stroke.

Now apply swirl field within this circle. There is a rotating motion at the center of the circle, dies of near the perimeter. The closer to its center the stronger the tangential force. Apply your intuition to pick a force intensity and any other params. List those params at the top of the .cc file with explanations.

The swirl will affect only the part of the shapes it overlaps with. First compute the intersection : {grid shape \intersection swirl circle}. Divide the curve into closely spaced vertices, then apply the tagential force to rotate them. As a result the points will be displaced. But we still keep track which curve segments the sequence of points belong to.

Finally apply Catmull-Rom spline on the points to convert them back to piecewise bezier path, and draw them on canvas.

# Revision A

- In the grid shapes, use only fill color no stroke. They are too faint. Need bright saturated filled shapes.
- Reduce the cell padding, also use uniform x-padding and y-padding. Keep the content center aligned in the canvas.
- The animated circle reduce opacity to 15%.
- Increase the tangential for and intensity of the swirl field significantly. A point near the center of the field should be rotated by 3 x 360-degrees, and look like a spiral shaped formed within that circle.
- In the mix of the shapes, add new types. See the fiddle "Shape intersection":
 (a) Tentacled blobs. Maybe it's better to re-factor that code into an util library for shaper builder. There havinf a function to created such tentacled blobs with given shapes, center and number of tentacles.
 (b) Cross shape similar to "Shape intersection". Ok to re-factor into shape builder.
 (c) Polygons with holes, see "Shape intersection". Ok to re-factor into shape builder.
 (d) Semi circle with semic-circle hole, Ok to re-factor this into shape builder.
 (e) Star shapes, 6 to 7 sides. Ok to re-factor into shape builder. Not too sharp.

Revise Fiddle: Swirl Deform
- Reduce the speed of the moving circle, to 70%
- Add more erraticness in its direction, currently it moves in straightline and reflected symmetrically. Plan a motion algorithm to cover all areas of teh canvas.
- Interoduce more circles, have total 4 circles of different sizes, 0.5x, 0.8x, 1x, 1.4x. They all move in similar way but avoid collision. Detect collision and change direction.
- 2 circles has their tagential force clockwise, and 2 anti-clockwise.
- Remove the grey fill color from the circles, and use 2px black dashed stroke. Sparse dashed lines (2px dash, 4px gap).

Update its description in top level readme. Not about the drawing logic, but how the swirl deform ia calculated on shapes.
