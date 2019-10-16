#include "Python.h"
#include <string.h>

int audit_hook(const char *event, PyObject *args, void *userData)
{
    if (Py_IsInitialized()) {
        PyObject *argsRepr = PyObject_Repr(args);
        if (!argsRepr) {
            return -1;
        }
        printf("Event occurred: %s(%.80s)\n", event, PyUnicode_AsUTF8(argsRepr));
        Py_DECREF(argsRepr);
        return 0;
    }

    printf("Event occurred: %s\n", event);
    return 0;
}

int main(int argc, char **argv)
{
    PySys_AddAuditHook(audit_hook, NULL);
    return Py_BytesMain(argc, argv);
}
