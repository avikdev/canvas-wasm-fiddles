#pragma once

class SkCanvas;

// Builds the shared animated Skia scene used by both the WebGL and CPU
// fiddles. Keeping all draw commands here makes backend comparisons exact.
void DrawSkiaDrawing(SkCanvas *canvas, int width, int height,
                     double time_seconds);
