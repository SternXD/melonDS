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

#ifndef IMGUIEMUINSTANCE_H
#define IMGUIEMUINSTANCE_H

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <array>
#include <cmath>
#include <SDL.h>
#include "Config.h"

// Hotkey constants
#include "HotkeyConstants.h"

#include "../../types.h"
#include "../../NDS.h"
#include "../../DSi.h"
#include "../../NDSCart.h"
#include "../../GBACart.h"
#include "../../Savestate.h"
#include "../../Platform.h"
#include "ImGuiEmuThread.h"
#include "ImGuiSaveManager.h"

namespace melonDS {
    class NDS;
    class DSi;
    class Firmware;
    class FATStorage;
    namespace DSi_NAND {
        class NANDImage;
    }
    class ARCodeFile;
}

class ImGuiEmuThread;

class ImGuiEmuInstance
{
public:
    ImGuiEmuInstance(int id);
    ~ImGuiEmuInstance();

    int getInstanceID() const { return instanceID; }
    int getConsoleType() const { return consoleType; }
    ImGuiEmuThread* getEmuThread() { return emuThread; }
    melonDS::NDS* getNDS(); // Returns pointer to NDS or DSi as NDS*
    melonDS::DSi* getDSi(); // Returns pointer to DSi if available

    Config::Table& getGlobalConfig();
    Config::Table& getLocalConfig();

    bool loadROM(const std::vector<std::string>& filepath, bool reset, std::string& errorstr);
    bool loadGBAROM(const std::vector<std::string>& filepath, std::string& errorstr);
    void ejectCart();
    void ejectGBACart();
    bool hasCart() const;
    bool hasGBACart() const;
    std::string getCartLabel();
    std::string getGBACartLabel();
    bool loadGBAAddon(int type, std::string& errorstr);
    std::string gbaAddonName(int addon);

    bool saveState(const std::string& filename);
    bool loadState(const std::string& filename);
    void undoStateLoad();
    std::string getSavestateName(int slot);
    bool savestateExists(int slot);

    melonDS::ARCodeFile* getCheatFile();
    void enableCheats(bool enable);

    void osdAddMessage(unsigned int color, const char* msg);
    std::vector<std::pair<std::string, unsigned int>> getOSDMessages() const { return osdMessages; }
    void clearOSDMessages() { osdMessages.clear(); }

    void inputInit();
    void inputDeInit();
    void inputProcess();
    void onKeyPress(SDL_KeyboardEvent* event);
    void onKeyRelease(SDL_KeyboardEvent* event);
    void onMouseClick(int x, int y);
    void onMouseRelease(int button, int x, int y);

    void* getScreenBuffer(int screen);

    void saveConfig();
    void loadConfig();
    std::string getConfigDirectory();
    std::string instanceFileSuffix();

    void setFirmwarePath(const std::string& path);
    void setDSiFirmwarePath(const std::string& path);
    void setDSiNANDPath(const std::string& path);

    bool isRunning() const { return running; }
    bool isActive() const { 
        if (!running) {
            return false;
        }
        if (nds) {
            return nds->IsRunning();
        }
        if (dsi) {
            return dsi->IsRunning();
        }
        return false;
    }
    bool isPaused() const { return paused; }

    std::string verifySetup();

    static const int GBAAddon_RAMExpansion = 0;
    static const int GBAAddon_RumblePak = 1;
    static const int GBAAddon_SolarSensorBoktai1 = 2;
    static const int GBAAddon_SolarSensorBoktai2 = 3;
    static const int GBAAddon_SolarSensorBoktai3 = 4;
    static const int GBAAddon_MotionPakHomebrew = 5;
    static const int GBAAddon_MotionPakRetail = 6;
    static const int GBAAddon_GuitarGrip = 7;

    void reset();
    void frameStep();
    void start();
    void stop();
    void pause();
    void resume();
    bool importSavefile(const std::string& filename);
    bool bootFirmware(std::string& errorstr);
    bool loadROMData(const std::vector<std::string>& filepath, std::unique_ptr<melonDS::u8[]>& filedata, melonDS::u32& filelen, std::string& basepath, std::string& romname);
    std::string verifyDSBIOS();
    std::string verifyDSiBIOS();
    std::string verifyDSFirmware();
    std::string verifyDSiFirmware();
    std::string verifyDSiNAND();
    bool bootToMenu(std::string& errorstr);
    void customizeFirmware(melonDS::Firmware& firmware, bool overridesettings) noexcept;
    bool parseMacAddress(void* data);

    // Touch state for ImGui frontend
    bool isTouching = false;
    int touchX = 0, touchY = 0;
    // Controller touch cursor
    int touchCursorX = 128, touchCursorY = 96;
    
    // Input mapping arrays
    int keyMapping[12];
    int joyMapping[12];
    int hkKeyMapping[HK_MAX];
    int hkJoyMapping[HK_MAX];
    
    // Joystick/controller state
    int joystickID;
    SDL_Joystick* joystick;
    SDL_GameController* controller;
    bool hasAccelerometer = false;
    bool hasGyroscope = false;
    bool hasRumble = false;
    bool isRumbling = false;
    
    // Input masks
    melonDS::u32 keyInputMask, joyInputMask;
    melonDS::u32 keyHotkeyMask, joyHotkeyMask;
    melonDS::u32 hotkeyMask, lastHotkeyMask;
    melonDS::u32 hotkeyPress, hotkeyRelease;
    melonDS::u32 inputMask;

    // Audio system
    void audioInit();
    void audioDeInit();
    void audioEnable();
    void audioDisable();
    void audioMute();
    void audioSync();
    void audioUpdateSettings();
    
    void micOpen();
    void micClose();
    void micLoadWav(const std::string& name);
    void micProcess();
    void setupMicInputData();
    
    int audioGetNumSamplesOut(int outlen);
    void audioResample(melonDS::s16* inbuf, int inlen, melonDS::s16* outbuf, int outlen, int volume);
    
    static void audioCallback(void* data, Uint8* stream, int len);
    static void micCallback(void* data, Uint8* stream, int len);

    // Audio state
    SDL_AudioDeviceID audioDevice;
    int audioFreq;
    int audioBufSize;
    float audioSampleFrac;
    bool audioMuted;
    SDL_cond* audioSyncCond;
    SDL_mutex* audioSyncLock;
    
    int mpAudioMode;
    
    SDL_AudioDeviceID micDevice;
    melonDS::s16 micExtBuffer[4096];
    melonDS::u32 micExtBufferWritePos;
    melonDS::u32 micExtBufferCount;
    
    melonDS::u32 micWavLength;
    melonDS::s16* micWavBuffer;
    
    melonDS::s16* micBuffer;
    melonDS::u32 micBufferLength;
    melonDS::u32 micBufferReadPos;
    
    SDL_mutex* micLock;
    
    int audioVolume;
    bool audioDSiVolumeSync;
    int micInputType;
    std::string micDeviceName;
    std::string micWavPath;

    // Input system constants and names
    static const char* buttonNames[12];
    static const char* hotkeyNames[HK_MAX];
    
    // Input system functions
    void inputLoadConfig();
    void inputRumbleStart(melonDS::u32 len_ms);
    void inputRumbleStop();
    float inputMotionQuery(melonDS::Platform::MotionQueryType type);
    void setJoystick(int id);
    void openJoystick();
    void closeJoystick();
    bool joystickButtonDown(int val);
    void touchScreen(int x, int y);
    void releaseScreen();
    melonDS::u32 convertSDLKeyToMask(SDL_Keycode key);
    SDL_Joystick* getJoystick() { return joystick; }
    SDL_GameController* getController() { return controller; }
    
    bool hotkeyDown(int id)     { return hotkeyMask    & (1<<id); }
    bool hotkeyPressed(int id)  { return hotkeyPress   & (1<<id); }
    bool hotkeyReleased(int id) { return hotkeyRelease & (1<<id); }
    
    void keyReleaseAll();

private:
    int instanceID;
    int consoleType; // 0 = DS, 1 = DSi
    std::unique_ptr<melonDS::NDS> nds;
    std::unique_ptr<melonDS::DSi> dsi;

    ImGuiEmuThread* emuThread;
    ImGuiSaveManager* saveManager;

    // State tracking
    bool cartInserted;
    bool gbaCartInserted;
    bool paused;
    bool running;

    // Configuration
    Config::Table config;
    Config::Table globalConfig;
    Config::Table localConfig;

    // State management
    std::unique_ptr<melonDS::Savestate> backupState;
    bool savestateLoaded;
    std::string previousSaveFile;

    // GBA cart state
    int gbaCartType;
    std::string baseGBAROMDir;
    std::string baseGBAROMName;
    std::string baseGBAAssetName;
    
    // Pending GBA addon for when emulation isn't running
    std::unique_ptr<melonDS::GBACart::CartCommon> pendingGBAAddon;
    int pendingGBAAddonType = -1;

    // Cheat file
    std::unique_ptr<melonDS::ARCodeFile> cheatFile;
    bool cheatsOn;

    // OSD messages
    std::vector<std::pair<std::string, unsigned int>> osdMessages;

    // Save managers
    std::unique_ptr<ImGuiSaveManager> ndsSave;
    std::unique_ptr<ImGuiSaveManager> gbaSave;
    std::unique_ptr<ImGuiSaveManager> firmwareSave;

    std::unique_ptr<melonDS::ARM9BIOSImage> loadARM9BIOS() noexcept;
    std::unique_ptr<melonDS::ARM7BIOSImage> loadARM7BIOS() noexcept;
    std::unique_ptr<melonDS::DSiBIOSImage> loadDSiARM9BIOS() noexcept;
    std::unique_ptr<melonDS::DSiBIOSImage> loadDSiARM7BIOS() noexcept;
    std::optional<melonDS::Firmware> loadFirmware(int type) noexcept;
    std::optional<melonDS::DSi_NAND::NANDImage> loadNAND(const melonDS::DSiBIOSImage& arm7ibios) noexcept;
    std::optional<melonDS::FATStorageArgs> getSDCardArgs(const std::string& key) noexcept;
    std::optional<melonDS::FATStorage> loadSDCard(const std::string& configKey) noexcept;
    std::string getEffectiveFirmwareSavePath();
    void initFirmwareSaveManager();
    void setBatteryLevels();
    void setDateTime();
};

#endif // IMGUIEMUINSTANCE_H