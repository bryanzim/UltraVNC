// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2025 UltraVNC Team Members. All Rights Reserved.

#include "MiniZipNG.h"

#include "../win32_helpers.h"

#include <minizip-ng/mz.h>
#include <minizip-ng/mz_strm.h>
#include <minizip-ng/mz_zip.h>
#include <minizip-ng/mz_zip_rw.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cwchar>
#include <string>
#include <vector>
// Debug output macro - only active in debug builds

#ifdef _DEBUG
#define MZ_DEBUG(x) OutputDebugStringW(x)
#define MZ_DEBUG_FMT(x, ...)                                                                                                       \
    do                                                                                                                             \
    {                                                                                                                              \
        std::array<WCHAR, 512U> _dbuf;                                                                                                         \
        ::_snwprintf_s(_dbuf.data(), 512, _TRUNCATE, x, __VA_ARGS__);                                                                       \
        ::OutputDebugStringW(_dbuf.data());                                                                                                 \
    } while (0)
#else
#define MZ_DEBUG(x)
#define MZ_DEBUG_FMT(x, ...)
#endif

// Helper: create all intermediate directories in a path
static void MZ_CreateDirectoryChain(const LPCWSTR szPath)
{
    std::array<WCHAR, MAX_PATH * 4> szTmp;
    ::wcscpy_s(szTmp.data(), szTmp.size(), szPath);
    // Remove trailing backslash
    std::size_t n = std::wcslen(szTmp.data());
    if ((n > 0) && ((szTmp[n - 1] == L'\\') || (szTmp[n - 1] == L'/')))
    {
        szTmp[--n] = L'\0';
    }
    // Walk from root forward creating each component
    for (WCHAR* p = szTmp.data() + 1; *p; ++p)
    {
        if ((*p == L'\\') || (*p == L'/'))
        {
            *p = L'\0';
            ::CreateDirectoryW(szTmp.data(), nullptr);
            *p = L'\\';
        }
    }
    ::CreateDirectoryW(szTmp.data(), nullptr);
}

CMiniZipNG::CMiniZipNG() : m_fAbortRequested{ false } {}

CMiniZipNG::~CMiniZipNG() {}

void CMiniZipNG::RequestAbort() { m_fAbortRequested = true; }

bool CMiniZipNG::IsAbortRequested() const { return m_fAbortRequested; }

void CMiniZipNG::ResetAbort() { m_fAbortRequested = false; }

// Unicode version - Zip a directory
bool CMiniZipNG::ZipDirectory(const LPCWSTR szDirPath, [[maybe_unused]] const LPCWSTR szWildcard, const LPCWSTR szZipPath, [[maybe_unused]] const bool bRecursive)
{
    MZ_DEBUG(L"[MiniZipNG] ZipDirectory START\n");
    MZ_DEBUG(L"  DirPath: ");
    MZ_DEBUG(szDirPath);
    MZ_DEBUG(L"\n");
    MZ_DEBUG(L"  ZipPath: ");
    MZ_DEBUG(szZipPath);
    MZ_DEBUG(L"\n");

    // Convert Unicode paths to UTF-8 for minizip-ng
    std::array<char, MAX_PATH * 4> szZipPathUtf8;
    ::WideCharToMultiByte(CP_UTF8, 0, szZipPath, -1, szZipPathUtf8.data(), sizeof(szZipPathUtf8), nullptr, nullptr);

    MZ_DEBUG(L"  Creating zip writer...\n");
    // Create zip writer
    void* zip_writer = mz_zip_writer_create();
    if (!zip_writer)
    {
        MZ_DEBUG(L"  ERROR: Failed to create zip writer\n");
        return false;
    }

    MZ_DEBUG(L"  Opening zip file...\n");
    // Open zip file for writing
    std::int32_t err = mz_zip_writer_open_file(zip_writer, szZipPathUtf8.data(), 0, 0);
    if (err != MZ_OK)
    {
        MZ_DEBUG(L"  ERROR: Failed to open zip file\n");
        mz_zip_writer_delete(&zip_writer);
        return false;
    }

    MZ_DEBUG(L"  Adding files recursively...\n");
    // Add files recursively
    bool bSuccess = ZipDirectoryRecursive(zip_writer, szDirPath, L"");

    MZ_DEBUG(L"  Closing zip file...\n");
    // Close zip file
    mz_zip_writer_close(zip_writer);
    mz_zip_writer_delete(&zip_writer);

    // If aborted, delete the partial zip file
    if (!bSuccess && m_fAbortRequested)
    {
        MZ_DEBUG(L"  Abort requested - deleting partial zip file\n");
        ::DeleteFileW(szZipPath);
    }

    MZ_DEBUG(bSuccess ? L"[MiniZipNG] ZipDirectory SUCCESS\n" : L"[MiniZipNG] ZipDirectory FAILED\n");
    return bSuccess;
}

// Recursive helper to add directory contents
bool CMiniZipNG::ZipDirectoryRecursive(void* const zip_writer, const LPCWSTR szBasePath, const LPCWSTR szRelPath)
{
    MZ_DEBUG(L"  [Recursive] BasePath: ");
    MZ_DEBUG(szBasePath);
    MZ_DEBUG(L" RelPath: ");
    MZ_DEBUG(szRelPath);
    MZ_DEBUG(L"\n");

    std::array<WCHAR, MAX_PATH * 4> szSearchPath;
    std::array<WCHAR, MAX_PATH * 4> szFullPath;

    // Build search pattern: basePath\relPath\*
    if (std::wcslen(szRelPath) > 0)
    {
        ::swprintf_s(szSearchPath.data(), szSearchPath.size(), L"%s\\%s\\*", szBasePath, szRelPath);
    }
    else
    {
        ::swprintf_s(szSearchPath.data(), szSearchPath.size(), L"%s\\*", szBasePath);
    }

    MZ_DEBUG(L"  [Recursive] SearchPath: ");
    MZ_DEBUG(szSearchPath.data());
    MZ_DEBUG(L"\n");

    WIN32_FIND_DATAW fd;
    HANDLE hFind = ::FindFirstFileW(szSearchPath.data(), &fd);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        MZ_DEBUG(L"  [Recursive] ERROR: FindFirstFileW failed\n");
        return false;
    }

    bool bSuccess{ true };
    do
    {
        // Check for abort request
        if (m_fAbortRequested)
        {
            MZ_DEBUG(L"  [Recursive] Abort requested - stopping\n");
            bSuccess = false;
            break;
        }

        // Skip . and ..
        if ((std::wcscmp(fd.cFileName, L".") == 0) || (std::wcscmp(fd.cFileName, L"..") == 0))
        {
            continue;
        }

        MZ_DEBUG(L"  [Recursive] Processing: ");
        MZ_DEBUG(fd.cFileName);
        MZ_DEBUG(L"\n");

        // Build full path and relative zip path
        std::array<WCHAR, MAX_PATH * 4> szZipPath;
        if (std::wcslen(szRelPath) > 0)
        {
            ::swprintf_s(szFullPath.data(), szFullPath.size(), L"%s\\%s\\%s", szBasePath, szRelPath, fd.cFileName);
            ::swprintf_s(szZipPath.data(), szZipPath.size(), L"%s\\%s", szRelPath, fd.cFileName);
        }
        else
        {
            ::swprintf_s(szFullPath.data(), szFullPath.size(), L"%s\\%s", szBasePath, fd.cFileName);
            ::wcscpy_s(szZipPath.data(), szZipPath.size(), fd.cFileName);
        }

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            MZ_DEBUG(L"  [Recursive] Entering subdirectory: ");
            MZ_DEBUG(szZipPath.data());
            MZ_DEBUG(L"\n");
            // Recursively add subdirectory
            if (!ZipDirectoryRecursive(zip_writer, szBasePath, szZipPath.data()))
            {
                bSuccess = false;
            }
        }
        else
        {
            MZ_DEBUG(L"  [Recursive] Adding file: ");
            MZ_DEBUG(szFullPath.data());
            MZ_DEBUG(L"\n");
            // Add file to zip (returns true even if skipped due to long name)
            if (!AddFileToZip(zip_writer, szFullPath.data(), szZipPath.data()))
            {
                MZ_DEBUG(L"  [Recursive] ERROR: AddFileToZip failed\n");
                // Don't fail the whole operation - continue with other files
                // bSuccess = false;
            }
        }
    } while (::FindNextFileW(hFind, &fd));

    ::FindClose(hFind);
    MZ_DEBUG(L"  [Recursive] Done with directory\n");
    return bSuccess;
}

// Add a single file to zip
bool CMiniZipNG::AddFileToZip(void* const zip_writer, const LPCWSTR szFilePath, const LPCWSTR szZipPath)
{
    // Check path length before conversion (be conservative - max 300 wide chars)
    std::size_t nWideLen = std::wcslen(szZipPath);
    if (nWideLen > 300)
    {
        MZ_DEBUG(L"  [AddFileToZip] Path too long (>300 wide chars), skipping: ");
        // Only print first 100 chars of the filename to avoid flooding debug output
        std::array<WCHAR, 128U> szTruncated;
        ::wcsncpy_s(szTruncated.data(), 128, szZipPath, 100);
        if (nWideLen > 100)
        {
            ::wcscat_s(szTruncated.data(), 128, L"...");
        }
        MZ_DEBUG(szTruncated.data());
        MZ_DEBUG(L"\n");
        return true; // Return true to continue with other files
    }

    // Convert paths to UTF-8
    std::array<char, MAX_PATH * 4> szFilePathUtf8;
    std::array<char, MAX_PATH * 4> szZipPathUtf8;
    [[maybe_unused]] const int nFilePathLen =
        ::WideCharToMultiByte(CP_UTF8, 0, szFilePath, -1, szFilePathUtf8.data(), sizeof(szFilePathUtf8), nullptr, nullptr);
    int nZipPathLen = ::WideCharToMultiByte(CP_UTF8, 0, szZipPath, -1, szZipPathUtf8.data(), sizeof(szZipPathUtf8), nullptr, nullptr);

    // Check if conversion failed or path is too long
    if ((nZipPathLen == 0) || (nZipPathLen > 512))
    {
        MZ_DEBUG(L"  [AddFileToZip] Path conversion failed or too long, skipping: ");
        MZ_DEBUG(szZipPath);
        MZ_DEBUG(L"\n");
        return true; // Return true to continue with other files
    }

    // Replace backslashes with forward slashes for zip format
    for (char* p = szZipPathUtf8.data(); *p; ++p)
    {
        if (*p == '\\')
        {
            *p = '/';
        }
    }

    // Get file info
    mz_zip_file file_info{ 0 };
    file_info.filename = szZipPathUtf8.data();
    file_info.compression_method = MZ_COMPRESS_METHOD_STORE; // No compression - faster transfers
    file_info.flag = MZ_ZIP_FLAG_UTF8;

    // Get file times - convert Windows FILETIME to Unix time_t
    WIN32_FILE_ATTRIBUTE_DATA attr;
    if (::GetFileAttributesExW(szFilePath, GetFileExInfoStandard, &attr))
    {
        // Convert FILETIME (100-nanosecond intervals since 1601) to Unix time (seconds since 1970)
        ULARGE_INTEGER ull;
        ull.LowPart = attr.ftLastWriteTime.dwLowDateTime;
        ull.HighPart = attr.ftLastWriteTime.dwHighDateTime;
        file_info.modified_date = static_cast<time_t>(((ull.QuadPart / 10000000ULL) - 11644473600ULL));
    }

    // Open entry in zip
    std::int32_t err = mz_zip_writer_entry_open(zip_writer, &file_info);
    if (err != MZ_OK)
    {
        MZ_DEBUG_FMT(L"  [AddFileToZip] Failed to open entry, error: %d\n", err);
        // Try to close any partial entry to keep writer in valid state
        mz_zip_writer_entry_close(zip_writer);
        MZ_DEBUG(L"  [AddFileToZip] Skipping file and continuing\n");
        return true; // Return true to continue with other files
    }

    // Read and write file data
    HANDLE hFile = ::CreateFileW(szFilePath, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        mz_zip_writer_entry_close(zip_writer);
        return false;
    }

    const DWORD BUFFER_SIZE = 64U * 1024;
    std::vector<BYTE> buffer(BUFFER_SIZE);
    DWORD dwRead;
    bool bSuccess{ true };

    while (::ReadFile(hFile, buffer.data(), BUFFER_SIZE, &dwRead, nullptr) && (dwRead > 0))
    {
        if (mz_zip_writer_entry_write(zip_writer, buffer.data(), static_cast<int32_t>(dwRead)) != dwRead)
        {
            bSuccess = false;
            break;
        }
    }

    ::CloseHandle(hFile);
    mz_zip_writer_entry_close(zip_writer);
    return bSuccess;
}

// Unicode version - Unzip a directory
bool CMiniZipNG::UnZipDirectory(LPCWSTR szExtractPath, const LPCWSTR szZipPath)
{
    MZ_DEBUG(L"[MiniZipNG] UnZipDirectory START\n");
    MZ_DEBUG(L"  Zip: ");
    MZ_DEBUG(szZipPath);
    MZ_DEBUG(L"\n");
    MZ_DEBUG(L"  Extract to: ");
    MZ_DEBUG(szExtractPath);
    MZ_DEBUG(L"\n");

    // Convert paths to UTF-8
    std::array<char, MAX_PATH * 4> szZipPathUtf8;
    std::array<char, MAX_PATH * 4> szExtractPathUtf8;
    ::WideCharToMultiByte(CP_UTF8, 0, szZipPath, -1, szZipPathUtf8.data(), sizeof(szZipPathUtf8), nullptr, nullptr);
    ::WideCharToMultiByte(CP_UTF8, 0, szExtractPath, -1, szExtractPathUtf8.data(), sizeof(szExtractPathUtf8), nullptr, nullptr);

    // Create zip reader
    void* zip_reader = mz_zip_reader_create();
    if (!zip_reader)
    {
        MZ_DEBUG(L"[MiniZipNG] Failed to create zip reader\n");
        return false;
    }

    // Open zip file
    std::int32_t err = mz_zip_reader_open_file(zip_reader, szZipPathUtf8.data());
    if (err != MZ_OK)
    {
        MZ_DEBUG(L"[MiniZipNG] Failed to open zip file\n");
        mz_zip_reader_delete(&zip_reader);
        return false;
    }

    // Extract files individually with long path support
    bool bAllOk{ true };
    std::int32_t num_errors{ 0 };

    // Convert extract path to use \\?\ prefix for long path support
    std::array<WCHAR, MAX_PATH * 4> szLongPathW;
    if (szExtractPath[0U] && (szExtractPath[1U] == L':'))
    {
        // Absolute path - add \\?\ prefix
        ::_snwprintf_s(szLongPathW.data(), szLongPathW.size(), _TRUNCATE, L"\\\\?\\%s", szExtractPath);
    }
    else
    {
        ::wcscpy_s(szLongPathW.data(), szLongPathW.size(), szExtractPath);
    }

    // Canonical extraction root without the \\?\ prefix. Every entry is
    // checked against it after GetFullPathNameW resolution.
    std::array<WCHAR, MAX_PATH * 4> szRootFullW;
    DWORD nRootLen = ::GetFullPathNameW(szExtractPath, MAX_PATH * 4, szRootFullW.data(), nullptr);
    std::size_t nRootFullLen = ((nRootLen > 0) && (nRootLen < MAX_PATH * 4)) ? std::wcslen(szRootFullW.data()) : 0;
    while ((nRootFullLen > 0) && (szRootFullW[nRootFullLen - 1] == L'\\'))
    {
        szRootFullW[--nRootFullLen] = L'\0';
    }
    if (nRootFullLen == 0)
    {
        MZ_DEBUG(L"[MiniZipNG] Failed to resolve extract root\n");
        mz_zip_reader_close(zip_reader);
        mz_zip_reader_delete(&zip_reader);
        return false;
    }

    // Go to first entry
    err = mz_zip_reader_goto_first_entry(zip_reader);

    while (err == MZ_OK)
    {
        // Check for abort request
        if (m_fAbortRequested)
        {
            MZ_DEBUG(L"[MiniZipNG] Abort requested during extraction\n");
            bAllOk = false;
            break;
        }

        // Get entry info
        mz_zip_file* file_info = nullptr;
        err = mz_zip_reader_entry_get_info(zip_reader, &file_info);
        if ((err != MZ_OK) || !file_info)
        {
            MZ_DEBUG(L"[MiniZipNG] Failed to get entry info\n");
            ++num_errors;
            err = mz_zip_reader_goto_next_entry(zip_reader);
            continue;
        }

        // Build full extraction path with \\?\ prefix
        std::array<WCHAR, MAX_PATH * 4> szEntryNameW;
        ::MultiByteToWideChar(CP_UTF8, 0, file_info->filename, -1, szEntryNameW.data(), static_cast<int>(szEntryNameW.size()));

        // Check if entry is a directory (filename ends with / in ZIP format)
        bool bIsDir{ false };
        std::size_t nNameLen = std::wcslen(szEntryNameW.data());
        if ((nNameLen > 0) && (szEntryNameW[nNameLen - 1] == L'/'))
        {
            bIsDir = true;
        }

        // Replace forward slashes with backslashes for Windows
        for (WCHAR* p = szEntryNameW.data(); *p; ++p)
        {
            if (*p == L'/')
            {
                *p = L'\\';
            }
        }

        // Skip entries whose names could escape the extraction root
        // (absolute paths, drive specifiers, "." or ".." components)
        if (!helper::IsSafeRelativePathW(szEntryNameW.data()))
        {
            MZ_DEBUG(L"[MiniZipNG] Skipping unsafe entry name\n");
            ++num_errors;
            err = mz_zip_reader_goto_next_entry(zip_reader);
            continue;
        }

        // Verify the resolved entry path stays inside the extraction root
        {
            std::array<WCHAR, MAX_PATH * 4> szJoinW;
            std::array<WCHAR, MAX_PATH * 4> szResolvedW;
            ::_snwprintf_s(szJoinW.data(), szJoinW.size(), _TRUNCATE, L"%s\\%s", szRootFullW.data(), szEntryNameW.data());
            DWORD nRes = ::GetFullPathNameW(szJoinW.data(), static_cast<DWORD>(szJoinW.size()), szResolvedW.data(), nullptr);
            if ((nRes == 0) || (nRes >= MAX_PATH * 4) || (std::wcslen(szResolvedW.data()) <= nRootFullLen) ||
                (::_wcsnicmp(szResolvedW.data(), szRootFullW.data(), nRootFullLen) != 0) || (szResolvedW[nRootFullLen] != L'\\'))
            {
                MZ_DEBUG(L"[MiniZipNG] Skipping entry outside extraction root\n");
                ++num_errors;
                err = mz_zip_reader_goto_next_entry(zip_reader);
                continue;
            }
        }

        std::array<WCHAR, MAX_PATH * 4> szFullPathW;
        ::_snwprintf_s(szFullPathW.data(), szFullPathW.size(), _TRUNCATE, L"%s\\%s", szLongPathW.data(), szEntryNameW.data());

        MZ_DEBUG_FMT(L"[MiniZipNG] Extracting: %s (len=%d)\n", szEntryNameW.data(), (int)std::wcslen(szEntryNameW.data()));

        if (bIsDir)
        {
            // Remove trailing backslash for CreateDirectoryW
            nNameLen = std::wcslen(szEntryNameW.data());
            if ((nNameLen > 0) && (szEntryNameW[nNameLen - 1] == L'\\'))
            {
                szEntryNameW[nNameLen - 1] = L'\0';
            }
            ::_snwprintf_s(szFullPathW.data(), szFullPathW.size(), _TRUNCATE, L"%s\\%s", szLongPathW.data(), szEntryNameW.data());

            MZ_DEBUG(L"[MiniZipNG] Creating directory: ");
            MZ_DEBUG(szEntryNameW.data());
            MZ_DEBUG(L"\n");
            MZ_CreateDirectoryChain(szFullPathW.data());
        }
        else
        {
            // Ensure parent directory exists (full chain)
            std::array<WCHAR, MAX_PATH * 4> szParentDir;
            ::wcscpy_s(szParentDir.data(), szParentDir.size(), szFullPathW.data());
            WCHAR* pLastSlash = std::wcsrchr(szParentDir.data(), L'\\');
            if (pLastSlash)
            {
                *pLastSlash = L'\0';
                MZ_CreateDirectoryChain(szParentDir.data());
            }

            // Convert to UTF-8 for minizip
            std::array<char, MAX_PATH * 4> szFullPathUtf8;
            ::WideCharToMultiByte(CP_UTF8, 0, szFullPathW.data(), -1, szFullPathUtf8.data(), sizeof(szFullPathUtf8), nullptr,
                                nullptr);

            // Open entry for reading
            err = mz_zip_reader_entry_open(zip_reader);
            if (err != MZ_OK)
            {
                MZ_DEBUG_FMT(L"[MiniZipNG] Failed to open entry: %d\n", err);
                ++num_errors;
                err = mz_zip_reader_goto_next_entry(zip_reader);
                continue;
            }

            // Create output file with long path support
            HANDLE hFile =
                ::CreateFileW(szFullPathW.data(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (hFile == INVALID_HANDLE_VALUE)
            {
                [[maybe_unused]] DWORD dwErr = ::GetLastError();
                MZ_DEBUG_FMT(L"[MiniZipNG] Failed to create file (err=%lu)\n", dwErr);
                mz_zip_reader_entry_close(zip_reader);
                ++num_errors;
                err = mz_zip_reader_goto_next_entry(zip_reader);
                continue;
            }

            // Read and write file data
            const DWORD BUFFER_SIZE = 64U * 1024;
            std::vector<BYTE> buffer(BUFFER_SIZE);
            std::int32_t bytes_read{ 0 };
            DWORD dwWritten{ 0U };
            bool bWriteOk{ true };

            do
            {
                bytes_read = mz_zip_reader_entry_read(zip_reader, buffer.data(), BUFFER_SIZE);
                if (bytes_read > 0)
                {
                    if (!::WriteFile(hFile, buffer.data(), static_cast<DWORD>(bytes_read), &dwWritten, nullptr) ||
                        (dwWritten != static_cast<DWORD>(bytes_read)))
                    {
                        bWriteOk = false;
                        break;
                    }
                }
            } while (bytes_read > 0);

            ::CloseHandle(hFile);
            mz_zip_reader_entry_close(zip_reader);

            if (!bWriteOk)
            {
                MZ_DEBUG(L"[MiniZipNG] Failed to write file data\n");
                ++num_errors;
            }
        }

        // Move to next entry
        err = mz_zip_reader_goto_next_entry(zip_reader);
    }

    MZ_DEBUG_FMT(L"[MiniZipNG] Extraction complete, errors: %d, aborted: %d\n", num_errors, m_fAbortRequested ? 1 : 0);

    // Close zip file
    mz_zip_reader_close(zip_reader);
    mz_zip_reader_delete(&zip_reader);

    // If aborted, try to clean up the partial extraction folder
    if (m_fAbortRequested)
    {
        MZ_DEBUG(L"[MiniZipNG] Abort requested - cleaning up partial extraction\n");
        // Delete the partially extracted folder
        std::array<WCHAR, MAX_PATH * 4> szExtractDirW;
        // szExtractPath is like "C:\temp\UVNCTMP12345678", extract just the folder name
        const WCHAR* pLastSlash = std::wcsrchr(szExtractPath, L'\\');
        if (pLastSlash)
        {
            ::wcscpy_s(szExtractDirW.data(), szExtractDirW.size(), szExtractPath);
        }
        else
        {
            ::_snwprintf_s(szExtractDirW.data(), szExtractDirW.size(), _TRUNCATE, L"%s\\%s", szLongPathW.data(), szExtractPath);
        }
        // Recursively remove the partial extraction directory
        ::RemoveDirectoryW(szLongPathW.data());
        return false;
    }

    MZ_DEBUG(L"[MiniZipNG] UnZipDirectory END\n");
    // Return true even with some errors (locked files, etc.) as long as we extracted something
    return true;
}

// ANSI compatibility wrappers
bool CMiniZipNG::ZipDirectory(const LPCSTR szDirPath, const LPCSTR szWildcard, const LPCSTR szZipPath, const bool bRecursive)
{
    std::array<WCHAR, MAX_PATH * 4> szDirPathW;
    std::array<WCHAR, MAX_PATH * 4> szWildcardW;
    std::array<WCHAR, MAX_PATH * 4> szZipPathW;

    ::MultiByteToWideChar(CP_ACP, 0, szDirPath, -1, szDirPathW.data(), static_cast<int>(szDirPathW.size()));
    ::MultiByteToWideChar(CP_ACP, 0, szWildcard, -1, szWildcardW.data(), static_cast<int>(szWildcardW.size()));
    ::MultiByteToWideChar(CP_ACP, 0, szZipPath, -1, szZipPathW.data(), static_cast<int>(szZipPathW.size()));

    return ZipDirectory(szDirPathW.data(), szWildcardW.data(), szZipPathW.data(), bRecursive);
}

bool CMiniZipNG::UnZipDirectory(const LPCSTR szExtractPath, const LPCSTR szZipPath)
{
    std::array<WCHAR, MAX_PATH * 4> szExtractPathW;
    std::array<WCHAR, MAX_PATH * 4> szZipPathW;

    ::MultiByteToWideChar(CP_ACP, 0, szExtractPath, -1, szExtractPathW.data(), static_cast<int>(szExtractPathW.size()));
    ::MultiByteToWideChar(CP_ACP, 0, szZipPath, -1, szZipPathW.data(), static_cast<int>(szZipPathW.size()));

    return UnZipDirectory(szExtractPathW.data(), szZipPathW.data());
}
