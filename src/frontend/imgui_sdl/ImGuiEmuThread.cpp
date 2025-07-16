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

#include "ImGuiEmuThread.h"
#include "ImGuiEmuInstance.h"
#include "../../NDS.h"
#include "../../DSi.h"
#include "../../Platform.h"
#include "../../net/LocalMP.h"
#include "../../net/LAN.h"

ImGuiEmuThread::ImGuiEmuThread(ImGuiEmuInstance* inst)
    : emuInstance(inst)
    , running(false)
    , emuActive(false)
    , emuStatus(emuStatus_Paused)
    , prevEmuStatus(emuStatus_Paused)
    , emuPauseStack(emuPauseStackRunning)
{
}

ImGuiEmuThread::~ImGuiEmuThread()
{
    stop();
    join();
}

void ImGuiEmuThread::start()
{
    if (!running) {
        running = true;
        thread = std::thread(&ImGuiEmuThread::run, this);
    }
}

void ImGuiEmuThread::stop()
{
    running = false;
    msgCondition.notify_all();
}

void ImGuiEmuThread::join()
{
    if (thread.joinable()) {
        thread.join();
    }
}

void ImGuiEmuThread::sendMessage(Message msg)
{
    std::lock_guard<std::mutex> lock(msgMutex);
    msgQueue.push(msg);
    msgCondition.notify_one();
}

void ImGuiEmuThread::sendMessage(MessageType type)
{
    sendMessage({.type = type});
}

void ImGuiEmuThread::waitMessage(int num)
{
    if (std::this_thread::get_id() == thread.get_id()) return;
    
    std::unique_lock<std::mutex> lock(msgMutex);
    for (int i = 0; i < num; i++) {
        msgCondition.wait(lock, [this] { return !msgQueue.empty(); });
    }
}

void ImGuiEmuThread::waitAllMessages()
{
    if (std::this_thread::get_id() == thread.get_id()) return;
    
    std::unique_lock<std::mutex> lock(msgMutex);
    while (!msgQueue.empty()) {
        msgCondition.wait(lock, [this] { return !msgQueue.empty(); });
    }
}

void ImGuiEmuThread::emuRun()
{
    sendMessage(msg_EmuRun);
    waitMessage();
}

void ImGuiEmuThread::emuPause(bool broadcast)
{
    sendMessage(msg_EmuPause);
    waitMessage();
}

void ImGuiEmuThread::emuUnpause(bool broadcast)
{
    sendMessage(msg_EmuUnpause);
    waitMessage();
}

void ImGuiEmuThread::emuTogglePause(bool broadcast)
{
    if (emuStatus == emuStatus_Paused)
        emuUnpause(broadcast);
    else
        emuPause(broadcast);
}

void ImGuiEmuThread::emuStop(bool external)
{
    sendMessage({.type = msg_EmuStop, .param = external});
    waitMessage();
}

void ImGuiEmuThread::emuExit()
{
    sendMessage(msg_Exit);
    waitAllMessages();
}

void ImGuiEmuThread::emuFrameStep()
{
    if (emuPauseStack < emuPauseStackPauseThreshold)
        sendMessage(msg_EmuPause);
    sendMessage(msg_EmuFrameStep);
    waitAllMessages();
}

void ImGuiEmuThread::emuReset()
{
    sendMessage(msg_EmuReset);
    waitMessage();
}

int ImGuiEmuThread::bootROM(const std::vector<std::string>& filename, std::string& errorstr)
{
    sendMessage({.type = msg_BootROM, .param = filename});
    waitMessage();
    errorstr = msgError;
    return msgResult;
}

int ImGuiEmuThread::bootFirmware(std::string& errorstr)
{
    sendMessage(msg_BootFirmware);
    waitMessage();
    errorstr = msgError;
    return msgResult;
}

int ImGuiEmuThread::insertCart(const std::vector<std::string>& filename, bool gba, std::string& errorstr)
{
    sendMessage({.type = msg_InsertCart, .param = filename});
    waitMessage();
    errorstr = msgError;
    return msgResult;
}

void ImGuiEmuThread::ejectCart(bool gba)
{
    sendMessage(msg_EjectCart);
    waitMessage();
}

int ImGuiEmuThread::saveState(const std::string& filename)
{
    sendMessage({.type = msg_SaveState, .param = filename});
    waitMessage();
    return msgResult;
}

int ImGuiEmuThread::loadState(const std::string& filename)
{
    sendMessage({.type = msg_LoadState, .param = filename});
    waitMessage();
    return msgResult;
}

int ImGuiEmuThread::undoStateLoad()
{
    sendMessage(msg_UndoStateLoad);
    waitMessage();
    return msgResult;
}

int ImGuiEmuThread::importSavefile(const std::string& filename)
{
    sendMessage({.type = msg_ImportSavefile, .param = filename});
    waitMessage();
    return msgResult;
}

void ImGuiEmuThread::enableCheats(bool enable)
{
    sendMessage({.type = msg_EnableCheats, .param = enable});
    waitMessage();
}

void ImGuiEmuThread::run()
{
    while (running) {
        melonDS::MPInterface::Get().Process();
        
        emuInstance->inputProcess();
        
        if (emuInstance->hotkeyPressed(HK_Pause)) emuTogglePause();
        if (emuInstance->hotkeyPressed(HK_Reset)) emuReset();
        if (emuInstance->hotkeyPressed(HK_FrameStep)) emuFrameStep();
        
        if (emuStatus == emuStatus_Running || emuStatus == emuStatus_FrameStep)
        {
            if (emuStatus == emuStatus_FrameStep) emuStatus = emuStatus_Paused;
            
            emuInstance->frameStep();
            
            melonDS::Platform::Sleep(16667); // ~60 FPS
        }
        else
        {
            melonDS::Platform::Sleep(75000);
        }
        
        handleMessages();
    }
}

void ImGuiEmuThread::handleMessages()
{
    std::lock_guard<std::mutex> lock(msgMutex);
    while (!msgQueue.empty())
    {
        Message msg = msgQueue.front();
        msgQueue.pop();
        
        switch (msg.type)
        {
        case msg_Exit:
            emuStatus = emuStatus_Exit;
            emuPauseStack = emuPauseStackRunning;
            emuInstance->audioDisable();
            break;

        case msg_EmuRun:
            emuStatus = emuStatus_Running;
            emuPauseStack = emuPauseStackRunning;
            emuActive = true;
            emuInstance->audioEnable();
            break;

        case msg_EmuPause:
            emuPauseStack++;
            if (emuPauseStack > emuPauseStackPauseThreshold) break;

            prevEmuStatus = emuStatus;
            emuStatus = emuStatus_Paused;

            if (prevEmuStatus != emuStatus_Paused)
            {
                emuInstance->audioDisable();
                emuInstance->osdAddMessage(0, "Paused");
            }
            break;

        case msg_EmuUnpause:
            if (emuPauseStack < emuPauseStackPauseThreshold) break;

            emuPauseStack--;
            if (emuPauseStack >= emuPauseStackPauseThreshold) break;

            emuStatus = prevEmuStatus;

            if (emuStatus != emuStatus_Paused)
            {
                emuInstance->audioEnable();
                emuInstance->osdAddMessage(0, "Resumed");
            }
            break;

        case msg_EmuStop:
            {
                bool external = std::get<bool>(msg.param);
                if (external)
                    emuInstance->stop();
                emuStatus = emuStatus_Paused;
                emuActive = false;
                emuInstance->audioDisable();
            }
            break;

        case msg_EmuFrameStep:
            emuStatus = emuStatus_FrameStep;
            break;

        case msg_EmuReset:
            emuInstance->reset();
            emuStatus = emuStatus_Running;
            emuPauseStack = emuPauseStackRunning;
            emuActive = true;
            emuInstance->audioEnable();
            emuInstance->osdAddMessage(0, "Reset");
            break;

        case msg_BootROM:
            {
                msgResult = 0;
                auto filename = std::get<std::vector<std::string>>(msg.param);
                if (!emuInstance->loadROM(filename, true, msgError))
                    break;
                emuInstance->start();
                msgResult = 1;
            }
            break;

        case msg_BootFirmware:
            msgResult = 0;
            if (!emuInstance->bootFirmware(msgError))
                break;
            emuInstance->start();
            msgResult = 1;
            break;

        case msg_InsertCart:
            {
                msgResult = 0;
                auto filename = std::get<std::vector<std::string>>(msg.param);
                if (!emuInstance->loadROM(filename, false, msgError))
                    break;
                msgResult = 1;
            }
            break;

        case msg_EjectCart:
            emuInstance->ejectCart();
            break;

        case msg_LoadState:
            {
                auto filename = std::get<std::string>(msg.param);
                msgResult = emuInstance->loadState(filename) ? 1 : 0;
            }
            break;

        case msg_SaveState:
            {
                auto filename = std::get<std::string>(msg.param);
                msgResult = emuInstance->saveState(filename) ? 1 : 0;
            }
            break;

        case msg_UndoStateLoad:
            emuInstance->undoStateLoad();
            msgResult = 1;
            break;

        case msg_ImportSavefile:
            {
                auto filename = std::get<std::string>(msg.param);
                msgResult = emuInstance->importSavefile(filename) ? 1 : 0;
            }
            break;

        case msg_EnableCheats:
            {
                bool enable = std::get<bool>(msg.param);
                emuInstance->enableCheats(enable);
            }
            break;
        }
        
        msgCondition.notify_one();
    }
}

void ImGuiEmuThread::handleEmulation()
{
    // noop
} 