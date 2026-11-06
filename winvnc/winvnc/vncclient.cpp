// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 2015 D. R. Commander. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 2000-2002 Const Kaplinsky. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 2002 RealVNC Ltd. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999 AT&T Laboratories Cambridge. All Rights Reserved.
//


#define _WINSOCK_DEPRECATED_NO_WARNINGS 1
#define DEBUG_FT 1
// vncClient.cpp

// The per-client object. This object takes care of all per-client stuff,
// such as socket input and buffering of updates.

// vncClient class handles the following functions:
// - Recieves requests from the connected client and
//   handles them
// - Handles incoming updates properly, using a vncBuffer
//   object to keep track of screen changes
// It uses a vncBuffer and is passed the vncDesktop and
// vncServer to communicate with.

// Includes
#include "stdhdrs.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

#ifndef MOUSEEVENTF_HWHEEL
#define MOUSEEVENTF_HWHEEL 0x01000
#endif

#include "vncclient.h"

#include "resource.h"
#include "rfb/dh.h"
#include "rfb/HighResolutionWheel.h"
#include "rfbRegion.h"
#include "RSAAESAuth.h"
#include "vncacceptdialog.h"
#include "vncauth.h"
#include "vncbuffer.h"
#include "vncdesktop.h"
#include "vnckeymap.h"
#include "vncmenu.h"
#include "vncpasswd.h"
// Custom
#include "vncserver.h"
#include "vsocket.h"

#include <omnithread.h>
#include <sodium.h> // For X25519 key exchange (MS-Logon III)

#include <sstream>
#include <string>

#ifdef _VCPKG
#include <zlib.h>
#include <zstd.h>

#else
#include "../zlib/zlib.h"
#include "../zstd/lib/zstd.h"

#endif

#include "common/win32_helpers.h"
#include "credentials.h"
#include "SettingsManager.h"
#include "uvncUiAccess.h"
#include "VirtualDisplay.h"
#include "vncOSVersion.h"
#include "winvnc.h"

#include <mmsystem.h> // sf@2002
#include <ShlObj.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <UserEnv.h>

#include <iterator>
#include <map>
#include <string>

using namespace std;

#pragma comment(lib, "mpr.lib") //for getting full mapped drive

#define DWEXTRA_VNC_REMOTE 0x564e4300

bool isDirectoryTransfer(const char* szFileName);

int getinfo(char mytext[1024U]);
int calc_updates{ 0 };
int old_calc_updates{ 0 };
extern bool PreConnect;
int PreConnectID{ 0 };

// take a full path & file name, split it, prepend prefix to filename, then merge it back
static std::string make_temp_filename(const char* const szFullPath)
{
    // don't add prefix for directory transfers.
    if (isDirectoryTransfer(szFullPath))
    {
        return szFullPath;
    }

    std::string tmpName(szFullPath);
    std::string::size_type pos = tmpName.rfind('\\');
    if (pos != std::string::npos)
    {
        tmpName.insert(pos + 1, rfbPartialFilePrefix);
    }

    return tmpName;
}

std::string get_real_filename(std::string name)
{
    std::string::size_type pos;

    pos = name.find(rfbPartialFilePrefix);
    if (pos != std::string::npos)
    {
        name.erase(pos, sz_rfbPartialFilePrefix);
    }

    return name;
}

// Returns true if the supplied peer address is a loopback address.
// Used to decide whether the opt-in AllowLoopbackWithoutDSM / AllowLoopbackWithoutAuth
// shortcuts may be applied. Only the addresses actually returned by VSocket::GetPeerName
// for IPv4/IPv6 loopback are recognised here.
static bool IsLoopbackAddress(const char* const addr)
{
    if (addr == nullptr)
    {
        return false;
    }
    if (::_stricmp(addr, "127.0.0.1") == 0)
    {
        return true;
    }
    if (::_stricmp(addr, "::1") == 0)
    {
        return true;
    }
    // IPv4-mapped IPv6 loopback, e.g. when an IPv6-mode socket accepts an IPv4 peer.
    if (::_stricmp(addr, "::ffff:127.0.0.1") == 0)
    {
        return true;
    }
    return false;
}

// Canonicalize a file-transfer destination path and pin it to the configured root.
// Returns true if the path is acceptable; on success outCanonical holds the canonical UTF-8 path.
static bool ValidateFileTransferPath(const char* const szPath, const char* szUtf8Root, std::string& outCanonical,
                                     const UINT cp = CP_UTF8)
{
    outCanonical.clear();

    std::array<WCHAR, MAX_PATH * 4> szPathW;
    int nPath = ::MultiByteToWideChar(cp, 0, szPath, -1, szPathW.data(), static_cast<int>(szPathW.size()));
    if (nPath <= 0)
    {
        return false;
    }

    std::array<WCHAR, MAX_PATH * 4> szCanonW;
    DWORD dwLen = ::GetFullPathNameW(szPathW.data(), static_cast<DWORD>(szCanonW.size()), szCanonW.data(), nullptr);
    if ((dwLen == 0) || (dwLen >= szCanonW.size()))
    {
        return false;
    }

    // Only accept absolute paths: X:\... or \\server\share
    bool fAbsolute = ((szCanonW[0U] != L'\0') && (szCanonW[1U] == L':') && (szCanonW[2U] == L'\\')) ||
                     ((szCanonW[0U] == L'\\') && (szCanonW[1U] == L'\\'));
    if (!fAbsolute)
    {
        return false;
    }

    // Reject any .. component remaining after canonicalization
    const WCHAR* p = szCanonW.data();
    while (*p)
    {
        while (*p == L'\\')
        {
            ++p;
        }
        const WCHAR* seg = p;
        while (*p && (*p != L'\\'))
        {
            ++p;
        }
        std::size_t segLen = static_cast<std::size_t>(p - seg);
        if ((segLen == 2) && (seg[0U] == L'.') && (seg[1U] == L'.'))
        {
            return false;
        }
        if (*p)
        {
            ++p;
        }
    }

    // If a root is configured, the path must be inside it
    if (szUtf8Root && (szUtf8Root[0U] != '\0'))
    {
        std::array<WCHAR, MAX_PATH * 4> szRootW;
        int nRoot = ::MultiByteToWideChar(CP_ACP, 0, szUtf8Root, -1, szRootW.data(), static_cast<int>(szRootW.size()));
        if (nRoot <= 0)
        {
            return false;
        }

        std::array<WCHAR, MAX_PATH * 4> szRootCanonW;
        DWORD dwRootLen = ::GetFullPathNameW(szRootW.data(), static_cast<DWORD>(szRootCanonW.size()), szRootCanonW.data(), nullptr);
        if ((dwRootLen == 0) || (dwRootLen >= szRootCanonW.size()))
        {
            return false;
        }

        std::size_t rootLen = std::wcslen(szRootCanonW.data());
        if (rootLen == 0)
        {
            return false;
        }
        if (szRootCanonW[rootLen - 1] != L'\\')
        {
            if (rootLen + 1 >= szRootCanonW.size())
            {
                return false;
            }
            ::wcscat_s(szRootCanonW.data(), szRootCanonW.size(), L"\\");
        }

        std::size_t rootPrefix = std::wcslen(szRootCanonW.data());
        if ((std::wcslen(szCanonW.data()) < rootPrefix) || (::_wcsnicmp(szCanonW.data(), szRootCanonW.data(), rootPrefix) != 0))
        {
            return false;
        }
    }

    std::array<char, MAX_PATH * 4> szCanonA;
    int nBytes = ::WideCharToMultiByte(CP_UTF8, 0, szCanonW.data(), -1, szCanonA.data(), sizeof(szCanonA), nullptr, nullptr);
    if (nBytes <= 0)
    {
        return false;
    }

    outCanonical = szCanonA.data();
    return true;
}

// #include "rfb.h"
bool DeleteFileOrDirectory(WCHAR* srcpath)
{
    // For files: use DeleteFileW with \\?\ prefix to support long paths (>260 chars).
    // For directories: use SHFileOperationW (recursive delete); \\?\ not supported there.
    DWORD attr = ::GetFileAttributesW(srcpath);
    if ((attr != INVALID_FILE_ATTRIBUTES) && !(attr & FILE_ATTRIBUTE_DIRECTORY))
    {
        // It's a file - use DeleteFileW with long path prefix
        std::array<WCHAR, MAX_PATH * 4 + 4> longpath;
        if (srcpath[0U] && (srcpath[1U] == L':'))
        {
            ::_snwprintf_s(longpath.data(), longpath.size(), _TRUNCATE, L"\\\\?\\%s", srcpath);
        }
        else
        {
            ::wcscpy_s(longpath.data(), longpath.size(), srcpath);
        }
        return ::DeleteFileW(longpath.data()) ? true : false;
    }

    // It's a directory (or doesn't exist) - use SHFileOperationW for recursive delete
    std::array<WCHAR, MAX_PATH * 4 + 2> path; // double null terminator required by SHFileOperationW
    std::memset(path.data(), 0, sizeof path);
    ::wcsncpy_s(path.data(), path.size(), srcpath, MAX_PATH * 4);
    path[std::wcslen(srcpath) + 1] = 0;

    SHFILEOPSTRUCTW op;
    std::memset(&op, 0, sizeof(SHFILEOPSTRUCTW));
    op.wFunc = FO_DELETE;
    op.pFrom = path.data();
    op.fFlags = FOF_SILENT | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_NOCONFIRMMKDIR;

    const int result = ::SHFileOperationW(&op);
    // MSDN says to not look at the error code, just treat 0 as SUCCESS, nonzero is failure.
    return result == 0;
}

static void MakeLongPath(const char* const utf8, WCHAR* const outW, const int outLen)
{
    std::array<WCHAR, MAX_PATH * 4> tmp;
    ::MultiByteToWideChar(CP_UTF8, 0, utf8, -1, tmp.data(), static_cast<int>(tmp.size()));
    if (tmp[0U] && (tmp[1U] == L':'))
    {
        ::_snwprintf_s(outW, static_cast<size_t>(outLen), _TRUNCATE, L"\\\\?\\%s", tmp.data());
    }
    else
    {
        ::wcscpy_s(outW, static_cast<rsize_t>(outLen), tmp.data());
    }
}

bool replaceFile(const char* const src, const char* const dst)
{
    // src and dst are UTF-8 paths - use \\?\ prefix for long path support
    std::array<WCHAR, MAX_PATH * 4> srcW;
    std::array<WCHAR, MAX_PATH * 4> dstW;
    MakeLongPath(src, srcW.data(), static_cast<int>(srcW.size()));
    MakeLongPath(dst, dstW.data(), static_cast<int>(dstW.size()));

    DWORD dwFileAttribs;
    bool status;

    dwFileAttribs = ::GetFileAttributesW(dstW.data());
    // make the file read/write if it's read only.
    if ((dwFileAttribs != INVALID_FILE_ATTRIBUTES) && dwFileAttribs & FILE_ATTRIBUTE_READONLY)
    {
        ::SetFileAttributesW(dstW.data(), dwFileAttribs & static_cast<DWORD>(~FILE_ATTRIBUTE_READONLY));
    }

    status = ::MoveFileExW(srcW.data(), dstW.data(), MOVEFILE_REPLACE_EXISTING) ? true : false;

    // restore orginal file attributes, if we have them. We won't have them if
    // the destination file didn't exist prior to the copy/move.
    if (dwFileAttribs != INVALID_FILE_ATTRIBUTES)
    {
        ::SetFileAttributesW(dstW.data(), dwFileAttribs);
    }

    return status;
}
#include "Localization.h" // Act : add localization on messages

typedef BOOL(WINAPI* PGETDISKFREESPACEEX)(LPCSTR, PULARGE_INTEGER, PULARGE_INTEGER, PULARGE_INTEGER);

// vncClient update thread class

//	[v1.0.2-jp1 fix]
//	yak!'s File transfer patch
//	Simply forward strchr() and strrchr() to _mbschr() and _mbsrchr() to avoid 0x5c problem, respectively.
//	Probably, it is better to write forward functions internally.
#include <mbstring.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <vector>

#define strchr(a, b) reinterpret_cast<char*>(_mbschr(reinterpret_cast<unsigned char*>(a), b))
#define strrchr(a, b) reinterpret_cast<char*>(_mbsrchr(reinterpret_cast<unsigned char*>(a), b))

// 31 January 2008 jdp
std::string AddDirPrefixAndSuffix(const char* const name)
{
    std::string dirname(rfbDirPrefix);
    dirname += name;
    dirname += rfbDirSuffix;

    return dirname;
}

bool isDirectory(const char* const name)
{
    struct _stat statbuf;
    // Convert UTF-8 to Unicode for Chinese character support
    std::array<WCHAR, MAX_PATH> nameW;
    ::MultiByteToWideChar(CP_UTF8, 0, name, -1, nameW.data(), static_cast<int>(nameW.size()));
    ::_wstat(nameW.data(), &statbuf);
    return (statbuf.st_mode & _S_IFDIR) == _S_IFDIR;
}
bool isDirectoryTransfer(const char* const szFileName)
{
    return (std::strncmp(strrchr(const_cast<char*>(szFileName), '\\') + 1, rfbZipDirectoryPrefix,
                         std::strlen(rfbZipDirectoryPrefix)) == 0);
}

void GetZippedFolderPathName(const char* const szZipFile, char* const path)
{
    // input lookes like: "c:\temp\!UVNCDIR-folder.zip"
    // output should look like "c:\temp\folder"

    const char* zip_ext = ".zip";
    std::string folder_path(szZipFile);
    std::string::size_type pos;

    // remove "!UVNCDIR-"
    pos = folder_path.find(rfbZipDirectoryPrefix);
    if (pos != std::string::npos)
    {
        folder_path.replace(pos, std::strlen(rfbZipDirectoryPrefix), "");
    }

    // remove ".zip"
    pos = folder_path.rfind(zip_ext);
    if (pos != std::string::npos)
    {
        folder_path.replace(pos, std::strlen(zip_ext), "");
    }

    // remove "[ "
    pos = folder_path.find(rfbDirPrefix);
    if (pos != std::string::npos)
    {
        folder_path.replace(pos, std::strlen(rfbDirPrefix), "");
    }

    // remove " ]"
    pos = folder_path.rfind(rfbDirSuffix);
    if (pos != std::string::npos)
    {
        folder_path.replace(pos, std::strlen(rfbDirSuffix), "");
    }

    std::copy(folder_path.begin(), folder_path.end(), path);
    path[folder_path.size()] = 0; // terminate the string
}

void SplitTransferredFileNameAndDate(char* const szFileAndDate, char* filetime)
{
    char* p = strrchr(szFileAndDate, ',');
    if (p == nullptr)
    {
        if (filetime)
        {
            *filetime = '\0';
        }
    }
    else
    {
        if (filetime)
        {
            ::strcpy_s(filetime, 324, p + 1);
        }
        *p = '\0';
    }
}

/*
 * File transfer event hooks
 *
 * The following functions are called from various points in the File Transfer
 * process. They are notification hooks; the hook function cannot affect the
 * file operation. One possible use of these hooks is to implement auditing
 * of file operations on the server side.
 *
 * Note that uploads are transfers from the server to the viewer and a
 * download is a transfer from the viewer to the server.
 *
 * The hook will have to use the state stored in the vncClient object
 * to figure out what was done in most cases. For those cases where it
 * is not possible, the hook is passed enough information about the event.
 *
 */

// called at the successful start of a file upload
void vncClient::FTUploadStartHook() {}

// called when an abort is received from the viewer. it is assumed that it was
// due to the user cancelling the transfer
void vncClient::FTUploadCancelledHook() {}

// Called when an upload fails for any reason: network failure, out of disk space, insufficient privileges etc.
void vncClient::FTUploadFailureHook() {}

// Called after the file is successfully uploaded. At this point the File Transfer is complete.
void vncClient::FTUploadCompleteHook() {}

// called at the successful start of a file download
void vncClient::FTDownloadStartHook() {}

// called when an abort is received from the viewer. it is assumed that it was
// due to the user cancelling the transfer
void vncClient::FTDownloadCancelledHook() {}

// Called when an dowload fails for any reason: network failure, out of disk space, insufficient privileges etc.
void vncClient::FTDownloadFailureHook() {}

// Called after the file is successfully downloaded. At this point the File Transfer is complete.
void vncClient::FTDownloadCompleteHook() {}

// Called when a new folder is created.
void vncClient::FTNewFolderHook([[maybe_unused]] const std::string name) {}

// called when a folder is created
void vncClient::FTDeleteHook([[maybe_unused]] const std::string name, [[maybe_unused]] const bool isDir) {}

// called when a file or folder is renamed.
void vncClient::FTRenameHook([[maybe_unused]] const std::string oldName, [[maybe_unused]] const std::string newname) {}

BOOL vncClientUpdateThread::Init(vncClient* client)
{
    vnclog.Print(LL_INTINFO, VNCLOG("init update thread\n"));
    m_client = client;
    omni_mutex_lock l(m_client->GetUpdateLock(), 80);
    m_signal = new omni_condition(&m_client->GetUpdateLock());
    m_sync_sig = new omni_condition(&m_client->GetUpdateLock());
    m_active = TRUE;
    m_enable = m_client->m_disable_protocol == 0;
    if (m_signal && m_sync_sig)
    {
        start_undetached();
        return TRUE;
    }
    return FALSE;
}

vncClientUpdateThread::~vncClientUpdateThread()
{
    if (m_signal)
    {
        delete m_signal;
    }
    if (m_sync_sig)
    {
        delete m_sync_sig;
    }
    m_client->m_updatethread = nullptr;
}

void vncClientUpdateThread::Trigger()
{
    // ALWAYS lock client UpdateLock before calling this!
    // Only trigger an update if protocol is enabled
    if (m_client->m_disable_protocol == 0)
    {
        m_signal->signal();
    }
}

void vncClientUpdateThread::Kill()
{
    omni_mutex_lock l(m_client->GetUpdateLock(), 81);
    m_active = FALSE;
    m_signal->signal();
}

void vncClientUpdateThread::get_time_now(unsigned long* abs_sec, unsigned long* abs_nsec)
{
    static std::array<int, 12U> days_in_preceding_months{ 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
    static std::array<int, 12U> days_in_preceding_months_leap{ 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335 };

    SYSTEMTIME st;

    ::GetSystemTime(&st);
    *abs_nsec = st.wMilliseconds * 1000000;

    // this formula should work until 1st March 2100

    const DWORD days =
        ((st.wYear - 1970) * 365 + (st.wYear - 1969) / 4 +
         ((st.wYear % 4) ? days_in_preceding_months[st.wMonth - 1] : days_in_preceding_months_leap[st.wMonth - 1]) + static_cast<DWORD>(st.wDay - 1));

    *abs_sec = st.wSecond + 60 * (st.wMinute + 60 * (st.wHour + 24 * days));
}
void vncClientUpdateThread::EnableUpdates(BOOL enable)
{
    // ALWAYS call this with the UpdateLock held!
    m_enable = enable;
    m_signal->signal();
    //unsigned long now_sec, now_nsec;
    //get_time_now(&now_sec, &now_nsec);

    // give bad results with java
    //if (enable)
    if (!m_sync_sig->wait(5000))
    {
        vnclog.Print(LL_INTINFO, VNCLOG("wait timeout\n"));
    }
    /*if  (m_sync_sig->timedwait(now_sec+1,0)==0)
        {
//			m_signal->signal();
            vnclog.Print(LL_INTINFO, VNCLOG("thread timeout\n"));
        } */
}

extern bool g_DesktopThread_running;

void* vncClientUpdateThread::run_undetached([[maybe_unused]] void* const arg)
{
    rfb::SimpleUpdateTracker update;
    rfb::Region2D clipregion;
    // adzm - 2010-07 - Extended clipboard
    //char *clipboard_text = 0;
    update.enable_copyrect(true);
    BOOL send_palette = FALSE;
    const int UPDATE_INTERVAL{ 40 };
    first_run = true;

    vnclog.Print(LL_INTINFO, VNCLOG("starting update thread\n"));
    //Make sure we never can get locked by the initail m_initial_update) wait loop
    //After 5 sec cont.
    while (g_DesktopThread_running && m_client->cl_connected)
    {
        {
            m_client->m_incr_rgn.assign_union(clipregion);
            omni_mutex_lock l(m_client->GetUpdateLock(), 82);
            // We block as long as updates are disabled, or the client
            // isn't interested in them, unless this thread is killed.

            if (first_run)
            {
                while (m_active &&
                       (!m_enable || (m_client->m_update_tracker.get_changed_region().intersect(m_client->m_incr_rgn).is_empty() &&
                                      m_client->m_update_tracker.get_copied_region().intersect(m_client->m_incr_rgn).is_empty() &&
                                      m_client->m_update_tracker.get_cached_region().intersect(m_client->m_incr_rgn).is_empty() &&
                                      // adzm - 2010-07 - Extended clipboard
                                      !(m_client->m_clipboard.m_bNeedToProvide || m_client->m_clipboard.m_bNeedToNotify) &&
                                      !m_client->m_cursor_pos_changed // nyama/marscha - PointerPos
                                      )))
                {
                    // Issue the synchronisation signal, to tell other threads
                    // where we have got to
                    m_sync_sig->broadcast();
                    // Wait to be kicked into action
                    m_signal->wait();
                    first_run = false;
                }
            }
            else
            {
                while (
                    m_active &&
                    (!m_enable || (m_client->m_update_tracker.get_changed_region().intersect(m_client->m_incr_rgn).is_empty() &&
                                   m_client->m_update_tracker.get_copied_region().intersect(m_client->m_incr_rgn).is_empty() &&
                                   m_client->m_update_tracker.get_cached_region().intersect(m_client->m_incr_rgn).is_empty() &&
                                   !m_client->m_encodemgr.IsCursorUpdatePending() &&
                                   // adzm - 2010-07 - Extended clipboard
                                   !(m_client->m_clipboard.m_bNeedToProvide || m_client->m_clipboard.m_bNeedToNotify) &&
                                   !m_client->m_NewSWUpdateWaiting && !m_client->m_cursor_pos_changed // nyama/marscha - PointerPos
                                   )))
                {
                    // Issue the synchronisation signal, to tell other threads
                    // where we have got to
                    m_sync_sig->broadcast();
                    do
                    {
                        if (!m_client->cl_connected)
                        {
                            return nullptr;
                        }
                        if (m_signal->wait(UPDATE_INTERVAL * 100) == false)
                        {
                            //do forcefull update after 4 seconds
                            m_client->TriggerUpdate();
                            m_client->TriggerUpdateThread();
                        }
                        else
                        {
                            break;
                        }
                    } while (g_DesktopThread_running);
                }
            }
            // If the thread is being killed then quit
            if (!m_active)
            {
                break;
            }
            clipregion = m_client->m_incr_rgn;
            m_client->m_incr_rgn.clear();

            // sf@2002
            // New scale requested, we do it before sending the next Update
            if (m_client->fNewScale)
            {
                // Send the new framebuffer size to the client
                rfb::Rect ViewerSize = m_client->m_encodemgr.m_buffer->GetViewerSize();

                // Copyright (C) 2001 - Harakan software
                if (m_client->m_fPalmVNCScaling)
                {
                    rfb::Rect ScreenSize = m_client->m_encodemgr.m_buffer->GetSize();
                    rfbPalmVNCReSizeFrameBufferMsg rsfb{ 0 };

                    rsfb.type = rfbPalmVNCReSizeFrameBuffer;
                    rsfb.desktop_w = Swap16IfLE(ScreenSize.br.x);
                    rsfb.desktop_h = Swap16IfLE(ScreenSize.br.y);
                    rsfb.buffer_w = Swap16IfLE(ViewerSize.br.x);
                    rsfb.buffer_h = Swap16IfLE(ViewerSize.br.y);
                    omni_mutex_lock lInner(m_client->GetUpdateLock(), 82);
                    m_client->m_socket->SendExact(reinterpret_cast<char*>(&rsfb), sz_rfbPalmVNCReSizeFrameBufferMsg, rfbPalmVNCReSizeFrameBuffer);
                }
                else // eSVNC-UltraVNC Scaling
                {
                    rfbResizeFrameBufferMsg rsmsg;
                    std::memset(&rsmsg, 0, sizeof(rfbResizeFrameBufferMsg));
                    rsmsg.type = rfbResizeFrameBuffer;
                    rsmsg.framebufferWidth = Swap16IfLE(ViewerSize.br.x);
                    rsmsg.framebufferHeigth = Swap16IfLE(ViewerSize.br.y);
                    omni_mutex_lock lInner(m_client->GetUpdateLock(), 82);
                    m_client->m_socket->SendExact(reinterpret_cast<char*>(&rsmsg), sz_rfbResizeFrameBufferMsg, rfbResizeFrameBuffer);
                    m_client->m_ScaledScreen = m_client->m_encodemgr.m_buffer->GetViewerSize();
                    m_client->m_nScale = m_client->m_encodemgr.m_buffer->GetScale();
                }

                m_client->m_encodemgr.m_buffer->ClearCache();
                m_client->fNewScale = false;
                m_client->m_fPalmVNCScaling = false;

                // return 0;
            }

            // Has the palette changed?
            send_palette = m_client->m_palettechanged;
            m_client->m_palettechanged = FALSE;

            // Get the update details from the update tracker
            m_client->m_update_tracker.flush_update(update, clipregion);

            m_client->m_cursor_update_pending = m_client->m_encodemgr.WasCursorUpdatePending();

            if (!m_client->m_cursor_update_sent && !m_client->m_cursor_update_pending)
            {
                if (m_client->m_mousemoved)
                {
                    // Re-render its old location
                    m_client->m_oldmousepos = m_client->m_oldmousepos.intersect(m_client->m_ScaledScreen); // sf@2002
                    if (!m_client->m_oldmousepos.is_empty())
                    {
                        update.add_changed(m_client->m_oldmousepos);
                    }

                    // And render its new one
                    m_client->m_encodemgr.m_buffer->GetMousePos(m_client->m_oldmousepos);
                    m_client->m_oldmousepos = m_client->m_oldmousepos.intersect(m_client->m_ScaledScreen);
                    if (!m_client->m_oldmousepos.is_empty())
                    {
                        update.add_changed(m_client->m_oldmousepos);
                    }
                    m_client->m_mousemoved = FALSE;
                }
            }

            // SEND THE CLIPBOARD
            // If there is clipboard text to be sent then send it
            // Also allow in loopbackmode
            // Loopback mode with VNC Viewer will cause a loping
            // But ssh is back working
            if (!m_client->m_fFileSessionOpen)
            {
                bool bShouldFlush{ false };
                omni_mutex_lock lInner(m_client->GetUpdateLock(), 82);
                // adzm - 2010-07 - Extended clipboard
                // send any clipboard data that should be sent automatically
                if (m_client->m_clipboard.m_bNeedToProvide)
                {
                    m_client->m_clipboard.m_bNeedToProvide = false;
                    if (m_client->m_clipboard.settings.m_bSupportsEx)
                    {
                        int actualLen = m_client->m_clipboard.extendedClipboardDataMessage.GetDataLength();

                        rfbServerCutTextMsg message;
                        std::memset(&message, 0, sizeof(rfbServerCutTextMsg));
                        message.type = rfbServerCutText;

                        message.length = static_cast<unsigned int>(Swap32IfLE(-actualLen));

                        bShouldFlush = true;

                        //adzm 2010-09 - minimize packets. SendExact flushes the queue.
                        if (!m_client->SendRFBMsgQueue(rfbServerCutText, reinterpret_cast<BYTE*>(&message), sizeof(message)))
                        {
                            m_client->m_socket->Close();
                        }
                        if (!m_client->m_socket->SendExactQueue(
                                reinterpret_cast<char*>(const_cast<BYTE *>((m_client->m_clipboard.extendedClipboardDataMessage.GetData()))),
                                static_cast<VCard>(m_client->m_clipboard.extendedClipboardDataMessage.GetDataLength())))
                        {
                            m_client->m_socket->Close();
                        }
                    }
                    else
                    {
                        rfbServerCutTextMsg message;
                        std::memset(&message, 0, sizeof(rfbServerCutTextMsg));

                        // Convert wstring to ANSI for wire transmission
                        const wchar_t* cliptext_w = m_client->m_clipboard.m_strLastCutText.c_str();
                        int ansiLen = ::WideCharToMultiByte(CP_ACP, 0, cliptext_w, -1, nullptr, 0, nullptr, nullptr);
                        char* cliptext = new char[static_cast<unsigned long long>(ansiLen)];
                        ::WideCharToMultiByte(CP_ACP, 0, cliptext_w, -1, cliptext, ansiLen, nullptr, nullptr);

                        char* unixtext = new char[static_cast<unsigned long long>(ansiLen)];

                        // Replace CR-LF with LF - never send CR-LF on the wire,
                        // since Unix won't like it
                        int unixpos{ 0 };
                        const std::size_t cliplen = std::strlen(cliptext);
                        for (unsigned int x{ 0U }; x < cliplen; ++x)
                        {
                            if (cliptext[x] != '\x0d')
                            {
                                unixtext[unixpos] = cliptext[x];
                                ++unixpos;
                            }
                        }
                        unixtext[unixpos] = 0;
                        delete[] cliptext;

                        message.length = Swap32IfLE(std::strlen(unixtext));

                        bShouldFlush = true;

                        //adzm 2010-09 - minimize packets. SendExact flushes the queue.
                        if (!m_client->SendRFBMsgQueue(rfbServerCutText, reinterpret_cast<BYTE*>(&message), sizeof(message)))
                        {
                            m_client->m_socket->Close();
                        }
                        if (!m_client->m_socket->SendExactQueue(unixtext, static_cast<VCard>(std::strlen(unixtext))))
                        {
                            m_client->m_socket->Close();
                        }
                        delete[] unixtext;
                    }
                    m_client->m_clipboard.extendedClipboardDataMessage.Reset();
                }

                // adzm - 2010-07 - Extended clipboard
                // notify of any other formats
                if (m_client->m_clipboard.m_bNeedToNotify)
                {
                    m_client->m_clipboard.m_bNeedToNotify = false;
                    if (m_client->m_clipboard.settings.m_bSupportsEx)
                    {
                        int actualLen = m_client->m_clipboard.extendedClipboardDataNotifyMessage.GetDataLength();

                        rfbServerCutTextMsg message;
                        std::memset(&message, 0, sizeof(rfbServerCutTextMsg));
                        message.type = rfbServerCutText;

                        message.length = static_cast<unsigned int>(Swap32IfLE(-actualLen));

                        //adzm 2010-09 - minimize packets. SendExact flushes the queue.Queue

                        bShouldFlush = true;

                        if (!m_client->SendRFBMsgQueue(rfbServerCutText, reinterpret_cast<BYTE*>(&message), sizeof(message)))
                        {
                            m_client->m_socket->Close();
                        }
                        if (!m_client->m_socket->SendExact(
                                reinterpret_cast<char*>(const_cast<BYTE *>((m_client->m_clipboard.extendedClipboardDataNotifyMessage.GetData()))),
                                static_cast<VCard>(m_client->m_clipboard.extendedClipboardDataNotifyMessage.GetDataLength())))
                        {
                            m_client->m_socket->Close();
                        }
                    }
                    m_client->m_clipboard.extendedClipboardDataNotifyMessage.Reset();
                }

                if (bShouldFlush)
                {
                    m_client->m_socket->ClearQueue();
                }
            }

            // SEND AN UPDATE
            // We do this without holding locks, to avoid network problems
            // stalling the server.

            // Update the client palette if necessary

            if (send_palette)
            {
                m_client->SendPalette();
            }

            //add extra check to avoid buffer/encoder sync problems
            if ((m_client->m_encodemgr.m_scrinfo.framebufferHeight ==
                 m_client->m_encodemgr.m_buffer->m_scrinfo.framebufferHeight) &&
                (m_client->m_encodemgr.m_scrinfo.framebufferWidth == m_client->m_encodemgr.m_buffer->m_scrinfo.framebufferWidth) &&
                ((m_client->m_encodemgr.m_scrinfo.format.bitsPerPixel ==
                  m_client->m_encodemgr.m_buffer->m_scrinfo.format.bitsPerPixel) &&
                 m_client->initialCapture_done))
            {
                if (settings->getMaxCpu() == 100)
                {
                    m_client->sendingUpdate = true;
                }
                if (m_client->SendUpdate(update))
                {
                    clipregion.clear();
                    /*#ifdef _DEBUG
                                        static DWORD sNotifyLastCopy1 = GetTickCount();
                                        DWORD now = GetTickCount();;
                                        OutputDevMessage("==================== SendUpdate %4d =======================", now -
                    sNotifyLastCopy1); sNotifyLastCopy1 = now; #endif*/
                }
                m_client->sendingUpdate = false;
            }
            //else
            //clipregion.clear();
        } //end omni_mutex_lock l(m_client->GetUpdateLock(),82);
        yield();
    }

    return nullptr;
}

vncClientThread::~vncClientThread()
{
    m_veNCryptAuth.reset();
    if ((m_client != nullptr) && !m_deleted)
    {
        delete m_client;
    }
#ifdef _Gii
#ifdef _USE_DLL
    if (win8dllHandle)
    {
        ::FreeLibrary(win8dllHandle);
    }
#endif
    if (point_status)
    {
        delete[] point_status;
    }
#endif
}

BOOL vncClientThread::Init(vncClient* client, vncServer* server, VSocket* socket, BOOL auth, BOOL shared)
{
    // Save the server pointer and window handle
    m_deleted = false;
    m_server = server;
    m_socket = socket;
    m_client = client;
    m_auth = auth;
    m_shared = shared;
    m_autoreconnectcounter_quit = false;
    m_client->m_Autoreconnect = m_server->AutoReconnect();
    m_server->AutoReconnect(false);

    m_AutoReconnectPort = m_server->AutoReconnectPort();
    ::strcpy_s(m_szAutoReconnectAdr.data(), m_szAutoReconnectAdr.size(), m_server->AutoReconnectAdr());
    ::strcpy_s(m_szAutoReconnectId.data(), m_szAutoReconnectId.size(), m_server->AutoReconnectId());
#ifdef _Gii
    point_status = nullptr;
    nr_points = 0;
#endif
    // Start the thread
    start();

    return TRUE;
}

BOOL vncClientThread::InitVersion()
{
    bool repeaterkeepaliveloop{ true };
    rfbProtocolVersionMsg protocol_ver{};
    while (repeaterkeepaliveloop)
    {
        // adzm 2010-09
        repeaterkeepaliveloop = false;
        m_major = 0;
        m_minor = 0;

        protocol_ver[12U] = 0;
        if (std::strcmp(m_client->ProtocolVersionMsg, "0.0.0.0") == 0)
        {
            // Generate the server's protocol version
            // RDV 2010-6-10
            rfbProtocolVersionMsg protocolMsg{};
            ::sprintf_s(reinterpret_cast<char*>(protocolMsg), 13, rfbProtocolVersionFormat, rfbProtocolMajorVersion, rfbProtocolMinorVersion);
            // adzm 2010-08
            bool bRetry{ true };
            bool bReady{ false };
            int nRetry{ 0 };
            while (!bReady && bRetry)
            {
                // RDV 2010-6-10
                int Send_OK{ 0 };
                int Recv_OK{ 0 };
                Send_OK = m_socket->SendExact(reinterpret_cast<char*>(&protocolMsg), sz_rfbProtocolVersionMsg);
                if (Send_OK == 1)
                {
                    Recv_OK = m_socket->ReadExact(reinterpret_cast<char*>(&protocol_ver), sz_rfbProtocolVersionMsg);
                }
                // Send our protocol version, and get the client's protocol version
                if (!Send_OK || !Recv_OK)
                {
                    bReady = false;
                    // we need to reconnect!

                    ::Sleep(static_cast<DWORD>(std::min(nRetry * 1000, 30000)));

                    if (TryReconnect())
                    {
                        // reconnect if in SC mode and not already using AutoReconnect
                        bRetry = true;
                        ++nRetry;
                    }
                    else
                    {
                        bRetry = false;
                    }
                }
                else
                {
                    bReady = true;
                }
            }

            if (!bReady)
            {
                return FALSE;
            }
        }
        else
        {
            std::memcpy(protocol_ver, m_client->ProtocolVersionMsg, sz_rfbProtocolVersionMsg);
        }
        //We let the repeater send a invalid sz_rfbProtocolVersionMsg, that start with "REP"
        //Old serverversion return FALSE; and close repeater connection
        //Using an old repeater this doesn't make a difference

        if (std::strncmp(protocol_ver, "REP", 3) == 0)
        {
            vnclog.Print(LL_STATE, VNCLOG("Keepalive received\n"));
            repeaterkeepaliveloop = true;
        }
    }

    // sf@2006 - Trying to fix neverending authentication bug - Check if this is RFB protocol
    if (std::strncmp(protocol_ver, "RFB", 3) != 0)
    {
        return FALSE;
    }

    // Check viewer's the protocol version
    sscanf_s(reinterpret_cast<char*>(&protocol_ver), rfbProtocolVersionFormat, &m_major, &m_minor);
    if (m_major != rfbProtocolMajorVersion)
    {
        return FALSE;
    }

    m_ms_logon = settings->getRequireMSLogon();
    m_client->SetUltraViewer(false);
    if ((m_minor >= 7) && m_socket->IsUsePluginEnabled() && m_server->GetDSMPluginPointer()->IsEnabled() &&
        (m_socket->GetIntegratedPlugin() != nullptr))
    {
        m_socket->SetPluginStreamingIn();
        m_socket->SetPluginStreamingOut();
    }

    return TRUE;
}

#ifdef _Gii
BOOL vncClientThread::InitGiiVersion()
{
    BOOL bReady = FALSE;
    if (true)
    {
        // Generate the server's Gii protocol version
        rfbGIIServerVersionMsg rfbGIIServerVersion{};
        rfbGIIServerVersion.header.messageType = rfbGIIMessage;
        rfbGIIServerVersion.header.subType = rfbGIIVersionMessage; //big endian
        //rfbGIIServerVersion.header.subType = 1;
        rfbGIIServerVersion.header.length = 4;
        rfbGIIServerVersion.header.length = Swap16IfLE(rfbGIIServerVersion.header.length);
        rfbGIIServerVersion.maxVersion = rfbGIIMaxVersion;
        rfbGIIServerVersion.maxVersion = Swap16IfLE(rfbGIIServerVersion.maxVersion);
        rfbGIIServerVersion.minVersion = rfbGIIMinVersion;
        rfbGIIServerVersion.minVersion = Swap16IfLE(rfbGIIServerVersion.minVersion);
        // Send our protocol version, and get the client's protocol version
        if (!m_socket->SendExactQueue(reinterpret_cast<char*>(&rfbGIIServerVersion), sz_rfbGIIServerVersionMsg))
        {
            vnclog.Print(LL_INTINFO, VNCLOG("Write GII Protocol Version failed"));
            bReady = FALSE;
        }
        else
        {
            vnclog.Print(LL_INTINFO, VNCLOG("Write GII Protocol Version"));
            bReady = TRUE;
        }
    }
    else
    {
        vnclog.Print(LL_INTINFO, VNCLOG("Touch Support Disabled -> DOESN'T Write GII Protocol Version"));
    }
    return bReady;
}
#endif

// RDV 2010-4-10
// Ask user Permission Accept/Reject
// Interactive, destop depended
#ifndef SC_20
BOOL vncClientThread::FilterClients_Ask_Permission()
{
    // Verify the peer host name against the AuthHosts string
    vncServer::AcceptQueryReject verified;
    if (m_auth)
    {
        verified = vncServer::aqrAccept;
    }
    else
    {
        verified = m_server->VerifyHost(m_socket->GetPeerName(false));
    }

    // If necessary, query the connection with a timed dialog
    std::array<char, UNLEN + 1> username;
    if (!processHelper::CurrentUser(username.data(), sizeof(username)))
    {
        return false;
    }
    if ((std::strcmp(username.data(), "") != 0) ||
        settings->getQueryIfNoLogon()) // marscha@2006 - Is AcceptDialog required even if no user is logged on
    {
        if (verified == vncServer::aqrQuery)
        {
            // 10 Dec 2008 jdp reject/accept all incoming connections if the workstation is locked
            if (processHelper::IsWSLocked() && !settings->getQueryIfNoLogon())
            {
                if (m_server->QueryAcceptLocked())
                {
                    verified = m_server->QueryAccept() == 1 ? vncServer::aqrAccept : vncServer::aqrReject;
                }
                else
                {
                    //m_queryaccept==2, new method to allow accept on locked user
                    verified = vncServer::aqrAccept;
                }
            }
            else
            {
                vncAcceptDialog* acceptDlg =
                    new vncAcceptDialog(settings->getQueryTimeout(), m_server->QueryAccept() == 1, m_socket->GetPeerName(true),
                                        m_client->infoMsg, settings->getNotification(), m_client->infoMsgIsUtf8);
                if (acceptDlg == nullptr)
                {
                    if (m_server->QueryAccept() == 1)
                    {
                        verified = vncServer::aqrAccept;
                    }
                    else
                    {
                        verified = vncServer::aqrReject;
                    }
                }
                else if (!(acceptDlg->DoDialog()))
                {
                    verified = vncServer::aqrReject;
                }
            }
        }
    }

    if (verified == vncServer::aqrReject)
    {
        return FALSE;
    }
    return TRUE;
}
#endif // SC_20

// RDV 2010-4-10
// Filter Blacklisted are refused connection
// Not interactive, not destop depended
BOOL vncClientThread::FilterClients_Blacklist()
{
    // Verify the peer host name against the AuthHosts string
    vncServer::AcceptQueryReject verified;
    if (m_auth)
    {
        verified = vncServer::aqrAccept;
    }
    else
    {
        verified = m_server->VerifyHost(m_socket->GetPeerName(false));
    }

    if (verified == vncServer::aqrReject)
    {
        return FALSE;
    }
    return TRUE;
}

BOOL vncClientThread::CheckEmptyPasswd()
{
    vncPasswd::ToText plain(settings->getPasswd(), settings->getSecure());
    // By default we disallow passwordless workstations!
    if ((std::strlen(plain) == 0) && settings->getAuthRequired())
    {
        if (m_client->m_IsLoopback && settings->getAllowLoopbackWithoutAuth())
        {
            vnclog.Print(LL_LOGSCREEN, "Loopback connection: no password required due to AllowLoopbackWithoutAuth");
            vnclog.Print(LL_CLIENTS,
                         VNCLOG("loopback connection: allowing empty password because AllowLoopbackWithoutAuth is set\n"));
            return TRUE;
        }
        vnclog.Print(LL_CONNERR, VNCLOG("no password specified for server - client rejected\n"));
        SendConnFailed("This server does not have a valid password enabled."
                       "Until a password is set, incoming connections cannot be accepted.");
        return FALSE;
    }
    return TRUE;
}

BOOL vncClientThread::CheckLoopBack()
{
    // By default we filter out local loop connections, because they're pointless
    if (!settings->getAllowLoopback())
    {
        char* localname = _strdup(m_socket->GetSockName());
        char* remotename = _strdup(m_socket->GetPeerName(false));

        // Check that the local & remote names are different!
        if ((localname != nullptr) && (remotename != nullptr))
        {
            BOOL ok = ::_stricmp(localname, remotename) != 0;

            if (localname != nullptr)
            {
                std::free(localname);
            }

            if (remotename != nullptr)
            {
                std::free(remotename);
            }

            if (!ok)
            {
                SendConnFailed("Local loop-back connections are disabled.");
                return FALSE;
            }
        }
    }
    else
    {
        char* localname = _strdup(m_socket->GetSockName());
        char* remotename = _strdup(m_socket->GetPeerName(false));

        // Check that the local & remote names are different!
        if ((localname != nullptr) && (remotename != nullptr))
        {
            BOOL ok = ::_stricmp(localname, remotename) != 0;

            if (localname != nullptr)
            {
                std::free(localname);
            }

            if (remotename != nullptr)
            {
                std::free(remotename);
            }

            if (!ok)
            {
                m_client->m_IsLoopback = true;
            }
        }
    }
    return TRUE;
}

//WARNING  USING THIS FUNCTION AT A WRONG PLACE
//SEND  0 ---> rfbVncAuthOK
void vncClientThread::SendConnFailed(const char* const szMessage)
{
    //adzm 2010-09 - minimize packets. SendExact flushes the queue.
    if (m_minor >= 7)
    {
        // 0 = Failure
        CARD8 value = 0;
        if (!m_socket->SendExactQueue(reinterpret_cast<char*>(&value), sizeof(value)))
        {
            return;
        }
    }
    else
    {
        CARD32 auth_val = Swap32IfLE(rfbConnFailed);
        if (!m_socket->SendExactQueue(reinterpret_cast<char*>(&auth_val), sizeof(auth_val)))
        {
            return;
        }
    }
    CARD32 errlen = Swap32IfLE(std::strlen(szMessage));
    if (!m_socket->SendExactQueue(reinterpret_cast<char*>(&errlen), sizeof(errlen)))
    {
        return;
    }
    m_socket->SendExact(szMessage, static_cast<const VCard>(std::strlen(szMessage)));
}

void vncClientThread::LogAuthResult(const bool success, const bool isconnected)
{
#ifndef SC_20
    if (!success)
    {
        vnclog.Print(LL_CONNERR, VNCLOG("authentication failed\n"));
        typedef BOOL (*LogeventFn)(char* machine, char* szMslogonLog);
        LogeventFn Logevent = nullptr;
        std::array<char, MAX_PATH> szCurrentDir;
        ::strcpy_s(szCurrentDir.data(), szCurrentDir.size(), winvncFolder.data());
        ::strcat_s(szCurrentDir.data(), szCurrentDir.size(), "\\logging.dll");
        HMODULE hModule = ::LoadLibrary(szCurrentDir.data());
        if (hModule)
        {
            Logevent = reinterpret_cast<LogeventFn>(::GetProcAddress(hModule, "LOGFAILEDV2"));
            Logevent(reinterpret_cast<char*>(const_cast<char *>(m_client->GetClientNameName())), settings->getLogFile());
            ::FreeLibrary(hModule);
        }
    }
    else
    {
        typedef BOOL (*LogeventFn)(char* machine, int clientId, bool isinteractive, char* szMsLogonLog);
        LogeventFn Logevent = nullptr;
        std::array<char, MAX_PATH> szCurrentDir;
        ::strcpy_s(szCurrentDir.data(), szCurrentDir.size(), winvncFolder.data());
        ::strcat_s(szCurrentDir.data(), szCurrentDir.size(), "\\logging.dll");
        HMODULE hModule = ::LoadLibrary(szCurrentDir.data());
        if (hModule)
        {
            if (!isconnected)
            {
                Logevent = reinterpret_cast<LogeventFn>(::GetProcAddress(hModule, "LOGCONNV2"));
            }
            else
            {
                Logevent = reinterpret_cast<LogeventFn>(::GetProcAddress(hModule, "LOGLOGONV2"));
            }
            Logevent(reinterpret_cast<char*>(const_cast<char *>(m_client->GetClientNameName())), m_client->GetClientId(),
                     (m_client->m_keyboardenabled && m_client->m_pointerenabled), settings->getLogFile());
            ::FreeLibrary(hModule);
        }
    }
#endif // SC_20
}

BOOL vncClientThread::InitAuthenticate()
{
    vnclog.Print(LL_INTINFO, "Entered InitAuthenticate\n");
    // RDV 2010-4-10
    // Split Filter in desktop in/depended
    if (!FilterClients_Blacklist())
    {
        vnclog.Print(LL_LOGSCREEN, "Blacklisten: connection has been rejected");
        SendConnFailed("Your connection has been rejected to many attempts.");
        return FALSE;
    }
    if (!CheckEmptyPasswd())
    {
        return FALSE;
    }
    if (!CheckLoopBack())
    {
        return FALSE;
    }

    //adzm 2010-09 - Do the actual authentication
    if (m_minor >= 7)
    {
        std::vector<CARD8> current_auth;
        if (!AuthenticateClient(current_auth, false))
        {
            return FALSE;
        }
    }
    else
    {
        // RDV 2010-4-10
#ifndef SC_20
        if (!FilterClients_Ask_Permission())
        {
            SendConnFailed("Your connection has been rejected.");
            return FALSE;
        }
#endif // SC_20
        if (!AuthenticateLegacyClient(true))
        {
            return FALSE;
        }
    }

    if (m_server->getNumberViewers() > settings->getMaxViewers() - 1)
    {
        if (settings->getMaxViewerSetting() == 0)
        {
            SendConnFailed("Max viewers reached");
            return FALSE;
        }
        else
        {
            m_server->AddAuthHostsBlacklist(m_client->GetClientNameAddress());
            m_server->AddAuthHostsBlacklist(m_client->GetClientNameAddress());
            m_server->AddAuthHostsBlacklist(m_client->GetClientNameAddress());
            m_server->AddAuthHostsBlacklist(m_client->GetClientNameAddress());
            m_server->AddAuthHostsBlacklist(m_client->GetClientNameAddress());
            m_server->AddAuthHostsBlacklist(m_client->GetClientNameAddress());

            m_server->GetClient(m_server->getOldestViewer())->Kill();
            m_client->forceBlacklist = true;
        }
    }

    // Read the client's initialisation message
    rfbClientInitMsg client_ini{};
    if (!m_socket->ReadExact(reinterpret_cast<char*>(&client_ini), sz_rfbClientInitMsg))
    {
        return FALSE;
    }

    if (client_ini.flags & clientInitExtraMsgSupport)
    {
        rfbClientInitExtraMsg msg{};
        if (!m_socket->ReadExact(reinterpret_cast<char*>(&msg), sz_rfbClientInitExtraMsg))
        {
            return FALSE;
        }
        if ((msg.textLength < 0) || (msg.textLength > 254))
        {
            return FALSE;
        }

        if (client_ini.flags & clientInitExtraMsgUnicode)
        {
            // Client sends UTF-16LE Unicode directly
            std::array<wchar_t, 128U> infoMsgW;
            if (!m_socket->ReadExact(reinterpret_cast<char*>(infoMsgW.data()), msg.textLength))
            {
                return FALSE;
            }
            infoMsgW[msg.textLength / sizeof(wchar_t)] = L'\0';
            // Convert to UTF-8 for storage in m_client->infoMsg
            ::WideCharToMultiByte(CP_UTF8, 0, infoMsgW.data(), -1, m_client->infoMsg, 255, nullptr, nullptr);
            m_client->infoMsgIsUtf8 = true;
        }
        else
        {
            // Legacy: client sends UTF-8 (or CP_ACP for very old clients)
            if (!m_socket->ReadExact(m_client->infoMsg, msg.textLength))
            {
                return FALSE;
            }
            m_client->infoMsg[msg.textLength] = '\0';
            // Auto-detect UTF-8 vs. CP_ACP so the new server displays both correctly.
            if (::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, m_client->infoMsg, -1, nullptr, 0) > 0)
            {
                m_client->infoMsgIsUtf8 = true;
            }
            else
            {
                m_client->infoMsgIsUtf8 = false;
            }
        }
    }

#ifndef SC_20
    // Check the FilterClients thing after final auth
    if (std::strlen(m_client->infoMsg) > 0)
    {
        typedef BOOL (*LogeventFn)(char* info, char* szMslogonLog);
        LogeventFn Logevent = nullptr;
        std::array<char, MAX_PATH> szCurrentDir;
        ::strcpy_s(szCurrentDir.data(), szCurrentDir.size(), winvncFolder.data());
        ::strcat_s(szCurrentDir.data(), szCurrentDir.size(), "\\logging.dll");
        HMODULE hModule = ::LoadLibrary(szCurrentDir.data());
        if (hModule)
        {
            Logevent = reinterpret_cast<LogeventFn>(::GetProcAddress(hModule, "LOGEXTRAINFOV2"));

            if (Logevent)
            {
                Logevent(reinterpret_cast<char*>(m_client->infoMsg), settings->getLogFile());
            }
            ::FreeLibrary(hModule);
        }
    }
    if ((m_minor >= 7) && !FilterClients_Ask_Permission())
    {
        vnclog.Print(LL_CLIENTS, VNCLOG("Your connection has been rejected.\n"));
        return FALSE;
    }
#endif // SC_20

    // If the client wishes to have exclusive access then remove other clients
    if ((settings->getConnectPriority() == 3) && !m_shared)
    {
        // Existing
        if (m_server->AuthClientCount() > 0)
        {
            vnclog.Print(LL_CLIENTS, VNCLOG("connections already exist - client rejected\n"));
            return FALSE;
        }
    }
    // adzm 2010-09
    if (!(client_ini.flags & clientInitShared) && !m_shared)
    {
        // Which client takes priority, existing or incoming?
        if (settings->getConnectPriority() < 1)
        {
            // Incoming
            vnclog.Print(LL_INTINFO, VNCLOG("non-shared connection - disconnecting old clients\n"));
            m_server->KillAuthClients();
        }
        else if (settings->getConnectPriority() > 1)
        {
            // Existing
            if (m_server->AuthClientCount() > 0)
            {
                vnclog.Print(LL_CLIENTS, VNCLOG("connections already exist - client rejected\n"));
                return FALSE;
            }
        }
    }

    // Tell the server that this client is ok
    return m_server->Authenticated(m_client->GetClientId());
}

BOOL vncClientThread::AuthenticateClient(std::vector<CARD8>& current_auth, const bool connected)
{
    // adzm 2010-09 - Gather all authentication types we support
    std::vector<CARD8> auth_types;

    const bool bUseSessionSelect{ false };

    // obviously needs to be one that we suggested in the first place
    bool bSecureVNCPluginActive =
        std::find(current_auth.begin(), current_auth.end(), rfbUltraVNC_SecureVNCPluginAuth_new) != current_auth.end();
    bool bSCPromptActive = std::find(current_auth.begin(), current_auth.end(), rfbUltraVNC_SCPrompt) != current_auth.end();
    bool bSessionSelectActive =
        std::find(current_auth.begin(), current_auth.end(), rfbUltraVNC_SessionSelect) != current_auth.end();

    if (current_auth.empty())
    {
        // send the UltraVNC auth type to identify ourselves as an UltraVNC Server, but only initially
        auth_types.push_back(rfbUltraVNC);
        auth_types.push_back(rfbClientInitExtraMsgSupportNew);
    }

    // encryption takes priority over everything, for now at least.
    // would be useful to have a host list to configure these settings.
    // Include the SecureVNCPluginAuth type for those that support it but are not UltraVNC Viewers
    // Secure authentication is fail-closed by default. Legacy top-level
    // authentication is advertised only when the administrator explicitly
    // allows it, or when no secure type (VeNCrypt/RA2/RA2ne) is enabled at all.
    const bool bSecureOnly = (settings->getEnableVeNCryptTLS() || settings->getEnableRA2() || settings->getEnableRA2ne()) &&
                             !settings->getVeNCryptAllowLegacyAuth();

    if (bSecureOnly)
    {
        if (settings->getEnableVeNCryptTLS())
        {
            auth_types.push_back(rfbVeNCypt);
        }
    }
    else if (!bSecureVNCPluginActive && m_socket->IsUsePluginEnabled() && m_server->GetDSMPluginPointer()->IsEnabled() &&
             (m_socket->GetIntegratedPlugin() != nullptr))
    {
        auth_types.push_back(rfbUltraVNC_SecureVNCPluginAuth_new);
        auth_types.push_back(rfbUltraVNC_SecureVNCPluginAuth);
    }
    else if ((settings->getScPrompt() || settings->getScExit()) && !bSCPromptActive)
    {
        // adzm 2010-10 - Add the SCPrompt pseudo-auth
        auth_types.push_back(rfbUltraVNC_SCPrompt);
    }
    else if (bUseSessionSelect)
    {
        // adzm 2010-10 - Add the SessionSelect pseudo-auth
        auth_types.push_back(rfbUltraVNC_SessionSelect);
    }
    else
    {
        vncPasswd::ToText plain(settings->getPasswd(), settings->getSecure());

        if (m_client->m_IsLoopback && settings->getAllowLoopbackWithoutAuth())
        {
            auth_types.push_back(rfbNoAuth);
            vnclog.Print(LL_LOGSCREEN, "Loopback connection: auth disabled by AllowLoopbackWithoutAuth");
            vnclog.Print(LL_CLIENTS,
                         VNCLOG("loopback connection: advertising rfbNoAuth because AllowLoopbackWithoutAuth is set\n"));
        }
        else if (!m_auth && m_ms_logon)
        {
            // Advertise only MS-Logon IV and III. MS-Logon II (31-bit DH with a
            // time-seeded rand()) is retired: old viewers that still select it
            // are handled by the reject path below so they get a proper
            // upgrade message instead of a silent disconnect.
            auth_types.push_back(rfbUltraVNC_MsLogonIVAuth);
            auth_types.push_back(rfbUltraVNC_MsLogonIIIAuth);
        }
        else
        {
            if (m_auth || (std::strlen(plain) == 0))
            {
                auth_types.push_back(rfbNoAuth);
            }
            else
            {
                auth_types.push_back(rfbVncAuth);
            }
        }
    }

    // VeNCrypt TLS is offered in addition to the normal auth types, so viewers
    // that don't select it can still connect with mslogon/plain VNC auth.
    if (settings->getEnableVeNCryptTLS() && !bSecureOnly)
    {
        auth_types.push_back(rfbVeNCypt);
    }

    // RA2/RA2ne (RealVNC RSA-AES types 5/6/129/130) reuse the VeNCrypt
    // certificate RSA key and are offered alongside the other auth types
    // when enabled. 256-bit variants first; viewers pick by preference.
    if (settings->getEnableRA2())
    {
        auth_types.push_back(rfbRSAAES_256);
    }
    if (settings->getEnableRA2ne())
    {
        auth_types.push_back(rfbRSAAESne_256);
    }
    if (settings->getEnableRA2())
    {
        auth_types.push_back(rfbRSAAES);
    }
    if (settings->getEnableRA2ne())
    {
        auth_types.push_back(rfbRSAAESne);
    }

    // adzm 2010-09 - Send the auths
    {
        CARD8 authCount = static_cast<CARD8>(auth_types.size());
        if (!m_socket->SendExactQueue(reinterpret_cast<const char*>(&authCount), sizeof(authCount)))
        {
            return FALSE;
        }

        for (std::vector<CARD8>::iterator auth_it = auth_types.begin(); auth_it != auth_types.end(); ++auth_it)
        {
            CARD8 authType = *auth_it;
            if (!m_socket->SendExactQueue(reinterpret_cast<const char*>(&authType), sizeof(authType)))
            {
                return FALSE;
            }
        }
        if (!m_socket->ClearQueue())
        {
            return FALSE;
        }
    }
    // read the accepted auth type
    CARD8 auth_accepted = rfbInvalidAuth;
    std::string auth_message;
    if (!m_socket->ReadExact(reinterpret_cast<char*>(&auth_accepted), sizeof(auth_accepted)))
    {
        return FALSE;
    }

    // obviously needs to be one that we suggested in the first place
    // BUT: MS-Logon II (0x71) is no longer advertised. If an old viewer still
    // selects it, accept the selection so we can send a proper error message
    // instead of just closing the connection.
    vnclog.Print(LL_INTINFO, "Client selected auth type: %d\n", static_cast<int>(auth_accepted));

    bool auth_type_valid = std::find(auth_types.begin(), auth_types.end(), auth_accepted) != auth_types.end();
    bool is_mslogonii_fallback = (m_ms_logon && (auth_accepted == rfbUltraVNC_MsLogonIIAuth));

    if (!auth_type_valid && !is_mslogonii_fallback)
    {
        return FALSE;
    }

    // MS-Logon I never seems to be used anymore -- the old code would say if (m_ms_logon) AuthMsLogon (II) else AuthVnc
    // and within AuthVnc would be if (m_ms_logon) { /* MS-Logon code */ }. THat could never be hit since the first case
    // would always match!

    // Authenticate the connection, if required
    BOOL auth_success = FALSE;
    BOOL auth_is_mslogon = FALSE;
    BOOL version_warning = FALSE;
    switch (auth_accepted)
    {
    case rfbUltraVNC:
        m_client->SetUltraViewer(true);
        vnclog.Print(LL_LOGSCREEN, "UltraVNC Viewer");
        auth_success = true;
        break;
    case rfbVeNCypt:
        auth_success = AuthVeNCrypt(auth_message);
        if (auth_success && m_veNCryptAuth && (m_veNCryptAuth->GetChosenType() == secTypeX509MsLogonIII))
        {
            auth_is_mslogon = TRUE;
        }
        vnclog.Print(LL_LOGSCREEN, "VeNCrypt auth success = %d", auth_success);
        break;
    case rfbUltraVNC_SecureVNCPluginAuth_new:
        auth_success = AuthSecureVNCPlugin(auth_message);
        vnclog.Print(LL_LOGSCREEN, "AuthSecureVNCPlugin success = %d", auth_success);
        break;
    case rfbUltraVNC_SecureVNCPluginAuth:
        auth_success = AuthSecureVNCPlugin_old(auth_message);
        auth_success = 0;
        version_warning = 1;
        break;
    case rfbUltraVNC_MsLogonIVAuth:
        auth_success = AuthMsLogonIV(auth_message);
        vnclog.Print(LL_LOGSCREEN, "MsLogonIV success = %d", auth_success);
        if (auth_success)
        {
            auth_is_mslogon = TRUE;
        }
        break;
    case rfbUltraVNC_MsLogonIIIAuth:
        auth_success = AuthMsLogonIII(auth_message);
        vnclog.Print(LL_LOGSCREEN, "MsLogonIII success = %d", auth_success);
        if (auth_success)
        {
            auth_is_mslogon = TRUE;
        }
        break;
    case rfbUltraVNC_MsLogonIIAuth:
        if (is_mslogonii_fallback)
        {
            // Old viewer tried MS-Logon II, which is retired (broken 31-bit
            // DH). Do the handshake so the protocol stays in sync, then
            // reject with an upgrade message.
            auth_success = AuthMsLogonReject(auth_message);
            vnclog.Print(LL_LOGSCREEN, "MsLogonII rejected (legacy auth retired) = %d", auth_success);
            break;
        }
        // MS-Logon II selected without MS-Logon enabled: reject.
        auth_success = FALSE;
        auth_message = "MS-Logon II is not supported by this server. Please upgrade your viewer.";
        break;
    case rfbVncAuth:
        auth_success = AuthVnc(auth_message);
        vnclog.Print(LL_LOGSCREEN, "Vnc password success = %d", auth_success);
        break;
    case rfbRSAAES:
    case rfbRSAAES_256:
    case rfbRSAAESne:
    case rfbRSAAESne_256:
        auth_success = AuthRSAAES(auth_accepted, auth_message);
        vnclog.Print(LL_LOGSCREEN, "RSA-AES auth success = %d", auth_success);
        break;
    case rfbNoAuth:
        auth_success = TRUE;
        break;
    case rfbUltraVNC_SCPrompt:
        // adzm 2010-10 - Do the SCPrompt auth
        auth_success = AuthSCPrompt(auth_message);
        break;
    case rfbUltraVNC_SessionSelect:
        // adzm 2010-10 - Do the SessionSelect auth
        auth_success = AuthSessionSelect(auth_message);
        break;
    default:
        auth_success = FALSE;
        break;
    }
    // Log authentication success or failure
    if (!auth_is_mslogon)
    {
        LogAuthResult(auth_success ? true : false, connected);
    }

    // Return the result
    CARD32 auth_result = rfbVncAuthFailed;
    if (auth_success == 1)
    {
        current_auth.push_back(auth_accepted);

        // continue the authentication if MS-Logon is enabled. any method of authentication should
        // work out fine with this method. Currently we limit ourselves to only one layer beyond
        // the plugin to avoid deep recursion, but that can easily be changed if necessary.
        if (m_ms_logon && (auth_accepted == rfbUltraVNC_SecureVNCPluginAuth_new) && m_socket->GetIntegratedPlugin())
        {
            auth_result = rfbVncAuthContinue;
        }
        else if (auth_accepted == rfbUltraVNC)
        {
            auth_result = rfbVncAuthContinue;
        }
        else if (auth_accepted == rfbUltraVNC_MsLogonIIIAuth)
        {
            // MS-Logon III successful - auth complete
            auth_result = rfbVncAuthOK;
            vnclog.Print(LL_LOGSCREEN, "MS-Logon III auth completed successfully\n");
        }
        else if (auth_accepted == rfbUltraVNC_MsLogonIVAuth)
        {
            // MS-Logon IV successful - auth complete
            auth_result = rfbVncAuthOK;
            vnclog.Print(LL_LOGSCREEN, "MS-Logon IV auth completed successfully\n");
        }
        else if ((settings->getScPrompt() || settings->getScExit()) && !bSCPromptActive)
        {
            auth_result = rfbVncAuthContinue;
        }
        else if (bUseSessionSelect && !bSessionSelectActive)
        {
            auth_result = rfbVncAuthContinue;
        }
        else
        {
            auth_result = rfbVncAuthOK;
        }
    }

    // RDV 2010-4-10
    // This is a good spot for asking the user permission Accept/Reject
    // Only when auth_result == rfbVncAuthOK, in all other cases it isn't needed
    // adzm 2010-10 - This was causing failure with DSM plugin, since this is
    // pretty much the same as another auth type we'll just move it into that code
    // instead. So see the AuthSCPrompt function.

    CARD32 auth_result_msg = Swap32IfLE(auth_result);
    if (!m_socket->SendExactQueue(reinterpret_cast<char*>(&auth_result_msg), sizeof(auth_result_msg)))
    {
        return FALSE;
    }

    // Subtype 300 carries a versioned request flag for session-token issuance.
    // Always send its response after SecurityResult so the viewer never blocks;
    // unsupported issuance is encoded as an empty response.
    if (auth_success && (auth_accepted == rfbVeNCypt) && m_veNCryptAuth && (m_veNCryptAuth->GetChosenType() == secTypeTokenTLS) &&
        m_veNCryptAuth->ShouldSendTokenResponse())
    {
        std::string sessionMsg;
        m_veNCryptAuth->IssueAndSendSessionToken(sessionMsg);
        if (!sessionMsg.empty())
        {
            vnclog.Print(LL_CONNERR, VNCLOG("VeNCrypt: session token issue failed: %s\n"), sessionMsg.c_str());
        }
    }

    //adzm 2010-09 - Set handshake complete if integrated plugin finished auth
    /*if ((auth_success || auth_accepted == rfbUltraVNC_SecureVNCPluginAuth) && (auth_accepted ==
    rfbUltraVNC_SecureVNCPluginAuth_new || auth_accepted == rfbUltraVNC_SecureVNCPluginAuth) && m_socket->GetIntegratedPlugin()) {
        m_socket->GetIntegratedPlugin()->SetHandshakeComplete();
    }*/
    if ((auth_success || (auth_accepted == rfbUltraVNC_SecureVNCPluginAuth)) &&
        (auth_accepted == rfbUltraVNC_SecureVNCPluginAuth) && m_socket->GetIntegratedPlugin())
    {
        m_socket->GetIntegratedPlugin()->SetHandshakeComplete();
    }

    // Send a failure reason
    if (!auth_success && !version_warning)
    {
        if (auth_message.empty())
        {
            vnclog.Print(LL_LOGSCREEN, "Authentication failed");
            auth_message = "authentication rejected";
        }
        CARD32 auth_message_length = Swap32IfLE(auth_message.length());
        if (!m_socket->SendExactQueue(reinterpret_cast<char*>(&auth_message_length), sizeof(auth_message_length)))
        {
            return FALSE;
        }
        if (!m_socket->SendExact(auth_message.c_str(), static_cast<VCard>(auth_message.length())))
        {
            return FALSE;
        }
        return FALSE;
    }

    else if (!auth_success)
    {
        if (auth_message.empty())
        {
            auth_message = "Server require a higher viewer version.";
        }
        CARD32 auth_message_length = Swap32IfLE(auth_message.length());
        if (!m_socket->SendExactQueue(reinterpret_cast<char*>(&auth_message_length), sizeof(auth_message_length)))
        {
            return FALSE;
        }
        if (!m_socket->SendExact(auth_message.c_str(), static_cast<VCard>(auth_message.length())))
        {
            return FALSE;
        }
        return FALSE;
    }

    if (!m_socket->ClearQueue())
    {
        return FALSE;
    }

    if (auth_success && (auth_result == rfbVncAuthContinue))
    {
        if (!AuthenticateClient(current_auth, true))
        {
            return FALSE;
        }
    }

    if (auth_success)
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

BOOL vncClientThread::AuthenticateLegacyClient([[maybe_unused]] const bool isconnected)
{
    vncPasswd::ToText plain(settings->getPasswd(), settings->getSecure());

    CARD32 auth_type = rfbInvalidAuth;

    // DSM plugin auth takes priority: if the plugin is actually active on this socket,
    // its handshake is what keys the transform, so it must not be skipped even for
    // loopback connections with AllowLoopbackWithoutAuth set. Only fall back to the
    // loopback bypass when DSM is not in use for this connection (e.g. DSM disabled
    // globally, or AllowLoopbackWithoutDSM already left the plugin disabled above).
    if (m_socket->IsUsePluginEnabled() && m_server->GetDSMPluginPointer()->IsEnabled() &&
        (m_socket->GetIntegratedPlugin() != nullptr))
    {
        auth_type = rfbLegacy_SecureVNCPlugin;
    }
    else if (m_client->m_IsLoopback && settings->getAllowLoopbackWithoutAuth())
    {
        auth_type = rfbNoAuth;
        vnclog.Print(LL_LOGSCREEN, "Loopback connection: legacy auth disabled by AllowLoopbackWithoutAuth");
        vnclog.Print(LL_CLIENTS, VNCLOG("loopback connection: forcing rfbNoAuth because AllowLoopbackWithoutAuth is set\n"));
    }
    else if (m_ms_logon)
    {
        auth_type = rfbLegacy_MsLogon;
    }
    else if (std::strlen(plain) > 0)
    {
        auth_type = rfbVncAuth;
    }
    else if (m_auth || (std::strlen(plain) == 0))
    {
        auth_type = rfbNoAuth;
    }

    // abort if invalid
    if (auth_type == rfbInvalidAuth)
    {
        SendConnFailed("unable to determine legacy authentication method");
        return FALSE;
    }

    // RDV 2010-4-10
    // CARD32, byte swap is needed
    auth_type = Swap32IfLE(auth_type);
    // adzm 2010-09 - Send the single auth type
    if (!m_socket->SendExact(reinterpret_cast<const char*>(&auth_type), sizeof(auth_type)))
    {
        return FALSE;
    }
    // RDV 2010-4-10
    // CARD32, reset original
    auth_type = Swap32IfLE(auth_type);

    // Authenticate the connection, if required
    BOOL auth_success = FALSE;
    BOOL auth_is_mslogon = FALSE;

    std::string auth_message;
    switch (auth_type)
    {
    case rfbLegacy_SecureVNCPlugin:
        auth_success = AuthSecureVNCPlugin(auth_message);
        // adzm 2010-11 - Legacy 1.0.8.2 special build will continue here with MS-Logon
        if (auth_success && m_ms_logon)
        {
            CARD32 auth_result_msg = Swap32IfLE(rfbLegacy_MsLogon);
            if (!m_socket->SendExact(reinterpret_cast<char*>(&auth_result_msg), sizeof(auth_result_msg)))
            {
                return FALSE;
            }

            //adzm 2010-09 - Set handshake complete if integrated plugin finished auth
            if (auth_success && (auth_type == rfbLegacy_SecureVNCPlugin) && m_socket->GetIntegratedPlugin())
            {
                m_socket->GetIntegratedPlugin()->SetHandshakeComplete();
            }

            auth_type = rfbLegacy_MsLogon;
            auth_success = AuthMsLogon(auth_message);
        }
        break;
    case rfbLegacy_MsLogon:
        // Legacy viewers can only do MS-Logon II, whose 31-bit DH is broken.
        // Complete the handshake so the protocol stays in sync, then reject
        // with an upgrade message. (The SecureVNCPlugin path above still runs
        // AuthMsLogon inside the plugin's encrypted channel, where the weak
        // DH exchange is not exposed.)
        auth_success = AuthMsLogonReject(auth_message);
        break;
    case rfbVncAuth:
        auth_success = AuthVnc(auth_message);
        break;
    case rfbNoAuth:
        auth_success = TRUE;
        break;
    default:
        auth_success = FALSE;
        break;
    }

    // Log authentication success or failure
    if (!auth_is_mslogon)
    {
        LogAuthResult(auth_success ? true : false, false);
    }

    // Return the result
    CARD32 auth_result = rfbVncAuthFailed;
    if (auth_success)
    {
        auth_result = rfbVncAuthOK;
    }
    //proto 6.2.1 np securityresult for none 3.3 and 3.7
    if (auth_type != rfbNoAuth)
    {
        CARD32 auth_result_msg = Swap32IfLE(auth_result);
        if (!m_socket->SendExact(reinterpret_cast<char*>(&auth_result_msg), sizeof(auth_result_msg)))
        {
            return FALSE;
        }
    }

    //adzm 2010-09 - Set handshake complete if integrated plugin finished auth
    if (auth_success && (auth_type == rfbLegacy_SecureVNCPlugin) && m_socket->GetIntegratedPlugin())
    {
        m_socket->GetIntegratedPlugin()->SetHandshakeComplete();
    }

    if (auth_success)
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

// must SetHandshakeComplete after sending auth result!
BOOL vncClientThread::AuthSecureVNCPlugin(std::string& auth_message)
{
    bool bPassphrase{ false };
    vncPasswd::ToText plain(settings->getPasswd(), settings->getSecure());
    vncPasswd::ToText plainViewOnly(settings->getPasswdViewOnly(), settings->getSecure());
    ConfigHelper ConfigHelpervar(settings->getDSMPluginConfig());
    if (std::strlen(ConfigHelpervar.m_szPassphrase) > 0)
    {
        bPassphrase = true;
        ConfigHelpervar.SetConfigHelper(ConfigHelpervar.m_dwFlags, "");
        m_socket->SetDSMPluginConfig(ConfigHelpervar.m_szConfig);
    }
    else
    {
        m_socket->SetDSMPluginConfig(settings->getDSMPluginConfig());
    }

    BOOL auth_ok = FALSE;
    BOOL plugin_auth_ok = FALSE;

    const char* plainPassword = plain;
    const char* plainPasswordViewOnly = plainViewOnly;
    /*if (!m_ms_logon && plainPassword && strlen(plainPassword) > 0) {
        m_socket->GetIntegratedPlugin()->SetPasswordData((const BYTE*)plainPassword, strlen(plainPassword));
    }*/

    int nSequenceNumber{ 0 };
    bool bSendChallenge{ true };
    do
    {
        BYTE* pChallenge{ nullptr };
        int nChallengeLength{ 0 };
        if (!m_socket->GetIntegratedPlugin()->GetChallenge(pChallenge, nChallengeLength, nSequenceNumber))
        {
            m_socket->GetIntegratedPlugin()->FreeMemory(pChallenge);
            auth_message = m_socket->GetIntegratedPlugin()->GetLastErrorString();
            return FALSE;
        }

        WORD wChallengeLength = static_cast<WORD>(nChallengeLength);

        if (!m_socket->SendExactQueue(reinterpret_cast<const char*>(&wChallengeLength), sizeof(wChallengeLength)))
        {
            m_socket->GetIntegratedPlugin()->FreeMemory(pChallenge);
            return FALSE;
        }

        if (!m_socket->SendExact(reinterpret_cast<const char*>(pChallenge), static_cast<VCard>(nChallengeLength)))
        {
            m_socket->GetIntegratedPlugin()->FreeMemory(pChallenge);
            return FALSE;
        }

        char passphraseused = bPassphrase;
        if (m_ms_logon || (std::strlen(plainPassword) == 0))
        {
            passphraseused = 2;
        }
        if (!m_socket->SendExact(reinterpret_cast<const char*>(&passphraseused), 1))
        {
            m_socket->GetIntegratedPlugin()->FreeMemory(pChallenge);
            return FALSE;
        }

        m_socket->GetIntegratedPlugin()->FreeMemory(pChallenge);
        WORD wResponseLength{ 0U };
        if (!m_socket->ReadExact(reinterpret_cast<char*>(&wResponseLength), sizeof(wResponseLength)))
        {
            return FALSE;
        }
        if (wResponseLength > 2024)
        {
            return FALSE;
        }
        BYTE* pResponseData = new BYTE[wResponseLength];
        ::ZeroMemory(pResponseData, wResponseLength);
        if (!m_socket->ReadExact(reinterpret_cast<char*>(pResponseData), wResponseLength))
        {
            delete[] pResponseData;
            return FALSE;
        }

        if (!m_socket->GetIntegratedPlugin()->HandleResponse(pResponseData, static_cast<int>(wResponseLength), nSequenceNumber,
                                                             bSendChallenge))
        {
            auth_message = m_socket->GetIntegratedPlugin()->GetLastErrorString();
            auth_ok = FALSE;
            plugin_auth_ok = FALSE;
            bSendChallenge = false;
        }
        else if (!bSendChallenge)
        {
            auth_ok = TRUE;
            plugin_auth_ok = TRUE;
        }

        delete[] pResponseData;

        m_socket->GetIntegratedPlugin()->SetHandshakeComplete();

        if (!m_ms_logon && ((std::strlen(plainPassword) != 0) || (std::strlen(plainPasswordViewOnly) != 0)))
        {
            if (!m_socket->ReadExact(reinterpret_cast<char*>(&wResponseLength), sizeof(wResponseLength)))
            {
                return FALSE;
            }
            if (wResponseLength > 2024)
            {
                return FALSE;
            }
            pResponseData = new BYTE[wResponseLength];

            if (!m_socket->ReadExact(reinterpret_cast<char*>(pResponseData), wResponseLength))
            {
                delete[] pResponseData;
                return FALSE;
            }
            if (bPassphrase == false)
            {
                std::size_t plainLen = std::strlen(plain);
                if ((plainLen == 0) || (wResponseLength < plainLen) || (sodium_memcmp(plain, pResponseData, plainLen) != 0))
                {
                    auth_ok = false;
                }
                std::size_t voLen = std::strlen(plainViewOnly);
                if (plugin_auth_ok && (auth_ok == false) && (voLen > 0) && (wResponseLength >= voLen) &&
                    (sodium_memcmp(plainViewOnly, pResponseData, voLen) == 0))
                {
                    m_client->EnableKeyboard(false); //PGM
                    m_client->EnablePointer(false);  //PGM
                    m_client->EnableGii(false);
                    auth_ok = true;
                }
            }
            else
            {
                std::size_t ppLen = std::strlen(ConfigHelpervar.m_szPassphrase);
                if ((wResponseLength < ppLen) || (sodium_memcmp(ConfigHelpervar.m_szPassphrase, pResponseData, ppLen) != 0))
                {
                    auth_ok = false;
                }
            }
            delete[] pResponseData;
        }
        ++nSequenceNumber;
    } while (bSendChallenge);

    if (auth_ok)
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

// must SetHandshakeComplete after sending auth result!
BOOL vncClientThread::AuthSecureVNCPlugin_old(std::string& auth_message)
{
    vncPasswd::ToText plain(settings->getPasswd(), settings->getSecure());
    m_socket->SetDSMPluginConfig(settings->getDSMPluginConfig());
    BOOL auth_ok = FALSE;

    const char* plainPassword = plain;

    if (!m_ms_logon && plainPassword && (std::strlen(plainPassword) > 0))
    {
        m_socket->GetIntegratedPlugin()->SetPasswordData(reinterpret_cast<const BYTE*>(plainPassword), static_cast<int>(std::strlen(plainPassword)));
    }

    int nSequenceNumber{ 0 };
    bool bSendChallenge{ true };
    do
    {
        BYTE* pChallenge{ nullptr };
        int nChallengeLength{ 0 };
        if (!m_socket->GetIntegratedPlugin()->GetChallenge(pChallenge, nChallengeLength, nSequenceNumber))
        {
            m_socket->GetIntegratedPlugin()->FreeMemory(pChallenge);
            auth_message = m_socket->GetIntegratedPlugin()->GetLastErrorString();
            return FALSE;
        }

        WORD wChallengeLength = static_cast<WORD>(nChallengeLength);

        if (!m_socket->SendExactQueue(reinterpret_cast<const char*>(&wChallengeLength), sizeof(wChallengeLength)))
        {
            m_socket->GetIntegratedPlugin()->FreeMemory(pChallenge);
            return FALSE;
        }

        if (!m_socket->SendExact(reinterpret_cast<const char*>(pChallenge), static_cast<VCard>(nChallengeLength)))
        {
            m_socket->GetIntegratedPlugin()->FreeMemory(pChallenge);
            return FALSE;
        }

        m_socket->GetIntegratedPlugin()->FreeMemory(pChallenge);
        WORD wResponseLength{ 0U };
        if (!m_socket->ReadExact(reinterpret_cast<char*>(&wResponseLength), sizeof(wResponseLength)))
        {
            return FALSE;
        }
        if (wResponseLength > 2024)
        {
            return FALSE;
        }

        BYTE* pResponseData = new BYTE[wResponseLength];

        if (!m_socket->ReadExact(reinterpret_cast<char*>(pResponseData), wResponseLength))
        {
            delete[] pResponseData;
            return FALSE;
        }

        if (!m_socket->GetIntegratedPlugin()->HandleResponse(pResponseData, static_cast<int>(wResponseLength), nSequenceNumber,
                                                             bSendChallenge))
        {
            auth_message = m_socket->GetIntegratedPlugin()->GetLastErrorString();
            auth_ok = FALSE;
            bSendChallenge = false;
        }
        else if (!bSendChallenge)
        {
            auth_ok = TRUE;
        }

        delete[] pResponseData;
        ++nSequenceNumber;
    } while (bSendChallenge);

    if (auth_ok)
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

// marscha@2006: Try to better hide the windows password.
// I know that this is no breakthrough in modern cryptography.
// It's just a patch/kludge/workaround.
BOOL vncClientThread::AuthMsLogon([[maybe_unused]] std::string& auth_message)
{
    DH dh;
    std::array<char, 8U> gen;
    std::array<char, 8U> mod;
    std::array<char, 8U> pub;
    std::array<char, 8U> resp;
    std::array<char, 256U> user;
    std::array<char, 64U> passwd;
    std::array<unsigned char, 8U> key{};

    dh.createKeys();
    int64ToBytes(dh.getValue(DH_GEN), gen.data());
    int64ToBytes(dh.getValue(DH_MOD), mod.data());
    int64ToBytes(dh.createInterKey(), pub.data());

    //adzm 2010-09 - minimize packets. SendExact flushes the queue.
    if (!m_socket->SendExactQueue(gen.data(), sizeof(gen)))
    {
        return FALSE;
    }
    if (!m_socket->SendExactQueue(mod.data(), sizeof(mod)))
    {
        return FALSE;
    }
    if (!m_socket->SendExact(pub.data(), sizeof(pub)))
    {
        return FALSE;
    }
    // Give the user time to type credentials (raise timeout for interactive wait)
    m_socket->SetTimeout(120000);
    m_client->authAttempted = true;
    if (!m_socket->ReadExact(resp.data(), sizeof(resp)))
    {
        return FALSE;
    }
    if (!m_socket->ReadExact(user.data(), sizeof(user)))
    {
        return FALSE;
    }
    if (!m_socket->ReadExact(passwd.data(), sizeof(passwd)))
    {
        return FALSE;
    }
    // Restore handshake timeout after interactive wait
    m_socket->SetTimeout(30000);

    int64ToBytes(dh.createEncryptionKey(bytesToInt64(resp.data())), reinterpret_cast<char*>(key.data()));
    vnclog.Print(0, "After DH: g=%I64u, m=%I64u, i=%I64u, key=%I64u\n", bytesToInt64(gen.data()), bytesToInt64(mod.data()),
                 bytesToInt64(pub.data()), bytesToInt64(reinterpret_cast<char*>(key.data())));
    vncDecryptBytes(reinterpret_cast<unsigned char*>(user.data()), sizeof(user), key.data());
    user[255U] = '\0';
    vncDecryptBytes(reinterpret_cast<unsigned char*>(passwd.data()), sizeof(passwd), key.data());
    passwd[63U] = '\0';

    int result = CheckUserGroupPasswordUni(user.data(), passwd.data(), m_client->GetClientNameAddress());
    vnclog.Print(LL_INTINFO, "CheckUserGroupPasswordUni result=%i\n", result);
    if (result == 2)
    { // ViewOnly?
        m_client->EnableKeyboard(false);
        m_client->EnablePointer(false);
        m_client->EnableGii(false);
    }

    if (result)
    {
        m_client->m_client_domain_username = _strdup(user.data());
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

// MS-Logon III: X25519 key exchange, but credentials are still DES-CBC
// (vncDecryptBytes) under the first 8 bytes of the derived key.
// Prefer MS-Logon IV (XChaCha20-Poly1305 AEAD) when the viewer supports it.
BOOL vncClientThread::AuthMsLogonIII([[maybe_unused]] std::string& auth_message)
{
    // X25519 key exchange
    std::array<unsigned char, crypto_scalarmult_BYTES> server_pk;
    std::array<unsigned char, crypto_scalarmult_BYTES> server_sk;
    std::array<unsigned char, crypto_scalarmult_BYTES> client_pk;
    std::array<unsigned char, crypto_scalarmult_BYTES> shared_secret;

    std::array<char, 256U> user{};
    std::array<char, 64U> passwd{};

    // Generate X25519 keypair
    if (crypto_box_keypair(server_pk.data(), server_sk.data()) != 0)
    {
        vnclog.Print(LL_LOGSCREEN, "Failed to generate X25519 keypair\n");
        return FALSE;
    }

    // Send server public key
    if (!m_socket->SendExactQueue(reinterpret_cast<char*>(server_pk.data()), sizeof(server_pk)))
    {
        return FALSE;
    }
    if (!m_socket->ClearQueue())
    {
        return FALSE;
    }

    // Give the user time to type credentials
    m_socket->SetTimeout(120000);
    m_client->authAttempted = true;

    // Read client public key
    if (!m_socket->ReadExact(reinterpret_cast<char*>(client_pk.data()), sizeof(client_pk)))
    {
        return FALSE;
    }

    // Compute shared secret
    if (crypto_scalarmult(shared_secret.data(), server_sk.data(), client_pk.data()) != 0)
    {
        vnclog.Print(LL_LOGSCREEN, "X25519 key exchange failed\n");
        return FALSE;
    }

    // Derive key material from shared secret (only first 8 bytes are used for DES)
    std::array<unsigned char, 32U> aes_key;
    crypto_hash_sha256(aes_key.data(), shared_secret.data(), sizeof(shared_secret));

    // Zero out secrets
    sodium_memzero(shared_secret.data(), sizeof(shared_secret));
    sodium_memzero(server_sk.data(), sizeof(server_sk));

    // Read encrypted credentials
    if (!m_socket->ReadExact(user.data(), sizeof(user)))
    {
        return FALSE;
    }
    if (!m_socket->ReadExact(passwd.data(), sizeof(passwd)))
    {
        return FALSE;
    }

    // Restore timeout
    m_socket->SetTimeout(30000);

    // Decrypt with DES-CBC (legacy wire format; key truncated to 8 bytes).
    // MS-Logon IV replaces this with XChaCha20-Poly1305 AEAD.
    std::array<unsigned char, 8U> key;
    std::memcpy(key.data(), aes_key.data(), 8);
    vncDecryptBytes(reinterpret_cast<unsigned char*>(user.data()), sizeof(user), key.data());
    user[255U] = '\0';
    vncDecryptBytes(reinterpret_cast<unsigned char*>(passwd.data()), sizeof(passwd), key.data());
    passwd[63U] = '\0';

    // Verify credentials
    int result = CheckUserGroupPasswordUni(user.data(), passwd.data(), m_client->GetClientNameAddress());
    vnclog.Print(LL_INTINFO, "CheckUserGroupPasswordUni result=%i\n", result);

    if (result == 2)
    { // ViewOnly?
        m_client->EnableKeyboard(false);
        m_client->EnablePointer(false);
        m_client->EnableGii(false);
    }

    // Cleanup
    sodium_memzero(aes_key.data(), sizeof(aes_key));
    sodium_memzero(key.data(), sizeof(key));

    if (result)
    {
        m_client->m_client_domain_username = _strdup(user.data());
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

// MS-Logon IV: X25519 key exchange + XChaCha20-Poly1305 AEAD on credentials.
// Wire format: server_pk(32) -> client_pk(32) -> user_ct(256+16) -> passwd_ct(64+16).
// Key and per-message nonces are derived from the shared secret (same derivation
// as the viewer), so nothing extra travels on the wire beyond ciphertexts + tags.
BOOL vncClientThread::AuthMsLogonIV([[maybe_unused]] std::string& auth_message)
{
    // X25519 key exchange
    std::array<unsigned char, crypto_scalarmult_BYTES> server_pk;
    std::array<unsigned char, crypto_scalarmult_BYTES> server_sk;
    std::array<unsigned char, crypto_scalarmult_BYTES> client_pk;
    std::array<unsigned char, crypto_scalarmult_BYTES> shared_secret;

    std::array<char, 256U> user;
    std::array<char, 64U> passwd;

    // Generate X25519 keypair
    if (crypto_box_keypair(server_pk.data(), server_sk.data()) != 0)
    {
        vnclog.Print(LL_LOGSCREEN, "Failed to generate X25519 keypair\n");
        return FALSE;
    }

    // Send server public key
    if (!m_socket->SendExactQueue(reinterpret_cast<char*>(server_pk.data()), sizeof(server_pk)))
    {
        return FALSE;
    }
    if (!m_socket->ClearQueue())
    {
        return FALSE;
    }

    // Give the user time to type credentials
    m_socket->SetTimeout(120000);
    m_client->authAttempted = true;

    // Read client public key
    if (!m_socket->ReadExact(reinterpret_cast<char*>(client_pk.data()), sizeof(client_pk)))
    {
        return FALSE;
    }

    // Compute shared secret
    if (crypto_scalarmult(shared_secret.data(), server_sk.data(), client_pk.data()) != 0)
    {
        vnclog.Print(LL_LOGSCREEN, "X25519 key exchange failed\n");
        return FALSE;
    }

    // Derive AEAD key and per-message nonces from the shared secret
    std::array<unsigned char, crypto_aead_xchacha20poly1305_ietf_KEYBYTES> enc_key;
    std::array<unsigned char, crypto_aead_xchacha20poly1305_ietf_NPUBBYTES> nonce_user;
    std::array<unsigned char, crypto_aead_xchacha20poly1305_ietf_NPUBBYTES> nonce_pass;
    crypto_generichash(enc_key.data(), sizeof(enc_key), reinterpret_cast<const unsigned char*>("uvnc-mslogon4-key"), 17, shared_secret.data(),
                       sizeof(shared_secret));
    crypto_generichash(nonce_user.data(), sizeof(nonce_user), reinterpret_cast<const unsigned char*>("uvnc-mslogon4-user"), 18, shared_secret.data(),
                       sizeof(shared_secret));
    crypto_generichash(nonce_pass.data(), sizeof(nonce_pass), reinterpret_cast<const unsigned char*>("uvnc-mslogon4-passwd"), 20, shared_secret.data(),
                       sizeof(shared_secret));

    // Zero out secrets
    sodium_memzero(shared_secret.data(), sizeof(shared_secret));
    sodium_memzero(server_sk.data(), sizeof(server_sk));

    // Read encrypted credentials (plaintext size + 16-byte Poly1305 tag)
    std::array<unsigned char, 256 + crypto_aead_xchacha20poly1305_ietf_ABYTES> user_ct;
    std::array<unsigned char, 64 + crypto_aead_xchacha20poly1305_ietf_ABYTES> passwd_ct;
    if (!m_socket->ReadExact(reinterpret_cast<char*>(user_ct.data()), sizeof(user_ct)))
    {
        return FALSE;
    }
    if (!m_socket->ReadExact(reinterpret_cast<char*>(passwd_ct.data()), sizeof(passwd_ct)))
    {
        return FALSE;
    }

    // Restore timeout
    m_socket->SetTimeout(30000);

    // Decrypt and authenticate credentials
    if (crypto_aead_xchacha20poly1305_ietf_decrypt(reinterpret_cast<unsigned char*>(user.data()), nullptr, nullptr, user_ct.data(), sizeof(user_ct), nullptr,
                                                   0, nonce_user.data(), enc_key.data()) != 0)
    {
        vnclog.Print(LL_LOGSCREEN, "MS-Logon IV: credential authentication failed (user)\n");
        sodium_memzero(enc_key.data(), sizeof(enc_key));
        return FALSE;
    }
    if (crypto_aead_xchacha20poly1305_ietf_decrypt(reinterpret_cast<unsigned char*>(passwd.data()), nullptr, nullptr, passwd_ct.data(), sizeof(passwd_ct),
                                                   nullptr, 0, nonce_pass.data(), enc_key.data()) != 0)
    {
        vnclog.Print(LL_LOGSCREEN, "MS-Logon IV: credential authentication failed (passwd)\n");
        sodium_memzero(enc_key.data(), sizeof(enc_key));
        return FALSE;
    }
    user[255U] = '\0';
    passwd[63U] = '\0';

    // Verify credentials
    int result = CheckUserGroupPasswordUni(user.data(), passwd.data(), m_client->GetClientNameAddress());
    vnclog.Print(LL_INTINFO, "CheckUserGroupPasswordUni result=%i\n", result);

    if (result == 2)
    { // ViewOnly?
        m_client->EnableKeyboard(false);
        m_client->EnablePointer(false);
        m_client->EnableGii(false);
    }

    // Cleanup
    sodium_memzero(enc_key.data(), sizeof(enc_key));
    sodium_memzero(passwd.data(), sizeof(passwd));

    if (result)
    {
        m_client->m_client_domain_username = _strdup(user.data());
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

// MS-Logon II fallback for old viewers when RequireMSLogonIII is set
// Does normal DH handshake but always rejects with upgrade message
BOOL vncClientThread::AuthMsLogonReject(std::string& auth_message)
{
    DH dh;
    std::array<char, 8U> gen;
    std::array<char, 8U> mod;
    std::array<char, 8U> pub;
    std::array<char, 8U> resp;
    std::array<char, 256U> user;
    std::array<char, 64U> passwd;
    std::array<unsigned char, 8U> key{};

    dh.createKeys();
    int64ToBytes(dh.getValue(DH_GEN), gen.data());
    int64ToBytes(dh.getValue(DH_MOD), mod.data());
    int64ToBytes(dh.createInterKey(), pub.data());

    // Send DH params (same as normal AuthMsLogon)
    if (!m_socket->SendExactQueue(gen.data(), sizeof(gen)))
    {
        return FALSE;
    }
    if (!m_socket->SendExactQueue(mod.data(), sizeof(mod)))
    {
        return FALSE;
    }
    if (!m_socket->SendExact(pub.data(), sizeof(pub)))
    {
        return FALSE;
    }

    // Give user time to respond
    m_socket->SetTimeout(120000);
    m_client->authAttempted = true;

    // Read viewer's response (we don't care about it)
    if (!m_socket->ReadExact(resp.data(), sizeof(resp)))
    {
        return FALSE;
    }
    if (!m_socket->ReadExact(user.data(), sizeof(user)))
    {
        return FALSE;
    }
    if (!m_socket->ReadExact(passwd.data(), sizeof(passwd)))
    {
        return FALSE;
    }

    // Restore timeout
    m_socket->SetTimeout(30000);

    // Compute key but don't use it for anything
    int64ToBytes(dh.createEncryptionKey(bytesToInt64(resp.data())), reinterpret_cast<char*>(key.data()));

    // Set error message for old viewer
    auth_message = "This server requires MS-Logon III or newer (secure authentication). "
                   "Your viewer is too old and only supports the legacy MS-Logon II. "
                   "Please upgrade your UltraVNC viewer to version 1.8.2.3 or later.";

    vnclog.Print(LL_LOGSCREEN, "Auth rejected: Viewer lacks MS-Logon III support (server enforces secure auth)\n");

    // Always return FALSE (reject)
    return FALSE;
}

BOOL vncClientThread::AuthVnc([[maybe_unused]] std::string& auth_message)
{
    vncPasswd::ToText plain(settings->getPasswd(), settings->getSecure());

    BOOL auth_ok = FALSE;
    {
        // Now create a 16-byte challenge
        std::array<char, 16U> challenge{};
        std::array<char, 16U> challenge2; //PGM
        std::array<char, 16U> response;
        vncRandomBytes(reinterpret_cast<BYTE*>(challenge.data()));
        std::memcpy(challenge2.data(), challenge.data(), 16); //PGM

        {
            vnclog.Print(LL_INTINFO, "password authentication\n");
            if (!m_socket->SendExact(challenge.data(), sizeof(challenge)))
            {
                vnclog.Print(LL_SOCKERR, VNCLOG("Failed to send challenge to client\n"));
                return FALSE;
            }

            // Give the user time to type the password (raise timeout for interactive wait)
            m_socket->SetTimeout(120000);
            m_client->authAttempted = true;
            // Read the response
            if (!m_socket->ReadExact(response.data(), sizeof(response)))
            {
                vnclog.Print(LL_SOCKERR, VNCLOG("Failed to receive challenge response from client\n"));
                return FALSE;
            }
            // Restore handshake timeout after the interactive wait
            m_socket->SetTimeout(30000);
            // Encrypt the challenge bytes
            vncEncryptBytes(reinterpret_cast<BYTE*>(challenge.data()), plain);

            auth_ok = TRUE;
            // Compare them to the response
            for (int i{ 0 }; i < sizeof(challenge); ++i)
            {
                if (challenge[static_cast<unsigned long long>(i)] != response[static_cast<unsigned long long>(i)])
                {
                    auth_ok = FALSE;
                    break;
                }
            }
            if (!auth_ok)                                                                       //PGM
            {                                                                                   //PGM
                vncPasswd::ToText plain2(settings->getPasswdViewOnly(), settings->getSecure()); //PGM
                if ((std::strlen(plain2) > 0))                                                  //PGM
                {                                                                               //PGM
                    vnclog.Print(LL_INTINFO, "View-only password authentication\n");            //PGM
                    m_client->EnableKeyboard(false);                                            //PGM
                    m_client->EnablePointer(false);                                             //PGM
                    m_client->EnableGii(false);
                    auth_ok = TRUE; //PGM

                    // Encrypt the view-only challenge bytes //PGM
                    vncEncryptBytes(reinterpret_cast<BYTE*>(challenge2.data()), plain2); //PGM

                    // Compare them to the response //PGM
                    for (int i{ 0 }; i < sizeof(challenge2); ++i) //PGM
                    {                                             //PGM
                        if (challenge2[static_cast<unsigned long long>(i)] != response[static_cast<unsigned long long>(i)])         //PGM
                        {                                         //PGM
                            auth_ok = FALSE;                      //PGM
                            break;                                //PGM
                        } //PGM
                    } //PGM
                } //PGM
            } //PGM
        }
    }

    if (auth_ok)
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

// adzm 2010-10
BOOL vncClientThread::AuthSCPrompt(std::string& auth_message)
{
    // Check if viewer accept connection
    std::array<char, 1024U> mytext;
    getinfo(mytext.data());
    int size = static_cast<int>(std::strlen(mytext.data()));
    //adzm 2010-09 - minimize packets. SendExact flushes the queue.
    if (!m_socket->SendExactQueue(reinterpret_cast<char*>(&size), sizeof(int)))
    {
        return FALSE;
    }
    if (!m_socket->SendExact(reinterpret_cast<char*>(mytext.data()), static_cast<VCard>(size)))
    {
        return FALSE;
    }
    // Give the user time to respond to the SC prompt (raise timeout for interactive wait)
    m_socket->SetTimeout(120000);
    m_client->authAttempted = true;
    int nummer{ 0 };
    if (!m_socket->ReadExact(reinterpret_cast<char*>(&nummer), sizeof(int)))
    {
        m_socket->SetTimeout(30000);
        return FALSE;
    }
    m_socket->SetTimeout(30000);
    if (nummer == 0)
    {
        auth_message = "Viewer refused connection";
        return FALSE;
    }
    else
    {
        return TRUE;
    }
}

BOOL vncClientThread::AuthSessionSelect([[maybe_unused]] std::string& auth_message)
{
    return TRUE;
    /* Session select */
    /*
    {
        //Fake Function
        CARD32 auth_result_msg = Swap32IfLE(rfbUltraVNC_SessionSelect);
        if (!m_socket->SendExact((char *)&auth_result_msg, sizeof(auth_result_msg)))
            return FALSE;
        CARD8 Items=3;
        if (!m_socket->SendExact((char *)&Items, sizeof(Items)))
            return FALSE;
        char line1[128];
        char line2[128];
        char line3[128];
        strcpy_s(line1,"line1 ");
        strcpy_s(line2,"line22 ");
        strcpy_s(line3,"line312 123 ");
        if (!m_socket->SendExact((char *)line1, 128))
            return FALSE;
        if (!m_socket->SendExact((char *)line2, 128))
            return FALSE;
        if (!m_socket->SendExact((char *)line3, 128))
            return FALSE;
        int nummer;
        if (!m_socket->ReadExact((char *)&nummer, sizeof(int)))
        {
            return FALSE;
        }
        int a=0;
    }
    */
}

BOOL vncClientThread::AuthVeNCrypt(std::string& auth_message)
{
    // The VeNCrypt state machine performs the TLS handshake and then
    // validates username+token through a configurable native DLL.
    // On success the TLS callbacks are left installed on m_socket so the
    // rest of the RFB session (including file transfer) stays encrypted.
    auto auth = std::make_unique<VeNCryptAuth>(m_socket);
    // secTypeX509Vnc runs the classic VNC challenge over the TLS channel;
    // AuthVnc's SendExact/ReadExact go through the TLS callbacks once
    // PerformTLSHandshake has installed them.
    auth->VncAuthCallback = [this](std::string& msg) { return AuthVnc(msg) != FALSE; };
    auth->MsLogonIIIAuthCallback = [this](std::string& msg) { return AuthMsLogonIII(msg) != FALSE; };
    // secTypeX509Plain carries user+password: validate via MS-Logon when
    // enabled, otherwise against the VNC / view-only passwords (same as RA2).
    auth->PlainAuthCallback = [this](const std::string& user, const std::string& pass, std::string& msg) {
        if (m_ms_logon)
        {
            int result = CheckUserGroupPasswordUni(reinterpret_cast<char*>(const_cast<char *>(user.c_str())), reinterpret_cast<char*>(const_cast<char *>(pass.c_str())), m_client->GetClientNameAddress());
            vnclog.Print(LL_INTINFO, "VeNCrypt Plain CheckUserGroupPasswordUni result=%i\n", result);
            if (result == 2)
            { // view-only
                m_client->EnableKeyboard(false);
                m_client->EnablePointer(false);
                m_client->EnableGii(false);
            }
            if (result)
            {
                m_client->m_client_domain_username = _strdup(user.c_str());
                return true;
            }
            msg = "Authentication failed";
            return false;
        }
        vncPasswd::ToText plain(settings->getPasswd(), settings->getSecure());
        if ((std::strlen(plain) > 0) && (pass == static_cast<const char*>(plain)))
        {
            return true;
        }
        vncPasswd::ToText plain2(settings->getPasswdViewOnly(), settings->getSecure());
        if ((std::strlen(plain2) > 0) && (pass == static_cast<const char*>(plain2)))
        {
            m_client->EnableKeyboard(false);
            m_client->EnablePointer(false);
            m_client->EnableGii(false);
            return true;
        }
        msg = "Authentication failed";
        return false;
    };

    // Give the user time to accept the certificate and type credentials
    m_socket->SetTimeout(120000);
    m_client->authAttempted = true;
    const BOOL ok = auth->Authenticate(auth_message);
    // Restore handshake timeout
    m_socket->SetTimeout(30000);

    m_veNCryptAuth = std::move(auth);
    return ok;
}

void vncClientThread::ReleaseRA2() { m_ra2Auth.reset(); }

// RA2 / RA2ne (RealVNC RSA-AES security types 5/6/129/130).
// The RA2 handshake runs the RSA key exchange, key hash verification and
// credential read; for RA2 the EAX callbacks stay installed on the socket
// for the rest of the session, for RA2ne they are removed inside
// Authenticate() after the credentials.
BOOL vncClientThread::AuthRSAAES(const CARD8 authType, std::string& auth_message)
{
    const bool allEncrypted = ((authType == rfbRSAAES) || (authType == rfbRSAAES_256));
    const DWORD keySize = ((authType == rfbRSAAES_256) || (authType == rfbRSAAESne_256)) ? 256 : 128;
    const bool requireUser = (m_ms_logon != FALSE);

    auto auth = std::make_unique<RSAAESAuth>(m_socket);

    // Give the user time to type credentials
    m_socket->SetTimeout(120000);
    m_client->authAttempted = true;

    std::string user, pass;
    BOOL ok = auth->Authenticate(keySize, allEncrypted, requireUser, user, pass, auth_message);

    m_socket->SetTimeout(30000);

    if (!ok)
    {
        return FALSE;
    }

    if (requireUser)
    {
        int result = CheckUserGroupPasswordUni(reinterpret_cast<char*>(const_cast<char *>(user.c_str())), reinterpret_cast<char*>(const_cast<char *>(pass.c_str())), m_client->GetClientNameAddress());
        vnclog.Print(LL_INTINFO, "RA2 CheckUserGroupPasswordUni result=%i\n", result);
        if (result == 2)
        { // ViewOnly?
            m_client->EnableKeyboard(false);
            m_client->EnablePointer(false);
            m_client->EnableGii(false);
        }
        if (result)
        {
            m_client->m_client_domain_username = _strdup(user.c_str());
            m_ra2Auth = std::move(auth);
            return TRUE;
        }
        auth_message = "Authentication failed";
        return FALSE;
    }

    // Password-only subtype: compare against the VNC password (and the
    // view-only password, which grants a restricted session).
    vncPasswd::ToText plain(settings->getPasswd(), settings->getSecure());
    if ((std::strlen(plain) > 0) && (pass == static_cast<const char*>(plain)))
    {
        m_ra2Auth = std::move(auth);
        return TRUE;
    }
    vncPasswd::ToText plain2(settings->getPasswdViewOnly(), settings->getSecure());
    if ((std::strlen(plain2) > 0) && (pass == static_cast<const char*>(plain2)))
    {
        m_client->EnableKeyboard(false);
        m_client->EnablePointer(false);
        m_client->EnableGii(false);
        m_ra2Auth = std::move(auth);
        return TRUE;
    }
    auth_message = "Authentication failed";
    return FALSE;
}

void ClearKeyState(const BYTE key)
{
    // This routine is used by the VNC client handler to clear the
    // CAPSLOCK, NUMLOCK and SCROLL-LOCK states.

    std::array<BYTE, 256U> keyState{};

    ::GetKeyboardState(reinterpret_cast<LPBYTE>(keyState.data()));

    if (keyState[key] & 1)
    {
        // Simulate the key being pressed
        keybd_uni_event(key, 0, KEYEVENTF_EXTENDEDKEY, 0);

        // Simulate it being release
        keybd_uni_event(key, 0, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
    }
}

// Modif sf@2002
// Get the local ip addresses as a human-readable string.
// If more than one, then with \n between them.
// If not available, then gets a message to that effect.
void GetIPString(char* buffer, const int buflen)
{
    std::array<char, 256U> namebuf;

    if (gethostname(namebuf.data(), 256) != 0)
    {
        ::strncpy_s(buffer, static_cast<rsize_t>(buflen), "Host name unavailable", static_cast<rsize_t>(buflen));
        return;
    }
    if (settings->getIPV6())
    {
        *buffer = '\0';

        LPSOCKADDR sockaddr_ip;
        struct addrinfo hint;
        struct addrinfo* serverinfo = nullptr;
        std::memset(&hint, 0, sizeof(hint));
        hint.ai_family = AF_UNSPEC;
        hint.ai_socktype = SOCK_STREAM;
        hint.ai_protocol = IPPROTO_TCP;
        struct sockaddr_in6* pIpv6Addr;
        struct sockaddr_in6 Ipv6Addr;
        std::memset(&Ipv6Addr, 0, sizeof(Ipv6Addr));

        //make sure the buffer is not overwritten

        if (getaddrinfo(namebuf.data(), nullptr, &hint, &serverinfo) == 0)
        {
            struct addrinfo* p;
            p = serverinfo;
            for (p = serverinfo; p != nullptr; p = p->ai_next)
            {
                switch (p->ai_family)
                {
                case AF_INET: {
                    break;
                }
                case AF_INET6: {
                    std::array<char, 46U> ipstringbuffer;
                    DWORD ipbufferlength{ 46U };
                    ipbufferlength = 46;
                    std::memset(ipstringbuffer.data(), 0, 46);

                    pIpv6Addr = reinterpret_cast<struct sockaddr_in6*>(p->ai_addr);
                    std::memcpy(&Ipv6Addr, pIpv6Addr, sizeof(Ipv6Addr));
                    Ipv6Addr.sin6_family = AF_INET6;

                    sockaddr_ip = reinterpret_cast<LPSOCKADDR>(p->ai_addr);

                    ::WSAAddressToString(sockaddr_ip, static_cast<DWORD>(p->ai_addrlen), nullptr, ipstringbuffer.data(),
                                       &ipbufferlength);
                    std::array<char, 256U> szText;
                    std::memset(szText.data(), 0, 256);
                    ::strncpy_s(szText.data(), szText.size(), ipstringbuffer.data(), ipbufferlength - 4);
                    ::strcat_s(szText.data(), szText.size(), "-");
                    int len = static_cast<int>(std::strlen(buffer));
                    int len2 = static_cast<int>(std::strlen(szText.data()));
                    if (len + len2 < buflen)
                    {
                        ::strcat_s(buffer, static_cast<rsize_t>(buflen), szText.data());
                    }
                    break;
                }
                default:
                    break;
                }
            }
        }
        freeaddrinfo(serverinfo);
    }
    else
    {
        HOSTENT* ph = nullptr;
        ph = gethostbyname(namebuf.data());
        if (!ph)
        {
            ::strncpy_s(buffer, static_cast<rsize_t>(buflen), "IP address unavailable", static_cast<rsize_t>(buflen));
            return;
        }

        *buffer = '\0';
        std::array<char, 5U> digtxt;
        for (int i{ 0 }; ph->h_addr_list[i]; ++i)
        {
            for (int j{ 0 }; j < ph->h_length; ++j)
            {
                ::sprintf_s(digtxt.data(), digtxt.size(), "%d.", static_cast<unsigned char>(ph->h_addr_list[i][j]));
                ::strncat_s(buffer, static_cast<rsize_t>(buflen), digtxt.data(), (static_cast<size_t>(buflen - 1)) - std::strlen(buffer));
            }
            buffer[std::strlen(buffer) - 1] = '\0';
            if (ph->h_addr_list[i + 1] != nullptr)
            {
                ::strncat_s(buffer, static_cast<rsize_t>(buflen), ", ", (static_cast<size_t>(buflen - 1)) - std::strlen(buffer));
            }
        }
    }
}

// adzm 2010-08
bool vncClientThread::InitSocket()
{
    // To avoid people connecting and then halting the connection, set a timeout

    // adzm 2010-10 - Set 0 timeout if using repeater
    VBool bSocketTimeoutSet = VFalse;
    if (m_client->GetRepeaterID())
    {
        bSocketTimeoutSet = m_socket->SetTimeout(0);
    }
    else
    {
        bSocketTimeoutSet = m_socket->SetTimeout(30000);
    }
    if (!bSocketTimeoutSet)
    {
        vnclog.Print(LL_INTERR, VNCLOG("failed to set socket timeout(%d)\n"), ::GetLastError());
    }

    // sf@2002 - DSM Plugin - Tell the client's socket where to find the DSMPlugin
    if (m_server->GetDSMPluginPointer() != nullptr)
    {
        m_socket->SetDSMPluginPointer(m_server->GetDSMPluginPointer());
    }
    else
    {
        vnclog.Print(LL_INTINFO, VNCLOG("Invalid DSMPlugin Pointer\n"));
        return false;
    }

    // Detect loopback early so DSM/auth relaxation decisions can be applied before
    // any protocol handshaking. The peer address comes from the OS, so it cannot be
    // spoofed by a remote client. Always recompute (not just set-to-true): the same
    // vncClient/socket pair can be reused across a repeater/auto-reconnect cycle
    // (see TryReconnect(), which calls InitSocket() again on a new outbound socket),
    // so a stale loopback flag from a previous connection must not leak forward.
    const char* peerAddr = m_socket->GetPeerName(false);
    m_client->m_IsLoopback = ((peerAddr != nullptr) && IsLoopbackAddress(peerAddr));

    // LOCK INITIAL SETUP
    // All clients have the m_protocol_ready flag set to FALSE initially, to prevent
    // updates and suchlike interfering with the initial protocol negotiations.

    // sf@2002 - DSMPlugin
    // Use Plugin only from this point (now BEFORE Protocol handshaking)
    bool usePlugin = m_server->GetDSMPluginPointer()->IsEnabled();
    if (usePlugin && m_client->m_IsLoopback && settings->getAllowLoopbackWithoutDSM())
    {
        usePlugin = false;
        vnclog.Print(LL_LOGSCREEN, "Loopback connection: DSM plugin disabled by AllowLoopbackWithoutDSM");
        vnclog.Print(LL_CLIENTS, VNCLOG("loopback connection: not enabling DSM plugin (AllowLoopbackWithoutDSM is set)\n"));
    }

    if (usePlugin)
    {
        // sf@2007 - Current DSM code does not support multithreading
        // Data mix is causing server crash and viewer drops when more than one viewer is copnnected at a time
        // We must reject any new viewer connection BEFORE any data passes through the plugin
        // This is a dirty workaround. We ignore all Multi Viewer connection settings...
        //adzm 2009-06-20 - Fixed this to use multi-threaded version, so therefore we can handle multiple
        // clients with no issues.
        // Use DSMActiveClientCount() rather than AuthClientCount(): with AllowLoopbackWithoutDSM,
        // authenticated clients may exist that are not using the plugin at all, and they must not
        // trip (or satisfy) this single-DSM-connection guard.
        if (!m_server->GetDSMPluginPointer()->SupportsMultithreaded() && (m_server->DSMActiveClientCount() > 0))
        {
            vnclog.Print(LL_CLIENTS, VNCLOG("A connection using DSM already exist - client rejected to avoid crash \n"));
            return false;
        }

        //adzm 2009-06-20 - TODO - Not sure about this. what about pending connections via the repeater?
        if (m_server->DSMActiveClientCount() == 0)
        {
            m_server->GetDSMPluginPointer()->ResetPlugin();
        } //SEC reset if needed
        m_socket->EnableUsePlugin(true);
        m_client->m_encodemgr.EnableQueuing(false);
        // TODO: Make a more secured challenge (with time stamp)
    }
    else
    {
        m_client->m_encodemgr.EnableQueuing(true);
    }

    return true;
}

bool vncClientThread::TryReconnect()
{
    if (m_client == nullptr)
    {
        return false;
    }
    if (fShutdownOrdered || m_client->m_Autoreconnect || !m_client->GetHost() || !m_client->GetRepeaterID())
    {
        return false;
    }

    if (m_socket)
    {
        m_veNCryptAuth.reset();
        m_socket->Close();
        if (m_client && m_client->m_socket)
        {
            m_client->m_socket = nullptr;
        }
        delete m_socket;
        m_socket = nullptr;
    }

    // Attempt to create a new socket
    VSocket* tmpsock = new VSocket;
    if (!tmpsock)
    {
        return false;
    }

    m_socket = tmpsock;
    if (m_client)
    {
        m_client->m_socket = tmpsock;
    }

    // Connect out to the specified host on the UltraVNC Viewer listen port
    // To be really good, we should allow a display number here but
    // for now we'll just assume we're connecting to display zero
    bool result;
    if (settings->getIPV6())
    {
        result = m_socket->CreateConnect(m_client->GetHost(), static_cast<VCard>(m_client->GetHostPort()));
    }
    else
    {
        m_socket->Create();
        result = m_socket->Connect(m_client->GetHost(), static_cast<VCard>(m_client->GetHostPort()));
    }
    if (result)
    {
        if (m_client->GetRepeaterID())
        {
            std::array<char, _MAX_PATH> finalidcode;
            //adzm 2010-08 - this was sending uninitialized data over the wire
            ::ZeroMemory(finalidcode.data(), sizeof(finalidcode));
            ::strncpy_s(finalidcode.data(), finalidcode.size(), m_client->GetRepeaterID(), sizeof(finalidcode) - 1);

            m_socket->Send(finalidcode.data(), 250);

            InitSocket();

            return true;
        }
    }

    return false;
}

void vncClientThread::run([[maybe_unused]] void* const arg)
{
#ifdef _Gii
#ifdef _USE_DLL
    DLL_InitializeTouchInjection = nullptr;
    DLL_PInjectTouch = nullptr;
    win8dllHandle = nullptr;
    if (VNC_OSVersion::getInstance()->OS_WIN8 || VNC_OSVersion::getInstance()->OS_WIN10)
    {
        win8dllHandle = ::LoadLibraryEx("InjectTouch.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        DLL_InitializeTouchInjection = (PInitializeTouchInjection)::GetProcAddress(win8dllHandle, "DLL_InitializeTouchInjection");
        DLL_PInjectTouch = (PInjectTouch)::GetProcAddress(win8dllHandle, "DLL_PInjectTouch");
        ;
    }
#endif
    InjectTouchInputUVNC = nullptr;
    InitializeTouchInjectionUVNC = nullptr;
    HINSTANCE user32 = ::LoadLibraryExA("user32.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (user32)
    {
        InjectTouchInputUVNC = reinterpret_cast<PtrInjectTouchInput>((::GetProcAddress(user32, "InjectTouchInput")));
        InitializeTouchInjectionUVNC = reinterpret_cast<PtrInitializeTouchInjection>((::GetProcAddress(user32, "InitializeTouchInjection")));
    }

#endif
    // All this thread does is go into a socket-receive loop,
    // waiting for stuff on the given socket

    // IMPORTANT : ALWAYS call RemoveClient on the server before quitting
    // this thread.
    ::SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);

    vnclog.Print(LL_CLIENTS, VNCLOG("client connected : %s (%hd)\n"), m_client->GetClientNameName(), m_client->GetClientId());
    // Save the handle to the thread's original desktop
    HDESK home_desktop = ::GetThreadDesktop(::GetCurrentThreadId());
    HDESK input_desktop = nullptr;

    // Initially blacklist the client so that excess connections from it get dropped
    m_server->AddAuthHostsBlacklist(m_client->GetClientNameAddress());

    // adzm 2010-08
    if (!InitSocket())
    {
        m_server->RemoveClient(m_client->GetClientId());
        return;
    }

    // GET PROTOCOL VERSION
    if (!InitVersion())
    {
        // Version negotiation failed before any auth attempt — undo pre-emptive blacklist entry.
        m_server->RemAuthHostsBlacklist(m_client->GetClientNameAddress());
        // wa@2005 - AutoReconnection attempt if required
        if (m_client->m_Autoreconnect && !fShutdownOrdered)
        {
            for (int i{ 0 }; i < 10 * m_server->AutoReconnect_counter; ++i)
            {
                ::Sleep(100);
                if (m_autoreconnectcounter_quit)
                {
                    return;
                }
            }
            m_server->AutoReconnect_counter += 10;
            if (m_server->AutoReconnect_counter > 20)
            {
                m_server->AutoReconnect_counter = 20;
            }
            vnclog.Print(LL_INTERR, VNCLOG("PostAddNewClient I\n"));
            m_server->AutoReconnect(m_client->m_Autoreconnect);
            m_server->AutoReconnectPort(m_AutoReconnectPort);
            m_server->AutoReconnectAdr(m_szAutoReconnectAdr.data());
            m_server->AutoReconnectId(m_szAutoReconnectId.data());
            if (settings->getIPV6())
            {
                postHelper::PostAddNewClient4(1111, 1111);
            }
            else
            {
                postHelper::PostAddNewClient(1111, 1111);
            }
        }
        m_server->RemoveClient(m_client->GetClientId());
        return;
    }
    m_server->AutoReconnect_counter = 0;
    vnclog.Print(LL_INTINFO, VNCLOG("negotiated version\n"));

    // AUTHENTICATE LINK
    if (!InitAuthenticate())
    {
        // If the connection was dropped/cancelled before credentials were ever submitted,
        // undo the pre-emptive blacklist entry — no wrong password was entered.
        if (!m_client->forceBlacklist && !m_client->authAttempted)
        {
            m_server->RemAuthHostsBlacklist(m_client->GetClientNameAddress());
        }
        m_server->RemoveClient(m_client->GetClientId());
        if (!fShutdownOrdered)
        {
            if (m_client->m_Autoreconnect)
            {
                vnclog.Print(LL_INTERR, VNCLOG("PostAddNewClient II\n"));
                m_server->AutoReconnect(m_client->m_Autoreconnect);
                m_server->AutoReconnectPort(m_AutoReconnectPort);
                m_server->AutoReconnectAdr(m_szAutoReconnectAdr.data());
                m_server->AutoReconnectId(m_szAutoReconnectId.data());
                if (settings->getIPV6())
                {
                    postHelper::PostAddNewClient4(1111, 1111);
                }
                else
                {
                    postHelper::PostAddNewClient(1111, 1111);
                }
            }
        }
        return;
    }

    // Authenticated OK - remove from blacklist and remove timeout
    if (!m_client->forceBlacklist)
    {
        m_server->RemAuthHostsBlacklist(m_client->GetClientNameAddress());
    }
    m_client->forceBlacklist = false;
    m_socket->SetTimeout(settings->getIdleTimeout() * 1000);
    vnclog.Print(LL_INTINFO, VNCLOG("authenticated connection\n"));

    // Set Client Connect time
    m_client->SetConnectTime(static_cast<long>(GetTimeFunction()));

    // INIT PIXEL FORMAT

    // Get the screen format
    //	m_client->m_fullscreen = m_client->m_encodemgr.GetSize();

    // Modif sf@2002 - Scaling
    if (m_server->AreThereMultipleViewers() == false)
    {
        omni_mutex_lock l(m_client->GetUpdateLock(), 84);
        m_client->m_encodemgr.m_buffer->SetScale(settings->getDefaultScale()); // v1.1.2
    }
    else
    {
        m_client->SetScreenOffset(m_server->m_desktop->m_ScreenOffsetx, m_server->m_desktop->m_ScreenOffsety,
                                  m_server->m_desktop->nr_monitors == 1);
        m_client->SetBufferOffset(m_server->m_desktop->m_SWOffsetx, m_server->m_desktop->m_SWOffsety);
    }
    m_client->m_ScaledScreen = m_client->m_encodemgr.m_buffer->GetViewerSize();
    m_client->m_nScale = m_client->m_encodemgr.m_buffer->GetScale();

    // Get the name of this desktop
    // sf@2002 - v1.1.x - Complete the computer name with the IP address if necessary
    bool fIP{ false };
    std::array<char, MAX_COMPUTERNAME_LENGTH + 3 + 256 + 32> desktopname{ 0 };
    DWORD desktopnamelen = MAX_COMPUTERNAME_LENGTH + 1 + 256 + 32;
    std::memset(reinterpret_cast<char*>(desktopname.data()), 0, sizeof(desktopname));
    if (::GetComputerName(desktopname.data(), &desktopnamelen))
    {
        // Make the name lowercase
        for (std::size_t x{ 0U }; x < std::strlen(desktopname.data()); ++x)
        {
            desktopname[x] = std::tolower(desktopname[x]);
        }
        // Check for the presence of "." in the string (then it's presumably an IP adr)
        if (strchr(desktopname.data(), '.') != nullptr)
        {
            fIP = true;
        }
    }
    else
    {
        ::strcpy_s(desktopname.data(), desktopname.size(), "WinVNC");
    }

    // We add the IP address(es) to the computer name, if possible and necessary
    if (!fIP)
    {
        std::array<char, 256U> szIP;
        GetIPString(szIP.data(), sizeof(szIP));
        if ((std::strlen(szIP.data()) > 3) && (szIP[0U] != 'I') && (szIP[0U] != 'H'))
        {
            ::strcat_s(desktopname.data(), desktopname.size(), " ( ");
            ::strcat_s(desktopname.data(), desktopname.size(), szIP.data());
            ::strcat_s(desktopname.data(), desktopname.size(), " )");
        }
    }

    ::strcat_s(desktopname.data(), desktopname.size(), " - ");
    if (settings->RunningFromExternalService())
    {
        ::strcat_s(desktopname.data(), desktopname.size(), "service mode");
    }
    else
    {
        ::strcat_s(desktopname.data(), desktopname.size(), "application mode");
    }

    // Send the server format message to the client
    rfbServerInitMsg server_ini{};
    server_ini.format = m_client->m_encodemgr.m_buffer->GetLocalFormat();

    // Endian swaps
    // Modif sf@2002 - Scaling
    server_ini.framebufferWidth = Swap16IfLE(m_client->m_ScaledScreen.br.x - m_client->m_ScaledScreen.tl.x);
    server_ini.framebufferHeight = Swap16IfLE(m_client->m_ScaledScreen.br.y - m_client->m_ScaledScreen.tl.y);

    server_ini.format.redMax = Swap16IfLE(server_ini.format.redMax);
    server_ini.format.greenMax = Swap16IfLE(server_ini.format.greenMax);
    server_ini.format.blueMax = Swap16IfLE(server_ini.format.blueMax);

    CARD32 nNameLength = static_cast<CARD32>(std::strlen(desktopname.data()));

    server_ini.nameLength = Swap32IfLE(nNameLength);

    //adzm 2010-09 - minimize packets. SendExact flushes the queue.
    if (!m_socket->SendExactQueue(reinterpret_cast<char*>(&server_ini), sizeof(server_ini)))
    {
        m_server->RemoveClient(m_client->GetClientId());
        return;
    }
    if (!m_socket->SendExact(desktopname.data(), nNameLength))
    {
        m_server->RemoveClient(m_client->GetClientId());
        return;
    }
    vnclog.Print(LL_INTINFO, VNCLOG("sent pixel format to client\n"));

    // UNLOCK INITIAL SETUP
    // Initial negotiation is complete, so set the protocol ready flag
    m_client->EnableProtocol();

    // Add a fullscreen update to the client's update list
    // sf@2002 - Scaling
    // m_client->m_update_tracker.add_changed(m_client->m_fullscreen);
    { // RealVNC 336
        omni_mutex_lock l(m_client->GetUpdateLock(), 91);
        m_client->m_update_tracker.add_changed(m_client->m_ScaledScreen);
    }

    // MAIN LOOP
#ifdef _DEBUG
    std::array<char, 256U> szText;
    ::sprintf_s(szText.data(), szText.size(), " MAIN LOOP \n");
    ::OutputDebugString(szText.data());
#endif
    // Set the input thread to a high priority
    set_priority(omni_thread::PRIORITY_HIGH);

    m_client->cl_connected = TRUE;
    // added jeff
    BOOL need_to_disable_input = settings->getDisableLocalInputs();
    bool need_to_clear_keyboard{ true };
    bool need_first_keepalive{ false };
    bool need_keepalive{ false };
    bool need_first_idletime{ false };
    bool need_monitor_info{ false };
    bool firstrun{ true };
    bool need_ft_version_msg{ false };
    // adzm - 2010-07 - Extended clipboard
    bool need_notify_extended_clipboard{ false };
    // adzm 2010-09 - Notify streaming DSM plugin support
    bool need_notify_streaming_DSM{ false };

    while (m_client->cl_connected)
    {
        rfbClientToServerMsg msg{};

        // Ensure that we're running in the correct desktop
        if (!m_client->IsFileTransBusy())
            // This desktop switch is responsible for the keyboard input
            if (desktopSelector::InputDesktopSelected() == 0)
            {
                vnclog.Print(LL_CONNERR, VNCLOG("vncClientThread \n"));
                if (!desktopSelector::SelectDesktop(nullptr, &input_desktop))
                {
                    break;
                }
            }
        // added jeff
        // 2 May 2008 jdp paquette@atnetsend.net moved so that we're on the right desktop  when we're a service
        // Clear the CapsLock and NumLock keys
        if (m_client->m_keyboardenabled && need_to_clear_keyboard)
        {
            ClearKeyState(VK_CAPITAL);
            // *** JNW - removed because people complain it's wrong
            //ClearKeyState(VK_NUMLOCK);
            ClearKeyState(VK_SCROLL);
            need_to_clear_keyboard = false;
        }
        //
        if (need_to_disable_input)
        {
            // have to do this here if we're a service so that we're on the correct desktop
            m_client->m_encodemgr.m_buffer->m_desktop->SetDisableInput();
            need_to_disable_input = false;
        }

        // reclaim input block after local C+A+D if user currently has it blocked
        m_client->m_encodemgr.m_buffer->m_desktop->block_input(firstrun);
        firstrun = false;

        if (need_first_keepalive)
        {
            // send first keepalive to let the client know we accepted the encoding request
            m_client->SendServerStateUpdate(rfbKeepAliveInterval, static_cast<CARD32>(settings->getkeepAliveInterval()));
            m_client->SendKeepAlive(true);
            need_first_keepalive = false;
        }
        if (need_keepalive)
        {
            m_client->SendKeepAlive(true);
            need_keepalive = false;
        }

        if (need_first_idletime)
        {
            // send idletime to let the client know we accepted the encoding request
            m_client->SendServerStateUpdate(rfbIdleInputTimeout, static_cast<CARD32>(settings->getIdleInputTimeout()));
            need_first_idletime = false;
        }

        if (need_monitor_info)
        {
            // send idletime to let the client know we accepted the encoding request
            m_client->SendMonitorInfo();
            need_monitor_info = false;
        }

        if (m_client->m_want_update_state && m_client->m_Support_rfbSetServerInput)
        {
            m_client->m_want_update_state = false;
            m_client->SendServerStateUpdate(m_client->m_state, m_client->m_value);
#ifdef _DEBUG
            std::array<char, 256U> szText;
            ::sprintf_s(szText.data(), szText.size(), "SendServerStateUpdate %i %i  \n", m_client->m_state, m_client->m_value);
            ::OutputDebugString(szText.data());
#endif
        }
        if (need_ft_version_msg)
        {
            // send a File Transfer Protocol message to client.
            m_client->SendFTProtocolMsg();
            need_ft_version_msg = false;
        }
        // adzm - 2010-07 - Extended clipboard
        if (need_notify_extended_clipboard)
        {
            m_client->NotifyExtendedClipboardSupport();
            need_notify_extended_clipboard = false;
        }
        // adzm 2010-09 - Notify streaming DSM plugin support
        if (need_notify_streaming_DSM)
        {
            m_client->NotifyPluginStreamingSupport();
            need_notify_streaming_DSM = false;
        }
        // Resume an ongoing file upload (server -> viewer) that yielded to incoming messages
        if (m_client->m_fFileUploadRunning)
        {
            m_client->cl_connected = m_client->SendFileChunk();
            if (!m_client->cl_connected)
            {
                break;
            }
        }

        // sf@2002 - v1.1.2
        int nTO{ 1 }; // Type offset
        // If DSM Plugin, we must read all the transformed incoming rfb messages (type included)
        // adzm 2010-09
        if (!m_socket->IsPluginStreamingIn() && m_socket->IsUsePluginEnabled() && m_server->GetDSMPluginPointer()->IsEnabled())
        {
            if (!m_socket->ReadExact(reinterpret_cast<char*>(&msg.type), sizeof(msg.type)))
            {
                m_client->cl_connected = FALSE;
                break;
            }
            nTO = 0;
        }
        else
        {
            // Try to read a message ID
            if (!m_socket->ReadExact(reinterpret_cast<char*>(&msg.type), sizeof(msg.type)))
            {
                vnclog.Print(LL_CONNERR, VNCLOG("client loop: msg.type read failed\n"));
                m_client->cl_connected = FALSE;
                break;
            }
        }
        /*#ifdef _DEBUG
                char			szText[256];
                sprintf_s(szText, " msg.type %i \n", msg.type);
                OutputDebugString(szText);
        #endif*/
        // What to do is determined by the message id
        switch (msg.type)
        {
        case rfbKeepAlive:
            need_keepalive = true;
            break;

        case rfbSetPixelFormat:
            // Read the rest of the message:
            if (!m_socket->ReadExact((reinterpret_cast<char*>(&msg)) + nTO, static_cast<VCard>(sz_rfbSetPixelFormatMsg - nTO)))
            {
                m_client->cl_connected = FALSE;
                break;
            }

            // Swap the relevant bits.
            msg.spf.format.redMax = Swap16IfLE(msg.spf.format.redMax);
            msg.spf.format.greenMax = Swap16IfLE(msg.spf.format.greenMax);
            msg.spf.format.blueMax = Swap16IfLE(msg.spf.format.blueMax);

            // sf@2005 - Additional param for Grey Scale transformation
            m_client->m_encodemgr.EnableGreyPalette((msg.spf.format.pad1 == 1));

            // Prevent updates while the pixel format is changed
            m_client->DisableProtocol();

            // Tell the buffer object of the change
            if (!m_client->m_encodemgr.SetClientFormat(msg.spf.format))
            {
                vnclog.Print(LL_CONNERR, VNCLOG("remote pixel format invalid\n"));

                m_client->cl_connected = FALSE;
            }

            // Set the palette-changed flag, just in case...
            m_client->m_palettechanged = TRUE;

            // Re-enable updates
            m_client->EnableProtocol();

            break;

        case rfbSetEncodings:
            // Read the rest of the message:
            if (!m_socket->ReadExact((reinterpret_cast<char*>(&msg)) + nTO, static_cast<VCard>(sz_rfbSetEncodingsMsg - nTO)))
            {
                m_client->cl_connected = FALSE;
                break;
            }

            // RDV cache
            m_client->m_encodemgr.EnableCache(FALSE);

            // RDV XOR and client detection
            m_client->m_encodemgr.AvailableQueueEnabled(FALSE);
            m_client->m_encodemgr.AvailableZRLE(FALSE);
#ifdef _XZ
            m_client->m_encodemgr.AvailableXZ(FALSE);
#endif
            m_client->m_encodemgr.AvailableTight(FALSE);

            // sf@2002 - Tight
            m_client->m_encodemgr.SetQualityLevel(-1);
            m_client->m_encodemgr.SetFineQualityLevel(-1);
            m_client->m_encodemgr.SetCompressLevel(6);
            m_client->m_encodemgr.EnableLastRect(FALSE);

            // Tight - CURSOR HANDLING
            m_client->m_encodemgr.EnableXCursor(FALSE);
            m_client->m_encodemgr.EnableRichCursor(FALSE);
            m_client->m_use_PointerPos = FALSE;
            m_server->EnableXRichCursor(FALSE);
            m_client->m_cursor_update_pending = FALSE;
            m_client->m_cursor_update_sent = FALSE;

            // Prevent updates while the encoder is changed
            m_client->DisableProtocol();

            // Read in the preferred encodings
            msg.se.nEncodings = Swap16IfLE(msg.se.nEncodings);
            {
                int x;
                BOOL encoding_set = FALSE;
                bool highResolutionWheelRequested{ false };
                bool serverStateRequested{ false };
#ifdef _Gii
                BOOL gii_set = FALSE;
#endif
                // By default, don't use copyrect!
                m_client->m_update_tracker.enable_copyrect(false);

                for (x = 0; x < msg.se.nEncodings; ++x)
                {
                    CARD32 encoding = 0;

                    // Read an encoding in
                    if (!m_socket->ReadExact(reinterpret_cast<char*>(&encoding), sizeof(encoding)))
                    {
                        m_client->cl_connected = FALSE;
                        break;
                    }

                    // Is this the CopyRect encoding (a special case)?
                    if (Swap32IfLE(encoding) == rfbEncodingCopyRect)
                    {
                        m_client->m_update_tracker.enable_copyrect(true);
                        continue;
                    }

                    // Is this a NewFBSize encoding request?
                    if (Swap32IfLE(encoding) == rfbEncodingNewFBSize)
                    {
                        m_client->m_use_NewSWSize = TRUE;
                        continue;
                    }

                    if (Swap32IfLE(encoding) == rfbEncodingExtDesktopSize)
                    {
                        // only allow desktop resize when to the first client
                        m_client->m_use_ExtDesktopSize = true;
                        continue;
                    }

                    // CACHE RDV
                    if (Swap32IfLE(encoding) == rfbEncodingCacheEnable)
                    {
                        m_client->m_encodemgr.EnableCache(TRUE);
                        vnclog.Print(LL_INTINFO, VNCLOG("Cache protocol extension enabled\n"));
                        continue;
                    }

                    // XOR zlib
                    if (Swap32IfLE(encoding) == rfbEncodingQueueEnable)
                    {
                        m_client->m_encodemgr.AvailableQueueEnabled(TRUE);
                        vnclog.Print(LL_INTINFO, VNCLOG("XOR protocol extension enabled\n"));
                        continue;
                    }

                    if (Swap32IfLE(encoding) == rfbEncodingHighResolutionWheel)
                    {
                        highResolutionWheelRequested = true;
                        continue;
                    }

                    if (Swap32IfLE(encoding) == rfbEncodingUnicodeTextChat)
                    {
                        m_client->m_supportsUnicodeTextChat = true;
                        // Send ack to viewer so it knows server supports Unicode chat
                        m_client->SendServerStateUpdate(rfbUnicodeTextChatState, rfbServerState_Enabled);
                        vnclog.Print(LL_INTINFO, VNCLOG("Unicode Text Chat protocol extension enabled\n"));
                        continue;
                    }

                    if (Swap32IfLE(encoding) == rfbEncodingChatFileTransfer)
                    {
                        m_client->m_supportsChatFileTransfer = true;
                        continue;
                    }

                    // Is this a CompressLevel encoding?
                    if ((Swap32IfLE(encoding) >= rfbEncodingCompressLevel0) && (Swap32IfLE(encoding) <= rfbEncodingCompressLevel9))
                    {
                        // Client specified encoding-specific compression level
                        int level = static_cast<int>((Swap32IfLE(encoding) - rfbEncodingCompressLevel0));
                        m_client->m_encodemgr.SetCompressLevel(level);
                        vnclog.Print(LL_INTINFO, VNCLOG("compression level requested: %d\n"), level);
                        continue;
                    }

                    // Is this a QualityLevel encoding?
                    if ((Swap32IfLE(encoding) >= rfbEncodingQualityLevel0) && (Swap32IfLE(encoding) <= rfbEncodingQualityLevel9))
                    {
                        // Client specified image quality level used for JPEG compression
                        int level = static_cast<int>((Swap32IfLE(encoding) - rfbEncodingQualityLevel0));
                        m_client->m_encodemgr.SetQualityLevel(level);
                        vnclog.Print(LL_INTINFO, VNCLOG("image quality level requested: %d\n"), level);
                        continue;
                    }

                    // Is this a FineQualityLevel encoding?
                    if ((Swap32IfLE(encoding) >= rfbEncodingFineQualityLevel0) &&
                        (Swap32IfLE(encoding) <= rfbEncodingFineQualityLevel100))
                    {
                        // Client specified fine-grained image quality level used for JPEG compression
                        int level = static_cast<int>((Swap32IfLE(encoding) - rfbEncodingFineQualityLevel0));
                        m_client->m_encodemgr.SetFineQualityLevel(level);
                        vnclog.Print(LL_INTINFO, VNCLOG("fine-grained image quality level requested: %d\n"), level);
                        continue;
                    }

                    // Is this a Subsamp encoding?
                    if ((Swap32IfLE(encoding) >= rfbEncodingSubsamp1X) && (Swap32IfLE(encoding) <= rfbEncodingSubsamp16X))
                    {
                        // Client specified subsampling used for JPEG compression
                        int subsamp = static_cast<int>((Swap32IfLE(encoding) - rfbEncodingSubsamp1X));
                        m_client->m_encodemgr.SetSubsampling(static_cast<subsamp_type>(subsamp));
                        vnclog.Print(LL_INTINFO, VNCLOG("subsampling requested: %d\n"), subsamp);
                        continue;
                    }

                    // Is this a LastRect encoding request?
                    if (Swap32IfLE(encoding) == rfbEncodingLastRect)
                    {
                        m_client->m_encodemgr.EnableLastRect(TRUE); // We forbid Last Rect for now
                        vnclog.Print(LL_INTINFO, VNCLOG("LastRect protocol extension enabled\n"));
                        continue;
                    }

                    // Is this an XCursor encoding request?
                    if (Swap32IfLE(encoding) == rfbEncodingXCursor)
                    {
                        m_client->m_encodemgr.EnableXCursor(TRUE);
                        m_server->EnableXRichCursor(TRUE);
                        m_client->m_encodemgr.m_buffer->m_desktop->requestMouseShapeUpdate();
                        vnclog.Print(LL_INTINFO, VNCLOG("X-style cursor shape updates enabled\n"));
                        continue;
                    }

                    // Is this a RichCursor encoding request?
                    if (Swap32IfLE(encoding) == rfbEncodingRichCursor)
                    {
                        m_client->m_encodemgr.EnableRichCursor(TRUE);
                        m_server->EnableXRichCursor(TRUE);
                        m_client->m_encodemgr.m_buffer->m_desktop->requestMouseShapeUpdate();
                        vnclog.Print(LL_INTINFO, VNCLOG("Full-color cursor shape updates enabled\n"));
                        continue;
                    }

                    // Is this a PointerPos encoding request? nyama/marscha - PointerPos
                    if (Swap32IfLE(encoding) == rfbEncodingPointerPos)
                    {
                        m_client->m_use_PointerPos = TRUE;
                        vnclog.Print(LL_INTINFO, VNCLOG("PointerPos protocol extension enabled\n"));
                        continue;
                    }
                    // 21 March 2008 jdp - client wants server state updates
                    if (Swap32IfLE(encoding) == rfbEncodingServerState)
                    {
                        serverStateRequested = true;
                        m_client->m_wants_ServerStateUpdates = true;
                        settings->EnableServerStateUpdates(true);
                        vnclog.Print(LL_INTINFO, VNCLOG("ServerState protocol extension enabled\n"));
                        continue;
                    }
                    // 21 March 2008 jdp  - client wants keepalive messages
                    if (Swap32IfLE(encoding) == rfbEncodingpseudoSession)
                    {
                        m_client->m_session_supported = true;
                        vnclog.Print(LL_INTINFO, VNCLOG("KeepAlive protocol extension enabled\n"));
                        continue;
                    }

                    if (Swap32IfLE(encoding) == rfbEncodingEnableKeepAlive)
                    {
                        m_client->m_wants_KeepAlive = true;
                        settings->EnableKeepAlives(true);
                        need_first_keepalive = true;
                        vnclog.Print(LL_INTINFO, VNCLOG("KeepAlive protocol extension enabled\n"));
                        continue;
                    }

                    if (Swap32IfLE(encoding) == rfbEncodingEnableIdleTime)
                    {
                        need_first_idletime = true;
                        vnclog.Print(LL_INTINFO, VNCLOG("IdleTime protocol extension enabled\n"));
                        continue;
                    }

                    if (Swap32IfLE(encoding) == rfbEncodingMonitorInfo)
                    {
                        need_monitor_info = true;
                        vnclog.Print(LL_INTINFO, VNCLOG("IdleTime protocol extension enabled\n"));
                        continue;
                    }

                    if (Swap32IfLE(encoding) == rfbEncodingFTProtocolVersion)
                    {
                        need_ft_version_msg = true;
                        m_client->m_fClientSupportsUnicode = true; // Client supports FT_PROTO_VERSION_4 (UTF-8)
                        vnclog.Print(LL_INTINFO, VNCLOG("FTProtocolVersion protocol extension enabled\n"));
                        continue;
                    }

                    // adzm - 2010-07 - Extended clipboard
                    if (Swap32IfLE(encoding) == rfbEncodingExtendedClipboard)
                    {
                        need_notify_extended_clipboard = true;
                        m_client->m_clipboard.settings.m_bSupportsEx = true;
                        vnclog.Print(LL_INTINFO, VNCLOG("Extended clipboard protocol extension enabled\n"));
                        continue;
                    }

                    // adzm 2010-09 - Notify streaming DSM plugin support
                    if (Swap32IfLE(encoding) == rfbEncodingPluginStreaming)
                    {
                        need_notify_streaming_DSM = true;
                        vnclog.Print(LL_INTINFO, VNCLOG("Streaming DSM support enabled\n"));
                        continue;
                    }

#ifdef _Gii
                    if (Swap32IfLE(encoding) == rfbEncodingGII)
                    {
                        vnclog.Print(LL_INTINFO, VNCLOG("Gii Encoding found\n"));
                        gii_set = TRUE;
                        continue;
                    }
#endif

                    // RDV - We try to detect which type of viewer tries to connect
                    if (Swap32IfLE(encoding) == rfbEncodingZRLE)
                    {
                        m_client->m_encodemgr.AvailableZRLE(TRUE);
                        vnclog.Print(LL_INTINFO, VNCLOG("ZRLE found \n"));
                    }
#ifdef _XZ
                    if (Swap32IfLE(encoding) == rfbEncodingXZ)
                    {
                        m_client->m_encodemgr.AvailableXZ(TRUE);
                        vnclog.Print(LL_INTINFO, VNCLOG("XZ found \n"));
                    }
#endif

                    if (Swap32IfLE(encoding) == rfbEncodingTight)
                    {
                        m_client->m_encodemgr.AvailableTight(TRUE);
                        vnclog.Print(LL_INTINFO, VNCLOG("Tight found\n"));
                    }

                    // Have we already found a suitable encoding?
                    if (!encoding_set)
                    {
                        // No, so try the buffer to see if this encoding will work...
                        omni_mutex_lock l(m_client->GetUpdateLock(), 85);
                        if (m_client->m_encodemgr.SetEncoding(Swap32IfLE(encoding), FALSE))
                        {
                            encoding_set = TRUE;
                        }
                    }
                }
                // If no encoding worked then default to RAW!
                if (!encoding_set)
                {
                    vnclog.Print(LL_INTINFO, VNCLOG("defaulting to raw encoder\n"));
                    omni_mutex_lock l(m_client->GetUpdateLock(), 86);
                    if (!m_client->m_encodemgr.SetEncoding(Swap32IfLE(rfbEncodingRaw), FALSE))
                    {
                        vnclog.Print(LL_INTERR, VNCLOG("failed to select raw encoder!\n"));

                        m_client->cl_connected = FALSE;
                    }
                }

                // Acknowledge only after reading the entire list, regardless of order.
                if (m_client->cl_connected && highResolutionWheelRequested && serverStateRequested)
                {
                    m_client->m_supportsHighResolutionWheel = true;
                    m_client->SendServerStateUpdate(rfbHighResolutionWheelState, rfbServerState_Enabled);
                }

                if (!m_client->m_use_PointerPos)
                {
                    m_client->m_use_PointerPos = FALSE;
                    if (!m_client->m_ForceCursorShape)
                    {
                        m_server->EnableXRichCursor(FALSE);
                        m_client->m_cursor_update_pending = FALSE;
                        m_client->m_cursor_update_sent = FALSE;
                        m_client->m_encodemgr.EnableXCursor(FALSE);
                        m_client->m_encodemgr.EnableRichCursor(FALSE);
                    }
                }

                // sf@2002 - For now we disable cache protocol when more than one client are connected
                // (But the cache buffer (if exists) is kept intact (for XORZlib usage))
                if (m_server->AuthClientCount() > 1)
                {
                    m_server->DisableCacheForAllClients();
                }
#ifdef _Gii
                // Gii encoding requested (MC Multitouch Extensions)
                if (gii_set && (InjectTouchInputUVNC != nullptr) && (InitializeTouchInjectionUVNC != nullptr) &&
                    m_client->m_GiiEnabled)
                {
                    InitGiiVersion();
                }
#endif
            }

            // Re-enable updates
            m_client->client_settings_passed = true;
            m_client->EnableProtocol();
            break;

#ifdef _Gii
        case rfbGIIMessage: {
            // Read the subtype and length of the message:
            std::uint8_t subType{ 0U };
            std::uint16_t lengthMsg{ 0U };
            bool bigEndian{ false };
            omni_mutex_lock ll(m_client->GetUpdateLock(), 776);
            if (!m_socket->ReadExact(reinterpret_cast<char*>(&subType), sizeof(subType)))
            {
                vnclog.Print(LL_INTERR, VNCLOG("GII:Extension Reading subtype failed !\n"));
                m_client->cl_connected = FALSE;
                break;
            }
            if (!m_socket->ReadExact(reinterpret_cast<char*>(&lengthMsg), sizeof(lengthMsg)))
            {
                vnclog.Print(LL_INTERR, VNCLOG("GII:Extension Reading length failed !\n"));
                m_client->cl_connected = FALSE;
                break;
            }
            //unmask big endian flag
            if (subType & 0x80)
            {
                bigEndian = true;
                //format is big endian, convert to multi byte data to little
                lengthMsg = Swap16IfLE(lengthMsg);
                //mask subtype
                subType &= 0x7f;
            }
            // process Gii Message
            switch (subType)
            {
            case _rfbGIIEventInjection: {
                if (!m_client->m_GiiEnabled)
                {
                    vnclog.Print(LL_INTERR, VNCLOG("GIIExtension not supported in readonly mode\n"));
                    m_client->cl_connected = FALSE;
                    break;
                }
                rfbGIIValuatorEventMsg rfbGIIValutorEvent{};
                rfbGIIValutorEvent.header.messageType = rfbGIIMessage;
                rfbGIIValutorEvent.header.subType = subType;
                rfbGIIValutorEvent.header.length = lengthMsg;
                // read rest of client's inject data
                if (!m_socket->ReadExact(reinterpret_cast<char*>((&rfbGIIValutorEvent)) + sz_rfbGIIMsgHeader,
                                         sizeof(rfbGIIValutorEvent) - sz_rfbGIIMsgHeader))
                {
                    vnclog.Print(LL_INTERR, VNCLOG("GIIExtension reading client creation failed\n"));
                    m_client->cl_connected = FALSE;
                    break;
                }
                if (bigEndian)
                {
                    rfbGIIValutorEvent.padding = Swap16IfLE(rfbGIIValutorEvent.padding);
                    rfbGIIValutorEvent.deviceOrigin = Swap32IfLE(rfbGIIValutorEvent.deviceOrigin);
                    rfbGIIValutorEvent.first = Swap32IfLE(rfbGIIValutorEvent.first);
                    rfbGIIValutorEvent.count = Swap32IfLE(rfbGIIValutorEvent.count);
                }
                /////////////////////////////////////////////////////////////////////////////////////////
                /////////////////////////////////////////////////////////////////////////////////////////
                /////////////////////////////////////////////////////////////////////////////////////////
                if ((rfbGIIValutorEvent.first == 0) || (rfbGIIValutorEvent.first > 254))
                {
                    vnclog.Print(LL_INTERR, VNCLOG("GII: invalid touch point count %u\n"), rfbGIIValutorEvent.first);
                    m_client->cl_connected = FALSE;
                    break;
                }
#ifdef _USE_DLL
                if (DLL_PInjectTouch)
#endif
                {
                    MyTouchINfo* TI = nullptr;
                    TI = new MyTouchINfo[rfbGIIValutorEvent.first];
                    for (unsigned int j{ 0U }; j < rfbGIIValutorEvent.first; ++j)
                    {
                        DWORD ValuatorFlag{ 0U };
                        DWORD ValuatorTouch_Id{ 0U };
                        DWORD ValuatorTouch_Position16XY{ 0U };
                        DWORD ValuatorTouch_Area{ 0U };
                        DWORD ValuatorTouch_Pressure{ 0U };
                        DWORD ValuatorTime_msec{ 0U };
                        DWORD64 ValuatorTime_usec{ 0U };

                        if (!m_socket->ReadExact(reinterpret_cast<char*>(&ValuatorFlag), sizeof(DWORD)))
                        {
                            vnclog.Print(LL_INTERR, VNCLOG("GIIExtension reading TouchINfo failed\n"));
                            m_client->cl_connected = FALSE;
                            break;
                        }
                        if (bigEndian)
                        {
                            ValuatorFlag = Swap32IfLE(ValuatorFlag);
                        }

                        if (!m_socket->ReadExact(reinterpret_cast<char*>(&ValuatorTouch_Id), sizeof(DWORD)))
                        {
                            vnclog.Print(LL_INTERR, VNCLOG("GIIExtension reading TouchINfo failed\n"));
                            m_client->cl_connected = FALSE;
                            break;
                        }
                        if (bigEndian)
                        {
                            ValuatorTouch_Id = Swap32IfLE(ValuatorTouch_Id);
                        }
                        TI[j].TouchId = ValuatorTouch_Id;

                        if (!m_socket->ReadExact(reinterpret_cast<char*>(&ValuatorTouch_Position16XY), sizeof(DWORD)))
                        {
                            vnclog.Print(LL_INTERR, VNCLOG("GIIExtension reading TouchINfo failed\n"));
                            m_client->cl_connected = FALSE;
                            break;
                        }
                        if (bigEndian)
                        {
                            ValuatorTouch_Position16XY = Swap32IfLE(ValuatorTouch_Position16XY);
                        }
                        if (ValuatorFlag & LANGE_16_flag)
                        {
                            TI[j].X = ((ValuatorTouch_Position16XY & 0xffff0000) >> 16);
                            TI[j].Y = ((ValuatorTouch_Position16XY & 0x0000ffff));
                        }
                        else
                        {
                            vnclog.Print(LL_INTERR, VNCLOG("Only 16-bit XT supported\n"));
                            m_client->cl_connected = FALSE;
                            break;
                        }

                        if (ValuatorFlag & S1_flag)
                        {
                            if (!m_socket->ReadExact(reinterpret_cast<char*>(&ValuatorTouch_Area), sizeof(DWORD)))
                            {
                                vnclog.Print(LL_INTERR, VNCLOG("GIIExtension reading TouchINfo failed\n"));
                                m_client->cl_connected = FALSE;
                                break;
                            }
                            if (bigEndian)
                            {
                                ValuatorTouch_Area = Swap32IfLE(ValuatorTouch_Area);
                            }
                            TI[j].ContactWidth = ((ValuatorTouch_Area & 0xffff0000) >> 16);
                            TI[j].ContactHeight = ((ValuatorTouch_Area & 0x0000ffff));
                        }
                        else
                        {
                            TI[j].ContactWidth = 4;
                            TI[j].ContactHeight = 4;
                        }

                        if (ValuatorFlag & PR_flag)
                            if (!m_socket->ReadExact(reinterpret_cast<char*>(&ValuatorTouch_Pressure), sizeof(DWORD)))
                            {
                                vnclog.Print(LL_INTERR, VNCLOG("GIIExtension reading TouchINfo failed\n"));
                                m_client->cl_connected = FALSE;
                                break;
                            }
                        if (bigEndian)
                        {
                            ValuatorTouch_Pressure = Swap32IfLE(ValuatorTouch_Pressure);
                        }

                        if (ValuatorFlag & TI_flag)
                            if (!m_socket->ReadExact(reinterpret_cast<char*>(&ValuatorTime_msec), sizeof(DWORD)))
                            {
                                vnclog.Print(LL_INTERR, VNCLOG("GIIExtension reading TouchINfo failed\n"));
                                m_client->cl_connected = FALSE;
                                break;
                            }
                        if (bigEndian)
                        {
                            ValuatorTime_msec = Swap32IfLE(ValuatorTime_msec);
                        }

                        if (ValuatorFlag & HC_flag)
                            if (!m_socket->ReadExact(reinterpret_cast<char*>(&ValuatorTime_usec), sizeof(DWORD64)))
                            {
                                vnclog.Print(LL_INTERR, VNCLOG("GIIExtension reading TouchINfo failed\n"));
                                m_client->cl_connected = FALSE;
                                break;
                            }
                        const int endianTest{ 1 };
                        if (bigEndian)
                        {
                            ValuatorTime_usec = static_cast<DWORD64>(Swap64IfLE(ValuatorTime_usec));
                        }

                        TI[j].pointerflag = POINTER_FLAG_NONE;

                        if (point_status && (TI[j].TouchId < nr_points)) // protect point_status array, else wrong value crash
                        {
                            if (ValuatorFlag & PF_flag)
                            {
                                if (point_status[TI[j].TouchId] == 1)
                                {
                                    //move
                                    TI[j].pointerflag |= POINTER_FLAG_UPDATE;
                                    TI[j].pointerflag |= POINTER_FLAG_INRANGE | POINTER_FLAG_INCONTACT;
                                }
                                else
                                {
                                    //down
                                    TI[j].pointerflag |= POINTER_FLAG_DOWN;
                                    TI[j].pointerflag |= POINTER_FLAG_INRANGE | POINTER_FLAG_INCONTACT;
                                    point_status[TI[j].TouchId] = 1;
                                }
                            }
                            else
                            {
                                // UP
                                TI[j].pointerflag |= POINTER_FLAG_UP;
                                point_status[TI[j].TouchId] = 0;
                            }
                        }

                        TI[j].time = 0;
                    }
#ifdef _USE_DLL
                    bRet = ::DLL_PInjectTouch(rfbGIIValutorEvent.first, TI);
#ifdef _DEBUG
                    if (bRet == 0)
                    {
                        ::sprintf_s(szText.data(), szText.size(), "FAIL index %d %i\n", ::GetLastError(), rfbGIIValutorEvent.first);
                    }
                    else
                    {
                        ::sprintf_s(szText.data(), szText.size(), "OK %i\n", rfbGIIValutorEvent.first);
                    }
                    ::OutputDebugString(szText.data());
#endif
#else
                    //////////////////////////////////////////////////
                    // Copy src from dll
                    //////////////////////////////////////////////////
                    MyTouchINfo* ti_array = TI;
                    BOOL value = FALSE;
                    POINTER_TOUCH_INFO* contact = nullptr;
                    if (rfbGIIValutorEvent.first > 254)
                    {
                        goto mydllend;
                    } // early check above guarantees this, kept as defense-in-depth
                    contact = new POINTER_TOUCH_INFO[rfbGIIValutorEvent.first];
                    if (contact == nullptr)
                    {
                        goto mydllend;
                    }
                    std::memset(contact, 0, sizeof(POINTER_TOUCH_INFO) * rfbGIIValutorEvent.first);

                    for (unsigned int i{ 0U }; i < rfbGIIValutorEvent.first; ++i)
                    {
                        contact[i].pointerInfo.pointerType = PT_TOUCH; //we're sending touch input
                        contact[i].pointerInfo.ptPixelLocation.x = ti_array[i].X;
                        contact[i].pointerInfo.ptPixelLocation.y = ti_array[i].Y;

                        contact[i].pointerInfo.dwTime = ti_array[i].time;

                        contact[i].touchFlags = TOUCH_FLAG_NONE;
                        contact[i].touchMask = TOUCH_MASK_CONTACTAREA | TOUCH_MASK_ORIENTATION | TOUCH_MASK_PRESSURE;
                        contact[i].orientation = 90;
                        contact[i].pressure = 32000;

                        contact[i].rcContact.top = contact[i].pointerInfo.ptPixelLocation.y - ti_array[i].ContactHeight / 2;
                        contact[i].rcContact.bottom = contact[i].pointerInfo.ptPixelLocation.y + ti_array[i].ContactHeight / 2;
                        contact[i].rcContact.left = contact[i].pointerInfo.ptPixelLocation.x - ti_array[i].ContactWidth / 2;
                        contact[i].rcContact.right = contact[i].pointerInfo.ptPixelLocation.x + ti_array[i].ContactWidth / 2;

                        contact[i].pointerInfo.pointerId = ti_array[i].TouchId; //contact 0
                        contact[i].pointerInfo.pointerFlags = ti_array[i].pointerflag;
                    }
                    value = InjectTouchInputUVNC(rfbGIIValutorEvent.first, contact);
#ifdef _DEBUG
                    if (value == 0)
                    {
                        ::sprintf_s(szText.data(), szText.size(), "FAIL index %d %i\n", ::GetLastError(), rfbGIIValutorEvent.first);
                    }
                    else
                    {
                        ::sprintf_s(szText.data(), szText.size(), "OK number points %i\n", rfbGIIValutorEvent.first);
                    }
                    ::OutputDebugString(szText.data());
#endif
                    delete[] contact;
                mydllend:
                    //////////////////////////////////////////////////
#endif
                    if (TI)
                    {
                        delete[] TI;
                    }
                }
                /////////////////////////////////////////////////////////////////////////////////////////
                /////////////////////////////////////////////////////////////////////////////////////////
                /////////////////////////////////////////////////////////////////////////////////////////
            }
            break;

            case _rfbGIIVersionMessage: {
                if (!m_client->m_GiiEnabled)
                {
                    vnclog.Print(LL_INTERR, VNCLOG("GIIExtension not supported in readonly mode\n"));
                    m_client->cl_connected = FALSE;
                    return;
                }
                std::uint16_t clientVersion{ 0U };
                //check length
                vnclog.Print(LL_INTINFO, VNCLOG("GII: Get Subtype _rfbGIIVersionMessage \n"));
                if (lengthMsg != sizeof(clientVersion))
                {
                    vnclog.Print(LL_INTERR, VNCLOG("GII:Extension invalid length: %d\n"), clientVersion);
                    m_client->cl_connected = FALSE;
                    break;
                }
                // read client's version info and process
                if (!m_socket->ReadExact(reinterpret_cast<char*>(&clientVersion), sizeof(clientVersion)))
                {
                    vnclog.Print(LL_INTERR, VNCLOG("GIIExtension reading client version failed\n"));
                    m_client->cl_connected = FALSE;
                    break;
                }
                if (bigEndian)
                {
                    clientVersion = Swap16IfLE(clientVersion);
                }
                if (((clientVersion < rfbGIIMinVersion) || (clientVersion > rfbGIIMaxVersion))) //TBR
                {
                    vnclog.Print(LL_INTERR, VNCLOG("GII: Extension client version invalid number: %d\n"), clientVersion);
                    m_client->cl_connected = FALSE;
                    break;
                }
                break;
            }

            case _rfbGIIDeviceCreation: {
                if (!m_client->m_GiiEnabled)
                {
                    vnclog.Print(LL_INTERR, VNCLOG("GIIExtension not supported in readonly mode\n"));
                    m_client->cl_connected = FALSE;
                    break;
                }
                rfbGIIClientDeviceCreationMsg rfbGIIClientDeviceCreation{};
                //prepare already known data
                omni_mutex_lock llInner(m_client->GetUpdateLock(), 777);
                vnclog.Print(LL_INTINFO, VNCLOG("GII: Get Subtype _rfbGIIDeviceCreation \n"));
                rfbGIIClientDeviceCreation.header.messageType = rfbGIIMessage;
                rfbGIIClientDeviceCreation.header.subType = subType;
                rfbGIIClientDeviceCreation.header.length = lengthMsg;
                // read rest of client's creation data
                if (!m_socket->ReadExact(reinterpret_cast<char*>((&rfbGIIClientDeviceCreation)) + sz_rfbGIIMsgHeader,
                                         sizeof(rfbGIIClientDeviceCreation) - sz_rfbGIIMsgHeader))
                {
                    vnclog.Print(LL_INTERR, VNCLOG("GIIExtension reading client creation failed\n"));
                    m_client->cl_connected = FALSE;
                    break;
                }
                if (bigEndian)
                {
                    rfbGIIClientDeviceCreation.vendorID = Swap32IfLE(rfbGIIClientDeviceCreation.vendorID);
                    rfbGIIClientDeviceCreation.productID = Swap32IfLE(rfbGIIClientDeviceCreation.productID);
                    rfbGIIClientDeviceCreation.eventMask = Swap32IfLE(rfbGIIClientDeviceCreation.eventMask);
                    rfbGIIClientDeviceCreation.numRegisters = Swap32IfLE(rfbGIIClientDeviceCreation.numRegisters);
                    rfbGIIClientDeviceCreation.numValuators = Swap32IfLE(rfbGIIClientDeviceCreation.numValuators);
                    rfbGIIClientDeviceCreation.numButtons = Swap32IfLE(rfbGIIClientDeviceCreation.numButtons);
                    rfbGIIClientDeviceCreation.vendorID = Swap32IfLE(rfbGIIClientDeviceCreation.vendorID);
                }
                if ((rfbGIIClientDeviceCreation.numValuators == 0) || (rfbGIIClientDeviceCreation.numValuators > 10))
                {
                    vnclog.Print(LL_INTERR, VNCLOG("GIIExtension reading device creation, invalid amount of valuators\n"));
                    m_client->cl_connected = FALSE;
                    break;
                }
                else
                {
                    //read all valutor
                    rfbGIIValuatorMsg rfbGIIValutor{};
                    for (unsigned int i{ 0U }; i < rfbGIIClientDeviceCreation.numValuators; ++i)
                    {
                        if (!m_socket->ReadExact(reinterpret_cast<char*>(&rfbGIIValutor), sizeof(rfbGIIValutor)))
                        {
                            vnclog.Print(LL_INTERR, VNCLOG("GIIExtension reading device creation valuators failed\n"));
                            m_client->cl_connected = FALSE;
                            break;
                        }
                        else
                        {
                            if (bigEndian)
                            {
                                rfbGIIValutor.index = Swap32IfLE(rfbGIIValutor.index);
                                rfbGIIValutor.rangeMin = Swap32IfLE(rfbGIIValutor.rangeMin);
                                rfbGIIValutor.rangeCenter = Swap32IfLE(rfbGIIValutor.rangeCenter);
                                rfbGIIValutor.rangeMax = Swap32IfLE(rfbGIIValutor.rangeMax);
                                rfbGIIValutor.SIUnit = Swap32IfLE(rfbGIIValutor.SIUnit);
                                rfbGIIValutor.SIAdd = Swap32IfLE(rfbGIIValutor.SIAdd);
                                rfbGIIValutor.SIMul = Swap32IfLE(rfbGIIValutor.SIMul);
                                rfbGIIValutor.SIDiv = Swap32IfLE(rfbGIIValutor.SIDiv);
                                rfbGIIValutor.SIShift = Swap32IfLE(rfbGIIValutor.SIShift);
                            }
                            //send device origin //TBR
                            rfbGIIServerDeviceCreationMsg rfbGIIServerDeviceCreation{};
                            // Generate the server's creation message
                            rfbGIIServerDeviceCreation.header.messageType = rfbGIIMessage;
                            rfbGIIServerDeviceCreation.header.subType = rfbGIIDeviceCreation;
                            //rfbGIIServerDeviceCreation.header.subType =  2; //no use of little endian
                            rfbGIIServerDeviceCreation.header.length = 4;
                            rfbGIIServerDeviceCreation.header.length = Swap16IfLE(rfbGIIServerDeviceCreation.header.length);
                            rfbGIIServerDeviceCreation.deviceOrigin = 1; //TBR
                            rfbGIIServerDeviceCreation.deviceOrigin = Swap32IfLE(rfbGIIServerDeviceCreation.deviceOrigin);
                            // Send our protocol version, and get the client's protocol version

                            vnclog.Print(LL_INTERR, VNCLOG("GIIExtension writing device creation\n"));
                            if (!m_socket->SendExactQueue(reinterpret_cast<char*>(&rfbGIIServerDeviceCreation), sz_rfbGIIServerDeviceCreationMsg))
                            {
                                vnclog.Print(LL_INTERR, VNCLOG("GIIExtension writing device creation failed\n"));
                                m_client->cl_connected = FALSE;
                                break;
                            }

                            if ((rfbGIIClientDeviceCreation.numButtons == 0) || (rfbGIIClientDeviceCreation.numButtons >= 254))
                            {
                                vnclog.Print(LL_INTERR, VNCLOG("GII: invalid numButtons %u\n"),
                                             rfbGIIClientDeviceCreation.numButtons);
                                m_client->cl_connected = FALSE;
                                break;
                            }
#ifdef _USE_DLL
                            if (DLL_InitializeTouchInjection)
                            {
                                DLL_InitializeTouchInjection(rfbGIIClientDeviceCreation.numButtons);
                            }
#else
                            InitializeTouchInjectionUVNC(rfbGIIClientDeviceCreation.numButtons, TOUCH_FEEDBACK_DEFAULT);
#endif
                            if (point_status)
                            {
                                delete[] point_status;
                                point_status = nullptr;
                            }
                            nr_points = rfbGIIClientDeviceCreation.numButtons;
                            point_status = new bool[nr_points](); // value-initialized to false
                        }
                    }
                }
                break;
            }

            case _rfbGIIDeviceDestruction:
                if (!m_client->m_GiiEnabled)
                {
                    vnclog.Print(LL_INTERR, VNCLOG("GIIExtension not supported in readonly mode\n"));
                    m_client->cl_connected = FALSE;
                    break;
                }
                //not yet implemented
                vnclog.Print(LL_INTERR, VNCLOG("GIIExtension writing device creation failed\n"));
                m_client->cl_connected = FALSE;
                break;

            default:
                vnclog.Print(LL_INTERR, VNCLOG("GII: Extension invalid subtype: %d\n"), subType);
                m_client->cl_connected = FALSE;
                break;
            }
        }
        break;
#endif

        case rfbFramebufferUpdateRequest:
            if (!m_socket->ReadExact((reinterpret_cast<char*>(&msg)) + nTO, static_cast<VCard>(sz_rfbFramebufferUpdateRequestMsg - nTO)))
            {
                m_client->cl_connected = FALSE;
                break;
            }

            if (!m_client->NotifyUpdate(msg.fur))
            {
                m_client->cl_connected = FALSE;
            }
            break;

        case rfbKeyEvent:
            // Read the rest of the message:
            if (m_socket->ReadExact((reinterpret_cast<char*>(&msg)) + nTO, static_cast<VCard>(sz_rfbKeyEventMsg - nTO)))
            {
                if (PreConnect)
                {
                    msg.ke.key = Swap32IfLE(msg.ke.key);
                    if (msg.ke.down != 0)
                    {
                        int index = static_cast<int>(msg.ke.key - 97);
                        HANDLE hprconnectevent = nullptr;
                        if ((index > -1) && (index < 100))
                        {
                            PreConnectID = static_cast<int>(m_client->m_encodemgr.m_buffer->m_desktop->sesmsg[index].ID);
                            hprconnectevent = ::OpenEvent(EVENT_MODIFY_STATE, FALSE, "Global\\SessionEventUltraPreConnect");
                            HANDLE m_hFileMa = nullptr;
                            m_hFileMa = ::OpenFileMapping(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, "Global\\SessionUltraPreConnect");
                            PVOID data = nullptr;
                            if (m_hFileMa)
                            {
                                data = ::MapViewOfFile(m_hFileMa, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 0);
                            }
                            int* mydata = reinterpret_cast<int*>(data);
                            if (data)
                            {
                                *mydata = PreConnectID;
                            }
                            if (data)
                            {
                                ::UnmapViewOfFile(data);
                            }
                            if (m_hFileMa != nullptr)
                            {
                                ::CloseHandle(m_hFileMa);
                            }
                            if (hprconnectevent)
                            {
                                ::SetEvent(hprconnectevent);
                            }
                            if (hprconnectevent)
                            {
                                ::CloseHandle(hprconnectevent);
                            }
                        }
                    }
                }
                else
                {
                    if (m_client->m_keyboardenabled)
                    {
                        msg.ke.key = Swap32IfLE(msg.ke.key);

                        // Get the keymapper to do the work
                        // m_client->m_keymap.DoXkeysym(msg.ke.key, msg.ke.down);
                        vncKeymap::keyEvent(msg.ke.key, (0 != msg.ke.down), m_client->m_jap, m_client->m_unicode);

                        m_client->m_remoteevent = TRUE;
                    }
                }
            }
            m_client->TriggerUpdate();
            break;

        case rfbHighResolutionWheel:
        case rfbPointerEvent: {
            const bool preciseWheel = msg.type == rfbHighResolutionWheel;
            const int size = preciseWheel ? sz_rfbHighResolutionWheelMsg : sz_rfbPointerEventMsg;
            unsigned wheelAxis = rfbWheelVertical;
            int wheelDelta{ 0 };
            // Use the same DSM-aware type offset as the legacy pointer path.
            if (m_socket->ReadExact((reinterpret_cast<char*>(&msg)) + nTO, static_cast<VCard>(size - nTO)))
            {
                if (preciseWheel)
                {
                    unsigned x, y;
                    rfb::Rect bounds = m_client->m_encodemgr.m_buffer->GetViewerSize();
                    if (!m_client->m_supportsHighResolutionWheel ||
                        !DecodeHighResolutionWheel(msg.wheel, static_cast<unsigned int>(bounds.width()), static_cast<unsigned int>(bounds.height()), wheelAxis, wheelDelta, x, y))
                    {
                        vnclog.Print(LL_CONNERR, VNCLOG("Invalid or unnegotiated high-resolution wheel message\n"));
                        m_client->cl_connected = FALSE;
                        break;
                    }
                    if (!wheelDelta)
                    {
                        break;
                    }
                    // Reuse pointer permissions, collaboration, scaling and injection below.
                    // Wheel input does not change the state of any mouse button.
                    msg.pe.type = rfbPointerEvent;
                    msg.pe.buttonMask = m_client->m_ptrevent.buttonMask;
                    msg.pe.x = Swap16IfLE(x);
                    msg.pe.y = Swap16IfLE(y);
                }
                if (PreConnect)
                {
                    break;
                }
                if (m_client->m_pointerenabled)
                {
                    // Convert the coords to Big Endian
                    // Modif sf@2002 - Scaling
                    msg.pe.x = (Swap16IfLE(msg.pe.x));
                    msg.pe.y = (Swap16IfLE(msg.pe.y));
                    //Error, msd.pe.x is defined as unsigned while with a negative secondary screen it's negative
                    //offset need to be used later in this function
                    msg.pe.x = static_cast<CARD16>((msg.pe.x) * m_client->m_nScale);
                    msg.pe.y = static_cast<CARD16>((msg.pe.y) * m_client->m_nScale);

                    // Work out the flags for this event
                    DWORD flags = MOUSEEVENTF_ABSOLUTE;

                    if ((msg.pe.x != m_client->m_ptrevent.x) || (msg.pe.y != m_client->m_ptrevent.y))
                    {
                        flags |= MOUSEEVENTF_MOVE;
                    }
                    if ((msg.pe.buttonMask & rfbButton1Mask) != (m_client->m_ptrevent.buttonMask & rfbButton1Mask))
                    {
                        if (GetSystemMetrics(SM_SWAPBUTTON))
                        {
                            flags |= (msg.pe.buttonMask & rfbButton1Mask) ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
                        }
                        else
                        {
                            flags |= (msg.pe.buttonMask & rfbButton1Mask) ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
                        }
                    }
                    if ((msg.pe.buttonMask & rfbButton2Mask) != (m_client->m_ptrevent.buttonMask & rfbButton2Mask))
                    {
                        flags |= (msg.pe.buttonMask & rfbButton2Mask) ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;
                    }
                    if ((msg.pe.buttonMask & rfbButton3Mask) != (m_client->m_ptrevent.buttonMask & rfbButton3Mask))
                    {
                        if (GetSystemMetrics(SM_SWAPBUTTON))
                        {
                            flags |= (msg.pe.buttonMask & rfbButton3Mask) ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
                        }
                        else
                        {
                            flags |= (msg.pe.buttonMask & rfbButton3Mask) ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
                        }
                    }

                    if (settings->getCollabo())
                    {
                        if (((msg.pe.buttonMask & rfbButton1Mask) != (m_client->m_ptrevent.buttonMask & rfbButton1Mask)) ||
                            ((msg.pe.buttonMask & rfbButton2Mask) != (m_client->m_ptrevent.buttonMask & rfbButton2Mask)) ||
                            ((msg.pe.buttonMask & rfbButton1Mask) != (m_client->m_ptrevent.buttonMask & rfbButton1Mask)))
                        {
                            if (!m_client->has_mouse)
                            {
                                m_client->ask_mouse = true;
                                m_server->SetHasMouse();
                            }
                        }

                        if (!m_client->has_mouse)
                        {
                            int xx = msg.pe.x - GetSystemMetrics(SM_XVIRTUALSCREEN) +
                                     (m_client->monitor_Offsetx + m_client->m_ScreenOffsetx);
                            int yy = msg.pe.y - GetSystemMetrics(SM_YVIRTUALSCREEN) +
                                     (m_client->monitor_Offsety + m_client->m_ScreenOffsety);
                            if (settings->getDriver())
                            {
                                xx = msg.pe.x + m_client->monitor_Offsetx;
                                yy = msg.pe.y + m_client->monitor_Offsety;
                            }
                            if (m_client->simulateCursor)
                            {
                                m_client->simulateCursor->moveCursor(xx, yy);
                            }
                            break;
                        }
                    }

                    // Preserve precise deltas; legacy wheel buttons represent full detents.
                    DWORD wheel_movement{ 0U };
                    if (preciseWheel)
                    {
                        flags |= wheelAxis == rfbWheelVertical ? MOUSEEVENTF_WHEEL : MOUSEEVENTF_HWHEEL;
                        wheel_movement = static_cast<DWORD>(wheelDelta);
                    }
                    else if (m_client->m_encodemgr.IsMouseWheelTight())
                    {
                        if (((msg.pe.buttonMask & rfbButton4Mask) != 0) &&
                            ((m_client->m_ptrevent.buttonMask & rfbButton4Mask) == 0))
                        {
                            flags |= MOUSEEVENTF_WHEEL;
                            wheel_movement = static_cast<DWORD>(+ 120);
                        }
                        else if (((msg.pe.buttonMask & rfbButton5Mask) != 0) &&
                                 ((m_client->m_ptrevent.buttonMask & rfbButton5Mask) == 0))
                        {
                            flags |= MOUSEEVENTF_WHEEL;
                            wheel_movement = static_cast<DWORD>(-120);
                        }
                        else if (((msg.pe.buttonMask & rfbButton6Mask) != 0) &&
                                 ((m_client->m_ptrevent.buttonMask & rfbButton6Mask) == 0))
                        {
                            flags |= MOUSEEVENTF_HWHEEL;
                            wheel_movement = static_cast<DWORD>(-120);
                        }
                        else if (((msg.pe.buttonMask & rfbButton7Mask) != 0) &&
                                 ((m_client->m_ptrevent.buttonMask & rfbButton7Mask) == 0))
                        {
                            flags |= MOUSEEVENTF_HWHEEL;
                            wheel_movement = static_cast<DWORD>(+ 120);
                        }
                    }
                    else
                    {
                        // RealVNC 335 Mouse wheel support
                        if (msg.pe.buttonMask & rfbWheelUpMask)
                        {
                            flags |= MOUSEEVENTF_WHEEL;
                            wheel_movement = WHEEL_DELTA;
                        }
                        if (msg.pe.buttonMask & rfbWheelDownMask)
                        {
                            flags |= MOUSEEVENTF_WHEEL;
                            wheel_movement = static_cast<DWORD>(-WHEEL_DELTA);
                        }
                        // UltraVNC horizontal wheel
                        if (msg.pe.buttonMask & rfbWheelRightMask)
                        {
                            flags |= MOUSEEVENTF_HWHEEL;
                            wheel_movement = WHEEL_DELTA;
                        }
                        if (msg.pe.buttonMask & rfbWheelLeftMask)
                        {
                            flags |= MOUSEEVENTF_HWHEEL;
                            wheel_movement = static_cast<DWORD>(-WHEEL_DELTA);
                        }
                    }

                    // Generate coordinate values
                    // bug fix John Latino
                    // offset for multi display
                    int screenX, screenY, screenDepth;
                    m_server->GetScreenInfo(screenX, screenY, screenDepth);
                    // 1 , only one display, so always positive
                    //primary display always have (0,0) as corner
                    if (m_client->m_single_display)
                    {
                        unsigned long x = static_cast<unsigned long>(((msg.pe.x + (m_client->monitor_Offsetx)) * 65535) / (screenX - 1));
                        unsigned long y = static_cast<unsigned long>(((msg.pe.y + (m_client->monitor_Offsety)) * 65535) / (screenY - 1));
                        // Do the pointer event
                        if (preciseWheel)
                        {
                            INPUT evt{};
                            evt.type = INPUT_MOUSE;
                            evt.mi.dx = static_cast<LONG>(x);
                            evt.mi.dy = static_cast<LONG>(y);
                            evt.mi.dwFlags = flags;
                            evt.mi.mouseData = wheel_movement;
                            evt.mi.dwExtraInfo = m_server->SendExtraMouse() ? DWEXTRA_VNC_REMOTE : 0;
                            ::SendInput(1, &evt, sizeof(evt));
                        }
                        else
                        {
                            ::mouse_event(flags, static_cast<DWORD>(x), static_cast<DWORD>(y), wheel_movement,
                                          m_server->SendExtraMouse() ? DWEXTRA_VNC_REMOTE : 0);
                        }
                        //							vnclog.Print(LL_INTINFO, VNCLOG("########mouse_event :%i %i \n"),x,y);
                    }
                    else
                    { //second or spanned
                        if (m_client->Sendinput.isValid() || preciseWheel)
                        {
                            INPUT evt{};
                            evt.type = INPUT_MOUSE;
                            int xx = msg.pe.x - GetSystemMetrics(SM_XVIRTUALSCREEN) +
                                     (m_client->monitor_Offsetx + m_client->m_ScreenOffsetx);
                            int yy = msg.pe.y - GetSystemMetrics(SM_YVIRTUALSCREEN) +
                                     (m_client->monitor_Offsety + m_client->m_ScreenOffsety);
                            if (settings->getDriver()) //chris
                            {
                                xx = msg.pe.x + m_client->monitor_Offsetx;
                                yy = msg.pe.y + m_client->monitor_Offsety;
                                //vnclog.Print(LL_INTINFO, VNCLOG("MouseMove m_cursor_pos(%d, %d), new(%d, %d)\n"),
                                //    xx, yy, msg.pe.x, msg.pe.y);
                            }
                            evt.mi.dx = (xx * 65535) / (GetSystemMetrics(SM_CXVIRTUALSCREEN) - 1);
                            evt.mi.dy = (yy * 65535) / (GetSystemMetrics(SM_CYVIRTUALSCREEN) - 1);
                            evt.mi.dwFlags = flags | MOUSEEVENTF_VIRTUALDESK;
                            evt.mi.dwExtraInfo = m_server->SendExtraMouse() ? DWEXTRA_VNC_REMOTE : 0;
                            evt.mi.mouseData = wheel_movement;
                            evt.mi.time = 0;
                            if (m_client->Sendinput.isValid())
                            {
                                (*m_client->Sendinput)(1, &evt, sizeof(evt));
                            }
                            else
                            {
                                ::SendInput(1, &evt, sizeof(evt));
                            }
                        }
                        else
                        {
                            POINT cursorPos;
                            ::GetCursorPos(&cursorPos);
                            ULONG oldSpeed{ 0U }, newSpeed = 10;
                            std::array<ULONG, 3U> mouseInfo{};
                            if (flags & MOUSEEVENTF_MOVE)
                            {
                                flags &= static_cast<DWORD>(~MOUSEEVENTF_ABSOLUTE);
                                ::SystemParametersInfo(SPI_GETMOUSE, 0, &mouseInfo, 0);
                                ::SystemParametersInfo(SPI_GETMOUSESPEED, 0, &oldSpeed, 0);
                                ULONG idealMouseInfo[] = { 10, 0, 0 };
                                ::SystemParametersInfo(SPI_SETMOUSESPEED, 0, &newSpeed, 0);
                                ::SystemParametersInfo(SPI_SETMOUSE, 0, &idealMouseInfo, 0);
                            }
                            ::mouse_event(flags, static_cast<DWORD>(msg.pe.x - cursorPos.x), static_cast<DWORD>(msg.pe.y - cursorPos.y), wheel_movement,
                                          m_server->SendExtraMouse() ? DWEXTRA_VNC_REMOTE : 0);
                            if (flags & MOUSEEVENTF_MOVE)
                            {
                                ::SystemParametersInfo(SPI_SETMOUSE, 0, &mouseInfo, 0);
                                ::SystemParametersInfo(SPI_SETMOUSESPEED, 0, &oldSpeed, 0);
                            }
                        }
                    }
                    // Save the old position
                    m_client->m_ptrevent = msg.pe;

                    // Flag that a remote event occurred
                    m_client->m_remoteevent = TRUE;

                    // Tell the desktop hook system to grab the screen...
                    // removed, terrible performance
                    // Why do we grap the screen after any inch a mouse move
                    // Removing it doesn't seems to have any missing update
                    m_client->TriggerUpdate();
                }
            }
            else if (preciseWheel)
            {
                m_client->cl_connected = FALSE;
            }
            break;
        }

        case rfbClientCutText:
            // Read the rest of the message:
            if (m_socket->ReadExact((reinterpret_cast<char*>(&msg)) + nTO, static_cast<VCard>(sz_rfbClientCutTextMsg - nTO)))
            {
                // Allocate storage for the text
                // adzm - 2010-07 - Extended clipboard
                int length = static_cast<int>(Swap32IfLE(msg.cct.length));
                // View-only clients may not write to the server clipboard.
                // The message body is still read and discarded below so the
                // protocol stays in sync.
                const bool fViewOnly = (!m_client->m_keyboardenabled || !m_client->m_pointerenabled);
                if (length > 104857600)
                { // 100 MBytes max
                    break;
                }
                if ((length < 0) && m_client->m_clipboard.settings.m_bSupportsEx)
                {
                    if ((length < -ExtendedClipboardDataMessage::maxPayload) || (length > -sz_rfbExtendedClipboardData))
                    {
                        m_client->cl_connected = FALSE;
                        break;
                    }
                    length = -length;
                    ExtendedClipboardDataMessage extendedClipboardDataMessage;

                    extendedClipboardDataMessage.EnsureBufferLength(length, false);

                    // Read in the data
                    if (!m_socket->ReadExact(reinterpret_cast<char*>(extendedClipboardDataMessage.GetBuffer()), static_cast<VCard>(length)))
                    {
                        break;
                    }

                    if (!extendedClipboardDataMessage.SetReceivedLength(length))
                    {
                        m_client->cl_connected = FALSE;
                        break;
                    }
                    DWORD action = extendedClipboardDataMessage.GetFlags() & static_cast<CARD32>(clipActionMask);

                    // clipCaps may be combined with other actions
                    if (action & clipCaps)
                    {
                        action = clipCaps;
                    }

                    switch (action)
                    {
                    case clipCaps: {
                        omni_mutex_lock l(m_client->GetUpdateLock(), 82);
                        m_client->m_clipboard.settings.HandleCapsPacket(extendedClipboardDataMessage);
                        m_client->m_clipboard.m_hasExtendedCRC = false;
                        m_client->m_clipboard.ResetTransferState();
                    }
                    break;
                    case clipProvide:
                        if (!fViewOnly)
                        {
                            m_server->UpdateLocalClipTextEx(extendedClipboardDataMessage, m_client);
                        }
                        break;
                    case clipRequest:
                    case clipPeek: {
                        ClipboardData clipboardData;

                        // only need an owner window when setting clipboard data -- by using NULL we can rely on fewer locks
                        if (!m_client->m_fFileSessionOpen && !m_server->IsThereFileTransBusy() && clipboardData.Load(nullptr, true))
                        {
                            m_client->UpdateClipTextEx(clipboardData, extendedClipboardDataMessage.GetFlags());
                        }
                    }
                    break;
                    case clipNotify: // request formats permitted by local automatic-transfer policy
                    {
                        if (fViewOnly || m_client->m_fFileSessionOpen || m_server->IsThereFileTransBusy())
                        {
                            break;
                        }
                        omni_mutex_lock l(m_client->GetUpdateLock(), 82);
                        m_client->m_clipboard.m_notifiedRemoteFormats = extendedClipboardDataMessage.GetFlags() & clipFormatMask;
                        m_client->m_clipboard.m_requestedRemoteFormats = 0;
                        CARD32 formats = m_client->m_clipboard.RequestFormats(m_client->m_clipboard.m_notifiedRemoteFormats, false);
                        if (formats)
                        {
                            ExtendedClipboardDataMessage request;
                            request.AddFlag(clipRequest | formats);
                            rfbServerCutTextMsg header{};
                            header.type = rfbServerCutText;
                            header.length = static_cast<unsigned int>(Swap32IfLE(-request.GetDataLength()));
                            if (!m_client->SendRFBMsgQueue(rfbServerCutText, reinterpret_cast<BYTE*>(&header), sizeof(header)) ||
                                !m_socket->SendExact(reinterpret_cast<char*>(const_cast<BYTE *>(request.GetData())), static_cast<VCard>(request.GetDataLength())))
                            {
                                m_client->cl_connected = FALSE;
                            }
                        }
                    }
                    break;
                    default:
                        // unsupported or not implemented
                        break;
                    }
                }
                else if (length >= 0)
                {
                    char* winStr{ nullptr };
                    {
                        char* text = new char[static_cast<unsigned long long>(length + 1)];
                        if (text == nullptr)
                        {
                            break;
                        }
                        text[length] = 0;

                        // Read in the text
                        if (!m_socket->ReadExact(text, static_cast<VCard>(length)))
                        {
                            delete[] text;
                            break;
                        }

                        const int len = static_cast<int>(std::strlen(text));
                        winStr = new char[static_cast<unsigned long long>(len * 2 + 1)];

                        int j{ 0 };
                        for (int i{ 0 }; i < len; ++i)
                        {
                            if (text[i] == 10)
                            {
                                winStr[j++] = 13;
                            }
                            winStr[j++] = text[i];
                        }
                        winStr[j++] = 0;

                        // Free the clip text we read
                        delete[] text;
                    }

                    if (winStr != nullptr)
                    {
                        // Convert ANSI to Unicode for m_strLastCutText (now std::wstring)
                        int nWideLen = ::MultiByteToWideChar(CP_ACP, 0, winStr, -1, nullptr, 0);
                        if (nWideLen > 0)
                        {
                            wchar_t* wideStr = new wchar_t[static_cast<unsigned long long>(nWideLen)];
                            ::MultiByteToWideChar(CP_ACP, 0, winStr, -1, wideStr, nWideLen);
                            m_client->m_clipboard.m_strLastCutText = wideStr;
                            delete[] wideStr;
                        }
                        // Get the server to update the local clipboard
                        if (!fViewOnly)
                        {
                            m_server->UpdateLocalClipText(winStr);
                        }

                        // Free the transformed clip text
                        delete[] winStr;
                    }
                }
                else
                {
                    break;
                }
            }
            break;

            // Modif sf@2002 - Scaling
            // Server Scaling Message received
        case rfbPalmVNCSetScaleFactor:
            if (m_server->AreThereMultipleViewers() == false)
            {
                m_client->m_fPalmVNCScaling = true;
            }
        case rfbSetScale: // Specific PalmVNC SetScaleFactor
                          // need to be ignored if multiple viewers are running, else buffer change on the fly and one of the
                          // viewers crash.
        {
            // m_client->m_fPalmVNCScaling = false;
            // Read the rest of the message
            if (m_client->m_fPalmVNCScaling)
            {
                if (!m_socket->ReadExact((reinterpret_cast<char*>(&msg)) + nTO, static_cast<VCard>(sz_rfbPalmVNCSetScaleFactorMsg - nTO)))
                {
                    m_client->cl_connected = FALSE;
                    break;
                }
            }
            else
            {
                if (!m_socket->ReadExact((reinterpret_cast<char*>(&msg)) + nTO, static_cast<VCard>(sz_rfbSetScaleMsg - nTO)))
                {
                    m_client->cl_connected = FALSE;
                    break;
                }
            }
            if (m_server->AreThereMultipleViewers() == true)
            {
                break;
            }
            // Only accept reasonable scales...
            if ((msg.ssc.scale < 1) || (msg.ssc.scale > 9))
            {
                break;
            }
            m_client->m_nScale_viewer = msg.ssc.scale;
            m_client->m_nScale = msg.ssc.scale;
            {
                omni_mutex_lock l(m_client->GetUpdateLock(), 87);
                if (!m_client->m_encodemgr.m_buffer->SetScale(msg.ssc.scale))
                {
                    m_client->cl_connected = FALSE;
                    break;
                }

                m_client->fNewScale = true;
                m_client->m_encodemgr.m_buffer->m_desktop->m_cursorpos.br.x =
                    m_client->m_encodemgr.m_buffer->m_desktop->m_cursorpos.br.x / msg.ssc.scale;
                m_client->m_encodemgr.m_buffer->m_desktop->m_cursorpos.br.y =
                    m_client->m_encodemgr.m_buffer->m_desktop->m_cursorpos.br.y / msg.ssc.scale;
                m_client->m_encodemgr.m_buffer->m_desktop->m_cursorpos.tl.x =
                    m_client->m_encodemgr.m_buffer->m_desktop->m_cursorpos.tl.x / msg.ssc.scale;
                m_client->m_encodemgr.m_buffer->m_desktop->m_cursorpos.tl.y =
                    m_client->m_encodemgr.m_buffer->m_desktop->m_cursorpos.tl.y / msg.ssc.scale;
                ::InvalidateRect(nullptr, nullptr, TRUE);
                m_client->TriggerUpdateThread();
            }
        }
        break;

        // Set Server Input
        case rfbSetServerInput:
            if (!m_socket->ReadExact((reinterpret_cast<char*>(&msg)) + nTO, static_cast<VCard>(sz_rfbSetServerInputMsg - nTO)))
            {
                m_client->cl_connected = FALSE;
                break;
            }

            m_client->m_Support_rfbSetServerInput = true;
            if (m_client->m_keyboardenabled)
            {
                // added jeff
                vnclog.Print(LL_INTINFO, VNCLOG("rfbSetServerInput: inputs %s\n"), (msg.sim.status == 1) ? "disabled" : "enabled");
#ifdef _DEBUG
                std::array<char, 256U> szText;
                ::sprintf_s(szText.data(), szText.size(), "rfbSetServerInput  %i %i %i\n", msg.sim.status, m_server->GetDesktopPointer()->GetBlockInputState(), m_client->m_bClientHasBlockedInput);
                ::OutputDebugString(szText.data());
#endif
                // only allow change if this is the client that originally changed the input state
                /// fix by PGM (pgmoney)
                if (!m_server->GetDesktopPointer()->GetBlockInputState() && (msg.sim.status == 1))
                {
                    m_client->m_encodemgr.m_buffer->m_desktop->SetBlockInputState(true);
                    m_client->m_bClientHasBlockedInput = (true);
                }

                else if (m_server->GetDesktopPointer()->GetBlockInputState() && m_client->m_bClientHasBlockedInput &&
                         (msg.sim.status == 0))
                {
                    m_client->m_encodemgr.m_buffer->m_desktop->SetBlockInputState(FALSE);
                    m_client->m_bClientHasBlockedInput = (FALSE);
                }

                else if (!m_server->GetDesktopPointer()->GetBlockInputState() && !m_client->m_bClientHasBlockedInput &&
                         (msg.sim.status == 0))
                {
                    m_client->m_encodemgr.m_buffer->m_desktop->SetBlockInputState(FALSE);
                    m_client->m_bClientHasBlockedInput = (FALSE);
                }
            }
            break;

        case rfbSetDesktopSize:
            if (!m_socket->ReadExact((reinterpret_cast<char*>(&msg)) + nTO, static_cast<VCard>(sz_rfbSetDesktopSizeMsg - nTO)))
            {
                m_client->cl_connected = FALSE;
                break;
            }
            {
                map<pair<int, int>, pair<int, int>> resolutionMap;
                DisplayMode flag = dmDisplay;
                for (int i{ 0 }; i < msg.sdm.numberOfScreens; ++i)
                {
                    rfbExtDesktopScreen eds{};
                    if (!m_socket->ReadExact((reinterpret_cast<char*>(&eds)), sz_rfbExtDesktopScreen))
                    {
                        m_client->cl_connected = FALSE;
                        break;
                    }
                    [[maybe_unused]] int id = static_cast<int>(Swap32IfLE(eds.id));
                    int x = Swap16IfLE(eds.x);
                    int y = Swap16IfLE(eds.y);
                    int w = Swap16IfLE(eds.width);
                    int h = Swap16IfLE(eds.height);
                    flag = DisplayMode(Swap32IfLE(eds.flags));
#ifdef _DEBUG
                    std::array<char, 256U> szText;
                    ::sprintf_s(szText.data(), szText.size(), "++++++++++++++++++++++++++++++ DesktopScreen %i %i %i %i \n", x, y, w, h);
                    ::OutputDebugString(szText.data());
#endif
                    //add some protection,
                    if (w * h <= 12582912)
                    {
                        resolutionMap.insert(make_pair(make_pair(x, y), make_pair(w, h)));
                    }
                }
                if (!m_server->m_virtualDisplaySupported)
                {
                    flag = dmDisplay;
                }
                // Only the first Viewer can set the resolution
                // unless it's extended mode with a single monitor
                // This way each viewer can add his own extended display
                // View-only clients may not change the host's display layout
                const bool fViewOnlySize = (!m_client->m_keyboardenabled || !m_client->m_pointerenabled);
                m_client->m_singleExtendMode = (flag == dmExtendOnly);
                if (!fViewOnlySize && m_server->virtualDisplay && !m_server->AreThereMultipleViewers())
                {
                    m_server->virtualDisplay->attachDisplay(flag, resolutionMap, m_client->m_singleExtendMode, m_client->m_id,
                                                            m_client->displayname.data());
                }
                else if (!fViewOnlySize && m_server->virtualDisplay && m_server->AreThereMultipleViewers() &&
                         m_client->m_singleExtendMode)
                {
                    m_server->virtualDisplay->attachDisplay(flag, resolutionMap, m_client->m_singleExtendMode, m_client->m_id,
                                                            m_client->displayname.data());
                }
                if (std::strlen(m_client->displayname.data()) == 0)
                {
                    m_client->m_singleExtendMode = false;
                }
            }
            break;
            // Set Single Window
        case rfbSetMonitor:
            if (m_socket->ReadExact((reinterpret_cast<char*>(&msg)) + nTO, static_cast<VCard>(sz_rfbMonitorMsg - nTO)))
            {
                m_client->m_encodemgr.m_buffer->m_desktop->SetMonitor(msg.mm.nbr);
            }
            break;
        case rfbSetSW:
            if (!m_socket->ReadExact((reinterpret_cast<char*>(&msg)) + nTO, static_cast<VCard>(sz_rfbSetSWMsg - nTO)))
            {
                m_client->cl_connected = FALSE;
                break;
            }
            if ((Swap16IfLE(msg.sw.x) < 5) && (Swap16IfLE(msg.sw.y) < 5))
            {
                m_client->m_encodemgr.m_buffer->m_desktop->SetSW(1, 1);
                break;
            }
            m_client->m_encodemgr.m_buffer->m_desktop->SetSW(
                static_cast<int>((Swap16IfLE(msg.sw.x) + m_client->monitor_Offsetx + m_client->m_ScreenOffsetx) * m_client->m_nScale),
                static_cast<int>((Swap16IfLE(msg.sw.y) + m_client->monitor_Offsety + m_client->m_ScreenOffsety) * m_client->m_nScale));
            break;

            // Modif sf@2002 - Text Chat
        case rfbTextChat:
            m_client->m_pTextChat->ProcessTextChatMsg(nTO);
            break;

            // Modif sf@2002 - File Transfer
            // File Transfer Message
        case rfbFileTransfer: {
            // sf@2004 - An unlogged user can't access to FT
            bool fUserOk{ true };
#ifndef SC_20
            if (settings->getFTUserImpersonation())
            {
                fUserOk = m_client->DoFTUserImpersonation();
            }
#endif // SC_20

            if (!m_client->m_keyboardenabled || !m_client->m_pointerenabled)
            {
                fUserOk = false;
            } //PGM

            omni_mutex_lock l(m_client->GetUpdateLock(), 88);

            // Read the rest of the message:
            m_client->m_fFileTransferRunning = TRUE;
            if (m_socket->ReadExact((reinterpret_cast<char*>(&msg)) + nTO, static_cast<VCard>(sz_rfbFileTransferMsg - nTO)))
            {
                switch (msg.ft.contentType)
                {
                    // A new file is received from the client
                    // case rfbFileHeader:
                case rfbFileTransferSessionStart:
                    m_client->m_fFileSessionOpen = true;
                    break;
                case rfbFileTransferSessionEnd:
                    m_client->m_fFileSessionOpen = false;
                    break;
                case rfbFileTransferOffer: {
                    omni_mutex_lock ll(m_client->GetUpdateLock(), 89);
                    if (!settings->getEnableFileTransfer() || !fUserOk)
                    {
                        break;
                    }
                    // bool fError = false;
                    const UINT length = Swap32IfLE(msg.ft.length);
                    std::memset(m_client->m_szFullDestName.data(), 0, sizeof(m_client->m_szFullDestName));
                    if (length > sizeof(m_client->m_szFullDestName) - 12)
                    {
                        break;
                    } // needed for temp_prefix
                    // Read in the Name of the file to create
                    if (!m_socket->ReadExact(m_client->m_szFullDestName.data(), length))
                    {
                        // vnclog.Print(LL_INTINFO, VNCLOG("*** File Transfer: Failed to receive FileName from Viewer. Abort!\n"));
                        break;
                    }

                    // sf@2004 - Improving huge files size handling
                    CARD32 sizeL = Swap32IfLE(msg.ft.size);
                    CARD32 sizeH = 0;
                    CARD32 sizeHtmp = 0;
                    if (!m_socket->ReadExact(reinterpret_cast<char*>(&sizeHtmp), sizeof(CARD32)))
                    {
                        //vnclog.Print(LL_INTINFO, VNCLOG("*** File Transfer: Failed to receive SizeH from Viewer. Abort!\n"));
                        break;
                    }
                    sizeH = Swap32IfLE(sizeHtmp);

                    // Parse the FileTime
                    char* p = strrchr(m_client->m_szFullDestName.data(), ',');
                    if (p == nullptr)
                    {
                        m_client->m_szFileTime[0U] = '\0';
                    }
                    else
                    {
                        ::strcpy_s(m_client->m_szFileTime.data(), m_client->m_szFileTime.size(), p + 1);
                        *p = '\0';
                    }

                    // Validate, canonicalize, and make a temp file name
                    bool fPathOk{ false };
                    std::string canonicalPath;
                    if (ValidateFileTransferPath(m_client->m_szFullDestName.data(), settings->getFileTransferRoot(), canonicalPath))
                    {
                        fPathOk = true;
                        ::strcpy_s(m_client->m_szFullDestName.data(), m_client->m_szFullDestName.size(), canonicalPath.c_str());
                        ::strcpy_s(m_client->m_szFullDestName.data(), m_client->m_szFullDestName.size(), make_temp_filename(m_client->m_szFullDestName.data()).c_str());
                    }
                    else
                    {
                        m_client->m_szFullDestName[0U] = '\0';
                    }

                    DWORD dwDstSize = fPathOk ? static_cast<DWORD>(0) : 0xFFFFFFFF; // Dummy size, actually a return value

#if DEBUG_FT
                    {
                        std::array<WCHAR, MAX_PATH * 4> _dbg;
                        ::MultiByteToWideChar(CP_UTF8, 0, m_client->m_szFullDestName.data(), -1, _dbg.data(), static_cast<int>(_dbg.size()));
                        ::OutputDebugStringW(L"=== SRV FTOffer: m_szFullDestName=");
                        ::OutputDebugStringW(_dbg.data());
                        ::OutputDebugStringW(L"\n");
                    }
#endif

                    // Also check the free space on destination drive
                    ULARGE_INTEGER lpFreeBytesAvailable{};
                    ULARGE_INTEGER lpTotalBytes{};
                    ULARGE_INTEGER lpTotalFreeBytes{};
                    unsigned long dwFreeKBytes;
                    {
                        std::array<WCHAR, MAX_PATH * 4> szDestPathW;
                        ::MultiByteToWideChar(CP_UTF8, 0, m_client->m_szFullDestName.data(), -1, szDestPathW.data(), static_cast<int>(szDestPathW.size()));
                        WCHAR* pSlash = std::wcsrchr(szDestPathW.data(), L'\\');
                        if (pSlash)
                        {
                            *pSlash = L'\0';
                        } // strip filename, keep directory
#if DEBUG_FT
                        ::OutputDebugStringW(L"=== SRV FTOffer: DiskFreeCheck dir=");
                        ::OutputDebugStringW(szDestPathW.data());
                        ::OutputDebugStringW(L"\n");
#endif
                        BOOL bFree =
                            ::GetDiskFreeSpaceExW(szDestPathW.data(), &lpFreeBytesAvailable, &lpTotalBytes, &lpTotalFreeBytes);
                        if (!bFree)
                        {
                            DWORD err = ::GetLastError();
#if DEBUG_FT
                            std::array<WCHAR, 64U> _e;
                            ::_snwprintf_s(_e.data(), 64, _TRUNCATE, L"=== SRV FTOffer: GetDiskFreeSpaceExW FAILED err=%lu\n", err);
                            ::OutputDebugStringW(_e.data());
#endif
                            dwDstSize = 0xFFFFFFFF;
                        }
                        else
                        {
#if DEBUG_FT
                            ::OutputDebugStringW(L"=== SRV FTOffer: GetDiskFreeSpaceExW OK\n");
#endif
                        }
                    }
                    dwFreeKBytes = static_cast<unsigned long>((Int64ShraMod32(lpFreeBytesAvailable.QuadPart, 10)));
                    __int64 nnFileSize = ((static_cast<__int64>(sizeH)) << 32) + sizeL;
                    if (static_cast<__int64>(dwFreeKBytes) < static_cast<__int64>((nnFileSize / 1000)))
                    {
#if DEBUG_FT
                        ::OutputDebugStringW(L"=== SRV FTOffer: FAIL - insufficient disk space\n");
#endif
                        dwDstSize = 0xFFFFFFFF;
                    }

                    // Allocate buffer for file packets
                    m_client->m_pBuff = new char[sz_rfbBlockSize + 1024];
                    if (m_client->m_pBuff == nullptr)
                    {
#if DEBUG_FT
                        ::OutputDebugStringW(L"=== SRV FTOffer: FAIL - pBuff alloc failed\n");
#endif
                        dwDstSize = 0xFFFFFFFF;
                    }

                    // Allocate buffer for DeCompression
                    m_client->m_pCompBuff = new char[sz_rfbBlockSize];
                    if (m_client->m_pCompBuff == nullptr)
                    {
#if DEBUG_FT
                        ::OutputDebugStringW(L"=== SRV FTOffer: FAIL - pCompBuff alloc failed\n");
#endif
                        dwDstSize = 0xFFFFFFFF;
                    }

                    rfbFileTransferMsg ft{ 0 };
                    ft.type = rfbFileTransfer;

                    // sf@2004 - Delta Transfer
                    bool fAlreadyExists{ false };
                    if (dwDstSize != 0xFFFFFFFF)
                    {
                        // sf@2004 - Directory Delta Transfer
                        // If the offered file is a zipped directory, we test if it already exists here
                        // and create the zip accordingly. This way we can generate the checksums for it.
                        // m_client->CheckAndZipDirectoryForChecksuming(m_client->m_szFullDestName);

                        if (m_client->m_hPToken)
                        {
                            ::ImpersonateLoggedOnUser(m_client->m_hPToken);
                        } //need to set this thread's impersonation or can find mapped network or share files

                        // Create Local Dest file
                        // Convert UTF-8 filename to Unicode for Chinese character support
                        // Use \\?\ prefix to support paths longer than MAX_PATH (260 chars)
                        std::array<WCHAR, MAX_PATH * 4> szFullDestNameW;
                        {
                            std::array<WCHAR, MAX_PATH * 4> szTmp;
                            ::MultiByteToWideChar(CP_UTF8, 0, m_client->m_szFullDestName.data(), -1, szTmp.data(), static_cast<int>(szTmp.size()));
                            // Only add prefix for absolute paths (not UNC) to avoid double-prefix
                            if (szTmp[0U] && (szTmp[1U] == L':'))
                            {
                                ::_snwprintf_s(szFullDestNameW.data(), szFullDestNameW.size(), _TRUNCATE, L"\\\\?\\%s", szTmp.data());
                            }
                            else
                            {
                                ::wcscpy_s(szFullDestNameW.data(), szFullDestNameW.size(), szTmp.data());
                            }
                        }
#if DEBUG_FT
                        ::OutputDebugStringW(L"=== SRV FTOffer: CreateFileW=");
                        ::OutputDebugStringW(szFullDestNameW.data());
                        ::OutputDebugStringW(L"\n");
#endif
                        m_client->m_hDestFile =
                            ::CreateFileW(szFullDestNameW.data(), GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                        nullptr, OPEN_ALWAYS, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
                        fAlreadyExists = (::GetLastError() == ERROR_ALREADY_EXISTS);
                        if (m_client->m_hDestFile == INVALID_HANDLE_VALUE)
                        {
#if DEBUG_FT
                            DWORD err = ::GetLastError();
                            std::array<WCHAR, 64U> _e2;
                            ::_snwprintf_s(_e2.data(), 64, _TRUNCATE, L"=== SRV FTOffer: CreateFileW FAILED err=%lu\n", err);
                            ::OutputDebugStringW(_e2.data());
#endif
                            dwDstSize = 0xFFFFFFFF;
                        }
                        else
                        {
#if DEBUG_FT
                            ::OutputDebugStringW(L"=== SRV FTOffer: CreateFileW OK\n");
#endif
                            dwDstSize = 0x00;
                        }
                    }
                    if (fAlreadyExists && (dwDstSize != 0xFFFFFFFF))
                    {
                        ULARGE_INTEGER n2SrcSize;
                        bool bSize = m_client->MyGetFileSize(m_client->m_szFullDestName.data(), &n2SrcSize);
                        //DWORD dwFileSize = GetFileSize(m_client->m_hDestFile, NULL);
                        //if (dwFileSize != 0xFFFFFFFF)
                        if (bSize)
                        {
                            unsigned long nCSBufferSize =
                                (4 * static_cast<unsigned long>((n2SrcSize.QuadPart / sz_rfbBlockSize))) + 1024;
                            if (nCSBufferSize > std::numeric_limits<std::size_t>::max())
                            {
                                break;
                            }
                            std::unique_ptr<char[]> lpCSBuff = std::make_unique<char[]>(nCSBufferSize);
                            if (lpCSBuff)
                            {
                                int nCSBufferLen =
                                    m_client->GenerateFileChecksums(m_client->m_hDestFile, lpCSBuff.get(), static_cast<int>(nCSBufferSize));
                                if (nCSBufferLen != -1)
                                {
                                    ft.contentType = rfbFileChecksums;
                                    ft.size = Swap32IfLE(nCSBufferSize);
                                    ft.length = static_cast<unsigned int>(Swap32IfLE(nCSBufferLen));
                                    //adzm 2010-09 - minimize packets. SendExact flushes the queue.
                                    m_socket->SendExactQueue(reinterpret_cast<char*>(&ft), sz_rfbFileTransferMsg, rfbFileTransfer);
                                    m_socket->SendExactQueue(reinterpret_cast<char*>(lpCSBuff.get()), static_cast<VCard>(nCSBufferLen));
                                }
                            }
                        }
                    }

                    ft.contentType = rfbFileAcceptHeader;
                    ft.size = Swap32IfLE(dwDstSize); // File Size in bytes, 0xFFFFFFFF (-1) means error
                    ft.length = Swap32IfLE(std::strlen(m_client->m_szFullDestName.data()));
                    //adzm 2010-09 - minimize packets. SendExact flushes the queue.
                    m_socket->SendExactQueue(reinterpret_cast<char*>(&ft), sz_rfbFileTransferMsg, rfbFileTransfer);
                    m_socket->SendExact(reinterpret_cast<char*>(m_client->m_szFullDestName.data()), static_cast<const VCard>(std::strlen(m_client->m_szFullDestName.data())));

                    if (dwDstSize == 0xFFFFFFFF)
                    {
                        helper::close_handle(m_client->m_hDestFile);
                        if (m_client->m_pCompBuff != nullptr)
                        {
                            delete[] m_client->m_pCompBuff;
                            m_client->m_pCompBuff = nullptr;
                        }
                        if (m_client->m_pBuff != nullptr)
                        {
                            delete[] m_client->m_pBuff;
                            m_client->m_pBuff = nullptr;
                        }

                        m_client->FTDownloadFailureHook();
                        break;
                    }

                    m_client->m_dwFileSize = Swap32IfLE(msg.ft.size);
                    m_client->m_dwNbPackets = static_cast<DWORD>((m_client->m_dwFileSize / sz_rfbBlockSize));
                    m_client->m_dwNbReceivedPackets = 0;
                    m_client->m_dwNbBytesWritten = 0;
                    m_client->m_dwTotalNbBytesWritten = 0;

                    m_client->m_fUserAbortedFileTransfer = false;
                    m_client->m_fFileDownloadError = false;
                    m_client->m_fFileDownloadRunning = true;
                    m_client->FTDownloadStartHook();
                    m_socket->SetRecvTimeout(static_cast<VCard32>(settings->getftTimeout() * 1000));
                }
                break;

                // The client requests a File
                case rfbFileTransferRequest:
                    m_client->filetransferrequestPart1(msg, fUserOk);
                    break;

                    // sf@2004 - Delta Transfer
                    // Destination file already exists - the viewer sends the checksums
                case rfbFileChecksums:
                    // Only accept checksums while a matching upload is pending.
                    // Without this, any authenticated client (even view-only,
                    // even with file transfer disabled) can make the server
                    // allocate and leak up to 100MB per message.
                    if (!settings->getEnableFileTransfer() || !fUserOk || !m_client->m_fFileDownloadRunning ||
                        (m_client->m_hDestFile == nullptr) || (m_client->m_hDestFile == INVALID_HANDLE_VALUE))
                    {
                        m_client->cl_connected = FALSE;
                        break;
                    }
                    m_socket->SetSendTimeout(static_cast<VCard32>(settings->getftTimeout() * 1000));
                    m_client->cl_connected =
                        m_client->ReceiveDestinationFileChecksums(static_cast<int>(Swap32IfLE(msg.ft.size)), static_cast<int>(Swap32IfLE(msg.ft.length)));
                    break;

                    // Destination file (viewer side) is ready for reception (size > 0) or not (size = -1)
                case rfbFileHeader: {
                    // Check if the file has been created on client side
                    if (Swap32IfLE(msg.ft.size) == -1)
                    {
                        helper::close_handle(m_client->m_hSrcFile);
                        m_client->FTUploadFailureHook();
                        break;
                    }

                    // Allocate buffer for file packets
                    m_client->m_pBuff = new char[sz_rfbBlockSize];
                    if (m_client->m_pBuff == nullptr)
                    {
                        helper::close_handle(m_client->m_hSrcFile);
                        m_client->FTUploadFailureHook();
                        break;
                    }

                    // Allocate buffer for compression
                    // Todo : make a global buffer with CheckBufferSize proc
                    m_client->m_pCompBuff = new char[sz_rfbBlockSize + 1024]; // TODO: Improve this
                    if (m_client->m_pCompBuff == nullptr)
                    {
                        helper::close_handle(m_client->m_hSrcFile);
                        if (m_client->m_pBuff != nullptr)
                        {
                            delete[] m_client->m_pBuff;
                            m_client->m_pBuff = nullptr;
                        }
                        m_client->FTUploadFailureHook();
                        break;
                    }

                    m_client->m_fEof = false;
                    m_client->m_dwNbBytesRead = 0;
                    m_client->m_dwTotalNbBytesWritten = 0;
                    m_client->m_nPacketCount = 0;
                    m_client->m_fFileUploadError = false;
                    m_client->m_fFileUploadRunning = true;
                    m_client->m_fUserAbortedFileTransfer = false;

                    m_client->cl_connected = m_client->SendFileChunk();
                }
                break;

                case rfbFilePacket:
                    if (!settings->getEnableFileTransfer() || !fUserOk)
                    {
                        break;
                    }
                    m_client->cl_connected = m_client->ReceiveFileChunk(static_cast<int>(Swap32IfLE(msg.ft.length)), static_cast<int>(Swap32IfLE(msg.ft.size)));
                    m_client->SendKeepAlive();
                    break;

                case rfbEndOfFile:
                    if (!settings->getEnableFileTransfer() || !fUserOk)
                    {
                        break;
                    }
                    if (m_client->m_fFileDownloadRunning)
                    {
                        m_client->FinishFileReception();
                    }
                    break;

                    // We use this message for File Transfer rights (<= RC18)
                    // The client asks for File Transfer permission
                case rfbAbortFileTransfer:

                    // For now...
                    if (m_client->m_fFileDownloadRunning)
                    {
                        m_client->m_fFileDownloadError = true;
                        m_client->m_fUserAbortedFileTransfer = true;
                        m_client->FinishFileReception();
                    }
                    else if (m_client->m_fFileUploadRunning)
                    {
                        m_client->m_fFileUploadError = true;
                        m_client->m_fUserAbortedFileTransfer = true;
                        m_client->FinishFileSending();
                    }
                    else // Old method for File Transfer handshake perimssion (<= RC18)
                    {
                        // We reject any <= RC18 UltraVNC Viewer File Transfer
                        m_client->fFTRequest = true;

                        // sf@2002 - DO IT HERE FOR THE MOMENT
                        // File Transfer permission requested by the client
                        if (m_client->fFTRequest)
                        {
                            rfbFileTransferMsg ft{ 0 };
                            ft.type = rfbFileTransfer;
                            ft.contentType = rfbAbortFileTransfer;

                            bool bOldFTProtocole = (msg.ft.contentParam == 0);
                            if (bOldFTProtocole)
                            {
                                ft.contentType = rfbAbortFileTransfer;
                            } // Viewer with New v2 File Transfer Protocol
                            else
                            {
                                ft.contentType = rfbFileTransferAccess;
                            } // Viewer with old File Transfer Protocol

                            if (!bOldFTProtocole && settings->getEnableFileTransfer() && settings->getEnableRemoteInputs() &&
                                fUserOk)
                            {
                                ft.size = Swap32IfLE(1);
                            }
                            else
                            {
                                ft.size = static_cast<unsigned int>(Swap32IfLE(-1));
                            }
                            m_client->m_socket->SendExact(reinterpret_cast<char*>(&ft), sz_rfbFileTransferMsg, rfbFileTransfer);
                            m_client->fFTRequest = false;
                        }
                    }
                    break;

                    /* Not yet used because we want backward compatibility...
                    // From RC19 versions, the UltraVNC Viewer uses this new message to request File Transfer permission
                    // It also transmits its File Transfer versions
                    case rfbFileTransferAccess:
                        m_client->fFTRequest = true;

                        // File Transfer permission requested by the client
                        if (m_client->fFTRequest)
                        {
                            rfbFileTransferMsg ft;
                            ft.type = rfbFileTransfer;
                            ft.contentType = rfbFileTransferAccess;

                            if (m_server->FileTransferEnabled() && m_client->m_server->RemoteInputsEnabled())
                               ft.size = Swap32IfLE(1);
                            else
                               ft.size = Swap32IfLE(-1);
                            m_client->m_socket->SendExact((char *)&ft, sz_rfbFileTransferMsg, rfbFileTransfer);
                            m_client->fFTRequest = false;
                        }

                        break;
                    */

                    // The client requests the content of a directory or Drives List
                case rfbDirContentRequest:
                    switch (msg.ft.contentParam & ~static_cast<CARD16>(rfbRDirContentUnicode))
                    {
                        // Client requests the List of Local Drives
                    case rfbRDrivesList: {
                        std::array<TCHAR, 256U> szDrivesList; // Format when filled : "C:\<NULL>D:\<NULL>....Z:\<NULL><NULL>
                        DWORD dwLen;
                        DWORD nIndex{ 0U };
                        int nType{ 0 };
                        std::array<TCHAR, 4U> szDrive;
                        dwLen = ::GetLogicalDriveStrings(256, szDrivesList.data());
                        // moved jdp 8/5/08 -- have to read whole packet to keep protocol in sync
                        if (!settings->getEnableFileTransfer() || !fUserOk)
                        {
                            break;
                        }

                        // We add Drives types to this drive list...
                        while (nIndex < dwLen - 3)
                        {
                            ::strcpy_s(szDrive.data(), szDrive.size(), szDrivesList.data() + nIndex);
                            // We replace the "\" char following the drive letter and ":"
                            // with a char corresponding to the type of drive
                            // We obtain something like "C:l<NULL>D:c<NULL>....Z:n\<NULL><NULL>"
                            // Isn't it ugly?
                            nType = static_cast<int>(::GetDriveType(szDrive.data()));
                            switch (nType)
                            {
                            case DRIVE_FIXED:
                                szDrivesList[nIndex + 2] = 'l';
                                break;
                            case DRIVE_REMOVABLE:
                                szDrivesList[nIndex + 2] = 'f';
                                break;
                            case DRIVE_CDROM:
                                szDrivesList[nIndex + 2] = 'c';
                                break;
                            case DRIVE_REMOTE:
                                szDrivesList[nIndex + 2] = 'n';
                                break;
                            }
                            nIndex += 4;
                        }

                        rfbFileTransferMsg ft{ 0 };
                        ft.type = rfbFileTransfer;
                        ft.contentType = rfbDirPacket;
                        ft.contentParam = rfbADrivesList;
                        ft.length = static_cast<unsigned int>(Swap32IfLE(static_cast<int>(dwLen)));
                        //adzm 2010-09 - minimize packets. SendExact flushes the queue.
                        m_socket->SendExactQueue(reinterpret_cast<char*>(&ft), sz_rfbFileTransferMsg, rfbFileTransfer);
                        m_socket->SendExact(reinterpret_cast<char*>(szDrivesList.data()), static_cast<int>(dwLen));
                    }
                    break;

                    // Client requests the content of a directory
                    case rfbRDirContent: {
                        //omni_mutex_lock l(m_client->GetUpdateLock());

                        // Check if client advertises unicode support (high bit of contentParam)
                        const bool fClientWantsUnicode = (msg.ft.contentParam & rfbRDirContentUnicode) != 0;

                        const UINT length = Swap32IfLE(msg.ft.length);
                        std::array<char, MAX_PATH * 3 + 2> szDir;
                        if (length > sizeof(szDir) - 1)
                        {
                            break;
                        }

                        // Read in the Name of Dir to explore
                        if (!m_socket->ReadExact(szDir.data(), length))
                        {
                            break;
                        }
                        szDir[length] = 0;

                        // moved jdp 8/5/08 -- have to read whole packet to keep protocol in sync
                        if (!settings->getEnableFileTransfer() || !fUserOk)
                        {
                            break;
                        }
                        // sf@2004 - Shortcuts Case
                        // Todo: Cultures translation?
                        // The viewer sends shortcut names wrapped in rfbDirPrefix/rfbDirSuffix: "[ My Documents ]"
                        // Strip those brackets to get the bare name for comparison.
                        int nFolder = -1;
                        std::array<char, MAX_PATH + 2> szP;
                        bool fShortError{ false };
                        {
                            const char* pCmp = szDir.data();
                            // Viewer sends the inner content of rfbDirPrefix/Suffix brackets,
                            // e.g. rfbDirPrefix="[ " rfbDirSuffix=" ]" so inner = " My Documents \"
                            // Skip leading '[' and spaces before comparing.
                            if (*pCmp == '[')
                            {
                                ++pCmp;
                            }
                            while (*pCmp == ' ')
                            {
                                ++pCmp;
                            }
                            if (!::_strnicmp(pCmp, "My Documents", 12))
                            {
                                nFolder = CSIDL_PERSONAL;
                            }
                            if (!::_strnicmp(pCmp, "Desktop", 7))
                            {
                                nFolder = CSIDL_DESKTOP;
                            }
                            if (!::_strnicmp(pCmp, "Network Favorites", 17))
                            {
                                nFolder = CSIDL_NETHOOD;
                            }
                        }

                        std::array<wchar_t, MAX_PATH * 4> szDirW{};
                        bool fSpecialFolderResolvedW{ false };
                        if ((nFolder != -1) && fClientWantsUnicode)
                        {
                            // Impersonate logged-in user so Shell APIs return the correct user's folders
                            // (e.g. Desktop/Downloads) when running as a service (SYSTEM account)
                            if (m_client->m_hPToken)
                            {
                                ::ImpersonateLoggedOnUser(m_client->m_hPToken);
                            }
                            LPITEMIDLIST pidl = nullptr;
                            LPMALLOC pMalloc = nullptr;
                            HRESULT hrMalloc = ::SHGetMalloc(&pMalloc);
                            HRESULT hrLoc =
                                (SUCCEEDED(hrMalloc) && pMalloc) ? ::SHGetSpecialFolderLocation(nullptr, nFolder, &pidl) : E_FAIL;
                            if (SUCCEEDED(hrMalloc) && (hrLoc == NOERROR) && pidl)
                            {
                                BOOL bPath = ::SHGetPathFromIDListW(pidl, szDirW.data());
                                if (bPath)
                                {
                                    if (szDirW[std::wcslen(szDirW.data()) - 1] != L'\\')
                                    {
                                        ::wcscat_s(szDirW.data(), szDirW.size(), L"\\");
                                    }
                                    fSpecialFolderResolvedW = true;
                                }
                                else
                                {
                                    fShortError = true;
                                }
                                pMalloc->Free(pidl);
                            }
                            else
                            {
                                fShortError = true;
                            }
                            if (pMalloc)
                            {
                                pMalloc->Release();
                            }
                            if (m_client->m_hPToken)
                            {
                                ::RevertToSelf();
                            }
                        }
                        else if (nFolder != -1)
                        {
                            // ANSI branch: original behaviour
                            if (m_client->m_hPToken)
                            {
                                ::ImpersonateLoggedOnUser(m_client->m_hPToken);
                            }
                            if (m_client->GetSpecialFolderPath(nFolder, szP.data()))
                            {
                                if (szP[std::strlen(szP.data()) - 1] != '\\')
                                {
                                    ::strcat_s(szP.data(), szP.size(), "\\");
                                }
                                ::strcpy_s(szDir.data(), szDir.size(), szP.data());
                            }
                            else
                            {
                                fShortError = true;
                            }
                            if (m_client->m_hPToken)
                            {
                                ::RevertToSelf();
                            }
                        }

                        // Enforce the configured file-transfer root on directory listings:
                        // either the client-supplied path or the resolved special folder.
                        if (!fShortError)
                        {
                            std::string canonDir;
                            bool fValid;
                            if (nFolder == -1)
                            {
                                fValid = ValidateFileTransferPath(szDir.data(), settings->getFileTransferRoot(), canonDir,
                                                                  fClientWantsUnicode ? CP_UTF8 : CP_ACP);
                            }
                            else if (fSpecialFolderResolvedW)
                            {
                                std::array<char, MAX_PATH * 4> szResolvedA;
                                fValid = (::WideCharToMultiByte(CP_UTF8, 0, szDirW.data(), -1, szResolvedA.data(),
                                                              sizeof(szResolvedA), nullptr, nullptr) > 0) &&
                                         ValidateFileTransferPath(szResolvedA.data(), settings->getFileTransferRoot(), canonDir);
                            }
                            else
                            {
                                fValid = ValidateFileTransferPath(szDir.data(), settings->getFileTransferRoot(), canonDir, CP_ACP);
                            }
                            if (!fValid)
                            {
                                fShortError = true;
                            }
                        }

                        ::strcat_s(szDir.data(), szDir.size(), "*");
                        if (fSpecialFolderResolvedW)
                        {
                            ::wcscat_s(szDirW.data(), szDirW.size(), L"*");
                        }

                        rfbFileTransferMsg ft{};
                        ft.type = rfbFileTransfer;
                        ft.contentType = rfbDirPacket;
                        ft.contentParam = rfbADirectory; // or rfbAFile...

                        DWORD errmode = ::SetErrorMode(SEM_FAILCRITICALERRORS); // No popup please !

                        if (fClientWantsUnicode)
                        {
                            // Unicode path: client sent UTF-8 (or special folder resolved directly to wchar_t above)
                            if (!fSpecialFolderResolvedW)
                            {
                                ::MultiByteToWideChar(CP_UTF8, 0, szDir.data(), -1, szDirW.data(), MAX_PATH + 2);
                            }

                            WIN32_FIND_DATAW fdW;
                            HANDLE ff = ::FindFirstFileW(szDirW.data(), &fdW);
                            ::SetErrorMode(errmode);

                            // Case of media not accessible
                            if ((ff == INVALID_HANDLE_VALUE) || fShortError)
                            {
                                // Send rfbADirInaccessible so new viewers can mark the folder red.
                                // Old viewers see an unrecognised contentParam with length=0 and treat it as end-of-dir.
                                ft.contentParam = rfbADirInaccessible;
                                ft.length = Swap32IfLE(0);
                                m_socket->SendExact(reinterpret_cast<char*>(&ft), sz_rfbFileTransferMsg, rfbFileTransfer);
                                break;
                            }

                            // Send back the full directory path as UTF-8 (strip trailing '*')
                            std::array<char, MAX_PATH * 3 + 2> szDirNoStar;
                            if (fSpecialFolderResolvedW)
                            {
                                // Convert resolved Unicode path to UTF-8 for the wire
                                std::array<wchar_t, MAX_PATH * 4> szDirWNoStar;
                                ::wcscpy_s(szDirWNoStar.data(), szDirWNoStar.size(), szDirW.data());
                                szDirWNoStar[std::wcslen(szDirWNoStar.data()) - 1] = L'\0'; // strip '*'
                                ::WideCharToMultiByte(CP_UTF8, 0, szDirWNoStar.data(), -1, szDirNoStar.data(), MAX_PATH * 3, nullptr,
                                                    nullptr);
                            }
                            else
                            {
                                ::strcpy_s(szDirNoStar.data(), szDirNoStar.size(), szDir.data());
                                szDirNoStar[std::strlen(szDirNoStar.data()) - 1] = '\0'; // strip '*'
                            }
                            ft.contentParam = static_cast<CARD16>((rfbADirectory | rfbADirUnicode));
                            ft.length = Swap32IfLE(static_cast<CARD32>(std::strlen(szDirNoStar.data())));
                            m_socket->SendExactQueue(reinterpret_cast<char*>(&ft), sz_rfbFileTransferMsg, rfbFileTransfer);
                            m_socket->SendExactQueue(szDirNoStar.data(), static_cast<const VCard>(std::strlen(szDirNoStar.data())));

                            BOOL fRet = TRUE;
                            while (fRet)
                            {
                                if ((((fdW.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY) &&
                                     std::wcscmp(fdW.cFileName, L".")) ||
                                    (!std::wcscmp(fdW.cFileName, L"..")))
                                {
                                    // Probe subfolder accessibility; flag it so viewer can show red before clicking
                                    if (std::wcscmp(fdW.cFileName, L".."))
                                    {
                                        std::array<wchar_t, MAX_PATH * 4> szProbeW;
                                        std::array<wchar_t, MAX_PATH * 4> szBaseW;
                                        ::wcscpy_s(szBaseW.data(), szBaseW.size(), szDirW.data());
                                        szBaseW[std::wcslen(szBaseW.data()) - 1] = L'\0'; // strip trailing '*'
                                        ::swprintf_s(szProbeW.data(), szProbeW.size(), L"%s%s\\*", szBaseW.data(), fdW.cFileName);
                                        WIN32_FIND_DATAW fdProbeW;
                                        HANDLE hProbe = ::FindFirstFileW(szProbeW.data(), &fdProbeW);
                                        if (hProbe == INVALID_HANDLE_VALUE)
                                        {
                                            fdW.dwReserved0 = rfbFD_INACCESSIBLE;
                                        }
                                        else
                                        {
                                            fdW.dwReserved0 = 0;
                                            ::FindClose(hProbe);
                                        }
                                    }
                                    // Serialize the interesting part of WIN32_FIND_DATAW:
                                    // send everything up to and including the actual filename (null-terminated),
                                    // omitting the unused trailing chars of cFileName and cAlternateFileName.
                                    static_assert(offsetof(WIN32_FIND_DATAW, cAlternateFileName) ==
                                                      offsetof(WIN32_FIND_DATAW, cFileName) + MAX_PATH * sizeof(wchar_t),
                                                  "WIN32_FIND_DATAW: cAlternateFileName must immediately follow cFileName");
                                    std::array<char, sizeof(WIN32_FIND_DATAW)> szFileSpec;
                                    int nOptLen = static_cast<int>(
                                        (offsetof(WIN32_FIND_DATAW, cFileName) + (static_cast<unsigned long long>(lstrlenW(fdW.cFileName) + 1)) * sizeof(wchar_t)));
                                    if ((nOptLen < 0) || (nOptLen > static_cast<int>(sizeof(WIN32_FIND_DATAW))))
                                    {
                                        nOptLen = sizeof(WIN32_FIND_DATAW);
                                    }
                                    std::memcpy(szFileSpec.data(), &fdW, static_cast<size_t>(nOptLen));

                                    ft.contentParam = static_cast<CARD16>((rfbADirectory | rfbADirUnicode));
                                    ft.length = static_cast<unsigned int>(Swap32IfLE(nOptLen));
                                    m_socket->SendExactQueue(reinterpret_cast<char*>(&ft), sz_rfbFileTransferMsg, rfbFileTransfer);
                                    m_socket->SendExactQueue(szFileSpec.data(), static_cast<VCard>(nOptLen));
                                }
                                else if (std::wcscmp(fdW.cFileName, L"."))
                                {
                                    std::array<char, sizeof(WIN32_FIND_DATAW)> szFileSpec;
                                    int nOptLen = static_cast<int>((offsetof(WIN32_FIND_DATAW, cFileName) +
                                                                    static_cast<unsigned long long>(lstrlenW(fdW.cFileName)) * sizeof(wchar_t) + sizeof(wchar_t)));
                                    if ((nOptLen < 0) || (nOptLen > static_cast<int>(sizeof(WIN32_FIND_DATAW))))
                                    {
                                        nOptLen = sizeof(WIN32_FIND_DATAW);
                                    }
                                    std::memcpy(szFileSpec.data(), &fdW, static_cast<size_t>(nOptLen));

                                    ft.contentParam = static_cast<CARD16>((rfbAFile | rfbADirUnicode));
                                    ft.length = static_cast<unsigned int>(Swap32IfLE(nOptLen));
                                    m_socket->SendExactQueue(reinterpret_cast<char*>(&ft), sz_rfbFileTransferMsg, rfbFileTransfer);
                                    m_socket->SendExactQueue(szFileSpec.data(), static_cast<VCard>(nOptLen));
                                }
                                fRet = ::FindNextFileW(ff, &fdW);
                            }
                            ::FindClose(ff);
                        }
                        else
                        {
                            // ANSI path: original behaviour
                            WIN32_FIND_DATA fd;
                            HANDLE ff = ::FindFirstFile(szDir.data(), &fd);
                            ::SetErrorMode(errmode);

                            // Case of media not accessible
                            if ((ff == INVALID_HANDLE_VALUE) || fShortError)
                            {
                                ft.contentParam = rfbADirInaccessible;
                                ft.length = Swap32IfLE(0);
                                m_socket->SendExact(reinterpret_cast<char*>(&ft), sz_rfbFileTransferMsg, rfbFileTransfer);
                                break;
                            }

                            ft.length = Swap32IfLE(std::strlen(szDir.data()) - 1);
                            //adzm 2010-09 - minimize packets. SendExact flushes the queue.
                            m_socket->SendExactQueue(reinterpret_cast<char*>(&ft), sz_rfbFileTransferMsg, rfbFileTransfer);
                            // sf@2004 - Also send back the full directory path to the viewer (necessary for Shorcuts)
                            m_socket->SendExactQueue(reinterpret_cast<char*>(szDir.data()), static_cast<const VCard>((std::strlen(szDir.data()) - 1)));

                            BOOL fRet = TRUE;
                            while (fRet)
                            {
                                if ((((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY) &&
                                     std::strcmp(fd.cFileName, ".")) ||
                                    (!std::strcmp(fd.cFileName, "..")))
                                {
                                    if (std::strcmp(fd.cFileName, ".."))
                                    {
                                        std::array<char, MAX_PATH * 4> szProbe;
                                        std::array<char, MAX_PATH * 4> szBase;
                                        ::strcpy_s(szBase.data(), szBase.size(), szDir.data());
                                        szBase[std::strlen(szBase.data()) - 1] = '\0'; // strip trailing '*'
                                        ::sprintf_s(szProbe.data(), szProbe.size(), "%s%s\\*", szBase.data(), fd.cFileName);
                                        WIN32_FIND_DATA fdProbe;
                                        HANDLE hProbe = ::FindFirstFile(szProbe.data(), &fdProbe);
                                        if (hProbe == INVALID_HANDLE_VALUE)
                                        {
                                            fd.dwReserved0 = rfbFD_INACCESSIBLE;
                                        }
                                        else
                                        {
                                            fd.dwReserved0 = 0;
                                            ::FindClose(hProbe);
                                        }
                                    }

                                    // Serialize the interesting part of WIN32_FIND_DATA
                                    std::array<char, sizeof(WIN32_FIND_DATA)> szFileSpec;
                                    int nOptLen = sizeof(WIN32_FIND_DATA) - MAX_PATH - static_cast<int>(14 + ::lstrlen(fd.cFileName));
                                    std::memcpy(szFileSpec.data(), &fd, static_cast<size_t>(nOptLen));

                                    ft.length = static_cast<unsigned int>(Swap32IfLE(nOptLen));
                                    //adzm 2010-09 - minimize packets. SendExact flushes the queue.
                                    m_socket->SendExactQueue(reinterpret_cast<char*>(&ft), sz_rfbFileTransferMsg, rfbFileTransfer);
                                    m_socket->SendExactQueue(reinterpret_cast<char*>(szFileSpec.data()), static_cast<VCard>(nOptLen));
                                }
                                else if (std::strcmp(fd.cFileName, "."))
                                {
                                    // Serialize the interesting part of WIN32_FIND_DATA
                                    // Get rid of the trailing blank chars. It makes a BIG
                                    // difference when there's a lot of files in the dir.
                                    std::array<char, sizeof(WIN32_FIND_DATA)> szFileSpec;
                                    int nOptLen = sizeof(WIN32_FIND_DATA) - MAX_PATH - static_cast<int>(14 + ::lstrlen(fd.cFileName));
                                    std::memcpy(szFileSpec.data(), &fd, static_cast<size_t>(nOptLen));

                                    ft.length = static_cast<unsigned int>(Swap32IfLE(nOptLen));
                                    //adzm 2010-09 - minimize packets. SendExact flushes the queue.
                                    m_socket->SendExactQueue(reinterpret_cast<char*>(&ft), sz_rfbFileTransferMsg, rfbFileTransfer);
                                    m_socket->SendExactQueue(reinterpret_cast<char*>(szFileSpec.data()), static_cast<VCard>(nOptLen));
                                }
                                fRet = ::FindNextFile(ff, &fd);
                            }
                            ::FindClose(ff);
                        }

                        // End of the transfer
                        ft.contentParam = 0;
                        ft.length = Swap32IfLE(0);
                        m_socket->SendExact(reinterpret_cast<char*>(&ft), sz_rfbFileTransferMsg, rfbFileTransfer);
                    }
                    } // end inner switch (rfbDirContentRequest)
                    break;

                // The client sends a command
                case rfbCommand:
                    switch (msg.ft.contentParam)
                    {
                        // Client requests the creation of a directory
                    case rfbCDirCreate: {
                        const UINT length = Swap32IfLE(msg.ft.length);
                        std::array<char, MAX_PATH * 3 + 1> szDir;
                        if (length > sizeof(szDir) - 1)
                        {
                            break;
                        }

                        // Read in the Name of Dir to explore
                        if (!m_socket->ReadExact(szDir.data(), length))
                        {
                            // todo: manage error !
                            break;
                        }
                        szDir[length] = 0;

                        // moved jdp 8/5/08 -- have to read whole packet to keep protocol in sync
                        if (!settings->getEnableFileTransfer() || !fUserOk)
                        {
                            break;
                        }
                        // Create the Dir
                        // Convert UTF-8 to Unicode for Chinese character support
                        // Enforce the configured file-transfer root on mkdir
                        std::array<WCHAR, MAX_PATH * 4> szDirW;
                        BOOL fRet = FALSE;
                        std::string canonDirCreate;
                        if (ValidateFileTransferPath(szDir.data(), settings->getFileTransferRoot(), canonDirCreate))
                        {
                            ::MultiByteToWideChar(CP_UTF8, 0, canonDirCreate.c_str(), -1, szDirW.data(), static_cast<int>(szDirW.size()));
                            fRet = ::CreateDirectoryW(szDirW.data(), nullptr);
                        }

                        rfbFileTransferMsg ft{};
                        ft.type = rfbFileTransfer;
                        ft.contentType = rfbCommandReturn;
                        ft.contentParam = rfbADirCreate;
                        ft.size = fRet ? 0 : static_cast<CARD32>(-1);
                        ft.length = msg.ft.length;

                        //adzm 2010-09 - minimize packets. SendExact flushes the queue.
                        m_socket->SendExactQueue(reinterpret_cast<char*>(&ft), sz_rfbFileTransferMsg, rfbFileTransfer);
                        m_socket->SendExact(reinterpret_cast<char*>(szDir.data()), static_cast<int>(length));
                        if (fRet)
                        {
                            m_client->FTNewFolderHook(szDir.data());
                        }
                    }
                    break;

                    // Client requests the deletion of a file
                    case rfbCFileDelete: {
                        UINT length = Swap32IfLE(msg.ft.length);
                        std::array<char, MAX_PATH * 3 + 1> szFile;
                        if (length > sizeof(szFile) - 1)
                        {
                            break;
                        }

                        // Read in the Name of the File
                        if (!m_socket->ReadExact(szFile.data(), length))
                        {
                            // todo: manage error !
                            break;
                        }
                        szFile[length] = 0;
                        // moved jdp 8/5/08 -- have to read whole packet to keep protocol in sync
                        if (!settings->getEnableFileTransfer() || !fUserOk)
                        {
                            break;
                        }

                        // Delete the file
                        // 13 February 2008 jdp
                        bool isDir = isDirectory(szFile.data());
                        std::string newname(szFile.data());
                        // put the '[]' around the name if it's a folder, so that the client can display
                        // the proper messages. Otherwise, the client assumes it's a file.
                        if (isDir)
                        {
                            newname = AddDirPrefixAndSuffix(szFile.data());
                        }

                        length = static_cast<UINT>((newname.length() + 1));
                        // Convert UTF-8 to Unicode for Chinese character support
                        // Enforce the configured file-transfer root on delete
                        std::array<WCHAR, MAX_PATH * 4> szFileW;
                        BOOL fRet = FALSE;
                        std::string canonFileDel;
                        if (ValidateFileTransferPath(szFile.data(), settings->getFileTransferRoot(), canonFileDel))
                        {
                            ::MultiByteToWideChar(CP_UTF8, 0, canonFileDel.c_str(), -1, szFileW.data(), static_cast<int>(szFileW.size()));
                            fRet = DeleteFileOrDirectory(szFileW.data());
                        }

                        rfbFileTransferMsg ft{};
                        ft.type = rfbFileTransfer;
                        ft.contentType = rfbCommandReturn;
                        ft.contentParam = rfbAFileDelete;
                        ft.size = fRet ? 0 : static_cast<CARD32>(-1);
                        ft.length = Swap32IfLE(length);

                        //adzm 2010-09 - minimize packets. SendExact flushes the queue.
                        m_socket->SendExactQueue(reinterpret_cast<char*>(&ft), sz_rfbFileTransferMsg, rfbFileTransfer);
                        m_socket->SendExact(reinterpret_cast<char*>(const_cast<char *>(newname.c_str())), length);
                        if (fRet)
                        {
                            m_client->FTDeleteHook(szFile.data(), isDir);
                        }
                    }
                    break;

                    // Client requests the Renaming of a file/directory
                    case rfbCFileRename: {
                        const UINT length = Swap32IfLE(msg.ft.length);
                        std::array<char, (2 * MAX_PATH * 3) + 1> szNames;
                        if (length > sizeof(szNames) - 1)
                        {
                            break;
                        }

                        // Read in the Names
                        if (!m_socket->ReadExact(szNames.data(), length))
                        {
                            // todo: manage error !
                            break;
                        }
                        szNames[length] = 0;
                        // moved jdp 8/5/08 -- have to read whole packet to keep protocol in sync
                        if (!settings->getEnableFileTransfer() || !fUserOk)
                        {
                            break;
                        }

                        char* p = strrchr(szNames.data(), '*');
                        if (p == nullptr)
                        {
                            break;
                        }
                        std::array<char, MAX_PATH * 3 + 1> szCurrentName;
                        std::array<char, MAX_PATH * 3 + 1> szNewName;

                        ::strcpy_s(szNewName.data(), szNewName.size(), p + 1);
                        *p = '\0';
                        ::strcpy_s(szCurrentName.data(), szCurrentName.size(), szNames.data());
                        *p = '*';

                        // Rename - use \\?\ prefix for long path support
                        // Enforce the configured file-transfer root on both names
                        std::array<WCHAR, MAX_PATH * 4> szCurrentNameW;
                        std::array<WCHAR, MAX_PATH * 4> szNewNameW;
                        BOOL fRet = FALSE;
                        std::string canonCurName, canonNewName;
                        if (ValidateFileTransferPath(szCurrentName.data(), settings->getFileTransferRoot(), canonCurName) &&
                            ValidateFileTransferPath(szNewName.data(), settings->getFileTransferRoot(), canonNewName))
                        {
                            MakeLongPath(canonCurName.c_str(), szCurrentNameW.data(), static_cast<int>(szCurrentNameW.size()));
                            MakeLongPath(canonNewName.c_str(), szNewNameW.data(), static_cast<int>(szNewNameW.size()));
                            fRet = ::MoveFileExW(szCurrentNameW.data(), szNewNameW.data(), MOVEFILE_REPLACE_EXISTING);
                        }

                        rfbFileTransferMsg ft{};
                        ft.type = rfbFileTransfer;
                        ft.contentType = rfbCommandReturn;
                        ft.contentParam = rfbAFileRename;
                        ft.size = fRet ? 0 : static_cast<CARD32>(-1);
                        ft.length = msg.ft.length;

                        //adzm 2010-09 - minimize packets. SendExact flushes the queue.
                        m_socket->SendExactQueue(reinterpret_cast<char*>(&ft), sz_rfbFileTransferMsg, rfbFileTransfer);
                        m_socket->SendExact(reinterpret_cast<char*>(szNames.data()), static_cast<int>(length));
                        if (fRet)
                        {
                            m_client->FTRenameHook(szCurrentName.data(), szNewName.data());
                        }
                    }
                    break;
                    } // End of swith
                    break;
                } // End of switch
            } // End of if
            else // Fix from Jeremy C.
            {
                if (m_client->m_fFileDownloadRunning)
                {
                    m_client->m_fFileDownloadError = true;
                    ::FlushFileBuffers(m_client->m_hDestFile);
                    helper::close_handle(m_client->m_hDestFile);
                    m_client->FTDownloadFailureHook();
                    m_client->m_fFileDownloadRunning = false;
                }
                if (m_client->m_fFileUploadRunning)
                {
                    m_client->m_fFileUploadError = true;
                    ::FlushFileBuffers(m_client->m_hSrcFile);
                    helper::close_handle(m_client->m_hSrcFile);
                    m_client->FTUploadFailureHook();
                    m_client->m_fFileUploadRunning = false;
                }
                //vnclog.Print(LL_INTINFO, VNCLOG("*** File Transfer: message content reading error\n"));
            }

            m_client->m_fFileTransferRunning = FALSE;
        }
        break;

        // adzm 2010-09 - Notify streaming DSM plugin support
        case rfbNotifyPluginStreaming:
            if (sz_rfbNotifyPluginStreamingMsg > 1)
            {
                if (!m_socket->ReadExact((reinterpret_cast<char*>(&msg)) + nTO, static_cast<VCard>(sz_rfbNotifyPluginStreamingMsg - nTO)))
                {
                    m_client->cl_connected = FALSE;
                    break;
                }
            }
            m_socket->SetPluginStreamingIn();
            break;
        default:
            // Unknown message, so fail!
            vnclog.Print(LL_CONNERR, VNCLOG("client loop: unknown msg.type=%u - disconnecting\n"), static_cast<unsigned>(msg.type));
            m_client->cl_connected = FALSE;
        }
#ifndef SC_20
        // sf@2005 - Cancel File Transfer User impersonation if possible
        // We do it here to ensure impersonation is cancelled
        if (settings->getFTUserImpersonation())
        {
            m_client->UndoFTUserImpersonation();
        }
#endif // SC_20
    }

    if (fShutdownOrdered)
    {
        m_autoreconnectcounter_quit = true;
        //needed to give autoreconnect (max 100) to quit
        ::Sleep(200);
    }

    if (m_client->m_fFileDownloadRunning)
    {
        m_client->m_fFileDownloadError = true;
        ::FlushFileBuffers(m_client->m_hDestFile);
        m_client->FTDownloadFailureHook();
        m_client->m_fFileDownloadRunning = false;
        helper::close_handle(m_client->m_hDestFile);
    }

    if (m_client->m_fFileUploadRunning)
    {
        m_client->m_fFileUploadError = true;
        ::FlushFileBuffers(m_client->m_hSrcFile);
        m_client->FTUploadFailureHook();
        m_client->m_fFileUploadRunning = false;
        helper::close_handle(m_client->m_hSrcFile);
    }

    // Move into the thread's original desktop
    // TAG 14
    desktopSelector::SelectHDESK(home_desktop);
    if (m_client->m_bClientHasBlockedInput)
    {
        m_client->m_encodemgr.m_buffer->m_desktop->SetBlockInputState(false);
        m_client->m_bClientHasBlockedInput = false;
    }
    if (input_desktop)
    {
        if (!::CloseDesktop(input_desktop))
        {
            vnclog.Print(LL_INTERR, VNCLOG("failed to close desktop\n"));
        }
    }
    // Quit this thread. This will automatically delete the thread and the
    // associated client.
    vnclog.Print(LL_CLIENTS, VNCLOG("client disconnected : %s (%hd)\n"), m_client->GetClientNameName(), m_client->GetClientId());
    //////////////////
    // LOG it also in the event
    //////////////////
#ifndef SC_20
    typedef BOOL (*LogeventFn)(char* machine, char* user, int clientId, bool isinteractive, char* szMslogonLog);
    LogeventFn Logevent = nullptr;
    std::array<char, MAX_PATH> szCurrentDir;
    ::strcpy_s(szCurrentDir.data(), szCurrentDir.size(), winvncFolder.data());
    ::strcat_s(szCurrentDir.data(), szCurrentDir.size(), "\\logging.dll");
    HMODULE hModule = ::LoadLibrary(szCurrentDir.data());
    if (hModule)
    {
        Logevent = reinterpret_cast<LogeventFn>(::GetProcAddress(hModule, "LOGEXITV2"));
        Logevent(reinterpret_cast<char*>(const_cast<char *>(m_client->GetClientNameName())), reinterpret_cast<char*>(const_cast<char *>(m_client->GetClientDomainUsername())), m_client->GetClientId(),
                 (m_client->m_keyboardenabled && m_client->m_pointerenabled), settings->getLogFile());
        ::FreeLibrary(hModule);
    }
#endif // SC_20

    // Disable the protocol to ensure that the update thread
    // is not accessing the desktop and buffer objects
    // DEADLOCK ON EXIT SLow systems... IS this realy needed
    //m_client->DisableProtocol();

    // Finally, it's safe to kill the update thread here
    if (m_client->m_updatethread)
    {
        m_client->m_updatethread->Kill();
        m_client->m_updatethread->join(nullptr);
    }
    // Remove the client from the server
    // This may result in the desktop and buffer being destroyed
    // It also guarantees that the client will not be passed further
    // updates
    m_server->RemoveClient(m_client->GetClientId());

    if (!fShutdownOrdered)
    {
        if (m_client->m_Autoreconnect)
        {
            vnclog.Print(LL_INTERR, VNCLOG("PostAddNewClient II\n"));
            m_server->AutoReconnect(m_client->m_Autoreconnect);
            m_server->AutoReconnectPort(m_AutoReconnectPort);
            m_server->AutoReconnectAdr(m_szAutoReconnectAdr.data());
            m_server->AutoReconnectId(m_szAutoReconnectId.data());
            if (settings->getIPV6())
            {
                postHelper::PostAddNewClient4(1111, 1111);
            }
            else
            {
                postHelper::PostAddNewClient(1111, 1111);
            }
        }
    }
}

// The vncClient itself

// adzm - 2010-07 - Extended clipboard
vncClient::vncClient() : m_clipboard(ClipboardSettings::defaultServerCaps), Sendinput{ "USER32", "SendInput" }
{
    vnclog.Print(LL_INTINFO, VNCLOG("vncClient() executing...\n"));

    m_hPToken = nullptr;

    m_socket = nullptr;
    m_client_name_name = nullptr;
    m_client_name_address = nullptr;
    m_client_domain_username = nullptr;

    // Initialise mouse fields
    m_mousemoved = FALSE;
    m_ptrevent.buttonMask = 0;
    m_ptrevent.x = 0;
    m_ptrevent.y = 0;

    // Other misc flags
    m_thread_ClientThread = nullptr;
    m_palettechanged = FALSE;

    // Initialise the two update stores
    m_updatethread = nullptr;
    m_update_tracker.init(this);

    m_remoteevent = FALSE;

    // adzm - 2010-07 - Extended clipboard
    //m_clipboard_text = 0;

    // IMPORTANT: Initially, client is not protocol-ready.
    m_disable_protocol = 1;

    //SINGLE WINDOW
    m_use_NewSWSize = FALSE;
    m_use_ExtDesktopSize = FALSE;
    m_requestedDesktopSizeChange = 0;
    m_lastDesktopSizeChangeError = 0;
    monitor_Offsetx = 0;
    monitor_Offsety = 0;
    m_ScreenOffsetx = 0;
    m_ScreenOffsety = 0;

    // sf@2002
    fNewScale = false;
    m_fPalmVNCScaling = false;
    fFTRequest = false;

    // Modif sf@2002 - File Transfer
    m_fFileTransferRunning = FALSE;
    m_fClientSupportsUnicode = false; // Set to true when client sends rfbEncodingFTProtocolVersion
    m_pZipUnZip = new CMiniZipNG();   // Directory File Transfer utils (Unicode-aware)

    m_hDestFile = nullptr;
    //m_szFullDestName = NULL;
    m_dwFileSize = 0;
    m_dwNbPackets = 0;
    m_dwNbReceivedPackets = 0;
    m_dwNbBytesWritten = 0;
    m_dwTotalNbBytesWritten = 0;
    m_fFileDownloadError = false;
    m_fUserAbortedFileTransfer = false;
    m_fFileDownloadRunning = false;
    m_hSrcFile = INVALID_HANDLE_VALUE;
    //m_szSrcFileName = NULL;
    m_fEof = false;
    m_dwNbBytesRead = 0;
    m_dwTotalNbBytesRead = 0;
    m_nPacketCount = 0;
    m_fCompressionEnabled = false;
    m_fFileUploadError = false;
    m_fFileUploadRunning = false;

    // sf@2004 - Delta Transfer
    m_lpCSBuffer = nullptr;
    m_nCSOffset = 0;
    m_nCSBufferSize = 0;

    // CURSOR HANDLING
    m_cursor_update_pending = FALSE;
    m_cursor_update_sent = FALSE;
    // nyama/marscha - PointerPos
    m_use_PointerPos = FALSE;
    m_cursor_pos_changed = FALSE;
    m_cursor_pos.x = 0;
    m_cursor_pos.y = 0;

    //cachestats
    totalraw = 0;

    m_pRawCacheZipBuf = nullptr;
    m_nRawCacheZipBufSize = 0;
    m_pCacheZipBuf = nullptr;
    m_nCacheZipBufSize = 0;

    // sf@2005 - FTUserImpersonation
    m_fFTUserImpersonatedOk = false;
    m_lLastFTUserImpersonationTime = 0L;

    // Modif sf@2002 - Text Chat
    m_pTextChat = new TextChat(this);
    m_fUltraViewer = true;
    m_IsLoopback = false;
    m_NewSWUpdateWaiting = false;
    client_settings_passed = false;
    initialCapture_done = false;
    m_wants_ServerStateUpdates = false;
    m_bClientHasBlockedInput = false;
    m_Support_rfbSetServerInput = false;
    m_wants_KeepAlive = false;
    m_session_supported = false;
    m_fFileSessionOpen = false;
    m_supportsUnicodeTextChat = false;
    m_supportsHighResolutionWheel = false;
    m_supportsChatFileTransfer = false;
    m_pBuff = nullptr;
    m_pCompBuff = nullptr;
    m_NewSWDesktop = 0;
    NewsizeW = 0;
    NewsizeH = 0;

    // adzm 2009-07-05
    m_szRepeaterID = nullptr; // as in, not using
    m_szHost = nullptr;
    m_hostPort = 0;
    m_want_update_state = false;
    m_initial_update = false;
    m_nScale_viewer = 1;
    nr_incr_rgn_empty = 0;
    ThreadHandleCompressFolder = nullptr;
    sendingUpdate = false;
    m_singleExtendMode = false;
    m_firstExtDesktop = true;
    m_firstExtDesktopIncremental = true;
    has_mouse = false;
    ask_mouse = false;
    simulateCursor = nullptr;
    forceBlacklist = false;
    authAttempted = false;
}

vncClient::~vncClient()
{
    cl_connected = false;
    vnclog.Print(LL_INTINFO, VNCLOG("~vncClient() executing...\n"));

    // Modif sf@2002 - Text Chat
    if (m_pTextChat)
    {
        m_pTextChat->KillDialog();
        delete (m_pTextChat);
        m_pTextChat = nullptr;
    }

    // Directory File Transfer utils
    if (m_pZipUnZip)
    {
        delete m_pZipUnZip;
    }

    // We now know the thread is dead, so we can clean up
    if (m_client_name_address != nullptr)
    {
        std::free(m_client_name_address);
        m_client_name_address = nullptr;
    }

    if (m_client_domain_username != nullptr)
    {
        std::free(m_client_domain_username);
        m_client_domain_username = nullptr;
    }
    if (m_client_name_name != nullptr)
    {
        std::free(m_client_name_name);
        m_client_name_name = nullptr;
    }

    // If we have a socket then kill it
    if (m_socket != nullptr)
    {
        vnclog.Print(LL_INTINFO, VNCLOG("deleting socket\n"));

        delete m_socket;
        m_socket = nullptr;
    }
    if (m_pRawCacheZipBuf != nullptr)
    {
        delete[] m_pRawCacheZipBuf;
        m_pRawCacheZipBuf = nullptr;
    }
    if (m_pCacheZipBuf != nullptr)
    {
        delete[] m_pCacheZipBuf;
        m_pCacheZipBuf = nullptr;
    }
    if (m_lpCSBuffer)
    {
        delete[] m_lpCSBuffer;
    }
    if (m_pBuff)
    {
        delete[] m_pBuff;
    }
    if (m_pCompBuff)
    {
        delete[] m_pCompBuff;
    }

    //thos give sometimes errors, hlogfile is already removed at this point
    //vnclog.Print(LL_INTINFO, VNCLOG("cached %d \n"),totalraw);

    if ((settings->getScExit() || (settings->RunningFromExternalServiceRdp())) &&
        !fShutdownOrdered) // if fShutdownOrdered, hwnd may not be valid
    {
        //adzm 2009-06-20 - if we are SC, only exit if no other viewers are connected!
        // (since multiple viewers is now allowed with the new DSM plugin)
        // adzm 2009-08-02
        // adzm 2010-08 - stay alive if we have an UnauthClientCount as well, since another connection may be pending
        if ((m_server == nullptr) || (m_server && (m_server->AuthClientCount() == 0) && (m_server->UnauthClientCount() == 0)))
        {
            // We want that the server exit when the viewer exit
            //adzm 2010-02-10 - Finds the appropriate VNC window for this process
            HWND hwnd = postHelper::FindWinVNCWindow(true);
            if (hwnd)
            {
                ::SendMessage(hwnd, WM_COMMAND, ID_CLOSE, 0);
            }
        }
    }

    // adzm 2009-07-05
    if (m_szRepeaterID)
    {
        std::free(m_szRepeaterID);
    }
    // adzm 2009-08-02
    if (m_szHost)
    {
        std::free(m_szHost);
    }
    if (m_updatethread)
    {
        m_updatethread->Kill();
        m_updatethread->join(nullptr);
    }

    int counter{ 0 };
    while (m_updatethread != nullptr)
    {
        std::array<char, 256U> szText;
        ::sprintf_s(szText.data(), szText.size(), " m_updatethread != NULL \n");
        ::OutputDebugString(szText.data());
        ::Sleep(100);
        ++counter;
        if (counter == 30)
        {
            break;
        }
    }
    if (ThreadHandleCompressFolder)
    {
        ::WaitForSingleObject(ThreadHandleCompressFolder, INFINITE);
        ::CloseHandle(ThreadHandleCompressFolder);
    }
    if (simulateCursor)
    {
        delete simulateCursor;
    }
}

// Init
BOOL vncClient::Init(vncServer* server, VSocket* socket, const BOOL auth, const BOOL shared, const vncClientId newid)
{
    // Save the server id;
    m_server = server;

    // Save the socket
    m_socket = socket;

    // Save the name of the connecting client
    char* name = m_socket->GetPeerName(true);
    if (name != nullptr)
    {
        m_client_name_name = _strdup(name);
    }
    else
    {
        m_client_name_name = _strdup("<unknown>");
    }

    char* address = m_socket->GetPeerName(false);
    if (address != nullptr)
    {
        m_client_name_address = _strdup(address);
    }
    else
    {
        m_client_name_address = _strdup("<unknown>");
    }

    // Save the client id
    m_id = newid;

    // Spawn the child thread here
    m_thread_ClientThread = new vncClientThread;
    if (m_thread_ClientThread == nullptr)
    {
        return FALSE;
    }
    return (reinterpret_cast<vncClientThread*>(m_thread_ClientThread))->Init(this, m_server, m_socket, auth, shared);

    return FALSE;
}

void vncClient::Kill(bool deleted)
{
    // Close the socket
    vnclog.Print(LL_INTERR, VNCLOG("client Kill() called"));
    if (m_pTextChat)
    {
        m_pTextChat->KillDialog();
    }
    if (m_thread_ClientThread)
    {
        (reinterpret_cast<vncClientThread*>(m_thread_ClientThread))->ReleaseVeNCrypt();
        (reinterpret_cast<vncClientThread*>(m_thread_ClientThread))->ReleaseRA2();
    }
    if (m_socket != nullptr)
    {
        m_socket->Close();
    }
    if (deleted && m_thread_ClientThread)
    {
        (reinterpret_cast<vncClientThread*>(m_thread_ClientThread))->m_deleted = true;
    }
    if (m_server->virtualDisplay)
    {
        m_server->virtualDisplay->disconnectDisplay(m_id, (!m_server->AreThereMultipleViewers() && initialCapture_done));
    }
}

// Client manipulation functions for use by the server
void vncClient::SetBuffer(vncBuffer* const buffer)
{
    // Until authenticated, the client object has no access
    // to the screen buffer. This means that there only need
    // be a buffer when there's at least one authenticated client.
    m_encodemgr.SetBuffer(buffer);
}

//helper to trigger update
bool vncClient::NotifyUpdate(const rfbFramebufferUpdateRequestMsg fur)
{
    rfb::Rect update;
    m_ScaledScreen = m_encodemgr.m_buffer->GetViewerSize();
    rfb::Region2D update_rgn;

    // Extended display with a single screen, block all updates on the other screens when all monitors
    // is selected. This minimize compression and network traffic
    monitor mon = m_encodemgr.m_buffer->m_desktop->devicenaamToPosMap[displayname.data()];
    if (mon.Height && mon.Width && (m_encodemgr.m_buffer->m_desktop->m_current_monitor == MULTI_MON_ALL))
    {
        m_ScaledScreen.tl.x = static_cast<int>(mon.offsetx / m_nScale);
        m_ScaledScreen.tl.y = static_cast<int>(mon.offsety / m_nScale);
        m_ScaledScreen.br.x = static_cast<int>((mon.Width + mon.offsetx) / m_nScale);
        m_ScaledScreen.br.y = static_cast<int>((mon.Height + mon.offsety) / m_nScale);
    }

    if (!fur.incremental)
    {
        if (m_use_ExtDesktopSize && m_firstExtDesktop)
        {
            m_NewSWUpdateWaiting = true;
        }
        update.tl.x = static_cast<int>((m_ScaledScreen.tl.x + monitor_Offsetx) * m_nScale);
        update.tl.y = static_cast<int>((m_ScaledScreen.tl.y + monitor_Offsety) * m_nScale);
        update.br.x = static_cast<int>(update.tl.x + (m_ScaledScreen.br.x - m_ScaledScreen.tl.x) * m_nScale);
        update.br.y = static_cast<int>(update.tl.y + (m_ScaledScreen.br.y - m_ScaledScreen.tl.y) * m_nScale);
        update_rgn = update;
        if (update_rgn.is_empty())
        {
            return false;
        }
        m_update_tracker.add_changed(update_rgn);
        m_encodemgr.m_buffer->m_desktop->UpdateFullScreen();
    }
    else
    {
        if (m_firstExtDesktopIncremental)
        {
            //The first full was used for the extDesktopSize, we send it now
            update.tl.x = static_cast<int>((m_ScaledScreen.tl.x + monitor_Offsetx) * m_nScale);
            update.tl.y = static_cast<int>((m_ScaledScreen.tl.y + monitor_Offsety) * m_nScale);
            update.br.x = static_cast<int>(update.tl.x + (m_ScaledScreen.br.x - m_ScaledScreen.tl.x) * m_nScale);
            update.br.y = static_cast<int>(update.tl.y + (m_ScaledScreen.br.y - m_ScaledScreen.tl.y) * m_nScale);
            update_rgn = update;
            if (update_rgn.is_empty())
            {
                return false;
            }
            m_update_tracker.add_changed(update_rgn);
            m_encodemgr.m_buffer->m_desktop->UpdateFullScreen();
            m_firstExtDesktopIncremental = false;
        }
        else
        {
            update.tl.x = static_cast<int>((Swap16IfLE(fur.x) + monitor_Offsetx) * m_nScale);
            update.tl.y = static_cast<int>((Swap16IfLE(fur.y) + monitor_Offsety) * m_nScale);
            update.br.x = static_cast<int>(update.tl.x + Swap16IfLE(fur.w) * m_nScale);
            update.br.y = static_cast<int>(update.tl.y + Swap16IfLE(fur.h) * m_nScale);
            // Verify max size, scaled changed on server while not pushed to viewer
            if (update.tl.x < static_cast<int>(((static_cast<UINT>(m_ScaledScreen.tl.x + monitor_Offsetx)) * m_nScale)))
            {
                update.tl.x = static_cast<int>((m_ScaledScreen.tl.x + monitor_Offsetx) * m_nScale);
            }
            if (update.tl.y < static_cast<int>(((static_cast<UINT>(m_ScaledScreen.tl.y + monitor_Offsety)) * m_nScale)))
            {
                update.tl.y = static_cast<int>((m_ScaledScreen.tl.y + monitor_Offsety) * m_nScale);
            }
            if (update.br.x > static_cast<int>((static_cast<UINT>(update.tl.x) + (static_cast<UINT>(m_ScaledScreen.br.x - m_ScaledScreen.tl.x)) * m_nScale)))
            {
                update.br.x = static_cast<int>(update.tl.x + (m_ScaledScreen.br.x - m_ScaledScreen.tl.x) * m_nScale);
            }
            if (update.br.y > static_cast<int>((static_cast<UINT>(update.tl.y) + (static_cast<UINT>(m_ScaledScreen.br.y - m_ScaledScreen.tl.y)) * m_nScale)))
            {
                update.br.y = static_cast<int>(update.tl.y + (m_ScaledScreen.br.y - m_ScaledScreen.tl.y) * m_nScale);
            }
            update_rgn = update;
            if (update_rgn.is_empty())
            {
                return false;
            }
        }
    }
    /*#ifdef _DEBUG
        OutputDevMessage("Update Rect %i %i %i %i", update.tl.x, update.tl.y, update.br.x - update.tl.x, update.br.y - update.tl.y);
        OutputDevMessage("++++++ rfbFramebufferUpdateRequestMsg");
    #endif*/
    m_incr_rgn.assign_union(update_rgn);

    // Kick the update thread (and create it if not there already)
    TriggerUpdate();
    TriggerUpdateThread();
    /*#ifdef _DEBUG
        static DWORD sNotifyLastCopy = GetTickCount();
        DWORD now = GetTickCount();;
        OutputDevMessage("%4d", now - sNotifyLastCopy);
        sNotifyLastCopy = now;
    #endif*/
    return TRUE;
}

void vncClient::TriggerUpdateThread()
{
    // ALWAYS lock the client UpdateLock before calling this!
    // RealVNC 336
    // Check that this client has an update thread
    // The update thread never dissappears, and this is the only
    // thread allowed to create it, so this can be done without locking.

    if (!m_updatethread)
    {
        if (m_server->AreThereMultipleViewers())
        {
            cl_connected = TRUE;
        }
        m_updatethread = new vncClientUpdateThread;
        if (!m_updatethread || !m_updatethread->Init(this))
        {
            Kill();
        }
    }
    if (m_updatethread)
    {
        m_updatethread->Trigger();
    }
}

void vncClient::UpdateMouse()
{
    RECT testrect{};
    testrect.top = m_encodemgr.m_buffer->m_desktop->m_Cliprect.tl.y + m_ScreenOffsety + monitor_Offsety;
    testrect.bottom = m_encodemgr.m_buffer->m_desktop->m_Cliprect.br.y + m_ScreenOffsety + monitor_Offsety;
    testrect.left = m_encodemgr.m_buffer->m_desktop->m_Cliprect.tl.x + m_ScreenOffsetx + monitor_Offsetx;
    testrect.right = m_encodemgr.m_buffer->m_desktop->m_Cliprect.br.x + m_ScreenOffsetx + monitor_Offsetx;

    //vnclog.Print(LL_INTINFO, VNCLOG("UpdateMouse sx %d sy %d mx %d my %d\n"), m_ScreenOffsetx, m_ScreenOffsety, monitor_Offsetx,
    //monitor_Offsety); vnclog.Print(LL_INTINFO, VNCLOG("UpdateMouse Rect %d %d %d %d\n"), testrect.left, testrect.top,
    //testrect.right, testrect.bottom);

    POINT cursorPos;
    ::GetCursorPos(&cursorPos);
    if (!::PtInRect(&testrect, cursorPos))
    {
        //vnclog.Print(LL_INTINFO, VNCLOG("UpdateMouse !PtInRect %d %d\n"), cursorPos.x, cursorPos.y);
        return;
    }

    if (!m_mousemoved && !m_cursor_update_sent)
    {
        omni_mutex_lock l(GetUpdateLock(), 93);
        m_mousemoved = TRUE;
    }
    // nyama/marscha - PointerPos
    // PointerPos code doesn take in account prim/secundary display
    // offset needed
    if (m_use_PointerPos && !m_cursor_pos_changed)
    {
        POINT cursorPosInner;
        ::GetCursorPos(&cursorPosInner);
        cursorPosInner.x = cursorPosInner.x - (m_ScreenOffsetx + monitor_Offsetx);
        cursorPosInner.y = cursorPosInner.y - (m_ScreenOffsety + monitor_Offsety);
        //vnclog.Print(LL_INTINFO, VNCLOG("UpdateMouse m_cursor_pos(%d, %d), new(%d, %d)\n"),
        //    m_cursor_pos.x, m_cursor_pos.y, cursorPos.x, cursorPos.y);
        if ((cursorPosInner.x != m_cursor_pos.x) || (cursorPosInner.y != m_cursor_pos.y))
        {
            // This movement isn't by this client, but generated locally or by other client.
            // Send it to this client.
            omni_mutex_lock l(GetUpdateLock(), 94);
            m_cursor_pos.x = cursorPosInner.x;
            m_cursor_pos.y = cursorPosInner.y;
            m_cursor_pos_changed = TRUE;
            TriggerUpdateThread();
        }
    }
}

// adzm - 2010-07 - Extended clipboard
/*
void
vncClient::UpdateClipText(const char* text)
{
    //This is already locked in the vncdesktopsynk
    //omni_mutex_lock l(GetUpdateLock());
    if (m_clipboard_text) {
        free(m_clipboard_text);
        m_clipboard_text = 0;
    }
    m_clipboard_text = _strdup(text);
    TriggerUpdateThread();
}
*/

void vncClient::AcceptClipboardData(const ClipboardData& data)
{
    omni_mutex_lock l(GetUpdateLock(), 95);
    m_clipboard.AcceptRemoteData(data);
}

// adzm - 2010-07 - Extended clipboard
void vncClient::UpdateClipTextEx(ClipboardData& clipboardData, const CARD32 overrideFlags)
{
    //This is already locked in the vncdesktopsynk
    //But it's just a critical section, doesn't matter how often you lock it in the same thread
    //as long as it is unlocked the same number of times.
    omni_mutex_lock l(GetUpdateLock(), 95);
    if (m_clipboard.UpdateClipTextEx(clipboardData, overrideFlags))
    {
        TriggerUpdateThread();
    }
}

void vncClient::UpdateCursorShape()
{
    //omni_mutex_lock l(GetUpdateLock(),96);
    TriggerUpdateThread();
}

void vncClient::UpdatePalette(const bool lock)
{
    if (lock)
    {
        omni_mutex_lock l(GetUpdateLock(), 97);
    }
    m_palettechanged = TRUE;
}

void vncClient::UpdateLocalFormat(const bool lock)
{
    if (lock)
    {
        DisableProtocol();
    }
    else
    {
        DisableProtocol_no_mutex();
    }
    vnclog.Print(LL_INTERR, VNCLOG("updating local pixel format\n"));
    m_encodemgr.SetServerFormat();
    if (lock)
    {
        EnableProtocol();
    }
    else
    {
        EnableProtocol_no_mutex();
    }
}

BOOL vncClient::SetNewSWSize(const long w, const long h, const BOOL Desktop)
{
    if (!m_use_NewSWSize)
    {
        return FALSE;
    }
    DisableProtocol_no_mutex();

    vnclog.Print(LL_INTERR, VNCLOG("updating local pixel format and buffer size\n"));
    m_encodemgr.SetServerFormat();
    m_palettechanged = TRUE;
    if (Desktop)
    {
        m_encodemgr.SetEncoding(0, TRUE);
    } //0=dummy
    m_NewSWUpdateWaiting = true;
    NewsizeW = w;
    NewsizeH = h;
    EnableProtocol_no_mutex();

    return TRUE;
}

const char* vncClient::GetClientDomainUsername() { return m_client_domain_username; }

// Functions used to set and retrieve the client settings
const char* vncClient::GetClientNameName() { return m_client_name_name; }

const char* vncClient::GetClientNameAddress() { return m_client_name_address; }

// Enabling and disabling clipboard/GFX updates
void vncClient::DisableProtocol()
{
    BOOL disable = FALSE;
    {
        omni_mutex_lock l(GetUpdateLock(), 98);
        if (m_disable_protocol == 0)
        {
            disable = TRUE;
        }
        ++m_disable_protocol;
        if (disable && m_updatethread)
        {
            m_updatethread->EnableUpdates(FALSE);
        }
    }
}

void vncClient::EnableProtocol()
{
    {
        omni_mutex_lock l(GetUpdateLock(), 99);
        if (m_disable_protocol == 0)
        {
            vnclog.Print(LL_INTERR, VNCLOG("protocol enabled too many times!\n"));
            m_socket->Close();
            return;
        }
        --m_disable_protocol;
        if ((m_disable_protocol == 0) && m_updatethread)
        {
            m_updatethread->EnableUpdates(TRUE);
        }
    }
}

void vncClient::DisableProtocol_no_mutex()
{
    BOOL disable = FALSE;
    {
        if (m_disable_protocol == 0)
        {
            disable = TRUE;
        }
        ++m_disable_protocol;
        if (disable && m_updatethread)
        {
            m_updatethread->EnableUpdates(FALSE);
        }
    }
}

void vncClient::EnableProtocol_no_mutex()
{
    {
        if (m_disable_protocol == 0)
        {
            vnclog.Print(LL_INTERR, VNCLOG("protocol enabled too many times!\n"));
            m_socket->Close();
            return;
        }
        --m_disable_protocol;
        if ((m_disable_protocol == 0) && m_updatethread)
        {
            m_updatethread->EnableUpdates(TRUE);
        }
    }
}

// Internal methods
BOOL vncClient::SendRFBMsg(const CARD8 type, BYTE* const buffer, const int buflen)
{
    // Set the message type
    (reinterpret_cast<rfbServerToClientMsg*>(buffer))->type = type;

    // Send the message
    if (!m_socket->SendExact(reinterpret_cast<char*>(buffer), static_cast<VCard>(buflen), type))
    {
        vnclog.Print(LL_CONNERR, VNCLOG("failed to send RFB message to client\n"));

        Kill();
        return FALSE;
    }
    return TRUE;
}

//adzm 2010-09 - minimize packets. SendExact flushes the queue.
BOOL vncClient::SendRFBMsgQueue(const CARD8 type, BYTE* const buffer, const int buflen)
{
    // Set the message type
    (reinterpret_cast<rfbServerToClientMsg*>(buffer))->type = type;

    // Send the message
    if (!m_socket->SendExactQueue(reinterpret_cast<char*>(buffer), static_cast<VCard>(buflen), type))
    {
        vnclog.Print(LL_CONNERR, VNCLOG("failed to send RFB message to client\n"));

        Kill();
        return FALSE;
    }
    return TRUE;
}

BOOL vncClient::SendUpdate(rfb::SimpleUpdateTracker& update)
{
    // If there is nothing to send then exit

    if (update.is_empty() && !m_cursor_update_pending && !m_NewSWUpdateWaiting && !m_cursor_pos_changed)
    {
        return FALSE;
    }

    // Get the update info from the tracker
    rfb::UpdateInfo update_info;
    update.get_update(update_info);
    update.clear();
    //Old update could be outsite the new bounding
    //We first make sure the new size is send to the client
    //The client ask a full update after screen_size change
    if (m_NewSWUpdateWaiting)
    {
        m_socket->ClearQueue();
        rfbFramebufferUpdateRectHeader hdr{};
        m_ScaledScreen = m_encodemgr.m_buffer->GetViewerSize();
        m_nScale = m_encodemgr.m_buffer->GetScale();
        rfbExtDesktopSizeMsg edsHdr{};
        rfbExtDesktopScreen eds{};
        bool sizeUpdateSent{ false };

        if (m_use_ExtDesktopSize && m_firstExtDesktop)
        {
            HDESK desktop = ::GetThreadDesktop(::GetCurrentThreadId());
            DWORD dummy;
            std::array<char, 256U> new_name{};
            if (::GetUserObjectInformation(desktop, UOI_NAME, &new_name, 256, &dummy) && (std::strcmp(new_name.data(), "Default") == 0))
            {
                //Send ExtDesktopSize pseudo - rectangle to inform the client we support size changes through setDesktopSize
                hdr.encoding = Swap32IfLE(rfbEncodingExtDesktopSize);
                hdr.r.w = Swap16IfLE(m_encodemgr.m_buffer->m_desktop->m_scrinfo.framebufferWidth * m_nScale_viewer / m_nScale);
                hdr.r.h = Swap16IfLE(m_encodemgr.m_buffer->m_desktop->m_scrinfo.framebufferHeight * m_nScale_viewer / m_nScale);
                hdr.r.x = Swap16IfLE(m_server->m_virtualDisplaySupported);
                hdr.r.y = Swap16IfLE(m_lastDesktopSizeChangeError);
                edsHdr.numberOfScreens = 1;
                edsHdr.pad[0U] = edsHdr.pad[1U] = edsHdr.pad[2U] = 0;
                for (int i{ 0 }; i < 1; ++i)
                {
                    eds.id = Swap32IfLE(1);
                    eds.x = Swap16IfLE(0);
                    eds.y = Swap16IfLE(0);
                    eds.width =
                        Swap16IfLE(m_encodemgr.m_buffer->m_desktop->m_scrinfo.framebufferWidth * m_nScale_viewer / m_nScale);
                    eds.height =
                        Swap16IfLE(m_encodemgr.m_buffer->m_desktop->m_scrinfo.framebufferHeight * m_nScale_viewer / m_nScale);
                    eds.flags = Swap32IfLE(0);
                }
                rfbFramebufferUpdateMsg header{};
                header.nRects = Swap16IfLE(1);
                //adzm 2010-09 - minimize packets. SendExact flushes the queue.
                SendRFBMsgQueue(rfbFramebufferUpdate, reinterpret_cast<BYTE*>(&header), sz_rfbFramebufferUpdateMsg);
                m_socket->SendExact(reinterpret_cast<char*>(&hdr), sizeof(hdr));
                m_socket->SendExact(reinterpret_cast<char*>(&edsHdr), sizeof(edsHdr));
                m_socket->SendExact(reinterpret_cast<char*>(&eds), sizeof(eds));
                m_use_ExtDesktopSize = false;
                m_firstExtDesktop = false;
                sizeUpdateSent = true;
            }
        }
        else if (m_use_NewSWSize)
        {
            hdr.r.x = 0;
            hdr.r.y = 0;
            hdr.r.w = Swap16IfLE(static_cast<UINT>(NewsizeW) * m_nScale_viewer / m_nScale);
            hdr.r.h = Swap16IfLE(static_cast<UINT>(NewsizeH) * m_nScale_viewer / m_nScale);
            hdr.encoding = Swap32IfLE(rfbEncodingNewFBSize);
            rfbFramebufferUpdateMsg header{};
            header.nRects = Swap16IfLE(1);
            //adzm 2010-09 - minimize packets. SendExact flushes the queue.
            SendRFBMsgQueue(rfbFramebufferUpdate, reinterpret_cast<BYTE*>(&header), sz_rfbFramebufferUpdateMsg);
            m_socket->SendExact(reinterpret_cast<char*>(&hdr), sizeof(hdr));
            if (m_singleExtendMode)
            {
                //send position of the virtual screen
                m_encodemgr.m_buffer->m_desktop->Checkmonitors();
                m_encodemgr.m_buffer->m_desktop->m_current_monitor = MULTI_MON_ALL;
                m_encodemgr.m_buffer->SetAllMonitors(true);
                monitor mon = m_encodemgr.m_buffer->m_desktop->devicenaamToPosMap[displayname.data()];
                hdr.r.x = Swap16IfLE(static_cast<UINT>(mon.offsetx) * m_nScale_viewer / m_nScale);
                hdr.r.y = Swap16IfLE(static_cast<UINT>(mon.offsety) * m_nScale_viewer / m_nScale);
                hdr.r.w = Swap16IfLE(static_cast<UINT>(mon.Width) * m_nScale_viewer / m_nScale);
                hdr.r.h = Swap16IfLE(static_cast<UINT>(mon.Height) * m_nScale_viewer / m_nScale);
                hdr.encoding = Swap32IfLE(rfbEncodingExtViewSize);
                rfbFramebufferUpdateMsg headerInner{};
                headerInner.nRects = Swap16IfLE(1);
                SendRFBMsgQueue(rfbFramebufferUpdate, reinterpret_cast<BYTE*>(&headerInner), sz_rfbFramebufferUpdateMsg);
                m_socket->SendExact(reinterpret_cast<char*>(&hdr), sizeof(hdr));
            }
            sizeUpdateSent = true;
        }
        if (sizeUpdateSent)
        {
            m_NewSWUpdateWaiting = false;
            return TRUE;
        }
        // The size-change rectangle could not be sent because the thread is not
        // on the Default desktop (e.g. attached to a secure desktop). Keep the
        // ExtendedDesktopSize update pending so it is retried once the Default
        // desktop is back, and fall through so the queued image update is
        // still sent.
        if (!(m_use_ExtDesktopSize && m_firstExtDesktop))
        {
            m_NewSWUpdateWaiting = false;
        }
    }

    // Find out how many rectangles in total will be updated
    // This includes copyrects and changed rectangles split
    // up by codings such as CoRRE.
    int updates{ 0 };
    int numsubrects{ 0 };
    updates += static_cast<int>(update_info.copied.size());
    if (m_encodemgr.IsCacheEnabled())
    {
        if (update_info.cached.size() > 5)
        {
            ++updates;
        }
        else
        {
            updates += static_cast<int>(update_info.cached.size());
            //vnclog.Print(LL_INTERR, "cached %d\n", updates);
        }
    }

    rfb::RectVector::const_iterator i;
    if (updates != 0xFFFF)
    {
        for (i = update_info.changed.begin(); i != update_info.changed.end(); ++i)
        {
            // Tight specific (lastrect)
            numsubrects = static_cast<int>(m_encodemgr.GetNumCodedRects(*i));

            // Skip rest rectangles if an encoder will use LastRect extension.
            if (numsubrects == 0)
            {
                updates = 0xFFFF;
                break;
            }
            updates += numsubrects;
            //vnclog.Print(LL_INTERR, "changed %d\n", updates);
        }
    }

    // if no cache is supported by the other viewer
    // We need to send the cache as a normal update
    if (!m_encodemgr.IsCacheEnabled() && (updates != 0xFFFF))
    {
        for (i = update_info.cached.begin(); i != update_info.cached.end(); ++i)
        {
            // Tight specific (lastrect)
            numsubrects = static_cast<int>(m_encodemgr.GetNumCodedRects(*i));

            // Skip rest rectangles if an encoder will use LastRect extension.
            if (numsubrects == 0)
            {
                updates = 0xFFFF;
                //break;
            }
            else
            {
                updates += numsubrects;
            }
            //vnclog.Print(LL_INTERR, "cached2 %d\n", updates);
        }
    }

    //
    if (!m_encodemgr.IsXCursorSupported())
    {
        m_cursor_update_pending = false;
    }
    // Tight specific (lastrect)
    if (updates != 0xFFFF)
    {
        // Tight - CURSOR HANDLING
        if (m_cursor_update_pending)
        {
            ++updates;
        }
        // nyama/marscha - PointerPos
        if (m_cursor_pos_changed)
        {
            ++updates;
        }
        if (updates == 0)
        {
            return FALSE;
        }
    }

    //	Sendtimer.start();

    omni_mutex_lock l(GetUpdateLock(), 100);
    // Otherwise, send <number of rectangles> header
    rfbFramebufferUpdateMsg header{};
    header.nRects = Swap16IfLE(updates);
    //adzm 2010-09 - minimize packets. SendExact flushes the queue.
    if (!SendRFBMsgQueue(rfbFramebufferUpdate, reinterpret_cast<BYTE*>(&header), sz_rfbFramebufferUpdateMsg))
    {
        return FALSE;
    }

    // CURSOR HANDLING
    if (m_cursor_update_pending)
    {
        if (!SendCursorShapeUpdate())
        {
            return FALSE;
        }
    }
    // nyama/marscha - PointerPos
    if (m_cursor_pos_changed)
    {
        if (!SendCursorPosUpdate())
        {
            return FALSE;
        }
    }

    // Send the copyrect rectangles
    if (!update_info.copied.empty())
    {
        rfb::Point to_src_delta = update_info.copy_delta.negate();
        for (i = update_info.copied.begin(); i != update_info.copied.end(); ++i)
        {
            rfb::Point src = (*i).tl.translate(to_src_delta);
            if (!SendCopyRect(*i, src))
            {
                return FALSE;
            }
        }
    }

    if (m_encodemgr.IsCacheEnabled())
    {
        if (update_info.cached.size() > 5)
        {
            if (!SendCacheZip(update_info.cached))
            {
                return FALSE;
            }
        }
        else
        {
            if (!SendCacheRectangles(update_info.cached))
            {
                return FALSE;
            }
        }
    }
    else
    {
        if (!SendRectangles(update_info.cached))
        {
            return FALSE;
        }
    }

    if (!SendRectangles(update_info.changed))
    {
        return FALSE;
    }
    // Tight specific - Send LastRect marker if needed.
    if (updates == 0xFFFF)
    {
        m_encodemgr.LastRect(m_socket);
        if (!SendLastRect())
        {
            return FALSE;
        }
    }
    m_socket->ClearQueue();
    // vnclog.Print(LL_INTINFO, VNCLOG("Update cycle\n"));
    return TRUE;
}

// Send a set of rectangles
BOOL vncClient::SendRectangles(const rfb::RectVector& rects)
{
    rfb::RectVector::const_iterator i;
    rfb::Rect rect;
    //int x,y;
    //int Blocksize=1920;
    //int BlocksizeX=1200;

#ifdef _XZ
    if (m_encodemgr.IsBulkRectEncoding())
    {
        return static_cast<BOOL>(m_encodemgr.EncodeBulkRects(rects, m_nScale, m_socket));
    }
#endif

    // Work through the list of rectangles, sending each one
    for (i = rects.begin(); i != rects.end(); ++i)
    {
        rect.tl.x = (*i).tl.x;
        rect.br.x = (*i).br.x;
        rect.tl.y = (*i).tl.y;
        rect.br.y = (*i).br.y;
        /*#ifdef _DEBUG
                    char			szText[256];

                        sprintf_s(szText,"SendRectangles  %i %i %i %i \n",rect.tl.x,
                        rect.tl.y,
                        rect.br.x,
                        rect.br.y);
                        OutputDebugString(szText);
        #endif*/

        /*if (m_encodemgr.ultra2_encoder_in_use)
        {
            if ((rect.br.x-rect.tl.x) * (rect.br.y-rect.tl.y) > Blocksize*BlocksizeX )
            {
            for (y = rect.tl.y; y < rect.br.y; y += Blocksize)
            {
                int blockbottom = std::min(y + Blocksize, rect.br.y);
                for (x = rect.tl.x; x < rect.br.x; x += BlocksizeX)
                    {
                       int blockright = std::min(x+BlocksizeX, rect.br.x);
                       rfb::Rect tilerect;
                       tilerect.tl.x=x;
                       tilerect.br.x=blockright;
                       tilerect.tl.y=y;
                       tilerect.br.y=blockbottom;
                       if (!SendRectangle(tilerect)) return FALSE;
                    }
            }
            }
            else
            {
                if (!SendRectangle(rect)) return FALSE;
            }
        }
        else*/
        {
            if (!SendRectangle(*i))
            {
                return FALSE;
            }
        }
    }
    return TRUE;
}

// Tell the encoder to send a single rectangle
BOOL vncClient::SendRectangle(const rfb::Rect& rect)
{
    // Get the buffer to encode the rectangle
    // Modif sf@2002 - Scaling
    rfb::Rect ScaledRect;
    ScaledRect.tl.y = static_cast<int>(rect.tl.y / m_nScale);
    ScaledRect.br.y = static_cast<int>(rect.br.y / m_nScale);
    ScaledRect.tl.x = static_cast<int>(rect.tl.x / m_nScale);
    ScaledRect.br.x = static_cast<int>(rect.br.x / m_nScale);
    /*#ifdef _DEBUG
    char			szText[256];
    sprintf_s(szText,"++++++++++++++++++++++++++++++++++++++++++++++REct1 %i %i %i %i  \n",rect.tl.x,rect.br.x,rect.tl.y,rect.br.y);
    OutputDebugString(szText);
    #endif*/

    //	Totalsend+=(ScaledRect.br.x-ScaledRect.tl.x)*(ScaledRect.br.y-ScaledRect.tl.y);

    // sf@2002 - DSMPlugin
    // Some encoders (Hextile, ZRLE, Raw..) store all the data to send into
    // m_clientbuffer and return the total size from EncodeRect()
    // Some Encoders (Tight, Zlib, ZlibHex..) send data on the fly and return
    // a partial size from EncodeRect().
    // On the viewer side, the data is read piece by piece or in one shot
    // still depending on the encoding...
    // It is not compatible with DSM: we need to read/write data blocks of same
    // size on both sides in one shot
    // We create a common method to send the data
    // adzm 2010-09
    if (!m_socket->IsPluginStreamingOut() && m_socket->IsUsePluginEnabled() && m_server->GetDSMPluginPointer()->IsEnabled())
    {
        // Tell the SendExact() calls to write into the local NetRectBuffer memory buffer
        m_socket->SetWriteToNetRectBuffer(true);
        m_socket->SetNetRectBufOffset(0);
        // sf@2003 - we can't easely predict how many rects are going to be sent
        // (for Tight encoding for instance)
        // Then we take the worse case (screen buffer size * 1.5) for the net rect buffer size.
        // m_socket->CheckNetRectBufferSize((int)(m_encodemgr.GetClientBuffSize() * 2));
        m_socket->CheckNetRectBufferSize(static_cast<int>((m_encodemgr.m_buffer->m_desktop->ScreenBuffSize() * 3 / 2)));
        UINT bytes = m_encodemgr.EncodeRect(ScaledRect, m_socket);
        if (bytes == 0)
        {
            return true;
        }
        m_socket->SetWriteToNetRectBuffer(false);

        BYTE* pDataBuffer{ nullptr };
        UINT TheSize{ 0U };

        // If SendExact() was called from inside the encoder
        if (m_socket->GetNetRectBufOffset() > 0)
        {
            TheSize = static_cast<UINT>(m_socket->GetNetRectBufOffset());
#ifdef _DEBUG
            std::array<char, 256U> szText;
            DWORD error = ::GetLastError();
            ::sprintf_s(szText.data(), szText.size(), " ++++++ crashtest TheSize %i \n", TheSize);
            SetLastError(0);
            ::OutputDebugString(szText.data());
#endif
            m_socket->SetNetRectBufOffset(0);
            pDataBuffer = m_socket->GetNetRectBuf();
            // Add the rest to the data buffer if it exists
            if (bytes > 0)
            {
                std::memcpy(pDataBuffer + TheSize, m_encodemgr.GetClientBuffer(), bytes);
            }
        }
        else // If all data was stored in m_clientbuffer
        {
            TheSize = bytes;
#ifdef _DEBUG
            std::array<char, 256U> szText;
            DWORD error = ::GetLastError();
            ::sprintf_s(szText.data(), szText.size(), " ++++++ crashtest TheSize2 %i \n", TheSize);
            SetLastError(0);
            ::OutputDebugString(szText.data());
#endif
            bytes = 0;
            pDataBuffer = m_encodemgr.GetClientBuffer();
        }

        // Send the header
        m_socket->SendExactQueue(reinterpret_cast<char*>(pDataBuffer), sz_rfbFramebufferUpdateRectHeader);

        // Send the size of the following rects data buffer
        CARD32 Size = static_cast<CARD32>((TheSize + bytes - sz_rfbFramebufferUpdateRectHeader));
#ifdef _DEBUG
        std::array<char, 256U> szText;
        DWORD error = ::GetLastError();
        ::sprintf_s(szText.data(), szText.size(), " ++++++ crashtest Size %i %i\n", Size, bytes);
        SetLastError(0);
        ::OutputDebugString(szText.data());
#endif
        Size = Swap32IfLE(Size);
        m_socket->SendExactQueue(reinterpret_cast<char*>(&Size), sizeof(CARD32));
        // Send the data buffer
        m_socket->SendExactQueue((reinterpret_cast<char*>(pDataBuffer) + sz_rfbFramebufferUpdateRectHeader),
                                 TheSize + bytes - sz_rfbFramebufferUpdateRectHeader);
    }
    else // Normal case - No DSM - Symetry is not important
    {
        UINT bytes = m_encodemgr.EncodeRect(ScaledRect, m_socket);

        // if (bytes == 0) return false; // From realvnc337. No! Causes viewer disconnections/

        // Send the encoded data
        return m_socket->SendExactQueue(reinterpret_cast<char*>((m_encodemgr.GetClientBuffer())), bytes);
    }

    return true;
}

// Send a single CopyRect message
BOOL vncClient::SendCopyRect(const rfb::Rect& dest, const rfb::Point& source)
{
    // Create the message header
    // Modif sf@2002 - Scaling
    rfbFramebufferUpdateRectHeader copyrecthdr{};
    copyrecthdr.r.x = Swap16IfLE((static_cast<UINT>(dest.tl.x - monitor_Offsetx)) / m_nScale);
    copyrecthdr.r.y = Swap16IfLE((static_cast<UINT>(dest.tl.y - monitor_Offsety)) / m_nScale);
    copyrecthdr.r.w = Swap16IfLE((static_cast<UINT>(dest.br.x - dest.tl.x)) / m_nScale);
    copyrecthdr.r.h = Swap16IfLE((static_cast<UINT>(dest.br.y - dest.tl.y)) / m_nScale);
    copyrecthdr.encoding = Swap32IfLE(rfbEncodingCopyRect);

    // Create the CopyRect-specific section
    rfbCopyRect copyrectbody{};
    copyrectbody.srcX = Swap16IfLE((static_cast<UINT>(source.x - monitor_Offsetx)) / m_nScale);
    copyrectbody.srcY = Swap16IfLE((static_cast<UINT>(source.y - monitor_Offsety)) / m_nScale);

    // Now send the message;
    if (!m_socket->SendExactQueue(reinterpret_cast<char*>(&copyrecthdr), sizeof(copyrecthdr)))
    {
        return FALSE;
    }
    if (!m_socket->SendExactQueue(reinterpret_cast<char*>(&copyrectbody), sizeof(copyrectbody)))
    {
        return FALSE;
    }

    return TRUE;
}

// Send the encoder-generated palette to the client
// This function only returns FALSE if the SendExact fails - any other
// error is coped with internally...
BOOL vncClient::SendPalette()
{
    rfbSetColourMapEntriesMsg setcmap{};
    RGBQUAD* rgbquad;
    UINT ncolours{ 256U };

    // Reserve space for the colour data
    rgbquad = new RGBQUAD[ncolours];
    if (rgbquad == nullptr)
    {
        return TRUE;
    }

    // Get the data
    if (!m_encodemgr.GetPalette(rgbquad, ncolours))
    {
        delete[] rgbquad;
        return TRUE;
    }

    // Compose the message
    setcmap.type = rfbSetColourMapEntries;
    setcmap.firstColour = Swap16IfLE(0);
    setcmap.nColours = Swap16IfLE(ncolours);

    //adzm 2010-09 - minimize packets. SendExact flushes the queue.
    if (!m_socket->SendExactQueue(reinterpret_cast<char*>(&setcmap), sz_rfbSetColourMapEntriesMsg, rfbSetColourMapEntries))
    {
        delete[] rgbquad;
        return FALSE;
    }

    // Now send the actual colour data...
    for (UINT i{ 0U }; i < ncolours; ++i)
    {
        struct _PIXELDATA
        {
            CARD16 r, g, b;
        } pixeldata{};

        pixeldata.r = Swap16IfLE((static_cast<CARD16>(rgbquad[i].rgbRed)) << 8);
        pixeldata.g = Swap16IfLE((static_cast<CARD16>(rgbquad[i].rgbGreen)) << 8);
        pixeldata.b = Swap16IfLE((static_cast<CARD16>(rgbquad[i].rgbBlue)) << 8);

        //adzm 2010-09 - minimize packets. SendExact flushes the queue.
        if (!m_socket->SendExactQueue(reinterpret_cast<char*>(&pixeldata), sizeof(pixeldata)))
        {
            delete[] rgbquad;
            return FALSE;
        }
    }

    // Delete the rgbquad data
    delete[] rgbquad;

    //adzm 2010-09 - minimize packets. SendExact flushes the queue.
    m_socket->ClearQueue();

    return TRUE;
}

void vncClient::SetBufferOffset(int x, int y)
{
    monitor_Offsetx = x;
    monitor_Offsety = y;
    m_encodemgr.SetBufferOffset(x, y);
}

void vncClient::SetScreenOffset(int x, int y, bool single_display)
{
    m_ScreenOffsetx = x;
    m_ScreenOffsety = y;
    m_single_display = single_display;
}

void vncClient::InitialUpdate(const bool value) { m_initial_update = value; }

// CACHE RDV
// Send a set of rectangles
BOOL vncClient::SendCacheRectangles(const rfb::RectVector& rects)
{
    //	rfb::Rect rect;
    rfb::RectVector::const_iterator i;

    if (rects.size() == 0)
    {
        return TRUE;
    }
    vnclog.Print(LL_INTINFO, VNCLOG("******** Sending %d Cache Rects \r\n"), rects.size());

    // Work through the list of rectangles, sending each one
    for (i = rects.begin(); i != rects.end(); ++i)
    {
        if (!SendCacheRect(*i))
        {
            return FALSE;
        }
    }

    return TRUE;
}

// Tell the encoder to send a single rectangle
BOOL vncClient::SendCacheRect(const rfb::Rect& dest)
{
    // Create the message header
    // Modif rdv@2002 - v1.1.x - Application Resize
    // Modif sf@2002 - Scaling
    rfbFramebufferUpdateRectHeader cacherecthdr{};
    cacherecthdr.r.x = Swap16IfLE((static_cast<UINT>(dest.tl.x - monitor_Offsetx)) / m_nScale);
    cacherecthdr.r.y = Swap16IfLE((static_cast<UINT>(dest.tl.y - monitor_Offsety)) / m_nScale);
    cacherecthdr.r.w = Swap16IfLE((static_cast<UINT>(dest.br.x - dest.tl.x)) / m_nScale);
    cacherecthdr.r.h = Swap16IfLE((static_cast<UINT>(dest.br.y - dest.tl.y)) / m_nScale);
    cacherecthdr.encoding = Swap32IfLE(rfbEncodingCache);

    totalraw += (dest.br.x - dest.tl.x) * (dest.br.y - dest.tl.y) * 32 / 8; // 32-bit test
    // Create the CopyRect-specific section
    rfbCacheRect cacherectbody{};
    cacherectbody.special = Swap16IfLE(9999); //not used dummy

    // Now send the message;
    if (!m_socket->SendExactQueue(reinterpret_cast<char*>(&cacherecthdr), sizeof(cacherecthdr)))
    {
        return FALSE;
    }
    if (!m_socket->SendExactQueue(reinterpret_cast<char*>(&cacherectbody), sizeof(cacherectbody)))
    {
        return FALSE;
    }
    return TRUE;
}

BOOL vncClient::SendCursorShapeUpdate()
{
    m_cursor_update_pending = FALSE;

    if (!m_encodemgr.SendCursorShape(m_socket))
    {
        m_cursor_update_sent = FALSE;
        return m_encodemgr.SendEmptyCursorShape(m_socket);
    }

    m_cursor_update_sent = TRUE;
    return TRUE;
}

BOOL vncClient::SendCursorPosUpdate()
{
    m_cursor_pos_changed = FALSE;

    rfbFramebufferUpdateRectHeader hdr{};
    hdr.r.x = Swap16IfLE(m_cursor_pos.x);
    hdr.r.y = Swap16IfLE(m_cursor_pos.y);
    hdr.r.w = 0;
    hdr.r.h = 0;
    hdr.encoding = Swap32IfLE(rfbEncodingPointerPos);

    if (!m_socket->SendExactQueue(reinterpret_cast<char*>(&hdr), sizeof(hdr)))
    {
        return FALSE;
    }

    return TRUE;
}

// Tight specific - Send LastRect marker indicating that there are no more rectangles to send
BOOL vncClient::SendLastRect()
{
    // Create the message header
    rfbFramebufferUpdateRectHeader hdr{};
    hdr.r.x = 0;
    hdr.r.y = 0;
    hdr.r.w = 0;
    hdr.r.h = 0;
    hdr.encoding = Swap32IfLE(rfbEncodingLastRect);

    // Now send the message;
    if (!m_socket->SendExactQueue(reinterpret_cast<char*>(&hdr), sizeof(hdr)))
    {
        return FALSE;
    }

    return TRUE;
}

//
// sf@2002 - New cache rects transport - Uses Zlib
//
//
BOOL vncClient::SendCacheZip(const rfb::RectVector& rects)
{
    //int totalCompDataLen = 0;

    int nNbCacheRects = static_cast<int>(rects.size());
    if (!nNbCacheRects)
    {
        return true;
    }
    unsigned long rawDataSize = static_cast<unsigned long>(nNbCacheRects * sz_rfbRectangle);
    unsigned long maxCompSize = (rawDataSize + (rawDataSize / 100) + 8);

    // Check RawCacheZipBuff
    // create a space big enough for the Zlib encoded cache rects list
    if (m_nRawCacheZipBufSize < rawDataSize)
    {
        if (m_pRawCacheZipBuf != nullptr)
        {
            delete[] m_pRawCacheZipBuf;
            m_pRawCacheZipBuf = nullptr;
        }
        m_pRawCacheZipBuf = new BYTE[rawDataSize + 1];
        if (m_pRawCacheZipBuf == nullptr)
        {
            return false;
        }
        m_nRawCacheZipBufSize = rawDataSize;
    }

    // Copy all the cache rects coordinates into the RawCacheZip Buffer
    rfbRectangle theRect{};
    rfb::RectVector::const_iterator i;
    BYTE* p = m_pRawCacheZipBuf;
    for (i = rects.begin(); i != rects.end(); ++i)
    {
        theRect.x = Swap16IfLE((static_cast<UINT>((*i).tl.x - monitor_Offsetx)) / m_nScale);
        theRect.y = Swap16IfLE((static_cast<UINT>((*i).tl.y - monitor_Offsety)) / m_nScale);
        theRect.w = Swap16IfLE((static_cast<UINT>((*i).br.x - (*i).tl.x)) / m_nScale);
        theRect.h = Swap16IfLE((static_cast<UINT>((*i).br.y - (*i).tl.y)) / m_nScale);
        std::memcpy(p, reinterpret_cast<BYTE*>(&theRect), sz_rfbRectangle);
        p += sz_rfbRectangle;
    }

    // Create a space big enough for the Zlib encoded cache rects list
    if (m_nCacheZipBufSize < maxCompSize)
    {
        if (m_pCacheZipBuf != nullptr)
        {
            delete[] m_pCacheZipBuf;
            m_pCacheZipBuf = nullptr;
        }
        m_pCacheZipBuf = new BYTE[maxCompSize + 1000];
        if (m_pCacheZipBuf == nullptr)
        {
            return 0;
        }
        m_nCacheZipBufSize = maxCompSize + 999;
    }

    int nRet =
        compress(reinterpret_cast<unsigned char*>((m_pCacheZipBuf)), reinterpret_cast<unsigned long*>(&maxCompSize), reinterpret_cast<unsigned char*>(m_pRawCacheZipBuf), rawDataSize);

    if (nRet != 0)
    {
        return false;
    }

    vnclog.Print(LL_INTINFO, VNCLOG("*** Sending CacheZip Rects=%d Size=%d (%d)\r\n"), nNbCacheRects, maxCompSize,
                 nNbCacheRects * 14);

    // Send the Update Rect header
    rfbFramebufferUpdateRectHeader CacheRectsHeader{};
    CacheRectsHeader.r.x = Swap16IfLE(nNbCacheRects);
    CacheRectsHeader.r.y = 0;
    CacheRectsHeader.r.w = 0;
    CacheRectsHeader.r.h = 0;
    CacheRectsHeader.encoding = Swap32IfLE(rfbEncodingCacheZip);

    // Format the ZlibHeader
    rfbZlibHeader CacheZipHeader{};
    CacheZipHeader.nBytes = Swap32IfLE(maxCompSize);

    // Now send the message
    if (!m_socket->SendExactQueue(reinterpret_cast<char*>(&CacheRectsHeader), sizeof(CacheRectsHeader)))
    {
        return FALSE;
    }
    if (!m_socket->SendExactQueue(reinterpret_cast<char*>(&CacheZipHeader), sizeof(CacheZipHeader)))
    {
        return FALSE;
    }
    if (!m_socket->SendExactQueue(reinterpret_cast<char*>(m_pCacheZipBuf), maxCompSize))
    {
        return FALSE;
    }

    return TRUE;
}

//
//
//
void vncClient::EnableCache(const BOOL enabled) { m_encodemgr.EnableCache(enabled); }

void vncClient::SetProtocolVersion(rfbProtocolVersionMsg* const protocolMsg)
{
    if (protocolMsg != nullptr)
    {
        std::memcpy(ProtocolVersionMsg, protocolMsg, sz_rfbProtocolVersionMsg);
    }
    else
    {
        ::strcpy_s(ProtocolVersionMsg, "0.0.0.0");
    }
}

void vncClient::Clear_Update_Tracker() { m_update_tracker.clear(); }

void vncClient::TriggerUpdate() { m_encodemgr.m_buffer->m_desktop->TriggerUpdate(); }

////////////////////////////////////////////////
// Asynchronous & Delta File Transfer functions
////////////////////////////////////////////////

//
// sf@2004 - Delta Transfer
// Create the checksums buffer of an open file
//
int vncClient::GenerateFileChecksums(const HANDLE hFile, char* const lpCSBuffer, const int nCSBufferSize)
{
    bool fEof{ false };
    bool fError{ false };
    DWORD dwNbBytesRead{ 0U };
    int nCSBufferOffset{ 0 };

    char* lpBuffer = new char[sz_rfbBlockSize];
    if (!lpBuffer)
    {
        return -1;
    }

    while (!fEof)
    {
        if (!::ReadFile(hFile, lpBuffer, sz_rfbBlockSize, &dwNbBytesRead, nullptr))
        {
            fError = true;
            break;
        }

        if (dwNbBytesRead == 0)
        {
            fEof = true;
        }
        else
        {
            if (nCSBufferOffset + 4 > nCSBufferSize)
            {
                fError = true;
                break;
            }

            unsigned long cs = adler32(0L, Z_NULL, 0);
            cs = adler32(cs, reinterpret_cast<unsigned char*>(lpBuffer), static_cast<int>(dwNbBytesRead));

            std::memcpy(lpCSBuffer + nCSBufferOffset, &cs, 4);
            nCSBufferOffset += 4;
        }
    }

    if (::SetFilePointer(hFile, 0L, nullptr, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
    {
        fError = true;
    }

    delete[] lpBuffer;

    return fError ? -1 : nCSBufferOffset;
}

//
// sf@2004 - Delta Transfer
// Destination file already exists
// The server sends the checksums of this file in one shot.
//
bool vncClient::ReceiveDestinationFileChecksums([[maybe_unused]] const int nSize, const int nLen)
{
    if ((nLen < 0) || (nLen > 104857600))
    { // 100 MBytes max
        return false;
    }

    // The client checksums cover the file it is uploading; a longer payload
    // than that file can ever need is a protocol error.
    if (nLen > 4 * static_cast<int>((m_dwFileSize / sz_rfbBlockSize)) + 1024)
    {
        return false;
    }

    // Free any buffer from an earlier message before replacing it.
    if (m_lpCSBuffer)
    {
        delete[] m_lpCSBuffer;
        m_lpCSBuffer = nullptr;
        m_nCSBufferSize = 0;
    }

    m_lpCSBuffer = new char[static_cast<unsigned long long>(nLen + 1)];
    if (m_lpCSBuffer == nullptr)
    {
        return false;
    }

    std::memset(m_lpCSBuffer, '\0', static_cast<size_t>(nLen + 1));

    VBool res = m_socket->ReadExact(reinterpret_cast<char*>(m_lpCSBuffer), static_cast<VCard>(nLen));
    m_nCSBufferSize = nLen;

    return res == VTrue;
}

//
//
//
bool vncClient::ReceiveFileChunk(const int nLen, const int nSize)
{
    bool connected{ true };

    if (!m_fFileDownloadRunning)
    {
        return connected;
    }

    if (m_fFileDownloadError)
    {
        FinishFileReception();
        return connected;
    }

    if (nLen < 0)
    {
        return false;
    }

    if (nLen > sz_rfbBlockSize)
    {
        return connected;
    }

    bool fCompressed{ true };
    BOOL fRes{ true };
    bool fAlreadyHere = (nSize == 2);

    // sf@2004 - Delta Transfer - Empty packet
    if (fAlreadyHere)
    {
        DWORD dwPtr = ::SetFilePointer(m_hDestFile, nLen, nullptr, FILE_CURRENT);
        if (dwPtr == INVALID_SET_FILE_POINTER)
        {
            fRes = false;
        }
    }
    else
    {
        connected = m_socket->ReadExact(reinterpret_cast<char*>(m_pBuff), static_cast<VCard>(nLen)) == VTrue;
        if (connected)
        {
            if (nSize == 0)
            {
                fCompressed = false;
            }
            unsigned int nRawBytes = sz_rfbBlockSize;

            if (fCompressed)
            {
                // Decompress incoming data
                int nRet = uncompress(reinterpret_cast<unsigned char*>(m_pCompBuff),   // Dest
                                      reinterpret_cast<unsigned long*>(&nRawBytes),    // Dest len
                                      reinterpret_cast<const unsigned char*>(m_pBuff), // Src
                                      static_cast<uLong>(nLen)                           // Src len
                );
                if (nRet != 0)
                {
                    m_fFileDownloadError = true;
                    FinishFileReception();
                    return connected;
                }
            }

            fRes = ::WriteFile(m_hDestFile, fCompressed ? m_pCompBuff : m_pBuff, fCompressed ? nRawBytes : static_cast<DWORD>(nLen), &m_dwNbBytesWritten,
                             nullptr);
        }
        else
        {
            m_fFileDownloadError = true;
            // FlushFileBuffers(m_client->m_hDestFile);
            FinishFileReception();
        }
    }

    if (!fRes)
    {
        // TODO : send an explicit error msg to the client...
        m_fFileDownloadError = true;
        FinishFileReception();
        return connected;
    }

    m_dwTotalNbBytesWritten += (fAlreadyHere ? static_cast<DWORD>(nLen) : m_dwNbBytesWritten);
    ++m_dwNbReceivedPackets;

    return connected;
}

void vncClient::FinishFileReception()
{
    vnclog.Print(0, _T("=== SRV: FinishFileReception CALLED ===\n"));
    vnclog.Print(0, _T("  m_fFileDownloadRunning: %d\n"), m_fFileDownloadRunning);
    if (!m_fFileDownloadRunning)
    {
        vnclog.Print(0, _T("  EARLY RETURN: m_fFileDownloadRunning is FALSE\n"));
        return;
    }

    m_fFileDownloadRunning = false;
    m_socket->SetRecvTimeout(settings->getIdleTimeout() * 1000);

    // sf@2004 - Delta transfer
    ::SetEndOfFile(m_hDestFile);

    // if error?
    ::FlushFileBuffers(m_hDestFile);

    // Set the DestFile Time Stamp
    if (std::strlen(m_szFileTime.data()))
    {
        FILETIME DestFileTime;
        SYSTEMTIME FileTime{};
        FileTime.wMonth = static_cast<WORD>(atoi(m_szFileTime.data()));
        FileTime.wDay = static_cast<WORD>(atoi(m_szFileTime.data() + 3));
        FileTime.wYear = static_cast<WORD>(atoi(m_szFileTime.data() + 6));
        FileTime.wHour = static_cast<WORD>(atoi(m_szFileTime.data() + 11));
        FileTime.wMinute = static_cast<WORD>(atoi(m_szFileTime.data() + 14));
        FileTime.wMilliseconds = 0;
        FileTime.wSecond = 0;
        ::SystemTimeToFileTime(&FileTime, &DestFileTime);
        // ToDo: hook error
        ::SetFileTime(m_hDestFile, &DestFileTime, &DestFileTime, &DestFileTime);
    }

    // CleanUp
    helper::close_handle(m_hDestFile);

    // sf@2004 - Delta Transfer : we can keep the existing file data :)
    // if (m_fFileDownloadError) DeleteFile(m_szFullDestName);

    // sf@2003 - Directory Transfer trick
    // If the file is an Ultra Directory Zip we unzip it here and we delete the
    // received file
    // Todo: make a better free space check above in this particular case. The free space must be at least
    // 3 times the size of the directory zip file (this zip file is ~50% of the real directory size)
    bool bWasDir = UnzipPossibleDirectory(m_szFullDestName.data());
    /*
    if (!m_fFileDownloadError && !strncmp(strrchr(m_szFullDestName, '\\') + 1, rfbZipDirectoryPrefix,
    strlen(rfbZipDirectoryPrefix)))
    {
        char szPath[MAX_PATH + MAX_PATH];
        char szDirName[MAX_PATH]; // Todo: improve this (size)
        strcpy_s(szPath, m_szFullDestName);
        // Todo: improve all this (p, p2, p3 NULL test or use a standard substring extraction function)
        char *p = strrchr(szPath, '\\') + 1;
        char *p2 = strchr(p, '-') + 1; // rfbZipDirectoryPrefix MUST have a "-" at the end...
        strcpy_s(szDirName, p2);
        char *p3 = strrchr(szDirName, '.');
        *p3 = '\0';
        if (p != NULL) *p = '\0';
        strcat_s(szPath, szDirName);

        // Create the Directory
        // BOOL fRet = CreateDirectory(szPath, NULL);
        m_pZipUnZip->UnZipDirectory(szPath, m_szFullDestName);
        DeleteFile(m_szFullDestName);
    }
    */

    if (m_fFileDownloadError && m_fUserAbortedFileTransfer)
    {
        SplitTransferredFileNameAndDate(m_szFullDestName.data(), nullptr);
        {
            std::array<WCHAR, MAX_PATH * 4> _dp;
            MakeLongPath(m_szFullDestName.data(), _dp.data(), static_cast<int>(_dp.size()));
            ::DeleteFileW(_dp.data());
        }
        FTDownloadCancelledHook();
    }
    else
    {
        std::string realName = get_real_filename(m_szFullDestName.data());
        if (!m_fFileDownloadError && !bWasDir)
        {
            m_fFileDownloadError = !replaceFile(m_szFullDestName.data(), realName.c_str());
        }
        if (m_fFileDownloadError)
        {
            FTDownloadFailureHook();
        }
        else
        {
            FTDownloadCompleteHook();
        }
    }
    //delete [] m_szFullDestName;

    if (m_pCompBuff != nullptr)
    {
        delete[] m_pCompBuff;
        m_pCompBuff = nullptr;
    }
    if (m_pBuff != nullptr)
    {
        delete[] m_pBuff;
        m_pBuff = nullptr;
    }
    if (m_lpCSBuffer)
    {
        delete[] m_lpCSBuffer;
        m_lpCSBuffer = nullptr;
    }

    return;
}

bool vncClient::SendFileChunk()
{
    bool connected{ true };
    static DWORD lastYieldTime{ 0U };
    // Legacy viewers (pre FT_PROTO_VERSION_4) only allocate 8192+1024 bytes for their chunk buffer
    const DWORD chunkSize = m_fClientSupportsUnicode ? sz_rfbBlockSize : 8192;
    do
    {
        connected = true;
        bool needYield{ false };

        // Scope block to limit mutex lock duration - release before sleep
        {
            omni_mutex_lock l(GetUpdateLock(), 101);

            if (!m_fFileUploadRunning)
            {
                return connected;
            }
            if (m_fEof || m_fFileUploadError)
            {
                FinishFileSending();
                return connected;
            }

            int nRes = ::ReadFile(m_hSrcFile, m_pBuff, chunkSize, &m_dwNbBytesRead, nullptr);
            if (!nRes && (m_dwNbBytesRead != 0))
            {
                m_fFileUploadError = true;
            }

            if (nRes && (m_dwNbBytesRead == 0))
            {
                m_fEof = true;
            }
            else
            {
                // sf@2004 - Delta Transfer
                bool fAlreadyThere{ false };
                unsigned long nCS{ 0U };
                // if Checksums are available for this file
                if (m_lpCSBuffer != nullptr)
                {
                    if (m_nCSOffset < m_nCSBufferSize)
                    {
                        std::memcpy(&nCS, &m_lpCSBuffer[m_nCSOffset], 4);
                        if (nCS != 0)
                        {
                            m_nCSOffset += 4;
                            unsigned long cs = adler32(0L, Z_NULL, 0);
                            cs = adler32(cs, reinterpret_cast<unsigned char*>(m_pBuff), static_cast<int>(m_dwNbBytesRead));
                            if (cs == nCS)
                            {
                                fAlreadyThere = true;
                            }
                        }
                    }
                }

                if (fAlreadyThere)
                {
                    // Send the FileTransferMsg with empty rfbFilePacket
                    rfbFileTransferMsg ft{};
                    ft.type = rfbFileTransfer;
                    ft.contentType = rfbFilePacket;
                    ft.size = Swap32IfLE(2); // Means "Empty packet"// Swap32IfLE(nCS);
                    ft.length = Swap32IfLE(m_dwNbBytesRead);
                    m_socket->SendExactQueue(reinterpret_cast<char*>(&ft), sz_rfbFileTransferMsg, rfbFileTransfer);
                }
                else
                {
                    // Compress the data
                    // (Compressed data can be longer if it was already compressed)
                    unsigned int nMaxCompSize = chunkSize + 1024; // TODO: Improve this...
                    bool fCompressed{ false };
                    if (m_fCompressionEnabled)
                    {
                        int nRetC = compress(reinterpret_cast<unsigned char*>((m_pCompBuff)), reinterpret_cast<unsigned long*>(&nMaxCompSize), reinterpret_cast<unsigned char*>(m_pBuff),
                                             m_dwNbBytesRead);

                        if (nRetC != 0)
                        {
                            vnclog.Print(LL_INTINFO, VNCLOG("Compress returned error in File Send :%d\n"), nRetC);
                            // Todo: send data uncompressed instead
                            m_fFileUploadError = true;
                            FinishFileSending();
                            return connected;
                        }
                        fCompressed = true;
                    }

                    // Test if we have to deal with already compressed data
                    if (nMaxCompSize > m_dwNbBytesRead)
                    {
                        fCompressed = false;
                    }
                    // m_fCompressionEnabled = false;

                    rfbFileTransferMsg ft{};

                    ft.type = rfbFileTransfer;
                    ft.contentType = rfbFilePacket;
                    ft.size = fCompressed ? Swap32IfLE(1) : Swap32IfLE(0);
                    ft.length = fCompressed ? Swap32IfLE(nMaxCompSize) : Swap32IfLE(m_dwNbBytesRead);

                    //adzm 2010-09 - minimize packets. SendExact flushes the queue.
                    connected = VFalse != m_socket->SendExactQueue(reinterpret_cast<char*>(&ft), sz_rfbFileTransferMsg, rfbFileTransfer);
                    if (connected)
                    {
                        if (fCompressed)
                        {
                            connected = VFalse != m_socket->SendExact(reinterpret_cast<char*>(m_pCompBuff), nMaxCompSize);
                        }
                        else
                        {
                            connected = VFalse != m_socket->SendExact(reinterpret_cast<char*>(m_pBuff), m_dwNbBytesRead);
                        }
                    }
                }

                m_dwTotalNbBytesRead += m_dwNbBytesRead;
                // TODO : test on nb of bytes written
            }

            // Check if we need to yield (but do it outside the lock)
            DWORD now = ::GetTickCount();
            if (now - lastYieldTime > 50)
            { // Yield every 50ms
                lastYieldTime = now;
                needYield = true;
            }
        } // mutex released here

        // Flow control: yield outside the lock to allow screen updates to proceed
        if (needYield)
        {
            ::Sleep(1); // Minimal yield to allow TCP buffers to drain and screen updates
        }

        // Let the client thread process pending viewer messages (e.g. rfbAbortFileTransfer);
        // it resumes sending once they are handled.
        if (connected && m_fFileUploadRunning && m_socket->ReadSelect(0))
        {
            break;
        }

        /*if (connected)
        {
            // Order next asynchronous packet sending
            PostToWinVNC( FileTransferSendPacketMessage, (WPARAM)this, (LPARAM)0);
        }*/
    } while (connected && m_fFileUploadRunning);

    return connected;
}

void vncClient::FinishFileSending()
{
    omni_mutex_lock l(GetUpdateLock(), 102);

    if (!m_fFileUploadRunning)
    {
        return;
    }

    // restore original timeout
    m_socket->SetSendTimeout(settings->getIdleTimeout() * 1000);

    m_fFileUploadRunning = false;

    rfbFileTransferMsg ft{};

    // File Copy OK
    if (!m_fFileUploadError /*nRet == 1*/)
    {
        ft.type = rfbFileTransfer;
        ft.contentType = rfbEndOfFile;
        m_socket->SendExact(reinterpret_cast<char*>(&ft), sz_rfbFileTransferMsg, rfbFileTransfer);
    }
    else // Error in file copy
    {
        // TODO : send an error msg to the client...
        ft.type = rfbFileTransfer;
        ft.contentType = rfbAbortFileTransfer;
        m_socket->SendExact(reinterpret_cast<char*>(&ft), sz_rfbFileTransferMsg, rfbFileTransfer);
    }

    helper::close_handle(m_hSrcFile);
    if (m_pBuff != nullptr)
    {
        delete[] m_pBuff;
        m_pBuff = nullptr;
    }
    if (m_pCompBuff != nullptr)
    {
        delete[] m_pCompBuff;
        m_pCompBuff = nullptr;
    }

    // sf@2003 - Directory Transfer trick
    // If the transfered file is a Directory zip, we delete it locally, whatever the result of the transfer
    if (!std::strncmp(strrchr(m_szSrcFileName.data(), '\\') + 1, rfbZipDirectoryPrefix, std::strlen(rfbZipDirectoryPrefix)))
    {
        char* p = strrchr(m_szSrcFileName.data(), ',');
        if (p != nullptr)
        {
            *p = '\0';
        } // Remove the time stamp we've added above from the file name
        ::DeleteFile(m_szSrcFileName.data());
    }
    //  jdp 8/8/2008
    if (m_fUserAbortedFileTransfer)
    {
        FTUploadCancelledHook();
    }
    else if (m_fFileUploadError)
    {
        FTUploadFailureHook();
    }
    else
    {
        FTUploadCompleteHook();
    }
}

bool vncClient::GetSpecialFolderPath(const int nId, char* const szPath)
{
    LPITEMIDLIST pidl;
    bool retval{ false };

    LPMALLOC pSHMalloc;

    if (FAILED(::SHGetMalloc(&pSHMalloc)))
    {
        return false;
    }

    if (::SHGetSpecialFolderLocation(nullptr, nId, &pidl) == NOERROR)
    {
        retval = ::SHGetPathFromIDList(pidl, szPath) ? true : false;

        pSHMalloc->Free(pidl);
    }

    pSHMalloc->Release();

    return retval;
}

//
// Zip a possible directory
//
int vncClient::ZipPossibleDirectory(const LPSTR szSrcFileName)
{
    char* p1 = strrchr(szSrcFileName, '\\') + 1;
    char* p2 = strrchr(szSrcFileName, rfbDirSuffix[0U]);
    if ((p1[0U] == rfbDirPrefix[0U]) && (p1[1U] == rfbDirPrefix[1U]) && // Check dir prefix
        (p2[1U] == rfbDirSuffix[1U]) && (p2 != nullptr) && (p1 < p2))   // Check dir suffix
    {
        // sf@2004 - Improving Directory Transfer: Avoids ReadOnly media problem
        // UltraVNC: Use Unicode paths throughout to support non-ASCII (Arabic etc.) dirnames.
        // szSrcFileName arrives as UTF-8: "C:\parent\[ dirname ]"
        std::array<WCHAR, MAX_PATH * 4> szWorkingDirW;
        ::GetTempPathW(MAX_PATH * 4, szWorkingDirW.data());
        if (m_fFTUserImpersonatedOk)
        {
            ::MultiByteToWideChar(CP_ACP, 0, m_szTempDir.data(), -1, szWorkingDirW.data(), static_cast<int>(szWorkingDirW.size()));
        }
        if (szWorkingDirW[0U] == L'\0')
        {
            if (::GetModuleFileNameW(nullptr, szWorkingDirW.data(), static_cast<DWORD>(szWorkingDirW.size())))
            {
                WCHAR* p = std::wcsrchr(szWorkingDirW.data(), L'\\');
                if (p == nullptr)
                {
                    return -1;
                }
                *(p + 1) = L'\0';
            }
            else
            {
                return -1;
            }
        }

        // Convert UTF-8 input path to Unicode
        std::array<WCHAR, MAX_PATH * 4> szSrcFileNameW;
        ::MultiByteToWideChar(CP_UTF8, 0, szSrcFileName, -1, szSrcFileNameW.data(), static_cast<int>(szSrcFileNameW.size()));

        // Parse: "C:\parent\[ dirname ]" -> szPathW="C:\parent\", szDirNameW="dirname"
        std::array<WCHAR, MAX_PATH * 4> szPathW;
        std::array<WCHAR, MAX_PATH * 4> szDirNameW;
        ::wcscpy_s(szPathW.data(), szPathW.size(), szSrcFileNameW.data());
        WCHAR* pw1 = std::wcsrchr(szPathW.data(), L'\\') + 1;
        ::wcscpy_s(szDirNameW.data(), szDirNameW.size(), pw1 + 2); // skip "[ "
        std::size_t nDirLen = std::wcslen(szDirNameW.data());
        if (nDirLen >= 2)
        {
            szDirNameW[nDirLen - 2] = L'\0';
        } // strip " ]"
        *pw1 = L'\0'; // szPathW = "C:\parent\"

        // The bracketed name is a single directory name, not a path: reject
        // "..", separators and drive specifiers so "C:\Shared\[ .. ]" cannot
        // escape the file-transfer root.
        std::size_t nDirNameLen = std::wcslen(szDirNameW.data());
        bool fBadDirName = (nDirNameLen == 0) || (std::wcscmp(szDirNameW.data(), L".") == 0) ||
                           (std::wcscmp(szDirNameW.data(), L"..") == 0) || (szDirNameW[nDirNameLen - 1] == L'.') ||
                           (szDirNameW[nDirNameLen - 1] == L' ') || (wcspbrk(szDirNameW.data(), L"\\/:*?\"<>|") != nullptr);
        if (fBadDirName)
        {
            return -1;
        }

        // Full source dir path and short path for ANSI zip library
        std::array<WCHAR, MAX_PATH * 4> szSrcDirW;
        ::_snwprintf_s(szSrcDirW.data(), szSrcDirW.size(), _TRUNCATE, L"%s%s", szPathW.data(), szDirNameW.data());
        m_OrigSourceDirectoryName = std::string(szSrcFileName); // keep for reference

        std::array<WCHAR, MAX_PATH * 4> szShortSrcW;
        if (!::GetShortPathNameW(szSrcDirW.data(), szShortSrcW.data(), static_cast<DWORD>(szShortSrcW.size())))
        {
            ::wcscpy_s(szShortSrcW.data(), szShortSrcW.size(), szSrcDirW.data());
        }

        // Build wildcard from short dir path (never call GetShortPathNameW on a wildcard)
        std::array<WCHAR, MAX_PATH * 4> szShortSrcWildW;
        ::_snwprintf_s(szShortSrcWildW.data(), szShortSrcWildW.size(), _TRUNCATE, L"%s\\*.*", szShortSrcW.data());

        // Hex-encode UTF-8 dirname for zip filename so viewer can recover original name
        std::array<char, MAX_PATH * 4> szDirNameUtf8;
        ::WideCharToMultiByte(CP_UTF8, 0, szDirNameW.data(), -1, szDirNameUtf8.data(), static_cast<int>(szDirNameUtf8.size()), nullptr, nullptr);
        std::array<char, MAX_PATH * 8> szHexName;
        szHexName[0U] = '\0';
        for (int _hi{ 0 }; (szDirNameUtf8[static_cast<unsigned long long>(_hi)] != '\0') && (_hi < MAX_PATH * 4 - 1); ++_hi)
        {
            std::array<char, 3U> _hbuf;
            ::sprintf_s(_hbuf.data(), _hbuf.size(), "%02X", static_cast<unsigned char>(szDirNameUtf8[static_cast<unsigned long long>(_hi)]));
            ::strcat_s(szHexName.data(), szHexName.size(), _hbuf.data());
        }
        if (std::strlen(szHexName.data()) > 200)
        {
            szHexName[200U] = '\0';
        }

        // Build zip output path (ASCII-safe temp dir + hex-encoded name)
        std::array<WCHAR, MAX_PATH * 4> szDirZipPathW;
        ::_snwprintf_s(szDirZipPathW.data(), szDirZipPathW.size(), _TRUNCATE, L"%s%hs%hs%s", szWorkingDirW.data(), rfbZipDirectoryPrefix, szHexName.data(), L".zip");

        // Use Unicode paths directly with minizip-ng
        bool fZip = m_pZipUnZip->ZipDirectory(szShortSrcW.data(), szShortSrcWildW.data(), szDirZipPathW.data(), true);
        if (!fZip)
        {
            return -1;
        }

        // Return zip path as UTF-8 in szSrcFileName (filetransferrequestPart2 opens it with CP_UTF8)
        ::WideCharToMultiByte(CP_UTF8, 0, szDirZipPathW.data(), -1, szSrcFileName, 324, nullptr, nullptr);
        return 1;
    }
    else
    {
        return 0;
    }
}

int vncClient::CheckAndZipDirectoryForChecksuming(const LPSTR szSrcFileName)
{
    if (!m_fFileDownloadError &&
        !std::strncmp(strrchr(szSrcFileName, '\\') + 1, rfbZipDirectoryPrefix, std::strlen(rfbZipDirectoryPrefix)))
    {
        std::array<char, MAX_PATH + MAX_PATH> szPath;
        std::array<char, MAX_PATH> szDirName;
        std::array<char, MAX_PATH * 2> szDirectoryName;
        ::strcpy_s(szPath.data(), szPath.size(), szSrcFileName);
        char* p = strrchr(szPath.data(), '\\') + 1;
        char* p2 = strchr(p, '-') + 1;
        ::strcpy_s(szDirName.data(), szDirName.size(), p2);
        char* p3 = strrchr(szDirName.data(), '.');
        *p3 = '\0';
        if (p != nullptr)
        {
            *p = '\0';
        }
        ::strcat_s(szPath.data(), szPath.size(), szDirName.data());

        ::CreateDirectory(szPath.data(), nullptr);
        if (::GetLastError() == ERROR_ALREADY_EXISTS)
        {
            ::strcpy_s(szDirectoryName.data(), szDirectoryName.size(), szPath.data());
            // p = strrchr(szPath, '\\') + 1;
            // if (p != NULL) *p = '\0'; else return -1;
            if (std::strlen(szDirectoryName.data()) > (MAX_PATH - 4))
            {
                return -1;
            }
            ::strcat_s(szDirectoryName.data(), szDirectoryName.size(), "\\*.*");

            // Convert to Unicode for minizip-ng
            std::array<WCHAR, MAX_PATH * 4> szPathW;
            std::array<WCHAR, MAX_PATH * 4> szDirectoryNameW;
            std::array<WCHAR, MAX_PATH * 4> szSrcFileNameW;
            ::MultiByteToWideChar(CP_ACP, 0, szPath.data(), -1, szPathW.data(), static_cast<int>(szPathW.size()));
            ::MultiByteToWideChar(CP_ACP, 0, szDirectoryName.data(), -1, szDirectoryNameW.data(), static_cast<int>(szDirectoryNameW.size()));
            ::MultiByteToWideChar(CP_ACP, 0, szSrcFileName, -1, szSrcFileNameW.data(), static_cast<int>(szSrcFileNameW.size()));

            bool fZip = m_pZipUnZip->ZipDirectory(szPathW.data(), szDirectoryNameW.data(), szSrcFileNameW.data(), true);
            if (!fZip)
            {
                return -1;
            }
        }
    }
    return 0;
}

//
// Unzip possible directory
// Todo: handle unzip error correctly...
//
bool vncClient::UnzipPossibleDirectory(const LPSTR szFileName)
{

    if (!m_fFileDownloadError &&
        !std::strncmp(strrchr(szFileName, '\\') + 1, rfbZipDirectoryPrefix, std::strlen(rfbZipDirectoryPrefix)))
    {
        std::array<char, MAX_PATH * 3> szPath;
        std::array<char, MAX_PATH * 3> szDirName;
        ::strcpy_s(szPath.data(), szPath.size(), szFileName);
        char* p = strrchr(szPath.data(), '\\') + 1;
        char* p2 = strchr(p, '-') + 1; // rfbZipDirectoryPrefix MUST have a "-" at the end...
        ::strcpy_s(szDirName.data(), szDirName.size(), p2);
        char* p3 = strrchr(szDirName.data(), '.');
        *p3 = '\0';
        if (p != nullptr)
        {
            *p = '\0';
        }

        {
            std::array<WCHAR, MAX_PATH * 4> _dbg2;
            ::MultiByteToWideChar(CP_ACP, 0, szDirName.data(), -1, _dbg2.data(), static_cast<int>(_dbg2.size()));
            vnclog.Print(0, _T("  Hex dirname: %s\n"), _dbg2.data());
        }

        // szDirName now contains either a hex-encoded UTF-8 dirname (new format)
        // or an 8.3 short dirname (old format). Detect by checking if all chars are hex digits.
        bool bIsHexEncoded = ((std::strlen(szDirName.data()) > 0) && (std::strlen(szDirName.data()) % 2 == 0));
        for (int _ci{ 0 }; bIsHexEncoded && szDirName[static_cast<unsigned long long>(_ci)]; ++_ci)
        {
            if (!std::isxdigit(static_cast<unsigned char>(szDirName[static_cast<unsigned long long>(_ci)])))
            {
                bIsHexEncoded = false;
            }
        }

        vnclog.Print(0, _T("  bIsHexEncoded: %d\n"), bIsHexEncoded);

        // Decode hex -> UTF-8 -> Unicode dirname for the final folder name
        std::array<WCHAR, MAX_PATH * 4> szFinalDirNameW{};
        if (bIsHexEncoded)
        {
            std::array<char, MAX_PATH * 4> szUtf8;
            int nBytes{ 0 };
            for (int _di{ 0 }; szDirName[static_cast<unsigned long long>(_di)] && szDirName[static_cast<unsigned long long>(_di + 1)]; _di += 2, ++nBytes)
            {
                char _hx[3U] = { szDirName[static_cast<unsigned long long>(_di)], szDirName[static_cast<unsigned long long>(_di + 1)], '\0' };
                szUtf8[static_cast<unsigned long long>(nBytes)] = static_cast<char>(std::strtol(_hx, nullptr, 16));
            }
            szUtf8[static_cast<unsigned long long>(nBytes)] = '\0';
            ::MultiByteToWideChar(CP_UTF8, 0, szUtf8.data(), -1, szFinalDirNameW.data(), static_cast<int>(szFinalDirNameW.size()));
            vnclog.Print(0, _T("  Decoded dirname: %s\n"), szFinalDirNameW.data());
        }

        // Security: the final folder name comes from the client. It must be a
        // single plain folder name - reject "..", path separators, drive
        // specifiers and trailing dots/spaces before extracting anything.
        UINT cpName = m_fClientSupportsUnicode ? CP_UTF8 : CP_ACP;
        std::array<WCHAR, MAX_PATH * 4> szDirNameW2;
        ::MultiByteToWideChar(cpName, 0, szDirName.data(), -1, szDirNameW2.data(), static_cast<int>(szDirNameW2.size()));
        const WCHAR* szEffectiveNameW = (std::wcslen(szFinalDirNameW.data()) > 0) ? szFinalDirNameW.data() : szDirNameW2.data();
        if (!helper::IsSafeFileNameW(szEffectiveNameW))
        {
            vnclog.Print(0, _T("  REJECTED unsafe dir name: %s\n"), szEffectiveNameW);
            m_fFileDownloadError = true;
            std::array<WCHAR, MAX_PATH * 4> szZipDelW;
            MakeLongPath(szFileName, szZipDelW.data(), static_cast<int>(szZipDelW.size()));
            ::DeleteFileW(szZipDelW.data());
            return true;
        }

        // Use a short ASCII temp name for extraction, then rename to Unicode
        std::array<char, 64U> szExtractName;
        ::sprintf_s(szExtractName.data(), szExtractName.size(), "UVNCTMP%08X", ::GetTickCount());
        ::strcat_s(szPath.data(), MAX_PATH * 3, szExtractName.data());

        // Create the Directory - wrap in SEH for safety
        // Convert paths to Unicode for minizip-ng
        // Use CP_UTF8 for Unicode-capable clients, CP_ACP for old clients
        UINT cp = m_fClientSupportsUnicode ? CP_UTF8 : CP_ACP;
        std::array<WCHAR, MAX_PATH * 4> szPathW;
        std::array<WCHAR, MAX_PATH * 4> szFileNameW;
        ::MultiByteToWideChar(cp, 0, szPath.data(), -1, szPathW.data(), static_cast<int>(szPathW.size()));
        ::MultiByteToWideChar(cp, 0, szFileName, -1, szFileNameW.data(), static_cast<int>(szFileNameW.size()));

        bool bUnzipOk{ false };
        __try
        {
            bUnzipOk = m_pZipUnZip->UnZipDirectory(szPathW.data(), szFileNameW.data());
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            bUnzipOk = false;
        }
        {
            std::array<WCHAR, MAX_PATH * 4> _dp;
            MakeLongPath(szFileName, _dp.data(), static_cast<int>(_dp.size()));
            ::DeleteFileW(_dp.data());
        }



        if (bUnzipOk)
        {
            // Rename extracted folder to the original Unicode dirname (or 8.3 name if not hex-encoded)
            std::array<WCHAR, MAX_PATH * 4> szExtractPathW;
            std::array<WCHAR, MAX_PATH * 4> szFinalPathW;
            // Parent path (destination dir) from szFileName
            std::array<WCHAR, MAX_PATH * 4> szFileNameWInner;
            ::MultiByteToWideChar(cp, 0, szFileName, -1, szFileNameWInner.data(), static_cast<int>(szFileNameWInner.size()));
            ::wcscpy_s(szExtractPathW.data(), szExtractPathW.size(), szFileNameWInner.data());
            WCHAR* pParent = std::wcsrchr(szExtractPathW.data(), L'\\');
            if (pParent)
            {
                *(pParent + 1) = L'\0';
            }
            std::array<WCHAR, 64U> szExtractNameW;
            ::MultiByteToWideChar(cp, 0, szExtractName.data(), -1, szExtractNameW.data(), 64);
            ::wcscat_s(szExtractPathW.data(), szExtractPathW.size(), szExtractNameW.data());

            // Build final path = parent + Unicode dirname
            if (pParent)
            {
                std::array<WCHAR, MAX_PATH * 4> szParentW;
                ::wcscpy_s(szParentW.data(), szParentW.size(), szFileNameWInner.data());
                WCHAR* pp2 = std::wcsrchr(szParentW.data(), L'\\');
                if (pp2)
                {
                    *(pp2 + 1) = L'\0';
                }
                ::_snwprintf_s(szFinalPathW.data(), szFinalPathW.size(), _TRUNCATE, L"%s%s", szParentW.data(), szEffectiveNameW);

                // Security: the resolved destination must be a direct child
                // of the destination folder.
                BOOL bMoved = FALSE;
                if (helper::IsDirectChildPathW(szParentW.data(), szFinalPathW.data()))
                {
                    bMoved = ::MoveFileW(szExtractPathW.data(), szFinalPathW.data());
                    if (!bMoved)
                    {
                        // Destination folder already exists: merge extracted content into it
                        DWORD dwDstAttr = ::GetFileAttributesW(szFinalPathW.data());
                        if ((dwDstAttr != INVALID_FILE_ATTRIBUTES) && (dwDstAttr & FILE_ATTRIBUTE_DIRECTORY))
                        {
                            bMoved = helper::MoveDirContentsInto(szExtractPathW.data(), szFinalPathW.data()) ? TRUE : FALSE;
                        }
                    }
                }
                if (!bMoved)
                {
                    m_fFileDownloadError = true;
                }
            }
        }
        else
        {
            m_fFileDownloadError = true;
        }
        return true;
    }
    return false;
}

//
// GetFileSize() doesn't handle files > 4GBytes...
// GetFileSizeEx() doesn't exist under Win9x...
// So let's write our own function.
//
bool vncClient::MyGetFileSize(char* const szFilePath, ULARGE_INTEGER* const n2FileSize)
{
    WIN32_FIND_DATAW fdW;
    HANDLE ff;

    // Convert UTF-8 to Unicode for Chinese character support
    std::array<WCHAR, MAX_PATH + 64> szFilePathW;
    ::MultiByteToWideChar(CP_UTF8, 0, szFilePath, -1, szFilePathW.data(), static_cast<int>(szFilePathW.size()));

    DWORD errmode = ::SetErrorMode(SEM_FAILCRITICALERRORS); // No popup please !
    ff = ::FindFirstFileW(szFilePathW.data(), &fdW);
    ::SetErrorMode(errmode);

    if (ff == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    ::FindClose(ff);

    (*n2FileSize).LowPart = fdW.nFileSizeLow;
    (*n2FileSize).HighPart = fdW.nFileSizeHigh;
    (*n2FileSize).QuadPart = static_cast<ULONGLONG>(((static_cast<__int64>(fdW.nFileSizeHigh)) << 32) + fdW.nFileSizeLow);

    return true;
}

#ifndef SC_20
bool vncClient::DoFTUserImpersonation()
{
    vnclog.Print(LL_INTERR, VNCLOG("%%%%%%%%%%%%% vncClient::DoFTUserImpersonation - Call\n"));
    omni_mutex_lock l(GetUpdateLock(), 103);

    if (m_fFileDownloadRunning)
    {
        return true;
    }
    if (m_fFileUploadRunning)
    {
        return true;
    }
    if (m_fFTUserImpersonatedOk)
    {
        return true;
    }
    // if we're already impersonating the user and have a session open, do nothing.
    if (m_fFileSessionOpen && m_fFTUserImpersonatedOk)
    {
        return true;
    }

    vnclog.Print(LL_INTERR, VNCLOG("%%%%%%%%%%%%% vncClient::DoFTUserImpersonation - 1\n"));
    bool fUserOk{ true };

    if (processHelper::IsWSLocked())
    {
        m_fFTUserImpersonatedOk = false;
        vnclog.Print(LL_INTERR, VNCLOG("%%%%%%%%%%%%% vncClient::DoFTUserImpersonation - WSLocked\n"));
        return false;
    }

    std::array<char, UNLEN + 1> username{};
    processHelper::CurrentUser(reinterpret_cast<char*>(username.data()), sizeof(username));
    vnclog.Print(LL_INTERR, VNCLOG("%%%%%%%%%%%%% vncClient::DoFTUserImpersonation - currentUser = %s\n"), username.data());
    if (std::strcmp(username.data(), "") != 0)
    {
        m_hPToken = DesktopUsersToken::getInstance()->getDesktopUsersToken();

        if (!m_hPToken)
        {
            vnclog.Print(LL_INTERR, VNCLOG("%%%%%%%%%%%%% vncClient::DoFTUserImpersonation - OpenProcessToken Error\n"));
            fUserOk = false;
        }
        else
        {
            if (!::ImpersonateLoggedOnUser(m_hPToken))
            {
                vnclog.Print(LL_INTERR,
                             VNCLOG("%%%%%%%%%%%%% vncClient::DoFTUserImpersonation - ImpersonateLoggedOnUser Failed\n"));
                fUserOk = false;
            }
            vnclog.Print(LL_INTERR, VNCLOG("%%%%%%%%%%%%% vncClient::DoFTUserImpersonation - thread = %d\n"), ::GetCurrentThreadId());

            ::ExpandEnvironmentStringsForUser(m_hPToken, "%TEMP%", m_szTempDir.data(), static_cast<DWORD>(m_szTempDir.size()));
            ::strcat_s(m_szTempDir.data(), m_szTempDir.size(), "\\");
        }
        if (!settings->RunningFromExternalService())
        {
            fUserOk = true;
        }
    }
    else
    {
        fUserOk = false;
    }

    if (fUserOk)
    {
        m_lLastFTUserImpersonationTime = GetTimeFunction();
    }
    m_fFTUserImpersonatedOk = fUserOk;
    return fUserOk;
}

void vncClient::UndoFTUserImpersonation()
{
    //vnclog.Print(LL_INTERR, VNCLOG("%%%%%%%%%%%%% vncClient::UNDoFTUserImpersonation - Call\n"));
    //moved to after returns, Is this lock realy needed if no revert is done?
    //
    //omni_mutex_lock l(GetUpdateLock());

    if (!m_fFTUserImpersonatedOk)
    {
        return;
    }
    if (m_fFileDownloadRunning)
    {
        return;
    }
    if (m_fFileUploadRunning)
    {
        return;
    }
    if (m_fFileSessionOpen)
    {
        return;
    }

    vnclog.Print(LL_INTERR, VNCLOG("%%%%%%%%%%%%% vncClient::UNDoFTUserImpersonation - 1\n"));
    DWORD lTime = GetTimeFunction();
    if (lTime - m_lLastFTUserImpersonationTime < 10000)
    {
        return;
    }
    omni_mutex_lock l(GetUpdateLock(), 104);
    vnclog.Print(LL_INTERR, VNCLOG("%%%%%%%%%%%%% vncClient::UNDoFTUserImpersonation - Impersonationtoken exists\n"));
    ::RevertToSelf();
    m_fFTUserImpersonatedOk = false;
    m_hPToken = nullptr;
}
#endif // SC_20

// 10 April 2008 jdp paquette@atnetsend.net
// This can crash as we can not send middle in an update...

void vncClient::Record_SendServerStateUpdate(CARD32 state, CARD32 value)
{
    m_state = state;
    m_value = value;
    m_want_update_state = true;
#ifdef _DEBUG
    std::array<char, 256U> szText;
    ::sprintf_s(szText.data(), szText.size(), "Record_SendServerStateUpdate %i %i  \n", m_state, m_value);
    ::OutputDebugString(szText.data());
#endif
}

void vncClient::SendServerStateUpdate(CARD32 state, CARD32 value)
{
    if (m_wants_ServerStateUpdates && m_socket)
    {
        // send message to client
        rfbServerStateMsg rsmsg;
        std::memset(&rsmsg, 0, sizeof rsmsg);
        rsmsg.type = rfbServerState;
        rsmsg.state = Swap32IfLE(state);
        rsmsg.value = Swap32IfLE(value);

        m_socket->SendExact(reinterpret_cast<char*>(&rsmsg), sz_rfbServerStateMsg, rfbServerState);
    }
}

void vncClient::SendMonitorInfo()
{
    rfbMonitorMsg mm;
    std::memset(&mm, 0, sizeof mm);
    mm.type = rfbMonitorInfo;
    mm.nbr = static_cast<CARD8>(m_encodemgr.m_buffer->m_desktop->nr_monitors);
    m_socket->SendExact(reinterpret_cast<char*>(&mm), sz_rfbMonitorMsg, rfbMonitorInfo);
}

void vncClient::SendKeepAlive(const bool bForce)
{
    if (m_wants_KeepAlive && m_socket)
    {
        //adzm 2010-08-01
        DWORD nInterval = static_cast<DWORD>(settings->getkeepAliveInterval()) * 1000;
        DWORD nTicksSinceLastSent = ::GetTickCount() - m_socket->GetLastSentTick();

        if ((!bForce && (nTicksSinceLastSent < nInterval)) || (nInterval == 0))
        {
            return;
        }

        rfbKeepAliveMsg kp;
        std::memset(&kp, 0, sizeof kp);
        kp.type = rfbKeepAlive;
        omni_mutex_lock l(GetUpdateLock(), 255);
        m_socket->SendExact(reinterpret_cast<char*>(&kp), sz_rfbKeepAliveMsg, rfbKeepAlive);
    }
}

void vncClient::SendFTProtocolMsg()
{
    rfbFileTransferMsg ft;
    std::memset(&ft, 0, sizeof ft);
    ft.type = rfbFileTransfer;
    ft.contentType = rfbFileTransferProtocolVersion;
    ft.contentParam = FT_PROTO_VERSION_4;
    ft.size = Swap32IfLE(sz_rfbBlockSize);
    m_socket->SendExact(reinterpret_cast<char*>(&ft), sz_rfbFileTransferMsg, rfbFileTransfer);
}

// adzm - 2010-07 - Extended clipboard
void vncClient::NotifyExtendedClipboardSupport()
{
    ExtendedClipboardDataMessage extendedDataMessage;
    m_clipboard.settings.PrepareCapsPacket(extendedDataMessage);

    rfbServerCutTextMsg msg;
    std::memset(&msg, 0, sizeof(rfbServerCutTextMsg));
    msg.type = rfbServerCutText;
    msg.length = static_cast<unsigned int>(Swap32IfLE(-extendedDataMessage.GetDataLength()));

    //adzm 2010-09 - minimize packets. SendExact flushes the queue.
    m_socket->SendExactQueue(reinterpret_cast<char*>(&msg), sz_rfbServerCutTextMsg, rfbServerCutText);
    m_socket->SendExact(reinterpret_cast<char*>(const_cast<BYTE *>((extendedDataMessage.GetData()))), static_cast<VCard>(extendedDataMessage.GetDataLength()));
}

// adzm 2010-09 - Notify streaming DSM plugin support
void vncClient::NotifyPluginStreamingSupport()
{
    rfbNotifyPluginStreamingMsg msg;
    std::memset(&msg, 0, sizeof(rfbNotifyPluginStreamingMsg));
    msg.type = rfbNotifyPluginStreaming;

    //adzm 2010-09 - minimize packets. SendExact flushes the queue.
    m_socket->SendExact(reinterpret_cast<char*>(&msg), sz_rfbNotifyPluginStreamingMsg, rfbNotifyPluginStreaming);
    m_socket->SetPluginStreamingOut();
}

DWORD WINAPI CompressFolder(const LPVOID lpParam)
{
    vncClient* client = reinterpret_cast<vncClient*>(lpParam);
    if (client->m_hPToken)
    {
        ::ImpersonateLoggedOnUser(client->m_hPToken);
    } //need to set this thread's impersonation or can find mapped network or share files
    int nDirZipRet = client->ZipPossibleDirectory(client->m_szSrcFileName.data());
    if (client->m_socket)
    {
        return static_cast<DWORD>(client->filetransferrequestPart2(nDirZipRet));
    }
    return 0;
}

int vncClient::filetransferrequestPart1(const rfbClientToServerMsg msg, const bool fUserOk)
{
    if (ThreadHandleCompressFolder != nullptr)
    {
        return 0;
    }
    bool fRejected{ false };
    {
        omni_mutex_lock ll(GetUpdateLock(), 90);
        m_fCompressionEnabled = (Swap32IfLE(msg.ft.size) == 1);
        const UINT length = Swap32IfLE(msg.ft.length);
        std::memset(m_szSrcFileName.data(), 0, sizeof(m_szSrcFileName));
        if (length > sizeof(m_szSrcFileName) - 2)
        {
            return 0;
        }
        // Read in the Name of the file to create
        if (!m_socket->ReadExact(m_szSrcFileName.data(), length))
        {
            return 0;
        }
        if (!settings->getEnableFileTransfer() || !fUserOk)
        {
            return 0;
        }
        // Enforce the configured file-transfer root on downloads as well.
        // Directory requests arrive as <path>\[ <name> ]; canonicalizing the
        // full request still pins the target inside the root.
        std::string canonicalPath;
        if (ValidateFileTransferPath(m_szSrcFileName.data(), settings->getFileTransferRoot(), canonicalPath))
        {
            ::strcpy_s(m_szSrcFileName.data(), m_szSrcFileName.size(), canonicalPath.c_str());
        }
        else
        {
            fRejected = true;
        }
    }
    if (fRejected)
    {
        return filetransferrequestPart2(-1);
    }
    DWORD dwTId;
    ThreadHandleCompressFolder = ::CreateThread(nullptr, 0, CompressFolder, this, 0, &dwTId);
    return 1;
}

int vncClient::filetransferrequestPart2(const int nDirZipRet)
{
    omni_mutex_lock ll(GetUpdateLock(), 90);
    if (nDirZipRet == -1)
    {
        //vnclog.Print(LL_INTINFO, VNCLOG("*** File Transfer: Failed to zip requested dir. Abort!\n"));

        //	[v1.0.2-jp1 fix] Empty directory receive problem
        rfbFileTransferMsg ft{};
        ft.type = rfbFileTransfer;
        ft.contentType = rfbFileHeader;
        ft.size = Swap32IfLE(0xffffffffu); // File Size in bytes, 0xFFFFFFFF (-1) means error
        ft.length = Swap32IfLE(std::strlen(m_szSrcFileName.data()));
        //adzm 2010-09 - minimize packets. SendExact flushes the queue.
        m_socket->SendExactQueue(reinterpret_cast<char*>(&ft), sz_rfbFileTransferMsg, rfbFileTransfer);
        m_socket->SendExactQueue(reinterpret_cast<char*>(m_szSrcFileName.data()), static_cast<const VCard>(std::strlen(m_szSrcFileName.data())));
        // 2 May 2008 jdp send the highpart too, else the client will hang
        // sf@2004 - Improving huge file size handling
        // TODO: what if we're speaking the old protocol, how can we tell?
        CARD32 sizeH = Swap32IfLE(0xffffffffu);
        m_socket->SendExact(reinterpret_cast<char*>(&sizeH), sizeof(CARD32));

        m_fFileUploadError = true;
        m_fFileUploadRunning = false;
        FTUploadFailureHook();
        if (ThreadHandleCompressFolder)
        {
            ::CloseHandle(ThreadHandleCompressFolder);
        }
        ThreadHandleCompressFolder = nullptr;
        return 0;
    }

    vnclog.Print(LL_INTERR, VNCLOG("%%%%%%%%%%%%% vncClient::filetransferrequestPart2 - thread = %d\n"), ::GetCurrentThreadId());

    // Open source file
    // Convert UTF-8 filename to Unicode for Chinese character support
    std::array<WCHAR, MAX_PATH * 4> szSrcFileNameW;
    ::MultiByteToWideChar(CP_UTF8, 0, m_szSrcFileName.data(), -1, szSrcFileNameW.data(), static_cast<int>(szSrcFileNameW.size()));
    m_hSrcFile = ::CreateFileW(szSrcFileNameW.data(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                             FILE_FLAG_SEQUENTIAL_SCAN, nullptr);

    // DWORD dwSrcSize = (DWORD)0;
    ULARGE_INTEGER n2SrcSize{};
    if (m_hSrcFile == INVALID_HANDLE_VALUE)
    {
        // dwSrcSize = 0xFFFFFFFF;
        n2SrcSize.LowPart = 0xFFFFFFFF;
        n2SrcSize.HighPart = 0xFFFFFFFF;
    }
    else
    {
        // Source file size
        bool bSize = MyGetFileSize(m_szSrcFileName.data(), &n2SrcSize);
        // dwSrcSize = GetFileSize(m_hSrcFile, NULL);
        // if (dwSrcSize == 0xFFFFFFFF)
        if (!bSize)
        {
            helper::close_handle(m_hSrcFile);
            n2SrcSize.LowPart = 0xFFFFFFFF;
            n2SrcSize.HighPart = 0xFFFFFFFF;
        }
        else
        {
            // Add the File Time Stamp to the filename
            FILETIME SrcFileModifTime;
            BOOL fRes = ::GetFileTime(m_hSrcFile, nullptr, nullptr, &SrcFileModifTime);
            if (fRes)
            {
                std::array<char, 18U> szSrcFileTime;
                // sf@2003 - Convert file time to local time
                // We've made the choice off displaying all the files
                // off client AND server sides converted in clients local
                // time only. So we don't convert server's files times.
                /*
                FILETIME LocalFileTime;
                FileTimeToLocalFileTime(&SrcFileModifTime, &LocalFileTime);
                */
                SYSTEMTIME FileTime;
                ::FileTimeToSystemTime(&SrcFileModifTime /*&LocalFileTime*/, &FileTime);
                std::snprintf(szSrcFileTime.data(), szSrcFileTime.size(), "%2.2d/%2.2d/%4.4d %2.2d:%2.2d", FileTime.wMonth, FileTime.wDay, FileTime.wYear,
                         FileTime.wHour, FileTime.wMinute);
                ::strcat_s(m_szSrcFileName.data(), m_szSrcFileName.size(), ",");
                ::strcat_s(m_szSrcFileName.data(), m_szSrcFileName.size(), szSrcFileTime.data());
            }
        }
    }

    // sf@2004 - Delta Transfer
    if (m_lpCSBuffer != nullptr)
    {
        delete[] m_lpCSBuffer;
        m_lpCSBuffer = nullptr;
    }
    m_nCSOffset = 0;
    m_nCSBufferSize = 0;

    // Send the FileTransferMsg with rfbFileHeader
    rfbFileTransferMsg ft{ 0 };

    ft.type = rfbFileTransfer;
    ft.contentType = rfbFileHeader;
    ft.size = Swap32IfLE(n2SrcSize.LowPart); // File Size in bytes, 0xFFFFFFFF (-1) means error
    ft.length = Swap32IfLE(std::strlen(m_szSrcFileName.data()));
    //adzm 2010-09 - minimize packets. SendExact flushes the queue.
    m_socket->SendExactQueue(reinterpret_cast<char*>(&ft), sz_rfbFileTransferMsg, rfbFileTransfer);
    m_socket->SendExactQueue(reinterpret_cast<char*>(m_szSrcFileName.data()), static_cast<const VCard>(std::strlen(m_szSrcFileName.data())));

    // sf@2004 - Improving huge file size handling
    CARD32 sizeH = Swap32IfLE(n2SrcSize.HighPart);
    m_socket->SendExact(reinterpret_cast<char*>(&sizeH), sizeof(CARD32));

    // delete [] szSrcFileName;
    if ((n2SrcSize.LowPart == 0xFFFFFFFF) && (n2SrcSize.HighPart == 0xFFFFFFFF))
    {
        //vnclog.Print(LL_INTINFO, VNCLOG("*** File Transfer: Wrong Src File size. Abort!\n"));
        FTUploadFailureHook();
        if (ThreadHandleCompressFolder)
        {
            ::CloseHandle(ThreadHandleCompressFolder);
        }
        ThreadHandleCompressFolder = nullptr;
        return 0;
    }

    FTUploadStartHook();
    if (ThreadHandleCompressFolder)
    {
        ::CloseHandle(ThreadHandleCompressFolder);
    }
    ThreadHandleCompressFolder = nullptr;
    return 0;
}

extern HINSTANCE hAppInstance;
void vncClient::SetHasMouse(const bool has_mouseIn)
{
    this->has_mouse = has_mouseIn;
    if (has_mouseIn)
    {
        if (simulateCursor)
        {
            delete simulateCursor;
        }
        simulateCursor = nullptr;
    }
    else
    {
        simulateCursor = new SimulateCursor(hAppInstance);
    }
}
