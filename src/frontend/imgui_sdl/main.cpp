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

#include <iostream>
#include <memory>
#include <fstream>
#include <SDL.h>

#include "ImGuiFrontend.h"
#include "ImGuiEmuInstance.h"
#include "../qt_sdl/Config.h"
#include "../../net/LocalMP.h"
#include "../../net/LAN.h"
#include "../../net/Net.h"
#include "../../net/Net_PCap.h"
#include "../../net/Net_Slirp.h"
#include "../../types.h"

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#endif

int melonDSMain(int argc, char** argv);

#ifdef _WIN32
// Windows entry point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    int argc = 0;
    char** argv = nullptr;
    
    const char* emptyArgs[] = { "melonDS-imgui" };
    argc = 1;
    argv = const_cast<char**>(emptyArgs);
    
    return melonDSMain(argc, argv);
}
#endif

melonDS::Net net;

void NetInit()
{
    Config::Table cfg = Config::GetGlobalTable();
    if (cfg.GetBool("LAN.DirectMode"))
    {
        static std::optional<melonDS::LibPCap> pcap;
        if (!pcap)
            pcap = melonDS::LibPCap::New();
        if (pcap)
        {
            std::string devicename = cfg.GetString("LAN.Device");
            std::unique_ptr<melonDS::Net_PCap> netPcap = pcap->Open(devicename, [](const unsigned char* data, int len) {
                net.RXEnqueue(data, len);
            });
            if (netPcap)
            {
                net.SetDriver(std::move(netPcap));
            }
        }
    }
    else
    {
        net.SetDriver(std::make_unique<melonDS::Net_Slirp>([](const unsigned char* data, int len) {
            net.RXEnqueue(data, len);
        }));
    }
}

int main(int argc, char** argv)
{
    return melonDSMain(argc, argv);
}

int melonDSMain(int argc, char** argv)
{
#ifdef _WIN32
    // Allocate a console for debug output on Windows
    if (AllocConsole())
    {
        FILE* pCout;
        freopen_s(&pCout, "CONOUT$", "w", stdout);
        FILE* pCerr;
        freopen_s(&pCerr, "CONOUT$", "w", stderr);
        SetConsoleTitle("melonDS Debug Console");
    }
#endif

    std::cout << "melonDS ImGui Frontend" << std::endl;

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) < 0)
    {
        std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
        return 1;
    }

    // Setup OpenGL attributes
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    // Load configuration
    Config::Load();

    melonDS::MPInterface::Set(melonDS::MPInterface_Dummy);
    melonDS::MPInterface::Get().SetRecvTimeout(Config::GetGlobalTable().GetInt("MP.RecvTimeout"));
    NetInit();

    // Create emulation instance
    std::unique_ptr<ImGuiEmuInstance> emuInstance = std::make_unique<ImGuiEmuInstance>(0);
    if (!emuInstance)
    {
        std::cerr << "Failed to create emulation instance" << std::endl;
        SDL_Quit();
        return 1;
    }

    // Create main window
    std::unique_ptr<ImGuiFrontend> mainWindow = std::make_unique<ImGuiFrontend>(0, emuInstance.get());
    if (!mainWindow)
    {
        std::cerr << "Failed to create main window" << std::endl;
        SDL_Quit();
        return 1;
    }

    // Initialize the window
    if (!mainWindow->init())
    {
        std::cerr << "Failed to initialize main window" << std::endl;
        SDL_Quit();
        return 1;
    }

    // Show the window
    mainWindow->show();

    // Frame timing for 60 FPS
    const double targetFPS = 60.0;
    const double targetFrameTime = 1000.0 / targetFPS;
    Uint32 lastFrameTime = SDL_GetTicks();

    // Main application loop
    while (!mainWindow->shouldClose())
    {
        Uint32 frameStart = SDL_GetTicks();
        
        // Poll events
        mainWindow->pollEvents();
        
        // Render frame
        mainWindow->render();
        
        // Present to screen
        mainWindow->present();
        
        // Frame rate limiting
        Uint32 frameEnd = SDL_GetTicks();
        double frameTime = frameEnd - frameStart;
        
        if (frameTime < targetFrameTime) {
            Uint32 delayTime = (Uint32)(targetFrameTime - frameTime);
            SDL_Delay(delayTime);
        }
        
        lastFrameTime = frameStart;
    }

    // Cleanup
    mainWindow->cleanup();
    mainWindow.reset();
    emuInstance.reset();

    // Save configuration
    Config::Save();

    // Cleanup MPInterface
    melonDS::MPInterface::Set(melonDS::MPInterface_Dummy);

    // Quit SDL
    SDL_Quit();
    
    return 0;
} 