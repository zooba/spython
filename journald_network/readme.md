linux_journald
============

This sample writes messages using [journald](https://www.freedesktop.org/software/systemd/man/latest/systemd-journald.service.html).

To build on Linux, run `make` with a copy of Python 3.8.0rc1 or later
installed. Ensure to have sd-journal.h (Fedora systemd-devel or Debian libsystemd-dev)
and update Makefile with your python version if necessary.

You may need to enable a journald service on your machine in order to
receive the events. For example, if using `rjournald`, you might use
these commands:

```
$ make
$ sudo systemctl start systemd-journald
$ ./spython test-file.py
$ sudo systemctl stop systemd-journald
$ sudo journalctl -l --no-pager -t spython -o json -n 3
```

References
* [systemd for Developers III](https://0pointer.de/blog/projects/journal-submit.html)

Known issues
* "TypeError: signal handler must be signal.SIG_IGN, signal.SIG_DFL, or a callable object" when using spython (Fedora-41, python-3.13.2). Check your current terminal aka env TERM. Setting TERM=dumb is a valid workaround.
* "Fatal Python error: none_dealloc: deallocating None: bug likely caused by a refcount error in a C extension\nPython runtime state: finalizing (tstate=0x000073676bf60018)\n\nCurrent thread 0x000073676bfbdd00 (most recent call first):\n  <no Python frame>": Debian /usr/sbin/ifup, /usr/bin/apt-listchanges, some ansible modules under condition. Revert interpreter to normal python. Some of those are due to os.system() disabled in spython.c.
