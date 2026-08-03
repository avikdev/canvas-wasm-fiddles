#include "morphing/correspondence_solver.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>

namespace skmorph::correspondence {
namespace {

constexpr float kPi = 3.14159265358979323846F;

float AngleDistance(float first, float second) {
  return std::abs(std::remainder(first - second, 2.0F * kPi)) / kPi;
}

float SkipCost(const Pivot &pivot, const SolverOptions &options) {
  return pivot.features == kOrdinary ? options.ordinarySkipPenalty
                                     : options.featureSkipPenalty;
}

float PairCost(const Pivot &source, const Pivot &target,
               const SolverOptions &options) {
  const uint32_t feature_union = source.features | target.features;
  const uint32_t feature_difference = source.features ^ target.features;
  float feature_penalty = 0.0F;
  if (feature_union != 0U) {
    feature_penalty = static_cast<float>(std::popcount(feature_difference)) /
                      static_cast<float>(std::popcount(feature_union));
    if (source.features == kOrdinary || target.features == kOrdinary) {
      feature_penalty += 1.0F;
    }
  }
  return options.arcWeight * std::abs(source.position - target.position) +
         options.radialAngleWeight *
             AngleDistance(source.radialAngle, target.radialAngle) +
         options.tangentWeight *
             AngleDistance(source.tangentAngle, target.tangentAngle) +
         options.curvatureWeight *
             std::min(2.0F, std::abs(source.scaledCurvature -
                                     target.scaledCurvature)) +
         options.featureWeight * feature_penalty;
}

} // namespace

std::vector<Match> SolveMonotonic(const std::vector<Pivot> &source,
                                  const std::vector<Pivot> &target,
                                  const SolverOptions &options) {
  if (source.size() < 2U || target.size() < 2U) {
    return {};
  }
  const size_t source_interior = source.size() - 2U;
  const size_t target_interior = target.size() - 2U;
  const size_t columns = target_interior + 1U;
  std::vector<uint8_t> decisions((source_interior + 1U) * columns, 0U);
  std::vector<float> previous(columns, 0.0F);
  std::vector<float> current(columns, 0.0F);
  for (size_t column = 1; column <= target_interior; ++column) {
    previous[column] =
        previous[column - 1U] + SkipCost(target[column], options);
    decisions[column] = 2U;
  }
  for (size_t row = 1; row <= source_interior; ++row) {
    current[0] = previous[0] + SkipCost(source[row], options);
    decisions[row * columns] = 1U;
    for (size_t column = 1; column <= target_interior; ++column) {
      const float skip_source =
          previous[column] + SkipCost(source[row], options);
      const float skip_target =
          current[column - 1U] + SkipCost(target[column], options);
      const float match = previous[column - 1U] +
                          PairCost(source[row], target[column], options);
      if (match <= skip_source && match <= skip_target) {
        current[column] = match;
        decisions[row * columns + column] = 3U;
      } else if (skip_source <= skip_target) {
        current[column] = skip_source;
        decisions[row * columns + column] = 1U;
      } else {
        current[column] = skip_target;
        decisions[row * columns + column] = 2U;
      }
    }
    std::swap(previous, current);
  }

  std::vector<Match> result = {{source.size() - 1U, target.size() - 1U}};
  size_t row = source_interior;
  size_t column = target_interior;
  while (row > 0U || column > 0U) {
    const uint8_t decision = decisions[row * columns + column];
    if (decision == 3U) {
      result.push_back({row, column});
      --row;
      --column;
    } else if (decision == 1U) {
      --row;
    } else {
      --column;
    }
  }
  result.push_back({0U, 0U});
  std::reverse(result.begin(), result.end());
  return result;
}

} // namespace skmorph::correspondence
