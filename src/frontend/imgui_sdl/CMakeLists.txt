cmake_minimum_required(VERSION 3.15)

include(FetchContent)

# ImGui frontend
set(SOURCES_IMGUI_FRONTEND
    main.cpp
    ImGuiFrontend.cpp
    Platform.cpp
    FileDialog.cpp
    ImGuiMultiInstance.cpp
    ImGuiEmuInstance.cpp
    ImGuiEmuThread.cpp
    ImGuiSaveManager.cpp
    ../qt_sdl/Config.cpp
)

set(HEADERS_IMGUI_FRONTEND
    ImGuiFrontend.h
    ImGuiMultiInstance.h
    ImGuiEmuInstance.h
    ImGuiEmuThread.h
    ImGuiSaveManager.h
    FileDialog.h
)

# Add Qt config include directory
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../qt_sdl)

# Set ImGui frontend flag globally for networking library
add_compile_definitions(MELONDS_IMGUI_FRONTEND)

# Enable system libslirp to avoid linking issues
option(USE_SYSTEM_LIBSLIRP "Use system libslirp instead of the bundled version" OFF)

# Find required packages
find_package(PkgConfig REQUIRED)
find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)

# Prefer static SDL2 and ENet
set(SDL2_STATIC ON)
set(ENET_STATIC ON)

# Use pkg-config with --static for static builds
if (BUILD_STATIC)
    set(PKG_CONFIG_USE_STATIC_LIBS ON)
    set(ENV{PKG_CONFIG} "pkg-config --static")
endif()

if (BUILD_STATIC)
    # Try to find static SDL2 and ENet libraries universally
    find_library(SDL2_LIBRARY_STATIC NAMES SDL2 PATHS /usr/lib /usr/local/lib /opt/local/lib C:/msys64/ucrt64/lib)
    if (SDL2_LIBRARY_STATIC)
        set(SDL2_LIBRARIES ${SDL2_LIBRARY_STATIC})
    endif()
    find_library(ENET_LIBRARY_STATIC NAMES enet PATHS /usr/lib /usr/local/lib /opt/local/lib C:/msys64/ucrt64/lib)
    if (ENET_LIBRARY_STATIC)
        set(ENET_LIBRARIES ${ENET_LIBRARY_STATIC})
    endif()
endif()

# Find networking dependencies
pkg_check_modules(ENET REQUIRED IMPORTED_TARGET libenet)
pkg_check_modules(LibArchive REQUIRED IMPORTED_TARGET libarchive)
pkg_check_modules(Zstd REQUIRED IMPORTED_TARGET libzstd)

# Try to find pcap, but make it optional
find_package(PkgConfig QUIET)
pkg_check_modules(PCAP QUIET IMPORTED_TARGET libpcap)
if(NOT PCAP_FOUND)
    message(WARNING "libpcap not found - networking functionality will be limited")
endif()

# Fetch ImGui from official repository
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.92.1
)

FetchContent_MakeAvailable(imgui)

message(STATUS "Using ImGui v1.92.1 from FetchContent")

# Set up ImGui sources and include directories
set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

set(IMGUI_INCLUDE_DIRS ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)

# GLAD for OpenGL
set(GLAD_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../glad/glad.c
)

# Windows resource file
if(WIN32)
    set(RESOURCE_FILES melonDS.rc)
endif()

# Create the executable
add_executable(melonDS-imgui
    ${SOURCES_IMGUI_FRONTEND}
    ${HEADERS_IMGUI_FRONTEND}
    ${IMGUI_SOURCES}
    ${GLAD_SOURCES}
    ${RESOURCE_FILES}
)

# Copy font resources to build directory
set(FONT_SOURCE_DIR "${CMAKE_SOURCE_DIR}/res/fonts")
set(FONT_DEST_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/res/fonts")

if(EXISTS "${FONT_SOURCE_DIR}")
    # Create res/fonts directory in build output
    file(MAKE_DIRECTORY "${FONT_DEST_DIR}")
    
    # Copy all font files from res/fonts to build/res/fonts
    file(GLOB FONT_FILES "${FONT_SOURCE_DIR}/*.ttf" "${FONT_SOURCE_DIR}/*.otf")
    
    foreach(FONT_FILE ${FONT_FILES})
        get_filename_component(FONT_NAME ${FONT_FILE} NAME)
        configure_file("${FONT_FILE}" "${FONT_DEST_DIR}/${FONT_NAME}" COPYONLY)
    endforeach()
    
    message(STATUS "Copied fonts from ${FONT_SOURCE_DIR} to ${FONT_DEST_DIR}")
else()
    message(STATUS "Font directory ${FONT_SOURCE_DIR} not found - fonts will use system defaults")
endif()

# Add post-build command to ensure fonts are copied every time
add_custom_command(
    TARGET melonDS-imgui POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/res/fonts $<TARGET_FILE_DIR:melonDS-imgui>/res/fonts
    COMMENT "Copying font files to build directory"
)

# Include directories
target_include_directories(melonDS-imgui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
    ${CMAKE_CURRENT_SOURCE_DIR}/../glad
    ${CMAKE_SOURCE_DIR}/src
    ${IMGUI_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}
    /ucrt64/include
)

# Link libraries
target_link_libraries(melonDS-imgui PRIVATE
    core
    net-utils-imgui
    slirp-imgui
    ${SDL2_LIBRARIES}
    ${OPENGL_LIBRARIES}
    ${ENET_LIBRARIES}
    PkgConfig::LibArchive
    PkgConfig::Zstd
)

# Extra system libraries for static SDL2 on Windows
if (WIN32 AND BUILD_STATIC)
    target_link_libraries(melonDS-imgui PRIVATE
        winmm
        imm32
        version
        setupapi
        ole32
        oleaut32
        uuid
        gdi32
        shell32
        user32
        advapi32
        ws2_32
        iphlpapi
    )
endif()

# Link pcap if available
if(PCAP_FOUND)
    target_link_libraries(melonDS-imgui PRIVATE PkgConfig::PCAP)
endif()

# Platform-specific settings
if(WIN32)
    target_link_libraries(melonDS-imgui PRIVATE
        comctl32
        d2d1
        dwrite
        uxtheme
        ws2_32
        iphlpapi
    )
    
    # Use Windows subsystem for GUI application
    if(MSVC)
        set_target_properties(melonDS-imgui PROPERTIES
            LINK_FLAGS "/SUBSYSTEM:WINDOWS"
        )
    else()
        # MinGW/GCC linker flags for Windows GUI application
        target_link_options(melonDS-imgui PRIVATE -Wl,--subsystem,windows)
    endif()
endif()

# Compiler definitions
target_compile_definitions(melonDS-imgui PRIVATE MELONDS_IMGUI_FRONTEND)
target_compile_definitions(melonDS-imgui PRIVATE
    IMGUI_IMPL_OPENGL_LOADER_GLAD
)

# Set C++ standard
set_property(TARGET melonDS-imgui PROPERTY CXX_STANDARD 17) 