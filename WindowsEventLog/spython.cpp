/* Minimal main program -- everything is loaded from the library */

#include "Python.h"
#include <windows.h>
#include "events.h"

static int
hook_compile(const char *event, PyObject *args)
{
    PyObject *code, *filename;
    const char *u8code = NULL, *u8filename = NULL;

    /* Avoid doing extra work if the event won't be recorded */
    if (!EventEnabledIMPORT_COMPILE()) {
        return 0;
    }

    if (!PyArg_ParseTuple(args, "OO", &code, &filename)) {
        return -1;
    }

    Py_INCREF(code);
    if (!PyObject_IsTrue(code)) {
        u8code = "";
    } else if (PyUnicode_Check(code)) {
        u8code = PyUnicode_AsUTF8(code);
    } else if (PyBytes_Check(code)) {
        u8code = PyBytes_AS_STRING(code);
    } else if (Py_IsInitialized()) {
        Py_SETREF(code, PyObject_Repr(code));
        if (!code) {
            return -1;
        }
        u8code = PyUnicode_AsUTF8(code);
    } else {
        u8code = "<cannot retrieve>";
    }

    Py_INCREF(filename);
    if (!PyObject_IsTrue(filename)) {
        u8filename = "";
    } else if (PyUnicode_Check(filename)) {
        u8filename = PyUnicode_AsUTF8(filename);
    } else if (PyBytes_Check(filename)) {
        u8filename = PyBytes_AS_STRING(filename);
    } else if (Py_IsInitialized()) {
        Py_SETREF(filename, PyObject_Repr(filename));
        if (!filename) {
            Py_DECREF(code);
            return -1;
        }
        u8filename = PyUnicode_AsUTF8(code);
    } else {
        u8filename = "<cannot retrieve>";
    }

    if (!u8code) {
        Py_DECREF(code);
        Py_DECREF(filename);
        return -1;
    }
    if (!u8filename) {
        Py_DECREF(code);
        Py_DECREF(filename);
        return -1;
    }

    EventWriteIMPORT_COMPILE(u8code, u8filename);

    Py_DECREF(code);
    Py_DECREF(filename);

    return 0;
}

static int
hook_import(const char *event, PyObject *args)
{
    PyObject *name, *_, *syspath;
    if (!PyArg_ParseTuple(args, "OOOOO", &name, &_, &syspath, &_, &_)) {
        return -1;
    }

    if (!PyUnicode_Check(name)) {
        PyErr_SetString(PyExc_TypeError, "require x");
        return -1;
    }
    if (syspath != Py_None && !PyList_CheckExact(syspath)) {
        PyErr_Format(PyExc_TypeError, "cannot use %.200s for sys.path", Py_TYPE(syspath)->tp_name);
        return -1;
    }

    if (EventEnabledIMPORT_RESOLVE()) {
        const char *u8name, *u8path = "\0";
        PyObject *paths = NULL;
        Py_ssize_t u8path_len = 1;

        u8name = PyUnicode_AsUTF8(name);
        if (!u8name) {
            return -1;
        }

        if (Py_IsInitialized() && syspath != Py_None) {
            PyObject *sep = PyUnicode_FromString(";");
            if (!sep) {
                return -1;
            }
            paths = PyUnicode_Join(sep, syspath);
            Py_DECREF(sep);
            if (!paths) {
                return -1;
            }
            u8path = PyUnicode_AsUTF8(paths);
            if (!u8path) {
                Py_DECREF(paths);
                return -1;
            }
        }

        EventWriteIMPORT_RESOLVE(u8name, u8path);

        Py_XDECREF(paths);
    }

    return 0;
}

static int
default_spython_hook(const char *event, PyObject *args, void *userData)
{
    if (strcmp(event, "compile") == 0) {
        return hook_compile(event, args);
    } else if (strcmp(event, "import") == 0) {
        return hook_import(event, args);
    }
    
    return 0;
}

static PyObject *
spython_open_code(PyObject *path, void *userData)
{
    static PyObject *io = NULL;
    PyObject *stream = NULL, *buffer = NULL, *err = NULL;
    PyObject *exc_type, *exc_value, *exc_tb;

    /* path is always provided as PyUnicodeObject */
    assert(PyUnicode_Check(path));

    EventWriteIMPORT_OPEN(PyUnicode_AsUTF8(path));

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

    /* Carefully preserve any exception while we close
     * the stream.
     */
    PyErr_Fetch(&exc_type, &exc_value, &exc_tb);
    err = PyObject_CallMethod(stream, "close", NULL);
    Py_DECREF(stream);
    if (!buffer) {
        /* An error occurred reading, so raise that one */
        PyErr_Restore(exc_type, exc_value, exc_tb);
        return NULL;
    }
    /* These should be clear, but xdecref just in case */
    Py_XDECREF(exc_type);
    Py_XDECREF(exc_value);
    Py_XDECREF(exc_tb);
    if (!err) {
        return NULL;
    }

    /* Here is a good place to validate the contents of
     * buffer and raise an error if not permitted
     */

    return PyObject_CallMethod(io, "BytesIO", "N", buffer);
}

int
wmain(int argc, wchar_t **argv)
{
    int res;

    EventRegisterExample_PythonProvider();

    PySys_AddAuditHook(default_spython_hook, NULL);
    PyFile_SetOpenCodeHook(spython_open_code, NULL);
    res = Py_Main(argc, argv);
    
    EventUnregisterExample_PythonProvider();
    return res;
}
