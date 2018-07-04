@setlocal
@echo off
if not defined PYTHONDIR echo PYTHONDIR must be set before building && exit /B 1
if exist "%PYTHONDIR%\PCbuild" (
    set _PYTHONINCLUDE=-I"%PYTHONDIR%\PC" -I"%PYTHONDIR%\include"
    if "%VSCMD_ARG_TGT_ARCH%" == "x86" (
        set _PYTHONLIB=%PYTHONDIR%\PCbuild\win32
    ) else (
        set _PYTHONLIB=%PYTHONDIR%\PCbuild\amd64
    )
) else (
    set _PYTHONINCLUDE=-I"%PYTHONDIR%\include"
    set _PYTHONLIB=%PYTHONDIR%\libs
)

if exist "%_PYTHONLIB%\python_d.exe" (
    set _MD=-MDd
) else (
    set _MD=-MD
)

@echo on
@if not exist obj mkdir obj
mc -um -h obj -r obj events.man
@if errorlevel 1 exit /B %ERRORLEVEL%
cl -nologo %_MD% -c spython.cpp -Foobj\spython.obj -Iobj %_PYTHONINCLUDE%
@if errorlevel 1 exit /B %ERRORLEVEL%
rc /nologo /fo obj\events.res obj\events.rc
@if errorlevel 1 exit /B %ERRORLEVEL%
link /nologo obj\spython.obj obj\events.res advapi32.lib /out:spython.exe /libpath:"%_PYTHONLIB%"
@if errorlevel 1 exit /B %ERRORLEVEL%
