#pragma once

#include "nn/nn_common.hpp"
#include "os_reader_writer_lock_api.hpp"
#include "os_reader_writer_lock_type.hpp"

namespace nn::os {

    class ReaderWriterLock {
        NON_COPYABLE(ReaderWriterLock);
        NON_MOVEABLE(ReaderWriterLock);
        private:
            ReaderWriterLockType m_lock;
        public:
            constexpr ReaderWriterLock() { InitializeReaderWriterLock(std::addressof(m_lock)); }

            ~ReaderWriterLock() { FinalizeReaderWriterLock(std::addressof(m_lock)); }

            // for use with std::scoped_lock
            void lock() {
                AcquireWriteLock(std::addressof(m_lock));
            }

            bool try_lock() {
                return TryAcquireWriteLock(std::addressof(m_lock));
            }

            void unlock() {
                ReleaseWriteLock(std::addressof(m_lock));
            }

            // for use with std::shared_lock
            void lock_shared() {
                AcquireReadLock(std::addressof(m_lock));
            }

            bool try_lock_shared() {
                return TryAcquireReadLock(std::addressof(m_lock));
            }

            void unlock_shared() {
                ReleaseReadLock(std::addressof(m_lock));
            }

            ALWAYS_INLINE void Lock() {
                this->lock();
            }

            ALWAYS_INLINE bool TryLock() {
                return this->try_lock();
            }

            ALWAYS_INLINE void Unlock() {
                this->unlock();
            }

            ALWAYS_INLINE void LockShared() {
                this->lock_shared();
            }

            ALWAYS_INLINE bool TryLockShared() {
                return this->try_lock_shared();
            }

            ALWAYS_INLINE void UnlockShared() {
                this->unlock_shared();
            }

            operator ReaderWriterLockType &() {
                return m_lock;
            }

            operator const ReaderWriterLockType &() const {
                return m_lock;
            }

            ReaderWriterLockType *GetBase() {
                return std::addressof(m_lock);
            }
    };
}