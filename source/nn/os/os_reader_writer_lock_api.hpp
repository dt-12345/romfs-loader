#pragma once

namespace nn::os {

    struct ReaderWriterLockType;

    void InitializeReaderWriterLock(nn::os::ReaderWriterLockType* lock);
    void FinalizeReaderWriterLock(nn::os::ReaderWriterLockType* lock);

    void AcquireReadLock(nn::os::ReaderWriterLockType* lock);
    bool TryAcquireReadLock(nn::os::ReaderWriterLockType* lock);
    void ReleaseReadLock(nn::os::ReaderWriterLockType* lock);
    
    void AcquireWriteLock(nn::os::ReaderWriterLockType* lock);
    bool TryAcquireWriteLock(nn::os::ReaderWriterLockType* lock);
    void ReleaseWriteLock(nn::os::ReaderWriterLockType* lock);

    bool IsReadLockHeld(nn::os::ReaderWriterLockType const* lock);
    bool IsWriteLockHeldByCurrentThread(nn::os::ReaderWriterLockType const* lock);
    bool IsReaderWriterLockOwnerThread(nn::os::ReaderWriterLockType const* lock);
}