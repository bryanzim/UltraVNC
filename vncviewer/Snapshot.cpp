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

#include "Snapshot.h"

#include "common/win32_helpers.h"
#include "vncviewer.h"

#include <windows.h>

#include <gdiplus.h>
#include <shlobj.h>

#include <array>
#include <cstddef>
#include <cstdlib>
#include <ctime>
#include <cwchar>
#include <iostream>
#include <sstream>
#include <string>

#pragma comment(lib, "gdiplus")

static int CALLBACK BrowseCallbackProc(const HWND hwnd, const UINT uMsg, [[maybe_unused]] const LPARAM lParam, const LPARAM lpData)
{

    if (uMsg == BFFM_INITIALIZED)
    {
        ::SendMessage(hwnd, BFFM_SETSELECTION, TRUE, lpData);
    }

    return 0;
}

TCHAR* BrowseFolder(TCHAR* const saved_path, const HWND hwnd)
{
    static std::array<TCHAR, MAX_PATH> path;

    const TCHAR* path_param = saved_path;

    BROWSEINFO bi{ 0 };
    bi.lpszTitle = _T("Browse for folder...");
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    bi.lpfn = BrowseCallbackProc;
    bi.lParam = (LPARAM)path_param;
    bi.hwndOwner = hwnd;

    LPITEMIDLIST pidl = ::SHBrowseForFolder(&bi);

    if (pidl != 0)
    {
        //get the name of the folder and put it in path
        ::SHGetPathFromIDList(pidl, path.data());

        //free memory used
        IMalloc* imalloc = 0;
        if (SUCCEEDED(::SHGetMalloc(&imalloc)))
        {
            imalloc->Free(pidl);
            imalloc->Release();
        }
        return path.data();
    }
    return saved_path;
}
std::string datetime()
{
    time_t rawtime;
    struct tm* timeinfo;
    std::array<TCHAR, 80U> buffer;
    std::time(&rawtime);
    timeinfo = localtime(&rawtime);
    _tcsftime(buffer.data(), 80, _T("%d-%m-%Y %H-%M-%S"), timeinfo);


    std::array<char, 80U> s;
    ::WideCharToMultiByte(CP_UTF8, 0, buffer.data(), -1, s.data(), sizeof(s), nullptr, nullptr);
    return std::string(s.data());
}

static std::basic_string<TCHAR> ReplaceToken(const TCHAR* const src, const TCHAR* const token,
                                             const std::basic_string<TCHAR>& value)
{
    std::basic_string<TCHAR> out;
    const std::size_t tokenLen = ::_tcslen(token);
    for (const TCHAR* p = src; *p;)
    {
        if (::_tcsnicmp(p, token, tokenLen) == 0)
        {
            out += value;
            p += tokenLen;
        }
        else
        {
            out += *p++;
        }
    }
    return out;
}

static std::basic_string<TCHAR> SanitizeFileName(std::basic_string<TCHAR> name)
{
    for (auto& c : name)
    {
        if ((c < 32) || (::_tcschr(_T("\\/:*?\"<>|"), c) != nullptr))
        {
            c = _T('_');
        }
    }
    std::size_t start = name.find_first_not_of(_T(" "));
    std::size_t end = name.find_last_not_of(_T(" ."));
    return (start == std::basic_string<TCHAR>::npos) ? _T("") : name.substr(start, end - start + 1);
}

void Snapshot::SaveJpeg(const HBITMAP membit, TCHAR folder[MAX_PATH], TCHAR prefix[56U], TCHAR imageFormat[56U],
                        const TCHAR* hostname)
{
    ::_tcscpy_s(m_folder.data(), m_folder.size(), folder);
    ::_tcscpy_s(m_prefix, prefix);
    if ((::_tcslen(m_folder.data()) == 0) || (::_tcslen(m_prefix) == 0))
    {
        DoDialogSnapshot(m_folder.data(), m_prefix);
    }

    std::array<TCHAR, MAX_PATH> filename;
    std::array<TCHAR, MAX_PATH> expanded_filename;
    if (::GetFileAttributesW(m_folder.data()) == INVALID_FILE_ATTRIBUTES)
    {
        ::CreateDirectoryW(m_folder.data(), nullptr);
    }

    ::_tcscpy_s(filename.data(), filename.size(), m_folder.data());
    ::_tcscat_s(filename.data(), filename.size(), _T("\\"));
    ::_tcscat_s(filename.data(), filename.size(), m_prefix);

    time_t rawtime;
    struct tm* timeinfo;
    std::array<TCHAR, 80U> buffer;
    std::time(&rawtime);
    timeinfo = localtime(&rawtime);
    _tcsftime(buffer.data(), 80, _T("%Y%m%d_%H%M%S"), timeinfo);
    ::_tcscat_s(filename.data(), filename.size(), _T("_"));
    ::_tcscat_s(filename.data(), filename.size(), buffer.data());
    ::_tcscat_s(filename.data(), filename.size(), imageFormat);
    // Replace %hostname% token with the connected server's name
    std::basic_string<TCHAR> filenameExpanded =
        ReplaceToken(filename.data(), _T("%hostname%"), SanitizeFileName(hostname ? hostname : _T("")));
    ::ExpandEnvironmentStrings(filenameExpanded.c_str(), expanded_filename.data(), static_cast<DWORD>(expanded_filename.size()));
    using namespace Gdiplus;
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

    {
        Gdiplus::Bitmap bitmap(membit, nullptr);
        CLSID clsid;
        if (::_tcscmp(imageFormat, _T(".jpeg")) == 0)
        {
            GetEncoderClsid(L"image/jpeg", &clsid);
        }
        else if (::_tcscmp(imageFormat, _T(".png")) == 0)
        {
            GetEncoderClsid(L"image/png", &clsid);
        }
        else if (::_tcscmp(imageFormat, _T(".gif")) == 0)
        {
            GetEncoderClsid(L"image/gif", &clsid);
        }
        else if (::_tcscmp(imageFormat, _T(".bmp")) == 0)
        {
            GetEncoderClsid(L"image/bmp", &clsid);
        }

        bitmap.Save(expanded_filename.data(), &clsid, nullptr);
    }
    GdiplusShutdown(gdiplusToken);
}

int Snapshot::GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
    using namespace Gdiplus;
    UINT num{ 0U };  // number of image encoders
    UINT size{ 0U }; // size of the image encoder array in bytes
    ImageCodecInfo* pImageCodecInfo = nullptr;
    GetImageEncodersSize(&num, &size);
    if (size == 0)
    {
        return -1;
    } // Failure
    pImageCodecInfo = (ImageCodecInfo*)(std::malloc(size));
    if (pImageCodecInfo == nullptr)
    {
        return -1;
    } // Failure
    GetImageEncoders(num, size, pImageCodecInfo);
    for (UINT j{ 0U }; j < num; ++j)
    {
        if (std::wcscmp(pImageCodecInfo[j].MimeType, format) == 0)
        {
            *pClsid = pImageCodecInfo[j].Clsid;
            std::free(pImageCodecInfo);
            return j; // Success
        }
    }
    std::free(pImageCodecInfo);
    return 0;
}

Snapshot::Snapshot()
{
    ::_tcscpy_s(m_folder.data(), m_folder.size(), _T(""));
    ::_tcscpy_s(m_prefix, _T(""));
}

Snapshot::~Snapshot() {}

int Snapshot::DoDialogSnapshot(TCHAR folder[MAX_PATH], TCHAR prefix[56U])
{
    extern HINSTANCE m_hInstResDLL;
    ::_tcscpy_s(m_folder.data(), m_folder.size(), folder);
    ::_tcscpy_s(m_prefix, prefix);
    return static_cast<int>(::DialogBoxParam(m_hInstResDLL, DIALOG_MAKEINTRESOURCE(IDD_SAVEIMAGE), nullptr, (DLGPROC)DlgProc, (LONG_PTR)this));
}

BOOL CALLBACK Snapshot::DlgProc(const HWND hwnd, const UINT uMsg, const WPARAM wParam, const LPARAM lParam)
{
    Snapshot* _this = helper::SafeGetWindowUserData<Snapshot>(hwnd);

    switch (uMsg)
    {

    case WM_INITDIALOG: {
        helper::SafeSetWindowUserData(hwnd, lParam);
        _this = (Snapshot*)lParam;
        ::SetDlgItemText(hwnd, IDC_FOLDER, _this->m_folder.data());
        ::SetDlgItemText(hwnd, IDC_PREFIX, _this->m_prefix);
        HICON hIcon = ::LoadIcon(::GetModuleHandle(nullptr), MAKEINTRESOURCE(IDR_TRAY));
        ::SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
        ::SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
        return TRUE;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK: {
            UINT res = ::GetDlgItemText(hwnd, IDC_FOLDER, _this->m_folder.data(), 256);
            res = ::GetDlgItemText(hwnd, IDC_PREFIX, _this->m_prefix, 256);
            ::EndDialog(hwnd, TRUE);
            return TRUE;
        }
        case IDCANCEL:
            ::EndDialog(hwnd, FALSE);
            return TRUE;

        case IDC_BROWSE:
            ::_tcscpy_s(_this->m_folder.data(), _this->m_folder.size(), BrowseFolder(_this->m_folder.data(), hwnd));
            ::SetDlgItemText(hwnd, IDC_FOLDER, _this->m_folder.data());
            return TRUE;
        }

        break;
    case WM_DESTROY:
        ::EndDialog(hwnd, FALSE);
        return TRUE;
    }
    return 0;
}