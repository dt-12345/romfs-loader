/*
 * Copyright (c) Atmosphère-NX
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "fs/fs_types.hpp"
#include "intrusive_list.hpp"

#include "common.hpp"

#include "nn/fs.hpp"

#include <memory>

#define R_RETURN(res) return res;

#define R_SUCCEED() R_RETURN(0)

namespace nn::fs {

    Result ResultNullptrArgument() {
        return MAKERESULT(Module_Fs, 6063);
    }

    Result ResultInvalidArgument() {
        return MAKERESULT(Module_Fs, 6001);
    }

    Result ResultDirectoryUnobtainable() {
        return MAKERESULT(Module_Fs, 6006);
    }

    namespace fsa {

        class IDirectory {
            public:
                virtual ~IDirectory() {}
    
                Result Read(s64 *out_count, DirectoryEntry *out_entries, s64 max_entries) {
                    R_UNLESS(out_count != nullptr, fs::ResultNullptrArgument());
                    if (max_entries == 0) {
                        *out_count = 0;
                        R_SUCCEED();
                    }
                    R_UNLESS(out_entries != nullptr, fs::ResultNullptrArgument());
                    R_UNLESS(max_entries > 0,        fs::ResultInvalidArgument());
                    R_RETURN(this->DoRead(out_count, out_entries, max_entries));
                }
    
                Result GetEntryCount(s64 *out) {
                    R_UNLESS(out != nullptr, fs::ResultNullptrArgument());
                    R_RETURN(this->DoGetEntryCount(out));
                }
            private:
                virtual Result DoRead(s64 *out_count, DirectoryEntry *out_entries, s64 max_entries) = 0;
                virtual Result DoGetEntryCount(s64 *out) = 0;
        };

    } // namespace fsa

    namespace detail {
        void* Allocate(size_t);
        void Deallocate(void*, size_t);

        class Newable {
            public:
                static ALWAYS_INLINE void *operator new(size_t size) noexcept {
                    return Allocate(size);
                }

                static ALWAYS_INLINE void *operator new(size_t size, Newable *placement) noexcept {
                    EXL_UNUSED(size);
                    return placement;
                }

                static ALWAYS_INLINE void *operator new[](size_t size) noexcept {
                    return Allocate(size);
                }

                static ALWAYS_INLINE void operator delete(void *ptr, size_t size) noexcept {
                    return Deallocate(ptr, size);
                }

                static ALWAYS_INLINE void operator delete[](void *ptr, size_t size) noexcept {
                    return Deallocate(ptr, size);
                }
        };

        class FileSystemAccessor;

        Result FindFileSystem(FileSystemAccessor**, const char**, const char*);

        class DirectoryAccessor : public util::IntrusiveListBaseNode<DirectoryAccessor>, public Newable {
            NON_COPYABLE(DirectoryAccessor);
            private:
                std::unique_ptr<fsa::IDirectory> m_impl;
                FileSystemAccessor &m_parent;
            public:
                DirectoryAccessor(std::unique_ptr<fsa::IDirectory>&& d, FileSystemAccessor &p) : m_impl(std::move(d)), m_parent(p) {}
                ~DirectoryAccessor();

                Result Read(s64 *out_count, DirectoryEntry *out_entries, s64 max_entries) {
                    R_RETURN(m_impl->Read(out_count, out_entries, max_entries));
                }
                Result GetEntryCount(s64 *out) {
                    R_RETURN(m_impl->GetEntryCount(out));
                }

                FileSystemAccessor *GetParent() const { return std::addressof(m_parent); }
        };

    } // namespace detail

} // namespace nn::fs

[[gnu::always_inline]] inline nn::fs::detail::DirectoryAccessor* Get(nn::fs::DirectoryHandle handle) {
    return reinterpret_cast<nn::fs::detail::DirectoryAccessor*>(handle._internal);
}

class MergedDirectory : public nn::fs::fsa::IDirectory, public nn::fs::detail::Newable {
public:
    MergedDirectory(nn::fs::DirectoryHandle handle1, nn::fs::DirectoryHandle handle2) : mHandle1(handle1), mHandle2(handle2) {}
    ~MergedDirectory() override {}

private:
    Result DoRead(s64 *out_count, nn::fs::DirectoryEntry *out_entries, s64 max_entries) override {
        nn::fs::DirectoryEntry* entries = out_entries;
        s64 remaining = max_entries;
        s64 count1, count2;
        R_TRY(nn::fs::ReadDirectory(&count1, entries, mHandle1, remaining));
        remaining = count1 >= remaining ? 0 : remaining - count1;
        entries += count1;
        if (remaining <= 0) {
            *out_count = max_entries;
            R_SUCCEED();
        }
        R_TRY(nn::fs::ReadDirectory(&count2, entries, mHandle2, remaining));
        remaining = count2 >= remaining ? 0 : remaining - count2;
        entries += count2;
        *out_count = max_entries;
        R_SUCCEED();
    }

    Result DoGetEntryCount(s64 *out) override {
        s64 count = 0;
        s64 count1, count2;
        R_TRY(nn::fs::GetDirectoryEntryCount(&count1, mHandle1));
        count += count1;
        R_TRY(nn::fs::GetDirectoryEntryCount(&count2, mHandle2));
        count += count2;
        *out = count;
        R_SUCCEED();
    }

    nn::fs::DirectoryHandle mHandle1;
    nn::fs::DirectoryHandle mHandle2;
};