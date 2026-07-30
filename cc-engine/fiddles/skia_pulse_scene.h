#pragma once

class SkCanvas;

// The shared scene used by both benchmark fiddles. Keeping every Skia draw
// command here makes the WebGL and CPU paths differ only at the SkSurface.
void DrawSkiaPulseScene(SkCanvas *canvas, int width, int height,
                        double time_seconds);
