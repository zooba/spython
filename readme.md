spython
=======

This repository contains sample implementations of CPython entry points
using the hooks added in [PEP 578](https://www.python.org/dev/peps/pep-0578/).

LogToStdErr
-----------

The implementation in `LogToStderr` is nearly the simplest possible
code. It takes every event and prints its arguments to standard error.

Two points are worth calling out:
* during initialisation, it does not render arguments, but this is only
  because `PyObject_Repr` does not work correctly
* `compile` is handled specially to avoid printing the full code of
  every module

