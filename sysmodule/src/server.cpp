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

#include <cstring>
#include <type_traits>
#include <utility>

#include <common.hpp>

#include "server.hpp"

namespace fz {

#define SET_OUTDATA(v) ({                               \
    *(std::remove_cvref_t<decltype(v)> *)out_data = v;  \
    *out_datasize = sizeof(v);                          \
})

Result Server::command_handler(void *userdata, const IpcServerRequest *r, u8 *out_data, size_t *out_datasize) {
    auto *self = static_cast<Server *>(userdata);

    switch (r->data.cmdId) {
        case FizeauCommandId_GetIsActive: {
            SET_OUTDATA(self->context.is_active);
            break;
        }
        case FizeauCommandId_SetIsActive: {
            if (r->data.size < sizeof(bool))
                return FIZEAU_MAKERESULT(INVALID_REQUEST);
            const auto active = *static_cast<const u8 *>(r->data.ptr) != 0;
            auto prev_active = std::exchange(self->context.is_active, active);

            if (prev_active != self->context.is_active)
                return self->manager.update_active();

            break;
        }
        case FizeauCommandId_GetConfiguration: {
            mutexLock(&self->context.configuration_mutex);
            FZ_SCOPEGUARD([self] { mutexUnlock(&self->context.configuration_mutex); });
            SET_OUTDATA(self->context.configuration);
            break;
        }
        case FizeauCommandId_SetConfiguration: {
            if (r->data.size < sizeof(FizeauConfiguration))
                return FIZEAU_MAKERESULT(INVALID_REQUEST);
            Config config;
            std::memcpy(&config.configuration, r->data.ptr, sizeof(config.configuration));
            config.sanitize();
            mutexLock(&self->context.configuration_mutex);
            self->context.configuration = config.configuration;
            mutexUnlock(&self->context.configuration_mutex);
            return self->manager.apply();
        }
        default:
            return MAKERESULT(10, 221);
    }

    return 0;
}

} // namespace fz
