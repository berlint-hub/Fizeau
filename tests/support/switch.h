// Minimal host-only declarations for color/config parser tests; not a libnx emulator.
#pragma once
#include <stdint.h>
#define BIT(n) (1u << (n))
#define NX_CONSTEXPR constexpr
using Result = uint32_t;
struct Service;
#define R_FAILED(rc) ((rc) != 0)
#define R_SUCCEEDED(rc) ((rc) == 0)
