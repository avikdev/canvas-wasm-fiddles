#include "geometry/path_text_deformer.h"

#include <cassert>
#include <cmath>
#include <numbers>

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

  options.curvature_probe = 0.25F;
  options.corner_transition_length = 20.0F;
  float previous_angle =
      std::atan2(corner_deformer.FrameAt(41.0F, options).tangent.fY,
                 corner_deformer.FrameAt(41.0F, options).tangent.fX);
  for (float distance = 42.0F; distance <= 59.0F; distance += 1.0F) {
    const geometry::PathFrame smooth_frame =
        corner_deformer.FrameAt(distance, options);
    const float angle =
        std::atan2(smooth_frame.tangent.fY, smooth_frame.tangent.fX);
    const float angle_step = std::remainder(angle - previous_angle,
                                            2.0F * std::numbers::pi_v<float>);
    assert(std::abs(angle_step) < 0.25F);
    previous_angle = angle;
  }
  const SkPath glyph =
      SkPathBuilder()
          .addRect(SkRect::MakeLTRB(45.0F, -12.0F, 55.0F, 4.0F))
          .detach();
  const SkPath deformed = corner_deformer.Deform(glyph, 0.0F, options);
  assert(!deformed.isEmpty());
  assert(deformed.isFinite());
  return 0;
}
