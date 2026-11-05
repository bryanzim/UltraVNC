# // This file is part of UltraVNC
# // https://github.com/ultravnc/UltraVNC
# // https://uvnc.com/
# //
# // SPDX-License-Identifier: GPL-3.0-or-later
# //
# // SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
# // SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
# //


project(winvnc LANGUAGES C CXX RC)

find_package(JPEG REQUIRED)
find_package(libjpeg-turbo CONFIG REQUIRED)
find_package(ZLIB REQUIRED)
find_package(zstd CONFIG REQUIRED)
find_package(LibLZMA REQUIRED)
find_package(unofficial-sodium CONFIG REQUIRED)
  
set(${PROJECT_NAME}_SOURCES
    ../../DSMPlugin/DSMPlugin.cpp
    ../../common/ZipUnzip/MiniZipNG.cpp
    ../../common/Base64.cpp
    ../../common/BcryptHelp.cpp
    ../../common/ChannelBinding.cpp
    ../../common/Clipboard.cpp
    ../../common/Hyperlinks.cpp
    ../../common/SaslWire.cpp
    ../../common/ScramSha256.cpp
    ../../common/UltraVncZ.cpp
    ../../common/d3des.c
    ../../common/inifile.cpp
    ../../common/win32_helpers.cpp
    ../../lzo/minilzo.c
    ../../rfb/dh.cpp
    CloudDialog.cpp
    CpuUsage.cpp
    DeskdupEngine.cpp
    DlgChangePassword.cpp
    Dtwinver.cpp
    HelperFunctions.cpp
    HideDesktop.cpp
    IPC.cpp
    LayeredWindows.cpp
    MouseSimulator.cpp
    PropertiesDialog.cpp
    RSAAESAuth.cpp
    RulesListView.cpp
    ScSelect.cpp
    SchannelTLS.cpp
    ScreenCapture.cpp
    SettingsManager.cpp
    TextChat.cpp
    UdpEchoServer.cpp
    UltraVNCService.cpp
    VirtualDisplay.cpp
    ../loadmemory/MemoryModule.c
    ../loadmemory/loadDllFromMemory.cpp
    benchmark.cpp
    buildtime.cpp
    cadthread.cpp
    credentials.cpp
    getinfo.cpp
    rfbRegion_X11.cxx
    rfbRegion_win32.cpp
    rfbUpdateTracker.cpp
    stdhdrs.cpp
    translate.cpp
    uvncUiAccess.cpp
    videodriver.cpp
    videodrivercheck.cpp
    vistahook.cpp
    vncDesktopSW.cpp
    vncEncodeTight.cpp
    vncEncodeUltra.cpp
    vncEncodeUltra2.cpp
    vncEncodeXZ.cpp
    vncEncodeZlib.cpp
    vncEncodeZlibHex.cpp
    vncListDlg.cpp
    vncMultiMonitor.cpp
    vncOSVersion.cpp
    VeNCryptAuth.cpp
    vncabout.cpp
    vncacceptdialog.cpp
    vncauth.c
    vncbuffer.cpp
    vncclient.cpp
    vncconndialog.cpp
    vncdesktop.cpp
    vncdesktopsink.cpp
    vncdesktopthread.cpp
    vncencodecorre.cpp
    vncencodehext.cpp
    vncencoder.cpp
    vncencoderCursor.cpp
    vncencoderre.cpp
    vncencodezrle.cpp
    vnchttpconnect.cpp
    vncinsthandler.cpp
    vnckeymap.cpp
    vnclog.cpp
    vncmenu.cpp
    vncntlm.cpp
    vncserver.cpp
    vncsetauth.cpp
    vncsockconnect.cpp
    vnctimedmsgbox.cpp
    vsocket.cpp
    winvnc.cpp
)
add_executable(${PROJECT_NAME} WIN32)
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/..")
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/../common")
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/../omnithread")
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/../winvnc")
foreach(file ${${PROJECT_NAME}_SOURCES})
    target_sources(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/../winvnc/winvnc/${file}")
endforeach(file)
target_sources(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/../winvnc/winvnc/winvnc.rc")
set_source_files_properties(${CMAKE_SOURCE_DIR}/../winvnc/winvnc/winvnc.rc LANGUAGE RC)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<NOT:$<CONFIG:Debug>>:NDEBUG>
    __x86__ __WIN32__ WIN32 _WINDOWS XMD_H _WINSTATIC _CRT_SECURE_NO_WARNINGS _RDPMODE _XZ _VCPKG)
target_link_libraries(${PROJECT_NAME}
    PRIVATE JPEG::JPEG
    PRIVATE $<IF:$<TARGET_EXISTS:libjpeg-turbo::turbojpeg>,libjpeg-turbo::turbojpeg,libjpeg-turbo::turbojpeg-static>
    PRIVATE ZLIB::ZLIB
    PRIVATE zstd::libzstd
    PRIVATE LibLZMA::LibLZMA
    PRIVATE unofficial-sodium::sodium
    PRIVATE ${ULTRAVNC_MINIZIP_TARGET}
    libomnithread
    librdr
    $<TARGET_NAME_IF_EXISTS:libudtcloud>
    $<TARGET_NAME_IF_EXISTS:libzip32>
    $<TARGET_NAME_IF_EXISTS:libzipunzip>
    bcrypt
    comctl32
    comdlg32
    crypt32
    cryptui
    d3d11
    delayimp
    dwmapi
    dxgi
    imm32
    iphlpapi
    mpr
    ncrypt
    newdev
    secur32
    shlwapi
    swdevice
    userenv
    version
    winmm
    ws2_32
    wtsapi32
)
if(MSVC)
    target_link_options(${PROJECT_NAME} PRIVATE "/DELAYLOAD:bcrypt.dll")
endif()
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_BINARY_DIR}/ultravnc)
if (MSVC)
    install(FILES $<TARGET_PDB_FILE:${PROJECT_NAME}> DESTINATION ${CMAKE_BINARY_DIR}/ultravnc OPTIONAL)
endif()
