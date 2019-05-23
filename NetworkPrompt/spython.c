#include "Python.h"
#include "opcode.h"
#include <locale.h>
#include <string.h>

static int
network_prompt_hook(const char *event, PyObject *args, void *userData)
{
    /* Only care about 'socket.' events */
    if (strncmp(event, "socket.", 7) != 0) {
        return 0;
    }

    PyObject *msg = NULL;

    /* So yeah, I'm very lazily using PyTuple_GET_ITEM here.
       Not best practice! PyArg_ParseTuple is much better! */
    if (strcmp(event, "socket.getaddrinfo") == 0) {
        msg = PyUnicode_FromFormat("WARNING: Attempt to resolve %S:%S",
            PyTuple_GET_ITEM(args, 0), PyTuple_GET_ITEM(args, 1));
    } else if (strcmp(event, "socket.connect") == 0) {
        PyObject *addro = PyTuple_GET_ITEM(args, 1);
        msg = PyUnicode_FromFormat("WARNING: Attempt to connect %S:%S",
            PyTuple_GET_ITEM(addro, 0), PyTuple_GET_ITEM(addro, 1));
    } else {
        msg = PyUnicode_FromFormat("WARNING: %s (event not handled)", event);
    }

    if (!msg) {
        return -1;
    }

    fprintf(stderr, "%s. Continue [Y/n]\n", PyUnicode_AsUTF8(msg));
    Py_DECREF(msg);
    int ch = fgetc(stdin);
    if (ch == 'n' || ch == 'N') {
        exit(1);
    }
    
    while (ch != '\n') {
        ch = fgetc(stdin);
    }

    return 0;
}


#ifdef MS_WINDOWS
int
wmain(int argc, wchar_t **argv)
{
    PySys_AddAuditHook(network_prompt_hook, NULL);
    return Py_Main(argc, argv);
}
#else
int
main(int argc, char **argv)
{
    PySys_AddAuditHook(network_prompt_hook, NULL);
    return _Py_UnixMain(argc, argv);
}
#endif
