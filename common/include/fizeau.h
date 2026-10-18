/**
 * Copyright (c) 2024 averne
 *
 * This file is part of Fizeau.
 *
 * Fizeau is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * Fizeau is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Fizeau.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _FZ_IPC_H
#define _FZ_IPC_H

#include <stdint.h>
#include <switch.h>

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FizeauCommandId_GetIsActive = 0,
    FizeauCommandId_SetIsActive = 1,
    // IDs 2-9 are retired profile/older configuration commands. Never reuse them.
    FizeauCommandId_GetConfiguration = 10,
    FizeauCommandId_SetConfiguration = 11,
} FizeauCommandId;

#define FIZEAU_RC_MODULE R_MODULE(0xf12)
#define FIZEAU_RC_INVALID_REQUEST 2
#define FIZEAU_MAKERESULT(r) MAKERESULT(FIZEAU_RC_MODULE, FIZEAU_RC_ ## r)

typedef struct {
    Temperature temperature;
    Saturation  saturation;
    Hue         hue;
    Contrast    contrast;
    Gamma       gamma;
    Luminance   luminance;
    ColorRange  range;
    FakeHdr     fake_hdr;
    ShadowGamma shadow_gamma;
} FizeauSettings;

typedef struct {
    FizeauSettings settings;
    Component components;
    Component filter;
    Time dimming_timeout;
} FizeauConfiguration;

Result fizeauIsServiceActive(bool *out);
Result fizeauInitialize();
void fizeauExit();

Service *fizeauGetServiceSession();

Result fizeauGetIsActive(bool *is_active);
Result fizeauSetIsActive(bool is_active);

Result fizeauGetConfiguration(FizeauConfiguration *configuration);
Result fizeauSetConfiguration(const FizeauConfiguration *configuration);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // _FZ_IPC_H
