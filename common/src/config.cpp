// Copyright (c) 2024 averne
//
// This file is part of Fizeau.
//
// Fizeau is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// Fizeau is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Fizeau.  If not, see <http://www.gnu.org/licenses/>.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <ini.h>
#include <sys/stat.h>

#include "config.hpp"

namespace fz {

std::string_view Config::find_config() {
    struct stat tmp;
    for (auto loc: config_locations)
        if (::stat(loc.data(), &tmp) == 0)
            return loc;
    return config_locations[1];
}

void Config::read() {
    ini_parse(Config::find_config().data(), Config::ini_handler, this);
    this->sanitize();
}

std::string Config::make() {
    this->sanitize();
    const auto &c = this->configuration;
    const auto &s = c.settings;
    std::string str = std::string("active = ") + (this->active ? "true\n" : "false\n");
    str += "\n[settings]\n";
    str += "temperature = " + std::to_string(s.temperature) + '\n';
    str += "saturation = " + std::to_string(s.saturation) + '\n';
    str += "hue = " + std::to_string(s.hue) + '\n';
    str += "contrast = " + std::to_string(s.contrast) + '\n';
    str += "gamma = " + std::to_string(s.gamma) + '\n';
    str += "luminance = " + std::to_string(s.luminance) + '\n';
    str += "fake_hdr = " + std::to_string(s.fake_hdr) + '\n';
    str += "range = " + std::to_string(s.range.lo) + "-" + std::to_string(s.range.hi) + '\n';
    std::string components;
    if (c.components & Component_Red) components += 'r';
    if (c.components & Component_Green) components += 'g';
    if (c.components & Component_Blue) components += 'b';
    str += "components = " + (components.empty() ? "none" : components) + '\n';
    const char *filter = c.filter == Component_Red ? "red" : c.filter == Component_Green ? "green" :
        c.filter == Component_Blue ? "blue" : "none";
    str += std::string("filter = ") + filter + '\n';
    char timeout[16];
    std::snprintf(timeout, sizeof(timeout), "%02u:%02u", c.dimming_timeout.m, c.dimming_timeout.s);
    str += std::string("dimming_timeout = ") + timeout + '\n';
    return str;
}

void Config::write() {
    auto str = this->make();
    if (FILE *fp = std::fopen(Config::find_config().data(), "w")) {
        std::fwrite(str.data(), str.size(), 1, fp);
        std::fclose(fp);
    }
}

Result Config::update() {
    if (auto rc = fizeauGetIsActive(&this->active); R_FAILED(rc))
        return rc;
    return fizeauGetConfiguration(&this->configuration);
}

Result Config::apply() {
    this->sanitize();
    return fizeauSetConfiguration(&this->configuration);
}

Result Config::reset() {
    this->configuration.settings = Config::default_settings;
    this->configuration.components = Component_All;
    this->configuration.filter = Component_None;
    return this->apply();
}

} // namespace fz
