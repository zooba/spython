spython
=======

This repository contains sample implementations of CPython entry points
using the hooks added in [PEP 578](https://www.python.org/dev/peps/pep-0578/).

Python 3.8 is required for these samples, or you can build Python yourself
from the [3.8](https://github.com/python/cpython/tree/3.8) or
[master](https://github.com/python/cpython) branch.

LogToStdErr
-----------

The implementation in `LogToStderr` is nearly the simplest possible
code. It takes every event and prints its arguments to standard error.

Two points are worth calling out:
* during initialisation, it does not render arguments, but this is only
  because `PyObject_Repr` does not always work correctly
* `compile` is handled specially to avoid printing the full code of
  every module

NetworkPrompt
-------------

The implementation in [`NetworkPrompt`](NetworkPrompt) is a very
simplistic hook that prompts the user on every `socket.*` event.
If the user types `n`, the process is aborted.

WindowsCatFile
--------------

The implementation in [`WindowsCatFile`](WindowsCatFile)
uses a signed `python_lib.cat` file to verify all imported modules.

See the readme in that directory for more information.

This sample only works on Windows.

WindowsEventLog
---------------

The implementation in [`WindowsEventLog`](WindowsEventLog)
writes a selection of events to a section of the Windows event log.

See the readme in that directory for more information.

This sample only works on Windows.

linux_xattr
-----------

The implementation in [`linux_xattr`](linux_xattr) is a proof of
concept for Linux. It verifies all imported modules by hashing their
content with OpenSSL and comparing the hashes against stored hashes in
extended file attributes.

See the readme in that directory for more information.

This sample only works on Linux and requires OpenSSL and libseccomp.