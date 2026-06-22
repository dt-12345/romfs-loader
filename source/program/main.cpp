#include "loader.hpp"

#include "lib.hpp"

extern "C" {
    extern void nnMain();
}

HOOK_DEFINE_TRAMPOLINE(Initialize) {
    static void Callback() {
        InitializeLoader();
        Orig();
    }
};

extern "C" void exl_main(void* x0, void* x1) {
    exl::hook::Initialize();
    Initialize::InstallAtFuncPtr(nnMain);
}

extern "C" NORETURN void exl_exception_entry() {
    /* Note: this is only applicable in the context of applets/sysmodules. */
    EXL_ABORT("Default exception handler called!");
}