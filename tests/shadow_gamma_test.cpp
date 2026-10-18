#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

#include "color.hpp"

int main() {
    using namespace fz;
    // Sweep the entire slider range: no tonal reversals, clipping or discontinuity.
    for (int g = 0; g <= 100; ++g) {
        const float gamma = MIN_SHADOW_GAMMA + g * (MAX_SHADOW_GAMMA - MIN_SHADOW_GAMMA) / 100.0f;
        float previous = -1.0f;
        for (int i = 0; i <= 65536; ++i) {
            const float x = i / 65536.0f;
            const float y = shadow_gamma_curve(x, gamma);
            assert(std::isfinite(y) && y >= previous && y >= 0.0f && y <= 1.0f);
            if (x >= 0.5f || gamma == 1.0f || x == 0.0f)
                assert(y == x);
            previous = y;
        }
        // Smooth join to the unchanged upper half (approximately unit slope).
        constexpr float epsilon = 0.0001f;
        const float slope = (0.5f - shadow_gamma_curve(0.5f - epsilon, gamma)) / epsilon;
        assert(std::abs(slope - 1.0f) < 0.002f);
    }
    assert(shadow_gamma_curve(0.1f, 1.2f) > 0.1f);
    assert(shadow_gamma_curve(0.1f, 0.8f) < 0.1f);
    assert(shadow_gamma_curve(0.1f, -10.0f) == shadow_gamma_curve(0.1f, MIN_SHADOW_GAMMA));
    assert(shadow_gamma_curve(0.1f, 10.0f) == shadow_gamma_curve(0.1f, MAX_SHADOW_GAMMA));

    std::array<std::uint16_t, 256> identity;
    for (std::size_t i = 0; i < identity.size(); ++i) identity[i] = i;
    for (float gamma : {1.0f, std::numeric_limits<float>::quiet_NaN(),
            std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()}) {
        auto lut = identity;
        apply_shadow_gamma(lut.data(), lut.size(), 8, gamma);
        assert(lut == identity);
    }

    // Real CMU split ramp: correct order with Fake HDR, dimming and range mapping.
    for (float gamma : {2.2f, DEFAULT_GAMMA, 4.0f}) {
        for (float hdr : {0.0f, 0.5f, 1.0f}) {
            std::array<std::uint16_t, 960> original;
            regamma_ramp(original.data(), 512, gamma, 8, 0.0f, 0.125f);
            regamma_ramp(original.data() + 512, 448, gamma, 8, 0.125f, 1.0f);
            apply_fake_hdr(original.data(), original.size(), 8, hdr);
            for (float shadow : {MIN_SHADOW_GAMMA, 1.0f, MAX_SHADOW_GAMMA}) {
                auto ramp = original;
                apply_shadow_gamma(ramp.data(), ramp.size(), 8, shadow);
                if (shadow == 1.0f) assert(ramp == original);
                assert(ramp.front() == 0 && ramp.back() == 255);
                assert(ramp[511] == ramp[512]);
                for (std::size_t i = 0; i < ramp.size(); ++i) {
                    if (i) assert(ramp[i] >= ramp[i - 1]);
                    if (original[i] >= 128) assert(ramp[i] == original[i]);
                }
                apply_luma(ramp.data(), ramp.size(), 8, -0.3f);
                assert(ramp.back() == 179);
                apply_range(ramp.data(), ramp.size(), 8, MIN_LIMITED_RANGE, MAX_LIMITED_RANGE);
                for (std::size_t i = 1; i < ramp.size(); ++i)
                    assert(ramp[i] >= ramp[i - 1] && ramp[i] <= 179);
            }
        }
    }
    std::cout << "Shadow gamma curve, neutral defaults and combined CMU LUT tests passed\n";
}
