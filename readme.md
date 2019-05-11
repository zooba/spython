spython
=======

This repository contains sample implementations of CPython entry points
using the hooks added in [PEP 578](https://www.python.org/dev/peps/pep-0578/).

For now, you will need to build Python yourself with the 
[pep-578](https://github.com/zooba/cpython/tree/pep-578) branch from
https://github.com/zooba/cpython/ to be able to use these samples.

LogToStdErr
-----------

The implementation in `LogToStderr` is nearly the simplest possible
code. It takes every event and prints its arguments to standard error.

Two points are worth calling out:
* during initialisation, it does not render arguments, but this is only
  because `PyObject_Repr` does not always work correctly
* `compile` is handled specially to avoid printing the full code of
  every module

WindowsCatFile
--------------

The implementation in [`WindowsCatFile`](https://github.com/zooba/spython/tree/master/WindowsCatFile)
uses a signed `python_lib.cat` file to verify all imported modules.

See the readme in that directory for more information.

This sample only works on Windows.

WindowsEventLog
---------------

The implementation in [`WindowsEventLog`](https://github.com/zooba/spython/tree/master/WindowsEventLog)
writes a selection of events to a section of the Windows event log.

See the readme in that directory for more information.

This sample only works on Windows.
