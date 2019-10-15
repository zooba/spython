linux_syslog
============

This sample writes messages using [syslog](https://wikipedia.org/wiki/Syslog).

To build on Linux, run `make` with a copy of Python 3.8.0rc1 or later
installed.

You may need to enable a syslog service on your machine in order to
receive the events. For example, if using `rsyslog`, you might use
these commands:

```
$ make
$ sudo service rsyslog start
$ ./spython test-file.py
$ sudo service rsyslog stop
$ cat /var/log/syslog
```
