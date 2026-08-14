#include "utils/weighted_picker.h"

#include <array>
#include <cassert>
#include <random>
#include <vector>

int main() {
  utils::WeightedPicker picker({1.0, 3.0, 6.0});
  std::mt19937 first_random(12345U);
  std::mt19937 repeated_random(12345U);
  assert(picker.PickSamples(100, first_random) ==
         picker.PickSamples(100, repeated_random));

  std::mt19937 distribution_random(77U);
  std::array<int, 3> counts = {};
  for (std::size_t picked : picker.PickSamples(10000, distribution_random)) {
    assert(picked < counts.size());
    ++counts[picked];
  }
  assert(counts[0] > 750 && counts[0] < 1250);
  assert(counts[1] > 2600 && counts[1] < 3400);
  assert(counts[2] > 5500 && counts[2] < 6500);

  utils::WeightedPicker sparse({0.0, -4.0, 2.0, 0.0});
  std::mt19937 sparse_random(1U);
  for (std::size_t picked : sparse.PickSamples(20, sparse_random)) {
    assert(picked == 2U);
  }

  utils::WeightedPicker empty({0.0, -1.0});
  assert(empty.empty());
  assert(empty.PickOne(sparse_random) == utils::WeightedPicker::kInvalidIndex);
  assert(empty.PickSamples(4, sparse_random).empty());
  return 0;
}
