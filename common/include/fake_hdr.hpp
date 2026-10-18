// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace fz {

// Global SDR approximation, not HDR encoding or local/adaptive tone mapping.
// Blend with a gentle S-curve: endpoints are fixed and the derivative remains
// positive (>= 0.4), so neither shadows nor highlights are hard-clipped.
inline float fake_hdr_curve(float x, float strength) {
    x = std::clamp(x, 0.0f, 1.0f);
    strength = std::isfinite(strength) ? std::clamp(strength, 0.0f, 1.0f) : 0.0f;
    return x + 0.6f * strength * x * (1.0f - x) * (2.0f * x - 1.0f);
}

// Apply in display-encoded space after regamma, before luminance/range.
// A zero/invalid strength leaves the original LUT byte-for-byte unchanged.
inline void apply_fake_hdr(std::uint16_t *array, std::size_t size,
        std::size_t nb_bits, float strength) {
    if (!std::isfinite(strength) || strength <= 0.0f)
        return;

    const auto max = static_cast<float>((1u << nb_bits) - 1);
    for (std::size_t i = 0; i < size; ++i)
        array[i] = static_cast<std::uint16_t>(std::round(fake_hdr_curve(array[i] / max, strength) * max));
}

} // namespace fz
