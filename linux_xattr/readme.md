# PEP 578 spython proof of concept for Linux

This is an experimental **proof of concept** implementation of
``spython`` for Linux. It uses extended file attributes to flag
permitted ``py``/``pyc`` files. The extended file attribute
``user.org.python.x-spython-hash`` contains a SHA-256 hashsum of the
file content. The ``spython`` interpreter refuses to load any Python
file that has no or an invalid hashsum.

Files must also be regular files that resides on an executable file system.

setxattr syscalls are blocked with libseccomp.
