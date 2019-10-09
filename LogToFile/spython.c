/* Minimal main program -- everything is loaded from the library */

#include "Python.h"
#include "opcode.h"
#include <locale.h>
#include <string.h>

#ifdef __FreeBSD__
#include <fenv.h>
#endif

static int
hook_addaudithook(const char *event, PyObject *args, FILE *audit_log)
{
    fprintf(audit_log, "%s: hook was not added\n", event);
    PyErr_SetString(PyExc_SystemError, "hook not permitted");
    return -1;
}


// Note that this event is raised by our hook below - it is not a "standard" audit item
static int
hook_open_code(const char *event, PyObject *args, FILE *audit_log)
{
    PyObject *path = PyTuple_GetItem(args, 0);
    PyObject *disallow = PyTuple_GetItem(args, 1);

    PyObject *msg = PyUnicode_FromFormat("'%S'; allowed = %S",
                                         path, disallow);
    if (!msg) {
        return -1;
    }

    fprintf(audit_log, "%s: %s\n", event, PyUnicode_AsUTF8(msg));
    Py_DECREF(msg);

    return 0;
}


static int
hook_import(const char *event, PyObject *args, FILE *audit_log)
{
    PyObject *module, *filename, *sysPath, *sysMetaPath, *sysPathHooks;
    if (!PyArg_ParseTuple(args, "OOOOO", &module, &filename, &sysPath,
                          &sysMetaPath, &sysPathHooks)) {
        return -1;
    }

    PyObject *msg;
    if (PyObject_IsTrue(filename)) {
        msg = PyUnicode_FromFormat("importing %S from %S",
                                   module, filename);
    } else {
        msg = PyUnicode_FromFormat("importing %S:\n"
                                   "    sys.path=%S\n"
                                   "    sys.meta_path=%S\n"
                                   "    sys.path_hooks=%S",
                                   module, sysPath, sysMetaPath,
                                   sysPathHooks);
    }

    if (!msg) {
        return -1;
    }

    fprintf(audit_log, "%s: %s\n", event, PyUnicode_AsUTF8(msg));
    Py_DECREF(msg);

    return 0;
}


static int
hook_compile(const char *event, PyObject *args, FILE *audit_log)
{
    PyObject *code, *filename, *_;
    if (!PyArg_ParseTuple(args, "OO", &code, &filename,
                          &_, &_, &_, &_, &_, &_)) {
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

    fprintf(audit_log, "%s: %s\n", event, PyUnicode_AsUTF8(msg));
    Py_DECREF(msg);
    return 0;
}


static int
hook_code_new(const char *event, PyObject *args, FILE *audit_log)
{
    PyObject *code, *filename, *name;
    int argcount, kwonlyargcount, nlocals, stacksize, flags;
    if (!PyArg_ParseTuple(args, "OOOiiiii", &code, &filename, &name,
                          &argcount, &kwonlyargcount, &nlocals,
                          &stacksize, &flags)) {
        return -1;
    }

    PyObject *msg = PyUnicode_FromFormat("compiling: %R", filename);
    if (!msg) {
        return -1;
    }

    fprintf(audit_log, "%s: %s\n", event, PyUnicode_AsUTF8(msg));
    Py_DECREF(msg);

    if (!PyBytes_Check(code)) {
        PyErr_SetString(PyExc_TypeError, "Invalid bytecode object");
        return -1;
    }

    // As an example, let's validate that no STORE_FAST operations are
    // going to overflow nlocals.
    char *wcode;
    Py_ssize_t wlen;
    if (PyBytes_AsStringAndSize(code, &wcode, &wlen) < 0) {
        return -1;
    }

    for (Py_ssize_t i = 0; i < wlen; i += 2) {
        if (wcode[i] == STORE_FAST) {
            if (wcode[i + 1] > nlocals) {
                PyErr_SetString(PyExc_ValueError, "invalid code object");
                fprintf(audit_log, "%s: code stores to local %d "
                        "but only allocates %d\n",
                        event, wcode[i + 1], nlocals);
                return -1;
            }
        }
    }

    return 0;
}


static int
hook_pickle_find_class(const char *event, PyObject *args, FILE *audit_log)
{
    PyObject *mod = PyTuple_GetItem(args, 0);
    PyObject *global = PyTuple_GetItem(args, 1);

    PyObject *msg = PyUnicode_FromFormat("finding %R.%R blocked",
        mod, global);
    if (!msg) {
        return -1;
    }

    fprintf(audit_log, "%s: %s\n", event, PyUnicode_AsUTF8(msg));
    Py_DECREF(msg);
    PyErr_SetString(PyExc_RuntimeError,
                    "unpickling arbitrary objects is disallowed");
    return -1;
}


static int
hook_system(const char *event, PyObject *args, FILE *audit_log)
{
    PyObject *cmd = PyTuple_GetItem(args, 0);

    PyObject *msg = PyUnicode_FromFormat("%S", cmd);
    if (!msg) {
        return -1;
    }

    fprintf(audit_log, "%s: %s\n", event, PyUnicode_AsUTF8(msg));
    Py_DECREF(msg);

    PyErr_SetString(PyExc_RuntimeError, "os.system() is disallowed");
    return -1;
}


static int
default_spython_hook(const char *event, PyObject *args, void *userData)
{
    assert(userData);

    if (strcmp(event, "sys.addaudithook") == 0) {
        return hook_addaudithook(event, args, (FILE*)userData);
    }

    if (strcmp(event, "spython.open_code") == 0) {
        return hook_open_code(event, args, (FILE*)userData);
    }

    if (strcmp(event, "import") == 0) {
        return hook_import(event, args, (FILE*)userData);
    }

    if (strcmp(event, "compile") == 0) {
        return hook_compile(event, args, (FILE*)userData);
    }

    if (strcmp(event, "code.__new__") == 0) {
        return hook_code_new(event, args, (FILE*)userData);
    }

    if (strcmp(event, "pickle.find_class") == 0) {
        return hook_pickle_find_class(event, args, (FILE*)userData);
    }

    if (strcmp(event, "os.system") == 0) {
        return hook_system(event, args, (FILE*)userData);
    }

    // All other events just get printed
    PyObject *msg = PyObject_Repr(args);
    if (!msg) {
        return -1;
    }

    fprintf((FILE*)userData, "%s: %s\n", event, PyUnicode_AsUTF8(msg));
    Py_DECREF(msg);

    return 0;
}

static PyObject *
spython_open_code(PyObject *path, void *userData)
{
    static PyObject *io = NULL;
    PyObject *stream = NULL, *buffer = NULL, *err = NULL;

    const char *ext = strrchr(PyUnicode_AsUTF8(path), '.');
    int disallow = !ext || (
        PyOS_stricmp(ext, ".py") != 0
        && PyOS_stricmp(ext, ".pth") != 0);

    PyObject *b = PyBool_FromLong(!disallow);
    if (PySys_Audit("spython.open_code", "OO", path, b) < 0) {
        Py_DECREF(b);
        return NULL;
    }
    Py_DECREF(b);

    if (disallow) {
        PyErr_SetString(PyExc_OSError, "invalid format - only .py");
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
    if (strstr(PyBytes_AsString(buffer), "I am a virus")) {
        Py_DECREF(buffer);
        PyErr_SetString(PyExc_OSError, "loading this file is not allowed");
        return NULL;
    }

    return PyObject_CallMethod(io, "BytesIO", "N", buffer);
}

static int
spython_usage(int exitcode, wchar_t *program)
{
    FILE *f = exitcode ? stderr : stdout;

    fprintf(f, "usage: %ls file [arg] ...\n" , program);

    return exitcode;
}

static int
spython_main(int argc, wchar_t **argv, FILE *audit_log)
{
    if (argc == 1) {
        return spython_usage(1, argv[0]);
    }

    /* The auditing log should be opened by the platform-specific main */
    if (!audit_log) {
        Py_FatalError("failed to open log file");
        return 1;
    }

    /* Run the interactive loop. This should be removed for production use */
    if (wcscmp(argv[1], L"-i") == 0) {
        fclose(audit_log);
        audit_log = stderr;
    }

    PySys_AddAuditHook(default_spython_hook, audit_log);
    PyFile_SetOpenCodeHook(spython_open_code, NULL);

    Py_IgnoreEnvironmentFlag = 1;
    Py_NoUserSiteDirectory = 1;
    Py_DontWriteBytecodeFlag = 1;

    Py_SetProgramName(argv[0]);
    Py_Initialize();
    PySys_SetArgv(argc - 1, &argv[1]);

    /* Run the interactive loop. This should be removed for production use */
    if (wcscmp(argv[1], L"-i") == 0) {
        PyRun_InteractiveLoop(stdin, "<stdin>");
        Py_Finalize();
        return 0;
    }

    FILE *fp = _Py_wfopen(argv[1], L"r");
    if (fp != NULL) {
        (void)PyRun_SimpleFile(fp, "__main__");
        PyErr_Clear();
        fclose(fp);
    } else {
        fprintf(stderr, "failed to open source file %ls\n", argv[1]);
    }

    Py_Finalize();
    return 0;
}

#ifdef MS_WINDOWS
int
wmain(int argc, wchar_t **argv)
{
    FILE *audit_log;
    wchar_t *log_path = NULL;
    size_t log_path_len;

    if (_wgetenv_s(&log_path_len, NULL, 0, L"SPYTHONLOG") == 0 &&
        log_path_len) {
        log_path_len += 1;
        log_path = (wchar_t*)malloc(log_path_len * sizeof(wchar_t));
        _wgetenv_s(&log_path_len, log_path, log_path_len, L"SPYTHONLOG");
    } else {
        log_path_len = wcslen(argv[0]) + 5;
        log_path = (wchar_t*)malloc(log_path_len * sizeof(wchar_t));
        wcscpy_s(log_path, log_path_len, argv[0]);
        wcscat_s(log_path, log_path_len, L".log");
    }

    if (_wfopen_s(&audit_log, log_path, L"w")) {
        fwprintf_s(stderr,
                   L"Fatal Python error: failed to open log file: %s\n",
                   log_path);
        return 1;
    }
    free(log_path);

    return spython_main(argc, argv, audit_log);
}

#else

int
main(int argc, char **argv)
{
    wchar_t **argv_copy;
    /* We need a second copy, as Python might modify the first one. */
    wchar_t **argv_copy2;
    int i, res;
    FILE *audit_log;

    argv_copy = (wchar_t **)malloc(sizeof(wchar_t*) * (argc+1));
    argv_copy2 = (wchar_t **)malloc(sizeof(wchar_t*) * (argc+1));
    if (!argv_copy || !argv_copy2) {
        fprintf(stderr, "out of memory\n");
        return 1;
    }

    /* Convert from char to wchar_t based on the locale settings */
    for (i = 0; i < argc; i++) {
        argv_copy[i] = Py_DecodeLocale(argv[i], NULL);
        if (!argv_copy[i]) {
            fprintf(stderr, "Fatal Python error: "
                            "unable to decode the command line argument #%i\n",
                            i + 1);
            return 1;
        }
        argv_copy2[i] = argv_copy[i];
    }
    argv_copy2[argc] = argv_copy[argc] = NULL;

    if (getenv("SPYTHONLOG")) {
        audit_log = fopen(getenv("SPYTHONLOG"), "w");
        if (!audit_log) {
            fprintf(stderr, "Fatal Python error: "
                "failed to open log file: %s\n", getenv("SPYTHONLOG"));
            return 1;
        }
    } else {
        unsigned int log_path_len = strlen(argv[0]) + 5;
        char *log_path = (char*)malloc(log_path_len);
        strcpy(log_path, argv[0]);
        strcat(log_path, ".log");
        audit_log = fopen(log_path, "w");
        if (!audit_log) {
            fprintf(stderr, "Fatal Python error: "
                "failed to open log file: %s\n", log_path);
            return 1;
        }
        free(log_path);
    }

    res = spython_main(argc, argv_copy, audit_log);

    for (i = 0; i < argc; i++) {
        free(argv_copy2[i]);
    }
    free(argv_copy);
    free(argv_copy2);
    return res;
}
#endif
