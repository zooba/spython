WindowsDefenderAppControl
=========================

This sample uses the [code integrity protection](https://docs.microsoft.com/windows/security/threat-protection/windows-defender-application-control/windows-defender-application-control) available in Windows 10 or Windows Server 2016 (with all updates) to protect against unsigned files being executed.

To build, open the Visual Studio Developer Command prompt of your choice (making sure you have a suitable Python install or build). Run `set PYTHONDIR=<path to your build or install>`, then run `make.cmd` to build.

Once built, the new `spython.exe` will need to be moved into a regular install directory. Additionally, you will need to enable code integrity protection (**warning**: this may make your machine unusable!):

```
PS> .\update.ps1
Removing old policy...
Merging new policy...
Updating options...
Updating current policy...
Getting recent Event Log entries...
Verifying enforcement status...
* enforced
```

To delete the policy, use the `reset.ps1` script:

```
PS> .\reset.ps1
Policy successfully deleted. You may need to restart your PC.
```

Once the policy is enabled, try running `python.exe` and `spython.exe`. You will find that `python.exe` is blocked, because of a specific rule in the `policy.xml` file. And , which will fail because it cannot verify any of the standard library.

```
PS> .\python.exe
Program 'python.exe' failed to run: Your organization used Device Guard to block this app. Contact your support person
for more infoAt line:1 char:1

PS> .\spython.exe
Fatal Python error: init_fs_encoding: failed to get the Python codec of the filesystem encoding
Traceback (most recent call last):
  File "<frozen importlib._bootstrap>", line 991, in _find_and_load
  File "<frozen importlib._bootstrap>", line 975, in _find_and_load_unlocked
  File "<frozen importlib._bootstrap>", line 671, in _load_unlocked
  File "<frozen importlib._bootstrap_external>", line 776, in exec_module
  File "<frozen importlib._bootstrap_external>", line 912, in get_code
  File "<frozen importlib._bootstrap_external>", line 969, in get_data
OSError: loading 'C:\Users\Administrator\Desktop\WDAC2\tools\lib\encodings\__init__.py' is blocked by policy
```

To enable `spython.exe` to operate, you will need to install the catalog file that is included with Python. This can be deleted later simply by removing the file.

```
PS> copy .\DLLs\python.cat "C:\Windows\System32\CatRoot\{127D0A1D-4EF2-11D1-8608-00C04FC295EE}\python.cat"
PS> .\spython.exe
Python 3.8.0b2 (tags/v3.8.0b2:21dd01d, Jul  4 2019, 16:00:09) [MSC v.1916 64 bit (AMD64)] on win32
Type "help", "copyright", "credits" or "license" for more information.
>>>
```

(If you are using your own build, you will need to sign this yourself, update the contents of `policy.xml` with your certificate, and run `update.ps1` again. Or just use a standard installation that's already signed.)

Set the `SPYTHON_AUDIT` environment variable to see more detailed output for each file as it is validated.

The default `policy.xml` file includes additional rules to prevent the `ssl`, `sqlite3` and `ctypes` modules being loaded. Try importing them to see the result.

Via the `cpython.run_command` auditing event, this entry point will also block (or warn about) use of the `-c` option when integrity enforcement is enabled. To completely disable enforcement, you will likely need to run the `reset.ps1` script and reboot your PC.
