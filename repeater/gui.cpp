// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
//


#include <array>
#include <cmath>
#include <cstring>
#include <setjmp.h>
#include <string.h>

#include "repeater.h"
#include "resource.h"
#include "resources.h"

#include <Windows.h>

#include <shellapi.h>
#include <windowsx.h>

int main_test();

#define UWM_SYSTRAY (WM_USER + 1) /* sent to us by the taskbar */
#define LOG_LINES 250
#define STRLEN 1024
#define LOG_EMERG 0
#define LOG_ALERT 1
#define LOG_CRIT 2
#define LOG_ERR 3
#define LOG_WARNING 4
#define LOG_NOTICE 5
#define LOG_INFO 6
#define LOG_DEBUG 7

#define safecopy(dst, src) (dst[STRLEN - 1] = '\0', std::strncpy((dst), (src), STRLEN - 1))
#define safeconcat(dst, src) (dst[STRLEN - 1] = '\0', strncat((dst), (src), STRLEN - std::strlen(dst) - 1))

BOOL taskbar = TRUE;
int num_clients; //undefined
typedef enum
{
    CRIT_KEYGEN,
    CRIT_NTOA,
    CRIT_CLIENTS,
    CRIT_WIN_LOG,
    CRIT_SECTIONS
} section_code;

/* Externals */
int unix_main(int, char*[]);

/* Prototypes */
static DWORD WINAPI ThreadFunc(LPVOID);
static LRESULT CALLBACK wndProc(HWND, UINT, WPARAM, LPARAM);

static int win_main(HINSTANCE, HINSTANCE, LPSTR, int);
static void save_file(HWND);
static LRESULT CALLBACK settings_proc(HWND, UINT, WPARAM, LPARAM);
static LRESULT CALLBACK about_proc(HWND, UINT, WPARAM, LPARAM);
static LRESULT CALLBACK pass_proc(HWND, UINT, WPARAM, LPARAM);
static void set_visible(int);

/* NT Service related function */
static int start_service(void);
static int install_service(void);
static int uninstall_service(void);
static void WINAPI service_main(DWORD, LPTSTR*);
static void WINAPI control_handler(DWORD);

/* Global variables */
static HINSTANCE ghInst;
static HWND EditControl = nullptr;
static HMENU htraymenu, hmainmenu;
static HMENU hpopup;
static HWND hwnd = nullptr;
static HICON small_icon; /* 16x16 icon */

static std::array<char, MAX_PATH> service_path;
static SERVICE_STATUS serviceStatus;
static SERVICE_STATUS_HANDLE serviceStatusHandle = nullptr;
static HANDLE stopServiceEvent = nullptr;

static int visible{ 0 }, error_mode = 0;
static jmp_buf jump_buf;

int portA{ 5901 };
int portS{ 5500 };
int portB{ 5500 };
BOOL mode2{ 1 };
BOOL mode1{ 0 };
BOOL keepalive{ 0 };
BOOL ssl_proxy{ 1 };
BOOL ip_service{ 1 };
BOOL allow{ 1 };
BOOL refuse{ 1 };
BOOL refuse2{ 1 };
std::array<char, 1024U> sample1;
std::array<char, 1024U> sample2;
std::array<char, 1024U> sample3;


int saved_portS{ 0 };

BOOL saved_ssl_proxy;
BOOL saved_ip_service;

char temp1[50U][25U];
int rule1{ 0 };
char temp2[50U][16U];
int rule2{ 0 };
char temp3[50U][16U];
int rule3{ 0 };

static std::array<char, STRLEN> passphrase;
extern int notstopped;
extern int notwebstopped;
extern "C" {
extern CRITICAL_SECTION cs;
}


void main_initialize([[maybe_unused]] char* const arg1, [[maybe_unused]] char* const arg2) { ::Read_settings(); }


int WINAPI WinMain(const HINSTANCE hInstance, const HINSTANCE hPrevInstance, const LPSTR lpszCmdLine, const int nCmdShow)
{
    std::array<char, MAX_PATH> exe_file_name;
    std::array<char, MAX_PATH> dir;
    char* ptr;

    ghInst = hInstance;

    ::GetModuleFileName(nullptr, exe_file_name.data(), static_cast<DWORD>(exe_file_name.size()));

    /* set current directory */
    ::strcpy_s(dir.data(), dir.size(), exe_file_name.data());
    ptr = std::strrchr(dir.data(), '\\'); /* last backslash */
    if (ptr)
    {
        ptr[1U] = '\0';
    } /* truncate program name */
    if (!::SetCurrentDirectory(dir.data()))
    {
        ::MessageBox(hwnd, "Cannot set current directory", "UltraVNC", MB_ICONERROR);
        return 1;
    }

    /* setup service_path for CreateService() */
    ::strcpy_s(service_path.data(), service_path.size(), "\"");
    ::strcat_s(service_path.data(), service_path.size(), exe_file_name.data());
    ::strcat_s(service_path.data(), service_path.size(), "\" -service");
    /* strcat_s(service_path, lpszCmdLine); */

    if (!_strcmpi(lpszCmdLine, "-service"))
    {
        if (!setjmp(jump_buf))
        {
            main_initialize(nullptr, nullptr);
        }
        return start_service(); /* Always start service with -service option */
    }

    if (!error_mode && !setjmp(jump_buf))
    { /* TRY */
        if (!_strcmpi(lpszCmdLine, "-install"))
        {
            main_initialize(nullptr, nullptr);
            return install_service();
        }
        else if (!_strcmpi(lpszCmdLine, "-uninstall"))
        {
            main_initialize(nullptr, nullptr);
            return uninstall_service();
        }
        else
        { /* not -service, -install or -uninstall */
            main_initialize(lpszCmdLine[0U] ? lpszCmdLine : nullptr, nullptr);
        }
    }

    const int returnvalue = win_main(hInstance, hPrevInstance, lpszCmdLine, nCmdShow);
    return returnvalue;
}


static int win_main(HINSTANCE hInstance, [[maybe_unused]] const HINSTANCE hPrevInstance, [[maybe_unused]] const LPSTR lpszCmdLine, [[maybe_unused]] const int nCmdShow)
{

    ::InitializeCriticalSection(&cs);
    ::Read_settings();

    DWORD iID;
    HANDLE webtemphandle = ::CreateThread(nullptr, 0, ThreadStartWeb, nullptr, 0, &iID);

    WNDCLASSEX wc;
    MSG msg;
    char classname[] = "UltraVNC_Repeater";
    RECT rect;

    /* register the class */
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_VREDRAW | CS_HREDRAW;
    wc.lpfnWndProc = wndProc;
    wc.cbClsExtra = wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = ::LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MYICON));
    wc.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>((COLOR_WINDOW + 1));
    wc.lpszMenuName = nullptr;
    wc.lpszClassName = classname;
    small_icon = (HICON)::LoadImage(hInstance, MAKEINTRESOURCE(IDI_MYICON), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
                                  GetSystemMetrics(SM_CYSMICON), 0);
    wc.hIconSm = small_icon; /* 16x16 icon */
    ::RegisterClassEx(&wc);

    /* create main window */
    if (taskbar)
    { /* save menu resources */
        htraymenu = ::LoadMenu(ghInst, MAKEINTRESOURCE(IDM_TRAYMENU));
        hpopup = ::GetSubMenu(htraymenu, 0);
    }
    hmainmenu = ::LoadMenu(ghInst, MAKEINTRESOURCE(IDM_MAINMENU));
    hwnd = ::CreateWindow(classname, "UltraVNC_Repeater", WS_TILEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                        nullptr, hmainmenu, hInstance, nullptr);

    /* create child edit window */
    EditControl = ::CreateWindow("EDIT", nullptr, WS_CHILD | WS_VISIBLE | WS_HSCROLL | WS_VSCROLL | ES_MULTILINE | ES_READONLY, 0, 0,
                               0, 0, hwnd, nullptr, hInstance, nullptr);
    ::SendMessage(EditControl, WM_SETFONT, reinterpret_cast<WPARAM>(::GetStockObject(OEM_FIXED_FONT)),
                MAKELPARAM(FALSE, 0)); /* no need to redraw right, now */
    ::GetClientRect(hwnd, &rect);
    ::MoveWindow(EditControl, 0, 0, rect.right, rect.bottom, TRUE);
    ::SetFocus(EditControl);

    if (error_mode)
    { /* log window is hidden by default */
        set_visible(1);
    }
    //else /* create the main thread */
    HANDLE temphandle = ::CreateThread(nullptr, 0, ThreadFunc, nullptr, 0, &iID);

    while (::GetMessage(&msg, nullptr, 0, 0))
    {
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
    }
    notwebstopped = false;
    notstopped = false;
    ::WaitForSingleObject(temphandle, 5000);
    if (temphandle)
    {
        ::CloseHandle(temphandle);
    }
    ::WaitForSingleObject(webtemphandle, 5000);
    if (webtemphandle)
    {
        ::CloseHandle(webtemphandle);
    }
    ::DeleteCriticalSection(&cs);
    return static_cast<int>(msg.wParam);
}

static void update_taskbar(void)
{ /* create the taskbar icon */
    NOTIFYICONDATA nid;

    ::ZeroMemory(&nid, sizeof(nid));
    nid.cbSize = sizeof(NOTIFYICONDATA); /* size */
    nid.hWnd = hwnd;                     /* window to receive notifications */
    nid.uID = 1;                         /* application-defined ID for icon */
    if (error_mode)
    {
        ::strcpy_s(nid.szTip, "Server is down");
    }
    else
    {
        ::strcpy_s(nid.szTip, "UltraVNC Repeater");
    }
    nid.uFlags = NIF_TIP;
    /* only nid.szTip and nid.uID are valid, change tip */
    if (::Shell_NotifyIcon(NIM_MODIFY, &nid))
    { /* modify tooltip */
        return;
    } /* OK: taskbar icon exists */

    /* trying to update tooltip failed - lets try to create the icon */
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = UWM_SYSTRAY;
    nid.hIcon = small_icon;          /* 16x16 icon */
    ::Shell_NotifyIcon(NIM_ADD, &nid); /* this adds the icon */
}

static DWORD WINAPI ThreadFunc([[maybe_unused]] const LPVOID arg)
{
    if (!setjmp(jump_buf))
    {
        main_test();
    }
    else
    {
        set_visible(1);
    } /* could be unsafe to call it from another thread */
    return 0;
}

static LRESULT CALLBACK wndProc(const HWND hwnd_local, UINT message, const WPARAM wParam, const LPARAM lParam)
{
    POINT pt;
    NOTIFYICONDATA nid;
    RECT rect;

#if 0
    if((message!=WM_CTLCOLORSTATIC) && (message!=WM_TIMER))
    {
        std::log(LOG_DEBUG, "Window message: %d", message);
    }
#endif
    switch (message)
    {
    case WM_CREATE:
        if (taskbar)
        { /* taskbar update mode2? */
            ::SetTimer(hwnd_local, 0x29a, 1000, nullptr);
        } /* 1-second timer */
        return TRUE;

    case WM_SIZE:
        ::GetClientRect(hwnd_local, &rect);
        ::MoveWindow(EditControl, 0, 0, rect.right, rect.bottom, TRUE);
        ::UpdateWindow(EditControl);
        return TRUE;

    case WM_TIMER:
        update_taskbar();
        return TRUE;

    case WM_CLOSE:
        set_visible(0);
        return TRUE;

    case WM_DESTROY:
        ::DestroyMenu(hmainmenu);
        ::DestroyMenu(htraymenu);
        ::ZeroMemory(&nid, sizeof(nid));
        nid.cbSize = sizeof(NOTIFYICONDATA);
        nid.hWnd = hwnd_local;
        nid.uID = 1;
        nid.uFlags = NIF_TIP;               /* not really sure what to put here, but it works */
        ::Shell_NotifyIcon(NIM_DELETE, &nid); /* this removes the icon */
        ::PostQuitMessage(0);
        ::KillTimer(hwnd_local, 0x29a);
        return TRUE;

    case WM_COMMAND:
        switch (wParam)
        {
        case IDM_ABOUT:
            ::DialogBox(ghInst, MAKEINTRESOURCE(ID_ABOUT), hwnd_local, reinterpret_cast<DLGPROC>(about_proc));
            break;
        case IDM_SETTINGS:
            ::DialogBox(ghInst, MAKEINTRESOURCE(ID_SETTINGS), hwnd_local, reinterpret_cast<DLGPROC>(settings_proc));
            break;
        case IDM_LOG:
            set_visible(!visible);
            break;
        case IDM_CLOSE:
            set_visible(0);
            break;
        case IDM_EXIT:
            ::DestroyWindow(hwnd_local);
            break;
        case IDM_SAVEAS:
            save_file(hwnd_local);
            break;
        case IDM_SETUP:
            ::MessageBox(hwnd_local, "Function not implemented", "UltraVNC_Repeater", MB_ICONERROR);
            break;
        }
        return TRUE;

    case UWM_SYSTRAY:
        switch (lParam)
        {
        case WM_RBUTTONUP: /* track a popup menu */
            /* http://support.microsoft.com/support/kb/articles/Q135/7/88.asp */
            ::GetCursorPos(&pt);
            ::SetForegroundWindow(hwnd_local);
            ::TrackPopupMenu(hpopup, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd_local, nullptr);
            ::PostMessage(hwnd_local, WM_NULL, 0, 0); /* see above */
            break;
        case WM_LBUTTONDBLCLK: /* switch log window visibility */
            //set_visible(!visible);
            break;
        }
        return TRUE;
    }
    return ::DefWindowProc(hwnd_local, message, wParam, lParam);
}

static LRESULT CALLBACK settings_proc(const HWND hDlg, const UINT message, const WPARAM wParam, [[maybe_unused]] const LPARAM lParam)
{
    std::array<char, 25U> tempchar;
    switch (message)
    {
    case WM_INITDIALOG:
        //win_log("init settings");
        portA = saved_portA;
        portB = saved_portB;
        portS = saved_portS;
        mode2 = saved_mode2;
        mode1 = saved_mode1;
        keepalive = saved_keepalive;
        ssl_proxy = saved_ssl_proxy;
        ip_service = saved_ip_service;
        allow = saved_allow;
        refuse = saved_refuse;
        refuse2 = saved_refuse2;
        ::strcpy_s(sample1.data(), sample1.size(), saved_sample1);
        ::strcpy_s(sample2.data(), sample2.size(), saved_sample2);
        ::strcpy_s(sample3.data(), sample3.size(), saved_sample3);
        ::_itoa_s(portA, tempchar.data(), tempchar.size(), 10);
        ::SetDlgItemText(hDlg, IDC_ACCEPTPORT, tempchar.data());
        ::_itoa_s(portB, tempchar.data(), tempchar.size(), 10);
        ::SetDlgItemText(hDlg, IDC_LISTENPORT, tempchar.data());
        ::SendDlgItemMessage(hDlg, IDC_ENABLE, BM_SETCHECK, static_cast<WPARAM>(mode2), 0);
        ::SendDlgItemMessage(hDlg, IDC_ENABLE2, BM_SETCHECK, static_cast<WPARAM>(mode1), 0);
        ::SendDlgItemMessage(hDlg, IDC_KEEPALIVE, BM_SETCHECK, static_cast<WPARAM>(keepalive), 0);
        ::SendDlgItemMessage(hDlg, IDC_ENABLE3, BM_SETCHECK, static_cast<WPARAM>(ssl_proxy), 0);
        ::SendDlgItemMessage(hDlg, IDC_ENABLE4, BM_SETCHECK, static_cast<WPARAM>(ip_service), 0);
        ::SendDlgItemMessage(hDlg, IDC_ALLOW, BM_SETCHECK, static_cast<WPARAM>(allow), 0);
        ::SendDlgItemMessage(hDlg, IDC_REFUSE, BM_SETCHECK, static_cast<WPARAM>(refuse), 0);
        ::SendDlgItemMessage(hDlg, IDC_REFUSE2, BM_SETCHECK, static_cast<WPARAM>(refuse2), 0);
        ::SetDlgItemText(hDlg, IDC_EDITALLOW, sample1.data());
        ::SetDlgItemText(hDlg, IDC_EDITREFUSE, sample2.data());
        ::SetDlgItemText(hDlg, IDC_EDITREFUSE2, sample3.data());
        ::_itoa_s(portS, tempchar.data(), tempchar.size(), 10);
        ::SetDlgItemText(hDlg, IDC_PORTSERVER, tempchar.data());

        if (mode2)
        {
            ::EnableWindow(::GetDlgItem(hDlg, IDC_BITMAP1), 1);
            ::EnableWindow(::GetDlgItem(hDlg, IDC_LISTENPORT), 1);
        }
        else
        {
            ::EnableWindow(::GetDlgItem(hDlg, IDC_BITMAP1), 0);
            ::EnableWindow(::GetDlgItem(hDlg, IDC_LISTENPORT), 0);
        }

        if (allow)
        {
            ::EnableWindow(::GetDlgItem(hDlg, IDC_EDITALLOW), 1);
        }
        else
        {
            ::EnableWindow(::GetDlgItem(hDlg, IDC_EDITALLOW), 0);
        }

        if (refuse)
        {
            ::EnableWindow(::GetDlgItem(hDlg, IDC_EDITREFUSE), 1);
        }
        else
        {
            ::EnableWindow(::GetDlgItem(hDlg, IDC_EDITREFUSE), 0);
        }
        if (refuse2)
        {
            ::EnableWindow(::GetDlgItem(hDlg, IDC_EDITREFUSE2), 1);
        }
        else
        {
            ::EnableWindow(::GetDlgItem(hDlg, IDC_EDITREFUSE2), 0);
        }

        return TRUE;
    case WM_COMMAND:
        switch (wParam)
        {
        case IDC_ENABLE:
            if (::SendDlgItemMessage(hDlg, IDC_ENABLE, BM_GETCHECK, 0, 0) == BST_CHECKED)
            {
                mode2 = 1;
            }
            else
            {
                mode2 = 0;
            }
            if (mode2)
            {
                ::EnableWindow(::GetDlgItem(hDlg, IDC_LISTENPORT), 1);
                ::EnableWindow(::GetDlgItem(hDlg, IDC_BITMAP1), 1);
            }
            else
            {
                ::EnableWindow(::GetDlgItem(hDlg, IDC_LISTENPORT), 0);
                ::EnableWindow(::GetDlgItem(hDlg, IDC_BITMAP1), 0);
            }
            break;

        case IDC_ENABLE2:
            if (::SendDlgItemMessage(hDlg, IDC_ENABLE2, BM_GETCHECK, 0, 0) == BST_CHECKED)
            {
                mode1 = 1;
            }
            else
            {
                mode1 = 0;
            }

        case IDC_KEEPALIVE:
            if (::SendDlgItemMessage(hDlg, IDC_KEEPALIVE, BM_GETCHECK, 0, 0) == BST_CHECKED)
            {
                keepalive = 1;
            }
            else
            {
                keepalive = 0;
            }

        case IDC_ENABLE3:
            if (::SendDlgItemMessage(hDlg, IDC_ENABLE3, BM_GETCHECK, 0, 0) == BST_CHECKED)
            {
                ssl_proxy = 1;
            }
            else
            {
                ssl_proxy = 0;
            }
        case IDC_ENABLE4:
            if (::SendDlgItemMessage(hDlg, IDC_ENABLE3, BM_GETCHECK, 0, 0) == BST_CHECKED)
            {
                ip_service = 1;
            }
            else
            {
                ip_service = 0;
            }

        case IDC_ALLOW:
            if (::SendDlgItemMessage(hDlg, IDC_ALLOW, BM_GETCHECK, 0, 0) == BST_CHECKED)
            {
                allow = 1;
            }
            else
            {
                allow = 0;
            }
            if (allow)
            {
                ::EnableWindow(::GetDlgItem(hDlg, IDC_EDITALLOW), 1);
            }
            else
            {
                ::EnableWindow(::GetDlgItem(hDlg, IDC_EDITALLOW), 0);
            }
            break;
        case IDC_REFUSE:
            if (::SendDlgItemMessage(hDlg, IDC_REFUSE, BM_GETCHECK, 0, 0) == BST_CHECKED)
            {
                refuse = 1;
            }
            else
            {
                refuse = 0;
            }
            if (refuse)
            {
                ::EnableWindow(::GetDlgItem(hDlg, IDC_EDITREFUSE), 1);
            }
            else
            {
                ::EnableWindow(::GetDlgItem(hDlg, IDC_EDITREFUSE), 0);
            }
            break;
        case IDC_REFUSE2:
            if (::SendDlgItemMessage(hDlg, IDC_REFUSE2, BM_GETCHECK, 0, 0) == BST_CHECKED)
            {
                refuse2 = 1;
            }
            else
            {
                refuse2 = 0;
            }
            if (refuse2)
            {
                ::EnableWindow(::GetDlgItem(hDlg, IDC_EDITREFUSE2), 1);
            }
            else
            {
                ::EnableWindow(::GetDlgItem(hDlg, IDC_EDITREFUSE2), 0);
            }
            break;
        case IDOK:
            ::GetDlgItemText(hDlg, IDC_ACCEPTPORT, tempchar.data(), 25);
            portA = atoi(tempchar.data());
            ::GetDlgItemText(hDlg, IDC_LISTENPORT, tempchar.data(), 25);
            portB = atoi(tempchar.data());
            ::GetDlgItemText(hDlg, IDC_PORTSERVER, tempchar.data(), 25);
            portS = atoi(tempchar.data());
            ::GetDlgItemText(hDlg, IDC_EDITALLOW, sample1.data(), 1024);
            ::GetDlgItemText(hDlg, IDC_EDITREFUSE, sample2.data(), 1024);
            ::GetDlgItemText(hDlg, IDC_EDITREFUSE2, sample3.data(), 1024);
            saved_portA = portA;
            saved_portB = portB;
            saved_portS = portS;
            saved_mode2 = mode2;
            saved_mode1 = mode1;
            saved_keepalive = keepalive;
            saved_ssl_proxy = ssl_proxy;
            saved_ip_service = ip_service;
            saved_allow = allow;
            saved_refuse = refuse;
            saved_refuse2 = refuse2;
            ::strcpy_s(saved_sample1, sample1.data());
            ::strcpy_s(saved_sample2, sample2.data());
            ::strcpy_s(saved_sample3, sample3.data());
            Save_settings();
            win_log("Settings Saved: WARNING, new settings become active after restart !");
        case IDCANCEL:
            ::EndDialog(hDlg, TRUE);
            return TRUE;
        }
    }
    return FALSE;
}

static LRESULT CALLBACK pass_proc(const HWND hDlg, const UINT message, const WPARAM wParam, const LPARAM lParam)
{
    std::array<char, STRLEN> titlebar;
    WORD cchPassword;

    switch (message)
    {
    case WM_INITDIALOG:
        /* Set the default push button to "Cancel." */
        ::SendMessage(hDlg, DM_SETDEFID, static_cast<WPARAM>(IDCANCEL), static_cast<LPARAM>(0));

        safecopy(titlebar.data(), "Private key: ");
        safeconcat(titlebar.data(), "options.key");
        ::SetWindowText(hDlg, titlebar.data());
        win_log("init");
        return TRUE;

    case WM_COMMAND:
        /* Set the default push button to "OK" when the user enters text. */
        if ((HIWORD(wParam) == EN_CHANGE) && (LOWORD(wParam) == IDE_PASSWORDEDIT))
        {
            ::SendMessage(hDlg, DM_SETDEFID, static_cast<WPARAM>(IDOK), static_cast<LPARAM>(0));
        }
        switch (wParam)
        {
        case IDOK:
            /* Get number of characters. */
            cchPassword = static_cast<WORD>(::SendDlgItemMessage(hDlg, IDE_PASSWORDEDIT, EM_LINELENGTH, (WPARAM)0, (LPARAM)0));
            if ((cchPassword == 0) || (cchPassword >= STRLEN))
            {
                ::EndDialog(hDlg, FALSE);
                return FALSE;
            }

            /* Put the number of characters into first word of buffer. */
            *(reinterpret_cast<LPWORD>(passphrase.data())) = cchPassword;

            /* Get the characters. */
            ::SendDlgItemMessage(hDlg, IDE_PASSWORDEDIT, EM_GETLINE, static_cast<WPARAM>(0), /* line 0 */ reinterpret_cast<LPARAM>(passphrase.data()));

            passphrase[cchPassword] = 0; /* Null-terminate the string. */
            ::EndDialog(hDlg, TRUE);
            return TRUE;

        case IDCANCEL:
            ::EndDialog(hDlg, FALSE);
            return TRUE;
        }
        return 0;
    }
    return FALSE;

    UNREFERENCED_PARAMETER(lParam);
}



static void save_file(const HWND hwnd_local)
{
    std::array<char, MAX_PATH> szFileName;
    OPENFILENAME ofn;
    HANDLE hFile;

    ::ZeroMemory(&ofn, sizeof(ofn));
    szFileName[0U] = '\0';

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd_local;
    ofn.lpstrFilter = "Log Files (*.log)\0*.log\0All Files (*.*)\0*.*\0\0";
    ofn.lpstrFile = szFileName.data();
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = "LOG";
    ofn.lpstrInitialDir = ".";

    ofn.lpstrTitle = "Save Log";
    ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
    if (!::GetSaveFileName(&ofn))
    {
        return;
    }

    if ((hFile = ::CreateFile(reinterpret_cast<LPCSTR>(szFileName.data()), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                            static_cast<HANDLE>(nullptr))) == INVALID_HANDLE_VALUE)
    {
        ::MessageBox(hwnd_local, "File open failed", "UltraVNC_Repeater", MB_ICONERROR);
        return;
    }
    ::CloseHandle(hFile);
}

static void set_visible(const int i)
{

    visible = i;                                                                          /* setup global variable */
    ::CheckMenuItem(hpopup, ::GetMenuItemID(hpopup, 1), visible ? MF_CHECKED : MF_UNCHECKED); /* check or uncheck menu item */
    if (visible)
    {
        ::ShowWindow(hwnd, SW_SHOWNORMAL); /* show window */
        ::SetForegroundWindow(hwnd);       /* bring on top */
    }
    else
    {
        ::ShowWindow(hwnd, SW_HIDE);
    } /* hide window */
}

static int start_service(void)
{
    SERVICE_TABLE_ENTRY serviceTable[] = { { "repeater_service", service_main }, { nullptr, nullptr } };

    if (!::StartServiceCtrlDispatcher(serviceTable))
    {
        ::MessageBox(hwnd, "Unable to start the service", "repeater_service", MB_ICONERROR);
        return 1;
    }
    return 0; /* NT service started */
}

static int install_service(void)
{
    SC_HANDLE scm, service;

    scm = ::OpenSCManager(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
    if (!scm)
    {
        ::MessageBox(hwnd, "Failed to open service control manager", "repeater_service", MB_ICONERROR);
        return 1;
    }
    service = ::CreateService(scm, "repeater_service", "repeater_service", SERVICE_ALL_ACCESS,
                            SERVICE_WIN32_OWN_PROCESS | SERVICE_INTERACTIVE_PROCESS, SERVICE_AUTO_START, SERVICE_ERROR_NORMAL,
                            service_path.data(), nullptr, nullptr, nullptr, nullptr, nullptr);
    if (!service)
    {
        ::MessageBox(hwnd, "Failed to create a new service", "repeater_service", MB_ICONERROR);
        ::CloseServiceHandle(scm);
        return 1;
    }
    ::MessageBox(hwnd, "Service installed", "repeater_service", MB_ICONINFORMATION);
    ::CloseServiceHandle(service);
    ::CloseServiceHandle(scm);
    return 0;
}

static int uninstall_service(void)
{
    SC_HANDLE scm, service;
    SERVICE_STATUS serviceStatus_local;

    scm = ::OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm)
    {
        ::MessageBox(hwnd, "Failed to open service control manager", "repeater_service", MB_ICONERROR);
        return 1;
    }
    service = ::OpenService(scm, "repeater_service", SERVICE_QUERY_STATUS | DELETE);
    if (!service)
    {
        ::MessageBox(hwnd, "Failed to open the service", "repeater_service", MB_ICONERROR);
        ::CloseServiceHandle(scm);
        return 1;
    }
    if (!::QueryServiceStatus(service, &serviceStatus_local))
    {
        ::MessageBox(hwnd, "Failed to query service status", "repeater_service", MB_ICONERROR);
        ::CloseServiceHandle(service);
        ::CloseServiceHandle(scm);
        return 1;
    }
    if (serviceStatus_local.dwCurrentState != SERVICE_STOPPED)
    {
        ::MessageBox(hwnd, "The service is still running", "repeater_service", MB_ICONERROR);
        ::CloseServiceHandle(service);
        ::CloseServiceHandle(scm);
        return 1;
    }
    if (!::DeleteService(service))
    {
        ::MessageBox(hwnd, "Failed to delete the service", "repeater_service", MB_ICONERROR);
        ::CloseServiceHandle(service);
        ::CloseServiceHandle(scm);
        return 1;
    }
    ::MessageBox(hwnd, "Service uninstalled", "repeater_service", MB_ICONINFORMATION);
    ::CloseServiceHandle(service);
    ::CloseServiceHandle(scm);
    return 0;
}

static void WINAPI service_main([[maybe_unused]] const DWORD argc, [[maybe_unused]] LPTSTR* const argv)
{
    /* initialise service status */
    serviceStatus.dwServiceType = SERVICE_WIN32;
    serviceStatus.dwCurrentState = SERVICE_STOPPED;
    serviceStatus.dwControlsAccepted = 0;
    serviceStatus.dwWin32ExitCode = NO_ERROR;
    serviceStatus.dwServiceSpecificExitCode = NO_ERROR;
    serviceStatus.dwCheckPoint = 0;
    serviceStatus.dwWaitHint = 0;

    serviceStatusHandle = ::RegisterServiceCtrlHandler("repeater_service", control_handler);

    if (serviceStatusHandle)
    {
        /* service is starting */
        serviceStatus.dwCurrentState = SERVICE_START_PENDING;
        ::SetServiceStatus(serviceStatusHandle, &serviceStatus);

        /* do initialisation here */
        stopServiceEvent = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);

        /* running */
        serviceStatus.dwControlsAccepted |= (SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN);
        serviceStatus.dwCurrentState = SERVICE_RUNNING;
        ::SetServiceStatus(serviceStatusHandle, &serviceStatus);

        win_main(ghInst, nullptr, "", 0);

        /* service was stopped */
        serviceStatus.dwCurrentState = SERVICE_STOP_PENDING;
        ::SetServiceStatus(serviceStatusHandle, &serviceStatus);

        /* do cleanup here */
        if (stopServiceEvent)
        {
            ::CloseHandle(stopServiceEvent);
        }
        stopServiceEvent = nullptr;

        /* service is now stopped */
        serviceStatus.dwControlsAccepted &= static_cast<DWORD>(~(SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN));
        serviceStatus.dwCurrentState = SERVICE_STOPPED;
        ::SetServiceStatus(serviceStatusHandle, &serviceStatus);
    }
}

static void WINAPI control_handler(DWORD controlCode)
{
    switch (controlCode)
    {
    case SERVICE_CONTROL_INTERROGATE:
        break;

    case SERVICE_CONTROL_SHUTDOWN:
    case SERVICE_CONTROL_STOP:
        serviceStatus.dwCurrentState = SERVICE_STOP_PENDING;
        ::SetServiceStatus(serviceStatusHandle, &serviceStatus);
        ::PostMessage(hwnd, WM_COMMAND, IDM_EXIT, 0);
        ::SetEvent(stopServiceEvent);
        return;

    case SERVICE_CONTROL_PAUSE:
        break;

    case SERVICE_CONTROL_CONTINUE:
        break;

    default:
        if ((controlCode >= 128) && (controlCode <= 255))
        {
            break;
        } /* user defined control code */
        else
        {
            break;
        } /* unrecognised control code */
    }

    ::SetServiceStatus(serviceStatusHandle, &serviceStatus);
}

static LRESULT CALLBACK about_proc(const HWND hDlg, const UINT message, const WPARAM wParam, [[maybe_unused]] const LPARAM lParam)
{
    switch (message)
    {
    case WM_INITDIALOG:
        return TRUE;
    case WM_COMMAND:
        switch (wParam)
        {
        case IDOK:
        case IDCANCEL:
            ::EndDialog(hDlg, TRUE);
            return TRUE;
        }
    }
    return FALSE;
}

/* End of gui.c */
