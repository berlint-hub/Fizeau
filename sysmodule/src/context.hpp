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

#pragma once

#include <array>

#include <common.hpp>

#include "nvdisp.hpp"

namespace fz {

struct Context {
    bool is_lite = false, is_active = true;
    Mutex configuration_mutex = {};
    FizeauConfiguration configuration = {
        .settings = Config::default_settings,
        .components = Component_All,
        .filter = Component_None,
    };

    DisplayController::CmuShadow cmu_shadow_internal = {}, cmu_shadow_external = {};
};

} // namespace fz
