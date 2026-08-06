set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

cmake_minimum_required(VERSION 3.13)

# We use PICO_SDK_PATH being set as an indicator that we're doing a Pico BUILD
if (PICO_SDK_PATH)
    # Pico-PIO-USB is an external checkout like the SDK and pico-extras (it used
    # to be a 3rdparty/ submodule). Setting PICO_PIO_USB_PATH here, before
    # pico_sdk_init(), is what makes tinyusb's own rp2040 family.cmake define the
    # tinyusb_pico_pio_usb target from our checkout instead of falling back to
    # its bundled hw/mcu/raspberry_pi/Pico-PIO-USB copy. Only the boards whose
    # USB host runs on PIO link that target (see ENABLE_PIO_USB in
    # src/pico/CMakeLists.txt), so an unset path is not an error by itself.
    if (NOT PICO_PIO_USB_PATH AND DEFINED ENV{PICO_PIO_USB_PATH})
        set(PICO_PIO_USB_PATH $ENV{PICO_PIO_USB_PATH})
    endif()
    if (PICO_PIO_USB_PATH AND NOT EXISTS ${PICO_PIO_USB_PATH}/src/pio_usb.c)
        message(FATAL_ERROR "PICO_PIO_USB_PATH=${PICO_PIO_USB_PATH} has no src/pio_usb.c.\
 Point it at a Pico-PIO-USB checkout, or unset it for a native-USB board.")
    endif()
    include(${PICO_SDK_PATH}/pico_sdk_init.cmake)
    add_subdirectory(${PICO_EXTRAS_PATH} pico_extras)
endif()

# Stamped into the UF2 binary info (picotool info) by src/CMakeLists.txt. The
# release workflow passes -DDOOM_RELEASE_VERSION=<tag> through CMAKE_ARGS, so no
# tracked file has to be rewritten to cut a release.
set(DOOM_RELEASE_VERSION "dev" CACHE STRING "Version stamped into the UF2 binary info")

project("Chocolate Doom" VERSION 3.0.0 LANGUAGES C CXX)
enable_language(CXX)
# C++17: the vendored pico_shared usb_hid headers use inline static constexpr members.
set(CMAKE_CXX_STANDARD 17)

# When ON, doom_tiny is relinked into the pico-bootLoader app partition
# (see cmake/BootPartition.cmake) and fruitjam_cflags.h picks a WHX
# address safe for the bootloader flash map.
option(BUILD_FOR_BOOTLOADER "Build doom_tiny for the pico-bootLoader" OFF)
if(BUILD_FOR_BOOTLOADER)
    add_compile_definitions(BUILD_FOR_BOOTLOADER=1)
endif()

# When ON, the device target becomes doom_tiny_full (WHD_SUPER_TINY=0): at
# boot it mounts the SD card and copies /roms/doom/doom.whd (full registered
# doom.wad converted with `whd_gen doom.wad doom.whd -no-super-tiny`) into
# PSRAM, then runs the WAD zero-copy from the XIP CS1 window at 0x11000000.
# The <board>_cflags.h headers see this define (add_compile_definitions runs
# before their forced -include) and repoint TINY_WAD_ADDR at PSRAM.
option(WHD_LOAD_FROM_SD "Load the WHD from SD card into PSRAM at boot (doom_tiny_full)" OFF)
if(WHD_LOAD_FROM_SD)
    add_compile_definitions(WHD_LOAD_FROM_SD=1)
endif()

if (PICO_SDK_PATH)
    # tinyusb comes from the SDK: PICO_TINYUSB_PATH defaults to
    # ${PICO_SDK_PATH}/lib/tinyusb, which is 0.18.0 in SDK 2.2.0 and carries the
    # RP2350 fixes and board definitions that rp2040-doom's liamfraser fork (an
    # old 3rdparty/tinyusb submodule, 0.12.0) was here for. Make sure the SDK's
    # own submodules are checked out -- pico-env.sh verifies this.
    # this only affects device builds device, but we want to use zone for malloc in this case so we don't have two separate elastic spaces and can fit more in
    set(SKIP_PICO_MALLOC 1)
    pico_sdk_init()
    # pico_generate_pio_header (used by our vendored pico_audio_i2s) is only
    # registered by pico_sdk_init(), so add the driver subdir here.
    if (PICO_ON_DEVICE)
        # XInput host class driver (Xbox 360/One/Series, 8BitDo in X-mode),
        # consumed by pico_shared_usb_hid.
        add_subdirectory(3rdparty/tusb_xinput)
    endif()
    add_subdirectory(3rdparty/pico_shared_drivers)
    if (PICO_ON_DEVICE AND NOT CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
        message(WARNING "You should do a MinSizeRel build when targeting the RP2040
        (with -DCMAKE_BUILD_TYPE=MinSizeRel)")
    endif()
endif()

set(CMAKE_C_STANDARD 11)

# Autotools variables
set(top_srcdir ${CMAKE_CURRENT_SOURCE_DIR})
set(top_builddir ${CMAKE_CURRENT_BINARY_DIR})

# AC_INIT variables
set(PACKAGE_NAME "${PROJECT_NAME}")
set(PACKAGE_TARNAME "chocolate-doom")
set(PACKAGE_VERSION "${PROJECT_VERSION}")
set(PACKAGE_STRING "${PROJECT_NAME} ${PROJECT_VERSION}")
set(PACKAGE_BUGREPORT "chocolate-doom-dev-list@chocolate-doom.org")

string(REGEX REPLACE " Doom$" "" PACKAGE_SHORTNAME "${PACKAGE_NAME}")
set(PACKAGE_COPYRIGHT "Copyright (C) 1993-2017")
set(PACKAGE_LICENSE "GNU General Public License, version 2")

# Any settings that should apply to all targets in this directory and all
# subdirectories should go here.  Use judiciously.
if(MSVC)
    add_definitions("/D_CRT_SECURE_NO_WARNINGS" "/D_CRT_SECURE_NO_DEPRECATE"
                    "/D_CRT_NONSTDC_NO_DEPRECATE")
else()
    #add_compile_options("-Wall" "-Wdeclaration-after-statement" "-Wredundant-decls")
    add_compile_options("-Wall" "-Wredundant-decls")
    # An implicit declaration means the compiler assumes int(), so a call like
    # `p = malloc(n)` silently truncates a pointer. GCC 13 only warns; GCC 14
    # made it an error by default, so code that builds on an older dev machine
    # can still break the release runner (this is exactly how a missing
    # <stdlib.h> in src/pico/i_video.c got as far as CI). Fail the same way
    # everywhere. C only: the warning does not apply to C++.
    add_compile_options("$<$<COMPILE_LANGUAGE:C>:-Werror=implicit-function-declaration>")
endif()

# Note PICO_SDK path is set by the SDK initialization if it occurs above
if (NOT PICO_SDK)
    # SDL2 (+ mixer/net) are only needed for the native SDL builds
    # (chocolate-doom, -setup, -server, midiread, mus2mid). The host-only
    # whd_gen WAD tool needs none of them, so probe without REQUIRED and let
    # configure succeed for a whd_gen-only build when they are absent.
    find_package(SDL2 2.0.1)
    find_package(SDL2_mixer 2.0.0)
    find_package(SDL2_net 2.0.0)
    if (SDL2_FOUND AND SDL2_MIXER_FOUND AND SDL2_NET_FOUND)
        set(HAVE_SDL TRUE)
    else()
        set(HAVE_SDL FALSE)
        message(STATUS "SDL2/SDL2_mixer/SDL2_net not all found: native SDL "
                "targets disabled; only host tools (whd_gen) will build. Install "
                "libsdl2-dev, libsdl2-mixer-dev and libsdl2-net-dev to build "
                "chocolate-doom and friends.")
    endif()

    # Check for libsamplerate.
    find_package(samplerate)
    if(SAMPLERATE_FOUND)
        set(HAVE_LIBSAMPLERATE TRUE)
    endif()

    # Check for libpng.
    find_package(PNG)
    if(PNG_FOUND)
        set(HAVE_LIBPNG TRUE)
    endif()
endif()

set(HAVE_MMAP 1)
find_package(m)

include(CheckSymbolExists)
include(CheckIncludeFile)
check_symbol_exists(strcasecmp "strings.h" HAVE_DECL_STRCASECMP)
check_symbol_exists(strncasecmp "strings.h" HAVE_DECL_STRNCASECMP)
check_include_file("dirent.h" HAVE_DIRENT_H)

string(CONCAT WINDOWS_RC_VERSION "${PROJECT_VERSION_MAJOR}, "
    "${PROJECT_VERSION_MINOR}, ${PROJECT_VERSION_PATCH}, 0")

# Without a hyphen. This is used for the bash-completion scripts.
string(TOLOWER "${PACKAGE_SHORTNAME}" PROGRAM_SPREFIX)

# With a hyphen, used almost everywhere else.
set(PROGRAM_PREFIX "${PROGRAM_SPREFIX}-")

configure_file(cmake/config.h.cin config.h)

configure_file(src/resource.rc.in src/resource.rc)
configure_file(src/setup-res.rc.in src/setup-res.rc)
configure_file(src/setup/setup-manifest.xml.in src/setup/setup-manifest.xml)

# textscreen/opl/pcsound link SDL2 (mixer) unconditionally in their native
# branch, so only add them when SDL is available. src always builds — it hosts
# the SDL-free whd_gen tool (its SDL games are gated on HAVE_SDL internally).
if (PICO_SDK OR HAVE_SDL)
    set(BUILD_SUBDIRS textscreen midiproc opl pcsound src)
else()
    set(BUILD_SUBDIRS src)
endif()
foreach(SUBDIR ${BUILD_SUBDIRS})
    add_subdirectory("${SUBDIR}")
endforeach()
