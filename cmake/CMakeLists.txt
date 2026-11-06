# // This file is part of UltraVNC
# // https://github.com/ultravnc/UltraVNC
# // https://uvnc.com/
# //
# // SPDX-License-Identifier: GPL-3.0-or-later
# //
# // SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
# // SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
# //


cmake_minimum_required(VERSION 3.26)

set(ULTRAVNC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")

# CMP0091 lets CMAKE_MSVC_RUNTIME_LIBRARY select the static CRT used by the
# Visual Studio projects (/MT). clang-cl honors the same flag.
cmake_policy(SET CMP0091 NEW)

if(NOT VCPKG_TARGET_TRIPLET AND DEFINED ENV{VCPKG_DEFAULT_TRIPLET})
  message(STATUS "Setting default vcpkg target triplet to $ENV{VCPKG_DEFAULT_TRIPLET}")
  set(VCPKG_TARGET_TRIPLET $ENV{VCPKG_DEFAULT_TRIPLET})
endif()
if(NOT VCPKG_TARGET_TRIPLET AND WIN32)
  set(VCPKG_TARGET_TRIPLET "x64-windows-static")
endif()

# The manifest and overlay triplets live in the repository root. The CMake
# source directory is cmake/, so point vcpkg at the parent before project().
if(NOT VCPKG_MANIFEST_DIR)
  set(VCPKG_MANIFEST_DIR "${ULTRAVNC_ROOT}" CACHE PATH "Directory containing vcpkg.json")
endif()

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VCPKG_ROOT} AND EXISTS "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
    set(_ULTRAVNC_VCPKG_ROOT "$ENV{VCPKG_ROOT}")
  elseif(EXISTS "${ULTRAVNC_ROOT}/../vcpkg/scripts/buildsystems/vcpkg.cmake")
    set(_ULTRAVNC_VCPKG_ROOT "${ULTRAVNC_ROOT}/../vcpkg")
  endif()
  if(_ULTRAVNC_VCPKG_ROOT)
    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
    set(X_VCPKG_APPLOCAL_DEPS_INSTALL ON)
    message(STATUS "VCPKG found: ${_ULTRAVNC_VCPKG_ROOT}")
    set(CMAKE_TOOLCHAIN_FILE "${_ULTRAVNC_VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
  else()
    message(WARNING "vcpkg not found. Set VCPKG_ROOT or pass -DCMAKE_TOOLCHAIN_FILE.")
  endif()
elseif(CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg.cmake")
  message(STATUS "Using toolchain: ${CMAKE_TOOLCHAIN_FILE}")
endif()

file(READ "${ULTRAVNC_ROOT}/common/versionServerViewer.inc" _UVNC_VERSION_INC)
foreach(_pair "MAJOR;APS_MAJORNUMBER" "MINOR;APS_MINORNUMBER" "BUILD;APS_BUILDNUMBER" "TWEAK;APS_MODIFICATIONNUMBER")
  list(GET _pair 0 _name)
  list(GET _pair 1 _macro)
  string(REGEX MATCH "${_macro}[ \t]+([0-9]+)" _ "${_UVNC_VERSION_INC}")
  if(NOT CMAKE_MATCH_1)
    message(FATAL_ERROR "Could not read ${_macro} from common/versionServerViewer.inc")
  endif()
  set(_UVNC_${_name} "${CMAKE_MATCH_1}")
endforeach()
set(TOOL_VERSION "${_UVNC_MAJOR}.${_UVNC_MINOR}.${_UVNC_BUILD}.${_UVNC_TWEAK}")
message(STATUS "TOOL_VERSION: ${TOOL_VERSION}")

project(UltraVNC VERSION ${TOOL_VERSION})
enable_language(CXX)
enable_language(C)
if(WIN32)
  enable_language(RC)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 17)

if(MSVC AND NOT CMAKE_MSVC_RUNTIME_LIBRARY)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if (MINGW)
    set(CMAKE_C_FLAGS "-Wno-write-strings")
    set(CMAKE_CXX_FLAGS "-Wno-write-strings -Wno-conversion-null")
endif()
# MSVC is also true for clang-cl, including the Visual Studio ClangCL toolset.
if(MSVC)
    add_compile_definitions("_CRT_SECURE_NO_WARNINGS")
    add_compile_definitions("_WINSOCK_DEPRECATED_NO_WARNINGS")
    add_compile_options($<$<OR:$<COMPILE_LANGUAGE:C>,$<COMPILE_LANGUAGE:CXX>>:/MP>)
    # CMake's MSVC default is /W3, for cl and clang-cl. /W4 is the next level
    # that still reports first-party code. /Wall also warns through the Windows SDK.
    foreach(_lang C CXX)
        if(CMAKE_${_lang}_FLAGS MATCHES "/W[0-4]")
            string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_${_lang}_FLAGS "${CMAKE_${_lang}_FLAGS}")
        else()
            string(APPEND CMAKE_${_lang}_FLAGS " /W4")
        endif()
    endforeach()
    # Resource scripts include afxres.h.
    set(CMAKE_MFC_FLAG 2)
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
    message(STATUS "Using clang-cl")
    # DES sources still use the C++17-removed register keyword.
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wno-register>)
    # /W4 above is the MSVC-compatible level. These are clang's own groups.
    # The Visual Studio generator maps -Wall to -Weverything. The groups below
    # are not defects to clean up: this tree is C++17, struct padding is the
    # VNC wire and Win32 layout, and pointer indexing plus the C library are
    # the existing memory style.
    add_compile_options(
        $<$<OR:$<COMPILE_LANGUAGE:C>,$<COMPILE_LANGUAGE:CXX>>:-Wall>
        $<$<OR:$<COMPILE_LANGUAGE:C>,$<COMPILE_LANGUAGE:CXX>>:-Wextra>
        $<$<OR:$<COMPILE_LANGUAGE:C>,$<COMPILE_LANGUAGE:CXX>>:-Wno-c++98-compat>
        $<$<OR:$<COMPILE_LANGUAGE:C>,$<COMPILE_LANGUAGE:CXX>>:-Wno-c++98-compat-pedantic>
        $<$<OR:$<COMPILE_LANGUAGE:C>,$<COMPILE_LANGUAGE:CXX>>:-Wno-c++98-compat-extra-semi>
        $<$<OR:$<COMPILE_LANGUAGE:C>,$<COMPILE_LANGUAGE:CXX>>:-Wno-c++98-compat-local-type-template-args>
        $<$<OR:$<COMPILE_LANGUAGE:C>,$<COMPILE_LANGUAGE:CXX>>:-Wno-padded>
        $<$<OR:$<COMPILE_LANGUAGE:C>,$<COMPILE_LANGUAGE:CXX>>:-Wno-unsafe-buffer-usage>
        $<$<OR:$<COMPILE_LANGUAGE:C>,$<COMPILE_LANGUAGE:CXX>>:-Wno-unsafe-buffer-usage-in-libc-call>)
endif()
# CMAKE_SYSTEM_PROCESSOR is the host on the Visual Studio generator, so a
# Win32 build still reports AMD64. Pointer size is the target.
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    add_compile_definitions("_X64")
endif()

option(asan "Build with address sanitizer enabled.")
if (asan)
    add_compile_options(-fsanitize=address)
    add_link_options(-fsanitize=address)
    if(DEFINED ENV{VCToolsInstallDir})
        install(FILES "$ENV{VCToolsInstallDir}/bin/Hostx64/x64/clang_rt.asan_dynamic-x86_64.dll" DESTINATION ${CMAKE_BINARY_DIR}/ultravnc OPTIONAL)
    endif()
endif()

find_package(minizip-ng CONFIG REQUIRED)
if(TARGET MINIZIP::minizip-ng)
  set(ULTRAVNC_MINIZIP_TARGET MINIZIP::minizip-ng)
elseif(TARGET MINIZIP::minizip)
  set(ULTRAVNC_MINIZIP_TARGET MINIZIP::minizip)
elseif(TARGET minizip)
  set(ULTRAVNC_MINIZIP_TARGET minizip)
else()
  message(FATAL_ERROR "minizip-ng was found, but it did not provide a CMake target")
endif()

if (MINGW)
    install(FILES /usr/lib/gcc/x86_64-w64-mingw32/13-win32/libstdc++-6.dll DESTINATION ${CMAKE_BINARY_DIR}/ultravnc OPTIONAL)
    install(FILES /usr/lib/gcc/x86_64-w64-mingw32/13-win32/libgcc_s_seh-1.dll DESTINATION ${CMAKE_BINARY_DIR}/ultravnc OPTIONAL)
endif()

add_subdirectory(authadmin)
add_subdirectory(authSSP)
add_subdirectory(createpassword)
add_subdirectory(ldapauth)
add_subdirectory(ldapauth9x)
add_subdirectory(ldapauthnt4)
add_subdirectory(libomnithread)
add_subdirectory(librdr)
#add_subdirectory(libudt4)
#add_subdirectory(libudtcloud)
# The legacy Info-ZIP sources are not part of the current tree. File transfer
# uses minizip-ng instead.
if(EXISTS "${ULTRAVNC_ROOT}/zipunzip_src/zip20/zip.c" AND EXISTS "${ULTRAVNC_ROOT}/zipunzip_src/unzip/api.c")
  add_subdirectory(libzip32)
  add_subdirectory(libzipunzip)
endif()
add_subdirectory(logging)
add_subdirectory(logmessages)
add_subdirectory(MSLogonACL)
add_subdirectory(repeater)
#add_subdirectory(SecureVNCPlugin) # Not publicly available.
add_subdirectory(setcad)
add_subdirectory(setpasswd)
add_subdirectory(testauth)
add_subdirectory(uvnc_settings)
add_subdirectory(uvnckeyboardhelper)
add_subdirectory(vnchooks)
add_subdirectory(vncviewer)
add_subdirectory(winvnc)
add_subdirectory(workgrpdomnt4)
