#include "utils/random_permutation.h"

#include <algorithm>
#include <iostream>
#include <vector>

namespace {

bool Expect(bool condition, const char *message) {
  if (!condition) {
    std::cerr << message << '\n';
  }
  return condition;
}

} // namespace

int main() {
  bool success = true;

  success &= Expect(utils::CreateRandomPermutation(0, 12).empty(),
                    "An empty range should produce an empty permutation.");
  success &=
      Expect(utils::CreateRandomPermutation(1, 12) == std::vector<int>{0},
             "A single-item permutation should contain index zero.");

  const std::vector<int> first = utils::CreateRandomPermutation(9, 0xE1A57C);
  const std::vector<int> repeated = utils::CreateRandomPermutation(9, 0xE1A57C);
  success &= Expect(first == repeated,
                    "The same seed should reproduce the same sequence.");

  std::vector<int> sorted = first;
  std::sort(sorted.begin(), sorted.end());
  success &= Expect(sorted == std::vector<int>({0, 1, 2, 3, 4, 5, 6, 7, 8}),
                    "Every index should appear exactly once.");

  const std::vector<int> grid =
      utils::CreateRandomGridTraversal(3, 3, 0xE1A57C);
  sorted = grid;
  std::sort(sorted.begin(), sorted.end());
  success &= Expect(sorted == std::vector<int>({0, 1, 2, 3, 4, 5, 6, 7, 8}),
                    "A grid traversal should visit every cell exactly once.");

  for (std::size_t index = 1; index < grid.size(); ++index) {
    success &= Expect(grid[index - 1] / 3 != grid[index] / 3,
                      "Adjacent grid entries should use different rows.");
  }
  success &= Expect(grid.back() / 3 != grid.front() / 3,
                    "The repeated grid traversal should change rows.");

  success &= Expect(utils::CreateRandomGridTraversal(0, 3, 12).empty(),
                    "A grid without rows should be empty.");
  success &= Expect(utils::CreateRandomGridTraversal(3, 0, 12).empty(),
                    "A grid without columns should be empty.");

  return success ? 0 : 1;
}
