#include "Python.h"

static PyObject*
spython_open_stream(const char *filename, int fd)
{
    PyObject *iomod = NULL;
    PyObject *fileio = NULL;

    char *args[] = { (char *)filename, NULL };
    char *env[] = { NULL };

    // We always call execveat(), but ignore failures when the
    // SECBIT_EXEC_RESTRICT_FILE bit is not set. This allows the
    // kernel to know that the script execution is about to occur,
    // allowing it to log or record it, rather than restricting it.
    if (execveat(fd, "", args, env, AT_EMPTY_PATH | AT_CHECK) < 0) {
        unsigned secbits = prctl(PR_GET_SECUREBITS);
        if (secbits & SECBIT_EXEC_RESTRICT_FILE) {
            PyErr_SetFromErrnoWithFilename(PyExc_OSError, filename);
            return NULL;
        }
    }

    if ((iomod = PyImport_ImportModule("_io")) == NULL) {
        return NULL;
    }

    fileio = PyObject_CallMethod(iomod, "FileIO", "isi", fd, "r", 1);

    Py_DECREF(iomod);
    return fileio;
}


static PyObject*
spython_open_code(PyObject *path, void *userData)
{
    PyObject *filename_obj = NULL;
    const char *filename;
    int fd = -1;
    PyObject *stream = NULL;

    if (!PyUnicode_FSConverter(path, &filename_obj)) {
        goto end;
    }
    filename = PyBytes_AS_STRING(filename_obj);

    fd = _Py_open(filename, O_RDONLY);
    if (fd > 0) {
        stream = spython_open_stream(filename, fd);
        if (stream) {
            /* stream will close the fd for us */
            fd = -1;
        }
    }

  end:
    Py_XDECREF(filename_obj);
    if (fd >= 0) {
        close(fd);
    }
    return stream;
}


static int
execveatHook(const char *event, PyObject *args, void *userData)
{
    // Fast exit if we've already handled a run event
    int *inspected = (int *)userData;
    if (*inspected) {
        return 0;
    }


    // Open the launch file and validate it
    if (strcmp(event, "cpython.run_file") == 0) {
        *inspected = 1;
        // We always open the launch file and let the open_code handler
        // decide whether to abort or not.
        PyObject *pathname;
        if (PyArg_ParseTuple(args, "U", &pathname)) {
            PyObject *stream = spython_open_code(pathname, NULL);
            if (!stream) {
                return -1;
            }
            Py_DECREF(stream);
        } else {
            return -1;
        }
        return 0;
    }

    // Other run options depend on the global setting.
    if (strncmp(event, "cpython.run_", 12) == 0) {
        *inspected = 1;
        unsigned secbits = prctl(PR_GET_SECUREBITS);
        if (secbits & SECBIT_EXEC_DENY_INTERACTIVE) {
            PyErr_Format(PyExc_OSError, "'%.20s' is disabled by policy", &event[8]);
            return -1;
        }
        return 0;
    }

    return 0;
}


int
main(int argc, char **argv)
{
    unsigned secbits = prctl(PR_GET_SECUREBITS);
    int inspected = 0;
    if (secbits & (SECBIT_EXEC_RESTRICT_FILE | SECBIT_EXEC_DENY_INTERACTIVE)) {
        // Either bit set means we need to inspect launch events
        PySys_AddAuditHook(spython_launch_hook, &inspected);
    }

    // All open_code calls will be hooked regardless of initial settings,
    // as these settings may change during runtime. Additionally, a kernel
    // may be auditing execveat() calls without restricting them, and so
    // we have to make sure those calls occur.
    PyFile_SetOpenCodeHook(spython_open_code, NULL);
    return Py_BytesMain(argc, argv);
}
