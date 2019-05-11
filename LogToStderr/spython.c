/* Minimal main program -- everything is loaded from the library */

#include "Python.h"
#include "opcode.h"
#include <locale.h>
#include <string.h>

#ifdef __FreeBSD__
#include <fenv.h>
#endif

static int
hook_compile(const char *event, PyObject *args)
{
    PyObject *code, *filename;
    if (!PyArg_ParseTuple(args, "OO", &code, &filename)) {
        return -1;
    }

    if (!PyUnicode_Check(code)) {
        code = PyObject_Repr(code);
        if (!code) {
            return -1;
        }
    } else {
        Py_INCREF(code);
    }

    if (PyUnicode_GetLength(code) > 200) {
        Py_SETREF(code, PyUnicode_Substring(code, 0, 200));
        if (!code) {
            return -1;
        }
        Py_SETREF(code, PyUnicode_FromFormat("%S...", code));
        if (!code) {
            return -1;
        }
    }

    PyObject *msg;
    if (PyObject_IsTrue(filename)) {
        if (code == Py_None) {
            msg = PyUnicode_FromFormat("compiling from file %S",
                                       filename);
        } else {
            msg = PyUnicode_FromFormat("compiling %S: %S",
                                       filename, code);
        }
    } else {
        msg = PyUnicode_FromFormat("compiling: %R", code);
    }
    Py_DECREF(code);
    if (!msg) {
        return -1;
    }

    fprintf(stderr, "%s: %s\n", event, PyUnicode_AsUTF8(msg));
    Py_DECREF(msg);
    return 0;
}

static int
default_spython_hook(const char *event, PyObject *args, void *userData)
{
    if (!Py_IsInitialized()) {
        fprintf(stderr, "%s: during startup/shutdown we cannot call repr() on arguments\n", event);
        return 0;
    }

    /* We handle compile() separately to trim the very long code argument */
    if (strcmp(event, "compile") == 0) {
        return hook_compile(event, args);
    }

    // All other events just get printed
    PyObject *msg = PyObject_Repr(args);
    if (!msg) {
        return -1;
    }

    fprintf(stderr, "%s: %s\n", event, PyUnicode_AsUTF8(msg));
    Py_DECREF(msg);

    return 0;
}

static PyObject *
spython_open_code(PyObject *path, void *userData)
{
    static PyObject *io = NULL;
    PyObject *stream = NULL, *buffer = NULL, *err = NULL;

    if (PySys_Audit("spython.open_code", "O", path) < 0) {
        return NULL;
    }

    if (!io) {
        io = PyImport_ImportModule("_io");
        if (!io) {
            return NULL;
        }
    }

    stream = PyObject_CallMethod(io, "open", "Osisssi", path, "rb",
                                 -1, NULL, NULL, NULL, 1);
    if (!stream) {
        return NULL;
    }

    buffer = PyObject_CallMethod(stream, "read", "(i)", -1);

    if (!buffer) {
        Py_DECREF(stream);
        return NULL;
    }

    err = PyObject_CallMethod(stream, "close", NULL);
    Py_DECREF(stream);
    if (!err) {
        return NULL;
    }

    /* Here is a good place to validate the contents of
     * buffer and raise an error if not permitted
     */

    return PyObject_CallMethod(io, "BytesIO", "N", buffer);
}

#ifdef MS_WINDOWS
int
wmain(int argc, wchar_t **argv)
{
    PySys_AddAuditHook(default_spython_hook, NULL);
    PyFile_SetOpenCodeHook(spython_open_code, NULL);
    return Py_Main(argc, argv);
}
#else
int
main(int argc, char **argv)
{
    PySys_AddAuditHook(default_spython_hook, NULL);
    PyFile_SetOpenCodeHook(spython_open_code, NULL);
    return _Py_UnixMain(argc, argv);
}
#endif
