// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace fz {

// Relative gamma for display-encoded shadows, not the overall sRGB exponent.
// Above half scale the output is unchanged. The squared taper makes both the
// value and slope meet the identity smoothly at the boundary. Black stays black.
inline float shadow_gamma_curve(float x, float gamma) {
    x = std::clamp(x, 0.0f, 1.0f);
    gamma = std::isfinite(gamma) ? std::clamp(gamma, 0.5f, 1.5f) : 1.0f;
    if (gamma == 1.0f || x == 0.0f || x >= 0.5f)
        return x;

    const float u = 2.0f * x;
    const float taper = (1.0f - u) * (1.0f - u);
    return x + 0.5f * taper * (std::pow(u, 1.0f / gamma) - u);
}

// After regamma/Fake HDR, before luminance, dimming and output range mapping.
// Neutral/invalid values must preserve the existing LUT exactly.
inline void apply_shadow_gamma(std::uint16_t *array, std::size_t size,
        std::size_t nb_bits, float gamma) {
    if (!std::isfinite(gamma) || gamma == 1.0f)
        return;
    const auto max = static_cast<float>((1u << nb_bits) - 1);
    for (std::size_t i = 0; i < size; ++i)
        array[i] = static_cast<std::uint16_t>(std::round(shadow_gamma_curve(array[i] / max, gamma) * max));
}

} // namespace fz
