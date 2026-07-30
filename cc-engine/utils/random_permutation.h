#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace utils {

std::vector<int> CreateRandomPermutation(std::size_t count, std::uint32_t seed);

std::vector<int> CreateRandomGridTraversal(std::size_t row_count,
                                           std::size_t column_count,
                                           std::uint32_t seed);

} // namespace utils
