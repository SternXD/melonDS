include(FixInterfaceIncludes)
include(FindPkgConfig)

# Use unique target names to avoid conflicts between ImGui and Qt builds
if(MELONDS_IMGUI_FRONTEND)
    set(NET_UTILS_TARGET "net-utils-imgui")
    set(SLIRP_TARGET "slirp-imgui")
    message(STATUS "[melonDS] ImGui frontend: Using bundled slirp: ${SLIRP_TARGET}")
else()
    set(NET_UTILS_TARGET "net-utils-qt")
    set(SLIRP_TARGET "slirp-qt")
endif()

# Check if target already exists before creating it
if(NOT TARGET ${NET_UTILS_TARGET})
    add_library(${NET_UTILS_TARGET} STATIC
        Net.cpp
        Net_PCap.cpp
        Net_Slirp.cpp
        PacketDispatcher.cpp
        LocalMP.cpp
        LAN.cpp
        Netplay.cpp
        MPInterface.cpp
    )
    # Use bundled slirp headers to be first in include path
    target_include_directories(${NET_UTILS_TARGET} BEFORE PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/libslirp/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/libslirp"
        "${CMAKE_CURRENT_BINARY_DIR}/libslirp"
    )
endif()

# Only set properties if the target exists
if(TARGET ${NET_UTILS_TARGET})
    target_include_directories(${NET_UTILS_TARGET} PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/..")

    option(USE_SYSTEM_LIBSLIRP "Use system libslirp instead of the bundled version" OFF)
    if (USE_SYSTEM_LIBSLIRP AND NOT MELONDS_IMGUI_FRONTEND)
        pkg_check_modules(Slirp REQUIRED IMPORTED_TARGET slirp)
        fix_interface_includes(PkgConfig::Slirp)
        target_link_libraries(${NET_UTILS_TARGET} PUBLIC PkgConfig::Slirp)
    else()
        add_subdirectory(libslirp EXCLUDE_FROM_ALL)
        target_link_libraries(${NET_UTILS_TARGET} PUBLIC ${SLIRP_TARGET})
    endif()

    if (USE_VCPKG)
        find_package(unofficial-enet CONFIG REQUIRED)
        target_link_libraries(${NET_UTILS_TARGET} PRIVATE unofficial::enet::enet)
    else()
        pkg_check_modules(ENet REQUIRED IMPORTED_TARGET libenet)
        fix_interface_includes(PkgConfig::ENet)
        target_link_libraries(${NET_UTILS_TARGET} PUBLIC PkgConfig::ENet)
    endif()
endif()
