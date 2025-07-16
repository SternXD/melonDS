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

#include "ImGuiFrontend.h"
#include "ImGuiEmuInstance.h"
#include "FileDialog.h"
#include <iostream>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <cstring>
#include "version.h"
#include <functional>
#include <vector>

#include "../glad/glad.h"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#include <commdlg.h>
#include <io.h>
#include <fcntl.h>
#endif

#include "types.h"
#include "NDS_Header.h"

using namespace melonDS;

ImGuiFrontend::ImGuiFrontend(int id, ImGuiEmuInstance* inst)
    : windowID(id), emuInstance(inst), window(nullptr), glContext(nullptr), hasOGL(false),
      windowCfg(emuInstance->getGlobalConfig().GetTable("Window"))
{
    shouldCloseFlag = false;
    focused = false;
    
    showMainMenuBar = true;
    showMenuBar = true;
    showStatusBar = true;
    consoleVisible = false;
    
    showEmuSettingsDialog = false;
    showInputConfigDialog = false;
    showVideoSettingsDialog = false;
    showAudioSettingsDialog = false;
    showCameraSettingsDialog = false;
    showMPSettingsDialog = false;
    showWifiSettingsDialog = false;
    showFirmwareSettingsDialog = false;
    showPathSettingsDialog = false;
    showInterfaceSettingsDialog = false;
    showPowerManagementDialog = false;
    showDateTimeDialog = false;
    showTitleManagerDialog = false;
    showROMInfoDialog = false;
    showRAMInfoDialog = false;
    showCheatsManagementDialog = false;
    showNetplayDialog = false;
    showAboutDialog = false;
    showImGuiDemo = false;
    
    topScreenTexture = 0;
    bottomScreenTexture = 0;
    texturesInitialized = false;
    
    loadRecentFilesMenu();
    
    currentMappingTarget = nullptr;
    isMappingInput = false;
    selectedJoystickID = -1;
    
    currentFontSize = FontSize_Normal;
    currentTheme = Theme_Dark;
    fontsLoaded = false;
    needFontRebuild = false;
    
    inRenderFrame = false;
    
    lastFrameTime = SDL_GetTicks();
    frameCount = 0;
    currentFPS = 0.0f;
    fpsUpdateTime = SDL_GetTicks();
    
    fontSizes[FontSize_Small] = 13.0f;
    fontSizes[FontSize_Normal] = 16.0f;
    fontSizes[FontSize_Large] = 20.0f;
    fontSizes[FontSize_ExtraLarge] = 24.0f;
    
    for (int i = 0; i < FontSize_COUNT; i++) {
        fonts[i] = nullptr;
    }

    lastTopScreen.resize(256 * 192, 0);
    lastBottomScreen.resize(256 * 192, 0);
    hasLastScreen = false;
    
    pauseOnLostFocus = emuInstance->getGlobalConfig().GetBool("PauseLostFocus");
    
    currentMappingTarget = nullptr;
    isMappingInput = false;
    selectedJoystickID = 0;
    
    showKeyboardMappings = true;
    showJoystickMappings = false;
    
    loadInputConfig();
}

ImGuiFrontend::~ImGuiFrontend()
{
    cleanup();
}

const char* ImGuiFrontend::dsButtonNames[ImGuiFrontend::numDSButtons] = {
    "A", "B", "Select", "Start", "Right", "Left", "Up", "Down", "R", "L", "X", "Y"
};

const char* ImGuiFrontend::dsButtonLabels[ImGuiFrontend::numDSButtons] = {
    "A", "B", "Select", "Start", "Right", "Left", "Up", "Down", "R", "L", "X", "Y"
};

const char* ImGuiFrontend::hotkeyNames[ImGuiFrontend::numHotkeys] = {
    "HK_Lid", "HK_Mic", "HK_Pause", "HK_Reset", "HK_FastForward",
    "HK_FrameLimitToggle", "HK_FullscreenToggle", "HK_SwapScreens", "HK_SwapScreenEmphasis",
    "HK_SolarSensorDecrease", "HK_SolarSensorIncrease", "HK_FrameStep", "HK_PowerButton",
    "HK_VolumeUp", "HK_VolumeDown", "HK_SlowMo", "HK_FastForwardToggle", "HK_SlowMoToggle",
    "HK_GuitarGripGreen", "HK_GuitarGripRed", "HK_GuitarGripYellow", "HK_GuitarGripBlue"
};

const char* ImGuiFrontend::hotkeyLabels[ImGuiFrontend::numHotkeys] = {
    "Close/open lid", "Microphone", "Pause/resume", "Reset", "Fast forward",
    "Toggle FPS limit", "Toggle fullscreen", "Swap screens", "Swap screen emphasis",
    "[Boktai] Sunlight -", "[Boktai] Sunlight +", "Frame step", "DSi Power button",
    "DSi Volume up", "DSi Volume down", "Slow motion", "Toggle fast forward", "Toggle slow motion",
    "[Guitar Grip] Green", "[Guitar Grip] Red", "[Guitar Grip] Yellow", "[Guitar Grip] Blue"
};

bool ImGuiFrontend::init()
{
    loadWindowState();
    
    // Create SDL window
    int width = windowCfg.GetInt("Width");
    if (width == 0) width = 1200;
    int height = windowCfg.GetInt("Height");
    if (height == 0) height = 900;
    int posX = windowCfg.GetInt("PosX");
    if (posX == 0) posX = SDL_WINDOWPOS_CENTERED;
    int posY = windowCfg.GetInt("PosY");
    if (posY == 0) posY = SDL_WINDOWPOS_CENTERED;
    
    window = SDL_CreateWindow(
        "melonDS - ImGui Frontend",
        posX, posY,
        width, height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN
    );
    
    if (!window)
    {
        std::cerr << "Failed to create SDL window: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // Create OpenGL context
    glContext = SDL_GL_CreateContext(window);
    if (!glContext)
    {
        std::cerr << "Failed to create OpenGL context: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // Make OpenGL context current
    SDL_GL_MakeCurrent(window, glContext);
    
    // Enable vsync
    SDL_GL_SetSwapInterval(1);
    
    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress))
    {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return false;
    }
    
    std::cout << "OpenGL version: " << glGetString(GL_VERSION) << std::endl;
    
    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    
    io.ConfigNavMoveSetMousePos = false;  // Don't move mouse cursor with gamepad
    io.ConfigNavCaptureKeyboard = true;   // Capture keyboard when using gamepad nav
    io.NavActive = true;                  // Enable navigation
    io.ConfigNavSwapGamepadButtons = false; // Use standard Xbox controller layout
    
    io.ConfigInputTrickleEventQueue = false;  // Process all input events immediately
    
    ImGui_ImplSDL2_InitForOpenGL(window, glContext);
    ImGui_ImplOpenGL3_Init("#version 330");
    
    initFonts();
    loadFontSettings();
    
    // Apply theme and styling
    applyTheme(currentTheme);
    
    // Initialize OpenGL
    initOpenGL();
    
    std::cout << "ImGuiFrontend initialized successfully" << std::endl;
    
    return true;
}

void ImGuiFrontend::cleanup()
{
    saveWindowState();
    
    // Hide console window if it's visible
    if (consoleVisible)
    {
        hideConsoleWindow();
    }
    
    // Cleanup ImGui
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    
    // Cleanup OpenGL
    deinitOpenGL();
    
    // Cleanup SDL
    if (glContext)
    {
        SDL_GL_DeleteContext(glContext);
        glContext = nullptr;
    }
    
    if (window)
    {
        SDL_DestroyWindow(window);
        window = nullptr;
    }
}

void ImGuiFrontend::show()
{
    if (window)
    {
        SDL_ShowWindow(window);
    }
}

void ImGuiFrontend::hide()
{
    if (window)
    {
        SDL_HideWindow(window);
    }
}

void ImGuiFrontend::pollEvents()
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        // Always process ImGui events first
        ImGui_ImplSDL2_ProcessEvent(&event);
        
        // Only process other events if ImGui doesn't want them
        if (!ImGui::GetIO().WantCaptureKeyboard && !ImGui::GetIO().WantCaptureMouse) {
            switch (event.type) {
                case SDL_QUIT:
                    shouldCloseFlag = true;
                    break;
                    
                case SDL_WINDOWEVENT:
                    switch (event.window.event) {
                        case SDL_WINDOWEVENT_FOCUS_GAINED:
                            onFocusIn();
                            break;
                        case SDL_WINDOWEVENT_FOCUS_LOST:
                            onFocusOut();
                            break;
                        case SDL_WINDOWEVENT_CLOSE:
                            shouldCloseFlag = true;
                            break;
                    }
                    break;
                    
                case SDL_KEYDOWN:
                    emuInstance->onKeyPress(&event.key);
                    break;
                    
                case SDL_KEYUP:
                    emuInstance->onKeyRelease(&event.key);
                    break;
                    
                case SDL_MOUSEBUTTONDOWN:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        lastMouseX = event.button.x;
                        lastMouseY = event.button.y;
                        mousePressed = true;
                    }
                    break;
                    
                case SDL_MOUSEBUTTONUP:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        mousePressed = false;
                        emuInstance->onMouseRelease(event.button.button, event.button.x, event.button.y);
                    }
                    break;
                    
                case SDL_MOUSEMOTION:
                    lastMouseX = event.motion.x;
                    lastMouseY = event.motion.y;
                    break;
            }
        }
    }
    
    emuInstance->inputProcess();
    
    if (isMappingInput && currentMappingTarget) {
        handleInputCapture();
    }
    
    if (emuInstance->hotkeyPressed(HK_Pause)) onPause();
    if (emuInstance->hotkeyPressed(HK_Reset)) onReset();
    if (emuInstance->hotkeyPressed(HK_FrameStep)) onFrameStep();
    if (emuInstance->hotkeyPressed(HK_FastForward)) {
        fastForward = !fastForward;
        if (fastForward) {
            emuInstance->osdAddMessage(0x00FF00FF, "Fast Forward ON");
        } else {
            emuInstance->osdAddMessage(0x00FF00FF, "Fast Forward OFF");
        }
    }
    if (emuInstance->hotkeyPressed(HK_FullscreenToggle)) {
        bool isFullscreen = SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP;
        if (isFullscreen) {
            SDL_SetWindowFullscreen(window, 0);
            emuInstance->osdAddMessage(0x00FF00FF, "Fullscreen OFF");
        } else {
            SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
            emuInstance->osdAddMessage(0x00FF00FF, "Fullscreen ON");
        }
    }
    if (emuInstance->hotkeyPressed(HK_SwapScreens)) {
        screenSwap = !screenSwap;
        if (screenSwap) {
            emuInstance->osdAddMessage(0x00FF00FF, "Screen Swap ON");
        } else {
            emuInstance->osdAddMessage(0x00FF00FF, "Screen Swap OFF");
        }
    }

    if (controllerTouchMode && emuInstance && emuInstance->isRunning()) {
        SDL_GameController* gc = nullptr;
        for (int i = 0; i < SDL_NumJoysticks(); ++i) {
            if (SDL_IsGameController(i)) {
                gc = SDL_GameControllerOpen(i);
                break;
            }
        }
        if (gc) {
            int dx = 0, dy = 0;
            dx += SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTX) / 8000;
            dy += SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTY) / 8000;
            if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_LEFT)) dx -= 2;
            if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) dx += 2;
            if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_UP)) dy -= 2;
            if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_DOWN)) dy += 2;
            if (dx != 0 || dy != 0) {
                emuInstance->touchCursorX = std::clamp(emuInstance->touchCursorX + dx, 0, 255);
                emuInstance->touchCursorY = std::clamp(emuInstance->touchCursorY + dy, 0, 191);
            }
            bool touchPressed =
                SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_A) ||
                SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_B) ||
                SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_X) ||
                SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_Y);
            if (touchPressed) {
                emuInstance->isTouching = true;
                emuInstance->touchX = emuInstance->touchCursorX;
                emuInstance->touchY = emuInstance->touchCursorY;
            } else {
                emuInstance->isTouching = false;
            }
            SDL_GameControllerClose(gc);
        }
    }
}

void ImGuiFrontend::render()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
    
    inRenderFrame = true;
    
    static int emuFrames = 0;
    static Uint32 lastFpsUpdate = SDL_GetTicks();
    if (emuInstance && emuInstance->isActive()) {
        updateScreenTextures();
        emuFrames++;
    }

    Uint32 now = SDL_GetTicks();
    if (emuFrames >= 30) {
        float dt = (now - lastFpsUpdate) / 1000.0f;
        if (dt > 0.0f) {
            currentFPS = (float)emuFrames / dt;
            std::cout << "[ImGuiFrontend] FPS calculated: " << currentFPS << " (emuFrames=" << emuFrames << ", dt=" << dt << ")" << std::endl;
        }
        emuFrames = 0;
        lastFpsUpdate = now;
    }

    float menuBarHeight = 0.0f;
    if (ImGui::BeginMainMenuBar()) {
        renderMenuBar();
        menuBarHeight = ImGui::GetFrameHeight();
        ImGui::EndMainMenuBar();
    }

    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    float statusBarHeight = ImGui::GetFrameHeight();
    ImVec2 mainContentPos = ImVec2(0, menuBarHeight);
    ImVec2 mainContentSize = ImVec2(displaySize.x, displaySize.y - menuBarHeight - statusBarHeight);
    ImGuiWindowFlags mainFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                                 ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoInputs;
    ImGui::SetNextWindowPos(mainContentPos);
    ImGui::SetNextWindowSize(mainContentSize);
    ImGui::Begin("MainContent", nullptr, mainFlags);

    if (!emuInstance || !emuInstance->isRunning()) {
        ImVec2 windowSize = ImGui::GetWindowSize();
        ImVec2 center = ImVec2(windowSize.x * 0.5f, windowSize.y * 0.5f);
        ImGui::SetCursorPos(ImVec2(center.x - 180, center.y - 20));
        ImGui::Text("No ROM loaded or emulation stopped.");
        ImGui::SetCursorPos(ImVec2(center.x - 180, center.y + 10));
        ImGui::Text("Use File -> Open ROM or Boot firmware to start emulation.");
    } else {
        renderDSScreensIntegrated();
    }
    renderSettingsDialogs();

    if (showAboutDialog)
    {
        if (ImGui::Begin("About melonDS", &showAboutDialog, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("melonDS");
            ImGui::Text("Version %s", MELONDS_VERSION);
            ImGui::Spacing();
            
#ifdef MELONDS_EMBED_BUILD_INFO
            ImGui::Text("Branch: " MELONDS_GIT_BRANCH);
            ImGui::Text("Commit: " MELONDS_GIT_HASH);
            ImGui::Text("Built by: " MELONDS_BUILD_PROVIDER);
            ImGui::Spacing();
#endif
            
            ImGui::Text("Nintendo DS/DSi emulator");
            ImGui::Spacing();
            
            ImGui::Text("Copyright 2016-2025 melonDS team");
            ImGui::Text("Licensed under GPLv3+");
            ImGui::Spacing();
            
            ImGui::Separator();
            if (ImGui::Button("Visit Website"))
            {
#ifdef _WIN32
                ShellExecuteA(nullptr, "open", "https://melonds.kuribo64.net/", nullptr, nullptr, SW_SHOWNORMAL);
#elif defined(__APPLE__)
                system("open https://melonds.kuribo64.net/");
#else
                system("xdg-open https://melonds.kuribo64.net/");
#endif
            }
            ImGui::SameLine();
            if (ImGui::Button("GitHub Repository"))
            {
#ifdef _WIN32
                ShellExecuteA(nullptr, "open", "https://github.com/melonDS-emu/melonDS", nullptr, nullptr, SW_SHOWNORMAL);
#elif defined(__APPLE__)
                system("open https://github.com/melonDS-emu/melonDS");
#else
                system("xdg-open https://github.com/melonDS-emu/melonDS");
#endif
            }
            
            ImGui::Spacing();
            ImGui::Separator();
            
            ImGui::Text("Controller Navigation:");
            ImGui::BulletText("D-Pad/Left Stick: Navigate menus");
            ImGui::BulletText("A/Cross: Select/Activate");
            ImGui::BulletText("B/Circle: Cancel/Back");
            ImGui::BulletText("Y/Square: Open menu");
            ImGui::BulletText("X/Triangle: Toggle menu focus");
            ImGui::BulletText("Start: Activate focused item");
            ImGui::BulletText("Back/Select: Cancel action");
            
            ImGui::Spacing();
            ImGui::Separator();
            
            if (ImGui::Button("OK"))
            {
                showAboutDialog = false;
            }
        }
        ImGui::End();
    }
    if (showImGuiDemo)
    {
        ImGui::ShowDemoWindow(&showImGuiDemo);
    }

    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(0, displaySize.y - statusBarHeight));
    ImGui::SetNextWindowSize(ImVec2(displaySize.x, statusBarHeight));
    ImGuiWindowFlags statusFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                                   ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
                                   ImGuiWindowFlags_NoInputs;
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.16f, 0.16f, 0.18f, 1.0f));
    ImGui::Begin("StatusBar", nullptr, statusFlags & ~ImGuiWindowFlags_NoInputs);
    ImGui::SetCursorPosY(0);
    ImGui::Text("Cart: %s | Status: %s | DirectBoot: %s | FPS: %.1f",
        emuInstance->getCartLabel().c_str(),
        emuInstance->isRunning() ? (emuInstance->isPaused() ? "Paused" : "Running") : "Stopped",
        emuInstance->getGlobalConfig().GetBool("Emu.DirectBoot") ? "On" : "Off",
        currentFPS
    );
    ImGui::End();
    ImGui::PopStyleColor();

    if (showOpenFileDialog) {
        showOpenFileDialog = false;
        std::cout << "[ImGuiFrontend] Open ROM dialog triggered" << std::endl;
        if (!emuInstance) {
            std::cout << "[ImGuiFrontend] emuInstance is null!" << std::endl;
        } else {
            auto files = pickROM(false); // false = DS ROM
            std::cout << "[ImGuiFrontend] pickROM returned " << files.size() << " file(s)" << std::endl;
            if (!files.empty()) {
                std::string errorstr;
                bool result = emuInstance->loadROM(files, false, errorstr);
                std::cout << "[ImGuiFrontend] loadROM result: " << result << ", error: " << errorstr << std::endl;
                std::cout << "[ImGuiFrontend] cartInserted after loadROM: " << emuInstance->hasCart() << std::endl;
                if (result) {
                    auto* thread = emuInstance->getEmuThread();
                    thread->emuRun();
                    std::cout << "[ImGuiFrontend] emuThread->emuRun() called" << std::endl;
                    updateCartInserted(false);
                } else {
                    showErrorDialog(errorstr.empty() ? "Failed to load ROM (unknown error)" : errorstr);
                }
            }
        }
    }
    if (requestBootFirmwareFlag) {
        requestBootFirmwareFlag = false;
        if (!emuInstance) {
            showErrorDialog("Emulation instance is null!");
        } else {
            std::string errorstr;
            bool result = emuInstance->bootToMenu(errorstr);
            if (result) {
                auto* thread = emuInstance->getEmuThread();
                thread->emuRun();
                updateCartInserted(false);
            } else {
                showErrorDialog(errorstr.empty() ? "Failed to boot firmware (unknown error)" : errorstr);
            }
        }
    }
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    
    inRenderFrame = false;

    if (showErrorPopup) {
        if (ImGui::BeginPopupModal("Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextWrapped("%s", errorPopupMessage.c_str());
            if (ImGui::Button("OK")) {
                showErrorPopup = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }
}

void ImGuiFrontend::present()
{
    SDL_GL_SwapWindow(window);
}

void ImGuiFrontend::renderMenuBar()
{
    // File menu
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Open ROM...", "Ctrl+O")) onOpenFile();
        if (ImGui::BeginMenu("Open recent")) {
            for (size_t i = 0; i < recentFiles.size(); ++i) {
                std::string label = std::to_string(i+1) + ".  " + recentFiles[i];
                if (ImGui::MenuItem(label.c_str())) onOpenRecentFile(static_cast<int>(i));
            }
            if (recentFiles.empty()) ImGui::MenuItem("(No recent files)", nullptr, false, false);
            ImGui::Separator();
            if (ImGui::MenuItem("Clear")) onClearRecentFiles();
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Boot firmware")) onBootFirmware();
        ImGui::Separator();
        ImGui::MenuItem(("DS slot: " + emuInstance->getCartLabel()).c_str(), nullptr, false, false);
        if (ImGui::MenuItem("Insert cart...")) onInsertCart();
        if (ImGui::MenuItem("Eject cart##ds", nullptr, false, emuInstance->hasCart())) onEjectCart();
        ImGui::Separator();
        ImGui::MenuItem(("GBA slot: " + emuInstance->getGBACartLabel()).c_str(), nullptr, false, false);
        if (ImGui::MenuItem("Insert ROM cart...")) onInsertGBACart();
        if (ImGui::BeginMenu("Insert add-on cart")) {
            struct AddonEntry { int type; const char* label; };
            static const AddonEntry addons[] = {
                { ImGuiEmuInstance::GBAAddon_RAMExpansion, "Memory expansion" },
                { ImGuiEmuInstance::GBAAddon_RumblePak, "Rumble Pak" },
                { ImGuiEmuInstance::GBAAddon_SolarSensorBoktai1, "Boktai solar sensor 1" },
                { ImGuiEmuInstance::GBAAddon_SolarSensorBoktai2, "Boktai solar sensor 2" },
                { ImGuiEmuInstance::GBAAddon_SolarSensorBoktai3, "Boktai solar sensor 3" },
                { ImGuiEmuInstance::GBAAddon_MotionPakHomebrew, "Motion Pak (Homebrew)" },
                { ImGuiEmuInstance::GBAAddon_MotionPakRetail, "Motion Pak (Retail)" },
                { ImGuiEmuInstance::GBAAddon_GuitarGrip, "Guitar Grip" },
            };
            for (const auto& addon : addons) {
                if (ImGui::MenuItem(addon.label)) {
                    std::string error;
                    if (!emuInstance->loadGBAAddon(addon.type, error)) {
                        showErrorDialog(error);
                    } else {
                        updateCartInserted(true);
                    }
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Eject cart##gba", nullptr, false, emuInstance->hasGBACart())) onEjectGBACart();
        ImGui::Separator();
        if (ImGui::MenuItem("Import savefile")) {
            onImportSavefile();
        }
        ImGui::Separator();
        if (ImGui::BeginMenu("Save state")) {
            for (int i = 1; i <= 8; ++i) {
                std::string label = std::to_string(i);
                if (ImGui::MenuItem(label.c_str())) onSaveState(i);
            }
            if (ImGui::MenuItem("File...")) {
                onSaveState(0);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Load state")) {
            for (int i = 1; i <= 8; ++i) {
                std::string label = std::to_string(i);
                bool exists = emuInstance->savestateExists(i);
                if (ImGui::MenuItem(label.c_str(), nullptr, false, exists)) onLoadState(i);
            }
            if (ImGui::MenuItem("File...")) {
                onLoadState(0);
            }
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Undo state load", "F12")) {
            onUndoStateLoad();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Open melonDS directory")) {
            std::string configDir = emuInstance->getConfigDirectory();
            
#ifdef _WIN32
            // Use Windows Explorer
            std::string command = "explorer \"" + configDir + "\"";
            system(command.c_str());
#elif defined(__APPLE__)
            // Use Finder on macOS
            std::string command = "open \"" + configDir + "\"";
            system(command.c_str());
#else
            // Use default file manager on Linux
            std::string command = "xdg-open \"" + configDir + "\"";
            system(command.c_str());
#endif
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Quit")) onQuit();
        ImGui::EndMenu();
    }
    // System menu
    if (ImGui::BeginMenu("System")) {
        bool paused = emuInstance->isPaused();
        if (ImGui::MenuItem("Pause", nullptr, &paused, emuInstance->isActive())) onPause();
        if (ImGui::MenuItem("Reset", nullptr, false, emuInstance->isActive())) onReset();
        if (ImGui::MenuItem("Stop", nullptr, false, emuInstance->isActive())) onStop();
        if (ImGui::MenuItem("Frame step", nullptr, false, emuInstance->isActive())) onFrameStep();
        ImGui::Separator();
        if (ImGui::MenuItem("Power management")) onOpenPowerManagement();
        if (ImGui::MenuItem("Date and time")) onOpenDateTime();
        ImGui::Separator();
        bool cheatsEnabled = emuInstance->getGlobalConfig().GetBool("Emu.EnableCheats");
        if (ImGui::MenuItem("Enable cheats", nullptr, &cheatsEnabled)) onEnableCheats();
        if (ImGui::MenuItem("Setup cheat codes")) onSetupCheats();
        ImGui::Separator();
        if (ImGui::MenuItem("ROM info")) onROMInfo();
        if (ImGui::MenuItem("RAM search")) onRAMInfo();
        if (ImGui::MenuItem("Manage DSi titles")) onOpenTitleManager();
        ImGui::Separator();
        if (ImGui::BeginMenu("Multiplayer")) {
            if (ImGui::MenuItem("Launch new instance")) { 
                onMPNewInstance();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Host LAN game")) { 
                onLANStartHost();
            }
            if (ImGui::MenuItem("Join LAN game")) { 
                onLANStartClient();
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenu();
    }
    // View menu
    if (ImGui::BeginMenu("View")) {
        if (ImGui::BeginMenu("Screen size")) {
            auto& cfg = emuInstance->getGlobalConfig();
            int currentSize = cfg.GetInt("Screen.WindowScale");
            for (int i = 1; i <= 4; ++i) {
                std::string label = std::to_string(i) + "x";
                bool selected = (currentSize == i);
                if (ImGui::MenuItem(label.c_str(), nullptr, selected)) {
                    onChangeScreenSize(i);
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Screen rotation")) {
            auto& cfg = emuInstance->getGlobalConfig();
            int currentRotation = cfg.GetInt("Screen.Rotation");
            const char* rotations[] = {"0°", "90°", "180°", "270°"};
            for (int i = 0; i < 4; ++i) {
                bool selected = (currentRotation == i);
                if (ImGui::MenuItem(rotations[i], nullptr, selected)) {
                    onChangeScreenRotation(i);
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Screen gap")) {
            auto& cfg = emuInstance->getGlobalConfig();
            int currentGap = cfg.GetInt("Screen.Gap");
            const int gaps[] = {0, 1, 8, 64, 90, 128};
            for (int i = 0; i < 6; ++i) {
                std::string label = std::to_string(gaps[i]) + " px";
                bool selected = (currentGap == gaps[i]);
                if (ImGui::MenuItem(label.c_str(), nullptr, selected)) {
                    onChangeScreenGap(gaps[i]);
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Screen layout")) {
            auto& cfg = emuInstance->getGlobalConfig();
            int currentLayout = cfg.GetInt("Screen.Layout");
            const char* layouts[] = {"Natural", "Vertical", "Horizontal", "Hybrid"};
            for (int i = 0; i < 4; ++i) {
                bool selected = (currentLayout == i);
                if (ImGui::MenuItem(layouts[i], nullptr, selected)) {
                    onChangeScreenLayout(i);
                }
            }
            ImGui::Separator();
            bool swap = cfg.GetBool("Screen.SwapScreens");
            if (ImGui::MenuItem("Swap screens", nullptr, &swap)) {
                onChangeScreenSwap(swap);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Screen sizing")) {
            auto& cfg = emuInstance->getGlobalConfig();
            int currentSizing = cfg.GetInt("Screen.Sizing");
            const char* sizings[] = {"Even", "Emphasize top", "Emphasize bottom", "Auto", "Top only", "Bottom only"};
            for (int i = 0; i < 6; ++i) {
                bool selected = (currentSizing == i);
                if (ImGui::MenuItem(sizings[i], nullptr, selected)) {
                    onChangeScreenSizing(i);
                }
            }
            ImGui::Separator();
            bool integerScaling = cfg.GetBool("Screen.IntegerScaling");
            if (ImGui::MenuItem("Force integer scaling", nullptr, &integerScaling)) {
                onChangeIntegerScaling(integerScaling);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Aspect ratio")) {
            auto& cfg = emuInstance->getGlobalConfig();
            
            ImGui::Text("Top Screen Aspect Ratio:");
            int topAspect = cfg.GetInt("Screen.AspectTop");
            const char* aspects[] = {"4:3", "16:9", "16:10", "21:9"};
            for (int i = 0; i < 4; ++i) {
                bool selected = (topAspect == i);
                if (ImGui::MenuItem(aspects[i], nullptr, selected)) {
                    onChangeScreenAspect(i, true);
                }
            }
            
            ImGui::Separator();
            ImGui::Text("Bottom Screen Aspect Ratio:");
            int bottomAspect = cfg.GetInt("Screen.AspectBot");
            for (int i = 0; i < 4; ++i) {
                bool selected = (bottomAspect == i);
                std::string label = std::string(aspects[i]) + "##bottom";
                if (ImGui::MenuItem(label.c_str(), nullptr, selected)) {
                    onChangeScreenAspect(i, false);
                }
            }
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Open new window")) { 
            onOpenNewWindow();
        }
        ImGui::Separator();
        bool filtering = emuInstance->getGlobalConfig().GetBool("Video.Filtering");
        if (ImGui::MenuItem("Screen filtering", nullptr, &filtering)) onChangeScreenFiltering(filtering);
        bool showOSD = emuInstance->getGlobalConfig().GetBool("Window.ShowOSD");
        if (ImGui::MenuItem("Show OSD", nullptr, &showOSD)) onChangeShowOSD(showOSD);
        ImGui::Separator();
        if (ImGui::MenuItem("Controller Touch Mode", nullptr, controllerTouchMode)) {
            controllerTouchMode = !controllerTouchMode;
        }
        ImGui::EndMenu();
    }
    // Config menu
    if (ImGui::BeginMenu("Config")) {
        if (ImGui::MenuItem("Emu settings")) onOpenEmuSettings();
        if (ImGui::MenuItem("Input and hotkeys")) onOpenInputConfig();
        if (ImGui::MenuItem("Video settings")) onOpenVideoSettings();
        if (ImGui::MenuItem("Camera settings")) onOpenCameraSettings();
        if (ImGui::MenuItem("Audio settings")) onOpenAudioSettings();
        if (ImGui::MenuItem("Multiplayer settings")) onOpenMPSettings();
        if (ImGui::MenuItem("Wifi settings")) onOpenWifiSettings();
        if (ImGui::MenuItem("Firmware settings")) onOpenFirmwareSettings();
        if (ImGui::MenuItem("Interface settings")) onOpenInterfaceSettings();
        if (ImGui::MenuItem("Path settings")) onOpenPathSettings();
        if (ImGui::BeginMenu("Savestate settings")) {
            bool separate = emuInstance->getGlobalConfig().GetBool("Savestate.SeparateSavefiles");
            if (ImGui::MenuItem("Separate savefiles", nullptr, &separate)) onChangeSavestateSRAMReloc(separate);
            ImGui::EndMenu();
        }
        ImGui::Separator();
        bool limitFramerate = emuInstance->getGlobalConfig().GetBool("Emu.LimitFramerate");
        if (ImGui::MenuItem("Limit framerate", nullptr, &limitFramerate)) onChangeLimitFramerate(limitFramerate);
        bool audioSync = emuInstance->getGlobalConfig().GetBool("Audio.Sync");
        if (ImGui::MenuItem("Audio sync", nullptr, &audioSync)) onChangeAudioSync(audioSync);
        ImGui::EndMenu();
    }
    // Help menu
    if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("About...")) showAboutDialog = true;
        ImGui::EndMenu();
    }
}

void ImGuiFrontend::renderDSScreensIntegrated()
{
    bool running = emuInstance && emuInstance->isRunning();
    bool active = emuInstance->isActive();
    
    if (!running || !active) {
        renderSplashScreen();
        return;
    }
    
    uint32_t* topScreenData = static_cast<uint32_t*>(emuInstance->getScreenBuffer(0));
    uint32_t* bottomScreenData = static_cast<uint32_t*>(emuInstance->getScreenBuffer(1));
    
    if (!topScreenData || !bottomScreenData) {
        ImGui::Text("No screen data available");
        std::cout << "[renderDSScreensIntegrated] Screen buffer(s) null, skipping draw. topScreenData=" << topScreenData << ", bottomScreenData=" << bottomScreenData << std::endl;
        return;
    }
    
    if (!texturesInitialized) {
        ImGui::Text("Textures not initialized");
        std::cout << "[renderDSScreensIntegrated] Textures not initialized!" << std::endl;
        return;
    }

    const ImVec2 screenSize(256, 192);
    const float scale = 2.0f;
    const ImVec2 scaledScreenSize = ImVec2(screenSize.x * scale, screenSize.y * scale);
    const float spacing = 16.0f * scale;
    const ImVec2 blockSize = ImVec2(scaledScreenSize.x, scaledScreenSize.y * 2 + spacing);

    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImVec2 blockPos = ImVec2(
        ImGui::GetCursorPosX() + (avail.x - blockSize.x) * 0.5f,
        ImGui::GetCursorPosY() + (avail.y - blockSize.y) * 0.5f
    );
    ImGui::SetCursorPos(blockPos);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 absBlockMin = ImGui::GetCursorScreenPos();
    ImVec2 absBlockMax = ImVec2(absBlockMin.x + blockSize.x, absBlockMin.y + blockSize.y);
    drawList->AddRectFilled(absBlockMin, absBlockMax, IM_COL32(30, 30, 40, 220), 12.0f);
    drawList->AddRect(absBlockMin, absBlockMax, IM_COL32(80, 80, 120, 255), 12.0f, 0, 2.0f);

    // Top screen
    ImGui::SetCursorPos(ImVec2(blockPos.x, blockPos.y));
    ImGui::Image((void*)(intptr_t)topScreenTexture, scaledScreenSize);

    // Bottom screen
    ImVec2 bottomScreenPos = ImVec2(blockPos.x, blockPos.y + scaledScreenSize.y + spacing);
    ImGui::SetCursorPos(bottomScreenPos);
    ImGui::PushID("BottomScreen");
    ImGui::Image((void*)(intptr_t)bottomScreenTexture, scaledScreenSize);
    ImVec2 imageMin = ImGui::GetItemRectMin();
    ImVec2 imageMax = ImGui::GetItemRectMax();
    
    if (!controllerTouchMode) {
        if (lastMouseX >= imageMin.x && lastMouseX <= imageMax.x && 
            lastMouseY >= imageMin.y && lastMouseY <= imageMax.y) {
            
            float relX = (lastMouseX - imageMin.x) / scale;
            float relY = (lastMouseY - imageMin.y) / scale;
            int dsX = (int)relX;
            int dsY = (int)relY;
            
            if (dsX >= 0 && dsX < 256 && dsY >= 0 && dsY < 192) {
                if (mousePressed) {
                    emuInstance->onMouseClick(dsX, dsY);
                }
            }
        }
    }
    if (controllerTouchMode) {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 base = imageMin;
        float cx = base.x + emuInstance->touchCursorX * scale;
        float cy = base.y + emuInstance->touchCursorY * scale;
        draw_list->AddCircle(ImVec2(cx, cy), 8.0f, IM_COL32(255, 255, 0, 255), 0, 2.5f);
        draw_list->AddLine(ImVec2(cx-10, cy), ImVec2(cx+10, cy), IM_COL32(255,255,0,255), 2.0f);
        draw_list->AddLine(ImVec2(cx, cy-10), ImVec2(cx, cy+10), IM_COL32(255,255,0,255), 2.0f);
    }
    ImGui::PopID();
}

void ImGuiFrontend::renderSettingsDialogs()
{
    if (showEmuSettingsDialog)
    {
        renderEmuSettingsDialog();
    }
    
    if (showInputConfigDialog)
    {
        renderInputConfigDialog();
    }
    
    if (showVideoSettingsDialog)
    {
        renderVideoSettingsDialog();
    }
    
    if (showAudioSettingsDialog)
    {
        renderAudioSettingsDialog();
    }
    
    if (showWifiSettingsDialog)
    {
        renderWifiSettingsDialog();
    }
    
    if (showFirmwareSettingsDialog)
    {
        renderFirmwareSettingsDialog();
    }
    
    if (showPathSettingsDialog)
    {
        renderPathSettingsDialog();
    }
    
    if (showInterfaceSettingsDialog)
    {
        renderInterfaceSettingsDialog();
    }
    
    if (showPowerManagementDialog)
    {
        renderPowerManagementDialog();
    }
    
    if (showDateTimeDialog)
    {
        renderDateTimeDialog();
    }
    
    if (showTitleManagerDialog)
    {
        renderTitleManagerDialog();
    }
    
    if (showROMInfoDialog)
    {
        renderROMInfoDialog();
    }
    
    if (showRAMInfoDialog)
    {
        renderRAMInfoDialog();
    }
    
    if (showCheatsManagementDialog)
    {
        renderCheatsManagementDialog();
    }
    
    if (showNetplayDialog)
    {
        renderNetplayDialog();
    }
    
    if (showCameraSettingsDialog)
    {
        renderCameraSettingsDialog();
    }
    
    if (showMPSettingsDialog)
    {
        renderMPSettingsDialog();
    }
}

constexpr ImGuiWindowFlags SettingsDialogFlags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse;

void ImGuiFrontend::renderEmuSettingsDialog()
{
    if (!showEmuSettingsDialog) return;
    
    ImGui::SetNextWindowSize(ImVec2(600, 700), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    
    if (ImGui::Begin("Emulator Settings", &showEmuSettingsDialog, SettingsDialogFlags))
    {
        auto& globalCfg = emuInstance->getGlobalConfig();
        auto& localCfg = emuInstance->getLocalConfig();
        int consoleType = globalCfg.GetInt("Emu.ConsoleType");
        bool directBoot = globalCfg.GetBool("Emu.DirectBoot");
        bool externalBIOS = globalCfg.GetBool("Emu.ExternalBIOSEnable");
        bool dldiEnable = globalCfg.GetBool("DLDI.Enable");
        bool dldiFolder = globalCfg.GetBool("DLDI.FolderSync");
        bool dldiReadOnly = globalCfg.GetBool("DLDI.ReadOnly");
        bool dsiFullBIOSBoot = globalCfg.GetBool("DSi.FullBIOSBoot");
        bool dsiSDEnable = globalCfg.GetBool("DSi.SD.Enable");
        bool dsiSDFolder = globalCfg.GetBool("DSi.SD.FolderSync");
        bool dsiSDReadOnly = globalCfg.GetBool("DSi.SD.ReadOnly");
        bool jitEnable = globalCfg.GetBool("JIT.Enable");
        bool jitBranchOpt = globalCfg.GetBool("JIT.BranchOptimisations");
        bool jitLiteralOpt = globalCfg.GetBool("JIT.LiteralOptimisations");
        bool jitFastMemory = globalCfg.GetBool("JIT.FastMemory");
        int jitMaxBlockSize = globalCfg.GetInt("JIT.MaxBlockSize");
        int dldiImageSize = globalCfg.GetInt("DLDI.ImageSize");
        int dsiSDImageSize = globalCfg.GetInt("DSi.SD.ImageSize");
        std::string dldiSDPath = globalCfg.GetString("DLDI.ImagePath");
        std::string dldiFolderPath = globalCfg.GetString("DLDI.FolderPath");
        std::string bios9Path = globalCfg.GetString("DS.BIOS9Path");
        std::string bios7Path = globalCfg.GetString("DS.BIOS7Path");
        std::string firmwarePath = globalCfg.GetString("DS.FirmwarePath");
        std::string dsiBios9Path = globalCfg.GetString("DSi.BIOS9Path");
        std::string dsiBios7Path = globalCfg.GetString("DSi.BIOS7Path");
        std::string dsiFirmwarePath = globalCfg.GetString("DSi.FirmwarePath");
        std::string dsiNANDPath = globalCfg.GetString("DSi.NANDPath");
        std::string dsiSDPath = globalCfg.GetString("DSi.SD.ImagePath");
        std::string dsiSDFolderPath = globalCfg.GetString("DSi.SD.FolderPath");
        bool gdbEnabled = localCfg.GetBool("Gdb.Enabled");
        int gdbPortA9 = localCfg.GetInt("Gdb.ARM9.Port");
        int gdbPortA7 = localCfg.GetInt("Gdb.ARM7.Port");
        bool gdbBOSA9 = localCfg.GetBool("Gdb.ARM9.BreakOnStartup");
        bool gdbBOSA7 = localCfg.GetBool("Gdb.ARM7.BreakOnStartup");
        const char* consoleItems[] = { "DS", "DSi (experimental)" };
        const char* dldiSizeItems[] = { "Auto", "256 MB", "512 MB", "1 GB", "2 GB", "4 GB" };
        const char* dsiSDSizeItems[] = { "Auto", "256 MB", "512 MB", "1 GB", "2 GB", "4 GB" };
        if (ImGui::BeginTabBar("EmuSettingsTabs")) {
            // General Tab
            if (ImGui::BeginTabItem("General")) {
                ImGui::Text("Console type:");
                ImGui::SameLine();
                if (ImGui::Combo("##ConsoleType", &consoleType, consoleItems, IM_ARRAYSIZE(consoleItems))) {
                    globalCfg.SetInt("Emu.ConsoleType", consoleType);
                    emuInstance->saveConfig();
                }
                if (ImGui::Checkbox("Boot game directly", &directBoot))
                    globalCfg.SetBool("Emu.DirectBoot", directBoot);
                ImGui::EndTabItem();
            }
            // DS-mode Tab
            if (ImGui::BeginTabItem("DS-mode")) {
                if (ImGui::Checkbox("Use external BIOS/firmware files", &externalBIOS))
                    globalCfg.SetBool("Emu.ExternalBIOSEnable", externalBIOS);
                bool disabled = !externalBIOS;
                ImGui::BeginDisabled(disabled);
                ImGui::Text("DS ARM9 BIOS:");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(350);
                char bios9PathBuf[512];
                strncpy(bios9PathBuf, bios9Path.c_str(), sizeof(bios9PathBuf) - 1);
                bios9PathBuf[sizeof(bios9PathBuf) - 1] = '\0';
                if (ImGui::InputText("##DSBIOS9Path", bios9PathBuf, sizeof(bios9PathBuf))) {
                    bios9Path = bios9PathBuf;
                    globalCfg.SetString("DS.BIOS9Path", bios9Path);
                }
                ImGui::SameLine();
                if (ImGui::Button("Browse##DSBIOS9")) {
                    std::string file = FileDialog::openFile("Select DS ARM9 BIOS", bios9Path, FileDialog::Filters::BIOS_FILES);
                    if (!file.empty()) {
                        bios9Path = file;
                        globalCfg.SetString("DS.BIOS9Path", bios9Path);
                    }
                }
                ImGui::Text("DS ARM7 BIOS:");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(350);
                char bios7PathBuf[512];
                strncpy(bios7PathBuf, bios7Path.c_str(), sizeof(bios7PathBuf) - 1);
                bios7PathBuf[sizeof(bios7PathBuf) - 1] = '\0';
                if (ImGui::InputText("##DSBIOS7Path", bios7PathBuf, sizeof(bios7PathBuf))) {
                    bios7Path = bios7PathBuf;
                    globalCfg.SetString("DS.BIOS7Path", bios7Path);
                }
                ImGui::SameLine();
                if (ImGui::Button("Browse##DSBIOS7")) {
                    std::string file = FileDialog::openFile("Select DS ARM7 BIOS", bios7Path, FileDialog::Filters::BIOS_FILES);
                    if (!file.empty()) {
                        bios7Path = file;
                        globalCfg.SetString("DS.BIOS7Path", bios7Path);
                    }
                }
                ImGui::Text("DS firmware:");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(350);
                char firmwarePathBuf[512];
                strncpy(firmwarePathBuf, firmwarePath.c_str(), sizeof(firmwarePathBuf) - 1);
                firmwarePathBuf[sizeof(firmwarePathBuf) - 1] = '\0';
                if (ImGui::InputText("##DSFirmwarePath", firmwarePathBuf, sizeof(firmwarePathBuf))) {
                    firmwarePath = firmwarePathBuf;
                    globalCfg.SetString("DS.FirmwarePath", firmwarePath);
                }
                ImGui::SameLine();
                if (ImGui::Button("Browse##DSFirmware")) {
                    std::string file = FileDialog::openFile("Select DS firmware", firmwarePath, FileDialog::Filters::FIRMWARE_FILES);
                    if (!file.empty()) {
                        firmwarePath = file;
                        globalCfg.SetString("DS.FirmwarePath", firmwarePath);
                    }
                }
                ImGui::EndDisabled();
                ImGui::EndTabItem();
            }
            // DSi-mode Tab
            if (ImGui::BeginTabItem("DSi-mode")) {
                ImGui::Text("DSi ARM9 BIOS:");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(350);
                char dsiBios9PathBuf[512];
                strncpy(dsiBios9PathBuf, dsiBios9Path.c_str(), sizeof(dsiBios9PathBuf) - 1);
                dsiBios9PathBuf[sizeof(dsiBios9PathBuf) - 1] = '\0';
                if (ImGui::InputText("##DSiBIOS9Path", dsiBios9PathBuf, sizeof(dsiBios9PathBuf))) {
                    dsiBios9Path = dsiBios9PathBuf;
                    globalCfg.SetString("DSi.BIOS9Path", dsiBios9Path);
                }
                ImGui::SameLine();
                if (ImGui::Button("Browse##DSiBIOS9")) {
                    std::string file = FileDialog::openFile("Select DSi ARM9 BIOS", dsiBios9Path, FileDialog::Filters::BIOS_FILES);
                    if (!file.empty()) {
                        dsiBios9Path = file;
                        globalCfg.SetString("DSi.BIOS9Path", dsiBios9Path);
                    }
                }
                ImGui::Text("DSi ARM7 BIOS:");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(350);
                char dsiBios7PathBuf[512];
                strncpy(dsiBios7PathBuf, dsiBios7Path.c_str(), sizeof(dsiBios7PathBuf) - 1);
                dsiBios7PathBuf[sizeof(dsiBios7PathBuf) - 1] = '\0';
                if (ImGui::InputText("##DSiBIOS7Path", dsiBios7PathBuf, sizeof(dsiBios7PathBuf))) {
                    dsiBios7Path = dsiBios7PathBuf;
                    globalCfg.SetString("DSi.BIOS7Path", dsiBios7Path);
                }
                ImGui::SameLine();
                if (ImGui::Button("Browse##DSiBIOS7")) {
                    std::string file = FileDialog::openFile("Select DSi ARM7 BIOS", dsiBios7Path, FileDialog::Filters::BIOS_FILES);
                    if (!file.empty()) {
                        dsiBios7Path = file;
                        globalCfg.SetString("DSi.BIOS7Path", dsiBios7Path);
                    }
                }
                ImGui::Text("DSi firmware:");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(350);
                char dsiFirmwarePathBuf[512];
                strncpy(dsiFirmwarePathBuf, dsiFirmwarePath.c_str(), sizeof(dsiFirmwarePathBuf) - 1);
                dsiFirmwarePathBuf[sizeof(dsiFirmwarePathBuf) - 1] = '\0';
                if (ImGui::InputText("##DSiFirmwarePath", dsiFirmwarePathBuf, sizeof(dsiFirmwarePathBuf))) {
                    dsiFirmwarePath = dsiFirmwarePathBuf;
                    globalCfg.SetString("DSi.FirmwarePath", dsiFirmwarePath);
                }
                ImGui::SameLine();
                if (ImGui::Button("Browse##DSiFirmware")) {
                    std::string file = FileDialog::openFile("Select DSi firmware", dsiFirmwarePath, FileDialog::Filters::FIRMWARE_FILES);
                    if (!file.empty()) {
                        dsiFirmwarePath = file;
                        globalCfg.SetString("DSi.FirmwarePath", dsiFirmwarePath);
                    }
                }
                ImGui::Text("DSi NAND:");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(350);
                char dsiNANDPathBuf[512];
                strncpy(dsiNANDPathBuf, dsiNANDPath.c_str(), sizeof(dsiNANDPathBuf) - 1);
                dsiNANDPathBuf[sizeof(dsiNANDPathBuf) - 1] = '\0';
                if (ImGui::InputText("##DSiNANDPath", dsiNANDPathBuf, sizeof(dsiNANDPathBuf))) {
                    dsiNANDPath = dsiNANDPathBuf;
                    globalCfg.SetString("DSi.NANDPath", dsiNANDPath);
                }
                ImGui::SameLine();
                if (ImGui::Button("Browse##DSiNAND")) {
                    std::string file = FileDialog::openFile("Select DSi NAND", dsiNANDPath, FileDialog::Filters::ALL_FILES);
                    if (!file.empty()) {
                        dsiNANDPath = file;
                        globalCfg.SetString("DSi.NANDPath", dsiNANDPath);
                    }
                }
                if (ImGui::Checkbox("Full BIOS Boot (requires all DSi BIOS, firmware, NAND)", &dsiFullBIOSBoot)) {
                    globalCfg.SetBool("DSi.FullBIOSBoot", dsiFullBIOSBoot);
                    emuInstance->saveConfig();
                }
                ImGui::Text("DSi mode requires external DSi BIOS/firmware/NAND");
                if (ImGui::Checkbox("Enable DSi SD card", &dsiSDEnable))
                    globalCfg.SetBool("DSi.SD.Enable", dsiSDEnable);
                ImGui::BeginDisabled(!dsiSDEnable);
                ImGui::Text("SD card image:");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(350);
                char dsiSDPathBuf[512];
                strncpy(dsiSDPathBuf, dsiSDPath.c_str(), sizeof(dsiSDPathBuf) - 1);
                dsiSDPathBuf[sizeof(dsiSDPathBuf) - 1] = '\0';
                if (ImGui::InputText("##DSiSDPath", dsiSDPathBuf, sizeof(dsiSDPathBuf))) {
                    dsiSDPath = dsiSDPathBuf;
                    globalCfg.SetString("DSi.SD.ImagePath", dsiSDPath);
                    Config::Save();
                }
                ImGui::SameLine();
                if (ImGui::Button("Browse##DSiSD")) {
                    std::string file = FileDialog::openFile("Select DSi SD image", dsiSDPath, FileDialog::Filters::IMAGE_FILES);
                    if (!file.empty()) {
                        dsiSDPath = file;
                        globalCfg.SetString("DSi.SD.ImagePath", dsiSDPath);
                        Config::Save();
                    }
                }
                int dsiSDSize = globalCfg.GetInt("DSi.SD.ImageSize");
                if (ImGui::Combo("Image size", &dsiSDSize, dsiSDSizeItems, IM_ARRAYSIZE(dsiSDSizeItems)))
                    globalCfg.SetInt("DSi.SD.ImageSize", dsiSDSize);
                if (ImGui::Checkbox("Read-only SD", &dsiSDReadOnly))
                    globalCfg.SetBool("DSi.SD.ReadOnly", dsiSDReadOnly);
                if (ImGui::Checkbox("Sync SD to folder", &dsiSDFolder))
                    globalCfg.SetBool("DSi.SD.FolderSync", dsiSDFolder);
                ImGui::BeginDisabled(!dsiSDFolder);
                ImGui::Text("Folder path:");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(350);
                char dsiSDFolderPathBuf[512];
                strncpy(dsiSDFolderPathBuf, dsiSDFolderPath.c_str(), sizeof(dsiSDFolderPathBuf) - 1);
                dsiSDFolderPathBuf[sizeof(dsiSDFolderPathBuf) - 1] = '\0';
                if (ImGui::InputText("##DSiSDFolderPath", dsiSDFolderPathBuf, sizeof(dsiSDFolderPathBuf))) {
                    dsiSDFolderPath = dsiSDFolderPathBuf;
                    globalCfg.SetString("DSi.SD.FolderPath", dsiSDFolderPath);
                }
                ImGui::SameLine();
                if (ImGui::Button("Browse##DSiSDFolder")) {
                    std::string folder = FileDialog::openFolder("Select DSi SD folder", dsiSDFolderPath);
                    if (!folder.empty()) {
                        dsiSDFolderPath = folder;
                        globalCfg.SetString("DSi.SD.FolderPath", dsiSDFolderPath);
                        Config::Save();
                    }
                }
                ImGui::EndDisabled();
                ImGui::EndDisabled();
                ImGui::EndTabItem();
            }
            // CPU emulation Tab
            if (ImGui::BeginTabItem("CPU emulation")) {
                if (ImGui::Checkbox("Enable JIT recompiler", &jitEnable))
                    globalCfg.SetBool("JIT.Enable", jitEnable);
                ImGui::Text("Maximum JIT block size:");
                ImGui::SameLine();
                if (ImGui::SliderInt("##JITBlockSize", &jitMaxBlockSize, 1, 32))
                    globalCfg.SetInt("JIT.MaxBlockSize", jitMaxBlockSize);
                if (ImGui::Checkbox("Branch optimisations", &jitBranchOpt))
                    globalCfg.SetBool("JIT.BranchOptimisations", jitBranchOpt);
                if (ImGui::Checkbox("Literal optimisations", &jitLiteralOpt))
                    globalCfg.SetBool("JIT.LiteralOptimisations", jitLiteralOpt);
                if (ImGui::Checkbox("Fast memory", &jitFastMemory))
                    globalCfg.SetBool("JIT.FastMemory", jitFastMemory);
                ImGui::EndTabItem();
            }
            // DLDI Tab
            if (ImGui::BeginTabItem("DLDI")) {
                if (ImGui::Checkbox("Enable DLDI (for homebrew)", &dldiEnable))
                    globalCfg.SetBool("DLDI.Enable", dldiEnable);
                ImGui::BeginDisabled(!dldiEnable);
                ImGui::Text("SD card image:");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(350);
                char dldiSDPathBuf[512];
                strncpy(dldiSDPathBuf, dldiSDPath.c_str(), sizeof(dldiSDPathBuf) - 1);
                dldiSDPathBuf[sizeof(dldiSDPathBuf) - 1] = '\0';
                if (ImGui::InputText("##DLDISDPath", dldiSDPathBuf, sizeof(dldiSDPathBuf))) {
                    dldiSDPath = dldiSDPathBuf;
                    globalCfg.SetString("DLDI.ImagePath", dldiSDPath);
                    Config::Save();
                }
                ImGui::SameLine();
                if (ImGui::Button("Browse##DLDISD")) {
                    std::string file = FileDialog::openFile("Select DLDI SD image", dldiSDPath, FileDialog::Filters::IMAGE_FILES);
                    if (!file.empty()) {
                        dldiSDPath = file;
                        globalCfg.SetString("DLDI.ImagePath", dldiSDPath);
                        Config::Save();
                    }
                }
                int dldiImageSize = globalCfg.GetInt("DLDI.ImageSize");
                if (ImGui::Combo("Image size", &dldiImageSize, dldiSizeItems, IM_ARRAYSIZE(dldiSizeItems)))
                    globalCfg.SetInt("DLDI.ImageSize", dldiImageSize);
                if (ImGui::Checkbox("Read-only SD", &dldiReadOnly))
                    globalCfg.SetBool("DLDI.ReadOnly", dldiReadOnly);
                if (ImGui::Checkbox("Sync SD to folder", &dldiFolder))
                    globalCfg.SetBool("DLDI.FolderSync", dldiFolder);
                ImGui::BeginDisabled(!dldiFolder);
                ImGui::Text("Folder path:");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(350);
                char dldiFolderPathBuf[512];
                strncpy(dldiFolderPathBuf, dldiFolderPath.c_str(), sizeof(dldiFolderPathBuf) - 1);
                dldiFolderPathBuf[sizeof(dldiFolderPathBuf) - 1] = '\0';
                if (ImGui::InputText("##DLDIFolderPath", dldiFolderPathBuf, sizeof(dldiFolderPathBuf))) {
                    dldiFolderPath = dldiFolderPathBuf;
                    globalCfg.SetString("DLDI.FolderPath", dldiFolderPath);
                }
                ImGui::SameLine();
                if (ImGui::Button("Browse##DLDIFolder")) {
                    std::string folder = FileDialog::openFolder("Select DLDI folder", dldiFolderPath);
                    if (!folder.empty()) {
                        dldiFolderPath = folder;
                        globalCfg.SetString("DLDI.FolderPath", dldiFolderPath);
                        Config::Save();
                    }
                }
                ImGui::EndDisabled();
                ImGui::EndDisabled();
                ImGui::EndTabItem();
            }
            // Devtools Tab
            if (ImGui::BeginTabItem("Devtools")) {
                if (ImGui::Checkbox("Enable GDB stub", &gdbEnabled))
                    localCfg.SetBool("Gdb.Enabled", gdbEnabled);
                ImGui::BeginDisabled(!gdbEnabled);
                ImGui::Text("ARM9 port");
                ImGui::SameLine();
                if (ImGui::InputInt("##GdbPortA9", &gdbPortA9))
                    localCfg.SetInt("Gdb.ARM9.Port", gdbPortA9);
                ImGui::SameLine();
                if (ImGui::Checkbox("Break on startup##A9", &gdbBOSA9))
                    localCfg.SetBool("Gdb.ARM9.BreakOnStartup", gdbBOSA9);
                ImGui::Text("ARM7 port");
                ImGui::SameLine();
                if (ImGui::InputInt("##GdbPortA7", &gdbPortA7))
                    localCfg.SetInt("Gdb.ARM7.Port", gdbPortA7);
                ImGui::SameLine();
                if (ImGui::Checkbox("Break on startup##A7", &gdbBOSA7))
                    localCfg.SetBool("Gdb.ARM7.BreakOnStartup", gdbBOSA7);
                ImGui::EndDisabled();
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Note: GDB stub cannot be used together with the JIT recompiler");
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Note: melonDS must be restarted in order for these changes to have effect");
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::End();
    }
}

void ImGuiFrontend::renderInputConfigDialog()
{
    if (!showInputConfigDialog) return;
    ImGui::Begin("Input Configuration", &showInputConfigDialog, SettingsDialogFlags);
    
    auto& cfg = emuInstance->getGlobalConfig();
    auto keycfg = cfg.GetTable("Keyboard");
    auto joycfg = cfg.GetTable("Joystick");
    
    static bool configLoaded = false;
    if (!configLoaded)
    {
        loadInputConfig();
        configLoaded = true;
    }
    
    ImGui::Text("Controller");
    ImGui::Separator();
    
    updateJoystickList();
    
    const char* joystickPreview = selectedJoystickID >= 0 && selectedJoystickID < availableJoysticks.size() 
        ? availableJoysticks[selectedJoystickID].c_str() 
        : "(no controller)";
        
    if (ImGui::BeginCombo("Controller", joystickPreview))
    {
        for (int i = 0; i < availableJoysticks.size(); i++)
        {
            bool isSelected = (selectedJoystickID == i);
            if (ImGui::Selectable(availableJoysticks[i].c_str(), isSelected))
            {
                selectedJoystickID = i;
                joycfg.SetInt("JoystickID", selectedJoystickID);
                if (emuInstance) {
                    emuInstance->setJoystick(selectedJoystickID);
                }
            }
            if (isSelected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    
    ImGui::Spacing();
    
    if (ImGui::BeginTabBar("InputConfigTabs"))
    {
        // DS Controls tab
        if (ImGui::BeginTabItem("DS keypad"))
        {
            renderDSControlsTab();
            ImGui::EndTabItem();
        }
        
        // General Hotkeys tab
        if (ImGui::BeginTabItem("Hotkeys"))
        {
            renderHotkeysTab();
            ImGui::EndTabItem();
        }
        
        // Add-ons tab (Boktai, Guitar Grip)
        if (ImGui::BeginTabItem("Add-ons"))
        {
            renderAddonsTab();
            ImGui::EndTabItem();
        }
        
        ImGui::EndTabBar();
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    
    // Input capture handling
    if (isMappingInput)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
        ImGui::Text("Press a key or controller button for %s", mappingButtonLabel.c_str());
        ImGui::Text("Press Escape to cancel, Backspace to clear");
        ImGui::PopStyleColor();
        
        handleInputCapture();
    }
    
    if (ImGui::Button("OK"))
    {
        saveInputConfig();
        showInputConfigDialog = false;
        configLoaded = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel"))
    {
        showInputConfigDialog = false;
        configLoaded = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Apply"))
    {
        saveInputConfig();
    }
    ImGui::End();
}

void ImGuiFrontend::renderVideoSettingsDialog()
{
    if (!showVideoSettingsDialog) return;
    ImGui::Begin("Video Settings", &showVideoSettingsDialog, SettingsDialogFlags);
    
    auto& cfg = emuInstance->getGlobalConfig();
    
    ImGui::Text("3D Renderer");
    ImGui::Separator();
    
    int renderer = cfg.GetInt("3D.Renderer");
    
    if (ImGui::RadioButton("Software renderer##Renderer", renderer == 0))
    {
        cfg.SetInt("3D.Renderer", 0);
    }
    
#ifdef OGLRENDERER_ENABLED
    if (ImGui::RadioButton("OpenGL renderer##Renderer", renderer == 1))
    {
        cfg.SetInt("3D.Renderer", 1);
    }
    
#ifndef __APPLE__
    if (ImGui::RadioButton("OpenGL Compute renderer##Renderer", renderer == 2))
    {
        cfg.SetInt("3D.Renderer", 2);
    }
#else
    ImGui::BeginDisabled();
    ImGui::RadioButton("OpenGL Compute renderer##Renderer", false);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextDisabled("(not available on macOS)");
#endif
#else
    ImGui::BeginDisabled();
    ImGui::RadioButton("OpenGL renderer##Renderer", false);
    ImGui::RadioButton("OpenGL Compute renderer##Renderer", false);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextDisabled("(OpenGL support not compiled)");
#endif
    
    ImGui::Spacing();
    
    if (renderer == 0) // Software
    {
        ImGui::Text("Software Renderer");
        ImGui::Separator();
        
        bool softwareThreaded = cfg.GetBool("3D.Soft.Threaded");
        if (ImGui::Checkbox("Threaded software renderer", &softwareThreaded))
        {
            cfg.SetBool("3D.Soft.Threaded", softwareThreaded);
        }
        
        ImGui::Spacing();
    }
    
    ImGui::Text("Display");
    ImGui::Separator();
    
    if (renderer != 0) ImGui::BeginDisabled();
    
    bool glDisplay = cfg.GetBool("Screen.UseGL");
    if (ImGui::Checkbox("Use OpenGL for main screen display", &glDisplay))
    {
        cfg.SetBool("Screen.UseGL", glDisplay);
    }
    
    if (renderer != 0) ImGui::EndDisabled();
    
    ImGui::Spacing();
    
    // VSync settings
    bool usesGL = glDisplay || (renderer != 0);
    if (!usesGL) ImGui::BeginDisabled();
    
    bool vsync = cfg.GetBool("Screen.VSync");
    if (ImGui::Checkbox("VSync", &vsync))
    {
        cfg.SetBool("Screen.VSync", vsync);
    }
    
    if (usesGL && vsync)
    {
        int vsyncInterval = cfg.GetInt("Screen.VSyncInterval");
        if (ImGui::SliderInt("VSync interval", &vsyncInterval, 1, 20))
        {
            cfg.SetInt("Screen.VSyncInterval", vsyncInterval);
        }
    }
    
    if (!usesGL) ImGui::EndDisabled();
    
    if (renderer == 1 || renderer == 2)
    {
        ImGui::Spacing();
        ImGui::Text("OpenGL Renderer");
        ImGui::Separator();
        
        int scaleFactor = cfg.GetInt("3D.GL.ScaleFactor");
        const char* scaleNames[] = { 
            "1x native (256x192)", "2x native (512x384)", "3x native (768x576)", "4x native (1024x768)",
            "5x native (1280x960)", "6x native (1536x1152)", "7x native (1792x1344)", "8x native (2048x1536)",
            "9x native (2304x1728)", "10x native (2560x1920)", "11x native (2816x2112)", "12x native (3072x2304)",
            "13x native (3328x2496)", "14x native (3584x2688)", "15x native (3840x2880)", "16x native (4096x3072)"
        };
        int scaleIndex = scaleFactor - 1;
        if (ImGui::Combo("Internal resolution", &scaleIndex, scaleNames, 16))
        {
            cfg.SetInt("3D.GL.ScaleFactor", scaleIndex + 1);
        }
        
        if (renderer == 1)
        {
            bool betterPolygons = cfg.GetBool("3D.GL.BetterPolygons");
            if (ImGui::Checkbox("Improved polygon splitting", &betterPolygons))
            {
                cfg.SetBool("3D.GL.BetterPolygons", betterPolygons);
            }
            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Reduces Z-fighting in some games, but may affect performance");
            }
        }
        
        if (renderer == 2)
        {
            bool hiresCoords = cfg.GetBool("3D.GL.HiresCoordinates");
            if (ImGui::Checkbox("High-resolution coordinates", &hiresCoords))
            {
                cfg.SetBool("3D.GL.HiresCoordinates", hiresCoords);
            }
            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Uses more accurate vertex coordinates for sub-pixel precision");
            }
        }
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    
    if (ImGui::Button("Close"))
    {
        Config::Save();
        showVideoSettingsDialog = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Apply"))
    {
        Config::Save();
    }
    ImGui::End();
}

void ImGuiFrontend::renderAudioSettingsDialog()
{
    if (!showAudioSettingsDialog) return;
    ImGui::Begin("Audio Settings", &showAudioSettingsDialog, SettingsDialogFlags);
    
    auto& cfg = emuInstance->getGlobalConfig();
    
    ImGui::Text("Audio Configuration");
    ImGui::Separator();
    
    int interpolation = cfg.GetInt("Audio.Interpolation");
    const char* interpolationModes[] = { "Linear", "Cosine", "Cubic" };
    if (ImGui::Combo("Interpolation##Audio", &interpolation, interpolationModes, 3))
    {
        cfg.SetInt("Audio.Interpolation", interpolation);
    }
    
    int bitDepth = cfg.GetInt("Audio.BitDepth");
    const char* bitDepths[] = { "16-bit", "24-bit", "32-bit" };
    if (ImGui::Combo("Bit Depth##Audio", &bitDepth, bitDepths, 3))
    {
        cfg.SetInt("Audio.BitDepth", bitDepth);
    }
    
    int volume = cfg.GetInt("Audio.Volume");
    if (ImGui::SliderInt("Volume", &volume, 0, 256))
    {
        cfg.SetInt("Audio.Volume", volume);
    }
    
    bool dsiVolumeSync = cfg.GetBool("Audio.DSiVolumeSync");
    if (ImGui::Checkbox("DSi Volume Sync", &dsiVolumeSync))
    {
        cfg.SetBool("Audio.DSiVolumeSync", dsiVolumeSync);
    }
    
    ImGui::Spacing();
    ImGui::Text("Microphone Settings");
    ImGui::Separator();
    
    int micInputType = cfg.GetInt("Mic.InputType");
    const char* micInputTypes[] = { "None", "WAV File", "Physical Device" };
    if (ImGui::Combo("Input Type##Mic", &micInputType, micInputTypes, 3))
    {
        cfg.SetInt("Mic.InputType", micInputType);
    }
    
    if (micInputType == 1)
    {
        std::string micWavPath = cfg.GetString("Mic.WavPath");
        char micWavBuffer[512];
        strncpy(micWavBuffer, micWavPath.c_str(), sizeof(micWavBuffer));
        if (ImGui::InputText("WAV File Path", micWavBuffer, sizeof(micWavBuffer)))
        {
            cfg.SetString("Mic.WavPath", micWavBuffer);
        }
        ImGui::SameLine();
        if (ImGui::Button("Browse##MicWav"))
        {
            std::string wavFile = FileDialog::openFile(
                "Select WAV File", 
                micWavBuffer,
                FileDialog::Filters::WAV_FILES
            );
            if (!wavFile.empty())
            {
                strncpy(micWavBuffer, wavFile.c_str(), sizeof(micWavBuffer));
                cfg.SetString("Mic.WavPath", wavFile);
            }
        }
    }
    else if (micInputType == 2)
    {
        std::string micDevice = cfg.GetString("Mic.Device");
        char micDeviceBuffer[512];
        strncpy(micDeviceBuffer, micDevice.c_str(), sizeof(micDeviceBuffer));
        if (ImGui::InputText("Device Name", micDeviceBuffer, sizeof(micDeviceBuffer)))
        {
            cfg.SetString("Mic.Device", micDeviceBuffer);
        }
    }
    
    ImGui::Spacing();
    if (ImGui::Button("Close"))
    {
        Config::Save();
        showAudioSettingsDialog = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Apply"))
    {
        Config::Save();
    }
    ImGui::End();
}

void ImGuiFrontend::renderWifiSettingsDialog()
{
    if (!showWifiSettingsDialog) return;
    ImGui::Begin("WiFi Settings", &showWifiSettingsDialog, SettingsDialogFlags);
    
    auto& cfg = emuInstance->getGlobalConfig();
    
    ImGui::End();
}

void ImGuiFrontend::renderFirmwareSettingsDialog()
{
    if (!showFirmwareSettingsDialog) return;
    ImGui::Begin("Firmware Settings", &showFirmwareSettingsDialog, SettingsDialogFlags);
    
    auto& cfg = emuInstance->getGlobalConfig();
    
    ImGui::End();
}

void ImGuiFrontend::renderPathSettingsDialog()
{
    if (!showPathSettingsDialog) return;
    ImGui::Begin("Path Settings", &showPathSettingsDialog, SettingsDialogFlags);
    
    auto& cfg = emuInstance->getGlobalConfig();
    
    ImGui::Text("File Paths Configuration");
    ImGui::Separator();
    
    std::string saveFilePath = cfg.GetString("SaveFilePath");
    char saveFileBuffer[512];
    strncpy(saveFileBuffer, saveFilePath.c_str(), sizeof(saveFileBuffer));
    ImGui::SetNextItemWidth(400);
    if (ImGui::InputText("Save files", saveFileBuffer, sizeof(saveFileBuffer)))
    {
        cfg.SetString("SaveFilePath", saveFileBuffer);
    }
    ImGui::SameLine();
    if (ImGui::Button("Browse##SaveFile"))
    {
        std::string path = FileDialog::openFolder("Select Save Files Directory");
        if (!path.empty())
        {
            strncpy(saveFileBuffer, path.c_str(), sizeof(saveFileBuffer));
            cfg.SetString("SaveFilePath", path);
        }
    }
    
    std::string savestatePath = cfg.GetString("SavestatePath");
    char savestateBuffer[512];
    strncpy(savestateBuffer, savestatePath.c_str(), sizeof(savestateBuffer));
    ImGui::SetNextItemWidth(400);
    if (ImGui::InputText("Savestates", savestateBuffer, sizeof(savestateBuffer)))
    {
        cfg.SetString("SavestatePath", savestateBuffer);
    }
    ImGui::SameLine();
    if (ImGui::Button("Browse##Savestate"))
    {
        std::string path = FileDialog::openFolder("Select Savestates Directory");
        if (!path.empty())
        {
            strncpy(savestateBuffer, path.c_str(), sizeof(savestateBuffer));
            cfg.SetString("SavestatePath", path);
        }
    }
    
    std::string cheatFilePath = cfg.GetString("CheatFilePath");
    char cheatFileBuffer[512];
    strncpy(cheatFileBuffer, cheatFilePath.c_str(), sizeof(cheatFileBuffer));
    ImGui::SetNextItemWidth(400);
    if (ImGui::InputText("Cheat files", cheatFileBuffer, sizeof(cheatFileBuffer)))
    {
        cfg.SetString("CheatFilePath", cheatFileBuffer);
    }
    ImGui::SameLine();
    if (ImGui::Button("Browse##CheatFile"))
    {
        std::string path = FileDialog::openFolder("Select Cheat Files Directory");
        if (!path.empty())
        {
            strncpy(cheatFileBuffer, path.c_str(), sizeof(cheatFileBuffer));
            cfg.SetString("CheatFilePath", path);
        }
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    
    if (ImGui::Button("Close"))
    {
        emuInstance->saveConfig();
        showPathSettingsDialog = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Apply"))
    {
        emuInstance->saveConfig();
    }
    
    ImGui::End();
}

void ImGuiFrontend::renderInterfaceSettingsDialog()
{
    if (!showInterfaceSettingsDialog) return;
    ImGui::Begin("Interface Settings", &showInterfaceSettingsDialog, SettingsDialogFlags);
    
    auto& cfg = emuInstance->getGlobalConfig();
    
    ImGui::Text("Mouse and Input Settings");
    ImGui::Separator();
    
    bool mouseHide = cfg.GetBool("Mouse.Hide");
    if (ImGui::Checkbox("Auto-hide mouse cursor", &mouseHide))
    {
        cfg.SetBool("Mouse.Hide", mouseHide);
    }
    
    if (mouseHide)
    {
        int hideSeconds = cfg.GetInt("Mouse.HideSeconds");
        ImGui::SetNextItemWidth(100);
        if (ImGui::InputInt("Hide after (seconds)", &hideSeconds))
        {
            if (hideSeconds < 1) hideSeconds = 1;
            cfg.SetInt("Mouse.HideSeconds", hideSeconds);
        }
    }
    
    bool pauseLostFocus = cfg.GetBool("PauseLostFocus");
    if (ImGui::Checkbox("Pause when window loses focus", &pauseLostFocus))
    {
        cfg.SetBool("PauseLostFocus", pauseLostFocus);
    }
    
    ImGui::Spacing();
    ImGui::Text("Performance Settings");
    ImGui::Separator();
    
    double targetFPS = cfg.GetDouble("TargetFPS");
    ImGui::SetNextItemWidth(150);
    if (ImGui::InputDouble("Target FPS", &targetFPS, 0.1, 1.0, "%.4f"))
    {
        if (targetFPS <= 0.0) targetFPS = 0.0001;
        cfg.SetDouble("TargetFPS", targetFPS);
    }
    
    ImGui::SameLine();
    if (ImGui::Button("60.0000"))
    {
        cfg.SetDouble("TargetFPS", 60.0);
        targetFPS = 60.0;
    }
    ImGui::SameLine();
    if (ImGui::Button("59.8261"))
    {
        cfg.SetDouble("TargetFPS", 59.8261);
        targetFPS = 59.8261;
    }
    ImGui::SameLine();
    if (ImGui::Button("30.0000"))
    {
        cfg.SetDouble("TargetFPS", 30.0);
        targetFPS = 30.0;
    }
    
    double fastForwardFPS = cfg.GetDouble("FastForwardFPS");
    ImGui::SetNextItemWidth(150);
    if (ImGui::InputDouble("Fast Forward FPS", &fastForwardFPS, 1.0, 10.0, "%.1f"))
    {
        if (fastForwardFPS <= 0.0) fastForwardFPS = 0.0001;
        cfg.SetDouble("FastForwardFPS", fastForwardFPS);
    }
    
    ImGui::SameLine();
    if (ImGui::Button("2x"))
    {
        cfg.SetDouble("FastForwardFPS", targetFPS * 2.0);
    }
    ImGui::SameLine();
    if (ImGui::Button("3x"))
    {
        cfg.SetDouble("FastForwardFPS", targetFPS * 3.0);
    }
    ImGui::SameLine();
    if (ImGui::Button("MAX"))
    {
        cfg.SetDouble("FastForwardFPS", 1000.0);
    }
    
    double slowmoFPS = cfg.GetDouble("SlowmoFPS");
    ImGui::SetNextItemWidth(150);
    if (ImGui::InputDouble("Slow Motion FPS", &slowmoFPS, 0.1, 1.0, "%.4f"))
    {
        if (slowmoFPS <= 0.0) slowmoFPS = 0.0001;
        cfg.SetDouble("SlowmoFPS", slowmoFPS);
    }
    
    ImGui::SameLine();
    if (ImGui::Button("1/2x"))
    {
        cfg.SetDouble("SlowmoFPS", targetFPS / 2.0);
    }
    ImGui::SameLine();
    if (ImGui::Button("1/4x"))
    {
        cfg.SetDouble("SlowmoFPS", targetFPS / 4.0);
    }
    
    ImGui::Spacing();
    ImGui::Text("UI Customization");
    ImGui::Separator();
    
    ImGui::Text("Theme:");
    const char* themeNames[] = {"Dark", "Light", "Classic", "Ocean", "Forest", "Cherry", "Purple", "Custom"};
    int currentThemeIndex = static_cast<int>(currentTheme);
    if (ImGui::Combo("##Theme", &currentThemeIndex, themeNames, IM_ARRAYSIZE(themeNames)))
    {
        setTheme(static_cast<ThemeStyle>(currentThemeIndex));
    }
    
    ImGui::Spacing();
    
    ImGui::Text("Font Size:");
    const char* fontSizeNames[] = {"Small", "Normal", "Large", "Extra Large"};
    int currentFontSizeIndex = static_cast<int>(currentFontSize);
    if (ImGui::Combo("##FontSize", &currentFontSizeIndex, fontSizeNames, IM_ARRAYSIZE(fontSizeNames)))
    {
        setFontSize(static_cast<FontSize>(currentFontSizeIndex));
    }
    
    ImGui::Spacing();
    
    ImGui::Text("Font: Default System Font");
    ImGui::TextDisabled("Using the default system font for optimal compatibility");
    
    ImGui::Spacing();
    ImGui::Separator();
    
    if (ImGui::Button("Close"))
    {
        emuInstance->saveConfig();
        showInterfaceSettingsDialog = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Apply"))
    {
        emuInstance->saveConfig();
    }
    
    ImGui::End();
}

void ImGuiFrontend::renderPowerManagementDialog()
{
    if (!showPowerManagementDialog) return;
    ImGui::Begin("Power Management", &showPowerManagementDialog, SettingsDialogFlags);
    
    if (!emuInstance->isRunning())
    {
        ImGui::Text("Console must be running to adjust power settings");
        ImGui::End();
        return;
    }
    
    auto& cfg = emuInstance->getGlobalConfig();
    bool needsSave = false;
    
    if (emuInstance->getConsoleType() == 1) // DSi
    {
        ImGui::Text("DSi Battery Settings");
        ImGui::Separator();
        
        int batteryLevel = cfg.GetInt("DSi.Battery.Level");
        const char* batteryLabels[] = {"Almost Empty", "Low", "Half", "Three Quarters", "Full"};
        if (ImGui::SliderInt("Battery Level", &batteryLevel, 0, 4, batteryLabels[batteryLevel]))
        {
            cfg.SetInt("DSi.Battery.Level", batteryLevel);
            needsSave = true;
        }
        
        bool batteryCharging = cfg.GetBool("DSi.Battery.Charging");
        if (ImGui::Checkbox("Battery Charging", &batteryCharging))
        {
            cfg.SetBool("DSi.Battery.Charging", batteryCharging);
            needsSave = true;
        }
    }
    else // DS
    {
        ImGui::Text("DS Battery Settings");
        ImGui::Separator();
        
        bool batteryOkay = cfg.GetBool("DS.Battery.LevelOkay");
        if (ImGui::RadioButton("Battery Okay##DSBattery", batteryOkay))
        {
            if (!batteryOkay)
            {
                cfg.SetBool("DS.Battery.LevelOkay", true);
                needsSave = true;
            }
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Battery Low##DSBattery", !batteryOkay))
        {
            if (batteryOkay)
            {
                cfg.SetBool("DS.Battery.LevelOkay", false);
                needsSave = true;
            }
        }
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    
    if (ImGui::Button("Close"))
    {
        if (needsSave) emuInstance->saveConfig();
        showPowerManagementDialog = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Apply"))
    {
        if (needsSave) emuInstance->saveConfig();
    }
    
    ImGui::End();
}

void ImGuiFrontend::renderDateTimeDialog()
{
    if (!showDateTimeDialog) return;
    ImGui::Begin("Date/Time Settings", &showDateTimeDialog, SettingsDialogFlags);
    
    auto& cfg = emuInstance->getGlobalConfig();
    
    ImGui::Text("Real-Time Clock Settings");
    ImGui::Separator();
    
    int64_t currentOffset = cfg.GetInt64("RTC.Offset");
    
    auto now = std::chrono::system_clock::now();
    auto time_with_offset = now + std::chrono::seconds(currentOffset);
    auto time_t_with_offset = std::chrono::system_clock::to_time_t(time_with_offset);
    auto tm_with_offset = *std::localtime(&time_t_with_offset);
    
    char timeStr[64];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &tm_with_offset);
    ImGui::Text("Current DS Time: %s", timeStr);
    
    ImGui::Spacing();
    
    if (ImGui::Button("Reset to System Time"))
    {
        cfg.SetInt64("RTC.Offset", 0);
        emuInstance->saveConfig();
    }
    
    ImGui::SameLine();
    
    static int timeAdjustHours = 0;
    static int timeAdjustMinutes = 0;
    
    ImGui::Text("Time Adjustment:");
    ImGui::SetNextItemWidth(100);
    ImGui::InputInt("Hours", &timeAdjustHours);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    ImGui::InputInt("Minutes", &timeAdjustMinutes);
    
    if (ImGui::Button("Apply Time Adjustment"))
    {
        int64_t adjustment = (timeAdjustHours * 3600) + (timeAdjustMinutes * 60);
        cfg.SetInt64("RTC.Offset", currentOffset + adjustment);
        emuInstance->saveConfig();
        timeAdjustHours = 0;
        timeAdjustMinutes = 0;
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    
    if (ImGui::Button("Close"))
    {
        showDateTimeDialog = false;
    }
    
    ImGui::End();
}

void ImGuiFrontend::renderRAMInfoDialog()
{
    if (!showRAMInfoDialog) return;
    
    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("RAM Search", &showRAMInfoDialog)) {
        static int searchByteType = 0;
        static char searchValue[32] = "";
        static bool searchAll = true;
        static std::vector<std::pair<u32, s32>> searchResults;
        static std::vector<s32> previousValues;
        static bool searchInProgress = false;
        
        ImGui::Text("Search Type:");
        ImGui::SameLine();
        if (ImGui::RadioButton("1 Byte", &searchByteType, 0)) {}
        ImGui::SameLine();
        if (ImGui::RadioButton("2 Bytes", &searchByteType, 1)) {}
        ImGui::SameLine();
        if (ImGui::RadioButton("4 Bytes", &searchByteType, 2)) {}
        
        ImGui::Checkbox("Search All", &searchAll);
        if (!searchAll) {
            ImGui::SameLine();
            ImGui::InputText("Search Value", searchValue, sizeof(searchValue));
        }
        
        if (ImGui::Button("Search") && !searchInProgress) {
            searchInProgress = true;
            searchResults.clear();
            previousValues.clear();
            
            auto nds = emuInstance->getNDS();
            if (nds) {
                u32 ramSize = nds->MainRAMMask + 1;
                u32 step = (searchByteType == 0) ? 1 : (searchByteType == 1) ? 2 : 4;
                
                for (u32 addr = 0; addr < ramSize - step + 1; addr += step) {
                    s32 value = 0;
                    switch (searchByteType) {
                        case 0:
                            value = *(s8*)(nds->MainRAM + (addr & nds->MainRAMMask));
                            break;
                        case 1:
                            value = *(s16*)(nds->MainRAM + (addr & nds->MainRAMMask));
                            break;
                        case 2:
                            value = *(s32*)(nds->MainRAM + (addr & nds->MainRAMMask));
                            break;
                    }
                    
                    if (searchAll || (searchValue[0] != '\0' && value == atoi(searchValue))) {
                        searchResults.push_back({addr, value});
                        previousValues.push_back(value);
                    }
                }
            }
            searchInProgress = false;
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Clear")) {
            searchResults.clear();
            previousValues.clear();
        }
        
        ImGui::Text("Found: %zu results", searchResults.size());
        
        if (ImGui::BeginTable("RAMResults", 3, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Address");
            ImGui::TableSetupColumn("Current Value");
            ImGui::TableSetupColumn("Previous Value");
            ImGui::TableHeadersRow();
            
            for (size_t i = 0; i < searchResults.size(); i++) {
                ImGui::TableNextRow();
                
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("0x%08X", searchResults[i].first);
                
                ImGui::TableSetColumnIndex(1);
                char valueStr[32];
                snprintf(valueStr, sizeof(valueStr), "%d", searchResults[i].second);
                if (ImGui::InputText(("##value" + std::to_string(i)).c_str(), valueStr, sizeof(valueStr), ImGuiInputTextFlags_EnterReturnsTrue)) {
                    s32 newValue = atoi(valueStr);
                    if (newValue != searchResults[i].second) {
                        auto nds = emuInstance->getNDS();
                        if (nds) {
                            u32 addr = searchResults[i].first;
                            switch (searchByteType) {
                                case 0:
                                    *(s8*)(nds->MainRAM + (addr & nds->MainRAMMask)) = (s8)newValue;
                                    break;
                                case 1:
                                    *(s16*)(nds->MainRAM + (addr & nds->MainRAMMask)) = (s16)newValue;
                                    break;
                                case 2:
                                    *(s32*)(nds->MainRAM + (addr & nds->MainRAMMask)) = newValue;
                                    break;
                            }
                            searchResults[i].second = newValue;
                        }
                    }
                }
                
                ImGui::TableSetColumnIndex(2);
                if (i < previousValues.size() && searchResults[i].second != previousValues[i]) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
                    ImGui::Text("%d", previousValues[i]);
                    ImGui::PopStyleColor();
                } else {
                    ImGui::Text("%d", (i < previousValues.size()) ? previousValues[i] : 0);
                }
            }
            ImGui::EndTable();
        }
    }
    ImGui::End();
}

void ImGuiFrontend::renderCheatsManagementDialog()
{
    if (!showCheatsManagementDialog) return;
    ImGui::Begin("Cheat Code Management", &showCheatsManagementDialog, SettingsDialogFlags);
    
    auto& cfg = emuInstance->getGlobalConfig();
    bool cheatsEnabled = cfg.GetBool("Emu.EnableCheats");
    
    ImGui::Text("Cheat System Configuration");
    ImGui::Separator();
    
    if (ImGui::Checkbox("Enable Cheats", &cheatsEnabled)) {
        cfg.SetBool("Emu.EnableCheats", cheatsEnabled);
        emuInstance->saveConfig();
    }
    
    if (!cheatsEnabled)
    {
        ImGui::TextDisabled("Enable cheats to manage cheat codes");
        ImGui::Separator();
        if (ImGui::Button("Close"))
        {
            showCheatsManagementDialog = false;
        }
        ImGui::End();
        return;
    }
    
    if (!emuInstance->hasCart())
    {
        ImGui::Text("Load a ROM to manage cheat codes");
        ImGui::Separator();
        if (ImGui::Button("Close"))
        {
            showCheatsManagementDialog = false;
        }
        ImGui::End();
        return;
    }
    
    ImGui::Spacing();
    
    ImGui::Text("Cheat Categories and Codes:");
    ImGui::Separator();
    
    ImGui::BeginChild("CheatList", ImVec2(350, 300), true);
    
    static bool categoryExpanded[5] = {true, false, false, false, false};
    static bool cheatEnabled[15] = {false};
    static int selectedCheat = -1;
    
    const char* categories[] = {"General Cheats", "Player Stats", "Items", "Game Progress", "Debug"};
    const char* cheats[][3] = {
        {"Infinite Health", "Max Money", "Invincibility"},
        {"Max Level", "All Stats 999", "Infinite EXP"},
        {"All Items", "Infinite Items", "Max Inventory"},
        {"All Levels Unlocked", "All Characters", "Complete Story"},
        {"Debug Mode", "Level Select", "No Collision"}
    };
    
    for (int cat = 0; cat < 5; cat++)
    {
        if (ImGui::TreeNodeEx(categories[cat], categoryExpanded[cat] ? ImGuiTreeNodeFlags_DefaultOpen : 0))
        {
            categoryExpanded[cat] = true;
            
            for (int cheat = 0; cheat < 3; cheat++)
            {
                int cheatIndex = cat * 3 + cheat;
                ImGui::PushID(cheatIndex);
                
                bool isSelected = (selectedCheat == cheatIndex);
                if (ImGui::Selectable(cheats[cat][cheat], isSelected))
                {
                    selectedCheat = cheatIndex;
                }
                
                ImGui::SameLine();
                ImGui::Checkbox("##enabled", &cheatEnabled[cheatIndex]);
                
                ImGui::PopID();
            }
            
            ImGui::TreePop();
        }
        else
        {
            categoryExpanded[cat] = false;
        }
    }
    
    ImGui::EndChild();
    
    ImGui::SameLine();
    
    ImGui::BeginGroup();
    ImGui::Text("Cheat Details:");
    
    ImGui::BeginChild("CheatDetails", ImVec2(300, 150), true);
    
    if (selectedCheat >= 0)
    {
        int cat = selectedCheat / 3;
        int cheat = selectedCheat % 3;
        ImGui::Text("Name: %s", cheats[cat][cheat]);
        ImGui::Text("Category: %s", categories[cat]);
        ImGui::Text("Status: %s", cheatEnabled[selectedCheat] ? "Enabled" : "Disabled");
        ImGui::Separator();
        ImGui::Text("Description:");
        ImGui::TextWrapped("This is an example cheat code. In a real implementation, "
                          "this would show the actual cheat description and metadata.");
    }
    else
    {
        ImGui::TextDisabled("Select a cheat to view details");
    }
    
    ImGui::EndChild();
    
    ImGui::Text("Cheat Code:");
    ImGui::BeginChild("CheatCode", ImVec2(300, 140), true);
    
    if (selectedCheat >= 0)
    {
        static char cheatCode[1024] = "94000130 FFFB0000\n12345678 00000001\nD2000000 00000000";
        ImGui::InputTextMultiline("##CheatCode", cheatCode, sizeof(cheatCode), 
                                 ImVec2(-1, -1), ImGuiInputTextFlags_AllowTabInput);
    }
    else
    {
        ImGui::TextDisabled("Select a cheat to edit its code");
    }
    
    ImGui::EndChild();
    
    ImGui::EndGroup();
    
    ImGui::Spacing();
    ImGui::Separator();
    
    if (ImGui::Button("Add Category"))
    {
        printf("Add category dialog - not yet implemented\n");
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Add Cheat"))
    {
        printf("Add cheat dialog - not yet implemented\n");
    }
    
    ImGui::SameLine();
    bool hasSelection = (selectedCheat >= 0);
    if (ImGui::Button("Edit Cheat") && hasSelection)
    {
        printf("Edit cheat dialog - not yet implemented\n");
    }
    if (!hasSelection && ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Select a cheat to edit");
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Delete") && hasSelection)
    {
        printf("Delete cheat - not yet implemented\n");
    }
    if (!hasSelection && ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Select a cheat to delete");
    }
    
    ImGui::Spacing();
    
    if (ImGui::Button("Import Cheats"))
    {
        std::string filename = FileDialog::openFile(
            "Import cheat file",
            emuInstance->getConfigDirectory(),
            FileDialog::Filters::CHEAT_FILES
        );
        if (!filename.empty())
        {
            printf("Would import cheats from: %s\n", filename.c_str());
        }
    }
    if (ImGui::Button("Export Cheats"))
    {
        std::string filename = FileDialog::saveFile(
            "Export cheat file",
            emuInstance->getConfigDirectory(),
            FileDialog::Filters::CHEAT_FILES
        );
        if (!filename.empty())
        {
            printf("Export cheats - not yet implemented\n");
        }
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Note:");
    ImGui::TextWrapped("Full cheat code management requires integration with "
                      "the AR code file system. This interface shows the planned functionality.");
    
    ImGui::Spacing();
    ImGui::Separator();
    
    if (ImGui::Button("OK"))
    {
        emuInstance->saveConfig();
        showCheatsManagementDialog = false;
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Cancel"))
    {
        showCheatsManagementDialog = false;
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Apply"))
    {
        emuInstance->saveConfig();
    }
    
    ImGui::End();
}

void ImGuiFrontend::renderCameraSettingsDialog()
{
    if (!showCameraSettingsDialog) return;
    ImGui::Begin("Camera Settings", &showCameraSettingsDialog, SettingsDialogFlags);
    
    auto& cfg = emuInstance->getGlobalConfig();
    static int selectedCamera = 0;
    
    ImGui::Text("DSi Camera Settings");
    ImGui::Separator();
    
    const char* cameras[] = {"DSi outer camera", "DSi inner camera"};
    if (ImGui::Combo("Camera##CameraSelect", &selectedCamera, cameras, 2))
    {
    }
    
    ImGui::Spacing();
    
    std::string configSection = (selectedCamera == 0) ? "Camera0" : "Camera1";
    auto camCfg = cfg.GetTable(configSection);
    
    int inputType = camCfg.GetInt("InputType");
    ImGui::Text("Input Type:");
    if (ImGui::RadioButton("No camera##InputType", inputType == 0))
    {
        camCfg.SetInt("InputType", 0);
    }
    if (ImGui::RadioButton("Static image##InputType", inputType == 1))
    {
        camCfg.SetInt("InputType", 1);
    }
    if (ImGui::RadioButton("Physical camera##InputType", inputType == 2))
    {
        camCfg.SetInt("InputType", 2);
    }
    
    ImGui::Spacing();
    
    if (inputType == 1)
    {
        static char imagePath[512] = "";
        std::string currentPath = camCfg.GetString("ImagePath");
        strncpy(imagePath, currentPath.c_str(), sizeof(imagePath) - 1);
        
        ImGui::Text("Image file:");
        ImGui::SetNextItemWidth(300);
        if (ImGui::InputText("##ImagePath", imagePath, sizeof(imagePath)))
        {
            camCfg.SetString("ImagePath", imagePath);
        }
        ImGui::SameLine();
        if (ImGui::Button("Browse..."))
        {
            std::string filename = FileDialog::openFile(
                "Select Image File",
                imagePath,
                FileDialog::Filters::IMAGE_FILES
            );
            if (!filename.empty())
            {
                strncpy(imagePath, filename.c_str(), sizeof(imagePath) - 1);
                camCfg.SetString("ImagePath", filename);
            }
        }
    }
    
    if (inputType == 2)
    {
        ImGui::Text("Physical camera device:");
        int selectedDevice = 0;
        const char* devices[] = {"Default camera", "Camera 1", "Camera 2"};
        if (ImGui::Combo("Device##CameraDevice", &selectedDevice, devices, 3))
        {
            camCfg.SetString("DeviceName", devices[selectedDevice]);
        }
    }
    
    ImGui::Spacing();
    
    bool xFlip = camCfg.GetBool("XFlip");
    if (ImGui::Checkbox("Flip picture horizontally", &xFlip))
    {
        camCfg.SetBool("XFlip", xFlip);
    }
    
    ImGui::Spacing();
    
    ImGui::Text("Preview:");
    ImGui::BeginChild("CameraPreview", ImVec2(256, 192), true);
    
    if (inputType == 0)
    {
        ImGui::Text("Camera disabled");
    }
    else
    {
        ImGui::Text("Camera preview not yet implemented");
        ImGui::Text("Size: 256x192");
    }
    
    ImGui::EndChild();
    
    ImGui::Spacing();
    ImGui::Separator();
    
    if (ImGui::Button("OK"))
    {
        emuInstance->saveConfig();
        showCameraSettingsDialog = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel"))
    {
        showCameraSettingsDialog = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Apply"))
    {
        emuInstance->saveConfig();
    }
    
    ImGui::End();
}

void ImGuiFrontend::renderMPSettingsDialog()
{
    if (!showMPSettingsDialog) return;
    ImGui::Begin("Multiplayer Settings", &showMPSettingsDialog, SettingsDialogFlags);
    
    auto& cfg = emuInstance->getGlobalConfig();
    
    ImGui::Text("Audio Mode");
    ImGui::Separator();
    
    int audioMode = cfg.GetInt("MP.AudioMode");
    if (ImGui::RadioButton("All instances##AudioMode", audioMode == 0))
    {
        cfg.SetInt("MP.AudioMode", 0);
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("One instance only##AudioMode", audioMode == 1))
    {
        cfg.SetInt("MP.AudioMode", 1);
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Active instance only##AudioMode", audioMode == 2))
    {
        cfg.SetInt("MP.AudioMode", 2);
    }
    
    ImGui::Spacing();
    ImGui::Text("Network Settings");
    ImGui::Separator();
    
    int receiveTimeout = cfg.GetInt("MP.RecvTimeout");
    ImGui::SetNextItemWidth(150);
    if (ImGui::InputInt("Receive timeout (ms)", &receiveTimeout))
    {
        if (receiveTimeout < 1) receiveTimeout = 1;
        cfg.SetInt("MP.RecvTimeout", receiveTimeout);
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    
    if (ImGui::Button("Close"))
    {
        emuInstance->saveConfig();
        showMPSettingsDialog = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Apply"))
    {
        emuInstance->saveConfig();
    }
    
    ImGui::End();
}

void ImGuiFrontend::renderNetplayDialog()
{
    if (!showNetplayDialog) return;
    ImGui::Begin("Netplay", &showNetplayDialog, SettingsDialogFlags);
    
    static int netplayMode = 0;
    static char serverIP[256] = "127.0.0.1";
    static int serverPort = 8064;
    static int maxPlayers = 4;
    static char playerName[64] = "Player";
    static bool isConnected = false;
    
    ImGui::Text("Netplay Settings");
    ImGui::Separator();
    
    ImGui::Text("Connection Mode:");
    if (ImGui::RadioButton("Host game##NetplayMode", netplayMode == 0))
    {
        netplayMode = 0;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Join game##NetplayMode", netplayMode == 1))
    {
        netplayMode = 1;
    }
    
    ImGui::Spacing();
    
    ImGui::Text("Player Name:");
    ImGui::SetNextItemWidth(200);
    ImGui::InputText("##PlayerName", playerName, sizeof(playerName));
    
    ImGui::Spacing();
    
    if (netplayMode == 0)
    {
        ImGui::Text("Host Settings:");
        ImGui::Separator();
        
        ImGui::Text("Port:");
        ImGui::SetNextItemWidth(150);
        ImGui::InputInt("##Port", &serverPort);
        if (serverPort < 1024) serverPort = 1024;
        if (serverPort > 65535) serverPort = 65535;
        
        ImGui::Text("Max Players:");
        ImGui::SetNextItemWidth(150);
        ImGui::SliderInt("##MaxPlayers", &maxPlayers, 2, 16);
        
        ImGui::Spacing();
        
        if (!isConnected)
        {
            if (ImGui::Button("Start Hosting"))
            {
                printf("Would start hosting on port %d for %d players\n", serverPort, maxPlayers);
                isConnected = true;
            }
        }
        else
        {
            ImGui::Text("Hosting on port %d", serverPort);
            if (ImGui::Button("Stop Hosting"))
            {
                isConnected = false;
            }
        }
    }
    else
    {
        ImGui::Text("Client Settings:");
        ImGui::Separator();
        
        ImGui::Text("Server IP:");
        ImGui::SetNextItemWidth(200);
        ImGui::InputText("##ServerIP", serverIP, sizeof(serverIP));
        
        ImGui::Text("Port:");
        ImGui::SetNextItemWidth(150);
        ImGui::InputInt("##ClientPort", &serverPort);
        if (serverPort < 1024) serverPort = 1024;
        if (serverPort > 65535) serverPort = 65535;
        
        ImGui::Spacing();
        
        if (!isConnected)
        {
            if (ImGui::Button("Connect"))
            {
                printf("Would connect to %s:%d as %s\n", serverIP, serverPort, playerName);
                isConnected = true;
            }
        }
        else
        {
            ImGui::Text("Connected to %s:%d", serverIP, serverPort);
            if (ImGui::Button("Disconnect"))
            {
                isConnected = false;
            }
        }
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    
    if (isConnected)
    {
        ImGui::Text("Connection Status: Connected");
        ImGui::Text("Players:");
        
        ImGui::BeginChild("PlayerList", ImVec2(0, 100), true);
        
        ImGui::Text("1. %s (You)", playerName);
        if (netplayMode == 0)
        {
            ImGui::Text("2. Remote Player 1");
            ImGui::Text("3. Remote Player 2");
        }
        
        ImGui::EndChild();
        
        ImGui::Spacing();
        
        if (ImGui::Button("Send Message"))
        {
            printf("Send message functionality not implemented\n");
        }
        ImGui::SameLine();
        if (ImGui::Button("Sync State"))
        {
            printf("State sync functionality not implemented\n");
        }
    }
    else
    {
        ImGui::Text("Connection Status: Disconnected");
        ImGui::TextDisabled("No players connected");
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Note:");
    ImGui::TextWrapped("Netplay functionality is not yet fully implemented. "
                      "This dialog shows the planned interface.");
    
    ImGui::Spacing();
    ImGui::Separator();
    
    if (ImGui::Button("Close"))
    {
        if (isConnected)
        {
            isConnected = false;
        }
        showNetplayDialog = false;
    }
    
    ImGui::End();
}

void ImGuiFrontend::saveWindowState()
{
    int x, y, w, h;
    SDL_GetWindowPosition(window, &x, &y);
    SDL_GetWindowSize(window, &w, &h);
    
    windowCfg.SetInt("WindowX", x);
    windowCfg.SetInt("WindowY", y);
    windowCfg.SetInt("WindowWidth", w);
    windowCfg.SetInt("WindowHeight", h);
    windowCfg.SetBool("WindowMaximized", (SDL_GetWindowFlags(window) & SDL_WINDOW_MAXIMIZED) != 0);
}

void ImGuiFrontend::loadWindowState()
{
    int x = windowCfg.GetInt("WindowX");
    if (x == 0) x = 100;
    int y = windowCfg.GetInt("WindowY");
    if (y == 0) y = 100;
    int w = windowCfg.GetInt("WindowWidth");
    if (w == 0) w = 1400;
    int h = windowCfg.GetInt("WindowHeight");
    if (h == 0) h = 1000;
    bool maximized = windowCfg.GetBool("WindowMaximized");
    
    SDL_SetWindowPosition(window, x, y);
    SDL_SetWindowSize(window, w, h);
    
    if (maximized)
        SDL_MaximizeWindow(window);
}

void ImGuiFrontend::initOpenGL()
{
    if (hasOGL) return;
    
    // Set OpenGL attributes
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    
    // Create OpenGL context
    glContext = SDL_GL_CreateContext(window);
    if (!glContext)
    {
        printf("Failed to create OpenGL context: %s\n", SDL_GetError());
        return;
    }
    
    // Initialize GLAD
    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress))
    {
        printf("Failed to initialize GLAD\n");
        SDL_GL_DeleteContext(glContext);
        glContext = nullptr;
        return;
    }
    
    hasOGL = true;
    
    // Initialize screen textures
    initScreenTextures();
}

void ImGuiFrontend::deinitOpenGL()
{
    if (glContext)
    {
        if (texturesInitialized)
        {
            glDeleteTextures(1, &topScreenTexture);
            glDeleteTextures(1, &bottomScreenTexture);
            texturesInitialized = false;
        }
        
        SDL_GL_DeleteContext(glContext);
        glContext = nullptr;
    }
    hasOGL = false;
}

void ImGuiFrontend::makeCurrentGL()
{
    if (glContext)
        SDL_GL_MakeCurrent(window, glContext);
}

void ImGuiFrontend::releaseGL()
{
    SDL_GL_MakeCurrent(window, nullptr);
}

void ImGuiFrontend::drawScreenGL()
{
    if (!hasOGL || !texturesInitialized) return;
    
    // Update screen textures with latest frame data
    updateScreenTextures();
}

void ImGuiFrontend::hideConsoleWindow()
{
#ifdef _WIN32
    HWND consoleWindow = GetConsoleWindow();
    if (consoleWindow)
        ShowWindow(consoleWindow, SW_HIDE);
#endif
    consoleVisible = false;
}

void ImGuiFrontend::showConsoleWindow()
{
#ifdef _WIN32
    HWND consoleWindow = GetConsoleWindow();
    if (consoleWindow)
        ShowWindow(consoleWindow, SW_SHOW);
#endif
    consoleVisible = true;
}

void ImGuiFrontend::toggleConsoleWindow()
{
    if (consoleVisible)
        hideConsoleWindow();
    else
        showConsoleWindow();
}

void ImGuiFrontend::initFonts()
{
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();

    ImFontConfig config;
    config.OversampleH = 2;
    config.OversampleV = 1;
    config.PixelSnapH = true;

    const char* fontPath = "res/fonts/OpenSans-Regular.ttf";
    bool fontLoaded = false;
    for (int i = 0; i < FontSize_COUNT; i++)
    {
        config.SizePixels = fontSizes[i];
        ImFont* font = io.Fonts->AddFontFromFileTTF(fontPath, fontSizes[i], &config);
        if (font)
        {
            fonts[i] = font;
            fontLoaded = true;
            std::cout << "Loaded OpenSans font size " << fontSizes[i] << " successfully" << std::endl;
        }
        else
        {
            fonts[i] = nullptr;
            std::cerr << "Failed to load OpenSans font size " << fontSizes[i] << ", will use default font for this size." << std::endl;
        }
    }

    if (!fontLoaded)
    {
        std::cerr << "OpenSans font not found, falling back to ImGui default font." << std::endl;
        fonts[FontSize_Normal] = io.Fonts->AddFontDefault();
    }

    if (!io.Fonts->Build())
    {
        std::cerr << "Failed to build font atlas in initFonts()" << std::endl;
        io.Fonts->Clear();
        fonts[FontSize_Normal] = io.Fonts->AddFontDefault();
        io.Fonts->Build();
    }

    fontsLoaded = true;
    needFontRebuild = false;
}



void ImGuiFrontend::applyTheme(ThemeStyle theme)
{
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;
    
    switch (currentTheme)
    {
        case Theme_Dark:
        {
            ImGui::StyleColorsDark();
            style.WindowRounding = 10.0f;
            style.FrameRounding = 6.0f;
            style.ChildRounding = 8.0f;
            style.PopupRounding = 8.0f;
            style.ScrollbarRounding = 6.0f;
            style.GrabRounding = 6.0f;
            style.TabRounding = 6.0f;
            
            style.WindowPadding = ImVec2(15, 15);
            style.FramePadding = ImVec2(8, 4);
            style.ItemSpacing = ImVec2(10, 8);
            style.ScrollbarSize = 16.0f;
            style.GrabMinSize = 8.0f;
            
            style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
            style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
            style.SelectableTextAlign = ImVec2(0.0f, 0.5f);
            style.DisplaySafeAreaPadding = ImVec2(3, 3);
            
            colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.11f, 0.12f, 1.00f);
            colors[ImGuiCol_ChildBg] = ImVec4(0.12f, 0.13f, 0.14f, 1.00f);
            colors[ImGuiCol_PopupBg] = ImVec4(0.11f, 0.12f, 0.13f, 1.00f);
            colors[ImGuiCol_FrameBg] = ImVec4(0.18f, 0.19f, 0.20f, 1.00f);
            colors[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.23f, 0.24f, 1.00f);
            colors[ImGuiCol_FrameBgActive] = ImVec4(0.26f, 0.27f, 0.28f, 1.00f);
            colors[ImGuiCol_Button] = ImVec4(0.20f, 0.21f, 0.22f, 1.00f);
            colors[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.26f, 0.27f, 1.00f);
            colors[ImGuiCol_ButtonActive] = ImVec4(0.30f, 0.31f, 0.32f, 1.00f);
            colors[ImGuiCol_Header] = ImVec4(0.18f, 0.19f, 0.20f, 1.00f);
            colors[ImGuiCol_HeaderHovered] = ImVec4(0.22f, 0.23f, 0.24f, 1.00f);
            colors[ImGuiCol_HeaderActive] = ImVec4(0.26f, 0.27f, 0.28f, 1.00f);
            break;
        }
            
        case Theme_Light:
        {
            ImGui::StyleColorsLight();
            style.WindowRounding = 8.0f;
            style.FrameRounding = 4.0f;
            style.ChildRounding = 6.0f;
            style.PopupRounding = 6.0f;
            style.ScrollbarRounding = 4.0f;
            style.GrabRounding = 4.0f;
            style.TabRounding = 4.0f;
            break;
        }
            
        case Theme_Classic:
        {
            ImGui::StyleColorsClassic();
            style.WindowRounding = 8.0f;
            style.FrameRounding = 4.0f;
            style.ChildRounding = 6.0f;
            style.PopupRounding = 6.0f;
            style.ScrollbarRounding = 4.0f;
            style.GrabRounding = 4.0f;
            style.TabRounding = 4.0f;
            break;
        }
            
        case Theme_Ocean:
        {
            ImGui::StyleColorsDark();
            style.WindowRounding = 12.0f;
            style.FrameRounding = 6.0f;
            style.ChildRounding = 8.0f;
            style.PopupRounding = 8.0f;
            style.ScrollbarRounding = 6.0f;
            style.GrabRounding = 6.0f;
            style.TabRounding = 6.0f;
            
            colors[ImGuiCol_WindowBg] = ImVec4(0.13f, 0.14f, 0.15f, 1.00f);
            colors[ImGuiCol_ChildBg] = ImVec4(0.16f, 0.17f, 0.18f, 1.00f);
            colors[ImGuiCol_PopupBg] = ImVec4(0.12f, 0.13f, 0.14f, 1.00f);
            colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.21f, 0.22f, 1.00f);
            colors[ImGuiCol_FrameBgHovered] = ImVec4(0.25f, 0.26f, 0.27f, 1.00f);
            colors[ImGuiCol_FrameBgActive] = ImVec4(0.30f, 0.31f, 0.32f, 1.00f);
            colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.11f, 0.12f, 1.00f);
            colors[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.13f, 0.14f, 1.00f);
            colors[ImGuiCol_Button] = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
            colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
            colors[ImGuiCol_ButtonActive] = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);
            break;
        }
            
        case Theme_Forest:
        {
            ImGui::StyleColorsDark();
            style.WindowRounding = 8.0f;
            style.FrameRounding = 4.0f;
            style.ChildRounding = 6.0f;
            style.PopupRounding = 6.0f;
            style.ScrollbarRounding = 4.0f;
            style.GrabRounding = 4.0f;
            style.TabRounding = 4.0f;
            
            colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.15f, 0.08f, 1.00f);
            colors[ImGuiCol_Button] = ImVec4(0.20f, 0.60f, 0.20f, 0.40f);
            colors[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.70f, 0.25f, 1.00f);
            colors[ImGuiCol_ButtonActive] = ImVec4(0.15f, 0.50f, 0.15f, 1.00f);
            break;
        }
            
        case Theme_Cherry:
        {
            ImGui::StyleColorsDark();
            style.WindowRounding = 8.0f;
            style.FrameRounding = 4.0f;
            style.ChildRounding = 6.0f;
            style.PopupRounding = 6.0f;
            style.ScrollbarRounding = 4.0f;
            style.GrabRounding = 4.0f;
            style.TabRounding = 4.0f;
            
            colors[ImGuiCol_WindowBg] = ImVec4(0.15f, 0.08f, 0.08f, 1.00f);
            colors[ImGuiCol_Button] = ImVec4(0.60f, 0.20f, 0.20f, 0.40f);
            colors[ImGuiCol_ButtonHovered] = ImVec4(0.70f, 0.25f, 0.25f, 1.00f);
            colors[ImGuiCol_ButtonActive] = ImVec4(0.50f, 0.15f, 0.15f, 1.00f);
            break;
        }
            
        case Theme_Purple:
        {   
            ImGui::StyleColorsDark();
            style.WindowRounding = 8.0f;
            style.FrameRounding = 4.0f;
            style.ChildRounding = 6.0f;
            style.PopupRounding = 6.0f;
            style.ScrollbarRounding = 4.0f;
            style.GrabRounding = 4.0f;
            style.TabRounding = 4.0f;
            
            colors[ImGuiCol_WindowBg] = ImVec4(0.12f, 0.08f, 0.15f, 1.00f);
            colors[ImGuiCol_Button] = ImVec4(0.50f, 0.20f, 0.60f, 0.40f);
            colors[ImGuiCol_ButtonHovered] = ImVec4(0.60f, 0.25f, 0.70f, 1.00f);
            colors[ImGuiCol_ButtonActive] = ImVec4(0.40f, 0.15f, 0.50f, 1.00f);
            break;
        }
            
        case Theme_Custom:
        {
            ImGui::StyleColorsDark();
            style.WindowRounding = 8.0f;
            style.FrameRounding = 4.0f;
            style.ChildRounding = 6.0f;
            style.PopupRounding = 6.0f;
            style.ScrollbarRounding = 4.0f;
            style.GrabRounding = 4.0f;
            style.TabRounding = 4.0f;
            break;
        }
    }
    
    if (fontsLoaded && currentFontSize >= 0 && currentFontSize < FontSize_COUNT && fonts[currentFontSize])
    {
        ImGui::GetIO().FontDefault = fonts[currentFontSize];
    }
}



void ImGuiFrontend::rebuildFonts()
{
    if (needFontRebuild)
    {
        buildFontAtlas();
        needFontRebuild = false;
    }
}

void ImGuiFrontend::setTheme(ThemeStyle theme)
{
    if (currentTheme != theme)
    {
        currentTheme = theme;
        applyTheme(theme);
        saveFontSettings();
    }
}

void ImGuiFrontend::setFontSize(FontSize size)
{
    if (currentFontSize != size)
    {
        currentFontSize = size;
        applyTheme(currentTheme);
        saveFontSettings();
    }
}

void ImGuiFrontend::onFocusIn()
{
    focused = true;
    
    if (pauseOnLostFocus && !pausedManually && emuInstance && emuInstance->isRunning()) {
        auto* thread = emuInstance->getEmuThread();
        thread->emuUnpause();
    }
    
    if (emuInstance) {
        emuInstance->audioEnable();
    }
}

void ImGuiFrontend::onFocusOut()
{
    focused = false;
    
    if (emuInstance) {
        emuInstance->keyReleaseAll();
    }
    
    if (pauseOnLostFocus && emuInstance && emuInstance->isRunning()) {
        auto* thread = emuInstance->getEmuThread();
        thread->emuPause();
    }
    
    if (emuInstance) {
        emuInstance->audioMute();
    }
}

void ImGuiFrontend::updateJoystickList()
{
    availableJoysticks.clear();
    availableJoysticks.push_back("(no controller)");
    
    for (int i = 0; i < SDL_NumJoysticks(); i++)
    {
        if (SDL_IsGameController(i))
        {
            SDL_GameController* controller = SDL_GameControllerOpen(i);
            if (controller)
            {
                const char* name = SDL_GameControllerName(controller);
                if (name)
                {
                    availableJoysticks.push_back(std::string(name));
                }
                else
                {
                    availableJoysticks.push_back("Controller " + std::to_string(i));
                }
                SDL_GameControllerClose(controller);
            }
        }
    }
}

void ImGuiFrontend::renderDSControlsTab()
{
    ImGui::Text("DS Button Mapping");
    ImGui::Separator();
    
    if (ImGui::Button(showKeyboardMappings ? "Switch to Joystick mappings" : "Switch to Keyboard mappings")) {
        showKeyboardMappings = !showKeyboardMappings;
        showJoystickMappings = !showJoystickMappings;
    }
    
    ImGui::Spacing();
    
    const int dskeyorder[12] = {0, 1, 10, 11, 5, 4, 6, 7, 9, 8, 2, 3};
    const char* dskeylabels[12] = {"A", "B", "X", "Y", "Left", "Right", "Up", "Down", "L", "R", "Select", "Start"};
    
    ImVec2 contentSize = ImGui::GetContentRegionAvail();
    float consoleWidth = 300.0f;
    float consoleHeight = 200.0f;
    
    ImVec2 consolePos = ImGui::GetCursorScreenPos();
    consolePos.x += (contentSize.x - consoleWidth) * 0.5f;
    
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 consoleMin = consolePos;
    ImVec2 consoleMax = ImVec2(consolePos.x + consoleWidth, consolePos.y + consoleHeight);
    
    draw_list->AddRectFilled(consoleMin, consoleMax, IM_COL32(180, 180, 180, 255));
    draw_list->AddRect(consoleMin, consoleMax, IM_COL32(100, 100, 100, 255), 0.0f, 0, 2.0f);
    
    ImVec2 screen1Min = ImVec2(consolePos.x + 20, consolePos.y + 20);
    ImVec2 screen1Max = ImVec2(consolePos.x + 140, consolePos.y + 80);
    draw_list->AddRectFilled(screen1Min, screen1Max, IM_COL32(0, 0, 0, 255));
    draw_list->AddRect(screen1Min, screen1Max, IM_COL32(50, 50, 50, 255));
    
    ImVec2 screen2Min = ImVec2(consolePos.x + 160, consolePos.y + 20);
    ImVec2 screen2Max = ImVec2(consolePos.x + 280, consolePos.y + 80);
    draw_list->AddRectFilled(screen2Min, screen2Max, IM_COL32(0, 0, 0, 255));
    draw_list->AddRect(screen2Min, screen2Max, IM_COL32(50, 50, 50, 255));
    
    ImVec2 dpadMin = ImVec2(consolePos.x + 20, consolePos.y + 100);
    ImVec2 dpadMax = ImVec2(consolePos.x + 80, consolePos.y + 160);
    draw_list->AddCircleFilled(ImVec2(consolePos.x + 50, consolePos.y + 130), 25, IM_COL32(120, 120, 120, 255));
    
    ImVec2 faceMin = ImVec2(consolePos.x + 220, consolePos.y + 100);
    ImVec2 faceMax = ImVec2(consolePos.x + 280, consolePos.y + 160);
    
    ImVec2 shoulderLMin = ImVec2(consolePos.x + 10, consolePos.y + 10);
    ImVec2 shoulderLMax = ImVec2(consolePos.x + 50, consolePos.y + 30);
    draw_list->AddRectFilled(shoulderLMin, shoulderLMax, IM_COL32(140, 140, 140, 255));
    
    ImVec2 shoulderRMin = ImVec2(consolePos.x + 250, consolePos.y + 10);
    ImVec2 shoulderRMax = ImVec2(consolePos.x + 290, consolePos.y + 30);
    draw_list->AddRectFilled(shoulderRMin, shoulderRMax, IM_COL32(140, 140, 140, 255));
    
    float buttonWidth = 80.0f;
    float buttonHeight = 25.0f;
    float spacing = 10.0f;
    
    float leftX = consolePos.x - buttonWidth - spacing;
    float leftStartY = consolePos.y + 20;
    
    float rightX = consolePos.x + consoleWidth + spacing;
    float rightStartY = consolePos.y + 20;
    
    float bottomX = consolePos.x + (consoleWidth - buttonWidth * 2 - spacing) * 0.5f;
    float bottomY = consolePos.y + consoleHeight + spacing;
    
    for (int i = 0; i < 12; i++) {
        int buttonIndex = dskeyorder[i];
        int* mapping = showKeyboardMappings ? &emuInstance->keyMapping[buttonIndex] : &emuInstance->joyMapping[buttonIndex];
        
        ImVec2 buttonPos;
        std::string buttonText = getKeyName(*mapping);
        if (buttonText == "None") buttonText = dskeylabels[i];
        
        if (i == 8) {
            buttonPos = ImVec2(leftX, leftStartY);
        } else if (i == 6) {
            buttonPos = ImVec2(leftX, leftStartY + buttonHeight + spacing);
        } else if (i == 4) {
            buttonPos = ImVec2(leftX, leftStartY + (buttonHeight + spacing) * 2);
        } else if (i == 5) {
            buttonPos = ImVec2(leftX, leftStartY + (buttonHeight + spacing) * 3);
        } else if (i == 7) {
            buttonPos = ImVec2(leftX, leftStartY + (buttonHeight + spacing) * 4);
        } else if (i == 9) {
            buttonPos = ImVec2(rightX, rightStartY);
        } else if (i == 2) {
            buttonPos = ImVec2(rightX, rightStartY + buttonHeight + spacing);
        } else if (i == 3) {
            buttonPos = ImVec2(rightX, rightStartY + (buttonHeight + spacing) * 2);
        } else if (i == 0) {
            buttonPos = ImVec2(rightX, rightStartY + (buttonHeight + spacing) * 3);
        } else if (i == 1) {
            buttonPos = ImVec2(rightX, rightStartY + (buttonHeight + spacing) * 4);
        } else if (i == 10) {
            buttonPos = ImVec2(bottomX, bottomY);
        } else if (i == 11) {
            buttonPos = ImVec2(bottomX + buttonWidth + spacing, bottomY);
        }
        
        ImGui::SetCursorScreenPos(buttonPos);
        std::string buttonLabel = std::string(dskeylabels[i]) + "##" + std::to_string(i);
        if (ImGui::Button(buttonLabel.c_str(), ImVec2(buttonWidth, buttonHeight))) {
            startInputMapping(mapping, dskeylabels[i]);
        }
        
        ImGui::SetCursorScreenPos(ImVec2(buttonPos.x, buttonPos.y + buttonHeight + 2));
        ImGui::Text("%s", buttonText.c_str());
        
        ImGui::SetCursorScreenPos(ImVec2(buttonPos.x + buttonWidth - 40, buttonPos.y + buttonHeight + 2));
        if (ImGui::Button(("Clear##" + std::to_string(i)).c_str(), ImVec2(40, 15))) {
            *mapping = -1;
        }
    }
    
    ImGui::SetCursorScreenPos(ImVec2(consolePos.x, consolePos.y + consoleHeight + 100));
}

void ImGuiFrontend::renderHotkeysTab()
{
    ImGui::Text("Hotkey Mapping");
    ImGui::Separator();
    
    static constexpr std::initializer_list<int> hk_general = {
        HK_Pause,
        HK_Reset,
        HK_FrameStep,
        HK_FastForward,
        HK_FastForwardToggle,
        HK_SlowMo,
        HK_SlowMoToggle,
        HK_FrameLimitToggle,
        HK_FullscreenToggle,
        HK_Lid,
        HK_Mic,
        HK_SwapScreens,
        HK_SwapScreenEmphasis,
        HK_PowerButton,
        HK_VolumeUp,
        HK_VolumeDown
    };
    
    static constexpr std::initializer_list<const char*> hk_general_labels = {
        "Pause/resume",
        "Reset",
        "Frame step",
        "Fast forward",
        "Toggle fast forward",
        "Slow mo",
        "Toggle slow mo",
        "Toggle FPS limit",
        "Toggle fullscreen",
        "Close/open lid",
        "Microphone",
        "Swap screens",
        "Swap screen emphasis",
        "DSi Power button",
        "DSi Volume up",
        "DSi Volume down"
    };
    
    if (ImGui::BeginTable("Hotkeys", 2, ImGuiTableFlags_Borders)) {
        ImGui::TableSetupColumn("Keyboard mappings:");
        ImGui::TableSetupColumn("Joystick mappings:");
        ImGui::TableHeadersRow();
        
        int i = 0;
        for (int hotkey : hk_general) {
            ImGui::TableNextRow();
            
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s:", hk_general_labels.begin()[i]);
            
            ImGui::TableSetColumnIndex(0);
            int* keyMapping = &emuInstance->hkKeyMapping[hotkey];
            
            std::string keyText = getKeyName(*keyMapping);
            if (ImGui::Button(("##KeyHotkey" + std::to_string(i)).c_str())) {
                startInputMapping(keyMapping, hk_general_labels.begin()[i]);
            }
            ImGui::SameLine();
            ImGui::Text("%s", keyText.c_str());
            
            ImGui::SameLine();
            if (ImGui::Button(("Clear##KeyHotkey" + std::to_string(i)).c_str())) {
                *keyMapping = -1;
            }
            
            ImGui::TableSetColumnIndex(1);
            int* joyMapping = &emuInstance->hkJoyMapping[hotkey];
            
            std::string joyText = getJoyButtonName(*joyMapping);
            if (ImGui::Button(("##JoyHotkey" + std::to_string(i)).c_str())) {
                startInputMapping(joyMapping, hk_general_labels.begin()[i]);
            }
            ImGui::SameLine();
            ImGui::Text("%s", joyText.c_str());
            
            ImGui::SameLine();
            if (ImGui::Button(("Clear##JoyHotkey" + std::to_string(i)).c_str())) {
                *joyMapping = -1;
            }
            
            i++;
        }
        
        ImGui::EndTable();
    }
}

void ImGuiFrontend::renderAddonsTab()
{
    ImGui::Text("Add-on Controls");
    ImGui::Separator();
    
    static constexpr std::initializer_list<int> hk_addons = {
        HK_SolarSensorIncrease,
        HK_SolarSensorDecrease,
        HK_GuitarGripGreen,
        HK_GuitarGripRed,
        HK_GuitarGripYellow,
        HK_GuitarGripBlue,
    };
    
    static constexpr std::initializer_list<const char*> hk_addons_labels = {
        "[Boktai] Sunlight + ",
        "[Boktai] Sunlight - ",
        "[Guitar Grip] Green",
        "[Guitar Grip] Red",
        "[Guitar Grip] Yellow",
        "[Guitar Grip] Blue",
    };
    
    if (ImGui::BeginTable("Addons", 2, ImGuiTableFlags_Borders)) {
        ImGui::TableSetupColumn("Keyboard mappings:");
        ImGui::TableSetupColumn("Joystick mappings:");
        ImGui::TableHeadersRow();
        
        int i = 0;
        for (int hotkey : hk_addons) {
            ImGui::TableNextRow();
            
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s:", hk_addons_labels.begin()[i]);
            
            ImGui::TableSetColumnIndex(0);
            int* keyMapping = &emuInstance->hkKeyMapping[hotkey];
            
            std::string keyText = getKeyName(*keyMapping);
            if (ImGui::Button(("##KeyAddon" + std::to_string(i)).c_str())) {
                startInputMapping(keyMapping, hk_addons_labels.begin()[i]);
            }
            ImGui::SameLine();
            ImGui::Text("%s", keyText.c_str());
            
            ImGui::SameLine();
            if (ImGui::Button(("Clear##KeyAddon" + std::to_string(i)).c_str())) {
                *keyMapping = -1;
            }
            
            ImGui::TableSetColumnIndex(1);
            int* joyMapping = &emuInstance->hkJoyMapping[hotkey];
            
            std::string joyText = getJoyButtonName(*joyMapping);
            if (ImGui::Button(("##JoyAddon" + std::to_string(i)).c_str())) {
                startInputMapping(joyMapping, hk_addons_labels.begin()[i]);
            }
            ImGui::SameLine();
            ImGui::Text("%s", joyText.c_str());
            
            ImGui::SameLine();
            if (ImGui::Button(("Clear##JoyAddon" + std::to_string(i)).c_str())) {
                *joyMapping = -1;
            }
            
            i++;
        }
        
        ImGui::EndTable();
    }
}

void ImGuiFrontend::handleInputCapture()
{
    if (!currentMappingTarget || !isMappingInput) return;
    
    const Uint8* keyboardState = SDL_GetKeyboardState(nullptr);
    
    for (int key = 0; key < SDL_NUM_SCANCODES; key++) {
        if (keyboardState[key]) {
            SDL_Keycode sdlKey = SDL_GetKeyFromScancode((SDL_Scancode)key);
            
            int mod = 0;
            if (keyboardState[SDL_SCANCODE_LCTRL] || keyboardState[SDL_SCANCODE_RCTRL]) mod |= KMOD_CTRL;
            if (keyboardState[SDL_SCANCODE_LSHIFT] || keyboardState[SDL_SCANCODE_RSHIFT]) mod |= KMOD_SHIFT;
            if (keyboardState[SDL_SCANCODE_LALT] || keyboardState[SDL_SCANCODE_RALT]) mod |= KMOD_ALT;
            if (keyboardState[SDL_SCANCODE_LGUI] || keyboardState[SDL_SCANCODE_RGUI]) mod |= KMOD_GUI;
            
            int keyVal = sdlKey | mod;
            
            if (key == SDL_SCANCODE_RCTRL || key == SDL_SCANCODE_RSHIFT || 
                key == SDL_SCANCODE_RALT || key == SDL_SCANCODE_RGUI) {
                keyVal |= (1 << 31);
            }
            
            *currentMappingTarget = keyVal;
            stopInputMapping();
            return;
        }
    }
    
    if (emuInstance && emuInstance->getJoystick()) {
        SDL_Joystick* joystick = emuInstance->getJoystick();
        
        for (int button = 0; button < SDL_JoystickNumButtons(joystick); button++) {
            if (SDL_JoystickGetButton(joystick, button)) {
                *currentMappingTarget = button;
                stopInputMapping();
                return;
            }
        }
        
        for (int hat = 0; hat < SDL_JoystickNumHats(joystick); hat++) {
            Uint8 hatValue = SDL_JoystickGetHat(joystick, hat);
            if (hatValue != SDL_HAT_CENTERED) {
                int hatMapping = 0x1000 | (hat << 8) | hatValue;
                *currentMappingTarget = hatMapping;
                stopInputMapping();
                return;
            }
        }
        
        for (int axis = 0; axis < SDL_JoystickNumAxes(joystick); axis++) {
            Sint16 axisValue = SDL_JoystickGetAxis(joystick, axis);
            if (abs(axisValue) > 8192) {
                int axisMapping = 0x2000 | (axis << 8) | (axisValue > 0 ? 1 : 0);
                *currentMappingTarget = axisMapping;
                stopInputMapping();
                return;
            }
        }
    }
    
    if (keyboardState[SDL_SCANCODE_ESCAPE]) {
        stopInputMapping();
        return;
    }
    
    if (keyboardState[SDL_SCANCODE_BACKSPACE]) {
        *currentMappingTarget = -1;
        stopInputMapping();
        return;
    }
}

void ImGuiFrontend::loadFontSettings()
{
    // TODO: Load font settings from config
    // For now, use defaults
    currentFontSize = FontSize_Normal;
    currentTheme = Theme_Dark;
}

void ImGuiFrontend::saveFontSettings()
{
    // TODO: Save font settings to config
    emuInstance->saveConfig();
}

void ImGuiFrontend::buildFontAtlas()
{
    // TODO: Build font atlas (I'll probably do something with this one day.)
    // This is handled by ImGui internally
}

void ImGuiFrontend::loadRecentFilesMenu()
{
    recentFiles.clear();
    auto& config = emuInstance->getGlobalConfig();
    int count = config.GetInt("RecentROM.Count");
    for (int i = 0; i < count && i < maxRecentFiles; ++i) {
        std::string key = "RecentROM." + std::to_string(i);
        std::string value = config.GetString(key);
        if (!value.empty()) recentFiles.push_back(value);
    }
}

std::vector<std::string> ImGuiFrontend::pickROM(bool gba)
{

    const char* console = gba ? "GBA" : "DS";
    static const std::vector<std::string> ndsExts = { ".nds", ".srl", ".dsi", ".ids" };
    static const std::vector<std::string> gbaExts = { ".gba", ".agb" };
    const std::vector<std::string>& romexts = gba ? gbaExts : ndsExts;

    std::string rawROMs;
    for (const auto& ext : romexts) rawROMs += "*" + ext + " ";
    std::string extraFilters = std::string(";;") + console + " ROMs (" + rawROMs + ")";
    std::string allROMs = rawROMs;

    std::string zstdROMs;
    for (const auto& ext : romexts) zstdROMs += "*" + ext + ".zst ";
    extraFilters += ");;Zstandard-compressed " + std::string(console) + " ROMs (" + zstdROMs + ")";
    allROMs += " " + zstdROMs;

#ifdef ARCHIVE_SUPPORT_ENABLED
    static const std::vector<std::string> archiveExts = { ".zip", ".7z", ".rar", ".tar", ".tar.gz", ".tgz", ".tar.xz", ".txz", ".tar.bz2", ".tbz2", ".tar.lz4", ".tlz4", ".tar.zst", ".tzst", ".tar.Z", ".taz", ".tar.lz", ".tar.lzma", ".tlz", ".tar.lrz", ".tlrz", ".tar.lzo", ".tzo" };
    std::string archives;
    for (const auto& ext : archiveExts) archives += "*" + ext + " ";
    extraFilters += ";;Archives (" + archives + ")";
    allROMs += " " + archives;
#endif
    extraFilters += ";;All files (*.*)";

    std::string lastFolder = emuInstance->getGlobalConfig().GetString("LastROMFolder");
    std::string filename = FileDialog::openFile(
        std::string("Open ") + console + " ROM",
        lastFolder,
        FileDialog::Filters::ROM_FILES
    );
    if (filename.empty()) {
        return {};
    }
    size_t slash = filename.find_last_of("/\\");
    if (slash != std::string::npos)
        emuInstance->getGlobalConfig().SetString("LastROMFolder", filename.substr(0, slash));
    auto ret = splitArchivePath(filename, false);
    return ret;
}

std::vector<std::string> ImGuiFrontend::splitArchivePath(const std::string& filename, bool useMemberSyntax)
{
    if (filename.empty()) return {};
#ifdef ARCHIVE_SUPPORT_ENABLED
    if (useMemberSyntax) {
        size_t bar = filename.find('|');
        if (bar != std::string::npos) {
            std::string archive = filename.substr(0, bar);
            std::string subfile = filename.substr(bar + 1);
            // TODO: Check if archive and subfile exist
            // For now, just return both
            return {archive, subfile};
        }
    }
#endif
    // Check if file exists
    FILE* f = fopen(filename.c_str(), "rb");
    if (!f) return {};
    fclose(f);
#ifdef ARCHIVE_SUPPORT_ENABLED
    if (SupportedArchiveByExtension(filename) /*|| SupportedArchiveByMimetype(filename)*/) {
        std::string subfile = pickFileFromArchive(filename);
        if (subfile.empty()) return {};
        return { filename, subfile };
    }
#endif
    return { filename };
}

std::string ImGuiFrontend::pickFileFromArchive(const std::string& archiveFileName)
{
#ifdef ARCHIVE_SUPPORT_ENABLED
    // TODO: Implement Archive::ListArchive and file picking dialog
    // For now, return empty string
    return "";
#else
    return "";
#endif
}

// Archive support helper functions
bool ImGuiFrontend::SupportedArchiveByExtension(const std::string& filename)
{
#ifdef ARCHIVE_SUPPORT_ENABLED
    static const std::vector<std::string> archiveExtensions = {
        ".zip", ".7z", ".rar", ".tar",
        ".tar.gz", ".tgz", ".tar.xz", ".txz",
        ".tar.bz2", ".tbz2", ".tar.lz4", ".tlz4",
        ".tar.zst", ".tzst", ".tar.Z", ".taz",
        ".tar.lz", ".tar.lzma", ".tlz", ".tar.lrz",
        ".tlrz", ".tar.lzo", ".tzo"
    };
    
    for (const auto& ext : archiveExtensions) {
        if (filename.length() >= ext.length() &&
            filename.compare(filename.length() - ext.length(), ext.length(), ext) == 0) {
            return true;
        }
    }
#endif
    return false;
}

bool ImGuiFrontend::SupportedArchiveByMimetype(const std::string& filename)
{
#ifdef ARCHIVE_SUPPORT_ENABLED
    static const std::vector<std::string> archiveMimeTypes = {
        "application/zip",
        "application/x-7z-compressed",
        "application/vnd.rar",
        "application/x-tar",
        "application/x-compressed-tar",
        "application/x-xz-compressed-tar",
        "application/x-bzip-compressed-tar",
        "application/x-lz4-compressed-tar",
        "application/x-zstd-compressed-tar"
    };
    
    for (const auto& mimeType : archiveMimeTypes) {
        if (filename.find(mimeType) != std::string::npos) {
            return true;
        }
    }
    return false;
#else
    return false;
#endif
}

bool ImGuiFrontend::NdsRomByExtension(const std::string& filename)
{
    static const std::vector<std::string> ndsExtensions = {".nds", ".srl", ".dsi", ".ids"};
    
    for (const auto& ext : ndsExtensions) {
        if (filename.length() >= ext.length() &&
            filename.compare(filename.length() - ext.length(), ext.length(), ext) == 0) {
            return true;
        }
    }
    return false;
}

bool ImGuiFrontend::GbaRomByExtension(const std::string& filename)
{
    static const std::vector<std::string> gbaExtensions = {".gba", ".agb"};
    
    for (const auto& ext : gbaExtensions) {
        if (filename.length() >= ext.length() &&
            filename.compare(filename.length() - ext.length(), ext.length(), ext) == 0) {
            return true;
        }
    }
    return false;
}

bool ImGuiFrontend::NdsRomByMimetype(const std::string& filename)
{
    static const std::vector<std::string> ndsMimeTypes = {
        "application/x-nintendo-ds-rom",
        "application/octet-stream"
    };
    
    return NdsRomByExtension(filename);
}

bool ImGuiFrontend::GbaRomByMimetype(const std::string& filename)
{
    static const std::vector<std::string> gbaMimeTypes = {
        "application/x-nintendo-gba-rom",
        "application/octet-stream"
    };
    
    return GbaRomByExtension(filename);
}

bool ImGuiFrontend::FileIsSupportedFiletype(const std::string& filename, bool insideArchive)
{
    if (filename.length() >= 4 && 
        filename.compare(filename.length() - 4, 4, ".zst") == 0) {
        std::string baseName = filename.substr(0, filename.length() - 4);
        return NdsRomByExtension(baseName) || GbaRomByExtension(baseName);
    }
    
    return NdsRomByExtension(filename) || 
           GbaRomByExtension(filename) || 
           SupportedArchiveByExtension(filename) ||
           NdsRomByMimetype(filename) || 
           GbaRomByMimetype(filename) || 
           SupportedArchiveByMimetype(filename);
}

void ImGuiFrontend::loadFont(FontSize size)
{
    if (size < 0 || size >= FontSize_COUNT) return;
    
    ImGuiIO& io = ImGui::GetIO();
    fonts[size] = io.Fonts->AddFontDefault();
    
    if (fonts[size]) {
        ImFontConfig config;
        config.SizePixels = fontSizes[size];
        fonts[size] = io.Fonts->AddFontDefault(&config);
    }
    
    fontsLoaded = true;
}



void ImGuiFrontend::startInputMapping(int* target, const std::string& label)
{
    currentMappingTarget = target;
    mappingButtonLabel = label;
    isMappingInput = true;
}

void ImGuiFrontend::stopInputMapping()
{
    currentMappingTarget = nullptr;
    mappingButtonLabel.clear();
    isMappingInput = false;
}

std::string ImGuiFrontend::getKeyName(int key)
{
    if (key == -1) return "None";
    
    if (key & (1 << 31)) {
        key &= ~(1 << 31);
        switch (key) {
            case SDLK_LCTRL: return "Right Ctrl";
            case SDLK_LSHIFT: return "Right Shift";
            case SDLK_LALT: return "Right Alt";
            case SDLK_LGUI: return "Right Meta";
        }
    } else {
        switch (key) {
            case SDLK_LCTRL: return "Left Ctrl";
            case SDLK_LSHIFT: return "Left Shift";
            case SDLK_LALT: return "Left Alt";
            case SDLK_LGUI: return "Left Meta";
        }
    }
    
    const char* keyName = SDL_GetKeyName(key);
    if (keyName && strlen(keyName) > 0) {
        return keyName;
    }
    
    return "Unknown";
}

std::string ImGuiFrontend::getJoyButtonName(int button)
{
    if (button == -1) return "None";
    
    if (button >= 0 && button < 32) {
        return "Button " + std::to_string(button);
    }
    
    if (button >= 0x1000 && button < 0x2000) {
        int hat = (button >> 8) & 0xFF;
        int direction = button & 0xFF;
        std::string dirName;
        
        switch (direction) {
            case SDL_HAT_CENTERED: dirName = "Center"; break;
            case SDL_HAT_UP: dirName = "Up"; break;
            case SDL_HAT_RIGHT: dirName = "Right"; break;
            case SDL_HAT_DOWN: dirName = "Down"; break;
            case SDL_HAT_LEFT: dirName = "Left"; break;
            case SDL_HAT_RIGHTUP: dirName = "Right+Up"; break;
            case SDL_HAT_RIGHTDOWN: dirName = "Right+Down"; break;
            case SDL_HAT_LEFTUP: dirName = "Left+Up"; break;
            case SDL_HAT_LEFTDOWN: dirName = "Left+Down"; break;
            default: dirName = "Unknown"; break;
        }
        
        return "Hat " + std::to_string(hat) + " " + dirName;
    }
    
    if (button >= 0x2000 && button < 0x3000) {
        int axis = (button >> 8) & 0xFF;
        bool positive = (button & 0xFF) != 0;
        return "Axis " + std::to_string(axis) + (positive ? "+" : "-");
    }
    
    return "Unknown";
}

int ImGuiFrontend::convertImGuiKeyToSDL(int imguiKey)
{
    switch (imguiKey) {
        case ImGuiKey_Tab: return SDLK_TAB;
        case ImGuiKey_LeftArrow: return SDLK_LEFT;
        case ImGuiKey_RightArrow: return SDLK_RIGHT;
        case ImGuiKey_UpArrow: return SDLK_UP;
        case ImGuiKey_DownArrow: return SDLK_DOWN;
        case ImGuiKey_PageUp: return SDLK_PAGEUP;
        case ImGuiKey_PageDown: return SDLK_PAGEDOWN;
        case ImGuiKey_Home: return SDLK_HOME;
        case ImGuiKey_End: return SDLK_END;
        case ImGuiKey_Insert: return SDLK_INSERT;
        case ImGuiKey_Delete: return SDLK_DELETE;
        case ImGuiKey_Backspace: return SDLK_BACKSPACE;
        case ImGuiKey_Space: return SDLK_SPACE;
        case ImGuiKey_Enter: return SDLK_RETURN;
        case ImGuiKey_Escape: return SDLK_ESCAPE;
        case ImGuiKey_LeftCtrl: return SDLK_LCTRL;
        case ImGuiKey_LeftShift: return SDLK_LSHIFT;
        case ImGuiKey_LeftAlt: return SDLK_LALT;
        case ImGuiKey_LeftSuper: return SDLK_LGUI;
        case ImGuiKey_RightCtrl: return SDLK_RCTRL;
        case ImGuiKey_RightShift: return SDLK_RSHIFT;
        case ImGuiKey_RightAlt: return SDLK_RALT;
        case ImGuiKey_RightSuper: return SDLK_RGUI;
        case ImGuiKey_Menu: return SDLK_MENU;
        case ImGuiKey_0: return SDLK_0;
        case ImGuiKey_1: return SDLK_1;
        case ImGuiKey_2: return SDLK_2;
        case ImGuiKey_3: return SDLK_3;
        case ImGuiKey_4: return SDLK_4;
        case ImGuiKey_5: return SDLK_5;
        case ImGuiKey_6: return SDLK_6;
        case ImGuiKey_7: return SDLK_7;
        case ImGuiKey_8: return SDLK_8;
        case ImGuiKey_9: return SDLK_9;
        case ImGuiKey_A: return SDLK_a;
        case ImGuiKey_B: return SDLK_b;
        case ImGuiKey_C: return SDLK_c;
        case ImGuiKey_D: return SDLK_d;
        case ImGuiKey_E: return SDLK_e;
        case ImGuiKey_F: return SDLK_f;
        case ImGuiKey_G: return SDLK_g;
        case ImGuiKey_H: return SDLK_h;
        case ImGuiKey_I: return SDLK_i;
        case ImGuiKey_J: return SDLK_j;
        case ImGuiKey_K: return SDLK_k;
        case ImGuiKey_L: return SDLK_l;
        case ImGuiKey_M: return SDLK_m;
        case ImGuiKey_N: return SDLK_n;
        case ImGuiKey_O: return SDLK_o;
        case ImGuiKey_P: return SDLK_p;
        case ImGuiKey_Q: return SDLK_q;
        case ImGuiKey_R: return SDLK_r;
        case ImGuiKey_S: return SDLK_s;
        case ImGuiKey_T: return SDLK_t;
        case ImGuiKey_U: return SDLK_u;
        case ImGuiKey_V: return SDLK_v;
        case ImGuiKey_W: return SDLK_w;
        case ImGuiKey_X: return SDLK_x;
        case ImGuiKey_Y: return SDLK_y;
        case ImGuiKey_Z: return SDLK_z;
        case ImGuiKey_F1: return SDLK_F1;
        case ImGuiKey_F2: return SDLK_F2;
        case ImGuiKey_F3: return SDLK_F3;
        case ImGuiKey_F4: return SDLK_F4;
        case ImGuiKey_F5: return SDLK_F5;
        case ImGuiKey_F6: return SDLK_F6;
        case ImGuiKey_F7: return SDLK_F7;
        case ImGuiKey_F8: return SDLK_F8;
        case ImGuiKey_F9: return SDLK_F9;
        case ImGuiKey_F10: return SDLK_F10;
        case ImGuiKey_F11: return SDLK_F11;
        case ImGuiKey_F12: return SDLK_F12;
        default: return SDLK_UNKNOWN;
    }
}

int ImGuiFrontend::getHatDirection(Uint8 hat)
{
    switch (hat) {
        case SDL_HAT_CENTERED: return 0;
        case SDL_HAT_UP: return 1;
        case SDL_HAT_RIGHT: return 2;
        case SDL_HAT_DOWN: return 3;
        case SDL_HAT_LEFT: return 4;
        case SDL_HAT_RIGHTUP: return 5;
        case SDL_HAT_RIGHTDOWN: return 6;
        case SDL_HAT_LEFTUP: return 7;
        case SDL_HAT_LEFTDOWN: return 8;
        default: return 0;
    }
}

void ImGuiFrontend::saveInputConfig()
{
    if (!emuInstance) return;
    
    Config::Table& instcfg = emuInstance->getLocalConfig();
    Config::Table keycfg = instcfg.GetTable("Keyboard");
    Config::Table joycfg = instcfg.GetTable("Joystick");
    
    for (int i = 0; i < 12; i++) {
        const char* btn = emuInstance->buttonNames[i];
        keycfg.SetInt(btn, emuInstance->keyMapping[i]);
        joycfg.SetInt(btn, emuInstance->joyMapping[i]);
    }
    
    for (int i = 0; i < HK_MAX; i++) {
        const char* btn = emuInstance->hotkeyNames[i];
        keycfg.SetInt(btn, emuInstance->hkKeyMapping[i]);
        joycfg.SetInt(btn, emuInstance->hkJoyMapping[i]);
    }
    
    instcfg.SetInt("JoystickID", selectedJoystickID);
    
    Config::Save();
    
    emuInstance->inputLoadConfig();
}

void ImGuiFrontend::loadInputConfig()
{
    if (!emuInstance) return;
    
    emuInstance->inputLoadConfig();
    
    for (int i = 0; i < 12; i++) {
        keyMapping[i] = emuInstance->keyMapping[i];
        joyMapping[i] = emuInstance->joyMapping[i];
    }
    
    for (int i = 0; i < HK_MAX; i++) {
        hkKeyMapping[i] = emuInstance->hkKeyMapping[i];
        hkJoyMapping[i] = emuInstance->hkJoyMapping[i];
    }
    
    Config::Table& instcfg = emuInstance->getLocalConfig();
    selectedJoystickID = instcfg.GetInt("JoystickID");
}

void ImGuiFrontend::saveEmuSettingsOriginals()
{
    if (!emuInstance) return;
    
    auto& globalCfg = emuInstance->getGlobalConfig();
    auto& localCfg = emuInstance->getLocalConfig();
    
    emuSettingsOriginals.externalBIOSEnable = globalCfg.GetBool("Emu.ExternalBIOSEnable");
    emuSettingsOriginals.ds_bios9Path = globalCfg.GetString("DS.Bios9Path");
    emuSettingsOriginals.ds_bios7Path = globalCfg.GetString("DS.Bios7Path");
    emuSettingsOriginals.ds_firmwarePath = globalCfg.GetString("DS.FirmwarePath");
    emuSettingsOriginals.dsi_bios9Path = globalCfg.GetString("DSi.Bios9Path");
    emuSettingsOriginals.dsi_bios7Path = globalCfg.GetString("DSi.Bios7Path");
    emuSettingsOriginals.dsi_firmwarePath = globalCfg.GetString("DSi.FirmwarePath");
    emuSettingsOriginals.dsi_nandPath = globalCfg.GetString("DSi.NANDPath");
    emuSettingsOriginals.dldiEnable = globalCfg.GetBool("DLDI.Enable");
    emuSettingsOriginals.dldiImagePath = globalCfg.GetString("DLDI.ImagePath");
    emuSettingsOriginals.dldiFolderPath = globalCfg.GetString("DLDI.FolderPath");
    emuSettingsOriginals.dldiImageSize = globalCfg.GetInt("DLDI.ImageSize");
    emuSettingsOriginals.dldiReadOnly = globalCfg.GetBool("DLDI.ReadOnly");
    emuSettingsOriginals.dldiFolderSync = globalCfg.GetBool("DLDI.FolderSync");
    emuSettingsOriginals.dsiFullBoot = globalCfg.GetBool("DSi.FullBIOSBoot");
    emuSettingsOriginals.dsiSDEnable = globalCfg.GetBool("DSi.SD.Enable");
    emuSettingsOriginals.dsiSDImagePath = globalCfg.GetString("DSi.SD.ImagePath");
    emuSettingsOriginals.dsiSDFolderPath = globalCfg.GetString("DSi.SD.FolderPath");
    emuSettingsOriginals.dsiSDImageSize = globalCfg.GetInt("DSi.SD.ImageSize");
    emuSettingsOriginals.dsiSDReadOnly = globalCfg.GetBool("DSi.SD.ReadOnly");
    emuSettingsOriginals.dsiSDFolderSync = globalCfg.GetBool("DSi.SD.FolderSync");
    emuSettingsOriginals.consoleType = globalCfg.GetInt("Emu.ConsoleType");
    emuSettingsOriginals.directBoot = globalCfg.GetBool("Emu.DirectBoot");
    emuSettingsOriginals.jitEnable = globalCfg.GetBool("JIT.Enable");
    emuSettingsOriginals.jitBranch = globalCfg.GetBool("JIT.BranchOptimisations");
    emuSettingsOriginals.jitLiteral = globalCfg.GetBool("JIT.LiteralOptimisations");
    emuSettingsOriginals.jitFastMem = globalCfg.GetBool("JIT.FastMemory");
    emuSettingsOriginals.jitMaxBlock = globalCfg.GetInt("JIT.MaxBlockSize");
    emuSettingsOriginals.gdbEnabled = localCfg.GetBool("Gdb.Enabled");
    emuSettingsOriginals.gdbPortARM7 = localCfg.GetInt("Gdb.ARM7.Port");
    emuSettingsOriginals.gdbPortARM9 = localCfg.GetInt("Gdb.ARM9.Port");
    emuSettingsOriginals.gdbBOSARM7 = localCfg.GetBool("Gdb.ARM7.BreakOnStartup");
    emuSettingsOriginals.gdbBOSARM9 = localCfg.GetBool("Gdb.ARM9.BreakOnStartup");
}

bool ImGuiFrontend::checkEmuSettingsChanged()
{
    if (!emuInstance) return false;
    
    auto& globalCfg = emuInstance->getGlobalConfig();
    auto& localCfg = emuInstance->getLocalConfig();
    
    return (emuSettingsOriginals.externalBIOSEnable != globalCfg.GetBool("Emu.ExternalBIOSEnable")) ||
           (emuSettingsOriginals.ds_bios9Path != globalCfg.GetString("DS.Bios9Path")) ||
           (emuSettingsOriginals.ds_bios7Path != globalCfg.GetString("DS.Bios7Path")) ||
           (emuSettingsOriginals.ds_firmwarePath != globalCfg.GetString("DS.FirmwarePath")) ||
           (emuSettingsOriginals.dsi_bios9Path != globalCfg.GetString("DSi.Bios9Path")) ||
           (emuSettingsOriginals.dsi_bios7Path != globalCfg.GetString("DSi.Bios7Path")) ||
           (emuSettingsOriginals.dsi_firmwarePath != globalCfg.GetString("DSi.FirmwarePath")) ||
           (emuSettingsOriginals.dsi_nandPath != globalCfg.GetString("DSi.NANDPath")) ||
           (emuSettingsOriginals.dldiEnable != globalCfg.GetBool("DLDI.Enable")) ||
           (emuSettingsOriginals.dldiImagePath != globalCfg.GetString("DLDI.ImagePath")) ||
           (emuSettingsOriginals.dldiFolderPath != globalCfg.GetString("DLDI.FolderPath")) ||
           (emuSettingsOriginals.dldiImageSize != globalCfg.GetInt("DLDI.ImageSize")) ||
           (emuSettingsOriginals.dldiReadOnly != globalCfg.GetBool("DLDI.ReadOnly")) ||
           (emuSettingsOriginals.dldiFolderSync != globalCfg.GetBool("DLDI.FolderSync")) ||
           (emuSettingsOriginals.dsiFullBoot != globalCfg.GetBool("DSi.FullBIOSBoot")) ||
           (emuSettingsOriginals.dsiSDEnable != globalCfg.GetBool("DSi.SD.Enable")) ||
           (emuSettingsOriginals.dsiSDImagePath != globalCfg.GetString("DSi.SD.ImagePath")) ||
           (emuSettingsOriginals.dsiSDFolderPath != globalCfg.GetString("DSi.SD.FolderPath")) ||
           (emuSettingsOriginals.dsiSDImageSize != globalCfg.GetInt("DSi.SD.ImageSize")) ||
           (emuSettingsOriginals.dsiSDReadOnly != globalCfg.GetBool("DSi.SD.ReadOnly")) ||
           (emuSettingsOriginals.dsiSDFolderSync != globalCfg.GetBool("DSi.SD.FolderSync")) ||
           (emuSettingsOriginals.consoleType != globalCfg.GetInt("Emu.ConsoleType")) ||
           (emuSettingsOriginals.directBoot != globalCfg.GetBool("Emu.DirectBoot")) ||
           (emuSettingsOriginals.jitEnable != globalCfg.GetBool("JIT.Enable")) ||
           (emuSettingsOriginals.jitBranch != globalCfg.GetBool("JIT.BranchOptimisations")) ||
           (emuSettingsOriginals.jitLiteral != globalCfg.GetBool("JIT.LiteralOptimisations")) ||
           (emuSettingsOriginals.jitFastMem != globalCfg.GetBool("JIT.FastMemory")) ||
           (emuSettingsOriginals.jitMaxBlock != globalCfg.GetInt("JIT.MaxBlockSize")) ||
           (emuSettingsOriginals.gdbEnabled != localCfg.GetBool("Gdb.Enabled")) ||
           (emuSettingsOriginals.gdbPortARM7 != localCfg.GetInt("Gdb.ARM7.Port")) ||
           (emuSettingsOriginals.gdbPortARM9 != localCfg.GetInt("Gdb.ARM9.Port")) ||
           (emuSettingsOriginals.gdbBOSARM7 != localCfg.GetBool("Gdb.ARM7.BreakOnStartup")) ||
           (emuSettingsOriginals.gdbBOSARM9 != localCfg.GetBool("Gdb.ARM9.BreakOnStartup"));
}

void ImGuiFrontend::applyEmuSettings()
{
    if (!emuInstance) return;
    
    Config::Save();
    
    emuInstance->osdAddMessage(0x00FF00FF, "Emulator settings applied");
    
    // Note: Some settings may require emulator restart to take effect
    // This is handled by the user restarting the emulator
}

void ImGuiFrontend::renderStatusBar()
{
    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    float statusBarHeight = ImGui::GetFrameHeight();
    ImVec2 statusBarPos = ImVec2(0, displaySize.y - statusBarHeight);
    ImVec2 statusBarSize = ImVec2(displaySize.x, statusBarHeight);
    
    ImGuiWindowFlags statusFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoInputs;
    ImGui::SetNextWindowPos(statusBarPos);
    ImGui::SetNextWindowSize(statusBarSize);
    ImGui::Begin("StatusBar", nullptr, statusFlags);
    
    // Status information
    if (emuInstance && emuInstance->isRunning()) {
        ImGui::Text("FPS: %.1f", currentFPS);
        ImGui::SameLine();
        ImGui::Text("|");
        ImGui::SameLine();
        ImGui::Text("Running");
        if (emuInstance->isPaused()) {
            ImGui::SameLine();
            ImGui::Text("|");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "PAUSED");
        }
    } else {
        ImGui::Text("Ready");
    }
    
    ImGui::End();
}

void ImGuiFrontend::renderDSScreens()
{
    renderDSScreensIntegrated();
}

bool ImGuiFrontend::showErrorPopup = false;
std::string ImGuiFrontend::errorPopupMessage;
void ImGuiFrontend::showErrorDialog(const std::string& message) {
    errorPopupMessage = message;
    showErrorPopup = true;
    ImGui::OpenPopup("Error");
}

void ImGuiFrontend::onInsertCart() {
    auto files = pickROM(false);
    if (files.empty()) return;
    std::string errorstr;
    if (!emuInstance->loadROM(files, false, errorstr)) {
        showErrorDialog(errorstr);
        return;
    }
    updateCartInserted(false);
}

void ImGuiFrontend::onEjectCart() {
    emuInstance->ejectCart();
    updateCartInserted(false);
}

void ImGuiFrontend::onInsertGBACart() {
    auto files = pickROM(true);
    if (files.empty()) return;
    std::string errorstr;
    if (!emuInstance->loadGBAROM(files, errorstr)) {
        showErrorDialog(errorstr);
        return;
    }
    updateCartInserted(true);
}

void ImGuiFrontend::onEjectGBACart() {
    emuInstance->ejectGBACart();
    updateCartInserted(true);
}

void ImGuiFrontend::onSaveState(int slot) {
    std::string filename;
    if (slot > 0) {
        filename = emuInstance->getSavestateName(slot);
    } else {
        filename = FileDialog::saveFile(
            "Save state",
            emuInstance->getConfigDirectory(),
            FileDialog::Filters::SAVESTATE_FILES
        );
        if (filename.empty()) return;
    }
    if (emuInstance->saveState(filename)) {
        if (slot > 0) emuInstance->osdAddMessage(0, ("State saved to slot " + std::to_string(slot)).c_str());
        else emuInstance->osdAddMessage(0, "State saved to file");
    } else {
        emuInstance->osdAddMessage(0xFFA0A0, "State save failed");
    }
}

void ImGuiFrontend::onLoadState(int slot) {
    std::string filename;
    if (slot > 0) {
        filename = emuInstance->getSavestateName(slot);
    } else {
        filename = FileDialog::openFile(
            "Load state",
            emuInstance->getConfigDirectory(),
            FileDialog::Filters::SAVESTATE_FILES
        );
        if (filename.empty()) return;
    }
    FILE* f = fopen(filename.c_str(), "rb");
    if (!f) {
        if (slot > 0) emuInstance->osdAddMessage(0xFFA0A0, ("State slot " + std::to_string(slot) + " is empty").c_str());
        else emuInstance->osdAddMessage(0xFFA0A0, "State file does not exist");
        return;
    }
    fclose(f);
    if (emuInstance->loadState(filename)) {
        if (slot > 0) emuInstance->osdAddMessage(0, ("State loaded from slot " + std::to_string(slot)).c_str());
        else emuInstance->osdAddMessage(0, "State loaded from file");
    } else {
        emuInstance->osdAddMessage(0xFFA0A0, "State load failed");
    }
}

void ImGuiFrontend::onUndoStateLoad() {
    emuInstance->undoStateLoad();
    emuInstance->osdAddMessage(0, "State load undone");
}

void ImGuiFrontend::onImportSavefile() {
    std::string path = FileDialog::openFile(
        "Select savefile",
        emuInstance->getGlobalConfig().GetString("LastROMFolder"),
        FileDialog::Filters::SAVE_FILES
    );
    if (path.empty()) return;
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) {
        showErrorDialog("Could not open the given savefile.");
        return;
    }
    fclose(f);
    if (emuInstance->isRunning()) {
        showErrorDialog("The emulation will be reset and the current savefile overwritten. (Not interactive in ImGui)");
    }
    if (!emuInstance->importSavefile(path)) {
        showErrorDialog("Could not import the given savefile.");
        return;
    }
}

void ImGuiFrontend::onROMInfo() {
    auto cart = emuInstance->getNDS()->NDSCartSlot.GetCart();
    if (cart) {
        showROMInfoDialog = true;
    }
}

void ImGuiFrontend::renderROMInfoDialog() {
    if (!showROMInfoDialog) return;
    
    ImGui::SetNextWindowSize(ImVec2(600, 500), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("ROM Info", &showROMInfoDialog)) {
        auto cart = emuInstance->getNDS()->NDSCartSlot.GetCart();
        if (!cart) {
            ImGui::Text("No cart inserted");
            ImGui::End();
            return;
        }
        
        const NDSBanner* banner = cart->Banner();
        const NDSHeader& header = cart->GetHeader();
        
        if (ImGui::BeginTabBar("ROMInfoTabs")) {
            if (ImGui::BeginTabItem("General")) {
                ImGui::Columns(2, "rominfo");
                
                ImGui::Text("Game Icon");
                ImGui::Dummy(ImVec2(64, 64));
                
                ImGui::Text("Game Title: %s", header.GameTitle);
                ImGui::Text("Game Code: %s", header.GameCode);
                ImGui::Text("Maker Code: %s", header.MakerCode);
                ImGui::Text("Card Size: %d KB", 128 << header.CardSize);
                
                ImGui::NextColumn();
                
                ImGui::Text("Titles:");
                ImGui::Text("Japanese: %ls", banner->JapaneseTitle);
                ImGui::Text("English: %ls", banner->EnglishTitle);
                ImGui::Text("French: %ls", banner->FrenchTitle);
                ImGui::Text("German: %ls", banner->GermanTitle);
                ImGui::Text("Italian: %ls", banner->ItalianTitle);
                ImGui::Text("Spanish: %ls", banner->SpanishTitle);
                if (banner->Version > 1) {
                    ImGui::Text("Chinese: %ls", banner->ChineseTitle);
                }
                if (banner->Version > 2) {
                    ImGui::Text("Korean: %ls", banner->KoreanTitle);
                }
                
                ImGui::Columns(1);
                ImGui::EndTabItem();
            }
            
            if (ImGui::BeginTabItem("ARM9")) {
                ImGui::Text("ARM9 ROM Offset: 0x%08X", header.ARM9ROMOffset);
                ImGui::Text("ARM9 Entry Address: 0x%08X", header.ARM9EntryAddress);
                ImGui::Text("ARM9 RAM Address: 0x%08X", header.ARM9RAMAddress);
                ImGui::Text("ARM9 Size: %d bytes", header.ARM9Size);
                ImGui::EndTabItem();
            }
            
            if (ImGui::BeginTabItem("ARM7")) {
                ImGui::Text("ARM7 ROM Offset: 0x%08X", header.ARM7ROMOffset);
                ImGui::Text("ARM7 Entry Address: 0x%08X", header.ARM7EntryAddress);
                ImGui::Text("ARM7 RAM Address: 0x%08X", header.ARM7RAMAddress);
                ImGui::Text("ARM7 Size: %d bytes", header.ARM7Size);
                ImGui::EndTabItem();
            }
            
            if (ImGui::BeginTabItem("File System")) {
                ImGui::Text("Font Table Offset: 0x%08X", header.FNTOffset);
                ImGui::Text("Font Table Size: %d bytes", header.FNTSize);
                ImGui::Text("FAT Offset: 0x%08X", header.FATOffset);
                ImGui::Text("FAT Size: %d bytes", header.FATSize);
                ImGui::EndTabItem();
            }
            
            ImGui::EndTabBar();
        }
        
        if (ImGui::Button("Save Icon")) {
            std::string filename = FileDialog::saveFile(
                "Save Icon",
                emuInstance->getGlobalConfig().GetString("LastROMFolder"),
                FileDialog::Filters::IMAGE_FILES
            );
            if (!filename.empty()) {
                showErrorDialog("Icon saving not yet implemented in ImGui frontend");
            }
        }
        
        if (banner->Version == 0x103) {
            ImGui::SameLine();
            if (ImGui::Button("Save Animated Icon")) {
                std::string filename = FileDialog::saveFile(
                    "Save Animated Icon",
                    emuInstance->getGlobalConfig().GetString("LastROMFolder"),
                    FileDialog::Filters::IMAGE_FILES
                );
                if (!filename.empty()) {
                    showErrorDialog("Animated icon saving not yet implemented in ImGui frontend");
                }
            }
        }
    }
    ImGui::End();
}

void ImGuiFrontend::onSetupCheats() {
    showCheatsDialog = true;
}

void ImGuiFrontend::renderCheatsDialog() {
    if (!showCheatsDialog) return;
    
    ImGui::SetNextWindowSize(ImVec2(700, 500), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Cheat Codes", &showCheatsDialog)) {
        static auto* codeFile = emuInstance->getCheatFile();
        static int selectedCategory = -1;
        static int selectedCode = -1;
        static char newCodeName[256] = "";
        static char newCodeText[512] = "";
        static bool showAddCategory = false;
        static bool showAddCode = false;
        
        ImGui::BeginChild("CheatList", ImVec2(300, 0), true);
        ImGui::Text("Cheat Categories");
        ImGui::Separator();
        
        if (ImGui::Button("Add Category")) {
            showAddCategory = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete Category") && selectedCategory >= 0) {
            if (selectedCategory < (int)codeFile->Categories.size()) {
                auto it = std::next(codeFile->Categories.begin(), selectedCategory);
                codeFile->Categories.erase(it);
                selectedCategory = -1;
                selectedCode = -1;
            }
        }
        
        ImGui::Separator();
        
        for (size_t i = 0; i < codeFile->Categories.size(); i++) {
            auto it = std::next(codeFile->Categories.begin(), i);
            auto& cat = *it;
            
            bool categorySelected = (selectedCategory == (int)i);
            if (ImGui::Selectable(("##cat" + std::to_string(i)).c_str(), categorySelected)) {
                selectedCategory = (int)i;
                selectedCode = -1;
            }
            ImGui::SameLine();
            
            char catName[256];
            strncpy(catName, cat.Name.c_str(), sizeof(catName) - 1);
            if (ImGui::InputText(("##catname" + std::to_string(i)).c_str(), catName, sizeof(catName))) {
                cat.Name = catName;
            }
            
            ImGui::Indent(20.0f);
            for (size_t j = 0; j < cat.Codes.size(); j++) {
                auto it_code = std::next(cat.Codes.begin(), j);
                auto& code = *it_code;
                
                bool codeSelected = (selectedCategory == (int)i && selectedCode == (int)j);
                if (ImGui::Selectable(("##code" + std::to_string(i) + "_" + std::to_string(j)).c_str(), codeSelected)) {
                    selectedCategory = (int)i;
                    selectedCode = (int)j;
                    strncpy(newCodeName, code.Name.c_str(), sizeof(newCodeName) - 1);
                    std::string codeString;
                    for (size_t k = 0; k + 1 < code.Code.size(); k += 2) {
                        char buf[32];
                        snprintf(buf, sizeof(buf), "%08X %08X\n", code.Code[k], code.Code[k+1]);
                        codeString += buf;
                    }
                    strncpy(newCodeText, codeString.c_str(), sizeof(newCodeText) - 1);
                }
                ImGui::SameLine();
                
                bool enabled = code.Enabled;
                if (ImGui::Checkbox("##enabled", &enabled)) {
                    code.Enabled = enabled;
                }
                ImGui::SameLine();
                
                ImGui::Text("%s", code.Name.c_str());
            }
            ImGui::Unindent(20.0f);
        }
        
        ImGui::EndChild();
        
        ImGui::SameLine();
        
        ImGui::BeginChild("CodeDetails", ImVec2(0, 0), true);
        
        if (selectedCategory >= 0 && selectedCategory < (int)codeFile->Categories.size()) {
            auto it = std::next(codeFile->Categories.begin(), selectedCategory);
            auto& cat = *it;
            
            ImGui::Text("Category: %s", cat.Name.c_str());
            ImGui::Separator();
            
            if (ImGui::Button("Add Code")) {
                showAddCode = true;
                newCodeName[0] = '\0';
                newCodeText[0] = '\0';
            }
            ImGui::SameLine();
            if (ImGui::Button("Delete Code") && selectedCode >= 0 && selectedCode < (int)cat.Codes.size()) {
                cat.Codes.erase(std::next(cat.Codes.begin(), selectedCode));
                selectedCode = -1;
            }
            
            ImGui::Separator();
            
            if (selectedCode >= 0 && selectedCode < (int)cat.Codes.size()) {
                auto it_code = std::next(cat.Codes.begin(), selectedCode);
                auto& code = *it_code;
                
                ImGui::Text("Code Name:");
                if (ImGui::InputText("##codename", newCodeName, sizeof(newCodeName))) {
                    code.Name = newCodeName;
                }
                
                ImGui::Text("Code:");
                if (ImGui::InputTextMultiline("##codetext", newCodeText, sizeof(newCodeText), ImVec2(0, 100))) {
                    std::vector<u32> newCodeVec;
                    std::istringstream iss(newCodeText);
                    std::string line;
                    while (std::getline(iss, line)) {
                        unsigned int c0, c1;
                        if (sscanf(line.c_str(), "%x %x", &c0, &c1) == 2) {
                            newCodeVec.push_back(c0);
                            newCodeVec.push_back(c1);
                        }
                    }
                    code.Code = newCodeVec;
                }
                bool validCode = true;
                std::string validationError;
                std::string cleanCode = newCodeText;
                cleanCode.erase(std::remove(cleanCode.begin(), cleanCode.end(), ' '), cleanCode.end());
                cleanCode.erase(std::remove(cleanCode.begin(), cleanCode.end(), '\n'), cleanCode.end());
                cleanCode.erase(std::remove(cleanCode.begin(), cleanCode.end(), '\r'), cleanCode.end());
                if (!cleanCode.empty()) {
                    if (cleanCode.length() % 16 != 0) {
                        validCode = false;
                        validationError = "Code length must be multiple of 16 hex digits (8 per value)";
                    } else {
                        for (char c : cleanCode) {
                            if (!isxdigit(c)) {
                                validCode = false;
                                validationError = "Code must contain only hexadecimal characters";
                                break;
                            }
                        }
                    }
                }
                if (!validCode) {
                    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Invalid code: %s", validationError.c_str());
                } else {
                    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Valid code");
                }
            } else {
                ImGui::Text("Select a code to edit");
            }
        } else {
            ImGui::Text("Select a category to view codes");
        }
        
        ImGui::EndChild();
        
        if (showAddCategory) {
            ImGui::OpenPopup("Add Category");
            showAddCategory = false;
        }
        
        if (ImGui::BeginPopupModal("Add Category", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            static char newCatName[256] = "";
            ImGui::Text("Enter category name:");
            ImGui::InputText("##newcatname", newCatName, sizeof(newCatName));
            
            if (ImGui::Button("Add")) {
                if (strlen(newCatName) > 0) {
                    ARCodeCat newCat;
                    newCat.Name = newCatName;
                    newCat.Codes.clear();
                    codeFile->Categories.push_back(newCat);
                    newCatName[0] = '\0';
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                newCatName[0] = '\0';
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        
        if (showAddCode) {
            ImGui::OpenPopup("Add Code");
            showAddCode = false;
        }
        
        if (ImGui::BeginPopupModal("Add Code", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            static char addCodeName[256] = "";
            static char addCodeText[512] = "";
            
            ImGui::Text("Enter code name:");
            ImGui::InputText("##addcodename", addCodeName, sizeof(addCodeName));
            ImGui::Text("Enter code:");
            ImGui::InputTextMultiline("##addcodetext", addCodeText, sizeof(addCodeText), ImVec2(300, 100));
            
            if (ImGui::Button("Add")) {
                if (strlen(addCodeName) > 0 && selectedCategory >= 0 && selectedCategory < (int)codeFile->Categories.size()) {
                    ARCode newCode;
                    newCode.Name = addCodeName;
                    newCode.Code.clear();
                    newCode.Enabled = true;
                    auto it = std::next(codeFile->Categories.begin(), selectedCategory);
                    it->Codes.push_back(newCode);
                    addCodeName[0] = '\0';
                    addCodeText[0] = '\0';
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                addCodeName[0] = '\0';
                addCodeText[0] = '\0';
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        
        ImGui::Separator();
        if (ImGui::Button("Save")) {
            codeFile->Save();
        }
        ImGui::SameLine();
        if (ImGui::Button("Load")) {
            codeFile->Load();
        }
        ImGui::SameLine();
        if (ImGui::Button("Close")) {
            showCheatsDialog = false;
        }
    }
    ImGui::End();
}

void ImGuiFrontend::onOpenTitleManager() {
    showTitleManagerDialog = true;
}

void ImGuiFrontend::renderTitleManagerDialog() {
    if (!showTitleManagerDialog) return;
    
    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("DSi Title Manager", &showTitleManagerDialog)) {
        static int selectedTitle = -1;
        static std::vector<std::pair<u32, u32>> titleList;
        static std::vector<std::string> titleNames;
        static std::vector<std::string> titleInfo;
        static bool titlesLoaded = false;
        
        if (!titlesLoaded) {
            titlesLoaded = true;
            titleList.clear();
            titleNames.clear();
            titleInfo.clear();
            
            auto nds = emuInstance->getNDS();
            if (nds && nds->ConsoleType == 1) { // DSi
                const u32 category = 0x00030004;
                std::vector<u32> titles;
                melonDS::DSi_NAND::NANDMount* mountPtr = nullptr;
                if (auto* dsi = dynamic_cast<melonDS::DSi*>(nds)) {
                    mountPtr = new melonDS::DSi_NAND::NANDMount(dsi->GetNAND());
                    mountPtr->ListTitles(category, titles);
                }
                if (mountPtr) {
                    for (u32 titleid : titles) {
                        u32 version;
                        NDSHeader header;
                        NDSBanner banner;
                        mountPtr->GetTitleInfo(category, titleid, version, &header, &banner);
                        titleList.push_back({category, titleid});
                        std::wstring titleWide;
                        for (int i = 0; i < 128 && banner.EnglishTitle[i]; ++i) {
                            titleWide += static_cast<wchar_t>(banner.EnglishTitle[i]);
                        }
                        std::string title = std::string(titleWide.begin(), titleWide.end());
                        title = title.substr(0, title.find('\0'));
                        size_t pos = 0;
                        while ((pos = title.find('\n', pos)) != std::string::npos) {
                            title.replace(pos, 1, " · ");
                        }
                        titleNames.push_back(title);
                        char gamecode[5];
                        memcpy(gamecode, header.GameCode, 4);
                        gamecode[4] = '\0';
                        char info[256];
                        snprintf(info, sizeof(info), "Game Code: %s | Title ID: %08X/%08X | Version: %08X", 
                                gamecode, category, titleid, version);
                        titleInfo.push_back(info);
                    }
                    delete mountPtr;
                }
            }
        }
        
        ImGui::BeginChild("TitleList", ImVec2(400, 0), true);
        ImGui::Text("DSi Titles");
        ImGui::Separator();
        
        if (titleList.empty()) {
            ImGui::Text("No DSi titles found or NAND not mounted");
        } else {
            for (size_t i = 0; i < titleList.size(); i++) {
                bool selected = (selectedTitle == (int)i);
                if (ImGui::Selectable(("##title" + std::to_string(i)).c_str(), selected)) {
                    selectedTitle = (int)i;
                }
                ImGui::SameLine();
                
                ImGui::Dummy(ImVec2(32, 32));
                ImGui::SameLine();
                
                ImGui::BeginGroup();
                ImGui::Text("%s", titleNames[i].c_str());
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s", titleInfo[i].c_str());
                ImGui::EndGroup();
            }
        }
        
        ImGui::EndChild();
        
        ImGui::SameLine();
        
        ImGui::BeginChild("TitleDetails", ImVec2(0, 0), true);
        
        if (selectedTitle >= 0 && selectedTitle < (int)titleList.size()) {
            auto& title = titleList[selectedTitle];
            u32 category = title.first;
            u32 titleid = title.second;
            
            ImGui::Text("Title: %s", titleNames[selectedTitle].c_str());
            ImGui::Text("Info: %s", titleInfo[selectedTitle].c_str());
            ImGui::Separator();
            
            auto nds = emuInstance->getNDS();
            if (nds && nds->ConsoleType == 1) {
                u32 version;
                NDSHeader header;
                NDSBanner banner;
                
                if (auto* dsi = dynamic_cast<melonDS::DSi*>(nds)) {
                    melonDS::DSi_NAND::NANDMount mount(dsi->GetNAND());
                    mount.GetTitleInfo(category, titleid, version, &header, &banner);
                    ImGui::Text("Save Data Sizes:");
                    ImGui::Text("  Public Save: %d bytes", header.DSiPublicSavSize);
                    ImGui::Text("  Private Save: %d bytes", header.DSiPrivateSavSize);
                    ImGui::Text("  Banner Save: %d bytes", (header.AppFlags & 0x04) ? 0x4000 : 0);
                }
            }
            
            ImGui::Separator();
            
            if (ImGui::BeginTabBar("TitleDataTabs")) {
                if (ImGui::BeginTabItem("Import")) {
                    ImGui::Text("Import Title Data:");
                    
                    if (ImGui::Button("Import public.sav")) {
                        std::string filename = FileDialog::openFile(
                            "Import public.sav",
                            emuInstance->getGlobalConfig().GetString("LastROMFolder"),
                            FileDialog::Filters::SAVE_FILES
                        );
                        if (!filename.empty()) {
                            showErrorDialog("Title data import not yet implemented in ImGui frontend");
                        }
                    }
                    
                    if (ImGui::Button("Import private.sav")) {
                        std::string filename = FileDialog::openFile(
                            "Import private.sav",
                            emuInstance->getGlobalConfig().GetString("LastROMFolder"),
                            FileDialog::Filters::SAVE_FILES
                        );
                        if (!filename.empty()) {
                            showErrorDialog("Title data import not yet implemented in ImGui frontend");
                        }
                    }
                    
                    if (ImGui::Button("Import banner.sav")) {
                        std::string filename = FileDialog::openFile(
                            "Import banner.sav",
                            emuInstance->getGlobalConfig().GetString("LastROMFolder"),
                            FileDialog::Filters::SAVE_FILES
                        );
                        if (!filename.empty()) {
                            showErrorDialog("Title data import not yet implemented in ImGui frontend");
                        }
                    }
                    
                    ImGui::EndTabItem();
                }
                
                if (ImGui::BeginTabItem("Export")) {
                    ImGui::Text("Export Title Data:");
                    
                    if (ImGui::Button("Export public.sav")) {
                        std::string filename = FileDialog::saveFile(
                            "Export public.sav",
                            emuInstance->getGlobalConfig().GetString("LastROMFolder"),
                            FileDialog::Filters::SAVE_FILES
                        );
                        if (!filename.empty()) {
                            showErrorDialog("Title data export not yet implemented in ImGui frontend");
                        }
                    }
                    
                    if (ImGui::Button("Export private.sav")) {
                        std::string filename = FileDialog::saveFile(
                            "Export private.sav",
                            emuInstance->getGlobalConfig().GetString("LastROMFolder"),
                            FileDialog::Filters::SAVE_FILES
                        );
                        if (!filename.empty()) {
                            showErrorDialog("Title data export not yet implemented in ImGui frontend");
                        }
                    }
                    
                    if (ImGui::Button("Export banner.sav")) {
                        std::string filename = FileDialog::saveFile(
                            "Export banner.sav",
                            emuInstance->getGlobalConfig().GetString("LastROMFolder"),
                            FileDialog::Filters::SAVE_FILES
                        );
                        if (!filename.empty()) {
                            showErrorDialog("Title data export not yet implemented in ImGui frontend");
                        }
                    }
                    
                    ImGui::EndTabItem();
                }
                
                ImGui::EndTabBar();
            }
            
            ImGui::Separator();
            
            if (ImGui::Button("Delete Title", ImVec2(120, 0))) {
                showErrorDialog("Title deletion not yet implemented in ImGui frontend");
            }
            
        } else {
            ImGui::Text("Select a title to view details");
        }
        
        ImGui::EndChild();
        
        ImGui::Separator();
        if (ImGui::Button("Refresh")) {
            titlesLoaded = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Close")) {
            showTitleManagerDialog = false;
        }
    }
    ImGui::End();
}

void ImGuiFrontend::onPause() {
    if (!emuInstance || !emuInstance->isActive()) return;
    
    auto* thread = emuInstance->getEmuThread();
    if (emuInstance->isPaused()) {
        thread->emuUnpause();
        pausedManually = false;
    } else {
        thread->emuPause();
        pausedManually = true;
    }
}

void ImGuiFrontend::onReset() {
    if (!emuInstance || !emuInstance->isActive()) return;
    
    auto* thread = emuInstance->getEmuThread();
    thread->emuReset();
}

void ImGuiFrontend::onStop() {
    if (!emuInstance || !emuInstance->isActive()) return;
    
    auto* thread = emuInstance->getEmuThread();
    thread->emuStop(true);
}

void ImGuiFrontend::onFrameStep() {
    if (!emuInstance || !emuInstance->isActive()) return;
    
    auto* thread = emuInstance->getEmuThread();
    thread->emuFrameStep();
}

void ImGuiFrontend::updateCartInserted(bool gba) {
    emuInstance->saveConfig();
}

void ImGuiFrontend::onOpenRecentFile(int index) {
    if (index < 0 || index >= (int)recentFiles.size()) return;
    std::string filename = recentFiles[index];
    std::vector<std::string> files = splitArchivePath(filename, true);
    if (files.empty()) {
        showErrorDialog("Could not open the selected recent file.");
        return;
    }
    std::string errorstr;
    if (!emuInstance->loadROM(files, true, errorstr)) {
        showErrorDialog(errorstr);
        return;
    }
    auto* thread = emuInstance->getEmuThread();
    thread->emuRun();
    recentFiles.erase(recentFiles.begin() + index);
    recentFiles.insert(recentFiles.begin(), filename);
    for (size_t i = 1; i < recentFiles.size(); ) {
        if (recentFiles[i] == filename) recentFiles.erase(recentFiles.begin() + i);
        else ++i;
    }
    if ((int)recentFiles.size() > maxRecentFiles) recentFiles.resize(maxRecentFiles);
    Config::Table& cfg = emuInstance->getGlobalConfig();
    for (int i = 0; i < (int)recentFiles.size(); ++i) {
        cfg.SetString("RecentROM." + std::to_string(i), recentFiles[i]);
    }
    cfg.SetInt("RecentROM.Count", (int)recentFiles.size());
    emuInstance->saveConfig();
    updateCartInserted(false);
}

void ImGuiFrontend::onChangeScreenSize(int factor) {
    Config::Table& cfg = emuInstance->getGlobalConfig();
    cfg.SetInt("Screen.WindowScale", factor);
    emuInstance->saveConfig();
    int layout = cfg.GetInt("Screen.Layout");
    int gap = cfg.GetInt("Screen.Gap");
    int w = 256, h = 192;
    int winW = w, winH = h;
    switch (layout) {
        case 0: // Natural (vertical)
        case 1: // Vertical
            winW = w * factor;
            winH = (h * 2 + gap) * factor;
            break;
        case 2: // Horizontal
            winW = (w * 2 + gap) * factor;
            winH = h * factor;
            break;
        case 3: // Hybrid (stacked)
        default:
            winW = w * factor;
            winH = (h * 2 + gap) * factor;
            break;
    }
    SDL_SetWindowSize(window, winW, winH);
}

void ImGuiFrontend::onChangeScreenGap(int gap) {
    Config::Table& cfg = emuInstance->getGlobalConfig();
    cfg.SetInt("Screen.Gap", gap);
    emuInstance->saveConfig();
}

void ImGuiFrontend::onChangeScreenAspect(int aspect, bool top) {
    Config::Table& cfg = emuInstance->getGlobalConfig();
    if (top) {
        cfg.SetInt("Screen.AspectTop", aspect);
    } else {
        cfg.SetInt("Screen.AspectBot", aspect);
    }
    emuInstance->saveConfig();
}

void ImGuiFrontend::onChangeScreenLayout(int layout) {
    Config::Table& cfg = emuInstance->getGlobalConfig();
    cfg.SetInt("Screen.Layout", layout);
    emuInstance->saveConfig();
    int factor = cfg.GetInt("Screen.WindowScale");
    int gap = cfg.GetInt("Screen.Gap");
    int w = 256, h = 192;
    int winW = w, winH = h;
    switch (layout) {
        case 0: // Natural (vertical)
        case 1: // Vertical
            winW = w * factor;
            winH = (h * 2 + gap) * factor;
            break;
        case 2: // Horizontal
            winW = (w * 2 + gap) * factor;
            winH = h * factor;
            break;
        case 3: // Hybrid (stacked)
        default:
            winW = w * factor;
            winH = (h * 2 + gap) * factor;
            break;
    }
    SDL_SetWindowSize(window, winW, winH);
}

void ImGuiFrontend::onChangeScreenRotation(int rotation) {
    Config::Table& cfg = emuInstance->getGlobalConfig();
    cfg.SetInt("Screen.Rotation", rotation);
    emuInstance->saveConfig();
}

void ImGuiFrontend::onRAMInfo() {
    showRAMInfoDialog = true;
}

void ImGuiFrontend::onOpenPowerManagement() {
    showPowerManagementDialog = true;
}

void ImGuiFrontend::onOpenDateTime() {
    showDateTimeDialog = true;
}

void ImGuiFrontend::onOpenInputConfig() {
    showInputConfigDialog = true;
}

void ImGuiFrontend::onOpenVideoSettings() {
    showVideoSettingsDialog = true;
}

void ImGuiFrontend::onOpenCameraSettings() {
    showCameraSettingsDialog = true;
}

void ImGuiFrontend::onOpenAudioSettings() {
    showAudioSettingsDialog = true;
}

void ImGuiFrontend::onOpenMPSettings() {
    showMPSettingsDialog = true;
}

void ImGuiFrontend::onOpenWifiSettings() {
    showWifiSettingsDialog = true;
}

void ImGuiFrontend::onOpenFirmwareSettings() {
    showFirmwareSettingsDialog = true;
}

void ImGuiFrontend::onOpenInterfaceSettings() {
    showInterfaceSettingsDialog = true;
}

void ImGuiFrontend::onOpenPathSettings() {
    showPathSettingsDialog = true;
}

void ImGuiFrontend::onChangeLimitFramerate(bool limit) {
    Config::Table& cfg = emuInstance->getGlobalConfig();
    cfg.SetBool("Emu.LimitFramerate", limit);
    emuInstance->saveConfig();
}

void ImGuiFrontend::onEnableCheats() {
    Config::Table& cfg = emuInstance->getGlobalConfig();
    bool enabled = cfg.GetBool("Emu.EnableCheats");
    cfg.SetBool("Emu.EnableCheats", !enabled);
    emuInstance->saveConfig();
}

void ImGuiFrontend::onChangeAudioSync(bool sync) {
    Config::Table& cfg = emuInstance->getGlobalConfig();
    cfg.SetBool("Audio.Sync", sync);
    emuInstance->saveConfig();
}

void ImGuiFrontend::onOpenFile() {
    showOpenFileDialog = true;
}

void ImGuiFrontend::onOpenEmuSettings() {
    showEmuSettingsDialog = true;
}

void ImGuiFrontend::onChangeIntegerScaling(bool enable) {
    Config::Table& cfg = emuInstance->getGlobalConfig();
    cfg.SetBool("Screen.IntegerScaling", enable);
    emuInstance->saveConfig();
}

void ImGuiFrontend::onChangeSavestateSRAMReloc(bool enable) {
    Config::Table& cfg = emuInstance->getGlobalConfig();
    cfg.SetBool("Savestate.SRAMReloc", enable);
    emuInstance->saveConfig();
}

void ImGuiFrontend::onChangeScreenSwap(bool enable) {
    Config::Table& cfg = emuInstance->getGlobalConfig();
    cfg.SetBool("Screen.Swap", enable);
    emuInstance->saveConfig();
}

void ImGuiFrontend::onOpenNewWindow() {
    requestNewWindowFlag = true;
}

void ImGuiFrontend::onChangeShowOSD(bool show) {
    Config::Table& cfg = emuInstance->getGlobalConfig();
    cfg.SetBool("OSD.Show", show);
    emuInstance->saveConfig();
}

void ImGuiFrontend::onChangeScreenFiltering(bool enable) {
    Config::Table& cfg = emuInstance->getGlobalConfig();
    cfg.SetBool("Screen.Filtering", enable);
    emuInstance->saveConfig();
}

void ImGuiFrontend::onQuit() {
    requestQuitFlag = true;
}

void ImGuiFrontend::onBootFirmware() {
    requestBootFirmwareFlag = true;
}

void ImGuiFrontend::onClearRecentFiles() {
    recentFiles.clear();
    Config::Table& cfg = emuInstance->getGlobalConfig();
    for (int i = 0; i < maxRecentFiles; ++i) {
        cfg.SetString("RecentROM[" + std::to_string(i) + "]", "");
    }
    emuInstance->saveConfig();
}

void ImGuiFrontend::onLANStartHost() {
    requestLANHostFlag = true;
}

void ImGuiFrontend::onMPNewInstance() {
    requestMPNewInstanceFlag = true;
}

void ImGuiFrontend::onChangeScreenSizing(int sizing) {
    Config::Table& cfg = emuInstance->getGlobalConfig();
    cfg.SetInt("Screen.Sizing", sizing);
    emuInstance->saveConfig();
}

void ImGuiFrontend::onLANStartClient() {
}

void ImGuiFrontend::initScreenTextures() {
    if (texturesInitialized) return;
    glGenTextures(1, &topScreenTexture);
    glBindTexture(GL_TEXTURE_2D, topScreenTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 256, 192, 0, GL_BGRA, GL_UNSIGNED_BYTE, nullptr);

    glGenTextures(1, &bottomScreenTexture);
    glBindTexture(GL_TEXTURE_2D, bottomScreenTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 256, 192, 0, GL_BGRA, GL_UNSIGNED_BYTE, nullptr);

    texturesInitialized = true;
}

void ImGuiFrontend::renderSplashScreen() {
    ImGui::SetCursorPos(ImVec2(10, 10));
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "melonDS ImGui Frontend");
    ImGui::SetCursorPos(ImVec2(10, 40));
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "File->Open ROM... to get started");
    ImGui::SetCursorPos(ImVec2(10, 70));
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "DSi firmware is booting...");
    
    static float loadingAngle = 0.0f;
    loadingAngle += 2.0f;
    if (loadingAngle > 360.0f) loadingAngle -= 360.0f;
    
    ImGui::SetCursorPos(ImVec2(10, 100));
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Loading...");
}

void ImGuiFrontend::updateScreenTextures() {
    std::cout << "[updateScreenTextures] Called - emuInstance=" << emuInstance << ", isRunning=" << (emuInstance ? emuInstance->isRunning() : false) << std::endl;
    if (!emuInstance || !emuInstance->isRunning()) return;
    void* topBuf = emuInstance->getScreenBuffer(0);
    void* botBuf = emuInstance->getScreenBuffer(1);
    std::cout << "[updateScreenTextures] Got buffers - topBuf=" << topBuf << ", botBuf=" << botBuf << std::endl;
    if (!topBuf || !botBuf) {
        std::cout << "[updateScreenTextures] Screen buffer(s) null, skipping texture update. topBuf=" << topBuf << ", botBuf=" << botBuf << std::endl;
        return;
    }
    
    uint32_t* topData = (uint32_t*)topBuf;
    uint32_t* botData = (uint32_t*)botBuf;
    std::cout << "[updateScreenTextures] Top screen first pixel: 0x" << std::hex << topData[0] << std::dec << ", Bottom screen first pixel: 0x" << std::hex << botData[0] << std::dec << std::endl;
    
    lastTopScreen.assign((uint32_t*)topBuf, (uint32_t*)topBuf + 256*192);
    lastBottomScreen.assign((uint32_t*)botBuf, (uint32_t*)botBuf + 256*192);
    hasLastScreen = true;
    
    if (texturesInitialized) {
        makeCurrentGL();
        
        static uint32_t lastTopPixel = 0xffffffff;
        static uint32_t lastBottomPixel = 0xffffffff;
        uint32_t currentTopPixel = ((uint32_t*)topBuf)[0];
        uint32_t currentBottomPixel = ((uint32_t*)botBuf)[0];
        
        if (currentTopPixel != lastTopPixel || currentBottomPixel != lastBottomPixel) {
            std::cout << "[updateScreenTextures] Screen data changed! Top: 0x" << std::hex << currentTopPixel 
                      << ", Bottom: 0x" << currentBottomPixel << std::dec << std::endl;
            lastTopPixel = currentTopPixel;
            lastBottomPixel = currentBottomPixel;
        }
        
        glBindTexture(GL_TEXTURE_2D, topScreenTexture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 192, GL_BGRA, GL_UNSIGNED_BYTE, topBuf);
        
        glBindTexture(GL_TEXTURE_2D, bottomScreenTexture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 192, GL_BGRA, GL_UNSIGNED_BYTE, botBuf);
        
        GLenum err = glGetError();
        if (err != GL_NO_ERROR) {
            std::cout << "[updateScreenTextures] OpenGL error: " << err << std::endl;
        }
    }
}