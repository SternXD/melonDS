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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <algorithm>
#include <iostream>
#include <stdarg.h>
#include <atomic>

#include "../../Platform.h"
#include "../../types.h"
#include "../../net/LocalMP.h"
#include "../../net/LAN.h"
#include "ImGuiEmuInstance.h"

#if defined(__WIN32__) || defined(_WIN32)
#include <windows.h>
#include <shlobj.h>
#include <knownfolders.h>
#include <commdlg.h>
#include <direct.h>
#define mkdir(x, y) _mkdir(x)
#else
#include <sys/stat.h>
#include <sys/types.h>
#ifndef __MINGW32__
#include <pwd.h>
#include <unistd.h>
#endif
#endif

#include <SDL.h>

namespace melonDS
{
namespace Platform
{

static std::atomic<bool> gEmuShouldStop{false};

std::string GetExecutableDir()
{
#if defined(__WIN32__) || defined(_WIN32)
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    std::wstring wpath(path);
    std::string strpath(wpath.begin(), wpath.end());
    
    size_t pos = strpath.find_last_of("\\/");
    if (pos != std::string::npos)
        strpath = strpath.substr(0, pos);
    
    return strpath;
#else
    char path[1024];
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len != -1)
    {
        path[len] = '\0';
        std::string strpath(path);
        
        size_t pos = strpath.find_last_of('/');
        if (pos != std::string::npos)
            strpath = strpath.substr(0, pos);
        
        return strpath;
    }
    
    return ".";
#endif
}

std::string GetConfigDir()
{
#if defined(__WIN32__) || defined(_WIN32)
    // Always use executable directory for portable configuration
    std::string exeDir = GetExecutableDir();
    return exeDir;
#elif defined(__APPLE__)
    // Always use executable directory for portable configuration
    std::string exeDir = GetExecutableDir();
    return exeDir;
#else
    // Linux/Unix
    // Always use executable directory for portable configuration
    std::string exeDir = GetExecutableDir();
    return exeDir;
#endif
}

std::string GetDataDir()
{
    return GetConfigDir();
}

void WriteFile(const std::string& path, const void* data, size_t size)
{
    FILE* file = fopen(path.c_str(), "wb");
    if (!file)
    {
        printf("Error: Cannot write file %s\n", path.c_str());
        return;
    }
    
    fwrite(data, 1, size, file);
    fclose(file);
}

void* ReadFile(const std::string& path, size_t* size)
{
    FILE* file = fopen(path.c_str(), "rb");
    if (!file)
    {
        if (size) *size = 0;
        return nullptr;
    }
    
    fseek(file, 0, SEEK_END);
    size_t filesize = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    void* data = malloc(filesize);
    if (!data)
    {
        fclose(file);
        if (size) *size = 0;
        return nullptr;
    }
    
    size_t read = fread(data, 1, filesize, file);
    fclose(file);
    
    if (size) *size = read;
    return data;
}

bool FileExists(const std::string& path)
{
    FILE* file = fopen(path.c_str(), "rb");
    if (file)
    {
        fclose(file);
        return true;
    }
    return false;
}

std::string GetLocalFilePath(const std::string& filename)
{
    std::string configDir = GetConfigDir();
    if (configDir.empty())
        return filename;
        
#if defined(__WIN32__) || defined(_WIN32)
    return configDir + "\\" + filename;
#else
    return configDir + "/" + filename;
#endif
}

bool CheckFileWritable(const std::string& path)
{
    // Try to open the file for writing
    FILE* file = fopen(path.c_str(), "a");
    if (file)
    {
        fclose(file);
        return true;
    }
    
    // If file doesn't exist, try to open directory for writing
    std::string dir = path;
    size_t pos = dir.find_last_of("/\\");
    if (pos != std::string::npos)
    {
        dir = dir.substr(0, pos);
        
        // Try to create a temporary file in the directory
        std::string testFile = dir;
#if defined(__WIN32__) || defined(_WIN32)
        testFile += "\\melonDS_test_write.tmp";
#else
        testFile += "/melonDS_test_write.tmp";
#endif
        
        FILE* testHandle = fopen(testFile.c_str(), "w");
        if (testHandle)
        {
            fclose(testHandle);
            remove(testFile.c_str());  // Clean up test file
            return true;
        }
    }
    
    return false;
}

bool LocalFileExists(const std::string& path)
{
    return FileExists(path);
}

FileHandle* OpenFile(const std::string& path, FileMode mode)
{
    const char* modestr = nullptr;
    switch (mode)
    {
        case FileMode::Read:      modestr = "rb"; break;
        case FileMode::Write:     modestr = "wb"; break;
        case FileMode::ReadWrite: modestr = "r+b"; break;
        case FileMode::Append:    modestr = "ab"; break;
        case FileMode::ReadWriteExisting: modestr = "r+b"; break;
        default:                  return nullptr;
    }
    FILE* file = fopen(path.c_str(), modestr);
    return reinterpret_cast<FileHandle*>(file);
}

FileHandle* OpenLocalFile(const std::string& path, FileMode mode)
{
    return OpenFile(path, mode);
}

bool CloseFile(FileHandle* file)
{
    if (!file) return false;
    fclose(reinterpret_cast<FILE*>(file));
    return true;
}

bool IsEndOfFile(FileHandle* file)
{
    if (!file) return true;
    return feof(reinterpret_cast<FILE*>(file)) != 0;
}

bool FileReadLine(char* str, int count, FileHandle* file)
{
    if (!file) return false;
    return fgets(str, count, reinterpret_cast<FILE*>(file)) != nullptr;
}

bool FileSeek(FileHandle* file, s64 offset, FileSeekOrigin origin)
{
    if (!file) return false;
    
    int whence;
    switch (origin)
    {
        case FileSeekOrigin::Start:   whence = SEEK_SET; break;
        case FileSeekOrigin::Current: whence = SEEK_CUR; break;
        case FileSeekOrigin::End:     whence = SEEK_END; break;
        default:                      return false;
    }
    
    return fseek(reinterpret_cast<FILE*>(file), (long)offset, whence) == 0;
}

void FileRewind(FileHandle* file)
{
    if (file)
        rewind(reinterpret_cast<FILE*>(file));
}

u64 FileRead(void* data, u64 size, u64 count, FileHandle* file)
{
    if (!file) return 0;
    return fread(data, (size_t)size, (size_t)count, reinterpret_cast<FILE*>(file));
}

bool FileFlush(FileHandle* file)
{
    if (!file) return false;
    return fflush(reinterpret_cast<FILE*>(file)) == 0;
}

u64 FileWrite(const void* data, u64 size, u64 count, FileHandle* file)
{
    if (!file) return 0;
    return fwrite(data, (size_t)size, (size_t)count, reinterpret_cast<FILE*>(file));
}

u64 FileWriteFormatted(FileHandle* file, const char* fmt, ...)
{
    if (!file) return 0;
    
    va_list args;
    va_start(args, fmt);
    int result = vfprintf(reinterpret_cast<FILE*>(file), fmt, args);
    va_end(args);
    
    return result >= 0 ? result : 0;
}

u64 FileLength(FileHandle* file)
{
    if (!file) return 0;
    
    FILE* f = reinterpret_cast<FILE*>(file);
    long pos = ftell(f);
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, pos, SEEK_SET);
    
    return size >= 0 ? size : 0;
}

void Log(LogLevel level, const char* fmt, ...)
{
    const char* levelstr;
    switch (level)
    {
        case LogLevel::Debug:   levelstr = "DEBUG"; break;
        case LogLevel::Info:    levelstr = "INFO"; break;
        case LogLevel::Warn:    levelstr = "WARN"; break;
        case LogLevel::Error:   levelstr = "ERROR"; break;
        default:                levelstr = "UNKNOWN"; break;
    }
    
    printf("[%s] ", levelstr);
    
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

void Sleep(u64 usecs)
{
    SDL_Delay((u32)(usecs / 1000));
}

void Semaphore_Free(Semaphore* sema)
{
    if (sema)
        SDL_DestroySemaphore(reinterpret_cast<SDL_sem*>(sema));
}

Semaphore* Semaphore_Create()
{
    return reinterpret_cast<Semaphore*>(SDL_CreateSemaphore(0));
}

void Semaphore_Reset(Semaphore* sema)
{
    if (!sema) return;
    
    SDL_sem* sem = reinterpret_cast<SDL_sem*>(sema);
    while (SDL_SemTryWait(sem) == 0);
}

void Semaphore_Wait(Semaphore* sema)
{
    if (sema)
        SDL_SemWait(reinterpret_cast<SDL_sem*>(sema));
}

bool Semaphore_TryWait(Semaphore* sema, int timeout_ms)
{
    if (!sema) return false;
    
    if (timeout_ms == 0)
        return SDL_SemTryWait(reinterpret_cast<SDL_sem*>(sema)) == 0;
    
    // For non-zero timeout, we need to implement a timeout mechanism
    // SDL doesn't have a direct timeout function, so we'll use a simple approach
    Uint32 start = SDL_GetTicks();
    while (SDL_GetTicks() - start < static_cast<Uint32>(timeout_ms))
    {
        if (SDL_SemTryWait(reinterpret_cast<SDL_sem*>(sema)) == 0)
            return true;
        SDL_Delay(1); // Small delay to avoid busy waiting
    }
    return false;
}

void Semaphore_Post(Semaphore* sema, int count)
{
    if (sema)
    {
        for (int i = 0; i < count; i++)
            SDL_SemPost(reinterpret_cast<SDL_sem*>(sema));
    }
}

Thread* Thread_Create(std::function<void()> func)
{
    auto wrapper = [](void* arg) -> int
    {
        auto* function = reinterpret_cast<std::function<void()>*>(arg);
        (*function)();
        delete function;
        return 0;
    };
    
    auto* function = new std::function<void()>(func);
    SDL_Thread* thread = SDL_CreateThread(wrapper, "melonDS_thread", function);
    return static_cast<Thread*>(static_cast<void*>(thread));
}

void Thread_Free(Thread* thread)
{
    if (thread)
    {
        SDL_WaitThread(reinterpret_cast<SDL_Thread*>(thread), nullptr);
    }
}

void Thread_Wait(Thread* thread)
{
    if (thread)
    {
        SDL_WaitThread(reinterpret_cast<SDL_Thread*>(thread), nullptr);
    }
}

Mutex* Mutex_Create()
{
    SDL_mutex* mutex = SDL_CreateMutex();
    return static_cast<Mutex*>(static_cast<void*>(mutex));
}

void Mutex_Free(Mutex* mutex)
{
    if (mutex)
        SDL_DestroyMutex(reinterpret_cast<SDL_mutex*>(mutex));
}

void Mutex_Lock(Mutex* mutex)
{
    if (mutex)
        SDL_LockMutex(reinterpret_cast<SDL_mutex*>(mutex));
}

void Mutex_Unlock(Mutex* mutex)
{
    if (mutex)
        SDL_UnlockMutex(reinterpret_cast<SDL_mutex*>(mutex));
}

bool Mutex_TryLock(Mutex* mutex)
{
    if (!mutex) return false;
    return SDL_TryLockMutex(reinterpret_cast<SDL_mutex*>(mutex)) == 0;
}

void SignalStop(StopReason reason, void* userdata)
{
    // Signal that emulation should stop
    printf("[INFO] SignalStop called with reason %d\n", (int)reason);
    gEmuShouldStop = true;
}

bool EmuShouldStop()
{
    return gEmuShouldStop.load();
}

void ClearEmuShouldStop()
{
    gEmuShouldStop = false;
}

u64 GetMSCount()
{
    return SDL_GetTicks64();
}

u64 GetUSCount()
{
    return SDL_GetPerformanceCounter() * 1000000 / SDL_GetPerformanceFrequency();
}

// Save file functions
void WriteNDSSave(const u8* savedata, u32 savelen, u32 writeoffset, u32 writelen, void* userdata)
{
    // In a full implementation, this would write DS save data to file but for now, stub it out
    printf("[DEBUG] WriteNDSSave: offset=%d, len=%d\n", writeoffset, writelen);
}

void WriteGBASave(const u8* savedata, u32 savelen, u32 writeoffset, u32 writelen, void* userdata)
{
    // In a full implementation, this would write GBA save data to file but for now, stub it out
    printf("[DEBUG] WriteGBASave: offset=%d, len=%d\n", writeoffset, writelen);
}

void WriteFirmware(const Firmware& firmware, u32 writeoffset, u32 writelen, void* userdata)
{
    // In a full implementation, this would write firmware changes to file but for now, stub it out
    printf("[DEBUG] WriteFirmware: offset=%d, len=%d\n", writeoffset, writelen);
}

void WriteDateTime(int year, int month, int day, int hour, int minute, int second, void* userdata)
{
    // In a full implementation, this would sync the system time to the host
    printf("[DEBUG] WriteDateTime: %04d-%02d-%02d %02d:%02d:%02d\n", year, month, day, hour, minute, second);
}

// Multiplayer functions (stubbed for now)
void MP_Begin(void* userdata)
{
    printf("[DEBUG] MP_Begin called\n");
    int inst = ((ImGuiEmuInstance*)userdata)->getInstanceID();
    printf("[DEBUG] MP_Begin: instance ID = %d\n", inst);
    printf("[DEBUG] MP_Begin: MPInterface type = %d\n", (int)melonDS::MPInterface::GetType());
    melonDS::MPInterface::Get().Begin(inst);
    printf("[DEBUG] MP_Begin completed\n");
}

void MP_End(void* userdata)
{
    printf("[DEBUG] MP_End called\n");
    int inst = ((ImGuiEmuInstance*)userdata)->getInstanceID();
    printf("[DEBUG] MP_End: instance ID = %d\n", inst);
    melonDS::MPInterface::Get().End(inst);
    printf("[DEBUG] MP_End completed\n");
}

int MP_SendPacket(u8* data, int len, u64 timestamp, void* userdata)
{
    int inst = ((ImGuiEmuInstance*)userdata)->getInstanceID();
    return melonDS::MPInterface::Get().SendPacket(inst, data, len, timestamp);
}

int MP_RecvPacket(u8* data, u64* timestamp, void* userdata)
{
    int inst = ((ImGuiEmuInstance*)userdata)->getInstanceID();
    return melonDS::MPInterface::Get().RecvPacket(inst, data, timestamp);
}

int MP_SendCmd(u8* data, int len, u64 timestamp, void* userdata)
{
    int inst = ((ImGuiEmuInstance*)userdata)->getInstanceID();
    return melonDS::MPInterface::Get().SendCmd(inst, data, len, timestamp);
}

int MP_SendReply(u8* data, int len, u64 timestamp, u16 aid, void* userdata)
{
    int inst = ((ImGuiEmuInstance*)userdata)->getInstanceID();
    return melonDS::MPInterface::Get().SendReply(inst, data, len, timestamp, aid);
}

int MP_SendAck(u8* data, int len, u64 timestamp, void* userdata)
{
    int inst = ((ImGuiEmuInstance*)userdata)->getInstanceID();
    return melonDS::MPInterface::Get().SendAck(inst, data, len, timestamp);
}

int MP_RecvHostPacket(u8* data, u64* timestamp, void* userdata)
{
    int inst = ((ImGuiEmuInstance*)userdata)->getInstanceID();
    return melonDS::MPInterface::Get().RecvHostPacket(inst, data, timestamp);
}

u16 MP_RecvReplies(u8* data, u64 timestamp, u16 aidmask, void* userdata)
{
    int inst = ((ImGuiEmuInstance*)userdata)->getInstanceID();
    return melonDS::MPInterface::Get().RecvReplies(inst, data, timestamp, aidmask);
}

// Network functions (stubbed for now)
int Net_SendPacket(u8* data, int len, void* userdata) { return 0; }
int Net_RecvPacket(u8* data, void* userdata) { return 0; }

// Camera functions (stubbed for now)
void Camera_Start(int num, void* userdata) { }
void Camera_Stop(int num, void* userdata) { }
void Camera_CaptureFrame(int num, u32* frame, int width, int height, bool yuv, void* userdata) { }

bool Addon_KeyDown(KeyType type, void* userdata) { return false; }
void Addon_RumbleStart(u32 len, void* userdata) { }
void Addon_RumbleStop(void* userdata) { }
float Addon_MotionQuery(MotionQueryType type, void* userdata) { return 0.0f; }

DynamicLibrary* DynamicLibrary_Load(const char* lib)
{
    return (DynamicLibrary*) SDL_LoadObject(lib);
}

void DynamicLibrary_Unload(DynamicLibrary* lib)
{
    SDL_UnloadObject(lib);
}

void* DynamicLibrary_LoadFunction(DynamicLibrary* lib, const char* name)
{
    return SDL_LoadFunction(lib, name);
}

} // namespace Platform 
} // namespace melonDS 