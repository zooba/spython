NetworkPrompt
=============

This sample intercepts socket events and prompts on stderr/stdin before continuing.

Example output
--------------

```
$ python
>>> from urllib.request import urlopen
>>> urlopen("http://example.com").read()
WARNING: Attempt to resolve example.com:80. Continue [Y/n]
y
WARNING: Attempt to connect 93.184.216.34:80. Continue [Y/n]
y
b'<!doctype html>\n<html>\n<head>\n    ...
>>>
```