WindowsAMSI
===========

This sample uses the [Antimalware Scan Interface](https://docs.microsoft.com/windows/win32/amsi/) (AMSI) available in Windows 10 or Windows Server 2016 to allow active antimalware software to scan dynamically compiled Python code.

To build, open the Visual Studio Developer Command prompt of your choice (making sure you have a suitable Python install or build). Run `set PYTHONDIR=<path to your build or install>`, then run `make.cmd` to build.

For testing, you can execute the [amsi_test.py](amsi_test.py) file in this directory, or pass the [EICAR test file](http://www.eicar.org/anti_virus_test_file.htm) to a `compile()` or `exec()` call. Typing the test file at an interactive is not sufficient, because Python compiles directly from `stdin` and no `compile` event is raised.

This sample does not handle the `cpython.run_command` auditing event, and so code passed on the command line using `-c` will not be scanned (though any calls to `compile()` or `exec()` _will_ be scanned). A more robust implementation would also handle this event.
