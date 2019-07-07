/* Minimal main program -- everything is loaded from the library */

#include "Python.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <softpub.h>
#include <wintrust.h>
#include <mscat.h>

static wchar_t wszCatalog[512];

static int
default_spython_hook(const char *event, PyObject *args, void *userData)
{
    if (strcmp(event, "cpython.run_command") == 0) {
        PyErr_SetString(PyExc_RuntimeError, "use of '-c' is disabled");
        return -1;
    }

    return 0;
}

static int
verify_trust(HANDLE hFile)
{
    static const GUID action = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    BYTE hash[256];
    wchar_t memberTag[256];

    WINTRUST_CATALOG_INFO wci = {
        .cbStruct = sizeof(WINTRUST_CATALOG_INFO),
        .hMemberFile = hFile,
        .pbCalculatedFileHash = hash,
        .cbCalculatedFileHash = sizeof(hash),
        .pcwszCatalogFilePath = wszCatalog,
        .pcwszMemberTag = memberTag,
    };
    WINTRUST_DATA wd = {
        .cbStruct = sizeof(WINTRUST_DATA),
        .dwUIChoice = WTD_UI_NONE,
        .fdwRevocationChecks = WTD_REVOKE_WHOLECHAIN,
        .dwUnionChoice = WTD_CHOICE_CATALOG,
        .pCatalog = &wci
    };

    if (!CryptCATAdminCalcHashFromFileHandle(hFile, &wci.cbCalculatedFileHash, hash, 0)) {
        return -1;
    }

    for (DWORD i = 0; i < wci.cbCalculatedFileHash; ++i) {
        swprintf(&memberTag[i*2], 3, L"%02X", hash[i]);
    }

    HRESULT hr = WinVerifyTrust(NULL, (LPGUID)&action, &wd);
    if (FAILED(hr)) {
        PyErr_SetExcFromWindowsErr(PyExc_OSError, GetLastError());
        return -1;
    }
    return 0;
}

static PyObject *
spython_open_stream(HANDLE hFile)
{
    PyObject *io = NULL, *buffer = NULL, *stream = NULL;
    DWORD cbFile;
    LARGE_INTEGER filesize;

    if (!hFile || hFile == INVALID_HANDLE_VALUE) {
        return NULL;
    }

    if (!GetFileSizeEx(hFile, &filesize)) {
        return NULL;
    }

    if (filesize.QuadPart > MAXDWORD) {
        PyErr_SetString(PyExc_OSError, "file too large");
        return NULL;
    }

    if (filesize.QuadPart == 0) {
        /* Zero-length files have no signatures, so do not verify */
        if (!(buffer = PyBytes_FromStringAndSize(NULL, 0))) {
            return NULL;
        }
    } else {
        if (verify_trust(hFile) < 0) {
            return NULL;
        }

        cbFile = (DWORD)filesize.QuadPart;
        if (!(buffer = PyBytes_FromStringAndSize(NULL, cbFile))) {
            return NULL;
        }

        if (!ReadFile(hFile, (LPVOID)PyBytes_AS_STRING(buffer), cbFile, &cbFile, NULL)) {
            Py_DECREF(buffer);
            return NULL;
        }
    }

    if (!(io = PyImport_ImportModule("_io"))) {
        Py_DECREF(buffer);
        return NULL;
    }

    stream = PyObject_CallMethod(io, "BytesIO", "N", buffer);
    Py_DECREF(io);

    return stream;
}

static PyObject *
spython_open_code(PyObject *path, void *userData)
{
    static PyObject *io = NULL;
    const wchar_t *filename;
    Py_ssize_t filename_len;
    HANDLE hFile;
    PyObject *stream = NULL;
    DWORD err;

    if (PySys_Audit("spython.open_code", "O", path) < 0) {
        return NULL;
    }

    if (!PyUnicode_Check(path)) {
        PyErr_SetString(PyExc_TypeError, "invalid type passed to open_code");
        return NULL;
    }

    if (!(filename = PyUnicode_AsWideCharString(path, &filename_len))) {
        return NULL;
    }

    hFile = CreateFileW(filename, GENERIC_READ, FILE_SHARE_READ, NULL,
                        OPEN_EXISTING, 0, NULL);

    PyMem_Free((void*)filename);

    stream = spython_open_stream(hFile);
    err = GetLastError();

    CloseHandle(hFile);

    if (!stream && !PyErr_Occurred()) {
        PyErr_SetExcFromWindowsErrWithFilenameObject(PyExc_OSError, err, path);
    }

    return stream;
}

int
wmain(int argc, wchar_t **argv)
{
    GetModuleFileNameW(NULL, wszCatalog, 512);
    wcscpy(wcsrchr(wszCatalog, L'\\') + 1, L"DLLs\\python_lib.cat");

    PySys_AddAuditHook(default_spython_hook, NULL);
    PyFile_SetOpenCodeHook(spython_open_code, NULL);
    int result = Py_Main(argc, argv);

    return result;
}
