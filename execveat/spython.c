#include "Python.h"

static PyObject*
spython_open_stream(const char *filename, int fd)
{
    PyObject *iomod = NULL;
    PyObject *fileio = NULL;

    unsigned secbits = prctl(PR_GET_SECUREBITS);
    if (secbits & SECBIT_SHOULD_EXEC_CHECK) {
        char *args[] = { (char *)pathname, NULL };
        char *env[] = { NULL };
        if (execveat(fd, "", args, env, AT_EMPTY_PATH | AT_CHECK) < 0) {
            if (secbits & SECBIT_SHOULD_EXEC_RESTRICT) {
                PyErr_SetFromErrnoWithFilename(PyExc_OSError, filename);
                return NULL;
            }
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
    // Fast exit for events that can't be "cpython.run_*"
    if (event[0] != 'c' || event[1] != 'p') {
        return 0;
    }

    if (strcmp(event, "cpython.run_file") == 0) {
        // Open the launch file and validate it
        PyObject *pathname;
        if (PyArg_ParseTuple(args, "U", &pathname)) {
            int fd = _Py_open(PyUnicode_AsUTF8(pathname), O_RDONLY);
            PyObject *stream = spython_open_stream(pathname, fd, context);
            if (stream) {
                Py_DECREF(stream);
                return 0;
            }
        }
        return -1;
    }
    if (strncmp(event, "cpython.run_", 12) == 0) {
        // Other run options depend on the global setting.
        unsigned secbits = prctl(PR_GET_SECUREBITS);
        if (secbits & SECBIT_SHOULD_EXEC_RESTRICT) {
            PyErr_Format(PyExc_OSError, "'%.20s' is disabled by policy", &event[8]);
            return -1;
        }
    }

    return 0;
}


int
main(int argc, char **argv)
{
    unsigned secbits = prctl(PR_GET_SECUREBITS);
    if (secbits & SECBIT_SHOULD_EXEC_CHECK) {
        // Check means we need to inspect some launch events
        PySys_AddAuditHook(spython_launch_hook, NULL);
    } else if (secbits & SECBIT_SHOULD_EXEC_RESTRICT) {
        // Restrict without check means we just won't run
        fprintf(stderr, "execution blocked\n");
        return 1;
    }

    PyFile_SetOpenCodeHook(spython_open_code, NULL);
    return Py_BytesMain(argc, argv);
}
