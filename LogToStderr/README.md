LogToStderr
===========

This sample writes messages to standard output. That's all it does.

To build on Windows, open the Visual Studio Developer Command prompt of your choice (making sure you have a suitable Python install or build). Run `set PYTHONDIR=<path to your build or install>`, then run `make.cmd` to build. Once built, the new `spython.exe` will need to be moved into `%PYTHONDIR%` or you can also set `PYTHONHOME`

(If you get it to build on Linux, please send a PR with the simplest instructions (or a fairly simple makefile). Thanks!)
