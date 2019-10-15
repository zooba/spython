LogToStderrMinimal
==================

This sample writes messages to standard output. Compared to the `LogToStderr` sample, this is the minimal amount of code to see some output for every event.

To build on Windows, open the Visual Studio Developer Command prompt of your choice (making sure you have a suitable Python install or build). Run `set PYTHONDIR=<path to your build or install>`, then run `make.cmd` to build. Once built, the new `spython.exe` will need to be moved into `%PYTHONDIR%` or you can also set `PYTHONHOME`

To build on Linux, run `make` with Python 3.8.0rc1 or later installed.
