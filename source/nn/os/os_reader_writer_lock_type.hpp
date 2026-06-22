#pragma once

#include "impl/os_internal_condition_variable.hpp"
#include "impl/os_internal_critical_section.hpp"

namespace nn::os {
    struct ThreadType;

    struct ReaderWriterLockType {
        detail::InternalCriticalSectionStorage critical_section;
        char _04[4];
        s32 lock_level;
        char _0c[0xc];
        ThreadType *owner_thread;
        char _20[4];
        detail::InternalConditionVariableStorage cv0;
        detail::InternalConditionVariableStorage cv1;
        char _2c[4];
    };
    static_assert(sizeof(ReaderWriterLockType) == 0x30);
}