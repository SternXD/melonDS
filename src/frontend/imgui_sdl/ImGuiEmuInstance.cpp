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

#include "ImGuiEmuInstance.h"
#include "Config.h"
#include "../qt_sdl/Config.h"
#include "NDS.h"
#include "DSi.h"
#include "Args.h"
#include "Savestate.h"
#include "NDSCart.h"
#include "GBACart.h"
#include "SPI_Firmware.h"
#include "DSi_NAND.h"
#include "FATStorage.h"
#include "Platform.h"
#include "FreeBIOS.h"
#include "GPU3D_Soft.h"
#include <SDL.h>
#include <memory>
#include <optional>
#include <iostream>
#include <ctime>
#include "ImGuiSaveManager.h"
#include <windows.h>
#include <libgen.h>
#include <cmath>
#include "../../net/Net.h"

class EmuThread;
class SaveManager;

ImGuiEmuInstance::ImGuiEmuInstance(int id)
    : instanceID(id)
    , consoleType(0)
    , nds(nullptr)
    , dsi(nullptr)
    , emuThread(nullptr)
    , saveManager(nullptr)
    , cartInserted(false)
    , gbaCartInserted(false)
    , paused(false)
    , running(false)
    , config(Config::GetGlobalTable())
    , globalConfig(Config::GetGlobalTable())
    , localConfig(Config::GetLocalTable(instanceID))
    , backupState(nullptr)
    , savestateLoaded(false)
    , previousSaveFile("")
    , gbaCartType(-1)
    , baseGBAROMDir("")
    , baseGBAROMName("")
    , baseGBAAssetName("")
    , audioDevice(0)
    , audioFreq(48000)
    , audioBufSize(1024)
    , audioSampleFrac(0.0f)
    , audioMuted(false)
    , audioSyncCond(nullptr)
    , audioSyncLock(nullptr)
    , mpAudioMode(0)
    , micDevice(0)
    , micExtBufferWritePos(0)
    , micExtBufferCount(0)
    , micWavLength(0)
    , micWavBuffer(nullptr)
    , micBuffer(nullptr)
    , micBufferLength(0)
    , micBufferReadPos(0)
    , micLock(nullptr)
    , audioVolume(256)
    , audioDSiVolumeSync(false)
    , micInputType(0)
    , joystickID(0)
    , joystick(nullptr)
    , controller(nullptr)
    , hasAccelerometer(false)
    , hasGyroscope(false)
    , hasRumble(false)
    , isRumbling(false)
    , keyInputMask(0xFFF)
    , joyInputMask(0xFFF)
    , keyHotkeyMask(0)
    , joyHotkeyMask(0)
    , hotkeyMask(0)
    , lastHotkeyMask(0)
    , hotkeyPress(0)
    , hotkeyRelease(0)
    ,     inputMask(0xFFF)
{
    inputInit();
    audioInit();
    consoleType = globalConfig.GetInt("Emu.ConsoleType");
    
    emuThread = new ImGuiEmuThread(this);
    emuThread->start();
    saveManager = new ImGuiSaveManager("");

    extern melonDS::Net net;
    net.RegisterInstance(instanceID);
}

ImGuiEmuInstance::~ImGuiEmuInstance()
{
    if (emuThread) {
        emuThread->stop();
        delete emuThread;
    }
    if (saveManager) {
        delete saveManager;
    }
    inputDeInit();
    audioDeInit();

    extern melonDS::Net net;
    net.UnregisterInstance(instanceID);
}

bool ImGuiEmuInstance::loadROM(const std::vector<std::string>& filepath, bool reset, std::string& errorstr)
{
    if (filepath.empty()) {
        errorstr = "No file path provided";
        return false;
    }

    std::unique_ptr<melonDS::u8[]> filedata;
    melonDS::u32 filelen;
    std::string basepath;
    std::string romname;

    if (!loadROMData(filepath, filedata, filelen, basepath, romname)) {
        errorstr = "Failed to load ROM data";
        return false;
    }

    bool isDSi = false;
    if (filelen >= 0x200) {
        melonDS::u8 consoleType = filedata[0x12];
        isDSi = (consoleType == 0x03);
    }

    bool directBoot = globalConfig.GetBool("Emu.DirectBoot");

    if (!isDSi) {
        auto arm9bios = loadARM9BIOS();
        auto arm7bios = loadARM7BIOS();
        std::optional<melonDS::Firmware> firmware;
        if (!directBoot) {
            firmware = loadFirmware(0);
            if (!firmware) {
                errorstr = "Failed to load DS firmware";
                return false;
            }
        }
#ifdef JIT_ENABLED
        auto jitopt = globalConfig.GetTable("JIT");
        melonDS::JITArgs _jitargs {
                static_cast<unsigned>(jitopt.GetInt("MaxBlockSize")),
                jitopt.GetBool("LiteralOptimisations"),
                jitopt.GetBool("BranchOptimisations"),
                jitopt.GetBool("FastMemory"),
        };
        auto jitargs = jitopt.GetBool("Enable") ? std::make_optional(_jitargs) : std::nullopt;
#else
        std::optional<melonDS::JITArgs> jitargs = std::nullopt;
#endif

        melonDS::NDSArgs args{
            std::move(arm9bios),
            std::move(arm7bios),
            directBoot ? melonDS::Firmware(0) : std::move(*firmware),
            jitargs,
            static_cast<melonDS::AudioBitDepth>(globalConfig.GetInt("Audio.BitDepth")),
            static_cast<melonDS::AudioInterpolation>(globalConfig.GetInt("Audio.Interpolation")),
            std::nullopt
        };
        nds = std::make_unique<melonDS::NDS>(std::move(args), this);
        auto cart = melonDS::NDSCart::ParseROM(filedata.get(), filelen, this);
        if (cart) {
            nds->SetNDSCart(std::move(cart));
            cartInserted = true;
        }
        consoleType = 0;
        if (directBoot && nds) {
            nds->SetupDirectBoot(romname);
        }
    } else {
        std::string biosErr = verifyDSiBIOS();
        if (!biosErr.empty()) {
            errorstr = biosErr;
            return false;
        }
        std::string fwErr = verifyDSiFirmware();
        if (!fwErr.empty()) {
            errorstr = fwErr;
            return false;
        }
        std::string nandErr = verifyDSiNAND();
        if (!nandErr.empty()) {
            errorstr = nandErr;
            return false;
        }
        auto arm9bios = loadARM9BIOS();
        if (!arm9bios) {
            errorstr = "Failed to load DSi ARM9 BIOS.";
            return false;
        }
        auto arm7bios = loadARM7BIOS();
        if (!arm7bios) {
            errorstr = "Failed to load DSi ARM7 BIOS.";
            return false;
        }
        auto arm9ibios = loadDSiARM9BIOS();
        if (!arm9ibios) {
            errorstr = "Failed to load DSi ARM9i BIOS.";
            return false;
        }
        auto arm7ibios = loadDSiARM7BIOS();
        if (!arm7ibios) {
            errorstr = "Failed to load DSi ARM7i BIOS.";
            return false;
        }
        auto nand = loadNAND(*arm7ibios);
        if (!nand) {
            errorstr = "Failed to load DSi NAND";
            return false;
        }
        auto sdcard = loadSDCard("DSi.SD");
        bool fullBIOSBoot = globalConfig.GetBool("DSi.FullBIOSBoot");
#ifdef JIT_ENABLED
        auto jitopt = globalConfig.GetTable("JIT");
        melonDS::JITArgs _jitargs {
                static_cast<unsigned>(jitopt.GetInt("MaxBlockSize")),
                jitopt.GetBool("LiteralOptimisations"),
                jitopt.GetBool("BranchOptimisations"),
                jitopt.GetBool("FastMemory"),
        };
        auto jitargs = jitopt.GetBool("Enable") ? std::make_optional(_jitargs) : std::nullopt;
#else
        std::optional<melonDS::JITArgs> jitargs = std::nullopt;
#endif

        melonDS::DSiArgs args{
            melonDS::NDSArgs{
                std::move(arm9bios),
                std::move(arm7bios),
                melonDS::Firmware(0),
                jitargs,
                static_cast<melonDS::AudioBitDepth>(globalConfig.GetInt("Audio.BitDepth")),
                static_cast<melonDS::AudioInterpolation>(globalConfig.GetInt("Audio.Interpolation")),
                std::nullopt
            },
            std::move(arm9ibios),
            std::move(arm7ibios),
            std::move(*nand),
            std::move(sdcard),
            fullBIOSBoot
        };
        dsi = std::make_unique<melonDS::DSi>(std::move(args), this);
        auto cart = melonDS::NDSCart::ParseROM(filedata.get(), filelen, this);
        if (cart) {
            dsi->SetNDSCart(std::move(cart));
            cartInserted = true;
        }
        consoleType = 1;
        if (directBoot && dsi) {
        }
    }

    if (reset) {
        this->reset();
    }

    return true;
}

bool ImGuiEmuInstance::loadGBAROM(const std::vector<std::string>& filepath, std::string& errorstr)
{
    if (filepath.empty()) {
        errorstr = "No file path provided";
        return false;
    }

    std::unique_ptr<melonDS::u8[]> filedata;
    melonDS::u32 filelen;
    std::string basepath;
    std::string romname;

    if (!loadROMData(filepath, filedata, filelen, basepath, romname)) {
        errorstr = "Failed to load ROM data";
        return false;
    }

    auto cart = melonDS::GBACart::ParseROM(filedata.get(), filelen, this);
    if (!cart) {
        errorstr = "Failed to parse GBA ROM";
        return false;
    }

    if (nds) {
        nds->SetGBACart(std::move(cart));
        gbaCartInserted = true;
    } else if (dsi) {
        dsi->SetGBACart(std::move(cart));
        gbaCartInserted = true;
    } else {
        errorstr = "No console instance available";
        return false;
    }

    gbaCartType = 0;
    baseGBAROMDir = basepath;
    baseGBAROMName = romname;
    baseGBAAssetName = romname;

    return true;
}

bool ImGuiEmuInstance::saveState(const std::string& filename)
{
    melonDS::Savestate savestate(melonDS::Savestate::DEFAULT_SIZE);
    if (savestate.Error) return false;
    
    bool success = false;
    if (nds) {
        success = nds->DoSavestate(&savestate);
    } else if (dsi) {
        success = static_cast<melonDS::NDS*>(dsi.get())->DoSavestate(&savestate);
    }
    
    if (!success || savestate.Error) return false;
    
    auto file = melonDS::Platform::OpenFile(filename, melonDS::Platform::FileMode::Write);
    if (!file) return false;
    
    bool writeSuccess = (melonDS::Platform::FileWrite(savestate.Buffer(), savestate.Length(), 1, file) == 1);
    melonDS::Platform::CloseFile(file);
    
    return writeSuccess;
}

bool ImGuiEmuInstance::loadState(const std::string& filename)
{
    if ((nds || dsi) && running)
    {
        backupState = std::make_unique<melonDS::Savestate>(melonDS::Savestate::DEFAULT_SIZE);
        if (!backupState->Error)
        {
            bool backupSuccess = false;
            if (nds)
            {
                backupSuccess = nds->DoSavestate(backupState.get());
            }
            else if (dsi)
            {
                backupSuccess = static_cast<melonDS::NDS*>(dsi.get())->DoSavestate(backupState.get());
            }
            
            if (!backupSuccess || backupState->Error)
            {
                backupState.reset();
            }
        }
        else
        {
            backupState.reset();
        }
    }

    auto file = melonDS::Platform::OpenFile(filename, melonDS::Platform::FileMode::Read);
    if (!file) return false;
    
    melonDS::u32 filelen = melonDS::Platform::FileLength(file);
    auto savestateData = std::make_unique<melonDS::u8[]>(filelen);
    if (melonDS::Platform::FileRead(savestateData.get(), 1, filelen, file) != filelen) {
        melonDS::Platform::CloseFile(file);
        return false;
    }
    melonDS::Platform::CloseFile(file);
    
    melonDS::Savestate savestate(savestateData.get(), filelen, false);
    if (savestate.Error) return false;
    
    bool success = false;
    if (nds) {
        success = nds->DoSavestate(&savestate);
    } else if (dsi) {
        success = static_cast<melonDS::NDS*>(dsi.get())->DoSavestate(&savestate);
    }
    
    if (success && !savestate.Error)
    {
        savestateLoaded = true;
    }
    
    return success && !savestate.Error;
}

void ImGuiEmuInstance::ejectCart()
{
    if (nds) {
        nds->EjectCart();
    } else if (dsi) {
        dsi->EjectCart();
    }
    cartInserted = false;
}

void ImGuiEmuInstance::ejectGBACart()
{
    if (nds) {
        nds->EjectGBACart();
    } else if (dsi) {
        dsi->EjectGBACart();
    }
    gbaCartInserted = false;
    gbaCartType = -1;
    baseGBAROMDir = "";
    baseGBAROMName = "";
    baseGBAAssetName = "";
}

bool ImGuiEmuInstance::hasCart() const {
    if (nds) return nds->CartInserted();
    if (dsi) return dsi->CartInserted();
    return false;
}

std::string ImGuiEmuInstance::getCartLabel() {
    if (nds && nds->CartInserted()) {
        auto cart = nds->GetNDSCart();
        if (cart) return std::string(cart->GetHeader().GameTitle);
    } else if (dsi && dsi->CartInserted()) {
        auto cart = dsi->GetNDSCart();
        if (cart) return std::string(cart->GetHeader().GameTitle);
    }
    return "";
}

std::string ImGuiEmuInstance::getGBACartLabel() {
    if (gbaCartType != -1 && gbaCartType != 0) {
        return gbaAddonName(gbaCartType);
    }
    if (nds) {
        auto cart = nds->GetGBACart();
        if (cart && cart->GetROM()) {
            auto gameCart = dynamic_cast<melonDS::GBACart::CartGame*>(cart);
            if (gameCart) return std::string(gameCart->GetHeader().Title);
        }
    } else if (dsi) {
        auto cart = dsi->GetGBACart();
        if (cart && cart->GetROM()) {
            auto gameCart = dynamic_cast<melonDS::GBACart::CartGame*>(cart);
            if (gameCart) return std::string(gameCart->GetHeader().Title);
        }
    }
    return "";
}

void* ImGuiEmuInstance::getScreenBuffer(int screen)
{
    if (nds) {
        int frontbuf = nds->GPU.FrontBuffer;
        return nds->GPU.Framebuffer[frontbuf][screen].get();
    } else if (dsi) {
        if (!dsi->IsRunning()) {
            std::cout << "[getScreenBuffer] DSi not running yet, returning nullptr" << std::endl;
            return nullptr;
        }
        int frontbuf = dsi->GPU.FrontBuffer;
        void* buffer = dsi->GPU.Framebuffer[frontbuf][screen].get();
        std::cout << "[getScreenBuffer] DSi screen " << screen << " buffer: " << buffer << " (frontbuf=" << frontbuf << ")" << std::endl;
        return buffer;
    }
    return nullptr;
}

void ImGuiEmuInstance::onKeyPress(SDL_KeyboardEvent* event)
{
    int key = event->keysym.sym;
    int mod = event->keysym.mod;
    
    int keyVal = key | mod;
    
    for (int i = 0; i < 12; i++) {
        if (keyVal == keyMapping[i]) {
            keyInputMask &= ~(1 << i);
        }
    }
    
    for (int i = 0; i < HK_MAX; i++) {
        if (keyVal == hkKeyMapping[i]) {
            keyHotkeyMask |= (1 << i);
        }
    }
}

void ImGuiEmuInstance::onKeyRelease(SDL_KeyboardEvent* event)
{
    int key = event->keysym.sym;
    int mod = event->keysym.mod;
    
    int keyVal = key | mod;
    
    for (int i = 0; i < 12; i++) {
        if (keyVal == keyMapping[i]) {
            keyInputMask |= (1 << i);
        }
    }
    
    for (int i = 0; i < HK_MAX; i++) {
        if (keyVal == hkKeyMapping[i]) {
            keyHotkeyMask &= ~(1 << i);
        }
    }
}

void ImGuiEmuInstance::onMouseClick(int x, int y)
{
    isTouching = true;
    touchX = x;
    touchY = y;
}

void ImGuiEmuInstance::onMouseRelease(int button, int x, int y)
{
    isTouching = false;
}

std::string ImGuiEmuInstance::gbaAddonName(int addon)
{
    switch (addon)
    {
    case melonDS::GBAAddon_RumblePak:
        return "Rumble Pak";
    case melonDS::GBAAddon_RAMExpansion:
        return "Memory expansion";
    case melonDS::GBAAddon_SolarSensorBoktai1:
        return "Solar Sensor (Boktai 1)";
    case melonDS::GBAAddon_SolarSensorBoktai2:
        return "Solar Sensor (Boktai 2)";
    case melonDS::GBAAddon_SolarSensorBoktai3:
        return "Solar Sensor (Boktai 3)";
    case melonDS::GBAAddon_MotionPakHomebrew:
        return "Motion Pak (Homebrew)";
    case melonDS::GBAAddon_MotionPakRetail:
        return "Motion Pack (Retail)";
    case melonDS::GBAAddon_GuitarGrip:
        return "Guitar Grip";
    }

    return "???";
}

void ImGuiEmuInstance::undoStateLoad()
{
    if (!savestateLoaded || !backupState) return;

    backupState->Rewind(false);
    
    if (nds)
    {
        nds->DoSavestate(backupState.get());
    }
    else if (dsi)
    {
        static_cast<melonDS::NDS*>(dsi.get())->DoSavestate(backupState.get());
    }

    if (saveManager && (!previousSaveFile.empty()))
    {
        saveManager->SetPath(previousSaveFile, true);
    }
    
    savestateLoaded = false;
}

std::string ImGuiEmuInstance::getSavestateName(int slot)
{
    if (slot <= 0) return "";
    
    std::string baseDir = getConfigDirectory() + "/savestates";
    std::string filename = baseDir + "/slot" + std::to_string(slot) + ".mln";
    return filename;
}

bool ImGuiEmuInstance::savestateExists(int slot)
{
    if (slot <= 0) return false;
    
    std::string filename = getSavestateName(slot);
    if (filename.empty()) return false;
    
    auto file = melonDS::Platform::OpenFile(filename, melonDS::Platform::FileMode::Read);
    if (!file) return false;
    
    melonDS::Platform::CloseFile(file);
    return true;
}

std::string ImGuiEmuInstance::getConfigDirectory() {
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string pathStr(exePath);
    size_t lastSlash = pathStr.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        return pathStr.substr(0, lastSlash);
    }
    return ".";
}

std::string ImGuiEmuInstance::instanceFileSuffix()
{
    if (instanceID == 0) return "";

    char suffix[16] = {0};
    snprintf(suffix, 15, ".%d", instanceID+1);
    return suffix;
}

melonDS::NDS* ImGuiEmuInstance::getNDS() {
    if (nds) return nds.get();
    if (dsi) return static_cast<melonDS::NDS*>(dsi.get());
    return nullptr;
}

melonDS::DSi* ImGuiEmuInstance::getDSi() {
    return dsi.get();
}

melonDS::ARCodeFile* ImGuiEmuInstance::getCheatFile() {
    return cheatFile.get();
}

void ImGuiEmuInstance::enableCheats(bool enable) {
    cheatsOn = enable;
    localConfig.SetBool("EnableCheats", enable);
    // If needed, propagate to cheat system
}

void ImGuiEmuInstance::osdAddMessage(unsigned int color, const char* msg) {
    osdMessages.emplace_back(msg, color);
    
    if (osdMessages.size() > 10) {
        osdMessages.erase(osdMessages.begin());
    }
    
    printf("[OSD] %08X: %s\n", color, msg);
}

void ImGuiEmuInstance::inputInit() {
    keyInputMask = 0xFFF;
    joyInputMask = 0xFFF;
    inputMask = 0xFFF;

    keyHotkeyMask = 0;
    joyHotkeyMask = 0;
    hotkeyMask = 0;
    lastHotkeyMask = 0;

    isTouching = false;
    touchX = 0;
    touchY = 0;

    joystick = nullptr;
    controller = nullptr;
    hasRumble = false;
    hasAccelerometer = false;
    hasGyroscope = false;
    isRumbling = false;
    
    inputLoadConfig();
}

void ImGuiEmuInstance::inputDeInit()
{
    closeJoystick();
}

void ImGuiEmuInstance::inputLoadConfig()
{
    Config::Table keycfg = localConfig.GetTable("Keyboard");
    Config::Table joycfg = localConfig.GetTable("Joystick");

    for (int i = 0; i < 12; i++)
    {
        keyMapping[i] = keycfg.GetInt(buttonNames[i]);
        joyMapping[i] = joycfg.GetInt(buttonNames[i]);
    }

    for (int i = 0; i < HK_MAX; i++)
    {
        hkKeyMapping[i] = keycfg.GetInt(hotkeyNames[i]);
        hkJoyMapping[i] = joycfg.GetInt(hotkeyNames[i]);
    }

    setJoystick(localConfig.GetInt("JoystickID"));
}

void ImGuiEmuInstance::inputRumbleStart(melonDS::u32 len_ms)
{
    if (controller && hasRumble && !isRumbling)
    {
        SDL_GameControllerRumble(controller, 0xFFFF, 0xFFFF, len_ms);
        isRumbling = true;
    }
}

void ImGuiEmuInstance::inputRumbleStop()
{
    if (controller && hasRumble && isRumbling)
    {
        SDL_GameControllerRumble(controller, 0, 0, 0);
        isRumbling = false;
    }
}

float ImGuiEmuInstance::inputMotionQuery(melonDS::Platform::MotionQueryType type)
{
    float values[3];
    if (type <= melonDS::Platform::MotionAccelerationZ)
    {
        if (controller && hasAccelerometer)
            if (SDL_GameControllerGetSensorData(controller, SDL_SENSOR_ACCEL, values, 3) == 0)
            {
                switch (type)
                {
                case melonDS::Platform::MotionAccelerationX:
                    return values[0];
                case melonDS::Platform::MotionAccelerationY:
                    return -values[2];
                case melonDS::Platform::MotionAccelerationZ:
                    return values[1];
                }
            }
    }
    else if (type <= melonDS::Platform::MotionRotationZ)
    {
        if (controller && hasGyroscope)
            if (SDL_GameControllerGetSensorData(controller, SDL_SENSOR_GYRO, values, 3) == 0)
            {   
                switch (type)
                {
                case melonDS::Platform::MotionRotationX:
                    return values[0];
                case melonDS::Platform::MotionRotationY:
                    return -values[2];
                case melonDS::Platform::MotionRotationZ:
                    return values[1];
                }
            }
    }
    if (type == melonDS::Platform::MotionAccelerationZ)
        return SDL_STANDARD_GRAVITY;
    return 0.0f;
}

void ImGuiEmuInstance::setJoystick(int id)
{
    joystickID = id;
    openJoystick();
}

void ImGuiEmuInstance::openJoystick()
{
    if (controller) SDL_GameControllerClose(controller);
    if (joystick) SDL_JoystickClose(joystick);

    int num = SDL_NumJoysticks();
    if (num < 1)
    {
        controller = nullptr;
        joystick = nullptr;
        hasRumble = false;
        hasAccelerometer = false;
        hasGyroscope = false;
        return;
    }

    if (joystickID >= num)
        joystickID = 0;

    joystick = SDL_JoystickOpen(joystickID);

    if (SDL_IsGameController(joystickID))
    {
        controller = SDL_GameControllerOpen(joystickID);
    }

    if (controller)
    {
        if (SDL_GameControllerHasRumble(controller))
        {
            hasRumble = true;
        }
        if (SDL_GameControllerHasSensor(controller, SDL_SENSOR_ACCEL))
        {
            hasAccelerometer = SDL_GameControllerSetSensorEnabled(controller, SDL_SENSOR_ACCEL, SDL_TRUE) == 0;
        }
        if (SDL_GameControllerHasSensor(controller, SDL_SENSOR_GYRO))
        {
            hasGyroscope = SDL_GameControllerSetSensorEnabled(controller, SDL_SENSOR_GYRO, SDL_TRUE) == 0;
        }
    }
}

void ImGuiEmuInstance::closeJoystick()
{
    if (controller)
    {
        SDL_GameControllerClose(controller);
        controller = nullptr;
        hasRumble = false;
        hasAccelerometer = false;
        hasGyroscope = false;
    }
    if (joystick)
    {
        SDL_JoystickClose(joystick);
        joystick = nullptr;
    }
}

bool ImGuiEmuInstance::joystickButtonDown(int val)
{
    if (val == -1) return false;

    bool hasbtn = ((val & 0xFFFF) != 0xFFFF);

    if (hasbtn)
    {
        if (val & 0x100)
        {
            int hatnum = (val >> 4) & 0xF;
            int hatdir = val & 0xF;
            Uint8 hatval = SDL_JoystickGetHat(joystick, hatnum);

            bool pressed = false;
            if      (hatdir == 0x1) pressed = (hatval & SDL_HAT_UP);
            else if (hatdir == 0x4) pressed = (hatval & SDL_HAT_DOWN);
            else if (hatdir == 0x2) pressed = (hatval & SDL_HAT_RIGHT);
            else if (hatdir == 0x8) pressed = (hatval & SDL_HAT_LEFT);

            if (pressed) return true;
        }
        else
        {
            int btnnum = val & 0xFFFF;
            Uint8 btnval = SDL_JoystickGetButton(joystick, btnnum);

            if (btnval) return true;
        }
    }

    if (val & 0x10000)
    {
        int axisnum = (val >> 24) & 0xF;
        int axisdir = (val >> 20) & 0xF;
        Sint16 axisval = SDL_JoystickGetAxis(joystick, axisnum);

        switch (axisdir)
        {
            case 0: // positive
                if (axisval > 16384) return true;
                break;

            case 1: // negative
                if (axisval < -16384) return true;
                break;

            case 2: // trigger
                if (axisval > 0) return true;
                break;
        }
    }

    return false;
}

void ImGuiEmuInstance::touchScreen(int x, int y)
{
    touchX = x;
    touchY = y;
    isTouching = true;
}

void ImGuiEmuInstance::releaseScreen()
{
    isTouching = false;
}

void ImGuiEmuInstance::inputProcess() {
    SDL_JoystickUpdate();

    if (joystick) {
        if (!SDL_JoystickGetAttached(joystick)) {
            if (controller) {
                SDL_GameControllerClose(controller);
                controller = nullptr;
            }
            SDL_JoystickClose(joystick);
            joystick = nullptr;
            hasRumble = false;
            hasAccelerometer = false;
            hasGyroscope = false;
        }
    }
    
    if (!joystick && (SDL_NumJoysticks() > 0)) {
        openJoystick();
    }

    joyInputMask = 0xFFF;
    if (joystick) {
        for (int i = 0; i < 12; i++) {
            if (joystickButtonDown(joyMapping[i])) {
                joyInputMask &= ~(1 << i);
            }
        }
    }

    inputMask = keyInputMask & joyInputMask;

    joyHotkeyMask = 0;
    if (joystick) {
        for (int i = 0; i < HK_MAX; i++) {
            if (joystickButtonDown(hkJoyMapping[i])) {
                joyHotkeyMask |= (1 << i);
            }
        }
    }

    hotkeyMask = keyHotkeyMask | joyHotkeyMask;
    hotkeyPress = hotkeyMask & ~lastHotkeyMask;
    hotkeyRelease = lastHotkeyMask & ~hotkeyMask;
    lastHotkeyMask = hotkeyMask;
}
std::unique_ptr<melonDS::ARM9BIOSImage> ImGuiEmuInstance::loadARM9BIOS() noexcept {
    if (!globalConfig.GetBool("Emu.ExternalBIOSEnable")) {
        return std::make_unique<melonDS::ARM9BIOSImage>(melonDS::bios_arm9_bin);
    }
    std::string path = globalConfig.GetString("DS.BIOS9Path");
    std::cout << "[loadARM9BIOS] Path: '" << path << "'" << std::endl;
    auto file = melonDS::Platform::OpenLocalFile(path, melonDS::Platform::FileMode::Read);
    if (file) {
        long len = melonDS::Platform::FileLength(file);
        std::cout << "[loadARM9BIOS] Opened, size: " << len << std::endl;
        std::unique_ptr<melonDS::ARM9BIOSImage> bios = std::make_unique<melonDS::ARM9BIOSImage>();
        melonDS::Platform::FileRewind(file);
        melonDS::Platform::FileRead(bios->data(), bios->size(), 1, file);
        melonDS::Platform::CloseFile(file);
        return bios;
    } else {
        std::cout << "[loadARM9BIOS] Failed to open" << std::endl;
    }
    return nullptr;
}
std::unique_ptr<melonDS::ARM7BIOSImage> ImGuiEmuInstance::loadARM7BIOS() noexcept {
    if (!globalConfig.GetBool("Emu.ExternalBIOSEnable")) {
        return std::make_unique<melonDS::ARM7BIOSImage>(melonDS::bios_arm7_bin);
    }
    std::string path = globalConfig.GetString("DS.BIOS7Path");
    std::cout << "[loadARM7BIOS] Path: '" << path << "'" << std::endl;
    auto file = melonDS::Platform::OpenLocalFile(path, melonDS::Platform::FileMode::Read);
    if (file) {
        long len = melonDS::Platform::FileLength(file);
        std::cout << "[loadARM7BIOS] Opened, size: " << len << std::endl;
        std::unique_ptr<melonDS::ARM7BIOSImage> bios = std::make_unique<melonDS::ARM7BIOSImage>();
        melonDS::Platform::FileRead(bios->data(), bios->size(), 1, file);
        melonDS::Platform::CloseFile(file);
        return bios;
    } else {
        std::cout << "[loadARM7BIOS] Failed to open" << std::endl;
    }
    return nullptr;
}
std::unique_ptr<melonDS::DSiBIOSImage> ImGuiEmuInstance::loadDSiARM9BIOS() noexcept {
    std::string path = globalConfig.GetString("DSi.BIOS9Path");
    
    auto file = melonDS::Platform::OpenLocalFile(path, melonDS::Platform::FileMode::Read);
    if (file) {
        std::unique_ptr<melonDS::DSiBIOSImage> bios = std::make_unique<melonDS::DSiBIOSImage>();
        melonDS::Platform::FileRead(bios->data(), bios->size(), 1, file);
        melonDS::Platform::CloseFile(file);
        
        if (!globalConfig.GetBool("DSi.FullBIOSBoot")) {
            *(melonDS::u32*)bios->data() = 0xEAFFFFFE; // overwrites the reset vector
        }
        
        std::cout << "[loadDSiARM9BIOS] ARM9i BIOS loaded from " << path << std::endl;
        return bios;
    }
    
    std::cout << "[loadDSiARM9BIOS] ARM9i BIOS not found at " << path << std::endl;
    return nullptr;
}

std::unique_ptr<melonDS::DSiBIOSImage> ImGuiEmuInstance::loadDSiARM7BIOS() noexcept {
    std::string path = globalConfig.GetString("DSi.BIOS7Path");
    
    auto file = melonDS::Platform::OpenLocalFile(path, melonDS::Platform::FileMode::Read);
    if (file) {
        std::unique_ptr<melonDS::DSiBIOSImage> bios = std::make_unique<melonDS::DSiBIOSImage>();
        melonDS::Platform::FileRead(bios->data(), bios->size(), 1, file);
        melonDS::Platform::CloseFile(file);
        
        if (!globalConfig.GetBool("DSi.FullBIOSBoot")) {
            *(melonDS::u32*)bios->data() = 0xEAFFFFFE; // overwrites the reset vector
        }
        
        std::cout << "[loadDSiARM7BIOS] ARM7i BIOS loaded from " << path << std::endl;
        return bios;
    }
    
    std::cout << "[loadDSiARM7BIOS] ARM7i BIOS not found at " << path << std::endl;
    return nullptr;
}
std::optional<melonDS::Firmware> ImGuiEmuInstance::loadFirmware(int type) noexcept {
    if (!globalConfig.GetBool("Emu.ExternalBIOSEnable")) {
        if (type == 0) {
            return melonDS::Firmware(0);
        } else {
            return std::nullopt;
        }
    }
    std::string firmwarePath;
    if (type == 0) {
        firmwarePath = globalConfig.GetString("DS.FirmwarePath");
    } else {
        firmwarePath = globalConfig.GetString("DSi.FirmwarePath");
    }
    std::cout << "[loadFirmware] type: " << type << ", path: '" << firmwarePath << "'" << std::endl;
    if (firmwarePath.empty()) {
        std::cout << "[loadFirmware] Firmware path is empty" << std::endl;
        return std::nullopt;
    }
    std::string fwpath_inst = firmwarePath + instanceFileSuffix();
    std::cout << "[loadFirmware] Trying instance path: '" << fwpath_inst << "'" << std::endl;
    auto file = melonDS::Platform::OpenLocalFile(fwpath_inst, melonDS::Platform::FileMode::Read);
    if (!file) {
        std::cout << "[loadFirmware] Instance file not found, trying base path" << std::endl;
        file = melonDS::Platform::OpenLocalFile(firmwarePath, melonDS::Platform::FileMode::Read);
        std::cout << "[loadFirmware] OpenFile returned: " << (file ? "success" : "failure") << std::endl;
        if (!file) {
            std::cout << "[loadFirmware] OpenFile failed for path: " << firmwarePath << std::endl;
            std::cout << "[loadFirmware] errno: " << errno << " (" << std::strerror(errno) << ")" << std::endl;
            return std::nullopt;
        }
    } else {
        std::cout << "[loadFirmware] Instance file found and opened" << std::endl;
    }
    long len = melonDS::Platform::FileLength(file);
    std::cout << "[loadFirmware] File size: " << len << std::endl;
    melonDS::Firmware firmware(file);
    melonDS::Platform::CloseFile(file);
    if (!firmware.Buffer()) {
        std::cout << "[loadFirmware] Failed to create firmware object" << std::endl;
        return std::nullopt;
    }
    customizeFirmware(firmware, localConfig.GetBool("Firmware.OverrideSettings"));
    std::cout << "[loadFirmware] Firmware object created with save data" << std::endl;
    return firmware;
}
std::optional<melonDS::DSi_NAND::NANDImage> ImGuiEmuInstance::loadNAND(const std::array<melonDS::u8, melonDS::DSiBIOSSize>& arm7ibios) noexcept 
{ 
    std::string nandPath = globalConfig.GetString("DSi.NANDPath");
    std::cout << "[loadNAND] NAND path: '" << nandPath << "'" << std::endl;
    if (nandPath.empty()) return std::nullopt;
    
    auto file = melonDS::Platform::OpenLocalFile(nandPath, melonDS::Platform::FileMode::ReadWriteExisting);
    if (!file) {
        std::cout << "[loadNAND] Failed to open NAND file" << std::endl;
        return std::nullopt;
    }
    long filelen = melonDS::Platform::FileLength(file);
    std::cout << "[loadNAND] NAND file opened, size: " << filelen << " bytes" << std::endl;
    melonDS::DSi_NAND::NANDImage nandImage(file, &arm7ibios[0x8308]);
    if (!nandImage) {
        std::cout << "[loadNAND] Failed to parse DSi NAND" << std::endl;
        melonDS::Platform::CloseFile(file);
        return std::nullopt;
    }
    std::cout << "[loadNAND] DSi NAND parsed successfully" << std::endl;
    auto mount = melonDS::DSi_NAND::NANDMount(nandImage);
    if (!mount) {
        std::cout << "[loadNAND] Failed to mount DSi NAND" << std::endl;
        return std::nullopt;
    }
    std::cout << "[loadNAND] DSi NAND mounted successfully" << std::endl;
    melonDS::DSi_NAND::DSiFirmwareSystemSettings settings{};
    bool userDataOk = mount.ReadUserData(settings);
    std::cout << "[loadNAND] ReadUserData returned: " << (userDataOk ? "true" : "false") << std::endl;
    if (!userDataOk) {
        long offset = 0;
        std::cout << "[loadNAND] Failed to read DSi NAND user data at offset (unknown, see code)" << std::endl;
        return std::nullopt;
    }
    std::cout << "[loadNAND] DSi NAND loaded and verified successfully" << std::endl;
    return nandImage;
}
std::optional<melonDS::FATStorage> ImGuiEmuInstance::loadSDCard(const std::string& key) noexcept 
{ 
    auto args = getSDCardArgs(key);
    if (!args.has_value())
        return std::nullopt;

    return melonDS::FATStorage(args.value());
}

std::optional<melonDS::FATStorageArgs> ImGuiEmuInstance::getSDCardArgs(const std::string& key) noexcept
{
    Config::Table sdopt = globalConfig.GetTable(key);

    if (!sdopt.GetBool("Enable"))
        return std::nullopt;

    static constexpr melonDS::u64 imgsizes[] = {0, 256ULL * 1024 * 1024, 512ULL * 1024 * 1024, 1024ULL * 1024 * 1024, 2048ULL * 1024 * 1024, 4096ULL * 1024 * 1024};

    return melonDS::FATStorageArgs {
            sdopt.GetString("ImagePath"),
            imgsizes[sdopt.GetInt("ImageSize")],
            sdopt.GetBool("ReadOnly"),
            sdopt.GetBool("FolderSync") ? std::make_optional(sdopt.GetString("FolderPath")) : std::nullopt
    };
}
melonDS::u32 ImGuiEmuInstance::convertSDLKeyToMask(SDL_Keycode key) 
{ 
    switch (key) {
        case SDLK_a: return 1 << 0;  // A
        case SDLK_s: return 1 << 1;  // B
        case SDLK_BACKSPACE: return 1 << 2;  // Select
        case SDLK_RETURN: return 1 << 3;  // Start
        case SDLK_RIGHT: return 1 << 4;  // Right
        case SDLK_LEFT: return 1 << 5;  // Left
        case SDLK_UP: return 1 << 6;  // Up
        case SDLK_DOWN: return 1 << 7;  // Down
        case SDLK_r: return 1 << 8;  // R
        case SDLK_l: return 1 << 9;  // L
        case SDLK_x: return 1 << 10; // X
        case SDLK_y: return 1 << 11; // Y
        default: return 0;
    }
}

std::string ImGuiEmuInstance::verifySetup()
{
    std::string res;

    bool extbios = globalConfig.GetBool("Emu.ExternalBIOSEnable");
    int console = globalConfig.GetInt("Emu.ConsoleType");

    if (extbios)
    {
        res = verifyDSBIOS();
        if (!res.empty()) return res;
    }

    if (console == 1)
    {
        res = verifyDSiBIOS();
        if (!res.empty()) return res;

        if (extbios)
        {
            res = verifyDSiFirmware();
            if (!res.empty()) return res;
        }

        res = verifyDSiNAND();
        if (!res.empty()) return res;
    }
    else
    {
        if (extbios)
        {
            res = verifyDSFirmware();
            if (!res.empty()) return res;
        }
    }

    return "";
}

Config::Table& ImGuiEmuInstance::getGlobalConfig() {
    return globalConfig;
}
Config::Table& ImGuiEmuInstance::getLocalConfig() {
    return localConfig;
}

void ImGuiEmuInstance::reset() {
    if (nds) {
        nds->Reset();
    } else if (dsi) {
        dsi->Reset();
    }
}
void ImGuiEmuInstance::frameStep() {
    static int emuFrameCount = 0;
    
    inputProcess();
    
    if (nds) {
        nds->SetKeyMask(inputMask);
        
        if (isTouching) {
            nds->TouchScreen(touchX, touchY);
        } else {
            nds->ReleaseScreen();
        }
        nds->RunFrame();
        emuFrameCount++;
    } else if (dsi) {
        dsi->SetKeyMask(inputMask);
        
        if (isTouching) {
            dsi->TouchScreen(touchX, touchY);
        } else {
            dsi->ReleaseScreen();
        }
        if (dsi->IsRunning()) {
            dsi->RunFrame();
            emuFrameCount++;
        }
    }
    
    audioSync();
    
    if (melonDS::Platform::EmuShouldStop()) {
        stop();
        melonDS::Platform::ClearEmuShouldStop();
    }
}
void ImGuiEmuInstance::start() {
    printf("[DEBUG] ImGuiEmuInstance::start called\n");
    if (nds) {
        printf("[DEBUG] ImGuiEmuInstance::start: Starting NDS\n");
        nds->Start();
    } else if (dsi) {
        printf("[DEBUG] ImGuiEmuInstance::start: Starting DSi\n");
        dsi->Start();
    }
    running = true;
    paused = false;
    
    // Enable audio when emulation starts
    audioEnable();
    printf("[DEBUG] ImGuiEmuInstance::start completed\n");
}
void ImGuiEmuInstance::stop() {
    if (nds) {
        nds->Stop(melonDS::Platform::StopReason::External);
    } else if (dsi) {
        dsi->Stop(melonDS::Platform::StopReason::External);
    }
    running = false;
    paused = false;
    
    // Disable audio when emulation stops
    audioDisable();
}
void ImGuiEmuInstance::pause() {
    if (nds) {
        nds->Stop(melonDS::Platform::StopReason::External);
    } else if (dsi) {
        dsi->Stop(melonDS::Platform::StopReason::External);
    }
    paused = true;
}
void ImGuiEmuInstance::resume() {
    if (nds) {
        nds->Start();
    } else if (dsi) {
        dsi->Start();
    }
    paused = false;
}

bool ImGuiEmuInstance::importSavefile(const std::string& filename) {
    if (!nds && !dsi) {
        return false;
    }

    auto file = melonDS::Platform::OpenFile(filename, melonDS::Platform::FileMode::Read);
    if (!file) {
        return false;
    }

    melonDS::u32 len = melonDS::Platform::FileLength(file);
    std::unique_ptr<melonDS::u8[]> data = std::make_unique<melonDS::u8[]>(len);
    melonDS::Platform::FileRewind(file);
    melonDS::Platform::FileRead(data.get(), len, 1, file);
    melonDS::Platform::CloseFile(file);

    if (nds) {
        nds->SetNDSSave(data.get(), len);
    } else if (dsi) {
        dsi->SetNDSSave(data.get(), len);
    }

    return true;
}

bool ImGuiEmuInstance::bootFirmware(std::string& errorstr) {
    std::cout << "[bootFirmware] Called. ConsoleType: " << globalConfig.GetInt("Emu.ConsoleType") << std::endl;
    consoleType = globalConfig.GetInt("Emu.ConsoleType");

    if (nds) nds->EjectCart();
    if (dsi) dsi->EjectCart();
    cartInserted = false;
    gbaCartInserted = false;
    gbaCartType = -1;
    baseGBAROMDir = "";
    baseGBAROMName = "";
    baseGBAAssetName = "";

    if (consoleType == 1) {
        std::cout << "[bootFirmware] DSi mode selected" << std::endl;
        std::string biosErr = verifyDSiBIOS();
        std::cout << "[bootFirmware] verifyDSiBIOS: '" << biosErr << "'" << std::endl;
        if (!biosErr.empty()) {
            errorstr = biosErr;
            std::cout << "[bootFirmware] Error: " << errorstr << std::endl;
            return false;
        }
        std::string fwErr = verifyDSiFirmware();
        std::cout << "[bootFirmware] verifyDSiFirmware: '" << fwErr << "'" << std::endl;
        if (!fwErr.empty()) {
            errorstr = fwErr;
            std::cout << "[bootFirmware] Error: " << errorstr << std::endl;
            return false;
        }
        std::string nandErr = verifyDSiNAND();
        std::cout << "[bootFirmware] verifyDSiNAND: '" << nandErr << "'" << std::endl;
        if (!nandErr.empty()) {
            errorstr = nandErr;
            std::cout << "[bootFirmware] Error: " << errorstr << std::endl;
            return false;
        }
        std::cout << "[bootFirmware] Loading DSi ARM9 BIOS..." << std::endl;
        auto arm9bios = loadARM9BIOS();
        if (!arm9bios) {
            errorstr = "Failed to load DSi ARM9 BIOS.";
            std::cout << "[bootFirmware] Error: " << errorstr << std::endl;
            return false;
        }
        std::cout << "[bootFirmware] Loading DSi ARM7 BIOS..." << std::endl;
        auto arm7bios = loadARM7BIOS();
        if (!arm7bios) {
            errorstr = "Failed to load DSi ARM7 BIOS.";
            std::cout << "[bootFirmware] Error: " << errorstr << std::endl;
            return false;
        }
        std::cout << "[bootFirmware] Loading DSi ARM9i BIOS..." << std::endl;
        auto arm9ibios = loadDSiARM9BIOS();
        if (!arm9ibios) {
            errorstr = "Failed to load DSi ARM9i BIOS.";
            std::cout << "[bootFirmware] Error: " << errorstr << std::endl;
            return false;
        }
        std::cout << "[bootFirmware] Loading DSi ARM7i BIOS..." << std::endl;
        auto arm7ibios = loadDSiARM7BIOS();
        if (!arm7ibios) {
            errorstr = "Failed to load DSi ARM7i BIOS.";
            std::cout << "[bootFirmware] Error: " << errorstr << std::endl;
            return false;
        }
        std::cout << "[bootFirmware] Loading DSi NAND..." << std::endl;
        auto nand = loadNAND(*arm7ibios);
        if (!nand) {
            errorstr = "Failed to load DSi NAND";
            std::cout << "[bootFirmware] Error: " << errorstr << std::endl;
            return false;
        }
        std::cout << "[bootFirmware] Loading DSi SD card (optional)..." << std::endl;
        auto sdcard = loadSDCard("DSi.SD");
        bool fullBIOSBoot = globalConfig.GetBool("DSi.FullBIOSBoot");
        std::cout << "[bootFirmware] Creating DSi instance..." << std::endl;
#ifdef JIT_ENABLED
        auto jitopt = globalConfig.GetTable("JIT");
        melonDS::JITArgs _jitargs {
                static_cast<unsigned>(jitopt.GetInt("MaxBlockSize")),
                jitopt.GetBool("LiteralOptimisations"),
                jitopt.GetBool("BranchOptimisations"),
                jitopt.GetBool("FastMemory"),
        };
        auto jitargs = jitopt.GetBool("Enable") ? std::make_optional(_jitargs) : std::nullopt;
#else
        std::optional<melonDS::JITArgs> jitargs = std::nullopt;
#endif

        melonDS::DSiArgs args{
            melonDS::NDSArgs{
                std::move(arm9bios),
                std::move(arm7bios),
                melonDS::Firmware(0), // Will be loaded later if needed
                jitargs,
                static_cast<melonDS::AudioBitDepth>(globalConfig.GetInt("Audio.BitDepth")),
                static_cast<melonDS::AudioInterpolation>(globalConfig.GetInt("Audio.Interpolation")),
                std::nullopt // GDB args
            },
            std::move(arm9ibios),
            std::move(arm7ibios),
            std::move(*nand),
            std::move(sdcard),
            fullBIOSBoot
        };
        dsi = std::make_unique<melonDS::DSi>(std::move(args), this);
        std::cout << "[bootFirmware] DSi instance created." << std::endl;
        std::string firmwarePath = globalConfig.GetString("DSi.FirmwarePath");
        if (!firmwarePath.empty()) {
            std::cout << "[bootFirmware] Loading DSi firmware..." << std::endl;
            auto firmware = loadFirmware(1);
            if (!firmware) {
                errorstr = "Failed to load DSi firmware.";
                std::cout << "[bootFirmware] Error: " << errorstr << std::endl;
                return false;
            }
            dsi->SetFirmware(std::move(*firmware));
        }
        dsi->EjectCart();
        cartInserted = false;
        std::cout << "[bootFirmware] Resetting DSi..." << std::endl;
        reset();
        setBatteryLevels();
        setDateTime();
        std::cout << "[bootFirmware] DSi firmware boot complete." << std::endl;
        printf("[DEBUG] bootFirmware: DSi firmware boot completed successfully\n");
        return true;
    }

    auto arm9bios = loadARM9BIOS();
    if (!arm9bios) {
        errorstr = "Failed to load DS ARM9 BIOS.";
        return false;
    }
    auto arm7bios = loadARM7BIOS();
    if (!arm7bios) {
        errorstr = "Failed to load DS ARM7 BIOS.";
        return false;
    }
    std::string firmwarePath = globalConfig.GetString("DS.FirmwarePath");
    std::optional<melonDS::Firmware> firmware;
    if (!firmwarePath.empty()) {
        firmware = loadFirmware(0);
        if (!firmware) {
            errorstr = "Failed to load DS firmware.";
            return false;
        }
    }
#ifdef JIT_ENABLED
    auto jitopt = globalConfig.GetTable("JIT");
    melonDS::JITArgs _jitargs {
            static_cast<unsigned>(jitopt.GetInt("MaxBlockSize")),
            jitopt.GetBool("LiteralOptimisations"),
            jitopt.GetBool("BranchOptimisations"),
            jitopt.GetBool("FastMemory"),
    };
    auto jitargs = jitopt.GetBool("Enable") ? std::make_optional(_jitargs) : std::nullopt;
#else
    std::optional<melonDS::JITArgs> jitargs = std::nullopt;
#endif

    melonDS::NDSArgs args{
        std::move(arm9bios),
        std::move(arm7bios),
        firmware ? std::move(*firmware) : melonDS::Firmware(0),
        jitargs,
        static_cast<melonDS::AudioBitDepth>(globalConfig.GetInt("Audio.BitDepth")),
        static_cast<melonDS::AudioInterpolation>(globalConfig.GetInt("Audio.Interpolation")),
        std::nullopt // GDB args
    };
    nds = std::make_unique<melonDS::NDS>(std::move(args));
    nds->EjectCart();
    cartInserted = false;
    gbaCartInserted = false;
    gbaCartType = -1;
    nds->Reset();
    setBatteryLevels();
    setDateTime();
    return true;
}

bool ImGuiEmuInstance::loadROMData(const std::vector<std::string>& filepath, std::unique_ptr<melonDS::u8[]>& filedata, melonDS::u32& filelen, std::string& basepath, std::string& romname) {
    if (filepath.empty()) return false;
    
    std::string path = filepath[0];
    
    size_t lastSlash = path.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        basepath = path.substr(0, lastSlash + 1);
        romname = path.substr(lastSlash + 1);
    } else {
        basepath = "";
        romname = path;
    }
    
    auto file = melonDS::Platform::OpenFile(path, melonDS::Platform::FileMode::Read);
    if (!file) return false;
    
    filelen = melonDS::Platform::FileLength(file);
    filedata = std::make_unique<melonDS::u8[]>(filelen);
    
    if (melonDS::Platform::FileRead(filedata.get(), 1, filelen, file) != filelen) {
        melonDS::Platform::CloseFile(file);
        return false;
    }
    
    melonDS::Platform::CloseFile(file);
    return true;
}

std::string ImGuiEmuInstance::verifyDSBIOS() {
    if (!globalConfig.GetBool("Emu.ExternalBIOSEnable")) return "";
    std::string bios9 = globalConfig.GetString("DS.BIOS9Path");
    std::string bios7 = globalConfig.GetString("DS.BIOS7Path");
    std::cout << "[verifyDSBIOS] BIOS9Path: '" << bios9 << "'" << std::endl;
    std::cout << "[verifyDSBIOS] BIOS7Path: '" << bios7 << "'" << std::endl;
    auto f = melonDS::Platform::OpenLocalFile(bios9, melonDS::Platform::FileMode::Read);
    if (!f) {
        std::cout << "[verifyDSBIOS] Failed to open BIOS9" << std::endl;
        return "DS ARM9 BIOS was not found or could not be accessed. Check your emu settings.";
    }
    long len = melonDS::Platform::FileLength(f);
    std::cout << "[verifyDSBIOS] BIOS9 size: " << len << std::endl;
    if (len != 0x1000) {
        melonDS::Platform::CloseFile(f);
        return "DS ARM9 BIOS is not a valid BIOS dump.";
    }
    melonDS::Platform::CloseFile(f);
    f = melonDS::Platform::OpenLocalFile(bios7, melonDS::Platform::FileMode::Read);
    if (!f) {
        std::cout << "[verifyDSBIOS] Failed to open BIOS7" << std::endl;
        return "DS ARM7 BIOS was not found or could not be accessed. Check your emu settings.";
    }
    len = melonDS::Platform::FileLength(f);
    std::cout << "[verifyDSBIOS] BIOS7 size: " << len << std::endl;
    if (len != 0x4000) {
        melonDS::Platform::CloseFile(f);
        return "DS ARM7 BIOS is not a valid BIOS dump.";
    }
    melonDS::Platform::CloseFile(f);
    return "";
}
std::string ImGuiEmuInstance::verifyDSiBIOS() {
    auto f = melonDS::Platform::OpenLocalFile(globalConfig.GetString("DSi.BIOS9Path"), melonDS::Platform::FileMode::Read);
    if (!f) return "DSi ARM9 BIOS was not found or could not be accessed. Check your emu settings.";

    long len = melonDS::Platform::FileLength(f);
    if (len != 0x10000)
    {
        melonDS::Platform::CloseFile(f);
        return "DSi ARM9 BIOS is not a valid BIOS dump.";
    }

    melonDS::Platform::CloseFile(f);

    f = melonDS::Platform::OpenLocalFile(globalConfig.GetString("DSi.BIOS7Path"), melonDS::Platform::FileMode::Read);
    if (!f) return "DSi ARM7 BIOS was not found or could not be accessed. Check your emu settings.";

    len = melonDS::Platform::FileLength(f);
    if (len != 0x10000)
    {
        melonDS::Platform::CloseFile(f);
        return "DSi ARM7 BIOS is not a valid BIOS dump.";
    }

    melonDS::Platform::CloseFile(f);

    return "";
}
std::string ImGuiEmuInstance::verifyDSFirmware() {
    if (!globalConfig.GetBool("Emu.ExternalBIOSEnable")) return "";
    std::string fwpath = globalConfig.GetString("DS.FirmwarePath");
    std::cout << "[verifyDSFirmware] FirmwarePath: '" << fwpath << "'" << std::endl;
    auto f = melonDS::Platform::OpenLocalFile(fwpath, melonDS::Platform::FileMode::Read);
    if (!f) {
        std::cout << "[verifyDSFirmware] Failed to open firmware" << std::endl;
        return "DS firmware was not found or could not be accessed. Check your emu settings.";
    }
    long len = melonDS::Platform::FileLength(f);
    std::cout << "[verifyDSFirmware] Firmware size: " << len << std::endl;
    if (len == 0x20000)
    {
        melonDS::Platform::CloseFile(f);
        return "";
    }
    else if (len != 0x40000 && len != 0x80000)
    {
        melonDS::Platform::CloseFile(f);
        return "DS firmware is not a valid firmware dump.";
    }

    melonDS::Platform::CloseFile(f);

    return "";
}
std::string ImGuiEmuInstance::verifyDSiFirmware() {
    std::string fwpath = globalConfig.GetString("DSi.FirmwarePath");
    std::cout << "[verifyDSiFirmware] Path: '" << fwpath << "'" << std::endl;
    auto f = melonDS::Platform::OpenLocalFile(fwpath, melonDS::Platform::FileMode::Read);
    std::cout << "[verifyDSiFirmware] OpenLocalFile returned: " << (f ? "success" : "failure") << std::endl;
    if (!f) {
        std::cout << "[verifyDSiFirmware] OpenLocalFile failed for path: " << fwpath << std::endl;
        std::cout << "[verifyDSiFirmware] errno: " << errno << " (" << std::strerror(errno) << ")" << std::endl;
        std::cout << "[verifyDSiFirmware] Path bytes: ";
        for (unsigned char c : fwpath) std::cout << std::hex << (int)c << " ";
        std::cout << std::dec << std::endl;
        return "DSi firmware was not found or could not be accessed. Check your emu settings.";
    }
    if (!melonDS::Platform::CheckFileWritable(fwpath))
        return "DSi firmware is unable to be written to.\nPlease check file/folder write permissions.";
    long len = melonDS::Platform::FileLength(f);
    if (len != 0x20000)
    {
        melonDS::Platform::CloseFile(f);
        return "DSi firmware is not a valid firmware dump.";
    }
    melonDS::Platform::CloseFile(f);
    return "";
}
std::string ImGuiEmuInstance::verifyDSiNAND() {
    std::string nandpath = globalConfig.GetString("DSi.NANDPath");
    std::cout << "[verifyDSiNAND] Path: '" << nandpath << "'" << std::endl;
    auto f = melonDS::Platform::OpenLocalFile(nandpath, melonDS::Platform::FileMode::ReadWriteExisting);
    std::cout << "[verifyDSiNAND] OpenLocalFile returned: " << (f ? "success" : "failure") << std::endl;
    if (!f) {
        std::cout << "[verifyDSiNAND] OpenLocalFile failed for path: " << nandpath << std::endl;
        std::cout << "[verifyDSiNAND] errno: " << errno << " (" << std::strerror(errno) << ")" << std::endl;
        std::cout << "[verifyDSiNAND] Path bytes: ";
        for (unsigned char c : nandpath) std::cout << std::hex << (int)c << " ";
        std::cout << std::dec << std::endl;
        return "DSi NAND was not found or could not be accessed. Check your emu settings.";
    }
    bool writable = melonDS::Platform::CheckFileWritable(nandpath);
    std::cout << "[verifyDSiNAND] CheckFileWritable: " << (writable ? "true" : "false") << std::endl;
    if (!writable)
        return "DSi NAND is unable to be written to.\nPlease check file/folder write permissions.";
    melonDS::Platform::CloseFile(f);
    return "";
}

bool ImGuiEmuInstance::loadGBAAddon(int type, std::string& errorstr) {
    if (consoleType == 1) return false;

    std::unique_ptr<melonDS::GBACart::CartCommon> cart = melonDS::GBACart::LoadAddon(type, this);
    if (!cart)
    {
        errorstr = "Failed to load the GBA addon.";
        return false;
    }

    if (nds && running)
    {
        nds->SetGBACart(std::move(cart));
    }
    else if (dsi && running)
    {
        dsi->SetGBACart(std::move(cart));
    }
    else
    {
        pendingGBAAddon = std::move(cart);
        pendingGBAAddonType = type;
    }

    gbaCartType = type;
    baseGBAROMDir = "";
    baseGBAROMName = "";
    baseGBAAssetName = "";
    gbaCartInserted = true;
    return true;
}

bool ImGuiEmuInstance::hasGBACart() const {
    return gbaCartInserted;
}

void ImGuiEmuInstance::saveConfig() {
    Config::Save();
}

std::string ImGuiEmuInstance::getEffectiveFirmwareSavePath() {
    if (!globalConfig.GetBool("Emu.ExternalBIOSEnable")) {
        return getConfigDirectory() + "/wifi_settings.bin";
    }
    if (consoleType == 1) {
        return globalConfig.GetString("DSi.FirmwarePath");
    } else {
        return globalConfig.GetString("DS.FirmwarePath");
    }
}

void ImGuiEmuInstance::initFirmwareSaveManager() {
    std::string path = getEffectiveFirmwareSavePath() + instanceFileSuffix();
    firmwareSave = std::make_unique<ImGuiSaveManager>(path);
}

void ImGuiEmuInstance::customizeFirmware(melonDS::Firmware& firmware, bool overridesettings) noexcept {
    if (!overridesettings) return;
    
    auto firmcfg = localConfig.GetTable("Firmware");
    
    auto& currentData = firmware.GetEffectiveUserData();
    
    std::string username = firmcfg.GetString("Username");
    if (!username.empty()) {
        size_t usernameLength = std::min(username.length(), (size_t)10);
        currentData.NameLength = usernameLength;
        for (size_t i = 0; i < usernameLength; i++) {
            currentData.Nickname[i] = username[i];
        }
    }
    
    int language = firmcfg.GetInt("Language");
    if (language >= 0) {
        currentData.Settings &= ~melonDS::Firmware::Language::Reserved;
        currentData.Settings |= static_cast<melonDS::Firmware::Language>(language);
    }
    
    int color = firmcfg.GetInt("FavouriteColour");
    if (color != 0xFF) {
        currentData.FavoriteColor = color;
    }
    
    int month = firmcfg.GetInt("BirthdayMonth");
    int day = firmcfg.GetInt("BirthdayDay");
    if (month > 0) {
        currentData.BirthdayMonth = month;
    }
    if (day > 0) {
        currentData.BirthdayDay = day;
    }
    
    std::string message = firmcfg.GetString("Message");
    if (!message.empty()) {
        size_t messageLength = std::min(message.length(), (size_t)26);
        currentData.MessageLength = messageLength;
        for (size_t i = 0; i < messageLength; i++) {
            currentData.Message[i] = message[i];
        }
    }
    
    melonDS::MacAddress mac;
    bool rep = false;
    auto& header = firmware.GetHeader();
    
    memcpy(&mac, header.MacAddr.data(), sizeof(melonDS::MacAddress));
    
    if (overridesettings) {
        melonDS::MacAddress configuredMac;
        rep = parseMacAddress(&configuredMac);
        rep &= (configuredMac != melonDS::MacAddress());
        
        if (rep) {
            mac = configuredMac;
        }
    }
    
    if (instanceID > 0) {
        rep = true;
        mac[3] += instanceID;
        mac[4] += instanceID*0x44;
        mac[5] += instanceID*0x10;
    }
    
    if (rep) {
        mac[0] &= 0xFC; // ensure the MAC isn't a broadcast MAC
        header.MacAddr = mac;
        header.UpdateChecksum();
    }
    
    firmware.UpdateChecksums();
}

bool ImGuiEmuInstance::parseMacAddress(void* data)
{
    const std::string mac_in = localConfig.GetString("Firmware.MAC");
    melonDS::u8* mac_out = (melonDS::u8*)data;

    int o = 0;
    melonDS::u8 tmp = 0;
    for (int i = 0; i < 18; i++)
    {
        char c = mac_in[i];
        if (c == '\0') break;

        int n;
        if      (c >= '0' && c <= '9') n = c - '0';
        else if (c >= 'a' && c <= 'f') n = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') n = c - 'A' + 10;
        else continue;

        if (!(o & 1))
            tmp = n;
        else
            mac_out[o >> 1] = n | (tmp << 4);

        o++;
        if (o >= 12) return true;
    }

    return false;
}

void ImGuiEmuInstance::setBatteryLevels() {
    if (consoleType == 1 && dsi) {
        dsi->I2C.GetBPTWL()->SetBatteryLevel(4);
        dsi->I2C.GetBPTWL()->SetBatteryCharging(false);
    } else if (nds) {
        nds->SPI.GetPowerMan()->SetBatteryLevelOkay(true);
    }
}

void ImGuiEmuInstance::setDateTime() {
    std::time_t t = std::time(nullptr);
    std::tm* now = std::localtime(&t);
    if (nds) {
        nds->RTC.SetDateTime(now->tm_year + 1900, now->tm_mon + 1, now->tm_mday,
                             now->tm_hour, now->tm_min, now->tm_sec);
    }
    if (dsi) {
        dsi->RTC.SetDateTime(now->tm_year + 1900, now->tm_mon + 1, now->tm_mday,
                             now->tm_hour, now->tm_min, now->tm_sec);
    }
}

bool ImGuiEmuInstance::bootToMenu(std::string& errorstr) {
    std::cout << "[bootToMenu] Begin" << std::endl;
    std::string setupError = verifySetup();
    std::cout << "[bootToMenu] verifySetup done: '" << setupError << "'" << std::endl;
    if (!setupError.empty()) {
        errorstr = setupError;
        std::cout << "[bootToMenu] Error: " << errorstr << std::endl;
        return false;
    }

    int newConsoleType = globalConfig.GetInt("Emu.ConsoleType");
    std::cout << "[bootToMenu] ConsoleType: " << newConsoleType << std::endl;
    if (consoleType != newConsoleType) {
        consoleType = newConsoleType;
        nds.reset();
        dsi.reset();
        std::cout << "[bootToMenu] Reset core objects" << std::endl;
    }

    std::optional<melonDS::Firmware> firmware = loadFirmware(consoleType);
    std::cout << "[bootToMenu] loadFirmware done: " << (firmware ? "success" : "fail") << std::endl;
    if (!firmware) {
        errorstr = "Failed to load firmware.";
        std::cout << "[bootToMenu] Error: " << errorstr << std::endl;
        return false;
    }

    std::optional<melonDS::DSi_NAND::NANDImage> nand;
    std::optional<melonDS::FATStorage> sdcard;
    if (consoleType == 1) {
        auto arm7ibios = loadDSiARM7BIOS();
        auto arm9ibios = loadDSiARM9BIOS();
        if (!arm7ibios || !arm9ibios) {
            errorstr = "Failed to load DSi BIOS.";
            return false;
        }
        nand = loadNAND(*arm7ibios);
        if (!nand) {
            errorstr = "Failed to load DSi NAND.";
            return false;
        }
        sdcard = loadSDCard("DSi.SD");
#ifdef JIT_ENABLED
        auto jitopt = globalConfig.GetTable("JIT");
        melonDS::JITArgs _jitargs {
                static_cast<unsigned>(jitopt.GetInt("MaxBlockSize")),
                jitopt.GetBool("LiteralOptimisations"),
                jitopt.GetBool("BranchOptimisations"),
                jitopt.GetBool("FastMemory"),
        };
        auto jitargs = jitopt.GetBool("Enable") ? std::make_optional(_jitargs) : std::nullopt;
#else
        std::optional<melonDS::JITArgs> jitargs = std::nullopt;
#endif
        melonDS::NDSArgs ndsargs{
            loadARM9BIOS(),
            loadARM7BIOS(),
            *firmware,
            jitargs,
            static_cast<melonDS::AudioBitDepth>(globalConfig.GetInt("Audio.BitDepth")),
            static_cast<melonDS::AudioInterpolation>(globalConfig.GetInt("Audio.Interpolation")),
            std::nullopt, // GDB
        };
        melonDS::DSiArgs dsiargs{
            std::move(ndsargs),
            std::move(arm9ibios),
            std::move(arm7ibios),
            std::move(*nand),
            std::move(sdcard),
            globalConfig.GetBool("DSi.FullBIOSBoot")
        };
        dsi = std::make_unique<melonDS::DSi>(std::move(dsiargs));
        // Set firmware on DSi instance (like Qt/bootFirmware)
        dsi->SetFirmware(std::move(*firmware));
    } else {
#ifdef JIT_ENABLED
        auto jitopt = globalConfig.GetTable("JIT");
        melonDS::JITArgs _jitargs {
                static_cast<unsigned>(jitopt.GetInt("MaxBlockSize")),
                jitopt.GetBool("LiteralOptimisations"),
                jitopt.GetBool("BranchOptimisations"),
                jitopt.GetBool("FastMemory"),
        };
        auto jitargs = jitopt.GetBool("Enable") ? std::make_optional(_jitargs) : std::nullopt;
#else
        std::optional<melonDS::JITArgs> jitargs = std::nullopt;
#endif
        melonDS::NDSArgs ndsargs{
            loadARM9BIOS(),
            loadARM7BIOS(),
            *firmware,
            jitargs,
            static_cast<melonDS::AudioBitDepth>(globalConfig.GetInt("Audio.BitDepth")),
            static_cast<melonDS::AudioInterpolation>(globalConfig.GetInt("Audio.Interpolation")),
            std::nullopt, // GDB
        };
        nds = std::make_unique<melonDS::NDS>(std::move(ndsargs));
    }

    reset();
    if (nds) nds->Start();
    if (dsi) dsi->Start();

    start();

    initFirmwareSaveManager();
    setBatteryLevels();
    setDateTime();
    errorstr.clear();
    return true;
}

void ImGuiEmuInstance::audioInit()
{
    audioVolume = globalConfig.GetInt("Audio.Volume");
    if (audioVolume == 0) {
        audioVolume = 256;
        globalConfig.SetInt("Audio.Volume", audioVolume);
    }
    audioDSiVolumeSync = globalConfig.GetBool("Audio.DSiVolumeSync");

    audioMuted = false;
    audioSyncCond = SDL_CreateCond();
    audioSyncLock = SDL_CreateMutex();
    
    if (!audioSyncCond || !audioSyncLock) {
        return;
    }

    audioFreq = globalConfig.GetInt("Audio.Frequency");
    if (audioFreq == 0) audioFreq = 48000;
    audioBufSize = globalConfig.GetInt("Audio.BufferSize");
    if (audioBufSize == 0) audioBufSize = 1024;
    SDL_AudioSpec whatIwant, whatIget;
    memset(&whatIwant, 0, sizeof(SDL_AudioSpec));
    whatIwant.freq = audioFreq;
    whatIwant.format = AUDIO_S16LSB;
    whatIwant.channels = 2;
    whatIwant.samples = audioBufSize;
    whatIwant.callback = audioCallback;
    whatIwant.userdata = this;
    
    audioDevice = SDL_OpenAudioDevice(NULL, 0, &whatIwant, &whatIget, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_SAMPLES_CHANGE);
    if (audioDevice)
    {
        audioFreq = whatIget.freq;
        audioBufSize = whatIget.samples;
        std::cout << "[audioInit] Audio device opened successfully" << std::endl;
        std::cout << "[audioInit] Requested freq: " << whatIwant.freq << " Hz, got: " << audioFreq << " Hz" << std::endl;
        std::cout << "[audioInit] Requested samples: " << whatIwant.samples << ", got: " << audioBufSize << std::endl;
        SDL_PauseAudioDevice(audioDevice, 1);
    }
    else
    {
        std::cout << "[audioInit] Failed to open audio device: " << SDL_GetError() << std::endl;
    }

    audioSampleFrac = 0;

    micDevice = 0;

    memset(micExtBuffer, 0, sizeof(micExtBuffer));
    micExtBufferWritePos = 0;
    micExtBufferCount = 0;
    micWavBuffer = nullptr;

    micBuffer = nullptr;
    micBufferLength = 0;
    micBufferReadPos = 0;

    micLock = SDL_CreateMutex();

    setupMicInputData();
}

void ImGuiEmuInstance::audioDeInit()
{
    if (audioDevice) SDL_CloseAudioDevice(audioDevice);
    audioDevice = 0;
    micClose();

    if (audioSyncCond) SDL_DestroyCond(audioSyncCond);
    audioSyncCond = nullptr;

    if (audioSyncLock) SDL_DestroyMutex(audioSyncLock);
    audioSyncLock = nullptr;

    if (micWavBuffer) delete[] micWavBuffer;
    micWavBuffer = nullptr;

    if (micLock) SDL_DestroyMutex(micLock);
    micLock = nullptr;
}

void ImGuiEmuInstance::audioEnable()
{
    if (audioDevice) {
        SDL_PauseAudioDevice(audioDevice, 0);
    }
    micOpen();
}

void ImGuiEmuInstance::audioDisable()
{
    if (audioDevice) {
        SDL_PauseAudioDevice(audioDevice, 1);
    }
    micClose();
}

void ImGuiEmuInstance::audioMute()
{
    audioMuted = false;
    
    extern int numEmuInstances();
    if (numEmuInstances() < 2) return;

    switch (mpAudioMode)
    {
        case 1:
            if (instanceID > 0) audioMuted = true;
            break;

        case 2:
            audioMuted = true;
            break;
    }
}

void ImGuiEmuInstance::audioSync()
{
    if (audioDevice)
    {
        SDL_LockMutex(audioSyncLock);
        if (nds) {
            int outputSize = nds->SPU.GetOutputSize();
            while (outputSize > audioBufSize)
            {
                int ret = SDL_CondWaitTimeout(audioSyncCond, audioSyncLock, 500);
                if (ret == SDL_MUTEX_TIMEDOUT) break;
                outputSize = nds->SPU.GetOutputSize();
            }
        } else if (dsi) {
            int outputSize = dsi->SPU.GetOutputSize();
            while (outputSize > audioBufSize)
            {
                int ret = SDL_CondWaitTimeout(audioSyncCond, audioSyncLock, 500);
                if (ret == SDL_MUTEX_TIMEDOUT) break;
                outputSize = dsi->SPU.GetOutputSize();
            }
        }
        SDL_UnlockMutex(audioSyncLock);
    }
}

void ImGuiEmuInstance::audioUpdateSettings()
{
    micClose();

    if (nds != nullptr)
    {
        int audiointerp = globalConfig.GetInt("Audio.Interpolation");
        nds->SPU.SetInterpolation(static_cast<melonDS::AudioInterpolation>(audiointerp));
    }

    setupMicInputData();
    micOpen();
}

void ImGuiEmuInstance::micOpen()
{
    if (micDevice) return;

    if (micInputType != 1)
    {
        micDevice = 0;
        return;
    }

    int numMics = SDL_GetNumAudioDevices(1);
    if (numMics == 0)
        return;

    SDL_AudioSpec whatIwant, whatIget;
    memset(&whatIwant, 0, sizeof(SDL_AudioSpec));
    whatIwant.freq = 44100;
    whatIwant.format = AUDIO_S16LSB;
    whatIwant.channels = 1;
    whatIwant.samples = 1024;
    whatIwant.callback = micCallback;
    whatIwant.userdata = this;
    const char* mic = NULL;
    if (micDeviceName != "")
    {
        mic = micDeviceName.c_str();
    }
    micDevice = SDL_OpenAudioDevice(mic, 1, &whatIwant, &whatIget, 0);
    if (!micDevice)
    {
        std::cout << "[micOpen] Mic init failed: " << SDL_GetError() << std::endl;
    }
    else
    {
        SDL_PauseAudioDevice(micDevice, 0);
    }
}

void ImGuiEmuInstance::micClose()
{
    if (micDevice)
        SDL_CloseAudioDevice(micDevice);

    micDevice = 0;
}

void ImGuiEmuInstance::micLoadWav(const std::string& name)
{
    SDL_AudioSpec format;
    memset(&format, 0, sizeof(SDL_AudioSpec));

    if (micWavBuffer) delete[] micWavBuffer;
    micWavBuffer = nullptr;
    micWavLength = 0;

    melonDS::u8* buf;
    melonDS::u32 len;
    if (!SDL_LoadWAV(name.c_str(), &format, &buf, &len))
        return;

    const melonDS::u64 dstfreq = 44100;

    int srcinc = format.channels;
    len /= ((SDL_AUDIO_BITSIZE(format.format) / 8) * srcinc);

    micWavLength = (len * dstfreq) / format.freq;
    if (micWavLength < 735) micWavLength = 735;
    micWavBuffer = new melonDS::s16[micWavLength];

    float res_incr = len / (float)micWavLength;
    float res_timer = 0;
    int res_pos = 0;

    for (int i = 0; i < micWavLength; i++)
    {
        melonDS::u16 val = 0;

        switch (SDL_AUDIO_BITSIZE(format.format))
        {
            case 8:
                val = buf[res_pos] << 8;
                break;

            case 16:
                if (SDL_AUDIO_ISBIGENDIAN(format.format))
                    val = (buf[res_pos*2] << 8) | buf[res_pos*2 + 1];
                else
                    val = (buf[res_pos*2 + 1] << 8) | buf[res_pos*2];
                break;

            case 32:
                if (SDL_AUDIO_ISFLOAT(format.format))
                {
                    melonDS::u32 rawval;
                    if (SDL_AUDIO_ISBIGENDIAN(format.format))
                        rawval = (buf[res_pos*4] << 24) | (buf[res_pos*4 + 1] << 16) | (buf[res_pos*4 + 2] << 8) | buf[res_pos*4 + 3];
                    else
                        rawval = (buf[res_pos*4 + 3] << 24) | (buf[res_pos*4 + 2] << 16) | (buf[res_pos*4 + 1] << 8) | buf[res_pos*4];

                    float fval = *(float*)&rawval;
                    melonDS::s32 ival = (melonDS::s32)(fval * 0x8000);
                    ival = std::clamp(ival, -0x8000, 0x7FFF);
                    val = (melonDS::s16)ival;
                }
                else if (SDL_AUDIO_ISBIGENDIAN(format.format))
                    val = (buf[res_pos*4] << 8) | buf[res_pos*4 + 1];
                else
                    val = (buf[res_pos*4 + 1] << 8) | buf[res_pos*4];
                break;
        }

        micWavBuffer[i] = (melonDS::s16)(val ^ 0x8000);

        res_timer += res_incr;
        while (res_timer >= 1.0)
        {
            res_timer -= 1.0;
            res_pos++;
        }
    }

    SDL_FreeWAV(buf);
}

void ImGuiEmuInstance::micProcess()
{
    SDL_LockMutex(micLock);

    int type = micInputType;
    bool cmd = hotkeyDown(HK_Mic);

    if (type != 1 && !cmd)
    {
        type = 0;
    }

    const int kFrameLen = 735;

    switch (type)
    {
        case 0:
            micBufferReadPos = 0;
            if (nds) {
                nds->MicInputFrame(nullptr, 0);
            } else if (dsi) {
                dsi->MicInputFrame(nullptr, 0);
            }
            break;

        case 1:
        case 2:
            if (micBuffer)
            {
                int len = kFrameLen;
                if (micExtBufferCount < len)
                    len = micExtBufferCount;

                melonDS::s16 tmp[kFrameLen];

                if ((micBufferReadPos + len) > micBufferLength)
                {
                    melonDS::u32 part1 = micBufferLength - micBufferReadPos;
                    memcpy(&tmp[0], &micBuffer[micBufferReadPos], part1*sizeof(melonDS::s16));
                    memcpy(&tmp[part1], &micBuffer[0], (len - part1)*sizeof(melonDS::s16));

                    micBufferReadPos = len - part1;
                }
                else
                {
                    memcpy(&tmp[0], &micBuffer[micBufferReadPos], len*sizeof(melonDS::s16));

                    micBufferReadPos += len;
                }

                if (len == 0)
                {
                    memset(tmp, 0, sizeof(tmp));
                }
                else if (len < kFrameLen)
                {
                    for (int i = len; i < kFrameLen; i++)
                        tmp[i] = tmp[len-1];
                }
                
                if (nds) {
                    nds->MicInputFrame(tmp, 735);
                } else if (dsi) {
                    dsi->MicInputFrame(tmp, 735);
                }

                micExtBufferCount -= len;
            }
            else
            {
                micBufferReadPos = 0;
                if (nds) {
                    nds->MicInputFrame(nullptr, 0);
                } else if (dsi) {
                    dsi->MicInputFrame(nullptr, 0);
                }
            }
            break;

        case 3:
            {
                melonDS::s16 tmp[kFrameLen];
                for (int i = 0; i < kFrameLen; i++)
                {
                    tmp[i] = (melonDS::s16)((rand() % 65536) - 32768);
                }
                
                if (nds) {
                    nds->MicInputFrame(tmp, kFrameLen);
                } else if (dsi) {
                    dsi->MicInputFrame(tmp, kFrameLen);
                }
            }
            break;
    }

    SDL_UnlockMutex(micLock);
}

void ImGuiEmuInstance::setupMicInputData()
{
    if (micWavBuffer != nullptr)
    {
        delete[] micWavBuffer;
        micWavBuffer = nullptr;
        micWavLength = 0;
    }

    micInputType = globalConfig.GetInt("Mic.InputType");
    micDeviceName = globalConfig.GetString("Mic.Device");
    micWavPath = globalConfig.GetString("Mic.WavPath");

    switch (micInputType)
    {
        case 0: // micInputType_Silence
        case 3: // micInputType_Noise
            micBuffer = nullptr;
            micBufferLength = 0;
            break;
        case 1: // micInputType_External
            micBuffer = micExtBuffer;
            micBufferLength = sizeof(micExtBuffer)/sizeof(melonDS::s16);
            break;
        case 2: // micInputType_Wav
            micLoadWav(micWavPath);
            micBuffer = micWavBuffer;
            micBufferLength = micWavLength;
            break;
    }

    micBufferReadPos = 0;
}

int ImGuiEmuInstance::audioGetNumSamplesOut(int outlen)
{
    float targetFPS = 60.0f;
    float f_len_in = (outlen * 32823.6328125 * (targetFPS/60.0)) / (float)audioFreq;
    f_len_in += audioSampleFrac;
    int len_in = (int)floor(f_len_in);
    audioSampleFrac = f_len_in - len_in;

    return len_in;
}

void ImGuiEmuInstance::audioResample(melonDS::s16* inbuf, int inlen, melonDS::s16* outbuf, int outlen, int volume)
{
    float res_incr = inlen / (float)outlen;
    float res_timer = -0.5;
    int res_pos = 0;

    for (int i = 0; i < outlen; i++)
    {
        melonDS::s16 l1 = inbuf[res_pos * 2];
        melonDS::s16 l2 = inbuf[res_pos * 2 + 2];
        melonDS::s16 r1 = inbuf[res_pos * 2 + 1];
        melonDS::s16 r2 = inbuf[res_pos * 2 + 3];

        float l = (float) l1 + ((l2 - l1) * res_timer);
        float r = (float) r1 + ((r2 - r1) * res_timer);

        outbuf[i*2  ] = (melonDS::s16) (((melonDS::s32) round(l) * volume) >> 8);
        outbuf[i*2+1] = (melonDS::s16) (((melonDS::s32) round(r) * volume) >> 8);

        res_timer += res_incr;
        while (res_timer >= 1.0)
        {
            res_timer -= 1.0;
            res_pos++;
        }
    }
}



// Audio callback
void ImGuiEmuInstance::audioCallback(void* data, Uint8* stream, int len)
{
    ImGuiEmuInstance* inst = (ImGuiEmuInstance*)data;
    len /= (sizeof(melonDS::s16) * 2);

    int len_in = inst->audioGetNumSamplesOut(len);
    if (len_in > inst->audioBufSize) len_in = inst->audioBufSize;
    
    static melonDS::s16 buf_in[4096*2];
    if (len_in > 4096) len_in = 4096;
    
    int num_in;

    SDL_LockMutex(inst->audioSyncLock);
    if (inst->nds) {
        num_in = inst->nds->SPU.ReadOutput(buf_in, len_in);
    } else if (inst->dsi) {
        num_in = inst->dsi->SPU.ReadOutput(buf_in, len_in);
    } else {
        num_in = 0;
    }
    SDL_CondSignal(inst->audioSyncCond);
    SDL_UnlockMutex(inst->audioSyncLock);

    if ((num_in < 1) || inst->audioMuted)
    {
        memset(stream, 0, len*sizeof(melonDS::s16)*2);
        return;
    }

    int margin = 6;
    if (num_in < len_in-margin)
    {
        int last = num_in-1;

        for (int i = num_in; i < len_in-margin; i++)
            ((melonDS::u32*)buf_in)[i] = ((melonDS::u32*)buf_in)[last];

        num_in = len_in-margin;
    }

    inst->audioResample(buf_in, num_in, (melonDS::s16*)stream, len, inst->audioVolume);
}

void ImGuiEmuInstance::micCallback(void* data, Uint8* stream, int len)
{
    ImGuiEmuInstance* inst = (ImGuiEmuInstance*)data;
    melonDS::s16* input = (melonDS::s16*)stream;
    len /= sizeof(melonDS::s16);

    SDL_LockMutex(inst->micLock);
    int maxlen = sizeof(inst->micExtBuffer) / sizeof(melonDS::s16);

    if ((inst->micExtBufferCount + len) > maxlen)
        len = maxlen - inst->micExtBufferCount;

    if ((inst->micExtBufferWritePos + len) > maxlen)
    {
        melonDS::u32 len1 = maxlen - inst->micExtBufferWritePos;
        memcpy(&inst->micExtBuffer[inst->micExtBufferWritePos], &input[0], len1*sizeof(melonDS::s16));
        memcpy(&inst->micExtBuffer[0], &input[len1], (len - len1)*sizeof(melonDS::s16));
        inst->micExtBufferWritePos = len - len1;
    }
    else
    {
        memcpy(&inst->micExtBuffer[inst->micExtBufferWritePos], input, len*sizeof(melonDS::s16));
        inst->micExtBufferWritePos += len;
    }

    inst->micExtBufferCount += len;
    SDL_UnlockMutex(inst->micLock);
}

const char* ImGuiEmuInstance::buttonNames[12] =
{
    "A",
    "B",
    "Select",
    "Start",
    "Right",
    "Left",
    "Up",
    "Down",
    "R",
    "L",
    "X",
    "Y"
};

const char* ImGuiEmuInstance::hotkeyNames[HK_MAX] =
{
    "HK_Lid",
    "HK_Mic",
    "HK_Pause",
    "HK_Reset",
    "HK_FastForward",
    "HK_FrameLimitToggle",
    "HK_FullscreenToggle",
    "HK_SwapScreens",
    "HK_SwapScreenEmphasis",
    "HK_SolarSensorDecrease",
    "HK_SolarSensorIncrease",
    "HK_FrameStep",
    "HK_PowerButton",
    "HK_VolumeUp",
    "HK_VolumeDown",
    "HK_SlowMo",
    "HK_FastForwardToggle",
    "HK_SlowMoToggle",
    "HK_GuitarGripGreen",
    "HK_GuitarGripRed",
    "HK_GuitarGripYellow",
    "HK_GuitarGripBlue"
};

void ImGuiEmuInstance::keyReleaseAll()
{
    keyInputMask = 0xFFF;
    keyHotkeyMask = 0;
}