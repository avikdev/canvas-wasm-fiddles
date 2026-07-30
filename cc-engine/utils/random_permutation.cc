#include "utils/random_permutation.h"

#include <algorithm>
#include <numeric>
#include <random>

namespace utils {

std::vector<int> CreateRandomPermutation(std::size_t count,
                                         std::uint32_t seed) {
  std::vector<int> indices(count);
  std::iota(indices.begin(), indices.end(), 0);

  std::mt19937 generator(seed);
  std::shuffle(indices.begin(), indices.end(), generator);
  return indices;
}

std::vector<int> CreateRandomGridTraversal(std::size_t row_count,
                                           std::size_t column_count,
                                           std::uint32_t seed) {
  if (row_count == 0 || column_count == 0) {
    return {};
  }

  const std::vector<int> rows = CreateRandomPermutation(row_count, seed);
  const std::vector<int> columns =
      CreateRandomPermutation(column_count, seed ^ 0x9E3779B9U);

  std::vector<int> traversal;
  traversal.reserve(row_count * column_count);
  for (std::size_t round = 0; round < column_count; ++round) {
    for (std::size_t row_position = 0; row_position < row_count;
         ++row_position) {
      const int row = rows[row_position];
      const int column = columns[(round + row_position) % column_count];
      traversal.push_back(row * static_cast<int>(column_count) + column);
    }
  }
  return traversal;
}

} // namespace utils
