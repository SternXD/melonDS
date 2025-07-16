/*
    Copyright 2016-2025 melonDS team

    This file is part of melonDS.

    melonDS is free software: you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    melonDS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with melonDS. If not, see http://www.gnu.org/licenses/.
*/

#ifndef IMGUISAVEMANAGER_H
#define IMGUISAVEMANAGER_H

#include <string>
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <condition_variable>

#include "../../types.h"

class ImGuiSaveManager
{
public:
    ImGuiSaveManager(const std::string& path);
    ~ImGuiSaveManager();

    std::string GetPath();
    void SetPath(const std::string& path, bool reload);

    void RequestFlush(const melonDS::u8* savedata, melonDS::u32 savelen, melonDS::u32 writeoffset, melonDS::u32 writelen);
    void CheckFlush();

    bool NeedsFlush();
    void FlushSecondaryBuffer(melonDS::u8* dst = nullptr, melonDS::u32 dstLength = 0);

private:
    void run();
    void flush();

    std::string Path;

    std::atomic_bool Running;
    std::thread thread;

    std::unique_ptr<melonDS::u8[]> Buffer;
    melonDS::u32 Length;
    bool FlushRequested;

    std::mutex SecondaryBufferLock;
    std::unique_ptr<melonDS::u8[]> SecondaryBuffer;
    melonDS::u32 SecondaryBufferLength;

    time_t TimeAtLastFlushRequest;

    // We keep versions in case the user closes the application before
    // a flush cycle is finished.
    melonDS::u32 PreviousFlushVersion;
    melonDS::u32 FlushVersion;

    std::condition_variable cv;
};

#endif // IMGUISAVEMANAGER_H 