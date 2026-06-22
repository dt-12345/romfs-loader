#pragma once

#include "common.hpp"

#define EXL_MODULE_NAME "exlaunch"

namespace exl::setting {
    /* How large the JIT area will be for hooks. */
    constexpr size_t JitSize = 0x4000;

    /* Sanity checks. */
    static_assert(ALIGN_UP(JitSize, PAGE_SIZE) == JitSize, "");
}
