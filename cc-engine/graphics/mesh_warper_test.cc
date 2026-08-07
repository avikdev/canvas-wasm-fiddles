#include "graphics/mesh_warper.h"

#include <cassert>
#include <cmath>

#include "include/core/SkPathBuilder.h"

namespace {

bool Near(float a, float b) { return std::abs(a - b) < 0.001F; }

int CountCubics(const SkPath &path) {
  int count = 0;
  SkPath::RawIter iterator(path);
  SkPoint points[4];
  for (SkPath::Verb verb = iterator.next(points); verb != SkPath::kDone_Verb;
       verb = iterator.next(points)) {
    count += verb == SkPath::kCubic_Verb ? 1 : 0;
  }
  return count;
}

} // namespace

int main() {
  graphics::MeshWarper deformer;
  graphics::MeshWarperOptions options;
  options.epsilon_gap = 20.0F;
  assert(deformer.Build(SkRect::MakeWH(100.0F, 100.0F), options));
  assert(Near(deformer.EpsilonGap(), 20.0F));
  assert(deformer.positions().size() == 36U);
  assert(deformer.indices().size() == 150U);
  assert(!deformer.ControlGridPath().isEmpty());

  const auto original = deformer.positions();
  assert(deformer.BeginDrag({40.0F, 40.0F}, 28.0F));
  assert(deformer.UpdateDrag({55.0F, 48.0F}));
  const auto preview = deformer.positions();
  assert(preview[14].fX > original[14].fX);
  assert(preview[14].fY > original[14].fY);
  const SkPoint mapped_center = deformer.MapPoint({40.0F, 40.0F});
  assert(mapped_center.fX > 50.0F);
  assert(mapped_center.fY > 45.0F);

  SkPathBuilder circle_builder;
  circle_builder.addCircle(50.0F, 50.0F, 20.0F);
  const SkPath deformed_circle = deformer.DeformPath(circle_builder.detach());
  assert(!deformed_circle.isEmpty());
  assert(CountCubics(deformed_circle) >= 8);

  // Every outer-ring vertex is a hard Dirichlet boundary.
  for (int row = 0; row <= 5; ++row) {
    for (int column = 0; column <= 5; ++column) {
      if (row == 0 || row == 5 || column == 0 || column == 5) {
        const std::size_t index = static_cast<std::size_t>(row * 6 + column);
        assert(Near(preview[index].fX, original[index].fX));
        assert(Near(preview[index].fY, original[index].fY));
      }
    }
  }

  deformer.CancelDrag();
  assert(Near(deformer.positions()[14].fX, original[14].fX));
  assert(deformer.BeginDrag({40.0F, 40.0F}, 28.0F));
  assert(deformer.UpdateDrag({55.0F, 48.0F}));
  assert(deformer.CommitDrag());
  assert(deformer.positions()[14].fX > original[14].fX);
  assert(!deformer.IsDragging());

  graphics::MeshWarperOptions push_options;
  push_options.epsilon_gap = 10.0F;
  push_options.damping_start_distance = 20.0F;
  push_options.far_drag_response = 0.10F;

  graphics::MeshWarper damped;
  assert(damped.Build(SkRect::MakeWH(200.0F, 100.0F), push_options));
  assert(damped.BeginDrag({50.0F, 50.0F}, 18.0F));
  assert(damped.UpdateDrag({150.0F, 50.0F}));
  const SkPoint damped_handle = damped.MapPoint({50.0F, 50.0F});
  assert(damped_handle.fX > 65.0F);
  assert(damped_handle.fX < 90.0F);

  // The compact swept brush leaves the opposite side exactly unchanged.
  const SkPoint far_original = {180.0F, 20.0F};
  const SkPoint far_mapped = damped.MapPoint(far_original);
  assert(Near(far_mapped.fX, far_original.fX));
  assert(Near(far_mapped.fY, far_original.fY));

  // An extreme push is shortened before row/column order or triangle
  // orientation can invert.
  graphics::MeshWarper guarded;
  push_options.damping_start_distance = 0.0F;
  assert(guarded.Build(SkRect::MakeWH(200.0F, 100.0F), push_options));
  assert(guarded.BeginDrag({40.0F, 50.0F}, 24.0F));
  assert(guarded.UpdateDrag({190.0F, 95.0F}));
  const auto &guarded_positions = guarded.positions();
  constexpr int columns = 20;
  constexpr int rows = 10;
  const auto index_at = [](int column, int row) {
    return static_cast<std::size_t>(row * (columns + 1) + column);
  };
  for (int row = 0; row <= rows; ++row) {
    for (int column = 0; column < columns; ++column) {
      assert(guarded_positions[index_at(column, row)].fX <
             guarded_positions[index_at(column + 1, row)].fX);
    }
  }
  for (int column = 0; column <= columns; ++column) {
    for (int row = 0; row < rows; ++row) {
      assert(guarded_positions[index_at(column, row)].fY <
             guarded_positions[index_at(column, row + 1)].fY);
    }
  }
  const auto &indices = guarded.indices();
  for (std::size_t offset = 0; offset < indices.size(); offset += 3U) {
    const SkPoint &a = guarded_positions[indices[offset]];
    const SkPoint &b = guarded_positions[indices[offset + 1U]];
    const SkPoint &c = guarded_positions[indices[offset + 2U]];
    const float twice_area =
        (b.fX - a.fX) * (c.fY - a.fY) - (b.fY - a.fY) * (c.fX - a.fX);
    assert(twice_area > 0.0F);
  }

  // Once a continued drag reaches topology protection, later pointer frames
  // must retain or advance the valid preview rather than bouncing backward.
  const auto first_guarded_preview = guarded.positions();
  assert(guarded.UpdateDrag({340.0F, 140.0F}));
  const auto second_guarded_preview = guarded.positions();
  const SkPoint guarded_direction = {150.0F, 45.0F};
  for (std::size_t index = 0; index < second_guarded_preview.size(); ++index) {
    const SkPoint frame_delta = {
        second_guarded_preview[index].fX - first_guarded_preview[index].fX,
        second_guarded_preview[index].fY - first_guarded_preview[index].fY};
    assert(frame_delta.fX * guarded_direction.fX +
               frame_delta.fY * guarded_direction.fY >=
           -0.01F);
  }
  assert(guarded.CommitDrag());

  // A nearby perpendicular gesture can still move unsaturated vertices after
  // a strong east-west/diagonal deformation.
  const auto before_perpendicular = guarded.positions();
  assert(guarded.BeginDrag({75.0F, 50.0F}, 24.0F));
  assert(guarded.UpdateDrag({75.0F, 5.0F}));
  float total_vertical_change = 0.0F;
  for (std::size_t index = 0; index < guarded.positions().size(); ++index) {
    total_vertical_change += std::abs(guarded.positions()[index].fY -
                                      before_perpendicular[index].fY);
  }
  assert(total_vertical_change > 1.0F);
  return 0;
}
