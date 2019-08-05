#include "Python.h"

#include <amsi.h>

typedef struct _AmsiContext {
    HAMSICONTEXT hContext;
    HAMSISESSION hSession;
} AmsiContext;

int amsi_compile_hook(PyObject *args, AmsiContext* context);
int amsi_scan_bytes(PyObject *buffer, PyObject *filename, AmsiContext *context);

/* Our audit hook callback */
static int
amsi_hook(const char *event, PyObject *args, void *userData)
{
    AmsiContext * const context = (AmsiContext*)userData;

    /* We only handle compile() events, but recommend also
     * handling 'cpython.run_command' (for '-c' code execution) */
    if (strcmp(event, "compile") == 0) {
        return amsi_compile_hook(args, context);
    }

    return 0;
}

/* AMSI handling for the 'compile' event */
static int
amsi_compile_hook(PyObject *args, AmsiContext* context)
{
    PyObject *code, *filename;
    if (!PyArg_ParseTuple(args, "OO", &code, &filename)) {
        return -1;
    }

    if (code == Py_None) {
        /* code is passed as None when compiling directly from a file or stdin.
         * We have to ignore it or Python will not start, but the disk access
         * should trigger an antimalware scan of a file, and interactive
         * execution can be prevented in other ways. */
        return 0;
    }
    
    if (PyBytes_Check(code)) {
        /* code is already bytes - fast path */
        return amsi_scan_bytes(code, filename, context);
    } else if (!PyUnicode_Check(code)) {
        /* Only AST nodes (from ast.parse) should make it here */
        PyErr_Format(PyExc_TypeError, "cannot compile '%.200s' objects",
                     Py_TYPE(code)->tp_name);
        return -1;
    }
    
    PyObject *buffer = PyUnicode_AsUTF8String(code);
    if (!buffer) {
        return -1;
    }
    
    int result = amsi_scan_bytes(buffer, filename, context);
    Py_DECREF(buffer);
    return result;
}

static int
amsi_scan_bytes(PyObject *buffer, PyObject *filename, AmsiContext *context)
{
    char *b;
    Py_ssize_t cb;
    if (PyBytes_AsStringAndSize(buffer, &b, &cb) < 0) {
        return -1;
    }
    if (cb == 0) {
        /* Zero-length buffers do not require scanning */
        return 0;
    }
    if (cb < 0 || cb > ULONG_MAX) {
        PyErr_SetString(PyExc_ValueError, "cannot compile this much code");
        return -1;
    }

    /* Use the filename as AMSI's content reference */
    const wchar_t *contentName = NULL;
    PyObject *filenameRepr = NULL;
    if (PyUnicode_Check(filename)) {
        contentName = PyUnicode_AsWideCharString(filename, NULL);
    } else if (filename != Py_None) {
        filenameRepr = PyObject_Repr(filename);
        if (!filenameRepr) {
            return -1;
        }
        contentName = PyUnicode_AsWideCharString(filenameRepr, NULL);
    }

    if (!contentName && PyErr_Occurred()) {
        Py_XDECREF(filenameRepr);
        return -1;
    }

    AMSI_RESULT result;
    HRESULT hr = AmsiScanBuffer(
        context->hContext,
        (LPVOID)b,
        (ULONG)cb,
        contentName,
        context->hSession,
        &result
    );

    Py_XDECREF(filenameRepr);

    if (FAILED(hr)) {
        PyErr_SetFromWindowsErr(hr);
        return -1;
    }

    if (AmsiResultIsMalware(result)) {
        PyErr_SetString(
            PyExc_OSError,
            "Compilation blocked by antimalware scan. "
            "Check your antimalware protection history for details."
        );
        return -1;
    }

    return 0;
}


int
wmain(int argc, wchar_t **argv)
{
    /* Initialize AMSI at startup. We use a single session to correlate
     * all events in this process, and argv[0] to correlate across sessions. */
    HRESULT hr;
    AmsiContext *context = (AmsiContext*)PyMem_RawMalloc(sizeof(AmsiContext));
    if (FAILED(hr = AmsiInitialize(argv[0], &context->hContext))) {
        fprintf(stderr, "AMSI initialization failed (0x%08X)\n", hr);
        return hr;
    }
    if (FAILED(hr = AmsiOpenSession(context->hContext, &context->hSession))) {
        fprintf(stderr, "AMSI session creation failed (0x%08X)\n", hr);
        return hr;
    }

    PySys_AddAuditHook(amsi_hook, (void*)context);
    int result = Py_Main(argc, argv);

    AmsiCloseSession(context->hContext, context->hSession);
    AmsiUninitialize(context->hContext);
    PyMem_RawFree((void*)context);

    return result;
}
