#include "FileWatcher.h"

FileWatcher::FileWatcher() {
}

void FileWatcher::Start(const char * Path) {
    _Path = Path;
    FRunnableThread::Create(this, TEXT("FileWatcher"));
}

bool FileWatcher::Update(TArray<uint8> & Result) {
    if(!_Mutex.TryLock()) {
        return false;
    }

    if(!_PendingUpdate) {
        _Mutex.Unlock();
        return false;
    }

    Result = _Data;

    _PendingUpdate = false;

    _Mutex.Unlock();
    
    return true;
}

bool FileWatcher::Init() {
    return true;
}

uint32 FileWatcher::Run() {
    IFileManager& FileManager = IFileManager::Get();
    while(!_Stop) {
        _Mutex.Lock();

        FDateTime WriteTime = FileManager.GetTimeStamp(*_Path);

        if(WriteTime > _LastWriteTime) {
            _LastWriteTime = WriteTime;
            _PendingUpdate = FFileHelper::LoadFileToArray(_Data, *_Path, 0);
        }

        _Mutex.Unlock();

        FPlatformProcess::Sleep(0.5f);
    }
    return 0;
}

void FileWatcher::Exit() {
}

void FileWatcher::Stop() {
    _Stop = true;
}
