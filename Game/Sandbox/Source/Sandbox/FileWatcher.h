//// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

class FileWatcher : public FRunnable {
public:
    FileWatcher();
    bool Update(TArray<uint8> & Result);
    void Start(const char * Path);
    virtual bool Init() override;
    virtual uint32 Run() override;
    virtual void Exit() override;
    virtual void Stop() override;

private:
    FString _Path;
    FDateTime _LastWriteTime;
    bool _Stop = false;
    bool _PendingUpdate = false;
    FPThreadsCriticalSection _Mutex;
    TArray<uint8> _Data;
};
