@setlocal
@echo off
if not defined PYTHONDIR echo PYTHONDIR must be set before running && exit /B 1
if exist "%PYTHONDIR%\PCbuild" (
    if "%VSCMD_ARG_TGT_ARCH%" == "x86" (
        set _PYTHONDIR=%PYTHONDIR%\PCbuild\win32
    ) else (
        set _PYTHONDIR=%PYTHONDIR%\PCbuild\amd64
    )
) else (
    set _PYTHONDIR=%PYTHONDIR%
)
set PATH=%PATH%;%_PYTHONDIR%

spython.exe %*
