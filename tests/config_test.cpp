#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <ini.h>
#include "config.hpp"

namespace {
FizeauConfiguration live_configuration;
bool live_active = true;
Result service_result = 0;
int apply_count = 0;

void parse(fz::Config &config, const std::string &ini) {
    assert(ini_parse_string(ini.c_str(), fz::Config::ini_handler, &config) == 0);
    config.sanitize();
}

void equal(const FizeauConfiguration &a, const FizeauConfiguration &b) {
    const auto &x = a.settings, &y = b.settings;
    assert(x.temperature == y.temperature);
    for (auto [v, w] : {std::pair{x.saturation, y.saturation}, {x.hue, y.hue},
            {x.contrast, y.contrast}, {x.gamma, y.gamma}, {x.luminance, y.luminance},
            {x.fake_hdr, y.fake_hdr}, {x.range.lo, y.range.lo}, {x.range.hi, y.range.hi}})
        assert(std::abs(v - w) < 0.00001f);
    assert(a.components == b.components && a.filter == b.filter);
    assert(a.dimming_timeout == b.dimming_timeout);
}
}

// Host service stand-ins. Configuration parsing, sanitizing and serialization
// below use the actual production implementation and the real inih parser.
extern "C" Result fizeauGetIsActive(bool *value) {
    if (!service_result) *value = live_active;
    return service_result;
}
extern "C" Result fizeauGetConfiguration(FizeauConfiguration *value) {
    if (!service_result) *value = live_configuration;
    return service_result;
}
extern "C" Result fizeauSetConfiguration(const FizeauConfiguration *value) {
    if (!service_result) { live_configuration = *value; ++apply_count; }
    return service_result;
}

int main() {
    using fz::Config;
    static_assert(FizeauCommandId_GetConfiguration == 8);
    static_assert(FizeauCommandId_SetConfiguration == 9);
    static_assert(sizeof(FizeauConfiguration) == 48);

    Config config;
    assert(config.configuration.settings.temperature == DEFAULT_TEMP);
    assert(config.configuration.settings.fake_hdr == 0.0f);
    parse(config, R"(active = false
[settings]
temperature = 7200
saturation = 1.2
hue = -0.3
contrast = 1.1
gamma = 2.6
luminance = -0.2
fake_hdr = 0.75
range = 0.1-0.9
components = rb
filter = green
dimming_timeout = 03:24
)");
    assert(!config.active);
    assert(config.configuration.settings.fake_hdr == 0.75f);
    assert(config.configuration.components == (Component_Red | Component_Blue));
    assert(config.configuration.dimming_timeout == (Time{0, 3, 24}));
    const auto saved = config.make();
    for (const auto *old_key : {"profile", "_day", "_night", "dawn", "dusk"})
        assert(saved.find(old_key) == std::string::npos);
    Config roundtrip;
    parse(roundtrip, saved);
    equal(config.configuration, roundtrip.configuration);
    assert(!roundtrip.active);

    Config legacy;
    assert(ini_parse("fixtures/legacy.cfg", Config::ini_handler, &legacy) == 0);
    assert(legacy.configuration.settings.temperature == 6500);
    assert(legacy.configuration.settings.luminance == 0.0f);
    assert(legacy.configuration.dimming_timeout == (Time{0, 5, 0}));
    parse(legacy, R"(handheld_profile = profile4
docked_profile = profile2
[profile1]
temperature_day = 6000
fake_hdr_day = 0.5
temperature_night = 1000
fake_hdr_night = 1.0
dusk_begin = 00:00
dawn_begin = 00:00
[profile2]
temperature_day = 3000
fake_hdr_day = 0.9
[profile4]
temperature_day = 4000
)");
    assert(legacy.configuration.settings.temperature == 6000);
    assert(legacy.configuration.settings.fake_hdr == 0.5f);
    Config migrated;
    parse(migrated, legacy.make());
    equal(legacy.configuration, migrated.configuration);

    // Native default INI has no profiles and round-trips unchanged.
    Config defaults;
    assert(ini_parse("../misc/default.ini", Config::ini_handler, &defaults) == 0);
    assert(defaults.configuration.settings.fake_hdr == 0.0f);
    assert(defaults.configuration.dimming_timeout == (Time{0, 5, 0}));

    Config malformed;
    parse(malformed, "[settings]\nfake_hdr = 5\nrange = 0.9-0.1\ndimming_timeout = 99:99\n");
    assert(malformed.configuration.settings.fake_hdr == 1.0f);
    assert(malformed.configuration.settings.range.lo == malformed.configuration.settings.range.hi);
    assert(malformed.configuration.dimming_timeout == (Time{0, 59, 59}));
    malformed.configuration.settings.fake_hdr = std::numeric_limits<float>::quiet_NaN();
    malformed.sanitize();
    assert(malformed.configuration.settings.fake_hdr == 0.0f);
    parse(malformed, "[settings]\nfake_hdr =\nrange = invalid\ndimming_timeout = invalid\n");

    assert(config.apply() == 0);
    assert(apply_count == 1);
    equal(config.configuration, live_configuration);
    Config another_client;
    live_active = false;
    assert(another_client.update() == 0);
    equal(config.configuration, another_client.configuration);
    assert(!another_client.active);
    assert(another_client.reset() == 0);
    assert(live_configuration.settings.fake_hdr == DEFAULT_FAKE_HDR);
    assert(live_configuration.settings.temperature == DEFAULT_TEMP);
    assert(live_configuration.dimming_timeout == (Time{0, 3, 24}));
    service_result = 123;
    assert(config.update() == 123 && config.apply() == 123);

    // Actual file save/read using test-local locations, not SD-card paths.
    Config::config_locations = {"build/settings.cfg", "build/fallback.cfg"};
    config.write();
    Config from_file;
    from_file.read();
    equal(config.configuration, from_file.configuration);
    assert(from_file.active == config.active);
    std::cout << "Single configuration, legacy migration, persistence and client tests passed\n";
}
