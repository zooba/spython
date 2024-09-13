execveat
========

This sample uses the proposed `AT_CHECK` flag to `execveat` to verify
that files are allowed to be executed before loading them.

It additionally blocks `-c`, `-m` and interactive launches when the
check+restrict mode is enabled.

To build on Linux, run `make` with a copy of Python 3.8 or later
installed. You will also currently need a patched kernel.
