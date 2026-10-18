#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <cstdint>

#include "config.hpp"

namespace fz {

namespace {

// The fz::Config::ini_handler function is compiled separately
//   so that exception handling code doesn't get pulled in the object file
// Likewise, we implement minimal versions of atoi, atof and std::string_view::substr
//   to avoid pulling in needless bloat in the sysmodule executable (vfiprintf, exceptions, etc)

constexpr std::string_view substr(std::string_view s, std::size_t pos, std::size_t n = std::string_view::npos) {
    return std::string_view(s.data() + pos, std::min(n, s.size() - pos));
}

constexpr int ctoi(char c) {
    auto res = c - '0';
    return (res >= 0 && res <= 9) ? res : 0;
}

constexpr int atoi(std::string_view s) {
    if (s.empty()) return 0;
    int mult = 1, res = 0;
    switch (s.front()) {
        case '-':
            mult = -1;
        case '+':
            s = substr(s, 1);
            break;
    }

    for (auto c: s)
        res = res * 10 + ctoi(c);

    return mult * res;
}

static_assert(atoi("0") == 0);
static_assert(atoi("+10") == 10);
static_assert(atoi("-0099") == -99);

constexpr double atof(std::string_view s) {
    if (s.empty()) return 0.0;
    double mult = 1, res = 0, decimal = 1;
    switch (s.front()) {
        case '-':
            mult = -1;
        case '+':
            s = substr(s, 1);
            break;
    }

    for (auto c: s) {
        if (c == '.')
            decimal = 10;
        else {
            res = res * (decimal > 1 ? 1 : 10) + ctoi(c) / decimal;
            if (decimal > 1)
                decimal *= 10;
        }
    }

    return mult * res;
}

static_assert(atof("0") == 0.0);
static_assert(atof("2.4") == 2.4);
static_assert(atof("+011.11") == 11.11);
static_assert(atof("-00333.444") == -333.444);

} // namespace

#define MATCH(s1, s2)     (std::strcmp(s1, s2) == 0)
#define MATCH_ENTRY(s, n) (MATCH(section, (s)) && MATCH(name, (n)))

int Config::ini_handler(void *user, const char *section, const char *name, const char *value) {
    Config *config = static_cast<Config *>(user);
    std::string_view v = value;

    auto parse_components = [](const std::string_view &str) -> Component {
        if (strcasecmp(str.data(), "none") == 0) {
            return Component_None;
        } else if (strcasecmp(str.data(), "all") == 0) {
            return Component_All;
        } else {
            std::uint32_t comp = 0;
            if ((str.find('r') != std::string_view::npos) || (str.find('R') != std::string_view::npos)) comp |= Component_Red;
            if ((str.find('g') != std::string_view::npos) || (str.find('G') != std::string_view::npos)) comp |= Component_Green;
            if ((str.find('b') != std::string_view::npos) || (str.find('B') != std::string_view::npos)) comp |= Component_Blue;
            return static_cast<Component>(comp);
        }
    };

    auto parse_filter = [](const std::string_view &str) -> Component {
        if (strcasecmp(str.data(), "red") == 0)
            return Component_Red;
        else if (strcasecmp(str.data(), "green") == 0)
            return Component_Green;
        else if (strcasecmp(str.data(), "blue") == 0)
            return Component_Blue;
        return Component_None;
    };

    auto parse_time = [](const std::string_view &str) -> Time {
        Time t = {};
        auto pos = str.find(':');
        t.h = atoi(substr(str, 0, pos));
        t.m = atoi(substr(str, pos + 1));
        return t;
    };

    static_assert(parse_time("09:02") == Time{9, 2});

    auto parse_range = [](const std::string_view &str) -> ColorRange {
        ColorRange r = {};
        auto pos = str.find('-');
        r.lo = atof(substr(str, 0, pos));
        r.hi = atof(substr(str, pos + 1));
        return r;
    };

    static_assert(parse_range("0.18-0.92") == ColorRange{0.18, 0.92});

    if (MATCH_ENTRY("", "active")) {
        config->active = MATCH(value, "1") || strcasecmp(value, "true") == 0;
        return 1;
    }

    // One-time legacy import: only daytime values from profile1 are used.
    // All other profiles, night values, selectors and schedules are ignored.
    std::string_view key = name;
    const bool legacy = MATCH(section, "profile1");
    if (!legacy && !MATCH(section, "settings") && !MATCH(section, ""))
        return 1;
    if (legacy && key.ends_with("_day"))
        key.remove_suffix(4);
    else if (legacy && key.ends_with("_night"))
        return 1;
    if (v.empty())
        return 1;

    auto &c = config->configuration;
    auto &s = c.settings;
    if (key == "temperature") s.temperature = atoi(v);
    else if (key == "saturation") s.saturation = atof(v);
    else if (key == "hue") s.hue = atof(v);
    else if (key == "contrast") s.contrast = atof(v);
    else if (key == "gamma") s.gamma = atof(v);
    else if (key == "luminance") s.luminance = atof(v);
    else if (key == "fake_hdr") s.fake_hdr = atof(v);
    else if (key == "components") c.components = parse_components(v);
    else if (key == "filter") c.filter = parse_filter(v);
    else if (key == "range" && v.find('-') != std::string_view::npos) s.range = parse_range(v);
    else if (key == "dimming_timeout" && v.find(':') != std::string_view::npos) {
        auto t = parse_time(v);
        c.dimming_timeout = { 0, t.h, t.m };
    }
    return 1;
}

void Config::sanitize() {
    auto &s = this->configuration.settings;
    auto finite_clamp = [](float v, float lo, float hi, float fallback) {
        return std::isfinite(v) ? std::clamp(v, lo, hi) : fallback;
    };
    s.temperature = std::clamp(s.temperature, MIN_TEMP, MAX_TEMP);
    s.saturation = finite_clamp(s.saturation, MIN_SAT, MAX_SAT, DEFAULT_SAT);
    s.hue = finite_clamp(s.hue, MIN_HUE, MAX_HUE, DEFAULT_HUE);
    s.contrast = finite_clamp(s.contrast, MIN_CONTRAST, MAX_CONTRAST, DEFAULT_CONTRAST);
    s.gamma = finite_clamp(s.gamma, MIN_GAMMA, MAX_GAMMA, DEFAULT_GAMMA);
    s.luminance = finite_clamp(s.luminance, MIN_LUMA, MAX_LUMA, DEFAULT_LUMA);
    s.fake_hdr = finite_clamp(s.fake_hdr, MIN_FAKE_HDR, MAX_FAKE_HDR, DEFAULT_FAKE_HDR);
    s.range.lo = finite_clamp(s.range.lo, MIN_RANGE, MAX_RANGE, MIN_RANGE);
    s.range.hi = finite_clamp(s.range.hi, s.range.lo, MAX_RANGE, MAX_RANGE);
    auto &c = this->configuration;
    c.components = static_cast<Component>(static_cast<unsigned>(c.components) & Component_All);
    if (c.filter != Component_Red && c.filter != Component_Green && c.filter != Component_Blue)
        c.filter = Component_None;
    c.dimming_timeout.h = 0;
    c.dimming_timeout.m = std::min<unsigned>(c.dimming_timeout.m, 59);
    c.dimming_timeout.s = std::min<unsigned>(c.dimming_timeout.s, 59);
}

} // namespace fz
