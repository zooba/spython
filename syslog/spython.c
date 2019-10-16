/* syslog example using PySys_AddAuditHook
 */
#include <Python.h>

/* logging */
#include <syslog.h>

int
syslogHook(const char *event, PyObject *args, void *userData)
{
    if (strcmp(event, "import") == 0) {
        PyObject *module, *filename, *sysPath, *sysMetaPath, *sysPathHooks;
        if (!PyArg_ParseTuple(args, "OOOOO", &module, &filename,
                              &sysPath, &sysMetaPath, &sysPathHooks)) {
            return -1;
        }
        if (filename == Py_None) {
            syslog(LOG_INFO, "importing %s", PyUnicode_AsUTF8(module));
        } else {
            syslog(LOG_INFO, "importing %s from %s",
                   PyUnicode_AsUTF8(module),
                   PyUnicode_AsUTF8(filename));
        }
        Py_DECREF(filename);
        return 0;
    }

    if (strcmp(event, "os.system") == 0 ||
        /* additional check for bug in 3.8.0rc1 */
        strcmp(event, "system") == 0) {
        PyObject *command;
        if (!PyArg_ParseTuple(args, "O&", PyUnicode_FSConverter, &command)) {
            return -1;
        }
        syslog(LOG_ERR, "os.system('%s') attempted", PyBytes_AsString(command));
        Py_DECREF(command);
        PyErr_SetString(PyExc_OSError, "os.system is disabled");
        return -1;
    }

    return 0;
}

int
main(int argc, char **argv)
{
    PyStatus status;
    PyConfig config;

    /* configure syslog */
    openlog(NULL, LOG_PID, LOG_USER);

    PySys_AddAuditHook(syslogHook, NULL);

    /* initialize Python in isolated mode, but allow argv */
    PyConfig_InitIsolatedConfig(&config);

    /* handle and parse argv */
    config.parse_argv = 1;
    status = PyConfig_SetBytesArgv(&config, argc, argv);
    if (PyStatus_Exception(status)) {
        goto fail;
    }

    /* perform remaining initialization */
    status = PyConfig_Read(&config);
    if (PyStatus_Exception(status)) {
        goto fail;
    }

    status = Py_InitializeFromConfig(&config);
    if (PyStatus_Exception(status)) {
        goto fail;
    }
    PyConfig_Clear(&config);

    return Py_RunMain();

  fail:
    PyConfig_Clear(&config);
    if (PyStatus_IsExit(status)) {
        return status.exitcode;
    }
    /* Display the error message and exit the process with
       non-zero exit code */
    Py_ExitStatusException(status);
}
