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

#include "ImGuiSaveManager.h"
#include "../../Platform.h"
#include <ctime>

ImGuiSaveManager::ImGuiSaveManager(const std::string& path)
    : Path(path)
    , Running(false)
    , Length(0)
    , FlushRequested(false)
    , SecondaryBufferLength(0)
    , TimeAtLastFlushRequest(0)
    , PreviousFlushVersion(0)
    , FlushVersion(0)
{
    Running = true;
    thread = std::thread(&ImGuiSaveManager::run, this);
}

ImGuiSaveManager::~ImGuiSaveManager()
{
    Running = false;
    cv.notify_all();
    if (thread.joinable()) {
        thread.join();
    }
}

std::string ImGuiSaveManager::GetPath()
{
    return Path;
}

void ImGuiSaveManager::SetPath(const std::string& path, bool reload)
{
    Path = path;
    if (reload) {
        // noop
    }
}

void ImGuiSaveManager::RequestFlush(const melonDS::u8* savedata, melonDS::u32 savelen, melonDS::u32 writeoffset, melonDS::u32 writelen)
{
    std::lock_guard<std::mutex> lock(SecondaryBufferLock);
    
    if (savelen > Length) {
        Buffer = std::make_unique<melonDS::u8[]>(savelen);
        Length = savelen;
    }
    
    if (savelen > SecondaryBufferLength) {
        SecondaryBuffer = std::make_unique<melonDS::u8[]>(savelen);
        SecondaryBufferLength = savelen;
    }
    
    // Copy the entire save data
    memcpy(Buffer.get(), savedata, savelen);
    
    // Mark the modified region
    memcpy(SecondaryBuffer.get() + writeoffset, savedata + writeoffset, writelen);
    
    FlushRequested = true;
    TimeAtLastFlushRequest = time(nullptr);
    FlushVersion++;
    
    cv.notify_one();
}

void ImGuiSaveManager::CheckFlush()
{
    if (NeedsFlush()) {
        flush();
    }
}

bool ImGuiSaveManager::NeedsFlush()
{
    return FlushRequested;
}

void ImGuiSaveManager::FlushSecondaryBuffer(melonDS::u8* dst, melonDS::u32 dstLength)
{
    std::lock_guard<std::mutex> lock(SecondaryBufferLock);
    
    if (dst && dstLength >= SecondaryBufferLength) {
        memcpy(dst, SecondaryBuffer.get(), SecondaryBufferLength);
    }
}

void ImGuiSaveManager::run()
{
    while (Running) {
        std::unique_lock<std::mutex> lock(SecondaryBufferLock);
        // Wait for either a flush request or a timeout (100ms)
        cv.wait_for(lock, std::chrono::milliseconds(100), [this] { return !Running || FlushRequested; });
        if (!Running) break;

        if (FlushRequested) {
            // Debounce: wait for 2 seconds of inactivity after the last flush request
            time_t now = time(nullptr);
            if (TimeAtLastFlushRequest == 0 || difftime(now, TimeAtLastFlushRequest) < 2) {
                // Not enough time has passed, continue loop
                continue;
            }
            lock.unlock();
            flush();
        }
    }
}

void ImGuiSaveManager::flush()
{
    if (Path.empty()) return;
    
    auto file = melonDS::Platform::OpenFile(Path, melonDS::Platform::FileMode::Write);
    if (!file) return;
    
    std::lock_guard<std::mutex> lock(SecondaryBufferLock);
    
    melonDS::Platform::FileWrite(Buffer.get(), 1, Length, file);
    melonDS::Platform::CloseFile(file);
    
    FlushRequested = false;
    PreviousFlushVersion = FlushVersion;
} 