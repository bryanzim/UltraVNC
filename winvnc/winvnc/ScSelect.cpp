// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
//


#ifdef SC_20
#include "ScSelect.h"

#include "common/win32_helpers.h"
#include "resource.h"
#include "winvnc.h"

#include <array>
#include <cctype>
#include <cstddef>
#include <cstdio>
#include <cstring>

#pragma comment(lib, "comctl32")
extern HINSTANCE hInstResDLL;


namespace ScSelect
{
    LONG old_pref{ 99 };
    bool g_dis_uac{ false };
    std::array<wchar_t, 150U> TextTop{};
    std::array<wchar_t, 150U> TextMiddle{};
    std::array<wchar_t, 150U> TextBottom{};
    std::array<wchar_t, 150U> TextRTop{};
    std::array<wchar_t, 150U> TextRBottom{};
    std::array<wchar_t, 150U> TextButton{};
    std::array<wchar_t, 150U> TextCButton{};
    std::array<wchar_t, 150U> TextRMiddle{};
    std::array<wchar_t, 150U> TextTitle{};
    std::array<wchar_t, 150U> Webpage{};
    bool bWebpage{ false };
    std::array<wchar_t, 150U> Balloon1Title{};
    std::array<wchar_t, 150U> Balloon2Title{};
    std::array<wchar_t, 150U> Balloon1A{};
    std::array<wchar_t, 150U> Balloon1B{};
    std::array<wchar_t, 150U> Balloon1C{};
    std::array<wchar_t, 150U> Balloon2A{};
    std::array<wchar_t, 150U> Balloon2B{};
    std::array<wchar_t, 150U> Balloon2C{};
    std::array<wchar_t, 150U> TextENTERCODE{};
    std::array<char, 15U> idStr{};
    int wdd{ 0 };
    int htd{ 0 };
    static HWND hList = nullptr;
    LVCOLUMN LvCol;
    LVITEMW LvItem;
    char g_var[25U][150U]{};
    std::array<char, 1024U> defaultCommandLine{};
    int iSelect{ 0 };
    int flag{ 0 };
    HWND hEdit = nullptr;
    int m_mytimerid{ 0 };
    wchar_t g_var_20[25U][50U]{};
    bool entercode{ false };
    bool g_wallpaper_enabled{ false };
    std::array<char, 100U> sfxname{};
    bool bExpanded{ true };
    int cx{ 0 }, cy = 0;

    ///////////////////////////////////////////////////////////////
    HBRUSH CreateGradientBrush(COLORREF top, COLORREF bottom, const LPNMCUSTOMDRAW item)
    {
        HBRUSH Brush = nullptr;
        HDC hdcmem = ::CreateCompatibleDC(item->hdc);
        HBITMAP hbitmap = ::CreateCompatibleBitmap(item->hdc, item->rc.right - item->rc.left, item->rc.bottom - item->rc.top);
        ::SelectObject(hdcmem, hbitmap);

        int r1 = GetRValue(top), r2 = GetRValue(bottom), g1 = GetGValue(top), g2 = GetGValue(bottom), b1 = GetBValue(top),
            b2 = GetBValue(bottom);
        for (int i{ 0 }; i < item->rc.bottom - item->rc.top; ++i)
        {
            RECT temp;
            int r, g, b;
            r = int(r1 + double(i * (r2 - r1) / item->rc.bottom - item->rc.top));
            g = int(g1 + double(i * (g2 - g1) / item->rc.bottom - item->rc.top));
            b = int(b1 + double(i * (b2 - b1) / item->rc.bottom - item->rc.top));
            Brush = ::CreateSolidBrush(RGB(r, g, b));
            temp.left = 0;
            temp.top = i;
            temp.right = item->rc.right - item->rc.left;
            temp.bottom = i + 1;
            ::FillRect(hdcmem, &temp, Brush);
            ::DeleteObject(Brush);
        }
        HBRUSH pattern = ::CreatePatternBrush(hbitmap);

        ::DeleteDC(hdcmem);
        ::DeleteObject(Brush);
        ::DeleteObject(hbitmap);

        return pattern;
    }


    char* InitSC(const HINSTANCE hInstance, PSTR szCmdLine)
    {
        if (std::strcmp(szCmdLine, "-dis_auc") == 0)
        {
            Disbale_UAC_for_admin();
            return 0;
        }
        if (strstr(szCmdLine, "-en_auc ") != nullptr)
        {
            std::array<char, 10U> temp;
            ::strcpy_s(temp.data(), temp.size(), szCmdLine + 8);
            int i = atoi(temp.data());
            Restore_UAC_for_admin(i);
            return 0;
        }

        if (strstr(szCmdLine, "-dsmpluginhelper ") != nullptr)
        {
            return szCmdLine;
        }

        if (strstr(szCmdLine, "-dsmplugininstance ") != nullptr)
        {
            return szCmdLine;
        }

        INITCOMMONCONTROLSEX InitCtrls;
        InitCtrls.dwICC = ICC_LISTVIEW_CLASSES | ICC_USEREX_CLASSES | ICC_BAR_CLASSES | ICC_COOL_CLASSES | ICC_TAB_CLASSES |
                          ICC_WIN95_CLASSES | ICC_PROGRESS_CLASS | ICC_PAGESCROLLER_CLASS;
        InitCtrls.dwSize = sizeof(INITCOMMONCONTROLSEX);
        const BOOL bRet = ::InitCommonControlsEx(&InitCtrls);
        ::DialogBoxParamW(hInstance, MAKEINTRESOURCEW(IDC_TITLEWINDOW), nullptr, (DLGPROC)DialogProc, 0);

        if (std::strlen(szCmdLine) == 0)
        {
            szCmdLine = defaultCommandLine.data();
        }

        for (unsigned int i{ 0U }; i < std::strlen(szCmdLine); ++i)
        {
            szCmdLine[i] = std::tolower(szCmdLine[i]);
        }





        ::strcat_s(defaultCommandLine.data(), defaultCommandLine.size(), " -run");
        return szCmdLine;
    }


    char* strreplace(char* const s, const char* const s1, const char* const s2)
    {
        char* p = strstr(s, s1);
        if (p != nullptr)
        {
            std::size_t len1 = std::strlen(s1);
            std::size_t len2 = std::strlen(s2);
            if (len1 != len2)
            {
                std::memmove(p + len2, p + len1, std::strlen(p + len1) + 1);
            }
            std::memcpy(p, s2, len2);
        }
        return s;
    }

    void connectRemote(const HWND hWnd, const HBRUSH defaultbrush, const HBRUSH hotbrush, const HBRUSH selectbrush)
    {
        std::array<char, 255U> Text{ 0 };
        std::array<char, 255U> temp{ 0 };
        std::array<char, 255U> temp1{ 0 };
        int iSlected{ 0 };
        const int j{ 0 };
        iSlected = ::SendMessageW(hList, LVM_GETNEXTITEM, -1, LVNI_FOCUSED);
        if (iSlected == -1)
        {
            helper::yesUVNCMessageBox(hInstResDLL, hWnd, "No Items in ListView", "Error", MB_ICONINFORMATION);
            return;
        }

        std::memset(&LvItem, 0, sizeof(LvItem));
        LvItem.mask = LVIF_TEXT;
        LvItem.iSubItem = 0;
        LvItem.pszText = (LPWSTR)Text.data();
        LvItem.cchTextMax = 256;
        LvItem.iItem = iSlected;
        ::SendMessageW(hList, LVM_GETITEMTEXT, iSlected, (LPARAM)&LvItem);

        ::sprintf_s(temp1.data(), Text.data());
        ::sprintf_s(temp1.data(), g_var[iSlected]);

        char* temp2;
        temp2 = _strupr(temp1.data());
        char* result = strstr(temp2, "[ENTERCODE]");
        if (result != nullptr)
        {
            ::SendDlgItemMessage(hWnd, IDC_IDCODE, WM_GETTEXT, 150, static_cast<LONG>(idStr.data()));
            if (std::strlen(idStr.data()) == 0)
            {
                ExpandBox(hWnd, result != nullptr);
                return;
            }
            strreplace(temp2, "[ENTERCODE]", idStr.data());
            ::strcpy_s(defaultCommandLine.data(), defaultCommandLine.size(), temp2);
        }
        else
        {
            ::strcpy_s(defaultCommandLine.data(), defaultCommandLine.size(), temp1.data());
        }

        ::DeleteObject(defaultbrush);
        ::DeleteObject(selectbrush);
        ::DeleteObject(hotbrush);
        ::EndDialog(hWnd, 0);
    }

    BOOL CALLBACK DialogProc(const HWND hWnd, const UINT Message, const WPARAM wParam, const LPARAM lParam)
    {
        static HBRUSH defaultbrush = nullptr;
        static HBRUSH hotbrush = nullptr;
        static HBRUSH selectbrush = nullptr;
        switch (Message)
        {

        case WM_NOTIFY: {
            LPNMHDR some_item = (LPNMHDR)lParam;

            if (((some_item->idFrom == IDC_CONNECT) || (some_item->idFrom == IDC_CLOSE) || (some_item->idFrom == IDC_HELPWEB)) &&
                (some_item->code == NM_CUSTOMDRAW))
            {
                LPNMCUSTOMDRAW item = (LPNMCUSTOMDRAW)some_item;

                if (item->uItemState & CDIS_SELECTED)
                {
                    //Select our color when the button is selected
                    if (selectbrush == nullptr)
                    {
                        selectbrush = CreateGradientBrush(RGB(220, 220, 220), RGB(220, 220, 220), item);
                    }

                    //Create pen for button border
                    HPEN pen = ::CreatePen(PS_INSIDEFRAME, 0, RGB(0, 0, 0));

                    //Select our brush into hDC
                    HGDIOBJ old_pen = ::SelectObject(item->hdc, pen);
                    HGDIOBJ old_brush = ::SelectObject(item->hdc, selectbrush);

                    //If you want rounded button, then use this, otherwise use FillRect().
                    ::RoundRect(item->hdc, item->rc.left, item->rc.top, item->rc.right, item->rc.bottom, 5, 5);

                    //Clean up
                    ::SelectObject(item->hdc, old_pen);
                    ::SelectObject(item->hdc, old_brush);
                    ::DeleteObject(pen);

                    //Now, I don't want to do anything else myself (draw text) so I use this value for return:
                    return CDRF_DODEFAULT;
                    //Let's say I wanted to draw text and stuff, then I would have to do it before return with
                    //DrawText() or other function and return CDRF_SKIPDEFAULT
                }
                else
                {
                    if (item->uItemState & CDIS_HOT) //Our mouse is over the button
                    {
                        //Select our color when the mouse hovers our button
                        if (hotbrush == nullptr)
                        {
                            hotbrush = CreateGradientBrush(RGB(220, 220, 220), RGB(255, 255, 255), item);
                        }

                        HPEN pen = ::CreatePen(PS_INSIDEFRAME, 0, RGB(0, 0, 0));

                        HGDIOBJ old_pen = ::SelectObject(item->hdc, pen);
                        HGDIOBJ old_brush = ::SelectObject(item->hdc, hotbrush);

                        ::RoundRect(item->hdc, item->rc.left, item->rc.top, item->rc.right, item->rc.bottom, 5, 5);

                        ::SelectObject(item->hdc, old_pen);
                        ::SelectObject(item->hdc, old_brush);
                        ::DeleteObject(pen);

                        return CDRF_DODEFAULT;
                    }

                    //Select our color when our button is doing nothing
                    if (defaultbrush == nullptr)
                    {
                        defaultbrush = CreateGradientBrush(RGB(255, 255, 255), RGB(220, 220, 220), item);
                    }

                    HPEN pen = ::CreatePen(PS_INSIDEFRAME, 0, RGB(0, 0, 0));

                    HGDIOBJ old_pen = ::SelectObject(item->hdc, pen);
                    HGDIOBJ old_brush = ::SelectObject(item->hdc, defaultbrush);

                    ::RoundRect(item->hdc, item->rc.left, item->rc.top, item->rc.right, item->rc.bottom, 5, 5);

                    ::SelectObject(item->hdc, old_pen);
                    ::SelectObject(item->hdc, old_brush);
                    ::DeleteObject(pen);

                    return CDRF_DODEFAULT;
                }
                return CDRF_DODEFAULT;
            }


            switch (LOWORD(wParam))
            {
            case IDC_LIST:
                if (((LPNMHDR)lParam)->code == NM_DBLCLK)
                {

                    connectRemote(hWnd, defaultbrush, hotbrush, selectbrush);
                }
                if (((LPNMHDR)lParam)->code == NM_CLICK)
                {
                    iSelect = ::SendMessageW(hList, LVM_GETNEXTITEM, -1, LVNI_FOCUSED);
                    if (iSelect == -1)
                    {
                        helper::yesUVNCMessageBox(hInstResDLL, hWnd, "No VNC Server selected", "Error", MB_ICONINFORMATION);
                        break;
                    }
                    std::array<char, 255U> temp1{ 0 };
                    char* temp2;
                    ::sprintf_s(temp1.data(), g_var[iSelect]);
                    temp2 = _strupr(temp1.data());
                    char* result = strstr(temp2, "[ENTERCODE]");
                    ExpandBox(hWnd, result != nullptr);
                    flag = 1;
                }

                if (((LPNMHDR)lParam)->code == LVN_BEGINLABELEDIT)
                {
                    hEdit = ListView_GetEditControl(hList);
                }

                if (((LPNMHDR)lParam)->code == LVN_ENDLABELEDIT)
                {
                    int iIndex;
                    std::array<char, 255U> text{};
                    iIndex = ::SendMessageW(hList, LVM_GETNEXTITEM, -1, LVNI_FOCUSED);
                    LvItem.iSubItem = 0;
                    LvItem.pszText = (LPWSTR)text.data();
                    ::GetWindowText(hEdit, text.data(), sizeof(text));
                    ::SendMessageW(hList, LVM_SETITEMTEXT, (WPARAM)iIndex, (LPARAM)&LvItem);
                }
                break;
            }
        }
        break;
        case WM_ERASEBKGND:
            return DoSDKEraseBkGnd((HDC)wParam, RGB(254, 254, 254));
        case WM_CTLCOLORSTATIC:
            //case WM_CTLCOLOREDIT:
            ::SetBkMode((HDC)wParam, TRANSPARENT);
            return static_cast<DWORD>(::GetStockObject(NULL_BRUSH));
        case WM_CLOSE:
            ::EndDialog(hWnd, 0); // kill dialog
            ::PostMessage(hWnd, WM_QUIT, 0, 0);
            exit(0);
        case WM_PAINT:
            ::InvalidateRect(hWnd, nullptr, true);
            return 0;
        case WM_TIMER:
            ::EndDialog(hWnd, 0);
            ::KillTimer(nullptr, m_mytimerid);
            break;
        case WM_INITDIALOG: {
            HICON hIcon = ::LoadIcon(::GetModuleHandle(nullptr), MAKEINTRESOURCE(IDI_WINVNC));
            ::SendMessage(hWnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
            ::SendMessage(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
            ::SetFocus(hWnd);
            hList = ::GetDlgItem(hWnd, IDC_LIST); // get the ID of the ListView
            ::SendMessageW(hList, LVM_SETEXTENDEDLISTVIEWSTYLE, 0,
                         LVS_EX_FULLROWSELECT | LVS_EX_AUTOSIZECOLUMNS | LVS_EX_FLATSB | LVS_REPORT); // Set style
            std::memset(&LvCol, 0, sizeof(LvCol));                                                    // Reset Coluom
            LvCol.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;                                       // Type of mask
            LvCol.cx = 0x200;
            std::array<char, 38U> selectHelpdask{"Select Helpdesk to connect to ......."}; // width between each coloum
            LvCol.pszText = selectHelpdask.data();                                     // First Header
            ::SendMessageW(hList, LVM_INSERTCOLUMN, 0, (LPARAM)&LvCol);           // Insert/Show the coloum
            std::memset(&LvItem, 0, sizeof(LvItem));                            // Reset Item Struct
            LvItem.mask = LVIF_TEXT;                                            // Text Style
            LvItem.cchTextMax = 256;                                            // Max size of test
            LvItem.iItem = 0;                                                   // choose item
            LvItem.iSubItem = 0;                                                // Put in first coluom
            std::array<wchar_t, 7U> item0{L"Item 0"};
            LvItem.pszText = item0.data(); // Text to display (can be from a char variable) (Items)
            FILE* fid;
            const bool done{ false };
            std::array<char, MAX_PATH> configfile;
            ::strcpy_s(configfile.data(), configfile.size(), winvncFolder.data());
            ::strcat_s(configfile.data(), configfile.size(), "\\helpdesk.txt");
            int iItem;

            int i{ 0 };
            bool direct{ false };
            if ((fid = std::fopen(configfile.data(), "r")) != nullptr)
            {
                HBITMAP hImage = (HBITMAP)::LoadImage(nullptr, "logo.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
                HWND logo = ::GetDlgItem(hWnd, IDC_LOGO);
                if (hImage)
                {
                    ::SendMessageW(logo, STM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)(HANDLE)hImage);
                }
                std::array<char, 150U> myline;
                bWebpage = false;
                ::EnableWindow(::GetDlgItem(hWnd, IDC_HELPWEB), false);
                ::ShowWindow(::GetDlgItem(hWnd, IDC_HELPWEB), SW_HIDE);
                while (fgets(myline.data(), sizeof(myline), fid))
                {
                    int j{ 0 };
                    if (std::strncmp(myline.data(), "[END]", std::strlen("[END]")) == 0)
                    {
                        break;
                    }
                    if (std::strncmp(myline.data(), "[BEGIN HOSTLIST]", std::strlen("[BEGIN HOSTLIST]")) == 0)
                    {
                        continue;
                    }
                    if (std::strncmp(myline.data(), "[HOST]", std::strlen("[HOST]")) == 0)
                    {
                        fgets(myline.data(), sizeof(myline), fid);
                        toUnicode(g_var_20[i], myline.data());
                        iItem = ::SendMessageW(hList, LVM_GETITEMCOUNT, 0, 0);
                        LvItem.iItem = iItem; // choose item
                        LvItem.iSubItem = 0;  // Put in first coluom
                        LvItem.pszText = g_var_20[i];
                        ::SendMessageW(hList, LVM_INSERTITEMW, 0, (LPARAM)&LvItem);
                        fgets(myline.data(), sizeof(myline), fid);
                        ::strcpy_s(g_var[i], "");
                        ::strcpy_s(g_var[i], myline.data());
                        ++i;
                    }

                    if (std::strncmp(myline.data(), "[TEXTTOP]", std::strlen("[TEXTTOP]")) == 0)
                    {
                        fgets(myline.data(), sizeof(myline), fid);
                        toUnicode(TextTop.data(), myline.data());
                        ::SendDlgItemMessageW(hWnd, IDC_TEXTTOP, WM_SETTEXT, 0, static_cast<LONG>(TextTop.data()));
                    }

                    if (std::strncmp(myline.data(), "[TEXTMIDDLE]", std::strlen("[TEXTMIDDLE]")) == 0)
                    {
                        fgets(myline.data(), sizeof(myline), fid);
                        toUnicode(TextMiddle.data(), myline.data());
                        ::SendDlgItemMessageW(hWnd, IDC_TEXTMIDDLE, WM_SETTEXT, 0, static_cast<LONG>(TextMiddle.data()));
                    }

                    if (std::strncmp(myline.data(), "[TEXTBOTTOM]", std::strlen("[TEXTBOTTOM]")) == 0)
                    {
                        fgets(myline.data(), sizeof(myline), fid);
                        toUnicode(TextBottom.data(), myline.data());
                        ::SendDlgItemMessageW(hWnd, IDC_TEXTBOTTOM, WM_SETTEXT, 0, static_cast<LONG>(TextBottom.data()));
                    }

                    if (std::strncmp(myline.data(), "[TEXTRTOP]", std::strlen("[TEXTRTOP]")) == 0)
                    {
                        fgets(myline.data(), sizeof(myline), fid);
                        toUnicode(TextRTop.data(), myline.data());
                        ::SendDlgItemMessageW(hWnd, IDC_TEXTRICHTTOP, WM_SETTEXT, 0, static_cast<LONG>(TextRTop.data()));
                    }

                    if (std::strncmp(myline.data(), "[TEXTRBOTTOM]", std::strlen("[TEXTRBOTTOM]")) == 0)
                    {
                        fgets(myline.data(), sizeof(myline), fid);
                        toUnicode(TextRBottom.data(), myline.data());
                        ::SendDlgItemMessageW(hWnd, IDC_TEXTRIGHTBOTTOM, WM_SETTEXT, 0, static_cast<LONG>(TextRBottom.data()));
                    }

                    if (std::strncmp(myline.data(), "[TEXTRMIDDLE]", std::strlen("[TEXTRMIDDLE]")) == 0)
                    {
                        fgets(myline.data(), sizeof(myline), fid);
                        toUnicode(TextRMiddle.data(), myline.data());
                        ::SendDlgItemMessageW(hWnd, IDC_TEXTRIGHTMIDDLE, WM_SETTEXT, 0, static_cast<LONG>(TextRMiddle.data()));
                    }

                    if (std::strncmp(myline.data(), "[TEXTBUTTON]", std::strlen("[TEXTBUTTON]")) == 0)
                    {
                        fgets(myline.data(), sizeof(myline), fid);
                        toUnicode(TextButton.data(), myline.data());
                        ::ShowWindow(::GetDlgItem(hWnd, IDC_HELPWEB), SW_SHOW);
                        ::EnableWindow(::GetDlgItem(hWnd, IDC_HELPWEB), true);
                        ::SendDlgItemMessageW(hWnd, IDC_HELPWEB, WM_SETTEXT, 0, static_cast<LONG>(TextButton.data()));
                    }

                    if (std::strncmp(myline.data(), "[TEXTCLOSEBUTTON]", std::strlen("[TEXTCLOSEBUTTON]")) == 0)
                    {
                        fgets(myline.data(), sizeof(myline), fid);
                        toUnicode(TextCButton.data(), myline.data());
                        ::SendDlgItemMessageW(hWnd, IDC_CLOSE, WM_SETTEXT, 0, static_cast<LONG>(TextCButton.data()));
                    }

                    if (std::strncmp(myline.data(), "[TEXTCONNECTBUTTON]", std::strlen("[TEXTCONNECTBUTTON]")) == 0)
                    {
                        fgets(myline.data(), sizeof(myline), fid);
                        toUnicode(TextCButton.data(), myline.data());
                        ::SendDlgItemMessageW(hWnd, IDC_CONNECT, WM_SETTEXT, 0, static_cast<LONG>(TextCButton.data()));
                    }

                    if (std::strncmp(myline.data(), "[TITLE]", std::strlen("[TITLE]")) == 0)
                    {
                        fgets(myline.data(), sizeof(myline), fid);
                        toUnicode(TextTitle.data(), myline.data());
                        ::SetWindowTextW(hWnd, TextTitle.data());
                    }

                    if (std::strncmp(myline.data(), "[WEBPAGE]", std::strlen("[WEBPAGE]")) == 0)
                    {
                        fgets(myline.data(), sizeof(myline), fid);
                        toUnicode(Webpage.data(), myline.data());
                        bWebpage = true;
                    }

                    if (std::strncmp(myline.data(), "[BALLOON1TITLE]", std::strlen("[BALLOON1TITLE]")) == 0)
                    {
                        fgets(myline.data(), sizeof(myline), fid);
                        toUnicode(Balloon1Title.data(), myline.data());
                        //Balloon1Title[strlen(Balloon1Title) - 1] = '\0';
                    }
                    if (std::strncmp(myline.data(), "[BALLOON1A]", std::strlen("[BALLOON1A]")) == 0)
                    {
                        fgets(myline.data(), sizeof(myline), fid);
                        toUnicode(Balloon1A.data(), myline.data());
                    }
                    if (std::strncmp(myline.data(), "[BALLOON1B]", std::strlen("[BALLOON1B]")) == 0)
                    {
                        fgets(myline.data(), sizeof(myline), fid);
                        toUnicode(Balloon1B.data(), myline.data());
                    }
                    if (std::strncmp(myline.data(), "[BALLOON1C]", std::strlen("[BALLOON1C]")) == 0)
                    {
                        fgets(myline.data(), sizeof(myline), fid);
                        toUnicode(Balloon1C.data(), myline.data());
                    }

                    if (std::strncmp(myline.data(), "[BALLOON2TITLE]", std::strlen("[BALLOON2TITLE]")) == 0)
                    {
                        fgets(myline.data(), sizeof(myline), fid);
                        toUnicode(Balloon2Title.data(), myline.data());
                    }
                    if (std::strncmp(myline.data(), "[BALLOON2A]", std::strlen("[BALLOON2A]")) == 0)
                    {
                        fgets(myline.data(), sizeof(myline), fid);
                        toUnicode(Balloon2A.data(), myline.data());
                    }
                    if (std::strncmp(myline.data(), "[BALLOON2B]", std::strlen("[BALLOON2B]")) == 0)
                    {
                        fgets(myline.data(), sizeof(myline), fid);
                        toUnicode(Balloon2B.data(), myline.data());
                    }
                    if (std::strncmp(myline.data(), "[BALLOON2C]", std::strlen("[BALLOON2C]")) == 0)
                    {
                        fgets(myline.data(), sizeof(myline), fid);
                        toUnicode(Balloon2C.data(), myline.data());
                    }
                    if (std::strncmp(myline.data(), "[DIRECT]", std::strlen("[DIRECT]")) == 0)
                    {
                        direct = true;
                    }
                    if (std::strncmp(myline.data(), "[ENTERCODE]", std::strlen("[ENTERCODE]")) == 0)
                    {
                        entercode = true;
                        fgets(myline.data(), sizeof(myline), fid);
                        toUnicode(TextENTERCODE.data(), myline.data());
                        ::SendDlgItemMessageW(hWnd, IDC_STATICIDCODE, WM_SETTEXT, 0, static_cast<LONG>(TextENTERCODE.data()));
                    }
                    if (std::strncmp(myline.data(), "[WALLPAPER]", std::strlen("[WALLPAPER]")) == 0)
                    {
                        g_wallpaper_enabled = true;
                    }
                    if (std::strncmp(myline.data(), "[DIS_UAC]", std::strlen("[DIS_UAC]")) == 0)
                    {
                        g_dis_uac = true;
                    }

                    if (std::strncmp(myline.data(), "[SAVEMODE]", std::strlen("[SAVEMODE]")) == 0)
                    {
                        fgets(myline.data(), sizeof(myline), fid);
                        ::strcpy_s(sfxname.data(), sfxname.size(), myline.data());
                        sfxname[std::strlen(myline.data()) - 1] = '\0';
                    }
                }
                std::fclose(fid);
            }
            else
            {
                ::EndDialog(hWnd, 0);
            }
            if ((i == 1) && direct)
            {
                ::strcpy_s(defaultCommandLine.data(), defaultCommandLine.size(), g_var[0U]);
                if (entercode)
                {
                    m_mytimerid = ::SetTimer(hWnd, 11, 20, nullptr);
                }
                else
                {
                    m_mytimerid = ::SetTimer(hWnd, 11, 2000, nullptr);
                }
            }

            ExpandBox(hWnd, !bExpanded);

            return TRUE; // Always True
        }
        break;
        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
            case IDC_CLOSE:
                ::DeleteObject(defaultbrush);
                ::DeleteObject(selectbrush);
                ::DeleteObject(hotbrush);
                ::EndDialog(hWnd, 0); // kill dialog
                ::PostMessage(hWnd, WM_QUIT, 0, 0);
                exit(0);
            case IDC_HELPWEB:
                if (bWebpage)
                {
                    ::ShellExecuteW(::GetDesktopWindow(), L"open", Webpage.data(), L"", 0, SW_SHOWNORMAL);
                }
                break;
            case IDC_CONNECT:
                connectRemote(hWnd, defaultbrush, hotbrush, selectbrush);
                break;
            }
            break;
        default:
            return FALSE;
        }
        return TRUE;
    }



    HBITMAP DoGetBkGndBitmap(IN CONST UINT uBmpResId)
    {
        static HBITMAP hbmBkGnd = nullptr;
        if (nullptr == hbmBkGnd)
        {
            hbmBkGnd = (HBITMAP)::LoadImage(nullptr, "background.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
            if (nullptr == hbmBkGnd)
            {
                hbmBkGnd = (HBITMAP)-1;
            }
        }
        return (hbmBkGnd == (HBITMAP)-1) ? nullptr : hbmBkGnd;
    }

    HBITMAP DoGetBkGndBitmap3(IN CONST UINT uBmpResId)
    {
        static HBITMAP hbmBkGnd = nullptr;
        if (nullptr == hbmBkGnd)
        {
            hbmBkGnd = (HBITMAP)::LoadImage(nullptr, "enter.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
            BITMAPINFOHEADER h2;
            h2.biSize = sizeof(h2);
            h2.biBitCount = 0;
            HDC hxdc = ::CreateDC("DISPLAY", nullptr, nullptr, nullptr);
            ::GetDIBits(hxdc, hbmBkGnd, 0, 0, nullptr, (BITMAPINFO*)&h2, DIB_RGB_COLORS);
            wdd = h2.biWidth;
            htd = h2.biHeight;
            ::DeleteDC(hxdc);
            if (nullptr == hbmBkGnd)
            {
                hbmBkGnd = (HBITMAP)-1;
            }
        }
        return (hbmBkGnd == (HBITMAP)-1) ? nullptr : hbmBkGnd;
    }

    BOOL DoSDKEraseBkGnd(IN CONST HDC hDC, IN CONST COLORREF crBkGndFill)
    {
        HBITMAP hbmBkGnd = DoGetBkGndBitmap(IDB_LOGO64);
        if (hDC || hbmBkGnd)
        {
            RECT rc;
            if ((ERROR != ::GetClipBox(hDC, &rc)) && !::IsRectEmpty(&rc))
            {
                HDC hdcMem = ::CreateCompatibleDC(hDC);
                if (hdcMem)
                {
                    HBRUSH hbrBkGnd = ::CreateSolidBrush(crBkGndFill);
                    if (hbrBkGnd)
                    {
                        HGDIOBJ hbrOld = ::SelectObject(hDC, hbrBkGnd);
                        if (hbrOld)
                        {
                            SIZE size{ (rc.right - rc.left), (rc.bottom - rc.top) };
                            if (::PatBlt(hDC, rc.left, rc.top, size.cx, size.cy, PATCOPY))
                            {
                                HGDIOBJ hbmOld = ::SelectObject(hdcMem, hbmBkGnd);
                                if (hbmOld)
                                {
                                    ::StretchBlt(hDC, rc.left, rc.top, size.cx, size.cy, hdcMem, rc.left, rc.top, 1, 283, SRCCOPY);
                                    ::SelectObject(hdcMem, hbmOld);
                                }
                            }
                            ::SelectObject(hDC, hbrOld);
                        }
                        ::DeleteObject(hbrBkGnd);
                    }
                    ::DeleteDC(hdcMem);
                }
            }
            return TRUE;
        }
        return TRUE;
    }

    int toUnicode(wchar_t* const buffer, const char* const str)
    {
        int lenW = ::MultiByteToWideChar(CP_UTF8, 0, str, -1, nullptr, 0);
        ::MultiByteToWideChar(CP_UTF8, 0, str, -1, buffer, lenW);
        return lenW;
    }

    void Disbale_UAC_for_admin_run_elevated()
    {
        HKEY hkLocal, hkLocalKey;
        DWORD dw;
        if (::RegCreateKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies", 0, REG_NONE,
                           REG_OPTION_NON_VOLATILE, KEY_READ, nullptr, &hkLocal, &dw) != ERROR_SUCCESS)
        {
            return;
        }
        if (::RegOpenKeyEx(hkLocal, "System", 0, KEY_READ, &hkLocalKey) != ERROR_SUCCESS)
        {
            ::RegCloseKey(hkLocal);
            return;
        }
        LONG pref;
        ULONG type = REG_DWORD;
        ULONG prefsize = sizeof(pref);
        if (RegQueryValueEx(hkLocalKey, "ConsentPromptBehaviorAdmin", nullptr, &type, (LPBYTE)&pref, &prefsize) != ERROR_SUCCESS)
        {
            return;
        }
        if (type != REG_DWORD)
        {
            return;
        }
        if (prefsize != sizeof(pref))
        {
            return;
        }
        old_pref = pref;
        ::RegCloseKey(hkLocalKey);
        ::RegCloseKey(hkLocal);

        std::array<char, MAX_PATH> exe_file_name;
        ::GetModuleFileName(0, exe_file_name.data(), exe_file_name.size());
        SHELLEXECUTEINFO shExecInfo;

        shExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
        shExecInfo.fMask = nullptr;
        shExecInfo.hwnd = ::GetForegroundWindow();
        shExecInfo.lpVerb = "runas";
        shExecInfo.lpFile = exe_file_name.data();
        shExecInfo.lpParameters = "-dis_auc";
        shExecInfo.lpDirectory = nullptr;
        shExecInfo.nShow = SW_SHOWNORMAL;
        shExecInfo.hInstApp = nullptr;
        ::ShellExecuteEx(&shExecInfo);
    }

    void Disbale_UAC_for_admin()
    {
        HKEY hkLocal, hkLocalKey;
        DWORD dw;
        if (::RegCreateKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies", 0, REG_NONE,
                           REG_OPTION_NON_VOLATILE, KEY_READ, nullptr, &hkLocal, &dw) != ERROR_SUCCESS)
        {
            return;
        }
        if (::RegOpenKeyEx(hkLocal, "System", 0, KEY_WRITE | KEY_READ, &hkLocalKey) != ERROR_SUCCESS)
        {
            ::RegCloseKey(hkLocal);
            return;
        }
        LONG pref;
        ULONG type = REG_DWORD;
        ULONG prefsize = sizeof(pref);
        if (RegQueryValueEx(hkLocalKey, "ConsentPromptBehaviorAdmin", nullptr, &type, (LPBYTE)&pref, &prefsize) != ERROR_SUCCESS)
        {
            return;
        }
        if (type != REG_DWORD)
        {
            return;
        }
        if (prefsize != sizeof(pref))
        {
            return;
        }
        old_pref = pref;
        pref = 0;
        ::RegSetValueEx(hkLocalKey, "ConsentPromptBehaviorAdmin", 0, REG_DWORD, (LPBYTE)&pref, sizeof(pref));
        ::RegCloseKey(hkLocalKey);
        ::RegCloseKey(hkLocal);
    }

    void Restore_UAC_for_admin_elevated()
    {
        g_dis_uac = false;
        if (old_pref == 99)
        {
            return;
        }
        HKEY hkLocal, hkLocalKey;
        DWORD dw;
        if (::RegCreateKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies", 0, REG_NONE,
                           REG_OPTION_NON_VOLATILE, KEY_READ, nullptr, &hkLocal, &dw) != ERROR_SUCCESS)
        {
            return;
        }
        if (::RegOpenKeyEx(hkLocal, "System", 0, KEY_READ, &hkLocalKey) != ERROR_SUCCESS)
        {
            ::RegCloseKey(hkLocal);
            return;
        }
        LONG pref;
        ULONG type = REG_DWORD;
        ULONG prefsize = sizeof(pref);
        if (RegQueryValueEx(hkLocalKey, "ConsentPromptBehaviorAdmin", nullptr, &type, (LPBYTE)&pref, &prefsize) != ERROR_SUCCESS)
        {
            return;
        }
        if (type != REG_DWORD)
        {
            return;
        }
        if (prefsize != sizeof(pref))
        {
            return;
        }
        ::RegCloseKey(hkLocalKey);
        ::RegCloseKey(hkLocal);
        if (pref == old_pref)
        {
            return;
        }

        std::array<char, MAX_PATH> exe_file_name;
        std::array<char, MAX_PATH> parameters;
        std::array<char, 10U> temp;
        ::GetModuleFileName(0, exe_file_name.data(), exe_file_name.size());
        ::strcpy_s(parameters.data(), parameters.size(), "-en_auc ");
        ::strcat_s(parameters.data(), parameters.size(), _itoa(old_pref, temp.data(), 10));
        SHELLEXECUTEINFO shExecInfo;
        shExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
        shExecInfo.fMask = nullptr;
        shExecInfo.hwnd = ::GetForegroundWindow();
        shExecInfo.lpVerb = "runas";
        shExecInfo.lpFile = exe_file_name.data();
        shExecInfo.lpParameters = parameters.data();
        shExecInfo.lpDirectory = nullptr;
        shExecInfo.nShow = SW_SHOWNORMAL;
        shExecInfo.hInstApp = nullptr;
        ::ShellExecuteEx(&shExecInfo);
        ::Sleep(3000);
    }

    void Restore_UAC_for_admin(const int i)
    {
        g_dis_uac = false;
        HKEY hkLocal, hkLocalKey;
        DWORD dw;
        old_pref = i;
        if (old_pref == 99)
        {
            return;
        }
        if (::RegCreateKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies", 0, REG_NONE,
                           REG_OPTION_NON_VOLATILE, KEY_READ, nullptr, &hkLocal, &dw) != ERROR_SUCCESS)
        {
            return;
        }
        if (::RegOpenKeyEx(hkLocal, "System", 0, KEY_WRITE | KEY_READ, &hkLocalKey) != ERROR_SUCCESS)
        {
            ::RegCloseKey(hkLocal);
            return;
        }
        ::RegSetValueEx(hkLocalKey, "ConsentPromptBehaviorAdmin", 0, REG_DWORD, (LPBYTE)&old_pref, sizeof(old_pref));
        ::RegCloseKey(hkLocalKey);
        ::RegCloseKey(hkLocal);
    }

    BOOL DoSDKEraseBkGnd3(IN CONST HDC hDC, IN CONST COLORREF crBkGndFill)
    {
        HBITMAP hbmBkGnd = DoGetBkGndBitmap3(IDB_LOGO64);
        if (hDC && hbmBkGnd)
        {
            RECT rc;
            if ((ERROR != ::GetClipBox(hDC, &rc)) && !::IsRectEmpty(&rc))
            {
                HDC hdcMem = ::CreateCompatibleDC(hDC);
                if (hdcMem)
                {
                    HBRUSH hbrBkGnd = ::CreateSolidBrush(crBkGndFill);
                    if (hbrBkGnd)
                    {
                        HGDIOBJ hbrOld = ::SelectObject(hDC, hbrBkGnd);
                        if (hbrOld)
                        {
                            SIZE size{ (rc.right - rc.left), (rc.bottom - rc.top) };

                            if (::PatBlt(hDC, rc.left, rc.top, size.cx, size.cy, PATCOPY))
                            {
                                HGDIOBJ hbmOld = ::SelectObject(hdcMem, hbmBkGnd);
                                if (hbmOld)
                                {
                                    ::StretchBlt(hDC, rc.left, rc.top, size.cx, size.cy, hdcMem, rc.left, rc.top, wdd, htd, SRCCOPY);
                                    ::SelectObject(hdcMem, hbmOld);
                                }
                            }
                            ::SelectObject(hDC, hbrOld);
                        }
                        ::DeleteObject(hbrBkGnd);
                    }
                    ::DeleteDC(hdcMem);
                }
            }
            return TRUE;
        }
        return false;
    }

    void ExpandBox(const HWND hDlg, const bool fExpand)
    {
        // if the dialog is already in the requested state, return
        // immediately.
        if (fExpand == bExpanded)
        {
            return;
        }
        ::SetFocus(::GetDlgItem(hDlg, IDC_IDCODE));
        RECT rcWnd{}, rcDefaultBox{}, rcChild{}, rcIntersection{};
        HWND wndChild = nullptr;
        HWND wndDefaultBox = nullptr;

        wndDefaultBox = ::GetDlgItem(hDlg, IDC_SCEXPAND);
        if (wndDefaultBox == nullptr)
        {
            return;
        }

        ::GetWindowRect(wndDefaultBox, &rcDefaultBox);

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
            _ASSERT(bExpanded);
            ::GetWindowRect(hDlg, &rcWnd);

            // this is the first time we are being called to shrink the dialog
            // box. The dialog box is currently in its expanded size and we must
            // save the expanded width and height so that it can be restored
            // later when the dialog box is expanded.

            if ((cx == 0) && (cy == 0))
            {
                cx = rcWnd.right - rcWnd.left;
                cy = rcDefaultBox.bottom - rcWnd.top;

                // we also hide the default box here so that it is not visible
                ::ShowWindow(wndDefaultBox, SW_HIDE);
            }

            // shrink the dialog box so that it encompasses everything from the top,
            // left up to and including the default box.
            ::SetWindowPos(hDlg, nullptr, 0, 0, rcDefaultBox.right - rcWnd.left, rcDefaultBox.bottom - rcWnd.top,
                         SWP_NOZORDER | SWP_NOMOVE);

            // record that the dialog is contracted.
            bExpanded = FALSE;
        }
        else // we are expanding
        {
            _ASSERT(!bExpanded);
            ::SetWindowPos(hDlg, nullptr, 0, 0, cx, cy, SWP_NOZORDER | SWP_NOMOVE);

            // make sure that the entire dialog box is visible on the user's
            // screen.
            ::SendMessage(hDlg, DM_REPOSITION, 0, 0);
            bExpanded = TRUE;
        }
    }
}
#endif // SC_20
