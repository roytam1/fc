/*
 * PROJECT:     ReactOS FC Command
 * LICENSE:     GPL-2.0-or-later (https://spdx.org/licenses/GPL-2.0-or-later)
 * PURPOSE:     Comparing files
 * COPYRIGHT:   Copyright 2021 Katayama Hirofumi MZ (katayama.hirofumi.mz@gmail.com)
 */

#define _UNICODE
#define UNICODE
#include "fc.h"
#include <tchar.h>

#if defined(_MSC_VER) && _MSC_VER < 1300
#define _countof(a) (sizeof(a)/sizeof(*(a)))
#endif

#ifdef __REACTOS__
    #include <conutils.h>
#else
    #include <stdio.h>
    #define ConInitStdStreams() /* empty */
    #define StdOut stdout
    #define StdErr stderr
    void ConPuts(FILE *fp, LPCWSTR psz)
    {
        fputws(psz, fp);
    }
    void ConPrintf(FILE *fp, LPCWSTR psz, ...)
    {
        va_list va;
        va_start(va, psz);
        vfwprintf(fp, psz, va);
        va_end(va);
    }
    void ConResPuts(FILE *fp, UINT nID)
    {
        WCHAR sz[MAX_PATH*5];
        LoadStringW(NULL, nID, sz, _countof(sz));
        fputws(sz, fp);
    }
    void ConResPrintf(FILE *fp, UINT nID, ...)
    {
        va_list va;
        WCHAR sz[MAX_PATH];
        va_start(va, nID);
        LoadStringW(NULL, nID, sz, _countof(sz));
        vfwprintf(fp, sz, va);
        va_end(va);
    }
#endif
//#include <strsafe.h>
#ifdef USE_SHLWAPI
#include <shlwapi.h>
#define CommandLineToArgvT CommandLineToArgvW
#else
LPTSTR *WINAPI CommandLineToArgvT(LPCTSTR lpCmdLine, int *lpArgc)
{
	HGLOBAL hargv;
	LPTSTR *argv, lpSrc, lpDest, lpArg;
	int argc, nBSlash;
	BOOL bInQuotes;

	// If null was passed in for lpCmdLine, there are no arguments
	if (!lpCmdLine) {
		if (lpArgc)
			*lpArgc = 0;
		return 0;
	}

	lpSrc = (LPTSTR)lpCmdLine;
	// Skip spaces at beginning
	while (*lpSrc == _T(' ') || *lpSrc == _T('\t'))
		lpSrc++;

	// If command-line starts with null, there are no arguments
	if (*lpSrc == 0) {
		if (lpArgc)
			*lpArgc = 0;
		return 0;
	}

	lpArg = lpSrc;
	argc = 0;
	nBSlash = 0;
	bInQuotes = FALSE;

	// Count the number of arguments
	while (1) {
		if (*lpSrc == 0 || ((*lpSrc == _T(' ') || *lpSrc == _T('\t')) && !bInQuotes)) {
			// Whitespace not enclosed in quotes signals the start of another argument
			argc++;

			// Skip whitespace between arguments
			while (*lpSrc == _T(' ') || *lpSrc == _T('\t'))
				lpSrc++;
			if (*lpSrc == 0)
				break;
			nBSlash = 0;
			continue;
		}
		else if (*lpSrc == _T('\\')) {
			// Count consecutive backslashes
			nBSlash++;
		}
		else if (*lpSrc == _T('\"') && !(nBSlash & 1)) {
			// Open or close quotes
			bInQuotes = !bInQuotes;
			nBSlash = 0;
		}
		else {
			// Some other character
			nBSlash = 0;
		}
		lpSrc++;
	}

	// Allocate space the same way as CommandLineToArgvW for compatibility
	hargv = GlobalAlloc(0, argc * sizeof(LPTSTR) + (_tcslen(lpArg) + 1) * sizeof(TCHAR));
	argv = (LPTSTR *)GlobalLock(hargv);

	if (!argv) {
		// Memory allocation failed
		if (lpArgc)
			*lpArgc = 0;
		return 0;
	}

	lpSrc = lpArg;
	lpDest = lpArg = (LPTSTR)(argv + argc);
	argc = 0;
	nBSlash = 0;
	bInQuotes = FALSE;

	// Fill the argument array
	while (1) {
		if (*lpSrc == 0 || ((*lpSrc == _T(' ') || *lpSrc == _T('\t')) && !bInQuotes)) {
			// Whitespace not enclosed in quotes signals the start of another argument
			// Null-terminate argument
			*lpDest++ = 0;
			argv[argc++] = lpArg;

			// Skip whitespace between arguments
			while (*lpSrc == _T(' ') || *lpSrc == _T('\t'))
				lpSrc++;
			if (*lpSrc == 0)
				break;
			lpArg = lpDest;
			nBSlash = 0;
			continue;
		}
		else if (*lpSrc == _T('\\')) {
			*lpDest++ = _T('\\');
			lpSrc++;

			// Count consecutive backslashes
			nBSlash++;
		}
		else if (*lpSrc == _T('\"')) {
			if (!(nBSlash & 1)) {
				// If an even number of backslashes are before the quotes,
				// the quotes don't go in the output
				lpDest -= nBSlash / 2;
				bInQuotes = !bInQuotes;
			}
			else {
				// If an odd number of backslashes are before the quotes,
				// output a quote
				lpDest -= (nBSlash + 1) / 2;
				*lpDest++ = _T('\"');
			}
			lpSrc++;
			nBSlash = 0;
		}
		else {
			// Copy other characters
			*lpDest++ = *lpSrc++;
			nBSlash = 0;
		}
	}

	if (lpArgc)
		*lpArgc = argc;
	return argv;
}

BOOL WINAPI PathIsUNCW(const WCHAR *path)
{
    return path && (path[0] == '\\') && (path[1] == '\\');
}

BOOL WINAPI PathIsRelativeW(const WCHAR *path)
{
    if (!path || !*path)
        return TRUE;

    return !(*path == '\\' || (*path && path[1] == ':'));
}

BOOL WINAPI PathIsUNCServerShareW(const WCHAR *path)
{
    BOOL seen_slash = FALSE;

    if (path && *path++ == '\\' && *path++ == '\\')
    {
        while (*path)
        {
            if (*path == '\\')
            {
                if (seen_slash)
                    return FALSE;
                seen_slash = TRUE;
            }

            path++;
        }
    }

    return seen_slash;
}

BOOL WINAPI PathCanonicalizeW(WCHAR *buffer, const WCHAR *path)
{
    const WCHAR *src = path;
    WCHAR *dst = buffer;

    if (dst)
        *dst = '\0';

    if (!dst || !path)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if (!*path)
    {
        *buffer++ = '\\';
        *buffer = '\0';
        return TRUE;
    }

    /* Copy path root */
    if (*src == '\\')
    {
        *dst++ = *src++;
    }
    else if (*src && src[1] == ':')
    {
        /* X:\ */
        *dst++ = *src++;
        *dst++ = *src++;
        if (*src == '\\')
            *dst++ = *src++;
    }

    /* Canonicalize the rest of the path */
    while (*src)
    {
        if (*src == '.')
        {
            if (src[1] == '\\' && (src == path || src[-1] == '\\' || src[-1] == ':'))
            {
                src += 2; /* Skip .\ */
            }
            else if (src[1] == '.' && dst != buffer && dst[-1] == '\\')
            {
                /* \.. backs up a directory, over the root if it has no \ following X:.
                 * .. is ignored if it would remove a UNC server name or initial \\
                 */
                if (dst != buffer)
                {
                    *dst = '\0'; /* Allow PathIsUNCServerShareA test on lpszBuf */
                    if (dst > buffer + 1 && dst[-1] == '\\' && (dst[-2] != '\\' || dst > buffer + 2))
                    {
                        if (dst[-2] == ':' && (dst > buffer + 3 || dst[-3] == ':'))
                        {
                            dst -= 2;
                            while (dst > buffer && *dst != '\\')
                                dst--;
                            if (*dst == '\\')
                                dst++; /* Reset to last '\' */
                            else
                                dst = buffer; /* Start path again from new root */
                        }
                        else if (dst[-2] != ':' && !PathIsUNCServerShareW(buffer))
                            dst -= 2;
                    }
                    while (dst > buffer && *dst != '\\')
                        dst--;
                    if (dst == buffer)
                    {
                        *dst++ = '\\';
                        src++;
                    }
                }
                src += 2; /* Skip .. in src path */
            }
            else
                *dst++ = *src++;
        }
        else
            *dst++ = *src++;
    }

    /* Append \ to naked drive specs */
    if (dst - buffer == 2 && dst[-1] == ':')
        *dst++ = '\\';
    *dst++ = '\0';
    return TRUE;
}

LPWSTR WINAPI PathAddBackslashW(WCHAR *path)
{
    unsigned int len;

    if (!path || (len = lstrlenW(path)) >= MAX_PATH)
        return NULL;

    if (len)
    {
        path += len;
        if (path[-1] != '\\')
        {
            *path++ = '\\';
            *path = '\0';
        }
    }

    return path;
}

BOOL WINAPI PathIsRootW(const WCHAR *path)
{
    if (!path || !*path)
        return FALSE;

    if (*path == '\\')
    {
        if (!path[1])
            return TRUE; /* \ */
        else if (path[1] == '\\')
        {
            BOOL seen_slash = FALSE;

            path += 2;
            /* Check for UNC root path */
            while (*path)
            {
                if (*path == '\\')
                {
                    if (seen_slash)
                        return FALSE;
                    seen_slash = TRUE;
                }
                path++;
            }

            return TRUE;
        }
    }
    else if (path[1] == ':' && path[2] == '\\' && path[3] == '\0')
        return TRUE; /* X:\ */

    return FALSE;
}

BOOL WINAPI PathRemoveFileSpecW(WCHAR *path)
{
    WCHAR *filespec = path;
    BOOL modified = FALSE;

    if (!path)
        return FALSE;

    /* Skip directory or UNC path */
    if (*path == '\\')
        filespec = ++path;
    if (*path == '\\')
        filespec = ++path;

    while (*path)
    {
        if (*path == '\\')
            filespec = path; /* Skip dir */
        else if (*path == ':')
        {
            filespec = ++path; /* Skip drive */
            if (*path == '\\')
                filespec++;
        }

        path++;
    }

    if (*filespec)
    {
        *filespec = '\0';
        modified = TRUE;
    }

    return modified;
}

BOOL WINAPI PathStripToRootW(WCHAR *path)
{
    if (!path)
        return FALSE;

    while (!PathIsRootW(path))
        if (!PathRemoveFileSpecW(path))
            return FALSE;

    return TRUE;
}

WCHAR * WINAPI PathCombineW(WCHAR *dst, const WCHAR *dir, const WCHAR *file)
{
    BOOL use_both = FALSE, strip = FALSE;
    WCHAR tmp[MAX_PATH];

    /* Invalid parameters */
    if (!dst)
        return NULL;

    if (!dir && !file)
    {
        dst[0] = 0;
        return NULL;
    }

    if ((!file || !*file) && dir)
    {
        /* Use dir only */
        lstrcpynW(tmp, dir, _countof(tmp));
    }
    else if (!dir || !*dir || !PathIsRelativeW(file))
    {
        if (!dir || !*dir || *file != '\\' || PathIsUNCW(file))
        {
            /* Use file only */
            lstrcpynW(tmp, file, _countof(tmp));
        }
        else
        {
            use_both = TRUE;
            strip = TRUE;
        }
    }
    else
        use_both = TRUE;

    if (use_both)
    {
        lstrcpynW(tmp, dir, _countof(tmp));
        if (strip)
        {
            PathStripToRootW(tmp);
            file++; /* Skip '\' */
        }

        if (!PathAddBackslashW(tmp) || lstrlenW(tmp) + lstrlenW(file) >= MAX_PATH)
        {
            dst[0] = 0;
            return NULL;
        }

        lstrcatW(tmp, file);
    }

    PathCanonicalizeW(dst, tmp);
    return dst;
}

BOOL WINAPI PathAppendW(WCHAR *path, const WCHAR *append)
{
    if (path && append)
    {
        if (!PathIsUNCW(append))
            while (*append == '\\')
                append++;

        if (PathCombineW(path, path, append))
            return TRUE;
    }

    return FALSE;
}

LPWSTR WINAPI PathFindExtensionW(const WCHAR *path)
{
    const WCHAR *lastpoint = NULL;

    if (path)
    {
        while (*path)
        {
            if (*path == '\\' || *path == ' ')
                lastpoint = NULL;
            else if (*path == '.')
                lastpoint = path;
            path++;
        }
    }

    return (LPWSTR)(lastpoint ? lastpoint : path);
}

BOOL WINAPI PathAddExtensionW(WCHAR *path, const WCHAR *ext)
{
    unsigned int len;

    if (!path || !ext || *(PathFindExtensionW(path)))
        return FALSE;

    len = lstrlenW(path);
    if (len + lstrlenW(ext) >= MAX_PATH)
        return FALSE;

    lstrcpyW(path + len, ext);
    return TRUE;
}
#endif

FCRET NoDifference(VOID)
{
    ConResPuts(StdOut, IDS_NO_DIFFERENCE);
    return FCRET_IDENTICAL;
}

FCRET Different(LPCWSTR file0, LPCWSTR file1)
{
    ConResPrintf(StdOut, IDS_DIFFERENT, file0, file1);
    return FCRET_DIFFERENT;
}

FCRET LongerThan(LPCWSTR file0, LPCWSTR file1)
{
    ConResPrintf(StdOut, IDS_LONGER_THAN, file0, file1);
    return FCRET_DIFFERENT;
}

FCRET OutOfMemory(VOID)
{
    ConResPuts(StdErr, IDS_OUT_OF_MEMORY);
    return FCRET_INVALID;
}

FCRET CannotRead(LPCWSTR file)
{
    ConResPrintf(StdErr, IDS_CANNOT_READ, file);
    return FCRET_INVALID;
}

FCRET InvalidSwitch(VOID)
{
    ConResPuts(StdErr, IDS_INVALID_SWITCH);
    return FCRET_INVALID;
}

FCRET ResyncFailed(VOID)
{
    ConResPuts(StdOut, IDS_RESYNC_FAILED);
    return FCRET_DIFFERENT;
}

VOID PrintCaption(LPCWSTR file)
{
    ConPrintf(StdOut, L"***** %ls\n", file);
}

VOID PrintEndOfDiff(VOID)
{
    ConPuts(StdOut, L"*****\n\n");
}

VOID PrintDots(VOID)
{
    ConPuts(StdOut, L"...\n");
}

VOID PrintLineW(const FILECOMPARE *pFC, DWORD lineno, LPCWSTR psz)
{
    if (pFC->dwFlags & FLAG_N)
        ConPrintf(StdOut, L"%5d:  %ls\n", lineno, psz);
    else
        ConPrintf(StdOut, L"%ls\n", psz);
}
VOID PrintLineA(const FILECOMPARE *pFC, DWORD lineno, LPCSTR psz)
{
    if (pFC->dwFlags & FLAG_N)
        ConPrintf(StdOut, L"%5d:  %hs\n", lineno, psz);
    else
        ConPrintf(StdOut, L"%hs\n", psz);
}

HANDLE DoOpenFileForInput(LPCWSTR file)
{
    HANDLE hFile = CreateFileW(file, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        ConResPrintf(StdErr, IDS_CANNOT_OPEN, file);
    }
    return hFile;
}

static FCRET BinaryFileCompare(FILECOMPARE *pFC)
{
    FCRET ret;
    HANDLE hFile0, hFile1, hMapping0 = NULL, hMapping1 = NULL;
    LPBYTE pb0 = NULL, pb1 = NULL;
    LARGE_INTEGER ib, cb0, cb1, cbCommon;
    DWORD cbView, ibView;
    BOOL fDifferent = FALSE;
    DWORD dwLastError = 0;

    hFile0 = DoOpenFileForInput(pFC->file[0]);
    if (hFile0 == INVALID_HANDLE_VALUE)
        return FCRET_CANT_FIND;
    hFile1 = DoOpenFileForInput(pFC->file[1]);
    if (hFile1 == INVALID_HANDLE_VALUE)
    {
        CloseHandle(hFile0);
        return FCRET_CANT_FIND;
    }

    do
    {
        if (_wcsicmp(pFC->file[0], pFC->file[1]) == 0)
        {
            ret = NoDifference();
            break;
        }
        //if (!GetFileSizeEx(hFile0, &cb0))
        cb0.LowPart = GetFileSize(hFile0, &(cb0.HighPart));
        if (cb0.LowPart == INVALID_FILE_SIZE)
            dwLastError = GetLastError();
        else dwLastError = 0;
        if (dwLastError != NO_ERROR)
        {
            ret = CannotRead(pFC->file[0]);
            break;
        }
        //if (!GetFileSizeEx(hFile1, &cb1))
        cb1.LowPart = GetFileSize(hFile1, &(cb1.HighPart));
        if (cb1.LowPart == INVALID_FILE_SIZE)
            dwLastError = GetLastError();
        else dwLastError = 0;
        if (dwLastError != NO_ERROR)
        {
            ret = CannotRead(pFC->file[1]);
            break;
        }
        cbCommon.QuadPart = min(cb0.QuadPart, cb1.QuadPart);
        if (cbCommon.QuadPart > 0)
        {
            hMapping0 = CreateFileMappingW(hFile0, NULL, PAGE_READONLY,
                                           cb0.HighPart, cb0.LowPart, NULL);
            if (hMapping0 == NULL)
            {
                ret = CannotRead(pFC->file[0]);
                break;
            }
            hMapping1 = CreateFileMappingW(hFile1, NULL, PAGE_READONLY,
                                           cb1.HighPart, cb1.LowPart, NULL);
            if (hMapping1 == NULL)
            {
                ret = CannotRead(pFC->file[1]);
                break;
            }

            ret = FCRET_IDENTICAL;
            for (ib.QuadPart = 0; ib.QuadPart < cbCommon.QuadPart; )
            {
                cbView = (DWORD)min(cbCommon.QuadPart - ib.QuadPart, MAX_VIEW_SIZE);
                pb0 = MapViewOfFile(hMapping0, FILE_MAP_READ, ib.HighPart, ib.LowPart, cbView);
                pb1 = MapViewOfFile(hMapping1, FILE_MAP_READ, ib.HighPart, ib.LowPart, cbView);
                if (!pb0 || !pb1)
                {
                    ret = OutOfMemory();
                    break;
                }
                for (ibView = 0; ibView < cbView; ++ib.QuadPart, ++ibView)
                {
                    if (pb0[ibView] == pb1[ibView])
                        continue;

                    fDifferent = TRUE;
                    if (cbCommon.QuadPart > MAXDWORD)
                    {
                        ConPrintf(StdOut, L"%016I64X: %02X %02X\n", ib.QuadPart,
                                  pb0[ibView], pb1[ibView]);
                    }
                    else
                    {
                        ConPrintf(StdOut, L"%08lX: %02X %02X\n", ib.LowPart,
                                  pb0[ibView], pb1[ibView]);
                    }
                }
                UnmapViewOfFile(pb0);
                UnmapViewOfFile(pb1);
                pb0 = pb1 = NULL;
            }
            if (ret != FCRET_IDENTICAL)
                break;
        }

        if (cb0.QuadPart < cb1.QuadPart)
            ret = LongerThan(pFC->file[1], pFC->file[0]);
        else if (cb0.QuadPart > cb1.QuadPart)
            ret = LongerThan(pFC->file[0], pFC->file[1]);
        else if (fDifferent)
            ret = FCRET_DIFFERENT;/*Different(pFC->file[0], pFC->file[1]);*/
        else
            ret = NoDifference();
    } while (0);

    UnmapViewOfFile(pb0);
    UnmapViewOfFile(pb1);
    CloseHandle(hMapping0);
    CloseHandle(hMapping1);
    CloseHandle(hFile0);
    CloseHandle(hFile1);
    return ret;
}

static FCRET TextFileCompare(FILECOMPARE *pFC)
{
    FCRET ret;
    HANDLE hFile0, hFile1, hMapping0 = NULL, hMapping1 = NULL;
    LARGE_INTEGER cb0, cb1;
    BOOL fUnicode = !!(pFC->dwFlags & FLAG_U);
    DWORD dwLastError = 0;

    hFile0 = DoOpenFileForInput(pFC->file[0]);
    if (hFile0 == INVALID_HANDLE_VALUE)
        return FCRET_CANT_FIND;
    hFile1 = DoOpenFileForInput(pFC->file[1]);
    if (hFile1 == INVALID_HANDLE_VALUE)
    {
        CloseHandle(hFile0);
        return FCRET_CANT_FIND;
    }

    do
    {
        if (_wcsicmp(pFC->file[0], pFC->file[1]) == 0)
        {
            ret = NoDifference();
            break;
        }
        //if (!GetFileSizeEx(hFile0, &cb0))
        cb0.LowPart = GetFileSize(hFile0, &(cb0.HighPart));
        if (cb0.LowPart == INVALID_FILE_SIZE)
            dwLastError = GetLastError();
        else dwLastError = 0;
        if (dwLastError != NO_ERROR)
        {
            ret = CannotRead(pFC->file[0]);
            break;
        }
        //if (!GetFileSizeEx(hFile1, &cb1))
        cb1.LowPart = GetFileSize(hFile1, &(cb1.HighPart));
        if (cb1.LowPart == INVALID_FILE_SIZE)
            dwLastError = GetLastError();
        else dwLastError = 0;
        if (dwLastError != NO_ERROR)
        {
            ret = CannotRead(pFC->file[1]);
            break;
        }

        if (cb0.QuadPart == 0 && cb1.QuadPart == 0)
        {
            ret = NoDifference();
            break;
        }
        if (cb0.QuadPart > 0)
        {
            hMapping0 = CreateFileMappingW(hFile0, NULL, PAGE_READONLY,
                                           cb0.HighPart, cb0.LowPart, NULL);
            if (hMapping0 == NULL)
            {
                ret = CannotRead(pFC->file[0]);
                break;
            }
        }
        if (cb1.QuadPart > 0)
        {
            hMapping1 = CreateFileMappingW(hFile1, NULL, PAGE_READONLY,
                                           cb1.HighPart, cb1.LowPart, NULL);
            if (hMapping1 == NULL)
            {
                ret = CannotRead(pFC->file[1]);
                break;
            }
        }
        if (fUnicode)
            ret = TextCompareW(pFC, &hMapping0, &cb0, &hMapping1, &cb1);
        else
            ret = TextCompareA(pFC, &hMapping0, &cb0, &hMapping1, &cb1);
    } while (0);

    CloseHandle(hMapping0);
    CloseHandle(hMapping1);
    CloseHandle(hFile0);
    CloseHandle(hFile1);
    return ret;
}

static BOOL IsBinaryExt(LPCWSTR filename)
{
    // Don't change this array. This is by design.
    // See also: https://docs.microsoft.com/en-us/windows-server/administration/windows-commands/fc
    static const LPCWSTR s_exts[] = { L"EXE", L"COM", L"SYS", L"OBJ", L"LIB", L"BIN" };
    size_t iext;
    LPCWSTR pch, ext, pch0 = wcsrchr(filename, L'\\'), pch1 = wcsrchr(filename, L'/');
    if (!pch0 && !pch1)
        pch = filename;
    else if (!pch0 && pch1)
        pch = pch1;
    else if (pch0 && !pch1)
        pch = pch0;
    else if (pch0 < pch1)
        pch = pch1;
    else
        pch = pch0;

    ext = wcsrchr(pch, L'.');
    if (ext)
    {
        ++ext;
        for (iext = 0; iext < _countof(s_exts); ++iext)
        {
            if (_wcsicmp(ext, s_exts[iext]) == 0)
                return TRUE;
        }
    }
    return FALSE;
}

static FCRET FileCompare(FILECOMPARE *pFC)
{
    FCRET ret;
    ConResPrintf(StdOut, IDS_COMPARING, pFC->file[0], pFC->file[1]);

    if (!(pFC->dwFlags & FLAG_L) &&
        ((pFC->dwFlags & FLAG_B) || IsBinaryExt(pFC->file[0]) || IsBinaryExt(pFC->file[1])))
    {
        ret = BinaryFileCompare(pFC);
    }
    else
    {
        ret = TextFileCompare(pFC);
    }

    ConPuts(StdOut, L"\n");
    return ret;
}

/* Is it L"." or L".."? */
#define IS_DOTS(pch) \
    ((*(pch) == L'.') && (((pch)[1] == 0) || (((pch)[1] == L'.') && ((pch)[2] == 0))))
#define HasWildcard(filename) \
    ((wcschr((filename), L'*') != NULL) || (wcschr((filename), L'?') != NULL))
#define IsExtOnly(filename) \
    ((filename)[0] == L'*' && (filename)[1] == L'.' && !HasWildcard(&(filename)[2]))

static FCRET WildcardFileCompareOneSide(FILECOMPARE *pFC, BOOL bWildRight)
{
    FCRET ret = FCRET_IDENTICAL;
    WIN32_FIND_DATAW find;
    HANDLE hFind;
    WCHAR szPath[MAX_PATH];
    FILECOMPARE fc;

    hFind = FindFirstFileW(pFC->file[bWildRight], &find);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        ConResPrintf(StdErr, IDS_CANNOT_OPEN, pFC->file[bWildRight]);
        return FCRET_CANT_FIND;
    }
    //StringCbCopyW(szPath, sizeof(szPath), pFC->file[bWildRight]);
    wcscpy(szPath, pFC->file[bWildRight]);

    fc = *pFC;
    fc.file[!bWildRight] = pFC->file[!bWildRight];
    fc.file[bWildRight] = szPath;
    do
    {
        if (IS_DOTS(find.cFileName))
            continue;
        PathRemoveFileSpecW(szPath);
        PathAppendW(szPath, find.cFileName);
        switch (FileCompare(&fc))
        {
            case FCRET_IDENTICAL:
                break;
            case FCRET_DIFFERENT:
                if (ret != FCRET_INVALID)
                    ret = FCRET_DIFFERENT;
                break;
            default:
                ret = FCRET_INVALID;
                break;
        }
    } while (FindNextFileW(hFind, &find));

    return ret;
}

static FCRET WildcardFileCompareBoth(FILECOMPARE *pFC)
{
    FCRET ret = FCRET_IDENTICAL;
    WIN32_FIND_DATAW find0, find1;
    HANDLE hFind0, hFind1;
    WCHAR szPath0[MAX_PATH], szPath1[MAX_PATH];
    BOOL f0, f1;
    LPWSTR pch;
    FILECOMPARE fc;

    hFind0 = FindFirstFileW(pFC->file[0], &find0);
    if (hFind0 == INVALID_HANDLE_VALUE)
    {
        ConResPrintf(StdErr, IDS_CANNOT_OPEN, pFC->file[0]);
        return FCRET_CANT_FIND;
    }
    hFind1 = FindFirstFileW(pFC->file[1], &find1);
    if (hFind1 == INVALID_HANDLE_VALUE)
    {
        CloseHandle(hFind0);
        ConResPrintf(StdErr, IDS_CANNOT_OPEN, pFC->file[1]);
        return FCRET_CANT_FIND;
    }
    //StringCbCopyW(szPath0, sizeof(szPath0), pFC->file[0]);
    wcscpy(szPath0, pFC->file[0]);
    //StringCbCopyW(szPath1, sizeof(szPath1), pFC->file[1]);
    wcscpy(szPath1, pFC->file[1]);

    fc = *pFC;
    fc.file[0] = szPath0;
    fc.file[1] = szPath1;
    do
    {
        while (IS_DOTS(find0.cFileName))
        {
            f0 = FindNextFileW(hFind0, &find0);
            if (!f0)
                goto quit;
        }
        while (IS_DOTS(find1.cFileName))
        {
            f1 = FindNextFileW(hFind1, &find1);
            if (!f1)
                goto quit;
        }
        PathRemoveFileSpecW(szPath0);
        PathRemoveFileSpecW(szPath1);
        PathAppendW(szPath0, find0.cFileName);
        PathAppendW(szPath1, find1.cFileName);
        switch (FileCompare(&fc))
        {
            case FCRET_IDENTICAL:
                break;
            case FCRET_DIFFERENT:
                if (ret != FCRET_INVALID)
                    ret = FCRET_DIFFERENT;
                break;
            default:
                ret = FCRET_INVALID;
                break;
        }
        f0 = FindNextFileW(hFind0, &find0);
        f1 = FindNextFileW(hFind1, &find1);
    } while (f0 && f1);
quit:
    if (f0 != f1 && IsExtOnly(pFC->file[0]) && IsExtOnly(pFC->file[1]))
    {
        if (f0)
        {
            pch = PathFindExtensionW(find0.cFileName);
            if (pch)
            {
                *pch = 0;
                pch = PathFindExtensionW(pFC->file[1]);
                PathAddExtensionW(find0.cFileName, pch);
                ConResPrintf(StdErr, IDS_CANNOT_OPEN, find0.cFileName);
            }
        }
        else if (f1)
        {
            pch = PathFindExtensionW(find1.cFileName);
            if (pch)
            {
                *pch = 0;
                pch = PathFindExtensionW(pFC->file[0]);
                PathAddExtensionW(find1.cFileName, pch);
                ConResPrintf(StdErr, IDS_CANNOT_OPEN, find1.cFileName);
            }
        }
        ret = FCRET_CANT_FIND;
    }
    CloseHandle(hFind0);
    CloseHandle(hFind1);
    return ret;
}

static FCRET WildcardFileCompare(FILECOMPARE *pFC)
{
    BOOL fWild0, fWild1;

    if (pFC->dwFlags & FLAG_HELP)
    {
        ConResPuts(StdOut, IDS_USAGE);
        return FCRET_INVALID;
    }

    if (!pFC->file[0] || !pFC->file[1])
    {
        ConResPuts(StdErr, IDS_NEEDS_FILES);
        return FCRET_INVALID;
    }

    fWild0 = HasWildcard(pFC->file[0]);
    fWild1 = HasWildcard(pFC->file[1]);
    if (fWild0 && fWild1)
        return WildcardFileCompareBoth(pFC);
    else if (fWild0)
        return WildcardFileCompareOneSide(pFC, FALSE);
    else if (fWild1)
        return WildcardFileCompareOneSide(pFC, TRUE);

    return FileCompare(pFC);
}

int wmain(int argc, WCHAR **argv)
{
    FILECOMPARE fc = { 0, 100, 2 };
    PWCHAR endptr;
    INT i;

    /* Initialize the Console Standard Streams */
    ConInitStdStreams();

    for (i = 1; i < argc; ++i)
    {
        if (argv[i][0] != L'/')
        {
            if (!fc.file[0])
                fc.file[0] = argv[i];
            else if (!fc.file[1])
                fc.file[1] = argv[i];
            else
                return InvalidSwitch();
            continue;
        }
        switch (towupper(argv[i][1]))
        {
            case L'A':
                fc.dwFlags |= FLAG_A;
                break;
            case L'B':
                fc.dwFlags |= FLAG_B;
                break;
            case L'C':
                fc.dwFlags |= FLAG_C;
                break;
            case L'L':
                if (_wcsicmp(argv[i], L"/L") == 0)
                {
                    fc.dwFlags |= FLAG_L;
                }
                else if (towupper(argv[i][2]) == L'B')
                {
                    if (iswdigit(argv[i][3]))
                    {
                        fc.dwFlags |= FLAG_LBn;
                        fc.n = wcstoul(&argv[i][3], &endptr, 10);
                        if (endptr == NULL || *endptr != 0)
                            return InvalidSwitch();
                    }
                    else
                    {
                        return InvalidSwitch();
                    }
                }
                break;
            case L'N':
                fc.dwFlags |= FLAG_N;
                break;
            case L'O':
                if (_wcsicmp(argv[i], L"/OFF") == 0 || _wcsicmp(argv[i], L"/OFFLINE") == 0)
                {
                    fc.dwFlags |= FLAG_OFFLINE;
                }
                break;
            case L'T':
                fc.dwFlags |= FLAG_T;
                break;
            case L'U':
                fc.dwFlags |= FLAG_U;
                break;
            case L'W':
                fc.dwFlags |= FLAG_W;
                break;
            case L'0': case L'1': case L'2': case L'3': case L'4':
            case L'5': case L'6': case L'7': case L'8': case L'9':
                fc.nnnn = wcstoul(&argv[i][1], &endptr, 10);
                if (endptr == NULL || *endptr != 0)
                    return InvalidSwitch();
                fc.dwFlags |= FLAG_nnnn;
                break;
            case L'?':
                fc.dwFlags |= FLAG_HELP;
                break;
            default:
                return InvalidSwitch();
        }
    }
    return WildcardFileCompare(&fc);
}

#ifndef __REACTOS__
int main(int argc, char **argv)
{
    INT my_argc;
    LPWSTR *my_argv = CommandLineToArgvT(GetCommandLineW(), &my_argc);
    INT ret = wmain(my_argc, my_argv);
    LocalFree(my_argv);
    return ret;
}
#endif
