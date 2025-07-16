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

#ifndef IMGUIEMUTHREAD_H
#define IMGUIEMUTHREAD_H

#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <queue>
#include <string>
#include <variant>

class ImGuiEmuInstance;

class ImGuiEmuThread
{
public:
    explicit ImGuiEmuThread(ImGuiEmuInstance* inst);
    ~ImGuiEmuThread();

    // Thread control
    void start();
    void stop();
    void join();
    bool isRunning() const { return running; }

    enum MessageType
    {
        msg_Exit,
        msg_EmuRun,
        msg_EmuPause,
        msg_EmuUnpause,
        msg_EmuStop,
        msg_EmuFrameStep,
        msg_EmuReset,
        msg_BootROM,
        msg_BootFirmware,
        msg_InsertCart,
        msg_EjectCart,
        msg_LoadState,
        msg_SaveState,
        msg_UndoStateLoad,
        msg_ImportSavefile,
        msg_EnableCheats,
    };

    struct Message
    {
        MessageType type;
        std::variant<bool, std::string, std::vector<std::string>> param;
    };

    void sendMessage(Message msg);
    void sendMessage(MessageType type);
    void waitMessage(int num = 1);
    void waitAllMessages();

    void emuRun();
    void emuPause(bool broadcast = true);
    void emuUnpause(bool broadcast = true);
    void emuTogglePause(bool broadcast = true);
    void emuStop(bool external = false);
    void emuExit();
    void emuFrameStep();
    void emuReset();

    int bootROM(const std::vector<std::string>& filename, std::string& errorstr);
    int bootFirmware(std::string& errorstr);
    int insertCart(const std::vector<std::string>& filename, bool gba, std::string& errorstr);
    void ejectCart(bool gba);

    int saveState(const std::string& filename);
    int loadState(const std::string& filename);
    int undoStateLoad();

    int importSavefile(const std::string& filename);
    void enableCheats(bool enable);

    bool emuIsRunning() const { return emuStatus == emuStatus_Running; }
    bool emuIsActive() const { return emuActive; }

private:
    void run();
    void handleMessages();
    void handleEmulation();

    ImGuiEmuInstance* emuInstance;
    std::thread thread;
    std::atomic<bool> running;
    std::atomic<bool> emuActive;

    enum EmuStatusKind
    {
        emuStatus_Exit,
        emuStatus_Running,
        emuStatus_Paused,
        emuStatus_FrameStep,
    };

    EmuStatusKind prevEmuStatus;
    EmuStatusKind emuStatus;
    
    constexpr static int emuPauseStackRunning = 0;
    constexpr static int emuPauseStackPauseThreshold = 1;
    int emuPauseStack;

    std::mutex msgMutex;
    std::condition_variable msgCondition;
    std::queue<Message> msgQueue;
    
    int msgResult = 0;
    std::string msgError;
};

#endif // IMGUIEMUTHREAD_H 