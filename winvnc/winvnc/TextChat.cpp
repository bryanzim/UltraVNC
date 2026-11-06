// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
//

#include "stdhdrs.h"

#include "TextChat.h"

#include "common/win32_helpers.h"
#include "Localization.h" // Act : add localization on messages
#include "resource.h"
#include "richedit.h"
#include "vncclient.h"
#include "vncserver.h"

#include <commctrl.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>

#define TEXTMAXSIZE 16384
#define MAXNAMESIZE 128 // MAX_COMPUTERNAME_LENGTH+1 (32)
#define CHAT_OPEN -1    // Todo; put these codes in rfbproto.h
#define CHAT_CLOSE -2
#define CHAT_FINISHED -3

//	[v1.0.2-jp1 fix]
LRESULT CALLBACK SBProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LONG pDefSBProc{ 0 };
extern HINSTANCE hInstResDLL;

extern HINSTANCE hAppInstance;

//
//
//
#ifdef HIGH_PRECISION
#include <mmsystem.h>

BOOL PlayResource(const LPSTR lpName)
{
    std::array<char, MAX_PATH> szWavFile; //PGM

    if (::GetModuleFileName(nullptr, szWavFile.data(), szWavFile.size())) //PGM

    { // PGM

        char* p = std::strrchr(szWavFile.data(), '\\'); //PGM

        *p = '\0'; //PGM

        ::strcat_s(szWavFile.data(), szWavFile.size(), "\\"); //PGM

    } //PGM

    ::strcat_s(szWavFile.data(), szWavFile.size(), "ding_dong.wav"); //PGM

    if (::PlaySound(szWavFile.data(), nullptr, SND_APPLICATION | SND_FILENAME | SND_ASYNC | SND_NOWAIT) != ERROR_SUCCESS)
    { //PGM

        return FALSE;
    } //PGM

    else
    { //PGM

        return TRUE;
    } //PGM
}
#else
BOOL PlayResource([[maybe_unused]] const LPSTR lpName) { return TRUE; }
#endif

///////////////////////////////////////////////////////
TextChat::TextChat(vncClient* pCC)
{
    m_hDlg = nullptr;
    m_pCC = pCC;
    m_fTextChatRunning = false;

    m_fPersistentTexts = false;
    m_szLocalText = new char[TEXTMAXSIZE]; // Text (message) entered by local user
    std::memset(m_szLocalText, 0, TEXTMAXSIZE);
    m_szLastLocalText = new char[TEXTMAXSIZE]; // Last local text (no more necessary)
    std::memset(m_szLastLocalText, 0, TEXTMAXSIZE);
    m_szRemoteText = new char[TEXTMAXSIZE]; // Incoming remote text (remote message)
    std::memset(m_szRemoteText, 0, TEXTMAXSIZE);
    m_szRemoteName = new char[MAXNAMESIZE]; // Name of remote machine
    std::memset(m_szRemoteName, 0, MAXNAMESIZE);
    m_szLocalName = new char[MAXNAMESIZE]; // Name of local machine
    std::memset(m_szLocalName, 0, MAXNAMESIZE);

    m_szTextBoxBuffer = new char[TEXTMAXSIZE]; // History Text (containing all chat messages from everybody)
    std::memset(m_szTextBoxBuffer, 0, TEXTMAXSIZE);

    // memset(m_szUserID, 0, 16);	// Short User ID (name, pseudo...whatever) that replace
    // the local computer name/IP

    unsigned long pcSize = MAXNAMESIZE;
    if (::GetComputerName(m_szLocalName, &pcSize))
    {
        if (pcSize >= MAXNAMESIZE)
        {
            m_szLocalName[MAXNAMESIZE - 4] = '.';
            m_szLocalName[MAXNAMESIZE - 3] = '.';
            m_szLocalName[MAXNAMESIZE - 2] = '.';
            m_szLocalName[MAXNAMESIZE - 1] = 0x00;
        }
    }

    //  Load the Rich Edit control DLL
    m_hRichEdit = ::LoadLibrary("RICHED32.DLL");
    if (!m_hRichEdit)
    {
        MessageBoxSecure(nullptr, sz_ID_RICHED32_UNLOAD.data(), sz_ID_RICHED32_DLL_LD.data(), MB_OK | MB_ICONEXCLAMATION);
        // Todo: do normal edit instead (no colors)
    }
    m_Thread = INVALID_HANDLE_VALUE;
}

static bool IsValidUtf8(const char* const s, const std::size_t len)
{
    std::size_t i{ 0U };
    while (i < len)
    {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c <= 0x7F)
        {
            ++i;
            continue;
        }

        std::size_t remaining{ 0U };
        if ((c & 0xE0) == 0xC0)
        {
            remaining = 1;
        }
        else if ((c & 0xF0) == 0xE0)
        {
            remaining = 2;
        }
        else if ((c & 0xF8) == 0xF0)
        {
            remaining = 3;
        }
        else
        {
            return false;
        }

        if (i + remaining >= len)
        {
            return false;
        }
        for (std::size_t j{ 1U }; j <= remaining; ++j)
        {
            unsigned char cc = static_cast<unsigned char>(s[i + j]);
            if ((cc & 0xC0) != 0x80)
            {
                return false;
            }
        }
        i += remaining + 1;
    }
    return true;
}

static bool WideToUtf8(const wchar_t* const w, const int wlen, char* const out, const int outCap)
{
    if (!out || (outCap <= 0))
    {
        return false;
    }
    out[0U] = 0;
    if (!w)
    {
        return true;
    }
    int n = ::WideCharToMultiByte(CP_UTF8, 0, w, wlen, out, outCap - 1, nullptr, nullptr);
    if (n <= 0)
    {
        return false;
    }
    out[n] = 0;
    return true;
}

static bool WideToMultiByte(const UINT codepage, const wchar_t* const w, const int wlen, char* const out, const int outCap)
{
    if (!out || (outCap <= 0))
    {
        return false;
    }
    out[0U] = 0;
    if (!w)
    {
        return true;
    }
    int n = ::WideCharToMultiByte(codepage, 0, w, wlen, out, outCap - 1, nullptr, nullptr);
    if (n <= 0)
    {
        return false;
    }
    out[n] = 0;
    return true;
}

static bool BytesToUtf8Display(const char* const inBytes, const int inLen, const UINT assumedCodepage, char* const outUtf8,
                               const int outCap)
{
    if (!outUtf8 || (outCap <= 0))
    {
        return false;
    }
    outUtf8[0U] = 0;
    if (!inBytes || (inLen <= 0))
    {
        return true;
    }

    UINT cp = assumedCodepage;
    if ((cp == CP_UTF8) && !IsValidUtf8(inBytes, static_cast<std::size_t>(inLen)))
    {
        cp = CP_ACP;
    }

    int wlen = ::MultiByteToWideChar(cp, 0, inBytes, inLen, nullptr, 0);
    if (wlen <= 0)
    {
        return false;
    }

    wchar_t* wtmp = new wchar_t[wlen + 1];
    wtmp[0U] = 0;
    int wlen2 = ::MultiByteToWideChar(cp, 0, inBytes, inLen, wtmp, wlen);
    if (wlen2 <= 0)
    {
        delete[] wtmp;
        return false;
    }
    wtmp[wlen2] = 0;

    const bool ok = WideToUtf8(wtmp, wlen2, outUtf8, outCap);
    delete[] wtmp;
    return ok;
}

//
//
//
TextChat::~TextChat()
{
    if (m_szLocalText != nullptr)
    {
        delete[] m_szLocalText;
    }
    if (m_szLastLocalText != nullptr)
    {
        delete[] m_szLastLocalText;
    }
    if (m_szRemoteText != nullptr)
    {
        delete[] m_szRemoteText;
    }
    if (m_szRemoteName != nullptr)
    {
        delete[] m_szRemoteName;
    }
    if (m_szLocalName != nullptr)
    {
        delete[] m_szLocalName;
    }
    if (m_szTextBoxBuffer != nullptr)
    {
        delete[] m_szTextBoxBuffer;
    }

    m_fTextChatRunning = false;
    if (m_hDlg != nullptr)
    {
        ::SendMessage(m_hDlg, WM_COMMAND, IDCANCEL, 0L);
    }

    if (m_hRichEdit != nullptr)
    {
        ::FreeLibrary(m_hRichEdit);
    }
}


//
//
//
void TextChat::ShowChatWindow(const bool fVisible)
{
    ::ShowWindow(m_hDlg, fVisible ? SW_RESTORE : SW_MINIMIZE);
    ::SetForegroundWindow(m_hDlg);
    //SetFocus(GetDlgItem(m_hDlg, IDC_INPUTAREA_EDIT));
}


//
// Set text format to a selection in the Chat area
//
void TextChat::SetTextFormat(const bool bBold, const bool bItalic, const long nSize, const char* const szFaceName,
                             const DWORD dwColor)
{
    if (::GetDlgItem(m_hDlg, IDC_CHATAREA_EDIT)) //  Sanity Check
    {
        CHARFORMAT cf;
        std::memset(&cf, 0, sizeof(CHARFORMAT)); //  Initialize structure

        cf.cbSize = sizeof(CHARFORMAT);
        cf.dwMask = CFM_COLOR | CFM_FACE | CFM_SIZE;
        if (bBold)
        {
            cf.dwMask |= CFM_BOLD;
            cf.dwEffects |= CFE_BOLD;
        }
        if (bItalic)
        {
            cf.dwMask |= CFM_ITALIC;
            cf.dwEffects |= CFE_ITALIC;
        }
        cf.crTextColor = dwColor; // set color in AABBGGRR mode (alpha-RGB)
        cf.yHeight = nSize;       // set size in points
        ::strcpy_s(cf.szFaceName, 32, szFaceName);

        ::SendDlgItemMessage(m_hDlg, IDC_CHATAREA_EDIT, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
    }
}

//
//
//
void TextChat::ProcessTextChatMsg(const int nTO)
{
    rfbTextChatMsg tcm;
    m_pCC->m_socket->ReadExact(((char*)&tcm) + nTO, sz_rfbTextChatMsg - nTO);
    int len = Swap32IfLE(tcm.length);

    if (len == CHAT_OPEN)
    {
        PlayResource((char*)"IDR_WAVE1");
        if (m_fTextChatRunning)
        {
            return;
        }
        // PostMessage(m_pCC->m_server->GetDesktopPointer()->Window(), WM_USER+888, 0, (LPARAM)this);
        DisplayTextChat();
        return;
    }
    else if (len == CHAT_CLOSE)
    {
        // Close Text Chat Dialog
        if (!m_fTextChatRunning)
        {
            return;
        }
        ::PostMessage(m_hDlg, WM_COMMAND, IDOK, 0);
        return;
    }
    else if (len == CHAT_FINISHED)
    {
        // Close Text Chat Dialog
        if (!m_fTextChatRunning)
        {
            return;
        }
        // m_fTextChatRunning = false;
        // PostMessage(m_hDlg, WM_COMMAND, IDCANCEL, 0);
        return;
    }
    else
    {
        // Read the incoming text
        // Any other negative length is a protocol error: it would reach
        // new char[len] below as a huge size and crash the server.
        if (len < 0)
        {
            m_pCC->cl_connected = FALSE;
            return;
        }
        if (len > TEXTMAXSIZE)
        {
            return;
        } // Todo: Improve this...
        if (len == 0)
        {
            ::SetDlgItemText(m_hDlg, IDC_CHATAREA_EDIT, "");
            std::memset(m_szRemoteText, 0, TEXTMAXSIZE);
        }
        else
        {
            char* raw = new char[len];
            if (!m_pCC->m_socket->ReadExact(raw, len))
            {
                delete[] raw;
                m_pCC->cl_connected = FALSE;
                return;
            }

            std::memset(m_szRemoteText, 0, TEXTMAXSIZE);
            UINT assumed = m_pCC->m_supportsUnicodeTextChat ? CP_UTF8 : CP_ACP;
            if (!BytesToUtf8Display(raw, len, assumed, m_szRemoteText, TEXTMAXSIZE))
            {
                int copyLen = (len >= TEXTMAXSIZE) ? (TEXTMAXSIZE - 1) : len;
                std::memcpy(m_szRemoteText, raw, copyLen);
                m_szRemoteText[copyLen] = 0;
            }
            delete[] raw;
            ShowChatWindow(true); // Show the chat window on new message reception
            PrintMessage(m_szRemoteText, m_szRemoteName, RED);
        }
    }
}


//
// Tell the client to initiate a Text Chat
//
void TextChat::OrderTextChat()
{
    SendTextChatRequest(CHAT_OPEN);
    return;
}


//
//
//
void TextChat::SendTextChatRequest(const int nMsg)
{
    if (m_pCC->m_fFileTransferRunning)
    {
        return;
    } // Don't break a running File Transfer please...
    rfbTextChatMsg tcm;
    std::memset(&tcm, 0, sizeof(rfbTextChatMsg));
    tcm.type = rfbTextChat;
    tcm.length = Swap32IfLE(nMsg);
    omni_mutex_lock l(m_pCC->GetUpdateLock(), 82);
    m_pCC->m_socket->SendExact((char*)&tcm, sz_rfbTextChatMsg, rfbTextChat);
    return;
}


//
// Output messages in the chat area
//
//
void TextChat::PrintMessage(const char* const szMessage, const char* const szSender, const DWORD dwColor)
{
    // Note: szMessage and szSender are UTF-8 encoded strings
    CHARRANGE cr;

    // sf@2005 - Empty the RichEdit control when it's close to the TEXTMAXSIZE limit
    GETTEXTLENGTHEX lpG;
    lpG.flags = GTL_NUMBYTES;
    lpG.codepage = CP_UTF8;

    int nLen = static_cast<int>(::SendDlgItemMessage(m_hDlg, IDC_CHATAREA_EDIT, EM_GETTEXTLENGTHEX, (WPARAM)&lpG, 0L));
    if (nLen + 32 > TEXTMAXSIZE)
    {
        std::memset(m_szTextBoxBuffer, 0, TEXTMAXSIZE);
        ::strcpy_s(m_szTextBoxBuffer, TEXTMAXSIZE,
                 "-----------------------------------------------------------------------------------------------------------------"
                 "-------\n");
        ::SetDlgItemText(m_hDlg, IDC_CHATAREA_EDIT, m_szTextBoxBuffer);
    }

    // Print sender's name with Unicode support
    if ((szSender != nullptr) && (std::strlen(szSender) > 0))
    {
        ::SendDlgItemMessage(m_hDlg, IDC_CHATAREA_EDIT, WM_GETTEXT, TEXTMAXSIZE - 1, (LPARAM)m_szTextBoxBuffer);
        cr.cpMax = nLen;
        cr.cpMin = cr.cpMax;
        ::SendDlgItemMessage(m_hDlg, IDC_CHATAREA_EDIT, EM_EXSETSEL, 0, (LPARAM)&cr);

        SetTextFormat(false, false, 0x75, "MS Shell Dlg 2", dwColor);

        // Convert UTF-8 sender name to Unicode for better display
        std::array<wchar_t, MAXNAMESIZE> senderW;
        ::MultiByteToWideChar(CP_UTF8, 0, szSender, -1, senderW.data(), static_cast<int>(senderW.size()));

        std::array<wchar_t, MAXNAMESIZE + 10> bufferW;
        ::swprintf_s(bufferW.data(), bufferW.size(), L"<%s>: ", senderW.data());

        SETTEXTEX st;
        st.flags = ST_SELECTION;
        st.codepage = 1200; // Unicode (UTF-16LE)
        ::SendDlgItemMessage(m_hDlg, IDC_CHATAREA_EDIT, EM_SETTEXTEX, (WPARAM)&st, (LPARAM)bufferW.data());
    }

    nLen = static_cast<int>(::SendDlgItemMessage(m_hDlg, IDC_CHATAREA_EDIT, EM_GETTEXTLENGTHEX, (WPARAM)&lpG, 0L));

    // Print message with Unicode support
    if (szMessage != nullptr)
    {
        ::SendDlgItemMessage(m_hDlg, IDC_CHATAREA_EDIT, WM_GETTEXT, TEXTMAXSIZE - 1, (LPARAM)m_szTextBoxBuffer);
        cr.cpMax = nLen;
        cr.cpMin = cr.cpMax;
        ::SendDlgItemMessage(m_hDlg, IDC_CHATAREA_EDIT, EM_EXSETSEL, 0, (LPARAM)&cr);

        SetTextFormat(false, false, 0x75, "MS Shell Dlg 2", dwColor != GREY ? BLACK : GREY);

        // Convert UTF-8 message to Unicode for better display
        std::array<wchar_t, TEXTMAXSIZE> messageW;
        ::MultiByteToWideChar(CP_UTF8, 0, szMessage, -1, messageW.data(), static_cast<int>(messageW.size()));

        SETTEXTEX st;
        st.flags = ST_SELECTION;
        st.codepage = 1200; // Unicode (UTF-16LE)
        ::SendDlgItemMessage(m_hDlg, IDC_CHATAREA_EDIT, EM_SETTEXTEX, (WPARAM)&st, (LPARAM)messageW.data());
    }

    // Scroll down the chat window
    // The following seems necessary under W9x & Me if we want the Edit to scroll to bottom...
    SCROLLINFO si;
    ::ZeroMemory(&si, sizeof(SCROLLINFO));
    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_RANGE | SIF_PAGE;
    ::GetScrollInfo(::GetDlgItem(m_hDlg, IDC_CHATAREA_EDIT), SB_VERT, &si);
    si.nPos = si.nMax - std::max(static_cast<int>((si.nPage - 1)), 0);
    ::SendDlgItemMessage(m_hDlg, IDC_CHATAREA_EDIT, WM_VSCROLL, MAKELONG(SB_THUMBPOSITION, si.nPos), 0L); // Scroll down the ch

    // This line does the bottom scrolling correctly under Windows NT4, Windows 2000, Windows XP...
    // SendDlgItemMessage(m_hDlg, IDC_CHATAREA_EDIT, WM_VSCROLL, SB_BOTTOM, 0L);
}

//
// Send local text content
//
void TextChat::SendLocalText(void)
{
    if (m_pCC->m_fFileTransferRunning)
    {
        return;
    } // Don't break a running File Transfer please...

    // We keep it because we could use it
    // for future retype functionality. (F3)
    std::memcpy(m_szLastLocalText, m_szLocalText, std::strlen(m_szLocalText));

    // Read input as UTF-16 from the control (Windows-native)
    std::array<wchar_t, TEXTMAXSIZE> wbuf;
    std::memset(wbuf.data(), 0, sizeof(wbuf));
    ::GetDlgItemTextW(m_hDlg, IDC_INPUTAREA_EDIT, wbuf.data(), static_cast<int>(wbuf.size()) - 2);
    if (wbuf[0U] == 0)
    {
        ::SetDlgItemText(m_hDlg, IDC_INPUTAREA_EDIT, "");
        return;
    }
    // Keep original behavior of appending newline
    ::wcscat_s(wbuf.data(), wbuf.size(), L"\n");

    // Always convert to UTF-8 for local display (RichEdit insertion uses CP_UTF8)
    std::array<char, TEXTMAXSIZE> displayUtf8;
    std::memset(displayUtf8.data(), 0, TEXTMAXSIZE);
    if (!WideToUtf8(wbuf.data(), -1, displayUtf8.data(), static_cast<int>(displayUtf8.size())))
    {
        displayUtf8[0U] = 0;
    }
    if (displayUtf8[0U] != 0)
    {
        ::strcpy_s(m_szLocalText, TEXTMAXSIZE, displayUtf8.data());
    }

    PrintMessage(m_szLocalText, m_szLocalName, BLUE);

    // Backward compatible send:
    // - If the viewer negotiated Unicode chat, send UTF-8 bytes.
    // - Otherwise, send legacy ACP bytes.
    std::array<char, TEXTMAXSIZE> sendBytes;
    std::memset(sendBytes.data(), 0, TEXTMAXSIZE);
    UINT sendCp = m_pCC->m_supportsUnicodeTextChat ? CP_UTF8 : CP_ACP;
    if (!WideToMultiByte(sendCp, wbuf.data(), -1, sendBytes.data(), static_cast<int>(sendBytes.size())))
    {
        // final fallback: if ACP conversion fails, try UTF-8
        if (!WideToUtf8(wbuf.data(), -1, sendBytes.data(), static_cast<int>(sendBytes.size())))
        {
            sendBytes[0U] = 0;
        }
    }

    rfbTextChatMsg tcm;
    std::memset(&tcm, 0, sizeof(rfbTextChatMsg));
    tcm.type = rfbTextChat;
    int sendLen = static_cast<int>(std::strlen(sendBytes.data()));
    tcm.length = Swap32IfLE(sendLen);
    //adzm 2010-09 - minimize packets. SendExact flushes the queue.
    omni_mutex_lock l(m_pCC->GetUpdateLock(), 82);
    m_pCC->m_socket->SendExactQueue((char*)&tcm, sz_rfbTextChatMsg, rfbTextChat);
    if (sendLen > 0)
    {
        m_pCC->m_socket->SendExact((char*)sendBytes.data(), (const VCard)sendLen);
    }

    //and we clear the input box
    ::SetDlgItemText(m_hDlg, IDC_INPUTAREA_EDIT, "");
    return;
}



LRESULT CALLBACK TextChat::DoDialogThread(const LPVOID lpParameter)
{
    TextChat* _this = (TextChat*)lpParameter;

    _this->m_fTextChatRunning = true;
    // TODO: Place code here.
    HDESK desktop;
    desktop = ::OpenInputDesktop(0, FALSE,
                               DESKTOP_CREATEMENU | DESKTOP_CREATEWINDOW | DESKTOP_ENUMERATE | DESKTOP_HOOKCONTROL |
                                   DESKTOP_WRITEOBJECTS | DESKTOP_READOBJECTS | DESKTOP_SWITCHDESKTOP | GENERIC_WRITE);

    if (desktop == nullptr)
    {
        vnclog.Print(LL_INTERR, VNCLOG("OpenInputdesktop Error \n"));
    }

    HDESK old_desktop = ::GetThreadDesktop(::GetCurrentThreadId());
    DWORD dummy;

    std::array<char, 256U> new_name;
    if (desktop)
    {
        if (!::GetUserObjectInformation(desktop, UOI_NAME, &new_name, 256, &dummy))
        {
            vnclog.Print(LL_INTERR, VNCLOG("!GetUserObjectInformation \n"));
        }

        vnclog.Print(LL_INTERR, VNCLOG("SelectHDESK to %s (%x) from %x\n"), new_name.data(), desktop, old_desktop);

        if (!::SetThreadDesktop(desktop))
        {
            vnclog.Print(LL_INTERR, VNCLOG("SelectHDESK:!SetThreadDesktop 3\n"));
        }
    }

    ::DialogBoxParam(hInstResDLL, MAKEINTRESOURCE(IDD_TEXTCHAT_DLG), nullptr, (DLGPROC)TextChatDlgProc, (LPARAM)_this);
    ::SetThreadDesktop(old_desktop);
    if (desktop)
    {
        ::CloseDesktop(desktop);
    }
    return 0;
}


HWND TextChat::DisplayTextChat()
{
    DWORD threadID;
    m_Thread = ::CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)(TextChat::DoDialogThread), (LPVOID)this, 0, &threadID);
    if (m_Thread)
    {
        ::ResumeThread(m_Thread);
    }
    return (HWND)0;
}

//
//
//
void TextChat::KillDialog()
{
    m_fTextChatRunning = false;
    if (m_Thread != INVALID_HANDLE_VALUE)
    {
        ::SendMessage(m_hDlg, WM_COMMAND, IDCANCEL, 0);
        ::WaitForSingleObject(m_Thread, INFINITE); // wait for thread to exit
        m_hDlg = nullptr;
    }
}


//
//
//
void TextChat::RefuseTextChat()
{
    SendTextChatRequest(CHAT_CLOSE);
    SendTextChatRequest(CHAT_FINISHED);
}


//	[v1.0.2-jp1 fix-->]
void AdjustLeft(const LPRECT lprc)
{
    int cx = lprc->right - lprc->left - GetSystemMetrics(SM_CXSIZEFRAME) * 2;

    if (cx < 240)
    {
        lprc->left = lprc->right - 240 - GetSystemMetrics(SM_CXSIZEFRAME) * 2;
    }
}

void AdjustTop(const LPRECT lprc)
{
    int cy = lprc->bottom - lprc->top - GetSystemMetrics(SM_CYSIZEFRAME) * 2;

    if (cy < 179)
    {
        lprc->top = lprc->bottom - 179 - GetSystemMetrics(SM_CYSIZEFRAME) * 2;
    }
}

void AdjustRight(const LPRECT lprc)
{
    int cx = lprc->right - lprc->left - GetSystemMetrics(SM_CXSIZEFRAME) * 2;

    if (cx < 240)
    {
        lprc->right = lprc->left + 240 + GetSystemMetrics(SM_CXSIZEFRAME) * 2;
    }
}

void AdjustBottom(const LPRECT lprc)
{
    int cy = lprc->bottom - lprc->top - GetSystemMetrics(SM_CYSIZEFRAME) * 2;

    if (cy < 179)
    {
        lprc->bottom = lprc->top + 179 + GetSystemMetrics(SM_CYSIZEFRAME) * 2;
    }
}
//	[<--v1.0.2-jp1 fix]


//
//
//
INT_PTR CALLBACK TextChat::TextChatDlgProc(HWND hWnd, const UINT uMsg, const WPARAM wParam, const LPARAM lParam)
{

    TextChat* _this = helper::SafeGetWindowUserData<TextChat>(hWnd);
#ifdef _DEBUG
    std::array<char, 256U> szText;
    ::sprintf_s(szText.data(), szText.size(), " ++++++ uMsg %i\n", uMsg);
    ::OutputDebugString(szText.data());
#endif
    switch (uMsg)
    {
    case WM_INITDIALOG: {
        HICON hIcon = ::LoadIcon(::GetModuleHandle(nullptr), MAKEINTRESOURCE(IDI_WINVNC));
        ::SendMessage(hWnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
        ::SendMessage(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
        helper::SafeSetWindowUserData(hWnd, lParam);
        TextChat* _thisInner = (TextChat*)lParam;

        if ((_thisInner->m_szLocalText == nullptr) || (_thisInner->m_szRemoteText == nullptr))
        {
            ::EndDialog(hWnd, FALSE);
        }

        _thisInner->m_hWnd = hWnd;
        _thisInner->m_hDlg = hWnd;

        if (::_snprintf_s(_thisInner->m_szRemoteName, MAXNAMESIZE, MAXNAMESIZE - 1, "%s", _thisInner->m_pCC->GetClientNameName()) < 0)
        {
            _thisInner->m_szRemoteName[MAXNAMESIZE - 4] = '.';
            _thisInner->m_szRemoteName[MAXNAMESIZE - 3] = '.';
            _thisInner->m_szRemoteName[MAXNAMESIZE - 2] = '.';
            _thisInner->m_szRemoteName[MAXNAMESIZE - 1] = 0x00;
        }

        const long lTitleBufSize{ 256 };
        std::array<char, lTitleBufSize> szTitle;

        ::_snprintf_s(szTitle.data(), szTitle.size(), _TRUNCATE, sz_ID_CHAT_WITH_S_ULTRAVNC.data(), _thisInner->m_szRemoteName);
        ::SetWindowText(hWnd, szTitle.data());

        std::memset(_thisInner->m_szLocalText, 0, TEXTMAXSIZE);

        // Local message box
        ::SetDlgItemText(hWnd, IDC_INPUTAREA_EDIT, _thisInner->m_szLocalText);

        //  Chat area
        _thisInner->SetTextFormat(); //  Set character formatting and background color
        ::SendDlgItemMessage(hWnd, IDC_CHATAREA_EDIT, EM_SETBKGNDCOLOR, FALSE, 0xFFFFFF);

        std::memset(_thisInner->m_szLastLocalText, 0, TEXTMAXSIZE); // Future retype functionnality
        std::memset(_thisInner->m_szTextBoxBuffer, 0, TEXTMAXSIZE); // Clear Chat area
                                                               //  Load and display diclaimer message
                                                               // sf@2005 - Only if the DSMplugin is used
        if (!_thisInner->m_pCC->m_server->GetDSMPluginPointer()->IsEnabled())
        {
            //	[v1.0.2-jp1 fix]
            if (::LoadString(hInstResDLL, IDS_WARNING, _thisInner->m_szRemoteText, TEXTMAXSIZE - 1))
            {
                _thisInner->PrintMessage(_thisInner->m_szRemoteText, nullptr, GREY);
            }
        }



        // Scroll down the chat window
        // The following seems necessary under W9x & Me if we want the Edit to scroll to bottom...


        SCROLLINFO si;
        ::ZeroMemory(&si, sizeof(SCROLLINFO));
        si.cbSize = sizeof(SCROLLINFO);
        si.fMask = SIF_RANGE | SIF_PAGE;
        ::GetScrollInfo(::GetDlgItem(hWnd, IDC_CHATAREA_EDIT), SB_VERT, &si);
        si.nPos = si.nMax - std::max(static_cast<int>((si.nPage - 1)), 0);
        ::SendDlgItemMessage(hWnd, IDC_CHATAREA_EDIT, WM_VSCROLL, MAKELONG(SB_THUMBPOSITION, si.nPos), 0L);

        ::SetForegroundWindow(hWnd);

        //	[v1.0.2-jp1 fix] SUBCLASS Split bar
        pDefSBProc = helper::SafeGetWindowProc(::GetDlgItem(hWnd, IDC_STATIC_SPLIT));
        // CRAsh X64 fixed
        helper::SafeSetWindowProc(::GetDlgItem(hWnd, IDC_STATIC_SPLIT), pDefSBProc);

        return TRUE;
    }
    break;

    case WM_COMMAND:
#ifdef _DEBUG
        //char			szText[256];
        ::sprintf_s(szText.data(), szText.size(), " ++++++ wm_command %i\n", LOWORD(wParam));
        ::OutputDebugString(szText.data());
#endif
        switch (LOWORD(wParam))
        {
            /*
            case IDC_PERSISTENT_CHECK:
                  _this->m_fPersistentTexts = (SendDlgItemMessage(hWnd, IDC_PERSISTENT_CHECK, BM_GETCHECK, 0, 0) == BST_CHECKED);
                return TRUE;
            */

        case IDOK:
            // Client order to close Text Chat

            //	[v1.0.2-jp1 fix] UNSUBCLASS Split bar
            helper::SafeSetWindowProc(::GetDlgItem(hWnd, IDC_STATIC_SPLIT), pDefSBProc);
            ::EndDialog(hWnd, FALSE);
            _this->m_fTextChatRunning = false;
            _this->SendTextChatRequest(CHAT_FINISHED);
            return TRUE;

        case IDCANCEL:
            _this->SendTextChatRequest(CHAT_CLOSE); // Client must close Text Chat

            //	[v1.0.2-jp1 fix] UNSUBCLASS Split bar
            helper::SafeSetWindowProc(::GetDlgItem(hWnd, IDC_STATIC_SPLIT), pDefSBProc);
            ::EndDialog(hWnd, FALSE);
            _this->m_fTextChatRunning = false;
            _this->SendTextChatRequest(CHAT_FINISHED);
            return TRUE;

        case IDC_SEND_B: {
            std::memset(_this->m_szLocalText, 0, TEXTMAXSIZE);
            ::GetDlgItemText(hWnd, IDC_LOCALTEXT_EDIT, _this->m_szLocalText, TEXTMAXSIZE - 1);
            ::strcat_s(_this->m_szLocalText, TEXTMAXSIZE, "\n");
            _this->SendLocalText();
            ::SetFocus(::GetDlgItem(hWnd, IDC_INPUTAREA_EDIT));
        }
            return TRUE;

        case IDC_INPUTAREA_EDIT:
            if (HIWORD(wParam) == EN_UPDATE)
            {
                ::GetDlgItemText(hWnd, IDC_INPUTAREA_EDIT, _this->m_szLocalText, TEXTMAXSIZE);
                if (strstr(_this->m_szLocalText, "\n") != 0) // Enter triggers the message transmission
                {
                    _this->SendLocalText();
                }
            }
            return TRUE;

        case IDC_HIDE_B:
            _this->ShowChatWindow(false);
            return TRUE;

        default:
            return TRUE;
            break;
        }
        break;

    case WM_SYSCOMMAND:
        switch (LOWORD(wParam))
        {
        case SC_RESTORE:
            _this->ShowChatWindow(true);
            //SetFocus(GetDlgItem(hWnd, IDC_INPUTAREA_EDIT));
            return TRUE;
        }
        break;

    //	[v1.0.2-jp1 fix-->]
    case WM_SIZING:
        LPRECT lprc;
        lprc = (LPRECT)lParam;
        switch (wParam)
        {
        case WMSZ_TOPLEFT:
            AdjustTop(lprc);
            AdjustLeft(lprc);
        case WMSZ_TOP:
            AdjustTop(lprc);
        case WMSZ_TOPRIGHT:
            AdjustTop(lprc);
            AdjustRight(lprc);
        case WMSZ_LEFT:
            AdjustLeft(lprc);
        case WMSZ_RIGHT:
            AdjustRight(lprc);
        case WMSZ_BOTTOMLEFT:
            AdjustBottom(lprc);
            AdjustLeft(lprc);
        case WMSZ_BOTTOM:
            AdjustBottom(lprc);
        case WMSZ_BOTTOMRIGHT:
            AdjustBottom(lprc);
            AdjustRight(lprc);
        }
        return TRUE;

    case WM_SIZE:
        int cx;
        int cy;
        int icy;
        RECT rc;

        if (wParam == SIZE_MINIMIZED)
        {
            break;
        }

        cx = LOWORD(lParam);
        cy = HIWORD(lParam);
        ::GetWindowRect(::GetDlgItem(hWnd, IDC_INPUTAREA_EDIT), &rc);
        icy = rc.bottom - rc.top;
        if (cy - icy - 12 < 80)
        {
            icy = cy - 92;
        }
        ::MoveWindow(::GetDlgItem(hWnd, IDC_CHATAREA_EDIT), 4, 4, cx - 8, cy - icy - 16, TRUE);
        ::MoveWindow(::GetDlgItem(hWnd, IDC_STATIC_SPLIT), 4, cy - icy - 12, cx - 8, 8, TRUE);
        ::MoveWindow(::GetDlgItem(hWnd, IDC_INPUTAREA_EDIT), 4, cy - icy - 4, cx - 88, icy, TRUE);

        ::MoveWindow(::GetDlgItem(hWnd, IDC_SEND_B), cx - 76, cy - 64, 72, 20, TRUE);
        ::MoveWindow(::GetDlgItem(hWnd, IDC_HIDE_B), cx - 76, cy - 40, 72, 18, TRUE);
        ::MoveWindow(::GetDlgItem(hWnd, IDCANCEL), cx - 76, cy - 22, 72, 18, TRUE);

        ::InvalidateRect(hWnd, nullptr, FALSE);
        return TRUE;
        //	[<--v1.0.2-jp1 fix]

    case WM_DESTROY:
        // _this->m_fTextChatRunning = false;
        // _this->SendTextChatRequest(CHAT_FINISHED);
        ::EndDialog(hWnd, FALSE);
        return TRUE;
    }
    return 0;
}

//	[v1.0.2-jp1 fix-->] Split bar
void DrawResizeLine(const HWND hWnd, int y)
{
    HWND hParent;
    RECT rc;
    HDC hDC;

    hParent = ::GetParent(hWnd);
    ::GetClientRect(hParent, &rc);

    if (y < 80)
    {
        y = 80;
    }
    else if (y > rc.bottom - 80)
    {
        y = rc.bottom - 80;
    }

    hDC = ::GetDC(hParent);
    ::SetROP2(hDC, R2_NOTXORPEN);
    ::MoveToEx(hDC, rc.left, y, nullptr);
    ::LineTo(hDC, rc.right, y);
    ::MoveToEx(hDC, rc.left, y + 1, nullptr);
    ::LineTo(hDC, rc.right, y + 1);
    ::MoveToEx(hDC, rc.left, y + 2, nullptr);
    ::LineTo(hDC, rc.right, y + 2);
    ::MoveToEx(hDC, rc.left, y + 3, nullptr);
    ::LineTo(hDC, rc.right, y + 3);
    ::ReleaseDC(hParent, hDC);
}
//	[<--v1.0.2-jp1 fix]

//	[v1.0.2-jp1 fix-->] Split bar proc
LRESULT CALLBACK SBProc(const HWND hWnd, const UINT uMsg, const WPARAM wParam, const LPARAM lParam)
{
    static BOOL bCapture;
    static UINT u;
    static int oldy;
    HWND hParent;
    RECT rc;
    POINT cp;
    int y;
    int cx;
    int cy;

    switch (uMsg)
    {
    case WM_SETCURSOR:
        SetCursor(::LoadCursor(nullptr, IDC_SIZENS));
        return TRUE;
    case WM_LBUTTONDOWN:
        ::SetCapture(hWnd);
        bCapture = TRUE;
        u = ::GetCaretBlinkTime();
        ::SetCaretBlinkTime(0x7fffffff);
        ::GetCursorPos(&cp);
        hParent = ::GetParent(hWnd);
        ::ScreenToClient(hParent, &cp);
        DrawResizeLine(hWnd, cp.y);
        oldy = cp.y;
        break;
    case WM_MOUSEMOVE:
        if (bCapture)
        {
            ::GetCursorPos(&cp);
            hParent = ::GetParent(hWnd);
            ::ScreenToClient(hParent, &cp);
            DrawResizeLine(hWnd, oldy);
            DrawResizeLine(hWnd, cp.y);
            oldy = cp.y;
        }
        break;
    case WM_LBUTTONUP:
        ReleaseCapture();
        bCapture = FALSE;
        ::SetCaretBlinkTime(u);
        ::GetCursorPos(&cp);
        hParent = ::GetParent(hWnd);
        ::GetClientRect(hParent, &rc);
        cx = rc.right - rc.left;
        cy = rc.bottom - rc.top;
        ::ScreenToClient(hParent, &cp);
        DrawResizeLine(hWnd, cp.y);
        y = cp.y;
        if (y < 80)
        {
            y = 80;
        }
        else if (y > cy - 80)
        {
            y = cy - 80;
        }
        ::MoveWindow(::GetDlgItem(hParent, IDC_CHATAREA_EDIT), 4, 4, cx - 8, y - 4, TRUE);
        ::MoveWindow(::GetDlgItem(hParent, IDC_STATIC_SPLIT), 4, y, cx - 8, 8, TRUE);
        ::MoveWindow(::GetDlgItem(hParent, IDC_INPUTAREA_EDIT), 4, y + 8, cx - 88, cy - y - 12, TRUE);
        break;
    }

    return ::CallWindowProc((WNDPROC)pDefSBProc, hWnd, uMsg, wParam, lParam);
}
//	[<--v1.0.2-jp1 fix]
