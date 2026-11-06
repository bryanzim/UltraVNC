// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
//


// FileTransfer.cpp: implementation of the File Transfer class.

#define DEBUG_FT 1

// sf@2002 - sf@2003 - sf@2004 - File Transfer
// This class handles all the File Transfer messages, events and procs, as well as the
// DialogBox which allows the user to browse Client ans Server disks-directories,
// and select some files to transfer between Client and Server.
//
// The GUI is very basic because I don't want to include MFC Classes in UltraVNC...
// I use only Windows SDK.
//
//
// The GUI is now quite bearable, but following modifs could be done one day or another
// - Add more columns to FileLists (File type, Attributes...)
// - Total progress should be based on total files' size instead of total number of files
// - Make the History persistent (file) so it's not lost each time the File Transfer Win is closed
// - Clean-up the code (duplicated parts, arrays and strings dimensions checks...)
// - Display the total files size in the currently displayed directory
// - Remember the current directories - Partially done: the File Transfer window can be minimized...


#include "stdhdrs.h"

#include "FileTransfer.h"

#include "Exception.h"
#include "ViewerBounds.h"
#include "vncviewer.h"

#include <CommCtrl.h>
#include <ShlObj.h>

#include <array>
#include <cstddef>
#include <cstring>

#ifdef _VCPKG
#include <zlib.h>
#include <zstd.h>

#else
#include "../zlib/zlib.h"
#include "../zstd/lib/zstd.h"

#endif
#include "common/win32_helpers.h"
#include "Log.h"
#include "UltraVNCHelperFunctions.h"

#include <Shlwapi.h>

#include <algorithm>
#include <limits>
#include <string>
#include <vector>

using namespace helper;
extern HINSTANCE m_hInstResDLL;

#pragma comment(lib, "Shlwapi.lib")

// [v1.0.2-jp1 fix] yak!'s File transfer patch
// Simply forward strchr() and strrchr() to _mbschr() and _mbsrchr() to avoid 0x5c problem, respectively.
// Probably, it is better to write forward functions internally.
#include <mbstring.h>

#include <cctype>
#include <cstdlib>
#include <cwchar>

#define strchr(a, b) reinterpret_cast<char*>(_mbschr(reinterpret_cast<unsigned char*>(a), b))
#define strrchr(a, b) reinterpret_cast<char*>(_mbsrchr(reinterpret_cast<unsigned char*>(a), b))

// These strings contain all the translated File Transfer messages
extern std::array<wchar_t, 64U> sz_H1;
extern std::array<wchar_t, 64U> sz_H2;
extern std::array<wchar_t, 128U> sz_H3;
extern std::array<wchar_t, 64U> sz_H4;
extern std::array<wchar_t, 64U> sz_H5;
extern std::array<wchar_t, 64U> sz_H6;
extern std::array<wchar_t, 64U> sz_H7;
extern std::array<wchar_t, 64U> sz_H8;
extern std::array<wchar_t, 64U> sz_H9;
extern std::array<wchar_t, 64U> sz_H10;
extern std::array<wchar_t, 64U> sz_H11;
extern std::array<wchar_t, 64U> sz_H12;
extern std::array<wchar_t, 64U> sz_H13;
extern std::array<wchar_t, 64U> sz_H14;
extern std::array<wchar_t, 64U> sz_H15;
extern std::array<wchar_t, 64U> sz_H16;
extern std::array<wchar_t, 64U> sz_H17;
extern std::array<wchar_t, 64U> sz_H18;
extern std::array<wchar_t, 64U> sz_H19;
extern std::array<wchar_t, 64U> sz_H20;
extern std::array<wchar_t, 64U> sz_H21;
extern std::array<wchar_t, 64U> sz_H22;
extern std::array<wchar_t, 64U> sz_H23;
extern std::array<wchar_t, 64U> sz_H24;
extern std::array<wchar_t, 64U> sz_H25;
extern std::array<wchar_t, 64U> sz_H26;
extern std::array<wchar_t, 64U> sz_H27;
extern std::array<wchar_t, 64U> sz_H28;
extern std::array<wchar_t, 64U> sz_H29;
extern std::array<wchar_t, 64U> sz_H30;
extern std::array<wchar_t, 64U> sz_H31;
extern std::array<wchar_t, 64U> sz_H32;
extern std::array<wchar_t, 64U> sz_H33;
extern std::array<wchar_t, 64U> sz_H34;
extern std::array<wchar_t, 64U> sz_H35;
extern std::array<wchar_t, 64U> sz_H36;
extern std::array<wchar_t, 64U> sz_H37;
extern std::array<wchar_t, 128U> sz_H38;
extern std::array<wchar_t, 64U> sz_H39;
extern std::array<wchar_t, 64U> sz_H40;
extern std::array<wchar_t, 64U> sz_H41;
extern std::array<wchar_t, 64U> sz_H42;
extern std::array<wchar_t, 128U> sz_H43;
extern std::array<wchar_t, 64U> sz_H44;
extern std::array<wchar_t, 64U> sz_H45;
extern std::array<wchar_t, 128U> sz_H46;
extern std::array<wchar_t, 64U> sz_H47;
extern std::array<wchar_t, 64U> sz_H48;
extern std::array<wchar_t, 64U> sz_H49;
extern std::array<wchar_t, 64U> sz_H50;
extern std::array<wchar_t, 64U> sz_H51;
extern std::array<wchar_t, 64U> sz_H52;
extern std::array<wchar_t, 64U> sz_H53;
extern std::array<wchar_t, 64U> sz_H54;
extern std::array<wchar_t, 64U> sz_H55;
extern std::array<wchar_t, 64U> sz_H56;
extern std::array<wchar_t, 64U> sz_H57;

// Folder Transfer messages
extern std::array<wchar_t, 64U> sz_H58;
extern std::array<wchar_t, 64U> sz_H59;
extern std::array<wchar_t, 64U> sz_H60;
extern std::array<wchar_t, 64U> sz_H61;
extern std::array<wchar_t, 128U> sz_H62;
extern std::array<wchar_t, 64U> sz_H63;
extern std::array<wchar_t, 64U> sz_H64;
extern std::array<wchar_t, 64U> sz_H65;
extern std::array<wchar_t, 64U> sz_H66;
extern std::array<wchar_t, 64U> sz_H67;
extern std::array<wchar_t, 128U> sz_H68;
extern std::array<wchar_t, 64U> sz_H69;
extern std::array<wchar_t, 64U> sz_H70;
extern std::array<wchar_t, 64U> sz_H71;
extern std::array<wchar_t, 128U> sz_H72;
extern std::array<wchar_t, 64U> sz_H73;

// File/dir Rename messages
extern std::array<wchar_t, 64U> sz_M1;
extern std::array<wchar_t, 64U> sz_M2;
extern std::array<wchar_t, 64U> sz_M3;
extern std::array<wchar_t, 64U> sz_M4;
extern std::array<wchar_t, 64U> sz_M5;
extern std::array<wchar_t, 64U> sz_M6;
extern std::array<wchar_t, 64U> sz_M7;
extern std::array<wchar_t, 64U> sz_M8;

// 14 April 2008 jdp
extern std::array<wchar_t, 64U> sz_H94;
extern std::array<wchar_t, 64U> sz_H95;
extern std::array<wchar_t, 64U> sz_H96;
extern std::array<wchar_t, 64U> sz_H97;
extern std::array<wchar_t, 64U> sz_H98;
extern std::array<wchar_t, 64U> sz_H99;
extern std::array<wchar_t, 64U> sz_E1;
extern std::array<wchar_t, 64U> sz_E2;
extern std::array<wchar_t, 64U> sz_H100;
extern std::array<wchar_t, 64U> sz_H101;
extern std::array<wchar_t, 128U> sz_H102;
#define CB_SETHORIZONTALEXTENT 0x015e
typedef BOOL(WINAPI* PGETDISKFREESPACEEX)(LPCSTR, PULARGE_INTEGER, PULARGE_INTEGER, PULARGE_INTEGER);


#define FT_USE_MMTIMER

static FileTransfer* g_FileTransferSingleton = nullptr;

// Global headless-upload flag. Kept OUTSIDE the FileTransfer object because the
// per-instance members (m_fHeadlessUpload / m_szHeadlessRemotePathW) get zeroed
// by a memory corruption during the transfer. A file-scope global is immune to that.
// Must be volatile: DoUploadFile runs on the main thread, but SendFileChunk /
// FinishFileSending are called from the multimedia timer thread (timeSetEvent),
// so without volatile the timer thread can read a stale cached value.
static volatile bool g_fHeadlessUpload{ false };

HWND hFTWnd = nullptr;
static std::string make_temp_filename(const char* const szRemoteFileName)
{
    // don't add prefix for directory transfers.
    char* pFileName = strrchr(const_cast<char*>(szRemoteFileName), '\\');
    if (pFileName != nullptr)
    {
        ++pFileName;
    }
    else
    {
        pFileName = const_cast<char*>(szRemoteFileName);
    }
    if (std::strncmp(pFileName, rfbZipDirectoryPrefix, std::strlen(rfbZipDirectoryPrefix)) == 0)
    {
        return szRemoteFileName;
    }

    std::string tmpName(rfbPartialFilePrefix);
    tmpName += szRemoteFileName;
    return tmpName;
}

static std::string get_real_filename(const char* const destFileName)
{
    std::string name(destFileName);
    std::string::size_type pos;

    pos = name.find(rfbPartialFilePrefix);
    if (pos != std::string::npos)
    {
        name.erase(pos, sz_rfbPartialFilePrefix);
    }

    return name;
}
bool FileTransfer::DeleteFileOrDirectory(WCHAR* const srcpath)
{
    std::array<WCHAR, MAX_PATH + 2> path; // room for double null terminator required by SHFileOperationW
    std::memset(path.data(), 0, sizeof path);
    ::wcsncpy_s(path.data(), path.size(), srcpath, MAX_PATH);

    SHFILEOPSTRUCTW op;
    std::memset(&op, 0, sizeof(SHFILEOPSTRUCTW));
    op.hwnd = hWnd;
    op.wFunc = FO_DELETE;
    op.pFrom = path.data();
    op.fFlags = FOF_SILENT | FOF_NOCONFIRMATION | FOF_NOCONFIRMMKDIR | FOF_ALLOWUNDO;

    // MSDN says to not look at the error code, just treat 0 as SUCCESS, nonzero is failure.
    // Do not use GetLastError with the return values of this function.
    int result = ::SHFileOperationW(&op);
    if (result == 0)
    {
        return true;
    }

    // Fallback: SHFileOperationW can fail for reserved shell names or when Recycle Bin
    // is unavailable. Try direct deletion.
    DWORD attr = ::GetFileAttributesW(srcpath);
    if (attr == INVALID_FILE_ATTRIBUTES)
    {
        return false;
    }
    if (attr & FILE_ATTRIBUTE_DIRECTORY)
    {
        return ::RemoveDirectoryW(srcpath) != 0;
    }
    return ::DeleteFileW(srcpath) != 0;
}

//
//
//
FileTransfer::FileTransfer(VNCviewerApp* const l_pApp, ClientConnection* pCC)
{
    //	vnclog.Print(0, _T("FileTransfer\n"));
    m_pApp = l_pApp;
    m_pCC = pCC;
    m_fAbort = false;
    m_fUserAbortedFileTransfer = false;
    m_fAborted = false;
    m_FilesList.clear();
    m_nFilesToTransfer = 0;
    m_nFilesTransfered = 0;
    m_fFileCommandPending = false;
    m_fFileTransferRunning = false;
    m_fFileDownloadRunning = false;
    m_fDirectoryReceptionRunning = false;
    m_fVisible = true;
    m_fFTAllowed = false;
    m_timer = 0;
    m_pMiniZipNG = new CMiniZipNG();
    m_lpCSBuffer = nullptr;
    m_nCSOffset = 0;
    m_nCSBufferSize = 0;
    m_nDeleteCount = 0;
    std::memset(m_szDeleteButtonLabel.data(), 0, sizeof(m_szDeleteButtonLabel));
    std::memset(m_szNewFolderButtonLabel.data(), 0, sizeof(m_szNewFolderButtonLabel));
    std::memset(m_szRenameButtonLabel.data(), 0, sizeof(m_szRenameButtonLabel));
    std::memset(m_szRefreshButtonLabel.data(), 0, sizeof(m_szRefreshButtonLabel));
    m_ServerFTProtocolVersion = FT_PROTO_VERSION_2;
    m_fServerSupportsUnicode = false;
    m_nBlockSize = 8192;
    m_dwCurrentValue = 0;
    m_dwCurrentPercent = 0;
    m_fSendFileChunk = false;
    m_hSrcFile = INVALID_HANDLE_VALUE;
    m_hDestFile = INVALID_HANDLE_VALUE;
    m_fFileUploadRunning = false;
    m_fFileDownloadRunning = false;
    hWnd = nullptr;
    m_fFocusLocal = true;
    std::memset(m_szFTParamTitle.data(), 0, sizeof m_szFTParamTitle);
    std::memset(m_szFTParamComment.data(), 0, sizeof m_szFTParamComment);
    std::memset(m_szFTParam.data(), 0, sizeof m_szFTParam);
    std::memset(m_szFTConfirmTitle.data(), 0, sizeof m_szFTConfirmTitle);
    std::memset(m_szFTConfirmComment.data(), 0, sizeof m_szFTConfirmComment);
    m_nConfirmAnswer = 0;
    m_fApplyToAll = false;
    m_fShowApplyToAll = true;
    m_nnFileSize = 0;
    std::memset(m_szSrcFileName.data(), 0, sizeof m_szSrcFileName);
    std::memset(m_szSrcFileNameW.data(), 0, sizeof m_szSrcFileNameW);
    std::memset(m_szDestFileNameW.data(), 0, sizeof m_szDestFileNameW);
    m_fEof = false;
    m_fFileUploadError = false;
    m_fCompress = true;
    m_nFileCount = 0;
    std::memset(m_szFileSpec.data(), 0, sizeof m_szFileSpec);
    std::memset(m_szDestFileName.data(), 0, sizeof m_szDestFileName);
    m_dwNbReceivedPackets = 0;
    m_dwNbBytesRead = 0;
    m_dwNbBytesWritten = 0;
    m_dwTotalNbBytesRead = 0;
    m_dwTotalNbBytesWritten = 0;
    m_dwTotalNbBytesNotReallyWritten = 0;
    m_nPacketCount = 0;
    m_fPacketCompressed = false;
    m_fFileDownloadError = false;
    std::memset(m_szIncomingFileTime.data(), 0, sizeof m_szIncomingFileTime);
    m_nNotSent = 0;
    m_dwLastChunkTime = 0;
    m_maxHistExtent = 0;
    m_mmRes = static_cast<MMRESULT>(-1);
    m_timerID = 0xFFFFFFFF;
    m_dwStartTick = ::GetTickCount();

    // adzm 2009-08-02
    std::memset(m_szLastLocalPath.data(), 0, sizeof(m_szLastLocalPath));
    std::memset(m_szLastRemotePath.data(), 0, sizeof(m_szLastRemotePath));
    std::memset(m_szLastRemotePathW.data(), 0, sizeof(m_szLastRemotePathW));
    m_nLastLocalAttemptItem = -1;
    std::memset(m_szLastLocalAttemptName.data(), 0, sizeof(m_szLastLocalAttemptName));
    m_nLastRemoteAttemptItem = -1;
    std::memset(m_szLastRemoteAttemptName.data(), 0, sizeof(m_szLastRemoteAttemptName));

    for (int i{ 0 }; i < 3; ++i)
    {
        bSortDirectionsL[static_cast<unsigned long long>(i)] = false;
        bSortDirectionsR[static_cast<unsigned long long>(i)] = false;
    }
    // 16 April 2008 jdp
    // load richedit so the path display can handly mbcs
    m_hRichEdit = ::LoadLibraryEx(_T("RICHED32.DLL"), nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!m_hRichEdit)
    {
        yesUVNCMessageBox(m_hInstResDLL, nullptr, sz_E1.data(), sz_E2.data(), MB_ICONEXCLAMATION);
    }
    m_szHeadlessRemotePathW[0U] = L'\0';
    m_fHeadlessUpload = false;
    ::InitializeCriticalSection(&crit);
    rfbFileHeaderRequested = false;
    rfbFileTransferOfferRequested = false;
}

//
//
//
FileTransfer::~FileTransfer()
{
    //	vnclog.Print(0, _T("nFileTransfer\n"));
    KillFTTimer();
    ::EnterCriticalSection(&crit);
    m_fFileCommandPending = false;
    m_fFileTransferRunning = false;
    m_FilesList.clear();
    if (m_pMiniZipNG)
    {
        delete m_pMiniZipNG;
    }
    if (m_lpCSBuffer != nullptr)
    {
        delete[] m_lpCSBuffer;
        m_lpCSBuffer = nullptr;
    }
    // 16 April 2008 jdp
    if (m_hRichEdit != nullptr)
    {
        ::FreeLibrary(m_hRichEdit);
    }
    ::LeaveCriticalSection(&crit);
    ::DeleteCriticalSection(&crit);
}


void FileTransfer::InitFTTimer()
{
#ifdef FT_USE_MMTIMER
    if (m_mmRes != -1)
    {
        return;
    }

    m_fSendFileChunk = false;
    m_mmRes = timeSetEvent(10, 0, reinterpret_cast<LPTIMECALLBACK>(fpTimer), reinterpret_cast<DWORD_PTR>(this), TIME_PERIODIC);
#else

    if (m_timerID != 0xFFFFFFFF)
    {
        return;
    }

    g_FileTransferSingleton = this;
    m_fSendFileChunk = false;
    m_timerID = ::SetTimer(nullptr, 0, USER_TIMER_MINIMUM, (TIMERPROC)fpTimerProc);
#endif
}


void FileTransfer::KillFTTimer()
{
#ifdef FT_USE_MMTIMER
    if (m_mmRes != -1)
    {
        timeKillEvent(m_mmRes);
    }
    m_mmRes = static_cast<MMRESULT>(-1);
#else

    if (m_timerID == 0xFFFFFFFF)
    {
        return;
    }

    ::KillTimer(nullptr, m_timerID);
    g_FileTransferSingleton = nullptr;
    m_timerID = 0xFFFFFFFF;
#endif
}

void CALLBACK FileTransfer::fpTimerProc([[maybe_unused]] const HWND hwnd, [[maybe_unused]] const UINT uMsg, [[maybe_unused]] const UINT_PTR idEvent, [[maybe_unused]] const DWORD dwTime)
{
    if (g_FileTransferSingleton != nullptr)
    {
        TimerCallback(g_FileTransferSingleton);
    }
}

void CALLBACK FileTransfer::fpTimer([[maybe_unused]] const UINT uID, [[maybe_unused]] const UINT uMsg, const DWORD_PTR dwUser, [[maybe_unused]] const DWORD_PTR dw1,
                                    [[maybe_unused]] const DWORD_PTR dw2)
{
    FileTransfer* ft = reinterpret_cast<FileTransfer*>(dwUser);

    TimerCallback(ft);
}

void FileTransfer::TimerCallback(FileTransfer* ft)
{
    ::EnterCriticalSection(&ft->crit);
    if (!ft->m_fFileUploadRunning)
    {
        ::LeaveCriticalSection(&ft->crit);
        return;
    }

    if (!ft->m_fSendFileChunk)
    {
        ft->m_fSendFileChunk = true;
        ft->SendFileChunk();
        ft->m_fSendFileChunk = false;
    }
    ::LeaveCriticalSection(&ft->crit);
}

//
//
//
void FileTransfer::ShowFileTransferWindow(bool fVisible)
{
    //	vnclog.Print(0, _T("ShowFileTransferWindow\n"));
    bool bChanged{ false };
    if (m_fVisible != fVisible)
    {
        bChanged = true;
    }

    if (fVisible)
    {
        ::ShowWindow(hWnd, ::IsIconic(hWnd) ? SW_RESTORE : SW_SHOW);
    }
    else
    {
        ::ShowWindow(hWnd, SW_MINIMIZE);
    }
    if (fVisible)
    {
        ::SetForegroundWindow(hWnd);
    }
    // Put the File Transfer Windows always on Top if fullscreen
    if (fVisible && m_pCC->InFullScreenMode())
    {
        RECT Rect;
        ::GetWindowRect(hWnd, &Rect);
        ::SetWindowPos(hWnd, HWND_TOPMOST, Rect.left, Rect.top, Rect.right - Rect.left, Rect.bottom - Rect.top, SWP_SHOWWINDOW);
    }

    m_fVisible = fVisible; // This enables screen updates to be processed in ClientConnection
    // Refresh screen view if File Transfer window has been hidden
    //adzm 2010-09 - all socket writes must remain on a single thread, but we only need an async request here
    if (bChanged && !fVisible)
    {
        m_pCC->SendAppropriateFramebufferUpdateRequest(true);
    }
}


//
// Simply the classic Windows message processing
//
bool PseudoYield(const HWND hWnd)
{
    //	vnclog.Print(0, _T("PseudoYield\n"));
    MSG msg;
    while (::PeekMessage(&msg, hWnd, 0, 0, PM_REMOVE))
    {
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
        if (msg.message == WM_CLOSE)
        {
            return FALSE;
        }
    }
    return TRUE;
}



//
//	ProcessFileTransferMsg
//
//  Here we process all incoming FileTransferMsg stuff
//  coming from the server.
//  The server only sends File Transfer data when requested
//  by the client. Possible request are:
//
//  - Send the list of your drives
//  - Send the content of a directory
//  - Send a file
//  - Accept a file
//  - ...
//
//  We use the main ClientConnection thread and its
//  rfb message reception loop.
//  This function is called by the rfb message processing thread.
//  Thus it's safe to call the ReadExact and ReadString
//  functions in the functions that are called from here:
//  PopulateRemoteListBox, ReceiveFile
//
void FileTransfer::ProcessFileTransferMsg(void)
{
    //	vnclog.Print(0, _T("ProcessFileTransferMsg\n"));
    // Save encoder buffer state to prevent corruption during file transfer
    const bool saved_fReadFromNetRectBuf = m_pCC->m_fReadFromNetRectBuf;
    const int saved_nNetRectBufOffset = m_pCC->m_nNetRectBufOffset;
    const int saved_nReadSize = m_pCC->m_nReadSize;

    // Ensure file transfer reads directly from socket, not from encoder buffers
    m_pCC->m_fReadFromNetRectBuf = false;
    m_pCC->m_nNetRectBufOffset = 0;

    rfbFileTransferMsg ft;
    m_pCC->ReadExact((reinterpret_cast<char*>(&ft)) + m_pCC->m_nTO, sz_rfbFileTransferMsg - m_pCC->m_nTO);
    if (!ViewerBounds::payload(Swap32IfLE(ft.length)))
    {
        throw ErrorException(L"File-transfer payload exceeds size limit.");
    }

    switch (ft.contentType)
    {
    // Response to a rfbDirContentRequest request:
    // some directory data is received from the server
    case rfbFileTransferProtocolVersion: {
        int proto_ver = ft.contentParam;
        if ((proto_ver >= FT_PROTO_VERSION_OLD) && (proto_ver <= FT_PROTO_VERSION_4))
        {
            m_ServerFTProtocolVersion = proto_ver;
        }
        m_fServerSupportsUnicode = (m_ServerFTProtocolVersion >= FT_PROTO_VERSION_4);

        if (m_ServerFTProtocolVersion >= FT_PROTO_VERSION_3)
        {
            CARD32 serverBlockSize = Swap32IfLE(ft.size);
            if ((serverBlockSize >= 4096) && (serverBlockSize <= 1048576))
            {
                m_nBlockSize = serverBlockSize;
            }
        }
    }
    break;

    case rfbDirPacket: {
        // Strip the unicode flag to get the base content param
        const CARD16 rawParam = ft.contentParam;
        const bool fUnicodeEntry = (rawParam & rfbADirUnicode) != 0;
        const CARD16 baseParam = rawParam & ~static_cast<CARD16>(rfbADirUnicode);
        switch (baseParam)
        {
        // Response to a rfbRDrivesList request
        case rfbADrivesList:
            ListRemoteDrives(hWnd, Swap32IfLE(ft.length));
            m_fFileCommandPending = false;
            break;

        // Server signals the folder cannot be read (permission denied, media error, etc.)
        // Old servers never send this value; old viewers treat it as end-of-dir (graceful degradation).
        case rfbADirInaccessible:
            if (m_fDirectoryReceptionRunning)
            {
                FinishDirectoryReception();
                m_fFileCommandPending = false;
            }
            PopulateRemoteListBox(hWnd, 0); // nLen=0 triggers the "inaccessible" UI path
            break;

        // Response to a rfbRDirContent request
        case rfbADirectory:
            if (nDirZipRet == 1)
            {
                SetStatus(L"Folder unzipped.");
                nDirZipRet = 0;
            }
            // fall through
        case rfbAFile:
            if (!m_fDirectoryReceptionRunning)
            {
                PopulateRemoteListBox(hWnd, Swap32IfLE(ft.length), fUnicodeEntry);
            }
            else
            {
                ReceiveDirectoryItem(hWnd, Swap32IfLE(ft.length), fUnicodeEntry);
            }
            break;
        default: // End-of-directory marker (contentParam==0) or unknown
            if (m_fDirectoryReceptionRunning)
            {
                FinishDirectoryReception();
                m_fFileCommandPending = false;
            }
            break;
        }
    }
    break;

    // In response to a rfbFileTransferRequest request
    // A file is received from the server.
    case rfbFileHeader:
        if (rfbFileHeaderRequested)
        {
            ReceiveFiles(Swap32IfLE(ft.size), Swap32IfLE(ft.length));
            rfbFileHeaderRequested = false;
        }
        break;

    // In response to a rfbFileTransferOffer request
    // The server can send the checksums of the destination file before sending a ack through
    // rfbFileAcceptHeader (only if the destination file already exists and is accessible)
    case rfbFileChecksums:
        ReceiveDestinationFileChecksums(static_cast<int>(Swap32IfLE(ft.size)), Swap32IfLE(ft.length));
        m_pCC->SetRecvTimeout();
        break;

    // In response to a rfbFileTransferOffer request
    // A ack or nack is received from the server.
    case rfbFileAcceptHeader:
        if (rfbFileTransferOfferRequested)
        {
            SendFiles(static_cast<long>(Swap32IfLE(ft.size)), Swap32IfLE(ft.length));
            rfbFileTransferOfferRequested = false;
        }
        break;

    // Response to a command
    case rfbCommandReturn:
        switch (ft.contentParam)
        {
        case rfbADirCreate:
            CreateRemoteDirectoryFeedback(static_cast<long>(Swap32IfLE(ft.size)), Swap32IfLE(ft.length));
            m_fFileCommandPending = false;
            break;

        case rfbAFileDelete:
            DeleteRemoteFileFeedback(static_cast<long>(Swap32IfLE(ft.size)), Swap32IfLE(ft.length));
            m_fFileCommandPending = false;
            break;

        case rfbAFileRename:
            RenameRemoteFileOrDirectoryFeedback(static_cast<long>(Swap32IfLE(ft.size)), Swap32IfLE(ft.length));
            m_fFileCommandPending = false;
            break;
        }
        break;

    // Should never be handled here but in the File Transfer Loop
    case rfbFilePacket:
        m_pCC->SetRecvTimeout();
        ReceiveFileChunk(Swap32IfLE(ft.length), static_cast<int>(Swap32IfLE(ft.size)));
        // adzm 2010-09
        m_pCC->SendKeepAlive(false, true);
        break;

    // Should never be handled here but in the File Transfer Loop
    case rfbEndOfFile:
        FinishFileReception();
        break;

    // Abort current File Transfer
    // For versions <= RC18 we also use it to test if we're allowed to use File Transfer on the server
    case rfbAbortFileTransfer:
        // AbortFileDownload();
        if (m_fFileDownloadRunning)
        {
            m_fFileDownloadError = true;
            FinishFileReception();
        }
        else
        {
            // We want the viewer to be backward compatible with UltraVNC Server running the old File Transfer Protocol
            m_ServerFTProtocolVersion = FT_PROTO_VERSION_OLD; // Old permission method -> it's a <= RC18 UltraVNC Server
            m_nBlockSize = 4096;                              // Old packet size value...
            ::ShowWindow(::GetDlgItem(hWnd, IDC_RENAME_B), SW_HIDE);

            TestPermission(static_cast<long>(Swap32IfLE(ft.size)), 0);
        }
        break;

    // New File Transfer handshaking/permission method (from RC19)
    case rfbFileTransferAccess:
        TestPermission(static_cast<long>(Swap32IfLE(ft.size)), ft.contentParam);
        break;

    default:
        break;
    }

    // Restore encoder buffer state after file transfer processing
    m_pCC->m_fReadFromNetRectBuf = saved_fReadFromNetRectBuf;
    m_pCC->m_nNetRectBufOffset = saved_nNetRectBufOffset;
    m_pCC->m_nReadSize = saved_nReadSize;
}


//
// request File Transfer permission
//
void FileTransfer::RequestPermission()
{
    //	vnclog.Print(0, _T("RequestPermission\n"));

    rfbFileTransferMsg ft;
    ft.type = rfbFileTransfer;
    // Versions <= RC18 method
    ft.contentType = rfbAbortFileTransfer;
    // ft.contentParam = 0;
    ft.contentParam = rfbFileTransferVersion; // Old viewer will send 0
    // New method can't be used yet as we want backward compatibility (new UltraVNC Viewer File Transfer must
    // work with old UltraVNC Server File Transfer
    // ft.contentType = rfbFileTransferAccess;
    // ft.contentParam = rfbFileTransferVersion;
    ft.length = 0;
    ft.size = 0;
    m_pCC->WriteExact(reinterpret_cast<char*>(&ft), sz_rfbFileTransferMsg, rfbFileTransfer);
    return;
}

void FileTransfer::StartFTSession()
{
    if (m_ServerFTProtocolVersion < FT_PROTO_VERSION_3)
    {
        return;
    }

    rfbFileTransferMsg ft;
    std::memset(&ft, 0, sizeof ft);
    ft.type = rfbFileTransfer;
    ft.contentType = rfbFileTransferSessionStart;
    m_pCC->WriteExact(reinterpret_cast<char*>(&ft), sz_rfbFileTransferMsg, rfbFileTransfer);
}

void FileTransfer::EndFTSession()
{
    if ((m_ServerFTProtocolVersion < FT_PROTO_VERSION_3) || !m_pCC->m_running)
    {
        return;
    }

    rfbFileTransferMsg ft;
    std::memset(&ft, 0, sizeof ft);
    ft.type = rfbFileTransfer;
    ft.contentType = rfbFileTransferSessionEnd;
    m_pCC->WriteExact(reinterpret_cast<char*>(&ft), sz_rfbFileTransferMsg, rfbFileTransfer);
}
//
// Free lParam (wstring*) memory for all items in a ListView, then clear it
// (0x00000001 == FT_LPARAM_UNREADABLE, kept as literal to avoid class-scope dependency)
//
// Sort comparator: folders (display text starts with '[') before files, then alphabetical within each group
static int CALLBACK FolderFirstSort(const LPARAM lParam1, const LPARAM lParam2, const LPARAM lParamSort)
{
    HWND hWndList = reinterpret_cast<HWND>(lParamSort);
    std::array<WCHAR, MAX_PATH + 4> sz1{ 0 };
    std::array<WCHAR, MAX_PATH + 4> sz2{ 0 };
    ListView_GetItemText(hWndList, lParam1, 0, sz1.data(), static_cast<int>(sz1.size()))
    ListView_GetItemText(hWndList, lParam2, 0, sz2.data(), static_cast<int>(sz2.size()))
    bool b1folder = (sz1[0U] == L'[');
    bool b2folder = (sz2[0U] == L'[');
    if (b1folder != b2folder)
    {
        return b1folder ? -1 : 1;
    }
    return lstrcmpiW(sz1.data(), sz2.data());
}

static void FTListViewClear(const HWND hWndList)
{
    const int nCount = ListView_GetItemCount(hWndList);
    for (int i{ 0 }; i < nCount; ++i)
    {
        LVITEMW ItemW;
        std::memset(&ItemW, 0, sizeof(ItemW));
        ItemW.mask = LVIF_PARAM;
        ItemW.iItem = i;
        ItemW.iSubItem = 0;
        if (::SendMessageW(hWndList, LVM_GETITEMW, 0, reinterpret_cast<LPARAM>(&ItemW)))
        {
            std::wstring* p = reinterpret_cast<std::wstring*>(ItemW.lParam & ~static_cast<LPARAM>(0x00000001));
            delete p;
        }
    }
    ListView_DeleteAllItems(hWndList);
}

//
// Test if we are allowed to access File Transfer
//
bool FileTransfer::TestPermission(const long lSize, [[maybe_unused]] const int nVersion)
{
    //	vnclog.Print(0, _T("TestPermission\n"));
    if (lSize == -1)
    {
        m_fFTAllowed = false;
        HWND hWndRemoteList = ::GetDlgItem(hWnd, IDC_REMOTE_FILELIST);
        ::SendDlgItemMessage(hWnd, IDC_REMOTE_DRIVECB, LB_RESETCONTENT, 0, 0L);
        FTListViewClear(hWndRemoteList);
        ::SetDlgItemTextW(hWnd, IDC_CURR_REMOTE, sz_H1.data());
        ::SetDlgItemTextW(hWnd, IDC_REMOTE_STATUS, sz_H2.data());
        SetStatus(sz_H3.data());
        DisableButtons(hWnd, false);
        ::ShowWindow(::GetDlgItem(hWnd, IDCANCEL), SW_SHOW);
        ::ShowWindow(::GetDlgItem(hWnd, IDCANCEL2), SW_SHOW);
    }
    else
    {
        m_fFTAllowed = true;
        StartFTSession();
        RequestRemoteDrives();
        SetStatus(sz_H4.data());

        // If headless upload mode, trigger auto-upload after permission granted
        if (g_fHeadlessUpload && hWnd)
        {
            ::PostMessage(hWnd, WM_APP + 1, 0, 0);
        }
    }

    return true;
}


//
// Receive all the files that are referenced in m_FilesList
//
bool FileTransfer::ReceiveFiles(const unsigned long lSize, const UINT nLen)
{
    //	vnclog.Print(0, _T("ReceiveFiles\n"));
    // Receive the incoming file
    ++m_nFilesTransfered;
    SetGlobalCount();
    if (!ReceiveFile(lSize, nLen))
    {
        RequestNextFile();
    }

    return true;
}

//
// A file has just been received
// Request the following one if any
//
bool FileTransfer::RequestNextFile()
{
    //	vnclog.Print(0, _T("RequestNextFile\n"));

    SetGlobalCount();

    ++m_iFile; // go to next file in the list of files to receive

    HWND hWndLocalList = ::GetDlgItem(hWnd, IDC_LOCAL_FILELIST);
    HWND hWndRemoteList = ::GetDlgItem(hWnd, IDC_REMOTE_FILELIST);

    // If one more file has to be received, ask for it !
    if ((m_iFile != m_FilesList.end()) && !m_fAbort)
    {
        // Retrieve Unicode filename from lParam
        LVITEMW ItemW;
        std::memset(&ItemW, 0, sizeof(ItemW));
        ItemW.mask = LVIF_PARAM;
        ItemW.iItem = *m_iFile;
        ItemW.iSubItem = 0;
        ::SendMessageW(hWndRemoteList, LVM_GETITEMW, 0, reinterpret_cast<LPARAM>(&ItemW));
        std::wstring* pNameW = reinterpret_cast<std::wstring*>(ItemW.lParam & ~static_cast<LPARAM>(FT_LPARAM_UNREADABLE));

        // Check display text to determine if it's a directory (shown as "[ name ]")
        bool bIsDir{ false };
        {
            std::array<WCHAR, MAX_PATH + 4> _disp{ 0 };
            LVITEMW _dii;
            std::memset(&_dii, 0, sizeof(_dii));
            _dii.mask = LVIF_TEXT;
            _dii.iItem = *m_iFile;
            _dii.pszText = _disp.data();
            _dii.cchTextMax = MAX_PATH + 4;
            ::SendMessageW(hWndRemoteList, LVM_GETITEMTEXTW, static_cast<WPARAM>(*m_iFile), reinterpret_cast<LPARAM>(&_dii));
            if ((_disp[0U] == L'[') && (_disp[1U] == L' '))
            {
                bIsDir = true;
            }
        }

        std::array<WCHAR, MAX_PATH * 4> szDstFileW;
        ::GetDlgItemTextW(hWnd, IDC_CURR_REMOTE, szDstFileW.data(), MAX_PATH * 4);
        if (!std::wcslen(szDstFileW.data()))
        {
            return false;
        } // no destination dir selected
        if (pNameW)
        {
            if (bIsDir)
            {
                ::wcscat_s(szDstFileW.data(), MAX_PATH * 4, L"[ ");
                ::wcscat_s(szDstFileW.data(), MAX_PATH * 4, pNameW->c_str());
                ::wcscat_s(szDstFileW.data(), MAX_PATH * 4, L" ]");
            }
            else
            {
                ::wcscat_s(szDstFileW.data(), MAX_PATH * 4, pNameW->c_str());
            }
        }

        // Convert full Unicode path to UTF-8 for sending to server
        std::array<char, MAX_PATH * 3> szDstFileUTF8;
        ::WideCharToMultiByte(CP_UTF8, 0, szDstFileW.data(), -1, szDstFileUTF8.data(), static_cast<int>(szDstFileUTF8.size()), nullptr, nullptr);
        RequestRemoteFile(szDstFileUTF8.data());
    }
    else // All the files have been processed and hopefully received
    {
        // Refresh the local list so new files are displayed and highlighted
        FTListViewClear(hWndLocalList);
        PopulateLocalListBoxW(hWnd, L"");

        if (m_fAbort)
        {
            SetStatus(sz_H5.data());
        }
        else if (!m_fFileDownloadError)
        {
            SetStatus(sz_H6.data());
        }

        EnableButtons(hWnd);

        ShowFileTransferWindow(true);
        ::Sleep(1000);
        //MessageBeep(-1);

        // Unlock
        m_fFileCommandPending = false;
    }

    return true;
}


//
// Send all the files that are referenced in m_FilesList
//
bool FileTransfer::SendFiles(const long lSize, const UINT nLen)
{
    InitFTTimer(); // sf@2005

    //	vnclog.Print(0, _T("SendFiles\n"));
    // Receive the incoming file
    ++m_nFilesTransfered;
    SetGlobalCount();
    if (!SendFile(lSize, nLen))
    {
        OfferNextFile();
    }

    return true;
}


bool FileTransfer::OfferNextFile()
{
    //	vnclog.Print(0, _T("OfferNextFile\n"));
    SetGlobalCount();

    ++m_iFile; // go to next file in the list of files to send

    HWND hWndLocalList = ::GetDlgItem(hWnd, IDC_LOCAL_FILELIST);
    HWND hWndRemoteList = ::GetDlgItem(hWnd, IDC_REMOTE_FILELIST);

    // If one more file has to be sent, offer it !
    if ((m_iFile != m_FilesList.end()) && !m_fAbort)
    {
        // Retrieve Unicode filename from lParam
        LVITEMW ItemW;
        std::memset(&ItemW, 0, sizeof(ItemW));
        ItemW.mask = LVIF_PARAM;
        ItemW.iItem = *m_iFile;
        ItemW.iSubItem = 0;
        ::SendMessageW(hWndLocalList, LVM_GETITEMW, 0, reinterpret_cast<LPARAM>(&ItemW));
        std::wstring* pNameW = reinterpret_cast<std::wstring*>(ItemW.lParam & ~static_cast<LPARAM>(FT_LPARAM_UNREADABLE));

        // Build full local path in Unicode
        std::array<WCHAR, MAX_PATH * 4> szSrcFileW;
        ::GetDlgItemTextW(hWnd, IDC_CURR_LOCAL, szSrcFileW.data(), static_cast<int>(szSrcFileW.size()));
        if (!std::wcslen(szSrcFileW.data()))
        {
            return false;
        } // no src dir selected
        if (pNameW)
        {
            ::wcscat_s(szSrcFileW.data(), szSrcFileW.size(), pNameW->c_str());
        }

        // Check if item is a directory - if so, wrap with brackets so ZipPossibleDirectory
        // recognizes it. The lParam stores the raw name without brackets for both files and dirs.
        DWORD dwAttr = ::GetFileAttributesW(szSrcFileW.data());
        if (dwAttr == INVALID_FILE_ATTRIBUTES)
        {
            // Fallback: try short path (handles bidi control chars like U+202B that
            // cause GetFileAttributesW to return INVALID_FILE_ATTRIBUTES)
            std::array<WCHAR, MAX_PATH * 4> szShortAttrW;
            if (::GetShortPathNameW(szSrcFileW.data(), szShortAttrW.data(), static_cast<DWORD>(szShortAttrW.size())))
            {
                dwAttr = ::GetFileAttributesW(szShortAttrW.data());
            }
        }
        bool bIsDir = ((dwAttr != INVALID_FILE_ATTRIBUTES) && (dwAttr & FILE_ATTRIBUTE_DIRECTORY));
#if DEBUG_FT
        {
            std::array<wchar_t, MAX_PATH * 4 + 64> _dbgAttr;
            ::_snwprintf_s(_dbgAttr.data(), _dbgAttr.size(), _TRUNCATE, L"  OfferNextFile: szSrcFileW=[%s] dwAttr=0x%lx bIsDir=%d\n", szSrcFileW.data(), dwAttr, static_cast<int>(bIsDir));
            ::OutputDebugStringW(_dbgAttr.data());
        }
#endif
        if (bIsDir && pNameW)
        {
            // Rebuild as bracketed path: dir\[ name ] - ZipPossibleDirectory expects rfbDirPrefix/Suffix format
            ::GetDlgItemTextW(hWnd, IDC_CURR_LOCAL, szSrcFileW.data(), static_cast<int>(szSrcFileW.size()));
            ::wcscat_s(szSrcFileW.data(), szSrcFileW.size(), L"[ ");
            ::wcscat_s(szSrcFileW.data(), szSrcFileW.size(), pNameW->c_str());
            ::wcscat_s(szSrcFileW.data(), szSrcFileW.size(), L" ]");
        }

        // Store Unicode path for OfferLocalFile (avoids CP_ACP corruption of Chinese filenames)
        ::wcscpy_s(m_szSrcFileNameW.data(), m_szSrcFileNameW.size(), szSrcFileW.data());
        // CP_ACP version for legacy code paths
        std::array<char, MAX_PATH * 4> szSrcFile;
        ::WideCharToMultiByte(CP_ACP, 0, szSrcFileW.data(), -1, szSrcFile.data(), static_cast<int>(szSrcFile.size()), nullptr, nullptr);

        if (!OfferLocalFile(szSrcFile.data()))
        {
            SendFiles(-1, 0);
        }
    }
    else // All the files have been processed and hopefully received
    {
        // In headless mode, close dialog after completion
        if (g_fHeadlessUpload)
        {
            if (m_fAbort)
            {
                SetStatus(sz_H7.data());
            }
            else if (!m_fFileUploadError)
            {
                SetStatus(sz_H6.data());
            }

            ::Sleep(1000); // Brief delay to show final status

            m_fFileCommandPending = false;
            KillFTTimer();
            g_fHeadlessUpload = false;

            // Close the dialog
            ::EndDialog(hWnd, TRUE);

            // Close the viewer window
            if (m_pCC && m_pCC->m_hwndMain)
            {
                ::PostMessage(m_pCC->m_hwndMain, WM_CLOSE, 0, 0);
            }

            // Disconnect viewer
            ::PostQuitMessage(0);
            return true;
        }

        // Normal interactive mode continues...
        // Refresh the remote list so new files are displayed and highlighted
        FTListViewClear(hWndRemoteList);
        RequestRemoteDirectoryContent(hWnd, L"");

        if (m_fAbort)
        {
            SetStatus(sz_H7.data());
        }
        else if (!m_fFileUploadError)
        {
            SetStatus(sz_H6.data());
        }

        //		EnableButtons(hWnd);

        ShowFileTransferWindow(true);
        ::Sleep(1000);
        //MessageBeep(-1);
        if (!m_fAbort && (nDirZipRet == 1))
        {
            SetStatus(L"Decompressing folder(s). Please wait...");
        }

        // Unlock
        m_fFileCommandPending = false;

        KillFTTimer(); // sf@2005
    }

    return true;
}


//
// Format file size so it is user friendly to read
//
void FileTransfer::GetFriendlyFileSizeString(__int64 Size, char* const szText, const int size)
{
    szText[0U] = '\0';
    if (Size > (1024 * 1024 * 1024))
    {
        __int64 lRest = (Size % (1024 * 1024 * 1024));
        Size /= (1024 * 1024 * 1024);
        ::_snprintf_s(szText, static_cast<size_t>(size), 256, "%u.%2.2lu Gb", static_cast<unsigned long>(Size),
                    static_cast<unsigned long>((lRest * 100 / 1024 / 1024 / 1024)));
    }
    else if (Size > (1024 * 1024))
    {
        unsigned long lRest = (static_cast<unsigned long>(Size % (1024 * 1024)));
        Size /= (1024 * 1024);
        ::_snprintf_s(szText, static_cast<size_t>(size), 256, "%u.%2.2lu Mb", static_cast<unsigned long>(Size), lRest * 100 / 1024 / 1024);
    }
    else if (Size > 1024)
    {
        unsigned long lRest = static_cast<unsigned long>(Size % (1024));
        Size /= 1024;
        ::_snprintf_s(szText, static_cast<size_t>(size), 256, "%u.%2.2lu Kb", static_cast<unsigned long>(Size), lRest * 100 / 1024);
    }
    else
    {
        ::_snprintf_s(szText, static_cast<size_t>(size), 256, "%u bytes", static_cast<unsigned long>(Size));
    }
}




//
// GetFileSize() doesn't handle files > 4GBytes...
// GetFileSizeEx() doesn't exist under Win9x...
// So let's write our own function.
//
bool FileTransfer::MyGetFileSize(const LPCWSTR szFilePath, ULARGE_INTEGER* const n2FileSize)
{
    WIN32_FIND_DATAW fdW;
    HANDLE ff;

    ::SetErrorMode(SEM_FAILCRITICALERRORS); // No popup please !
    ff = ::FindFirstFileW(szFilePath, &fdW);
    ::SetErrorMode(0);

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


//
// Add a file line to a ListView
//
void FileTransfer::AddFileToFileList(const HWND hWndIn, const int nListId, WIN32_FIND_DATA& fd, const bool fLocalSide,
                                     const WCHAR* const pszUnicodeFileName)
{
    //	vnclog.Print(0, _T("AddFileToFileList\n"));
    HWND hWndList = ::GetDlgItem(hWndIn, nListId);

    // Build Unicode filename: use pszUnicodeFileName if provided, else convert fd.cFileName from CP_ACP
    std::array<WCHAR, MAX_PATH + 2> szFileNameW;
    if (pszUnicodeFileName)
    {
        ::wcscpy_s(szFileNameW.data(), szFileNameW.size(), pszUnicodeFileName);
    }
    else
    {
        ::MultiByteToWideChar(CP_ACP, 0, reinterpret_cast<LPCSTR>(fd.cFileName), -1, szFileNameW.data(), MAX_PATH);
    }

    if ((((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY) && std::wcscmp(szFileNameW.data(), L".")) ||
        (!std::wcscmp(szFileNameW.data(), L"..")))
    {
        // Build "[dirname]" display name in Unicode
        std::array<WCHAR, MAX_PATH + 4> szDirNameW;
        std::array<WCHAR, 4U> szPrefixW;
        std::array<WCHAR, 4U> szSuffixW;
        ::MultiByteToWideChar(CP_ACP, 0, rfbDirPrefix, -1, szPrefixW.data(), 4);
        ::MultiByteToWideChar(CP_ACP, 0, rfbDirSuffix, -1, szSuffixW.data(), 4);
        ::swprintf_s(szDirNameW.data(), szDirNameW.size(), L"%s%s%s", szPrefixW.data(), szFileNameW.data(), szSuffixW.data());

        // Insert using Unicode message to support Chinese filenames regardless of dialog mode
        LVITEMW ItemW;
        std::memset(&ItemW, 0, sizeof(ItemW));
        ItemW.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;
        ItemW.iItem = 0;
        ItemW.iSubItem = 0;
        ItemW.iImage = 0;
        ItemW.pszText = szDirNameW.data();
        // Store heap-allocated Unicode filename in lParam (freed when list is cleared)
        ItemW.lParam = reinterpret_cast<LPARAM>((new std::wstring(szFileNameW.data())));
        int nItem = static_cast<int>(::SendMessageW(hWndList, LVM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(&ItemW)));

        // Type subitem
        LVITEMW SubW;
        std::memset(&SubW, 0, sizeof(SubW));
        SubW.mask = LVIF_TEXT;
        SubW.iItem = nItem;
        SubW.iSubItem = 1;
        SubW.pszText = L"Folder";
        ::SendMessageW(hWndList, LVM_SETITEMW, 0, reinterpret_cast<LPARAM>(&SubW));

        // For remote items: server sets dwReserved0=rfbFD_INACCESSIBLE when subfolder can't be read
        if (!fLocalSide && std::wcscmp(szFileNameW.data(), L"..") && (fd.dwReserved0 == rfbFD_INACCESSIBLE))
        {
            LVITEMW flagItem;
            std::memset(&flagItem, 0, sizeof(flagItem));
            flagItem.mask = LVIF_PARAM;
            flagItem.iItem = nItem;
            flagItem.iSubItem = 0;
            ::SendMessageW(hWndList, LVM_GETITEMW, 0, reinterpret_cast<LPARAM>(&flagItem));
            flagItem.lParam |= FT_LPARAM_UNREADABLE;
            ::SendMessageW(hWndList, LVM_SETITEMW, 0, reinterpret_cast<LPARAM>(&flagItem));
        }

        // Proactively check if local folder is accessible
        if (fLocalSide && std::wcscmp(szFileNameW.data(), L".."))
        {
            std::array<WCHAR, MAX_PATH * 4> szCurrPathW;
            ::GetDlgItemTextW(hWndIn, IDC_CURR_LOCAL, szCurrPathW.data(), static_cast<int>(szCurrPathW.size()));
            if (std::wcslen(szCurrPathW.data()) > 0)
            {
                std::array<WCHAR, MAX_PATH * 4> szProbeW;
                ::swprintf_s(szProbeW.data(), szProbeW.size(), L"%s%s\\*", szCurrPathW.data(), szFileNameW.data());
                WIN32_FIND_DATAW fdProbeW;
                ::SetErrorMode(SEM_FAILCRITICALERRORS);
                HANDLE hProbe = ::FindFirstFileW(szProbeW.data(), &fdProbeW);
                ::SetErrorMode(0);
                if (hProbe == INVALID_HANDLE_VALUE)
                {
                    LVITEMW flagItem;
                    std::memset(&flagItem, 0, sizeof(flagItem));
                    flagItem.mask = LVIF_PARAM;
                    flagItem.iItem = nItem;
                    flagItem.iSubItem = 0;
                    ::SendMessageW(hWndList, LVM_GETITEMW, 0, reinterpret_cast<LPARAM>(&flagItem));
                    flagItem.lParam |= FT_LPARAM_UNREADABLE;
                    ::SendMessageW(hWndList, LVM_SETITEMW, 0, reinterpret_cast<LPARAM>(&flagItem));
                }
                else
                {
                    ::FindClose(hProbe);
                }
            }
        }
    }
    else if (std::wcscmp(szFileNameW.data(), L".")) // Test actually Not necessary for remote list
    {
        // Insert file using Unicode message - append at end so folders always appear first
        LVITEMW ItemW;
        std::memset(&ItemW, 0, sizeof(ItemW));
        ItemW.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;
        ItemW.iItem = ListView_GetItemCount(hWndList); // append after all existing items (folders)
        ItemW.iSubItem = 0;
        ItemW.iImage = 1;
        ItemW.pszText = szFileNameW.data();
        // Store heap-allocated Unicode filename in lParam (freed when list is cleared)
        ItemW.lParam = reinterpret_cast<LPARAM>((new std::wstring(szFileNameW.data())));
        const int nItem = static_cast<int>(::SendMessageW(hWndList, LVM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(&ItemW)));

        // Size
        __int64 Size = ((static_cast<__int64>(fd.nFileSizeHigh)) << 32) + fd.nFileSizeLow;
        std::array<char, 256U> szText;
        GetFriendlyFileSizeString(Size, szText.data(), 256);
        std::array<WCHAR, 256U> szTextW;
        ::MultiByteToWideChar(CP_ACP, 0, szText.data(), -1, szTextW.data(), 256);

        LVITEMW SubW;
        std::memset(&SubW, 0, sizeof(SubW));
        SubW.mask = LVIF_TEXT;
        SubW.iItem = nItem;
        SubW.iSubItem = 1;
        SubW.pszText = szTextW.data();
        ::SendMessageW(hWndList, LVM_SETITEMW, 0, reinterpret_cast<LPARAM>(&SubW));

        // Last Modif Time - display using OS locale format, store ISO prefix for correct sorting
        // Format stored: "YYYYMMDDHHMMSS|locale_date locale_time" - sort on full string, display after '|'
        FILETIME LocalFileTime;
        ::FileTimeToLocalFileTime(&fd.ftLastWriteTime, &LocalFileTime);
        SYSTEMTIME FileTime;
        ::FileTimeToSystemTime(&LocalFileTime, &FileTime);
        std::array<WCHAR, 64U> szDateW{ 0 };
        std::array<WCHAR, 32U> szTimeW{ 0 };
        ::GetDateFormatW(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &FileTime, nullptr, szDateW.data(), 64);
        ::GetTimeFormatW(LOCALE_USER_DEFAULT, TIME_NOSECONDS, &FileTime, nullptr, szTimeW.data(), 32);
        std::array<WCHAR, 128U> szSortableW;
        ::swprintf_s(szSortableW.data(), szSortableW.size(), L"%04d%02d%02d%02d%02d|%s %s", FileTime.wYear, FileTime.wMonth, FileTime.wDay, FileTime.wHour, FileTime.wMinute, szDateW.data(), szTimeW.data());

        SubW.mask = LVIF_TEXT;
        SubW.iSubItem = 2;
        SubW.pszText = szSortableW.data();
        ::SendMessageW(hWndList, LVM_SETITEMW, 0, reinterpret_cast<LPARAM>(&SubW));
    }
}


//
// Select the new transfered files in the dest FileList so the user find them easely
//
void FileTransfer::HighlightTransferedFiles(const HWND hSrcList, const HWND hDstList)
{
    //	vnclog.Print(0, _T("HighlightTransferedFiles\n"));
    if (m_FilesList.size() > 0)
    {
        std::array<TCHAR, 128U> szSelectedFile;

        LVITEM Item;
        Item.mask = LVIF_TEXT;
        Item.iSubItem = 0;
        Item.pszText = szSelectedFile.data();
        Item.cchTextMax = 128;

        LVFINDINFO Info;
        Info.flags = LVFI_STRING;
        Info.psz = reinterpret_cast<LPWSTR>(szSelectedFile.data());

        for (m_iFile = m_FilesList.begin(); m_iFile != m_FilesList.end(); ++m_iFile)
        {
            // Get the name of the file sent
            Item.iItem = *m_iFile;
            ListView_GetItem(hSrcList, &Item);

            // Find this file in the list and highlight it
            int nTheIndex = ListView_FindItem(hDstList, -1, &Info);
            if (nTheIndex > -1)
            {
                ListView_SetItemState(hDstList, nTheIndex, LVIS_SELECTED, LVIS_SELECTED)
                ListView_EnsureVisible(hDstList, nTheIndex, FALSE);
            }
        }
        m_FilesList.clear();
    }
}


//
//
//
bool FileTransfer::IsShortcutFolder(const LPCWSTR szPath)
{
    //	vnclog.Print(0, _T("IsShortcutFolder\n"));
    // Todo: Cultures Translation
    if (!::_wcsnicmp(szPath, L"[ My Documents ]", std::wcslen(L"[ My Documents ]")))
    {
        return true;
    }
    if (!::_wcsnicmp(szPath, L"[ Desktop ]", std::wcslen(L"[ Desktop ]")))
    {
        return true;
    }
    if (!::_wcsnicmp(szPath, L"[ Network Favorites ]", std::wcslen(L"[ Network Favorites ]")))
    {
        return true;
    }

    return false;
}


//
//
//
bool FileTransfer::ResolvePossibleShortcutFolder(const HWND hWndIn, const LPSTR szFolder)
{
    //	vnclog.Print(0, _T("ResolvePossibleShortcutFolder\n"));
    std::array<TCHAR, MAX_PATH> szP;
    int nFolder = -1;

    std::array<char, 64U> szGUIDir;

    ::sprintf_s(szGUIDir.data(), szGUIDir.size(), "%s%s%s", rfbDirPrefix, "My Documents", rfbDirSuffix);
    if (!::_strnicmp(szFolder, szGUIDir.data(), std::strlen(szGUIDir.data())))
    {
        nFolder = CSIDL_PERSONAL;
    }

    ::sprintf_s(szGUIDir.data(), szGUIDir.size(), "%s%s%s", rfbDirPrefix, "Desktop", rfbDirSuffix);
    if (!::_strnicmp(szFolder, szGUIDir.data(), std::strlen(szGUIDir.data())))
    {
        nFolder = CSIDL_DESKTOP;
    }

    ::sprintf_s(szGUIDir.data(), szGUIDir.size(), "%s%s%s", rfbDirPrefix, "Network Favorites", rfbDirSuffix);
    if (!::_strnicmp(szFolder, szGUIDir.data(), std::strlen(szGUIDir.data())))
    {
        nFolder = CSIDL_NETHOOD;
    }

    /*
    if (!strnicmp(szFolder, "(Net. Shares)", 9))
        nFolder = CSIDL_NETWORK;
    */

    if (nFolder != -1)
    {
        // Use Unicode version directly
        if (GetSpecialFolderPathW(nFolder, szP.data()))
        {
            std::size_t len = std::wcslen(szP.data());
            if ((len > 0) && (szP[len - 1] != L'\\') && (len < MAX_PATH - 1))
            {
                szP[len] = L'\\';
                szP[len + 1] = L'\0';
            }
            ::SetDlgItemTextW(hWndIn, IDC_CURR_LOCAL, szP.data());
        }
        return true;
    }
    else
    {
        ::MultiByteToWideChar(CP_ACP, 0, szFolder, -1, szP.data(), static_cast<int>(szP.size()));

        //PGM len of "[ C: ] - Local Disk" is always > 2

        // Check first 2 chars directly in Unicode - no conversion needed
        if (!((szP[0U] == L'[') && (szP[1U] == L' '))) //PGM
        {                                              //PGM
            int len = static_cast<int>(std::wcslen(szP.data()));

            if (len > 2)
            {
                if (::GetFileAttributesW(szP.data()) & FILE_ATTRIBUTE_DIRECTORY)
                {
                    ::SetDlgItemTextW(hWndIn, IDC_CURR_LOCAL, szP.data());
                    return true;
                }
            }
        } //PGM
    }
    return false;
}


bool FileTransfer::GetSpecialFolderPathW(const int nId, WCHAR* const szPathW)
{
    LPITEMIDLIST pidl;

    if (::SHGetSpecialFolderLocation(nullptr, nId, &pidl) != NOERROR)
    {
        return false;
    }

    if (!::SHGetPathFromIDListW(pidl, szPathW))
    {
        return false;
    }

    return true;
}

// Legacy ANSI version - kept for compatibility
bool FileTransfer::GetSpecialFolderPath(const int nId, char* const szPath)
{
    std::array<WCHAR, MAX_PATH> szPathW;
    if (!GetSpecialFolderPathW(nId, szPathW.data()))
    {
        return false;
    }
    ::WideCharToMultiByte(CP_ACP, 0, szPathW.data(), -1, szPath, MAX_PATH, nullptr, nullptr);
    return true;
}
//
// Unicode wrapper for PopulateLocalListBox
//
void FileTransfer::PopulateLocalListBoxW(const HWND hWndIn, LPCWSTR szPathW)
{
    if (szPathW[0U] == L'\0')
    {
        // Empty string = double-click or refresh - delegate directly
        std::array<char, 1U> szPath{ 0 };
        PopulateLocalListBox(hWndIn, szPath.data());
        return;
    }

    // Check for "[ .. ]" - up directory
    std::array<WCHAR, 16U> szUpDirMaskW;
    ::_snwprintf_s(szUpDirMaskW.data(), 16, _TRUNCATE, L"%hs..%hs", rfbDirPrefix, rfbDirSuffix);
    if (!::_wcsicmp(szPathW, szUpDirMaskW.data()))
    {
        // Go up one directory - read current path, strip last component
        std::array<WCHAR, MAX_PATH * 4> szCurrentW;
        ::GetDlgItemTextW(hWndIn, IDC_CURR_LOCAL, szCurrentW.data(), static_cast<int>(szCurrentW.size()));
        if (std::wcslen(szCurrentW.data()) < 1)
        {
            return;
        }
        // Strip trailing backslash
        if (szCurrentW[std::wcslen(szCurrentW.data()) - 1] == L'\\')
        {
            szCurrentW[std::wcslen(szCurrentW.data()) - 1] = L'\0';
        }
        WCHAR* p = std::wcsrchr(szCurrentW.data(), L'\\');
        if (p == nullptr)
        {
            ListDrives(hWndIn);
            return;
        } // At root, show drives
        *(p + 1) = L'\0'; // Keep trailing backslash
        ::SetDlgItemTextW(hWndIn, IDC_CURR_LOCAL, szCurrentW.data());
        // IDC_CURR_LOCAL is set - PopulateLocalListBox will read it
        std::array<char, 1U> szEmpty{ 0 };
        PopulateLocalListBox(hWndIn, szEmpty.data());
        return;
    }

    // Check for bracketed format from combo box: "[ X: ] - Type" or "[ Shortcut ]"
    if ((szPathW[0U] == L'[') && (szPathW[1U] == L' '))
    {
        // Check for special folder shortcuts first
        if (IsShortcutFolder(szPathW))
        {
            // Resolve shortcut folder
            int nFolder = -1;
            if (!::_wcsnicmp(szPathW, L"[ My Documents ]", std::wcslen(L"[ My Documents ]")))
            {
                nFolder = CSIDL_PERSONAL;
            }
            else if (!::_wcsnicmp(szPathW, L"[ Desktop ]", std::wcslen(L"[ Desktop ]")))
            {
                nFolder = CSIDL_DESKTOP;
            }
            else if (!::_wcsnicmp(szPathW, L"[ Network Favorites ]", std::wcslen(L"[ Network Favorites ]")))
            {
                nFolder = CSIDL_NETHOOD;
            }
            if (nFolder != -1)
            {
                std::array<WCHAR, MAX_PATH * 4> szFolderW;
                if (GetSpecialFolderPathW(nFolder, szFolderW.data()))
                {
                    std::size_t len = std::wcslen(szFolderW.data());
                    if ((len > 0) && (szFolderW[len - 1] != L'\\') && (len < MAX_PATH * 4 - 2))
                    {
                        szFolderW[len] = L'\\';
                        szFolderW[len + 1] = L'\0';
                    }
                    ::SetDlgItemTextW(hWndIn, IDC_CURR_LOCAL, szFolderW.data());
                    FTListViewClear(::GetDlgItem(hWndIn, IDC_LOCAL_FILELIST));
                    std::array<char, 1U> szEmpty{ 0 };
                    PopulateLocalListBox(hWndIn, szEmpty.data());
                    return;
                }
            }
            return;
        }

        // Drive letter: "[ C: ] - Local Disk" or "[ C: ]"
        // Extract drive letter from position 2-3 (e.g., "C:")
        std::array<WCHAR, 4U> szDriveW{ 0 };
        szDriveW[0U] = szPathW[2U];
        szDriveW[1U] = szPathW[3U];
        szDriveW[2U] = L'\0';
        if (szDriveW[1U] == L':')
        {
            std::array<WCHAR, 8U> szDrivePathW;
            ::_snwprintf_s(szDrivePathW.data(), 8, _TRUNCATE, L"%s\\", szDriveW.data());
            ::SetDlgItemTextW(hWndIn, IDC_CURR_LOCAL, szDrivePathW.data());
            FTListViewClear(::GetDlgItem(hWndIn, IDC_LOCAL_FILELIST));
            std::array<char, 1U> szEmpty{ 0 };
            PopulateLocalListBox(hWndIn, szEmpty.data());
            return;
        }
    }

    // Fallback: convert to ANSI for legacy code path
    std::array<char, MAX_PATH> szPath;
    ::WideCharToMultiByte(CP_ACP, 0, szPathW, -1, szPath.data(), MAX_PATH, nullptr, nullptr);
    PopulateLocalListBox(hWndIn, szPath.data());
}

//
// Populate the local machine listbox with files located in szPath
//
void FileTransfer::PopulateLocalListBox(const HWND hWndIn, const LPSTR szPath)
{
    //	vnclog.Print(0, _T("PopulateLocalListBox\n"));
    std::array<char, MAX_PATH> ofDir;
    std::array<char, MAX_PATH> ofDirT;
    std::array<WCHAR, MAX_PATH * 4> ofDirW{ 0 };             // Unicode search path - avoids ANSI round-trip corruption
    std::array<WCHAR, MAX_PATH * 4> szSavedParentPathW{ 0 }; // Unicode parent path for error recovery
    int nSelected = -1;
    int nCount{ 0 };
    int nFileCount{ 0 };


    HWND hWndLocalList = ::GetDlgItem(hWndIn, IDC_LOCAL_FILELIST);
    HWND hWndRemoteList = ::GetDlgItem(hWndIn, IDC_REMOTE_FILELIST);

    ofDir[0U] = '\0';
    ofDirT[0U] = '\0';

    if (lstrlenA(szPath) == 0)
    {
        nCount = ListView_GetItemCount(hWndLocalList);
        for (nSelected = 0; nSelected < nCount; ++nSelected)
        {
            if (ListView_GetItemState(hWndLocalList, nSelected, LVIS_SELECTED) & LVIS_SELECTED)
            {
                // Get the full Unicode filename from lParam (not display text which may be truncated)
                LVITEMW ItemW;
                std::memset(&ItemW, 0, sizeof(ItemW));
                ItemW.mask = LVIF_PARAM;
                ItemW.iItem = nSelected;
                ItemW.iSubItem = 0;
                ::SendMessageW(hWndLocalList, LVM_GETITEMW, 0, reinterpret_cast<LPARAM>(&ItemW));
                std::wstring* pNameW = reinterpret_cast<std::wstring*>(ItemW.lParam & ~static_cast<LPARAM>(FT_LPARAM_UNREADABLE));

                // Work in Unicode - no ANSI conversion needed!
                std::array<WCHAR, MAX_PATH * 4> szFolderNameW{ 0 };
                if (pNameW)
                {
                    ::wcscpy_s(szFolderNameW.data(), szFolderNameW.size(), pNameW->c_str());
                }
                else
                {
                    // Fallback: read display text and strip brackets
                    std::array<WCHAR, MAX_PATH> szDispW{ 0 };
                    ItemW.mask = LVIF_TEXT;
                    ItemW.pszText = szDispW.data();
                    ItemW.cchTextMax = MAX_PATH;
                    ::SendMessageW(hWndLocalList, LVM_GETITEMW, 0, reinterpret_cast<LPARAM>(&ItemW));
                    // Strip brackets: "[ name ]" -> "name"
                    if ((std::wcslen(szDispW.data()) > 4) && (szDispW[0U] == L'[') && (szDispW[1U] == L' '))
                    {
                        ::wcsncpy_s(szFolderNameW.data(), szFolderNameW.size(), szDispW.data() + 2, std::wcslen(szDispW.data()) - 4);
                        szFolderNameW[std::wcslen(szDispW.data()) - 4] = L'\0';
                    }
                    else
                    {
                        ::wcscpy_s(szFolderNameW.data(), szFolderNameW.size(), szDispW.data());
                    }
                }

                // Now navigate to this folder - build full path in Unicode
                std::array<WCHAR, MAX_PATH * 4> szCurrentPathW;
                ::GetDlgItemTextW(hWndIn, IDC_CURR_LOCAL, szCurrentPathW.data(), static_cast<int>(szCurrentPathW.size()));

                std::array<WCHAR, MAX_PATH * 4> szNewPathW;
                if (!::_wcsicmp(szFolderNameW.data(), L".."))
                {
                    // Go up one directory
                    if (std::wcslen(szCurrentPathW.data()) < 1)
                    {
                        return;
                    }
                    ::wcscpy_s(szNewPathW.data(), szNewPathW.size(), szCurrentPathW.data());
                    if (szNewPathW[std::wcslen(szNewPathW.data()) - 1] == L'\\')
                    {
                        szNewPathW[std::wcslen(szNewPathW.data()) - 1] = L'\0';
                    }
                    WCHAR* p = std::wcsrchr(szNewPathW.data(), L'\\');
                    if (p == nullptr)
                    {
                        // At root of drive, show drives list
                        ListDrives(hWndIn);
                        return;
                    }
                    *(p + 1) = L'\0'; // Keep trailing backslash
                }
                else if ((std::wcslen(szFolderNameW.data()) == 2) && (szFolderNameW[1U] == L':'))
                {
                    // Drive letter (e.g. "C:") - absolute path, not relative
                    ::_snwprintf_s(szNewPathW.data(), szNewPathW.size(), _TRUNCATE, L"%s\\", szFolderNameW.data());
                }
                else if (!::_wcsicmp(szFolderNameW.data(), L"My Documents") || !::_wcsicmp(szFolderNameW.data(), L"Desktop") ||
                         !::_wcsicmp(szFolderNameW.data(), L"Network Favorites"))
                {
                    // Special folder - resolve to actual path
                    int nFolder = -1;
                    if (!::_wcsicmp(szFolderNameW.data(), L"My Documents"))
                    {
                        nFolder = CSIDL_PERSONAL;
                    }
                    else if (!::_wcsicmp(szFolderNameW.data(), L"Desktop"))
                    {
                        nFolder = CSIDL_DESKTOP;
                    }
                    else if (!::_wcsicmp(szFolderNameW.data(), L"Network Favorites"))
                    {
                        nFolder = CSIDL_NETHOOD;
                    }
                    if ((nFolder != -1) && GetSpecialFolderPathW(nFolder, szNewPathW.data()))
                    {
                        std::size_t len = std::wcslen(szNewPathW.data());
                        if ((len > 0) && (szNewPathW[len - 1] != L'\\') && (len < MAX_PATH * 4 - 1))
                        {
                            szNewPathW[len] = L'\\';
                            szNewPathW[len + 1] = L'\0';
                        }
                    }
                    else
                    {
                        return;
                    } // Can't resolve special folder
                }
                else
                {
                    // Navigate into subfolder
                    ::_snwprintf_s(szNewPathW.data(), szNewPathW.size(), _TRUNCATE, L"%s%s\\", szCurrentPathW.data(), szFolderNameW.data());
                }

                ::SetDlgItemTextW(hWndIn, IDC_CURR_LOCAL, szNewPathW.data());

                // Build Unicode search path directly - no ANSI round-trip
                ::_snwprintf_s(ofDirW.data(), ofDirW.size(), _TRUNCATE, L"%s*", szNewPathW.data());
                // Save parent path for error recovery
                ::wcscpy_s(szSavedParentPathW.data(), szSavedParentPathW.size(), szCurrentPathW.data());

                // Legacy tracking (ANSI - best effort)
                ::WideCharToMultiByte(CP_ACP, 0, szNewPathW.data(), -1, ofDir.data(), MAX_PATH, nullptr, nullptr);
                ::strcat_s(ofDir.data(), ofDir.size(), "*");
                ::WideCharToMultiByte(CP_ACP, 0, szFolderNameW.data(), -1, ofDirT.data(), static_cast<int>(ofDirT.size()), nullptr, nullptr);
                m_nLastLocalAttemptItem = nSelected;
                ::strcpy_s(m_szLastLocalAttemptName.data(), m_szLastLocalAttemptName.size(), ofDirT.data());

                // Skip the rest of the ANSI path building logic
                goto populate_folder;
            }
        }
        // If no item was selected (including empty list), treat as "no selection"
        if (nSelected == -1)
        {
            nSelected = nCount;
        }
    }
    else
    {
        // Usual shortcuts case
        if (ResolvePossibleShortcutFolder(hWndIn, szPath)) {}
        else
        {
            // General case
            szPath[6U] = '\0';
            // szPath always contains a drive letter (X:) or (..)
            ::strcpy_s(ofDirT.data(), ofDirT.size(), szPath);
            // remember which item was attempted so we can mark it on error
            LVFINDINFO Info;
            std::memset(&Info, 0, sizeof(Info));
            Info.flags = LVFI_STRING;
            Info.psz = reinterpret_cast<LPCWSTR>(ofDirT.data());
            m_nLastLocalAttemptItem = ListView_FindItem(hWndLocalList, -1, &Info);
            ::strcpy_s(m_szLastLocalAttemptName.data(), m_szLastLocalAttemptName.size(), ofDirT.data());
            // In the case of (..) we keep the current path intact
            std::array<char, 16U> szUpDirMask;
            ::sprintf_s(szUpDirMask.data(), szUpDirMask.size(), "%s..%s", rfbDirPrefix, rfbDirSuffix);
            if (std::strcmp(ofDirT.data(), szUpDirMask.data()))
            {
                ::SetDlgItemTextW(hWndIn, IDC_CURR_LOCAL, L"");
            }
        }
    }

    // Build Unicode search path directly from IDC_CURR_LOCAL
    std::array<char, MAX_PATH> szSavedParentPath{ 0 };
    bool bDirectPath{ false };
    if ((nSelected == nCount) || (lstrlenA(ofDirT.data()) == 0))
    {
        std::array<WCHAR, MAX_PATH * 4> _wb;
        ::GetDlgItemTextW(hWndIn, IDC_CURR_LOCAL, _wb.data(), static_cast<int>(_wb.size()));
        if (std::wcslen(_wb.data()) == 0)
        {
            return;
        }
        if (_wb[0U] != L'[')
        {
            // Build Unicode search path directly - no ANSI corruption
            ::_snwprintf_s(ofDirW.data(), ofDirW.size(), _TRUNCATE, L"%s*", _wb.data());
            ::wcscpy_s(szSavedParentPathW.data(), szSavedParentPathW.size(), _wb.data());
            // Legacy ANSI copies (best effort)
            ::WideCharToMultiByte(CP_ACP, 0, _wb.data(), -1, ofDirT.data(), static_cast<int>(ofDirT.size()), nullptr, nullptr);
            ::strcpy_s(szSavedParentPath.data(), szSavedParentPath.size(), ofDirT.data());
            ::strcpy_s(ofDir.data(), ofDir.size(), ofDirT.data());
            ::strcat_s(ofDir.data(), ofDir.size(), "*");
            bDirectPath = true;
        }
    }
    if (!bDirectPath)
    {
        // This code path is now bypassed by the Unicode handling above (goto populate_folder)
        // Kept for compatibility with legacy code paths
        return;
    }

    // Select the good drive in the drives combo box (the first time only)
    int nIndex = static_cast<int>(::SendDlgItemMessage(hWndIn, IDC_LOCAL_DRIVECB, CB_GETCURSEL, 0, 0L));
    if (nIndex == LB_ERR)
    {
        std::array<char, 5U> szDrive;
        ::strcpy_s(szDrive.data(), szDrive.size(), rfbDirPrefix);
        ::strncat_s(szDrive.data(), szDrive.size(), ofDir.data(), 2);
        nIndex = static_cast<int>(::SendDlgItemMessage(hWndIn, IDC_LOCAL_DRIVECB, CB_FINDSTRING, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(szDrive.data())));
        ::SendDlgItemMessage(hWndIn, IDC_LOCAL_DRIVECB, CB_SETCURSEL, static_cast<WPARAM>(nIndex), 0L);
    }

    ::SetDlgItemTextW(hWndIn, IDC_LOCAL_STATUS, sz_H8.data());

populate_folder:
    WIN32_FIND_DATA fd;
    WIN32_FIND_DATAW fdW;
    HANDLE ff;
    int bRet{ 1 };

    // ofDirW is already set in Unicode by the code paths above
    ::SetErrorMode(SEM_FAILCRITICALERRORS); // No popup please !
    ff = ::FindFirstFileW(ofDirW.data(), &fdW);
    ::SetErrorMode(0);

    // Convert Unicode result back to ANSI (system code page)
    if (ff != INVALID_HANDLE_VALUE)
    {
        std::memset(&fd, 0, sizeof(WIN32_FIND_DATA));
        fd.dwFileAttributes = fdW.dwFileAttributes;
        fd.ftCreationTime = fdW.ftCreationTime;
        fd.ftLastAccessTime = fdW.ftLastAccessTime;
        fd.ftLastWriteTime = fdW.ftLastWriteTime;
        fd.nFileSizeHigh = fdW.nFileSizeHigh;
        fd.nFileSizeLow = fdW.nFileSizeLow;
        fd.dwReserved0 = fdW.dwReserved0;
        ::wcscpy_s(fd.cFileName, MAX_PATH, fdW.cFileName);
        ::wcscpy_s(fd.cAlternateFileName, 14, fdW.cAlternateFileName);
    }

    if (ff == INVALID_HANDLE_VALUE)
    {
        ::SetDlgItemTextW(hWndIn, IDC_LOCAL_STATUS, sz_H9.data());
        if (m_nLastLocalAttemptItem >= 0)
        {
            LVITEMW ItemW;
            std::memset(&ItemW, 0, sizeof(ItemW));
            ItemW.mask = LVIF_PARAM;
            ItemW.iItem = m_nLastLocalAttemptItem;
            ItemW.iSubItem = 0;
            if (::SendMessageW(hWndLocalList, LVM_GETITEMW, 0, reinterpret_cast<LPARAM>(&ItemW)))
            {
                ItemW.lParam |= FT_LPARAM_UNREADABLE;
            }
            else
            {
                ItemW.lParam = FT_LPARAM_UNREADABLE;
            }
            ::SendMessageW(hWndLocalList, LVM_SETITEMW, 0, reinterpret_cast<LPARAM>(&ItemW));
            ListView_RedrawItems(hWndLocalList, m_nLastLocalAttemptItem, m_nLastLocalAttemptItem);
            ::UpdateWindow(hWndLocalList);
        }
        // Restore saved parent path and re-populate it using Unicode
        ::SetDlgItemTextW(hWndIn, IDC_CURR_LOCAL, szSavedParentPathW.data());
        ::_snwprintf_s(ofDirW.data(), ofDirW.size(), _TRUNCATE, L"%s*", szSavedParentPathW.data());
        ff = ::FindFirstFileW(ofDirW.data(), &fdW);
        if (ff == INVALID_HANDLE_VALUE)
        {
            return;
        } // Parent also unreadable, nothing we can do
        FTListViewClear(hWndLocalList);
        // Update m_szLastLocalPath (ANSI - best effort)
        ::WideCharToMultiByte(CP_ACP, 0, szSavedParentPathW.data(), -1, m_szLastLocalPath.data(), MAX_PATH, nullptr, nullptr);
        // Fall through to populate parent folder (fdW already valid)
    }
    else
    {
        FTListViewClear(hWndLocalList);
        // Update m_szLastLocalPath from Unicode search path (strip trailing *)
        std::array<WCHAR, MAX_PATH * 4> szPathCopyW;
        ::wcscpy_s(szPathCopyW.data(), szPathCopyW.size(), ofDirW.data());
        std::size_t len = std::wcslen(szPathCopyW.data());
        if ((len > 0) && (szPathCopyW[len - 1] == L'*'))
        {
            szPathCopyW[len - 1] = L'\0';
        }
        ::WideCharToMultiByte(CP_ACP, 0, szPathCopyW.data(), -1, m_szLastLocalPath.data(), MAX_PATH, nullptr, nullptr);
    }

    while (bRet != 0)
    {
        AddFileToFileList(hWndIn, IDC_LOCAL_FILELIST, fdW, true, fdW.cFileName);
        ++nFileCount;
        if (!PseudoYield(::GetParent(hWndIn)))
        {
            ::FindClose(ff);
            return;
        }
        bRet = ::FindNextFileW(ff, &fdW);
    }

    ::FindClose(ff);

    // Sort: folders before files, then alphabetical within each group
    ListView_SortItemsEx(hWndLocalList, FolderFirstSort, reinterpret_cast<LPARAM>(hWndLocalList));

    {
        std::array<wchar_t, 128U> _ws58;
        ::_snwprintf_s(_ws58.data(), 128, _TRUNCATE, L" > %d %s", nFileCount, sz_H58.data());
        ::SetDlgItemTextW(hWndIn, IDC_LOCAL_STATUS, _ws58.data());
    }

    // If some files have been received
    HighlightTransferedFiles(hWndRemoteList, hWndLocalList);
}



//
// Request the contents of a remote directory
//
void FileTransfer::RequestRemoteDirectoryContent(const HWND hWndIn, const LPCWSTR szPath)
{
    //	vnclog.Print(0, _T("RequestRemoteDirectoryContent\n"));

#if DEBUG_FT
    {
        std::array<wchar_t, 512U> debugMsg2;
        ::_snwprintf_s(debugMsg2.data(), 512, _TRUNCATE, L"RequestRemoteDirectoryContent: szPath=[%s] Length=%d\n", szPath, static_cast<int>(std::wcslen(szPath)));
        ::OutputDebugStringW(debugMsg2.data());
    }
#endif

    if (!m_fFTAllowed)
    {
        m_fFileCommandPending = false;
        return;
    }

    std::array<char, MAX_PATH * 3> ofDir;    // UTF-8 path to send to server (ONLY conversion point at wire boundary)
    std::array<WCHAR, MAX_PATH * 4> ofDirW;  // Unicode folder name (raw, no brackets)
    std::array<WCHAR, MAX_PATH * 4> ofDirTW; // Unicode full path (for IDC_CURR_REMOTE and processing)
    std::array<WCHAR, MAX_PATH * 4> szPathW; // Unicode copy of szPath for processing
    int nSelected = -1;
    int nCount{ 0 };
    HWND hWndRemoteList = ::GetDlgItem(hWndIn, IDC_REMOTE_FILELIST);

    ofDir[0U] = '\0';
    ofDirW[0U] = L'\0';
    ofDirTW[0U] = L'\0';
    ::wcscpy_s(szPathW.data(), szPathW.size(), szPath);

    if (std::wcslen(szPath) == 0)
    {
        nCount = ListView_GetItemCount(hWndRemoteList);
        for (nSelected = 0; nSelected < nCount; ++nSelected)
        {
            if (ListView_GetItemState(hWndRemoteList, nSelected, LVIS_SELECTED) & LVIS_SELECTED)
            {
                // Get raw Unicode filename from lParam for server path
                LVITEMW ItemLP;
                std::memset(&ItemLP, 0, sizeof(ItemLP));
                ItemLP.mask = LVIF_PARAM;
                ItemLP.iItem = nSelected;
                ItemLP.iSubItem = 0;
                ::SendMessageW(hWndRemoteList, LVM_GETITEMW, 0, reinterpret_cast<LPARAM>(&ItemLP));
                // Also get display text (e.g. "[subfolder]" or "[C:]") for ANSI display/legacy
                std::array<WCHAR, MAX_PATH> szDispW{ 0 };
                LVITEMW ItemW;
                std::memset(&ItemW, 0, sizeof(ItemW));
                ItemW.mask = LVIF_TEXT;
                ItemW.iItem = nSelected;
                ItemW.iSubItem = 0;
                ItemW.pszText = szDispW.data();
                ItemW.cchTextMax = MAX_PATH;
                ::SendMessageW(hWndRemoteList, LVM_GETITEMW, 0, reinterpret_cast<LPARAM>(&ItemW));
                // Store display text for error tracking (convert to ANSI only for legacy m_szLastRemoteAttemptName)
                std::array<char, MAX_PATH> szDispA;
                ::WideCharToMultiByte(CP_ACP, 0, szDispW.data(), -1, szDispA.data(), static_cast<int>(szDispA.size()), nullptr, nullptr);
                m_nLastRemoteAttemptItem = nSelected;
                ::strcpy_s(m_szLastRemoteAttemptName.data(), m_szLastRemoteAttemptName.size(), szDispA.data());
                ::GetDlgItemTextW(hWndIn, IDC_CURR_REMOTE, m_szLastRemotePathW.data(), static_cast<int>(m_szLastRemotePathW.size()));
                ::WideCharToMultiByte(CP_ACP, 0, m_szLastRemotePathW.data(), -1, m_szLastRemotePath.data(), static_cast<int>(m_szLastRemotePath.size()), nullptr, nullptr);

                std::wstring* pNameW = reinterpret_cast<std::wstring*>(ItemLP.lParam & ~static_cast<LPARAM>(FT_LPARAM_UNREADABLE));
                if (pNameW)
                {
                    // Regular folder: get raw name from lParam
                    ::wcscpy_s(ofDirW.data(), ofDirW.size(), pNameW->c_str());
                    // Also copy to ofDirTW with brackets for processing below
                    ::_snwprintf_s(ofDirTW.data(), MAX_PATH, _TRUNCATE, L"[ %s ]", ofDirW.data());
                }
                else
                {
                    // lParam=0: drive or shortcut item (e.g. "[C:]", "[My Documents]", "[\]", "[/]").
                    // Strip brackets from Unicode szDispW directly
                    std::size_t tlenW = std::wcslen(szDispW.data());

#if DEBUG_FT
                    {
                        std::array<wchar_t, 256U> debugRoot;
                        ::_snwprintf_s(debugRoot.data(), 256, _TRUNCATE, L"Root/Drive item: szDispW=[%s] Length=%d\n", szDispW.data(), static_cast<int>(tlenW));
                        ::OutputDebugStringW(debugRoot.data());
                    }
#endif

                    if ((tlenW >= 4) && (szDispW[0U] == L'[') && (szDispW[1U] == L' '))
                    {
                        std::array<WCHAR, MAX_PATH> szNameW;
                        std::size_t nameLenW = tlenW - 4; // Remove "[ " and " ]"
                        ::wcsncpy_s(szNameW.data(), szNameW.size(), szDispW.data() + 2, nameLenW);
                        szNameW[nameLenW] = L'\0';

                        // Handle root folder: convert "/" to "\"
                        if ((nameLenW == 1) && (szNameW[0U] == L'/'))
                        {
                            szNameW[0U] = L'\\';
                        }

                        // Only add backslash if not already present (e.g. [\] case)
                        if ((nameLenW == 0) || ((szNameW[nameLenW - 1] != L'\\') && (szNameW[nameLenW - 1] != L'/')))
                        {
                            ::wcscat_s(szNameW.data(), szNameW.size(), L"\\");
                        }

                        ::wcscpy_s(ofDirTW.data(), ofDirTW.size(), szNameW.data());
                        ::SetDlgItemTextW(hWndIn, IDC_CURR_REMOTE, szNameW.data());
                        // Convert to UTF-8 for server
                        ::WideCharToMultiByte(CP_UTF8, 0, ofDirTW.data(), -1, ofDir.data(), MAX_PATH * 3, nullptr, nullptr);

#if DEBUG_FT
                        {
                            std::array<wchar_t, 256U> debugSend;
                            ::_snwprintf_s(debugSend.data(), 256, _TRUNCATE, L"Sending root/drive: ofDirTW=[%s] ofDir=[%hs]\n", ofDirTW.data(), ofDir.data());
                            ::OutputDebugStringW(debugSend.data());
                        }
#endif

                        // Jump directly to send
                        goto send_request;
                    }
                }
                break;
            }
        }
        // If no item was selected (including empty list), treat as "no selection"
        if (nSelected == -1)
        {
            nSelected = nCount;
        }
    }
    else
    {
        // szPath provided from combo box or button - work in Unicode
        // Check if it's already a direct path (e.g., "E:\", "D:\folder\")
        if (szPathW[0U] != L'[')
        {
            // Direct path - send as-is
            ::wcscpy_s(ofDirTW.data(), ofDirTW.size(), szPathW.data());
            ::SetDlgItemTextW(hWndIn, IDC_CURR_REMOTE, ofDirTW.data());
            ::WideCharToMultiByte(CP_UTF8, 0, ofDirTW.data(), -1, ofDir.data(), MAX_PATH * 3, nullptr, nullptr);

#if DEBUG_FT
            {
                std::array<wchar_t, 256U> debugDirect;
                ::_snwprintf_s(debugDirect.data(), 256, _TRUNCATE, L"Direct path: ofDirTW=[%s] ofDir=[%hs]\n", ofDirTW.data(), ofDir.data());
                ::OutputDebugStringW(debugDirect.data());
            }
#endif
            goto send_request;
        }

        // Bracketed path from combo box (e.g., "[ D: ]", "[ .. ]")
        if (!IsShortcutFolder(szPathW.data()))
        {
            szPathW[6U] = L'\0';
        } // Truncate "[ D: ]" to keep only drive
        // szPathW always contains a drive letter (X:) or (..)
        ::wcscpy_s(ofDirTW.data(), ofDirTW.size(), szPathW.data());

#if DEBUG_FT
        {
            std::array<wchar_t, 256U> debugTrunc;
            ::_snwprintf_s(debugTrunc.data(), 256, _TRUNCATE, L"After truncation: ofDirTW=[%s] Length=%d\n", ofDirTW.data(), static_cast<int>(std::wcslen(ofDirTW.data())));
            ::OutputDebugStringW(debugTrunc.data());
        }
#endif
        // remember which item was attempted so we can mark it on error
        LVFINDINFO Info;
        std::memset(&Info, 0, sizeof(Info));
        Info.flags = LVFI_STRING;
        Info.psz = ofDirTW.data();
        m_nLastRemoteAttemptItem = ListView_FindItem(hWndRemoteList, -1, &Info);
        // Convert to ANSI only for legacy m_szLastRemoteAttemptName
        std::array<char, MAX_PATH> ofDirTA;
        ::WideCharToMultiByte(CP_ACP, 0, ofDirTW.data(), -1, ofDirTA.data(), static_cast<int>(ofDirTA.size()), nullptr, nullptr);
        ::strcpy_s(m_szLastRemoteAttemptName.data(), m_szLastRemoteAttemptName.size(), ofDirTA.data());
        // In the case of (..) we keep the current path intact
        std::array<WCHAR, 16U> szUpDirMaskW;
        ::_snwprintf_s(szUpDirMaskW.data(), 16, _TRUNCATE, L"%hs..%hs", rfbDirPrefix, rfbDirSuffix);
        // Save current path BEFORE clearing, so we can restore it if the folder is inaccessible
        ::GetDlgItemTextW(hWndIn, IDC_CURR_REMOTE, m_szLastRemotePathW.data(), static_cast<int>(m_szLastRemotePathW.size()));
        ::WideCharToMultiByte(CP_ACP, 0, m_szLastRemotePathW.data(), -1, m_szLastRemotePath.data(), static_cast<int>(m_szLastRemotePath.size()), nullptr, nullptr);
        if (std::wcscmp(ofDirTW.data(), szUpDirMaskW.data()))
        {
            ::SetDlgItemTextW(hWndIn, IDC_CURR_REMOTE, L"");
        }
    }

    bool bDirectPath{ false };
    if ((nSelected == nCount) || (std::wcslen(ofDirTW.data()) == 0))
    {
        ::GetDlgItemTextW(hWndIn, IDC_CURR_REMOTE, ofDirTW.data(), MAX_PATH);
        if (std::wcslen(ofDirTW.data()) == 0)
        {
            m_fFileCommandPending = false;
            return;
        }
        // If IDC_CURR_REMOTE already holds a full path (e.g. pasted by user), send it directly
        if ((ofDirTW[0U] != L'[') || (ofDirTW[1U] != L' '))
        {
            // Convert Unicode full path to UTF-8 for server
            ::WideCharToMultiByte(CP_UTF8, 0, ofDirTW.data(), -1, ofDir.data(), MAX_PATH * 3, nullptr, nullptr);
            bDirectPath = true;
        }
    }
    std::array<WCHAR, MAX_PATH> ofDirStrippedW{ 0 }; // Declare outside if block for later use
    if (!bDirectPath)
    {
        // Strip brackets from Unicode ofDirTW to get folder name for .. detection
        if ((ofDirTW[0U] == L'[') && (ofDirTW[1U] == L' '))
        {
            // "[ My Documents ]" -> "My Documents"
            // Skip "[ " (2 chars) and remove " ]" (2 chars) from end
            std::size_t contentLenW = std::wcslen(ofDirTW.data()) - 4; // Total - prefix - suffix
            ::wcsncpy_s(ofDirStrippedW.data(), ofDirStrippedW.size(), ofDirTW.data() + 2, contentLenW);
            ofDirStrippedW[contentLenW] = L'\0';

#if DEBUG_FT
            {
                std::array<wchar_t, 256U> debugStrip;
                ::_snwprintf_s(debugStrip.data(), 256, _TRUNCATE, L"Bracket strip: ofDirTW=[%s] ofDirStripped=[%s] len=%d\n", ofDirTW.data(), ofDirStrippedW.data(), static_cast<int>(std::wcslen(ofDirTW.data())));
                ::OutputDebugStringW(debugStrip.data());
            }
#endif
            // Also strip brackets from Unicode: use ofDirW (raw name from lParam) if set,
            // else strip from ofDirTW
            if (ofDirW[0U] == L'\0')
            {
                // fallback: strip from ofDirTW (e.g. when szPath was provided)
                // "[ D: ]" -> "D:" or "[ My Documents ]" -> "My Documents"
                std::size_t lenW = std::wcslen(ofDirTW.data());
                if (lenW > 4) // Must have at least "[ X ]"
                {
                    // Skip "[ " (2 chars) and remove " ]" (2 chars) from end
                    std::size_t contentLenWInner = lenW - 4;
                    ::wcsncpy_s(ofDirW.data(), ofDirW.size(), ofDirTW.data() + 2, contentLenWInner);
                    ofDirW[contentLenWInner] = L'\0';
                }
            }
        }
        else
        {
            m_fFileCommandPending = false;
            return;
        }

        // Get current remote path in Unicode
        ::GetDlgItemTextW(hWndIn, IDC_CURR_REMOTE, ofDirTW.data(), MAX_PATH);
        if (!::_wcsicmp(ofDirStrippedW.data(), L".."))
        {
            if (std::wcslen(ofDirTW.data()) < 1)
            {
                return;
            }
            ofDirTW[std::wcslen(ofDirTW.data()) - 1] = L'\0'; // strip trailing backslash
            WCHAR* p = std::wcsrchr(ofDirTW.data(), L'\\');
            if (p == nullptr)
            {
                m_fFileCommandPending = false;
                return;
            }
            *p = L'\0';
        }
        else
        {
            ::wcscat_s(ofDirTW.data(), ofDirTW.size(), ofDirW.data());
        }
        ::wcscat_s(ofDirTW.data(), ofDirTW.size(), L"\\");
        // Update IDC_CURR_REMOTE display using Unicode directly (avoids CP_ACP corruption)
        ::SetDlgItemTextW(hWndIn, IDC_CURR_REMOTE, ofDirTW.data());
        // Build ofDir as UTF-8 for server
        ::WideCharToMultiByte(CP_UTF8, 0, ofDirTW.data(), -1, ofDir.data(), MAX_PATH * 3, nullptr, nullptr);

#if DEBUG_FT
        {
            std::array<wchar_t, 512U> debugFinal;
            ::_snwprintf_s(debugFinal.data(), 512, _TRUNCATE, L"Final path: ofDirTW=[%s] ofDir=[%hs] ofDirW=[%s]\n", ofDirTW.data(), ofDir.data(), ofDirW.data());
            ::OutputDebugStringW(debugFinal.data());
        }
#endif
    }

    // Todo: In case of shortcuts dir, do a translation here !

    // Select the good drive in the drives combo box (the first time only)
    int nIndex = static_cast<int>(::SendDlgItemMessage(hWndIn, IDC_REMOTE_DRIVECB, CB_GETCURSEL, 0, 0L));
    if (nIndex == LB_ERR)
    {
        std::array<char, 5U> szDrive;
        ::strcpy_s(szDrive.data(), szDrive.size(), rfbDirPrefix);
        ::strncat_s(szDrive.data(), szDrive.size(), ofDir.data(), 2);
        nIndex = static_cast<int>(::SendDlgItemMessage(hWndIn, IDC_REMOTE_DRIVECB, CB_FINDSTRING, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(szDrive.data())));
        ::SendDlgItemMessage(hWndIn, IDC_REMOTE_DRIVECB, CB_SETCURSEL, static_cast<WPARAM>(nIndex), 0L);
    }

    // Don't clear list here - wait until we know folder is readable (in PopulateRemoteListBox)
    //ListView_DeleteAllItems(hWndRemoteList);

send_request:
    // Send directory content request. When Unicode server: send UTF-8 path.
    // When old server: ofDir is still ANSI (szPath came in as ANSI from drive CB).
    rfbFileTransferMsg ft;
    ft.type = rfbFileTransfer;
    ft.contentType = rfbDirContentRequest;
    ft.contentParam = m_fServerSupportsUnicode ? static_cast<CARD16>((rfbRDirContent | rfbRDirContentUnicode)) : static_cast<CARD16>(rfbRDirContent);
    ft.length = Swap32IfLE(static_cast<UINT32>(std::strlen(ofDir.data())));
    //adzm 2010-09
    m_pCC->WriteExactQueue(reinterpret_cast<char*>(&ft), sz_rfbFileTransferMsg, rfbFileTransfer);
    m_pCC->WriteExact(reinterpret_cast<char*>(ofDir.data()), static_cast<int>(std::strlen(ofDir.data())));

    return;
}


//
// Populate the remote machine listbox with files received from server
//
void FileTransfer::PopulateRemoteListBox(const HWND hWndIn, const UINT nLen, [[maybe_unused]] const bool fUnicodeEntry)
{
    //	vnclog.Print(0, _T("PopulateRemoteListBox\n"));

    // If the distant media is not browsable for some reason
    if (nLen == 0)
    {
        ::SetDlgItemTextW(hWndIn, IDC_REMOTE_STATUS, sz_H10.data());
        HWND hWndRemoteList = ::GetDlgItem(hWndIn, IDC_REMOTE_FILELIST);
        if (m_nLastRemoteAttemptItem >= 0)
        {
            LVITEMW ItemW;
            std::memset(&ItemW, 0, sizeof(ItemW));
            ItemW.mask = LVIF_PARAM;
            ItemW.iItem = m_nLastRemoteAttemptItem;
            ItemW.iSubItem = 0;
            if (::SendMessageW(hWndRemoteList, LVM_GETITEMW, 0, reinterpret_cast<LPARAM>(&ItemW)))
            {
                ItemW.lParam |= FT_LPARAM_UNREADABLE;
            }
            else
            {
                ItemW.lParam = FT_LPARAM_UNREADABLE;
            }
            ::SendMessageW(hWndRemoteList, LVM_SETITEMW, 0, reinterpret_cast<LPARAM>(&ItemW));
            ListView_RedrawItems(hWndRemoteList, m_nLastRemoteAttemptItem, m_nLastRemoteAttemptItem);
            ::UpdateWindow(hWndRemoteList);
        }
        // Restore previous path so user can navigate back
        if (m_szLastRemotePathW[0U] != L'\0')
        {
            ::SetDlgItemTextW(hWndIn, IDC_CURR_REMOTE, m_szLastRemotePathW.data());
        }
        else if (std::strlen(m_szLastRemotePath.data()) > 0)
        {
            std::array<WCHAR, MAX_PATH * 4> szLastW;
            ::MultiByteToWideChar(CP_ACP, 0, m_szLastRemotePath.data(), -1, szLastW.data(), static_cast<int>(szLastW.size()));
            ::SetDlgItemTextW(hWndIn, IDC_CURR_REMOTE, szLastW.data());
        }
        // Don't clear list - keep showing parent folder contents
        // User stays in parent folder with error message
        m_fFileCommandPending = false;
        return;
    }

    // Folder is readable, clear list and populate
    HWND hWndRemoteList = ::GetDlgItem(hWndIn, IDC_REMOTE_FILELIST);
    FTListViewClear(hWndRemoteList);

    // sf@2004 - Read the returned Directory full path
    if ((nLen > 1) && !UsingOldProtocol())
    {
        std::array<char, MAX_PATH * 3> szPath;
        if ((nLen + 1) > static_cast<int>(sizeof(szPath)))
        {
            return;
        }
        m_pCC->ReadString(szPath.data(), nLen);
        // szPath is UTF-8 (server sends back the UTF-8 path it received).
        // Decode to Unicode and store via SetDlgItemTextW so Chinese paths display correctly.
        std::array<WCHAR, MAX_PATH * 4> szPathW;
        ::MultiByteToWideChar(CP_UTF8, 0, szPath.data(), -1, szPathW.data(), MAX_PATH * 4);
        ::SetDlgItemTextW(hWndIn, IDC_CURR_REMOTE, szPathW.data());
    }

    ::SetDlgItemTextW(hWndIn, IDC_REMOTE_STATUS, sz_H11.data());

    // The dir in the current packet
    std::memset(&m_fd, '\0', sizeof(WIN32_FIND_DATA));

    m_nFileCount = 0;
    m_fDirectoryReceptionRunning = true;

    // File Transfer Backward compatibility DIRTY hack for DSMPlugin mode...
    if (UsingOldProtocol() && m_pCC->m_fUsePlugin && !m_pCC->m_fPluginStreamingIn)
    {
        m_pCC->m_nTO = 0;
        ProcessFileTransferMsg();
    }

    //ListView_DeleteAllItems(GetDlgItem(hWnd, IDC_REMOTE_FILELIST));
}


//
//
//
void FileTransfer::ReceiveDirectoryItem(const HWND hWndIn, const UINT nLen, const bool fUnicodeEntry)
{
    //	vnclog.Print(0, _T("ReceiveDirectoryItem\n"));
    if (!m_fDirectoryReceptionRunning)
    {
        return;
    }

    std::array<WCHAR, MAX_PATH * 4> szUnicodeFileName{ 0 };
    if (fUnicodeEntry)
    {
        // WIN32_FIND_DATAW is larger than m_szFileSpec; use a local buffer
        if (nLen > sizeof(WIN32_FIND_DATAW))
        {
            return;
        }
        WIN32_FIND_DATAW fdW;
        std::memset(&fdW, 0, sizeof(fdW));
        m_pCC->ReadExact(reinterpret_cast<char*>(&fdW), static_cast<int>(nLen));
        if (std::wmemchr(fdW.cFileName, L'\0', _countof(fdW.cFileName)) == nullptr)
        {
            throw ErrorException(L"Unterminated directory filename.");
        }
        std::memset(&m_fd, 0, sizeof(WIN32_FIND_DATA));
        m_fd.dwFileAttributes = fdW.dwFileAttributes;
        m_fd.ftCreationTime = fdW.ftCreationTime;
        m_fd.ftLastAccessTime = fdW.ftLastAccessTime;
        m_fd.ftLastWriteTime = fdW.ftLastWriteTime;
        m_fd.nFileSizeHigh = fdW.nFileSizeHigh;
        m_fd.nFileSizeLow = fdW.nFileSizeLow;
        m_fd.dwReserved0 = fdW.dwReserved0;
        m_fd.cFileName[0U] = '\0';
        ::wcscpy_s(szUnicodeFileName.data(), szUnicodeFileName.size(), fdW.cFileName);
    }
    else
    {
        if ((nLen >= sizeof(m_szFileSpec)) || (nLen > sizeof(m_fd)))
        {
            throw ErrorException(L"Remote directory entry exceeds size limit.");
        }
        // Read the File/Directory full info
        m_pCC->ReadString(reinterpret_cast<char*>(m_szFileSpec.data()), nLen);
        std::memset(&m_fd, '\0', sizeof(WIN32_FIND_DATA));
        std::memcpy(&m_fd, m_szFileSpec.data(), nLen);
    }
    // Pass Unicode filename directly for Unicode entries so display is correct on all Windows locales
    AddFileToFileList(hWndIn, IDC_REMOTE_FILELIST, m_fd, false, fUnicodeEntry ? szUnicodeFileName.data() : nullptr);
    ++m_nFileCount;

    // PseudoYield(pFileTransfer->hWnd);
    if (!PseudoYield(::GetParent(hWndIn)))
    {
        return;
    }

    // File Transfer Backward compatibility DIRTY hack for DSMPlugin mode...
    if (UsingOldProtocol() && m_pCC->m_fUsePlugin && !m_pCC->m_fPluginStreamingIn)
    {
        m_pCC->m_nTO = 0;
        ProcessFileTransferMsg();
    }
}


//
//
//
void FileTransfer::FinishDirectoryReception()
{
    //	vnclog.Print(0, _T("FinishDirectoryReception\n"));
    if (!m_fDirectoryReceptionRunning)
    {
        return;
    }

    HWND hWndLocalList = ::GetDlgItem(hWnd, IDC_LOCAL_FILELIST);
    HWND hWndRemoteList = ::GetDlgItem(hWnd, IDC_REMOTE_FILELIST);

    {
        std::array<wchar_t, 128U> _ws58r;
        ::_snwprintf_s(_ws58r.data(), 128, _TRUNCATE, L" > %d %s", m_nFileCount, sz_H58.data());
        ::SetDlgItemTextW(hWnd, IDC_REMOTE_STATUS, _ws58r.data());
    }

    // Sort: folders before files, then alphabetical within each group
    ListView_SortItemsEx(hWndRemoteList, FolderFirstSort, reinterpret_cast<LPARAM>(hWndRemoteList));

    // If some files have been sent to remote side highlight them
    HighlightTransferedFiles(hWndLocalList, hWndRemoteList);
    // UpdateWindow(hWnd);

    m_fDirectoryReceptionRunning = false;
    EnableButtons(hWnd);
    CheckButtonState(hWnd);
}


//
// request the list of remote drives
//
void FileTransfer::RequestRemoteDrives()
{
    //	vnclog.Print(0, _T("RequestRemoteDrives\n"));
    if (!m_fFTAllowed)
    {
        return;
    }

    // TODO : hook error !
    rfbFileTransferMsg ft;
    ft.type = rfbFileTransfer;
    ft.contentType = rfbDirContentRequest;
    ft.contentParam = rfbRDrivesList; // List of Remote Drives please
    ft.length = 0;
    m_pCC->WriteExact(reinterpret_cast<char*>(&ft), sz_rfbFileTransferMsg, rfbFileTransfer);

    return;
}


//
// Fill the Remote FilesList and remote drives combo box
//
void FileTransfer::ListRemoteDrives(const HWND hWndIn, const UINT nLen)
{
    //	vnclog.Print(0, _T("ListRemoteDrives\n"));
    std::array<char, 256U> szDrivesList; // Format when filled : "C:t<NULL>D:t<NULL>....Z:t<NULL><NULL>" (ANSI protocol)
    std::array<char, 4U> szDriveA;
    std::array<TCHAR, 4U> szDrive;
    std::array<TCHAR, 128U> szTheDrive{ 0 }; // Initialize to prevent garbage data
    std::array<TCHAR, 32U> szType;
    UINT nIndex{ 0U };

    if (nLen >= sizeof(szDrivesList))
    {
        throw ErrorException(L"Remote drive list exceeds size limit.");
    }

    m_pCC->ReadString(szDrivesList.data(), nLen);

    HWND hWndRemoteList = ::GetDlgItem(hWndIn, IDC_REMOTE_FILELIST);

    ::SendDlgItemMessage(hWndIn, IDC_REMOTE_DRIVECB, CB_RESETCONTENT, 0, 0L);
    FTListViewClear(hWndRemoteList);
    ::SetDlgItemTextW(hWndIn, IDC_CURR_REMOTE, L"");

    // Fill the tree with the remote drives
    while (ViewerBounds::range(nIndex, 4, nLen))
    {
        if (szDrivesList[nIndex + 3] != '\0')
        {
            throw ErrorException(L"Invalid remote drive record.");
        }
        std::memcpy(szDriveA.data(), szDrivesList.data() + nIndex, sizeof(szDriveA));
        ::MultiByteToWideChar(CP_ACP, 0, szDriveA.data(), -1, szDrive.data(), 4);
        nIndex += 4;

        // Get the type of drive
        switch (szDrive[2U])
        {
        case 'l':
            ::_tcscpy_s(szType.data(), 32, _T("Local Disk"));
            break;
        case 'f':
            ::_tcscpy_s(szType.data(), 32, _T("Removable"));
            break;
        case 'c':
            ::_tcscpy_s(szType.data(), 32, _T("CD-ROM"));
            break;
        case 'n':
            ::_tcscpy_s(szType.data(), 32, _T("Network"));
            break;
        default:
            ::_tcscpy_s(szType.data(), 32, _T("Unknown"));
            break;
        }

        szDrive[2U] = '\0'; // remove the type char
        _sntprintf_s(szTheDrive.data(), 128, _TRUNCATE, _T("%s%s%s"), L"[ ", szDrive.data(), L" ]");

        LVITEM Item;
        Item.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;
        Item.iItem = 0;
        Item.iSubItem = 0;
        Item.iImage = 2;
        Item.pszText = szTheDrive.data();
        Item.lParam = 0;
        const int nItem = ListView_InsertItem(hWndRemoteList, &Item);

        Item.mask = LVIF_TEXT;
        Item.iItem = nItem;
        Item.iSubItem = 1;
        Item.pszText = szType.data();
        ListView_SetItem(hWndRemoteList, &Item);

        // Prepare it for Combo Box and add it
        ::_tcscat_s(szTheDrive.data(), 128, _T(" - "));
        ::_tcscat_s(szTheDrive.data(), 128, szType.data());

#if DEBUG_FT
        {
            std::array<wchar_t, 256U> debugCombo;
            ::_snwprintf_s(debugCombo.data(), 256, _TRUNCATE, L"CB_ADDSTRING: [%s]\n", szTheDrive.data());
            ::OutputDebugStringW(debugCombo.data());
        }
#endif

        ::SendMessageW(::GetDlgItem(hWndIn, IDC_REMOTE_DRIVECB), CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(szTheDrive.data()));
    }

    // List the usual shorcuts

    if (!UsingOldProtocol())
    {
        std::array<TCHAR, 64U> szGUIDir2;

        // MyDocuments
        LVITEM Item;
        Item.mask = LVIF_TEXT | LVIF_PARAM;
        Item.iItem = 0;
        Item.iSubItem = 0;
        Item.lParam = 0;
        ::_sntprintf_s(szGUIDir2.data(), 64, _TRUNCATE, _T("%s%s%s"), L"[ ", _T("My Documents"), L" ]");
        Item.pszText = szGUIDir2.data(); // Todo: Fr/De
        ListView_InsertItem(hWndRemoteList, &Item);
        ::SendMessageW(::GetDlgItem(hWndIn, IDC_REMOTE_DRIVECB), CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(szGUIDir2.data()));

        // Desktop
        Item.mask = LVIF_TEXT | LVIF_PARAM;
        Item.iItem = 0;
        Item.iSubItem = 0;
        Item.lParam = 0;
        ::_sntprintf_s(szGUIDir2.data(), 64, _TRUNCATE, _T("%s%s%s"), L"[ ", _T("Desktop"), L" ]");
        Item.pszText = szGUIDir2.data(); // Todo: Fr/De
        ListView_InsertItem(hWndRemoteList, &Item);
        ::SendMessageW(::GetDlgItem(hWndIn, IDC_REMOTE_DRIVECB), CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(szGUIDir2.data()));

        Item.mask = LVIF_TEXT | LVIF_PARAM;
        Item.iItem = 0;
        Item.iSubItem = 0;
        Item.lParam = 0;
        ::_sntprintf_s(szGUIDir2.data(), 64, _TRUNCATE, _T("%s%s%s"), L"[ ", _T("Network Favorites"), L" ]");
        Item.pszText = szGUIDir2.data(); // Todo: Fr/De
        ListView_InsertItem(hWndRemoteList, &Item);
        ::SendMessageW(::GetDlgItem(hWndIn, IDC_REMOTE_DRIVECB), CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(szGUIDir2.data()));
    }

    ::SendMessage(::GetDlgItem(hWndIn, IDC_REMOTE_DRIVECB), CB_SETCURSEL, static_cast<WPARAM>(-1), 0);
}


//
// List local drives
//
void FileTransfer::ListDrives(const HWND hWndIn)
{
    //	vnclog.Print(0, _T("ListDrives\n"));
    std::array<TCHAR, 256U> szDrivesList{ 0 }; // Format when filled : "C:\<NULL>D:\<NULL>....Z:\<NULL><NULL>"
    std::array<TCHAR, 4U> szDrive;
    std::array<TCHAR, 32U> szTheDrive;
    std::array<TCHAR, 32U> szType;
    UINT nType{ 0U };
    DWORD dwLen;
    DWORD nIndex{ 0U };
    dwLen = ::GetLogicalDriveStrings(256, szDrivesList.data());

    HWND hWndLocalList = ::GetDlgItem(hWndIn, IDC_LOCAL_FILELIST);
    ::SendDlgItemMessage(hWndIn, IDC_LOCAL_DRIVECB, CB_RESETCONTENT, 0, 0L);

    FTListViewClear(hWndLocalList);
    ::SetDlgItemTextW(hWndIn, IDC_CURR_LOCAL, L"");

    // Parse the list of drives
    while (nIndex < dwLen - 3)
    {
        ::_tcscpy_s(szDrive.data(), 4, szDrivesList.data() + nIndex);
        nIndex += 4;
        szDrive[2U] = '\0'; // remove the '\'
        ::_sntprintf_s(szTheDrive.data(), 32, _TRUNCATE, _T("%s%s%s"), L"[ ", szDrive.data(), L" ]");

        // szName[0] = '\0';
        szType[0U] = '\0';

        ::_tcscat_s(szDrive.data(), 4, _T("\\"));

        // GetVolumeInformation(szDrive, szName, sizeof(szName), NULL, NULL, NULL, NULL, NULL);

        // Get infos on the Drive (type and Name)
        nType = ::GetDriveType(szDrive.data());
        switch (nType)
        {
        case DRIVE_FIXED:
            ::_tcscpy_s(szType.data(), 32, _T("Local Disk"));
            break;
        case DRIVE_REMOVABLE:
            ::_tcscpy_s(szType.data(), 32, _T("Removable"));
            break;
        case DRIVE_CDROM:
            ::_tcscpy_s(szType.data(), 32, _T("CD-ROM"));
            break;
        case DRIVE_REMOTE:
            ::_tcscpy_s(szType.data(), 32, _T("Network"));
            break;
        default:
            ::_tcscpy_s(szType.data(), 32, _T("Unknown"));
            break;
        }

        // Add it to the ListView - store drive letter (e.g. "C:") in lParam for PopulateLocalListBox
        LVITEMW ItemW;
        std::memset(&ItemW, 0, sizeof(ItemW));
        ItemW.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;
        ItemW.iItem = static_cast<int>(::SendMessageW(hWndLocalList, LVM_GETITEMCOUNT, 0, 0)); // Append to end
        ItemW.iSubItem = 0;
        ItemW.iImage = 2;
        ItemW.pszText = szTheDrive.data();
        std::array<TCHAR, 4U> szDriveLetter;
        ::_tcscpy_s(szDriveLetter.data(), 4, szDrivesList.data() + (nIndex - 4));
        szDriveLetter[2U] = _T('\0');
        ItemW.lParam = reinterpret_cast<LPARAM>((new std::wstring(szDriveLetter.data())));
        const int nItem = static_cast<int>(::SendMessageW(hWndLocalList, LVM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(&ItemW)));

        LVITEMW SubW;
        std::memset(&SubW, 0, sizeof(SubW));
        SubW.mask = LVIF_TEXT;
        SubW.iItem = nItem;
        SubW.iSubItem = 1;
        SubW.pszText = szType.data();
        ::SendMessageW(hWndLocalList, LVM_SETITEMW, 0, reinterpret_cast<LPARAM>(&SubW));

        // Prepare it for Combo Box and add it
        ::_tcscat_s(szTheDrive.data(), 32, _T(" - "));
        ::_tcscat_s(szTheDrive.data(), 32, szType.data());

        ::SendMessageW(::GetDlgItem(hWndIn, IDC_LOCAL_DRIVECB), CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(szTheDrive.data()));
    }

    // List the usual shorcuts

    std::array<TCHAR, 64U> szGUIDir;

    // MyDocuments - store shortcut name in lParam for PopulateLocalListBox
    LVITEMW Item;
    std::memset(&Item, 0, sizeof(Item));
    Item.mask = LVIF_TEXT | LVIF_PARAM;
    Item.iItem = static_cast<int>(::SendMessageW(hWndLocalList, LVM_GETITEMCOUNT, 0, 0)); // Append to end
    Item.iSubItem = 0;
    ::_sntprintf_s(szGUIDir.data(), 64, _TRUNCATE, _T("%s%s%s"), L"[ ", _T("My Documents"), L" ]");
    Item.pszText = szGUIDir.data(); // Todo: Fr/De
    Item.lParam = reinterpret_cast<LPARAM>((new std::wstring(L"My Documents")));
    ::SendMessageW(hWndLocalList, LVM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(&Item));
    ::SendMessageW(::GetDlgItem(hWndIn, IDC_LOCAL_DRIVECB), CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(szGUIDir.data()));

    // Desktop
    std::memset(&Item, 0, sizeof(Item));
    Item.mask = LVIF_TEXT | LVIF_PARAM;
    Item.iItem = static_cast<int>(::SendMessageW(hWndLocalList, LVM_GETITEMCOUNT, 0, 0)); // Append to end
    Item.iSubItem = 0;
    ::_sntprintf_s(szGUIDir.data(), 64, _TRUNCATE, _T("%s%s%s"), L"[ ", _T("Desktop"), L" ]");
    Item.pszText = szGUIDir.data(); // Todo: Fr/De
    Item.lParam = reinterpret_cast<LPARAM>((new std::wstring(L"Desktop")));
    ::SendMessageW(hWndLocalList, LVM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(&Item));
    ::SendMessageW(::GetDlgItem(hWndIn, IDC_LOCAL_DRIVECB), CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(szGUIDir.data()));

    // Network Favorites
    std::memset(&Item, 0, sizeof(Item));
    Item.mask = LVIF_TEXT | LVIF_PARAM;
    Item.iItem = static_cast<int>(::SendMessageW(hWndLocalList, LVM_GETITEMCOUNT, 0, 0)); // Append to end
    Item.iSubItem = 0;
    ::_sntprintf_s(szGUIDir.data(), 64, _TRUNCATE, _T("%s%s%s"), L"[ ", _T("Network Favorites"), L" ]");
    Item.pszText = szGUIDir.data(); // Todo: Fr/De
    Item.lParam = reinterpret_cast<LPARAM>((new std::wstring(L"Network Favorites")));
    ::SendMessageW(hWndLocalList, LVM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(&Item));
    ::SendMessageW(::GetDlgItem(hWndIn, IDC_LOCAL_DRIVECB), CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(szGUIDir.data()));

    ::SendMessage(::GetDlgItem(hWndIn, IDC_LOCAL_DRIVECB), CB_SETCURSEL, static_cast<WPARAM>(-1), 0);
}


//
// Set gauge max value
//
void FileTransfer::SetTotalSize(const HWND hWndIn, [[maybe_unused]] const DWORD dwTotalSize)
{
    //	vnclog.Print(0, _T("SetTotalSize\n"));
    ::SendDlgItemMessage(hWndIn, IDC_PROGRESS, PBM_SETPOS, static_cast<WPARAM>(0), static_cast<LPARAM>(0L));
    ::SendDlgItemMessage(hWndIn, IDC_PROGRESS, PBM_SETRANGE, static_cast<WPARAM>(0), MAKELPARAM(0, m_nBlockSize));
}

//
// Set gauge value
//
void FileTransfer::SetGauge(const HWND hWndIn, const __int64 dwCount)
{
    //	vnclog.Print(0, _T("SetGauge\n"));
    const DWORD dwSmallerCount = static_cast<DWORD>((dwCount / m_nBlockSize));
    DWORD dwSmallerFileSize = static_cast<DWORD>((m_nnFileSize / m_nBlockSize));
    if (dwSmallerFileSize == 0)
    {
        dwSmallerFileSize = 1;
    }
    DWORD dwValue = static_cast<DWORD>((((static_cast<__int64>((dwSmallerCount))*m_nBlockSize / dwSmallerFileSize))));
    if (dwValue != m_dwCurrentValue)
    {
        ::SendDlgItemMessage(hWndIn, IDC_PROGRESS, PBM_SETPOS, dwValue, 0);
        m_dwCurrentValue = dwValue;
    }

    DWORD dwPercent = static_cast<DWORD>(((static_cast<__int64>((dwSmallerCount)) * 100 / dwSmallerFileSize)));
    if (dwPercent != m_dwCurrentPercent)
    {
        // adzm - Include the speed and kb total
        std::array<wchar_t, 255U> szPercentAndSpeed;
        DWORD dwMsElapsed = ::GetTickCount() - m_dwStartTick;
        double dKbTotal = double(dwCount) / 1024;
        double dKbps = (dKbTotal / (double(dwMsElapsed) / 1000));
        ::_snwprintf_s(szPercentAndSpeed.data(), 255, _TRUNCATE, L"%d%% (%4.0f kb @ ~%4.0f kb/s)", dwPercent, dKbTotal, dKbps);
        ::SetDlgItemTextW(hWndIn, IDC_PERCENT, szPercentAndSpeed.data());
        m_dwCurrentPercent = dwPercent;
    }
}


//
// Display global progress (ratio files transfered/Total Files To transfer)
//
void FileTransfer::SetGlobalCount()
{
    //	vnclog.Print(0, _T("SetGlobalCount\n"));
    std::array<wchar_t, 64U> szGlobal;
    ::_snwprintf_s(szGlobal.data(), 64, _TRUNCATE, L"File %d/%d", m_nFilesTransfered, m_nFilesToTransfer);
    ::SetDlgItemTextW(hWnd, IDC_GLOBAL_STATUS, szGlobal.data());
}


//
// Display current status and add it to the history combo box
//
void FileTransfer::SetStatus(const LPWSTR szStatus)
{
    if (std::wcslen(szStatus) > (512 + 256 - 1))
    {
        szStatus[768U] = L'\0';
    }

    //	vnclog.Print(0, _T("SetStatus\n"));
    // time_t lTime;
    std::array<char, 9U> dbuffer;
    std::array<char, 9U> tbuffer;

    std::array<wchar_t, 800U> szHist;

    ::SetDlgItemTextW(hWnd, IDC_STATUS, szStatus);
    _tzset();
    // time(&lTime);
    ::_strdate_s(dbuffer.data(), dbuffer.size());
    ::_strtime_s(tbuffer.data(), tbuffer.size());
    ::_snwprintf_s(szHist.data(), 800, _TRUNCATE, L" > %hs %hs - %s", dbuffer.data(), tbuffer.data() /*ctime(&lTime)*/, szStatus);
    {
        COMBOBOXINFO cbi;
        cbi.cbSize = sizeof cbi;

        ::GetComboBoxInfo(::GetDlgItem(hWnd, IDC_HISTORY_CB), &cbi);
        HDC hdc = ::GetDC(cbi.hwndList);
        RECT rc{ 0, 0, 0, 0 };
        ::DrawTextW(hdc, szHist.data(), -1, &rc, DT_CALCRECT | DT_SINGLELINE);
        ::ReleaseDC(cbi.hwndList, hdc);
        int dx = rc.right - rc.left;
        m_maxHistExtent = maximum(m_maxHistExtent, dx);
        ::SendDlgItemMessage(hWnd, IDC_HISTORY_CB, CB_SETHORIZONTALEXTENT, static_cast<WPARAM>(m_maxHistExtent), 0L);
    }
    LRESULT Index = ::SendMessageW(::GetDlgItem(hWnd, IDC_HISTORY_CB), CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(szHist.data()));
    ::SendMessageW(::GetDlgItem(hWnd, IDC_HISTORY_CB), CB_SETCURSEL, static_cast<WPARAM>(Index), static_cast<LPARAM>(0));
}


//
// Request a file
//
void FileTransfer::RequestRemoteFile(const LPSTR szRemoteFileName)
{

    //	vnclog.Print(0, _T("RequestRemoteFile\n"));
    if (!m_fFTAllowed)
    {
        return;
    }

#if DEBUG_FT
    {
        std::array<wchar_t, 2048U> debugMsg;
        ::MultiByteToWideChar(CP_UTF8, 0, szRemoteFileName, -1, debugMsg.data(), 2048);
        ::OutputDebugStringW(L"=== FT_DOWNLOAD: RequestRemoteFile ===\n");
        ::OutputDebugStringW(L"  Requesting file: ");
        ::OutputDebugStringW(debugMsg.data());
        std::array<wchar_t, 64U> debugProto;
        ::_snwprintf_s(debugProto.data(), 64, _TRUNCATE, L" (UTF-8=%s)\n", m_fServerSupportsUnicode ? L"Y" : L"N");
        ::OutputDebugStringW(debugProto.data());
    }
#endif

    // Ensure Backward File Transfer compatibility (Directory reception)....
    if (UsingOldProtocol())
    {
        char* p1 = strrchr(szRemoteFileName, '\\') + 1;
        char* p2 = strrchr(szRemoteFileName, rfbDirSuffix[0U]);
        if ((p1[0U] == rfbDirPrefix[0U]) && (p1[1U] == rfbDirPrefix[1U]) &&                               // Check dir prefix
            (p2[0U] == rfbDirSuffix[0U]) && (p2[1U] == rfbDirSuffix[1U]) && (p2 != nullptr) && (p1 < p2)) // Check dir suffix
        {
            // p1 = strrchr(szRemoteFileName, '\\') + 1;
            std::array<char, MAX_PATH> szDirectoryName;
            ::strcpy_s(szDirectoryName.data(), szDirectoryName.size(), p1 + 2);                        // Skip dir prefix (2 chars)
            szDirectoryName[std::strlen(szDirectoryName.data()) - 2] = '\0'; // Remove dir suffix (2 chars)
            *p1 = '\0';
            ::strcat_s(szRemoteFileName, MAX_PATH, "("), ::strcat_s(szRemoteFileName, MAX_PATH, szDirectoryName.data());
            ::strcat_s(szRemoteFileName, MAX_PATH, ")");
        }
    }

    // TODO : hook error !
    rfbFileTransferMsg ft;
    ft.type = rfbFileTransfer;
    ft.contentType = rfbFileTransferRequest;
    ft.contentParam = 0;

    // szRemoteFileName is already UTF-8 encoded (callers use Unicode->UTF-8 conversion)
    // For old servers without Unicode support, convert UTF-8 back to CP_ACP
    std::array<char, MAX_PATH * 3> szFileNameToSend;
    if (m_fServerSupportsUnicode)
    {
        // Already UTF-8, send as-is
        ::strcpy_s(szFileNameToSend.data(), szFileNameToSend.size(), szRemoteFileName);
    }
    else
    {
        // Old server: convert UTF-8 -> Unicode -> CP_ACP
        std::array<WCHAR, MAX_PATH * 4> szFileNameW;
        ::MultiByteToWideChar(CP_UTF8, 0, szRemoteFileName, -1, szFileNameW.data(), MAX_PATH * 4);
        ::WideCharToMultiByte(CP_ACP, 0, szFileNameW.data(), -1, szFileNameToSend.data(), static_cast<int>(szFileNameToSend.size()), nullptr, nullptr);
    }

    ft.length = Swap32IfLE(std::strlen(szFileNameToSend.data()));
    ft.size = (m_pCC->kbitsPerSecond > 2048) ? Swap32IfLE(0) : Swap32IfLE(1); // 1 means "Enable compression"
                                                                              //adzm 2010-09
    m_pCC->WriteExactQueue(reinterpret_cast<char*>(&ft), sz_rfbFileTransferMsg, rfbFileTransfer);
    m_pCC->WriteExact(reinterpret_cast<char*>(szFileNameToSend.data()), static_cast<int>(std::strlen(szFileNameToSend.data())));
    ::strncpy_s(szRemoteFileNameRequested.data(), sizeof(szRemoteFileNameRequested), szFileNameToSend.data(), _TRUNCATE);
    rfbFileHeaderRequested = true;
    return;
}

//
// Receive a file
//
bool FileTransfer::ReceiveFile(unsigned long lSize, const UINT nLen)
{
    //	vnclog.Print(0, _T("ReceiveFile\n"));
    if (!m_fFTAllowed)
    {
        return false;
    }

#if DEBUG_FT
    ::OutputDebugStringW(L"=== FT_DOWNLOAD: ReceiveFile START ===\n");
    {
        std::array<wchar_t, 128U> debugMsg;
        ::_snwprintf_s(debugMsg.data(), 128, _TRUNCATE, L"  nLen=%d, lSize=%lu\n", nLen, lSize);
        ::OutputDebugStringW(debugMsg.data());
    }
#endif

    rfbFileTransferMsg ft;
    ft.type = rfbFileTransfer;
    ft.contentType = rfbFileHeader;
    ft.size = Swap32IfLE(0);
    ft.length = Swap32IfLE(0);

    if (!ViewerBounds::payload(nLen))
    {
        throw ErrorException(L"Invalid remote filename length.");
    }
    char* szRemoteFileName = new char[nLen + 1];
    if (szRemoteFileName == nullptr)
    {
        return false;
    }
    std::memset(szRemoteFileName, 0, nLen + 1);

    // Read in the Name of the file to copy (remote full name !)
    m_pCC->ReadExact(szRemoteFileName, static_cast<int>(nLen));

    szRemoteFileName[nLen] = '\0';

    // sf@2004 - The file size can be wrong for huge files (>4Gb)
    // idealy we should pass another param (sizeH) in the rfbFileTransfer msg (same thing
    // for the Date/Time) but we want to maintain backward compatibility between all Ultra V1 RC version..
    // So instead we pass the additionnal High size param after the received string...of the current msg
    // Parse the FileTime and isolate filename
    CARD32 sizeH = 0;
    if (!UsingOldProtocol())
    {
        CARD32 sizeHtmp;
        m_pCC->ReadExact(reinterpret_cast<char*>(&sizeHtmp), sizeof(CARD32));
        sizeH = Swap32IfLE(sizeHtmp);
    }
    // 5/2/2008 moved jdp so that the entire packet is read
    // If lSize = -1 (0xFFFFFFFF) that means that the Src file on the remote machine
    // could not be opened for some reason (locked, doesn't exits any more...)
    if ((UsingOldProtocol() && (lSize == 0xFFFFFFFFu)) || (!UsingOldProtocol() && (lSize == 0xFFFFFFFFu) && (sizeH == 0xFFFFFFFFu)))
    {
        {
            std::array<wchar_t, MAX_PATH * 4> _wrf;
            ::MultiByteToWideChar(CP_UTF8, 0, szRemoteFileName, -1, _wrf.data(), static_cast<int>(_wrf.size()));
            std::array<wchar_t, 512U> szStatusW;
            ::_snwprintf_s(szStatusW.data(), 512, _TRUNCATE, L" %s < %s > %s", sz_H12.data(), _wrf.data(), sz_H13.data());
            SetStatus(szStatusW.data());
        }
        delete[] szRemoteFileName;
        m_fFileDownloadError = true;
        return false;
    }

    // Get the current path (destination path) in Unicode
    std::array<WCHAR, MAX_PATH * 4> szDestDirW;
    ::GetDlgItemTextW(hWnd, IDC_CURR_LOCAL, szDestDirW.data(), static_cast<int>(szDestDirW.size()));
    // Also store as CP_ACP for legacy uses below
    ::WideCharToMultiByte(CP_ACP, 0, szDestDirW.data(), -1, m_szDestFileName.data(), sizeof(m_szDestFileName), nullptr, nullptr);

    char* p = strrchr(szRemoteFileName, ',');
    if (p == nullptr)
    {
        m_szIncomingFileTime[0U] = '\0';
    }
    else
    {
        ::strcpy_s(m_szIncomingFileTime.data(), m_szIncomingFileTime.size(), p + 1);
        *p = '\0';
    }

    std::array<char, MAX_PATH * 4> displayName;
    {
        const char* pSlash = strrchr(szRemoteFileName, '\\');
        ::sprintf_s(displayName.data(), displayName.size(), "%s%s", m_szDestFileName.data(), pSlash ? pSlash + 1 : szRemoteFileName);
    }
    // Check the free space on local destination drive
    bool fErr{ false };
    ULARGE_INTEGER lpFreeBytesAvailable{ 0, 0 };
    ULARGE_INTEGER lpTotalBytes;
    ULARGE_INTEGER lpTotalFreeBytes;
    unsigned long dwFreeKBytes{ 0U };
    //security requested filename must be the received filename
    if (std::strcmp(szRemoteFileName, szRemoteFileNameRequested.data()) != 0)
    {
        // Only a "[ name ]" directory request may be answered with a
        // differently-named reply: the !UVNCDIR-...zip built for that
        // directory. Anything else is rejected.
        if (!IsExpectedDirZipReply(szRemoteFileName, szRemoteFileNameRequested.data(), m_fServerSupportsUnicode, UsingOldProtocol()))
        {
            delete[] szRemoteFileName;
            m_fFileDownloadError = true;
            return false;
        }
    }

    // Check free space using Unicode API on the destination drive root
    {
        std::array<WCHAR, 8U> szDriveW{ 0 };
        if (szDestDirW[0U] && (szDestDirW[1U] == L':'))
        {
            szDriveW[0U] = szDestDirW[0U];
            szDriveW[1U] = L':';
            szDriveW[2U] = L'\\';
            szDriveW[3U] = L'\0';
        }
        else
        {
            ::wcscpy_s(szDriveW.data(), szDriveW.size(), szDestDirW.data());
        }
        if (!::GetDiskFreeSpaceExW(szDriveW.data(), &lpFreeBytesAvailable, &lpTotalBytes, &lpTotalFreeBytes))
        {
            fErr = true;
        }
    }
    dwFreeKBytes = static_cast<unsigned long>((Int64ShraMod32(lpFreeBytesAvailable.QuadPart, 10)));
    if (dwFreeKBytes < static_cast<unsigned long>((lSize / 1000)))
    {
        fErr = true;
    }

    bool fErrNoFileName{ false };
    char* plbs = strrchr(szRemoteFileName, '\\');
    if (plbs == nullptr)
    {
        fErrNoFileName = true;
        fErr = true;
    }
    else if (plbs[1U] == '\0')
    {
        fErrNoFileName = true;
        fErr = true;
    }

    if (fErr)
    {
        std::array<wchar_t, 512U> szStatusW4;
        if (!fErrNoFileName)
        {
            std::array<wchar_t, MAX_PATH * 4> _wdn;
            ::MultiByteToWideChar(CP_UTF8, 0, displayName.data(), -1, _wdn.data(), static_cast<int>(_wdn.size()));
            std::array<wchar_t, 512U> szStatusW4Inner;
            ::_snwprintf_s(szStatusW4Inner.data(), 512, _TRUNCATE, L" %s < %s >", sz_H14.data(), _wdn.data());
            SetStatus(szStatusW4Inner.data());
        }
        else
        {
            ::_snwprintf_s(szStatusW4.data(), 512, _TRUNCATE, L" %s < Invalid remote file name >", sz_H14.data());
        }

        delete[] szRemoteFileName;
        // Tell the server to cancel the transfer
        ft.size = static_cast<unsigned int>(Swap32IfLE(-1));
        if (UsingOldProtocol())
        {
            m_pCC->WriteExact(reinterpret_cast<char*>(&ft), sz_rfbFileTransferMsg);
        }
        else
        {
            m_pCC->WriteExact(reinterpret_cast<char*>(&ft), sz_rfbFileTransferMsg, rfbFileTransfer);
        }
        m_fFileDownloadError = true;
        return false;
    }


    // Build destination filename: decode UTF-8 remote filename to Unicode, append to local dir
    {
        // Decode the UTF-8 remote filename to Unicode
        std::array<WCHAR, MAX_PATH * 4> szRemoteFileNameW;
        ::MultiByteToWideChar(CP_UTF8, 0, szRemoteFileName, -1, szRemoteFileNameW.data(), static_cast<int>(szRemoteFileNameW.size()));
        // Get just the filename part (after last backslash)
        const WCHAR* pFilePartW = std::wcsrchr(szRemoteFileNameW.data(), L'\\');
        if (pFilePartW)
        {
            ++pFilePartW;
        }
        else
        {
            pFilePartW = szRemoteFileNameW.data();
        }
        // Apply temp filename logic on the Unicode filename
        std::array<char, MAX_PATH * 3> szFilePartUTF8;
        ::WideCharToMultiByte(CP_UTF8, 0, pFilePartW, -1, szFilePartUTF8.data(), static_cast<int>(szFilePartUTF8.size()), nullptr, nullptr);
        std::string tempName = make_temp_filename(szFilePartUTF8.data());
        std::array<WCHAR, MAX_PATH * 4> szTempNameW;
        ::MultiByteToWideChar(CP_UTF8, 0, tempName.c_str(), -1, szTempNameW.data(), static_cast<int>(szTempNameW.size()));
        // Build full Unicode dest path and store as member for FinishFileReception
        ::wcscpy_s(m_szDestFileNameW.data(), m_szDestFileNameW.size(), szDestDirW.data());
        ::wcscat_s(m_szDestFileNameW.data(), m_szDestFileNameW.size(), szTempNameW.data());
        // Also store CP_ACP version for legacy code that reads m_szDestFileName
        ::WideCharToMultiByte(CP_ACP, 0, m_szDestFileNameW.data(), -1, m_szDestFileName.data(), sizeof(m_szDestFileName), nullptr, nullptr);
        m_nnFileSize = ((static_cast<__int64>((sizeH))) << 32) + lSize;
        std::array<char, 96U> szFFS;
        GetFriendlyFileSizeString(m_nnFileSize, szFFS.data(), 96);
        {
            std::array<wchar_t, MAX_PATH * 4> _wdn2;
            ::MultiByteToWideChar(CP_UTF8, 0, displayName.data(), -1, _wdn2.data(), static_cast<int>(_wdn2.size()));
            std::array<wchar_t, 96U> _wffs;
            ::MultiByteToWideChar(CP_ACP, 0, szFFS.data(), -1, _wffs.data(), 96);
            std::array<wchar_t, 512U> szStatusW12;
            ::_snwprintf_s(szStatusW12.data(), 512, _TRUNCATE, L" %s < %s > (%s) <<<", sz_H15.data(), _wdn2.data(), _wffs.data());
            SetStatus(szStatusW12.data());
        }
        SetTotalSize(hWnd, lSize);
        SetGauge(hWnd, 0);
        ::UpdateWindow(hWnd);
// DEBUG: Show destination path
#if DEBUG_FT
        ::OutputDebugStringW(L"  Creating dest file: ");
        ::OutputDebugStringW(m_szDestFileNameW.data());
        ::OutputDebugStringW(L"\n");
#endif

        // Create the local Destination file using Unicode path with \\?\ prefix for long path support
        std::array<WCHAR, MAX_PATH * 4> szDestLongW;
        if (m_szDestFileNameW[0U] && (m_szDestFileNameW[1U] == L':'))
        {
            ::_snwprintf_s(szDestLongW.data(), szDestLongW.size(), _TRUNCATE, L"\\\\?\\%s", m_szDestFileNameW.data());
        }
        else
        {
            ::wcscpy_s(szDestLongW.data(), szDestLongW.size(), m_szDestFileNameW.data());
        }
        m_hDestFile = ::CreateFileW(szDestLongW.data(), GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                  OPEN_ALWAYS, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    }

    // sf@2004 - Delta Transfer
    bool fAlreadyExists = (::GetLastError() == ERROR_ALREADY_EXISTS);

    if (m_hDestFile == INVALID_HANDLE_VALUE)
    {
        {
            std::array<wchar_t, MAX_PATH * 4> _wdn3;
            ::MultiByteToWideChar(CP_UTF8, 0, displayName.data(), -1, _wdn3.data(), static_cast<int>(_wdn3.size()));
            std::array<wchar_t, 512U> szStatusW3;
            ::_snwprintf_s(szStatusW3.data(), 512, _TRUNCATE, L" %s < %s > %s", sz_H12.data(), _wdn3.data(), sz_H16.data());
            SetStatus(szStatusW3.data());
        }
#if DEBUG_FT
        ::OutputDebugStringW(L"  ERROR: CreateFileW failed for destination file\n");
#endif
        ::CloseHandle(m_hDestFile);
        delete[] szRemoteFileName;
        m_fFileDownloadError = true;

        // Tell the server to cancel the transfer
        ft.size = static_cast<unsigned int>(Swap32IfLE(-1));
        if (UsingOldProtocol())
        {
            m_pCC->WriteExact(reinterpret_cast<char*>(&ft), sz_rfbFileTransferMsg);
        }
        else
        {
            m_pCC->WriteExact(reinterpret_cast<char*>(&ft), sz_rfbFileTransferMsg, rfbFileTransfer);
        }
        return false;
    }

    delete[] szRemoteFileName;

    m_pCC->CheckFileChunkBufferSize(m_nBlockSize + 1024);

    // sf@2004 - Delta Transfer
    if (fAlreadyExists && !UsingOldProtocol())
    {
        // DWORD dwFileSize = GetFileSize(m_hDestFile, NULL);
        ULARGE_INTEGER n2FileSize;
        bool bSizeOk = MyGetFileSize(m_szDestFileNameW.data(), &n2FileSize);
        // if (dwFileSize != 0xFFFFFFFF)
        if (bSizeOk)
        {
            unsigned long long nCSBufferSize = (4 * static_cast<unsigned long long>((n2FileSize.QuadPart / m_nBlockSize))) + 1024;
            if (nCSBufferSize > std::numeric_limits<std::size_t>::max())
            {
                return false;
            }

            std::unique_ptr<char[]> lpCSBuff = std::make_unique<char[]>(static_cast<std::size_t>(nCSBufferSize));
            if (lpCSBuff)
            {
                int nCSBufferLen = GenerateFileChecksums(m_hDestFile, lpCSBuff.get(), static_cast<int>(nCSBufferSize));
                if (nCSBufferLen != -1)
                {
                    //sprintf_s(szStatus, " Sending %d bytes of file checksums to remote machine. Please Wait...", nCSBufferSize);
                    //SetStatus(szStatus);

                    rfbFileTransferMsg ftm;
                    ftm.type = rfbFileTransfer;
                    ftm.contentType = rfbFileChecksums;
                    ftm.size = Swap32IfLE(nCSBufferSize);
                    ftm.length = static_cast<unsigned int>(Swap32IfLE(nCSBufferLen));
                    //adzm 2010-09
                    m_pCC->WriteExactQueue(reinterpret_cast<char*>(&ftm), sz_rfbFileTransferMsg, rfbFileTransfer);
                    m_pCC->WriteExactQueue(reinterpret_cast<char*>(lpCSBuff.get()), nCSBufferLen);
                }
            }
        }
    }

    // Tell the server that the transfer can start
    ft.size = Swap32IfLE(lSize);
    if (UsingOldProtocol())
    {
        m_pCC->WriteExact(reinterpret_cast<char*>(&ft), sz_rfbFileTransferMsg);
    }
    else
    {
        m_pCC->WriteExact(reinterpret_cast<char*>(&ft), sz_rfbFileTransferMsg, rfbFileTransfer);
    }

    // DWORD dwNbPackets = (DWORD)(lSize / m_nBlockSize);
    m_dwNbReceivedPackets = 0;
    m_dwNbBytesWritten = 0;
    m_dwTotalNbBytesWritten = 0;
    m_dwTotalNbBytesNotReallyWritten = 0;
    m_nPacketCount = 0;
    m_fPacketCompressed = true;

    m_fFileDownloadError = false;
    m_fFileDownloadRunning = true;

#if DEBUG_FT
    ::OutputDebugStringW(L"  Dest file opened successfully, download started\n");
    {
        std::array<wchar_t, 256U> debugMsg;
        ::_snwprintf_s(debugMsg.data(), 256, _TRUNCATE, L"  FileSize=%I64d, fAlreadyExists=%d\n", m_nnFileSize, fAlreadyExists);
        ::OutputDebugStringW(debugMsg.data());
    }
#endif

    m_dwStartTick = ::GetTickCount();

    // File Transfer Backward compatibility DIRTY hack for DSMPlugin mode...
    if (UsingOldProtocol() && m_pCC->m_fUsePlugin && !m_pCC->m_fPluginStreamingIn)
    {
        m_pCC->m_nTO = 0;
        ProcessFileTransferMsg();
    }

    return true;
}


//
// Receive incoming file chunk
//
bool FileTransfer::ReceiveFileChunk(const UINT nLen, const int nSize)
{
    //	vnclog.Print(0, _T("ReceiveFileChunk\n"));
    if (!m_fFileDownloadRunning)
    {
        return false;
    }

    static DWORD s_debugLastReport{ 0U };
    static __int64 s_debugLastBytes{ 0 };

    if (m_fFileDownloadError)
    {
#if DEBUG_FT
        ::OutputDebugStringW(L"  ReceiveFileChunk: m_fFileDownloadError=TRUE, calling FinishFileReception\n");
#endif
        FinishFileReception();
    }

    // DEBUG: Report progress every 1 second
    if (::GetTickCount() - s_debugLastReport > 1000)
    {
        DWORD dwElapsed = ::GetTickCount() - s_debugLastReport;
        __int64 bytesRecv = m_dwTotalNbBytesWritten - s_debugLastBytes;
        DWORD kbps = dwElapsed > 0 ? static_cast<DWORD>(((bytesRecv * 8 / 1024) * 1000 / dwElapsed)) : 0;
#if DEBUG_FT
        {
            std::array<wchar_t, 256U> debugMsg;
            ::_snwprintf_s(debugMsg.data(), 256, _TRUNCATE, L"  ReceiveFileChunk: TotalBytes=%I64d, Speed=%lu Kbps\n", m_dwTotalNbBytesWritten, kbps);
            ::OutputDebugStringW(debugMsg.data());
        }
#endif
        s_debugLastReport = ::GetTickCount();
        s_debugLastBytes = m_dwTotalNbBytesWritten;
    }

    BOOL fRes{ true };
    bool fAlreadyHere = (nSize == 2);
    m_fPacketCompressed = true; // sf@2005 - This missing line was causing RC19 file reception bug...

    if (nLen > m_pCC->m_filechunkbufsize)
    {
        return false;
    }

    // sf@2004 - Delta Transfer - Empty packet
    if (fAlreadyHere)
    {
        DWORD dwPtr = ::SetFilePointer(m_hDestFile, static_cast<LONG>(nLen), nullptr, FILE_CURRENT);
        if (dwPtr == 0xFFFFFFFF)
        {
            fRes = false;
        }
    }
    else
    {
        m_pCC->ReadExact(reinterpret_cast<char*>(m_pCC->m_filechunkbuf), static_cast<int>(nLen));

        if (nSize == 0)
        {
            m_fPacketCompressed = false;
        }

        unsigned int nRawBytes = m_nBlockSize + 1024;

        if (m_fPacketCompressed)
        {
            // Decompress incoming data
            m_pCC->CheckFileZipBufferSize(static_cast<int>(nRawBytes));
            int nRetU = uncompress(reinterpret_cast<unsigned char*>(m_pCC->m_filezipbuf),   // Dest
                                   reinterpret_cast<unsigned long*>(&nRawBytes),            // Dest len
                                   reinterpret_cast<unsigned char*>(m_pCC->m_filechunkbuf), // Src
                                   nLen                                   // Src len
            );

            if (nRetU != 0)
            {
                // vnclog.Print(0, _T("uncompress error in ReceiveFile: %d\n"), nRet);
                m_fFileDownloadError = true;
                // break;
            }
            ::Sleep(5);
        }

        fRes = ::WriteFile(m_hDestFile, m_fPacketCompressed ? m_pCC->m_filezipbuf : m_pCC->m_filechunkbuf,
                         m_fPacketCompressed ? nRawBytes : nLen, &m_dwNbBytesWritten, nullptr);
    }

    if (!fRes)
    {
        // TODO: Ask the server to stop the transfer
        m_fFileDownloadError = true;
    }

    m_dwTotalNbBytesWritten += (fAlreadyHere ? nLen : m_dwNbBytesWritten);
    m_dwTotalNbBytesNotReallyWritten += (fAlreadyHere ? nLen : 0);
    ++m_dwNbReceivedPackets;

    // Refresh of the progress bar
    SetGauge(hWnd, m_dwTotalNbBytesWritten);
    PseudoYield(::GetParent(hWnd));

    // We still support the *dirty* old "Abort" method (for backward compatibility with UltraVNC Server <= RC18)
    if (UsingOldProtocol())
    {
        // Every 10 packets, test if the transfer must be stopped
        ++m_nPacketCount;
        if (m_nPacketCount > 10)
        {
            m_fAborted = true;
            rfbFileTransferMsg ft;
            ft.type = rfbFileTransfer;
            ft.contentType = rfbFilePacket;
            ft.contentParam = 0;
            ft.length = 0;

            if (m_fAbort || m_fFileDownloadError)
            {
                // Ask the server to stop the transfer
                ft.size = static_cast<unsigned int>(Swap32IfLE(-1));
                m_pCC->WriteExact(reinterpret_cast<char*>(&ft), sz_rfbFileTransferMsg /*, rfbFileTransfer*/);
            }
            else
            {
                // Tell the server to continue the transfer
                ft.size = Swap32IfLE(0);
                m_pCC->WriteExact(reinterpret_cast<char*>(&ft), sz_rfbFileTransferMsg /*, rfbFileTransfer*/);
            }
            m_nPacketCount = 0;
        }
    }
    else // New v2 File Transfer Protocol
    {
        // Now abort the File Transfer if required by the user
        if (m_fAbort && !m_fAborted)
        {
            m_fAborted = true;
            rfbFileTransferMsg ft;
            ft.type = rfbFileTransfer;
            ft.contentType = rfbAbortFileTransfer;
            ft.contentParam = rfbFileTransferVersion;
            ft.length = 0;
            ft.size = 0;
            m_pCC->WriteExact(reinterpret_cast<char*>(&ft), sz_rfbFileTransferMsg, rfbFileTransfer);
        }
    }

    // File Transfer Backward compatibility DIRTY hack for DSMPlugin mode...
    if (UsingOldProtocol() && m_pCC->m_fUsePlugin && !m_pCC->m_fPluginStreamingIn)
    {
        m_pCC->m_nTO = 0;
        ProcessFileTransferMsg();
    }
    return true;
}


//
// Finish File Download
//
bool FileTransfer::FinishFileReception()
{
    //	vnclog.Print(0, _T("FinishFileReception\n"));
    if (!m_fFileDownloadRunning)
    {
        return false;
    }

#if DEBUG_FT
    ::OutputDebugStringW(L"=== FT_DOWNLOAD: FinishFileReception ===\n");
    {
        std::array<wchar_t, 256U> debugMsg;
        ::_snwprintf_s(debugMsg.data(), 256, _TRUNCATE, L"  m_fFileDownloadError=%d, TotalBytesWritten=%I64d\n", m_fFileDownloadError, m_dwTotalNbBytesWritten);
        ::OutputDebugStringW(debugMsg.data());
    }
#endif

    m_fFileDownloadRunning = false;
    m_pCC->SetRecvTimeout(0);
    // adzm 2010-09
    m_pCC->SendKeepAlive(false, true);

    // sf@2004 - Delta transfer
    ::SetEndOfFile(m_hDestFile);

    // TODO : check dwNbReceivedPackets and dwTotalNbBytesWritten or test a checksum
    ::FlushFileBuffers(m_hDestFile);

    std::string realName = get_real_filename(m_szDestFileName.data());

    std::array<wchar_t, 512 + 256> szStatusW4;
    {
        std::array<wchar_t, MAX_PATH * 4> _wrn;
        ::MultiByteToWideChar(CP_UTF8, 0, realName.c_str(), -1, _wrn.data(), static_cast<int>(_wrn.size()));
        std::array<wchar_t, 512U> szStatusW4Inner;
        ::_snwprintf_s(szStatusW4Inner.data(), 512, _TRUNCATE, L" %s < %s >", sz_H17.data(), _wrn.data());
        SetStatus(szStatusW4Inner.data());
    }
    // Set the DestFile Time Stamp
    if (std::strlen(m_szIncomingFileTime.data()))
    {
        FILETIME DestFileTime;
        SYSTEMTIME FileTime;
        FileTime.wMonth = static_cast<WORD>(atoi(m_szIncomingFileTime.data()));
        FileTime.wDay = static_cast<WORD>(atoi(m_szIncomingFileTime.data() + 3));
        FileTime.wYear = static_cast<WORD>(atoi(m_szIncomingFileTime.data() + 6));
        FileTime.wHour = static_cast<WORD>(atoi(m_szIncomingFileTime.data() + 11));
        FileTime.wMinute = static_cast<WORD>(atoi(m_szIncomingFileTime.data() + 14));
        FileTime.wMilliseconds = 0;
        FileTime.wSecond = 0;
        ::SystemTimeToFileTime(&FileTime, &DestFileTime);
        // ToDo: hook error
        ::SetFileTime(m_hDestFile, &DestFileTime, &DestFileTime, &DestFileTime);
    }

    ::CloseHandle(m_hDestFile);

    // sf@2004 - Delta Transfer - Now we can keep the existing file data :)
    if (m_fFileDownloadError && (UsingOldProtocol() || m_fUserAbortedFileTransfer))
    {
        std::array<WCHAR, MAX_PATH * 4> _dlp;
        if (m_szDestFileNameW[0U] && (m_szDestFileNameW[1U] == L':'))
        {
            ::_snwprintf_s(_dlp.data(), _dlp.size(), _TRUNCATE, L"\\\\?\\%s", m_szDestFileNameW.data());
        }
        else
        {
            ::wcscpy_s(_dlp.data(), _dlp.size(), m_szDestFileNameW.data());
        }
        ::DeleteFileW(_dlp.data());
    }

    // sf@2003 - Directory Transfer trick
    // If the file is an Ultra Directory Zip we unzip it here and we delete the
    // received file
    // Todo: make a better free space check (above) in this particular case. The free space must be at least
    // 3 times the size of the directory zip file (this zip file is ~50% of the real directory size)

    // hide the stop button
    ::ShowWindow(::GetDlgItem(hWnd, IDC_ABORT_B), SW_HIDE);
    ::ShowWindow(::GetDlgItem(hWnd, IDC_ABORT_B2), SW_HIDE);
    bool bWasDir = UnzipPossibleDirectory(m_szDestFileNameW.data());
    ::ShowWindow(::GetDlgItem(hWnd, IDC_ABORT_B), SW_SHOW);
    ::ShowWindow(::GetDlgItem(hWnd, IDC_ABORT_B2), SW_SHOW);

    if (bWasDir)
    {
        FTListViewClear(::GetDlgItem(hWnd, IDC_REMOTE_FILELIST));
        RequestRemoteDirectoryContent(hWnd, L"");
    }

    if (!m_fFileDownloadError && !bWasDir)
    {
        // Build real (final) Unicode filename by stripping the !UVNCPFT- prefix from m_szDestFileNameW
        std::array<WCHAR, MAX_PATH * 4> realNameW;
        ::wcscpy_s(realNameW.data(), realNameW.size(), m_szDestFileNameW.data());
        {
            // find and remove rfbPartialFilePrefix in Unicode path
            std::array<WCHAR, 32U> prefixW;
            ::MultiByteToWideChar(CP_ACP, 0, rfbPartialFilePrefix, -1, prefixW.data(), 32);
            WCHAR* pos = std::wcsstr(realNameW.data(), prefixW.data());
            if (pos)
            {
                std::wmemmove(pos, pos + std::wcslen(prefixW.data()), std::wcslen(pos + std::wcslen(prefixW.data())) + 1);
            }
        }
        std::array<WCHAR, MAX_PATH * 4> srcLongW;
        std::array<WCHAR, MAX_PATH * 4> dstLongW;
        if (m_szDestFileNameW[0U] && (m_szDestFileNameW[1U] == L':'))
        {
            ::_snwprintf_s(srcLongW.data(), srcLongW.size(), _TRUNCATE, L"\\\\?\\%s", m_szDestFileNameW.data());
        }
        else
        {
            ::wcscpy_s(srcLongW.data(), srcLongW.size(), m_szDestFileNameW.data());
        }
        if (realNameW[0U] && (realNameW[1U] == L':'))
        {
            ::_snwprintf_s(dstLongW.data(), dstLongW.size(), _TRUNCATE, L"\\\\?\\%s", realNameW.data());
        }
        else
        {
            ::wcscpy_s(dstLongW.data(), dstLongW.size(), realNameW.data());
        }
        if (!::MoveFileExW(srcLongW.data(), dstLongW.data(), MOVEFILE_REPLACE_EXISTING))
        {
            // failure. Updated status
            {
                std::array<wchar_t, MAX_PATH * 4> _wrn2;
                ::MultiByteToWideChar(CP_UTF8, 0, realName.c_str(), -1, _wrn2.data(), static_cast<int>(_wrn2.size()));
                std::array<wchar_t, 512U> szStatusW5;
                ::_snwprintf_s(szStatusW5.data(), 512, _TRUNCATE, L" %s < %s >", sz_H12.data(), _wrn2.data());
                SetStatus(szStatusW5.data());
            }
        }
    }

    // SetStatus(szStatus);
    ::UpdateWindow(hWnd);

    // Sound notif
    // MessageBeep(-1);

    // Request the next file in the list
    RequestNextFile();
    return true;
}


//
// Unzip possible directory
// Todo: handle unzip error correctly...
//
bool FileTransfer::UnzipPossibleDirectory(const LPCWSTR szFileName)
{
#if DEBUG_FT
    ::OutputDebugStringW(L"\n=== UnzipPossibleDirectory (viewer) ===\n");
    ::OutputDebugStringW(L"  szFileName: ");
    ::OutputDebugStringW(szFileName);
    ::OutputDebugStringW(L"\n");
#endif

    // Convert Unicode zip path to ANSI (zip path is always ASCII-safe temp dir)
    std::array<char, MAX_PATH * 3> szFileNameA;
    ::WideCharToMultiByte(CP_ACP, 0, szFileName, -1, szFileNameA.data(), sizeof(szFileNameA), nullptr, nullptr);

    if (!m_fFileDownloadError &&
        !std::strncmp(strrchr(szFileNameA.data(), '\\') + 1, rfbZipDirectoryPrefix, std::strlen(rfbZipDirectoryPrefix)))
    {
#if DEBUG_FT
        ::OutputDebugStringW(L"  Directory zip detected - proceeding with extract+rename\n");
#endif
        // Get parent path in Unicode directly (preserves national characters)
        std::array<WCHAR, MAX_PATH * 4> szParentPathW;
        ::wcscpy_s(szParentPathW.data(), szParentPathW.size(), szFileName);
        WCHAR* pw = std::wcsrchr(szParentPathW.data(), L'\\');
        if (pw)
        {
            *(pw + 1) = L'\0';
        } // Keep trailing backslash, remove filename

        // Also get ANSI version for zip library (zip path itself is ASCII-safe)
        std::array<char, MAX_PATH * 3> szPath;
        std::array<char, MAX_PATH * 3> szDirName;
        ::strcpy_s(szPath.data(), szPath.size(), szFileNameA.data());
        char* p = strrchr(szPath.data(), '\\') + 1;
        char* p2 = strchr(p, '-') + 1; // rfbZipDirectoryPrefix MUST have a "-" at the end
        ::strcpy_s(szDirName.data(), szDirName.size(), p2);
        char* p3 = strrchr(szDirName.data(), '.');
        *p3 = '\0';
        if (p != nullptr)
        {
            *p = '\0';
        }
        // szPath is now the parent dir in ANSI (for zip extraction)
        // szParentPathW is the parent dir in Unicode (for rename, preserves national chars)
        // szDirName is either hex-encoded UTF-8 dirname or 8.3 name (old format)

        // Detect hex encoding: all chars are hex digits and length is even
        bool bIsHexEncoded = ((std::strlen(szDirName.data()) > 0) && (std::strlen(szDirName.data()) % 2 == 0));
        for (int _ci{ 0 }; bIsHexEncoded && szDirName[static_cast<unsigned long long>(_ci)]; ++_ci)
        {
            if (!std::isxdigit(static_cast<unsigned char>(szDirName[static_cast<unsigned long long>(_ci)])))
            {
                bIsHexEncoded = false;
            }
        }

        // Decode hex -> UTF-8 -> Unicode dirname for the final folder name
        std::array<WCHAR, MAX_PATH * 4> szFinalDirNameW{};
        if (bIsHexEncoded)
        {
#if DEBUG_FT
            ::OutputDebugStringW(L"  Hex-encoded dirname detected\n");
#endif
            std::array<char, MAX_PATH * 4> szUtf8;
            int nBytes{ 0 };
            for (int _di{ 0 }; szDirName[static_cast<unsigned long long>(_di)] && szDirName[static_cast<unsigned long long>(_di + 1)]; _di += 2, ++nBytes)
            {
                char _hx[3U] = { szDirName[static_cast<unsigned long long>(_di)], szDirName[static_cast<unsigned long long>(_di + 1)], '\0' };
                szUtf8[static_cast<unsigned long long>(nBytes)] = static_cast<char>(std::strtol(_hx, nullptr, 16));
            }
            szUtf8[static_cast<unsigned long long>(nBytes)] = '\0';
            ::MultiByteToWideChar(CP_UTF8, 0, szUtf8.data(), -1, szFinalDirNameW.data(), static_cast<int>(szFinalDirNameW.size()));
#if DEBUG_FT
            ::OutputDebugStringW(L"  Decoded dirname: ");
            ::OutputDebugStringW(szFinalDirNameW.data());
            ::OutputDebugStringW(L"\n");
#endif
        }

        // Security: the final folder name comes from the server. It must be a
        // single plain folder name - reject "..", path separators, drive
        // specifiers and trailing dots/spaces before extracting anything.
        std::array<WCHAR, MAX_PATH * 4> szDirNameW2;
        ::MultiByteToWideChar(CP_ACP, 0, szDirName.data(), -1, szDirNameW2.data(), static_cast<int>(szDirNameW2.size()));
        const WCHAR* szEffectiveNameW = (std::wcslen(szFinalDirNameW.data()) > 0) ? szFinalDirNameW.data() : szDirNameW2.data();
        if (!IsSafeFileNameW(szEffectiveNameW))
        {
#if DEBUG_FT
            ::OutputDebugStringW(L"  REJECTED unsafe dir name: ");
            ::OutputDebugStringW(szEffectiveNameW);
            ::OutputDebugStringW(L"\n");
#endif
            ::DeleteFileW(szFileName);
            m_fFileDownloadError = true;
            std::array<wchar_t, 512U> szStatusErr;
            ::_snwprintf_s(szStatusErr.data(), 512, _TRUNCATE, L" %s < %s >", sz_H12.data(), szEffectiveNameW);
            SetStatus(szStatusErr.data());
            return true;
        }

        // Extract to ASCII temp name, then rename to Unicode
        std::array<char, 64U> szExtractName;
        ::sprintf_s(szExtractName.data(), szExtractName.size(), "UVNCTMP%08X", ::GetTickCount());
        std::array<char, MAX_PATH * 3> szExtractPath;
        ::strcpy_s(szExtractPath.data(), szExtractPath.size(), szPath.data());
        ::strcat_s(szExtractPath.data(), szExtractPath.size(), szExtractName.data());

#if DEBUG_FT
        ::OutputDebugStringW(L"  szPath (ANSI): ");
        std::array<WCHAR, MAX_PATH * 4> _dbgPath;
        ::MultiByteToWideChar(CP_ACP, 0, szPath.data(), -1, _dbgPath.data(), static_cast<int>(_dbgPath.size()));
        ::OutputDebugStringW(_dbgPath.data());
        ::OutputDebugStringW(L"\n");

        ::OutputDebugStringW(L"  szExtractPath (ANSI): ");
        std::array<WCHAR, MAX_PATH * 4> _dbgExtract;
        ::MultiByteToWideChar(CP_ACP, 0, szExtractPath.data(), -1, _dbgExtract.data(), static_cast<int>(_dbgExtract.size()));
        ::OutputDebugStringW(_dbgExtract.data());
        ::OutputDebugStringW(L"\n");
#endif

        bool fUnzip{ false };
        std::array<WCHAR, MAX_PATH * 4> szExtractPathW;
        ::MultiByteToWideChar(CP_ACP, 0, szExtractPath.data(), -1, szExtractPathW.data(), static_cast<int>(szExtractPathW.size()));

        {
            std::array<wchar_t, MAX_PATH * 4> _wdn4;
            ::MultiByteToWideChar(CP_ACP, 0, szExtractName.data(), -1, _wdn4.data(), static_cast<int>(_wdn4.size()));
            std::array<wchar_t, MAX_PATH * 4 + 64> szStatusW6;
            ::_snwprintf_s(szStatusW6.data(), szStatusW6.size(), _TRUNCATE, L" %s < %s >", sz_H59.data(), _wdn4.data());
            SetStatus(szStatusW6.data());

#if DEBUG_FT
            ::OutputDebugStringW(L"  Calling UnZipDirectory (Unicode)...\n");
#endif
            // Reset abort flag before starting unzip operation
            m_pMiniZipNG->ResetAbort();
            // Use CMiniZipNG for full Unicode support - run in thread to allow abort
            struct UnzipThreadParam
            {
                CMiniZipNG* pZip;
                LPCWSTR szExtractPath;
                LPCWSTR szZipPath;
                bool result;
            };
            UnzipThreadParam unzipParam{ m_pMiniZipNG, szExtractPathW.data(), szFileName, false };
            HANDLE hUnzipThread = ::CreateThread(
                nullptr, 0,
                [](LPVOID pv) -> DWORD {
                    UnzipThreadParam* p = reinterpret_cast<UnzipThreadParam*>(pv);
                    p->result = p->pZip->UnZipDirectory(p->szExtractPath, p->szZipPath);
                    return 0;
                },
                &unzipParam, 0, nullptr);
            if (hUnzipThread)
            {
                MSG msg;
                while (::MsgWaitForMultipleObjects(1, &hUnzipThread, FALSE, INFINITE, QS_ALLINPUT) != WAIT_OBJECT_0)
                {
                    // Check if user requested abort and signal the unzip thread to stop
                    if (m_fAbort && !m_pMiniZipNG->IsAbortRequested())
                    {
                        m_pMiniZipNG->RequestAbort();
                    }
                    while (::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
                    {
                        ::TranslateMessage(&msg);
                        ::DispatchMessage(&msg);
                    }
                }
                ::CloseHandle(hUnzipThread);
                fUnzip = unzipParam.result;
            }
            else
            {
                fUnzip = m_pMiniZipNG->UnZipDirectory(szExtractPathW.data(), szFileName);
            }
            if (fUnzip)
            {
                ::_snwprintf_s(szStatusW6.data(), szStatusW6.size(), _TRUNCATE, L" %s < %s >", sz_H61.data(), _wdn4.data());
            }
            else
            {
                ::_snwprintf_s(szStatusW6.data(), szStatusW6.size(), _TRUNCATE, L" %s < %s >. %s", sz_H62.data(), _wdn4.data(), sz_H63.data());
            }
            SetStatus(szStatusW6.data());
        }

        // Check if extracted folder exists
        const DWORD dwAttr = ::GetFileAttributesW(szExtractPathW.data());
        bool bFolderExists = ((dwAttr != INVALID_FILE_ATTRIBUTES) && (dwAttr & FILE_ATTRIBUTE_DIRECTORY));
#if DEBUG_FT
        ::OutputDebugStringW(L"  Extracted folder exists: ");
        ::OutputDebugStringW(bFolderExists ? L"YES\n" : L"NO\n");
#endif

        ::DeleteFileW(szFileName);

        if (fUnzip)
        {
#if DEBUG_FT
            ::OutputDebugStringW(L"  Unzip SUCCESS\n");
#endif

            // Rename extracted temp folder to original Unicode dirname
            std::array<WCHAR, MAX_PATH * 4> szExtractPathWInner;
            std::array<WCHAR, MAX_PATH * 4> szFinalPathW;

            // Use szParentPathW directly (Unicode, preserves national chars)
            std::array<WCHAR, 64U> szExtractNameW;
            ::MultiByteToWideChar(CP_ACP, 0, szExtractName.data(), -1, szExtractNameW.data(), 64);
            ::_snwprintf_s(szExtractPathWInner.data(), szExtractPathWInner.size(), _TRUNCATE, L"%s%s", szParentPathW.data(), szExtractNameW.data());

            ::_snwprintf_s(szFinalPathW.data(), szFinalPathW.size(), _TRUNCATE, L"%s%s", szParentPathW.data(), szEffectiveNameW);
#if DEBUG_FT
            ::OutputDebugStringW(L"  Renaming:\n    From: ");
            ::OutputDebugStringW(szExtractPathWInner.data());
            ::OutputDebugStringW(L"\n    To: ");
            ::OutputDebugStringW(szFinalPathW.data());
            ::OutputDebugStringW(L"\n");
#endif

            // Security: the resolved destination must be a direct child of
            // the download folder.
            BOOL bMoved = FALSE;
            if (IsDirectChildPathW(szParentPathW.data(), szFinalPathW.data()))
            {
                bMoved = ::MoveFileW(szExtractPathWInner.data(), szFinalPathW.data());
                if (!bMoved)
                {
                    // Destination folder already exists: merge extracted content into it
                    DWORD dwDstAttr = ::GetFileAttributesW(szFinalPathW.data());
                    if ((dwDstAttr != INVALID_FILE_ATTRIBUTES) && (dwDstAttr & FILE_ATTRIBUTE_DIRECTORY))
                    {
                        bMoved = MoveDirContentsInto(szExtractPathWInner.data(), szFinalPathW.data());
                    }
                }
            }
            else
            {
                m_fFileDownloadError = true;
            }
            if (!bMoved)
            {
                std::array<wchar_t, MAX_PATH * 4 + 64> szStatusW7;
                ::_snwprintf_s(szStatusW7.data(), MAX_PATH * 4 + 64, _TRUNCATE, L" %s < %s >", sz_H12.data(), szFinalPathW.data());
                SetStatus(szStatusW7.data());
            }
#if DEBUG_FT
            if (!bMoved)
            {
                DWORD dwErr = ::GetLastError();
                std::array<wchar_t, 128U> _emsg;
                ::_snwprintf_s(_emsg.data(), 128, _TRUNCATE, L"  Rename FAILED err=%lu\n", dwErr);
                ::OutputDebugStringW(_emsg.data());
            }
            else
            {
                ::OutputDebugStringW(L"  Rename/Merge SUCCESS\n");
            }
#endif
        }
#if DEBUG_FT
        ::OutputDebugStringW(L"=== UnzipPossibleDirectory complete ===\n\n");
#endif
        return true;
    }
    return false;
}

//
// Finish File Download
//
bool FileTransfer::AbortFileReception()
{
    //	vnclog.Print(0, _T("AbortFileReception\n"));
    if (!m_fFileDownloadRunning)
    {
        return false;
    }

    m_fFileDownloadError = true;
    ::FlushFileBuffers(m_hDestFile);
    std::array<char, 512 + 256> szStatus;
    ::sprintf_s(szStatus.data(), szStatus.size(), " %ls < %s >", sz_H19.data(), m_szDestFileName.data());

    m_fFileDownloadRunning = false;

    return true;
}

//
// Offer a file
//
bool FileTransfer::OfferLocalFileW(const LPCWSTR szSrcFileNameW)
{
    if (!m_fFTAllowed)
    {
        return false;
    }

    // Store Unicode path directly
    ::wcscpy_s(m_szSrcFileNameW.data(), m_szSrcFileNameW.size(), szSrcFileNameW);
    // Also store CP_ACP version for legacy code
    ::WideCharToMultiByte(CP_ACP, 0, szSrcFileNameW, -1, m_szSrcFileName.data(), sizeof(m_szSrcFileName), nullptr, nullptr);

    // Call legacy ANSI version which will use m_szSrcFileNameW internally
    return OfferLocalFile(m_szSrcFileName.data());
}

// Legacy ANSI version
bool FileTransfer::OfferLocalFile(const LPSTR szSrcFileName)
{
    //	vnclog.Print(0, _T("OfferLocalFile\n"));
    if (!m_fFTAllowed)
    {
        return false;
    }

#if DEBUG_FT
    {
        std::array<WCHAR, 512U> _dbg;
        ::_snwprintf_s(_dbg.data(), 512, _TRUNCATE, L"=== OfferLocalFile START: m_szHeadlessRemotePathW=[%s]\n", m_szHeadlessRemotePathW.data());
        ::OutputDebugStringW(_dbg.data());
    }
    {
        std::array<wchar_t, 2048U> debugMsg;
        ::MultiByteToWideChar(CP_ACP, 0, szSrcFileName, -1, debugMsg.data(), 2048);
        ::OutputDebugStringW(L"=== FT_UPLOAD: OfferLocalFile START ===\n");
        ::OutputDebugStringW(L"  szSrcFileName=");
        ::OutputDebugStringW(debugMsg.data());
        ::OutputDebugStringW(L"  m_szSrcFileNameW=");
        ::OutputDebugStringW(m_szSrcFileNameW.data());
        ::OutputDebugStringW(L"  m_fServerSupportsUnicode=");
        ::OutputDebugStringW(m_fServerSupportsUnicode ? L"TRUE\n" : L"FALSE\n");
    }
#endif

    ::strcpy_s(m_szSrcFileName.data(), m_szSrcFileName.size(), szSrcFileName);

    // sf@2003 - Directory Transfer trick
    // The File to transfer is actually a directory, so we must Zip it recursively and send
    // the resulting zip file (it will be recursively unzipped on server side once
    // the transfer is done)
    nDirZipRet = ZipPossibleDirectory(m_szSrcFileName.data());
    if (nDirZipRet == -1)
    {
        m_fFileUploadError = true;
        return false;
    }
    if (nDirZipRet == 1)
    {
        // ZipPossibleDirectory updated m_szSrcFileName to a zip path (always ASCII-safe temp path)
        ::MultiByteToWideChar(CP_ACP, 0, m_szSrcFileName.data(), -1, m_szSrcFileNameW.data(), static_cast<int>(m_szSrcFileNameW.size()));
    }

    // Open local src file using Unicode path (m_szSrcFileNameW) to support Chinese filenames
    // If m_szSrcFileNameW is empty (legacy call), fall back to CP_ACP conversion
    if (m_szSrcFileNameW[0U] == L'\0')
    {
        ::MultiByteToWideChar(CP_ACP, 0, m_szSrcFileName.data(), -1, m_szSrcFileNameW.data(), static_cast<int>(m_szSrcFileNameW.size()));
    }

    // Build long-path version of Unicode source path
    // Only add \\?\ prefix when path exceeds MAX_PATH - the prefix disables
    // path canonicalization which can cause ACCESS_DENIED with some Unicode chars
    std::array<WCHAR, MAX_PATH * 4> szSrcLongW;
    if (m_szSrcFileNameW[0U] && (m_szSrcFileNameW[1U] == L':') && (std::wcslen(m_szSrcFileNameW.data()) >= MAX_PATH))
    {
        ::_snwprintf_s(szSrcLongW.data(), szSrcLongW.size(), _TRUNCATE, L"\\\\?\\%s", m_szSrcFileNameW.data());
    }
    else
    {
        ::wcscpy_s(szSrcLongW.data(), szSrcLongW.size(), m_szSrcFileNameW.data());
    }

#if DEBUG_FT
    {
        std::array<wchar_t, 64U> _lenMsg;
        ::_snwprintf_s(_lenMsg.data(), 64, _TRUNCATE, L"  szSrcLongW len=%d MAX_PATH=%d\n", static_cast<int>(std::wcslen(szSrcLongW.data())), MAX_PATH);
        ::OutputDebugStringW(_lenMsg.data());
        ::OutputDebugStringW(L"  CreateFileW using szSrcLongW=");
        ::OutputDebugStringW(szSrcLongW.data());
        ::OutputDebugStringW(L"\n");
    }
#endif

    m_hSrcFile =
        ::CreateFileW(szSrcLongW.data(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);

    if (m_hSrcFile == INVALID_HANDLE_VALUE)
    {
        DWORD _openErr = ::GetLastError();
        // Fallback: try short (8.3) path - handles filenames with bidi control chars
        // or paths that exceed MAX_PATH where long paths are not enabled
        if ((_openErr == ERROR_ACCESS_DENIED) || (_openErr == ERROR_FILENAME_EXCED_RANGE))
        {
            std::array<WCHAR, MAX_PATH * 4> szShortW;
            DWORD nShort = ::GetShortPathNameW(m_szSrcFileNameW.data(), szShortW.data(), static_cast<DWORD>(szShortW.size()));
            if ((nShort > 0) && (nShort < MAX_PATH * 4))
            {
#if DEBUG_FT
                ::OutputDebugStringW(L"  Retrying with short path: ");
                ::OutputDebugStringW(szShortW.data());
                ::OutputDebugStringW(L"\n");
#endif
                m_hSrcFile = ::CreateFileW(szShortW.data(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                         FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
                if (m_hSrcFile != INVALID_HANDLE_VALUE)
                {
                    // Also update m_szSrcFileNameW to the short path for subsequent operations
                    ::wcscpy_s(m_szSrcFileNameW.data(), m_szSrcFileNameW.size(), szShortW.data());
                    goto src_open_ok;
                }
                _openErr = ::GetLastError();
            }
        }
        {
            std::array<wchar_t, MAX_PATH * 4> _wsf;
            ::MultiByteToWideChar(CP_ACP, 0, m_szSrcFileName.data(), -1, _wsf.data(), MAX_PATH * 4);
            std::array<wchar_t, 512U> szStatusW7;
            ::_snwprintf_s(szStatusW7.data(), 512, _TRUNCATE, L" %s < %s >", sz_H21.data(), _wsf.data());
            SetStatus(szStatusW7.data());
        }
        m_fFileUploadError = true;
#if DEBUG_FT
        {
            std::array<wchar_t, MAX_PATH * 4 + 64> _emsg;
            ::_snwprintf_s(_emsg.data(), MAX_PATH * 4 + 64, _TRUNCATE, L"  ERROR: CreateFileW failed err=%lu path=[%s]\n", _openErr, szSrcLongW.data());
            ::OutputDebugStringW(_emsg.data());
        }
#endif
        return false;
    }
src_open_ok:
#if DEBUG_FT
    ::OutputDebugStringW(L"  Source file opened successfully\n");
#endif

    // Size of src file - use Unicode path for correct size retrieval
    ULARGE_INTEGER n2SrcSize;
    WIN32_FIND_DATAW fdW2;
    HANDLE ffSize = ::FindFirstFileW(m_szSrcFileNameW.data(), &fdW2);
    bool bSize = (ffSize != INVALID_HANDLE_VALUE);
    if (bSize)
    {
        ::FindClose(ffSize);
        n2SrcSize.LowPart = fdW2.nFileSizeLow;
        n2SrcSize.HighPart = fdW2.nFileSizeHigh;
        n2SrcSize.QuadPart = static_cast<ULONGLONG>(((static_cast<__int64>(fdW2.nFileSizeHigh)) << 32) + fdW2.nFileSizeLow);
    }
    if (!bSize)
    {
        {
            std::array<wchar_t, MAX_PATH * 4> _wsf4;
            ::MultiByteToWideChar(CP_ACP, 0, m_szSrcFileName.data(), -1, _wsf4.data(), static_cast<int>(_wsf4.size()));
            std::array<wchar_t, 512U> szStatusW12;
            ::_snwprintf_s(szStatusW12.data(), 512, _TRUNCATE, L" %s < %s >", sz_H21.data(), _wsf4.data());
            SetStatus(szStatusW12.data());
        }
        ::CloseHandle(m_hSrcFile);
        m_fFileUploadError = true;
        return false;
    }

    std::array<char, 96U> szFFS;
    GetFriendlyFileSizeString(static_cast<long long>(n2SrcSize.QuadPart), szFFS.data(), 96);
    {
        std::array<wchar_t, MAX_PATH * 4> _wsf2;
        ::MultiByteToWideChar(CP_ACP, 0, m_szSrcFileName.data(), -1, _wsf2.data(), static_cast<int>(_wsf2.size()));
        std::array<wchar_t, 96U> _wffs2;
        ::MultiByteToWideChar(CP_ACP, 0, szFFS.data(), -1, _wffs2.data(), 96);
        std::array<wchar_t, 512U> szStatusW8;
        ::_snwprintf_s(szStatusW8.data(), 512, _TRUNCATE, L" %s < %s > (%s) >>>", sz_H22.data(), _wsf2.data(), _wffs2.data());
        SetStatus(szStatusW8.data());
    }
    m_nnFileSize = static_cast<long long>(n2SrcSize.QuadPart);
    SetTotalSize(hWnd, static_cast<DWORD>(m_nnFileSize)); // In bytes
    SetGauge(hWnd, 0);                                    // In bytes
    ::UpdateWindow(hWnd);

    // Add the File Time Stamp to the filename
    FILETIME SrcFileModifTime;
    BOOL fRes = ::GetFileTime(m_hSrcFile, nullptr, nullptr, &SrcFileModifTime);
    if (!fRes)
    {
        {
            std::array<wchar_t, MAX_PATH * 4> _wsf3;
            ::MultiByteToWideChar(CP_ACP, 0, m_szSrcFileName.data(), -1, _wsf3.data(), static_cast<int>(_wsf3.size()));
            std::array<wchar_t, 512U> szStatusW9;
            ::_snwprintf_s(szStatusW9.data(), 512, _TRUNCATE, L" %s < %s >", sz_H23.data(), _wsf3.data());
            SetStatus(szStatusW9.data());
        }
        ::CloseHandle(m_hSrcFile);
        m_fFileUploadError = true;
        return false;
    }

    ::CloseHandle(m_hSrcFile);

    // Build destination path in Unicode to handle Chinese remote paths correctly.
    std::array<WCHAR, MAX_PATH * 4> szDstFileNameW;
    if (hWnd != nullptr)
    {
        ::GetDlgItemTextW(hWnd, IDC_CURR_REMOTE, szDstFileNameW.data(), MAX_PATH);
        if (!std::wcslen(szDstFileNameW.data()))
        {
            return false;
        } // no destination dir selected
        // Append local filename from Unicode source path (avoids CP_ACP corruption)
        const WCHAR* pSrcBasenameW = std::wcsrchr(m_szSrcFileNameW.data(), L'\\');
        if (pSrcBasenameW)
        {
            ++pSrcBasenameW;
        }
        else
        {
            pSrcBasenameW = m_szSrcFileNameW.data();
        }
        ::wcscat_s(szDstFileNameW.data(), szDstFileNameW.size(), pSrcBasenameW);
    }
    else
    {
        // Headless mode: full remote destination path is in m_szHeadlessRemotePathW
        if (!m_szHeadlessRemotePathW[0U])
        {
            return false;
        }
        ::wcscpy_s(szDstFileNameW.data(), szDstFileNameW.size(), m_szHeadlessRemotePathW.data());
    }
    // Convert full Unicode dest path to CP_ACP for legacy szDstFileName
    std::array<char, MAX_PATH * 4> szDstFileName;
    ::WideCharToMultiByte(CP_ACP, 0, szDstFileNameW.data(), -1, szDstFileName.data(), static_cast<int>(szDstFileName.size()), nullptr, nullptr);

    std::array<char, 18U> szSrcFileTime;
    // sf@2003
    // For now, we've made the choice off displaying all the files
    // off client AND server sides converted in clients local
    // time only. We keep file time as it is before transfering the file (absolute time)
    /*
    FILETIME LocalFileTime;
    FileTimeToLocalFileTime(&SrcFileModifTime, &LocalFileTime);
    */
    SYSTEMTIME FileTime;
    ::FileTimeToSystemTime(/*&LocalFileTime*/ &SrcFileModifTime, &FileTime);
    ::sprintf_s(szSrcFileTime.data(), szSrcFileTime.size(), "%2.2d/%2.2d/%4.4d %2.2d:%2.2d", FileTime.wMonth, FileTime.wDay, FileTime.wYear, FileTime.wHour, FileTime.wMinute);
    // Append timestamp to both ANSI and Unicode dest filename
    ::strcat_s(szDstFileName.data(), szDstFileName.size(), ",");
    ::strcat_s(szDstFileName.data(), szDstFileName.size(), szSrcFileTime.data());
    {
        std::array<WCHAR, 32U> szTimestampW;
        ::MultiByteToWideChar(CP_ACP, 0, ",", -1, szTimestampW.data(), 32);
        ::wcscat_s(szDstFileNameW.data(), szDstFileNameW.size(), szTimestampW.data());
        ::MultiByteToWideChar(CP_ACP, 0, szSrcFileTime.data(), -1, szTimestampW.data(), 32);
        ::wcscat_s(szDstFileNameW.data(), szDstFileNameW.size(), szTimestampW.data());
    }

    // sf@2004 - Delta Transfer
    if (m_lpCSBuffer != nullptr)
    {
        delete[] m_lpCSBuffer;
        m_lpCSBuffer = nullptr;
    }
    m_nCSOffset = 0;
    m_nCSBufferSize = 0;

    // Send the FileTransferMsg with rfbFileTransferOffer
    // So the server creates the appropriate new file on the other side
    rfbFileTransferMsg ft;

    ft.type = rfbFileTransfer;
    ft.contentType = rfbFileTransferOffer;
    ft.contentParam = 0;
    ft.size = Swap32IfLE(n2SrcSize.LowPart); // File Size in bytes

    // Convert filename to UTF-8 if server supports Unicode
    std::array<char, MAX_PATH * 3 + 32> szDstFileNameToSend; // UTF-8 can be up to 3x longer + timestamp
    if (m_fServerSupportsUnicode)
    {
        // szDstFileNameW already has the correct Unicode path (remote dir + local basename + timestamp)
        ::WideCharToMultiByte(CP_UTF8, 0, szDstFileNameW.data(), -1, szDstFileNameToSend.data(), static_cast<int>(szDstFileNameToSend.size()), nullptr, nullptr);
    }
    else
    {
        // Old server: send as-is
        ::strcpy_s(szDstFileNameToSend.data(), szDstFileNameToSend.size(), szDstFileName.data());
    }

    // DEBUG: Show what we're sending to server
#if DEBUG_FT
    {
        std::array<wchar_t, 2048U> debugMsg;
        ::MultiByteToWideChar(CP_UTF8, 0, szDstFileNameToSend.data(), -1, debugMsg.data(), 2048);
        ::OutputDebugStringW(L"  Sending filename to server: ");
        ::OutputDebugStringW(debugMsg.data());
        std::array<wchar_t, 64U> debugProto;
        ::_snwprintf_s(debugProto.data(), 64, _TRUNCATE, L" (UTF-8=%s, len=%d)\n", m_fServerSupportsUnicode ? L"Y" : L"N", static_cast<int>(std::strlen(szDstFileNameToSend.data())));
        ::OutputDebugStringW(debugProto.data());
    }
#endif

    ft.length = Swap32IfLE(std::strlen(szDstFileNameToSend.data()));
    //adzm 2010-09
    m_pCC->WriteExactQueue(reinterpret_cast<char*>(&ft), sz_rfbFileTransferMsg, rfbFileTransfer);
    m_pCC->WriteExactQueue(reinterpret_cast<char*>(szDstFileNameToSend.data()), static_cast<int>(std::strlen(szDstFileNameToSend.data())));
    rfbFileTransferOfferRequested = true;
    if (!UsingOldProtocol())
    {
        CARD32 sizeH = Swap32IfLE(n2SrcSize.HighPart);
        m_pCC->WriteExact(reinterpret_cast<char*>(&sizeH), sizeof(CARD32));
    }
    else
    {
        m_pCC->FlushWriteQueue();
    }

    // show the stop button
    ::ShowWindow(::GetDlgItem(hWnd, IDC_ABORT_B), SW_SHOW);
    ::ShowWindow(::GetDlgItem(hWnd, IDC_ABORT_B2), SW_SHOW);
    m_pCC->SetSendTimeout();
    return true;
}


//
// Zip a possible directory
//
int FileTransfer::ZipPossibleDirectory(const LPSTR szSrcFileName)
{
#if DEBUG_FT
    ::OutputDebugStringW(L"\n=== ZipPossibleDirectory ===\n");
    ::OutputDebugStringW(L"  m_szSrcFileNameW: ");
    ::OutputDebugStringW(m_szSrcFileNameW.data());
    ::OutputDebugStringW(L"\n");
#endif

    char* p1 = strrchr(szSrcFileName, '\\') + 1;
    char* p2 = strrchr(szSrcFileName, rfbDirSuffix[0U]);
    if ((p1[0U] == rfbDirPrefix[0U]) && (p1[1U] == rfbDirPrefix[1U]) && // Check dir prefix
        (p2[1U] == rfbDirSuffix[1U]) && (p2 != nullptr) && (p1 < p2))   // Check dir suffix
    {
#if DEBUG_FT
        ::OutputDebugStringW(L"  Directory detected\n");
#endif
        // sf@2004 - Improving Directory Transfer: Avoids ReadOnly media problem
        // Use Unicode paths derived from m_szSrcFileNameW to correctly handle
        // non-ASCII directory names (Arabic, Chinese, etc.)
        std::array<WCHAR, MAX_PATH> szWorkingDirW;
        ::GetTempPathW(MAX_PATH, szWorkingDirW.data());
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

        // Parse Unicode source path from m_szSrcFileNameW: "C:\parent\[ dirname ]"
        std::array<WCHAR, MAX_PATH * 4> szPathW;
        std::array<WCHAR, MAX_PATH * 4> szDirNameW;
        ::wcscpy_s(szPathW.data(), szPathW.size(), m_szSrcFileNameW.data());
        WCHAR* pw1 = std::wcsrchr(szPathW.data(), L'\\') + 1;
        // pw1 points to "[ dirname ]" - skip rfbDirPrefix (2 chars "[ ") and strip rfbDirSuffix (2 chars " ]")
        ::wcscpy_s(szDirNameW.data(), szDirNameW.size(), pw1 + 2);
        std::size_t nDirLen = std::wcslen(szDirNameW.data());
        if (nDirLen >= 2)
        {
            szDirNameW[nDirLen - 2] = L'\0';
        }
        *pw1 = L'\0'; // szPathW is now "C:\parent\"

        ::OutputDebugStringW(L"  szDirNameW: ");
        ::OutputDebugStringW(szDirNameW.data());
        ::OutputDebugStringW(L"\n");
        ::OutputDebugStringW(L"  szPathW: ");
        ::OutputDebugStringW(szPathW.data());
        ::OutputDebugStringW(L"\n");

        // Full source dir path: "C:\parent\dirname"
        std::array<WCHAR, MAX_PATH * 4> szSrcDirW;
        ::_snwprintf_s(szSrcDirW.data(), szSrcDirW.size(), _TRUNCATE, L"%s%s", szPathW.data(), szDirNameW.data());

        // Full wildcard path for zip: "C:\parent\dirname\*.*"
        std::array<WCHAR, MAX_PATH * 4> szSrcDirWild;
        ::_snwprintf_s(szSrcDirWild.data(), szSrcDirWild.size(), _TRUNCATE, L"%s%s\\*.*", szPathW.data(), szDirNameW.data());

        // Build zip output path in temp dir - must be ASCII-safe.
        // Encode the original Unicode dirname as UTF-8 hex so the server can
        // recover the original name and rename the extracted folder.
        // Format: !UVNCDIR-<hexUTF8ofDirName>.zip
        std::array<char, MAX_PATH * 4> szDirNameUtf8;
        ::WideCharToMultiByte(CP_UTF8, 0, szDirNameW.data(), -1, szDirNameUtf8.data(), static_cast<int>(szDirNameUtf8.size()), nullptr, nullptr);
        std::array<char, MAX_PATH * 8> szHexName; // 2 hex chars per byte
        szHexName[0U] = '\0';
        for (int _hi{ 0 }; (szDirNameUtf8[static_cast<unsigned long long>(_hi)] != '\0') && (_hi < MAX_PATH * 4 - 1); ++_hi)
        {
            std::array<char, 3U> _hbuf;
            ::sprintf_s(_hbuf.data(), _hbuf.size(), "%02X", static_cast<unsigned char>(szDirNameUtf8[static_cast<unsigned long long>(_hi)]));
            ::strcat_s(szHexName.data(), szHexName.size(), _hbuf.data());
        }
        // Truncate hex to keep zip filename reasonable (max 200 hex chars = 100 bytes UTF-8)
        if (std::strlen(szHexName.data()) > 200)
        {
            szHexName[200U] = '\0';
        }

        ::OutputDebugStringW(L"  Hex-encoded name: ");
        std::array<WCHAR, MAX_PATH * 8> _dbgHex;
        ::MultiByteToWideChar(CP_ACP, 0, szHexName.data(), -1, _dbgHex.data(), static_cast<int>(_dbgHex.size()));
        ::OutputDebugStringW(_dbgHex.data());
        ::OutputDebugStringW(L"\n");

        std::array<WCHAR, MAX_PATH * 4> szDirZipPathW;
        ::_snwprintf_s(szDirZipPathW.data(), szDirZipPathW.size(), _TRUNCATE, L"%s%hs%hs%s", szWorkingDirW.data(), rfbZipDirectoryPrefix, szHexName.data(), L".zip");

        ::OutputDebugStringW(L"  Zip path: ");
        ::OutputDebugStringW(szDirZipPathW.data());
        ::OutputDebugStringW(L"\n");

        // Convert to short (8.3) ANSI paths for the ANSI zip library
        // Short paths are always ASCII-safe
        std::array<char, MAX_PATH * 4> szSrcDirA;
        std::array<char, MAX_PATH * 4> szSrcDirWildA;
        std::array<char, MAX_PATH * 4> szDirZipPathA;
        std::array<WCHAR, MAX_PATH * 4> szShortSrcW;
        std::array<WCHAR, MAX_PATH * 4> szShortSrcWildW;
        std::array<WCHAR, MAX_PATH * 4> szShortZipW;
        // Try short path first; fall back to direct CP_ACP if GetShortPathNameW fails
        if (!::GetShortPathNameW(szSrcDirW.data(), szShortSrcW.data(), static_cast<DWORD>(szShortSrcW.size())))
        {
            ::wcscpy_s(szShortSrcW.data(), szShortSrcW.size(), szSrcDirW.data());
        }
        // Do NOT call GetShortPathNameW on a wildcard path - it always fails.
        // Instead build the wildcard from the already-computed short dir path.
        ::_snwprintf_s(szShortSrcWildW.data(), szShortSrcWildW.size(), _TRUNCATE, L"%s\\*.*", szShortSrcW.data());
        // Zip output is already in ASCII-safe temp dir
        ::wcscpy_s(szShortZipW.data(), szShortZipW.size(), szDirZipPathW.data());
        ::WideCharToMultiByte(CP_ACP, 0, szShortSrcW.data(), -1, szSrcDirA.data(), static_cast<int>(szSrcDirA.size()), nullptr, nullptr);
        ::WideCharToMultiByte(CP_ACP, 0, szShortSrcWildW.data(), -1, szSrcDirWildA.data(), static_cast<int>(szSrcDirWildA.size()), nullptr, nullptr);
        ::WideCharToMultiByte(CP_ACP, 0, szShortZipW.data(), -1, szDirZipPathA.data(), static_cast<int>(szDirZipPathA.size()), nullptr, nullptr);

        bool fZip;
        {
            std::array<wchar_t, MAX_PATH * 4 + 64> szStatusW10;
            ::_snwprintf_s(szStatusW10.data(), szStatusW10.size(), _TRUNCATE, L" %s < %s >", sz_H64.data(), szSrcDirW.data());
            SetStatus(szStatusW10.data());
            HWND hProg = ::GetDlgItem(hWnd, IDC_PROGRESS);
            ::SetWindowLong(hProg, GWL_STYLE, ::GetWindowLong(hProg, GWL_STYLE) | PBS_MARQUEE);
            ::SendMessage(hProg, PBM_SETMARQUEE, TRUE, 50);
            ::UpdateWindow(hWnd);
            // Reset abort flag before starting zip operation
            m_pMiniZipNG->ResetAbort();
            struct ZipThreadParam
            {
                CMiniZipNG* pZip;
                LPCWSTR szSrcDir;
                LPCWSTR szSrcWild;
                LPCWSTR szZipPath;
                bool result;
            };
            ZipThreadParam zipParam{ m_pMiniZipNG, szSrcDirW.data(), szSrcDirWild.data(), szDirZipPathW.data(), false };
            HANDLE hZipThread = ::CreateThread(
                nullptr, 0,
                [](LPVOID pv) -> DWORD {
                    ZipThreadParam* p = reinterpret_cast<ZipThreadParam*>(pv);
                    p->result = p->pZip->ZipDirectory(p->szSrcDir, p->szSrcWild, p->szZipPath, true);
                    return 0;
                },
                &zipParam, 0, nullptr);
            if (hZipThread)
            {
                MSG msg;
                while (::MsgWaitForMultipleObjects(1, &hZipThread, FALSE, INFINITE, QS_ALLINPUT) != WAIT_OBJECT_0)
                {
                    // Check if user requested abort and signal the zip thread to stop
                    if (m_fAbort && !m_pMiniZipNG->IsAbortRequested())
                    {
                        m_pMiniZipNG->RequestAbort();
                    }
                    while (::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
                    {
                        ::TranslateMessage(&msg);
                        ::DispatchMessage(&msg);
                    }
                }
                ::CloseHandle(hZipThread);
                fZip = zipParam.result;
            }
            else
            {
                fZip = m_pMiniZipNG->ZipDirectory(szSrcDirW.data(), szSrcDirWild.data(), szDirZipPathW.data(), true);
            }
            ::SendMessage(hProg, PBM_SETMARQUEE, FALSE, 0);
            ::SetWindowLong(hProg, GWL_STYLE, ::GetWindowLong(hProg, GWL_STYLE) & ~PBS_MARQUEE);
            ::SendMessage(hProg, PBM_SETPOS, 0, 0);
            if (fZip)
            {
                ::_snwprintf_s(szStatusW10.data(), szStatusW10.size(), _TRUNCATE, L" %s < %s >", sz_H66.data(), szSrcDirW.data());
            }
            else
            {
                ::_snwprintf_s(szStatusW10.data(), szStatusW10.size(), _TRUNCATE, L" %s < %s >. %s", sz_H68.data(), szSrcDirW.data(), sz_H69.data());
            }
            SetStatus(szStatusW10.data());
        }

#if DEBUG_FT
        ::OutputDebugStringW(fZip ? L"  Zip SUCCESS\n" : L"  Zip FAILED\n");
#endif
        if (!fZip)
        {
            return -1;
        }
        ::strcpy_s(szSrcFileName, MAX_PATH * 3, szDirZipPathA.data());
#if DEBUG_FT
        ::OutputDebugStringW(L"=== ZipPossibleDirectory complete ===\n\n");
#endif
        return 1;
    }
    else
    {
        return 0;
    }
}

//
// sf@2004 - Delta Transfer
// Destination file already exists
// The server sends the checksums of this file in one shot.
//
bool FileTransfer::ReceiveDestinationFileChecksums([[maybe_unused]] const int nSize, const UINT nLen)
{
    //	vnclog.Print(0, _T("ReceiveDestinationFileChecksums\n"));
    if (!ViewerBounds::payload(nLen) || (nLen % 4 != 0))
    {
        throw ErrorException(L"Invalid checksum payload length.");
    }
    delete[] m_lpCSBuffer;
    m_lpCSBuffer = nullptr;
    m_nCSBufferSize = 0;
    m_nCSOffset = 0;
    m_lpCSBuffer = new char[nLen + 1]; //nSize
    if (m_lpCSBuffer == nullptr)
    {
        return false;
    }

    // char szStatus[255];
    // sprintf_s(szStatus, " Receiving %d bytes of file checksums from remote machine. Please wait...", nLen);
    // SetStatus(szStatus);

    std::memset(m_lpCSBuffer, '\0', nLen + 1); // nSize

    m_pCC->ReadExact(reinterpret_cast<char*>(m_lpCSBuffer), static_cast<int>(nLen));
    m_nCSBufferSize = static_cast<int>(nLen);

    return true;
}


//
//  SendFile
//
bool FileTransfer::SendFile(const long lSize, const UINT nLen)
{
    //	vnclog.Print(0, _T("SendFile\n"));
    if (!m_fFTAllowed)
    {
        return false;
    }
    if (nLen == 0)
    {
        return false;
    } // Used when the local file could no be open in OfferLocalFile

#if DEBUG_FT
    ::OutputDebugStringW(L"=== FT_UPLOAD: SendFile START (server accepted) ===\n");
    {
        std::array<wchar_t, 256U> debugMsg;
        ::_snwprintf_s(debugMsg.data(), 256, _TRUNCATE, L"  nLen=%d, lSize=%ld\n", nLen, lSize);
        ::OutputDebugStringW(debugMsg.data());
    }
#endif

    if (!ViewerBounds::payload(nLen))
    {
        throw ErrorException(L"Invalid remote filename length.");
    }
    char* szRemoteFileName = new char[nLen + 1];
    if (szRemoteFileName == nullptr)
    {
        return false;
    }
    std::memset(szRemoteFileName, 0, nLen + 1);

    // Read in the Name of the file to copy (remote full name !)
    m_pCC->ReadExact(szRemoteFileName, static_cast<int>(nLen));

    if (nLen > MAX_PATH)
    {
        szRemoteFileName[MAX_PATH] = '\0';
    }
    else
    {
        szRemoteFileName[nLen] = '\0';
    }

    // DEBUG: Show what server sent back
#if DEBUG_FT
    {
        std::array<wchar_t, 2048U> debugMsg;
        ::MultiByteToWideChar(CP_UTF8, 0, szRemoteFileName, -1, debugMsg.data(), 2048);
        ::OutputDebugStringW(L"  Server returned remote filename: ");
        ::OutputDebugStringW(debugMsg.data());
        ::OutputDebugStringW(L"\n");
    }
#endif

    // If lSize = -1 (0xFFFFFFFF) that means that the Dst file on the remote machine
    // could not be created for some reason (locked..)
    if (lSize == -1)
    {
        {
            std::array<wchar_t, MAX_PATH> _wrf3;
            ::MultiByteToWideChar(CP_UTF8, 0, get_real_filename(szRemoteFileName).c_str(), -1, _wrf3.data(), static_cast<int>(_wrf3.size()));
            std::array<wchar_t, 512U> szStatusW11;
            ::_snwprintf_s(szStatusW11.data(), 512, _TRUNCATE, L" %s < %s > %s", sz_H12.data(), _wrf3.data(), sz_H24.data());
            SetStatus(szStatusW11.data());
            ::_snwprintf_s(szStatusW11.data(), 512, _TRUNCATE, L" %s < %s > %s", sz_H25.data(), _wrf3.data(), sz_H26.data());
            SetStatus(szStatusW11.data());
        }
        m_fFileUploadError = true;

        delete[] szRemoteFileName;
        return false;
    }

    delete[] szRemoteFileName;

    // Open src file using Unicode path from m_szSrcFileNameW (set in OfferLocalFile)
    if (m_szSrcFileNameW[0U] == L'\0')
    {
        ::MultiByteToWideChar(CP_ACP, 0, m_szSrcFileName.data(), -1, m_szSrcFileNameW.data(), MAX_PATH + 32);
    }
    std::array<WCHAR, MAX_PATH * 4> szSrcLongW2;
    if (m_szSrcFileNameW[0U] && (m_szSrcFileNameW[1U] == L':') && (std::wcslen(m_szSrcFileNameW.data()) >= MAX_PATH))
    {
        ::_snwprintf_s(szSrcLongW2.data(), szSrcLongW2.size(), _TRUNCATE, L"\\\\?\\%s", m_szSrcFileNameW.data());
    }
    else
    {
        ::wcscpy_s(szSrcLongW2.data(), szSrcLongW2.size(), m_szSrcFileNameW.data());
    }

#if DEBUG_FT
    ::OutputDebugStringW(L"  Opening source file: ");
    ::OutputDebugStringW(szSrcLongW2.data());
    ::OutputDebugStringW(L"\n");
#endif

    m_hSrcFile =
        ::CreateFileW(szSrcLongW2.data(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);

    if (m_hSrcFile == INVALID_HANDLE_VALUE)
    {
        {
            std::array<wchar_t, MAX_PATH> _wsf;
            ::MultiByteToWideChar(CP_ACP, 0, m_szSrcFileName.data(), -1, _wsf.data(), MAX_PATH);
            std::array<wchar_t, 512U> szStatusW7;
            ::_snwprintf_s(szStatusW7.data(), 512, _TRUNCATE, L" %s < %s >", sz_H21.data(), _wsf.data());
            SetStatus(szStatusW7.data());
        }
#if DEBUG_FT
        {
            DWORD _err2 = ::GetLastError();
            std::array<wchar_t, 128U> _emsg2;
            ::_snwprintf_s(_emsg2.data(), 128, _TRUNCATE, L"  ERROR: SendFile CreateFileW failed err=%lu path=[%s]\n", _err2, szSrcLongW2.data());
            ::OutputDebugStringW(_emsg2.data());
        }
#endif
        m_fFileUploadError = true;
        return false;
    }
#if DEBUG_FT
    ::OutputDebugStringW(L"  Source file opened successfully\n");
#endif

    m_fFileUploadError = false;
    m_dwNbBytesRead = 0;
    m_dwTotalNbBytesRead = 0;
    m_fEof = false;

    // If the connection speed is > 2048 Kbit/s, no need to compress.
    m_fCompress = (m_pCC->kbitsPerSecond <= 2048);

    m_fFileUploadRunning = true;
    m_fFileUploadError = false;
    //m_dwLastChunkTime = timeGetTime();
    m_dwLastChunkTime = ::GetTickCount();
    m_dwStartTick = ::GetTickCount();
    // m_nNotSent = 0;
    // SendFileChunk();
    return true;
}


//
// Send the next file packet (upload)
// This function sends chunks for up to 50ms, then returns to allow UI processing.
// The timer calls this periodically. No more tight infinite loop.
//
bool FileTransfer::SendFileChunk()
{
    DWORD dwStartTime = ::GetTickCount();
    const DWORD dwMaxTimePerCall{ 50U }; // Max 50ms per call to maintain responsiveness

    static DWORD s_debugLastReport{ 0U };
    static __int64 s_debugLastBytes{ 0 };

    while (true)
    {
        m_dwLastChunkTime = ::GetTickCount();

        if (!m_fFileUploadRunning)
        {
#if DEBUG_FT
            ::OutputDebugStringW(L"  SendFileChunk: m_fFileUploadRunning=FALSE, returning\n");
#endif
            return false;
        }

        if (m_fEof || m_fFileUploadError)
        {
#if DEBUG_FT
            {
                std::array<wchar_t, 128U> debugMsg;
                ::_snwprintf_s(debugMsg.data(), 128, _TRUNCATE, L"  SendFileChunk: EOF=%d, Error=%d, calling FinishFileSending\n", m_fEof, m_fFileUploadError);
                ::OutputDebugStringW(debugMsg.data());
            }
#endif
            FinishFileSending();
            return true;
        }

        if (m_fAbort)
        {
            m_fFileUploadError = true;
            FinishFileSending();
            return false;
        }

        m_pCC->CheckFileChunkBufferSize(m_nBlockSize + 1024);

        int nRes = ::ReadFile(m_hSrcFile, m_pCC->m_filechunkbuf, m_nBlockSize, &m_dwNbBytesRead, nullptr);
        if (!nRes && (m_dwNbBytesRead != 0))
        {
            m_fFileUploadError = true;
            return false;
        }

        if (nRes && (m_dwNbBytesRead == 0))
        {
            m_fEof = true;
#if DEBUG_FT
            ::OutputDebugStringW(L"  SendFileChunk: ReadFile returned 0 bytes, EOF reached\n");
#endif
            return true; // Next timer tick will call FinishFileSending
        }

        // DEBUG: Report progress every 1 second
        if (::GetTickCount() - s_debugLastReport > 1000)
        {
            DWORD dwElapsed = ::GetTickCount() - s_debugLastReport;
            __int64 bytesSent = m_dwTotalNbBytesRead - s_debugLastBytes;
            [[maybe_unused]] const DWORD kbps = dwElapsed > 0 ? static_cast<DWORD>(((bytesSent * 8 / 1024) * 1000 / dwElapsed)) : 0;
#if DEBUG_FT
            {
                std::array<wchar_t, 256U> debugMsg;
                ::_snwprintf_s(debugMsg.data(), 256, _TRUNCATE, L"  SendFileChunk: Read=%lu bytes, Total=%I64d\n", m_dwNbBytesRead, m_dwTotalNbBytesRead);
                ::OutputDebugStringW(debugMsg.data());
            }
#endif
            s_debugLastReport = ::GetTickCount();
            s_debugLastBytes = m_dwTotalNbBytesRead;
        }

        // sf@2004 - Delta Transfer
        bool fAlreadyThere{ false };
        unsigned long nCS{ 0U };
        // if Checksums are available for this file
        if (m_lpCSBuffer != nullptr)
        {
            if (ViewerBounds::range(m_nCSOffset, 4, m_nCSBufferSize))
            {
                std::memcpy(&nCS, &m_lpCSBuffer[m_nCSOffset], 4);
                if (nCS != 0)
                {
                    m_nCSOffset += 4;
                    unsigned long cs = adler32(0L, Z_NULL, 0);
                    cs = adler32(cs, m_pCC->m_filechunkbuf, static_cast<int>(m_dwNbBytesRead));
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
            rfbFileTransferMsg ft;
            ft.type = rfbFileTransfer;
            ft.contentType = rfbFilePacket;
            ft.size = Swap32IfLE(2); // Means "Empty packet"// Swap32IfLE(nCS);
            ft.length = Swap32IfLE(m_dwNbBytesRead);
            m_pCC->WriteExact(reinterpret_cast<char*>(&ft), sz_rfbFileTransferMsg, rfbFileTransfer);
            // m_nNotSent += m_dwNbBytesRead;
        }
        else
        {
            // Compress the data
            // (Compressed data can be longer if it was already compressed)
            unsigned int nMaxCompSize = m_nBlockSize + 1024; // TODO: Improve this...
            bool fCompressed{ false };
            if (m_fCompress && !UsingOldProtocol())
            {
                m_pCC->CheckFileZipBufferSize(static_cast<int>(nMaxCompSize));
                int nRetC = compress(reinterpret_cast<unsigned char*>((m_pCC->m_filezipbuf)), reinterpret_cast<unsigned long*>(&nMaxCompSize),
                                     reinterpret_cast<unsigned char*>(m_pCC->m_filechunkbuf), m_dwNbBytesRead);
                if (nRetC != 0)
                {
                    // Todo: send data uncompressed instead
                    m_fFileUploadError = true;
                    return false;
                }
                fCompressed = true;
            }

            // If data compressed is larger, we're presumably dealing with already compressed data.
            if (nMaxCompSize > m_dwNbBytesRead)
            {
                fCompressed = false;
            }

            // Send the FileTransferMsg with rfbFilePacket
            rfbFileTransferMsg ft;
            ft.type = rfbFileTransfer;
            ft.contentType = rfbFilePacket;
            ft.size = fCompressed ? Swap32IfLE(1) : Swap32IfLE(0);
            ft.length = fCompressed ? Swap32IfLE(nMaxCompSize) : Swap32IfLE(m_dwNbBytesRead);
            //adzm 2010-09
            if (UsingOldProtocol())
            {
                m_pCC->WriteExactQueue(reinterpret_cast<char*>(&ft), sz_rfbFileTransferMsg);
            }
            else
            {
                m_pCC->WriteExactQueue(reinterpret_cast<char*>(&ft), sz_rfbFileTransferMsg, rfbFileTransfer);
            }

            if (fCompressed)
            {
                m_pCC->WriteExact(reinterpret_cast<char*>(m_pCC->m_filezipbuf), static_cast<int>(nMaxCompSize));
            }
            else
            {
                m_pCC->WriteExact(reinterpret_cast<char*>(m_pCC->m_filechunkbuf), static_cast<int>(m_dwNbBytesRead));
            }
        }

        m_dwTotalNbBytesRead += m_dwNbBytesRead;

        // Refresh progress bar periodically
        SetGauge(hWnd, m_dwTotalNbBytesRead);

        // Check if we've been running too long - yield for UI responsiveness
        if (::GetTickCount() - dwStartTime >= dwMaxTimePerCall)
        {
            break;
        }
    } // end while

    return true;
}


bool FileTransfer::FinishFileSending()
{
    //	vnclog.Print(0, _T("FinishSendFile\n"));
    if (!m_fFileUploadRunning)
    {
        return false;
    }

#if DEBUG_FT
    {
        std::array<WCHAR, 512U> _dbg;
        ::_snwprintf_s(_dbg.data(), 512, _TRUNCATE, L"=== FinishFileSending: this=0x%p g_fHeadlessUpload=%d m_szHeadlessRemotePathW=[%s]\n", reinterpret_cast<void*>(this), static_cast<int>(g_fHeadlessUpload), m_szHeadlessRemotePathW.data());
        ::OutputDebugStringW(_dbg.data());
    }
#endif

#if DEBUG_FT
    ::OutputDebugStringW(L"=== FT_UPLOAD: FinishFileSending ===\n");
    {
        std::array<wchar_t, 256U> debugMsg;
        ::_snwprintf_s(debugMsg.data(), 256, _TRUNCATE, L"  m_fFileUploadError=%d, m_fEof=%d, TotalBytes=%I64d\n", m_fFileUploadError, m_fEof, m_dwTotalNbBytesRead);
        ::OutputDebugStringW(debugMsg.data());
    }
#endif

    m_fFileUploadRunning = false;
    m_pCC->SetSendTimeout(0);

    std::array<char, 512 + 256> szStatus;

    ::CloseHandle(m_hSrcFile);

    if (!m_fFileUploadError || m_fEof)
    {
        rfbFileTransferMsg ft;

        ft.type = rfbFileTransfer;
        ft.contentType = rfbEndOfFile;
#if DEBUG_FT
        ::OutputDebugStringW(L"  Sending rfbEndOfFile message to server\n");
#endif
        if (UsingOldProtocol())
        {
            m_pCC->WriteExact(reinterpret_cast<char*>(&ft), sz_rfbFileTransferMsg);
        }
        else
        {
            m_pCC->WriteExact(reinterpret_cast<char*>(&ft), sz_rfbFileTransferMsg, rfbFileTransfer);
        }
#if DEBUG_FT
        ::OutputDebugStringW(L"  rfbEndOfFile message sent successfully\n");
#endif
        // Flush the write queue to ensure rfbEndOfFile is sent before any subsequent messages
        // In headless mode, skip flush to avoid potential iterator crashes during cleanup
#if DEBUG_FT
        ::OutputDebugStringW(L"  About to check headless mode for FlushWriteQueue\n");
        {
            std::array<WCHAR, 256U> _dbg;
            ::_snwprintf_s(_dbg.data(), 256, _TRUNCATE, L"  m_szHeadlessRemotePathW[0]=%d\n", static_cast<int>(m_szHeadlessRemotePathW[0U]));
            ::OutputDebugStringW(_dbg.data());
        }
#endif
        if (!g_fHeadlessUpload)
        {
#if DEBUG_FT
            ::OutputDebugStringW(L"  Calling FlushWriteQueue (non-headless mode)\n");
#endif
            m_pCC->FlushWriteQueue();
#if DEBUG_FT
            ::OutputDebugStringW(L"  Write queue flushed\n");
#endif
        }
        else
        {
#if DEBUG_FT
            ::OutputDebugStringW(L"  Skipping FlushWriteQueue in headless mode\n");
#endif
        }
#if DEBUG_FT
        ::OutputDebugStringW(L"  About to sprintf_s szStatus\n");
#endif
        ::sprintf_s(szStatus.data(), szStatus.size(), " %ls < %s > %ls", sz_H17.data(), m_szSrcFileName.data(), sz_H27.data() /*, (int)((lTotalComp * 100) / dwTotalNbBytesWritten), fCompress ? "C" : "N"*/ /*, szDstFileName*/);
#if DEBUG_FT
        ::OutputDebugStringW(L"  sprintf_s completed\n");
#endif
    }
    else // Error during File Transfer loop
    {
        rfbFileTransferMsg ft;
        ft.type = rfbFileTransfer;
        ft.contentType = rfbAbortFileTransfer;
        ft.contentParam = rfbFileTransferVersion;
        ft.length = 0;
        ft.size = 0;
        if (UsingOldProtocol())
        {
            m_pCC->WriteExact(reinterpret_cast<char*>(&ft), sz_rfbFileTransferMsg);
        }
        else
        {
            m_pCC->WriteExact(reinterpret_cast<char*>(&ft), sz_rfbFileTransferMsg, rfbFileTransfer);
        }
        ::sprintf_s(szStatus.data(), szStatus.size(), " %ls < %s > %ls", sz_H19.data(), m_szSrcFileName.data(), sz_H28.data());
    }

#if DEBUG_FT
    ::OutputDebugStringW(L"  After error/success block, about to check directory zip\n");
#endif
    // If the transfered file is a Directory zip, we delete it locally, whatever the result of the transfer
    if (!std::strncmp(strrchr(m_szSrcFileName.data(), '\\') + 1, rfbZipDirectoryPrefix, std::strlen(rfbZipDirectoryPrefix)))
    {
        std::array<WCHAR, MAX_PATH * 3> szSrcFileNameW;
        ::MultiByteToWideChar(CP_ACP, 0, m_szSrcFileName.data(), -1, szSrcFileNameW.data(), static_cast<int>(szSrcFileNameW.size()));
        ::DeleteFileW(szSrcFileNameW.data());
        if (!m_fFileUploadError)
        {
            // Only update GUI in non-headless mode
            if (!g_fHeadlessUpload)
            {
                std::array<char, MAX_PATH * 3> szDirectoryName;
                char* p = strrchr(m_szSrcFileName.data(), '\\');
                char* p1 = strchr(p, '-');
                ::strcpy_s(szDirectoryName.data(), szDirectoryName.size(), p1 + 1);
                szDirectoryName[std::strlen(szDirectoryName.data()) - 4] = '\0'; // Remove '.zip'
                // sprintf_s(szStatus, " %s < %s > %s - Not really sent: %ld", sz_H66, szDirectoryName, sz_H70, m_nNotSent);
                {
                    std::array<wchar_t, MAX_PATH * 4> _wdn5;
                    ::MultiByteToWideChar(CP_ACP, 0, szDirectoryName.data(), -1, _wdn5.data(), static_cast<int>(_wdn5.size()));
                    std::array<wchar_t, MAX_PATH * 4 + 64> szStatusW13;
                    ::_snwprintf_s(szStatusW13.data(), MAX_PATH * 4 + 64, _TRUNCATE, L" %s < %s > %s", sz_H66.data(), _wdn5.data(), sz_H70.data());
                    SetStatus(szStatusW13.data());
                }
            }
        }
    }
    else
    {
        // Only update GUI in non-headless mode
        if (!g_fHeadlessUpload)
        {
            std::array<wchar_t, 512U> szStatusW13;
            ::_snwprintf_s(szStatusW13.data(), 512, _TRUNCATE, L" (status not set)");
            SetStatus(szStatusW13.data());
        }
    }

#if DEBUG_FT
    ::OutputDebugStringW(L"  After directory/status block, about to check GUI operations\n");
#endif
    // Only perform GUI operations in non-headless mode
    if (!g_fHeadlessUpload)
    {
#if DEBUG_FT
        ::OutputDebugStringW(L"  Performing GUI operations (non-headless)\n");
#endif
        ::UpdateWindow(hWnd);

        // hide the stop button
        ::ShowWindow(::GetDlgItem(hWnd, IDC_ABORT_B), SW_HIDE);
        ::ShowWindow(::GetDlgItem(hWnd, IDC_ABORT_B2), SW_HIDE);
        // Sound notif
        //MessageBeep(-1);

        // Send next file in the list, if any
        OfferNextFile();
    }
    else
    {
        // In headless mode, close dialog after upload completes
        ::Sleep(1000); // Brief delay to show final status

        m_fFileCommandPending = false;
        KillFTTimer();
        g_fHeadlessUpload = false;

        // Close the dialog
        ::EndDialog(hWnd, TRUE);

        // Close the viewer window
        if (m_pCC && m_pCC->m_hwndMain)
        {
            ::PostMessage(m_pCC->m_hwndMain, WM_CLOSE, 0, 0);
        }

        // Disconnect viewer
        ::PostQuitMessage(0);
    }

#if DEBUG_FT
    ::OutputDebugStringW(L"  FinishFileSending about to return\n");
#endif
    // if (nRet) return true; else return false;
    // return !m_fFileUploadError;
    return true;
}



//
// Create all path components of szFullPathW on the remote server.
// e.g. "C:\ProgramData\Microsoft\Windows\Start Menu\Programs\Startup\"
// sends rfbCDirCreate for each successive component.
//
void FileTransfer::CreateRemoteDirectoryPath(const LPCWSTR szFullPathW)
{
    // Work on a mutable copy, strip trailing backslash
    std::array<WCHAR, MAX_PATH * 2> szPath;
    ::wcscpy_s(szPath.data(), szPath.size(), szFullPathW);
    std::size_t nLen = std::wcslen(szPath.data());
    if ((nLen > 0) && (szPath[nLen - 1] == L'\\'))
    {
        szPath[--nLen] = L'\0';
    }

    // Walk each component from root and create progressively deeper dirs.
    // Start after the drive letter (e.g. skip "C:")
    WCHAR* p = szPath.data();
    if ((nLen >= 2) && (szPath[1U] == L':'))
    {
        p = szPath.data() + 2;
    } // skip "C:"

    while ((p = std::wcschr(p + 1, L'\\')) != nullptr)
    {
        *p = L'\0';
        std::array<char, MAX_PATH * 3> szDirUTF8;
        ::WideCharToMultiByte(CP_UTF8, 0, szPath.data(), -1, szDirUTF8.data(), static_cast<int>(szDirUTF8.size()), nullptr, nullptr);
        CreateRemoteDirectory(szDirUTF8.data());
        *p = L'\\';
    }
    // Create the full path itself
    std::array<char, MAX_PATH * 3> szDirUTF8;
    ::WideCharToMultiByte(CP_UTF8, 0, szPath.data(), -1, szDirUTF8.data(), static_cast<int>(szDirUTF8.size()), nullptr, nullptr);
    CreateRemoteDirectory(szDirUTF8.data());
}

// Request the creation of a directory on the remote machine
//
void FileTransfer::CreateRemoteDirectory(const LPSTR szDir)
{
    rfbFileTransferMsg ft;
    ft.type = rfbFileTransfer;
    ft.contentType = rfbCommand;
    ft.contentParam = rfbCDirCreate;
    ft.size = 0;

    // szDir is already UTF-8 when server supports Unicode (caller converts Unicode->UTF-8)
    std::array<char, MAX_PATH * 3> szDirToSend;
    if (m_fServerSupportsUnicode)
    {
        ::strcpy_s(szDirToSend.data(), szDirToSend.size(), szDir);
    }
    else
    {
        // Old server: convert UTF-8 -> Unicode -> CP_ACP
        std::array<WCHAR, MAX_PATH> szDirW;
        ::MultiByteToWideChar(CP_UTF8, 0, szDir, -1, szDirW.data(), static_cast<int>(szDirW.size()));
        ::WideCharToMultiByte(CP_ACP, 0, szDirW.data(), -1, szDirToSend.data(), static_cast<int>(szDirToSend.size()), nullptr, nullptr);
    }

    ft.length = Swap32IfLE(std::strlen(szDirToSend.data()));
    //adzm 2010-09
    m_pCC->WriteExactQueue(reinterpret_cast<char*>(&ft), sz_rfbFileTransferMsg, rfbFileTransfer);
    m_pCC->WriteExact(reinterpret_cast<char*>(szDirToSend.data()), static_cast<int>(std::strlen(szDirToSend.data())));
    return;
}


//
// Request the deletion of a file on the remote machine
//
void FileTransfer::DeleteRemoteFile(const std::string szFile)
{
    rfbFileTransferMsg ft;
    ft.type = rfbFileTransfer;
    ft.contentType = rfbCommand;
    ft.contentParam = rfbCFileDelete;
    ft.size = 0;

    // szFile is already UTF-8 when server supports Unicode (caller converts Unicode->UTF-8)
    std::array<char, MAX_PATH * 3> szFileToSend;
    if (m_fServerSupportsUnicode)
    {
        ::strcpy_s(szFileToSend.data(), szFileToSend.size(), szFile.c_str());
    }
    else
    {
        // Old server: convert UTF-8 -> Unicode -> CP_ACP
        std::array<WCHAR, MAX_PATH> szFileW;
        ::MultiByteToWideChar(CP_UTF8, 0, szFile.c_str(), -1, szFileW.data(), static_cast<int>(szFileW.size()));
        ::WideCharToMultiByte(CP_ACP, 0, szFileW.data(), -1, szFileToSend.data(), static_cast<int>(szFileToSend.size()), nullptr, nullptr);
    }

    std::size_t len = std::strlen(szFileToSend.data());
    ft.length = Swap32IfLE(len);
    //adzm 2010-09
    m_pCC->WriteExactQueue(reinterpret_cast<char*>(&ft), sz_rfbFileTransferMsg, rfbFileTransfer);
    m_pCC->WriteExact(reinterpret_cast<char*>(szFileToSend.data()), static_cast<int>(len));
    return;
}

//
// Request the renaming of a file or a directory
//
void FileTransfer::RenameRemoteFileOrDirectory(const LPSTR szCurrentName, const LPSTR szNewName)
{
    // szCurrentName and szNewName are already UTF-8 when server supports Unicode
    std::array<char, (2 * MAX_PATH * 3) + 1> szMsgContentToSend;
    if (m_fServerSupportsUnicode)
    {
        // Already UTF-8, concatenate directly
        if (std::strlen(szCurrentName) + std::strlen(szNewName) + 2 > sizeof(szMsgContentToSend))
        {
            return;
        }
        ::sprintf_s(szMsgContentToSend.data(), szMsgContentToSend.size(), "%s*%s", szCurrentName, szNewName);
    }
    else
    {
        // Old server: convert UTF-8 -> Unicode -> CP_ACP, then build message
        if ((std::strlen(szCurrentName) > MAX_PATH) || (std::strlen(szNewName) > MAX_PATH))
        {
            return;
        }
        std::array<char, MAX_PATH> szCurrentACP;
        std::array<char, MAX_PATH> szNewACP;
        std::array<WCHAR, MAX_PATH> szW;
        ::MultiByteToWideChar(CP_UTF8, 0, szCurrentName, -1, szW.data(), static_cast<int>(szW.size()));
        ::WideCharToMultiByte(CP_ACP, 0, szW.data(), -1, szCurrentACP.data(), static_cast<int>(szCurrentACP.size()), nullptr, nullptr);
        ::MultiByteToWideChar(CP_UTF8, 0, szNewName, -1, szW.data(), static_cast<int>(szW.size()));
        ::WideCharToMultiByte(CP_ACP, 0, szW.data(), -1, szNewACP.data(), static_cast<int>(szNewACP.size()), nullptr, nullptr);
        ::sprintf_s(szMsgContentToSend.data(), szMsgContentToSend.size(), "%s*%s", szCurrentACP.data(), szNewACP.data());
    }

    rfbFileTransferMsg ft;
    ft.type = rfbFileTransfer;
    ft.contentType = rfbCommand;
    ft.contentParam = rfbCFileRename; // or rfbCDirRename ...
    ft.size = 0;
    ft.length = Swap32IfLE(std::strlen(szMsgContentToSend.data()));
    //adzm 2010-09
    m_pCC->WriteExactQueue(reinterpret_cast<char*>(&ft), sz_rfbFileTransferMsg, rfbFileTransfer);
    m_pCC->WriteExact(reinterpret_cast<char*>(szMsgContentToSend.data()), static_cast<int>(std::strlen(szMsgContentToSend.data())));
    return;
}

//
// Server's response to a directory creation command
//
bool FileTransfer::CreateRemoteDirectoryFeedback(const long lSize, const UINT nLen)
{
    if (!ViewerBounds::payload(nLen))
    {
        throw ErrorException(L"Invalid remote filename length.");
    }
    char* szRemoteName = new char[nLen + 1];
    if (szRemoteName == nullptr)
    {
        return false;
    }
    std::memset(szRemoteName, 0, nLen + 1);
    m_pCC->ReadExact(szRemoteName, static_cast<int>(nLen));

    szRemoteName[nLen] = '\0';

    {
        std::array<wchar_t, MAX_PATH * 4> _wrn4;
        ::MultiByteToWideChar(CP_UTF8, 0, szRemoteName, -1, _wrn4.data(), static_cast<int>(_wrn4.size()));
        std::array<wchar_t, MAX_PATH * 4 + 64> szStatusW14;
        if (lSize == -1)
        {
            ::_snwprintf_s(szStatusW14.data(), szStatusW14.size(), _TRUNCATE, L"%s < %s > %s", sz_H29.data(), _wrn4.data(), sz_H30.data());
            SetStatus(szStatusW14.data());
            delete[] szRemoteName;
            return false;
        }

        ::_snwprintf_s(szStatusW14.data(), szStatusW14.size(), _TRUNCATE, L"%s < %s > %s", sz_H31.data(), _wrn4.data(), sz_H32.data());
        SetStatus(szStatusW14.data());
    }
    // Refresh the remote list
    FTListViewClear(::GetDlgItem(hWnd, IDC_REMOTE_FILELIST));
    RequestRemoteDirectoryContent(hWnd, L"");

    delete[] szRemoteName;
    return true;
}


//
// Server's response to a File Deletion command
//
bool FileTransfer::DeleteRemoteFileFeedback(const long lSize, const UINT nLen)
{
    if (!ViewerBounds::payload(nLen))
    {
        throw ErrorException(L"Invalid remote filename length.");
    }
    char* szRemoteName = new char[nLen + 1];
    if (szRemoteName == nullptr)
    {
        return false;
    }
    std::memset(szRemoteName, 0, nLen + 1);
    m_pCC->ReadExact(szRemoteName, static_cast<int>(nLen));

    szRemoteName[nLen] = '\0';

    std::array<wchar_t, MAX_PATH * 4> szDisplayNameW{};
    UINT cp = m_fServerSupportsUnicode ? CP_UTF8 : CP_ACP;
    ::MultiByteToWideChar(cp, 0, szRemoteName, -1, szDisplayNameW.data(), static_cast<int>(szDisplayNameW.size()));

    std::array<wchar_t, MAX_PATH * 4 + 256> szStatusW;
    bool isDir = IsDirectoryGetIt(szRemoteName, static_cast<int>(nLen + 1));
    if (lSize == -1)
    {
        ::_snwprintf_s(szStatusW.data(), MAX_PATH * 4 + 256, _TRUNCATE, L"%s < %s > %s", isDir ? sz_H99.data() : sz_H33.data(), szDisplayNameW.data(), sz_H30.data());
        SetStatus(szStatusW.data());
        delete[] szRemoteName;
        return false;
    }
    ::_snwprintf_s(szStatusW.data(), MAX_PATH * 4 + 256, _TRUNCATE, L"%s < %s > %s", isDir ? sz_H31.data() : sz_H17.data(), szDisplayNameW.data(), sz_H34.data());
    SetStatus(szStatusW.data());
    // Refresh the remote list
    if (--m_nDeleteCount == 0)
    {
        FTListViewClear(::GetDlgItem(hWnd, IDC_REMOTE_FILELIST));
        RequestRemoteDirectoryContent(hWnd, L"");
    }

    delete[] szRemoteName;
    return true;
}


//
// Server's response to a File Deletion command
//
bool FileTransfer::RenameRemoteFileOrDirectoryFeedback(const long lSize, const UINT nLen)
{
    if (nLen <= 0)
    {
        return false;
    }

    if (!ViewerBounds::payload(nLen))
    {
        throw ErrorException(L"Invalid remote filename length.");
    }
    char* szContent = new char[nLen + 1];
    if (szContent == nullptr)
    {
        return false;
    }
    std::memset(szContent, 0, nLen + 1);
    m_pCC->ReadExact(szContent, static_cast<int>(nLen));
    szContent[nLen] = '\0';

    std::array<wchar_t, MAX_PATH * 4 + 200> szStatusW2;

    char* p = strrchr(szContent, '*');
    if (p == nullptr)
    {
        ::_snwprintf_s(szStatusW2.data(), szStatusW2.size(), _TRUNCATE, L" %s < selected file > %s", sz_M5.data(), sz_H30.data());
        SetStatus(szStatusW2.data());
        delete[] szContent;
        return false;
    }

    std::array<char, MAX_PATH * 3 + 1> szOldName;
    std::array<char, MAX_PATH * 3 + 1> szCurrentName;

    ::strcpy_s(szCurrentName.data(), szCurrentName.size(), p + 1);
    *p = '\0';
    ::strcpy_s(szOldName.data(), szOldName.size(), szContent);

    {
        std::array<wchar_t, MAX_PATH * 4> _won;
        std::array<wchar_t, MAX_PATH * 4> _wcn;
        ::MultiByteToWideChar(CP_UTF8, 0, szOldName.data(), -1, _won.data(), static_cast<int>(_won.size()));
        ::MultiByteToWideChar(CP_UTF8, 0, szCurrentName.data(), -1, _wcn.data(), static_cast<int>(_wcn.size()));
        if (lSize == -1)
        {
            ::_snwprintf_s(szStatusW2.data(), szStatusW2.size(), _TRUNCATE, L" %s < %s > %s", sz_M5.data(), _won.data(), sz_H30.data());
            SetStatus(szStatusW2.data());
            delete[] szContent;
            return false;
        }

        ::_snwprintf_s(szStatusW2.data(), szStatusW2.size(), _TRUNCATE, L" %s < %s > %s < %s >", sz_M8.data(), _won.data(), sz_M7.data(), _wcn.data());
        SetStatus(szStatusW2.data());
    }
    // Refresh the remote list
    FTListViewClear(::GetDlgItem(hWnd, IDC_REMOTE_FILELIST));
    RequestRemoteDirectoryContent(hWnd, L"");

    delete[] szContent;
    return true;
}


//
// sf@2004 - Delta Transfer
// Create the checksums buffer of an open file
//
int FileTransfer::GenerateFileChecksums(const HANDLE hFile, char* const lpCSBuffer, const int nCSBufferSize)
{
    bool fEof{ false };
    bool fError{ false };
    DWORD dwNbBytesRead{ 0U };
    int nCSBufferOffset{ 0 };

    char* lpBuffer = new char[m_nBlockSize];
    if (!lpBuffer)
    {
        return -1;
    }

    while (!fEof)
    {
        if (!::ReadFile(hFile, lpBuffer, m_nBlockSize, &dwNbBytesRead, nullptr))
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


bool FileTransfer::IsDirectoryGetItW(WCHAR* const szName, const int size)
{
    std::array<WCHAR, MAX_PATH> szWork;
    ::wcscpy_s(szWork.data(), szWork.size(), szName);
    // Check if starts with "[ " (bracket prefix)
    if ((szWork[0U] == L'[') && (szWork[1U] == L' '))
    {
        std::size_t len = std::wcslen(szWork.data());
        if (len > 4) // Must have at least "[ x ]"
        {
            ::wcsncpy_s(szName, static_cast<rsize_t>(size), szWork.data() + 2, len - 3);
            szName[len - 4] = L'\0';
            return true;
        }
    }
    return false;
}

// Legacy ANSI version
bool FileTransfer::IsDirectoryGetIt(char* const szName, const int size)
{
    std::array<char, MAX_PATH * 3> szWork;
    ::strcpy_s(szWork.data(), szWork.size(), szName);
    if ((szWork[0U] == rfbDirPrefix[0U]) && (szWork[1U] == rfbDirPrefix[1U]))
    {
        ::strncpy_s(szName, static_cast<rsize_t>(size), szWork.data() + 2, std::strlen(szWork.data()) - 3);
        szName[std::strlen(szWork.data()) - 4] = '\0';
        return true;
    }
    return false;
}

int FileTransfer::DoDialog()
{
    extern HINSTANCE m_hInstResDLL;
    return static_cast<int>(::DialogBoxParam(m_hInstResDLL, DIALOG_MAKEINTRESOURCE(IDD_FILETRANSFER_DLG), nullptr, reinterpret_cast<DLGPROC>(FileTransferDlgProc),
                          reinterpret_cast<LONG_PTR>(this)));
}


//
// 2006 - Resizable File Transfer Window mod - By Roytam1 & and KP774
//
void FTAdjustLeft(const LPRECT lprc)
{
    int cx = lprc->right - lprc->left - GetSystemMetrics(SM_CXSIZEFRAME) * 2;
    if (cx < 611)
    {
        lprc->left = lprc->right - 611 - GetSystemMetrics(SM_CXSIZEFRAME) * 2;
    }
}


void FTAdjustTop(const LPRECT lprc)
{
    int cy = lprc->bottom - lprc->top - GetSystemMetrics(SM_CYSIZEFRAME) * 2;
    if (cy < 429)
    {
        lprc->top = lprc->bottom - 429 - GetSystemMetrics(SM_CYSIZEFRAME) * 2;
    }
}



void FTAdjustRight(const LPRECT lprc)
{
    int cx = lprc->right - lprc->left - GetSystemMetrics(SM_CXSIZEFRAME) * 2;
    if (cx < 611)
    {
        lprc->right = lprc->left + 611 + GetSystemMetrics(SM_CXSIZEFRAME) * 2;
    }
}



void FTAdjustBottom(const LPRECT lprc)
{
    int cy = lprc->bottom - lprc->top - GetSystemMetrics(SM_CYSIZEFRAME) * 2;
    if (cy < 429)
    {
        lprc->bottom = lprc->top + 429 + GetSystemMetrics(SM_CYSIZEFRAME) * 2;
    }
}


// sf@2006
void FTAdjustFileNameColumns(const HWND hWnd)
{

    RECT rc;
    ::GetWindowRect(::GetDlgItem(hWnd, IDC_LOCAL_FILELIST), &rc);
    int w = rc.right - rc.left;
    int cw = w - (70 + 155 + 25);
    if (cw < 120)
    {
        cw = 120;
    }

    LVCOLUMN Column;
    Column.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_ORDER | LVCF_SUBITEM;
    Column.fmt = LVCFMT_LEFT;
    Column.cx = cw;
    Column.pszText = reinterpret_cast<LPWSTR>(const_cast<wchar_t *>(L"Name"));
    Column.iSubItem = 0;
    Column.iOrder = 0;

    HWND hWndList = ::GetDlgItem(hWnd, IDC_LOCAL_FILELIST);
    ListView_SetColumn(hWndList, 0, &Column);
    hWndList = ::GetDlgItem(hWnd, IDC_REMOTE_FILELIST);
    ListView_SetColumn(hWndList, 0, &Column);
}

//
// Subclass proc for IDC_CURR_LOCAL and IDC_CURR_REMOTE RichEdit path bars.
// Intercepts Enter key to trigger navigation to the typed/pasted path.
//
static LRESULT CALLBACK PathEditSubclassProc(const HWND hEdit, const UINT uMsg, WPARAM wParam, const LPARAM lParam,
                                             const UINT_PTR uIdSubclass, [[maybe_unused]] const DWORD_PTR dwRefData)
{
    if (uMsg == WM_GETDLGCODE)
    {
        // Claim Enter key so IsDialogMessage doesn't swallow it; leave Tab alone
        LRESULT lr = ::DefSubclassProc(hEdit, uMsg, wParam, lParam);
        MSG* pMsg = reinterpret_cast<MSG*>(lParam);
        if (pMsg && (pMsg->message == WM_KEYDOWN) && (pMsg->wParam == VK_RETURN))
        {
            return lr | DLGC_WANTMESSAGE;
        }
        return lr;
    }
    if ((uMsg == WM_KEYDOWN) && (wParam == VK_RETURN))
    {
        HWND hDlg = ::GetParent(hEdit);
        FileTransfer* _this = reinterpret_cast<FileTransfer*>(helper::SafeGetWindowUserData<FileTransfer>(hDlg));
        if (_this && !_this->m_fFileCommandPending)
        {
            std::array<wchar_t, MAX_PATH + 1> szPath;
            ::GetWindowTextW(hEdit, szPath.data(), MAX_PATH);
            szPath[MAX_PATH] = L'\0';
            // Ensure trailing backslash
            std::size_t len = std::wcslen(szPath.data());
            if ((len > 0) && (szPath[len - 1] != L'\\') && (len < MAX_PATH))
            {
                szPath[len] = L'\\';
                szPath[len + 1] = L'\0';
            }
            if (uIdSubclass == IDC_CURR_LOCAL)
            {
                ::SetDlgItemTextW(hDlg, IDC_CURR_LOCAL, szPath.data());
                _this->PopulateLocalListBoxW(hDlg, L"");
            }
            else // IDC_CURR_REMOTE
            {
                _this->m_fFileCommandPending = true;
                ::SetWindowTextW(hEdit, szPath.data());
                _this->RequestRemoteDirectoryContent(hDlg, L"");
            }
        }
        return 0; // swallow Enter
    }
    return ::DefSubclassProc(hEdit, uMsg, wParam, lParam);
}

//
//
//
BOOL CALLBACK FileTransfer::FileTransferDlgProc(const HWND hWnd, const UINT uMsg, WPARAM wParam, const LPARAM lParam)
{
    // This is a static method, so we don't know which instantiation we're
    // dealing with. But we can get a pseudo-this from the parameter to
    // WM_INITDIALOG, which we therafter store with the window and retrieve
    // as follows:
    FileTransfer* _this = helper::SafeGetWindowUserData<FileTransfer>(hWnd);

    switch (uMsg)
    {
    case WM_TIMER: {
        // We have to wait for NetBuf flush
        // !!!: Not necessary anymore - Pb solved on server side.
        // DWORD lTime = timeGetTime();
        // DWORD lLastTime = _this->m_pCC->m_lLastRfbRead;
        // DWORD lDelta = abs(timeGetTime() - _this->m_pCC->m_lLastRfbRead);
        if (true && (wParam != 100) /*meGetTime() - _this->m_pCC->m_lLastRfbRead) > 1000 */)
        {
            _this->m_fFileCommandPending = true;
            _this->RequestPermission();
            if (::KillTimer(hWnd, static_cast<UINT_PTR>(_this->m_timer)))
            {
                _this->m_timer = 0;
            }
        }
        else
        {
            _this->m_pCC->SendKeepAlive(false, true);
        }
        break;
    }

    case WM_INITDIALOG: {
        HICON hIcon = ::LoadIcon(::GetModuleHandle(nullptr), MAKEINTRESOURCE(IDR_TRAY));
        ::SendMessage(hWnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(hIcon));
        ::SendMessage(hWnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(hIcon));
        helper::SafeSetWindowUserData(hWnd, lParam);

        FileTransfer* l_this = reinterpret_cast<FileTransfer*>(lParam);
        ::SetTimer(hWnd, 100, 5000, nullptr);
        // CentreWindow(hWnd);
        l_this->hWnd = hWnd;
        hFTWnd = hWnd;

        // Window always on top if Fullscreen On
        if (l_this->m_pCC->InFullScreenMode())
        {
            RECT Rect;
            ::GetWindowRect(hWnd, &Rect);
            ::SetWindowPos(hWnd, HWND_TOPMOST, Rect.left, Rect.top, Rect.right - Rect.left, Rect.bottom - Rect.top, SWP_SHOWWINDOW);
        }

        ::SetForegroundWindow(hWnd);

        // Set the title
        const long lTitleBufSize{ 256 };
        std::array<wchar_t, lTitleBufSize> szRemoteNameW;
        std::array<wchar_t, lTitleBufSize> szTitleW;
        ::_tcsncpy_s(szRemoteNameW.data(), szRemoteNameW.size(), l_this->m_pCC->m_desktopName, _TRUNCATE);
        szRemoteNameW[lTitleBufSize - 1] = L'\0';
        ::_snwprintf_s(szTitleW.data(), szTitleW.size(), _TRUNCATE, L" %s < %s>  -  UltraVNC", sz_H35.data(), szRemoteNameW.data());
        ::SetWindowTextW(hWnd, szTitleW.data());

        // Create all the columns of the Files ListViews
        LVCOLUMN Column;
        Column.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_ORDER | LVCF_SUBITEM;
        Column.fmt = LVCFMT_LEFT;
        Column.cx = 166;
        Column.pszText = reinterpret_cast<LPWSTR>(const_cast<wchar_t *>(L"Name"));
        Column.iSubItem = 0;
        Column.iOrder = 0;
        HWND hWndList = ::GetDlgItem(hWnd, IDC_LOCAL_FILELIST);
        ListView_InsertColumn(hWndList, 0, &Column);
        hWndList = ::GetDlgItem(hWnd, IDC_REMOTE_FILELIST);
        ListView_InsertColumn(hWndList, 0, &Column);

        Column.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_ORDER | LVCF_SUBITEM;
        Column.fmt = LVCFMT_LEFT;
        Column.cx = 70;
        Column.pszText = reinterpret_cast<LPWSTR>(const_cast<wchar_t *>(L"Size"));
        Column.iSubItem = 1;
        Column.iOrder = 1;
        hWndList = ::GetDlgItem(hWnd, IDC_LOCAL_FILELIST);
        ListView_InsertColumn(hWndList, 1, &Column);
        hWndList = ::GetDlgItem(hWnd, IDC_REMOTE_FILELIST);
        ListView_InsertColumn(hWndList, 1, &Column);

        Column.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_ORDER | LVCF_SUBITEM;
        Column.fmt = LVCFMT_LEFT;
        Column.cx = 155;
        Column.pszText = reinterpret_cast<LPWSTR>(const_cast<wchar_t *>(L"Modified"));
        Column.iSubItem = 2;
        Column.iOrder = 2;
        hWndList = ::GetDlgItem(hWnd, IDC_LOCAL_FILELIST);
        ListView_InsertColumn(hWndList, 2, &Column);
        hWndList = ::GetDlgItem(hWnd, IDC_REMOTE_FILELIST);
        ListView_InsertColumn(hWndList, 2, &Column);

        // Make the selection bar full width in the ListViews
        hWndList = ::GetDlgItem(hWnd, IDC_LOCAL_FILELIST);
        ListView_SetExtendedListViewStyleEx(hWndList, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);
        hWndList = ::GetDlgItem(hWnd, IDC_REMOTE_FILELIST);
        ListView_SetExtendedListViewStyleEx(hWndList, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

        // Create Icons List of ListViews
        l_this->InitListViewImagesList(::GetDlgItem(hWnd, IDC_LOCAL_FILELIST));
        l_this->InitListViewImagesList(::GetDlgItem(hWnd, IDC_REMOTE_FILELIST));

        // Create the status bar
        [[maybe_unused]] HWND hStatusBar = ::CreateStatusWindowW(WS_VISIBLE | WS_CHILD, sz_H36.data(), hWnd, IDC_STATUS);

        // Populate the Local listboxes with local drives
        l_this->ListDrives(hWnd);

        // adzm 2009-08-02 - Still list drives above in case this is incorrect, also to populate the dropdown combo
        if (lstrlenA(l_this->m_szLastLocalPath.data()) > 0)
        {
            std::array<WCHAR, MAX_PATH> szLastLocalPathW;
            ::MultiByteToWideChar(CP_UTF8, 0, l_this->m_szLastLocalPath.data(), -1, szLastLocalPathW.data(), static_cast<int>(szLastLocalPathW.size()));
            if (::GetFileAttributesW(szLastLocalPathW.data()) & FILE_ATTRIBUTE_DIRECTORY)
            {
                // let's try to use the last path
                l_this->PopulateLocalListBox(hWnd, l_this->m_szLastLocalPath.data()); //PGM
            }
        }

        // Populate the remote listboxes with remote drives if allowed by the server
        if (false /*!_this->m_pCC->m_pEncrypt->IsEncryptionEnabled()*/)
        {
            /*
            if (!_this->m_fFileCommandPending)
            {
                _this->m_fFileCommandPending = true;
                // RequestRemoteDrives(_this);
                RequestPermission(_this);
                SetStatus(_this, " Connecting. Please Wait...");
            }
            */
        }
        else
        {
            l_this->m_timer = static_cast<int>(::SetTimer(hWnd, 3333, 1000, nullptr));
            l_this->SetStatus(sz_H37.data());
        }

        // Subclass path edit controls so Enter key navigates to typed/pasted path
        ::SetWindowSubclass(::GetDlgItem(hWnd, IDC_CURR_LOCAL), PathEditSubclassProc, IDC_CURR_LOCAL, 0);
        ::SetWindowSubclass(::GetDlgItem(hWnd, IDC_CURR_REMOTE), PathEditSubclassProc, IDC_CURR_REMOTE, 0);

        // Save original (translated) button labels as wide strings
        if (l_this->m_szDeleteButtonLabel[0U] == L'\0')
        {
            ::GetWindowTextW(::GetDlgItem(hWnd, IDC_DELETE_B), l_this->m_szDeleteButtonLabel.data(), 64);
        }
        if (l_this->m_szNewFolderButtonLabel[0U] == L'\0')
        {
            ::GetWindowTextW(::GetDlgItem(hWnd, IDC_NEWFOLDER_B), l_this->m_szNewFolderButtonLabel.data(), 64);
        }
        if (l_this->m_szRenameButtonLabel[0U] == L'\0')
        {
            ::GetWindowTextW(::GetDlgItem(hWnd, IDC_RENAME_B), l_this->m_szRenameButtonLabel.data(), 64);
        }
        if (l_this->m_szRefreshButtonLabel[0U] == L'\0')
        {
            ::GetWindowTextW(::GetDlgItem(hWnd, IDC_REFRESH), l_this->m_szRefreshButtonLabel.data(), 64);
        }

        l_this->CheckButtonState(hWnd);

        // Mark headless mode if command-line upload parameters present
        if ((l_this->m_pCC->m_opts->m_szUploadLocal[0U] != L'\0') && (l_this->m_pCC->m_opts->m_szUploadRemote[0U] != L'\0'))
        {
            g_fHeadlessUpload = true;
        }

        return TRUE;
    }
    break;

    case WM_APP + 1:
        // Auto-setup FT dialog from command line paths, then trigger upload
        {
            FileTransfer* _thisInner = helper::SafeGetWindowUserData<FileTransfer>(hWnd);
            if (!_thisInner)
            {
                return 0;
            }
            LPCWSTR szLocalPath = _thisInner->m_pCC->m_opts->m_szUploadLocal.data();
            LPCWSTR szRemotePath = _thisInner->m_pCC->m_opts->m_szUploadRemote.data();
            if ((szLocalPath[0U] == L'\0') || (szRemotePath[0U] == L'\0'))
            {
                return 0;
            }

            // Verify local file exists
            if (::GetFileAttributesW(szLocalPath) == INVALID_FILE_ATTRIBUTES)
            {
                std::array<WCHAR, 512U> szError;
                ::_snwprintf_s(szError.data(), 512, _TRUNCATE, L"Local file not found:\n%s", szLocalPath);
                ::MessageBoxW(hWnd, szError.data(), L"Upload Error", MB_OK | MB_ICONERROR);
                g_fHeadlessUpload = false;
                ::EndDialog(hWnd, FALSE);
                ::PostQuitMessage(1);
                return 0;
            }

            // Minimize viewer window NOW (after dialog is shown)
            if (_thisInner->m_pCC->m_hwndMain)
            {
                ::ShowWindow(_thisInner->m_pCC->m_hwndMain, SW_MINIMIZE);
            }

            // Extract local folder and filename
            std::array<WCHAR, MAX_PATH * 2> szLocalDir;
            std::array<WCHAR, MAX_PATH> szFileName;
            ::wcscpy_s(szLocalDir.data(), szLocalDir.size(), szLocalPath);
            WCHAR* pSlash = std::wcsrchr(szLocalDir.data(), L'\\');
            if (pSlash)
            {
                ::wcscpy_s(szFileName.data(), szFileName.size(), pSlash + 1);
                *(pSlash + 1) = L'\0'; // Keep trailing backslash in dir
            }
            else
            {
                ::wcscpy_s(szFileName.data(), szFileName.size(), szLocalPath);
                szLocalDir[0U] = L'.';
                szLocalDir[1U] = L'\\';
                szLocalDir[2U] = L'\0';
            }

            // Extract remote folder (without trailing backslash for creation)
            std::array<WCHAR, MAX_PATH * 2> szRemoteDir;
            std::array<WCHAR, MAX_PATH * 2> szRemoteDisplay;
            ::wcscpy_s(szRemoteDir.data(), szRemoteDir.size(), szRemotePath);
            pSlash = std::wcsrchr(szRemoteDir.data(), L'\\');
            if (pSlash)
            {
                *pSlash = L'\0';                                                 // Remove backslash for clean dir path
                ::swprintf_s(szRemoteDisplay.data(), szRemoteDisplay.size(), L"%s\\", szRemoteDir.data()); // Add back for display
            }
            else
            {
                szRemoteDir[0U] = L'\0';
                ::wcscpy_s(szRemoteDisplay.data(), szRemoteDisplay.size(), szRemotePath);
            }
            // Set local path and populate list
            ::SetDlgItemTextW(hWnd, IDC_CURR_LOCAL, szLocalDir.data());
            _thisInner->PopulateLocalListBoxW(hWnd, szLocalDir.data());

            // Find and select the file in the local list
            HWND hWndLocalList = ::GetDlgItem(hWnd, IDC_LOCAL_FILELIST);
            int nCount = ListView_GetItemCount(hWndLocalList);
            for (int i{ 0 }; i < nCount; ++i)
            {
                LVITEMW ItemW;
                std::memset(&ItemW, 0, sizeof(ItemW));
                ItemW.mask = LVIF_PARAM;
                ItemW.iItem = i;
                ::SendMessageW(hWndLocalList, LVM_GETITEMW, 0, reinterpret_cast<LPARAM>(&ItemW));
                std::wstring* pNameW = reinterpret_cast<std::wstring*>(ItemW.lParam & ~static_cast<LPARAM>(FT_LPARAM_UNREADABLE));
                if (pNameW && (::_wcsicmp(pNameW->c_str(), szFileName.data()) == 0))
                {
                    ListView_SetItemState(hWndLocalList, i, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED)
                    ListView_EnsureVisible(hWndLocalList, i, FALSE);
                    break;
                }
            }

            // Set remote path display (with backslash)
            ::SetDlgItemTextW(hWnd, IDC_CURR_REMOTE, szRemoteDisplay.data());

            // Create remote directory if it doesn't exist
            if (szRemoteDir[0U] != L'\0')
            {
                _thisInner->CreateRemoteDirectoryPath(szRemoteDir.data());

                // Wait for directory creation
                DWORD dwStart = ::GetTickCount();
                while (_thisInner->m_fFileCommandPending && (::GetTickCount() - dwStart < 3000))
                {
                    MSG msg;
                    while (::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
                    {
                        ::TranslateMessage(&msg);
                        ::DispatchMessage(&msg);
                    }
                    ::Sleep(50);
                }
            }

            // Navigate to remote folder and wait for response
            _thisInner->m_fFileCommandPending = true;
            _thisInner->RequestRemoteDirectoryContent(hWnd, szRemoteDisplay.data());

            // Wait for remote directory content to arrive
            DWORD dwStart = ::GetTickCount();
            while (_thisInner->m_fFileCommandPending && (::GetTickCount() - dwStart < 5000))
            {
                MSG msg;
                while (::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
                {
                    ::TranslateMessage(&msg);
                    ::DispatchMessage(&msg);
                }
                ::Sleep(50);
            }

            // Trigger upload
            _thisInner->CheckButtonState(hWnd);
            ::EnableWindow(::GetDlgItem(hWnd, IDC_UPLOAD_B), TRUE);
            ::PostMessage(hWnd, WM_COMMAND, MAKEWPARAM(IDC_UPLOAD_B, BN_CLICKED), reinterpret_cast<LPARAM>(::GetDlgItem(hWnd, IDC_UPLOAD_B)));
        }
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
            return TRUE;

        case IDCANCEL:
            _this->EndFTSession();
            ::EndDialog(hWnd, FALSE);
            return TRUE;

        // Send selected files
        case IDC_UPLOAD_B: {
            ::OutputDebugStringW(L"IDC_UPLOAD_B: handler entered\n");
            if (_this->m_fFileCommandPending)
            {
                ::OutputDebugStringW(L"IDC_UPLOAD_B: FileCommandPending, breaking\n");
                break;
            }

            int nSelected = -1;
            int nCount{ 0 };
            HWND hWndLocalList = ::GetDlgItem(hWnd, IDC_LOCAL_FILELIST);
            HWND hWndRemoteList = ::GetDlgItem(hWnd, IDC_REMOTE_FILELIST);

            // If no destination is set, nothing to do.
            std::array<WCHAR, MAX_PATH + 32> szDstFileW;
            ::GetDlgItemTextW(hWnd, IDC_CURR_REMOTE, szDstFileW.data(), MAX_PATH + 32);
            {
                std::array<WCHAR, 256U> dbg;
                ::_snwprintf_s(dbg.data(), 256, _TRUNCATE, L"IDC_UPLOAD_B: szDstFileW=[%s] len=%d\n", szDstFileW.data(), static_cast<int>(std::wcslen(szDstFileW.data())));
                ::OutputDebugStringW(dbg.data());
            }
            if (!std::wcslen(szDstFileW.data()))
            {
                ::OutputDebugStringW(L"IDC_UPLOAD_B: empty dest, breaking\n");
                break;
            }

            // Build [..] mask for up-dir detection
            std::array<WCHAR, 16U> szUpDirMaskW;
            std::array<WCHAR, 4U> szPrefixW;
            std::array<WCHAR, 4U> szSuffixW;
            ::MultiByteToWideChar(CP_ACP, 0, rfbDirPrefix, -1, szPrefixW.data(), 4);
            ::MultiByteToWideChar(CP_ACP, 0, rfbDirSuffix, -1, szSuffixW.data(), 4);
            ::swprintf_s(szUpDirMaskW.data(), szUpDirMaskW.size(), L"%s..%s", szPrefixW.data(), szSuffixW.data());

            // Get all selected files, check if they already exist on remote side
            _this->m_FilesList.clear();
            nCount = ListView_GetItemCount(hWndLocalList);
            {
                std::array<WCHAR, 256U> dbg;
                ::_snwprintf_s(dbg.data(), 256, _TRUNCATE, L"IDC_UPLOAD_B: nCount=%d\n", nCount);
                ::OutputDebugStringW(dbg.data());
            }
            _this->m_nConfirmAnswer = CONFIRM_YES;
            int nSelCount{ 0 };
            for (nSelected = 0; nSelected < nCount; ++nSelected)
            {
                if (ListView_GetItemState(hWndLocalList, nSelected, LVIS_SELECTED) & LVIS_SELECTED)
                {
                    ++nSelCount;
                    // Get Unicode filename from lParam
                    LVITEMW ItemW;
                    std::memset(&ItemW, 0, sizeof(ItemW));
                    ItemW.mask = LVIF_PARAM;
                    ItemW.iItem = nSelected;
                    ::SendMessageW(hWndLocalList, LVM_GETITEMW, 0, reinterpret_cast<LPARAM>(&ItemW));
                    std::wstring* pNameW = reinterpret_cast<std::wstring*>(ItemW.lParam & ~static_cast<LPARAM>(FT_LPARAM_UNREADABLE));
                    if (!pNameW)
                    {
                        ::OutputDebugStringW(L"IDC_UPLOAD_B: pNameW is NULL, skipping\n");
                        continue;
                    }
                    const WCHAR* szSelW = pNameW->c_str();
                    {
                        std::array<WCHAR, 256U> dbg;
                        ::_snwprintf_s(dbg.data(), 256, _TRUNCATE, L"IDC_UPLOAD_B: selected item %d=[%s]\n", nSelected, szSelW);
                        ::OutputDebugStringW(dbg.data());
                    }

                    // Display text is "[ name ]" for folders (lParam holds the raw name)
                    std::array<WCHAR, MAX_PATH + 8> szDispW{ 0 };
                    {
                        LVITEMW _di;
                        std::memset(&_di, 0, sizeof(_di));
                        _di.mask = LVIF_TEXT;
                        _di.iItem = nSelected;
                        _di.pszText = szDispW.data();
                        _di.cchTextMax = MAX_PATH + 8;
                        ::SendMessageW(hWndLocalList, LVM_GETITEMTEXTW, static_cast<WPARAM>(nSelected), reinterpret_cast<LPARAM>(&_di));
                    }
                    if (::_wcsicmp(szDispW.data(), szUpDirMaskW.data()) != 0)
                    {
                        bool fDirectory = ((szDispW[0U] == szPrefixW[0U]) && (szDispW[1U] == szPrefixW[1U]));
                        if (_this->FileOrFolderExists(hWndRemoteList, std::wstring(szDispW.data())))
                        {
                            if ((_this->m_nConfirmAnswer == CONFIRM_YES) || (_this->m_nConfirmAnswer == CONFIRM_NO))
                            {
                                std::array<wchar_t, MAX_PATH + 96> szMes;
                                if (fDirectory)
                                {
                                    ::_snwprintf_s(szMes.data(), MAX_PATH + 96, _TRUNCATE, L"%s < %s >\n%s", sz_H71.data(), szSelW, sz_H72.data());
                                }
                                else
                                {
                                    ::_snwprintf_s(szMes.data(), MAX_PATH + 96, _TRUNCATE, L"%s < %s >\n%s", sz_H17.data(), szSelW, sz_H38.data());
                                }
                                _this->DoFTConfirmDialog(fDirectory ? sz_H101.data() : sz_H39.data(), szMes.data());
                                if (_this->m_nConfirmAnswer == CONFIRM_NO)
                                {
                                    continue;
                                }
                                if (_this->m_nConfirmAnswer == CONFIRM_NOALL)
                                {
                                    break;
                                }
                            }
                        }
                        _this->m_FilesList.push_back(nSelected);
                    }
                }
            }
            {
                std::array<WCHAR, 256U> dbg;
                ::_snwprintf_s(dbg.data(), 256, _TRUNCATE, L"IDC_UPLOAD_B: m_FilesList.size()=%d (nSelCount=%d)\n", static_cast<int>(_this->m_FilesList.size()), nSelCount);
                ::OutputDebugStringW(dbg.data());
            }
            if (_this->m_FilesList.size() == 0)
            {
                break;
            }

            // Display Status
            {
                std::array<wchar_t, 128U> _ws40;
                ::_snwprintf_s(_ws40.data(), 128, _TRUNCATE, L" > %d %s", static_cast<int>(_this->m_FilesList.size()), sz_H40.data());
                ::SetDlgItemTextW(hWnd, IDC_LOCAL_STATUS, _ws40.data());
            }
            {
                std::array<wchar_t, 256U> _sws;
                ::_snwprintf_s(_sws.data(), 256, _TRUNCATE, L"%s %d %s", sz_H41.data(), static_cast<int>(_this->m_FilesList.size()), sz_H42.data());
                _this->SetStatus(_sws.data());
            }

            _this->m_nFilesTransfered = 0;
            _this->m_nFilesToTransfer = static_cast<int>(_this->m_FilesList.size());

            // Disable buttons
            _this->DisableButtons(_this->hWnd);
            ::ShowWindow(::GetDlgItem(hWnd, IDC_ABORT_B), SW_SHOW);
            ::ShowWindow(::GetDlgItem(hWnd, IDC_ABORT_B2), SW_SHOW);

            // Get the first selected file Unicode filename from lParam
            _this->m_iFile = _this->m_FilesList.begin();
            {
                LVITEMW ItemW0;
                std::memset(&ItemW0, 0, sizeof(ItemW0));
                ItemW0.mask = LVIF_PARAM;
                ItemW0.iItem = *_this->m_iFile;
                ::SendMessageW(hWndLocalList, LVM_GETITEMW, 0, reinterpret_cast<LPARAM>(&ItemW0));
                std::wstring* pNameW0 = reinterpret_cast<std::wstring*>(ItemW0.lParam & ~static_cast<LPARAM>(FT_LPARAM_UNREADABLE));

                std::array<WCHAR, MAX_PATH * 4> szCurrLocalW;
                ::GetDlgItemTextW(hWnd, IDC_CURR_LOCAL, szCurrLocalW.data(), MAX_PATH * 4);
                if (!std::wcslen(szCurrLocalW.data()))
                {
                    break;
                }
                if (pNameW0)
                {
                    ::wcscat_s(szCurrLocalW.data(), MAX_PATH * 4, pNameW0->c_str());
                }

                // Detect directories and wrap with brackets for ZipPossibleDirectory
                DWORD dwAttr0 = ::GetFileAttributesW(szCurrLocalW.data());
                if (dwAttr0 == INVALID_FILE_ATTRIBUTES)
                {
                    std::array<WCHAR, MAX_PATH * 4> szShortW0;
                    if (::GetShortPathNameW(szCurrLocalW.data(), szShortW0.data(), static_cast<DWORD>(szShortW0.size())))
                    {
                        dwAttr0 = ::GetFileAttributesW(szShortW0.data());
                    }
                }
                if (((dwAttr0 != INVALID_FILE_ATTRIBUTES) && (dwAttr0 & FILE_ATTRIBUTE_DIRECTORY)) && pNameW0)
                {
                    ::GetDlgItemTextW(hWnd, IDC_CURR_LOCAL, szCurrLocalW.data(), MAX_PATH * 4);
                    ::wcscat_s(szCurrLocalW.data(), MAX_PATH * 4, L"[ ");
                    ::wcscat_s(szCurrLocalW.data(), MAX_PATH * 4, pNameW0->c_str());
                    ::wcscat_s(szCurrLocalW.data(), MAX_PATH * 4, L" ]");
                }

                _this->m_fFileCommandPending = true;
                _this->m_fAbort = false;
                _this->m_fAborted = false;
                _this->m_fUserAbortedFileTransfer = false;
                if (!_this->OfferLocalFileW(szCurrLocalW.data()))
                {
                    _this->SendFiles(-1, 0);
                }
            }
        }
        break;

        // Receive selected files
        case IDC_DOWNLOAD_B: {
            if (_this->m_fFileCommandPending)
            {
                break;
            }
            {
                int nSelected = -1;
                int nCount{ 0 };
                HWND hWndLocalList = ::GetDlgItem(hWnd, IDC_LOCAL_FILELIST);
                HWND hWndRemoteList = ::GetDlgItem(hWnd, IDC_REMOTE_FILELIST);

                // If no dst dir is selected, nothing to do
                std::array<WCHAR, MAX_PATH> szCurrLocalCheckW;
                ::GetDlgItemTextW(hWnd, IDC_CURR_LOCAL, szCurrLocalCheckW.data(), static_cast<int>(szCurrLocalCheckW.size()));
                if (!std::wcslen(szCurrLocalCheckW.data()))
                {
                    break;
                }

                // Build [..] mask for up-dir detection
                std::array<WCHAR, 16U> szUpDirMaskW;
                std::array<WCHAR, 4U> szPrefixW;
                std::array<WCHAR, 4U> szSuffixW;
                ::MultiByteToWideChar(CP_ACP, 0, rfbDirPrefix, -1, szPrefixW.data(), 4);
                ::MultiByteToWideChar(CP_ACP, 0, rfbDirSuffix, -1, szSuffixW.data(), 4);
                ::swprintf_s(szUpDirMaskW.data(), szUpDirMaskW.size(), L"%s..%s", szPrefixW.data(), szSuffixW.data());

                // Get all selected files, check if they already exist on local side
                _this->m_FilesList.clear();
                nCount = ListView_GetItemCount(hWndRemoteList);
                _this->m_nConfirmAnswer = CONFIRM_YES;
                for (nSelected = 0; nSelected < nCount; ++nSelected)
                {
                    if (ListView_GetItemState(hWndRemoteList, nSelected, LVIS_SELECTED) & LVIS_SELECTED)
                    {
                        // Get Unicode filename from lParam
                        LVITEMW ItemW;
                        std::memset(&ItemW, 0, sizeof(ItemW));
                        ItemW.mask = LVIF_PARAM;
                        ItemW.iItem = nSelected;
                        ::SendMessageW(hWndRemoteList, LVM_GETITEMW, 0, reinterpret_cast<LPARAM>(&ItemW));
                        std::wstring* pNameW = reinterpret_cast<std::wstring*>(ItemW.lParam & ~static_cast<LPARAM>(FT_LPARAM_UNREADABLE));
                        if (!pNameW)
                        {
                            continue;
                        }
                        const WCHAR* szSelW = pNameW->c_str();

                        // Display text is "[ name ]" for folders (lParam holds the raw name)
                        std::array<WCHAR, MAX_PATH + 8> szDispW{ 0 };
                        {
                            LVITEMW _di;
                            std::memset(&_di, 0, sizeof(_di));
                            _di.mask = LVIF_TEXT;
                            _di.iItem = nSelected;
                            _di.pszText = szDispW.data();
                            _di.cchTextMax = MAX_PATH + 8;
                            ::SendMessageW(hWndRemoteList, LVM_GETITEMTEXTW, static_cast<WPARAM>(nSelected), reinterpret_cast<LPARAM>(&_di));
                        }
                        if (::_wcsicmp(szDispW.data(), szUpDirMaskW.data()) != 0)
                        {
                            bool fDirectory = ((szDispW[0U] == szPrefixW[0U]) && (szDispW[1U] == szPrefixW[1U]));
                            if (_this->FileOrFolderExists(hWndLocalList, std::wstring(szDispW.data())))
                            {
                                if ((_this->m_nConfirmAnswer == CONFIRM_YES) || (_this->m_nConfirmAnswer == CONFIRM_NO))
                                {
                                    std::array<wchar_t, MAX_PATH + 96> szMes;
                                    if (fDirectory)
                                    {
                                        ::_snwprintf_s(szMes.data(), MAX_PATH + 96, _TRUNCATE, L"%s < %s >\n\n%s", sz_H73.data(), szSelW, sz_H72.data());
                                    }
                                    else
                                    {
                                        ::_snwprintf_s(szMes.data(), MAX_PATH + 96, _TRUNCATE, L"%s < %s >\n\n%s", sz_H17.data(), szSelW, sz_H43.data());
                                    }
                                    _this->DoFTConfirmDialog(fDirectory ? sz_H101.data() : sz_H39.data(), szMes.data());
                                    if (_this->m_nConfirmAnswer == CONFIRM_NO)
                                    {
                                        continue;
                                    }
                                    if (_this->m_nConfirmAnswer == CONFIRM_NOALL)
                                    {
                                        break;
                                    }
                                }
                            }
                            _this->m_FilesList.push_back(nSelected);
                        }
                    }
                }
                if (_this->m_FilesList.size() == 0)
                {
                    break;
                }

                // Display Status
                {
                    std::array<wchar_t, 128U> _ws40r;
                    ::_snwprintf_s(_ws40r.data(), 128, _TRUNCATE, L" > %d %s", static_cast<int>(_this->m_FilesList.size()), sz_H40.data());
                    ::SetDlgItemTextW(hWnd, IDC_REMOTE_STATUS, _ws40r.data());
                }
                {
                    std::array<wchar_t, 256U> _sws2;
                    ::_snwprintf_s(_sws2.data(), 256, _TRUNCATE, L"%s %d %s", sz_H44.data(), static_cast<int>(_this->m_FilesList.size()), sz_H45.data());
                    _this->SetStatus(_sws2.data());
                }

                _this->m_nFilesTransfered = 0;
                _this->m_nFilesToTransfer = static_cast<int>(_this->m_FilesList.size());

                // Disable buttons
                _this->DisableButtons(_this->hWnd);
                ::ShowWindow(::GetDlgItem(_this->hWnd, IDC_ABORT_B), SW_SHOW);
                ::ShowWindow(::GetDlgItem(_this->hWnd, IDC_ABORT_B2), SW_SHOW);

                // Get first file Unicode from lParam, build UTF-8 path for server
                _this->m_iFile = _this->m_FilesList.begin();
                {
                    LVITEMW ItemW0;
                    std::memset(&ItemW0, 0, sizeof(ItemW0));
                    ItemW0.mask = LVIF_PARAM;
                    ItemW0.iItem = *_this->m_iFile;
                    ::SendMessageW(hWndRemoteList, LVM_GETITEMW, 0, reinterpret_cast<LPARAM>(&ItemW0));
                    std::wstring* pNameW0 = reinterpret_cast<std::wstring*>(ItemW0.lParam & ~static_cast<LPARAM>(FT_LPARAM_UNREADABLE));

                    // Check display text to determine if it's a directory (shown as "[ name ]")
                    bool bIsDir0{ false };
                    {
                        std::array<WCHAR, MAX_PATH + 4> _disp0{ 0 };
                        LVITEMW _di0;
                        std::memset(&_di0, 0, sizeof(_di0));
                        _di0.mask = LVIF_TEXT;
                        _di0.iItem = *_this->m_iFile;
                        _di0.pszText = _disp0.data();
                        _di0.cchTextMax = MAX_PATH + 4;
                        ::SendMessageW(hWndRemoteList, LVM_GETITEMTEXTW, static_cast<WPARAM>(*_this->m_iFile), reinterpret_cast<LPARAM>(&_di0));
                        if ((_disp0[0U] == L'[') && (_disp0[1U] == L' '))
                        {
                            bIsDir0 = true;
                        }
                    }

                    std::array<WCHAR, MAX_PATH * 4> szDstFileW;
                    ::GetDlgItemTextW(hWnd, IDC_CURR_REMOTE, szDstFileW.data(), MAX_PATH * 4);
                    if (!std::wcslen(szDstFileW.data()))
                    {
                        break;
                    }
                    if (pNameW0)
                    {
                        if (bIsDir0)
                        {
                            ::wcscat_s(szDstFileW.data(), szDstFileW.size(), L"[ ");
                            ::wcscat_s(szDstFileW.data(), szDstFileW.size(), pNameW0->c_str());
                            ::wcscat_s(szDstFileW.data(), szDstFileW.size(), L" ]");
                        }
                        else
                        {
                            ::wcscat_s(szDstFileW.data(), szDstFileW.size(), pNameW0->c_str());
                        }
                    }

                    std::array<char, MAX_PATH * 3> szDstFileUTF8;
                    ::WideCharToMultiByte(CP_UTF8, 0, szDstFileW.data(), -1, szDstFileUTF8.data(), static_cast<int>(szDstFileUTF8.size()), nullptr, nullptr);

                    _this->m_fFileCommandPending = true;
                    _this->m_fAbort = false;
                    _this->m_fAborted = false;
                    _this->m_fUserAbortedFileTransfer = false;
                    _this->RequestRemoteFile(szDstFileUTF8.data());
                }
            }
        }
        break;

        case IDC_LOCAL_ROOTB: {
            std::array<wchar_t, MAX_PATH * 4> ofDirW{ 0 };
            int nSelected = static_cast<int>(::SendMessage(::GetDlgItem(hWnd, IDC_LOCAL_DRIVECB), CB_GETCURSEL, 0, 0));
            if (nSelected == -1)
            {
                break;
            }
            ::SendMessageW(::GetDlgItem(hWnd, IDC_LOCAL_DRIVECB), CB_GETLBTEXT, static_cast<WPARAM>(nSelected), reinterpret_cast<LPARAM>(ofDirW.data()));
            // Truncate at first ']' to handle "[ C: ] - Local Disk" format
            wchar_t* pEnd = std::wcschr(ofDirW.data(), L']');
            if (pEnd)
            {
                *(pEnd + 1) = L'\0';
            }
            _this->PopulateLocalListBoxW(hWnd, ofDirW.data());
        }
        break;

        case IDC_REMOTE_ROOTB:
            if (!_this->m_fFileCommandPending)
            {
                std::array<WCHAR, MAX_PATH * 4> ofDirW{ 0 };
                int nSelected = static_cast<int>(::SendMessage(::GetDlgItem(hWnd, IDC_REMOTE_DRIVECB), CB_GETCURSEL, 0, 0));
                if (nSelected == -1)
                {
                    break;
                }
                ::SendMessageW(::GetDlgItem(hWnd, IDC_REMOTE_DRIVECB), CB_GETLBTEXT, static_cast<WPARAM>(nSelected), reinterpret_cast<LPARAM>(ofDirW.data()));
                // Truncate at first ']' to handle "[ C: ] - Remote" format
                WCHAR* pEnd = std::wcschr(ofDirW.data(), L']');
                if (pEnd)
                {
                    *(pEnd + 1) = L'\0';
                }
                _this->m_fFileCommandPending = true;
                _this->RequestRemoteDirectoryContent(hWnd, ofDirW.data());
            }
            break;

        case IDC_LOCAL_UPB: {
            std::array<WCHAR, 16U> szUpDirMask;
            ::_snwprintf_s(szUpDirMask.data(), 16, _TRUNCATE, L"%hs..%hs", rfbDirPrefix, rfbDirSuffix);
            _this->PopulateLocalListBoxW(hWnd, szUpDirMask.data());
        }
        break;

        case IDC_REMOTE_UPB:
            if (!_this->m_fFileCommandPending)
            {
                _this->m_fFileCommandPending = true;
                std::array<WCHAR, 16U> szUpDirMask;
                ::_snwprintf_s(szUpDirMask.data(), 16, _TRUNCATE, L"%hs..%hs", rfbDirPrefix, rfbDirSuffix);
                _this->RequestRemoteDirectoryContent(hWnd, szUpDirMask.data());
            }
            break;

        case IDC_REFRESH:
            if (_this->m_fFocusLocal)
            {
                FTListViewClear(::GetDlgItem(hWnd, IDC_LOCAL_FILELIST));
                _this->PopulateLocalListBoxW(hWnd, L"");
            }
            else if (!_this->m_fFileCommandPending)
            {
                _this->m_fFileCommandPending = true;
                _this->RequestRemoteDirectoryContent(hWnd, L"");
            }
            break;

        case IDC_ABORT_B:
            _this->m_fAbort = true;
            _this->m_fUserAbortedFileTransfer = true;
            ::ShowWindow(::GetDlgItem(_this->hWnd, IDC_ABORT_B), SW_HIDE);
            break;

        case IDCANCEL2:
        case IDC_ABORT_B2:
            _this->m_fAbort = true;
            _this->m_fUserAbortedFileTransfer = true;
            _this->m_fUserForcedAbortedFileTransfer = true;
            closesocket(_this->m_pCC->m_sock);
            ::ShowWindow(::GetDlgItem(_this->hWnd, IDC_ABORT_B), SW_HIDE);
            break;

        case IDC_DELETE_B:
            // Delete Local File
            if (_this->m_fFocusLocal)
            {
                std::array<wchar_t, MAX_PATH + 96> szMes;
                std::array<WCHAR, MAX_PATH> szCurrLocalW;
                ::GetDlgItemTextW(hWnd, IDC_CURR_LOCAL, szCurrLocalW.data(), MAX_PATH);
                if (!std::wcslen(szCurrLocalW.data()))
                {
                    break;
                }

                HWND hWndLocalList = ::GetDlgItem(hWnd, IDC_LOCAL_FILELIST);

                int nSelected = static_cast<int>(ListView_GetSelectedCount(hWndLocalList));
                if (nSelected == 0)
                {
                    yesUVNCMessageBox(m_hInstResDLL, _this->hWnd, sz_H46.data(), sz_H47.data(), MB_ICONINFORMATION);
                    break;
                }

                int nCount = ListView_GetItemCount(hWndLocalList);
                _this->m_nConfirmAnswer = CONFIRM_YES;
                _this->m_nDeleteCount = 0;
                for (int i{ 0 }; i < nCount; ++i)
                {
                    if (ListView_GetItemState(hWndLocalList, i, LVIS_SELECTED) & LVIS_SELECTED)
                    {
                        LVITEMW ItemW;
                        std::memset(&ItemW, 0, sizeof(ItemW));
                        ItemW.mask = LVIF_PARAM;
                        ItemW.iItem = i;
                        ::SendMessageW(hWndLocalList, LVM_GETITEMW, 0, reinterpret_cast<LPARAM>(&ItemW));
                        std::wstring* pNameW = reinterpret_cast<std::wstring*>(ItemW.lParam & ~static_cast<LPARAM>(FT_LPARAM_UNREADABLE));
                        if (!pNameW)
                        {
                            continue;
                        }
                        const WCHAR* szSelW = pNameW->c_str();
                        bool isDir = ((szSelW[0U] == L'[') && (szSelW[1U] != L'.'));

                        if ((_this->m_nConfirmAnswer == CONFIRM_YES) || (_this->m_nConfirmAnswer == CONFIRM_NO))
                        {
                            std::array<wchar_t, MAX_PATH + 96> szMesInner;
                            ::_snwprintf_s(szMesInner.data(), MAX_PATH + 96, _TRUNCATE, L"%s\n\n< %s > ?\n", isDir ? sz_H95.data() : sz_H48.data(), szSelW);
                            _this->DoFTConfirmDialog(isDir ? sz_H94.data() : sz_H47.data(), szMesInner.data());
                            if (_this->m_nConfirmAnswer == CONFIRM_NO)
                            {
                                continue;
                            }
                            if (_this->m_nConfirmAnswer == CONFIRM_NOALL)
                            {
                                break;
                            }
                        }

                        std::array<WCHAR, MAX_PATH * 4> szFullPathW;
                        ::wcscpy_s(szFullPathW.data(), szFullPathW.size(), szCurrLocalW.data());
                        ::wcscat_s(szFullPathW.data(), szFullPathW.size(), szSelW);

                        if (!_this->DeleteFileOrDirectory(szFullPathW.data()))
                        {
                            ::_snwprintf_s(szMes.data(), MAX_PATH + 96, _TRUNCATE, L"%s < %s >", isDir ? sz_H97.data() : sz_H49.data(), szFullPathW.data());
                            _this->SetStatus(szMes.data());
                            continue;
                        }
                        ::_snwprintf_s(szMes.data(), MAX_PATH * 4 + 96, _TRUNCATE, L"%s < %s > %s", isDir ? sz_H31.data() : sz_H17.data(), szFullPathW.data(), sz_H50.data());
                        _this->SetStatus(szMes.data());
                        ++_this->m_nDeleteCount;
                    }
                }
                // Refresh the local list view after deleting multiple files
                FTListViewClear(::GetDlgItem(hWnd, IDC_LOCAL_FILELIST));
                _this->PopulateLocalListBoxW(hWnd, L"");
            }
            else // Delete remote file
            {
                if (_this->m_fFileCommandPending)
                {
                    break;
                }
                HWND hWndRemoteList = ::GetDlgItem(hWnd, IDC_REMOTE_FILELIST);

                std::array<WCHAR, MAX_PATH * 4> szCurrRemoteW;
                ::GetDlgItemTextW(hWnd, IDC_CURR_REMOTE, szCurrRemoteW.data(), static_cast<int>(szCurrRemoteW.size()));
                if (!std::wcslen(szCurrRemoteW.data()))
                {
                    break;
                }

                int nSelected = static_cast<int>(ListView_GetSelectedCount(hWndRemoteList));
                if (nSelected == 0)
                {
                    yesUVNCMessageBox(m_hInstResDLL, _this->hWnd, sz_H46.data(), sz_H47.data(), MB_ICONINFORMATION);
                    break;
                }

                int nCount = ListView_GetItemCount(hWndRemoteList);
                _this->m_nConfirmAnswer = CONFIRM_YES;
                _this->m_nDeleteCount = 0;
                std::vector<std::string> pathsToDelete; // UTF-8 paths
                for (int i{ 0 }; i < nCount; ++i)
                {
                    if (ListView_GetItemState(hWndRemoteList, i, LVIS_SELECTED) & LVIS_SELECTED)
                    {
                        LVITEMW ItemW;
                        std::memset(&ItemW, 0, sizeof(ItemW));
                        ItemW.mask = LVIF_PARAM;
                        ItemW.iItem = i;
                        ::SendMessageW(hWndRemoteList, LVM_GETITEMW, 0, reinterpret_cast<LPARAM>(&ItemW));
                        std::wstring* pNameW = reinterpret_cast<std::wstring*>(ItemW.lParam & ~static_cast<LPARAM>(FT_LPARAM_UNREADABLE));
                        if (!pNameW)
                        {
                            continue;
                        }
                        const WCHAR* szSelW = pNameW->c_str();
                        bool isDir = ((szSelW[0U] == L'[') && (szSelW[1U] != L'.'));
                        if ((_this->m_nConfirmAnswer == CONFIRM_YES) || (_this->m_nConfirmAnswer == CONFIRM_NO))
                        {
                            std::array<wchar_t, MAX_PATH + 96> szMes;
                            ::_snwprintf_s(szMes.data(), MAX_PATH + 96, _TRUNCATE, L"%s\n\n< %s > ?\n", isDir ? sz_H96.data() : sz_H51.data(), szSelW);
                            _this->DoFTConfirmDialog(isDir ? sz_H94.data() : sz_H47.data(), szMes.data());
                            if (_this->m_nConfirmAnswer == CONFIRM_NO)
                            {
                                continue;
                            }
                            if (_this->m_nConfirmAnswer == CONFIRM_NOALL)
                            {
                                break;
                            }
                        }
                        // Build full Unicode path (dir + filename) then convert to UTF-8 for server
                        std::array<WCHAR, MAX_PATH * 4> szFullW;
                        ::wcscpy_s(szFullW.data(), szFullW.size(), szCurrRemoteW.data());
                        ::wcscat_s(szFullW.data(), szFullW.size(), szSelW);
                        std::array<char, MAX_PATH * 3> szFullPathUTF8;
                        ::WideCharToMultiByte(CP_UTF8, 0, szFullW.data(), -1, szFullPathUTF8.data(), static_cast<int>(szFullPathUTF8.size()), nullptr, nullptr);
                        ++_this->m_nDeleteCount;
                        pathsToDelete.push_back(std::string(szFullPathUTF8.data()));
                    }
                }
                // Set command pending only once after collecting all paths
                if (!pathsToDelete.empty())
                {
                    _this->m_fFileCommandPending = true;
                }
                for (auto& path : pathsToDelete)
                {
                    _this->DeleteRemoteFile(path.c_str());
                    ::UpdateWindow(_this->hWnd);
                }
            }
            break;

        case IDC_NEWFOLDER_B:
            // Create Local Folder
            if (_this->m_fFocusLocal)
            {
                std::array<wchar_t, MAX_PATH * 4 + 96> szMes;
                std::array<WCHAR, MAX_PATH * 4> szCurrLocalW;
                ::GetDlgItemTextW(hWnd, IDC_CURR_LOCAL, szCurrLocalW.data(), MAX_PATH * 4);
                if (!std::wcslen(szCurrLocalW.data()))
                {
                    break;
                }

                HWND hWndLocalList = ::GetDlgItem(hWnd, IDC_LOCAL_FILELIST);

                std::memset(_this->m_szFTParam.data(), '\0', sizeof(_this->m_szFTParam));
                _this->m_szFTParamW[0U] = L'\0';
                _this->DoFTParamDialog(sz_H100.data(), sz_H52.data());
                if ((_this->m_szFTParamW[0U] == L'\0') ||
                    ((std::wcslen(szCurrLocalW.data()) + std::wcslen(_this->m_szFTParamW.data())) > MAX_PATH * 4 - 2))
                {
                    break;
                }
                std::array<WCHAR, MAX_PATH * 4> szFullLocalW;
                ::wcscpy_s(szFullLocalW.data(), szFullLocalW.size(), szCurrLocalW.data());
                ::wcscat_s(szFullLocalW.data(), szFullLocalW.size(), _this->m_szFTParamW.data());
                if (_this->FileOrFolderExists(hWndLocalList, std::wstring(szFullLocalW.data())))
                {
                    ::_snwprintf_s(szMes.data(), MAX_PATH + 96, _TRUNCATE, L"%s < %s >: %s", sz_H53.data(), szFullLocalW.data(), sz_H102.data());
                    _this->SetStatus(szMes.data());
                    break;
                }
                if (!::CreateDirectoryW(szFullLocalW.data(), nullptr))
                {
                    ::_snwprintf_s(szMes.data(), MAX_PATH + 96, _TRUNCATE, L"%s < %s >", sz_H53.data(), szFullLocalW.data());
                    _this->SetStatus(szMes.data());
                    break;
                }

                ::_snwprintf_s(szMes.data(), MAX_PATH + 96, _TRUNCATE, L"%s < %s > %s", sz_H54.data(), szFullLocalW.data(), sz_H55.data());
                _this->SetStatus(szMes.data());

                // Refresh the Local List view
                FTListViewClear(::GetDlgItem(hWnd, IDC_LOCAL_FILELIST));
                _this->PopulateLocalListBoxW(hWnd, L"");
            }
            else // Create Remote Folder
            {
                if (_this->m_fFileCommandPending)
                {
                    break;
                }
                std::array<WCHAR, MAX_PATH * 4> szCurrRemoteW;
                ::GetDlgItemTextW(hWnd, IDC_CURR_REMOTE, szCurrRemoteW.data(), static_cast<int>(szCurrRemoteW.size()));
                if (!std::wcslen(szCurrRemoteW.data()))
                {
                    break;
                }
                std::memset(_this->m_szFTParam.data(), '\0', sizeof(_this->m_szFTParam));
                _this->m_szFTParamW[0U] = L'\0';
                _this->DoFTParamDialog(sz_H100.data(), sz_H56.data());
                if ((_this->m_szFTParamW[0U] == L'\0') ||
                    ((std::wcslen(szCurrRemoteW.data()) + std::wcslen(_this->m_szFTParamW.data())) > MAX_PATH * 4 - 2))
                {
                    break;
                }

                std::array<wchar_t, MAX_PATH> szFolderNameR;
                ::_snwprintf_s(szFolderNameR.data(), szFolderNameR.size(), _TRUNCATE, L"[ %s ]", _this->m_szFTParam.data());
                if (_this->FileOrFolderExists(::GetDlgItem(hWnd, IDC_REMOTE_FILELIST), std::wstring(szFolderNameR.data())))
                {
                    std::array<wchar_t, MAX_PATH + 128> szStatus;
                    ::_snwprintf_s(szStatus.data(), MAX_PATH + 128, _TRUNCATE, L"%s < %s > %s: %s", sz_H29.data(), szCurrRemoteW.data(), sz_H30.data(), sz_H102.data());
                    _this->SetStatus(szStatus.data());
                    break;
                }

                _this->m_fFileCommandPending = true;
                {
                    // Build full Unicode path and convert to UTF-8 for server
                    ::wcscat_s(szCurrRemoteW.data(), szCurrRemoteW.size(), _this->m_szFTParamW.data());
                    std::array<char, MAX_PATH * 3> szCurrRemoteUTF8;
                    ::WideCharToMultiByte(CP_UTF8, 0, szCurrRemoteW.data(), -1, szCurrRemoteUTF8.data(), static_cast<int>(szCurrRemoteUTF8.size()), nullptr,
                                        nullptr);
                    _this->CreateRemoteDirectory(szCurrRemoteUTF8.data());
                }
            }
            break;

        case IDC_RENAME_B: {
            if (_this->m_fFocusLocal)
            {
                HWND hWndLocalList = ::GetDlgItem(hWnd, IDC_LOCAL_FILELIST);
                int nCount = static_cast<int>(ListView_GetSelectedCount(hWndLocalList));
                if ((nCount == 0) || (nCount > 1))
                {
                    yesUVNCMessageBox(m_hInstResDLL, _this->hWnd, sz_M1.data(), sz_M2.data(), MB_ICONINFORMATION);
                    break;
                }

                std::array<wchar_t, MAX_PATH * 4 + 96> szMes;
                std::array<WCHAR, MAX_PATH * 4> szCurrLocalW;
                ::GetDlgItemTextW(hWnd, IDC_CURR_LOCAL, szCurrLocalW.data(), MAX_PATH * 4);
                if (!std::wcslen(szCurrLocalW.data()))
                {
                    break;
                }

                nCount = ListView_GetItemCount(hWndLocalList);
                for (int nSelected{ 0 }; nSelected < nCount; ++nSelected)
                {
                    if (ListView_GetItemState(hWndLocalList, nSelected, LVIS_SELECTED) & LVIS_SELECTED)
                    {
                        LVITEMW ItemW;
                        std::memset(&ItemW, 0, sizeof(ItemW));
                        ItemW.mask = LVIF_PARAM;
                        ItemW.iItem = nSelected;
                        ::SendMessageW(hWndLocalList, LVM_GETITEMW, 0, reinterpret_cast<LPARAM>(&ItemW));
                        std::wstring* pNameW = reinterpret_cast<std::wstring*>(ItemW.lParam & ~static_cast<LPARAM>(FT_LPARAM_UNREADABLE));
                        if (!pNameW)
                        {
                            break;
                        }
                        const WCHAR* szSelW = pNameW->c_str();
                        std::array<WCHAR, MAX_PATH * 4> szSelW_copy;
                        ::wcscpy_s(szSelW_copy.data(), szSelW_copy.size(), szSelW);
                        _this->IsDirectoryGetItW(szSelW_copy.data(), static_cast<int>(szSelW_copy.size()));
                        ::wcscpy_s(_this->m_szFTParam.data(), _this->m_szFTParam.size(), szSelW_copy.data());
                        _this->m_szFTParamW[0U] = L'\0';
                        _this->DoFTParamDialog(sz_M3.data(), sz_M4.data());
                        if ((_this->m_szFTParamW[0U] == L'\0') ||
                            ((std::wcslen(szCurrLocalW.data()) + std::wcslen(_this->m_szFTParamW.data())) > MAX_PATH * 4 - 2))
                        {
                            break;
                        }
                        std::array<wchar_t, MAX_PATH * 4> szFolderName2;
                        ::_snwprintf_s(szFolderName2.data(), szFolderName2.size(), _TRUNCATE, L"[ %s ]", _this->m_szFTParam.data());
                        if ((_this->FileOrFolderExists(::GetDlgItem(hWnd, IDC_LOCAL_FILELIST), std::wstring(szFolderName2.data()))) ||
                            (_this->FileOrFolderExists(::GetDlgItem(hWnd, IDC_LOCAL_FILELIST), std::wstring(_this->m_szFTParam.data()))))
                        {
                            ::_snwprintf_s(szMes.data(), MAX_PATH * 4 + 96, _TRUNCATE, L"%s < %s >: %s", sz_M5.data(), szSelW, sz_H102.data());
                            _this->SetStatus(szMes.data());
                            break;
                        }
                        std::array<WCHAR, MAX_PATH * 4> szOldPathW;
                        std::array<WCHAR, MAX_PATH * 4> szNewPathW;
                        ::wcscpy_s(szOldPathW.data(), szOldPathW.size(), szCurrLocalW.data());
                        ::wcscat_s(szOldPathW.data(), szOldPathW.size(), szSelW);
                        ::wcscpy_s(szNewPathW.data(), szNewPathW.size(), szCurrLocalW.data());
                        ::wcscat_s(szNewPathW.data(), szNewPathW.size(), _this->m_szFTParamW.data());
                        if (!::MoveFileW(szOldPathW.data(), szNewPathW.data()))
                        {
                            ::_snwprintf_s(szMes.data(), MAX_PATH * 4 + 96, _TRUNCATE, L"%s < %s >", sz_M5.data(), szOldPathW.data());
                            _this->SetStatus(szMes.data());
                            break;
                        }
                        ::_snwprintf_s(szMes.data(), MAX_PATH * 4 + 96, _TRUNCATE, L"%s < %s > %s < %s >", sz_M6.data(), szOldPathW.data(), sz_M7.data(), szNewPathW.data());
                        _this->SetStatus(szMes.data());
                    }
                }
                FTListViewClear(::GetDlgItem(hWnd, IDC_LOCAL_FILELIST));
                _this->PopulateLocalListBoxW(hWnd, L"");
            }
            else // Rename Remote
            {
                if (_this->m_fFileCommandPending)
                {
                    break;
                }
                HWND hWndRemoteList = ::GetDlgItem(hWnd, IDC_REMOTE_FILELIST);
                int nCount = static_cast<int>(ListView_GetSelectedCount(hWndRemoteList));
                if ((nCount == 0) || (nCount > 1))
                {
                    yesUVNCMessageBox(m_hInstResDLL, _this->hWnd, sz_M1.data(), sz_M2.data(), MB_ICONINFORMATION);
                    break;
                }

                std::array<WCHAR, MAX_PATH * 4> szCurrRemoteW;
                ::GetDlgItemTextW(hWnd, IDC_CURR_REMOTE, szCurrRemoteW.data(), static_cast<int>(szCurrRemoteW.size()));
                if (!std::wcslen(szCurrRemoteW.data()))
                {
                    break;
                }

                nCount = ListView_GetItemCount(hWndRemoteList);
                for (int nSel{ 0 }; nSel < nCount; ++nSel)
                {
                    if (ListView_GetItemState(hWndRemoteList, nSel, LVIS_SELECTED) & LVIS_SELECTED)
                    {
                        LVITEMW ItemW;
                        std::memset(&ItemW, 0, sizeof(ItemW));
                        ItemW.mask = LVIF_PARAM;
                        ItemW.iItem = nSel;
                        ::SendMessageW(hWndRemoteList, LVM_GETITEMW, 0, reinterpret_cast<LPARAM>(&ItemW));
                        std::wstring* pNameW = reinterpret_cast<std::wstring*>(ItemW.lParam & ~static_cast<LPARAM>(FT_LPARAM_UNREADABLE));
                        if (!pNameW)
                        {
                            break;
                        }
                        const WCHAR* szSelW = pNameW->c_str();
                        std::array<WCHAR, MAX_PATH * 4> szSelW_copy;
                        ::wcscpy_s(szSelW_copy.data(), szSelW_copy.size(), szSelW);
                        _this->IsDirectoryGetItW(szSelW_copy.data(), static_cast<int>(szSelW_copy.size()));
                        ::wcscpy_s(_this->m_szFTParam.data(), _this->m_szFTParam.size(), szSelW_copy.data());
                        _this->m_szFTParamW[0U] = L'\0';
                        _this->DoFTParamDialog(sz_M3.data(), sz_M4.data());
                        if ((_this->m_szFTParamW[0U] == L'\0') || (std::wcschr(_this->m_szFTParamW.data(), L'*') != nullptr) ||
                            ((std::wcslen(szCurrRemoteW.data()) + std::wcslen(_this->m_szFTParamW.data())) > MAX_PATH * 4 - 2))
                        {
                            break;
                        }
                        std::array<wchar_t, MAX_PATH * 4> szFolderName3;
                        ::_snwprintf_s(szFolderName3.data(), szFolderName3.size(), _TRUNCATE, L"[ %s ]", _this->m_szFTParam.data());
                        if ((_this->FileOrFolderExists(::GetDlgItem(hWnd, IDC_REMOTE_FILELIST),
                                                       std::wstring(szFolderName3.data()))) ||
                            (_this->FileOrFolderExists(::GetDlgItem(hWnd, IDC_REMOTE_FILELIST), std::wstring(_this->m_szFTParam.data()))))
                        {
                            std::array<wchar_t, MAX_PATH * 4 + 256> szMesR;
                            ::_snwprintf_s(szMesR.data(), szMesR.size(), _TRUNCATE, L"%s < %s > %s: %s", sz_M5.data(), szSelW, sz_H30.data(), sz_H102.data());
                            _this->SetStatus(szMesR.data());
                            break;
                        }
                        std::array<WCHAR, MAX_PATH * 4> szOldPathW;
                        std::array<WCHAR, MAX_PATH * 4> szNewPathW;
                        ::wcscpy_s(szOldPathW.data(), szOldPathW.size(), szCurrRemoteW.data());
                        ::wcscat_s(szOldPathW.data(), szOldPathW.size(), szSelW);
                        ::wcscpy_s(szNewPathW.data(), szNewPathW.size(), szCurrRemoteW.data());
                        ::wcscat_s(szNewPathW.data(), szNewPathW.size(), _this->m_szFTParamW.data());
                        std::array<char, MAX_PATH * 3> szOldPathUTF8;
                        std::array<char, MAX_PATH * 3> szNewPathUTF8;
                        ::WideCharToMultiByte(CP_UTF8, 0, szOldPathW.data(), -1, szOldPathUTF8.data(), static_cast<int>(szOldPathUTF8.size()), nullptr,
                                            nullptr);
                        ::WideCharToMultiByte(CP_UTF8, 0, szNewPathW.data(), -1, szNewPathUTF8.data(), static_cast<int>(szNewPathUTF8.size()), nullptr,
                                            nullptr);
                        _this->m_fFileCommandPending = true;
                        _this->RenameRemoteFileOrDirectory(szOldPathUTF8.data(), szNewPathUTF8.data());
                    }
                }
            }
        }
        break;

        case IDC_LOCAL_DRIVECB:
            switch (HIWORD(wParam))
            {
            case CBN_SELCHANGE: {
                std::array<wchar_t, MAX_PATH * 4> ofDirW{ 0 };
                int nSelected = static_cast<int>(::SendMessage(::GetDlgItem(hWnd, IDC_LOCAL_DRIVECB), CB_GETCURSEL, 0, 0));
                if (nSelected == -1)
                {
                    break;
                }
                ::SendMessageW(::GetDlgItem(hWnd, IDC_LOCAL_DRIVECB), CB_GETLBTEXT, static_cast<WPARAM>(nSelected), reinterpret_cast<LPARAM>(ofDirW.data()));
                // Truncate at first ']' to handle "[ C: ] - Local Disk" format
                wchar_t* pEnd = std::wcschr(ofDirW.data(), L']');
                if (pEnd)
                {
                    *(pEnd + 1) = L'\0';
                }
                _this->PopulateLocalListBoxW(hWnd, ofDirW.data());
                break;
            }
            }
            break;

        case IDC_REMOTE_DRIVECB:
            switch (HIWORD(wParam))
            {
            case CBN_SELCHANGE: {
                std::array<WCHAR, MAX_PATH * 4> ofDirW{ 0 };
                int nSelected = static_cast<int>(::SendMessage(::GetDlgItem(hWnd, IDC_REMOTE_DRIVECB), CB_GETCURSEL, 0, 0));
                if (nSelected == -1)
                {
                    break;
                }
                ::SendMessageW(::GetDlgItem(hWnd, IDC_REMOTE_DRIVECB), CB_GETLBTEXT, static_cast<WPARAM>(nSelected), reinterpret_cast<LPARAM>(ofDirW.data()));

                // Truncate drive items from "[ D: ] - Local Disk" to "[ D: ]"
                // Format: [ D: ] where positions are: [0]='[' [1]=' ' [2]='D' [3]=':' [4]=' ' [5]=']'
                // Check if it's a drive and has extra text after the bracket
                if ((std::wcslen(ofDirW.data()) > 6) && (ofDirW[0U] == L'[') && (ofDirW[1U] == L' ') && (ofDirW[3U] == L':') &&
                    (ofDirW[4U] == L' ') && (ofDirW[5U] == L']') && (ofDirW[6U] == L' '))
                {
                    ofDirW[6U] = L'\0';
                } // Truncate at position 6 to keep "[ D: ]"

#if DEBUG_FT
                {
                    std::array<wchar_t, 512U> debugMsg;
                    ::_snwprintf_s(debugMsg.data(), 512, _TRUNCATE, L"Remote CB: Unicode=[%s] Len=%d\n", ofDirW.data(), static_cast<int>(std::wcslen(ofDirW.data())));
                    ::OutputDebugStringW(debugMsg.data());
                }
#endif

                _this->RequestRemoteDirectoryContent(hWnd, ofDirW.data());
                break;
            }
            }
            break;

        case IDC_HIDE_B:
            _this->ShowFileTransferWindow(false);
            return TRUE;
        }
        break;


    case WM_SYSCOMMAND:
        switch (LOWORD(wParam))
        {
        case SC_RESTORE:
            _this->ShowFileTransferWindow(true);
            return TRUE;

        case SC_MINIMIZE:
            ::SendMessage(hWnd, WM_COMMAND, MAKEWPARAM(IDC_HIDE_B, 0), 0);
            return FALSE;
        }
        break;

    case WM_SIZING:

        LPRECT lprc;
        lprc = reinterpret_cast<LPRECT>(lParam);
        switch (wParam)
        {
        case WMSZ_TOPLEFT:
            FTAdjustTop(lprc);
            FTAdjustLeft(lprc);

        case WMSZ_TOP:
            FTAdjustTop(lprc);

        case WMSZ_TOPRIGHT:
            FTAdjustTop(lprc);
            FTAdjustRight(lprc);

        case WMSZ_LEFT:
            FTAdjustLeft(lprc);

        case WMSZ_RIGHT:
            FTAdjustRight(lprc);

        case WMSZ_BOTTOMLEFT:
            FTAdjustBottom(lprc);
            FTAdjustLeft(lprc);

        case WMSZ_BOTTOM:
            FTAdjustBottom(lprc);

        case WMSZ_BOTTOMRIGHT:
            FTAdjustBottom(lprc);
            FTAdjustRight(lprc);
        }
        return TRUE;


    case WM_SIZE:

        int cx;
        int cy;
        int icx;
        int icy;
        int icyb;
        int lf_an;
        int rg_an;
        RECT rc;
        RECT rcLMStatic;
        RECT rcUpload;
        RECT rcStatus;
        RECT rcHistory;
        RECT rcProgress;
        RECT rcRoot;
        int buttonUploadWidth;
        int buttonUploadHeight;
        int buttonStatusWidth;
        int buttonStatusHeight;
        int buttonHistoryWidth;
        int buttonHistoryHeight;
        int buttonProgressWidth;
        int buttonProgressHeight;
        int buttonRootWidth;
        int buttonRootHeight;
        int foced_closed;
        int topbuttonsHeight;
        int buttonLMStaticWidth;

        if (wParam == SIZE_MINIMIZED)
        {
            break;
        }

        ::GetWindowRect(::GetDlgItem(hWnd, IDC_UPLOAD_B), &rcUpload);
        ::GetWindowRect(::GetDlgItem(hWnd, IDC_LOCAL_STATUS), &rcStatus);
        ::GetWindowRect(::GetDlgItem(hWnd, IDC_HISTORY_CB), &rcHistory);
        ::GetWindowRect(::GetDlgItem(hWnd, IDC_PROGRESS), &rcProgress);
        ::GetWindowRect(::GetDlgItem(hWnd, IDC_LOCAL_ROOTB), &rcRoot);
        ::GetWindowRect(::GetDlgItem(hWnd, IDC_LM_STATIC), &rcLMStatic);

        buttonUploadWidth = rcUpload.right - rcUpload.left;
        buttonUploadHeight = rcUpload.bottom - rcUpload.top;

        buttonStatusWidth = rcStatus.right - rcStatus.left;
        buttonStatusHeight = rcStatus.bottom - rcStatus.top;

        buttonHistoryWidth = rcHistory.right - rcHistory.left;
        buttonHistoryHeight = rcHistory.bottom - rcHistory.top;

        buttonProgressWidth = rcProgress.right - rcProgress.left;
        buttonProgressHeight = rcProgress.bottom - rcProgress.top;

        buttonRootWidth = rcRoot.right - rcRoot.left;
        buttonRootHeight = rcRoot.bottom - rcRoot.top;

        buttonLMStaticWidth = rcLMStatic.right - rcLMStatic.left;

        foced_closed = buttonStatusHeight + buttonHistoryHeight + buttonProgressHeight + 12;
        topbuttonsHeight = buttonUploadHeight * 2 + 12;

        cx = LOWORD(lParam); //Client Width
        cy = HIWORD(lParam); //Client Height
        icy = cy - topbuttonsHeight - (buttonUploadHeight * 4);
        lf_an = (cx - buttonUploadWidth) / 2 - 12;

        ::GetWindowRect(::GetDlgItem(hWnd, IDC_LOCAL_DRIVECB), &rc);
        icx = lf_an - (buttonRootWidth + 4) * 2 - buttonLMStaticWidth - 4;
        rg_an = buttonUploadWidth + 20;


        //Left

        ::MoveWindow(::GetDlgItem(hWnd, IDC_LOCAL_DRIVECB), 4, 4, icx, rc.bottom - rc.top, TRUE);
        ::MoveWindow(::GetDlgItem(hWnd, IDC_LM_STATIC), 4 + icx + 7, 4, buttonLMStaticWidth, buttonUploadHeight, TRUE);
        ::MoveWindow(::GetDlgItem(hWnd, IDC_LOCAL_ROOTB), 4 + icx + 7 + buttonLMStaticWidth + 7, 4, buttonRootWidth, buttonRootHeight,
                   TRUE);
        ::MoveWindow(::GetDlgItem(hWnd, IDC_LOCAL_UPB), 4 + icx + 7 + buttonLMStaticWidth + 7 + buttonRootWidth + 4, 4, buttonRootWidth,
                   buttonUploadHeight, TRUE);
        ::MoveWindow(::GetDlgItem(hWnd, IDC_CURR_LOCAL), 4, buttonUploadHeight + 4, lf_an, buttonUploadHeight, TRUE);
        ::MoveWindow(::GetDlgItem(hWnd, IDC_LOCAL_FILELIST), 4, (buttonUploadHeight + 4) * 2, lf_an, icy, TRUE);

        //Right
        ::GetWindowRect(::GetDlgItem(hWnd, IDC_REMOTE_DRIVECB), &rc);
        ::MoveWindow(::GetDlgItem(hWnd, IDC_REMOTE_DRIVECB), lf_an + rg_an, 4, icx, rc.bottom - rc.top, TRUE);
        ::MoveWindow(::GetDlgItem(hWnd, IDC_RM_STATIC), lf_an + rg_an + icx + 7, 4, buttonLMStaticWidth, buttonUploadHeight, TRUE);
        ::MoveWindow(::GetDlgItem(hWnd, IDC_REMOTE_ROOTB), lf_an + rg_an + icx + 7 + buttonLMStaticWidth + 7, 4, buttonRootWidth,
                   buttonRootHeight, TRUE);
        ::MoveWindow(::GetDlgItem(hWnd, IDC_REMOTE_UPB), lf_an + rg_an + icx + 7 + buttonLMStaticWidth + 7 + buttonRootWidth + 4, 4,
                   buttonRootWidth, buttonUploadHeight, TRUE);
        ::MoveWindow(::GetDlgItem(hWnd, IDC_CURR_REMOTE), lf_an + rg_an, buttonUploadHeight + 4, lf_an, buttonUploadHeight, TRUE);
        ::MoveWindow(::GetDlgItem(hWnd, IDC_REMOTE_FILELIST), lf_an + rg_an, (buttonUploadHeight + 4) * 2, lf_an, icy, TRUE);


        //Bottom
        icyb = icy + 7 + (buttonUploadHeight + 4) * 2;
        ::MoveWindow(::GetDlgItem(hWnd, IDC_LOCAL_STATUS), 4, icyb, lf_an, buttonStatusHeight, TRUE);
        ::MoveWindow(::GetDlgItem(hWnd, IDC_REMOTE_STATUS), lf_an + rg_an, icyb, lf_an, buttonStatusHeight, TRUE);

        ::MoveWindow(::GetDlgItem(hWnd, IDC_HS_STATIC), 8, icyb + 7 + buttonStatusHeight, buttonUploadWidth, buttonStatusHeight, TRUE);
        ::GetWindowRect(::GetDlgItem(hWnd, IDC_HISTORY_CB), &rc);
        ::MoveWindow(::GetDlgItem(hWnd, IDC_HISTORY_CB), buttonUploadWidth + 4, icyb + 7 + buttonStatusHeight,
                   cx - buttonUploadWidth * 4, rc.bottom - rc.top, TRUE);

        ::MoveWindow(::GetDlgItem(hWnd, IDC_PR_STATIC), 8, icyb + 7 + buttonStatusHeight * 2, buttonUploadWidth, buttonProgressHeight,
                   TRUE);
        ::MoveWindow(::GetDlgItem(hWnd, IDC_PROGRESS), buttonUploadWidth + 4, icyb + 7 + buttonStatusHeight * 2,
                   cx - buttonUploadWidth * 4, buttonProgressHeight, TRUE);
        ::MoveWindow(::GetDlgItem(hWnd, IDC_PERCENT), cx - buttonUploadWidth * 2 - 12, icyb + 7 + buttonStatusHeight * 2,
                   buttonUploadWidth, buttonProgressHeight, TRUE);
        ::MoveWindow(::GetDlgItem(hWnd, IDC_GLOBAL_STATUS), cx - buttonUploadWidth * 1 - 18, icyb + 7 + buttonStatusHeight * 2,
                   buttonUploadWidth, buttonProgressHeight, TRUE);



        ::GetWindowRect(::GetDlgItem(hWnd, IDC_STATUS), &rc);
        ::MoveWindow(::GetDlgItem(hWnd, IDC_STATUS), 0, cy - (rc.bottom - rc.top), cx, rc.bottom - rc.top, TRUE);

        //Center

        ::MoveWindow(::GetDlgItem(hWnd, IDC_UPLOAD_B), lf_an + 10 + 2, icy - (buttonUploadHeight + 4) * 12, buttonUploadWidth,
                   buttonUploadHeight, TRUE);
        ::MoveWindow(::GetDlgItem(hWnd, IDC_DOWNLOAD_B), lf_an + 10 + 2, icy - (buttonUploadHeight + 4) * 11, buttonUploadWidth,
                   buttonUploadHeight, TRUE);
        ::MoveWindow(::GetDlgItem(hWnd, IDC_ABORT_B), lf_an + 10 + 2, icy - (buttonUploadHeight + 4) * 10, buttonUploadWidth,
                   buttonUploadHeight, TRUE);
        ::MoveWindow(::GetDlgItem(hWnd, IDC_ABORT_B2), lf_an + 10 + 2, icy - (buttonUploadHeight + 4) * 9, buttonUploadWidth,
                   buttonUploadHeight, TRUE);
        ::MoveWindow(::GetDlgItem(hWnd, IDC_DELETE_B), lf_an + 10 + 2, icy - (buttonUploadHeight + 4) * 7, buttonUploadWidth,
                   buttonUploadHeight, TRUE);
        ::MoveWindow(::GetDlgItem(hWnd, IDC_NEWFOLDER_B), lf_an + 10 + 2, icy - (buttonUploadHeight + 4) * 6, buttonUploadWidth,
                   buttonUploadHeight, TRUE);
        ::MoveWindow(::GetDlgItem(hWnd, IDC_RENAME_B), lf_an + 10 + 2, icy - (buttonUploadHeight + 4) * 5, buttonUploadWidth,
                   buttonUploadHeight, TRUE);
        ::MoveWindow(::GetDlgItem(hWnd, IDC_REFRESH), lf_an + 10 + 2, icy - (buttonUploadHeight + 4) * 4, buttonUploadWidth,
                   buttonUploadHeight, TRUE);
        ::MoveWindow(::GetDlgItem(hWnd, IDC_HIDE_B), lf_an + 10 + 2, icy - (buttonUploadHeight + 4) * 2, buttonUploadWidth,
                   buttonUploadHeight, TRUE);
        ::MoveWindow(::GetDlgItem(hWnd, IDCANCEL), lf_an + 10 + 2, icy - (buttonUploadHeight + 4), buttonUploadWidth,
                   buttonUploadHeight, TRUE);
        ::MoveWindow(::GetDlgItem(hWnd, IDCANCEL2), lf_an + 10 + 2, icy, buttonUploadWidth, buttonUploadHeight, TRUE);
        ::InvalidateRect(hWnd, nullptr, FALSE);

        FTAdjustFileNameColumns(hWnd); // sf@2006

        return TRUE;


    // Messages from ListViews
    case WM_NOTIFY: {
        LPNMLISTVIEW lpNmlv = reinterpret_cast<LPNMLISTVIEW>(lParam);
        [[maybe_unused]] LV_DISPINFO* pLvdi = reinterpret_cast<LV_DISPINFO*>(lParam);


        switch (lpNmlv->hdr.code)
        {
        case HDN_ITEMCLICK:
            return TRUE;

        case NM_CUSTOMDRAW: {
            LPNMLVCUSTOMDRAW lpNmlvcd = reinterpret_cast<LPNMLVCUSTOMDRAW>(lParam);
            if ((lpNmlvcd->nmcd.hdr.hwndFrom != ::GetDlgItem(hWnd, IDC_LOCAL_FILELIST)) &&
                (lpNmlvcd->nmcd.hdr.hwndFrom != ::GetDlgItem(hWnd, IDC_REMOTE_FILELIST)))
            {
                break;
            }

            switch (lpNmlvcd->nmcd.dwDrawStage)
            {
            case CDDS_PREPAINT:
                ::SetWindowLongPtr(hWnd, DWLP_MSGRESULT, CDRF_NOTIFYITEMDRAW);
                return TRUE;
            case CDDS_ITEMPREPAINT: {
                LVITEMW ItemW;
                std::memset(&ItemW, 0, sizeof(ItemW));
                ItemW.mask = LVIF_PARAM;
                ItemW.iItem = static_cast<int>(lpNmlvcd->nmcd.dwItemSpec);
                ItemW.iSubItem = 0;
                if (::SendMessageW(lpNmlvcd->nmcd.hdr.hwndFrom, LVM_GETITEMW, 0, reinterpret_cast<LPARAM>(&ItemW)))
                {
                    if ((ItemW.lParam & FT_LPARAM_UNREADABLE) == FT_LPARAM_UNREADABLE)
                    {
                        lpNmlvcd->clrText = RGB(255, 0, 0);
                        // Request subitem notifications to color all subitems red
                        ::SetWindowLongPtr(hWnd, DWLP_MSGRESULT, CDRF_NOTIFYSUBITEMDRAW);
                        return TRUE;
                    }
                }
                // Request subitem notifications to strip ISO prefix from date column
                ::SetWindowLongPtr(hWnd, DWLP_MSGRESULT, CDRF_NOTIFYSUBITEMDRAW);
                return TRUE;
            }
            case CDDS_ITEMPREPAINT | CDDS_SUBITEM: {
                // For date column (subitem 2): strip ISO sort prefix before display
                if (lpNmlvcd->iSubItem == 2)
                {
                    std::array<WCHAR, 128U> szFull{ 0 };
                    LVITEMW getItem;
                    std::memset(&getItem, 0, sizeof(getItem));
                    getItem.mask = LVIF_TEXT;
                    getItem.iItem = static_cast<int>(lpNmlvcd->nmcd.dwItemSpec);
                    getItem.iSubItem = 2;
                    getItem.pszText = szFull.data();
                    getItem.cchTextMax = 128;
                    ::SendMessageW(lpNmlvcd->nmcd.hdr.hwndFrom, LVM_GETITEMW, 0, reinterpret_cast<LPARAM>(&getItem));
                    WCHAR* pSep = std::wcschr(szFull.data(), L'|');
                    if (pSep)
                    {
                        // Draw the locale date ourselves and suppress default drawing
                        RECT rcInner;
                        rcInner.left = LVIR_BOUNDS; // Use BOUNDS for subitems, not LABEL
                        rcInner.top = lpNmlvcd->iSubItem;
                        ::SendMessageW(lpNmlvcd->nmcd.hdr.hwndFrom, LVM_GETSUBITEMRECT, static_cast<WPARAM>(lpNmlvcd->nmcd.dwItemSpec),
                                     reinterpret_cast<LPARAM>(&rcInner));
                        // Use actual selection state from ListView
                        BOOL bSelected = (ListView_GetItemState(lpNmlvcd->nmcd.hdr.hwndFrom,
                                                                static_cast<int>(lpNmlvcd->nmcd.dwItemSpec), LVIS_SELECTED) &
                                          LVIS_SELECTED) != 0;
                        BOOL bFocused = (::GetFocus() == lpNmlvcd->nmcd.hdr.hwndFrom);
                        // Fill background since CDRF_SKIPDEFAULT skips default painting
                        HBRUSH hBrush;
                        if (bSelected && bFocused)
                        {
                            hBrush = ::CreateSolidBrush(::GetSysColor(COLOR_HIGHLIGHT));
                        }
                        else if (bSelected)
                        {
                            hBrush = ::CreateSolidBrush(::GetSysColor(COLOR_BTNFACE));
                        }
                        else
                        {
                            hBrush = ::CreateSolidBrush(::GetSysColor(COLOR_WINDOW));
                        }
                        ::FillRect(lpNmlvcd->nmcd.hdc, &rcInner, hBrush);
                        ::DeleteObject(hBrush);
                        ::SetBkMode(lpNmlvcd->nmcd.hdc, TRANSPARENT);
                        ::SetTextColor(lpNmlvcd->nmcd.hdc,
                                     (bSelected && bFocused) ? ::GetSysColor(COLOR_HIGHLIGHTTEXT) : lpNmlvcd->clrText);
                        rcInner.left += 4; // small indent
                        ::DrawTextW(lpNmlvcd->nmcd.hdc, pSep + 1, -1, &rcInner, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                        ::SetWindowLongPtr(hWnd, DWLP_MSGRESULT, CDRF_SKIPDEFAULT);
                        return TRUE;
                    }
                }
                ::SetWindowLongPtr(hWnd, DWLP_MSGRESULT, CDRF_DODEFAULT);
                return TRUE;
            }
            }
        }
        break;

        case NM_SETFOCUS:
            if (lpNmlv->hdr.hwndFrom == ::GetDlgItem(hWnd, IDC_LOCAL_FILELIST))
            {
                _this->m_fFocusLocal = true;
                _this->CheckButtonState(hWnd);
                std::array<wchar_t, 64U> szTxtW;
                HWND hB = ::GetDlgItem(hWnd, IDC_DELETE_B);
                ::_snwprintf_s(szTxtW.data(), 64, _TRUNCATE, L"<- %s", _this->m_szDeleteButtonLabel.data());
                ::SetWindowTextW(hB, szTxtW.data());
                hB = ::GetDlgItem(hWnd, IDC_NEWFOLDER_B);
                ::_snwprintf_s(szTxtW.data(), 64, _TRUNCATE, L"<- %s", _this->m_szNewFolderButtonLabel.data());
                ::SetWindowTextW(hB, szTxtW.data());
                hB = ::GetDlgItem(hWnd, IDC_RENAME_B);
                ::_snwprintf_s(szTxtW.data(), 64, _TRUNCATE, L"<- %s", _this->m_szRenameButtonLabel.data());
                ::SetWindowTextW(hB, szTxtW.data());
                hB = ::GetDlgItem(hWnd, IDC_REFRESH);
                ::_snwprintf_s(szTxtW.data(), 64, _TRUNCATE, L"<- %s", _this->m_szRefreshButtonLabel.data());
                ::SetWindowTextW(hB, szTxtW.data());
            }

            if (lpNmlv->hdr.hwndFrom == ::GetDlgItem(hWnd, IDC_REMOTE_FILELIST))
            {
                _this->m_fFocusLocal = false;
                _this->CheckButtonState(hWnd);
                std::array<wchar_t, 64U> szTxtW;
                HWND hB = ::GetDlgItem(hWnd, IDC_DELETE_B);
                ::_snwprintf_s(szTxtW.data(), 64, _TRUNCATE, L"%s ->", _this->m_szDeleteButtonLabel.data());
                ::SetWindowTextW(hB, szTxtW.data());
                hB = ::GetDlgItem(hWnd, IDC_NEWFOLDER_B);
                ::_snwprintf_s(szTxtW.data(), 64, _TRUNCATE, L"%s ->", _this->m_szNewFolderButtonLabel.data());
                ::SetWindowTextW(hB, szTxtW.data());
                hB = ::GetDlgItem(hWnd, IDC_RENAME_B);
                ::_snwprintf_s(szTxtW.data(), 64, _TRUNCATE, L"%s ->", _this->m_szRenameButtonLabel.data());
                ::SetWindowTextW(hB, szTxtW.data());
                hB = ::GetDlgItem(hWnd, IDC_REFRESH);
                ::_snwprintf_s(szTxtW.data(), 64, _TRUNCATE, L"%s ->", _this->m_szRefreshButtonLabel.data());
                ::SetWindowTextW(hB, szTxtW.data());
            }
            return TRUE;

        case NM_DBLCLK:
            if (lpNmlv->hdr.hwndFrom == ::GetDlgItem(hWnd, IDC_LOCAL_FILELIST))
            {
                _this->PopulateLocalListBoxW(hWnd, L"");
                _this->CheckButtonState(hWnd);
            }

            if (lpNmlv->hdr.hwndFrom == ::GetDlgItem(hWnd, IDC_REMOTE_FILELIST))
            {
                if (!_this->m_fFileCommandPending)
                {
                    _this->m_fFileCommandPending = true;
                    _this->RequestRemoteDirectoryContent(hWnd, L"");
                }
            }
            return TRUE;

        // sf@2006 - 2007 - Added in the ability to sort on the headers of the ListView
        // Modif initiated by TAW. Thanks !
        case LVN_COLUMNCLICK:
            if (lpNmlv->hdr.hwndFrom == ::GetDlgItem(hWnd, IDC_LOCAL_FILELIST))
            {
                int c = static_cast<LPARAM>(lpNmlv->iSubItem);
                _this->bSortDirectionsL[static_cast<unsigned long long>(c)] = !(_this->bSortDirectionsL[static_cast<unsigned long long>(c)]);
                ::SendMessage(lpNmlv->hdr.hwndFrom, LVM_FIRST + 81 /*LVM_SORTITEMSEX*/,
                              static_cast<WPARAM>(static_cast<LPARAM>(_this->bSortDirectionsL[c] ? c + 1 : ((c + 1) * -1))),
                              reinterpret_cast<LPARAM>(reinterpret_cast<PFNLVCOMPARE>(ListViewLocalCompareProc)));
            }

            if (lpNmlv->hdr.hwndFrom == ::GetDlgItem(hWnd, IDC_REMOTE_FILELIST))
            {
                int c = static_cast<LPARAM>(lpNmlv->iSubItem);
                _this->bSortDirectionsR[static_cast<unsigned long long>(c)] = !(_this->bSortDirectionsR[static_cast<unsigned long long>(c)]);
                ::SendMessage(lpNmlv->hdr.hwndFrom, LVM_FIRST + 81 /*LVM_SORTITEMSEX*/,
                              static_cast<WPARAM>(static_cast<LPARAM>(_this->bSortDirectionsR[c] ? c + 1 + 10 : ((c + 1 + 10) * -1))),
                              reinterpret_cast<LPARAM>(reinterpret_cast<PFNLVCOMPARE>(ListViewLocalCompareProc)));
            }
            return TRUE;
        } // end switch(lpNmlv->hdr.code)
    } // end WM_NOTIFY block
    break;


    case WM_DESTROY:
        if (_this->m_timer != 0)
        {
            ::KillTimer(hWnd, static_cast<UINT_PTR>(_this->m_timer));
        }
        ::KillTimer(hWnd, 100);
        ::EndDialog(hWnd, FALSE);
        return TRUE;

    case WM_SHOWWINDOW:
        // adzm - fix for window not restoring when hidden via show desktop, win+d, etc
        if (!wParam)
        {
            _this->m_fVisible = false;
        }
        break;
    }
    /*
    // Process File Transfer asynchronous Send Packet Message
    if (uMsg == FileTransferSendPacketMessage)
    {
        _this->SendFileChunk();
        return 0;
    }
    */

    return 0;
}


//
// Params acquisition Dialog Box
//
int FileTransfer::DoFTParamDialog(const LPWSTR szTitle, const LPWSTR szComment)
{
    extern HINSTANCE m_hInstResDLL;
    ::wcscpy_s(m_szFTParamTitle.data(), m_szFTParamTitle.size(), szTitle);
    ::wcscpy_s(m_szFTParamComment.data(), m_szFTParamComment.size(), szComment);
    return static_cast<int>(::DialogBoxParam(m_hInstResDLL, DIALOG_MAKEINTRESOURCE(IDD_FTPARAM_DLG), hWnd, reinterpret_cast<DLGPROC>(FTParamDlgProc), reinterpret_cast<LONG_PTR>(this)));
}


BOOL CALLBACK FileTransfer::FTParamDlgProc(const HWND hwnd, const UINT uMsg, const WPARAM wParam, const LPARAM lParam)
{
    FileTransfer* _this = helper::SafeGetWindowUserData<FileTransfer>(hwnd);
    switch (uMsg)
    {
    case WM_INITDIALOG: {
        HICON hIcon = ::LoadIcon(::GetModuleHandle(nullptr), MAKEINTRESOURCE(IDR_TRAY));
        ::SendMessage(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(hIcon));
        ::SendMessage(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(hIcon));
        helper::SafeSetWindowUserData(hwnd, lParam);
        _this = reinterpret_cast<FileTransfer*>(lParam);
        //CentreWindow(hwnd);

        // Set Title
        ::SetWindowTextW(hwnd, _this->m_szFTParamTitle.data());
        // Set Comment
        ::SetDlgItemTextW(hwnd, IDC_FTPARAMCOMMENT, _this->m_szFTParamComment.data());
        // Set param initial value
        ::SetDlgItemTextW(hwnd, IDC_FTPARAM_EDIT, _this->m_szFTParam.data());

        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_FTPARAM_EDIT:
            if (HIWORD(wParam) == EN_CHANGE)
            {
                std::size_t length = static_cast<std::size_t>(::GetWindowTextLength(::GetDlgItem(hwnd, IDC_FTPARAM_EDIT)));
                bool stringOk = length > 0;

                if (stringOk)
                {
                    std::wstring text;
                    text.resize(length + 1);
                    ::GetDlgItemTextW(hwnd, IDC_FTPARAM_EDIT, &*text.begin(), static_cast<int>((length + 1)));

                    std::wstring::size_type pos;
                    pos = text.find_first_not_of(L" \t\n\r");
                    if ((pos == std::wstring::npos) || (pos >= std::wcslen(text.c_str())))
                    {
                        stringOk = false;
                    }
                }

                ::EnableWindow(::GetDlgItem(hwnd, IDOK), stringOk);
            }
            break;

        case IDOK: {
            ::GetDlgItemTextW(hwnd, IDC_FTPARAM_EDIT, _this->m_szFTParamW.data(), 256);
            ::wcscpy_s(_this->m_szFTParam.data(), _this->m_szFTParam.size(), _this->m_szFTParamW.data());
            ::EndDialog(hwnd, TRUE);
            return TRUE;
        }
        case IDCANCEL:
            _this->m_szFTParamW[0U] = L'\0';
            _this->m_szFTParam[0U] = L'\0';
            ::EndDialog(hwnd, FALSE);
            return TRUE;
        }
        break;
    case WM_DESTROY:
        ::EndDialog(hwnd, FALSE);
        return TRUE;
    }
    return 0;
}


//
// Params acquisition Dialog Box
//
int FileTransfer::DoFTConfirmDialog(const LPWSTR szTitle, const LPWSTR szComment)
{
    extern HINSTANCE m_hInstResDLL;
    ::wcscpy_s(m_szFTConfirmTitle.data(), m_szFTConfirmTitle.size(), szTitle);
    ::wcscpy_s(m_szFTConfirmComment.data(), m_szFTConfirmComment.size(), szComment);
    m_nConfirmAnswer = CONFIRM_NO;
    return static_cast<int>(::DialogBoxParam(m_hInstResDLL, DIALOG_MAKEINTRESOURCE(IDD_FTCONFIRM_DLG), hWnd, reinterpret_cast<DLGPROC>(FTConfirmDlgProc),
                          reinterpret_cast<LONG_PTR>(this)));
}


BOOL CALLBACK FileTransfer::FTConfirmDlgProc(const HWND hwnd, const UINT uMsg, const WPARAM wParam, const LPARAM lParam)
{
    FileTransfer* _this = helper::SafeGetWindowUserData<FileTransfer>(hwnd);
    switch (uMsg)
    {
    case WM_INITDIALOG: {
        HICON hIcon = ::LoadIcon(::GetModuleHandle(nullptr), MAKEINTRESOURCE(IDR_TRAY));
        ::SendMessage(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(hIcon));
        ::SendMessage(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(hIcon));
        helper::SafeSetWindowUserData(hwnd, lParam);
        _this = reinterpret_cast<FileTransfer*>(lParam);
        //CentreWindow(hwnd);

        // Set Title
        ::SetWindowTextW(hwnd, _this->m_szFTConfirmTitle.data());
        // Set Comment
        ::SetDlgItemTextW(hwnd, IDC_FTPCONFIRMCOMMENT, _this->m_szFTConfirmComment.data());

        // Todo: Init buttons labels with corrsponding culture (En, Fr, De...)
        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_YES_B:
            _this->m_nConfirmAnswer = CONFIRM_YES;
            ::EndDialog(hwnd, TRUE);
            return TRUE;

        case IDC_YESALL_B:
            _this->m_nConfirmAnswer = CONFIRM_YESALL;
            ::EndDialog(hwnd, TRUE);
            return TRUE;

        case IDC_NO_B:
            _this->m_nConfirmAnswer = CONFIRM_NO;
            ::EndDialog(hwnd, TRUE);
            return TRUE;

        case IDC_NOALL_B:
            _this->m_nConfirmAnswer = CONFIRM_NOALL;
            ::EndDialog(hwnd, TRUE);
            return TRUE;

        case IDOK:
            ::EndDialog(hwnd, TRUE);
            return TRUE;

        case IDCANCEL:
            ::EndDialog(hwnd, FALSE);
            return TRUE;
        }
        break;

    case WM_DESTROY:
        ::EndDialog(hwnd, FALSE);
        return TRUE;
    }
    return 0;
}

//
// DoUploadFile: generic headless upload of a single file.
// Uploads szLocalPath to szRemotePath on the remote server, then disconnects.
// Works against any old UltraVNC server with file transfer enabled.
//
void FileTransfer::DoUploadFile(const LPCWSTR szLocalPath, const LPCWSTR szRemotePath)
{
    // Set IMMEDIATELY - before any network I/O or message pumping.
    // The MM timer (started by SendFiles on the receive thread) may fire
    // before we reach the permission-wait loop, so the flag must be set here.
    g_fHeadlessUpload = true;

    if (!szLocalPath || (szLocalPath[0U] == L'\0'))
    {
        g_fHeadlessUpload = false;
        ::MessageBoxW(nullptr, L"Local path is empty", L"Upload Error", MB_OK | MB_ICONERROR);
        return;
    }
    if (!szRemotePath || (szRemotePath[0U] == L'\0'))
    {
        g_fHeadlessUpload = false;
        ::MessageBoxW(nullptr, L"Remote path is empty", L"Upload Error", MB_OK | MB_ICONERROR);
        return;
    }

    // Verify the local file exists
    if (::GetFileAttributesW(szLocalPath) == INVALID_FILE_ATTRIBUTES)
    {
        g_fHeadlessUpload = false;
        std::array<WCHAR, 512U> msg;
        ::_snwprintf_s(msg.data(), 512, _TRUNCATE, L"Local file not found:\n%s", szLocalPath);
        ::MessageBoxW(nullptr, msg.data(), L"Upload Error", MB_OK | MB_ICONERROR);
        return;
    }

    // --- Phase 1: Request FT permission and wait for server grant ---
    // When called from WM_APP+1 (after dialog init), permission is already granted
    // by the dialog's own initialization flow. Only request if not yet granted.
    if (!m_fFTAllowed)
    {
        m_fFileCommandPending = false;
        RequestPermission();

        DWORD dwPermStart = ::GetTickCount();
        while (!m_fFTAllowed && !m_fAbort)
        {
            MSG msg;
            while (::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
            {
                ::TranslateMessage(&msg);
                ::DispatchMessage(&msg);
            }
            if (::GetTickCount() - dwPermStart > 10000)
            {
                break;
            }
            ::Sleep(50);
        }

        if (!m_fFTAllowed)
        {
            g_fHeadlessUpload = false;
            ::MessageBoxW(nullptr, L"File transfer permission not granted by server.\nTimeout after 10 seconds.", L"Upload Error",
                        MB_OK | MB_ICONERROR);
            return;
        }
    }

    // Ensure remote directory exists (extract parent directory from full file path)
    std::array<WCHAR, MAX_PATH * 2> szRemoteDir;
    ::wcscpy_s(szRemoteDir.data(), szRemoteDir.size(), szRemotePath);
    WCHAR* pLastSlash = std::wcsrchr(szRemoteDir.data(), L'\\');
    if (pLastSlash != nullptr)
    {
        *pLastSlash = L'\0';
        CreateRemoteDirectoryPath(szRemoteDir.data());
    }

    // --- Phase 2: Upload the file ---
    ::wcscpy_s(m_szHeadlessRemotePathW.data(), m_szHeadlessRemotePathW.size(), szRemotePath);
    m_fHeadlessUpload = true;
    // g_fHeadlessUpload already set to true above before message pumping started
#if DEBUG_FT
    {
        std::array<WCHAR, 512U> _dbg;
        ::_snwprintf_s(_dbg.data(), 512, _TRUNCATE, L"=== DoUploadFile: this=0x%p Set g_fHeadlessUpload=true m_szHeadlessRemotePathW=[%s]\n", reinterpret_cast<void*>(this), m_szHeadlessRemotePathW.data());
        ::OutputDebugStringW(_dbg.data());
    }
#endif

    m_fFileCommandPending = true;
    m_fAbort = false;
    m_fAborted = false;
    m_fUserAbortedFileTransfer = false;
    m_fFileUploadError = false;

#if DEBUG_FT
    {
        std::array<WCHAR, 512U> _dbg;
        ::_snwprintf_s(_dbg.data(), 512, _TRUNCATE, L"=== DoUploadFile: About to call OfferLocalFileW, m_szHeadlessRemotePathW=[%s]\n", m_szHeadlessRemotePathW.data());
        ::OutputDebugStringW(_dbg.data());
    }
#endif
    if (OfferLocalFileW(szLocalPath))
    {
        DWORD dwStart = ::GetTickCount();
        while (m_fFileUploadRunning && !m_fAbort)
        {
            MSG msg;
            while (::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
            {
                ::TranslateMessage(&msg);
                ::DispatchMessage(&msg);
            }
            if (::GetTickCount() - dwStart > 300000)
            {
                break;
            } // 5 min timeout
            ::Sleep(50);
        }
    }

    // Give server time to process rfbEndOfFile and rename the file from !UVNCPFT-filename to filename
    // The server renames the file in FinishFileReception() after receiving rfbEndOfFile
    // We need to wait long enough for the server to complete this operation before disconnecting
    // Note: Don't pump messages here as it can cause crashes during cleanup
    if (!m_fAbort && !m_fFileUploadError)
    {
        ::Sleep(2000);
    }

    // Clean up
    m_szHeadlessRemotePathW[0U] = L'\0';
    m_fHeadlessUpload = false;
    g_fHeadlessUpload = false;
    EndFTSession();

    // Show completion status
    if (m_fAbort)
    {
        ::MessageBoxW(nullptr, L"Upload was aborted.", L"Upload Status", MB_OK | MB_ICONWARNING);
    }
    else if (m_fFileUploadError)
    {
        ::MessageBoxW(nullptr, L"Upload failed with errors.", L"Upload Status", MB_OK | MB_ICONERROR);
    }
    else
    {
        std::array<WCHAR, 512U> msg;
        ::_snwprintf_s(msg.data(), 512, _TRUNCATE, L"Upload completed successfully!\n\nLocal: %s\nRemote: %s", szLocalPath, szRemotePath);
        ::MessageBoxW(nullptr, msg.data(), L"Upload Complete", MB_OK | MB_ICONINFORMATION);
    }
    // Caller will disconnect
}

//
//
void FileTransfer::DisableButtons(const HWND hWndIn, const bool X)
{
    ::ShowWindow(::GetDlgItem(hWndIn, IDC_UPLOAD_B), SW_HIDE);
    ::ShowWindow(::GetDlgItem(hWndIn, IDC_DOWNLOAD_B), SW_HIDE);
    ::ShowWindow(::GetDlgItem(hWndIn, IDC_DELETE_B), SW_HIDE);
    ::ShowWindow(::GetDlgItem(hWndIn, IDC_NEWFOLDER_B), SW_HIDE);
    ::ShowWindow(::GetDlgItem(hWndIn, IDC_RENAME_B), SW_HIDE);
    ::ShowWindow(::GetDlgItem(hWndIn, IDCANCEL), SW_HIDE);
    ::ShowWindow(::GetDlgItem(hWndIn, IDCANCEL2), SW_HIDE);
    ::ShowWindow(::GetDlgItem(hWndIn, IDC_HIDE_B), SW_SHOW);
    ::EnableWindow(::GetDlgItem(hWndIn, IDC_LOCAL_FILELIST), FALSE);
    ::EnableWindow(::GetDlgItem(hWndIn, IDC_LOCAL_DRIVECB), FALSE);
    ::EnableWindow(::GetDlgItem(hWndIn, IDC_LOCAL_ROOTB), FALSE);
    ::EnableWindow(::GetDlgItem(hWndIn, IDC_LOCAL_UPB), FALSE);
    ::EnableWindow(::GetDlgItem(hWndIn, IDC_REFRESH), FALSE);
    ::EnableWindow(::GetDlgItem(hWndIn, IDC_REMOTE_FILELIST), FALSE);
    ::EnableWindow(::GetDlgItem(hWndIn, IDC_REMOTE_DRIVECB), FALSE);
    ::EnableWindow(::GetDlgItem(hWndIn, IDC_REMOTE_ROOTB), FALSE);
    ::EnableWindow(::GetDlgItem(hWndIn, IDC_REMOTE_UPB), FALSE);

    // Disable Close Window in in title bar
    if (X == true)
    {
        HMENU hMenu = ::GetSystemMenu(hWndIn, 0);
        int nCount = ::GetMenuItemCount(hMenu);
        ::EnableMenuItem(hMenu, static_cast<UINT>(nCount - 1), MF_DISABLED | MF_GRAYED | MF_BYPOSITION);
        ::EnableMenuItem(hMenu, static_cast<UINT>(nCount - 2), MF_DISABLED | MF_GRAYED | MF_BYPOSITION);
    }
    ::DrawMenuBar(hWndIn);
}
void FileTransfer::CheckButtonState(const HWND hWndIn)
{
    //    bool bEnable = m_fFocusLocal && ||
    bool bEnable;
    bool localDirSet, remoteDirSet;

    localDirSet = ::GetWindowTextLength(::GetDlgItem(hWndIn, IDC_CURR_LOCAL)) > 0;
    remoteDirSet = ::GetWindowTextLength(::GetDlgItem(hWndIn, IDC_CURR_REMOTE)) > 0;
    bEnable = m_fFocusLocal ? localDirSet : remoteDirSet;

    ::EnableWindow(::GetDlgItem(hWndIn, IDC_UPLOAD_B), (localDirSet && remoteDirSet));
    ::EnableWindow(::GetDlgItem(hWndIn, IDC_DOWNLOAD_B), (localDirSet && remoteDirSet));
    ::EnableWindow(::GetDlgItem(hWndIn, IDC_DELETE_B), bEnable);
    ::EnableWindow(::GetDlgItem(hWndIn, IDC_NEWFOLDER_B), bEnable);
    ::EnableWindow(::GetDlgItem(hWndIn, IDC_RENAME_B), bEnable);
    //	EnableWindow(GetDlgItem(hWnd, IDCANCEL), bEnable);
    //	EnableWindow(GetDlgItem(hWnd, IDC_HIDE_B), bEnable);
}

//
//
//
void FileTransfer::EnableButtons(const HWND hWndIn)
{
    ::ShowWindow(::GetDlgItem(hWndIn, IDC_ABORT_B), SW_HIDE);
    ::ShowWindow(::GetDlgItem(hWndIn, IDC_ABORT_B2), SW_HIDE);
    ::ShowWindow(::GetDlgItem(hWndIn, IDC_UPLOAD_B), SW_SHOW);
    ::ShowWindow(::GetDlgItem(hWndIn, IDC_DOWNLOAD_B), SW_SHOW);
    ::ShowWindow(::GetDlgItem(hWndIn, IDCANCEL), SW_SHOW);
    ::ShowWindow(::GetDlgItem(hWndIn, IDCANCEL2), SW_SHOW);
    ::ShowWindow(::GetDlgItem(hWndIn, IDC_DELETE_B), SW_SHOW);
    ::ShowWindow(::GetDlgItem(hWndIn, IDC_NEWFOLDER_B), SW_SHOW);
    if (!UsingOldProtocol())
    {
        ::ShowWindow(::GetDlgItem(hWndIn, IDC_RENAME_B), SW_SHOW);
    }
    ::EnableWindow(::GetDlgItem(hWndIn, IDC_LOCAL_FILELIST), TRUE);
    ::EnableWindow(::GetDlgItem(hWndIn, IDC_LOCAL_DRIVECB), TRUE);
    ::EnableWindow(::GetDlgItem(hWndIn, IDC_LOCAL_ROOTB), TRUE);
    ::EnableWindow(::GetDlgItem(hWndIn, IDC_LOCAL_UPB), TRUE);
    ::EnableWindow(::GetDlgItem(hWndIn, IDC_REFRESH), TRUE);
    ::EnableWindow(::GetDlgItem(hWndIn, IDC_REMOTE_FILELIST), TRUE);
    ::EnableWindow(::GetDlgItem(hWndIn, IDC_REMOTE_DRIVECB), TRUE);
    ::EnableWindow(::GetDlgItem(hWndIn, IDC_REMOTE_ROOTB), TRUE);
    ::EnableWindow(::GetDlgItem(hWndIn, IDC_REMOTE_UPB), TRUE);
    // Disable Close Window in in title bar
    HMENU hMenu = ::GetSystemMenu(hWndIn, 0);
    int nCount = ::GetMenuItemCount(hMenu);
    ::EnableMenuItem(hMenu, static_cast<UINT>(nCount - 1), MF_ENABLED | MF_BYPOSITION);
    ::EnableMenuItem(hMenu, static_cast<UINT>(nCount - 2), MF_ENABLED | MF_BYPOSITION);
    ::DrawMenuBar(hWndIn);

    // Reset progress bar and status after transfer completes or is aborted
    HWND hProg = ::GetDlgItem(hWndIn, IDC_PROGRESS);
    ::SendMessage(hProg, PBM_SETMARQUEE, FALSE, 0);
    ::SetWindowLong(hProg, GWL_STYLE, ::GetWindowLong(hProg, GWL_STYLE) & ~PBS_MARQUEE);
    ::SendMessage(hProg, PBM_SETPOS, 0, 0);
    ::SetDlgItemTextW(hWndIn, IDC_PERCENT, L"");
    ::SetDlgItemTextW(hWndIn, IDC_GLOBAL_STATUS, L"");
}



void FileTransfer::InitListViewImagesList(const HWND hListView)
{
    HIMAGELIST hImageList = ::ImageList_Create(16, 16, ILC_MASK, 2, 2);

    // Load icons from main executable, not language DLL
    extern HINSTANCE hInstance;
    HICON hIcon = ::LoadIcon(hInstance, MAKEINTRESOURCE(IDI_DIR));
    ::ImageList_AddIcon(hImageList, hIcon);
    ::DestroyIcon(hIcon);

    hIcon = ::LoadIcon(hInstance, MAKEINTRESOURCE(IDI_FILE));
    ::ImageList_AddIcon(hImageList, hIcon);
    ::DestroyIcon(hIcon);

    hIcon = ::LoadIcon(hInstance, MAKEINTRESOURCE(IDI_DRIVE));
    ::ImageList_AddIcon(hImageList, hIcon);
    ::DestroyIcon(hIcon);

    ListView_SetImageList(hListView, hImageList, LVSIL_SMALL);
}


// sf@2007 - Column sorting comparizon function
// Modif initiated by TAW. Thanks !
int CALLBACK FileTransfer::ListViewLocalCompareProc(const LPARAM lParam1, const LPARAM lParam2, LPARAM lParamSort)
{
    int iResult{ 0 };
    bool bSortDirection{ true };
    bool bRemoteList{ false };
    std::array<TCHAR, 255U> szBuf1;
    std::array<TCHAR, 255U> szBuf2;

    if (lParamSort < 0)
    {
        bSortDirection = false;
        lParamSort *= -1;
    }
    if (lParamSort >= 10)
    {
        lParamSort -= 10;
        bRemoteList = true;
    }
    lParamSort -= 1;

    HWND hWndList = ::GetDlgItem(hFTWnd, bRemoteList ? IDC_REMOTE_FILELIST : IDC_LOCAL_FILELIST);

    ListView_GetItemText(hWndList, static_cast<int>(lParam1), static_cast<int>(lParamSort), szBuf1.data(), sizeof(szBuf1))
    ListView_GetItemText(hWndList, static_cast<int>(lParam2), static_cast<int>(lParamSort), szBuf2.data(), sizeof(szBuf2))

    switch (lParamSort)
    {
    case 0: //Sort by name
        iResult = lstrcmpi(szBuf1.data(), szBuf2.data());
        break;
    case 1: //Sort by size
    {
        iResult = lstrcmpi(szBuf1.data(), szBuf2.data());
        __int64 s1 = GetFileSizeFromStringW(szBuf1.data());
        __int64 s2 = GetFileSizeFromStringW(szBuf2.data());
        iResult = ((s1 >= s2) ? 1 : -1);
    }
    break;
    case 2: //Sort by date - stored as "YYYYMMDDHHMMSS|locale_date time", ISO prefix sorts correctly
        iResult = std::wcscmp(szBuf1.data(), szBuf2.data());
        break;
    }

    if (bSortDirection == false)
    {
        iResult *= -1;
    }

    return (iResult);
}


__int64 FileTransfer::GetFileSizeFromStringW(WCHAR* const szSize)
{
    __int64 Size{ 0 };
    unsigned long m{ 0U }, r = 0;
    std::size_t len = std::wcslen(szSize);
    if (len < 2)
    {
        return 0;
    }
    WCHAR ts0 = szSize[len - 1];
    if (ts0 == L'r')
    {
        return -1;
    }
    WCHAR ts1 = szSize[len - 2];
    if (ts1 != L'e')
    {
        swscanf_s(szSize, L"%d.%02d", &m, &r);
    }
    else
    {
        swscanf_s(szSize, L"%d", &m);
    }

    switch (ts1)
    {
    case L'G':
        Size = static_cast<__int64>((m * static_cast<__int64>(1024 * 1024 * 1024))) + static_cast<__int64>((r * static_cast<__int64>(1024 * 1024 * 1024 / 100)));
        break;
    case L'M':
        Size = static_cast<__int64>((m * static_cast<__int64>(1024 * 1024))) + static_cast<__int64>((r * static_cast<__int64>(1024 * 1024 / 100)));
        break;
    case L'K':
        Size = static_cast<__int64>((m * static_cast<__int64>(1024))) + static_cast<__int64>((r * 1024 / 100));
        break;
    case L'e':
        Size = static_cast<__int64>(m);
        break;
    default:
        Size = 0;
        break;
    }
    return Size;
}

// Legacy ANSI version
__int64 FileTransfer::GetFileSizeFromString(char* const szSize)
{
    std::array<WCHAR, 256U> szSizeW;
    ::MultiByteToWideChar(CP_ACP, 0, szSize, -1, szSizeW.data(), 256);
    return GetFileSizeFromStringW(szSizeW.data());
}


FILETIME FileTransfer::GetFileTimeFromStringW(WCHAR* const szFileSystemTime)
{
    SYSTEMTIME FileSystemTime;
    FILETIME LocalFileTime;
    int m{ 0 }, d = 0, y = 0, h = 0, mn = 0;

    if (std::wcslen(szFileSystemTime) == 0)
    {
        m = d = y = h = mn = 0;
    }
    else
    {
        swscanf_s(szFileSystemTime, L"%2d/%2d/%4d %2d:%2d", &m, &d, &y, &h, &mn);
    }
    FileSystemTime.wMonth = static_cast<WORD>(m);
    FileSystemTime.wDay = static_cast<WORD>(d);
    FileSystemTime.wYear = static_cast<WORD>(y);
    FileSystemTime.wHour = static_cast<WORD>(h);
    FileSystemTime.wMinute = static_cast<WORD>(mn);
    FileSystemTime.wSecond = 0;
    FileSystemTime.wMilliseconds = 0;
    FileSystemTime.wDayOfWeek = 0;
    ::SystemTimeToFileTime(&FileSystemTime, &LocalFileTime);
    return LocalFileTime;
}

// Legacy ANSI version
FILETIME FileTransfer::GetFileTimeFromString(char* const szFileSystemTime)
{
    std::array<WCHAR, 256U> szFileSystemTimeW;
    ::MultiByteToWideChar(CP_ACP, 0, szFileSystemTime, -1, szFileSystemTimeW.data(), 256);
    return GetFileTimeFromStringW(szFileSystemTimeW.data());
}
bool FileTransfer::FileOrFolderExists(const HWND fileListWnd, const std::wstring fileOrFolder)
{
    LVFINDINFO Info;
    Info.flags = LVFI_STRING;
    Info.psz = fileOrFolder.c_str();
    const int nTheIndex = ListView_FindItem(fileListWnd, -1, &Info);
    return nTheIndex > -1;
}
