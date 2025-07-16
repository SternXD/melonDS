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

#ifndef IMGUIFRONTEND_H
#define IMGUIFRONTEND_H

#include <string>
#include <vector>
#include <memory>
#include <SDL.h>

#include "../qt_sdl/Config.h"
#include "HotkeyConstants.h"

typedef unsigned int GLuint;

class ImGuiEmuInstance;
struct ImFont;

class ImGuiFrontend
{
public:
    ImGuiFrontend(int id, ImGuiEmuInstance* inst);
    ~ImGuiFrontend();

    // Core window management
    bool init();
    void cleanup();
    void show();
    void hide();
    bool shouldClose() { return shouldCloseFlag; }
    void setShouldClose(bool close) { shouldCloseFlag = close; }

    // Main loop
    void pollEvents();
    void render();
    void present();

    // Configuration
    Config::Table& getWindowConfig() { return windowCfg; }
    void saveWindowState();
    void loadWindowState();

    // OpenGL management
    void initOpenGL();
    void deinitOpenGL();
    void makeCurrentGL();
    void releaseGL();
    void drawScreenGL();

    // Input handling
    void onKeyPress(const SDL_KeyboardEvent& event);
    void onKeyRelease(const SDL_KeyboardEvent& event);

    // Window state
    bool isFocused() const { return focused; }
    void onFocusIn();
    void onFocusOut();
    
    // Pause state tracking
    bool pausedManually = false;
    bool pauseOnLostFocus = false;

    // Console window management
    void showConsoleWindow();
    void hideConsoleWindow();
    void toggleConsoleWindow();
    bool isConsoleWindowVisible() const { return consoleVisible; }

    // DS screen rendering
    void initScreenTextures();
    void updateScreenTextures();
    void renderSplashScreen();
    void renderDSScreens();
    void renderDSScreensIntegrated();

    // File operations
    void onOpenFile();
    void onOpenRecentFile(int index);
    void onClearRecentFiles();
    void onBootFirmware();
    void onInsertCart();
    void onEjectCart();
    void onInsertGBACart();
    void onEjectGBACart();
    void onImportSavefile();
    void onSaveState(int slot = 0);
    void onLoadState(int slot = 0);
    void onUndoStateLoad();
    void onQuit();

    // Emulation controls
    void onPause();
    void onReset();
    void onStop();
    void onFrameStep();

    // Cheats
    void onEnableCheats();
    void onSetupCheats();

    // ROM info
    void onROMInfo();
    void onRAMInfo();

    // Settings dialogs
    void onOpenEmuSettings();
    void onOpenInputConfig();
    void onOpenVideoSettings();
    void onOpenAudioSettings();
    void onOpenCameraSettings();
    void onOpenMPSettings();
    void onOpenWifiSettings();
    void onOpenFirmwareSettings();
    void onOpenPathSettings();
    void onOpenInterfaceSettings();
    void onOpenPowerManagement();
    void onOpenDateTime();
    void onOpenTitleManager();
    void onMPNewInstance();
    void onLANStartHost();
    void onLANStartClient();

    // Screen management
    void onOpenNewWindow();
    void onChangeScreenSize(int factor);
    void onChangeScreenRotation(int rotation);
    void onChangeScreenGap(int gap);
    void onChangeScreenLayout(int layout);
    void onChangeScreenSwap(bool swap);
    void onChangeScreenSizing(int sizing);
    void onChangeScreenAspect(int aspect, bool top);
    void onChangeIntegerScaling(bool integer);
    void onChangeScreenFiltering(bool filtering);
    void onChangeShowOSD(bool show);
    void onChangeLimitFramerate(bool limit);
    void onChangeAudioSync(bool sync);
    void onChangeSavestateSRAMReloc(bool separate);

    // Console
    void onToggleConsole();

    // OSD
    void osdAddMessage(unsigned int color, const char* msg);

    // ROM management  
    bool verifySetup();
    void updateCartInserted(bool gba);
    void loadRecentFilesMenu();
    void updateRecentFilesMenu();
    bool preloadROMs(const std::vector<std::string>& file, const std::vector<std::string>& gbafile, bool boot);
    std::vector<std::string> splitArchivePath(const std::string& filename, bool useMemberSyntax);
    std::string pickFileFromArchive(const std::string& archiveFileName);
    std::vector<std::string> pickROM(bool gba);

    // Archive support helpers
    static bool SupportedArchiveByExtension(const std::string& filename);
    static bool SupportedArchiveByMimetype(const std::string& filename);
    static bool NdsRomByExtension(const std::string& filename);
    static bool GbaRomByExtension(const std::string& filename);
    static bool NdsRomByMimetype(const std::string& filename);
    static bool GbaRomByMimetype(const std::string& filename);
    static bool FileIsSupportedFiletype(const std::string& filename, bool insideArchive = false);

    // Constants  
    static const int maxRecentFiles = 10;

    // Font management
    enum FontSize {
        FontSize_Small = 0,
        FontSize_Normal,
        FontSize_Large,
        FontSize_ExtraLarge,
        FontSize_COUNT
    };

    enum ThemeStyle {
        Theme_Dark = 0,
        Theme_Light,
        Theme_Classic,
        Theme_Ocean,
        Theme_Forest,
        Theme_Cherry,
        Theme_Purple,
        Theme_Custom,
        Theme_COUNT
    };

    void initFonts();
    void loadFont(FontSize size);
    void applyTheme(ThemeStyle theme);
    void buildFontAtlas();
    void loadFontSettings();
    void saveFontSettings();
    void setTheme(ThemeStyle theme);
    void setFontSize(FontSize size);
    void rebuildFonts();

    static bool showErrorPopup;
    static std::string errorPopupMessage;
    bool showCheatsDialog = false;
    void renderCheatsDialog();

private:
    // Core members
    int windowID;
    ImGuiEmuInstance* emuInstance;
    Config::Table windowCfg;

    // SDL/OpenGL
    SDL_Window* window;
    SDL_GLContext glContext;
    bool hasOGL;

    // Window state
    bool shouldCloseFlag;
    bool focused;
    
    // UI state
    bool showMainMenuBar;
    bool showMenuBar;
    bool showStatusBar;
    bool consoleVisible;
    
    // Font and theming system
    FontSize currentFontSize;
    ThemeStyle currentTheme;
    float fontSizes[FontSize_COUNT];
    ImFont* fonts[FontSize_COUNT];
    bool fontsLoaded;
    bool needFontRebuild;
    
    // Render frame tracking
    bool inRenderFrame;
    
    // FPS tracking
    Uint32 lastFrameTime;
    Uint32 frameCount;
    float currentFPS;
    Uint32 fpsUpdateTime;
    
    // Dialog state
    bool showEmuSettingsDialog;
    bool showInputConfigDialog; 
    bool showVideoSettingsDialog;
    bool showAudioSettingsDialog;
    bool showCameraSettingsDialog;
    bool showMPSettingsDialog;
    bool showWifiSettingsDialog;
    bool showFirmwareSettingsDialog;
    bool showPathSettingsDialog;
    bool showInterfaceSettingsDialog;
    bool showPowerManagementDialog;
    bool showDateTimeDialog;
    bool showTitleManagerDialog;
    bool showROMInfoDialog;
    bool showRAMInfoDialog;
    bool showCheatsManagementDialog;
    bool showNetplayDialog;
    bool showAboutDialog;
    bool showImGuiDemo;
    bool showOpenFileDialog = false;
    bool requestNewWindowFlag = false;
    bool requestQuitFlag = false;
    bool requestBootFirmwareFlag = false;
    bool requestLANHostFlag = false;
    bool requestMPNewInstanceFlag = false;
    
    // Emu settings change tracking
    struct EmuSettingsOriginals {
        bool externalBIOSEnable;
        std::string ds_bios9Path, ds_bios7Path, ds_firmwarePath;
        std::string dsi_bios9Path, dsi_bios7Path, dsi_firmwarePath, dsi_nandPath;
        bool dldiEnable;
        std::string dldiImagePath, dldiFolderPath;
        int dldiImageSize;
        bool dldiReadOnly, dldiFolderSync;
        bool dsiFullBoot, dsiSDEnable;
        std::string dsiSDImagePath, dsiSDFolderPath;
        int dsiSDImageSize;
        bool dsiSDReadOnly, dsiSDFolderSync;
        int consoleType;
        bool directBoot;
        bool jitEnable, jitBranch, jitLiteral, jitFastMem;
        int jitMaxBlock;
        bool gdbEnabled;
        int gdbPortARM7, gdbPortARM9;
        bool gdbBOSARM7, gdbBOSARM9;
    } emuSettingsOriginals;

    // DS screen rendering
    GLuint topScreenTexture;
    GLuint bottomScreenTexture;
    bool texturesInitialized;

    // Recent files
    std::vector<std::string> recentFiles;

    // Input mapping system
    static const int numDSButtons = 12;
    static const int numHotkeys = 22; // HK_MAX from Qt frontend
    
    // DS button and hotkey mapping arrays
    int keyMapping[numDSButtons];      // Keyboard mappings for DS buttons
    int joyMapping[numDSButtons];      // Joystick mappings for DS buttons
    int hkKeyMapping[numHotkeys];      // Keyboard mappings for hotkeys
    int hkJoyMapping[numHotkeys];      // Joystick mappings for hotkeys
    
    // Input capture state
    int* currentMappingTarget;         // Currently mapping this input
    bool isMappingInput;               // Whether we're in mapping mode
    std::string mappingButtonLabel;    // Label of button being mapped
    
    // Joystick state
    int selectedJoystickID;
    std::vector<std::string> availableJoysticks;
    
    // Input mapping mode
    bool showKeyboardMappings;
    bool showJoystickMappings;
    
    // Constants for DS buttons and hotkeys
    static const char* dsButtonNames[numDSButtons];
    static const char* hotkeyNames[numHotkeys];
    static const char* dsButtonLabels[numDSButtons];
    static const char* hotkeyLabels[numHotkeys];

    // Private UI methods
    void renderMenuBar();
    void renderStatusBar();
    void renderSettingsDialogs();
    void renderEmuSettingsDialog();
    void renderInputConfigDialog();
    void renderVideoSettingsDialog();
    void renderAudioSettingsDialog();
    void renderCameraSettingsDialog();
    void renderMPSettingsDialog();
    void renderWifiSettingsDialog();
    void renderFirmwareSettingsDialog();
    void renderPathSettingsDialog();
    void renderInterfaceSettingsDialog();
    void renderPowerManagementDialog();
    void renderDateTimeDialog();
    void renderTitleManagerDialog();
    void renderROMInfoDialog();
    void renderRAMInfoDialog();
    void renderCheatsManagementDialog();
    void renderNetplayDialog();
    
    // Input configuration helpers
    void loadInputConfig();
    void saveInputConfig();
    void updateJoystickList();
    void renderDSControlsTab();
    void renderHotkeysTab();
    void renderAddonsTab();
    void handleInputCapture();
    void startInputMapping(int* target, const std::string& label);
    void stopInputMapping();
    std::string getKeyName(int key);
    std::string getJoyButtonName(int button);
    int convertImGuiKeyToSDL(int imguiKey);
    int getHatDirection(Uint8 hat);
    
    // Emu settings helpers
    void saveEmuSettingsOriginals();
    bool checkEmuSettingsChanged();
    void applyEmuSettings();

    void showErrorDialog(const std::string& message);
    bool controllerTouchMode = false;
    std::vector<uint32_t> lastTopScreen;
    std::vector<uint32_t> lastBottomScreen;
    bool hasLastScreen = false;
    
    // Mouse tracking for touch input
    int lastMouseX = 0;
    int lastMouseY = 0;
    bool mousePressed = false;
    
    // Emulation state
    bool fastForward = false;
    bool screenSwap = false;
};

#endif // IMGUIFRONTEND_H 