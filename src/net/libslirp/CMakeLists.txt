cmake_minimum_required(VERSION 3.16)

project(libslirp VERSION 4.8.0 LANGUAGES C)

set(SLIRP_MAJOR_VERSION "${libslirp_VERSION_MAJOR}")
set(SLIRP_MINOR_VERSION "${libslirp_VERSION_MINOR}")
set(SLIRP_MICRO_VERSION "${libslirp_VERSION_PATCH}")
set(SLIRP_VERSION_STRING "\"${libslirp_VERSION}\"")

set(SOURCES
	src/arp_table.c
	src/bootp.c
	src/cksum.c
	src/dhcpv6.c
	src/dnssearch.c
	src/if.c
	src/ip6_icmp.c
	src/ip6_input.c
	src/ip6_output.c
	src/ip_icmp.c
	src/ip_input.c
	src/ip_output.c
	src/mbuf.c
	src/misc.c
	src/ncsi.c
	src/ndp_table.c
	src/sbuf.c
	src/slirp.c
	src/socket.c
	src/state.c
	src/stream.c
	src/tcp_input.c
	src/tcp_output.c
	src/tcp_subr.c
	src/tcp_timer.c
	src/tftp.c
	src/udp6.c
	src/udp.c
	src/util.c
	src/version.c
	src/vmstate.c

	# glib shim
	glib/glib.c
)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/libslirp-version.h.in" "${CMAKE_CURRENT_BINARY_DIR}/libslirp-version.h")

# Use unique target names to avoid conflicts between ImGui and Qt builds
if(MELONDS_IMGUI_FRONTEND)
    set(SLIRP_TARGET "slirp-imgui")
else()
    set(SLIRP_TARGET "slirp-qt")
endif()

# Check if target already exists before creating it
if(NOT TARGET ${SLIRP_TARGET})
    add_library(${SLIRP_TARGET} STATIC ${SOURCES})
endif()

# Only set properties if the target exists
if(TARGET ${SLIRP_TARGET})
    target_compile_definitions(${SLIRP_TARGET} PUBLIC LIBSLIRP_STATIC_BUILD)

    target_include_directories(${SLIRP_TARGET} SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/glib")
    target_include_directories(${SLIRP_TARGET} SYSTEM PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_include_directories(${SLIRP_TARGET} SYSTEM PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")

    target_compile_definitions(${SLIRP_TARGET} PRIVATE BUILDING_LIBSLIRP)
    target_compile_definitions(${SLIRP_TARGET} PRIVATE "G_LOG_DOMAIN=\"Slirp\"")

    if (WIN32)
        target_link_libraries(${SLIRP_TARGET} PRIVATE ws2_32 iphlpapi)
    elseif(HAIKU)
        target_Link_libraries(${SLIRP_TARGET} PRIVATE network)
    elseif(APPLE)
        target_link_libraries(${SLIRP_TARGET} PRIVATE resolv)
    else()
        set_source_files_properties(glib/glib.c PROPERTIES COMPILE_FLAGS -fvisibility=hidden)
    endif()
endif()
