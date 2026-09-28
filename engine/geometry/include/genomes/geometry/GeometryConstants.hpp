#pragma once

namespace genomes::geometry {

// Shared numerical policy for generated topology. Domain generators may
// choose stricter thresholds, but they should not invent unrelated literals
// for the same validity question.
inline constexpr float kPositionEpsilon = 1.0e-6F;
inline constexpr float kTriangleAreaEpsilon = 1.0e-8F;
inline constexpr float kWeightEpsilon = 1.0e-5F;
inline constexpr float kSectionOrderingEpsilon = 1.0e-5F;
inline constexpr float kNormalLengthEpsilon = 1.0e-6F;

} // namespace genomes::geometry
