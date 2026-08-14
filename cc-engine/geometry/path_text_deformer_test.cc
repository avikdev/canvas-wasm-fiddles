#include "geometry/path_text_deformer.h"

#include <cassert>
#include <cmath>

#include "include/core/SkPathBuilder.h"

int main() {
  const SkPath straight =
      SkPathBuilder().moveTo(0.0F, 50.0F).lineTo(100.0F, 50.0F).detach();
  geometry::PathTextDeformer straight_deformer(straight);
  assert(straight_deformer.valid());
  assert(std::abs(straight_deformer.length() - 100.0F) < 0.01F);
  geometry::PathTextDeformOptions options;
  const SkPoint mapped =
      straight_deformer.MapPoint({20.0F, -5.0F}, 0.0F, options);
  assert(std::abs(mapped.fX - 20.0F) < 0.01F);
  assert(std::abs(mapped.fY - 45.0F) < 0.01F);

  const SkPath corner = SkPathBuilder()
                            .moveTo(0.0F, 50.0F)
                            .lineTo(50.0F, 50.0F)
                            .lineTo(50.0F, 0.0F)
                            .detach();
  geometry::PathTextDeformer corner_deformer(corner);
  options.curvature_probe = 4.0F;
  options.protect_sharp_turns = true;
  const geometry::PathFrame frame = corner_deformer.FrameAt(50.0F, options);
  assert(std::isfinite(frame.signed_curvature));
  assert(std::abs(frame.turn_angle) > 0.5F);
  const SkPath glyph =
      SkPathBuilder()
          .addRect(SkRect::MakeLTRB(45.0F, -12.0F, 55.0F, 4.0F))
          .detach();
  const SkPath deformed = corner_deformer.Deform(glyph, 0.0F, options);
  assert(!deformed.isEmpty());
  assert(deformed.isFinite());
  return 0;
}
