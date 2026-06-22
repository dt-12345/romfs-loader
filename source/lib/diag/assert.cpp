#include "common.hpp"

#include "assert.hpp"
#include "abort.hpp"
#include "log_types.hpp"

#include <program/setting.hpp>
#include <cstdarg>
#include <cinttypes>
#include <memory>

namespace exl::diag {

    namespace {

        inline NORETURN void AbortWithCtx(const AbortCtx & ctx) {
            {
                /* We have no capability of chainloading payloads on mariko. */
                /* Don't have a great solution for this at the moment, just data abort. */
                /* TODO: maybe write to a file? custom fatal program? */

                /* Have lower 32-bits of the address be the lower 32-bits of the value. This ensures users can see (most of) the value even only given the bad address. */
                register u64 addr __asm__("x27") = (0x69696969ull << 32) | static_cast<u32>(ctx.m_Value);
                register u64 val __asm__("x28")  = ctx.m_Value;
                while (true) {
                    __asm__ __volatile__ (
                        "str %[val], [%[addr]]"
                        :
                        : [val]"r"(val), [addr]"r"(addr)
                    );
                }
            }

            UNREACHABLE;
        }

        AbortReason ToAbortReason(AssertionType type) {
            switch (type) {
                case AssertionType_Audit:  return AbortReason_Audit;
                case AssertionType_Assert: return AbortReason_Assert;
                default:
                    return AbortReason_Abort;
            }
        }

        void InvokeAssertionFailureHandler(const AssertionInfo &info) {
            /* TODO: support continuing from assertion. */
            const AbortInfo abort_info = {
                ToAbortReason(info.type),
                info.message,
                info.expr,
                info.func,
                info.file,
                info.line,
            };

            AbortWithCtx({ 0, abort_info });
        }
    }


    void AbortImpl(const char *expr, const char *func, const char *file, int line) {
        EXL_UNUSED(file, line, func, expr);

        /* Create abort info. */
        std::va_list vl{};
        const diag::LogMessage message = { "", std::addressof(vl) };

        const AbortInfo abort_info = {
            AbortReason_Abort,
            std::addressof(message),
            expr,
            func,
            file,
            line,
        };

        AbortWithCtx({ 0, abort_info });
    }

    void AbortImpl(const char *expr, const char *func, const char *file, int line, const char *format, ...) {
        EXL_UNUSED(file, line, func, expr, format);

        /* Create abort info. */
        std::va_list vl;
        va_start(vl, format);
        const diag::LogMessage message = { format, std::addressof(vl) };

        const AbortInfo abort_info = {
            AbortReason_Abort,
            std::addressof(message),
            expr,
            func,
            file,
            line,
        };

        AbortWithCtx({ 0, abort_info });
    }

    void AbortImpl(const char *expr, const char *func, const char *file, int line, const ::exl::Result *result, const char *format, ...) {
        EXL_UNUSED(file, line, func, expr, result, format);

        /* Create abort info. */
        std::va_list vl;
        va_start(vl, format);
        const diag::LogMessage message = { format, std::addressof(vl) };

        const AbortInfo abort_info = {
            AbortReason_Abort,
            std::addressof(message),
            expr,
            func,
            file,
            line,
        };

        AbortWithCtx({ *result, abort_info });
    }

    NOINLINE void OnAssertionFailure(AssertionType type, const char *expr, const char *func, const char *file, int line, const char *format, ...) {
        EXL_UNUSED(type, expr, func, file, line, format);

        /* Create the assertion info. */
        std::va_list vl;
        va_start(vl, format);

        const ::exl::diag::LogMessage message = { format, std::addressof(vl) };

        const AssertionInfo info = {
            type,
            std::addressof(message),
            expr,
            func,
            file,
            line,
        };

        InvokeAssertionFailureHandler(info);
        va_end(vl);
    }


    void OnAssertionFailure(AssertionType type, const char *expr, const char *func, const char *file, int line) {
        return OnAssertionFailure(type, expr, func, file, line, "");
    }
    extern "C" void exl_abort(Result result) {
        R_ABORT_UNLESS(result);
    }
}

namespace exl::impl {
    NORETURN NOINLINE void UnexpectedDefaultImpl(const char *func, const char *file, int line) {
        /* Create abort info. */
        std::va_list vl{};
        const ::exl::diag::LogMessage message = { "" , std::addressof(vl) };
        const ::exl::diag::AbortInfo abort_info = {
            ::exl::diag::AbortReason_UnexpectedDefault,
            std::addressof(message),
            "",
            func,
            file,
            line,
        };

        /* Abort. */
        diag::AbortWithCtx({ 0, abort_info });
    }
}
