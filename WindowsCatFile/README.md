WindowsCatFile
==============

This sample uses the python_lib.cat file to verify all imports.

To build, open the Visual Studio Developer Command prompt of your choice (making sure you have a suitable Python install or build). Run `set PYTHONDIR=<path to your build or install>`, then run `make.cmd` to build.

Once built, the new `spython.exe` will need to be moved into a regular install directory. It expects a `DLLs\python_lib.cat` file to exist and be signed. A more complete example may include a search for matching catalog files and/or restricting the valid certificates to a known set.
