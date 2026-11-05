// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999 AT&T Laboratories Cambridge. All Rights Reserved.
//


// vncConnDialog.cpp: implementation of the vncConnDialog class, used
// to forge outgoing connections to VNC-viewer

#include "stdhdrs.h"

#include "vncconndialog.h"

#include "common/win32_helpers.h"
#include "Localization.h" // ACT : Add localization on messages
#include "resource.h"
#include "SettingsManager.h"
#include "winvnc.h"

#include <array>
#include <cctype>
#include <cstddef>
#include <cstring>
#include <ctype.h>
//	[v1.0.2-jp1 fix] Load resouce from dll

extern HINSTANCE hInstResDLL;

// Constructor

vncConnDialog::vncConnDialog(vncServer* server)
{
    m_server = server;
    m_hicon = nullptr;
    m_hfont = nullptr;
}

// Destructor

vncConnDialog::~vncConnDialog() {}

// Routine called to activate the dialog and, once it's done, delete it

INT_PTR vncConnDialog::DoDialog(const bool rep)
{
    m_repeater = rep;
    //	[v1.0.2-jp1 fix]
    //DialogBoxParam(hAppInstance, MAKEINTRESOURCE(IDD_OUTGOING_CONN),
    //adzm 2009-06-20 - Return the result
    INT_PTR nResult =
        ::DialogBoxParam(hInstResDLL, MAKEINTRESOURCE(IDD_OUTGOING_CONN), nullptr, (DLGPROC)vncConnDlgProc, (LONG_PTR)this);
    return nResult;
}

// Callback function - handles messages sent to the dialog box

BOOL CALLBACK vncConnDialog::vncConnDlgProc(const HWND hwnd, const UINT uMsg, const WPARAM wParam, const LPARAM lParam)
{
    // This is a static method, so we don't know which instantiation we're
    // dealing with. But we can get a pseudo-this from the parameter to
    // WM_INITDIALOG, which we therafter store with the window and retrieve
    // as follows:
    vncConnDialog* _this = helper::SafeGetWindowUserData<vncConnDialog>(hwnd);

    switch (uMsg)
    {

        // Dialog has just been created
    case WM_INITDIALOG: {
        HICON hIcon = ::LoadIcon(::GetModuleHandle(nullptr), MAKEINTRESOURCE(IDI_WINVNC));
        ::SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
        ::SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
        // Save the lParam into our user data so that subsequent calls have
        // access to the parent C++ object
        helper::SafeSetWindowUserData(hwnd, lParam);

        vncConnDialog* _this = (vncConnDialog*)lParam;

        //adzm 2009-06-20
        if (g_szRepeaterHost)
        {
            ::SetDlgItemText(hwnd, IDC_HOSTNAME_EDIT, g_szRepeaterHost);

            //adzm 2009-06-20
            if (settings->getScPrompt())
            {
                HWND hwndChild = ::GetDlgItem(hwnd, IDC_HOSTNAME_EDIT);
                if (hwndChild)
                {
                    ::ShowWindow(hwndChild, SW_HIDE);
                }
                hwndChild = ::GetDlgItem(hwnd, IDC_HOSTNAME_STATIC);
                if (hwndChild)
                {
                    ::ShowWindow(hwndChild, SW_HIDE);
                }

                HWND hwndEdit = ::GetDlgItem(hwnd, IDC_IDCODE);
                HWND hwndLabel = ::GetDlgItem(hwnd, IDC_CONNECTION_NUMBER_STATIC);

                RECT rcEdit;
                RECT rcLabel;
                ::GetWindowRect(hwndEdit, &rcEdit);
                ::GetWindowRect(hwndLabel, &rcLabel);

                LPRECT lprcEdit = &rcEdit;
                LPRECT lprcLabel = &rcLabel;

                ::ScreenToClient(hwnd, (LPPOINT)lprcEdit);
                ::ScreenToClient(hwnd, ((LPPOINT)lprcEdit) + 1);

                ::ScreenToClient(hwnd, (LPPOINT)lprcLabel);
                ::ScreenToClient(hwnd, ((LPPOINT)lprcLabel) + 1);

                RECT rcClient;
                ::GetClientRect(hwnd, &rcClient);

                const long nTotalHeight = rcEdit.bottom - rcLabel.top;

                const long nAdjustedTop = (rcClient.bottom - nTotalHeight) / 2;

                long nAdjustment = nAdjustedTop - rcLabel.top;

                ::MoveWindow(hwndLabel, rcLabel.left, rcLabel.top + nAdjustment, rcLabel.right - rcLabel.left,
                           rcLabel.bottom - rcLabel.top, TRUE);
                ::MoveWindow(hwndEdit, rcEdit.left, rcEdit.top + nAdjustment, rcEdit.right - rcEdit.left, rcEdit.bottom - rcEdit.top,
                           TRUE);

                HWND hwndCaption = ::GetDlgItem(hwnd, IDC_CAPTION_STATIC);
                HFONT hFont = (HFONT)::SendMessage(hwndCaption, WM_GETFONT, 0, 0);
                if (hFont)
                {
                    LOGFONT lf;
                    if (::GetObject(hFont, sizeof(LOGFONT), &lf))
                    {
                        lf.lfWidth = 0;
                        lf.lfHeight = (lf.lfHeight * 6) / 4;

                        _this->m_hfont = ::CreateFontIndirect(&lf);

                        if (_this->m_hfont)
                        {
                            ::SendMessage(hwndCaption, WM_SETFONT, (WPARAM)_this->m_hfont, (LPARAM)TRUE);
                        }
                    }
                }
                ::SetWindowText(hwndCaption, "Connect to Technical Support");
                ::ShowWindow(hwndCaption, SW_SHOWNA);
            }

            ::SetFocus(::GetDlgItem(hwnd, IDC_IDCODE));
        }
        else
        {
            // Make the text entry box active
            ::SetFocus(::GetDlgItem(hwnd, IDC_HOSTNAME_EDIT));
            if (_this->m_repeater)
            {
                ::SetDlgItemText(hwnd, IDC_HOSTNAME_STATIC, "Repeater:");
            }
        }

        ::SetForegroundWindow(hwnd);

        // Return success!
        return TRUE;
    }

        // Dialog has just received a command
    case WM_COMMAND:
        switch (LOWORD(wParam))
        { // User clicked OK or pressed return
        case IDOK: {
            // sf@2002 - host:num & host::num analyse.
            // Compatible with both RealVNC and TightVNC methods
            std::array<char, _MAX_PATH> hostname;
            std::array<char, _MAX_PATH> actualhostname;
            std::array<char, _MAX_PATH> idcode;
            char* portp;
            int port;
            bool id;

            // Get the hostname of the UltraVNC Viewer
            ::GetDlgItemText(hwnd, IDC_HOSTNAME_EDIT, hostname.data(), hostname.size());
            ::GetDlgItemText(hwnd, IDC_IDCODE, idcode.data(), idcode.size());
            if (std::strcmp(idcode.data(), "") == 0)
            {
                id = false;
            }
            else
            {
                id = true;
            }

            ::strcpy_s(actualhostname.data(), actualhostname.size(), hostname.data());

            //adzm 2010-02-15 - Multiple repeaters chosen by modulo of ID

            std::array<char, _MAX_PATH> finalidcode;
            //adzm 2010-08 - this was sending uninitialized data over the wire
            ::ZeroMemory(finalidcode.data(), sizeof(finalidcode));

            //adzm 2009-06-20
            if (id)
            {
                std::size_t i{ 0U };
                for (i = 0; i < std::strlen(idcode.data()); ++i)
                {
                    finalidcode[i] = std::toupper(idcode[i]);
                }
                finalidcode[i] = 0;
                if (0 != std::strncmp("ID:", idcode.data(), 3))
                {
                    ::strcpy_s(finalidcode.data(), finalidcode.size(), "ID:");
                    for (i = 0; i < std::strlen(idcode.data()); ++i)
                    {
                        finalidcode[i + 3] = std::toupper(idcode[i]);
                    }
                    finalidcode[i + 3] = 0;
                }

                //adzm 2010-02-15 - At this point, finalidcode is of the form "ID:#####"
                int numericId = atoi(finalidcode.data() + 3);

                int numberOfHosts{ 1 };
                for (i = 0; i < std::strlen(hostname.data()); ++i)
                {
                    if (hostname[i] == ';')
                    {
                        ++numberOfHosts;
                    }
                }

                if (numberOfHosts > 1)
                {
                    int modulo = numericId % numberOfHosts;
                    char* context{ nullptr };
                    char* szToken = ::strtok_s(hostname.data(), ";", &context);
                    while (szToken)
                    {
                        if (modulo == 0)
                        {
                            ::strcpy_s(actualhostname.data(), actualhostname.size(), szToken);
                            break;
                        }
                        --modulo;
                        szToken = ::strtok_s(nullptr, ";", &context);
                    }
                }
            }

            // Calculate the Display and Port offset.
            port = INCOMING_PORT_OFFSET;
            portp = std::strchr(actualhostname.data(), ':');
            if (portp)
            {
                *portp++ = '\0';
                if (*portp == ':') // Tight127 method
                {
                    port = atoi(++portp); // Port number after "::"
                }
                else // RealVNC method
                {
                    if (atoi(portp) < 100)
                    { // If < 100 after ":" -> display number
                        port += atoi(portp);
                    }
                    else
                    {
                        port = atoi(portp);
                    } // If > 100 after ":" -> Port number
                }
            }

            // Attempt to create a new socket
            VSocket* tmpsock;
            tmpsock = new VSocket;
            if (!tmpsock)
            {
                return TRUE;
            }

            // Connect out to the specified host on the UltraVNC Viewer listen port
            // To be really good, we should allow a display number here but
            // for now we'll just assume we're connecting to display zero
            bool result;
            if (settings->getIPV6())
            {
                result = tmpsock->CreateConnect(actualhostname.data(), port);
            }
            else
            {
                tmpsock->Create();
                result = tmpsock->Connect(actualhostname.data(), port);
            }
            if (result)
            {
                if (id)
                {
                    tmpsock->Send(finalidcode.data(), 250);
                    tmpsock->SetTimeout(0);
                    // adzm 2009-07-05 - repeater IDs
                    // Add the new client to this server
                    // adzm 2009-08-02
                    _this->m_server->AddClient(tmpsock, !settings->getReverseAuthRequired(), TRUE, 0, nullptr, finalidcode.data(),
                                               actualhostname.data(), port, true);
                }
                else
                {
                    // Add the new client to this server
                    // adzm 2009-08-02
                    _this->m_server->AddClient(tmpsock, !settings->getReverseAuthRequired(), TRUE, 0, nullptr, nullptr,
                                               actualhostname.data(), port, true);
                }
                // And close the dialog
                ::EndDialog(hwnd, TRUE);
            }
            else
            {
                // Print up an error message
                MessageBoxSecure(nullptr, sz_ID_FAILED_CONNECT_LISTING_VIEW.data(), sz_ID_OUTGOING_CONNECTION.data(), MB_OK | MB_ICONEXCLAMATION);
                delete tmpsock;
            }
            return TRUE;
        }

            // Cancel the dialog
        case IDCANCEL:
            ::EndDialog(hwnd, FALSE);
            return TRUE;
        };

        break;

    case WM_DESTROY:
        ::EndDialog(hwnd, FALSE);
        if (_this->m_hicon != nullptr)
        {
            ::DestroyIcon(_this->m_hicon);
            _this->m_hicon = nullptr;
        }
        if (_this->m_hfont != nullptr)
        {
            ::DeleteObject(_this->m_hfont);
            _this->m_hfont = nullptr;
        }
        return TRUE;
    }
    return 0;
}
