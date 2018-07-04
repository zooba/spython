WindowsEventLog
===============

This sample writes a small selection of messages to the Windows Event Log.

To build, open the Visual Studio Developer Command prompt of your choice (making sure you have a suitable Python install or build). Run `set PYTHONDIR=<path to your build or install>`, then run `make.cmd` to build.

After building, you will need an elevated command prompt (not necessarily a Visual Studio prompt) in the same directory to run `enable.cmd`. This will add a node in Event Viewer called `Example-SPythonProvider` to contain all logged events.

Once enabled, you can run `run.cmd` from the original prompt (this one also needs `PYTHONDIR` to be set), and do whatever you like. All imports and compiled code will be added to event viewer.

When you are done, you can clear the log through Event Viewer and then run `disable.cmd` from the elevated command prompt to remove the entry. Simply running `disable.cmd` does not clear the log, and if you enable it again later your previous entries will still be there.
