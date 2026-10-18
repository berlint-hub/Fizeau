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

#include <cmath>
#include <chrono>
#include <switch.h>

#include <common.hpp>
#include <omm.h>

#include "t210_regs.hpp"
#include "nvdisp.hpp"

#include "display_manager.hpp"

using namespace std::chrono_literals;

namespace fz {

namespace {

constexpr std::uint32_t ins_evt_id = 0;

} // namespace

void DisplayManager::monitor_thread_func(void *args) {
    auto *self = static_cast<DisplayManager *>(args);

    UTimer timer;
    utimerCreate(&timer, std::chrono::nanoseconds(100ms).count(), TimerType_Repeating);
    utimerStart(&timer);

    while (true) {
        int idx;
        auto rc = waitMulti(&idx, UINT64_MAX,
            waiterForUTimer(&timer),
            waiterForUEvent(&self->thread_exit_event));
        if (R_FAILED(rc))
            return;

        switch (idx) {
            case 0:
                break;
            case 1:
            default:
                return;
        }

        if (!self->context.is_active)
            continue;

        bool need_apply = false, is_handheld = self->operation_mode == AppletOperationMode_Handheld;

        // CMU resets
        if (!need_apply) {
            // Check the clock for the specific head we are about to poll: reading the
            // registers of a clock-gated module stalls the bus, hanging the core (#104)
            if (!(READ(self->clock_va_base + CLK_RST_CONTROLLER_CLK_OUT_ENB_L) & (is_handheld ? CLK_ENB_DISP1 : CLK_ENB_DISP2)) ||
                    !mutexTryLock(&self->commit_mutex))
                goto cmu_end;

            FZ_SCOPEGUARD([self] { mutexUnlock(&self->commit_mutex); });

            // Poll DISPLAY_A in handheld mode, DISPLAY_B in docked mode
            std::uint64_t iobase = self->disp_va_base + (is_handheld ? 0 : 0x40000);

            auto &shadow = is_handheld ? self->context.cmu_shadow_internal : self->context.cmu_shadow_external;
            auto &csc    = shadow.csc;

            // There is a race when waking from reset, where the configuration
            // sometimes gets applied before nvdrv internally disables the CMU
            if (!(READ(iobase + DC_DISP_DISP_COLOR_CONTROL) & CMU_ENABLE)) {
                need_apply = true;
                goto cmu_end;
            }

            for (std::size_t i = 0; i < csc.size(); ++i) {
                if (csc[i] != READ(iobase + DC_COM_CMU_CSC_KRR + i * sizeof(std::uint32_t))) {
                    need_apply = true;
                    goto cmu_end;
                }
            }
        }

cmu_end:
        // Dimming
        if (!need_apply) {
            mutexLock(&self->context.configuration_mutex);
            auto timeout = to_timestamp(self->context.configuration.dimming_timeout);
            mutexUnlock(&self->context.configuration_mutex);
            auto delta = armTicksToNs(armGetSystemTick() - self->activity_tick) / std::chrono::nanoseconds(1s).count();

            if (timeout && (
                (!self->is_dimming && delta >= timeout) ||
                ( self->is_dimming && delta <  timeout)
            ))
                need_apply = true;
        }

        if (need_apply)
            self->apply();
    }
}

void DisplayManager::event_monitor_thread_func(void *args) {
    auto *self = static_cast<DisplayManager *>(args);

    while (true) {
        int idx;
        auto rc = waitMulti(&idx, UINT64_MAX,
            waiterForEvent(&self->operation_mode_event),
            waiterForEvent(&self->activity_event),
            waiterForUEvent(&self->thread_exit_event));
        if (R_FAILED(rc))
            return;

        switch (idx) {
            case 0: {
                ommGetOperationMode(&self->operation_mode);
                break;
            }
            case 1: {
                insrGetLastTick(ins_evt_id, &self->activity_tick);
                break;
            }
            case 2:
            default:
                return;
        }
    }
}

Result DisplayManager::initialize() {
    std::uint64_t size;
    if (auto rc = svcQueryMemoryMapping(&this->clock_va_base, &size, CLOCK_IO_BASE, CLOCK_IO_SIZE); R_FAILED(rc))
        diagAbortWithResult(rc);

    if (auto rc = svcQueryMemoryMapping(&this->disp_va_base, &size, DISP_IO_BASE, DISP_IO_SIZE); R_FAILED(rc))
        diagAbortWithResult(rc);

    if (auto rc = ommGetOperationModeChangeEvent(&this->operation_mode_event, false); R_FAILED(rc))
        diagAbortWithResult(rc);

    if (auto rc = ommGetOperationMode(&this->operation_mode); R_FAILED(rc))
        diagAbortWithResult(rc);

    if (auto rc = insrGetReadableEvent(ins_evt_id, &this->activity_event); R_FAILED(rc))
        diagAbortWithResult(rc);

    if (auto rc = insrGetLastTick(ins_evt_id, &this->activity_tick); R_FAILED(rc))
        diagAbortWithResult(rc);

    ueventCreate(&this->thread_exit_event, false);

    // The event monitor thread should have a higher priority than the display monitor thread to ensure it wins on mutex races
    if (auto rc = threadCreate(&this->event_monitor_thread, &DisplayManager::event_monitor_thread_func, this,
            this->event_monitor_thread_stack, sizeof(this->event_monitor_thread_stack), 0x3d, -2); R_FAILED(rc))
        diagAbortWithResult(rc);

    if (auto rc = threadStart(&this->event_monitor_thread); R_FAILED(rc))
        diagAbortWithResult(rc);

    if (auto rc = threadCreate(&this->monitor_thread, &DisplayManager::monitor_thread_func, this,
            this->monitor_thread_stack, sizeof(this->monitor_thread_stack), 0x3e, -2); R_FAILED(rc))
        diagAbortWithResult(rc);

    if (auto rc = threadStart(&this->monitor_thread); R_FAILED(rc))
        diagAbortWithResult(rc);

    return 0;
}

Result DisplayManager::finalize() {
    ueventSignal(&this->thread_exit_event);

    threadWaitForExit(&this->event_monitor_thread);
    threadWaitForExit(&this->monitor_thread);
    threadClose(&this->event_monitor_thread);
    threadClose(&this->monitor_thread);

    eventClose(&this->operation_mode_event);
    eventClose(&this->activity_event);

    return 0;
}

Result DisplayManager::apply() {
    if (!this->context.is_active)
        return 0;

    mutexLock(&this->commit_mutex);
    FZ_SCOPEGUARD([this] { mutexUnlock(&this->commit_mutex); });

    mutexLock(&this->context.configuration_mutex);
    const auto configuration = this->context.configuration;
    mutexUnlock(&this->context.configuration_mutex);
    auto timeout = armTicksToNs(armGetSystemTick() - this->activity_tick) / 1'000'000'000;
    auto dim_timeout = to_timestamp(configuration.dimming_timeout);
    this->is_dimming = dim_timeout && timeout >= dim_timeout;

    auto apply_settings = [this, &configuration](bool external) -> Result {
        auto settings = configuration.settings;
        if (this->is_dimming)
            settings.luminance = external ? dimmed_luma_external : dimmed_luma_internal;
        auto &shadow = external ? this->context.cmu_shadow_external : this->context.cmu_shadow_internal;
        if (auto rc = this->disp.apply_color_profile(external, settings,
                configuration.components, configuration.filter, shadow); R_FAILED(rc))
            return rc;
        return this->disp.set_hdmi_color_range(external, settings.range);
    };

    if (auto rc = apply_settings(false); R_FAILED(rc))
        return rc;
    if (!this->context.is_lite)
        if (auto rc = apply_settings(true); R_FAILED(rc))
            return rc;

    return 0;
}

Result DisplayManager::update_active() {
    if (this->context.is_active) {
        return this->apply();
    } else {
        if (auto rc = this->disp.disable(false); R_FAILED(rc))
            return rc;

        if (!this->context.is_lite) {
            if (auto rc = this->disp.disable(true); R_FAILED(rc))
                return rc;
        }
    }

    return 0;
}

} // namespace fz
