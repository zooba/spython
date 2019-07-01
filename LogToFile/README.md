LogToFile
=========

This sample writes messages to a file and limits some events.

It also limits `open_code` to only allow Python (.py) and path (.pth) files, provided they do not contain the string `I am a virus` (you can probably find a better heuristic).

To build on Windows, open the Visual Studio Developer Command prompt of your choice (making sure you have a suitable Python install or build). Run `set PYTHONDIR=<path to your build or install>`, then run `make.cmd` to build. Once built, the new `spython.exe` will need to be moved into `%PYTHONDIR%`.

(If you get it to build on Linux, please send a PR with the simplest instructions (or a fairly simple makefile). Thanks!)
