#include "Python.h"

int startup_hook(const char *event, PyObject *args, void *userData)
{
    /* cpython.run_*(*) - disable launch options except run_file */
    if (strncmp(event, "cpython.run_", 12) == 0
        && strcmp(event, "cpython.run_file") != 0) {
        PyErr_Format(PyExc_OSError, "'%.100s' is disabled by policy", &event[8]);
        return -1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    PySys_AddAuditHook(startup_hook, NULL);
    return Py_BytesMain(argc, argv);
}
