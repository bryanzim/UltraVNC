// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
//


#include "RulesListView.h"

#include "resource.h"
#include "SettingsManager.h"

#include <CommCtrl.h>

#include <array>
#include <cctype>
#include <cstdlib>
#include <cstring>

extern HINSTANCE hInstResDLL;

RulesListView::RulesListView() {}
RulesListView::~RulesListView() {}
void RulesListView::init(const HWND hListViewIn)
{
    this->hListView = hListViewIn;
    const char* delimiter = ":";
    if (hListViewIn)
    {
        ListView_DeleteAllItems(hListViewIn);

        LVCOLUMN lvCol;
        lvCol.mask = LVCF_TEXT | LVCF_WIDTH;
        lvCol.cx = 100; // Column width
        lvCol.pszText = const_cast<char*>("Action");
        ListView_InsertColumn(hListViewIn, 0, &lvCol);
        lvCol.cx = 300; // Column width
        lvCol.pszText = const_cast<char*>("Rule");
        ListView_InsertColumn(hListViewIn, 1, &lvCol);
        lvCol.cx = 0;
        lvCol.pszText = const_cast<char*>("string");
        ListView_InsertColumn(hListViewIn, 2, &lvCol);

        index = 0;
        char* authHost = _strdup(settings->getAuthhosts());
        if (authHost != 0)
        {
            char* context{ nullptr };
            char* token = ::strtok_s(authHost, delimiter, &context);
            while (token != nullptr)
            {

                {
                    LVITEM lvItem;
                    lvItem.mask = LVIF_TEXT; // Specifies that we're adding text
                    lvItem.iItem = index;    // Item index (position)
                    lvItem.iSubItem = 0;
                    switch (token[0U])
                    {
                    case '-':
                        lvItem.pszText = "Refuse"; // Item text
                        ListView_InsertItem(hListViewIn, &lvItem);
                        break;
                    case '+':
                        lvItem.pszText = "Allow"; // Item text
                        ListView_InsertItem(hListViewIn, &lvItem);
                        break;
                    case '?':
                        lvItem.pszText = "Query"; // Item text
                        ListView_InsertItem(hListViewIn, &lvItem);
                        break;
                    default:
                        lvItem.pszText = "Refuse"; // Item text
                        ListView_InsertItem(hListViewIn, &lvItem);
                    }
                }
                std::array<char, 1024U> restOfString;
                ::strcpy_s(restOfString.data(), restOfString.size(), token + 1);
                ListView_SetItemText(hListViewIn, index, 1, const_cast<char*>(restOfString.data()));
                ListView_SetItemText(hListViewIn, index, 2, const_cast<char*>(token));

                ++index;
                token = ::strtok_s(nullptr, delimiter, &context);
            }
        }
    }
}

void RulesListView::addItem(char* const caption)
{
    LVITEM lvItem;
    lvItem.mask = LVIF_TEXT; // Specifies that we're adding text
    lvItem.iItem = index;    // Item index (position)
    lvItem.iSubItem = 0;
    switch (caption[0U])
    {
    case '-':
        lvItem.pszText = "Refuse"; // Item text
        ListView_InsertItem(hListView, &lvItem);
        break;
    case '+':
        lvItem.pszText = "Allow"; // Item text
        ListView_InsertItem(hListView, &lvItem);
        break;
    case '?':
        lvItem.pszText = "Query"; // Item text
        ListView_InsertItem(hListView, &lvItem);
        break;
    default:
        lvItem.pszText = "Refuse"; // Item text
        ListView_InsertItem(hListView, &lvItem);
    }
    std::array<char, 1024U> restOfString;
    ::strcpy_s(restOfString.data(), restOfString.size(), caption + 1);
    ListView_SetItemText(hListView, index, 1, const_cast<char*>(restOfString.data()));
    ListView_SetItemText(hListView, index, 2, const_cast<char*>(caption));
    ++index;
}

void RulesListView::updateItem(const int indexIn, char* const caption)
{
    LVITEM lvItem;
    lvItem.mask = LVIF_TEXT; // Specifies that we're adding text
    lvItem.iItem = indexIn;    // Item index (position)
    lvItem.iSubItem = 0;
    switch (caption[0U])
    {
    case '-':
        lvItem.pszText = "Refuse"; // Item text
        ListView_SetItemText(hListView, indexIn, 0, "Refuse");
        break;
    case '+':
        lvItem.pszText = "Allow"; // Item text
        ListView_SetItemText(hListView, indexIn, 0, "Allow");
        break;
    case '?':
        lvItem.pszText = "Query"; // Item text
        ListView_SetItemText(hListView, indexIn, 0, "Query");
        break;
    default:
        lvItem.pszText = "Refuse"; // Item text
        ListView_SetItemText(hListView, indexIn, 0, "Refuse");
    }
    std::array<char, 1024U> restOfString;
    ::strcpy_s(restOfString.data(), restOfString.size(), caption + 1);
    ListView_SetItemText(hListView, indexIn, 1, const_cast<char*>(restOfString.data()));
    ListView_SetItemText(hListView, indexIn, 2, const_cast<char*>(caption));
}

void RulesListView::selectItem(const int indexIn)
{
    WPARAM itemIndex = (WPARAM)indexIn;
    ListView_SetItemState(hListView, -1, 0, LVIS_SELECTED);
    ::SendMessage(hListView, LVM_ENSUREVISIBLE, itemIndex, FALSE);
    ListView_SetItemState(hListView, itemIndex, LVIS_SELECTED, LVIS_SELECTED);
    ListView_SetItemState(hListView, itemIndex, LVIS_FOCUSED, LVIS_FOCUSED);
    ::SetFocus(hListView);
}

void RulesListView::removeSelectedItem()
{
    int iSelect = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
    ListView_DeleteItem(hListView, iSelect);

    selectItem(iSelect);
    if (ListView_GetNextItem(hListView, -1, LVNI_SELECTED) == -1)
    {
        selectItem(iSelect - 1);
    }
    else if (ListView_GetNextItem(hListView, -1, LVNI_SELECTED) == -1)
    {
        selectItem(iSelect + 1);
    }
    --index;
}

void RulesListView::moveUp()
{
    int si = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
    // If nothing selected
    if ((si == -1) || (si == 0))
    {
        return;
    }
    LVITEM lv1;
    lv1.mask = LVIF_TEXT;
    lv1.iItem = si;
    lv1.iSubItem = 2;
    std::array<char, 256U> accessRule;
    lv1.pszText = (char*)&accessRule[0U];
    lv1.cchTextMax = 256;
    ListView_GetItem(hListView, &lv1);

    LVITEM lv2;
    lv2.mask = LVIF_TEXT;
    lv2.iItem = si - 1;
    lv2.iSubItem = 2;
    std::array<char, 256U> accessRule2;
    lv2.pszText = (char*)&accessRule2[0U];
    lv2.cchTextMax = 256;
    ListView_GetItem(hListView, &lv2);

    updateItem(si - 1, accessRule.data());
    updateItem(si, accessRule2.data());
    selectItem(si - 1);
}

void RulesListView::moveDown()
{
    int si = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
    // If nothing selected
    if ((si == -1) || (si == ListView_GetItemCount(hListView) - 1))
    {
        return;
    }

    LVITEM lv1;
    lv1.mask = LVIF_TEXT;
    lv1.iItem = si;
    lv1.iSubItem = 2;
    std::array<char, 256U> accessRule;
    lv1.pszText = (char*)&accessRule[0U];
    lv1.cchTextMax = 256;
    ListView_GetItem(hListView, &lv1);

    LVITEM lv2;
    lv2.mask = LVIF_TEXT;
    lv2.iItem = si + 1;
    lv2.iSubItem = 2;
    std::array<char, 256U> accessRule2;
    lv2.pszText = (char*)&accessRule2[0U];
    lv2.cchTextMax = 256;
    ListView_GetItem(hListView, &lv2);

    updateItem(si, accessRule2.data());
    updateItem(si + 1, accessRule.data());
    selectItem(si + 1);
}


INT_PTR CALLBACK dialogProc(const HWND hwnd, const UINT uMsg, const WPARAM wParam, const LPARAM lParam)
{
    RulesListView* _this;
    BOOL bResult;

    bResult = FALSE;
    if (uMsg == WM_INITDIALOG)
    {
        _this = (RulesListView*)lParam;
        ::SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)_this);
        //_this->m_ctrlThis.setWindow(hwnd);
        //_this->updateIcon();
    }
    else
    {
        _this = (RulesListView*)::GetWindowLongPtr(hwnd, GWLP_USERDATA);
        if (_this == 0)
        {
            return FALSE;
        }
    }

    switch (uMsg)
    {
    case WM_INITDIALOG:
        bResult = _this->onInitDialog(hwnd);
        break;
    case WM_NOTIFY:
        bResult = FALSE;
        break;
    case WM_COMMAND:
        bResult = _this->onCommand(hwnd, LOWORD(wParam), HIWORD(wParam));
        break;
    case WM_CLOSE:
        bResult = FALSE;
        break;
    case WM_DESTROY:
        bResult = FALSE;
        break;
    case WM_DRAWITEM:
        bResult = TRUE;
        break;
    }

    return bResult;
}

bool RulesListView::onInitDialog(const HWND hwnd)
{
    if (isEdit)
    {
        switch (oldrule[0U])
        {
        case '-':
            ::SendDlgItemMessage(hwnd, IDC_DENY, BM_SETCHECK, true, 0);
            break;
        case '+':
            ::SendDlgItemMessage(hwnd, IDC_ALLOW, BM_SETCHECK, true, 0);
            break;
        case '?':
            ::SendDlgItemMessage(hwnd, IDC_QUERY, BM_SETCHECK, true, 0);
            break;
        }
        std::array<char, 1024U> restOfString{};
        ::strcpy_s(restOfString.data(), restOfString.size(), oldrule.data() + 1);
        ::SetDlgItemText(hwnd, IDC_FIRST_IP, restOfString.data());
    }
    else
    {
        ::SetDlgItemText(hwnd, IDC_FIRST_IP, "");
        ::SetWindowText(hwnd, "UltraVNC Server - Add IPv4 Access Rule");
    }
    HICON hIcon = ::LoadIcon(::GetModuleHandle(nullptr), MAKEINTRESOURCE(IDI_WINVNC));
    ::SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
    ::SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
    return true;
}

void RulesListView::add()
{
    isEdit = false;
    ::DialogBoxParam(hInstResDLL, MAKEINTRESOURCE(IDD_EDIT_IP_ACESS_CONTROL), nullptr, (DLGPROC)dialogProc, (LONG_PTR)this);
}
void RulesListView::edit()
{
    int si = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
    oldindex = si;
    // If nothing selected
    if (si == -1)
    {
        return;
    }
    LVITEM lv1;
    lv1.mask = LVIF_TEXT;
    lv1.iItem = si;
    lv1.iSubItem = 2;
    std::array<char, 256U> accessRule;
    lv1.pszText = (char*)&accessRule[0U];
    lv1.cchTextMax = 256;
    ListView_GetItem(hListView, &lv1);

    ::strcpy_s(oldrule.data(), oldrule.size(), accessRule.data());
    isEdit = true;
    ::DialogBoxParam(hInstResDLL, MAKEINTRESOURCE(IDD_EDIT_IP_ACESS_CONTROL), nullptr, (DLGPROC)dialogProc, (LONG_PTR)this);
}

void RulesListView::onOkButtonClick(const HWND hwnd)
{
    ::SetDlgItemText(hwnd, IDC_ERROR_TEXT, "");
    std::array<char, 256U> first;
    ::GetDlgItemText(hwnd, IDC_FIRST_IP, first.data(), 25);
    std::array<char, 256U> rule{};

    if (::SendMessage(::GetDlgItem(hwnd, IDC_ALLOW), BM_GETCHECK, 0, 0) == BST_CHECKED)
    {
        rule[0U] = '+';
    }

    else if (::SendMessage(::GetDlgItem(hwnd, IDC_DENY), BM_GETCHECK, 0, 0) == BST_CHECKED)
    {
        rule[0U] = '-';
    }

    else if (::SendMessage(::GetDlgItem(hwnd, IDC_QUERY), BM_GETCHECK, 0, 0) == BST_CHECKED)
    {
        rule[0U] = '?';
    }
    else
    {
        ::SetDlgItemText(hwnd, IDC_ERROR_TEXT, "No action selected");
        return;
    }


    ::strcat_s(rule.data(), rule.size(), first.data());

    if (!isValidIPAddress(first.data()))
    {
        ::SetDlgItemText(hwnd, IDC_ERROR_TEXT, "Ip address is invalid");
        return;
    }

    if (isEdit)
    {
        updateItem(oldindex, rule.data());
    }
    else
    {
        addItem(rule.data());
    }

    ::DestroyWindow(hwnd);
}

bool RulesListView::onCommand(const HWND hwnd, const UINT cID, const UINT nID)
{
    if (nID == BN_CLICKED)
    {
        switch (cID)
        {
        case IDOK:
            onOkButtonClick(hwnd);
            break;
        case IDCANCEL:
            ::DestroyWindow(hwnd);
            break;
        }
    }
    return TRUE;
}

bool RulesListView::isValidNumber(const char* str)
{
    int len = static_cast<int>(std::strlen(str));
    if ((len == 0) || (len > 3))
    {
        return false;
    }
    for (int i{ 0 }; i < len; ++i)
    {
        if (!std::isdigit(str[i]))
        {
            return false;
        }
    }
    int num = std::atoi(str);
    if ((num < 0) || (num > 255))
    {
        return false;
    }
    if ((len > 1) && (str[0U] == '0'))
    {
        return false;
    }
    return true;
}

bool RulesListView::isValidIPAddress(const char* const ip)
{
    std::array<char, 20U> ipCopy;
    ::strncpy_s(ipCopy.data(), ipCopy.size(), ip, 19);
    ipCopy[19U] = '\0';

    char* context{ nullptr };
    char* token = ::strtok_s(ipCopy.data(), ".", &context);
    int count{ 0 };

    while (token != nullptr)
    {
        // Check if the token (part between dots) is a valid number
        if (!isValidNumber(token))
        {
            return false;
        }

        token = ::strtok_s(nullptr, ".", &context);
        ++count;
    }

    return true;
}

char* RulesListView::getAuthHost()
{
    const int itemCount = ListView_GetItemCount(hListView);
    static std::array<char, 10000U> result;
    result[0U] = '\0'; // Clear buffer for each call
    for (int i{ 0 }; i < itemCount; ++i)
    {
        // Define a LVITEM structure to hold the item's data
        LVITEM lvItem;
        lvItem.iItem = i;        // Specify the item index
        lvItem.iSubItem = 2;     // If you want the first column (subitem)
        lvItem.mask = LVIF_TEXT; // We want the text

        // Buffer to store the item's text
        std::array<char, 256U> buffer;
        lvItem.pszText = buffer.data();
        lvItem.cchTextMax = sizeof(buffer);
        ListView_GetItem(hListView, &lvItem);
        if (i != 0)
        {
            ::strcat_s(result.data(), result.size(), ":");
        }
        ::strcat_s(result.data(), result.size(), lvItem.pszText);
    }
    return result.data();
}
