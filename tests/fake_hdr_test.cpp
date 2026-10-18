#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

#include "color.hpp"
#include "config.hpp"

int main() {
    using namespace fz;
    for (float strength : {0.0f, 0.25f, 0.5f, 1.0f}) {
        assert(fake_hdr_curve(0.0f, strength) == 0.0f);
        assert(fake_hdr_curve(1.0f, strength) == 1.0f);
        assert(fake_hdr_curve(0.5f, strength) == 0.5f);
        float prev = -1.0f;
        for (int i = 0; i <= 65535; ++i) {
            float x = i / 65535.0f;
            float y = fake_hdr_curve(x, strength);
            assert(y >= prev && y >= 0.0f && y <= 1.0f);
            assert(std::abs(y + fake_hdr_curve(1.0f - x, strength) - 1.0f) < 0.000001f);
            if (strength == 0.0f)
                assert(y == x);
            prev = y;
        }
    }
    assert(fake_hdr_curve(0.25f, 1.0f) < 0.25f);
    assert(fake_hdr_curve(0.75f, 1.0f) > 0.75f);
    assert(fake_hdr_curve(0.25f, -1.0f) == 0.25f);
    assert(fake_hdr_curve(0.25f, 2.0f) == fake_hdr_curve(0.25f, 1.0f));
    assert(fake_hdr_curve(0.25f, std::numeric_limits<float>::quiet_NaN()) == 0.25f);

    std::array<std::uint16_t, 256> identity;
    for (std::size_t i = 0; i < identity.size(); ++i)
        identity[i] = i;
    auto lut = identity;
    for (float strength : {0.0f, -1.0f, std::numeric_limits<float>::infinity()}) {
        apply_fake_hdr(lut.data(), lut.size(), 8, strength);
        assert(lut == identity);
    }

    // Exercise the real two-part CMU LUT2, gamma, dimming and range order.
    for (float gamma : {2.2f, DEFAULT_GAMMA, 4.0f}) {
        std::array<std::uint16_t, 960> ramp;
        regamma_ramp(ramp.data(), 512, gamma, 8, 0.0f, 0.125f);
        regamma_ramp(ramp.data() + 512, 448, gamma, 8, 0.125f, 1.0f);
        auto original = ramp;
        apply_fake_hdr(ramp.data(), ramp.size(), 8, 0.0f);
        assert(ramp == original);
        apply_fake_hdr(ramp.data(), ramp.size(), 8, 1.0f);
        assert(ramp.front() == 0 && ramp.back() == 255);
        assert(ramp[511] == ramp[512]);
        for (std::size_t i = 1; i < ramp.size(); ++i)
            assert(ramp[i] >= ramp[i - 1] && ramp[i] <= 255);
        apply_luma(ramp.data(), ramp.size(), 8, -0.3f);
        assert(ramp.back() == 179);
        apply_range(ramp.data(), ramp.size(), 8, MIN_LIMITED_RANGE, MAX_LIMITED_RANGE);
        for (auto value : ramp)
            assert(value <= 179);
    }

    Config config;
    assert(config.configuration.settings.fake_hdr == 0.0f);
    Config::ini_handler(&config, "settings", "fake_hdr", "0.75");
    assert(config.configuration.settings.fake_hdr == 0.75f);
    std::cout << "Fake HDR curve, CMU LUT pipeline and config parser tests passed\n";
}
