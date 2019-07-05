#include "Python.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <wldp.h>

/* We need to GetProcAddress this method, and the header does not provide
 * a definition for it, so here it is. */
typedef HRESULT (WINAPI *PWLDP_GETLOCKDOWNPOLICY_API)(
    _In_opt_ PWLDP_HOST_INFORMATION hostInformation,
    _Out_ PDWORD lockdownState,
    _In_ DWORD lockdownFlags
);

static int
IsVerbose(void)
{
    static int result = -1;
    if (result < 0) {
        const char *e = getenv("SPYTHON_AUDIT");
        result = (e && *e) ? 1 : 0;
    }
    return result;
}

/* Returns the current lockdown policy for the specified file. */
static HRESULT
GetLockdownPolicy(LPCWSTR path, HANDLE hFile, LPDWORD lockdownState)
{
    static HMODULE wldp = NULL;
    static PWLDP_GETLOCKDOWNPOLICY_API getLockdownPolicy = NULL;

    if (!wldp) {
        Py_BEGIN_ALLOW_THREADS
        wldp = LoadLibraryW(WLDP_DLL);
        getLockdownPolicy = wldp ? (PWLDP_GETLOCKDOWNPOLICY_API)GetProcAddress(wldp, WLDP_GETLOCKDOWNPOLICY_FN) : NULL;
        Py_END_ALLOW_THREADS
        if (!getLockdownPolicy) {
            return E_FAIL;
        }
    }

    WLDP_HOST_INFORMATION hostInfo = {
        .dwRevision = WLDP_HOST_INFORMATION_REVISION,
        /* Lie and claim to be PowerShell. */
        .dwHostId = WLDP_HOST_ID_POWERSHELL,
        .szSource = path,
        .hSource = hFile
    };

    HRESULT hr;
    Py_BEGIN_ALLOW_THREADS
    hr = getLockdownPolicy(&hostInfo, lockdownState, 0);

    if (IsVerbose()) {
        printf_s("error=0x%08X, state=0x%08X, path=%ls\n", hr, *lockdownState, path ? path : L"<none>");
    }

    Py_END_ALLOW_THREADS

    return hr;
}


static int
default_spython_hook(const char *event, PyObject *args, void *userData)
{
    /* cpython.run_command(command) */
    if (strcmp(event, "cpython.run_command") == 0) {
        DWORD lockdownPolicy = 0;
        HRESULT hr = GetLockdownPolicy(NULL, NULL, &lockdownPolicy);
        if (FAILED(hr)) {
            PyErr_SetExcFromWindowsErr(PyExc_OSError, (int)hr);
            return -1;
        }
        
        if (WLDP_LOCKDOWN_IS_ENFORCE(lockdownPolicy)) {
            PyErr_SetString(PyExc_RuntimeError, "use of '-c' is disabled");
            return -1;
        }
        
        if (!WLDP_LOCKDOWN_IS_OFF(lockdownPolicy)) {
            fprintf_s(stderr, "Allowing use of '-c' in audit-only mode\n");
        }
        return 0;
    }

    /* spython.code_integrity_failure(path, audit_mode) */
    if (strcmp(event, "spython.code_integrity_failure") == 0 && IsVerbose()) {
        PyObject *opath = PyTuple_GetItem(args, 0);
        if (!opath) {
            return -1;
        }

        PyObject *is_audit = PyTuple_GetItem(args, 1);
        if (!is_audit) {
            return -1;
        }

        const wchar_t *path = PyUnicode_AsWideCharString(opath, NULL);
        if (!path) {
            return -1;
        }

        fprintf_s(stderr, "Failed to verify integrity%s: %ls\n",
                  is_audit && PyObject_IsTrue(is_audit) ? " (audit)" : "",
                  path);

        PyMem_Free((void *)path);
        return 0;
    }

    return 0;
}

static int
verify_trust(LPCWSTR path, HANDLE hFile)
{
    DWORD lockdownState = 0;
    HRESULT hr = GetLockdownPolicy(path, hFile, &lockdownState);

    if (FAILED(hr)) {
        PyErr_SetExcFromWindowsErr(PyExc_OSError, (int)hr);
        return -1;
    }
    
    if (WLDP_LOCKDOWN_IS_ENFORCE(lockdownState)) {
        if (PySys_Audit("spython.code_integrity_failure", "ui", path, 0) < 0) {
            return -1;
        }
        PyObject *opath = PyUnicode_FromWideChar(path, -1);
        if (opath) {
            PyErr_Format(PyExc_OSError, "loading '%.300S' is blocked by policy", opath);
            Py_DECREF(opath);
        }
        return -1;
    }
    
    if (WLDP_LOCKDOWN_IS_AUDIT(lockdownState)) {
        if (PySys_Audit("spython.code_integrity_failure", "ui", path, 1) < 0) {
            return -1;
        }
    }

    return 0;
}

static PyObject *
spython_open_stream(LPCWSTR filename, HANDLE hFile)
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
        if (verify_trust(filename, hFile) < 0) {
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

    if (!(filename = PyUnicode_AsWideCharString(path, NULL))) {
        return NULL;
    }

    hFile = CreateFileW(filename, GENERIC_READ, FILE_SHARE_READ, NULL,
                        OPEN_EXISTING, 0, NULL);

    stream = spython_open_stream(filename, hFile);
    err = GetLastError();

    PyMem_Free((void*)filename);
    CloseHandle(hFile);

    if (!stream && !PyErr_Occurred()) {
        PyErr_SetExcFromWindowsErrWithFilenameObject(PyExc_OSError, err, path);
    }

    return stream;
}

int
wmain(int argc, wchar_t **argv)
{
    PySys_AddAuditHook(default_spython_hook, NULL);
    PyFile_SetOpenCodeHook(spython_open_code, NULL);
    int result = Py_Main(argc, argv);

    return result;
}
