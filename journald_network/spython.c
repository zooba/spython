/* journald example using PySys_AddAuditHook
 */
#include <Python.h>

/* logging */
#include <systemd/sd-journal.h>

int
journaldHook(const char *event, PyObject *args, void *userData)
{
    if (strcmp(event, "import") == 0) {
        PyObject *module, *filename, *sysPath, *sysMetaPath, *sysPathHooks;
        if (!PyArg_ParseTuple(args, "OOOOO", &module, &filename,
                              &sysPath, &sysMetaPath, &sysPathHooks)) {
            return -1;
        }
        if (filename == Py_None) {
            sd_journal_send("MESSAGE=importing module %s", PyUnicode_AsUTF8(module),
                "MESSAGE_ID=697945225c004609a15d0d57fcda3ead",
                "PRIORITY=5",
                "USER=%s", getenv("USER"),
                "HOME=%s", getenv("HOME"),
                "PWD=%s", getenv("PWD"),
                "TERM=%s", getenv("TERM"),
                "PAGE_SIZE=%li", sysconf(_SC_PAGESIZE),
                "N_CPUS=%li", sysconf(_SC_NPROCESSORS_ONLN),
                NULL);
        } else {
            sd_journal_send("MESSAGE=importing module %s from %s",
                PyUnicode_AsUTF8(module),
                PyUnicode_AsUTF8(filename),
                "MESSAGE_ID=697945225c004609a15d0d57fcda3ead",
                "PRIORITY=5",
                "USER=%s", getenv("USER"),
                "HOME=%s", getenv("HOME"),
                "PWD=%s", getenv("PWD"),
                "TERM=%s", getenv("TERM"),
                "PAGE_SIZE=%li", sysconf(_SC_PAGESIZE),
                "N_CPUS=%li", sysconf(_SC_NPROCESSORS_ONLN),
                NULL);
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
        sd_journal_send("MESSAGE=os.system('%s') attempted",
                PyBytes_AsString(command),
                "MESSAGE_ID=697945225c004609a15d0d57fcda3ead",
                "PRIORITY=5",
                "USER=%s", getenv("USER"),
                "HOME=%s", getenv("HOME"),
                "PWD=%s", getenv("PWD"),
                "TERM=%s", getenv("TERM"),
                "PAGE_SIZE=%li", sysconf(_SC_PAGESIZE),
                "N_CPUS=%li", sysconf(_SC_NPROCESSORS_ONLN),
                NULL);
        Py_DECREF(command);
        PyErr_SetString(PyExc_OSError, "os.system is disabled");
        return -1;
    }

    return 0;
}

static int
network_hook(const char *event, PyObject *args, void *userData)
{
    /* Only care about 'socket.' events */
    if (strncmp(event, "socket.", 7) != 0) {
        return 0;
    }

    PyObject *msg = NULL;

    /* So yeah, I'm very lazily using PyTuple_GET_ITEM here.
       Not best practice! PyArg_ParseTuple is much better! */
    if (strcmp(event, "socket.getaddrinfo") == 0) {
        msg = PyUnicode_FromFormat("network: Attempt to resolve %S:%S",
            PyTuple_GET_ITEM(args, 0), PyTuple_GET_ITEM(args, 1));
    } else if (strcmp(event, "socket.connect") == 0) {
        PyObject *addro = PyTuple_GET_ITEM(args, 1);
        msg = PyUnicode_FromFormat("network: Attempt to connect %S:%S",
            PyTuple_GET_ITEM(addro, 0), PyTuple_GET_ITEM(addro, 1));
    } else if (strcmp(event, "socket.bind") == 0) {
        PyObject *addro = PyTuple_GET_ITEM(args, 1);
        msg = PyUnicode_FromFormat("network: Attempt to bind %S:%S",
            PyTuple_GET_ITEM(addro, 0), PyTuple_GET_ITEM(addro, 1));
    } else {
        msg = PyUnicode_FromFormat("network: %s (event not handled)", event);
    }

    if (!msg) {
        return -1;
    }
    Py_DECREF(msg);
    sd_journal_send("MESSAGE=%s",
        PyUnicode_AsUTF8(msg),
        "MESSAGE_ID=697945225c004609a15d0d57fcda3ead",
        "PRIORITY=5",
        "USER=%s", getenv("USER"),
        "HOME=%s", getenv("HOME"),
        "PWD=%s", getenv("PWD"),
        "TERM=%s", getenv("TERM"),
        "PAGE_SIZE=%li", sysconf(_SC_PAGESIZE),
        "N_CPUS=%li", sysconf(_SC_NPROCESSORS_ONLN),
        NULL);

    return 0;
}

int
main(int argc, char **argv)
{
    PyStatus status;
    PyConfig config;

    /* configure journald */
    openlog(NULL, LOG_PID, LOG_USER);

    PySys_AddAuditHook(journaldHook, NULL);
    PySys_AddAuditHook(network_hook, NULL);

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
