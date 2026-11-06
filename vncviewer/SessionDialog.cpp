// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
//


// SessionDialog.cpp: implementation of the SessionDialog class.

#include "stdhdrs.h"

#include "SessionDialog.h"

#include "AboutBox.h"
#include "common/win32_helpers.h"
#include "display.h"
#include "Exception.h"
#include "UltraVNCHelperFunctions.h"
#include "vncviewer.h"

#include <direct.h>
#include <ShlObj.h>
#include <sys/stat.h>

#include <array>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <cwchar>

using namespace helper;
extern HINSTANCE m_hInstResDLL;

#define SESSION_MRU_KEY_NAME _T("Software\\ORL\\VNCviewer\\MRU")
#define NUM_MRU_ENTRIES 8

extern std::array<wchar_t, 64U> sz_F1;
extern std::array<wchar_t, 64U> sz_F2;
extern std::array<wchar_t, 64U> sz_F3;
extern std::array<wchar_t, 64U> sz_F4;
extern std::array<wchar_t, 128U> sz_F5;
extern std::array<wchar_t, 64U> sz_F6;
extern std::array<wchar_t, 128U> sz_F7;
extern std::array<wchar_t, 128U> sz_F8;
extern std::array<wchar_t, 64U> sz_F9;
extern std::array<wchar_t, 64U> sz_F10;
extern std::array<wchar_t, 64U> sz_F11;

SessionDialog::SessionDialog(VNCOptions* pOpt, ClientConnection* pCC, CDSMPlugin* pDSMPlugin)
{
    m_bExpanded = true;
    m_nExpandedWidth = 0;
    m_nExpandedHeight = 0;
    m_Dpi = pCC->m_Dpi;
    m_pCC = pCC;
    m_pOpt = pOpt;
    m_pMRU = new MRU(SESSION_MRU_KEY_NAME, 98);
    m_pDSMPlugin = pDSMPlugin;
    /////////////////////////////////////////////////
    std::array<TCHAR, 256U> tmphost2;
    ::_tcscpy_s(m_proxyhost.data(), m_proxyhost.size(), m_pOpt->m_proxyhost.data());
    if (::_tcscmp(m_proxyhost.data(), _T("")) != 0)
    {
        ::_tcscat_s(m_proxyhost.data(), m_proxyhost.size(), _T(":"));
        _itot_s(m_pOpt->m_proxyport, tmphost2.data(), tmphost2.size(), 10);
        ::_tcscat_s(m_proxyhost.data(), m_proxyhost.size(), tmphost2.data());
    }

    for (int i = rfbEncodingRaw; i <= LASTENCODING; ++i)
    {
        UseEnc[static_cast<unsigned long long>(i)] = m_pOpt->m_UseEnc[static_cast<unsigned long long>(i)];
    }

    PreferredEncodings.clear();
    for (std::size_t i{ 0U }; i < m_pOpt->m_PreferredEncodings.size(); ++i)
    {
        PreferredEncodings.push_back(m_pOpt->m_PreferredEncodings[i]);
    }

    ViewOnly = m_pOpt->m_ViewOnly;
    fAutoScaling = m_pOpt->m_fAutoScaling;
    fAutoScalingEven = m_pOpt->m_fAutoScalingEven;
    fAutoScalingLimit = m_pOpt->m_fAutoScalingLimit;
    fExitCheck = m_pOpt->m_fExitCheck;
    m_connectionType = m_pOpt->m_connectionType;
    allowMonitorSpanning = m_pOpt->m_allowMonitorSpanning;
    changeServerRes = m_pOpt->m_ChangeServerRes;
    extendDisplay = m_pOpt->m_extendDisplay;
    showExtend = m_pOpt->m_showExtend;
    use_virt = m_pOpt->m_use_virt;
    useAllMonitors = m_pOpt->m_useAllMonitors;
    requestedWidth = m_pOpt->m_requestedWidth;
    requestedHeight = m_pOpt->m_requestedHeight;
    autoDetect = m_pOpt->autoDetect;
    SwapMouse = m_pOpt->m_SwapMouse;
    DisableClipboard = m_pOpt->m_DisableClipboard;
    Use8Bit = m_pOpt->m_Use8Bit;
    Shared = m_pOpt->m_Shared;
    running = m_pOpt->m_running;
    ShowToolbar = m_pOpt->m_ShowToolbar;
    scale_num = m_pOpt->m_scale_num;
    scale_den = m_pOpt->m_scale_den;
    nServerScale = m_pOpt->m_nServerScale;
    reconnectcounter = m_pOpt->m_reconnectcounter;
    autoReconnect = m_pOpt->m_autoReconnect;
    FTTimeout = m_pOpt->m_FTTimeout;
    listenport = m_pOpt->m_listenPort;
    fEnableCache = m_pOpt->m_fEnableCache;
    fEnableZstd = m_pOpt->m_fEnableZstd;
    useCompressLevel = m_pOpt->m_useCompressLevel;
    enableJpegCompression = m_pOpt->m_enableJpegCompression;
    compressLevel = m_pOpt->m_compressLevel;
    jpegQualityLevel = m_pOpt->m_jpegQualityLevel;
    throttleMouse = m_pOpt->m_throttleMouse;
    requestShapeUpdates = m_pOpt->m_requestShapeUpdates;
    ignoreShapeUpdates = m_pOpt->m_ignoreShapeUpdates;
    BlockSameMouse = m_pOpt->m_BlockSameMouse;
    Emul3Buttons = m_pOpt->m_Emul3Buttons;
    JapKeyboard = m_pOpt->m_JapKeyboard;
    quickoption = m_pOpt->m_quickoption;
    preemptiveUpdates = m_pOpt->m_preemptiveUpdates;
    FullScreen = m_pOpt->m_FullScreen;
    Directx = m_pOpt->m_Directx;
    SavePos = m_pOpt->m_SavePos;
    SaveSize = m_pOpt->m_SaveSize;
    GNOME = m_pOpt->m_GNOME;
    fUseDSMPlugin = m_pOpt->m_fUseDSMPlugin;
    ::_tcscpy_s(szDSMPluginFilename.data(), szDSMPluginFilename.size(), m_pOpt->m_szDSMPluginFilename.data());
    listening = m_pOpt->m_listening;
    oldplugin = m_pOpt->m_oldplugin;

    ::_tcscpy_s(folder.data(), folder.size(), m_pOpt->m_document_folder.data());
    ::_tcscpy_s(prefix, _countof(prefix), m_pOpt->m_prefix);
    ::_tcscpy_s(imageFormat.data(), imageFormat.size(), m_pOpt->m_imageFormat.data());

    scaling = m_pOpt->m_scaling;

    keepAliveInterval = m_pOpt->m_keepAliveInterval;
#ifdef _Gii
    giiEnable = m_pOpt->m_giiEnable;
#endif
    fAutoAcceptIncoming = m_pOpt->m_fAutoAcceptIncoming;
    fAutoAcceptNoDSM = m_pOpt->m_fAutoAcceptNoDSM;
    fRequireEncryption = m_pOpt->m_fRequireEncryption;
    fUseTokenTLS = m_pOpt->m_fUseTokenTLS;
    fUseOnlyDefaultConfigFile = m_pOpt->m_UseOnlyDefaultConfigFile;
    restricted = m_pOpt->m_restricted;
    ipv6 = m_pOpt->m_ipv6;
    AllowUntrustedServers = m_pOpt->m_AllowUntrustedServers;
    NoStatus = m_pOpt->m_NoStatus;
    HideEndOfStreamError = m_pOpt->m_HideEndOfStreamError;
    NoHotKeys = m_pOpt->m_NoHotKeys;
    setdefaults = false;
    /////////////////////////////////////////////////
    extern HINSTANCE hInstance;
    hBmpExpand = reinterpret_cast<HBITMAP>(::LoadImage(hInstance, MAKEINTRESOURCE(IDB_EXPAND), IMAGE_BITMAP, 0, 0, LR_LOADTRANSPARENT));
    hBmpCollaps = reinterpret_cast<HBITMAP>(::LoadImage(hInstance, MAKEINTRESOURCE(IDB_COLLAPS), IMAGE_BITMAP, 0, 0, LR_LOADTRANSPARENT));
    hTabEncoders = nullptr;
    hTabKeyboardMouse = nullptr;
    hTabDisplay = nullptr;
    hTabMisc = nullptr;
    hTabSecurity = nullptr;
    hTabQuickOptions = nullptr;
    hTabListen = nullptr;
    m_hTab = nullptr;
    browser_hwnd = nullptr;
}

SessionDialog::~SessionDialog()
{
    if (m_hTab != nullptr)
    {
        return;
    }
    if (hTabEncoders)
    {
        ::DestroyWindow(hTabEncoders);
    }
    if (hTabKeyboardMouse)
    {
        ::DestroyWindow(hTabKeyboardMouse);
    }
    if (hTabDisplay)
    {
        ::DestroyWindow(hTabDisplay);
    }
    if (hTabMisc)
    {
        ::DestroyWindow(hTabMisc);
    }
    if (hTabSecurity)
    {
        ::DestroyWindow(hTabSecurity);
    }
    if (hTabQuickOptions)
    {
        ::DestroyWindow(hTabQuickOptions);
    }
    if (hTabListen)
    {
        ::DestroyWindow(hTabListen);
    }
    delete m_pMRU;
}

BOOL CALLBACK SessDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
// It's exceedingly unlikely, but possible, that if two modal dialogs were
// closed at the same time, the static variables used for transfer between
// window procedure and this method could overwrite each other.
int SessionDialog::DoDialog()
{
    // Use m_hInstResDLL for dialog resources (contains translated dialogs)
    return static_cast<int>(::DialogBoxParam(m_hInstResDLL, DIALOG_MAKEINTRESOURCE(IDD_SESSION_DLG), nullptr, reinterpret_cast<DLGPROC>(SessDlgProc), reinterpret_cast<LONG_PTR>(this)));
}

BOOL CALLBACK SessDlgProc(const HWND hwnd, const UINT uMsg, const WPARAM wParam, const LPARAM lParam)
{
    SessionDialog* _this;
    if (uMsg == WM_INITDIALOG)
    {
        HICON hIcon = ::LoadIcon(::GetModuleHandle(nullptr), MAKEINTRESOURCE(IDR_TRAY));
        ::SendMessage(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(hIcon));
        ::SendMessage(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(hIcon));
        _this = reinterpret_cast<SessionDialog*>(lParam);
        helper::SafeSetWindowUserData(hwnd, lParam);
        std::array<char, 50U> cver{};
        std::array<wchar_t, 256U> title{};
        ::wcscpy_s(title.data(), title.size(), L"UltraVNC Viewer -");
        std::array<wchar_t, 50U> wver{};
        ::MultiByteToWideChar(CP_ACP, 0, GetVersionFromResource(cver.data()), -1, wver.data(), static_cast<int>(wver.size()));
        ::wcscat_s(title.data(), title.size(), wver.data());
        ::SetWindowText(hwnd, title.data());
    }
    else
    {
        _this = reinterpret_cast<SessionDialog*>(helper::SafeGetWindowUserData<SessionDialog>(hwnd));
    }
    switch (uMsg)
    {
    case WM_INITDIALOG: {
        HICON hIcon = ::LoadIcon(::GetModuleHandle(nullptr), MAKEINTRESOURCE(IDR_TRAY));
        ::SendMessage(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(hIcon));
        ::SendMessage(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(hIcon));
        helper::SafeSetWindowUserData(hwnd, lParam);
        SessionDialog* l_this = reinterpret_cast<SessionDialog*>(lParam);
        ::SetForegroundWindow(hwnd);
        l_this->m_fFromOptions = false;
        l_this->m_fFromFile = false;
        l_this->m_pCC->m_hSessionDialog = hwnd;
        l_this->SessHwnd = hwnd;
        _this->InitDlgProc();
        _this->InitLanguage(hwnd);
        HWND hExitCheck = ::GetDlgItem(hwnd, IDC_EXIT_CHECK);          //PGM @ Advantig
        ::SendMessage(hExitCheck, BM_SETCHECK, l_this->fExitCheck, 0); //PGM @ Advantig

        _this->ExpandBox(hwnd, !_this->m_bExpanded);
        ::SendMessage(::GetDlgItem(hwnd, IDC_BUTTON_EXPAND), BM_SETIMAGE, static_cast<WPARAM>(IMAGE_BITMAP), reinterpret_cast<LPARAM>(_this->hBmpExpand));
        //SetWindowText(GetDlgItem(hwnd, IDC_BUTTON_EXPAND), "Show Options");
        return TRUE;
    }

    case WM_NOTIFY:
        return _this->HandleNotify(hwnd, wParam, lParam);

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_ABOUT:
            ShowAboutBox();
            break;
        case IDC_RADIOREPEATER:
        case IDC_RADIODIRECT:
            _this->ModeSwitch(hwnd, wParam);
            ::SetTimer(hwnd, 100, 3000, nullptr);
            break;
        case IDC_HOSTNAME_EDIT:
            if (HIWORD(wParam) == CBN_SELCHANGE)
            {
                std::array<TCHAR, 256U> hostname;
                int ItemIndex = static_cast<int>(::SendMessage(reinterpret_cast<HWND>(lParam), CB_GETCURSEL, 0, 0));
                ::SendMessage(reinterpret_cast<HWND>(lParam), static_cast<UINT>(CB_GETLBTEXT), static_cast<WPARAM>(ItemIndex), reinterpret_cast<LPARAM>(hostname.data()));
                _this->m_pMRU->RemoveItem(hostname.data());
                _this->m_pMRU->AddItem(hostname.data());
                _this->IfHostExistLoadSettings(hostname.data());
                _this->SettingsToUI(false);
            }
            if (HIWORD(wParam) == CBN_SELENDOK)
            {
                std::array<TCHAR, 256U> hostname;
                int ItemIndex = static_cast<int>(::SendMessage(reinterpret_cast<HWND>(lParam), CB_GETCURSEL, 0, 0));
                ::SendMessage(reinterpret_cast<HWND>(lParam), static_cast<UINT>(CB_SETCURSEL), static_cast<WPARAM>(ItemIndex), reinterpret_cast<LPARAM>(hostname.data()));
            }
            ::SetTimer(hwnd, 100, 3000, nullptr);
            break;
        case IDC_SAVEASDEFAULT:
            _this->SaveToFile(_this->m_pOpt->getDefaultOptionsFileName(), true);
            return true;
        case IDC_RESET_DEFAULTS:
            ::DeleteFile(_this->m_pOpt->getDefaultOptionsFileName());
            _this->SetDefaults();
            return TRUE;
        case IDC_SAVE:
            _this->ReadDlgProcConfig();
            if (_this->fUseOnlyDefaultConfigFile)
            {
                _this->SaveToFile(_this->m_pOpt->getDefaultOptionsFileName(), true);
            }
            else
            {
                _this->SaveConnection(hwnd, false);
                _this->saveInt(L"UseOnlyDefaultConfigFile", _this->fUseOnlyDefaultConfigFile,
                               _this->m_pOpt->getDefaultOptionsFileName());
            }
            break;
        case IDC_SAVEAS:
            _this->SaveConnection(hwnd, true);
            break;
        case IDCONNECT:
            _this->InitTab(hwnd);
            return _this->connect(hwnd);
        case IDCANCEL:
            ::EndDialog(hwnd, FALSE);
            return TRUE;
        case IDC_SHOWOPTIONS:
        case IDC_BUTTON_EXPAND:
            _this->ExpandBox(hwnd, !_this->m_bExpanded);
            if (_this->m_bExpanded)
            {
                _this->InitTab(hwnd);
            }
            return TRUE;
        case IDC_OPTIONBUTTON: {
            if (!_this->m_fFromFile)
            {
                HWND hExitCheck = ::GetDlgItem(hwnd, IDC_EXIT_CHECK);                              //PGM @ Advantig
                _this->fExitCheck = (::SendMessage(hExitCheck, BM_GETCHECK, 0, 0) == BST_CHECKED); //PGM @ Advantig
            }

            //if (_this->m_pOpt->DoDialog())
            {
                _this->m_fFromOptions = true;
                HWND hDyn = ::GetDlgItem(hwnd, IDC_DYNAMIC);
                ::SendMessage(hDyn, BM_SETCHECK, false, 0);
                HWND hLan = ::GetDlgItem(hwnd, IDC_LAN_RB);
                ::SendMessage(hLan, BM_SETCHECK, false, 0);
                HWND hUltraLan = ::GetDlgItem(hwnd, IDC_ULTRA_LAN_RB);
                ::SendMessage(hUltraLan, BM_SETCHECK, false, 0);
                HWND hMedium = ::GetDlgItem(hwnd, IDC_MEDIUM_RB);
                ::SendMessage(hMedium, BM_SETCHECK, false, 0);
                HWND hModem = ::GetDlgItem(hwnd, IDC_MODEM_RB);
                ::SendMessage(hModem, BM_SETCHECK, false, 0);
                HWND hSlow = ::GetDlgItem(hwnd, IDC_SLOW_RB);
                ::SendMessage(hSlow, BM_SETCHECK, false, 0);
                HWND hManual = ::GetDlgItem(hwnd, IDC_MANUAL);
                ::SendMessage(hManual, BM_SETCHECK, true, 0);
                _this->quickoption = 8;
            }
            return TRUE;
        }
        case IDC_LOADPROFILE_B: {
            std::array<TCHAR, MAX_PATH> szFileName;
            std::memset(szFileName.data(), '\0', MAX_PATH);
            if (_this->m_pCC->LoadConnection(szFileName.data(), true) != -1)
            {
                std::array<TCHAR, 250U> szHost;
                if (_this->m_pCC->m_port == 5900)
                {
                    ::_tcscpy_s(szHost.data(), szHost.size(), _this->m_pCC->m_host.data());
                }
                else if ((_this->m_pCC->m_port > 5900) && (_this->m_pCC->m_port <= 5999))
                {
                    ::_sntprintf_s(szHost.data(), szHost.size(), _TRUNCATE, _T("%s:%d"), _this->m_pCC->m_host.data(),
                                 _this->m_pCC->m_port - 5900);
                }
                else
                {
                    ::_sntprintf_s(szHost.data(), szHost.size(), _TRUNCATE, _T("%s::%d"), _this->m_pCC->m_host.data(), _this->m_pCC->m_port);
                }

                ::SetDlgItemText(hwnd, IDC_HOSTNAME_EDIT, szHost.data());
                //AaronP
                HWND hPlugins = ::GetDlgItem(hwnd, IDC_PLUGINS_COMBO);
                if ((::_tcscmp(_this->szDSMPluginFilename.data(), _T("")) != 0) && _this->fUseDSMPlugin)
                {
                    int pos = static_cast<int>(::SendMessage(hPlugins, CB_FINDSTRINGEXACT, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(& (_this->szDSMPluginFilename[0U]))));

                    if (pos != CB_ERR)
                    {
                        ::SendMessage(hPlugins, CB_SETCURSEL, static_cast<WPARAM>(pos), 0);
                        HWND hUsePlugin = ::GetDlgItem(hwnd, IDC_PLUGIN_CHECK);
                        ::SendMessage(hUsePlugin, BM_SETCHECK, TRUE, 0);
                    }
                }
                //EndAaronP
                _this->LoadFromFile(szFileName.data());
                _this->SettingsToUI();
            }
            _this->m_fFromOptions = true;
            _this->m_fFromFile = true;
            return TRUE;
        }

        // [v1.0.2-jp1 fix]
        case IDC_HOSTNAME_DEL: {
            _this->SettingsFromUI();
            HWND hcombo = ::GetDlgItem(hwnd, IDC_HOSTNAME_EDIT);
            int sel = static_cast<int>(::SendMessage(hcombo, CB_GETCURSEL, 0, 0));
            if (sel != CB_ERR)
            {
                ::SendMessage(hcombo, CB_DELETESTRING, static_cast<WPARAM>(sel), 0);
                _this->m_pMRU->RemoveItem(sel);
                std::array<wchar_t, _MAX_PATH> fname;
                int disp = PORT_TO_DISPLAY(_this->m_port);
                ::swprintf_s(fname.data(), fname.size(), L"%.15s-%d.vnc", _this->m_host_dialog.data(),
                           ((disp > 0) && (disp < 100)) ? disp : _this->m_port);
                std::array<wchar_t, _MAX_PATH> buffer;
                _this->getAppData(buffer.data());
                ::wcscat_s(buffer.data(), buffer.size(), L"\\UltraVNC");
                ::_wmkdir(buffer.data());
                ::wcscat_s(buffer.data(), buffer.size(), L"\\");
                ::wcscat_s(buffer.data(), buffer.size(), fname.data());
                ::DeleteFileW(buffer.data());
                _this->SetDefaults();
            }
            return TRUE;
        }
        case IDC_LANGUAGE_COMBO:
            if (HIWORD(wParam) == CBN_SELCHANGE)
            {
                HWND hLangCombo = ::GetDlgItem(hwnd, IDC_LANGUAGE_COMBO);
                int selIdx = static_cast<int>(::SendMessage(hLangCombo, CB_GETCURSEL, 0, 0));
                if (selIdx != CB_ERR)
                {
                    wchar_t* langCode = reinterpret_cast<wchar_t*>(::SendMessage(hLangCombo, CB_GETITEMDATA, selIdx, 0));
                    if (langCode)
                    {
                        _this->SwitchLanguage(langCode);
                    }
                }
            }
            break;
        }

        break;
    case WM_DPICHANGED:
        _this->m_Dpi = HIWORD(wParam);
        return FALSE;
    /*case WM_MOVE:
        return FALSE;
    case WM_CLOSE:
        return FALSE;*/
    case WM_DESTROY:
        // Free language combobox item data
        if (::GetDlgItem(hwnd, IDC_LANGUAGE_COMBO))
        {
            HWND hLangCombo = ::GetDlgItem(hwnd, IDC_LANGUAGE_COMBO);
            const int langCount = static_cast<int>(::SendMessage(hLangCombo, CB_GETCOUNT, 0, 0));
            for (int i{ 0 }; i < langCount; ++i)
            {
                wchar_t* langCode = reinterpret_cast<wchar_t*>(::SendMessage(hLangCombo, CB_GETITEMDATA, i, 0));
                if (langCode)
                {
                    std::free(langCode);
                }
            }
        }
        ::EndDialog(hwnd, FALSE);
        ::KillTimer(hwnd, 100);
        ::KillTimer(hwnd, 101);
        return TRUE;
    case WM_TIMER:
        if (static_cast<UINT>(wParam) == 100)
        {
            ::KillTimer(hwnd, 100);
        }
        if (static_cast<UINT>(wParam) == 101)
        {
            return 0;
        }
    }
    return 0;
}


void SessionDialog::ExpandBox(const HWND hDlg, const BOOL fExpand)
{
    // if the dialog is already in the requested state, return
    // immediately.
    if (fExpand == m_bExpanded)
    {
        return;
    }

    extern std::array<TCHAR, 64U> sz_ShowOptions;
    extern std::array<TCHAR, 64U> sz_HideOptions;

    RECT rcWnd, rcDefaultBox, rcChild, rcIntersection;
    HWND wndChild = nullptr;
    HWND wndDefaultBox = nullptr;

    // get the window of the button
    HWND pCtrl = ::GetDlgItem(hDlg, IDC_SHOWOPTIONS);
    if (pCtrl == nullptr)
    {
        return;
    }

    wndDefaultBox = ::GetDlgItem(hDlg, IDC_DEFAULTBOX);
    if (wndDefaultBox == nullptr)
    {
        return;
    }

    // Change button image and text label
    if (!fExpand)
    {
        ::SendMessage(::GetDlgItem(hDlg, IDC_BUTTON_EXPAND), BM_SETIMAGE, static_cast<WPARAM>(IMAGE_BITMAP), reinterpret_cast<LPARAM>(hBmpExpand));
        ::SetWindowText(pCtrl, sz_ShowOptions.data());
    }
    else
    {
        ::SendMessage(::GetDlgItem(hDlg, IDC_BUTTON_EXPAND), BM_SETIMAGE, static_cast<WPARAM>(IMAGE_BITMAP), reinterpret_cast<LPARAM>(hBmpCollaps));
        ::SetWindowText(pCtrl, sz_HideOptions.data());
    }

    // retrieve coordinates for the default child window
    ::GetWindowRect(wndDefaultBox, &rcDefaultBox);
    //rcDefaultBox.left += 2;
    rcDefaultBox.right += 6;
    // enable/disable all of the child window outside of the default box.
    wndChild = ::GetTopWindow(hDlg);

    for (; wndChild != nullptr; wndChild = ::GetWindow(wndChild, GW_HWNDNEXT))
    {
        // get rectangle occupied by child window in screen coordinates.
        ::GetWindowRect(wndChild, &rcChild);

        if (!::IntersectRect(&rcIntersection, &rcChild, &rcDefaultBox))
        {
            ::EnableWindow(wndChild, fExpand);
        }
    }

    if (!fExpand) // we are contracting
    {
        _ASSERT(m_bExpanded);
        ::GetWindowRect(hDlg, &rcWnd);

        // Save the full expanded window dimensions for exact restoration
        m_nExpandedWidth = rcWnd.right - rcWnd.left;
        m_nExpandedHeight = rcWnd.bottom - rcWnd.top;

        // we also hide the default box here so that it is not visible
        ::ShowWindow(wndDefaultBox, SW_HIDE);

        // shrink only the height; width stays unchanged
        ::SetWindowPos(hDlg, nullptr, 0, 0, rcWnd.right - rcWnd.left, rcDefaultBox.bottom - rcWnd.top, SWP_NOZORDER | SWP_NOMOVE);

        // record that the dialog is contracted.
        m_bExpanded = FALSE;
    }
    else // we are expanding
    {
        _ASSERT(!m_bExpanded);

        // Restore only the height; keep current width unchanged
        ::GetWindowRect(hDlg, &rcWnd);
        ::SetWindowPos(hDlg, nullptr, 0, 0, rcWnd.right - rcWnd.left, m_nExpandedHeight, SWP_NOMOVE | SWP_NOZORDER);

        // make sure that the entire dialog box is visible on the user's screen.
        ::SendMessage(hDlg, DM_REPOSITION, 0, 0);
        m_bExpanded = TRUE;
    }
}

void SessionDialog::InitPlugin(const HWND hwnd)
{
    // sf@2002 - List available DSM Plugins
    HWND hPlugins = ::GetDlgItem(hwnd, IDC_PLUGINS_COMBO);
    int nPlugins = m_pDSMPlugin->ListPlugins(hPlugins);
    if (!nPlugins)
    {
        ::SendMessageW(hPlugins, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(sz_F11.data()));
    }

    ::SendMessage(hPlugins, CB_SETCURSEL, 0, 0);

    ::EnableWindow(::GetDlgItem(hwnd, IDC_PLUGIN_BUTTON), FALSE); // sf@2009 - Disable plugin config button by default
    ::EnableWindow(::GetDlgItem(hwnd, IDC_PLUGINS_COMBO), fUseDSMPlugin);

    //AaronP
    if ((::_tcscmp(szDSMPluginFilename.data(), _T("")) != 0) && fUseDSMPlugin)
    {
        int pos = static_cast<int>(::SendMessage(hPlugins, CB_FINDSTRINGEXACT, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(& (szDSMPluginFilename[0U]))));

        if (pos != CB_ERR)
        {
            ::SendMessage(hPlugins, CB_SETCURSEL, static_cast<WPARAM>(pos), 0);
            HWND hUsePlugin = ::GetDlgItem(hwnd, IDC_PLUGIN_CHECK);
            ::SendMessage(hUsePlugin, BM_SETCHECK, TRUE, 0);
            ::EnableWindow(::GetDlgItem(hwnd, IDC_PLUGIN_BUTTON), TRUE); // sf@2009 - Enable plugin config button
        }
    }
    //EndAaron
}

void SessionDialog::InitDlgProc(const bool loadhost, const bool initMruNeeded)
{
    HWND hwnd = SessHwnd;
    if (!setdefaults)
    {
        if (loadhost && ((m_pCC->m_port != 0) || (::_tcslen(m_pCC->m_host.data()) != 0)))
        {
            std::array<TCHAR, 250U> szHost;
            if (m_pCC->m_port == 5900)
            {
                ::_tcscpy_s(szHost.data(), szHost.size(), m_pCC->m_host.data());
            }
            else if ((m_pCC->m_port > 5900) && (m_pCC->m_port <= 5999))
            {
                ::_sntprintf_s(szHost.data(), szHost.size(), _TRUNCATE, _T("%s:%d"), m_pCC->m_host.data(), m_pCC->m_port - 5900);
            }
            else
            {
                ::_sntprintf_s(szHost.data(), szHost.size(), _TRUNCATE, _T("%s::%d"), m_pCC->m_host.data(), m_pCC->m_port);
            }
            ::SetDlgItemText(hwnd, IDC_HOSTNAME_EDIT, szHost.data());
        }
        else if (initMruNeeded)
        {
            InitMRU(hwnd);
        }
    }
    std::array<TCHAR, 256U> tmphost;
    std::array<TCHAR, 256U> tmphost2;
    if (::_tcscmp(m_proxyhost.data(), _T("")) != 0)
    {
        ::_tcscpy_s(tmphost.data(), tmphost.size(), m_proxyhost.data());
        ::_tcscat_s(tmphost.data(), tmphost.size(), _T(":"));
        _itot_s(m_proxyport, tmphost2.data(), tmphost2.size(), 10);
        ::_tcscat_s(tmphost.data(), tmphost.size(), tmphost2.data());
        ::SetDlgItemText(hwnd, IDC_PROXY_EDIT, tmphost.data());
    }
    else
    {
        ::SetDlgItemText(hwnd, IDC_PROXY_EDIT, _T(""));
    }

    ::SendMessage(::GetDlgItem(hwnd, IDC_RADIOREPEATER), BM_SETCHECK, false, 0);
    ::SendMessage(::GetDlgItem(hwnd, IDC_RADIODIRECT), BM_SETCHECK, false, 0);

    if (m_connectionType == REPEATER_SERVER)
    {
        ::SendMessage(::GetDlgItem(hwnd, IDC_RADIOREPEATER), BM_SETCHECK, true, 0);
        ModeSwitch(hwnd, IDC_RADIOREPEATER);
    }
    else
    {
        ::SendMessage(::GetDlgItem(hwnd, IDC_RADIODIRECT), BM_SETCHECK, true, 0);
        ModeSwitch(hwnd, IDC_RADIODIRECT);
    }

    HFONT font = ::CreateFont(24,                       // nHeight
                            0,                        // nWidth
                            0,                        // nEscapement
                            0,                        // nOrientation
                            FW_BOLD,                  // nWeight
                            false,                    // bItalic
                            FALSE,                    // bUnderline
                            0,                        // cStrikeOut
                            ANSI_CHARSET,             // nCharSet
                            OUT_DEFAULT_PRECIS,       // nOutPrecision
                            CLIP_DEFAULT_PRECIS,      // nClipPrecision
                            DEFAULT_QUALITY,          // nQuality
                            DEFAULT_PITCH | FF_SWISS, // nPitchAndFamily
                            _T("Arial"));             // lpszFacename

    ::SendMessage(::GetDlgItem(hwnd, IDC_LOGO), WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
}

void SessionDialog::InitMRU(const HWND hwnd)
{
    // Set up recently-used list
    HWND hcombo = ::GetDlgItem(hwnd, IDC_HOSTNAME_EDIT);
    std::array<TCHAR, 256U> valname;
    ::SendMessage(hcombo, CB_RESETCONTENT, 0, 0);
    for (int i{ 0 }; i < m_pMRU->NumItems(); ++i)
    {
        m_pMRU->GetItem(i, valname.data(), 255);
        if (::_tcslen(valname.data()) != 0)
        {
            ::SendMessageW(hcombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(valname.data()));
        }
    }
    ::SendMessage(hcombo, CB_SETCURSEL, 0, 0);

    m_pMRU->GetItem(0, valname.data(), 255);
    IfHostExistLoadSettings(valname.data());
}

bool SessionDialog::connect(const HWND hwnd)
{
    SettingsFromUI();
    for (int i = rfbEncodingRaw; i <= LASTENCODING; ++i)
    {
        m_pOpt->m_UseEnc[static_cast<unsigned long long>(i)] = UseEnc[static_cast<unsigned long long>(i)];
    }

    m_pOpt->m_PreferredEncodings.clear();
    for (std::size_t i{ 0U }; i < PreferredEncodings.size(); ++i)
    {
        m_pOpt->m_PreferredEncodings.push_back(PreferredEncodings[i]);
    }

    m_pOpt->autoDetect = autoDetect;
    m_pOpt->m_fExitCheck = fExitCheck;
    m_pOpt->m_connectionType = m_connectionType;
    m_pOpt->m_allowMonitorSpanning = allowMonitorSpanning;
    m_pOpt->m_ChangeServerRes = changeServerRes;
    m_pOpt->m_extendDisplay = extendDisplay;
    m_pOpt->m_showExtend = showExtend;
    m_pOpt->m_use_virt = use_virt;
    m_pOpt->m_useAllMonitors = useAllMonitors;
    m_pOpt->m_requestedWidth = requestedWidth;
    m_pOpt->m_requestedHeight = requestedHeight;
    m_pOpt->m_SwapMouse = SwapMouse;
    m_pOpt->m_DisableClipboard = DisableClipboard;
    m_pOpt->m_Use8Bit = Use8Bit;
    m_pOpt->m_Shared = Shared;
    m_pOpt->m_running = running;
    m_pOpt->m_ViewOnly = ViewOnly;
    m_pOpt->m_ShowToolbar = ShowToolbar;
    m_pOpt->m_fAutoScaling = fAutoScaling;
    m_pOpt->m_fAutoScalingEven = fAutoScalingEven;
    m_pOpt->m_fAutoScalingLimit = fAutoScalingLimit;
    m_pOpt->m_scale_num = scale_num;
    m_pOpt->m_scale_den = scale_den;
    m_pOpt->m_nServerScale = nServerScale;
    m_pOpt->m_reconnectcounter = reconnectcounter;
    m_pOpt->m_autoReconnect = autoReconnect;
    m_pOpt->m_FTTimeout = FTTimeout;
    m_pOpt->m_listenPort = listenport;
    m_pOpt->m_fEnableCache = fEnableCache;
    m_pOpt->m_fEnableZstd = fEnableZstd;
    m_pOpt->m_useCompressLevel = useCompressLevel;
    m_pOpt->m_enableJpegCompression = enableJpegCompression;
    m_pOpt->m_compressLevel = compressLevel;
    m_pOpt->m_jpegQualityLevel = jpegQualityLevel;
    m_pOpt->m_throttleMouse = throttleMouse;
    m_pOpt->m_requestShapeUpdates = requestShapeUpdates;
    m_pOpt->m_ignoreShapeUpdates = ignoreShapeUpdates;
    m_pOpt->m_BlockSameMouse = BlockSameMouse;
    m_pOpt->m_Emul3Buttons = Emul3Buttons;
    m_pOpt->m_JapKeyboard = JapKeyboard;
    m_pOpt->m_quickoption = quickoption;
    m_pOpt->m_preemptiveUpdates = preemptiveUpdates;
    m_pOpt->m_FullScreen = FullScreen;
    m_pOpt->m_Directx = Directx;
    m_pOpt->m_SavePos = SavePos;
    m_pOpt->m_SaveSize = SaveSize;
    m_pOpt->m_GNOME = GNOME;
    m_pOpt->m_fUseDSMPlugin = fUseDSMPlugin;
    ::_tcscpy_s(m_pOpt->m_szDSMPluginFilename.data(), m_pOpt->m_szDSMPluginFilename.size(), szDSMPluginFilename.data());
    m_pOpt->m_listening = listening;
    m_pOpt->m_oldplugin = oldplugin;
    ::_tcscpy_s(m_pOpt->m_document_folder.data(), m_pOpt->m_document_folder.size(), folder.data());
    ::_tcscpy_s(m_pOpt->m_prefix, _countof(m_pOpt->m_prefix), prefix);
    ::_tcscpy_s(m_pOpt->m_imageFormat.data(), m_pOpt->m_imageFormat.size(), imageFormat.data());
    m_pOpt->m_scaling = scaling;
    m_pOpt->m_keepAliveInterval = keepAliveInterval;
#ifdef _Gii
    m_pOpt->m_giiEnable = giiEnable;
#endif
    m_pOpt->m_fAutoAcceptIncoming = fAutoAcceptIncoming;
    m_pOpt->m_fAutoAcceptNoDSM = fAutoAcceptNoDSM;
    m_pOpt->m_fRequireEncryption = fRequireEncryption;
    m_pOpt->m_fUseTokenTLS = fUseTokenTLS;
    m_pOpt->m_UseOnlyDefaultConfigFile = fUseOnlyDefaultConfigFile;
    m_pOpt->m_restricted = restricted;
    m_pOpt->m_ipv6 = ipv6;
    m_pOpt->m_AllowUntrustedServers = AllowUntrustedServers;
    m_pOpt->m_NoStatus = NoStatus;
    m_pOpt->m_HideEndOfStreamError = HideEndOfStreamError;
    m_pOpt->m_NoHotKeys = NoHotKeys;

    if (fUseDSMPlugin)
    {
        if (!m_pDSMPlugin->IsLoaded())
        {
            std::array<char, MAX_PATH> szPluginA;
            ::WideCharToMultiByte(CP_ACP, 0, szDSMPluginFilename.data(), -1, szPluginA.data(), static_cast<int>(szPluginA.size()), nullptr, nullptr);
            m_pDSMPlugin->LoadPlugin(szPluginA.data(), listening);
            if (m_pDSMPlugin->IsLoaded())
            {
                if (m_pDSMPlugin->InitPlugin())
                {
                    if (!m_pDSMPlugin->SupportsMultithreaded())
                    {
                        oldplugin = true;
                    } //PGM
                    else
                    { //PGM
                        oldplugin = false;
                    }
                    m_pDSMPlugin->SetEnabled(true);
                    m_pDSMPlugin->DescribePlugin();
                }
                else
                {
                    m_pDSMPlugin->SetEnabled(false);
                    yesUVNCMessageBox(m_hInstResDLL, hwnd, sz_F7.data(), sz_F6.data(), MB_ICONEXCLAMATION);
                    return TRUE;
                }
            }
            else
            {
                m_pDSMPlugin->SetEnabled(false);
                yesUVNCMessageBox(m_hInstResDLL, hwnd, sz_F5.data(), sz_F6.data(), MB_ICONEXCLAMATION);
                return TRUE;
            }
        }
        else
        {
            // sf@2003 - If the plugin is already loaded here it has been loaded
            // by clicking on the config button: we need to init it !
            // But we must first check that the loaded plugin is the same that
            // the one currently selected...
            m_pDSMPlugin->DescribePlugin();
            if (::_tcsicmp(reinterpret_cast<LPCTSTR>(m_pDSMPlugin->GetPluginFileName()), szDSMPluginFilename.data()))
            {
                // Unload the previous plugin
                m_pDSMPlugin->UnloadPlugin();
                // Load the new selected one
                std::array<char, MAX_PATH> szPluginA;
                ::WideCharToMultiByte(CP_ACP, 0, szDSMPluginFilename.data(), -1, szPluginA.data(), static_cast<int>(szPluginA.size()), nullptr, nullptr);
                m_pDSMPlugin->LoadPlugin(szPluginA.data(), listening);
            }
            if (m_pDSMPlugin->IsLoaded())
            {
                if (m_pDSMPlugin->InitPlugin())
                {
                    if (!m_pDSMPlugin->SupportsMultithreaded())
                    {
                        oldplugin = true;
                    } //PGM
                    else
                    { //PGM
                        oldplugin = false;
                    }
                    m_pDSMPlugin->SetEnabled(true);
                    m_pDSMPlugin->DescribePlugin();
                }
                else
                {
                    m_pDSMPlugin->SetEnabled(false);
                    yesUVNCMessageBox(m_hInstResDLL, hwnd, sz_F7.data(), sz_F6.data(), MB_ICONEXCLAMATION);
                    return TRUE;
                }
            }
            else
            {
                m_pDSMPlugin->SetEnabled(false);
                yesUVNCMessageBox(m_hInstResDLL, hwnd, sz_F5.data(), sz_F6.data(), MB_ICONEXCLAMATION);
                return TRUE;
            }
        }
    }
    else
    {
        if (m_pDSMPlugin->IsEnabled())
        {
            m_pDSMPlugin->UnloadPlugin();
            m_pDSMPlugin->SetEnabled(false);
        }
    }

    std::array<wchar_t, _MAX_PATH> fname;
    int disp = PORT_TO_DISPLAY(m_port);
    std::array<wchar_t, _MAX_PATH> buffer;

    ::swprintf_s(fname.data(), fname.size(), L"%.15s-%d.vnc", m_host_dialog.data(), ((disp > 0) && (disp < 100)) ? disp : m_port);
    getAppData(buffer.data());
    ::wcscat_s(buffer.data(), buffer.size(), L"\\UltraVNC");
    ::_wmkdir(buffer.data());
    ::wcscat_s(buffer.data(), buffer.size(), L"\\");
    ::wcscat_s(buffer.data(), buffer.size(), fname.data());
    SaveToFile(buffer.data());

    std::array<TCHAR, 256U> hostname;
    ::GetDlgItemText(hwnd, IDC_HOSTNAME_EDIT, hostname.data(), 256);
    m_pMRU->AddItem(hostname.data());
    ::_tcsncpy_s(m_pOpt->m_InfoMsg.data(), m_pOpt->m_InfoMsg.size(), InfoMsg.data(), 254);
    ::EndDialog(hwnd, TRUE);
    return TRUE;
}

void SessionDialog::ModeSwitch(const HWND hwnd, const WPARAM wParam)
{
    extern std::array<TCHAR, 64U> sz_Computer;
    extern std::array<TCHAR, 64U> sz_ID;
    extern std::array<TCHAR, 64U> sz_Port;

    ::SetTimer(hwnd, 100, 1000, nullptr);
    switch (LOWORD(wParam))
    {
    case IDC_RADIOREPEATER:
        ::EnableWindow(::GetDlgItem(hwnd, IDC_PROXY_EDIT), true);
        ::ShowWindow(::GetDlgItem(hwnd, IDC_HOSTNAME_EDIT), true);
        ::ShowWindow(::GetDlgItem(hwnd, IDC_PROXY_EDIT), true);
        ::SetDlgItemText(hwnd, IDC_LINE1, sz_ID.data());
        ::SetDlgItemText(hwnd, IDC_LINE2, sz_Port.data());
        ::EnableWindow(::GetDlgItem(hwnd, IDCONNECT), true);
        break;
    case IDC_RADIODIRECT:
        ::EnableWindow(::GetDlgItem(hwnd, IDC_PROXY_EDIT), false);
        ::ShowWindow(::GetDlgItem(hwnd, IDC_HOSTNAME_EDIT), true);
        ::ShowWindow(::GetDlgItem(hwnd, IDC_PROXY_EDIT), false);
        ::SetDlgItemText(hwnd, IDC_LINE1, sz_Computer.data());
        ::SetDlgItemText(hwnd, IDC_LINE2, _T(""));
        ::EnableWindow(::GetDlgItem(hwnd, IDCONNECT), true);
        break;
    }
}

void SessionDialog::InitLanguage(const HWND hwnd)
{
    HWND hLanguageCombo = ::GetDlgItem(hwnd, IDC_LANGUAGE_COMBO);
    if (!hLanguageCombo)
    {
        return;
    }

    ::SendMessage(hLanguageCombo, CB_RESETCONTENT, 0, 0);

    // Add English (default - no DLL needed)
    int idx = static_cast<int>(::SendMessageW(hLanguageCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(_T("English"))));
    ::SendMessage(hLanguageCombo, CB_SETITEMDATA, static_cast<WPARAM>(idx), reinterpret_cast<LPARAM>(_wcsdup(L"en")));

    // Get viewer executable path
    std::array<wchar_t, MAX_PATH> exePath;
    std::array<wchar_t, MAX_PATH> languagesPath;
    std::array<wchar_t, MAX_PATH> dllPath;

    if (::GetModuleFileNameW(nullptr, exePath.data(), static_cast<DWORD>(exePath.size())))
    {
        wchar_t* lastSlash = std::wcsrchr(exePath.data(), L'\\');
        if (lastSlash)
        {
            *lastSlash = L'\0';
            ::swprintf_s(languagesPath.data(), languagesPath.size(), L"%s\\languages\\vnclang_*.dll", exePath.data());

            WIN32_FIND_DATAW findData;
            HANDLE hFind = ::FindFirstFileW(languagesPath.data(), &findData);

            if (hFind != INVALID_HANDLE_VALUE)
            {
                do
                {
                    // Extract language code from filename (vnclang_XX.dll)
                    wchar_t* underscore = std::wcsstr(findData.cFileName, L"_");
                    wchar_t* dot = std::wcsstr(findData.cFileName, L".");

                    if (underscore && dot && (dot > underscore))
                    {
                        std::array<wchar_t, 8U> langCode{ 0 };
                        std::size_t len = min(static_cast<std::size_t>(dot - underscore - 1), static_cast<std::size_t>(7));
                        ::wcsncpy_s(langCode.data(), langCode.size(), underscore + 1, len);

                        // Build full path to DLL
                        ::swprintf_s(dllPath.data(), dllPath.size(), L"%s\\languages\\%s", exePath.data(), findData.cFileName);

                        // Load DLL temporarily to read language name
                        HMODULE hLangDll = ::LoadLibraryExW(dllPath.data(), nullptr, LOAD_LIBRARY_AS_DATAFILE);
                        std::array<TCHAR, 64U> displayName{ 0 };

                        if (hLangDll)
                        {
                            // Try to load IDS_LANGUAGE_NAME string resource from DLL
                            if (::LoadStringW(hLangDll, IDS_LANGUAGE_NAME, displayName.data(), sizeof(displayName) / sizeof(TCHAR)) >
                                0)
                            {
                                // Successfully loaded language name from DLL
                                int idxInner = static_cast<int>(::SendMessageW(hLanguageCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(displayName.data())));
                                ::SendMessage(hLanguageCombo, CB_SETITEMDATA, static_cast<WPARAM>(idxInner), reinterpret_cast<LPARAM>(_wcsdup(langCode.data())));
                            }
                            else
                            {
                                // Fallback: use language code if string not found
                                ::swprintf_s(displayName.data(), displayName.size(), L"Language (%s)", langCode.data());
                                int idxInner = static_cast<int>(::SendMessageW(hLanguageCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(displayName.data())));
                                ::SendMessage(hLanguageCombo, CB_SETITEMDATA, static_cast<WPARAM>(idxInner), reinterpret_cast<LPARAM>(_wcsdup(langCode.data())));
                            }
                            ::FreeLibrary(hLangDll);
                        }
                        else
                        {
                            // DLL couldn't be loaded, use code as fallback
                            ::swprintf_s(displayName.data(), displayName.size(), L"Language (%s)", langCode.data());
                            int idxInner = static_cast<int>(::SendMessageW(hLanguageCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(displayName.data())));
                            ::SendMessage(hLanguageCombo, CB_SETITEMDATA, static_cast<WPARAM>(idxInner), reinterpret_cast<LPARAM>(_wcsdup(langCode.data())));
                        }
                    }
                } while (::FindNextFileW(hFind, &findData));
                ::FindClose(hFind);
            }
        }
    }

    // Select current language
    const int count = static_cast<int>(::SendMessage(hLanguageCombo, CB_GETCOUNT, 0, 0));
    for (int i{ 0 }; i < count; ++i)
    {
        wchar_t* langCode = reinterpret_cast<wchar_t*>(::SendMessage(hLanguageCombo, CB_GETITEMDATA, i, 0));
        if (langCode && (::_wcsicmp(langCode, m_pOpt->m_language.data()) == 0))
        {
            ::SendMessage(hLanguageCombo, CB_SETCURSEL, static_cast<WPARAM>(i), 0);
            return;
        }
    }

    ::SendMessage(hLanguageCombo, CB_SETCURSEL, 0, 0);
}

void SessionDialog::SwitchLanguage(const wchar_t* const langCode)
{
    if (!langCode)
    {
        return;
    }

    ::_tcscpy_s(m_pOpt->m_language.data(), m_pOpt->m_language.size(), langCode);
    m_pOpt->SaveOptions(m_pOpt->getDefaultOptionsFileName());

    // Reload language DLL and all strings
    ReloadLanguage(langCode);

    // Close and reopen the dialog to refresh UI
    ::EndDialog(SessHwnd, -2); // Special code to indicate language change
}
