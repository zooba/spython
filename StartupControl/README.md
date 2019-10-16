StartupControl
==============

This sample disallows all ways of launching Python other than passing a filename.

To build on Windows, open the Visual Studio Developer Command prompt of your choice (making sure you have a suitable Python install or build). Run `set PYTHONDIR=<path to your build or install>`, then run `make.cmd` to build. Once built, the new `spython.exe` will need to be moved into `%PYTHONDIR%` or you can also set `PYTHONHOME`

To build on Linux, run `make` with Python 3.8.0rc1 or later installed.
